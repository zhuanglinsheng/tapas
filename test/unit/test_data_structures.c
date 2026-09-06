#include "../../src/stdlib/function_metadata.h"
#include "../../src/stdlib/random/object.h"
#include "../../src/stdlib/rules/codec.h"
#include "../../src/stdlib/rules/hash.h"
#include "tapas/dsa/tstring.h"
#include "tapas/objects/tarray.h"
#include "tapas/objects/tcfn.h"
#include "stdlib/rules/domain.h"
#include "tapas/objects/tlist.h"
#include "tapas/objects/tpair.h"
#include "tapas/objects/trule.h"
#include "tapas/objects/trule_ir.h"
#include "tapas/objects/tstr.h"
#include "tapas/objects/ttime.h"
#include "tapas/objects/ttype.h"
#include "runtime/tenv.h"
#include "tapas/textension.h"
#include "tapas/tstdlib.h"
#include "tapas/tval.h"
#include <stdlib.h>

#include <assert.h>
#include <math.h>
#include <string.h>

static tobj integer(long value) {
  tobj out;
  tobj_set_nil(&out);
  tobj_set_int(&out, value);
  return out;
}

static void test_type_template_canonical_roundtrip(void) {
  ttypeval *type_parameter = ttypeval_new_parameter("T");
  ttypeval *value_parameter = ttypeval_new_value_parameter("N");
  tstring *type_name = tstring_new("T");
  tstring *value_name = tstring_new("N");
  const tstring *type_names[] = {type_name};
  ttype_value_parameter value_parameters[] = {
      {.name = value_name, .type = ttypeval_builtin(tbuiltintype_int)}};
  ttype_field fields[] = {
      {.name = type_name, .type = type_parameter},
      {.name = value_name, .type = value_parameter}};
  ttypeval *body = ttypeval_new_fields(fields, 2);
  ttypeval *type = ttypeval_new_template(type_names, 1, value_parameters, 1,
                                         body);
  ttypeval *decoded =
      ttypeval_from_canonical(tstring_cstr(type->canonical));
  assert(decoded && ttypeval_equal(type, decoded));

  tobj value = integer(3);
  ttypeval *argument = ttypeval_builtin(tbuiltintype_string);
  ttypeval *applied = ttypeval_apply_template(type, &argument, 1, &value, 1);
  assert(ttypeval_field_named(applied, "T") == argument);
  tobj wrong = integer(4);
  assert(ttypeval_matches(&value, ttypeval_field_named(applied, "N")));
  assert(!ttypeval_matches(&wrong, ttypeval_field_named(applied, "N")));

  ttypeval_release(applied);
  ttypeval_release(decoded);
  ttypeval_release(type);
  tstring_free(type_name);
  tstring_free(value_name);
}

static void test_extension_nominal_template(void) {
  static const textension_template_type_parameter type_parameters[] = {
      {.name = "T", .slot = "item"}};
  static const textension_template_value_parameter value_parameters[] = {
      {.name = "N", .slot = "size", .constraint = "Int"}};
  static const textension_nominal_template schema = {
      .identity = "sample::Buffer",
      .type_parameters = type_parameters,
      .type_parameter_count = 1,
      .value_parameters = value_parameters,
      .value_parameter_count = 1,
      .capabilities = textension_type_indexable};
  ttypeval *type = ttypeval_new_extension_template(&schema);
  assert(type && ttypeval_is_template(type));
  ttypeval *item = ttypeval_builtin(tbuiltintype_string);
  tobj size = integer(3);
  ttypeval *applied = ttypeval_apply_template(type, &item, 1, &size, 1);
  assert(applied && applied->kind == ttype_kind_named);
  assert(tstring_eq_cstr(applied->named_identity, "sample::Buffer"));
  assert(ttypeval_equal(ttypeval_parameter(applied, "item"), item));
  assert(ttypeval_matches(&size, ttypeval_parameter(applied, "size")));
  assert(applied->capabilities == textension_type_indexable);
  ttypeval *decoded = ttypeval_from_canonical(
      tstring_cstr(applied->canonical));
  assert(decoded && ttypeval_equal(decoded, applied));
  assert(decoded->capabilities == textension_type_indexable);
  ttypeval_release(decoded);
  ttypeval_release(applied);
  ttypeval_release(type);
}

static void native_identity(tobj *params, uint_regs len, tobj *result) {
  assert(len == 1);
  tobj_copy(result, params);
}

static void test_c_function_descriptor(void) {
  tcfn_descriptor descriptor = TCFN_DESCRIPTOR(
      "identity", native_identity, TCFN_RANGE("Function[...] -> Int", 1, 2));
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

static void test_extension_descriptor(void) {
  static const textension_symbol symbols[] = {TAPAS_NATIVE_FUNCTION(
      "identity", native_identity, 1, 1, "Function[AnyType] -> AnyType")};
  static const textension_module module =
      TAPAS_PACKAGE_MODULE("sample", symbols);
  static const textension_module *const modules[] = {&module};
  static const textension_descriptor extension =
      TAPAS_EXTENSION("sample", "1", modules);
  static const textension_module *const duplicates[] = {&module, &module};
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
  assert(strcmp(found.symbol->type, "Function[Int, Int, AnyType] -> Unknown") ==
         0);
  assert(!textension_find(stdlib, "dense", "new", &found));
  assert(textension_find(stdlib, "dense", "rows", &found));
  assert(strcmp(found.symbol->type, "Function[RealArray | BoolArray] -> Int") ==
         0);
  assert(textension_find(stdlib, "dense", "inner", &found));
  assert(strcmp(found.symbol->type,
                "Function[RealArray, RealArray] -> Float") == 0);
  assert(textension_find(stdlib, "dense", "normalize", &found));
  assert(strcmp(found.symbol->detail,
                "dense::normalize(value: RealArray) -> RealArray") == 0);
  assert(textension_find(stdlib, "dense", "gemm", &found));
  assert(
      strcmp(found.symbol->type, "Function[Int | Float, RealArray, RealArray, "
                                 "Int | Float, RealArray] -> Nil") == 0);
  assert(textension_find(stdlib, nullptr, "pop_front", &found));
  assert(strcmp(found.symbol->type, "Function[List] -> AnyType") == 0);
  assert(textension_find(stdlib, nullptr, "pop_back", &found));
  assert(strcmp(found.symbol->detail, "pop_back(list: List) -> AnyType") == 0);
  assert(textension_find(stdlib, "io", "replace_text", &found));
  assert(strcmp(found.symbol->type, "Function[String, String] -> Nil") == 0);
  assert(textension_find(stdlib, "syntax", "Token", &found));
  assert(found.symbol->kind == textension_type);
  assert(textension_find(stdlib, "syntax", "tokens", &found));
  assert(strcmp(found.symbol->type,
                "Function[String] -> List[syntax::Token]") == 0);
  assert(strcmp(found.symbol->detail,
                "syntax::tokens(source: String) -> List[syntax::Token]") == 0);
  assert(textension_find(stdlib, "random", "pcg32_xsh_rr", &found));
  assert(strcmp(found.symbol->type, "Source") == 0);
  const textension_symbol *source_symbol = found.symbol;
  assert(textension_find(stdlib, "random", "generator", &found));
  const textension_symbol *generator_symbol = found.symbol;
  assert(strcmp(generator_symbol->type, "Function[Source, Int] -> Generator") ==
         0);
  tobj arguments[2], generated;
  tobj_set_nil(&arguments[0]);
  tobj_set_int(&arguments[1], 42);
  tobj_set_nil(&generated);
  source_symbol->value_factory(&arguments[0]);
  generator_symbol->function(arguments, 2, &generated);
  assert(generated.val.v_tcompo->vtable == &trandom_generator_vtable);
  tobj_try_clear(&generated);
  tobj_try_clear(&arguments[0]);
  assert(textension_find(stdlib, "evaluators", "Evaluator", &found));
  tobj evaluator_type;
  tobj_set_nil(&evaluator_type);
  found.symbol->value_factory(&evaluator_type);
  assert(evaluator_type.type == tcompo &&
         tobj_compo_type(&evaluator_type) == compo_ttypeval);
  ttypeval *evaluator = (ttypeval *)evaluator_type.val.v_tcompo;
  assert(evaluator->kind == ttype_kind_named);
  assert(tstring_eq_cstr(evaluator->named_identity, "evaluators::Evaluator"));
  tobj_try_clear(&evaluator_type);
}

static void test_numeric_array(void) {
  tdarr *a = tdarr_new(2, 3, 0.0);
  for (size_t i = 0; i < 6; i++)
    a->data[i] = (double)(i + 1);
  assert(tarr_rows((tcompo_v *)a) == 2);
  assert(tarr_cols((tcompo_v *)a) == 3);
  assert(tdarr_at(a, 1, 2) == 6.0);

  tobj indices[2] = {integer(1), integer(0)};
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
      at->data[col_index * at->cols + row] = a->data[row * a->cols + col_index];
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

static void test_empty_and_identity_arrays(void) {
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
  assert(empty_bools->base.vtable->identical(empty_bools, empty_bools_copy));

  tdarr *positive_zero = tdarr_new(1, 1, 0.0);
  tdarr *negative_zero = tdarr_new(1, 1, -0.0);
  assert(positive_zero->base.vtable->identical(positive_zero, negative_zero));

  empty->base.vtable->free(empty);
  empty_copy->base.vtable->free(empty_copy);
  empty_bools->base.vtable->free(empty_bools);
  empty_bools_copy->base.vtable->free(empty_bools_copy);
  positive_zero->base.vtable->free(positive_zero);
  negative_zero->base.vtable->free(negative_zero);
}

static void test_boolean_array_and_time(void) {
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

static void test_random_generator(void) {
  static const uint32_t expected[] = {
      UINT32_C(0xa15c02b7), UINT32_C(0x7b47f409), UINT32_C(0xba1d3330),
      UINT32_C(0x83d2f293), UINT32_C(0xbfa4784b), UINT32_C(0xcbed606e)};
  trandom_source *source = trandom_source_pcg32_xsh_rr();
  trandom_generator *generator = trandom_generator_new(source, 42);
  ttypeval *source_type = tstdlib_random_source_type();
  tobj source_value = {.type = tcompo, .val.v_tcompo = (tcompo_v *)source};
  assert(ttypeval_matches(&source_value, source_type));
  ttypeval *decoded =
      ttypeval_from_canonical(tstring_cstr(source_type->canonical));
  assert(decoded && ttypeval_equal(source_type, decoded));
  ttypeval_release(decoded);
  ttypeval_release(source_type);
  for (size_t i = 0; i < sizeof(expected) / sizeof(expected[0]); i++)
    assert(trandom_generator_u32(generator) == expected[i]);

  tcompo_v *generator_base = (tcompo_v *)generator;
  trandom_generator *copy = generator_base->vtable->copy(generator);
  assert(generator_base->vtable->identical(generator, copy));
  for (size_t i = 0; i < 100; i++) {
    assert(trandom_generator_bounded(generator, 37) < 37);
    assert(trandom_generator_bounded(copy, 37) < 37);
  }
  assert(generator_base->vtable->identical(generator, copy));
  double unit = trandom_generator_unit(generator);
  assert(unit >= 0.0 && unit < 1.0);

  trandom_generator *advanced = trandom_generator_new(source, 42);
  trandom_generator *iterated = trandom_generator_new(source, 42);
  uint32_t expected_after_advance = 0;
  for (size_t i = 0; i <= 12; i++)
    expected_after_advance = trandom_generator_u32(iterated);
  trandom_generator_advance(advanced, 12);
  uint32_t advanced_value = trandom_generator_u32(advanced);
  assert(advanced_value == expected_after_advance);
  trandom_generator_advance(advanced, -1);
  assert(trandom_generator_u32(advanced) == advanced_value);

  textension_symbol_ref symbol;
  tobj params[2], result;
  tobj_set_nil(&params[0]);
  tobj_set_compo(&params[0], (tcompo_v *)advanced);
  params[1] = integer(7);
  tobj_set_nil(&result);
  assert(textension_find(tstdlib_descriptor(), "random", "next_int", &symbol));
  symbol.symbol->function(params, 2, &result);
  assert(result.type == tint && result.val.v_tint >= 0 &&
         result.val.v_tint < 7);
  assert(
      textension_find(tstdlib_descriptor(), "random", "next_float", &symbol));
  symbol.symbol->function(params, 1, &result);
  assert(result.type == tfloat && result.val.v_tfloat >= 0.0 &&
         result.val.v_tfloat < 1.0);
  assert(textension_find(tstdlib_descriptor(), "random", "next_bool", &symbol));
  symbol.symbol->function(params, 1, &result);
  assert(result.type == tbool);
  params[1] = integer(-3);
  assert(textension_find(tstdlib_descriptor(), "random", "advance", &symbol));
  symbol.symbol->function(params, 2, &result);
  assert(result.type == tnil);

  ((tcompo_v *)advanced)->vtable->free(advanced);
  ((tcompo_v *)iterated)->vtable->free(iterated);
  ((tcompo_v *)copy)->vtable->free(copy);
  generator_base->vtable->free(generator);
  ((tcompo_v *)source)->vtable->free(source);
}

static void test_short_string_storage(void) {
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
  assert(strcmp(tstring_cstr(runtime_string->data), "value remains mutable") ==
         0);
  runtime_string->base.vtable->free(runtime_string);

  tstr *prefix = tstr_new_len("prefix", 3);
  assert(strcmp(tstring_cstr(prefix->data), "pre") == 0);
  assert(tstring_len(prefix->data) == 3);
  prefix->base.vtable->free(prefix);
}

static void test_object_vector_range_copy(void) {
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

static void test_object_vector_take(void) {
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
  assert(result.type == tcompo && result.val.v_tcompo == (tcompo_v *)text);
  assert(text->base.refctr == 1);
  tobj_vec_free(&values);
  assert(text->base.refctr == 1);
  tobj_ddc_ref_clear(&result);
}

static void test_capability_dispatch(void) {
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
  assert(ttypeval_matches(&list_value, ttypeval_builtin(tbuiltintype_indexable)));
  assert(ttypeval_matches(&list_value, ttypeval_builtin(tbuiltintype_appendable)));
  assert(ttypeval_matches(&list_value, ttypeval_builtin(tbuiltintype_deletable)));
  assert(
      ttypeval_matches(&list_value, ttypeval_builtin(tbuiltintype_index_settable)));
  assert(ttypeval_matches(&list_value, ttypeval_builtin(tbuiltintype_contains)));
  assert(ttypeval_matches(&list_value, ttypeval_builtin(tbuiltintype_iterable)));
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

static ttypeval *metadata_string(void) {
  return ttypeval_retain(ttypeval_builtin(tbuiltintype_string));
}

static void test_function_metadata(void) {
  assert(tstdlib_validate_function_metadata());
  ttypeval *types[] = {ttypeval_builtin(tbuiltintype_int),
                       ttypeval_builtin(tbuiltintype_string)};
  ttypeval *signature =
      ttypeval_new_function(types, 2, ttypeval_builtin(tbuiltintype_bool), 0);
  tfunction_metadata *metadata =
      tfunction_metadata_decode("value\x1f"
                                "label",
                                tstring_cstr(signature->canonical));
  assert(metadata && tfunction_metadata_count(metadata) == 2);
  assert(!tfunction_metadata_display_name(metadata));
  assert(!strcmp(tfunction_metadata_name(metadata, 0), "value"));
  assert(ttypeval_equal(tfunction_metadata_type(metadata, 0),
                        ttypeval_builtin(tbuiltintype_int)));
  assert(ttypeval_equal(tfunction_metadata_return_type(metadata),
                        ttypeval_builtin(tbuiltintype_bool)));
  tfunction_metadata *shared = tfunction_metadata_retain(metadata);
  assert(shared == metadata);
  tfunction_metadata_release(metadata);
  assert(!strcmp(tfunction_metadata_name(shared, 1), "label"));
  tfunction_metadata_release(shared);
  metadata = tfunction_metadata_decode("named\x1e"
                                       "value\x1f"
                                       "label",
                                       tstring_cstr(signature->canonical));
  assert(metadata &&
         !strcmp(tfunction_metadata_display_name(metadata), "named"));
  assert(tfunction_metadata_count(metadata) == 2);
  assert(!strcmp(tfunction_metadata_name(metadata, 0), "value"));
  tfunction_metadata_release(metadata);
  assert(
      !tfunction_metadata_decode("value", tstring_cstr(signature->canonical)));
  assert(!tfunction_metadata_decode("", "Int"));
  assert(
      !tfunction_metadata_decode("\x1f", tstring_cstr(signature->canonical)));
  ttypeval_release(signature);
  const tparameter_spec optional[] = {{"prompt", metadata_string, 1, 0}};
  metadata =
      tfunction_metadata_new(optional, 1, ttypeval_builtin(tbuiltintype_string), 0);
  assert(metadata && tfunction_metadata_optional(metadata, 0));
  tfunc *function = tfunc_new(0, nullptr, 0, 0, 1, 0, 0);
  function->metadata = tfunction_metadata_retain(metadata);
  tfunc *copy = function->compo_base.vtable->copy(function);
  assert(copy->metadata == function->metadata);
  function->compo_base.vtable->free(function);
  assert(!strcmp(tfunction_metadata_name(copy->metadata, 0), "prompt"));
  copy->compo_base.vtable->free(copy);
  tfunction_metadata_release(metadata);
}

static void test_rule_logic_ir(void) {
  const char *valid = "TPIR5;0:0:5;3;2;1;1;16:B12:RuleInstances1:p0;0:0:0;-1;-"
                      "1;1;16:B12:RuleInstances1:q0;0:0:0;-1;-1;10;7:B4:Booln;"
                      "2;0;1;0:0:0;-1;-1;0;1;0;2;0;0:-1;-1;";
  trule_ir *ir = trule_ir_deserialize(valid);
  assert(ir && ir->version == 5);
  trule_item *item = (trule_item *)ir->items.data[0].val.v_tcompo;
  assert(item->term->kind == trule_term_or && item->term->arguments.len == 2);
  tstring *encoded = nullptr;
  assert(trule_ir_serialize(ir, &encoded));
  trule_ir *restored = trule_ir_deserialize(tstring_cstr(encoded));
  assert(restored &&
         trule_ir_semantic_hash(restored) == trule_ir_semantic_hash(ir));
  restored->base.vtable->free(restored);
  ir->base.vtable->free(ir);
  tstring_free(encoded);
  /* New operators cannot be smuggled into older versions; arity and
   * operand Type remain part of deserialization validation. */
  assert(
      !trule_ir_deserialize("TPIR4;0:0:4;2;1;1;1;7:B4:Bools1:p0;0:0:0;-1;-1;9;"
                            "7:B4:Booln;2;0;0;0:0:0;-1;-1;0;0;1;0;0:-1;-1;"));
  assert(
      !trule_ir_deserialize("TPIR5;0:0:5;2;1;1;1;7:B4:Bools1:p0;0:0:0;-1;-1;9;"
                            "7:B4:Booln;1;0;0:0:0;-1;-1;0;0;1;0;0:-1;-1;"));
  assert(
      !trule_ir_deserialize("TPIR5;0:0:5;2;1;1;1;6:B3:Ints1:p0;0:0:0;-1;-1;10;"
                            "7:B4:Booln;2;0;0;0:0:0;-1;-1;0;0;1;0;0:-1;-1;"));
}

static void test_rule_not_ir(void) {
  const char *valid = "TPIR4;0:0:4;2;1;1;1;16:B12:RuleInstances1:p0;0:0:0;-1;-"
                      "1;8;7:B4:Booln;1;0;0:0:0;-1;-1;0;0;1;0;0:-1;-1;";
  trule_ir *ir = trule_ir_deserialize(valid);
  assert(ir && ir->version == 4);
  trule_item *item = (trule_item *)ir->items.data[0].val.v_tcompo;
  assert(item->term->kind == trule_term_not && item->term->arguments.len == 1);
  tstring *encoded = nullptr;
  assert(trule_ir_serialize(ir, &encoded));
  trule_ir *restored = trule_ir_deserialize(tstring_cstr(encoded));
  assert(restored &&
         trule_ir_semantic_hash(restored) == trule_ir_semantic_hash(ir));
  restored->base.vtable->free(restored);
  tstring_free(encoded);
  ir->base.vtable->free(ir);
  /* Reject a new node under an old version, a non-Bool result, a missing
   * operand, and a statically incompatible operand. */
  assert(!trule_ir_deserialize(
      "TPIR3;0:0:3;2;1;1;1;16:B12:RuleInstances1:p0;0:0:0;-1;-1;8;7:B4:Booln;1;"
      "0;0:0:0;-1;-1;0;0;1;0;0:-1;-1;"));
  assert(!trule_ir_deserialize(
      "TPIR4;0:0:4;2;1;1;1;16:B12:RuleInstances1:p0;0:0:0;-1;-1;8;6:B3:Intn;1;"
      "0;0:0:0;-1;-1;0;0;1;0;0:-1;-1;"));
  assert(!trule_ir_deserialize(
      "TPIR4;0:0:4;2;1;1;1;16:B12:RuleInstances1:p0;0:0:0;-1;-1;8;7:B4:Booln;0;"
      "0:0:0;-1;-1;0;0;1;0;0:-1;-1;"));
  assert(
      !trule_ir_deserialize("TPIR4;0:0:4;2;1;1;1;6:B3:Ints1:p0;0:0:0;-1;-1;8;7:"
                            "B4:Booln;1;0;0:0:0;-1;-1;0;0;1;0;0:-1;-1;"));
}

static void test_rule_representations(void) {
  trule *r = trule_new(nullptr, "rule(x: Int) { x > 0 }", "", "", "", "");
  tobj rule = {.type = tcompo, .val.v_tcompo = (tcompo_v *)r};
  tstring *abbr = tobj_tostring_abbr(&rule), *full = tobj_tostring_full(&rule);
  char expected[64];
  snprintf(expected, sizeof(expected), "<%p>", (void *)r);
  assert(!strcmp(tstring_cstr(abbr), expected));
  snprintf(expected, sizeof(expected), "Rule #%llu[]",
           (unsigned long long)r->identity);
  assert(!strcmp(tstring_cstr(full), expected));
  tstring_free(abbr);
  tstring_free(full);
  r->base.vtable->free(r);

  trule_term *flag =
      trule_term_parameter_new("flag", ttypeval_builtin(tbuiltintype_bool));
  tobj parameter = {.type = tcompo, .val.v_tcompo = (tcompo_v *)flag};
  abbr = tobj_tostring_abbr(&parameter);
  snprintf(expected, sizeof(expected), "<%p>", (void *)flag);
  assert(!strcmp(tstring_cstr(abbr), expected));
  tstring_free(abbr);
  trule_term *both = trule_term_logic_new(trule_term_and, flag, flag);
  tobj dag = {.type = tcompo, .val.v_tcompo = (tcompo_v *)both};
  abbr = tobj_tostring_abbr(&dag);
  snprintf(expected, sizeof(expected), "<%p>", (void *)both);
  assert(!strcmp(tstring_cstr(abbr), expected));
  tstring_free(abbr);
  full = tobj_tostring_full(&dag);
  assert(strstr(tstring_cstr(full), "(flag and flag)"));
  assert(!strstr(tstring_cstr(full), "<cycle>"));
  tstring_free(full);
  both->base.vtable->free(both);

  trule_term *cycle = trule_term_new(
      trule_term_not, ttypeval_builtin(tbuiltintype_bool), nullptr, nullptr, 0);
  cycle->base.refctr =
      1; /* Keep an external reference while breaking the cycle. */
  tobj self = {.type = tcompo, .val.v_tcompo = (tcompo_v *)cycle};
  tobj_vec_push(&cycle->arguments, &self);
  full = tobj_tostring_full(&self);
  assert(strstr(tstring_cstr(full), "<cycle>"));
  tstring_free(full);
  tobj_vec_free(&cycle->arguments);
  cycle->base.refctr = 0;
  cycle->base.vtable->free(cycle);

  /* Arbitrary point members may lead back through a container to the domain. */
  tlist *list = tlist_new();
  list->base.refctr = 1;
  tobj member = {.type = tcompo, .val.v_tcompo = (tcompo_v *)list};
  trules_domain *points = trules_points_new(
      ttypeval_builtin(tbuiltintype_any), &member, 1);
  tobj domain = {.type = tcompo, .val.v_tcompo = (tcompo_v *)points};
  tobj_vec_push(&list->items, &domain);
  full = tobj_tostring_full(&domain);
  assert(strstr(tstring_cstr(full), "<cycle>"));
  tstring_free(full);
  tobj_vec_free(&list->items);
  list->base.refctr = 0;
  list->base.vtable->free(list);

  tobj label = {.type = tcompo,
                .val.v_tcompo = (tcompo_v *)tstr_new("a\"b\nc")};
  points = trules_points_new(
      ttypeval_builtin(tbuiltintype_string), &label, 1);
  domain.val.v_tcompo = (tcompo_v *)points;
  full = tobj_tostring_full(&domain);
  assert(!strcmp(tstring_cstr(full), "PointsOf[String][\"a\\\"b\\nc\"]"));
  tstring_free(full);
  points->base.vtable->free(points);

  char *large = malloc(20001);
  memset(large, 'x', 20000);
  large[20000] = 0;
  r = trule_new(nullptr, "", "", "", "", "");
  tstring_free(r->ir->display_name);
  r->ir->display_name = tstring_new(large);
  free(large);
  rule.val.v_tcompo = (tcompo_v *)r;
  full = tobj_tostring_full(&rule);
  assert(tstring_len(full) <= 16400 &&
         strstr(tstring_cstr(full), "<truncated>"));
  tstring_free(full);
  r->base.vtable->free(r);
}

int main(void) {
  test_type_template_canonical_roundtrip();
  test_extension_nominal_template();
  test_rule_representations();
  test_rule_not_ir();
  test_rule_logic_ir();
  test_function_metadata();
  test_c_function_descriptor();
  test_extension_descriptor();
  test_numeric_array();
  test_empty_and_identity_arrays();
  test_boolean_array_and_time();
  test_random_generator();
  test_short_string_storage();
  test_object_vector_range_copy();
  test_object_vector_take();
  test_capability_dispatch();
  return 0;
}
