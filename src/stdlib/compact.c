#include "compact.h"
#include "ranges.h"

#include "tapas/dsa/tstring.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static size_t item_size(tcompact_kind kind)
{
	return kind == tcompact_real ? sizeof(double) : 1;
}

tcompact *tcompact_new(tcompo_vtable *vtable, tcompact_kind kind, size_t length,
			 tcompo_v *owner, void *data)
{
	if (length > LONG_MAX || length > SIZE_MAX / item_size(kind))
		twarn(ErrRuntime_IntOutOfRange, "compact sequence", "length exceeds Int");
	tcompact *sequence = calloc(1, sizeof(*sequence));
	if (!sequence)
		twarn(ErrRuntime_Other, "compact sequence", "out of memory");
	sequence->base.vtable = vtable;
	sequence->kind = kind;
	sequence->length = length;
	sequence->owner = owner;
	if (owner) {
		owner->refctr++;
		sequence->data = data;
	} else if (length) {
		sequence->data = malloc(length * item_size(kind));
		if (!sequence->data) {
			free(sequence);
			twarn(ErrRuntime_Other, "compact sequence", "out of memory");
		}
	}
	return sequence;
}

long tcompact_len(void *self)
{
	return (long)((tcompact *)self)->length;
}

void *tcompact_copy(void *self)
{
	tcompact *source = self;
	tcompact *copy = tcompact_new(source->base.vtable, source->kind,
				     source->length, nullptr, nullptr);
	if (source->length)
		memcpy(copy->data, source->data, source->length * item_size(source->kind));
	return copy;
}

void tcompact_free(void *self)
{
	tcompact *sequence = self;
	if (sequence->owner) {
		tobj owner = {0};
		tobj_set_compo(&owner, sequence->owner);
		tobj_ddc_ref_clear(&owner);
	} else {
		free(sequence->data);
	}
	free(sequence);
}

int tcompact_identical(void *self, void *other)
{
	tcompact *a = self, *b = other;
	if (a->kind != b->kind || a->length != b->length)
		return 0;
	if (a->kind == tcompact_real) {
		const double *left = a->data, *right = b->data;
		for (size_t i = 0; i < a->length; i++)
			if (left[i] != right[i])
				return 0;
		return 1;
	}
	return !a->length || memcmp(a->data, b->data, a->length) == 0;
}

tstring *tcompact_string(void *self)
{
	tcompact *sequence = self;
	return tobj_tostring_pointer(sequence->base.vtable->get_type(), self);
}

static int index_position(const tcompact *sequence, const tobj *arguments,
			  uint_regs count, long *position)
{
	if (count != 1)
		twarn(ErrRuntime_ParamsCtr, "compact sequence index", "one index required");
	if (arguments[0].type != tint)
		twarn(ErrRuntime_ParamsType, "compact sequence index", "Int required");
	*position = arguments[0].val.v_tint;
	if (*position < 0)
		*position += (long)sequence->length;
	return *position >= 0 && (size_t)*position < sequence->length;
}

static void read_item(const tcompact *sequence, size_t position, tobj *result)
{
	if (sequence->kind == tcompact_real)
		tobj_set_float(result, ((double *)sequence->data)[position]);
	else if (sequence->kind == tcompact_bool)
		tobj_set_bool(result, ((unsigned char *)sequence->data)[position] != 0);
	else
		tobj_set_int(result, ((unsigned char *)sequence->data)[position]);
}

static int compact_try_index(void *self, const tobj *arguments,
			     uint_regs count, tobj *result)
{
	long position;
	if (!index_position(self, arguments, count, &position))
		return 0;
	read_item(self, (size_t)position, result);
	return 1;
}

static void compact_index(void *self, const tobj *arguments,
			  uint_regs count, tobj *result)
{
	if (!compact_try_index(self, arguments, count, result))
		twarn(ErrRuntime_IdxOutRange, "compact sequence index", "index out of range");
}

static double write_value(const tcompact *sequence, const tobj *value)
{
	if (sequence->kind == tcompact_real && (value->type == tint || value->type == tfloat))
		return value->type == tint ? (double)value->val.v_tint : value->val.v_tfloat;
	if (sequence->kind == tcompact_bool && value->type == tbool)
		return value->val.v_tbool != 0;
	if (sequence->kind == tcompact_byte && value->type == tint &&
	    value->val.v_tint >= 0 && value->val.v_tint <= 255)
		return (double)value->val.v_tint;
	twarn(ErrRuntime_ParamsType, "compact sequence write", "value does not match storage type");
	return 0;
}

static void compact_set(void *self, const tobj *arguments,
			uint_regs count, const tobj *value)
{
	tcompact *sequence = self;
	long position;
	if (!index_position(sequence, arguments, count, &position))
		twarn(ErrRuntime_IdxOutRange, "compact sequence write", "index out of range");
	double replacement = write_value(sequence, value);
	if (sequence->kind == tcompact_real)
		((double *)sequence->data)[position] = replacement;
	else
		((unsigned char *)sequence->data)[position] = (unsigned char)replacement;
}

static int compact_next(void *self, long *position, tobj *result)
{
	tcompact *sequence = self;
	if (*position < 0 || (size_t)*position >= sequence->length)
		return 0;
	read_item(sequence, (size_t)(*position)++, result);
	return 1;
}

/* Bool and byte storage both use one byte, but retain distinct Type rules. */
static int byte_key(const tcompact *sequence, const tobj *value, unsigned char *key)
{
	if (sequence->kind == tcompact_bool) {
		if (value->type != tbool)
			return 0;
		*key = (unsigned char)value->val.v_tbool;
		return 1;
	}
	if (value->type != tint || value->val.v_tint < 0 || value->val.v_tint > 255)
		return 0;
	*key = (unsigned char)value->val.v_tint;
	return 1;
}

static int compact_count(void *self, const tobj *value, long *result)
{
	tcompact *sequence = self;
	long count = 0;
	if (sequence->kind == tcompact_real) {
		if (value->type == tfloat) {
			const double *data = sequence->data;
			for (size_t i = 0; i < sequence->length; i++)
				count += data[i] == value->val.v_tfloat;
		}
	} else {
		unsigned char key;
		if (byte_key(sequence, value, &key)) {
			const unsigned char *data = sequence->data;
			for (size_t i = 0; i < sequence->length; i++)
				count += data[i] == key;
		}
	}
	*result = count;
	return 1;
}

/* Isolate the libc call so real membership keeps its original leaf loop.
 * Inlining this helper adds a stack frame to the real path on ARM64. */
#if defined(__GNUC__)
__attribute__((noinline))
#endif
static int contains_byte(const tcompact *sequence, const tobj *value)
{
	unsigned char key;
	return byte_key(sequence, value, &key) && sequence->length &&
		memchr(sequence->data, key, sequence->length) != nullptr;
}

static int compact_contains(void *self, const tobj *value)
{
	tcompact *sequence = self;
	if (sequence->kind != tcompact_real)
		return contains_byte(sequence, value);
	/* Keep the vectorized count path for real storage. */
	long count;
	compact_count(self, value, &count);
	return count != 0;
}

static int compact_fill(void *self, const tobj *value, const tcompo_v *indices)
{
	tcompact *sequence = self;
	if (indices->vtable != &titer_vtable)
		return 0;
	tstdlib_range range;
	if (!tstdlib_range_in_bounds((const titer *)indices, (long)sequence->length, &range))
		return 0;
	if (!range.count)
		return 1;
	double replacement = write_value(sequence, value);
	if (sequence->kind != tcompact_real && (range.step == 1 || range.step == -1)) {
		long first = range.step > 0 ? range.start : range.start - (long)(range.count - 1);
		memset((unsigned char *)sequence->data + first, (unsigned char)replacement, range.count);
		return 1;
	}
	long position = range.start;
	if (sequence->kind == tcompact_real) {
		double *data = sequence->data;
		for (size_t remaining = range.count; remaining; remaining--) {
			data[position] = replacement;
			if (remaining > 1)
				position += range.step;
		}
	} else {
		unsigned char *data = sequence->data;
		for (size_t remaining = range.count; remaining; remaining--) {
			data[position] = (unsigned char)replacement;
			if (remaining > 1)
				position += range.step;
		}
	}
	return 1;
}

const tcompo_capabilities tcompact_capabilities = {
	.indexable = compact_index,
	.try_indexable = compact_try_index,
	.index_settable = compact_set,
	.contains = compact_contains,
	.iterable = compact_next
};

const tcompo_bulk_operations tcompact_bulk_operations = {
	.fill = compact_fill,
	.count = compact_count
};
