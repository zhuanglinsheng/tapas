/**
 * @file tformat.c
 * @brief Implements generic support for formatting Tapas values.
 * @details Maintains a shared traversal for nested vtable calls so concrete
 * objects can format themselves while retaining cycle and size protection.
 * @note Concrete object kinds and layouts must never be added to this file.
 */
#include "tapas/tformat.h"
#include "tapas/dsa/tstring.h"
#include "tapas/objects/tstr.h"

#include <stdio.h>
#include <string.h>

typedef struct {
	const void *active[24];
	unsigned depth;
	unsigned nodes;
} tformat_traversal;

struct tformat_context {
	tstring *out;
	tformat_traversal *traversal;
	size_t limit;
	unsigned nested_depth;
	int stopped;
};

static _Thread_local tformat_traversal *active_traversal;

void tformat_text(tformat_context *context, const char *text)
{
	if (context->stopped || !text) return;
	size_t current = tstring_len(context->out);
	size_t length = strlen(text);
	if (current + length <= context->limit) {
		tstring_append(context->out, text);
		return;
	}
	size_t remaining = context->limit > current ?
		context->limit - current : 0;
	while (remaining && ((unsigned char)text[remaining] & 0xc0) == 0x80)
		remaining--;
	tstring_append_len(context->out, text, remaining);
	tstring_append(context->out, "...<truncated>");
	context->stopped = 1;
}

void tformat_number(tformat_context *context, long number)
{
	char buffer[32];
	snprintf(buffer, sizeof(buffer), "%ld", number);
	tformat_text(context, buffer);
}

void tformat_quoted(tformat_context *context, const char *text)
{
	tformat_text(context, "\"");
	for (; text && *text && !context->stopped; text++) {
		switch (*text) {
		case '\\': tformat_text(context, "\\\\"); break;
		case '"': tformat_text(context, "\\\""); break;
		case '\n': tformat_text(context, "\\n"); break;
		case '\r': tformat_text(context, "\\r"); break;
		case '\t': tformat_text(context, "\\t"); break;
		default: {
			char buffer[8];
			if ((unsigned char)*text < 32) {
				snprintf(buffer, sizeof(buffer), "\\x%02x",
					 (unsigned char)*text);
			} else {
				size_t width = 1;
				if ((unsigned char)*text >= 0xc0) {
					size_t expected = (unsigned char)*text >= 0xf0 ? 4 :
						(unsigned char)*text >= 0xe0 ? 3 : 2;
					while (width < expected && text[width] &&
					       ((unsigned char)text[width] & 0xc0) == 0x80)
						width++;
				}
				memcpy(buffer, text, width);
				buffer[width] = 0;
				text += width - 1;
			}
			tformat_text(context, buffer);
		}
		}
	}
	tformat_text(context, "\"");
}

void tformat_value(tformat_context *context, const tobj *value)
{
	if (value->type == tcompo && value->val.v_tcompo &&
	    tobj_compo_type(value) == compo_tstr) {
		tformat_quoted(context,
			tstring_cstr(((tstr *)value->val.v_tcompo)->data));
		return;
	}
	tstring *rendered = tobj_tostring_full(value);
	tformat_text(context, tstring_cstr(rendered));
	tstring_free(rendered);
}

void tformat_sequence(tformat_context *context, const tobj_vec *values)
{
	for (uint_objs i = 0; i < values->len && !context->stopped; i++) {
		if (i) tformat_text(context, ", ");
		tformat_value(context, &values->data[i]);
	}
}

void tformat_named(tformat_context *context, const char *kind,
		   const char *name)
{
	tformat_text(context, kind);
	if (name && *name) {
		tformat_text(context, " ");
		tformat_text(context, name);
	}
}

int tformat_begin_nested(tformat_context *context)
{
	if (context->nested_depth >= 24 ||
	    ++context->traversal->nodes > 1024) {
		tformat_text(context, "...<limit>");
		return 0;
	}
	context->nested_depth++;
	return 1;
}

void tformat_end_nested(tformat_context *context)
{
	if (context->nested_depth) context->nested_depth--;
}

int tformat_stopped(const tformat_context *context)
{
	return context->stopped;
}

int tformat_is_root(const tformat_context *context)
{
	return context->traversal->depth == 1;
}

tstring *tformat_object(const void *self, size_t limit,
			tformat_renderer renderer)
{
	tformat_traversal traversal = {0};
	int root = active_traversal == nullptr;
	if (root) active_traversal = &traversal;
	tformat_traversal *active = active_traversal;
	for (unsigned i = 0; i < active->depth; i++)
		if (active->active[i] == self) {
			if (root) active_traversal = nullptr;
			return tstring_new("<cycle>");
		}
	if (active->depth == 24 || ++active->nodes > 1024) {
		if (root) active_traversal = nullptr;
		return tstring_new("...<limit>");
	}
	active->active[active->depth++] = self;
	tformat_context context = {
		.out = tstring_new_empty(),
		.traversal = active,
		.limit = limit
	};
	renderer(&context, self);
	active->depth--;
	if (root) active_traversal = nullptr;
	return context.out;
}

tstring *tformat_pointer(const void *self)
{
	tstring *out = tstring_new_empty();
	tstring_append_fmt(out, "<%p>", self);
	return out;
}
