/**
 * @file table.h
 * @brief Declares the immutable finite Table owned by the rules package.
 * @note This representation is private to the rules package. Other packages
 * consume its named Type and ordinary object capabilities.
 */
#ifndef TAPAS_STDLIB_RULES_TABLE_H
#define TAPAS_STDLIB_RULES_TABLE_H

#include "tapas/objects/tdict.h"
#include "tapas/objects/tlist.h"

typedef struct {
	tcompo_v base;
	tobj_vec columns;
	tobj_vec cells;
} trules_table;

extern const tcompo_capabilities trules_table_capabilities;

trules_table *trules_table_new(const tlist *columns, const tdict *data);
int trules_table_is(const tobj *value);

#endif
