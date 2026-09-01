#ifndef TAPAS_DS_TSTRING_H
#define TAPAS_DS_TSTRING_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================*
 * tstring - C++-like String for C
 *
 * A dynamic, length-cached string type inspired by C++ std::string.
 * Supports copy-on-assign semantics with heap-allocated storage.
 *
 * Unlike raw char*, tstring caches its length for O(1) strlen equivalent,
 * supports efficient concatenation via capacity-based growth, and provides
 * a rich set of comparison, search, and manipulation primitives.
 *===========================================================================*/

#define TSTRING_INLINE_CAP 16

typedef struct tstring {
	char *data;
	size_t len;
	size_t cap;
	char inline_data[TSTRING_INLINE_CAP];
} tstring;

/* ---- Constructors & Destructor ---- */

/** Initialize caller-owned tstring storage from a C string. */
bool tstring_init(tstring *s, const char *data);

/** Initialize caller-owned tstring storage from a length-delimited buffer. */
bool tstring_init_len(tstring *s, const char *data, size_t len);

/** Release a tstring's dynamic buffer without freeing the tstring itself. */
void tstring_deinit(tstring *s);

/** Create a tstring from a null-terminated C string. */
tstring *tstring_new(const char *s);

/** Create a tstring from a buffer with explicit length. */
tstring *tstring_new_len(const char *s, size_t len);

/** Create a tstring with pre-allocated capacity. */
tstring *tstring_new_cap(size_t cap);

/** Create an empty tstring. */
tstring *tstring_new_empty(void);

/** Deep-copy a tstring. */
tstring *tstring_dup(const tstring *s);

/** Free a tstring and its data. */
void tstring_free(tstring *s);

/* ---- Accessors ---- */

/** Return the underlying null-terminated C string. */
const char *tstring_cstr(const tstring *s);

/** Return the length in bytes (O(1)). */
size_t tstring_len(const tstring *s);

/** Return the allocated capacity. */
size_t tstring_cap(const tstring *s);

/** Return true if the string is empty. */
bool tstring_empty(const tstring *s);

/** Get the character at index i. Behaviour is undefined if i >= len. */
char tstring_at(const tstring *s, size_t i);

/* ---- Assignment ---- */

/** Replace contents with a C string. */
void tstring_assign(tstring *s, const char *str);

/** Replace contents with a length-delimited string. */
void tstring_assign_len(tstring *s, const char *str, size_t len);

/** Copy from another tstring. */
void tstring_assign_ts(tstring *dst, const tstring *src);

/* ---- Appending ---- */

/** Append a null-terminated C string. */
void tstring_append(tstring *s, const char *str);

/** Append a length-delimited string. */
void tstring_append_len(tstring *s, const char *str, size_t len);

/** Append a single character. */
void tstring_append_c(tstring *s, char c);

/** Append another tstring. */
void tstring_append_ts(tstring *s, const tstring *other);

/** Append a formatted string (printf-style). */
void tstring_append_fmt(tstring *s, const char *fmt, ...)
	__attribute__((format(printf, 2, 3)));

/* ---- Comparison ---- */

/** Lexicographical comparison. Returns <0, 0, >0 like strcmp. */
int tstring_cmp(const tstring *a, const tstring *b);

/** Compare tstring against a C string. */
int tstring_cmp_cstr(const tstring *a, const char *b);

/** Compare two C strings via tstring wrapper (convenience). */
int tstring_cmp_cstr_cstr(const char *a, const char *b);

/** Equality check. */
bool tstring_eq(const tstring *a, const tstring *b);

/** Equality check against C string. */
bool tstring_eq_cstr(const tstring *a, const char *b);

/** Equality check between two C strings. */
bool tstring_eq_cstr_cstr(const char *a, const char *b);

/** Compare first n characters. */
bool tstring_eq_n(const tstring *a, const tstring *b, size_t n);

/** Compare first n characters against C string. */
bool tstring_eq_n_cstr(const tstring *a, const char *b, size_t n);

/* ---- Search ---- */

/** Find first occurrence of c. Returns index or SIZE_MAX if not found. */
size_t tstring_find_c(const tstring *s, char c, size_t pos);

/** Find last occurrence of c. Returns index or SIZE_MAX if not found. */
size_t tstring_rfind_c(const tstring *s, char c);

/** Find first occurrence of substring. Returns index or SIZE_MAX. */
size_t tstring_find(const tstring *s, const char *sub, size_t pos);

/** Check if s contains sub. */
bool tstring_contains(const tstring *s, const char *sub);

/** Check if s starts with prefix. */
bool tstring_starts_with(const tstring *s, const char *prefix);

/** Check if s ends with suffix. */
bool tstring_ends_with(const tstring *s, const char *suffix);

/* ---- Substring / Extraction ---- */

/** Extract a substring [pos, pos+len). Returns new tstring. */
tstring *tstring_substr(const tstring *s, size_t pos, size_t len);

/** Copy content into a caller-provided buffer. Returns bytes written. */
size_t tstring_copy_to(const tstring *s, char *buf, size_t bufsz);

/* ---- Modification ---- */

/** Remove trailing whitespace, newlines. Modifies in-place. */
void tstring_trim_back(tstring *s);

/** Remove leading whitespace. Modifies in-place. */
void tstring_trim_front(tstring *s);

/** Remove leading and trailing whitespace. Modifies in-place. */
void tstring_trim(tstring *s);

/** Clear to empty string. */
void tstring_clear(tstring *s);

/** Reserve at least cap bytes of storage. */
void tstring_reserve(tstring *s, size_t cap);

/** Shrink capacity to fit current length. */
void tstring_shrink_to_fit(tstring *s);

/** Remove the first character (pop front). Returns the removed char. */
char tstring_pop_front(tstring *s);

/** Remove the last character (pop back). Returns the removed char. */
char tstring_pop_back(tstring *s);

/** Replace all occurrences of 'from' with 'to'. Modifies in-place. */
void tstring_replace(tstring *s, const char *from, const char *to);

/** Replace the character at index i with c. Index must be < len. */
void tstring_set_at(tstring *s, size_t i, char c);

/* ---- Iteration helpers ---- */

/** Return a pointer to the first character (same as data, for iterator-style use). */
char *tstring_begin(tstring *s);

/** Return a pointer to one past the last character. */
char *tstring_end(tstring *s);

/* ---- IO ---- */

/** Print to FILE. */
void tstring_print(const tstring *s, FILE *fp);

/** Print with trailing newline to FILE. */
void tstring_println(const tstring *s, FILE *fp);

/** Read a line from FILE into tstring. Returns chars read, or SIZE_MAX on EOF/error. */
size_t tstring_getline(tstring *s, FILE *fp);

/* ---- Conversion ---- */

/** Convert to long int. Returns 0 on success, non-zero on failure. */
int tstring_to_long(const tstring *s, long *out);

/** Convert to double. Returns 0 on success, non-zero on failure. */
int tstring_to_double(const tstring *s, double *out);

/* ---- Hash ---- */

/** Simple hash (djb2) for use in hash tables. */
uint64_t tstring_hash(const tstring *s);

#ifdef __cplusplus
}
#endif

#endif /* TAPAS_DS_TSTRING_H */
