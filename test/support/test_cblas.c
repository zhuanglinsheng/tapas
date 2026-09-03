#include <math.h>

/* Minimal CBLAS-compatible test library. Production Tapas never links this
 * target; it only verifies the runtime loader without requiring system BLAS. */
void cblas_dcopy(const int n, const double *x, const int incx,
		 double *y, const int incy)
{
	for (int i = 0; i < n; i++)
		y[i * incy] = x[i * incx];
}

void cblas_daxpy(const int n, const double alpha, const double *x,
		 const int incx, double *y, const int incy)
{
	for (int i = 0; i < n; i++)
		y[i * incy] += alpha * x[i * incx];
}

void cblas_dscal(const int n, const double alpha, double *x, const int incx)
{
	for (int i = 0; i < n; i++)
		x[i * incx] *= alpha;
}

double cblas_ddot(const int n, const double *x, const int incx,
		  const double *y, const int incy)
{
	double result = 0.0;
	for (int i = 0; i < n; i++)
		result += x[i * incx] * y[i * incy];
	return result;
}

double cblas_dnrm2(const int n, const double *x, const int incx)
{
	double squared = 0.0;
	for (int i = 0; i < n; i++)
		squared += x[i * incx] * x[i * incx];
	return sqrt(squared);
}

void cblas_dgemm(const int layout, const int transa, const int transb,
		 const int m, const int n, const int k, const double alpha,
		 const double *a, const int lda, const double *b,
		 const int ldb, const double beta, double *c, const int ldc)
{
	(void)layout;
	(void)transa;
	(void)transb;
	for (int i = 0; i < m; i++)
		for (int j = 0; j < n; j++) {
			double sum = 0.0;
			for (int p = 0; p < k; p++)
				sum += a[i * lda + p] * b[p * ldb + j];
			c[i * ldc + j] = alpha * sum + beta * c[i * ldc + j];
		}
}
