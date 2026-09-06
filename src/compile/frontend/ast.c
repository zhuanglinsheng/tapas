#include "compile/frontend/ast.h"
#include "tapas/dsa/tstring.h"

#include <stdlib.h>

void tast_arena_init(tast_arena *arena)
{
	*arena = (tast_arena){ 0 };
}

void tast_arena_free(tast_arena *arena)
{
	if (!arena)
		return;
	free(arena->nodes);
	free(arena->children);
	*arena = (tast_arena){ 0 };
}

tast_id tast_arena_add(tast_arena *arena, tast_node node)
{
	if (arena->node_count >= arena->node_capacity) {
		uint32_t capacity = arena->node_capacity ? arena->node_capacity * 2 : 64;
		tast_node *nodes = (tast_node *)realloc(
			arena->nodes, capacity * sizeof(tast_node));
		if (!nodes)
			abort();
		arena->nodes = nodes;
		arena->node_capacity = capacity;
	}
	tast_id id = arena->node_count++;
	arena->nodes[id] = node;
	return id;
}

uint32_t tast_arena_add_children(tast_arena *arena,
				 const tast_id *children, uint32_t count)
{
	uint32_t start = arena->child_count;
	if (count == 0)
		return start;
	uint32_t required = arena->child_count + count;
	if (required > arena->child_capacity) {
		uint32_t capacity = arena->child_capacity ? arena->child_capacity : 64;
		while (capacity < required)
			capacity *= 2;
		tast_id *items = (tast_id *)realloc(
			arena->children, capacity * sizeof(tast_id));
		if (!items)
			abort();
		arena->children = items;
		arena->child_capacity = capacity;
	}
	for (uint32_t i = 0; i < count; i++)
		arena->children[arena->child_count++] = children[i];
	return start;
}

const tast_node *tast_get(const tast_arena *arena, tast_id id)
{
	return arena && id < arena->node_count ? &arena->nodes[id] : nullptr;
}

const tast_id *tast_get_children(const tast_arena *arena,
				 uint32_t start, uint32_t count)
{
	if (!arena || start > arena->child_count ||
	    count > arena->child_count - start)
		return nullptr;
	return &arena->children[start];
}

tstring *tast_string_contents_value(const tsource_document *document,
				    tsource_span contents)
{
	if (!document || contents.end < contents.start)
		return nullptr;
	tstring *encoded = tsource_document_slice(document, contents);
	tstring *decoded = tstring_new_cap(tstring_len(encoded) + 1);
	for (size_t i = 0; i < tstring_len(encoded); i++) {
		char value = tstring_at(encoded, i);
		if (value == '\\') {
			if (++i >= tstring_len(encoded) ||
			    !tsyntax_decode_escape(
				    (unsigned char)tstring_at(encoded, i), &value)) {
				tstring_free(encoded);
				tstring_free(decoded);
				return nullptr;
			}
		}
		tstring_append_c(decoded, value);
	}
	tstring_free(encoded);
	return decoded;
}

tstring *tast_string_value(const tsource_document *document,
			   const tast_node *node)
{
	if (!node || node->kind != tast_string ||
	    node->span.end < node->span.start + 2)
		return nullptr;
	return tast_string_contents_value(document, (tsource_span){
		node->span.start + 1, node->span.end - 1
	});
}

tstring *tast_scoped_member_name(const tsource_document *document,
				 const tast_arena *arena,
				 const tast_node *member,
				 const char *scope)
{
	if (!member || member->kind != tast_member ||
	    member->member.op != tsyntax_scope)
		return nullptr;
	const tast_node *receiver = tast_get(arena, member->member.receiver);
	if (!receiver || receiver->kind != tast_name)
		return nullptr;
	tstring *name = tsource_document_slice(document, receiver->span);
	int matches = tstring_eq_cstr(name, scope);
	tstring_free(name);
	return matches ? tsource_document_slice(document, member->member.name) : nullptr;
}
