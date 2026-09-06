#ifndef TAPAS_RUNTIME_TCFN_H
#define TAPAS_RUNTIME_TCFN_H

#include "tapas/tval.h"
#include "tapas/runtime/tfunction_metadata.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*genf_t)(tobj *params, uint_regs len, tobj *vre);
typedef void (*sessf_t)(tobj *params, uint_regs len, tobj *vre,
			tcompo_env *env);

typedef struct {
	const char *type;
	uint_regs minimum_parameters;
	uint_regs maximum_parameters;
} tcfn_signature;

typedef struct {
	const char *name;
	genf_t function;
	sessf_t session_function;
	tcfn_signature signature;
} tcfn_descriptor;

#define TCFN_FIXED(type_, count_) \
	((tcfn_signature){ (type_), (count_), (count_) })
#define TCFN_RANGE(type_, minimum_, maximum_) \
	((tcfn_signature){ (type_), (minimum_), (maximum_) })
#define TCFN_VARIADIC(type_, minimum_) \
	TCFN_RANGE((type_), (minimum_), UNDEF_NPARAMS)
#define TCFN_DESCRIPTOR(name_, function_, signature_) \
	((tcfn_descriptor){ (name_), (function_), nullptr, (signature_) })
#define TCFN_SESSION_DESCRIPTOR(name_, function_, signature_) \
	((tcfn_descriptor){ (name_), nullptr, (function_), (signature_) })

int tcfn_descriptor_valid(const tcfn_descriptor *descriptor);

struct tcppgenf {
	tcompo_v base;
	tfunction_metadata *metadata;
	genf_t f;
	tstring *name;
	tstring *signature_type;
	uint_regs minimum_parameters;
	uint_regs maximum_parameters;
};

extern tcompo_vtable tcppgenf_vtable;

tcppgenf *tcppgenf_new(genf_t f, const char *name, uint_regs nparams_sig);
tcppgenf *tcppgenf_new_descriptor(const tcfn_descriptor *descriptor);
genf_t tcppgenf_get_f(tcppgenf *g);
uint_regs tcppgenf_get_nparams_sig(tcppgenf *g);
const char *tcppgenf_get_signature_type(const tcppgenf *g);
uint_regs tcppgenf_get_minimum_parameters(const tcppgenf *g);
uint_regs tcppgenf_get_maximum_parameters(const tcppgenf *g);
int tcppgenf_accepts(const tcppgenf *g, uint_regs count);

struct tcppsessf {
	tcompo_v base;
	tfunction_metadata *metadata;
	sessf_t f;
	tstring *name;
	tstring *signature_type;
	uint_regs minimum_parameters;
	uint_regs maximum_parameters;
};

extern tcompo_vtable tcppsessf_vtable;

tcppsessf *tcppsessf_new(sessf_t f, const char *name);
tcppsessf *tcppsessf_new_descriptor(const tcfn_descriptor *descriptor);
sessf_t tcppsessf_get_f(tcppsessf *s);
const char *tcppsessf_get_signature_type(const tcppsessf *s);
int tcppsessf_accepts(const tcppsessf *s, uint_regs count);

#ifdef __cplusplus
}
#endif

#endif /* TAPAS_RUNTIME_TCFN_H */
