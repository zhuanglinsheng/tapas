#include "tapas/ds/tobj_vec.h"
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
	v->data = NULL;
	v->len = 0;
	v->capacity = 0;
}

void tobj_vec_init_cap(tobj_vec *v, uint_objs cap)
{
	v->data = NULL;
	v->len = 0;
	v->capacity = 0;
	if (cap)
		tobj_vec_reserve(v, cap);
}

void tobj_vec_free(tobj_vec *v)
{
	if (v->data) {
		for (uint_objs i = 0; i < v->len; i++)
			if (v->data[i].type == tcompo)
				tobj_ddc_ref_clear(&v->data[i]);
		free(v->data);
	}
	v->data = NULL;
	v->len = 0;
	v->capacity = 0;
}

uint_objs tobj_vec_len(const tobj_vec *v)
{
	return v->len;
}

uint_objs tobj_vec_cap(const tobj_vec *v)
{
	return v->capacity;
}

tobj *tobj_vec_at(tobj_vec *v, uint_objs idx)
{
	return &v->data[idx];
}

const tobj *tobj_vec_at_const(const tobj_vec *v, uint_objs idx)
{
	return &v->data[idx];
}

tobj *tobj_vec_data(tobj_vec *v)
{
	return v->data;
}

void tobj_vec_reserve(tobj_vec *v, uint_objs cap)
{
	if (cap <= v->capacity)
		return;
	v->data = (tobj *)realloc(v->data, cap * sizeof(tobj));
	v->capacity = cap;
}

void tobj_vec_push(tobj_vec *v, const tobj *obj)
{
	if (v->len >= v->capacity)
		tobj_vec_reserve(v, v->capacity ? v->capacity * 2 : 16);
	v->data[v->len] = *obj;
	tobj_vec_retain(obj);
	v->len++;
}

void tobj_vec_set(tobj_vec *v, uint_objs idx, const tobj *obj)
{
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
}

void tobj_vec_insert(tobj_vec *v, uint_objs idx, const tobj *obj)
{
	if (v->len >= v->capacity)
		tobj_vec_reserve(v, v->capacity ? v->capacity * 2 : 16);
	memmove(v->data + idx + 1,
		v->data + idx,
		(v->len - idx) * sizeof(tobj));
	v->data[idx] = *obj;
	tobj_vec_retain(obj);
	v->len++;
}

void tobj_vec_pop(tobj_vec *v, uint_objs idx)
{
	if (v->data[idx].type == tcompo)
		tobj_ddc_ref_clear(&v->data[idx]);
	memmove(v->data + idx,
		v->data + idx + 1,
		(v->len - idx - 1) * sizeof(tobj));
	v->len--;
}

void tobj_vec_copy(tobj_vec *dst, const tobj_vec *src)
{
	tobj_vec_copy_range(dst, src, 0, src->len);
}

void tobj_vec_copy_range(tobj_vec *dst, const tobj_vec *src,
			 uint_objs start, uint_objs count)
{
	if (start > src->len || count > src->len - start)
		twarn(ErrRuntime_IdxOutRange, "tobj_vec_copy_range", "");
	tobj_vec_init_cap(dst, count);
	if (count == 0)
		return;
	memcpy(dst->data, src->data + start, count * sizeof(tobj));
	dst->len = count;
	for (uint_objs i = 0; i < count; i++)
		tobj_vec_retain(&dst->data[i]);
}
