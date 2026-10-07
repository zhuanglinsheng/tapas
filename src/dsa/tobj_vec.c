#include "tapas/dsa/tobj_vec.h"
#include "tblockpool.h"
#include "tapas/tval.h"

#include <stdlib.h>
#include <string.h>

static void tobj_vec_retain(const tobj *obj)
{
	if (obj->type == tcompo && obj->val.v_tcompo)
		obj->val.v_tcompo->refctr++;
}

void tobj_vec_init(tobj_vec *v)
{
	v->data = nullptr;
	v->len = 0;
	v->capacity = 0;
	v->compo_count = 0;
	v->embedded = 0;
}

void tobj_vec_init_cap(tobj_vec *v, uint_count cap)
{
	v->data = nullptr;
	v->len = 0;
	v->capacity = 0;
	v->compo_count = 0;
	v->embedded = 0;
	if (cap)
		tobj_vec_reserve(v, cap);
}

void tobj_vec_free(tobj_vec *v)
{
	if (v->data) {
		if (v->compo_count)
			for (uint_count i = 0; i < v->len; i++)
				if (v->data[i].type == tcompo)
					tobj_ddc_ref_clear(&v->data[i]);
		if (!v->embedded)
			tblockpool_free(v->data, v->capacity * sizeof(tobj));
	}
	v->data = nullptr;
	v->len = 0;
	v->capacity = 0;
	v->compo_count = 0;
	v->embedded = 0;
}

uint_count tobj_vec_len(const tobj_vec *v)
{
	return v->len;
}

uint_count tobj_vec_cap(const tobj_vec *v)
{
	return v->capacity;
}

tobj *tobj_vec_at(tobj_vec *v, uint_count idx)
{
	return &v->data[idx];
}

const tobj *tobj_vec_at_const(const tobj_vec *v, uint_count idx)
{
	return &v->data[idx];
}

tobj *tobj_vec_data(tobj_vec *v)
{
	return v->data;
}

void tobj_vec_reserve(tobj_vec *v, uint_count cap)
{
	if (cap <= v->capacity)
		return;
	if (v->embedded) {
		/* The current storage lives inside an enclosing allocation and
		 * cannot be realloc'd; migrate it to an owned array first. */
		tobj *grown = (tobj *)tblockpool_alloc(cap * sizeof(tobj));
		if (v->len)
			memcpy(grown, v->data, v->len * sizeof(tobj));
		v->data = grown;
		v->embedded = 0;
	} else if (!v->data) {
		v->data = (tobj *)tblockpool_alloc(cap * sizeof(tobj));
	} else if (cap * sizeof(tobj) > TBLOCKPOOL_MAX_BYTES) {
		/* 超出池化区间的块始终是真实的 malloc 块，可直接 realloc。 */
		v->data = (tobj *)realloc(v->data, cap * sizeof(tobj));
	} else {
		/* Pooled blocks carry no exact size and cannot be realloc'd;
		 * grow by copy instead. */
		tobj *grown = (tobj *)tblockpool_alloc(cap * sizeof(tobj));
		if (v->len)
			memcpy(grown, v->data, v->len * sizeof(tobj));
		tblockpool_free(v->data, v->capacity * sizeof(tobj));
		v->data = grown;
	}
	v->capacity = cap;
}

/* Doubling must stay inside the count domain; clamp at the practical
 * maximum instead of wrapping around. */
#define TOBJ_VEC_MAX_CAP (1u << 30)
static uint_count tobj_vec_grown_cap(uint_count cap)
{
	if (cap == 0)
		return 16;
	if (cap >= TOBJ_VEC_MAX_CAP)
		return TOBJ_VEC_MAX_CAP;
	return cap * 2;
}

void tobj_vec_push(tobj_vec *v, const tobj *obj)
{
	if (v->len >= v->capacity)
		tobj_vec_reserve(v, tobj_vec_grown_cap(v->capacity));
	v->data[v->len] = *obj;
	tobj_vec_retain(obj);
	if (obj->type == tcompo)
		v->compo_count++;
	v->len++;
}

void tobj_vec_set(tobj_vec *v, uint_count idx, const tobj *obj)
{
	ttypes old_type = v->data[idx].type;
	if (v->data[idx].type != tcompo && obj->type != tcompo) {
		v->data[idx] = *obj;
		return;
	}
	if (v->data[idx].type == tcompo && obj->type == tcompo &&
	    v->data[idx].val.v_tcompo == obj->val.v_tcompo)
		return;
	tobj_vec_retain(obj);
	tobj_ddc_ref_clear(&v->data[idx]);
	v->data[idx] = *obj;
	if (old_type != tcompo && obj->type == tcompo)
		v->compo_count++;
	else if (old_type == tcompo && obj->type != tcompo)
		v->compo_count--;
}

void tobj_vec_insert(tobj_vec *v, uint_count idx, const tobj *obj)
{
	if (v->len >= v->capacity)
		tobj_vec_reserve(v, tobj_vec_grown_cap(v->capacity));
	memmove(v->data + idx + 1,
		v->data + idx,
		(v->len - idx) * sizeof(tobj));
	v->data[idx] = *obj;
	tobj_vec_retain(obj);
	if (obj->type == tcompo)
		v->compo_count++;
	v->len++;
}

static tobj tobj_vec_remove(tobj_vec *v, uint_count idx)
{
	tobj removed = v->data[idx];
	if (removed.type == tcompo)
		v->compo_count--;
	memmove(v->data + idx,
		v->data + idx + 1,
		(v->len - idx - 1) * sizeof(tobj));
	v->len--;
	return removed;
}

void tobj_vec_pop(tobj_vec *v, uint_count idx)
{
	tobj removed = tobj_vec_remove(v, idx);
	tobj_ddc_ref_clear(&removed);
}

void tobj_vec_take(tobj_vec *v, uint_count idx, tobj *result)
{
	tobj_ddc_ref_clear(result);
	*result = tobj_vec_remove(v, idx);
}

void tobj_vec_copy(tobj_vec *dst, const tobj_vec *src)
{
	tobj_vec_copy_range(dst, src, 0, src->len);
}

void tobj_vec_copy_range(tobj_vec *dst, const tobj_vec *src,
			 uint_count start, uint_count count)
{
	if (start > src->len || count > src->len - start)
		twarn(ErrRuntime_IdxOutRange, "tobj_vec_copy_range", "");
	tobj_vec_init_cap(dst, count);
	if (count == 0)
		return;
	memcpy(dst->data, src->data + start, count * sizeof(tobj));
	dst->len = count;
	if (src->compo_count)
		for (uint_count i = 0; i < count; i++) {
			tobj_vec_retain(&dst->data[i]);
			if (dst->data[i].type == tcompo)
				dst->compo_count++;
		}
}
