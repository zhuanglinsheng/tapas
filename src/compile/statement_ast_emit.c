/** Migration adapter from reusable statement AST nodes to compiler actions. */
#include "ast_emit_internal.h"
#include "tapas/compile/module.h"

#include <stdlib.h>

static tstring *span_text(const tast_emitter *emitter, tsource_span span)
{
	return tsource_document_slice(emitter->document, span);
}

static void emit_statement(tast_emitter *emitter, tast_id id,
			   tstring **paths, uint_lexs npaths,
			   int cleanstk, int inblk);

typedef struct {
	tobj_ctr *bindings;
	uint_objs count;
	uint8_t *initialized;
} initialization_scope;

typedef struct {
	initialization_scope *scopes;
	uint32_t count;
} initialization_state;

static initialization_state capture_initialization(tcp *cp)
{
	uint32_t count = 1;
	for (tobj_ctr *scope = &cp->objctr; scope; scope = scope->father)
		count++;
	initialization_state state = {
		.scopes = (initialization_scope *)calloc(count, sizeof(*state.scopes)),
		.count = count
	};
	if (!state.scopes) abort();
	state.scopes[0].bindings = &cp->tmpctr;
	state.scopes[0].count = cp->tmpctr.len;
	uint32_t at = 1;
	for (tobj_ctr *scope = &cp->objctr; scope; scope = scope->father, at++) {
		state.scopes[at].bindings = scope;
		state.scopes[at].count = scope->len;
	}
	for (uint32_t i = 0; i < state.count; i++) {
		initialization_scope *scope = &state.scopes[i];
		if (!scope->count) continue;
		scope->initialized = (uint8_t *)malloc(scope->count);
		if (!scope->initialized) abort();
		for (uint_objs j = 0; j < scope->count; j++)
			scope->initialized[j] =
				scope->bindings->bindings[j].initialized;
	}
	return state;
}

static void restore_initialization(const initialization_state *state)
{
	for (uint32_t i = 0; i < state->count; i++) {
		const initialization_scope *scope = &state->scopes[i];
		uint_objs count = scope->count < scope->bindings->len ?
			scope->count : scope->bindings->len;
		for (uint_objs j = 0; j < count; j++)
			scope->bindings->bindings[j].initialized =
				scope->initialized[j];
	}
}

static void intersect_initialization(initialization_state *state,
				     const initialization_state *branch)
{
	for (uint32_t i = 0; i < state->count && i < branch->count; i++) {
		initialization_scope *target = &state->scopes[i];
		const initialization_scope *source = &branch->scopes[i];
		uint_objs count = target->count < source->count ?
			target->count : source->count;
		for (uint_objs j = 0; j < count; j++)
			target->initialized[j] =
				target->initialized[j] && source->initialized[j];
	}
}

static void free_initialization(initialization_state *state)
{
	if (!state) return;
	for (uint32_t i = 0; i < state->count; i++)
		free(state->scopes[i].initialized);
	free(state->scopes);
	*state = (initialization_state){ 0 };
}

static uint_objs field_order_index(tstring *const *order, uint_objs count,
				   const tstring *name)
{
	for (uint_objs i = 0; i < count; i++)
		if (tstring_eq(order[i], name))
			return i;
	return count;
}

static void emit_structure(tast_emitter *emitter, const tast_node *structure,
			   const ttypeval *target,
			   tstring *const *order, uint_objs order_count)
{
	if (!target || target->kind != ttype_kind_fields || !order ||
	    order_count != ttypeval_field_count(target))
		twarn(ErrCompile_Other, "structure literal",
		      "target must be a field Type with construction order");
	uint8_t *assigned = (uint8_t *)calloc(order_count, sizeof(uint8_t));
	if (!assigned)
		abort();
	const tast_id *items = tast_get_children(emitter->arena,
		structure->aggregate.children, structure->aggregate.count);
	uint_objs next_positional = 0;
	for (uint32_t i = 0; i < structure->aggregate.count; i++) {
		const tast_node *item = tast_get(emitter->arena, items[i]);
		tast_id value = items[i];
		uint_objs field = order_count;
		if (item && item->kind == tast_named_field) {
			tstring *name = span_text(emitter, item->named_field.name);
			field = field_order_index(order, order_count, name);
			tstring_free(name);
			value = item->named_field.value;
		} else {
			while (next_positional < order_count && assigned[next_positional])
				next_positional++;
			field = next_positional++;
		}
		if (field >= order_count)
			twarn(ErrCompile_Other, "structure literal",
			      "field is not declared by the target Type");
		if (assigned[field])
			twarn(ErrCompile_Other, "structure literal",
			      "field is specified more than once");
		assigned[field] = 1;
		ttypeval *field_type = ttypeval_field_named(
			target, tstring_cstr(order[field]));
		if (!field_type || !tast_expression_assignable_to(
			emitter, value, field_type))
			twarn(ErrCompile_Other, "structure literal",
			      "field value Type mismatch");
		tast_emit_expression(emitter, value);
		uint_csts key = tconsts_add_str_const(
			emitter->constants, tstring_cstr(order[field]));
		tvmcmd_vect_append(emitter->instructions,
			tbycode_make_u(OP_PUSHS, key));
		treg_ctr_add(&emitter->cp->regctr);
		tvmcmd_vect_append(emitter->instructions, tbycode_make(OP_PAIR));
		treg_ctr_ddt_n(&emitter->cp->regctr, 2);
		treg_ctr_add(&emitter->cp->regctr);
	}
	for (uint_objs i = 0; i < order_count; i++)
		if (!assigned[i])
			twarn(ErrCompile_Other, "structure literal",
			      "all target fields must be initialized");
	free(assigned);
	tvmcmd_vect_append(emitter->instructions,
		tbycode_make_u(OP_PUSHDICT, structure->aggregate.count));
	treg_ctr_ddt_n(&emitter->cp->regctr,
			  (uint_regs)structure->aggregate.count);
	treg_ctr_add(&emitter->cp->regctr);
}

void tast_emit_block(tast_emitter *emitter, const tast_node *block,
		     tstring **paths, uint_lexs npaths, int inblk)
{
	uint_objs original_temporaries =
		tobj_ctr_obj_len_all(&emitter->cp->tmpctr);
	const tast_id *statements = tast_get_children(
		emitter->arena, block->aggregate.children, block->aggregate.count);
	for (uint32_t i = 0; i < block->aggregate.count; i++) {
		const tast_node *statement = tast_get(emitter->arena, statements[i]);
		if (statement && statement->kind == tast_if_statement) {
			initialization_state entry = capture_initialization(emitter->cp);
			initialization_state merged = { 0 };
			int has_else = 0;
			uint32_t branch = i;
			for (; branch < block->aggregate.count; branch++) {
				const tast_node *current = tast_get(
					emitter->arena, statements[branch]);
				if (!current || (branch != i &&
				    current->kind != tast_elif_statement &&
				    current->kind != tast_else_statement))
					break;
				restore_initialization(&entry);
				uint_regs original_registers =
					treg_ctr_get(&emitter->cp->regctr);
				emit_statement(emitter, statements[branch], paths,
					npaths, 1, inblk);
				clean_stk(emitter->cp, emitter->instructions, 1,
					  original_registers);
				initialization_state result =
					capture_initialization(emitter->cp);
				const tast_node *branch_body = tast_get(
					emitter->arena, current->control_statement.body);
				if (!tast_block_definitely_returns(
				    emitter->arena, branch_body)) {
					if (!merged.count)
						merged = result;
					else {
						intersect_initialization(&merged, &result);
						free_initialization(&result);
					}
				} else
					free_initialization(&result);
				if (current->kind == tast_else_statement) {
					has_else = 1;
					branch++;
					break;
				}
			}
			if (!has_else) {
				if (!merged.count)
					merged = capture_initialization(emitter->cp);
				intersect_initialization(&merged, &entry);
			}
			restore_initialization(merged.count ? &merged : &entry);
			free_initialization(&merged);
			free_initialization(&entry);
			i = branch - 1;
			continue;
		}
		initialization_state loop_entry = { 0 };
		int is_loop = statement &&
			(statement->kind == tast_while_statement ||
			 statement->kind == tast_for_statement);
		if (is_loop)
			loop_entry = capture_initialization(emitter->cp);
		uint_regs original_registers = treg_ctr_get(&emitter->cp->regctr);
		emit_statement(emitter, statements[i], paths, npaths, 1, inblk);
		clean_stk(emitter->cp, emitter->instructions, 1,
			  original_registers);
		if (is_loop) {
			restore_initialization(&loop_entry);
			free_initialization(&loop_entry);
		}
	}
	if (block->kind == tast_module) {
		tcompile_module_interface_free(emitter->cp->module_interface);
		emitter->cp->module_interface = tcompile_extract_module_interface(
			emitter->cp, emitter->frontend);
	}
	uint_objs new_temporaries =
		tobj_ctr_obj_len_all(&emitter->cp->tmpctr) - original_temporaries;
	if (new_temporaries > 0) {
		tobj_ctr_obj_del_last_n(&emitter->cp->tmpctr, new_temporaries);
		tvmcmd_vect_append(
			emitter->instructions,
			tbycode_make_u(OP_TMPDEL, (uint32_t)new_temporaries));
	}
}

static void emit_if(tast_emitter *emitter, const tast_node *statement,
		    tstring **paths, uint_lexs npaths)
{
	tast_emit_expression(emitter, statement->control_statement.condition);
	tvmcmd_vect_append(emitter->instructions,
			   tbycode_make_u(OP_CJPFPOP, 0));
	uint_cmds condition = tvmcmd_vect_size32(emitter->instructions) - 1;
	treg_ctr_ddt(&emitter->cp->regctr);

	tvmcmd_vect body;
	tvmcmd_vect_init(&body);
	tvmcmd_vect *outer = emitter->instructions;
	emitter->instructions = &body;
	tast_emit_block(emitter,
		tast_get(emitter->arena, statement->control_statement.body),
		paths, npaths, 1);
	emitter->instructions = outer;
	outer->data[condition] = tbycode_make_u(
		OP_CJPFPOP, tvmcmd_vect_size32(&body) + 1);
	tvmcmd_vect_insert_vect(outer, tvmcmd_vect_size32(outer), &body);
	tvmcmd_vect_append(outer, tbycode_make(OP_PASS));
	tvmcmd_vect_free(&body);
}

static void emit_elif(tast_emitter *emitter, const tast_node *statement,
		      tstring **paths, uint_lexs npaths)
{
	if (tvmcmd_vect_size32(emitter->instructions) > 0 &&
	    tbycode_ins(tvmcmd_vect_back(emitter->instructions)) == OP_PASS)
		tvmcmd_vect_pop_back(emitter->instructions);
	tvmcmd_vect_append(emitter->instructions, tbycode_make_u(OP_JPF, 0));
	uint_cmds jump_over = tvmcmd_vect_size32(emitter->instructions) - 1;

	tvmcmd_vect condition;
	tvmcmd_vect_init(&condition);
	tvmcmd_vect *outer = emitter->instructions;
	emitter->instructions = &condition;
	tast_emit_expression(emitter, statement->control_statement.condition);
	emitter->instructions = outer;
	tvmcmd_vect_insert_vect(outer, tvmcmd_vect_size32(outer), &condition);
	tvmcmd_vect_append(outer, tbycode_make_u(OP_CJPFPOP, 0));
	uint_cmds conditional_jump = tvmcmd_vect_size32(outer) - 1;
	treg_ctr_ddt(&emitter->cp->regctr);

	tvmcmd_vect body;
	tvmcmd_vect_init(&body);
	emitter->instructions = &body;
	tast_emit_block(emitter,
		tast_get(emitter->arena, statement->control_statement.body),
		paths, npaths, 1);
	emitter->instructions = outer;
	tvmcmd_vect_insert_vect(outer, tvmcmd_vect_size32(outer), &body);
	tvmcmd_vect_append(outer, tbycode_make(OP_PASS));

	outer->data[jump_over] = tbycode_make_u(
		OP_JPF, tvmcmd_vect_size32(&condition) + 1 +
			tvmcmd_vect_size32(&body));
	outer->data[conditional_jump] = tbycode_make_u(
		OP_CJPFPOP, tvmcmd_vect_size32(&body) + 1);
	tvmcmd_vect_free(&condition);
	tvmcmd_vect_free(&body);
}

static void emit_else(tast_emitter *emitter, const tast_node *statement,
		      tstring **paths, uint_lexs npaths)
{
	if (tvmcmd_vect_size32(emitter->instructions) > 0 &&
	    tbycode_ins(tvmcmd_vect_back(emitter->instructions)) == OP_PASS)
		tvmcmd_vect_pop_back(emitter->instructions);
	tvmcmd_vect body;
	tvmcmd_vect_init(&body);
	tvmcmd_vect *outer = emitter->instructions;
	emitter->instructions = &body;
	tast_emit_block(emitter,
		tast_get(emitter->arena, statement->control_statement.body),
		paths, npaths, 1);
	emitter->instructions = outer;
	tvmcmd_vect_append(outer,
		tbycode_make_u(OP_JPF, tvmcmd_vect_size32(&body)));
	tvmcmd_vect_insert_vect(outer, tvmcmd_vect_size32(outer), &body);
	tvmcmd_vect_free(&body);
}

static void emit_while(tast_emitter *emitter, const tast_node *statement,
		       tstring **paths, uint_lexs npaths)
{
	uint_cmds loop_start = tvmcmd_vect_size32(emitter->instructions);
	tast_emit_expression(emitter, statement->control_statement.condition);
	treg_ctr_ddt(&emitter->cp->regctr);

	tvmcmd_vect body;
	tvmcmd_vect_init(&body);
	tvmcmd_vect *outer = emitter->instructions;
	emitter->instructions = &body;
	emitter->cp->in_loop++;
	tast_emit_block(emitter,
		tast_get(emitter->arena, statement->control_statement.body),
		paths, npaths, 1);
	emitter->cp->in_loop--;
	emitter->instructions = outer;

	tvmcmd_vect_append(outer, tbycode_make_u(
		OP_CJPFPOP, tvmcmd_vect_size32(&body) + 1));
	uint_cmds body_start = tvmcmd_vect_size32(outer);
	tvmcmd_vect_insert_vect(outer, tvmcmd_vect_size32(outer), &body);
	uint_cmds back = 1 + tvmcmd_vect_size32(outer) - loop_start;
	tvmcmd_vect_append(outer, tbycode_make_u(OP_JPB, back));
	tvmcmd_vect_resolve_loop_control(
		outer, body_start, body_start + tvmcmd_vect_size32(&body),
		loop_start, tvmcmd_vect_size32(outer),
		TCOMPILE_CONTINUE_MARK, TCOMPILE_BREAK_MARK);
	tvmcmd_vect_free(&body);
}

static void emit_for(tast_emitter *emitter, const tast_node *statement,
		     tstring **paths, uint_lexs npaths)
{
	uint_objs original_temporaries =
		tobj_ctr_obj_len_cur(&emitter->cp->tmpctr);
	tast_emit_expression(emitter, statement->for_statement.iterable);
	tstring *name = span_text(emitter, statement->for_statement.name);
	uint_objs location = 0;
	int is_environment = 0;
	tobj_ctr *iteration_owner = NULL;
	uint_objs iteration_owner_slot = 0;
	uint8_t iteration_was_initialized = 0;
	if (statement->for_statement.declares_binding) {
		uint_objs object_location =
			tobj_ctr_obj_loc(&emitter->cp->objctr, name);
		uint_objs temporary_location =
			tobj_ctr_obj_loc(&emitter->cp->tmpctr, name);
		if (temporary_location <
		    tobj_ctr_obj_len_cur(&emitter->cp->tmpctr))
			twarn(ErrCompile_DblVDeclare, "AST for", tstring_cstr(name));
		if (object_location < tobj_ctr_obj_len_cur(&emitter->cp->objctr) &&
		    !tobj_ctr_is_preload(&emitter->cp->objctr, object_location))
			twarn(ErrCompile_DblVDeclare, "AST for", tstring_cstr(name));
		uint_csts name_location = UNDEF_NAMELOC;
		location = tobj_ctr_obj_create(
			&emitter->cp->tmpctr, name, 0,
			emitter->constants, &name_location);
		emitter->cp->tmpctr.bindings[location].initialized = 1;
		tvmcmd_vect_append(
			emitter->instructions,
			tbycode_make_lr(OP_VCRT, (uint16_t)name_location, 0));
	} else {
		tobj_ctr *owner = NULL;
		uint_objs owner_slot = 0;
		uint_objs temporary_location =
			tobj_ctr_obj_loc(&emitter->cp->tmpctr, name);
		uint_objs object_location =
			tobj_ctr_obj_loc(&emitter->cp->objctr, name);
		if (temporary_location !=
		    tobj_ctr_obj_len_all(&emitter->cp->tmpctr))
			location = temporary_location;
		else if (object_location !=
			 tobj_ctr_obj_len_all(&emitter->cp->objctr)) {
			location = object_location;
			is_environment = 1;
		} else
			twarn(ErrCompile_ObjUnfound, "AST for", tstring_cstr(name));
		if (tcompile_find_binding(
			&emitter->cp->tmpctr, name, &owner, &owner_slot) ||
		    tcompile_find_binding(
			&emitter->cp->objctr, name, &owner, &owner_slot)) {
			iteration_owner = owner;
			iteration_owner_slot = owner_slot;
			iteration_was_initialized =
				owner->bindings[owner_slot].initialized;
			/* OP_LOOPAS initializes the target before every body entry. */
			owner->bindings[owner_slot].initialized = 1;
		}
	}
	tstring_free(name);

	uint_cmds loop_start = tvmcmd_vect_size32(emitter->instructions);
	tvmcmd_vect_append(
		emitter->instructions,
		tbycode_make_lr(OP_LOOPAS, (uint16_t)location,
				(uint16_t)is_environment));
	treg_ctr_add(&emitter->cp->regctr);
	treg_ctr_ddt(&emitter->cp->regctr);

	tvmcmd_vect body;
	tvmcmd_vect_init(&body);
	tvmcmd_vect *outer = emitter->instructions;
	emitter->instructions = &body;
	emitter->cp->in_loop++;
	tast_emit_block(emitter,
		tast_get(emitter->arena, statement->for_statement.body),
		paths, npaths, 1);
	emitter->cp->in_loop--;
	emitter->instructions = outer;

	tvmcmd_vect_append(outer, tbycode_make_u(
		OP_CJPFPOP, 1 + tvmcmd_vect_size32(&body)));
	uint_cmds body_start = tvmcmd_vect_size32(outer);
	tvmcmd_vect_insert_vect(outer, tvmcmd_vect_size32(outer), &body);
	tvmcmd_vect_append(outer,
		tbycode_make_u(OP_JPB, 3 + tvmcmd_vect_size32(&body)));
	tvmcmd_vect_resolve_loop_control(
		outer, body_start, body_start + tvmcmd_vect_size32(&body),
		loop_start, tvmcmd_vect_size32(outer),
		TCOMPILE_CONTINUE_MARK, TCOMPILE_BREAK_MARK);
	tvmcmd_vect_append(outer, tbycode_make_lr(OP_POPN, 1, 0));
	treg_ctr_ddt(&emitter->cp->regctr);

	uint_objs new_temporaries =
		tobj_ctr_obj_len_cur(&emitter->cp->tmpctr) - original_temporaries;
	if (new_temporaries > 0) {
		tobj_ctr_obj_del_last_n(&emitter->cp->tmpctr, new_temporaries);
		tvmcmd_vect_append(
			outer, tbycode_make_u(OP_TMPDEL, (uint32_t)new_temporaries));
	}
	if (iteration_owner && iteration_owner_slot < iteration_owner->len)
		iteration_owner->bindings[iteration_owner_slot].initialized =
			iteration_was_initialized;
	tvmcmd_vect_free(&body);
}

static void emit_declaration(tast_emitter *emitter,
			     const tast_node *declaration,
			     tstring **paths, uint_lexs npaths, int inblk)
{
	(void)paths;
	(void)npaths;
	int is_mutable = declaration->declaration_statement.is_mutable;
	tstring *name = span_text(emitter,
		declaration->declaration_statement.name);
	ttypeval *annotation = declaration->declaration_statement.has_annotation ?
		tast_resolve_annotation(
			emitter, declaration->declaration_statement.annotation) : NULL;
	int has_initializer = declaration->declaration_statement.has_initializer;
	ttypeval *static_value = NULL;
	ttypeval *inferred = has_initializer ? tast_infer_expression_type(
		emitter, declaration->declaration_statement.initializer,
		&static_value) : NULL;
	if (has_initializer && annotation &&
	    !tast_expression_assignable_to(
		emitter, declaration->declaration_statement.initializer,
		annotation))
		twarn(ErrCompile_Other, is_mutable ? "var" : "let",
		      "initializer Type mismatch");
	if (is_mutable) {
		ttypeval_release(static_value);
		static_value = NULL;
	}
	if (!has_initializer && !is_mutable && annotation &&
	    ttypeval_equal(annotation,
		ttypeval_builtin(ttype_builtin_type)))
		static_value = ttypeval_new_recursive();
	tstring **field_order = NULL;
	uint_objs field_order_count = 0;
	if (has_initializer &&
	    (static_value || (inferred && inferred->kind == ttype_kind_fields)))
		tast_static_type_field_order(
			emitter, declaration->declaration_statement.initializer,
			&field_order, &field_order_count);
	else if (annotation)
		tast_annotation_field_order(
			emitter, declaration->declaration_statement.annotation,
			&field_order, &field_order_count);

	tobj_ctr *bindings = is_mutable ?
		&emitter->cp->objctr : &emitter->cp->tmpctr;
	uint_objs other_location = tobj_ctr_obj_loc(
		is_mutable ? &emitter->cp->tmpctr : &emitter->cp->objctr, name);
	if (other_location < tobj_ctr_obj_len_cur(
		is_mutable ? &emitter->cp->tmpctr : &emitter->cp->objctr) &&
	    (is_mutable || !tobj_ctr_is_preload(
		&emitter->cp->objctr, other_location)))
		twarn(ErrCompile_DblVDeclare, "declaration", tstring_cstr(name));
	uint_csts name_location = UNDEF_NAMELOC;
	uint_objs location = tobj_ctr_obj_create(
		bindings, name, is_mutable ? inblk : 0,
		emitter->constants, &name_location);
	tcompile_set_metadata(bindings, location,
		annotation ? annotation : inferred,
		static_value, annotation != NULL);
	bindings->bindings[location].initialized = has_initializer;
	bindings->bindings[location].is_types_package = has_initializer &&
		tast_is_types_package_expression(
			emitter, declaration->declaration_statement.initializer);
	tcompile_set_field_order(bindings, location, field_order,
				 field_order_count);
	tvmcmd_vect_append(emitter->instructions,
		tbycode_make_lr(OP_VCRT, (uint16_t)name_location,
				(uint16_t)is_mutable));
	if (!has_initializer) {
		if (static_value) {
			tvmcmd_vect_append(emitter->instructions,
				tbycode_make(OP_TYPEFWD));
			treg_ctr_add(&emitter->cp->regctr);
			tvmcmd_vect_append(emitter->instructions,
				tbycode_make_lr(OP_POPCOV, (uint16_t)location,
					(uint16_t)is_mutable));
			treg_ctr_ddt(&emitter->cp->regctr);
		}
		tstring_free(name);
		ttypeval_release(annotation);
		ttypeval_release(inferred);
		ttypeval_release(static_value);
		tast_free_field_order(field_order, field_order_count);
		return;
	}
	const tast_node *initializer = tast_get(emitter->arena,
		declaration->declaration_statement.initializer);
	if (initializer->kind == tast_structure)
		emit_structure(emitter, initializer, annotation,
			       field_order, field_order_count);
	else {
		const ttypeval *saved_pending = emitter->pending_function_type;
		if (initializer->kind == tast_function && inferred &&
		    inferred->kind == ttype_kind_function)
			emitter->pending_function_type = inferred;
		tast_emit_expression(
			emitter, declaration->declaration_statement.initializer);
		emitter->pending_function_type = saved_pending;
	}
	tvmcmd_vect_append(emitter->instructions,
		tbycode_make_lr(OP_POPCOV, (uint16_t)location,
				(uint16_t)is_mutable));
	treg_ctr_ddt(&emitter->cp->regctr);

	tstring_free(name);
	ttypeval_release(annotation);
	ttypeval_release(inferred);
	ttypeval_release(static_value);
	tast_free_field_order(field_order, field_order_count);
}

static void emit_return(tast_emitter *emitter, const tast_node *statement,
			tstring **paths, uint_lexs npaths, int inblk)
{
	(void)paths;
	(void)npaths;
	(void)inblk;
	tast_id value_id = statement->return_statement.value;
	if (emitter->expected_return_type) {
		if (value_id == TAST_INVALID_ID) {
			if (!compile_type_assignable(
				ttypeval_builtin(ttype_builtin_nil),
				emitter->expected_return_type))
				twarn(ErrCompile_Other, "return",
				      "result Type mismatch");
		} else if (!tast_expression_assignable_to(
			emitter, value_id, emitter->expected_return_type)) {
			twarn(ErrCompile_Other, "return",
			      "result Type mismatch");
		}
	}
	if (value_id != TAST_INVALID_ID) {
		const tast_node *value = tast_get(emitter->arena, value_id);
		if (value->kind == tast_name) {
			tstring *name = span_text(emitter, value->span);
			if (tobj_ctr_obj_loc(&emitter->cp->tmpctr, name) !=
			    tobj_ctr_obj_len_all(&emitter->cp->tmpctr))
				twarn(ErrCompile_ReturnTmpObj, "return", "");
			tstring_free(name);
		}
		tast_emit_expression(emitter, value_id);
	}
	tvmcmd_vect_append(emitter->instructions, tbycode_make(OP_RET));
	treg_ctr_ddt_n(&emitter->cp->regctr,
			  treg_ctr_get(&emitter->cp->regctr));
}

static void emit_import(tast_emitter *emitter, const tast_node *statement,
			tstring **paths, uint_lexs npaths, int inblk)
{
	tstring *path = span_text(emitter, statement->import_statement.path);
	tstring *alias = statement->import_statement.has_alias ?
		span_text(emitter, statement->import_statement.alias) : NULL;
	tcompile_emit_import(emitter->cp, path, alias, emitter->instructions,
			     emitter->constants, paths, npaths, inblk);
	tstring_free(alias);
	tstring_free(path);
}

static void emit_assignment(tast_emitter *emitter, const tast_node *statement,
			    tstring **paths, uint_lexs npaths,
			    int cleanstk, int inblk)
{
	(void)paths;
	(void)npaths;
	(void)cleanstk;
	const tast_node *target = tast_get(
		emitter->arena, statement->assignment_statement.target);
	if (!target)
		twarn(ErrCompile_Other, "assignment", "missing assignment target");
	if (target->kind == tast_name) {
		tstring *name = span_text(emitter, target->span);
		uint_objs temporary_location =
			tobj_ctr_obj_loc(&emitter->cp->tmpctr, name);
		uint_objs location = temporary_location;
		int is_environment = 0;
		tobj_ctr *owner = NULL;
		uint_objs owner_slot = 0;
		if (temporary_location !=
		    tobj_ctr_obj_len_all(&emitter->cp->tmpctr)) {
			tcompile_find_binding(
				&emitter->cp->tmpctr, name, &owner, &owner_slot);
		} else {
			location = tobj_ctr_obj_loc(&emitter->cp->objctr, name);
			is_environment = 1;
			if (location == tobj_ctr_obj_len_all(&emitter->cp->objctr))
				twarn(ErrCompile_ObjUnfound, "assignment",
				      tstring_cstr(name));
			if (tobj_ctr_is_preload(&emitter->cp->objctr, location))
				twarn(ErrCompile_AsgDefault, "assignment",
				      tstring_cstr(name));
			tcompile_find_binding(
				&emitter->cp->objctr, name, &owner, &owner_slot);
		}
		const tast_node *value = tast_get(
			emitter->arena, statement->assignment_statement.value);
		ttypeval *assigned_static = NULL;
		ttypeval *actual = value->kind == tast_structure ? NULL :
			tast_infer_expression_type(
				emitter, statement->assignment_statement.value,
				&assigned_static);
		int defines_recursive = owner && !owner->bindings[owner_slot].initialized &&
			ttypeval_is_recursive(owner->bindings[owner_slot].type_value);
		if (defines_recursive && inblk)
			twarn(ErrCompile_Other, "recursive Type",
			      "definition must be unconditional in its declaring block");
		if (defines_recursive && (!assigned_static ||
		    !ttypeval_define_recursive(
			owner->bindings[owner_slot].type_value, assigned_static)))
			twarn(ErrCompile_Other, "recursive Type",
			      "initial value must be a static Type definition");
		if (owner && owner->bindings[owner_slot].has_annotation &&
		    !tast_expression_assignable_to(
			emitter, statement->assignment_statement.value,
			owner->bindings[owner_slot].value_type))
			twarn(ErrCompile_Other, "assignment",
			      "assigned Type mismatch");
		if (owner && !owner->bindings[owner_slot].has_annotation) {
			tcompile_set_metadata(
				owner, owner_slot, actual, NULL, 0);
			tstring **new_order = NULL;
			uint_objs new_order_count = 0;
			if (actual && actual->kind == ttype_kind_fields)
				tast_static_type_field_order(
					emitter, statement->assignment_statement.value,
					&new_order, &new_order_count);
			tcompile_set_field_order(owner, owner_slot,
				new_order, new_order_count);
			tast_free_field_order(new_order, new_order_count);
			owner->bindings[owner_slot].is_types_package =
				tast_is_types_package_expression(
					emitter, statement->assignment_statement.value);
		} else if (owner && !defines_recursive &&
			   owner->bindings[owner_slot].value_type &&
			   ttypeval_equal(owner->bindings[owner_slot].value_type,
				ttypeval_builtin(ttype_builtin_type))) {
			tcompile_set_metadata(owner, owner_slot,
				owner->bindings[owner_slot].value_type,
				inblk ? NULL : assigned_static, 1);
		}
		if (owner)
			owner->bindings[owner_slot].initialized = 1;
		ttypeval_release(actual);
		ttypeval_release(assigned_static);
		if (value->kind == tast_structure) {
			if (!owner)
				twarn(ErrCompile_ObjUnfound, "assignment",
				      tstring_cstr(name));
			emit_structure(emitter, value,
				owner->bindings[owner_slot].value_type,
				owner->bindings[owner_slot].field_order,
				owner->bindings[owner_slot].field_order_count);
		} else {
			uint8_t saved_pending = emitter->allow_pending_type_references;
			if (defines_recursive)
				emitter->allow_pending_type_references = 1;
			tast_emit_expression(
				emitter, statement->assignment_statement.value);
			emitter->allow_pending_type_references = saved_pending;
		}
		tvmcmd_vect_append(emitter->instructions,
			defines_recursive ?
				tbycode_make_lr(OP_TYPEDEFINE, (uint16_t)location,
					(uint16_t)is_environment) :
				tbycode_make_lr(OP_POPCOV, (uint16_t)location,
					(uint16_t)is_environment));
		treg_ctr_ddt(&emitter->cp->regctr);
		tstring_free(name);
		return;
	}
	if (target->kind != tast_index || target->aggregate.count == 0)
		twarn(ErrCompile_Other, "assignment",
		      "target must be a name or indexed name");

	const tast_node *receiver = tast_get(emitter->arena,
					    target->aggregate.receiver);
	if (!receiver || receiver->kind != tast_name)
		twarn(ErrCompile_Other, "assignment",
		      "indexed target receiver must be a name");
	const tast_id *target_indices = tast_get_children(emitter->arena,
		target->aggregate.children, target->aggregate.count);
	for (uint32_t i = 0; i < target->aggregate.count; i++) {
		const tast_node *index = tast_get(emitter->arena, target_indices[i]);
		if (!index || index->kind == tast_slice)
			twarn(ErrCompile_Other, "assignment",
			      "slice assignment is not supported");
	}
	tstring *name = span_text(emitter, receiver->span);
	tobj_ctr *owner = NULL;
	uint_objs owner_slot = 0;
	int binding_found = tcompile_find_binding(
		&emitter->cp->tmpctr, name, &owner, &owner_slot);
	if (!binding_found)
		binding_found = tcompile_find_binding(
			&emitter->cp->objctr, name, &owner, &owner_slot);
	uint_objs location = tobj_ctr_obj_loc(&emitter->cp->tmpctr, name);
	int is_environment = 0;
	if (location == tobj_ctr_obj_len_all(&emitter->cp->tmpctr)) {
		location = tobj_ctr_obj_loc(&emitter->cp->objctr, name);
		is_environment = 1;
	}
	if ((!is_environment && location ==
	     tobj_ctr_obj_len_all(&emitter->cp->tmpctr)) ||
	    (is_environment && location ==
	     tobj_ctr_obj_len_all(&emitter->cp->objctr)))
		twarn(ErrCompile_ObjUnfound, "index assignment",
		      tstring_cstr(name));
	if (binding_found && owner->bindings[owner_slot].type_value)
		twarn(ErrCompile_Other, "index assignment",
		      "Type values are immutable");
	const tast_id *indices = tast_get_children(
		emitter->arena, target->aggregate.children, target->aggregate.count);
	if (binding_found && owner->bindings[owner_slot].value_type &&
	    target->aggregate.count == 1) {
		ttypeval *container = owner->bindings[owner_slot].value_type;
		ttypeval *expected = NULL;
		if (container->kind == ttype_kind_fields) {
			const tast_node *key = tast_get(emitter->arena, indices[0]);
			if (!key || key->kind != tast_string)
				twarn(ErrCompile_Other, "field assignment",
				      "field Type requires a String literal key");
			tsource_span contents = key->span;
			contents.start++;
			contents.end--;
			tstring *field_name = span_text(emitter, contents);
			expected = ttypeval_field_named(
				container, tstring_cstr(field_name));
			if (!expected)
				twarn(ErrCompile_Other, "field assignment",
				      "field is not declared by the target Type");
			tstring_free(field_name);
		} else if (container->kind == ttype_kind_list)
			expected = ttypeval_parameter(container, "item");
		else if (container->kind == ttype_kind_dictionary)
			expected = ttypeval_parameter(container, "value");
		if (expected && !tast_expression_assignable_to(
			emitter, statement->assignment_statement.value, expected))
			twarn(ErrCompile_Other, "index assignment",
			      "assigned Type mismatch");
	}
	tstring_free(name);
	tast_emit_expression(emitter, statement->assignment_statement.value);
	for (uint32_t i = 0; i < target->aggregate.count; i++)
		tast_emit_expression(emitter, indices[i]);
	tvmcmd_vect_append(emitter->instructions,
		tbycode_make_lbi(OP_IDXL, (uint16_t)location,
				 (uint8_t)target->aggregate.count,
				 (uint8_t)is_environment));
	treg_ctr_ddt_n(&emitter->cp->regctr,
			  (uint_regs)target->aggregate.count + 1);
}

static void emit_statement(tast_emitter *emitter, tast_id id,
			   tstring **paths, uint_lexs npaths,
			   int cleanstk, int inblk)
{
	const tast_node *statement = tast_get(emitter->arena, id);
	if (statement->kind != tast_declaration_group &&
	    statement->kind != tast_module && statement->kind != tast_block)
		tast_validate_node(emitter, id);
	switch (statement->kind) {
	case tast_expression_statement:
		tast_emit_expression(emitter,
			statement->expression_statement.value);
		break;
	case tast_declaration_statement:
		emit_declaration(emitter, statement, paths, npaths, inblk);
		break;
	case tast_declaration_group: {
		const tast_id *children = tast_get_children(
			emitter->arena, statement->aggregate.children,
			statement->aggregate.count);
		for (uint32_t i = 0; i < statement->aggregate.count; i++)
			emit_statement(
				emitter, children[i], paths, npaths, cleanstk, inblk);
	} break;
	case tast_assignment_statement:
		emit_assignment(emitter, statement, paths, npaths, cleanstk, inblk);
		break;
	case tast_return_statement:
		emit_return(emitter, statement, paths, npaths, inblk);
		break;
	case tast_import_statement:
		emit_import(emitter, statement, paths, npaths, inblk);
		break;
	case tast_if_statement:
		emit_if(emitter, statement, paths, npaths);
		break;
	case tast_elif_statement:
		emit_elif(emitter, statement, paths, npaths);
		break;
	case tast_else_statement:
		emit_else(emitter, statement, paths, npaths);
		break;
	case tast_while_statement:
		emit_while(emitter, statement, paths, npaths);
		break;
	case tast_for_statement:
		emit_for(emitter, statement, paths, npaths);
		break;
	case tast_break_statement:
		if (emitter->cp->in_loop == 0)
			twarn(ErrCompile_Other, "AST break", "break outside loop");
		tvmcmd_vect_append(
			emitter->instructions,
			tbycode_make(TCOMPILE_BREAK_MARK));
		break;
	case tast_continue_statement:
		if (emitter->cp->in_loop == 0)
			twarn(ErrCompile_Other, "AST continue", "continue outside loop");
		tvmcmd_vect_append(
			emitter->instructions,
			tbycode_make(TCOMPILE_CONTINUE_MARK));
		break;
	default:
		twarn(ErrCompile_Other, "AST statement",
		      "unsupported statement node");
	}
}

static void compile_ast_source(
	tcp *cp, const tstring *source, tfrontend_mode mode,
	tvmcmd_vect *tcmds, tconsts *consts,
	tstring **paths, uint_lexs npaths, int cleanstk, int inblk)
{
	tfrontend frontend;
	const char *name = mode == tfrontend_statement ?
		"<statement>" : "<module>";
	tcompile_frontend_init(
		cp, &frontend, name, tstring_cstr(source), mode);
	if (!tfrontend_valid(&frontend)) {
		const char *message = frontend.diagnostics.count ?
			tstring_cstr(frontend.diagnostics.items[0].message) :
			"invalid source";
		twarn(ErrCompile_Other, "AST frontend", message);
	}
	tast_emitter emitter = {
		.cp = cp,
		.document = &frontend.document,
		.arena = &frontend.arena,
		.frontend = &frontend,
		.instructions = tcmds,
		.constants = consts,
		.paths = paths,
		.npaths = npaths
	};
	if (mode == tfrontend_statement)
		emit_statement(&emitter, frontend.root,
			paths, npaths, cleanstk, inblk);
	else
		tast_emit_block(&emitter,
			tast_get(&frontend.arena, frontend.root),
			paths, npaths, inblk);
	tfrontend_free(&frontend);
}

void tcompile_ast_statement(tcp *cp, const tstring *source,
			       tvmcmd_vect *tcmds, tconsts *consts,
			       tstring **paths, uint_lexs npaths,
			       int cleanstk, int inblk)
{
	compile_ast_source(cp, source, tfrontend_statement,
		tcmds, consts, paths, npaths, cleanstk, inblk);
}

void tcompile_ast_module(tcp *cp, const tstring *source,
			    tvmcmd_vect *tcmds, tconsts *consts,
			    tstring **paths, uint_lexs npaths,
			    int inblk)
{
	compile_ast_source(cp, source, tfrontend_module,
		tcmds, consts, paths, npaths, 1, inblk);
}

tcompile_module_interface *tcompile_extract_module_interface(
		tcp *cp, const tfrontend *frontend)
{
	tcompile_module_interface *interface =
		(tcompile_module_interface *)calloc(1, sizeof(*interface));
	if (!interface)
		abort();
	tmodule_interface surface;
	tmodule_interface_init(&surface, "<module-interface>");
	if (frontend && tfrontend_valid(frontend))
		tmodule_interface_extract(frontend, "<module-interface>", 0, &surface);
	for (uint32_t i = 0; i < surface.export_count; i++) {
		const tmodule_export *public_export = &surface.exports[i];
		if (public_export->local_symbol >= frontend->semantic.symbol_count)
			continue;
		const tsemantic_symbol *symbol =
			&frontend->semantic.symbols[public_export->local_symbol];
		tobj_ctr *owner = NULL;
		uint_objs slot = 0;
		int found = tcompile_find_binding(&cp->tmpctr, symbol->name,
			&owner, &slot);
		if (!found)
			found = tcompile_find_binding(&cp->objctr, symbol->name,
				&owner, &slot);
		if (!found || !owner->bindings[slot].type_value)
			continue;
		interface->exports = (tcompile_export *)realloc(interface->exports,
			(interface->count + 1) * sizeof(tcompile_export));
		if (!interface->exports) abort();
		tcompile_export *exported = &interface->exports[interface->count++];
		*exported = (tcompile_export){
			.name = tstring_dup(public_export->name),
			.type = owner->bindings[slot].type_value,
			.field_order_count = owner->bindings[slot].field_order_count
		};
		ttypeval_retain(exported->type);
		if (exported->field_order_count) {
			exported->field_order = (tstring **)calloc(
				exported->field_order_count, sizeof(tstring *));
			if (!exported->field_order) abort();
			for (uint_objs j = 0; j < exported->field_order_count; j++)
				exported->field_order[j] = tstring_dup(
					owner->bindings[slot].field_order[j]);
		}
	}
	tmodule_interface_free(&surface);
	return interface;
}
