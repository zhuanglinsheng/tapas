/**
 * @file ttime.h
 * @brief 声明核心 Time 引用对象。
 *
 * @details 定义基于 `time_t` 的运行时布局、构造接口和原生时间值读取接口。
 *
 * @note 日历计算及面向语言的时间操作属于 time 标准包，不应进入核心对象层。
 */
#ifndef TAPAS_OBJECTS_TTIME_H
#define TAPAS_OBJECTS_TTIME_H

#include "tapas/tval.h"

#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 对象布局 */

/** Time 对象的运行时布局。 */
struct ttime {
	tcompo_v base; /**< 引用对象共有的对象头。 */
	time_t value;  /**< 原生时间值。 */
};

/* 共享 vtable */

/** Time 对象使用的共享 vtable。 */
extern tcompo_vtable ttime_vtable;

/* 构造 */

/**
 * @brief 使用当前时间创建 Time 对象。
 *
 * @return 新对象，由 Tapas 引用计数管理。
 */
ttime *ttime_new(void);

/**
 * @brief 根据原生时间值创建 Time 对象。
 *
 * @param value 原生时间值。
 * @return 新对象，由 Tapas 引用计数管理。
 */
ttime *ttime_from_time(time_t value);

/* 查询 */

/**
 * @brief 取得 Time 对象保存的原生时间值。
 *
 * @param value Time 对象。
 * @return 保存的 `time_t` 值。
 */
time_t ttime_get(const ttime *value);

#ifdef __cplusplus
}
#endif

#endif /* TAPAS_OBJECTS_TTIME_H */
