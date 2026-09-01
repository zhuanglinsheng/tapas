/** Bytecode emission from the reusable expression AST. */
#include "ast_emit_internal.h"

#include <stdlib.h>
#include <string.h>

static int binary_instruction(tsyntax_kind op)
{
	switch (op) {
	case tsyntax_colon: return OP_PAIR;
	case tsyntax_kw_or: return OP_OR;
	case tsyntax_kw_and: return OP_AND;
	case tsyntax_element_or: return OP_BOR;
	case tsyntax_element_and: return OP_BAND;
	case tsyntax_kw_in: return OP_IN;
	case tsyntax_kw_to: return OP_TO;
	case tsyntax_eq: return OP_EQ;
	case tsyntax_ne: return OP_NE;
	case tsyntax_gt: return OP_SG;
	case tsyntax_ge: return OP_GE;
	case tsyntax_lt: return OP_SL;
	case tsyntax_le: return OP_LE;
	case tsyntax_plus: return OP_ADD;
	case tsyntax_minus: return OP_SUB;
	case tsyntax_star: return OP_MUL;
	case tsyntax_slash: return OP_DIV;
	case tsyntax_percent: return OP_MOD;
	case tsyntax_matmul: return OP_MMUL;
	case tsyntax_power: return OP_POW;
	default: return -1;
	}
}

static tstring *node_text(const tast_emitter *emitter, tsource_span span)
{
	return tsource_document_slice(emitter->document, span);
}

void tast_emit_expression(tast_emitter *emitter, tast_id id);
static void emit_name(tast_emitter *emitter, const tast_node *node);

static void emit_reference(tast_emitter *emitter, tsource_span span)
{
	tast_node name = { .kind = tast_name, .span = span };
	emit_name(emitter, &name);
}

static void emit_name(tast_emitter *emitter, const tast_node *node)
{
	tstring *name = node_text(emitter, node->span);
	const char *text = tstring_cstr(name);
	if (strcmp(text, "this") == 0) {
		tvmcmd_vect_append(emitter->instructions, tbycode_make(OP_THIS));
		treg_ctr_add(&emitter->cp->regctr);
	} else if (strcmp(text, "base") == 0) {
		tvmcmd_vect_append(emitter->instructions, tbycode_make(OP_BASE));
		treg_ctr_add(&emitter->cp->regctr);
	} else {
		tobj_ctr *owner = NULL;
		uint_objs slot = 0;
		int found = tcompile_find_binding(
			&emitter->cp->tmpctr, name, &owner, &slot);
		if (!found)
			found = tcompile_find_binding(
				&emitter->cp->objctr, name, &owner, &slot);
		if (found && !owner->bindings[slot].initialized &&
		    !(emitter->allow_pending_type_references &&
		      ttypeval_is_recursive(owner->bindings[slot].type_value)))
			twarn(ErrCompile_Other,
			      "variable is read before initialization",
			      tstring_cstr(name));
		compile_emit_reference(emitter->cp, name, emitter->instructions,
				       emitter->constants);
	}
	tstring_free(name);
}

static void emit_number(tast_emitter *emitter, const tast_node *node)
{
	tstring *literal = node_text(emitter, node->span);
	if (node->kind == tast_integer) {
		long value = 0;
		if (!str_to_long_int(literal, &value))
			twarn(ErrCompile_InvalidLiter, "emit_number",
			      tstring_cstr(literal));
		uint_csts index = tconsts_add_int_const(emitter->constants, value);
		tvmcmd_vect_append(emitter->instructions,
				   tbycode_make_u(OP_PUSHI, index));
	} else {
		double value = 0.0;
		if (!str_to_float(literal, &value))
			twarn(ErrCompile_InvalidLiter, "emit_number",
			      tstring_cstr(literal));
		uint_csts index = tconsts_add_float_const(emitter->constants, value);
		tvmcmd_vect_append(emitter->instructions,
				   tbycode_make_u(OP_PUSHFLT, index));
	}
	treg_ctr_add(&emitter->cp->regctr);
	tstring_free(literal);
}

static void emit_string(tast_emitter *emitter, const tast_node *node)
{
	tsource_span contents = node->span;
	if (contents.end >= contents.start + 2) {
		contents.start++;
		contents.end--;
	}
	tstring *value = node_text(emitter, contents);
	uint_csts index = tconsts_add_str_const(emitter->constants,
					       tstring_cstr(value));
	tvmcmd_vect_append(emitter->instructions,
			   tbycode_make_u(OP_PUSHS, index));
	treg_ctr_add(&emitter->cp->regctr);
	tstring_free(value);
}

static void emit_short_circuit(tast_emitter *emitter,
			       const tast_node *node, int instruction)
{
	tast_emit_expression(emitter, node->binary.left);
	tvmcmd_vect_append(
		emitter->instructions,
		tbycode_make_u(instruction == OP_AND ? OP_CJPFPOP : OP_CJPBPOP, 0));
	uint_cmds condition = tvmcmd_vect_size32(emitter->instructions) - 1;
	treg_ctr_ddt(&emitter->cp->regctr);

	tvmcmd_vect right;
	tvmcmd_vect_init(&right);
	tvmcmd_vect *outer = emitter->instructions;
	emitter->instructions = &right;
	tast_emit_expression(emitter, node->binary.right);
	emitter->instructions = outer;

	uint_cmds right_length = tvmcmd_vect_size32(&right);
	outer->data[condition] = tbycode_make_u(
		instruction == OP_AND ? OP_CJPFPOP : OP_CJPBPOP,
		right_length + 1);
	tvmcmd_vect_insert_vect(outer, tvmcmd_vect_size32(outer), &right);
	tvmcmd_vect_free(&right);

	tvmcmd_vect_append(outer, tbycode_make_u(OP_JPF, 1));
	tvmcmd_vect_append(outer,
		tbycode_make_u(OP_PUSHB, instruction == OP_AND ? 0 : 1));
}

static void emit_binary(tast_emitter *emitter, const tast_node *node)
{
	int instruction = binary_instruction(node->binary.op);
	if (instruction == OP_AND || instruction == OP_OR) {
		emit_short_circuit(emitter, node, instruction);
		return;
	}

	/* The VM's binary convention places the left operand at the stack top. */
	tast_emit_expression(emitter, node->binary.right);
	tast_emit_expression(emitter, node->binary.left);
	if (instruction == OP_IN || instruction == OP_PAIR || instruction == OP_TO) {
		tvmcmd_vect_append(emitter->instructions,
				   tbycode_make((uint8_t)instruction));
	} else {
		tvmcmd_vect_append(emitter->instructions,
				   tbycode_make_u(OP_PUSHINFO, 0));
		treg_ctr_add(&emitter->cp->regctr);
		tvmcmd_vect_append(emitter->instructions,
				   tbycode_make_lr((uint8_t)instruction, 0, 1));
		treg_ctr_ddt(&emitter->cp->regctr);
	}
	treg_ctr_ddt_n(&emitter->cp->regctr, 2);
	treg_ctr_add(&emitter->cp->regctr);
}

static const tast_id *aggregate_children(const tast_emitter *emitter,
					 const tast_node *node)
{
	return tast_get_children(emitter->arena, node->aggregate.children,
				 node->aggregate.count);
}

static void emit_arguments(tast_emitter *emitter, const tast_node *node)
{
	const tast_id *children = aggregate_children(emitter, node);
	for (uint32_t i = 0; i < node->aggregate.count; i++)
		tast_emit_expression(emitter, children[i]);
}

static void emit_member(tast_emitter *emitter, const tast_node *node)
{
	tstring *name = node_text(emitter, node->member.name);
	uint_csts index = tconsts_add_str_const(emitter->constants,
					       tstring_cstr(name));
	tvmcmd_vect_append(emitter->instructions,
			   tbycode_make_u(OP_PUSHS, index));
	treg_ctr_add(&emitter->cp->regctr);
	tstring_free(name);
	tast_emit_expression(emitter, node->member.receiver);
	tvmcmd_vect_append(emitter->instructions, tbycode_make_u(OP_IDXR, 1));
	treg_ctr_ddt(&emitter->cp->regctr);
}

static void emit_call(tast_emitter *emitter, const tast_node *node)
{
	const tast_node *callee = tast_get(emitter->arena,
					  node->aggregate.receiver);
	uint32_t arguments = node->aggregate.count;
	if (callee->kind == tast_name) {
		tstring *name = node_text(emitter, callee->span);
		int calls_current_function = strcmp(tstring_cstr(name), "this") == 0;
		tstring_free(name);
		if (calls_current_function) {
			emit_arguments(emitter, node);
			tvmcmd_vect_append(emitter->instructions,
					   tbycode_make_u(OP_EVALTF, arguments));
			treg_ctr_ddt_n(&emitter->cp->regctr,
					  (uint_regs)arguments);
			treg_ctr_add(&emitter->cp->regctr);
			return;
		}
	}
	if (callee->kind == tast_member && callee->member.op == tsyntax_dot) {
		/* receiver.method(args) is the language's tunnel-call form. */
		tast_emit_expression(emitter, callee->member.receiver);
		emit_arguments(emitter, node);
		emit_reference(emitter, callee->member.name);
		tvmcmd_vect_append(emitter->instructions,
				   tbycode_make_u(OP_EVAL, arguments + 1));
		treg_ctr_ddt_n(&emitter->cp->regctr,
				  (uint_regs)(arguments + 2));
		treg_ctr_add(&emitter->cp->regctr);
		return;
	}

	emit_arguments(emitter, node);
	tast_emit_expression(emitter, node->aggregate.receiver);
	tvmcmd_vect_append(emitter->instructions,
			   tbycode_make_u(OP_EVAL, arguments));
	treg_ctr_ddt_n(&emitter->cp->regctr, (uint_regs)(arguments + 1));
	treg_ctr_add(&emitter->cp->regctr);
}

static void emit_index(tast_emitter *emitter, const tast_node *node)
{
	const tast_id *children = aggregate_children(emitter, node);
	for (uint32_t i = 0; i < node->aggregate.count; i++) {
		const tast_node *argument = tast_get(emitter->arena, children[i]);
		if (!argument || argument->kind != tast_slice) {
			tast_emit_expression(emitter, children[i]);
			continue;
		}
		if (argument->slice.has_end)
			tast_emit_expression(emitter, argument->slice.end);
		else {
			tast_emit_expression(emitter, node->aggregate.receiver);
			tstring *len_name = tstring_new("len");
			compile_emit_reference(emitter->cp, len_name,
				emitter->instructions, emitter->constants);
			tstring_free(len_name);
			tvmcmd_vect_append(emitter->instructions,
				tbycode_make_u(OP_EVAL, 1));
			treg_ctr_ddt_n(&emitter->cp->regctr, 2);
			treg_ctr_add(&emitter->cp->regctr);
		}
		if (argument->slice.has_start)
			tast_emit_expression(emitter, argument->slice.start);
		else {
			uint_csts zero = tconsts_add_int_const(emitter->constants, 0);
			tvmcmd_vect_append(emitter->instructions,
				tbycode_make_u(OP_PUSHI, zero));
			treg_ctr_add(&emitter->cp->regctr);
		}
		tvmcmd_vect_append(emitter->instructions, tbycode_make(OP_PAIR));
		treg_ctr_ddt_n(&emitter->cp->regctr, 2);
		treg_ctr_add(&emitter->cp->regctr);
	}
	tast_emit_expression(emitter, node->aggregate.receiver);
	tvmcmd_vect_append(emitter->instructions,
			   tbycode_make_u(OP_IDXR, node->aggregate.count));
	treg_ctr_ddt_n(&emitter->cp->regctr,
			  (uint_regs)(node->aggregate.count + 1));
	treg_ctr_add(&emitter->cp->regctr);
}

static void emit_list(tast_emitter *emitter, const tast_node *node)
{
	emit_arguments(emitter, node);
	tstring *list_name = tstring_new("list");
	compile_emit_reference(emitter->cp, list_name, emitter->instructions,
			       emitter->constants);
	tstring_free(list_name);
	tvmcmd_vect_append(emitter->instructions,
			   tbycode_make_u(OP_EVAL, node->aggregate.count));
	treg_ctr_ddt_n(&emitter->cp->regctr,
			  (uint_regs)(node->aggregate.count + 1));
	treg_ctr_add(&emitter->cp->regctr);
}

static void emit_dictionary(tast_emitter *emitter, const tast_node *node)
{
	const tast_id *children = aggregate_children(emitter, node);
	uint32_t entries = node->aggregate.count / 2;
	for (uint32_t i = 0; i < entries; i++) {
		/* Build the Pair expected by OP_PUSHDICT. */
		tast_emit_expression(emitter, children[i * 2 + 1]);
		tast_emit_expression(emitter, children[i * 2]);
		tvmcmd_vect_append(emitter->instructions, tbycode_make(OP_PAIR));
		treg_ctr_ddt_n(&emitter->cp->regctr, 2);
		treg_ctr_add(&emitter->cp->regctr);
	}
	tvmcmd_vect_append(emitter->instructions,
			   tbycode_make_u(OP_PUSHDICT, entries));
	treg_ctr_ddt_n(&emitter->cp->regctr, (uint_regs)entries);
	treg_ctr_add(&emitter->cp->regctr);
}

int tast_block_definitely_returns(const tast_arena *arena,
				  const tast_node *block)
{
	if (!block || (block->kind != tast_block && block->kind != tast_module))
		return 0;
	const tast_id *statements = tast_get_children(
		arena, block->aggregate.children, block->aggregate.count);
	for (uint32_t i = 0; i < block->aggregate.count; i++) {
		const tast_node *statement = tast_get(arena, statements[i]);
		if (!statement)
			continue;
		if (statement->kind == tast_return_statement)
			return 1;
		if (statement->kind != tast_if_statement)
			continue;
		int all_return = tast_block_definitely_returns(
			arena, tast_get(arena, statement->control_statement.body));
		int has_else = 0;
		uint32_t j = i + 1;
		for (; j < block->aggregate.count; j++) {
			const tast_node *branch = tast_get(arena, statements[j]);
			if (!branch || (branch->kind != tast_elif_statement &&
					branch->kind != tast_else_statement))
				break;
			all_return = all_return && tast_block_definitely_returns(
				arena, tast_get(arena, branch->control_statement.body));
			if (branch->kind == tast_else_statement) {
				has_else = 1;
				j++;
				break;
			}
		}
		if (has_else && all_return)
			return 1;
		if (j > i + 1)
			i = j - 1;
	}
	return 0;
}

static void emit_function(tast_emitter *emitter, const tast_node *node)
{
	const tast_id *parameter_ids = tast_get_children(
		emitter->arena, node->function.parameters,
		node->function.parameter_count);
	tstring **parameters = NULL;
	if (node->function.parameter_count) {
		parameters = (tstring **)malloc(
			node->function.parameter_count * sizeof(tstring *));
		if (!parameters)
			abort();
		for (uint32_t i = 0; i < node->function.parameter_count; i++)
			parameters[i] = node_text(emitter,
				tast_get(emitter->arena, parameter_ids[i])->parameter.name);
	}
	ttypeval *signature;
	if (emitter->pending_function_type) {
		signature = (ttypeval *)emitter->pending_function_type;
		ttypeval_retain(signature);
	} else
		signature = tast_function_signature(emitter, node);
	ttypeval *result_type = ttypeval_function_result(signature);
	if (result_type &&
	    !tast_block_definitely_returns(
		emitter->arena, tast_get(emitter->arena, node->function.body)) &&
	    !compile_type_assignable(
		ttypeval_builtin(ttype_builtin_nil), result_type))
		twarn(ErrCompile_Other, "function result",
		      "not every path returns the annotated Type");

	tcp function_cp;
	tcp_init_preload(&function_cp, parameters,
		(uint_objs)node->function.parameter_count,
		&emitter->cp->objctr, emitter->cp->interactive);
	for (uint32_t i = 0; i < node->function.parameter_count; i++) {
		ttypeval *parameter_type =
			ttypeval_function_parameter_at(signature, i);
		if (parameter_type)
			tcompile_set_metadata(&function_cp.objctr, i,
				parameter_type, NULL, 1);
	}
	tvmcmd_vect body;
	tvmcmd_vect_init(&body);
	tast_emitter function_emitter = *emitter;
	function_emitter.cp = &function_cp;
	function_emitter.instructions = &body;
	function_emitter.current_function_type = signature;
	function_emitter.expected_return_type = result_type;
	function_emitter.pending_function_type = NULL;
	tast_emit_block(&function_emitter,
		tast_get(emitter->arena, node->function.body),
		emitter->paths, emitter->npaths, 0);
	tcinfo info = tcp_get_compile_info(&function_cp);

	uint_regs parameter_count = node->function.variadic ?
		UNDEF_NPARAMS : (uint_regs)node->function.parameter_count;
	uint_cmds instruction_count = tvmcmd_vect_size32(&body);
	tvmcmd_vect_append(emitter->instructions,
		tbycode_make_u(OP_PUSHINFO, (uint32_t)info.obj_max));
	tvmcmd_vect_append(emitter->instructions,
		tbycode_make_u(OP_PUSHINFO, (uint32_t)info.tmp_max));
	tvmcmd_vect_append(emitter->instructions,
		tbycode_make_u(OP_PUSHINFO, (uint32_t)info.reg_max));
	tvmcmd_vect_append(emitter->instructions,
		tbycode_make_u(OP_PUSHINFO, (uint32_t)parameter_count));
	treg_ctr_add_n(&emitter->cp->regctr, 4);
	tvmcmd_vect_append(emitter->instructions,
		tbycode_make_u(OP_PUSHF, instruction_count));
	treg_ctr_ddt_n(&emitter->cp->regctr, 4);
	treg_ctr_add(&emitter->cp->regctr);
	tvmcmd_vect_insert_vect(emitter->instructions,
		tvmcmd_vect_size32(emitter->instructions), &body);

	for (uint32_t i = 0; i < node->function.parameter_count; i++)
		tstring_free(parameters[i]);
	free(parameters);
	tvmcmd_vect_free(&body);
	tcp_free(&function_cp);
	ttypeval_release(signature);
}

void tast_emit_expression(tast_emitter *emitter, tast_id id)
{
	const tast_node *node = tast_get(emitter->arena, id);
	switch (node->kind) {
	case tast_name:
		emit_name(emitter, node);
		break;
	case tast_bool: {
		tstring *value = node_text(emitter, node->span);
		tvmcmd_vect_append(emitter->instructions,
			tbycode_make_u(OP_PUSHB,
				       strcmp(tstring_cstr(value), "true") == 0));
		treg_ctr_add(&emitter->cp->regctr);
		tstring_free(value);
	} break;
	case tast_integer:
	case tast_float:
		emit_number(emitter, node);
		break;
	case tast_string:
		emit_string(emitter, node);
		break;
	case tast_group:
		tast_emit_expression(emitter, node->group.value);
		break;
	case tast_unary:
		tast_emit_expression(emitter, node->unary.operand);
		tvmcmd_vect_append(emitter->instructions,
			tbycode_make(node->unary.op == tsyntax_plus ? OP_POS : OP_NEG));
		break;
	case tast_binary:
		emit_binary(emitter, node);
		break;
	case tast_call:
		emit_call(emitter, node);
		break;
	case tast_index:
		emit_index(emitter, node);
		break;
	case tast_member:
		emit_member(emitter, node);
		break;
	case tast_list:
		emit_list(emitter, node);
		break;
	case tast_dictionary:
		emit_dictionary(emitter, node);
		break;
	case tast_function:
		emit_function(emitter, node);
		break;
	default:
		twarn(ErrCompile_Other, "AST expression",
		      "unsupported expression node");
	}
}
