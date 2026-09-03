#include "tapas/runtime/tdict.h"

#include "tapas/runtime/tpair.h"

#include <stdlib.h>

tdict *tdict_new(void)
{
	tdict *dictionary = (tdict *)calloc(1, sizeof(tdict));
	dictionary->base.vtable = &tdict_vtable;
	dictionary->items = thashtbl_new();
	return dictionary;
}

void tdict_set(tdict *dictionary, const tobj *key, const tobj *value)
{
	thashtbl_set(dictionary->items, key, value);
}

void tdict_get(tdict *dictionary, const tobj *key, tobj *result)
{
	const tobj *value = thashtbl_get(dictionary->items, key);
	if (!value)
		twarn(ErrRuntime_ObjUnfound, "tdict_get", "");
	*result = *value;
	if (result->type == tcompo && result->val.v_tcompo)
		result->val.v_tcompo->refctr++;
}

int tdict_contains(tdict *dictionary, const tobj *key)
{
	return thashtbl_contains(dictionary->items, key);
}

/*----------------------- Required Vtable Operations -----------------------*/

static const char *tdict_get_type(void)
{
	return "Dictionary";
}

static tcompo_type tdict_get_code(void)
{
	return compo_tdict;
}

static long tdict_len(void *self)
{
	tdict *d = (tdict *)self;
	return (long)thashtbl_len(d->items);
}

static void *tdict_copy(void *self)
{
	tdict *d = (tdict *)self;
	tdict *n = (tdict *)calloc(1, sizeof(tdict));
	n->base.vtable = d->base.vtable;
	n->items = thashtbl_copy(d->items);
	return n;
}

static void tdict_free(void *self)
{
	tdict *d = (tdict *)self;
	thashtbl_free(d->items);
	free(d);
}

static int tdict_identical(void *self, void *other)
{
	return self == other;
}

typedef struct {
	tstring *out;
	uint_objs idx;
	uint_objs len;
	int multiline;
} tdict_string_ctx;

static void tdict_string_append_pair(const tobj *key, const tobj *value, void *ctx)
{
	tdict_string_ctx *sctx = (tdict_string_ctx *)ctx;
	tstring *k = tobj_tostring_full(key);
	tstring *v = tobj_tostring_full(value);
	if (sctx->multiline)
		tstring_replace(v, "\n", "\n    ");
	if (sctx->multiline)
		tstring_append(sctx->out, "    ");
	tstring_append_ts(sctx->out, k);
	tstring_append(sctx->out, " : ");
	tstring_append_ts(sctx->out, v);
	tstring_free(k);
	tstring_free(v);
	if (sctx->idx < sctx->len - 1) {
		if (sctx->multiline)
			tstring_append(sctx->out, ",\n");
		else
			tstring_append(sctx->out, ", ");
	}
	sctx->idx++;
}

static tstring *tdict_tostring_abbr(void *self)
{
	return tobj_tostring_pointer("Dictionary", self);
}

static tstring *tdict_tostring_full(void *self)
{
	tdict *d = (tdict *)self;
	uint_objs len = thashtbl_len(d->items);
	tstring *out = tstring_new("{");
	tdict_string_ctx ctx = { out, 0, len, len > 1 };
	if (ctx.multiline)
		tstring_append_c(out, '\n');
	thashtbl_each(d->items, tdict_string_append_pair, &ctx);
	if (ctx.multiline)
		tstring_append_c(out, '\n');
	tstring_append_c(out, '}');
	return out;
}

/*------------------------------ Capabilities ------------------------------*/

/*
 * Dictionary supports indexed reads and writes, pair append, key deletion,
 * and key membership. It is deliberately not Iterable: callers choose keys
 * or values explicitly through stdlib, where those collection policies live.
 */

static void dictionary_append(void *self, const tobj *value)
{
	if (value->type != tcompo || tobj_compo_type(value) != compo_tpair)
		twarn(ErrRuntime_ParamsType, "append", "Pair required");
	tpair *pair = (tpair *)value->val.v_tcompo;
	tdict_set((tdict *)self, &pair->first, &pair->second);
}

static int tdict_delete(tdict *dictionary, const tobj *key)
{
	return thashtbl_delete(dictionary->items, key);
}

static void dictionary_delete(void *self, const tobj *key)
{
	if (!tdict_delete((tdict *)self, key))
		twarn(ErrRuntime_ObjUnfound, "delete", "");
}

static int dictionary_contains(void *self, const tobj *value)
{
	return tdict_contains((tdict *)self, value);
}

static void tdict_idx(tdict *d, const tobj *params, uint_regs np, tobj *vre)
{
	if (np != 1)
		twarn(ErrRuntime_ParamsCtr, "tdict_idx", "");
	tdict_get(d, &params[0], vre);
}

static void dictionary_index(void *self, const tobj *arguments,
			     uint_regs argument_count, tobj *result)
{
	tdict_idx((tdict *)self, arguments, argument_count, result);
}

static void tdict_iset(tdict *d, const tobj *params, uint_regs np,
		       const tobj *vright)
{
	if (np != 1)
		twarn(ErrRuntime_ParamsCtr, "tdict_iset", "");
	tdict_set(d, &params[0], vright);
}

static void dictionary_index_set(void *self, const tobj *arguments,
				 uint_regs argument_count, const tobj *value)
{
	tdict_iset((tdict *)self, arguments, argument_count, value);
}

static const tcompo_capabilities dictionary_capabilities = {
	.indexable = dictionary_index,
	.index_settable = dictionary_index_set,
	.appendable = dictionary_append,
	.deletable = dictionary_delete,
	.contains = dictionary_contains
};

tcompo_vtable tdict_vtable = {
	.get_type = tdict_get_type,
	.get_compo_type_code = tdict_get_code,
	.len = tdict_len,
	.copy = tdict_copy,
	.free = tdict_free,
	.identical = tdict_identical,
	.tostring_abbr = tdict_tostring_abbr,
	.tostring_full = tdict_tostring_full,
	.capabilities = &dictionary_capabilities
};
