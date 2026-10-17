#ifndef TAPAS_RUNTIME_VALUE_ORDER_H
#define TAPAS_RUNTIME_VALUE_ORDER_H

#include "tapas/tval.h"

#include <limits.h>
#include <math.h>

/* Keep non-recursive numeric ordering inline for the VM's native clients.
 * Composite dispatch and recursive List traversal stay in tval.c. */
/* Mixed comparisons must not round a large Int to double. The upper bound
 * is exclusive, including platforms where LONG_MAX rounds upward. */
static inline int compare_int_float(long integer, double real)
{
	if (real < (double)LONG_MIN)
		return 1;
	if (real >= -(double)LONG_MIN)
		return -1;
	long integral = (long)real;
	if (integer != integral)
		return (integer > integral) - (integer < integral);
	return ((double)integral > real) - ((double)integral < real);
}

static inline int tobj_compare_numbers(const tobj *a, const tobj *b, int *order)
{
	if (a->type == tint && b->type == tint) {
		*order = (a->val.v_tint > b->val.v_tint) -
			 (a->val.v_tint < b->val.v_tint);
		return 1;
	}
	if ((a->type == tint || a->type == tfloat) &&
	    (b->type == tint || b->type == tfloat)) {
		if ((a->type == tfloat && isnan(a->val.v_tfloat)) ||
		    (b->type == tfloat && isnan(b->val.v_tfloat)))
			return 0;
		if (a->type == tint)
			*order = compare_int_float(a->val.v_tint, b->val.v_tfloat);
		else if (b->type == tint)
			*order = -compare_int_float(b->val.v_tint, a->val.v_tfloat);
		else
			*order = (a->val.v_tfloat > b->val.v_tfloat) -
				 (a->val.v_tfloat < b->val.v_tfloat);
		return 1;
	}
	return 0;
}

#endif /* TAPAS_RUNTIME_VALUE_ORDER_H */
