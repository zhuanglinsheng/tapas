/** Migration adapter from reusable statement AST nodes to compiler actions. */
#include "ast_emit_internal.h"
#include "tapas/compile/module.h"

#include <stdlib.h>

static tstring *span_text(const tast_emitter *emitter, tsource_span span)
{
	return tsource_document_slice(emitter->document, span);
}

static int assignment_target_supported(const tast_arena *arena, tast_id id)
{
	const tast_node *target = tast_get(arena, id);
	if (!target)
		return 0;
	if (target->kind == tast_name)
		return 1;
	if (target->kind != tast_index || target->aggregate.count == 0)
		return 0;
	const tast_node *receiver = tast_get(arena, target->aggregate.receiver);
	if (!receiver || receiver->kind != tast_name)
		return 0;
	const tast_id *children = tast_get_children(
		arena, target->aggregate.children, target->aggregate.count);
	for (uint32_t i = 0; i < target->aggregate.count; i++)
		if (!tast_expression_supported(arena, children[i]) ||
		    tast_get(arena, children[i])->kind == tast_slice)
			return 0;
	return 1;
}

int tast_statement_supported(const tast_arena *arena, tast_id id)
{
	const tast_node *statement = tast_get(arena, id);
	if (!statement)
		return 0;
	switch (statement->kind) {
	case tast_expression_statement:
		return tast_expression_supported(
			arena, statement->expression_statement.value);
	case tast_declaration_statement:
		return tast_expression_supported(
			arena, statement->declaration_statement.initializer) ||
		       (statement->declaration_statement.has_annotation &&
			tast_get(arena,
			 statement->declaration_statement.initializer)->kind ==
			 tast_structure);
	case tast_declaration_group: {
		const tast_id *children = tast_get_children(
			arena, statement->aggregate.children,
			statement->aggregate.count);
		for (uint32_t i = 0; i < statement->aggregate.count; i++)
			if (!tast_statement_supported(arena, children[i]))
				return 0;
		return 1;
	}
	case tast_assignment_statement:
		return assignment_target_supported(
			arena, statement->assignment_statement.target) &&
		       (tast_expression_supported(
			arena, statement->assignment_statement.value) ||
			tast_get(arena,
			 statement->assignment_statement.value)->kind == tast_structure);
	case tast_return_statement:
		return statement->return_statement.value == TAST_INVALID_ID ||
		       tast_expression_supported(
			arena, statement->return_statement.value);
	case tast_import_statement:
		return 1;
	case tast_block:
	case tast_module: {
		const tast_id *children = tast_get_children(
			arena, statement->aggregate.children,
			statement->aggregate.count);
		for (uint32_t i = 0; i < statement->aggregate.count; i++)
			if (!tast_statement_supported(arena, children[i]))
				return 0;
		return 1;
	}
	case tast_if_statement:
	case tast_elif_statement:
	case tast_while_statement:
		return tast_expression_supported(
			arena, statement->control_statement.condition) &&
		       tast_statement_supported(
			arena, statement->control_statement.body);
	case tast_else_statement:
		return tast_statement_supported(
			arena, statement->control_statement.body);
	case tast_for_statement:
		return tast_expression_supported(
			arena, statement->for_statement.iterable) &&
		       tast_statement_supported(arena, statement->for_statement.body);
	case tast_break_statement:
	case tast_continue_statement:
		return 1;
	default:
		return 0;
	}
}

static void emit_statement(tast_emitter *emitter, tast_id id,
			   tstring **paths, uint_lexs npaths,
			   int cleanstk, int inblk);

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
		uint_regs original_registers = treg_ctr_get(&emitter->cp->regctr);
		emit_statement(emitter, statements[i], paths, npaths, 1, inblk);
		clean_stk(emitter->cp, emitter->instructions, 1,
			  original_registers);
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
	tvmcmd_vect_insert_vect(outer, tvmcmd_vect_size32(outer), &body);
	uint_cmds back = 1 + tvmcmd_vect_size32(outer) - loop_start;
	tvmcmd_vect_append(outer, tbycode_make_u(OP_JPB, back));
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
		tvmcmd_vect_append(
			emitter->instructions,
			tbycode_make_lr(OP_VCRT, (uint16_t)name_location, 0));
	} else {
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
	}
	tstring_free(name);

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
	tvmcmd_vect_insert_vect(outer, tvmcmd_vect_size32(outer), &body);
	tvmcmd_vect_append(outer,
		tbycode_make_u(OP_JPB, 3 + tvmcmd_vect_size32(&body)));
	tvmcmd_vect_append(outer, tbycode_make_lr(OP_POPN, 1, 0));
	treg_ctr_ddt(&emitter->cp->regctr);

	uint_objs new_temporaries =
		tobj_ctr_obj_len_cur(&emitter->cp->tmpctr) - original_temporaries;
	if (new_temporaries > 0) {
		tobj_ctr_obj_del_last_n(&emitter->cp->tmpctr, new_temporaries);
		tvmcmd_vect_append(
			outer, tbycode_make_u(OP_TMPDEL, (uint32_t)new_temporaries));
	}
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
	ttypeval *static_value = NULL;
	ttypeval *inferred = tast_infer_expression_type(
		emitter, declaration->declaration_statement.initializer,
		&static_value);
	if (annotation &&
	    !tast_expression_assignable_to(
		emitter, declaration->declaration_statement.initializer,
		annotation))
		twarn(ErrCompile_Other, is_mutable ? "var" : "let",
		      "initializer Type mismatch");
	if (is_mutable) {
		ttypeval_release(static_value);
		static_value = NULL;
	}
	tstring **field_order = NULL;
	uint_objs field_order_count = 0;
	if (static_value || (inferred && inferred->kind == ttype_kind_fields))
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
	bindings->bindings[location].is_types_package =
		tast_is_types_package_expression(
			emitter, declaration->declaration_statement.initializer);
	tcompile_set_field_order(bindings, location, field_order,
				 field_order_count);
	tvmcmd_vect_append(emitter->instructions,
		tbycode_make_lr(OP_VCRT, (uint16_t)name_location,
				(uint16_t)is_mutable));
	const tast_node *initializer = tast_get(emitter->arena,
		declaration->declaration_statement.initializer);
	if (initializer->kind == tast_structure)
		emit_structure(emitter, initializer, annotation,
			       field_order, field_order_count);
	else
		tast_emit_expression(
			emitter, declaration->declaration_statement.initializer);
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
	ttoken token = {
		.type = token_import,
		.nvals = statement->import_statement.has_alias ? 2 : 1,
		.val1 = span_text(emitter, statement->import_statement.path),
		.val2 = statement->import_statement.has_alias ?
			span_text(emitter, statement->import_statement.alias) :
			tstring_new_empty(),
		.val3 = tstring_new_empty()
	};
	parse_import(emitter->cp, &token, emitter->instructions,
		     emitter->constants, paths, npaths, inblk);
	ttoken_free(&token);
}

static void emit_assignment(tast_emitter *emitter, const tast_node *statement,
			    tstring **paths, uint_lexs npaths,
			    int cleanstk, int inblk)
{
	(void)paths;
	(void)npaths;
	(void)cleanstk;
	(void)inblk;
	const tast_node *target = tast_get(
		emitter->arena, statement->assignment_statement.target);
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
		ttypeval *actual = value->kind == tast_structure ? NULL :
			tast_infer_expression_type(
				emitter, statement->assignment_statement.value, NULL);
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
		}
		ttypeval_release(actual);
		if (value->kind == tast_structure) {
			if (!owner)
				twarn(ErrCompile_ObjUnfound, "assignment",
				      tstring_cstr(name));
			emit_structure(emitter, value,
				owner->bindings[owner_slot].value_type,
				owner->bindings[owner_slot].field_order,
				owner->bindings[owner_slot].field_order_count);
		} else
			tast_emit_expression(
				emitter, statement->assignment_statement.value);
		tvmcmd_vect_append(emitter->instructions,
			tbycode_make_lr(OP_POPCOV, (uint16_t)location,
					(uint16_t)is_environment));
		treg_ctr_ddt(&emitter->cp->regctr);
		tstring_free(name);
		return;
	}

	const tast_node *receiver = tast_get(emitter->arena,
					    target->aggregate.receiver);
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
		tvmcmd_vect_append(emitter->instructions, tbycode_make(OP_BREAK));
		break;
	case tast_continue_statement:
		if (emitter->cp->in_loop == 0)
			twarn(ErrCompile_Other, "AST continue", "continue outside loop");
		tvmcmd_vect_append(emitter->instructions, tbycode_make(OP_CONTI));
		break;
	default:
		break;
	}
}

int tcompile_try_ast_statement(tcp *cp, const tstring *source,
			       tvmcmd_vect *tcmds, tconsts *consts,
			       tstring **paths, uint_lexs npaths,
			       int cleanstk, int inblk)
{
	tfrontend frontend;
	tfrontend_init(&frontend, "<statement>", tstring_cstr(source),
		tfrontend_statement);
	int supported = tfrontend_valid(&frontend) &&
		tast_statement_supported(&frontend.arena, frontend.root);
	if (supported) {
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
		emit_statement(&emitter, frontend.root,
			paths, npaths, cleanstk, inblk);
	}
	tfrontend_free(&frontend);
	return supported;
}

int tcompile_try_ast_module(tcp *cp, const tstring *source,
			    tvmcmd_vect *tcmds, tconsts *consts,
			    tstring **paths, uint_lexs npaths,
			    int inblk)
{
	tfrontend frontend;
	tfrontend_init(&frontend, "<module>", tstring_cstr(source),
		tfrontend_module);
	if (!tfrontend_valid(&frontend)) {
		const char *message = frontend.diagnostics.count ?
			tstring_cstr(frontend.diagnostics.items[0].message) :
			"invalid source module";
		twarn(ErrCompile_Other, "AST frontend", message);
	}
	int supported = tast_statement_supported(
		&frontend.arena, frontend.root);
	if (!supported)
		twarn(ErrCompile_Other, "AST frontend",
		      "source form is not supported by the production AST compiler");
	if (supported) {
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
		tast_emit_block(&emitter,
			tast_get(&frontend.arena, frontend.root),
			paths, npaths, inblk);
	}
	tfrontend_free(&frontend);
	return 1;
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
