/**
 * @file object.h
 * @brief Declares the finite package's native Index and Distribution objects.
 */
#ifndef TAPAS_STDLIB_FINITE_OBJECT_H
#define TAPAS_STDLIB_FINITE_OBJECT_H

#include "tapas/tval.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tfinite_index tfinite_index;
typedef struct tfinite_distribution tfinite_distribution;
typedef struct trandom_generator trandom_generator;

ttypeval *tstdlib_finite_index_type(void);
ttypeval *tstdlib_finite_distribution_type(void);

tfinite_index *tfinite_index_new(const size_t *coordinates, size_t rank);
size_t tfinite_index_rank(const tfinite_index *index);
size_t tfinite_index_at(const tfinite_index *index, size_t dimension);

tfinite_distribution *tfinite_uniform_new(size_t size);
tfinite_distribution *tfinite_dense_new(const size_t *shape, size_t rank,
					 const double *weights, size_t count);
tfinite_distribution *tfinite_piecewise_constant_new(
				size_t size, const size_t *starts,
				const double *weights, size_t count);
tfinite_distribution *tfinite_piecewise_linear_new(
				const size_t *coordinates,
				const double *weights, size_t count);
tfinite_distribution *tfinite_product_new(
				tfinite_distribution *const *distributions,
				size_t count);
tfinite_distribution *tfinite_mixture_new(
				tfinite_distribution *const *distributions,
				const double *weights, size_t count);
tfinite_distribution *tfinite_conditional_new(
				tfinite_distribution *distribution,
				tfinite_index *const *indices, size_t count);
tfinite_distribution *tfinite_binomial_new(size_t trials, double probability);
tfinite_distribution *tfinite_multinomial_new(size_t trials,
				const double *weights, size_t count);
tfinite_distribution *tfinite_hypergeometric_new(size_t population,
				size_t successes, size_t draws);
tfinite_distribution *tfinite_zipf_new(size_t size, double exponent);

size_t tfinite_distribution_rank(const tfinite_distribution *distribution);
size_t tfinite_distribution_shape_at(
				const tfinite_distribution *distribution,
				size_t dimension);
size_t tfinite_distribution_cardinality(
				const tfinite_distribution *distribution);
int tfinite_distribution_contains(const tfinite_distribution *distribution,
				  const tfinite_index *index);
double tfinite_distribution_prob(const tfinite_distribution *distribution,
				 const tfinite_index *index);
double tfinite_distribution_prob_coordinates(
				 const tfinite_distribution *distribution,
				 const size_t *coordinates, size_t rank);
double tfinite_distribution_prob_at(
				 const tfinite_distribution *distribution,
				 size_t coordinate);
tfinite_index *tfinite_distribution_sample(
				const tfinite_distribution *distribution,
				trandom_generator *generator);

extern tcompo_vtable tfinite_index_vtable;
extern tcompo_vtable tfinite_distribution_vtable;

#ifdef __cplusplus
}
#endif

#endif /* TAPAS_STDLIB_FINITE_OBJECT_H */
