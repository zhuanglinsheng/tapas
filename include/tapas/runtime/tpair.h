#ifndef T_RUNTIME_PAIR_H
#define T_RUNTIME_PAIR_H

#include "tapas/tval.h"

#ifdef __cplusplus
extern "C" {
#endif

struct tpair {
	tcompo_v base;
	tobj first;
	tobj second;
};

extern tcompo_vtable tpair_vtable;

tpair *tpair_new(const tobj *first, const tobj *second);
void tpair_idx(tpair *p, const tobj *params, uint_regs np, tobj *vre);
void tpair_iset(tpair *p, const tobj *params, uint_regs np,
		const tobj *vright);

#ifdef __cplusplus
}
#endif

#endif /* T_RUNTIME_PAIR_H */
