#ifndef T_VM_H
#define T_VM_H

#include "tcompile.h"

#ifdef __cplusplus
extern "C" {
#endif


/*===========================================================================*
 * 1. Binary Operation Function Type
 *===========================================================================*/

typedef void (*binopf)(const tobj *v1, const tobj *v2, tobj *vre);

typedef struct {
	uint_cmds ins_idx;
	long pos;
} tloop_state;

typedef struct {
	tfunc     *func;
	tcompo_env env;
	tobj_array tmps;
	tobj      *params;
	uint_regs  nparams;

	tobj_array saved_tmps;
	tobj      *saved_stk;
	uint_regs  saved_regmax;
	uint_regs  saved_stklen;
} tcall_frame;


/*===========================================================================*
 * 2. Virtual Machine (tvm)
 *===========================================================================*/

typedef struct tvm {
	tobj_array  tmps;
	uint_regs   regmax;
	tobj       *stk;
	uint_regs   stklen;
	tobj        rev;
	tloop_state *loop_states;
	uint_cmds   loop_state_len;
	uint_cmds   loop_state_cap;
	tcall_frame **frames;
	uint32_t     frame_len;
	uint32_t     frame_cap;
} tvm;

void tvm_init(tvm *vm, uint_objs tmpmax);
void tvm_set_tmpmax(tvm *vm, uint_objs m);
void tvm_set_vmstack(tvm *vm, tobj *s, uint_regs n);
tobj *tvm_get_vre(tvm *vm);
void tvm_set_rev_empty(tvm *vm);
void tvm_clean(tvm *vm);


/*===========================================================================*
 * 3. Stack Helpers
 *===========================================================================*/

tobj *stk_at(tvm *vm, uint_regs loc);
tobj *stk_top(tvm *vm);
tobj *stk_topn(tvm *vm, uint_regs n);
tobj *stk_free(tvm *vm);
void stk_fill(tvm *vm);
uint_regs stk_len(tvm *vm);
void stk_pop(tvm *vm);
void stk_popc(tvm *vm);
void stk_popcn(tvm *vm, uint_regs n);
void stk_push(tvm *vm, const tobj *v);


/*===========================================================================*
 * 4. Temp Object Management
 *===========================================================================*/

tobj *tmp_obj(tvm *vm, uint_objs loc);
void tmp_add(tvm *vm);
void tmp_del(tvm *vm, uint_objs n);


/*===========================================================================*
 * 5. VM Operations — cstrlsts now uses tstring
 *===========================================================================*/

void vm_idxr(tvm *vm, uint_regs nparams);
void vm_idxl(tvm *vm, uint_objs loc, uint_regs nparams, int isenv, tcompo_env *env);
void vm_loopas(tvm *vm, tbycode *iter, uint_cmds ins_idx, uint_objs idx, int isenv, tcompo_env *env);
void vm_eval(tvm *vm, tbycode *iter, tcompo_env *env);
void vm_import(tvm *vm, uint_csts cloc, tstring **cstrlsts, tcompo_env *env);
void vm_binop(tvm *vm, binopf f, tbycode *iter, uint_regs type, tcompo_env *env);
void exec_tin(tvm *vm, tbycode *iter, uint_cmds *idx, uint_cmds end, tcompo_env *env, long *cints, double *cflts, tstring **cstrs);


/*===========================================================================*
 * 6. Instruction Execution
 *===========================================================================*/

void exec_tins(tvm *vm, uint_cmds from, uint_cmds ncmds, tcompo_env *env);
void eval_bycodes(tvm *vm, uint_cmds from, tlib *lib);


/*===========================================================================*
 * 7. Built-in C Functions Registration
 *===========================================================================*/

void register_cppfuncs(tlib *lib);

#ifdef __cplusplus
}
#endif

#endif /* T_VM_H */
