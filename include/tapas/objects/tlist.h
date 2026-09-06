/**
 * @file tlist.h
 * @brief 声明核心 List 对象。
 *
 * @details 定义列表的运行时存储，以及创建、长度查询、索引读取和元素替换接口。
 *
 * @note 更高层的列表算法属于标准库包，不应加入本核心对象头文件。
 */
#ifndef TAPAS_OBJECTS_TLIST_H
#define TAPAS_OBJECTS_TLIST_H

#include "tapas/dsa/tobj_vec.h"
#include "tapas/tval.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 对象布局 */

/** List 对象的运行时布局。 */
struct tlist {
	tcompo_v base; /**< 引用对象共有的对象头。 */
	tobj_vec items; /**< 对象拥有的有序元素集合。 */
};

/* 共享 vtable */

/** List 对象使用的共享 vtable。 */
extern tcompo_vtable tlist_vtable;

/* 构造 */

/**
 * @brief 创建空列表。
 *
 * @return 新对象，由 Tapas 引用计数管理。
 */
tlist *tlist_new(void);

/* 能力 */

/**
 * @brief 取得列表的元素数量。
 *
 * @param l 列表。
 * @return 元素数量。
 */
uint_objs tlist_size(const tlist *l);

/**
 * @brief 取得指定位置的只读元素。
 *
 * @param l 列表。
 * @param idx 有效的元素索引。
 * @return 由列表持有的借用指针。
 */
const tobj *tlist_at(const tlist *l, uint_objs idx);

/**
 * @brief 替换指定位置的元素。
 *
 * @param l 列表。
 * @param idx 有效的元素索引。
 * @param v 新值；列表取得独立引用。
 */
void tlist_set_at(tlist *l, uint_objs idx, const tobj *v);

#ifdef __cplusplus
}
#endif

#endif /* TAPAS_OBJECTS_TLIST_H */
