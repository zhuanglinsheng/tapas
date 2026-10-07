/**
 * @file tblockpool.c
 * @brief 热路径小块内存的线程本地回收池。
 *
 * @details 解释器的高频工作负载每轮迭代都会创建并销毁同尺寸的短期对象
 * （列表元素缓冲、哈希桶数组、Pair 节点、树节点等），直接往返
 * malloc/free 会成为主要开销。本模块按 2 的幂尺寸类别组织空闲块：分配
 * 向上取整到类别尺寸，释放把块推入该类别的单链空闲栈（链接指针写在
 * 块内，不再额外分配）。类别设每类驻留上限，超出部分归还系统，避免
 * 偶发的内存尖峰被无限期保留。
 *
 * @note 池中的块始终是真实的 malloc 块，只是暂缓归还；线程本地静态
 * 数组保持对它们的引用，不会被泄漏检查误报，也不跨会话共享所有权。
 */
#include "tblockpool.h"

#include <stdlib.h>
#include <string.h>

/* 类别覆盖 32B 到 128KiB；更大的块分配频率低，不值得缓存。 */
#define TBPOOL_MIN_SHIFT 5
#define TBPOOL_MAX_SHIFT 17
#define TBPOOL_CLASSES (TBPOOL_MAX_SHIFT - TBPOOL_MIN_SHIFT + 1)

/* 每个类别最多驻留 32MiB，防止尖峰后的长期保留。 */
#define TBPOOL_MAX_CLASS_BYTES ((size_t)32 << 20)

static _Thread_local void *tbpool_head[TBPOOL_CLASSES];
static _Thread_local size_t tbpool_count[TBPOOL_CLASSES];

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
	if (bytes == 0 || bytes > ((size_t)1 << TBPOOL_MAX_SHIFT))
		return malloc(bytes);
	int cls = tbpool_class(bytes);
	void *block = tbpool_head[cls];
	if (block) {
		void *next;
		memcpy(&next, block, sizeof(next));
		tbpool_head[cls] = next;
		tbpool_count[cls]--;
		return block;
	}
	return malloc((size_t)1 << (TBPOOL_MIN_SHIFT + cls));
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
	if (!ptr)
		return;
	if (bytes > ((size_t)1 << TBPOOL_MAX_SHIFT)) {
		free(ptr);
		return;
	}
	if (bytes < ((size_t)1 << TBPOOL_MIN_SHIFT))
		bytes = (size_t)1 << TBPOOL_MIN_SHIFT;
	int cls = tbpool_class(bytes);
	size_t cap = TBPOOL_MAX_CLASS_BYTES >> (TBPOOL_MIN_SHIFT + cls);
	if (tbpool_count[cls] >= cap) {
		free(ptr);
		return;
	}
	memcpy(ptr, &tbpool_head[cls], sizeof(void *));
	tbpool_head[cls] = ptr;
	tbpool_count[cls]++;
}
