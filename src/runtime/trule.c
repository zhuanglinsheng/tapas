#include "tapas/runtime/trule.h"
#include "tapas/runtime/tstr.h"

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
static int pointer_identical(void *self, void *other) { return self == other; }

static void *rule_copy(void *self)
{
	trule *rule = (trule *)self;
	trule *copy = (trule *)calloc(1, sizeof(*copy));
	copy->base.vtable = &trule_vtable;
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

static tstring *rule_string(void *self)
{
	return tobj_tostring_pointer("Rule", self);
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

static tstring *instance_string(void *self)
{
	return tobj_tostring_pointer("RuleInstance", self);
}

static void *builtin_copy(void *self)
{
	trule_builtin *builtin = (trule_builtin *)self;
	return trule_builtin_new(builtin->kind);
}

static void builtin_free(void *self) { free(self); }
static tstring *builtin_string(void *self)
{
	return tobj_tostring_pointer("Rule Function", self);
}

tcompo_vtable trule_vtable = {
	.get_type = rule_type, .get_compo_type_code = rule_code,
	.len = zero_len, .copy = rule_copy, .free = rule_free,
	.identical = pointer_identical,
	.tostring_abbr = rule_string, .tostring_full = rule_string
};

tcompo_vtable trule_instance_vtable = {
	.get_type = instance_type, .get_compo_type_code = instance_code,
	.len = zero_len, .copy = instance_copy, .free = instance_free,
	.identical = pointer_identical,
	.tostring_abbr = instance_string, .tostring_full = instance_string
};

tcompo_vtable trule_builtin_vtable = {
	.get_type = builtin_type, .get_compo_type_code = builtin_code,
	.len = zero_len, .copy = builtin_copy, .free = builtin_free,
	.identical = pointer_identical,
	.tostring_abbr = builtin_string, .tostring_full = builtin_string
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

static void trule_load_items(trule *rule, const char *metadata)
{
	const char *cursor = metadata ? metadata : "";
	long item_index = 0;
	while (*cursor) {
		char kind = *cursor++;
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
			kind == 'C' ? ttypeval_builtin(tbuiltin_bool) :
			ttypeval_builtin(tbuiltin_any), &payload, nullptr, 0);
		term->origin_start = strtol(start_text, nullptr, 10);
		term->origin_end = strtol(end_text, nullptr, 10);
		tstring_free(term->provider);
		tstring_free(term->provider_kind);
		term->provider = tstring_new("tapas.source");
		term->provider_kind = tstring_new(
			kind == 'C' ? "condition" : "requirement");
		term->provider_version = item_index++;
		tobj_try_clear(&payload);
		trule_item *item = kind == 'C' ?
			trule_condition_new(term, description) :
			trule_requirement_new(term, nullptr, 0);
		item->origin_start = term->origin_start;
		item->origin_end = term->origin_end;
		trule_ir_add_item(rule->ir, item);
		if (item->base.refctr == 0) item->base.vtable->free(item);
		if (term->base.refctr == 0) term->base.vtable->free(term);
		free(description);
		free(expression);
		free(start_text);
		free(end_text);
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
		if (!type) type = ttypeval_builtin(tbuiltin_any);
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
	for (uint_regs i = 0; i < count; i++) {
		char name[32];
		const char *name_end = strchr(name_cursor, '\x1f');
		size_t name_length = name_end ? (size_t)(name_end - name_cursor) :
			strlen(name_cursor);
		if (name_length && name_length < sizeof(name)) {
			memcpy(name, name_cursor, name_length);
			name[name_length] = '\0';
		} else
			snprintf(name, sizeof(name), "parameter%u", (unsigned)i);
		const char *end = strchr(type_cursor, '\x1f');
		size_t length = end ? (size_t)(end - type_cursor) :
			strlen(type_cursor);
		char *canonical = calloc(length + 1, 1);
		if (!canonical) abort();
		memcpy(canonical, type_cursor, length);
		ttypeval *type = ttypeval_from_canonical(canonical);
		free(canonical);
		trule_term *parameter = trule_term_parameter_new(name,
			type ? type : ttypeval_builtin(tbuiltin_any));
		trule_ir_add_parameter(rule->ir, parameter);
		if (parameter->base.refctr == 0) parameter->base.vtable->free(parameter);
		if (end) type_cursor = end + 1;
		if (name_end) name_cursor = name_end + 1;
	}
	trule_load_items(rule, item_metadata);
	trule_load_captures(rule, capture_metadata);
	for (uint_objs i = 0; i < rule->ir->terms.len; i++) {
		trule_term *term =
			(trule_term *)rule->ir->terms.data[i].val.v_tcompo;
		if (term->kind != trule_term_construct ||
		    strcmp(tstring_cstr(term->provider), "tapas.source") != 0)
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
	if (rule->ir) {
		if (rule->ir->base.refctr > 0) rule->ir->base.refctr--;
		if (rule->ir->base.refctr == 0) trule_ir_vtable.free(rule->ir);
	}
	rule->ir = ir;
	ir->base.refctr++;
	return rule;
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
