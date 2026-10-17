#ifndef TAPAS_STDLIB_RANGES_H
#define TAPAS_STDLIB_RANGES_H

#include "tapas/objects/titer.h"

#include <limits.h>
#include <stddef.h>

typedef struct {
	long start;
	long step;
	size_t count;
} tstdlib_range;

/* Prove all normalized indices valid before choosing an unchecked loop.
 * A negative-to-positive crossing wraps around the sequence, so it keeps the
 * ordinary per-index path. Failure makes no writes, preserving partial writes
 * and the first error of that path. Unsigned differences avoid Int overflow. */
static inline int tstdlib_range_in_bounds(const titer *range, long length,
					 tstdlib_range *result)
{
	long start = range->start, end = range->end, step = range->step;
	*result = (tstdlib_range){0, step, 0};
	if (!step || (step > 0 ? start >= end : start <= end))
		return 1;
	unsigned long distance = step > 0 ?
		(unsigned long)end - (unsigned long)start :
		(unsigned long)start - (unsigned long)end;
	unsigned long stride = step > 0 ? (unsigned long)step :
		0UL - (unsigned long)step;
	unsigned long remainder = (distance - 1) % stride;
	long last = step > 0 ? end - 1 - (long)remainder :
		end + 1 + (long)remainder;
	if ((start < 0) != (last < 0))
		return 0;
	if (start < 0) {
		if (start < -length || last < -length)
			return 0;
		start += length;
		last += length;
	}
	if (start < 0 || last < 0 || start >= length || last >= length)
		return 0;
	result->start = start;
	result->count = (distance - 1) / stride + 1;
	return 1;
}

#endif
