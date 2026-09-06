#ifndef TAPAS_TBASIS_H
#define TAPAS_TBASIS_H

#if !defined(__cplusplus) && (!defined(__STDC_VERSION__) || __STDC_VERSION__ < 202311L)
#error "Tapas requires an ISO C23 compiler"
#endif

/* ctypes */
#include <math.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>

/* cpp stl replacements */
#include <stdlib.h>
#include <string.h>

#include "tapas/ds/tstring.h"
#include "tapas/version.h"

#ifdef __cplusplus
extern "C" {
#endif


/*===========================================================================*
 * Optimization Flags
 *===========================================================================*/

#define Tap_Static_OPT
#define Tap_Runtime_OPT


/*===========================================================================*
 * Limits
 *===========================================================================*/

#define Tap_Long_MAX (2e31 - 1)


/*===========================================================================*
 * Type Aliases
 *===========================================================================*/

typedef uint64_t uint_lexs;
typedef int64_t int_lexs;

typedef uint32_t uint_cmds;

typedef uint32_t uint_csts;

typedef uint16_t uint_objs;

typedef uint8_t uint_regs;


/*===========================================================================*
 * System Limits
 *===========================================================================*/

#define Limit_U ((uint32_t)67108863)
#define Limit_C ((uint32_t)262143)
#define Limit_L ((uint16_t)8191)
#define Limit_P ((uint8_t)255)

#define CMD_LIMIT Limit_U
#define CST_LIMIT Limit_C
#define OBJ_LIMIT Limit_L
#define REG_LIMIT Limit_P


/*===========================================================================*
 * Special Markers
 *===========================================================================*/

#define UNDEF_NPARAMS REG_LIMIT
#define UNDEF_NAMELOC CST_LIMIT
#define UNDEF_ENVLOC OBJ_LIMIT

/*===========================================================================*
 * OP_PUSHX Address Encoding
 *===========================================================================*/

/* OP_PUSHX keeps L as the slot/location. R is packed as:
 *   bit 0    : isenv    (0 = tmp, 1 = env)
 *   bit 1    : is_upval (0 = local env, 1 = outer env)
 *   bit 2-12 : depth    (number of father_env hops for upval)
 *
 * Common R values:
 *   0  -> tmp[L]
 *   1  -> current env slot L
 *   7  -> father env depth 1, slot L
 *   11 -> father env depth 2, slot L
 */
static inline uint16_t tpushx_tmp_addr(void)
{
	return 0;
}

static inline uint16_t tpushx_local_addr(void)
{
	return 1;
}

static inline uint16_t tpushx_upval_addr(uint16_t depth)
{
	return (uint16_t)(1u | 2u | ((uint16_t)depth << 2));
}

static inline int tpushx_isenv(uint16_t r)
{
	return (int)(r & 1u);
}

static inline int tpushx_is_upval(uint16_t r)
{
	return (int)((r >> 1) & 1u);
}

static inline uint16_t tpushx_depth(uint16_t r)
{
	return (uint16_t)(r >> 2);
}


/*===========================================================================*
 * Bytecode Instructions
 *===========================================================================*/

typedef enum {
	OP_PASS,
	OP_VCRT,
	OP_TMPDEL,
	OP_THIS,
	OP_BASE,
	OP_RET,
	OP_IN,
	OP_PAIR,
	OP_TO,
	OP_POPN,
	OP_POPCOV,
	OP_TYPEFWD,
	OP_TYPEDEFINE,
	OP_LOOPAS,
	OP_JPF,
	OP_JPB,
	OP_CJPFPOP,
	OP_CJPBPOP,
	OP_PUSHX,
	OP_PUSHI,
	OP_PUSHFLT,
	OP_PUSHB,
	OP_PUSHS,
	OP_PUSHDICT,
	OP_PUSHINFO,
	OP_IMPORT,
	OP_IDXR,
	OP_EVAL,
	OP_EVALSF,
	OP_EVALCF,
	OP_EVALTF,
	OP_IDXL,
	OP_PUSHF,
	OP_PUSHRULE,
	OP_RULECOND,
	OP_RULEREQ,
	OP_ADD,
	OP_SUB,
	OP_MUL,
	OP_DIV,
	OP_MOD,
	OP_POW,
	OP_MMUL,
	OP_EQ,
	OP_NE,
	OP_GE,
	OP_SG,
	OP_LE,
	OP_SL,
	OP_AND,
	OP_OR,
	OP_POS,
	OP_NEG,
	OP_BAND,
	OP_BOR,
	OP_FUNCMETA,
	OP_BINDTYPE,
	OP_CHECKTYPE,
	OP_RULETYPE,
	OP_RULENOT,
	OP_RULETRUTH,
	OP_RULEITEM,
	OP_RULEVALUE,
	OP_COUNT
} tins;


/*===========================================================================*
 * Value Type Codes
 *===========================================================================*/

typedef enum {
	tnil    = 0,
	tbool   = 1,
	tint    = 2,
	tfloat  = 3,
	tcompo  = 4
} ttypes;


/*===========================================================================*
 * Composite Type Codes
 *===========================================================================*/

typedef enum {
	compo_tstr     = 0,
	compo_tlist    = 1,
	compo_tpair    = 2,
	compo_tdict    = 3,
	compo_tfunc    = 4,
	compo_tlib     = 5,
	compo_titer    = 6,
	compo_tbarr    = 7,
	compo_tdarr    = 8,
	compo_tarr     = 9,
	compo_cppfunc  = 10,
	compo_sessfunc = 11,
	compo_time     = 12,
	compo_ttypeval = 13,
	compo_trule    = 14,
	compo_trule_instance = 15,
	compo_trule_builtin = 16,
	compo_tevaluator = 17,
	compo_trule_ir = 18,
	compo_trule_term = 19,
	compo_trule_item = 20,
	compo_tpoints = 21,
	compo_trange = 22
} tcompo_type;


/*===========================================================================*
 * Error Types
 *===========================================================================*/

typedef enum {
	ErrCompile_Other,
	ErrCompile_UnfoundFile,
	ErrCompile_BracketsOpen,
	ErrCompile_VarNoType,
	ErrCompile_DblVDeclare,
	ErrCompile_InBlkVarDef,
	ErrCompile_ObjUnfound,
	ErrCompile_InvalidVname,
	ErrCompile_InvalidLiter,
	ErrCompile_AsgDefault,
	ErrCompile_REGOutOfLimit,
	ErrCompile_CMDOutOfLimit,
	ErrCompile_OBJOutOfLimit,
	ErrCompile_CSTOutOfLimit,
	ErrCompile_ReturnTmpObj,
	ErrCompile_InvalidFile,

	ErrSession_IO,

	ErrRuntime_Other,
	ErrRuntime_DivIntZero,
	ErrRuntime_ParamsCtr,
	ErrRuntime_ParamsType,
	ErrRuntime_IdxOutRange,
	ErrRuntime_InvalidIndex,
	ErrRuntime_LoopRef,
	ErrRuntime_RefType,
	ErrRuntime_LenInconsis,
	ErrRuntime_AssignNil,
	ErrRuntime_ObjUnfound,
	ErrRuntime_IntOutOfRange,
	ErrRuntime_RefEmptySet,
	ErrRuntime_StringEval,
	ErrRuntime_EnvInconsis,
	ErrRuntime_RecurseRefRet
} terror_type;

/*===========================================================================*
 * Error System — declarations
 *===========================================================================*/

typedef enum {
	TErrorPhase_Compile,
	TErrorPhase_Runtime,
	TErrorPhase_Session,
	TErrorPhase_Unknown
} terror_phase;

typedef struct {
	terror_type type;
	terror_phase phase;
	const char *reason;
	tstring *where;
	tstring *detail;
	tstring *source;
	tstring *file;
	uint64_t line;
	uint64_t column;
	uint_cmds instruction;
	int has_location;
	int has_instruction;
} terror;

const char *terror_type_name(terror_type t);
const char *terror_phase_name(terror_phase p);
const terror *terror_last(void);
void terror_clear_last(void);
void terror_set_source_context(const char *source);
void terror_set_source_context_borrowed(const char *source);
void terror_clear_source_context(void);
const char *terror_current_source_context(void);
void terror_set_file_context(const char *file, uint64_t line, uint64_t column);
void terror_set_file_context_borrowed(const char *file, uint64_t line, uint64_t column);
void terror_clear_file_context(void);
const char *terror_current_file_context(void);
uint64_t terror_current_line_context(void);
uint64_t terror_current_column_context(void);
void terror_set_instruction_context(uint_cmds instruction);
void terror_clear_instruction_context(void);
typedef void (*terror_runtime_context_resolver)(
	void *context, const char **source, const char **file,
	uint64_t *line, uint64_t *column, uint_cmds *instruction);

typedef struct {
	terror_runtime_context_resolver resolver;
	void *context;
} terror_runtime_context_state;

/* Install a lazy runtime source resolver and return the previous resolver.
 * The resolver is called only if an error is raised. */
terror_runtime_context_state terror_set_runtime_context_resolver(
	terror_runtime_context_resolver resolver, void *context);
void terror_restore_runtime_context_resolver(
	terror_runtime_context_state state);
void twarn(terror_type type, const char *fname, const char *info);

/* Optional error recovery hook used by the interactive CLI. */
extern jmp_buf tapas_error_jmpbuf;
extern int tapas_error_recover_enabled;

#ifdef __cplusplus
}
#endif

#endif /* TAPAS_TBASIS_H */
