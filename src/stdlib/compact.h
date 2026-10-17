/* Shared storage operations for package-owned, fixed-length compact sequences.
 * Package vtables own their public Type identities; the runtime needs no new
 * built-in value tag. Views retain their source and never resize its storage. */
#ifndef TAPAS_STDLIB_COMPACT_H
#define TAPAS_STDLIB_COMPACT_H

#include "tapas/tval.h"

#include <stddef.h>

typedef enum {
	tcompact_byte,
	tcompact_bool,
	tcompact_real
} tcompact_kind;

typedef struct {
	tcompo_v base;
	tcompact_kind kind;
	size_t length;
	void *data;
	tcompo_v *owner;
} tcompact;

tcompact *tcompact_new(tcompo_vtable *vtable, tcompact_kind kind, size_t length,
			 tcompo_v *owner, void *data);
long tcompact_len(void *self);
void *tcompact_copy(void *self);
void tcompact_free(void *self);
int tcompact_identical(void *self, void *other);
tstring *tcompact_string(void *self);

extern const tcompo_capabilities tcompact_capabilities;
extern const tcompo_bulk_operations tcompact_bulk_operations;

#endif
