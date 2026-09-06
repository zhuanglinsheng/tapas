#include "tapas/compile/syntax.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

typedef struct { const char *text; tsyntax_kind kind; } keyword_entry;

static const keyword_entry keywords[] = {
	{ "and", tsyntax_kw_and }, { "as", tsyntax_kw_as },
	{ "base", tsyntax_kw_base }, { "break", tsyntax_kw_break },
	{ "continue", tsyntax_kw_continue }, { "elif", tsyntax_kw_elif },
	{ "else", tsyntax_kw_else }, { "false", tsyntax_kw_false },
	{ "for", tsyntax_kw_for }, { "function", tsyntax_kw_function },
	{ "if", tsyntax_kw_if }, { "import", tsyntax_kw_import },
	{ "in", tsyntax_kw_in }, { "let", tsyntax_kw_let },
	{ "nil", tsyntax_kw_nil }, { "not", tsyntax_kw_not }, { "of", tsyntax_kw_of },
	{ "or", tsyntax_kw_or }, { "return", tsyntax_kw_return },
	{ "rule", tsyntax_kw_rule },
	{ "implies", tsyntax_kw_implies },
	{ "this", tsyntax_kw_this }, { "to", tsyntax_kw_to },
	{ "true", tsyntax_kw_true }, { "var", tsyntax_kw_var },
	{ "while", tsyntax_kw_while }
};

uint32_t tsyntax_keyword_count(void)
{
	return (uint32_t)(sizeof(keywords) / sizeof(keywords[0]));
}

const char *tsyntax_keyword_at(uint32_t index)
{
	return index < tsyntax_keyword_count() ? keywords[index].text : nullptr;
}

static void tokens_push(tsyntax_tokens *tokens, tsyntax_kind kind,
			uint32_t start, uint32_t end)
{
	if (tokens->count >= tokens->capacity) {
		uint32_t capacity = tokens->capacity ? tokens->capacity * 2 : 64;
		tsyntax_token *items = (tsyntax_token *)realloc(
			tokens->items, capacity * sizeof(tsyntax_token));
		if (!items)
			abort();
		tokens->items = items;
		tokens->capacity = capacity;
	}
	tokens->items[tokens->count++] = (tsyntax_token){
		.kind = kind,
		.span = { start, end }
	};
}

void tsyntax_tokens_init(tsyntax_tokens *tokens)
{
	*tokens = (tsyntax_tokens){ 0 };
}

void tsyntax_tokens_free(tsyntax_tokens *tokens)
{
	if (!tokens)
		return;
	free(tokens->items);
	*tokens = (tsyntax_tokens){ 0 };
}

int tsyntax_kind_is_trivia(tsyntax_kind kind)
{
	return kind == tsyntax_whitespace || kind == tsyntax_newline ||
	       kind == tsyntax_comment;
}

static int identifier_start(unsigned char c)
{
	return c == '_' || isalpha(c) || c >= 0x80;
}

static int identifier_continue(unsigned char c)
{
	return identifier_start(c) || isdigit(c);
}

static tsyntax_kind identifier_kind(const char *text, uint32_t start,
				    uint32_t end)
{
	size_t length = end - start;
	for (size_t i = 0; i < sizeof(keywords) / sizeof(keywords[0]); i++) {
		if (strlen(keywords[i].text) == length &&
		    memcmp(text + start, keywords[i].text, length) == 0)
			return keywords[i].kind;
	}
	return tsyntax_identifier;
}

static uint32_t scan_exponent(const char *text, uint32_t length, uint32_t at)
{
	if (at >= length || (text[at] != 'e' && text[at] != 'E'))
		return at;
	uint32_t cursor = at + 1;
	if (cursor < length && (text[cursor] == '+' || text[cursor] == '-'))
		cursor++;
	uint32_t digits = cursor;
	while (cursor < length && isdigit((unsigned char)text[cursor]))
		cursor++;
	return cursor == digits ? at : cursor;
}

void tsyntax_lex(const tsource_document *document, tsyntax_tokens *tokens)
{
	tokens->count = 0;
	const char *text = tstring_cstr(document->text);
	uint32_t length = tsource_document_length(document);
	uint32_t at = 0;
	while (at < length) {
		uint32_t start = at;
		unsigned char c = (unsigned char)text[at];
		if (c == ' ' || c == '\t' || c == '\v' || c == '\f') {
			while (at < length && (text[at] == ' ' || text[at] == '\t' ||
			       text[at] == '\v' || text[at] == '\f'))
				at++;
			tokens_push(tokens, tsyntax_whitespace, start, at);
			continue;
		}
		if (c == '\r' || c == '\n') {
			at++;
			if (c == '\r' && at < length && text[at] == '\n')
				at++;
			tokens_push(tokens, tsyntax_newline, start, at);
			continue;
		}
		if (c == '/' && at + 1 < length && text[at + 1] == '/') {
			at += 2;
			while (at < length && text[at] != '\r' && text[at] != '\n')
				at++;
			tokens_push(tokens, tsyntax_comment, start, at);
			continue;
		}
		if (identifier_start(c)) {
			at++;
			while (at < length && identifier_continue((unsigned char)text[at]))
				at++;
			tokens_push(tokens, identifier_kind(text, start, at), start, at);
			continue;
		}
		if (isdigit(c) || (c == '.' && at + 1 < length &&
				     isdigit((unsigned char)text[at + 1]))) {
			int floating = c == '.';
			if (c == '.')
				at++;
			while (at < length && isdigit((unsigned char)text[at]))
				at++;
			if (at < length && text[at] == '.') {
				floating = 1;
				at++;
				while (at < length && isdigit((unsigned char)text[at]))
					at++;
			}
			uint32_t exponent = scan_exponent(text, length, at);
			if (exponent != at) {
				floating = 1;
				at = exponent;
			}
			tokens_push(tokens, floating ? tsyntax_float : tsyntax_integer,
				    start, at);
			continue;
		}
		if (c == '\'' || c == '"') {
			unsigned char quote = c;
			at++;
			while (at < length && (unsigned char)text[at] != quote &&
			       text[at] != '\r' && text[at] != '\n')
				at++;
			if (at < length && (unsigned char)text[at] == quote) {
				at++;
				tokens_push(tokens, tsyntax_string, start, at);
			} else
				tokens_push(tokens, tsyntax_invalid, start, at);
			continue;
		}

#define TWO(a, b, kind) if (c == (a) && at + 1 < length && text[at + 1] == (b)) { at += 2; tokens_push(tokens, (kind), start, at); continue; }
		TWO(':', ':', tsyntax_scope)
		TWO('=', '=', tsyntax_eq)
		TWO('!', '=', tsyntax_ne)
		TWO('>', '=', tsyntax_ge)
		TWO('<', '=', tsyntax_le)
		TWO('-', '>', tsyntax_arrow)
#undef TWO
		if (c == '.' && at + 2 < length && text[at + 1] == '.' &&
		    text[at + 2] == '.') {
			at += 3;
			tokens_push(tokens, tsyntax_ellipsis, start, at);
			continue;
		}
		at++;
		tsyntax_kind kind = tsyntax_invalid;
		switch (c) {
		case '(': kind = tsyntax_lparen; break;
		case ')': kind = tsyntax_rparen; break;
		case '[': kind = tsyntax_lbracket; break;
		case ']': kind = tsyntax_rbracket; break;
		case '{': kind = tsyntax_lbrace; break;
		case '}': kind = tsyntax_rbrace; break;
		case ',': kind = tsyntax_comma; break;
		case ':': kind = tsyntax_colon; break;
		case ';': kind = tsyntax_semicolon; break;
		case '.': kind = tsyntax_dot; break;
		case '=': kind = tsyntax_assign; break;
		case '>': kind = tsyntax_gt; break;
		case '<': kind = tsyntax_lt; break;
		case '+': kind = tsyntax_plus; break;
		case '-': kind = tsyntax_minus; break;
		case '*': kind = tsyntax_star; break;
		case '/': kind = tsyntax_slash; break;
		case '%': kind = tsyntax_percent; break;
		case '^': kind = tsyntax_power; break;
		case '@': kind = tsyntax_matmul; break;
		case '&': kind = tsyntax_element_and; break;
		case '|': kind = tsyntax_element_or; break;
		}
		tokens_push(tokens, kind, start, at);
	}
	tokens_push(tokens, tsyntax_eof, length, length);
}

const char *tsyntax_kind_name(tsyntax_kind kind)
{
	static const char *const names[] = {
		"eof", "invalid", "whitespace", "newline", "comment",
		"identifier", "integer", "float", "string", "and", "as",
		"base", "break", "continue", "elif", "else", "false", "for",
		"function", "if", "import", "in", "let", "nil", "not", "of", "or",
		"return", "rule", "implies", "this", "to", "true", "var", "while", "(", ")",
		"[", "]", "{", "}", ",", ":", ";", ".", "::", "...", "->",
		"=", "==", "!=", ">", ">=", "<", "<=", "+", "-", "*",
		"/", "%", "^", "@", "&", "|"
	};
	return (unsigned)kind < sizeof(names) / sizeof(names[0]) ?
		names[kind] : "unknown";
}
