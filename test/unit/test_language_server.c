#include "tapas/lsp/server.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

static void frame(FILE *stream, const char *json)
{
	fprintf(stream, "Content-Length: %zu\r\n\r\n%s", strlen(json), json);
}

int main(void)
{
	FILE *input = tmpfile();
	FILE *output = tmpfile();
	assert(input && output);
	frame(input, "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{}}");
	frame(input, "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":{\"textDocument\":{\"uri\":\"file:///test.tap\",\"languageId\":\"tapas\",\"version\":1,\"text\":\"let 名称 = 1\\nlet value = 名称 +\\n\"}}}");
	frame(input, "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"textDocument/definition\",\"params\":{\"textDocument\":{\"uri\":\"file:///test.tap\"},\"position\":{\"line\":1,\"character\":12}}}");
	frame(input, "{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"textDocument/hover\",\"params\":{\"textDocument\":{\"uri\":\"file:///test.tap\"},\"position\":{\"line\":1,\"character\":12}}}");
	frame(input, "{\"jsonrpc\":\"2.0\",\"id\":4,\"method\":\"textDocument/references\",\"params\":{\"textDocument\":{\"uri\":\"file:///test.tap\"},\"position\":{\"line\":1,\"character\":12},\"context\":{\"includeDeclaration\":true}}}");
	frame(input, "{\"jsonrpc\":\"2.0\",\"id\":5,\"method\":\"textDocument/documentSymbol\",\"params\":{\"textDocument\":{\"uri\":\"file:///test.tap\"}}}");
	frame(input, "{\"jsonrpc\":\"2.0\",\"id\":6,\"method\":\"textDocument/completion\",\"params\":{\"textDocument\":{\"uri\":\"file:///test.tap\"},\"position\":{\"line\":1,\"character\":12}}}");
	frame(input, "{\"jsonrpc\":\"2.0\",\"id\":7,\"method\":\"textDocument/prepareRename\",\"params\":{\"textDocument\":{\"uri\":\"file:///test.tap\"},\"position\":{\"line\":1,\"character\":12}}}");
	frame(input, "{\"jsonrpc\":\"2.0\",\"id\":8,\"method\":\"textDocument/rename\",\"params\":{\"textDocument\":{\"uri\":\"file:///test.tap\"},\"position\":{\"line\":1,\"character\":12},\"newName\":\"renamed\"}}");
	frame(input, "{\"jsonrpc\":\"2.0\",\"id\":9,\"method\":\"shutdown\",\"params\":null}");
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
	assert(strstr(text, "\"line\":0,\"character\":4"));
	assert(strstr(text, "let 名称"));
	assert(strstr(text, "let 名称: Int"));
	assert(strstr(text, "referencesProvider"));
	assert(strstr(text, "documentSymbolProvider"));
	assert(strstr(text, "workspaceSymbolProvider"));
	assert(strstr(text, "completionProvider"));
	assert(strstr(text, "triggerCharacters"));
	assert(strstr(text, "renameProvider"));
	assert(strstr(text, "\\\"newText\\\":\\\"renamed\\\"") == NULL);
	assert(strstr(text, "\"newText\":\"renamed\""));
	free(text);
	fclose(input);
	fclose(output);
	return 0;
}
