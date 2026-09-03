#include "tapas/compile/parser.h"
#include "tapas/compile/diagnostic.h"

#include <stdlib.h>

typedef struct {
	uint8_t left;
	uint8_t right;
	uint8_t non_associative;
} binding_power;

static const tsyntax_token *raw_token(const tparser *parser, uint32_t at)
{
	if (at >= parser->tokens->count)
		return &parser->tokens->items[parser->tokens->count - 1];
	return &parser->tokens->items[at];
}

static uint32_t next_significant(const tparser *parser, uint32_t at)
{
	while (at < parser->tokens->count &&
	       tsyntax_kind_is_trivia(parser->tokens->items[at].kind))
		at++;
	return at;
}

static const tsyntax_token *peek(const tparser *parser)
{
	return raw_token(parser, next_significant(parser, parser->cursor));
}

static const tsyntax_token *advance(tparser *parser)
{
	parser->cursor = next_significant(parser, parser->cursor);
	const tsyntax_token *token = raw_token(parser, parser->cursor);
	if (parser->cursor < parser->tokens->count)
		parser->cursor++;
	return token;
}

static int consume(tparser *parser, tsyntax_kind kind,
		   const tsyntax_token **token)
{
	if (peek(parser)->kind != kind)
		return 0;
	const tsyntax_token *matched = advance(parser);
	if (token)
		*token = matched;
	return 1;
}

static tast_id error_node(tparser *parser, tsource_span span,
			  const char *message)
{
	tdiagnostics_add(parser->diagnostics, tdiagnostic_error, span, message);
	return tast_arena_add(parser->arena, (tast_node){
		.kind = tast_error,
		.span = span
	});
}

static binding_power infix_power(tsyntax_kind kind)
{
	switch (kind) {
	case tsyntax_colon: return (binding_power){ 1, 1, 0 };
	case tsyntax_kw_or: return (binding_power){ 2, 3, 0 };
	case tsyntax_kw_and: return (binding_power){ 3, 4, 0 };
	case tsyntax_element_or: return (binding_power){ 4, 5, 0 };
	case tsyntax_element_and: return (binding_power){ 5, 6, 0 };
	case tsyntax_kw_in: return (binding_power){ 6, 7, 1 };
	case tsyntax_kw_to: return (binding_power){ 7, 8, 1 };
	case tsyntax_eq:
	case tsyntax_ne:
	case tsyntax_gt:
	case tsyntax_ge:
	case tsyntax_lt:
	case tsyntax_le:
		return (binding_power){ 8, 9, 1 };
	case tsyntax_plus:
	case tsyntax_minus:
		return (binding_power){ 10, 11, 0 };
	case tsyntax_star:
	case tsyntax_slash:
	case tsyntax_percent:
	case tsyntax_matmul:
		return (binding_power){ 20, 21, 0 };
	case tsyntax_power:
		return (binding_power){ 30, 30, 0 };
	default:
		return (binding_power){ 0, 0, 0 };
	}
}

static tast_id parse_bp(tparser *parser, uint8_t minimum);
static tast_id parse_block(tparser *parser);
static tast_id parse_rule(tparser *parser, tsource_span start);

static int lparen_begins_function(const tparser *parser)
{
	uint32_t at = next_significant(parser, parser->cursor);
	uint32_t depth = 0;
	while (at < parser->tokens->count) {
		tsyntax_kind kind = parser->tokens->items[at].kind;
		if (kind == tsyntax_lparen)
			depth++;
		else if (kind == tsyntax_rparen) {
			if (depth)
				depth--;
			else {
				at = next_significant(parser, at + 1);
				return raw_token(parser, at)->kind == tsyntax_lbrace ||
				       raw_token(parser, at)->kind == tsyntax_arrow;
			}
		}
		if (kind == tsyntax_eof)
			break;
		at++;
	}
	return 0;
}

static tast_id parse_function(tparser *parser, tsource_span start)
{
	tast_id *parameters = nullptr;
	uint32_t count = 0;
	uint32_t capacity = 0;
	int variadic = consume(parser, tsyntax_ellipsis, nullptr);
	if (!variadic) {
		while (peek(parser)->kind != tsyntax_rparen &&
		       peek(parser)->kind != tsyntax_eof) {
			const tsyntax_token *name = peek(parser);
			if (name->kind != tsyntax_identifier) {
				error_node(parser, name->span,
					   "parameter name expected");
				break;
			}
			advance(parser);
			tsource_span annotation = { name->span.end, name->span.end };
			int has_annotation = consume(parser, tsyntax_colon, nullptr);
			if (has_annotation) {
				uint32_t annotation_start = next_significant(
					parser, parser->cursor);
				uint32_t at = annotation_start;
				uint32_t brackets = 0;
				while (at < parser->tokens->count) {
					tsyntax_kind kind = parser->tokens->items[at].kind;
					if (kind == tsyntax_lbracket)
						brackets++;
					else if (kind == tsyntax_rbracket && brackets)
						brackets--;
					else if (brackets == 0 &&
						 (kind == tsyntax_comma ||
						  kind == tsyntax_rparen ||
						  kind == tsyntax_eof))
						break;
					at++;
				}
				uint32_t annotation_end = at;
				while (annotation_end > annotation_start &&
				       tsyntax_kind_is_trivia(parser->tokens->items[
					       annotation_end - 1].kind))
					annotation_end--;
				if (annotation_end == annotation_start)
					tdiagnostics_add(parser->diagnostics,
						tdiagnostic_error, peek(parser)->span,
						"parameter type annotation expected");
				else
					annotation = (tsource_span){
						parser->tokens->items[annotation_start].span.start,
						parser->tokens->items[annotation_end - 1].span.end
					};
				parser->cursor = at;
			}
			if (count >= capacity) {
				capacity = capacity ? capacity * 2 : 4;
				parameters = (tast_id *)realloc(
					parameters, capacity * sizeof(tast_id));
				if (!parameters)
					abort();
			}
			parameters[count++] = tast_arena_add(parser->arena,
				(tast_node){
					.kind = tast_parameter,
					.span = name->span,
					.parameter = { name->span, annotation,
						       has_annotation }
				});
			if (!consume(parser, tsyntax_comma, nullptr))
				break;
			if (peek(parser)->kind == tsyntax_rparen)
				break;
		}
	}
	if (!consume(parser, tsyntax_rparen, nullptr))
		tdiagnostics_add(parser->diagnostics, tdiagnostic_error,
				 peek(parser)->span, "missing ')' after parameters");
	if (variadic && count != 0)
		tdiagnostics_add(parser->diagnostics, tdiagnostic_error,
				 start, "'...' must be the entire parameter list");

	tsource_span return_annotation = { start.end, start.end };
	int has_return_annotation = consume(parser, tsyntax_arrow, nullptr);
	if (has_return_annotation) {
		uint32_t annotation_start = next_significant(parser, parser->cursor);
		uint32_t at = annotation_start;
		uint32_t brackets = 0;
		while (at < parser->tokens->count) {
			tsyntax_kind kind = parser->tokens->items[at].kind;
			if (kind == tsyntax_lbracket)
				brackets++;
			else if (kind == tsyntax_rbracket && brackets)
				brackets--;
			else if (brackets == 0 &&
				 (kind == tsyntax_lbrace || kind == tsyntax_eof))
				break;
			at++;
		}
		uint32_t annotation_end = at;
		while (annotation_end > annotation_start &&
		       tsyntax_kind_is_trivia(
			       parser->tokens->items[annotation_end - 1].kind))
			annotation_end--;
		if (annotation_end == annotation_start)
			tdiagnostics_add(parser->diagnostics, tdiagnostic_error,
				peek(parser)->span,
				"function result type annotation expected");
		else
			return_annotation = (tsource_span){
				parser->tokens->items[annotation_start].span.start,
				parser->tokens->items[annotation_end - 1].span.end
			};
		parser->cursor = at;
	}

	uint32_t children = tast_arena_add_children(parser->arena, parameters, count);
	free(parameters);
	tast_id body = parse_block(parser);
	const tast_node *block = tast_get(parser->arena, body);
	return tast_arena_add(parser->arena, (tast_node){
		.kind = tast_function,
		.span = { start.start, block ? block->span.end : start.end },
		.function = {
			.parameters = children,
			.parameter_count = count,
			.body = body,
			.return_annotation = return_annotation,
			.variadic = variadic,
			.has_return_annotation = has_return_annotation
		}
	});
}

static tast_id parse_sequence(tparser *parser, tsyntax_kind closing,
			      tast_kind kind, tast_id receiver,
			      tsource_span opening)
{
	tast_id *children = nullptr;
	uint32_t count = 0;
	uint32_t capacity = 0;
	const tsyntax_token *close = nullptr;
	while (peek(parser)->kind != closing && peek(parser)->kind != tsyntax_eof) {
		if (count >= capacity) {
			capacity = capacity ? capacity * 2 : 4;
			children = (tast_id *)realloc(children,
						      capacity * sizeof(tast_id));
			if (!children)
				abort();
		}
		children[count++] = parse_bp(parser, 0);
		if (!consume(parser, tsyntax_comma, nullptr))
			break;
	}
	if (!consume(parser, closing, &close)) {
		tdiagnostics_add(parser->diagnostics, tdiagnostic_error,
				 peek(parser)->span, "missing closing delimiter");
	}
	uint32_t start = tast_arena_add_children(parser->arena, children, count);
	free(children);
	const tast_node *receiver_node = tast_get(parser->arena, receiver);
	uint32_t span_start = receiver_node ? receiver_node->span.start : opening.start;
	tsource_span span = tsource_span_make(
		span_start, close ? close->span.end : peek(parser)->span.start);
	return tast_arena_add(parser->arena, (tast_node){
		.kind = kind,
		.span = span,
		.aggregate = { receiver, start, count }
	});
}

static tast_id parse_dictionary(tparser *parser, tsource_span opening)
{
	tast_id *children = nullptr;
	uint32_t count = 0;
	uint32_t capacity = 0;
	int mode = 0; /* 1 dictionary, 2 structure */
	int saw_named = 0;
	const tsyntax_token *close = nullptr;
	while (peek(parser)->kind != tsyntax_rbrace &&
	       peek(parser)->kind != tsyntax_eof) {
		if (count + 2 > capacity) {
			capacity = capacity ? capacity * 2 : 8;
			while (capacity < count + 2)
				capacity *= 2;
			children = (tast_id *)realloc(children,
						      capacity * sizeof(tast_id));
			if (!children)
				abort();
		}
		tast_id left = parse_bp(parser, 2);
		if (consume(parser, tsyntax_colon, nullptr)) {
			if (mode == 2)
				tdiagnostics_add(parser->diagnostics, tdiagnostic_error,
					 peek(parser)->span,
					 "cannot mix dictionary and structure entries");
			mode = 1;
			children[count++] = left;
			children[count++] = parse_bp(parser, 0);
		} else {
			if (mode == 1)
				tdiagnostics_add(parser->diagnostics, tdiagnostic_error,
					 peek(parser)->span,
					 "dictionary entry requires ':'");
			mode = 2;
			if (consume(parser, tsyntax_assign, nullptr)) {
				const tast_node *name = tast_get(parser->arena, left);
				tast_id value = parse_bp(parser, 0);
				const tast_node *value_node = tast_get(parser->arena, value);
				if (!name || name->kind != tast_name)
					children[count++] = error_node(
						parser, name ? name->span : peek(parser)->span,
						"named field requires an identifier");
				else {
					children[count++] = tast_arena_add(
						parser->arena, (tast_node){
							.kind = tast_named_field,
							.span = { name->span.start,
								  value_node->span.end },
							.named_field = { name->span, value }
						});
				}
				saw_named = 1;
			} else {
				if (saw_named)
					tdiagnostics_add(
						parser->diagnostics, tdiagnostic_error,
						tast_get(parser->arena, left)->span,
						"positional field must precede named fields");
				children[count++] = left;
			}
		}
		if (!consume(parser, tsyntax_comma, nullptr))
			break;
	}
	if (!consume(parser, tsyntax_rbrace, &close))
		tdiagnostics_add(parser->diagnostics, tdiagnostic_error,
				 peek(parser)->span, "missing '}'");
	uint32_t start = tast_arena_add_children(parser->arena, children, count);
	free(children);
	return tast_arena_add(parser->arena, (tast_node){
		.kind = mode == 2 ? tast_structure : tast_dictionary,
		.span = { opening.start,
			  close ? close->span.end : peek(parser)->span.start },
		.aggregate = { TAST_INVALID_ID, start, count }
	});
}

static tast_id parse_index(tparser *parser, tast_id receiver,
			   tsource_span opening)
{
	tast_id *arguments = nullptr;
	uint32_t count = 0;
	uint32_t capacity = 0;
	const tsyntax_token *close = nullptr;
	while (peek(parser)->kind != tsyntax_rbracket &&
	       peek(parser)->kind != tsyntax_eof) {
		if (count == capacity) {
			capacity = capacity ? capacity * 2 : 4;
			arguments = (tast_id *)realloc(
				arguments, capacity * sizeof(tast_id));
			if (!arguments)
				abort();
		}
		int has_start = peek(parser)->kind != tsyntax_colon;
		tast_id start = has_start ? parse_bp(parser, 2) : TAST_INVALID_ID;
		if (consume(parser, tsyntax_colon, nullptr)) {
			int has_end = peek(parser)->kind != tsyntax_comma &&
				peek(parser)->kind != tsyntax_rbracket;
			tast_id end = has_end ? parse_bp(parser, 2) : TAST_INVALID_ID;
			const tast_node *start_node = tast_get(parser->arena, start);
			const tast_node *end_node = tast_get(parser->arena, end);
			arguments[count++] = tast_arena_add(parser->arena,
				(tast_node){
					.kind = tast_slice,
					.span = {
						has_start ? start_node->span.start : opening.end,
						has_end ? end_node->span.end : peek(parser)->span.start
					},
					.slice = { start, end, has_start, has_end }
				});
		} else
			arguments[count++] = start;
		if (!consume(parser, tsyntax_comma, nullptr))
			break;
	}
	if (!consume(parser, tsyntax_rbracket, &close))
		tdiagnostics_add(parser->diagnostics, tdiagnostic_error,
			peek(parser)->span, "missing ']'");
	uint32_t children = tast_arena_add_children(parser->arena,
		arguments, count);
	free(arguments);
	const tast_node *receiver_node = tast_get(parser->arena, receiver);
	return tast_arena_add(parser->arena, (tast_node){
		.kind = tast_index,
		.span = { receiver_node->span.start,
			  close ? close->span.end : peek(parser)->span.start },
		.aggregate = { receiver, children, count }
	});
}

static tast_id parse_primary(tparser *parser)
{
	const tsyntax_token *token = advance(parser);
	tast_kind kind;
	switch (token->kind) {
	case tsyntax_identifier:
	case tsyntax_kw_this:
	case tsyntax_kw_base:
		kind = tast_name;
		break;
	case tsyntax_kw_nil: kind = tast_nil; break;
	case tsyntax_kw_true:
	case tsyntax_kw_false: kind = tast_bool; break;
	case tsyntax_integer: kind = tast_integer; break;
	case tsyntax_float: kind = tast_float; break;
	case tsyntax_string: kind = tast_string; break;
	case tsyntax_plus:
	case tsyntax_minus: {
		tast_id operand = parse_bp(parser, 25);
		const tast_node *operand_node = tast_get(parser->arena, operand);
		return tast_arena_add(parser->arena, (tast_node){
			.kind = tast_unary,
			.span = { token->span.start,
				  operand_node ? operand_node->span.end : token->span.end },
			.unary = { token->kind, operand }
		});
	}
	case tsyntax_lparen: {
		if (lparen_begins_function(parser))
			return parse_function(parser, token->span);
		tast_id value = parse_bp(parser, 0);
		const tsyntax_token *close = nullptr;
		if (!consume(parser, tsyntax_rparen, &close))
			tdiagnostics_add(parser->diagnostics, tdiagnostic_error,
					 peek(parser)->span, "missing ')'");
		const tast_node *value_node = tast_get(parser->arena, value);
		return tast_arena_add(parser->arena, (tast_node){
			.kind = tast_group,
			.span = { token->span.start,
				  close ? close->span.end :
				  (value_node ? value_node->span.end : token->span.end) },
			.group = { value }
		});
	}
	case tsyntax_lbracket:
		return parse_sequence(parser, tsyntax_rbracket, tast_list,
				      TAST_INVALID_ID, token->span);
	case tsyntax_lbrace:
		return parse_dictionary(parser, token->span);
	case tsyntax_kw_rule:
		return parse_rule(parser, token->span);
	case tsyntax_kw_function:
		return error_node(parser, token->span,
			"'function' begins a named declaration, not an expression");
	case tsyntax_invalid:
		return error_node(parser, token->span, "invalid token");
	default:
		if (token->kind != tsyntax_eof)
			return error_node(parser, token->span, "expression expected");
		return error_node(parser, token->span, "unexpected end of input");
	}
	return tast_arena_add(parser->arena, (tast_node){
		.kind = kind,
		.span = token->span
	});
}

static tast_id parse_postfix(tparser *parser, tast_id left)
{
	for (;;) {
		const tsyntax_token *token = peek(parser);
		if (token->kind == tsyntax_lparen) {
			advance(parser);
			left = parse_sequence(parser, tsyntax_rparen, tast_call,
					      left, token->span);
			continue;
		}
		if (token->kind == tsyntax_lbracket) {
			advance(parser);
			left = parse_index(parser, left, token->span);
			continue;
		}
		if (token->kind == tsyntax_scope || token->kind == tsyntax_dot) {
			advance(parser);
			const tsyntax_token *name = peek(parser);
			if (name->kind != tsyntax_identifier &&
			    name->kind != tsyntax_kw_base &&
			    name->kind != tsyntax_kw_of &&
			    name->kind != tsyntax_kw_rule) {
				left = error_node(parser, name->span,
						  "member name expected");
				continue;
			}
			advance(parser);
			const tast_node *receiver = tast_get(parser->arena, left);
			left = tast_arena_add(parser->arena, (tast_node){
				.kind = tast_member,
				.span = { receiver->span.start, name->span.end },
				.member = { left, token->kind, name->span }
			});
			continue;
		}
		return left;
	}
}

static tast_id parse_bp(tparser *parser, uint8_t minimum)
{
	if (++parser->recursion_depth > parser->recursion_limit) {
		parser->recursion_depth--;
		return error_node(parser, peek(parser)->span,
				  "expression nesting limit exceeded");
	}
	tast_id left = parse_postfix(parser, parse_primary(parser));
	int saw_non_associative = 0;
	for (;;) {
		const tsyntax_token *operator_token = peek(parser);
		binding_power power = infix_power(operator_token->kind);
		if (power.left == 0 || power.left < minimum)
			break;
		if (power.non_associative && saw_non_associative) {
			tdiagnostics_add(parser->diagnostics, tdiagnostic_error,
					 operator_token->span,
					 "non-associative operators require parentheses");
		}
		saw_non_associative = saw_non_associative || power.non_associative;
		advance(parser);
		tast_id right = parse_bp(parser, power.right);
		const tast_node *left_node = tast_get(parser->arena, left);
		const tast_node *right_node = tast_get(parser->arena, right);
		left = tast_arena_add(parser->arena, (tast_node){
			.kind = tast_binary,
			.span = { left_node->span.start, right_node->span.end },
			.binary = { operator_token->kind, left, right }
		});
		left = parse_postfix(parser, left);
	}
	parser->recursion_depth--;
	return left;
}

void tparser_init(tparser *parser,
		  const tsource_document *document,
		  const tsyntax_tokens *tokens,
		  tast_arena *arena,
		  tdiagnostics *diagnostics)
{
	*parser = (tparser){
		.document = document,
		.tokens = tokens,
		.arena = arena,
		.diagnostics = diagnostics,
		.recursion_limit = 512
	};
}

tast_id tparser_parse_expression(tparser *parser)
{
	tast_id expression = parse_bp(parser, 0);
	const tsyntax_token *remaining = peek(parser);
	if (remaining->kind != tsyntax_eof)
		tdiagnostics_add(parser->diagnostics, tdiagnostic_error,
				 remaining->span, "unexpected token after expression");
	return expression;
}

static tast_id parse_declaration_statement(tparser *parser,
					   const tsyntax_token *keyword)
{
	const tsyntax_token *name = peek(parser);
	if (name->kind != tsyntax_identifier)
		return error_node(parser, name->span, "declaration name expected");
	advance(parser);

	int has_annotation = consume(parser, tsyntax_colon, nullptr);
	tsource_span annotation = { name->span.end, name->span.end };
	if (has_annotation) {
		uint32_t annotation_start = next_significant(parser, parser->cursor);
		uint32_t at = annotation_start;
		uint32_t depth = 0;
		while (at < parser->tokens->count) {
			tsyntax_kind kind = parser->tokens->items[at].kind;
			if (kind == tsyntax_lparen || kind == tsyntax_lbracket ||
			    kind == tsyntax_lbrace)
				depth++;
			else if (kind == tsyntax_rparen || kind == tsyntax_rbracket ||
				 kind == tsyntax_rbrace) {
				if (depth)
					depth--;
			} else if ((kind == tsyntax_assign || kind == tsyntax_comma) &&
				   depth == 0)
				break;
			if (kind == tsyntax_eof)
				break;
			at++;
		}
		uint32_t annotation_end = at;
		while (annotation_end > annotation_start &&
		       tsyntax_kind_is_trivia(
			       parser->tokens->items[annotation_end - 1].kind))
			annotation_end--;
		if (annotation_end == annotation_start) {
			tdiagnostics_add(parser->diagnostics, tdiagnostic_error,
					 peek(parser)->span,
					 "type annotation expected");
		} else {
			annotation = (tsource_span){
				parser->tokens->items[annotation_start].span.start,
				parser->tokens->items[annotation_end - 1].span.end
			};
		}
		parser->cursor = at;
	}

	int has_initializer = consume(parser, tsyntax_assign, nullptr);
	tast_id initializer = has_initializer ? parse_bp(parser, 0) : TAST_INVALID_ID;
	const tast_node *value = tast_get(parser->arena, initializer);
	return tast_arena_add(parser->arena, (tast_node){
		.kind = tast_declaration_statement,
		.span = { keyword->span.start, value ? value->span.end :
			(has_annotation ? annotation.end : name->span.end) },
		.declaration_statement = {
			.name = name->span,
			.annotation = annotation,
			.initializer = initializer,
			.is_mutable = keyword->kind == tsyntax_kw_var,
			.has_annotation = has_annotation,
			.has_initializer = has_initializer
		}
	});
}

static tast_id parse_statement_range(tparser *parser,
				     uint32_t start, uint32_t end)
{
	while (start < end &&
	       tsyntax_kind_is_trivia(parser->tokens->items[start].kind))
		start++;
	while (end > start &&
	       tsyntax_kind_is_trivia(parser->tokens->items[end - 1].kind))
		end--;
	if (start == end)
		return TAST_INVALID_ID;

	uint32_t count = end - start;
	tsyntax_token *items = (tsyntax_token *)malloc(
		(count + 1) * sizeof(tsyntax_token));
	if (!items)
		abort();
	for (uint32_t i = 0; i < count; i++)
		items[i] = parser->tokens->items[start + i];
	uint32_t eof_offset = items[count - 1].span.end;
	items[count] = (tsyntax_token){
		.kind = tsyntax_eof,
		.span = { eof_offset, eof_offset }
	};
	tsyntax_tokens tokens = {
		.items = items,
		.count = count + 1,
		.capacity = count + 1
	};
	tparser nested;
	tparser_init(&nested, parser->document, &tokens,
		     parser->arena, parser->diagnostics);
	nested.recursion_depth = parser->recursion_depth;
	nested.recursion_limit = parser->recursion_limit;
	tast_id statement = tparser_parse_statement(&nested);
	free(items);
	return statement;
}

static tast_id parse_rule_item_range(tparser *parser,
				      uint32_t start, uint32_t end)
{
	while (start < end &&
	       tsyntax_kind_is_trivia(parser->tokens->items[start].kind))
		start++;
	while (end > start &&
	       tsyntax_kind_is_trivia(parser->tokens->items[end - 1].kind))
		end--;
	if (start == end)
		return TAST_INVALID_ID;

	uint32_t count = end - start;
	tsyntax_token *items = (tsyntax_token *)malloc(
		(count + 1) * sizeof(*items));
	if (!items)
		abort();
	for (uint32_t i = 0; i < count; i++)
		items[i] = parser->tokens->items[start + i];
	items[count] = (tsyntax_token){
		.kind = tsyntax_eof,
		.span = { items[count - 1].span.end, items[count - 1].span.end }
	};
	tsyntax_tokens tokens = { items, count + 1, count + 1 };
	tparser nested;
	tparser_init(&nested, parser->document, &tokens,
		     parser->arena, parser->diagnostics);
	nested.recursion_depth = parser->recursion_depth;
	nested.recursion_limit = parser->recursion_limit;

	const tsyntax_token *first = peek(&nested);
	tast_id item;
	if (first->kind == tsyntax_kw_let) {
		item = tparser_parse_statement(&nested);
		const tast_node *declaration = tast_get(parser->arena, item);
		if (!declaration || declaration->kind != tast_declaration_statement ||
		    !declaration->declaration_statement.has_initializer)
			tdiagnostics_add(parser->diagnostics, tdiagnostic_error,
				 first->span, "rule let requires an initializer");
	} else if (first->kind == tsyntax_kw_require) {
		advance(&nested);
		tast_id value = parse_bp(&nested, 0);
		const tast_node *expression = tast_get(parser->arena, value);
		item = tast_arena_add(parser->arena, (tast_node){
			.kind = tast_rule_requirement,
			.span = { first->span.start,
				expression ? expression->span.end : first->span.end },
			.expression_statement = { value }
		});
	} else {
		tsource_span description = { first->span.start, first->span.start };
		int described = first->kind == tsyntax_string &&
			raw_token(&nested, next_significant(&nested, nested.cursor + 1))->kind ==
				tsyntax_colon;
		if (described) {
			description = advance(&nested)->span;
			advance(&nested);
		}
		tast_id value = described && peek(&nested)->kind == tsyntax_lbrace ?
			parse_block(&nested) : parse_bp(&nested, 0);
		const tast_node *expression = tast_get(parser->arena, value);
		item = tast_arena_add(parser->arena, (tast_node){
			.kind = tast_rule_condition,
			.span = { first->span.start,
				expression ? expression->span.end : first->span.end },
			.rule_condition = { value, description, described }
		});
	}
	if (peek(&nested)->kind != tsyntax_eof)
		tdiagnostics_add(parser->diagnostics, tdiagnostic_error,
			peek(&nested)->span, "unexpected token in rule item");
	free(items);
	return item;
}

static void append_node(tast_id **nodes, uint32_t *count,
			uint32_t *capacity, tast_id node)
{
	if (node == TAST_INVALID_ID)
		return;
	if (*count >= *capacity) {
		*capacity = *capacity ? *capacity * 2 : 8;
		*nodes = (tast_id *)realloc(
			*nodes, *capacity * sizeof(tast_id));
		if (!*nodes)
			abort();
	}
	(*nodes)[(*count)++] = node;
}

typedef enum {
	block_not_expected,
	block_expected,
	block_opened
} block_state;

typedef struct {
	uint32_t end;
	uint32_t next;
	const tsyntax_token *closing;
} statement_boundary;

typedef tast_id (*range_parser)(tparser *, uint32_t, uint32_t);

typedef struct {
	tast_id *items;
	uint32_t count;
	uint32_t capacity;
	const tsyntax_token *closing;
} statement_list;

static int begins_block_statement(tsyntax_kind kind)
{
	return kind == tsyntax_kw_function || kind == tsyntax_kw_if ||
		kind == tsyntax_kw_elif || kind == tsyntax_kw_else ||
		kind == tsyntax_kw_while || kind == tsyntax_kw_for ||
		kind == tsyntax_kw_rule;
}

static int begins_function_parameters(const tparser *parser,
				       uint32_t start, uint32_t lparen)
{
	uint32_t at = lparen;
	while (at > start) {
		tsyntax_kind kind = parser->tokens->items[--at].kind;
		if (tsyntax_kind_is_trivia(kind)) continue;
		return infix_power(kind).left || kind == tsyntax_assign ||
			kind == tsyntax_comma || kind == tsyntax_kw_return ||
			kind == tsyntax_lparen || kind == tsyntax_lbracket ||
			kind == tsyntax_lbrace;
	}
	return 1;
}

static statement_boundary find_statement_end(
	const tparser *parser, uint32_t start, tsyntax_kind closing)
{
	uint32_t first = next_significant(parser, start);
	tsyntax_kind first_kind = raw_token(parser, first)->kind;
	block_state block = begins_block_statement(first_kind) ?
		block_expected : block_not_expected;
	int conditional = first_kind == tsyntax_kw_if;
	uint32_t parentheses = 0, brackets = 0, braces = 0;
	int function_parameters = 0;
	for (uint32_t at = start; at < parser->tokens->count; at++) {
		const tsyntax_token *token = &parser->tokens->items[at];
		tsyntax_kind kind = token->kind;
		int top = parentheses == 0 && brackets == 0 && braces == 0;
		if (top && (kind == closing || kind == tsyntax_eof))
			return (statement_boundary){
				at, kind == tsyntax_eof ? at : at + 1,
				kind == closing ? token : nullptr
			};
		if (top && kind == tsyntax_semicolon)
			return (statement_boundary){ at, at + 1, nullptr };
		if (top && kind == tsyntax_newline) {
			uint32_t next = next_significant(parser, at + 1);
			tsyntax_kind next_kind = raw_token(parser, next)->kind;
			if ((block == block_expected && next_kind == tsyntax_lbrace) ||
			    (conditional && block == block_opened &&
			     (next_kind == tsyntax_kw_elif ||
			      next_kind == tsyntax_kw_else))) {
				at = next - 1;
				continue;
			}
			return (statement_boundary){ at, at + 1, nullptr };
		}
		if (top && conditional &&
		    (kind == tsyntax_kw_elif || kind == tsyntax_kw_else))
			block = block_expected;
		if (top && kind == tsyntax_kw_rule)
			block = block_expected;
		switch (kind) {
		case tsyntax_lparen:
			if (top)
				function_parameters = begins_function_parameters(
					parser, start, at);
			parentheses++;
			break;
		case tsyntax_rparen:
			if (parentheses == 1 && function_parameters) {
				tsyntax_kind next = raw_token(parser,
					next_significant(parser, at + 1))->kind;
				if (next == tsyntax_lbrace || next == tsyntax_arrow)
					block = block_expected;
				function_parameters = 0;
			}
			if (parentheses) parentheses--;
			break;
		case tsyntax_lbracket: brackets++; break;
		case tsyntax_rbracket: if (brackets) brackets--; break;
		case tsyntax_lbrace:
			if (top && block == block_expected) block = block_opened;
			braces++;
			break;
		case tsyntax_rbrace: if (braces) braces--; break;
		default: break;
		}
	}
	uint32_t end = parser->tokens->count;
	return (statement_boundary){ end, end, nullptr };
}

static statement_list parse_statement_list(tparser *parser,
					    tsyntax_kind closing,
					    range_parser parse_range)
{
	statement_list list = { 0 };
	uint32_t start = parser->cursor;
	for (;;) {
		statement_boundary boundary = find_statement_end(
			parser, start, closing);
		append_node(&list.items, &list.count, &list.capacity,
			parse_range(parser, start, boundary.end));
		parser->cursor = boundary.next;
		if (boundary.closing || boundary.next == boundary.end) {
			list.closing = boundary.closing;
			return list;
		}
		start = boundary.next;
	}
}

static tast_id parse_block(tparser *parser)
{
	const tsyntax_token *opening = nullptr;
	if (!consume(parser, tsyntax_lbrace, &opening))
		return error_node(parser, peek(parser)->span, "block expected");

	statement_list statements = parse_statement_list(
		parser, tsyntax_rbrace, parse_statement_range);
	if (!statements.closing)
		tdiagnostics_add(parser->diagnostics, tdiagnostic_error,
				 peek(parser)->span, "missing '}'");
	uint32_t children = tast_arena_add_children(
		parser->arena, statements.items, statements.count);
	free(statements.items);
	return tast_arena_add(parser->arena, (tast_node){
		.kind = tast_block,
		.span = { opening->span.start,
			  statements.closing ? statements.closing->span.end :
			  peek(parser)->span.start },
		.aggregate = { TAST_INVALID_ID, children, statements.count }
	});
}

static tast_id parse_rule_block(tparser *parser)
{
	const tsyntax_token *opening = nullptr;
	if (!consume(parser, tsyntax_lbrace, &opening))
		return error_node(parser, peek(parser)->span, "rule body expected");
	statement_list items = parse_statement_list(
		parser, tsyntax_rbrace, parse_rule_item_range);
	if (!items.closing)
		tdiagnostics_add(parser->diagnostics, tdiagnostic_error,
			peek(parser)->span, "missing '}' after rule body");
	uint32_t children = tast_arena_add_children(
		parser->arena, items.items, items.count);
	free(items.items);
	return tast_arena_add(parser->arena, (tast_node){
		.kind = tast_block,
		.span = { opening->span.start,
			items.closing ? items.closing->span.end :
			peek(parser)->span.start },
		.aggregate = { TAST_INVALID_ID, children, items.count }
	});
}

static tast_id parse_rule(tparser *parser, tsource_span start)
{
	tast_id *parameters = nullptr;
	uint32_t count = 0, capacity = 0;
	if (consume(parser, tsyntax_lparen, nullptr)) {
		while (peek(parser)->kind != tsyntax_rparen &&
		       peek(parser)->kind != tsyntax_eof) {
			const tsyntax_token *name = peek(parser);
			if (name->kind != tsyntax_identifier) {
				error_node(parser, name->span, "rule parameter name expected");
				break;
			}
			advance(parser);
			if (!consume(parser, tsyntax_colon, nullptr)) {
				error_node(parser, name->span,
					"rule parameter Type annotation required");
				break;
			}
			uint32_t annotation_start = next_significant(parser, parser->cursor);
			uint32_t at = annotation_start, depth = 0;
			while (at < parser->tokens->count) {
				tsyntax_kind kind = parser->tokens->items[at].kind;
				if (kind == tsyntax_lbracket) depth++;
				else if (kind == tsyntax_rbracket && depth) depth--;
				else if (!depth && (kind == tsyntax_comma ||
					 kind == tsyntax_rparen || kind == tsyntax_eof)) break;
				at++;
			}
			uint32_t annotation_end = at;
			while (annotation_end > annotation_start &&
			       tsyntax_kind_is_trivia(parser->tokens->items[annotation_end - 1].kind))
				annotation_end--;
			tsource_span annotation = annotation_end > annotation_start ?
				(tsource_span){ parser->tokens->items[annotation_start].span.start,
					parser->tokens->items[annotation_end - 1].span.end } :
				(tsource_span){ name->span.end, name->span.end };
			parser->cursor = at;
			if (count == capacity) {
				capacity = capacity ? capacity * 2 : 4;
				parameters = (tast_id *)realloc(parameters,
					capacity * sizeof(*parameters));
				if (!parameters) abort();
			}
			parameters[count++] = tast_arena_add(parser->arena, (tast_node){
				.kind = tast_parameter,
				.span = name->span,
				.parameter = { name->span, annotation, 1 }
			});
			if (!consume(parser, tsyntax_comma, nullptr)) break;
		}
		if (!consume(parser, tsyntax_rparen, nullptr))
			tdiagnostics_add(parser->diagnostics, tdiagnostic_error,
				peek(parser)->span, "missing ')' after rule parameters");
	}
	uint32_t children = tast_arena_add_children(parser->arena, parameters, count);
	free(parameters);
	tast_id body = parse_rule_block(parser);
	const tast_node *block = tast_get(parser->arena, body);
	return tast_arena_add(parser->arena, (tast_node){
		.kind = tast_rule,
		.span = { start.start, block ? block->span.end : start.end },
		.function = { children, count, body, { start.end, start.end }, 0, 0 }
	});
}

tast_id tparser_parse_module(tparser *parser)
{
	statement_list statements = parse_statement_list(
		parser, tsyntax_eof, parse_statement_range);
	uint32_t children = tast_arena_add_children(
		parser->arena, statements.items, statements.count);
	free(statements.items);
	return tast_arena_add(parser->arena, (tast_node){
		.kind = tast_module,
		.span = { 0, tsource_document_length(parser->document) },
		.aggregate = { TAST_INVALID_ID, children, statements.count }
	});
}

static tast_id parse_condition(tparser *parser)
{
	if (!consume(parser, tsyntax_lparen, nullptr))
		return error_node(parser, peek(parser)->span, "'(' expected");
	tast_id condition = parse_bp(parser, 0);
	if (!consume(parser, tsyntax_rparen, nullptr))
		tdiagnostics_add(parser->diagnostics, tdiagnostic_error,
				 peek(parser)->span, "missing ')'");
	return condition;
}

static tast_id parse_conditional_branch(tparser *parser,
					const tsyntax_token *keyword)
{
	int has_condition = keyword->kind != tsyntax_kw_else;
	tast_id condition = has_condition ? parse_condition(parser) :
		TAST_INVALID_ID;
	tast_id body = parse_block(parser);
	const tast_node *block = tast_get(parser->arena, body);
	return tast_arena_add(parser->arena, (tast_node){
		.kind = tast_conditional_branch,
		.span = { keyword->span.start, block->span.end },
		.conditional_branch = { condition, body, has_condition }
	});
}

static tast_id parse_if_statement(tparser *parser,
				  const tsyntax_token *keyword)
{
	tast_id *branches = nullptr;
	uint32_t count = 0, capacity = 0;
	uint32_t start = keyword->span.start;
	uint32_t end = keyword->span.end;
	int has_else = 0;
	for (;;) {
		int valid = !has_else;
		if (!valid)
			tdiagnostics_add(parser->diagnostics, tdiagnostic_error,
				keyword->span, "branch cannot follow else");
		tast_id branch = parse_conditional_branch(parser, keyword);
		const tast_node *node = tast_get(parser->arena, branch);
		if (node) end = node->span.end;
		if (valid) append_node(&branches, &count, &capacity, branch);
		if (keyword->kind == tsyntax_kw_else) has_else = 1;
		const tsyntax_token *next = peek(parser);
		if (next->kind != tsyntax_kw_elif &&
		    next->kind != tsyntax_kw_else)
			break;
		keyword = advance(parser);
	}
	uint32_t children = tast_arena_add_children(parser->arena, branches, count);
	free(branches);
	return tast_arena_add(parser->arena, (tast_node){
		.kind = tast_if_statement,
		.span = { start, end },
		.conditional_statement = { children, count }
	});
}

static tast_id parse_while_statement(tparser *parser,
				     const tsyntax_token *keyword)
{
	tast_id condition = parse_condition(parser);
	tast_id body = parse_block(parser);
	const tast_node *block = tast_get(parser->arena, body);
	return tast_arena_add(parser->arena, (tast_node){
		.kind = tast_while_statement,
		.span = { keyword->span.start, block->span.end },
		.control_statement = { condition, body }
	});
}

static tast_id parse_for_statement(tparser *parser,
				   const tsyntax_token *keyword)
{
	if (!consume(parser, tsyntax_lparen, nullptr))
		return error_node(parser, peek(parser)->span, "'(' expected");
	int declares = consume(parser, tsyntax_kw_let, nullptr);
	const tsyntax_token *name = peek(parser);
	if (name->kind != tsyntax_identifier)
		return error_node(parser, name->span, "loop binding expected");
	advance(parser);
	if (!consume(parser, tsyntax_kw_in, nullptr))
		return error_node(parser, peek(parser)->span, "'in' expected");
	tast_id iterable = parse_bp(parser, 0);
	if (!consume(parser, tsyntax_rparen, nullptr))
		tdiagnostics_add(parser->diagnostics, tdiagnostic_error,
				 peek(parser)->span, "missing ')'");
	tast_id body = parse_block(parser);
	const tast_node *block = tast_get(parser->arena, body);
	return tast_arena_add(parser->arena, (tast_node){
		.kind = tast_for_statement,
		.span = { keyword->span.start, block->span.end },
		.for_statement = { name->span, iterable, body, declares }
	});
}

static tast_id parse_import_statement(tparser *parser,
				      const tsyntax_token *keyword)
{
	uint32_t path_start = next_significant(parser, parser->cursor);
	uint32_t at = path_start;
	uint32_t alias_keyword = UINT32_MAX;
	while (at < parser->tokens->count) {
		tsyntax_kind kind = parser->tokens->items[at].kind;
		if (kind == tsyntax_kw_as) {
			alias_keyword = at;
			break;
		}
		if (kind == tsyntax_eof)
			break;
		at++;
	}
	uint32_t path_end = alias_keyword == UINT32_MAX ? at : alias_keyword;
	while (path_end > path_start &&
	       tsyntax_kind_is_trivia(parser->tokens->items[path_end - 1].kind))
		path_end--;
	if (path_start == path_end)
		return error_node(parser, keyword->span, "import path expected");
	tsource_span path = {
		parser->tokens->items[path_start].span.start,
		parser->tokens->items[path_end - 1].span.end
	};
	if (path_end == path_start + 1 &&
	    parser->tokens->items[path_start].kind == tsyntax_string &&
	    path.end >= path.start + 2) {
		path.start++;
		path.end--;
	}

	int has_alias = alias_keyword != UINT32_MAX;
	tsource_span alias = { path.end, path.end };
	if (has_alias) {
		parser->cursor = alias_keyword + 1;
		const tsyntax_token *name = peek(parser);
		if (name->kind != tsyntax_identifier)
			return error_node(parser, name->span, "import alias expected");
		advance(parser);
		alias = name->span;
	} else
		parser->cursor = at;
	return tast_arena_add(parser->arena, (tast_node){
		.kind = tast_import_statement,
		.span = { keyword->span.start,
			  has_alias ? alias.end : path.end },
		.import_statement = { path, alias, has_alias }
	});
}

tast_id tparser_parse_statement(tparser *parser)
{
	const tsyntax_token *first = peek(parser);
	tast_id statement;
	if (first->kind == tsyntax_kw_if) {
		advance(parser);
		statement = parse_if_statement(parser, first);
	} else if (first->kind == tsyntax_kw_while) {
		advance(parser);
		statement = parse_while_statement(parser, first);
	} else if (first->kind == tsyntax_kw_elif ||
		   first->kind == tsyntax_kw_else) {
		advance(parser);
		parse_conditional_branch(parser, first);
		statement = error_node(parser, first->span,
			first->kind == tsyntax_kw_elif ?
			"'elif' without preceding 'if'" :
			"'else' without preceding 'if'");
	} else if (first->kind == tsyntax_kw_for) {
		advance(parser);
		statement = parse_for_statement(parser, first);
	} else if (first->kind == tsyntax_kw_import) {
		advance(parser);
		statement = parse_import_statement(parser, first);
	} else if (first->kind == tsyntax_kw_break ||
		   first->kind == tsyntax_kw_continue) {
		advance(parser);
		statement = tast_arena_add(parser->arena, (tast_node){
			.kind = first->kind == tsyntax_kw_break ?
				tast_break_statement : tast_continue_statement,
			.span = first->span
		});
	} else if (first->kind == tsyntax_kw_function) {
		advance(parser);
		const tsyntax_token *name = peek(parser);
		if (name->kind != tsyntax_identifier)
			statement = error_node(parser, name->span,
				"function name expected");
		else {
			advance(parser);
			if (!consume(parser, tsyntax_lparen, nullptr))
				statement = error_node(parser, peek(parser)->span,
					"'(' expected after function name");
			else {
				tast_id function = parse_function(parser, first->span);
				const tast_node *function_node = tast_get(
					parser->arena, function);
				statement = tast_arena_add(parser->arena,
					(tast_node){
						.kind = tast_declaration_statement,
						.span = {
							first->span.start,
							function_node ? function_node->span.end :
								name->span.end
						},
						.declaration_statement = {
							.name = name->span,
							.annotation = {
								name->span.end,
								name->span.end
							},
							.initializer = function,
							.is_mutable = 0,
							.is_function_declaration = 1,
							.has_annotation = 0,
							.has_initializer = 1
						}
					});
			}
		}
	} else if (first->kind == tsyntax_kw_let || first->kind == tsyntax_kw_var) {
		advance(parser);
		statement = parse_declaration_statement(parser, first);
		if (peek(parser)->kind == tsyntax_comma) {
			tast_id *declarations = nullptr;
			uint32_t count = 0, capacity = 0;
			append_node(&declarations, &count, &capacity, statement);
			while (consume(parser, tsyntax_comma, nullptr))
				append_node(
					&declarations, &count, &capacity,
					parse_declaration_statement(parser, first));
			uint32_t children = tast_arena_add_children(
				parser->arena, declarations, count);
			const tast_node *last = tast_get(
				parser->arena, declarations[count - 1]);
			free(declarations);
			statement = tast_arena_add(parser->arena, (tast_node){
				.kind = tast_declaration_group,
				.span = { first->span.start, last->span.end },
				.aggregate = { TAST_INVALID_ID, children, count }
			});
		}
	} else if (first->kind == tsyntax_kw_return) {
		advance(parser);
		tast_id value = TAST_INVALID_ID;
		if (peek(parser)->kind != tsyntax_eof)
			value = parse_bp(parser, 0);
		const tast_node *value_node = tast_get(parser->arena, value);
		statement = tast_arena_add(parser->arena, (tast_node){
			.kind = tast_return_statement,
			.span = { first->span.start,
				  value_node ? value_node->span.end : first->span.end },
			.return_statement = { value }
		});
	} else {
		tast_id left = parse_bp(parser, 0);
		if (consume(parser, tsyntax_assign, nullptr)) {
			tast_id value = parse_bp(parser, 0);
			const tast_node *left_node = tast_get(parser->arena, left);
			const tast_node *value_node = tast_get(parser->arena, value);
			statement = tast_arena_add(parser->arena, (tast_node){
				.kind = tast_assignment_statement,
				.span = { left_node->span.start, value_node->span.end },
				.assignment_statement = { left, value }
			});
		} else {
			const tast_node *expression = tast_get(parser->arena, left);
			statement = tast_arena_add(parser->arena, (tast_node){
				.kind = tast_expression_statement,
				.span = expression->span,
				.expression_statement = { left }
			});
		}
	}

	const tsyntax_token *remaining = peek(parser);
	if (remaining->kind != tsyntax_eof)
		tdiagnostics_add(parser->diagnostics, tdiagnostic_error,
				 remaining->span, "unexpected token after statement");
	return statement;
}
