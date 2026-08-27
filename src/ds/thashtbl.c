#include "tapas/ds/thashtbl.h"
#include "tapas/runtime/tstr.h"
#include "tapas/runtime/ttype.h"
#include "tapas/tval.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define THASH_MIN_CAP 16
#define THASH_MAX_CAP 32768

typedef enum {
	THASH_EMPTY = 0,
	THASH_USED,
	THASH_DELETED
} thash_state;

typedef struct {
	tobj key;
	tobj value;
	uint64_t hash;
	thash_state state;
} thash_entry;

struct thashtbl {
	thash_entry *entries;
	uint_objs len;
	uint_objs used;
	uint_objs capacity;
};

static uint64_t thash_mix(uint64_t x)
{
	x ^= x >> 30;
	x *= 0xbf58476d1ce4e5b9ULL;
	x ^= x >> 27;
	x *= 0x94d049bb133111ebULL;
	x ^= x >> 31;
	return x;
}

static uint64_t thash_tobj_hash(const tobj *key)
{
	switch (key->type) {
	case tnil:
		return thash_mix(0x10);
	case tbool:
		return thash_mix(0x20 ^ (uint64_t)key->val.v_tbool);
	case tint:
		return thash_mix(0x30 ^ (uint64_t)key->val.v_tint);
	case tfloat: {
		double v = key->val.v_tfloat;
		uint64_t bits = 0;
		if (v == 0.0)
			v = 0.0;
		memcpy(&bits, &v, sizeof(bits));
		return thash_mix(0x40 ^ bits);
	}
	case tcompo:
		if (key->val.v_tcompo &&
		    key->val.v_tcompo->vtable &&
		    key->val.v_tcompo->vtable->get_compo_type_code() == compo_tstr) {
			tstr *s = (tstr *)key->val.v_tcompo;
			return thash_mix(0x50 ^ tstring_hash(s->data));
		}
		if (key->val.v_tcompo && key->val.v_tcompo->vtable &&
		    key->val.v_tcompo->vtable->get_compo_type_code() ==
			    compo_ttypeval)
			return thash_mix(
				0x70 ^ ttypeval_hash((ttypeval *)key->val.v_tcompo));
		return thash_mix(0x60 ^ (uintptr_t)key->val.v_tcompo);
	}
	return thash_mix(0xff);
}

static int thash_tobj_key_eq(const tobj *a, const tobj *b)
{
	if (a->type != b->type)
		return 0;
	if (a->type != tcompo)
		return tobj_identical(a, b);
	if (a->val.v_tcompo == b->val.v_tcompo)
		return 1;
	if (!a->val.v_tcompo || !b->val.v_tcompo ||
	    !a->val.v_tcompo->vtable || !b->val.v_tcompo->vtable)
		return 0;
	if (a->val.v_tcompo->vtable->get_compo_type_code() == compo_tstr &&
	    b->val.v_tcompo->vtable->get_compo_type_code() == compo_tstr)
		return tobj_identical(a, b);
	if (a->val.v_tcompo->vtable->get_compo_type_code() == compo_ttypeval &&
	    b->val.v_tcompo->vtable->get_compo_type_code() == compo_ttypeval)
		return ttypeval_equal((ttypeval *)a->val.v_tcompo,
				      (ttypeval *)b->val.v_tcompo);
	return 0;
}

static void thash_retain(const tobj *obj)
{
	if (obj->type == tcompo && obj->val.v_tcompo)
		obj->val.v_tcompo->refctr++;
}

static int thash_same_stored_value(const tobj *a, const tobj *b)
{
	if (a->type != b->type)
		return 0;
	if (a->type == tcompo)
		return a->val.v_tcompo == b->val.v_tcompo;
	return tobj_identical(a, b);
}

static uint_objs thash_next_cap(uint_objs cap)
{
	if (cap < THASH_MIN_CAP)
		return THASH_MIN_CAP;
	if (cap >= THASH_MAX_CAP)
		twarn(ErrRuntime_Other, "thash_next_cap", "hash table is too large");
	return (uint_objs)(cap * 2);
}

static void thashtbl_alloc_entries(thashtbl *tbl, uint_objs capacity)
{
	tbl->entries = (thash_entry *)calloc(capacity, sizeof(thash_entry));
	if (!tbl->entries)
		twarn(ErrRuntime_Other, "thashtbl_alloc_entries", "out of memory");
	tbl->capacity = capacity;
	tbl->len = 0;
	tbl->used = 0;
}

thashtbl *thashtbl_new(void)
{
	thashtbl *tbl = (thashtbl *)calloc(1, sizeof(thashtbl));
	return tbl;
}

static void thashtbl_clear_entries(thash_entry *entries, uint_objs capacity)
{
	for (uint_objs i = 0; i < capacity; i++) {
		if (entries[i].state != THASH_USED)
			continue;
		tobj_ddc_ref_clear(&entries[i].key);
		tobj_ddc_ref_clear(&entries[i].value);
	}
}

void thashtbl_free(thashtbl *tbl)
{
	if (!tbl)
		return;
	if (tbl->entries) {
		thashtbl_clear_entries(tbl->entries, tbl->capacity);
		free(tbl->entries);
	}
	free(tbl);
}

uint_objs thashtbl_len(const thashtbl *tbl)
{
	return tbl ? tbl->len : 0;
}

static uint_objs thashtbl_find_slot(const thashtbl *tbl,
				    const tobj *key,
				    uint64_t hash,
				    int *found)
{
	uint_objs mask = tbl->capacity - 1;
	uint_objs idx = (uint_objs)(hash & mask);
	uint_objs first_deleted = tbl->capacity;

	for (;;) {
		thash_entry *entry = &tbl->entries[idx];
		if (entry->state == THASH_EMPTY) {
			*found = 0;
			return first_deleted != tbl->capacity ? first_deleted : idx;
		}
		if (entry->state == THASH_DELETED) {
			if (first_deleted == tbl->capacity)
				first_deleted = idx;
		} else if (entry->hash == hash &&
			   thash_tobj_key_eq(&entry->key, key)) {
			*found = 1;
			return idx;
		}
		idx = (idx + 1) & mask;
	}
}

static void thashtbl_place(thashtbl *tbl,
			   const tobj *key,
			   const tobj *value,
			   uint64_t hash)
{
	int found = 0;
	uint_objs idx = thashtbl_find_slot(tbl, key, hash, &found);
	thash_entry *entry = &tbl->entries[idx];

	if (found) {
		if (thash_same_stored_value(&entry->value, value))
			return;
		thash_retain(value);
		tobj_ddc_ref_clear(&entry->value);
		entry->value = *value;
		return;
	}

	if (entry->state == THASH_EMPTY)
		tbl->used++;
	entry->key = *key;
	entry->value = *value;
	entry->hash = hash;
	entry->state = THASH_USED;
	thash_retain(key);
	thash_retain(value);
	tbl->len++;
}

static void thashtbl_move_entry(thashtbl *tbl, thash_entry *src)
{
	int found = 0;
	uint_objs idx = thashtbl_find_slot(tbl, &src->key, src->hash, &found);
	thash_entry *dst = &tbl->entries[idx];
	if (dst->state == THASH_EMPTY)
		tbl->used++;
	*dst = *src;
	dst->state = THASH_USED;
	tbl->len++;
}

static void thashtbl_rehash(thashtbl *tbl, uint_objs newcap)
{
	thash_entry *old_entries = tbl->entries;
	uint_objs old_capacity = tbl->capacity;

	thashtbl_alloc_entries(tbl, newcap);

	for (uint_objs i = 0; i < old_capacity; i++) {
		if (old_entries[i].state != THASH_USED)
			continue;
		thashtbl_move_entry(tbl, &old_entries[i]);
	}
	free(old_entries);
}

static void thashtbl_ensure_room(thashtbl *tbl)
{
	if (tbl->capacity == 0) {
		thashtbl_alloc_entries(tbl, THASH_MIN_CAP);
		return;
	}
	if ((tbl->len + 1) * 10 >= tbl->capacity * 7)
		thashtbl_rehash(tbl, thash_next_cap(tbl->capacity));
	else if ((tbl->used + 1) * 10 >= tbl->capacity * 7)
		thashtbl_rehash(tbl, tbl->capacity);
}

static void thashtbl_maybe_compact_after_delete(thashtbl *tbl)
{
	uint_objs tombstones;
	if (!tbl || tbl->capacity == 0)
		return;
	tombstones = tbl->used - tbl->len;
	if (tbl->capacity > THASH_MIN_CAP && tbl->len * 10 < tbl->capacity * 2) {
		uint_objs newcap = tbl->capacity / 2;
		while (newcap > THASH_MIN_CAP && tbl->len * 10 < newcap * 2)
			newcap /= 2;
		thashtbl_rehash(tbl, newcap);
	} else if (tombstones > tbl->len && tombstones > 8) {
		thashtbl_rehash(tbl, tbl->capacity);
	}
}

void thashtbl_set(thashtbl *tbl, const tobj *key, const tobj *value)
{
	uint64_t hash;
	thashtbl_ensure_room(tbl);
	hash = thash_tobj_hash(key);
	thashtbl_place(tbl, key, value, hash);
}

const tobj *thashtbl_get(const thashtbl *tbl, const tobj *key)
{
	if (!tbl || tbl->capacity == 0)
		return NULL;
	int found = 0;
	uint64_t hash = thash_tobj_hash(key);
	uint_objs idx = thashtbl_find_slot(tbl, key, hash, &found);
	return found ? &tbl->entries[idx].value : NULL;
}

int thashtbl_contains(const thashtbl *tbl, const tobj *key)
{
	return thashtbl_get(tbl, key) != NULL;
}

int thashtbl_delete(thashtbl *tbl, const tobj *key)
{
	if (!tbl || tbl->capacity == 0)
		return 0;
	int found = 0;
	uint64_t hash = thash_tobj_hash(key);
	uint_objs idx = thashtbl_find_slot(tbl, key, hash, &found);
	if (!found)
		return 0;
	tobj_ddc_ref_clear(&tbl->entries[idx].key);
	tobj_ddc_ref_clear(&tbl->entries[idx].value);
	tbl->entries[idx].hash = 0;
	tbl->entries[idx].state = THASH_DELETED;
	tbl->len--;
	thashtbl_maybe_compact_after_delete(tbl);
	return 1;
}

thashtbl *thashtbl_copy(const thashtbl *tbl)
{
	thashtbl *copy = thashtbl_new();
	if (!tbl || tbl->len == 0)
		return copy;
	thashtbl_rehash(copy, tbl->capacity);
	for (uint_objs i = 0; i < tbl->capacity; i++) {
		if (tbl->entries[i].state != THASH_USED)
			continue;
		thashtbl_set(copy, &tbl->entries[i].key, &tbl->entries[i].value);
	}
	return copy;
}

void thashtbl_each(const thashtbl *tbl, thashtbl_each_fn fn, void *ctx)
{
	if (!tbl || !fn)
		return;
	for (uint_objs i = 0; i < tbl->capacity; i++) {
		if (tbl->entries[i].state != THASH_USED)
			continue;
		fn(&tbl->entries[i].key, &tbl->entries[i].value, ctx);
	}
}
