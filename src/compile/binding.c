/** Compiler binding metadata, module interfaces, and Type assignability. */
#include "internal.h"

#include <stdlib.h>
#include <string.h>

int tcompile_find_binding(tobj_ctr *c, const tstring *name,
			  tobj_ctr **owner, uint_objs *slot)
{
	for (tobj_ctr *cur = c; cur; cur = cur->father) {
		for (uint_objs i = 0; i < cur->len; i++) {
			if (tstring_eq(cur->bindings[i].name, name)) {
				if (owner)
					*owner = cur;
				if (slot)
					*slot = i;
				return 1;
			}
		}
	}
	return 0;
}

void tcompile_set_metadata(tobj_ctr *c, uint_objs slot,
			   ttypeval *value_type, ttypeval *type_value,
			   int has_annotation)
{
	if (!c || slot >= c->len)
		return;
	ttypeval_release(c->bindings[slot].value_type);
	ttypeval_release(c->bindings[slot].type_value);
	c->bindings[slot].value_type = value_type;
	c->bindings[slot].type_value = type_value;
	c->bindings[slot].has_annotation = has_annotation ? 1 : 0;
	ttypeval_retain(value_type);
	ttypeval_retain(type_value);
}

void tcompile_set_field_order(tobj_ctr *c, uint_objs slot,
			      tstring *const *field_order, uint_objs count)
{
	if (!c || slot >= c->len)
		return;
	tcompile_binding *binding = &c->bindings[slot];
	for (uint_objs i = 0; i < binding->field_order_count; i++)
		tstring_free(binding->field_order[i]);
	free(binding->field_order);
	binding->field_order = NULL;
	binding->field_order_count = 0;
	if (!field_order || count == 0)
		return;
	binding->field_order = (tstring **)calloc(count, sizeof(tstring *));
	if (!binding->field_order)
		abort();
	for (uint_objs i = 0; i < count; i++)
		binding->field_order[i] = tstring_dup(field_order[i]);
	binding->field_order_count = count;
}

void tcompile_module_interface_free(tcompile_module_interface *interface)
{
	if (!interface)
		return;
	for (uint_objs i = 0; i < interface->count; i++) {
		tcompile_export *exported = &interface->exports[i];
		tstring_free(exported->name);
		ttypeval_release(exported->type);
		for (uint_objs j = 0; j < exported->field_order_count; j++)
			tstring_free(exported->field_order[j]);
		free(exported->field_order);
	}
	free(interface->exports);
	free(interface);
}

const tcompile_export *tcompile_module_export(
	const tcompile_module_interface *interface, const char *name)
{
	if (!interface || !name)
		return NULL;
	for (uint_objs i = 0; i < interface->count; i++)
		if (strcmp(tstring_cstr(interface->exports[i].name), name) == 0)
			return &interface->exports[i];
	return NULL;
}

int compile_type_assignable(const ttypeval *actual, const ttypeval *target)
{
	return compile_runtime_types_assignable(actual, target);
}
