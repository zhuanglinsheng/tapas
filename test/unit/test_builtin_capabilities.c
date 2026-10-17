#include "tapas/objects/tlist.h"
#include "tapas/objects/tstr.h"
#include "tapas/textension.h"
#include "tapas/tstdlib.h"
#include "tapas/tval.h"

#include <assert.h>
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

static const tcompo_capabilities probe_capabilities = {
    .indexable = probe_index,
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
  probe.contents->base.vtable->free(probe.contents);
  return 0;
}
