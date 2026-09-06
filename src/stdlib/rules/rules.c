#include "tapas/textension.h"

#include "tapas/runtime/tlist.h"
#include "tapas/runtime/tpair.h"
#include <string.h>
#include "tapas/runtime/tdomain.h"
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

static trule *rule_is(const tobj *value, const char *name)
{
	if (!value || value->type != tcompo ||
	    tobj_compo_type(value) != compo_trule)
		twarn(ErrRuntime_ParamsType, name, "Rule required");
	return (trule *)value->val.v_tcompo;
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

static void rules_negation(tobj *params, uint_regs count, tobj *result)
{
	count_is("rules::negation", count, 1);
	tobj_set_compo(result, (tcompo_v *)trule_term_not_new(
		term_is(&params[0], "rules::negation")));
}

static void rules_conjunction(tobj *params, uint_regs count, tobj *result)
{
	count_is("rules::conjunction", count, 2);
	tobj_set_compo(result, (tcompo_v *)trule_term_logic_new(trule_term_and,
		term_is(&params[0], "rules::conjunction"), term_is(&params[1], "rules::conjunction")));
}

static void rules_disjunction(tobj *params, uint_regs count, tobj *result)
{
	count_is("rules::disjunction", count, 2);
	tobj_set_compo(result, (tcompo_v *)trule_term_logic_new(trule_term_or,
		term_is(&params[0], "rules::disjunction"), term_is(&params[1], "rules::disjunction")));
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

static void rules_implication(tobj *params, uint_regs count, tobj *result)
{
	if (count < 2 || count > 3)
		twarn(ErrRuntime_ParamsCtr, "rules::implication", "two or three arguments required");
	trule_term *antecedent = term_is(&params[0], "rules::implication");
	tlist *consequents = list_is(&params[1], "rules::implication");
	const char *description = count == 3 && params[2].type != tnil ?
		string_is(&params[2], "rules::implication") : "";
	tobj_set_compo(result, (tcompo_v *)trule_implication_new(antecedent,
		consequents->items.data, (uint_regs)consequents->items.len, description));
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
		if (item->kind != trule_item_requirement) {
			if (item->kind == trule_item_implication ? !trule_antecedent_type(item->term->type) :
			    !ttypeval_equal(item->term->type, ttypeval_builtin(tbuiltin_bool)))
				twarn(ErrRuntime_ParamsType, "rules::make", "invalid condition or antecedent Term Type");
			validate_term(ir, item->term, path, 0);
			for (uint_objs j = 0; j < item->arguments.len; j++)
				validate_term(ir, term_is(&item->arguments.data[j],
					"rules::make"), path, 0);
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

static uint_objs rules_item_index(const trule *rule, const tobj *selector,
				  const char *api)
{
	const tobj_vec *items = &rule->ir->items;
	if (selector->type == tint) {
		if (selector->val.v_tint < 0 ||
		    (uint_objs)selector->val.v_tint >= items->len)
			twarn(ErrRuntime_Other, api, "Rule Item index is out of range");
		return (uint_objs)selector->val.v_tint;
	}
	if (selector->type == tcompo &&
	    tobj_compo_type(selector) == compo_tstr) {
		const char *description = tstring_cstr(
			((tstr *)selector->val.v_tcompo)->data);
		uint_objs match = 0;
		uint_objs index = 0;
		for (uint_objs i = 0; i < items->len; i++) {
			trule_item *item = (trule_item *)items->data[i].val.v_tcompo;
			if (!strcmp(description, tstring_cstr(item->description))) {
				index = i;
				match++;
			}
		}
		if (!match)
			twarn(ErrRuntime_Other, api,
			      "Rule has no direct Item with this description");
		if (match > 1)
			twarn(ErrRuntime_Other, api,
			      "Rule Item description is ambiguous");
		return index;
	}
	if (selector->type == tcompo &&
	    tobj_compo_type(selector) == compo_trule_item) {
		for (uint_objs i = 0; i < items->len; i++)
			if (items->data[i].val.v_tcompo == selector->val.v_tcompo)
				return i;
		twarn(ErrRuntime_Other, api,
		      "Rule Item does not belong directly to this Rule");
	}
	twarn(ErrRuntime_ParamsType, api, "Int, String or Item selector required");
	return 0;
}

static void rules_item(tobj *params, uint_regs count, tobj *result)
{
	const char *api = "rules::item";
	count_is(api, count, 2);
	trule *rule = rule_is(&params[0], api);
	uint_objs index = rules_item_index(rule, &params[1], api);
	tobj_copy(result, &rule->ir->items.data[index]);
}

static trule_term *requirement_predicate(const trule_item *item)
{
	if (trule_antecedent_type(item->rule->type)) return item->rule;
	tobj callable = {
		.type = tcompo,
		.val.v_tcompo = (tcompo_v *)item->rule
	};
	return trule_term_new(trule_term_call,
		ttypeval_builtin(tbuiltin_rule_instance), &callable,
		item->arguments.data, (uint_regs)item->arguments.len);
}

static trule_term *implication_violation(const trule_item *item)
{
	trule_term *consequents =
		(trule_term *)item->arguments.data[0].val.v_tcompo;
	for (uint_objs i = 1; i < item->arguments.len; i++)
		consequents = trule_term_logic_new(trule_term_and, consequents,
			(trule_term *)item->arguments.data[i].val.v_tcompo);
	trule_term *not_consequents = trule_term_not_new(consequents);
	return trule_term_logic_new(trule_term_and, item->term,
		not_consequents);
}

/* Return a Term whose truth is exactly the satisfaction of one Rule Item.
 * Conditions already are predicates. Requirements become RuleInstance calls,
 * and an implication is true when its antecedent is false or every
 * consequent is true. */
static trule_term *item_predicate(const trule_item *item)
{
	if (item->kind == trule_item_condition)
		return item->term;
	if (item->kind == trule_item_requirement)
		return requirement_predicate(item);

	trule_term *consequents =
		(trule_term *)item->arguments.data[0].val.v_tcompo;
	for (uint_objs i = 1; i < item->arguments.len; i++)
		consequents = trule_term_logic_new(trule_term_and, consequents,
			(trule_term *)item->arguments.data[i].val.v_tcompo);
	trule_term *not_antecedent = trule_term_not_new(item->term);
	return trule_term_logic_new(trule_term_or, not_antecedent,
		consequents);
}

static void compatible_rule_is(const trule *source, const trule *premise,
			       const char *api)
{
	if (source->ir->parameters.len != premise->ir->parameters.len)
		twarn(ErrRuntime_ParamsCtr, api,
		      "premise Rule signature does not match source Rule");
	for (uint_objs i = 0; i < source->ir->parameters.len; i++) {
		trule_term *left = (trule_term *)
			source->ir->parameters.data[i].val.v_tcompo;
		trule_term *right = (trule_term *)
			premise->ir->parameters.data[i].val.v_tcompo;
		if (!ttypeval_equal(left->type, right->type))
			twarn(ErrRuntime_ParamsType, api,
			      "premise Rule signature does not match source Rule");
	}
}

/* Apply premise to the source Rule's parameters. The Rule is retained as a
 * Constant below the Call Term, so its own closure remains live. */
static trule_term *premise_predicate(const trule *premise,
				     const trule_ir *source)
{
	tobj value = {
		.type = tcompo,
		.val.v_tcompo = (tcompo_v *)premise
	};
	trule_term *constant = trule_term_constant_new(&value);
	tobj callable = {
		.type = tcompo,
		.val.v_tcompo = (tcompo_v *)constant
	};
	return trule_term_new(trule_term_call,
		ttypeval_builtin(tbuiltin_rule_instance), &callable,
		source->parameters.data, (uint_regs)source->parameters.len);
}

static trule_item *violated_item(const trule_item *item)
{
	trule_term *predicate;
	if (item->kind == trule_item_implication)
		predicate = implication_violation(item);
	else {
		trule_term *operand = item->kind == trule_item_requirement ?
			requirement_predicate(item) : item->term;
		predicate = trule_term_not_new(operand);
	}
	trule_item *violated = trule_condition_new(predicate,
		tstring_cstr(item->description));
	violated->origin_start = item->origin_start;
	violated->origin_end = item->origin_end;
	if (predicate->base.refctr == 0)
		predicate->base.vtable->free(predicate);
	return violated;
}

static void rules_transform(tobj *params, uint_regs count, tobj *result,
			    int violate)
{
	const char *api = violate ? "rules::violate" : "rules::drop";
	count_is(api, count, 2);
	trule *source = rule_is(&params[0], api);
	uint_objs target = rules_item_index(source, &params[1], api);
	trule_ir *ir = trule_ir_new("", tstring_cstr(source->ir->source));
	for (uint_objs i = 0; i < source->ir->parameters.len; i++)
		trule_ir_add_parameter(ir, (trule_term *)
			source->ir->parameters.data[i].val.v_tcompo);
	for (uint_objs i = 0; i < source->ir->captures.len; i++)
		trule_ir_add_capture(ir, (trule_term *)
			source->ir->captures.data[i].val.v_tcompo);
	for (uint_objs i = 0; i < source->ir->items.len; i++) {
		if (i == target) {
			if (!violate) continue;
			trule_item *item = violated_item((trule_item *)
				source->ir->items.data[i].val.v_tcompo);
			trule_ir_add_item(ir, item);
			continue;
		}
		trule_ir_add_item(ir, (trule_item *)
			source->ir->items.data[i].val.v_tcompo);
	}
	if (ir->version < source->ir->version)
		ir->version = source->ir->version;
	tobj_set_compo(result, (tcompo_v *)trule_new_derived(source, ir));
	if (ir->base.refctr > 0) ir->base.refctr--;
}

static void rules_drop(tobj *params, uint_regs count, tobj *result)
{
	rules_transform(params, count, result, 0);
}

static void rules_violate(tobj *params, uint_regs count, tobj *result)
{
	rules_transform(params, count, result, 1);
}

static void rules_conditional_transform(tobj *params, uint_regs count,
					tobj *result, int violate)
{
	const char *api = violate ? "rules::violate_if" : "rules::drop_if";
	count_is(api, count, 3);
	trule *source = rule_is(&params[0], api);
	uint_objs target = rules_item_index(source, &params[1], api);
	trule *premise = rule_is(&params[2], api);
	compatible_rule_is(source, premise, api);

	trule_ir *ir = trule_ir_new("", tstring_cstr(source->ir->source));
	for (uint_objs i = 0; i < source->ir->parameters.len; i++)
		trule_ir_add_parameter(ir, (trule_term *)
			source->ir->parameters.data[i].val.v_tcompo);
	for (uint_objs i = 0; i < source->ir->captures.len; i++)
		trule_ir_add_capture(ir, (trule_term *)
			source->ir->captures.data[i].val.v_tcompo);

	for (uint_objs i = 0; i < source->ir->items.len; i++) {
		trule_item *item = (trule_item *)
			source->ir->items.data[i].val.v_tcompo;
		if (i != target) {
			trule_ir_add_item(ir, item);
			continue;
		}

		trule_term *premise_term = premise_predicate(premise, ir);
		trule_term *target_term = item_predicate(item);
		/* drop_if: P or t, i.e. P may drop t and !P requires t. */
		trule_term *predicate = trule_term_logic_new(trule_term_or,
			premise_term, target_term);
		if (violate) {
			/* violate_if: (P or t) and (!P or !t), i.e. P xor t. */
			trule_term *not_premise = trule_term_not_new(premise_term);
			trule_term *not_target = trule_term_not_new(target_term);
			trule_term *opposites = trule_term_logic_new(trule_term_or,
				not_premise, not_target);
			predicate = trule_term_logic_new(trule_term_and,
				predicate, opposites);
		}
		trule_item *replacement = trule_condition_new(predicate,
			tstring_cstr(item->description));
		replacement->origin_start = item->origin_start;
		replacement->origin_end = item->origin_end;
		trule_ir_add_item(ir, replacement);
		if (predicate->base.refctr == 0)
			predicate->base.vtable->free(predicate);
	}
	if (ir->version < source->ir->version)
		ir->version = source->ir->version;
	tobj_set_compo(result, (tcompo_v *)trule_new_derived(source, ir));
	if (ir->base.refctr > 0) ir->base.refctr--;
}

static void rules_drop_if(tobj *params, uint_regs count, tobj *result)
{
	rules_conditional_transform(params, count, result, 0);
}

static void rules_violate_if(tobj *params, uint_regs count, tobj *result)
{
	rules_conditional_transform(params, count, result, 1);
}

/* Keep the original Rule as a Requirement, preserving its checker and closure. */
static void rules_restrict(tobj *params, uint_regs count, tobj *result)
{
    const char *api = "rules::restrict";
    if (!count) twarn(ErrRuntime_ParamsCtr, api, "Rule required");
    if (params[0].type != tcompo || tobj_compo_type(&params[0]) != compo_trule)
        twarn(ErrRuntime_ParamsType, api, "Rule required");
    trule *original = (trule *)params[0].val.v_tcompo;
    /* Validate before allocating the derived graph. */
    for (uint_regs i = 1; i < count; i++) {
        if (params[i].type != tcompo || tobj_compo_type(&params[i]) != compo_tpair)
            twarn(ErrRuntime_ParamsType, api, "Pair[String : value or domain] required");
        tpair *pair = (tpair *)params[i].val.v_tcompo;
        const char *name = string_is(&pair->first, api);
        trule_term *parameter = nullptr;
        for (uint_objs j = 0; j < original->ir->parameters.len; j++) {
            trule_term *candidate = (trule_term *)original->ir->parameters.data[j].val.v_tcompo;
            if (!strcmp(name, string_is(&candidate->payload, api))) parameter = candidate;
        }
        if (!parameter) twarn(ErrRuntime_ParamsType, api, "unknown Rule parameter name");
        tobj *value = &pair->second;
        if (value->type == tcompo && (tobj_compo_type(value) == compo_tpoints ||
                                     tobj_compo_type(value) == compo_trange)) {
            tdomain *domain = (tdomain *)value->val.v_tcompo;
            if (domain->range) {
                if (!ttypeval_equal(parameter->type, ttypeval_builtin(tbuiltin_int)) &&
                    parameter->type->kind != ttype_kind_any)
                    twarn(ErrRuntime_ParamsType, api, "Range restriction requires an Int parameter");
            } else {
                for (uint_objs k = 0; k < domain->values.len; k++)
                    if (!ttypeval_matches(&domain->values.data[k], parameter->type))
                        twarn(ErrRuntime_ParamsType, api, "point does not match parameter Type");
            }
        } else if (!ttypeval_matches(value, parameter->type)) {
            twarn(ErrRuntime_ParamsType, api, "value does not match parameter Type");
        }
    }
    trule_ir *ir = trule_ir_new("", "");
    for (uint_objs j = 0; j < original->ir->parameters.len; j++)
        trule_ir_add_parameter(ir, (trule_term *)original->ir->parameters.data[j].val.v_tcompo);
    trule_term *base = trule_term_constant_new(&params[0]);
    trule_ir_add_item(ir, trule_requirement_new(base, ir->parameters.data,
                                             (uint_regs)ir->parameters.len));
    base->base.vtable->free(base);
    for (uint_regs i = 1; i < count; i++) {
        tpair *pair = (tpair *)params[i].val.v_tcompo;
        const char *name = string_is(&pair->first, api);
        trule_term *parameter = nullptr;
        for (uint_objs j = 0; j < ir->parameters.len; j++) {
            trule_term *candidate = (trule_term *)ir->parameters.data[j].val.v_tcompo;
            if (!strcmp(name, string_is(&candidate->payload, api))) parameter = candidate;
        }
        trule_term *constant = trule_term_constant_new(&pair->second);
        int domain = pair->second.type == tcompo &&
            (tobj_compo_type(&pair->second) == compo_tpoints ||
             tobj_compo_type(&pair->second) == compo_trange);
        trule_term *condition;
        if (domain) condition = trule_term_in_new(parameter, constant);
        else {
            tobj arguments[] = {
                {.type = tcompo, .val.v_tcompo = (tcompo_v *)parameter},
                {.type = tcompo, .val.v_tcompo = (tcompo_v *)constant}
            };
            tobj operation = {.type = tcompo, .val.v_tcompo = (tcompo_v *)tstr_new("==")};
            condition = trule_term_new(trule_term_intrinsic, ttypeval_builtin(tbuiltin_bool),
                                      &operation, arguments, 2);
            tobj_try_clear(&operation);
        }
        trule_ir_add_item(ir, trule_condition_new(condition, name));
        condition->base.vtable->free(condition);
    }
    tobj_set_compo(result, (tcompo_v *)trule_new_dynamic(ir, tstring_cstr(original->signature)));
    if (ir->base.refctr > 0) ir->base.refctr--;
}

static void rules_membership(tobj *params, uint_regs count, tobj *result)
{
    count_is("rules::membership", count, 2);
    tobj_set_compo(result, (tcompo_v *)trule_term_in_new(term_is(&params[0], "rules::membership"), term_is(&params[1], "rules::membership")));
}
static void rules_points(tobj *params, uint_regs count, tobj *result)
{
    if (!count) twarn(ErrRuntime_ParamsCtr, "rules::points", "element Type required");
    tobj_set_compo(result, (tcompo_v *)tpoints_new(type_is(&params[0], "rules::points"), params + 1, count - 1));
}
static void rules_range(tobj *params, uint_regs count, tobj *result)
{
    count_is("rules::range", count, 2);
    if (params[0].type != tint || params[1].type != tint)
        twarn(ErrRuntime_ParamsType, "rules::range", "Int endpoints required");
    tobj_set_compo(result, (tcompo_v *)trange_new(params[0].val.v_tint, params[1].val.v_tint));
}
TYPE_FACTORY(create_points_type, tbuiltin_points)
TYPE_FACTORY(create_range_type, tbuiltin_range)
static const textension_symbol symbols[] = {
    { .name = "item", .type = "Function[Rule, Int | String | Item] -> Item", .detail = "rules::item(rule: Rule, target: Int | String | Item) -> Item", .kind = textension_function, .function = rules_item, .minimum_arguments = 2, .maximum_arguments = 2 },
    { .name = "drop", .type = "Function[Rule, Int | String | Item] -> Rule", .detail = "rules::drop(rule: Rule, target: Int | String | Item) -> Rule", .kind = textension_function, .function = rules_drop, .minimum_arguments = 2, .maximum_arguments = 2 },
    { .name = "violate", .type = "Function[Rule, Int | String | Item] -> Rule", .detail = "rules::violate(rule: Rule, target: Int | String | Item) -> Rule", .kind = textension_function, .function = rules_violate, .minimum_arguments = 2, .maximum_arguments = 2 },
    { .name = "drop_if", .type = "Function[Rule, Int | String | Item, Rule] -> Rule", .detail = "rules::drop_if(rule: Rule, target: Int | String | Item, premise: Rule) -> Rule", .kind = textension_function, .function = rules_drop_if, .minimum_arguments = 3, .maximum_arguments = 3 },
    { .name = "violate_if", .type = "Function[Rule, Int | String | Item, Rule] -> Rule", .detail = "rules::violate_if(rule: Rule, target: Int | String | Item, premise: Rule) -> Rule", .kind = textension_function, .function = rules_violate_if, .minimum_arguments = 3, .maximum_arguments = 3 },
    { .name = "restrict", .type = "Function[...] -> Rule", .detail = "rules::restrict(rule: Rule, ...restrictions: Pair) -> Rule", .kind = textension_function, .function = rules_restrict, .minimum_arguments = 1, .maximum_arguments = UINT8_MAX },
    { .name = "membership", .type = "Function[Term, Term] -> Term", .detail = "rules::membership(value: Term, domain: Term) -> Term", .kind = textension_function, .function = rules_membership, .minimum_arguments = 2, .maximum_arguments = 2 },
    { .name = "PointsOf", .type = "Type", .detail = "PointsOf[T]: finite points", .kind = textension_type, .value_factory = create_points_type },
    { .name = "RangeOf", .type = "Type", .detail = "RangeOf[Int]: closed interval", .kind = textension_type, .value_factory = create_range_type },
    { .name = "points", .type = "Function[...] -> PointsOf", .detail = "rules::points(element_type: Type, ...values) -> PointsOf", .kind = textension_function, .function = rules_points, .minimum_arguments = 1, .maximum_arguments = UINT8_MAX },
    { .name = "range", .type = "Function[Int, Int] -> RangeOf[Int]", .detail = "rules::range(start: Int, end: Int) -> RangeOf[Int]", .kind = textension_function, .function = rules_range, .minimum_arguments = 2, .maximum_arguments = 2 },
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
		.type = "Function[RuleIR | Rule | RuleInstance] -> List",
		.detail = "rules::parameters(ir: RuleIR | Rule | RuleInstance) -> List[Parameter]",
		.kind = textension_value,
		.value_factory = create_parameters
	},
	{
		.name = "items",
		.type = "Function[RuleIR | Rule | RuleInstance] -> List",
		.detail = "rules::items(ir: RuleIR | Rule | RuleInstance) -> List[Item]",
		.kind = textension_value,
		.value_factory = create_items
	},
	{
		.name = "terms",
		.type = "Function[RuleIR | Rule | RuleInstance] -> List",
		.detail = "rules::terms(ir: RuleIR | Rule | RuleInstance) -> List[Term]",
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
		.name = "negation",
		.type = "Function[AnyType] -> Term",
		.detail = "rules::negation(operand: Term) -> Term[Bool]",
		.kind = textension_function,
		.function = rules_negation,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	},
	{
		.name = "conjunction",
		.type = "Function[AnyType, AnyType] -> Term",
		.detail = "rules::conjunction(left: Term, right: Term) -> Term[Bool]",
		.kind = textension_function,
		.function = rules_conjunction,
		.minimum_arguments = 2,
		.maximum_arguments = 2
	},
	{
		.name = "disjunction",
		.type = "Function[AnyType, AnyType] -> Term",
		.detail = "rules::disjunction(left: Term, right: Term) -> Term[Bool]",
		.kind = textension_function,
		.function = rules_disjunction,
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
		.name = "implication",
		.type = "Function[...] -> Item",
		.detail = "rules::implication(antecedent: Term, consequents: List[Term], description: String?) -> Item",
		.kind = textension_function,
		.function = rules_implication,
		.minimum_arguments = 2,
		.maximum_arguments = 3
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
