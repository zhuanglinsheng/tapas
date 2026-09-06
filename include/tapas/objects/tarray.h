/**
 * @file tarray.h
 * @brief 声明核心稠密数值数组对象。
 *
 * @details 定义实数数组与布尔数组的运行时布局，以及构造、索引、修改、形状查询
 * 和实数矩阵乘法接口。
 *
 * @note 本文件只描述核心对象表示；dense 包的策略与注册逻辑不属于这里。
 */
#ifndef TAPAS_OBJECTS_TARRAY_H
#define TAPAS_OBJECTS_TARRAY_H

#include "tapas/tval.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 对象布局 */

/** 稠密实数数组的运行时布局。 */
struct tdarr {
	tcompo_v base; /**< 引用对象共有的对象头。 */
	size_t rows;   /**< 行数。 */
	size_t cols;   /**< 列数。 */
	double *data;  /**< 按行连续存储的元素，由对象持有。 */
};

/** 稠密布尔数组的运行时布局。 */
struct tbarr {
	tcompo_v base;      /**< 引用对象共有的对象头。 */
	size_t rows;        /**< 行数。 */
	size_t cols;        /**< 列数。 */
	unsigned char *data; /**< 按行连续存储的布尔值，由对象持有。 */
};

/* 共享 vtable */

/** 实数数组对象使用的共享 vtable。 */
extern tcompo_vtable tdarr_vtable;

/** 布尔数组对象使用的共享 vtable。 */
extern tcompo_vtable tbarr_vtable;

/* 构造 */

/**
 * @brief 创建并用同一个值填充实数数组。
 *
 * @param rows 行数。
 * @param cols 列数。
 * @param value 初始元素值。
 * @return 新对象，由 Tapas 引用计数管理。
 */
tdarr *tdarr_new(size_t rows, size_t cols, double value);

/**
 * @brief 创建并用同一个值填充布尔数组。
 *
 * @param rows 行数。
 * @param cols 列数。
 * @param value 零表示假，非零表示真。
 * @return 新对象，由 Tapas 引用计数管理。
 */
tbarr *tbarr_new(size_t rows, size_t cols, int value);

/**
 * @brief 创建不初始化元素的实数数组。
 *
 * @param rows 行数。
 * @param cols 列数。
 * @return 新对象；调用者必须在读取前写入元素。
 */
tdarr *tdarr_new_uninitialized(size_t rows, size_t cols);

/**
 * @brief 创建不初始化元素的布尔数组。
 *
 * @param rows 行数。
 * @param cols 列数。
 * @return 新对象；调用者必须在读取前写入元素。
 */
tbarr *tbarr_new_uninitialized(size_t rows, size_t cols);

/* 能力 */

/**
 * @brief 取得数组的行数。
 *
 * @param arr 实数数组或布尔数组对象。
 * @return 行数。
 */
size_t tarr_rows(const tcompo_v *arr);

/**
 * @brief 取得数组的列数。
 *
 * @param arr 实数数组或布尔数组对象。
 * @return 列数。
 */
size_t tarr_cols(const tcompo_v *arr);

/**
 * @brief 读取实数数组元素。
 *
 * @param arr 数组。
 * @param row 行索引。
 * @param col 列索引。
 * @return 指定位置的元素值。
 */
double tdarr_at(const tdarr *arr, size_t row, size_t col);

/**
 * @brief 读取布尔数组元素。
 *
 * @param arr 数组。
 * @param row 行索引。
 * @param col 列索引。
 * @return 真返回非零，假返回零。
 */
int tbarr_at(const tbarr *arr, size_t row, size_t col);

/**
 * @brief 写入实数数组元素。
 *
 * @param arr 数组。
 * @param row 行索引。
 * @param col 列索引。
 * @param value 新值。
 */
void tdarr_set(tdarr *arr, size_t row, size_t col, double value);

/**
 * @brief 写入布尔数组元素。
 *
 * @param arr 数组。
 * @param row 行索引。
 * @param col 列索引。
 * @param value 零表示假，非零表示真。
 */
void tbarr_set(tbarr *arr, size_t row, size_t col, int value);

/* 操作符 */

/**
 * @brief 计算两个实数矩阵的乘积。
 *
 * @param left 左矩阵。
 * @param right 右矩阵，其行数必须等于左矩阵的列数。
 * @return 新的乘积矩阵，由 Tapas 引用计数管理。
 */
tdarr *tdarr_matmul(const tdarr *left, const tdarr *right);

#ifdef __cplusplus
}
#endif

#endif /* TAPAS_OBJECTS_TARRAY_H */
