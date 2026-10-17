/**
 * @file thashtbl_internal.h
 * @brief 哈希表的桶布局与整数键快路径，供库实现与运行时热路径内联使用。
 *
 * @details 公共头文件保持不透明并禁止依赖核心值 ABI；虚拟机的索引读写
 * 热路径需要在派发点内联完成整数键的哈希与探测，因此布局移入本私有头，
 * 由 thashtbl.c 与运行时共同包含。其余代码仍应通过 thashtbl.h 访问。
 */
#ifndef TAPAS_DSA_THASHTBL_INTERNAL_H
#define TAPAS_DSA_THASHTBL_INTERNAL_H

#include "tapas/basic_defs/tbasis.h"
#include "tapas/dsa/thashtbl.h"
#include "tapas/tval.h"

/* 装载上限：len/used 达到容量的 7/10 时扩容或同容量重排。 */
#define THASH_LOAD_NUM 7
#define THASH_LOAD_DEN 10

/* 恒等哈希下随机键的最长探测链约为 log n 量级；超过此阈值几乎必然是
 * 跨步整数键的低位聚集，插入侧据此把表升级为混合模式。 */
#define THASH_PROBE_CHAIN_LIMIT 32

/** @brief 单个桶：键值加规范化哈希；零表示空桶，一表示墓碑。 */
typedef struct {
	tobj key;
	tobj value;
	uint64_t hash;
} thash_entry;

/** @brief 开放寻址哈希表的内联布局。 */
struct thashtbl {
	thash_entry *entries;  /**< 桶数组。 */
	uint_count len;        /**< 存活键值对数量。 */
	uint_count used;       /**< 占用桶数（含墓碑）。 */
	uint_count capacity;   /**< 桶数量，恒为 2 的幂。 */
	uint8_t mixed;         /**< 非零时整数键改用混合哈希（跨步键去聚集）。 */
};

/**
 * @brief 计算整数键的规范化哈希。
 *
 * @details 与通用键哈希的 tint 分支完全一致。低位保持原样（等价于
 * CPython 的 int 恒等哈希），顺序整数键落在相邻桶中，探测对缓存友好；
 * 第 63 位的标记把结果抬到 2 以上，使其可以直接作为桶的占用标记，且
 * 不会像加偏移那样引入额外碰撞。跨步键（如 2 的幂间隔）在低位上聚集，
 * 由插入侧的探测链长度检测升级为混合模式（见 thashtbl_int_key_hash）。
 *
 * @param key 整数键的位表示。
 * @return 规范化哈希。
 */
static inline uint64_t thashtbl_hash_int(uint64_t key)
{
	return (key ^ 0x30ULL) | (1ULL << 63);
}

/**
 * @brief 整数键的混合哈希（splitmix64 终置换）。
 *
 * @details 仅在表切换到混合模式后使用：恒等哈希下跨步整数键（如 2 的幂
 * 间隔的扁平化矩阵下标）会在低位聚集成超长探测链，表在插入侧检测到后
 * 升级为混合哈希并重排。异或折叠破坏 2-adic 结构，跨步键随之散开。
 */
static inline uint64_t thash_mix(uint64_t x)
{
	x ^= x >> 30;
	x *= 0xbf58476d1ce4e5b9ULL;
	x ^= x >> 27;
	x *= 0x94d049bb133111ebULL;
	x ^= x >> 31;
	return x;
}

/** @brief 混合模式下整数键的规范化哈希。 */
static inline uint64_t thashtbl_hash_int_mixed(uint64_t key)
{
	return thash_mix(key) | (1ULL << 63);
}

/**
 * @brief 按表的当前模式计算整数键的规范化哈希。
 *
 * @details VM 的索引读写热路径直接内联整数键的探测，必须与表内存储
 * 使用的哈希一致；非混合表保持恒等哈希的顺序键缓存局部性。
 */
static inline uint64_t thashtbl_int_key_hash(const thashtbl *tbl, uint64_t key)
{
	return tbl->mixed ? thashtbl_hash_int_mixed(key) : thashtbl_hash_int(key);
}

/**
 * @brief 指针命中快速读：仅处理内弦键指针直中的热路径。
 *
 * @details 返回空表示需要走完整的 thashtbl_get_entry_at()（键不同、
 * 槽位无效或桶为空）。
 *
 * @param tbl 哈希表。
 * @param slot 候选桶位。
 * @param key_ptr 内弦键对象指针。
 * @return 命中时的值指针，否则为 NULL。
 */
static inline const tobj *thashtbl_entry_value_at(const thashtbl *tbl,
						  uint_count slot,
						  const tcompo_v *key_ptr)
{
	if (slot >= tbl->capacity)
		return nullptr;
	const thash_entry *entry = &tbl->entries[slot];
	if (entry->hash < 2 || entry->key.val.v_tcompo != key_ptr)
		return nullptr;
	return &entry->value;
}

/**
 * @brief 判断再写入一个元素是否需要扩容或同容量重排。
 *
 * @param tbl 哈希表。
 * @return 需要时返回非零；调用方应回退到 thashtbl_set()。
 */
static inline int thashtbl_needs_rehash(const thashtbl *tbl)
{
	return (tbl->len + 1) * THASH_LOAD_DEN >= tbl->capacity * THASH_LOAD_NUM ||
	       (tbl->used + 1) * THASH_LOAD_DEN >= tbl->capacity * THASH_LOAD_NUM;
}

#endif /* TAPAS_DSA_THASHTBL_INTERNAL_H */
