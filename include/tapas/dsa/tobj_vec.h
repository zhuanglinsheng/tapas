/**
 * @file tobj_vec.h
 * @brief 声明保存 Tapas 值的动态数组。
 *
 * @details 容器负责元素引用的复制与释放，可用于运行时对象内部的有序值集合。
 *
 * @note `tobj_vec` 由调用者提供存储并显式初始化、释放；越界访问属于调用错误。
 */
#ifndef TAPAS_DSA_TOBJ_VEC_H
#define TAPAS_DSA_TOBJ_VEC_H

#include "tapas/basic_defs/tbasis.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tobj tobj; /**< Tapas 值，完整定义见 `tval.h`。 */

/** @brief Tapas 值动态数组的公开布局。 */
typedef struct {
	tobj *data;          /**< 连续元素存储区。 */
	uint_objs len;       /**< 当前元素数量。 */
	uint_objs capacity;  /**< 已分配的元素容量。 */
} tobj_vec;

/**
 * @brief 将调用者提供的数组初始化为空。
 *
 * @param v 待初始化数组。
 */
void tobj_vec_init(tobj_vec *v);

/**
 * @brief 按指定初始容量初始化数组。
 *
 * @param v 待初始化数组。
 * @param cap 初始容量。
 */
void tobj_vec_init_cap(tobj_vec *v, uint_objs cap);

/**
 * @brief 释放元素引用和动态存储区，但不释放 v 本身。
 *
 * @param v 已初始化数组。
 */
void tobj_vec_free(tobj_vec *v);

/**
 * @brief 返回元素数量。
 *
 * @param v 数组。
 * @return 当前长度。
 */
uint_objs tobj_vec_len(const tobj_vec *v);

/**
 * @brief 返回已分配容量。
 *
 * @param v 数组。
 * @return 当前容量。
 */
uint_objs tobj_vec_cap(const tobj_vec *v);

/**
 * @brief 取得可修改元素指针。
 *
 * @param v 数组。
 * @param idx 索引。
 * @return 第 idx 个元素。
 */
tobj *tobj_vec_at(tobj_vec *v, uint_objs idx);

/**
 * @brief 取得只读元素指针。
 *
 * @param v 数组。
 * @param idx 索引。
 * @return 第 idx 个元素。
 */
const tobj *tobj_vec_at_const(const tobj_vec *v, uint_objs idx);

/**
 * @brief 取得连续存储区首地址。
 *
 * @param v 数组。
 * @return 数据指针；空数组也可能非空。
 */
tobj *tobj_vec_data(tobj_vec *v);

/**
 * @brief 确保容量不少于指定值。
 *
 * @param v 数组。
 * @param cap 所需最小容量。
 */
void tobj_vec_reserve(tobj_vec *v, uint_objs cap);

/**
 * @brief 在末尾复制加入一个值。
 *
 * @param v 数组。
 * @param obj 要加入的值。
 */
void tobj_vec_push(tobj_vec *v, const tobj *obj);

/**
 * @brief 替换指定元素并正确更新引用。
 *
 * @param v 数组。
 * @param idx 索引。
 * @param obj 新值。
 */
void tobj_vec_set(tobj_vec *v, uint_objs idx, const tobj *obj);

/**
 * @brief 在指定位置插入值。
 *
 * @param v 数组。
 * @param idx 插入位置。
 * @param obj 要复制的值。
 */
void tobj_vec_insert(tobj_vec *v, uint_objs idx, const tobj *obj);

/**
 * @brief 删除指定元素并释放其引用。
 *
 * @param v 数组。
 * @param idx 索引。
 */
void tobj_vec_pop(tobj_vec *v, uint_objs idx);

/**
 * @brief 从数组中移出元素而不额外复制引用。
 *
 * @param v 数组。
 * @param idx 索引。
 * @param result 接收被移出值的已分配位置；调用者随后拥有该值。
 */
void tobj_vec_take(tobj_vec *v, uint_objs idx, tobj *result);

/**
 * @brief 复制整个数组。
 *
 * @param dst 已初始化的目标数组。
 * @param src 源数组。
 */
void tobj_vec_copy(tobj_vec *dst, const tobj_vec *src);

/**
 * @brief 复制源数组中的连续区间。
 *
 * @param dst 已初始化的目标数组。
 * @param src 源数组。
 * @param start 首个元素索引。
 * @param count 要复制的元素数量。
 */
void tobj_vec_copy_range(tobj_vec *dst, const tobj_vec *src,
			 uint_objs start, uint_objs count);

#ifdef __cplusplus
}
#endif

#endif /* TAPAS_DSA_TOBJ_VEC_H */
