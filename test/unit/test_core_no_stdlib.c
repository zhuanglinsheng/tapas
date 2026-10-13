#include "tapas/textension.h"
#include "tapas/tsession.h"

#include <assert.h>

static void intrinsic_stub(tobj *arguments, uint_regs argument_count,
			   tobj *result)
{
	(void)arguments;
	(void)argument_count;
	(void)result;
}

static const textension_symbol alpha_symbols[] = {
	{
		.name = "alpha_type", .type = "(Type) -> Type",
		.detail = "test intrinsic", .kind = textension_function,
		.function = intrinsic_stub, .minimum_arguments = 1,
		.maximum_arguments = 1, .intrinsic = tnative_intrinsic_type_list
	}
};

static const textension_module alpha_module = TAPAS_ROOT_MODULE(alpha_symbols);
static const textension_module *const alpha_modules[] = { &alpha_module };
static const textension_descriptor alpha_extension = TAPAS_EXTENSION(
	"alpha", "1", alpha_modules);

static const textension_symbol beta_symbols[] = {
	{
		.name = "beta_type", .type = "(Type) -> Type",
		.detail = "test intrinsic", .kind = textension_function,
		.function = intrinsic_stub, .minimum_arguments = 1,
		.maximum_arguments = 1, .intrinsic = tnative_intrinsic_type_list
	}
};

static const textension_module beta_module = TAPAS_ROOT_MODULE(beta_symbols);
static const textension_module *const beta_modules[] = { &beta_module };
static const textension_descriptor beta_extension = TAPAS_EXTENSION(
	"beta", "1", beta_modules);

static const textension_descriptor duplicate_name_extension = {
	.abi_version = TAPAS_EXTENSION_ABI,
	.structure_size = sizeof(textension_descriptor),
	.name = "alpha",
	.version = "2",
	.modules = nullptr,
	.module_count = 0
};

static void test_extension_registry_isolation(void)
{
	textension_registry *left = textension_registry_new();
	textension_registry *right = textension_registry_new();
	assert(left && right);

	assert(textension_registry_register(left, &alpha_extension));
	assert(textension_registry_count(left) == 1);
	assert(textension_registry_count(right) == 0);
	assert(textension_registry_revision(left) == 1);
	assert(textension_registry_register(left, &alpha_extension));
	assert(textension_registry_revision(left) == 1);

	assert(!textension_registry_accepts(left, &duplicate_name_extension));
	assert(!textension_registry_register(left, &duplicate_name_extension));
	assert(!textension_registry_accepts(left, &beta_extension));
	assert(!textension_registry_register(left, &beta_extension));
	assert(textension_registry_count(left) == 1);
	assert(textension_registry_count(right) == 0);

	textension_registry_free(right);
	textension_registry_free(left);
}

int main(void)
{
	test_extension_registry_isolation();
	tsession *session = tsession_new();
	assert(session);
	tsession_execute_str(session, "let answer = 42\n", 0);
	tsession_free(session);
	return 0;
}
