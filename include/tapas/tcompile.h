#ifndef T_COMPILE_H
#define T_COMPILE_H

#include "tlex.h"

#ifdef __cplusplus
extern "C" {
#endif


/*===========================================================================*
 * 1. Register Counter
 *===========================================================================*/

typedef struct {
	uint_regs reg_ctr;
	uint_regs reg_max;
} treg_ctr;

void treg_ctr_init(treg_ctr *c);
void treg_ctr_update_max(treg_ctr *c);
void treg_ctr_add(treg_ctr *c);
void treg_ctr_add_n(treg_ctr *c, uint_regs n);
void treg_ctr_ddt(treg_ctr *c);
void treg_ctr_ddt_n(treg_ctr *c, uint_regs n);
uint_regs treg_ctr_get(const treg_ctr *c);
uint_regs treg_ctr_get_max(const treg_ctr *c);


/*===========================================================================*
 * 2. Object Counter — uses tstring
 *===========================================================================*/

typedef struct tobj_ctr {
	tstring          **objs;
	uint_objs         len;
	uint_objs         capacity;
	uint_objs         current_env_objmax;
	struct tobj_ctr  *father;
	uint_objs         npreload;
} tobj_ctr;

typedef struct {
	uint_objs slot;
	uint16_t depth;
} tobj_ctr_addr;

void tobj_ctr_init(tobj_ctr *c, tobj_ctr *father);
void tobj_ctr_init_preload(tobj_ctr *c, tstring **precludes, uint_objs npre, tobj_ctr *father);
void tobj_ctr_free(tobj_ctr *c);
void tobj_ctr_update_obj_max(tobj_ctr *c);
uint_objs tobj_ctr_len_in_all(tobj_ctr *father);
uint_objs tobj_ctr_obj_len_all(tobj_ctr *c);
uint_objs tobj_ctr_obj_len_cur(tobj_ctr *c);
uint_objs tobj_ctr_obj_max_cur(const tobj_ctr *c);
uint_objs tobj_ctr_obj_loc(tobj_ctr *c, const tstring *objname);
int tobj_ctr_obj_addr(tobj_ctr *c, const tstring *objname, tobj_ctr_addr *addr);
uint_objs tobj_ctr_obj_create(tobj_ctr *c, const tstring *left, int inblk, tconsts *consts, uint_csts *nl);
void tobj_ctr_obj_del_last_n(tobj_ctr *c, uint_objs n);
int tobj_ctr_is_preload(const tobj_ctr *c, uint_objs loc);
tstring **tobj_ctr_first_n_objs(tobj_ctr *c, uint_objs n);
void tobj_ctr_first_n_objs_free(tstring **objs, uint_objs n);


/*===========================================================================*
 * 3. Binary Expression — uses tstring
 *===========================================================================*/

typedef struct {
	tstring *left;
	tstring *right;
	tstring *optr;
	uint8_t al_type;
	uint_objs lloc;
	uint_objs rloc;
} tbin_expr;


/*===========================================================================*
 * 4. Compiler Context (tcp)
 *===========================================================================*/

struct tcp {
	tobj_ctr objctr;
	tobj_ctr tmpctr;
	tunit_ctr lexctr;
	treg_ctr regctr;
	uint_objs n_default_objs;
	tvmcmd_vect *outer_instructions;
	tvmcmd_vect *cur_instructions;
	tconsts *outer_consts;
	tconsts *cur_consts;
	tcompo_env *outer_env;
	tcompo_env *cur_env;
	int in_loop;
	int interactive;
};

typedef struct tcp tcp;

void tcp_init(tcp *cp, tobj_ctr *father_objctr, int interactive);
void tcp_init_preload(tcp *cp, tstring **default_objs, uint_objs ndefault, tobj_ctr *father_objctr, int interactive);
tcinfo tcp_get_compile_info(tcp *cp);
void tcp_free(tcp *cp);


/*===========================================================================*
 * 5. Parsing Functions (public API)
 *===========================================================================*/

int find_imported_file(tstring **file_ptr, tstring **paths, uint_lexs npaths);
int str_to_long_int(const tstring *cmds, long *it);
int str_to_float(const tstring *cmds, double *dt);

tcinfo parse_unit(tcp *cp, const tstring *src, tvmcmd_vect *tcmds, tconsts *consts, tstring **paths, uint_lexs npaths, int cleanstk, int inblk);
uint_regs parse_params(tcp *cp, const tstring *src, tvmcmd_vect *tcmds, tconsts *consts, tstring **paths, uint_lexs npaths, int inblk);
tcinfo parse_file(tcp *cp, FILE *f, tvmcmd_vect *tcmds, tconsts *consts, tstring **paths, uint_lexs npaths);
tcinfo parse_blk(tcp *cp, const tstring *src, tvmcmd_vect *tcmds, tconsts *consts, tstring **paths, uint_lexs npaths, int cleanstk, int inblk);


/*===========================================================================*
 * 6. High-level Compile Functions
 *===========================================================================*/

twrapper *compile_str(tcp *cp, const tstring *src, tstring **paths, uint_lexs npaths);
twrapper *compile_file(tcp *cp, const tstring *file, tstring **paths, uint_lexs npaths);
void compile_file_save(tcp *cp, const tstring *file, tstring **paths, uint_lexs npaths);

#ifdef __cplusplus
}
#endif

#endif /* T_COMPILE_H */
