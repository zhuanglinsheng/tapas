#include "tapas/compile/lexer.h"

#include <ctype.h>


/*===========================================================================*
 * 1. Unit Counter (parenthesis/bracket/brace/quote tracking)
 *===========================================================================*/

void tunit_ctr_init(tunit_ctr *c)
{
	c->parenthesis = 0;
	c->bracket = 0;
	c->curlybrace = 0;
	c->singlequote = 0;
	c->doublequote = 0;
}

void tunit_ctr_restore(tunit_ctr *c)
{
	c->parenthesis = 0;
	c->bracket = 0;
	c->curlybrace = 0;
	c->singlequote = 0;
	c->doublequote = 0;
}

int tunit_ctr_out_of_single_string(const tunit_ctr *c)
{
	return c->singlequote == 0;
}

int tunit_ctr_out_of_double_string(const tunit_ctr *c)
{
	return c->doublequote == 0;
}

int tunit_ctr_out_of_string(const tunit_ctr *c)
{
	return tunit_ctr_out_of_single_string(c) &&
	       tunit_ctr_out_of_double_string(c);
}

int tunit_ctr_out_of_parenthesis(const tunit_ctr *c)
{
	return c->parenthesis == 0;
}

int tunit_ctr_out_of_bracket(const tunit_ctr *c)
{
	return c->bracket == 0;
}

int tunit_ctr_out_of_curlybrace(const tunit_ctr *c)
{
	return c->curlybrace == 0;
}

int tunit_ctr_independent(const tunit_ctr *c)
{
	return tunit_ctr_out_of_parenthesis(c) && tunit_ctr_out_of_bracket(c) &&
	       tunit_ctr_out_of_curlybrace(c) && tunit_ctr_out_of_string(c);
}

/** Update counters by scanning one character. Returns always 1. */
int tunit_ctr_update(tunit_ctr *c, char ch)
{
	switch (ch) {
	case '(':
		if (tunit_ctr_out_of_string(c))
			c->parenthesis++;
		break;
	case ')':
		if (tunit_ctr_out_of_string(c))
			c->parenthesis--;
		break;
	case '[':
		if (tunit_ctr_out_of_string(c))
			c->bracket++;
		break;
	case ']':
		if (tunit_ctr_out_of_string(c))
			c->bracket--;
		break;
	case '{':
		if (tunit_ctr_out_of_string(c))
			c->curlybrace++;
		break;
	case '}':
		if (tunit_ctr_out_of_string(c))
			c->curlybrace--;
		break;
	case '\'':
		if (tunit_ctr_out_of_double_string(c))
			c->singlequote = 1 - c->singlequote;
		break;
	case '"':
		if (tunit_ctr_out_of_single_string(c))
			c->doublequote = 1 - c->doublequote;
		break;
	}
	return 1;
}

/*===========================================================================*
 * 2. Lexer Utilities
 *===========================================================================*/

static tstring *trim_ts_dup(const char *str)
{
	tstring *s = tstring_new(str ? str : "");
	tstring_trim(s);
	return s;
}

static tstring *trim_ts_new_len(const char *str, size_t len)
{
	tstring *s = tstring_new_len(str, len);
	tstring_trim(s);
	return s;
}

static void strip_surrounding_quotes_ts(tstring *s)
{
	size_t len = tstring_len(s);
	const char *buf = tstring_cstr(s);
	if (len < 2)
		return;
	if ((buf[0] == '"' && buf[len - 1] == '"') ||
	    (buf[0] == '\'' && buf[len - 1] == '\''))
		tstring_assign_len(s, buf + 1, len - 2);
}

int tlex_is_name_char(char ch)
{
	return isalpha((unsigned char)ch) || isdigit((unsigned char)ch) || ch == '_';
}

static int tlex_delimiters_contains(const tstring *delimiters, char ch)
{
	if (!delimiters)
		return 0;
	return tstring_find_c(delimiters, ch, 0) != SIZE_MAX;
}

int tlex_find_top_level_assignment_ts(const tstring *src, uint_lexs *off)
{
	tunit_ctr ctr;
	const char *str;
	size_t len;
	tunit_ctr_init(&ctr);
	if (!src || !off)
		return 0;
	str = tstring_cstr(src);
	len = tstring_len(src);

	for (size_t i = 0; i < len; i++) {
		tunit_ctr_update(&ctr, str[i]);
		if (!tunit_ctr_independent(&ctr) || str[i] != '=')
			continue;
		if ((i > 0 && (str[i - 1] == '=' || str[i - 1] == '!' ||
			       str[i - 1] == '>' || str[i - 1] == '<')) ||
		    (i + 1 < len && str[i + 1] == '='))
			continue;
		*off = (uint_lexs)i;
		return 1;
	}
	return 0;
}

int tlex_get_first_enclosed_ts(
		const tstring *src, char open, char close, tstring **out, uint_lexs *loc)
{
	const char *str;
	size_t len;
	if (!src || !out || !loc)
		return 0;
	str = tstring_cstr(src);
	if (!str || str[0] != open)
		return 0;
	tunit_ctr ctr;
	tunit_ctr_init(&ctr);
	len = tstring_len(src);
	for (size_t i = 0; i < len; i++) {
		tunit_ctr_update(&ctr, str[i]);
		if (tunit_ctr_independent(&ctr)) {
			if (str[i] != close || i == 0)
				return 0;
			*out = tstring_new_len(str + 1, i - 1);
			*loc = (uint_lexs)i;
			return 1;
		}
	}
	return 0;
}

int tlex_get_last_enclosed_ts(
		const tstring *src, char open, char close, tstring **out, uint_lexs *loc)
{
	const char *str;
	size_t len;
	if (!src || !out || !loc)
		return 0;
	str = tstring_cstr(src);
	len = tstring_len(src);
	if (len == 0 || str[len - 1] != close)
		return 0;
	tunit_ctr ctr;
	tunit_ctr_init(&ctr);
	int64_t i;
	for (i = (int64_t)len - 1; i >= 0; i--) {
		tunit_ctr_update(&ctr, str[i]);
		if (tunit_ctr_independent(&ctr)) {
			if (str[i] != open)
				return 0;
			size_t start = (size_t)i + 1;
			*out = tstring_new_len(str + start, len - (size_t)i - 2);
			*loc = (uint_lexs)i;
			return 1;
		}
	}
	return 0;
}

static int get_first_parenthesis_ts(const tstring *src, tstring **out, uint_lexs *loc)
{
	return tlex_get_first_enclosed_ts(src, '(', ')', out, loc);
}

static int get_last_parenthesis_ts(const tstring *src, tstring **out, uint_lexs *loc)
{
	return tlex_get_last_enclosed_ts(src, '(', ')', out, loc);
}

static int get_last_bracket_ts(const tstring *src, tstring **out, uint_lexs *loc)
{
	return tlex_get_last_enclosed_ts(src, '[', ']', out, loc);
}

static int get_first_block_ts(const tstring *src, tstring **out, uint_lexs *loc)
{
	return tlex_get_first_enclosed_ts(src, '{', '}', out, loc);
}

static int get_first_single_quote_ts(const tstring *src, tstring **out, uint_lexs *loc)
{
	return tlex_get_first_enclosed_ts(src, '\'', '\'', out, loc);
}

static int get_first_double_quote_ts(const tstring *src, tstring **out, uint_lexs *loc)
{
	return tlex_get_first_enclosed_ts(src, '"', '"', out, loc);
}

int tlex_find_last_top_level_char_ts(const tstring *src, char ch, uint_lexs *off)
{
	tunit_ctr ctr;
	const char *str;
	size_t len;
	int found = 0;
	tunit_ctr_init(&ctr);
	if (!src || !off)
		return 0;
	str = tstring_cstr(src);
	len = tstring_len(src);

	for (size_t i = 0; i < len; i++) {
		if (tunit_ctr_independent(&ctr) && str[i] == ch) {
			*off = (uint_lexs)i;
			found = 1;
		}
		tunit_ctr_update(&ctr, str[i]);
	}
	return found;
}

int tlex_find_last_top_level_word_ts(const tstring *src, const tstring *word, uint_lexs *off)
{
	tunit_ctr ctr;
	const char *str;
	const char *wstr;
	size_t len;
	size_t wlen;
	int found = 0;
	tunit_ctr_init(&ctr);

	if (!src || !word || !off)
		return 0;
	str = tstring_cstr(src);
	wstr = tstring_cstr(word);
	len = tstring_len(src);
	wlen = tstring_len(word);
	if (wlen == 0 || len < wlen)
		return 0;

	for (size_t i = 0; i + wlen <= len; i++) {
		if (tunit_ctr_independent(&ctr) &&
		    strncmp(str + i, wstr, wlen) == 0 &&
		    (i == 0 || !tlex_is_name_char(str[i - 1])) &&
		    (i + wlen == len || !tlex_is_name_char(str[i + wlen]))) {
			*off = (uint_lexs)i;
			found = 1;
		}
		tunit_ctr_update(&ctr, str[i]);
	}
	return found;
}

static int find_last_top_level_word_cstr(const tstring *src, const char *word, uint_lexs *off)
{
	tstring *word_ts = tstring_new(word);
	int found = tlex_find_last_top_level_word_ts(src, word_ts, off);
	tstring_free(word_ts);
	return found;
}

static uint32_t count_top_level_word_cstr(const tstring *src, const char *word)
{
	tunit_ctr ctr;
	const char *str = tstring_cstr(src);
	size_t len = tstring_len(src);
	size_t wlen = strlen(word);
	uint32_t count = 0;
	tunit_ctr_init(&ctr);
	for (size_t i = 0; i + wlen <= len; i++) {
		if (tunit_ctr_independent(&ctr) &&
		    strncmp(str + i, word, wlen) == 0 &&
		    (i == 0 || !tlex_is_name_char(str[i - 1])) &&
		    (i + wlen == len || !tlex_is_name_char(str[i + wlen])))
			count++;
		tunit_ctr_update(&ctr, str[i]);
	}
	return count;
}

static int find_first_top_level_char_ts(const tstring *src, char ch, uint_lexs *off)
{
	tunit_ctr ctr;
	const char *str = tstring_cstr(src);
	size_t len = tstring_len(src);
	tunit_ctr_init(&ctr);
	for (size_t i = 0; i < len; i++) {
		if (tunit_ctr_independent(&ctr) && str[i] == ch) {
			*off = (uint_lexs)i;
			return 1;
		}
		tunit_ctr_update(&ctr, str[i]);
	}
	return 0;
}

static int find_last_top_level_multiplicative_ts(
		const tstring *src, char *operator_ch, uint_lexs *off)
{
	tunit_ctr ctr;
	const char *str = tstring_cstr(src);
	size_t len = tstring_len(src);
	int found = 0;
	tunit_ctr_init(&ctr);
	for (size_t i = 0; i < len; i++) {
		if (tunit_ctr_independent(&ctr) &&
		    strchr("*/%@", str[i]) != NULL) {
			*operator_ch = str[i];
			*off = (uint_lexs)i;
			found = 1;
		}
		tunit_ctr_update(&ctr, str[i]);
	}
	return found;
}

static int prefix_ends_with_word(const char *str, size_t end, const char *word)
{
	size_t wlen = strlen(word);
	while (end > 0 && isspace((unsigned char)str[end - 1]))
		end--;
	if (end < wlen || strncmp(str + end - wlen, word, wlen) != 0)
		return 0;
	return end == wlen || !tlex_is_name_char(str[end - wlen - 1]);
}

static int sign_follows_numeric_exponent(const char *str, size_t off)
{
	if (off < 2 || (str[off - 1] != 'e' && str[off - 1] != 'E'))
		return 0;

	size_t start = off - 1;
	while (start > 0 &&
	       (isdigit((unsigned char)str[start - 1]) || str[start - 1] == '.'))
		start--;
	if (start == off - 1 ||
	    (start > 0 && tlex_is_name_char(str[start - 1])))
		return 0;

	int seen_digit = 0;
	int seen_dot = 0;
	for (size_t i = start; i < off - 1; i++) {
		if (isdigit((unsigned char)str[i]))
			seen_digit = 1;
		else if (str[i] == '.' && !seen_dot)
			seen_dot = 1;
		else
			return 0;
	}
	return seen_digit;
}

static int sign_is_unary(const char *str, size_t off)
{
	size_t prev = off;
	while (prev > 0 && isspace((unsigned char)str[prev - 1]))
		prev--;
	if (prev == 0)
		return 1;
	char ch = str[prev - 1];
	if ((ch == 'e' || ch == 'E') && sign_follows_numeric_exponent(str, prev))
		return 1;
	if (strchr("+-*/%@^&|<>=!:,([{", ch) != NULL)
		return 1;
	return prefix_ends_with_word(str, prev, "and") ||
	       prefix_ends_with_word(str, prev, "or") ||
	       prefix_ends_with_word(str, prev, "in") ||
	       prefix_ends_with_word(str, prev, "to");
}

static int find_last_top_level_binary_sign_ts(
		const tstring *src, char sign, uint_lexs *off)
{
	tunit_ctr ctr;
	const char *str = tstring_cstr(src);
	size_t len = tstring_len(src);
	int found = 0;
	tunit_ctr_init(&ctr);
	for (size_t i = 0; i < len; i++) {
		if (tunit_ctr_independent(&ctr) && str[i] == sign &&
		    !sign_is_unary(str, i)) {
			*off = (uint_lexs)i;
			found = 1;
		}
		tunit_ctr_update(&ctr, str[i]);
	}
	return found;
}

static uint32_t count_top_level_comparisons(const tstring *src)
{
	tunit_ctr ctr;
	const char *str = tstring_cstr(src);
	size_t len = tstring_len(src);
	uint32_t count = 0;
	tunit_ctr_init(&ctr);
	for (size_t i = 0; i < len; i++) {
		if (tunit_ctr_independent(&ctr)) {
			if (i + 1 < len &&
			    ((str[i] == '=' && str[i + 1] == '=') ||
			     (str[i] == '!' && str[i + 1] == '=') ||
			     (str[i] == '>' && str[i + 1] == '=') ||
			     (str[i] == '<' && str[i + 1] == '='))) {
				count++;
				i++;
				continue;
			}
			if (str[i] == '>' || str[i] == '<')
				count++;
		}
		tunit_ctr_update(&ctr, str[i]);
	}
	return count;
}

int tlex_find_last_top_level_idx2_sep_ts(const tstring *src, uint_lexs *off, size_t *sep_len)
{
	tunit_ctr ctr;
	const char *str;
	size_t len;
	int found = 0;
	tunit_ctr_init(&ctr);
	if (!src || !off || !sep_len)
		return 0;
	str = tstring_cstr(src);
	len = tstring_len(src);

	for (size_t i = 0; i < len; i++) {
		if (tunit_ctr_independent(&ctr)) {
			if (i + 1 < len && str[i] == ':' && str[i + 1] == ':') {
				*off = (uint_lexs)i;
				*sep_len = 2;
				found = 1;
				i++;
				continue;
			}
			if (str[i] == '.' &&
			    !(i > 0 && i + 1 < len &&
			      isdigit((unsigned char)str[i - 1]) &&
			      isdigit((unsigned char)str[i + 1]))) {
				*off = (uint_lexs)i;
				*sep_len = 1;
				found = 1;
			}
		}
		tunit_ctr_update(&ctr, str[i]);
	}
	return found;
}

/** Split string by delimiter respecting bracket system. */
void tlex_split_top_level_ts(const tstring *src,
			     const tstring *delimiters,
			     tstring ***result,
			     uint32_t *result_count)
{
	*result = NULL;
	*result_count = 0;

	tstring *cmds = tstring_dup(src);
	tstring_trim(cmds);
	const char *buf = tstring_cstr(cmds);
	uint_lexs len = (uint_lexs)tstring_len(cmds);
	uint_lexs start = 0;
	tunit_ctr ctr;
	tunit_ctr_init(&ctr);

	uint_lexs i;
	for (i = 0; i < len; i++) {
		tunit_ctr_update(&ctr, buf[i]);
		if (!tunit_ctr_independent(&ctr) ||
		    !tlex_delimiters_contains(delimiters, buf[i]))
			continue;

		tstring *part = tstring_new_len(buf + start, i - start);
		tstring_trim(part);
		if (!tstring_empty(part)) {
			(*result_count)++;
			*result = (tstring **)realloc(
				*result, (*result_count) * sizeof(tstring *));
			(*result)[*result_count - 1] = part;
		} else {
			tstring_free(part);
		}
		start = i + 1;
	}

	tstring *part = tstring_new_len(buf + start, len - start);
	tstring_trim(part);
	if (!tstring_empty(part)) {
		(*result_count)++;
		*result = (tstring **)realloc(*result,
					      (*result_count) * sizeof(tstring *));
		(*result)[*result_count - 1] = part;
	} else {
		tstring_free(part);
	}
	tstring_free(cmds);
}

tstring *tlex_strip_outer_parentheses_ts(const tstring *src)
{
	tstring *cur = tstring_dup(src);
	int changed = 1;

	while (changed) {
		const char *buf;
		size_t len;
		tstring *inner = NULL;
		uint_lexs end = 0;
		changed = 0;
		tstring_trim(cur);
		buf = tstring_cstr(cur);
		len = tstring_len(cur);
		if (len < 2 || buf[0] != '(' || buf[len - 1] != ')')
			break;
		if (!tlex_get_first_enclosed_ts(cur, '(', ')', &inner, &end))
			break;
		if (end == len - 1) {
			tstring_free(cur);
			cur = inner;
			changed = 1;
		} else {
			tstring_free(inner);
		}
	}
	return cur;
}

/*===========================================================================*
 * 3. Variable Name Validation
 *===========================================================================*/

static int check_vname_validity(const char *str)
{
	size_t len;
	/* Keywords */
	if (strcmp(str, "var") == 0 || strcmp(str, "let") == 0 ||
	    strcmp(str, "of") == 0)
		return 0;

	if (strcmp(str, "nil") == 0)
		return 0;

	if (strcmp(str, "true") == 0 || strcmp(str, "false") == 0)
		return 0;

	if (strcmp(str, "this") == 0 || strcmp(str, "base") == 0)
		return 0;

	if (strcmp(str, "to") == 0 || strcmp(str, "in") == 0 ||
	    strcmp(str, "and") == 0 || strcmp(str, "or") == 0)
		return 0;

	if (strcmp(str, "if") == 0 || strcmp(str, "elif") == 0 ||
	    strcmp(str, "else") == 0)
		return 0;

	if (strcmp(str, "for") == 0 || strcmp(str, "while") == 0)
		return 0;

	if (strcmp(str, "break") == 0 || strcmp(str, "continue") == 0)
		return 0;

	if (strcmp(str, "return") == 0)
		return 0;

	if (strcmp(str, "import") == 0 || strcmp(str, "as") == 0)
		return 0;
	if (strcmp(str, "function") == 0)
		return 0;

	if (isdigit((unsigned char)str[0]))
		return 0;

	len = strlen(str);
	for (size_t i = 0; i < len; i++) {
		if (!isalpha((unsigned char)str[i]) &&
		    !isdigit((unsigned char)str[i]) && str[i] != '_')
			return 0;
	}
	return 1;
}

static int find_last_single_colon_ts(const tstring *src, uint_lexs *offset)
{
	tstring *search = tstring_dup(src);
	const char *full = tstring_cstr(src);
	uint_lexs found = 0;
	while (tlex_find_last_top_level_char_ts(search, ':', &found)) {
		if ((found == 0 || full[found - 1] != ':') &&
		    full[found + 1] != ':') {
			*offset = found;
			tstring_free(search);
			return 1;
		}
		if (found == 0)
			break;
		tstring_assign_len(search, full, found);
	}
	tstring_free(search);
	return 0;
}

static int split_decl_ts(const tstring *unit,
			 size_t kw_len,
			 tstring **vname,
			 tstring **vtype,
			 tstring **right)
{
	tstring *body = tstring_substr(unit, kw_len, tstring_len(unit) - kw_len);
	tstring_trim(body);

	uint_lexs eq = 0;
	if (!tlex_find_top_level_assignment_ts(body, &eq))
		twarn(ErrCompile_InvalidLiter, "get_tokens", tstring_cstr(body));

	tstring *sig = tstring_substr(body, 0, eq);
	tstring_trim(sig);
	*right = tstring_substr(body, eq + 1, tstring_len(body) - eq - 1);
	tstring_trim(*right);
	if (tstring_empty(*right))
		twarn(ErrCompile_InvalidLiter, "get_tokens", tstring_cstr(sig));

	uint_lexs colon = 0;
	if (find_last_single_colon_ts(sig, &colon)) {
		*vname = tstring_substr(sig, 0, colon);
		*vtype = tstring_substr(sig, colon + 1, tstring_len(sig) - colon - 1);
		tstring_trim(*vtype);
		if (tstring_empty(*vtype))
			twarn(ErrCompile_VarNoType, "get_tokens", tstring_cstr(*vname));
	} else {
		*vname = tstring_dup(sig);
		*vtype = tstring_new_empty();
	}
	tstring_trim(*vname);
	if (!check_vname_validity(tstring_cstr(*vname)))
		twarn(ErrCompile_InvalidVname, "get_tokens", tstring_cstr(*vname));

	tstring_free(sig);
	tstring_free(body);
	return 1;
}

/*===========================================================================*
 * 4. Token Structures — values are tstring
 *===========================================================================*/

void ttoken_free(ttoken *t)
{
	tstring_free(t->val1);
	tstring_free(t->val2);
	tstring_free(t->val3);
}

/*===========================================================================*
 * 5. Tokenizer (get_tokens)
 *===========================================================================*/

/** Convert a syntax unit string into tokens. */
void
get_tokens_ts(const tstring *src, ttoken **out_tokens, uint32_t *out_count)
{
	*out_tokens = NULL;
	*out_count = 0;
	uint32_t cap = 0;

	tstring *unit_ts = tstring_dup(src);
	tstring_trim(unit_ts);
	const char *unit = tstring_cstr(unit_ts);
	size_t len = tstring_len(unit_ts);
	if (len == 0) {
		tstring_free(unit_ts);
		return;
	}

	uint_lexs genloc;

#define EMIT_TOKEN(t, n, v1, v2, v3)                                           \
	do {                                                                   \
		if (*out_count >= cap) {                                       \
			cap = cap ? cap * 2 : 8;                               \
			*out_tokens = (ttoken *)realloc(*out_tokens,           \
							cap * sizeof(ttoken)); \
		}                                                              \
		(*out_tokens)[*out_count].type = (t);                          \
		(*out_tokens)[*out_count].nvals = (n);                          \
		(*out_tokens)[*out_count].val1 =                            \
			v1 ? tstring_new(v1) : tstring_new_empty();            \
		(*out_tokens)[*out_count].val2 =                            \
			v2 ? tstring_new(v2) : tstring_new_empty();            \
		(*out_tokens)[*out_count].val3 =                            \
			v3 ? tstring_new(v3) : tstring_new_empty();            \
		(*out_count)++;                                                \
		tstring_free(unit_ts);                                         \
		return;                                                        \
	} while (0)

#define EMIT_TOKEN_TS(t, n, v1, v2, v3)                                        \
	do {                                                                   \
		if (*out_count >= cap) {                                       \
			cap = cap ? cap * 2 : 8;                               \
			*out_tokens = (ttoken *)realloc(*out_tokens,           \
							cap * sizeof(ttoken)); \
		}                                                              \
		(*out_tokens)[*out_count].type = (t);                          \
		(*out_tokens)[*out_count].nvals = (n);                          \
		(*out_tokens)[*out_count].val1 =                            \
			(v1) ? tstring_dup(v1) : tstring_new_empty();          \
		(*out_tokens)[*out_count].val2 =                            \
			(v2) ? tstring_dup(v2) : tstring_new_empty();          \
		(*out_tokens)[*out_count].val3 =                            \
			(v3) ? tstring_dup(v3) : tstring_new_empty();          \
		(*out_count)++;                                                \
		if (v1) tstring_free(v1);                                      \
		if (v2) tstring_free(v2);                                      \
		if (v3) tstring_free(v3);                                      \
		tstring_free(unit_ts);                                         \
		return;                                                        \
	} while (0)

#define EMIT_BINARY_TS(t, left_len, right_start)                               \
	do {                                                                   \
		tstring *emit_l = trim_ts_new_len(unit, (left_len));           \
		tstring *emit_r = trim_ts_dup(unit + (right_start));           \
		if (!tstring_empty(emit_l) && !tstring_empty(emit_r))           \
			EMIT_TOKEN_TS((t), 2, emit_l, emit_r, NULL);           \
		tstring_free(emit_l);                                          \
		tstring_free(emit_r);                                          \
	} while (0)

#define APPEND_TOKEN_MOVE(tok)                                                 \
	do {                                                                   \
		if (*out_count >= cap) {                                       \
			cap = cap ? cap * 2 : 8;                               \
			*out_tokens = (ttoken *)realloc(*out_tokens,           \
							cap * sizeof(ttoken)); \
		}                                                              \
		(*out_tokens)[*out_count] = (tok);                             \
		(*out_count)++;                                                \
	} while (0)

	if (strcmp(unit, "true") == 0)
		EMIT_TOKEN(token_true, 0, "", "", "");
	if (strcmp(unit, "false") == 0)
		EMIT_TOKEN(token_false, 0, "", "", "");
	if (strcmp(unit, "this") == 0)
		EMIT_TOKEN(token_this, 0, "", "", "");
	if (strcmp(unit, "base") == 0)
		EMIT_TOKEN(token_base, 0, "", "", "");
	if (strcmp(unit, "continue") == 0)
		EMIT_TOKEN(token_continue, 0, "", "", "");
	if (strcmp(unit, "break") == 0)
		EMIT_TOKEN(token_break, 0, "", "", "");

	/* return [value] */
	if (len >= 6 && strncmp(unit, "return", 6) == 0 &&
	    (len == 6 || !tlex_is_name_char(unit[6]))) {
		tstring *v = tstring_substr(unit_ts, 6, len - 6);
		tstring_trim(v);
		if (!tstring_empty(v))
			EMIT_TOKEN_TS(token_return, 1, v, NULL, NULL);
		else {
			tstring_free(v);
			EMIT_TOKEN(token_return, 0, "", "", "");
		}
	}

	/* var/let declaration lists: var a = 0, b = 1, c = 0 */
	if ((len >= 4 && strncmp(unit, "var ", 4) == 0) ||
	    (len >= 4 && strncmp(unit, "let ", 4) == 0)) {
		const char *kw = unit[0] == 'v' ? "var " : "let ";
		tstring **decls = NULL;
		uint32_t ndecls = 0;
		tstring *decl_src = tstring_substr(unit_ts, 4, len - 4);
		tstring *comma = tstring_new(",");
		tlex_split_top_level_ts(decl_src, comma, &decls, &ndecls);
		tstring_free(comma);
		tstring_free(decl_src);
		if (ndecls > 1) {
			uint32_t i;
			for (i = 0; i < ndecls; i++) {
				tstring *decl = tstring_new(kw);
				ttoken *sub_tokens = NULL;
				uint32_t sub_count = 0;
				uint32_t j;
				tstring_append_ts(decl, decls[i]);
				get_tokens_ts(decl, &sub_tokens, &sub_count);
				for (j = 0; j < sub_count; j++)
					APPEND_TOKEN_MOVE(sub_tokens[j]);
				free(sub_tokens);
				tstring_free(decl);
			}
			lex_str_free(decls, ndecls);
			tstring_free(unit_ts);
			return;
		}
		lex_str_free(decls, ndecls);
	}

	/* var name [: type] = value */
	if (len >= 4 && strncmp(unit, "var ", 4) == 0) {
		tstring *vname = NULL;
		tstring *vtype = NULL;
		tstring *right = NULL;
		split_decl_ts(unit_ts, 4, &vname, &vtype, &right);
		EMIT_TOKEN_TS(token_var, 3, vname, vtype, right);
	}

	/* let name [: type] = value */
	if (len >= 4 && strncmp(unit, "let ", 4) == 0) {
		tstring *vname = NULL;
		tstring *vtype = NULL;
		tstring *right = NULL;
		split_decl_ts(unit_ts, 4, &vname, &vtype, &right);
		EMIT_TOKEN_TS(token_let, 3, vname, vtype, right);
	}

	/* import */
	if (len >= 7 && strncmp(unit, "import ", 7) == 0) {
		tstring *import_body = tstring_substr(unit_ts, 7, len - 7);
		tstring_trim(import_body);
		size_t as_pos = tstring_find(import_body, " as ", 0);
		tstring *file = NULL;
		tstring *name = NULL;
		if (as_pos != SIZE_MAX) {
			file = tstring_substr(import_body, 0, as_pos);
			name = tstring_substr(import_body,
					      as_pos + 4,
					      tstring_len(import_body) - as_pos - 4);
			tstring_trim(name);
		} else {
			file = tstring_dup(import_body);
		}
		tstring_trim(file);
		strip_surrounding_quotes_ts(file);
		tstring_free(import_body);
		if (name)
			EMIT_TOKEN_TS(token_import, 2, file, name, NULL);
		else
			EMIT_TOKEN_TS(token_import, 1, file, NULL, NULL);
	}

	/* while(cond){blk} */
	if (len >= 5 && strncmp(unit, "while", 5) == 0 &&
	    (len == 5 || !tlex_is_name_char(unit[5]))) {
		tstring *contents = trim_ts_dup(unit + 5);
		tstring *cond = NULL, *blk = NULL;
		uint_lexs idx_cond;
		if (get_first_parenthesis_ts(contents, &cond, &idx_cond)) {
			tstring *blk_full =
				trim_ts_dup(tstring_cstr(contents) + idx_cond + 1);
			uint_lexs idx_blk;
			if (get_first_block_ts(blk_full, &blk, &idx_blk)) {
				if (idx_blk == tstring_len(blk_full) - 1) {
					tstring_free(contents);
					tstring_free(blk_full);
					EMIT_TOKEN_TS(token_while,
						      2,
						      cond,
						      blk,
						      NULL);
				} else {
					tstring_free(blk);
				}
			}
			tstring_free(blk_full);
		}
		if (cond)
			tstring_free(cond);
		tstring_free(contents);
	}

	/* for(i in iter){blk} */
	if (len >= 3 && strncmp(unit, "for", 3) == 0 &&
	    (len == 3 || !tlex_is_name_char(unit[3]))) {
		tstring *contents = trim_ts_dup(unit + 3);
		tstring *cond = NULL, *blk = NULL;
		uint_lexs idx_cond;
		if (get_first_parenthesis_ts(contents, &cond, &idx_cond)) {
			tstring *blk_full =
				trim_ts_dup(tstring_cstr(contents) + idx_cond + 1);
			uint_lexs idx_blk;
			if (get_first_block_ts(blk_full, &blk, &idx_blk)) {
				if (idx_blk == tstring_len(blk_full) - 1) {
					tstring *cond_t = tstring_dup(cond);
					uint_lexs in_off = 0;
					if (find_last_top_level_word_cstr(cond_t, "in", &in_off)) {
						tstring *in_l = tstring_substr(cond_t, 0, in_off);
						tstring *in_r = tstring_substr(cond_t,
									       in_off + 2,
									       tstring_len(cond_t) - in_off - 2);
						tstring_trim(in_l);
						tstring_trim(in_r);
						tstring_free(contents);
						tstring_free(blk_full);
						tstring_free(cond);
						tstring_free(cond_t);
						EMIT_TOKEN_TS(token_for,
							      3,
							      in_l,
							      in_r,
							      blk);
					}
					tstring_free(cond_t);
				}
				tstring_free(blk);
			}
			tstring_free(blk_full);
		}
		if (cond)
			tstring_free(cond);
		tstring_free(contents);
	}

	/* if(cond){blk} */
	if (len >= 2 && strncmp(unit, "if", 2) == 0 &&
	    (len == 2 || !tlex_is_name_char(unit[2]))) {
		tstring *contents = trim_ts_dup(unit + 2);
		tstring *cond = NULL, *blk = NULL;
		uint_lexs idx_cond;
		if (get_first_parenthesis_ts(contents, &cond, &idx_cond)) {
			tstring *blk_full =
				trim_ts_dup(tstring_cstr(contents) + idx_cond + 1);
			uint_lexs idx_blk;
			if (get_first_block_ts(blk_full, &blk, &idx_blk)) {
				if (idx_blk == tstring_len(blk_full) - 1) {
					tstring_free(contents);
					tstring_free(blk_full);
					EMIT_TOKEN_TS(
						token_if, 2, cond, blk, NULL);
				} else {
					tstring_free(blk);
				}
			}
			tstring_free(blk_full);
		}
		if (cond)
			tstring_free(cond);
		tstring_free(contents);
	}

	/* elif(cond){blk} */
	if (len >= 4 && strncmp(unit, "elif", 4) == 0 &&
	    (len == 4 || !tlex_is_name_char(unit[4]))) {
		tstring *contents = trim_ts_dup(unit + 4);
		tstring *cond = NULL, *blk = NULL;
		uint_lexs idx_cond;
		if (get_first_parenthesis_ts(contents, &cond, &idx_cond)) {
			tstring *blk_full =
				trim_ts_dup(tstring_cstr(contents) + idx_cond + 1);
			uint_lexs idx_blk;
			if (get_first_block_ts(blk_full, &blk, &idx_blk)) {
				if (idx_blk == tstring_len(blk_full) - 1) {
					tstring_free(contents);
					tstring_free(blk_full);
					EMIT_TOKEN_TS(token_elif,
						      2,
						      cond,
						      blk,
						      NULL);
				} else {
					tstring_free(blk);
				}
			}
			tstring_free(blk_full);
		}
		if (cond)
			tstring_free(cond);
		tstring_free(contents);
	}

	/* else{blk} */
	if (len >= 4 && strncmp(unit, "else", 4) == 0 &&
	    (len == 4 || !tlex_is_name_char(unit[4]))) {
		tstring *contents = trim_ts_dup(unit + 4);
		tstring *blk = NULL;
		uint_lexs idx_blk;
		if (get_first_block_ts(contents, &blk, &idx_blk)) {
			if (idx_blk == tstring_len(contents) - 1) {
				tstring_free(contents);
				EMIT_TOKEN_TS(token_else, 1, blk, NULL, NULL);
			}
			tstring_free(blk);
		}
		tstring_free(contents);
	}

	/* Compatibility form: #{ expression } */
	if (unit[0] == '#') {
		tstring *contents = trim_ts_dup(unit + 1);
		tstring *blk = NULL;
		uint_lexs idx_blk;
		if (get_first_block_ts(contents, &blk, &idx_blk)) {
			if (idx_blk == tstring_len(contents) - 1) {
				tstring_free(contents);
				EMIT_TOKEN_TS(token_kappa, 1, blk, NULL, NULL);
			}
			tstring_free(blk);
		}
		tstring_free(contents);
	}

	/* single string '' */
	{
		tstring *gencontents = NULL;
		if (get_first_single_quote_ts(unit_ts, &gencontents, &genloc)) {
			if (genloc == len - 1) {
				EMIT_TOKEN_TS(token_sstr, 1, gencontents, NULL, NULL);
			}
			tstring_free(gencontents);
		}
	}

	/* double string "" */
	{
		tstring *gencontents = NULL;
		if (get_first_double_quote_ts(unit_ts, &gencontents, &genloc)) {
			if (genloc == len - 1) {
				EMIT_TOKEN_TS(token_dstr, 1, gencontents, NULL, NULL);
			}
			tstring_free(gencontents);
		}
	}

	/* { key : value, ... } dictionary */
	if (unit[0] == '{') {
		tstring *contents = NULL;
		uint_lexs idx;
		if (get_first_block_ts(unit_ts, &contents, &idx)) {
			if (idx == len - 1) {
				EMIT_TOKEN_TS(token_dict, 1, contents, NULL, NULL);
			}
			tstring_free(contents);
		}
	}

	/* function(params){blk} */
	if (len >= 8 && strncmp(unit, "function", 8) == 0 &&
	    (len == 8 || !tlex_is_name_char(unit[8]))) {
		tstring *contents = trim_ts_dup(unit + 8);
		tstring *params = NULL, *blk = NULL;
		uint_lexs idx_par;
		if (get_first_parenthesis_ts(contents, &params, &idx_par)) {
			tstring *blk_full =
				trim_ts_dup(tstring_cstr(contents) + idx_par + 1);
			uint_lexs idx_blk;
			if (get_first_block_ts(blk_full, &blk, &idx_blk)) {
				if (idx_blk == tstring_len(blk_full) - 1) {
					tstring_free(contents);
					tstring_free(blk_full);
					EMIT_TOKEN_TS(token_func,
						      2,
						      params,
						      blk,
						      NULL);
				} else {
					tstring_free(blk);
				}
			}
			tstring_free(blk_full);
		}
		if (params)
			tstring_free(params);
		tstring_free(contents);
	}

	/* (params){blk} function literal */
	if (unit[0] == '(') {
		tstring *params = NULL, *blk = NULL;
		uint_lexs idx_par;
		if (get_first_parenthesis_ts(unit_ts, &params, &idx_par)) {
			tstring *blk_full = trim_ts_dup(unit + idx_par + 1);
			uint_lexs idx_blk;
			if (get_first_block_ts(blk_full, &blk, &idx_blk)) {
				if (idx_blk == tstring_len(blk_full) - 1) {
					tstring_free(blk_full);
					EMIT_TOKEN_TS(token_func,
						      2,
						      params,
						      blk,
						      NULL);
				} else {
					tstring_free(blk);
				}
			}
			tstring_free(blk_full);
		}
		if (params)
			tstring_free(params);
	}

	/* Compatibility form #{ ... } is already handled above. */

	/* Assignment is a statement form whose right side may contain any
	 * expression operator, so split it before expression precedence. */
	{
		uint_lexs off_eq = 0;
		if (tlex_find_top_level_assignment_ts(unit_ts, &off_eq)) {
			tstring *contents = NULL;
			uint_lexs idx_br;
			tstring *lhs_t = trim_ts_new_len(unit, off_eq);
			if (get_last_bracket_ts(lhs_t, &contents, &idx_br)) {
				tstring *trim_obj =
					trim_ts_new_len(tstring_cstr(lhs_t), idx_br);
				tstring *trim_params = trim_ts_dup(tstring_cstr(contents));
				tstring *trim_right = trim_ts_dup(unit + off_eq + 1);
				if (!tstring_empty(trim_obj) && !tstring_empty(trim_right)) {
					tstring_free(lhs_t);
					tstring_free(contents);
					EMIT_TOKEN_TS(token_idxl, 3,
						      trim_obj, trim_params, trim_right);
				}
				tstring_free(trim_obj);
				tstring_free(trim_params);
				tstring_free(trim_right);
				tstring_free(contents);
			}
			tstring *trim_r = trim_ts_dup(unit + off_eq + 1);
			if (!tstring_empty(lhs_t) && !tstring_empty(trim_r))
				EMIT_TOKEN_TS(token_asg, 2, lhs_t, trim_r, NULL);
			tstring_free(lhs_t);
			tstring_free(trim_r);
		}
	}

	/* Binary operators, from lowest to highest precedence. */
	{
		/* pair */
		uint_lexs off = 0;
		if (find_last_single_colon_ts(unit_ts, &off) && off > 0)
			EMIT_BINARY_TS(token_pair, off, off + 1);
	}
	{
		uint_lexs off = 0;
		/* or has lower precedence than and. */
		if (find_last_top_level_word_cstr(unit_ts, "or", &off)) {
			EMIT_BINARY_TS(token_or, off, off + 2);
		}
		if (len >= 2 && strncmp(unit, "or", 2) == 0 &&
		    (len == 2 || !tlex_is_name_char(unit[2]))) {
			twarn(ErrCompile_Other, "get_tokens", "bare 'or'");
		}
	}
	{
		uint_lexs off = 0;
		if (find_last_top_level_word_cstr(unit_ts, "and", &off)) {
			EMIT_BINARY_TS(token_and, off, off + 3);
		}
		if (len >= 3 && strncmp(unit, "and", 3) == 0 &&
		    (len == 3 || !tlex_is_name_char(unit[3])))
			twarn(ErrCompile_Other, "get_tokens", "bare 'and'");
	}
	{
		uint_lexs off = 0;
		if (tlex_find_last_top_level_char_ts(unit_ts, '|', &off))
			EMIT_BINARY_TS(token_bor, off, off + 1);
	}
	{
		uint_lexs off = 0;
		if (tlex_find_last_top_level_char_ts(unit_ts, '&', &off))
			EMIT_BINARY_TS(token_band, off, off + 1);
	}
	{
		uint_lexs off = 0;
		if (count_top_level_word_cstr(unit_ts, "in") > 1)
			twarn(ErrCompile_InvalidLiter, "get_tokens", "chained 'in'");
		if (find_last_top_level_word_cstr(unit_ts, "in", &off))
			EMIT_BINARY_TS(token_in, off, off + 2);
	}
	{
		uint_lexs off = 0;
		if (count_top_level_word_cstr(unit_ts, "to") > 1)
			twarn(ErrCompile_InvalidLiter, "get_tokens", "chained 'to'");
		if (find_last_top_level_word_cstr(unit_ts, "to", &off))
			EMIT_BINARY_TS(token_to, off, off + 2);
	}
	{
		if (count_top_level_comparisons(unit_ts) > 1)
			twarn(ErrCompile_InvalidLiter, "get_tokens", "chained comparison");
		uint_lexs off = 0;
		if (tlex_find_last_top_level_char_ts(unit_ts, '=', &off)) {
			if (off > 0 && unit[off - 1] == '!') {
				EMIT_BINARY_TS(token_ne, off - 1, off + 1);
			} else if (off > 0 && unit[off - 1] == '=') {
				EMIT_BINARY_TS(token_eq, off - 1, off + 1);
			} else if (off > 0 && (unit[off - 1] == '>' || unit[off - 1] == '<')) {
				char op_char = unit[off - 1];
				if (op_char == '>')
					EMIT_BINARY_TS(token_ge, off - 1, off + 1);
				else
					EMIT_BINARY_TS(token_le, off - 1, off + 1);
			}
		}
	}
	{
		uint_lexs off = 0;
		if (tlex_find_last_top_level_char_ts(unit_ts, '>', &off)) {
			if (!(off > 0 && unit[off - 1] == '='))
				EMIT_BINARY_TS(token_sg, off, off + 1);
		}
	}
	{
		uint_lexs off = 0;
		if (tlex_find_last_top_level_char_ts(unit_ts, '<', &off)) {
			if (!(off > 0 && unit[off - 1] == '='))
				EMIT_BINARY_TS(token_sl, off, off + 1);
		}
	}
	{
		uint_lexs off = 0;
		if (find_last_top_level_binary_sign_ts(unit_ts, '+', &off)) {
			EMIT_BINARY_TS(token_add, off, off + 1);
		}
	}
	{
		uint_lexs off = 0;
		if (find_last_top_level_binary_sign_ts(unit_ts, '-', &off)) {
			EMIT_BINARY_TS(token_sub, off, off + 1);
		}
	}
	{
		uint_lexs off = 0;
		char operator_ch = '\0';
		if (find_last_top_level_multiplicative_ts(
			    unit_ts, &operator_ch, &off)) {
			switch (operator_ch) {
			case '*': EMIT_BINARY_TS(token_mul, off, off + 1);
			case '/': EMIT_BINARY_TS(token_div, off, off + 1);
			case '%': EMIT_BINARY_TS(token_mod, off, off + 1);
			case '@': EMIT_BINARY_TS(token_mmul, off, off + 1);
			}
		}
	}
	/* Unary signs bind less tightly than exponentiation and more tightly than
	 * multiplication. A sign is represented explicitly instead of being
	 * rewritten as arithmetic with zero. */
	if (unit[0] == '+' || unit[0] == '-') {
		tstring *operand = trim_ts_dup(unit + 1);
		if (tstring_empty(operand)) {
			tstring_free(operand);
			twarn(ErrCompile_InvalidLiter, "get_tokens", "missing unary operand");
		}
		EMIT_TOKEN_TS(unit[0] == '+' ? token_pos : token_neg,
			      1, operand, NULL, NULL);
	}
	{
		uint_lexs off = 0;
		if (find_first_top_level_char_ts(unit_ts, '^', &off)) {
			EMIT_BINARY_TS(token_pow, off, off + 1);
		}
	}
	{
		/* eval: f(params) */
		tstring *contents = NULL;
		uint_lexs idx;
		if (get_last_parenthesis_ts(unit_ts, &contents, &idx)) {
			if (idx > 0) {
				tstring *trim_l = trim_ts_new_len(unit, idx);
				tstring *trim_r = trim_ts_dup(tstring_cstr(contents));
				if (!tstring_empty(trim_l)) {
					tstring_free(contents);
					EMIT_TOKEN_TS(token_eval, 2, trim_l, trim_r, NULL);
				}
				tstring_free(trim_l);
				tstring_free(trim_r);
			}
			tstring_free(contents);
		}
	}
	{
		/* idx: f[params] */
		tstring *contents = NULL;
		uint_lexs idx;
		if (get_last_bracket_ts(unit_ts, &contents, &idx)) {
			tstring *trim_l = trim_ts_new_len(unit, idx);
			tstring *trim_r = trim_ts_dup(tstring_cstr(contents));
			if (!tstring_empty(trim_l)) {
				tstring_free(contents);
				EMIT_TOKEN_TS(token_idx, 2, trim_l, trim_r, NULL);
			}
			else {
				/* No left — this is [obj, ...] */
				tstring_free(trim_l);
				tstring_free(contents);
				EMIT_TOKEN_TS(token_idx, 2, NULL, trim_r, NULL);
			}
			tstring_free(trim_l);
			tstring_free(trim_r);
			tstring_free(contents);
		}
	}
	{
		/* idx2: obj::name, plus legacy/tunnel obj.name */
		uint_lexs off = 0;
		size_t sep_len = 2;
		if (tlex_find_last_top_level_idx2_sep_ts(unit_ts, &off, &sep_len)) {
			if (off > 0 && !isdigit((unsigned char)unit[0]) &&
			    unit[off + sep_len] != '\0') {
				tstring *trim_l =
					trim_ts_new_len(unit, off);
				tstring *trim_r = trim_ts_dup(unit + off + sep_len);
				EMIT_TOKEN_TS(token_idx2, 2, trim_l, trim_r, NULL);
			}
		}
	}
	tstring *final = tstring_dup(unit_ts);
	tstring_trim(final);
	if (!tstring_empty(final))
		EMIT_TOKEN_TS(token_v, 1, final, NULL, NULL);
	tstring_free(final);
	tstring_free(unit_ts);
}

/** Free token array */
void get_tokens_free(ttoken *tokens, uint32_t count)
{
	uint32_t i;
	for (i = 0; i < count; i++)
		ttoken_free(&tokens[i]);
	free(tokens);
}

/*===========================================================================*
 * 6. High-level lexing — tstring-based units
 *===========================================================================*/

tstring *preprocessing_1_ts(const tstring *fullcmd)
{
	tstring *cmd = tstring_dup(fullcmd);
	tstring_trim(cmd);
	const char *buf = tstring_cstr(cmd);
	size_t len = tstring_len(cmd);
	size_t i;
	tunit_ctr ctr;
	tunit_ctr_init(&ctr);
	for (i = 0; i + 1 < len; i++) {
		if (tunit_ctr_independent(&ctr) && buf[i] == '/' && buf[i + 1] == '/') {
			tstring_assign_len(cmd, buf, i);
			break;
		}
		tunit_ctr_update(&ctr, buf[i]);
	}
	return cmd;
}

void split_params_ts(const tstring *src, tstring ***params, uint_regs *nparams)
{
	tstring **tmp = NULL;
	uint32_t count = 0;
	tstring *comma = tstring_new(",");
	tlex_split_top_level_ts(src, comma, &tmp, &count);
	tstring_free(comma);
	*nparams = (uint_regs)count;
	*params = (tstring **)malloc(count * sizeof(tstring *));
	uint_regs i;
	for (i = 0; i < *nparams; i++) {
		(*params)[i] = tmp[i];
	}
	free(tmp);
}

void split_params_free(tstring **params, uint_regs nparams)
{
	uint_regs i;
	for (i = 0; i < nparams; i++)
		tstring_free(params[i]);
	free(params);
}

void lex_str_ts(const tstring *src, tstring ***units, uint32_t *n_units)
{
	tstring *separators = tstring_new(";\n");
	tlex_split_top_level_ts(src, separators, units, n_units);
	tstring_free(separators);
}

void lex_str_free(tstring **units, uint32_t n_units)
{
	uint32_t i;
	for (i = 0; i < n_units; i++)
		tstring_free(units[i]);
	free(units);
}

void lex_file(FILE *f, tstring ***units, uint32_t *n_units)
{
	tstring *buf = tstring_new_empty();
	int c;
	while ((c = fgetc(f)) != EOF)
		tstring_append_c(buf, (char)c);

	lex_str_ts(buf, units, n_units);
	tstring_free(buf);
}
