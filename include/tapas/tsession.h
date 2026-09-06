/**
 * @file tsession.h
 * @brief 声明稳定的 Tapas 高层会话接口。
 *
 * @details 提供源码编译与执行、模块调用、搜索路径管理和会话根库访问，同时隐藏
 * VM 的具体实现状态。
 *
 * @note `tsession` 是不透明类型；调用者只能使用本文件声明的接口，不得依赖其
 * 存储布局。
 */
#ifndef TAPAS_TSESSION_H
#define TAPAS_TSESSION_H

#include "tapas/basic_defs/tbasis.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tsession tsession; /**< 不透明的 Tapas 会话。 */
typedef struct tdict tdict;       /**< Dictionary 对象。 */
typedef struct tlib tlib;         /**< 会话使用的符号库。 */
typedef struct tobj tobj;         /**< Tapas 值。 */

/**
 * @brief 创建新的独立会话。
 *
 * @return 新会话，由调用者使用 tsession_free() 释放。
 */
tsession *tsession_new(void);

/**
 * @brief 释放会话及其持有的运行时资源。
 *
 * @param sess 可为空的会话指针。
 */
void tsession_free(tsession *sess);

/**
 * @brief 取得会话的根符号库。
 *
 * @param sess 会话。
 * @return 由会话持有的借用指针，在会话释放后失效。
 */
tlib *tsession_get_lib(tsession *sess);

/**
 * @brief 将 `.tap` 源文件编译为 `.tapc` 字节码文件。
 *
 * @param sess 会话。
 * @param file 源文件路径。
 * @param interactive 非零时使用交互式诊断方式。
 */
void tsession_compile_file(tsession *sess, const char *file, int interactive);

/**
 * @brief 执行已经编译的 `.tapc` 字节码文件。
 *
 * @param sess 会话。
 * @param file 字节码文件路径。
 */
void tsession_eval_bycodes(tsession *sess, const char *file);

/**
 * @brief 编译并执行 `.tap` 文件，但不保存中间 `.tapc` 文件。
 *
 * @param sess 会话。
 * @param file 源文件路径。
 * @param interactive 非零时使用交互式诊断方式。
 */
void tsession_execute_file(tsession *sess, const char *file, int interactive);

/**
 * @brief 在会话中执行模块入口。
 *
 * @param sess 会话。
 * @param module 模块名称或模块路径。
 * @param argument_count 命令行参数数量。
 * @param arguments 命令行参数数组，只在调用期间读取。
 * @return 执行成功返回零，失败返回非零。
 */
int tsession_execute_module(tsession *sess, const char *module,
			    int argument_count, const char *const *arguments);

/**
 * @brief 执行 Markdown 中的 Tapas 代码，并原地更新结果区块。
 *
 * @param sess 会话。
 * @param file Markdown 文件路径。
 * @param interactive 非零时使用交互式诊断方式。
 */
void tsession_execute_markdown_update(tsession *sess, const char *file, int interactive);

/**
 * @brief 编译并执行一段 Tapas 源码字符串。
 *
 * @param sess 会话。
 * @param str 以空字符结尾的源码。
 * @param interactive 非零时使用交互式诊断方式。
 */
void tsession_execute_str(tsession *sess, const char *str, int interactive);

/**
 * @brief 显示 `.tapc` 文件中的字节码。
 *
 * @param sess 会话。
 * @param file 字节码文件路径。
 */
void tsession_show_bycodes(tsession *sess, const char *file);

/**
 * @brief 向会话追加模块和文件搜索路径。
 *
 * @param sess 会话。
 * @param path 要追加的目录路径。
 */
void tsession_add_path(tsession *sess, const char *path);

/**
 * @brief 在会话根库中创建并注册包字典。
 *
 * @param sess 会话。
 * @param pkgname 包名称。
 * @return 由会话持有的包字典借用指针。
 */
tdict *tsession_add_pkg(tsession *sess, const char *pkgname);

/**
 * @brief 按槽位取得会话根库中的 Tapas 值。
 *
 * @param sess 会话。
 * @param loc 根库槽位索引。
 * @return 由会话持有的值指针；调用者不得释放。
 */
tobj *tsession_get_obj(tsession *sess, uint_objs loc);

#ifdef __cplusplus
}
#endif

#endif /* TAPAS_TSESSION_H */
