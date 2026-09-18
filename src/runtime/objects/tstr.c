#include "tapas/objects/tstr.h"
#include "tapas/dsa/tstring.h"

#include "tapas/objects/tpair.h"

#include <stdlib.h>
#include <string.h>

/* Strings are immutable, so two mechanisms make the common short-lived
 * cases cheap: a freelist recycles fixed-size objects (as titer/tpair do),
 * and a canonical byte table makes every one-byte string a shared object,
 * so dictionary keys built from reads and slices compare by pointer. */

#define TSTR_POOL_MAX 256u
static _Thread_local tstr *tstr_pool;
static _Thread_local unsigned tstr_pool_len;

static tstr *tstr_byte_canonical[256];

static tstr *tstr_alloc_raw(const char *text, size_t length)
{
	tstr *string = tstr_pool;
	if (string) {
		memcpy(&tstr_pool, &string->base.vtable, sizeof(tstr_pool));
		tstr_pool_len--;
	} else {
		string = (tstr *)malloc(sizeof(tstr));
		if (!string)
			return nullptr;
	}
	string->base.vtable = &tstr_vtable;
	string->base.refctr = 0;
	string->data = &string->storage;
	string->hash = 0;
	if (!tstring_init_len(string->data, text, length)) {
		free(string);
		return nullptr;
	}
	return string;
}

/* Canonical objects live for the process: they are created with one
 * baseline reference and their free is a no-op. */
static tstr *tstr_byte_string(unsigned char byte)
{
	tstr *string = tstr_byte_canonical[byte];
	if (string)
		return string;
	string = tstr_alloc_raw((const char *)&byte, 1);
	if (!string)
		return nullptr;
	string->hash = tstring_hash(string->data);
	string->base.refctr = 1;
	tstr_byte_canonical[byte] = string;
	return string;
}

tstr *tstr_new_len(const char *text, size_t length)
{
	if (length == 1)
		return tstr_byte_string((unsigned char)text[0]);
	return tstr_alloc_raw(text, length);
}

tstr *tstr_new(const char *text)
{
	return tstr_new_len(text ? text : "", text ? strlen(text) : 0);
}

tstr *tstr_new_interned(const char *text)
{
	tstr *string = tstr_new(text);
	if (string) {
		string->hash = tstring_hash(string->data);
		string->base.refctr = 1;
	}
	return string;
}

/*----------------------- Required Vtable Operations -----------------------*/

static const char *tstr_get_type(void)
{
	return "String";
}


static long tstr_len(void *self)
{
	tstr *s = (tstr *)self;
	return (long)tstring_len(s->data);
}

static void *tstr_copy(void *self)
{
	tstr *s = (tstr *)self;
	return tstr_new_len(tstring_cstr(s->data), tstring_len(s->data));
}

static void tstr_free(void *self)
{
	tstr *s = (tstr *)self;
	/* Canonical byte strings are immortal. */
	if (tstring_len(s->data) == 1 &&
	    tstr_byte_canonical[(unsigned char)tstring_cstr(s->data)[0]] == s)
		return;
	tstring_deinit(&s->storage);
	if (tstr_pool_len < TSTR_POOL_MAX) {
		memcpy(&s->base.vtable, &tstr_pool, sizeof(tstr_pool));
		tstr_pool = s;
		tstr_pool_len++;
		return;
	}
	free(s);
}

static int tstr_identical(void *self, void *other)
{
	tstr *a = (tstr *)self;
	tstr *b = (tstr *)other;
	if (a == b)
		return 1;
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

/*------------------------------ Capabilities ------------------------------*/

/*
 * String is immutable: it supports indexed reads, including Pair ranges,
 * but no in-place writes or appends. Composition goes through join() and
 * friends, which allocate fresh strings. It is not Iterable because Tapas
 * has not defined byte, code-point, or grapheme iteration semantics.
 */

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


/* String indexing helper */
void tstr_slice_index(tstr *s, const tobj *params, tobj *vre)
{
	/* params[0] is the end bound (pushed first), params[1] the start. */
	long first = params[1].val.v_tint;
	long last = params[0].val.v_tint;
	long len = (long)tstring_len(s->data);
	if (first < 0)
		first += len;
	if (last < 0)
		last += len;
	if (first < 0 || last < first || last > len)
		twarn(ErrRuntime_IdxOutRange, "pair_to_range", "");
	tobj_set_compo(
		vre,
		(tcompo_v *)tstr_new_len(tstring_cstr(s->data) + first,
					 (size_t)(last - first)));
}

static void
tstr_idx(tstr *s, const tobj *params, uint_regs np, tobj *vre)
{
	if (np != 1)
		twarn(ErrRuntime_ParamsCtr, "tstr_idx", "");
	long start, end;
	if (pair_to_range(&params[0],
			  (long)tstring_len(s->data),
			  &start,
			  &end)) {
		tobj_set_compo(
			vre,
			(tcompo_v *)tstr_new_len(tstring_cstr(s->data) + start,
						 (size_t)(end - start)));
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
	tobj_set_compo(
		vre,
		(tcompo_v *)tstr_new_len(tstring_cstr(s->data) + idx, 1));
}

static void string_index(void *self, const tobj *arguments,
			 uint_regs argument_count, tobj *result)
{
	tstr_idx((tstr *)self, arguments, argument_count, result);
}


static const tcompo_capabilities string_capabilities = {
	.indexable = string_index
};

tcompo_vtable tstr_vtable = {
	.get_type = tstr_get_type,
	.compo_code = compo_tstr,
	.len = tstr_len,
	.copy = tstr_copy,
	.free = tstr_free,
	.identical = tstr_identical,
	.tostring_abbr = tstr_tostring_abbr,
	.tostring_full = tstr_tostring_full,
	.capabilities = &string_capabilities
};
