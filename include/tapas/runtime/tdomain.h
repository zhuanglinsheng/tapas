#ifndef TAPAS_RUNTIME_TDOMAIN_H
#define TAPAS_RUNTIME_TDOMAIN_H
#include "tapas/runtime/ttype.h"
#include "tapas/ds/tobj_vec.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    tcompo_v base;
    ttypeval *item_type;
    tobj_vec values;
    long start, end;
    int range;
} tdomain;
extern tcompo_vtable tpoints_vtable, trange_vtable;
tdomain *tpoints_new(ttypeval *type, const tobj *values, uint_regs count);
tdomain *trange_new(long start, long end);
#ifdef __cplusplus
}
#endif
#endif
