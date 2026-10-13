#include "type_constructor.h"
#include "compile/frontend/module.h"

#include <string.h>

ttype_constructor_id ttype_constructor_named(
	tstandard_catalog *catalog, const char *name)
{
	if (!catalog || !name) return ttype_constructor_invalid;
	(void)tstandard_symbol_count(catalog);
	for (int id = 1; id < ttype_constructor_count; id++)
		if (catalog->constructors[id].name &&
		    strcmp(name, catalog->constructors[id].name) == 0)
			return (ttype_constructor_id)id;
	return ttype_constructor_invalid;
}

const ttype_constructor *ttype_constructor_get(
	tstandard_catalog *catalog, ttype_constructor_id id)
{
	if (!catalog) return nullptr;
	(void)tstandard_symbol_count(catalog);
	return id > 0 && id < ttype_constructor_count ?
		&catalog->constructors[id] : nullptr;
}
