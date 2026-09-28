/**
 * @file tblockpool.h
 * @brief 热路径小块内存的线程本地回收池接口。
 *
 * @details 释放时必须给出分配方所知的请求尺寸（可以小于真实块大小，
 * 池按尺寸下界复用，始终安全）。池只缓存 32B 到 128KiB 的块，其余
 * 直接转发给 malloc/free。
 *
 * @note 块的内容在复用前不被清理；需要清零时使用 tblockpool_calloc()。
 */
#ifndef TAPAS_DSA_TBLOCKPOOL_H
#define TAPAS_DSA_TBLOCKPOOL_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 分配至少 `bytes` 字节的块，优先复用池中的同类别块。
 *
 * @param bytes 请求尺寸。
 * @return 块指针；内存未初始化。失败时返回 NULL（与 malloc 一致）。
 */
void *tblockpool_alloc(size_t bytes);

/**
 * @brief 分配并清零至少 `bytes` 字节的块。
 *
 * @param bytes 请求尺寸。
 * @return 已清零的块指针；失败时返回 NULL。
 */
void *tblockpool_calloc(size_t bytes);

/**
 * @brief 归还块；`bytes` 为分配时的请求尺寸。
 *
 * @param ptr 块指针；NULL 安全。
 * @param bytes 分配方所知的请求尺寸。
 */
void tblockpool_free(void *ptr, size_t bytes);

#ifdef __cplusplus
}
#endif

#endif /* TAPAS_DSA_TBLOCKPOOL_H */
