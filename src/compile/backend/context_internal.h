#ifndef TAPAS_COMPILE_CONTEXT_INTERNAL_H
#define TAPAS_COMPILE_CONTEXT_INTERNAL_H

#include "compile/compiler.h"
#include "runtime/tenv.h"
#include "tapas/objects/ttype.h"

typedef struct {
	uint_regs reg_ctr;
	uint_regs reg_max;
} treg_ctr;

typedef struct {
	tstring *name;
	ttypeval *type;
	tstring **field_order;
	uint_objs field_order_count;
} tcompile_export;

typedef struct {
	tcompile_export *exports;
	uint_objs count;
} tcompile_module_interface;

typedef struct {
	tstring *name;
	ttypeval *value_type;
	ttypeval *type_value;
	tstring **field_order;
	uint_objs field_order_count;
	tcompile_module_interface *module_interface;
	uint8_t has_annotation;
	uint8_t is_types_package;
	uint8_t initialized;
} tcompile_binding;

typedef struct tobj_ctr {
	tcompile_binding *bindings;
	uint_objs len;
	uint_objs capacity;
	uint_objs current_env_objmax;
	struct tobj_ctr *father;
	uint_objs npreload;
} tobj_ctr;

typedef struct {
	uint_objs slot;
	uint16_t depth;
} tobj_ctr_addr;

typedef struct {
	tstring **files;
	uint32_t count;
	uint32_t capacity;
} tcompile_import_stack;

struct tcp {
	tobj_ctr objctr;
	tobj_ctr tmpctr;
	treg_ctr regctr;
	uint_objs n_default_objs;
	tcompile_module_interface *module_interface;
	tcompile_import_stack *imports;
	const tlib *preload_library;
	uint8_t owns_imports;
	int in_loop;
	int interactive;
};

void treg_ctr_init(treg_ctr *counter);
void treg_ctr_update_max(treg_ctr *counter);
void treg_ctr_add(treg_ctr *counter);
void treg_ctr_add_n(treg_ctr *counter, uint_regs count);
void treg_ctr_ddt(treg_ctr *counter);
void treg_ctr_ddt_n(treg_ctr *counter, uint_regs count);
uint_regs treg_ctr_get(const treg_ctr *counter);
uint_regs treg_ctr_get_max(const treg_ctr *counter);

void tobj_ctr_init(tobj_ctr *counter, tobj_ctr *parent);
void tobj_ctr_init_preload(tobj_ctr *counter, tstring **names,
			   uint_objs count, tobj_ctr *parent);
void tobj_ctr_free(tobj_ctr *counter);
void tobj_ctr_update_obj_max(tobj_ctr *counter);
uint_objs tobj_ctr_obj_len_all(tobj_ctr *counter);
uint_objs tobj_ctr_obj_len_cur(tobj_ctr *counter);
uint_objs tobj_ctr_obj_max_cur(const tobj_ctr *counter);
uint_objs tobj_ctr_obj_loc(tobj_ctr *counter, const tstring *name);
int tobj_ctr_obj_addr(tobj_ctr *counter, const tstring *name,
			      tobj_ctr_addr *address);
uint_objs tobj_ctr_obj_create(tobj_ctr *counter, const tstring *name,
			      int in_block, tconsts *constants,
			      uint_csts *constant_count);
void tobj_ctr_obj_del_last_n(tobj_ctr *counter, uint_objs count);
int tobj_ctr_is_preload(const tobj_ctr *counter, uint_objs slot);
tstring **tobj_ctr_first_n_objs(tobj_ctr *counter, uint_objs count);
void tobj_ctr_first_n_objs_free(tstring **names, uint_objs count);

void tcp_init_preload(tcp *cp, tstring **default_objects,
		      uint_objs default_count, tobj_ctr *parent,
		      const tlib *library,
		      int interactive);
void tcp_share_import_stack(tcp *child, tcp *parent);
tcinfo tcp_get_compile_info(tcp *cp);
void tcp_free(tcp *cp);

#endif
