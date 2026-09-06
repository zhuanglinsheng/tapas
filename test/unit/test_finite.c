#include "../../src/stdlib/finite/object.h"
#include "../../src/stdlib/random/object.h"

#include "tapas/tval.h"

#include <assert.h>
#include <math.h>
#include <stddef.h>

static void assert_close(double actual, double expected)
{
	assert(fabs(actual - expected) < 1e-12);
}

static void destroy_index(tfinite_index *index)
{
	tfinite_index_vtable.free(index);
}

static void destroy_distribution(tfinite_distribution *distribution)
{
	tfinite_distribution_vtable.free(distribution);
}

static void test_index_and_base_distributions(void)
{
	size_t coordinate = 2;
	tfinite_index *index = tfinite_index_new(&coordinate, 1);
	assert(tfinite_index_rank(index) == 1);
	assert(tfinite_index_at(index, 0) == 2);
	assert(tfinite_index_vtable.capabilities->indexable);

	tfinite_distribution *uniform = tfinite_uniform_new(4);
	assert(tfinite_distribution_contains(uniform, index));
	assert_close(tfinite_distribution_prob(uniform, index), 0.25);
	assert_close(tfinite_distribution_prob_coordinates(
		uniform, &coordinate, 1), 0.25);
	assert_close(tfinite_distribution_prob_at(uniform, coordinate), 0.25);
	assert_close(tfinite_distribution_prob_at(uniform, 4), 0.0);

	double weights[] = {1.0, 2.0, 3.0};
	size_t shape = 3;
	tfinite_distribution *categorical = tfinite_dense_new(
		&shape, 1, weights, 3);
	assert_close(tfinite_distribution_prob(categorical, index), 0.5);

	size_t starts[] = {0, 2};
	double step_weights[] = {1.0, 3.0};
	tfinite_distribution *constant = tfinite_piecewise_constant_new(
		5, starts, step_weights, 2);
	assert_close(tfinite_distribution_prob(constant, index), 3.0 / 11.0);

	size_t knots[] = {0, 2};
	double knot_weights[] = {1.0, 3.0};
	tfinite_distribution *linear = tfinite_piecewise_linear_new(
		knots, knot_weights, 2);
	assert_close(tfinite_distribution_prob(linear, index), 0.5);

	destroy_distribution(linear);
	destroy_distribution(constant);
	destroy_distribution(categorical);
	destroy_distribution(uniform);
	destroy_index(index);
}

static void test_compositions(void)
{
	tfinite_distribution *left = tfinite_uniform_new(2);
	double right_weights[] = {1.0, 3.0};
	size_t right_shape = 2;
	tfinite_distribution *right = tfinite_dense_new(
		&right_shape, 1, right_weights, 2);
	tfinite_distribution *children[] = {left, right};
	tfinite_distribution *product = tfinite_product_new(children, 2);
	assert(tfinite_distribution_rank(product) == 2);
	size_t product_coordinates[] = {1, 1};
	tfinite_index *product_index = tfinite_index_new(product_coordinates, 2);
	assert_close(tfinite_distribution_prob(product, product_index), 0.375);

	double mixture_weights[] = {1.0, 1.0};
	tfinite_distribution *mixture = tfinite_mixture_new(
		children, mixture_weights, 2);
	size_t one = 1;
	tfinite_index *one_index = tfinite_index_new(&one, 1);
	assert_close(tfinite_distribution_prob(mixture, one_index), 0.625);

	size_t zero = 0;
	tfinite_index *zero_index = tfinite_index_new(&zero, 1);
	tfinite_index *event[] = {zero_index, one_index, one_index};
	tfinite_distribution *conditional = tfinite_conditional_new(
		right, event, 3);
	assert_close(tfinite_distribution_prob(conditional, zero_index), 0.25);
	assert_close(tfinite_distribution_prob(conditional, one_index), 0.75);

	destroy_distribution(conditional);
	destroy_index(zero_index);
	destroy_index(one_index);
	destroy_distribution(mixture);
	destroy_index(product_index);
	destroy_distribution(product);
	destroy_distribution(right);
	destroy_distribution(left);
}

static void test_named_distributions(void)
{
	size_t one = 1;
	tfinite_index *one_index = tfinite_index_new(&one, 1);
	tfinite_distribution *binomial = tfinite_binomial_new(2, 0.5);
	assert_close(tfinite_distribution_prob(binomial, one_index), 0.5);

	double weights[] = {1.0, 1.0};
	tfinite_distribution *multinomial = tfinite_multinomial_new(2, weights, 2);
	size_t counts[] = {1, 1};
	tfinite_index *count_index = tfinite_index_new(counts, 2);
	assert_close(tfinite_distribution_prob(multinomial, count_index), 0.5);

	tfinite_distribution *hypergeometric =
		tfinite_hypergeometric_new(10, 4, 3);
	assert_close(tfinite_distribution_prob(hypergeometric, one_index), 0.5);

	tfinite_distribution *zipf = tfinite_zipf_new(2, 1.0);
	size_t zero = 0;
	tfinite_index *zero_index = tfinite_index_new(&zero, 1);
	assert_close(tfinite_distribution_prob(zipf, zero_index), 2.0 / 3.0);

	destroy_index(zero_index);
	destroy_distribution(zipf);
	destroy_distribution(hypergeometric);
	destroy_index(count_index);
	destroy_distribution(multinomial);
	destroy_distribution(binomial);
	destroy_index(one_index);
}

static void test_sampling(void)
{
	trandom_source *source = trandom_source_pcg32_xsh_rr();
	trandom_generator *generator = trandom_generator_new(source, 42);
	double weights[] = {0.0, 1.0, 0.0};
	size_t shape = 3;
	tfinite_distribution *distribution = tfinite_dense_new(
		&shape, 1, weights, 3);
	for (size_t i = 0; i < 20; i++) {
		tfinite_index *sample = tfinite_distribution_sample(
			distribution, generator);
		assert(tfinite_index_at(sample, 0) == 1);
		destroy_index(sample);
	}
	destroy_distribution(distribution);
	trandom_generator_vtable.free(generator);
	trandom_source_vtable.free(source);
}

int main(void)
{
	test_index_and_base_distributions();
	test_compositions();
	test_named_distributions();
	test_sampling();
	return 0;
}
