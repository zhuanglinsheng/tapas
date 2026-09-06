/**
 * @file textension.h
 * @brief 声明原生 C 扩展使用的公共 ABI。
 *
 * @details 描述扩展模块、导出符号、原生函数、命名 Type 模板、依赖参数的结果模式
 * 以及包中立的对象能力。
 *
 * @note 扩展只能在这里声明面向语言的元数据；核心和 VM 不得了解包对象的私有
 * 表示。描述符及其引用的静态字符串和数组必须在扩展安装期间保持有效。
 */
#ifndef TAPAS_TEXTENSION_H
#define TAPAS_TEXTENSION_H

#include "tapas/basic_defs/tbasis.h"

#include <stdint.h>

typedef struct tobj tobj;             /**< Tapas 值。 */
typedef struct tcompo_env tcompo_env; /**< 运行时执行环境。 */
typedef struct tlib tlib;             /**< 可安装扩展的符号库。 */

#ifdef __cplusplus
extern "C" {
#endif

/** 当前原生扩展描述符 ABI 版本。 */
#define TAPAS_EXTENSION_ABI 3u

/** 普通原生函数回调；参数只在调用期间有效，结果写入 `result`。 */
typedef void (*tnative_function)(tobj *arguments, uint_regs argument_count,
				 tobj *result);

/** 会话原生函数回调；额外接收当前执行环境的借用指针。 */
typedef void (*tnative_session_function)(tobj *arguments,
	uint_regs argument_count, tobj *result, tcompo_env *environment);

/** 延迟创建扩展导出值的工厂回调。 */
typedef void (*tnative_value_factory)(tobj *result);

/** 扩展符号的导出类别。 */
typedef enum {
	textension_function, /**< 原生可调用符号。 */
	textension_value,    /**< 由工厂创建的普通值。 */
	textension_type      /**< 由工厂创建的 Type 值。 */
} textension_symbol_kind;

/** 扩展模块的安装范围。 */
typedef enum {
	textension_root,   /**< 直接安装到根符号库。 */
	textension_package /**< 安装到命名包字典。 */
} textension_module_scope;

/** 编译器能够识别的包中立原生函数语义。 */
typedef enum {
	tnative_intrinsic_none,                 /**< 没有编译器内建语义。 */
	tnative_intrinsic_type_make,            /**< 构造字段结构 Type。 */
	tnative_intrinsic_type_union,           /**< 构造联合 Type。 */
	tnative_intrinsic_type_list,            /**< 构造 List Type。 */
	tnative_intrinsic_type_iterator,        /**< 构造 Iterator Type。 */
	tnative_intrinsic_type_optional,        /**< 构造可选 Type。 */
	tnative_intrinsic_type_pair,            /**< 构造 Pair Type。 */
	tnative_intrinsic_type_dictionary,      /**< 构造 Dictionary Type。 */
	tnative_intrinsic_type_rule,            /**< 构造 Rule Type。 */
	tnative_intrinsic_type_rule_instance,   /**< 构造 RuleInstance Type。 */
	tnative_intrinsic_type_enum,            /**< 构造枚举 Type。 */
	tnative_intrinsic_type_parameter,       /**< 构造 Type 参数占位符。 */
	tnative_intrinsic_type_value_parameter, /**< 构造值参数占位符。 */
	tnative_intrinsic_type_template         /**< 构造 Type 模板。 */
} tnative_intrinsic;

/** 原生函数返回 Type 与参数之间的关系。 */
typedef enum {
	tnative_result_declared, /**< 使用签名中直接声明的返回 Type。 */
	tnative_result_argument, /**< 返回 Type 等于指定参数的 Type。 */
	tnative_result_template_argument /**< 将结果模板应用到指定 Type 参数。 */
} tnative_result_relation;

/** 命名 Type 可以声明的通用对象能力位。 */
typedef enum {
	textension_type_indexable = 1u << 0, /**< 对象支持索引读取。 */
	textension_type_contains = 1u << 1,  /**< 对象支持成员判断。 */
	textension_type_iterable = 1u << 2   /**< 对象支持迭代。 */
} textension_type_capability;

/** Type 模板中的一个 Type 参数。 */
typedef struct {
	const char *name; /**< Type 参数名称。 */
	const char *slot; /**< 实例化后写入命名 Type 的参数槽位。 */
} textension_template_type_parameter;

/** Type 模板中的一个受 Type 约束的值参数。 */
typedef struct {
	const char *name; /**< 值参数名称。 */
	const char *slot; /**< 实例化后写入命名 Type 的参数槽位。 */
	const char *constraint; /**< 例如 `Int` 或 `pkg::Constraint` 的 Type 约束。 */
} textension_template_value_parameter;

/** C 包对象对 `types::template` 的声明式等价表示。 */
typedef struct textension_nominal_template {
	const char *identity; /**< 模板实例的限定命名 Type 身份。 */
	const textension_template_type_parameter *type_parameters; /**< Type 参数数组。 */
	uint_regs type_parameter_count; /**< Type 参数数量。 */
	const textension_template_value_parameter *value_parameters; /**< 值参数数组。 */
	uint_regs value_parameter_count; /**< 值参数数量。 */
	uint32_t capabilities; /**< 实例对象必须提供的能力位集合。 */
} textension_nominal_template;

/** 扩展导出的一个函数、值或 Type 符号。 */
typedef struct {
	const char *name; /**< 导出的符号名称。 */
	const char *type; /**< 函数签名、值 Type 或 Type 符号定义。 */
	const char *detail; /**< 面向工具和诊断的详细说明。 */
	textension_symbol_kind kind; /**< 符号类别。 */
	tnative_function function; /**< 普通原生函数回调。 */
	tnative_session_function session_function; /**< 会话原生函数回调。 */
	tnative_value_factory value_factory; /**< 值或 Type 工厂。 */
	uint_regs minimum_arguments; /**< 最少参数数。 */
	uint_regs maximum_arguments; /**< 最多参数数。 */
	tnative_intrinsic intrinsic; /**< 编译器可识别的通用内建语义。 */
	tnative_result_relation result_relation; /**< 返回 Type 与参数的关系。 */
	uint_regs result_argument; /**< 结果关系引用的参数索引。 */
	const textension_nominal_template *nominal_template; /**< Type 符号的命名模板。 */
	const textension_nominal_template *result_template; /**< 结果 Type 使用的模板。 */
	uint8_t following_arguments_match_result_parameter; /**< 后续参数是否匹配结果参数。 */
	uint8_t compile_time_signature; /**< 是否启用编译期签名检查。 */
} textension_symbol;

/** 一组安装到根库或包字典的扩展符号。 */
typedef struct {
	textension_module_scope scope; /**< 安装范围。 */
	const char *name; /**< 包模块名称；根模块可为空。 */
	const char *detail; /**< 模块说明。 */
	const textension_symbol *symbols; /**< 导出符号数组。 */
	uint32_t symbol_count; /**< 导出符号数量。 */
} textension_module;

/** 一个完整原生扩展的顶层描述符。 */
typedef struct {
	uint32_t abi_version; /**< 必须等于 `TAPAS_EXTENSION_ABI`。 */
	uint32_t structure_size; /**< 当前描述符结构大小。 */
	const char *name; /**< 扩展名称。 */
	const char *version; /**< 扩展版本字符串。 */
	const textension_module *const *modules; /**< 模块指针数组。 */
	uint32_t module_count; /**< 模块数量。 */
} textension_descriptor;

/** 同时指向所属模块和命中符号的查询结果。 */
typedef struct {
	const textension_module *module; /**< 命中符号所属模块。 */
	const textension_symbol *symbol; /**< 命中的符号。 */
} textension_symbol_ref;

/**
 * @brief 将扩展描述符安装到符号库。
 *
 * @param library 目标符号库。
 * @param extension 待验证并安装的扩展描述符。
 * @return 安装成功返回非零，描述符无效时返回零。
 */
int tlib_install_extension(tlib *library,
			   const textension_descriptor *extension);

/** 使用详细说明初始化普通原生函数符号。 */
#define TAPAS_NATIVE_FUNCTION_DETAIL( \
	name_, function_, minimum_, maximum_, type_, detail_) \
	{ \
		.name = (name_), .type = (type_), .detail = (detail_), \
		.kind = textension_function, .function = (function_), \
		.minimum_arguments = (minimum_), \
		.maximum_arguments = (maximum_) \
	}

/** 使用 Type 标注同时作为说明初始化普通原生函数符号。 */
#define TAPAS_NATIVE_FUNCTION(name_, function_, minimum_, maximum_, type_) \
	TAPAS_NATIVE_FUNCTION_DETAIL(name_, function_, minimum_, maximum_, \
		type_, type_)

/** 使用详细说明初始化会话原生函数符号。 */
#define TAPAS_NATIVE_SESSION_FUNCTION_DETAIL( \
	name_, function_, minimum_, maximum_, type_, detail_) \
	{ \
		.name = (name_), .type = (type_), .detail = (detail_), \
		.kind = textension_function, .session_function = (function_), \
		.minimum_arguments = (minimum_), \
		.maximum_arguments = (maximum_) \
	}

/** 使用 Type 标注同时作为说明初始化会话原生函数符号。 */
#define TAPAS_NATIVE_SESSION_FUNCTION( \
	name_, function_, minimum_, maximum_, type_) \
	TAPAS_NATIVE_SESSION_FUNCTION_DETAIL(name_, function_, minimum_, maximum_, \
		type_, type_)

/** 初始化由工厂创建的普通值符号。 */
#define TAPAS_NATIVE_VALUE(name_, factory_, type_) \
	{ \
		.name = (name_), .type = (type_), .detail = (type_), \
		.kind = textension_value, .value_factory = (factory_) \
	}

/** 初始化由工厂创建的 Type 符号。 */
#define TAPAS_NATIVE_TYPE(name_, factory_) \
	{ \
		.name = (name_), .type = "Type", .detail = "Type", \
		.kind = textension_type, .value_factory = (factory_) \
	}

/** 根据静态符号数组初始化根模块。 */
#define TAPAS_ROOT_MODULE(symbols_) \
	{ \
		.scope = textension_root, .symbols = (symbols_), \
		.symbol_count = sizeof(symbols_) / sizeof((symbols_)[0]) \
	}

/** 根据名称和静态符号数组初始化包模块。 */
#define TAPAS_PACKAGE_MODULE(name_, symbols_) \
	{ \
		.scope = textension_package, .name = (name_), \
		.detail = "default package", .symbols = (symbols_), \
		.symbol_count = sizeof(symbols_) / sizeof((symbols_)[0]) \
	}

/** 根据静态模块数组初始化扩展描述符。 */
#define TAPAS_EXTENSION(name_, version_, modules_) \
	{ \
		.abi_version = TAPAS_EXTENSION_ABI, \
		.structure_size = sizeof(textension_descriptor), \
		.name = (name_), .version = (version_), .modules = (modules_), \
		.module_count = sizeof(modules_) / sizeof((modules_)[0]) \
	}

/**
 * @brief 检查扩展描述符及其所有模块和符号是否有效。
 *
 * @param extension 待检查的描述符。
 * @return 有效返回非零，否则返回零。
 */
int textension_validate(const textension_descriptor *extension);

/**
 * @brief 统计扩展中所有模块的符号总数。
 *
 * @param extension 扩展描述符。
 * @return 符号总数。
 */
uint32_t textension_symbol_count(const textension_descriptor *extension);

/**
 * @brief 按扁平索引取得符号引用。
 *
 * @param extension 扩展描述符。
 * @param index 跨模块连续计数的符号索引。
 * @param result 接收所属模块和符号指针。
 * @return 索引有效时返回非零，否则返回零。
 */
int textension_symbol_at(const textension_descriptor *extension, uint32_t index,
			 textension_symbol_ref *result);

/**
 * @brief 按包名和符号名查找导出。
 *
 * @param extension 扩展描述符。
 * @param package 包名；查找根模块时传空指针。
 * @param name 符号名。
 * @param result 接收所属模块和符号指针。
 * @return 找到时返回非零，否则返回零。
 */
int textension_find(const textension_descriptor *extension,
		     const char *package, const char *name,
		     textension_symbol_ref *result);

#ifdef __cplusplus
}
#endif

#endif
