/**
 * @file evaluators.c
 * @brief Implements the evaluators standard package.
 * @details The package owns Evaluator values, Context and Result records, and
 * all evaluator-specific behavior. It reaches core execution only through the
 * package-facing VM service interface carried by a session environment.
 * @note No evaluator identity or dispatch branch may be added to the VM or
 * another core source file; new behavior belongs in this package.
 */
#include "tapas/textension.h"
#include "tapas/dsa/tstring.h"
#include "runtime/tenv.h"

#include "tapas/objects/tdict.h"
#include "tapas/objects/tlist.h"
#include "tapas/objects/trule.h"
#include "tapas/objects/trule_ir.h"
#include "tapas/objects/tstr.h"
#include "tapas/objects/ttype.h"
#include "tapas/vm_service.h"

#include "object.h"

#include <stdlib.h>
#include <string.h>

static void dictionary_set(tdict *dictionary, const char *name,
                           const tobj *value) {
  tobj key;
  tobj_set_nil(&key);
  tobj_set_compo(&key, (tcompo_v *)tstr_new(name));
  tdict_set(dictionary, &key, value);
  tobj_try_clear(&key);
}

static void create_evaluator_type(tobj *result) {
  tobj_set_compo(result, (tcompo_v *)tstdlib_evaluator_type());
}

static void create_dictionary_type(tobj *result) {
  tobj_set_compo(result, (tcompo_v *)ttypeval_builtin(tbuiltintype_dictionary));
}

static const char *string_value(const tobj *value, const char *name) {
  if (!value || value->type != tcompo || tobj_compo_type(value) != compo_tstr)
    twarn(ErrRuntime_ParamsType, name, "String required");
  return tstring_cstr(((tstr *)value->val.v_tcompo)->data);
}

static int callable(const tobj *value) {
  if (!value || value->type != tcompo || !value->val.v_tcompo)
    return 0;
  tcompo_type type = tobj_compo_type(value);
  return type == compo_tfunc || type == compo_cppfunc || type == compo_sessfunc;
}

static void evaluators_make(tobj *params, uint_regs count, tobj *result) {
  if (count < 3 || count > 4)
    twarn(ErrRuntime_ParamsCtr, "evaluators::make",
          "three or four arguments required");
  if (!callable(&params[2]) ||
      (count == 4 && params[3].type != tnil && !callable(&params[3])))
    twarn(ErrRuntime_ParamsType, "evaluators::make",
          "evaluate and compile must be Functions");
  if (params[1].type != tint)
    twarn(ErrRuntime_ParamsType, "evaluators::make", "version must be Int");
  tobj_set_compo(
      result, (tcompo_v *)tevaluator_new(
                  string_value(&params[0], "evaluators::make"),
                  params[1].val.v_tint, &params[2],
                  count == 4 && params[3].type != tnil ? &params[3] : nullptr));
}

static int rule_parameter_index(const trule_ir *ir, const trule_term *term) {
  for (uint_objs i = 0; i < ir->parameters.len; i++)
    if (((trule_term *)ir->parameters.data[i].val.v_tcompo)->id == term->id)
      return (int)i;
  return -1;
}

static void evaluator_context(trule_instance *instance, tobj *result) {
  tdict *context = tdict_new();
  dictionary_set(context, "instance",
                 &(tobj){.type = tcompo, .val.v_tcompo = (tcompo_v *)instance});
  dictionary_set(context, "rule", &instance->rule);

  trule *rule = (trule *)instance->rule.val.v_tcompo;
  tobj value;
  tobj_set_nil(&value);
  tobj_set_compo(&value, (tcompo_v *)rule->ir);
  dictionary_set(context, "ir", &value);
  tobj_try_clear(&value);

  tlist *bindings = tlist_new();
  for (uint_objs i = 0; i < instance->arguments.len; i++)
    tobj_vec_push(&bindings->items, &instance->arguments.data[i]);
  tobj_set_compo(&value, (tcompo_v *)bindings);
  dictionary_set(context, "binding", &value);
  tobj_try_clear(&value);
  tobj_set_nil(&value);
  dictionary_set(context, "capture", &value);
  dictionary_set(context, "value", &value);
  dictionary_set(context, "requirement", &value);
  tobj_set_compo(result, (tcompo_v *)context);
}

static void evaluator_result(tobj *result, const char *status,
                             const tobj *value, const char *diagnostic) {
  tdict *record = tdict_new();
  tobj field;
  tobj_set_nil(&field);
  tobj_set_compo(&field, (tcompo_v *)tstr_new(status));
  dictionary_set(record, "status", &field);
  tobj_try_clear(&field);
  if (value)
    tobj_copy(&field, value);
  dictionary_set(record, "value", &field);
  tobj_try_clear(&field);
  tobj_set_compo(&field, (tcompo_v *)tlist_new());
  dictionary_set(record, "violations", &field);
  tobj_try_clear(&field);

  tlist *diagnostics = tlist_new();
  if (diagnostic && *diagnostic) {
    tobj message;
    tobj_set_nil(&message);
    tobj_set_compo(&message, (tcompo_v *)tstr_new(diagnostic));
    tobj_vec_push(&diagnostics->items, &message);
    tobj_try_clear(&message);
  }
  tobj_set_compo(&field, (tcompo_v *)diagnostics);
  dictionary_set(record, "diagnostics", &field);
  tobj_try_clear(&field);
  tobj_set_compo(result, (tcompo_v *)record);
}

static void context_field(const tobj *context, const char *name, tobj *result) {
  if (!context || context->type != tcompo ||
      tobj_compo_type(context) != compo_tdict)
    twarn(ErrRuntime_ParamsType, "evaluators::Context", "Context required");
  tobj key;
  tobj_set_nil(&key);
  tobj_set_compo(&key, (tcompo_v *)tstr_new(name));
  tdict_get((tdict *)context->val.v_tcompo, &key, result);
  tobj_try_clear(&key);
}

static trule_instance *context_instance(const tobj *context) {
  tobj instance;
  tobj_set_nil(&instance);
  context_field(context, "instance", &instance);
  if (instance.type != tcompo ||
      tobj_compo_type(&instance) != compo_trule_instance)
    twarn(ErrRuntime_ParamsType, "evaluators::Context",
          "invalid Context instance");
  trule_instance *value = (trule_instance *)instance.val.v_tcompo;
  tobj_ddc_ref_clear(&instance);
  return value;
}

static void evaluators_binding(tobj *params, uint_regs count, tobj *result,
                               tcompo_env *environment) {
  (void)environment;
  if (count != 2)
    twarn(ErrRuntime_ParamsCtr, "evaluators::binding",
          "two arguments required");
  trule_instance *instance = context_instance(&params[0]);
  trule *rule = (trule *)instance->rule.val.v_tcompo;
  int index = -1;
  if (params[1].type == tint)
    index = (int)params[1].val.v_tint;
  else if (params[1].type == tcompo &&
           tobj_compo_type(&params[1]) == compo_trule_term)
    index =
        rule_parameter_index(rule->ir, (trule_term *)params[1].val.v_tcompo);
  if (index < 0 || (uint_objs)index >= instance->arguments.len)
    twarn(ErrRuntime_Other, "evaluators::binding", "unknown ParameterId");
  tobj_copy(result, &instance->arguments.data[index]);
}

static void evaluators_capture(tobj *params, uint_regs count, tobj *result,
                               tcompo_env *environment) {
  (void)environment;
  if (count != 2)
    twarn(ErrRuntime_ParamsCtr, "evaluators::capture",
          "two arguments required");
  trule_instance *instance = context_instance(&params[0]);
  trule *rule = (trule *)instance->rule.val.v_tcompo;
  trule_term *capture = nullptr;
  if (params[1].type == tint && params[1].val.v_tint >= 0 &&
      (uint_objs)params[1].val.v_tint < rule->ir->captures.len)
    capture = (trule_term *)rule->ir->captures.data[params[1].val.v_tint]
                  .val.v_tcompo;
  else if (params[1].type == tcompo &&
           tobj_compo_type(&params[1]) == compo_trule_term &&
           ((trule_term *)params[1].val.v_tcompo)->kind == trule_term_capture)
    capture = (trule_term *)params[1].val.v_tcompo;
  if (!capture || !trule_read_capture(rule, capture, result))
    twarn(ErrRuntime_Other, "evaluators::capture",
          "unknown or unavailable CaptureId");
}

static void evaluators_value(tobj *params, uint_regs count, tobj *result,
                             tcompo_env *environment) {
  if (count != 2 || params[1].type != tcompo ||
      tobj_compo_type(&params[1]) != compo_trule_term)
    twarn(ErrRuntime_ParamsType, "evaluators::value",
          "Context and Term required");
  trule_instance *instance = context_instance(&params[0]);
  tobj instance_value = {.type = tcompo, .val.v_tcompo = (tcompo_v *)instance};
  tvm_services_evaluate_rule_term(environment, &instance_value, &params[1],
                                  result);
}

static void evaluators_requirement(tobj *params, uint_regs count, tobj *result,
                                   tcompo_env *environment) {
  if (count != 2 || params[1].type != tcompo ||
      tobj_compo_type(&params[1]) != compo_trule_item ||
      ((trule_item *)params[1].val.v_tcompo)->kind != trule_item_requirement)
    twarn(ErrRuntime_ParamsType, "evaluators::requirement",
          "Context and Requirement required");
  trule_instance *instance = context_instance(&params[0]);
  trule_item *item = (trule_item *)params[1].val.v_tcompo;
  tobj instance_value = {.type = tcompo, .val.v_tcompo = (tcompo_v *)instance};
  tobj rule_term = {.type = tcompo, .val.v_tcompo = (tcompo_v *)item->rule};
  tobj target;
  tobj_set_nil(&target);
  tvm_services_evaluate_rule_term(environment, &instance_value, &rule_term,
                                  &target);
  if (target.type == tcompo &&
      tobj_compo_type(&target) == compo_trule_instance &&
      item->arguments.len == 0) {
    tobj_copy(result, &target);
    tobj_try_clear(&target);
    return;
  }
  if (target.type != tcompo || tobj_compo_type(&target) != compo_trule)
    twarn(ErrRuntime_ParamsType, "evaluators::requirement",
          "Requirement Rule Term did not produce Rule");

  uint_regs argument_count = (uint_regs)item->arguments.len;
  tobj *arguments =
      argument_count ? calloc(argument_count, sizeof(*arguments)) : nullptr;
  for (uint_regs i = 0; i < argument_count; i++) {
    tobj_set_nil(&arguments[i]);
    tobj argument_term = {.type = tcompo,
                          .val.v_tcompo = item->arguments.data[i].val.v_tcompo};
    tvm_services_evaluate_rule_term(environment, &instance_value,
                                    &argument_term, &arguments[i]);
  }
  tobj_set_compo(result, (tcompo_v *)trule_bind((trule *)target.val.v_tcompo,
                                                arguments, argument_count));
  for (uint_regs i = 0; i < argument_count; i++)
    tobj_try_clear(&arguments[i]);
  free(arguments);
  tobj_try_clear(&target);
}

static void evaluators_eval(tobj *params, uint_regs count, tobj *result,
                            tcompo_env *environment) {
  if (count != 2 || params[0].type != tcompo ||
      tobj_compo_type(&params[0]) != compo_trule_instance ||
      params[1].type != tcompo ||
      params[1].val.v_tcompo->vtable != &tevaluator_vtable)
    twarn(ErrRuntime_ParamsType, "evaluators::eval",
          "RuleInstance and Evaluator required");
  tevaluator *evaluator = (tevaluator *)params[1].val.v_tcompo;
  tobj context;
  tobj_set_nil(&context);
  evaluator_context((trule_instance *)params[0].val.v_tcompo, &context);
  context.val.v_tcompo->refctr++;
  tvm_services_invoke(environment, &evaluator->evaluate, &context, 1, result);
  tobj_try_clear(&context);
  if (result->type != tcompo || tobj_compo_type(result) != compo_tdict)
    twarn(ErrRuntime_ParamsType, "evaluators::eval",
          "evaluate must return evaluators::Result");
}

typedef struct evaluator_cache_entry {
  uint64_t rule_identity;
  tstring *name;
  long version;
  tobj result;
  struct evaluator_cache_entry *next;
} evaluator_cache_entry;

static evaluator_cache_entry *evaluator_cache;

static void evaluator_cached_copy(const tobj *cached, tobj *result) {
  if (cached->type == tcompo && cached->val.v_tcompo)
    tobj_set_compo(result,
                   cached->val.v_tcompo->vtable->copy(cached->val.v_tcompo));
  else
    *result = *cached;
}

static void evaluators_compile(tobj *params, uint_regs count, tobj *result,
                               tcompo_env *environment) {
  if (count != 2 || params[0].type != tcompo ||
      tobj_compo_type(&params[0]) != compo_trule || params[1].type != tcompo ||
      params[1].val.v_tcompo->vtable != &tevaluator_vtable)
    twarn(ErrRuntime_ParamsType, "evaluators::compile",
          "Rule and Evaluator required");
  tevaluator *evaluator = (tevaluator *)params[1].val.v_tcompo;
  if (evaluator->compile.type == tnil) {
    evaluator_result(result, "Unsupported", nullptr,
                     "Evaluator does not provide compile");
    return;
  }

  trule *rule = (trule *)params[0].val.v_tcompo;
  uint64_t rule_identity = rule->identity;
  for (evaluator_cache_entry *entry = evaluator_cache; entry;
       entry = entry->next)
    if (entry->rule_identity == rule_identity &&
        entry->version == evaluator->version &&
        tstring_cmp(entry->name, evaluator->name) == 0) {
      evaluator_cached_copy(&entry->result, result);
      return;
    }

  tvm_services_invoke(environment, &evaluator->compile, &params[0], 1, result);
  if (result->type != tcompo || tobj_compo_type(result) != compo_tdict)
    twarn(ErrRuntime_ParamsType, "evaluators::compile",
          "compile must return evaluators::Result");
  tobj status;
  tobj_set_nil(&status);
  context_field(result, "status", &status);
  if (status.type != tcompo || tobj_compo_type(&status) != compo_tstr)
    twarn(ErrRuntime_ParamsType, "evaluators::compile",
          "Result.status must be String");
  if (strcmp(tstring_cstr(((tstr *)status.val.v_tcompo)->data), "Success") ==
      0) {
    tobj compiled;
    tobj_set_nil(&compiled);
    context_field(result, "value", &compiled);
    if (!callable(&compiled))
      twarn(ErrRuntime_ParamsType, "evaluators::compile",
            "successful compile must return a Function value");
    if (tobj_compo_type(&compiled) == compo_tfunc &&
        ((tfunc *)compiled.val.v_tcompo)->env.nparams !=
            rule->ir->parameters.len)
      twarn(ErrRuntime_ParamsCtr, "evaluators::compile",
            "compiled Function signature does not match Rule");
    tobj_try_clear(&compiled);
  }
  tobj_try_clear(&status);

  evaluator_cache_entry *entry = calloc(1, sizeof(*entry));
  entry->rule_identity = rule_identity;
  entry->name = tstring_dup(evaluator->name);
  entry->version = evaluator->version;
  tobj_set_nil(&entry->result);
  evaluator_cached_copy(result, &entry->result);
  entry->next = evaluator_cache;
  evaluator_cache = entry;
}

static const textension_symbol symbols[] = {
    {.name = "Evaluator",
     .type = "Type",
     .detail = "evaluators::Evaluator: Type",
     .kind = textension_type,
     .value_factory = create_evaluator_type},
    {.name = "Context",
     .type = "Dictionary",
     .detail = "evaluators::Context: Type",
     .kind = textension_type,
     .value_factory = create_dictionary_type},
    {.name = "Result",
     .type = "Dictionary",
     .detail = "evaluators::Result: Type",
     .kind = textension_type,
     .value_factory = create_dictionary_type},
    {.name = "Diagnostic",
     .type = "Dictionary",
     .detail = "evaluators::Diagnostic: Type",
     .kind = textension_type,
     .value_factory = create_dictionary_type},
    {.name = "make",
     .type = "Function[...] -> Evaluator",
     .detail = "evaluators::make(name: String, version: Int, evaluate: "
               "Function, compile: Function?) -> Evaluator",
     .kind = textension_function,
     .function = evaluators_make,
     .minimum_arguments = 3,
     .maximum_arguments = 4},
    TAPAS_NATIVE_SESSION_FUNCTION_DETAIL(
        "eval", evaluators_eval, 2, 2,
        "Function[RuleInstance, Evaluator] -> AnyType",
        "evaluators::eval(instance: RuleInstance, evaluator: Evaluator) -> "
        "Result"),
    TAPAS_NATIVE_SESSION_FUNCTION_DETAIL(
        "compile", evaluators_compile, 2, 2,
        "Function[Rule, Evaluator] -> Result",
        "evaluators::compile(rule: Rule, evaluator: Evaluator) -> Result"),
    TAPAS_NATIVE_SESSION_FUNCTION_DETAIL(
        "binding", evaluators_binding, 2, 2,
        "Function[Context, AnyType] -> AnyType",
        "evaluators::binding(context: Context, parameter: Parameter | Int) -> "
        "AnyType"),
    TAPAS_NATIVE_SESSION_FUNCTION_DETAIL(
        "capture", evaluators_capture, 2, 2,
        "Function[Context, AnyType] -> AnyType",
        "evaluators::capture(context: Context, capture: Capture | Int) -> "
        "AnyType"),
    TAPAS_NATIVE_SESSION_FUNCTION_DETAIL(
        "value", evaluators_value, 2, 2, "Function[Context, RuleTerm] -> AnyType",
        "evaluators::value(context: Context, term: RuleTerm) -> AnyType"),
    TAPAS_NATIVE_SESSION_FUNCTION_DETAIL(
        "requirement", evaluators_requirement, 2, 2,
        "Function[Context, Requirement] -> RuleInstance",
        "evaluators::requirement(context: Context, requirement: Requirement) "
        "-> RuleInstance")};

const textension_module tstdlib_evaluators_module = {
    .scope = textension_package,
    .name = "evaluators",
    .detail = "Custom Rule evaluator adapters",
    .symbols = symbols,
    .symbol_count = sizeof(symbols) / sizeof(symbols[0])};
