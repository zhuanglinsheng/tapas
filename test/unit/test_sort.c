#include "tapas/objects/tlist.h"
#include "tapas/objects/tstr.h"
#include "tapas/textension.h"
#include "tapas/tstdlib.h"

#include <assert.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int compare_values(const void *left, const void *right)
{
	int order;
	assert(tobj_compare(left, right, &order));
	return order;
}

/* An independent identity ordering checks the complete multiset, including
 * signed zero, mixed numeric representations and equal-valued object aliases. */
static int compare_identity(const void *left, const void *right)
{
	const tobj *a = left, *b = right;
	if (a->type != b->type)
		return (a->type > b->type) - (a->type < b->type);
	if (a->name_loc != b->name_loc)
		return (a->name_loc > b->name_loc) - (a->name_loc < b->name_loc);
	uint64_t av, bv;
	memcpy(&av, &a->val, sizeof(av));
	memcpy(&bv, &b->val, sizeof(bv));
	return (av > bv) - (av < bv);
}

static void append_integer(tlist *list, long integer)
{
	tobj value = { 0 };
	tobj_set_int(&value, integer);
	tobj_vec_push(&list->items, &value);
}

static void append_value(tlist *list, unsigned kind, long key, unsigned index,
			 int boundary, int duplicate)
{
	tobj value = { 0 };
	const double reals[] = {-INFINITY, -0.0, 0.0, INFINITY,
		DBL_MAX, -DBL_MAX, DBL_MIN, -DBL_MIN};
	if (kind == 0 || (kind == 2 && index % 2 == 0)) {
		tobj_set_int(&value, key);
	} else if (kind <= 2) {
		tobj_set_float(&value, boundary ? reals[index % 8] : key * 0.5);
	} else if (kind == 3) {
		char text[64];
		snprintf(text, sizeof(text), "%020ld", key);
		tobj_set_compo(&value, (tcompo_v *)tstr_new(text));
	} else {
		tlist *record = tlist_new();
		if (kind == 5) {
			tlist *prefix = tlist_new();
			append_integer(prefix, key % 17);
			tobj_set_compo(&value, (tcompo_v *)prefix);
			tobj_vec_push(&record->items, &value);
			tobj_set_nil(&value);
		}
		append_integer(record, key);
		append_integer(record, duplicate ? 0 : index);
		tobj_set_compo(&value, (tcompo_v *)record);
	}
	tobj_vec_push(&list->items, &value);
	tobj_try_clear(&value);
}

int main(void)
{
	textension_symbol_ref found;
	assert(textension_find(tstdlib_descriptor(), nullptr, "sort", &found));
	tnative_function sort = found.symbol->function;
	const unsigned sizes[] = {0, 1, 2, 3, 7, 8, 9, 15, 16, 17, 31, 32,
		63, 64, 127, 128, 129, 511, 512, 513, 4093, 4096, 8191};
	const long boundaries[] = {LONG_MIN, LONG_MAX, 0, -1, 1,
		9007199254740993L, 9007199254740992L, -9007199254740993L};
	for (unsigned kind = 0; kind < 6; kind++) {
		for (unsigned pattern = 0; pattern < 13; pattern++) {
			for (unsigned k = 0; k < sizeof(sizes) / sizeof(*sizes); k++) {
				unsigned n = sizes[k];
				tlist *list = tlist_new_sized(n);
				uint32_t state = 42;
				for (unsigned i = 0; i < n; i++) {
					state = state * 1664525u + 1013904223u;
					long key = (long)(state % 200001) - 100000;
					if (pattern >= 1 && pattern <= 3) key = i;
					if (pattern == 4) key = 7;
					if (pattern == 5) key = i % 8;
					if (pattern == 6) key = i < n / 2 ? i : n - i;
					if (pattern == 7) key = boundaries[i % 8];
					if (pattern == 8) key = (state >> 16) % 8;
					if (pattern == 9) key = (state >> 16) % 100 < 95 ? 0 : (state >> 16) % 8 + 1;
					if (pattern == 10) key = i % 7;
					if (pattern == 11) key = i % 31;
					if (pattern == 12) key = i % 2;
					if (kind >= 3 && i && i % 7 == 0) {
						tobj alias = list->items.data[list->items.len - 1];
						tobj_vec_push(&list->items, &alias);
					} else {
						append_value(list, kind, key, i, pattern == 7,
							     pattern == 4 || pattern == 5 || pattern >= 8);
					}
				}
				if (pattern >= 1 && pattern <= 3) {
					qsort(list->items.data, n, sizeof(tobj), compare_values);
					if (pattern == 2)
						for (unsigned i = 0; i < n / 2; i++) {
							tobj value = list->items.data[i];
							list->items.data[i] = list->items.data[n - i - 1];
							list->items.data[n - i - 1] = value;
						}
					if (pattern == 3 && n > 1) {
						tobj value = list->items.data[n / 3];
						list->items.data[n / 3] = list->items.data[n * 2 / 3];
						list->items.data[n * 2 / 3] = value;
					}
				}
				size_t bytes = n * sizeof(tobj);
				tobj *expected = malloc(bytes ? bytes : 1);
				tobj *identities = malloc(bytes ? bytes : 1);
				tobj *actual = malloc(bytes ? bytes : 1);
				assert(expected && identities && actual);
				if (bytes) {
					memcpy(expected, list->items.data, bytes);
					memcpy(identities, expected, bytes);
				}
				qsort(expected, n, sizeof(tobj), compare_values);
				qsort(identities, n, sizeof(tobj), compare_identity);
				uint_count composites = list->items.compo_count;
				tobj argument = { 0 }, result = { 0 };
				tobj_set_compo(&argument, (tcompo_v *)list);
				sort(&argument, 1, &result);
				assert(result.type == tnil && list->items.len == n);
				assert(list->items.compo_count == composites);
				for (unsigned i = 0; i < n; i++) {
					assert(compare_values(&expected[i], &list->items.data[i]) == 0);
				}
				if (bytes) memcpy(actual, list->items.data, bytes);
				qsort(actual, n, sizeof(tobj), compare_identity);
				for (unsigned i = 0; i < n; i++)
					assert(compare_identity(&actual[i], &identities[i]) == 0);
				for (unsigned i = 0; i < n;) {
					unsigned end = i + 1;
					while (end < n && compare_identity(&actual[i], &actual[end]) == 0)
						end++;
					if (actual[i].type == tcompo)
						assert(actual[i].val.v_tcompo->refctr == (int)(end - i));
					i = end;
				}
				free(actual);
				free(identities);
				free(expected);
				tobj_try_clear(&argument);
			}
		}
	}
	return 0;
}
