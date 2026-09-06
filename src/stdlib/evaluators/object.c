/**
 * @file object.c
 * @brief Implements the evaluators package's native Evaluator object.
 * @details Evaluator uses a qualified package Type identity and the runtime's
 * generic extension-object code.
 * @note Evaluator is a standard-library Type, not a language builtin.
 */
#include "object.h"
#include "tapas/dsa/tstring.h"

#include "tapas/objects/ttype.h"

#include <stdlib.h>

static const char *evaluator_type(void) { return "evaluators::Evaluator"; }
static tcompo_type evaluator_code(void) { return compo_extension; }
static long evaluator_len(void *self) { (void)self; return 0; }
static int evaluator_identical(void *self, void *other) { return self == other; }

ttypeval *tstdlib_evaluator_type(void)
{
	return ttypeval_new_named("evaluators::Evaluator");
}

static void *evaluator_copy(void *self)
{
	tevaluator *value = (tevaluator *)self;
	return tevaluator_new(tstring_cstr(value->name),
		value->version, &value->evaluate, &value->compile);
}

static void evaluator_free(void *self)
{
	tevaluator *value = (tevaluator *)self;
	tstring_free(value->name);
	tobj_try_clear(&value->evaluate);
	tobj_try_clear(&value->compile);
	free(value);
}

static tstring *evaluator_string(void *self)
{
	tevaluator *value = (tevaluator *)self;
	tstring *text = tstring_new("Evaluator[");
	tstring_append_ts(text, value->name);
	tstring_append_fmt(text, "@%ld", value->version);
	tstring_append_c(text, ']');
	return text;
}

tcompo_vtable tevaluator_vtable = {
	.get_type = evaluator_type,
	.get_compo_type_code = evaluator_code,
	.len = evaluator_len,
	.copy = evaluator_copy,
	.free = evaluator_free,
	.identical = evaluator_identical,
	.tostring_abbr = evaluator_string,
	.tostring_full = evaluator_string
};

tevaluator *tevaluator_new(const char *name, long version,
			   const tobj *evaluate, const tobj *compile)
{
	tevaluator *value = (tevaluator *)calloc(1, sizeof(*value));
	value->base.vtable = &tevaluator_vtable;
	value->name = tstring_new(name ? name : "");
	value->version = version;
	tobj_set_nil(&value->evaluate);
	tobj_set_nil(&value->compile);
	if (evaluate) tobj_copy(&value->evaluate, evaluate);
	if (compile) tobj_copy(&value->compile, compile);
	return value;
}
