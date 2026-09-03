#include "tapas/runtime/tcfn.h"

#include <stdlib.h>

int tcfn_descriptor_valid(const tcfn_descriptor *descriptor)
{
	if (!descriptor || !descriptor->name || !*descriptor->name ||
	    (!!descriptor->function == !!descriptor->session_function))
		return 0;
	return descriptor->signature.maximum_parameters == UNDEF_NPARAMS ||
		descriptor->signature.minimum_parameters <=
		descriptor->signature.maximum_parameters;
}

/*----------------------- Required Vtable Operations -----------------------*/

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
	n->signature_type = tstring_dup(g->signature_type);
	n->minimum_parameters = g->minimum_parameters;
	n->maximum_parameters = g->maximum_parameters;
	return n;
}

static void tcppgenf_free(void *self)
{
	tcppgenf *g = (tcppgenf *)self;
	tstring_free(g->name);
	tstring_free(g->signature_type);
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

/*------------------------------ Capabilities ------------------------------*/

/*
 * Native function wrappers expose no object capabilities. Invocation needs
 * VM argument and session handling, so it belongs to the execution protocol
 * rather than to data-object dispatch through tcompo_capabilities.
 */

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
	tcfn_descriptor descriptor = TCFN_DESCRIPTOR(name, f,
		nparams_sig == UNDEF_NPARAMS ?
		TCFN_VARIADIC(nullptr, 0) : TCFN_FIXED(nullptr, nparams_sig));
	return tcppgenf_new_descriptor(&descriptor);
}

tcppgenf *tcppgenf_new_descriptor(const tcfn_descriptor *descriptor)
{
	tcppgenf *g = (tcppgenf *)calloc(1, sizeof(tcppgenf));
	g->base.vtable = &tcppgenf_vtable;
	g->f = descriptor ? descriptor->function : nullptr;
	g->name = tstring_new(descriptor && descriptor->name ?
		descriptor->name : "");
	g->signature_type = tstring_new(descriptor && descriptor->signature.type ?
		descriptor->signature.type : "");
	g->minimum_parameters = descriptor ?
		descriptor->signature.minimum_parameters : 0;
	g->maximum_parameters = descriptor ?
		descriptor->signature.maximum_parameters : UNDEF_NPARAMS;
	return g;
}

genf_t tcppgenf_get_f(tcppgenf *g)
{
	return g->f;
}

uint_regs tcppgenf_get_nparams_sig(tcppgenf *g)
{
	return g->minimum_parameters == g->maximum_parameters ?
		g->minimum_parameters : UNDEF_NPARAMS;
}

const char *tcppgenf_get_signature_type(const tcppgenf *g)
{
	return g ? tstring_cstr(g->signature_type) : nullptr;
}

uint_regs tcppgenf_get_minimum_parameters(const tcppgenf *g)
{
	return g ? g->minimum_parameters : 0;
}

uint_regs tcppgenf_get_maximum_parameters(const tcppgenf *g)
{
	return g ? g->maximum_parameters : UNDEF_NPARAMS;
}

int tcppgenf_accepts(const tcppgenf *g, uint_regs count)
{
	return g && count >= g->minimum_parameters &&
		(g->maximum_parameters == UNDEF_NPARAMS ||
		 count <= g->maximum_parameters);
}

/*----------------------- Required Vtable Operations -----------------------*/

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
	n->signature_type = tstring_dup(s->signature_type);
	n->minimum_parameters = s->minimum_parameters;
	n->maximum_parameters = s->maximum_parameters;
	return n;
}

static void tcppsessf_free(void *self)
{
	tcppsessf *s = (tcppsessf *)self;
	tstring_free(s->name);
	tstring_free(s->signature_type);
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

/*------------------------------ Capabilities ------------------------------*/

/*
 * Session-native wrappers use the same VM invocation boundary as ordinary
 * native functions and consequently expose no data-object capabilities.
 */

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
	tcfn_descriptor descriptor = TCFN_SESSION_DESCRIPTOR(
		name, f, TCFN_VARIADIC(nullptr, 0));
	return tcppsessf_new_descriptor(&descriptor);
}

tcppsessf *tcppsessf_new_descriptor(const tcfn_descriptor *descriptor)
{
	tcppsessf *s = (tcppsessf *)calloc(1, sizeof(tcppsessf));
	s->base.vtable = &tcppsessf_vtable;
	s->f = descriptor ? descriptor->session_function : nullptr;
	s->name = tstring_new(descriptor && descriptor->name ?
		descriptor->name : "");
	s->signature_type = tstring_new(descriptor && descriptor->signature.type ?
		descriptor->signature.type : "");
	s->minimum_parameters = descriptor ?
		descriptor->signature.minimum_parameters : 0;
	s->maximum_parameters = descriptor ?
		descriptor->signature.maximum_parameters : UNDEF_NPARAMS;
	return s;
}

sessf_t tcppsessf_get_f(tcppsessf *s)
{
	return s->f;
}

const char *tcppsessf_get_signature_type(const tcppsessf *s)
{
	return s ? tstring_cstr(s->signature_type) : nullptr;
}

int tcppsessf_accepts(const tcppsessf *s, uint_regs count)
{
	return s && count >= s->minimum_parameters &&
		(s->maximum_parameters == UNDEF_NPARAMS ||
		 count <= s->maximum_parameters);
}
