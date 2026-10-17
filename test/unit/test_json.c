#include "tapas/dsa/tstring.h"
#include "tapas/objects/tdict.h"
#include "tapas/objects/tlist.h"
#include "tapas/objects/tstr.h"
#include "tapas/textension.h"
#include "tapas/tstdlib.h"

#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static tnative_function function(const char *name)
{
	textension_symbol_ref found;
	assert(textension_find(tstdlib_descriptor(), "json", name, &found));
	return found.symbol->function;
}

static const tobj *field(const tobj *record, const char *name, tobj *out)
{
	tobj key = {0};
	tobj_set_compo(&key, (tcompo_v *)tstr_new(name));
	tdict_get((tdict *)record->val.v_tcompo, &key, out);
	tobj_try_clear(&key);
	return out;
}

static void check_invalid(const char *text, size_t length)
{
	tobj input = {0}, record = {0}, value = {0};
	tobj_set_compo(&input, (tcompo_v *)tstr_new_len(text, length));
	function("try_parse")(&input, 1, &record);
	assert(!field(&record, "ok", &value)->val.v_tbool);
	field(&record, "error", &value);
	assert(tstring_len(((tstr *)value.val.v_tcompo)->data));
	tobj_ddc_ref_clear(&value);
	tobj_try_clear(&input);
	tobj_try_clear(&record);
}

static void expect_write_error(tobj *value)
{
	tobj result = {0};
	tapas_error_recover_enabled = 1;
	if (setjmp(tapas_error_jmpbuf) == 0) {
		function("stringify")(value, 1, &result);
		assert(!"invalid JSON value accepted");
	}
	tapas_error_recover_enabled = 0;
}

int main(void)
{
	/* Decoded strings and keys own their storage, including embedded NUL. */
	tobj input = {0}, parsed = {0}, encoded = {0}, reparsed = {0};
	const char *source = "{\"a\\u0000b\":[\"x\\u0000y\",\"\\uD83D\\uDE00\",null,true,9007199254740993]}";
	tobj_set_compo(&input, (tcompo_v *)tstr_new(source));
	function("parse")(&input, 1, &parsed);
	tobj_try_clear(&input);
	function("stringify")(&parsed, 1, &encoded);
	function("parse")(&encoded, 1, &reparsed);
	tobj key = {0}, list = {0};
	tobj_set_compo(&key, (tcompo_v *)tstr_new_len("a\0b", 3));
	tdict_get((tdict *)reparsed.val.v_tcompo, &key, &list);
	tlist *items = (tlist *)list.val.v_tcompo;
	const tstr *text = (tstr *)tlist_at(items, 0)->val.v_tcompo;
	assert(tstring_len(text->data) == 3 && !memcmp(tstring_cstr(text->data), "x\0y", 3));
	text = (tstr *)tlist_at(items, 1)->val.v_tcompo;
	assert(tstring_len(text->data) == 4 && !memcmp(tstring_cstr(text->data), "\xF0\x9F\x98\x80", 4));
	assert(tlist_at(items, 4)->type == tint && tlist_at(items, 4)->val.v_tint == 9007199254740993L);
	tobj_ddc_ref_clear(&list);
	tobj_try_clear(&key);
	tobj_try_clear(&parsed);
	tobj_try_clear(&encoded);
	tobj_try_clear(&reparsed);

	const char *invalid[] = {"", "{", "[1,]", "true false", "01", "NaN", "1e999",
		"9223372036854775808", "-9223372036854775809", "18446744073709551616",
		"{\"a\":1,\"\\u0061\":2}", "{\"a\":[1,{\"b\":[2,3]}],\"a\":4}",
		"\"\\uD800\"", "\"\xFF\"", "\"\xC0\x80\"", "\xEF\xBB\xBF{}"};
	/* Repetition exercises partial-container cleanup under sanitizers. */
	for (unsigned repetition = 0; repetition < 100; repetition++)
		for (size_t i = 0; i < sizeof(invalid) / sizeof(*invalid); i++)
			check_invalid(invalid[i], strlen(invalid[i]));
	check_invalid("true\0false", 10);

	char nested[520];
	memset(nested, '[', 256);
	nested[256] = '0';
	memset(nested + 257, ']', 256);
	tobj_set_compo(&input, (tcompo_v *)tstr_new_len(nested, 513));
	function("parse")(&input, 1, &parsed);
	function("stringify")(&parsed, 1, &encoded);
	assert(tstring_len(((tstr *)encoded.val.v_tcompo)->data) == 513);
	tobj_try_clear(&input);
	tobj_try_clear(&parsed);
	tobj_try_clear(&encoded);
	memset(nested, '[', 257);
	nested[257] = '0';
	memset(nested + 258, ']', 257);
	check_invalid(nested, 515);

	const long integers[] = {LONG_MIN, LONG_MAX, 9007199254740993L};
	for (size_t i = 0; i < sizeof(integers) / sizeof(*integers); i++) {
		tobj_set_int(&input, integers[i]);
		function("stringify")(&input, 1, &encoded);
		function("parse")(&encoded, 1, &parsed);
		assert(parsed.type == tint && parsed.val.v_tint == integers[i]);
		tobj_try_clear(&encoded);
		tobj_try_clear(&parsed);
	}

	/* The writer validates Tapas byte strings as UTF-8. */
	tobj_set_compo(&input, (tcompo_v *)tstr_new_len("\xFF", 1));
	expect_write_error(&input);
	tobj_try_clear(&input);
	tobj_set_float(&input, NAN);
	expect_write_error(&input);
	tobj_set_float(&input, INFINITY);
	expect_write_error(&input);

	/* Hold an owned root while adding and removing a self-reference. */
	tobj temporary = {0};
	tlist *cycle = tlist_new();
	tobj_set_compo(&temporary, (tcompo_v *)cycle);
	tobj_copy(&input, &temporary);
	tobj_vec_push(&cycle->items, &input);
	expect_write_error(&input);
	tobj_set_nil(&temporary);
	tlist_set_at(cycle, 0, &temporary);
	tobj_ddc_ref_clear(&input);

	/* Shared acyclic data is legal. */
	tlist *shared = tlist_new(), *outer = tlist_new();
	tobj_set_compo(&temporary, (tcompo_v *)shared);
	tobj_vec_push(&outer->items, &temporary);
	tobj_vec_push(&outer->items, &temporary);
	tobj_set_compo(&input, (tcompo_v *)outer);
	function("stringify")(&input, 1, &encoded);
	assert(!strcmp(tstring_cstr(((tstr *)encoded.val.v_tcompo)->data), "[[],[]]"));
	tobj_try_clear(&input);
	tobj_try_clear(&encoded);
	return 0;
}
