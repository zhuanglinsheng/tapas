#ifndef TAPAS_COMPILE_CONTROL_FLOW_H
#define TAPAS_COMPILE_CONTROL_FLOW_H

#include "tapas/compile/ast.h"

typedef struct tsemantic_model tsemantic_model;

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	tcontrol_child_none,
	tcontrol_child_expression,
	tcontrol_child_receiver,
	tcontrol_child_argument,
	tcontrol_child_assignment_target,
	tcontrol_child_assignment_value,
	tcontrol_child_parameter,
	tcontrol_child_function_body,
	tcontrol_child_condition,
	tcontrol_child_control_body
} tcontrol_child_role;

typedef struct tcontrol_node tcontrol_node;

typedef struct {
	tcontrol_node *nodes;
	uint8_t *assigned_symbols;
	uint32_t node_count;
	uint32_t symbol_count;
} tcontrol_flow;

void tcontrol_flow_init(tcontrol_flow *flow);

void tcontrol_flow_free(tcontrol_flow *flow);

void tcontrol_flow_build(tcontrol_flow *flow, const tast_arena *arena,
			 const tsemantic_model *semantic, tast_id root);

tast_id tcontrol_parent(const tcontrol_flow *flow, tast_id node);

tast_id tcontrol_enclosing_function(const tcontrol_flow *flow, tast_id node);

tcontrol_child_role tcontrol_role(const tcontrol_flow *flow, tast_id node);

int tcontrol_definitely_returns(const tcontrol_flow *flow, tast_id block);
int tcontrol_definitely_assigned(const tcontrol_flow *flow, tast_id node);
int tcontrol_symbol_definitely_assigned(
	const tcontrol_flow *flow, uint32_t symbol);

tast_id tcontrol_find_node_at(const tcontrol_flow *flow,
			      const tast_arena *arena, tast_id root,
			      uint32_t offset);

#ifdef __cplusplus
}
#endif

#endif
