#include "tapas/tstdlib.h"

#include "modules.h"

static const textension_module *const modules[] = {
	&tstdlib_console_functions,
	&tstdlib_conversion_functions,
	&tstdlib_list_functions,
	&tstdlib_array_functions,
	&tstdlib_pair_functions,
	&tstdlib_capability_functions,
	&tstdlib_iterator_functions,
	&tstdlib_dict_functions,
	&tstdlib_sort_functions,
	&tstdlib_object_functions,
	&tstdlib_reflection_functions,
	&tstdlib_time_functions,
	&tstdlib_session_functions,
	&tstdlib_rule_functions,
	&tstdlib_dense_module,
	&tstdlib_io_module,
	&tstdlib_syntax_module,
	&tstdlib_time_module,
	&tstdlib_random_module,
	&tstdlib_finite_module,
	&tstdlib_math_module,
	&tstdlib_types_module,
	&tstdlib_rules_module,
	&tstdlib_evaluators_module,
	&tstdlib_solve_module
};

const textension_descriptor *tstdlib_descriptor(void)
{
	static const textension_descriptor descriptor =
		TAPAS_EXTENSION("stdlib", "0.1", modules);
	return &descriptor;
}
