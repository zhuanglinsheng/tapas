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
int tdict_delete(tdict *d, const tobj *key);
void tdict_idx(tdict *d, const tobj *params, uint_regs np, tobj *vre);
void tdict_iset(tdict *d, const tobj *params, uint_regs np,
		const tobj *vright);
void tdict_set_append(tdict *d, const tobj *pair_val);
tlist *tdict_keys(tdict *d);
tlist *tdict_values(tdict *d);

#ifdef __cplusplus
}
#endif

#endif /* TAPAS_RUNTIME_TDICT_H */
