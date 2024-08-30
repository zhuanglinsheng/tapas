#ifndef TVM_H
#define TVM_H

#include "tenv.h"
#include "extensions/teigen.h"

namespace tapas
{

/*===========================================================================*
 * 1. Operators as generators
 *===========================================================================*/

/// C++ function of binary operations (v1, v2, vre)
typedef void (* binopf)(const tobj &, const tobj &, tobj &);

/// operator pair is the generator of tapas::tpair
///     Form a pair `a : b`
inline void operator_pair(const tobj & v1, const tobj & v2, tobj & vre)
{
	tpair * pair = new tpair(v1, v2);
	vre.ddc_ref_clear();
	vre.set_v(pair);
}

/// operator to is the generator of tapas::iter
///     Form an iterator `1 to 100`
inline void operator_to(const tobj & v1, const tobj & v2, tobj & vre)
{
	if (v1.get_type() != tint || v2.get_type() != tint)
		twarn(ErrRuntime_ParamsType).warn("operator_to", "");
	titer * iter = new titer(v1.get_v_tint(), v2.get_v_tint());
	vre.ddc_ref_clear();
	vre.set_v(iter);
}

/// Check whether v1 is in v2 and return a boolean to vre.
///     v2 should be iterable
template<class T>
inline void operator_in_basic(const tobj & v1, T * v2_iterable, tobj & vre)
{
	vre.ddc_ref_clear();
	vre.set_v(v2_iterable->in(v1));
}

/// operator in is an interface provided by tapas::tcompo_iter
inline void operator_in(const tobj & v1, const tobj & v2, tobj & vre)
{
	if (v2.get_type() == tcompo)
	{
		tcompo_v * v2_compo_v = v2.get_v_tcompo();

		if (v2_compo_v->get_compo_type_code() == compo_titer)
		{
			titer * v2_titer = reinterpret_cast<titer *>(v2_compo_v);
			operator_in_basic<titer>(v1, v2_titer, vre);
			return;  // return if passing through
		}
		if (v2_compo_v->get_compo_type_code() == compo_tlist)
		{
			tlist * v2_tlist = reinterpret_cast<tlist *>(v2_compo_v);
			operator_in_basic<tlist>(v1, v2_tlist, vre);
			return;  // return if passing through
		}
		tcompo_iter * v2_iterable = dynamic_cast<tcompo_iter *>(v2_compo_v);

		if (v2_iterable != nullptr)
		{
			operator_in_basic<tcompo_iter>(v1, v2_iterable, vre);
			return;  // return if passing through
		}
	}
	vre.ddc_ref_clear();
	vre.set_v(false);
}


/*===========================================================================*
 * 2. Algorithmic operators
 *===========================================================================*/

/// Exeute '(p->*opf)(v, vre)'. (Taking care of the ctr of vre)
template<class T>
inline void op_compo_basic(const tobj & v, tobj & vre, T * p, void (T::*opf)(const tobj &, tobj &))
{
	tobj vre_tmp; (p->*opf)(v, vre_tmp);
	vre.try_clear(); vre = vre_tmp;
}

/// Excute 'op_compo_basic(v2, vre, v1, T::f1)' or
///        'op_compo_basic(v1, vre, v2, T::f2)'.
template<class T>
inline void op_compo_extended(const std::string & fname,
				const std::string & op,
				ttypes type_v1,
				ttypes type_v2,
				const tobj & v1,
				const tobj & v2,
				tobj & vre,
				void (T::*f1)(const tobj &, tobj &),
				void (T::*f2)(const tobj &, tobj &))
{
	if (type_v1 != tcompo && type_v2 != tcompo)
		twarn(ErrRuntime_ParamsType).warn(fname, v1.tostring_full() + op + v2.tostring_full());
	if (type_v1 == tcompo) {
		tcompo_v * v1_compo = v1.get_v_tcompo();
		T * p = dynamic_cast<T *>(v1_compo);

		if (p != nullptr) {
			op_compo_basic(v2, vre, p, f1);
			return;
		} else
			twarn(ErrRuntime_ParamsType).warn(fname, v1.tostring_full() + op + v2.tostring_full());
	}
	if (type_v2 == tcompo) {
		tcompo_v * v2_compo = v2.get_v_tcompo();
		T * p = dynamic_cast<T *>(v2_compo);

		if (p != nullptr) {
			op_compo_basic(v1, vre, p, f2);
			return;
		} else
			twarn(ErrRuntime_ParamsType).warn(fname, v1.tostring_full() + op + v2.tostring_full());
	}
}

/// Numerical add, sub, mul and div calculations
inline bool bin_algo_ops_value_type(ttypes type1, ttypes type2,
			const tobj & v1, const tobj & v2, tobj & vre,
			long (*f1)(const long &, const long &),
			double (*f2)(const long &, const double &),
			double (*f3)(const double &, const long &),
			double (*f4)(const double &, const double &))
{
	if (type1 == tint && type2 == tint) {
		long re = f1(v1.get_v_tint(), v2.get_v_tint());
		vre.ddc_ref_clear();
		vre.set_v(re);
		return 1;
	}
	if (type1 == tint && type2 == tdouble) {
		double re = f2(v1.get_v_tint(), v2.get_v_tdouble());
		vre.ddc_ref_clear();
		vre.set_v(re);
		return 1;
	}
	if (type1 == tdouble && type2 == tint) {
		double re = f3(v1.get_v_tdouble(), v2.get_v_tint());
		vre.ddc_ref_clear();
		vre.set_v(re);
		return 1;
	}
	if (type1 == tdouble && type2 == tdouble) {
		double re = f4(v1.get_v_tdouble(),v2.get_v_tdouble());
		vre.ddc_ref_clear();
		vre.set_v(re);
		return 1;
	}
	return 0;
}

/// operator add (+) is an interface of tapas::talgop_add
inline void operator_add(const tobj & v1, const tobj & v2, tobj & vre)
{
	ttypes type_v1 = v1.get_type();
	ttypes type_v2 = v2.get_type();

	if (!bin_algo_ops_value_type(type_v1, type_v2, v1, v2, vre,
					&(wrapper_add<long, long, long>),
					&(wrapper_add<double, long, double>),
					&(wrapper_add<double, double, long>),
					&(wrapper_add<double, double, double>)))
		op_compo_extended("operator_add", " + ", type_v1, type_v2, v1, v2, vre,
					&top_add::operator_add, &top_add::operator_radd);
}

/// operator sub (-) is an interface of tapas::talgop_sub
inline void operator_sub(const tobj & v1, const tobj & v2, tobj & vre)
{
	ttypes type_v1 = v1.get_type();
	ttypes type_v2 = v2.get_type();

	if (!bin_algo_ops_value_type(type_v1, type_v2, v1, v2, vre,
					&(wrapper_sub<long, long, long>),
					&(wrapper_sub<double, long, double>),
					&(wrapper_sub<double, double, long>),
					&(wrapper_sub<double, double, double>)))
		op_compo_extended("operator_sub", " - ", type_v1, type_v2, v1, v2, vre,
					&top_sub::operator_sub, &top_sub::operator_rsub);
}

/// operator mul (*) is an interface of tapas::talgop_mul
inline void operator_mul(const tobj & v1, const tobj & v2, tobj & vre)
{
	ttypes type_v1 = v1.get_type();
	ttypes type_v2 = v2.get_type();

	if (!bin_algo_ops_value_type(type_v1, type_v2, v1, v2, vre,
					&(wrapper_mul<long, long, long>),
					&(wrapper_mul<double, long, double>),
					&(wrapper_mul<double, double, long>),
					&(wrapper_mul<double, double, double>)))
		op_compo_extended("operator_mul", " * ", type_v1, type_v2, v1, v2, vre,
					&top_mul::operator_mul, &top_mul::operator_rmul);
}

/// operator div (/) is an interface of tapas::talgop_div
inline void operator_div(const tobj & v1, const tobj & v2, tobj & vre)
{
	ttypes type_v1 = v1.get_type();
	ttypes type_v2 = v2.get_type();

	if (!bin_algo_ops_value_type(type_v1, type_v2, v1, v2, vre,
					&(wrapper_div_int),
					&(wrapper_div<double, long, double>),
					&(wrapper_div<double, double, long>),
					&(wrapper_div<double, double, double>)))
		op_compo_extended("operator_div", " / ", type_v1, type_v2, v1, v2, vre,
					&top_div::operator_div, &top_div::operator_rdiv);
}

/// Numerical pow, mod and mmul calculations
inline bool bin_libfcall_value_types(ttypes type1, ttypes type2,
					const tobj & v1, const tobj & v2, tobj & vre,
					double (*f)(double, double))
{
	if (type1 == tint && type2 == tint) {
		double re = f(v1.get_v_tint(), v2.get_v_tint());
		vre.ddc_ref_clear();
		vre.set_v(re);
		return 1;
	}
	if (type1 == tint && type2 == tdouble) {
		double re = f(static_cast<double>(v1.get_v_tint()), v2.get_v_tdouble());
		vre.ddc_ref_clear();
		vre.set_v(re);
		return 1;
	}
	if (type1 == tdouble && type2 == tint) {
		double re = f(v1.get_v_tdouble(),static_cast<double>(v2.get_v_tint()));
		vre.ddc_ref_clear();
		vre.set_v(re);
		return 1;
	}
	if (type1 == tdouble && type2 == tdouble) {
		double re = f(v1.get_v_tdouble(),v2.get_v_tdouble());
		vre.ddc_ref_clear();
		vre.set_v(re);
		return 1;
	}
	return 0;
}

/// operator mod (%) is an interface of tapas::talgop_mod
inline void operator_mod(const tobj & v1, const tobj & v2, tobj & vre)
{
	ttypes type1 = v1.get_type();
	ttypes type2 = v2.get_type();

	if (!bin_libfcall_value_types(type1, type2, v1, v2, vre, fmod))
		op_compo_extended("operator_mod", " % ", type1, type2, v1, v2, vre,
					&top_mod::operator_mod, &top_mod::operator_rmod);
}

/// operator pow (^) is an interface of tapas::talgop_pow
inline void operator_pow(const tobj & v1, const tobj & v2, tobj & vre)
{
	ttypes type1 = v1.get_type();
	ttypes type2 = v2.get_type();

	if (!bin_libfcall_value_types(type1, type2, v1, v2, vre, pow))
		op_compo_extended("operator_pow", " ^ ", type1, type2, v1, v2, vre,
					&top_pow::operator_pow, &top_pow::operator_rpow);
}

/// operator mmul (@) is an interface of tapas::talgop_mmul
inline void operator_mmul(const tobj & v1, const tobj & v2, tobj & vre)
{
	ttypes type_v1 = v1.get_type();
	ttypes type_v2 = v2.get_type();
	op_compo_extended("operator_mmul", " @ ", type_v1, type_v2, v1, v2, vre,
				&top_mmul::operator_mmul, &top_mmul::operator_rmmul);
}


/*===========================================================================*
 * 3. Logical operators
 *===========================================================================*/

/// operator equality (==) is an interface of tapas::tapv::identical
inline void operator_eq(const tobj & v1, const tobj & v2, tobj & vre)
{
	if (v1.get_type() == tcompo) {
		tcompo_v * v1_compo = v1.get_v_tcompo();
		top_eq * p = dynamic_cast<top_eq *>(v1_compo);

		if (p != nullptr) {
			op_compo_basic(v2, vre, p, &top_eq::operator_eq);
			return;  // return if passing through
		}
	}
	if (v2.get_type() == tcompo) {
		tcompo_v * v2_compo = v2.get_v_tcompo();
		top_eq * p = dynamic_cast<top_eq *>(v2_compo);

		if (p != nullptr) {
			op_compo_basic(v1, vre, p, &top_eq::operator_req);
			return;  // return if passing through
		}
	}
	bool re = v1.identical(v2);
	vre.try_clear();
	vre.set_v(re);
}

/// operator un-equality (!=) is an interface of tapas::tapv::identical
inline void
operator_ne(const tobj & v1, const tobj & v2, tobj & vre)
{
	if (v1.get_type() == tcompo) {
		tcompo_v * v1_compo = v1.get_v_tcompo();
		top_ne * p = dynamic_cast<top_ne *>(v1_compo);

		if (p != nullptr) {
			op_compo_basic(v2, vre, p, &top_ne::operator_ne);
			return;  // return if passing through
		}
	}
	if (v2.get_type() == tcompo) {
		tcompo_v * v2_compo = v2.get_v_tcompo();
		top_ne * p = dynamic_cast<top_ne *>(v2_compo);

		if (p != nullptr) {
			op_compo_basic(v1, vre, p, &top_ne::operator_rne);
			return;  // return if passing through
		}
	}
	operator_eq(v1, v2, vre);
	vre.set_v(bool(1 - vre.get_v_tbool()));
}

/// Numerical >, <, >=, <=, ==, != calculations
inline bool bin_logi_ops_value_type(ttypes type1, ttypes type2,
			const tobj & v1, const tobj & v2, tobj & vre,
			bool (*f1)(const long &, const long &),
			bool (*f2)(const long &, const double &),
			bool (*f3)(const double &, const long &),
			bool (*f4)(const double &, const double &))
{
	if (type1 == tint && type2 == tint) {
		bool re = f1(v1.get_v_tint(), v2.get_v_tint());
		vre.ddc_ref_clear();
		vre.set_v(re);
		return 1;
	}
	if (type1 == tint && type2 == tdouble) {
		bool re = f2(v1.get_v_tint(), v2.get_v_tdouble());
		vre.ddc_ref_clear();
		vre.set_v(re);
		return 1;
	}
	if (type1 == tdouble && type2 == tint) {
		bool re = f3(v1.get_v_tdouble(), v2.get_v_tint());
		vre.ddc_ref_clear();
		vre.set_v(re);
		return 1;
	}
	if (type1 == tdouble && type2 == tdouble) {
		bool re = f4(v1.get_v_tdouble(), v2.get_v_tdouble());
		vre.ddc_ref_clear();
		vre.set_v(re);
		return 1;
	}
	return 0;
}

/// operator sg (>) is an interface of tapas::tlogop_sg
inline void operator_sg(const tobj & v1, const tobj & v2, tobj & vre)
{
	ttypes type_v1 = v1.get_type();
	ttypes type_v2 = v2.get_type();

	if (!bin_logi_ops_value_type(type_v1, type_v2, v1, v2, vre,
					&wrapper_sg<bool, long, long>,
					&wrapper_sg<bool, long, double>,
					&wrapper_sg<bool, double, long>,
					&wrapper_sg<bool, double, double>))
		op_compo_extended("operator_sg", " > ", type_v1, type_v2, v1, v2, vre,
					&top_sg::operator_sg, &top_sg::operator_rsg);
}

/// operator ge (>=) is an interface of tapas::tlogop_ge
inline void operator_ge(const tobj & v1, const tobj & v2, tobj & vre)
{
	ttypes type_v1 = v1.get_type();
	ttypes type_v2 = v2.get_type();

	if (!bin_logi_ops_value_type(type_v1, type_v2, v1, v2, vre,
					&wrapper_ge<bool, long, long>,
					&wrapper_ge<bool, long, double>,
					&wrapper_ge<bool, double, long>,
					&wrapper_ge<bool, double, double>))
		op_compo_extended("operator_ge", " >= ", type_v1, type_v2, v1, v2, vre,
					&top_ge::operator_ge, &top_ge::operator_rge);
}

/// operator sl (<) is an interface of tapas::tlogop_sl
inline void operator_sl(const tobj & v1, const tobj & v2, tobj & vre)
{
	ttypes type_v1 = v1.get_type();
	ttypes type_v2 = v2.get_type();

	if (!bin_logi_ops_value_type(type_v1, type_v2, v1, v2, vre,
					&wrapper_sl<bool, long, long>,
					&wrapper_sl<bool, long, double>,
					&wrapper_sl<bool, double, long>,
					&wrapper_sl<bool, double, double>))
		op_compo_extended("operator_sl", " < ", type_v1, type_v2, v1, v2, vre,
					&top_sl::operator_sl, &top_sl::operator_rsl);
}

/// operator le (<=) is an interface of tapas::tlogop_le
inline void operator_le(const tobj & v1, const tobj & v2, tobj & vre)
{
	ttypes type_v1 = v1.get_type();
	ttypes type_v2 = v2.get_type();

	if (!bin_logi_ops_value_type(type_v1, type_v2, v1, v2, vre,
					&wrapper_le<bool, long, long>,
					&wrapper_le<bool, long, double>,
					&wrapper_le<bool, double, long>,
					&wrapper_le<bool, double, double>))
		op_compo_extended("operator_le", " <= ", type_v1, type_v2, v1, v2, vre,
					&top_le::operator_le, &top_le::operator_rle);
}

/// operator and is an interface of tapas::tlogop_and
inline void operator_and(const tobj & v1, const tobj & v2, tobj & vre)
{
	ttypes type_v1 = v1.get_type();
	ttypes type_v2 = v2.get_type();

	if (type_v1 == tbool && type_v2 == tbool) {
		bool re = wrapper_and<bool, bool, bool>(v1.get_v_tbool(), v2.get_v_tbool());
		vre.ddc_ref_clear();
		vre.set_v(re);
		return;  // return if passing through
	}
	op_compo_extended("operator_and", " and ", type_v1, type_v2, v1, v2, vre,
				&top_and::operator_and, &top_and::operator_rand);
}

/// operator or is an interface of tapas::tlogop_or
inline void operator_or(const tobj & v1, const tobj & v2, tobj & vre)
{
	ttypes type_v1 = v1.get_type();
	ttypes type_v2 = v2.get_type();

	if (type_v1 == tbool && type_v2 == tbool) {
		bool re = wrapper_or<bool, bool, bool>(v1.get_v_tbool(), v2.get_v_tbool());
		vre.ddc_ref_clear();
		vre.set_v(re);
		return;  // return if passing through
	}
	op_compo_extended("operator_or", " -or ", type_v1, type_v2, v1, v2, vre,
				&top_or::operator_or, &top_or::operator_ror);
}


/*===========================================================================*
 * 4. Virtual machine
 *===========================================================================*/

/** The tvm class
 *  @details This class execute the bycodes on the imported vmstack from
 *           environment and get the returned value.
 */
class tvm : tobj_array
{
private:
	/// Runtime: maximum registers in vmstack
	uint_size_reg __regmax;

	/// Runtime: not created by tvm
	tobj * __stk;
	uint_size_reg __stklen;

	/// Runtime: returned value
	tobj __rev;

	/// Runtime: in the layer of loops
	uint_size_reg __inloop;

	/// Compilation
	std::vector<std::string> __paths;

/// Remove the contents of __rev without cleaning it
void set_rev_empty()
{
	__rev.set_nil();
}

/// Get the length of vmstack.
uint_size_reg vmstk_size() const
{
	return __stklen;
}

/// Get the element of vmstack at loc
tobj & vmstk_at(uint_size_reg loc)
{
	return __stk[__stklen - loc - 1];
}

/// Get the pointer of vmstack since n
tobj * vmstk_get_top_n(uint_size_reg n)
{
	return __stk + __stklen - n;
}

/// Get the first element of vmstack
tobj & vmstk_top()
{
	return __stk[__stklen - 1];
}

/// Get the free space above the first element of vmstack
tobj & topfree_rv()
{
	return __stk[__stklen];
}

/// Mark that the free space above the first element of vmstack is filled
void topfree_filled()
{
	__stklen++;
}

/// Pop the first element of vmstack without cleaning it.
void vmstk_pop_front()
{
	__stklen --;
}

/// Pop the first element of vmstack and clean it.
void vmstk_pop_clean_front()
{
	vmstk_top().try_clear(); __stklen --;
}

/// Pop the first n elements of vmstack and clean them.
void vmstk_pop_clean_front_n(uint_size_reg n)
{
	for (uint_size_reg i = 0; i<n; i++)
		vmstk_pop_clean_front();
}

/// Push v to the top of vmstack.
void vmstk_push_front_v(const tobj & v)
{
	__stk[__stklen] = v; __stklen++;
}

/// Copying environment (not applicable to libraries)
void copy_env(tcompo_env * env_basic, tobj & rev)
{
	switch (env_basic->tenv_get_compo_type()) {
	case compo_tfunc:
		rev.set_v(static_cast<tfunc *>(env_basic)->copy());
		break;
	case compo_tlib:
		twarn(ErrRuntime_RefType).warn("tvm::copy_env", "");
		break;
	default:
		tcompo_v * env_v = nullptr;

		if (nullptr == (env_v = dynamic_cast<tcompo_v *>(env_basic)))
			twarn(ErrRuntime_RefType).warn("tvm::copy_env", "");
		rev.set_v(env_v->copy());
	}
}

/// OP_IDXR
void parse_idx(const uint_size_reg nparams)
{
	tobj & obj = vmstk_top();
	vmstk_pop_front();

	if (obj.get_type() != tcompo)
		twarn(ErrRuntime_RefType).warn("tvm::parse_idx", "");
	tcompo_v * arr = obj.get_v_tcompo();

	switch (arr->get_compo_type_code()) {
	case compo_tstr:
		parse_idx_basic(nparams, reinterpret_cast<tstr *>(arr), obj);
		break;
	case compo_tdict:
		parse_idx_basic(nparams, reinterpret_cast<tdict *>(arr), obj);
		break;
	case compo_tlist:
		parse_idx_basic(nparams, reinterpret_cast<tlist *>(arr), obj);
		break;
	case compo_tpair:
		parse_idx_basic(nparams, reinterpret_cast<tpair *>(arr), obj);
		break;
	case compo_tlib:
		parse_idx_basic(nparams, reinterpret_cast<tlib *>(arr), obj);
		break;
	case compo_tbarr:
		parse_idx_basic(nparams, reinterpret_cast<tbarr *>(arr), obj);
		break;
	case compo_tdarr:
		parse_idx_basic(nparams, reinterpret_cast<tdarr *>(arr), obj);
		break;
	default:
		tcompo_idx * ptype = nullptr;
		if (nullptr == (ptype = dynamic_cast<tcompo_idx *>(arr)))
			twarn(ErrRuntime_RefType).warn("tvm::parse_idx", "Un-indexable");
		parse_idx_basic(nparams, ptype, obj);
	}
}

/// Push to the top of vmstack the value of 'arr[idxs]' where
/// idxs are top nparams of vmstack
template<typename T>
void parse_idx_basic(const uint_size_reg nparams, T * arr, tobj & obj)
{
	tobj * params = vmstk_get_top_n(nparams);
	arr->idx(params, nparams, __rev);
	vmstk_push_front_v(obj);
	vmstk_pop_clean_front_n(1 + nparams);
	vmstk_push_front_v(__rev);
	set_rev_empty();
}

/// OP_EVAL
void parse_eval(tbycode * iter, tcompo_env * env)
{
	uint_size_reg nparams = static_cast<uint_size_reg>(iter->get_U());
	tobj & obj = vmstk_top();
	tobj * params = this->vmstk_get_top_n(nparams + 1);

	if (obj.get_type() != tcompo)
		twarn(ErrRuntime_RefType).warn("tvm::parse_eval", "");
	tcompo_v * v = obj.get_v_tcompo();

	switch (v->get_compo_type_code()) {
	case compo_tfunc:
		parse_eval_tf(v, params, nparams, env);
		break;
	case compo_cppfunc:
		reinterpret_cast<tcppgenf *>(v)->get_f()(params, nparams, __rev);
		iter->set_ins(OP_EVALCF); // cppfunc cannot be re-assigned values
		break;
	case compo_sessfunc:
		reinterpret_cast<tcppsessf *>(v)->get_f()(params, nparams, __rev, env);
		iter->set_ins(OP_EVALSF); // sessfunc cannot be re-assigned values
		break;
	default: ;
	}
	vmstk_pop_clean_front_n(1 + nparams);
	vmstk_push_front_v(__rev);
	set_rev_empty();
}

/// OP_EVALTF (Not yet)
void parse_eval_tf(tcompo_v * v, tobj * params, uint_size_reg nparams, tcompo_env * env)
{
	tfunc * f = reinterpret_cast<tfunc *>(v);
	// check environment tree looping (recursion)
	if (f == env)
		twarn(ErrRuntime_EnvInconsis).warn("tvm::eval_tfunc", "");
	// check parameter number
	if (f->get_nparams() != UNDEF_NPARAMS && nparams != f->get_nparams())
		twarn(ErrRuntime_ParamsCtr).warn("tvm::eval_tfunc", "1");
	// create new virtual machine
	tvm new_vm(f->get_tmpmax());
	new_vm.set_vmstack(f->get_vmstack(), f->get_regmax());
	// execution
	f->assign_params(params, nparams);
	new_vm.exec_tins(f->get_cmdloc(), f->get_ncmds(), f);
	// get returned value
	__rev = new_vm.get_vre();
	new_vm.set_rev_empty();
	new_vm.set_vmstack(nullptr, 0);
}

/// OP_IMPORT
void parse_import(const uint_size_cst cloc, char ** const clst, tcompo_env * const env)
{
	// load current lib
	tlib * current_lib = static_cast<tlib *>(env);
	// create a new lib for it
	tlib * lib = current_lib->recreate();
	// load the file to be imported
	std::string file = std::string(clst[cloc]);
	// load its binary
	std::string filename = file.substr(0, file.find_last_of("."));
	twrapper * w = tanalyser().load_bin_file(filename + ".tapc");
	// initialize the new lib
	lib->set_wrapper(w);

	tvm vm(w->info.tmp_max);
	vm.set_vmstack(lib->get_vmstack(), w->info.reg_max);

	try {
		vm.exec_tins(0, w->ncmds, lib); // excute imported file in new lib
	} catch (...) {
		vm.clean();
		delete lib;
		twarn(ErrRuntime_Other).warn("tvm::parse_import", file);
	}

	tobj returned_v = vm.get_vre();
	vm.set_rev_empty();         // strip vm->rev
	vm.clean();                 // clean vm stack
	vm.set_vmstack(nullptr, 0); // strip lib->vmstack

	if (returned_v.get_type() == tcompo) {
		tcompo_v * v = returned_v.get_v_tcompo();

		if (v->get_compo_type_code() == compo_tdict)
			lib->set_exposed(reinterpret_cast<tdict *>(v));
		else
		{
			delete v;
			lib->set_exposed(new tdict());
		}
	}
	else lib->set_exposed(new tdict());

	topfree_rv().set_v(lib);
	topfree_filled();
}

/// OP_IDXL
void parse_idxl(const uint_size_obj loc, const uint_size_reg nparams,
		bool isenv, tcompo_env * env)
{
	tobj & obj = isenv ? env->get_obj(loc) : get_obj(loc);

	if (obj.get_type() != tcompo)
		twarn(ErrRuntime_RefType).warn("tvm::parse_idxl", "");
	tcompo_v * arr = obj.get_v_tcompo();

	switch (arr->get_compo_type_code()) {
	case compo_tpair:
		parse_idxl_basic(nparams, reinterpret_cast<tpair *>(arr));
		break;
	case compo_tstr:
		parse_idxl_basic(nparams, reinterpret_cast<tstr *>(arr));
		break;
	case compo_tdict:
		parse_idxl_basic(nparams, reinterpret_cast<tdict *>(arr));
		break;
	case compo_tlist:
		parse_idxl_basic(nparams, reinterpret_cast<tlist *>(arr));
		break;
	case compo_tbarr:
		parse_idxl_basic(nparams, reinterpret_cast<tbarr *>(arr));
		break;
	case compo_tdarr:
		parse_idxl_basic(nparams, reinterpret_cast<tdarr *>(arr));
		break;
	default:
		tcompo_idx * arrx = dynamic_cast<tcompo_idx *>(arr);
		if (arrx == nullptr)
			twarn(ErrRuntime_RefType).warn("tvm::parse_idxl", "");
		tobj & rv = vmstk_at(nparams);
		tobj * params = vmstk_get_top_n(nparams);
		arrx->iset(params, nparams, rv);
	}
	vmstk_pop_clean_front_n(1 + nparams);
}

/// Set 'arr[vmstk_get_top_n(nparams)] = vmstk_at(nparams)'
template<typename T>
void parse_idxl_basic(const uint_size_reg nparams, T * arr)
{
	tobj & rv = vmstk_at(nparams);
	tobj * params = vmstk_get_top_n(nparams);
	arr->iset(params, nparams, rv);
}

/// OP_LOOPAS runtime optimization: RO0 (default)
///
/// IMPORTANT:
/// TO DO - check whether the ietrator is changed dynamically in the loop
///
void parse_loopas(tbycode * iter, tobj & vre, tcompo_env * const env)
{
	uint_size_obj idx = iter->get_L();
	bool isenv = iter->get_R();

	// Get the iterator (list, titer, ...)
	const tobj & viter = vmstk_top();
	if (viter.get_type() != tcompo)
		twarn(ErrRuntime_RefType).warn("tvm::parse_loopas", "");
	tcompo_v * it = viter.get_v_tcompo();

	switch (it->get_compo_type_code()) {
	case compo_titer:
		parse_loopas_basic(reinterpret_cast<titer *>(it), idx, vre, isenv, env);
		iter->set_ins(OP_LOOPIAS);  // runtime optimiztion of bycodes
		break;
	case compo_tlist:
		parse_loopas_basic(reinterpret_cast<tlist *>(it), idx, vre, isenv, env);
		iter->set_ins(OP_LOOPLAS);  // runtime optimiztion of bycodes
		break;
	default:
		tcompo_iter * pgiter = nullptr;
		if (nullptr == (pgiter = dynamic_cast<tcompo_iter *>(it)))
		{
			twarn(ErrRuntime_RefType).warn("tvm::parse_loopas", "");
		}
		parse_loopas_basic(pgiter, idx, vre, isenv, env);
		iter->set_ins(OP_LOOPGAS);  // runtime optimiztion of bycodes
	}
}

/// Set 'vre = p->next()' and 'p->get_v_at_loc(env[vloc]/__tmps[vloc])'
template<typename T>
void parse_loopas_basic(T * p, uint_size_obj vloc, tobj & vre,
			bool isenv, tcompo_env * const env)
{
	vre.set_v(bool(p->next()));
	tobj v;
	p->get_v_at_loc(v);
	if (isenv)
		env->set_obj(vloc, v);
	else
		set_obj(vloc, v);
}

/// OP_ADD : OP_OR
/// @details ee tcp::binop_split and tcp::parse_binop for the types
void parse_binop(const binopf & f, tbycode * iter,
			uint_size_reg type, tcompo_env * const env)
{
	uint16_t left = iter->get_L();
	uint16_t right = iter->get_R();

	switch (type) {
	case 0: // value value
		f(vmstk_at(static_cast<uint_size_reg>(left)),
		  vmstk_at(static_cast<uint_size_reg>(right)),
		  vmstk_at(static_cast<uint_size_reg>(right)));
		vmstk_pop_clean_front();
		break;
	case 1: // env value
		f(env->get_obj(left),
		  vmstk_at(static_cast<uint_size_reg>(right)),
		  vmstk_at(static_cast<uint_size_reg>(right)));
		break;
	case 2: // value env
		f(vmstk_at(static_cast<uint_size_reg>(left)),
		  env->get_obj(right),
		  vmstk_at(static_cast<uint_size_reg>(left)));
		break;
	case 3: // env env
		f(env->get_obj(left), env->get_obj(right), topfree_rv());
		topfree_filled();
		break;
	case 4: // tmp value
		f(get_obj(left),
		  vmstk_at(static_cast<uint_size_reg>(right)),
		  vmstk_at(static_cast<uint_size_reg>(right)));
		break;
	case 5: // value tmp
		f(vmstk_at(static_cast<uint_size_reg>(left)),
		  get_obj(right),
		  vmstk_at(static_cast<uint_size_reg>(left)));
		break;
	case 6: // tmp tmp
		f(get_obj(left), get_obj(right), topfree_rv());
		topfree_filled();
		break;
	case 7: // env tmp
		f(env->get_obj(left), get_obj(right), topfree_rv());
		topfree_filled();
		break;
	case 8: // tmp env
		f(get_obj(left), env->get_obj(right), topfree_rv());
		topfree_filled();
		break;
	}
}

/// Execute a single Instruction
void exec_tin(tbycode *& iter, uint_size_cmd & idx, uint_size_cmd end, tcompo_env * env,
		long * cintlsts, double * cdbllsts, char ** cstrlsts)
{
	// printf("%s\n", iter->tostring().c_str());

	switch (iter->ins()) {
	case OP_PASS: {
		break;
	}
	case OP_VCRT: {
		uint_size_cst nameloc = iter->get_C();
		bool isenv = iter->get_P();

		if (isenv)
			env->add_obj(nameloc);
		else
			add_obj(nameloc);
		break;
	}
	case OP_TMPDEL: {
		del_obj(iter->get_U());
		break;
	}
	case OP_THIS: {
		copy_env(env, topfree_rv());
		topfree_filled();
		break;
	}
	case OP_BASE: {
		copy_env(env->get_father_env(), topfree_rv());
		topfree_filled();
		break;
	}
	case OP_BREAK: {
		while (iter->ins() != OP_JPB && idx < end) {
			idx++;
			iter++;
		}
		break;
	}
	case OP_CONTI: {
		while (iter->ins() != OP_JPB && idx < end) {
			idx++;
			iter++;
		}
		if (iter->ins() == OP_JPB) {
			idx--;
			iter--;
		}
		break;
	}
	case OP_RET: {
		if (vmstk_size() > 0) {
			__rev = vmstk_top();
			vmstk_pop_front();

			/* Check reference variables returned to be freed in recursion. */
			if (__rev.get_type() == tcompo && env->tenv_get_compo_type() == compo_tfunc) {
				tfunc * f = static_cast<tfunc *>(env);
				uint_size_obj loc = env->get_ref_obj_loc(__rev.get_v_tcompo());

				if (__rev.get_v_tcompo()->get_refctr() > 0 // __rev seems normal
				&& f->get_refctr() == 0            // but f is to be freed, and
				&& loc < env->get_objlst_len()     // __rev refers a local var
				&& loc >= f->get_nparams())        // which is not a parameter.
					twarn(ErrRuntime_RecurseRefRet).warn("tvm::exec_tins", "");
			}
		} else
			set_rev_empty();

		vmstk_pop_clean_front_n(vmstk_size());
		idx = end;
		return;
	}
	case OP_IN: {
		operator_in(vmstk_top(), vmstk_at(1), __rev);
		vmstk_pop_clean_front_n(2);
		vmstk_push_front_v(__rev);
		set_rev_empty();
		break;
	}
	case OP_PAIR: {
		operator_pair(vmstk_top(), vmstk_at(1), __rev);
		vmstk_pop_clean_front_n(2);
		vmstk_push_front_v(__rev);
		set_rev_empty();
		break;
	}
	case OP_TO: {
		operator_to(vmstk_top(), vmstk_at(1), __rev);
		vmstk_pop_clean_front_n(2);
		vmstk_push_front_v(__rev);
		set_rev_empty();
		break;
	}
	case OP_POPN: {
		bool print = iter->get_R();

		for (uint_size_reg i = 0; i < static_cast<uint_size_reg>(iter->get_L()); i++) {
			if (print && vmstk_top().get_type() != tnil)
				printf("%s\n", vmstk_top().tostring_full().c_str());
			vmstk_pop_clean_front();
		}
		break;
	}
	case OP_POPCOV: {
		if (iter->get_R())
			env->set_obj(iter->get_L(), vmstk_top());
		else
			set_obj(iter->get_L(), vmstk_top());
		vmstk_pop_front();
		break;
	}
	case OP_LOOPAS: {
		parse_loopas(iter, topfree_rv(), env);
		topfree_filled();
		break;
	}
	case OP_LOOPIAS: {
		titer * p = reinterpret_cast<titer *>(vmstk_top().get_v_tcompo());
		topfree_rv().set_v(p->next());
		uint_size_cmd vloc = iter->get_L();
		bool isenv = iter->get_R();
		if (isenv)
			env->get_obj(vloc).set_v(p->get_locidx());
		else
			get_obj(vloc).set_v(p->get_locidx());
		topfree_filled();
		break;
	}
	case OP_LOOPLAS: {
		tlist * p = reinterpret_cast<tlist *>(vmstk_top().get_v_tcompo());
		parse_loopas_basic(p, iter->get_L(), topfree_rv(), iter->get_R(), env);
		topfree_filled();
		break;
	}
	case OP_LOOPGAS: {
		const tobj & v_iter = vmstk_top();
		tcompo_v * it = v_iter.get_v_tcompo();
		tcompo_iter * ptype = dynamic_cast<tcompo_iter *>(it);
		parse_loopas_basic(ptype, iter->get_L(), topfree_rv(), iter->get_R(), env);
		topfree_filled();
		break;
	}
	case OP_JPF: {
		idx += iter->get_U();
		iter += iter->get_U();
		break;
	}
	case OP_JPB: {
		idx -= iter->get_U();
		iter -= iter->get_U();
		break;
	}
	case OP_CJPFPOP: {
		if (vmstk_top().get_type() == tbool && vmstk_top().get_v_tbool() == 0) {
			uint_size_cmd U = iter->get_U();
			idx += U;
			iter += U;
			vmstk_pop_front();
		}
		else
			vmstk_pop_clean_front();
		break;
	}
	case OP_CJPBPOP: {
		if (vmstk_top().get_type() == tbool && vmstk_top().get_v_tbool() == 0) {
			uint_size_cmd U = iter->get_U();
			idx -= U;
			iter -= U;
			vmstk_pop_front();
		}
		else
			vmstk_pop_clean_front();
		break;
	}
	case OP_PUSHX: {
		uint_size_obj loc = iter->get_L();
		bool isenv = iter->get_R();
		if (isenv)
			topfree_rv() = env->get_obj(loc);
		else
			topfree_rv() = get_obj(loc);
		topfree_filled();
		break;
	}
	case OP_PUSHI: {
		topfree_rv().set_v(cintlsts[iter->get_U()]);
		topfree_filled();
		break;
	}
	case OP_PUSHD: {
		topfree_rv().set_v(cdbllsts[iter->get_U()]);
		topfree_filled();
		break;
	}
	case OP_PUSHB: {
		topfree_rv().set_v(bool(iter->get_U()));
		topfree_filled();
		break;
	}
	case OP_PUSHS: {
		topfree_rv().set_v(new tstr(cstrlsts[iter->get_U()]));
		topfree_filled();
		break;
	}
	case OP_PUSHDICT: {
		tdict * dict = new tdict();
		uint_size_reg nparams = static_cast<uint_size_reg>(iter->get_U());
		tobj * params = vmstk_get_top_n(nparams);

		for (uint_size_reg i = 0; i < nparams; i++) {
			dict->set_append(params);
			params++;
		}
		__rev.set_v(dict);
		vmstk_pop_clean_front_n(nparams);
		vmstk_push_front_v(__rev);
		set_rev_empty();
		break;
	}
	case OP_PUSHINFO: {
		topfree_rv().set_v(static_cast<long>(iter->get_U()));
		topfree_filled();
		break;
	}
	case OP_IMPORT: {
		parse_import(static_cast<uint_size_cst>(iter->get_U()), cstrlsts, env);
		break;
	}
	case OP_IDXR: {
		parse_idx(static_cast<uint_size_reg>(iter->get_U()));
		break;
	}
	case OP_EVAL: {
		parse_eval(iter, env);
		break;
	}
	case OP_EVALSF: {
		uint_size_reg nparams = static_cast<uint_size_reg>(iter->get_U());
		tcompo_v * v = vmstk_top().get_v_tcompo();
		tobj * params = this->vmstk_get_top_n(nparams + 1);
		reinterpret_cast<tcppsessf *>(v)->get_f()(params, nparams, __rev, env);
		vmstk_pop_clean_front_n(1 + nparams);
		vmstk_push_front_v(__rev);
		set_rev_empty();
		break;
	}
	case OP_EVALCF: {
		uint_size_reg nparams = static_cast<uint_size_reg>(iter->get_U());
		tcompo_v * v = vmstk_top().get_v_tcompo();
		tobj * params = this->vmstk_get_top_n(nparams + 1);
		reinterpret_cast<tcppgenf *>(v)->get_f()(params, nparams, __rev);
		vmstk_pop_clean_front_n(1 + nparams);
		vmstk_push_front_v(__rev);
		set_rev_empty();
		break;
	}
	case OP_EVALTF: {
		break;
	}
	case OP_IDXL: {
		uint_size_obj loc = iter->get_L();
		uint_size_reg nparams = iter->get_b();
		bool isenv = iter->get_i();
		parse_idxl(loc, nparams, isenv, env);
		break;
	}
	case OP_PUSHF: {
		uint_size_cmd ncmds = iter->get_U();
		uint_size_reg nparams = static_cast<uint_size_reg>(vmstk_top().get_v_tint());
		vmstk_pop_front();
		uint_size_reg fregmax = static_cast<uint_size_reg>(vmstk_top().get_v_tint());
		vmstk_pop_front();
		uint_size_obj ntmps = static_cast<uint_size_obj>(vmstk_top().get_v_tint());
		vmstk_pop_front();
		uint_size_obj nobjs = static_cast<uint_size_obj>(vmstk_top().get_v_tint());
		vmstk_pop_front();
		tfunc * f = new tfunc(nobjs, env, fregmax, ntmps, nparams, idx + 1, ncmds);
		idx += ncmds;
		iter += ncmds;
		topfree_rv().set_v(f);
		topfree_filled();
		break;
	}
	case OP_ADD: {
		uint_size_reg type = static_cast<uint_size_reg>(vmstk_top().get_v_tint());
		vmstk_pop_front();
		parse_binop(operator_add, iter, type, env);
		break;
	}
	case OP_SUB: {
		uint_size_reg type = static_cast<uint_size_reg>(vmstk_top().get_v_tint());
		vmstk_pop_front();
		parse_binop(operator_sub, iter, type, env);
		break;
	}
	case OP_MUL: {
		uint_size_reg type = static_cast<uint_size_reg>(vmstk_top().get_v_tint());
		vmstk_pop_front();
		parse_binop(operator_mul, iter, type, env);
		break;
	}
	case OP_DIV: {
		uint_size_reg type = static_cast<uint_size_reg>(vmstk_top().get_v_tint());
		vmstk_pop_front();
		parse_binop(operator_div, iter, type, env);
		break;
	}
	case OP_MOD: {
		uint_size_reg type = static_cast<uint_size_reg>(vmstk_top().get_v_tint());
		vmstk_pop_front();
		parse_binop(operator_mod, iter, type, env);
		break;
	}
	case OP_POW: {
		uint_size_reg type = static_cast<uint_size_reg>(vmstk_top().get_v_tint());
		vmstk_pop_front();
		parse_binop(operator_pow, iter, type, env);
		break;
	}
	case OP_MMUL: {
		uint_size_reg type = static_cast<uint_size_reg>(vmstk_top().get_v_tint());
		vmstk_pop_front();
		parse_binop(operator_mmul, iter, type, env);
		break;
	}
	case OP_EQ: {
		uint_size_reg type = static_cast<uint_size_reg>(vmstk_top().get_v_tint());
		vmstk_pop_front();
		parse_binop(operator_eq, iter, type, env);
		break;
	}
	case OP_NE: {
		uint_size_reg type = static_cast<uint_size_reg>(vmstk_top().get_v_tint());
		vmstk_pop_front();
		parse_binop(operator_ne, iter, type, env);
		break;
	}
	case OP_GE: {
		uint_size_reg type = static_cast<uint_size_reg>(vmstk_top().get_v_tint());
		vmstk_pop_front();
		parse_binop(operator_ge, iter, type, env);
		break;
	}
	case OP_SG: {
		uint_size_reg type = static_cast<uint_size_reg>(vmstk_top().get_v_tint());
		vmstk_pop_front();
		parse_binop(operator_sg, iter, type, env);
		break;
	}
	case OP_LE: {
		uint_size_reg type = static_cast<uint_size_reg>(vmstk_top().get_v_tint());
		vmstk_pop_front();
		parse_binop(operator_le, iter, type, env);
		break;
	}
	case OP_SL: {
		uint_size_reg type = static_cast<uint_size_reg>(vmstk_top().get_v_tint());
		vmstk_pop_front();
		parse_binop(operator_sl, iter, type, env);
		break;
	}
	case OP_AND: {
		uint_size_reg type = static_cast<uint_size_reg>(vmstk_top().get_v_tint());
		vmstk_pop_front();
		parse_binop(operator_and, iter, type, env);
		break;
	}
	case OP_OR: {
		uint_size_reg type = static_cast<uint_size_reg>(vmstk_top().get_v_tint());
		vmstk_pop_front();
		parse_binop(operator_or, iter, type, env);
		break;
	}
	}
}

/// Get wrapper from the top father environment of env
twrapper * get_wrapper_from_env(tcompo_env_abstract * env)
{
	while (env->get_father_env())
		env = env->get_father_env();

	tlib * lib_env = static_cast<tlib *>(env);
	return lib_env->get_wrapper();
}

public:

/** Constructor of tvm class
 *  @param tmpmax - the maximum length of temporary objects to be decleared
 */
tvm(uint_size_obj tmpmax) : tobj_array(tmpmax)
{
	__stklen = 0;
	__regmax = 0;
	__stk = nullptr;
	__inloop = 0;
}

/// Deconstructor
~tvm()
{
	clean();
}

/// Clean virtual machine
void clean()
{
	__rev.try_clear();
	vmstk_pop_clean_front_n(__stklen);
}

/// Set the maximum room for temporary variables
void set_tmpmax(uint_size_obj tmpmax)
{
	try_expand_objlist(tmpmax);
}

/** Set __stk to be an outside Tapv array
 *  @param vmstack - (tapv *) A pointer to array of taps as vmstack
 *  @param nreg - (uint_regs) The maximum number of registers in vamstack
 */
void set_vmstack(tobj * vmstack, uint_size_reg nreg)
{
	__stk = vmstack;
	__regmax = nreg;
	__stklen = 0;
}

/// The reference of __rev
inline tobj & get_vre()
{
	return __rev;
}

/** Excute bycodes from `from` to the `from _ ncmds`
 *  @param from  - Starting point of bycodes
 *  @param ncmds - Number of bycodes to be executed
 *  @param env   - Current running environment
 */
void exec_tins(uint_size_cmd from, uint_size_cmd ncmds, tcompo_env * env)
{
	const twrapper * wrapper = get_wrapper_from_env(env);
	long * cintlsts = wrapper->consts.cints;
	double * cdbllsts = wrapper->consts.cdbls;
	char ** cstrlsts = wrapper->consts.cstrs;
	uint_size_cmd idx = from;
	uint_size_cmd end = from + ncmds;
	tbycode * iter = wrapper->cmdarr + from;

	while (idx < end) {
		exec_tin(iter, idx, end, env, cintlsts, cdbllsts, cstrlsts);
		idx++;
		iter++;
	}
}

/** Eval bycodes of `lib` since `from` location
 *  @param from - Starting point of bycodes
 *  @param lib  - Current running library
 */
void eval_bycodes(uint_size_cmd from, tlib * lib)
{
	twrapper * wrapper = lib->get_wrapper();
	set_vmstack(lib->get_vmstack(), wrapper->info.reg_max);
	exec_tins(from, wrapper->ncmds - from, lib);
	set_vmstack(nullptr, 0);
}

};


/*===========================================================================*
 * 5. Composit Types Interfaces: Cpp Functions Registration
 *===========================================================================*/

/// print(v1, v2, ...)
inline void gen_print(tobj * const params, uint_size_reg len, tobj & vre)
{
	for (uint_size_reg i = 0; i < len; i++) {
		tobj * atom_i = params + i;
		printf("%s", atom_i->tostring_abbr().c_str());
	}
	printf("\n");
	vre.set_nil();
}

/// sprt(v1, v2, ...)
inline void gen_sprt(tobj* const params, uint_size_reg len, tobj& vre)
{
	for (uint_size_reg i = 0; i < len; i++) {
		tobj* atom_i = params + i;
		printf("%s", atom_i->tostring_full().c_str());
	}
	printf("\n");
	vre.set_nil();
}

/// len(value)
inline void gen_len(tobj * const params, uint_size_reg len, tobj & vre)
{
	if (len != 1)
		twarn(ErrRuntime_ParamsCtr).warn("gen_len", "1 parameter");
	if (params->get_type() == tnil)
		vre.set_v(long(0));
	else if (params->get_type() != tcompo)
		vre.set_v(long(1));
	else {
		tcompo_v * pv = params->get_v_tcompo();
		vre.set_v(pv->len());
	}
}

/// type(obj)
inline void gen_type(tobj * const params, uint_size_reg len, tobj & vre)
{
	if (len != 1)
		twarn(ErrRuntime_ParamsCtr).warn("gen_type", "");

	switch (params->get_type()) {
	case tnil:
		vre.set_v(new tstr("nil"));
		break;
	case tbool:
		vre.set_v(new tstr("bool"));
		break;
	case tint:
		vre.set_v(new tstr("int"));
		break;
	case tdouble:
		vre.set_v(new tstr("float"));
		break;
	case tcompo:
		vre.set_v(new tstr(params->get_v_tcompo()->get_type()));
		break;
	}
}

/// copy(value)
inline void gen_copy(tobj * const params, uint_size_reg len, tobj & vre)
{
	if (len != 1)
		twarn(ErrRuntime_ParamsCtr).warn("gen_copy", "");

	vre = params->copy();
}

/// identical(v1, v2)
inline void gen_identical(tobj * const params, uint_size_reg len, tobj & vre)
{
	if (len != 2)
		twarn(ErrRuntime_ParamsCtr).warn("gen_identical", "");
	tobj * v1 = params;
	tobj * v2 = params + 1;
	bool re = v1->identical(*v2);
	vre.set_v(re);
}

/// tobool(value)
inline void to_bool(tobj * const params, uint_size_reg len, tobj & vre)
{
	if (len != 1)
		twarn(ErrRuntime_ParamsCtr).warn("to_bool", "");
	ttypes type = params->get_type();

	switch (type) {
	case tnil:
		twarn(ErrRuntime_AssignNil).warn("to_bool", "");
		break;
	case tint:
		vre.set_v(static_cast<bool>(params->get_v_tint() != 0));
		break;
	case tdouble:
		twarn(ErrRuntime_ParamsType).warn("to_bool", "");
		break;
	case tbool:
		vre.set_v(static_cast<bool>(params->get_v_tbool()));
		break;
	case tcompo:
		twarn(ErrRuntime_ParamsType).warn("to_bool", "");
		break;
	}
}

/// toint(value)
inline void to_int(tobj * const params, uint_size_reg len, tobj & vre)
{
	if (len != 1)
		twarn(ErrRuntime_ParamsCtr).warn("to_int", "");
	ttypes type = params->get_type();

	switch (type) {
	case tnil:
		twarn(ErrRuntime_AssignNil).warn("to_int", "");
		break;
	case tint:
		vre.set_v(params->get_v_tint());
		break;
	case tdouble:
		vre.set_v(static_cast<long>(params->get_v_tdouble()));
		break;
	case tbool:
		vre.set_v(static_cast<long>(params->get_v_tbool()));
		break;
	case tcompo:
		twarn(ErrRuntime_ParamsType).warn("to_int", "");
		break;
	}
}

/// todouble(value)
inline void to_double(tobj * const params, uint_size_reg len, tobj & vre)
{
	if (len != 1)
		twarn(ErrRuntime_ParamsCtr).warn("to_double", "");
	ttypes type = params->get_type();

	switch (type) {
	case tnil:
		twarn(ErrRuntime_ParamsType).warn("to_double", "");
		break;
	case tint:
		vre.set_v(static_cast<double>(params->get_v_tint()));
		break;
	case tdouble:
		vre.set_v(params->get_v_tdouble());
		break;
	case tbool:
		vre.set_v(static_cast<double>(params->get_v_tbool()));
		break;
	case tcompo:
		twarn(ErrRuntime_ParamsType).warn("to_double", "");
		break;
	}
}

/// topair(v1, v2): cpp function for pair construction
inline void to_pair(tobj * const params, uint_size_reg len, tobj& vre)
{
	if (len != 2)
		twarn(ErrRuntime_ParamsCtr).warn("to_iter", "");
	tobj * v1 = params;
	tobj * v2 = params + 1;
	tpair * pair = new tpair(*v1, *v2);
	vre.set_v(static_cast<tcompo_v *>(pair));
}

/// toiter(from, by, to): cpp function for iter construction
inline void to_iter(tobj * const params, uint_size_reg len, tobj& vre)
{
	if (len != 3)
		twarn(ErrRuntime_ParamsCtr).warn("to_iter", "");
	tobj atom_start = params[0];
	tobj atom_middle = params[1];
	tobj atom_end = params[2];

	if (atom_start.get_type() != tint
	||  atom_middle.get_type() != tint
	||  atom_end.get_type() != tint)
		twarn(ErrRuntime_ParamsType).warn("to_iter", "");

	long start = atom_start.get_v_tint();
	long middle = atom_middle.get_v_tint();
	long end   = atom_end.get_v_tint();
	titer * v = new titer(start, middle, end);
	vre.set_v(v);
}


/// General C++ Functions Registrations.
inline void register_cppfuncs(tlib & lib)
{
	tdict * tstd = lib.add_pkg("std");

	// Functions defined in "tvm.h".
	tstd->add_cppf("print",      gen_print,     UNDEF_NPARAMS);
	tstd->add_cppf("sprt",       gen_sprt,      UNDEF_NPARAMS);
	tstd->add_cppf("len",        gen_len,       1);
	tstd->add_cppf("type",       gen_type,      1);
	tstd->add_cppf("copy",       gen_copy,      1);
	tstd->add_cppf("identical",  gen_identical, 2);
	tstd->add_cppf("tobool",     to_bool,       1);
	tstd->add_cppf("toint",      to_int,        1);
	tstd->add_cppf("todouble",   to_double,     1);

	// Functions defined in "extensions/tstd.h"
	tstd->add_cppf("str2bool",   str_to_bool,   1);
	tstd->add_cppf("str2int",    str_to_int,    1);
	tstd->add_cppf("str2double", str_to_double, 1);
	tstd->add_cppf("toiter",     to_iter,       3);
	tstd->add_cppf("topair",     to_pair,       2);
	tstd->add_cppf("tolist",     to_list,       UNDEF_NPARAMS);
	tstd->add_cppf("tostr",      to_str,        1);
	tstd->add_cppf("append",     set_append,    2);
	tstd->add_cppf("insert",     set_insert,    3);
	tstd->add_cppf("pop",        set_pop,       2);
	tstd->add_cppf("delete",     set_delete,    3);
	tstd->add_cppf("union",      set_union,     2);
	tstd->add_cppf("dkeys",      dict_keys,     1);
	tstd->add_cppf("dvalues",    dict_values,   1);
	tstd->add_cppf("now",        time_now,      0);
}

/// The Eigen Libeary Registration
inline void register_for_Eigen_APIs(tlib & lib)
{
	tdict * eig = lib.add_pkg("eig");
	eig->add_cppf("toarr",       to_arr,          3);
	eig->add_cppf("random",      to_arr_random,   2);
	eig->add_cppf("cols",        arr_cols,        1);
	eig->add_cppf("rows",        arr_rows,        1);
	eig->add_cppf("t",           arr_transpose,   1);

	eig->add_cppf("top",         arr_toprows,     2);
	eig->add_cppf("bottom",      arr_bottomrows,  2);
	eig->add_cppf("left",        arr_leftcols,    2);
	eig->add_cppf("right",       arr_rightcols,   2);
	eig->add_cppf("topright",    arr_topright,    3);
	eig->add_cppf("topleft",     arr_topleft,     3);
	eig->add_cppf("bottomright", arr_bottomright, 3);
	eig->add_cppf("bottomleft",  arr_bottomleft,  3);

	eig->add_cppf("abs",         arr_abs,         1);
	eig->add_cppf("eleinv",      arr_eleinv,      1);
	eig->add_cppf("conjugate",   arr_conjugate,   1);
	eig->add_cppf("exp",         arr_exp,         1);
	eig->add_cppf("log",         arr_log,         1);
	eig->add_cppf("log1p",       arr_log1p,       1);
	eig->add_cppf("log10",       arr_log10,       1);
	eig->add_cppf("pow",         arr_pow,         2);
	eig->add_cppf("sqrt",        arr_sqrt,        1);
	eig->add_cppf("rsqrt",       arr_rsqrt,       1);

	eig->add_cppf("sin",         arr_sin,         1);
	eig->add_cppf("asin",        arr_asin,        1);
	eig->add_cppf("cos",         arr_cos,         1);
	eig->add_cppf("acos",        arr_acos,        1);
	eig->add_cppf("tan",         arr_tan,         1);
	eig->add_cppf("atan",        arr_atan,        1);
	eig->add_cppf("sinh",        arr_sinh,        1);
	eig->add_cppf("cosh",        arr_cosh,        1);
	eig->add_cppf("tanh",        arr_tanh,        1);

	eig->add_cppf("ceil",        arr_ceil,        1);
	eig->add_cppf("floor",       arr_floor,       1);
	eig->add_cppf("round",       arr_round,       1);
	eig->add_cppf("isfinite",    arr_isFinite,    1);
	eig->add_cppf("isinf",       arr_isInf,       1);
	eig->add_cppf("isnan",       arr_isNaN,       1);
}

}

#endif // TVM_H
