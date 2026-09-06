/**
 * @file object.c
 * @brief Implements finite package objects and probability algorithms.
 */
#include "object.h"

#include "../random/object.h"

#include "tapas/dsa/tstring.h"
#include "tapas/objects/ttype.h"
#include "tapas/textension.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
	finite_uniform,
	finite_dense,
	finite_piecewise_constant,
	finite_piecewise_linear,
	finite_product,
	finite_mixture,
	finite_conditional,
	finite_binomial,
	finite_multinomial,
	finite_hypergeometric
} tfinite_kind;

typedef struct tfinite_payload tfinite_payload;

typedef struct {
	double *probabilities;
	double *cumulative;
	size_t count;
} tfinite_table;

typedef struct {
	size_t *points;
	double *weights;
	long double *cumulative;
	long double total;
	size_t count;
} tfinite_segments;

typedef struct {
	tfinite_payload **items;
	size_t *offsets;
	size_t count;
} tfinite_product_data;

typedef struct {
	tfinite_payload **items;
	double *probabilities;
	double *cumulative;
	size_t count;
} tfinite_mixture_data;

typedef struct {
	size_t *coordinates;
	double *probabilities;
	double *cumulative;
	size_t count;
} tfinite_conditional_data;

typedef struct {
	size_t trials;
	double probability;
} tfinite_binomial_data;

typedef struct {
	size_t trials;
	double *probabilities;
	double *cumulative;
	size_t count;
} tfinite_multinomial_data;

typedef struct {
	size_t population;
	size_t successes;
	size_t draws;
} tfinite_hypergeometric_data;

struct tfinite_payload {
	size_t references;
	tfinite_kind kind;
	size_t rank;
	size_t *shape;
	union {
		size_t uniform_size;
		tfinite_table dense;
		tfinite_segments segments;
		tfinite_product_data product;
		tfinite_mixture_data mixture;
		tfinite_conditional_data conditional;
		tfinite_binomial_data binomial;
		tfinite_multinomial_data multinomial;
		tfinite_hypergeometric_data hypergeometric;
	} data;
};

struct tfinite_index {
	tcompo_v base;
	size_t rank;
	size_t *coordinates;
};

struct tfinite_distribution {
	tcompo_v base;
	tfinite_payload *payload;
};

static void *checked_calloc(size_t count, size_t size, const char *where)
{
	if (size && count > SIZE_MAX / size)
		twarn(ErrRuntime_Other, where, "allocation size overflow");
	void *memory = calloc(count, size);
	if (!memory && count)
		twarn(ErrRuntime_Other, where, "out of memory");
	return memory;
}

static size_t shape_cardinality(const size_t *shape, size_t rank)
{
	size_t result = 1;
	for (size_t i = 0; i < rank; i++) {
		if (!shape[i] || result > SIZE_MAX / shape[i])
			twarn(ErrRuntime_Other, "finite shape",
			      "shape cardinality overflow");
		result *= shape[i];
	}
	return result;
}

static tfinite_payload *payload_new(tfinite_kind kind,
				     const size_t *shape, size_t rank)
{
	tfinite_payload *payload = checked_calloc(1, sizeof(*payload),
					     "finite distribution");
	payload->references = 1;
	payload->kind = kind;
	payload->rank = rank;
	payload->shape = checked_calloc(rank, sizeof(*payload->shape),
					  "finite shape");
	memcpy(payload->shape, shape, rank * sizeof(*shape));
	return payload;
}

static tfinite_payload *payload_retain(tfinite_payload *payload)
{
	payload->references++;
	return payload;
}

static void payload_release(tfinite_payload *payload)
{
	if (!payload || --payload->references)
		return;
	switch (payload->kind) {
	case finite_dense:
		free(payload->data.dense.probabilities);
		free(payload->data.dense.cumulative);
		break;
	case finite_piecewise_constant:
	case finite_piecewise_linear:
		free(payload->data.segments.points);
		free(payload->data.segments.weights);
		free(payload->data.segments.cumulative);
		break;
	case finite_product:
		for (size_t i = 0; i < payload->data.product.count; i++)
			payload_release(payload->data.product.items[i]);
		free(payload->data.product.items);
		free(payload->data.product.offsets);
		break;
	case finite_mixture:
		for (size_t i = 0; i < payload->data.mixture.count; i++)
			payload_release(payload->data.mixture.items[i]);
		free(payload->data.mixture.items);
		free(payload->data.mixture.probabilities);
		free(payload->data.mixture.cumulative);
		break;
	case finite_conditional:
		free(payload->data.conditional.coordinates);
		free(payload->data.conditional.probabilities);
		free(payload->data.conditional.cumulative);
		break;
	case finite_multinomial:
		free(payload->data.multinomial.probabilities);
		free(payload->data.multinomial.cumulative);
		break;
	case finite_uniform:
	case finite_binomial:
	case finite_hypergeometric:
		break;
	}
	free(payload->shape);
	free(payload);
}

static tfinite_distribution *distribution_new(tfinite_payload *payload)
{
	tfinite_distribution *distribution = checked_calloc(
		1, sizeof(*distribution), "finite distribution");
	distribution->base.vtable = &tfinite_distribution_vtable;
	distribution->payload = payload;
	return distribution;
}

static void normalize_table(const double *weights, size_t count,
			    double *probabilities, double *cumulative,
			    const char *where)
{
	long double total = 0.0L;
	for (size_t i = 0; i < count; i++) {
		if (!isfinite(weights[i]) || weights[i] < 0.0)
			twarn(ErrRuntime_ParamsType, where,
			      "weights must be finite and nonnegative");
		total += (long double)weights[i];
	}
	if (!(total > 0.0L) || !isfinite(total))
		twarn(ErrRuntime_ParamsType, where,
		      "total weight must be finite and positive");
	long double sum = 0.0L;
	for (size_t i = 0; i < count; i++) {
		probabilities[i] = (double)((long double)weights[i] / total);
		sum += weights[i];
		cumulative[i] = (double)(sum / total);
	}
	cumulative[count - 1] = 1.0;
}

static size_t table_sample(const double *cumulative, size_t count,
			   trandom_generator *generator)
{
	double target = trandom_generator_unit(generator);
	size_t low = 0;
	size_t high = count;
	while (low < high) {
		size_t middle = low + (high - low) / 2;
		if (target < cumulative[middle])
			high = middle;
		else
			low = middle + 1;
	}
	return low == count ? count - 1 : low;
}

tfinite_index *tfinite_index_new(const size_t *coordinates, size_t rank)
{
	tfinite_index *index = checked_calloc(1, sizeof(*index), "finite::index");
	index->base.vtable = &tfinite_index_vtable;
	index->rank = rank;
	index->coordinates = checked_calloc(rank, sizeof(*index->coordinates),
					      "finite::index");
	memcpy(index->coordinates, coordinates, rank * sizeof(*coordinates));
	return index;
}

size_t tfinite_index_rank(const tfinite_index *index)
{
	return index->rank;
}

size_t tfinite_index_at(const tfinite_index *index, size_t dimension)
{
	return index->coordinates[dimension];
}

tfinite_distribution *tfinite_uniform_new(size_t size)
{
	tfinite_payload *payload = payload_new(finite_uniform, &size, 1);
	payload->data.uniform_size = size;
	return distribution_new(payload);
}

tfinite_distribution *tfinite_dense_new(const size_t *shape, size_t rank,
					 const double *weights, size_t count)
{
	if (shape_cardinality(shape, rank) != count)
		twarn(ErrRuntime_ParamsType, "finite dense distribution",
		      "weight count does not match shape");
	tfinite_payload *payload = payload_new(finite_dense, shape, rank);
	tfinite_table *table = &payload->data.dense;
	table->count = count;
	table->probabilities = checked_calloc(count, sizeof(double),
						"finite dense distribution");
	table->cumulative = checked_calloc(count, sizeof(double),
					     "finite dense distribution");
	normalize_table(weights, count, table->probabilities, table->cumulative,
			"finite dense distribution");
	return distribution_new(payload);
}

tfinite_distribution *tfinite_piecewise_constant_new(
				size_t size, const size_t *starts,
				const double *weights, size_t count)
{
	tfinite_payload *payload = payload_new(finite_piecewise_constant,
					       &size, 1);
	tfinite_segments *segments = &payload->data.segments;
	segments->count = count;
	segments->points = checked_calloc(count, sizeof(size_t),
					    "finite::piecewise_constant");
	segments->weights = checked_calloc(count, sizeof(double),
					     "finite::piecewise_constant");
	segments->cumulative = checked_calloc(count, sizeof(long double),
						"finite::piecewise_constant");
	memcpy(segments->points, starts, count * sizeof(size_t));
	memcpy(segments->weights, weights, count * sizeof(double));
	long double total = 0.0L;
	for (size_t i = 0; i < count; i++) {
		size_t end = i + 1 < count ? starts[i + 1] : size;
		total += (long double)weights[i] * (long double)(end - starts[i]);
		segments->cumulative[i] = total;
	}
	segments->total = total;
	return distribution_new(payload);
}

static long double linear_prefix(double first, double slope, size_t count)
{
	return (long double)count * (long double)first +
		(long double)slope * (long double)count *
		(long double)(count - (count != 0)) / 2.0L;
}

tfinite_distribution *tfinite_piecewise_linear_new(
				const size_t *coordinates,
				const double *weights, size_t count)
{
	size_t size = coordinates[count - 1] + 1;
	tfinite_payload *payload = payload_new(finite_piecewise_linear, &size, 1);
	tfinite_segments *segments = &payload->data.segments;
	segments->count = count;
	segments->points = checked_calloc(count, sizeof(size_t),
					    "finite::piecewise_linear");
	segments->weights = checked_calloc(count, sizeof(double),
					     "finite::piecewise_linear");
	segments->cumulative = checked_calloc(count, sizeof(long double),
						"finite::piecewise_linear");
	memcpy(segments->points, coordinates, count * sizeof(size_t));
	memcpy(segments->weights, weights, count * sizeof(double));
	long double total = 0.0L;
	for (size_t i = 0; i < count; i++) {
		if (i + 1 == count) {
			total += weights[i];
		} else {
			size_t width = coordinates[i + 1] - coordinates[i];
			double slope = (weights[i + 1] - weights[i]) /
				(double)width;
			total += linear_prefix(weights[i], slope, width);
		}
		segments->cumulative[i] = total;
	}
	segments->total = total;
	return distribution_new(payload);
}

tfinite_distribution *tfinite_product_new(
				tfinite_distribution *const *distributions,
				size_t count)
{
	size_t rank = 0;
	for (size_t i = 0; i < count; i++) {
		if (rank > SIZE_MAX - distributions[i]->payload->rank)
			twarn(ErrRuntime_Other, "finite::product", "rank overflow");
		rank += distributions[i]->payload->rank;
	}
	size_t *shape = checked_calloc(rank, sizeof(size_t), "finite::product");
	size_t offset = 0;
	for (size_t i = 0; i < count; i++) {
		tfinite_payload *child = distributions[i]->payload;
		memcpy(shape + offset, child->shape,
		       child->rank * sizeof(size_t));
		offset += child->rank;
	}
	tfinite_payload *payload = payload_new(finite_product, shape, rank);
	free(shape);
	tfinite_product_data *product = &payload->data.product;
	product->count = count;
	product->items = checked_calloc(count, sizeof(*product->items),
					  "finite::product");
	product->offsets = checked_calloc(count, sizeof(*product->offsets),
					    "finite::product");
	offset = 0;
	for (size_t i = 0; i < count; i++) {
		product->items[i] = payload_retain(distributions[i]->payload);
		product->offsets[i] = offset;
		offset += distributions[i]->payload->rank;
	}
	return distribution_new(payload);
}

tfinite_distribution *tfinite_mixture_new(
				tfinite_distribution *const *distributions,
				const double *weights, size_t count)
{
	tfinite_payload *first = distributions[0]->payload;
	tfinite_payload *payload = payload_new(finite_mixture, first->shape,
					       first->rank);
	tfinite_mixture_data *mixture = &payload->data.mixture;
	mixture->count = count;
	mixture->items = checked_calloc(count, sizeof(*mixture->items),
					  "finite::mixture");
	mixture->probabilities = checked_calloc(count, sizeof(double),
						  "finite::mixture");
	mixture->cumulative = checked_calloc(count, sizeof(double),
					       "finite::mixture");
	for (size_t i = 0; i < count; i++)
		mixture->items[i] = payload_retain(distributions[i]->payload);
	normalize_table(weights, count, mixture->probabilities,
			mixture->cumulative, "finite::mixture");
	return distribution_new(payload);
}

static int coordinates_equal(const size_t *left, const size_t *right,
			     size_t rank)
{
	return memcmp(left, right, rank * sizeof(size_t)) == 0;
}

static double payload_prob(const tfinite_payload *payload,
			   const size_t *coordinates);

tfinite_distribution *tfinite_conditional_new(
				tfinite_distribution *distribution,
				tfinite_index *const *indices, size_t count)
{
	tfinite_payload *source = distribution->payload;
	double *raw = checked_calloc(count, sizeof(double),
				     "finite::conditional");
	size_t unique = 0;
	for (size_t i = 0; i < count; i++) {
		int duplicate = 0;
		for (size_t j = 0; j < i; j++) {
			if (coordinates_equal(indices[i]->coordinates,
					      indices[j]->coordinates,
					      source->rank)) {
				duplicate = 1;
				break;
			}
		}
		if (!duplicate)
			raw[unique++] = payload_prob(source, indices[i]->coordinates);
	}
	tfinite_payload *payload = payload_new(finite_conditional, source->shape,
					       source->rank);
	tfinite_conditional_data *conditional = &payload->data.conditional;
	conditional->count = unique;
	conditional->coordinates = checked_calloc(unique * source->rank,
						      sizeof(size_t),
						      "finite::conditional");
	conditional->probabilities = checked_calloc(unique, sizeof(double),
						  "finite::conditional");
	conditional->cumulative = checked_calloc(unique, sizeof(double),
						"finite::conditional");
	size_t position = 0;
	for (size_t i = 0; i < count; i++) {
		int duplicate = 0;
		for (size_t j = 0; j < i; j++)
			if (coordinates_equal(indices[i]->coordinates,
					      indices[j]->coordinates,
					      source->rank))
				duplicate = 1;
		if (!duplicate) {
			memcpy(conditional->coordinates + position * source->rank,
			       indices[i]->coordinates,
			       source->rank * sizeof(size_t));
			position++;
		}
	}
	normalize_table(raw, unique, conditional->probabilities,
			conditional->cumulative, "finite::conditional");
	free(raw);
	return distribution_new(payload);
}

tfinite_distribution *tfinite_binomial_new(size_t trials, double probability)
{
	size_t size = trials + 1;
	tfinite_payload *payload = payload_new(finite_binomial, &size, 1);
	payload->data.binomial.trials = trials;
	payload->data.binomial.probability = probability;
	return distribution_new(payload);
}

tfinite_distribution *tfinite_multinomial_new(size_t trials,
				const double *weights, size_t count)
{
	size_t *shape = checked_calloc(count, sizeof(size_t),
					 "finite::multinomial");
	for (size_t i = 0; i < count; i++)
		shape[i] = trials + 1;
	tfinite_payload *payload = payload_new(finite_multinomial, shape, count);
	free(shape);
	tfinite_multinomial_data *data = &payload->data.multinomial;
	data->trials = trials;
	data->count = count;
	data->probabilities = checked_calloc(count, sizeof(double),
						"finite::multinomial");
	data->cumulative = checked_calloc(count, sizeof(double),
					     "finite::multinomial");
	normalize_table(weights, count, data->probabilities, data->cumulative,
			"finite::multinomial");
	return distribution_new(payload);
}

tfinite_distribution *tfinite_hypergeometric_new(size_t population,
				size_t successes, size_t draws)
{
	size_t size = draws + 1;
	tfinite_payload *payload = payload_new(finite_hypergeometric, &size, 1);
	payload->data.hypergeometric.population = population;
	payload->data.hypergeometric.successes = successes;
	payload->data.hypergeometric.draws = draws;
	return distribution_new(payload);
}

tfinite_distribution *tfinite_zipf_new(size_t size, double exponent)
{
	double *weights = checked_calloc(size, sizeof(double), "finite::zipf");
	for (size_t i = 0; i < size; i++)
		weights[i] = pow((double)(i + 1), -exponent);
	tfinite_distribution *distribution = tfinite_dense_new(&size, 1, weights,
								 size);
	free(weights);
	return distribution;
}

size_t tfinite_distribution_rank(const tfinite_distribution *distribution)
{
	return distribution->payload->rank;
}

size_t tfinite_distribution_shape_at(
				const tfinite_distribution *distribution,
				size_t dimension)
{
	return distribution->payload->shape[dimension];
}

size_t tfinite_distribution_cardinality(
				const tfinite_distribution *distribution)
{
	return shape_cardinality(distribution->payload->shape,
				 distribution->payload->rank);
}

int tfinite_distribution_contains(const tfinite_distribution *distribution,
				  const tfinite_index *index)
{
	if (index->rank != distribution->payload->rank)
		return 0;
	for (size_t i = 0; i < index->rank; i++)
		if (index->coordinates[i] >= distribution->payload->shape[i])
			return 0;
	return 1;
}

static size_t flatten(const tfinite_payload *payload, const size_t *coordinates)
{
	size_t flat = 0;
	for (size_t i = 0; i < payload->rank; i++)
		flat = flat * payload->shape[i] + coordinates[i];
	return flat;
}

static void unflatten(const tfinite_payload *payload, size_t flat,
			      size_t *coordinates)
{
	for (size_t i = payload->rank; i-- > 0;) {
		coordinates[i] = flat % payload->shape[i];
		flat /= payload->shape[i];
	}
}

static size_t find_segment(const size_t *points, size_t count, size_t value)
{
	size_t low = 0;
	size_t high = count;
	while (low + 1 < high) {
		size_t middle = low + (high - low) / 2;
		if (points[middle] <= value)
			low = middle;
		else
			high = middle;
	}
	return low;
}

static long double log_combination(size_t n, size_t k)
{
	if (k > n)
		return -INFINITY;
	return lgammal((long double)n + 1.0L) -
		lgammal((long double)k + 1.0L) -
		lgammal((long double)(n - k) + 1.0L);
}

static double payload_prob(const tfinite_payload *payload,
			   const size_t *coordinates)
{
	switch (payload->kind) {
	case finite_uniform:
		return 1.0 / (double)payload->data.uniform_size;
	case finite_dense:
		return payload->data.dense.probabilities[
			flatten(payload, coordinates)];
	case finite_piecewise_constant: {
		tfinite_segments *segments = (tfinite_segments *)&payload->data.segments;
		size_t segment = find_segment(segments->points, segments->count,
					      coordinates[0]);
		return (double)((long double)segments->weights[segment] /
				segments->total);
	}
	case finite_piecewise_linear: {
		tfinite_segments *segments = (tfinite_segments *)&payload->data.segments;
		size_t segment = find_segment(segments->points, segments->count,
					      coordinates[0]);
		double weight = segments->weights[segment];
		if (segment + 1 < segments->count) {
			size_t width = segments->points[segment + 1] -
				segments->points[segment];
			double slope = (segments->weights[segment + 1] - weight) /
				(double)width;
			weight += slope * (double)(coordinates[0] -
						  segments->points[segment]);
		}
		return (double)((long double)weight / segments->total);
	}
	case finite_product: {
		double probability = 1.0;
		for (size_t i = 0; i < payload->data.product.count; i++)
			probability *= payload_prob(payload->data.product.items[i],
				coordinates + payload->data.product.offsets[i]);
		return probability;
	}
	case finite_mixture: {
		double probability = 0.0;
		for (size_t i = 0; i < payload->data.mixture.count; i++)
			probability += payload->data.mixture.probabilities[i] *
				payload_prob(payload->data.mixture.items[i], coordinates);
		return probability;
	}
	case finite_conditional:
		for (size_t i = 0; i < payload->data.conditional.count; i++)
			if (coordinates_equal(coordinates,
				payload->data.conditional.coordinates + i * payload->rank,
				payload->rank))
				return payload->data.conditional.probabilities[i];
		return 0.0;
	case finite_binomial: {
		size_t trials = payload->data.binomial.trials;
		size_t successes = coordinates[0];
		double p = payload->data.binomial.probability;
		if (p == 0.0)
			return successes == 0 ? 1.0 : 0.0;
		if (p == 1.0)
			return successes == trials ? 1.0 : 0.0;
		long double log_probability = log_combination(trials, successes) +
			(long double)successes * logl(p) +
			(long double)(trials - successes) * log1pl(-p);
		return (double)expl(log_probability);
	}
	case finite_multinomial: {
		tfinite_multinomial_data *data =
			(tfinite_multinomial_data *)&payload->data.multinomial;
		size_t total = 0;
		long double log_probability = lgammal((long double)data->trials + 1.0L);
		for (size_t i = 0; i < data->count; i++) {
			if (total > data->trials ||
			    coordinates[i] > data->trials - total)
				return 0.0;
			total += coordinates[i];
			log_probability -= lgammal((long double)coordinates[i] + 1.0L);
			if (coordinates[i] && data->probabilities[i] == 0.0)
				return 0.0;
			if (coordinates[i])
				log_probability += (long double)coordinates[i] *
					logl(data->probabilities[i]);
		}
		return total == data->trials ? (double)expl(log_probability) : 0.0;
	}
	case finite_hypergeometric: {
		tfinite_hypergeometric_data data = payload->data.hypergeometric;
		size_t successes = coordinates[0];
		if (successes > data.successes || successes > data.draws ||
		    data.draws - successes > data.population - data.successes)
			return 0.0;
		long double value = log_combination(data.successes, successes) +
			log_combination(data.population - data.successes,
					data.draws - successes) -
			log_combination(data.population, data.draws);
		return (double)expl(value);
	}
	}
	return 0.0;
}

double tfinite_distribution_prob(const tfinite_distribution *distribution,
				 const tfinite_index *index)
{
	return payload_prob(distribution->payload, index->coordinates);
}

double tfinite_distribution_prob_coordinates(
				 const tfinite_distribution *distribution,
				 const size_t *coordinates, size_t rank)
{
	if (distribution->payload->rank != rank)
		return 0.0;
	for (size_t i = 0; i < rank; i++)
		if (coordinates[i] >= distribution->payload->shape[i])
			return 0.0;
	return payload_prob(distribution->payload, coordinates);
}

double tfinite_distribution_prob_at(
				 const tfinite_distribution *distribution,
				 size_t coordinate)
{
	return tfinite_distribution_prob_coordinates(
		distribution, &coordinate, 1);
}

static size_t segment_sample(const long double *cumulative, size_t count,
			     long double target)
{
	size_t low = 0;
	size_t high = count;
	while (low < high) {
		size_t middle = low + (high - low) / 2;
		if (target < cumulative[middle])
			high = middle;
		else
			low = middle + 1;
	}
	return low == count ? count - 1 : low;
}

static void payload_sample(const tfinite_payload *payload,
			   trandom_generator *generator, size_t *coordinates)
{
	switch (payload->kind) {
	case finite_uniform:
		coordinates[0] = trandom_generator_bounded(generator,
						payload->data.uniform_size);
		break;
	case finite_dense: {
		size_t flat = table_sample(payload->data.dense.cumulative,
					   payload->data.dense.count, generator);
		unflatten(payload, flat, coordinates);
		break;
	}
	case finite_piecewise_constant: {
		tfinite_segments *segments = (tfinite_segments *)&payload->data.segments;
		long double target = (long double)trandom_generator_unit(generator) *
			segments->total;
		size_t segment = segment_sample(segments->cumulative,
						segments->count, target);
		size_t end = segment + 1 < segments->count ?
			segments->points[segment + 1] : payload->shape[0];
		coordinates[0] = segments->points[segment] +
			trandom_generator_bounded(generator,
						  end - segments->points[segment]);
		break;
	}
	case finite_piecewise_linear: {
		tfinite_segments *segments = (tfinite_segments *)&payload->data.segments;
		long double target = (long double)trandom_generator_unit(generator) *
			segments->total;
		size_t segment = segment_sample(segments->cumulative,
						segments->count, target);
		long double prior = segment ? segments->cumulative[segment - 1] : 0.0L;
		target -= prior;
		if (segment + 1 == segments->count) {
			coordinates[0] = segments->points[segment];
			break;
		}
		size_t width = segments->points[segment + 1] -
			segments->points[segment];
		double slope = (segments->weights[segment + 1] -
			segments->weights[segment]) / (double)width;
		size_t low = 0;
		size_t high = width;
		while (low < high) {
			size_t middle = low + (high - low) / 2;
			if (target < linear_prefix(segments->weights[segment],
						  slope, middle + 1))
				high = middle;
			else
				low = middle + 1;
		}
		coordinates[0] = segments->points[segment] +
			(low == width ? width - 1 : low);
		break;
	}
	case finite_product:
		for (size_t i = 0; i < payload->data.product.count; i++)
			payload_sample(payload->data.product.items[i], generator,
				coordinates + payload->data.product.offsets[i]);
		break;
	case finite_mixture: {
		size_t chosen = table_sample(payload->data.mixture.cumulative,
					     payload->data.mixture.count,
					     generator);
		payload_sample(payload->data.mixture.items[chosen], generator,
			       coordinates);
		break;
	}
	case finite_conditional: {
		size_t chosen = table_sample(payload->data.conditional.cumulative,
					     payload->data.conditional.count,
					     generator);
		memcpy(coordinates,
		       payload->data.conditional.coordinates + chosen * payload->rank,
		       payload->rank * sizeof(size_t));
		break;
	}
	case finite_binomial: {
		size_t successes = 0;
		for (size_t i = 0; i < payload->data.binomial.trials; i++)
			if (trandom_generator_unit(generator) <
			    payload->data.binomial.probability)
				successes++;
		coordinates[0] = successes;
		break;
	}
	case finite_multinomial:
		memset(coordinates, 0, payload->rank * sizeof(size_t));
		for (size_t i = 0; i < payload->data.multinomial.trials; i++) {
			size_t category = table_sample(
				payload->data.multinomial.cumulative,
				payload->data.multinomial.count, generator);
			coordinates[category]++;
		}
		break;
	case finite_hypergeometric: {
		tfinite_hypergeometric_data data = payload->data.hypergeometric;
		size_t successes = 0;
		size_t remaining_successes = data.successes;
		size_t remaining_population = data.population;
		for (size_t i = 0; i < data.draws; i++) {
			uint64_t selected = trandom_generator_bounded(
				generator, remaining_population);
			if (selected < remaining_successes) {
				successes++;
				remaining_successes--;
			}
			remaining_population--;
		}
		coordinates[0] = successes;
		break;
	}
	}
}

tfinite_index *tfinite_distribution_sample(
				const tfinite_distribution *distribution,
				trandom_generator *generator)
{
	size_t rank = distribution->payload->rank;
	size_t *coordinates = checked_calloc(rank, sizeof(size_t),
					      "finite::samples");
	payload_sample(distribution->payload, generator, coordinates);
	tfinite_index *index = tfinite_index_new(coordinates, rank);
	free(coordinates);
	return index;
}

ttypeval *tstdlib_finite_index_type(void)
{
	return ttypeval_new_named("finite::Index");
}

ttypeval *tstdlib_finite_distribution_type(void)
{
	return ttypeval_new_named("finite::Distribution");
}

static const char *index_type(void)
{
	return "finite::Index";
}

static const char *distribution_type(void)
{
	return "finite::Distribution";
}

static tcompo_type finite_code(void)
{
	return compo_extension;
}

static long index_len(void *self)
{
	return (long)((tfinite_index *)self)->rank;
}

static void *index_copy(void *self)
{
	tfinite_index *index = self;
	return tfinite_index_new(index->coordinates, index->rank);
}

static void index_free(void *self)
{
	tfinite_index *index = self;
	free(index->coordinates);
	free(index);
}

static int index_identical(void *self, void *other)
{
	tfinite_index *left = self;
	tfinite_index *right = other;
	return left->rank == right->rank &&
		coordinates_equal(left->coordinates, right->coordinates, left->rank);
}

static tstring *index_string(void *self)
{
	tfinite_index *index = self;
	tstring *text = tstring_new("(");
	for (size_t i = 0; i < index->rank; i++) {
		tstring_append_fmt(text, "%zu", index->coordinates[i]);
		if (i + 1 < index->rank)
			tstring_append(text, ", ");
	}
	if (index->rank == 1)
		tstring_append_c(text, ',');
	tstring_append_c(text, ')');
	return text;
}

static void index_at(void *self, const tobj *arguments, uint_regs count,
			     tobj *result)
{
	tfinite_index *index = self;
	if (count != 1 || arguments[0].type != tint)
		twarn(ErrRuntime_ParamsType, "finite::Index", "Int index required");
	long position = arguments[0].val.v_tint;
	if (position < 0)
		position += (long)index->rank;
	if (position < 0 || (size_t)position >= index->rank)
		twarn(ErrRuntime_IdxOutRange, "finite::Index", "");
	tobj_set_int(result, (long)index->coordinates[position]);
}

static const tcompo_capabilities index_capabilities = {
	.indexable = index_at
};

static long distribution_len(void *self)
{
	return (long)((tfinite_distribution *)self)->payload->rank;
}

static void *distribution_copy(void *self)
{
	tfinite_distribution *distribution = self;
	return distribution_new(payload_retain(distribution->payload));
}

static void distribution_free(void *self)
{
	tfinite_distribution *distribution = self;
	payload_release(distribution->payload);
	free(distribution);
}

static int distribution_identical(void *self, void *other)
{
	return ((tfinite_distribution *)self)->payload ==
		((tfinite_distribution *)other)->payload;
}

static tstring *distribution_string(void *self)
{
	tfinite_distribution *distribution = self;
	tstring *text = tstring_new("Distribution[shape=(");
	for (size_t i = 0; i < distribution->payload->rank; i++) {
		tstring_append_fmt(text, "%zu", distribution->payload->shape[i]);
		if (i + 1 < distribution->payload->rank)
			tstring_append(text, ", ");
	}
	tstring_append(text, ")]");
	return text;
}

tcompo_vtable tfinite_index_vtable = {
	.get_type = index_type,
	.get_compo_type_code = finite_code,
	.len = index_len,
	.copy = index_copy,
	.free = index_free,
	.identical = index_identical,
	.tostring_abbr = index_string,
	.tostring_full = index_string,
	.capabilities = &index_capabilities
};

tcompo_vtable tfinite_distribution_vtable = {
	.get_type = distribution_type,
	.get_compo_type_code = finite_code,
	.len = distribution_len,
	.copy = distribution_copy,
	.free = distribution_free,
	.identical = distribution_identical,
	.tostring_abbr = distribution_string,
	.tostring_full = distribution_string
};
