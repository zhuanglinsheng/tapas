/**
 * @file tfunction_metadata.c
 * @brief Implements immutable core function metadata.
 * @details Owns parameter names and types, signatures, reference counting,
 * decoding, construction, and signature formatting.
 * @note Metadata records describe callable interfaces; they do not dispatch
 * or implement package functions.
 */
#include "tfunction_metadata.h"
#include "tapas/dsa/tstring.h"
#include "tapas/objects/ttype.h"
#include "tapas/tformat.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
	tstring *name;
	ttypeval *type;
	uint8_t optional;
} parameter_info;

struct tfunction_metadata {
	uint32_t references;
	tstring *display_name;
	uint32_t count;
	parameter_info *parameters;
	ttypeval *signature;
	uint8_t variadic;
};

tfunction_metadata *tfunction_metadata_retain(tfunction_metadata *metadata)
{
	if (metadata) metadata->references++;
	return metadata;
}

void tfunction_metadata_release(tfunction_metadata *metadata)
{
	if (!metadata || --metadata->references) return;
	for (uint32_t i = 0; i < metadata->count; i++) {
		tstring_free(metadata->parameters[i].name);
		ttypeval_release(metadata->parameters[i].type);
	}
	tstring_free(metadata->display_name);
	free(metadata->parameters);
	ttypeval_release(metadata->signature);
	free(metadata);
}

static tfunction_metadata *allocate_metadata(uint32_t count, ttypeval *signature)
{
	ttypeval *type = ttypeval_retain(signature);
	if (!type || type->kind != ttype_kind_function) {
		if (type) ttypeval_release(type);
		return nullptr;
	}
	tfunction_metadata *metadata = calloc(1, sizeof(*metadata));
	if (!metadata) abort();
	metadata->references = 1;
	metadata->count = count;
	metadata->signature = type;
	metadata->variadic = ttypeval_function_variadic(type);
	metadata->parameters = calloc(count + 1, sizeof(*metadata->parameters));
	if (!metadata->parameters) abort();
	return metadata;
}

tfunction_metadata *tfunction_metadata_from_type(const char *names, ttypeval *signature)
{
	if (!names || !signature) return nullptr;
	/* Optional declaration name followed by RS; old parameter-only metadata remains valid. */
    const char *separator = strchr(names, '\x1e');
    const char *declaration_name = names;
    if (separator) names = separator + 1;
	uint32_t count = *names ? 1 : 0;
	for (const char *p = names; *p; p++) if (*p == '\x1f') count++;
	tfunction_metadata *metadata = allocate_metadata(count, signature);
	if (!metadata) return nullptr;
    if (separator) metadata->display_name = tstring_new_len(declaration_name, separator - declaration_name);
	if (count != ttypeval_function_parameter_count(metadata->signature)) {
		tfunction_metadata_release(metadata);
		return nullptr;
	}
	for (uint32_t i = 0; i < count; i++) {
		const char *end = strchr(names, '\x1f');
		if (!end) end = names + strlen(names);
		if (end == names) { tfunction_metadata_release(metadata); return nullptr; }
		metadata->parameters[i].name = tstring_new_len(names, end - names);
		ttypeval *type = ttypeval_function_parameter_at(metadata->signature, i);
		metadata->parameters[i].type = ttypeval_retain(type ? type : ttypeval_builtin(tbuiltintype_any));
		names = *end ? end + 1 : end;
	}
	return metadata;
}

tfunction_metadata *tfunction_metadata_decode(const char *names, const char *signature)
{
	if (!names || !signature) return nullptr;
	ttypeval *type = ttypeval_retain(ttypeval_from_canonical(signature));
	tfunction_metadata *result = tfunction_metadata_from_type(names, type);
	ttypeval_release(type);
	return result;
}

tfunction_metadata *tfunction_metadata_new(const tparameter_spec *parameters,
    uint32_t count, ttypeval *return_type, int variadic)
{
    if (!return_type || (count && !parameters)) return nullptr;
    tfunction_metadata *metadata = calloc(1, sizeof(*metadata));
    if (!metadata) abort();
    metadata->references = 1;
    metadata->count = count;
    metadata->variadic = variadic;
    metadata->parameters = calloc(count + 1, sizeof(*metadata->parameters));
    ttypeval **types = calloc(count + 1, sizeof(*types));
    if (!metadata->parameters || !types) abort();
    for (uint32_t i = 0; i < count; i++) {
        if (!parameters[i].name || !*parameters[i].name || !parameters[i].type) {
            free(types); tfunction_metadata_release(metadata); return nullptr;
        }
        metadata->parameters[i].name = tstring_new(parameters[i].name);
        metadata->parameters[i].type = parameters[i].type();
        if (!metadata->parameters[i].type) {
            free(types); tfunction_metadata_release(metadata); return nullptr;
        }
        types[i] = metadata->parameters[i].type;
        metadata->parameters[i].optional = parameters[i].optional;
        metadata->variadic |= parameters[i].variadic;
    }
    metadata->signature = ttypeval_new_function(types, count, return_type, metadata->variadic);
    free(types);
    return metadata;
}

uint32_t tfunction_metadata_count(const tfunction_metadata *m) { return m ? m->count : 0; }
const char *tfunction_metadata_name(const tfunction_metadata *m, uint32_t i)
{ return m && i < m->count ? tstring_cstr(m->parameters[i].name) : nullptr; }
ttypeval *tfunction_metadata_type(const tfunction_metadata *m, uint32_t i)
{ return m && i < m->count ? m->parameters[i].type : nullptr; }
ttypeval *tfunction_metadata_return_type(const tfunction_metadata *m)
{ return m ? ttypeval_function_result(m->signature) : nullptr; }
int tfunction_metadata_variadic(const tfunction_metadata *m) { return m && m->variadic; }
int tfunction_metadata_optional(const tfunction_metadata *m, uint32_t i)
{ return m && i < m->count && m->parameters[i].optional; }

const char *tfunction_metadata_display_name(const tfunction_metadata *m)
{ return m && m->display_name ? tstring_cstr(m->display_name) : nullptr; }

void tformat_function_signature(tformat_context *context, const char *name,
				const tfunction_metadata *metadata)
{
	tformat_named(context, "Function", name);
	tformat_text(context, "[");
	if (!metadata || tfunction_metadata_variadic(metadata)) {
		tformat_text(context, "...");
	} else {
		for (uint32_t i = 0; i < tfunction_metadata_count(metadata) &&
		     !tformat_stopped(context); i++) {
			if (i) tformat_text(context, ", ");
			tformat_type(context,
				tfunction_metadata_type(metadata, i));
		}
	}
	tformat_text(context, "] -> ");
	tformat_type(context, metadata ?
		tfunction_metadata_return_type(metadata) : nullptr);
}
