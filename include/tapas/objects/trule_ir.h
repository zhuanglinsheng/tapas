/**
 * @file trule_ir.h
 * @brief 声明核心 RuleIR、RuleTerm 和 RuleItem 对象。
 *
 * @details RuleTerm 表示可求值的规则表达式，RuleItem 表示条件、要求或蕴含项，
 * RuleIR 聚合参数、捕获、Term 与 Item，构成 Rule 的结构化执行数据。
 *
 * @note RuleIR 哈希与持久化等包功能必须由所属包声明并实现，不得加入本核心头文件。
 */
#ifndef TAPAS_OBJECTS_TRULE_IR_H
#define TAPAS_OBJECTS_TRULE_IR_H

#include "tapas/dsa/tobj_vec.h"
#include "tapas/tval.h"

/** OP_RULECOND 的内部模式：记录原始前件并把真值留在栈上。 */
#define TRULE_ANTECEDENT_RECORD 0x03ffffffu

/**
 * @brief 判断 Type 是否可以作为 Rule 蕴含项的前件。
 *
 * @param type 待检查的 Type。
 * @return 可以作为前件返回非零，否则返回零。
 */
int trule_antecedent_type(const ttypeval *type);

#ifdef __cplusplus
extern "C" {
#endif

/* 对象类别 */

/** RuleTerm 表示的求值节点类别。 */
typedef enum {
	trule_term_constant,  /**< 常量值。 */
	trule_term_parameter, /**< Rule 参数。 */
	trule_term_capture,   /**< 闭包捕获值。 */
	trule_term_intrinsic, /**< 核心内建求值节点。 */
	trule_term_call,      /**< 函数调用。 */
	trule_term_construct, /**< 值构造。 */
	trule_term_convert,   /**< 类型转换。 */
	trule_term_extension, /**< 由扩展提供者解释的节点。 */
	trule_term_not,       /**< 逻辑非。 */
	trule_term_and,       /**< 逻辑与。 */
	trule_term_or,        /**< 逻辑或。 */
	trule_term_in         /**< 成员判断。 */
} trule_term_kind;

/** RuleItem 表示的规则条目类别。 */
typedef enum {
	trule_item_condition,   /**< 普通布尔条件。 */
	trule_item_requirement, /**< 对另一 Rule 的要求。 */
	trule_item_implication  /**< 前件成立时检查后件。 */
} trule_item_kind;

/* 对象布局 */

/** RuleTerm 对象的运行时布局。 */
struct trule_term {
	tcompo_v base;          /**< 引用对象共有的对象头。 */
	uint64_t id;            /**< Term 在所属 IR 中的身份。 */
	trule_term_kind kind;   /**< Term 类别。 */
	ttypeval *type;         /**< 求值结果 Type，由 Term 持有。 */
	tobj payload;           /**< 类别相关的负载值。 */
	tobj_vec arguments;     /**< 子 Term 或扩展参数。 */
	tstring *provider;      /**< 可为空的扩展提供者名称。 */
	tstring *provider_kind; /**< 可为空的扩展节点类别。 */
	long provider_version;  /**< 扩展节点协议版本。 */
	long origin_start;      /**< 源码起始偏移。 */
	long origin_end;        /**< 源码结束偏移。 */
};

/** RuleItem 对象的运行时布局。 */
struct trule_item {
	tcompo_v base;        /**< 引用对象共有的对象头。 */
	trule_item_kind kind; /**< Item 类别。 */
	trule_term *term;     /**< 条件 Term 或蕴含项前件。 */
	trule_term *rule;     /**< 要求项引用的 Rule Term。 */
	tobj_vec arguments;   /**< 后件或 Rule 实参数组。 */
	tstring *description; /**< 可为空的人类可读说明。 */
	long origin_start;    /**< 源码起始偏移。 */
	long origin_end;      /**< 源码结束偏移。 */
};

/** RuleIR 对象的运行时布局。 */
struct trule_ir {
	tcompo_v base;         /**< 引用对象共有的对象头。 */
	tstring *display_name; /**< 可为空的显示名称。 */
	tstring *source;       /**< 可为空的原始源码。 */
	long version;          /**< IR 格式版本。 */
	tobj_vec parameters;   /**< Rule 参数 Term。 */
	tobj_vec captures;     /**< 闭包捕获 Term。 */
	tobj_vec terms;        /**< IR 持有的全部 Term。 */
	tobj_vec items;        /**< IR 持有的全部 Item。 */
};

/* 共享 vtable */

/** RuleTerm 对象使用的共享 vtable。 */
extern tcompo_vtable trule_term_vtable;

/** RuleItem 对象使用的共享 vtable。 */
extern tcompo_vtable trule_item_vtable;

/** RuleIR 对象使用的共享 vtable。 */
extern tcompo_vtable trule_ir_vtable;

/* RuleTerm 构造 */

/**
 * @brief 按类别、结果 Type、负载和子项创建通用 RuleTerm。
 *
 * @param kind Term 类别。
 * @param type 求值结果 Type。
 * @param payload 类别相关负载，可为空。
 * @param arguments 子项数组，可为空。
 * @param argument_count 子项数量。
 * @return 新 RuleTerm，由 Tapas 引用计数管理。
 */
trule_term *trule_term_new(trule_term_kind kind, ttypeval *type,
			   const tobj *payload, const tobj *arguments,
			   uint_regs argument_count);

/**
 * @brief 创建逻辑与或逻辑或 Term。
 *
 * @param kind `trule_term_and` 或 `trule_term_or`。
 * @param left 左子 Term。
 * @param right 右子 Term。
 * @return 新 RuleTerm。
 */
trule_term *trule_term_logic_new(trule_term_kind kind, trule_term *left, trule_term *right);

/**
 * @brief 创建成员判断 Term。
 *
 * @param value 待判断的值 Term。
 * @param domain 提供成员范围的 Term。
 * @return 新 RuleTerm。
 */
trule_term *trule_term_in_new(trule_term *value, trule_term *domain);

/**
 * @brief 创建逻辑非 Term。
 *
 * @param operand 操作数 Term。
 * @return 新 RuleTerm。
 */
trule_term *trule_term_not_new(trule_term *operand);

/**
 * @brief 创建命名参数 Term。
 *
 * @param name 参数名称。
 * @param type 参数 Type。
 * @return 新 RuleTerm。
 */
trule_term *trule_term_parameter_new(const char *name, ttypeval *type);

/**
 * @brief 创建持有指定值的常量 Term。
 *
 * @param value 常量值。
 * @return 新 RuleTerm。
 */
trule_term *trule_term_constant_new(const tobj *value);

/**
 * @brief 创建由指定扩展提供者解释的 Term。
 *
 * @param provider 扩展提供者名称。
 * @param kind 扩展节点类别。
 * @param arguments 扩展参数数组。
 * @param argument_count 扩展参数数量。
 * @param payload 扩展负载，可为空。
 * @return 新 RuleTerm。
 */
trule_term *trule_term_extension_new(const char *provider,
			     const char *kind, const tobj *arguments,
			     uint_regs argument_count, const tobj *payload);

/* RuleItem 构造 */

/**
 * @brief 创建普通条件 Item。
 *
 * @param term Boolean 条件 Term。
 * @param description 可为空的说明文本。
 * @return 新 RuleItem。
 */
trule_item *trule_condition_new(trule_term *term, const char *description);
/* 蕴含项持有 Boolean 或 RuleInstance 前件；arguments 保存 Boolean 后件，是否
 * 满足由 evaluator 解释。 */

/**
 * @brief 创建前件成立时检查一组后件的蕴含 Item。
 *
 * @param antecedent Boolean 或 RuleInstance 前件 Term。
 * @param consequents Boolean 后件 Term 数组。
 * @param count 后件数量。
 * @param description 可为空的说明文本。
 * @return 新 RuleItem。
 */
trule_item *trule_implication_new(trule_term *antecedent,
	const tobj *consequents, uint_regs count, const char *description);

/**
 * @brief 创建调用另一 Rule Term 的要求 Item。
 *
 * @param rule 产生 Rule 的 Term。
 * @param arguments Rule 实参 Term 数组。
 * @param argument_count 实参数量。
 * @return 新 RuleItem。
 */
trule_item *trule_requirement_new(trule_term *rule,
				  const tobj *arguments,
				  uint_regs argument_count);

/* RuleIR 构造与聚合 */

/**
 * @brief 创建空 RuleIR 并复制显示名称与源码。
 *
 * @param display_name 可为空的显示名称。
 * @param source 可为空的源码文本。
 * @return 新 RuleIR，由 Tapas 引用计数管理。
 */
trule_ir *trule_ir_new(const char *display_name, const char *source);

/**
 * @brief 将参数 Term 加入 RuleIR。
 *
 * @param ir 目标 RuleIR。
 * @param parameter 参数 Term；RuleIR 取得一个引用。
 */
void trule_ir_add_parameter(trule_ir *ir, trule_term *parameter);

/**
 * @brief 将捕获 Term 加入 RuleIR。
 *
 * @param ir 目标 RuleIR。
 * @param capture 捕获 Term；RuleIR 取得一个引用。
 */
void trule_ir_add_capture(trule_ir *ir, trule_term *capture);

/**
 * @brief 将 Item 加入 RuleIR。
 *
 * @param ir 目标 RuleIR。
 * @param item RuleItem；RuleIR 取得一个引用。
 */
void trule_ir_add_item(trule_ir *ir, trule_item *item);

/**
 * @brief 递归收集 Term 及其子项到 RuleIR 的 Term 集合。
 *
 * @param ir 目标 RuleIR。
 * @param term 要收集的根 Term。
 */
void trule_ir_collect_term(trule_ir *ir, trule_term *term);

/* 查询 */

/**
 * @brief 取得 RuleTerm 类别的静态名称。
 *
 * @param kind Term 类别。
 * @return 静态只读名称。
 */
const char *trule_term_kind_name(trule_term_kind kind);

#ifdef __cplusplus
}
#endif

#endif
