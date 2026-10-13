#include "lsp/server.h"
#include "tapas/textension.h"
#include "tapas/tstdlib.h"

int main(void)
{
	textension_registry *extensions = textension_registry_new();
	if (!extensions || !textension_registry_register(
	    extensions, tstdlib_descriptor())) {
		textension_registry_free(extensions);
		return 1;
	}
	int result = tlsp_run(stdin, stdout, extensions);
	textension_registry_free(extensions);
	return result;
}
