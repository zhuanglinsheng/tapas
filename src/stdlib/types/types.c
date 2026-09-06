#include "tapas/textension.h"
#include "tapas/runtime/tdomain.h"

#include "tapas/runtime/tdict.h"
#include "tapas/runtime/tlist.h"
#include "tapas/runtime/tpair.h"
#include "tapas/runtime/tstr.h"
#include "tapas/runtime/ttype.h"
#include "tapas/runtime/trule_ir.h"
#include "tapas/runtime/trule.h"

#include <stdlib.h>


static void require_count(const char *name, uint_regs actual, uint_regs expected)
{
	if (actual != expected)
		twarn(ErrRuntime_ParamsCtr, name, "incorrect parameter count");
}

static ttypeval *require_type(const tobj *value, const char *name)
{
	if (!value || value->type != tcompo ||
	    tobj_compo_type(value) != compo_ttypeval)
		twarn(ErrRuntime_ParamsType, name, "Type value required");
	return (ttypeval *)value->val.v_tcompo;
}

static void return_type(tobj *result, ttypeval *type)
{
	tobj_set_compo(result, (tcompo_v *)type);
}

static void types_make_type(tobj *params, uint_regs len, tobj *result)
{
	if (len == 0)
		twarn(ErrRuntime_ParamsCtr, "types::make_type",
		      "at least one field is required");
	ttype_field *fields = (ttype_field *)calloc(len, sizeof(ttype_field));
	tstring **owned_names = (tstring **)calloc(len, sizeof(*owned_names));
	if (!fields || !owned_names)
		twarn(ErrRuntime_Other, "types::make_type", "out of memory");
	for (uint_regs i = 0; i < len; i++) {
		if (params[i].type != tcompo ||
		    tobj_compo_type(&params[i]) != compo_tpair)
			twarn(ErrRuntime_ParamsType, "types::make_type",
			      "field Pair required");
		tpair *pair = (tpair *)params[i].val.v_tcompo;
		if (pair->first.type != tcompo ||
		    tobj_compo_type(&pair->first) != compo_tstr)
			twarn(ErrRuntime_ParamsType, "types::make_type",
			      "field name must be String");
		const tstring *encoded = ((tstr *)pair->first.val.v_tcompo)->data;
		const char *name = tstring_cstr(encoded);
		if (strncmp(name, "@optional/", 10) == 0) {
			owned_names[i] = tstring_new(name + 10);
			fields[i].name = owned_names[i];
			fields[i].optional = 1;
		} else
			fields[i].name = encoded;
		fields[i].type = require_type(&pair->second, "types::make_type");
	}
	ttypeval *type = ttypeval_new_fields(fields, len);
	for (uint_regs i = 0; i < len; i++) tstring_free(owned_names[i]);
	free(owned_names);
	free(fields);
	return_type(result, type);
}

static void types_union(tobj *params, uint_regs len, tobj *result)
{
	if (len < 2)
		twarn(ErrRuntime_ParamsCtr, "types::union",
		      "at least two Type values are required");
	ttypeval **members = (ttypeval **)calloc(len, sizeof(ttypeval *));
	if (!members)
		twarn(ErrRuntime_Other, "types::union", "out of memory");
	for (uint_regs i = 0; i < len; i++)
		members[i] = require_type(&params[i], "types::union");
	ttypeval *type = ttypeval_new_union(members, len);
	free(members);
	return_type(result, type);
}

static void types_enum(tobj *params, uint_regs len, tobj *result)
{
	if (len == 0)
		twarn(ErrRuntime_ParamsCtr, "types::enum",
		      "at least one String member is required");
	const tstring **members = (const tstring **)calloc(len, sizeof(*members));
	if (!members)
		twarn(ErrRuntime_Other, "types::enum", "out of memory");
	for (uint_regs i = 0; i < len; i++) {
		if (params[i].type != tcompo ||
		    tobj_compo_type(&params[i]) != compo_tstr)
			twarn(ErrRuntime_ParamsType, "types::enum",
			      "String member required");
		members[i] = ((const tstr *)params[i].val.v_tcompo)->data;
	}
	ttypeval *type = ttypeval_new_enum(members, len);
	free(members);
	return_type(result, type);
}

static void types_list(tobj *params, uint_regs len, tobj *result)
{
	require_count("types::list", len, 1);
	return_type(result,
		    ttypeval_new_list(require_type(&params[0], "types::list")));
}

static void types_iterator(tobj *params, uint_regs len, tobj *result)
{
	require_count("types::iterator", len, 1);
	return_type(result, ttypeval_new_iterator(
		require_type(&params[0], "types::iterator")));
}

static void types_optional(tobj *params, uint_regs len, tobj *result)
{
	require_count("types::optional", len, 1);
	if (params[0].type != tcompo || tobj_compo_type(&params[0]) != compo_tpair)
		twarn(ErrRuntime_ParamsType, "types::optional", "field Pair required");
	tpair *field = (tpair *)params[0].val.v_tcompo;
	if (field->first.type != tcompo ||
	    tobj_compo_type(&field->first) != compo_tstr)
		twarn(ErrRuntime_ParamsType, "types::optional",
		      "field name must be String");
	tstring *encoded = tstring_new("@optional/");
	tstring_append_ts(encoded, ((tstr *)field->first.val.v_tcompo)->data);
	tobj name;
	tobj_set_nil(&name);
	tobj_set_compo(&name, (tcompo_v *)tstr_new(tstring_cstr(encoded)));
	tstring_free(encoded);
	tobj_set_compo(result, (tcompo_v *)tpair_new(&name, &field->second));
	tobj_try_clear(&name);
}

static void types_pair(tobj *params, uint_regs len, tobj *result)
{
	require_count("types::pair", len, 2);
	return_type(result,
		    ttypeval_new_pair(require_type(&params[0], "types::pair"),
				      require_type(&params[1], "types::pair")));
}

static void types_dictionary(tobj *params, uint_regs len, tobj *result)
{
	require_count("types::dictionary", len, 2);
	return_type(result,
		    ttypeval_new_dictionary(
			    require_type(&params[0], "types::dictionary"),
			    require_type(&params[1], "types::dictionary")));
}

static void types_rule_type(tobj *params, uint_regs len, tobj *result)
{
	ttypeval **parameters = len ?
		(ttypeval **)calloc(len, sizeof(*parameters)) : nullptr;
	if (len && !parameters)
		twarn(ErrRuntime_Other, "types::rule", "out of memory");
	for (uint_regs i = 0; i < len; i++)
		parameters[i] = require_type(&params[i], "types::rule");
	return_type(result, ttypeval_new_rule(parameters, len));
	free(parameters);
}

static void types_rule_instance_type(tobj *params, uint_regs len, tobj *result)
{
	ttypeval **parameters = len ?
		(ttypeval **)calloc(len, sizeof(*parameters)) : nullptr;
	if (len && !parameters)
		twarn(ErrRuntime_Other, "types::rule_instance", "out of memory");
	for (uint_regs i = 0; i < len; i++)
		parameters[i] = require_type(&params[i], "types::rule_instance");
	return_type(result, ttypeval_new_rule_instance(parameters, len));
	free(parameters);
}

static ttypeval *type_of_value(const tobj *value)
{
	switch (value->type) {
	case tnil:
		return ttypeval_builtin(tbuiltin_nil);
	case tbool:
		return ttypeval_builtin(tbuiltin_bool);
	case tint:
		return ttypeval_builtin(tbuiltin_int);
	case tfloat:
		return ttypeval_builtin(tbuiltin_float);
	case tcompo:
		break;
	}
	switch (tobj_compo_type(value)) {
	case compo_tstr:
		return ttypeval_builtin(tbuiltin_string);
	case compo_tlist:
		return ttypeval_builtin(tbuiltin_list);
	case compo_tpair:
		return ttypeval_builtin(tbuiltin_pair);
	case compo_tdict:
		return ttypeval_builtin(tbuiltin_dictionary);
	case compo_titer:
		return ttypeval_builtin(tbuiltin_iterator);
	case compo_tfunc:
	case compo_cppfunc:
	case compo_sessfunc:
		return ttypeval_builtin(tbuiltin_function);
	case compo_tlib:
		return ttypeval_builtin(tbuiltin_library);
	case compo_tdarr:
		return ttypeval_builtin(tbuiltin_real_array);
	case compo_tbarr:
		return ttypeval_builtin(tbuiltin_bool_array);
	case compo_time:
		return ttypeval_builtin(tbuiltin_time);
	case compo_ttypeval:
		return ttypeval_builtin(tbuiltin_type);
	case compo_trule:
	case compo_trule_instance: {
		trule *rule = tobj_compo_type(value) == compo_trule ?
			(trule *)value->val.v_tcompo :
			(trule *)((trule_instance *)value->val.v_tcompo)
				->rule.val.v_tcompo;
		uint_objs count = rule->ir ? rule->ir->parameters.len : 0;
		ttypeval **parameters = count ? calloc(count,
			sizeof(*parameters)) : nullptr;
		for (uint_objs i = 0; i < count; i++)
			parameters[i] = ((trule_term *)rule->ir->parameters
				.data[i].val.v_tcompo)->type;
		ttypeval *type = tobj_compo_type(value) == compo_trule ?
			ttypeval_new_rule(parameters, count) :
			ttypeval_new_rule_instance(parameters, count);
		free(parameters);
		return type;
	}
	case compo_trule_ir:
		return ttypeval_builtin(tbuiltin_rule_ir);
	case compo_trule_term: {
		trule_term *term = (trule_term *)value->val.v_tcompo;
		if (term->kind == trule_term_parameter)
			return ttypeval_builtin(tbuiltin_rule_parameter);
		if (term->kind == trule_term_capture)
			return ttypeval_builtin(tbuiltin_rule_capture);
		return ttypeval_builtin(tbuiltin_rule_term);
	}
	case compo_trule_item:
		return ttypeval_builtin(
			((trule_item *)value->val.v_tcompo)->kind ==
			trule_item_condition ? tbuiltin_rule_condition :
			tbuiltin_rule_requirement);
	case compo_tpoints:
	case compo_trange:
		return ttypeval_new_domain(tobj_compo_type(value) == compo_trange, ((tdomain *)value->val.v_tcompo)->item_type);
	case compo_tevaluator:
		return ttypeval_builtin(tbuiltin_evaluator);
	default:
		twarn(ErrRuntime_ParamsType, "types::of", "unsupported value type");
	}
	return nullptr;
}

static void types_of(tobj *params, uint_regs len, tobj *result)
{
	require_count("types::of", len, 1);
	return_type(result, type_of_value(&params[0]));
}

static void types_matches(tobj *params, uint_regs len, tobj *result)
{
	require_count("types::matches", len, 2);
	tobj_set_bool(result,
		      ttypeval_matches(&params[0],
				       require_type(&params[1], "types::matches")));
}

static void copy_definition_entry(const tobj *key, const tobj *value,
				  void *context)
{
	tdict_set((tdict *)context, key, value);
}

static tdict *definition_copy(const ttypeval *type)
{
	tdict *copy = tdict_new();
	thashtbl_each(ttypeval_definition(type), copy_definition_entry, copy);
	return copy;
}

static void types_fields(tobj *params, uint_regs len, tobj *result)
{
	require_count("types::fields", len, 1);
	ttypeval *type = require_type(&params[0], "types::fields");
	tdict *fields = tdict_new();
	for (uint_objs i = 0; i < ttypeval_field_count(type); i++) {
		const tobj *name;
		ttypeval *field;
		ttypeval_field_at(type, i, &name, &field);
		tobj value;
		tobj_set_nil(&value);
		tobj_set_compo(&value, (tcompo_v *)field);
		tdict_set(fields, name, &value);
	}
	tobj_set_compo(result, (tcompo_v *)fields);
}

static void types_members(tobj *params, uint_regs len, tobj *result)
{
	require_count("types::members", len, 1);
	ttypeval *type = require_type(&params[0], "types::members");
	tlist *members = tlist_new();
	if (type->kind == ttype_kind_enum) {
		for (uint_objs i = 0; i < ttypeval_enum_member_count(type); i++) {
			tobj value;
			tobj_set_nil(&value);
			tobj_set_compo(&value, (tcompo_v *)tstr_new(
				tstring_cstr(ttypeval_enum_member_at(type, i))));
			tobj_vec_push(&members->items, &value);
			tobj_try_clear(&value);
		}
		tobj_set_compo(result, (tcompo_v *)members);
		return;
	}
	uint_objs count = ttypeval_member_count(type);
	if (count == 0) {
		tobj_vec_push(&members->items, &params[0]);
	} else {
		for (uint_objs i = 0; i < count; i++) {
			tobj value;
			tobj_set_nil(&value);
			tobj_set_compo(
				&value, (tcompo_v *)ttypeval_member_at(type, i));
			tobj_vec_push(&members->items, &value);
		}
	}
	tobj_set_compo(result, (tcompo_v *)members);
}

static void types_base(tobj *params, uint_regs len, tobj *result)
{
	require_count("types::base", len, 1);
	return_type(result,
		    ttypeval_base(require_type(&params[0], "types::base")));
}

static void types_optional_fields(tobj *params, uint_regs len, tobj *result)
{
	require_count("types::optional_fields", len, 1);
	ttypeval *type = require_type(&params[0], "types::optional_fields");
	tlist *names = tlist_new();
	for (uint_objs i = 0; i < ttypeval_field_count(type); i++) {
		const tobj *name = nullptr;
		ttypeval_field_at(type, i, &name, nullptr);
		if (name && name->type == tcompo &&
		    tobj_compo_type(name) == compo_tstr &&
		    ttypeval_field_optional(type,
			tstring_cstr(((const tstr *)name->val.v_tcompo)->data)))
			tobj_vec_push(&names->items, name);
	}
	tobj_set_compo(result, (tcompo_v *)names);
}

static void set_parameter(tdict *parameters, const char *name, ttypeval *type)
{
	if (!type)
		return;
	tobj key;
	tobj value;
	tobj_set_nil(&key);
	tobj_set_nil(&value);
	tobj_set_compo(&key, (tcompo_v *)tstr_new(name));
	tobj_set_compo(&value, (tcompo_v *)type);
	tdict_set(parameters, &key, &value);
	tobj_try_clear(&key);
}

static void types_parameters(tobj *params, uint_regs len, tobj *result)
{
	require_count("types::parameters", len, 1);
	ttypeval *type = require_type(&params[0], "types::parameters");
	tdict *parameters = tdict_new();
	if (type->kind == ttype_kind_points || type->kind == ttype_kind_range || type->kind == ttype_kind_list || type->kind == ttype_kind_iterator)
		set_parameter(parameters, "item", ttypeval_parameter(type, "item"));
	else if (type->kind == ttype_kind_pair) {
		set_parameter(parameters, "first", ttypeval_parameter(type, "first"));
		set_parameter(parameters, "second", ttypeval_parameter(type, "second"));
	} else if (type->kind == ttype_kind_dictionary) {
		set_parameter(parameters, "key", ttypeval_parameter(type, "key"));
		set_parameter(parameters, "value", ttypeval_parameter(type, "value"));
	}
	tobj_set_compo(result, (tcompo_v *)parameters);
}

static void types_definition(tobj *params, uint_regs len, tobj *result)
{
	require_count("types::definition", len, 1);
	tobj_set_compo(
		result,
		(tcompo_v *)definition_copy(
			require_type(&params[0], "types::definition")));
}


#define DEFINE_TYPE_VALUE(id) \
	static void create_##id(tobj *result) \
	{ \
		tobj_set_compo(result, \
			(tcompo_v *)ttypeval_builtin(tbuiltin_##id)); \
	}

DEFINE_TYPE_VALUE(any)
DEFINE_TYPE_VALUE(nil)
DEFINE_TYPE_VALUE(bool)
DEFINE_TYPE_VALUE(int)
DEFINE_TYPE_VALUE(float)
DEFINE_TYPE_VALUE(string)
DEFINE_TYPE_VALUE(list)
DEFINE_TYPE_VALUE(pair)
DEFINE_TYPE_VALUE(dictionary)
DEFINE_TYPE_VALUE(iterator)
DEFINE_TYPE_VALUE(function)
DEFINE_TYPE_VALUE(library)
DEFINE_TYPE_VALUE(real_array)
DEFINE_TYPE_VALUE(bool_array)
DEFINE_TYPE_VALUE(time)
DEFINE_TYPE_VALUE(type)
DEFINE_TYPE_VALUE(indexable)
DEFINE_TYPE_VALUE(index_settable)
DEFINE_TYPE_VALUE(appendable)
DEFINE_TYPE_VALUE(deletable)
DEFINE_TYPE_VALUE(contains)
DEFINE_TYPE_VALUE(iterable)
DEFINE_TYPE_VALUE(rule)
DEFINE_TYPE_VALUE(rule_instance)

#undef DEFINE_TYPE_VALUE

static const textension_symbol symbols[] = {
	{
		.name = "AnyType",
		.type = "Type",
		.detail = "types::AnyType: Type",
		.kind = textension_type,
		.value_factory = create_any
	},
	{
		.name = "Nil",
		.type = "Type",
		.detail = "types::Nil: Type",
		.kind = textension_type,
		.value_factory = create_nil
	},
	{
		.name = "Bool",
		.type = "Type",
		.detail = "types::Bool: Type",
		.kind = textension_type,
		.value_factory = create_bool
	},
	{
		.name = "Int",
		.type = "Type",
		.detail = "types::Int: Type",
		.kind = textension_type,
		.value_factory = create_int
	},
	{
		.name = "Float",
		.type = "Type",
		.detail = "types::Float: Type",
		.kind = textension_type,
		.value_factory = create_float
	},
	{
		.name = "String",
		.type = "Type",
		.detail = "types::String: Type",
		.kind = textension_type,
		.value_factory = create_string
	},
	{
		.name = "List",
		.type = "Type",
		.detail = "types::List: Type",
		.kind = textension_type,
		.value_factory = create_list
	},
	{
		.name = "Pair",
		.type = "Type",
		.detail = "types::Pair: Type",
		.kind = textension_type,
		.value_factory = create_pair
	},
	{
		.name = "Dictionary",
		.type = "Type",
		.detail = "types::Dictionary: Type",
		.kind = textension_type,
		.value_factory = create_dictionary
	},
	{
		.name = "Iterator",
		.type = "Type",
		.detail = "types::Iterator: Type",
		.kind = textension_type,
		.value_factory = create_iterator
	},
	{
		.name = "Function",
		.type = "Type",
		.detail = "types::Function: Type",
		.kind = textension_type,
		.value_factory = create_function
	},
	{
		.name = "Library",
		.type = "Type",
		.detail = "types::Library: Type",
		.kind = textension_type,
		.value_factory = create_library
	},
	{
		.name = "RealArray",
		.type = "Type",
		.detail = "types::RealArray: Type",
		.kind = textension_type,
		.value_factory = create_real_array
	},
	{
		.name = "BoolArray",
		.type = "Type",
		.detail = "types::BoolArray: Type",
		.kind = textension_type,
		.value_factory = create_bool_array
	},
	{
		.name = "Time",
		.type = "Type",
		.detail = "types::Time: Type",
		.kind = textension_type,
		.value_factory = create_time
	},
	{
		.name = "Type",
		.type = "Type",
		.detail = "types::Type: Type",
		.kind = textension_type,
		.value_factory = create_type
	},
	{
		.name = "Indexable",
		.type = "Type",
		.detail = "types::Indexable: Type",
		.kind = textension_type,
		.value_factory = create_indexable
	},
	{
		.name = "Appendable",
		.type = "Type",
		.detail = "types::Appendable: Type",
		.kind = textension_type,
		.value_factory = create_appendable
	},
	{
		.name = "IndexSettable",
		.type = "Type",
		.detail = "types::IndexSettable: Type",
		.kind = textension_type,
		.value_factory = create_index_settable
	},
	{
		.name = "Deletable",
		.type = "Type",
		.detail = "types::Deletable: Type",
		.kind = textension_type,
		.value_factory = create_deletable
	},
	{
		.name = "Contains",
		.type = "Type",
		.detail = "types::Contains: Type",
		.kind = textension_type,
		.value_factory = create_contains
	},
	{
		.name = "Iterable",
		.type = "Type",
		.detail = "types::Iterable: Type",
		.kind = textension_type,
		.value_factory = create_iterable
	},
	{
		.name = "Rule",
		.type = "Type",
		.detail = "types::Rule: Type",
		.kind = textension_type,
		.value_factory = create_rule
	},
	{
		.name = "RuleInstance",
		.type = "Type",
		.detail = "types::RuleInstance: Type",
		.kind = textension_type,
		.value_factory = create_rule_instance
	},
	{
		.name = "make_type",
		.type = "Function[...] -> Type",
		.detail = "types::make_type(...fields: Pair[String, Type]) -> Type",
		.kind = textension_function,
		.function = types_make_type,
		.minimum_arguments = 1,
		.maximum_arguments = UNDEF_NPARAMS,
		.intrinsic = tnative_intrinsic_type_make
	},
	{
		.name = "union",
		.type = "Function[...] -> Type",
		.detail = "types::union(...members: Type) -> Type",
		.kind = textension_function,
		.function = types_union,
		.minimum_arguments = 2,
		.maximum_arguments = UNDEF_NPARAMS,
		.intrinsic = tnative_intrinsic_type_union
	},
	{
		.name = "enum",
		.type = "Function[...] -> Type",
		.detail = "types::enum(...members: String) -> Type",
		.kind = textension_function,
		.function = types_enum,
		.minimum_arguments = 1,
		.maximum_arguments = UNDEF_NPARAMS,
		.intrinsic = tnative_intrinsic_type_enum
	},
	{
		.name = "list",
		.type = "Function[Type] -> Type",
		.detail = "types::list(item: Type) -> Type",
		.kind = textension_function,
		.function = types_list,
		.minimum_arguments = 1,
		.maximum_arguments = 1,
		.intrinsic = tnative_intrinsic_type_list
	},
	{
		.name = "iterator",
		.type = "Function[Type] -> Type",
		.detail = "types::iterator(item: Type) -> Type",
		.kind = textension_function,
		.function = types_iterator,
		.minimum_arguments = 1,
		.maximum_arguments = 1,
		.intrinsic = tnative_intrinsic_type_iterator
	},
	{
		.name = "optional",
		.type = "Function[Pair[String, Type]] -> Type",
		.detail = "types::optional(field: Pair[String, Type]) -> Type",
		.kind = textension_function,
		.function = types_optional,
		.minimum_arguments = 1,
		.maximum_arguments = 1,
		.intrinsic = tnative_intrinsic_type_optional
	},
	{
		.name = "pair",
		.type = "Function[Type, Type] -> Type",
		.detail = "types::pair(first: Type, second: Type) -> Type",
		.kind = textension_function,
		.function = types_pair,
		.minimum_arguments = 2,
		.maximum_arguments = 2,
		.intrinsic = tnative_intrinsic_type_pair
	},
	{
		.name = "dictionary",
		.type = "Function[Type, Type] -> Type",
		.detail = "types::dictionary(key: Type, value: Type) -> Type",
		.kind = textension_function,
		.function = types_dictionary,
		.minimum_arguments = 2,
		.maximum_arguments = 2,
		.intrinsic = tnative_intrinsic_type_dictionary
	},
	{
		.name = "rule",
		.type = "Function[...] -> Type",
		.detail = "types::rule(...parameters: Type) -> Type",
		.kind = textension_function,
		.function = types_rule_type,
		.minimum_arguments = 0,
		.maximum_arguments = UNDEF_NPARAMS,
		.intrinsic = tnative_intrinsic_type_rule
	},
	{
		.name = "rule_instance",
		.type = "Function[...] -> Type",
		.detail = "types::rule_instance(...parameters: Type) -> Type",
		.kind = textension_function,
		.function = types_rule_instance_type,
		.minimum_arguments = 0,
		.maximum_arguments = UNDEF_NPARAMS,
		.intrinsic = tnative_intrinsic_type_rule_instance
	},
	{
		.name = "of",
		.type = "Function[AnyType] -> Type",
		.detail = "types::of(value: AnyType) -> Type",
		.kind = textension_function,
		.function = types_of,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	},
	{
		.name = "matches",
		.type = "Function[AnyType, Type] -> Bool",
		.detail = "types::matches(value: AnyType, expected: Type) -> Bool",
		.kind = textension_function,
		.function = types_matches,
		.minimum_arguments = 2,
		.maximum_arguments = 2
	},
	{
		.name = "fields",
		.type = "Function[Type] -> Dictionary",
		.detail = "types::fields(value: Type) -> Dictionary",
		.kind = textension_function,
		.function = types_fields,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	},
	{
		.name = "optional_fields",
		.type = "Function[Type] -> List[String]",
		.detail = "types::optional_fields(value: Type) -> List[String]",
		.kind = textension_function,
		.function = types_optional_fields,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	},
	{
		.name = "members",
		.type = "Function[Type] -> List",
		.detail = "types::members(value: Type) -> List",
		.kind = textension_function,
		.function = types_members,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	},
	{
		.name = "base",
		.type = "Function[Type] -> Type",
		.detail = "types::base(value: Type) -> Type",
		.kind = textension_function,
		.function = types_base,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	},
	{
		.name = "parameters",
		.type = "Function[Type] -> Dictionary",
		.detail = "types::parameters(value: Type) -> Dictionary",
		.kind = textension_function,
		.function = types_parameters,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	},
	{
		.name = "definition",
		.type = "Function[Type] -> Dictionary",
		.detail = "types::definition(value: Type) -> Dictionary",
		.kind = textension_function,
		.function = types_definition,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	}
};

const textension_module tstdlib_types_module = {
	.scope = textension_package,
	.name = "types",
	.detail = "Runtime type construction and reflection",
	.symbols = symbols,
	.symbol_count = sizeof(symbols) / sizeof(symbols[0])
};
