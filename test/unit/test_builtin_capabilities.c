#include "tapas/objects/tlist.h"
#include "tapas/objects/titer.h"
#include "tapas/dsa/tstring.h"
#include "tapas/objects/tstr.h"
#include "tapas/textension.h"
#include "tapas/tstdlib.h"
#include "tapas/tval.h"

#include <assert.h>
#include <limits.h>
#include <string.h>

typedef struct {
  tcompo_v base;
  tlist *contents;
  unsigned lookups;
} capability_probe;

static int probe_next(void *self, long *position, tobj *result) {
  capability_probe *probe = self;
  if (*position >= (long)tlist_size(probe->contents))
    return 0;
  tobj_copy(result, tlist_at(probe->contents, (uint_count)(*position)++));
  return 1;
}

static int probe_try_index(void *self, const tobj *arguments,
                           uint_regs count, tobj *result) {
  capability_probe *probe = self;
  probe->lookups++;
  assert(count == 2);
  assert(arguments[0].type == tint && arguments[1].type == tint);
  if (arguments[0].val.v_tint < 0)
    return 0;
  tobj_set_int(result,
               arguments[0].val.v_tint * 10 + arguments[1].val.v_tint);
  return 1;
}

static void probe_index(void *self, const tobj *arguments,
                        uint_regs count, tobj *result) {
  assert(probe_try_index(self, arguments, count, result));
}

static void probe_set(void *self, const tobj *arguments,
                      uint_regs count, const tobj *value) {
  capability_probe *probe = self;
  assert(count == 1 && arguments[0].type == tint);
  assert(arguments[0].val.v_tint >= 0 &&
         arguments[0].val.v_tint < (long)tlist_size(probe->contents));
  probe->lookups++;
  tobj_vec_set(&probe->contents->items,
               (uint_count)arguments[0].val.v_tint, value);
}

static const tcompo_capabilities probe_capabilities = {
    .indexable = probe_index,
    .index_settable = probe_set,
    .try_indexable = probe_try_index,
    .iterable = probe_next};

static tcompo_vtable probe_vtable = {
    .compo_code = compo_extension,
    .capabilities = &probe_capabilities};

static tnative_function builtin(const char *name) {
  textension_symbol_ref found;
  assert(textension_find(tstdlib_descriptor(), nullptr, name, &found));
  assert(found.symbol->function);
  return found.symbol->function;
}

int main(void) {
  capability_probe probe = {
      .base = {.vtable = &probe_vtable, .refctr = 1},
      .contents = tlist_new()};
  tobj word = {.type = tcompo,
               .val.v_tcompo = (tcompo_v *)tstr_new("first")};
  tobj_vec_push(&probe.contents->items, &word);
  tobj_set_nil(&word);
  tobj_set_compo(&word, (tcompo_v *)tstr_new("second"));
  tobj_vec_push(&probe.contents->items, &word);
  tobj_set_nil(&word);

  tobj arguments[4];
  for (unsigned i = 0; i < 4; i++)
    tobj_set_nil(&arguments[i]);
  arguments[0].type = tcompo;
  arguments[0].val.v_tcompo = &probe.base;
  tobj_set_int(&arguments[1], 2);
  tobj_set_int(&arguments[2], 3);
  tobj_set_int(&arguments[3], 99);
  tobj result;
  tobj_set_nil(&result);

  builtin("idx")(arguments, 3, &result);
  assert(result.type == tint && result.val.v_tint == 23);
  assert(probe.lookups == 1);
  builtin("idx_or")(arguments, 4, &result);
  assert(result.type == tint && result.val.v_tint == 23);
  assert(probe.lookups == 2);
  tobj_set_int(&arguments[1], -1);
  builtin("idx_or")(arguments, 4, &result);
  assert(result.type == tint && result.val.v_tint == 99);
  assert(probe.lookups == 3);

  tobj_set_compo(&arguments[1], (tcompo_v *)tstr_new(" / "));
  builtin("join")(arguments, 2, &result);
  assert(tobj_compo_type(&result) == compo_tstr);
  assert(strcmp(tstring_cstr(((tstr *)result.val.v_tcompo)->data),
                "first / second") == 0);
  tobj_try_clear(&result);
  tobj_try_clear(&arguments[1]);

  arguments[1] = arguments[0];
  builtin("concat")(arguments, 2, &result);
  assert(tobj_compo_type(&result) == compo_tlist);
  tlist *combined = (tlist *)result.val.v_tcompo;
  assert(tlist_size(combined) == 4);
  for (uint_count i = 0; i < 4; i++)
    assert(tobj_identical(tlist_at(combined, i),
                          tlist_at(probe.contents, i % 2)));
  tobj_try_clear(&result);
  assert(tlist_at(probe.contents, 0)->val.v_tcompo->refctr == 1);
  assert(tlist_at(probe.contents, 1)->val.v_tcompo->refctr == 1);
  // Count must use the extension's iteration protocol without retaining values.
  arguments[1] = *tlist_at(probe.contents, 0);
  builtin("count")(arguments, 2, &result);
  assert(result.type == tint && result.val.v_tint == 1);
  assert(arguments[1].val.v_tcompo->refctr == 1);

  // Fill must dispatch writes on a non-core IndexSettable object.
  tobj_set_compo(&arguments[1], (tcompo_v *)tlist_new());
  arguments[1].val.v_tcompo->refctr++;
  tobj_set_compo(&arguments[2], (tcompo_v *)titer_new(0, 2));
  unsigned previous_lookups = probe.lookups;
  builtin("fill")(arguments, 3, &result);
  assert(result.type == tnil && probe.lookups == previous_lookups + 2);
  assert(arguments[1].val.v_tcompo->refctr == 3);
  assert(probe.contents->items.compo_count == 2);
  // The List fast path must release replaced composite values correctly.
  arguments[0].val.v_tcompo = (tcompo_v *)probe.contents;
  tobj payload = arguments[1];
  tobj_set_bool(&arguments[1], 0);
  builtin("fill")(arguments, 3, &result);
  assert(payload.val.v_tcompo->refctr == 1);
  assert(probe.contents->items.compo_count == 0);
  builtin("count")(arguments, 2, &result);
  assert(result.val.v_tint == 2);
  tobj_ddc_ref_clear(&payload);
  tobj_try_clear(&arguments[2]);
  tobj_set_compo(&arguments[2], (tcompo_v *)titer_new_step(-1, LONG_MIN, -2));
  tobj_set_bool(&arguments[1], 1);
  builtin("fill")(arguments, 3, &result);
  assert(tlist_at(probe.contents, 1)->val.v_tbool);
  tobj_try_clear(&arguments[2]);

  // Embedded NULs are bytes, not terminators, for String counting.
  const char bytes[] = {'x', '\0', 'x', '\0'};
  tobj_set_nil(&arguments[0]); // Borrowed List, freed explicitly below.
  tobj_set_compo(&arguments[0], (tcompo_v *)tstr_new_len(bytes, sizeof(bytes)));
  tobj_set_compo(&arguments[1], (tcompo_v *)tstr_new_len(bytes + 1, 1));
  builtin("count")(arguments, 2, &result);
  assert(result.val.v_tint == 2);
  tobj_try_clear(&arguments[0]);
  tobj_try_clear(&arguments[1]);
  probe.contents->base.vtable->free(probe.contents);
  return 0;
}
