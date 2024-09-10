#ifndef TENV_H
#define TENV_H

#include "trts.h"
#include "extensions/tstd.h"

namespace tapas
{

/// Abstract Environment, a tree structure which stores objects in runtime.
class tcompo_env_abstract : public tobj_array
{
private:
	tcompo_env_abstract * __father_env;       /// Environment as a Tree
	uint_size_obj __loc_in_father_env;            /// Environment as a Tree

public:
tcompo_env_abstract(uint_size_obj objlst_cap, tcompo_env_abstract * father_env) : tobj_array(objlst_cap)
{
	__father_env = father_env;
	__loc_in_father_env = 0;

	// Env Tree: check looping reference
	if (0 == self_not_in_tree_of(father_env))
		twarn(ErrRuntime_LoopRef).warn("tcompo_env::tcompo_env", "");

	// Env Tree: get __loc_in_father_env
	if (__father_env == nullptr)
		return;
	for (; __loc_in_father_env < father_env->get_objlst_len(); __loc_in_father_env++) {
		tobj & vi = __father_env->tobj_array::get_obj(__loc_in_father_env);

		if (vi.get_type() != tcompo)
			continue;
		if (this == reinterpret_cast<tcompo_env_abstract *>(vi.get_v_tcompo()))
			break;
	}
}

virtual ~tcompo_env_abstract() {}

/// @return the father environment
tcompo_env_abstract * get_father_env()
{
	return __father_env;
}

/// @return the top environment
tcompo_env_abstract * get_top_env()
{
	tcompo_env_abstract * top = this;

	while (nullptr != top->get_father_env())
		top = top->get_father_env();
	return top;
}

/// @return the location of `this` object in father environment
uint_size_obj get_loc_in_father_env() const
{
	return __loc_in_father_env;
}

/// @return the object located at `loc`
tobj & get_obj(uint_size_obj loc, tcompo_env_abstract * from = nullptr)
{
	if (from == nullptr) {
		if (loc < get_objlst_len())
			return tobj_array::get_obj(loc);
		else {
			if (!__father_env)
				twarn(ErrRuntime_ObjUnfound).warn("tcompo_env::get_obj", "");
			return __father_env->get_obj(loc - get_objlst_len(), this);
		}
	} else {
		uint_size_obj from_loc = from->__loc_in_father_env;

		if (loc < from_loc)
			return tobj_array::get_obj(loc);
		else {
			if (!__father_env)
				twarn(ErrRuntime_ObjUnfound).warn("tcompo_env::get_obj", "");
			return __father_env->get_obj(loc - from_loc, this);
		}
	}
}

void set_obj(uint_size_obj loc, const tobj & v, tcompo_env_abstract * from = nullptr)
{
	if (from == nullptr) {
		if (loc < get_objlst_len())
			tobj_array::set_obj(loc, v);
		else {
			if (!__father_env)
				twarn(ErrRuntime_ObjUnfound).warn("tcompo_env::set_obj", "");
			__father_env->set_obj(loc - get_objlst_len(), v);
		}
	} else {
		uint_size_obj from_loc = from->__loc_in_father_env;

		if (loc < from_loc)
			tobj_array::set_obj(loc, v);
		else {
			if (!__father_env)
				twarn(ErrRuntime_ObjUnfound).warn("tcompo_env::set_obj", "");
			__father_env->set_obj(loc - from_loc, v, this);
		}
	}
}

/// check 'env' is not in the tree of 'this'
bool self_tree_has_no(tcompo_env_abstract * const env)
{
	if (env == nullptr)
		return true;
	if (env == this)
		return false;
	if (!__father_env)
		return true;
	else
		return __father_env->self_tree_has_no(env);
}

/// check 'this' is not the the tree of 'env'
bool self_not_in_tree_of(tcompo_env_abstract * const env)
{
	if (env == nullptr)
		return true;
	if (env == this)
		return false;
	return self_not_in_tree_of(env->get_father_env());
}

};


/// Environment = environment tree + VM infos
class tcompo_env : public tcompo_env_abstract
{
private:
	tobj * __vmstack;                /// VM info
	tobj * __params;                 /// VM info
	uint_size_obj __tmpmax;              /// VM info
	uint_size_stk __regmax;              /// VM info
	uint_size_stk __nparams;             /// VM info
	uint_size_stk __dynamic_nparams = 0; /// VM info
	tcompo_type __compo_type;        /// Environment as a Tree

public:

tcompo_env(uint_size_obj objlst_cap, tcompo_env * father_env, uint_size_stk regmax,
		uint_size_stk tmpmax, uint_size_stk nparams, tcompo_type compo_type)
	: tcompo_env_abstract(objlst_cap, father_env)
{
	__vmstack = nullptr;
	__params = nullptr;
	__dynamic_nparams = 0;
	set_tmpmax(tmpmax);
	set_regmax(regmax);
	set_nparams(nparams);
	__compo_type = compo_type;
}

virtual ~tcompo_env()
{
	set_regmax(0);
}

/// @return a pointer to self
virtual tcompo_env * get_self()
{
	return this;
}

/// @return a type code of this `__compo_type`
tcompo_type tenv_get_compo_type()
{
	return __compo_type;
}

/// Set the type of composite data
void tenv_set_compo_type(tcompo_type type)
{
	__compo_type = type;
}

/// @return the father environment
tcompo_env * get_father_env()
{
	return static_cast<tcompo_env *>(tcompo_env_abstract::get_father_env());
}

/// @return the number of parameters (for VM)
uint_size_stk get_nparams() const
{
	return __nparams;
}

/// @return __dynamic_nparams (for VM)
uint_size_stk get_dynamic_nparams() const
{
	return __dynamic_nparams;
}

/// @return __regmax - the maximum usage of registers (for VM)
uint_size_stk get_regmax() const
{
	return __regmax;
}

/// @return __vmstack (for VM)
tobj * get_vmstack() const
{
	return __vmstack;
}

/// @return __tmpmax (for VM)
uint_size_obj get_tmpmax() const
{
	return __tmpmax;
}

/// @return the parameter list (for VM)
tobj * get_params() const
{
	return __params;
}

/// Set the number of parameters in signiture (for VM)
void set_nparams(uint_size_stk n)
{
	 __nparams = n;
}

/// Set the number of parameters inputted (for VM)
void set_dynamic_nparams(uint_size_stk n)
{
	__dynamic_nparams = n;
}

/// Set __regmax (for VM)
void set_regmax(uint_size_stk n)
{
	__regmax = n;

	if (__vmstack) {
		delete [] __vmstack;
		__vmstack = nullptr;
	}
	if (n > 0)
		__vmstack = new tobj[n];
}

/// Set __tmpmax (for VM)
void set_tmpmax(uint_size_obj ntmps)
{
	__tmpmax = ntmps;
}

/// Set __params (for VM)
void set_params(tobj * params)
{
	__params = params;
}

};


/// C++ Session Level Functions
typedef void (* sessf)(tobj * const, uint_size_stk, tobj &, tcompo_env *);


/// A wrapper of C++ Session Level Functions
class tcppsessf : public tcompo_v
{
private:
	sessf __f;
	const char * __name;

public:
tcppsessf(const sessf & f, const char * name)
{
	__f = f;
	__name = name;
}

~tcppsessf() {}

/// @return the Session level function __f
sessf & get_f()
{
	return __f;
}

/// @return a brief string of this object
std::string tostring_abbr() const
{
	return tostring_pointer(get_type(), this);
}

/// @return a detailed string of this object
std::string tostring_full() const
{
	return tostring_abbr();
}

/// @return a copy of this object
tcppsessf * copy()
{
	return new tcppsessf(__f, __name);
}

/// @return a string of the type of this object "C++ Session Function"
const char * get_type() const
{
	return "C++ Session Function";
}

/// @return the type code of this composite object
tcompo_type get_compo_type_code() const
{
	return compo_sessfunc;
}

/// @return 0 - the length of this object
long len() const
{
	return 0;
}

/// @return a boolean of Is this is identical to `v`
bool identical(tcompo_v * v) const
{
	return v == this;
}

/// @return the name of this Session level C++ function
const char * get_name() const
{
	return __name;
}

};


/// User Defined Functions in Tap
class tfunc : public tcompo_v, public tcompo_env
{
private:
	uint_size_cmd __cmdloc;
	uint_size_cmd __ncmds;

public:
tfunc(uint_size_obj nlocals, tcompo_env * father_env, uint_size_stk reg_max,
		uint_size_obj tmpmax, uint_size_stk nparams,
		uint_size_cmd cmdloc, uint_size_cmd ncmds)
	: tcompo_env(nlocals, father_env, reg_max, tmpmax, nparams, compo_tfunc)
{
	__cmdloc = cmdloc;
	__ncmds  = ncmds;
}

~tfunc() {}

/// Assign `params` to the object array of this environment
void assign_params(tobj * const params, uint_size_stk nparams)
{
	// settings: tcompo_env
	set_objlst_len(0);

	if (get_nparams() != UNDEF_NPARAMS)
		for (uint_size_stk i = 0; i < nparams; i++) {
			add_obj();
			set_obj(i, params[i]);
		}

	// settings: tcompo_eval_basic
	set_dynamic_nparams(nparams);
	set_params(params);
}

/// @return the location of the command of this function in bycode list
uint_size_cmd get_cmdloc() const
{
	return __cmdloc;
}

/// @return number of commands
uint_size_cmd get_ncmds() const
{
	return __ncmds;
}

/// @return a brief string of this object
std::string tostring_abbr() const
{
	return tostring_pointer(get_type(), this);
}

/// @return a detailed string of this object
std::string tostring_full() const
{
	return tostring_abbr();
}

/// @return a copy of this object
tfunc * copy()
{
	return new tfunc(get_objlst_cap(), get_father_env(), get_regmax(),
				get_tmpmax(), get_nparams(),
				get_cmdloc(), get_ncmds());
}

/// @return a pointer to self
tfunc * get_self()
{
	return this;
}

/// @return a string of the type of this object `Function`
const char * get_type() const
{
	return "Function";
}

/// @return the type code of this composite object `compo_tfunc`
tcompo_type get_compo_type_code() const
{
	return compo_tfunc;
}

/// @return 0 - the length of this object
long len() const
{
	return 0;
}

/// @return a boolean of Is this is identical to `v`
bool identical(tcompo_v * v) const
{
	return v == this;
}

};


/// Library in Tap.
class tlib : public tcompo_v, public tcompo_env
{
private:
	std::vector<std::string> __default_v_names; /// default variables
	std::vector<std::string> __paths;           /// searching path
	twrapper               * __wrapper;         /// wrapper
	tdict                  * __exposed;         /// exposed dict

/// Remove wrapper from the library
void rm_wrapper()
{
	if (nullptr != __wrapper)
		tanalyser().clean_wrapper(__wrapper);
	__wrapper = nullptr;
	set_tmpmax(0);
	set_regmax(0);
}

public:
tlib() : tcompo_env(0, nullptr, 0, 0, 0, compo_tlib)
{
	__wrapper = nullptr;
	__exposed = nullptr;
}

~tlib()
{
	rm_wrapper();
	if (__exposed != nullptr)
		delete __exposed;
}

/// Add wrapper to the library
void set_wrapper(twrapper * wrapper)
{
	if (nullptr != __wrapper)
		rm_wrapper();
	__wrapper = wrapper;
	try_expand_objlist(wrapper->info.obj_max);
	set_tmpmax(wrapper->info.tmp_max);
	set_regmax(wrapper->info.reg_max);
}

/// Add object `v` named by `name` to the library
void lib_add_obj(const std::string & name, const tobj & v)
{
	uint_size_obj current_len = __default_v_names.size();
	try_expand_objlist(current_len + 10);
	add_obj();
	set_obj(current_len, v);
	__default_v_names.push_back(name);
}

/// Add a package to the library
tdict * add_pkg(const std::string & pkgname)
{
	for (auto iter  = __default_v_names.begin(); iter != __default_v_names.end(); iter++) {
		if (0 == iter->compare(pkgname)) // package already exists
			twarn(ErrRuntime_Other).warn("tlib::add_pkg", pkgname);
	}
	tdict * pkg = new tdict();
	lib_add_obj(pkgname, tobj(pkg));
	return pkg;
}

/// Add a location `path` to the library
void add_path(const std::string & paths)
{
	utils::append_to_pathpool(__paths, paths);
}

/// @return the wrapper
twrapper * get_wrapper()
{
	return __wrapper;
}

/// @return a pointer to self
tlib * get_self()
{
	return this;
}

/// @return a list of string of the names of default objects
std::vector<std::string> & get_default_v_names()
{
	return __default_v_names;
}

/// @return the objects that is exposed to outside and indexable
tdict * get_exposed()
{
	return __exposed;
}

/// Set the objects that is exposed to outside and indexable
void set_exposed(tdict * v)
{
	__exposed = v;
}

/// lib::object - Get the object from library
void idx(const tobj * params, uint_size_stk np, tobj& vre)
{
	__exposed->idx(params, np, vre);
}

/// @return a list of string of paths of the library
std::vector<std::string> & get_paths()
{
	return __paths;
}

/** Copy the default part of tlib (default cpp functions).
 *  @details Just create the default part which contains the default cpp
 *  functions and its flags.
 */
tlib * recreate()
{
	tlib * lib_rct = new tlib();
	uint_size_obj idx = 0;

	for (auto default_iter  = __default_v_names.begin(); default_iter != __default_v_names.end(); default_iter++) {
		lib_rct->lib_add_obj(*default_iter, get_obj(idx));
		idx++;
	}
	return lib_rct;
}

/// @return a brief string of this library object
std::string tostring_abbr() const
{
	return tostring_pointer(get_type(), this);
}

/// @return a detailed string of this library object
std::string tostring_full() const
{
	return __exposed->tostring_full();
}

/// Not applicable
tlib * copy()
{
	twarn(ErrRuntime_Other).warn("tlib::copy", "tlib can not be copied.");
	return nullptr;
}

/// @return a string of the type of this object `Library`
const char * get_type() const
{
	return "Library";
}

/// @return a type code of this composite object `compo_tlib`
tcompo_type get_compo_type_code() const
{
	return compo_tlib;
}

/// @return the number of objects
long len() const
{
	return get_objlst_len();
}

/// @return a boolean of Is this is identical to `v`
bool identical(tcompo_v * v) const
{
	return v == this;
}

/// @return a tlist of string of all object names
tlist * listing_objects()
{
	tlist * ls;
	long idx = 0;

	if (__exposed != nullptr)
		ls = __exposed->keys();
	else
		ls = new tlist();

	for (auto iter  = __default_v_names.begin(); iter != __default_v_names.end(); iter++) {
		tobj v(new tstr(*iter));
		ls->set_insert(&v, idx);
		idx ++;
	}
	for (uint_size_obj idx = __default_v_names.size(); idx < get_objlst_len(); idx++) {
		tobj vi = get_obj(idx);
		std::string name;
		name += std::string(__wrapper->consts.cstrs[vi.get_name_loc()]);
		tobj v(new tstr(name));
		ls->set_append(&v);
	}
	return ls;
}

/// @return a tlist of string of paths of the library
tlist * listing_paths()
{
	tlist * paths = new tlist();

	for (auto it = __paths.begin(); it != __paths.end(); it++) {
		tobj v(new tstr(*it));
		paths->set_append(&v);
	}
	return paths;
}

};

}

#endif // TENV_H
