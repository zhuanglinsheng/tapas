#ifndef TAPAS_TVM_H
#define TAPAS_TVM_H

#include "tapas/compile/compiler.h"

#ifdef __cplusplus
extern "C" {
#endif


/*===========================================================================*
 * 1. Binary Operation Function Type
 *===========================================================================*/

typedef void (*binopf)(const tobj *v1, const tobj *v2, tobj *vre);

typedef struct {
	long  pos;
	tobj *iterator_slot;
	uint8_t iterator_kind;
} tloop_state;

typedef struct tvm_code_cache tvm_code_cache;

typedef struct {
	tfunc     *func;
	tcompo_env env;
	tobj_array tmps;
	tobj_array tail_args;
	uint_regs  regcap;
	int        initialized;

	tobj_array saved_tmps;
	tobj      *saved_stk;
	uint_regs  saved_regmax;
	uint_regs  saved_stklen;
	tloop_state *loop_states;
	uint_cmds   loop_state_len;
	uint_cmds   loop_state_cap;
	tloop_state *saved_loop_states;
	uint_cmds   saved_loop_state_len;
	uint_cmds   saved_loop_state_cap;

	uint_cmds  return_pc;
	uint_cmds  return_end;
	uint_regs  return_stack_values;
	tcompo_env *return_env;
	twrapper   *return_wrapper;
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
	tvm_code_cache *code_caches;
	uint32_t       code_cache_len;
	uint32_t       code_cache_cap;
	tcall_frame **frames;
	uint32_t     frame_len;
	uint32_t     frame_cap;
	tsource_loc  *error_source_locs;
	uint_cmds     error_source_loc_count;
	uint_cmds    *error_instruction;
	uint32_t      execution_depth;
} tvm;

void tvm_init(tvm *vm, uint_objs tmpmax);
void tvm_set_tmpmax(tvm *vm, uint_objs m);
void tvm_set_vmstack(tvm *vm, tobj *s, uint_regs n);
tobj *tvm_get_vre(tvm *vm);
void tvm_set_rev_empty(tvm *vm);
void tvm_clean(tvm *vm);


/*===========================================================================*
 * 3. Temp Object Management
 *===========================================================================*/

tobj *tmp_obj(tvm *vm, uint_objs loc);
void tmp_add(tvm *vm);
void tmp_del(tvm *vm, uint_objs n);


/*===========================================================================*
 * 4. VM Operations — cstrlsts now uses tstring
 *===========================================================================*/

void vm_eval(tvm *vm, tbycode *iter, tcompo_env *env);
void vm_import(tvm *vm, uint_csts cloc, tstring **cstrlsts, tcompo_env *env);
void vm_binop(tvm *vm, binopf f, tbycode *iter, uint_regs type, tcompo_env *env);


/*===========================================================================*
 * 5. Instruction Execution
 *===========================================================================*/

void exec_tins(tvm *vm, uint_cmds from, uint_cmds ncmds, tcompo_env *env);
void eval_bycodes(tvm *vm, uint_cmds from, tlib *lib);


/*===========================================================================*
 * 6. Built-in C Functions Registration
 *===========================================================================*/

void register_cppfuncs(tlib *lib);

#ifdef __cplusplus
}
#endif

#endif /* TAPAS_TVM_H */
