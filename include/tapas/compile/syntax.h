#ifndef TAPAS_COMPILE_SYNTAX_H
#define TAPAS_COMPILE_SYNTAX_H

#include "tapas/compile/source.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	tsyntax_eof = 0,
	tsyntax_invalid,
	tsyntax_whitespace,
	tsyntax_newline,
	tsyntax_comment,
	tsyntax_identifier,
	tsyntax_integer,
	tsyntax_float,
	tsyntax_string,

	tsyntax_kw_and,
	tsyntax_kw_as,
	tsyntax_kw_base,
	tsyntax_kw_break,
	tsyntax_kw_continue,
	tsyntax_kw_elif,
	tsyntax_kw_else,
	tsyntax_kw_false,
	tsyntax_kw_for,
	tsyntax_kw_function,
	tsyntax_kw_if,
	tsyntax_kw_import,
	tsyntax_kw_in,
	tsyntax_kw_let,
	tsyntax_kw_nil,
	tsyntax_kw_not,
	tsyntax_kw_of,
	tsyntax_kw_or,
	tsyntax_kw_return,
	tsyntax_kw_rule,
	tsyntax_kw_implies,
	tsyntax_kw_this,
	tsyntax_kw_to,
	tsyntax_kw_true,
	tsyntax_kw_var,
	tsyntax_kw_while,

	tsyntax_lparen,
	tsyntax_rparen,
	tsyntax_lbracket,
	tsyntax_rbracket,
	tsyntax_lbrace,
	tsyntax_rbrace,
	tsyntax_comma,
	tsyntax_colon,
	tsyntax_semicolon,
	tsyntax_dot,
	tsyntax_scope,
	tsyntax_ellipsis,
	tsyntax_arrow,
	tsyntax_assign,
	tsyntax_eq,
	tsyntax_ne,
	tsyntax_gt,
	tsyntax_ge,
	tsyntax_lt,
	tsyntax_le,
	tsyntax_plus,
	tsyntax_minus,
	tsyntax_star,
	tsyntax_slash,
	tsyntax_percent,
	tsyntax_power,
	tsyntax_matmul,
	tsyntax_element_and,
	tsyntax_element_or
} tsyntax_kind;

typedef struct {
	tsyntax_kind kind;
	tsource_span span;
} tsyntax_token;

typedef struct {
	tsyntax_token *items;
	uint32_t count;
	uint32_t capacity;
} tsyntax_tokens;

const char *tsyntax_kind_name(tsyntax_kind kind);
uint32_t tsyntax_keyword_count(void);
const char *tsyntax_keyword_at(uint32_t index);
int tsyntax_kind_is_trivia(tsyntax_kind kind);
void tsyntax_tokens_init(tsyntax_tokens *tokens);
void tsyntax_tokens_free(tsyntax_tokens *tokens);
void tsyntax_lex(const tsource_document *document, tsyntax_tokens *tokens);

#ifdef __cplusplus
}
#endif

#endif /* TAPAS_COMPILE_SYNTAX_H */
