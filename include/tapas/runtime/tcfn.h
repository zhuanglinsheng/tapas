#ifndef T_RUNTIME_CFN_H
#define T_RUNTIME_CFN_H

#include "tapas/tval.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*genf_t)(tobj *params, uint_regs len, tobj *vre);

struct tcppgenf {
	tcompo_v base;
	genf_t f;
	tstring *name;
	uint_regs nparams_sig;
};

extern tcompo_vtable tcppgenf_vtable;

tcppgenf *tcppgenf_new(genf_t f, const char *name,
		       uint_regs nparams_sig);
genf_t tcppgenf_get_f(tcppgenf *g);
uint_regs tcppgenf_get_nparams_sig(tcppgenf *g);

typedef void (*sessf_t)(tobj *params, uint_regs len, tobj *vre,
			tcompo_env *env);

struct tcppsessf {
	tcompo_v base;
	sessf_t f;
	tstring *name;
};

extern tcompo_vtable tcppsessf_vtable;

tcppsessf *tcppsessf_new(sessf_t f, const char *name);
sessf_t tcppsessf_get_f(tcppsessf *s);

#ifdef __cplusplus
}
#endif

#endif /* T_RUNTIME_CFN_H */
