#include "tapas/objects/titer.h"
#include "tapas/objects/tlist.h"
#include "tapas/textension.h"
#include "tapas/tstdlib.h"
#include "../../src/stdlib/ranges.h"

#include <assert.h>
#include <limits.h>
#include <stddef.h>

static tnative_function function(const char *package, const char *name)
{
	textension_symbol_ref found;
	assert(textension_find(tstdlib_descriptor(), package, name, &found));
	assert(found.symbol->function);
	return found.symbol->function;
}

static void check_range(long start, long step, long end, long length)
{
	titer iterator = {.start = start, .step = step, .end = end};
	tstdlib_range range;
	if (!tstdlib_range_in_bounds(&iterator, length, &range))
		return;
	long index = start, position = range.start;
	size_t count = 0;
	while (step && (step > 0 ? index < end : index > end)) {
		long normalized = index < 0 ? index + length : index;
		assert(normalized >= 0 && normalized < length);
		assert(position == normalized);
		count++;
		assert(count <= range.count);
		if ((step > 0 && index > LONG_MAX - step) ||
		    (step < 0 && index < LONG_MIN - step))
			break;
		index += step;
		if (count < range.count)
			position += step;
	}
	assert(count == range.count);
}

int main(void)
{
	for (long length = 0; length <= 9; length++)
		for (long start = -12; start <= 12; start++)
			for (long end = -12; end <= 12; end++)
				for (long step = -8; step <= 8; step++)
					check_range(start, step, end, length);
	const long boundaries[] = {LONG_MIN, LONG_MIN + 1, -9, -1, 0, 1, 9,
		LONG_MAX - 1, LONG_MAX};
	for (size_t a = 0; a < sizeof(boundaries) / sizeof(*boundaries); a++)
		for (size_t b = 0; b < sizeof(boundaries) / sizeof(*boundaries); b++)
			for (size_t c = 0; c < sizeof(boundaries) / sizeof(*boundaries); c++)
				check_range(boundaries[a], boundaries[b], boundaries[c], 9);

	tnative_function create = function("bytes", "buffer");
	tnative_function fill = function(nullptr, "fill");
	tnative_function count = function(nullptr, "count");
	const long sizes[] = {0, 1, 7, 128, 1025};
	for (size_t size = 0; size < sizeof(sizes) / sizeof(*sizes); size++) {
		long length = sizes[size];
		tobj arguments[3] = {{0}}, buffer = {0}, fallback = {0}, result = {0};
		tobj_set_int(&arguments[0], length);
		tobj_set_int(&arguments[1], 13);
		create(arguments, 2, &buffer);
		tcompo_vtable generic = *buffer.val.v_tcompo->vtable;
		generic.bulk_operations = nullptr;
		for (long step = -5; step <= 5; step++) {
			tobj_set_compo(&fallback, buffer.val.v_tcompo->vtable->copy(buffer.val.v_tcompo));
			fallback.val.v_tcompo->vtable = &generic;
			tobj_set_compo(&arguments[2], (tcompo_v *)titer_new_step(
				step < 0 ? length - 1 : 0, step, step < 0 ? -1 : length));
			tobj_set_int(&arguments[1], 42);
			arguments[0] = buffer;
			fill(arguments, 3, &result);
			arguments[0] = fallback;
			fill(arguments, 3, &result);
			assert(buffer.val.v_tcompo->vtable->identical(
				buffer.val.v_tcompo, fallback.val.v_tcompo));
			for (long key = -1; key <= 256; key++) {
				tobj_set_int(&arguments[1], key);
				arguments[0] = buffer;
				count(arguments, 2, &result);
				long expected = result.val.v_tint;
				assert(!!tcompo_contains(buffer.val.v_tcompo, &arguments[1]) ==
					(expected != 0));
				arguments[0] = fallback;
				count(arguments, 2, &result);
				assert(result.val.v_tint == expected);
			}
			tobj_try_clear(&fallback);
		}
		/* arguments[0] was borrowed, not an additional owned reference. */
		tobj_try_clear(&buffer);
		arguments[0] = (tobj){0};
		tobj_try_clear(&arguments[2]);
	}
	return 0;
}
