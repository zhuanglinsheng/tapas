#include "tapas/compile/static_type.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

void tstatic_type_arena_init(tstatic_type_arena *arena)
{
	*arena = (tstatic_type_arena){ 0 };
	for (uint32_t i = 0; i < tbuiltin_count; i++)
		tstatic_type_builtin_id(arena, (tbuiltin_id)i);
}

void tstatic_type_arena_free(tstatic_type_arena *arena)
{
	if (!arena) return;
	for (uint32_t i = 0; i < arena->type_count; i++) {
		tstring_free(arena->types[i].value_reference);
		tstring_free(arena->types[i].display_name);
	}
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
	uint32_t source = UINT32_MAX;
	uintptr_t source_address = (uintptr_t)children;
	uintptr_t children_begin = (uintptr_t)arena->children;
	uintptr_t children_end = children_begin +
		arena->child_count * sizeof(*arena->children);
	if (arena->children && source_address >= children_begin &&
	    source_address < children_end)
		source = (uint32_t)(children - arena->children);
	if (arena->child_count + count > arena->child_capacity) {
		uint32_t capacity = arena->child_capacity ? arena->child_capacity * 2 : 64;
		while (capacity < arena->child_count + count) capacity *= 2;
		tstatic_type_id *items = (tstatic_type_id *)realloc(
			arena->children, capacity * sizeof(*items));
		if (!items) abort();
		arena->children = items;
		arena->child_capacity = capacity;
	}
	if (source != UINT32_MAX) children = arena->children + source;
	memmove(arena->children + arena->child_count, children,
		count * sizeof(*children));
	arena->child_count += count;
	return start;
}

tstatic_type_id tstatic_type_builtin_id(tstatic_type_arena *arena,
						tbuiltin_id builtin)
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
			.name = tstring_dup(fields[i].name), .type = fields[i].type,
			.optional = fields[i].optional
		};
	}
	reserve_types(arena, 1);
	tstatic_type_id id = arena->type_count++;
	arena->types[id] = (tstatic_type){
		.kind = tstatic_type_fields, .fields = start, .field_count = field_count
	};
	return id;
}

tstatic_type_id tstatic_type_make_enum(tstatic_type_arena *arena,
				       tstring *const *members,
				       uint32_t member_count)
{
	if (!arena || !members || member_count == 0)
		return TSTATIC_TYPE_UNKNOWN;
	tstatic_field *items = (tstatic_field *)calloc(member_count, sizeof(*items));
	if (!items) abort();
	for (uint32_t i = 0; i < member_count; i++) {
		items[i].name = members[i];
		items[i].type = tbuiltin_string;
	}
	tstatic_type_id id = tstatic_type_make_fields(arena, items, member_count);
	free(items);
	arena->types[id].kind = tstatic_type_enum;
	return id;
}

tstatic_type_id tstatic_type_make_recursive(tstatic_type_arena *arena)
{
	return tstatic_type_make(arena, tstatic_type_recursive, nullptr, 0, 0);
}

int tstatic_type_define_recursive(tstatic_type_arena *arena,
				  tstatic_type_id recursive,
				  tstatic_type_id body)
{
	tstatic_type *type = recursive < arena->type_count ?
		&arena->types[recursive] : nullptr;
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
	return arena && id < arena->type_count ? &arena->types[id] : nullptr;
}

const tstatic_type_id *tstatic_type_children(const tstatic_type_arena *arena,
					      const tstatic_type *type)
{
	return arena && type && type->children + type->child_count <= arena->child_count ?
		arena->children + type->children : nullptr;
}

const tstatic_field *tstatic_type_field_items(const tstatic_type_arena *arena,
					      const tstatic_type *type)
{
	return arena && type && type->fields + type->field_count <= arena->field_count ?
		arena->fields + type->fields : nullptr;
}

uint32_t tstatic_type_enum_member_count(const tstatic_type *type)
{
	return type && type->kind == tstatic_type_enum ? type->field_count : 0;
}

const tstring *tstatic_type_enum_member_at(const tstatic_type_arena *arena,
					   const tstatic_type *type,
					   uint32_t index)
{
	const tstatic_field *members = tstatic_type_field_items(arena, type);
	return type && type->kind == tstatic_type_enum && members &&
	       index < type->field_count ? members[index].name : nullptr;
}

int tstatic_type_enum_contains(const tstatic_type_arena *arena,
			       const tstatic_type *type, const tstring *member)
{
	if (!type || type->kind != tstatic_type_enum || !member) return 0;
	for (uint32_t i = 0; i < type->field_count; i++)
		if (tstring_eq(tstatic_type_enum_member_at(arena, type, i), member))
			return 1;
	return 0;
}

typedef struct {
	tstatic_type_arena *arena;
	const char *text;
	size_t at;
	tstatic_type_resolver resolver;
	void *context;
	tstatic_value_type_resolver value_resolver;
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
	if (!identifier_start((unsigned char)parser->text[parser->at])) return nullptr;
	parser->at++;
	while (identifier_continue((unsigned char)parser->text[parser->at]))
		parser->at++;
	while (parser->text[parser->at] == ':' && parser->text[parser->at + 1] == ':') {
		parser->at += 2;
		if (!identifier_start((unsigned char)parser->text[parser->at])) return nullptr;
		parser->at++;
		while (identifier_continue((unsigned char)parser->text[parser->at]))
			parser->at++;
	}
	return tstring_new_len(parser->text + start, parser->at - start);
}

tstatic_type_id tstatic_type_builtin_named(tstatic_type_arena *arena,
					   const char *name)
{
	if (strncmp(name, "types::", 7) == 0) name += 7;
	else if (strstr(name, "::")) return TSTATIC_TYPE_UNKNOWN;
	for (uint32_t i = 0; i < tbuiltin_count; i++)
		if (strcmp(name, tbuiltin_name((tbuiltin_id)i)) == 0)
			return tstatic_type_builtin_id(arena, (tbuiltin_id)i);
	return TSTATIC_TYPE_UNKNOWN;
}

static tstatic_type_id parse_type(type_parser *parser);

/* The signature determines parameter categories and optional syntax; never
 * guess a category from the argument's spelling. Current constructors accept
 * only Type parameters. Value-bearing signatures need a static value resolver. */
typedef struct {
	const char *name;
	tstatic_type_kind kind;
	uint32_t minimum;
	uint32_t maximum;
	int accepts_variadic;
	int has_result;
} type_application_signature;

static const type_application_signature *application_signature(const char *name)
{
	static const type_application_signature signatures[] = {
		{ "PointsOf", tstatic_type_points, 1, 1, 0, 0 },
		{ "RangeOf", tstatic_type_range, 1, 1, 0, 0 },
		{ "List", tstatic_type_list, 1, 1, 0, 0 },
		{ "Iterator", tstatic_type_iterator, 1, 1, 0, 0 },
		{ "Pair", tstatic_type_pair, 2, 2, 0, 0 },
		{ "Dictionary", tstatic_type_dictionary, 2, 2, 0, 0 },
		{ "Union", tstatic_type_union, 2, UINT32_MAX, 0, 0 },
		{ "Function", tstatic_type_function, 0, UINT32_MAX, 1, 1 },
		{ "Rule", tstatic_type_rule, 0, UINT32_MAX, 0, 0 },
		{ "RuleInstance", tstatic_type_rule_instance, 0, UINT32_MAX, 0, 0 },
	};
	for (size_t i = 0; i < sizeof(signatures) / sizeof(signatures[0]); i++)
		if (strcmp(name, signatures[i].name) == 0) return &signatures[i];
	return nullptr;
}

static int application_end(type_parser *parser)
{
	if (consume_text(parser, "]")) return 1;
	/* Explicit empty Value section for a Type-only constructor. */
	if (consume_text(parser, ";"))
		return consume_text(parser, "]") ? 1 : -1;
	return 0;
}

static tstatic_type_id parse_application(type_parser *parser, tstring *name)
{
	const char *constructor = tstring_cstr(name);
	if (strncmp(constructor, "rules::", 7) == 0) constructor += 7;
	else if (strncmp(constructor, "types::", 7) == 0) constructor += 7;
	else {
		if (strstr(constructor, "::")) return TSTATIC_TYPE_UNKNOWN;
		if (parser->resolver && parser->resolver(
		    parser->context, constructor) != TSTATIC_TYPE_UNKNOWN)
			return TSTATIC_TYPE_UNKNOWN;
	}
	if (strcmp(constructor, "InstanceOf") == 0) {
		consume_text(parser, ";");
		tstring *reference = parse_name(parser);
		if (!reference) return TSTATIC_TYPE_UNKNOWN;
		consume_text(parser, ",");
		if (!consume_text(parser, "]")) { tstring_free(reference); return TSTATIC_TYPE_UNKNOWN; }
		tstatic_type_id source = parser->value_resolver ?
			parser->value_resolver(parser->context, tstring_cstr(reference)) : TSTATIC_TYPE_UNKNOWN;
		const tstatic_type *rule = tstatic_type_get(parser->arena, source);
		if (source == TSTATIC_TYPE_INVALID_NAME || (rule && rule->kind != tstatic_type_rule &&
		    !(rule->kind == tstatic_type_builtin && rule->builtin == tbuiltin_rule))) {
			tstring_free(reference); return TSTATIC_TYPE_UNKNOWN;
		}
		tstatic_type_id instance = tstatic_type_make(parser->arena, tstatic_type_instance_of,
			rule ? tstatic_type_children(parser->arena, rule) : nullptr,
			rule ? rule->child_count : 0, 0);
		parser->arena->types[instance].value_reference = reference;
		return instance;
	}
	const type_application_signature *signature = application_signature(constructor);
	if (!signature) return TSTATIC_TYPE_UNKNOWN;
	tstatic_type_id *arguments = nullptr;
	uint32_t count = 0, capacity = 0;
	int variadic = 0;
	int end = application_end(parser);
	if (end < 0) goto invalid;
	if (!end) {
		if (consume_text(parser, "...")) {
			variadic = 1;
			if (application_end(parser) != 1) goto invalid;
		} else {
			for (;;) {
				tstatic_type_id argument = parse_type(parser);
				if (argument == TSTATIC_TYPE_INVALID_NAME) goto invalid;
				if (count == capacity) {
					capacity = capacity ? capacity * 2 : 4;
					tstatic_type_id *next = (tstatic_type_id *)realloc(
						arguments, capacity * sizeof(*next));
					if (!next) abort();
					arguments = next;
				}
				arguments[count++] = argument;
				end = application_end(parser);
				if (end < 0) goto invalid;
				if (end) break;
				if (!consume_text(parser, ",")) goto invalid;
				end = application_end(parser);
				if (end < 0) goto invalid;
				if (end) break;
			}
		}
	}
	if (count < signature->minimum || count > signature->maximum ||
	    (variadic && !signature->accepts_variadic)) goto invalid;
	tstatic_type_id result = TSTATIC_TYPE_UNKNOWN;
	if (signature->has_result) {
		if (!consume_text(parser, "->")) goto invalid;
		tstatic_type_id return_type = parse_type(parser);
		if (return_type == TSTATIC_TYPE_INVALID_NAME) goto invalid;
		tstatic_type_id *children = (tstatic_type_id *)realloc(
			arguments, (count + 1) * sizeof(*children));
		if (!children) abort();
		arguments = children;
		arguments[count] = return_type;
		result = tstatic_type_make(parser->arena, tstatic_type_function,
			arguments, count + 1, variadic);
	} else {
		result = tstatic_type_make(parser->arena, signature->kind,
			arguments, count, 0);
	}
	free(arguments);
	return result;

invalid:
	free(arguments);
	return TSTATIC_TYPE_UNKNOWN;
}

static tstatic_type_id parse_primary_type(type_parser *parser)
{
    if (consume_text(parser, "{")) {
        tstatic_field *fields = nullptr;
        uint32_t count = 0;
        tstatic_type_id result = TSTATIC_TYPE_UNKNOWN;
        if (!consume_text(parser, "}")) for (;;) {
            tstring *name = parse_name(parser);
            if (!name) goto fields_done;
            int optional = consume_text(parser, "?");
            if (!consume_text(parser, ":")) { tstring_free(name); goto fields_done; }
            tstatic_type_id field_type = parse_type(parser);
            if (field_type == TSTATIC_TYPE_INVALID_NAME) { tstring_free(name); goto fields_done; }
            for (uint32_t i=0;i<count;i++) if (tstring_eq_cstr(fields[i].name,tstring_cstr(name))) {
                tstring_free(name); goto fields_done;
            }
            tstatic_field *next = realloc(fields,(count+1)*sizeof(*fields));
            if (!next) abort();
            fields=next; fields[count++]=(tstatic_field){.name=name,.type=field_type,.optional=optional};
            if (consume_text(parser,"}")) break;
            if (!consume_text(parser,",")) goto fields_done;
            if (consume_text(parser,"}")) break;
        }
        result = tstatic_type_make_fields(parser->arena,fields,count);
fields_done:
        for (uint32_t i=0;i<count;i++) tstring_free(fields[i].name);
        free(fields);
        return result;
    }
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
			result = tstatic_type_builtin_named(
				parser->arena, tstring_cstr(name));
		if (result == TSTATIC_TYPE_UNKNOWN &&
		    !tstring_eq_cstr(name, "Unknown"))
			result = TSTATIC_TYPE_INVALID_NAME;
	}
	tstring_free(name);
	return result;
}

static tstatic_type_id parse_type(type_parser *parser)
{
	tstatic_type_id first = parse_primary_type(parser);
	if (first >= TSTATIC_TYPE_INVALID_NAME)
		return first;
	if (!consume_text(parser, "|"))
		return first;
	tstatic_type_id *members = nullptr;
	uint32_t count = 0, capacity = 0;
	for (;;) {
		if (count == capacity) {
			capacity = capacity ? capacity * 2 : 4;
			tstatic_type_id *next = (tstatic_type_id *)realloc(
				members, capacity * sizeof(*next));
			if (!next) abort();
			members = next;
		}
		members[count++] = first;
		skip_space(parser);
		if (!identifier_start((unsigned char)parser->text[parser->at])) {
			free(members);
			return TSTATIC_TYPE_INVALID_NAME;
		}
		first = parse_primary_type(parser);
		if (first >= TSTATIC_TYPE_INVALID_NAME) {
			free(members);
			return first;
		}
		if (!consume_text(parser, "|"))
			break;
	}
	members[count++] = first;
	tstatic_type_id result = tstatic_type_make(
		parser->arena, tstatic_type_union, members, count, 0);
	free(members);
	return result;
}

tstatic_type_id tstatic_type_parse(tstatic_type_arena *arena,
				   const char *text,
				   tstatic_type_resolver resolver,
				   void *context)
{
	return tstatic_type_parse_with_values(arena, text, resolver, nullptr, context);
}

tstatic_type_id tstatic_type_parse_with_values(tstatic_type_arena *arena,
	const char *text, tstatic_type_resolver resolver,
	tstatic_value_type_resolver value_resolver, void *context)
{
	if (!arena || !text) return TSTATIC_TYPE_UNKNOWN;
	type_parser parser = { arena, text, 0, resolver, context, value_resolver };
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
	if (a->kind == tstatic_type_instance_of &&
	    !tstring_eq(a->value_reference, b->value_reference)) return 0;
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
			    af[i].optional == bf[j].optional &&
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
	    target == tbuiltin_any) return 1;
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
    if (a->kind == tstatic_type_builtin && b->kind == tstatic_type_builtin) {
        if (b->builtin == tbuiltin_rule_term && (a->builtin == tbuiltin_rule_parameter || a->builtin == tbuiltin_rule_capture)) return 1;
        if (b->builtin == tbuiltin_rule_item && (a->builtin == tbuiltin_rule_condition || a->builtin == tbuiltin_rule_requirement)) return 1;
    }
    if (a->kind == tstatic_type_builtin &&
        ((a->builtin == tbuiltin_points && b->kind == tstatic_type_points) ||
         (a->builtin == tbuiltin_range && b->kind == tstatic_type_range))) return 1;
	/* Value identity is enforced after binding the actual Rule at runtime. */
	if (b->kind == tstatic_type_instance_of)
		return a->kind == tstatic_type_instance_of ||
			a->kind == tstatic_type_rule_instance ||
			(a->kind == tstatic_type_builtin && a->builtin == tbuiltin_rule_instance);
	if (a->kind == tstatic_type_instance_of && b->kind == tstatic_type_rule_instance) {
		/* Unknown Rule signatures remain permissive until runtime binding. */
		if (!a->child_count) return 1;
		if (a->child_count != b->child_count) return 0;
		for (uint32_t i = 0; i < a->child_count; i++)
			if (!tstatic_type_equal(arena, ac[i], bc[i])) return 0;
		return 1;
	}
	if (a->kind == tstatic_type_enum && b->kind == tstatic_type_enum) {
		for (uint32_t i = 0; i < a->field_count; i++)
			if (!tstatic_type_enum_contains(arena, b,
				tstatic_type_enum_member_at(arena, a, i))) return 0;
		return 1;
	}
	if (a->kind == tstatic_type_enum && b->kind == tstatic_type_builtin &&
	    b->builtin == tbuiltin_string)
		return 1;
	if (b->kind == tstatic_type_builtin &&
	    (b->builtin == tbuiltin_indexable ||
	     b->builtin == tbuiltin_index_settable ||
	     b->builtin == tbuiltin_appendable ||
	     b->builtin == tbuiltin_deletable ||
	     b->builtin == tbuiltin_contains ||
	     b->builtin == tbuiltin_iterable)) {
		tbuiltin_id builtin = a->kind == tstatic_type_builtin ?
			a->builtin : a->kind == tstatic_type_list ? tbuiltin_list :
			a->kind == tstatic_type_enum ? tbuiltin_string :
			a->kind == tstatic_type_points ? tbuiltin_points :
			a->kind == tstatic_type_range ? tbuiltin_range :
			a->kind == tstatic_type_iterator ? tbuiltin_iterator :
			a->kind == tstatic_type_pair ? tbuiltin_pair :
			a->kind == tstatic_type_dictionary ||
			a->kind == tstatic_type_fields ? tbuiltin_dictionary :
			tbuiltin_count;
		if (b->builtin == tbuiltin_indexable)
			return builtin == tbuiltin_points || builtin == tbuiltin_range || builtin == tbuiltin_string ||
			       builtin == tbuiltin_list ||
			       builtin == tbuiltin_pair ||
			       builtin == tbuiltin_dictionary ||
			       builtin == tbuiltin_library ||
			       builtin == tbuiltin_real_array ||
			       builtin == tbuiltin_bool_array ||
			       builtin == tbuiltin_type;
		if (b->builtin == tbuiltin_index_settable)
			return builtin == tbuiltin_string ||
			       builtin == tbuiltin_list ||
			       builtin == tbuiltin_pair ||
			       builtin == tbuiltin_dictionary ||
			       builtin == tbuiltin_real_array ||
			       builtin == tbuiltin_bool_array;
		if (b->builtin == tbuiltin_appendable)
			return builtin == tbuiltin_string ||
			       builtin == tbuiltin_list ||
			       builtin == tbuiltin_dictionary;
		if (b->builtin == tbuiltin_deletable)
			return builtin == tbuiltin_list ||
			       builtin == tbuiltin_dictionary;
		if (b->builtin == tbuiltin_contains)
			return builtin == tbuiltin_points || builtin == tbuiltin_range || builtin == tbuiltin_list ||
			       builtin == tbuiltin_dictionary ||
			       builtin == tbuiltin_iterator;
		return builtin == tbuiltin_list ||
		       builtin == tbuiltin_iterator ||
		       builtin == tbuiltin_type;
	}
	if ((a->kind == tstatic_type_points && target == tbuiltin_points) ||
	    (a->kind == tstatic_type_range && target == tbuiltin_range) ||
	    (a->kind == tstatic_type_list && target == tbuiltin_list) ||
	    (a->kind == tstatic_type_iterator && target == tbuiltin_iterator) ||
	    (a->kind == tstatic_type_pair && target == tbuiltin_pair) ||
	    (a->kind == tstatic_type_dictionary && target == tbuiltin_dictionary) ||
	    (a->kind == tstatic_type_function && target == tbuiltin_function) ||
	    (a->kind == tstatic_type_rule && target == tbuiltin_rule) ||
	    ((a->kind == tstatic_type_rule_instance || a->kind == tstatic_type_instance_of) && target == tbuiltin_rule_instance) ||
	    (a->kind == tstatic_type_fields && target == tbuiltin_dictionary))
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
				    (bf[i].optional || !af[j].optional) &&
				    (bf[i].type == TSTATIC_TYPE_UNKNOWN || af[j].type == TSTATIC_TYPE_UNKNOWN ||
                     tstatic_type_equal(arena, bf[i].type, af[j].type))) break;
			if (j == a->field_count && !bf[i].optional) return 0;
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

/* Do not name the shared definition: two annotations may deliberately use
 * different aliases for the same structural Type. Only this occurrence is named. */
tstatic_type_id tstatic_type_with_name(tstatic_type_arena *arena,
		tstatic_type_id id, const char *name)
{
	const tstatic_type *type = tstatic_type_get(arena, id);
	if (!type || type->kind == tstatic_type_builtin) return id;
	tstatic_type copy = *type;
	copy.display_name = tstring_new(name);
	copy.display_definition = id;
	copy.value_reference = type->value_reference ? tstring_dup(type->value_reference) : nullptr;
	if (type->kind == tstatic_type_recursive) {
		/* Follow the original placeholder, including its later definition. */
		copy.children = append_children(arena, &id, 1);
		copy.child_count = 1;
	}
	reserve_types(arena, 1);
	tstatic_type_id result = arena->type_count++;
	arena->types[result] = copy;
	return result;
}

static void format_into(const tstatic_type_arena *arena,
			tstatic_type_id id, tstring *out, int display)
{
	if (id == TSTATIC_TYPE_UNKNOWN) { tstring_append(out, "?"); return; }
	const tstatic_type *type = tstatic_type_get(arena, id);
	if (!type) { tstring_append(out, "?"); return; }
	if (display && type->display_name) {
		tstring_append_ts(out, type->display_name);
		return;
	}
	if (type->kind == tstatic_type_instance_of) {
		tstring_append(out, "InstanceOf[");
		tstring_append_ts(out, type->value_reference);
		tstring_append_c(out, ']');
		return;
	}
	if (type->kind == tstatic_type_builtin) {
		tstring_append(out, tbuiltin_name(type->builtin));
		return;
	}
	if (type->kind == tstatic_type_recursive) {
		tstring_append(out, "Recursive");
		return;
	}
	const tstatic_type_id *children = tstatic_type_children(arena, type);
	const char *name = type->kind == tstatic_type_points ? "PointsOf" :
		type->kind == tstatic_type_range ? "RangeOf" :
		type->kind == tstatic_type_list ? "List" :
		type->kind == tstatic_type_iterator ? "Iterator" :
		type->kind == tstatic_type_pair ? "Pair" :
		type->kind == tstatic_type_dictionary ? "Dictionary" :
		type->kind == tstatic_type_union ? "Union" :
		type->kind == tstatic_type_rule ? "Rule" :
		type->kind == tstatic_type_rule_instance ? "RuleInstance" : nullptr;
	if (type->kind == tstatic_type_function) {
		tstring_append(out, "Function[");
		if (type->variadic) tstring_append(out, "...");
		else for (uint32_t i = 0; i + 1 < type->child_count; i++) {
			if (i) tstring_append(out, ", ");
			format_into(arena, children[i], out, display);
		}
		tstring_append(out, "] -> ");
		format_into(arena, type->child_count ?
			children[type->child_count - 1] : TSTATIC_TYPE_UNKNOWN, out, display);
		return;
	}
	if (type->kind == tstatic_type_fields) {
		tstring_append(out, "{");
		const tstatic_field *fields = tstatic_type_field_items(arena, type);
		for (uint32_t i = 0; i < type->field_count; i++) {
			if (i) tstring_append(out, ", ");
			tstring_append_ts(out, fields[i].name);
			if (fields[i].optional) tstring_append_c(out, '?');
			tstring_append(out, ": ");
			format_into(arena, fields[i].type, out, display);
		}
		tstring_append(out, "}");
		return;
	}
	if (type->kind == tstatic_type_enum) {
		tstring_append(out, "Enum[");
		for (uint32_t i = 0; i < type->field_count; i++) {
			if (i) tstring_append(out, ", ");
			tstring_append_c(out, '\'');
			tstring_append_ts(out,
				tstatic_type_enum_member_at(arena, type, i));
			tstring_append_c(out, '\'');
		}
		tstring_append_c(out, ']');
		return;
	}
	tstring_append(out, name ? name : "?");
	tstring_append_c(out, '[');
	for (uint32_t i = 0; i < type->child_count; i++) {
		if (i) tstring_append(out, ", ");
		format_into(arena, children[i], out, display);
	}
	tstring_append_c(out, ']');
}

tstring *tstatic_type_format(const tstatic_type_arena *arena,
			     tstatic_type_id id)
{
	tstring *result = tstring_new_empty();
	format_into(arena, id, result, 0);
	return result;
}

tstring *tstatic_type_display(const tstatic_type_arena *arena, tstatic_type_id id)
{
	tstring *result = tstring_new_empty();
	format_into(arena, id, result, 1);
	return result;
}
