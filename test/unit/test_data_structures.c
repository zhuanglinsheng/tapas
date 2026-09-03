#include "tapas/tval.h"
#include "tapas/tenv.h"
#include "tapas/textension.h"
#include "tapas/tstdlib.h"
#include "tapas/ds/tstring.h"
#include "tapas/runtime/tarray.h"
#include "tapas/runtime/tcfn.h"
#include "tapas/runtime/tlist.h"
#include "tapas/runtime/tpair.h"
#include "tapas/runtime/tstr.h"
#include "tapas/runtime/ttime.h"
#include "tapas/runtime/ttype.h"

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

static void native_identity(tobj *params, uint_regs len, tobj *result)
{
	assert(len == 1);
	tobj_copy(result, params);
}

static void test_c_function_descriptor(void)
{
	tcfn_descriptor descriptor = TCFN_DESCRIPTOR(
		"identity", native_identity,
		TCFN_RANGE("Function[...] -> Int", 1, 2));
	assert(tcfn_descriptor_valid(&descriptor));
	tcppgenf *function = tcppgenf_new_descriptor(&descriptor);
	assert(tcppgenf_get_f(function) == native_identity);
	assert(strcmp(tcppgenf_get_signature_type(function),
		      "Function[...] -> Int") == 0);
	assert(tcppgenf_get_minimum_parameters(function) == 1);
	assert(tcppgenf_get_maximum_parameters(function) == 2);
	assert(tcppgenf_accepts(function, 1));
	assert(tcppgenf_accepts(function, 2));
	assert(!tcppgenf_accepts(function, 0));
	assert(!tcppgenf_accepts(function, 3));
	function->base.vtable->free(function);
}

static void test_extension_descriptor(void)
{
	static const textension_symbol symbols[] = {
		TAPAS_NATIVE_FUNCTION("identity", native_identity, 1, 1,
			"Function[AnyType] -> AnyType")
	};
	static const textension_module module =
		TAPAS_PACKAGE_MODULE("sample", symbols);
	static const textension_module *const modules[] = { &module };
	static const textension_descriptor extension =
		TAPAS_EXTENSION("sample", "1", modules);
	static const textension_module *const duplicates[] = {
		&module, &module
	};
	static const textension_descriptor invalid =
		TAPAS_EXTENSION("invalid", "1", duplicates);

	assert(textension_validate(&extension));
	assert(!textension_validate(&invalid));
	assert(textension_symbol_count(&extension) == 1);
	textension_symbol_ref found;
	assert(textension_find(&extension, "sample", "identity", &found));
	assert(found.module == &module && found.symbol == &symbols[0]);

	tlib *library = tlib_new();
	assert(tlib_install_extension(library, &extension));
	const tobj *package = tlib_find(library, "sample");
	assert(package && package->type == tcompo &&
	       tobj_compo_type(package) == compo_tdict);
	library->compo_base.vtable->free(library);

	const textension_descriptor *stdlib = tstdlib_descriptor();
	assert(textension_find(stdlib, nullptr, "print", &found));
	assert(found.module->scope == textension_root);
	assert(!textension_find(stdlib, "io", "print", &found));
	assert(textension_find(stdlib, nullptr, "array", &found));
	assert(strcmp(found.symbol->type,
		      "Function[Int, Int, AnyType] -> Unknown") == 0);
	assert(!textension_find(stdlib, "dense", "new", &found));
	assert(textension_find(stdlib, "dense", "rows", &found));
	assert(strcmp(found.symbol->type,
		      "Function[RealArray | BoolArray] -> Int") == 0);
	assert(textension_find(stdlib, "dense", "inner", &found));
	assert(strcmp(found.symbol->type,
		      "Function[RealArray, RealArray] -> Float") == 0);
	assert(textension_find(stdlib, "dense", "normalize", &found));
	assert(strcmp(found.symbol->detail,
		      "dense::normalize(value: RealArray) -> RealArray") == 0);
	assert(textension_find(stdlib, "dense", "gemm", &found));
	assert(strcmp(found.symbol->type,
		      "Function[Int | Float, RealArray, RealArray, Int | Float, RealArray] -> Nil") == 0);
	assert(textension_find(stdlib, nullptr, "pop_front", &found));
	assert(strcmp(found.symbol->type,
		      "Function[List] -> AnyType") == 0);
	assert(textension_find(stdlib, nullptr, "pop_back", &found));
	assert(strcmp(found.symbol->detail,
		      "pop_back(list: List) -> AnyType") == 0);
	assert(textension_find(stdlib, "io", "replace_text", &found));
	assert(strcmp(found.symbol->type,
		      "Function[String, String] -> Nil") == 0);
	assert(textension_find(stdlib, "syntax", "Token", &found));
	assert(found.symbol->kind == textension_type);
	assert(textension_find(stdlib, "syntax", "tokens", &found));
	assert(strcmp(found.symbol->detail,
		      "syntax::tokens(source: String) -> List[syntax::Token]") == 0);
}

static void test_numeric_array(void)
{
	tdarr *a = tdarr_new(2, 3, 0.0);
	for (size_t i = 0; i < 6; i++)
		a->data[i] = (double)(i + 1);
	assert(tarr_rows((tcompo_v *)a) == 2);
	assert(tarr_cols((tcompo_v *)a) == 3);
	assert(tdarr_at(a, 1, 2) == 6.0);

	tobj indices[2] = { integer(1), integer(0) };
	tobj result;
	tobj_set_nil(&result);
	tcompo_index((tcompo_v *)a, indices, 2, &result);
	assert(result.type == tfloat && result.val.v_tfloat == 4.0);
	tobj first = integer(0), last = integer(2), col = integer(1);
	tpair *row_range = tpair_new(&first, &last);
	tobj slice_indices[2];
	tobj_set_nil(&slice_indices[0]);
	tobj_set_compo(&slice_indices[0], (tcompo_v *)row_range);
	slice_indices[1] = col;
	tobj_set_nil(&result);
	tcompo_index((tcompo_v *)a, slice_indices, 2, &result);
	assert(result.type == tcompo && tobj_compo_type(&result) == compo_tdarr);
	assert(((tdarr *)result.val.v_tcompo)->rows == 2);
	assert(((tdarr *)result.val.v_tcompo)->cols == 1);
	assert(tdarr_at((tdarr *)result.val.v_tcompo, 1, 0) == 5.0);
	tobj_try_clear(&result);
	row_range->base.vtable->free(row_range);

	tdarr *at = tdarr_new(3, 2, 0.0);
	for (size_t row = 0; row < a->rows; row++)
		for (size_t col_index = 0; col_index < a->cols; col_index++)
			at->data[col_index * at->cols + row] =
				a->data[row * a->cols + col_index];
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
	assert(strstr(tstring_cstr(rendered), "[1, 2, 3]") != nullptr);
	tstring_free(rendered);
	tobj_try_clear(&sum);
	tobj_ddc_ref_clear(&array_obj);
	product->base.vtable->free(product);
	at->base.vtable->free(at);
}

static void test_empty_and_identity_arrays(void)
{
	tdarr *empty = tdarr_new(0, 3, 0.0);
	tdarr *empty_copy = empty->base.vtable->copy(empty);
	assert(empty_copy->base.vtable->identical(empty, empty_copy));

	tobj negated;
	tobj_set_nil(&negated);
	empty->base.vtable->op_neg(empty, &negated);
	assert(negated.type == tcompo);
	assert(tobj_compo_type(&negated) == compo_tdarr);
	assert(tarr_rows(negated.val.v_tcompo) == 0);
	assert(tarr_cols(negated.val.v_tcompo) == 3);
	tobj_try_clear(&negated);

	tbarr *empty_bools = tbarr_new(2, 0, 0);
	tbarr *empty_bools_copy = empty_bools->base.vtable->copy(empty_bools);
	assert(empty_bools->base.vtable->identical(empty_bools,
						     empty_bools_copy));

	tdarr *positive_zero = tdarr_new(1, 1, 0.0);
	tdarr *negative_zero = tdarr_new(1, 1, -0.0);
	assert(positive_zero->base.vtable->identical(positive_zero,
						       negative_zero));

	empty->base.vtable->free(empty);
	empty_copy->base.vtable->free(empty_copy);
	empty_bools->base.vtable->free(empty_bools);
	empty_bools_copy->base.vtable->free(empty_bools_copy);
	positive_zero->base.vtable->free(positive_zero);
	negative_zero->base.vtable->free(negative_zero);
}

static void test_boolean_array_and_time(void)
{
	tbarr *flags = tbarr_new(2, 2, 0);
	tbarr_set(flags, 0, 1, 1);
	tbarr_set(flags, 1, 0, 1);
	assert(tbarr_at(flags, 0, 1));
	flags->base.vtable->free(flags);

	ttime *before = ttime_from_time((time_t)100);
	ttime *after = ttime_from_time((time_t)145);
	tobj other, delta;
	tobj_set_nil(&other);
	tobj_set_nil(&delta);
	tobj_set_compo(&other, (tcompo_v *)before);
	after->base.vtable->op_sub(after, &other, 0, &delta);
	assert(delta.type == tfloat && fabs(delta.val.v_tfloat - 45.0) < 1e-9);
	assert(ttime_get(before) == (time_t)100);
	tobj offset, shifted_value;
	tobj_set_nil(&offset);
	tobj_set_nil(&shifted_value);
	tobj_set_int(&offset, 30);
	before->base.vtable->op_add(before, &offset, 0, &shifted_value);
	ttime *shifted = (ttime *)shifted_value.val.v_tcompo;
	assert(ttime_get(shifted) == (time_t)130);
	tstring *formatted = before->base.vtable->tostring_full(before);
	assert(tstring_len(formatted) > 0);
	tstring_free(formatted);
	tobj_try_clear(&shifted_value);
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

static void test_object_vector_take(void)
{
	tobj_vec values;
	tobj_vec_init(&values);
	tobj source;
	tobj result;
	tobj_set_nil(&source);
	tobj_set_nil(&result);
	tstr *text = tstr_new("moved");
	tobj_set_compo(&source, (tcompo_v *)text);
	tobj_vec_push(&values, &source);
	assert(text->base.refctr == 1);

	tobj_vec_take(&values, 0, &result);
	assert(values.len == 0);
	assert(result.type == tcompo &&
	       result.val.v_tcompo == (tcompo_v *)text);
	assert(text->base.refctr == 1);
	tobj_vec_free(&values);
	assert(text->base.refctr == 1);
	tobj_ddc_ref_clear(&result);
}

static void test_capability_dispatch(void)
{
	tlist *list = tlist_new();
	tobj first = integer(7);
	tcompo_append((tcompo_v *)list, &first);

	tobj index = integer(0);
	tobj result;
	tobj_set_nil(&result);
	tcompo_index((tcompo_v *)list, &index, 1, &result);
	assert(result.type == tint && result.val.v_tint == 7);

	tobj list_value;
	tobj_set_nil(&list_value);
	tobj_set_compo(&list_value, (tcompo_v *)list);
	assert(ttypeval_matches(&list_value,
		ttypeval_builtin(tbuiltin_indexable)));
	assert(ttypeval_matches(&list_value,
		ttypeval_builtin(tbuiltin_appendable)));
	assert(ttypeval_matches(&list_value,
		ttypeval_builtin(tbuiltin_deletable)));
	assert(ttypeval_matches(&list_value,
		ttypeval_builtin(tbuiltin_index_settable)));
	assert(ttypeval_matches(&list_value,
		ttypeval_builtin(tbuiltin_contains)));
	assert(ttypeval_matches(&list_value,
		ttypeval_builtin(tbuiltin_iterable)));
	assert(tcompo_contains((tcompo_v *)list, &first));
	long position = 0;
	tobj item;
	tobj_set_nil(&item);
	assert(tcompo_next((tcompo_v *)list, &position, &item));
	assert(item.type == tint && item.val.v_tint == 7);
	assert(!tcompo_next((tcompo_v *)list, &position, &item));
	tobj replacement = integer(9);
	tcompo_index_set((tcompo_v *)list, &index, 1, &replacement);
	tcompo_index((tcompo_v *)list, &index, 1, &result);
	assert(result.type == tint && result.val.v_tint == 9);

	tcompo_delete((tcompo_v *)list, &index);
	assert(tlist_size(list) == 0);
	tobj_try_clear(&list_value);
}

int main(void)
{
	test_c_function_descriptor();
	test_extension_descriptor();
	test_numeric_array();
	test_empty_and_identity_arrays();
	test_boolean_array_and_time();
	test_short_string_storage();
	test_object_vector_range_copy();
	test_object_vector_take();
	test_capability_dispatch();
	return 0;
}
