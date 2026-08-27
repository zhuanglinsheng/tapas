/** Internal compiler implementation. */
#include "internal.h"

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

static ttypeval *ttype_retain_result(ttypeval *type)
{
	ttypeval_retain(type);
	return type;
}

static ttypeval *compile_builtin_type_name(const char *name)
{
	static const char *const names[ttype_builtin_count] = {
		"AnyType", "Nil", "Bool", "Int", "Float", "String",
		"List", "Pair", "Dictionary", "Iterator", "Function",
		"Library", "RealArray", "BoolArray", "Time", "Type"
	};
	for (int i = 0; i < ttype_builtin_count; i++) {
		if (strcmp(name, names[i]) == 0)
			return ttypeval_builtin((ttype_builtin)i);
	}
	return NULL;
}

/* Returns an owned compiler reference, or NULL when expr is not static Type. */
static ttypeval *compile_static_type_expr(tcp *cp, const tstring *expr);

static ttypeval *compile_static_type_call(tcp *cp, const ttoken *tok)
{
	const char *callee = tstring_cstr(tok->val1);
	const char *name = NULL;
	if (strncmp(callee, "types::", 7) == 0)
		name = callee + 7;
	else
		return NULL;

	tstring **params = NULL;
	uint_regs count = 0;
	split_params_ts(tok->val2, &params, &count);
	ttypeval *result = NULL;

	if (strcmp(name, "make_type") == 0) {
		if (count == 0)
			goto done;
		ttype_field *fields = (ttype_field *)calloc(count, sizeof(ttype_field));
		int valid = fields != NULL;
		for (uint_regs i = 0; valid && i < count; i++) {
			ttoken *pair_tokens = NULL;
			uint32_t pair_count = 0;
			get_tokens_ts(params[i], &pair_tokens, &pair_count);
			if (pair_count != 1 || pair_tokens[0].type != token_pair) {
				valid = 0;
				get_tokens_free(pair_tokens, pair_count);
				break;
			}
			ttoken *name_tokens = NULL;
			uint32_t name_count = 0;
			get_tokens_ts(pair_tokens[0].val1, &name_tokens, &name_count);
			if (name_count != 1 ||
			    (name_tokens[0].type != token_sstr &&
			     name_tokens[0].type != token_dstr)) {
				valid = 0;
			} else {
				fields[i].name = tstring_dup(name_tokens[0].val1);
				fields[i].type =
					compile_static_type_expr(cp, pair_tokens[0].val2);
				if (!fields[i].type)
					valid = 0;
			}
			get_tokens_free(name_tokens, name_count);
			get_tokens_free(pair_tokens, pair_count);
		}
		if (valid)
			result = ttypeval_new_fields(fields, count);
		for (uint_regs i = 0; i < count; i++) {
			tstring_free((tstring *)fields[i].name);
			ttypeval_release(fields[i].type);
		}
		free(fields);
	} else if (strcmp(name, "union") == 0) {
		if (count < 2)
			goto done;
		ttypeval **members = (ttypeval **)calloc(count, sizeof(ttypeval *));
		int valid = members != NULL;
		for (uint_regs i = 0; valid && i < count; i++) {
			members[i] = compile_static_type_expr(cp, params[i]);
			if (!members[i])
				valid = 0;
		}
		if (valid)
			result = ttypeval_new_union(members, count);
		for (uint_regs i = 0; i < count; i++)
			ttypeval_release(members ? members[i] : NULL);
		free(members);
	} else if (strcmp(name, "list") == 0 && count == 1) {
		ttypeval *item = compile_static_type_expr(cp, params[0]);
		if (item)
			result = ttypeval_new_list(item);
		ttypeval_release(item);
	} else if (strcmp(name, "pair") == 0 && count == 2) {
		ttypeval *first = compile_static_type_expr(cp, params[0]);
		ttypeval *second = compile_static_type_expr(cp, params[1]);
		if (first && second)
			result = ttypeval_new_pair(first, second);
		ttypeval_release(first);
		ttypeval_release(second);
	} else if (strcmp(name, "dictionary") == 0 && count == 2) {
		ttypeval *key = compile_static_type_expr(cp, params[0]);
		ttypeval *value = compile_static_type_expr(cp, params[1]);
		if (key && value)
			result = ttypeval_new_dictionary(key, value);
		ttypeval_release(key);
		ttypeval_release(value);
	}

done:
	split_params_free(params, count);
	return result ? ttype_retain_result(result) : NULL;
}

static ttypeval *compile_static_type_expr(tcp *cp, const tstring *expr)
{
	tstring *stripped = tlex_strip_outer_parentheses_ts(expr);
	ttoken *tokens = NULL;
	uint32_t count = 0;
	get_tokens_ts(stripped, &tokens, &count);
	ttypeval *result = NULL;
	if (count == 1 && tokens[0].type == token_idx2 &&
	    strcmp(tstring_cstr(tokens[0].val1), "types") == 0) {
		result = compile_builtin_type_name(tstring_cstr(tokens[0].val2));
		if (result)
			ttypeval_retain(result);
	} else if (count == 1 && tokens[0].type == token_v) {
		tobj_ctr *owner = NULL;
		uint_objs slot = 0;
		if (tcompile_find_binding(&cp->tmpctr, tokens[0].val1,
					  &owner, &slot) && owner->bindings[slot].type_value)
			result = ttype_retain_result(owner->bindings[slot].type_value);
		else if (tcompile_find_binding(&cp->objctr, tokens[0].val1,
					       &owner, &slot) && owner->bindings[slot].type_value)
			result = ttype_retain_result(owner->bindings[slot].type_value);
	} else if (count == 1 && tokens[0].type == token_eval) {
		result = compile_static_type_call(cp, &tokens[0]);
	}
	get_tokens_free(tokens, count);
	tstring_free(stripped);
	return result;
}

ttypeval *compile_resolve_annotation(tcp *cp, const tstring *annotation)
{
	if (tstring_empty(annotation))
		return NULL;
	ttoken *tokens = NULL;
	uint32_t count = 0;
	get_tokens_ts(annotation, &tokens, &count);
	ttypeval *result = NULL;
	if (count == 1 && tokens[0].type == token_idx2 &&
	    strcmp(tstring_cstr(tokens[0].val1), "types") == 0) {
		result = compile_builtin_type_name(tstring_cstr(tokens[0].val2));
	} else if (count == 1 && tokens[0].type == token_v) {
		tobj_ctr *owner = NULL;
		uint_objs slot = 0;
		int found = tcompile_find_binding(&cp->tmpctr, tokens[0].val1,
						&owner, &slot);
		if (!found)
			found = tcompile_find_binding(&cp->objctr, tokens[0].val1,
							&owner, &slot);
		if (found)
			result = owner->bindings[slot].type_value;
		else
			result = compile_builtin_type_name(
				tstring_cstr(tokens[0].val1));
	}
	if (result)
		ttypeval_retain(result);
	get_tokens_free(tokens, count);
	if (!result)
		twarn(ErrCompile_Other, "type annotation",
		      tstring_cstr(annotation));
	return result;
}

ttypeval *compile_infer_expr_type(tcp *cp, const tstring *expr,
					 ttypeval **static_value)
{
	ttypeval *resolved_static = compile_static_type_expr(cp, expr);
	if (static_value)
		*static_value = resolved_static;
	if (resolved_static) {
		if (!static_value)
			ttypeval_release(resolved_static);
		return ttype_retain_result(ttypeval_builtin(ttype_builtin_type));
	}

	tstring *stripped = tlex_strip_outer_parentheses_ts(expr);
	ttoken *tokens = NULL;
	uint32_t count = 0;
	get_tokens_ts(stripped, &tokens, &count);
	ttypeval *result = NULL;
	if (count == 1) {
		switch (tokens[0].type) {
		case token_true:
		case token_false:
			result = ttypeval_builtin(ttype_builtin_bool);
			break;
		case token_sstr:
		case token_dstr:
			result = ttypeval_builtin(ttype_builtin_string);
			break;
		case token_func:
		case token_kappa:
			result = ttypeval_builtin(ttype_builtin_function);
			break;
		case token_v: {
			long integer;
			double floating;
			if (str_to_long_int(tokens[0].val1, &integer))
				result = ttypeval_builtin(ttype_builtin_int);
			else if (str_to_float(tokens[0].val1, &floating))
				result = ttypeval_builtin(ttype_builtin_float);
			else {
				tobj_ctr *owner = NULL;
				uint_objs slot = 0;
				if (tcompile_find_binding(&cp->tmpctr, tokens[0].val1,
							  &owner, &slot) ||
				    tcompile_find_binding(&cp->objctr, tokens[0].val1,
							  &owner, &slot))
					result = owner->bindings[slot].value_type;
			}
		} break;
		case token_pair: {
			ttypeval *first = compile_infer_expr_type(cp, tokens[0].val1, NULL);
			ttypeval *second = compile_infer_expr_type(cp, tokens[0].val2, NULL);
			if (first && second)
				result = ttypeval_new_pair(first, second);
			ttypeval_release(first);
			ttypeval_release(second);
		} break;
		default:
			break;
		}
	}
	if (result)
		ttypeval_retain(result);
	get_tokens_free(tokens, count);
	tstring_free(stripped);
	return result;
}

/** Find imported file in paths */
