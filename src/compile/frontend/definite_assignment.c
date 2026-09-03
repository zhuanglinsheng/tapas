#include "control_flow_internal.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
	uint8_t *assigned;
	uint32_t count;
} assignment_state;

static assignment_state state_new(uint32_t count)
{
	assignment_state state = {
		.assigned = (uint8_t *)calloc(count, sizeof(*state.assigned)),
		.count = count
	};
	if (count && !state.assigned) abort();
	return state;
}

static assignment_state state_copy(const assignment_state *source)
{
	assignment_state copy = state_new(source->count);
	if (source->count)
		memcpy(copy.assigned, source->assigned, source->count);
	return copy;
}

static void state_intersect(assignment_state *target,
			    const assignment_state *source)
{
	for (uint32_t i = 0; i < target->count; i++)
		target->assigned[i] &= source->assigned[i];
}

static void state_free(assignment_state *state)
{
	free(state->assigned);
	*state = (assignment_state){ 0 };
}

typedef struct {
	tcontrol_flow *flow;
	const tast_arena *arena;
	const tsemantic_model *semantic;
} assignment_analyzer;

static void analyze_node(assignment_analyzer *analyzer, tast_id id,
			 assignment_state *state);

static uint32_t symbol_id(const assignment_analyzer *analyzer,
			  const tsemantic_symbol *symbol)
{
	return symbol ? (uint32_t)(symbol - analyzer->semantic->symbols) :
		TSEMANTIC_INVALID_ID;
}

static void assign_declaration(const assignment_analyzer *analyzer,
			       assignment_state *state, tast_id declaration)
{
	uint32_t id = symbol_id(analyzer, tsemantic_symbol_for_declaration(
		analyzer->semantic, declaration));
	if (id < state->count) state->assigned[id] = 1;
}

static void analyze_children(assignment_analyzer *analyzer,
			     const tast_node *node,
			     assignment_state *state)
{
	const tast_id *children = tast_get_children(analyzer->arena,
		node->aggregate.children, node->aggregate.count);
	for (uint32_t i = 0; children && i < node->aggregate.count; i++)
		analyze_node(analyzer, children[i], state);
}

static void analyze_conditional(assignment_analyzer *analyzer,
				const tast_node *conditional,
				assignment_state *state)
{
	assignment_state entry = state_copy(state);
	assignment_state merged = { 0 };
	int has_else = 0;
	const tast_id *branches = tast_get_children(analyzer->arena,
		conditional->conditional_statement.branches,
		conditional->conditional_statement.branch_count);
	for (uint32_t i = 0; branches &&
	     i < conditional->conditional_statement.branch_count; i++) {
		const tast_node *branch = tast_get(analyzer->arena, branches[i]);
		if (!branch) continue;
		assignment_state result = state_copy(&entry);
		if (branch->conditional_branch.has_condition)
			analyze_node(analyzer,
				branch->conditional_branch.condition, &result);
		analyze_node(analyzer, branch->conditional_branch.body, &result);
		if (!tcontrol_definitely_returns(
		    analyzer->flow, branch->conditional_branch.body)) {
			if (!merged.assigned)
				merged = result;
			else {
				state_intersect(&merged, &result);
				state_free(&result);
			}
		} else
			state_free(&result);
		has_else |= !branch->conditional_branch.has_condition;
	}
	if (!has_else) {
		if (!merged.assigned) merged = state_copy(&entry);
		state_intersect(&merged, &entry);
	}
	if (state->count)
		memcpy(state->assigned,
			merged.assigned ? merged.assigned : entry.assigned,
			state->count);
	state_free(&merged);
	state_free(&entry);
}

static void analyze_block(assignment_analyzer *analyzer,
			  const tast_node *block, assignment_state *state)
{
	const tast_id *statements = tast_get_children(analyzer->arena,
		block->aggregate.children, block->aggregate.count);
	for (uint32_t i = 0; statements && i < block->aggregate.count; i++)
		analyze_node(analyzer, statements[i], state);
}

static void analyze_function(assignment_analyzer *analyzer,
			     const tast_node *node,
			     const assignment_state *outer)
{
	assignment_state nested = state_copy(outer);
	if (node->kind == tast_rule && nested.count)
		memset(nested.assigned, 1, nested.count);
	const tast_id *parameters = tast_get_children(analyzer->arena,
		node->function.parameters, node->function.parameter_count);
	for (uint32_t i = 0; parameters && i < node->function.parameter_count; i++)
		assign_declaration(analyzer, &nested, parameters[i]);
	analyze_node(analyzer, node->function.body, &nested);
	state_free(&nested);
}

static void analyze_node(assignment_analyzer *analyzer, tast_id id,
			 assignment_state *state)
{
	const tast_node *node = tast_get(analyzer->arena, id);
	if (!node) return;
	switch (node->kind) {
	case tast_name: {
		uint32_t symbol = symbol_id(analyzer,
			tsemantic_resolved_symbol(analyzer->semantic, id));
		analyzer->flow->nodes[id].definitely_assigned =
			symbol >= state->count || state->assigned[symbol] ? 2 : 1;
	} break;
	case tast_group:
		analyze_node(analyzer, node->group.value, state);
		break;
	case tast_unary:
		analyze_node(analyzer, node->unary.operand, state);
		break;
	case tast_binary:
		analyze_node(analyzer, node->binary.left, state);
		analyze_node(analyzer, node->binary.right, state);
		break;
	case tast_call:
	case tast_index:
		analyze_node(analyzer, node->aggregate.receiver, state);
		analyze_children(analyzer, node, state);
		break;
	case tast_list:
	case tast_dictionary:
	case tast_structure:
	case tast_declaration_group:
		analyze_children(analyzer, node, state);
		break;
	case tast_member:
		analyze_node(analyzer, node->member.receiver, state);
		break;
	case tast_named_field:
		analyze_node(analyzer, node->named_field.value, state);
		break;
	case tast_slice:
		if (node->slice.has_start) analyze_node(analyzer, node->slice.start, state);
		if (node->slice.has_end) analyze_node(analyzer, node->slice.end, state);
		break;
	case tast_function:
	case tast_rule:
		analyze_function(analyzer, node, state);
		break;
	case tast_rule_condition:
		analyze_node(analyzer, node->rule_condition.value, state);
		break;
	case tast_rule_requirement:
		analyze_node(analyzer, node->expression_statement.value, state);
		break;
	case tast_module:
	case tast_block:
		analyze_block(analyzer, node, state);
		break;
	case tast_expression_statement:
		analyze_node(analyzer, node->expression_statement.value, state);
		break;
	case tast_declaration_statement:
		if (node->declaration_statement.has_initializer) {
			analyze_node(analyzer, node->declaration_statement.initializer, state);
			assign_declaration(analyzer, state, id);
		}
		break;
	case tast_assignment_statement: {
		analyze_node(analyzer, node->assignment_statement.value, state);
		analyze_node(analyzer, node->assignment_statement.target, state);
		const tast_node *target = tast_get(
			analyzer->arena, node->assignment_statement.target);
		if (target && target->kind == tast_name) {
			uint32_t symbol = symbol_id(analyzer, tsemantic_resolved_symbol(
				analyzer->semantic, node->assignment_statement.target));
			if (symbol < state->count) state->assigned[symbol] = 1;
		}
	} break;
	case tast_return_statement:
		if (node->return_statement.value != TAST_INVALID_ID)
			analyze_node(analyzer, node->return_statement.value, state);
		break;
	case tast_import_statement:
		if (node->import_statement.has_alias)
			assign_declaration(analyzer, state, id);
		break;
	case tast_if_statement:
		analyze_conditional(analyzer, node, state);
		break;
	case tast_conditional_branch: {
		if (node->conditional_branch.has_condition)
			analyze_node(analyzer,
				node->conditional_branch.condition, state);
		assignment_state nested = state_copy(state);
		analyze_node(analyzer, node->conditional_branch.body, &nested);
		state_free(&nested);
	} break;
	case tast_while_statement: {
		analyze_node(analyzer, node->control_statement.condition, state);
		assignment_state nested = state_copy(state);
		analyze_node(analyzer, node->control_statement.body, &nested);
		state_free(&nested);
	} break;
	case tast_for_statement: {
		analyze_node(analyzer, node->for_statement.iterable, state);
		assignment_state nested = state_copy(state);
		const tsemantic_symbol *symbol = node->for_statement.declares_binding ?
			tsemantic_symbol_for_declaration(analyzer->semantic, id) :
			tsemantic_resolved_symbol(analyzer->semantic, id);
		uint32_t sid = symbol_id(analyzer, symbol);
		if (sid < nested.count) nested.assigned[sid] = 1;
		analyze_node(analyzer, node->for_statement.body, &nested);
		state_free(&nested);
	} break;
	default:
		break;
	}
}

void tcontrol_compute_assignments(tcontrol_flow *flow,
	const tast_arena *arena, const tsemantic_model *semantic, tast_id root)
{
	if (!flow || !arena || !semantic) return;
	assignment_analyzer analyzer = { flow, arena, semantic };
	assignment_state state = state_new(semantic->symbol_count);
	for (uint32_t i = 0; i < semantic->symbol_count; i++)
		state.assigned[i] = semantic->symbols[i].external &&
			semantic->symbols[i].initially_assigned;
	analyze_node(&analyzer, root, &state);
	flow->symbol_count = state.count;
	flow->assigned_symbols = state.assigned;
	state.assigned = nullptr;
	state_free(&state);
}
