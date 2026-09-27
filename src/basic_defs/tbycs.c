#include "tapas/basic_defs/tbycs.h"
#include "tapas/version.h"
#include "tapas/basic_defs/tbasis.h"
#include "tapas/dsa/tstring.h"
#include "tapas/objects/tstr.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static_assert(OP_COUNT <= 64, "bytecode opcode exceeds its 6-bit encoding");

/*===========================================================================*
 * 1. Single Bytecode (tbycode)
 *===========================================================================*/

/**
 * Each bytecode instruction is packed into a single uint32_t.
 * The low 6 bits encode the instruction (tins).
 * The upper 26 bits encode parameters depending on instruction type:
 *   U-type:  26-bit unsigned parameter
 *   LR-type: 13-bit L (left), 13-bit R (right)
 *   Lbi-type: 13-bit L, 8-bit b, 5-bit i
 */

/* ---- Constructors ---- */

/** OP_PASS (no params) */
tbycode tbycode_make(uint8_t ins)
{
	return ((uint32_t)ins << 2) >> 2;
}

/** U-type: 26-bit unsigned parameter */
tbycode tbycode_make_u(uint8_t ins, uint32_t u)
{
	uint32_t tmp = 0;
	tmp += (u << 6);
	tmp += (((uint32_t)ins << 2) >> 2);
	return tmp;
}

/** LR-type: 13-bit L, 13-bit R */
tbycode tbycode_make_lr(uint8_t ins, uint16_t L, uint16_t R)
{
	uint32_t tmp = 0;
	uint32_t iL = L;
	uint32_t iR = R;
	tmp += (iR << 19);
	tmp += ((iL << 19) >> 13);
	tmp += (((uint32_t)ins << 2) >> 2);
	return tmp;
}

/** Lbi-type: 13-bit L, 8-bit b, 5-bit i */
tbycode tbycode_make_lbi(uint8_t ins, uint16_t L, uint8_t b, uint8_t i)
{
	uint32_t tmp = 0;
	uint32_t iL = L;
	uint32_t ib = b;
	uint32_t ii = i;
	tmp += (ii << 27);
	tmp += ((ib << 24) >> 5);
	tmp += ((iL << 19) >> 13);
	tmp += (((uint32_t)ins << 2) >> 2);
	return tmp;
}

static void tbycode_tostring_binary(char *buf, size_t buf_size,
				    const char *name, tbycode c)
{
	if (tbycode_binop_named(c))
		snprintf(buf, buf_size, "%-11s %u  named %u", name,
			(unsigned)tbycode_get_L(c),
			(unsigned)tbycode_binop_address(c));
	else
		snprintf(buf, buf_size, "%-11s %u  %u", name,
			(unsigned)tbycode_get_L(c),
			(unsigned)tbycode_get_R(c));
}

/**
 * Convert bytecode to debug string representation.
 * Writes at most buf_size bytes, including the terminating null character.
 */
void tbycode_tostring(tbycode c, char *buf, size_t buf_size)
{
	if (!buf || buf_size == 0)
		return;
	tins ins = tbycode_ins(c);
	switch (ins) {
	case OP_PASS:
		snprintf(buf, buf_size, "OP_PASS     ");
		break;
	case OP_VCRT:
		snprintf(buf, buf_size, "OP_VCRT     %u  %s%s",
			(unsigned)tbycode_get_L(c),
			tbycode_get_R(c) & TVCRT_ENV_FLAG ? "env" : "tmp",
			tbycode_get_R(c) & TVCRT_INIT_FLAG ? "  init" : "");
		break;
	case OP_TMPDEL:
		snprintf(buf, buf_size, "OP_TMPDEL   %u", (unsigned)tbycode_get_U(c));
		break;
	case OP_THIS:
		snprintf(buf, buf_size, "OP_THIS     ");
		break;
	case OP_BASE:
		snprintf(buf, buf_size, "OP_BASE     ");
		break;
	case OP_RET:
		snprintf(buf, buf_size, "OP_RET      ");
		break;
	case OP_IN:
		snprintf(buf, buf_size, "OP_IN       ");
		break;
	case OP_PAIR:
		snprintf(buf, buf_size, "OP_PAIR     ");
		break;
	case OP_TO:
		snprintf(buf, buf_size, "OP_TO       ");
		break;
	case OP_POPN:
		snprintf(buf, buf_size,
			"OP_POPN     %u  print=%u  tmp=%u",
			(unsigned)tbycode_get_L(c),
			(unsigned)tbycode_popn_print(c),
			(unsigned)tbycode_popn_temporary_count(c));
		break;
	case OP_POPCOV:
		snprintf(buf, buf_size,
			"OP_POPCOV   %u  %u",
			(unsigned)tbycode_get_L(c),
			(unsigned)tbycode_get_R(c));
		break;
	case OP_TYPEFWD:
		snprintf(buf, buf_size, "OP_TYPEFWD  ");
		break;
	case OP_TYPEDEFINE:
		snprintf(buf, buf_size,
			"OP_TYPEDEFINE %u  %u",
			(unsigned)tbycode_get_L(c),
			(unsigned)tbycode_get_R(c));
		break;
	case OP_LOOPAS:
		snprintf(buf, buf_size,
			"OP_LOOPAS   %u  %u",
			(unsigned)tbycode_get_L(c),
			(unsigned)tbycode_get_R(c));
		break;
	case OP_LOOPRANGE:
		snprintf(buf, buf_size,
			"OP_LOOPRANGE %u  %u",
			(unsigned)tbycode_get_L(c),
			(unsigned)tbycode_get_R(c));
		break;
	case OP_JPF:
		snprintf(buf, buf_size, "OP_JPF      %u", (unsigned)tbycode_get_U(c));
		break;
	case OP_JPB:
		snprintf(buf, buf_size, "OP_JPB      %u", (unsigned)tbycode_get_U(c));
		break;
	case OP_CJPFPOP:
		snprintf(buf, buf_size, "OP_CJPFPOP  %u", (unsigned)tbycode_get_U(c));
		break;
	case OP_CJPBPOP:
		snprintf(buf, buf_size, "OP_CJPBPOP  %u", (unsigned)tbycode_get_U(c));
		break;
	case OP_PUSHX:
		if (!tpushx_isenv(tbycode_get_R(c)))
			snprintf(buf, buf_size,
				"OP_PUSHX    %u  tmp",
				(unsigned)tbycode_get_L(c));
		else if (!tpushx_is_upval(tbycode_get_R(c)))
			snprintf(buf, buf_size,
				"OP_PUSHX    %u  local",
				(unsigned)tbycode_get_L(c));
		else
			snprintf(buf, buf_size,
				"OP_PUSHX    %u  upval %u",
				(unsigned)tbycode_get_L(c),
				(unsigned)tpushx_depth(tbycode_get_R(c)));
		break;
	case OP_PUSHI:
		if (tbycode_pushi_is_immediate(c))
			snprintf(buf, buf_size, "OP_PUSHI    %ld  immediate",
				tbycode_pushi_immediate_value(c));
		else
			snprintf(buf, buf_size, "OP_PUSHI    %u",
				(unsigned)tbycode_get_U(c));
		break;
	case OP_PUSHFLT:
		snprintf(buf, buf_size, "OP_PUSHFLT  %u", (unsigned)tbycode_get_U(c));
		break;
	case OP_PUSHB:
		snprintf(buf, buf_size, "OP_PUSHB    %u", (unsigned)tbycode_get_U(c));
		break;
	case OP_PUSHS:
		snprintf(buf, buf_size, "OP_PUSHS    %u", (unsigned)tbycode_get_U(c));
		break;
	case OP_PUSHDICT:
		snprintf(buf, buf_size, "OP_PUSHDICT %u", (unsigned)tbycode_get_U(c));
		break;
	case OP_PUSHLIST:
		snprintf(buf, buf_size, "OP_PUSHLIST %u", (unsigned)tbycode_get_U(c));
		break;
	case OP_PUSHINFO:
		snprintf(buf, buf_size, "OP_PUSHINFO %u", (unsigned)tbycode_get_U(c));
		break;
	case OP_IMPORT:
		snprintf(buf, buf_size, "OP_IMPORT   %u", (unsigned)tbycode_get_U(c));
		break;
	case OP_IDXR:
		if (tbycode_idxr_named(c))
			snprintf(buf, buf_size,
				"OP_IDXR     %u  named %u %u%s%s",
				(unsigned)tbycode_idxr_count(c),
				(unsigned)tbycode_idxr_named_slot(c),
				(unsigned)tbycode_idxr_named_address(c),
				tbycode_idxr_slice(c) ? "  slice" : "",
				tbycode_idxr_compare(c) ? "  compare" : "");
		else
			snprintf(buf, buf_size, "OP_IDXR     %u%s%s",
				(unsigned)tbycode_idxr_count(c),
				tbycode_idxr_slice(c) ? "  slice" : "",
				tbycode_idxr_compare(c) ? "  compare" : "");
		break;
	case OP_EVALSF:
		if (tbycode_get_i(c))
			snprintf(buf, buf_size,
				"OP_EVALSF   %u  direct %u  depth %u",
				(unsigned)tbycode_get_b(c),
				(unsigned)tbycode_get_L(c),
				(unsigned)tbycode_get_i(c) - 1);
		else
			snprintf(buf, buf_size, "OP_EVALSF   %u",
				(unsigned)tbycode_get_U(c));
		break;
	case OP_EVALCF:
		if (tbycode_get_i(c))
			snprintf(buf, buf_size,
				"OP_EVALCF   %u  direct %u  depth %u",
				(unsigned)tbycode_get_b(c),
				(unsigned)tbycode_get_L(c),
				(unsigned)tbycode_get_i(c) - 1);
		else
			snprintf(buf, buf_size, "OP_EVALCF   %u",
				(unsigned)tbycode_get_U(c));
		break;
	case OP_EVALTF:
		snprintf(buf, buf_size, "OP_EVALTF   %u", (unsigned)tbycode_get_U(c));
		break;
	case OP_EVALDF:
		if ((tbycode_get_i(c) & 0x0f) != 0)
			snprintf(buf, buf_size,
				"OP_EVALDF   %u  env %u depth %u%s",
				(unsigned)tbycode_get_b(c),
				(unsigned)tbycode_get_L(c),
				(unsigned)(tbycode_get_i(c) & 0x0f) - 1,
				tbycode_get_i(c) & 0x10 ? " retained" : "");
		else
			snprintf(buf, buf_size, "OP_EVALDF   %u  local %u%s",
				(unsigned)tbycode_get_b(c),
				(unsigned)tbycode_get_L(c),
				tbycode_get_i(c) & 0x10 ? " retained" : "");
		break;
	case OP_EVAL:
		snprintf(buf, buf_size, "OP_EVAL     %u", (unsigned)tbycode_get_U(c));
		break;
	case OP_IDXL:
		snprintf(buf, buf_size,
			"OP_IDXL     %u  %u  %u",
			(unsigned)tbycode_get_L(c),
			(unsigned)tbycode_get_b(c),
			(unsigned)tbycode_get_i(c));
		break;
	case OP_PUSHF:
		snprintf(buf, buf_size, "OP_PUSHF    %u", (unsigned)tbycode_get_U(c));
		break;
	case OP_PUSHRULE:
		snprintf(buf, buf_size, "OP_PUSHRULE %u", (unsigned)tbycode_get_U(c));
		break;
	case OP_RULEVALUE:
		snprintf(buf, buf_size, "OP_RULEVALUE %u", (unsigned)tbycode_get_U(c));
		break;
	case OP_RULENOT:
		snprintf(buf, buf_size, "OP_RULENOT %u", (unsigned)tbycode_get_U(c));
		break;
	case OP_RULETRUTH:
		snprintf(buf, buf_size, "OP_RULETRUTH %u", (unsigned)tbycode_get_U(c));
		break;
	case OP_RULEITEM:
		snprintf(buf, buf_size, "OP_RULEITEM %u", (unsigned)tbycode_get_U(c));
		break;
	case OP_RULEIMPLY:
		snprintf(buf, buf_size, "OP_RULEIMPLY %u", (unsigned)tbycode_get_U(c));
		break;
	case OP_ADD:
		tbycode_tostring_binary(buf, buf_size, "OP_ADD", c);
		break;
	case OP_SUB:
		tbycode_tostring_binary(buf, buf_size, "OP_SUB", c);
		break;
	case OP_MUL:
		tbycode_tostring_binary(buf, buf_size, "OP_MUL", c);
		break;
	case OP_DIV:
		tbycode_tostring_binary(buf, buf_size, "OP_DIV", c);
		break;
	case OP_MOD:
		tbycode_tostring_binary(buf, buf_size, "OP_MOD", c);
		break;
	case OP_POW:
		tbycode_tostring_binary(buf, buf_size, "OP_POW", c);
		break;
	case OP_MMUL:
		tbycode_tostring_binary(buf, buf_size, "OP_MMUL", c);
		break;
	case OP_POS:
		snprintf(buf, buf_size, "OP_POS");
		break;
	case OP_NEG:
		snprintf(buf, buf_size, "OP_NEG");
		break;
	case OP_NOT:
		snprintf(buf, buf_size, "OP_NOT");
		break;
	case OP_EQ:
		tbycode_tostring_binary(buf, buf_size, "OP_EQ", c);
		break;
	case OP_NE:
		tbycode_tostring_binary(buf, buf_size, "OP_NE", c);
		break;
	case OP_GE:
		tbycode_tostring_binary(buf, buf_size, "OP_GE", c);
		break;
	case OP_SG:
		tbycode_tostring_binary(buf, buf_size, "OP_SG", c);
		break;
	case OP_LE:
		tbycode_tostring_binary(buf, buf_size, "OP_LE", c);
		break;
	case OP_SL:
		tbycode_tostring_binary(buf, buf_size, "OP_SL", c);
		break;
	case OP_AND:
		tbycode_tostring_binary(buf, buf_size, "OP_AND", c);
		break;
	case OP_OR:
		tbycode_tostring_binary(buf, buf_size, "OP_OR", c);
		break;
	case OP_BAND:
		tbycode_tostring_binary(buf, buf_size, "OP_BAND", c);
		break;
	case OP_BOR:
		tbycode_tostring_binary(buf, buf_size, "OP_BOR", c);
		break;
	default:
		buf[0] = '\0';
		break;
	}
}

/*===========================================================================*
 * 2. Dynamic Bytecode Vector (replacing tvmcmd_vect)
 *===========================================================================*/

void tvmcmd_vect_init(tvmcmd_vect *v)
{
	v->data = nullptr;
	v->locs = nullptr;
	v->size = 0;
	v->capacity = 0;
}

static void tsource_loc_clear(tsource_loc *loc)
{
	if (!loc)
		return;
	tstring_free(loc->source);
	tstring_free(loc->file);
	memset(loc, 0, sizeof(*loc));
}

static void tsource_loc_array_clear(tsource_loc *locs, uint_cmds count)
{
	if (!locs)
		return;
	for (uint_cmds i = 0; i < count; i++)
		tsource_loc_clear(&locs[i]);
	free(locs);
}

static void tsource_loc_copy(tsource_loc *dst, const tsource_loc *src)
{
	memset(dst, 0, sizeof(*dst));
	if (!src)
		return;
	dst->source = src->source ? tstring_dup(src->source) : nullptr;
	dst->file = src->file ? tstring_dup(src->file) : nullptr;
	dst->line = src->line;
	dst->column = src->column;
}

static void tsource_loc_capture(tsource_loc *dst)
{
	const char *source = terror_current_source_context();
	const char *file = terror_current_file_context();
	memset(dst, 0, sizeof(*dst));
	dst->source = source ? tstring_new(source) : nullptr;
	dst->file = file ? tstring_new(file) : nullptr;
	dst->line = terror_current_line_context();
	dst->column = terror_current_column_context();
}

static void tvmcmd_vect_reserve(tvmcmd_vect *v, uint32_t needed)
{
	while (needed > v->capacity) {
		v->capacity = v->capacity ? v->capacity * 2 : 64;
		v->data = (tbycode *)realloc(v->data,
					     v->capacity * sizeof(tbycode));
		v->locs = (tsource_loc *)realloc(v->locs,
						 v->capacity * sizeof(tsource_loc));
		if (!v->data || !v->locs)
			twarn(ErrRuntime_Other, "tvmcmd_vect_reserve", "out of memory");
	}
}

void tvmcmd_vect_free(tvmcmd_vect *v)
{
	free(v->data);
	tsource_loc_array_clear(v->locs, v->size);
	v->data = nullptr;
	v->locs = nullptr;
	v->size = 0;
	v->capacity = 0;
}

uint_cmds tvmcmd_vect_size32(tvmcmd_vect *v)
{
	return (uint_cmds)v->size;
}

tbycode tvmcmd_vect_back(tvmcmd_vect *v)
{
	return v->data[v->size - 1];
}

void tvmcmd_vect_pop_back(tvmcmd_vect *v)
{
	if (v->size > 0) {
		tsource_loc_clear(&v->locs[v->size - 1]);
		v->size--;
	}
}

void tvmcmd_vect_append(tvmcmd_vect *v, tbycode cmd)
{
	if (v->size >= CMD_LIMIT - 1)
		twarn(ErrCompile_CMDOutOfLimit, "tvmcmd_vect_append", "");
	tvmcmd_vect_reserve(v, v->size + 1);
	v->data[v->size] = cmd;
	tsource_loc_capture(&v->locs[v->size]);
	v->size++;
}

void tvmcmd_vect_insert_range(
		tvmcmd_vect *v, uint32_t pos, tbycode *src, uint32_t count)
{
	tvmcmd_vect_reserve(v, v->size + count);
	memmove(v->data + pos + count,
		v->data + pos,
		(v->size - pos) * sizeof(tbycode));
	memmove(v->locs + pos + count,
		v->locs + pos,
		(v->size - pos) * sizeof(tsource_loc));
	memcpy(v->data + pos, src, count * sizeof(tbycode));
	for (uint32_t i = 0; i < count; i++)
		tsource_loc_capture(&v->locs[pos + i]);
	v->size += count;
}

void tvmcmd_vect_insert_vect(tvmcmd_vect *v, uint32_t pos, const tvmcmd_vect *src)
{
	uint32_t count = src ? src->size : 0;
	tvmcmd_vect_reserve(v, v->size + count);
	memmove(v->data + pos + count,
		v->data + pos,
		(v->size - pos) * sizeof(tbycode));
	memmove(v->locs + pos + count,
		v->locs + pos,
		(v->size - pos) * sizeof(tsource_loc));
	for (uint32_t i = 0; i < count; i++) {
		v->data[pos + i] = src->data[i];
		tsource_loc_copy(&v->locs[pos + i], &src->locs[i]);
	}
	v->size += count;
}

void tvmcmd_vect_resolve_loop_control(tvmcmd_vect *v, uint32_t begin,
				      uint32_t end, uint32_t continue_target,
				      uint32_t break_target,
				      tbycode continue_marker,
				      tbycode break_marker)
{
	if (!v || begin > end || end > v->size || continue_target > v->size ||
	    break_target > v->size)
		twarn(ErrCompile_Other, "loop control", "invalid jump range");

	for (uint32_t i = begin; i < end; i++) {
		if (v->data[i] == break_marker) {
			if (break_target <= i)
				twarn(ErrCompile_Other, "break", "invalid jump target");
			v->data[i] = tbycode_make_u(
				OP_JPF, break_target - i - 1);
		} else if (v->data[i] == continue_marker) {
			if (continue_target <= i)
				v->data[i] = tbycode_make_u(
					OP_JPB, i - continue_target + 1);
			else
				v->data[i] = tbycode_make_u(
					OP_JPF, continue_target - i - 1);
		}
	}
}

/*===========================================================================*
 * 3. Constant Vectors (using tstring instead of char*)
 *===========================================================================*/

void consts_str_vect_init(consts_str_vect *v)
{
	v->data = nullptr;
	v->size = 0;
	v->capacity = 0;
}

void consts_str_vect_free(consts_str_vect *v)
{
	uint32_t i;
	for (i = 0; i < v->size; i++)
		tstring_free(v->data[i]);
	free(v->data);
	v->data = nullptr;
	v->size = 0;
	v->capacity = 0;
}

uint_csts consts_str_vect_add(consts_str_vect *v, const char *str)
{
	uint32_t i;
	for (i = 0; i < v->size; i++) {
		if (tstring_eq_cstr(v->data[i], str))
			return i;
	}
	if (v->size >= CST_LIMIT - 1)
		twarn(ErrCompile_CSTOutOfLimit, "consts_str_vect_append", "");
	if (v->size >= v->capacity) {
		v->capacity = v->capacity ? v->capacity * 2 : 64;
		v->data = (tstring **)realloc(v->data, v->capacity * sizeof(tstring *));
	}
	v->data[v->size] = tstring_new(str);
	return v->size++;
}

void consts_long_vect_init(consts_long_vect *v)
{
	v->data = nullptr;
	v->size = 0;
	v->capacity = 0;
}

void consts_long_vect_free(consts_long_vect *v)
{
	free(v->data);
	v->data = nullptr;
	v->size = 0;
	v->capacity = 0;
}

uint_csts consts_long_vect_add(consts_long_vect *v, long val)
{
	uint32_t i;
	for (i = 0; i < v->size; i++) {
		if (v->data[i] == val)
			return i;
	}
	if (v->size >= CST_LIMIT - 1)
		twarn(ErrCompile_CSTOutOfLimit, "consts_long_vect_append", "");
	if (v->size >= v->capacity) {
		v->capacity = v->capacity ? v->capacity * 2 : 64;
		v->data = (long *)realloc(v->data, v->capacity * sizeof(long));
	}
	v->data[v->size] = val;
	return v->size++;
}

void consts_float_vect_init(consts_float_vect *v)
{
	v->data = nullptr;
	v->size = 0;
	v->capacity = 0;
}

void consts_float_vect_free(consts_float_vect *v)
{
	free(v->data);
	v->data = nullptr;
	v->size = 0;
	v->capacity = 0;
}

uint_csts consts_float_vect_add(consts_float_vect *v, double val)
{
	uint32_t i;
	for (i = 0; i < v->size; i++) {
		if (v->data[i] == val)
			return i;
	}
	if (v->size >= CST_LIMIT - 1)
		twarn(ErrCompile_CSTOutOfLimit,
			  "consts_float_vect_append",
			  "");
	if (v->size >= v->capacity) {
		v->capacity = v->capacity ? v->capacity * 2 : 64;
		v->data = (double *)realloc(
			v->data, v->capacity * sizeof(double));
	}
	v->data[v->size] = val;
	return v->size++;
}

void tconsts_init(tconsts *c)
{
	consts_str_vect_init(&c->__strcsts);
	consts_long_vect_init(&c->__intcsts);
	consts_float_vect_init(&c->__fltcsts);
}

void tconsts_free(tconsts *c)
{
	consts_str_vect_free(&c->__strcsts);
	consts_long_vect_free(&c->__intcsts);
	consts_float_vect_free(&c->__fltcsts);
}

uint_csts tconsts_add_str_const(tconsts *c, const char *str)
{
	return consts_str_vect_add(&c->__strcsts, str);
}

uint_csts tconsts_add_int_const(tconsts *c, long val)
{
	return consts_long_vect_add(&c->__intcsts, val);
}

uint_csts tconsts_add_float_const(tconsts *c, double val)
{
	return consts_float_vect_add(&c->__fltcsts, val);
}

void tconsts_copy(tconsts *dst, tconsts *src)
{
	uint32_t i;
	tconsts_init(dst);
	for (i = 0; i < src->__strcsts.size; i++)
		consts_str_vect_add(&dst->__strcsts,
			tstring_cstr(src->__strcsts.data[i]));
	for (i = 0; i < src->__intcsts.size; i++)
		consts_long_vect_add(&dst->__intcsts, src->__intcsts.data[i]);
	for (i = 0; i < src->__fltcsts.size; i++)
		consts_float_vect_add(&dst->__fltcsts, src->__fltcsts.data[i]);
}


/**
 * Build a twrapper from compiling data (tvmcmd_vect, tconsts, tcinfo).
 * The wrapper owns deep copies of all data, leaving the compiler state intact.
 */
twrapper *tanalyser_wrap(tvmcmd_vect *tcmds, tconsts *consts, tcinfo *info)
{
	for (uint_cmds i = 0; i < tvmcmd_vect_size32(tcmds); i++) {
		if (tbycode_ins(tcmds->data[i]) >= OP_COUNT)
			twarn(ErrCompile_Other, "tanalyser_wrap",
			      "unresolved compiler marker");
	}
	twrapper *wrapper = (twrapper *)calloc(1, sizeof(twrapper));
	if (!wrapper)
		return nullptr;

	wrapper->info = *info;
	wrapper->ncmds = tvmcmd_vect_size32(tcmds);
	wrapper->cmdarr = (tbycode *)malloc(wrapper->ncmds * sizeof(tbycode));
	if (wrapper->ncmds > 0 && !wrapper->cmdarr) {
		free(wrapper);
		return nullptr;
	}
	if (wrapper->ncmds > 0)
		memcpy(wrapper->cmdarr, tcmds->data, wrapper->ncmds * sizeof(tbycode));
	if (wrapper->ncmds > 0) {
		wrapper->source_locs =
			(tsource_loc *)calloc(wrapper->ncmds, sizeof(tsource_loc));
		if (!wrapper->source_locs)
			goto wrap_err;
		for (uint_cmds i = 0; i < wrapper->ncmds; i++)
			tsource_loc_copy(&wrapper->source_locs[i], &tcmds->locs[i]);
	}

	wrapper->consts.ncstrs = consts->__strcsts.size;
	wrapper->consts.ncints = consts->__intcsts.size;
	wrapper->consts.ncflts = consts->__fltcsts.size;

	/* Strings */
	if (wrapper->consts.ncstrs > 0) {
		uint_csts i;
		wrapper->consts.cstrs =
			(tstring **)calloc(wrapper->consts.ncstrs, sizeof(tstring *));
		if (!wrapper->consts.cstrs)
			goto wrap_err;
		for (i = 0; i < wrapper->consts.ncstrs; i++) {
			wrapper->consts.cstrs[i] = tstring_dup(consts->__strcsts.data[i]);
			if (!wrapper->consts.cstrs[i])
				goto wrap_err;
		}
		wrapper->consts.csobjs =
			(tstr **)calloc(wrapper->consts.ncstrs, sizeof(tstr *));
		if (!wrapper->consts.csobjs)
			goto wrap_err;
		for (i = 0; i < wrapper->consts.ncstrs; i++) {
			wrapper->consts.csobjs[i] = tstr_new_interned(
				tstring_cstr(wrapper->consts.cstrs[i]));
			if (!wrapper->consts.csobjs[i])
				goto wrap_err;
		}
	} else {
		wrapper->consts.cstrs = nullptr;
		wrapper->consts.csobjs = nullptr;
	}

	/* Integers */
	if (wrapper->consts.ncints > 0) {
		wrapper->consts.cints =
			(long *)malloc(wrapper->consts.ncints * sizeof(long));
		if (!wrapper->consts.cints)
			goto wrap_err;
		memcpy(wrapper->consts.cints,
			   consts->__intcsts.data,
			   wrapper->consts.ncints * sizeof(long));
	} else {
		wrapper->consts.cints = nullptr;
	}

	/* Doubles */
	if (wrapper->consts.ncflts > 0) {
		wrapper->consts.cflts = (double *)malloc(
			wrapper->consts.ncflts * sizeof(double));
		if (!wrapper->consts.cflts)
			goto wrap_err;
		memcpy(wrapper->consts.cflts,
			   consts->__fltcsts.data,
			   wrapper->consts.ncflts * sizeof(double));
	} else {
		wrapper->consts.cflts = nullptr;
	}

	return wrapper;

wrap_err:
	tanalyser_clean_wrapper(wrapper);
	return nullptr;
}

#define TAPC_FILE_MAGIC       ((uint64_t)0x5441504153424331ULL)
#define TAPC_SOURCE_MAP_MAGIC ((uint64_t)0x5450415352433031ULL)
#define TAPC_VERSION_MAX      UINT32_C(1024)

static int tapc_write_header(FILE *f, const twrapper *wrapper)
{
	uint64_t magic = TAPC_FILE_MAGIC;
	uint32_t version_length = (uint32_t)strlen(TAPAS_VERSION);
	uint8_t reserved[3] = { wrapper->info.padding_1, 0, 0 };
	return fwrite(&magic, sizeof(magic), 1, f) == 1 &&
	       fwrite(&version_length, sizeof(version_length), 1, f) == 1 &&
	       fwrite(TAPAS_VERSION, 1, version_length, f) == version_length &&
	       fwrite(&wrapper->ncmds, sizeof(wrapper->ncmds), 1, f) == 1 &&
	       fwrite(&wrapper->consts.ncints,
		      sizeof(wrapper->consts.ncints), 1, f) == 1 &&
	       fwrite(&wrapper->consts.ncstrs,
		      sizeof(wrapper->consts.ncstrs), 1, f) == 1 &&
	       fwrite(&wrapper->consts.ncflts,
		      sizeof(wrapper->consts.ncflts), 1, f) == 1 &&
	       fwrite(&wrapper->info.obj_max,
		      sizeof(wrapper->info.obj_max), 1, f) == 1 &&
	       fwrite(&wrapper->info.tmp_max,
		      sizeof(wrapper->info.tmp_max), 1, f) == 1 &&
	       fwrite(&wrapper->info.reg_max,
		      sizeof(wrapper->info.reg_max), 1, f) == 1 &&
	       fwrite(reserved, sizeof(reserved), 1, f) == 1;
}

static int tapc_read_header(FILE *f, twrapper *wrapper)
{
	uint64_t magic = 0;
	uint32_t version_length = 0;
	char version[TAPC_VERSION_MAX + 1];
	uint8_t reserved[3];
	if (fread(&magic, sizeof(magic), 1, f) != 1 ||
	    fread(&version_length, sizeof(version_length), 1, f) != 1 ||
	    magic != TAPC_FILE_MAGIC || version_length > TAPC_VERSION_MAX ||
	    fread(version, 1, version_length, f) != version_length)
		return 0;
	version[version_length] = '\0';
	if (strcmp(version, TAPAS_VERSION) != 0)
		return 0;
	int ok = fread(&wrapper->ncmds, sizeof(wrapper->ncmds), 1, f) == 1 &&
	       fread(&wrapper->consts.ncints,
		     sizeof(wrapper->consts.ncints), 1, f) == 1 &&
	       fread(&wrapper->consts.ncstrs,
		     sizeof(wrapper->consts.ncstrs), 1, f) == 1 &&
	       fread(&wrapper->consts.ncflts,
		     sizeof(wrapper->consts.ncflts), 1, f) == 1 &&
	       fread(&wrapper->info.obj_max,
		     sizeof(wrapper->info.obj_max), 1, f) == 1 &&
	       fread(&wrapper->info.tmp_max,
		     sizeof(wrapper->info.tmp_max), 1, f) == 1 &&
	       fread(&wrapper->info.reg_max,
		     sizeof(wrapper->info.reg_max), 1, f) == 1 &&
	       fread(reserved, sizeof(reserved), 1, f) == 1;
	if (ok)
		wrapper->info.padding_1 = reserved[0];
	return ok;
}

static int tapc_write_tstring(FILE *f, const tstring *s)
{
	uint8_t has_value = s != nullptr;
	if (fwrite(&has_value, sizeof(has_value), 1, f) != 1)
		return 0;
	if (!has_value)
		return 1;
	size_t bytes = tstring_len(s);
	if (bytes == SIZE_MAX)
		return 0;
	uint64_t len = bytes;
	bytes++;
	return fwrite(&len, sizeof(len), 1, f) == 1 &&
		fwrite(tstring_cstr(s), 1, bytes, f) == bytes;
}

static int tapc_read_tstring(FILE *f, tstring **out)
{
	uint8_t has_value = 0;
	*out = nullptr;
	if (1 != fread(&has_value, sizeof(has_value), 1, f))
		return 0;
	if (!has_value)
		return 1;
	uint64_t len = 0;
	if (1 != fread(&len, sizeof(len), 1, f))
		return 0;
	if (len > SIZE_MAX - 1)
		return 0;
	size_t bytes = (size_t)len + 1;
	char *raw = (char *)calloc(bytes, 1);
	if (!raw)
		return 0;
	if (fread(raw, 1, bytes, f) != bytes) {
		free(raw);
		return 0;
	}
	if (raw[bytes - 1] != '\0') {
		free(raw);
		return 0;
	}
	*out = tstring_new(raw);
	free(raw);
	return *out != nullptr;
}

typedef struct {
	const tstring **items;
	uint32_t count;
	uint32_t capacity;
} tapc_string_table;

#define TAPC_NO_STRING UINT32_MAX

/* The source-map payload stores one shared string table followed by runs of
 * identical locations. Each run retains its full 64-bit line and column and
 * references nullable source/file strings by table index. */

static int tapc_string_table_id(tapc_string_table *table,
				const tstring *value, uint32_t *id)
{
	if (!value) {
		*id = TAPC_NO_STRING;
		return 1;
	}
	for (uint32_t i = 0; i < table->count; i++) {
		if (tstring_eq(table->items[i], value)) {
			*id = i;
			return 1;
		}
	}
	if (table->count == table->capacity) {
		if (table->capacity > UINT32_MAX / 2)
			return 0;
		uint32_t capacity = table->capacity ? table->capacity * 2 : 8;
		const tstring **items = (const tstring **)realloc(
			table->items, capacity * sizeof(*items));
		if (!items)
			return 0;
		table->items = items;
		table->capacity = capacity;
	}
	*id = table->count;
	table->items[table->count++] = value;
	return 1;
}

static int tapc_source_loc_equal(const tsource_loc *left,
				 const tsource_loc *right)
{
	if (left->line != right->line || left->column != right->column)
		return 0;
	if ((left->source == nullptr) != (right->source == nullptr) ||
	    (left->file == nullptr) != (right->file == nullptr))
		return 0;
	return (!left->source || tstring_eq(left->source, right->source)) &&
		(!left->file || tstring_eq(left->file, right->file));
}

static int tapc_write_source_map(FILE *f, const twrapper *wrapper)
{
	uint64_t magic = TAPC_SOURCE_MAP_MAGIC;
	uint_cmds count = wrapper->source_locs ? wrapper->ncmds : 0;
	uint_cmds run_count = 0;
	tapc_string_table strings = { 0 };
	for (uint_cmds i = 0; i < count; i++) {
		uint32_t ignored;
		if (!tapc_string_table_id(
				&strings, wrapper->source_locs[i].source, &ignored) ||
		    !tapc_string_table_id(
				&strings, wrapper->source_locs[i].file, &ignored)) {
			free(strings.items);
			return 0;
		}
		if (i == 0 || !tapc_source_loc_equal(
				&wrapper->source_locs[i - 1],
				&wrapper->source_locs[i]))
			run_count++;
	}
	int ok = fwrite(&magic, sizeof(magic), 1, f) == 1 &&
		fwrite(&count, sizeof(count), 1, f) == 1 &&
		fwrite(&strings.count, sizeof(strings.count), 1, f) == 1 &&
		fwrite(&run_count, sizeof(run_count), 1, f) == 1;
	for (uint32_t i = 0; ok && i < strings.count; i++)
		ok = tapc_write_tstring(f, strings.items[i]);
	for (uint_cmds begin = 0; ok && begin < count;) {
		uint_cmds end = begin + 1;
		while (end < count && tapc_source_loc_equal(
				&wrapper->source_locs[begin],
				&wrapper->source_locs[end]))
			end++;
		uint_cmds length = end - begin;
		uint32_t source_id;
		uint32_t file_id;
		ok = tapc_string_table_id(
			&strings, wrapper->source_locs[begin].source, &source_id) &&
			tapc_string_table_id(
				&strings, wrapper->source_locs[begin].file, &file_id) &&
			fwrite(&length, sizeof(length), 1, f) == 1 &&
			fwrite(&source_id, sizeof(source_id), 1, f) == 1 &&
			fwrite(&file_id, sizeof(file_id), 1, f) == 1 &&
			fwrite(&wrapper->source_locs[begin].line,
				sizeof(wrapper->source_locs[begin].line), 1, f) == 1 &&
			fwrite(&wrapper->source_locs[begin].column,
				sizeof(wrapper->source_locs[begin].column), 1, f) == 1;
		begin = end;
	}
	free(strings.items);
	return ok;
}

static void tapc_string_array_free(tstring **strings, uint32_t count)
{
	if (!strings)
		return;
	for (uint32_t i = 0; i < count; i++)
		tstring_free(strings[i]);
	free(strings);
}

static int tapc_read_source_map(FILE *f, twrapper *wrapper)
{
	uint64_t magic = 0;
	uint_cmds count = 0;
	uint32_t string_count = 0;
	uint_cmds run_count = 0;
	if (fread(&magic, sizeof(magic), 1, f) != 1 ||
	    magic != TAPC_SOURCE_MAP_MAGIC ||
	    fread(&count, sizeof(count), 1, f) != 1 ||
	    fread(&string_count, sizeof(string_count), 1, f) != 1 ||
	    fread(&run_count, sizeof(run_count), 1, f) != 1)
		return 0;
	if (count != 0 && count != wrapper->ncmds)
		return 0;
	if (count == 0)
		return string_count == 0 && run_count == 0;
	if ((uint64_t)string_count > (uint64_t)count * 2 ||
	    run_count == 0 || run_count > count)
		return 0;
	tstring **strings = string_count ?
		(tstring **)calloc(string_count, sizeof(*strings)) : nullptr;
	if (string_count && !strings)
		return 0;
	for (uint32_t i = 0; i < string_count; i++) {
		if (!tapc_read_tstring(f, &strings[i]) || !strings[i]) {
			tapc_string_array_free(strings, string_count);
			return 0;
		}
	}
	wrapper->source_locs =
		(tsource_loc *)calloc(wrapper->ncmds, sizeof(tsource_loc));
	if (!wrapper->source_locs) {
		tapc_string_array_free(strings, string_count);
		return 0;
	}
	uint_cmds cursor = 0;
	for (uint_cmds run = 0; run < run_count; run++) {
		uint_cmds length = 0;
		uint32_t source_id = 0;
		uint32_t file_id = 0;
		uint64_t line = 0;
		uint64_t column = 0;
		if (fread(&length, sizeof(length), 1, f) != 1 ||
		    fread(&source_id, sizeof(source_id), 1, f) != 1 ||
		    fread(&file_id, sizeof(file_id), 1, f) != 1 ||
		    fread(&line, sizeof(line), 1, f) != 1 ||
		    fread(&column, sizeof(column), 1, f) != 1 ||
		    length == 0 || length > count - cursor ||
		    (source_id != TAPC_NO_STRING && source_id >= string_count) ||
		    (file_id != TAPC_NO_STRING && file_id >= string_count))
			goto load_loc_err;
		for (uint_cmds i = 0; i < length; i++) {
			tsource_loc *loc = &wrapper->source_locs[cursor++];
			loc->line = line;
			loc->column = column;
			loc->source = source_id == TAPC_NO_STRING ? nullptr :
				tstring_dup(strings[source_id]);
			loc->file = file_id == TAPC_NO_STRING ? nullptr :
				tstring_dup(strings[file_id]);
			if ((source_id != TAPC_NO_STRING && !loc->source) ||
			    (file_id != TAPC_NO_STRING && !loc->file))
				goto load_loc_err;
		}
	}
	if (cursor != count)
		goto load_loc_err;
	tapc_string_array_free(strings, string_count);
	return 1;

load_loc_err:
	tapc_string_array_free(strings, string_count);
	tsource_loc_array_clear(wrapper->source_locs, wrapper->ncmds);
	wrapper->source_locs = nullptr;
	return 0;
}

/**
 * Save wrapper to binary file (.tapc format).
 * Returns 0 on success, -1 on error.
 */
int tanalyser_save_bin_file(const twrapper *wrapper, const char *filename)
{
	FILE *f = fopen(filename, "wb");
	uint_csts i;

	if (!f) {
		twarn(ErrSession_IO, "tanalyser_save_bin_file", filename);
		return -1;
	}

	if (!tapc_write_header(f, wrapper)) {
		fclose(f);
		twarn(ErrSession_IO, "tanalyser_save_bin_file", filename);
		return -1;
	}

	/* Write cmdarr */
	fwrite(wrapper->cmdarr, sizeof(tbycode), wrapper->ncmds, f);

	/* Write cints */
	if (wrapper->consts.ncints > 0)
		fwrite(wrapper->consts.cints,
			   sizeof(long),
			   wrapper->consts.ncints,
			   f);

	/* Write cflts */
	if (wrapper->consts.ncflts > 0)
		fwrite(wrapper->consts.cflts,
			   sizeof(double),
			   wrapper->consts.ncflts,
			   f);

	/* Write cstrs */
	for (i = 0; i < wrapper->consts.ncstrs; i++) {
		tstring *ts = wrapper->consts.cstrs[i];
		uint64_t len_i = tstring_len(ts);
		fwrite(&len_i, sizeof(uint64_t), 1, f);
		fwrite(tstring_cstr(ts), 1, len_i + 1, f);
	}

	int source_map_saved = tapc_write_source_map(f, wrapper);
	int close_result = fclose(f);
	if (!source_map_saved || close_result != 0) {
		twarn(ErrSession_IO, "tanalyser_save_bin_file", filename);
		return -1;
	}
	return 0;
}

/**
 * Load wrapper from binary file (.tapc format).
 * Returns twrapper* on success, nullptr on error.
 */
static twrapper *tapc_load_bin_file(const char *filename, int report_errors)
{
	FILE *f = fopen(filename, "rb");
	twrapper *wrapper;
	uint_csts i;

	if (!f) {
		if (report_errors)
			twarn(ErrSession_IO, "tanalyser_load_bin_file", filename);
		return nullptr;
	}

	wrapper = (twrapper *)calloc(1, sizeof(twrapper));
	if (!wrapper) {
		fclose(f);
		return nullptr;
	}

	if (!tapc_read_header(f, wrapper)) {
		if (report_errors)
			twarn(ErrSession_IO, "tanalyser_load_bin_file",
			      "unsupported or truncated bytecode file");
		free(wrapper);
		fclose(f);
		return nullptr;
	}
	wrapper->source_locs = nullptr;

	/* Read cmdarr */
	{
		uint_cmds cmdlen = wrapper->ncmds;
		tbycode *cmdarr = (tbycode *)calloc(cmdlen, sizeof(tbycode));
		if (!cmdarr
		 || cmdlen != fread(cmdarr, sizeof(tbycode), cmdlen, f)) {
			free(cmdarr);
			free(wrapper);
			fclose(f);
			return nullptr;
		}
		wrapper->cmdarr = cmdarr;
		for (uint_cmds instruction = 0; instruction < cmdlen; instruction++) {
			if (tbycode_ins(cmdarr[instruction]) >= OP_COUNT) {
				free(cmdarr);
				free(wrapper);
				fclose(f);
				if (report_errors)
					twarn(ErrSession_IO, "tanalyser_load_bin_file",
					      "invalid bytecode instruction");
				return nullptr;
			}
		}
	}

	/* Read cints */
	if (wrapper->consts.ncints > 0) {
		uint_csts ncints = wrapper->consts.ncints;
		long *cints = (long *)calloc(ncints, sizeof(long));
		if (!cints || ncints != fread(cints, sizeof(long), ncints, f)) {
			free(cints);
			free(wrapper->cmdarr);
			free(wrapper);
			fclose(f);
			return nullptr;
		}
		wrapper->consts.cints = cints;
	}

	/* Read cflts */
	if (wrapper->consts.ncflts > 0) {
		uint_csts ncflts = wrapper->consts.ncflts;
		double *cflts = (double *)calloc(ncflts, sizeof(double));
		if (!cflts ||
			ncflts != fread(cflts, sizeof(double), ncflts, f)) {
			free(cflts);
			free(wrapper->consts.cints);
			free(wrapper->cmdarr);
			free(wrapper);
			fclose(f);
			return nullptr;
		}
		wrapper->consts.cflts = cflts;
	}

	/* Read cstrs */
	if (wrapper->consts.ncstrs > 0) {
		uint_csts ncstrs = wrapper->consts.ncstrs;
		wrapper->consts.cstrs = (tstring **)calloc(ncstrs, sizeof(tstring *));
		if (!wrapper->consts.cstrs) {
			free(wrapper->consts.cflts);
			free(wrapper->consts.cints);
			free(wrapper->cmdarr);
			free(wrapper);
			fclose(f);
			return nullptr;
		}

		for (i = 0; i < ncstrs; i++) {
			uint64_t len_i = 0;
			if (1 != fread(&len_i, sizeof(uint64_t), 1, f))
				goto load_str_err;
			char *raw = (char *)calloc((size_t)(len_i + 1), 1);
			if (!raw)
				goto load_str_err;
			if (len_i + 1 != fread(raw, 1, len_i + 1, f)) {
				free(raw);
				goto load_str_err;
			}
			wrapper->consts.cstrs[i] = tstring_new(raw);
			free(raw);
		}
		goto load_done;

	load_str_err:
		for (; i > 0;) {
			i--;
			tstring_free(wrapper->consts.cstrs[i]);
		}
		free(wrapper->consts.cstrs);
		free(wrapper->consts.cflts);
		free(wrapper->consts.cints);
		free(wrapper->cmdarr);
		free(wrapper);
		fclose(f);
		return nullptr;
	}
load_done:;

	/* Interned string objects */
	if (wrapper->consts.ncstrs > 0) {
		uint_csts ncstrs = wrapper->consts.ncstrs;
		wrapper->consts.csobjs = (tstr **)calloc(ncstrs, sizeof(tstr *));
		if (wrapper->consts.csobjs) {
			for (i = 0; i < ncstrs; i++) {
				wrapper->consts.csobjs[i] = tstr_new_interned(
					tstring_cstr(wrapper->consts.cstrs[i]));
				if (!wrapper->consts.csobjs[i])
					break;
			}
		}
		if (!wrapper->consts.csobjs || i < ncstrs) {
			tanalyser_clean_wrapper(wrapper);
			fclose(f);
			return nullptr;
		}
	}

	if (!tapc_read_source_map(f, wrapper)) {
		tanalyser_clean_wrapper(wrapper);
		fclose(f);
		if (report_errors)
			twarn(ErrSession_IO, "tanalyser_load_bin_file",
			      "invalid or truncated source map");
		return nullptr;
	}

	fclose(f);
	return wrapper;
}

twrapper *tanalyser_load_bin_file(const char *filename)
{
	return tapc_load_bin_file(filename, 1);
}

twrapper *tanalyser_try_load_bin_file(const char *filename)
{
	return tapc_load_bin_file(filename, 0);
}

/**
 * Release wrapper memory.
 */
void tanalyser_clean_wrapper(twrapper *wrapper)
{
	uint_csts i;
	if (!wrapper)
		return;
	free(wrapper->cmdarr);
	tsource_loc_array_clear(wrapper->source_locs, wrapper->ncmds);
	if (wrapper->consts.ncflts > 0)
		free(wrapper->consts.cflts);
	if (wrapper->consts.ncints > 0)
		free(wrapper->consts.cints);
	if (wrapper->consts.ncstrs > 0) {
		for (i = 0; i < wrapper->consts.ncstrs; i++)
			tstring_free(wrapper->consts.cstrs[i]);
		free(wrapper->consts.cstrs);
		if (wrapper->consts.csobjs) {
			for (i = 0; i < wrapper->consts.ncstrs; i++) {
				tstr *shared = wrapper->consts.csobjs[i];
				if (!shared)
					continue;
				if (shared->base.refctr > 0)
					shared->base.refctr--;
				if (shared->base.refctr == 0)
					shared->base.vtable->free(shared);
			}
			free(wrapper->consts.csobjs);
		}
	}
	free(wrapper);
}

/**
 * Display wrapper contents (debug).
 */
void tanalyser_display_wrapper(const twrapper *wrapper)
{
	uint_cmds i;
	char buf[128];
	for (i = 0; i < wrapper->ncmds; i++) {
		tbycode_tostring(wrapper->cmdarr[i], buf, sizeof(buf));
		printf("[%u]%s\n", (unsigned)i, buf);
	}
	printf("Max Obj. Number: %u\n", (unsigned)wrapper->info.obj_max);
	printf("Max Tmp. Number: %u\n", (unsigned)wrapper->info.tmp_max);
	printf("Max Reg. Number: %u\n", (unsigned)wrapper->info.reg_max);
	printf("Const Value List (Integers): ");
	for (i = 0; i < wrapper->consts.ncints; i++) {
		printf("%li", wrapper->consts.cints[i]);
		if (i < wrapper->consts.ncints - 1)
			printf(", ");
	}
	printf("\n");
	printf("Const Value List (Double Floats): ");
	for (i = 0; i < wrapper->consts.ncflts; i++) {
		printf("%f", wrapper->consts.cflts[i]);
		if (i < wrapper->consts.ncflts - 1)
			printf(", ");
	}
	printf("\n");
	printf("Const Value List (Character Strings): ");
	for (i = 0; i < wrapper->consts.ncstrs; i++) {
		printf("%s", tstring_cstr(wrapper->consts.cstrs[i]));
		if (i < wrapper->consts.ncstrs - 1)
			printf(", ");
	}
	printf("\n");
}
