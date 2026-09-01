#include "tapas/compile/static_type.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static const char *const builtin_names[tstatic_builtin_count] = {
	"AnyType", "Nil", "Bool", "Int", "Float", "String", "List",
	"Pair", "Dictionary", "Iterator", "Function", "Library",
	"RealArray", "BoolArray", "Time", "Type"
};

void tstatic_type_arena_init(tstatic_type_arena *arena)
{
	*arena = (tstatic_type_arena){ 0 };
	for (uint32_t i = 0; i < tstatic_builtin_count; i++)
		tstatic_type_builtin_id(arena, (tstatic_builtin)i);
}

void tstatic_type_arena_free(tstatic_type_arena *arena)
{
	if (!arena) return;
	for (uint32_t i = 0; i < arena->field_count; i++)
		tstring_free(arena->fields[i].name);
	free(arena->types);
	free(arena->children);
	free(arena->fields);
	*arena = (tstatic_type_arena){ 0 };
}

static void reserve_types(tstatic_type_arena *arena, uint32_t extra)
{
	if (arena->type_count + extra <= arena->type_capacity) return;
	uint32_t capacity = arena->type_capacity ? arena->type_capacity * 2 : 32;
	while (capacity < arena->type_count + extra) capacity *= 2;
	tstatic_type *types = (tstatic_type *)realloc(
		arena->types, capacity * sizeof(*types));
	if (!types) abort();
	arena->types = types;
	arena->type_capacity = capacity;
}

static uint32_t append_children(tstatic_type_arena *arena,
				const tstatic_type_id *children, uint32_t count)
{
	uint32_t start = arena->child_count;
	if (!count) return start;
	if (arena->child_count + count > arena->child_capacity) {
		uint32_t capacity = arena->child_capacity ? arena->child_capacity * 2 : 64;
		while (capacity < arena->child_count + count) capacity *= 2;
		tstatic_type_id *items = (tstatic_type_id *)realloc(
			arena->children, capacity * sizeof(*items));
		if (!items) abort();
		arena->children = items;
		arena->child_capacity = capacity;
	}
	memcpy(arena->children + arena->child_count, children,
		count * sizeof(*children));
	arena->child_count += count;
	return start;
}

tstatic_type_id tstatic_type_builtin_id(tstatic_type_arena *arena,
						tstatic_builtin builtin)
{
	if ((uint32_t)builtin < arena->type_count &&
	    arena->types[builtin].kind == tstatic_type_builtin &&
	    arena->types[builtin].builtin == builtin)
		return (tstatic_type_id)builtin;
	reserve_types(arena, 1);
	tstatic_type_id id = arena->type_count++;
	arena->types[id] = (tstatic_type){
		.kind = tstatic_type_builtin, .builtin = builtin
	};
	return id;
}

tstatic_type_id tstatic_type_make(tstatic_type_arena *arena,
				  tstatic_type_kind kind,
				  const tstatic_type_id *children,
				  uint32_t child_count, int variadic)
{
	reserve_types(arena, 1);
	tstatic_type_id id = arena->type_count++;
	arena->types[id] = (tstatic_type){
		.kind = kind,
		.children = append_children(arena, children, child_count),
		.child_count = child_count,
		.variadic = variadic ? 1 : 0
	};
	return id;
}

tstatic_type_id tstatic_type_make_fields(tstatic_type_arena *arena,
					 const tstatic_field *fields,
					 uint32_t field_count)
{
	if (arena->field_count + field_count > arena->field_capacity) {
		uint32_t capacity = arena->field_capacity ? arena->field_capacity * 2 : 32;
		while (capacity < arena->field_count + field_count) capacity *= 2;
		tstatic_field *items = (tstatic_field *)realloc(
			arena->fields, capacity * sizeof(*items));
		if (!items) abort();
		arena->fields = items;
		arena->field_capacity = capacity;
	}
	uint32_t start = arena->field_count;
	for (uint32_t i = 0; i < field_count; i++) {
		arena->fields[arena->field_count++] = (tstatic_field){
			.name = tstring_dup(fields[i].name), .type = fields[i].type
		};
	}
	reserve_types(arena, 1);
	tstatic_type_id id = arena->type_count++;
	arena->types[id] = (tstatic_type){
		.kind = tstatic_type_fields, .fields = start, .field_count = field_count
	};
	return id;
}

tstatic_type_id tstatic_type_make_recursive(tstatic_type_arena *arena)
{
	return tstatic_type_make(arena, tstatic_type_recursive, NULL, 0, 0);
}

int tstatic_type_define_recursive(tstatic_type_arena *arena,
				  tstatic_type_id recursive,
				  tstatic_type_id body)
{
	tstatic_type *type = recursive < arena->type_count ?
		&arena->types[recursive] : NULL;
	if (!type || type->kind != tstatic_type_recursive ||
	    type->child_count || body >= arena->type_count ||
	    arena->types[body].kind == tstatic_type_recursive)
		return 0;
	type->children = append_children(arena, &body, 1);
	type->child_count = 1;
	return 1;
}

const tstatic_type *tstatic_type_get(const tstatic_type_arena *arena,
				    tstatic_type_id id)
{
	return arena && id < arena->type_count ? &arena->types[id] : NULL;
}

const tstatic_type_id *tstatic_type_children(const tstatic_type_arena *arena,
					      const tstatic_type *type)
{
	return arena && type && type->children + type->child_count <= arena->child_count ?
		arena->children + type->children : NULL;
}

const tstatic_field *tstatic_type_field_items(const tstatic_type_arena *arena,
					      const tstatic_type *type)
{
	return arena && type && type->fields + type->field_count <= arena->field_count ?
		arena->fields + type->fields : NULL;
}

typedef struct {
	tstatic_type_arena *arena;
	const char *text;
	size_t at;
	tstatic_type_resolver resolver;
	void *context;
} type_parser;

static void skip_space(type_parser *parser)
{
	for (;;) {
		while (isspace((unsigned char)parser->text[parser->at])) parser->at++;
		if (parser->text[parser->at] != '/' || parser->text[parser->at + 1] != '/')
			return;
		parser->at += 2;
		while (parser->text[parser->at] && parser->text[parser->at] != '\n')
			parser->at++;
	}
}

static int consume_text(type_parser *parser, const char *text)
{
	skip_space(parser);
	size_t length = strlen(text);
	if (strncmp(parser->text + parser->at, text, length) != 0) return 0;
	parser->at += length;
	return 1;
}

static int identifier_start(unsigned char c)
{
	return c == '_' || isalpha(c) || c >= 0x80;
}

static int identifier_continue(unsigned char c)
{
	return identifier_start(c) || isdigit(c);
}

static tstring *parse_name(type_parser *parser)
{
	skip_space(parser);
	size_t start = parser->at;
	if (!identifier_start((unsigned char)parser->text[parser->at])) return NULL;
	parser->at++;
	while (identifier_continue((unsigned char)parser->text[parser->at]))
		parser->at++;
	while (parser->text[parser->at] == ':' && parser->text[parser->at + 1] == ':') {
		parser->at += 2;
		if (!identifier_start((unsigned char)parser->text[parser->at])) return NULL;
		parser->at++;
		while (identifier_continue((unsigned char)parser->text[parser->at]))
			parser->at++;
	}
	return tstring_new_len(parser->text + start, parser->at - start);
}

static tstatic_type_id builtin_named(tstatic_type_arena *arena, const char *name)
{
	if (strncmp(name, "types::", 7) == 0) name += 7;
	else if (strstr(name, "::")) return TSTATIC_TYPE_UNKNOWN;
	for (uint32_t i = 0; i < tstatic_builtin_count; i++)
		if (strcmp(name, builtin_names[i]) == 0)
			return tstatic_type_builtin_id(arena, (tstatic_builtin)i);
	return TSTATIC_TYPE_UNKNOWN;
}

static tstatic_type_id parse_type(type_parser *parser);

static tstatic_type_id parse_application(type_parser *parser, tstring *name)
{
	tstatic_type_id *arguments = NULL;
	uint32_t count = 0, capacity = 0;
	int variadic = 0;
	if (!consume_text(parser, "]")) {
		if (consume_text(parser, "...")) {
			variadic = 1;
			if (!consume_text(parser, "]")) goto invalid;
		} else {
			for (;;) {
				tstatic_type_id argument = parse_type(parser);
				if (argument >= TSTATIC_TYPE_INVALID_NAME) goto invalid;
				if (count == capacity) {
					capacity = capacity ? capacity * 2 : 4;
					tstatic_type_id *next = (tstatic_type_id *)realloc(
						arguments, capacity * sizeof(*next));
					if (!next) abort();
					arguments = next;
				}
				arguments[count++] = argument;
				if (consume_text(parser, "]")) break;
				if (!consume_text(parser, ",")) goto invalid;
				if (consume_text(parser, "]")) break;
			}
		}
	}
	const char *constructor = tstring_cstr(name);
	if (strncmp(constructor, "types::", 7) == 0) constructor += 7;
	else {
		if (strstr(constructor, "::")) goto invalid;
		if (parser->resolver && parser->resolver(
		    parser->context, constructor) != TSTATIC_TYPE_UNKNOWN)
			goto invalid;
	}
	tstatic_type_id result = TSTATIC_TYPE_UNKNOWN;
	if (strcmp(constructor, "Function") == 0) {
		if (!consume_text(parser, "->")) goto invalid;
		tstatic_type_id return_type = parse_type(parser);
		if (return_type >= TSTATIC_TYPE_INVALID_NAME) goto invalid;
		tstatic_type_id *signature = (tstatic_type_id *)realloc(
			arguments, (count + 1) * sizeof(*signature));
		if (!signature) abort();
		arguments = signature;
		arguments[count] = return_type;
		result = tstatic_type_make(parser->arena, tstatic_type_function,
			arguments, count + 1, variadic);
	} else if (!variadic && strcmp(constructor, "List") == 0 && count == 1) {
		result = tstatic_type_make(parser->arena, tstatic_type_list,
			arguments, count, 0);
	} else if (!variadic && strcmp(constructor, "Pair") == 0 && count == 2) {
		result = tstatic_type_make(parser->arena, tstatic_type_pair,
			arguments, count, 0);
	} else if (!variadic && strcmp(constructor, "Dictionary") == 0 && count == 2) {
		result = tstatic_type_make(parser->arena, tstatic_type_dictionary,
			arguments, count, 0);
	} else if (!variadic && strcmp(constructor, "Union") == 0 && count >= 2) {
		result = tstatic_type_make(parser->arena, tstatic_type_union,
			arguments, count, 0);
	}
	free(arguments);
	return result;

invalid:
	free(arguments);
	return TSTATIC_TYPE_UNKNOWN;
}

static tstatic_type_id parse_type(type_parser *parser)
{
	tstring *name = parse_name(parser);
	if (!name) return TSTATIC_TYPE_UNKNOWN;
	skip_space(parser);
	tstatic_type_id result;
	if (parser->text[parser->at] == '[') {
		parser->at++;
		result = parse_application(parser, name);
	} else {
		result = parser->resolver ? parser->resolver(
			parser->context, tstring_cstr(name)) : TSTATIC_TYPE_UNKNOWN;
		if (result == TSTATIC_TYPE_UNKNOWN)
			result = builtin_named(parser->arena, tstring_cstr(name));
	}
	tstring_free(name);
	return result;
}

tstatic_type_id tstatic_type_parse(tstatic_type_arena *arena,
				   const char *text,
				   tstatic_type_resolver resolver,
				   void *context)
{
	if (!arena || !text) return TSTATIC_TYPE_UNKNOWN;
	type_parser parser = { arena, text, 0, resolver, context };
	tstatic_type_id result = parse_type(&parser);
	skip_space(&parser);
	return result < TSTATIC_TYPE_INVALID_NAME && !parser.text[parser.at] ?
		result : TSTATIC_TYPE_UNKNOWN;
}

typedef struct {
	tstatic_type_id left;
	tstatic_type_id right;
} tstatic_compare_pair;

typedef struct {
	tstatic_compare_pair *items;
	uint32_t count;
	uint32_t capacity;
} tstatic_compare_state;

static int compare_seen_or_add(tstatic_compare_state *state,
			       tstatic_type_id left, tstatic_type_id right)
{
	for (uint32_t i = 0; i < state->count; i++)
		if (state->items[i].left == left &&
		    state->items[i].right == right)
			return 1;
	if (state->count == state->capacity) {
		uint32_t capacity = state->capacity ? state->capacity * 2 : 16;
		tstatic_compare_pair *items = (tstatic_compare_pair *)realloc(
			state->items, capacity * sizeof(*items));
		if (!items) abort();
		state->items = items;
		state->capacity = capacity;
	}
	state->items[state->count++] = (tstatic_compare_pair){ left, right };
	return 0;
}

static int static_type_equal_graph(const tstatic_type_arena *arena,
				   tstatic_type_id left,
				   tstatic_type_id right,
				   tstatic_compare_state *state)
{
	if (left == right) return left != TSTATIC_TYPE_UNKNOWN;
	if (left == TSTATIC_TYPE_UNKNOWN || right == TSTATIC_TYPE_UNKNOWN)
		return 0;
	if (compare_seen_or_add(state, left, right)) return 1;
	const tstatic_type *a = tstatic_type_get(arena, left);
	const tstatic_type *b = tstatic_type_get(arena, right);
	if (!a || !b) return 0;
	const tstatic_type_id *ac = tstatic_type_children(arena, a);
	const tstatic_type_id *bc = tstatic_type_children(arena, b);
	if (a->kind == tstatic_type_recursive) {
		if (a->child_count != 1) return 0;
		return static_type_equal_graph(arena, ac[0], right, state);
	}
	if (b->kind == tstatic_type_recursive) {
		if (b->child_count != 1) return 0;
		return static_type_equal_graph(arena, left, bc[0], state);
	}
	if (a->kind != b->kind) return 0;
	if (a->kind == tstatic_type_builtin) return a->builtin == b->builtin;
	if (a->variadic != b->variadic || a->child_count != b->child_count ||
	    a->field_count != b->field_count) return 0;
	if (a->kind == tstatic_type_union) {
		for (uint32_t i = 0; i < a->child_count; i++) {
			int found = 0;
			for (uint32_t j = 0; j < b->child_count && !found; j++) {
				uint32_t saved = state->count;
				found = static_type_equal_graph(
					arena, ac[i], bc[j], state);
				if (!found) state->count = saved;
			}
			if (!found) return 0;
		}
		return 1;
	}
	for (uint32_t i = 0; i < a->child_count; i++)
		if (!static_type_equal_graph(arena, ac[i], bc[i], state)) return 0;
	const tstatic_field *af = tstatic_type_field_items(arena, a);
	const tstatic_field *bf = tstatic_type_field_items(arena, b);
	for (uint32_t i = 0; i < a->field_count; i++) {
		uint32_t j = 0;
		for (; j < b->field_count; j++) {
			uint32_t saved = state->count;
			if (tstring_eq(af[i].name, bf[j].name) &&
			    static_type_equal_graph(
				    arena, af[i].type, bf[j].type, state))
				break;
			state->count = saved;
		}
		if (j == b->field_count) return 0;
	}
	return 1;
}

int tstatic_type_equal(const tstatic_type_arena *arena,
			       tstatic_type_id left, tstatic_type_id right)
{
	tstatic_compare_state state = { 0 };
	int equal = static_type_equal_graph(arena, left, right, &state);
	free(state.items);
	return equal;
}

static int static_type_assignable_graph(const tstatic_type_arena *arena,
					tstatic_type_id actual,
					tstatic_type_id target,
					tstatic_compare_state *state)
{
	if (actual == TSTATIC_TYPE_UNKNOWN || target == TSTATIC_TYPE_UNKNOWN)
		return 1;
	if (tstatic_type_equal(arena, actual, target) ||
	    target == tstatic_builtin_any) return 1;
	if (compare_seen_or_add(state, actual, target)) return 1;
	const tstatic_type *a = tstatic_type_get(arena, actual);
	const tstatic_type *b = tstatic_type_get(arena, target);
	if (!a || !b) return 1;
	const tstatic_type_id *ac = tstatic_type_children(arena, a);
	const tstatic_type_id *bc = tstatic_type_children(arena, b);
	if (a->kind == tstatic_type_recursive)
		return a->child_count == 1 &&
			static_type_assignable_graph(arena, ac[0], target, state);
	if (b->kind == tstatic_type_recursive)
		return b->child_count == 1 &&
			static_type_assignable_graph(arena, actual, bc[0], state);
	if (a->kind == tstatic_type_union) {
		for (uint32_t i = 0; i < a->child_count; i++)
			if (!static_type_assignable_graph(
				arena, ac[i], target, state)) return 0;
		return 1;
	}
	if (b->kind == tstatic_type_union) {
		for (uint32_t i = 0; i < b->child_count; i++) {
			uint32_t saved = state->count;
			if (static_type_assignable_graph(
				arena, actual, bc[i], state)) return 1;
			state->count = saved;
		}
		return 0;
	}
	if ((a->kind == tstatic_type_list && target == tstatic_builtin_list) ||
	    (a->kind == tstatic_type_pair && target == tstatic_builtin_pair) ||
	    (a->kind == tstatic_type_dictionary && target == tstatic_builtin_dictionary) ||
	    (a->kind == tstatic_type_function && target == tstatic_builtin_function) ||
	    (a->kind == tstatic_type_fields && target == tstatic_builtin_dictionary))
		return 1;
	if (a->kind == tstatic_type_function && b->kind == tstatic_type_function) {
		if (a->variadic != b->variadic || a->child_count != b->child_count)
			return 0;
		uint32_t parameters = a->child_count ? a->child_count - 1 : 0;
		for (uint32_t i = 0; i < parameters; i++)
			if (!static_type_assignable_graph(
				arena, bc[i], ac[i], state)) return 0;
		return a->child_count && static_type_assignable_graph(
			arena, ac[parameters], bc[parameters], state);
	}
	if (a->kind == tstatic_type_fields && b->kind == tstatic_type_fields) {
		const tstatic_field *af = tstatic_type_field_items(arena, a);
		const tstatic_field *bf = tstatic_type_field_items(arena, b);
		for (uint32_t i = 0; i < b->field_count; i++) {
			uint32_t j = 0;
			for (; j < a->field_count; j++)
				if (tstring_eq(bf[i].name, af[j].name) &&
				    tstatic_type_equal(arena, bf[i].type, af[j].type)) break;
			if (j == a->field_count) return 0;
		}
		return 1;
	}
	return 0;
}

int tstatic_type_assignable(const tstatic_type_arena *arena,
				    tstatic_type_id actual,
				    tstatic_type_id target)
{
	tstatic_compare_state state = { 0 };
	int assignable = static_type_assignable_graph(
		arena, actual, target, &state);
	free(state.items);
	return assignable;
}

static void format_into(const tstatic_type_arena *arena,
			tstatic_type_id id, tstring *out)
{
	if (id == TSTATIC_TYPE_UNKNOWN) { tstring_append(out, "?"); return; }
	const tstatic_type *type = tstatic_type_get(arena, id);
	if (!type) { tstring_append(out, "?"); return; }
	if (type->kind == tstatic_type_builtin) {
		tstring_append(out, builtin_names[type->builtin]);
		return;
	}
	if (type->kind == tstatic_type_recursive) {
		tstring_append(out, "Recursive");
		return;
	}
	const tstatic_type_id *children = tstatic_type_children(arena, type);
	const char *name = type->kind == tstatic_type_list ? "List" :
		type->kind == tstatic_type_pair ? "Pair" :
		type->kind == tstatic_type_dictionary ? "Dictionary" :
		type->kind == tstatic_type_union ? "Union" : NULL;
	if (type->kind == tstatic_type_function) {
		tstring_append(out, "Function[");
		if (type->variadic) tstring_append(out, "...");
		else for (uint32_t i = 0; i + 1 < type->child_count; i++) {
			if (i) tstring_append(out, ", ");
			format_into(arena, children[i], out);
		}
		tstring_append(out, "] -> ");
		format_into(arena, type->child_count ?
			children[type->child_count - 1] : TSTATIC_TYPE_UNKNOWN, out);
		return;
	}
	if (type->kind == tstatic_type_fields) {
		tstring_append(out, "{");
		const tstatic_field *fields = tstatic_type_field_items(arena, type);
		for (uint32_t i = 0; i < type->field_count; i++) {
			if (i) tstring_append(out, ", ");
			tstring_append_ts(out, fields[i].name);
			tstring_append(out, ": ");
			format_into(arena, fields[i].type, out);
		}
		tstring_append(out, "}");
		return;
	}
	tstring_append(out, name ? name : "?");
	tstring_append_c(out, '[');
	for (uint32_t i = 0; i < type->child_count; i++) {
		if (i) tstring_append(out, ", ");
		format_into(arena, children[i], out);
	}
	tstring_append_c(out, ']');
}

tstring *tstatic_type_format(const tstatic_type_arena *arena,
			     tstatic_type_id id)
{
	tstring *result = tstring_new_empty();
	format_into(arena, id, result);
	return result;
}
