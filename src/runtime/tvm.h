/**
 * @file tvm.h
 * @brief Declares the concrete Tapas virtual-machine execution interface.
 * @details Defines VM frames, execution state, and bytecode evaluation entry
 * points used by the core implementation.
 * @note Packages must call the narrow interface in `tapas/vm_service.h` and
 * must not depend on this concrete VM representation.
 */
#ifndef TAPAS_TVM_H
#define TAPAS_TVM_H

#include "tenv.h"

#ifdef __cplusplus
extern "C" {
#endif


typedef struct {
	long  pos;
	long  range_start;
	long  range_step;
	long  range_end;
	tobj *iterator_slot;
} tloop_state;

typedef struct tvm_code_cache tvm_code_cache;

typedef struct {
	uint32_t antecedent_depth;
	tlist *output;
	tlist *logic_values;
} trule_eval_context;

typedef struct {
	tfunc       *func;
	tcompo_env   env;
	tobj_array   tmps;
	tobj_array   tail_args;
	uint_regs    regcap;
	int          initialized;
	/* 被调函数是否含循环指令。在入栈时判定并缓存：函数可能在其自身运行
	 * 期间被释放（持有它的环境槽位被改写），因此出栈时不能再解引用
	 * func 去重新判定。 */
	int          callee_has_loops;

	tobj_array   saved_tmps;
	tobj        *saved_stk;
	uint_regs    saved_regmax;
	uint_regs    saved_stklen;
	tloop_state *loop_states;
	uint_cmds    loop_state_len;
	uint_cmds    loop_state_cap;
	tloop_state *saved_loop_states;
	uint_cmds    saved_loop_state_len;
	uint_cmds    saved_loop_state_cap;

	uint_cmds    return_pc;
	uint_cmds    return_end;
	uint_regs    return_stack_values;
	int          return_has_callable;
	tobj         retained_callable;
	tcompo_env  *return_env;
	twrapper    *return_wrapper;
} tcall_frame;


/*===========================================================================*
 * 1. Virtual Machine (tvm)
 *===========================================================================*/

typedef struct tvm {
	tobj_array       tmps;
	uint_regs        regmax;
	tobj            *stk;
	uint_regs        stklen;
	tobj             rev;
	tloop_state     *loop_states;
	uint_cmds        loop_state_len;
	uint_cmds        loop_state_cap;
	tvm_code_cache  *code_caches;
	uint32_t         code_cache_len;
	uint32_t         code_cache_cap;
	tcall_frame    **frames;
	uint32_t         frame_len;
	uint32_t         frame_cap;
	tsource_loc     *error_source_locs;
	uint_cmds        error_source_loc_count;
	uint_cmds       *error_instruction;
	uint32_t         execution_depth;
	trule_eval_context rule;
} tvm;

void tvm_init(tvm *vm, uint_objs tmpmax);
void tvm_set_tmpmax(tvm *vm, uint_objs m);
void tvm_set_vmstack(tvm *vm, tobj *s, uint_regs n);
/** Transfer the current result out of the VM and leave its result slot Nil. */
tobj tvm_take_result(tvm *vm);
/**
 * Clear transient execution state while retaining top-level temporaries.
 * REPL and Markdown sessions use those slots as bindings across code blocks.
 */
void tvm_reset(tvm *vm);
/** Release all VM-owned storage. The VM must be initialized before reuse. */
void tvm_destroy(tvm *vm);


/*===========================================================================*
 * 2. Temp Object Management
 *===========================================================================*/

tobj *tmp_obj(tvm *vm, uint_objs loc);
void tmp_add(tvm *vm);
void tmp_del(tvm *vm, uint_objs n);


/*===========================================================================*
 * 3. VM Operations — cstrlsts now uses tstring
 *===========================================================================*/

void vm_eval(tvm *vm, tbycode *iter, tcompo_env *env);
void tvm_call(tvm *vm, const tobj *callable, tobj *arguments,
	      uint_regs argument_count, tcompo_env *environment,
	      tobj *result);


/*===========================================================================*
 * 4. Instruction Execution
 *===========================================================================*/

void exec_tins(tvm *vm, uint_cmds from, uint_cmds ncmds, tcompo_env *env);
void eval_bycodes(tvm *vm, uint_cmds from, tlib *lib);


#ifdef __cplusplus
}
#endif

#endif /* TAPAS_TVM_H */
