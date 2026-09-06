#ifndef TAPAS_COMPILE_AST_H
#define TAPAS_COMPILE_AST_H

#include "tapas/compile/syntax.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t tast_id;
#define TAST_INVALID_ID UINT32_MAX

typedef enum {
	tast_error = 0,
	tast_name,
	tast_nil,
	tast_bool,
	tast_integer,
	tast_float,
	tast_string,
	tast_group,
	tast_unary,
	tast_binary,
	tast_call,
	tast_index,
	tast_slice,
	tast_member,
	tast_list,
	tast_dictionary,
	tast_structure,
	tast_named_field,
	tast_parameter,
	tast_function,
	tast_rule,
	tast_rule_condition,
	tast_rule_implication,
	tast_module,
	tast_import_statement,
	tast_expression_statement,
	tast_declaration_statement,
	tast_declaration_group,
	tast_assignment_statement,
	tast_return_statement,
	tast_block,
	tast_if_statement,
	tast_conditional_branch,
	tast_while_statement,
	tast_for_statement,
	tast_break_statement,
	tast_continue_statement
} tast_kind;

typedef struct {
	tast_kind kind;
	tsource_span span;
	union {
		struct { tsyntax_kind op; tast_id operand; } unary;
		struct { tsyntax_kind op; tast_id left; tast_id right; } binary;
		struct { tast_id value; } group;
		struct { tast_id receiver; uint32_t children; uint32_t count; } aggregate;
		struct { tast_id receiver; tsyntax_kind op; tsource_span name; } member;
		struct { tsource_span name; tast_id value; } named_field;
		struct {
			tsource_span name;
			tsource_span annotation;
			uint8_t has_annotation;
		} parameter;
		struct {
			tast_id start;
			tast_id end;
			uint8_t has_start;
			uint8_t has_end;
		} slice;
		struct {
			uint32_t parameters;
			uint32_t parameter_count;
			tast_id body;
			tsource_span return_annotation;
			uint8_t variadic;
			uint8_t has_return_annotation;
		} function;
		struct {
			tast_id value;
			tsource_span description;
			uint8_t has_description;
		} rule_condition;
		struct {
			tast_id antecedent;
			tast_id consequent;
			tsource_span description;
			uint8_t has_description;
		} rule_implication;
		struct {
			tsource_span path;
			tsource_span alias;
			uint8_t has_alias;
		} import_statement;
		struct { tast_id value; } expression_statement;
		struct {
			tsource_span name;
			tsource_span annotation;
			tast_id initializer;
			uint8_t is_mutable;
			uint8_t is_function_declaration;
			uint8_t has_annotation;
			uint8_t has_initializer;
		} declaration_statement;
		struct { tast_id target; tast_id value; } assignment_statement;
		struct { tast_id value; } return_statement;
		struct {
			uint32_t branches;
			uint32_t branch_count;
		} conditional_statement;
		struct {
			tast_id condition;
			tast_id body;
			uint8_t has_condition;
		} conditional_branch;
		struct { tast_id condition; tast_id body; } control_statement;
		struct {
			tsource_span name;
			tast_id iterable;
			tast_id body;
			uint8_t declares_binding;
		} for_statement;
	};
} tast_node;

typedef struct {
	tast_node *nodes;
	uint32_t node_count;
	uint32_t node_capacity;
	tast_id *children;
	uint32_t child_count;
	uint32_t child_capacity;
} tast_arena;

void tast_arena_init(tast_arena *arena);

void tast_arena_free(tast_arena *arena);

tast_id tast_arena_add(tast_arena *arena, tast_node node);

uint32_t tast_arena_add_children(tast_arena *arena,
				 const tast_id *children, uint32_t count);

const tast_node *tast_get(const tast_arena *arena, tast_id id);

const tast_id *tast_get_children(const tast_arena *arena,
				 uint32_t start, uint32_t count);

tstring *tast_string_value(const tsource_document *document,
			   const tast_node *node);

tstring *tast_scoped_member_name(const tsource_document *document,
				 const tast_arena *arena,
				 const tast_node *member,
				 const char *scope);

#ifdef __cplusplus
}
#endif

#endif /* TAPAS_COMPILE_AST_H */
