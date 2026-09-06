/** Internal compiler implementation. */
#include "internal.h"
#include "tapas/dsa/tstring.h"

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

/*===========================================================================*
 * 1. Register Counter
 *===========================================================================*/

void treg_ctr_init(treg_ctr *c)
{
	c->reg_ctr = 0;
	c->reg_max = 0;
}

void treg_ctr_update_max(treg_ctr *c)
{
	if (c->reg_max < c->reg_ctr)
		c->reg_max = c->reg_ctr;
}

void treg_ctr_add(treg_ctr *c)
{
	if (c->reg_ctr + 1 > REG_LIMIT)
		twarn(ErrCompile_REGOutOfLimit, "treg_ctr_add", "");
	c->reg_ctr++;
	treg_ctr_update_max(c);
}

void treg_ctr_add_n(treg_ctr *c, uint_regs n)
{
	if (c->reg_ctr + n > REG_LIMIT)
		twarn(ErrCompile_REGOutOfLimit, "treg_ctr_add_n", "");
	c->reg_ctr += n;
	treg_ctr_update_max(c);
}

void treg_ctr_ddt(treg_ctr *c)
{
	if (c->reg_ctr == 0)
		twarn(ErrCompile_REGOutOfLimit, "treg_ctr_ddt", "");
	c->reg_ctr--;
}

void treg_ctr_ddt_n(treg_ctr *c, uint_regs n)
{
	if (c->reg_ctr < n)
		twarn(ErrCompile_REGOutOfLimit, "treg_ctr_ddt_n", "");
	c->reg_ctr -= n;
}

uint_regs treg_ctr_get(const treg_ctr *c)
{
	return c->reg_ctr;
}

uint_regs treg_ctr_get_max(const treg_ctr *c)
{
	return c->reg_max;
}


/*===========================================================================*
 * 2. Object Counter — uses tstring
 *===========================================================================*/

void tobj_ctr_init(tobj_ctr *c, tobj_ctr *father)
{
	c->bindings = nullptr;
	c->len = 0;
	c->capacity = 0;
	c->current_env_objmax = 0;
	c->father = father;
	c->npreload = 0;
}

static void tobj_ctr_reserve(tobj_ctr *c, uint_objs required)
{
	if (required <= c->capacity)
		return;
	uint_objs capacity = c->capacity ? c->capacity : 16;
	while (capacity < required)
		capacity *= 2;
	tcompile_binding *bindings = (tcompile_binding *)realloc(
		c->bindings, capacity * sizeof(tcompile_binding));
	if (!bindings)
		twarn(ErrCompile_Other, "tobj_ctr_reserve", "out of memory");
	c->bindings = bindings;
	c->capacity = capacity;
}

static void tcompile_binding_clear(tcompile_binding *binding)
{
	tstring_free(binding->name);
	ttypeval_release(binding->value_type);
	ttypeval_release(binding->type_value);
	for (uint_objs i = 0; i < binding->field_order_count; i++)
		tstring_free(binding->field_order[i]);
	free(binding->field_order);
	tcompile_module_interface_free(binding->module_interface);
	binding->name = nullptr;
	binding->value_type = nullptr;
	binding->type_value = nullptr;
	binding->field_order = nullptr;
	binding->field_order_count = 0;
	binding->module_interface = nullptr;
	binding->has_annotation = 0;
	binding->is_types_package = 0;
	binding->initialized = 0;
}

void tobj_ctr_init_preload(
		tobj_ctr *c, tstring **precludes, uint_objs npre, tobj_ctr *father)
{
	tobj_ctr_init(c, father);
	c->npreload = npre;
	tobj_ctr_reserve(c, npre);
	for (uint_objs i = 0; i < npre; i++) {
		c->bindings[i] = (tcompile_binding){
			.name = tstring_dup(precludes[i]),
			.initialized = 1,
		};
	}
	c->len = npre;
	c->current_env_objmax = c->len;

	if (c->len + tobj_ctr_obj_len_all(c->father) >= OBJ_LIMIT)
		twarn(ErrCompile_OBJOutOfLimit, "tobj_ctr_init_preload", "");
}

void tobj_ctr_free(tobj_ctr *c)
{
	if (!c)
		return;
	for (uint_objs i = 0; i < c->len; i++)
		tcompile_binding_clear(&c->bindings[i]);
	free(c->bindings);
	c->bindings = nullptr;
	c->len = 0;
	c->capacity = 0;
	c->current_env_objmax = 0;
}

void tobj_ctr_update_obj_max(tobj_ctr *c)
{
	if (c->current_env_objmax < c->len)
		c->current_env_objmax = c->len;
}

uint_objs tobj_ctr_obj_len_all(tobj_ctr *c)
{
	if (!c)
		return 0;
	return c->len + tobj_ctr_obj_len_all(c->father);
}

uint_objs tobj_ctr_obj_len_cur(tobj_ctr *c)
{
	return c ? c->len : 0;
}

uint_objs tobj_ctr_obj_max_cur(const tobj_ctr *c)
{
	return c ? c->current_env_objmax : 0;
}

uint_objs tobj_ctr_obj_loc(tobj_ctr *c, const tstring *objname)
{
	if (!c || !objname)
		return OBJ_LIMIT;
	for (uint_objs i = 0; i < c->len; i++) {
		if (tstring_eq(c->bindings[i].name, objname))
			return i;
	}
	if (c->father)
		return c->len + tobj_ctr_obj_loc(c->father, objname);
	return c->len;
}

int tobj_ctr_obj_addr(tobj_ctr *c, const tstring *objname, tobj_ctr_addr *addr)
{
	uint16_t depth = 0;
	for (tobj_ctr *cur = c; cur; cur = cur->father, depth++) {
		for (uint_objs i = 0; i < cur->len; i++) {
			if (tstring_eq(cur->bindings[i].name, objname)) {
				addr->slot = i;
				addr->depth = depth;
				return 1;
			}
		}
	}
	return 0;
}

uint_objs tobj_ctr_obj_create(
		tobj_ctr *c, const tstring *left, int inblk,
		tconsts *consts, uint_csts *nameloc)
{
	uint_objs loc = tobj_ctr_obj_loc(c, left);
	uint_objs len_all = tobj_ctr_obj_len_all(c);

	if (len_all + 1 >= OBJ_LIMIT)
		twarn(ErrCompile_OBJOutOfLimit, "tobj_ctr_obj_create", "");
	if (loc < c->len)
		twarn(ErrCompile_DblVDeclare, "tobj_ctr_obj_create", "");
	if (inblk)
		twarn(ErrCompile_InBlkVarDef, "tobj_ctr_obj_create", "");

	*nameloc = tconsts_add_str_const(consts, tstring_cstr(left));
	tobj_ctr_reserve(c, c->len + 1);
	c->bindings[c->len] = (tcompile_binding){
		.name = tstring_dup(left),
	};
	c->len++;
	tobj_ctr_update_obj_max(c);
	return c->len - 1;
}

void tobj_ctr_obj_del_last_n(tobj_ctr *c, uint_objs n)
{
	while (n > 0 && c->len > c->npreload) {
		c->len--;
		tcompile_binding_clear(&c->bindings[c->len]);
		n--;
	}
}

int tobj_ctr_is_preload(const tobj_ctr *c, uint_objs loc)
{
	if (!c)
		return 0;
	if (loc < c->len)
		return loc < c->npreload;
	if (!c->father)
		return 0;
	return tobj_ctr_is_preload(c->father, loc - c->len);
}

/** Get first n objects */
tstring **tobj_ctr_first_n_objs(tobj_ctr *c, uint_objs n)
{
	if (!n)
		return nullptr;
	if (!c || n > c->len)
		abort();
	tstring **result = (tstring **)malloc(n * sizeof(*result));
	if (!result)
		abort();
	for (uint_objs i = 0; i < n; i++)
		result[i] = tstring_dup(c->bindings[i].name);
	return result;
}

void tobj_ctr_first_n_objs_free(tstring **objs, uint_objs n)
{
	uint_objs i;
	if (!objs)
		return;
	for (i = 0; i < n; i++)
		tstring_free(objs[i]);
	free(objs);
}


/*===========================================================================*
 * 4. Compiler
 *===========================================================================*/

static tcompile_import_stack *tcompile_import_stack_new(void)
{
	tcompile_import_stack *stack =
		(tcompile_import_stack *)calloc(1, sizeof(*stack));
	if (!stack)
		abort();
	return stack;
}

static void tcompile_import_stack_free(tcompile_import_stack *stack)
{
	if (!stack)
		return;
	for (uint32_t i = 0; i < stack->count; i++)
		tstring_free(stack->files[i]);
	free(stack->files);
	free(stack);
}

void tcp_init_preload(
		tcp *cp,
		tstring **default_objs,
		uint_objs ndefault,
		tobj_ctr *father_objctr,
		const tlib *library,
		int interactive)
{
	tobj_ctr_init_preload(
		&cp->objctr, default_objs, ndefault, father_objctr);
	tobj_ctr_init(&cp->tmpctr, nullptr);
	treg_ctr_init(&cp->regctr);
	cp->n_default_objs = ndefault;
	cp->module_interface = nullptr;
	cp->imports = tcompile_import_stack_new();
	cp->preload_library = library;
	cp->owns_imports = 1;
	cp->in_loop = 0;
	cp->interactive = interactive;
}

tcp *tcp_new_preload(tstring **default_objects, uint_objs default_count,
		     int interactive)
{
	tcp *cp = (tcp *)malloc(sizeof(*cp));
	if (!cp)
		abort();
	tcp_init_preload(cp, default_objects, default_count, nullptr, nullptr, interactive);
	return cp;
}

tcp *tcp_new_library(const tlib *library, int interactive)
{
	if (!library) return tcp_new_preload(nullptr, 0, interactive);
	tcp *cp = (tcp *)malloc(sizeof(*cp));
	if (!cp) abort();
	tcp_init_preload(cp, tlib_get_default_v_names((tlib *)library),
		tlib_get_ndefault((tlib *)library), nullptr, library, interactive);
	return cp;
}

void tcp_share_import_stack(tcp *child, tcp *parent)
{
	if (!child || !parent)
		return;
	if (child->owns_imports)
		tcompile_import_stack_free(child->imports);
	child->imports = parent->imports;
	child->owns_imports = 0;
}

tcinfo tcp_get_compile_info(tcp *cp)
{
	tcinfo info;
	info.obj_max = tobj_ctr_obj_max_cur(&cp->objctr);
	info.tmp_max = tobj_ctr_obj_max_cur(&cp->tmpctr);
	info.reg_max = treg_ctr_get_max(&cp->regctr);
	info.padding_1 = 0;
	return info;
}

void tcp_free(tcp *cp)
{
	if (!cp)
		return;
	tobj_ctr_free(&cp->objctr);
	tobj_ctr_free(&cp->tmpctr);
	tcompile_module_interface_free(cp->module_interface);
	cp->module_interface = nullptr;
	if (cp->owns_imports)
		tcompile_import_stack_free(cp->imports);
	cp->imports = nullptr;
	cp->owns_imports = 0;
}

void tcp_delete(tcp *cp)
{
	if (!cp)
		return;
	tcp_free(cp);
	free(cp);
}
