/* Native List sorting versus libc qsort on the same 16-byte Tapas values.
 * The builtin includes validation; the qsort control receives valid values.
 * Copying is timed equally; input construction and output checks are excluded. */
#include "tapas/objects/tlist.h"
#include "tapas/textension.h"
#include "tapas/tstdlib.h"

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int floats;
static uint64_t comparisons;
static uint64_t array_writes;

static int compare_ints(const void *left, const void *right)
{
	long a = ((const tobj *)left)->val.v_tint;
	long b = ((const tobj *)right)->val.v_tint;
	return (a > b) - (a < b);
}

static int compare_floats(const void *left, const void *right)
{
	double a = ((const tobj *)left)->val.v_tfloat;
	double b = ((const tobj *)right)->val.v_tfloat;
	return (a > b) - (a < b);
}

static int compare_counted(const tobj *a, const tobj *b)
{
	comparisons++;
	return floats ? compare_floats(a, b) : compare_ints(a, b);
}

static int qsort_counted(const void *a, const void *b)
{
	return compare_counted(a, b);
}

static inline void sort_swap(tobj *a, tobj *b)
{
	if (a == b) return;
	array_writes += 2;
	tobj value = *a; *a = *b; *b = value;
}

/* A separate, untimed instantiation counts comparisons without changing the
 * production builtin or using instrumented timings as performance evidence. */
#define SORT_STORE(slot, value) (array_writes++, (slot) = (value))
#define SORT_TWO_ENDED
#define SORT_PREFIX counted_integer
#define SORT_COMPARE(a, b) compare_counted(a, b)
#include "stdlib/builtins/sort_impl.h"

#define SORT_STORE(slot, value) (array_writes++, (slot) = (value))
#define SORT_TWO_ENDED
#define SORT_DOMINANT_EQUAL
#define SORT_PREFIX counted_float
#define SORT_COMPARE(a, b) compare_counted(a, b)
#include "stdlib/builtins/sort_impl.h"

#define SORT_STORE(slot, value) (array_writes++, (slot) = (value))
#define SORT_PREFIX counted_single
#define SORT_COMPARE(a, b) compare_counted(a, b)
#include "stdlib/builtins/sort_impl.h"

/* Scalar-domain control mirrors the builtin's tag, homogeneity and NaN
 * checks. Its checksum keeps the homogeneity reduction observable. This is
 * a diagnostic for generated numeric arrays, not a replacement validator. */
static volatile unsigned validation_checksum;

static void validate_numbers(const tobj *data, uint_count size)
{
	ttypes homogeneous = size ? data[0].type : tnil;
	for (uint_count i = 0; i < size; i++) {
		if (data[i].type != homogeneous)
			homogeneous = tnil;
		assert(data[i].type == tint ||
		       (data[i].type == tfloat && !isnan(data[i].val.v_tfloat)));
	}
	validation_checksum += homogeneous;
}

static uint64_t cpu_ns(void)
{
	struct timespec value;
	assert(clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &value) == 0);
	return (uint64_t)value.tv_sec * 1000000000 + value.tv_nsec;
}

int main(int argc, char **argv)
{
	if (argc != 5) return 2;
	floats = atoi(argv[1]);
	unsigned pattern = (unsigned)strtoul(argv[2], nullptr, 10);
	uint_count n = (uint_count)strtoul(argv[3], nullptr, 10);
	unsigned runs = (unsigned)strtoul(argv[4], nullptr, 10);
	if (floats < 0 || floats > 1 || pattern > 10 || n < 8 || n > 1000000 ||
	    runs < 3 || runs > 1000) return 2;
	int (*compare)(const void *, const void *) = floats ? compare_floats : compare_ints;
	size_t bytes = n * sizeof(tobj);
	tobj *input = calloc(n, sizeof(tobj));
	tobj *expected = malloc(bytes);
	uint64_t *samples[4];
	for (unsigned i = 0; i < 4; i++) samples[i] = calloc(runs, sizeof(uint64_t));
	assert(input && expected && samples[0] && samples[1] && samples[2] && samples[3]);
	uint32_t seed = 42;
	for (uint_count i = 0; i < n; i++) {
		seed = seed * 1664525u + 1013904223u;
		long key = (long)(seed % 2000001) - 1000000;
		if (pattern == 1 || pattern == 2 || pattern == 5) key = i;
		if (pattern == 3) key = 7;
		if (pattern == 4) key = i % 8;
		if (pattern == 6) key = i < n / 2 ? i : n - i;
		if (pattern == 7) key = (seed >> 16) % 8;
		if (pattern == 8) key = (seed >> 16) % 100 < 95 ? 0 : (seed >> 16) % 8 + 1;
		if (pattern == 9) key = i % 7;
		if (pattern == 10) key = i % 31;
		if (floats) tobj_set_float(&input[i], key * 0.5);
		else tobj_set_int(&input[i], key);
	}
	if (pattern == 2)
		for (uint_count i = 0; i < n / 2; i++)
			sort_swap(&input[i], &input[n - i - 1]);
	if (pattern == 5)
		sort_swap(&input[n / 3], &input[n * 2 / 3]);
	memcpy(expected, input, bytes);
	qsort(expected, n, sizeof(tobj), compare);
	textension_symbol_ref found;
	assert(textension_find(tstdlib_descriptor(), nullptr, "sort", &found));
	tnative_function sort = found.symbol->function;
	tlist *working = tlist_new_sized(n);
	working->items.len = n;
	tobj argument = { 0 }, result = { 0 };
	tobj_set_compo(&argument, (tcompo_v *)working);
	unsigned batches = (65536 + n - 1) / n;
	for (unsigned run = 0; run <= runs; run++) {
		for (unsigned pass = 0; pass < 4; pass++) {
			unsigned method = (run + pass) % 4;
			uint64_t started = cpu_ns();
			for (unsigned batch = 0; batch < batches; batch++) {
				memcpy(working->items.data, input, bytes);
				if (method == 1) {
					sort(&argument, 1, &result);
				} else {
					if (method >= 2) validate_numbers(working->items.data, n);
					if (method != 3) qsort(working->items.data, n, sizeof(tobj), compare);
				}
			}
			uint64_t elapsed = cpu_ns() - started;
			if (run) samples[method][run - 1] = elapsed;
			assert(memcmp(working->items.data, method == 3 ? input : expected, bytes) == 0);
		}
	}
	comparisons = 0;
	array_writes = 0;
	memcpy(working->items.data, input, bytes);
	if (floats) counted_float_values(working->items.data, n);
	else counted_integer_values(working->items.data, n);
	uint64_t native_comparisons = comparisons;
	uint64_t selected_writes = array_writes;
	comparisons = 0;
	array_writes = 0;
	memcpy(working->items.data, input, bytes);
	counted_single_values(working->items.data, n);
	uint64_t single_comparisons = comparisons, single_writes = array_writes;
	assert(memcmp(working->items.data, expected, bytes) == 0);
	comparisons = 0;
	memcpy(working->items.data, input, bytes);
	qsort(working->items.data, n, sizeof(tobj), qsort_counted);
	printf("{\"value_bytes\":%zu,\"size\":%u,\"pattern\":%u,\"float\":%d,\"batches\":%u,"
	       "\"comparisons\":{\"qsort\":%llu,\"tapas\":%llu},", sizeof(tobj), (unsigned)n, pattern, floats, batches,
	       (unsigned long long)comparisons, (unsigned long long)native_comparisons);
	printf("\"partition_control\":{\"single_comparisons\":%llu,\"single_writes\":%llu,\"selected_writes\":%llu},",
	       (unsigned long long)single_comparisons,
	       (unsigned long long)single_writes, (unsigned long long)selected_writes);
	const char *names[] = {"qsort", "tapas", "validated_qsort", "validation"};
	for (unsigned method = 0; method < 4; method++) {
		printf("\"%s_ns\":[", names[method]);
		for (unsigned i = 0; i < runs; i++)
			printf("%s%llu", i ? "," : "", (unsigned long long)samples[method][i]);
		printf("]%s", method == 3 ? "}\n" : ",");
	}
	tobj_try_clear(&argument);
	free(input);
	free(expected);
	for (unsigned i = 0; i < 4; i++) free(samples[i]);
	return 0;
}
