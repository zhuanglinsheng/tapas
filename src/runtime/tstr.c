#include "tapas/runtime/tstr.h"

#include "tapas/runtime/tpair.h"

#include <stdlib.h>
#include <string.h>

static const char *tstr_get_type(void)
{
	return "String";
}

static tcompo_type tstr_get_code(void)
{
	return compo_tstr;
}

static long tstr_len(void *self)
{
	tstr *s = (tstr *)self;
	return (long)tstring_len(s->data);
}

static void *tstr_copy(void *self)
{
	tstr *s = (tstr *)self;
	tstr *n = (tstr *)calloc(1, sizeof(tstr));
	n->base.vtable = s->base.vtable;
	n->data = tstring_dup(s->data);
	return n;
}

static void tstr_free(void *self)
{
	tstr *s = (tstr *)self;
	tstring_free(s->data);
	free(s);
}

static int tstr_identical(void *self, void *other)
{
	tstr *a = (tstr *)self;
	tstr *b = (tstr *)other;
	return tstring_eq(a->data, b->data) ? 1 : 0;
}

static tstring *tstr_tostring_abbr(void *self)
{
	tstr *s = (tstr *)self;
	return tstring_dup(s->data);
}

static tstring *tstr_tostring_full(void *self)
{
	tstr *s = (tstr *)self;
	return tstring_dup(s->data);
}

tcompo_vtable tstr_vtable = { tstr_get_type,	 tstr_get_code,
				     tstr_len,		 tstr_copy,
				     tstr_free,		 tstr_identical,
				     tstr_tostring_abbr, tstr_tostring_full };

tstr *tstr_new(const char *s)
{
	tstr *st = (tstr *)calloc(1, sizeof(tstr));
	st->base.vtable = &tstr_vtable;
	st->data = tstring_new(s ? s : "");
	return st;
}

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

static void tstr_replace_range(tstr *s, long start, long end, const char *repl)
{
	size_t slen = tstring_len(s->data);
	size_t rlen = strlen(repl);
	tstring *next = tstring_new_cap((size_t)start + rlen +
					(slen - (size_t)end) + 1);
	tstring_append_len(next, tstring_cstr(s->data), (size_t)start);
	tstring_append_len(next, repl, rlen);
	tstring_append_len(next,
			   tstring_cstr(s->data) + end,
			   slen - (size_t)end);
	tstring_assign_ts(s->data, next);
	tstring_free(next);
}

/* String indexing helper */
void
tstr_idx(tstr *s, const tobj *params, uint_regs np, tobj *vre)
{
	if (np != 1)
		twarn(ErrRuntime_ParamsCtr, "tstr_idx", "");
	long start, end;
	if (pair_to_range(&params[0],
			  (long)tstring_len(s->data),
			  &start,
			  &end)) {
		tstring *slice = tstring_new_len(tstring_cstr(s->data) + start,
						 (size_t)(end - start));
		tobj_set_compo(vre, (tcompo_v *)tstr_new(tstring_cstr(slice)));
		tstring_free(slice);
		return;
	}
	if (params[0].type != tint)
		twarn(ErrRuntime_ParamsType, "tstr_idx", "");
	long idx = params[0].val.v_tint;
	size_t slen = tstring_len(s->data);
	if (idx < 0)
		idx += (long)slen;
	if (idx < 0 || (size_t)idx >= slen)
		twarn(ErrRuntime_IdxOutRange, "tstr_idx", "");
	char ch[2] = { tstring_cstr(s->data)[idx], '\0' };
	tobj_set_compo(vre, (tcompo_v *)tstr_new(ch));
}

void
tstr_iset(tstr *s, const tobj *params, uint_regs np, const tobj *vright)
{
	if (np != 1)
		twarn(ErrRuntime_ParamsCtr, "tstr_iset", "");
	if (vright->type != tcompo || tobj_compo_type(vright) != compo_tstr)
		twarn(ErrRuntime_ParamsType, "tstr_iset", "");
	const char *repl = tstring_cstr(((tstr *)vright->val.v_tcompo)->data);
	long start, end;
	if (pair_to_range(&params[0],
			  (long)tstring_len(s->data),
			  &start,
			  &end)) {
		tstr_replace_range(s, start, end, repl);
		return;
	}
	if (params[0].type != tint)
		twarn(ErrRuntime_ParamsType, "tstr_iset", "");
	long idx = params[0].val.v_tint;
	long slen = (long)tstring_len(s->data);
	if (idx < 0)
		idx += slen;
	if (idx < 0 || idx >= slen)
		twarn(ErrRuntime_IdxOutRange, "tstr_iset", "");
	tstr_replace_range(s, idx, idx + 1, repl);
}
