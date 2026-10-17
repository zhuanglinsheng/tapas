/* POSIX helpers shared by packages; never changes process-global environment. */
#ifndef TAPAS_STDLIB_SYSTEM_H
#define TAPAS_STDLIB_SYSTEM_H
#include "result.h"
#include "tapas/objects/tdict.h"

int tstdlib_fail(tstdlib_error *error, const char *code, const char *message);
int tstdlib_syserr(tstdlib_error *error, int number, const char *operation);
const char *tstdlib_path(const tobj *value, tstdlib_error *error);
void tstdlib_field(tdict *record, const char *name, const tobj *value);
void tstdlib_field_string(tdict *record, const char *name, const char *value);
void tstdlib_field_int(tdict *record, const char *name, long value);
void tstdlib_field_bool(tdict *record, const char *name, int value);
void tstdlib_field_float(tdict *record, const char *name, double value);
int tstdlib_monotonic(double *value);
#endif
