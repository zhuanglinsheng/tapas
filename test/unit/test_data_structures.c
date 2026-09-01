#include "tapas/tval.h"
#include "tapas/ds/tstring.h"
#include "tapas/runtime/tarray.h"
#include "tapas/runtime/tlist.h"
#include "tapas/runtime/tpair.h"
#include "tapas/runtime/tstr.h"
#include "tapas/runtime/ttime.h"

#include <assert.h>
#include <math.h>
#include <string.h>

static tobj integer(long value)
{
	tobj out;
	tobj_set_nil(&out);
	tobj_set_int(&out, value);
	return out;
}

static void test_numeric_array(void)
{
	tlist *values = tlist_new();
	for (long i = 1; i <= 6; i++) {
		tobj v = integer(i);
		tlist_push(values, &v);
	}
	tdarr *a = tdarr_from_list(2, 3, values);
	assert(tarr_rows((tcompo_v *)a) == 2);
	assert(tarr_cols((tcompo_v *)a) == 3);
	assert(tdarr_at(a, 1, 2) == 6.0);

	tobj indices[2] = { integer(1), integer(0) };
	tobj result;
	tobj_set_nil(&result);
	tarr_idx((tcompo_v *)a, indices, 2, &result);
	assert(result.type == tfloat && result.val.v_tfloat == 4.0);
	tobj first = integer(0), last = integer(2), col = integer(1);
	tpair *row_range = tpair_new(&first, &last);
	tobj slice_indices[2];
	tobj_set_nil(&slice_indices[0]);
	tobj_set_compo(&slice_indices[0], (tcompo_v *)row_range);
	slice_indices[1] = col;
	tobj_set_nil(&result);
	tarr_idx((tcompo_v *)a, slice_indices, 2, &result);
	assert(result.type == tcompo && tobj_compo_type(&result) == compo_tdarr);
	assert(((tdarr *)result.val.v_tcompo)->rows == 2);
	assert(((tdarr *)result.val.v_tcompo)->cols == 1);
	assert(tdarr_at((tdarr *)result.val.v_tcompo, 1, 0) == 5.0);
	tobj_try_clear(&result);
	row_range->base.vtable->free(row_range);

	tdarr *at = tdarr_transpose(a);
	assert(at->rows == 3 && at->cols == 2 && tdarr_at(at, 2, 1) == 6.0);
	tdarr *product = tdarr_matmul(a, at);
	assert(product->rows == 2 && product->cols == 2);
	assert(tdarr_at(product, 0, 0) == 14.0);
	assert(tdarr_at(product, 1, 1) == 77.0);

	tobj scalar = integer(2);
	tobj array_obj;
	tobj_set_nil(&array_obj);
	tobj_set_compo(&array_obj, (tcompo_v *)a);
	a->base.refctr++;
	tobj sum;
	tobj_set_nil(&sum);
	a->base.vtable->op_add(a, &scalar, 0, &sum);
	assert(sum.type == tcompo && tobj_compo_type(&sum) == compo_tdarr);
	assert(tdarr_at((tdarr *)sum.val.v_tcompo, 0, 0) == 3.0);

	tstring *rendered = a->base.vtable->tostring_full(a);
	assert(strstr(tstring_cstr(rendered), "[1, 2, 3]") != NULL);
	tstring_free(rendered);
	tobj_try_clear(&sum);
	tobj_ddc_ref_clear(&array_obj);
	product->base.vtable->free(product);
	at->base.vtable->free(at);
	values->base.vtable->free(values);
}

static void test_boolean_array_and_time(void)
{
	tbarr *flags = tbarr_new(2, 2, 0);
	tbarr_set(flags, 0, 1, 1);
	tbarr_set(flags, 1, 0, 1);
	assert(tbarr_at(flags, 0, 1));
	tbarr *transposed = tbarr_transpose(flags);
	assert(tbarr_at(transposed, 1, 0));
	flags->base.vtable->free(flags);
	transposed->base.vtable->free(transposed);

	ttime *before = ttime_from_time((time_t)100);
	ttime *after = ttime_from_time((time_t)145);
	tobj other, delta;
	tobj_set_nil(&other);
	tobj_set_nil(&delta);
	tobj_set_compo(&other, (tcompo_v *)before);
	after->base.vtable->op_sub(after, &other, 0, &delta);
	assert(delta.type == tfloat && fabs(delta.val.v_tfloat - 45.0) < 1e-9);
	assert(ttime_unix(before) == 100);
	ttime *shifted = ttime_shift(before, 30);
	assert(ttime_unix(shifted) == 130);
	tstring *formatted = ttime_format(before, "%Y");
	assert(tstring_len(formatted) > 0);
	tstring_free(formatted);
	shifted->base.vtable->free(shifted);
	before->base.vtable->free(before);
	after->base.vtable->free(after);
}

static void test_short_string_storage(void)
{
	tstring *text = tstring_new("short");
	assert(text->data == text->inline_data);
	tstring_append(text, " string that grows beyond inline storage");
	assert(text->data != text->inline_data);
	tstring_assign(text, "small");
	tstring_shrink_to_fit(text);
	assert(text->data == text->inline_data);
	assert(strcmp(tstring_cstr(text), "small") == 0);
	tstring_free(text);

	tstring local;
	assert(tstring_init(&local, "embedded"));
	assert(local.data == local.inline_data);
	tstring_append_c(&local, '!');
	assert(strcmp(tstring_cstr(&local), "embedded!") == 0);
	tstring_deinit(&local);

	tstr *runtime_string = tstr_new("value");
	assert(runtime_string->data == &runtime_string->storage);
	assert(runtime_string->data->data == runtime_string->data->inline_data);
	tstring_append(runtime_string->data, " remains mutable");
	assert(strcmp(tstring_cstr(runtime_string->data),
		      "value remains mutable") == 0);
	runtime_string->base.vtable->free(runtime_string);

	tstr *prefix = tstr_new_len("prefix", 3);
	assert(strcmp(tstring_cstr(prefix->data), "pre") == 0);
	assert(tstring_len(prefix->data) == 3);
	prefix->base.vtable->free(prefix);
}

static void test_object_vector_range_copy(void)
{
	tobj_vec source;
	tobj_vec slice;
	tobj_vec_init(&source);

	tobj first = integer(10);
	tobj shared;
	tobj last = integer(30);
	tobj_set_nil(&shared);
	tstr *text = tstr_new("shared");
	tobj_set_compo(&shared, (tcompo_v *)text);
	tobj_vec_push(&source, &first);
	tobj_vec_push(&source, &shared);
	tobj_vec_push(&source, &last);
	assert(text->base.refctr == 1);

	tobj_vec_copy_range(&slice, &source, 1, 2);
	assert(slice.len == 2 && slice.capacity == 2);
	assert(slice.data[0].type == tcompo &&
	       slice.data[0].val.v_tcompo == (tcompo_v *)text);
	assert(slice.data[1].type == tint && slice.data[1].val.v_tint == 30);
	assert(text->base.refctr == 2);

	tobj_vec_free(&slice);
	assert(text->base.refctr == 1);
	tobj_vec_free(&source);
}

int main(void)
{
	test_numeric_array();
	test_boolean_array_and_time();
	test_short_string_storage();
	test_object_vector_range_copy();
	return 0;
}
