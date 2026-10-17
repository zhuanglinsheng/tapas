/**
 * @file textension.c
 * @brief Validates and queries native C extension descriptors.
 * @details Enforces symbol, callable, nominal template, dependent result, and
 * namespace invariants before an extension is installed.
 * @note Validation concerns the public ABI only and must not depend on any
 * standard package's private implementation.
 */
#include "tapas/textension.h"
#include "tapas/tval.h"

#include <stdlib.h>
#include <string.h>

/* One mapping serves runtime Type matching and static extension metadata. */
#define CAPABILITY_RULES(X) \
	X(indexable, indexable) \
	X(index_settable, index_settable) \
	X(appendable, appendable) \
	X(deletable, deletable) \
	X(contains, member_testable) \
	X(iterable, iterable)

uint32_t textension_capability_flag(tbuiltintype_id type)
{
	switch (type) {
#define FLAG(field, builtin) case tbuiltintype_##builtin: return textension_type_##field;
	CAPABILITY_RULES(FLAG)
#undef FLAG
	default: return 0;
	}
}

uint32_t textension_capability_mask(const tcompo_capabilities *capabilities)
{
	if (!capabilities)
		return 0;
	uint32_t mask = 0;
#define MASK(field, builtin) if (capabilities->field) mask |= textension_type_##field;
	CAPABILITY_RULES(MASK)
#undef MASK
	return mask;
}

int textension_capability_matches(const tcompo_capabilities *capabilities,
				 tbuiltintype_id type)
{
	if (!capabilities)
		return 0;
	switch (type) {
#define MATCH(field, builtin) case tbuiltintype_##builtin: return capabilities->field != nullptr;
	CAPABILITY_RULES(MATCH)
#undef MATCH
	default: return 0;
	}
}

#undef CAPABILITY_RULES

int textension_symbol_capabilities(const textension_symbol *symbol, uint32_t *mask)
{
	if (!symbol || !mask || symbol->kind != textension_type)
		return 0;
	if (symbol->instance_capabilities) {
		*mask = textension_capability_mask(symbol->instance_capabilities);
		return 1;
	}
	if (symbol->nominal_template) {
		*mask = symbol->nominal_template->capabilities;
		return 1;
	}
	return 0;
}

struct textension_registry {
	const textension_descriptor **extensions;
	uint32_t count;
	uint32_t capacity;
	uint64_t revision;
};

static int same_namespace(const textension_module *left,
			  const textension_module *right);

static int modules_conflict(const textension_module *left,
			    const textension_module *right)
{
	if (left->scope == textension_root && right->scope == textension_package) {
		for (uint32_t i = 0; i < left->symbol_count; i++)
			if (!strcmp(left->symbols[i].name, right->name)) return 1;
		return 0;
	}
	if (left->scope == textension_package && right->scope == textension_root)
		return modules_conflict(right, left);
	if (!same_namespace(left, right)) return 0;
	for (uint32_t i = 0; i < left->symbol_count; i++)
		for (uint32_t j = 0; j < right->symbol_count; j++)
			if (!strcmp(left->symbols[i].name, right->symbols[j].name))
				return 1;
	return 0;
}

static int nominal_template_valid(const textension_nominal_template *schema)
{
	if (!schema || !schema->identity || !strstr(schema->identity, "::") ||
	    (!schema->type_parameters && schema->type_parameter_count) ||
	    (!schema->value_parameters && schema->value_parameter_count) ||
	    (!schema->type_parameter_count && !schema->value_parameter_count) ||
	    (schema->capabilities & ~(textension_type_indexable |
		textension_type_contains | textension_type_iterable |
		textension_type_index_settable | textension_type_appendable |
		textension_type_deletable)))
		return 0;
	uint_regs count = schema->type_parameter_count +
		schema->value_parameter_count;
	for (uint_regs i = 0; i < count; i++) {
		const char *name;
		const char *slot;
		const char *constraint = nullptr;
		if (i < schema->type_parameter_count) {
			name = schema->type_parameters[i].name;
			slot = schema->type_parameters[i].slot;
		} else {
			const textension_template_value_parameter *parameter =
				&schema->value_parameters[i - schema->type_parameter_count];
			name = parameter->name;
			slot = parameter->slot;
			constraint = parameter->constraint;
		}
		if (!name || !*name || !slot || !*slot ||
		    (i >= schema->type_parameter_count &&
		     (!constraint || !*constraint)))
			return 0;
		for (uint_regs j = 0; j < i; j++) {
			const char *earlier_name;
			const char *earlier_slot;
			if (j < schema->type_parameter_count) {
				earlier_name = schema->type_parameters[j].name;
				earlier_slot = schema->type_parameters[j].slot;
			} else {
				const textension_template_value_parameter *earlier =
					&schema->value_parameters[j - schema->type_parameter_count];
				earlier_name = earlier->name;
				earlier_slot = earlier->slot;
			}
			if (!strcmp(name, earlier_name) || !strcmp(slot, earlier_slot))
				return 0;
		}
	}
	return 1;
}

static int symbol_valid(const textension_symbol *symbol)
{
	if (!symbol || !symbol->name || !*symbol->name ||
	    !symbol->type || !*symbol->type ||
	    symbol->kind < textension_function ||
	    symbol->kind > textension_type)
		return 0;
	if (symbol->instance_capabilities && symbol->kind != textension_type)
		return 0;
	/* A template's runtime Type and the tooling metadata must describe the
	 * same capabilities. Reject conflicting declarations before installation. */
	if (symbol->instance_capabilities && symbol->nominal_template &&
	    textension_capability_mask(symbol->instance_capabilities) !=
		    symbol->nominal_template->capabilities)
		return 0;
	if (symbol->kind == textension_function) {
		if (!!symbol->function == !!symbol->session_function ||
		    symbol->value_factory ||
		    symbol->nominal_template ||
		    symbol->intrinsic > tnative_intrinsic_type_template ||
		    symbol->result_relation > tnative_result_template_argument ||
		    (symbol->result_relation != tnative_result_declared &&
		     symbol->result_argument >= symbol->minimum_arguments) ||
		    (symbol->following_arguments_match_result_parameter &&
		     symbol->result_relation != tnative_result_template_argument) ||
		    (symbol->result_relation == tnative_result_template_argument ?
		     !nominal_template_valid(symbol->result_template) :
		     !!symbol->result_template))
			return 0;
		return symbol->maximum_arguments == UNDEF_NPARAMS ||
			symbol->minimum_arguments <= symbol->maximum_arguments;
	}
	return !symbol->function && !symbol->session_function &&
		symbol->value_factory &&
		symbol->intrinsic == tnative_intrinsic_none &&
		symbol->result_relation == tnative_result_declared &&
		(symbol->kind == textension_type ?
			(!symbol->nominal_template ||
			 nominal_template_valid(symbol->nominal_template)) :
			!symbol->nominal_template) &&
		!symbol->result_template &&
		!symbol->following_arguments_match_result_parameter &&
		!symbol->compile_time_signature;
}

static int same_namespace(const textension_module *left,
			  const textension_module *right)
{
	return left->scope == right->scope &&
		(left->scope == textension_root ||
		 strcmp(left->name, right->name) == 0);
}

int textension_validate(const textension_descriptor *extension)
{
	if (!extension || extension->abi_version != TAPAS_EXTENSION_ABI ||
	    extension->structure_size < sizeof(*extension) ||
	    !extension->name || !*extension->name ||
	    (!extension->modules && extension->module_count))
		return 0;
	for (uint32_t i = 0; i < extension->module_count; i++) {
		const textension_module *module = extension->modules[i];
		if (!module ||
		    (module->scope != textension_root &&
		     module->scope != textension_package) ||
		    (module->scope == textension_package &&
		     (!module->name || !*module->name)) ||
		    (module->scope == textension_root && module->name) ||
		    (!module->symbols && module->symbol_count))
			return 0;
		for (uint32_t j = 0; j < module->symbol_count; j++)
			if (!symbol_valid(&module->symbols[j]))
				return 0;
		for (uint32_t j = 0; j < i; j++) {
			const textension_module *earlier = extension->modules[j];
			if (!earlier) continue;
			if (module->scope == textension_package &&
			    earlier->scope == textension_root)
				for (uint32_t k = 0; k < earlier->symbol_count; k++)
					if (strcmp(module->name,
					    earlier->symbols[k].name) == 0)
						return 0;
			if (module->scope == textension_root &&
			    earlier->scope == textension_package)
				for (uint32_t k = 0; k < module->symbol_count; k++)
					if (strcmp(earlier->name,
					    module->symbols[k].name) == 0)
						return 0;
			if (!same_namespace(module, earlier)) continue;
			for (uint32_t k = 0; k < module->symbol_count; k++)
				for (uint32_t l = 0; l < earlier->symbol_count; l++)
					if (strcmp(module->symbols[k].name,
					    earlier->symbols[l].name) == 0)
						return 0;
		}
		for (uint32_t j = 0; j < module->symbol_count; j++)
			for (uint32_t k = 0; k < j; k++)
				if (strcmp(module->symbols[j].name,
				    module->symbols[k].name) == 0)
					return 0;
	}
	return 1;
}

textension_registry *textension_registry_new(void)
{
	return (textension_registry *)calloc(1, sizeof(textension_registry));
}

void textension_registry_free(textension_registry *registry)
{
	if (!registry) return;
	free(registry->extensions);
	free(registry);
}

static int extensions_have_intrinsic_conflict(
	const textension_descriptor *left, const textension_descriptor *right)
{
	for (uint32_t i = 0; i < textension_symbol_count(left); i++) {
		textension_symbol_ref a;
		if (!textension_symbol_at(left, i, &a) ||
		    a.symbol->intrinsic == tnative_intrinsic_none) continue;
		for (uint32_t j = 0; j < textension_symbol_count(right); j++) {
			textension_symbol_ref b;
			if (textension_symbol_at(right, j, &b) &&
			    b.symbol->intrinsic == a.symbol->intrinsic)
				return 1;
		}
	}
	return 0;
}

int textension_registry_accepts(const textension_registry *registry,
				const textension_descriptor *extension)
{
	if (!registry || !textension_validate(extension)) return 0;
	for (uint32_t i = 0; i < registry->count; i++) {
		const textension_descriptor *registered = registry->extensions[i];
		if (registered == extension) return 1;
		if (!strcmp(registered->name, extension->name) ||
		    extensions_have_intrinsic_conflict(registered, extension))
			return 0;
		for (uint32_t left = 0; left < registered->module_count; left++)
			for (uint32_t right = 0; right < extension->module_count; right++)
				if (modules_conflict(registered->modules[left],
						     extension->modules[right]))
					return 0;
	}
	return 1;
}

int textension_registry_register(textension_registry *registry,
				 const textension_descriptor *extension)
{
	if (!textension_registry_accepts(registry, extension)) return 0;
	for (uint32_t i = 0; i < registry->count; i++)
		if (registry->extensions[i] == extension) return 1;
	if (registry->count == registry->capacity) {
		uint32_t capacity = registry->capacity ? registry->capacity * 2 : 4;
		const textension_descriptor **grown = realloc(registry->extensions,
			capacity * sizeof(*grown));
		if (!grown) abort();
		registry->extensions = grown;
		registry->capacity = capacity;
	}
	registry->extensions[registry->count++] = extension;
	registry->revision++;
	return 1;
}

uint32_t textension_registry_count(const textension_registry *registry)
{
	return registry ? registry->count : 0;
}

const textension_descriptor *textension_registry_at(
	const textension_registry *registry, uint32_t index)
{
	return registry && index < registry->count ?
		registry->extensions[index] : nullptr;
}

uint64_t textension_registry_revision(const textension_registry *registry)
{
	return registry ? registry->revision : 0;
}

uint32_t textension_symbol_count(const textension_descriptor *extension)
{
	if (!extension) return 0;
	uint32_t count = 0;
	for (uint32_t i = 0; i < extension->module_count; i++)
		if (extension->modules[i])
			count += extension->modules[i]->symbol_count;
	return count;
}

int textension_symbol_at(const textension_descriptor *extension, uint32_t index,
			 textension_symbol_ref *result)
{
	if (result) *result = (textension_symbol_ref){ 0 };
	if (!extension) return 0;
	for (uint32_t i = 0; i < extension->module_count; i++) {
		const textension_module *module = extension->modules[i];
		if (!module) continue;
		if (index < module->symbol_count) {
			if (result) *result = (textension_symbol_ref){
				.module = module, .symbol = &module->symbols[index]
			};
			return 1;
		}
		index -= module->symbol_count;
	}
	return 0;
}

int textension_find(const textension_descriptor *extension,
		     const char *package, const char *name,
		     textension_symbol_ref *result)
{
	if (result) *result = (textension_symbol_ref){ 0 };
	if (!extension || !name) return 0;
	for (uint32_t i = 0; i < extension->module_count; i++) {
		const textension_module *module = extension->modules[i];
		if (!module) continue;
		if ((package && (module->scope != textension_package ||
		    strcmp(package, module->name) != 0)) ||
		    (!package && module->scope != textension_root))
			continue;
		for (uint32_t j = 0; j < module->symbol_count; j++)
			if (strcmp(name, module->symbols[j].name) == 0) {
				if (result) *result = (textension_symbol_ref){
					.module = module,
					.symbol = &module->symbols[j]
				};
				return 1;
			}
	}
	return 0;
}
