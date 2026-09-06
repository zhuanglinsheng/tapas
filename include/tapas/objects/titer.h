/**
 * @file titer.h
 * @brief 声明核心整数迭代器对象。
 *
 * @details 定义有界整数序列的迭代状态，以及使用默认步长或显式步长的构造接口。
 *
 * @note 迭代行为通过通用对象能力对外提供，调用者不应直接修改状态字段。
 */
#ifndef TAPAS_OBJECTS_TITER_H
#define TAPAS_OBJECTS_TITER_H

#include "tapas/tval.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 对象布局 */

/** 整数迭代器的运行时布局。 */
struct titer {
	tcompo_v base; /**< 引用对象共有的对象头。 */
	long start;    /**< 序列起点。 */
	long end;      /**< 序列终点。 */
	long step;     /**< 每次迭代的增量。 */
};

/* 共享 vtable */

/** 整数迭代器对象使用的共享 vtable。 */
extern tcompo_vtable titer_vtable;

/* 构造 */

/**
 * @brief 创建带显式步长的整数迭代器。
 *
 * @param start 序列起点。
 * @param step 每次迭代的增量。
 * @param end 序列终点。
 * @return 新对象，由 Tapas 引用计数管理。
 */
titer *titer_new_step(long start, long step, long end);

/**
 * @brief 创建使用默认步长的整数迭代器。
 *
 * @param start 序列起点。
 * @param end 序列终点。
 * @return 新对象，由 Tapas 引用计数管理。
 */
titer *titer_new(long start, long end);

#ifdef __cplusplus
}
#endif

#endif /* TAPAS_OBJECTS_TITER_H */
