#ifndef TAPAS_DS_TOBJ_ARRAY_H
#define TAPAS_DS_TOBJ_ARRAY_H

#include "tapas/tval.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	tobj *data;
	uint_objs len;
	uint_objs capacity;
} tobj_array;

void tobj_array_init(tobj_array *arr, uint_objs cap);
void tobj_array_free(tobj_array *arr);
void tobj_array_try_expand(tobj_array *arr, uint_objs newcap);
uint_objs tobj_array_get_len(const tobj_array *arr);
uint_objs tobj_array_get_cap(const tobj_array *arr);
tobj *tobj_array_get_obj(tobj_array *arr, uint_objs loc);
void tobj_array_set_obj(tobj_array *arr, uint_objs loc, const tobj *v);
void tobj_array_add_obj(tobj_array *arr, uint_csts nameloc);
void tobj_array_set_len(tobj_array *arr, uint_objs n);
void tobj_array_del_obj(tobj_array *arr, uint_objs n);
uint_objs tobj_array_get_ref_obj_loc(tobj_array *arr, tcompo_v *compo);

#ifdef __cplusplus
}
#endif

#endif /* TAPAS_DS_TOBJ_ARRAY_H */
