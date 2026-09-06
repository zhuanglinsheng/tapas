/**
 * @file codec.c
 * @brief Implements RuleIR persistence for the rules package.
 * @details Encodes and validates versioned TPIR text and implements the public
 * rules package serialization and deserialization functions.
 * @note Only explicitly supported values are portable; source closures retain
 * the existing process-local fallback behavior.
 */
#include "codec.h"
#include "tapas/dsa/tstring.h"
#include "domain.h"
#include "hash.h"

#include "tapas/objects/trule.h"
#include "tapas/objects/tstr.h"

#include <stdlib.h>
#include <string.h>

static void serial_text(tstring *out, const char *text) {
  size_t length = text ? strlen(text) : 0;
  tstring_append_fmt(out, "%zu:", length);
  if (length)
    tstring_append(out, text);
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

static int serial_value(tstring *out, const trule_ir *ir, const tobj *value) {
  switch (value->type) {
  case tnil:
    tstring_append(out, "n;");
    return 1;
  case tbool:
    tstring_append_fmt(out, "b%d;", value->val.v_tbool);
    return 1;
  case tint:
    tstring_append_fmt(out, "i%ld;", value->val.v_tint);
    return 1;
  case tfloat:
    tstring_append_fmt(out, "f%.17g;", value->val.v_tfloat);
    return 1;
  case tcompo:
    if (trules_domain_is_range(value)) {
      const trules_domain *d = (trules_domain *)value->val.v_tcompo;
      tstring_append_fmt(out, "g%ld;%ld;", d->start, d->end);
      return 1;
    }
    if (trules_domain_is_points(value)) {
      const trules_domain *d = (trules_domain *)value->val.v_tcompo;
      if (d->item_type->contains_instance)
        return 0;
      tstring_append_c(out, 'p');
      serial_text(out, tstring_cstr(d->item_type->canonical));
      tstring_append_fmt(out, "%u;", (unsigned)d->values.len);
      for (uint_objs i = 0; i < d->values.len; i++) {
        const tobj *v = &d->values.data[i];
        /* Retain the existing transport boundary: no arbitrary object graphs.
         */
        if (v->type == tcompo && tobj_compo_type(v) != compo_tstr &&
            tobj_compo_type(v) != compo_ttypeval)
          return 0;
        if (!serial_value(out, ir, v))
          return 0;
      }
      return 1;
    }
    if (tobj_compo_type(value) == compo_tstr) {
      tstring_append_c(out, 's');
      serial_text(out, tstring_cstr(((tstr *)value->val.v_tcompo)->data));
      return 1;
    }
    if (tobj_compo_type(value) == compo_ttypeval) {
      if (((ttypeval *)value->val.v_tcompo)->contains_instance)
        return 0;
      tstring_append_c(out, 'y');
      serial_text(out,
                  tstring_cstr(((ttypeval *)value->val.v_tcompo)->canonical));
      return 1;
    }
    if (tobj_compo_type(value) == compo_trule_term) {
      long index = serial_term_index(ir, (trule_term *)value->val.v_tcompo);
      if (index < 0)
        return 0;
      tstring_append_fmt(out, "r%ld;", index);
      return 1;
    }
    return 0;
  }
  return 0;
}

int trule_ir_serialize(const trule_ir *ir, tstring **result) {
  tstring *out = tstring_new(ir->version >= 7   ? "TPIR7;"
                             : ir->version >= 6 ? "TPIR6;"
                             : ir->version == 5 ? "TPIR5;"
                             : ir->version == 4 ? "TPIR4;"
                             : ir->version == 3 ? "TPIR3;"
                             : ir->version == 2 ? "TPIR2;"
                                                : "TPIR1;");
  serial_text(out, tstring_cstr(ir->display_name));
  serial_text(out, tstring_cstr(ir->source));
  tstring_append_fmt(out, "%ld;%u;%u;%u;", ir->version, (unsigned)ir->terms.len,
                     (unsigned)ir->parameters.len, (unsigned)ir->items.len);
  for (uint_objs i = 0; i < ir->terms.len; i++) {
    trule_term *term = (trule_term *)ir->terms.data[i].val.v_tcompo;
    tstring_append_fmt(out, "%d;", (int)term->kind);
    if (term->type->contains_instance) {
      tstring_free(out);
      return 0;
    }
    serial_text(out, tstring_cstr(term->type->canonical));
    if (!serial_value(out, ir, &term->payload)) {
      tstring_free(out);
      return 0;
    }
    tstring_append_fmt(out, "%u;", (unsigned)term->arguments.len);
    for (uint_objs j = 0; j < term->arguments.len; j++) {
      long index = serial_term_index(
          ir, (trule_term *)term->arguments.data[j].val.v_tcompo);
      if (index < 0) {
        tstring_free(out);
        return 0;
      }
      tstring_append_fmt(out, "%ld;", index);
    }
    serial_text(out, tstring_cstr(term->provider));
    serial_text(out, tstring_cstr(term->provider_kind));
    tstring_append_fmt(out, "%ld;%ld;%ld;", term->provider_version,
                       term->origin_start, term->origin_end);
  }
  for (uint_objs i = 0; i < ir->parameters.len; i++) {
    long index = serial_term_index(
        ir, (trule_term *)ir->parameters.data[i].val.v_tcompo);
    if (index < 0) {
      tstring_free(out);
      return 0;
    }
    tstring_append_fmt(out, "%ld;", index);
  }
  for (uint_objs i = 0; i < ir->items.len; i++) {
    trule_item *item = (trule_item *)ir->items.data[i].val.v_tcompo;
    long index = serial_term_index(
        ir, item->kind == trule_item_requirement ? item->rule : item->term);
    if (index < 0) {
      tstring_free(out);
      return 0;
    }
    tstring_append_fmt(out, "%d;%ld;%u;", (int)item->kind, index,
                       (unsigned)item->arguments.len);
    for (uint_objs j = 0; j < item->arguments.len; j++) {
      long argument = serial_term_index(
          ir, (trule_term *)item->arguments.data[j].val.v_tcompo);
      if (argument < 0) {
        tstring_free(out);
        return 0;
      }
      tstring_append_fmt(out, "%ld;", argument);
    }
    serial_text(out, tstring_cstr(item->description));
    tstring_append_fmt(out, "%ld;%ld;", item->origin_start, item->origin_end);
  }
  *result = out;
  return 1;
}

static int parse_long(const char **cursor, long *value) {
  char *end;
  long parsed = strtol(*cursor, &end, 10);
  if (end == *cursor || *end != ';')
    return 0;
  *cursor = end + 1;
  *value = parsed;
  return 1;
}

static char *parse_text(const char **cursor) {
  char *end;
  unsigned long length = strtoul(*cursor, &end, 10);
  if (end == *cursor || *end != ':' || strlen(end + 1) < length)
    return nullptr;
  char *text = calloc(length + 1, 1);
  if (!text)
    abort();
  memcpy(text, end + 1, length);
  *cursor = end + 1 + length;
  return text;
}

static int parse_value(const char **cursor, trule_term **terms,
                       uint_objs term_count, tobj *result) {
  char tag = *(*cursor)++;
  tobj_set_nil(result);
  if (tag == 'g') {
    long start, end;
    if (!parse_long(cursor, &start) || !parse_long(cursor, &end) || start > end)
      return 0;
    tobj_set_compo(result, (tcompo_v *)trules_range_new(start, end));
    return 1;
  }
  if (tag == 'p') {
    char *canonical = parse_text(cursor);
    ttypeval *type = canonical
                         ? ttypeval_retain(ttypeval_from_canonical(canonical))
                         : nullptr;
    free(canonical);
    long count;
    if (!type || !parse_long(cursor, &count) || count < 0 ||
        (unsigned long)count > strlen(*cursor)) {
      ttypeval_release(type);
      return 0;
    }
    tobj *values = calloc(count + 1, sizeof(*values));
    if (!values)
      abort();
    int valid = 1;
    for (long i = 0; valid && i < count; i++) {
      if (**cursor != 'n' && **cursor != 'b' && **cursor != 'i' &&
          **cursor != 'f' && **cursor != 's' && **cursor != 'y')
        valid = 0;
      else
        valid = parse_value(cursor, terms, term_count, &values[i]) &&
                ttypeval_matches(&values[i], type);
    }
    if (valid)
      tobj_set_compo(result,
                     (tcompo_v *)trules_points_new(
                         type, values, (uint_regs)count));
    for (long i = 0; i < count; i++)
      tobj_try_clear(&values[i]);
    free(values);
    ttypeval_release(type);
    return valid;
  }
  if (tag == 'n')
    return *(*cursor)++ == ';';
  if (tag == 's' || tag == 'y') {
    char *text = parse_text(cursor);
    if (!text)
      return 0;
    if (tag == 's')
      tobj_set_compo(result, (tcompo_v *)tstr_new(text));
    else {
      ttypeval *type = ttypeval_from_canonical(text);
      if (!type) {
        free(text);
        return 0;
      }
      tobj_set_compo(result, (tcompo_v *)type);
    }
    free(text);
    return 1;
  }
  long value;
  if (tag == 'f') {
    char *end;
    double number = strtod(*cursor, &end);
    if (end == *cursor || *end != ';')
      return 0;
    *cursor = end + 1;
    tobj_set_float(result, number);
    return 1;
  }
  if (!parse_long(cursor, &value))
    return 0;
  if (tag == 'b')
    tobj_set_bool(result, (int)value);
  else if (tag == 'i')
    tobj_set_int(result, value);
  else if (tag == 'r' && value >= 0 && (uint_objs)value < term_count)
    tobj_set_compo(result, (tcompo_v *)terms[value]);
  else
    return 0;
  return 1;
}

trule_ir *trule_ir_deserialize(const char *data) {
  if (!data ||
      (strncmp(data, "TPIR1;", 6) != 0 && strncmp(data, "TPIR2;", 6) != 0 &&
       strncmp(data, "TPIR3;", 6) != 0 && strncmp(data, "TPIR4;", 6) != 0 &&
       strncmp(data, "TPIR5;", 6) != 0 && strncmp(data, "TPIR6;", 6) != 0 &&
       strncmp(data, "TPIR7;", 6) != 0))
    return nullptr;
  const char *cursor = data + 6;
  char *name = parse_text(&cursor);
  char *source = parse_text(&cursor);
  long version;
  long term_count_value;
  long parameter_count_value;
  long item_count_value;
  if (!name || !source || !parse_long(&cursor, &version) ||
      version != data[4] - '0' || !parse_long(&cursor, &term_count_value) ||
      !parse_long(&cursor, &parameter_count_value) ||
      !parse_long(&cursor, &item_count_value) || term_count_value < 0 ||
      parameter_count_value < 0 || item_count_value < 0) {
    free(name);
    free(source);
    return nullptr;
  }
  uint_objs term_count = (uint_objs)term_count_value;
  trule_term **terms =
      term_count ? calloc(term_count, sizeof(*terms)) : nullptr;
  trule_ir *ir = trule_ir_new(name, source);
  ir->version = version;
  free(name);
  free(source);
  for (uint_objs i = 0; i < term_count; i++) {
    long kind;
    char *canonical;
    tobj payload;
    long argument_count;
    if (!parse_long(&cursor, &kind) || kind < trule_term_constant ||
        kind > trule_term_in || (kind == trule_term_in && version < 6) ||
        (kind == trule_term_not && version < 4) ||
        (kind >= trule_term_and && version < 5) ||
        !(canonical = parse_text(&cursor)) ||
        !parse_value(&cursor, terms, i, &payload) ||
        !parse_long(&cursor, &argument_count) || argument_count < 0)
      goto invalid;
    ttypeval *type = ttypeval_from_canonical(canonical);
    free(canonical);
    if (!type) {
      tobj_try_clear(&payload);
      goto invalid;
    }
    tobj *arguments =
        argument_count ? calloc((uint_objs)argument_count, sizeof(*arguments))
                       : nullptr;
    for (long j = 0; j < argument_count; j++) {
      long index;
      if (!parse_long(&cursor, &index) || index < 0 || (uint_objs)index >= i) {
        free(arguments);
        goto invalid;
      }
      tobj_set_nil(&arguments[j]);
      tobj_set_compo(&arguments[j], (tcompo_v *)terms[index]);
    }
    if (kind == trule_term_in &&
        (argument_count != 2 || payload.type != tnil ||
         !ttypeval_equal(type, ttypeval_builtin(tbuiltintype_bool)))) {
      free(arguments);
      tobj_try_clear(&payload);
      goto invalid;
    }
    if (kind == trule_term_not &&
        (argument_count != 1 ||
         !ttypeval_equal(type, ttypeval_builtin(tbuiltintype_bool)) ||
         payload.type != tnil ||
         (!trule_antecedent_type(
              ((trule_term *)arguments[0].val.v_tcompo)->type) &&
          !ttypeval_equal(((trule_term *)arguments[0].val.v_tcompo)->type,
                          ttypeval_builtin(tbuiltintype_any))))) {
      free(arguments);
      tobj_try_clear(&payload);
      goto invalid;
    }
    if (kind == trule_term_and || kind == trule_term_or) {
      int valid = argument_count == 2 && payload.type == tnil &&
                  ttypeval_equal(type, ttypeval_builtin(tbuiltintype_bool));
      for (long j = 0; valid && j < argument_count; j++) {
        ttypeval *operand = ((trule_term *)arguments[j].val.v_tcompo)->type;
        valid = trule_antecedent_type(operand) ||
                ttypeval_equal(operand, ttypeval_builtin(tbuiltintype_any));
      }
      if (!valid) {
        free(arguments);
        tobj_try_clear(&payload);
        goto invalid;
      }
    }
    char *provider = parse_text(&cursor);
    char *provider_kind = parse_text(&cursor);
    long provider_version;
    long origin_start;
    long origin_end;
    if (!provider || !provider_kind ||
        !parse_long(&cursor, &provider_version) ||
        !parse_long(&cursor, &origin_start) ||
        !parse_long(&cursor, &origin_end)) {
      free(provider);
      free(provider_kind);
      free(arguments);
      goto invalid;
    }
    terms[i] = trule_term_new((trule_term_kind)kind, type, &payload, arguments,
                              (uint_regs)argument_count);
    tobj_try_clear(&payload);
    free(arguments);
    tstring_free(terms[i]->provider);
    tstring_free(terms[i]->provider_kind);
    terms[i]->provider = tstring_new(provider);
    terms[i]->provider_kind = tstring_new(provider_kind);
    terms[i]->provider_version = provider_version;
    terms[i]->origin_start = origin_start;
    terms[i]->origin_end = origin_end;
    free(provider);
    free(provider_kind);
  }
  for (long i = 0; i < parameter_count_value; i++) {
    long index;
    if (!parse_long(&cursor, &index) || index < 0 ||
        (uint_objs)index >= term_count ||
        terms[index]->kind != trule_term_parameter)
      goto invalid;
    trule_ir_add_parameter(ir, terms[index]);
  }
  for (long i = 0; i < item_count_value; i++) {
    long kind;
    long index;
    long argument_count;
    if (!parse_long(&cursor, &kind) || !parse_long(&cursor, &index) ||
        !parse_long(&cursor, &argument_count) || index < 0 ||
        (uint_objs)index >= term_count || argument_count < 0 ||
        kind < trule_item_condition || kind > trule_item_implication ||
        (kind == trule_item_implication && data[4] == '1'))
      goto invalid;
    tobj *arguments =
        argument_count ? calloc((uint_objs)argument_count, sizeof(*arguments))
                       : nullptr;
    for (long j = 0; j < argument_count; j++) {
      long argument;
      if (!parse_long(&cursor, &argument) || argument < 0 ||
          (uint_objs)argument >= term_count) {
        free(arguments);
        goto invalid;
      }
      tobj_set_nil(&arguments[j]);
      tobj_set_compo(&arguments[j], (tcompo_v *)terms[argument]);
    }
    char *description = parse_text(&cursor);
    long start;
    long end;
    if (!description || !parse_long(&cursor, &start) ||
        !parse_long(&cursor, &end)) {
      free(description);
      free(arguments);
      goto invalid;
    }
    if (kind == trule_item_implication) {
      int valid =
          argument_count > 0 && trule_antecedent_type(terms[index]->type) &&
          (data[4] >= '3' ||
           ttypeval_equal(terms[index]->type, ttypeval_builtin(tbuiltintype_bool)));
      for (long j = 0; valid && j < argument_count; j++)
        valid = ttypeval_equal(((trule_term *)arguments[j].val.v_tcompo)->type,
                               ttypeval_builtin(tbuiltintype_bool));
      if (!valid) {
        free(description);
        free(arguments);
        goto invalid;
      }
    }
    trule_item *item =
        kind == trule_item_implication
            ? trule_implication_new(terms[index], arguments,
                                    (uint_regs)argument_count, description)
        : kind == trule_item_condition
            ? trule_condition_new(terms[index], description)
            : trule_requirement_new(terms[index], arguments,
                                    (uint_regs)argument_count);
    trule_term **root =
        kind == trule_item_requirement ? &item->rule : &item->term;
    if ((*root)->base.refctr > 0)
      (*root)->base.refctr--;
    if ((*root)->base.refctr == 0)
      (*root)->base.vtable->free(*root);
    *root = terms[index];
    terms[index]->base.refctr++;
    item->origin_start = start;
    item->origin_end = end;
    trule_ir_add_item(ir, item);
    if (item->base.refctr == 0)
      item->base.vtable->free(item);
    free(description);
    free(arguments);
  }
  if (*cursor)
    goto invalid;
  for (uint_objs i = 0; i < term_count; i++)
    if (terms[i]->base.refctr == 0)
      terms[i]->base.vtable->free(terms[i]);
  free(terms);
  return ir;

invalid:
  for (uint_objs i = 0; i < term_count; i++)
    if (terms[i] && terms[i]->base.refctr == 0)
      terms[i]->base.vtable->free(terms[i]);
  free(terms);
  if (ir->base.refctr == 0)
    ir->base.vtable->free(ir);
  return nullptr;
}

typedef struct rule_serial_entry {
  tstring *key;
  tobj rule;
  struct rule_serial_entry *next;
} rule_serial_entry;

static rule_serial_entry *rule_serialized;

void trules_serialize(tobj *params, uint_regs count, tobj *result) {
  if (count != 1)
    twarn(ErrRuntime_ParamsCtr, "rules::serialize",
          "incorrect parameter count");
  const tobj *value = &params[0];
  trule *rule = nullptr;
  trule_ir *standalone = nullptr;
  if (value->type == tcompo && value->val.v_tcompo &&
      tobj_compo_type(value) == compo_trule)
    rule = (trule *)value->val.v_tcompo;
  else if (value->type == tcompo && value->val.v_tcompo &&
           tobj_compo_type(value) == compo_trule_ir)
    standalone = (trule_ir *)value->val.v_tcompo;
  if (!rule && !standalone)
    twarn(ErrRuntime_ParamsType, "rules::serialize", "Rule or RuleIR required");
  if (standalone || rule->checker.type == tnil) {
    trule_ir *ir = standalone ? standalone : rule->ir;
    for (uint_objs i = 0; i < ir->terms.len; i++) {
      trule_term *term = (trule_term *)ir->terms.data[i].val.v_tcompo;
      if (strcmp(tstring_cstr(term->provider), "tapas.source") == 0)
        twarn(ErrRuntime_Other, "rules::serialize",
              "source RuleIR requires its Rule closure");
    }
    tstring *serialized = nullptr;
    if (!trule_ir_serialize(ir, &serialized))
      twarn(ErrRuntime_Other, "rules::serialize",
            "Rule contains a value that cannot be serialized");
    tobj_set_compo(result, (tcompo_v *)tstr_new(tstring_cstr(serialized)));
    tstring_free(serialized);
    return;
  }
  tstring *key = tstring_new("TPRULE1:");
  tstring_append_fmt(
      key, "%llu:%llu:%p", (unsigned long long)trule_ir_semantic_hash(rule->ir),
      (unsigned long long)trule_ir_content_hash(rule->ir), (void *)rule);
  for (rule_serial_entry *entry = rule_serialized; entry; entry = entry->next)
    if (tstring_cmp(entry->key, key) == 0) {
      tstring_free(key);
      tobj_set_compo(result, (tcompo_v *)tstr_new(tstring_cstr(entry->key)));
      return;
    }
  rule_serial_entry *entry = calloc(1, sizeof(*entry));
  entry->key = key;
  tobj_set_nil(&entry->rule);
  tobj_copy(&entry->rule, value);
  entry->next = rule_serialized;
  rule_serialized = entry;
  tobj_set_compo(result, (tcompo_v *)tstr_new(tstring_cstr(key)));
}

void trules_deserialize(tobj *params, uint_regs count, tobj *result) {
  if (count != 1)
    twarn(ErrRuntime_ParamsCtr, "rules::deserialize",
          "incorrect parameter count");
  const tobj *value = &params[0];
  if (value->type != tcompo || tobj_compo_type(value) != compo_tstr)
    twarn(ErrRuntime_ParamsType, "rules::deserialize", "String required");
  const tstring *key = ((tstr *)value->val.v_tcompo)->data;
  const char *data = tstring_cstr(key);
  if (strncmp(data, "TPIR1;", 6) == 0 || strncmp(data, "TPIR2;", 6) == 0 ||
      strncmp(data, "TPIR3;", 6) == 0 || strncmp(data, "TPIR4;", 6) == 0 ||
      strncmp(data, "TPIR5;", 6) == 0 || strncmp(data, "TPIR6;", 6) == 0 ||
      strncmp(data, "TPIR7;", 6) == 0) {
    trule_ir *ir = trule_ir_deserialize(data);
    if (!ir)
      twarn(ErrRuntime_Other, "rules::deserialize",
            "invalid or unsupported RuleIR serialization");
    tstring *signature = tstring_new_empty();
    for (uint_objs i = 0; i < ir->parameters.len; i++) {
      trule_term *parameter = (trule_term *)ir->parameters.data[i].val.v_tcompo;
      if (i)
        tstring_append_c(signature, '\x1f');
      tstring_append_ts(signature, parameter->type->canonical);
    }
    tobj_set_compo(result,
                   (tcompo_v *)trule_new_dynamic(ir, tstring_cstr(signature)));
    tstring_free(signature);
    if (ir->base.refctr > 0)
      ir->base.refctr--;
    return;
  }
  for (rule_serial_entry *entry = rule_serialized; entry; entry = entry->next)
    if (tstring_cmp(entry->key, key) == 0) {
      tobj_copy(result, &entry->rule);
      return;
    }
  twarn(ErrRuntime_Other, "rules::deserialize",
        "serialized Rule closure is unavailable in this process");
}
