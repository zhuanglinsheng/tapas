#include "tapas/tval.h"
#include "tapas/dsa/tstring.h"

/*----------------------- Value Lifetime and Assignment --------------------*/

/* 循环结构（如包含自身的 List/Dictionary）会让递归式打印与比较失去终止点。
 * 复合值的递归必然穿过 tobj_tostring/tobj_identical 这两个汇聚点，在这里
 * 限深即可保护所有复合类型。 */
#define TOBJ_RECURSE_LIMIT 512
static _Thread_local unsigned tobj_tostring_depth;
static _Thread_local unsigned tobj_identical_depth;

static void tobj_release_compo(tcompo_v *compo)
{
	if (!compo)
		return;
	if (compo->refctr > 0)
		compo->refctr--;
	if (compo->refctr == 0 && compo->vtable)
		compo->vtable->free(compo);
}

static void tobj_clear_impl(tobj *v, int release_ref)
{
	if (v->type == tcompo && v->val.v_tcompo) {
		if (release_ref)
			tobj_release_compo(v->val.v_tcompo);
		else if (v->val.v_tcompo->refctr == 0 &&
			 v->val.v_tcompo->vtable)
			v->val.v_tcompo->vtable->free(v->val.v_tcompo);
	}
	v->type = tnil;
	v->name_loc = (uint_csts)UNDEF_NAMELOC;
	v->val.v_tint = 0;
}

void tobj_set_nil(tobj *v)
{
	v->type = tnil;
	v->name_loc = (uint_csts)UNDEF_NAMELOC;
	v->val.v_tint = 0;
}

void tobj_set_bool(tobj *v, int b)
{
	tobj_try_clear(v);
	v->type = tbool;
	v->name_loc = (uint_csts)UNDEF_NAMELOC;
	v->val.v_tbool = b;
}

void tobj_set_int(tobj *v, long i)
{
	tobj_try_clear(v);
	v->type = tint;
	v->name_loc = (uint_csts)UNDEF_NAMELOC;
	v->val.v_tint = i;
}

void tobj_set_float(tobj *v, double d)
{
	tobj_try_clear(v);
	v->type = tfloat;
	v->name_loc = (uint_csts)UNDEF_NAMELOC;
	v->val.v_tfloat = d;
}

void tobj_set_compo(tobj *v, tcompo_v *compo)
{
	if (v->type == tcompo && v->val.v_tcompo == compo)
		return;
	tobj_try_clear(v);
	v->type = tcompo;
	v->name_loc = (uint_csts)UNDEF_NAMELOC;
	v->val.v_tcompo = compo;
}

/* Release one owned composite reference and clear the value. */
void tobj_ddc_ref_clear(tobj *v)
{
	tobj_clear_impl(v, 1);
}

void tobj_try_clear(tobj *v)
{
	tobj_clear_impl(v, 0);
}

void tobj_copy(tobj *dst, const tobj *src)
{
	if (dst == src)
		return;
	if (src->type == tcompo && src->val.v_tcompo) {
		if (dst->type == tcompo && dst->val.v_tcompo == src->val.v_tcompo)
			return;
		src->val.v_tcompo->refctr++;
	}
	tobj_ddc_ref_clear(dst);
	dst->type = src->type;
	dst->name_loc = src->name_loc;
	dst->val = src->val;
}

int tobj_identical(const tobj *a, const tobj *b)
{
	if (a->type != b->type)
		return 0;
	switch (a->type) {
	case tnil:
		return 1;
	case tbool:
		return a->val.v_tbool == b->val.v_tbool;
	case tint:
		return a->val.v_tint == b->val.v_tint;
	case tfloat:
		return a->val.v_tfloat == b->val.v_tfloat;
	case tcompo: {
		if (a->val.v_tcompo == b->val.v_tcompo)
			return 1;
		if (!a->val.v_tcompo || !b->val.v_tcompo ||
		    !a->val.v_tcompo->vtable || !b->val.v_tcompo->vtable ||
		    a->val.v_tcompo->vtable->compo_code !=
			    b->val.v_tcompo->vtable->compo_code ||
		    (a->val.v_tcompo->vtable->compo_code == compo_extension &&
		     a->val.v_tcompo->vtable != b->val.v_tcompo->vtable))
			return 0;
		/* 深度耗尽时保守地报告不等；循环结构不再无限递归。 */
		if (tobj_identical_depth >= TOBJ_RECURSE_LIMIT)
			return 0;
		tobj_identical_depth++;
		int same = a->val.v_tcompo->vtable->identical(
			a->val.v_tcompo, b->val.v_tcompo);
		tobj_identical_depth--;
		return same;
	}
	}
	return 0;
}

/*---------------------------- String Conversion ---------------------------*/

tstring *tobj_tostring_pointer(const char *type, const void *ptr)
{
	tstring *out = tstring_new_empty();
	tstring_append_fmt(out, "%s <%p>", type, ptr);
	return out;
}

static tstring *tobj_tostring(const tobj *value, int full)
{
	tstring *out;
	switch (value->type) {
	case tnil:
		return tstring_new("nil");
	case tbool:
		return tstring_new(value->val.v_tbool ? "true" : "false");
	case tint:
		out = tstring_new_empty();
		tstring_append_fmt(out, "%ld", value->val.v_tint);
		return out;
	case tfloat:
		out = tstring_new_empty();
		tstring_append_fmt(out, "%g", value->val.v_tfloat);
		return out;
	case tcompo:
		if (!value->val.v_tcompo || !value->val.v_tcompo->vtable)
			return tstring_new("<null>");
		if (tobj_tostring_depth >= TOBJ_RECURSE_LIMIT)
			return tstring_new("<recursive>");
		tobj_tostring_depth++;
		out = full
			? value->val.v_tcompo->vtable->tostring_full(
				value->val.v_tcompo)
			: value->val.v_tcompo->vtable->tostring_abbr(
				value->val.v_tcompo);
		tobj_tostring_depth--;
		return out;
	}
	return tstring_new("<unknown>");
}

tstring *tobj_tostring_abbr(const tobj *value)
{
	return tobj_tostring(value, 0);
}

tstring *tobj_tostring_full(const tobj *value)
{
	return tobj_tostring(value, 1);
}

/*------------------------------- Accessors --------------------------------*/

ttypes tobj_get_type(const tobj *v)
{
	return v->type;
}

long tobj_get_v_tint(const tobj *v)
{
	return v->val.v_tint;
}

double tobj_get_v_tfloat(const tobj *v)
{
	return v->val.v_tfloat;
}

int tobj_get_v_tbool(const tobj *v)
{
	return v->val.v_tbool;
}

tcompo_v *tobj_get_v_tcompo(const tobj *v)
{
	return v->val.v_tcompo;
}

int tobj_get_name_loc(const tobj *v)
{
	return v->name_loc;
}

int tobj_is_nil(const tobj *v)
{
	return v->type == tnil;
}

tcompo_type tobj_compo_type(const tobj *v)
{
	if (v->type == tcompo && v->val.v_tcompo && v->val.v_tcompo->vtable)
		return v->val.v_tcompo->vtable->compo_code;
	return (tcompo_type)(-1);
}

const char *tobj_compo_type_name(const tobj *v)
{
	if (v->type == tcompo && v->val.v_tcompo && v->val.v_tcompo->vtable)
		return v->val.v_tcompo->vtable->get_type();
	return "Unknown";
}
