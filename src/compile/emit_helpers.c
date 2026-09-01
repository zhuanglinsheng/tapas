#include "internal.h"

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>

int str_to_long_int(const tstring *literal, long *value)
{
	const char *text = tstring_cstr(literal);
	size_t length = tstring_len(literal);
	if (length == 0 || (length > 1 && text[0] == '0') ||
	    !isdigit((unsigned char)text[0]))
		return 0;
	for (size_t i = 1; i < length; i++)
		if (!isdigit((unsigned char)text[i])) return 0;
	errno = 0;
	char *end = NULL;
	long parsed = strtol(text, &end, 10);
	if (errno == ERANGE || end != text + length)
		twarn(ErrCompile_InvalidLiter, "integer literal", text);
	*value = parsed;
	return 1;
}

int str_to_float(const tstring *literal, double *value)
{
	const char *text = tstring_cstr(literal);
	size_t length = tstring_len(literal);
	if (!length) return 0;
	errno = 0;
	char *end = NULL;
	double parsed = strtod(text, &end);
	if (end == text || end != text + length) return 0;
	int has_float_marker = 0;
	for (size_t i = 0; i < length; i++)
		has_float_marker |= text[i] == '.' || text[i] == 'e' || text[i] == 'E';
	if (!has_float_marker) return 0;
	if (errno == ERANGE)
		twarn(ErrCompile_InvalidLiter, "float literal", text);
	*value = parsed;
	return 1;
}

void compile_emit_reference(tcp *cp, const tstring *name,
			    tvmcmd_vect *instructions, tconsts *constants)
{
	if (tstring_empty(name))
		twarn(ErrCompile_InvalidLiter, "reference", "empty name");
	tobj_ctr_addr address;
	uint_objs temporary = tobj_ctr_obj_loc(&cp->tmpctr, name);
	if (temporary < tobj_ctr_obj_len_all(&cp->tmpctr)) {
		tvmcmd_vect_append(instructions, tbycode_make_lr(
			OP_PUSHX, (uint16_t)temporary, tpushx_tmp_addr()));
	} else if (tobj_ctr_obj_addr(&cp->objctr, name, &address)) {
		uint16_t kind = address.depth == 0 ? tpushx_local_addr() :
			tpushx_upval_addr(address.depth);
		tvmcmd_vect_append(instructions, tbycode_make_lr(
			OP_PUSHX, (uint16_t)address.slot, kind));
	} else {
		twarn(ErrCompile_InvalidLiter, "reference", tstring_cstr(name));
	}
	(void)constants;
	treg_ctr_add(&cp->regctr);
}
