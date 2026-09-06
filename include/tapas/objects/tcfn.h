/**
 * @file tcfn.h
 * @brief 声明包装原生 C 可调用函数的核心对象。
 *
 * @details 定义普通原生函数、会话原生函数及其签名描述符，并提供对应引用对象的
 * 构造和查询接口。
 *
 * @note 包函数通过这些通用对象接入调用协议，不得因此向 VM 增加包专用分派逻辑。
 */
#ifndef TAPAS_OBJECTS_TCFN_H
#define TAPAS_OBJECTS_TCFN_H

#include "tapas/tval.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 原生调用协议 */

/** 普通原生函数回调；参数只在调用期间有效，结果写入 `vre`。 */
typedef void (*genf_t)(tobj *params, uint_regs len, tobj *vre);

/** 会话原生函数回调；额外接收当前执行环境的借用指针。 */
typedef void (*sessf_t)(tobj *params, uint_regs len, tobj *vre,
			tcompo_env *env);

/** 原生函数的语言签名与参数数量约束。 */
typedef struct {
	const char *type;              /**< 函数 Type 标注字符串。 */
	uint_regs minimum_parameters;  /**< 允许的最少参数数。 */
	uint_regs maximum_parameters;  /**< 允许的最多参数数。 */
} tcfn_signature;

/** 一个可以注册为 Tapas 符号的原生函数描述符。 */
typedef struct {
	const char *name;               /**< 导出的符号名称。 */
	genf_t function;                /**< 普通回调；与会话回调二选一。 */
	sessf_t session_function;       /**< 会话回调；与普通回调二选一。 */
	tcfn_signature signature;       /**< 类型及参数数量约束。 */
} tcfn_descriptor;

/** 创建固定参数数量的签名初始化值。 */
#define TCFN_FIXED(type_, count_) \
	((tcfn_signature){ (type_), (count_), (count_) })
/** 创建具有最少和最多参数数量的签名初始化值。 */
#define TCFN_RANGE(type_, minimum_, maximum_) \
	((tcfn_signature){ (type_), (minimum_), (maximum_) })
/** 创建至少接受指定参数数量的可变参数签名初始化值。 */
#define TCFN_VARIADIC(type_, minimum_) \
	TCFN_RANGE((type_), (minimum_), UNDEF_NPARAMS)
/** 创建普通原生函数描述符初始化值。 */
#define TCFN_DESCRIPTOR(name_, function_, signature_) \
	((tcfn_descriptor){ (name_), (function_), nullptr, (signature_) })
/** 创建会话原生函数描述符初始化值。 */
#define TCFN_SESSION_DESCRIPTOR(name_, function_, signature_) \
	((tcfn_descriptor){ (name_), nullptr, (function_), (signature_) })

/* 描述符验证与注册 */

/**
 * @brief 检查原生函数描述符是否自洽。
 *
 * @param descriptor 待检查的描述符。
 * @return 有效返回非零，否则返回零。
 */
int tcfn_descriptor_valid(const tcfn_descriptor *descriptor);

/* 兼容旧代码的直接注册接口；新扩展应使用 textension_descriptor。 */

/**
 * @brief 将原生函数直接注册到根符号库。
 *
 * @param library 目标符号库。
 * @param descriptor 原生函数描述符。
 */
void tlib_add_cfn(tlib *library, const tcfn_descriptor *descriptor);

/**
 * @brief 将原生函数直接注册到包字典。
 *
 * @param package 目标包字典。
 * @param descriptor 原生函数描述符。
 */
void tlib_add_pkg_cfn(tdict *package, const tcfn_descriptor *descriptor);

/**
 * @brief 按固定参数数量将普通原生函数注册到根符号库。
 *
 * @param library 目标符号库。
 * @param name 导出名称。
 * @param function 原生回调。
 * @param parameter_count 固定参数数量。
 */
void tlib_add_cppf(tlib *library, const char *name, genf_t function,
		   uint_regs parameter_count);

/* 普通原生函数对象布局 */

/** 普通原生函数对象的运行时布局。 */
struct tcppgenf {
	tcompo_v base;                  /**< 引用对象共有的对象头。 */
	tfunction_metadata *metadata;   /**< 运行时函数元数据。 */
	genf_t f;                       /**< 原生回调。 */
	tstring *name;                  /**< 对象持有的函数名称。 */
	tstring *signature_type;        /**< 对象持有的 Type 标注。 */
	uint_regs minimum_parameters;   /**< 最少参数数。 */
	uint_regs maximum_parameters;   /**< 最多参数数。 */
};

/* 共享 vtable */

/** 普通原生函数对象使用的共享 vtable。 */
extern tcompo_vtable tcppgenf_vtable;

/* 普通原生函数对象的构造与查询 */

/**
 * @brief 根据旧式固定参数签名创建普通原生函数对象。
 *
 * @param f 原生回调。
 * @param name 函数名称。
 * @param nparams_sig 固定参数数量。
 * @return 新对象，由 Tapas 引用计数管理。
 */
tcppgenf *tcppgenf_new(genf_t f, const char *name, uint_regs nparams_sig);

/**
 * @brief 根据描述符创建普通原生函数对象。
 *
 * @param descriptor 有效的普通原生函数描述符。
 * @return 新对象，由 Tapas 引用计数管理。
 */
tcppgenf *tcppgenf_new_descriptor(const tcfn_descriptor *descriptor);

/**
 * @brief 取得对象保存的普通原生回调。
 *
 * @param g 普通原生函数对象。
 * @return 回调指针。
 */
genf_t tcppgenf_get_f(tcppgenf *g);

/**
 * @brief 取得旧式固定参数数量。
 *
 * @param g 普通原生函数对象。
 * @return 固定参数数；非固定签名返回约定的未定义值。
 */
uint_regs tcppgenf_get_nparams_sig(tcppgenf *g);

/**
 * @brief 取得对象持有的 Type 标注。
 *
 * @param g 普通原生函数对象。
 * @return 对象持有的只读字符串。
 */
const char *tcppgenf_get_signature_type(const tcppgenf *g);

/**
 * @brief 取得最少参数数。
 *
 * @param g 普通原生函数对象。
 * @return 最少参数数。
 */
uint_regs tcppgenf_get_minimum_parameters(const tcppgenf *g);

/**
 * @brief 取得最多参数数。
 *
 * @param g 普通原生函数对象。
 * @return 最多参数数；可变参数使用约定的未定义值。
 */
uint_regs tcppgenf_get_maximum_parameters(const tcppgenf *g);

/**
 * @brief 判断对象是否接受指定数量的参数。
 *
 * @param g 普通原生函数对象。
 * @param count 待检查的参数数量。
 * @return 接受返回非零，否则返回零。
 */
int tcppgenf_accepts(const tcppgenf *g, uint_regs count);

/* 会话原生函数对象布局 */

/** 会话原生函数对象的运行时布局。 */
struct tcppsessf {
	tcompo_v base;                  /**< 引用对象共有的对象头。 */
	tfunction_metadata *metadata;   /**< 运行时函数元数据。 */
	sessf_t f;                      /**< 会话原生回调。 */
	tstring *name;                  /**< 对象持有的函数名称。 */
	tstring *signature_type;        /**< 对象持有的 Type 标注。 */
	uint_regs minimum_parameters;   /**< 最少参数数。 */
	uint_regs maximum_parameters;   /**< 最多参数数。 */
};

/* 共享 vtable */

/** 会话原生函数对象使用的共享 vtable。 */
extern tcompo_vtable tcppsessf_vtable;

/* 会话原生函数对象的构造与查询 */

/**
 * @brief 根据旧式签名创建会话原生函数对象。
 *
 * @param f 会话原生回调。
 * @param name 函数名称。
 * @return 新对象，由 Tapas 引用计数管理。
 */
tcppsessf *tcppsessf_new(sessf_t f, const char *name);

/**
 * @brief 根据描述符创建会话原生函数对象。
 *
 * @param descriptor 有效的会话原生函数描述符。
 * @return 新对象，由 Tapas 引用计数管理。
 */
tcppsessf *tcppsessf_new_descriptor(const tcfn_descriptor *descriptor);

/**
 * @brief 取得对象保存的会话原生回调。
 *
 * @param s 会话原生函数对象。
 * @return 回调指针。
 */
sessf_t tcppsessf_get_f(tcppsessf *s);

/**
 * @brief 取得对象持有的 Type 标注。
 *
 * @param s 会话原生函数对象。
 * @return 对象持有的只读字符串。
 */
const char *tcppsessf_get_signature_type(const tcppsessf *s);

/**
 * @brief 判断对象是否接受指定数量的参数。
 *
 * @param s 会话原生函数对象。
 * @param count 待检查的参数数量。
 * @return 接受返回非零，否则返回零。
 */
int tcppsessf_accepts(const tcppsessf *s, uint_regs count);

#ifdef __cplusplus
}
#endif

#endif /* TAPAS_OBJECTS_TCFN_H */
