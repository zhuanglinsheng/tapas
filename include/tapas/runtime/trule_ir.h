#ifndef TAPAS_RUNTIME_TRULE_IR_H
#define TAPAS_RUNTIME_TRULE_IR_H

#include "tapas/ds/tobj_vec.h"
#include "tapas/runtime/ttype.h"

/* Private OP_RULECOND mode: record the raw antecedent and leave its truth
 * value on the stack. It does not enable general RuleInstance truthiness. */
#define TRULE_ANTECEDENT_RECORD 0x03ffffffu
int trule_antecedent_type(const ttypeval *type);

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	trule_term_constant,
	trule_term_parameter,
	trule_term_capture,
	trule_term_intrinsic,
	trule_term_call,
	trule_term_construct,
	trule_term_convert,
	trule_term_extension,
	trule_term_not,
	trule_term_and,
	trule_term_or,
	trule_term_in
} trule_term_kind;

typedef enum {
	trule_item_condition,
	trule_item_requirement,
	trule_item_implication
} trule_item_kind;

typedef struct trule_term {
	tcompo_v base;
	uint64_t id;
	trule_term_kind kind;
	ttypeval *type;
	tobj payload;
	tobj_vec arguments;
	tstring *provider;
	tstring *provider_kind;
	long provider_version;
	long origin_start;
	long origin_end;
} trule_term;

typedef struct trule_item {
	tcompo_v base;
	trule_item_kind kind;
	trule_term *term;
	trule_term *rule;
	tobj_vec arguments;
	tstring *description;
	long origin_start;
	long origin_end;
} trule_item;

typedef struct trule_ir {
	tcompo_v base;
	tstring *display_name;
	tstring *source;
	long version;
	tobj_vec parameters;
	tobj_vec captures;
	tobj_vec terms;
	tobj_vec items;
} trule_ir;

extern tcompo_vtable trule_term_vtable;
extern tcompo_vtable trule_item_vtable;
extern tcompo_vtable trule_ir_vtable;

trule_term *trule_term_new(trule_term_kind kind, ttypeval *type,
			   const tobj *payload, const tobj *arguments,
			   uint_regs argument_count);
trule_term *trule_term_logic_new(trule_term_kind kind, trule_term *left, trule_term *right);
trule_term *trule_term_in_new(trule_term *value, trule_term *domain);
trule_term *trule_term_not_new(trule_term *operand);
trule_term *trule_term_parameter_new(const char *name, ttypeval *type);
trule_term *trule_term_constant_new(const tobj *value);
trule_term *trule_term_extension_new(const char *provider,
			     const char *kind, const tobj *arguments,
			     uint_regs argument_count, const tobj *payload);
trule_item *trule_condition_new(trule_term *term, const char *description);
/* Implication retains its Bool/RuleInstance antecedent Term; arguments are
 * Bool consequent Terms. Satisfaction is interpreted by the evaluator. */
trule_item *trule_implication_new(trule_term *antecedent,
	const tobj *consequents, uint_regs count, const char *description);
trule_item *trule_requirement_new(trule_term *rule,
				  const tobj *arguments,
				  uint_regs argument_count);
trule_ir *trule_ir_new(const char *display_name, const char *source);
void trule_ir_add_parameter(trule_ir *ir, trule_term *parameter);
void trule_ir_add_capture(trule_ir *ir, trule_term *capture);
void trule_ir_add_item(trule_ir *ir, trule_item *item);
void trule_ir_collect_term(trule_ir *ir, trule_term *term);
uint64_t trule_ir_semantic_hash(const trule_ir *ir);
uint64_t trule_ir_content_hash(const trule_ir *ir);
const char *trule_term_kind_name(trule_term_kind kind);
int trule_ir_serialize(const trule_ir *ir, tstring **result);
trule_ir *trule_ir_deserialize(const char *data);

#ifdef __cplusplus
}
#endif

#endif
