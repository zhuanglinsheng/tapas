#ifndef TRTS_H
#define TRTS_H

#include "tbasis.h"
#include "tbycs.h"

namespace tapas
{

/*===========================================================================*
 * 1. Type System
 *===========================================================================*/

/// Any Tap object could only be one of the following 5 primitive types.
enum ttypes : uint8_t
{
	tnil,     ///< Type **nil**
	tbool,    ///< Type **bool**
	tint,     ///< Type **int**
	tdouble,  ///< Type **double**
	tcompo    ///< Type **tcompo**, Composite (reference) type, a pointer in essence.
};


/**  Types of tapas::tcompo_v
 *   @details  The inner built **ttypes::tcompo** types includes the following.
 *   They are the essential component of Tap language, providing syntaxes like,
 *   - Environment
 *   - Data structure
 *   - Cpp interaction
 *   - Other basic syntaxes
 */
enum tcompo_type : uint8_t
{
	compo_tpair,      ///< Type **pair**, a shorter list consisting only two elements.
	compo_tstr,       ///< Type **str**, a wrapper of std::string type.
	compo_tdict,      ///< Type **dict**, a wrapper of std::unsorted_map type.
	compo_tlist,      ///< Type **list**, a wrapper of std::vector type.
	compo_time,       ///< Type **time**, a wrapper of std::tm
	compo_titer,      ///< Type **iter**, consisting of four integers marking the indexes.
	compo_tfunc,      ///< Type **func**, functions defined in Tap language.
	compo_cppfunc,    ///< Type **cppfunc**, functions written in Cpp.
	compo_sessfunc,   ///< Type **cppfunc**, Session level functions written in Cpp.
	compo_tarr,       ///< Type **arr**, a wrapper of Eigen::Matrix
	compo_tdarr,      ///< Type **darr**, a wrapper of Eigen::Matrix of double floats.
	compo_tbarr,      ///< Type **barr**, a wrapper of Eigen::Matrix of booleans.
	compo_tlib,       ///< Type **lib**, Tap library, providing a basic runtime environment.
};


/** Composite data in Tap
 *  @details A reference counter is maintained in tcompo_v. General methods of
 *    reference types including `tostring`, `get_type`, `copy`,
 *    `len`, `get_compo_type_code` and `identical`.
 */
class tcompo_v
{
private:
	uint16_t __refctr;

public:
/// Contructor: reference counter = 0, expression type = constant
tcompo_v()
{
	__refctr = 0;
}

/// Deconstructor (virtual)
virtual ~tcompo_v() {};

/// Reference counter add by one
inline void add_refctr()
{
	__refctr++;
}

/// Reference counter deducted by one
inline void ddc_refctr()
{
	__refctr--;
}

/// Get reference counter
inline uint16_t get_refctr() const
{
	return __refctr;
}

/// Generate a string of "<type at location>"
inline std::string tostring_pointer(const char * type, const void * p) const
{
	std::string re;
	char ptr_addr[100];

	if (nullptr == p)
		snprintf(ptr_addr, 100, "\"%s\"", type);
		// sprintf(ptr_addr, "\"%s\"", type);
	else
		snprintf(ptr_addr, 100, "\"%s at %p\"", type, p);
		// sprintf(ptr_addr, "\"%s at %p\"", type, p);
	re += std::string(ptr_addr);
	return re;
}

/// Generate an brief string (pure virtual)
virtual std::string tostring_abbr() const = 0;

/// Generate a detailed string (pure virtual)
virtual std::string tostring_full() const = 0;

/// Copy `this` (pure virtual)
virtual tcompo_v * copy() = 0;

/// Generate a string of the type of `this` (pure virtual)
virtual const char * get_type() const = 0;

/// Get the length of `this` (pure virtual)
virtual long len() const = 0;

/// Get the type code of the composite object `this` (pure virtual)
virtual tcompo_type get_compo_type_code() const = 0;

/// Returns a boolean of Is `this` identical to `v` (pure virtual)
virtual bool identical(tcompo_v * v) const = 0;

};


/// Data of objects in Tap.
union tdata
{
	long i;            ///< Storage for integers
	double d;          ///< Storage for double floats
	tcompo_v * compo;  ///< Storage for compo values
};


/// Objects in Tap where `data` and `type` are maintained
class tobj
{
private:
	ttypes    __type;
	uint_size_obj __env_loc;
	uint_size_obj __name_loc;
	tdata     __data;

/// @return a string of this is a value type object
std::string tostring_values() const
{
	std::string out;

	switch (get_type()) {
	case tnil:
		out += "nil";
		break;
	case tbool:
		if (__data.i) out += "true";
		else         out += "false";
		break;
	case tint:
		out += std::to_string(__data.i);
		break;
	case tdouble:
		out += std::to_string(__data.d);
		break;
	case tcompo:
		break;
	}
	return out;
}

/// Fill in __data
void set_data(tdata data)
{
	__data = data;
}

/// Set type `type`
void set_type(ttypes type)
{
	__type = type;
}

/// Set __data.i to be a boolean value `vbool`
void set_v_tbool(long vbool)
{
	__data.i = vbool;
}

/// Set __data.i to be an integer value `i`
void set_v_tint(long i)
{
	__data.i = i;
}

/// Set __data.d to be a double float value `d`
void set_v_tdouble(double d)
{
	__data.d = d;
}

/// Set __data.compo to be a composite value `v`
void set_v_tcompo(tcompo_v * v)
{
	__data.compo = v;
}

public:
/// Constructor: a nil type object in default
tobj(uint_size_obj obj_loc = UNDEF_ENVLOC, uint_size_cst name_loc = UNDEF_NAMELOC)
{
	set_nil();
	__env_loc = obj_loc;
	__name_loc = name_loc;
}

/// Constructor: a boolean object in default
tobj(bool bv, uint_size_obj obj_loc = UNDEF_ENVLOC, uint_size_cst name_loc = UNDEF_NAMELOC)
{
	set_type(tbool);
	set_v_tbool(bv);
	__env_loc = obj_loc;
	__name_loc = name_loc;
}

/// Constructor: an integer object in default
tobj(long iv, uint_size_obj obj_loc = UNDEF_ENVLOC, uint_size_cst name_loc = UNDEF_NAMELOC)
{
	set_type(tint);
	set_v_tint(iv);
	__env_loc = obj_loc;
	__name_loc = name_loc;
}

/// Constructor: a double float object in default
tobj(double dv, uint_size_obj obj_loc = UNDEF_ENVLOC, uint_size_cst name_loc = UNDEF_NAMELOC)
{
	set_type(tdouble);
	set_v_tdouble(dv);
	__env_loc = obj_loc;
	__name_loc = name_loc;
}

/// Constructor: a composite object in default
tobj(tcompo_v * cv, uint_size_obj obj_loc = UNDEF_ENVLOC, uint_size_cst name_loc = UNDEF_NAMELOC)
{
	set_type(tcompo);
	set_v_tcompo(cv);
	__env_loc = obj_loc;
	__name_loc = name_loc;
}

/// Deconstructor
~tobj() {}

/// Clean the reference object if its reference counter is zero.
///     Then set the object as nil.
void try_clear(bool ddt_refctr = false)
{
	if (get_type() == tcompo) {
		if (ddt_refctr)
			__data.compo->ddc_refctr();
		if (__data.compo->get_refctr() == 0)
			delete __data.compo;
	}
	set_nil();
}

/// Deduct the reference counter of reference object if it is. Then try to
///     clean it if the reference counter is zero. Set it to nil.
void ddc_ref_clear()
{
	try_clear(true);
}

/// @return a boolean of Is this object is composite.
bool is_compo() const
{
	return get_type() == tcompo;
}

/// Set the location of name in Constant list of string.
void set_name_loc(uint_size_cst loc)
{
	__name_loc = loc;
}

/// Set the location of object in list of environmental objects.
void set_env_loc(uint_size_obj loc)
{
	__env_loc = loc;
}

/// Set the type to be `tnil` and the data to be 0
void set_nil()
{
	set_type(tnil);
	set_v_tint(0);
}

/// Set the type and data of this object that type and data of `v`
void set_v(const tobj & v)
{
	set_type(v.get_type());
	set_data(v.__data);
}

/// Set type to be tcompo and data to be `comp`
void set_v(tcompo_v * comp)
{
	set_type(tcompo);
	set_v_tcompo(comp);
}

/// Set type to be tbool and data to be `b`
void set_v(bool b)
{
	set_type(tbool);
	set_v_tbool(b);
}

/// Set type to be tint and data to be `i`
void set_v(long i)
{
	set_type(tint);
	set_v_tint(i);
}

/// Set type to be tdouble and data to be `d`
void set_v(double d)
{
	set_type(tdouble);
	set_v_tdouble(d);
}

/// Get the location of object in environmental object list
uint_size_obj get_obj_loc() const
{
	return __env_loc;
}

/// Get the location of name in constant list of strings
uint_size_cst get_name_loc() const
{
	return __name_loc;
}

/// Get type
ttypes get_type() const
{
	return __type;
}

/// Get the boolean value
long get_v_tbool() const
{
	return __data.i;
}

/// Get the integer value
long get_v_tint() const
{
	return __data.i;
}

/// Get the double float value
double get_v_tdouble() const
{
	return __data.d;
}

/// Get the composite value
tcompo_v * get_v_tcompo() const
{
	return __data.compo;
}

/// @return a detailed string of this object
std::string tostring_full() const
{
	return is_compo() ? get_v_tcompo()->tostring_full() : tostring_values();
}

/// @return a brief string of this object
std::string tostring_abbr() const
{
	return is_compo() ? get_v_tcompo()->tostring_abbr() : tostring_values();
}

/// @return a copy of this object
tobj copy()
{
	return is_compo() ? tobj(get_v_tcompo()->copy()) : *this;
}

/// @return a boolean of Is this object identical to `v`
bool identical(const tobj & v) const
{
	switch (__type) {
	case tnil:
		return false;
	case tbool:
		switch (v.get_type()) {
		case tnil:
			return false;
		case tbool:
			return get_v_tbool() == v.get_v_tbool();
		case tint:
			return get_v_tbool() == v.get_v_tint();
		case tdouble:
			return get_v_tbool() == v.get_v_tdouble();
		case tcompo:
			return false;
		}
		break;
	case tint:
		switch (v.get_type()) {
		case tnil:
			return false;
		case tbool:
			return get_v_tint() == v.get_v_tbool();
		case tint:
			return get_v_tint() == v.get_v_tint();
		case tdouble:
			return get_v_tint() == v.get_v_tdouble();
		case tcompo:
			return false;
		}
		break;
	case tdouble:
		switch (v.get_type()) {
		case tnil:
			return false;
		case tbool:
			return get_v_tdouble() == v.get_v_tbool();
		case tint:
			return get_v_tdouble() == v.get_v_tint();
		case tdouble:
			return get_v_tdouble() == v.get_v_tdouble();
		case tcompo:
			return false;
		}
		break;
	case tcompo:
		switch (v.get_type()) {
		case tnil:
			return false;
		case tbool:
			return false;
		case tint:
			return false;
		case tdouble:
			return false;
		case tcompo:
			return get_v_tcompo()->identical(v.get_v_tcompo());
		}
		break;
	}
	return false;
}

};


/// Two objects as a pair where two `datas` and `types` are maintained
class tobj_pair
{
private:
	tobj __first;
	tobj __second;

public:
/// Constructor: nil and nil pair in default
tobj_pair() {}

/// Constructor: `first` and `second` pair
tobj_pair(const tobj & first, const tobj & second)
{
	__first = first;
	__second = second;

	if (__first.get_type() == tcompo)
		first.get_v_tcompo()->add_refctr();
	if (__second.get_type() == tcompo)
		second.get_v_tcompo()->add_refctr();
}

/// Deconstructor (virtual)
virtual ~tobj_pair()
{
	get_first().ddc_ref_clear();
	get_second().ddc_ref_clear();
}

/// Get the type of first element
ttypes get_first_type() const
{
	return __first.get_type();
}

/// Get the type of second element
ttypes get_second_type() const
{
	return __second.get_type();
}

/// Get the first element
tobj get_first() const
{
	return __first;
}

/// Get the second element
tobj get_second() const
{
	return __second;
}

/// Set the first element to be `v`
void set_first(const tobj & v)
{
	if (__first.get_type() == tcompo)
		get_first().ddc_ref_clear();
	if (v.get_type() == tcompo)
		v.get_v_tcompo()->add_refctr();
	__first.set_v(v);
}

/// Set the second element to be `v`
void set_second(const tobj & v)
{
	if (__second.get_type() == tcompo)
		get_second().ddc_ref_clear();
	if (v.get_type() == tcompo)
		v.get_v_tcompo()->add_refctr();
	__second.set_v(v);
}

/// Generate a string of the pair (`first` : `second`)
std::string tostring_pair() const
{
	std::string is;
	is += get_first().tostring_abbr();
	is += " : ";
	is += get_second().tostring_abbr();
	return is;
}

};


/// Wrapper of array of tobjs
class tobj_array
{
private:
	tobj * __objlst;
	uint_size_obj __objlst_cap;
	uint_size_obj __objlst_len;

public:
tobj_array(uint_size_obj objlst_cap)
{
	__objlst_cap = objlst_cap;
	__objlst_len = 0;
	__objlst = new tobj[objlst_cap];
}

virtual
~tobj_array()
{
	// Clean object list
	for (uint_size_obj i = 0; i<__objlst_cap; i++) {
		tobj * iter = __objlst + i;
		iter->ddc_ref_clear();
	}
	delete [] __objlst;
}

/// Try to expand the object array if the room is not enough.
/// Old values in the object lists are copied.
void try_expand_objlist(uint_size_obj obj_max)
{
	if (__objlst_cap < obj_max) {
		tobj * new_objlst = new tobj[obj_max];
		std::copy(__objlst, __objlst + __objlst_len, new_objlst);
		delete [] __objlst;
		__objlst = new_objlst;
		__objlst_cap = obj_max;
	}
}

/// @return the Capability of current object array
uint_size_obj get_objlst_cap()
{
	return __objlst_cap;
}

/// @return the length of current object array
uint_size_obj get_objlst_len() const
{
	return __objlst_len;
}

/// @return the location of the composition object `pv`
uint_size_obj get_ref_obj_loc(tcompo_v * pv) const
{
	for (uint16_t i = 0; i<__objlst_len; i++) {
		tobj vi = __objlst[i];

		if (vi.get_type() == tcompo && vi.get_v_tcompo() == pv)
			return i;
	}
	return __objlst_len;
}

/// Decleare an object
void add_obj(uint_size_cst nameloc = UNDEF_NAMELOC)
{
	__objlst[__objlst_len].set_name_loc(nameloc);
	__objlst_len++;
}

/// Delete an object
void del_obj()
{
	__objlst_len--;
	__objlst[__objlst_len].ddc_ref_clear();
}

/// Delete `n` objects
void del_obj(uint_size_obj n)
{
	while (n > 0) {
		del_obj();
		n--;
	}
}

/// Set the length of object array
void set_objlst_len(uint_size_obj nleft)
{
	__objlst_len = nleft;
}

bool has_obj(uint_size_cst nameloc)
{
	for (uint16_t i = 0; i<__objlst_len; i++) {
		tobj vi = __objlst[i];

		if (vi.get_name_loc() == nameloc)
			return true;
	}
	return false;
}

/// @return the object located at `n`
tobj & get_obj(uint_size_obj n)
{
	return __objlst[n];
}

/// Set the object at `loc` to be `v`
void set_obj(uint_size_obj loc, const tobj & v)
{
	ttypes vtype = v.get_type();
	tobj & vloc = get_obj(loc);

	if (vtype == tnil)
		twarn(ErrRuntime_AssignNil).warn("tobj_array::set_obj", "");
	if (vtype == tcompo) {
		// check self assigment: a = a
		if (vloc.get_type() == tcompo && vloc.get_v_tcompo() == v.get_v_tcompo())
			return;
		v.get_v_tcompo()->add_refctr();
	}
	vloc.ddc_ref_clear();
	vloc.set_v(v);
}

};


/// Pair in Tap. Created by `v1 : v2` (pair expression)
class tpair : public tcompo_v, public tobj_pair
{
public:
/// Constructor
tpair(const tobj & first, const tobj & second) : tobj_pair(first, second) {}

/// Deconstructor
~tpair() {}

/// Generate a string "Pair"
const char * get_type() const
{
	 return "Pair";
}

/// Generate a copy of this object
tpair * copy()
{
	return new tpair(get_first(), get_second());
}

/// @return the type code of composite value pair
tcompo_type get_compo_type_code() const
{
	return compo_tpair;
}

/// @return 2 - the length of pair object
long len() const
{
	return 2;
}

/// @return a brief string of this pair object
std::string tostring_abbr() const
{
	return tostring_pointer(get_type(), this);
}

/// @return a detailed string of this pair object
std::string tostring_full() const
{
	return tostring_pair();
}

/// @return a boolean of whether this is identical to `v`
bool identical(tcompo_v * v) const
{
	if (v == nullptr)
		return false;
	if (v->get_compo_type_code() != compo_tpair)
		return false;
	tpair * pair = reinterpret_cast<tpair *>(v);

	return get_first().identical(pair->get_first())
		&& get_second().identical(pair->get_second());
}

/// Indexing the element by 0 or 1
void idx(const tobj * params, uint_size_stk np, tobj& vre)
{
	if (np != 1)
		twarn(ErrRuntime_ParamsCtr).warn("tpair::idx", "1 parameter");
	if (params->get_type() != tint)
		twarn(ErrRuntime_ParamsType).warn("tpair::idx", "Should be 'tint'");
	long idxi = params->get_v_tint();

	if (idxi == 0)
		vre = get_first();
	else if (idxi == 1)
		vre = get_second();
	else twarn(ErrRuntime_IdxOutRange).warn("tpair::idx", "");
}

/// Set the elements by indexing of 0 or 1
void iset(const tobj * params, uint_size_stk np, const tobj & v)
{
	if (np != 1)
		twarn(ErrRuntime_ParamsCtr).warn("tpair::idx", "1 parameter");
	if (params->get_type() != tint)
		twarn(ErrRuntime_ParamsType).warn("tpair::idx", "Should be 'tint'");
	if (params->get_v_tint() != 0 && params->get_v_tint() != 1)
		twarn(ErrRuntime_IdxOutRange).warn("tpair::idx", "");

	if (params->get_v_tint() == 0)
		set_first(v);
	else // params->get_v_tint() == 1
		set_second(v);
}

};


/*===========================================================================*
 * 2. Algorithmic Operators
 *===========================================================================*/

/// Virtual class for `operator_add` and `operator_radd`
class top_add
{
public:
virtual ~top_add() {};
virtual void operator_add(const tobj & v, tobj & vre) = 0;  ///< this + v
virtual void operator_radd(const tobj & v, tobj & vre) = 0; ///< v + this
};

/// Virtual class for `operator_sub` and `operator_rsub`
class top_sub
{
public:
virtual ~top_sub() {};
virtual void operator_sub(const tobj & v, tobj & vre) = 0;  ///< this - v
virtual void operator_rsub(const tobj & v, tobj & vre) = 0; ///< v - this
};

/// Virtual class for `operator_mul` and `operator_rmul`
class top_mul
{
public:
virtual ~top_mul() {};
virtual void operator_mul(const tobj & v, tobj & vre) = 0;  ///< this * v
virtual void operator_rmul(const tobj & v, tobj & vre) = 0; ///< v * this
};

/// Virtual class for `operator_div` and `operator_rdiv`
class top_div
{
public:
virtual ~top_div() {};
virtual void operator_div(const tobj & v, tobj & vre) = 0;  ///< this / v
virtual void operator_rdiv(const tobj & v, tobj & vre) = 0; ///< v / this
};

/// Virtual class for `operator_mod` and `operator_rmod`
class top_mod
{
public:
virtual ~top_mod() {};
virtual void operator_mod(const tobj & v, tobj & vre) = 0;  ///< this % v
virtual void operator_rmod(const tobj & v, tobj & vre) = 0; ///< v % this
};

/// Virtual class for `operator_mmul` and `operator_rmmul`
class top_mmul
{
public:
virtual ~top_mmul() {};
virtual void operator_mmul(const tobj & v, tobj & vre) = 0;  ///< this @ v
virtual void operator_rmmul(const tobj & v, tobj & vre) = 0; ///< v @ this
};

/// Virtual class for `operator_pow` and `operator_rpow`
class top_pow
{
public:
virtual ~top_pow() {};
virtual void operator_pow(const tobj & v, tobj & vre) = 0;  ///< this^v
virtual void operator_rpow(const tobj & v, tobj & vre) = 0; ///< v^this
};

/// A wrapper of operator +
template <class Tre, class Tleft, class Tright>
Tre wrapper_add(const Tleft & vleft, const Tright & vright)
{
	return vleft + vright;
}

/// A wrapper of operator -
template <class Tre, class Tleft, class Tright>
Tre wrapper_sub(const Tleft & vleft, const Tright & vright)
{
	return vleft - vright;
}

/// A wrapper of operator *
template <class Tre, class Tleft, class Tright>
Tre wrapper_mul(const Tleft & vleft, const Tright & vright)
{
	return vleft * vright;
}

/// A wrapper of operator /
template <class Tre, class Tleft, class Tright>
Tre wrapper_div(const Tleft & vleft, const Tright & vright)
{
	return vleft / vright;
}

/// A wrapper of operator / for integer division
inline long wrapper_div_int(const long & v1, const long & v2)
{
	if (v2 == 0)
		twarn(ErrRuntime_DivIntZero).warn("wrapper_div_int", "");
	return v1 / v2;
}


/*===========================================================================*
 * 3. Logical Operators
 *===========================================================================*/

/// Virtual class for `operator_eq` and `operator_req`
class top_eq
{
public:
virtual ~top_eq() {};
virtual void operator_eq(const tobj & v, tobj & vre) = 0;  ///< this == v
virtual void operator_req(const tobj & v, tobj & vre) = 0; ///< v == this
};

/// Virtual class for `operator_ne` and `operator_rne`
class top_ne
{
public:
virtual ~top_ne() {};
virtual void operator_ne(const tobj & v, tobj & vre) = 0;  ///< this != v
virtual void operator_rne(const tobj & v, tobj & vre) = 0; ///< v != this
};

/// Virtual class for `operator_sg` and `operator_rsg`
class top_sg
{
public:
virtual ~top_sg() {};
virtual void operator_sg(const tobj & v, tobj & vre) = 0;  ///< this > v
virtual void operator_rsg(const tobj & v, tobj & vre) = 0; ///< v > this
};

/// Virtual class for `operator_sl` and `operator_rsl`
class top_sl
{
public:
virtual ~top_sl() {};
virtual void operator_sl(const tobj & v, tobj & vre) = 0;  ///< this < v
virtual void operator_rsl(const tobj & v, tobj & vre) = 0; ///< v < this
};

/// Virtual class for `operator_ge` and `operator_rge`
class top_ge
{
public:
virtual ~top_ge() {};
virtual void operator_ge(const tobj & v, tobj & vre) = 0;  ///< this >= v
virtual void operator_rge(const tobj & v, tobj & vre) = 0; ///< v >= this
};

/// Virtual class for `operator_le` and `operator_rle`
class top_le
{
public:
virtual ~top_le() {};
virtual void operator_le(const tobj & v, tobj & vre) = 0;  ///< this <= v
virtual void operator_rle(const tobj & v, tobj & vre) = 0; ///< v <= this
};

/// Virtual class for `operator_and` and `operator_rand`
class top_and
{
public:
virtual ~top_and() {};
virtual void operator_and(const tobj & v, tobj & vre) = 0;  ///< this and v
virtual void operator_rand(const tobj & v, tobj & vre) = 0; ///< v and this
};

/// Virtual class for `operator_or` and `operator_ror`
class top_or
{
public:
virtual ~top_or() {};
virtual void operator_or(const tobj & v, tobj & vre) = 0;  ///< this or v
virtual void operator_ror(const tobj & v, tobj & vre) = 0; ///< v or this
};

/// wrapper of operator >
template <class Tre, class Tleft, class Tright>
Tre wrapper_sg(const Tleft & vleft, const Tright & vright)
{
	return vleft > vright;
}

/// wrapper of operator <
template <class Tre, class Tleft, class Tright>
Tre wrapper_sl(const Tleft & vleft, const Tright & vright)
{
	return vleft < vright;
}

/// wrapper of operator >=
template <class Tre, class Tleft, class Tright>
Tre wrapper_ge(const Tleft & vleft, const Tright & vright)
{
	return vleft >= vright;
}

/// wrapper of operator <=
template <class Tre, class Tleft, class Tright>
Tre wrapper_le(const Tleft & vleft, const Tright & vright)
{
	return vleft <= vright;
}

/// wrapper of operator ==
template <class Tre, class Tleft, class Tright>
Tre wrapper_eq(const Tleft & vleft, const Tright & vright)
{
	return vleft == vright;
}

/// wrapper of operator !=
template <class Tre, class Tleft, class Tright>
Tre wrapper_ne(const Tleft & vleft, const Tright & vright)
{
	return vleft != vright;
}

/// wrapper of operator &&
template <class Tre, class Tleft, class Tright>
Tre wrapper_and(const Tleft & vleft, const Tright & vright)
{
	return vleft && vright;
}

/// wrapper of operator ||
template <class Tre, class Tleft, class Tright>
Tre wrapper_or(const Tleft & vleft, const Tright & vright)
{
	return vleft || vright;
}


/*===========================================================================*
 * 4. Traits
 *===========================================================================*/

/// Basic indexing methods in Tap
class tcompo_idx
{
public:
virtual ~tcompo_idx() {};
/// arr[params]
virtual void idx(const tobj * params, uint_size_stk np, tobj& vre) = 0;
/// arr[params] = v
virtual void iset(const tobj * params, uint_size_stk np, const tobj& v) = 0;
};

/// Basic iteration methods in Tap
class tcompo_iter
{
public:
virtual ~tcompo_iter() {};
virtual void get_v_at_loc(tobj & vre) = 0;  ///< vre = iterator[loc]
virtual bool next() = 0;                    ///< loc = loc + step
virtual bool in(const tobj & e) = 0;        ///< e in iterator
virtual void iter_restore() = 0;            ///< loc = 0
};

/// Iterator in Tap. Created by `v1 to v2` (to expression)
class titer : public tcompo_v
{
private:
	long __loc;
	long __start;
	long __middle;
	long __end;

public:
/// Constructor of iterator (start to end by middle)
titer(const long start, const long middle, const long end)
{
	if (middle == 0)
		twarn(ErrRuntime_RefType).warn("titer::titer", "Step cannot be 0");
	__start = start;
	__middle = middle;
	__end = end + middle;
	__loc = start;
}

/// Constructor of iterator (start to end by 1)
titer(const long start, const long end)
{
	__start = start;
	__middle = 1;
	__end = end + 1;
	__loc = start;
}

/// Deconstructor
~titer() {}

/// @return the Start location
long get_start() const
{
	return __start;
}

/// @return the Step length
long get_middle() const
{
	return __middle;
}

/// @return the Ending location
long get_end() const
{
	return __end - __middle;
}

/// @return a copy of this iterator
titer * copy()
{
	return new titer(__start, __end);
}

/// @return a string 'Iterator'
const char * get_type() const
{
	return "Iterator";
}

/// @return the type code of composite value `compo_titer`
tcompo_type get_compo_type_code() const
{
	return compo_titer;
}

/// @return the length of iterator (how many times of iterating)
long len() const
{
	long gap = __end - __middle - __start;
	return gap / __middle + ((gap % __middle) > 0);
}

/// @return a brief string of this iterator
std::string tostring_abbr() const
{
	return tostring_pointer(get_type(), this);
}

/// @return a detailed string of this iterator
std::string tostring_full() const
{
	std::string is;
	is += std::to_string(__start);
	is += " to ";
	is += std::to_string(__end - __middle);
	is += " (by ";
	is += std::to_string(__middle);
	is += ")";
	return is;
}

/// Restore __loc to the start location
void iter_restore()
{
	__loc = __start;
}

/// @return the index of __loc
long get_locidx()
{
	if (__middle > 0)
		return __loc - __middle >= __start ? __loc - __middle : __start;
	else
		return __loc - __middle <= __start ? __loc - __middle : __start;
}

void get_v_at_loc(tobj & vre)
{
	vre.set_v(get_locidx());
}

/// Let __loc move to the next index
bool next()
{
	__loc += __middle;
	return __middle > 0 ? __loc < __end : __loc > __end;
}

/// @return a boolean Is `e` in iterator
bool in(const tobj & e)
{
	if (e.get_type() != tint)
		return false;

	long idx = e.get_v_tint();
	return __middle > 0 ? __start <= idx && idx < __end
						: __end   < idx  && idx < __start;
}

/// @return a boolean is this iterator identical to `v`
bool identical(tcompo_v * v) const
{
	if (v == nullptr)
		return false;
	if (v->get_compo_type_code() != compo_titer)
		return false;

	titer * iter = reinterpret_cast<titer *>(v);
	long start = iter->get_start();
	long middle = iter->get_middle();
	long end = iter->get_end();
	return __start == start && __middle == middle && __end == end;
}

};


/*===========================================================================*
 * 5. C++ Functions
 *===========================================================================*/

/// C++ General Functions
typedef void (* cppf)(tobj * const, uint_size_stk, tobj &);

/// A wrapper of C++ General Functions
class tcppgenf : public tcompo_v
{
private:
	cppf __f;
	const char * __name;
	uint_size_stk __nparams;

public:
/// Constructor: a wrapper of cppf `f`
tcppgenf(const cppf & f, const char * name, uint_size_stk nparams)
{
	__f = f; __name = name; __nparams = nparams;
}

/// Deconstructor
~tcppgenf() {}

/// @return the cppf __f
cppf & get_f()
{
	return __f;
}

/// @return the number of parameters
uint_size_stk get_nparams() const
{
	return __nparams;
}

/// @return a brief string of this cppf object
std::string tostring_abbr() const
{
	return tostring_pointer(get_type(), this);
}

/// @return a detailed string of this cppf object
std::string tostring_full() const
{
	return tostring_abbr();
}

/// @return a copy of this cppf object
tcppgenf * copy()
{
	return new tcppgenf(__f, __name, __nparams);
}

/// @return a string of type of this cppf object "C++ General Function"
const char * get_type() const
{
	return "C++ General Function";
}

/// @return the type code of this composite object `compo_cppfunc`
tcompo_type get_compo_type_code() const
{
	return compo_cppfunc;
}

/// @return 0 - the length of this object
long len() const
{
	return 0;
}

/// @return a boolean Is this is identical to `v`
bool identical(tcompo_v * v) const
{
	return v == this;
}

/// @return the name of this object
std::string get_name() const
{
	return __name;
}

};

}

#endif // TRTS_H
