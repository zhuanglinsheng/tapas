#include "tapas/textension.h"

#include "tapas/runtime/trule.h"

static void create_assert(tobj *result)
{
	tobj_set_compo(result,
		(tcompo_v *)trule_builtin_new(trule_builtin_assert));
}

static const textension_symbol symbols[] = {
	{
		.name = "assert",
		.type = "Function[RuleInstance | Rule] -> Nil",
		.detail = "assert(rule: RuleInstance | Rule) -> Nil",
		.kind = textension_value,
		.value_factory = create_assert
	}
};

const textension_module tstdlib_rule_functions = {
	.scope = textension_root,
	.detail = "core Rule assertion",
	.symbols = symbols,
	.symbol_count = sizeof(symbols) / sizeof(symbols[0])
};
