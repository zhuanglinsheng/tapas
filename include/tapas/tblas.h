#ifndef TAPAS_TBLAS_H
#define TAPAS_TBLAS_H

#include <stddef.h>

/* Tapas deliberately does not link against BLAS at build time. These
 * wrappers load an LP64 CBLAS implementation on first use and raise a Tapas
 * runtime error when no compatible implementation is available. */
void tblas_copy(size_t n, const double *x, double *y);
void tblas_axpy(size_t n, double alpha, const double *x, double *y);
void tblas_scal(size_t n, double alpha, double *x);
double tblas_dot(size_t n, const double *x, const double *y);
double tblas_norm(size_t n, const double *x);
void tblas_gemm(size_t rows, size_t shared, size_t cols,
		double alpha, const double *left, const double *right,
		double beta, double *out);

#endif /* TAPAS_TBLAS_H */
