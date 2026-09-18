/**
 * @file thashtbl.h
 * @brief 声明以 Tapas 值为键和值的哈希表。
 *
 * @details 提供字典对象使用的创建、查询、写入、删除、复制和遍历接口；键的
 * 哈希与相等语义沿用 Tapas 值协议。
 *
 * @note 哈希表拥有保存的键和值引用，但其内部桶布局不属于公共 ABI。
 */
#ifndef TAPAS_DSA_THASHTBL_H
#define TAPAS_DSA_THASHTBL_H

#include "tapas/basic_defs/tbasis.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tobj tobj;           /**< Tapas 值，完整定义见 `tval.h`。 */
typedef struct thashtbl thashtbl;   /**< 不透明哈希表实例。 */

/**
 * @brief 哈希表遍历回调。
 *
 * @param key 当前键，只在回调期间有效。
 * @param value 当前值，只在回调期间有效。
 * @param ctx 调用 thashtbl_each() 时传入的用户上下文。
 */
typedef void (*thashtbl_each_fn)(const tobj *key, const tobj *value, void *ctx);

/**
 * @brief 创建空哈希表。
 *
 * @return 新哈希表，由调用者使用 thashtbl_free() 释放。
 */
thashtbl *thashtbl_new(void);

/**
 * @brief 释放哈希表及其持有的键和值引用。
 *
 * @param tbl 可为空的哈希表指针。
 */
void thashtbl_free(thashtbl *tbl);

/**
 * @brief 返回表中键值对数量。
 *
 * @param tbl 哈希表。
 * @return 键值对数量。
 */
uint_count thashtbl_len(const thashtbl *tbl);

/**
 * @brief 写入或替换一个键值对。
 *
 * @param tbl 哈希表。
 * @param key 要保存的键；函数取得独立引用。
 * @param value 要保存的值；函数取得独立引用。
 */
uint_count thashtbl_set(thashtbl *tbl, const tobj *key, const tobj *value);

/**
 * @brief 在缓存槽位写入或替换一个键值对。
 *
 * @details 供内联缓存使用：槽位失效（重哈希、删除或键不匹配）时
 * 不做任何修改并返回零，调用方回退到 `thashtbl_set`。
 *
 * @param tbl 哈希表。
 * @param slot 缓存的候选槽位。
 * @param key 要保存的键。
 * @param value 要保存的值；写入成功时取得独立引用。
 * @return 成功返回非零，槽位无效返回零。
 */
int thashtbl_set_at(thashtbl *tbl, uint_count slot, const tobj *key,
		    const tobj *value);

/**
 * @brief 查询键对应的值。
 *
 * @param tbl 哈希表。
 * @param key 要查询的键。
 * @return 表内只读值指针；键不存在时返回 `nullptr`。调用者不得释放该指针。
 */
const tobj *thashtbl_get(const thashtbl *tbl, const tobj *key);

/**
 * @brief 查询键对应的值并报告其槽位。
 *
 * @param tbl 哈希表。
 * @param key 要查询的键。
 * @param slot 输出命中槽位；键不存在时输出未定义值。
 * @return 表内只读值指针；键不存在时返回 `nullptr`。
 */
const tobj *thashtbl_find(const thashtbl *tbl, const tobj *key,
			  uint_count *slot);

/**
 * @brief 读取指定槽位的值并校验其键。
 *
 * @details 供内联缓存使用：槽位可能因重哈希或删除而失效，
 * 返回前会复核该槽位仍存有指定键。
 *
 * @param tbl 哈希表。
 * @param slot 候选槽位。
 * @param key 期望的键。
 * @return 槽位复核通过时返回表内只读值指针，否则返回 `nullptr`。
 */
const tobj *thashtbl_get_entry_at(const thashtbl *tbl, uint_count slot,
				  const tobj *key);


/**
 * @brief 返回哈希表容量。
 *
 * @param tbl 哈希表。
 * @return 槽位容量（2 的幂）。
 */
uint_count thashtbl_capacity(const thashtbl *tbl);

/**
 * @brief 判断键是否存在。
 *
 * @param tbl 哈希表。
 * @param key 要判断的键。
 * @return 存在返回非零，否则返回零。
 */
int thashtbl_contains(const thashtbl *tbl, const tobj *key);

/**
 * @brief 删除键值对。
 *
 * @param tbl 哈希表。
 * @param key 要删除的键。
 * @return 删除成功返回非零，键不存在返回零。
 */
int thashtbl_delete(thashtbl *tbl, const tobj *key);

/**
 * @brief 深复制哈希表容器并保留其中的值。
 *
 * @param tbl 源表。
 * @return 新表，由调用者释放。
 */
thashtbl *thashtbl_copy(const thashtbl *tbl);

/**
 * @brief 按表的内部遍历顺序访问全部键值对。
 *
 * @param tbl 哈希表。
 * @param fn 每个键值对调用一次的回调。
 * @param ctx 原样传给回调的用户上下文。
 */
void thashtbl_each(const thashtbl *tbl, thashtbl_each_fn fn, void *ctx);

#ifdef __cplusplus
}
#endif

#endif /* TAPAS_DSA_THASHTBL_H */
