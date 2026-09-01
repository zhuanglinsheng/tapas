#include "tapas/tblas.h"

#include "tapas/tbasis.h"

#include <limits.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

enum {
	CBLAS_ROW_MAJOR = 101,
	CBLAS_NO_TRANS = 111
};

typedef void (*cblas_dcopy_fn)(const int, const double *, const int,
			       double *, const int);
typedef void (*cblas_daxpy_fn)(const int, const double, const double *,
			       const int, double *, const int);
typedef void (*cblas_dscal_fn)(const int, const double, double *, const int);
typedef void (*cblas_dgemm_fn)(const int, const int, const int, const int,
			       const int, const int, const double,
			       const double *, const int, const double *,
			       const int, const double, double *, const int);

typedef struct {
	cblas_dcopy_fn dcopy;
	cblas_daxpy_fn daxpy;
	cblas_dscal_fn dscal;
	cblas_dgemm_fn dgemm;
} tblas_api;

static tblas_api api;
static atomic_int load_state = 0; /* 0: unknown, 1: loading, 2: ready, -1: failed */

#if defined(_WIN32)
typedef HMODULE tblas_handle;

static tblas_handle open_library(const char *path)
{
	return LoadLibraryA(path);
}

static void close_library(tblas_handle handle)
{
	FreeLibrary(handle);
}

static void *find_symbol(tblas_handle handle, const char *name)
{
	FARPROC symbol = GetProcAddress(handle, name);
	void *result = NULL;
	if (sizeof(symbol) == sizeof(result))
		memcpy(&result, &symbol, sizeof(result));
	return result;
}
#else
typedef void *tblas_handle;

static tblas_handle open_library(const char *path)
{
	return dlopen(path, RTLD_NOW | RTLD_LOCAL);
}

static void close_library(tblas_handle handle)
{
	dlclose(handle);
}

static void *find_symbol(tblas_handle handle, const char *name)
{
	return dlsym(handle, name);
}
#endif

static int set_function(void *target, size_t target_size, void *symbol)
{
	if (!symbol || target_size != sizeof(symbol))
		return 0;
	memcpy(target, &symbol, target_size);
	return 1;
}

static int try_library(const char *path)
{
	tblas_handle handle = open_library(path);
	if (!handle)
		return 0;

	tblas_api candidate = {0};
	int valid =
		set_function(&candidate.dcopy, sizeof(candidate.dcopy),
			     find_symbol(handle, "cblas_dcopy")) &&
		set_function(&candidate.daxpy, sizeof(candidate.daxpy),
			     find_symbol(handle, "cblas_daxpy")) &&
		set_function(&candidate.dscal, sizeof(candidate.dscal),
			     find_symbol(handle, "cblas_dscal")) &&
		set_function(&candidate.dgemm, sizeof(candidate.dgemm),
			     find_symbol(handle, "cblas_dgemm"));
	if (!valid) {
		close_library(handle);
		return 0;
	}

	api = candidate;
	/* Keep the library loaded for the lifetime of the process: api contains
	 * pointers into it, and unloading at shutdown adds no useful guarantee. */
	return 1;
}

static int load_blas(void)
{
	const char *explicit_path = getenv("TAPAS_BLAS_LIBRARY");
	if (explicit_path)
		return try_library(explicit_path);

#if defined(_WIN32)
	static const char *const candidates[] = {
		"libopenblas.dll", "openblas.dll", "mkl_rt.dll"
	};
#elif defined(__APPLE__)
	static const char *const candidates[] = {
		"/System/Library/Frameworks/Accelerate.framework/Accelerate",
		"libopenblas.dylib", "libblas.dylib"
	};
#else
	static const char *const candidates[] = {
		"libopenblas.so.0", "libopenblas.so", "libblas.so.3",
		"libblas.so", "libblis.so"
	};
#endif
	for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++)
		if (try_library(candidates[i]))
			return 1;
	return 0;
}

static void require_blas(void)
{
	int state = atomic_load_explicit(&load_state, memory_order_acquire);
	if (state == 2)
		return;
	if (state == 0) {
		int expected = 0;
		if (atomic_compare_exchange_strong_explicit(
			    &load_state, &expected, 1,
			    memory_order_acq_rel, memory_order_acquire)) {
			int loaded = load_blas();
			atomic_store_explicit(&load_state, loaded ? 2 : -1,
					      memory_order_release);
			state = loaded ? 2 : -1;
		}
	}
	while (state == 1)
		state = atomic_load_explicit(&load_state, memory_order_acquire);
	if (state != 2)
		twarn(ErrRuntime_Other, "BLAS",
		      "a compatible LP64 CBLAS library is required; set TAPAS_BLAS_LIBRARY to its path");
}

static int blas_size(size_t value, const char *where)
{
	if (value > INT_MAX)
		twarn(ErrRuntime_Other, where, "array dimension exceeds the LP64 CBLAS limit");
	return (int)value;
}

void tblas_copy(size_t n, const double *x, double *y)
{
	require_blas();
	if (n != 0)
		api.dcopy(blas_size(n, "tblas_copy"), x, 1, y, 1);
}

void tblas_axpy(size_t n, double alpha, const double *x, double *y)
{
	require_blas();
	if (n != 0)
		api.daxpy(blas_size(n, "tblas_axpy"), alpha, x, 1, y, 1);
}

void tblas_scal(size_t n, double alpha, double *x)
{
	require_blas();
	if (n != 0)
		api.dscal(blas_size(n, "tblas_scal"), alpha, x, 1);
}

void tblas_gemm(size_t rows, size_t shared, size_t cols,
		const double *left, const double *right, double *out)
{
	require_blas();
	int m = blas_size(rows, "tblas_gemm");
	int k = blas_size(shared, "tblas_gemm");
	int n = blas_size(cols, "tblas_gemm");
	if (m == 0 || n == 0 || k == 0)
		return;
	api.dgemm(CBLAS_ROW_MAJOR, CBLAS_NO_TRANS, CBLAS_NO_TRANS,
		  m, n, k, 1.0, left, k, right, n, 0.0, out, n);
}
