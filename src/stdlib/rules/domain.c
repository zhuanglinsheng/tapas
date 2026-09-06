/**
 * @file domain.c
 * @brief Implements the Domain objects owned by the `rules` package.
 * @details Defines PointsOf and RangeOf construction, membership, ownership,
 * precise runtime Type reflection, Type matching, formatting, and vtables.
 * @note Core code sees these values only as extension objects through generic
 * capabilities; their representation remains private to the package.
 */
#include "domain.h"
#include "tapas/dsa/tstring.h"
#include "tapas/textension.h"

#include "tapas/objects/tlist.h"
#include "tapas/objects/tstr.h"
#include "tapas/tformat.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static tcompo_vtable points_vtable;
static tcompo_vtable range_vtable;

static const textension_template_type_parameter points_parameters[] = {
	{ .name = "T", .slot = "item" }
};

const textension_nominal_template trules_points_template = {
	.identity = "rules::PointsOf",
	.type_parameters = points_parameters,
	.type_parameter_count = 1,
	.capabilities = textension_type_indexable | textension_type_contains
};

static const textension_template_type_parameter range_parameters[] = {
	{ .name = "T", .slot = "item" }
};

const textension_nominal_template trules_range_template = {
	.identity = "rules::RangeOf",
	.type_parameters = range_parameters,
	.type_parameter_count = 1,
	.capabilities = textension_type_indexable | textension_type_contains
};

int trules_domain_is_points(const tobj *value)
{
	return value && value->type == tcompo && value->val.v_tcompo &&
		value->val.v_tcompo->vtable == &points_vtable;
}

int trules_domain_is_range(const tobj *value)
{
	return value && value->type == tcompo && value->val.v_tcompo &&
		value->val.v_tcompo->vtable == &range_vtable;
}

static int contains(void *self, const tobj *value)
{
	trules_domain *domain = self;
	if (!ttypeval_matches(value, domain->item_type)) return 0;
	if (domain->range)
		return value->type == tint && value->val.v_tint >= domain->start &&
			value->val.v_tint <= domain->end;
	for (uint_objs i = 0; i < domain->values.len; i++)
		if (tobj_identical(&domain->values.data[i], value)) return 1;
	return 0;
}

trules_domain *trules_points_new(ttypeval *type, const tobj *values,
	uint_regs count)
{
	for (uint_regs i = 0; i < count; i++)
		if (!ttypeval_matches(&values[i], type))
			twarn(ErrRuntime_ParamsType, "rules::points",
				"point does not match declared element Type");
	trules_domain *domain = calloc(1, sizeof(*domain));
	if (!domain) abort();
	domain->base.vtable = &points_vtable;
	domain->item_type = ttypeval_retain(type);
	tobj_vec_init(&domain->values);
	for (uint_regs i = 0; i < count; i++)
		if (!contains(domain, &values[i]))
			tobj_vec_push(&domain->values, &values[i]);
	return domain;
}

trules_domain *trules_range_new(long start, long end)
{
	if (start > end)
		twarn(ErrRuntime_ParamsType, "rules::range",
			"start must not exceed end");
	trules_domain *domain = calloc(1, sizeof(*domain));
	if (!domain) abort();
	domain->base.vtable = &range_vtable;
	domain->item_type = ttypeval_retain(ttypeval_builtin(tbuiltintype_int));
	domain->range = 1;
	domain->start = start;
	domain->end = end;
	tobj_vec_init(&domain->values);
	return domain;
}

static const char *points_name(void) { return "rules::PointsOf"; }
static const char *range_name(void) { return "rules::RangeOf"; }
static tcompo_type domain_code(void) { return compo_extension; }

static long domain_len(void *self)
{
	trules_domain *domain = self;
	if (domain->range) {
		uint64_t distance = (uint64_t)domain->end -
			(uint64_t)domain->start;
		if (distance >= LONG_MAX)
			twarn(ErrRuntime_Other, "len",
			      "Range cardinality exceeds Int");
		return (long)(distance + 1);
	}
	return (long)domain->values.len;
}

static void *domain_copy(void *self)
{
	trules_domain *domain = self;
	return domain->range ? trules_range_new(domain->start, domain->end) :
		trules_points_new(domain->item_type, domain->values.data,
			domain->values.len);
}

static void domain_free(void *self)
{
	trules_domain *domain = self;
	ttypeval_release(domain->item_type);
	tobj_vec_free(&domain->values);
	free(domain);
}

static int domain_identical(void *self, void *other)
{
	trules_domain *left = self;
	trules_domain *right = other;
	if (!ttypeval_equal(left->item_type, right->item_type)) return 0;
	if (left->range)
		return left->start == right->start && left->end == right->end;
	if (left->values.len != right->values.len) return 0;
	for (uint_objs i = 0; i < left->values.len; i++)
		if (!contains(right, &left->values.data[i])) return 0;
	return 1;
}

static void domain_render(tformat_context *context, const void *self)
{
	const trules_domain *domain = self;
	tformat_text(context, domain->range ? "RangeOf[" : "PointsOf[");
	tformat_type(context, domain->item_type);
	tformat_text(context, "][");
	if (domain->range) {
		tformat_number(context, domain->start);
		tformat_text(context, ", ");
		tformat_number(context, domain->end);
	} else {
		tformat_sequence(context, &domain->values);
	}
	tformat_text(context, "]");
}

static tstring *domain_tostring_abbr(void *self)
{
	return tformat_pointer(self);
}

static tstring *domain_tostring_full(void *self)
{
	return tformat_object(self, 16384, domain_render);
}

static void domain_index(void *self, const tobj *arguments,
	uint_regs count, tobj *result)
{
	trules_domain *domain = self;
	if (count == 1 && arguments[0].type == tint) {
		long index = arguments[0].val.v_tint;
		long length = domain_len(domain);
		if (index < 0 || index >= length)
			twarn(ErrRuntime_ObjUnfound, "domain",
			      "index outside Domain");
		if (domain->range)
			tobj_set_int(result, domain->start + index);
		else
			tobj_copy(result, &domain->values.data[index]);
		return;
	}
	if (count != 1 || arguments[0].type != tcompo ||
	    tobj_compo_type(&arguments[0]) != compo_tstr)
		twarn(ErrRuntime_ParamsType, "domain", "String field required");
	const char *name = tstring_cstr(
		((tstr *)arguments[0].val.v_tcompo)->data);
	if (!strcmp(name, "type"))
		tobj_set_compo(result, (tcompo_v *)domain->item_type);
	else if (domain->range && !strcmp(name, "start"))
		tobj_set_int(result, domain->start);
	else if (domain->range && !strcmp(name, "end"))
		tobj_set_int(result, domain->end);
	else if (!domain->range && !strcmp(name, "values")) {
		tlist *list = tlist_new();
		for (uint_objs i = 0; i < domain->values.len; i++)
			tobj_vec_push(&list->items, &domain->values.data[i]);
		tobj_set_compo(result, (tcompo_v *)list);
	} else {
		twarn(ErrRuntime_ObjUnfound, "domain", "unknown field");
	}
}

static void domain_runtime_type(void *self, tobj *result)
{
	trules_domain *domain = self;
	tstring *item_name = tstring_new("item");
	ttype_field parameter = { .name = item_name, .type = domain->item_type };
	const textension_nominal_template *schema = domain->range ?
		&trules_range_template : &trules_points_template;
	tobj_set_compo(result, (tcompo_v *)ttypeval_new_named_application(
		schema->identity, &parameter, 1, schema->capabilities));
	tstring_free(item_name);
}

static int domain_matches_type(void *self, const tobj *value)
{
	trules_domain *domain = self;
	if (!value || value->type != tcompo ||
	    tobj_compo_type(value) != compo_ttypeval)
		return 0;
	const ttypeval *type = (const ttypeval *)value->val.v_tcompo;
	const char *identity = domain->range ? trules_range_template.identity :
		trules_points_template.identity;
	if (type->kind != ttype_kind_named || !type->named_identity ||
	    !tstring_eq_cstr(type->named_identity, identity) ||
	    type->named_parameter_count != 1) return 0;
	return ttypeval_equal(domain->item_type,
		ttypeval_parameter(type, "item"));
}

static const tcompo_capabilities domain_capabilities = {
	.indexable = domain_index,
	.contains = contains,
	.runtime_type = domain_runtime_type,
	.matches_type = domain_matches_type
};

#define DOMAIN_VTABLE(name) { \
	.get_type = name, \
	.get_compo_type_code = domain_code, \
	.len = domain_len, \
	.copy = domain_copy, \
	.free = domain_free, \
	.identical = domain_identical, \
	.tostring_abbr = domain_tostring_abbr, \
	.tostring_full = domain_tostring_full, \
	.capabilities = &domain_capabilities \
}

static tcompo_vtable points_vtable = DOMAIN_VTABLE(points_name);
static tcompo_vtable range_vtable = DOMAIN_VTABLE(range_name);
