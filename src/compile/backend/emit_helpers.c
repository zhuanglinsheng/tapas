#include "internal.h"
#include "tapas/dsa/tstring.h"

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
		if (!isdigit((unsigned char)text[i]))
			return 0;
	errno = 0;
	char *end = nullptr;
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
	char *end = nullptr;
	double parsed = strtod(text, &end);
	if (end == text || end != text + length)
		return 0;
	int has_float_marker = 0;
	for (size_t i = 0; i < length; i++)
		has_float_marker |= text[i] == '.' || text[i] == 'e' || text[i] == 'E';
	if (!has_float_marker)
		return 0;
	if (errno == ERANGE)
		twarn(ErrCompile_InvalidLiter, "float literal", text);
	*value = parsed;
	return 1;
}

int compile_reference_address(tcp *cp, const tstring *name,
			      uint16_t *slot, uint16_t *address)
{
	uint_objs temporary = tobj_ctr_obj_loc(&cp->tmpctr, name);
	if (temporary < tobj_ctr_obj_len_all(&cp->tmpctr)) {
		*slot = (uint16_t)temporary;
		*address = tpushx_tmp_addr();
		return 1;
	}
	tobj_ctr_addr resolved;
	if (!tobj_ctr_obj_addr(&cp->objctr, name, &resolved))
		return 0;
	*slot = (uint16_t)resolved.slot;
	*address = resolved.depth == 0 ? tpushx_local_addr() :
		tpushx_upval_addr(resolved.depth);
	return 1;
}

void compile_emit_reference(tcp *cp, const tstring *name,
			    tvmcmd_vect *instructions, tconsts *constants)
{
	if (tstring_empty(name))
		twarn(ErrCompile_InvalidLiter, "reference", "empty name");
	uint16_t slot = 0;
	uint16_t address = 0;
	if (!compile_reference_address(cp, name, &slot, &address))
		twarn(ErrCompile_InvalidLiter, "reference", tstring_cstr(name));
	tvmcmd_vect_append(instructions,
		tbycode_make_lr(OP_PUSHX, slot, address));
	(void)constants;
	treg_ctr_add(&cp->regctr);
}
