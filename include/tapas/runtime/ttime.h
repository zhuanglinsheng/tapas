#ifndef TAPAS_RUNTIME_TTIME_H
#define TAPAS_RUNTIME_TTIME_H

#include "tapas/tval.h"

#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ttime {
	tcompo_v base;
	time_t value;
};

extern tcompo_vtable ttime_vtable;

ttime *ttime_new(void);
ttime *ttime_from_time(time_t value);
time_t ttime_get(const ttime *value);

#ifdef __cplusplus
}
#endif

#endif /* TAPAS_RUNTIME_TTIME_H */
