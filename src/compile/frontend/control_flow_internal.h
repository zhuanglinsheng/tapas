#ifndef TAPAS_COMPILE_CONTROL_FLOW_INTERNAL_H
#define TAPAS_COMPILE_CONTROL_FLOW_INTERNAL_H

#include "tapas/compile/control_flow.h"
#include "tapas/compile/semantic.h"

struct tcontrol_node {
	tast_id parent;
	tast_id first_child;
	tast_id next_sibling;
	tast_id enclosing_function;
	tcontrol_child_role role;
	uint8_t definitely_returns;
	uint8_t definitely_assigned;
};

void tcontrol_compute_assignments(tcontrol_flow *flow,
	const tast_arena *arena, const tsemantic_model *semantic, tast_id root);

#endif
