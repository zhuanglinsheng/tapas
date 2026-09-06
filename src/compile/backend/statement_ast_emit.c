/** Migration adapter from reusable statement AST nodes to compiler actions. */
#include "ast_emit_internal.h"
#include "tapas/dsa/tstring.h"
#include "compile/frontend/module.h"
#include "tapas/objects/trule_ir.h"

#include <stdlib.h>

static void emit_statement(tast_emitter *emitter, tast_id id,
			   int cleanstk, int inblk);

static void sync_initialization(tast_emitter *emitter)
{
	const tsemantic_model *semantic = &emitter->frontend->semantic;
	for (uint32_t i = 0; i < semantic->symbol_count; i++) {
		const tsemantic_symbol *symbol = &semantic->symbols[i];
		if (symbol->scope != 0) continue;
		tobj_ctr *owner = nullptr;
		uint_objs slot = 0;
		if (tcompile_find_binding(
		    &emitter->cp->tmpctr, symbol->name, &owner, &slot) ||
		    tcompile_find_binding(
		    &emitter->cp->objctr, symbol->name, &owner, &slot))
			owner->bindings[slot].initialized =
				tcontrol_symbol_definitely_assigned(
					&emitter->frontend->flow, i);
	}
}

static void emit_rule_condition(tast_emitter *emitter,
				const tast_node *condition)
{
	tstring *description;
	if (condition->rule_condition.has_description) {
		tast_node literal = {
			.kind = tast_string,
			.span = condition->rule_condition.description
		};
		description = tast_string_value(emitter->document, &literal);
	} else
		description = tast_emitter_text(emitter, condition->span);
	uint_csts description_id = tconsts_add_str_const(
		emitter->constants, tstring_cstr(description));
	tstring_free(description);
	const tast_node *value = tast_get(
		emitter->arena, condition->rule_condition.value);
	if (value && value->kind == tast_block) {
		const tast_id *items = tast_get_children(emitter->arena,
			value->aggregate.children, value->aggregate.count);
		for (uint32_t i = 0; i < value->aggregate.count; i++) {
			const tast_node *statement = tast_get(emitter->arena, items[i]);
			if (!statement || statement->kind != tast_expression_statement)
				twarn(ErrCompile_Other, "rule condition block",
				      "only expressions are allowed");
			tast_emit_expression(
				emitter, statement->expression_statement.value);
			tvmcmd_vect_append(emitter->instructions,
				tbycode_make_u(OP_RULEITEM, description_id));
			treg_ctr_ddt(&emitter->cp->regctr);
		}
		return;
	}
	tast_emit_expression(emitter, condition->rule_condition.value);
	tvmcmd_vect_append(emitter->instructions,
		tbycode_make_u(OP_RULEITEM, description_id));
	treg_ctr_ddt(&emitter->cp->regctr);
}

static void emit_rule_implication(tast_emitter *emitter, const tast_node *node)
{
	const tast_node *body = tast_get(emitter->arena, node->rule_implication.consequent);
	uint32_t count = body->kind == tast_block ? body->aggregate.count : 1;
	const tast_id *items = body->kind == tast_block ? tast_get_children(
		emitter->arena, body->aggregate.children, count) : nullptr;
	tstring *description;
	if (node->rule_implication.has_description) {
		tast_node literal = { .kind = tast_string,
			.span = node->rule_implication.description };
		description = tast_string_value(emitter->document, &literal);
	} else description = tast_emitter_text(emitter, node->span);
	uint_csts label = tconsts_add_str_const(emitter->constants, tstring_cstr(description));
	tstring_free(description);
	tvmcmd_vect *code = emitter->instructions;
	tast_emit_expression(emitter, node->rule_implication.antecedent);
	tvmcmd_vect_append(code, tbycode_make_u(OP_RULECOND, TRULE_ANTECEDENT_RECORD));
	uint_cmds branch = tvmcmd_vect_size32(code);
	tvmcmd_vect_append(code, tbycode_make_u(OP_CJPFPOP, 0));
	treg_ctr_ddt(&emitter->cp->regctr);
	/* One guard record and one record per consequent, on either branch. */
	tvmcmd_vect_append(code, tbycode_make_u(OP_PUSHB, 1));
	treg_ctr_add(&emitter->cp->regctr);
	tvmcmd_vect_append(code, tbycode_make_u(OP_RULECOND, label));
	treg_ctr_ddt(&emitter->cp->regctr);
	for (uint32_t i = 0; i < count; i++) {
		tast_id value = items ? tast_get(emitter->arena, items[i])->expression_statement.value :
			node->rule_implication.consequent;
		tast_emit_expression(emitter, value);
		tvmcmd_vect_append(code, tbycode_make_u(OP_RULECOND, label));
		treg_ctr_ddt(&emitter->cp->regctr);
	}
	uint_cmds end_jump = tvmcmd_vect_size32(code);
	tvmcmd_vect_append(code, tbycode_make_u(OP_JPF, 0));
	code->data[branch] = tbycode_make_u(OP_CJPFPOP,
		tvmcmd_vect_size32(code) - branch - 1);
	for (uint32_t i = 0; i <= count; i++) {
		tvmcmd_vect_append(code, tbycode_make_u(OP_PUSHB, i ? 1 : 0));
		treg_ctr_add(&emitter->cp->regctr);
		tvmcmd_vect_append(code, tbycode_make_u(OP_RULECOND, label));
		treg_ctr_ddt(&emitter->cp->regctr);
	}
	code->data[end_jump] = tbycode_make_u(OP_JPF,
		tvmcmd_vect_size32(code) - end_jump - 1);
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
			tstring *name = tast_emitter_text(emitter, item->named_field.name);
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
		if (!assigned[i] && !ttypeval_field_optional(
			target, tstring_cstr(order[i])))
			twarn(ErrCompile_Other, "structure literal",
			      "all target fields must be initialized");
	free(assigned);
	tvmcmd_vect_append(emitter->instructions,
		tbycode_make_u(OP_PUSHDICT, structure->aggregate.count));
	treg_ctr_ddt_n(&emitter->cp->regctr,
			  (uint_regs)structure->aggregate.count);
	treg_ctr_add(&emitter->cp->regctr);
}

void tast_emit_block(tast_emitter *emitter, const tast_node *block, int inblk)
{
	uint_objs original_temporaries =
		tobj_ctr_obj_len_all(&emitter->cp->tmpctr);
	const tast_id *statements = tast_get_children(
		emitter->arena, block->aggregate.children, block->aggregate.count);
	for (uint32_t i = 0; i < block->aggregate.count; i++) {
		uint_regs original_registers = treg_ctr_get(&emitter->cp->regctr);
		emit_statement(emitter, statements[i], 1, inblk);
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

static void emit_conditional(tast_emitter *emitter,
			     const tast_node *statement)
{
	uint32_t count = statement->conditional_statement.branch_count;
	const tast_id *branches = tast_get_children(emitter->arena,
		statement->conditional_statement.branches, count);
	uint_cmds *end_jumps = count ? calloc(count, sizeof(*end_jumps)) : nullptr;
	if (count && !end_jumps) abort();
	uint32_t end_jump_count = 0;
	for (uint32_t i = 0; branches && i < count; i++) {
		const tast_node *branch = tast_get(emitter->arena, branches[i]);
		if (!branch) continue;
		uint_cmds condition_jump = UINT32_MAX;
		if (branch->conditional_branch.has_condition) {
			tast_emit_expression(
				emitter, branch->conditional_branch.condition);
			tvmcmd_vect_append(emitter->instructions,
				tbycode_make_u(OP_CJPFPOP, 0));
			condition_jump = tvmcmd_vect_size32(
				emitter->instructions) - 1;
			treg_ctr_ddt(&emitter->cp->regctr);
		}
		tast_emit_block(emitter, tast_get(
			emitter->arena, branch->conditional_branch.body), 1);
		if (branch->conditional_branch.has_condition && i + 1 < count) {
			tvmcmd_vect_append(emitter->instructions,
				tbycode_make_u(OP_JPF, 0));
			end_jumps[end_jump_count++] = tvmcmd_vect_size32(
				emitter->instructions) - 1;
		}
		if (condition_jump != UINT32_MAX) {
			uint_cmds after = tvmcmd_vect_size32(emitter->instructions);
			emitter->instructions->data[condition_jump] = tbycode_make_u(
				OP_CJPFPOP, after - condition_jump - 1);
		}
	}
	uint_cmds end = tvmcmd_vect_size32(emitter->instructions);
	for (uint32_t i = 0; i < end_jump_count; i++)
		emitter->instructions->data[end_jumps[i]] = tbycode_make_u(
			OP_JPF, end - end_jumps[i] - 1);
	free(end_jumps);
}

static void emit_while(tast_emitter *emitter, const tast_node *statement)
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
		tast_get(emitter->arena, statement->control_statement.body), 1);
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

static void emit_for(tast_emitter *emitter, const tast_node *statement)
{
	uint_objs original_temporaries =
		tobj_ctr_obj_len_cur(&emitter->cp->tmpctr);
	tast_emit_expression(emitter, statement->for_statement.iterable);
	tstring *name = tast_emitter_text(emitter, statement->for_statement.name);
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
		tast_get(emitter->arena, statement->for_statement.body), 1);
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
	tvmcmd_vect_free(&body);
}

static void emit_declaration(tast_emitter *emitter,
			     const tast_node *declaration, int inblk)
{
	int is_mutable = declaration->declaration_statement.is_mutable;
	tstring *name = tast_emitter_text(emitter,
		declaration->declaration_statement.name);
	ttypeval *annotation = declaration->declaration_statement.has_annotation ?
		tast_resolve_annotation(
			emitter, declaration->declaration_statement.annotation) : nullptr;
	int has_initializer = declaration->declaration_statement.has_initializer;
	const tast_node *initializer = has_initializer ? tast_get(emitter->arena,
		declaration->declaration_statement.initializer) : nullptr;
	const tsemantic_symbol *symbol = tsemantic_symbol_for_declaration(
		&emitter->frontend->semantic,
		(tast_id)(declaration - emitter->arena->nodes));
	int environment_binding = is_mutable || (symbol && symbol->captured);
	ttypeval *static_value = nullptr;
	ttypeval *inferred = has_initializer ? tast_infer_expression_type(
		emitter, declaration->declaration_statement.initializer,
		&static_value) : nullptr;
	if (is_mutable) {
		ttypeval_release(static_value);
		static_value = nullptr;
	}
	if (!has_initializer && !is_mutable && annotation &&
	    ttypeval_equal(annotation,
		ttypeval_builtin(tbuiltintype_type)))
		static_value = ttypeval_new_recursive();
	tstring **field_order = nullptr;
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

	tobj_ctr *bindings = environment_binding ?
		&emitter->cp->objctr : &emitter->cp->tmpctr;
	uint_objs other_location = tobj_ctr_obj_loc(
		environment_binding ? &emitter->cp->tmpctr : &emitter->cp->objctr, name);
	if (other_location < tobj_ctr_obj_len_cur(
		environment_binding ? &emitter->cp->tmpctr : &emitter->cp->objctr) &&
	    (is_mutable || !tobj_ctr_is_preload(
		&emitter->cp->objctr, other_location)))
		twarn(ErrCompile_DblVDeclare, "declaration", tstring_cstr(name));
	uint_csts name_location = UNDEF_NAMELOC;
	uint_objs location = tobj_ctr_obj_create(
		bindings, name, environment_binding ? inblk : 0,
		emitter->constants, &name_location);
	tcompile_set_metadata(bindings, location,
		annotation ? annotation : inferred,
		static_value, annotation != nullptr);
	bindings->bindings[location].initialized = has_initializer;
	bindings->bindings[location].is_types_package = has_initializer &&
		tast_is_types_package_expression(
			emitter, declaration->declaration_statement.initializer);
	tcompile_set_field_order(bindings, location, field_order,
				 field_order_count);
	tvmcmd_vect_append(emitter->instructions,
		tbycode_make_lr(OP_VCRT, (uint16_t)name_location,
				(uint16_t)environment_binding));
	if (!has_initializer) {
		if (static_value) {
			tvmcmd_vect_append(emitter->instructions,
				tbycode_make(OP_TYPEFWD));
			treg_ctr_add(&emitter->cp->regctr);
			tvmcmd_vect_append(emitter->instructions,
				tbycode_make_lr(OP_POPCOV, (uint16_t)location,
					(uint16_t)environment_binding));
			treg_ctr_ddt(&emitter->cp->regctr);
		}
		tstring_free(name);
		ttypeval_release(annotation);
		ttypeval_release(inferred);
		ttypeval_release(static_value);
		tast_free_field_order(field_order, field_order_count);
		return;
	}
	if (initializer->kind == tast_structure)
		emit_structure(emitter, initializer, annotation,
			       field_order, field_order_count);
	else {
		const ttypeval *saved_pending = emitter->pending_function_type;
		const char *saved_name = emitter->pending_display_name;
		emitter->pending_display_name = (initializer->kind == tast_function && declaration->declaration_statement.is_function_declaration) ? tstring_cstr(name) : nullptr;
		if (initializer->kind == tast_function && inferred &&
		    inferred->kind == ttype_kind_function)
			emitter->pending_function_type = inferred;
		tast_emit_expression(
			emitter, declaration->declaration_statement.initializer);
		emitter->pending_function_type = saved_pending;
		emitter->pending_display_name = saved_name;
	}
	if (annotation && (annotation->contains_instance || annotation->contains_custom)) {
		tast_emit_bound_type(emitter, annotation);
		tvmcmd_vect_append(emitter->instructions, tbycode_make(OP_CHECKTYPE));
		treg_ctr_ddt(&emitter->cp->regctr);
	}
	tvmcmd_vect_append(emitter->instructions,
		tbycode_make_lr(OP_POPCOV, (uint16_t)location,
				(uint16_t)environment_binding));
	treg_ctr_ddt(&emitter->cp->regctr);

	tstring_free(name);
	ttypeval_release(annotation);
	ttypeval_release(inferred);
	ttypeval_release(static_value);
	tast_free_field_order(field_order, field_order_count);
}

static void emit_return(tast_emitter *emitter, const tast_node *statement)
{
	tast_id value_id = statement->return_statement.value;
	if (value_id != TAST_INVALID_ID) {
		const tast_node *value = tast_get(emitter->arena, value_id);
		if (value->kind == tast_name) {
			tstring *name = tast_emitter_text(emitter, value->span);
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
			int inblk)
{
	tstring *path = tast_string_contents_value(
		emitter->document, statement->import_statement.path);
	if (!path)
		twarn(ErrCompile_InvalidLiter, "emit_import", "invalid escape");
	tstring *alias = statement->import_statement.has_alias ?
		tast_emitter_text(emitter, statement->import_statement.alias) : nullptr;
	tcompile_emit_import(emitter->cp, path, alias, emitter->instructions,
			     emitter->constants, emitter->paths, emitter->npaths, inblk);
	tstring_free(alias);
	tstring_free(path);
}

static void emit_assignment(tast_emitter *emitter, const tast_node *statement,
			    int inblk)
{
	const tast_node *target = tast_get(
		emitter->arena, statement->assignment_statement.target);
	if (!target)
		twarn(ErrCompile_Other, "assignment", "missing assignment target");
	if (target->kind == tast_name) {
		tstring *name = tast_emitter_text(emitter, target->span);
		uint_objs temporary_location =
			tobj_ctr_obj_loc(&emitter->cp->tmpctr, name);
		uint_objs location = temporary_location;
		int is_environment = 0;
		tobj_ctr *owner = nullptr;
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
		if (owner && owner->bindings[owner_slot].has_annotation &&
		    owner->bindings[owner_slot].value_type &&
		    owner->bindings[owner_slot].value_type->contains_instance)
			twarn(ErrCompile_Other, "InstanceOf",
			      "reassignment of a value-bound annotation is not supported");
		const tast_node *value = tast_get(
			emitter->arena, statement->assignment_statement.value);
		ttypeval *assigned_static = nullptr;
		ttypeval *actual = value->kind == tast_structure ? nullptr :
			tast_infer_expression_type(
				emitter, statement->assignment_statement.value,
				&assigned_static);
		int defines_recursive = owner && owner->bindings[owner_slot].type_value &&
			owner->bindings[owner_slot].type_value->kind == ttype_kind_recursive &&
			!owner->bindings[owner_slot].type_value->recursive_defined;
		if (defines_recursive && inblk)
			twarn(ErrCompile_Other, "recursive Type",
			      "definition must be unconditional in its declaring block");
		if (defines_recursive && (!assigned_static ||
		    !ttypeval_define_recursive(
			owner->bindings[owner_slot].type_value, assigned_static)))
			twarn(ErrCompile_Other, "recursive Type",
			      "initial value must be a static Type definition");
		if (owner && !owner->bindings[owner_slot].has_annotation) {
			tcompile_set_metadata(
				owner, owner_slot, actual, nullptr, 0);
			tstring **new_order = nullptr;
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
				ttypeval_builtin(tbuiltintype_type))) {
			tcompile_set_metadata(owner, owner_slot,
				owner->bindings[owner_slot].value_type,
				inblk ? nullptr : assigned_static, 1);
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
			tast_emit_expression(
				emitter, statement->assignment_statement.value);
		}
        if (owner && owner->bindings[owner_slot].has_annotation &&
            owner->bindings[owner_slot].value_type && owner->bindings[owner_slot].value_type->contains_custom) {
            tast_emit_bound_type(emitter, owner->bindings[owner_slot].value_type);
            tvmcmd_vect_append(emitter->instructions, tbycode_make(OP_CHECKTYPE));
            treg_ctr_ddt(&emitter->cp->regctr);
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
	tstring *name = tast_emitter_text(emitter, receiver->span);
	tobj_ctr *owner = nullptr;
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
			   int cleanstk, int inblk)
{
	const tast_node *statement = tast_get(emitter->arena, id);
	switch (statement->kind) {
	case tast_expression_statement:
		tast_emit_expression(emitter,
			statement->expression_statement.value);
		break;
	case tast_rule_condition:
		emit_rule_condition(emitter, statement);
		break;
	case tast_rule_implication:
		emit_rule_implication(emitter, statement);
		break;

	case tast_declaration_statement:
		emit_declaration(emitter, statement, inblk);
		break;
	case tast_declaration_group: {
		const tast_id *children = tast_get_children(
			emitter->arena, statement->aggregate.children,
			statement->aggregate.count);
		for (uint32_t i = 0; i < statement->aggregate.count; i++)
			emit_statement(emitter, children[i], cleanstk, inblk);
	} break;
	case tast_assignment_statement:
		emit_assignment(emitter, statement, inblk);
		break;
	case tast_return_statement:
		emit_return(emitter, statement);
		break;
	case tast_import_statement:
		emit_import(emitter, statement, inblk);
		break;
	case tast_if_statement:
		emit_conditional(emitter, statement);
		break;
	case tast_while_statement:
		emit_while(emitter, statement);
		break;
	case tast_for_statement:
		emit_for(emitter, statement);
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
	tstring **paths, uint_lexs npaths, int cleanstk, int inblk,
	int keep_bindings)
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
		emit_statement(&emitter, frontend.root, cleanstk, inblk);
	else if (keep_bindings) {
		const tast_node *block = tast_get(&frontend.arena, frontend.root);
		const tast_id *statements = tast_get_children(&frontend.arena,
			block->aggregate.children, block->aggregate.count);
		/* Each fence continues the session, including its local bindings. */
		for (uint32_t i = 0; i < block->aggregate.count; i++) {
			uint_regs original = treg_ctr_get(&cp->regctr);
			emit_statement(&emitter, statements[i], 1, inblk);
			clean_stk(cp, tcmds, 1, original);
		}
	} else
		tast_emit_block(&emitter,
			tast_get(&frontend.arena, frontend.root), inblk);
	sync_initialization(&emitter);
	tfrontend_free(&frontend);
}

void tcompile_ast_statement(tcp *cp, const tstring *source,
			       tvmcmd_vect *tcmds, tconsts *consts,
			       tstring **paths, uint_lexs npaths,
			       int cleanstk, int inblk)
{
	compile_ast_source(cp, source, tfrontend_statement,
		tcmds, consts, paths, npaths, cleanstk, inblk, 0);
}

void tcompile_ast_module(tcp *cp, const tstring *source,
			    tvmcmd_vect *tcmds, tconsts *consts,
			    tstring **paths, uint_lexs npaths,
			    int inblk)
{
	compile_ast_source(cp, source, tfrontend_module,
		tcmds, consts, paths, npaths, 1, inblk, 0);
}

void tcompile_ast_sequence(tcp *cp, const tstring *source,
                          tvmcmd_vect *tcmds, tconsts *consts,
                          tstring **paths, uint_lexs npaths)
{
	compile_ast_source(cp, source, tfrontend_module,
		tcmds, consts, paths, npaths, 1, 0, 1);
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
		tobj_ctr *owner = nullptr;
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
