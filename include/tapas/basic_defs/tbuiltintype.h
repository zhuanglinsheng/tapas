/**
 * @file tbuiltintype.h
 * @brief 声明 Tapas 语言核心直接定义的 Type 身份。
 *
 * @details 这里给出编译器、运行时、VM、序列化与开发工具共同使用的稳定编号，
 * 并提供编号到规范名称的查询接口。
 *
 * @note 包定义的 Type 必须通过扩展模式声明；即使使用 C 实现，也不得因此加入
 * 本文件的核心 Type 枚举。
 */
#ifndef TAPAS_TBUILTINTYPE_H
#define TAPAS_TBUILTINTYPE_H

/**
 * @brief 语言核心 Type 的稳定身份编号。
 *
 * @details 此枚举是闭合集合，只收录不依赖任何包、由编译器和运行时共同认识的
 * Type。标准包可以重新导出核心 Type，但包对象采用 C 实现或被 VM 调用，并不会
 * 自动使其成为核心 Type。
 * @note 数值会用于进程级表索引并构成公共 C ABI。新增成员只能追加在
 * `tbuiltintype_count` 之前，不得重排、复用或填补既有编号。
 */
typedef enum {
	/* 基础值和核心容器。 */
	tbuiltintype_any,          /**< 任意 Tapas Type 的上界。 */
	tbuiltintype_nil,          /**< 空值 Type。 */
	tbuiltintype_bool,         /**< 布尔值 Type。 */
	tbuiltintype_int,          /**< 整数 Type。 */
	tbuiltintype_float,        /**< 浮点数 Type。 */
	tbuiltintype_string,       /**< 字符串 Type。 */
	tbuiltintype_list,         /**< 列表 Type。 */
	tbuiltintype_pair,         /**< 二元组 Type。 */
	tbuiltintype_dictionary,   /**< 字典 Type。 */
	tbuiltintype_iterator,     /**< 整数迭代器 Type。 */
	tbuiltintype_function,     /**< 可调用函数 Type。 */
	tbuiltintype_library,      /**< 模块或运行库 Type。 */
	tbuiltintype_real_array,   /**< 实数稠密数组 Type。 */
	tbuiltintype_bool_array,   /**< 布尔稠密数组 Type。 */
	tbuiltintype_time,         /**< 时间值 Type。 */
	tbuiltintype_type,         /**< Type 值自身的 Type。 */

	/* 由对象能力表实现的核心能力 Type。 */
	tbuiltintype_indexable,      /**< 支持索引读取。 */
	tbuiltintype_index_settable, /**< 支持索引写入。 */
	tbuiltintype_appendable,     /**< 支持追加元素。 */
	tbuiltintype_deletable,      /**< 支持删除元素。 */
	tbuiltintype_contains,       /**< 支持成员包含判断。 */
	tbuiltintype_iterable,       /**< 支持迭代访问。 */

	/* Rule 语言模型使用的核心 Type。 */
	tbuiltintype_rule,             /**< Rule 定义。 */
	tbuiltintype_rule_instance,    /**< 已绑定实参的 Rule 实例。 */
	tbuiltintype_rule_ir,          /**< Rule 中间表示。 */
	tbuiltintype_rule_parameter,   /**< Rule 参数 Term。 */
	tbuiltintype_rule_capture,     /**< Rule 捕获 Term。 */
	tbuiltintype_rule_item,        /**< Rule 项目。 */
	tbuiltintype_rule_condition,   /**< Rule 条件项目。 */
	tbuiltintype_rule_requirement, /**< Rule 子规则要求项目。 */
	tbuiltintype_rule_term,        /**< Rule 表达式 Term。 */
	tbuiltintype_count              /**< 有效核心 Type 身份的数量。 */
} tbuiltintype_id;

/**
 * @brief 取得核心 Type 的未限定规范名称。
 *
 * @param builtin 要查询的核心 Type 身份。
 * @return 指向静态存储区的名称字符串；参数不在有效范围时返回 `nullptr`。
 * @note 调用者不得释放或修改返回的字符串。
 */
const char *tbuiltintype_name(tbuiltintype_id builtin);

#endif
