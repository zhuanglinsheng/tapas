#include "workspace_internal.h"
#include "tapas/dsa/tstring.h"

#include <stdlib.h>

typedef struct {
	const tworkspace_document *source;
	tstring *target_uri;
	tstring *name;
	tsource_span span;
} reference_entry;

struct tworkspace_reference_index {
	reference_entry *items;
	uint32_t count;
	uint32_t capacity;
};

static int compare_entry(const void *left, const void *right)
{
	const reference_entry *a = (const reference_entry *)left;
	const reference_entry *b = (const reference_entry *)right;
	int order = tstring_cmp(a->target_uri, b->target_uri);
	if (!order) order = tstring_cmp(a->name, b->name);
	if (!order) order = tstring_cmp(a->source->uri, b->source->uri);
	if (!order)
		order = a->span.start < b->span.start ? -1 :
			a->span.start > b->span.start;
	return order;
}

static void free_entry(reference_entry *entry)
{
	tstring_free(entry->target_uri);
	tstring_free(entry->name);
}

void tworkspace_reference_index_free(tworkspace *workspace)
{
	if (!workspace || !workspace->references) return;
	for (uint32_t i = 0; i < workspace->references->count; i++)
		free_entry(&workspace->references->items[i]);
	free(workspace->references->items);
	free(workspace->references);
	workspace->references = nullptr;
}

void tworkspace_reference_remove(tworkspace *workspace,
	const tworkspace_document *source)
{
	if (!workspace || !workspace->references) return;
	tworkspace_reference_index *index = workspace->references;
	uint32_t kept = 0;
	for (uint32_t i = 0; i < index->count; i++) {
		if (index->items[i].source == source)
			free_entry(&index->items[i]);
		else
			index->items[kept++] = index->items[i];
	}
	index->count = kept;
}

void tworkspace_reference_add(tworkspace *workspace,
	const tworkspace_document *source, const tworkspace_document *target,
	const tmodule_export *exported, tsource_span span)
{
	if (!workspace || !source || !target || !exported) return;
	if (!workspace->references) {
		workspace->references = (tworkspace_reference_index *)calloc(
			1, sizeof(*workspace->references));
		if (!workspace->references) abort();
	}
	tworkspace_reference_index *index = workspace->references;
	if (index->count == index->capacity) {
		uint32_t capacity = index->capacity ? index->capacity * 2 : 32;
		reference_entry *items = (reference_entry *)realloc(
			index->items, capacity * sizeof(*items));
		if (!items) abort();
		index->items = items;
		index->capacity = capacity;
	}
	index->items[index->count++] = (reference_entry){
		.source = source,
		.target_uri = tstring_dup(target->uri),
		.name = tstring_dup(exported->name),
		.span = span
	};
}

void tworkspace_reference_sort(tworkspace *workspace)
{
	if (!workspace || !workspace->references) return;
	qsort(workspace->references->items, workspace->references->count,
		sizeof(*workspace->references->items), compare_entry);
}

static int compare_key(const reference_entry *entry,
		       const tworkspace_document *target,
		       const tmodule_export *exported)
{
	int order = tstring_cmp(entry->target_uri, target->uri);
	return order ? order : tstring_cmp(entry->name, exported->name);
}

uint32_t tworkspace_find_references(
	const tworkspace *workspace, const tworkspace_document *target,
	const tmodule_export *exported, tworkspace_reference **references)
{
	if (references) *references = nullptr;
	if (!workspace || !workspace->references || !target || !exported)
		return 0;
	const tworkspace_reference_index *index = workspace->references;
	uint32_t low = 0, high = index->count;
	while (low < high) {
		uint32_t middle = low + (high - low) / 2;
		if (compare_key(&index->items[middle], target, exported) < 0)
			low = middle + 1;
		else
			high = middle;
	}
	uint32_t first = low;
	while (low < index->count &&
	       compare_key(&index->items[low], target, exported) == 0)
		low++;
	uint32_t count = low - first;
	if (!count || !references) return count;
	*references = (tworkspace_reference *)malloc(count * sizeof(**references));
	if (!*references) abort();
	for (uint32_t i = 0; i < count; i++)
		(*references)[i] = (tworkspace_reference){
			index->items[first + i].source, index->items[first + i].span
		};
	return count;
}
