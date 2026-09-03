#ifndef TAPAS_TAPAS_H
#define TAPAS_TAPAS_H

#include "tapas/tvm.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================*
 * 2. Session Management (tsession)
 *===========================================================================*/

typedef struct tsession {
	tlib *lib;
} tsession;

tsession *tsession_new(void);
void tsession_free(tsession *sess);
tlib *tsession_get_lib(tsession *sess);

/** Compile a .tap file to .tapc */
void tsession_compile_file(tsession *sess, const char *file, int interactive);

/** Evaluate compiled .tapc binary */
void tsession_eval_bycodes(tsession *sess, const char *file);

/** Compile & execute a .tap file without saving .tapc */
void tsession_execute_file(tsession *sess, const char *file, int interactive);

int tsession_execute_module(tsession *sess, const char *module,
			    int argument_count, const char *const *arguments);

/** Execute markdown and update Tapas return blocks in place */
void tsession_execute_markdown_update(tsession *sess, const char *file, int interactive);

/** Compile & evaluate a string */
void tsession_execute_str(tsession *sess, const char *str, int interactive);

/** Show bycodes of compiled .tapc file */
void tsession_show_bycodes(tsession *sess, const char *file);

/** Add a file-searching path */
void tsession_add_path(tsession *sess, const char *path);

/** Add a package (dict) to the session */
tdict *tsession_add_pkg(tsession *sess, const char *pkgname);

/** Get a Tap object from the session's root library */
tobj *tsession_get_obj(tsession *sess, uint_objs loc);

#ifdef __cplusplus
}
#endif

#endif /* TAPAS_TAPAS_H */
