#ifndef TAPAS_COMPILE_STATIC_TYPE_H
#define TAPAS_COMPILE_STATIC_TYPE_H

#include "tapas/ds/tstring.h"
#include "tapas/tbuiltin.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t tstatic_type_id;
#define TSTATIC_TYPE_UNKNOWN UINT32_MAX
#define TSTATIC_TYPE_INVALID_NAME (UINT32_MAX - 1)

typedef enum {
	tstatic_type_builtin,
	tstatic_type_list,
	tstatic_type_iterator,
	tstatic_type_pair,
	tstatic_type_dictionary,
	tstatic_type_function,
	tstatic_type_rule,
	tstatic_type_rule_instance,
	tstatic_type_union,
	tstatic_type_enum,
	tstatic_type_fields,
	tstatic_type_recursive,
	tstatic_type_instance_of,
	tstatic_type_points,
	tstatic_type_range
} tstatic_type_kind;

typedef struct {
	tstatic_type_kind kind;
	tbuiltin_id builtin;
	uint32_t children;
	uint32_t child_count;
	uint32_t fields;
	uint32_t field_count;
	uint8_t variadic;
	tstring *value_reference;
	/* Presentation provenance of this occurrence, never part of Type equality. */
	tstring *display_name;
	tstatic_type_id display_definition;
} tstatic_type;

typedef struct {
	tstring *name;
	tstatic_type_id type;
	uint8_t optional;
} tstatic_field;

typedef struct {
	tstatic_type *types;
	uint32_t type_count;
	uint32_t type_capacity;
	tstatic_type_id *children;
	uint32_t child_count;
	uint32_t child_capacity;
	tstatic_field *fields;
	uint32_t field_count;
	uint32_t field_capacity;
} tstatic_type_arena;

typedef tstatic_type_id (*tstatic_type_resolver)(void *context,
						 const char *qualified_name);

typedef tstatic_type_id (*tstatic_value_type_resolver)(void *context, const char *name);
tstatic_type_id tstatic_type_parse_with_values(tstatic_type_arena *arena,
	const char *text, tstatic_type_resolver resolver,
	tstatic_value_type_resolver value_resolver, void *context);

void tstatic_type_arena_init(tstatic_type_arena *arena);
void tstatic_type_arena_free(tstatic_type_arena *arena);
tstatic_type_id tstatic_type_builtin_id(tstatic_type_arena *arena,
						tbuiltin_id builtin);
tstatic_type_id tstatic_type_builtin_named(tstatic_type_arena *arena,
					   const char *name);
tstatic_type_id tstatic_type_make(tstatic_type_arena *arena,
				  tstatic_type_kind kind,
				  const tstatic_type_id *children,
				  uint32_t child_count, int variadic);
tstatic_type_id tstatic_type_make_fields(tstatic_type_arena *arena,
					 const tstatic_field *fields,
					 uint32_t field_count);
tstatic_type_id tstatic_type_make_enum(tstatic_type_arena *arena,
				       tstring *const *members,
				       uint32_t member_count);
tstatic_type_id tstatic_type_make_recursive(tstatic_type_arena *arena);
int tstatic_type_define_recursive(tstatic_type_arena *arena,
				  tstatic_type_id recursive,
				  tstatic_type_id body);
const tstatic_type *tstatic_type_get(const tstatic_type_arena *arena,
				    tstatic_type_id id);
const tstatic_type_id *tstatic_type_children(const tstatic_type_arena *arena,
					      const tstatic_type *type);
const tstatic_field *tstatic_type_field_items(const tstatic_type_arena *arena,
					      const tstatic_type *type);
uint32_t tstatic_type_enum_member_count(const tstatic_type *type);
const tstring *tstatic_type_enum_member_at(const tstatic_type_arena *arena,
					   const tstatic_type *type,
					   uint32_t index);
int tstatic_type_enum_contains(const tstatic_type_arena *arena,
			       const tstatic_type *type, const tstring *member);
tstatic_type_id tstatic_type_parse(tstatic_type_arena *arena,
				   const char *text,
				   tstatic_type_resolver resolver,
				   void *context);
int tstatic_type_equal(const tstatic_type_arena *arena,
			       tstatic_type_id left, tstatic_type_id right);
int tstatic_type_assignable(const tstatic_type_arena *arena,
				    tstatic_type_id actual,
				    tstatic_type_id target);
tstring *tstatic_type_format(const tstatic_type_arena *arena,
			     tstatic_type_id id);
tstring *tstatic_type_display(const tstatic_type_arena *arena, tstatic_type_id id);
tstatic_type_id tstatic_type_with_name(tstatic_type_arena *arena,
		tstatic_type_id id, const char *name);

#ifdef __cplusplus
}
#endif

#endif
