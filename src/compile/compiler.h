#ifndef TAPAS_COMPILE_COMPILER_H
#define TAPAS_COMPILE_COMPILER_H

#include "tapas/basic_defs/tbycs.h"

#ifdef __cplusplus
extern "C" {
#endif


typedef struct tcp tcp;
typedef struct tlib tlib;


tcp *tcp_new_preload(tstring **default_objects, uint_objs default_count,
		     int interactive);

tcp *tcp_new_library(const tlib *library, int interactive);

void tcp_delete(tcp *cp);

/* Compile multiple statements while retaining bindings for the next call. */
tcinfo parse_sequence(tcp *cp, const tstring *source,
                      tvmcmd_vect *instructions, tconsts *constants,
                      tstring **paths, uint_lexs path_count);

tcinfo parse_unit(tcp *cp, const tstring *source,
		  tvmcmd_vect *instructions, tconsts *constants,
		  tstring **paths, uint_lexs path_count,
		  int clean_stack, int in_block);

tcinfo parse_blk(tcp *cp, const tstring *source,
		 tvmcmd_vect *instructions, tconsts *constants,
		 tstring **paths, uint_lexs path_count,
		 int clean_stack, int in_block);

twrapper *compile_str(tcp *cp, const tstring *source,
		      tstring **paths, uint_lexs path_count);

twrapper *compile_file(tcp *cp, const tstring *file,
		       tstring **paths, uint_lexs path_count);

void compile_file_save(tcp *cp, const tstring *file,
		       tstring **paths, uint_lexs path_count);

#ifdef __cplusplus
}
#endif

#endif
