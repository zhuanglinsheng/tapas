/**
 * @file solve.h
 * @brief Declares private implementation details of the solve package.
 * @details The worker, result builder, and witness rejection helpers are owned
 * and used exclusively by src/stdlib/solve.
 * @note This is not a VM or public runtime interface.
 */
#ifndef TAPAS_STDLIB_SOLVE_H
#define TAPAS_STDLIB_SOLVE_H

#include "tapas/objects/trule.h"
#include "tapas/objects/ttype.h"

typedef struct tsolve_worker tsolve_worker;

#define TSOLVE_QUERY_TIMEOUT_MS 15000L

void tsolve_worker_free(tsolve_worker *worker);
void tsolve_hold(tsolve_worker **worker, const tobj *input, tobj *result);
void tsolve_hold_timed(tsolve_worker **worker, const tobj *input,
		       tobj *result, long timeout_ms);
void tsolve_hold_timed_quiet(tsolve_worker **worker, const tobj *input,
			     tobj *result, long timeout_ms);
void tsolve_hold_timed_binding_core(tsolve_worker **worker,
				    const tobj *input, tobj *result,
				    long timeout_ms);
void tsolve_sample(tsolve_worker **worker, tobj *params, uint_regs count,
		   tobj *result, tcompo_env *environment);
void tsolve_reject_witness(tobj *result, const char *reason);
ttypeval *tsolve_result_type(void);
ttypeval *tsolve_sample_result_type(void);

#endif /* TAPAS_STDLIB_SOLVE_H */
