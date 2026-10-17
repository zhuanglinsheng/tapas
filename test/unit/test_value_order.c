#include "tapas/objects/tlist.h"
#include "tapas/objects/tstr.h"

#include <assert.h>
#include <limits.h>
#include <math.h>

int main(void)
{
	tobj a = { 0 }, b = { 0 };
	int order;
	const long integers[] = { LONG_MIN, -9007199254740993L, -1, 0,
		1, 9007199254740993L, LONG_MAX };
	const double reals[] = { -INFINITY, (double)LONG_MIN, -9007199254740992.0,
		-0.5, -0.0, 0.5, 9007199254740992.0, -(double)LONG_MIN, INFINITY };
	for (unsigned i = 0; i < sizeof(integers) / sizeof(*integers); i++) {
		for (unsigned j = 0; j < sizeof(reals) / sizeof(*reals); j++) {
			tobj_set_int(&a, integers[i]);
			tobj_set_float(&b, reals[j]);
			assert(tobj_compare(&a, &b, &order));
			int reverse;
			assert(tobj_compare(&b, &a, &reverse));
			assert(order == -reverse);
		}
	}
	tobj_set_float(&a, NAN);
	assert(!tobj_is_orderable(&a));
	assert(!tobj_compare(&a, &a, &order));
	const char left[] = { 'a', '\0', 'b' };
	const char right[] = { 'a', '\0', 'c' };
	tobj_set_compo(&a, (tcompo_v *)tstr_new_len(left, sizeof(left)));
	tobj_set_compo(&b, (tcompo_v *)tstr_new_len(right, sizeof(right)));
	assert(tobj_compare(&a, &b, &order) && order < 0);
	tobj_try_clear(&a);
	tobj_try_clear(&b);
	// Reject cycles without leaving a recursive depth counter poisoned.
	tlist *cycle = tlist_new();
	tobj_set_compo(&a, (tcompo_v *)cycle);
	a.val.v_tcompo->refctr++;
	tobj_vec_push(&cycle->items, &a);
	assert(!tobj_is_orderable(&a));
	assert(!tobj_compare(&a, &a, &order));
	tobj_set_int(&b, 3);
	tlist_set_at(cycle, 0, &b);
	assert(tobj_is_orderable(&a));
	assert(tobj_compare(&a, &a, &order) && order == 0);
	tobj_ddc_ref_clear(&a);
	// Sort must retain a transitive fallback even for incompatible categories.
	tobj values[7] = { 0 };
	tobj_set_int(&values[0], 1);
	tobj_set_float(&values[1], 1.0);
	tobj_set_compo(&values[2], (tcompo_v *)tstr_new("a"));
	for (unsigned i = 3; i < 7; i++) {
		tlist *list = tlist_new();
		tobj_vec_push(&list->items, &values[i - 3]);
		tobj_set_compo(&values[i], (tcompo_v *)list);
	}
	for (unsigned i = 0; i < 7; i++)
		if (values[i].type == tcompo)
			values[i].val.v_tcompo->refctr++;
	for (unsigned i = 0; i < 7; i++) {
		assert(tobj_is_orderable(&values[i]));
		for (unsigned j = 0; j < 7; j++) {
			int ij, ji;
			tobj_compare(&values[i], &values[j], &ij);
			tobj_compare(&values[j], &values[i], &ji);
			assert(ij == -ji);
			for (unsigned k = 0; k < 7; k++) {
				int jk, ik;
				tobj_compare(&values[j], &values[k], &jk);
				tobj_compare(&values[i], &values[k], &ik);
				assert(!(ij <= 0 && jk <= 0) || ik <= 0);
			}
		}
	}
	for (unsigned i = 7; i > 0; i--)
		tobj_ddc_ref_clear(&values[i - 1]);
	return 0;
}
