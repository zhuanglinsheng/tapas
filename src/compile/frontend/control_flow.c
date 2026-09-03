#include "control_flow_internal.h"

#include <stdlib.h>

static void connect(tcontrol_flow *flow, const tast_arena *arena,
		    tast_id parent, tast_id child, tcontrol_child_role role,
		    tast_id enclosing);

static void connect_children(tcontrol_flow *flow, const tast_arena *arena,
			     tast_id parent, uint32_t start, uint32_t count,
			     tcontrol_child_role role, tast_id enclosing)
{
	const tast_id *children = tast_get_children(arena, start, count);
	for (uint32_t i = 0; children && i < count; i++)
		connect(flow, arena, parent, children[i], role, enclosing);
}

static void connect(tcontrol_flow *flow, const tast_arena *arena,
		    tast_id parent, tast_id child, tcontrol_child_role role,
		    tast_id enclosing)
{
	const tast_node *node = tast_get(arena, child);
	if (!node || child >= flow->node_count) return;
	flow->nodes[child].parent = parent;
	flow->nodes[child].role = role;
	flow->nodes[child].enclosing_function = enclosing;
	if (parent != TAST_INVALID_ID && parent < flow->node_count) {
		flow->nodes[child].next_sibling = flow->nodes[parent].first_child;
		flow->nodes[parent].first_child = child;
	}
	tast_id nested = node->kind == tast_function || node->kind == tast_rule ?
		child : enclosing;
	switch (node->kind) {
	case tast_group:
		connect(flow, arena, child, node->group.value,
			tcontrol_child_expression, nested);
		break;
	case tast_unary:
		connect(flow, arena, child, node->unary.operand,
			tcontrol_child_expression, nested);
		break;
	case tast_binary:
		connect(flow, arena, child, node->binary.left,
			tcontrol_child_expression, nested);
		connect(flow, arena, child, node->binary.right,
			tcontrol_child_expression, nested);
		break;
	case tast_call:
	case tast_index:
		connect(flow, arena, child, node->aggregate.receiver,
			tcontrol_child_receiver, nested);
		connect_children(flow, arena, child, node->aggregate.children,
			node->aggregate.count, tcontrol_child_argument, nested);
		break;
	case tast_list:
	case tast_dictionary:
	case tast_structure:
	case tast_module:
	case tast_block:
	case tast_declaration_group:
		connect_children(flow, arena, child, node->aggregate.children,
			node->aggregate.count, tcontrol_child_expression, nested);
		break;
	case tast_member:
		connect(flow, arena, child, node->member.receiver,
			tcontrol_child_receiver, nested);
		break;
	case tast_named_field:
		connect(flow, arena, child, node->named_field.value,
			tcontrol_child_expression, nested);
		break;
	case tast_slice:
		if (node->slice.has_start)
			connect(flow, arena, child, node->slice.start,
				tcontrol_child_expression, nested);
		if (node->slice.has_end)
			connect(flow, arena, child, node->slice.end,
				tcontrol_child_expression, nested);
		break;
	case tast_function:
	case tast_rule:
		connect_children(flow, arena, child, node->function.parameters,
			node->function.parameter_count, tcontrol_child_parameter, child);
		connect(flow, arena, child, node->function.body,
			tcontrol_child_function_body, child);
		break;
	case tast_rule_condition:
		connect(flow, arena, child, node->rule_condition.value,
			tcontrol_child_condition, nested);
		break;
	case tast_rule_requirement:
		connect(flow, arena, child, node->expression_statement.value,
			tcontrol_child_expression, nested);
		break;
	case tast_expression_statement:
		connect(flow, arena, child, node->expression_statement.value,
			tcontrol_child_expression, nested);
		break;
	case tast_declaration_statement:
		if (node->declaration_statement.has_initializer)
			connect(flow, arena, child,
				node->declaration_statement.initializer,
				tcontrol_child_expression, nested);
		break;
	case tast_assignment_statement:
		connect(flow, arena, child, node->assignment_statement.target,
			tcontrol_child_assignment_target, nested);
		connect(flow, arena, child, node->assignment_statement.value,
			tcontrol_child_assignment_value, nested);
		break;
	case tast_return_statement:
		if (node->return_statement.value != TAST_INVALID_ID)
			connect(flow, arena, child, node->return_statement.value,
				tcontrol_child_expression, nested);
		break;
	case tast_if_statement:
		connect_children(flow, arena, child,
			node->conditional_statement.branches,
			node->conditional_statement.branch_count,
			tcontrol_child_expression, nested);
		break;
	case tast_conditional_branch:
		if (node->conditional_branch.has_condition)
			connect(flow, arena, child,
				node->conditional_branch.condition,
				tcontrol_child_condition, nested);
		connect(flow, arena, child, node->conditional_branch.body,
			tcontrol_child_control_body, nested);
		break;
	case tast_while_statement:
		connect(flow, arena, child, node->control_statement.condition,
			tcontrol_child_condition, nested);
		connect(flow, arena, child, node->control_statement.body,
			tcontrol_child_control_body, nested);
		break;
	case tast_for_statement:
		connect(flow, arena, child, node->for_statement.iterable,
			tcontrol_child_expression, nested);
		connect(flow, arena, child, node->for_statement.body,
			tcontrol_child_control_body, nested);
		break;
	default:
		break;
	}
}

static int compute_returns(tcontrol_flow *flow, const tast_arena *arena,
			   tast_id block_id)
{
	if (block_id >= flow->node_count) return 0;
	if (flow->nodes[block_id].definitely_returns)
		return flow->nodes[block_id].definitely_returns == 2;
	const tast_node *block = tast_get(arena, block_id);
	if (!block || (block->kind != tast_block && block->kind != tast_module)) {
		flow->nodes[block_id].definitely_returns = 1;
		return 0;
	}
	const tast_id *statements = tast_get_children(
		arena, block->aggregate.children, block->aggregate.count);
	int result = 0;
	for (uint32_t i = 0; statements && i < block->aggregate.count; i++) {
		const tast_node *statement = tast_get(arena, statements[i]);
		if (!statement) continue;
		if (statement->kind == tast_return_statement) {
			result = 1;
			break;
		}
		if (statement->kind != tast_if_statement) continue;
		const tast_id *branches = tast_get_children(arena,
			statement->conditional_statement.branches,
			statement->conditional_statement.branch_count);
		int all_return = statement->conditional_statement.branch_count > 0;
		int has_else = 0;
		for (uint32_t j = 0; branches &&
		     j < statement->conditional_statement.branch_count; j++) {
			const tast_node *branch = tast_get(arena, branches[j]);
			if (!branch) { all_return = 0; continue; }
			all_return = all_return && compute_returns(
				flow, arena, branch->conditional_branch.body);
			has_else |= !branch->conditional_branch.has_condition;
		}
		if (has_else && all_return) {
			result = 1;
			break;
		}
	}
	flow->nodes[block_id].definitely_returns = result ? 2 : 1;
	return result;
}

void tcontrol_flow_init(tcontrol_flow *flow)
{
	*flow = (tcontrol_flow){ 0 };
}

void tcontrol_flow_free(tcontrol_flow *flow)
{
	if (!flow) return;
	free(flow->nodes);
	free(flow->assigned_symbols);
	*flow = (tcontrol_flow){ 0 };
}

void tcontrol_flow_build(tcontrol_flow *flow, const tast_arena *arena,
			 const tsemantic_model *semantic, tast_id root)
{
	tcontrol_flow_free(flow);
	if (!arena || !arena->node_count) return;
	flow->nodes = (tcontrol_node *)calloc(
		arena->node_count, sizeof(*flow->nodes));
	if (!flow->nodes) abort();
	flow->node_count = arena->node_count;
	for (uint32_t i = 0; i < flow->node_count; i++) {
		flow->nodes[i].parent = TAST_INVALID_ID;
		flow->nodes[i].first_child = TAST_INVALID_ID;
		flow->nodes[i].next_sibling = TAST_INVALID_ID;
		flow->nodes[i].enclosing_function = TAST_INVALID_ID;
	}
	connect(flow, arena, TAST_INVALID_ID, root, tcontrol_child_none,
		TAST_INVALID_ID);
	for (tast_id id = 0; id < arena->node_count; id++) {
		const tast_node *node = tast_get(arena, id);
		if (node && (node->kind == tast_block || node->kind == tast_module))
			compute_returns(flow, arena, id);
	}
	tcontrol_compute_assignments(flow, arena, semantic, root);
}

tast_id tcontrol_parent(const tcontrol_flow *flow, tast_id node)
{
	return flow && node < flow->node_count ?
		flow->nodes[node].parent : TAST_INVALID_ID;
}

tast_id tcontrol_enclosing_function(const tcontrol_flow *flow, tast_id node)
{
	return flow && node < flow->node_count ?
		flow->nodes[node].enclosing_function : TAST_INVALID_ID;
}

tcontrol_child_role tcontrol_role(const tcontrol_flow *flow, tast_id node)
{
	return flow && node < flow->node_count ?
		flow->nodes[node].role : tcontrol_child_none;
}

int tcontrol_definitely_returns(const tcontrol_flow *flow, tast_id block)
{
	return flow && block < flow->node_count &&
		flow->nodes[block].definitely_returns == 2;
}

int tcontrol_definitely_assigned(const tcontrol_flow *flow, tast_id node)
{
	return flow && node < flow->node_count &&
		flow->nodes[node].definitely_assigned == 2;
}

int tcontrol_symbol_definitely_assigned(
	const tcontrol_flow *flow, uint32_t symbol)
{
	return flow && symbol < flow->symbol_count &&
		flow->assigned_symbols[symbol];
}

static tast_id find_node_at(const tcontrol_flow *flow,
			    const tast_arena *arena, tast_id id,
			    uint32_t offset)
{
	const tast_node *node = tast_get(arena, id);
	if (!node) return TAST_INVALID_ID;
	int contains = node->span.start <= offset && offset < node->span.end;
	if (!contains && node->span.start == node->span.end)
		contains = offset == node->span.start;
	if (!contains) return TAST_INVALID_ID;
	tast_id best = id;
	uint32_t width = node->span.end - node->span.start;
	for (tast_id child = flow->nodes[id].first_child;
	     child != TAST_INVALID_ID;
	     child = flow->nodes[child].next_sibling) {
		tast_id candidate = find_node_at(flow, arena, child, offset);
		const tast_node *found = tast_get(arena, candidate);
		if (!found) continue;
		uint32_t candidate_width = found->span.end - found->span.start;
		if (candidate_width < width ||
		    (candidate_width == width && candidate > best)) {
			best = candidate;
			width = candidate_width;
		}
	}
	return best;
}

tast_id tcontrol_find_node_at(const tcontrol_flow *flow,
			      const tast_arena *arena, tast_id root,
			      uint32_t offset)
{
	const tast_node *node = tast_get(arena, root);
	if (!flow || root >= flow->node_count || !node ||
	    offset < node->span.start || offset > node->span.end)
		return TAST_INVALID_ID;
	if (offset == node->span.end) return root;
	return find_node_at(flow, arena, root, offset);
}
