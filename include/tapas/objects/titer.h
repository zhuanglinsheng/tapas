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

#include <limits.h>

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

/**
 * @brief 判定第 pos 个迭代位置是否在范围内并给出其值。
 *
 * @details 步长与边界可为任意整数（包括 LONG_MIN 和零步长）；内部在
 * 64 位无符号域中比较，避免 `start + pos*step`、`end - start` 与
 * `-step` 的有符号溢出。pos 不在范围内时返回零且不写 value。
 * 内联定义让循环热路径可以在已知步长（如 1）下折叠掉多余分支。
 *
 * @param start 序列起点。
 * @param step 每次迭代的增量，可为任意非零整数。
 * @param end 序列终点。
 * @param pos 迭代位置（非负）。
 * @param value 接收当前值。
 * @return 位置在范围内返回非零。
 */
static inline unsigned long titer_abs_step(long step)
{
	return step > 0 ? (unsigned long)step : (unsigned long)(-(step + 1)) + 1;
}

static inline int titer_step_value(long start, long step, long end, long pos,
				   long *value)
{
	if (step == 0 || pos < 0)
		return 0;
	unsigned long span;
	if (step > 0) {
		if (start >= end)
			return 0;
		span = (unsigned long)end - (unsigned long)start;
	} else {
		if (start <= end)
			return 0;
		span = (unsigned long)start - (unsigned long)end;
	}
	unsigned long upos = (unsigned long)pos;
	unsigned long ustep = titer_abs_step(step);
	/* 乘积可能无符号回绕：回绕意味着 pos*step >= 2^64 > span，迭代已结束。 */
	if (upos != 0 && ustep > ULONG_MAX / upos)
		return 0;
	if (upos * ustep >= span)
		return 0;
	*value = start + pos * step;
	return 1;
}

#ifdef __cplusplus
}
#endif

#endif /* TAPAS_OBJECTS_TITER_H */
