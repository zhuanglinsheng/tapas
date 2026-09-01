#ifndef TAPAS_DS_THASHTBL_H
#define TAPAS_DS_THASHTBL_H

#include "tapas/tbycs.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tobj tobj;
typedef struct thashtbl thashtbl;

typedef void (*thashtbl_each_fn)(const tobj *key, const tobj *value, void *ctx);

thashtbl *thashtbl_new(void);
void thashtbl_free(thashtbl *tbl);
uint_objs thashtbl_len(const thashtbl *tbl);
void thashtbl_set(thashtbl *tbl, const tobj *key, const tobj *value);
const tobj *thashtbl_get(const thashtbl *tbl, const tobj *key);
int thashtbl_contains(const thashtbl *tbl, const tobj *key);
int thashtbl_delete(thashtbl *tbl, const tobj *key);
thashtbl *thashtbl_copy(const thashtbl *tbl);
void thashtbl_each(const thashtbl *tbl, thashtbl_each_fn fn, void *ctx);

#ifdef __cplusplus
}
#endif

#endif /* TAPAS_DS_THASHTBL_H */
