#include "tapas/runtime/tdict.h"

#include "tapas/runtime/tlist.h"
#include "tapas/runtime/tpair.h"

#include <stdlib.h>

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

static void tdict_keys_append(const tobj *key, const tobj *value, void *ctx)
{
	(void)value;
	tlist_push((tlist *)ctx, key);
}

static void tdict_values_append(const tobj *key, const tobj *value, void *ctx)
{
	(void)key;
	tlist_push((tlist *)ctx, value);
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

tcompo_vtable tdict_vtable = {
	tdict_get_type,	     tdict_get_code,	 tdict_len,
	tdict_copy,	     tdict_free,	 tdict_identical,
	tdict_tostring_abbr, tdict_tostring_full
};

tdict *tdict_new(void)
{
	tdict *d = (tdict *)calloc(1, sizeof(tdict));
	d->base.vtable = &tdict_vtable;
	d->items = thashtbl_new();
	return d;
}

void tdict_set(tdict *d, const tobj *key, const tobj *val)
{
	thashtbl_set(d->items, key, val);
}

void tdict_get(tdict *d, const tobj *key, tobj *vre)
{
	const tobj *value = thashtbl_get(d->items, key);
	if (value) {
		*vre = *value;
		if (vre->type == tcompo && vre->val.v_tcompo)
			vre->val.v_tcompo->refctr++;
		return;
	}
	twarn(ErrRuntime_ObjUnfound, "tdict_get", "");
}

int tdict_contains(tdict *d, const tobj *key)
{
	return thashtbl_contains(d->items, key);
}

int tdict_delete(tdict *d, const tobj *key)
{
	return thashtbl_delete(d->items, key);
}

void
tdict_idx(tdict *d, const tobj *params, uint_regs np, tobj *vre)
{
	if (np != 1)
		twarn(ErrRuntime_ParamsCtr, "tdict_idx", "");
	tdict_get(d, &params[0], vre);
}

void
tdict_iset(tdict *d, const tobj *params, uint_regs np, const tobj *vright)
{
	if (np != 1)
		twarn(ErrRuntime_ParamsCtr, "tdict_iset", "");
	tdict_set(d, &params[0], vright);
}

/** Append pair-like values during PUSHDICT */
void tdict_set_append(tdict *d, const tobj *pair_val)
{
	if (pair_val->type != tcompo)
		return;
	tcompo_v *c = pair_val->val.v_tcompo;
	if (c->vtable->get_compo_type_code() != compo_tpair)
		return;
	tpair *p = (tpair *)c;
	tdict_set(d, &p->first, &p->second);
}

/** Get keys as tlist */
tlist *tdict_keys(tdict *d)
{
	tlist *l = tlist_new();
	thashtbl_each(d->items, tdict_keys_append, l);
	return l;
}

tlist *tdict_values(tdict *d)
{
	tlist *l = tlist_new();
	thashtbl_each(d->items, tdict_values_append, l);
	return l;
}
