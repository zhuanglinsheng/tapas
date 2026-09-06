/**
 * @file sample.c
 * @brief Implements finite-domain probability sampling for solve.
 */
#include "solve.h"

#include "../finite/object.h"
#include "../random/object.h"

#include "tapas/dsa/thashtbl.h"
#include "tapas/objects/tdict.h"
#include "tapas/objects/tlist.h"
#include "tapas/objects/tpair.h"
#include "tapas/objects/trule_ir.h"
#include "tapas/objects/tstr.h"
#include "tapas/vm_service.h"

#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SAMPLE_PARAMETER_LIMIT UINT8_MAX

typedef struct {
	size_t parameter;
	const tobj *domain;
	size_t size;
	tfinite_distribution *distribution;
	tobj owned_distribution;
} sample_dimension;

typedef struct {
	uint8_t *bound;
	size_t *coordinates;
	size_t count;
} sample_region;

typedef struct {
	sample_region **items;
	size_t count;
	size_t capacity;
} sample_cache;

typedef struct {
	tobj *values;
	size_t *coordinates;
	size_t count;
	uint8_t *working;
	size_t cursor;
	size_t minimum_size;
	size_t *combination;
	int combination_ready;
	int closed;
} sample_task;

typedef struct {
	sample_task **items;
	size_t count;
	size_t capacity;
} sample_tasks;

typedef enum {
	endpoint_unknown,
	endpoint_sat,
	endpoint_unsat
} sample_endpoint;

typedef struct {
	sample_task *task;
	uint8_t *included;  /* I: bindings present in every descendant core. */
	uint8_t *undecided; /* J: bindings still available to descendants. */
	sample_endpoint minimum;
	sample_endpoint maximum;
	size_t created;
	int closed;
} sample_node;

typedef struct {
	sample_node **items;
	size_t count;
	size_t capacity;
	size_t next_created;
} sample_nodes;

typedef struct {
	double deadline;
	size_t steps;
	int timed_out;
} sample_mass_control;

typedef enum {
	generalization_none,
	generalization_full,
	generalization_raw_core,
	generalization_deletion_mus,
	generalization_minimum_core,
	generalization_online_mass_core
} sample_generalization;

typedef enum {
	scheduler_mass_fair,
	scheduler_mass,
	scheduler_fifo,
	scheduler_binding_count
} sample_scheduler;

typedef struct {
	tdict *distributions;
	tfinite_distribution *joint_distribution;
	trandom_generator *generator;
	tobj owned_generator;
	long candidate_limit;
	double time_limit;
	double core_budget_factor;
	long fairness_interval;
	sample_generalization generalization;
	sample_scheduler scheduler;
	int trace;
} sample_options;

static void *sample_calloc(size_t count, size_t size)
{
	if (size && count > SIZE_MAX / size)
		twarn(ErrRuntime_Other, "solve::sample",
		      "allocation size overflow");
	void *memory = calloc(count, size);
	if (!memory && count)
		twarn(ErrRuntime_Other, "solve::sample", "out of memory");
	return memory;
}

static char *sample_strdup(const char *text)
{
	size_t length = strlen(text) + 1;
	char *copy = sample_calloc(length, sizeof(*copy));
	memcpy(copy, text, length);
	return copy;
}

static double monotonic_seconds(void)
{
	struct timespec now;
#if defined(CLOCK_MONOTONIC)
	if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
		twarn(ErrRuntime_Other, "solve::sample",
		      "cannot read monotonic clock");
#else
	if (!timespec_get(&now, TIME_UTC))
		twarn(ErrRuntime_Other, "solve::sample",
		      "cannot read system clock");
#endif
	return (double)now.tv_sec + (double)now.tv_nsec / 1000000000.0;
}

static void sample_field(tdict *dictionary, const char *name,
			 const tobj *value)
{
	tobj key;
	tobj_set_nil(&key);
	tobj_set_compo(&key, (tcompo_v *)tstr_new(name));
	tdict_set(dictionary, &key, value);
	tobj_try_clear(&key);
}

static void sample_string_field(tdict *dictionary, const char *name,
				const char *text)
{
	tobj value;
	tobj_set_nil(&value);
	tobj_set_compo(&value, (tcompo_v *)tstr_new(text));
	sample_field(dictionary, name, &value);
	tobj_try_clear(&value);
}

static const char *parameter_name(const trule_term *parameter)
{
	if (parameter->payload.type != tcompo ||
	    tobj_compo_type(&parameter->payload) != compo_tstr)
		twarn(ErrRuntime_Other, "solve::sample",
		      "Rule parameter has no stable name");
	return tstring_cstr(
		((tstr *)parameter->payload.val.v_tcompo)->data);
}

static void require_dictionary(const tobj *value, const char *message,
			       tdict **result)
{
	if (value->type != tcompo ||
	    tobj_compo_type(value) != compo_tdict)
		twarn(ErrRuntime_ParamsType, "solve::sample", message);
	*result = (tdict *)value->val.v_tcompo;
}

static long require_nonnegative_int(const tobj *value,
				    const char *message)
{
	if (value->type != tint || value->val.v_tint < 0)
		twarn(ErrRuntime_ParamsType, "solve::sample", message);
	return value->val.v_tint;
}

static const char *option_name(const tobj *value, const tobj **argument)
{
	if (value->type != tcompo ||
	    tobj_compo_type(value) != compo_tpair)
		twarn(ErrRuntime_ParamsType, "solve::sample",
		      "optional arguments must be named");
	tpair *pair = (tpair *)value->val.v_tcompo;
	if (pair->first.type != tcompo ||
	    tobj_compo_type(&pair->first) != compo_tstr)
		twarn(ErrRuntime_ParamsType, "solve::sample",
		      "optional argument name must be String");
	*argument = &pair->second;
	return tstring_cstr(((tstr *)pair->first.val.v_tcompo)->data);
}

static void initialize_options(sample_options *options, long count)
{
	memset(options, 0, sizeof(*options));
	tobj_set_nil(&options->owned_generator);
	trandom_source *source = trandom_source_pcg32_xsh_rr();
	options->generator = trandom_generator_new(source, 0);
	tobj source_value;
	tobj_set_nil(&source_value);
	tobj_set_compo(&source_value, (tcompo_v *)source);
	tobj_try_clear(&source_value);
	tobj_set_compo(&options->owned_generator,
		       (tcompo_v *)options->generator);
	options->candidate_limit = count > LONG_MAX / 1000 ?
		LONG_MAX : count * 1000;
	options->time_limit = -1.0;
	options->core_budget_factor = 1.0;
	options->fairness_interval = 4;
	options->generalization = generalization_online_mass_core;
	options->scheduler = scheduler_mass_fair;
}

static const char *require_string(const tobj *value, const char *message)
{
	if (value->type != tcompo || tobj_compo_type(value) != compo_tstr)
		twarn(ErrRuntime_ParamsType, "solve::sample", message);
	return tstring_cstr(((tstr *)value->val.v_tcompo)->data);
}

static void parse_options(sample_options *options, tobj *params,
			  uint_regs count)
{
	enum {
		option_distributions = 1u << 0,
		option_rng = 1u << 1,
		option_candidates = 1u << 2,
		option_time = 1u << 3,
		option_budget = 1u << 4,
		option_fairness = 1u << 5,
		option_generalization = 1u << 6,
		option_scheduler = 1u << 7,
		option_trace = 1u << 8
	};
	unsigned seen = 0;
	for (uint_regs i = 3; i < count; i++) {
		const tobj *argument;
		const char *name = option_name(&params[i], &argument);
		unsigned bit = 0;
		if (!strcmp(name, "distributions")) {
			bit = option_distributions;
			if (argument->type == tcompo &&
			    tobj_compo_type(argument) == compo_tdict)
				options->distributions =
					(tdict *)argument->val.v_tcompo;
			else if (argument->type == tcompo &&
				 argument->val.v_tcompo->vtable ==
					 &tfinite_distribution_vtable)
				options->joint_distribution =
					(tfinite_distribution *)
					argument->val.v_tcompo;
			else
				twarn(ErrRuntime_ParamsType, "solve::sample",
				      "distributions must be Dictionary or "
				      "finite::Distribution");
		} else if (!strcmp(name, "rng")) {
			bit = option_rng;
			if (argument->type != tcompo ||
			    argument->val.v_tcompo->vtable !=
				    &trandom_generator_vtable)
				twarn(ErrRuntime_ParamsType, "solve::sample",
				      "rng must be random::Generator");
			tobj_try_clear(&options->owned_generator);
			options->generator =
				(trandom_generator *)argument->val.v_tcompo;
		} else if (!strcmp(name, "candidate_limit")) {
			bit = option_candidates;
			options->candidate_limit = require_nonnegative_int(
				argument,
				"candidate_limit must be a nonnegative Int");
		} else if (!strcmp(name, "time_limit")) {
			bit = option_time;
			if (argument->type == tnil)
				options->time_limit = -1.0;
			else if (argument->type == tfloat &&
				 isfinite(argument->val.v_tfloat) &&
				 argument->val.v_tfloat > 0.0)
				options->time_limit = argument->val.v_tfloat;
			else
				twarn(ErrRuntime_ParamsType, "solve::sample",
				      "time_limit must be nil or a positive finite Float");
		} else if (!strcmp(name, "core_budget_factor")) {
			bit = option_budget;
			if (argument->type != tfloat ||
			    !isfinite(argument->val.v_tfloat) ||
			    argument->val.v_tfloat <= 0.0)
				twarn(ErrRuntime_ParamsType, "solve::sample",
				      "core_budget_factor must be a positive finite Float");
			options->core_budget_factor = argument->val.v_tfloat;
		} else if (!strcmp(name, "fairness_interval")) {
			bit = option_fairness;
			options->fairness_interval = require_nonnegative_int(
				argument,
				"fairness_interval must be an Int greater than one");
			if (options->fairness_interval < 2)
				twarn(ErrRuntime_ParamsType, "solve::sample",
				      "fairness_interval must be at least 2");
		} else if (!strcmp(name, "generalization")) {
			bit = option_generalization;
			const char *mode = require_string(argument,
				"generalization must be String");
			if (!strcmp(mode, "none"))
				options->generalization = generalization_none;
			else if (!strcmp(mode, "full"))
				options->generalization = generalization_full;
			else if (!strcmp(mode, "raw_core"))
				options->generalization = generalization_raw_core;
			else if (!strcmp(mode, "deletion_mus"))
				options->generalization = generalization_deletion_mus;
			else if (!strcmp(mode, "minimum_core"))
				options->generalization = generalization_minimum_core;
			else if (!strcmp(mode, "online_mass_core"))
				options->generalization = generalization_online_mass_core;
			else
				twarn(ErrRuntime_ParamsType, "solve::sample",
				      "unknown generalization");
		} else if (!strcmp(name, "scheduler")) {
			bit = option_scheduler;
			const char *mode = require_string(argument,
				"scheduler must be String");
			if (!strcmp(mode, "mass_fair"))
				options->scheduler = scheduler_mass_fair;
			else if (!strcmp(mode, "mass"))
				options->scheduler = scheduler_mass;
			else if (!strcmp(mode, "fifo"))
				options->scheduler = scheduler_fifo;
			else if (!strcmp(mode, "binding_count"))
				options->scheduler = scheduler_binding_count;
			else
				twarn(ErrRuntime_ParamsType, "solve::sample",
				      "unknown scheduler");
		} else if (!strcmp(name, "trace")) {
			bit = option_trace;
			if (argument->type != tbool)
				twarn(ErrRuntime_ParamsType, "solve::sample",
				      "trace must be Bool");
			options->trace = argument->val.v_tbool;
		} else {
			twarn(ErrRuntime_ParamsType, "solve::sample",
			      "unknown optional argument");
		}
		if (seen & bit)
			twarn(ErrRuntime_ParamsType, "solve::sample",
			      "duplicate optional argument");
		seen |= bit;
	}
}

static int deadline_reached(double deadline)
{
	return deadline >= 0.0 && monotonic_seconds() >= deadline;
}

static int mass_interrupted(sample_mass_control *control)
{
	if (control->timed_out)
		return 1;
	control->steps++;
	if (control->deadline >= 0.0 &&
	    (control->steps == 1 || (control->steps & 1023u) == 0) &&
	    deadline_reached(control->deadline))
		control->timed_out = 1;
	return control->timed_out;
}

static void sample_hold(tsolve_worker **worker, const tobj *input,
			 tobj *result, double deadline)
{
	long timeout = TSOLVE_QUERY_TIMEOUT_MS;
	if (deadline >= 0.0) {
		double remaining = (deadline - monotonic_seconds()) * 1000.0;
		if (remaining < 1.0)
			timeout = 1;
		else if (remaining < (double)timeout)
			timeout = (long)ceil(remaining);
	}
	tsolve_hold_timed_quiet(worker, input, result, timeout);
}

static void sample_hold_binding_core(tsolve_worker **worker,
				     const tobj *input, tobj *result,
				     double deadline)
{
	long timeout = TSOLVE_QUERY_TIMEOUT_MS;
	if (deadline >= 0.0) {
		double remaining = (deadline - monotonic_seconds()) * 1000.0;
		if (remaining < 1.0)
			timeout = 1;
		else if (remaining < (double)timeout)
			timeout = (long)ceil(remaining);
	}
	tsolve_hold_timed_binding_core(worker, input, result, timeout);
}

static int raw_core_bound(tdict *held,
			  const sample_dimension *dimensions,
			  const trule *rule, size_t dimension_count,
			  uint8_t *bound)
{
	tobj key, names;
	tobj_set_nil(&key);
	tobj_set_nil(&names);
	tobj_set_compo(&key, (tcompo_v *)tstr_new("binding_core"));
	if (!tdict_contains(held, &key)) {
		tobj_try_clear(&key);
		return 0;
	}
	tdict_get(held, &key, &names);
	tobj_try_clear(&key);
	if (names.type != tcompo || tobj_compo_type(&names) != compo_tlist) {
		tobj_ddc_ref_clear(&names);
		return 0;
	}
	tlist *list = (tlist *)names.val.v_tcompo;
	int mapped_all = 1;
	for (uint_objs i = 0; i < list->items.len; i++) {
		const tobj *name = &list->items.data[i];
		if (name->type != tcompo || tobj_compo_type(name) != compo_tstr)
			continue;
		const char *text = tstring_cstr(((tstr *)name->val.v_tcompo)->data);
		int mapped = 0;
		for (size_t j = 0; j < dimension_count; j++) {
			trule_term *parameter = (trule_term *)
				rule->ir->parameters.data[
					dimensions[j].parameter].val.v_tcompo;
			const char *parameter_text = parameter_name(parameter);
			size_t length = strlen(parameter_text);
			if (!strcmp(text, parameter_text) ||
			    (!strncmp(text, parameter_text, length) &&
			     text[length] == ':' && text[length + 1] == ':')) {
				bound[j] = 1;
				mapped = 1;
			}
		}
		mapped_all &= mapped;
	}
	tobj_ddc_ref_clear(&names);
	return mapped_all;
}

static void domain_at(const sample_dimension *dimension, size_t index,
		      tobj *result)
{
	tobj argument;
	tobj_set_nil(&argument);
	tobj_set_int(&argument, (long)index);
	tcompo_index(dimension->domain->val.v_tcompo, &argument, 1, result);
}

static void validate_dimensions(trule *rule, tdict *space,
				tdict *distributions,
				tfinite_distribution *joint_distribution,
				sample_dimension **dimensions_out,
				size_t *count_out)
{
	if (!rule->ir || !rule->ir->parameters.len ||
	    rule->ir->parameters.len > SAMPLE_PARAMETER_LIMIT)
		twarn(ErrRuntime_ParamsType, "solve::sample",
		      "Rule must have between 1 and 255 parameters");
	size_t capacity = rule->ir->parameters.len;
	sample_dimension *dimensions = sample_calloc(
		capacity, sizeof(*dimensions));
	for (size_t i = 0; i < capacity; i++)
		tobj_set_nil(&dimensions[i].owned_distribution);
	size_t count = 0;
	size_t distribution_count = 0;
	for (uint_objs parameter_index = 0;
	     parameter_index < rule->ir->parameters.len; parameter_index++) {
		trule_term *parameter = (trule_term *)
			rule->ir->parameters.data[parameter_index].val.v_tcompo;
		tobj key;
		tobj_set_nil(&key);
		tobj_set_compo(&key, (tcompo_v *)tstr_new(
			parameter_name(parameter)));
		if (!tdict_contains(space, &key)) {
			tobj_try_clear(&key);
			continue;
		}
		tobj domain;
		tobj_set_nil(&domain);
		tdict_get(space, &key, &domain);
		if (domain.type != tcompo || !domain.val.v_tcompo ||
		    !domain.val.v_tcompo->vtable ||
		    !domain.val.v_tcompo->vtable->capabilities ||
		    !domain.val.v_tcompo->vtable->capabilities->indexable)
			twarn(ErrRuntime_ParamsType, "solve::sample",
			      "space values must be Indexable");
		long length = domain.val.v_tcompo->vtable->len(
			domain.val.v_tcompo);
		if (length <= 0)
			twarn(ErrRuntime_ParamsType, "solve::sample",
			      "sample domains must be finite and nonempty");
		sample_dimension *dimension = &dimensions[count++];
		dimension->parameter = parameter_index;
		dimension->domain = thashtbl_get(space->items, &key);
		dimension->size = (size_t)length;
		if (joint_distribution) {
			dimension->distribution = nullptr;
		} else if (distributions && tdict_contains(distributions, &key)) {
			tobj distribution;
			tobj_set_nil(&distribution);
			tdict_get(distributions, &key, &distribution);
			if (distribution.type != tcompo ||
			    distribution.val.v_tcompo->vtable !=
				    &tfinite_distribution_vtable)
				twarn(ErrRuntime_ParamsType, "solve::sample",
				      "distribution values must be finite::Distribution");
			dimension->distribution = (tfinite_distribution *)
				distribution.val.v_tcompo;
			if (tfinite_distribution_rank(
				    dimension->distribution) != 1 ||
			    tfinite_distribution_shape_at(
				    dimension->distribution, 0) != dimension->size)
				twarn(ErrRuntime_ParamsType, "solve::sample",
				      "distribution shape does not match its domain");
			distribution_count++;
			tobj_ddc_ref_clear(&distribution);
		} else {
			dimension->distribution = tfinite_uniform_new(
				dimension->size);
			tobj_set_compo(&dimension->owned_distribution,
				       (tcompo_v *)dimension->distribution);
		}
		tobj_ddc_ref_clear(&domain);
		tobj_try_clear(&key);
	}
	if (!count)
		twarn(ErrRuntime_ParamsType, "solve::sample",
		      "space must contain at least one Rule parameter");
	if ((size_t)thashtbl_len(space->items) != count)
		twarn(ErrRuntime_ParamsType, "solve::sample",
		      "space contains an unknown Rule parameter");
	if (distributions &&
	    (size_t)thashtbl_len(distributions->items) != distribution_count)
		twarn(ErrRuntime_ParamsType, "solve::sample",
		      "distributions contains a variable outside space");
	if (joint_distribution) {
		if (tfinite_distribution_rank(joint_distribution) != count)
			twarn(ErrRuntime_ParamsType, "solve::sample",
			      "joint distribution rank does not match space");
		for (size_t i = 0; i < count; i++)
			if (tfinite_distribution_shape_at(
				    joint_distribution, i) != dimensions[i].size)
				twarn(ErrRuntime_ParamsType, "solve::sample",
				      "joint distribution shape does not match space");
	}
	*dimensions_out = dimensions;
	*count_out = count;
}

static int binding_core_supported(const trule *rule,
				  const sample_dimension *dimensions,
				  size_t dimension_count)
{
	for (size_t i = 0; i < dimension_count; i++) {
		trule_term *parameter = (trule_term *)
			rule->ir->parameters.data[
				dimensions[i].parameter].val.v_tcompo;
		const ttypeval *type = parameter->type;
		if (type->kind == ttype_kind_enum ||
		    ttypeval_equal(type, ttypeval_builtin(tbuiltintype_int)) ||
		    ttypeval_equal(type, ttypeval_builtin(tbuiltintype_bool)) ||
		    ttypeval_equal(type, ttypeval_builtin(tbuiltintype_string)))
			continue;
		return 0;
	}
	return 1;
}

static trule *restricted_rule(trule *rule,
			      const sample_dimension *dimensions,
			      const tobj *values, const uint8_t *bound,
			      size_t count)
{
	trule_ir *ir = trule_ir_new("", "");
	for (uint_objs i = 0; i < rule->ir->parameters.len; i++)
		trule_ir_add_parameter(ir, (trule_term *)
			rule->ir->parameters.data[i].val.v_tcompo);
	tobj original;
	tobj_set_nil(&original);
	tobj_set_compo(&original, (tcompo_v *)rule);
	trule_term *base = trule_term_constant_new(&original);
	trule_item *requirement = trule_requirement_new(
		base, ir->parameters.data, (uint_regs)ir->parameters.len);
	trule_ir_add_item(ir, requirement);
	if (requirement->base.refctr == 0)
		requirement->base.vtable->free(requirement);
	if (base->base.refctr == 0)
		base->base.vtable->free(base);
	for (size_t i = 0; i < count; i++) {
		if (!bound[i])
			continue;
		trule_term *parameter = (trule_term *)
			ir->parameters.data[dimensions[i].parameter].val.v_tcompo;
		trule_term *constant = trule_term_constant_new(&values[i]);
		tobj arguments[] = {
			{.type = tcompo, .val.v_tcompo = (tcompo_v *)parameter},
			{.type = tcompo, .val.v_tcompo = (tcompo_v *)constant}
		};
		tobj operation;
		tobj_set_nil(&operation);
		tobj_set_compo(&operation, (tcompo_v *)tstr_new("=="));
		trule_term *condition = trule_term_new(
			trule_term_intrinsic,
			ttypeval_builtin(tbuiltintype_bool),
			&operation, arguments, 2);
		tobj_try_clear(&operation);
		trule_item *item = trule_condition_new(
			condition, parameter_name(parameter));
		trule_ir_add_item(ir, item);
		if (item->base.refctr == 0)
			item->base.vtable->free(item);
		if (condition->base.refctr == 0)
			condition->base.vtable->free(condition);
	}
	trule *result = trule_new_dynamic(
		ir, tstring_cstr(rule->signature));
	if (ir->base.refctr > 0)
		ir->base.refctr--;
	return result;
}

static int result_text(tdict *result, const char *field_name,
		       tobj *value, const char **text)
{
	tobj key;
	tobj_set_nil(&key);
	tobj_set_compo(&key, (tcompo_v *)tstr_new(field_name));
	tdict_get(result, &key, value);
	tobj_try_clear(&key);
	if (value->type != tcompo ||
	    tobj_compo_type(value) != compo_tstr)
		return 0;
	*text = tstring_cstr(((tstr *)value->val.v_tcompo)->data);
	return 1;
}

static int witness_passes(tcompo_env *environment, const tobj *witness)
{
	tobj checked;
	tobj key;
	tobj passed;
	tobj_set_nil(&checked);
	tobj_set_nil(&key);
	tobj_set_nil(&passed);
	tvm_services_check_rule(environment, witness, &checked);
	tobj_set_compo(&key, (tcompo_v *)tstr_new("passed"));
	tdict_get((tdict *)checked.val.v_tcompo, &key, &passed);
	int result = passed.type == tbool && passed.val.v_tbool;
	tobj_try_clear(&passed);
	tobj_try_clear(&key);
	tobj_try_clear(&checked);
	return result;
}

static tdict *witness_assignment(const trule *rule,
				 const trule_instance *instance)
{
	tdict *assignment = tdict_new();
	for (uint_objs i = 0; i < rule->ir->parameters.len; i++) {
		trule_term *parameter = (trule_term *)
			rule->ir->parameters.data[i].val.v_tcompo;
		sample_field(assignment, parameter_name(parameter),
			&instance->arguments.data[i]);
	}
	return assignment;
}

static sample_task *task_new(const tobj *values, const size_t *coordinates,
			     size_t count)
{
	sample_task *task = sample_calloc(1, sizeof(*task));
	task->values = sample_calloc(count, sizeof(*task->values));
	task->coordinates = sample_calloc(count, sizeof(*task->coordinates));
	task->working = sample_calloc(count, sizeof(*task->working));
	task->combination = sample_calloc(count, sizeof(*task->combination));
	task->count = count;
	for (size_t i = 0; i < count; i++) {
		tobj_set_nil(&task->values[i]);
		tobj_copy(&task->values[i], &values[i]);
		task->coordinates[i] = coordinates[i];
		task->working[i] = 1;
	}
	return task;
}

static void task_free(sample_task *task)
{
	if (!task)
		return;
	for (size_t i = 0; i < task->count; i++)
		tobj_ddc_ref_clear(&task->values[i]);
	free(task->values);
	free(task->coordinates);
	free(task->working);
	free(task->combination);
	free(task);
}

static void tasks_push(sample_tasks *tasks, sample_task *task)
{
	if (tasks->count == tasks->capacity) {
		size_t capacity = tasks->capacity ? tasks->capacity * 2 : 8;
		if (capacity > SIZE_MAX / sizeof(*tasks->items))
			twarn(ErrRuntime_Other, "solve::sample",
			      "too many core tasks");
		sample_task **items = realloc(tasks->items,
			capacity * sizeof(*items));
		if (!items)
			twarn(ErrRuntime_Other, "solve::sample",
			      "out of memory");
		tasks->items = items;
		tasks->capacity = capacity;
	}
	tasks->items[tasks->count++] = task;
}

static int task_exists(const sample_tasks *tasks, const size_t *coordinates,
		       size_t count)
{
	for (size_t i = 0; i < tasks->count; i++) {
		const sample_task *task = tasks->items[i];
		if (task->count != count)
			continue;
		size_t j = 0;
		while (j < count && task->coordinates[j] == coordinates[j])
			j++;
		if (j == count)
			return 1;
	}
	return 0;
}

static sample_region *region_new(const uint8_t *bound,
				 const size_t *coordinates, size_t count)
{
	sample_region *region = sample_calloc(1, sizeof(*region));
	region->bound = sample_calloc(count, sizeof(*region->bound));
	region->coordinates = sample_calloc(count,
		sizeof(*region->coordinates));
	memcpy(region->bound, bound, count * sizeof(*bound));
	memcpy(region->coordinates, coordinates,
		count * sizeof(*coordinates));
	region->count = count;
	return region;
}

static void region_free(sample_region *region)
{
	if (!region)
		return;
	free(region->bound);
	free(region->coordinates);
	free(region);
}

static int region_subsumes(const sample_region *region, const uint8_t *bound,
			   const size_t *coordinates)
{
	for (size_t i = 0; i < region->count; i++)
		if (region->bound[i] &&
		    (!bound[i] || region->coordinates[i] != coordinates[i]))
			return 0;
	return 1;
}

static int region_matches(const sample_region *region,
			  const size_t *coordinates)
{
	for (size_t i = 0; i < region->count; i++)
		if (region->bound[i] &&
		    region->coordinates[i] != coordinates[i])
			return 0;
	return 1;
}

static void cache_push(sample_cache *cache, sample_region *region)
{
	if (cache->count == cache->capacity) {
		size_t capacity = cache->capacity ? cache->capacity * 2 : 8;
		if (capacity > SIZE_MAX / sizeof(*cache->items))
			twarn(ErrRuntime_Other, "solve::sample",
			      "too many cached regions");
		sample_region **items = realloc(cache->items,
			capacity * sizeof(*items));
		if (!items)
			twarn(ErrRuntime_Other, "solve::sample",
			      "out of memory");
		cache->items = items;
		cache->capacity = capacity;
	}
	cache->items[cache->count++] = region;
}

static int cache_insert(sample_cache *cache, const uint8_t *bound,
			const size_t *coordinates, size_t count)
{
	for (size_t i = 0; i < cache->count; i++)
		if (region_subsumes(cache->items[i], bound, coordinates))
			return 0;
	for (size_t i = 0; i < cache->count;) {
		sample_region candidate = {
			.bound = (uint8_t *)bound,
			.coordinates = (size_t *)coordinates,
			.count = count
		};
		if (!region_subsumes(&candidate, cache->items[i]->bound,
				      cache->items[i]->coordinates)) {
			i++;
			continue;
		}
		region_free(cache->items[i]);
		cache->items[i] = cache->items[--cache->count];
	}
	cache_push(cache, region_new(bound, coordinates, count));
	return 1;
}

static int cache_matches(const sample_cache *cache,
			 const size_t *coordinates)
{
	for (size_t i = 0; i < cache->count; i++)
		if (region_matches(cache->items[i], coordinates))
			return 1;
	return 0;
}

static long double coordinate_mass(const sample_dimension *dimension,
				   size_t coordinate)
{
	return tfinite_distribution_prob_at(
		dimension->distribution, coordinate);
}

static int region_compatible(const sample_region *region,
			     const uint8_t *bound,
			     const size_t *coordinates)
{
	for (size_t i = 0; i < region->count; i++)
		if (region->bound[i] && bound[i] &&
		    region->coordinates[i] != coordinates[i])
			return 0;
	return 1;
}

static long double uncovered_partition(
	const sample_cache *cache, const sample_dimension *dimensions,
	const size_t *active, size_t active_count, uint8_t *processed,
	size_t dimension_count, long double mass,
	sample_mass_control *control)
{
	if (mass_interrupted(control))
		return 0.0L;
	/* Split only at coordinates named by compatible cached cubes. Under the
	 * product prior, these branches are disjoint and their masses are exact. */
	for (size_t i = 0; i < active_count; i++) {
		const sample_region *region = cache->items[active[i]];
		int remaining = 0;
		for (size_t j = 0; j < dimension_count; j++)
			if (!processed[j] && region->bound[j]) {
				remaining = 1;
				break;
			}
		if (!remaining)
			return 0.0L;
	}

	size_t dimension = dimension_count;
	for (size_t i = 0; i < active_count && dimension == dimension_count;
	     i++)
		for (size_t j = 0; j < dimension_count; j++)
			if (!processed[j] && cache->items[active[i]]->bound[j]) {
				dimension = j;
				break;
			}
	if (dimension == dimension_count)
		return mass;

	size_t *coordinates = sample_calloc(active_count, sizeof(*coordinates));
	size_t coordinate_count = 0;
	for (size_t i = 0; i < active_count; i++) {
		const sample_region *region = cache->items[active[i]];
		if (!region->bound[dimension])
			continue;
		size_t coordinate = region->coordinates[dimension];
		int seen = 0;
		for (size_t j = 0; j < coordinate_count; j++)
			seen |= coordinates[j] == coordinate;
		if (!seen)
			coordinates[coordinate_count++] = coordinate;
	}

	processed[dimension] = 1;
	long double uncovered = 0.0L;
	long double specified_mass = 0.0L;
	size_t *next = sample_calloc(active_count, sizeof(*next));
	for (size_t i = 0; i < coordinate_count; i++) {
		long double probability = coordinate_mass(
			&dimensions[dimension], coordinates[i]);
		specified_mass += probability;
		size_t next_count = 0;
		for (size_t j = 0; j < active_count; j++) {
			const sample_region *region = cache->items[active[j]];
			if (!region->bound[dimension] ||
			    region->coordinates[dimension] == coordinates[i])
				next[next_count++] = active[j];
		}
		uncovered += uncovered_partition(cache, dimensions, next,
			next_count, processed, dimension_count,
			mass * probability, control);
	}

	long double other_mass = 1.0L - specified_mass;
	if (other_mass > 0.0L) {
		size_t next_count = 0;
		for (size_t i = 0; i < active_count; i++)
			if (!cache->items[active[i]]->bound[dimension])
				next[next_count++] = active[i];
		uncovered += uncovered_partition(cache, dimensions, next,
			next_count, processed, dimension_count,
			mass * other_mass, control);
	}
	free(next);
	processed[dimension] = 0;
	free(coordinates);
	return uncovered;
}

static long double joint_uncovered_partition(
	const sample_cache *cache, const sample_dimension *dimensions,
	const tfinite_distribution *distribution, const uint8_t *bound,
	const size_t *base, size_t *coordinates, size_t dimension,
	size_t dimension_count, sample_mass_control *control)
{
	if (mass_interrupted(control))
		return 0.0L;
	if (dimension == dimension_count) {
		if (cache_matches(cache, coordinates))
			return 0.0L;
		return tfinite_distribution_prob_coordinates(
			distribution, coordinates, dimension_count);
	}
	if (bound[dimension]) {
		coordinates[dimension] = base[dimension];
		return joint_uncovered_partition(cache, dimensions, distribution,
			bound, base, coordinates, dimension + 1,
			dimension_count, control);
	}
	long double mass = 0.0L;
	for (size_t coordinate = 0;
	     coordinate < dimensions[dimension].size; coordinate++) {
		coordinates[dimension] = coordinate;
		mass += joint_uncovered_partition(cache, dimensions,
			distribution, bound, base, coordinates, dimension + 1,
			dimension_count, control);
	}
	return mass;
}

static long double uncovered_mass(const sample_cache *cache,
				   const sample_dimension *dimensions,
				   const uint8_t *bound,
				   const size_t *coordinates,
				   size_t dimension_count,
				   const tfinite_distribution *joint_distribution,
				   sample_mass_control *control)
{
	if (joint_distribution) {
		size_t *point = sample_calloc(
			dimension_count, sizeof(*point));
		long double result = joint_uncovered_partition(cache, dimensions,
			joint_distribution, bound, coordinates, point, 0,
			dimension_count, control);
		free(point);
		return result < 0.0L ? 0.0L : result;
	}
	long double mass = 1.0L;
	uint8_t *processed = sample_calloc(
		dimension_count, sizeof(*processed));
	for (size_t i = 0; i < dimension_count; i++)
		if (bound[i]) {
			processed[i] = 1;
			mass *= coordinate_mass(&dimensions[i], coordinates[i]);
		}
	size_t *active = sample_calloc(cache->count, sizeof(*active));
	size_t active_count = 0;
	for (size_t i = 0; i < cache->count; i++)
		if (region_compatible(cache->items[i], bound, coordinates))
			active[active_count++] = i;
	long double result = uncovered_partition(cache, dimensions, active,
		active_count, processed, dimension_count, mass, control);
	free(active);
	free(processed);
	return result < 0.0L ? 0.0L : result;
}

static sample_node *node_new(sample_nodes *nodes, sample_task *task,
			     const uint8_t *included, const uint8_t *undecided,
			     sample_endpoint minimum, sample_endpoint maximum)
{
	sample_node *node = sample_calloc(1, sizeof(*node));
	node->task = task;
	node->included = sample_calloc(task->count, sizeof(*node->included));
	node->undecided = sample_calloc(task->count, sizeof(*node->undecided));
	memcpy(node->included, included,
		task->count * sizeof(*node->included));
	memcpy(node->undecided, undecided,
		task->count * sizeof(*node->undecided));
	node->minimum = minimum;
	node->maximum = maximum;
	node->created = nodes->next_created++;
	if (nodes->count == nodes->capacity) {
		size_t capacity = nodes->capacity ? nodes->capacity * 2 : 16;
		if (capacity > SIZE_MAX / sizeof(*nodes->items))
			twarn(ErrRuntime_Other, "solve::sample",
			      "too many core search nodes");
		sample_node **items = realloc(nodes->items,
			capacity * sizeof(*items));
		if (!items)
			twarn(ErrRuntime_Other, "solve::sample",
			      "out of memory");
		nodes->items = items;
		nodes->capacity = capacity;
	}
	nodes->items[nodes->count++] = node;
	return node;
}

static void node_free(sample_node *node)
{
	if (!node)
		return;
	free(node->included);
	free(node->undecided);
	free(node);
}

static uint8_t *node_maximum_bound(const sample_node *node)
{
	uint8_t *bound = sample_calloc(node->task->count, sizeof(*bound));
	for (size_t i = 0; i < node->task->count; i++)
		bound[i] = node->included[i] || node->undecided[i];
	return bound;
}

static size_t node_first_undecided(const sample_node *node)
{
	for (size_t i = 0; i < node->task->count; i++)
		if (node->undecided[i])
			return i;
	return node->task->count;
}

static size_t bound_count(const uint8_t *bound, size_t count)
{
	size_t result = 0;
	for (size_t i = 0; i < count; i++)
		result += !!bound[i];
	return result;
}

static void node_branch(sample_nodes *nodes, sample_node *node)
{
	size_t selected = node_first_undecided(node);
	if (selected == node->task->count) {
		node->closed = 1;
		return;
	}
	uint8_t *child_undecided = sample_calloc(
		node->task->count, sizeof(*child_undecided));
	memcpy(child_undecided, node->undecided,
		node->task->count * sizeof(*child_undecided));
	child_undecided[selected] = 0;
	uint8_t *kept = sample_calloc(node->task->count, sizeof(*kept));
	memcpy(kept, node->included,
		node->task->count * sizeof(*kept));
	kept[selected] = 1;
	node_new(nodes, node->task, kept, child_undecided,
		 endpoint_unknown, node->maximum);
	node_new(nodes, node->task, node->included, child_undecided,
		 node->minimum, endpoint_unknown);
	free(kept);
	free(child_undecided);
	node->closed = 1;
}

static void prepare_node(sample_nodes *nodes, sample_cache *cache,
			 const sample_dimension *dimensions,
			 const tfinite_distribution *joint_distribution,
			 sample_mass_control *control, sample_node *node)
{
	if (node->closed)
		return;
	long double priority = uncovered_mass(cache, dimensions,
		node->included, node->task->coordinates, node->task->count,
		joint_distribution, control);
	if (control->timed_out)
		return;
	if (priority <= 0.0L) {
		node->closed = 1;
		return;
	}
	size_t undecided = node_first_undecided(node);
	if (undecided == node->task->count) {
		if (node->minimum == endpoint_unknown)
			node->minimum = node->maximum;
		if (node->maximum == endpoint_unknown)
			node->maximum = node->minimum;
	}
	if (node->maximum == endpoint_sat) {
		node->closed = 1;
		return;
	}
	if (node->minimum == endpoint_unsat) {
		cache_insert(cache, node->included, node->task->coordinates,
			node->task->count);
		node->closed = 1;
		return;
	}
	if (node->maximum == endpoint_unknown ||
	    node->minimum == endpoint_unknown)
		return;
	node_branch(nodes, node);
}

static sample_node *scheduled_node(sample_nodes *nodes, sample_cache *cache,
				   const sample_dimension *dimensions,
				   const tfinite_distribution *joint_distribution,
				   long core_calls, long fairness,
				   sample_scheduler scheduler,
				   double deadline, int *timed_out)
{
	sample_mass_control control = {
		.deadline = deadline,
		.steps = 0,
		.timed_out = 0
	};
	for (size_t i = 0; i < nodes->count; i++)
		prepare_node(nodes, cache, dimensions, joint_distribution, &control,
			nodes->items[i]);
	if (control.timed_out) {
		*timed_out = 1;
		return nullptr;
	}
	sample_node *selected = nullptr;
	long double best = -1.0L;
	int fair = scheduler == scheduler_mass_fair &&
		(core_calls + 1) % fairness == 0;
	for (size_t i = 0; i < nodes->count; i++) {
		sample_node *candidate = nodes->items[i];
		prepare_node(nodes, cache, dimensions, joint_distribution, &control,
			candidate);
		if (control.timed_out) {
			*timed_out = 1;
			return nullptr;
		}
		if (candidate->closed)
			continue;
		long double priority = uncovered_mass(cache, dimensions,
			candidate->included, candidate->task->coordinates,
			candidate->task->count, joint_distribution, &control);
		if (control.timed_out) {
			*timed_out = 1;
			return nullptr;
		}
		if (priority <= 0.0L) {
			candidate->closed = 1;
			continue;
		}
		size_t candidate_bindings = bound_count(candidate->included,
			candidate->task->count);
		size_t selected_bindings = selected ? bound_count(selected->included,
			selected->task->count) : 0;
		int prefer_oldest = fair || scheduler == scheduler_fifo;
		int prefer_count = scheduler == scheduler_binding_count;
		if (!selected || (prefer_oldest ?
			candidate->created < selected->created : prefer_count ?
			candidate_bindings < selected_bindings ||
			(candidate_bindings == selected_bindings &&
			 candidate->created < selected->created) :
			priority > best || (priority == best &&
			candidate->created < selected->created))) {
			selected = candidate;
			best = priority;
		}
	}
	return selected;
}

static int refinement_step(tsolve_worker **worker, trule *rule,
			   const sample_dimension *dimensions,
			   sample_cache *cache, sample_node *node, double deadline,
			   char **failure_status, char **failure_reason)
{
	int query_minimum = node->maximum == endpoint_unsat;
	uint8_t *bound = query_minimum ? node->included :
		node_maximum_bound(node);
	trule *restricted = restricted_rule(rule, dimensions, node->task->values,
		bound, node->task->count);
	tobj input;
	tobj held;
	tobj status;
	tobj reason;
	tobj_set_nil(&input);
	tobj_set_nil(&held);
	tobj_set_nil(&status);
	tobj_set_nil(&reason);
	tobj_set_compo(&input, (tcompo_v *)restricted);
	sample_hold(worker, &input, &held, deadline);
	const char *status_text = nullptr;
	const char *reason_text = nullptr;
	if (!result_text((tdict *)held.val.v_tcompo, "status",
			 &status, &status_text) ||
	    !result_text((tdict *)held.val.v_tcompo, "reason",
			 &reason, &reason_text))
		twarn(ErrRuntime_Other, "solve::sample",
		      "invalid core refinement result");
	int successful = !strcmp(status_text, "sat") ||
		!strcmp(status_text, "unsat");
	if (successful) {
		sample_endpoint outcome = !strcmp(status_text, "unsat") ?
			endpoint_unsat : endpoint_sat;
		if (query_minimum)
			node->minimum = outcome;
		else
			node->maximum = outcome;
		if (outcome == endpoint_unsat)
			cache_insert(cache, bound, node->task->coordinates,
				node->task->count);
		if ((!query_minimum && outcome == endpoint_sat) ||
		    (query_minimum && outcome == endpoint_unsat))
			node->closed = 1;
	}
	if (!successful) {
		*failure_status = sample_strdup(status_text);
		*failure_reason = sample_strdup(reason_text);
	}
	/* The hold result may retain the dynamic rule through its witness or
	 * conflicts. Clear the uncounted input handle before those references. */
	tobj_try_clear(&input);
	tobj_ddc_ref_clear(&reason);
	tobj_ddc_ref_clear(&status);
	tobj_try_clear(&held);
	if (!query_minimum)
		free(bound);
	return successful ? 1 : -1;
}

static int query_bound(tsolve_worker **worker, trule *rule,
			 const sample_dimension *dimensions, sample_task *task,
			 const uint8_t *bound, double deadline,
			 sample_endpoint *outcome, char **failure_status,
			 char **failure_reason)
{
	trule *restricted = restricted_rule(rule, dimensions, task->values,
		bound, task->count);
	tobj input, held, status, reason;
	tobj_set_nil(&input);
	tobj_set_nil(&held);
	tobj_set_nil(&status);
	tobj_set_nil(&reason);
	tobj_set_compo(&input, (tcompo_v *)restricted);
	sample_hold(worker, &input, &held, deadline);
	const char *status_text = nullptr;
	const char *reason_text = nullptr;
	if (!result_text((tdict *)held.val.v_tcompo, "status",
			 &status, &status_text) ||
	    !result_text((tdict *)held.val.v_tcompo, "reason",
			 &reason, &reason_text))
		twarn(ErrRuntime_Other, "solve::sample",
		      "invalid core generalization result");
	int successful = !strcmp(status_text, "sat") ||
		!strcmp(status_text, "unsat");
	if (successful)
		*outcome = !strcmp(status_text, "unsat") ?
			endpoint_unsat : endpoint_sat;
	else {
		*failure_status = sample_strdup(status_text);
		*failure_reason = sample_strdup(reason_text);
	}
	tobj_try_clear(&input);
	tobj_ddc_ref_clear(&reason);
	tobj_ddc_ref_clear(&status);
	tobj_try_clear(&held);
	return successful ? 1 : -1;
}

static void minimum_prepare(sample_task *task)
{
	if (task->combination_ready || task->minimum_size > task->count)
		return;
	for (size_t i = 0; i < task->minimum_size; i++)
		task->combination[i] = i;
	task->combination_ready = 1;
}

static void minimum_advance(sample_task *task)
{
	size_t k = task->minimum_size;
	if (!k) {
		task->minimum_size = 1;
		task->combination_ready = 0;
		return;
	}
	for (size_t position = k; position > 0; position--) {
		size_t i = position - 1;
		if (task->combination[i] < task->count - k + i) {
			task->combination[i]++;
			for (size_t j = i + 1; j < k; j++)
				task->combination[j] = task->combination[j - 1] + 1;
			return;
		}
	}
	task->minimum_size++;
	task->combination_ready = 0;
}

static uint8_t *task_next_bound(sample_task *task,
				sample_generalization generalization)
{
	uint8_t *bound = sample_calloc(task->count, sizeof(*bound));
	if (generalization == generalization_deletion_mus) {
		while (task->cursor < task->count && !task->working[task->cursor])
			task->cursor++;
		if (task->cursor == task->count) {
			task->closed = 1;
			free(bound);
			return nullptr;
		}
		memcpy(bound, task->working, task->count * sizeof(*bound));
		bound[task->cursor] = 0;
		return bound;
	}
	minimum_prepare(task);
	if (task->minimum_size >= task->count) {
		task->closed = 1;
		free(bound);
		return nullptr;
	}
	for (size_t i = 0; i < task->minimum_size; i++)
		bound[task->combination[i]] = 1;
	return bound;
}

static sample_task *scheduled_task(sample_tasks *tasks, sample_cache *cache,
					 const sample_dimension *dimensions,
					 const tfinite_distribution *joint_distribution,
					 sample_generalization generalization,
					 sample_scheduler scheduler,
					 long core_calls, long fairness,
					 double deadline, uint8_t **selected_bound,
					 int *timed_out)
{
	sample_mass_control control = {.deadline = deadline};
	sample_task *selected = nullptr;
	long double best = -1.0L;
	size_t best_bindings = 0;
	int oldest = (scheduler == scheduler_fifo) ||
		(scheduler == scheduler_mass_fair &&
		 (core_calls + 1) % fairness == 0);
	for (size_t i = 0; i < tasks->count; i++) {
		sample_task *task = tasks->items[i];
		if (task->closed)
			continue;
		uint8_t *bound = task_next_bound(task, generalization);
		if (!bound) {
			cache_insert(cache, task->working, task->coordinates,
				task->count);
			continue;
		}
		size_t bindings = bound_count(bound, task->count);
		long double mass = uncovered_mass(cache, dimensions, bound,
			task->coordinates, task->count, joint_distribution, &control);
		if (control.timed_out) {
			free(bound);
			free(*selected_bound);
			*selected_bound = nullptr;
			*timed_out = 1;
			return nullptr;
		}
		int choose = !selected || (oldest ? 0 :
			scheduler == scheduler_binding_count ?
			bindings < best_bindings : mass > best);
		if (choose) {
			free(*selected_bound);
			*selected_bound = bound;
			selected = task;
			best = mass;
			best_bindings = bindings;
		} else
			free(bound);
		if (oldest && selected)
			break;
	}
	return selected;
}

static int baseline_step(tsolve_worker **worker, trule *rule,
			 const sample_dimension *dimensions, sample_cache *cache,
			 sample_task *task, uint8_t *bound,
			 sample_generalization generalization, double deadline,
			 char **failure_status, char **failure_reason)
{
	sample_endpoint outcome = endpoint_unknown;
	int queried = query_bound(worker, rule, dimensions, task, bound,
		deadline, &outcome, failure_status, failure_reason);
	if (queried < 0)
		return queried;
	if (generalization == generalization_deletion_mus) {
		if (outcome == endpoint_unsat) {
			task->working[task->cursor] = 0;
			cache_insert(cache, task->working, task->coordinates,
				task->count);
		}
		task->cursor++;
	} else if (outcome == endpoint_unsat) {
		memcpy(task->working, bound, task->count * sizeof(*bound));
		cache_insert(cache, bound, task->coordinates, task->count);
		task->closed = 1;
	} else
		minimum_advance(task);
	return 1;
}

static size_t active_node_count(const sample_nodes *nodes)
{
	size_t count = 0;
	for (size_t i = 0; i < nodes->count; i++)
		count += !nodes->items[i]->closed;
	return count;
}

static size_t active_task_count(const sample_tasks *tasks)
{
	size_t count = 0;
	for (size_t i = 0; i < tasks->count; i++)
		count += !tasks->items[i]->closed;
	return count;
}

static void trace_append(tlist *trace, const char *outcome,
			 long candidates, long solver_calls, long core_calls,
			 long cache_hits, const sample_cache *cache,
			 const sample_nodes *nodes, const sample_tasks *tasks,
			 const sample_dimension *dimensions,
			 size_t dimension_count,
			 const tfinite_distribution *joint_distribution,
			 double deadline)
{
	tdict *entry = tdict_new();
	tobj value;
	tobj_set_nil(&value);
	sample_string_field(entry, "outcome", outcome);
	tobj_set_int(&value, candidates);
	sample_field(entry, "candidate", &value);
	tobj_set_int(&value, solver_calls);
	sample_field(entry, "solver_calls", &value);
	tobj_set_int(&value, core_calls);
	sample_field(entry, "core_calls", &value);
	tobj_set_int(&value, cache_hits);
	sample_field(entry, "cache_hits", &value);
	tobj_set_int(&value, (long)cache->count);
	sample_field(entry, "cache_regions", &value);
	tobj_set_int(&value, (long)(active_node_count(nodes) +
		active_task_count(tasks)));
	sample_field(entry, "active_searches", &value);
	uint8_t *empty = sample_calloc(dimension_count, sizeof(*empty));
	size_t *coordinates = sample_calloc(dimension_count,
		sizeof(*coordinates));
	sample_mass_control control = {.deadline = deadline};
	long double uncovered = uncovered_mass(cache, dimensions, empty,
		coordinates, dimension_count, joint_distribution, &control);
	free(coordinates);
	free(empty);
	tobj_set_float(&value, control.timed_out ? -1.0 :
		(double)(1.0L - uncovered));
	sample_field(entry, "cache_mass", &value);
	tobj entry_value;
	tobj_set_nil(&entry_value);
	tobj_set_compo(&entry_value, (tcompo_v *)entry);
	tobj_vec_push(&trace->items, &entry_value);
	tobj_try_clear(&entry_value);
}

static void sample_result(tobj *result, const char *status,
			  const char *reason, tlist *samples,
			  long candidates, long solver_calls,
			  long core_calls, long cache_hits,
			  long cache_regions, tlist *trace, double elapsed)
{
	tdict *dictionary = tdict_new();
	sample_string_field(dictionary, "status", status);
	sample_string_field(dictionary, "reason", reason ? reason : "");
	tobj value;
	tobj_set_nil(&value);
	tobj_set_compo(&value, (tcompo_v *)samples);
	sample_field(dictionary, "samples", &value);
	tobj_try_clear(&value);
	tobj_set_int(&value, candidates);
	sample_field(dictionary, "candidates", &value);
	tobj_set_int(&value, solver_calls);
	sample_field(dictionary, "solver_calls", &value);
	tobj_set_int(&value, core_calls);
	sample_field(dictionary, "core_calls", &value);
	tobj_set_int(&value, cache_hits);
	sample_field(dictionary, "cache_hits", &value);
	tobj_set_int(&value, cache_regions);
	sample_field(dictionary, "cache_regions", &value);
	tobj_set_compo(&value, (tcompo_v *)trace);
	sample_field(dictionary, "trace", &value);
	tobj_try_clear(&value);
	tobj_set_float(&value, elapsed);
	sample_field(dictionary, "elapsed", &value);
	tobj_set_compo(result, (tcompo_v *)dictionary);
}

void tsolve_sample(tsolve_worker **worker, tobj *params, uint_regs count,
		   tobj *result, tcompo_env *environment)
{
	if (count < 3 || count > 12)
		twarn(ErrRuntime_ParamsCtr, "solve::sample",
		      "three positional arguments and up to nine named arguments required");
	if (params[0].type != tcompo ||
	    tobj_compo_type(&params[0]) != compo_trule)
		twarn(ErrRuntime_ParamsType, "solve::sample", "Rule required");
	long requested = require_nonnegative_int(&params[1],
		"count must be a nonnegative Int");
	tdict *space;
	require_dictionary(&params[2], "space must be Dictionary", &space);
	sample_options options;
	initialize_options(&options, requested);
	parse_options(&options, params, count);
	if (requested && !options.candidate_limit)
		twarn(ErrRuntime_ParamsType, "solve::sample",
		      "candidate_limit must be positive when count is positive");

	trule *rule = (trule *)params[0].val.v_tcompo;
	sample_dimension *dimensions;
	size_t dimension_count;
	validate_dimensions(rule, space, options.distributions,
		options.joint_distribution,
		&dimensions, &dimension_count);
	int use_binding_core = binding_core_supported(rule, dimensions,
		dimension_count) &&
		(options.generalization == generalization_raw_core ||
		 options.generalization == generalization_online_mass_core);
	tobj *values = sample_calloc(dimension_count, sizeof(*values));
	size_t *coordinates = sample_calloc(
		dimension_count, sizeof(*coordinates));
	uint8_t *all_bound = sample_calloc(
		dimension_count, sizeof(*all_bound));
	for (size_t i = 0; i < dimension_count; i++) {
		tobj_set_nil(&values[i]);
		all_bound[i] = 1;
	}
	tlist *samples = tlist_new();
	tlist *trace = tlist_new();
	sample_cache cache = {0};
	sample_tasks tasks = {0};
	sample_nodes nodes = {0};
	long candidates = 0;
	long solver_calls = 0;
	long core_calls = 0;
	long cache_hits = 0;
	const char *final_status = "complete";
	char *final_reason = nullptr;
	double started = monotonic_seconds();
	double deadline = options.time_limit < 0.0 ? -1.0 :
		started + options.time_limit;

	while ((long)tlist_size(samples) < requested) {
		const char *candidate_outcome = "cache_hit";
		if (candidates >= options.candidate_limit) {
			final_status = "incomplete";
			final_reason = sample_strdup("candidate_limit reached");
			break;
		}
		if (deadline_reached(deadline)) {
			final_status = "incomplete";
			final_reason = sample_strdup("time_limit reached");
			break;
		}
		if (options.joint_distribution) {
			tfinite_index *index = tfinite_distribution_sample(
				options.joint_distribution, options.generator);
			for (size_t i = 0; i < dimension_count; i++)
				coordinates[i] = tfinite_index_at(index, i);
			tobj index_value;
			tobj_set_nil(&index_value);
			tobj_set_compo(&index_value, (tcompo_v *)index);
			tobj_try_clear(&index_value);
		} else {
			for (size_t i = 0; i < dimension_count; i++) {
				tfinite_index *index = tfinite_distribution_sample(
					dimensions[i].distribution,
					options.generator);
				coordinates[i] = tfinite_index_at(index, 0);
				tobj index_value;
				tobj_set_nil(&index_value);
				tobj_set_compo(&index_value,
					       (tcompo_v *)index);
				tobj_try_clear(&index_value);
			}
		}
		for (size_t i = 0; i < dimension_count; i++) {
			tobj_try_clear(&values[i]);
			domain_at(&dimensions[i], coordinates[i], &values[i]);
			trule_term *parameter = (trule_term *)
				rule->ir->parameters.data[
					dimensions[i].parameter].val.v_tcompo;
			if (!ttypeval_matches(&values[i], parameter->type))
				twarn(ErrRuntime_ParamsType, "solve::sample",
				      "domain value does not match Rule parameter Type");
		}
		candidates++;
		if (!cache_matches(&cache, coordinates)) {
			trule *restricted = restricted_rule(rule, dimensions,
				values, all_bound, dimension_count);
			tobj input;
			tobj held;
			tobj status;
			tobj reason;
			tobj_set_nil(&input);
			tobj_set_nil(&held);
			tobj_set_nil(&status);
			tobj_set_nil(&reason);
			tobj_set_compo(&input, (tcompo_v *)restricted);
			if (use_binding_core)
				sample_hold_binding_core(worker, &input, &held,
					deadline);
			else
				sample_hold(worker, &input, &held, deadline);
			solver_calls++;
			const char *status_text = nullptr;
			const char *reason_text = nullptr;
			if (!result_text((tdict *)held.val.v_tcompo, "status",
					 &status, &status_text) ||
			    !result_text((tdict *)held.val.v_tcompo, "reason",
					 &reason, &reason_text))
				twarn(ErrRuntime_Other, "solve::sample",
				      "invalid hold result");
			if (!strcmp(status_text, "sat")) {
				candidate_outcome = "sat";
				tobj key;
				tobj witness;
				tobj_set_nil(&key);
				tobj_set_nil(&witness);
				tobj_set_compo(&key,
					(tcompo_v *)tstr_new("witness"));
				tdict_get((tdict *)held.val.v_tcompo,
					&key, &witness);
				tobj_try_clear(&key);
				if (witness.type != tcompo ||
				    tobj_compo_type(&witness) !=
					    compo_trule_instance)
					twarn(ErrRuntime_Other, "solve::sample",
					      "solver returned no witness");
				trule_instance *restricted_instance =
					(trule_instance *)witness.val.v_tcompo;
				trule_instance *original_instance = trule_bind(
					rule, restricted_instance->arguments.data,
					(uint_regs)restricted_instance->arguments.len);
				tobj original;
				tobj_set_nil(&original);
				tobj_set_compo(&original,
					       (tcompo_v *)original_instance);
				if (!witness_passes(environment, &original))
					twarn(ErrRuntime_Other, "solve::sample",
					      "solver witness failed the Rule checker");
				tobj assignment;
				tobj_set_nil(&assignment);
				tobj_set_compo(&assignment, (tcompo_v *)
					witness_assignment(rule, original_instance));
				tobj_vec_push(&samples->items, &assignment);
				tobj_try_clear(&assignment);
				tobj_try_clear(&original);
				tobj_ddc_ref_clear(&witness);
			} else if (!strcmp(status_text, "unsat")) {
				uint8_t *raw_bound = nullptr;
				int has_raw_core = 0;
				if (use_binding_core) {
					raw_bound = sample_calloc(dimension_count,
						sizeof(*raw_bound));
					has_raw_core = raw_core_bound(
						(tdict *)held.val.v_tcompo, dimensions,
						rule, dimension_count, raw_bound);
				}
				candidate_outcome = options.generalization ==
					generalization_raw_core ? has_raw_core ?
					"unsat_raw_core" : "unsat_raw_fallback" : "unsat";
				if (options.generalization != generalization_none &&
				    !task_exists(&tasks, coordinates, dimension_count)) {
					sample_task *task = task_new(values,
						coordinates, dimension_count);
					cache_insert(&cache, has_raw_core ? raw_bound : all_bound,
						task->coordinates, dimension_count);
					if (options.generalization ==
					    generalization_online_mass_core) {
						tasks_push(&tasks, task);
						uint8_t *included = sample_calloc(
							dimension_count, sizeof(*included));
						node_new(&nodes, task, included, all_bound,
							 endpoint_unknown, endpoint_unsat);
						free(included);
					} else if (options.generalization ==
						   generalization_deletion_mus ||
						   options.generalization ==
						   generalization_minimum_core)
						tasks_push(&tasks, task);
					else
						task_free(task);
				}
				free(raw_bound);
			} else {
				final_status = deadline_reached(deadline) ?
					"incomplete" :
					!strcmp(status_text, "unknown") ?
					"unknown" :
					!strcmp(status_text, "unsupported") ?
					"unsupported" : "error";
				final_reason = sample_strdup(
					deadline_reached(deadline) ?
					"time_limit reached" : reason_text);
			}
			/* The hold result may retain the dynamic rule through its witness or
			 * conflicts. Clear the uncounted input handle before those references. */
			tobj_try_clear(&input);
			tobj_ddc_ref_clear(&reason);
			tobj_ddc_ref_clear(&status);
			tobj_try_clear(&held);
			if (final_reason)
				break;
		} else
			cache_hits++;

		if ((long)tlist_size(samples) >= requested) {
			if (options.trace)
				trace_append(trace, candidate_outcome, candidates,
					solver_calls, core_calls, cache_hits, &cache,
					&nodes, &tasks, dimensions, dimension_count,
					options.joint_distribution, deadline);
			break;
		}
		if (candidates >= options.candidate_limit) {
			if (options.trace)
				trace_append(trace, candidate_outcome, candidates,
					solver_calls, core_calls, cache_hits, &cache,
					&nodes, &tasks, dimensions, dimension_count,
					options.joint_distribution, deadline);
			final_status = "incomplete";
			final_reason = sample_strdup("candidate_limit reached");
			break;
		}
		if (deadline_reached(deadline)) {
			final_status = "incomplete";
			final_reason = sample_strdup("time_limit reached");
			break;
		}

		long double budget_value = options.core_budget_factor *
			sqrtl((long double)candidates);
		long budget = budget_value >= (long double)LONG_MAX ? LONG_MAX :
			(long)floorl(budget_value);
		while (options.generalization != generalization_none &&
		       options.generalization != generalization_full &&
		       options.generalization != generalization_raw_core &&
		       core_calls < budget && !deadline_reached(deadline)) {
			int mass_timed_out = 0;
			sample_node *node = nullptr;
			sample_task *task = nullptr;
			uint8_t *task_bound = nullptr;
			if (options.generalization ==
			    generalization_online_mass_core)
				node = scheduled_node(&nodes, &cache, dimensions,
					options.joint_distribution, core_calls,
					options.fairness_interval, options.scheduler,
					deadline, &mass_timed_out);
			else
				task = scheduled_task(&tasks, &cache, dimensions,
					options.joint_distribution,
					options.generalization, options.scheduler,
					core_calls, options.fairness_interval,
					deadline, &task_bound, &mass_timed_out);
			if (mass_timed_out) {
				final_status = "incomplete";
				final_reason = sample_strdup("time_limit reached");
				break;
			}
			if (deadline_reached(deadline)) {
				final_status = "incomplete";
				final_reason = sample_strdup("time_limit reached");
				break;
			}
			if (!node && !task) {
				free(task_bound);
				break;
			}
			char *failure_status = nullptr;
			char *failure_reason = nullptr;
			int refined = node ? refinement_step(worker, rule, dimensions,
				&cache, node, deadline, &failure_status,
				&failure_reason) : baseline_step(worker, rule, dimensions,
				&cache, task, task_bound, options.generalization,
				deadline, &failure_status, &failure_reason);
			free(task_bound);
			if (refined)
				core_calls++;
			if (refined < 0) {
				final_status = deadline_reached(deadline) ?
					"incomplete" :
					!strcmp(failure_status, "unknown") ?
					"unknown" :
					!strcmp(failure_status, "unsupported") ?
					"unsupported" : "error";
				final_reason = sample_strdup(
					deadline_reached(deadline) ?
					"time_limit reached" : failure_reason);
				free(failure_status);
				free(failure_reason);
				break;
			}
		}
		if (final_reason)
			break;
		if (options.trace)
			trace_append(trace, candidate_outcome, candidates,
				solver_calls, core_calls, cache_hits, &cache, &nodes,
				&tasks, dimensions, dimension_count,
				options.joint_distribution, deadline);
	}

	double elapsed = monotonic_seconds() - started;
	sample_result(result, final_status, final_reason, samples,
		candidates, solver_calls, core_calls, cache_hits,
		(long)cache.count, trace, elapsed);
	free(final_reason);
	for (size_t i = 0; i < nodes.count; i++)
		node_free(nodes.items[i]);
	free(nodes.items);
	for (size_t i = 0; i < cache.count; i++)
		region_free(cache.items[i]);
	free(cache.items);
	for (size_t i = 0; i < tasks.count; i++)
		task_free(tasks.items[i]);
	free(tasks.items);
	for (size_t i = 0; i < dimension_count; i++) {
		tobj_ddc_ref_clear(&values[i]);
		tobj_try_clear(&dimensions[i].owned_distribution);
	}
	free(all_bound);
	free(coordinates);
	free(values);
	free(dimensions);
	tobj_try_clear(&options.owned_generator);
}

ttypeval *tsolve_sample_result_type(void)
{
	const char *names[] = {
		"status", "reason", "samples", "candidates",
		"solver_calls", "core_calls", "cache_hits", "cache_regions",
		"trace", "elapsed"
	};
	tstring *field_names[10];
	ttype_field fields[10];
	ttypeval *assignments = ttypeval_new_list(
		ttypeval_builtin(tbuiltintype_dictionary));
	ttypeval *traces = ttypeval_new_list(
		ttypeval_builtin(tbuiltintype_dictionary));
	for (size_t i = 0; i < 10; i++) {
		field_names[i] = tstring_new(names[i]);
		fields[i].name = field_names[i];
		fields[i].type = i < 2 ?
			ttypeval_builtin(tbuiltintype_string) :
			i == 2 ? assignments :
			i == 8 ? traces :
			i == 9 ? ttypeval_builtin(tbuiltintype_float) :
			ttypeval_builtin(tbuiltintype_int);
	}
	ttypeval *type = ttypeval_new_fields(fields, 10);
	for (size_t i = 0; i < 10; i++)
		tstring_free(field_names[i]);
	return type;
}
