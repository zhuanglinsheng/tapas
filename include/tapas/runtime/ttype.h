#ifndef TAPAS_RUNTIME_TTYPE_H
#define TAPAS_RUNTIME_TTYPE_H

#include "tapas/ds/thashtbl.h"
#include "tapas/tbuiltin.h"
#include "tapas/tval.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	ttype_kind_any = 0,
	ttype_kind_builtin,
	ttype_kind_fields,
	ttype_kind_list,
	ttype_kind_iterator,
	ttype_kind_pair,
	ttype_kind_dictionary,
	ttype_kind_function,
	ttype_kind_rule,
	ttype_kind_rule_instance,
	ttype_kind_rule_term,
	ttype_kind_union,
	ttype_kind_enum,
	ttype_kind_recursive,
	ttype_kind_instance_of,
	ttype_kind_points,
	ttype_kind_range
} ttype_kind;

typedef struct {
	const tstring *name;
	ttypeval *type;
	uint8_t optional;
} ttype_field;

struct ttypeval {
	tcompo_v base;
	ttype_kind kind;
	tbuiltin_id builtin;
	thashtbl *definition;
	tstring *canonical;
	uint64_t canonical_hash;
	uint_objs function_parameter_count;
	uint8_t function_variadic;
	uint8_t contains_recursive;
	uint8_t recursive_defined;
	uint32_t recursive_id;
	ttypeval *recursive_body;
	tobj instance_rule;
	tstring *instance_reference;
	uint8_t contains_instance;
	uint8_t contains_domain;
	tstring **optional_fields;
	uint_objs optional_field_count;
	tstring **enum_members;
	uint_objs enum_member_count;
};

extern tcompo_vtable ttypeval_vtable;

ttypeval *ttypeval_builtin(tbuiltin_id builtin);
ttypeval *ttypeval_builtin_named(const char *name);
ttypeval *ttypeval_from_canonical(const char *canonical);
ttypeval *ttypeval_retain(ttypeval *type);
void ttypeval_release(ttypeval *type);
ttypeval *ttypeval_new_fields(const ttype_field *fields, uint_objs count);
ttypeval *ttypeval_new_domain(int range, ttypeval *item);
ttypeval *ttypeval_new_list(ttypeval *item);
ttypeval *ttypeval_new_iterator(ttypeval *item);
ttypeval *ttypeval_new_pair(ttypeval *first, ttypeval *second);
ttypeval *ttypeval_new_dictionary(ttypeval *key, ttypeval *value);
ttypeval *ttypeval_new_function(ttypeval *const *parameters,
				uint_objs parameter_count,
				ttypeval *result,
				int variadic);
ttypeval *ttypeval_new_rule(ttypeval *const *parameters,
			    uint_objs parameter_count);
ttypeval *ttypeval_new_rule_instance(ttypeval *const *parameters,
				     uint_objs parameter_count);
ttypeval *ttypeval_new_instance_reference(const char *name,
	ttypeval *const *parameters, uint_objs count);
ttypeval *ttypeval_new_instance_of(const tobj *rule);
typedef const tobj *(*ttype_instance_resolver)(void *context, const char *name);
/* Returns one owned reference. Callback is visited for symbolic Rule values. */
ttypeval *ttypeval_resolve_instances(ttypeval *type, ttype_instance_resolver resolver, void *context);
ttypeval *ttypeval_new_rule_term(ttypeval *result);
ttypeval *ttypeval_new_union(ttypeval *const *members, uint_objs count);
ttypeval *ttypeval_new_enum(const tstring *const *members, uint_objs count);
ttypeval *ttypeval_new_recursive(void);
int ttypeval_define_recursive(ttypeval *type, ttypeval *body);
int ttypeval_is_recursive(const ttypeval *type);
int ttypeval_equal(const ttypeval *left, const ttypeval *right);
uint64_t ttypeval_hash(const ttypeval *type);
int ttypeval_matches(const tobj *value, const ttypeval *expected);
void ttypeval_idx(ttypeval *type, const tobj *params, uint_regs np,
		  tobj *result);
uint_objs ttypeval_field_count(const ttypeval *type);
int ttypeval_field_at(const ttypeval *type, uint_objs index,
		      const tobj **name, ttypeval **field_type);
ttypeval *ttypeval_field_named(const ttypeval *type, const char *name);
int ttypeval_field_optional(const ttypeval *type, const char *name);
uint_objs ttypeval_member_count(const ttypeval *type);
ttypeval *ttypeval_member_at(const ttypeval *type, uint_objs index);
uint_objs ttypeval_enum_member_count(const ttypeval *type);
const tstring *ttypeval_enum_member_at(const ttypeval *type, uint_objs index);
int ttypeval_enum_contains(const ttypeval *type, const tstring *member);
ttypeval *ttypeval_base(const ttypeval *type);
ttypeval *ttypeval_parameter(const ttypeval *type, const char *name);
uint_objs ttypeval_function_parameter_count(const ttypeval *type);
ttypeval *ttypeval_function_parameter_at(const ttypeval *type,
					 uint_objs index);
ttypeval *ttypeval_function_result(const ttypeval *type);
int ttypeval_function_variadic(const ttypeval *type);
const thashtbl *ttypeval_definition(const ttypeval *type);

#ifdef __cplusplus
}
#endif

#endif /* TAPAS_RUNTIME_TTYPE_H */
