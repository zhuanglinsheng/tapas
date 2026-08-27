#include "tapas/runtime/tpair.h"

#include <stdlib.h>

static const char *tpair_get_type(void)
{
	return "Pair";
}

static tcompo_type tpair_get_code(void)
{
	return compo_tpair;
}

static long tpair_len(void *self)
{
	(void)self;
	return 2;
}

static void *tpair_copy(void *self)
{
	tpair *p = (tpair *)self;
	tpair *n = (tpair *)calloc(1, sizeof(tpair));
	n->base.vtable = p->base.vtable;
	n->first = p->first;
	n->second = p->second;
	if (n->first.type == tcompo && n->first.val.v_tcompo)
		n->first.val.v_tcompo->refctr++;
	if (n->second.type == tcompo && n->second.val.v_tcompo)
		n->second.val.v_tcompo->refctr++;
	return n;
}

static void tpair_free(void *self)
{
	tpair *p = (tpair *)self;
	tobj_ddc_ref_clear(&p->first);
	tobj_ddc_ref_clear(&p->second);
	free(p);
}

static int tpair_identical(void *self, void *other)
{
	tpair *a = (tpair *)self;
	tpair *b = (tpair *)other;
	return tobj_identical(&a->first, &b->first) &&
	       tobj_identical(&a->second, &b->second);
}

static tstring *tpair_tostring_abbr(void *self)
{
	return tobj_tostring_pointer("Pair", self);
}

static tstring *tpair_tostring_full(void *self)
{
	tpair *p = (tpair *)self;
	tstring *first = tobj_tostring_full(&p->first);
	tstring *second = tobj_tostring_full(&p->second);
	tstring *out = tstring_dup(first);
	tstring_append(out, " : ");
	tstring_append_ts(out, second);
	tstring_free(first);
	tstring_free(second);
	return out;
}

tcompo_vtable tpair_vtable = {
	tpair_get_type,	     tpair_get_code,	 tpair_len,
	tpair_copy,	     tpair_free,	 tpair_identical,
	tpair_tostring_abbr, tpair_tostring_full
};

tpair *tpair_new(const tobj *f, const tobj *s)
{
	tpair *p = (tpair *)calloc(1, sizeof(tpair));
	p->base.vtable = &tpair_vtable;
	p->first = *f;
	p->second = *s;
	if (f->type == tcompo && f->val.v_tcompo)
		f->val.v_tcompo->refctr++;
	if (s->type == tcompo && s->val.v_tcompo)
		s->val.v_tcompo->refctr++;
	return p;
}

void
tpair_idx(tpair *p, const tobj *params, uint_regs np, tobj *vre)
{
	if (np != 1)
		twarn(ErrRuntime_ParamsCtr, "tpair_idx", "");
	if (params[0].type != tint)
		twarn(ErrRuntime_ParamsType, "tpair_idx", "");
	long idx = params[0].val.v_tint;
	if (idx < 0)
		idx += 2;
	if (idx == 0) {
		*vre = p->first;
		if (vre->type == tcompo && vre->val.v_tcompo)
			vre->val.v_tcompo->refctr++;
	} else if (idx == 1) {
		*vre = p->second;
		if (vre->type == tcompo && vre->val.v_tcompo)
			vre->val.v_tcompo->refctr++;
	} else
		twarn(ErrRuntime_IdxOutRange, "tpair_idx", "");
}

void
tpair_iset(tpair *p, const tobj *params, uint_regs np, const tobj *vright)
{
	if (np != 1)
		twarn(ErrRuntime_ParamsCtr, "tpair_iset", "");
	if (params[0].type != tint)
		twarn(ErrRuntime_ParamsType, "tpair_iset", "");
	long idx = params[0].val.v_tint;
	if (idx < 0)
		idx += 2;
	if (idx == 0) {
		if (p->first.type == tcompo && vright->type == tcompo &&
		    p->first.val.v_tcompo == vright->val.v_tcompo)
			return;
		if (vright->type == tcompo)
			vright->val.v_tcompo->refctr++;
		tobj_ddc_ref_clear(&p->first);
		p->first = *vright;
	} else if (idx == 1) {
		if (p->second.type == tcompo && vright->type == tcompo &&
		    p->second.val.v_tcompo == vright->val.v_tcompo)
			return;
		if (vright->type == tcompo)
			vright->val.v_tcompo->refctr++;
		tobj_ddc_ref_clear(&p->second);
		p->second = *vright;
	} else
		twarn(ErrRuntime_IdxOutRange, "tpair_iset", "");
}
