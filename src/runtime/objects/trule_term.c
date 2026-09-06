/**
 * @file trule_term.c
 * @brief Implements the core RuleTerm object.
 * @details Defines RuleTerm construction and validation, lifetime, identity,
 * field access, object operators, capabilities, formatting hooks, and vtable
 * registration.
 * @note Keep RuleIR aggregation, hashing, and wire formats out of this file;
 * it contains only the RuleTerm object contract.
 */
#include "tapas/objects/trule_ir.h"
#include "tapas/dsa/tstring.h"
#include "tapas/objects/ttype.h"

#include "tapas/tformat.h"
#include "tapas/objects/tlist.h"
#include "tapas/objects/tpair.h"
#include "tapas/objects/trule.h"
#include "tapas/objects/tstr.h"

#include <stdlib.h>
#include <string.h>

static uint64_t next_term_id = 1;

static void value_clone(tobj *result, const tobj *value)
{
	tobj_set_nil(result);
	if (!value || value->type != tcompo || !value->val.v_tcompo) {
		if (value) *result = *value;
		return;
	}
	tobj_set_compo(result, value->val.v_tcompo->vtable->copy(
		value->val.v_tcompo));
}

static ttypeval *value_type(const tobj *value)
{
	if (!value) return ttypeval_builtin(tbuiltintype_nil);
	switch (value->type) {
	case tnil: return ttypeval_builtin(tbuiltintype_nil);
	case tbool: return ttypeval_builtin(tbuiltintype_bool);
	case tint: return ttypeval_builtin(tbuiltintype_int);
	case tfloat: return ttypeval_builtin(tbuiltintype_float);
	case tcompo:
		if (!value->val.v_tcompo) return ttypeval_builtin(tbuiltintype_nil);
		{
			tobj reflected;
			if (tcompo_runtime_type(value->val.v_tcompo, &reflected)) {
				if (tobj_compo_type(&reflected) == compo_ttypeval)
					return (ttypeval *)reflected.val.v_tcompo;
				tobj_try_clear(&reflected);
			}
		}
		switch (tobj_compo_type(value)) {
		case compo_tstr: return ttypeval_builtin(tbuiltintype_string);
		case compo_tlist: {
			tlist *list = (tlist *)value->val.v_tcompo;
			if (!tlist_size(list))
				return ttypeval_new_list(
					ttypeval_builtin(tbuiltintype_any));
			ttypeval **members = calloc(tlist_size(list),
				sizeof(*members));
			uint_objs count = 0;
			for (uint_objs i = 0; i < tlist_size(list); i++) {
				ttypeval *item = value_type(tlist_at(list, i));
				int present = 0;
				for (uint_objs j = 0; j < count; j++)
					present |= ttypeval_equal(members[j], item);
				if (!present) members[count++] = item;
			}
			ttypeval *item = count == 1 ? members[0] :
				ttypeval_new_union(members, count);
			free(members);
			return ttypeval_new_list(item);
		}
		case compo_tpair: {
			tpair *pair = (tpair *)value->val.v_tcompo;
			return ttypeval_new_pair(value_type(&pair->first),
				value_type(&pair->second));
		}
		case compo_tdict: return ttypeval_builtin(tbuiltintype_dictionary);
		case compo_titer: return ttypeval_builtin(tbuiltintype_iterator);
		case compo_tfunc:
		case compo_cppfunc:
		case compo_sessfunc:
			return ttypeval_builtin(tbuiltintype_function);
		case compo_ttypeval: return ttypeval_builtin(tbuiltintype_type);
		case compo_trule: {
			trule *rule = (trule *)value->val.v_tcompo;
			uint_objs count = rule->ir ? rule->ir->parameters.len : 0;
			ttypeval **parameters = count ? calloc(count,
				sizeof(*parameters)) : nullptr;
			for (uint_objs i = 0; i < count; i++)
				parameters[i] = ((trule_term *)rule->ir->parameters
					.data[i].val.v_tcompo)->type;
			ttypeval *type = ttypeval_new_rule(parameters, count);
			free(parameters);
			return type;
		}
		case compo_trule_instance: {
			trule *rule = (trule *)((trule_instance *)value->val.v_tcompo)
				->rule.val.v_tcompo;
			uint_objs count = rule->ir ? rule->ir->parameters.len : 0;
			ttypeval **parameters = count ? calloc(count,
				sizeof(*parameters)) : nullptr;
			for (uint_objs i = 0; i < count; i++)
				parameters[i] = ((trule_term *)rule->ir->parameters
					.data[i].val.v_tcompo)->type;
			ttypeval *type = ttypeval_new_rule_instance(parameters, count);
			free(parameters);
			return type;
		}
		case compo_trule_ir: return ttypeval_builtin(tbuiltintype_rule_ir);
		case compo_trule_term: return ttypeval_builtin(tbuiltintype_rule_term);
		case compo_trule_item: return ttypeval_builtin(tbuiltintype_rule_item);
		default: return ttypeval_builtin(tbuiltintype_any);
		}
	}
	return ttypeval_builtin(tbuiltintype_any);
}

trule_term *trule_term_new(trule_term_kind kind, ttypeval *type,
			   const tobj *payload, const tobj *arguments,
			   uint_regs argument_count)
{
	trule_term *term = calloc(1, sizeof(*term));
	if (!term) abort();
	term->base.vtable = &trule_term_vtable;
	term->id = next_term_id++;
	term->kind = kind;
	term->type = ttypeval_retain(type ? type : ttypeval_builtin(tbuiltintype_any));
	tobj_set_nil(&term->payload);
	if (payload) tobj_copy(&term->payload, payload);
	tobj_vec_init(&term->arguments);
	for (uint_regs i = 0; i < argument_count; i++)
		tobj_vec_push(&term->arguments, &arguments[i]);
	term->provider = tstring_new_empty();
	term->provider_kind = tstring_new_empty();
	term->origin_start = -1;
	term->origin_end = -1;
	return term;
}

int trule_antecedent_type(const ttypeval *type)
{
	if (!type) return 0;
	if (ttypeval_equal(type, ttypeval_builtin(tbuiltintype_bool)) ||
	    type->kind == ttype_kind_rule_instance ||
	    (type->kind == ttype_kind_builtin &&
	     type->builtin == tbuiltintype_rule_instance))
		return 1;
	if (type->kind != ttype_kind_union) return 0;
	uint_objs count = ttypeval_member_count(type);
	for (uint_objs i = 0; i < count; i++)
		if (!trule_antecedent_type(ttypeval_member_at(type, i))) return 0;
	return count != 0;
}

trule_term *trule_term_logic_new(trule_term_kind kind, trule_term *left,
				 trule_term *right)
{
	if (kind != trule_term_and && kind != trule_term_or)
		twarn(ErrRuntime_ParamsType, "Rule logic", "And or Or required");
	trule_term *operands[] = { left, right };
	tobj arguments[2];
	for (unsigned i = 0; i < 2; i++) {
		if (!operands[i] ||
		    (!trule_antecedent_type(operands[i]->type) &&
		     !ttypeval_equal(operands[i]->type,
			ttypeval_builtin(tbuiltintype_any))))
			twarn(ErrRuntime_ParamsType, "Rule logic",
			      "Bool or RuleInstance Terms required");
		arguments[i] = (tobj){ .type = tcompo,
			.val.v_tcompo = (tcompo_v *)operands[i] };
	}
	return trule_term_new(kind, ttypeval_builtin(tbuiltintype_bool), nullptr,
		arguments, 2);
}

trule_term *trule_term_in_new(trule_term *value, trule_term *domain)
{
	if (!value || !domain)
		twarn(ErrRuntime_ParamsType, "rules::membership",
		      "two Terms required");
	tobj arguments[] = {
		{ .type = tcompo, .val.v_tcompo = (tcompo_v *)value },
		{ .type = tcompo, .val.v_tcompo = (tcompo_v *)domain }
	};
	return trule_term_new(trule_term_in, ttypeval_builtin(tbuiltintype_bool),
		nullptr, arguments, 2);
}

trule_term *trule_term_not_new(trule_term *operand)
{
	if (!operand || (!trule_antecedent_type(operand->type) &&
	    !ttypeval_equal(operand->type, ttypeval_builtin(tbuiltintype_any))))
		twarn(ErrRuntime_ParamsType, "negation",
		      "Bool or RuleInstance Term required");
	tobj argument = {
		.type = tcompo,
		.val.v_tcompo = (tcompo_v *)operand
	};
	return trule_term_new(trule_term_not, ttypeval_builtin(tbuiltintype_bool),
		nullptr, &argument, 1);
}

trule_term *trule_term_parameter_new(const char *name, ttypeval *type)
{
	tobj payload;
	tobj_set_nil(&payload);
	tobj_set_compo(&payload, (tcompo_v *)tstr_new(name ? name : ""));
	trule_term *term = trule_term_new(trule_term_parameter, type,
		&payload, nullptr, 0);
	tobj_try_clear(&payload);
	return term;
}

trule_term *trule_term_constant_new(const tobj *value)
{
	tobj stored;
	value_clone(&stored, value);
	trule_term *term = trule_term_new(trule_term_constant,
		value_type(value), &stored, nullptr, 0);
	tobj_try_clear(&stored);
	return term;
}

trule_term *trule_term_extension_new(const char *provider,
			     const char *kind, const tobj *arguments,
			     uint_regs argument_count, const tobj *payload)
{
	trule_term *term = trule_term_new(trule_term_extension,
		ttypeval_builtin(tbuiltintype_any), payload, arguments, argument_count);
	tstring_free(term->provider);
	tstring_free(term->provider_kind);
	term->provider = tstring_new(provider ? provider : "");
	term->provider_kind = tstring_new(kind ? kind : "");
	term->provider_version = 1;
	return term;
}

const char *trule_term_kind_name(trule_term_kind kind)
{
	static const char *const names[] = {
		"Constant", "Parameter", "Capture", "Intrinsic", "Call",
		"Construct", "Convert", "Extension", "Not", "And", "Or", "In"
	};
	return kind >= trule_term_constant && kind <= trule_term_in ?
		names[kind] : "Invalid";
}

/*----------------------- Required Vtable Operations -----------------------*/

static const char *term_type(void) { return "RuleTerm"; }
static tcompo_type term_code(void) { return compo_trule_term; }
static long term_len(void *self) { (void)self; return 0; }

static void *term_copy(void *self)
{
	trule_term *term = self;
	term->base.refctr++;
	return term;
}

static void term_free(void *self)
{
	trule_term *term = self;
	ttypeval_release(term->type);
	tobj_try_clear(&term->payload);
	tobj_vec_free(&term->arguments);
	tstring_free(term->provider);
	tstring_free(term->provider_kind);
	free(term);
}

static int term_identical(void *self, void *other) { return self == other; }

static const char *term_string_value(const tobj *value)
{
	return value && value->type == tcompo &&
		tobj_compo_type(value) == compo_tstr ?
		tstring_cstr(((tstr *)value->val.v_tcompo)->data) : nullptr;
}

static void term_expression(tformat_context *context, const trule_term *term)
{
	const char *payload = term_string_value(&term->payload);
	if (term->kind == trule_term_parameter) {
		tformat_text(context, payload ? payload : "?");
		return;
	}
	if (term->kind == trule_term_capture) {
		tformat_text(context, "capture(");
		tformat_value(context, &term->payload);
		tformat_text(context, ": ");
		tformat_type(context, term->type);
		tformat_text(context, ")");
		return;
	}
	if (term->kind == trule_term_constant) {
		tformat_value(context, &term->payload);
		return;
	}
	if (term->kind == trule_term_not && term->arguments.len == 1) {
		tformat_text(context, "not (");
		tformat_value(context, &term->arguments.data[0]);
		tformat_text(context, ")");
		return;
	}
	const char *binary = term->kind == trule_term_and ? "and" :
		term->kind == trule_term_or ? "or" :
		term->kind == trule_term_in ? "in" : nullptr;
	if (!binary && term->kind == trule_term_intrinsic && payload &&
	    (!strcmp(payload, "+") || !strcmp(payload, "-") ||
	     !strcmp(payload, "*") || !strcmp(payload, "/") ||
	     !strcmp(payload, "==") || !strcmp(payload, "!=") ||
	     !strcmp(payload, "<") || !strcmp(payload, ">") ||
	     !strcmp(payload, "<=") || !strcmp(payload, ">=")))
		binary = payload;
	if (binary && term->arguments.len == 2) {
		tformat_text(context, "(");
		tformat_value(context, &term->arguments.data[0]);
		tformat_text(context, " ");
		tformat_text(context, binary);
		tformat_text(context, " ");
		tformat_value(context, &term->arguments.data[1]);
		tformat_text(context, ")");
		return;
	}
	if (term->kind == trule_term_call) {
		tformat_text(context, "(");
		tformat_value(context, &term->payload);
		tformat_text(context, ")(");
		tformat_sequence(context, &term->arguments);
		tformat_text(context, ")");
		return;
	}
	tformat_text(context, trule_term_kind_name(term->kind));
	tformat_text(context, "(payload=");
	tformat_value(context, &term->payload);
	if (tstring_len(term->provider)) {
		tformat_text(context, ", provider=");
		tformat_quoted(context, tstring_cstr(term->provider));
	}
	if (tstring_len(term->provider_kind)) {
		tformat_text(context, ", kind=");
		tformat_quoted(context, tstring_cstr(term->provider_kind));
	}
	tformat_text(context, ", arguments=[");
	tformat_sequence(context, &term->arguments);
	tformat_text(context, "])");
}

static void term_render(tformat_context *context, const void *self)
{
	const trule_term *term = self;
	if (!tformat_is_root(context)) {
		term_expression(context, term);
		return;
	}
	const char *name =
		(term->kind == trule_term_parameter ||
		 term->kind == trule_term_capture) ?
		term_string_value(&term->payload) : nullptr;
	tformat_text(context, "RuleTerm ");
	tformat_text(context, trule_term_kind_name(term->kind));
	tformat_text(context, "[");
	if (name) {
		tformat_text(context, name);
		tformat_text(context, ": ");
		tformat_type(context, term->type);
	} else {
		term_expression(context, term);
		tformat_text(context, ": ");
		tformat_type(context, term->type);
	}
	tformat_text(context, "]");
}

static tstring *term_tostring_abbr(void *self)
{
	return tformat_pointer(self);
}

static tstring *term_tostring_full(void *self)
{
	return tformat_object(self, 16384, term_render);
}

/*------------------------------- Operators --------------------------------*/

static trule_term *as_term(const tobj *value)
{
	return value && value->type == tcompo && value->val.v_tcompo &&
		tobj_compo_type(value) == compo_trule_term ?
		(trule_term *)value->val.v_tcompo : nullptr;
}

static void term_binary(void *self, const tobj *other, int is_rhs,
			const char *operation, int comparison, tobj *result)
{
	trule_term *left = self;
	trule_term *owned = nullptr;
	trule_term *right = as_term(other);
	if (!right) right = owned = trule_term_constant_new(other);
	tobj arguments[2];
	tobj_set_nil(&arguments[0]);
	tobj_set_nil(&arguments[1]);
	tobj_set_compo(&arguments[is_rhs ? 1 : 0], (tcompo_v *)left);
	tobj_set_compo(&arguments[is_rhs ? 0 : 1], (tcompo_v *)right);
	tobj payload;
	tobj_set_nil(&payload);
	tobj_set_compo(&payload, (tcompo_v *)tstr_new(operation));
	ttypeval *type;
	if (comparison)
		type = ttypeval_builtin(tbuiltintype_bool);
	else if (ttypeval_equal(left->type, right->type))
		type = left->type;
	else if ((ttypeval_equal(left->type, ttypeval_builtin(tbuiltintype_int)) ||
		  ttypeval_equal(left->type, ttypeval_builtin(tbuiltintype_float))) &&
		 (ttypeval_equal(right->type, ttypeval_builtin(tbuiltintype_int)) ||
		  ttypeval_equal(right->type, ttypeval_builtin(tbuiltintype_float))))
		type = ttypeval_builtin(tbuiltintype_float);
	else
		type = ttypeval_builtin(tbuiltintype_any);
	trule_term *term = trule_term_new(trule_term_intrinsic, type,
		&payload, arguments, 2);
	tobj_try_clear(&payload);
	if (owned) {
		tobj owner;
		tobj_set_nil(&owner);
		tobj_set_compo(&owner, (tcompo_v *)owned);
		tobj_try_clear(&owner);
	}
	tobj output;
	tobj_set_nil(&output);
	tobj_set_compo(&output, (tcompo_v *)term);
	tobj_copy(result, &output);
}

#define TERM_BINARY(name, text, comparison) \
	static void name(void *self, const tobj *other, int is_rhs, tobj *result) \
	{ term_binary(self, other, is_rhs, text, comparison, result); }

TERM_BINARY(term_add, "+", 0)
TERM_BINARY(term_sub, "-", 0)
TERM_BINARY(term_mul, "*", 0)
TERM_BINARY(term_div, "/", 0)
TERM_BINARY(term_mod, "%", 0)
TERM_BINARY(term_pow, "^", 0)
TERM_BINARY(term_mmul, "@", 0)
TERM_BINARY(term_eq, "==", 1)
TERM_BINARY(term_ne, "!=", 1)
TERM_BINARY(term_sg, ">", 1)
TERM_BINARY(term_sl, "<", 1)
TERM_BINARY(term_ge, ">=", 1)
TERM_BINARY(term_le, "<=", 1)
TERM_BINARY(term_and, "and", 1)
TERM_BINARY(term_or, "or", 1)

static void term_neg(void *self, tobj *result)
{
	trule_term *term = self;
	tobj argument;
	tobj payload;
	tobj_set_nil(&argument);
	tobj_set_nil(&payload);
	tobj_set_compo(&argument, (tcompo_v *)term);
	tobj_set_compo(&payload, (tcompo_v *)tstr_new("neg"));
	tobj output;
	tobj_set_nil(&output);
	tobj_set_compo(&output, (tcompo_v *)trule_term_new(
		trule_term_intrinsic, term->type, &payload, &argument, 1));
	tobj_copy(result, &output);
	tobj_try_clear(&payload);
}

/*------------------------------ Capabilities ------------------------------*/

static const char *index_name(const tobj *arguments, uint_regs count)
{
	if (count != 1 || arguments[0].type != tcompo ||
	    tobj_compo_type(&arguments[0]) != compo_tstr)
		twarn(ErrRuntime_ParamsType, "RuleTerm", "String field required");
	return tstring_cstr(((tstr *)arguments[0].val.v_tcompo)->data);
}

static tlist *vector_snapshot(const tobj_vec *values)
{
	tlist *list = tlist_new();
	for (uint_objs i = 0; i < values->len; i++)
		tobj_vec_push(&list->items, &values->data[i]);
	return list;
}

static void term_index(void *self, const tobj *arguments, uint_regs count,
		       tobj *result)
{
	trule_term *term = self;
	const char *name = index_name(arguments, count);
	if (strcmp(name, "id") == 0) tobj_set_int(result, (long)term->id);
	else if (strcmp(name, "kind") == 0)
		tobj_set_compo(result, (tcompo_v *)tstr_new(
			trule_term_kind_name(term->kind)));
	else if (strcmp(name, "type") == 0)
		tobj_copy(result, &(tobj){ .type = tcompo,
			.val.v_tcompo = (tcompo_v *)term->type });
	else if (strcmp(name, "arguments") == 0)
		tobj_set_compo(result, (tcompo_v *)vector_snapshot(&term->arguments));
	else if (strcmp(name, "payload") == 0) tobj_copy(result, &term->payload);
	else if (strcmp(name, "provider") == 0)
		tobj_set_compo(result, (tcompo_v *)tstr_new(
			tstring_cstr(term->provider)));
	else if (strcmp(name, "version") == 0)
		tobj_set_int(result, term->provider_version);
	else twarn(ErrRuntime_Other, "RuleTerm", name);
}

static const tcompo_capabilities term_capabilities = {
	.indexable = term_index
};

tcompo_vtable trule_term_vtable = {
	.get_type = term_type,
	.get_compo_type_code = term_code,
	.len = term_len,
	.copy = term_copy,
	.free = term_free,
	.identical = term_identical,
	.tostring_abbr = term_tostring_abbr,
	.tostring_full = term_tostring_full,
	.op_neg = term_neg,
	.op_add = term_add,
	.op_sub = term_sub,
	.op_mul = term_mul,
	.op_div = term_div,
	.op_mod = term_mod,
	.op_pow = term_pow,
	.op_mmul = term_mmul,
	.op_eq = term_eq,
	.op_ne = term_ne,
	.op_sg = term_sg,
	.op_sl = term_sl,
	.op_ge = term_ge,
	.op_le = term_le,
	.op_and = term_and,
	.op_or = term_or,
	.capabilities = &term_capabilities
};
