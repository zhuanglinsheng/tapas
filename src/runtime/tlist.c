#include "tapas/runtime/tlist.h"

#include "tapas/runtime/tpair.h"

#include <stdlib.h>

static int pair_to_range(const tobj *param, long len, long *start, long *end)
{
	if (param->type != tcompo || tobj_compo_type(param) != compo_tpair)
		return 0;
	tpair *p = (tpair *)param->val.v_tcompo;
	if (p->first.type != tint || p->second.type != tint)
		twarn(ErrRuntime_ParamsType, "pair_to_range", "");

	long sidx = p->first.val.v_tint;
	long eidx = p->second.val.v_tint;
	if (sidx < 0)
		sidx += len;
	if (eidx < 0)
		eidx += len;
	if (sidx < 0 || eidx < sidx || eidx > len)
		twarn(ErrRuntime_IdxOutRange, "pair_to_range", "");
	*start = sidx;
	*end = eidx;
	return 1;
}

static const char *tlist_get_type(void)
{
	return "List";
}

static tcompo_type tlist_get_code(void)
{
	return compo_tlist;
}

static long tlist_len(void *self)
{
	tlist *l = (tlist *)self;
	return (long)tobj_vec_len(&l->items);
}

static void *tlist_copy(void *self)
{
	tlist *l = (tlist *)self;
	tlist *n = (tlist *)calloc(1, sizeof(tlist));
	n->base.vtable = l->base.vtable;
	tobj_vec_copy(&n->items, &l->items);
	return n;
}

static void tlist_free(void *self)
{
	tlist *l = (tlist *)self;
	tobj_vec_free(&l->items);
	free(l);
}

static int tlist_identical(void *self, void *other)
{
	tlist *a = (tlist *)self;
	tlist *b = (tlist *)other;
	if (tobj_vec_len(&a->items) != tobj_vec_len(&b->items))
		return 0;
	for (uint_objs i = 0; i < tobj_vec_len(&a->items); i++) {
		if (!tobj_identical(tobj_vec_at(&a->items, i),
				    tobj_vec_at(&b->items, i)))
			return 0;
	}
	return 1;
}

static tstring *tlist_tostring_abbr(void *self)
{
	return tobj_tostring_pointer("List", self);
}

static tstring *tlist_tostring_full(void *self)
{
	tlist *l = (tlist *)self;
	uint_objs len = tobj_vec_len(&l->items);
	tstring *out = tstring_new("[");
	for (uint_objs i = 0; i < len; i++) {
		tstring *item = tobj_tostring_full(tobj_vec_at(&l->items, i));
		tstring_append_ts(out, item);
		tstring_free(item);
		if (i < len - 1)
			tstring_append(out, ", ");
	}
	tstring_append_c(out, ']');
	return out;
}

tcompo_vtable tlist_vtable = {
	.get_type = tlist_get_type,
	.get_compo_type_code = tlist_get_code,
	.len = tlist_len,
	.copy = tlist_copy,
	.free = tlist_free,
	.identical = tlist_identical,
	.tostring_abbr = tlist_tostring_abbr,
	.tostring_full = tlist_tostring_full
};

tlist *tlist_new(void)
{
	tlist *l = (tlist *)calloc(1, sizeof(tlist));
	l->base.vtable = &tlist_vtable;
	tobj_vec_init(&l->items);
	return l;
}

uint_objs tlist_size(const tlist *l)
{
	return tobj_vec_len(&l->items);
}

const tobj *tlist_at(const tlist *l, uint_objs idx)
{
	return tobj_vec_at_const(&l->items, idx);
}

void tlist_push(tlist *l, const tobj *v)
{
	tobj_vec_push(&l->items, v);
}

void tlist_insert(tlist *l, uint_objs idx, const tobj *v)
{
	if (idx > tobj_vec_len(&l->items))
		twarn(ErrRuntime_IdxOutRange, "tlist_insert", "");
	tobj_vec_insert(&l->items, idx, v);
}

void tlist_set_at(tlist *l, uint_objs idx, const tobj *v)
{
	if (idx >= tobj_vec_len(&l->items))
		twarn(ErrRuntime_IdxOutRange, "tlist_set_at", "");
	tobj_vec_set(&l->items, idx, v);
}

void tlist_pop(tlist *l, uint_objs idx)
{
	if (idx >= tobj_vec_len(&l->items))
		twarn(ErrRuntime_IdxOutRange, "tlist_pop", "");
	tobj_vec_pop(&l->items, idx);
}

void tlist_sort(tlist *l, int (*compar)(const void *, const void *))
{
	qsort(tobj_vec_data(&l->items),
	      tobj_vec_len(&l->items),
	      sizeof(tobj),
	      compar);
}

int tlist_in(tlist *l, const tobj *v)
{
	for (uint_objs i = 0; i < tobj_vec_len(&l->items); i++) {
		if (tobj_identical(tobj_vec_at(&l->items, i), v))
			return 1;
	}
	return 0;
}

void
tlist_idx(tlist *l, const tobj *params, uint_regs np, tobj *vre)
{
	if (np != 1)
		twarn(ErrRuntime_ParamsCtr, "tlist_idx", "");
	long start, end;
	if (pair_to_range(&params[0], (long)tobj_vec_len(&l->items), &start, &end)) {
		tlist *slice = tlist_new();
		tobj_vec_copy_range(&slice->items, &l->items,
				    (uint_objs)start, (uint_objs)(end - start));
		tobj_set_compo(vre, (tcompo_v *)slice);
		return;
	}
	if (params[0].type != tint)
		twarn(ErrRuntime_ParamsType, "tlist_idx", "");
	long idx = params[0].val.v_tint;
	uint_objs len = tobj_vec_len(&l->items);
	if (idx < 0)
		idx += (long)len;
	if (idx < 0 || (uint_objs)idx >= len)
		twarn(ErrRuntime_IdxOutRange, "tlist_idx", "");
	*vre = *tobj_vec_at(&l->items, (uint_objs)idx);
	if (vre->type == tcompo && vre->val.v_tcompo)
		vre->val.v_tcompo->refctr++;
}

void
tlist_iset(tlist *l, const tobj *params, uint_regs np, const tobj *vright)
{
	if (np != 1)
		twarn(ErrRuntime_ParamsCtr, "tlist_iset", "");
	if (params[0].type != tint)
		twarn(ErrRuntime_ParamsType, "tlist_iset", "");
	long idx = params[0].val.v_tint;
	uint_objs len = tobj_vec_len(&l->items);
	if (idx < 0)
		idx += (long)len;
	if (idx < 0 || (uint_objs)idx >= len)
		twarn(ErrRuntime_IdxOutRange, "tlist_iset", "");
	tlist_set_at(l, (uint_objs)idx, vright);
}

/* Iteration support */
int tlist_next_at(tlist *l, long *iter_pos, tobj *vre)
{
	if (*iter_pos >= (long)tobj_vec_len(&l->items)) {
		*iter_pos = 0;
		return 0;
	}
	*vre = *tobj_vec_at(&l->items, (uint_objs)*iter_pos);
	if (vre->type == tcompo && vre->val.v_tcompo)
		vre->val.v_tcompo->refctr++;
	(*iter_pos)++;
	return 1;
}
