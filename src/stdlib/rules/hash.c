/**
 * @file hash.c
 * @brief Implements structural hashes for the rules package.
 * @details Computes semantic and content fingerprints from core RuleIR objects
 * and exposes the corresponding rules package functions.
 * @note Hashes are cache and comparison aids, not collision-free identities.
 */
#include "hash.h"
#include "tapas/dsa/tstring.h"

#include "tapas/objects/trule.h"
#include "tapas/objects/tstr.h"
#include "tapas/objects/ttype.h"

#include <string.h>

static trule_term *as_term(const tobj *value) {
  return value && value->type == tcompo && value->val.v_tcompo &&
                 tobj_compo_type(value) == compo_trule_term
             ? (trule_term *)value->val.v_tcompo
             : nullptr;
}

static uint64_t hash_bytes(uint64_t hash, const char *data, size_t length) {
  for (size_t i = 0; i < length; i++) {
    hash ^= (unsigned char)data[i];
    hash *= 1099511628211ULL;
  }
  return hash;
}

static long serial_term_index(const trule_ir *ir, const trule_term *term);

uint64_t trule_ir_semantic_hash(const trule_ir *ir) {
  uint64_t hash = 1469598103934665603ULL;
  for (uint_objs i = 0; i < ir->terms.len; i++) {
    trule_term *term = (trule_term *)ir->terms.data[i].val.v_tcompo;
    hash = hash_bytes(hash, trule_term_kind_name(term->kind),
                      strlen(trule_term_kind_name(term->kind)));
    hash = hash_bytes(hash, tstring_cstr(term->type->canonical),
                      tstring_len(term->type->canonical));
    if (term->kind == trule_term_constant ||
        term->kind == trule_term_intrinsic || term->kind == trule_term_call ||
        term->kind == trule_term_construct ||
        term->kind == trule_term_convert ||
        term->kind == trule_term_extension) {
      if (term->kind == trule_term_call && as_term(&term->payload)) {
        long target = serial_term_index(ir, as_term(&term->payload));
        hash ^= (uint64_t)target + 1;
        hash *= 1099511628211ULL;
      } else {
        tstring *payload = tobj_tostring_full(&term->payload);
        hash = hash_bytes(hash, tstring_cstr(payload), tstring_len(payload));
        tstring_free(payload);
      }
    }
    if (tstring_len(term->provider)) {
      hash = hash_bytes(hash, tstring_cstr(term->provider),
                        tstring_len(term->provider));
      hash = hash_bytes(hash, tstring_cstr(term->provider_kind),
                        tstring_len(term->provider_kind));
      hash ^= (uint64_t)term->provider_version;
      hash *= 1099511628211ULL;
    }
    for (uint_objs j = 0; j < term->arguments.len; j++) {
      trule_term *argument = (trule_term *)term->arguments.data[j].val.v_tcompo;
      long position = serial_term_index(ir, argument);
      hash ^= (uint64_t)position + 1;
      hash *= 1099511628211ULL;
    }
  }
  for (uint_objs i = 0; i < ir->items.len; i++) {
    trule_item *item = (trule_item *)ir->items.data[i].val.v_tcompo;
    hash ^= (uint64_t)item->kind + 1;
    hash *= 1099511628211ULL;
    trule_term *root =
        item->kind == trule_item_requirement ? item->rule : item->term;
    long position = serial_term_index(ir, root);
    hash ^= (uint64_t)(position + 1);
    hash *= 1099511628211ULL;
    for (uint_objs j = 0; j < item->arguments.len; j++) {
      position = serial_term_index(
          ir, (trule_term *)item->arguments.data[j].val.v_tcompo);
      hash ^= (uint64_t)(position + 1);
      hash *= 1099511628211ULL;
    }
  }
  return hash;
}

uint64_t trule_ir_content_hash(const trule_ir *ir) {
  uint64_t hash = trule_ir_semantic_hash(ir);
  hash = hash_bytes(hash, tstring_cstr(ir->display_name),
                    tstring_len(ir->display_name));
  hash = hash_bytes(hash, tstring_cstr(ir->source), tstring_len(ir->source));
  for (uint_objs i = 0; i < ir->items.len; i++) {
    trule_item *item = (trule_item *)ir->items.data[i].val.v_tcompo;
    hash = hash_bytes(hash, tstring_cstr(item->description),
                      tstring_len(item->description));
  }
  for (uint_objs i = 0; i < ir->parameters.len; i++) {
    trule_term *parameter = (trule_term *)ir->parameters.data[i].val.v_tcompo;
    tstring *name = tobj_tostring_full(&parameter->payload);
    hash = hash_bytes(hash, tstring_cstr(name), tstring_len(name));
    tstring_free(name);
  }
  return hash;
}

static long serial_term_index(const trule_ir *ir, const trule_term *term) {
  for (uint_objs i = 0; i < ir->terms.len; i++)
    if (ir->terms.data[i].val.v_tcompo == (tcompo_v *)term ||
        (term->kind == trule_term_parameter &&
         ((trule_term *)ir->terms.data[i].val.v_tcompo)->kind ==
             trule_term_parameter &&
         ((trule_term *)ir->terms.data[i].val.v_tcompo)->id == term->id))
      return (long)i;
  return -1;
}

static trule_ir *rule_ir_argument(const tobj *value, const char *name) {
  if (value && value->type == tcompo && value->val.v_tcompo) {
    if (tobj_compo_type(value) == compo_trule_ir)
      return (trule_ir *)value->val.v_tcompo;
    if (tobj_compo_type(value) == compo_trule)
      return ((trule *)value->val.v_tcompo)->ir;
    if (tobj_compo_type(value) == compo_trule_instance)
      return ((trule *)((trule_instance *)value->val.v_tcompo)
                  ->rule.val.v_tcompo)
          ->ir;
  }
  twarn(ErrRuntime_ParamsType, name, "RuleIR, Rule or RuleInstance required");
  return nullptr;
}

static void hash_result(tobj *params, uint_regs count, tobj *result,
                        int content) {
  const char *name = content ? "rules::content_hash" : "rules::semantic_hash";
  if (count != 1)
    twarn(ErrRuntime_ParamsCtr, name, "incorrect parameter count");
  trule_ir *ir = rule_ir_argument(&params[0], name);
  uint64_t hash =
      content ? trule_ir_content_hash(ir) : trule_ir_semantic_hash(ir);
  tobj_set_int(result, (long)(hash & 0x7fffffffffffffffULL));
}

void trules_semantic_hash(tobj *params, uint_regs count, tobj *result) {
  hash_result(params, count, result, 0);
}

void trules_content_hash(tobj *params, uint_regs count, tobj *result) {
  hash_result(params, count, result, 1);
}
