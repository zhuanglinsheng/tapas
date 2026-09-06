/**
 * @file trule_ir.c
 * @brief Implements the core RuleIR object.
 * @details Defines RuleIR construction and aggregate maintenance, lifetime,
 * identity, field access, capabilities, formatting hooks, and vtable
 * registration.
 * @note Keep RuleIR hashing and wire formats out of this file; it contains
 * only the RuleIR object contract.
 */
#include "tapas/objects/trule_ir.h"
#include "tapas/dsa/tstring.h"
#include "tapas/objects/ttype.h"

#include "tapas/tformat.h"
#include "tapas/objects/tdict.h"
#include "tapas/objects/tlist.h"
#include "tapas/objects/tstr.h"

#include <stdlib.h>
#include <string.h>

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

static trule_term *as_term(const tobj *value)
{
	return value && value->type == tcompo && value->val.v_tcompo &&
		tobj_compo_type(value) == compo_trule_term ?
		(trule_term *)value->val.v_tcompo : nullptr;
}

void trule_ir_collect_term(trule_ir *ir, trule_term *term)
{
	if (term->type->contains_custom && ir->version < 6) ir->version = 6;
	if (term->kind == trule_term_in && ir->version < 6) ir->version = 6;
	if (term->kind == trule_term_not && ir->version < 4) ir->version = 4;
	if ((term->kind == trule_term_and || term->kind == trule_term_or) &&
	    ir->version < 5)
		ir->version = 5;
	for (uint_objs i = 0; i < ir->terms.len; i++)
		if (ir->terms.data[i].val.v_tcompo == (tcompo_v *)term ||
		    (term->kind == trule_term_parameter &&
		     ((trule_term *)ir->terms.data[i].val.v_tcompo)->kind ==
			trule_term_parameter &&
		     ((trule_term *)ir->terms.data[i].val.v_tcompo)->id ==
			term->id))
			return;
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
		long required = ttypeval_equal(item->term->type,
			ttypeval_builtin(tbuiltintype_bool)) ? 2 : 3;
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

/*----------------------- Required Vtable Operations -----------------------*/

static const char *ir_type(void) { return "RuleIR"; }
static tcompo_type ir_code(void) { return compo_trule_ir; }
static long ir_len(void *self) { (void)self; return 0; }

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

static int ir_identical(void *self, void *other)
{
	return self == other;
}

static void ir_declarations(tformat_context *context, const tobj_vec *terms)
{
	tformat_text(context, "[");
	for (uint_objs i = 0; i < terms->len; i++) {
		if (i) tformat_text(context, ", ");
		const trule_term *term =
			(trule_term *)terms->data[i].val.v_tcompo;
		const char *name = term->payload.type == tcompo &&
			tobj_compo_type(&term->payload) == compo_tstr ?
			tstring_cstr(((tstr *)term->payload.val.v_tcompo)->data) :
			nullptr;
		if (name && *name) {
			tformat_text(context, name);
		} else {
			tformat_text(context, "#");
			tformat_number(context, (long)term->id);
		}
		tformat_text(context, ": ");
		tformat_type(context, term->type);
	}
	tformat_text(context, "]");
}

static void ir_render(tformat_context *context, const void *self)
{
	const trule_ir *ir = self;
	tformat_named(context, "RuleIR", tstring_cstr(ir->display_name));
	tformat_text(context, " {\n  parameters: ");
	ir_declarations(context, &ir->parameters);
	tformat_text(context, ",\n  captures: ");
	ir_declarations(context, &ir->captures);
	tformat_text(context, ",\n  conditions: [");
	for (uint_objs i = 0; i < ir->items.len; i++) {
		tformat_text(context, i ? ",\n    " : "\n    ");
		const trule_item *item =
			(trule_item *)ir->items.data[i].val.v_tcompo;
		if (item->kind == trule_item_condition) {
			tobj term = {
				.type = tcompo,
				.val.v_tcompo = (tcompo_v *)item->term
			};
			tformat_value(context, &term);
		} else {
			tformat_value(context, &ir->items.data[i]);
		}
	}
	tformat_text(context, ir->items.len ? "\n  ]\n}" : "]\n}");
}

static tstring *ir_tostring_abbr(void *self)
{
	return tformat_pointer(self);
}

static tstring *ir_tostring_full(void *self)
{
	return tformat_object(self, 16384, ir_render);
}

/*------------------------------ Capabilities ------------------------------*/

static const char *index_name(const tobj *arguments, uint_regs count)
{
	if (count != 1 || arguments[0].type != tcompo ||
	    tobj_compo_type(&arguments[0]) != compo_tstr)
		twarn(ErrRuntime_ParamsType, "RuleIR", "String field required");
	return tstring_cstr(((tstr *)arguments[0].val.v_tcompo)->data);
}

static tlist *vector_snapshot(const tobj_vec *values)
{
	tlist *list = tlist_new();
	for (uint_objs i = 0; i < values->len; i++)
		tobj_vec_push(&list->items, &values->data[i]);
	return list;
}

static void ir_index(void *self, const tobj *arguments, uint_regs count, tobj *result)
{
	trule_ir *ir = self;
	const char *name = index_name(arguments, count);
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

static const tcompo_capabilities ir_capabilities = {
	.indexable = ir_index
};

tcompo_vtable trule_ir_vtable = {
	.get_type = ir_type,
	.get_compo_type_code = ir_code,
	.len = ir_len,
	.copy = ir_copy,
	.free = ir_free,
	.identical = ir_identical,
	.tostring_abbr = ir_tostring_abbr,
	.tostring_full = ir_tostring_full,
	.capabilities = &ir_capabilities
};
