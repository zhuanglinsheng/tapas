#include "tapas/ds/tstring.h"

#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

/* ---- Internal helpers ---- */

#define TSTRING_MIN_CAP 16

static void tstring_grow(tstring *s, size_t needed)
{
	if (needed <= s->cap)
		return;
	size_t newcap = s->cap ? s->cap * 2 : TSTRING_MIN_CAP;
	if (newcap < s->cap)
		newcap = needed;
	while (newcap < needed) {
		if (newcap > SIZE_MAX / 2) {
			newcap = needed;
			break;
		}
		newcap *= 2;
	}
	char *newdata = (char *)realloc(s->data, newcap);
	if (!newdata)
		return; /* allocation failure — keep old data */
	s->data = newdata;
	s->cap = newcap;
}

static bool tstring_range_overlaps(const char *a, size_t alen, const char *b, size_t blen)
{
	uintptr_t abeg = (uintptr_t)a;
	uintptr_t bbeg = (uintptr_t)b;
	uintptr_t aend = abeg + alen;
	uintptr_t bend = bbeg + blen;
	return abeg < bend && bbeg < aend;
}

static bool tstring_points_into(const tstring *s, const char *p, size_t len)
{
	if (!s || !s->data || !p)
		return false;
	return tstring_range_overlaps(p, len, s->data, s->len + 1);
}

/* ---- Constructors & Destructor ---- */

tstring *tstring_new(const char *s)
{
	if (!s)
		s = "";
	size_t len = strlen(s);
	if (len == SIZE_MAX)
		return NULL;
	tstring *ts = (tstring *)calloc(1, sizeof(tstring));
	if (!ts)
		return NULL;
	ts->len = len;
	ts->cap = len + 1;
	if (ts->cap < TSTRING_MIN_CAP)
		ts->cap = TSTRING_MIN_CAP;
	ts->data = (char *)malloc(ts->cap);
	if (!ts->data) {
		free(ts);
		return NULL;
	}
	memcpy(ts->data, s, len);
	ts->data[len] = '\0';
	return ts;
}

tstring *tstring_new_len(const char *s, size_t len)
{
	if (len == SIZE_MAX)
		return NULL;
	tstring *ts = (tstring *)calloc(1, sizeof(tstring));
	if (!ts)
		return NULL;
	ts->len = len;
	ts->cap = len + 1;
	if (ts->cap < TSTRING_MIN_CAP)
		ts->cap = TSTRING_MIN_CAP;
	ts->data = (char *)malloc(ts->cap);
	if (!ts->data) {
		free(ts);
		return NULL;
	}
	if (s && len > 0)
		memcpy(ts->data, s, len);
	ts->data[len] = '\0';
	return ts;
}

tstring *tstring_new_cap(size_t cap)
{
	tstring *ts = (tstring *)calloc(1, sizeof(tstring));
	if (!ts)
		return NULL;
	ts->len = 0;
	ts->cap = cap > TSTRING_MIN_CAP ? cap : TSTRING_MIN_CAP;
	ts->data = (char *)malloc(ts->cap);
	if (!ts->data) {
		free(ts);
		return NULL;
	}
	ts->data[0] = '\0';
	return ts;
}

tstring *tstring_new_empty(void)
{
	return tstring_new("");
}

tstring *tstring_dup(const tstring *s)
{
	if (!s)
		return tstring_new_empty();
	return tstring_new_len(s->data, s->len);
}

void tstring_free(tstring *s)
{
	if (!s)
		return;
	free(s->data);
	free(s);
}

/* ---- Accessors ---- */

const char *tstring_cstr(const tstring *s)
{
	return s ? s->data : "";
}

size_t tstring_len(const tstring *s)
{
	return s ? s->len : 0;
}

size_t tstring_cap(const tstring *s)
{
	return s ? s->cap : 0;
}

bool tstring_empty(const tstring *s)
{
	return !s || s->len == 0;
}

char tstring_at(const tstring *s, size_t i)
{
	return s->data[i];
}

/* ---- Assignment ---- */

void tstring_assign(tstring *s, const char *str)
{
	if (!s)
		return;
	if (!str)
		str = "";
	size_t len = strlen(str);
	tstring_assign_len(s, str, len);
}

void tstring_assign_len(tstring *s, const char *str, size_t len)
{
	if (!s)
		return;
	if (!str && len > 0)
		return;
	if (len == SIZE_MAX)
		return;
	bool overlaps = str && tstring_points_into(s, str, len);
	size_t offset = overlaps ? (size_t)(str - s->data) : 0;
	tstring_grow(s, len + 1);
	if (s->cap < len + 1)
		return;
	if (overlaps)
		str = s->data + offset;
	if (str && len > 0)
		memmove(s->data, str, len);
	s->len = len;
	s->data[len] = '\0';
}

void tstring_assign_ts(tstring *dst, const tstring *src)
{
	if (!dst || !src)
		return;
	tstring_assign_len(dst, src->data, src->len);
}

/* ---- Appending ---- */

void tstring_append(tstring *s, const char *str)
{
	if (!s || !str)
		return;
	tstring_append_len(s, str, strlen(str));
}

void tstring_append_len(tstring *s, const char *str, size_t len)
{
	if (!s || !str || len == 0)
		return;
	if (len > SIZE_MAX - s->len - 1)
		return;
	bool overlaps = tstring_points_into(s, str, len);
	size_t offset = overlaps ? (size_t)(str - s->data) : 0;
	tstring_grow(s, s->len + len + 1);
	if (s->cap < s->len + len + 1)
		return;
	if (overlaps)
		str = s->data + offset;
	memmove(s->data + s->len, str, len);
	s->len += len;
	s->data[s->len] = '\0';
}

void tstring_append_c(tstring *s, char c)
{
	if (!s)
		return;
	if (s->len > SIZE_MAX - 2)
		return;
	tstring_grow(s, s->len + 2);
	if (s->cap < s->len + 2)
		return;
	s->data[s->len++] = c;
	s->data[s->len] = '\0';
}

void tstring_append_ts(tstring *s, const tstring *other)
{
	if (!s || !other)
		return;
	tstring_append_len(s, other->data, other->len);
}

void tstring_append_fmt(tstring *s, const char *fmt, ...)
{
	if (!s || !fmt)
		return;
	va_list ap;
	va_start(ap, fmt);
	int need = vsnprintf(NULL, 0, fmt, ap);
	va_end(ap);
	if (need < 0)
		return;
	size_t needed = (size_t)need + 1;
	if (needed == 0 || s->len > SIZE_MAX - needed)
		return;
	tstring_grow(s, s->len + needed);
	if (s->cap < s->len + needed)
		return;
	va_start(ap, fmt);
	vsnprintf(s->data + s->len, needed, fmt, ap);
	va_end(ap);
	s->len += (size_t)need;
}

/* ---- Comparison ---- */

int tstring_cmp(const tstring *a, const tstring *b)
{
	if (!a && !b)
		return 0;
	if (!a)
		return -1;
	if (!b)
		return 1;
	size_t minlen = a->len < b->len ? a->len : b->len;
	int r = memcmp(a->data, b->data, minlen);
	if (r != 0)
		return r;
	if (a->len < b->len)
		return -1;
	if (a->len > b->len)
		return 1;
	return 0;
}

int tstring_cmp_cstr(const tstring *a, const char *b)
{
	if (!a && !b)
		return 0;
	if (!a)
		return -1;
	if (!b)
		return 1;
	return strcmp(a->data, b);
}

int tstring_cmp_cstr_cstr(const char *a, const char *b)
{
	if (!a && !b)
		return 0;
	if (!a)
		return -1;
	if (!b)
		return 1;
	return strcmp(a, b);
}

bool tstring_eq(const tstring *a, const tstring *b)
{
	return tstring_cmp(a, b) == 0;
}

bool tstring_eq_cstr(const tstring *a, const char *b)
{
	return tstring_cmp_cstr(a, b) == 0;
}

bool tstring_eq_cstr_cstr(const char *a, const char *b)
{
	return tstring_cmp_cstr_cstr(a, b) == 0;
}

bool tstring_eq_n(const tstring *a, const tstring *b, size_t n)
{
	if (!a || !b)
		return false;
	if (a->len < n || b->len < n)
		return false;
	return memcmp(a->data, b->data, n) == 0;
}

bool tstring_eq_n_cstr(const tstring *a, const char *b, size_t n)
{
	if (!a || !b)
		return false;
	if (a->len < n)
		return false;
	return strncmp(a->data, b, n) == 0;
}

/* ---- Search ---- */

size_t tstring_find_c(const tstring *s, char c, size_t pos)
{
	if (!s || pos >= s->len)
		return SIZE_MAX;
	const char *p = (const char *)memchr(s->data + pos, (int)(unsigned char)c,
					     s->len - pos);
	return p ? (size_t)(p - s->data) : SIZE_MAX;
}

size_t tstring_rfind_c(const tstring *s, char c)
{
	if (!s || s->len == 0)
		return SIZE_MAX;
	size_t i = s->len;
	while (i > 0) {
		i--;
		if (s->data[i] == c)
			return i;
	}
	return SIZE_MAX;
}

size_t tstring_find(const tstring *s, const char *sub, size_t pos)
{
	if (!s || !sub || sub[0] == '\0')
		return SIZE_MAX;
	size_t sublen = strlen(sub);
	if (sublen > s->len || pos > s->len - sublen)
		return SIZE_MAX;
	const char *p = strstr(s->data + pos, sub);
	return p ? (size_t)(p - s->data) : SIZE_MAX;
}

bool tstring_contains(const tstring *s, const char *sub)
{
	return tstring_find(s, sub, 0) != SIZE_MAX;
}

bool tstring_starts_with(const tstring *s, const char *prefix)
{
	if (!s || !prefix)
		return false;
	size_t plen = strlen(prefix);
	if (plen > s->len)
		return false;
	return memcmp(s->data, prefix, plen) == 0;
}

bool tstring_ends_with(const tstring *s, const char *suffix)
{
	if (!s || !suffix)
		return false;
	size_t slen = strlen(suffix);
	if (slen > s->len)
		return false;
	return memcmp(s->data + s->len - slen, suffix, slen) == 0;
}

/* ---- Substring / Extraction ---- */

tstring *tstring_substr(const tstring *s, size_t pos, size_t len)
{
	if (!s || pos >= s->len)
		return tstring_new_empty();
	if (len > s->len - pos)
		len = s->len - pos;
	return tstring_new_len(s->data + pos, len);
}

size_t tstring_copy_to(const tstring *s, char *buf, size_t bufsz)
{
	if (!s || !buf || bufsz == 0)
		return 0;
	size_t n = s->len < bufsz - 1 ? s->len : bufsz - 1;
	memcpy(buf, s->data, n);
	buf[n] = '\0';
	return n;
}

/* ---- Modification ---- */

void tstring_trim_back(tstring *s)
{
	if (!s || s->len == 0)
		return;
	while (s->len > 0 &&
	       (s->data[s->len - 1] == '\n' || s->data[s->len - 1] == '\r' ||
		s->data[s->len - 1] == '\t' || s->data[s->len - 1] == ' ')) {
		s->len--;
	}
	s->data[s->len] = '\0';
}

void tstring_trim_front(tstring *s)
{
	if (!s || s->len == 0)
		return;
	size_t skip = 0;
	while (skip < s->len &&
	       (s->data[skip] == '\n' || s->data[skip] == '\r' ||
		s->data[skip] == '\t' || s->data[skip] == ' ')) {
		skip++;
	}
	if (skip == 0)
		return;
	s->len -= skip;
	memmove(s->data, s->data + skip, s->len);
	s->data[s->len] = '\0';
}

void tstring_trim(tstring *s)
{
	tstring_trim_back(s);
	tstring_trim_front(s);
}

void tstring_clear(tstring *s)
{
	if (!s)
		return;
	s->len = 0;
	s->data[0] = '\0';
}

void tstring_reserve(tstring *s, size_t cap)
{
	if (!s || cap <= s->cap)
		return;
	char *newdata = (char *)realloc(s->data, cap);
	if (!newdata)
		return;
	s->data = newdata;
	s->cap = cap;
}

void tstring_shrink_to_fit(tstring *s)
{
	if (!s || s->len + 1 >= s->cap)
		return;
	char *newdata = (char *)realloc(s->data, s->len + 1);
	if (!newdata)
		return;
	s->data = newdata;
	s->cap = s->len + 1;
}

char tstring_pop_front(tstring *s)
{
	if (!s || s->len == 0)
		return '\0';
	char c = s->data[0];
	s->len--;
	memmove(s->data, s->data + 1, s->len);
	s->data[s->len] = '\0';
	return c;
}

char tstring_pop_back(tstring *s)
{
	if (!s || s->len == 0)
		return '\0';
	char c = s->data[s->len - 1];
	s->len--;
	s->data[s->len] = '\0';
	return c;
}

void tstring_replace(tstring *s, const char *from, const char *to)
{
	if (!s || !from || !to || from[0] == '\0')
		return;
	size_t flen = strlen(from);
	size_t tlen = strlen(to);
	if (flen == 0)
		return;

	size_t matches = 0;
	size_t pos = 0;
	while ((pos = tstring_find_c(s, from[0], pos)) != SIZE_MAX) {
		if (pos + flen <= s->len &&
		    memcmp(s->data + pos, from, flen) == 0) {
			matches++;
			pos += flen;
		} else {
			pos++;
		}
	}
	if (matches == 0)
		return;

	size_t newlen;
	if (tlen >= flen) {
		size_t growth = matches * (tlen - flen);
		if (matches != 0 && growth / matches != tlen - flen)
			return;
		if (s->len > SIZE_MAX - growth - 1)
			return;
		newlen = s->len + growth;
	} else {
		size_t shrink = matches * (flen - tlen);
		if (matches != 0 && shrink / matches != flen - tlen)
			return;
		newlen = s->len - shrink;
	}

	size_t newcap = newlen + 1;
	if (newcap < TSTRING_MIN_CAP)
		newcap = TSTRING_MIN_CAP;
	char *newdata = (char *)malloc(newcap);
	if (!newdata)
		return;

	size_t write_pos = 0;
	size_t read_pos = 0;
	while (read_pos < s->len) {
		if (read_pos + flen <= s->len &&
		    memcmp(s->data + read_pos, from, flen) == 0) {
			memcpy(newdata + write_pos, to, tlen);
			write_pos += tlen;
			read_pos += flen;
		} else {
			newdata[write_pos++] = s->data[read_pos];
			read_pos++;
		}
	}
	newdata[write_pos] = '\0';
	free(s->data);
	s->data = newdata;
	s->len = newlen;
	s->cap = newcap;
}

void tstring_set_at(tstring *s, size_t i, char c)
{
	if (!s || i >= s->len)
		return;
	s->data[i] = c;
}

/* ---- Iteration helpers ---- */

char *tstring_begin(tstring *s)
{
	return s ? s->data : NULL;
}

char *tstring_end(tstring *s)
{
	return s ? s->data + s->len : NULL;
}

/* ---- IO ---- */

void tstring_print(const tstring *s, FILE *fp)
{
	if (!s || !fp)
		return;
	fwrite(s->data, 1, s->len, fp);
}

void tstring_println(const tstring *s, FILE *fp)
{
	tstring_print(s, fp);
	if (fp)
		fputc('\n', fp);
}

size_t tstring_getline(tstring *s, FILE *fp)
{
	if (!s || !fp)
		return SIZE_MAX;
	tstring_clear(s);
	if (feof(fp))
		return SIZE_MAX;
	int c;
	while ((c = fgetc(fp)) != EOF) {
		if (c == '\n')
			break;
		tstring_append_c(s, (char)c);
	}
	if (c == EOF && s->len == 0)
		return SIZE_MAX;
	return s->len;
}

/* ---- Conversion ---- */

int tstring_to_long(const tstring *s, long *out)
{
	if (!s || !out)
		return -1;
	if (s->len == 0)
		return -1;
	char *end;
	errno = 0;
	*out = strtol(s->data, &end, 10);
	return (errno != 0 || end == s->data || *end != '\0') ? -1 : 0;
}

int tstring_to_double(const tstring *s, double *out)
{
	if (!s || !out)
		return -1;
	if (s->len == 0)
		return -1;
	char *end;
	errno = 0;
	*out = strtod(s->data, &end);
	return (errno != 0 || end == s->data || *end != '\0') ? -1 : 0;
}

/* ---- Hash ---- */

uint64_t tstring_hash(const tstring *s)
{
	if (!s)
		return 0;
	uint64_t hash = 5381;
	size_t i;
	for (i = 0; i < s->len; i++)
		hash = ((hash << 5) + hash) + (uint64_t)(unsigned char)s->data[i];
	return hash;
}
