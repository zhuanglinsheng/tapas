/**
 * @file table.c
 * @brief Implements immutable scalar relation tables and row membership.
 * @details Column order is explicit. Strings and returned collections are
 * copied so neither source data nor reflected rows can mutate the relation.
 */
#include "table.h"
#include "tapas/dsa/tstring.h"
#include "tapas/objects/tstr.h"
#include "tapas/objects/ttype.h"
#include "tapas/textension.h"
#include "tapas/tformat.h"

#include <stdlib.h>
#include <string.h>

static tcompo_vtable table_vtable;

static int is_string(const tobj *value)
{
	return value->type == tcompo && tobj_compo_type(value) == compo_tstr;
}

static int is_cell(const tobj *value)
{
	return value->type == tint || value->type == tbool || is_string(value);
}

static void push_snapshot(tobj_vec *values, const tobj *value)
{
	if (is_string(value)) {
		tobj text;
		const tstring *source = ((tstr *)value->val.v_tcompo)->data;
		tobj_set_compo(&text, (tcompo_v *)tstr_new_len(
			tstring_cstr(source), tstring_len(source)));
		tobj_vec_push(values, &text);
		tobj_try_clear(&text);
	} else {
		tobj_vec_push(values, value);
	}
}

int trules_table_is(const tobj *value)
{
	return value && value->type == tcompo && value->val.v_tcompo &&
		value->val.v_tcompo->vtable == &table_vtable;
}

trules_table *trules_table_new(const tlist *columns, const tdict *data)
{
	uint_objs width = tlist_size(columns);
	if (!width || (long)width != data->base.vtable->len((void *)data))
		twarn(ErrRuntime_ParamsType, "rules::table",
			"nonempty columns must name every data column exactly once");
	uint_objs height = 0;
	/* Validate before allocating the result: malformed data is a constructor
	 * error, whereas a candidate row with the wrong shape is simply absent. */
	for (uint_objs c = 0; c < width; c++) {
		const tobj *name = tlist_at(columns, c);
		if (!is_string(name))
			twarn(ErrRuntime_ParamsType, "rules::table", "String column names required");
		for (uint_objs p = 0; p < c; p++)
			if (tobj_identical(name, tlist_at(columns, p)))
				twarn(ErrRuntime_ParamsType, "rules::table", "duplicate column name");
		tobj column;
		tobj_set_nil(&column);
		if (!tdict_try_get((tdict *)data, name, &column) ||
		    column.type != tcompo || tobj_compo_type(&column) != compo_tlist)
			twarn(ErrRuntime_ParamsType, "rules::table", "List data column required");
		tlist *values = (tlist *)column.val.v_tcompo;
		if (c && height != tlist_size(values))
			twarn(ErrRuntime_ParamsType, "rules::table", "column lengths must match");
		height = tlist_size(values);
		for (uint_objs r = 0; r < height; r++) {
			const tobj *cell = tlist_at(values, r);
			if (!is_cell(cell) || cell->type != tlist_at(values, 0)->type)
				twarn(ErrRuntime_ParamsType, "rules::table",
					"each column requires one scalar Type: Int, Bool or String");
		}
		tobj_try_clear(&column);
	}
	trules_table *table = calloc(1, sizeof(*table));
	if (!table) abort();
	table->base.vtable = &table_vtable;
	tobj_vec_init(&table->columns);
	tobj_vec_init(&table->cells);
	for (uint_objs c = 0; c < width; c++)
		push_snapshot(&table->columns, tlist_at(columns, c));
	for (uint_objs r = 0; r < height; r++) {
		for (uint_objs c = 0; c < width; c++) {
			tobj column;
			tobj_set_nil(&column);
			tdict_get((tdict *)data, tlist_at(columns, c), &column);
			push_snapshot(&table->cells,
				tlist_at((tlist *)column.val.v_tcompo, r));
			tobj_try_clear(&column);
		}
	}
	return table;
}

static long table_len(void *self)
{
	trules_table *table = self;
	return (long)(table->cells.len / table->columns.len);
}

static int table_contains(void *self, const tobj *value)
{
	trules_table *table = self;
	if (value->type != tcompo || tobj_compo_type(value) != compo_tlist)
		return 0;
	tlist *row = (tlist *)value->val.v_tcompo;
	uint_objs width = table->columns.len;
	if (tlist_size(row) != width) return 0;
	for (uint_objs r = 0; r < table->cells.len; r += width) {
		int matches = 1;
		for (uint_objs c = 0; c < width; c++) {
			const tobj *cell = tlist_at(row, c);
			const tobj *expected = &table->cells.data[r + c];
			if (!is_cell(cell) || cell->type != expected->type ||
			    !tobj_identical(cell, expected)) {
				matches = 0;
				break;
			}
		}
		if (matches) return 1;
	}
	return 0;
}

static void table_free(void *self)
{
	trules_table *table = self;
	tobj_vec_free(&table->columns);
	tobj_vec_free(&table->cells);
	free(table);
}

static void *table_copy(void *self)
{
	trules_table *source = self;
	trules_table *table = calloc(1, sizeof(*table));
	if (!table) abort();
	table->base.vtable = &table_vtable;
	tobj_vec_init(&table->columns);
	tobj_vec_init(&table->cells);
	for (uint_objs c = 0; c < source->columns.len; c++)
		push_snapshot(&table->columns, &source->columns.data[c]);
	for (uint_objs c = 0; c < source->cells.len; c++)
		push_snapshot(&table->cells, &source->cells.data[c]);
	return table;
}

static int table_identical(void *self, void *other)
{
	trules_table *left = self;
	trules_table *right = other;
	if (left->columns.len != right->columns.len ||
	    left->cells.len != right->cells.len) return 0;
	for (uint_objs i = 0; i < left->columns.len; i++)
		if (!tobj_identical(&left->columns.data[i], &right->columns.data[i]))
			return 0;
	for (uint_objs i = 0; i < left->cells.len; i++)
		if (left->cells.data[i].type != right->cells.data[i].type ||
		    !tobj_identical(&left->cells.data[i], &right->cells.data[i])) return 0;
	return 1;
}

static int table_try_index(void *self, const tobj *arguments,
	uint_regs count, tobj *result)
{
	trules_table *table = self;
	if (count != 1 || !is_string(arguments))
		twarn(ErrRuntime_ParamsType, "Table", "one String field required");
	const char *name = tstring_cstr(((tstr *)arguments->val.v_tcompo)->data);
	if (strcmp(name, "columns") && strcmp(name, "rows")) return 0;
	tlist *list = tlist_new();
	if (!strcmp(name, "columns")) {
		for (uint_objs c = 0; c < table->columns.len; c++)
			push_snapshot(&list->items, &table->columns.data[c]);
	} else {
		for (uint_objs r = 0; r < table->cells.len; r += table->columns.len) {
			tlist *row = tlist_new();
			for (uint_objs c = 0; c < table->columns.len; c++)
				push_snapshot(&row->items, &table->cells.data[r + c]);
			tobj value;
			tobj_set_compo(&value, (tcompo_v *)row);
			tobj_vec_push(&list->items, &value);
			tobj_try_clear(&value);
		}
	}
	tobj_set_compo(result, (tcompo_v *)list);
	return 1;
}

static void table_index(void *self, const tobj *arguments,
	uint_regs count, tobj *result)
{
	if (!table_try_index(self, arguments, count, result))
		twarn(ErrRuntime_ObjUnfound, "Table", "unknown field");
}

static const char *table_name(void) { return "rules::Table"; }

static void table_runtime_type(void *self, tobj *result)
{
	(void)self;
	tobj_set_compo(result, (tcompo_v *)ttypeval_new_named("rules::Table"));
}

static void table_render(tformat_context *context, const void *self)
{
	const trules_table *table = self;
	tformat_text(context, "Table[");
	tformat_sequence(context, &table->columns);
	tformat_text(context, "][");
	tformat_sequence(context, &table->cells);
	tformat_text(context, "]");
}

static tstring *table_string(void *self)
{
	return tformat_object(self, 16384, table_render);
}

const tcompo_capabilities trules_table_capabilities = {
	.indexable = table_index,
	.try_indexable = table_try_index,
	.contains = table_contains,
	.runtime_type = table_runtime_type
};

static tcompo_vtable table_vtable = {
	.get_type = table_name,
	.compo_code = compo_extension,
	.len = table_len,
	.copy = table_copy,
	.free = table_free,
	.identical = table_identical,
	.tostring_abbr = table_string,
	.tostring_full = table_string,
	.capabilities = &trules_table_capabilities
};
