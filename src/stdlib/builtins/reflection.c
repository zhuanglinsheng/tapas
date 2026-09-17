#include "tapas/textension.h"

#include "../arguments.h"

#include "tapas/objects/tlist.h"
#include "tapas/objects/tpair.h"
#include "tapas/objects/trule.h"
#include "tapas/objects/trule_ir.h"

static void builtin_parameters(tobj *params, uint_regs count, tobj *result)
{
	tstdlib_require_arguments("parameters", count, 1);
	const tobj *value = &params[0];
	trule_ir *ir = nullptr;
	if (value->type == tcompo) {
		if (tobj_compo_type(value) == compo_trule_instance)
			value = &((trule_instance *)value->val.v_tcompo)->rule;
		if (tobj_compo_type(value) == compo_trule)
			ir = ((trule *)value->val.v_tcompo)->ir;
		else if (tobj_compo_type(value) == compo_trule_ir)
			ir = (trule_ir *)value->val.v_tcompo;
	}
	if (!ir)
		twarn(ErrRuntime_ParamsType, "parameters",
		      "Rule, RuleInstance or RuleIR required");
	tlist *list = tlist_new();
	for (uint_objs i = 0; i < ir->parameters.len; i++) {
		const trule_term *parameter =
			(trule_term *)ir->parameters.data[i].val.v_tcompo;
		tobj type, entry;
		tobj_set_nil(&type);
		tobj_set_nil(&entry);
		tobj_set_compo(&type, (tcompo_v *)parameter->type);
		tobj_set_compo(&entry, (tcompo_v *)tpair_new(&parameter->payload, &type));
		tobj_vec_push(&list->items, &entry);
		tobj_try_clear(&entry);
		tobj_try_clear(&type);
	}
	tobj_set_compo(result, (tcompo_v *)list);
}

static void builtin_arguments(tobj *params, uint_regs count, tobj *result)
{
	tstdlib_require_arguments("arguments", count, 1);
	if (params[0].type != tcompo ||
	    tobj_compo_type(&params[0]) != compo_trule_instance)
		twarn(ErrRuntime_ParamsType, "arguments",
		      "bound arguments unavailable; RuleInstance required");
	const trule_instance *instance =
		(trule_instance *)params[0].val.v_tcompo;
	tlist *list = tlist_new();
	/* Copy the binding container, not the bound values' object graphs. */
	for (uint_objs i = 0; i < instance->arguments.len; i++)
		tobj_vec_push(&list->items, &instance->arguments.data[i]);
	tobj_set_compo(result, (tcompo_v *)list);
}

static const textension_symbol symbols[] = {
	{
		.name = "parameters",
		.type = "Function[Rule | RuleInstance | RuleIR] -> List[Pair[String, Type]]",
		.detail = "parameters(value: Rule | RuleInstance | RuleIR) -> List[Pair[String, Type]]",
		.kind = textension_function,
		.function = builtin_parameters,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	},
	{
		.name = "arguments",
		.type = "Function[RuleInstance] -> List[AnyType]",
		.detail = "arguments(instance: RuleInstance) -> List[AnyType]",
		.kind = textension_function,
		.function = builtin_arguments,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	}
};

const textension_module tstdlib_reflection_functions = {
	.scope = textension_root,
	.detail = "parameter declarations and bound argument reflection",
	.symbols = symbols,
	.symbol_count = sizeof(symbols) / sizeof(symbols[0])
};
