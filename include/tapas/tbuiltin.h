#ifndef TAPAS_TBUILTIN_H
#define TAPAS_TBUILTIN_H

typedef enum {
	tbuiltin_any,
	tbuiltin_nil,
	tbuiltin_bool,
	tbuiltin_int,
	tbuiltin_float,
	tbuiltin_string,
	tbuiltin_list,
	tbuiltin_pair,
	tbuiltin_dictionary,
	tbuiltin_iterator,
	tbuiltin_function,
	tbuiltin_library,
	tbuiltin_real_array,
	tbuiltin_bool_array,
	tbuiltin_time,
	tbuiltin_type,
	tbuiltin_indexable,
	tbuiltin_index_settable,
	tbuiltin_appendable,
	tbuiltin_deletable,
	tbuiltin_contains,
	tbuiltin_iterable,
	tbuiltin_rule,
	tbuiltin_rule_instance,
	tbuiltin_rule_ir,
	tbuiltin_rule_parameter,
	tbuiltin_rule_capture,
	tbuiltin_rule_item,
	tbuiltin_rule_condition,
	tbuiltin_rule_requirement,
	tbuiltin_rule_term,
	tbuiltin_rule_origin,
	tbuiltin_rule_check_result,
	tbuiltin_rule_violation,
	tbuiltin_rule_diagnostic,
	tbuiltin_evaluator,
	tbuiltin_evaluator_context,
	tbuiltin_evaluator_result,
	tbuiltin_evaluator_diagnostic,
	tbuiltin_points,
	tbuiltin_range,
	tbuiltin_count
} tbuiltin_id;

const char *tbuiltin_name(tbuiltin_id type);

#endif
