#include "tapas/textension.h"

#include "tapas/runtime/tevaluator.h"
#include "tapas/runtime/trule.h"
#include "tapas/runtime/tstr.h"
#include "tapas/runtime/ttype.h"


static void create_evaluator_type(tobj *result)
{
	tobj_set_compo(result,
		(tcompo_v *)ttypeval_builtin(tbuiltin_evaluator));
}

static void create_context_type(tobj *result)
{
	tobj_set_compo(result,
		(tcompo_v *)ttypeval_builtin(tbuiltin_evaluator_context));
}

static void create_result_type(tobj *result)
{
	tobj_set_compo(result,
		(tcompo_v *)ttypeval_builtin(tbuiltin_evaluator_result));
}

static void create_diagnostic_type(tobj *result)
{
	tobj_set_compo(result,
		(tcompo_v *)ttypeval_builtin(tbuiltin_evaluator_diagnostic));
}

static const char *string_value(const tobj *value, const char *name)
{
	if (!value || value->type != tcompo ||
	    tobj_compo_type(value) != compo_tstr)
		twarn(ErrRuntime_ParamsType, name, "String required");
	return tstring_cstr(((tstr *)value->val.v_tcompo)->data);
}

static int callable(const tobj *value)
{
	if (!value || value->type != tcompo || !value->val.v_tcompo)
		return 0;
	tcompo_type type = tobj_compo_type(value);
	return type == compo_tfunc || type == compo_cppfunc ||
		type == compo_sessfunc;
}

static void evaluators_make(tobj *params, uint_regs len, tobj *result)
{
	if (len < 3 || len > 4)
		twarn(ErrRuntime_ParamsCtr, "evaluators::make",
		      "three or four arguments required");
	if (!callable(&params[2]) || (len == 4 && params[3].type != tnil &&
	    !callable(&params[3])))
		twarn(ErrRuntime_ParamsType, "evaluators::make",
		      "evaluate and compile must be Functions");
	if (params[1].type != tint)
		twarn(ErrRuntime_ParamsType, "evaluators::make",
		      "version must be Int");
	tobj_set_compo(result, (tcompo_v *)tevaluator_new(
		string_value(&params[0], "evaluators::make"),
		params[1].val.v_tint,
		&params[2], len == 4 && params[3].type != tnil ? &params[3] : nullptr));
}

static void create_eval(tobj *result)
{
	tobj_set_compo(result,
		(tcompo_v *)trule_builtin_new(trule_builtin_evaluate));
}

static void create_compile(tobj *result)
{
	tobj_set_compo(result,
		(tcompo_v *)trule_builtin_new(trule_builtin_compile));
}

static void create_binding(tobj *result)
{
	tobj_set_compo(result,
		(tcompo_v *)trule_builtin_new(trule_builtin_context_binding));
}

static void create_capture(tobj *result)
{
	tobj_set_compo(result,
		(tcompo_v *)trule_builtin_new(trule_builtin_context_capture));
}

static void create_value(tobj *result)
{
	tobj_set_compo(result,
		(tcompo_v *)trule_builtin_new(trule_builtin_context_value));
}

static void create_requirement(tobj *result)
{
	tobj_set_compo(result,
		(tcompo_v *)trule_builtin_new(trule_builtin_context_requirement));
}

static const textension_symbol symbols[] = {
	{
		.name = "Evaluator",
		.type = "Type",
		.detail = "evaluators::Evaluator: Type",
		.kind = textension_type,
		.value_factory = create_evaluator_type
	},
	{
		.name = "Context",
		.type = "Type",
		.detail = "evaluators::Context: Type",
		.kind = textension_type,
		.value_factory = create_context_type
	},
	{
		.name = "Result",
		.type = "Type",
		.detail = "evaluators::Result: Type",
		.kind = textension_type,
		.value_factory = create_result_type
	},
	{
		.name = "Diagnostic",
		.type = "Type",
		.detail = "evaluators::Diagnostic: Type",
		.kind = textension_type,
		.value_factory = create_diagnostic_type
	},
	{
		.name = "make",
		.type = "Function[...] -> Evaluator",
		.detail = "evaluators::make(name: String, version: Int, evaluate: Function, compile: Function?) -> Evaluator",
		.kind = textension_function,
		.function = evaluators_make,
		.minimum_arguments = 3,
		.maximum_arguments = 4
	},
	{
		.name = "eval",
		.type = "Function[RuleInstance, Evaluator] -> AnyType",
		.detail = "evaluators::eval(instance: RuleInstance, evaluator: Evaluator) -> Result",
		.kind = textension_value,
		.value_factory = create_eval
	},
	{
		.name = "compile",
		.type = "Function[Rule, Evaluator] -> Result",
		.detail = "evaluators::compile(rule: Rule, evaluator: Evaluator) -> Result",
		.kind = textension_value,
		.value_factory = create_compile
	},
	{
		.name = "binding",
		.type = "Function[Context, AnyType] -> AnyType",
		.detail = "evaluators::binding(context: Context, parameter: Parameter | Int) -> AnyType",
		.kind = textension_value,
		.value_factory = create_binding
	},
	{
		.name = "capture",
		.type = "Function[Context, AnyType] -> AnyType",
		.detail = "evaluators::capture(context: Context, capture: Capture | Int) -> AnyType",
		.kind = textension_value,
		.value_factory = create_capture
	},
	{
		.name = "value",
		.type = "Function[Context, Term] -> AnyType",
		.detail = "evaluators::value(context: Context, term: Term) -> AnyType",
		.kind = textension_value,
		.value_factory = create_value
	},
	{
		.name = "requirement",
		.type = "Function[Context, Requirement] -> RuleInstance",
		.detail = "evaluators::requirement(context: Context, requirement: Requirement) -> RuleInstance",
		.kind = textension_value,
		.value_factory = create_requirement
	}
};

const textension_module tstdlib_evaluators_module = {
	.scope = textension_package,
	.name = "evaluators",
	.detail = "Custom Rule evaluator adapters",
	.symbols = symbols,
	.symbol_count = sizeof(symbols) / sizeof(symbols[0])
};
