#include "tapas/dsa/tstring.h"
#include "thashtbl_internal.h"
#include "tblockpool.h"
#include "tapas/objects/tstr.h"
#include "tapas/objects/ttype.h"
#include "tapas/tval.h"

#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#define THASH_MIN_CAP 4
#define THASH_DEFAULT_CAP 16
#define THASH_MAX_CAP (1u << 30)

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
	uint64_t hash;
	switch (key->type) {
	case tnil:
		hash = thash_mix(0x10);
		break;
	case tbool:
		hash = thash_mix(0x20 ^ (uint64_t)key->val.v_tbool);
		break;
	case tint:
		/* Same computation as thashtbl_hash_int(); keep both in sync. */
		hash = thashtbl_hash_int((uint64_t)key->val.v_tint);
		break;
	case tfloat: {
		double v = key->val.v_tfloat;
		uint64_t bits = 0;
		if (v == 0.0)
			v = 0.0;
		memcpy(&bits, &v, sizeof(bits));
		hash = thash_mix(0x40 ^ bits);
		break;
	}
	case tcompo:
		if (key->val.v_tcompo &&
		    key->val.v_tcompo->vtable &&
		    key->val.v_tcompo->vtable->compo_code == compo_tstr) {
			tstr *s = (tstr *)key->val.v_tcompo;
			/* Strings are immutable, so the content hash is cached on
			 * first use and stays valid for the object's lifetime. */
			if (!s->hash)
				s->hash = tstring_hash(s->data);
			hash = thash_mix(0x50 ^ s->hash);
			break;
		}
		if (key->val.v_tcompo && key->val.v_tcompo->vtable &&
		    key->val.v_tcompo->vtable->compo_code ==
			    compo_ttypeval) {
			hash = thash_mix(
				0x70 ^ ttypeval_hash((ttypeval *)key->val.v_tcompo));
			break;
		}
		hash = thash_mix(0x60 ^ (uintptr_t)key->val.v_tcompo);
		break;
	default:
		hash = thash_mix(0xff);
		break;
	}
	return hash < 2 ? hash + 2 : hash;
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
	if (a->val.v_tcompo->vtable->compo_code == compo_tstr &&
	    b->val.v_tcompo->vtable->compo_code == compo_tstr)
		return tobj_identical(a, b);
	if (a->val.v_tcompo->vtable->compo_code == compo_ttypeval &&
	    b->val.v_tcompo->vtable->compo_code == compo_ttypeval)
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

static uint_count thash_next_cap(uint_count cap)
{
	if (cap < THASH_MIN_CAP)
		return THASH_MIN_CAP;
	if (cap >= THASH_MAX_CAP)
		twarn(ErrRuntime_Other, "thash_next_cap", "hash table is too large");
	return (uint_count)(cap * 2);
}

static void thashtbl_alloc_entries(thashtbl *tbl, uint_count capacity)
{
	tbl->entries = (thash_entry *)tblockpool_calloc(
		capacity * sizeof(thash_entry));
	if (!tbl->entries)
		twarn(ErrRuntime_Other, "thashtbl_alloc_entries", "out of memory");
	tbl->capacity = capacity;
	tbl->len = 0;
	tbl->used = 0;
}

thashtbl *thashtbl_new(void)
{
	thashtbl *tbl = (thashtbl *)tblockpool_calloc(sizeof(thashtbl));
	return tbl;
}

thashtbl *thashtbl_new_sized(uint_count expected_items)
{
	thashtbl *tbl = thashtbl_new();
	if (!expected_items)
		return tbl;
	uint_count capacity = THASH_MIN_CAP;
	/* Use the same load limit as the insertion path. This is a capacity hint,
	 * not a separately tuned literal-only threshold. */
	while ((uint64_t)expected_items * THASH_LOAD_DEN >=
	       (uint64_t)capacity * THASH_LOAD_NUM)
		capacity = thash_next_cap(capacity);
	thashtbl_alloc_entries(tbl, capacity);
	return tbl;
}

static void thashtbl_clear_entries(thash_entry *entries, uint_count capacity)
{
	for (uint_count i = 0; i < capacity; i++) {
		if (entries[i].hash < 2)
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
		tblockpool_free(tbl->entries, tbl->capacity * sizeof(thash_entry));
	}
	tblockpool_free(tbl, sizeof(thashtbl));
}

uint_count thashtbl_len(const thashtbl *tbl)
{
	return tbl ? tbl->len : 0;
}

static uint_count thashtbl_find_slot(const thashtbl *tbl,
				    const tobj *key,
				    uint64_t hash,
				    int *found)
{
	uint_count mask = tbl->capacity - 1;
	uint_count idx = (uint_count)(hash & mask);
	uint_count first_deleted = tbl->capacity;

	for (;;) {
		thash_entry *entry = &tbl->entries[idx];
		if (entry->hash == 0) {
			*found = 0;
			return first_deleted != tbl->capacity ? first_deleted : idx;
		}
		if (entry->hash == 1) {
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
const tobj *thashtbl_find(const thashtbl *tbl, const tobj *key,
			   uint_count *slot)
{
	if (!tbl || !key || tbl->capacity == 0) {
		if (slot) *slot = 0;
		return nullptr;
	}
	uint64_t hash = thash_tobj_hash(key);
	int found = 0;
	uint_count idx = thashtbl_find_slot(tbl, key, hash, &found);
	if (slot) *slot = idx;
	return found ? &tbl->entries[idx].value : nullptr;
}

const tobj *thashtbl_get_entry_at(const thashtbl *tbl, uint_count slot,
				  const tobj *key)
{
	if (!tbl || slot >= tbl->capacity)
		return nullptr;
	const thash_entry *entry = &tbl->entries[slot];
	/* Interned keys hit the pointer fast path; key equality implies the
	 * stored hash still matches, so the entry stays valid after rehashes
	 * and tombstones without recomputing the key hash. */
	if (entry->hash < 2)
		return nullptr;
	if (key->type == tcompo) {
		if (entry->key.val.v_tcompo == key->val.v_tcompo)
			return &entry->value;
		return thash_tobj_key_eq(&entry->key, key) ?
			&entry->value : nullptr;
	}
	return thash_tobj_key_eq(&entry->key, key) ? &entry->value : nullptr;
}

uint_count thashtbl_capacity(const thashtbl *tbl)
{
	return tbl ? tbl->capacity : 0;
}


static void thashtbl_move_entry(thashtbl *tbl, thash_entry *src)
{
	uint_count mask = tbl->capacity - 1;
	uint_count idx = (uint_count)(src->hash & mask);
	while (tbl->entries[idx].hash != 0)
		idx = (idx + 1) & mask;
	thash_entry *dst = &tbl->entries[idx];
	*dst = *src;
	tbl->used++;
	tbl->len++;
}

static void thashtbl_rehash(thashtbl *tbl, uint_count newcap)
{
	thash_entry *old_entries = tbl->entries;
	uint_count old_capacity = tbl->capacity;

	thashtbl_alloc_entries(tbl, newcap);

	for (uint_count i = 0; i < old_capacity; i++) {
		if (old_entries[i].hash < 2)
			continue;
		thashtbl_move_entry(tbl, &old_entries[i]);
	}
	tblockpool_free(old_entries, old_capacity * sizeof(thash_entry));
}

static void thashtbl_ensure_room(thashtbl *tbl)
{
	if (tbl->capacity == 0) {
		thashtbl_alloc_entries(tbl, THASH_DEFAULT_CAP);
		return;
	}
	if ((tbl->len + 1) * THASH_LOAD_DEN >=
	    tbl->capacity * THASH_LOAD_NUM)
		thashtbl_rehash(tbl, thash_next_cap(tbl->capacity));
	else if ((tbl->used + 1) * THASH_LOAD_DEN >=
		 tbl->capacity * THASH_LOAD_NUM)
		thashtbl_rehash(tbl, tbl->capacity);
}

uint_count thashtbl_set(thashtbl *tbl, const tobj *key, const tobj *value)
{
	if (!tbl || !key || !value)
		return 0;
	if (tbl->capacity == 0)
		thashtbl_alloc_entries(tbl, THASH_DEFAULT_CAP);
	uint64_t hash = thash_tobj_hash(key);
	int found = 0;
	uint_count idx = thashtbl_find_slot(tbl, key, hash, &found);
	if (found) {
		thash_entry *entry = &tbl->entries[idx];
		if (!thash_same_stored_value(&entry->value, value)) {
			thash_retain(value);
			tobj_ddc_ref_clear(&entry->value);
			entry->value = *value;
		}
		return idx;
	}
	thash_entry *old_entries = tbl->entries;
	thashtbl_ensure_room(tbl);
	if (tbl->entries != old_entries) {
		found = 0;
		idx = thashtbl_find_slot(tbl, key, hash, &found);
	}
	thash_entry *entry = &tbl->entries[idx];
	if (entry->hash == 0)
		tbl->used++;
	entry->key = *key;
	entry->value = *value;
	entry->hash = hash;
	thash_retain(key);
	thash_retain(value);
	tbl->len++;
	return idx;
}

int thashtbl_set_at(thashtbl *tbl, uint_count slot, const tobj *key,
		    const tobj *value)
{
	if (!tbl || slot >= tbl->capacity)
		return 0;
	thash_entry *entry = &tbl->entries[slot];
	if (entry->hash < 2)
		return 0;
	if (key->type == tcompo &&
	    entry->key.val.v_tcompo == key->val.v_tcompo) {
		/* pointer hit */
	} else if (!thash_tobj_key_eq(&entry->key, key))
		return 0;
	if (thash_same_stored_value(&entry->value, value))
		return 1;
	thash_retain(value);
	tobj_ddc_ref_clear(&entry->value);
	entry->value = *value;
	return 1;
}

const tobj *thashtbl_get(const thashtbl *tbl, const tobj *key)
{
	if (!tbl || tbl->capacity == 0)
		return nullptr;
	int found = 0;
	uint64_t hash = thash_tobj_hash(key);
	uint_count idx = thashtbl_find_slot(tbl, key, hash, &found);
	return found ? &tbl->entries[idx].value : nullptr;
}

int thashtbl_contains(const thashtbl *tbl, const tobj *key)
{
	return thashtbl_get(tbl, key) != nullptr;
}

int thashtbl_delete(thashtbl *tbl, const tobj *key)
{
	if (!tbl || tbl->capacity == 0)
		return 0;
	int found = 0;
	uint64_t hash = thash_tobj_hash(key);
	uint_count idx = thashtbl_find_slot(tbl, key, hash, &found);
	if (!found)
		return 0;
	tobj_ddc_ref_clear(&tbl->entries[idx].key);
	tobj_ddc_ref_clear(&tbl->entries[idx].value);
	tbl->entries[idx].hash = 1;
	tbl->len--;
	/* Leave tombstone compaction to the insertion path. Rehashing after a
	 * deletion makes bulk erasure repeatedly move entries that are about to
	 * be deleted as well. Release the backing array when the table becomes
	 * empty so the next insertion starts from a clean default-size table. */
	if (tbl->len == 0) {
		tblockpool_free(tbl->entries, tbl->capacity * sizeof(thash_entry));
		tbl->entries = nullptr;
		tbl->used = 0;
		tbl->capacity = 0;
	}
	return 1;
}

thashtbl *thashtbl_copy(const thashtbl *tbl)
{
	thashtbl *copy = thashtbl_new();
	if (!tbl || tbl->len == 0)
		return copy;
	thashtbl_alloc_entries(copy, tbl->capacity);
	uint_count mask = copy->capacity - 1;
	for (uint_count i = 0; i < tbl->capacity; i++) {
		if (tbl->entries[i].hash < 2)
			continue;
		const thash_entry *src = &tbl->entries[i];
		uint_count slot = (uint_count)(src->hash & mask);
		while (copy->entries[slot].hash != 0)
			slot = (slot + 1) & mask;
		copy->entries[slot] = *src;
		thash_retain(&copy->entries[slot].key);
		thash_retain(&copy->entries[slot].value);
		copy->len++;
		copy->used++;
	}
	return copy;
}

void thashtbl_each(const thashtbl *tbl, thashtbl_each_fn fn, void *ctx)
{
	if (!tbl || !fn)
		return;
	for (uint_count i = 0; i < tbl->capacity; i++) {
		if (tbl->entries[i].hash < 2)
			continue;
		fn(&tbl->entries[i].key, &tbl->entries[i].value, ctx);
	}
}
