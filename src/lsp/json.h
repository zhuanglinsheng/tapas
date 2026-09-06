#ifndef TAPAS_LSP_JSON_H
#define TAPAS_LSP_JSON_H

#include "tapas/dsa/tstring.h"

typedef enum {
	tjson_null, tjson_boolean, tjson_number, tjson_string,
	tjson_array, tjson_object
} tjson_kind;

typedef struct tjson_value tjson_value;
typedef struct tjson_member tjson_member;

struct tjson_member {
	tstring *key;
	tjson_value *value;
	tjson_member *next;
};

struct tjson_value {
	tjson_kind kind;
	union {
		int boolean;
		double number;
		tstring *string;
		struct { tjson_value **items; uint32_t count; } array;
		tjson_member *object;
	};
};

tjson_value *tjson_parse(const char *text, const char **error);
void tjson_free(tjson_value *value);
const tjson_value *tjson_get(const tjson_value *object, const char *key);
const char *tjson_string_value(const tjson_value *value);
int tjson_integer_value(const tjson_value *value, int fallback);
void tjson_append_escaped(tstring *output, const char *text);
void tjson_append_value(tstring *output, const tjson_value *value);

#endif
