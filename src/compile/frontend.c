#include "tapas/compile/frontend.h"

void tfrontend_init(tfrontend *frontend, const char *name,
		    const char *source, tfrontend_mode mode)
{
	*frontend = (tfrontend){ .root = TAST_INVALID_ID, .mode = mode };
	tsource_document_init(&frontend->document, name, source);
	tsyntax_tokens_init(&frontend->tokens);
	tsyntax_lex(&frontend->document, &frontend->tokens);
	tast_arena_init(&frontend->arena);
	tdiagnostics_init(&frontend->diagnostics);
	tsemantic_model_init(&frontend->semantic);
	ttype_info_model_init(&frontend->types);
	tparser parser;
	tparser_init(&parser, &frontend->document, &frontend->tokens,
		&frontend->arena, &frontend->diagnostics);
	switch (mode) {
	case tfrontend_expression:
		frontend->root = tparser_parse_expression(&parser);
		break;
	case tfrontend_statement:
		frontend->root = tparser_parse_statement(&parser);
		break;
	case tfrontend_module:
		frontend->root = tparser_parse_module(&parser);
		break;
	}
	/* Editors spend most of their time holding temporarily invalid source.
	 * Error nodes are intentionally traversable, so preserve all semantic
	 * information that can still be recovered after a parser diagnostic. */
	tsemantic_analyze(&frontend->document, &frontend->arena,
		frontend->root, &frontend->semantic, &frontend->diagnostics);
	ttype_info_analyze(&frontend->document, &frontend->arena,
		&frontend->semantic, &frontend->types);
}

void tfrontend_free(tfrontend *frontend)
{
	if (!frontend)
		return;
	ttype_info_model_free(&frontend->types);
	tsemantic_model_free(&frontend->semantic);
	tdiagnostics_free(&frontend->diagnostics);
	tast_arena_free(&frontend->arena);
	tsyntax_tokens_free(&frontend->tokens);
	tsource_document_free(&frontend->document);
	frontend->root = TAST_INVALID_ID;
}

int tfrontend_valid(const tfrontend *frontend)
{
	return frontend && frontend->root != TAST_INVALID_ID &&
		frontend->diagnostics.count == 0;
}
