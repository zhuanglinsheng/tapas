#include "tapas/textension.h"

#include "tapas/runtime/tlist.h"
#include "tapas/runtime/trule.h"
#include "tapas/runtime/trule_ir.h"
#include "tapas/runtime/tstr.h"
#include "tapas/runtime/ttype.h"

#include <stdlib.h>


static void count_is(const char *name, uint_regs count, uint_regs expected)
{
	if (count != expected)
		twarn(ErrRuntime_ParamsCtr, name, "incorrect parameter count");
}

static const char *string_is(const tobj *value, const char *name)
{
	if (!value || value->type != tcompo ||
	    tobj_compo_type(value) != compo_tstr)
		twarn(ErrRuntime_ParamsType, name, "String required");
	return tstring_cstr(((tstr *)value->val.v_tcompo)->data);
}

static ttypeval *type_is(const tobj *value, const char *name)
{
	if (!value || value->type != tcompo ||
	    tobj_compo_type(value) != compo_ttypeval)
		twarn(ErrRuntime_ParamsType, name, "Type required");
	return (ttypeval *)value->val.v_tcompo;
}

static tlist *list_is(const tobj *value, const char *name)
{
	if (!value || value->type != tcompo ||
	    tobj_compo_type(value) != compo_tlist)
		twarn(ErrRuntime_ParamsType, name, "List required");
	return (tlist *)value->val.v_tcompo;
}

static trule_term *term_is(const tobj *value, const char *name)
{
	if (!value || value->type != tcompo ||
	    tobj_compo_type(value) != compo_trule_term)
		twarn(ErrRuntime_ParamsType, name, "Rule Term required");
	return (trule_term *)value->val.v_tcompo;
}

static trule_item *item_is(const tobj *value, const char *name)
{
	if (!value || value->type != tcompo ||
	    tobj_compo_type(value) != compo_trule_item)
		twarn(ErrRuntime_ParamsType, name, "Rule Item required");
	return (trule_item *)value->val.v_tcompo;
}

static tobj *term_list(tlist *list, const char *name, uint_regs *count)
{
	*count = (uint_regs)tlist_size(list);
	for (uint_objs i = 0; i < tlist_size(list); i++)
		term_is(tlist_at(list, i), name);
	return list->items.data;
}

static void create_type(tobj *result, tbuiltin_id id)
{
	tobj_set_compo(result, (tcompo_v *)ttypeval_builtin(id));
}

#define TYPE_FACTORY(name, id) \
	static void name(tobj *result) { create_type(result, id); }

TYPE_FACTORY(create_rule_ir, tbuiltin_rule_ir)
TYPE_FACTORY(create_parameter_type, tbuiltin_rule_parameter)
TYPE_FACTORY(create_capture_type, tbuiltin_rule_capture)
TYPE_FACTORY(create_item_type, tbuiltin_rule_item)
TYPE_FACTORY(create_condition_type, tbuiltin_rule_condition)
TYPE_FACTORY(create_requirement_type, tbuiltin_rule_requirement)
TYPE_FACTORY(create_term_type, tbuiltin_rule_term)
TYPE_FACTORY(create_origin_type, tbuiltin_rule_origin)
TYPE_FACTORY(create_check_result_type, tbuiltin_rule_check_result)
TYPE_FACTORY(create_violation_type, tbuiltin_rule_violation)
TYPE_FACTORY(create_diagnostic_type, tbuiltin_rule_diagnostic)

static void create_builtin(tobj *result, trule_builtin_kind kind)
{
	tobj_set_compo(result, (tcompo_v *)trule_builtin_new(kind));
}

#define BUILTIN_FACTORY(name, kind) \
	static void name(tobj *result) { create_builtin(result, kind); }

BUILTIN_FACTORY(create_check, trule_builtin_check)
BUILTIN_FACTORY(create_inspect, trule_builtin_inspect)
BUILTIN_FACTORY(create_parameters, trule_builtin_parameters)
BUILTIN_FACTORY(create_items, trule_builtin_items)
BUILTIN_FACTORY(create_terms, trule_builtin_terms)
BUILTIN_FACTORY(create_origin, trule_builtin_origin)
BUILTIN_FACTORY(create_semantic_hash, trule_builtin_semantic_hash)
BUILTIN_FACTORY(create_content_hash, trule_builtin_content_hash)
BUILTIN_FACTORY(create_serialize, trule_builtin_serialize)
BUILTIN_FACTORY(create_deserialize, trule_builtin_deserialize)

static void rules_term(tobj *params, uint_regs count, tobj *result)
{
	count_is("rules::term", count, 1);
	tobj_set_compo(result, (tcompo_v *)ttypeval_new_rule_term(
		type_is(&params[0], "rules::term")));
}

static void rules_parameter(tobj *params, uint_regs count, tobj *result)
{
	count_is("rules::parameter", count, 2);
	tobj_set_compo(result, (tcompo_v *)trule_term_parameter_new(
		string_is(&params[0], "rules::parameter"),
		type_is(&params[1], "rules::parameter")));
}

static void rules_constant(tobj *params, uint_regs count, tobj *result)
{
	count_is("rules::constant", count, 1);
	tobj_set_compo(result, (tcompo_v *)trule_term_constant_new(&params[0]));
}

static void rules_call(tobj *params, uint_regs count, tobj *result)
{
	count_is("rules::call", count, 2);
	if (params[0].type != tcompo || !params[0].val.v_tcompo)
		twarn(ErrRuntime_ParamsType, "rules::call", "Function Term required");
	if (tobj_compo_type(&params[0]) != compo_trule_term &&
	    tobj_compo_type(&params[0]) != compo_tfunc &&
	    tobj_compo_type(&params[0]) != compo_cppfunc &&
	    tobj_compo_type(&params[0]) != compo_sessfunc)
		twarn(ErrRuntime_ParamsType, "rules::call", "Function Term required");
	tlist *arguments = list_is(&params[1], "rules::call");
	uint_regs argument_count;
	term_list(arguments, "rules::call", &argument_count);
	trule_term *function = params[0].type == tcompo &&
		tobj_compo_type(&params[0]) == compo_trule_term ?
		(trule_term *)params[0].val.v_tcompo :
		trule_term_constant_new(&params[0]);
	tobj payload;
	tobj_set_nil(&payload);
	tobj_set_compo(&payload, (tcompo_v *)function);
	tobj_set_compo(result, (tcompo_v *)trule_term_new(
		trule_term_call, ttypeval_builtin(tbuiltin_any), &payload,
		arguments->items.data, argument_count));
	if (params[0].type != tcompo ||
	    tobj_compo_type(&params[0]) != compo_trule_term)
		tobj_try_clear(&payload);
}

static void rules_condition(tobj *params, uint_regs count, tobj *result)
{
	if (count < 1 || count > 2)
		twarn(ErrRuntime_ParamsCtr, "rules::condition",
		      "one or two arguments required");
	trule_term *term = term_is(&params[0], "rules::condition");
	if (!ttypeval_equal(term->type, ttypeval_builtin(tbuiltin_bool)))
		twarn(ErrRuntime_ParamsType, "rules::condition", "Bool Term required");
	const char *description = count == 2 && params[1].type != tnil ?
		string_is(&params[1], "rules::condition") : "";
	tobj_set_compo(result, (tcompo_v *)trule_condition_new(term, description));
}

static void rules_requirement(tobj *params, uint_regs count, tobj *result)
{
	count_is("rules::requirement", count, 2);
	int owned = params[0].type != tcompo ||
		tobj_compo_type(&params[0]) != compo_trule_term;
	if (owned && (params[0].type != tcompo ||
	    tobj_compo_type(&params[0]) != compo_trule))
		twarn(ErrRuntime_ParamsType, "rules::requirement",
		      "Rule or Rule Term required");
	trule_term *rule = owned ? trule_term_constant_new(&params[0]) :
		(trule_term *)params[0].val.v_tcompo;
	tlist *arguments = list_is(&params[1], "rules::requirement");
	uint_regs argument_count;
	term_list(arguments, "rules::requirement", &argument_count);
	tobj_set_compo(result, (tcompo_v *)trule_requirement_new(
		rule, arguments->items.data, argument_count));
	if (owned && rule->base.refctr > 0) rule->base.refctr--;
}

static void rules_extension(tobj *params, uint_regs count, tobj *result)
{
	count_is("rules::extension", count, 4);
	tlist *arguments = list_is(&params[2], "rules::extension");
	uint_regs argument_count;
	term_list(arguments, "rules::extension", &argument_count);
	tobj_set_compo(result, (tcompo_v *)trule_term_extension_new(
		string_is(&params[0], "rules::extension"),
		string_is(&params[1], "rules::extension"),
		arguments->items.data, argument_count, &params[3]));
}

static int parameter_index(const trule_ir *ir, const trule_term *parameter)
{
	for (uint_objs i = 0; i < ir->parameters.len; i++)
		if (((trule_term *)ir->parameters.data[i].val.v_tcompo)->id ==
		    parameter->id)
			return (int)i;
	return -1;
}

static void validate_term(const trule_ir *ir, const trule_term *term,
			  trule_term **path, uint_objs depth)
{
	if (depth >= 256) twarn(ErrRuntime_Other, "rules::make", "cyclic Term graph");
	for (uint_objs i = 0; i < depth; i++)
		if (path[i] == term)
			twarn(ErrRuntime_Other, "rules::make", "cyclic Term graph");
	path[depth] = (trule_term *)term;
	if (term->kind == trule_term_parameter && parameter_index(ir, term) < 0)
		twarn(ErrRuntime_Other, "rules::make",
		      "Parameter Term is not in the parameter list");
	if (term->kind == trule_term_extension &&
	    (!tstring_len(term->provider) || !tstring_len(term->provider_kind)))
		twarn(ErrRuntime_Other, "rules::make", "invalid Extension Term");
	for (uint_objs i = 0; i < term->arguments.len; i++)
		validate_term(ir, term_is(&term->arguments.data[i], "rules::make"),
			path, depth + 1);
}

static void rules_make(tobj *params, uint_regs count, tobj *result)
{
	count_is("rules::make", count, 3);
	const char *name = string_is(&params[0], "rules::make");
	tlist *parameters = list_is(&params[1], "rules::make");
	tlist *items = list_is(&params[2], "rules::make");
	trule_ir *ir = trule_ir_new(name, "");
	for (uint_objs i = 0; i < tlist_size(parameters); i++) {
		trule_term *parameter = term_is(tlist_at(parameters, i), "rules::make");
		if (parameter->kind != trule_term_parameter)
			twarn(ErrRuntime_ParamsType, "rules::make", "Parameter Terms required");
		if (parameter_index(ir, parameter) >= 0)
			twarn(ErrRuntime_Other, "rules::make", "duplicate Parameter Term");
		trule_ir_add_parameter(ir, parameter);
	}
	trule_term *path[256];
	for (uint_objs i = 0; i < tlist_size(items); i++) {
		trule_item *item = item_is(tlist_at(items, i), "rules::make");
		if (item->kind == trule_item_condition) {
			if (!ttypeval_equal(item->term->type,
				ttypeval_builtin(tbuiltin_bool)))
				twarn(ErrRuntime_ParamsType, "rules::make", "Bool Term required");
			validate_term(ir, item->term, path, 0);
		} else {
			validate_term(ir, item->rule, path, 0);
			for (uint_objs j = 0; j < item->arguments.len; j++)
				validate_term(ir, term_is(&item->arguments.data[j],
					"rules::make"), path, 0);
			if (item->rule->kind == trule_term_constant &&
			    item->rule->payload.type == tcompo &&
			    tobj_compo_type(&item->rule->payload) == compo_trule) {
				trule *required =
					(trule *)item->rule->payload.val.v_tcompo;
				if (required->ir->parameters.len != item->arguments.len)
					twarn(ErrRuntime_ParamsCtr, "rules::make",
					      "Requirement argument count does not match Rule");
				for (uint_objs j = 0; j < item->arguments.len; j++) {
					trule_term *argument = (trule_term *)
						item->arguments.data[j].val.v_tcompo;
					trule_term *parameter = (trule_term *)
						required->ir->parameters.data[j].val.v_tcompo;
					if (!ttypeval_equal(argument->type, parameter->type) &&
					    parameter->type->kind != ttype_kind_any)
						twarn(ErrRuntime_ParamsType, "rules::make",
						      "Requirement argument Type mismatch");
				}
			}
		}
		trule_ir_add_item(ir, item);
	}
	tstring *signature = tstring_new_empty();
	for (uint_objs i = 0; i < ir->parameters.len; i++) {
		trule_term *parameter = (trule_term *)ir->parameters.data[i].val.v_tcompo;
		if (i) tstring_append_c(signature, '\x1f');
		tstring_append_ts(signature, parameter->type->canonical);
	}
	tobj_set_compo(result, (tcompo_v *)trule_new_dynamic(
		ir, tstring_cstr(signature)));
	tstring_free(signature);
	if (ir->base.refctr > 0) ir->base.refctr--;
}

static const textension_symbol symbols[] = {
	{
		.name = "RuleIR",
		.type = "Type",
		.detail = "rules::RuleIR: Type",
		.kind = textension_type,
		.value_factory = create_rule_ir
	},
	{
		.name = "Parameter",
		.type = "Type",
		.detail = "rules::Parameter: Type",
		.kind = textension_type,
		.value_factory = create_parameter_type
	},
	{
		.name = "Capture",
		.type = "Type",
		.detail = "rules::Capture: Type",
		.kind = textension_type,
		.value_factory = create_capture_type
	},
	{
		.name = "Item",
		.type = "Type",
		.detail = "rules::Item: Type",
		.kind = textension_type,
		.value_factory = create_item_type
	},
	{
		.name = "Condition",
		.type = "Type",
		.detail = "rules::Condition: Type",
		.kind = textension_type,
		.value_factory = create_condition_type
	},
	{
		.name = "Requirement",
		.type = "Type",
		.detail = "rules::Requirement: Type",
		.kind = textension_type,
		.value_factory = create_requirement_type
	},
	{
		.name = "Term",
		.type = "Type",
		.detail = "rules::Term: Type",
		.kind = textension_type,
		.value_factory = create_term_type
	},
	{
		.name = "Origin",
		.type = "Type",
		.detail = "rules::Origin: Type",
		.kind = textension_type,
		.value_factory = create_origin_type
	},
	{
		.name = "CheckResult",
		.type = "Type",
		.detail = "rules::CheckResult: Type",
		.kind = textension_type,
		.value_factory = create_check_result_type
	},
	{
		.name = "Violation",
		.type = "Type",
		.detail = "rules::Violation: Type",
		.kind = textension_type,
		.value_factory = create_violation_type
	},
	{
		.name = "Diagnostic",
		.type = "Type",
		.detail = "rules::Diagnostic: Type",
		.kind = textension_type,
		.value_factory = create_diagnostic_type
	},
	{
		.name = "term",
		.type = "Function[Type] -> Type",
		.detail = "rules::term(type: Type) -> Type",
		.kind = textension_function,
		.function = rules_term,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	},
	{
		.name = "check",
		.type = "Function[RuleInstance | Rule] -> CheckResult",
		.detail = "rules::check(rule: RuleInstance | Rule) -> CheckResult",
		.kind = textension_value,
		.value_factory = create_check
	},
	{
		.name = "inspect",
		.type = "Function[RuleInstance | Rule] -> RuleIR",
		.detail = "rules::inspect(rule: RuleInstance | Rule) -> RuleIR",
		.kind = textension_value,
		.value_factory = create_inspect
	},
	{
		.name = "parameters",
		.type = "Function[RuleIR] -> List",
		.detail = "rules::parameters(ir: RuleIR) -> List[Parameter]",
		.kind = textension_value,
		.value_factory = create_parameters
	},
	{
		.name = "items",
		.type = "Function[RuleIR] -> List",
		.detail = "rules::items(ir: RuleIR) -> List[Item]",
		.kind = textension_value,
		.value_factory = create_items
	},
	{
		.name = "terms",
		.type = "Function[RuleIR] -> List",
		.detail = "rules::terms(ir: RuleIR) -> List[Term]",
		.kind = textension_value,
		.value_factory = create_terms
	},
	{
		.name = "origin",
		.type = "Function[AnyType] -> Origin",
		.detail = "rules::origin(value: AnyType) -> Origin",
		.kind = textension_value,
		.value_factory = create_origin
	},
	{
		.name = "semantic_hash",
		.type = "Function[RuleIR | Rule] -> Int",
		.detail = "rules::semantic_hash(value: RuleIR | Rule) -> Int",
		.kind = textension_value,
		.value_factory = create_semantic_hash
	},
	{
		.name = "content_hash",
		.type = "Function[RuleIR | Rule] -> Int",
		.detail = "rules::content_hash(value: RuleIR | Rule) -> Int",
		.kind = textension_value,
		.value_factory = create_content_hash
	},
	{
		.name = "serialize",
		.type = "Function[RuleIR | Rule] -> String",
		.detail = "rules::serialize(value: RuleIR | Rule) -> String",
		.kind = textension_value,
		.value_factory = create_serialize
	},
	{
		.name = "deserialize",
		.type = "Function[String] -> Rule",
		.detail = "rules::deserialize(data: String) -> Rule",
		.kind = textension_value,
		.value_factory = create_deserialize
	},
	{
		.name = "parameter",
		.type = "Function[String, Type] -> Parameter",
		.detail = "rules::parameter(name: String, type: Type) -> Parameter",
		.kind = textension_function,
		.function = rules_parameter,
		.minimum_arguments = 2,
		.maximum_arguments = 2
	},
	{
		.name = "constant",
		.type = "Function[AnyType] -> Term",
		.detail = "rules::constant(value: AnyType) -> Term",
		.kind = textension_function,
		.function = rules_constant,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	},
	{
		.name = "call",
		.type = "Function[AnyType, List] -> Term",
		.detail = "rules::call(function: AnyType, arguments: List[Term]) -> Term",
		.kind = textension_function,
		.function = rules_call,
		.minimum_arguments = 2,
		.maximum_arguments = 2
	},
	{
		.name = "condition",
		.type = "Function[...] -> Condition",
		.detail = "rules::condition(term: Term, description: String?) -> Condition",
		.kind = textension_function,
		.function = rules_condition,
		.minimum_arguments = 1,
		.maximum_arguments = 2
	},
	{
		.name = "requirement",
		.type = "Function[AnyType, List] -> Requirement",
		.detail = "rules::requirement(rule: Rule | Term, arguments: List[Term]) -> Requirement",
		.kind = textension_function,
		.function = rules_requirement,
		.minimum_arguments = 2,
		.maximum_arguments = 2
	},
	{
		.name = "extension",
		.type = "Function[String, String, List, AnyType] -> Term",
		.detail = "rules::extension(provider: String, kind: String, arguments: List[Term], payload: AnyType) -> Term",
		.kind = textension_function,
		.function = rules_extension,
		.minimum_arguments = 4,
		.maximum_arguments = 4
	},
	{
		.name = "make",
		.type = "Function[String, List, List] -> Rule",
		.detail = "rules::make(display_name: String, parameters: List[Parameter], items: List[Item]) -> Rule",
		.kind = textension_function,
		.function = rules_make,
		.minimum_arguments = 3,
		.maximum_arguments = 3
	}
};

const textension_module tstdlib_rules_module = {
	.scope = textension_package,
	.name = "rules",
	.detail = "Immutable Rule IR construction, inspection and checking",
	.symbols = symbols,
	.symbol_count = sizeof(symbols) / sizeof(symbols[0])
};
