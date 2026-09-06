/**
 * @file main.c
 * @brief Implements the Tapas command-line program.
 * @details Parses command-line options, runs files and modules, and provides
 * the interactive Readline-based REPL.
 * @note This file is a host-program entry point and is not part of the Tapas
 * language core or embedding API.
 */
#if defined(__linux__) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include "tapas/tsession.h"
#include "tapas/dsa/tstring.h"
#include "tapas/version.h"
#include "input_state.h"
#include "compile/compiler.h"
#include "runtime/tvm.h"

#include <readline/history.h>
#include <readline/readline.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#elif defined(__linux__)
#include <unistd.h>
#endif

static char *running_executable_path(const char *argument_zero)
{
#if defined(__APPLE__)
	uint32_t size = 0;
	_NSGetExecutablePath(nullptr, &size);
	char *path = (char *)malloc(size);
	if (path && _NSGetExecutablePath(path, &size) == 0)
		return path;
	free(path);
#elif defined(__linux__)
	size_t capacity = 1024;
	for (;;) {
		char *path = (char *)malloc(capacity);
		if (!path)
			break;
		ssize_t length = readlink("/proc/self/exe", path, capacity - 1);
		if (length >= 0 && (size_t)length < capacity - 1) {
			path[length] = '\0';
			return path;
		}
		free(path);
		if (length < 0 || capacity > SIZE_MAX / 2)
			break;
		capacity *= 2;
	}
#endif
	if (!argument_zero || !strchr(argument_zero, '/'))
		return nullptr;
	size_t length = strlen(argument_zero) + 1;
	char *fallback_path = (char *)malloc(length);
	if (fallback_path)
		memcpy(fallback_path, argument_zero, length);
	return fallback_path;
}

static void add_executable_standard_library(tsession *session,
					    const char *argument_zero)
{
#ifdef TAPAS_RELATIVE_STDLIB_DIR
	char *executable = running_executable_path(argument_zero);
	if (!executable)
		return;
	char *separator = strrchr(executable, '/');
	if (!separator) {
		free(executable);
		return;
	}
	size_t directory_length = (size_t)(separator - executable);
	const char suffix[] = "/" TAPAS_RELATIVE_STDLIB_DIR;
	char *stdlib_path = (char *)malloc(directory_length + sizeof(suffix));
	if (stdlib_path) {
		memcpy(stdlib_path, executable, directory_length);
		memcpy(stdlib_path + directory_length, suffix, sizeof(suffix));
		tsession_add_path(session, stdlib_path);
		free(stdlib_path);
	}
	free(executable);
#else
	(void)session;
	(void)argument_zero;
#endif
}

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
		tvm_set_vmstack(vm, nullptr, 0);
		return 1;
	}
	tapas_error_recover_enabled = 1;

	/* Shell Commands: for debugging */
	if (strcmp(cmd, "exit()") == 0) {
		tapas_error_recover_enabled = 0;
		return 0;
	}
	/* Try to compile code block */
	tstring **paths = tlib_get_paths(tsession_get_lib(sess));
	uint_lexs npaths = tlib_get_npaths(tsession_get_lib(sess));
	uint_cmds ncmd_old = tvmcmd_vect_size32(tcmds);

	/* Parse unit - compile into bytecodes */
	tstring *cmd_ts = tstring_new(cmd);
	terror_set_source_context(cmd);
	*info = parse_unit(syner, cmd_ts, tcmds, consts, paths, npaths, 1, 0);
	tstring_free(cmd_ts);

	/* Try to execute the compiled code block */
	twrapper* wrapper = tanalyser_wrap(tcmds, consts, info);
	if (wrapper) {
		tlib_set_wrapper(tsession_get_lib(sess), wrapper);
		tvm_set_tmpmax(vm, info->tmp_max);
		tvm_set_vmstack(vm,
					tcompo_env_get_vmstack(&tsession_get_lib(sess)->env),
					info->reg_max);
		eval_bycodes(vm, ncmd_old, tsession_get_lib(sess));
		tvm_clean(vm);
		tvm_set_vmstack(vm, nullptr, 0);
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
	printf("Usage: tapas [OPTION] [FILE/CMD]\n");
	printf("\n");
	printf("Example: tapas -ce example.tap\n");
	printf("If no OPTION or FILE/CMD is given, tapas enters interactive mode.\n");
	printf("If only FILE is given but no OPTION, tapas executes the file directly.\n");
	printf("\n");
	printf("OPTION without FILE/CMD followed:\n");
	printf("  -h, --help        show this help\n");
	printf("  -v, --version     show version information\n");
	printf("\n");
	printf("OPTION with FILE followed:\n");
	printf("  -c                compile source file (.tap/.md) and get binary file (.tapc)\n");
	printf("  -e                execute binary file (.tapc)\n");
	printf("  -r                display binary file (.tapc)\n");
	printf("  -ce               combination of -c and -e\n");
	printf("  -cr               combination of -c and -r\n");
	printf("  --stdout          execute markdown output on stdout instead of updating the file\n");
	printf("  -m MODULE [ARGS]  execute MODULE's exported main(arguments)\n");
	printf("\n");
	printf("OPTION with PATH followed:\n");
	printf("  -p                add a module search path\n");
	printf("\n");
	printf("OPTION with CMD followed:\n");
	printf("  -i                execute the CMD\n");
	printf("\n");
	printf("Report bugs: %s\n", TAPAS_BUG_REPORT_URL);
	printf("Homepage: %s\n", TAPAS_HOMEPAGE);
}

static void cope_with_stdin(tsession *sess)
{
	printf("Tapas %s Copyright (C) %s\n",
	       TAPAS_VERSION, TAPAS_COPYRIGHT_YEARS);
	printf("MIT License: <https://opensource.org/licenses/MIT>\n");

	/* Tap lexer */
	tinput_state input_state;
	tinput_state_init(&input_state);

	/* Tap compiler & VM */
	tcp *syner = tcp_new_library(tsession_get_lib(sess), 1);
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
	char *fullcmd = nullptr;
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
			tinput_state_update(&input_state, line[j]);
		in_dpd = tinput_state_complete(&input_state);

		/* Execute line */
		if (in_dpd) {
			int keep_running = 1;
			tstring *blk = tstring_new(fullcmd);
			tstring_trim(blk);
			if (tstring_empty(blk))
				goto END_LABEL;
			keep_running = exec_cmd_seq(sess, syner, vm,
				tstring_cstr(blk), &cmds, &consts, &info);
			END_LABEL:
			tinput_state_init(&input_state);
			free(fullcmd);
			fullcmd = nullptr;
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
	tcp_delete(syner);
	tvm_clean(&vm_storage);
}

static void cope_with_1_input(tsession *sess, const char *p)
{
	if (strcmp(p, "-v") == 0 || strcmp(p, "--version") == 0) {
		printf("Tapas %s Copyright (C) %s\n",
		       TAPAS_VERSION, TAPAS_COPYRIGHT_YEARS);
		printf("MIT License: <https://opensource.org/licenses/MIT>\n");
	} else if (strcmp(p, "-h") == 0 || strcmp(p, "--help") == 0) {
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
	add_executable_standard_library(sess, argv[0]);
	int exit_code = 0;
	int markdown_stdout = 0;

	if (argc == 1) {
		cope_with_stdin(sess);
		tsession_free(sess);
		return 0;
	}

	for (int i = 1; i < argc; i++) {
		const char *arg = argv[i];
		if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
			print_usage();
			break;
		} else if (strcmp(arg, "-v") == 0 || strcmp(arg, "--version") == 0) {
			cope_with_1_input(sess, arg);
			break;
		} else if (strcmp(arg, "--stdout") == 0) {
			markdown_stdout = 1;
		} else if (strcmp(arg, "-m") == 0) {
			if (i + 1 >= argc) {
				fprintf(stderr, "tapas: option -m requires a module\n");
				exit_code = 2;
				break;
			}
			const char *module = argv[++i];
			exit_code = tsession_execute_module(
				sess, module, argc - i - 1,
				(const char *const *)(argv + i + 1));
			break;
		} else if (strcmp(arg, "-p") == 0 ||
			   strcmp(arg, "-i") == 0 ||
			   strcmp(arg, "-c") == 0 ||
			   strcmp(arg, "-e") == 0 ||
			   strcmp(arg, "-r") == 0 ||
			   strcmp(arg, "-ce") == 0 ||
			   strcmp(arg, "-cr") == 0) {
			if (i + 1 >= argc) {
				fprintf(stderr, "tapas: option %s requires an argument\n", arg);
				exit_code = 2;
				break;
			}
			cope_with_2_params(sess, arg, argv[++i]);
		} else if (arg[0] == '-') {
			fprintf(stderr, "tapas: unknown option %s\n", arg);
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
