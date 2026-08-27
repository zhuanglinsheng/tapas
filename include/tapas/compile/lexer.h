#ifndef T_LEX_H
#define T_LEX_H

#include "Tapas/tenv.h"

#include <ctype.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================*
 * 1. Unit Counter (parenthesis/bracket/brace/quote tracking)
 *===========================================================================*/

typedef struct {
	int_lexs parenthesis;
	int_lexs bracket;
	int_lexs curlybrace;
	int_lexs singlequote;
	int_lexs doublequote;
} tunit_ctr;

void tunit_ctr_init(tunit_ctr *c);
void tunit_ctr_restore(tunit_ctr *c);
int tunit_ctr_out_of_single_string(const tunit_ctr *c);
int tunit_ctr_out_of_double_string(const tunit_ctr *c);
int tunit_ctr_out_of_string(const tunit_ctr *c);
int tunit_ctr_out_of_parenthesis(const tunit_ctr *c);
int tunit_ctr_out_of_bracket(const tunit_ctr *c);
int tunit_ctr_out_of_curlybrace(const tunit_ctr *c);
int tunit_ctr_independent(const tunit_ctr *c);
int tunit_ctr_update(tunit_ctr *c, char ch);

/*===========================================================================*
 * 2. Structured source scanning
 *===========================================================================*/

int tlex_is_name_char(char ch);
int tlex_find_top_level_assignment_ts(const tstring *src, uint_lexs *off);
int tlex_find_last_top_level_char_ts(const tstring *src, char ch, uint_lexs *off);
int tlex_find_last_top_level_word_ts(const tstring *src, const tstring *word, uint_lexs *off);
int tlex_find_last_top_level_idx2_sep_ts(const tstring *src, uint_lexs *off, size_t *sep_len);
int tlex_get_first_enclosed_ts(const tstring *src, char open, char close, tstring **out, uint_lexs *loc);
int tlex_get_last_enclosed_ts(const tstring *src, char open, char close, tstring **out, uint_lexs *loc);
tstring *tlex_strip_outer_parentheses_ts(const tstring *src);
void tlex_split_top_level_ts(const tstring *src, const tstring *delimiters, tstring ***result, uint32_t *result_count);

/*===========================================================================*
 * 3. Token — val fields use tstring
 *===========================================================================*/

typedef struct {
	token_type type;
	uint8_t nvals;
	tstring *val1;
	tstring *val2;
	tstring *val3;
} ttoken;

void ttoken_free(ttoken *t);
void get_tokens_ts(const tstring *src, ttoken **out_tokens, uint32_t *out_count);
void get_tokens_free(ttoken *tokens, uint32_t count);

/*===========================================================================*
 * 4. High-level Lexing
 *===========================================================================*/

tstring *preprocessing_1_ts(const tstring *fullcmd);
void split_params_ts(const tstring *src, tstring ***params, uint_regs *nparams);
void split_params_free(tstring **params, uint_regs nparams);
void lex_str_ts(const tstring *src, tstring ***units, uint32_t *n_units);
void lex_str_free(tstring **units, uint32_t n_units);
void lex_file(FILE *f, tstring ***units, uint32_t *n_units);

#ifdef __cplusplus
}
#endif

#endif /* T_LEX_H */
