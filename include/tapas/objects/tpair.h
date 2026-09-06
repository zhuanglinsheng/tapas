/**
 * @file tpair.h
 * @brief 声明核心 Pair 对象。
 *
 * @details 定义保存两个 Tapas 值的运行时布局及其构造接口。
 *
 * @note Pair 的语言操作由对象 vtable 与标准库函数提供，本文件只描述核心表示。
 */
#ifndef TAPAS_OBJECTS_TPAIR_H
#define TAPAS_OBJECTS_TPAIR_H

#include "tapas/tval.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 对象布局 */

/** Pair 对象的运行时布局。 */
struct tpair {
	tcompo_v base; /**< 引用对象共有的对象头。 */
	tobj first;    /**< 第一个值，由 Pair 持有。 */
	tobj second;   /**< 第二个值，由 Pair 持有。 */
};

/* 共享 vtable */

/** Pair 对象使用的共享 vtable。 */
extern tcompo_vtable tpair_vtable;

/* 构造 */

/**
 * @brief 创建包含两个值的 Pair。
 *
 * @param first 第一个值。
 * @param second 第二个值。
 * @return 新对象，由 Tapas 引用计数管理，并持有两个参数的独立引用。
 */
tpair *tpair_new(const tobj *first, const tobj *second);

#ifdef __cplusplus
}
#endif

#endif /* TAPAS_OBJECTS_TPAIR_H */
