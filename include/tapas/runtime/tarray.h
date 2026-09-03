#ifndef TAPAS_RUNTIME_TARRAY_H
#define TAPAS_RUNTIME_TARRAY_H

#include "tapas/tval.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct tdarr {
	tcompo_v base;
	size_t rows;
	size_t cols;
	double *data;
};

struct tbarr {
	tcompo_v base;
	size_t rows;
	size_t cols;
	unsigned char *data;
};

extern tcompo_vtable tdarr_vtable;
extern tcompo_vtable tbarr_vtable;

tdarr *tdarr_new(size_t rows, size_t cols, double value);
tbarr *tbarr_new(size_t rows, size_t cols, int value);
tdarr *tdarr_new_uninitialized(size_t rows, size_t cols);
tbarr *tbarr_new_uninitialized(size_t rows, size_t cols);
size_t tarr_rows(const tcompo_v *arr);
size_t tarr_cols(const tcompo_v *arr);
double tdarr_at(const tdarr *arr, size_t row, size_t col);
int tbarr_at(const tbarr *arr, size_t row, size_t col);
void tdarr_set(tdarr *arr, size_t row, size_t col, double value);
void tbarr_set(tbarr *arr, size_t row, size_t col, int value);
tdarr *tdarr_matmul(const tdarr *left, const tdarr *right);

#ifdef __cplusplus
}
#endif

#endif /* TAPAS_RUNTIME_TARRAY_H */
