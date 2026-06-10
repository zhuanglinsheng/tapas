#include "tapas/tapas.h"

#include <readline/history.h>
#include <readline/readline.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/// @return a boolean of continuing the program or not
static int exec_cmd_seq(
		tsession *sess,
		tcp *syner,
		tvm *vm,
		const char *cmd,
		tvmcmd_vect *tcmds,
		tconsts *consts,
		tcinfo *info)
{
	if (setjmp(tapas_error_jmpbuf) != 0) {
		tapas_error_recover_enabled = 0;
		tvm_clean(vm);
		tvm_set_vmstack(vm, NULL, 0);
		return 1;
	}
	tapas_error_recover_enabled = 1;

	/* Shell Commands: for debugging */
	if (strcmp(cmd, "exit()") == 0) {
		tapas_error_recover_enabled = 0;
		return 0;
	}
	/* Try to compile code block */
	tstring **paths = tlib_get_paths(sess->lib);
	uint_lexs npaths = tlib_get_npaths(sess->lib);
	uint_cmds ncmd_old = tvmcmd_vect_size32(tcmds);

	/* Parse unit - compile into bytecodes */
	tstring *cmd_ts = tstring_new(cmd);
	terror_set_source_context(cmd);
	*info = parse_unit(syner, cmd_ts, tcmds, consts, paths, npaths, 1, 0);
	tstring_free(cmd_ts);

	/* Try to execute the compiled code block */
	twrapper* wrapper = tanalyser_wrap(tcmds, consts, info);
	if (wrapper) {
		tlib_set_wrapper(sess->lib, wrapper);
		tvm_set_tmpmax(vm, info->tmp_max);
		tvm_set_vmstack(vm,
					tcompo_env_get_vmstack(&sess->lib->env),
					info->reg_max);
		eval_bycodes(vm, ncmd_old, sess->lib);
		tvm_clean(vm);
		tvm_set_vmstack(vm, NULL, 0);
	}
	tapas_error_recover_enabled = 0;
	return 1;
}

static void exec_interact(tsession *sess, const char *p_i1)
{
	printf("Result:\n");
	tsession_execute_str(sess, p_i1, 1);
}

static int is_markdown_file(const char *file)
{
	size_t len = strlen(file);
	return (len >= 3 &&
		(strcmp(file + len - 3, ".md") == 0 ||
		 strcmp(file + len - 3, ".Md") == 0 ||
		 strcmp(file + len - 3, ".MD") == 0));
}

static void print_usage(void)
{
	printf("Usage: tap [OPTION] [FILE/CMD]\n");
	printf("\n");
	printf("Example: tap -ce example.tap\n");
	printf("If no OPTION or FILE/CMD is given, tap enters interactive mode.\n");
	printf("If only FILE is given but no OPTION, tap executes the file directly.\n");
	printf("\n");
	printf("OPTION without FILE/CMD followed:\n");
	printf("  -h                get manuals of the software\n");
	printf("  -v                version of the software\n");
	printf("\n");
	printf("OPTION with FILE followed:\n");
	printf("  -c                compile source file (.tap/.md) and get binary file (.tapc)\n");
	printf("  -e                execute binary file (.tapc)\n");
	printf("  -r                display binary file (.tapc)\n");
	printf("  -ce               combination of -c and -e\n");
	printf("  -cr               combination of -c and -r\n");
	printf("  --stdout          execute markdown output on stdout instead of updating the file\n");
	printf("\n");
	printf("OPTION with PATH followed:\n");
	printf("  -p                add a module search path\n");
	printf("\n");
	printf("OPTION with CMD followed:\n");
	printf("  -i                execute the CMD\n");
	printf("\n");
	printf("Report bugs\n");
	printf("Contact: <zhuanglinsheng@outlook.com>\n");
}

static void cope_with_stdin(tsession *sess)
{
	printf("tap-script (%s) Copyright (C) %s %s.\n", Tap_Version, Tap_Year, Tap_Author);
	printf("MIT License: <https://opensource.org/licenses/MIT>\n");

	/* Tap lexer */
	tunit_ctr uint_ctr;
	tunit_ctr_init(&uint_ctr);

	/* Tap compiler & VM */
	tcp syner;
	tstring **default_names = tlib_get_default_v_names(sess->lib);
	uint_objs ndef = tlib_get_ndefault(sess->lib);
	tcp_init_preload(&syner, default_names, ndef, NULL, 1);
	tvmcmd_vect cmds;
	tvmcmd_vect_init(&cmds);
	tconsts consts;
	tconsts_init(&consts);
	tcinfo info;
	memset(&info, 0, sizeof(info));
	tvm vm_storage;
	tvm_init(&vm_storage, 0);
	tvm* vm = &vm_storage;

	/* UI */
	int in_dpd = 1;
	char *fullcmd = NULL;
	size_t full_len = 0;
	rl_bind_key('\t', rl_complete);

	while (1) {
		char *line = readline(in_dpd ? ">> " : ".. ");
		if (!line)
			break;

		if (line[0] != '\0')
			add_history(line);

		/* Append line */
		size_t len = strlen(line);
		char *grown = (char *)realloc(fullcmd, full_len + len + 2);
		if (!grown) {
			free(line);
			free(fullcmd);
			twarn(ErrSession_IO, "cope_with_stdin", "out of memory");
		}
		fullcmd = grown;
		memcpy(fullcmd + full_len, line, len);
		full_len += len;
		fullcmd[full_len++] = '\n';
		fullcmd[full_len] = '\0';

		/* Scan line */
		size_t j;
		for (j = 0; j < len; j++)
			tunit_ctr_update(&uint_ctr, line[j]);
		in_dpd = tunit_ctr_independent(&uint_ctr);

		/* Execute line */
		if (in_dpd) {
			int keep_running = 1;
			tstring *blk = tstring_new(fullcmd);
			tstring_trim(blk);
			if (tstring_empty(blk))
				goto END_LABEL;
			keep_running = exec_cmd_seq(sess, &syner, vm, tstring_cstr(blk), &cmds, &consts, &info);
			END_LABEL:
			tunit_ctr_restore(&uint_ctr);
			free(fullcmd);
			fullcmd = NULL;
			full_len = 0;
			tstring_free(blk);
			if (!keep_running) {
				free(line);
				break;
			}
		}
		free(line);
	}

	free(fullcmd);
	tvmcmd_vect_free(&cmds);
	tconsts_free(&consts);
	tcp_free(&syner);
	tvm_clean(&vm_storage);
}

static void cope_with_1_input(tsession *sess, const char *p)
{
	if (strcmp(p, "-v") == 0) {
		printf("tap-script (%s) Copyright (C) %s %s.\n", Tap_Version, Tap_Year, Tap_Author);
		printf("MIT License: <https://opensource.org/licenses/MIT>\n");
	} else if (strcmp(p, "-h") == 0) {
		print_usage();
	} else {
		tsession_execute_file(sess, p, 1);
	}
}

static void cope_with_2_params(tsession *sess, const char *p_i0, const char *p_i1)
{
	if (strcmp(p_i0, "-c") == 0)
		tsession_compile_file(sess, p_i1, 1);
	else if (strcmp(p_i0, "-e") == 0)
		tsession_eval_bycodes(sess, p_i1);
	else if (strcmp(p_i0, "-r") == 0)
		tsession_show_bycodes(sess, p_i1);
	else if (strcmp(p_i0, "-p") == 0)
		tsession_add_path(sess, p_i1);
	else if (strcmp(p_i0, "-i") == 0)
		exec_interact(sess, p_i1);
	else if (strcmp(p_i0, "-ce") == 0) {
		tsession_compile_file(sess, p_i1, 1);
		tsession_eval_bycodes(sess, p_i1);
	} else if (strcmp(p_i0, "-cr") == 0) {
		tsession_compile_file(sess, p_i1, 1);
		tsession_show_bycodes(sess, p_i1);
	}
}

int main(int argc, char **argv)
{
	tsession * sess = tsession_new();
	int exit_code = 0;
	int markdown_stdout = 0;

	if (argc == 1) {
		cope_with_stdin(sess);
		tsession_free(sess);
		return 0;
	}

	for (int i = 1; i < argc; i++) {
		const char *arg = argv[i];
		if (strcmp(arg, "-h") == 0) {
			print_usage();
			break;
		} else if (strcmp(arg, "-v") == 0) {
			cope_with_1_input(sess, arg);
			break;
		} else if (strcmp(arg, "--stdout") == 0) {
			markdown_stdout = 1;
		} else if (strcmp(arg, "-p") == 0 ||
			   strcmp(arg, "-i") == 0 ||
			   strcmp(arg, "-c") == 0 ||
			   strcmp(arg, "-e") == 0 ||
			   strcmp(arg, "-r") == 0 ||
			   strcmp(arg, "-ce") == 0 ||
			   strcmp(arg, "-cr") == 0) {
			if (i + 1 >= argc) {
				fprintf(stderr, "tap: option %s requires an argument\n", arg);
				exit_code = 2;
				break;
			}
			cope_with_2_params(sess, arg, argv[++i]);
		} else if (arg[0] == '-') {
			fprintf(stderr, "tap: unknown option %s\n", arg);
			exit_code = 2;
			break;
		} else {
			if (is_markdown_file(arg) && !markdown_stdout)
				tsession_execute_markdown_update(sess, arg, 1);
			else
				tsession_execute_file(sess, arg, 1);
		}
	}

	tsession_free(sess);
	return exit_code;
}
