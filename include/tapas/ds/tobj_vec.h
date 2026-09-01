#ifndef TAPAS_DS_TOBJ_VEC_H
#define TAPAS_DS_TOBJ_VEC_H

#include "tapas/tbycs.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tobj tobj;

typedef struct {
	tobj *data;
	uint_objs len;
	uint_objs capacity;
} tobj_vec;

void tobj_vec_init(tobj_vec *v);
void tobj_vec_init_cap(tobj_vec *v, uint_objs cap);
void tobj_vec_free(tobj_vec *v);
uint_objs tobj_vec_len(const tobj_vec *v);
uint_objs tobj_vec_cap(const tobj_vec *v);
tobj *tobj_vec_at(tobj_vec *v, uint_objs idx);
const tobj *tobj_vec_at_const(const tobj_vec *v, uint_objs idx);
tobj *tobj_vec_data(tobj_vec *v);
void tobj_vec_reserve(tobj_vec *v, uint_objs cap);
void tobj_vec_push(tobj_vec *v, const tobj *obj);
void tobj_vec_set(tobj_vec *v, uint_objs idx, const tobj *obj);
void tobj_vec_insert(tobj_vec *v, uint_objs idx, const tobj *obj);
void tobj_vec_pop(tobj_vec *v, uint_objs idx);
void tobj_vec_copy(tobj_vec *dst, const tobj_vec *src);
void tobj_vec_copy_range(tobj_vec *dst, const tobj_vec *src,
			 uint_objs start, uint_objs count);

#ifdef __cplusplus
}
#endif

#endif /* TAPAS_DS_TOBJ_VEC_H */
