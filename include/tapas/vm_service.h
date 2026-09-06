/**
 * @file vm_service.h
 * @brief 声明 VM 向原生包提供的窄服务接口。
 *
 * @details 原生会话函数需要调用核心执行能力时，通过本回调表间接访问 VM。接口
 * 不包含任何具体包的身份或对象表示，并保持 `tvm` 的具体结构仅由核心掌握。
 *
 * @note 服务表及上下文只在关联执行环境求值期间有效，包不得长期保存上下文指针。
 */
#ifndef TAPAS_VM_SERVICE_H
#define TAPAS_VM_SERVICE_H

#include "tapas/basic_defs/tbasis.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tobj tobj;             /**< Tapas 值。 */
typedef struct tcompo_env tcompo_env; /**< 一次运行时执行使用的环境。 */

/** VM 向原生包开放的核心执行能力表。 */
typedef struct tvm_services {
	/**
	 * 调用任意 Tapas 可调用值。
	 *
	 * `arguments` 由调用者持有，结果写入 `result`。
	 */
	void (*invoke)(void *context, const tobj *callable, tobj *arguments,
		uint_regs argument_count, tcompo_env *environment, tobj *result);

	/** 在给定规则实例中求值一个 RuleTerm，并把结果写入 `result`。 */
	void (*evaluate_rule_term)(void *context, const tobj *instance,
		const tobj *term, tcompo_env *environment, tobj *result);

	/** 检查规则并把检查结果写入 `result`。 */
	void (*check_rule)(void *context, const tobj *rule,
		tcompo_env *environment, tobj *result);
} tvm_services;

/**
 * @brief 为执行环境绑定 VM 服务表及其上下文。
 *
 * @param environment 要配置的执行环境。
 * @param services 服务表；其生命周期必须覆盖环境的求值过程。
 * @param context 每次调用服务回调时原样传入的不透明上下文。
 */
void tcompo_env_set_vm_services(tcompo_env *environment,
	const tvm_services *services, void *context);

/**
 * @brief 通过执行环境绑定的 VM 服务调用一个可调用值。
 *
 * @param environment 当前执行环境。
 * @param callable 要调用的 Tapas 值。
 * @param arguments 参数数组。
 * @param argument_count 参数数量。
 * @param result 接收调用结果。
 */
void tvm_services_invoke(tcompo_env *environment, const tobj *callable,
	tobj *arguments, uint_regs argument_count, tobj *result);

/**
 * @brief 通过执行环境绑定的 VM 服务求值 RuleTerm。
 *
 * @param environment 当前执行环境。
 * @param instance Rule 实例。
 * @param term 要求值的 RuleTerm。
 * @param result 接收求值结果。
 */
void tvm_services_evaluate_rule_term(tcompo_env *environment,
	const tobj *instance, const tobj *term, tobj *result);

/**
 * @brief 通过执行环境绑定的 VM 服务检查规则。
 *
 * @param environment 当前执行环境。
 * @param rule 要检查的 Rule。
 * @param result 接收检查结果。
 */
void tvm_services_check_rule(tcompo_env *environment, const tobj *rule,
	tobj *result);

#ifdef __cplusplus
}
#endif

#endif /* TAPAS_VM_SERVICE_H */
