#include "tapas/runtime/tlist.h"

#include "tapas/runtime/tpair.h"

#include <stdlib.h>

tlist *tlist_new(void)
{
	tlist *list = (tlist *)calloc(1, sizeof(tlist));
	list->base.vtable = &tlist_vtable;
	tobj_vec_init(&list->items);
	return list;
}

uint_objs tlist_size(const tlist *list)
{
	return tobj_vec_len(&list->items);
}

const tobj *tlist_at(const tlist *list, uint_objs index)
{
	return tobj_vec_at_const(&list->items, index);
}

void tlist_set_at(tlist *list, uint_objs index, const tobj *value)
{
	if (index >= tobj_vec_len(&list->items))
		twarn(ErrRuntime_IdxOutRange, "tlist_set_at", "");
	tobj_vec_set(&list->items, index, value);
}

/*----------------------- Required Vtable Operations -----------------------*/

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

/*------------------------------ Capabilities ------------------------------*/

/*
 * List implements every object capability: indexed reads and writes,
 * append, delete by index, membership testing, and iteration. List-only
 * operations such as push_front, push_back, pop_front, pop_back, insert,
 * concat and sort belong to stdlib.
 */

static int pair_to_range(const tobj *param, long len, long *start, long *end)
{
	if (param->type != tcompo || tobj_compo_type(param) != compo_tpair)
		return 0;
	tpair *pair = (tpair *)param->val.v_tcompo;
	if (pair->first.type != tint || pair->second.type != tint)
		twarn(ErrRuntime_ParamsType, "pair_to_range", "");
	long first = pair->first.val.v_tint;
	long last = pair->second.val.v_tint;
	if (first < 0)
		first += len;
	if (last < 0)
		last += len;
	if (first < 0 || last < first || last > len)
		twarn(ErrRuntime_IdxOutRange, "pair_to_range", "");
	*start = first;
	*end = last;
	return 1;
}

static int list_next(void *self, long *position, tobj *result)
{
	tlist *list = (tlist *)self;
	if (*position >= (long)tobj_vec_len(&list->items))
		return 0;
	*result = *tobj_vec_at(&list->items, (uint_objs)*position);
	if (result->type == tcompo && result->val.v_tcompo)
		result->val.v_tcompo->refctr++;
	(*position)++;
	return 1;
}

static void list_append(void *self, const tobj *value)
{
	tobj_vec_push(&((tlist *)self)->items, value);
}

static void list_delete(void *self, const tobj *key)
{
	if (key->type != tint)
		twarn(ErrRuntime_ParamsType, "delete", "integer index required");
	tlist *list = (tlist *)self;
	long index = key->val.v_tint;
	if (index < 0)
		index += (long)tobj_vec_len(&list->items);
	if (index < 0 || (uint_objs)index >= tobj_vec_len(&list->items))
		twarn(ErrRuntime_IdxOutRange, "delete", "");
	tobj_vec_pop(&list->items, (uint_objs)index);
}

static int list_contains(void *self, const tobj *value)
{
	tlist *list = (tlist *)self;
	for (uint_objs i = 0; i < tobj_vec_len(&list->items); i++)
		if (tobj_identical(tobj_vec_at(&list->items, i), value))
			return 1;
	return 0;
}

static void
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

static void
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

static void list_index(void *self, const tobj *arguments,
		       uint_regs argument_count, tobj *result)
{
	tlist_idx((tlist *)self, arguments, argument_count, result);
}

static void list_index_set(void *self, const tobj *arguments,
			   uint_regs argument_count, const tobj *value)
{
	tlist_iset((tlist *)self, arguments, argument_count, value);
}

static const tcompo_capabilities list_capabilities = {
	.indexable = list_index,
	.index_settable = list_index_set,
	.appendable = list_append,
	.deletable = list_delete,
	.contains = list_contains,
	.iterable = list_next
};

tcompo_vtable tlist_vtable = {
	.get_type = tlist_get_type,
	.get_compo_type_code = tlist_get_code,
	.len = tlist_len,
	.copy = tlist_copy,
	.free = tlist_free,
	.identical = tlist_identical,
	.tostring_abbr = tlist_tostring_abbr,
	.tostring_full = tlist_tostring_full,
	.capabilities = &list_capabilities
};
