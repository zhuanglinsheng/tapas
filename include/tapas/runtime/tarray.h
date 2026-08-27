#ifndef T_RUNTIME_ARRAY_H
#define T_RUNTIME_ARRAY_H

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
tdarr *tdarr_from_list(size_t rows, size_t cols, const tlist *values);
tbarr *tbarr_new(size_t rows, size_t cols, int value);
tbarr *tbarr_from_list(size_t rows, size_t cols, const tlist *values);
size_t tarr_rows(const tcompo_v *arr);
size_t tarr_cols(const tcompo_v *arr);
double tdarr_at(const tdarr *arr, size_t row, size_t col);
int tbarr_at(const tbarr *arr, size_t row, size_t col);
void tdarr_set(tdarr *arr, size_t row, size_t col, double value);
void tbarr_set(tbarr *arr, size_t row, size_t col, int value);
void tarr_idx(tcompo_v *arr, const tobj *params, uint_regs np, tobj *vre);
void tarr_iset(tcompo_v *arr, const tobj *params, uint_regs np,
	       const tobj *value);
tdarr *tdarr_slice(const tdarr *arr, size_t row, size_t col,
		   size_t rows, size_t cols);
tbarr *tbarr_slice(const tbarr *arr, size_t row, size_t col,
		   size_t rows, size_t cols);
tdarr *tdarr_transpose(const tdarr *arr);
tbarr *tbarr_transpose(const tbarr *arr);
tdarr *tdarr_matmul(const tdarr *left, const tdarr *right);
tdarr *tdarr_neg(const tdarr *arr);

#ifdef __cplusplus
}
#endif

#endif /* T_RUNTIME_ARRAY_H */
