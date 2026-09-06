/**
 * @file tdict.h
 * @brief 声明核心 Dictionary 对象。
 *
 * @details 定义字典的运行时布局，以及基于 Tapas 值的创建、查询、写入和成员判断
 * 接口。
 *
 * @note 哈希桶等实现细节由不透明的 `thashtbl` 指针隐藏。
 */
#ifndef TAPAS_OBJECTS_TDICT_H
#define TAPAS_OBJECTS_TDICT_H

#include "tapas/tval.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 对象布局 */

/** 字典内部使用的不透明哈希表。 */
typedef struct thashtbl thashtbl;

/** Dictionary 对象的运行时布局。 */
struct tdict {
	tcompo_v base;   /**< 所有引用对象共有的对象头。 */
	thashtbl *items; /**< 保存键值对的哈希表。 */
};

/* 共享 vtable */

/** Dictionary 对象使用的共享 vtable。 */
extern tcompo_vtable tdict_vtable;

/* 构造 */

/**
 * @brief 创建空字典。
 *
 * @return 新对象，由 Tapas 引用计数管理。
 */
tdict *tdict_new(void);

/* 能力 */

/**
 * @brief 写入或替换键值对。
 *
 * @param d 字典。
 * @param key 要保存的键。
 * @param val 要保存的值。
 */
void tdict_set(tdict *d, const tobj *key, const tobj *val);

/**
 * @brief 查询键对应的值。
 *
 * @param d 字典。
 * @param key 要查询的键。
 * @param vre 接收查询结果；键不存在时接收 Nil。
 */
void tdict_get(tdict *d, const tobj *key, tobj *vre);

/**
 * @brief 判断字典是否包含指定键。
 *
 * @param d 字典。
 * @param key 要查询的键。
 * @return 包含时返回非零，否则返回零。
 */
int tdict_contains(tdict *d, const tobj *key);

#ifdef __cplusplus
}
#endif

#endif /* TAPAS_OBJECTS_TDICT_H */
