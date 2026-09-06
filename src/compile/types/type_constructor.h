#ifndef TAPAS_COMPILE_TYPE_CONSTRUCTOR_H
#define TAPAS_COMPILE_TYPE_CONSTRUCTOR_H

#include "tapas/textension.h"

#include <stdint.h>

typedef enum {
	ttype_constructor_make_type = tnative_intrinsic_type_make,
	ttype_constructor_union = tnative_intrinsic_type_union,
	ttype_constructor_list = tnative_intrinsic_type_list,
	ttype_constructor_iterator = tnative_intrinsic_type_iterator,
	ttype_constructor_optional = tnative_intrinsic_type_optional,
	ttype_constructor_pair = tnative_intrinsic_type_pair,
	ttype_constructor_dictionary = tnative_intrinsic_type_dictionary,
	ttype_constructor_rule = tnative_intrinsic_type_rule,
	ttype_constructor_rule_instance = tnative_intrinsic_type_rule_instance,
	ttype_constructor_enum = tnative_intrinsic_type_enum,
	ttype_constructor_count = tnative_intrinsic_type_enum + 1,
	ttype_constructor_invalid = -1
} ttype_constructor_id;

typedef struct {
	const char *name;
	uint32_t minimum_arguments;
	uint32_t maximum_arguments;
} ttype_constructor;

ttype_constructor_id ttype_constructor_named(const char *name);
const ttype_constructor *ttype_constructor_get(ttype_constructor_id id);

#endif
