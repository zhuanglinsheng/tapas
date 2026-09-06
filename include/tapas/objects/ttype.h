/**
 * @file ttype.h
 * @brief 声明语言核心的运行时 Type 对象接口。
 *
 * @details 提供内建 Type、结构 Type、Type 模板和包中立命名 Type 的构造、反射、
 * 规范化与匹配接口。
 *
 * @note 包对象仍由各包实现，并通过扩展模式和通用对象能力暴露 Type；本文件只提供
 * 表达这些 Type 所需的通用核心机制。
 */
#ifndef TAPAS_OBJECTS_TTYPE_H
#define TAPAS_OBJECTS_TTYPE_H

#include "tapas/basic_defs/tbuiltintype.h"
#include "tapas/tval.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Type 类别 */

/** Type 对象采用的结构类别。 */
typedef enum {
	ttype_kind_any = 0,       /**< 接受任意值。 */
	ttype_kind_builtin,       /**< 核心内建 Type。 */
	ttype_kind_named,         /**< 可带参数的限定命名 Type。 */
	ttype_kind_fields,        /**< 按字段描述的结构 Type。 */
	ttype_kind_list,          /**< List Type。 */
	ttype_kind_iterator,      /**< Iterator Type。 */
	ttype_kind_pair,          /**< Pair Type。 */
	ttype_kind_dictionary,    /**< Dictionary Type。 */
	ttype_kind_function,      /**< Function Type。 */
	ttype_kind_rule,          /**< Rule Type。 */
	ttype_kind_rule_instance, /**< RuleInstance Type。 */
	ttype_kind_rule_term,     /**< RuleTerm Type。 */
	ttype_kind_union,         /**< 联合 Type。 */
	ttype_kind_enum,          /**< 枚举 Type。 */
	ttype_kind_recursive,     /**< 递归 Type。 */
	ttype_kind_instance_of,   /**< 由具体 Rule 定义的实例 Type。 */
	ttype_kind_parameter,     /**< Type 参数占位符。 */
	ttype_kind_value_parameter, /**< 值参数占位符。 */
	ttype_kind_exact_value,   /**< 精确匹配某个值的 Type。 */
	ttype_kind_template       /**< 同时接收 Type 参数和值参数的模板。 */
} ttype_kind;

typedef struct textension_nominal_template textension_nominal_template; /**< C 扩展命名模板模式。 */
typedef struct thashtbl thashtbl; /**< Type 内部使用的哈希表。 */

/* 参数与字段描述 */

/** 字段结构或命名 Type 的一个字段参数。 */
typedef struct {
	const tstring *name; /**< 字段名称，构造期间借用。 */
	ttypeval *type;      /**< 字段 Type。 */
	uint8_t optional;    /**< 非零表示字段可省略。 */
} ttype_field;

/** Type 模板的一个值参数及其约束。 */
typedef struct {
	tstring *name;  /**< 值参数名称。 */
	ttypeval *type; /**< 值参数约束 Type。 */
} ttype_value_parameter;

/* 对象布局 */

/** Type 对象的运行时布局。 */
struct ttypeval {
	tcompo_v base;                         /**< 引用对象共有的对象头。 */
	ttype_kind kind;                       /**< Type 的结构类别。 */
	tbuiltintype_id builtin;               /**< 内建 Type 身份。 */
	thashtbl *definition;                  /**< 结构成员与参数定义。 */
	tstring *canonical;                    /**< 缓存的规范表示。 */
	uint64_t canonical_hash;               /**< 规范表示的缓存哈希。 */
	uint_objs function_parameter_count;    /**< 函数参数数量。 */
	uint8_t function_variadic;             /**< 函数是否接受可变参数。 */
	uint8_t contains_recursive;            /**< 是否包含递归节点。 */
	uint8_t recursive_defined;             /**< 递归体是否已定义。 */
	uint32_t recursive_id;                 /**< 递归节点身份。 */
	ttypeval *recursive_body;              /**< 递归 Type 的主体。 */
	tstring *named_identity;               /**< 限定命名 Type 身份。 */
	tstring **named_parameter_names;       /**< 命名参数名称数组。 */
	ttypeval **named_parameter_types;      /**< 命名参数 Type 数组。 */
	uint_objs named_parameter_count;       /**< 命名参数数量。 */
	uint32_t capabilities;                 /**< 命名 Type 声明的对象能力。 */
	tobj instance_rule;                    /**< instance-of Type 持有的 Rule。 */
	tstring *instance_reference;           /**< 尚未解析的 Rule 符号名。 */
	uint8_t contains_instance;             /**< 是否包含实例 Type。 */
	uint8_t contains_custom;               /**< 是否包含包定义 Type。 */
	tstring **optional_fields;             /**< 可选字段名称数组。 */
	uint_objs optional_field_count;        /**< 可选字段数量。 */
	tstring **enum_members;                /**< 枚举成员名称数组。 */
	uint_objs enum_member_count;           /**< 枚举成员数量。 */
	tstring *parameter_name;               /**< 模板参数占位符名称。 */
	ttypeval *template_body;               /**< 模板实例化前的 Type 主体。 */
	tstring **template_type_parameters;    /**< Type 参数名称数组。 */
	uint_objs template_type_parameter_count; /**< Type 参数数量。 */
	ttype_value_parameter *template_value_parameters; /**< 值参数描述数组。 */
	uint_objs template_value_parameter_count; /**< 值参数数量。 */
	tobj exact_value;                      /**< 精确值 Type 持有的值。 */
};

/* 共享 vtable 与文本表示 */

/** Type 对象使用的共享 vtable。 */
extern tcompo_vtable ttypeval_vtable;

struct tformat_context; /**< 统一对象文本格式化上下文。 */

/**
 * @brief 将 Type 的规范文本写入统一格式化上下文。
 *
 * @param context 活动格式化上下文。
 * @param type 要渲染的 Type。
 */
void tformat_type(struct tformat_context *context, ttypeval *type);

/* 构造与解析 */

/**
 * @brief 取得指定核心内建 Type 的共享对象。
 *
 * @param builtin 核心 Type 身份。
 * @return 由运行时持有的 Type 指针。
 */
ttypeval *ttypeval_builtin(tbuiltintype_id builtin);

/**
 * @brief 按核心 Type 名称取得共享对象。
 *
 * @param name 核心 Type 的未限定名称。
 * @return 对应 Type；名称无效时返回空指针。
 */
ttypeval *ttypeval_builtin_named(const char *name);

/**
 * @brief 创建不带参数的限定命名 Type。
 *
 * @param qualified_name 包含包限定的稳定身份名称。
 * @return 新 Type，由 Tapas 引用计数管理。
 */
ttypeval *ttypeval_new_named(const char *qualified_name);

/**
 * @brief 创建带命名参数的限定命名 Type。
 *
 * @param qualified_name 限定身份名称。
 * @param parameters 参数名称与 Type 数组。
 * @param parameter_count 参数数量。
 * @param capabilities 实例对象承诺提供的能力位。
 * @return 新 Type。
 */
ttypeval *ttypeval_new_named_application(const char *qualified_name,
	const ttype_field *parameters, uint_objs parameter_count,
	uint32_t capabilities);

/**
 * @brief 根据 C 扩展声明的命名模板创建 Type 模板。
 *
 * @param schema 扩展静态持有的模板模式。
 * @return 新 Type 模板。
 */
ttypeval *ttypeval_new_extension_template(
	const textension_nominal_template *schema);

/**
 * @brief 创建 Type 参数占位符。
 *
 * @param name 参数名称。
 * @return 新占位符 Type。
 */
ttypeval *ttypeval_new_parameter(const char *name);

/**
 * @brief 创建值参数占位符。
 *
 * @param name 参数名称。
 * @return 新占位符 Type。
 */
ttypeval *ttypeval_new_value_parameter(const char *name);

/**
 * @brief 创建只匹配指定值的精确值 Type。
 *
 * @param value 要持有并精确匹配的值。
 * @return 新 Type。
 */
ttypeval *ttypeval_new_exact_value(const tobj *value);

/**
 * @brief 取得精确值 Type 持有的值。
 *
 * @param type 精确值 Type。
 * @return Type 持有的只读值；类别不符时返回空指针。
 */
const tobj *ttypeval_exact_value(const ttypeval *type);

/**
 * @brief 创建同时接收 Type 参数和值参数的 Type 模板。
 *
 * @param type_parameters Type 参数名称数组。
 * @param type_parameter_count Type 参数数量。
 * @param value_parameters 值参数描述数组。
 * @param value_parameter_count 值参数数量。
 * @param body 包含参数占位符的模板主体。
 * @return 新 Type 模板。
 */
ttypeval *ttypeval_new_template(const tstring *const *type_parameters,
	uint_objs type_parameter_count,
	const ttype_value_parameter *value_parameters,
	uint_objs value_parameter_count, ttypeval *body);

/**
 * @brief 将 Type 实参与值实参代入模板。
 *
 * @param template_type Type 模板。
 * @param type_arguments Type 实参数组。
 * @param type_argument_count Type 实参数量。
 * @param value_arguments 值实参数组。
 * @param value_argument_count 值实参数量。
 * @return 实例化后的新 Type；数量或约束不符时报告错误。
 */
ttypeval *ttypeval_apply_template(ttypeval *template_type,
	ttypeval *const *type_arguments, uint_objs type_argument_count,
	const tobj *value_arguments, uint_objs value_argument_count);

/**
 * @brief 判断对象是否为 Type 模板。
 *
 * @param type Type 对象。
 * @return 是模板返回非零，否则返回零。
 */
int ttypeval_is_template(const ttypeval *type);

/**
 * @brief 解析规范 Type 标注并创建对应对象。
 *
 * @param canonical 规范 Type 标注。
 * @return 新 Type；标注无效时返回空指针。
 */
ttypeval *ttypeval_from_canonical(const char *canonical);

/* 生命周期 */

/**
 * @brief 增加 Type 对象的引用计数。
 *
 * @param type Type 对象。
 * @return 与输入相同的指针。
 */
ttypeval *ttypeval_retain(ttypeval *type);

/**
 * @brief 释放一个 Type 对象引用。
 *
 * @param type 可为空的 Type 指针。
 */
void ttypeval_release(ttypeval *type);

/* 结构 Type 构造 */

/**
 * @brief 创建字段结构 Type。
 *
 * @param fields 字段描述数组。
 * @param count 字段数量。
 * @return 新 Type。
 */
ttypeval *ttypeval_new_fields(const ttype_field *fields, uint_objs count);

/**
 * @brief 创建 List Type。
 *
 * @param item 元素 Type。
 * @return 新 Type。
 */
ttypeval *ttypeval_new_list(ttypeval *item);

/**
 * @brief 创建 Iterator Type。
 *
 * @param item 迭代产生的元素 Type。
 * @return 新 Type。
 */
ttypeval *ttypeval_new_iterator(ttypeval *item);

/**
 * @brief 创建 Pair Type。
 *
 * @param first 第一个元素 Type。
 * @param second 第二个元素 Type。
 * @return 新 Type。
 */
ttypeval *ttypeval_new_pair(ttypeval *first, ttypeval *second);

/**
 * @brief 创建 Dictionary Type。
 *
 * @param key 键 Type。
 * @param value 值 Type。
 * @return 新 Type。
 */
ttypeval *ttypeval_new_dictionary(ttypeval *key, ttypeval *value);

/**
 * @brief 创建 Function Type。
 *
 * @param parameters 参数 Type 数组。
 * @param parameter_count 参数数量。
 * @param result 返回 Type。
 * @param variadic 非零表示接受可变参数。
 * @return 新 Type。
 */
ttypeval *ttypeval_new_function(ttypeval *const *parameters,
				uint_objs parameter_count,
				ttypeval *result,
				int variadic);

/**
 * @brief 创建 Rule Type。
 *
 * @param parameters 参数 Type 数组。
 * @param parameter_count 参数数量。
 * @return 新 Type。
 */
ttypeval *ttypeval_new_rule(ttypeval *const *parameters,
			    uint_objs parameter_count);

/**
 * @brief 创建 RuleInstance Type。
 *
 * @param parameters 参数 Type 数组。
 * @param parameter_count 参数数量。
 * @return 新 Type。
 */
ttypeval *ttypeval_new_rule_instance(ttypeval *const *parameters,
				     uint_objs parameter_count);

/**
 * @brief 创建尚未解析具体 Rule 的符号实例 Type。
 *
 * @param name Rule 的符号名称。
 * @param parameters 实例参数 Type 数组。
 * @param count 参数数量。
 * @return 新 Type。
 */
ttypeval *ttypeval_new_instance_reference(const char *name,
	ttypeval *const *parameters, uint_objs count);

/**
 * @brief 根据具体 Rule 值创建实例 Type。
 *
 * @param rule Rule 值。
 * @return 新 Type。
 */
ttypeval *ttypeval_new_instance_of(const tobj *rule);

/* Rule 实例 Type 解析 */

/** 按符号名称返回 Rule 借用指针的解析回调。 */
typedef const tobj *(*ttype_instance_resolver)(void *context, const char *name);

/**
 * @brief 解析 Type 树中的符号 Rule 引用。
 *
 * @param type 待解析的 Type。
 * @param resolver 符号 Rule 解析回调。
 * @param context 原样传给解析回调的上下文。
 * @return 解析后的 Type 独立引用。
 */
ttypeval *ttypeval_resolve_instances(ttypeval *type, ttype_instance_resolver resolver, void *context);

/* 其余 Type 构造 */

/**
 * @brief 创建具有指定结果 Type 的 RuleTerm Type。
 *
 * @param result Term 求值结果 Type。
 * @return 新 Type。
 */
ttypeval *ttypeval_new_rule_term(ttypeval *result);

/**
 * @brief 创建联合 Type。
 *
 * @param members 成员 Type 数组。
 * @param count 成员数量。
 * @return 规范化后的新 Type。
 */
ttypeval *ttypeval_new_union(ttypeval *const *members, uint_objs count);

/**
 * @brief 创建字符串成员枚举 Type。
 *
 * @param members 枚举成员名称数组。
 * @param count 成员数量。
 * @return 新 Type。
 */
ttypeval *ttypeval_new_enum(const tstring *const *members, uint_objs count);

/**
 * @brief 创建尚未定义主体的递归 Type。
 *
 * @return 新的递归 Type 占位符。
 */
ttypeval *ttypeval_new_recursive(void);

/**
 * @brief 为递归 Type 定义主体。
 *
 * @param type 尚未定义的递归 Type。
 * @param body 递归主体。
 * @return 定义成功返回非零，否则返回零。
 */
int ttypeval_define_recursive(ttypeval *type, ttypeval *body);

/* 比较与匹配 */

/**
 * @brief 判断 Type 是否为递归 Type。
 *
 * @param type Type 对象。
 * @return 是递归 Type 返回非零，否则返回零。
 */
int ttypeval_is_recursive(const ttypeval *type);

/**
 * @brief 判断两个 Type 的结构语义是否相等。
 *
 * @param left 左 Type。
 * @param right 右 Type。
 * @return 相等返回非零，否则返回零。
 */
int ttypeval_equal(const ttypeval *left, const ttypeval *right);

/**
 * @brief 计算 Type 的稳定结构哈希。
 *
 * @param type Type 对象。
 * @return 结构哈希值。
 */
uint64_t ttypeval_hash(const ttypeval *type);

/**
 * @brief 判断 Tapas 值是否符合预期 Type。
 *
 * @param value 待检查的值。
 * @param expected 预期 Type。
 * @return 匹配返回非零，否则返回零。
 */
int ttypeval_matches(const tobj *value, const ttypeval *expected);

/* 操作符 */

/**
 * @brief 实现 Type 对象的语言层索引操作。
 *
 * @param type 被索引的 Type 或 Type 模板。
 * @param params Type 参数和值参数数组。
 * @param np 参数数量。
 * @param result 接收实例化或索引结果。
 */
void ttypeval_idx(ttypeval *type, const tobj *params, uint_regs np,
		  tobj *result);

/* 反射查询 */

/**
 * @brief 取得字段结构 Type 的字段数量。
 *
 * @param type 字段结构 Type。
 * @return 字段数量。
 */
uint_objs ttypeval_field_count(const ttypeval *type);

/**
 * @brief 按索引取得字段名称与 Type。
 *
 * @param type 字段结构 Type。
 * @param index 字段索引。
 * @param name 接收字段名称值的借用指针。
 * @param field_type 接收字段 Type 的借用指针。
 * @return 索引有效返回非零，否则返回零。
 */
int ttypeval_field_at(const ttypeval *type, uint_objs index,
		      const tobj **name, ttypeval **field_type);

/**
 * @brief 按名称取得字段 Type。
 *
 * @param type 字段结构 Type。
 * @param name 字段名称。
 * @return 字段 Type 的借用指针；不存在时返回空指针。
 */
ttypeval *ttypeval_field_named(const ttypeval *type, const char *name);

/**
 * @brief 判断指定字段是否为可选字段。
 *
 * @param type 字段结构 Type。
 * @param name 字段名称。
 * @return 可选返回非零，否则返回零。
 */
int ttypeval_field_optional(const ttypeval *type, const char *name);

/**
 * @brief 取得联合 Type 或参数化 Type 的成员数量。
 *
 * @param type Type 对象。
 * @return 成员数量。
 */
uint_objs ttypeval_member_count(const ttypeval *type);

/**
 * @brief 按索引取得成员 Type。
 *
 * @param type Type 对象。
 * @param index 成员索引。
 * @return 成员 Type 的借用指针；索引无效时返回空指针。
 */
ttypeval *ttypeval_member_at(const ttypeval *type, uint_objs index);

/**
 * @brief 取得枚举 Type 的成员数量。
 *
 * @param type 枚举 Type。
 * @return 成员数量。
 */
uint_objs ttypeval_enum_member_count(const ttypeval *type);

/**
 * @brief 按索引取得枚举成员名称。
 *
 * @param type 枚举 Type。
 * @param index 成员索引。
 * @return 名称的借用指针；索引无效时返回空指针。
 */
const tstring *ttypeval_enum_member_at(const ttypeval *type, uint_objs index);

/**
 * @brief 判断枚举 Type 是否包含指定名称。
 *
 * @param type 枚举 Type。
 * @param member 成员名称。
 * @return 包含返回非零，否则返回零。
 */
int ttypeval_enum_contains(const ttypeval *type, const tstring *member);

/**
 * @brief 取得参数化 Type 的基础 Type。
 *
 * @param type 参数化 Type。
 * @return 基础 Type 的借用指针；不适用时返回空指针。
 */
ttypeval *ttypeval_base(const ttypeval *type);

/**
 * @brief 按名称取得参数化 Type 的参数。
 *
 * @param type 参数化 Type。
 * @param name 参数名称。
 * @return 参数 Type 的借用指针；不存在时返回空指针。
 */
ttypeval *ttypeval_parameter(const ttypeval *type, const char *name);

/**
 * @brief 取得 Function Type 的参数数量。
 *
 * @param type Function Type。
 * @return 参数数量。
 */
uint_objs ttypeval_function_parameter_count(const ttypeval *type);

/**
 * @brief 按索引取得 Function Type 的参数 Type。
 *
 * @param type Function Type。
 * @param index 参数索引。
 * @return 参数 Type 的借用指针；索引无效时返回空指针。
 */
ttypeval *ttypeval_function_parameter_at(const ttypeval *type,
					 uint_objs index);

/**
 * @brief 取得 Function Type 的返回 Type。
 *
 * @param type Function Type。
 * @return 返回 Type 的借用指针。
 */
ttypeval *ttypeval_function_result(const ttypeval *type);

/**
 * @brief 判断 Function Type 是否接受可变参数。
 *
 * @param type Function Type。
 * @return 接受可变参数返回非零，否则返回零。
 */
int ttypeval_function_variadic(const ttypeval *type);

/**
 * @brief 取得 Type 内部定义表。
 *
 * @param type Type 对象。
 * @return 内部定义表的只读借用指针；不适用时返回空指针。
 */
const thashtbl *ttypeval_definition(const ttypeval *type);

#ifdef __cplusplus
}
#endif

#endif /* TAPAS_OBJECTS_TTYPE_H */
