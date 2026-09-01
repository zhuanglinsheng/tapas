#include "tapas/compile/frontend.h"
#include "tapas/compile/parser.h"
#include "tapas/compile/semantic.h"
#include "tapas/compile/module.h"
#include "tapas/compile/workspace.h"

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

typedef struct {
	tsource_document document;
	tsyntax_tokens tokens;
	tast_arena arena;
	tdiagnostics diagnostics;
	tast_id root;
} parsed_expression;

static parsed_expression parse(const char *source)
{
	parsed_expression parsed;
	tsource_document_init(&parsed.document, "test.tap", source);
	tsyntax_tokens_init(&parsed.tokens);
	tsyntax_lex(&parsed.document, &parsed.tokens);
	tast_arena_init(&parsed.arena);
	tdiagnostics_init(&parsed.diagnostics);
	tparser parser;
	tparser_init(&parser, &parsed.document, &parsed.tokens,
		     &parsed.arena, &parsed.diagnostics);
	parsed.root = tparser_parse_expression(&parser);
	return parsed;
}

static parsed_expression parse_statement(const char *source)
{
	parsed_expression parsed;
	tsource_document_init(&parsed.document, "test.tap", source);
	tsyntax_tokens_init(&parsed.tokens);
	tsyntax_lex(&parsed.document, &parsed.tokens);
	tast_arena_init(&parsed.arena);
	tdiagnostics_init(&parsed.diagnostics);
	tparser parser;
	tparser_init(&parser, &parsed.document, &parsed.tokens,
		     &parsed.arena, &parsed.diagnostics);
	parsed.root = tparser_parse_statement(&parser);
	return parsed;
}

static parsed_expression parse_module(const char *source)
{
	parsed_expression parsed;
	tsource_document_init(&parsed.document, "test.tap", source);
	tsyntax_tokens_init(&parsed.tokens);
	tsyntax_lex(&parsed.document, &parsed.tokens);
	tast_arena_init(&parsed.arena);
	tdiagnostics_init(&parsed.diagnostics);
	tparser parser;
	tparser_init(&parser, &parsed.document, &parsed.tokens,
		     &parsed.arena, &parsed.diagnostics);
	parsed.root = tparser_parse_module(&parser);
	return parsed;
}

static void parsed_free(parsed_expression *parsed)
{
	tdiagnostics_free(&parsed->diagnostics);
	tast_arena_free(&parsed->arena);
	tsyntax_tokens_free(&parsed->tokens);
	tsource_document_free(&parsed->document);
}

static const tast_node *node(const parsed_expression *parsed, tast_id id)
{
	const tast_node *result = tast_get(&parsed->arena, id);
	assert(result);
	return result;
}

static void test_document_and_lossless_tokens(void)
{
	parsed_expression parsed = parse("a + 2 // note\n  * b");
	uint32_t previous = 0;
	for (uint32_t i = 0; i < parsed.tokens.count; i++) {
		assert(parsed.tokens.items[i].span.start == previous);
		previous = parsed.tokens.items[i].span.end;
	}
	assert(previous == tsource_document_length(&parsed.document));
	uint32_t line = 0, column = 0;
	tsource_document_position(&parsed.document, 16, &line, &column);
	assert(line == 1 && column == 2);
	parsed_expression unicode = parse("a😀中\nvalue");
	uint32_t lsp_line = 0, lsp_character = 0;
	tsource_document_lsp_position(&unicode.document, 8,
		&lsp_line, &lsp_character);
	assert(lsp_line == 0 && lsp_character == 4);
	assert(tsource_document_lsp_offset(&unicode.document, 0, 3) == 5);
	parsed_free(&unicode);
	assert(parsed.diagnostics.count == 0);
	parsed_free(&parsed);
}

static void test_precedence_and_postfix(void)
{
	parsed_expression parsed = parse("-2 ^ 2 + f(3, x)::value[0]");
	assert(parsed.diagnostics.count == 0);
	const tast_node *root = node(&parsed, parsed.root);
	assert(root->kind == tast_binary && root->binary.op == tsyntax_plus);
	const tast_node *left = node(&parsed, root->binary.left);
	assert(left->kind == tast_unary && left->unary.op == tsyntax_minus);
	const tast_node *power = node(&parsed, left->unary.operand);
	assert(power->kind == tast_binary && power->binary.op == tsyntax_power);
	const tast_node *index = node(&parsed, root->binary.right);
	assert(index->kind == tast_index);
	const tast_node *member = node(&parsed, index->aggregate.receiver);
	assert(member->kind == tast_member &&
	       member->member.op == tsyntax_scope);
	const tast_node *call = node(&parsed, member->member.receiver);
	assert(call->kind == tast_call && call->aggregate.count == 2);
	parsed_free(&parsed);
}

static void test_tunnel_call_is_distinct_from_member_call(void)
{
	parsed_expression parsed = parse("items.append(3)");
	assert(parsed.diagnostics.count == 0);
	const tast_node *call = node(&parsed, parsed.root);
	assert(call->kind == tast_call);
	const tast_node *member = node(&parsed, call->aggregate.receiver);
	assert(member->kind == tast_member &&
	       member->member.op == tsyntax_dot);
	parsed_free(&parsed);
}

static void test_right_associativity(void)
{
	parsed_expression parsed = parse("1 : 2 : 3 ^ 4 ^ 5");
	assert(parsed.diagnostics.count == 0);
	const tast_node *pair = node(&parsed, parsed.root);
	assert(pair->kind == tast_binary && pair->binary.op == tsyntax_colon);
	const tast_node *right_pair = node(&parsed, pair->binary.right);
	assert(right_pair->kind == tast_binary &&
	       right_pair->binary.op == tsyntax_colon);
	const tast_node *power = node(&parsed, right_pair->binary.right);
	assert(power->kind == tast_binary && power->binary.op == tsyntax_power);
	const tast_node *right_power = node(&parsed, power->binary.right);
	assert(right_power->kind == tast_binary &&
	       right_power->binary.op == tsyntax_power);
	parsed_free(&parsed);
}

static void test_collections(void)
{
	parsed_expression parsed = parse("{'x': [1, 2], 'y': 3}");
	assert(parsed.diagnostics.count == 0);
	const tast_node *dictionary = node(&parsed, parsed.root);
	assert(dictionary->kind == tast_dictionary &&
	       dictionary->aggregate.count == 4);
	const tast_id *entries = tast_get_children(
		&parsed.arena, dictionary->aggregate.children,
		dictionary->aggregate.count);
	assert(entries);
	assert(node(&parsed, entries[1])->kind == tast_list);
	parsed_free(&parsed);
}

static void test_recoverable_errors(void)
{
	parsed_expression chained = parse("1 < 2 < 3");
	assert(chained.diagnostics.count == 1);
	assert(node(&chained, chained.root)->kind == tast_binary);
	parsed_free(&chained);

	parsed_expression incomplete = parse("value +");
	assert(incomplete.diagnostics.count >= 1);
	const tast_node *root = node(&incomplete, incomplete.root);
	assert(root->kind == tast_binary);
	assert(node(&incomplete, root->binary.right)->kind == tast_error);
	parsed_free(&incomplete);
}

static void test_basic_statements(void)
{
	parsed_expression declaration =
		parse_statement("let answer: types::Int = 40 + 2");
	assert(declaration.diagnostics.count == 0);
	const tast_node *decl = node(&declaration, declaration.root);
	assert(decl->kind == tast_declaration_statement);
	assert(!decl->declaration_statement.is_mutable);
	assert(decl->declaration_statement.has_annotation);
	assert(node(&declaration, decl->declaration_statement.initializer)->kind ==
	       tast_binary);
	tstring *annotation = tsource_document_slice(
		&declaration.document, decl->declaration_statement.annotation);
	assert(strcmp(tstring_cstr(annotation), "types::Int") == 0);
	tstring_free(annotation);
	parsed_free(&declaration);

	parsed_expression parameterized = parse_statement(
		"let values: List[Dictionary[String, Int,],] = []");
	assert(parameterized.diagnostics.count == 0);
	const tast_node *parameterized_decl = node(
		&parameterized, parameterized.root);
	assert(parameterized_decl->kind == tast_declaration_statement);
	annotation = tsource_document_slice(
		&parameterized.document,
		parameterized_decl->declaration_statement.annotation);
	assert(strcmp(tstring_cstr(annotation),
		      "List[Dictionary[String, Int,],]") == 0);
	tstring_free(annotation);
	parsed_free(&parameterized);

	parsed_expression assignment = parse_statement("items[0] = value");
	assert(assignment.diagnostics.count == 0);
	const tast_node *asg = node(&assignment, assignment.root);
	assert(asg->kind == tast_assignment_statement);
	assert(node(&assignment, asg->assignment_statement.target)->kind ==
	       tast_index);
	parsed_free(&assignment);

	parsed_expression returned = parse_statement("return");
	assert(returned.diagnostics.count == 0);
	const tast_node *ret = node(&returned, returned.root);
	assert(ret->kind == tast_return_statement &&
	       ret->return_statement.value == TAST_INVALID_ID);
	parsed_free(&returned);

	parsed_expression multiple = parse_statement("let a = 1, b = 2");
	assert(multiple.diagnostics.count == 0);
	const tast_node *group = node(&multiple, multiple.root);
	assert(group->kind == tast_declaration_group &&
	       group->aggregate.count == 2);
	parsed_free(&multiple);
}

static void test_control_flow_blocks(void)
{
	parsed_expression parsed = parse_statement(
		"while(value < 3){\n"
		"  let next = value + 1; value = next\n"
		"  if(value == 2){ continue }\n"
		"}");
	assert(parsed.diagnostics.count == 0);
	const tast_node *while_statement = node(&parsed, parsed.root);
	assert(while_statement->kind == tast_while_statement);
	assert(node(&parsed, while_statement->control_statement.condition)->kind ==
	       tast_binary);
	const tast_node *body = node(&parsed, while_statement->control_statement.body);
	assert(body->kind == tast_block && body->aggregate.count == 3);
	const tast_id *statements = tast_get_children(
		&parsed.arena, body->aggregate.children, body->aggregate.count);
	assert(statements);
	assert(node(&parsed, statements[0])->kind ==
	       tast_declaration_statement);
	assert(node(&parsed, statements[1])->kind ==
	       tast_assignment_statement);
	const tast_node *conditional = node(&parsed, statements[2]);
	assert(conditional->kind == tast_if_statement);
	const tast_node *conditional_body = node(
		&parsed, conditional->control_statement.body);
	assert(conditional_body->kind == tast_block &&
	       conditional_body->aggregate.count == 1);
	parsed_free(&parsed);

	parsed_expression loop = parse_statement(
		"for(let item in 0 to 3){ if(item == 1){ break } }");
	assert(loop.diagnostics.count == 0);
	const tast_node *for_statement = node(&loop, loop.root);
	assert(for_statement->kind == tast_for_statement &&
	       for_statement->for_statement.declares_binding);
	tstring *binding = tsource_document_slice(
		&loop.document, for_statement->for_statement.name);
	assert(strcmp(tstring_cstr(binding), "item") == 0);
	tstring_free(binding);
	parsed_free(&loop);

	parsed_expression alternative =
		parse_statement("elif(value == 3){ value = 4 }");
	assert(alternative.diagnostics.count == 0);
	assert(node(&alternative, alternative.root)->kind ==
	       tast_elif_statement);
	parsed_free(&alternative);

	parsed_expression fallback =
		parse_statement("else { value = 5 }");
	assert(fallback.diagnostics.count == 0);
	assert(node(&fallback, fallback.root)->kind ==
	       tast_else_statement);
	parsed_free(&fallback);
}

static void test_function_literals(void)
{
	parsed_expression parsed = parse(
		"(left: Int, right: List[Int]) -> Int { "
		"let sum = left + right[0]; return sum }");
	assert(parsed.diagnostics.count == 0);
	const tast_node *function = node(&parsed, parsed.root);
	assert(function->kind == tast_function);
	assert(function->function.parameter_count == 2);
	assert(!function->function.variadic);
	const tast_id *parameters = tast_get_children(
		&parsed.arena, function->function.parameters,
		function->function.parameter_count);
	assert(parameters);
	assert(node(&parsed, parameters[0])->kind == tast_parameter);
	assert(node(&parsed, parameters[0])->parameter.has_annotation);
	tstring *first = tsource_document_slice(
		&parsed.document, node(&parsed, parameters[0])->parameter.name);
	assert(strcmp(tstring_cstr(first), "left") == 0);
	tstring_free(first);
	tstring *first_type = tsource_document_slice(
		&parsed.document, node(&parsed, parameters[0])->parameter.annotation);
	assert(strcmp(tstring_cstr(first_type), "Int") == 0);
	tstring_free(first_type);
	assert(function->function.has_return_annotation);
	tstring *result_type = tsource_document_slice(
		&parsed.document, function->function.return_annotation);
	assert(strcmp(tstring_cstr(result_type), "Int") == 0);
	tstring_free(result_type);
	const tast_node *body = node(&parsed, function->function.body);
	assert(body->kind == tast_block && body->aggregate.count == 2);
	parsed_free(&parsed);

	parsed_expression variadic = parse("(...){ return __nparam__() }");
	assert(variadic.diagnostics.count == 0);
	const tast_node *variadic_function = node(&variadic, variadic.root);
	assert(variadic_function->kind == tast_function);
	assert(variadic_function->function.variadic);
	assert(variadic_function->function.parameter_count == 0);
	parsed_free(&variadic);

	parsed_expression removed =
		parse("function(value){ return value }");
	assert(removed.diagnostics.count != 0);
	parsed_free(&removed);

	parsed_expression named = parse_statement(
		"function identity(value: Int) -> Int { return value }");
	assert(named.diagnostics.count == 0);
	const tast_node *declaration = node(&named, named.root);
	assert(declaration->kind == tast_declaration_statement);
	const tast_node *named_function = node(
		&named, declaration->declaration_statement.initializer);
	assert(named_function->kind == tast_function);
	assert(named_function->function.parameter_count == 1);
	assert(named_function->function.has_return_annotation);
	parsed_free(&named);
}

static void test_module_tree(void)
{
	parsed_expression parsed = parse_module(
		"let offset = 2\n"
		"let add = (value){ return value + offset }\n"
		"print(add(3))\n");
	assert(parsed.diagnostics.count == 0);
	const tast_node *module = node(&parsed, parsed.root);
	assert(module->kind == tast_module && module->aggregate.count == 3);
	const tast_id *statements = tast_get_children(
		&parsed.arena, module->aggregate.children, module->aggregate.count);
	assert(statements);
	assert(node(&parsed, statements[0])->kind == tast_declaration_statement);
	const tast_node *function_declaration = node(&parsed, statements[1]);
	assert(function_declaration->kind == tast_declaration_statement);
	assert(node(&parsed,
		function_declaration->declaration_statement.initializer)->kind ==
	       tast_function);
	assert(node(&parsed, statements[2])->kind == tast_expression_statement);
	parsed_free(&parsed);

	parsed_expression imports = parse_module(
		"import path/to/module.tap\n"
		"import 'path with spaces/module.tap' as module\n");
	assert(imports.diagnostics.count == 0);
	const tast_node *import_module = node(&imports, imports.root);
	assert(import_module->kind == tast_module &&
	       import_module->aggregate.count == 2);
	const tast_id *import_statements = tast_get_children(
		&imports.arena, import_module->aggregate.children,
		import_module->aggregate.count);
	const tast_node *bare = node(&imports, import_statements[0]);
	const tast_node *aliased = node(&imports, import_statements[1]);
	assert(bare->kind == tast_import_statement &&
	       !bare->import_statement.has_alias);
	assert(aliased->kind == tast_import_statement &&
	       aliased->import_statement.has_alias);
	tstring *path = tsource_document_slice(
		&imports.document, aliased->import_statement.path);
	assert(strcmp(tstring_cstr(path), "path with spaces/module.tap") == 0);
	tstring_free(path);
	parsed_free(&imports);
}

static void test_semantic_model(void)
{
	parsed_expression parsed = parse_module(
		"var outer = 1\n"
		"let function_value = (parameter){\n"
		"  let inner = parameter\n"
		"  return inner + outer\n"
		"}\n");
	assert(parsed.diagnostics.count == 0);
	tsemantic_model semantic;
	tsemantic_model_init(&semantic);
	tsemantic_analyze(&parsed.document, &parsed.arena, parsed.root,
		&semantic, &parsed.diagnostics);
	assert(parsed.diagnostics.count == 0);
	assert(semantic.symbol_count == 4);
	int resolved_parameter = 0;
	int resolved_inner = 0;
	int resolved_outer = 0;
	for (tast_id id = 0; id < parsed.arena.node_count; id++) {
		const tast_node *candidate = tast_get(&parsed.arena, id);
		if (!candidate || candidate->kind != tast_name)
			continue;
		const tsemantic_symbol *symbol =
			tsemantic_resolved_symbol(&semantic, id);
		if (!symbol)
			continue;
		const char *name = tstring_cstr(symbol->name);
		resolved_parameter += strcmp(name, "parameter") == 0;
		resolved_inner += strcmp(name, "inner") == 0;
		resolved_outer += strcmp(name, "outer") == 0;
	}
	assert(resolved_parameter == 1);
	assert(resolved_inner == 1);
	assert(resolved_outer == 1);
	uint32_t reference_offset = (uint32_t)(strstr(
		tstring_cstr(parsed.document.text), "inner +") -
		tstring_cstr(parsed.document.text));
	const tsemantic_symbol *at = tsemantic_symbol_at(
		&semantic, &parsed.arena, reference_offset, NULL);
	assert(at && strcmp(tstring_cstr(at->name), "inner") == 0);
	tsemantic_model_free(&semantic);
	parsed_free(&parsed);
}

static void test_let_capture_diagnostic(void)
{
	tfrontend frontend;
	tfrontend_init(&frontend, "capture.tap",
		"let value = 1\n"
		"let reader = () { return value }\n",
		tfrontend_module);
	assert(frontend.diagnostics.count == 1);
	assert(strcmp(tstring_cstr(frontend.diagnostics.items[0].message),
		"let binding cannot be captured") == 0);
	tfrontend_free(&frontend);
}

static void test_editor_type_information(void)
{
	tfrontend frontend;
	tfrontend_init(&frontend, "types.tap",
		"let count = 1\n"
		"let ratio: Float = count\n"
		"let values = [count, 2]\n"
		"function add(left: Int, right: List[Int]) -> Int {\n"
		"  return left + right[0]\n"
		"}\n"
		"let total = add(1, [2])\n",
		tfrontend_module);
	assert(frontend.diagnostics.count == 0);
	assert(frontend.semantic.symbol_count == 7);
	assert(strcmp(ttype_info_for_symbol(&frontend.types, &frontend.semantic,
		&frontend.semantic.symbols[0]), "Int") == 0);
	assert(strcmp(ttype_info_for_symbol(&frontend.types, &frontend.semantic,
		&frontend.semantic.symbols[1]), "Float") == 0);
	assert(strcmp(ttype_info_for_symbol(&frontend.types, &frontend.semantic,
		&frontend.semantic.symbols[2]), "List[Int]") == 0);
	int found_add = 0, found_left = 0, found_right = 0, found_total = 0;
	for (uint32_t i = 3; i < frontend.semantic.symbol_count; i++) {
		const tsemantic_symbol *symbol = &frontend.semantic.symbols[i];
		const char *type = ttype_info_for_symbol(&frontend.types,
			&frontend.semantic, symbol);
		if (strcmp(tstring_cstr(symbol->name), "add") == 0) {
			assert(type && strcmp(type,
				"Function[Int, List[Int]] -> Int") == 0);
			found_add = 1;
		} else if (strcmp(tstring_cstr(symbol->name), "left") == 0) {
			assert(type && strcmp(type, "Int") == 0);
			found_left = 1;
		} else if (strcmp(tstring_cstr(symbol->name), "right") == 0) {
			assert(type && strcmp(type, "List[Int]") == 0);
			found_right = 1;
		} else if (strcmp(tstring_cstr(symbol->name), "total") == 0) {
			assert(type && strcmp(type, "Int") == 0);
			found_total = 1;
		}
	}
	assert(found_add && found_left && found_right && found_total);
	tfrontend_free(&frontend);
}

static tstatic_type_id test_external_type_resolver(
	void *context, tstatic_type_arena *arena,
	const char *qualified_name, int static_value)
{
	int *calls = (int *)context;
	(*calls)++;
	if (static_value || strcmp(qualified_name, "external") != 0)
		return TSTATIC_TYPE_UNKNOWN;
	tstatic_type_id signature[] = {
		tstatic_type_builtin_id(arena, tstatic_builtin_int),
		tstatic_type_builtin_id(arena, tstatic_builtin_string)
	};
	return tstatic_type_make(
		arena, tstatic_type_function, signature, 2, 0);
}

static void test_external_type_information(void)
{
	tfrontend frontend;
	tfrontend_init(&frontend, "external.tap",
		"let result = external(1)\n", tfrontend_module);
	int resolver_calls = 0;
	ttype_info_analyze_with_resolver(
		&frontend.document, &frontend.arena, &frontend.semantic,
		&frontend.types, test_external_type_resolver, &resolver_calls);
	assert(resolver_calls > 0);
	const tsemantic_symbol *result = NULL;
	for (uint32_t i = 0; i < frontend.semantic.symbol_count; i++)
		if (strcmp(tstring_cstr(frontend.semantic.symbols[i].name),
			   "result") == 0)
			result = &frontend.semantic.symbols[i];
	assert(result);
	assert(strcmp(ttype_info_for_symbol(
		&frontend.types, &frontend.semantic, result), "String") == 0);
	tfrontend_free(&frontend);
}

static void test_declaration_without_initializer(void)
{
	parsed_expression parsed = parse_statement("let Node: Type");
	assert(parsed.diagnostics.count == 0);
	const tast_node *declaration = node(&parsed, parsed.root);
	assert(declaration->kind == tast_declaration_statement);
	assert(declaration->declaration_statement.has_annotation);
	assert(!declaration->declaration_statement.has_initializer);
	assert(declaration->declaration_statement.initializer == TAST_INVALID_ID);
	parsed_free(&parsed);
}

static void test_static_function_types(void)
{
	tstatic_type_arena arena;
	tstatic_type_arena_init(&arena);
	tstatic_type_id type = tstatic_type_parse(&arena,
		"Function[Int, List[String]] -> Dictionary[String, Int]",
		NULL, NULL);
	assert(type != TSTATIC_TYPE_UNKNOWN);
	tstring *formatted = tstatic_type_format(&arena, type);
	assert(strcmp(tstring_cstr(formatted),
		"Function[Int, List[String]] -> Dictionary[String, Int]") == 0);
	tstring_free(formatted);

	tstatic_type_id nested = tstatic_type_parse(&arena,
		"Function[] -> Function[Int] -> String", NULL, NULL);
	assert(nested != TSTATIC_TYPE_UNKNOWN);
	formatted = tstatic_type_format(&arena, nested);
	assert(strcmp(tstring_cstr(formatted),
		"Function[] -> Function[Int] -> String") == 0);
	tstring_free(formatted);
	assert(tstatic_type_parse(&arena, "Function[Int]", NULL, NULL) ==
		TSTATIC_TYPE_UNKNOWN);
	tstatic_type_arena_free(&arena);
}

static tstatic_type_id make_static_tree_type(tstatic_type_arena *arena,
					     tstatic_type_id value_type)
{
	tstatic_type_id recursive = tstatic_type_make_recursive(arena);
	tstatic_type_id list_children[] = { recursive };
	tstatic_type_id children = tstatic_type_make(
		arena, tstatic_type_list, list_children, 1, 0);
	tstatic_field fields[] = {
		{ tstring_new("value"), value_type },
		{ tstring_new("children"), children }
	};
	tstatic_type_id body = tstatic_type_make_fields(arena, fields, 2);
	tstring_free(fields[0].name);
	tstring_free(fields[1].name);
	assert(tstatic_type_define_recursive(arena, recursive, body));
	return recursive;
}

static void test_static_recursive_types(void)
{
	tstatic_type_arena arena;
	tstatic_type_arena_init(&arena);
	tstatic_type_id int_type = tstatic_type_builtin_id(
		&arena, tstatic_builtin_int);
	tstatic_type_id string_type = tstatic_type_builtin_id(
		&arena, tstatic_builtin_string);
	tstatic_type_id first = make_static_tree_type(&arena, int_type);
	tstatic_type_id second = make_static_tree_type(&arena, int_type);
	tstatic_type_id different = make_static_tree_type(&arena, string_type);
	assert(tstatic_type_equal(&arena, first, second));
	assert(tstatic_type_assignable(&arena, first, second));
	assert(!tstatic_type_equal(&arena, first, different));
	assert(!tstatic_type_assignable(&arena, first, different));
	tstatic_type_arena_free(&arena);
}

static void test_module_interface_and_standard_environment(void)
{
	tfrontend frontend;
	tfrontend_init(&frontend, "module.tap",
		"let answer = 42\n"
		"function double(value: Int) -> Int { return value + value }\n"
		"return {'answer': answer, 'double': double}\n",
		tfrontend_module);
	tmodule_interface interface;
	tmodule_interface_init(&interface, "file:///module.tap");
	tmodule_interface_extract(&frontend, "file:///module.tap", 1, &interface);
	assert(interface.export_count == 2);
	const tmodule_export *answer = tmodule_interface_find(&interface, "answer");
	const tmodule_export *double_value = tmodule_interface_find(&interface, "double");
	assert(answer && strcmp(tstring_cstr(answer->detail), "Int") == 0);
	assert(double_value && double_value->kind == tmodule_symbol_function);
	assert(strcmp(tstring_cstr(double_value->detail),
		"Function[Int] -> Int") == 0);
	assert(tstandard_package("math"));
	int found_sqrt = 0;
	for (uint32_t i = 0; i < tstandard_symbol_count(); i++) {
		const tstandard_symbol *symbol = tstandard_symbol_at(i);
		found_sqrt |= symbol->package && strcmp(symbol->package, "math") == 0 &&
			strcmp(symbol->name, "sqrt") == 0;
	}
	assert(found_sqrt);
	tmodule_interface_free(&interface);
	tfrontend_free(&frontend);
}

static void test_workspace_import_resolution(void)
{
	char directory[] = "/tmp/tapas-workspace-XXXXXX";
	assert(mkdtemp(directory));
	char module_path[512], bridge_path[512], main_path[512];
	snprintf(module_path, sizeof(module_path), "%s/module.tap", directory);
	snprintf(bridge_path, sizeof(bridge_path), "%s/bridge.tap", directory);
	snprintf(main_path, sizeof(main_path), "%s/main.tap", directory);
	FILE *file = fopen(module_path, "w");
	assert(file);
	fputs("let answer = 42\nreturn {'answer': answer}\n", file);
	fclose(file);
	file = fopen(bridge_path, "w");
	assert(file);
	fputs("import module.tap as nested\nreturn {'nested': nested}\n", file);
	fclose(file);
	tstring *main_uri = tworkspace_uri_from_path(main_path);
	tworkspace workspace;
	tworkspace_init(&workspace);
	tworkspace_add_root(&workspace, directory);
	tworkspace_document *main_document = tworkspace_open(&workspace,
		tstring_cstr(main_uri),
		"import module.tap as module\nlet value = module::answer\n", 1);
	assert(main_document && main_document->import_count == 1);
	assert(main_document->imports[0].resolved);
	uint32_t member_offset = (uint32_t)(strstr(
		tstring_cstr(main_document->frontend.document.text), "answer") -
		tstring_cstr(main_document->frontend.document.text));
	tworkspace_member_resolution member;
	assert(tworkspace_resolve_member(&workspace, main_document,
		member_offset, &member));
	assert(member.exported && strcmp(tstring_cstr(member.exported->name),
		"answer") == 0);
	assert(member.document && strcmp(tstring_cstr(member.exported->detail),
		"Int") == 0);
	main_document = tworkspace_update(&workspace, tstring_cstr(main_uri),
		"import bridge.tap as outer\nlet value = outer::nested::answer\n", 2);
	member_offset = (uint32_t)(strstr(
		tstring_cstr(main_document->frontend.document.text), "answer") -
		tstring_cstr(main_document->frontend.document.text));
	assert(tworkspace_resolve_member(&workspace, main_document,
		member_offset, &member));
	assert(member.exported && strcmp(tstring_cstr(member.exported->name),
		"answer") == 0);
	tworkspace_namespace standard;
	tworkspace_document *standard_document = tworkspace_update(&workspace,
		tstring_cstr(main_uri), "let value = math::sqrt(4)\n", 3);
	assert(tworkspace_resolve_namespace_at(&workspace, standard_document,
		(uint32_t)(strstr(tstring_cstr(standard_document->frontend.document.text),
		"math") - tstring_cstr(standard_document->frontend.document.text)), &standard));
	assert(standard.standard_package && strcmp(standard.standard_package, "math") == 0);
	tworkspace_free(&workspace);
	tstring_free(main_uri);
	unlink(module_path);
	unlink(bridge_path);
	rmdir(directory);
}

int main(void)
{
	test_document_and_lossless_tokens();
	test_precedence_and_postfix();
	test_tunnel_call_is_distinct_from_member_call();
	test_right_associativity();
	test_collections();
	test_recoverable_errors();
	test_basic_statements();
	test_control_flow_blocks();
	test_function_literals();
	test_module_tree();
	test_semantic_model();
	test_let_capture_diagnostic();
	test_editor_type_information();
	test_external_type_information();
	test_declaration_without_initializer();
	test_static_function_types();
	test_static_recursive_types();
	test_module_interface_and_standard_environment();
	test_workspace_import_resolution();
	return 0;
}
