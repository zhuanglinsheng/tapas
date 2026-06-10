#include "tapas/tval.h"
#include "tapas/tbycs.h"


/* ---- tobj constructors ---- */

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

/** DDC (Direct Decrease Count) ref clear */
void tobj_ddc_ref_clear(tobj *v)
{
	tobj_clear_impl(v, 1);
}

void tobj_try_clear(tobj *v)
{
	tobj_clear_impl(v, 0);
}

/** Copy assignment */
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

/** Identity comparison */
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
	case tcompo:
		if (a->val.v_tcompo == b->val.v_tcompo)
			return 1;
		if (a->val.v_tcompo && b->val.v_tcompo &&
		    a->val.v_tcompo->vtable && b->val.v_tcompo->vtable &&
		    a->val.v_tcompo->vtable->get_compo_type_code() ==
			    b->val.v_tcompo->vtable->get_compo_type_code())
			return a->val.v_tcompo->vtable->identical(
				a->val.v_tcompo, b->val.v_tcompo);
		return 0;
	}
	return 0;
}

/* ---- tobj tostring ---- */

tstring *tobj_tostring_pointer(const char *type, const void *ptr)
{
	tstring *out = tstring_new_empty();
	tstring_append_fmt(out, "%s <%p>", type, ptr);
	return out;
}

tstring *tobj_tostring_abbr(const tobj *v)
{
	tstring *out;
	switch (v->type) {
	case tnil:
		return tstring_new("nil");
	case tbool:
		return tstring_new(v->val.v_tbool ? "true" : "false");
	case tint:
		out = tstring_new_empty();
		tstring_append_fmt(out, "%ld", v->val.v_tint);
		return out;
	case tfloat:
		out = tstring_new_empty();
		tstring_append_fmt(out, "%g", v->val.v_tfloat);
		return out;
	case tcompo:
		if (v->val.v_tcompo && v->val.v_tcompo->vtable)
			return v->val.v_tcompo->vtable->tostring_abbr(v->val.v_tcompo);
		return tstring_new("<null>");
	}
	return tstring_new("<unknown>");
}

tstring *tobj_tostring_full(const tobj *v)
{
	tstring *out;
	switch (v->type) {
	case tnil:
		return tstring_new("nil");
	case tbool:
		return tstring_new(v->val.v_tbool ? "true" : "false");
	case tint:
		out = tstring_new_empty();
		tstring_append_fmt(out, "%ld", v->val.v_tint);
		return out;
	case tfloat:
		out = tstring_new_empty();
		tstring_append_fmt(out, "%g", v->val.v_tfloat);
		return out;
	case tcompo:
		if (v->val.v_tcompo && v->val.v_tcompo->vtable)
			return v->val.v_tcompo->vtable->tostring_full(v->val.v_tcompo);
		return tstring_new("<null>");
	}
	return tstring_new("<unknown>");
}

/* ---- tobj accessors ---- */

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

/*===========================================================================*
 * 3. Object Array (tobj_array)
 *===========================================================================*/

void tobj_array_init(tobj_array *arr, uint_objs cap)
{
	arr->data = NULL;
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
			tobj_ddc_ref_clear(&arr->data[i]);
		free(arr->data);
	}
	arr->data = NULL;
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

void
tobj_array_set_obj(tobj_array *arr, uint_objs loc, const tobj *v)
{
	if (loc >= arr->len) {
		twarn(ErrRuntime_ObjUnfound, "tobj_array_set_obj", "");
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
		tobj_ddc_ref_clear(&arr->data[arr->len]);
	}
	arr->len = n;
}

void tobj_array_del_obj(tobj_array *arr, uint_objs n)
{
	while (n > 0 && arr->len > 0) {
		arr->len--;
		tobj_ddc_ref_clear(&arr->data[arr->len]);
		n--;
	}
}

uint_objs tobj_array_get_ref_obj_loc(tobj_array *arr,
						   tcompo_v *compo)
{
	uint_objs i;
	for (i = 0; i < arr->len; i++) {
		if (arr->data[i].type == tcompo &&
		    arr->data[i].val.v_tcompo == compo)
			return i;
	}
	return UNDEF_ENVLOC;
}

/*===========================================================================*
 * 4. String (tstr) — uses tstring
 *===========================================================================*/

static const char *tstr_get_type(void)
{
	return "String";
}

static tcompo_type tstr_get_code(void)
{
	return compo_tstr;
}

static long tstr_len(void *self)
{
	tstr *s = (tstr *)self;
	return (long)tstring_len(s->data);
}

static void *tstr_copy(void *self)
{
	tstr *s = (tstr *)self;
	tstr *n = (tstr *)calloc(1, sizeof(tstr));
	n->base.vtable = s->base.vtable;
	n->data = tstring_dup(s->data);
	return n;
}

static void tstr_free(void *self)
{
	tstr *s = (tstr *)self;
	tstring_free(s->data);
	free(s);
}

static int tstr_identical(void *self, void *other)
{
	tstr *a = (tstr *)self;
	tstr *b = (tstr *)other;
	return tstring_eq(a->data, b->data) ? 1 : 0;
}

static tstring *tstr_tostring_abbr(void *self)
{
	tstr *s = (tstr *)self;
	return tstring_dup(s->data);
}

static tstring *tstr_tostring_full(void *self)
{
	tstr *s = (tstr *)self;
	return tstring_dup(s->data);
}

tcompo_vtable tstr_vtable = { tstr_get_type,	 tstr_get_code,
				     tstr_len,		 tstr_copy,
				     tstr_free,		 tstr_identical,
				     tstr_tostring_abbr, tstr_tostring_full };

tstr *tstr_new(const char *s)
{
	tstr *st = (tstr *)calloc(1, sizeof(tstr));
	st->base.vtable = &tstr_vtable;
	st->data = tstring_new(s ? s : "");
	return st;
}

static int pair_to_range(const tobj *param, long len, long *start, long *end)
{
	if (param->type != tcompo || tobj_compo_type(param) != compo_tpair)
		return 0;
	tpair *p = (tpair *)param->val.v_tcompo;
	if (p->first.type != tint || p->second.type != tint)
		twarn(ErrRuntime_ParamsType, "pair_to_range", "");

	long sidx = p->first.val.v_tint;
	long eidx = p->second.val.v_tint;
	if (sidx < 0)
		sidx += len;
	if (eidx < 0)
		eidx += len;
	if (sidx < 0 || eidx < sidx || eidx > len)
		twarn(ErrRuntime_IdxOutRange, "pair_to_range", "");
	*start = sidx;
	*end = eidx;
	return 1;
}

static void tstr_replace_range(tstr *s, long start, long end, const char *repl)
{
	size_t slen = tstring_len(s->data);
	size_t rlen = strlen(repl);
	tstring *next = tstring_new_cap((size_t)start + rlen +
					(slen - (size_t)end) + 1);
	tstring_append_len(next, tstring_cstr(s->data), (size_t)start);
	tstring_append_len(next, repl, rlen);
	tstring_append_len(next,
			   tstring_cstr(s->data) + end,
			   slen - (size_t)end);
	tstring_assign_ts(s->data, next);
	tstring_free(next);
}

/* String indexing helper */
void
tstr_idx(tstr *s, const tobj *params, uint_regs np, tobj *vre)
{
	if (np != 1)
		twarn(ErrRuntime_ParamsCtr, "tstr_idx", "");
	long start, end;
	if (pair_to_range(&params[0],
			  (long)tstring_len(s->data),
			  &start,
			  &end)) {
		tstring *slice = tstring_new_len(tstring_cstr(s->data) + start,
						 (size_t)(end - start));
		tobj_set_compo(vre, (tcompo_v *)tstr_new(tstring_cstr(slice)));
		tstring_free(slice);
		return;
	}
	if (params[0].type != tint)
		twarn(ErrRuntime_ParamsType, "tstr_idx", "");
	long idx = params[0].val.v_tint;
	size_t slen = tstring_len(s->data);
	if (idx < 0)
		idx += (long)slen;
	if (idx < 0 || (size_t)idx >= slen)
		twarn(ErrRuntime_IdxOutRange, "tstr_idx", "");
	char ch[2] = { tstring_cstr(s->data)[idx], '\0' };
	tobj_set_compo(vre, (tcompo_v *)tstr_new(ch));
}

void
tstr_iset(tstr *s, const tobj *params, uint_regs np, const tobj *vright)
{
	if (np != 1)
		twarn(ErrRuntime_ParamsCtr, "tstr_iset", "");
	if (vright->type != tcompo || tobj_compo_type(vright) != compo_tstr)
		twarn(ErrRuntime_ParamsType, "tstr_iset", "");
	const char *repl = tstring_cstr(((tstr *)vright->val.v_tcompo)->data);
	long start, end;
	if (pair_to_range(&params[0],
			  (long)tstring_len(s->data),
			  &start,
			  &end)) {
		tstr_replace_range(s, start, end, repl);
		return;
	}
	if (params[0].type != tint)
		twarn(ErrRuntime_ParamsType, "tstr_iset", "");
	long idx = params[0].val.v_tint;
	long slen = (long)tstring_len(s->data);
	if (idx < 0)
		idx += slen;
	if (idx < 0 || idx >= slen)
		twarn(ErrRuntime_IdxOutRange, "tstr_iset", "");
	tstr_replace_range(s, idx, idx + 1, repl);
}

/*===========================================================================*
 * 5. List (tlist) - dynamic array of tobj
 *===========================================================================*/

static const char *tlist_get_type(void)
{
	return "List";
}

static tcompo_type tlist_get_code(void)
{
	return compo_tlist;
}

static long tlist_len(void *self)
{
	tlist *l = (tlist *)self;
	return (long)tobj_vec_len(&l->items);
}

static void *tlist_copy(void *self)
{
	tlist *l = (tlist *)self;
	tlist *n = (tlist *)calloc(1, sizeof(tlist));
	n->base.vtable = l->base.vtable;
	tobj_vec_copy(&n->items, &l->items);
	return n;
}

static void tlist_free(void *self)
{
	tlist *l = (tlist *)self;
	tobj_vec_free(&l->items);
	free(l);
}

static int tlist_identical(void *self, void *other)
{
	tlist *a = (tlist *)self;
	tlist *b = (tlist *)other;
	if (tobj_vec_len(&a->items) != tobj_vec_len(&b->items))
		return 0;
	for (uint_objs i = 0; i < tobj_vec_len(&a->items); i++) {
		if (!tobj_identical(tobj_vec_at(&a->items, i),
				    tobj_vec_at(&b->items, i)))
			return 0;
	}
	return 1;
}

static tstring *tlist_tostring_abbr(void *self)
{
	return tobj_tostring_pointer("List", self);
}

static tstring *tlist_tostring_full(void *self)
{
	tlist *l = (tlist *)self;
	uint_objs len = tobj_vec_len(&l->items);
	tstring *out = tstring_new("[");
	for (uint_objs i = 0; i < len; i++) {
		tstring *item = tobj_tostring_full(tobj_vec_at(&l->items, i));
		tstring_append_ts(out, item);
		tstring_free(item);
		if (i < len - 1)
			tstring_append(out, ", ");
	}
	tstring_append_c(out, ']');
	return out;
}

tcompo_vtable tlist_vtable = {
	tlist_get_type,	     tlist_get_code,	 tlist_len,
	tlist_copy,	     tlist_free,	 tlist_identical,
	tlist_tostring_abbr, tlist_tostring_full
};

tlist *tlist_new(void)
{
	tlist *l = (tlist *)calloc(1, sizeof(tlist));
	l->base.vtable = &tlist_vtable;
	tobj_vec_init(&l->items);
	return l;
}

uint_objs tlist_size(const tlist *l)
{
	return tobj_vec_len(&l->items);
}

const tobj *tlist_at(const tlist *l, uint_objs idx)
{
	return tobj_vec_at_const(&l->items, idx);
}

void tlist_push(tlist *l, const tobj *v)
{
	tobj_vec_push(&l->items, v);
}

void tlist_insert(tlist *l, uint_objs idx, const tobj *v)
{
	if (idx > tobj_vec_len(&l->items))
		twarn(ErrRuntime_IdxOutRange, "tlist_insert", "");
	tobj_vec_insert(&l->items, idx, v);
}

void tlist_set_at(tlist *l, uint_objs idx, const tobj *v)
{
	if (idx >= tobj_vec_len(&l->items))
		twarn(ErrRuntime_IdxOutRange, "tlist_set_at", "");
	tobj_vec_set(&l->items, idx, v);
}

void tlist_pop(tlist *l, uint_objs idx)
{
	if (idx >= tobj_vec_len(&l->items))
		twarn(ErrRuntime_IdxOutRange, "tlist_pop", "");
	tobj_vec_pop(&l->items, idx);
}

void tlist_sort(tlist *l, int (*compar)(const void *, const void *))
{
	qsort(tobj_vec_data(&l->items),
	      tobj_vec_len(&l->items),
	      sizeof(tobj),
	      compar);
}

int tlist_in(tlist *l, const tobj *v)
{
	for (uint_objs i = 0; i < tobj_vec_len(&l->items); i++) {
		if (tobj_identical(tobj_vec_at(&l->items, i), v))
			return 1;
	}
	return 0;
}

void
tlist_idx(tlist *l, const tobj *params, uint_regs np, tobj *vre)
{
	if (np != 1)
		twarn(ErrRuntime_ParamsCtr, "tlist_idx", "");
	long start, end;
	if (pair_to_range(&params[0], (long)tobj_vec_len(&l->items), &start, &end)) {
		tlist *slice = tlist_new();
		long i;
		for (i = start; i < end; i++)
			tlist_push(slice, tobj_vec_at(&l->items, (uint_objs)i));
		tobj_set_compo(vre, (tcompo_v *)slice);
		return;
	}
	if (params[0].type != tint)
		twarn(ErrRuntime_ParamsType, "tlist_idx", "");
	long idx = params[0].val.v_tint;
	uint_objs len = tobj_vec_len(&l->items);
	if (idx < 0)
		idx += (long)len;
	if (idx < 0 || (uint_objs)idx >= len)
		twarn(ErrRuntime_IdxOutRange, "tlist_idx", "");
	*vre = *tobj_vec_at(&l->items, (uint_objs)idx);
	if (vre->type == tcompo && vre->val.v_tcompo)
		vre->val.v_tcompo->refctr++;
}

void
tlist_iset(tlist *l, const tobj *params, uint_regs np, const tobj *vright)
{
	if (np != 1)
		twarn(ErrRuntime_ParamsCtr, "tlist_iset", "");
	if (params[0].type != tint)
		twarn(ErrRuntime_ParamsType, "tlist_iset", "");
	long idx = params[0].val.v_tint;
	uint_objs len = tobj_vec_len(&l->items);
	if (idx < 0)
		idx += (long)len;
	if (idx < 0 || (uint_objs)idx >= len)
		twarn(ErrRuntime_IdxOutRange, "tlist_iset", "");
	tlist_set_at(l, (uint_objs)idx, vright);
}

/* Iteration support */
int tlist_next_at(tlist *l, long *iter_pos, tobj *vre)
{
	if (*iter_pos >= (long)tobj_vec_len(&l->items)) {
		*iter_pos = 0;
		return 0;
	}
	*vre = *tobj_vec_at(&l->items, (uint_objs)*iter_pos);
	if (vre->type == tcompo && vre->val.v_tcompo)
		vre->val.v_tcompo->refctr++;
	(*iter_pos)++;
	return 1;
}

/*===========================================================================*
 * 6. Pair (tpair)
 *===========================================================================*/

static const char *tpair_get_type(void)
{
	return "Pair";
}

static tcompo_type tpair_get_code(void)
{
	return compo_tpair;
}

static long tpair_len(void *self)
{
	(void)self;
	return 2;
}

static void *tpair_copy(void *self)
{
	tpair *p = (tpair *)self;
	tpair *n = (tpair *)calloc(1, sizeof(tpair));
	n->base.vtable = p->base.vtable;
	n->first = p->first;
	n->second = p->second;
	if (n->first.type == tcompo && n->first.val.v_tcompo)
		n->first.val.v_tcompo->refctr++;
	if (n->second.type == tcompo && n->second.val.v_tcompo)
		n->second.val.v_tcompo->refctr++;
	return n;
}

static void tpair_free(void *self)
{
	tpair *p = (tpair *)self;
	tobj_ddc_ref_clear(&p->first);
	tobj_ddc_ref_clear(&p->second);
	free(p);
}

static int tpair_identical(void *self, void *other)
{
	tpair *a = (tpair *)self;
	tpair *b = (tpair *)other;
	return tobj_identical(&a->first, &b->first) &&
	       tobj_identical(&a->second, &b->second);
}

static tstring *tpair_tostring_abbr(void *self)
{
	return tobj_tostring_pointer("Pair", self);
}

static tstring *tpair_tostring_full(void *self)
{
	tpair *p = (tpair *)self;
	tstring *first = tobj_tostring_full(&p->first);
	tstring *second = tobj_tostring_full(&p->second);
	tstring *out = tstring_dup(first);
	tstring_append(out, " : ");
	tstring_append_ts(out, second);
	tstring_free(first);
	tstring_free(second);
	return out;
}

tcompo_vtable tpair_vtable = {
	tpair_get_type,	     tpair_get_code,	 tpair_len,
	tpair_copy,	     tpair_free,	 tpair_identical,
	tpair_tostring_abbr, tpair_tostring_full
};

tpair *tpair_new(const tobj *f, const tobj *s)
{
	tpair *p = (tpair *)calloc(1, sizeof(tpair));
	p->base.vtable = &tpair_vtable;
	p->first = *f;
	p->second = *s;
	if (f->type == tcompo && f->val.v_tcompo)
		f->val.v_tcompo->refctr++;
	if (s->type == tcompo && s->val.v_tcompo)
		s->val.v_tcompo->refctr++;
	return p;
}

void
tpair_idx(tpair *p, const tobj *params, uint_regs np, tobj *vre)
{
	if (np != 1)
		twarn(ErrRuntime_ParamsCtr, "tpair_idx", "");
	if (params[0].type != tint)
		twarn(ErrRuntime_ParamsType, "tpair_idx", "");
	long idx = params[0].val.v_tint;
	if (idx < 0)
		idx += 2;
	if (idx == 0) {
		*vre = p->first;
		if (vre->type == tcompo && vre->val.v_tcompo)
			vre->val.v_tcompo->refctr++;
	} else if (idx == 1) {
		*vre = p->second;
		if (vre->type == tcompo && vre->val.v_tcompo)
			vre->val.v_tcompo->refctr++;
	} else
		twarn(ErrRuntime_IdxOutRange, "tpair_idx", "");
}

void
tpair_iset(tpair *p, const tobj *params, uint_regs np, const tobj *vright)
{
	if (np != 1)
		twarn(ErrRuntime_ParamsCtr, "tpair_iset", "");
	if (params[0].type != tint)
		twarn(ErrRuntime_ParamsType, "tpair_iset", "");
	long idx = params[0].val.v_tint;
	if (idx < 0)
		idx += 2;
	if (idx == 0) {
		if (p->first.type == tcompo && vright->type == tcompo &&
		    p->first.val.v_tcompo == vright->val.v_tcompo)
			return;
		if (vright->type == tcompo)
			vright->val.v_tcompo->refctr++;
		tobj_ddc_ref_clear(&p->first);
		p->first = *vright;
	} else if (idx == 1) {
		if (p->second.type == tcompo && vright->type == tcompo &&
		    p->second.val.v_tcompo == vright->val.v_tcompo)
			return;
		if (vright->type == tcompo)
			vright->val.v_tcompo->refctr++;
		tobj_ddc_ref_clear(&p->second);
		p->second = *vright;
	} else
		twarn(ErrRuntime_IdxOutRange, "tpair_iset", "");
}

/*===========================================================================*
 * 7. Dictionary (tdict)
 *===========================================================================*/

static const char *tdict_get_type(void)
{
	return "Dictionary";
}

static tcompo_type tdict_get_code(void)
{
	return compo_tdict;
}

static long tdict_len(void *self)
{
	tdict *d = (tdict *)self;
	return (long)thashtbl_len(d->items);
}

static void *tdict_copy(void *self)
{
	tdict *d = (tdict *)self;
	tdict *n = (tdict *)calloc(1, sizeof(tdict));
	n->base.vtable = d->base.vtable;
	n->items = thashtbl_copy(d->items);
	return n;
}

static void tdict_free(void *self)
{
	tdict *d = (tdict *)self;
	thashtbl_free(d->items);
	free(d);
}

static int tdict_identical(void *self, void *other)
{
	return self == other;
}

typedef struct {
	tstring *out;
	uint_objs idx;
	uint_objs len;
	int multiline;
} tdict_string_ctx;

static void tdict_string_append_pair(const tobj *key, const tobj *value, void *ctx)
{
	tdict_string_ctx *sctx = (tdict_string_ctx *)ctx;
	tstring *k = tobj_tostring_full(key);
	tstring *v = tobj_tostring_full(value);
	if (sctx->multiline)
		tstring_replace(v, "\n", "\n    ");
	if (sctx->multiline)
		tstring_append(sctx->out, "    ");
	tstring_append_ts(sctx->out, k);
	tstring_append(sctx->out, " : ");
	tstring_append_ts(sctx->out, v);
	tstring_free(k);
	tstring_free(v);
	if (sctx->idx < sctx->len - 1) {
		if (sctx->multiline)
			tstring_append(sctx->out, ",\n");
		else
			tstring_append(sctx->out, ", ");
	}
	sctx->idx++;
}

static void tdict_keys_append(const tobj *key, const tobj *value, void *ctx)
{
	(void)value;
	tlist_push((tlist *)ctx, key);
}

static void tdict_values_append(const tobj *key, const tobj *value, void *ctx)
{
	(void)key;
	tlist_push((tlist *)ctx, value);
}

static tstring *tdict_tostring_abbr(void *self)
{
	return tobj_tostring_pointer("Dictionary", self);
}

static tstring *tdict_tostring_full(void *self)
{
	tdict *d = (tdict *)self;
	uint_objs len = thashtbl_len(d->items);
	tstring *out = tstring_new("{");
	tdict_string_ctx ctx = { out, 0, len, len > 1 };
	if (ctx.multiline)
		tstring_append_c(out, '\n');
	thashtbl_each(d->items, tdict_string_append_pair, &ctx);
	if (ctx.multiline)
		tstring_append_c(out, '\n');
	tstring_append_c(out, '}');
	return out;
}

tcompo_vtable tdict_vtable = {
	tdict_get_type,	     tdict_get_code,	 tdict_len,
	tdict_copy,	     tdict_free,	 tdict_identical,
	tdict_tostring_abbr, tdict_tostring_full
};

tdict *tdict_new(void)
{
	tdict *d = (tdict *)calloc(1, sizeof(tdict));
	d->base.vtable = &tdict_vtable;
	d->items = thashtbl_new();
	return d;
}

void tdict_set(tdict *d, const tobj *key, const tobj *val)
{
	thashtbl_set(d->items, key, val);
}

void tdict_get(tdict *d, const tobj *key, tobj *vre)
{
	const tobj *value = thashtbl_get(d->items, key);
	if (value) {
		*vre = *value;
		if (vre->type == tcompo && vre->val.v_tcompo)
			vre->val.v_tcompo->refctr++;
		return;
	}
	twarn(ErrRuntime_ObjUnfound, "tdict_get", "");
}

int tdict_contains(tdict *d, const tobj *key)
{
	return thashtbl_contains(d->items, key);
}

int tdict_delete(tdict *d, const tobj *key)
{
	return thashtbl_delete(d->items, key);
}

void
tdict_idx(tdict *d, const tobj *params, uint_regs np, tobj *vre)
{
	if (np != 1)
		twarn(ErrRuntime_ParamsCtr, "tdict_idx", "");
	tdict_get(d, &params[0], vre);
}

void
tdict_iset(tdict *d, const tobj *params, uint_regs np, const tobj *vright)
{
	if (np != 1)
		twarn(ErrRuntime_ParamsCtr, "tdict_iset", "");
	tdict_set(d, &params[0], vright);
}

/** Append pair-like values during PUSHDICT */
void tdict_set_append(tdict *d, const tobj *pair_val)
{
	if (pair_val->type != tcompo)
		return;
	tcompo_v *c = pair_val->val.v_tcompo;
	if (c->vtable->get_compo_type_code() != compo_tpair)
		return;
	tpair *p = (tpair *)c;
	tdict_set(d, &p->first, &p->second);
}

/** Get keys as tlist */
tlist *tdict_keys(tdict *d)
{
	tlist *l = tlist_new();
	thashtbl_each(d->items, tdict_keys_append, l);
	return l;
}

tlist *tdict_values(tdict *d)
{
	tlist *l = tlist_new();
	thashtbl_each(d->items, tdict_values_append, l);
	return l;
}

/*===========================================================================*
 * 8. Iterator (titer)
 *===========================================================================*/

static const char *titer_get_type(void)
{
	return "Iterator";
}

static tcompo_type titer_get_code(void)
{
	return compo_titer;
}

static long titer_len(void *self)
{
	titer *it = (titer *)self;
	if (it->step > 0) {
		if (it->start >= it->end)
			return 0;
		return (it->end - it->start + it->step - 1) / it->step;
	}
	if (it->step < 0) {
		long step_abs = -it->step;
		if (it->start <= it->end)
			return 0;
		return (it->start - it->end + step_abs - 1) / step_abs;
	}
	return 0;
}

static void *titer_copy(void *self)
{
	titer *it = (titer *)self;
	titer *n = (titer *)calloc(1, sizeof(titer));
	n->base.vtable = it->base.vtable;
	n->start = it->start;
	n->end = it->end;
	n->step = it->step;
	n->current = it->current;
	return n;
}

static void titer_free(void *self)
{
	free(self);
}

static int titer_identical(void *self, void *other)
{
	titer *a = (titer *)self;
	titer *b = (titer *)other;
	return a->start == b->start && a->end == b->end && a->step == b->step;
}

static tstring *titer_tostring_abbr(void *self)
{
	return tobj_tostring_pointer("Iterator", self);
}

static tstring *titer_tostring_full(void *self)
{
	titer *it = (titer *)self;
	tstring *out = tstring_new_empty();
	tstring_append_fmt(out, "%ld : %ld", it->start, it->end);
	return out;
}

tcompo_vtable titer_vtable = {
	titer_get_type,	     titer_get_code,	 titer_len,
	titer_copy,	     titer_free,	 titer_identical,
	titer_tostring_abbr, titer_tostring_full
};

titer *titer_new_step(long start, long step, long end)
{
	titer *it = (titer *)calloc(1, sizeof(titer));
	it->base.vtable = &titer_vtable;
	it->start = start;
	it->step = step;
	it->end = end;
	it->current = start;
	return it;
}

titer *titer_new(long start, long end)
{
	return titer_new_step(start, 1, end);
}

int titer_next(titer *it)
{
	it->current += it->step;
	return (it->step > 0 && it->current < it->end) ||
	       (it->step < 0 && it->current > it->end);
}

int titer_next_at(titer *it, long *iter_pos, tobj *vre)
{
	long val = it->start + (*iter_pos) * it->step;
	if ((it->step > 0 && val < it->end) ||
	    (it->step < 0 && val > it->end)) {
		tobj_set_int(vre, val);
		(*iter_pos)++;
		return 1;
	}
	*iter_pos = 0;
	return 0;
}

void titer_get_v_at_loc(titer *it, tobj *vre)
{
	tobj_set_int(vre, it->current);
}

void titer_iter_restore(titer *it)
{
	it->current = it->start;
}

int titer_in(titer *it, const tobj *v)
{
	if (v->type != tint)
		return 0;
	long val = v->val.v_tint;
	if (it->step > 0)
		return val >= it->start && val < it->end &&
		       (val - it->start) % it->step == 0;
	return val <= it->start && val > it->end &&
	       (it->start - val) % (-it->step) == 0;
}

/*===========================================================================*
 * 9. C General Function Wrapper (tcppgenf) — uses tstring for name
 *===========================================================================*/

static const char *tcppgenf_get_type(void)
{
	return "C Function";
}

static tcompo_type tcppgenf_get_code(void)
{
	return compo_cppfunc;
}

static long tcppgenf_len(void *self)
{
	(void)self;
	return 0;
}

static void *tcppgenf_copy(void *self)
{
	tcppgenf *g = (tcppgenf *)self;
	tcppgenf *n = (tcppgenf *)calloc(1, sizeof(tcppgenf));
	n->base.vtable = g->base.vtable;
	n->f = g->f;
	n->name = tstring_dup(g->name);
	n->nparams_sig = g->nparams_sig;
	return n;
}

static void tcppgenf_free(void *self)
{
	tcppgenf *g = (tcppgenf *)self;
	tstring_free(g->name);
	free(g);
}

static int tcppgenf_identical(void *self, void *other)
{
	return self == other;
}

static tstring *tcppgenf_tostring_abbr(void *self)
{
	return tobj_tostring_pointer("C Function", self);
}

static tstring *tcppgenf_tostring_full(void *self)
{
	return tobj_tostring_pointer("C Function", self);
}

tcompo_vtable tcppgenf_vtable = {
	tcppgenf_get_type,       tcppgenf_get_code,      tcppgenf_len,
	tcppgenf_copy,           tcppgenf_free,          tcppgenf_identical,
	tcppgenf_tostring_abbr,  tcppgenf_tostring_full
};

tcppgenf *tcppgenf_new(genf_t f, const char *name, uint_regs nparams_sig)
{
	tcppgenf *g = (tcppgenf *)calloc(1, sizeof(tcppgenf));
	g->base.vtable = &tcppgenf_vtable;
	g->f = f;
	g->name = tstring_new(name ? name : "");
	g->nparams_sig = nparams_sig;
	return g;
}

genf_t tcppgenf_get_f(tcppgenf *g)
{
	return g->f;
}

uint_regs tcppgenf_get_nparams_sig(tcppgenf *g)
{
	return g->nparams_sig;
}

tcompo_type tobj_compo_type(const tobj *v)
{
	if (v->type == tcompo && v->val.v_tcompo && v->val.v_tcompo->vtable)
		return v->val.v_tcompo->vtable->get_compo_type_code();
	return (tcompo_type)(-1);
}

const char *tobj_compo_type_name(const tobj *v)
{
	if (v->type == tcompo && v->val.v_tcompo && v->val.v_tcompo->vtable)
		return v->val.v_tcompo->vtable->get_type();
	return "Unknown";
}
