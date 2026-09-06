/**
 * @file tbuiltintype.c
 * @brief Defines canonical language names for core builtin Type identities.
 * @details Canonical spellings are kept here rather than in standard-library
 * module descriptors because core type parsing, formatting, serialization,
 * and tooling must share them. Designated initializers make each enum-to-name
 * relationship explicit.
 * @note This file contains language-core Types only; package-owned Types never
 * belong here. A standard package may re-export a core Type, but a C
 * implementation or VM dependency does not transfer ownership to the core.
 * Append new core IDs before tbuiltintype_count without reordering existing IDs,
 * and add the corresponding canonical name; the static assertion detects a
 * missing name when the enum is extended at the end.
 */
#include "tapas/basic_defs/tbuiltintype.h"

#include <stddef.h>

static const char *const builtin_names[] = {
	[tbuiltintype_any] = "AnyType",
	[tbuiltintype_nil] = "Nil",
	[tbuiltintype_bool] = "Bool",
	[tbuiltintype_int] = "Int",
	[tbuiltintype_float] = "Float",
	[tbuiltintype_string] = "String",
	[tbuiltintype_list] = "List",
	[tbuiltintype_pair] = "Pair",
	[tbuiltintype_dictionary] = "Dictionary",
	[tbuiltintype_iterator] = "Iterator",
	[tbuiltintype_function] = "Function",
	[tbuiltintype_library] = "Library",
	[tbuiltintype_real_array] = "RealArray",
	[tbuiltintype_bool_array] = "BoolArray",
	[tbuiltintype_time] = "Time",
	[tbuiltintype_type] = "Type",
	[tbuiltintype_indexable] = "Indexable",
	[tbuiltintype_index_settable] = "IndexSettable",
	[tbuiltintype_appendable] = "Appendable",
	[tbuiltintype_deletable] = "Deletable",
	[tbuiltintype_contains] = "Contains",
	[tbuiltintype_iterable] = "Iterable",
	[tbuiltintype_rule] = "Rule",
	[tbuiltintype_rule_instance] = "RuleInstance",
	[tbuiltintype_rule_ir] = "RuleIR",
	[tbuiltintype_rule_parameter] = "Parameter",
	[tbuiltintype_rule_capture] = "Capture",
	[tbuiltintype_rule_item] = "RuleItem",
	[tbuiltintype_rule_condition] = "Condition",
	[tbuiltintype_rule_requirement] = "Requirement",
	[tbuiltintype_rule_term] = "RuleTerm"
};

static_assert(sizeof(builtin_names) / sizeof(*builtin_names) == tbuiltintype_count,
	"every tbuiltintype_id must have a canonical name");

const char *tbuiltintype_name(tbuiltintype_id builtin)
{
	return builtin >= 0 && builtin < tbuiltintype_count ?
		builtin_names[builtin] : nullptr;
}
