/**
 * @file trule_item.c
 * @brief Implements the core RuleItem object.
 * @details Defines RuleItem construction and validation, lifetime, identity,
 * field access, capabilities, formatting hooks, and vtable registration.
 * @note Keep RuleIR aggregation, hashing, and wire formats out of this file;
 * it contains only the RuleItem object contract.
 */
#include "tapas/objects/trule_ir.h"
#include "tapas/dsa/tstring.h"
#include "tapas/objects/ttype.h"

#include "tapas/tformat.h"
#include "tapas/objects/tlist.h"
#include "tapas/objects/tstr.h"

#include <stdlib.h>
#include <string.h>

static trule_term *as_term(const tobj *value)
{
	return value && value->type == tcompo && value->val.v_tcompo &&
		tobj_compo_type(value) == compo_trule_term ?
		(trule_term *)value->val.v_tcompo : nullptr;
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

static trule_item *item_new(trule_item_kind kind, trule_term *primary,
			    const char *description)
{
	trule_item *item = calloc(1, sizeof(*item));
	if (!item) abort();
	item->base.vtable = &trule_item_vtable;
	item->kind = kind;
	if (kind == trule_item_requirement)
		item->rule = term_detach(primary);
	else
		item->term = term_detach(primary);
	primary = kind == trule_item_requirement ? item->rule : item->term;
	primary->base.refctr++;
	tobj_vec_init(&item->arguments);
	item->description = tstring_new(description ? description : "");
	item->origin_start = -1;
	item->origin_end = -1;
	return item;
}

trule_item *trule_condition_new(trule_term *term, const char *description)
{
	return item_new(trule_item_condition, term, description);
}

trule_item *trule_implication_new(trule_term *antecedent,
	const tobj *consequents, uint_regs count, const char *description)
{
	if (!antecedent || !trule_antecedent_type(antecedent->type) || !count)
		twarn(ErrRuntime_ParamsType, "implication",
		      "Bool or RuleInstance antecedent and nonempty consequents required");
	trule_item *item = item_new(trule_item_implication, antecedent,
		description);
	for (uint_regs i = 0; i < count; i++) {
		trule_term *term = as_term(&consequents[i]);
		if (!term || !ttypeval_equal(term->type,
		    ttypeval_builtin(tbuiltintype_bool)))
			twarn(ErrRuntime_ParamsType, "implication",
			      "Bool consequent Terms required");
		trule_term *copy = term_detach(term);
		tobj value = {
			.type = tcompo,
			.val.v_tcompo = (tcompo_v *)copy
		};
		tobj_vec_push(&item->arguments, &value);
	}
	return item;
}

trule_item *trule_requirement_new(trule_term *rule,
				  const tobj *arguments,
				  uint_regs argument_count)
{
	trule_item *item = item_new(trule_item_requirement, rule, "");
	for (uint_regs i = 0; i < argument_count; i++)
		tobj_vec_push(&item->arguments, &arguments[i]);
	return item;
}

/*----------------------- Required Vtable Operations -----------------------*/

static const char *item_type(void) { return "RuleItem"; }
static tcompo_type item_code(void) { return compo_trule_item; }
static long item_len(void *self) { (void)self; return 0; }

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

static int item_identical(void *self, void *other) { return self == other; }

static void item_render(tformat_context *context, const void *self)
{
	const trule_item *item = self;
	const char *name = item->kind == trule_item_condition ? "Condition" :
		item->kind == trule_item_requirement ? "Requirement" :
		"Implication";
	if (tformat_is_root(context)) tformat_text(context, "RuleItem ");
	tformat_text(context, name);
	tformat_text(context, "[");
	if (item->kind == trule_item_requirement) {
		tobj rule = {
			.type = tcompo,
			.val.v_tcompo = (tcompo_v *)item->rule
		};
		tformat_value(context, &rule);
		tformat_text(context, ", arguments=[");
		tformat_sequence(context, &item->arguments);
		tformat_text(context, "]");
	} else {
		tobj term = {
			.type = tcompo,
			.val.v_tcompo = (tcompo_v *)item->term
		};
		tformat_value(context, &term);
		if (item->kind == trule_item_implication) {
			tformat_text(context, " implies [");
			tformat_sequence(context, &item->arguments);
			tformat_text(context, "]");
		}
	}
	if (tstring_len(item->description)) {
		tformat_text(context, ", description=");
		tformat_quoted(context, tstring_cstr(item->description));
	}
	tformat_text(context, "]");
}

static tstring *item_tostring_abbr(void *self)
{
	return tformat_pointer(self);
}

static tstring *item_tostring_full(void *self)
{
	return tformat_object(self, 16384, item_render);
}

/*------------------------------ Capabilities ------------------------------*/

static const char *index_name(const tobj *arguments, uint_regs count)
{
	if (count != 1 || arguments[0].type != tcompo ||
	    tobj_compo_type(&arguments[0]) != compo_tstr)
		twarn(ErrRuntime_ParamsType, "RuleItem", "String field required");
	return tstring_cstr(((tstr *)arguments[0].val.v_tcompo)->data);
}

static tlist *vector_snapshot(const tobj_vec *values)
{
	tlist *list = tlist_new();
	for (uint_objs i = 0; i < values->len; i++)
		tobj_vec_push(&list->items, &values->data[i]);
	return list;
}

static void item_index(void *self, const tobj *arguments, uint_regs count,
		       tobj *result)
{
	trule_item *item = self;
	const char *name = index_name(arguments, count);
	if (strcmp(name, "kind") == 0)
		tobj_set_compo(result, (tcompo_v *)tstr_new(
			item->kind == trule_item_condition ? "Condition" :
			item->kind == trule_item_implication ? "Implication" : "Requirement"));
	else if (strcmp(name, "antecedent") == 0 && item->kind == trule_item_implication)
		tobj_copy(result, &(tobj){ .type = tcompo,
			.val.v_tcompo = (tcompo_v *)item->term });
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

static const tcompo_capabilities item_capabilities = {
	.indexable = item_index
};

tcompo_vtable trule_item_vtable = {
	.get_type = item_type,
	.get_compo_type_code = item_code,
	.len = item_len,
	.copy = item_copy,
	.free = item_free,
	.identical = item_identical,
	.tostring_abbr = item_tostring_abbr,
	.tostring_full = item_tostring_full,
	.capabilities = &item_capabilities
};
