/**
 * @file tbycs.h
 * @brief 声明编译器与 VM 共享的字节码协议和容器。
 *
 * @details 定义单条指令的编码、指令序列、常量池、编译资源信息，以及保存或加载
 * 字节码文件所使用的封装结构。
 *
 * @note 这些结构构成编译产物与运行时之间的二进制协议。修改字段布局、编号宽度或
 * 编码方式时，必须同步更新编译器、VM 和序列化实现。
 */
#ifndef TAPAS_TBYCS_H
#define TAPAS_TBYCS_H

#include "tapas/basic_defs/tbasis.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 单条字节码 */

/** 一条 32 位字节码指令。 */
typedef uint32_t tbycode;

/* 指令构造 */

/**
 * @brief 使用操作码创建无操作数指令。
 *
 * @param ins 操作码。
 * @return 编码后的指令。
 */
tbycode tbycode_make(uint8_t ins);

/**
 * @brief 使用操作码和 U 操作数创建指令。
 *
 * @param ins 操作码。
 * @param u U 操作数。
 * @return 编码后的指令。
 */
tbycode tbycode_make_u(uint8_t ins, uint32_t u);

/**
 * @brief 使用操作码及 L、R 操作数创建指令。
 *
 * @param ins 操作码。
 * @param L L 操作数。
 * @param R R 操作数。
 * @return 编码后的指令。
 */
tbycode tbycode_make_lr(uint8_t ins, uint16_t L, uint16_t R);

/**
 * @brief 使用操作码及 L、b、i 操作数创建指令。
 *
 * @param ins 操作码。
 * @param L L 操作数。
 * @param b b 操作数。
 * @param i i 操作数。
 * @return 编码后的指令。
 */
tbycode tbycode_make_lbi(uint8_t ins, uint16_t L, uint8_t b, uint8_t i);

/* 指令解码 */

/**
 * @brief 取得指令的操作码。
 *
 * @param c 指令。
 * @return 操作码。
 */
static inline tins tbycode_ins(tbycode c)
{
	return (tins)(c & 0x3fU);
}

/**
 * @brief 取得指令的 U 操作数。
 *
 * @param c 指令。
 * @return U 操作数。
 */
static inline uint32_t tbycode_get_U(tbycode c)
{
	return c >> 6;
}

/**
 * @brief 取得指令的 L 操作数。
 *
 * @param c 指令。
 * @return L 操作数。
 */
static inline uint16_t tbycode_get_L(tbycode c)
{
	return (uint16_t)((c >> 6) & 0x1fffU);
}

/**
 * @brief 取得指令的 R 操作数。
 *
 * @param c 指令。
 * @return R 操作数。
 */
static inline uint16_t tbycode_get_R(tbycode c)
{
	return (uint16_t)(c >> 19);
}

/**
 * @brief 取得指令的 b 操作数。
 *
 * @param c 指令。
 * @return b 操作数。
 */
static inline uint8_t tbycode_get_b(tbycode c)
{
	return (uint8_t)((c >> 19) & 0xffU);
}

/**
 * @brief 取得指令的 i 操作数。
 *
 * @param c 指令。
 * @return i 操作数。
 */
static inline uint8_t tbycode_get_i(tbycode c)
{
	return (uint8_t)(c >> 27);
}

/**
 * @brief 将一条指令转换为便于诊断的文本。
 *
 * @param c 指令。
 * @param buf 接收文本的缓冲区。
 * @param buf_size 缓冲区大小。
 */
void tbycode_tostring(tbycode c, char *buf, size_t buf_size);

/* 指令序列 */

/** 一条字节码对应的源码位置。 */
typedef struct {
	tstring *source; /**< 源码文本，由位置记录持有。 */
	tstring *file;   /**< 文件名，由位置记录持有。 */
	uint64_t line;   /**< 从一开始的行号。 */
	uint64_t column; /**< 从一开始的列号。 */
} tsource_loc;

/** 可增长的字节码指令序列。 */
typedef struct {
	tbycode *data;     /**< 连续指令存储区。 */
	tsource_loc *locs; /**< 与指令一一对应的源码位置。 */
	uint32_t size;     /**< 当前指令数量。 */
	uint32_t capacity; /**< 已分配容量。 */
} tvmcmd_vect;

/**
 * @brief 初始化空指令序列。
 *
 * @param v 尚未初始化的目标存储。
 */
void tvmcmd_vect_init(tvmcmd_vect *v);

/**
 * @brief 释放指令序列持有的存储。
 *
 * @param v 已初始化的指令序列。
 */
void tvmcmd_vect_free(tvmcmd_vect *v);

/**
 * @brief 取得可由字节码协议表示的指令数量。
 *
 * @param v 指令序列。
 * @return 当前指令数量。
 */
uint_cmds tvmcmd_vect_size32(tvmcmd_vect *v);

/**
 * @brief 取得最后一条指令。
 *
 * @param v 非空指令序列。
 * @return 最后一条指令。
 */
tbycode tvmcmd_vect_back(tvmcmd_vect *v);

/**
 * @brief 删除最后一条指令。
 *
 * @param v 非空指令序列。
 */
void tvmcmd_vect_pop_back(tvmcmd_vect *v);

/**
 * @brief 在序列末尾追加一条指令。
 *
 * @param v 目标序列。
 * @param cmd 要追加的指令。
 */
void tvmcmd_vect_append(tvmcmd_vect *v, tbycode cmd);

/**
 * @brief 在指定位置插入连续指令。
 *
 * @param v 目标序列。
 * @param pos 插入位置。
 * @param src 源指令数组。
 * @param count 插入数量。
 */
void tvmcmd_vect_insert_range(tvmcmd_vect *v, uint32_t pos, tbycode *src, uint32_t count);

/**
 * @brief 在指定位置插入另一条指令序列。
 *
 * @param v 目标序列。
 * @param pos 插入位置。
 * @param src 源序列。
 */
void tvmcmd_vect_insert_vect(tvmcmd_vect *v, uint32_t pos, const tvmcmd_vect *src);

/**
 * @brief 将指定范围内的循环控制标记解析为跳转目标。
 *
 * @param v 指令序列。
 * @param begin 待处理范围的起点。
 * @param end 待处理范围的终点。
 * @param continue_target `continue` 的目标位置。
 * @param break_target `break` 的目标位置。
 * @param continue_marker `continue` 临时标记。
 * @param break_marker `break` 临时标记。
 */
void tvmcmd_vect_resolve_loop_control(tvmcmd_vect *v, uint32_t begin,
				      uint32_t end, uint32_t continue_target,
				      uint32_t break_target,
				      uint8_t continue_marker,
				      uint8_t break_marker);

/* 常量池 */

/** 字符串常量的可增长数组。 */
typedef struct {
	tstring **data;   /**< 字符串常量数组。 */
	uint32_t size;     /**< 当前常量数量。 */
	uint32_t capacity; /**< 已分配容量。 */
} consts_str_vect;

/**
 * @brief 初始化空字符串常量数组。
 *
 * @param v 尚未初始化的目标存储。
 */
void consts_str_vect_init(consts_str_vect *v);

/**
 * @brief 释放字符串常量数组及其持有的字符串。
 *
 * @param v 已初始化的数组。
 */
void consts_str_vect_free(consts_str_vect *v);

/**
 * @brief 加入字符串常量。
 *
 * @param v 目标数组。
 * @param str 要复制的字符串。
 * @return 常量池索引。
 */
uint_csts consts_str_vect_add(consts_str_vect *v, const char *str);

/** 整数常量的可增长数组。 */
typedef struct {
	long *data;        /**< 整数常量数组。 */
	uint32_t size;     /**< 当前常量数量。 */
	uint32_t capacity; /**< 已分配容量。 */
} consts_long_vect;

/**
 * @brief 初始化空整数常量数组。
 *
 * @param v 尚未初始化的目标存储。
 */
void consts_long_vect_init(consts_long_vect *v);

/**
 * @brief 释放整数常量数组。
 *
 * @param v 已初始化的数组。
 */
void consts_long_vect_free(consts_long_vect *v);

/**
 * @brief 加入整数常量。
 *
 * @param v 目标数组。
 * @param val 常量值。
 * @return 常量池索引。
 */
uint_csts consts_long_vect_add(consts_long_vect *v, long val);

/** 浮点常量的可增长数组。 */
typedef struct {
	double *data;      /**< 浮点常量数组。 */
	uint32_t size;     /**< 当前常量数量。 */
	uint32_t capacity; /**< 已分配容量。 */
} consts_float_vect;

/**
 * @brief 初始化空浮点常量数组。
 *
 * @param v 尚未初始化的目标存储。
 */
void consts_float_vect_init(consts_float_vect *v);

/**
 * @brief 释放浮点常量数组。
 *
 * @param v 已初始化的数组。
 */
void consts_float_vect_free(consts_float_vect *v);

/**
 * @brief 加入浮点常量。
 *
 * @param v 目标数组。
 * @param val 常量值。
 * @return 常量池索引。
 */
uint_csts consts_float_vect_add(consts_float_vect *v, double val);

/** 汇总字符串、整数和浮点常量的常量池。 */
typedef struct {
	consts_str_vect __strcsts;  /**< 字符串常量。 */
	consts_long_vect __intcsts; /**< 整数常量。 */
	consts_float_vect __fltcsts; /**< 浮点常量。 */
} tconsts;

/**
 * @brief 初始化空常量池。
 *
 * @param c 尚未初始化的目标存储。
 */
void tconsts_init(tconsts *c);

/**
 * @brief 释放常量池持有的全部存储。
 *
 * @param c 已初始化的常量池。
 */
void tconsts_free(tconsts *c);

/**
 * @brief 加入字符串常量。
 *
 * @param c 目标常量池。
 * @param str 要复制的字符串。
 * @return 字符串常量索引。
 */
uint_csts tconsts_add_str_const(tconsts *c, const char *str);

/**
 * @brief 加入整数常量。
 *
 * @param c 目标常量池。
 * @param val 常量值。
 * @return 整数常量索引。
 */
uint_csts tconsts_add_int_const(tconsts *c, long val);

/**
 * @brief 加入浮点常量。
 *
 * @param c 目标常量池。
 * @param val 常量值。
 * @return 浮点常量索引。
 */
uint_csts tconsts_add_float_const(tconsts *c, double val);

/**
 * @brief 将源常量池完整复制到已初始化的目标常量池。
 *
 * @param dst 目标常量池。
 * @param src 源常量池。
 */
void tconsts_copy(tconsts *dst, tconsts *src);

/* 编译资源信息 */

/** 执行一段字节码所需的最大运行时资源。 */
typedef struct {
	uint_objs obj_max; /**< 所需对象槽位上限。 */
	uint_objs tmp_max; /**< 所需临时槽位上限。 */
	uint_regs reg_max; /**< 所需寄存器上限。 */
	uint16_t padding_1; /**< 为稳定布局保留的填充字段。 */
} tcinfo;

/* 可序列化字节码封装 */

/** 可序列化常量池的连续数组表示。 */
typedef struct {
	uint_csts ncints; /**< 整数常量数量。 */
	uint_csts ncstrs; /**< 字符串常量数量。 */
	uint_csts ncflts; /**< 浮点常量数量。 */
	long *cints;      /**< 整数常量数组。 */
	tstring **cstrs;  /**< 字符串常量数组。 */
	double *cflts;    /**< 浮点常量数组。 */
} twrapper_consts;

/** 可保存到 `.tapc` 文件或交给 VM 执行的字节码封装。 */
typedef struct {
	twrapper_consts consts;     /**< 常量池快照。 */
	tbycode *cmdarr;            /**< 指令数组。 */
	tsource_loc *source_locs;   /**< 指令对应的源码位置数组。 */
	tcinfo info;                /**< 执行所需资源信息。 */
	uint_cmds ncmds;            /**< 指令数量。 */
} twrapper;

/* 字节码封装管理 */

/**
 * @brief 根据指令、常量池和资源信息创建可序列化封装。
 *
 * @param tcmds 指令序列。
 * @param consts 常量池。
 * @param info 编译资源信息。
 * @return 新封装，由调用者使用 tanalyser_clean_wrapper() 释放。
 */
twrapper *tanalyser_wrap(tvmcmd_vect *tcmds, tconsts *consts, tcinfo *info);

/**
 * @brief 将字节码封装保存到二进制文件。
 *
 * @param wrapper 字节码封装。
 * @param filename 目标文件路径。
 * @return 成功返回零，失败返回非零。
 */
int tanalyser_save_bin_file(const twrapper *wrapper, const char *filename);

/**
 * @brief 从二进制文件加载字节码封装。
 *
 * @param filename 源文件路径。
 * @return 新封装；失败返回空指针。
 */
twrapper *tanalyser_load_bin_file(const char *filename);

/**
 * @brief 释放字节码封装及其持有的全部存储。
 *
 * @param wrapper 可为空的字节码封装。
 */
void tanalyser_clean_wrapper(twrapper *wrapper);

/**
 * @brief 将字节码封装以可读形式输出到标准输出。
 *
 * @param wrapper 字节码封装。
 */
void tanalyser_display_wrapper(const twrapper *wrapper);

#ifdef __cplusplus
}
#endif

#endif /* TAPAS_TBYCS_H */
