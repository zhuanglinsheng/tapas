#ifndef TAPAS_TBYCS_H
#define TAPAS_TBYCS_H

#include "tapas/tbasis.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================*
 * 1. Single Bytecode (tbycode)
 *===========================================================================*/

typedef uint32_t tbycode;

/* ---- Constructors ---- */
tbycode tbycode_make(uint8_t ins);
tbycode tbycode_make_u(uint8_t ins, uint32_t u);
tbycode tbycode_make_lr(uint8_t ins, uint16_t L, uint16_t R);
tbycode tbycode_make_lbi(uint8_t ins, uint16_t L, uint8_t b, uint8_t i);

/* ---- Getters ---- */
static inline tins tbycode_ins(tbycode c)
{
	return (tins)(c & 0x3fU);
}

static inline uint32_t tbycode_get_U(tbycode c)
{
	return c >> 6;
}

static inline uint16_t tbycode_get_L(tbycode c)
{
	return (uint16_t)((c >> 6) & 0x1fffU);
}

static inline uint16_t tbycode_get_R(tbycode c)
{
	return (uint16_t)(c >> 19);
}

static inline uint8_t tbycode_get_b(tbycode c)
{
	return (uint8_t)((c >> 19) & 0xffU);
}

static inline uint8_t tbycode_get_i(tbycode c)
{
	return (uint8_t)(c >> 27);
}
void tbycode_tostring(tbycode c, char *buf, size_t buf_size);

/*===========================================================================*
 * 2. Dynamic Bytecode Vector (tvmcmd_vect)
 *===========================================================================*/

typedef struct {
	tstring *source;
	tstring *file;
	uint64_t line;
	uint64_t column;
} tsource_loc;

typedef struct {
	tbycode *data;
	tsource_loc *locs;
	uint32_t size;
	uint32_t capacity;
} tvmcmd_vect;

void tvmcmd_vect_init(tvmcmd_vect *v);
void tvmcmd_vect_free(tvmcmd_vect *v);
uint_cmds tvmcmd_vect_size32(tvmcmd_vect *v);
tbycode tvmcmd_vect_back(tvmcmd_vect *v);
void tvmcmd_vect_pop_back(tvmcmd_vect *v);
void tvmcmd_vect_append(tvmcmd_vect *v, tbycode cmd);
void tvmcmd_vect_insert_range(tvmcmd_vect *v, uint32_t pos, tbycode *src, uint32_t count);
void tvmcmd_vect_insert_vect(tvmcmd_vect *v, uint32_t pos, const tvmcmd_vect *src);
void tvmcmd_vect_resolve_loop_control(tvmcmd_vect *v, uint32_t begin,
				      uint32_t end, uint32_t continue_target,
				      uint32_t break_target,
				      uint8_t continue_marker,
				      uint8_t break_marker);

/*===========================================================================*
 * 3. Constant Vectors (using tstring)
 *===========================================================================*/

typedef struct {
	tstring **data;
	uint32_t size;
	uint32_t capacity;
} consts_str_vect;

void consts_str_vect_init(consts_str_vect *v);
void consts_str_vect_free(consts_str_vect *v);
uint_csts consts_str_vect_add(consts_str_vect *v, const char *str);

typedef struct {
	long *data;
	uint32_t size;
	uint32_t capacity;
} consts_long_vect;

void consts_long_vect_init(consts_long_vect *v);
void consts_long_vect_free(consts_long_vect *v);
uint_csts consts_long_vect_add(consts_long_vect *v, long val);

typedef struct {
	double *data;
	uint32_t size;
	uint32_t capacity;
} consts_float_vect;

void consts_float_vect_init(consts_float_vect *v);
void consts_float_vect_free(consts_float_vect *v);
uint_csts consts_float_vect_add(consts_float_vect *v, double val);

/** Combined constant pool */
typedef struct {
	consts_str_vect __strcsts;
	consts_long_vect __intcsts;
	consts_float_vect __fltcsts;
} tconsts;

void tconsts_init(tconsts *c);
void tconsts_free(tconsts *c);
uint_csts tconsts_add_str_const(tconsts *c, const char *str);
uint_csts tconsts_add_int_const(tconsts *c, long val);
uint_csts tconsts_add_float_const(tconsts *c, double val);
void tconsts_copy(tconsts *dst, tconsts *src);

/*===========================================================================*
 * 4. Compilation Info
 *===========================================================================*/

typedef struct {
	uint_objs obj_max;
	uint_objs tmp_max;
	uint_regs reg_max;
	uint16_t padding_1;
} tcinfo;

/*===========================================================================*
 * 5. Wrapper (serialized bytecode) — uses tstring
 *===========================================================================*/

typedef struct {
	uint_csts ncints;
	uint_csts ncstrs;
	uint_csts ncflts;
	long *cints;
	tstring **cstrs;
	double *cflts;
} twrapper_consts;

typedef struct {
	twrapper_consts consts;
	tbycode *cmdarr;
	tsource_loc *source_locs;
	tcinfo info;
	uint_cmds ncmds;
} twrapper;

/*===========================================================================*
 * 6. Analyser (wrapper management)
 *===========================================================================*/

twrapper *tanalyser_wrap(tvmcmd_vect *tcmds, tconsts *consts, tcinfo *info);
int tanalyser_save_bin_file(const twrapper *wrapper, const char *filename);
twrapper *tanalyser_load_bin_file(const char *filename);
void tanalyser_clean_wrapper(twrapper *wrapper);
void tanalyser_display_wrapper(const twrapper *wrapper);

#ifdef __cplusplus
}
#endif

#endif /* TAPAS_TBYCS_H */
