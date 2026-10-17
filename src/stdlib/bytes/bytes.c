#include "tapas/textension.h"

#include "../arguments.h"
#include "../compact.h"
#include "buffer.h"
#include "../text.h"
#include "tapas/objects/ttype.h"

#include <string.h>
#include <limits.h>
#include <stdlib.h>

static const char *buffer_type(void)
{
	return "bytes::Buffer";
}

static tcompo_vtable buffer_vtable = {
	.get_type = buffer_type,
	.compo_code = compo_extension,
	.len = tcompact_len,
	.copy = tcompact_copy,
	.free = tcompact_free,
	.identical = tcompact_identical,
	.tostring_abbr = tcompact_string,
	.tostring_full = tcompact_string,
	.capabilities = &tcompact_capabilities,
	.bulk_operations = &tcompact_bulk_operations
};

void tstdlib_bytes_take(tobj *result, void *data, size_t length)
{
	if (length > LONG_MAX) { free(data); twarn(ErrRuntime_IntOutOfRange,"bytes storage","length exceeds Int"); }
	tcompact *buffer=tcompact_new(&buffer_vtable,tcompact_byte,0,nullptr,nullptr);
	buffer->data=data; buffer->length=length; tobj_set_compo(result,(tcompo_v *)buffer);
}
int tstdlib_bytes_data(const tobj *value, const void **data, size_t *length)
{
	if (value->type!=tcompo || value->val.v_tcompo->vtable!=&buffer_vtable) return 0;
	tcompact *buffer=(tcompact *)value->val.v_tcompo; *data=buffer->data; *length=buffer->length; return 1;
}

static void create_buffer_type(tobj *result)
{
	tobj_set_compo(result, (tcompo_v *)ttypeval_new_named("bytes::Buffer"));
}

static void bytes_buffer(tobj *params, uint_regs len, tobj *result)
{
	tstdlib_require_arguments("bytes::buffer", len, 2);
	if (params[0].type != tint || params[0].val.v_tint < 0)
		twarn(ErrRuntime_ParamsType, "bytes::buffer", "length must be a non-negative Int");
	if (params[1].type != tint || params[1].val.v_tint < 0 || params[1].val.v_tint > 255)
		twarn(ErrRuntime_ParamsType, "bytes::buffer", "value must be an Int in [0, 255]");
	tcompact *buffer = tcompact_new(&buffer_vtable, tcompact_byte,
		(size_t)params[0].val.v_tint, nullptr, nullptr);
	if (buffer->length)
		memset(buffer->data, (unsigned char)params[1].val.v_tint, buffer->length);
	tobj_set_compo(result, (tcompo_v *)buffer);
}

static void bytes_from_string(tobj *params, uint_regs len, tobj *result)
{
	(void)len;
	tstr *string = tstdlib_string_arg(&params[0], "bytes::from_string");
	size_t length = tstring_len(string->data);
	tcompact *buffer = tcompact_new(&buffer_vtable, tcompact_byte, length, nullptr, nullptr);
	if (length) memcpy(buffer->data, tstring_cstr(string->data), length);
	tobj_set_compo(result, (tcompo_v *)buffer);
}

static void bytes_to_string(tobj *params, uint_regs len, tobj *result)
{
	(void)len;
	if (params[0].type != tcompo || params[0].val.v_tcompo->vtable != &buffer_vtable)
		twarn(ErrRuntime_ParamsType, "bytes::to_string", "bytes::Buffer required");
	tcompact *buffer = (tcompact *)params[0].val.v_tcompo;
	tstdlib_string_result(result, buffer->length ? buffer->data : "", buffer->length, "bytes::to_string");
}

static const textension_symbol symbols[] = {
	{
		.name = "Buffer",
		.type = "Type",
		.detail = "bytes::Buffer: Type",
		.kind = textension_type,
		.value_factory = create_buffer_type,
		.instance_capabilities = &tcompact_capabilities
	},
	{
		.name = "buffer",
		.type = "Function[Int, Int] -> Buffer",
		.detail = "bytes::buffer(length: Int, value: Int) -> Buffer",
		.kind = textension_function,
		.function = bytes_buffer,
		.minimum_arguments = 2,
		.maximum_arguments = 2
	},
	{
		.name = "from_string", .type = "Function[String] -> Buffer",
		.detail = "bytes::from_string(value: String) -> Buffer",
		.kind = textension_function, .function = bytes_from_string,
		.minimum_arguments = 1, .maximum_arguments = 1
	},
	{
		.name = "to_string", .type = "Function[Buffer] -> String",
		.detail = "bytes::to_string(value: Buffer) -> String",
		.kind = textension_function, .function = bytes_to_string,
		.minimum_arguments = 1, .maximum_arguments = 1
	}
};

const textension_module tstdlib_bytes_module = {
	.scope = textension_package,
	.name = "bytes",
	.detail = "Compact mutable byte buffers",
	.symbols = symbols,
	.symbol_count = sizeof(symbols) / sizeof(symbols[0])
};
