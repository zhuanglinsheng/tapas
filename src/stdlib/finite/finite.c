/**
 * @file finite.c
 * @brief Exposes finite probability distributions as a native C package.
 */
#include "tapas/textension.h"

#include "../arguments.h"
#include "../random/object.h"
#include "object.h"

#include "tapas/dsa/tobj_vec.h"
#include "tapas/objects/tlist.h"
#include "tapas/objects/tpair.h"
#include "tapas/objects/ttype.h"
#include "tapas/vm_service.h"

#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static void *checked_calloc(size_t count, size_t size, const char *where)
{
	if (size && count > SIZE_MAX / size)
		twarn(ErrRuntime_Other, where, "allocation size overflow");
	void *memory = calloc(count, size);
	if (!memory && count)
		twarn(ErrRuntime_Other, where, "out of memory");
	return memory;
}

static tlist *require_list(const tobj *value, const char *where)
{
	if (value->type != tcompo ||
	    value->val.v_tcompo->vtable != &tlist_vtable)
		twarn(ErrRuntime_ParamsType, where, "List required");
	return (tlist *)value->val.v_tcompo;
}

static tfinite_index *require_index(const tobj *value, const char *where)
{
	if (value->type != tcompo ||
	    value->val.v_tcompo->vtable != &tfinite_index_vtable)
		twarn(ErrRuntime_ParamsType, where, "finite::Index required");
	return (tfinite_index *)value->val.v_tcompo;
}

static tfinite_distribution *require_distribution(const tobj *value,
						   const char *where)
{
	if (value->type != tcompo ||
	    value->val.v_tcompo->vtable != &tfinite_distribution_vtable)
		twarn(ErrRuntime_ParamsType, where,
		      "finite::Distribution required");
	return (tfinite_distribution *)value->val.v_tcompo;
}

static trandom_generator *require_generator(const tobj *value,
					     const char *where)
{
	if (value->type != tcompo ||
	    value->val.v_tcompo->vtable != &trandom_generator_vtable)
		twarn(ErrRuntime_ParamsType, where, "random::Generator required");
	return (trandom_generator *)value->val.v_tcompo;
}

static size_t positive_size(const tobj *value, const char *where,
			    const char *message)
{
	if (value->type != tint || value->val.v_tint <= 0)
		twarn(ErrRuntime_ParamsType, where, message);
	return (size_t)value->val.v_tint;
}

static size_t nonnegative_size(const tobj *value, const char *where,
			       const char *message)
{
	if (value->type != tint || value->val.v_tint < 0)
		twarn(ErrRuntime_ParamsType, where, message);
	return (size_t)value->val.v_tint;
}

static double probability_value(const tobj *value, const char *where)
{
	if (value->type != tfloat || !isfinite(value->val.v_tfloat) ||
	    value->val.v_tfloat < 0.0 || value->val.v_tfloat > 1.0)
		twarn(ErrRuntime_ParamsType, where,
		      "probability must be a finite Float in [0, 1]");
	return value->val.v_tfloat;
}

static double weight_value(const tobj *value, const char *where)
{
	if (value->type != tfloat || !isfinite(value->val.v_tfloat) ||
	    value->val.v_tfloat < 0.0)
		twarn(ErrRuntime_ParamsType, where,
		      "weights must be finite nonnegative Floats");
	return value->val.v_tfloat;
}

static double *weights_from_list(const tobj *value, size_t *count,
				 const char *where)
{
	tlist *list = require_list(value, where);
	*count = tlist_size(list);
	if (!*count)
		twarn(ErrRuntime_ParamsType, where, "weights must be nonempty");
	double *weights = checked_calloc(*count, sizeof(double), where);
	long double total = 0.0L;
	for (size_t i = 0; i < *count; i++) {
		weights[i] = weight_value(tlist_at(list, (uint_objs)i), where);
		total += weights[i];
	}
	if (!(total > 0.0L) || !isfinite(total))
		twarn(ErrRuntime_ParamsType, where,
		      "total weight must be finite and positive");
	return weights;
}

static void create_index_type(tobj *result)
{
	tobj_set_compo(result, (tcompo_v *)tstdlib_finite_index_type());
}

static void create_distribution_type(tobj *result)
{
	tobj_set_compo(result, (tcompo_v *)tstdlib_finite_distribution_type());
}

static void finite_index(tobj *params, uint_regs len, tobj *result)
{
	tstdlib_require_arguments("finite::index", len, 1);
	tlist *coordinates = require_list(&params[0], "finite::index");
	size_t rank = tlist_size(coordinates);
	if (!rank)
		twarn(ErrRuntime_ParamsType, "finite::index",
		      "coordinates must be nonempty");
	size_t *values = checked_calloc(rank, sizeof(size_t), "finite::index");
	for (size_t i = 0; i < rank; i++)
		values[i] = nonnegative_size(tlist_at(coordinates, (uint_objs)i),
					     "finite::index",
					     "coordinates must be nonnegative Ints");
	tobj_set_compo(result, (tcompo_v *)tfinite_index_new(values, rank));
	free(values);
}

static void finite_shape(tobj *params, uint_regs len, tobj *result)
{
	tstdlib_require_arguments("finite::shape", len, 1);
	tfinite_distribution *distribution = require_distribution(
		&params[0], "finite::shape");
	size_t rank = tfinite_distribution_rank(distribution);
	size_t *shape = checked_calloc(rank, sizeof(size_t), "finite::shape");
	for (size_t i = 0; i < rank; i++)
		shape[i] = tfinite_distribution_shape_at(distribution, i);
	tobj_set_compo(result, (tcompo_v *)tfinite_index_new(shape, rank));
	free(shape);
}

static void finite_prob(tobj *params, uint_regs len, tobj *result)
{
	tstdlib_require_arguments("finite::prob", len, 2);
	tfinite_distribution *distribution = require_distribution(
		&params[0], "finite::prob");
	tfinite_index *index = require_index(&params[1], "finite::prob");
	if (!tfinite_distribution_contains(distribution, index))
		twarn(ErrRuntime_ParamsType, "finite::prob",
		      "Index does not match distribution shape");
	tobj_set_float(result, tfinite_distribution_prob(distribution, index));
}

static int same_index(const tfinite_index *left, const tfinite_index *right)
{
	if (tfinite_index_rank(left) != tfinite_index_rank(right))
		return 0;
	for (size_t i = 0; i < tfinite_index_rank(left); i++)
		if (tfinite_index_at(left, i) != tfinite_index_at(right, i))
			return 0;
	return 1;
}

static void finite_mass(tobj *params, uint_regs len, tobj *result)
{
	tstdlib_require_arguments("finite::mass", len, 2);
	tfinite_distribution *distribution = require_distribution(
		&params[0], "finite::mass");
	tlist *values = require_list(&params[1], "finite::mass");
	long double sum = 0.0L;
	long double correction = 0.0L;
	for (size_t i = 0; i < tlist_size(values); i++) {
		tfinite_index *index = require_index(
			tlist_at(values, (uint_objs)i), "finite::mass");
		if (!tfinite_distribution_contains(distribution, index))
			twarn(ErrRuntime_ParamsType, "finite::mass",
			      "Index does not match distribution shape");
		int duplicate = 0;
		for (size_t j = 0; j < i; j++) {
			tfinite_index *prior = require_index(
				tlist_at(values, (uint_objs)j), "finite::mass");
			if (same_index(index, prior)) {
				duplicate = 1;
				break;
			}
		}
		if (duplicate)
			continue;
		long double value = tfinite_distribution_prob(distribution, index);
		long double adjusted = value - correction;
		long double next = sum + adjusted;
		correction = (next - sum) - adjusted;
		sum = next;
	}
	tobj_set_float(result, (double)sum);
}

static void finite_samples(tobj *params, uint_regs len, tobj *result)
{
	tstdlib_require_arguments("finite::samples", len, 3);
	tfinite_distribution *distribution = require_distribution(
		&params[0], "finite::samples");
	size_t count = nonnegative_size(&params[1], "finite::samples",
					"count must be a nonnegative Int");
	if (count > UINT16_MAX)
		twarn(ErrRuntime_ParamsType, "finite::samples",
		      "count exceeds List capacity");
	trandom_generator *generator = require_generator(
		&params[2], "finite::samples");
	tlist *samples = tlist_new();
	for (size_t i = 0; i < count; i++) {
		tobj sample;
		tobj_set_nil(&sample);
		tobj_set_compo(&sample, (tcompo_v *)tfinite_distribution_sample(
			distribution, generator));
		tobj_vec_push(&samples->items, &sample);
		tobj_try_clear(&sample);
	}
	tobj_set_compo(result, (tcompo_v *)samples);
}

static void finite_uniform(tobj *params, uint_regs len, tobj *result)
{
	tstdlib_require_arguments("finite::uniform", len, 1);
	size_t size = positive_size(&params[0], "finite::uniform",
				    "n must be a positive Int");
	tobj_set_compo(result, (tcompo_v *)tfinite_uniform_new(size));
}

static void finite_categorical(tobj *params, uint_regs len, tobj *result)
{
	tstdlib_require_arguments("finite::categorical", len, 1);
	size_t count;
	double *weights = weights_from_list(&params[0], &count,
					   "finite::categorical");
	tobj_set_compo(result, (tcompo_v *)tfinite_dense_new(
		&count, 1, weights, count));
	free(weights);
}

static tpair *require_pair(const tobj *value, const char *where)
{
	if (value->type != tcompo ||
	    value->val.v_tcompo->vtable != &tpair_vtable)
		twarn(ErrRuntime_ParamsType, where, "Pair required");
	return (tpair *)value->val.v_tcompo;
}

static void steps_from_list(const tobj *value, size_t limit,
			    size_t **points_out, double **weights_out,
			    size_t *count_out, const char *where)
{
	tlist *list = require_list(value, where);
	size_t count = tlist_size(list);
	if (!count)
		twarn(ErrRuntime_ParamsType, where, "steps must be nonempty");
	size_t *points = checked_calloc(count, sizeof(size_t), where);
	double *weights = checked_calloc(count, sizeof(double), where);
	for (size_t i = 0; i < count; i++) {
		tpair *pair = require_pair(tlist_at(list, (uint_objs)i), where);
		points[i] = nonnegative_size(&pair->first, where,
					      "coordinates must be nonnegative Ints");
		weights[i] = weight_value(&pair->second, where);
		if ((i == 0 && points[i] != 0) ||
		    (i && points[i] <= points[i - 1]) || points[i] >= limit)
			twarn(ErrRuntime_ParamsType, where,
			      "coordinates must start at zero and increase within shape");
	}
	long double total = 0.0L;
	for (size_t i = 0; i < count; i++) {
		size_t end = i + 1 < count ? points[i + 1] : limit;
		total += (long double)weights[i] *
			(long double)(end - points[i]);
	}
	if (!(total > 0.0L) || !isfinite(total))
		twarn(ErrRuntime_ParamsType, where,
		      "total weight must be finite and positive");
	*points_out = points;
	*weights_out = weights;
	*count_out = count;
}

static void finite_piecewise_constant(tobj *params, uint_regs len,
				      tobj *result)
{
	tstdlib_require_arguments("finite::piecewise_constant", len, 2);
	size_t size = positive_size(&params[0], "finite::piecewise_constant",
				    "n must be a positive Int");
	size_t *starts;
	double *weights;
	size_t count;
	steps_from_list(&params[1], size, &starts, &weights, &count,
			"finite::piecewise_constant");
	tobj_set_compo(result, (tcompo_v *)tfinite_piecewise_constant_new(
		size, starts, weights, count));
	free(starts);
	free(weights);
}

static void finite_piecewise_linear(tobj *params, uint_regs len, tobj *result)
{
	tstdlib_require_arguments("finite::piecewise_linear", len, 1);
	tlist *list = require_list(&params[0], "finite::piecewise_linear");
	size_t count = tlist_size(list);
	if (!count)
		twarn(ErrRuntime_ParamsType, "finite::piecewise_linear",
		      "knots must be nonempty");
	size_t *coordinates = checked_calloc(count, sizeof(size_t),
					     "finite::piecewise_linear");
	double *weights = checked_calloc(count, sizeof(double),
					 "finite::piecewise_linear");
	for (size_t i = 0; i < count; i++) {
		tpair *pair = require_pair(tlist_at(list, (uint_objs)i),
					   "finite::piecewise_linear");
		coordinates[i] = nonnegative_size(
			&pair->first, "finite::piecewise_linear",
			"coordinates must be nonnegative Ints");
		weights[i] = weight_value(&pair->second,
					 "finite::piecewise_linear");
		if ((i == 0 && coordinates[i] != 0) ||
		    (i && coordinates[i] <= coordinates[i - 1]))
			twarn(ErrRuntime_ParamsType, "finite::piecewise_linear",
			      "coordinates must start at zero and strictly increase");
	}
	if (coordinates[count - 1] == SIZE_MAX)
		twarn(ErrRuntime_ParamsType, "finite::piecewise_linear",
		      "shape overflow");
	long double total = weights[count - 1];
	for (size_t i = 0; i + 1 < count; i++) {
		size_t width = coordinates[i + 1] - coordinates[i];
		long double slope = ((long double)weights[i + 1] - weights[i]) /
			(long double)width;
		total += (long double)width * weights[i] + slope *
			(long double)width * (long double)(width - 1) / 2.0L;
	}
	if (!(total > 0.0L) || !isfinite(total))
		twarn(ErrRuntime_ParamsType, "finite::piecewise_linear",
		      "total weight must be finite and positive");
	tobj_set_compo(result, (tcompo_v *)tfinite_piecewise_linear_new(
		coordinates, weights, count));
	free(coordinates);
	free(weights);
}

static tfinite_distribution **distributions_from_list(
				const tobj *value, size_t *count,
				const char *where)
{
	tlist *list = require_list(value, where);
	*count = tlist_size(list);
	if (!*count)
		twarn(ErrRuntime_ParamsType, where,
		      "distributions must be nonempty");
	tfinite_distribution **items = checked_calloc(
		*count, sizeof(*items), where);
	for (size_t i = 0; i < *count; i++)
		items[i] = require_distribution(tlist_at(list, (uint_objs)i), where);
	return items;
}

static int same_shape(const tfinite_distribution *left,
			      const tfinite_distribution *right)
{
	if (tfinite_distribution_rank(left) != tfinite_distribution_rank(right))
		return 0;
	for (size_t i = 0; i < tfinite_distribution_rank(left); i++)
		if (tfinite_distribution_shape_at(left, i) !=
		    tfinite_distribution_shape_at(right, i))
			return 0;
	return 1;
}

static void finite_product(tobj *params, uint_regs len, tobj *result)
{
	tstdlib_require_arguments("finite::product", len, 1);
	size_t count;
	tfinite_distribution **items = distributions_from_list(
		&params[0], &count, "finite::product");
	tobj_set_compo(result, (tcompo_v *)tfinite_product_new(items, count));
	free(items);
}

static void finite_mixture(tobj *params, uint_regs len, tobj *result)
{
	tstdlib_require_arguments("finite::mixture", len, 2);
	size_t count;
	tfinite_distribution **items = distributions_from_list(
		&params[0], &count, "finite::mixture");
	size_t weight_count;
	double *weights = weights_from_list(&params[1], &weight_count,
					   "finite::mixture");
	if (weight_count != count)
		twarn(ErrRuntime_ParamsType, "finite::mixture",
		      "weights and distributions must have equal lengths");
	for (size_t i = 1; i < count; i++)
		if (!same_shape(items[0], items[i]))
			twarn(ErrRuntime_ParamsType, "finite::mixture",
			      "all distributions must have the same shape");
	tobj_set_compo(result, (tcompo_v *)tfinite_mixture_new(
		items, weights, count));
	free(items);
	free(weights);
}

static void finite_conditional(tobj *params, uint_regs len, tobj *result)
{
	tstdlib_require_arguments("finite::conditional", len, 2);
	tfinite_distribution *distribution = require_distribution(
		&params[0], "finite::conditional");
	tlist *list = require_list(&params[1], "finite::conditional");
	size_t count = tlist_size(list);
	if (!count)
		twarn(ErrRuntime_ParamsType, "finite::conditional",
		      "event must be nonempty");
	tfinite_index **indices = checked_calloc(count, sizeof(*indices),
						"finite::conditional");
	for (size_t i = 0; i < count; i++) {
		indices[i] = require_index(tlist_at(list, (uint_objs)i),
					   "finite::conditional");
		if (!tfinite_distribution_contains(distribution, indices[i]))
			twarn(ErrRuntime_ParamsType, "finite::conditional",
			      "Index does not match distribution shape");
	}
	tobj_set_compo(result, (tcompo_v *)tfinite_conditional_new(
		distribution, indices, count));
	free(indices);
}

static size_t flatten_index(const tfinite_index *shape,
			    const tfinite_index *index)
{
	size_t flat = 0;
	for (size_t i = 0; i < tfinite_index_rank(shape); i++)
		flat = flat * tfinite_index_at(shape, i) + tfinite_index_at(index, i);
	return flat;
}

static size_t index_cardinality(const tfinite_index *shape, const char *where)
{
	size_t total = 1;
	for (size_t i = 0; i < tfinite_index_rank(shape); i++) {
		size_t size = tfinite_index_at(shape, i);
		if (!size || total > SIZE_MAX / size)
			twarn(ErrRuntime_ParamsType, where,
			      "shape must be positive and have finite cardinality");
		total *= size;
	}
	return total;
}

static void finite_map(tobj *params, uint_regs len, tobj *result,
		       tcompo_env *environment)
{
	tstdlib_require_arguments("finite::map", len, 3);
	tfinite_distribution *source = require_distribution(&params[0],
							       "finite::map");
	tfinite_index *target_shape = require_index(&params[1], "finite::map");
	for (size_t i = 0; i < tfinite_index_rank(target_shape); i++)
		if (!tfinite_index_at(target_shape, i))
			twarn(ErrRuntime_ParamsType, "finite::map",
			      "shape components must be positive");
	size_t source_count = tfinite_distribution_cardinality(source);
	size_t target_count = index_cardinality(target_shape, "finite::map");
	double *weights = checked_calloc(target_count, sizeof(double),
					 "finite::map");
	size_t source_rank = tfinite_distribution_rank(source);
	size_t *coordinates = checked_calloc(source_rank, sizeof(size_t),
					     "finite::map");
	for (size_t flat = 0; flat < source_count; flat++) {
		size_t remainder = flat;
		for (size_t i = source_rank; i-- > 0;) {
			size_t size = tfinite_distribution_shape_at(source, i);
			coordinates[i] = remainder % size;
			remainder /= size;
		}
		tobj argument;
		tobj mapped;
		tobj_set_nil(&argument);
		tobj_set_nil(&mapped);
		tfinite_index *source_index = tfinite_index_new(coordinates,
							       source_rank);
		tobj_set_compo(&argument, (tcompo_v *)source_index);
		argument.val.v_tcompo->refctr++;
		tvm_services_invoke(environment, &params[2], &argument, 1, &mapped);
		tfinite_index *target = require_index(&mapped, "finite::map");
		if (tfinite_index_rank(target) != tfinite_index_rank(target_shape))
			twarn(ErrRuntime_ParamsType, "finite::map",
			      "mapping result does not match target shape");
		for (size_t i = 0; i < tfinite_index_rank(target_shape); i++)
			if (tfinite_index_at(target, i) >=
			    tfinite_index_at(target_shape, i))
				twarn(ErrRuntime_ParamsType, "finite::map",
				      "mapping result does not match target shape");
		weights[flatten_index(target_shape, target)] +=
			tfinite_distribution_prob(source, source_index);
		tobj_try_clear(&mapped);
		tobj_ddc_ref_clear(&argument);
	}
	size_t target_rank = tfinite_index_rank(target_shape);
	size_t *shape = checked_calloc(target_rank, sizeof(size_t), "finite::map");
	for (size_t i = 0; i < target_rank; i++)
		shape[i] = tfinite_index_at(target_shape, i);
	tobj_set_compo(result, (tcompo_v *)tfinite_dense_new(
		shape, target_rank, weights, target_count));
	free(shape);
	free(coordinates);
	free(weights);
}

static void finite_bernoulli(tobj *params, uint_regs len, tobj *result)
{
	tstdlib_require_arguments("finite::bernoulli", len, 1);
	double probability = probability_value(&params[0], "finite::bernoulli");
	double weights[2] = {1.0 - probability, probability};
	size_t shape = 2;
	tobj_set_compo(result, (tcompo_v *)tfinite_dense_new(
		&shape, 1, weights, 2));
}

static void finite_binomial(tobj *params, uint_regs len, tobj *result)
{
	tstdlib_require_arguments("finite::binomial", len, 2);
	size_t trials = nonnegative_size(&params[0], "finite::binomial",
					 "trials must be a nonnegative Int");
	if (trials == (size_t)LONG_MAX)
		twarn(ErrRuntime_ParamsType, "finite::binomial", "shape overflow");
	double probability = probability_value(&params[1], "finite::binomial");
	tobj_set_compo(result, (tcompo_v *)tfinite_binomial_new(
		trials, probability));
}

static void finite_multinomial(tobj *params, uint_regs len, tobj *result)
{
	tstdlib_require_arguments("finite::multinomial", len, 2);
	size_t trials = nonnegative_size(&params[0], "finite::multinomial",
					 "trials must be a nonnegative Int");
	if (trials == (size_t)LONG_MAX)
		twarn(ErrRuntime_ParamsType, "finite::multinomial", "shape overflow");
	size_t count;
	double *weights = weights_from_list(&params[1], &count,
					   "finite::multinomial");
	tobj_set_compo(result, (tcompo_v *)tfinite_multinomial_new(
		trials, weights, count));
	free(weights);
}

static void finite_hypergeometric(tobj *params, uint_regs len, tobj *result)
{
	tstdlib_require_arguments("finite::hypergeometric", len, 3);
	size_t population = positive_size(&params[0], "finite::hypergeometric",
					  "population must be a positive Int");
	size_t successes = nonnegative_size(&params[1],
		"finite::hypergeometric", "successes must be a nonnegative Int");
	size_t draws = nonnegative_size(&params[2], "finite::hypergeometric",
					"draws must be a nonnegative Int");
	if (successes > population || draws > population ||
	    draws == (size_t)LONG_MAX)
		twarn(ErrRuntime_ParamsType, "finite::hypergeometric",
		      "parameters violate finite population constraints");
	tobj_set_compo(result, (tcompo_v *)tfinite_hypergeometric_new(
		population, successes, draws));
}

static void finite_zipf(tobj *params, uint_regs len, tobj *result)
{
	tstdlib_require_arguments("finite::zipf", len, 2);
	size_t size = positive_size(&params[0], "finite::zipf",
				    "n must be a positive Int");
	if (params[1].type != tfloat || !isfinite(params[1].val.v_tfloat) ||
	    params[1].val.v_tfloat <= 0.0)
		twarn(ErrRuntime_ParamsType, "finite::zipf",
		      "exponent must be a positive finite Float");
	tobj_set_compo(result, (tcompo_v *)tfinite_zipf_new(
		size, params[1].val.v_tfloat));
}

static const textension_symbol symbols[] = {
	{
		.name = "Index",
		.type = "Type",
		.detail = "finite::Index: Type",
		.kind = textension_type,
		.value_factory = create_index_type
	},
	{
		.name = "Distribution",
		.type = "Type",
		.detail = "finite::Distribution: Type",
		.kind = textension_type,
		.value_factory = create_distribution_type
	},
	TAPAS_NATIVE_FUNCTION_DETAIL("index", finite_index, 1, 1,
		"Function[List[Int]] -> Index",
		"finite::index(coords: List[Int]) -> Index"),
	TAPAS_NATIVE_FUNCTION_DETAIL("shape", finite_shape, 1, 1,
		"Function[Distribution] -> Index",
		"finite::shape(dist: Distribution) -> Index"),
	TAPAS_NATIVE_FUNCTION_DETAIL("prob", finite_prob, 2, 2,
		"Function[Distribution, Index] -> Float",
		"finite::prob(dist: Distribution, index: Index) -> Float"),
	TAPAS_NATIVE_FUNCTION_DETAIL("mass", finite_mass, 2, 2,
		"Function[Distribution, List[Index]] -> Float",
		"finite::mass(dist: Distribution, indices: List[Index]) -> Float"),
	TAPAS_NATIVE_FUNCTION_DETAIL("samples", finite_samples, 3, 3,
		"Function[Distribution, Int, random::Generator] -> List[Index]",
		"finite::samples(dist: Distribution, count: Int, "
		"rng: random::Generator) -> List[Index]"),
	TAPAS_NATIVE_FUNCTION_DETAIL("uniform", finite_uniform, 1, 1,
		"Function[Int] -> Distribution",
		"finite::uniform(n: Int) -> Distribution"),
	TAPAS_NATIVE_FUNCTION_DETAIL("categorical", finite_categorical, 1, 1,
		"Function[List[Float]] -> Distribution",
		"finite::categorical(weights: List[Float]) -> Distribution"),
	TAPAS_NATIVE_FUNCTION_DETAIL("piecewise_constant",
		finite_piecewise_constant, 2, 2,
		"Function[Int, List[Pair[Int, Float]]] -> Distribution",
		"finite::piecewise_constant(n: Int, "
		"steps: List[Pair[Int, Float]]) -> Distribution"),
	TAPAS_NATIVE_FUNCTION_DETAIL("piecewise_linear", finite_piecewise_linear,
		1, 1, "Function[List[Pair[Int, Float]]] -> Distribution",
		"finite::piecewise_linear(knots: List[Pair[Int, Float]]) "
		"-> Distribution"),
	TAPAS_NATIVE_FUNCTION_DETAIL("product", finite_product, 1, 1,
		"Function[List[Distribution]] -> Distribution",
		"finite::product(dists: List[Distribution]) -> Distribution"),
	TAPAS_NATIVE_FUNCTION_DETAIL("mixture", finite_mixture, 2, 2,
		"Function[List[Distribution], List[Float]] -> Distribution",
		"finite::mixture(dists: List[Distribution], "
		"weights: List[Float]) -> Distribution"),
	TAPAS_NATIVE_FUNCTION_DETAIL("conditional", finite_conditional, 2, 2,
		"Function[Distribution, List[Index]] -> Distribution",
		"finite::conditional(dist: Distribution, "
		"indices: List[Index]) -> Distribution"),
	{
		.name = "map",
		.type = "Function[Distribution, Index, Function[Index] -> Index] "
			"-> Distribution",
		.detail = "finite::map(dist: Distribution, shape: Index, "
			"mapping: Function[Index] -> Index) -> Distribution",
		.kind = textension_function,
		.session_function = finite_map,
		.minimum_arguments = 3,
		.maximum_arguments = 3
	},
	TAPAS_NATIVE_FUNCTION_DETAIL("bernoulli", finite_bernoulli, 1, 1,
		"Function[Float] -> Distribution",
		"finite::bernoulli(p: Float) -> Distribution"),
	TAPAS_NATIVE_FUNCTION_DETAIL("binomial", finite_binomial, 2, 2,
		"Function[Int, Float] -> Distribution",
		"finite::binomial(trials: Int, p: Float) -> Distribution"),
	TAPAS_NATIVE_FUNCTION_DETAIL("multinomial", finite_multinomial, 2, 2,
		"Function[Int, List[Float]] -> Distribution",
		"finite::multinomial(trials: Int, weights: List[Float]) "
		"-> Distribution"),
	TAPAS_NATIVE_FUNCTION_DETAIL("hypergeometric", finite_hypergeometric,
		3, 3, "Function[Int, Int, Int] -> Distribution",
		"finite::hypergeometric(population: Int, successes: Int, "
		"draws: Int) -> Distribution"),
	TAPAS_NATIVE_FUNCTION_DETAIL("zipf", finite_zipf, 2, 2,
		"Function[Int, Float] -> Distribution",
		"finite::zipf(n: Int, exponent: Float) -> Distribution")
};

const textension_module tstdlib_finite_module = {
	.scope = textension_package,
	.name = "finite",
	.detail = "Probability distributions on finite index spaces",
	.symbols = symbols,
	.symbol_count = sizeof(symbols) / sizeof(symbols[0])
};
