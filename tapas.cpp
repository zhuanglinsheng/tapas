#include "src/tapas.h"
#include <readline/readline.h>
#include <readline/history.h>

using namespace tapas;

/// @return a boolean of continuing the program or not
bool exec_cmd_seq(tsession & sess,
			tcp & syner,
			tvm & vm,
			std::string & cmd,    // a syntax analysis unit
			tvmcmd_vect & tcmds,
			tconsts & consts,
			tcinfo & info)
{
	// Shell Commands: for debugging
	if (cmd == "exit()")
		return false;
	if (cmd == "binary()")
	{
		tanalyser analyser;
		twrapper * wrapper = analyser.wrap(tcmds, consts, info);
		analyser.display_wrapper(wrapper);
		analyser.clean_wrapper(wrapper);
		return true;
	}

	// Try to compile code block
	std::vector<std::string> paths = sess.get_lib()->get_paths();
	uint_cmds ncmd_old = tcmds.size32();
	try{
		info = syner.parse_unit(cmd, tcmds, consts, paths, 1, 0);
	} catch(...) {
		return true;
	}

	// Try to execute the compiled code block
	tconsts consts_cpy = consts.copy();
	sess.get_lib()->set_wrapper(tanalyser().wrap(tcmds, consts_cpy, info));
	vm.set_tmpmax(info.tmp_max);
	try{
		vm.eval_bycodes(ncmd_old, sess.get_lib());
	} catch(...) {
		vm.clean();
	}
	return true;
}

void exec_interact(tsession & sess, const std::string & p_i1)
{
	std::string cmds = p_i1;
	if ((p_i1[0] == '\'' && p_i1[p_i1.length() - 1] == '\'')
	 ||(p_i1[0] == '"'  && p_i1[p_i1.length() - 1] == '"'))
		cmds = tlexer1().trim(p_i1.substr(1, p_i1.length() - 2));
	printf("Result:\n");
	sess.execute_str(cmds);
}

void cope_with_stdin(tsession & sess)
{
	printf("Tapas Script (%s) Copyright (C) %s %s.\n", Tap_Basic_Info);
	printf("MIT License: <https://opensource.org/licenses/MIT>\n\n");
	printf("Type `exit()` for leaving,\n");
	printf("     `binary()` for printing out binary codes, and\n");
	printf("     `sys::__ls__()` for displaying all preloads.\n\n");

	// Tap lexer
	tunit_ctr uint_ctr;
	uint_ctr.restore_lex_ctrs();

	// Tap compiler & VM
	tcp syner(sess.get_lib()->get_default_v_names(), nullptr, true);
	tvmcmd_vect cmds;
	tconsts consts;
	tcinfo info;
	tvm vm(0);

	// UI
	bool in_dpd = true;

	// Calling GNU readline
	char * buffer;
	std::string cmd;
	rl_bind_key('\t', rl_complete);

	while ((buffer = readline(in_dpd? ">> " : ".. ")) != nullptr) {
		if (!buffer) {
			break;
		}

		// append buffer
		cmd += buffer;
		cmd += "\n";

		// Add to history
		add_history(buffer);

		// Scan line
		uint_lexs i = 0;

		for (; i<std::string(buffer).length(); i++)
			uint_ctr.update_lex_ctrs(buffer[i]);
		in_dpd = uint_ctr.independent();

		// Excute line
		if (in_dpd)
		{
			std::string blk = tlexer1().trim(std::string(cmd));

			if (blk.size() == 0)
				goto END;
			if (!exec_cmd_seq(sess, syner, vm, blk, cmds, consts, info))
			{
				free(buffer);
				break;
			}
			END:
			uint_ctr.restore_lex_ctrs();
			cmd = "";
		}
		free(buffer);
	}
}

void cope_with_1_input(tsession & sess, std::string p)
{
	if (p == "-v") {
		printf("tap-script (%s) Copyright (C) %s %s.\n", Tap_Basic_Info);
		printf("MIT License: <https://opensource.org/licenses/MIT>\n");
	} else if (p == "-h") {
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
		printf("  -c                compile source file (.tap) and get binary file (.tapc)\n");
		printf("  -e                execute binary file (.tapc)\n");
		printf("  -r                display binary file (.tapc)\n");
		printf("  -ce               combination of -c and -e\n");
		printf("  -cr               combination of -c and -r\n");
		printf("\n");
		printf("OPTION with CMD followed:\n");
		printf("  -i                execute the CMD\n");
		printf("\n");
		printf("Report bugs\n");
		printf("Contact: <zhuanglinsheng@outlook.com>\n");
	} else
		sess.execute_file(p);
}

void cope_with_2_params(tsession & sess, const std::string & p_i0, const std::string & p_i1)
{
	if (p_i0 == "-c")
		sess.compile_file(p_i1);
	else if (p_i0 == "-e")
		sess.eval_bycodes(p_i1);
	else if (p_i0 == "-r")
		sess.show_bycodes(p_i1);
	else if (p_i0 == "-p")
		sess.add_path(p_i1);
	else if (p_i0 == "-i")
		exec_interact(sess, p_i1);
	else if (p_i0 == "-ce") {
		sess.compile_file(p_i1);
		sess.eval_bycodes(p_i1);
	} else if (p_i0 == "-cr") {
		sess.compile_file(p_i1);
		sess.show_bycodes(p_i1);
	}
}

int main(int argc, char **argv)
{
	tsession sess;

	if (argc == 1)
		cope_with_stdin(sess);
	if (argc == 2)
		cope_with_1_input(sess, argv[1]);
	if (argc >= 3) {
		for (int i = 0; i < argc - 1; i++)
			cope_with_2_params(sess, argv[i], argv[i + 1]);
	}
	return 0;
}
