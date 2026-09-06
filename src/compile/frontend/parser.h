#ifndef TAPAS_COMPILE_PARSER_H
#define TAPAS_COMPILE_PARSER_H

#include "compile/frontend/ast.h"

typedef struct tdiagnostics tdiagnostics;

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	const tsource_document *document;
	const tsyntax_tokens *tokens;
	uint32_t cursor;
	tast_arena *arena;
	tdiagnostics *diagnostics;
	uint32_t recursion_depth;
	uint32_t recursion_limit;
} tparser;

void tparser_init(tparser *parser,
		  const tsource_document *document,
		  const tsyntax_tokens *tokens,
		  tast_arena *arena,
		  tdiagnostics *diagnostics);

tast_id tparser_parse_expression(tparser *parser);

tast_id tparser_parse_statement(tparser *parser);

tast_id tparser_parse_module(tparser *parser);

#ifdef __cplusplus
}
#endif

#endif /* TAPAS_COMPILE_PARSER_H */
