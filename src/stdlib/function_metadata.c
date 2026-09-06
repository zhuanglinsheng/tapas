#include "solve/solve.h"
#include "tapas/dsa/tstring.h"
#include "function_metadata.h"
#include "modules.h"
#include "evaluators/object.h"
#include "finite/object.h"
#include "random/object.h"
#include "tapas/objects/tcfn.h"
#include "tapas/objects/trule.h"
#include "tapas/tstdlib.h"

#include <string.h>
#include <stdio.h>

/* ABI-neutral declarations: real Type factories, never parsed display text. */
#define BUILTIN_TYPE(factory_, name_) \
    static ttypeval *factory_(void) { return ttypeval_retain(ttypeval_builtin_named(name_)); }

static ttypeval *native_type_int(void);
static ttypeval *native_type_anytype(void);
static ttypeval *native_type_indexable(void);
static ttypeval *native_type_appendable(void);
static ttypeval *native_type_deletable(void);
static ttypeval *native_type_string(void);
static ttypeval *native_type_dictionary_type(void);
static ttypeval *native_type_dictionary(void);
static ttypeval *native_type_list(void);
static ttypeval *native_type_function_rule_ruleinstance_ruleir(void);
static ttypeval *native_type_ruleinstance(void);
static ttypeval *native_type_ruleinstance_rule(void);
static ttypeval *native_type_library(void);
static ttypeval *native_type_realarray_boolarray(void);
static ttypeval *native_type_realarray(void);
static ttypeval *native_type_int_float(void);
static ttypeval *native_type_function(void);
static ttypeval *native_type_evaluator(void);
static ttypeval *native_type_rule(void);
static ttypeval *native_type_hold_result(void) { return ttypeval_retain(tsolve_result_type()); }
static ttypeval *native_type_sample_result(void)
{
	return ttypeval_retain(tsolve_sample_result_type());
}
static ttypeval *native_type_context(void);
static ttypeval *native_type_parameter_int(void);
static ttypeval *native_type_capture_int(void);
static ttypeval *native_type_term(void);
static ttypeval *native_type_requirement(void);
static ttypeval *native_type_union_int_float(void);
static ttypeval *native_type_type(void);
static ttypeval *native_type_ruleir(void);
static ttypeval *native_type_ruleir_rule(void);
static ttypeval *native_type_list_term(void);
static ttypeval *native_type_rule_term(void);
static ttypeval *native_type_list_parameter(void);
static ttypeval *native_type_list_item(void);
static ttypeval *native_type_time(void);
static ttypeval *native_type_pair_string_type(void);
static ttypeval *native_type_nil(void);
static ttypeval *native_type_string_nil(void);
static ttypeval *native_type_float(void);
static ttypeval *native_type_bool(void);
static ttypeval *native_type_iterator(void);
static ttypeval *native_type_pair(void);
static ttypeval *native_type_list_pair_string_type(void);
static ttypeval *native_type_list_anytype(void);
static ttypeval *native_type_list_string(void);
static ttypeval *native_type_result(void);
static ttypeval *native_type_checkresult(void);
static ttypeval *native_type_origin(void);
static ttypeval *native_type_parameter(void);
static ttypeval *native_type_condition(void);
static ttypeval *native_type_item(void);
static ttypeval *native_type_item_selector(void);
static ttypeval *native_type_boolarray(void);
static ttypeval *native_type_capture(void);
static ttypeval *native_type_random_source(void);
static ttypeval *native_type_random_generator(void);
static ttypeval *native_type_finite_index(void);
static ttypeval *native_type_finite_distribution(void);
static ttypeval *native_type_list_finite_index(void);
static ttypeval *native_type_dictionary_string_indexable(void);
static ttypeval *native_type_dictionary_string_distribution(void);
static ttypeval *native_type_float_nil(void);

static ttypeval *native_domain_type(const char *identity, ttypeval *item)
{
	tstring *name = tstring_new("item");
	ttype_field parameter = {
		.name = name,
		.type = item
	};
	ttypeval *result = ttypeval_new_named_application(identity,
		&parameter, 1,
		textension_type_indexable | textension_type_contains);
	tstring_free(name);
	return ttypeval_retain(result);
}

static ttypeval *native_type_points(void)
{
	return native_domain_type("rules::PointsOf",
		ttypeval_builtin(tbuiltintype_any));
}
static ttypeval *native_type_random_source(void)
{
	return ttypeval_retain(tstdlib_random_source_type());
}

static ttypeval *native_type_random_generator(void)
{
	return ttypeval_retain(tstdlib_random_generator_type());
}

static ttypeval *native_type_finite_index(void)
{
	return ttypeval_retain(tstdlib_finite_index_type());
}

static ttypeval *native_type_finite_distribution(void)
{
	return ttypeval_retain(tstdlib_finite_distribution_type());
}

static ttypeval *native_type_list_finite_index(void)
{
	ttypeval *item = native_type_finite_index();
	ttypeval *result = ttypeval_new_list(item);
	ttypeval_release(item);
	return ttypeval_retain(result);
}

static ttypeval *native_dictionary(ttypeval *(*value_factory)(void))
{
	ttypeval *key = native_type_string();
	ttypeval *value = value_factory();
	ttypeval *result = ttypeval_new_dictionary(key, value);
	ttypeval_release(key);
	ttypeval_release(value);
	return ttypeval_retain(result);
}

static ttypeval *native_type_dictionary_string_indexable(void)
{
	return native_dictionary(native_type_indexable);
}

static ttypeval *native_type_dictionary_string_distribution(void)
{
	return native_dictionary(native_type_finite_distribution);
}

static ttypeval *native_type_float_nil(void)
{
	ttypeval *number = native_type_float();
	ttypeval *nil = native_type_nil();
	ttypeval *members[] = { number, nil };
	ttypeval *result = ttypeval_new_union(members, 2);
	ttypeval_release(number);
	ttypeval_release(nil);
	return ttypeval_retain(result);
}
static ttypeval *native_type_range_int(void)
{
	return native_domain_type("rules::RangeOf",
		ttypeval_builtin(tbuiltintype_int));
}
BUILTIN_TYPE(native_type_int, "Int")

BUILTIN_TYPE(native_type_anytype, "AnyType")

BUILTIN_TYPE(native_type_indexable, "Indexable")

BUILTIN_TYPE(native_type_appendable, "Appendable")

BUILTIN_TYPE(native_type_deletable, "Deletable")

BUILTIN_TYPE(native_type_string, "String")

static ttypeval *native_type_dictionary_type(void)
{
    ttypeval *a0 = native_type_dictionary();
    ttypeval *a1 = native_type_type();
    ttypeval *members[] = {a0, a1};
    ttypeval *result = ttypeval_new_union(members, 2);
    ttypeval_release(a0);
    ttypeval_release(a1);
    return ttypeval_retain(result);
}

BUILTIN_TYPE(native_type_dictionary, "Dictionary")

BUILTIN_TYPE(native_type_list, "List")

static ttypeval *native_type_function_rule_ruleinstance_ruleir(void)
{
    ttypeval *a0 = native_type_function();
    ttypeval *a1 = native_type_rule();
    ttypeval *a2 = native_type_ruleinstance();
    ttypeval *a3 = native_type_ruleir();
    ttypeval *members[] = {a0, a1, a2, a3};
    ttypeval *result = ttypeval_new_union(members, 4);
    ttypeval_release(a0);
    ttypeval_release(a1);
    ttypeval_release(a2);
    ttypeval_release(a3);
    return ttypeval_retain(result);
}

BUILTIN_TYPE(native_type_ruleinstance, "RuleInstance")

static ttypeval *native_type_ruleinstance_rule(void)
{
    ttypeval *a0 = native_type_ruleinstance();
    ttypeval *a1 = native_type_rule();
    ttypeval *members[] = {a0, a1};
    ttypeval *result = ttypeval_new_union(members, 2);
    ttypeval_release(a0);
    ttypeval_release(a1);
    return ttypeval_retain(result);
}

BUILTIN_TYPE(native_type_library, "Library")

static ttypeval *native_type_realarray_boolarray(void)
{
    ttypeval *a0 = native_type_realarray();
    ttypeval *a1 = native_type_boolarray();
    ttypeval *members[] = {a0, a1};
    ttypeval *result = ttypeval_new_union(members, 2);
    ttypeval_release(a0);
    ttypeval_release(a1);
    return ttypeval_retain(result);
}

BUILTIN_TYPE(native_type_realarray, "RealArray")

static ttypeval *native_type_int_float(void)
{
    ttypeval *a0 = native_type_int();
    ttypeval *a1 = native_type_float();
    ttypeval *members[] = {a0, a1};
    ttypeval *result = ttypeval_new_union(members, 2);
    ttypeval_release(a0);
    ttypeval_release(a1);
    return ttypeval_retain(result);
}

BUILTIN_TYPE(native_type_function, "Function")

static ttypeval *native_type_evaluator(void)
{
	return ttypeval_retain(tstdlib_evaluator_type());
}

BUILTIN_TYPE(native_type_rule, "Rule")

BUILTIN_TYPE(native_type_context, "Dictionary")

static ttypeval *native_type_parameter_int(void)
{
    ttypeval *a0 = native_type_parameter();
    ttypeval *a1 = native_type_int();
    ttypeval *members[] = {a0, a1};
    ttypeval *result = ttypeval_new_union(members, 2);
    ttypeval_release(a0);
    ttypeval_release(a1);
    return ttypeval_retain(result);
}

static ttypeval *native_type_capture_int(void)
{
    ttypeval *a0 = native_type_capture();
    ttypeval *a1 = native_type_int();
    ttypeval *members[] = {a0, a1};
    ttypeval *result = ttypeval_new_union(members, 2);
    ttypeval_release(a0);
    ttypeval_release(a1);
    return ttypeval_retain(result);
}

BUILTIN_TYPE(native_type_term, "RuleTerm")

BUILTIN_TYPE(native_type_requirement, "Requirement")

static ttypeval *native_type_union_int_float(void)
{
    ttypeval *a0 = native_type_int();
    ttypeval *a1 = native_type_float();
    ttypeval *members[] = {a0, a1};
    ttypeval *result = ttypeval_new_union(members, 2);
    ttypeval_release(a0);
    ttypeval_release(a1);
    return ttypeval_retain(result);
}

BUILTIN_TYPE(native_type_type, "Type")

BUILTIN_TYPE(native_type_ruleir, "RuleIR")

static ttypeval *native_type_ruleir_rule(void)
{
    ttypeval *a0 = native_type_ruleir();
    ttypeval *a1 = native_type_rule();
    ttypeval *members[] = {a0, a1};
    ttypeval *result = ttypeval_new_union(members, 2);
    ttypeval_release(a0);
    ttypeval_release(a1);
    return ttypeval_retain(result);
}

static ttypeval *native_type_list_term(void)
{
    ttypeval *a0 = native_type_term();
    ttypeval *result = ttypeval_new_list(a0);
    ttypeval_release(a0);
    return ttypeval_retain(result);
}

static ttypeval *native_type_rule_term(void)
{
    ttypeval *a0 = native_type_rule();
    ttypeval *a1 = native_type_term();
    ttypeval *members[] = {a0, a1};
    ttypeval *result = ttypeval_new_union(members, 2);
    ttypeval_release(a0);
    ttypeval_release(a1);
    return ttypeval_retain(result);
}

static ttypeval *native_type_list_parameter(void)
{
    ttypeval *a0 = native_type_parameter();
    ttypeval *result = ttypeval_new_list(a0);
    ttypeval_release(a0);
    return ttypeval_retain(result);
}

static ttypeval *native_type_list_item(void)
{
    ttypeval *a0 = native_type_item();
    ttypeval *result = ttypeval_new_list(a0);
    ttypeval_release(a0);
    return ttypeval_retain(result);
}

BUILTIN_TYPE(native_type_time, "Time")

static ttypeval *native_type_pair_string_type(void)
{
    ttypeval *a0 = native_type_string();
    ttypeval *a1 = native_type_type();
    ttypeval *result = ttypeval_new_pair(a0, a1);
    ttypeval_release(a0);
    ttypeval_release(a1);
    return ttypeval_retain(result);
}

BUILTIN_TYPE(native_type_nil, "Nil")

static ttypeval *native_type_string_nil(void)
{
    ttypeval *a0 = native_type_string();
    ttypeval *a1 = native_type_nil();
    ttypeval *members[] = {a0, a1};
    ttypeval *result = ttypeval_new_union(members, 2);
    ttypeval_release(a0);
    ttypeval_release(a1);
    return ttypeval_retain(result);
}

BUILTIN_TYPE(native_type_float, "Float")

BUILTIN_TYPE(native_type_bool, "Bool")

BUILTIN_TYPE(native_type_iterator, "Iterator")

BUILTIN_TYPE(native_type_pair, "Pair")

static ttypeval *native_type_list_pair_string_type(void)
{
    ttypeval *a0 = native_type_pair_string_type();
    ttypeval *result = ttypeval_new_list(a0);
    ttypeval_release(a0);
    return ttypeval_retain(result);
}

static ttypeval *native_type_list_anytype(void)
{
    ttypeval *a0 = native_type_anytype();
    ttypeval *result = ttypeval_new_list(a0);
    ttypeval_release(a0);
    return ttypeval_retain(result);
}

static ttypeval *native_type_list_string(void)
{
    ttypeval *a0 = native_type_string();
    ttypeval *result = ttypeval_new_list(a0);
    ttypeval_release(a0);
    return ttypeval_retain(result);
}

BUILTIN_TYPE(native_type_result, "Dictionary")

BUILTIN_TYPE(native_type_checkresult, "Dictionary")

BUILTIN_TYPE(native_type_origin, "Dictionary")

BUILTIN_TYPE(native_type_parameter, "Parameter")

BUILTIN_TYPE(native_type_condition, "Condition")

BUILTIN_TYPE(native_type_item, "RuleItem")

static ttypeval *native_type_item_selector(void)
{
    ttypeval *item = native_type_item();
    ttypeval *integer = native_type_int();
    ttypeval *string = native_type_string();
    ttypeval *members[] = {integer, string, item};
    ttypeval *result = ttypeval_new_union(members, 3);
    ttypeval_release(item);
    ttypeval_release(integer);
    ttypeval_release(string);
    return ttypeval_retain(result);
}

BUILTIN_TYPE(native_type_boolarray, "BoolArray")

BUILTIN_TYPE(native_type_capture, "Capture")

#undef BUILTIN_TYPE

typedef struct {
    const textension_module *module;
    const char *name;
    ttypeval *(*return_type)(void);
    const tparameter_spec *parameters;
    uint32_t count;
} function_declaration;
#define DECL(module_, name_, result_, ...) { &(module_), (name_), (result_), \
    (const tparameter_spec[]){ __VA_ARGS__ }, \
    sizeof((const tparameter_spec[]){ __VA_ARGS__ }) / sizeof(tparameter_spec) }
#define EMPTY(module_, name_, result_) { &(module_), (name_), (result_), nullptr, 0 }

static const function_declaration declarations[] = {
    DECL(tstdlib_array_functions, "array", native_type_anytype, {"rows", native_type_int, 0, 0}, {"cols", native_type_int, 0, 0}, {"value", native_type_anytype, 0, 0}),
    DECL(tstdlib_capability_functions, "idx", native_type_anytype, {"target", native_type_indexable, 0, 0}, {"key", native_type_anytype, 0, 0}),
    DECL(tstdlib_capability_functions, "append", native_type_nil, {"target", native_type_appendable, 0, 0}, {"value", native_type_anytype, 0, 0}),
    DECL(tstdlib_capability_functions, "delete", native_type_nil, {"target", native_type_deletable, 0, 0}, {"key", native_type_anytype, 0, 0}),
    DECL(tstdlib_console_functions, "print", native_type_nil, {"values", native_type_anytype, 0, 1}),
    DECL(tstdlib_console_functions, "pprint", native_type_nil, {"values", native_type_anytype, 0, 1}),
    DECL(tstdlib_console_functions, "input", native_type_string_nil, {"prompt", native_type_string, 1, 0}),
    DECL(tstdlib_conversion_functions, "int", native_type_int, {"value", native_type_anytype, 0, 0}),
    DECL(tstdlib_conversion_functions, "float", native_type_float, {"value", native_type_anytype, 0, 0}),
    DECL(tstdlib_conversion_functions, "bool", native_type_bool, {"value", native_type_anytype, 0, 0}),
    DECL(tstdlib_conversion_functions, "str", native_type_string, {"value", native_type_anytype, 0, 0}),
    DECL(tstdlib_dict_functions, "keys", native_type_list, {"value", native_type_dictionary_type, 0, 0}),
    DECL(tstdlib_dict_functions, "values", native_type_list, {"dictionary", native_type_dictionary, 0, 0}),
    DECL(tstdlib_iterator_functions, "iter", native_type_iterator, {"start", native_type_int, 0, 0}, {"step", native_type_int, 1, 0}, {"end", native_type_int, 0, 0}),
    DECL(tstdlib_list_functions, "list", native_type_list, {"values", native_type_anytype, 0, 1}),
    DECL(tstdlib_list_functions, "push_front", native_type_nil, {"list", native_type_list, 0, 0}, {"value", native_type_anytype, 0, 0}),
    DECL(tstdlib_list_functions, "push_back", native_type_nil, {"list", native_type_list, 0, 0}, {"value", native_type_anytype, 0, 0}),
    DECL(tstdlib_list_functions, "pop_front", native_type_anytype, {"list", native_type_list, 0, 0}),
    DECL(tstdlib_list_functions, "pop_back", native_type_anytype, {"list", native_type_list, 0, 0}),
    DECL(tstdlib_list_functions, "insert", native_type_nil, {"list", native_type_list, 0, 0}, {"value", native_type_anytype, 0, 0}, {"index", native_type_int, 0, 0}),
    DECL(tstdlib_list_functions, "concat", native_type_list, {"left", native_type_list, 0, 0}, {"right", native_type_list, 0, 0}),
    DECL(tstdlib_object_functions, "len", native_type_int, {"value", native_type_anytype, 0, 0}),
    DECL(tstdlib_object_functions, "type", native_type_string, {"value", native_type_anytype, 0, 0}),
    DECL(tstdlib_object_functions, "copy", native_type_anytype, {"value", native_type_anytype, 0, 0}),
    DECL(tstdlib_object_functions, "identical", native_type_bool, {"left", native_type_anytype, 0, 0}, {"right", native_type_anytype, 0, 0}),
    DECL(tstdlib_pair_functions, "pair", native_type_pair, {"first", native_type_anytype, 0, 0}, {"second", native_type_anytype, 0, 0}),
    DECL(tstdlib_reflection_functions, "parameters", native_type_list_pair_string_type, {"value", native_type_function_rule_ruleinstance_ruleir, 0, 0}),
    DECL(tstdlib_reflection_functions, "arguments", native_type_list_anytype, {"instance", native_type_ruleinstance, 0, 0}),
    DECL(tstdlib_rule_functions, "assert", native_type_nil, {"rule", native_type_ruleinstance_rule, 0, 0}),
    DECL(tstdlib_session_functions, "__ls__", native_type_list_string, {"library", native_type_library, 1, 0}),
    DECL(tstdlib_session_functions, "__path__", native_type_list_string, {"library", native_type_library, 1, 0}),
    DECL(tstdlib_session_functions, "__param__", native_type_anytype, {"index", native_type_int, 0, 0}),
    EMPTY(tstdlib_session_functions, "__nparam__", native_type_int),
    DECL(tstdlib_session_functions, "__binary__", native_type_nil, {"environment", native_type_anytype, 1, 0}),
    DECL(tstdlib_sort_functions, "sort", native_type_nil, {"values", native_type_list, 0, 0}),
    EMPTY(tstdlib_time_functions, "clock", native_type_float),
    EMPTY(tstdlib_time_functions, "clock_ns", native_type_int),
    EMPTY(tstdlib_time_functions, "now", native_type_time),
    DECL(tstdlib_dense_module, "rows", native_type_int, {"value", native_type_realarray_boolarray, 0, 0}),
    DECL(tstdlib_dense_module, "cols", native_type_int, {"value", native_type_realarray_boolarray, 0, 0}),
    DECL(tstdlib_dense_module, "transpose", native_type_anytype, {"value", native_type_realarray_boolarray, 0, 0}),
    DECL(tstdlib_dense_module, "identity", native_type_realarray, {"size", native_type_int, 0, 0}),
    DECL(tstdlib_dense_module, "trace", native_type_float, {"value", native_type_realarray, 0, 0}),
    DECL(tstdlib_dense_module, "inner", native_type_float, {"left", native_type_realarray, 0, 0}, {"right", native_type_realarray, 0, 0}),
    DECL(tstdlib_dense_module, "outer", native_type_realarray, {"left", native_type_realarray, 0, 0}, {"right", native_type_realarray, 0, 0}),
    DECL(tstdlib_dense_module, "norm", native_type_float, {"value", native_type_realarray, 0, 0}),
    DECL(tstdlib_dense_module, "normalize", native_type_realarray, {"value", native_type_realarray, 0, 0}),
    DECL(tstdlib_dense_module, "copy_into", native_type_nil, {"source", native_type_realarray, 0, 0}, {"target", native_type_realarray, 0, 0}),
    DECL(tstdlib_dense_module, "scale_inplace", native_type_nil, {"value", native_type_realarray, 0, 0}, {"factor", native_type_int_float, 0, 0}),
    DECL(tstdlib_dense_module, "add_scaled_inplace", native_type_nil, {"target", native_type_realarray, 0, 0}, {"source", native_type_realarray, 0, 0}, {"factor", native_type_int_float, 0, 0}),
    DECL(tstdlib_dense_module, "gemm", native_type_nil, {"alpha", native_type_int_float, 0, 0}, {"left", native_type_realarray, 0, 0}, {"right", native_type_realarray, 0, 0}, {"beta", native_type_int_float, 0, 0}, {"output", native_type_realarray, 0, 0}),
    DECL(tstdlib_evaluators_module, "make", native_type_evaluator, {"name", native_type_string, 0, 0}, {"version", native_type_int, 0, 0}, {"evaluate", native_type_function, 0, 0}, {"compile", native_type_function, 1, 0}),
    DECL(tstdlib_evaluators_module, "eval", native_type_anytype, {"instance", native_type_ruleinstance, 0, 0}, {"evaluator", native_type_evaluator, 0, 0}),
    DECL(tstdlib_evaluators_module, "compile", native_type_result, {"rule", native_type_rule, 0, 0}, {"evaluator", native_type_evaluator, 0, 0}),
    DECL(tstdlib_evaluators_module, "binding", native_type_anytype, {"context", native_type_context, 0, 0}, {"parameter", native_type_parameter_int, 0, 0}),
    DECL(tstdlib_evaluators_module, "capture", native_type_anytype, {"context", native_type_context, 0, 0}, {"capture", native_type_capture_int, 0, 0}),
    DECL(tstdlib_evaluators_module, "value", native_type_anytype, {"context", native_type_context, 0, 0}, {"term", native_type_term, 0, 0}),
    DECL(tstdlib_evaluators_module, "requirement", native_type_ruleinstance, {"context", native_type_context, 0, 0}, {"requirement", native_type_requirement, 0, 0}),
    DECL(tstdlib_io_module, "read_text", native_type_string, {"path", native_type_string, 0, 0}),
    DECL(tstdlib_io_module, "write_text", native_type_nil, {"path", native_type_string, 0, 0}, {"text", native_type_string, 0, 0}),
    DECL(tstdlib_io_module, "append_text", native_type_nil, {"path", native_type_string, 0, 0}, {"text", native_type_string, 0, 0}),
    DECL(tstdlib_io_module, "replace_text", native_type_nil, {"path", native_type_string, 0, 0}, {"text", native_type_string, 0, 0}),
    DECL(tstdlib_math_module, "abs", native_type_anytype, {"value", native_type_union_int_float, 0, 0}),
    DECL(tstdlib_math_module, "fabs", native_type_float, {"value", native_type_union_int_float, 0, 0}),
    DECL(tstdlib_math_module, "sqrt", native_type_float, {"value", native_type_union_int_float, 0, 0}),
    DECL(tstdlib_math_module, "rsqrt", native_type_float, {"value", native_type_union_int_float, 0, 0}),
    DECL(tstdlib_math_module, "cbrt", native_type_float, {"value", native_type_union_int_float, 0, 0}),
    DECL(tstdlib_math_module, "pow", native_type_float, {"left", native_type_union_int_float, 0, 0}, {"right", native_type_union_int_float, 0, 0}),
    DECL(tstdlib_math_module, "hypot", native_type_float, {"left", native_type_union_int_float, 0, 0}, {"right", native_type_union_int_float, 0, 0}),
    DECL(tstdlib_math_module, "sin", native_type_float, {"value", native_type_union_int_float, 0, 0}),
    DECL(tstdlib_math_module, "cos", native_type_float, {"value", native_type_union_int_float, 0, 0}),
    DECL(tstdlib_math_module, "tan", native_type_float, {"value", native_type_union_int_float, 0, 0}),
    DECL(tstdlib_math_module, "asin", native_type_float, {"value", native_type_union_int_float, 0, 0}),
    DECL(tstdlib_math_module, "acos", native_type_float, {"value", native_type_union_int_float, 0, 0}),
    DECL(tstdlib_math_module, "atan", native_type_float, {"value", native_type_union_int_float, 0, 0}),
    DECL(tstdlib_math_module, "atan2", native_type_float, {"left", native_type_union_int_float, 0, 0}, {"right", native_type_union_int_float, 0, 0}),
    DECL(tstdlib_math_module, "sinh", native_type_float, {"value", native_type_union_int_float, 0, 0}),
    DECL(tstdlib_math_module, "cosh", native_type_float, {"value", native_type_union_int_float, 0, 0}),
    DECL(tstdlib_math_module, "tanh", native_type_float, {"value", native_type_union_int_float, 0, 0}),
    DECL(tstdlib_math_module, "asinh", native_type_float, {"value", native_type_union_int_float, 0, 0}),
    DECL(tstdlib_math_module, "acosh", native_type_float, {"value", native_type_union_int_float, 0, 0}),
    DECL(tstdlib_math_module, "atanh", native_type_float, {"value", native_type_union_int_float, 0, 0}),
    DECL(tstdlib_math_module, "exp", native_type_float, {"value", native_type_union_int_float, 0, 0}),
    DECL(tstdlib_math_module, "exp2", native_type_float, {"value", native_type_union_int_float, 0, 0}),
    DECL(tstdlib_math_module, "expm1", native_type_float, {"value", native_type_union_int_float, 0, 0}),
    DECL(tstdlib_math_module, "log", native_type_float, {"value", native_type_union_int_float, 0, 0}),
    DECL(tstdlib_math_module, "log2", native_type_float, {"value", native_type_union_int_float, 0, 0}),
    DECL(tstdlib_math_module, "log10", native_type_float, {"value", native_type_union_int_float, 0, 0}),
    DECL(tstdlib_math_module, "log1p", native_type_float, {"value", native_type_union_int_float, 0, 0}),
    DECL(tstdlib_math_module, "logb", native_type_float, {"value", native_type_union_int_float, 0, 0}),
    DECL(tstdlib_math_module, "ilogb", native_type_int, {"value", native_type_union_int_float, 0, 0}),
    DECL(tstdlib_math_module, "frexp", native_type_pair, {"value", native_type_union_int_float, 0, 0}),
    DECL(tstdlib_math_module, "modf", native_type_pair, {"value", native_type_union_int_float, 0, 0}),
    DECL(tstdlib_math_module, "ldexp", native_type_float, {"value", native_type_union_int_float, 0, 0}, {"exponent", native_type_int, 0, 0}),
    DECL(tstdlib_math_module, "scalbn", native_type_float, {"value", native_type_union_int_float, 0, 0}, {"exponent", native_type_int, 0, 0}),
    DECL(tstdlib_math_module, "scalbln", native_type_float, {"value", native_type_union_int_float, 0, 0}, {"exponent", native_type_int, 0, 0}),
    DECL(tstdlib_math_module, "erf", native_type_float, {"value", native_type_union_int_float, 0, 0}),
    DECL(tstdlib_math_module, "erfc", native_type_float, {"value", native_type_union_int_float, 0, 0}),
    DECL(tstdlib_math_module, "lgamma", native_type_float, {"value", native_type_union_int_float, 0, 0}),
    DECL(tstdlib_math_module, "tgamma", native_type_float, {"value", native_type_union_int_float, 0, 0}),
    DECL(tstdlib_math_module, "ceil", native_type_float, {"value", native_type_union_int_float, 0, 0}),
    DECL(tstdlib_math_module, "floor", native_type_float, {"value", native_type_union_int_float, 0, 0}),
    DECL(tstdlib_math_module, "nearbyint", native_type_float, {"value", native_type_union_int_float, 0, 0}),
    DECL(tstdlib_math_module, "rint", native_type_float, {"value", native_type_union_int_float, 0, 0}),
    DECL(tstdlib_math_module, "lrint", native_type_int, {"value", native_type_union_int_float, 0, 0}),
    DECL(tstdlib_math_module, "llrint", native_type_int, {"value", native_type_union_int_float, 0, 0}),
    DECL(tstdlib_math_module, "round", native_type_float, {"value", native_type_union_int_float, 0, 0}),
    DECL(tstdlib_math_module, "lround", native_type_int, {"value", native_type_union_int_float, 0, 0}),
    DECL(tstdlib_math_module, "llround", native_type_int, {"value", native_type_union_int_float, 0, 0}),
    DECL(tstdlib_math_module, "trunc", native_type_float, {"value", native_type_union_int_float, 0, 0}),
    DECL(tstdlib_math_module, "fmod", native_type_float, {"left", native_type_union_int_float, 0, 0}, {"right", native_type_union_int_float, 0, 0}),
    DECL(tstdlib_math_module, "remainder", native_type_float, {"left", native_type_union_int_float, 0, 0}, {"right", native_type_union_int_float, 0, 0}),
    DECL(tstdlib_math_module, "remquo", native_type_pair, {"left", native_type_union_int_float, 0, 0}, {"right", native_type_union_int_float, 0, 0}),
    DECL(tstdlib_math_module, "copysign", native_type_float, {"left", native_type_union_int_float, 0, 0}, {"right", native_type_union_int_float, 0, 0}),
    DECL(tstdlib_math_module, "nextafter", native_type_float, {"left", native_type_union_int_float, 0, 0}, {"right", native_type_union_int_float, 0, 0}),
    DECL(tstdlib_math_module, "fdim", native_type_float, {"left", native_type_union_int_float, 0, 0}, {"right", native_type_union_int_float, 0, 0}),
    DECL(tstdlib_math_module, "fmax", native_type_float, {"left", native_type_union_int_float, 0, 0}, {"right", native_type_union_int_float, 0, 0}),
    DECL(tstdlib_math_module, "fmin", native_type_float, {"left", native_type_union_int_float, 0, 0}, {"right", native_type_union_int_float, 0, 0}),
    DECL(tstdlib_math_module, "fma", native_type_float, {"first", native_type_union_int_float, 0, 0}, {"second", native_type_union_int_float, 0, 0}, {"third", native_type_union_int_float, 0, 0}),
    DECL(tstdlib_math_module, "eleinv", native_type_float, {"value", native_type_union_int_float, 0, 0}),
    EMPTY(tstdlib_math_module, "make_nan", native_type_float),
    DECL(tstdlib_math_module, "isfinite", native_type_bool, {"value", native_type_union_int_float, 0, 0}),
    DECL(tstdlib_math_module, "isinf", native_type_bool, {"value", native_type_union_int_float, 0, 0}),
    DECL(tstdlib_math_module, "isnan", native_type_bool, {"value", native_type_union_int_float, 0, 0}),
    DECL(tstdlib_math_module, "isnormal", native_type_bool, {"value", native_type_union_int_float, 0, 0}),
    DECL(tstdlib_math_module, "fpclassify", native_type_int, {"value", native_type_union_int_float, 0, 0}),
    DECL(tstdlib_math_module, "signbit", native_type_bool, {"value", native_type_union_int_float, 0, 0}),
    DECL(tstdlib_math_module, "isgreater", native_type_bool, {"left", native_type_union_int_float, 0, 0}, {"right", native_type_union_int_float, 0, 0}),
    DECL(tstdlib_math_module, "isgreaterequal", native_type_bool, {"left", native_type_union_int_float, 0, 0}, {"right", native_type_union_int_float, 0, 0}),
    DECL(tstdlib_math_module, "isless", native_type_bool, {"left", native_type_union_int_float, 0, 0}, {"right", native_type_union_int_float, 0, 0}),
    DECL(tstdlib_math_module, "islessequal", native_type_bool, {"left", native_type_union_int_float, 0, 0}, {"right", native_type_union_int_float, 0, 0}),
    DECL(tstdlib_math_module, "islessgreater", native_type_bool, {"left", native_type_union_int_float, 0, 0}, {"right", native_type_union_int_float, 0, 0}),
    DECL(tstdlib_math_module, "isunordered", native_type_bool, {"left", native_type_union_int_float, 0, 0}, {"right", native_type_union_int_float, 0, 0}),
    DECL(tstdlib_rules_module, "term", native_type_type, {"type", native_type_type, 0, 0}),
    DECL(tstdlib_solve_module, "hold", native_type_hold_result, {"rule", native_type_ruleinstance_rule, 0, 0}),
    DECL(tstdlib_solve_module, "sample", native_type_sample_result,
	{"rule", native_type_rule, 0, 0},
	{"count", native_type_int, 0, 0},
	{"space", native_type_dictionary_string_indexable, 0, 0},
	{"distributions", native_type_dictionary_string_distribution, 1, 0},
	{"rng", native_type_random_generator, 1, 0},
	{"candidate_limit", native_type_int, 1, 0},
	{"time_limit", native_type_float_nil, 1, 0},
	{"core_budget_factor", native_type_float, 1, 0},
	{"fairness_interval", native_type_int, 1, 0}),
    DECL(tstdlib_rules_module, "check", native_type_checkresult, {"rule", native_type_ruleinstance_rule, 0, 0}),
    DECL(tstdlib_rules_module, "inspect", native_type_ruleir, {"rule", native_type_ruleinstance_rule, 0, 0}),
    DECL(tstdlib_rules_module, "parameters", native_type_list, {"ir", native_type_ruleir, 0, 0}),
    DECL(tstdlib_rules_module, "items", native_type_list, {"ir", native_type_ruleir, 0, 0}),
    DECL(tstdlib_rules_module, "terms", native_type_list, {"ir", native_type_ruleir, 0, 0}),
    DECL(tstdlib_rules_module, "origin", native_type_origin, {"value", native_type_anytype, 0, 0}),
    DECL(tstdlib_rules_module, "semantic_hash", native_type_int, {"value", native_type_ruleir_rule, 0, 0}),
    DECL(tstdlib_rules_module, "content_hash", native_type_int, {"value", native_type_ruleir_rule, 0, 0}),
    DECL(tstdlib_rules_module, "serialize", native_type_string, {"value", native_type_ruleir_rule, 0, 0}),
    DECL(tstdlib_rules_module, "deserialize", native_type_rule, {"data", native_type_string, 0, 0}),
    DECL(tstdlib_rules_module, "parameter", native_type_parameter, {"name", native_type_string, 0, 0}, {"type", native_type_type, 0, 0}),
    DECL(tstdlib_rules_module, "constant", native_type_term, {"value", native_type_anytype, 0, 0}),
    DECL(tstdlib_rules_module, "call", native_type_term, {"function", native_type_anytype, 0, 0}, {"arguments", native_type_list_term, 0, 0}),
    DECL(tstdlib_rules_module, "conjunction", native_type_term, {"left", native_type_term, 0, 0}, {"right", native_type_term, 0, 0}),
    DECL(tstdlib_rules_module, "disjunction", native_type_term, {"left", native_type_term, 0, 0}, {"right", native_type_term, 0, 0}),
    DECL(tstdlib_rules_module, "negation", native_type_term, {"operand", native_type_term, 0, 0}),
    DECL(tstdlib_rules_module, "condition", native_type_condition, {"term", native_type_term, 0, 0}, {"description", native_type_string, 1, 0}),
    DECL(tstdlib_rules_module, "requirement", native_type_requirement, {"rule", native_type_rule_term, 0, 0}, {"arguments", native_type_list_term, 0, 0}),
    DECL(tstdlib_rules_module, "implication", native_type_item, {"antecedent", native_type_term, 0, 0}, {"consequents", native_type_list_term, 0, 0}, {"description", native_type_string, 1, 0}),
    DECL(tstdlib_rules_module, "extension", native_type_term, {"provider", native_type_string, 0, 0}, {"kind", native_type_string, 0, 0}, {"arguments", native_type_list_term, 0, 0}, {"payload", native_type_anytype, 0, 0}),
    DECL(tstdlib_rules_module, "membership", native_type_term, {"value", native_type_term, 0, 0}, {"domain", native_type_term, 0, 0}),
    DECL(tstdlib_rules_module, "restrict", native_type_rule, {"rule", native_type_rule, 0, 0}, {"restrictions", native_type_pair, 0, 1}),
    DECL(tstdlib_rules_module, "item", native_type_item, {"rule", native_type_rule, 0, 0}, {"target", native_type_item_selector, 0, 0}),
    DECL(tstdlib_rules_module, "drop", native_type_rule, {"rule", native_type_rule, 0, 0}, {"target", native_type_item_selector, 0, 0}),
    DECL(tstdlib_rules_module, "violate", native_type_rule, {"rule", native_type_rule, 0, 0}, {"target", native_type_item_selector, 0, 0}),
    DECL(tstdlib_rules_module, "drop_if", native_type_rule, {"rule", native_type_rule, 0, 0}, {"target", native_type_item_selector, 0, 0}, {"premise", native_type_rule, 0, 0}),
    DECL(tstdlib_rules_module, "violate_if", native_type_rule, {"rule", native_type_rule, 0, 0}, {"target", native_type_item_selector, 0, 0}, {"premise", native_type_rule, 0, 0}),
    DECL(tstdlib_rules_module, "points", native_type_points, {"element_type", native_type_type, 0, 0}, {"values", native_type_anytype, 0, 1}),
    DECL(tstdlib_rules_module, "range", native_type_range_int, {"start", native_type_int, 0, 0}, {"end", native_type_int, 0, 0}),
    DECL(tstdlib_rules_module, "make", native_type_rule, {"display_name", native_type_string, 0, 0}, {"parameters", native_type_list_parameter, 0, 0}, {"items", native_type_list_item, 0, 0}),
    DECL(tstdlib_syntax_module, "tokens", native_type_list, {"source", native_type_string, 0, 0}),
    DECL(tstdlib_time_module, "from_unix", native_type_time, {"seconds", native_type_int, 0, 0}),
    DECL(tstdlib_time_module, "unix", native_type_int, {"value", native_type_time, 0, 0}),
    DECL(tstdlib_time_module, "format", native_type_string, {"value", native_type_time, 0, 0}, {"pattern", native_type_string, 0, 0}),
    DECL(tstdlib_random_module, "generator", native_type_random_generator, {"source", native_type_random_source, 0, 0}, {"seed", native_type_int, 0, 0}),
    DECL(tstdlib_random_module, "next_int", native_type_int, {"rng", native_type_random_generator, 0, 0}, {"bound", native_type_int, 0, 0}),
    DECL(tstdlib_random_module, "next_float", native_type_float, {"rng", native_type_random_generator, 0, 0}),
    DECL(tstdlib_random_module, "next_bool", native_type_bool, {"rng", native_type_random_generator, 0, 0}),
    DECL(tstdlib_random_module, "advance", native_type_nil, {"rng", native_type_random_generator, 0, 0}, {"steps", native_type_int, 0, 0}),
    DECL(tstdlib_finite_module, "index", native_type_finite_index, {"coords", native_type_list, 0, 0}),
    DECL(tstdlib_finite_module, "shape", native_type_finite_index, {"dist", native_type_finite_distribution, 0, 0}),
    DECL(tstdlib_finite_module, "prob", native_type_float, {"dist", native_type_finite_distribution, 0, 0}, {"index", native_type_finite_index, 0, 0}),
    DECL(tstdlib_finite_module, "mass", native_type_float, {"dist", native_type_finite_distribution, 0, 0}, {"indices", native_type_list_finite_index, 0, 0}),
    DECL(tstdlib_finite_module, "samples", native_type_list_finite_index, {"dist", native_type_finite_distribution, 0, 0}, {"count", native_type_int, 0, 0}, {"rng", native_type_random_generator, 0, 0}),
    DECL(tstdlib_finite_module, "uniform", native_type_finite_distribution, {"n", native_type_int, 0, 0}),
    DECL(tstdlib_finite_module, "categorical", native_type_finite_distribution, {"weights", native_type_list, 0, 0}),
    DECL(tstdlib_finite_module, "piecewise_constant", native_type_finite_distribution, {"n", native_type_int, 0, 0}, {"steps", native_type_list, 0, 0}),
    DECL(tstdlib_finite_module, "piecewise_linear", native_type_finite_distribution, {"knots", native_type_list, 0, 0}),
    DECL(tstdlib_finite_module, "product", native_type_finite_distribution, {"dists", native_type_list, 0, 0}),
    DECL(tstdlib_finite_module, "mixture", native_type_finite_distribution, {"dists", native_type_list, 0, 0}, {"weights", native_type_list, 0, 0}),
    DECL(tstdlib_finite_module, "conditional", native_type_finite_distribution, {"dist", native_type_finite_distribution, 0, 0}, {"indices", native_type_list_finite_index, 0, 0}),
    DECL(tstdlib_finite_module, "map", native_type_finite_distribution, {"dist", native_type_finite_distribution, 0, 0}, {"shape", native_type_finite_index, 0, 0}, {"mapping", native_type_function, 0, 0}),
    DECL(tstdlib_finite_module, "bernoulli", native_type_finite_distribution, {"p", native_type_float, 0, 0}),
    DECL(tstdlib_finite_module, "binomial", native_type_finite_distribution, {"trials", native_type_int, 0, 0}, {"p", native_type_float, 0, 0}),
    DECL(tstdlib_finite_module, "multinomial", native_type_finite_distribution, {"trials", native_type_int, 0, 0}, {"weights", native_type_list, 0, 0}),
    DECL(tstdlib_finite_module, "hypergeometric", native_type_finite_distribution, {"population", native_type_int, 0, 0}, {"successes", native_type_int, 0, 0}, {"draws", native_type_int, 0, 0}),
    DECL(tstdlib_finite_module, "zipf", native_type_finite_distribution, {"n", native_type_int, 0, 0}, {"exponent", native_type_float, 0, 0}),
    DECL(tstdlib_types_module, "make_type", native_type_type, {"fields", native_type_pair_string_type, 0, 1}),
    DECL(tstdlib_types_module, "union", native_type_type, {"members", native_type_type, 0, 1}),
    DECL(tstdlib_types_module, "enum", native_type_type, {"members", native_type_string, 0, 1}),
    DECL(tstdlib_types_module, "list", native_type_type, {"item", native_type_type, 0, 0}),
    DECL(tstdlib_types_module, "iterator", native_type_type, {"item", native_type_type, 0, 0}),
    DECL(tstdlib_types_module, "optional", native_type_type, {"field", native_type_pair_string_type, 0, 0}),
    DECL(tstdlib_types_module, "pair", native_type_type, {"first", native_type_type, 0, 0}, {"second", native_type_type, 0, 0}),
    DECL(tstdlib_types_module, "dictionary", native_type_type, {"key", native_type_type, 0, 0}, {"value", native_type_type, 0, 0}),
    DECL(tstdlib_types_module, "rule", native_type_type, {"parameters", native_type_type, 0, 1}),
    DECL(tstdlib_types_module, "rule_instance", native_type_type, {"parameters", native_type_type, 0, 1}),
    DECL(tstdlib_types_module, "of", native_type_type, {"value", native_type_anytype, 0, 0}),
    DECL(tstdlib_types_module, "matches", native_type_bool, {"value", native_type_anytype, 0, 0}, {"expected", native_type_type, 0, 0}),
    DECL(tstdlib_types_module, "fields", native_type_dictionary, {"value", native_type_type, 0, 0}),
    DECL(tstdlib_types_module, "optional_fields", native_type_list_string, {"value", native_type_type, 0, 0}),
    DECL(tstdlib_types_module, "members", native_type_list, {"value", native_type_type, 0, 0}),
    DECL(tstdlib_types_module, "base", native_type_type, {"value", native_type_type, 0, 0}),
    DECL(tstdlib_types_module, "parameters", native_type_dictionary, {"value", native_type_type, 0, 0}),
    DECL(tstdlib_types_module, "definition", native_type_dictionary, {"value", native_type_type, 0, 0}),
    DECL(tstdlib_types_module, "parameter", native_type_type, {"name", native_type_string, 0, 0}),
    DECL(tstdlib_types_module, "value_parameter", native_type_type, {"name", native_type_string, 0, 0}),
    DECL(tstdlib_types_module, "template", native_type_type, {"type_parameters", native_type_list, 0, 0}, {"value_parameters", native_type_list, 0, 0}, {"definition", native_type_type, 0, 0}),
};
#undef DECL
#undef EMPTY

static tfunction_metadata *make_metadata(const function_declaration *declaration)
{
    ttypeval *result_type = declaration->return_type();
    tfunction_metadata *metadata = tfunction_metadata_new(declaration->parameters,
        declaration->count, result_type, 0);
    ttypeval_release(result_type);
    return metadata;
}
static const textension_symbol *declared_symbol(const function_declaration *declaration)
{
    for (uint32_t i = 0; i < declaration->module->symbol_count; i++) {
        const textension_symbol *symbol = &declaration->module->symbols[i];
        if (!strcmp(symbol->name, declaration->name)) return symbol;
    }
    return nullptr;
}

int tstdlib_validate_function_metadata(void)
{
    const textension_descriptor *stdlib = tstdlib_descriptor();
    for (uint32_t i = 0; i < textension_symbol_count(stdlib); i++) {
        textension_symbol_ref symbol;
        if (!textension_symbol_at(stdlib, i, &symbol) ||
            strncmp(symbol.symbol->type, "Function[", 9)) continue;
        int found = 0;
        for (uint32_t j = 0; j < sizeof(declarations) / sizeof(declarations[0]); j++)
            if (declarations[j].module == symbol.module &&
                !strcmp(declarations[j].name, symbol.symbol->name)) found++;
        if (found != 1) return 0;
    }
    for (uint32_t i = 0; i < sizeof(declarations) / sizeof(declarations[0]); i++) {
        const function_declaration *declaration = &declarations[i];
        const textension_symbol *symbol = declared_symbol(declaration);
        if (!symbol) return 0;
        tfunction_metadata *metadata = make_metadata(declaration);
        if (!metadata) {
            fprintf(stderr, "Invalid function metadata: %s (%s)\n", declaration->name, symbol->type);
            return 0;
        }
        tfunction_metadata_release(metadata);
    }
    return 1;
}

tfunction_metadata *tstdlib_function_metadata(const tobj *value)
{
    tfunction_metadata **slot = nullptr;
    tcompo_type kind = tobj_compo_type(value);
    if (kind == compo_cppfunc) slot = &((tcppgenf *)value->val.v_tcompo)->metadata;
    else if (kind == compo_sessfunc) slot = &((tcppsessf *)value->val.v_tcompo)->metadata;
    else if (kind == compo_trule_builtin) slot = &((trule_builtin *)value->val.v_tcompo)->metadata;
    if (!slot || *slot) return slot ? *slot : nullptr;
    for (uint32_t i = 0; i < sizeof(declarations) / sizeof(declarations[0]); i++) {
        const function_declaration *declaration = &declarations[i];
        const textension_symbol *symbol = declared_symbol(declaration);
        if (!symbol) continue;
        int matches = 0;
        if (kind == compo_cppfunc)
            matches = symbol->function == ((tcppgenf *)value->val.v_tcompo)->f;
        else if (kind == compo_sessfunc)
            matches = symbol->session_function == ((tcppsessf *)value->val.v_tcompo)->f;
        else if (symbol->value_factory) {
            tobj candidate;
            tobj_set_nil(&candidate);
            symbol->value_factory(&candidate);
            matches = tobj_compo_type(&candidate) == compo_trule_builtin &&
                ((trule_builtin *)candidate.val.v_tcompo)->kind ==
                ((trule_builtin *)value->val.v_tcompo)->kind;
            tobj_try_clear(&candidate);
        }
        if (!matches) continue;
        *slot = make_metadata(declaration);
        return *slot;
    }
    return nullptr;
}
