#include "type_constructor.h"

#include "tapas/tstdlib.h"

#include <string.h>

static ttype_constructor constructors[ttype_constructor_count];
static int initialized;

static void initialize_constructors(void)
{
	if (initialized) return;
	const textension_descriptor *stdlib = tstdlib_descriptor();
	for (uint32_t i = 0; i < textension_symbol_count(stdlib); i++) {
		textension_symbol_ref reference;
		if (!textension_symbol_at(stdlib, i, &reference) ||
		    reference.symbol->intrinsic == tnative_intrinsic_none)
			continue;
		uint32_t id = (uint32_t)reference.symbol->intrinsic;
		if (id >= ttype_constructor_count) continue;
		constructors[id] = (ttype_constructor){
			.name = reference.symbol->name,
			.minimum_arguments = reference.symbol->minimum_arguments,
			.maximum_arguments = reference.symbol->maximum_arguments
		};
	}
	initialized = 1;
}

ttype_constructor_id ttype_constructor_named(const char *name)
{
	if (!name) return ttype_constructor_invalid;
	initialize_constructors();
	for (int id = 1; id < ttype_constructor_count; id++)
		if (constructors[id].name &&
		    strcmp(name, constructors[id].name) == 0)
			return (ttype_constructor_id)id;
	return ttype_constructor_invalid;
}

const ttype_constructor *ttype_constructor_get(ttype_constructor_id id)
{
	initialize_constructors();
	return id > 0 && id < ttype_constructor_count ? &constructors[id] : nullptr;
}
