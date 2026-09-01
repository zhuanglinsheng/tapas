#ifndef TAPAS_RUNTIME_TPAIR_H
#define TAPAS_RUNTIME_TPAIR_H

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

#endif /* TAPAS_RUNTIME_TPAIR_H */
