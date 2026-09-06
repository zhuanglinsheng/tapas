/**
 * @file trule.h
 * @brief 声明核心 Rule、RuleInstance 与内建 Rule 可调用对象。
 *
 * @details 定义 Rule 的运行时布局、参数绑定、闭包捕获、派生规则，以及内建 Rule
 * 可调用值的通用表示。
 *
 * @note 面向包的 Rule 检查、哈希和持久化接口不属于本核心对象头文件。
 */
#ifndef TAPAS_OBJECTS_TRULE_H
#define TAPAS_OBJECTS_TRULE_H

#include "tapas/dsa/tobj_vec.h"
#include "tapas/tval.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 对象类别 */

/** 内建 Rule 可调用对象支持的操作类别。 */
typedef enum {
	trule_builtin_assert /**< 断言 Rule 检查结果为真。 */
} trule_builtin_kind;

/* 对象布局 */

/** Rule 对象的运行时布局。 */
struct trule {
	tcompo_v base;      /**< 引用对象共有的对象头。 */
	uint64_t identity;  /**< 当前进程内稳定的 Rule 身份。 */
	/** 非零时直接求值 RuleIR；源码 Rule 仍可保留 checker 以解析捕获值。 */
	int evaluate_ir;
	tobj checker;       /**< Rule 持有的源码检查函数。 */
	tstring *source;    /**< Rule 持有的源码文本。 */
	tstring *signature; /**< Rule 持有的签名文本。 */
	trule_ir *ir;       /**< Rule 持有的结构化执行数据。 */
};

/** Rule 与实参绑定后的实例对象。 */
struct trule_instance {
	tcompo_v base;      /**< 引用对象共有的对象头。 */
	tobj rule;          /**< 实例持有的 Rule。 */
	tobj_vec arguments; /**< 实例持有的实参数组。 */
};

/** 内建 Rule 可调用对象。 */
struct trule_builtin {
	tcompo_v base;                /**< 引用对象共有的对象头。 */
	trule_builtin_kind kind;      /**< 内建操作类别。 */
	tfunction_metadata *metadata; /**< 函数元数据。 */
};

/* 共享 vtable */

/** Rule 对象使用的共享 vtable。 */
extern tcompo_vtable trule_vtable;

/** RuleInstance 对象使用的共享 vtable。 */
extern tcompo_vtable trule_instance_vtable;

/** 内建 Rule 可调用对象使用的共享 vtable。 */
extern tcompo_vtable trule_builtin_vtable;

/* 构造 */

/**
 * @brief 根据源码检查函数及编译期元数据创建 Rule。
 *
 * @param checker 执行检查的 Tapas 函数。
 * @param source Rule 源码。
 * @param signature Rule 签名。
 * @param parameter_names 编码后的参数名称。
 * @param item_metadata 编码后的 Item 元数据。
 * @param capture_metadata 编码后的捕获元数据。
 * @return 新 Rule，由 Tapas 引用计数管理。
 */
trule *trule_new(tfunc *checker, const char *source, const char *signature,
		 const char *parameter_names, const char *item_metadata,
		 const char *capture_metadata);

/**
 * @brief 根据独立 RuleIR 创建动态 Rule。
 *
 * @param ir Rule 的结构化执行数据，构造结果取得一个引用。
 * @param signature Rule 签名。
 * @return 新 Rule，由 Tapas 引用计数管理。
 */
trule *trule_new_dynamic(trule_ir *ir, const char *signature);

/**
 * @brief 根据已有 Rule 和替换后的 RuleIR 创建派生 Rule。
 *
 * @param source 提供源码、签名和捕获上下文的原 Rule。
 * @param ir 派生后的 RuleIR。
 * @return 新 Rule，由 Tapas 引用计数管理。
 */
trule *trule_new_derived(const trule *source, trule_ir *ir);

/* 能力 */

/**
 * @brief 从当前闭包读取声明的捕获值，而不调用检查函数。
 *
 * @param rule Rule。
 * @param capture 描述捕获槽位的 RuleTerm。
 * @param result 接收捕获值。
 * @return 成功读取返回非零，否则返回零。
 */
int trule_read_capture(const trule *rule, const trule_term *capture, tobj *result);

/**
 * @brief 让 Rule 捕获指定执行环境中声明的自由变量。
 *
 * @param rule 要闭合的 Rule。
 * @param environment 提供捕获值的当前执行环境。
 */
void trule_close_over(trule *rule, tcompo_env *environment);

/**
 * @brief 将实参数组绑定到 Rule，并创建 RuleInstance。
 *
 * @param rule 要绑定的 Rule。
 * @param arguments 实参数组。
 * @param argument_count 实参数量。
 * @return 新 RuleInstance，由 Tapas 引用计数管理。
 */
trule_instance *trule_bind(trule *rule, const tobj *arguments,
			   uint_regs argument_count);

/* 内建 Rule 可调用对象 */

/**
 * @brief 创建指定类别的内建 Rule 可调用对象。
 *
 * @param kind 内建操作类别。
 * @return 新对象，由 Tapas 引用计数管理。
 */
trule_builtin *trule_builtin_new(trule_builtin_kind kind);

#ifdef __cplusplus
}
#endif

#endif
