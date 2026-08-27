#include "tapas/compile/ast.h"

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
	return arena && id < arena->node_count ? &arena->nodes[id] : NULL;
}

const tast_id *tast_get_children(const tast_arena *arena,
				 uint32_t start, uint32_t count)
{
	if (!arena || start > arena->child_count ||
	    count > arena->child_count - start)
		return NULL;
	return &arena->children[start];
}

tast_id tast_find_node_at(const tast_arena *arena, tast_id root,
			 uint32_t offset)
{
	const tast_node *root_node = tast_get(arena, root);
	if (!root_node || offset < root_node->span.start ||
	    offset > root_node->span.end)
		return TAST_INVALID_ID;
	tast_id best = root;
	uint32_t best_width = root_node->span.end - root_node->span.start;
	for (tast_id id = 0; id < arena->node_count; id++) {
		const tast_node *node = &arena->nodes[id];
		int contains = node->span.start <= offset && offset < node->span.end;
		if (!contains && node->span.start == node->span.end)
			contains = offset == node->span.start;
		if (!contains)
			continue;
		uint32_t width = node->span.end - node->span.start;
		if (width <= best_width) {
			best = id;
			best_width = width;
		}
	}
	return best;
}
