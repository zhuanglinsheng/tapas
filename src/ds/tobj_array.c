#include "tapas/ds/tobj_array.h"

#include <stdlib.h>

void tobj_array_init(tobj_array *arr, uint_objs cap)
{
	arr->data = nullptr;
	arr->len = 0;
	arr->capacity = 0;
	if (cap > 0) {
		arr->data = (tobj *)calloc(cap, sizeof(tobj));
		arr->capacity = cap;
		uint_objs i;
		for (i = 0; i < cap; i++)
			tobj_set_nil(&arr->data[i]);
	}
}

void tobj_array_free(tobj_array *arr)
{
	if (arr->data) {
		uint_objs i;
		for (i = 0; i < arr->len; i++)
			if (arr->data[i].type == tcompo)
				tobj_ddc_ref_clear(&arr->data[i]);
		free(arr->data);
	}
	arr->data = nullptr;
	arr->len = 0;
	arr->capacity = 0;
}

void tobj_array_try_expand(tobj_array *arr, uint_objs newcap)
{
	if (newcap <= arr->capacity)
		return;
	uint_objs i;
	arr->data = (tobj *)realloc(arr->data, newcap * sizeof(tobj));
	for (i = arr->capacity; i < newcap; i++)
		tobj_set_nil(&arr->data[i]);
	arr->capacity = newcap;
}

uint_objs tobj_array_get_len(const tobj_array *arr)
{
	return arr->len;
}

uint_objs tobj_array_get_cap(const tobj_array *arr)
{
	return arr->capacity;
}

tobj *tobj_array_get_obj(tobj_array *arr, uint_objs loc)
{
	if (loc >= arr->len)
		twarn(ErrRuntime_ObjUnfound, "tobj_array_get_obj", "");
	return &arr->data[loc];
}

void tobj_array_set_obj(tobj_array *arr, uint_objs loc, const tobj *v)
{
	if (loc >= arr->len) {
		twarn(ErrRuntime_ObjUnfound, "tobj_array_set_obj", "");
		return;
	}
	if (arr->data[loc].type != tcompo && v->type != tcompo) {
		arr->data[loc] = *v;
		return;
	}
	/* Copy with ref counting */
	if (arr->data[loc].type == tcompo && v->type == tcompo &&
	    arr->data[loc].val.v_tcompo == v->val.v_tcompo)
		return;
	if (v->type == tcompo && v->val.v_tcompo)
		v->val.v_tcompo->refctr++;
	tobj_ddc_ref_clear(&arr->data[loc]);
	arr->data[loc] = *v;
}

void tobj_array_add_obj(tobj_array *arr, uint_csts nameloc)
{
	if (arr->len + 1 >= OBJ_LIMIT)
		twarn(ErrCompile_OBJOutOfLimit, "tobj_array_add_obj", "");
	if (arr->len >= arr->capacity) {
		uint_objs newcap = arr->capacity ? arr->capacity * 2 : 16;
		tobj_array_try_expand(arr, newcap);
	}
	arr->data[arr->len].type = tnil;
	arr->data[arr->len].name_loc = (int)nameloc;
	arr->len++;
}

void tobj_array_set_len(tobj_array *arr, uint_objs n)
{
	if (n > arr->capacity)
		tobj_array_try_expand(arr, n);
	while (arr->len > n) {
		arr->len--;
		if (arr->data[arr->len].type == tcompo)
			tobj_ddc_ref_clear(&arr->data[arr->len]);
		else
			tobj_set_nil(&arr->data[arr->len]);
	}
	arr->len = n;
}

void tobj_array_del_obj(tobj_array *arr, uint_objs n)
{
	while (n > 0 && arr->len > 0) {
		arr->len--;
		if (arr->data[arr->len].type == tcompo)
			tobj_ddc_ref_clear(&arr->data[arr->len]);
		else
			tobj_set_nil(&arr->data[arr->len]);
		n--;
	}
}

uint_objs tobj_array_get_ref_obj_loc(tobj_array *arr, tcompo_v *compo)
{
	uint_objs i;
	for (i = 0; i < arr->len; i++) {
		if (arr->data[i].type == tcompo &&
		    arr->data[i].val.v_tcompo == compo)
			return i;
	}
	return UNDEF_ENVLOC;
}
