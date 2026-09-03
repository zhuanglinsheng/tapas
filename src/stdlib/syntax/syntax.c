#include "tapas/textension.h"

#include "tapas/compile/source.h"
#include "tapas/compile/syntax.h"
#include "tapas/runtime/tdict.h"
#include "tapas/runtime/tlist.h"
#include "tapas/runtime/tstr.h"
#include "tapas/runtime/ttype.h"

static void dictionary_set(tdict *dictionary, const char *name, const tobj *value)
{
	tobj key;
	tobj_set_nil(&key);
	tobj_set_compo(&key, (tcompo_v *)tstr_new(name));
	tdict_set(dictionary, &key, value);
	tobj_try_clear(&key);
}

static void dictionary_set_int(tdict *dictionary, const char *name, long value)
{
	tobj object;
	tobj_set_int(&object, value);
	dictionary_set(dictionary, name, &object);
}

static void dictionary_set_string(tdict *dictionary, const char *name,
				  const char *value)
{
	tobj object;
	tobj_set_nil(&object);
	tobj_set_compo(&object, (tcompo_v *)tstr_new(value));
	dictionary_set(dictionary, name, &object);
	tobj_try_clear(&object);
}

static void syntax_tokens(tobj *arguments, uint_regs argument_count, tobj *result)
{
	(void)argument_count;
	if (arguments[0].type != tcompo ||
	    tobj_compo_type(&arguments[0]) != compo_tstr)
		twarn(ErrRuntime_ParamsType, "syntax::tokens",
		      "String source required");
	tstr *source = (tstr *)arguments[0].val.v_tcompo;
	tsource_document document;
	tsource_document_init(&document, "<syntax>",
			      tstring_cstr(source->data));
	tsyntax_tokens tokens;
	tsyntax_tokens_init(&tokens);
	tsyntax_lex(&document, &tokens);

	tlist *items = tlist_new();
	for (uint32_t i = 0; i < tokens.count; i++) {
		const tsyntax_token *token = &tokens.items[i];
		tdict *entry = tdict_new();
		dictionary_set_string(entry, "kind",
				      tsyntax_kind_name(token->kind));
		dictionary_set_int(entry, "start", token->span.start);
		dictionary_set_int(entry, "end", token->span.end);
		tobj object;
		tobj_set_nil(&object);
		tobj_set_compo(&object, (tcompo_v *)entry);
		tobj_vec_push(&items->items, &object);
		tobj_try_clear(&object);
	}

	tsyntax_tokens_free(&tokens);
	tsource_document_free(&document);
	tobj_set_compo(result, (tcompo_v *)items);
}

static void syntax_token_type(tobj *result)
{
	tstring *kind_name = tstring_new("kind");
	tstring *start_name = tstring_new("start");
	tstring *end_name = tstring_new("end");
	ttype_field fields[] = {
		{ kind_name, ttypeval_builtin(tbuiltin_string), 0 },
		{ start_name, ttypeval_builtin(tbuiltin_int), 0 },
		{ end_name, ttypeval_builtin(tbuiltin_int), 0 }
	};
	tobj_set_compo(result,
		      (tcompo_v *)ttypeval_new_fields(fields, 3));
	tstring_free(kind_name);
	tstring_free(start_name);
	tstring_free(end_name);
}

static void syntax_line_feed(tobj *result)
{
	tobj_set_compo(result, (tcompo_v *)tstr_new_len("\n", 1));
}

static void syntax_horizontal_tab(tobj *result)
{
	tobj_set_compo(result, (tcompo_v *)tstr_new_len("\t", 1));
}

static const textension_symbol symbols[] = {
	{
		.name = "Token",
		.type = "Type",
		.detail = "syntax::Token: Type",
		.kind = textension_type,
		.value_factory = syntax_token_type
	},
	{
		.name = "line_feed",
		.type = "String",
		.detail = "syntax::line_feed: String",
		.kind = textension_value,
		.value_factory = syntax_line_feed
	},
	{
		.name = "horizontal_tab",
		.type = "String",
		.detail = "syntax::horizontal_tab: String",
		.kind = textension_value,
		.value_factory = syntax_horizontal_tab
	},
	{
		.name = "tokens",
		.type = "Function[String] -> List",
		.detail = "syntax::tokens(source: String) -> List[syntax::Token]",
		.kind = textension_function,
		.function = syntax_tokens,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	}
};

const textension_module tstdlib_syntax_module = {
	.scope = textension_package,
	.name = "syntax",
	.detail = "Source tokenization",
	.symbols = symbols,
	.symbol_count = sizeof(symbols) / sizeof(symbols[0])
};
