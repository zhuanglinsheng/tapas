#include "presentation.h"
#include "tapas/dsa/tstring.h"

#include <string.h>

typedef enum { value_symbol, function_symbol, type_symbol, package_symbol } presentation_kind;

static void append_type(tstring *out, const tstatic_type_arena *arena, tstatic_type_id id, int expand)
{
    tstring *type = expand ? tstatic_type_format(arena,id) : tstatic_type_display(arena,id);
    tstring_append_ts(out,type);
    tstring_free(type);
}

/* Categories own punctuation; adapters only supply semantic facts. */
static tstring *present(presentation_kind kind, const char *prefix, const char *name,
    const tstatic_type_arena *arena, tstatic_type_id id, tstatic_type_id definition, int result_argument)
{
    tstring *out = tstring_new_empty();
    const tstatic_type *type = tstatic_type_get(arena,id);
    if (kind == package_symbol) {
        tstring_append_fmt(out,"package %s",name);
    } else if (kind == function_symbol) {
        tstring_append_fmt(out,"Function %s[",name);
        if (!type || type->kind != tstatic_type_function) tstring_append(out,"?");
        else if (type->variadic) tstring_append(out,"...");
        else {
            const tstatic_type_id *children = tstatic_type_children(arena,type);
            for(uint32_t i=0;i+1<type->child_count;i++) {
                if(i) tstring_append(out,", ");
                if ((int)i == result_argument) tstring_append(out,"T");
                else append_type(out,arena,children[i],0);
            }
        }
        tstring_append(out,"] -> ");
        const tstatic_type_id *children = type ? tstatic_type_children(arena,type) : nullptr;
        if (type && type->kind == tstatic_type_function && !type->variadic &&
            result_argument >= 0 && (uint32_t)result_argument + 1 < type->child_count) tstring_append(out,"T");
        else append_type(out,arena,type && type->kind == tstatic_type_function && type->child_count ?
            children[type->child_count-1] : TSTATIC_TYPE_UNKNOWN,0);
    } else if (kind == type_symbol) {
        tstring_append_fmt(out,"Type %s",name);
        const tstatic_type *body = tstatic_type_get(arena,definition);
        if(body) {
            tstring_append(out,body->kind == tstatic_type_fields ? " " : " = ");
            append_type(out,arena,definition,1);
        }
    } else {
        if(prefix && *prefix) tstring_append_fmt(out,"%s ",prefix);
        tstring_append_fmt(out,"%s: ",name);
        append_type(out,arena,id,0);
        /* Expand only a top-level named record. Nested aliases remain concise. */
        if(type && type->display_name && type->kind == tstatic_type_fields) {
            tstring_append(out," ");
            append_type(out,arena,type->display_definition,1);
        }
    }
    return out;
}

tstring *tlsp_present_source(const tfrontend *frontend, const tsemantic_symbol *symbol)
{
    uint32_t index = (uint32_t)(symbol - frontend->semantic.symbols);
    tstatic_type_id id = index < frontend->types.symbol_count ? frontend->types.symbol_type_ids[index] : TSTATIC_TYPE_UNKNOWN;
    tstatic_type_id definition = index < frontend->types.symbol_count ? frontend->types.symbol_static_values[index] : TSTATIC_TYPE_UNKNOWN;
    presentation_kind kind = definition != TSTATIC_TYPE_UNKNOWN ? type_symbol :
        symbol->kind == tsemantic_symbol_function ? function_symbol :
        symbol->kind == tsemantic_symbol_import ? package_symbol : value_symbol;
    return present(kind,tsemantic_symbol_kind_name(symbol->kind),tstring_cstr(symbol->name),
        &frontend->types.arena,id,definition,-1);
}

tstring *tlsp_present_standard(const tstandard_symbol *symbol)
{
    tstatic_type_arena arena;
    tstatic_type_arena_init(&arena);
    tstring *name = tstring_new_empty();
    if(symbol->package) tstring_append_fmt(name,"%s::",symbol->package);
    tstring_append(name,symbol->name);
    presentation_kind kind = symbol->kind == tmodule_symbol_function ? function_symbol :
        symbol->kind == tmodule_symbol_type ? type_symbol :
        symbol->kind == tmodule_symbol_package ? package_symbol : value_symbol;
    tstatic_type_id id = tstandard_type_resolve(&arena,tstring_cstr(name),kind == type_symbol);
    if(kind == value_symbol && symbol->type) id = tstatic_type_parse(&arena,symbol->type,nullptr,nullptr);
	const tstatic_type *function = tstatic_type_get(&arena, id);
	size_t qualified_length = tstring_len(name);
	/* A keyword-only signature carries information that Function[...] cannot
	 * encode yet. Its qualified prefix makes it an explicit callable schema,
	 * rather than ordinary descriptive prose. */
	if (kind == function_symbol && function && function->variadic &&
	    symbol->detail &&
	    strncmp(symbol->detail, tstring_cstr(name), qualified_length) == 0 &&
	    symbol->detail[qualified_length] == '(' &&
	    strstr(symbol->detail, ", *, ")) {
		tstring *result = tstring_new("Function ");
		tstring_append(result, symbol->detail);
		tstring_free(name);
		tstatic_type_arena_free(&arena);
		return result;
	}
    tstring *result = present(kind,nullptr,tstring_cstr(name),&arena,id,
        kind == type_symbol ? id : TSTATIC_TYPE_UNKNOWN,
        symbol->result_from_argument ? (int)symbol->result_argument : -1);
    tstring_free(name);
    tstatic_type_arena_free(&arena);
    return result;
}

tstring *tlsp_present_field(const tstatic_type_arena *arena, const char *name, tstatic_type_id id)
{
    return present(value_symbol,nullptr,name,arena,id,TSTATIC_TYPE_UNKNOWN,-1);
}

tstring *tlsp_present_export(const tfrontend *frontend, const tmodule_export *exported)
{
    if(frontend && exported->local_symbol < frontend->semantic.symbol_count)
        return tlsp_present_source(frontend,&frontend->semantic.symbols[exported->local_symbol]);
    /* Exports without a source declaration carry a machine-generated Type. */
    tstatic_type_arena arena;
    tstatic_type_arena_init(&arena);
    tstatic_type_id id = exported->detail ? tstatic_type_parse(&arena,tstring_cstr(exported->detail),nullptr,nullptr) : TSTATIC_TYPE_UNKNOWN;
    presentation_kind kind = exported->kind == tmodule_symbol_function ? function_symbol :
        exported->kind == tmodule_symbol_type ? type_symbol :
        exported->kind == tmodule_symbol_package ? package_symbol : value_symbol;
    tstring *out = present(kind,nullptr,tstring_cstr(exported->name),&arena,id,TSTATIC_TYPE_UNKNOWN,-1);
    tstatic_type_arena_free(&arena);
    return out;
}
