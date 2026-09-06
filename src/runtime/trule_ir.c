#include "tapas/runtime/trule_format.h"
#include "tapas/runtime/trule_ir.h"
#include "tapas/runtime/tdomain.h"

#include "tapas/runtime/tstr.h"
#include "tapas/runtime/trule.h"
#include "tapas/runtime/tdict.h"
#include "tapas/runtime/tlist.h"
#include "tapas/runtime/tpair.h"

#include <stdlib.h>
#include <string.h>

static uint64_t next_term_id = 1;

static long empty_len(void *self) { (void)self; return 0; }
static int pointer_identical(void *self, void *other) { return self == other; }
static const char *term_type(void) { return "RuleTerm"; }
static const char *item_type(void) { return "RuleItem"; }
static const char *ir_type(void) { return "RuleIR"; }
static tcompo_type term_code(void) { return compo_trule_term; }
static tcompo_type item_code(void) { return compo_trule_item; }
static tcompo_type ir_code(void) { return compo_trule_ir; }

const char *trule_term_kind_name(trule_term_kind kind)
{
	static const char *const names[] = {
		"Constant", "Parameter", "Capture", "Intrinsic", "Call",
		"Construct", "Convert", "Extension", "Not", "And", "Or", "In"
	};
	return kind >= trule_term_constant && kind <= trule_term_in ?
		names[kind] : "Invalid";
}

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
	if (!value) return ttypeval_builtin(tbuiltin_nil);
	switch (value->type) {
	case tnil: return ttypeval_builtin(tbuiltin_nil);
	case tbool: return ttypeval_builtin(tbuiltin_bool);
	case tint: return ttypeval_builtin(tbuiltin_int);
	case tfloat: return ttypeval_builtin(tbuiltin_float);
	case tcompo:
		if (!value->val.v_tcompo) return ttypeval_builtin(tbuiltin_nil);
		switch (tobj_compo_type(value)) {
		case compo_tpoints:
		case compo_trange: return ttypeval_new_domain(tobj_compo_type(value) == compo_trange, ((tdomain *)value->val.v_tcompo)->item_type);
		case compo_tstr: return ttypeval_builtin(tbuiltin_string);
		case compo_tlist: {
			tlist *list = (tlist *)value->val.v_tcompo;
			if (!tlist_size(list))
				return ttypeval_new_list(ttypeval_builtin(tbuiltin_any));
			ttypeval **members = calloc(tlist_size(list), sizeof(*members));
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
		case compo_tdict: return ttypeval_builtin(tbuiltin_dictionary);
		case compo_titer: return ttypeval_builtin(tbuiltin_iterator);
		case compo_tfunc:
		case compo_cppfunc:
		case compo_sessfunc: return ttypeval_builtin(tbuiltin_function);
		case compo_ttypeval: return ttypeval_builtin(tbuiltin_type);
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
		case compo_trule_ir: return ttypeval_builtin(tbuiltin_rule_ir);
		case compo_trule_term: return ttypeval_builtin(tbuiltin_rule_term);
		case compo_trule_item: return ttypeval_builtin(tbuiltin_rule_item);
		default: return ttypeval_builtin(tbuiltin_any);
		}
	}
	return ttypeval_builtin(tbuiltin_any);
}

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



static const char *index_name(const tobj *arguments, uint_regs count,
			      const char *owner)
{
	if (count != 1 || arguments[0].type != tcompo ||
	    tobj_compo_type(&arguments[0]) != compo_tstr)
		twarn(ErrRuntime_ParamsType, owner, "String field required");
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
	const char *name = index_name(arguments, count, "RuleTerm");
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
		type = ttypeval_builtin(tbuiltin_bool);
	else if (ttypeval_equal(left->type, right->type))
		type = left->type;
	else if ((ttypeval_equal(left->type, ttypeval_builtin(tbuiltin_int)) ||
		  ttypeval_equal(left->type, ttypeval_builtin(tbuiltin_float))) &&
		 (ttypeval_equal(right->type, ttypeval_builtin(tbuiltin_int)) ||
		  ttypeval_equal(right->type, ttypeval_builtin(tbuiltin_float))))
		type = ttypeval_builtin(tbuiltin_float);
	else
		type = ttypeval_builtin(tbuiltin_any);
	trule_term *term = trule_term_new(trule_term_intrinsic, type,
		&payload, arguments, 2);
	tobj_try_clear(&payload);
	if (owned) {
		tobj owner;
		tobj_set_nil(&owner);
		tobj_set_compo(&owner, (tcompo_v *)owned);
		tobj_try_clear(&owner);
	}
	/* Operator results occupy an owned stack slot (possibly an operand).
	 * Retain the new DAG root and release the replaced slot's reference. */
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

static void *item_copy(void *self)
{
	trule_item *item = self;
	item->base.refctr++;
	return item;
}

static void item_free(void *self)
{
	trule_item *item = self;
	if (item->term) {
		if (item->term->base.refctr > 0) item->term->base.refctr--;
		if (item->term->base.refctr == 0)
			item->term->base.vtable->free(item->term);
	}
	if (item->rule) {
		if (item->rule->base.refctr > 0) item->rule->base.refctr--;
		if (item->rule->base.refctr == 0)
			item->rule->base.vtable->free(item->rule);
	}
	tobj_vec_free(&item->arguments);
	tstring_free(item->description);
	free(item);
}



static void item_index(void *self, const tobj *arguments, uint_regs count,
		       tobj *result)
{
	trule_item *item = self;
	const char *name = index_name(arguments, count, "RuleItem");
	if (strcmp(name, "kind") == 0)
		tobj_set_compo(result, (tcompo_v *)tstr_new(
			item->kind == trule_item_condition ? "Condition" :
			item->kind == trule_item_implication ? "Implication" : "Requirement"));
	else if (strcmp(name, "antecedent") == 0 && item->kind == trule_item_implication)
		tobj_copy(result, &(tobj){ .type = tcompo, .val.v_tcompo = (tcompo_v *)item->term });
	else if (strcmp(name, "consequents") == 0 && item->kind == trule_item_implication)
		tobj_set_compo(result, (tcompo_v *)vector_snapshot(&item->arguments));
	else if (strcmp(name, "term") == 0 && item->term)
		tobj_copy(result, &(tobj){ .type = tcompo,
			.val.v_tcompo = (tcompo_v *)item->term });
	else if (strcmp(name, "rule") == 0 && item->rule)
		tobj_copy(result, &(tobj){ .type = tcompo,
			.val.v_tcompo = (tcompo_v *)item->rule });
	else if (strcmp(name, "arguments") == 0)
		tobj_set_compo(result, (tcompo_v *)vector_snapshot(&item->arguments));
	else if (strcmp(name, "description") == 0)
		tobj_set_compo(result, (tcompo_v *)tstr_new(
			tstring_cstr(item->description)));
	else twarn(ErrRuntime_Other, "RuleItem", name);
}

static void *ir_copy(void *self)
{
	trule_ir *ir = self;
	ir->base.refctr++;
	return ir;
}

static void ir_free(void *self)
{
	trule_ir *ir = self;
	tstring_free(ir->display_name);
	tstring_free(ir->source);
	tobj_vec_free(&ir->parameters);
	tobj_vec_free(&ir->captures);
	tobj_vec_free(&ir->terms);
	tobj_vec_free(&ir->items);
	free(ir);
}



static void ir_index(void *self, const tobj *arguments, uint_regs count,
		     tobj *result)
{
	trule_ir *ir = self;
	const char *name = index_name(arguments, count, "RuleIR");
	if (strcmp(name, "display_name") == 0)
		tobj_set_compo(result, (tcompo_v *)tstr_new(
			tstring_cstr(ir->display_name)));
	else if (strcmp(name, "source") == 0)
		tobj_set_compo(result, (tcompo_v *)tstr_new(
			tstring_cstr(ir->source)));
	else if (strcmp(name, "version") == 0) tobj_set_int(result, ir->version);
	else if (strcmp(name, "parameters") == 0)
		tobj_set_compo(result, (tcompo_v *)vector_snapshot(&ir->parameters));
	else if (strcmp(name, "captures") == 0)
		tobj_set_compo(result, (tcompo_v *)vector_snapshot(&ir->captures));
	else if (strcmp(name, "terms") == 0)
		tobj_set_compo(result, (tcompo_v *)vector_snapshot(&ir->terms));
	else if (strcmp(name, "items") == 0)
		tobj_set_compo(result, (tcompo_v *)vector_snapshot(&ir->items));
	else if (strcmp(name, "origins") == 0) {
		tlist *origins = tlist_new();
		for (uint_objs i = 0; i < ir->terms.len; i++) {
			trule_term *term =
				(trule_term *)ir->terms.data[i].val.v_tcompo;
			tdict *origin = tdict_new();
			tobj key;
			tobj value;
			tobj_set_nil(&key);
			tobj_set_nil(&value);
			tobj_set_compo(&key, (tcompo_v *)tstr_new("start"));
			tobj_set_int(&value, term->origin_start);
			tdict_set(origin, &key, &value);
			tobj_try_clear(&key);
			tobj_set_compo(&key, (tcompo_v *)tstr_new("end"));
			tobj_set_int(&value, term->origin_end);
			tdict_set(origin, &key, &value);
			tobj_try_clear(&key);
			tobj_set_compo(&value, (tcompo_v *)origin);
			tobj_vec_push(&origins->items, &value);
			tobj_try_clear(&value);
		}
		tobj_set_compo(result, (tcompo_v *)origins);
	}
	else twarn(ErrRuntime_Other, "RuleIR", name);
}

static const tcompo_capabilities term_capabilities = { .indexable = term_index };
static const tcompo_capabilities item_capabilities = { .indexable = item_index };
static const tcompo_capabilities ir_capabilities = { .indexable = ir_index };

tcompo_vtable trule_term_vtable = {
	.get_type = term_type, .get_compo_type_code = term_code,
	.len = empty_len, .copy = term_copy, .free = term_free,
	.identical = pointer_identical, .tostring_abbr = trule_format_abbr,
	.tostring_full = trule_format_full, .op_neg = term_neg, .op_add = term_add,
	.op_sub = term_sub, .op_mul = term_mul, .op_div = term_div,
	.op_mod = term_mod, .op_pow = term_pow, .op_mmul = term_mmul,
	.op_eq = term_eq, .op_ne = term_ne, .op_sg = term_sg,
	.op_sl = term_sl, .op_ge = term_ge, .op_le = term_le,
	.op_and = term_and, .op_or = term_or
	, .capabilities = &term_capabilities
};

tcompo_vtable trule_item_vtable = {
	.get_type = item_type, .get_compo_type_code = item_code,
	.len = empty_len, .copy = item_copy, .free = item_free,
	.identical = pointer_identical, .tostring_abbr = trule_format_abbr,
	.tostring_full = trule_format_full, .capabilities = &item_capabilities
};

tcompo_vtable trule_ir_vtable = {
	.get_type = ir_type, .get_compo_type_code = ir_code,
	.len = empty_len, .copy = ir_copy, .free = ir_free,
	.identical = pointer_identical, .tostring_abbr = trule_format_abbr,
	.tostring_full = trule_format_full, .capabilities = &ir_capabilities
};

trule_term *trule_term_new(trule_term_kind kind, ttypeval *type,
			   const tobj *payload, const tobj *arguments,
			   uint_regs argument_count)
{
	trule_term *term = calloc(1, sizeof(*term));
	if (!term) abort();
	term->base.vtable = &trule_term_vtable;
	term->id = next_term_id++;
	term->kind = kind;
	term->type = ttypeval_retain(type ? type : ttypeval_builtin(tbuiltin_any));
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

trule_term *trule_term_logic_new(trule_term_kind kind, trule_term *left, trule_term *right)
{
	if (kind != trule_term_and && kind != trule_term_or)
		twarn(ErrRuntime_ParamsType, "Rule logic", "And or Or required");
	trule_term *operands[] = {left, right};
	tobj arguments[2];
	for (unsigned i = 0; i < 2; i++) {
		if (!operands[i] || (!trule_antecedent_type(operands[i]->type) &&
		    !ttypeval_equal(operands[i]->type, ttypeval_builtin(tbuiltin_any))))
			twarn(ErrRuntime_ParamsType, "Rule logic", "Bool or RuleInstance Terms required");
		arguments[i] = (tobj){.type = tcompo, .val.v_tcompo = (tcompo_v *)operands[i]};
	}
	return trule_term_new(kind, ttypeval_builtin(tbuiltin_bool), nullptr, arguments, 2);
}

trule_term *trule_term_in_new(trule_term *value, trule_term *domain)
{
    if (!value || !domain) twarn(ErrRuntime_ParamsType, "rules::membership", "two Terms required");
    tobj arguments[] = {
        {.type = tcompo, .val.v_tcompo = (tcompo_v *)value},
        {.type = tcompo, .val.v_tcompo = (tcompo_v *)domain}
    };
    return trule_term_new(trule_term_in, ttypeval_builtin(tbuiltin_bool), nullptr, arguments, 2);
}

trule_term *trule_term_not_new(trule_term *operand)
{
	if (!operand || (!trule_antecedent_type(operand->type) &&
	    !ttypeval_equal(operand->type, ttypeval_builtin(tbuiltin_any))))
		twarn(ErrRuntime_ParamsType, "negation", "Bool or RuleInstance Term required");
	tobj argument = { .type = tcompo, .val.v_tcompo = (tcompo_v *)operand };
	return trule_term_new(trule_term_not, ttypeval_builtin(tbuiltin_bool),
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
		ttypeval_builtin(tbuiltin_any), payload, arguments, argument_count);
	tstring_free(term->provider);
	tstring_free(term->provider_kind);
	term->provider = tstring_new(provider ? provider : "");
	term->provider_kind = tstring_new(kind ? kind : "");
	term->provider_version = 1;
	return term;
}

static trule_term *term_detach(trule_term *source)
{
	trule_term *copy = trule_term_new(source->kind, source->type,
		&source->payload, source->arguments.data,
		(uint_regs)source->arguments.len);
	copy->id = source->id;
	tstring_free(copy->provider);
	tstring_free(copy->provider_kind);
	copy->provider = tstring_dup(source->provider);
	copy->provider_kind = tstring_dup(source->provider_kind);
	copy->provider_version = source->provider_version;
	copy->origin_start = source->origin_start;
	copy->origin_end = source->origin_end;
	return copy;
}

trule_item *trule_condition_new(trule_term *term, const char *description)
{
	trule_item *item = calloc(1, sizeof(*item));
	if (!item) abort();
	item->base.vtable = &trule_item_vtable;
	item->kind = trule_item_condition;
	item->term = term_detach(term);
	item->term->base.refctr++;
	tobj_vec_init(&item->arguments);
	item->description = tstring_new(description ? description : "");
	item->origin_start = item->origin_end = -1;
	return item;
}

int trule_antecedent_type(const ttypeval *type)
{
	if (!type) return 0;
	if (ttypeval_equal(type, ttypeval_builtin(tbuiltin_bool)) ||
	    type->kind == ttype_kind_rule_instance ||
	    (type->kind == ttype_kind_builtin && type->builtin == tbuiltin_rule_instance)) return 1;
	if (type->kind != ttype_kind_union) return 0;
	uint_objs count = ttypeval_member_count(type);
	for (uint_objs i = 0; i < count; i++)
		if (!trule_antecedent_type(ttypeval_member_at(type, i))) return 0;
	return count != 0;
}

trule_item *trule_implication_new(trule_term *antecedent,
	const tobj *consequents, uint_regs count, const char *description)
{
	if (!antecedent || !trule_antecedent_type(antecedent->type) || !count)
		twarn(ErrRuntime_ParamsType, "implication", "Bool or RuleInstance antecedent and nonempty consequents required");
	trule_item *item = trule_condition_new(antecedent, description);
	item->kind = trule_item_implication;
	for (uint_regs i = 0; i < count; i++) {
		trule_term *term = as_term(&consequents[i]);
		if (!term || !ttypeval_equal(term->type, ttypeval_builtin(tbuiltin_bool)))
			twarn(ErrRuntime_ParamsType, "implication", "Bool consequent Terms required");
		trule_term *copy = term_detach(term);
		tobj value = { .type = tcompo, .val.v_tcompo = (tcompo_v *)copy };
		tobj_vec_push(&item->arguments, &value);
	}
	return item;
}

trule_item *trule_requirement_new(trule_term *rule,
				  const tobj *arguments,
				  uint_regs argument_count)
{
	trule_item *item = calloc(1, sizeof(*item));
	if (!item) abort();
	item->base.vtable = &trule_item_vtable;
	item->kind = trule_item_requirement;
	item->rule = term_detach(rule);
	item->rule->base.refctr++;
	tobj_vec_init(&item->arguments);
	for (uint_regs i = 0; i < argument_count; i++)
		tobj_vec_push(&item->arguments, &arguments[i]);
	item->description = tstring_new_empty();
	item->origin_start = item->origin_end = -1;
	return item;
}

trule_ir *trule_ir_new(const char *display_name, const char *source)
{
	trule_ir *ir = calloc(1, sizeof(*ir));
	if (!ir) abort();
	ir->base.vtable = &trule_ir_vtable;
	ir->display_name = tstring_new(display_name ? display_name : "");
	ir->source = tstring_new(source ? source : "");
	ir->version = 1;
	tobj_vec_init(&ir->parameters);
	tobj_vec_init(&ir->captures);
	tobj_vec_init(&ir->terms);
	tobj_vec_init(&ir->items);
	return ir;
}

void trule_ir_collect_term(trule_ir *ir, trule_term *term)
{
	if (term->type->contains_domain && ir->version < 6) ir->version = 6;
	if (term->payload.type == tcompo && (tobj_compo_type(&term->payload) == compo_tpoints || tobj_compo_type(&term->payload) == compo_trange) && ir->version < 6) ir->version = 6;
	if (term->kind == trule_term_in && ir->version < 6) ir->version = 6;
	if (term->kind == trule_term_not && ir->version < 4) ir->version = 4;
	if ((term->kind == trule_term_and || term->kind == trule_term_or) && ir->version < 5) ir->version = 5;
	for (uint_objs i = 0; i < ir->terms.len; i++)
		if (ir->terms.data[i].val.v_tcompo == (tcompo_v *)term ||
		    (term->kind == trule_term_parameter &&
		     ((trule_term *)ir->terms.data[i].val.v_tcompo)->kind == trule_term_parameter &&
		     ((trule_term *)ir->terms.data[i].val.v_tcompo)->id == term->id)) return;
	for (uint_objs i = 0; i < term->arguments.len; i++) {
		trule_term *argument = as_term(&term->arguments.data[i]);
		if (argument) trule_ir_collect_term(ir, argument);
	}
	if (term->kind == trule_term_call && term->payload.type == tcompo &&
	    tobj_compo_type(&term->payload) == compo_trule_term)
		trule_ir_collect_term(ir,
			(trule_term *)term->payload.val.v_tcompo);
	tobj value;
	tobj_set_nil(&value);
	tobj_set_compo(&value, (tcompo_v *)term);
	tobj_vec_push(&ir->terms, &value);
}

void trule_ir_add_parameter(trule_ir *ir, trule_term *parameter)
{
	tobj value;
	tobj_set_nil(&value);
	tobj_set_compo(&value, (tcompo_v *)parameter);
	tobj_vec_push(&ir->parameters, &value);
	trule_ir_collect_term(ir, parameter);
}

void trule_ir_add_capture(trule_ir *ir, trule_term *capture)
{
	tobj value;
	tobj_set_nil(&value);
	tobj_set_compo(&value, (tcompo_v *)capture);
	tobj_vec_push(&ir->captures, &value);
	trule_ir_collect_term(ir, capture);
}

void trule_ir_add_item(trule_ir *ir, trule_item *item)
{
	if (item->kind == trule_item_implication) {
		long required = ttypeval_equal(item->term->type, ttypeval_builtin(tbuiltin_bool)) ? 2 : 3;
		if (ir->version < required) ir->version = required;
	}
	if (item->term) trule_ir_collect_term(ir, item->term);
	if (item->rule) trule_ir_collect_term(ir, item->rule);
	for (uint_objs i = 0; i < item->arguments.len; i++) {
		trule_term *argument = as_term(&item->arguments.data[i]);
		if (argument) trule_ir_collect_term(ir, argument);
	}
	tobj value;
	tobj_set_nil(&value);
	tobj_set_compo(&value, (tcompo_v *)item);
	tobj_vec_push(&ir->items, &value);
}

static uint64_t hash_bytes(uint64_t hash, const char *data, size_t length)
{
	for (size_t i = 0; i < length; i++) {
		hash ^= (unsigned char)data[i];
		hash *= 1099511628211ULL;
	}
	return hash;
}

static long serial_term_index(const trule_ir *ir, const trule_term *term);

uint64_t trule_ir_semantic_hash(const trule_ir *ir)
{
	uint64_t hash = 1469598103934665603ULL;
	for (uint_objs i = 0; i < ir->terms.len; i++) {
		trule_term *term = (trule_term *)ir->terms.data[i].val.v_tcompo;
		hash = hash_bytes(hash, trule_term_kind_name(term->kind),
			strlen(trule_term_kind_name(term->kind)));
		hash = hash_bytes(hash, tstring_cstr(term->type->canonical),
			tstring_len(term->type->canonical));
		if (term->kind == trule_term_constant ||
		    term->kind == trule_term_intrinsic ||
		    term->kind == trule_term_call ||
		    term->kind == trule_term_construct ||
		    term->kind == trule_term_convert ||
		    term->kind == trule_term_extension) {
            if (term->kind == trule_term_call && as_term(&term->payload)) {
                long target = serial_term_index(ir, as_term(&term->payload));
                hash ^= (uint64_t)target + 1;
                hash *= 1099511628211ULL;
            } else {
                tstring *payload = tobj_tostring_full(&term->payload);
                hash = hash_bytes(hash, tstring_cstr(payload), tstring_len(payload));
                tstring_free(payload);
            }
		}
		if (tstring_len(term->provider)) {
			hash = hash_bytes(hash, tstring_cstr(term->provider),
				tstring_len(term->provider));
			hash = hash_bytes(hash, tstring_cstr(term->provider_kind),
				tstring_len(term->provider_kind));
			hash ^= (uint64_t)term->provider_version;
			hash *= 1099511628211ULL;
		}
		for (uint_objs j = 0; j < term->arguments.len; j++) {
			trule_term *argument =
				(trule_term *)term->arguments.data[j].val.v_tcompo;
			long position = serial_term_index(ir, argument);
			hash ^= (uint64_t)position + 1;
			hash *= 1099511628211ULL;
		}
	}
	for (uint_objs i = 0; i < ir->items.len; i++) {
		trule_item *item = (trule_item *)ir->items.data[i].val.v_tcompo;
		hash ^= (uint64_t)item->kind + 1;
		hash *= 1099511628211ULL;
		trule_term *root = item->kind == trule_item_requirement ? item->rule : item->term;
		long position = serial_term_index(ir, root);
		hash ^= (uint64_t)(position + 1);
		hash *= 1099511628211ULL;
		for (uint_objs j = 0; j < item->arguments.len; j++) {
			position = serial_term_index(ir,
				(trule_term *)item->arguments.data[j].val.v_tcompo);
			hash ^= (uint64_t)(position + 1);
			hash *= 1099511628211ULL;
		}
	}
	return hash;
}

uint64_t trule_ir_content_hash(const trule_ir *ir)
{
	uint64_t hash = trule_ir_semantic_hash(ir);
	hash = hash_bytes(hash, tstring_cstr(ir->display_name),
		tstring_len(ir->display_name));
	hash = hash_bytes(hash, tstring_cstr(ir->source),
		tstring_len(ir->source));
	for (uint_objs i = 0; i < ir->items.len; i++) {
		trule_item *item = (trule_item *)ir->items.data[i].val.v_tcompo;
		hash = hash_bytes(hash, tstring_cstr(item->description),
			tstring_len(item->description));
	}
	for (uint_objs i = 0; i < ir->parameters.len; i++) {
		trule_term *parameter =
			(trule_term *)ir->parameters.data[i].val.v_tcompo;
		tstring *name = tobj_tostring_full(&parameter->payload);
		hash = hash_bytes(hash, tstring_cstr(name), tstring_len(name));
		tstring_free(name);
	}
	return hash;
}

static void serial_text(tstring *out, const char *text)
{
	size_t length = text ? strlen(text) : 0;
	tstring_append_fmt(out, "%zu:", length);
	if (length) tstring_append(out, text);
}

static long serial_term_index(const trule_ir *ir, const trule_term *term)
{
	for (uint_objs i = 0; i < ir->terms.len; i++)
		if (ir->terms.data[i].val.v_tcompo == (tcompo_v *)term ||
		    (term->kind == trule_term_parameter &&
		     ((trule_term *)ir->terms.data[i].val.v_tcompo)->kind == trule_term_parameter &&
		     ((trule_term *)ir->terms.data[i].val.v_tcompo)->id == term->id))
			return (long)i;
	return -1;
}

static int serial_value(tstring *out, const trule_ir *ir, const tobj *value)
{
	switch (value->type) {
	case tnil: tstring_append(out, "n;"); return 1;
	case tbool: tstring_append_fmt(out, "b%d;", value->val.v_tbool); return 1;
	case tint: tstring_append_fmt(out, "i%ld;", value->val.v_tint); return 1;
	case tfloat:
		tstring_append_fmt(out, "f%.17g;", value->val.v_tfloat);
		return 1;
	case tcompo:
        if (tobj_compo_type(value) == compo_trange) {
            const tdomain *d = (tdomain *)value->val.v_tcompo;
            tstring_append_fmt(out, "g%ld;%ld;", d->start, d->end); return 1;
        }
        if (tobj_compo_type(value) == compo_tpoints) {
            const tdomain *d = (tdomain *)value->val.v_tcompo;
            if (d->item_type->contains_instance) return 0;
            tstring_append_c(out, 'p'); serial_text(out, tstring_cstr(d->item_type->canonical));
            tstring_append_fmt(out, "%u;", (unsigned)d->values.len);
            for (uint_objs i = 0; i < d->values.len; i++) {
                const tobj *v = &d->values.data[i];
                /* Retain the existing transport boundary: no arbitrary object graphs. */
                if (v->type == tcompo && tobj_compo_type(v) != compo_tstr && tobj_compo_type(v) != compo_ttypeval) return 0;
                if (!serial_value(out, ir, v)) return 0;
            }
            return 1;
        }
		if (tobj_compo_type(value) == compo_tstr) {
			tstring_append_c(out, 's');
			serial_text(out, tstring_cstr(
				((tstr *)value->val.v_tcompo)->data));
			return 1;
		}
		if (tobj_compo_type(value) == compo_ttypeval) {
			if (((ttypeval *)value->val.v_tcompo)->contains_instance) return 0;
			tstring_append_c(out, 'y');
			serial_text(out, tstring_cstr(
				((ttypeval *)value->val.v_tcompo)->canonical));
			return 1;
		}
		if (tobj_compo_type(value) == compo_trule_term) {
			long index = serial_term_index(
				ir, (trule_term *)value->val.v_tcompo);
			if (index < 0) return 0;
			tstring_append_fmt(out, "r%ld;", index);
			return 1;
		}
		return 0;
	}
	return 0;
}

int trule_ir_serialize(const trule_ir *ir, tstring **result)
{
	tstring *out = tstring_new(ir->version >= 7 ? "TPIR7;" : ir->version >= 6 ? "TPIR6;" : ir->version == 5 ? "TPIR5;" : ir->version == 4 ? "TPIR4;" : ir->version == 3 ? "TPIR3;" : ir->version == 2 ? "TPIR2;" : "TPIR1;");
	serial_text(out, tstring_cstr(ir->display_name));
	serial_text(out, tstring_cstr(ir->source));
	tstring_append_fmt(out, "%ld;%u;%u;%u;", ir->version,
		(unsigned)ir->terms.len, (unsigned)ir->parameters.len,
		(unsigned)ir->items.len);
	for (uint_objs i = 0; i < ir->terms.len; i++) {
		trule_term *term = (trule_term *)ir->terms.data[i].val.v_tcompo;
		tstring_append_fmt(out, "%d;", (int)term->kind);
		if (term->type->contains_instance) { tstring_free(out); return 0; }
		serial_text(out, tstring_cstr(term->type->canonical));
		if (!serial_value(out, ir, &term->payload)) {
			tstring_free(out);
			return 0;
		}
		tstring_append_fmt(out, "%u;", (unsigned)term->arguments.len);
		for (uint_objs j = 0; j < term->arguments.len; j++) {
			long index = serial_term_index(ir,
				(trule_term *)term->arguments.data[j].val.v_tcompo);
			if (index < 0) { tstring_free(out); return 0; }
			tstring_append_fmt(out, "%ld;", index);
		}
		serial_text(out, tstring_cstr(term->provider));
		serial_text(out, tstring_cstr(term->provider_kind));
		tstring_append_fmt(out, "%ld;%ld;%ld;", term->provider_version,
			term->origin_start, term->origin_end);
	}
	for (uint_objs i = 0; i < ir->parameters.len; i++) {
		long index = serial_term_index(ir,
			(trule_term *)ir->parameters.data[i].val.v_tcompo);
		if (index < 0) { tstring_free(out); return 0; }
		tstring_append_fmt(out, "%ld;", index);
	}
	for (uint_objs i = 0; i < ir->items.len; i++) {
		trule_item *item = (trule_item *)ir->items.data[i].val.v_tcompo;
		long index = serial_term_index(ir,
			item->kind == trule_item_requirement ? item->rule : item->term);
		if (index < 0) { tstring_free(out); return 0; }
		tstring_append_fmt(out, "%d;%ld;%u;", (int)item->kind, index,
			(unsigned)item->arguments.len);
		for (uint_objs j = 0; j < item->arguments.len; j++) {
			long argument = serial_term_index(ir,
				(trule_term *)item->arguments.data[j].val.v_tcompo);
			if (argument < 0) { tstring_free(out); return 0; }
			tstring_append_fmt(out, "%ld;", argument);
		}
		serial_text(out, tstring_cstr(item->description));
		tstring_append_fmt(out, "%ld;%ld;", item->origin_start,
			item->origin_end);
	}
	*result = out;
	return 1;
}

static int parse_long(const char **cursor, long *value)
{
	char *end;
	long parsed = strtol(*cursor, &end, 10);
	if (end == *cursor || *end != ';') return 0;
	*cursor = end + 1;
	*value = parsed;
	return 1;
}

static char *parse_text(const char **cursor)
{
	char *end;
	unsigned long length = strtoul(*cursor, &end, 10);
	if (end == *cursor || *end != ':' || strlen(end + 1) < length)
		return nullptr;
	char *text = calloc(length + 1, 1);
	if (!text) abort();
	memcpy(text, end + 1, length);
	*cursor = end + 1 + length;
	return text;
}

static int parse_value(const char **cursor, trule_term **terms,
		       uint_objs term_count, tobj *result)
{
	char tag = *(*cursor)++;
	tobj_set_nil(result);
    if (tag == 'g') {
        long start, end;
        if (!parse_long(cursor, &start) || !parse_long(cursor, &end) || start > end) return 0;
        tobj_set_compo(result, (tcompo_v *)trange_new(start, end)); return 1;
    }
    if (tag == 'p') {
        char *canonical = parse_text(cursor);
        ttypeval *type = canonical ? ttypeval_retain(ttypeval_from_canonical(canonical)) : nullptr;
        free(canonical);
        long count;
        if (!type || !parse_long(cursor, &count) || count < 0 || (unsigned long)count > strlen(*cursor)) { ttypeval_release(type); return 0; }
        tobj *values = calloc(count + 1, sizeof(*values));
        if (!values) abort();
        int valid = 1;
        for (long i = 0; valid && i < count; i++) {
            if (**cursor != 'n' && **cursor != 'b' && **cursor != 'i' && **cursor != 'f' && **cursor != 's' && **cursor != 'y') valid = 0;
            else valid = parse_value(cursor, terms, term_count, &values[i]) && ttypeval_matches(&values[i], type);
        }
        if (valid) tobj_set_compo(result, (tcompo_v *)tpoints_new(type, values, (uint_regs)count));
        for (long i = 0; i < count; i++) tobj_try_clear(&values[i]);
        free(values); ttypeval_release(type); return valid;
    }
	if (tag == 'n') return *(*cursor)++ == ';';
	if (tag == 's' || tag == 'y') {
		char *text = parse_text(cursor);
		if (!text) return 0;
		if (tag == 's') tobj_set_compo(result, (tcompo_v *)tstr_new(text));
		else {
			ttypeval *type = ttypeval_from_canonical(text);
			if (!type) { free(text); return 0; }
			tobj_set_compo(result, (tcompo_v *)type);
		}
		free(text);
		return 1;
	}
	long value;
	if (tag == 'f') {
		char *end;
		double number = strtod(*cursor, &end);
		if (end == *cursor || *end != ';') return 0;
		*cursor = end + 1;
		tobj_set_float(result, number);
		return 1;
	}
	if (!parse_long(cursor, &value)) return 0;
	if (tag == 'b') tobj_set_bool(result, (int)value);
	else if (tag == 'i') tobj_set_int(result, value);
	else if (tag == 'r' && value >= 0 && (uint_objs)value < term_count)
		tobj_set_compo(result, (tcompo_v *)terms[value]);
	else return 0;
	return 1;
}

trule_ir *trule_ir_deserialize(const char *data)
{
	if (!data || (strncmp(data, "TPIR1;", 6) != 0 && strncmp(data, "TPIR2;", 6) != 0 && strncmp(data, "TPIR3;", 6) != 0 && strncmp(data, "TPIR4;", 6) != 0 && strncmp(data, "TPIR5;", 6) != 0 && strncmp(data, "TPIR6;", 6) != 0 && strncmp(data, "TPIR7;", 6) != 0)) return nullptr;
	const char *cursor = data + 6;
	char *name = parse_text(&cursor);
	char *source = parse_text(&cursor);
	long version;
	long term_count_value;
	long parameter_count_value;
	long item_count_value;
	if (!name || !source || !parse_long(&cursor, &version) ||
	    version != data[4] - '0' ||
	    !parse_long(&cursor, &term_count_value) ||
	    !parse_long(&cursor, &parameter_count_value) ||
	    !parse_long(&cursor, &item_count_value) || term_count_value < 0 ||
	    parameter_count_value < 0 || item_count_value < 0) {
		free(name); free(source); return nullptr;
	}
	uint_objs term_count = (uint_objs)term_count_value;
	trule_term **terms = term_count ? calloc(term_count, sizeof(*terms)) : nullptr;
	trule_ir *ir = trule_ir_new(name, source);
	ir->version = version;
	free(name);
	free(source);
	for (uint_objs i = 0; i < term_count; i++) {
		long kind;
		char *canonical;
		tobj payload;
		long argument_count;
		if (!parse_long(&cursor, &kind) || kind < trule_term_constant ||
		    kind > trule_term_in || (kind == trule_term_in && version < 6) || (kind == trule_term_not && version < 4) ||
		    (kind >= trule_term_and && version < 5) ||
		    !(canonical = parse_text(&cursor)) ||
		    !parse_value(&cursor, terms, i, &payload) ||
		    !parse_long(&cursor, &argument_count) || argument_count < 0)
			goto invalid;
		ttypeval *type = ttypeval_from_canonical(canonical);
		free(canonical);
		if (!type) { tobj_try_clear(&payload); goto invalid; }
		tobj *arguments = argument_count ? calloc(
			(uint_objs)argument_count, sizeof(*arguments)) : nullptr;
		for (long j = 0; j < argument_count; j++) {
			long index;
			if (!parse_long(&cursor, &index) || index < 0 ||
			    (uint_objs)index >= i) { free(arguments); goto invalid; }
			tobj_set_nil(&arguments[j]);
			tobj_set_compo(&arguments[j], (tcompo_v *)terms[index]);
		}
		if (kind == trule_term_in && (argument_count != 2 || payload.type != tnil ||
		    !ttypeval_equal(type, ttypeval_builtin(tbuiltin_bool)))) {
			free(arguments); tobj_try_clear(&payload); goto invalid;
		}
		if (kind == trule_term_not &&
		    (argument_count != 1 || !ttypeval_equal(type, ttypeval_builtin(tbuiltin_bool)) ||
		     payload.type != tnil ||
		     (!trule_antecedent_type(((trule_term *)arguments[0].val.v_tcompo)->type) &&
		      !ttypeval_equal(((trule_term *)arguments[0].val.v_tcompo)->type,
				     ttypeval_builtin(tbuiltin_any))))) {
			free(arguments); tobj_try_clear(&payload); goto invalid;
		}
		if (kind == trule_term_and || kind == trule_term_or) {
			int valid = argument_count == 2 && payload.type == tnil &&
				ttypeval_equal(type, ttypeval_builtin(tbuiltin_bool));
			for (long j = 0; valid && j < argument_count; j++) {
				ttypeval *operand = ((trule_term *)arguments[j].val.v_tcompo)->type;
				valid = trule_antecedent_type(operand) ||
					ttypeval_equal(operand, ttypeval_builtin(tbuiltin_any));
			}
			if (!valid) { free(arguments); tobj_try_clear(&payload); goto invalid; }
		}
		char *provider = parse_text(&cursor);
		char *provider_kind = parse_text(&cursor);
		long provider_version;
		long origin_start;
		long origin_end;
		if (!provider || !provider_kind ||
		    !parse_long(&cursor, &provider_version) ||
		    !parse_long(&cursor, &origin_start) ||
		    !parse_long(&cursor, &origin_end)) {
			free(provider); free(provider_kind); free(arguments); goto invalid;
		}
		terms[i] = trule_term_new((trule_term_kind)kind, type, &payload,
			arguments, (uint_regs)argument_count);
		tobj_try_clear(&payload);
		free(arguments);
		tstring_free(terms[i]->provider);
		tstring_free(terms[i]->provider_kind);
		terms[i]->provider = tstring_new(provider);
		terms[i]->provider_kind = tstring_new(provider_kind);
		terms[i]->provider_version = provider_version;
		terms[i]->origin_start = origin_start;
		terms[i]->origin_end = origin_end;
		free(provider);
		free(provider_kind);
	}
	for (long i = 0; i < parameter_count_value; i++) {
		long index;
		if (!parse_long(&cursor, &index) || index < 0 ||
		    (uint_objs)index >= term_count ||
		    terms[index]->kind != trule_term_parameter) goto invalid;
		trule_ir_add_parameter(ir, terms[index]);
	}
	for (long i = 0; i < item_count_value; i++) {
		long kind;
		long index;
		long argument_count;
		if (!parse_long(&cursor, &kind) || !parse_long(&cursor, &index) ||
		    !parse_long(&cursor, &argument_count) || index < 0 ||
		    (uint_objs)index >= term_count || argument_count < 0 ||
		    kind < trule_item_condition || kind > trule_item_implication ||
	    (kind == trule_item_implication && data[4] == '1')) goto invalid;
		tobj *arguments = argument_count ? calloc(
			(uint_objs)argument_count, sizeof(*arguments)) : nullptr;
		for (long j = 0; j < argument_count; j++) {
			long argument;
			if (!parse_long(&cursor, &argument) || argument < 0 ||
			    (uint_objs)argument >= term_count) {
				free(arguments); goto invalid;
			}
			tobj_set_nil(&arguments[j]);
			tobj_set_compo(&arguments[j], (tcompo_v *)terms[argument]);
		}
		char *description = parse_text(&cursor);
		long start;
		long end;
		if (!description || !parse_long(&cursor, &start) ||
		    !parse_long(&cursor, &end)) {
			free(description); free(arguments); goto invalid;
		}
		if (kind == trule_item_implication) {
			int valid = argument_count > 0 && trule_antecedent_type(terms[index]->type) &&
				(data[4] >= '3' || ttypeval_equal(terms[index]->type, ttypeval_builtin(tbuiltin_bool)));
			for (long j = 0; valid && j < argument_count; j++)
				valid = ttypeval_equal(((trule_term *)arguments[j].val.v_tcompo)->type,
					ttypeval_builtin(tbuiltin_bool));
			if (!valid) { free(description); free(arguments); goto invalid; }
		}
		trule_item *item = kind == trule_item_implication ?
			trule_implication_new(terms[index], arguments, (uint_regs)argument_count, description) :
			kind == trule_item_condition ?
			trule_condition_new(terms[index], description) :
			trule_requirement_new(terms[index], arguments,
				(uint_regs)argument_count);
		trule_term **root = kind == trule_item_requirement ? &item->rule : &item->term;
		if ((*root)->base.refctr > 0) (*root)->base.refctr--;
		if ((*root)->base.refctr == 0) (*root)->base.vtable->free(*root);
		*root = terms[index];
		terms[index]->base.refctr++;
		item->origin_start = start;
		item->origin_end = end;
		trule_ir_add_item(ir, item);
		if (item->base.refctr == 0) item->base.vtable->free(item);
		free(description);
		free(arguments);
	}
	if (*cursor) goto invalid;
	for (uint_objs i = 0; i < term_count; i++)
		if (terms[i]->base.refctr == 0) terms[i]->base.vtable->free(terms[i]);
	free(terms);
	return ir;

invalid:
	for (uint_objs i = 0; i < term_count; i++)
		if (terms[i] && terms[i]->base.refctr == 0)
			terms[i]->base.vtable->free(terms[i]);
	free(terms);
	if (ir->base.refctr == 0) ir->base.vtable->free(ir);
	return nullptr;
}
