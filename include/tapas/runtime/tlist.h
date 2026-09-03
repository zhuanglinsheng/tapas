#ifndef TAPAS_RUNTIME_TLIST_H
#define TAPAS_RUNTIME_TLIST_H

#include "tapas/ds/tobj_vec.h"
#include "tapas/tval.h"

#ifdef __cplusplus
extern "C" {
#endif

struct tlist {
	tcompo_v base;
	tobj_vec items;
};

extern tcompo_vtable tlist_vtable;

tlist *tlist_new(void);
uint_objs tlist_size(const tlist *l);
const tobj *tlist_at(const tlist *l, uint_objs idx);
void tlist_set_at(tlist *l, uint_objs idx, const tobj *v);

#ifdef __cplusplus
}
#endif

#endif /* TAPAS_RUNTIME_TLIST_H */
