/**
 * @file tstdlib.h
 * @brief 声明 Tapas 标准库的扩展描述符入口。
 *
 * @details 嵌入方通过该入口取得标准库各模块、符号及类型模式的统一注册描述。
 *
 * @note 返回的描述符由标准库静态持有，调用者不得修改或释放。
 */
#ifndef TAPAS_TSTDLIB_H
#define TAPAS_TSTDLIB_H

#include "tapas/textension.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 取得 Tapas 标准库的扩展描述符。
 *
 * @return 由标准库静态持有的只读描述符。
 */
const textension_descriptor *tstdlib_descriptor(void);

#ifdef __cplusplus
}
#endif

#endif
