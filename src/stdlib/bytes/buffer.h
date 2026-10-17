/* Internal byte-storage bridge for I/O packages, not a new runtime value tag. */
#ifndef TAPAS_STDLIB_BYTES_BUFFER_H
#define TAPAS_STDLIB_BYTES_BUFFER_H
#include "tapas/tval.h"
#include <stddef.h>
void tstdlib_bytes_take(tobj *result, void *data, size_t length);
int tstdlib_bytes_data(const tobj *value, const void **data, size_t *length);
#endif
