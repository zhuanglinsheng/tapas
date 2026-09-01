#ifndef TAPAS_RUNTIME_TITER_H
#define TAPAS_RUNTIME_TITER_H

#include "tapas/tval.h"

#ifdef __cplusplus
extern "C" {
#endif

struct titer {
	tcompo_v base;
	long start;
	long end;
	long step;
	long current;
};

extern tcompo_vtable titer_vtable;

titer *titer_new_step(long start, long step, long end);
titer *titer_new(long start, long end);
int titer_next(titer *it);
int titer_next_at(titer *it, long *iter_pos, tobj *vre);
void titer_get_v_at_loc(titer *it, tobj *vre);
void titer_iter_restore(titer *it);
int titer_in(titer *it, const tobj *v);

#ifdef __cplusplus
}
#endif

#endif /* TAPAS_RUNTIME_TITER_H */
