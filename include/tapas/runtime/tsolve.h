#ifndef TAPAS_RUNTIME_TSOLVE_H
#define TAPAS_RUNTIME_TSOLVE_H
#include "tapas/runtime/trule.h"
#include "tapas/runtime/ttype.h"
/* Produces a HoldResult and an optional candidate. The VM verifies candidates. */
typedef struct tsolve_worker tsolve_worker;
void tsolve_worker_free(tsolve_worker *worker);
void tsolve_hold(tsolve_worker **worker, const tobj *input, tobj *result);
void tsolve_reject_witness(tobj *result, const char *reason);
ttypeval *tsolve_result_type(void);
#endif
