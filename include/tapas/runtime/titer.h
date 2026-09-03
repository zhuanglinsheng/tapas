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
};

extern tcompo_vtable titer_vtable;

titer *titer_new_step(long start, long step, long end);
titer *titer_new(long start, long end);

#ifdef __cplusplus
}
#endif

#endif /* TAPAS_RUNTIME_TITER_H */
