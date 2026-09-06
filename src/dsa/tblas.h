/**
 * @file tblas.h
 * @brief Declares the internal dynamically loaded BLAS adapter.
 * @details Provides the numeric operations used by core array and dense
 * package implementations without making BLAS part of the public Tapas ABI.
 * @note This is an implementation header under `src`; extensions must not
 * depend on the selected BLAS ABI or loader behavior.
 */
#ifndef TAPAS_DSA_TBLAS_H
#define TAPAS_DSA_TBLAS_H

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

#endif /* TAPAS_DSA_TBLAS_H */
