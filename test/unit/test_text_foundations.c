#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "tapas/dsa/tstring.h"
#include "tapas/objects/tdict.h"
#include "tapas/objects/tlist.h"
#include "tapas/objects/tstr.h"
#include "tapas/textension.h"
#include "tapas/tstdlib.h"
#include "../../src/stdlib/compact.h"

#include <assert.h>
#include <dirent.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void call(const char *package, const char *name, tobj *arguments,
	uint_regs count, tobj *result)
{
	textension_symbol_ref found;
	assert(textension_find(tstdlib_descriptor(), package, name, &found));
	assert(found.symbol->function);
	found.symbol->function(arguments, count, result);
}
static void text(tobj *value, const char *data, size_t length)
{
	tobj_set_compo(value, (tcompo_v *)tstr_new_len(data, length));
}
static int equals(const tobj *value, const char *data, size_t length)
{
	assert(value->type == tcompo && tobj_compo_type(value) == compo_tstr);
	tstring *s = ((tstr *)value->val.v_tcompo)->data;
	return tstring_len(s) == length && !memcmp(tstring_cstr(s), data, length);
}
static void field(const tobj *record, const char *name, tobj *out)
{
	tobj key = {0}; text(&key, name, strlen(name));
	tdict_get((tdict *)record->val.v_tcompo, &key, out);
	tobj_try_clear(&key);
}
static int ok(const tobj *record)
{
	tobj value = {0}; field(record, "ok", &value);
	assert(value.type == tbool);
	return value.val.v_tbool;
}
static void check_code(const tobj *record, const char *code)
{
	tobj value = {0}; field(record, "code", &value);
	assert(equals(&value, code, strlen(code)));
	tobj_ddc_ref_clear(&value);
}
static void check_numeric(void)
{
	const char *invalid[] = {"", " ", "\t", "12x", "12 ", "--1"};
	tobj input = {0}, record = {0}, value = {0};
	for (size_t i = 0; i < sizeof(invalid)/sizeof(*invalid); i++) {
		text(&input, invalid[i], strlen(invalid[i]));
		call(nullptr, "try_int", &input, 1, &record);
		assert(!ok(&record)); check_code(&record, "invalid_number");
		call(nullptr, "try_float", &input, 1, &record);
		assert(!ok(&record)); check_code(&record, "invalid_number");
	}
	text(&input, "12\0x", 4);
	call(nullptr, "try_int", &input, 1, &record);
	assert(!ok(&record));
	call(nullptr, "try_float", &input, 1, &record);
	assert(!ok(&record));
	const double limit = ldexp(1.0, sizeof(long)*CHAR_BIT - 1);
	const double numbers[] = {NAN, INFINITY, -INFINITY, limit, nextafter(-limit, -INFINITY)};
	for (size_t i = 0; i < sizeof(numbers)/sizeof(*numbers); i++) {
		tobj_set_float(&input, numbers[i]);
		call(nullptr, "try_int", &input, 1, &record);
		assert(!ok(&record));
	}
	tobj_set_float(&input, -limit);
	call(nullptr, "try_int", &input, 1, &record);
	assert(ok(&record)); field(&record, "value", &value);
	assert(value.type == tint && value.val.v_tint == LONG_MIN);
	tobj_set_float(&input, nextafter(limit, 0));
	call(nullptr, "try_int", &input, 1, &record);
	assert(ok(&record)); field(&record, "value", &value);
	assert(value.val.v_tint == (long)nextafter(limit, 0));
	tobj_set_float(&input, -3.75);
	call(nullptr, "int", &input, 1, &value);
	assert(value.val.v_tint == -3);
	char boundary[64]; snprintf(boundary, sizeof(boundary), "%ld", LONG_MAX);
	text(&input, boundary, strlen(boundary));
	call(nullptr, "try_int", &input, 1, &record);
	assert(ok(&record)); field(&record, "value", &value);
	assert(value.val.v_tint == LONG_MAX);
	text(&input, "1e999", 5); call(nullptr, "try_float", &input, 1, &record);
	assert(!ok(&record)); check_code(&record, "out_of_range");
	text(&input, "nan", 3); call(nullptr, "try_float", &input, 1, &record);
	assert(!ok(&record)); check_code(&record, "non_finite");
	text(&input, "false\0x", 7); call(nullptr, "bool", &input, 1, &value);
	assert(value.val.v_tbool);
	call(nullptr, "str", &input, 1, &value);
	assert(equals(&value, "false\0x", 7));
	tobj_try_clear(&input); tobj_try_clear(&record); tobj_try_clear(&value);
}

static size_t naive_find(const char *s, size_t n, const char *needle, size_t m)
{
	if (m > n) return SIZE_MAX;
	for (size_t i = 0; i <= n - m; i++)
		if (!memcmp(s + i, needle, m)) return i;
	return SIZE_MAX;
}
static unsigned power3(unsigned n)
{
	unsigned result = 1;
	while (n--) result *= 3;
	return result;
}
static void word(char *out, unsigned length, unsigned code)
{
	const char alphabet[] = {'a', 'b', 0};
	for (unsigned i = 0; i < length; i++) { out[i] = alphabet[code % 3]; code /= 3; }
}
static void check_strings(void)
{
	tobj args[3] = {{0}}, result = {0};
	/* Exhaustively compare against a simple byte-based oracle, including NUL. */
	for (unsigned n = 0; n <= 5; n++) {
		for (unsigned a = 0; a < power3(n); a++) {
			char haystack[6]; word(haystack, n, a); text(&args[0], haystack, n);
			for (unsigned m = 0; m <= 3; m++) {
				for (unsigned b = 0; b < power3(m); b++) {
					char needle[4]; word(needle, m, b); text(&args[1], needle, m);
					call("strings", "find", args, 2, &result);
					size_t wanted = naive_find(haystack, n, needle, m);
					assert(result.val.v_tint == (wanted == SIZE_MAX ? -1 : (long)wanted));
					if (!m) continue;
					call("strings", "split", args, 2, &result);
					tlist *parts = (tlist *)result.val.v_tcompo;
					size_t start = 0, count = 0, found;
					while ((found = naive_find(haystack + start, n - start, needle, m)) != SIZE_MAX) {
						assert(count < tlist_size(parts));
						assert(equals(tlist_at(parts, count++), haystack + start, found));
						start += found + m;
					}
					assert(count + 1 == tlist_size(parts));
					assert(equals(tlist_at(parts, count), haystack + start, n - start));
				}
			}
		}
	}
	text(&args[0], "abc", 3); text(&args[1], "a", 1); tobj_set_int(&args[2], LONG_MIN);
	call("strings", "find", args, 3, &result); assert(result.val.v_tint == 0);
	text(&args[0], "a\0a", 3); text(&args[1], "a", 1); text(&args[2], "x\0", 2);
	call("strings", "replace", args, 3, &result); assert(equals(&result, "x\0\0x\0", 5));
	const char *invalid_utf8[] = {"\xFF", "\x80", "\xC0\x80", "\xE0\x80\x80", "\xED\xA0\x80", "\xF4\x90\x80\x80", "\xE2\x82"};
	for (size_t i = 0; i < sizeof(invalid_utf8)/sizeof(*invalid_utf8); i++) {
		text(&args[0], invalid_utf8[i], strlen(invalid_utf8[i]));
		call("strings", "is_utf8", args, 1, &result); assert(!result.val.v_tbool);
	}
	const char valid[] = "\0\xC2\x80\xED\x9F\xBF\xF4\x8F\xBF\xBF";
	text(&args[0], valid, sizeof(valid) - 1);
	call("strings", "is_utf8", args, 1, &result); assert(result.val.v_tbool);
	/* Package join preserves byte lengths on both iterable and List paths. */
	text(&args[0], "a\0b", 3); text(&args[1], "-", 1);
	call("strings", "join", args, 2, &result);
	assert(equals(&result, "a-\0-b", 5));
	tlist *parts = tlist_new();
	tobj piece = {0};
	text(&piece, "a\0b", 3); tobj_vec_push(&parts->items, &piece);
	text(&piece, "c", 1); tobj_vec_push(&parts->items, &piece);
	tobj_try_clear(&piece);
	tobj_set_compo(&args[0], (tcompo_v *)parts); text(&args[1], ":\0", 2);
	call("strings", "join", args, 2, &result);
	assert(equals(&result, "a\0b:\0c", 6));
	for (unsigned i = 0; i < 3; i++) tobj_try_clear(&args[i]);
	tobj_try_clear(&result);
}

static void check_bytes(void)
{
	char data[256];
	for (unsigned i = 0; i < 256; i++) data[i] = (char)i;
	tobj source = {0}, buffer = {0}, output = {0};
	text(&source, data, sizeof(data));
	call("bytes", "from_string", &source, 1, &buffer);
	tobj_try_clear(&source); /* The buffer must own an independent copy. */
	call("bytes", "to_string", &buffer, 1, &output);
	assert(equals(&output, data, sizeof(data)));
	((unsigned char *)((tcompact *)buffer.val.v_tcompo)->data)[0] = 255;
	assert(equals(&output, data, sizeof(data)));
	tobj_try_clear(&buffer); tobj_try_clear(&output);
	text(&source, "", 0); call("bytes", "from_string", &source, 1, &buffer);
	call("bytes", "to_string", &buffer, 1, &output); assert(equals(&output, "", 0));
	tobj_try_clear(&source); tobj_try_clear(&buffer); tobj_try_clear(&output);
}

static void check_io(void)
{
	char directory[] = "/tmp/tapas-text-XXXXXX";
	assert(mkdtemp(directory));
	char filename[160], absent[160];
	snprintf(filename, sizeof(filename), "%s/data", directory);
	snprintf(absent, sizeof(absent), "%s/absent/data", directory);
	tobj args[2] = {{0}}, result = {0}, value = {0};
	text(&args[0], filename, strlen(filename)); text(&args[1], "a\0b", 3);
	call("io", "try_write_text", args, 2, &result); assert(ok(&result)); check_code(&result, "");
	assert(chmod(filename, 0640) == 0);
	call("io", "try_read_text", args, 1, &result); assert(ok(&result));
	field(&result, "value", &value); assert(equals(&value, "a\0b", 3)); tobj_ddc_ref_clear(&value);
	call("io", "try_append_text", args, 2, &result); assert(ok(&result));
	call("io", "read_text", args, 1, &value); assert(equals(&value, "a\0ba\0b", 6)); tobj_try_clear(&value);
	call("io", "try_replace_text", args, 2, &result); assert(ok(&result));
	struct stat st; assert(stat(filename, &st) == 0 && (st.st_mode & 0777) == 0640);
	call("io", "read_text", args, 1, &value); assert(equals(&value, "a\0b", 3)); tobj_try_clear(&value);
	/* Invalid content must be rejected before opening/truncating the file. */
	tobj_set_int(&args[1], 42); call("io", "try_write_text", args, 2, &result);
	assert(!ok(&result)); check_code(&result, "invalid_type");
	call("io", "read_text", args, 1, &value); assert(equals(&value, "a\0b", 3)); tobj_try_clear(&value);
	text(&args[1], "x", 1); text(&args[0], absent, strlen(absent));
	const char *operations[] = {"try_read_text", "try_write_text", "try_append_text", "try_replace_text"};
	for (size_t i = 0; i < 4; i++) {
		call("io", operations[i], args, i ? 2 : 1, &result);
		assert(!ok(&result)); check_code(&result, "not_found");
		field(&result, "errno", &value); assert(value.val.v_tint != 0);
	}
	text(&args[0], "a\0x", 3); call("io", "try_read_text", args, 1, &result);
	assert(!ok(&result)); check_code(&result, "invalid_path");
	text(&args[0], directory, strlen(directory)); call("io", "try_replace_text", args, 2, &result);
	assert(!ok(&result));
	/* A failed rename must remove its temporary file. */
	DIR *dir = opendir("/tmp"); assert(dir);
	struct dirent *entry;
	char prefix[160]; snprintf(prefix, sizeof(prefix), "%s.tmp.", strrchr(directory, '/') + 1);
	while ((entry = readdir(dir))) assert(strncmp(entry->d_name, prefix, strlen(prefix)) != 0);
	closedir(dir);
	assert(unlink(filename) == 0); assert(rmdir(directory) == 0);
	for (unsigned i = 0; i < 2; i++) tobj_try_clear(&args[i]);
	tobj_try_clear(&result); tobj_try_clear(&value);
}

static void check_paths(void)
{
	const char *pairs[][2] = {
		{"", "."}, {".", "."}, {"a/..", "."}, {"a/../../b", "../b"},
		{"/a/../../b//", "/b"}, {"//a///b/", "/a/b"}, {"../../a", "../../a"},
		{"/../..", "/"}, {"a/./b/../c", "a/c"}, {"a/../../..", "../.."}
	};
	tobj input = {0}, output = {0}, again = {0};
	for (size_t i = 0; i < sizeof(pairs)/sizeof(*pairs); i++) {
		text(&input, pairs[i][0], strlen(pairs[i][0]));
		call("path", "normalize", &input, 1, &output);
		assert(equals(&output, pairs[i][1], strlen(pairs[i][1])));
		call("path", "normalize", &output, 1, &again);
		assert(tobj_identical(&output, &again));
	}
	const char *no_extension[] = {".env", "..env", "...", "..", ".", "name", "/"};
	for (size_t i = 0; i < sizeof(no_extension)/sizeof(*no_extension); i++) {
		text(&input, no_extension[i], strlen(no_extension[i]));
		call("path", "extension", &input, 1, &output);
		assert(equals(&output, "", 0));
	}
	tobj_try_clear(&input); tobj_try_clear(&output); tobj_try_clear(&again);
}

int main(void)
{
	check_numeric(); check_strings(); check_bytes(); check_io(); check_paths();
	return 0;
}
