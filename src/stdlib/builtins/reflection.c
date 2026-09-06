#include "tapas/textension.h"
#include "../arguments.h"
#include "tapas/runtime/trule.h"
#include "tapas/runtime/tlist.h"
#include "tapas/runtime/tpair.h"
#include "tapas/runtime/tstr.h"
#include "tapas/runtime/tcfn.h"
#include "../function_metadata.h"
#include <string.h>

static void append_parameter(tlist *list, const char *name, size_t length, ttypeval *type)
{
	tobj name_value, type_value, entry;
	tobj_set_nil(&name_value);
	tobj_set_nil(&type_value);
	tobj_set_nil(&entry);
	tstring *text = tstring_new_len(name, length);
	tobj_set_compo(&name_value, (tcompo_v *)tstr_new(tstring_cstr(text)));
	tstring_free(text);
	tobj_set_compo(&type_value, (tcompo_v *)type);
	tobj_set_compo(&entry, (tcompo_v *)tpair_new(&name_value, &type_value));
	tobj_vec_push(&list->items, &entry);
	tobj_try_clear(&entry);
	tobj_try_clear(&type_value);
	tobj_try_clear(&name_value);
}

static int function_parameters(const tobj *value, tobj *result)
{
    tcompo_type kind = tobj_compo_type(value);
    if (kind != compo_tfunc && kind != compo_cppfunc &&
        kind != compo_sessfunc && kind != compo_trule_builtin) return 0;
    const tfunction_metadata *metadata = kind == compo_tfunc ?
        ((tfunc *)value->val.v_tcompo)->metadata : tstdlib_function_metadata(value);
    if (!metadata)
        twarn(ErrRuntime_ParamsType, "parameters",
            "Function metadata unavailable; recompile source or register native metadata");
    tlist *list = tlist_new();
    for (uint32_t i = 0; i < tfunction_metadata_count(metadata); i++) {
        const char *name = tfunction_metadata_name(metadata, i);
        append_parameter(list, name, strlen(name), tfunction_metadata_type(metadata, i));
    }
    tobj_set_compo(result, (tcompo_v *)list);
    return 1;
}

static void builtin_parameters(tobj *params, uint_regs count, tobj *result)
{
	tstdlib_require_arguments("parameters", count, 1);
	const tobj *value = &params[0];
	if (value->type == tcompo && function_parameters(value, result)) return;
	trule_ir *ir = nullptr;
	if (value->type == tcompo) {
		if (tobj_compo_type(value) == compo_trule_instance)
			value = &((trule_instance *)value->val.v_tcompo)->rule;
		if (tobj_compo_type(value) == compo_trule)
			ir = ((trule *)value->val.v_tcompo)->ir;
		else if (tobj_compo_type(value) == compo_trule_ir)
			ir = (trule_ir *)value->val.v_tcompo;
	}
	if (!ir) twarn(ErrRuntime_ParamsType, "parameters",
		"parameter metadata unavailable; Function, Rule, RuleInstance or RuleIR required");
	tlist *list = tlist_new();
	for (uint_objs i = 0; i < ir->parameters.len; i++) {
		const trule_term *parameter = (trule_term *)ir->parameters.data[i].val.v_tcompo;
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
	if (params[0].type != tcompo || tobj_compo_type(&params[0]) != compo_trule_instance)
		twarn(ErrRuntime_ParamsType, "arguments", "bound arguments unavailable; RuleInstance required");
	const trule_instance *instance = (trule_instance *)params[0].val.v_tcompo;
	tlist *list = tlist_new();
	/* Copy the binding container, not the bound values' object graphs. */
	for (uint_objs i = 0; i < instance->arguments.len; i++)
		tobj_vec_push(&list->items, &instance->arguments.data[i]);
	tobj_set_compo(result, (tcompo_v *)list);
}

static const textension_symbol symbols[] = {
	{
		.name = "parameters",
		.type = "Function[Function | Rule | RuleInstance | RuleIR] -> List[Pair[String, Type]]",
		.detail = "parameters(value: Function | Rule | RuleInstance | RuleIR) -> List[Pair[String, Type]]",
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
