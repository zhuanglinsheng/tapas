#ifndef TAPAS_RUNTIME_TTYPE_H
#define TAPAS_RUNTIME_TTYPE_H

#include "tapas/ds/thashtbl.h"
#include "tapas/tval.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	ttype_kind_any = 0,
	ttype_kind_builtin,
	ttype_kind_fields,
	ttype_kind_list,
	ttype_kind_pair,
	ttype_kind_dictionary,
	ttype_kind_function,
	ttype_kind_union,
	ttype_kind_recursive
} ttype_kind;

typedef enum {
	ttype_builtin_any = 0,
	ttype_builtin_nil,
	ttype_builtin_bool,
	ttype_builtin_int,
	ttype_builtin_float,
	ttype_builtin_string,
	ttype_builtin_list,
	ttype_builtin_pair,
	ttype_builtin_dictionary,
	ttype_builtin_iterator,
	ttype_builtin_function,
	ttype_builtin_library,
	ttype_builtin_real_array,
	ttype_builtin_bool_array,
	ttype_builtin_time,
	ttype_builtin_type,
	ttype_builtin_count
} ttype_builtin;

typedef struct {
	const tstring *name;
	ttypeval *type;
} ttype_field;

struct ttypeval {
	tcompo_v base;
	ttype_kind kind;
	ttype_builtin builtin;
	thashtbl *definition;
	tstring *canonical;
	uint64_t canonical_hash;
	uint_objs function_parameter_count;
	uint8_t function_variadic;
	uint8_t contains_recursive;
	uint8_t recursive_defined;
	uint32_t recursive_id;
	ttypeval *recursive_body;
};

extern tcompo_vtable ttypeval_vtable;

ttypeval *ttypeval_builtin(ttype_builtin builtin);
void ttypeval_retain(ttypeval *type);
void ttypeval_release(ttypeval *type);
ttypeval *ttypeval_new_fields(const ttype_field *fields, uint_objs count);
ttypeval *ttypeval_new_list(ttypeval *item);
ttypeval *ttypeval_new_pair(ttypeval *first, ttypeval *second);
ttypeval *ttypeval_new_dictionary(ttypeval *key, ttypeval *value);
ttypeval *ttypeval_new_function(ttypeval *const *parameters,
				uint_objs parameter_count,
				ttypeval *result,
				int variadic);
ttypeval *ttypeval_new_union(ttypeval *const *members, uint_objs count);
ttypeval *ttypeval_new_recursive(void);
int ttypeval_define_recursive(ttypeval *type, ttypeval *body);
int ttypeval_is_recursive(const ttypeval *type);
int ttypeval_equal(const ttypeval *left, const ttypeval *right);
uint64_t ttypeval_hash(const ttypeval *type);
int ttypeval_matches(const tobj *value, const ttypeval *expected);
void ttypeval_idx(ttypeval *type, const tobj *params, uint_regs np,
		  tobj *result);
int ttypeval_next_key(const ttypeval *type, long *position, tobj *result);
uint_objs ttypeval_field_count(const ttypeval *type);
int ttypeval_field_at(const ttypeval *type, uint_objs index,
		      const tobj **name, ttypeval **field_type);
ttypeval *ttypeval_field_named(const ttypeval *type, const char *name);
uint_objs ttypeval_member_count(const ttypeval *type);
ttypeval *ttypeval_member_at(const ttypeval *type, uint_objs index);
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
