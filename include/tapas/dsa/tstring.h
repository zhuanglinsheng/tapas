/**
 * @file tstring.h
 * @brief 声明带长度缓存的小型动态字节字符串。
 *
 * @details `tstring` 以空字符结尾并缓存字节长度，短内容直接保存在结构体内；接口
 * 提供构造、拼接、比较、查找、修改、输入输出和数值转换等通用操作。
 *
 * @note 本类型按字节处理内容，不负责验证或解释 UTF-8。返回的内部指针可能在
 * 字符串发生修改后失效。
 */
#ifndef TAPAS_DSA_TSTRING_H
#define TAPAS_DSA_TSTRING_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 短字符串直接存储区的字节容量。 */
#define TSTRING_INLINE_CAP 16

/** 带长度缓存和短字符串优化的可变字节字符串。 */
typedef struct tstring {
	char *data;                           /**< 当前数据区，始终以空字符结尾。 */
	size_t len;                           /**< 内容字节数，不包含末尾空字符。 */
	size_t cap;                           /**< 数据区可容纳的内容字节数。 */
	char inline_data[TSTRING_INLINE_CAP]; /**< 结构体内的短字符串存储区。 */
} tstring;

/* 构造与销毁 */

/**
 * @brief 用 C 字符串初始化调用者提供的 `tstring` 存储。
 *
 * @param s 尚未初始化的目标存储。
 * @param data 以空字符结尾的源字符串。
 * @return 成功返回 `true`，分配失败返回 `false`。
 */
bool tstring_init(tstring *s, const char *data);

/**
 * @brief 用指定长度的字节序列初始化调用者提供的 `tstring` 存储。
 *
 * @param s 尚未初始化的目标存储。
 * @param data 源字节序列。
 * @param len 要复制的字节数。
 * @return 成功返回 `true`，分配失败返回 `false`。
 */
bool tstring_init_len(tstring *s, const char *data, size_t len);

/**
 * @brief 释放动态缓冲区，但不释放 `tstring` 结构体本身。
 *
 * @param s 已初始化的字符串。
 */
void tstring_deinit(tstring *s);

/**
 * @brief 根据以空字符结尾的 C 字符串创建新字符串。
 *
 * @param s 源字符串。
 * @return 新字符串，由调用者使用 tstring_free() 释放。
 */
tstring *tstring_new(const char *s);

/**
 * @brief 根据指定长度的字节序列创建新字符串。
 *
 * @param s 源字节序列。
 * @param len 要复制的字节数。
 * @return 新字符串，由调用者使用 tstring_free() 释放。
 */
tstring *tstring_new_len(const char *s, size_t len);

/**
 * @brief 创建具有指定初始容量的空字符串。
 *
 * @param cap 初始容量。
 * @return 新字符串，由调用者使用 tstring_free() 释放。
 */
tstring *tstring_new_cap(size_t cap);

/**
 * @brief 创建空字符串。
 *
 * @return 新字符串，由调用者使用 tstring_free() 释放。
 */
tstring *tstring_new_empty(void);

/**
 * @brief 完整复制字符串。
 *
 * @param s 源字符串。
 * @return 内容独立的新字符串，由调用者使用 tstring_free() 释放。
 */
tstring *tstring_dup(const tstring *s);

/**
 * @brief 释放字符串及其数据。
 *
 * @param s 可为空的字符串指针。
 */
void tstring_free(tstring *s);

/* 读取 */

/**
 * @brief 取得内部以空字符结尾的只读 C 字符串。
 *
 * @param s 字符串。
 * @return 由 `s` 持有的内部指针，修改 `s` 后可能失效。
 */
const char *tstring_cstr(const tstring *s);

/**
 * @brief 取得内容的字节长度。
 *
 * @param s 字符串。
 * @return 不包含末尾空字符的字节数。
 */
size_t tstring_len(const tstring *s);

/**
 * @brief 取得当前容量。
 *
 * @param s 字符串。
 * @return 数据区可容纳的内容字节数。
 */
size_t tstring_cap(const tstring *s);

/**
 * @brief 判断字符串是否为空。
 *
 * @param s 字符串。
 * @return 为空返回 `true`，否则返回 `false`。
 */
bool tstring_empty(const tstring *s);

/**
 * @brief 读取指定索引的字节。
 *
 * @param s 字符串。
 * @param i 字节索引，必须小于字符串长度。
 * @return 指定位置的字节。
 */
char tstring_at(const tstring *s, size_t i);

/* 赋值 */

/**
 * @brief 用 C 字符串替换原内容。
 *
 * @param s 目标字符串。
 * @param str 以空字符结尾的源字符串。
 */
void tstring_assign(tstring *s, const char *str);

/**
 * @brief 用指定长度的字节序列替换原内容。
 *
 * @param s 目标字符串。
 * @param str 源字节序列。
 * @param len 要复制的字节数。
 */
void tstring_assign_len(tstring *s, const char *str, size_t len);

/**
 * @brief 复制另一个 `tstring` 的内容。
 *
 * @param dst 目标字符串。
 * @param src 源字符串。
 */
void tstring_assign_ts(tstring *dst, const tstring *src);

/* 追加 */

/**
 * @brief 在末尾追加 C 字符串。
 *
 * @param s 目标字符串。
 * @param str 追加内容。
 */
void tstring_append(tstring *s, const char *str);

/**
 * @brief 在末尾追加指定长度的字节序列。
 *
 * @param s 目标字符串。
 * @param str 追加内容。
 * @param len 要追加的字节数。
 */
void tstring_append_len(tstring *s, const char *str, size_t len);

/**
 * @brief 在末尾追加一个字节。
 *
 * @param s 目标字符串。
 * @param c 要追加的字节。
 */
void tstring_append_c(tstring *s, char c);

/**
 * @brief 在末尾追加另一个 `tstring`。
 *
 * @param s 目标字符串。
 * @param other 追加内容。
 */
void tstring_append_ts(tstring *s, const tstring *other);

/**
 * @brief 按 `printf` 格式在末尾追加文本。
 *
 * @param s 目标字符串。
 * @param fmt 格式字符串。
 * @param ... 与格式字符串对应的参数。
 */
void tstring_append_fmt(tstring *s, const char *fmt, ...)
	__attribute__((format(printf, 2, 3)));

/* 比较 */

/**
 * @brief 按字典序比较两个字符串。
 *
 * @param a 左操作数。
 * @param b 右操作数。
 * @return 小于、等于或大于零，含义与 `strcmp()` 相同。
 */
int tstring_cmp(const tstring *a, const tstring *b);

/**
 * @brief 将 `tstring` 与 C 字符串按字典序比较。
 *
 * @param a 左操作数。
 * @param b 右操作数。
 * @return 小于、等于或大于零，含义与 `strcmp()` 相同。
 */
int tstring_cmp_cstr(const tstring *a, const char *b);

/**
 * @brief 通过统一接口比较两个 C 字符串。
 *
 * @param a 左操作数。
 * @param b 右操作数。
 * @return 小于、等于或大于零，含义与 `strcmp()` 相同。
 */
int tstring_cmp_cstr_cstr(const char *a, const char *b);

/**
 * @brief 判断两个字符串是否相等。
 *
 * @param a 左操作数。
 * @param b 右操作数。
 * @return 相等返回 `true`，否则返回 `false`。
 */
bool tstring_eq(const tstring *a, const tstring *b);

/**
 * @brief 判断 `tstring` 是否等于 C 字符串。
 *
 * @param a 左操作数。
 * @param b 右操作数。
 * @return 相等返回 `true`，否则返回 `false`。
 */
bool tstring_eq_cstr(const tstring *a, const char *b);

/**
 * @brief 判断两个 C 字符串是否相等。
 *
 * @param a 左操作数。
 * @param b 右操作数。
 * @return 相等返回 `true`，否则返回 `false`。
 */
bool tstring_eq_cstr_cstr(const char *a, const char *b);

/**
 * @brief 比较两个字符串开头至多 `n` 个字节。
 *
 * @param a 左操作数。
 * @param b 右操作数。
 * @param n 最大比较字节数。
 * @return 指定范围相等返回 `true`，否则返回 `false`。
 */
bool tstring_eq_n(const tstring *a, const tstring *b, size_t n);

/**
 * @brief 将字符串开头至多 `n` 个字节与 C 字符串比较。
 *
 * @param a 左操作数。
 * @param b 右操作数。
 * @param n 最大比较字节数。
 * @return 指定范围相等返回 `true`，否则返回 `false`。
 */
bool tstring_eq_n_cstr(const tstring *a, const char *b, size_t n);

/* 查找 */

/**
 * @brief 从指定位置查找首个匹配字节。
 *
 * @param s 字符串。
 * @param c 目标字节。
 * @param pos 起始索引。
 * @return 匹配索引，未找到时返回 `SIZE_MAX`。
 */
size_t tstring_find_c(const tstring *s, char c, size_t pos);

/**
 * @brief 反向查找最后一个匹配字节。
 *
 * @param s 字符串。
 * @param c 目标字节。
 * @return 匹配索引，未找到时返回 `SIZE_MAX`。
 */
size_t tstring_rfind_c(const tstring *s, char c);

/**
 * @brief 从指定位置查找子串。
 *
 * @param s 字符串。
 * @param sub 目标子串。
 * @param pos 起始索引。
 * @return 首个匹配索引，未找到时返回 `SIZE_MAX`。
 */
size_t tstring_find(const tstring *s, const char *sub, size_t pos);

/**
 * @brief 判断字符串是否包含指定子串。
 *
 * @param s 字符串。
 * @param sub 目标子串。
 * @return 包含返回 `true`，否则返回 `false`。
 */
bool tstring_contains(const tstring *s, const char *sub);

/**
 * @brief 判断字符串是否以指定内容开头。
 *
 * @param s 字符串。
 * @param prefix 前缀。
 * @return 匹配返回 `true`，否则返回 `false`。
 */
bool tstring_starts_with(const tstring *s, const char *prefix);

/**
 * @brief 判断字符串是否以指定内容结尾。
 *
 * @param s 字符串。
 * @param suffix 后缀。
 * @return 匹配返回 `true`，否则返回 `false`。
 */
bool tstring_ends_with(const tstring *s, const char *suffix);

/* 截取与复制 */

/**
 * @brief 复制指定范围并创建新字符串。
 *
 * @param s 源字符串。
 * @param pos 起始索引。
 * @param len 最大复制字节数。
 * @return 新字符串，由调用者使用 tstring_free() 释放。
 */
tstring *tstring_substr(const tstring *s, size_t pos, size_t len);

/**
 * @brief 将内容复制到调用者提供的缓冲区。
 *
 * @param s 源字符串。
 * @param buf 目标缓冲区。
 * @param bufsz 目标缓冲区大小。
 * @return 实际写入的内容字节数。
 */
size_t tstring_copy_to(const tstring *s, char *buf, size_t bufsz);

/* 修改 */

/**
 * @brief 原地删除末尾空白字符。
 *
 * @param s 目标字符串。
 */
void tstring_trim_back(tstring *s);

/**
 * @brief 原地删除开头空白字符。
 *
 * @param s 目标字符串。
 */
void tstring_trim_front(tstring *s);

/**
 * @brief 原地删除两端空白字符。
 *
 * @param s 目标字符串。
 */
void tstring_trim(tstring *s);

/**
 * @brief 清空内容并保留可复用的存储。
 *
 * @param s 目标字符串。
 */
void tstring_clear(tstring *s);

/**
 * @brief 确保容量不小于指定值。
 *
 * @param s 目标字符串。
 * @param cap 所需最小容量。
 */
void tstring_reserve(tstring *s, size_t cap);

/**
 * @brief 将容量收缩到适合当前内容的大小。
 *
 * @param s 目标字符串。
 */
void tstring_shrink_to_fit(tstring *s);

/**
 * @brief 删除并返回首字节。
 *
 * @param s 非空字符串。
 * @return 被删除的字节。
 */
char tstring_pop_front(tstring *s);

/**
 * @brief 删除并返回末字节。
 *
 * @param s 非空字符串。
 * @return 被删除的字节。
 */
char tstring_pop_back(tstring *s);

/**
 * @brief 原地替换全部匹配子串。
 *
 * @param s 目标字符串。
 * @param from 要替换的子串。
 * @param to 替换后的子串。
 */
void tstring_replace(tstring *s, const char *from, const char *to);

/**
 * @brief 替换指定索引的字节。
 *
 * @param s 目标字符串。
 * @param i 索引，必须小于字符串长度。
 * @param c 新字节。
 */
void tstring_set_at(tstring *s, size_t i, char c);

/* 遍历辅助接口 */

/**
 * @brief 取得数据区首字节的可修改指针。
 *
 * @param s 字符串。
 * @return 数据区首地址，修改字符串后可能失效。
 */
char *tstring_begin(tstring *s);

/**
 * @brief 取得数据区末字节后一位的指针。
 *
 * @param s 字符串。
 * @return 数据区尾后指针，修改字符串后可能失效。
 */
char *tstring_end(tstring *s);

/* 输入输出 */

/**
 * @brief 将内容原样写入文件流。
 *
 * @param s 字符串。
 * @param fp 目标文件流。
 */
void tstring_print(const tstring *s, FILE *fp);

/**
 * @brief 将内容和一个换行符写入文件流。
 *
 * @param s 字符串。
 * @param fp 目标文件流。
 */
void tstring_println(const tstring *s, FILE *fp);

/**
 * @brief 从文件流读取一行并替换原内容。
 *
 * @param s 接收内容的字符串。
 * @param fp 源文件流。
 * @return 读取的字符数；到达末尾或出错时返回 `SIZE_MAX`。
 */
size_t tstring_getline(tstring *s, FILE *fp);

/* 数值转换 */

/**
 * @brief 将完整内容解析为 `long`。
 *
 * @param s 待解析字符串。
 * @param out 接收结果。
 * @return 成功返回零，失败返回非零。
 */
int tstring_to_long(const tstring *s, long *out);

/**
 * @brief 将完整内容解析为 `double`。
 *
 * @param s 待解析字符串。
 * @param out 接收结果。
 * @return 成功返回零，失败返回非零。
 */
int tstring_to_double(const tstring *s, double *out);

/* 哈希 */

/**
 * @brief 按 djb2 算法计算适用于哈希表的 64 位哈希值。
 *
 * @param s 字符串。
 * @return 计算所得哈希值。
 */
uint64_t tstring_hash(const tstring *s);

#ifdef __cplusplus
}
#endif

#endif /* TAPAS_DSA_TSTRING_H */
