#include "tapas/compile/source.h"

#include <stdlib.h>

static uint32_t utf8_decode(const unsigned char *text, uint32_t remaining, uint32_t *width)
{
	unsigned char first = text[0];
	if (first < 0x80) {
		*width = 1;
		return first;
	}
	uint32_t needed = first >= 0xc2 && first <= 0xdf ? 2 :
		first >= 0xe0 && first <= 0xef ? 3 :
		first >= 0xf0 && first <= 0xf4 ? 4 : 0;
	if (!needed || needed > remaining) {
		*width = 1;
		return 0xfffd;
	}
	uint32_t value = first & (0x7f >> needed);
	for (uint32_t i = 1; i < needed; i++) {
		if ((text[i] & 0xc0) != 0x80) {
			*width = 1;
			return 0xfffd;
		}
		value = (value << 6) | (text[i] & 0x3f);
	}
	if ((needed == 3 && value < 0x800) ||
	    (needed == 4 && value < 0x10000) ||
	    (value >= 0xd800 && value <= 0xdfff) || value > 0x10ffff) {
		*width = 1;
		return 0xfffd;
	}
	*width = needed;
	return value;
}

tsource_span tsource_span_make(uint32_t start, uint32_t end)
{
	return (tsource_span){ start, end };
}

tsource_span tsource_span_cover(tsource_span first, tsource_span last)
{
	return tsource_span_make(first.start, last.end);
}

int tsource_span_valid(tsource_span span, const tsource_document *document)
{
	return document && span.start <= span.end &&
	       span.end <= tsource_document_length(document);
}

static void document_add_line(tsource_document *document, uint32_t offset)
{
	if (document->line_count >= document->line_capacity) {
		uint32_t capacity = document->line_capacity ?
			document->line_capacity * 2 : 32;
		uint32_t *starts = (uint32_t *)realloc(
			document->line_starts, capacity * sizeof(uint32_t));
		if (!starts)
			abort();
		document->line_starts = starts;
		document->line_capacity = capacity;
	}
	document->line_starts[document->line_count++] = offset;
}

void tsource_document_init(tsource_document *document,
			   const char *name, const char *text)
{
	document->name = tstring_new(name ? name : "");
	document->text = tstring_new(text ? text : "");
	document->line_starts = nullptr;
	document->line_count = 0;
	document->line_capacity = 0;
	document_add_line(document, 0);
	const char *source = tstring_cstr(document->text);
	uint32_t length = tsource_document_length(document);
	for (uint32_t i = 0; i < length; i++) {
		if (source[i] == '\n')
			document_add_line(document, i + 1);
	}
}

void tsource_document_free(tsource_document *document)
{
	if (!document)
		return;
	tstring_free(document->name);
	tstring_free(document->text);
	free(document->line_starts);
	*document = (tsource_document){ 0 };
}

uint32_t tsource_document_length(const tsource_document *document)
{
	if (!document || !document->text)
		return 0;
	size_t length = tstring_len(document->text);
	return length > UINT32_MAX ? UINT32_MAX : (uint32_t)length;
}

void tsource_document_position(const tsource_document *document,
			       uint32_t offset,
			       uint32_t *line, uint32_t *column)
{
	uint32_t length = tsource_document_length(document);
	if (offset > length)
		offset = length;
	uint32_t low = 0;
	uint32_t high = document && document->line_count ?
		document->line_count : 1;
	while (low + 1 < high) {
		uint32_t middle = low + (high - low) / 2;
		if (document->line_starts[middle] <= offset)
			low = middle;
		else
			high = middle;
	}
	if (line)
		*line = low;
	if (column)
		*column = offset - document->line_starts[low];
}

void tsource_document_lsp_position(const tsource_document *document,
				   uint32_t offset,
				   uint32_t *line, uint32_t *character)
{
	uint32_t row = 0, byte_column = 0;
	tsource_document_position(document, offset, &row, &byte_column);
	uint32_t start = document->line_starts[row];
	uint32_t end = start + byte_column;
	const unsigned char *text = (const unsigned char *)tstring_cstr(document->text);
	uint32_t units = 0;
	for (uint32_t at = start; at < end;) {
		uint32_t width = 1;
		uint32_t scalar = utf8_decode(text + at, end - at, &width);
		units += scalar > 0xffff ? 2 : 1;
		at += width;
	}
	if (line)
		*line = row;
	if (character)
		*character = units;
}

uint32_t tsource_document_lsp_offset(const tsource_document *document,
				     uint32_t line, uint32_t character)
{
	if (!document || !document->line_count)
		return 0;
	if (line >= document->line_count)
		return tsource_document_length(document);
	uint32_t start = document->line_starts[line];
	uint32_t limit = line + 1 < document->line_count ?
		document->line_starts[line + 1] : tsource_document_length(document);
	const unsigned char *text = (const unsigned char *)tstring_cstr(document->text);
	uint32_t at = start, units = 0;
	while (at < limit && text[at] != '\r' && text[at] != '\n') {
		uint32_t width = 1;
		uint32_t scalar = utf8_decode(text + at, limit - at, &width);
		uint32_t scalar_units = scalar > 0xffff ? 2 : 1;
		if (units + scalar_units > character)
			break;
		units += scalar_units;
		at += width;
		if (units == character)
			break;
	}
	return at;
}

tstring *tsource_document_slice(const tsource_document *document,
				tsource_span span)
{
	if (!tsource_span_valid(span, document))
		return tstring_new_empty();
	return tstring_substr(document->text, span.start, span.end - span.start);
}
