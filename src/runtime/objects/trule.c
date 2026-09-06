/**
 * @file trule.c
 * @brief Implements the core Rule and RuleInstance objects.
 * @details Defines their construction, ownership, identity, type information,
 * formatting hooks, and vtable registration.
 * @note Rule evaluation policy and package-specific Rule operations do not
 * belong in this object implementation.
 */
#include "tapas/objects/trule.h"
#include "tapas/dsa/tstring.h"
#include "tapas/objects/trule_ir.h"
#include "tapas/objects/ttype.h"
#include "runtime/tenv.h"
#include "tfunction_metadata.h"
#include "tapas/objects/tstr.h"
#include "tapas/tformat.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *rule_type(void) { return "Rule"; }
static const char *instance_type(void) { return "RuleInstance"; }
static const char *builtin_type(void) { return "Rule Function"; }
static tcompo_type rule_code(void) { return compo_trule; }
static tcompo_type instance_code(void) { return compo_trule_instance; }
static tcompo_type builtin_code(void) { return compo_trule_builtin; }
static long zero_len(void *self) { (void)self; return 0; }
static uint64_t next_rule_identity = 1;
static int pointer_identical(void *self, void *other) { return self == other; }

static void *rule_copy(void *self)
{
	trule *rule = (trule *)self;
	trule *copy = (trule *)calloc(1, sizeof(*copy));
	copy->identity = next_rule_identity++;
	copy->base.vtable = &trule_vtable;
	copy->evaluate_ir = rule->evaluate_ir;
	tobj_copy(&copy->checker, &rule->checker);
	copy->source = tstring_dup(rule->source);
	copy->signature = tstring_dup(rule->signature);
	copy->ir = rule->ir;
	if (copy->ir) copy->ir->base.refctr++;
	return copy;
}

static void rule_free(void *self)
{
	trule *rule = (trule *)self;
	tobj_try_clear(&rule->checker);
	tstring_free(rule->source);
	tstring_free(rule->signature);
	if (rule->ir) {
		if (rule->ir->base.refctr > 0) rule->ir->base.refctr--;
		if (rule->ir->base.refctr == 0) trule_ir_vtable.free(rule->ir);
	}
	free(rule);
}



static void *instance_copy(void *self)
{
	trule_instance *instance = (trule_instance *)self;
	trule_instance *copy = (trule_instance *)calloc(1, sizeof(*copy));
	copy->base.vtable = &trule_instance_vtable;
	tobj_copy(&copy->rule, &instance->rule);
	tobj_vec_copy(&copy->arguments, &instance->arguments);
	return copy;
}

static void instance_free(void *self)
{
	trule_instance *instance = (trule_instance *)self;
	tobj_try_clear(&instance->rule);
	tobj_vec_free(&instance->arguments);
	free(instance);
}



static void *builtin_copy(void *self)
{
	trule_builtin *builtin = (trule_builtin *)self;
	trule_builtin *copy = trule_builtin_new(builtin->kind);
	copy->metadata = tfunction_metadata_retain(builtin->metadata);
	return copy;
}

static void builtin_free(void *self)
{
	tfunction_metadata_release(((trule_builtin *)self)->metadata);
	free(self);
}

static void rule_signature(tformat_context *context, const char *kind,
			   const trule_ir *ir, uint64_t identity)
{
	const char *name = ir ? tstring_cstr(ir->display_name) : nullptr;
	tformat_named(context, kind, name);
	if ((!name || !*name) && identity) {
		char label[32];
		snprintf(label, sizeof(label), " #%" PRIu64, identity);
		tformat_text(context, label);
	}
	tformat_text(context, "[");
	if (ir) {
		for (uint_objs i = 0; i < ir->parameters.len; i++) {
			if (i) tformat_text(context, ", ");
			trule_term *parameter =
				(trule_term *)ir->parameters.data[i].val.v_tcompo;
			tformat_type(context, parameter->type);
		}
	}
	tformat_text(context, "]");
}

static void rule_render(tformat_context *context, const void *self)
{
	const tcompo_v *object = self;
	switch (object->vtable->get_compo_type_code()) {
	case compo_trule: {
		const trule *rule = self;
		rule_signature(context, "Rule", rule->ir, rule->identity);
		break;
	}
	case compo_trule_instance: {
		const trule_instance *instance = self;
		const trule *rule = (trule *)instance->rule.val.v_tcompo;
		tformat_text(context, "RuleInstance[");
		const char *name = rule->ir ?
			tstring_cstr(rule->ir->display_name) : nullptr;
		if (name && *name) {
			tformat_text(context, name);
		} else {
			char label[32];
			snprintf(label, sizeof(label), "#%" PRIu64,
				rule->identity);
			tformat_text(context, label);
		}
		if (instance->arguments.len) tformat_text(context, "; ");
		for (uint_objs i = 0; i < instance->arguments.len; i++) {
			if (i) tformat_text(context, ", ");
			trule_term *parameter = rule->ir &&
				i < rule->ir->parameters.len ?
				(trule_term *)rule->ir->parameters.data[i].val.v_tcompo :
				nullptr;
			const char *parameter_name = parameter &&
				parameter->payload.type == tcompo &&
				tobj_compo_type(&parameter->payload) == compo_tstr ?
				tstring_cstr(((tstr *)parameter->payload.val.v_tcompo)->data) :
				nullptr;
			if (parameter_name) tformat_text(context, parameter_name);
			else {
				tformat_text(context, "p");
				tformat_number(context, (long)i);
			}
			tformat_text(context, "=");
			tformat_value(context, &instance->arguments.data[i]);
		}
		tformat_text(context, "]");
		break;
	}
	case compo_trule_builtin: {
		const trule_builtin *builtin = self;
		const char *name = builtin->kind == trule_builtin_assert ?
			"assert" : "unknown";
		tformat_function_signature(context, name, builtin->metadata);
		break;
	}
	default:
		tformat_text(context, "<invalid Rule object>");
	}
}

static tstring *rule_tostring_abbr(void *self)
{
	return tformat_pointer(self);
}

static tstring *rule_tostring_full(void *self)
{
	return tformat_object(self, 16384, rule_render);
}


tcompo_vtable trule_vtable = {
	.get_type = rule_type, .get_compo_type_code = rule_code,
	.len = zero_len, .copy = rule_copy, .free = rule_free,
	.identical = pointer_identical,
	.tostring_abbr = rule_tostring_abbr, .tostring_full = rule_tostring_full
};

tcompo_vtable trule_instance_vtable = {
	.get_type = instance_type, .get_compo_type_code = instance_code,
	.len = zero_len, .copy = instance_copy, .free = instance_free,
	.identical = pointer_identical,
	.tostring_abbr = rule_tostring_abbr, .tostring_full = rule_tostring_full
};

tcompo_vtable trule_builtin_vtable = {
	.get_type = builtin_type, .get_compo_type_code = builtin_code,
	.len = zero_len, .copy = builtin_copy, .free = builtin_free,
	.identical = pointer_identical,
	.tostring_abbr = rule_tostring_abbr, .tostring_full = rule_tostring_full
};

static char *metadata_field(const char **cursor)
{
	char *end;
	unsigned long length = strtoul(*cursor, &end, 10);
	if (end == *cursor || *end != ':' || strlen(end + 1) < length)
		return nullptr;
	char *field = calloc(length + 1, 1);
	if (!field) abort();
	memcpy(field, end + 1, length);
	*cursor = end + 1 + length;
	return field;
}

static long metadata_number(const char **cursor)
{
	char *text = metadata_field(cursor), *end = nullptr;
	long value = text ? strtol(text, &end, 10) : -1;
	if (!text || !*text || *end || value < 0)
		twarn(ErrRuntime_Other, "RuleIR", "invalid source negation metadata");
	free(text);
	return value;
}

static trule_term *load_source_tree(trule_ir *ir, const char **cursor,
    const char *role, long index, unsigned depth);

/* E encodes a semantic expression, independently of the checker's record slot.
 * Parameter and capture references reuse their declaration nodes. */
static trule_term *load_source_expression(trule_ir *ir, const char **cursor, unsigned depth)
{
    long start = metadata_number(cursor), end = metadata_number(cursor);
    long slot = metadata_number(cursor);
    char *tag = metadata_field(cursor), *text = metadata_field(cursor);
    char *canonical = metadata_field(cursor);
    ttypeval *type = canonical ? ttypeval_retain(ttypeval_from_canonical(canonical)) : nullptr;
    if (!tag || !text || !type) twarn(ErrRuntime_Other, "RuleIR", "invalid expression metadata");
    long count = metadata_number(cursor);
    if (count > 256) twarn(ErrRuntime_Other, "RuleIR", "too many expression operands");
    tobj_vec arguments;
    tobj_vec_init(&arguments);
    for (long i = 0; i < count; i++) {
        trule_term *child = load_source_tree(ir, cursor, "expression-value", slot, depth + 1);
        tobj value = {.type = tcompo, .val.v_tcompo = (tcompo_v *)child};
        tobj_vec_push(&arguments, &value);
    }
    /* A local initializer may be referenced by several later expressions. */
    for (uint_objs i = 0; i < ir->terms.len; i++) {
        trule_term *existing = (trule_term *)ir->terms.data[i].val.v_tcompo;
        if (!strcmp(tstring_cstr(existing->provider), "tapas.source") &&
            !strcmp(tstring_cstr(existing->provider_kind), "expression-value") &&
            existing->provider_version == slot) {
            tobj_vec_free(&arguments);
            ttypeval_release(type);
            free(tag); free(text); free(canonical);
            return existing;
        }
    }
    tobj payload;
    tobj_set_nil(&payload);
    trule_term_kind kind = trule_term_construct;
    if (!strcmp(tag, "int")) { kind = trule_term_constant; tobj_set_int(&payload, strtol(text, nullptr, 10)); }
    else if (!strcmp(tag, "float")) { kind = trule_term_constant; tobj_set_float(&payload, strtod(text, nullptr)); }
    else if (!strcmp(tag, "bool")) { kind = trule_term_constant; tobj_set_bool(&payload, !strcmp(text, "true")); }
    else if (!strcmp(tag, "nil")) kind = trule_term_constant;
    else if (!strcmp(tag, "string")) { kind = trule_term_constant; tobj_set_compo(&payload, (tcompo_v *)tstr_new(text)); }
    else if (!strcmp(tag, "intrinsic")) {
        kind = !strcmp(text, "and") ? trule_term_and : !strcmp(text, "or") ? trule_term_or :
               !strcmp(text, "not") ? trule_term_not : !strcmp(text, "in") ? trule_term_in :
               !strcmp(text, "to") ? trule_term_convert : trule_term_intrinsic;
        if (kind == trule_term_intrinsic || kind == trule_term_convert)
            tobj_set_compo(&payload, (tcompo_v *)tstr_new(text));
    } else if (!strcmp(tag, "call")) {
        if (!arguments.len) twarn(ErrRuntime_Other, "RuleIR", "Call target required");
        kind = trule_term_call;
        tobj_copy(&payload, &arguments.data[0]);
    } else if (!strcmp(tag, "member")) {
        kind = trule_term_intrinsic;
        tobj_set_compo(&payload, (tcompo_v *)tstr_new("member"));
        tobj key = {.type = tcompo, .val.v_tcompo = (tcompo_v *)tstr_new(text)};
        trule_term *field = trule_term_constant_new(&key);
        tobj_try_clear(&key);
        tobj argument = {.type = tcompo, .val.v_tcompo = (tcompo_v *)field};
        tobj_vec_push(&arguments, &argument);
    } else if (!strcmp(tag, "index")) {
        kind = trule_term_intrinsic;
        tobj_set_compo(&payload, (tcompo_v *)tstr_new("index"));
    } else {
        /* Constructors have named semantics; unsupported lexical literals and
         * special references remain explicit Extension nodes, never guessed. */
        if (!strcmp(tag, "opaque") || !strcmp(tag, "reference")) kind = trule_term_extension;
        tstring *operation = tstring_new(tag);
        if (*text) { tstring_append_c(operation, ':'); tstring_append(operation, text); }
        tobj_set_compo(&payload, (tcompo_v *)tstr_new(tstring_cstr(operation)));
        tstring_free(operation);
    }
    uint_regs offset = kind == trule_term_call ? 1 : 0;
    trule_term *term = trule_term_new(kind, type, &payload,
        arguments.len ? arguments.data + offset : nullptr, (uint_regs)(arguments.len - offset));
    /* copy() adds a reference only for the Call target; literals start unowned. */
    if (kind == trule_term_call) tobj_ddc_ref_clear(&payload);
    else tobj_try_clear(&payload);
    tobj_vec_free(&arguments);
    ttypeval_release(type);
    tstring_free(term->provider); tstring_free(term->provider_kind);
    term->provider = tstring_new("tapas.source");
    term->provider_kind = tstring_new("expression-value");
    term->provider_version = slot;
    term->origin_start = start; term->origin_end = end;
    trule_ir_collect_term(ir, term);
    free(tag); free(text); free(canonical);
    return term;
}

static trule_term *load_source_tree(trule_ir *ir, const char **cursor, const char *role,
	long index, unsigned depth)
{
	if (depth > 256) twarn(ErrRuntime_Other, "RuleIR", "source Term nesting limit exceeded");
	char kind = *(*cursor)++;
    if (kind == 'D') {
        long slot = metadata_number(cursor);
        for (uint_objs i = 0; i < ir->terms.len; i++) {
            trule_term *term = (trule_term *)ir->terms.data[i].val.v_tcompo;
            if (!strcmp(tstring_cstr(term->provider), "tapas.source") &&
                !strcmp(tstring_cstr(term->provider_kind), "expression-value") &&
                term->provider_version == slot) return term;
        }
        twarn(ErrRuntime_Other, "RuleIR", "invalid expression reference");
    }
    if (kind == 'E') return load_source_expression(ir, cursor, depth);
    if (kind == 'P') {
        long parameter = metadata_number(cursor);
        if ((uint_objs)parameter >= ir->parameters.len) twarn(ErrRuntime_Other, "RuleIR", "invalid Parameter reference");
        return (trule_term *)ir->parameters.data[parameter].val.v_tcompo;
    }
    if (kind == 'T') {
        long environment_depth = metadata_number(cursor), slot = metadata_number(cursor);
        char *name = metadata_field(cursor);
        long start = metadata_number(cursor), end = metadata_number(cursor);
        if (!name) twarn(ErrRuntime_Other, "RuleIR", "missing tunnel target");
        for (uint_objs i = 0; i < ir->captures.len; i++) {
            trule_term *capture = (trule_term *)ir->captures.data[i].val.v_tcompo;
            if (capture->provider_version == slot &&
                strtol(tstring_cstr(capture->provider_kind), nullptr, 10) == environment_depth) {
                free(name); return capture;
            }
        }
        tobj payload = {.type = tcompo, .val.v_tcompo = (tcompo_v *)tstr_new(name)};
        trule_term *capture = trule_term_new(trule_term_capture, ttypeval_builtin(tbuiltintype_any), &payload, nullptr, 0);
        tobj_try_clear(&payload);
        tstring_free(capture->provider); tstring_free(capture->provider_kind);
        capture->provider = tstring_new("tapas.capture");
        capture->provider_kind = tstring_new_empty();
        tstring_append_fmt(capture->provider_kind, "%ld", environment_depth);
        capture->provider_version = slot;
        capture->origin_start = start; capture->origin_end = end;
        trule_ir_add_capture(ir, capture);
        free(name); return capture;
    }
    if (kind == 'C') {
        long environment_depth = metadata_number(cursor), slot = metadata_number(cursor);
        for (uint_objs i = 0; i < ir->captures.len; i++) {
            trule_term *capture = (trule_term *)ir->captures.data[i].val.v_tcompo;
            if (capture->provider_version == slot &&
                strtol(tstring_cstr(capture->provider_kind), nullptr, 10) == environment_depth) return capture;
        }
        twarn(ErrRuntime_Other, "RuleIR", "invalid Capture reference");
    }
	if (kind != 'N'  && kind != 'A' && kind != 'O' && kind != 'M' && kind != 'S')
		twarn(ErrRuntime_Other, "RuleIR", "invalid source Term metadata");
	long start = metadata_number(cursor), end = metadata_number(cursor);
	trule_term *term;
	if (kind == 'N' || kind == 'A' || kind == 'O' || kind == 'M') {
		index = metadata_number(cursor);
		trule_term *operand = load_source_tree(ir, cursor, "negation-operand",
			kind == 'N' ? index : index + 1, depth + 1);
		if (kind == 'N') term = trule_term_not_new(operand);
		else {
			trule_term *right = load_source_tree(ir, cursor, "negation-operand", index + 2, depth + 1);
			term = kind == 'M' ? trule_term_in_new(operand, right) :
				trule_term_logic_new(kind == 'A' ? trule_term_and : trule_term_or, operand, right);
		}
		role = "negation-value";
	} else {
		char *text = metadata_field(cursor), *canonical = metadata_field(cursor);
		ttypeval *type = canonical ? ttypeval_from_canonical(canonical) : nullptr;
		if (!text || !type) twarn(ErrRuntime_Other, "RuleIR", "invalid source Term Type");
		tobj payload;
		tobj_set_nil(&payload);
		tobj_set_compo(&payload, (tcompo_v *)tstr_new(text));
		term = trule_term_new(trule_term_construct, type, &payload, nullptr, 0);
		tobj_try_clear(&payload);
		free(text); free(canonical);
		long count = metadata_number(cursor);
		if (count > 65535) twarn(ErrRuntime_Other, "RuleIR", "too many source Terms");
		for (long i = 0; i < count; i++) {
			trule_term *child = load_source_tree(ir, cursor, role, index, depth + 1);
			tobj value = { .type = tcompo, .val.v_tcompo = (tcompo_v *)child };
			tobj_vec_push(&term->arguments, &value);
		}
	}
	tstring_free(term->provider); tstring_free(term->provider_kind);
	term->provider = tstring_new("tapas.source");
	term->provider_kind = tstring_new(role);
	term->provider_version = index;
	term->origin_start = start; term->origin_end = end;
	return term;
}

static trule_item *trule_load_item(trule_ir *ir, const char **position, long *item_index)
{
	const char *cursor = *position;
	char kind = *cursor++;
	int tree_metadata = kind == 'c' || kind == 'j' || kind == 'r';
	if (tree_metadata) kind -= ('a' - 'A');
	if (kind != 'C' && kind != 'R' && kind != 'I' && kind != 'J')
		twarn(ErrRuntime_Other, "RuleIR", "unknown source item kind");
	char *description = metadata_field(&cursor);
	char *expression = metadata_field(&cursor);
	char *start_text = metadata_field(&cursor);
	char *end_text = metadata_field(&cursor);
	if (!description || !expression || !start_text || !end_text)
		twarn(ErrRuntime_Other, "RuleIR", "invalid source metadata");
	tobj payload;
	tobj_set_nil(&payload);
	tobj_set_compo(&payload, (tcompo_v *)tstr_new(expression));
	trule_term *term = trule_term_new(trule_term_construct,
		kind != 'R' ? ttypeval_builtin(tbuiltintype_bool) :
		ttypeval_builtin(tbuiltintype_any), &payload, nullptr, 0);
	term->origin_start = strtol(start_text, nullptr, 10);
	term->origin_end = strtol(end_text, nullptr, 10);
	tstring_free(term->provider);
	tstring_free(term->provider_kind);
	term->provider = tstring_new("tapas.source");
	term->provider_kind = tstring_new(
		kind == 'J' ? "antecedent-value" : kind == 'I' ? "antecedent" : kind == 'C' ? "condition" : "requirement");
	term->provider_version = (*item_index)++;
	char *tree_text = tree_metadata ? metadata_field(&cursor) : nullptr;
	if (tree_metadata && !tree_text)
		twarn(ErrRuntime_Other, "RuleIR", "missing source Term tree");
	if (kind == 'J') {
		char *canonical = metadata_field(&cursor);
		ttypeval *type = canonical ? ttypeval_from_canonical(canonical) : nullptr;
		free(canonical);
		if (!trule_antecedent_type(type))
			twarn(ErrRuntime_Other, "RuleIR", "invalid antecedent Type metadata");
		ttypeval_release(term->type);
		term->type = type;
		type->base.refctr++;
		(*item_index)++; /* cached truth record after the raw antecedent */
	}
	if (tree_metadata) {
		const char *tree_cursor = tree_text;
        if (*tree_cursor == 'E' || *tree_cursor == 'P' || *tree_cursor == 'C') ir->version = 7;
		trule_term *root = load_source_tree(ir, &tree_cursor,
			tstring_cstr(term->provider_kind), term->provider_version, 0);
		if (*tree_cursor) twarn(ErrRuntime_Other, "RuleIR", "trailing source Term metadata");
		/* Keep the declared item role Type when an opaque source root has
		 * weaker expression inference (for example an imported callable). */
		if (root->kind == trule_term_construct) {
			ttypeval_release(root->type);
			root->type = ttypeval_retain(term->type);
		}
		term->base.vtable->free(term);
		term = root;
		free(tree_text);
	}
	tobj_try_clear(&payload);
	trule_item *item = kind != 'R' ?
		trule_condition_new(term, description) :
		trule_requirement_new(term, nullptr, 0);
	if (kind == 'R') {
		tstring_free(item->description);
		item->description = tstring_new(description);
	}
	item->origin_start = term->origin_start;
	item->origin_end = term->origin_end;
	if (kind == 'I' || kind == 'J') {
		char *count_text = metadata_field(&cursor);
		char *end = nullptr;
		long count = count_text ? strtol(count_text, &end, 10) : 0;
		if (!count_text || *end || count <= 0 || count > 65535)
			twarn(ErrRuntime_Other, "RuleIR", "invalid implication metadata");
		free(count_text);
		tobj_vec consequents;
		tobj_vec_init(&consequents);
		for (long i = 0; i < count; i++) {
			trule_item *child = trule_load_item(ir, &cursor, item_index);
			if (child->kind != trule_item_condition)
				twarn(ErrRuntime_Other, "RuleIR", "Bool consequent required");
			tobj value = { .type = tcompo, .val.v_tcompo = (tcompo_v *)child->term };
			tobj_vec_push(&consequents, &value);
			child->base.vtable->free(child);
		}
		trule_item *implication = trule_implication_new(term,
			consequents.data, (uint_regs)consequents.len, description);
		implication->origin_start = item->origin_start;
		implication->origin_end = item->origin_end;
		item->base.vtable->free(item);
		item = implication;
		tobj_vec_free(&consequents);
	}
	if (term->base.refctr == 0) term->base.vtable->free(term);
	free(description);
	free(expression);
	free(start_text);
	free(end_text);
	*position = cursor;
	return item;
}

static void trule_load_items(trule *rule, const char *metadata)
{
	const char *cursor = metadata ? metadata : "";
	long item_index = 0;
	while (*cursor) {
		trule_item *item = trule_load_item(rule->ir, &cursor, &item_index);
		trule_ir_add_item(rule->ir, item);
		if (item->base.refctr == 0) item->base.vtable->free(item);
	}
}

static void trule_load_captures(trule *rule, const char *metadata)
{
	const char *cursor = metadata ? metadata : "";
	while (*cursor) {
		char *name = metadata_field(&cursor);
		char *canonical = metadata_field(&cursor);
		char *depth_text = metadata_field(&cursor);
		char *slot_text = metadata_field(&cursor);
		char *start_text = metadata_field(&cursor);
		char *end_text = metadata_field(&cursor);
		if (!name || !canonical || !depth_text || !slot_text ||
		    !start_text || !end_text)
			twarn(ErrRuntime_Other, "RuleIR", "invalid capture metadata");
		ttypeval *type = ttypeval_from_canonical(canonical);
		if (!type) type = ttypeval_builtin(tbuiltintype_any);
		tobj payload;
		tobj_set_nil(&payload);
		tobj_set_compo(&payload, (tcompo_v *)tstr_new(name));
		trule_term *capture = trule_term_new(
			trule_term_capture, type, &payload, nullptr, 0);
		tobj_try_clear(&payload);
		tstring_free(capture->provider);
		tstring_free(capture->provider_kind);
		capture->provider = tstring_new("tapas.capture");
		capture->provider_kind = tstring_new(depth_text);
		capture->provider_version = strtol(slot_text, nullptr, 10);
		capture->origin_start = strtol(start_text, nullptr, 10);
		capture->origin_end = strtol(end_text, nullptr, 10);
		trule_ir_add_capture(rule->ir, capture);
		if (capture->base.refctr == 0)
			capture->base.vtable->free(capture);
		free(name); free(canonical); free(depth_text); free(slot_text);
		free(start_text); free(end_text);
	}
}

trule *trule_new(tfunc *checker, const char *source, const char *signature,
		 const char *parameter_names, const char *item_metadata,
		 const char *capture_metadata)
{
	trule *rule = (trule *)calloc(1, sizeof(*rule));
	rule->identity = next_rule_identity++;
	rule->base.vtable = &trule_vtable;
	tobj_set_nil(&rule->checker);
	if (checker) {
		tobj_set_compo(&rule->checker, (tcompo_v *)checker);
		checker->compo_base.refctr++;
	}
	rule->source = tstring_new(source ? source : "");
	rule->signature = tstring_new(signature ? signature : "");
	rule->ir = trule_ir_new("", source);
	rule->ir->base.refctr++;
	uint_regs count = checker ? checker->env.nparams : 0;
	const char *type_cursor = signature ? signature : "";
	const char *name_cursor = parameter_names ? parameter_names : "";
    const char *separator = strchr(name_cursor, '\x1e');
    if (separator) {
        tstring_free(rule->ir->display_name);
        rule->ir->display_name = tstring_new_len(name_cursor, separator - name_cursor);
        name_cursor = separator + 1;
    }
	for (uint_regs i = 0; i < count; i++) {
		char fallback_name[32];
		const char *name_end = strchr(name_cursor, '\x1f');
		size_t name_length = name_end ? (size_t)(name_end - name_cursor) :
			strlen(name_cursor);
        char *name = calloc(name_length + 1, 1);
        if (!name) abort();
        memcpy(name, name_cursor, name_length);
        if (!name_length) snprintf(fallback_name, sizeof(fallback_name), "parameter%u", (unsigned)i);
		const char *end = strchr(type_cursor, '\x1f');
		size_t length = end ? (size_t)(end - type_cursor) :
			strlen(type_cursor);
		char *canonical = calloc(length + 1, 1);
		if (!canonical) abort();
		memcpy(canonical, type_cursor, length);
		ttypeval *type = ttypeval_from_canonical(canonical);
		free(canonical);
		trule_term *parameter = trule_term_parameter_new(name_length ? name : fallback_name,
			type ? type : ttypeval_builtin(tbuiltintype_any));
		free(name);
		trule_ir_add_parameter(rule->ir, parameter);
		if (parameter->base.refctr == 0) parameter->base.vtable->free(parameter);
		if (end) type_cursor = end + 1;
		if (name_end) name_cursor = name_end + 1;
	}
	trule_load_captures(rule, capture_metadata);
	trule_load_items(rule, item_metadata);
	for (uint_objs i = 0; i < rule->ir->terms.len; i++) {
		trule_term *term =
			(trule_term *)rule->ir->terms.data[i].val.v_tcompo;
		if (term->kind != trule_term_construct ||
		    strcmp(tstring_cstr(term->provider), "tapas.source") != 0 ||
            !strcmp(tstring_cstr(term->provider_kind), "expression-value"))
			continue;
		for (uint_objs j = 0; j < rule->ir->parameters.len; j++)
			tobj_vec_push(&term->arguments,
				&rule->ir->parameters.data[j]);
		for (uint_objs j = 0; j < rule->ir->captures.len; j++)
			tobj_vec_push(&term->arguments, &rule->ir->captures.data[j]);
	}
	return rule;
}

trule *trule_new_dynamic(trule_ir *ir, const char *signature)
{
	trule *rule = trule_new(nullptr, tstring_cstr(ir->source), signature,
		nullptr, nullptr, nullptr);
	rule->evaluate_ir = 1;
	if (rule->ir) {
		if (rule->ir->base.refctr > 0) rule->ir->base.refctr--;
		if (rule->ir->base.refctr == 0) trule_ir_vtable.free(rule->ir);
	}
	rule->ir = ir;
	ir->base.refctr++;
	return rule;
}

trule *trule_new_derived(const trule *source, trule_ir *ir)
{
	trule *rule = trule_new_dynamic(ir, tstring_cstr(source->signature));
	tobj_copy(&rule->checker, &source->checker);
	return rule;
}

int trule_read_capture(const trule *rule, const trule_term *capture, tobj *result)
{
    if (!rule || !capture || capture->kind != trule_term_capture ||
        rule->checker.type != tcompo || tobj_compo_type(&rule->checker) != compo_tfunc) return 0;
    const trule_term *declared = nullptr;
    for (uint_objs i = 0; i < rule->ir->captures.len; i++) {
        const trule_term *candidate = (trule_term *)rule->ir->captures.data[i].val.v_tcompo;
        if (candidate->id == capture->id) declared = candidate;
    }
    if (!declared || strcmp(tstring_cstr(declared->provider), "tapas.capture")) return 0;
    tcompo_env_abstract *env = &((tfunc *)rule->checker.val.v_tcompo)->env.base;
    char *end;
    long depth = strtol(tstring_cstr(declared->provider_kind), &end, 10);
    if (*end || depth <= 0) return 0;
    for (long i = 0; i < depth; i++) {
        env = env->father_env;
        if (!env) return 0;
    }
    if (declared->provider_version < 0 || (uint_objs)declared->provider_version >= env->objs.len) return 0;
    tobj_copy(result, &env->objs.data[declared->provider_version]);
    return 1;
}

void trule_close_over(trule *rule, tcompo_env *environment)
{
	if (rule && rule->checker.type == tcompo &&
	    tobj_compo_type(&rule->checker) == compo_tfunc)
		tfunc_close_over((tfunc *)rule->checker.val.v_tcompo, environment);
}

trule_instance *trule_bind(trule *rule, const tobj *arguments,
			   uint_regs argument_count)
{
	if (rule->ir && argument_count != rule->ir->parameters.len)
		twarn(ErrRuntime_ParamsCtr, "Rule", "incorrect parameter count");
	for (uint_regs i = 0; rule->ir && i < argument_count; i++) {
		trule_term *parameter = (trule_term *)
			rule->ir->parameters.data[i].val.v_tcompo;
		if (!ttypeval_matches(&arguments[i], parameter->type))
			twarn(ErrRuntime_ParamsType, "Rule",
			      "argument does not match parameter Type");
	}
	trule_instance *instance = (trule_instance *)calloc(1, sizeof(*instance));
	instance->base.vtable = &trule_instance_vtable;
	tobj_set_compo(&instance->rule, (tcompo_v *)rule);
	rule->base.refctr++;
	tobj_vec_init(&instance->arguments);
	for (uint_regs i = 0; i < argument_count; i++)
		tobj_vec_push(&instance->arguments, &arguments[i]);
	return instance;
}

trule_builtin *trule_builtin_new(trule_builtin_kind kind)
{
	trule_builtin *builtin = (trule_builtin *)calloc(1, sizeof(*builtin));
	builtin->base.vtable = &trule_builtin_vtable;
	builtin->kind = kind;
	return builtin;
}
