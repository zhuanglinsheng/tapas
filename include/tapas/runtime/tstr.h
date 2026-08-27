#ifndef T_RUNTIME_STR_H
#define T_RUNTIME_STR_H

#include "tapas/tval.h"

#ifdef __cplusplus
extern "C" {
#endif

struct tstr {
	tcompo_v base;
	tstring *data;
};

extern tcompo_vtable tstr_vtable;

tstr *tstr_new(const char *s);
void tstr_idx(tstr *s, const tobj *params, uint_regs np, tobj *vre);
void tstr_iset(tstr *s, const tobj *params, uint_regs np,
	       const tobj *vright);

#ifdef __cplusplus
}
#endif

#endif /* T_RUNTIME_STR_H */
