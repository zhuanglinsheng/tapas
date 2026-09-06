/** Bytecode emission from the reusable expression AST. */
#include "ast_emit_internal.h"
#include "tapas/runtime/trule_ir.h"

#include <stdio.h>
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

void tast_emit_expression(tast_emitter *emitter, tast_id id);
static void emit_name(tast_emitter *emitter, const tast_node *node);

static void emit_reference(tast_emitter *emitter, tsource_span span)
{
	tast_node name = { .kind = tast_name, .span = span };
	emit_name(emitter, &name);
}

static void emit_name(tast_emitter *emitter, const tast_node *node)
{
	tstring *name = tast_emitter_text(emitter, node->span);
	const char *text = tstring_cstr(name);
	if (strcmp(text, "this") == 0) {
		tvmcmd_vect_append(emitter->instructions, tbycode_make(OP_THIS));
		treg_ctr_add(&emitter->cp->regctr);
	} else if (strcmp(text, "base") == 0) {
		tvmcmd_vect_append(emitter->instructions, tbycode_make(OP_BASE));
		treg_ctr_add(&emitter->cp->regctr);
	} else {
		compile_emit_reference(emitter->cp, name, emitter->instructions,
				       emitter->constants);
	}
	tstring_free(name);
}

static void emit_number(tast_emitter *emitter, const tast_node *node)
{
	tstring *literal = tast_emitter_text(emitter, node->span);
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
	tstring *value = tast_emitter_text(emitter, contents);
	uint_csts index = tconsts_add_str_const(emitter->constants,
					       tstring_cstr(value));
	tvmcmd_vect_append(emitter->instructions,
			   tbycode_make_u(OP_PUSHS, index));
	treg_ctr_add(&emitter->cp->regctr);
	tstring_free(value);
}

static uint32_t source_logic_index(tast_emitter *emitter, tast_id id);

static void emit_short_circuit(tast_emitter *emitter,
			       const tast_node *node, int instruction)
{
	tast_id id = (tast_id)(node - emitter->arena->nodes);
	const tast_node *owner = tast_get(emitter->arena,
		tcontrol_enclosing_function(&emitter->frontend->flow, id));
	int rule_logic = owner && owner->kind == tast_rule;
	uint32_t slot = rule_logic ? source_logic_index(emitter, id) : 0;
	tast_emit_expression(emitter, node->binary.left);
	if (rule_logic) tvmcmd_vect_append(emitter->instructions,
		tbycode_make_u(OP_RULETRUTH, slot + 1));
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
	if (rule_logic) tvmcmd_vect_append(emitter->instructions,
		tbycode_make_u(OP_RULETRUTH, slot + 2));
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
	if (rule_logic) tvmcmd_vect_append(outer, tbycode_make_u(OP_RULETRUTH, slot));
}

static void emit_binary(tast_emitter *emitter, const tast_node *node)
{
	int instruction = binary_instruction(node->binary.op);
	if (instruction == OP_AND || instruction == OP_OR) {
		emit_short_circuit(emitter, node, instruction);
		return;
	}

    if (instruction == OP_IN) {
        tast_id id = (tast_id)(node - emitter->arena->nodes);
        const tast_node *owner = tast_get(emitter->arena, tcontrol_enclosing_function(&emitter->frontend->flow, id));
        if (owner && owner->kind == tast_rule) {
            uint32_t slot = source_logic_index(emitter, id);
            tast_emit_expression(emitter, node->binary.right);
            tvmcmd_vect_append(emitter->instructions, tbycode_make_u(OP_RULEVALUE, slot + 2));
            tast_emit_expression(emitter, node->binary.left);
            tvmcmd_vect_append(emitter->instructions, tbycode_make_u(OP_RULEVALUE, slot + 1));
            tvmcmd_vect_append(emitter->instructions, tbycode_make(OP_IN));
            treg_ctr_ddt(&emitter->cp->regctr);
            tvmcmd_vect_append(emitter->instructions, tbycode_make_u(OP_RULEVALUE, slot));
            return;
        }
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
	tstring *name = tast_emitter_text(emitter, node->member.name);
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
		tstring *name = tast_emitter_text(emitter, callee->span);
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

static ttypeval *resolve_value_annotation(tast_emitter *emitter,
		tsource_span annotation, int present, ttypeval *inferred)
{
	if (present) {
		/* Only value-bound syntax needs rebinding in the backend. Ordinary
		 * annotations retain the frontend's lexical Type resolution. */
		const tsyntax_tokens *tokens = &emitter->frontend->tokens;
		for (uint32_t i = 0; i < tokens->count; i++) {
			const tsyntax_token *token = &tokens->items[i];
			if (token->span.start >= annotation.end) break;
			if (token->span.start < annotation.start ||
			    token->kind != tsyntax_identifier) continue;
			tstring *name = tast_emitter_text(emitter, token->span);
			int value_bound = tstring_eq_cstr(name, "InstanceOf") || tstring_eq_cstr(name, "PointsOf") || tstring_eq_cstr(name, "RangeOf");
			tstring_free(name);
			if (value_bound) return tast_resolve_annotation(emitter, annotation);
		}
	}
	return ttypeval_retain(inferred);
}

static void emit_function(tast_emitter *emitter, const tast_node *node)
{
	const tast_id *parameter_ids = tast_get_children(
		emitter->arena, node->function.parameters,
		node->function.parameter_count);
	tstring **parameters = nullptr;
	if (node->function.parameter_count) {
		parameters = (tstring **)malloc(
			node->function.parameter_count * sizeof(tstring *));
		if (!parameters)
			abort();
		for (uint32_t i = 0; i < node->function.parameter_count; i++)
			parameters[i] = tast_emitter_text(emitter,
				tast_get(emitter->arena, parameter_ids[i])->parameter.name);
	}
	ttypeval *signature;
	if (emitter->pending_function_type) {
		signature = (ttypeval *)emitter->pending_function_type;
		ttypeval_retain(signature);
	} else
		signature = tast_function_signature(emitter, node);
	/* Re-resolve explicit annotations at the definition site. Frontend Unknown
	 * must not silently erase a value-bound contract from runtime metadata. */
	uint32_t count = node->function.parameter_count;
	ttypeval **resolved = calloc(count + 1, sizeof(*resolved));
	if (!resolved) abort();
	for (uint32_t i = 0; i < count; i++) {
		const tast_node *parameter = tast_get(emitter->arena, parameter_ids[i]);
		resolved[i] = resolve_value_annotation(emitter,
			parameter->parameter.annotation, parameter->parameter.has_annotation,
			ttypeval_function_parameter_at(signature, i));
	}
	resolved[count] = resolve_value_annotation(emitter,
		node->function.return_annotation, node->function.has_return_annotation,
		ttypeval_function_result(signature));
	ttypeval *bound_signature = ttypeval_retain(ttypeval_new_function(
		resolved, count, resolved[count], node->function.variadic));
	for (uint32_t i = 0; i <= count; i++) ttypeval_release(resolved[i]);
	free(resolved);
	ttypeval_release(signature);
	signature = bound_signature;
	tcp function_cp;
	tcp_init_preload(&function_cp, parameters,
		(uint_objs)node->function.parameter_count,
		&emitter->cp->objctr, emitter->cp->preload_library,
		emitter->cp->interactive);
	for (uint32_t i = 0; i < node->function.parameter_count; i++) {
		ttypeval *parameter_type =
			ttypeval_function_parameter_at(signature, i);
		if (parameter_type)
			tcompile_set_metadata(&function_cp.objctr, i,
				parameter_type, nullptr, 1);
	}
	tvmcmd_vect body;
	tvmcmd_vect_init(&body);
	tast_emitter function_emitter = *emitter;
	function_emitter.cp = &function_cp;
	function_emitter.instructions = &body;
	function_emitter.pending_function_type = nullptr;
	function_emitter.pending_display_name = nullptr;
	tast_emit_block(&function_emitter,
		tast_get(emitter->arena, node->function.body), 0);
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
	/* Attach declaration metadata outside the body: reflection never calls it. */
	tstring *names = tstring_new_empty();
    if (emitter->pending_display_name) {
        tstring_append(names, emitter->pending_display_name);
        tstring_append_c(names, '\x1e');
    }
	for (uint32_t i = 0; i < node->function.parameter_count; i++) {
		if (i) tstring_append_c(names, '\x1f');
		tstring_append_ts(names, parameters[i]);
	}
	tvmcmd_vect_append(emitter->instructions, tbycode_make_u(OP_PUSHS,
		tconsts_add_str_const(emitter->constants, tstring_cstr(names))));
	treg_ctr_add(&emitter->cp->regctr);
	if (signature->contains_instance) tast_emit_bound_type(emitter, signature);
	else {
		tvmcmd_vect_append(emitter->instructions, tbycode_make_u(OP_PUSHS,
			tconsts_add_str_const(emitter->constants, tstring_cstr(signature->canonical))));
		treg_ctr_add(&emitter->cp->regctr);
	}
	tvmcmd_vect_append(emitter->instructions, tbycode_make(OP_FUNCMETA));
	treg_ctr_ddt_n(&emitter->cp->regctr, 2);
	tstring_free(names);

	for (uint32_t i = 0; i < node->function.parameter_count; i++)
		tstring_free(parameters[i]);
	free(parameters);
	tvmcmd_vect_free(&body);
	tcp_free(&function_cp);
	ttypeval_release(signature);
}

static void append_metadata_field(tstring *metadata, const char *text);

static int is_rule_logic(const tast_node *node)
{
	return node && ((node->kind == tast_unary && node->unary.op == tsyntax_kw_not) ||
		(node->kind == tast_binary && (node->binary.op == tsyntax_kw_in || node->binary.op == tsyntax_kw_and || node->binary.op == tsyntax_kw_or)));
}


/* Stable within a Rule, independent of unrelated preceding AST nodes. */
static uint32_t source_logic_index(tast_emitter *emitter, tast_id id)
{
	tast_id owner = tcontrol_enclosing_function(&emitter->frontend->flow, id);
	uint32_t index = 0;
	for (tast_id candidate = 0; candidate < id; candidate++) {
		const tast_node *node = tast_get(emitter->arena, candidate);
		if (is_rule_logic(node) &&
		    tcontrol_enclosing_function(&emitter->frontend->flow, candidate) == owner) index++;
	}
	return index * 3;
}

static void append_metadata_number(tstring *metadata, uint32_t value)
{
	char text[32];
	snprintf(text, sizeof(text), "%u", value);
	append_metadata_field(metadata, text);
}

/* Expression slots occupy a separate range from the legacy logic records. */
static uint32_t source_expression_index(tast_emitter *emitter, tast_id id)
{
    tast_id owner = tcontrol_enclosing_function(&emitter->frontend->flow, id);
    uint32_t count = 0, before = 0;
    for (tast_id i = 0; i < emitter->arena->node_count; i++) {
        if (tcontrol_enclosing_function(&emitter->frontend->flow, i) != owner) continue;
        count++;
        if (i < id) before++;
    }
    return count * 3 + before;
}

static const char *source_operator(tsyntax_kind op)
{
    switch (op) {
    case tsyntax_plus: return "+";
    case tsyntax_minus: return "-";
    case tsyntax_star: return "*";
    case tsyntax_slash: return "/";
    case tsyntax_percent: return "%";
    case tsyntax_power: return "^";
    case tsyntax_matmul: return "@";
    case tsyntax_eq: return "==";
    case tsyntax_ne: return "!=";
    case tsyntax_gt: return ">";
    case tsyntax_ge: return ">=";
    case tsyntax_lt: return "<";
    case tsyntax_le: return "<=";
    case tsyntax_kw_and: return "and";
    case tsyntax_kw_or: return "or";
    case tsyntax_kw_not: return "not";
    case tsyntax_kw_in: return "in";
    case tsyntax_kw_to: return "to";
    case tsyntax_colon: return "pair";
    case tsyntax_element_and: return "&";
    case tsyntax_element_or: return "|";
    default: return "unsupported";
    }
}

/* A tunnel target is emitted as a lexical reference by the checker, so retain
 * that exact environment address even though it has no standalone Name AST. */
static void append_source_expression(tast_emitter *emitter, tstring *metadata, tast_id id, unsigned depth);

static void append_source_capture(tast_emitter *emitter, tstring *metadata,
                                  const tstring *name, tsource_span origin, tast_id member, unsigned depth)
{
    tast_id owner_id = tcontrol_enclosing_function(&emitter->frontend->flow, member);
    const tast_node *owner = tast_get(emitter->arena, owner_id);
    for (uint32_t i = 0; i < emitter->frontend->semantic.symbol_count; i++) {
        const tsemantic_symbol *symbol = &emitter->frontend->semantic.symbols[i];
        if (!tstring_eq(symbol->name, name) || symbol->declaration == TAST_INVALID_ID ||
            tcontrol_enclosing_function(&emitter->frontend->flow, symbol->declaration) != owner_id ||
            symbol->span.start > origin.start) continue;
        const tast_node *decl = tast_get(emitter->arena, symbol->declaration);
        if (decl->kind == tast_declaration_statement && decl->declaration_statement.has_initializer) {
            append_source_expression(emitter, metadata, decl->declaration_statement.initializer, depth + 1);
            return;
        }
        const tast_id *ids = tast_get_children(emitter->arena, owner->function.parameters, owner->function.parameter_count);
        for (uint32_t j = 0; j < owner->function.parameter_count; j++) if (ids[j] == symbol->declaration) {
            tstring_append_c(metadata, 'P'); append_metadata_number(metadata, j); return;
        }
    }
    tobj_ctr_addr address;
    if (!tobj_ctr_obj_addr(&emitter->cp->objctr, name, &address) || address.depth == 0)
        twarn(ErrCompile_Other, "RuleIR", "unresolved tunnel target");
    tstring_append_c(metadata, 'T');
    append_metadata_number(metadata, address.depth);
    append_metadata_number(metadata, address.slot);
    append_metadata_field(metadata, tstring_cstr(name));
    append_metadata_number(metadata, origin.start);
    append_metadata_number(metadata, origin.end);
}

static void append_source_expression(tast_emitter *emitter, tstring *metadata,
                                     tast_id id, unsigned depth)
{
    if (depth > 256) twarn(ErrCompile_Other, "RuleIR", "expression nesting limit exceeded");
    const tast_node *node = tast_get(emitter->arena, id);
    if (node->kind == tast_group) {
        append_source_expression(emitter, metadata, node->group.value, depth + 1);
        return;
    }
    if (emitter->rule_ir_emitted[id] == 2) {
        tstring_append_c(metadata, 'D');
        append_metadata_number(metadata, source_expression_index(emitter, id));
        return;
    }
    if (emitter->rule_ir_emitted[id] == 1)
        twarn(ErrCompile_Other, "RuleIR", "cyclic local initializer");
    tast_id owner_id = tcontrol_enclosing_function(&emitter->frontend->flow, id);
    const tast_node *owner = tast_get(emitter->arena, owner_id);
    const char *tag = "opaque";
    tstring *text = tstring_new_empty();
    tast_id operands[2];
    const tast_id *children = operands;
    uint32_t count = 0;
    if (node->kind == tast_name) {
        const tsemantic_symbol *symbol = tsemantic_resolved_symbol(&emitter->frontend->semantic, id);
        if (symbol && owner) {
            const tast_id *parameters = tast_get_children(emitter->arena,
                owner->function.parameters, owner->function.parameter_count);
            for (uint32_t i = 0; i < owner->function.parameter_count; i++) {
                if (parameters[i] != symbol->declaration) continue;
                tstring_append_c(metadata, 'P');
                append_metadata_number(metadata, i);
                tstring_free(text);
                return;
            }
            const tast_node *decl = tast_get(emitter->arena, symbol->declaration);
            if (decl && decl->kind == tast_declaration_statement &&
                tcontrol_enclosing_function(&emitter->frontend->flow, symbol->declaration) == owner_id &&
                decl->declaration_statement.has_initializer) {
                tag = "local";
                tstring_append_fmt(text, "%u:", source_expression_index(emitter, symbol->declaration));
                tstring_append_ts(text, symbol->name);
                operands[count++] = decl->declaration_statement.initializer;
            } else {
                /* Resolve in the Rule checker scope, never by a runtime name lookup. */
                tobj_ctr_addr address;
                if (tobj_ctr_obj_addr(&emitter->cp->objctr, symbol->name, &address) && address.depth > 0) {
                    tstring_append_c(metadata, 'C');
                    append_metadata_number(metadata, address.depth);
                    append_metadata_number(metadata, address.slot);
                    tstring_free(text);
                    return;
                }
                tag = "reference";
                tstring_append_ts(text, symbol->name);
            }
        } else {
            tag = "reference";
            tstring_free(text); text = tast_emitter_text(emitter, node->span);
        }
    } else if (node->kind == tast_integer || node->kind == tast_float ||
               node->kind == tast_bool || node->kind == tast_string || node->kind == tast_nil) {
        tag = node->kind == tast_integer ? "int" : node->kind == tast_float ? "float" :
              node->kind == tast_bool ? "bool" : node->kind == tast_string ? "string" : "nil";
        tstring_free(text);
        if (node->kind == tast_string) {
            tsource_span contents = node->span; contents.start++; contents.end--;
            text = tast_emitter_text(emitter, contents);
        } else text = tast_emitter_text(emitter, node->span);
        /* Store normalized numbers using the same parser as bytecode emission. */
        if (node->kind == tast_integer) {
            long value = 0;
            if (!str_to_long_int(text, &value)) twarn(ErrCompile_InvalidLiter, "RuleIR", tstring_cstr(text));
            tstring_free(text); text = tstring_new_empty(); tstring_append_fmt(text, "%ld", value);
        } else if (node->kind == tast_float) {
            double value = 0;
            if (!str_to_float(text, &value)) twarn(ErrCompile_InvalidLiter, "RuleIR", tstring_cstr(text));
            tstring_free(text); text = tstring_new_empty(); tstring_append_fmt(text, "%.17g", value);
        }
    } else if (node->kind == tast_unary || node->kind == tast_binary) {
        tag = "intrinsic";
        tstring_append(text, node->kind == tast_unary && node->unary.op == tsyntax_minus ? "neg" :
                            node->kind == tast_unary && node->unary.op == tsyntax_plus ? "pos" :
                            source_operator(node->kind == tast_unary ? node->unary.op : node->binary.op));
        operands[count++] = node->kind == tast_unary ? node->unary.operand : node->binary.left;
        if (node->kind == tast_binary) operands[count++] = node->binary.right;
    } else if (node->kind == tast_member) {
        tag = node->member.op == tsyntax_dot ? "tunnel" : "member";
        tstring_free(text); text = tast_emitter_text(emitter, node->member.name);
        operands[count++] = node->member.receiver;
    } else if (node->kind == tast_call || node->kind == tast_index ||
               node->kind == tast_list || node->kind == tast_dictionary) {
        tag = node->kind == tast_call ? "call" : node->kind == tast_index ? "index" :
              node->kind == tast_list ? "list" : "dictionary";
        children = tast_get_children(emitter->arena, node->aggregate.children, node->aggregate.count);
        count = node->aggregate.count;
    } else if (node->kind == tast_slice) {
        tag = "slice";
        tstring_append(text, node->slice.has_start ? "start" : "default-start");
        tstring_append(text, node->slice.has_end ? ":end" : ":default-end");
        if (node->slice.has_start) operands[count++] = node->slice.start;
        if (node->slice.has_end) operands[count++] = node->slice.end;
    } else {
        /* Function/Rule literals remain explicitly opaque; do not traverse a new lexical scope. */
        tstring_free(text); text = tast_emitter_text(emitter, node->span);
    }
    emitter->rule_ir_emitted[id] = 1;
    tstring_append_c(metadata, 'E');
    append_metadata_number(metadata, node->span.start);
    append_metadata_number(metadata, node->span.end);
    append_metadata_number(metadata, source_expression_index(emitter, id));
    append_metadata_field(metadata, tag);
    append_metadata_field(metadata, tstring_cstr(text));
    tstring_free(text);
    ttypeval *type = tast_infer_expression_type(emitter, id, nullptr);
    append_metadata_field(metadata, type ? tstring_cstr(type->canonical) : "A");
    ttypeval_release(type);
    int receiver = node->kind == tast_call || node->kind == tast_index;
    int tunnel = node->kind == tast_member && node->member.op == tsyntax_dot;
    append_metadata_number(metadata, count + receiver + tunnel);
    if (tunnel) {
        tstring *name = tast_emitter_text(emitter, node->member.name);
        append_source_capture(emitter, metadata, name, node->member.name, id, depth);
        tstring_free(name);
    }
    if (receiver) append_source_expression(emitter, metadata, node->aggregate.receiver, depth + 1);
    for (uint32_t i = 0; i < count; i++)
        append_source_expression(emitter, metadata, children[i], depth + 1);
    emitter->rule_ir_emitted[id] = 2;
}

static void append_source_tree(tast_emitter *emitter, tstring *metadata, tast_id id)
{
    append_source_expression(emitter, metadata, id, 0);
}

static void append_rule_metadata(tast_emitter *emitter, tstring *metadata,
				 char kind, const tstring *description,
				 const tast_node *expression, tsource_span origin)
{
	tstring *text = expression ? tast_emitter_text(emitter, expression->span) :
		tstring_new_empty();
	char start[32];
	char end[32];
	snprintf(start, sizeof(start), "%u", (unsigned)origin.start);
	snprintf(end, sizeof(end), "%u", (unsigned)origin.end);
	tast_id expression_id = expression ? (tast_id)(expression - emitter->arena->nodes) : TAST_INVALID_ID;
	int has_logic = expression != nullptr;
	if (kind == 'C' && expression) {
		ttypeval *type = tast_infer_expression_type(emitter, expression_id, nullptr);
		if (type && (type->kind == ttype_kind_rule_instance || type->kind == ttype_kind_instance_of ||
		    ttypeval_equal(type, ttypeval_builtin(tbuiltin_rule_instance)))) kind = 'R';
		ttypeval_release(type);
	}
	tstring_append_c(metadata, has_logic ? kind + ('a' - 'A') : kind);
	tstring_append_fmt(metadata, "%zu:", tstring_len(description));
	tstring_append_ts(metadata, description);
	tstring_append_fmt(metadata, "%zu:", tstring_len(text));
	tstring_append_ts(metadata, text);
	tstring_append_fmt(metadata, "%zu:%s", strlen(start), start);
	tstring_append_fmt(metadata, "%zu:%s", strlen(end), end);
	tstring_free(text);
	if (has_logic) {
		tstring *tree = tstring_new_empty();
		append_source_tree(emitter, tree, expression_id);
		append_metadata_field(metadata, tstring_cstr(tree));
		tstring_free(tree);
	}
}

static void append_metadata_field(tstring *metadata, const char *text)
{
	size_t length = text ? strlen(text) : 0;
	tstring_append_fmt(metadata, "%zu:", length);
	if (length) tstring_append(metadata, text);
}

static void emit_rule(tast_emitter *emitter, const tast_node *node)
{
	const tast_id *parameter_ids = tast_get_children(
		emitter->arena, node->function.parameters,
		node->function.parameter_count);
	tstring **parameters = node->function.parameter_count ?
		(tstring **)calloc(node->function.parameter_count, sizeof(*parameters)) :
		nullptr;
	if (node->function.parameter_count && !parameters) abort();
	for (uint32_t i = 0; i < node->function.parameter_count; i++)
		parameters[i] = tast_emitter_text(emitter,
			tast_get(emitter->arena, parameter_ids[i])->parameter.name);
	tcp rule_cp;
	tcp_init_preload(&rule_cp, parameters,
		(uint_objs)node->function.parameter_count,
		&emitter->cp->objctr, emitter->cp->preload_library,
		emitter->cp->interactive);
	tstring *signature = tstring_new_empty();
	tstring *parameter_names = tstring_new_empty();
    if (emitter->pending_display_name) {
        tstring_append(parameter_names, emitter->pending_display_name);
        tstring_append_c(parameter_names, '\x1e');
    }
	tstring *item_metadata = tstring_new_empty();
	tstring *capture_metadata = tstring_new_empty();
	for (uint32_t i = 0; i < node->function.parameter_count; i++) {
		const tast_node *parameter = tast_get(emitter->arena, parameter_ids[i]);
		ttypeval *type = tast_resolve_annotation(
			emitter, parameter->parameter.annotation);
		tcompile_set_metadata(&rule_cp.objctr, i, type, nullptr, 1);
		if (i) tstring_append_c(signature, '\x1f');
		tstring_append_ts(signature, type->canonical);
		if (i) tstring_append_c(parameter_names, '\x1f');
		tstring_append_ts(parameter_names, parameters[i]);
		ttypeval_release(type);
	}
	tast_emitter metadata_emitter = *emitter;
	metadata_emitter.cp = &rule_cp;
    metadata_emitter.rule_ir_emitted = calloc(emitter->arena->node_count, 1);
    if (!metadata_emitter.rule_ir_emitted) abort();
	const tast_node *rule_body = tast_get(emitter->arena, node->function.body);
	if (rule_body && rule_body->kind == tast_block) {
		const tast_id *items = tast_get_children(emitter->arena,
			rule_body->aggregate.children, rule_body->aggregate.count);
		for (uint32_t i = 0; i < rule_body->aggregate.count; i++) {
			const tast_node *item = tast_get(emitter->arena, items[i]);
			if (item && item->kind == tast_rule_implication) {
				tstring *description;
				if (item->rule_implication.has_description) {
					tast_node literal = { .kind = tast_string,
						.span = item->rule_implication.description };
					description = tast_string_value(emitter->document, &literal);
				} else description = tast_emitter_text(emitter, item->span);
				const tast_node *guard = tast_get(emitter->arena,
					item->rule_implication.antecedent);
				append_rule_metadata(&metadata_emitter, item_metadata, 'J', description, guard, item->span);
				ttypeval *guard_type = tast_infer_expression_type(emitter,
					item->rule_implication.antecedent, nullptr);
				if (!guard_type || !trule_antecedent_type(guard_type)) {
					ttypeval_release(guard_type);
					ttypeval *members[] = { ttypeval_builtin(tbuiltin_bool),
						ttypeval_builtin(tbuiltin_rule_instance) };
					guard_type = ttypeval_new_union(members, 2);
				}
				append_metadata_field(item_metadata, tstring_cstr(guard_type->canonical));
				ttypeval_release(guard_type);
				const tast_node *body = tast_get(emitter->arena,
					item->rule_implication.consequent);
				uint32_t count = body->kind == tast_block ? body->aggregate.count : 1;
				char number[32];
				snprintf(number, sizeof(number), "%u", count);
				append_metadata_field(item_metadata, number);
				const tast_id *children = body->kind == tast_block ? tast_get_children(
					emitter->arena, body->aggregate.children, count) : nullptr;
				for (uint32_t j = 0; j < count; j++) {
					const tast_node *value = children ? tast_get(emitter->arena,
						tast_get(emitter->arena, children[j])->expression_statement.value) : body;
					append_rule_metadata(&metadata_emitter, item_metadata, 'C', description, value, value->span);
				}
				tstring_free(description);
				continue;
			}
			if (!item || item->kind != tast_rule_condition) continue;
			tstring *description;
			tast_id expression_id = item->rule_condition.value;
			if (item->rule_condition.has_description) {
				tast_node literal = { .kind = tast_string,
					.span = item->rule_condition.description };
				description = tast_string_value(emitter->document, &literal);
			} else description = tast_emitter_text(emitter, item->span);
			const tast_node *expression = tast_get(emitter->arena, expression_id);
			if (item->kind == tast_rule_condition && expression &&
			    expression->kind == tast_block) {
				const tast_id *expressions = tast_get_children(
					emitter->arena, expression->aggregate.children,
					expression->aggregate.count);
				for (uint32_t j = 0; j < expression->aggregate.count; j++) {
					const tast_node *statement = tast_get(
						emitter->arena, expressions[j]);
					const tast_node *value = statement &&
						statement->kind == tast_expression_statement ?
						tast_get(emitter->arena,
							statement->expression_statement.value) : nullptr;
					append_rule_metadata(&metadata_emitter, item_metadata, 'C',
						description, value,
						value ? value->span : item->span);
				}
			} else
				append_rule_metadata(&metadata_emitter, item_metadata,
					'C',
					description, expression, item->span);
			tstring_free(description);
		}
	}
	free(metadata_emitter.rule_ir_emitted);
	uint8_t *captured = emitter->frontend->semantic.symbol_count ? calloc(
		emitter->frontend->semantic.symbol_count, sizeof(*captured)) : nullptr;
	for (tast_id id = 0; rule_body && id < emitter->arena->node_count; id++) {
		const tast_node *reference = tast_get(emitter->arena, id);
		if (!reference || reference->kind != tast_name ||
		    reference->span.start < rule_body->span.start ||
		    reference->span.end > rule_body->span.end ||
            tcontrol_enclosing_function(&emitter->frontend->flow, id) != (tast_id)(node - emitter->arena->nodes)) continue;
		const tsemantic_symbol *symbol = tsemantic_resolved_symbol(
			&emitter->frontend->semantic, id);
		if (!symbol) continue;
        if (symbol->declaration != TAST_INVALID_ID &&
            tcontrol_enclosing_function(&emitter->frontend->flow, symbol->declaration) ==
            (tast_id)(node - emitter->arena->nodes)) continue;
		uint32_t symbol_id = (uint32_t)(symbol -
			emitter->frontend->semantic.symbols);
		if (captured[symbol_id]) continue;
		captured[symbol_id] = 1;
		tobj_ctr_addr address;
		if (!tobj_ctr_obj_addr(&rule_cp.objctr, symbol->name, &address) ||
		    address.depth == 0) continue;
		ttypeval *type = tast_infer_expression_type(emitter, id, nullptr);
		if (!type) type = ttypeval_builtin(tbuiltin_any);
		char depth[32];
		char slot[32];
		char start[32];
		char end[32];
		snprintf(depth, sizeof(depth), "%u", (unsigned)address.depth);
		snprintf(slot, sizeof(slot), "%u", (unsigned)address.slot);
		snprintf(start, sizeof(start), "%u", (unsigned)reference->span.start);
		snprintf(end, sizeof(end), "%u", (unsigned)reference->span.end);
		append_metadata_field(capture_metadata,
			tstring_cstr(symbol->name));
		append_metadata_field(capture_metadata,
			tstring_cstr(type->canonical));
		append_metadata_field(capture_metadata, depth);
		append_metadata_field(capture_metadata, slot);
		append_metadata_field(capture_metadata, start);
		append_metadata_field(capture_metadata, end);
		ttypeval_release(type);
	}
	free(captured);
	tvmcmd_vect body;
	tvmcmd_vect_init(&body);
	tast_emitter checker = *emitter;
	checker.cp = &rule_cp;
	checker.instructions = &body;
	checker.pending_function_type = nullptr;
	checker.pending_display_name = nullptr;
	tast_emit_block(&checker,
		tast_get(emitter->arena, node->function.body), 0);
	tcinfo info = tcp_get_compile_info(&rule_cp);
	tvmcmd_vect_append(emitter->instructions,
		tbycode_make_u(OP_PUSHINFO, (uint32_t)info.obj_max));
	tvmcmd_vect_append(emitter->instructions,
		tbycode_make_u(OP_PUSHINFO, (uint32_t)info.tmp_max));
	tvmcmd_vect_append(emitter->instructions,
		tbycode_make_u(OP_PUSHINFO, (uint32_t)info.reg_max));
	tvmcmd_vect_append(emitter->instructions,
		tbycode_make_u(OP_PUSHINFO, node->function.parameter_count));
	treg_ctr_add_n(&emitter->cp->regctr, 4);
	tvmcmd_vect_append(emitter->instructions,
		tbycode_make_u(OP_PUSHF, tvmcmd_vect_size32(&body)));
	treg_ctr_ddt_n(&emitter->cp->regctr, 4);
	treg_ctr_add(&emitter->cp->regctr);
	tvmcmd_vect_insert_vect(emitter->instructions,
		tvmcmd_vect_size32(emitter->instructions), &body);
	tstring *source = tast_emitter_text(emitter, node->span);
	uint_csts source_id = tconsts_add_str_const(
		emitter->constants, tstring_cstr(source));
	tstring_free(source);
	uint_csts signature_id = tconsts_add_str_const(
		emitter->constants, tstring_cstr(signature));
	tstring_free(signature);
	uint_csts names_id = tconsts_add_str_const(
		emitter->constants, tstring_cstr(parameter_names));
	tstring_free(parameter_names);
	uint_csts metadata_id = tconsts_add_str_const(
		emitter->constants, tstring_cstr(item_metadata));
	tstring_free(item_metadata);
	uint_csts captures_id = tconsts_add_str_const(
		emitter->constants, tstring_cstr(capture_metadata));
	tstring_free(capture_metadata);
	tvmcmd_vect_append(emitter->instructions,
		tbycode_make_u(OP_PUSHS, signature_id));
	treg_ctr_add(&emitter->cp->regctr);
	tvmcmd_vect_append(emitter->instructions,
		tbycode_make_u(OP_PUSHS, names_id));
	treg_ctr_add(&emitter->cp->regctr);
	tvmcmd_vect_append(emitter->instructions,
		tbycode_make_u(OP_PUSHS, metadata_id));
	treg_ctr_add(&emitter->cp->regctr);
	tvmcmd_vect_append(emitter->instructions,
		tbycode_make_u(OP_PUSHS, captures_id));
	treg_ctr_add(&emitter->cp->regctr);
	tvmcmd_vect_append(emitter->instructions,
		tbycode_make_u(OP_PUSHRULE, source_id));
	treg_ctr_ddt_n(&emitter->cp->regctr, 4);
	ttypeval *rule_type = tast_infer_expression_type(emitter,
		(tast_id)(node - emitter->arena->nodes), nullptr);
	if (rule_type && rule_type->contains_instance) {
		tast_emit_bound_type(emitter, rule_type);
		tvmcmd_vect_append(emitter->instructions, tbycode_make(OP_RULETYPE));
		treg_ctr_ddt(&emitter->cp->regctr);
	}
	ttypeval_release(rule_type);
	for (uint32_t i = 0; i < node->function.parameter_count; i++)
		tstring_free(parameters[i]);
	free(parameters);
	tvmcmd_vect_free(&body);
	tcp_free(&rule_cp);
}

void tast_emit_expression(tast_emitter *emitter, tast_id id)
{
	const tast_node *node = tast_get(emitter->arena, id);
	switch (node->kind) {
	case tast_name:
		emit_name(emitter, node);
		break;
	case tast_bool: {
		tstring *value = tast_emitter_text(emitter, node->span);
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
		if (node->unary.op == tsyntax_kw_not) {
			const tast_node *owner = tast_get(emitter->arena,
				tcontrol_enclosing_function(&emitter->frontend->flow, id));
			if (owner && owner->kind == tast_rule) {
				tvmcmd_vect_append(emitter->instructions,
					tbycode_make_u(OP_RULENOT, source_logic_index(emitter, id)));
				break;
			}
			/* The existing conditional jump checks Bool and consumes the
			 * operand. Both branches replace it with its negation. */
			tvmcmd_vect_append(emitter->instructions,
				tbycode_make_u(OP_CJPFPOP, 2));
			tvmcmd_vect_append(emitter->instructions,
				tbycode_make_u(OP_PUSHB, 0));
			tvmcmd_vect_append(emitter->instructions,
				tbycode_make_u(OP_JPF, 1));
			tvmcmd_vect_append(emitter->instructions,
				tbycode_make_u(OP_PUSHB, 1));
		} else {
			tvmcmd_vect_append(emitter->instructions,
				tbycode_make(node->unary.op == tsyntax_plus ? OP_POS : OP_NEG));
		}
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
	case tast_rule:
		emit_rule(emitter, node);
		break;
	default:
		twarn(ErrCompile_Other, "AST expression",
		      "unsupported expression node");
	}
    const tast_node *expression_owner = tast_get(emitter->arena,
        tcontrol_enclosing_function(&emitter->frontend->flow, id));
    if (expression_owner && expression_owner->kind == tast_rule)
        tvmcmd_vect_append(emitter->instructions,
            tbycode_make_u(OP_RULEVALUE, source_expression_index(emitter, id)));

}
