/**
 * @file tblockpool.c
 * @brief 热路径小块内存的线程本地回收池。
 *
 * @details 解释器的高频工作负载每轮迭代都会创建并销毁同尺寸的短期对象
 * （列表元素缓冲、哈希桶数组、Pair 节点等），直接往返 malloc/free 会
 * 成为主要开销。本模块按 2 的幂尺寸类别缓存少量刚释放的块，供同类别
 * 的后续分配直接复用。池中的块始终是真实的 malloc 块，只是暂缓归还；
 * 静态线程本地数组保持对它们的引用，因此不会被泄漏检查误报，也不会
 * 在多会话间共享所有权语义。
 *
 * @note 归还时记录的尺寸以分配方所知的请求尺寸为准（真实块可能更大，
 * 下界记录始终安全）；超出池范围的块直接走 malloc/free。
 */
#include "tblockpool.h"

#define TBPOOL_DISABLE 0

#include <stdlib.h>
#include <string.h>

/* 类别覆盖 32B 到 128KiB；更大的块分配频率低，不值得缓存。 */
#define TBPOOL_MIN_SHIFT 5
#define TBPOOL_MAX_SHIFT 17
#define TBPOOL_CLASSES (TBPOOL_MAX_SHIFT - TBPOOL_MIN_SHIFT + 1)
#define TBPOOL_PER_CLASS 4

typedef struct {
	void *ptr;   /**< 缓存的块；空槽为 NULL。 */
	size_t size; /**< 已知的可用尺寸下界（字节）。 */
} tbpool_slot;

static _Thread_local tbpool_slot tbpool[TBPOOL_CLASSES][TBPOOL_PER_CLASS];

static int tbpool_class(size_t bytes)
{
	int cls = 0;
	size_t span = (size_t)1 << TBPOOL_MIN_SHIFT;
	while (span < bytes && cls < TBPOOL_CLASSES - 1) {
		span <<= 1;
		cls++;
	}
	return cls;
}

void *tblockpool_alloc(size_t bytes)
{
#if TBPOOL_DISABLE
	return malloc(bytes);
#endif
	if (bytes == 0 || bytes > ((size_t)1 << TBPOOL_MAX_SHIFT))
		return malloc(bytes);
	tbpool_slot *slots = tbpool[tbpool_class(bytes)];
	for (int i = 0; i < TBPOOL_PER_CLASS; i++) {
		if (slots[i].ptr && slots[i].size >= bytes) {
			void *ptr = slots[i].ptr;
			slots[i].ptr = nullptr;
			slots[i].size = 0;
			return ptr;
		}
	}
	return malloc(bytes);
}

void *tblockpool_calloc(size_t bytes)
{
	void *ptr = tblockpool_alloc(bytes);
	if (ptr)
		memset(ptr, 0, bytes);
	return ptr;
}

void tblockpool_free(void *ptr, size_t bytes)
{
#if TBPOOL_DISABLE
	free(ptr);
	return;
#endif
	if (!ptr)
		return;
	if (bytes < ((size_t)1 << TBPOOL_MIN_SHIFT))
		bytes = (size_t)1 << TBPOOL_MIN_SHIFT;
	if (bytes > ((size_t)1 << TBPOOL_MAX_SHIFT)) {
		free(ptr);
		return;
	}
	tbpool_slot *slots = tbpool[tbpool_class(bytes)];
	int smallest = 0;
	for (int i = 0; i < TBPOOL_PER_CLASS; i++) {
		if (!slots[i].ptr) {
			slots[i].ptr = ptr;
			slots[i].size = bytes;
			return;
		}
		if (slots[i].size < slots[smallest].size)
			smallest = i;
	}
	/* 类别已满：保留较大的块，归还较小的一个。 */
	if (bytes > slots[smallest].size) {
		free(slots[smallest].ptr);
		slots[smallest].ptr = ptr;
		slots[smallest].size = bytes;
	} else
		free(ptr);
}
