#include "tapas/runtime/trule_format.h"
#include "tapas/runtime/tdomain.h"
#include "tapas/runtime/tlist.h"
#include "tapas/runtime/tstr.h"
#include <stdlib.h>
#include <string.h>

static int contains(void *self, const tobj *value)
{
    tdomain *d = self;
    if (!ttypeval_matches(value, d->item_type)) return 0;
    if (d->range) return value->type == tint && value->val.v_tint >= d->start && value->val.v_tint <= d->end;
    for (uint_objs i = 0; i < d->values.len; i++)
        if (tobj_identical(&d->values.data[i], value)) return 1;
    return 0;
}

tdomain *tpoints_new(ttypeval *type, const tobj *values, uint_regs count)
{
    /* Validate before allocation. Elements retain normal Tapas value/reference semantics. */
    for (uint_regs i = 0; i < count; i++)
        if (!ttypeval_matches(&values[i], type))
            twarn(ErrRuntime_ParamsType, "rules::points", "point does not match declared element Type");
    tdomain *d = calloc(1, sizeof(*d));
    if (!d) abort();
    d->base.vtable = &tpoints_vtable;
    d->item_type = ttypeval_retain(type);
    tobj_vec_init(&d->values);
    for (uint_regs i = 0; i < count; i++)
        if (!contains(d, &values[i])) tobj_vec_push(&d->values, &values[i]);
    return d;
}

tdomain *trange_new(long start, long end)
{
    if (start > end) twarn(ErrRuntime_ParamsType, "rules::range", "start must not exceed end");
    tdomain *d = calloc(1, sizeof(*d));
    if (!d) abort();
    d->base.vtable = &trange_vtable;
    d->item_type = ttypeval_retain(ttypeval_builtin(tbuiltin_int));
    d->range = 1; d->start = start; d->end = end;
    tobj_vec_init(&d->values);
    return d;
}
static const char *points_name(void) { return "PointsOf"; }
static const char *range_name(void) { return "RangeOf"; }
static tcompo_type points_code(void) { return compo_tpoints; }
static tcompo_type range_code(void) { return compo_trange; }
static long domain_len(void *self)
{
    tdomain *d = self;
    if (d->range) twarn(ErrRuntime_ParamsType, "len", "Range cardinality is not exposed; use start/end");
    return (long)d->values.len;
}
static void *domain_copy(void *self)
{
    tdomain *d = self;
    return d->range ? trange_new(d->start, d->end) : tpoints_new(d->item_type, d->values.data, d->values.len);
}
static void domain_free(void *self)
{
    tdomain *d = self;
    ttypeval_release(d->item_type); tobj_vec_free(&d->values); free(d);
}
static int domain_identical(void *self, void *other)
{
    tdomain *a = self, *b = other;
    if (a->range != b->range || !ttypeval_equal(a->item_type, b->item_type)) return 0;
    if (a->range) return a->start == b->start && a->end == b->end;
    if (a->values.len != b->values.len) return 0;
    for (uint_objs i = 0; i < a->values.len; i++)
        if (!contains(b, &a->values.data[i])) return 0;
    return 1;
}

static void domain_index(void *self, const tobj *args, uint_regs count, tobj *result)
{
    tdomain *d = self;
    if (count != 1 || args[0].type != tcompo || tobj_compo_type(&args[0]) != compo_tstr)
        twarn(ErrRuntime_ParamsType, "domain", "String field required");
    const char *name = tstring_cstr(((tstr *)args[0].val.v_tcompo)->data);
    if (!strcmp(name, "type")) tobj_set_compo(result, (tcompo_v *)d->item_type);
    else if (d->range && !strcmp(name, "start")) tobj_set_int(result, d->start);
    else if (d->range && !strcmp(name, "end")) tobj_set_int(result, d->end);
    else if (!d->range && !strcmp(name, "values")) {
        tlist *list = tlist_new();
        for (uint_objs i = 0; i < d->values.len; i++) tobj_vec_push(&list->items, &d->values.data[i]);
        tobj_set_compo(result, (tcompo_v *)list);
    } else twarn(ErrRuntime_ObjUnfound, "domain", "unknown field");
}
static const tcompo_capabilities caps = { .contains = contains, .indexable = domain_index };
#define DOMAIN_VTABLE(name, code) { .get_type = name, .get_compo_type_code = code, .len = domain_len, .copy = domain_copy, .free = domain_free, .identical = domain_identical, .tostring_abbr = trule_format_abbr, .tostring_full = trule_format_full, .capabilities = &caps }
tcompo_vtable tpoints_vtable = DOMAIN_VTABLE(points_name, points_code);
tcompo_vtable trange_vtable = DOMAIN_VTABLE(range_name, range_code);
