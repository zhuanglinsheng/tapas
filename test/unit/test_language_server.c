#include "tapas/lsp/server.h"
#include "../../src/lsp/presentation.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

static void frame(FILE *stream, const char *json)
{
	fprintf(stream, "Content-Length: %zu\r\n\r\n%s", strlen(json), json);
}

static void test_presentation_categories(void)
{
    tfrontend f;
    tfrontend_init(&f,"presentation.tap",
        "let count = 1\nvar total: Int = 2\n"
        "function identity(value: Int) -> Int { return value }\n"
        "let callback = (input: Int) -> Int { return input }\n"
        "let R = rule(quantity: Int) { quantity > 0 }\n",tfrontend_module);
    assert(tfrontend_valid(&f));
    const char *names[] = {"count","total","identity","value","callback","R"};
    const char *expected[] = {"let count: Int","var total: Int","Function identity[Int] -> Int",
        "parameter value: Int","let callback: Function[Int] -> Int","let R: Rule[Int]"};
    for(unsigned i=0;i<6;i++) {
        const tsemantic_symbol *found = nullptr;
        for(uint32_t j=0;j<f.semantic.symbol_count;j++)
            if(tstring_eq_cstr(f.semantic.symbols[j].name,names[i])) found=&f.semantic.symbols[j];
        assert(found);
        tstring *local = tlsp_present_source(&f,found);
        assert(tstring_eq_cstr(local,expected[i]));
        tmodule_export exported = {.local_symbol=(uint32_t)(found-f.semantic.symbols)};
        tstring *imported = tlsp_present_export(&f,&exported);
        assert(tstring_eq(local,imported));
        tstring_free(imported); tstring_free(local);
    }
    tstring *field=tlsp_present_field(&f.types.arena,"reason",tbuiltin_string);
    assert(tstring_eq_cstr(field,"reason: String")); tstring_free(field);
    tstandard_symbol standard=*tstandard_symbol_find("solve","hold");
    standard.detail="Misleading handwritten signature";
    tstring *description=tlsp_present_standard(&standard);
    assert(tstring_eq_cstr(description,"Function solve::hold[Union[Rule, RuleInstance]] -> solve::HoldResult"));
    tstring_free(description);
    description=tlsp_present_standard(tstandard_package("solve"));
    assert(tstring_eq_cstr(description,"package solve")); tstring_free(description);
    description=tlsp_present_standard(tstandard_symbol_find("solve","HoldResult"));
    assert(strstr(tstring_cstr(description),"Type solve::HoldResult {status: String")); tstring_free(description);
    tfrontend_free(&f);
}

int main(void)
{
    test_presentation_categories();
	FILE *input = tmpfile();
	FILE *output = tmpfile();
	assert(input && output);
	frame(input, "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{}}");
	frame(input, "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":{\"textDocument\":{\"uri\":\"file:///test.tap\",\"languageId\":\"tapas\",\"version\":1,\"text\":\"let 名称 = 1\\nlet value = 名称 +\\n\"}}}");
	frame(input, "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":{\"textDocument\":{\"uri\":\"file:///type-error.tap\",\"languageId\":\"tapas\",\"version\":1,\"text\":\"var values: Dictionary[String, Int] = {'a': 1}\\nvalues[2] = 3\\n\"}}}");
	frame(input, "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":{\"textDocument\":{\"uri\":\"file:///builtin.tap\",\"languageId\":\"tapas\",\"version\":1,\"text\":\"pprint([1])\\nlet primes = [2, 3, 5]\\nlet copied = primes.copy()\\n\"}}}");
	frame(input, "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"textDocument/definition\",\"params\":{\"textDocument\":{\"uri\":\"file:///test.tap\"},\"position\":{\"line\":1,\"character\":12}}}");
	frame(input, "{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"textDocument/hover\",\"params\":{\"textDocument\":{\"uri\":\"file:///test.tap\"},\"position\":{\"line\":1,\"character\":12}}}");
	frame(input, "{\"jsonrpc\":\"2.0\",\"id\":4,\"method\":\"textDocument/references\",\"params\":{\"textDocument\":{\"uri\":\"file:///test.tap\"},\"position\":{\"line\":1,\"character\":12},\"context\":{\"includeDeclaration\":true}}}");
	frame(input, "{\"jsonrpc\":\"2.0\",\"id\":5,\"method\":\"textDocument/documentSymbol\",\"params\":{\"textDocument\":{\"uri\":\"file:///test.tap\"}}}");
	frame(input, "{\"jsonrpc\":\"2.0\",\"id\":6,\"method\":\"textDocument/completion\",\"params\":{\"textDocument\":{\"uri\":\"file:///test.tap\"},\"position\":{\"line\":1,\"character\":12}}}");
	frame(input, "{\"jsonrpc\":\"2.0\",\"id\":7,\"method\":\"textDocument/prepareRename\",\"params\":{\"textDocument\":{\"uri\":\"file:///test.tap\"},\"position\":{\"line\":1,\"character\":12}}}");
	frame(input, "{\"jsonrpc\":\"2.0\",\"id\":8,\"method\":\"textDocument/rename\",\"params\":{\"textDocument\":{\"uri\":\"file:///test.tap\"},\"position\":{\"line\":1,\"character\":12},\"newName\":\"renamed\"}}");
	frame(input, "{\"jsonrpc\":\"2.0\",\"id\":9,\"method\":\"textDocument/hover\",\"params\":{\"textDocument\":{\"uri\":\"file:///builtin.tap\"},\"position\":{\"line\":0,\"character\":2}}}");
	frame(input, "{\"jsonrpc\":\"2.0\",\"id\":10,\"method\":\"textDocument/hover\",\"params\":{\"textDocument\":{\"uri\":\"file:///builtin.tap\"},\"position\":{\"line\":2,\"character\":22}}}");
	frame(input, "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":{\"textDocument\":{\"uri\":\"file:///solve-hover.tap\",\"languageId\":\"tapas\",\"version\":1,\"text\":\"let R = rule(x: Int) { x > 0 }\\nlet bounds = solve::hold(R)\\nlet reason = bounds::reason\\nlet conflicts = bounds::conflicts\\nlet condition = conflicts[0]::condition\\nlet annotated: solve::HoldResult = bounds\\nbounds::reason\\nlet solver = solve\\nlet aliased = solver::hold(R)\\n\"}}}");
	frame(input, "{\"jsonrpc\":\"2.0\",\"id\":101,\"method\":\"textDocument/hover\",\"params\":{\"textDocument\":{\"uri\":\"file:///solve-hover.tap\"},\"position\":{\"line\":1,\"character\":5}}}");
	frame(input, "{\"jsonrpc\":\"2.0\",\"id\":102,\"method\":\"textDocument/hover\",\"params\":{\"textDocument\":{\"uri\":\"file:///solve-hover.tap\"},\"position\":{\"line\":2,\"character\":23}}}");
	frame(input, "{\"jsonrpc\":\"2.0\",\"id\":103,\"method\":\"textDocument/hover\",\"params\":{\"textDocument\":{\"uri\":\"file:///solve-hover.tap\"},\"position\":{\"line\":3,\"character\":26}}}");
	frame(input, "{\"jsonrpc\":\"2.0\",\"id\":104,\"method\":\"textDocument/hover\",\"params\":{\"textDocument\":{\"uri\":\"file:///solve-hover.tap\"},\"position\":{\"line\":4,\"character\":6}}}");
	frame(input, "{\"jsonrpc\":\"2.0\",\"id\":105,\"method\":\"textDocument/hover\",\"params\":{\"textDocument\":{\"uri\":\"file:///solve-hover.tap\"},\"position\":{\"line\":5,\"character\":5}}}");
	frame(input, "{\"jsonrpc\":\"2.0\",\"id\":106,\"method\":\"textDocument/completion\",\"params\":{\"textDocument\":{\"uri\":\"file:///solve-hover.tap\"},\"position\":{\"line\":6,\"character\":8}}}");
	frame(input, "{\"jsonrpc\":\"2.0\",\"id\":107,\"method\":\"textDocument/hover\",\"params\":{\"textDocument\":{\"uri\":\"file:///solve-hover.tap\"},\"position\":{\"line\":1,\"character\":20}}}");
	frame(input, "{\"jsonrpc\":\"2.0\",\"id\":108,\"method\":\"textDocument/hover\",\"params\":{\"textDocument\":{\"uri\":\"file:///solve-hover.tap\"},\"position\":{\"line\":8,\"character\":23}}}");
	frame(input, "{\"jsonrpc\":\"2.0\",\"id\":11,\"method\":\"shutdown\",\"params\":null}");
	frame(input, "{\"jsonrpc\":\"2.0\",\"method\":\"exit\",\"params\":null}");
	rewind(input);
	assert(tlsp_run(input, output) == 0);
	fflush(output);
	fseek(output, 0, SEEK_END);
	long length = ftell(output);
	assert(length > 0);
	rewind(output);
	char *text = (char *)malloc((size_t)length + 1);
	assert(text);
	assert(fread(text, 1, (size_t)length, output) == (size_t)length);
	text[length] = '\0';
	assert(strstr(text, "Tapas Language Server"));
	assert(strstr(text, "publishDiagnostics"));
	assert(strstr(text, "unexpected end of input"));
	assert(strstr(text, "Dictionary key Type mismatch"));
	assert(strstr(text, "\"line\":0,\"character\":4"));
	assert(strstr(text, "let 名称"));
	assert(strstr(text, "let 名称: Int"));
	assert(strstr(text, "referencesProvider"));
	assert(strstr(text, "documentSymbolProvider"));
	assert(strstr(text, "workspaceSymbolProvider"));
	assert(strstr(text, "completionProvider"));
	assert(strstr(text, "triggerCharacters"));
	assert(strstr(text, "renameProvider"));
	assert(strstr(text, "Function pprint[...] -> Nil"));
	assert(strstr(text, "Function copy[T] -> T"));
	assert(strstr(text, "\\\"newText\\\":\\\"renamed\\\"") == nullptr);
	assert(strstr(text, "\"newText\":\"renamed\""));
    assert(strstr(text, "let bounds: solve::HoldResult"));
    assert(strstr(text, "\"value\":\"```tapas\\nFunction solve::hold[Union[Rule, RuleInstance]] -> solve::HoldResult\\n```\""));
    assert(!strstr(text, "hold: solve::hold"));
    assert(strstr(text, "\"id\":108,\"result\":{\"contents\":{\"kind\":\"markdown\",\"value\":\"```tapas\\nFunction solve::hold[Union[Rule, RuleInstance]] -> solve::HoldResult\\n```\""));
    assert(!strstr(text, "copy: copy("));
    assert(strstr(text, "let bounds: solve::HoldResult {status: String, scope: String, reason: String, witness: ?, integer_min: Int, integer_max: Int, conflicts: List[{rule: Rule, condition: String}], bindings: Dictionary}"));
    assert(!strstr(text, "solve::HoldResult = {"));
    assert(strstr(text, "reason: String"));
    assert(strstr(text, "conflicts: List[{rule: Rule, condition: String}]"));
    assert(strstr(text, "let condition: String"));
    assert(strstr(text, "let annotated: solve::HoldResult"));
    assert(strstr(text, "\"label\":\"conflicts\""));
    assert(strstr(text, "\"uri\":\"file:///solve-hover.tap\",\"version\":1,\"diagnostics\":[]"));
	free(text);
	fclose(input);
	fclose(output);
	return 0;
}
