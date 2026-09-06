/**
 * @file tval.h
 * @brief 声明语言核心的值 ABI 与引用对象协议。
 *
 * @details 定义立即值、引用对象句柄、对象 vtable、可选能力，以及不依赖具体对象
 * 布局的通用值操作。
 *
 * @note 核心对象的具体布局位于 `tapas/objects`；包对象的布局必须留在所属包中，
 * 只能通过本文件的通用协议接入运行时。
 */
#ifndef TAPAS_TVAL_H
#define TAPAS_TVAL_H

#include "tapas/basic_defs/tbasis.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 核心值类型前置声明；具体对象定义位于各自头文件中。 */
typedef struct tobj tobj;                         /**< Tapas 值。 */
typedef struct tcompo_v tcompo_v;                 /**< 引用对象共有的对象头。 */
typedef struct tstr tstr;                         /**< String 对象。 */
typedef struct tlist tlist;                       /**< List 对象。 */
typedef struct tpair tpair;                       /**< Pair 对象。 */
typedef struct tdict tdict;                       /**< Dictionary 对象。 */
typedef struct titer titer;                       /**< Iterator 对象。 */
typedef struct tdarr tdarr;                       /**< 实数数组对象。 */
typedef struct tbarr tbarr;                       /**< 布尔数组对象。 */
typedef struct ttime ttime;                       /**< Time 对象。 */
typedef struct tfunc tfunc;                       /**< Tapas 函数对象。 */
typedef struct tlib tlib;                         /**< 符号库对象。 */
typedef struct tcompo_env tcompo_env;             /**< 运行时执行环境。 */
typedef struct tcppgenf tcppgenf;                 /**< 普通原生函数对象。 */
typedef struct tcppsessf tcppsessf;               /**< 会话原生函数对象。 */
typedef struct ttypeval ttypeval;                 /**< Type 对象。 */
typedef struct tfunction_metadata tfunction_metadata; /**< 函数元数据。 */
typedef struct trule trule;                       /**< Rule 对象。 */
typedef struct trule_instance trule_instance;     /**< RuleInstance 对象。 */
typedef struct trule_builtin trule_builtin;       /**< 内建 Rule 可调用对象。 */
typedef struct trule_ir trule_ir;                 /**< RuleIR 对象。 */
typedef struct trule_term trule_term;             /**< RuleTerm 对象。 */
typedef struct trule_item trule_item;             /**< RuleItem 对象。 */

typedef const char *(*compo_get_type_fn)(void); /**< 返回对象类别名称。 */
typedef tcompo_type (*compo_get_code_fn)(void); /**< 返回对象表示编号。 */
typedef long (*compo_len_fn)(void *self);       /**< 返回对象长度。 */
typedef void *(*compo_copy_fn)(void *self);     /**< 创建对象副本。 */
typedef void (*compo_free_fn)(void *self);      /**< 销毁对象。 */
typedef int (*compo_identical_fn)(void *self, void *other); /**< 判断对象同一性。 */
typedef tstring *(*compo_tostring_abbr_fn)(void *self); /**< 创建简略文本表示。 */
typedef tstring *(*compo_tostring_full_fn)(void *self); /**< 创建完整文本表示。 */
typedef void (*compo_op_unary_fn)(void *self, tobj *result); /**< 执行一元运算。 */
typedef void (*compo_op_bin_fn)(void *self, const tobj *other,
				int is_rhs, tobj *vre); /**< 执行可交换左右位置的二元运算。 */

/* 可选对象能力 */

/*
 * 对象能力是语言语法和通用标准库函数可以调用的可选操作。槽位为空表示对象不支持
 * 对应操作。
 *
 * indexable       读取 value[indices]，也是 idx(value, ...) 的底层操作。
 * index_settable  写入 value[indices] = replacement。
 * appendable      实现 append(value, item)。
 * deletable       实现 delete(value, key_or_index)。
 * contains        实现 item in value。
 * iterable        为 for 循环逐个产生值。
 * runtime_type    返回包对象精确的运行时 Type。
 * matches_type    判断包对象是否匹配包所拥有的 Type。
 *
 * 迭代实现接收由调用者持有的游标存储。成功产生一个值后更新游标并返回 1，遍历
 * 结束时返回 0。对象不得保存游标；同一对象上的独立迭代和嵌套迭代必须互不影响。
 *
 * 运算符不属于可选能力，因为分派可能同时依赖两个操作数，所以使用独立 vtable
 * 槽位。生命周期、长度、同一性、复制和字符串转换属于每个引用对象必须实现的
 * 基础操作。
 */

/** 对象索引读取能力回调。 */
typedef void (*tcompo_index_fn)(void *self, const tobj *arguments,
	uint_regs argument_count, tobj *result);

/** 对象索引写入能力回调。 */
typedef void (*tcompo_index_set_fn)(void *self, const tobj *arguments,
	uint_regs argument_count, const tobj *value);

/** 对象追加能力回调。 */
typedef void (*tcompo_append_fn)(void *self, const tobj *value);

/** 对象删除能力回调。 */
typedef void (*tcompo_delete_fn)(void *self, const tobj *key);

/** 对象成员判断能力回调。 */
typedef int (*tcompo_contains_fn)(void *self, const tobj *value);

/** 对象迭代能力回调。 */
typedef int (*tcompo_next_fn)(void *self, long *position, tobj *result);

/** 对象精确运行时 Type 查询回调。 */
typedef void (*tcompo_runtime_type_fn)(void *self, tobj *result);

/** 包对象自定义 Type 匹配回调。 */
typedef int (*tcompo_matches_type_fn)(void *self, const tobj *type);

/** 引用对象可以选择实现的通用能力表。 */
typedef struct {
	tcompo_index_fn indexable;            /**< 索引读取能力。 */
	tcompo_index_set_fn index_settable;   /**< 索引写入能力。 */
	tcompo_append_fn appendable;          /**< 追加元素能力。 */
	tcompo_delete_fn deletable;           /**< 删除成员能力。 */
	tcompo_contains_fn contains;          /**< 成员判断能力。 */
	tcompo_next_fn iterable;              /**< 迭代能力。 */
	tcompo_runtime_type_fn runtime_type;  /**< 精确运行时 Type 查询能力。 */
	tcompo_matches_type_fn matches_type;  /**< 包对象自定义 Type 匹配能力。 */
} tcompo_capabilities;

/** 所有引用对象共享的 vtable 布局。 */
typedef struct {
	/* 必需的对象操作 */

	compo_get_type_fn get_type;              /**< 返回对象类别名称。 */
	compo_get_code_fn get_compo_type_code;   /**< 返回运行时表示编号。 */
	compo_len_fn len;                        /**< 返回对象长度。 */
	compo_copy_fn copy;                      /**< 创建对象副本。 */
	compo_free_fn free;                      /**< 销毁对象。 */
	compo_identical_fn identical;            /**< 判断对象同一性。 */
	compo_tostring_abbr_fn tostring_abbr;    /**< 创建简略文本表示。 */
	compo_tostring_full_fn tostring_full;    /**< 创建完整文本表示。 */

	/* 运算符 */

	compo_op_unary_fn op_neg; /**< 一元负号。 */
	compo_op_bin_fn op_add;   /**< 加法。 */
	compo_op_bin_fn op_sub;   /**< 减法。 */
	compo_op_bin_fn op_mul;   /**< 乘法。 */
	compo_op_bin_fn op_div;   /**< 除法。 */
	compo_op_bin_fn op_mod;   /**< 取模。 */
	compo_op_bin_fn op_pow;   /**< 幂运算。 */
	compo_op_bin_fn op_mmul;  /**< 矩阵乘法。 */
	compo_op_bin_fn op_eq;    /**< 相等比较。 */
	compo_op_bin_fn op_ne;    /**< 不等比较。 */
	compo_op_bin_fn op_sg;    /**< 大于比较。 */
	compo_op_bin_fn op_sl;    /**< 小于比较。 */
	compo_op_bin_fn op_ge;    /**< 大于等于比较。 */
	compo_op_bin_fn op_le;    /**< 小于等于比较。 */
	compo_op_bin_fn op_and;   /**< 逻辑与。 */
	compo_op_bin_fn op_or;    /**< 逻辑或。 */

	/* 可选能力 */

	const tcompo_capabilities *capabilities; /**< 可为空的能力表。 */
} tcompo_vtable;

/** 所有引用对象共有的对象头。 */
struct tcompo_v {
	tcompo_vtable *vtable; /**< 对象实现使用的共享操作表。 */
	int refctr;            /**< 对象的引用计数。 */
};

/** Tapas 值的统一运行时表示。 */
struct tobj {
	ttypes type;  /**< 顶层存储类别。 */
	int name_loc; /**< 关联名称槽位；无名称时使用约定的未定义值。 */
	/** 与 `type` 对应的实际负载。 */
	union {
		long v_tint;       /**< Integer 值。 */
		double v_tfloat;  /**< Float 值。 */
		int v_tbool;      /**< Boolean 值。 */
		tcompo_v *v_tcompo; /**< 引用对象指针。 */
	} val;
};

/**
 * @brief 将目标值设置为 Nil。
 *
 * @param v 目标值。
 */
void tobj_set_nil(tobj *v);

/**
 * @brief 将目标值设置为 Boolean。
 *
 * @param v 目标值。
 * @param b 零表示假，非零表示真。
 */
void tobj_set_bool(tobj *v, int b);

/**
 * @brief 将目标值设置为 Integer。
 *
 * @param v 目标值。
 * @param i 整数值。
 */
void tobj_set_int(tobj *v, long i);

/**
 * @brief 将目标值设置为 Float。
 *
 * @param v 目标值。
 * @param d 浮点值。
 */
void tobj_set_float(tobj *v, double d);

/**
 * @brief 将目标值设置为引用对象。
 *
 * @param v 目标值。
 * @param compo 要由目标接管的对象引用。
 */
void tobj_set_compo(tobj *v, tcompo_v *compo);

/**
 * @brief 递减目标持有的引用对象计数，并把目标清为 Nil。
 *
 * @param v 目标值。
 */
void tobj_ddc_ref_clear(tobj *v);

/**
 * @brief 当目标持有引用对象时释放该引用，并把目标清为 Nil。
 *
 * @param v 目标值。
 */
void tobj_try_clear(tobj *v);

/**
 * @brief 复制值，并为引用对象增加引用计数。
 *
 * @param dst 目标值。
 * @param src 源值。
 */
void tobj_copy(tobj *dst, const tobj *src);

/**
 * @brief 判断两个值是否具有相同身份或相同立即值。
 *
 * @param a 左值。
 * @param b 右值。
 * @return 相同返回非零，否则返回零。
 */
int tobj_identical(const tobj *a, const tobj *b);

/**
 * @brief 通过对象能力执行索引读取。
 *
 * @param self 目标对象。
 * @param arguments 索引参数数组。
 * @param argument_count 参数数量。
 * @param result 接收读取结果。
 */
void tcompo_index(tcompo_v *self, const tobj *arguments,
		  uint_regs argument_count, tobj *result);

/**
 * @brief 通过对象能力执行索引写入。
 *
 * @param self 目标对象。
 * @param arguments 索引参数数组。
 * @param argument_count 参数数量。
 * @param value 要写入的值。
 */
void tcompo_index_set(tcompo_v *self, const tobj *arguments,
		      uint_regs argument_count, const tobj *value);

/**
 * @brief 通过对象能力追加一个值。
 *
 * @param self 目标对象。
 * @param value 要追加的值。
 */
void tcompo_append(tcompo_v *self, const tobj *value);

/**
 * @brief 通过对象能力删除一个键或索引对应的成员。
 *
 * @param self 目标对象。
 * @param key 键或索引值。
 */
void tcompo_delete(tcompo_v *self, const tobj *key);

/**
 * @brief 通过对象能力判断是否包含指定值。
 *
 * @param self 目标对象。
 * @param value 待判断的值。
 * @return 包含返回非零，否则返回零。
 */
int tcompo_contains(tcompo_v *self, const tobj *value);

/**
 * @brief 通过对象能力产生下一个迭代值。
 *
 * @param self 目标对象。
 * @param position 调用者持有的迭代游标。
 * @param result 接收下一个值。
 * @return 成功产生值返回 1，遍历结束返回 0。
 */
int tcompo_next(tcompo_v *self, long *position, tobj *result);

/**
 * @brief 查询对象精确的运行时 Type。
 *
 * @param self 目标对象。
 * @param result 接收 Type 值。
 * @return 对象实现该能力时返回非零，否则返回零。
 */
int tcompo_runtime_type(tcompo_v *self, tobj *result);

/**
 * @brief 让对象自行判断是否匹配指定 Type。
 *
 * @param self 目标对象。
 * @param type 待匹配的 Type 值。
 * @return 对象未接管匹配时返回 -1，不匹配返回 0，匹配返回 1。
 */
int tcompo_matches_type(tcompo_v *self, const tobj *type);

/**
 * @brief 创建包含对象类别与地址的文本表示。
 *
 * @param type 对象类别名称。
 * @param ptr 对象地址。
 * @return 新字符串，由调用者使用 tstring_free() 释放。
 */
tstring *tobj_tostring_pointer(const char *type, const void *ptr);

/**
 * @brief 创建值的简略文本表示。
 *
 * @param v Tapas 值。
 * @return 新字符串，由调用者使用 tstring_free() 释放。
 */
tstring *tobj_tostring_abbr(const tobj *v);

/**
 * @brief 创建值的完整文本表示。
 *
 * @param v Tapas 值。
 * @return 新字符串，由调用者使用 tstring_free() 释放。
 */
tstring *tobj_tostring_full(const tobj *v);

/**
 * @brief 读取 Integer 值。
 *
 * @param v Tapas 值；类型不符时报告运行时错误。
 * @return 整数值。
 */
long tobj_get_v_tint(const tobj *v);

/**
 * @brief 读取 Float 值。
 *
 * @param v Tapas 值；类型不符时报告运行时错误。
 * @return 浮点值。
 */
double tobj_get_v_tfloat(const tobj *v);

/**
 * @brief 读取 Boolean 值。
 *
 * @param v Tapas 值；类型不符时报告运行时错误。
 * @return 零表示假，非零表示真。
 */
int tobj_get_v_tbool(const tobj *v);

/**
 * @brief 读取引用对象指针。
 *
 * @param v Tapas 值；类型不符时报告运行时错误。
 * @return 由 `v` 持有的借用指针。
 */
tcompo_v *tobj_get_v_tcompo(const tobj *v);

/**
 * @brief 取得值的顶层存储类别。
 *
 * @param v Tapas 值。
 * @return 存储类别。
 */
ttypes tobj_get_type(const tobj *v);

/**
 * @brief 取得值关联的名称槽位。
 *
 * @param v Tapas 值。
 * @return 名称槽位或约定的未定义值。
 */
int tobj_get_name_loc(const tobj *v);

/**
 * @brief 判断值是否为 Nil。
 *
 * @param v Tapas 值。
 * @return 是 Nil 返回非零，否则返回零。
 */
int tobj_is_nil(const tobj *v);

/**
 * @brief 取得引用对象的运行时表示编号。
 *
 * @param v Tapas 值；不是引用对象时报告运行时错误。
 * @return 引用对象表示编号。
 */
tcompo_type tobj_compo_type(const tobj *v);

/**
 * @brief 取得引用对象表示编号对应的名称。
 *
 * @param v 引用对象值。
 * @return 静态只读名称。
 */
const char *tobj_compo_type_name(const tobj *v);

#ifdef __cplusplus
}
#endif

#endif /* TAPAS_TVAL_H */
