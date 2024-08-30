#ifndef TAPAS_H
#define TAPAS_H

#include "tc.h"
#include "tvm.h"

namespace tapas
{

/// ls(lib): list library variables
inline void lib_ls(tobj* const params, uint_size_reg len, tobj& vre, tcompo_env * env)
{
	if (len != 1 && len != 0)
		twarn(ErrRuntime_ParamsCtr).warn("lib_ls", "");

	if (len == 1)
	{
		if (params->get_type() != tcompo)
			twarn(ErrRuntime_ParamsType).warn("lib_ls", "");
		if (params->get_v_tcompo()->get_compo_type_code() != compo_tlib)
			twarn(ErrRuntime_ParamsType).warn("lib_ls", "");
		tlib * lib = reinterpret_cast<tlib*>(params->get_v_tcompo());
		vre.set_v(lib->listing_objects());
	}
	if (len == 0)
	{
		if (env->get_father_env() != nullptr)
			twarn(ErrRuntime_RefType).warn("lib_ls", "tlib supported only");
		tlib * lib = static_cast<tlib *>(env);
		vre.set_v(lib->listing_objects());
	}
}

/// path([lib]): list library paths
inline void lib_path(tobj* const params, uint_size_reg len, tobj& vre, tcompo_env * env)
{
	if (len != 0 && len != 1)
		twarn(ErrRuntime_ParamsCtr).warn("lib_path", "");
	if (len == 1) {
		if (params->get_type() != tcompo)
			twarn(ErrRuntime_ParamsType).warn("lib_path", "");
		if (params->get_v_tcompo()->get_compo_type_code() != compo_tlib)
			twarn(ErrRuntime_ParamsType).warn("lib_path", "");
		tlib * lib = reinterpret_cast<tlib*>(params->get_v_tcompo());
		vre.set_v(lib->listing_paths());
	}
	if (len == 0) {
		if (env->get_father_env() != nullptr)
			twarn(ErrRuntime_EnvInconsis).warn("lib_path", "");
		tlib * lib = static_cast<tlib *>(env);
		vre.set_v(lib->listing_paths());
	}
}

/// __param__(idx): used in functions, return the value of the idx-th params.
inline void tf_param(tobj* const params, uint_size_reg len, tobj& vre, tcompo_env * env)
{
	if (env->get_father_env() == nullptr)
		twarn(ErrRuntime_EnvInconsis).warn("tf_param", "");
	if (len != 1)
		twarn(ErrRuntime_ParamsCtr).warn("tf_param", "1 parameter");
	if (params->get_type() != tint)
		twarn(ErrRuntime_ParamsType).warn("tf_param", "integer as parameter");
	long idx = params->get_v_tint();
	tfunc * f = static_cast<tfunc *>(env);
	if (idx >= f->get_dynamic_nparams())
		twarn(ErrRuntime_IdxOutRange).warn("tf_param", "");
	vre = f->get_params()[idx];
}

/// __nparam__(): used in functions, return the number of params.
inline void tf_nparam(tobj * const params, uint_size_reg len, tobj& vre, tcompo_env * env)
{
	if (env->get_father_env() == nullptr)
		twarn(ErrRuntime_EnvInconsis).warn("tf_nparam", "");
	if (len != 0 || params->get_type() != tcompo)
		twarn(ErrRuntime_ParamsCtr).warn("tf_nparam", "0 parameter");
	tfunc * f = static_cast<tfunc *>(env);
	vre.set_v(static_cast<long>(f->get_dynamic_nparams()));
}

/// Session level functions Registration
inline void register_os_sessf(tlib & lib)
{
	tdict * sys = lib.add_pkg("sys");

	sys->add_obj("__ls__", tobj(new tcppsessf(lib_ls, "__ls__")));
	sys->add_obj("__path__", tobj(new tcppsessf(lib_path, "__path__")));
	sys->add_obj("__param__", tobj(new tcppsessf(tf_param, "__param__")));
	sys->add_obj("__nparam__", tobj(new tcppsessf(tf_nparam, "__nparam__")));
}

/** Management of Tap session through APIs between Tap and C++.
 *  @details
 *  These APIs include
 *    - (1) source code compiling: compile_file(tap_file_location)
 *    - (2) binary check: show_bycodes(tap_file_location)
 *    - (3) binary exection: eval_bycodes(tap_file_location)
 */
class tsession
{
private:
	tlib * __lib;

public:
tsession()
{
	__lib = new tlib();
	register_os_sessf(*__lib);
	register_cppfuncs(*__lib);
	register_for_Eigen_APIs(*__lib);
}

~tsession()
{
	delete __lib;
}

/// @return __lib
tlib * get_lib() const
{
	return __lib;
}

/** Compile tap source code file or markdown file to '.tapc' file
 *  @param file (std::string) tap source code file location.
 *  @param interactive (bool) compile in interactive mode
 */
void compile_file(const std::string & file, bool interactive = true)
{
	try {
		tcp syner(__lib->get_default_v_names(), nullptr, interactive);
		syner.compile_file(file, __lib->get_paths());
	} catch(...) {
		exit(-1);
	}
}

/** Evaluate the compiled tap source code file
 *  @param file (std::string) tap source code file location.
 */
void eval_bycodes(const std::string & file)
{
	try {
		std::string binf = file.substr(0, file.find_last_of(".")) + ".tapc";
		twrapper * wrapper = tanalyser().load_bin_file(binf);
		__lib->set_wrapper(wrapper);
		__lib->add_path(tlexer1().get_folder_from_file_loc(binf));  // add path
		tvm(wrapper->info.tmp_max).eval_bycodes(0, __lib);
	} catch(...) {
		exit(-1);
	}
}

/** Compile & evaluate file without store the binary codes
 *  @param file (std::string) tap source code file location.
 *  @param interactive (bool) compile in interactive mode
 */
void execute_file(const std::string & file, bool interactive = true)
{
	try {
		// Add directory to path
		__lib->add_path(tlexer1().get_folder_from_file_loc(file));

		// Compile
		tcp syner(__lib->get_default_v_names(), nullptr, interactive);
		twrapper * wrapper = syner.compile_file_2(file, __lib->get_paths());

		// Execution
		__lib->set_wrapper(wrapper);
		tvm(wrapper->info.tmp_max).eval_bycodes(0, __lib);
	} catch(...) {
		exit(-1);
	}
}

/// Compile & evaluate `str`
void execute_str(const std::string & str, bool interactive = true)
{
	try {
		twrapper * wrapper = nullptr;
		tcp syner(__lib->get_default_v_names(), nullptr, interactive);
		wrapper = syner.compile_str(str, __lib->get_paths());
		__lib->set_wrapper(wrapper);
		tvm(wrapper->info.tmp_max).eval_bycodes(0, __lib);
	} catch(...) {
		exit(-1);
	}
}

/** Show bycodes of the compiled tap source code file
 *  @param file (std::string) tap source code file location
 */
void show_bycodes(const std::string & file)
{
	try {
		std::string binf = file.substr(0, file.find_last_of(".")) + ".tapc";
		twrapper * wrapper = tanalyser().load_bin_file(binf);
		tanalyser analyser;
		analyser.display_wrapper(wrapper);
		analyser.clean_wrapper(wrapper);
	} catch(...) {
		exit(-1);
	}
}

/** Add a package (tdict)
 *  @param pkgname (std::string) name of the package
 *  @return pkg    (tdict) a dictionary
 */
tdict * add_pkg(const std::string & pkgname)
{
	return __lib->add_pkg(pkgname);
}

/** Add tap source code file searching path
 *  @param file (std::string) path to be added.
 */
void add_path(const std::string & file)
{
	__lib->add_path(file);
}

/** Get C++ entity of a tap object, which is **tapas::tapv**
 *  @param loc (unsigned short int) the location of the required tap
 *                                  object in root library environment
 *  @return the C++ entity of the required tap object.
 */
tobj & get_obj(uint_size_obj loc)
{
	uint_size_obj len = __lib->get_objlst_len();

	// return the nearest object to loc.
	if (loc >= len)
		twarn(ErrRuntime_IdxOutRange).warn("tsession::get_obj", "");
	uint_size_obj rloc = len - loc - 1;
	return __lib->get_obj(rloc);
}

/** Load bycodes
 *  @param file (std::string) the location of tap source code file
 *  @return a pointer of tapas::twrapper, i.e., a wrapper of bycodes
 */
twrapper * load_bycodes(const std::string & file)
{
	return tanalyser().load_bin_file(file + "c");
}

/** Release bycodes
 *  @param wrapper (tapas::twrapper *) a wrapper of bycodes
 */
void release_bycodes(twrapper * wrapper)
{
	tanalyser().clean_wrapper(wrapper);
}

};

}; // end namespace tap

#endif
