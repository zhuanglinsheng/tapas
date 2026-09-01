#include "tapas/runtime/tcfn.h"

#include <stdlib.h>

static const char *tcppgenf_get_type(void)
{
	return "C Function";
}

static tcompo_type tcppgenf_get_code(void)
{
	return compo_cppfunc;
}

static long tcppgenf_len(void *self)
{
	(void)self;
	return 0;
}

static void *tcppgenf_copy(void *self)
{
	tcppgenf *g = (tcppgenf *)self;
	tcppgenf *n = (tcppgenf *)calloc(1, sizeof(tcppgenf));
	n->base.vtable = g->base.vtable;
	n->f = g->f;
	n->name = tstring_dup(g->name);
	n->nparams_sig = g->nparams_sig;
	return n;
}

static void tcppgenf_free(void *self)
{
	tcppgenf *g = (tcppgenf *)self;
	tstring_free(g->name);
	free(g);
}

static int tcppgenf_identical(void *self, void *other)
{
	return self == other;
}

static tstring *tcppgenf_tostring_abbr(void *self)
{
	return tobj_tostring_pointer("C Function", self);
}

static tstring *tcppgenf_tostring_full(void *self)
{
	return tobj_tostring_pointer("C Function", self);
}

tcompo_vtable tcppgenf_vtable = {
	.get_type = tcppgenf_get_type,
	.get_compo_type_code = tcppgenf_get_code,
	.len = tcppgenf_len,
	.copy = tcppgenf_copy,
	.free = tcppgenf_free,
	.identical = tcppgenf_identical,
	.tostring_abbr = tcppgenf_tostring_abbr,
	.tostring_full = tcppgenf_tostring_full
};

tcppgenf *tcppgenf_new(genf_t f, const char *name, uint_regs nparams_sig)
{
	tcppgenf *g = (tcppgenf *)calloc(1, sizeof(tcppgenf));
	g->base.vtable = &tcppgenf_vtable;
	g->f = f;
	g->name = tstring_new(name ? name : "");
	g->nparams_sig = nparams_sig;
	return g;
}

genf_t tcppgenf_get_f(tcppgenf *g)
{
	return g->f;
}

uint_regs tcppgenf_get_nparams_sig(tcppgenf *g)
{
	return g->nparams_sig;
}

/* Session-level native function wrapper. */

static const char *tcppsessf_get_type(void)
{
	return "C Session Function";
}

static tcompo_type tcppsessf_get_code(void)
{
	return compo_sessfunc;
}

static long tcppsessf_len(void *self)
{
	(void)self;
	return 0;
}

static void *tcppsessf_copy(void *self)
{
	tcppsessf *s = (tcppsessf *)self;
	tcppsessf *n = (tcppsessf *)calloc(1, sizeof(tcppsessf));
	n->base.vtable = s->base.vtable;
	n->f = s->f;
	n->name = tstring_dup(s->name);
	return n;
}

static void tcppsessf_free(void *self)
{
	tcppsessf *s = (tcppsessf *)self;
	tstring_free(s->name);
	free(s);
}

static int tcppsessf_identical(void *self, void *other)
{
	return self == other;
}

static tstring *tcppsessf_tostring_abbr(void *self)
{
	return tobj_tostring_pointer("C Session Function", self);
}

static tstring *tcppsessf_tostring_full(void *self)
{
	return tobj_tostring_pointer("C Session Function", self);
}

tcompo_vtable tcppsessf_vtable = {
	.get_type = tcppsessf_get_type,
	.get_compo_type_code = tcppsessf_get_code,
	.len = tcppsessf_len,
	.copy = tcppsessf_copy,
	.free = tcppsessf_free,
	.identical = tcppsessf_identical,
	.tostring_abbr = tcppsessf_tostring_abbr,
	.tostring_full = tcppsessf_tostring_full
};

tcppsessf *tcppsessf_new(sessf_t f, const char *name)
{
	tcppsessf *s = (tcppsessf *)calloc(1, sizeof(tcppsessf));
	s->base.vtable = &tcppsessf_vtable;
	s->f = f;
	s->name = tstring_new(name ? name : "");
	return s;
}

sessf_t tcppsessf_get_f(tcppsessf *s)
{
	return s->f;
}
