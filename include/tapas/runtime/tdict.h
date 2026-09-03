#ifndef TAPAS_RUNTIME_TDICT_H
#define TAPAS_RUNTIME_TDICT_H

#include "tapas/ds/thashtbl.h"
#include "tapas/tval.h"

#ifdef __cplusplus
extern "C" {
#endif

struct tdict {
	tcompo_v base;
	thashtbl *items;
};

extern tcompo_vtable tdict_vtable;

tdict *tdict_new(void);
void tdict_set(tdict *d, const tobj *key, const tobj *val);
void tdict_get(tdict *d, const tobj *key, tobj *vre);
int tdict_contains(tdict *d, const tobj *key);

#ifdef __cplusplus
}
#endif

#endif /* TAPAS_RUNTIME_TDICT_H */
