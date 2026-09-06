/**
 * @file tformat.h
 * @brief 声明 Tapas 值统一使用的对象文本渲染接口。
 *
 * @details 核心对象和包对象可用本接口生成风格一致的文本表示。接口统一处理字符串
 * 转义、嵌套值、输出长度限制和循环引用；对象渲染器只需描述自身内容，遍历状态和
 * 有界文本构造由格式化上下文负责。
 *
 * @note 本接口供 C 扩展实现对象 vtable 的 `tostring` 操作，与 Tapas 源码层的
 * `format` 包无关。渲染上下文只能在一次回调期间使用，不得由对象长期保存。
 */
#ifndef TAPAS_TFORMAT_H
#define TAPAS_TFORMAT_H

#include "tapas/dsa/tobj_vec.h"

#include <stddef.h>

/**
 * @brief 一次对象渲染操作使用的不透明上下文。
 *
 * @details 渲染器只能把该指针传给本文件声明的函数。上下文的存储、输出缓冲区和
 * 遍历状态均由 tformat_object() 管理。
 */
typedef struct tformat_context tformat_context;

/**
 * @brief 将对象内容写入格式化上下文的回调类型。
 *
 * @details 回调不得释放任何参数，也不得在返回后保存 `context`。嵌套的 Tapas 值
 * 应通过 tformat_value() 或 tformat_sequence() 输出，以继续使用统一的递归与
 * 循环引用保护。
 *
 * @param context 由 tformat_object() 创建的活动输出上下文。
 * @param self 传给 tformat_object() 的对象指针。
 */
typedef void (*tformat_renderer)(tformat_context *context, const void *self);

/**
 * @brief 在带循环检测和长度限制的上下文中渲染对象。
 *
 * @param self 用于循环检测并原样传给渲染回调的对象标识。
 * @param limit 添加截断标记前允许写入的普通输出字节上限。
 * @param renderer 负责写出对象内容的回调。
 * @return 新创建的字符串，由调用者使用 tstring_free() 释放。当前遍历已经包含
 * `self` 时返回 `<cycle>`；遍历预算耗尽时返回 `...<limit>`。
 */
tstring *tformat_object(const void *self, size_t limit,
			tformat_renderer renderer);

/**
 * @brief 创建对象地址的简写文本表示。
 *
 * @param self 要显示的地址。
 * @return 形如 `<address>` 的新字符串，由调用者使用 tstring_free() 释放。
 */
tstring *tformat_pointer(const void *self);

/**
 * @brief 向当前文本表示追加普通文本。
 *
 * @details 若追加后超过上下文限制，函数会避免从 UTF-8 延续字节中间截断，追加
 * `...<truncated>`，并停止接受后续输出。`text` 为空指针时不写入任何内容。
 *
 * @param context 活动格式化上下文。
 * @param text 要追加的、以空字符结尾的文本。
 */
void tformat_text(tformat_context *context, const char *text);

/**
 * @brief 以十进制形式追加有符号整数。
 *
 * @param context 活动格式化上下文。
 * @param number 要追加的整数。
 */
void tformat_number(tformat_context *context, long number);

/**
 * @brief 追加带双引号和转义的字符串。
 *
 * @details 反斜杠、双引号、换行、回车和水平制表符使用常见转义形式，其他控制
 * 字节写成 `\\xNN`。`text` 为空指针时输出空的带引号字符串。
 *
 * @param context 活动格式化上下文。
 * @param text 要转义并追加的、以空字符结尾的字符串。
 */
void tformat_quoted(tformat_context *context, const char *text);

/**
 * @brief 追加 Tapas 值的完整文本表示。
 *
 * @details String 对象会加引号，其他值交给 tobj_tostring_full()。嵌套对象继续
 * 参与当前遍历，因此仍受循环引用保护。
 *
 * @param context 活动格式化上下文。
 * @param value 要渲染的 Tapas 值。
 */
void tformat_value(tformat_context *context, const tobj *value);

/**
 * @brief 追加以逗号和空格分隔的 Tapas 值序列。
 *
 * @details 本函数只输出序列元素，外层括号或其他定界符由调用者输出。
 *
 * @param context 活动格式化上下文。
 * @param values 按存储顺序渲染的值序列。
 */
void tformat_sequence(tformat_context *context, const tobj_vec *values);

/**
 * @brief 追加对象类别和可选显示名称。
 *
 * @details 非空名称与类别之间插入一个空格；名称为空指针或空字符串时只输出类别。
 *
 * @param context 活动格式化上下文。
 * @param kind 要追加的对象类别文本。
 * @param name 可为空的显示名称。
 */
void tformat_named(tformat_context *context, const char *kind,
		   const char *name);

/**
 * @brief 进入由渲染器手动管理的递归区段。
 *
 * @details 当渲染器不经过 tformat_object() 而直接递归处理结构时使用。成功调用
 * 必须与一次 tformat_end_nested() 配对；失败时函数会追加 `...<limit>`，调用者
 * 不得进入递归区段，也不得调用 tformat_end_nested()。
 *
 * @param context 活动格式化上下文。
 * @return 可以继续渲染时返回非零；递归深度或遍历节点预算耗尽时返回零。
 */
int tformat_begin_nested(tformat_context *context);

/**
 * @brief 离开由 tformat_begin_nested() 成功进入的递归区段。
 *
 * @param context 活动格式化上下文。
 */
void tformat_end_nested(tformat_context *context);

/**
 * @brief 判断上下文是否已因截断而停止接受输出。
 *
 * @param context 活动格式化上下文。
 * @return 已停止时返回非零，仍可输出时返回零。
 */
int tformat_stopped(const tformat_context *context);

/**
 * @brief 判断当前渲染器是否正在处理本次遍历的根对象。
 *
 * @details 渲染器可据此为最外层对象添加完整标签，同时保持嵌套表示简洁。
 *
 * @param context 活动格式化上下文。
 * @return 当前是根对象时返回非零，处理嵌套对象时返回零。
 */
int tformat_is_root(const tformat_context *context);

#endif
