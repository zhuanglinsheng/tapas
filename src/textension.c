#include "tapas/textension.h"

#include <string.h>

static int symbol_valid(const textension_symbol *symbol)
{
	if (!symbol || !symbol->name || !*symbol->name ||
	    !symbol->type || !*symbol->type ||
	    symbol->kind < textension_function ||
	    symbol->kind > textension_type)
		return 0;
	if (symbol->kind == textension_function) {
		if (!!symbol->function == !!symbol->session_function ||
		    symbol->value_factory ||
		    symbol->intrinsic > tnative_intrinsic_type_enum ||
		    symbol->result_relation > tnative_result_argument ||
		    (symbol->result_relation == tnative_result_argument &&
		     symbol->result_argument >= symbol->minimum_arguments))
			return 0;
		return symbol->maximum_arguments == UNDEF_NPARAMS ||
			symbol->minimum_arguments <= symbol->maximum_arguments;
	}
	return !symbol->function && !symbol->session_function &&
		symbol->value_factory &&
		symbol->intrinsic == tnative_intrinsic_none &&
		symbol->result_relation == tnative_result_declared;
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
