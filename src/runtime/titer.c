#include "tapas/runtime/titer.h"

#include <stdlib.h>

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
	n->current = it->current;
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

tcompo_vtable titer_vtable = {
	titer_get_type,	     titer_get_code,	 titer_len,
	titer_copy,	     titer_free,	 titer_identical,
	titer_tostring_abbr, titer_tostring_full
};

titer *titer_new_step(long start, long step, long end)
{
	titer *it = (titer *)calloc(1, sizeof(titer));
	it->base.vtable = &titer_vtable;
	it->start = start;
	it->step = step;
	it->end = end;
	it->current = start;
	return it;
}

titer *titer_new(long start, long end)
{
	return titer_new_step(start, 1, end);
}

int titer_next(titer *it)
{
	it->current += it->step;
	return (it->step > 0 && it->current < it->end) ||
	       (it->step < 0 && it->current > it->end);
}

int titer_next_at(titer *it, long *iter_pos, tobj *vre)
{
	long val = it->start + (*iter_pos) * it->step;
	if ((it->step > 0 && val < it->end) ||
	    (it->step < 0 && val > it->end)) {
		tobj_set_int(vre, val);
		(*iter_pos)++;
		return 1;
	}
	*iter_pos = 0;
	return 0;
}

void titer_get_v_at_loc(titer *it, tobj *vre)
{
	tobj_set_int(vre, it->current);
}

void titer_iter_restore(titer *it)
{
	it->current = it->start;
}

int titer_in(titer *it, const tobj *v)
{
	if (v->type != tint)
		return 0;
	long val = v->val.v_tint;
	if (it->step > 0)
		return val >= it->start && val < it->end &&
		       (val - it->start) % it->step == 0;
	return val <= it->start && val > it->end &&
	       (it->start - val) % (-it->step) == 0;
}
