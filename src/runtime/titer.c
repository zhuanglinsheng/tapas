#include "tapas/runtime/titer.h"

#include <stdlib.h>

titer *titer_new_step(long start, long step, long end)
{
	titer *iterator = (titer *)calloc(1, sizeof(titer));
	iterator->base.vtable = &titer_vtable;
	iterator->start = start;
	iterator->step = step;
	iterator->end = end;
	return iterator;
}

titer *titer_new(long start, long end)
{
	return titer_new_step(start, 1, end);
}

/*----------------------- Required Vtable Operations -----------------------*/

static const char *titer_get_type(void)
{
	return "Iterator";
}

static tcompo_type titer_get_code(void)
{
	return compo_titer;
}

static long titer_len(void *self)
{
	titer *it = (titer *)self;
	if (it->step > 0) {
		if (it->start >= it->end)
			return 0;
		return (it->end - it->start + it->step - 1) / it->step;
	}
	if (it->step < 0) {
		long step_abs = -it->step;
		if (it->start <= it->end)
			return 0;
		return (it->start - it->end + step_abs - 1) / step_abs;
	}
	return 0;
}

static void *titer_copy(void *self)
{
	titer *it = (titer *)self;
	titer *n = (titer *)calloc(1, sizeof(titer));
	n->base.vtable = it->base.vtable;
	n->start = it->start;
	n->end = it->end;
	n->step = it->step;
	return n;
}

static void titer_free(void *self)
{
	free(self);
}

static int titer_identical(void *self, void *other)
{
	titer *a = (titer *)self;
	titer *b = (titer *)other;
	return a->start == b->start && a->end == b->end && a->step == b->step;
}

static tstring *titer_tostring_abbr(void *self)
{
	return tobj_tostring_pointer("Iterator", self);
}

static tstring *titer_tostring_full(void *self)
{
	titer *it = (titer *)self;
	tstring *out = tstring_new_empty();
	tstring_append_fmt(out, "%ld : %ld", it->start, it->end);
	return out;
}

/*------------------------------ Capabilities ------------------------------*/

/*
 * Range Iterator supports membership and iteration. Its iteration cursor is
 * supplied by the caller, so the object is an immutable range description and
 * the same Iterator can be traversed concurrently or in nested loops.
 */

static int iterator_next(void *self, long *position, tobj *result)
{
	titer *it = (titer *)self;
	long value = it->start + *position * it->step;
	if ((it->step > 0 && value < it->end) ||
	    (it->step < 0 && value > it->end)) {
		tobj_set_int(result, value);
		(*position)++;
		return 1;
	}
	return 0;
}

static int iterator_contains(void *self, const tobj *value)
{
	titer *it = (titer *)self;
	if (value->type != tint)
		return 0;
	long candidate = value->val.v_tint;
	if (it->step > 0)
		return candidate >= it->start && candidate < it->end &&
		       (candidate - it->start) % it->step == 0;
	return candidate <= it->start && candidate > it->end &&
	       (it->start - candidate) % (-it->step) == 0;
}

static const tcompo_capabilities iterator_capabilities = {
	.contains = iterator_contains,
	.iterable = iterator_next
};

tcompo_vtable titer_vtable = {
	.get_type = titer_get_type,
	.get_compo_type_code = titer_get_code,
	.len = titer_len,
	.copy = titer_copy,
	.free = titer_free,
	.identical = titer_identical,
	.tostring_abbr = titer_tostring_abbr,
	.tostring_full = titer_tostring_full,
	.capabilities = &iterator_capabilities
};
