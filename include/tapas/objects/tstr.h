/**
 * @file tstr.h
 * @brief 声明核心 String 引用对象。
 *
 * @details 定义以内嵌 `tstring` 为存储的 String 对象，以及从 C 字符串或定长
 * 字节序列创建对象的接口。
 *
 * @note 通用可变字符串工具位于 `dsa/tstring.h`，不属于 String 对象协议。
 */
#ifndef TAPAS_OBJECTS_TSTR_H
#define TAPAS_OBJECTS_TSTR_H

#include "tapas/tval.h"
#include "tapas/dsa/tstring.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 对象布局 */

/** String 对象的运行时布局。 */
struct tstr {
	tcompo_v base;   /**< 引用对象共有的对象头。 */
	tstring storage; /**< 对象内嵌的字符串存储。 */
	tstring *data;   /**< 指向 `storage` 的兼容访问指针。 */
};

/* 共享 vtable */

/** String 对象使用的共享 vtable。 */
extern tcompo_vtable tstr_vtable;

/* 构造 */

/**
 * @brief 根据 C 字符串创建 String 对象。
 *
 * @param s 以空字符结尾的源字符串。
 * @return 新对象，由 Tapas 引用计数管理。
 */
tstr *tstr_new(const char *s);

/**
 * @brief 根据指定长度的字节序列创建 String 对象。
 *
 * @param s 源字节序列。
 * @param len 要复制的字节数。
 * @return 新对象，由 Tapas 引用计数管理。
 */
tstr *tstr_new_len(const char *s, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* TAPAS_OBJECTS_TSTR_H */
