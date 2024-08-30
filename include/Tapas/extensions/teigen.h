#ifndef TEIGEN_H
#define TEIGEN_H

#include <Eigen/Dense>
#include "../trts.h"
#include "tstd.h"

namespace tapas
{

typedef Eigen::Array<bool,   Eigen::Dynamic, Eigen::Dynamic> eigbarr;
typedef Eigen::Array<double, Eigen::Dynamic, Eigen::Dynamic> eigdarr;

/** @brief The virtual wrapper of Eigen array
 *  @details Provide basic methods of tcompo_v and indexing.
 */
template <class T>
class tarr : public tcompo_v,
			 public Eigen::Array<T, Eigen::Dynamic, Eigen::Dynamic>
{
public:
tarr(const Eigen::Array<T, Eigen::Dynamic, Eigen::Dynamic> & a)
	: Eigen::Array<T, Eigen::Dynamic, Eigen::Dynamic>(a) {}

tarr(long rows, long cols)
	: Eigen::Array<T, Eigen::Dynamic, Eigen::Dynamic>(rows, cols) {}

virtual tarr * copy()
{
	return new tarr(*this);
}

virtual const char * get_type() const
{
	return "Eigen Array";
}

virtual tcompo_type get_compo_type_code() const
{
	return compo_tarr;
}

bool identical(tcompo_v * v) const
{
	return v == this ? true : false;
}

std::string tostring_abbr() const
{
	return tostring_pointer(get_type(), this);
}

std::string tostring_full() const
{
	std::stringstream ss; ss << *this; return ss.str();
}

long len() const
{
	return this->rows() * this->cols();
}

void idx(const tobj * params, uint_size_reg np, tobj& vre)
{
	if (np != 2)
		twarn(ErrRuntime_ParamsCtr).warn("tarr_general::idx", "");
	const tobj * p1 = params;
	const tobj * p2 = params + 1;

	if (p1->get_type() != tint && p1->get_type() != tcompo)
		twarn(ErrRuntime_ParamsType).warn("tarr_general::idx", "");
	if (p2->get_type() != tint && p2->get_type() != tcompo)
		twarn(ErrRuntime_ParamsType).warn("tarr_general::idx", "");

	if (p1->get_type() == tint) {
		long idx1 = p1->get_v_tint();

		if (p2->get_type() == tint) {
			long idx2 = p2->get_v_tint();
			vre.set_v((*this)(idx1, idx2));
		}
		if (p2->get_type() == tcompo) {
			if (p2->get_v_tcompo()->get_compo_type_code() != compo_tpair)
				twarn(ErrRuntime_ParamsType).warn("tarr_general::idx", "");
			tpair * idx2_pair = reinterpret_cast<tpair *>(p2->get_v_tcompo());
			tobj idx2_bgn = idx2_pair->get_first();
			tobj idx2_end = idx2_pair->get_second();

			if (idx2_bgn.get_type() != tint || idx2_end.get_type() != tint)
				twarn(ErrRuntime_ParamsType).warn("tarr_general::idx", "");
			long idx2_bgn_i = idx2_bgn.get_v_tint();
			long idx2_end_i = idx2_end.get_v_tint();
			long len = idx2_end_i - idx2_bgn_i;

			if (len <= 0)
				twarn(ErrRuntime_IdxOutRange).warn("tarr_general::idx", "");
			vre.set_v(new tarr(this->block(idx1, idx2_bgn_i, 1, len)));
		}
	}
	if (p1->get_type() == tcompo) {
		if (p1->get_v_tcompo()->get_compo_type_code() != compo_tpair)
			twarn(ErrRuntime_ParamsType).warn("tarr_general::idx", "");

		tpair * idx1 = reinterpret_cast<tpair *>(p1->get_v_tcompo());
		tobj idx1_bgn_t = idx1->get_first();
		tobj idx1_end_t = idx1->get_second();
		long idx1_bgn_i = idx1_bgn_t.get_v_tint();
		long idx1_end_i = idx1_end_t.get_v_tint();
		long len_1 = idx1_end_i - idx1_bgn_i;

		if (len_1 <= 0)
			twarn(ErrRuntime_IdxOutRange).warn("tarr_general::idx", "");
		if (p2->get_type() == tint) {
			long idx_2_int = p2->get_v_tint();
			vre.set_v(new tarr(this->block(idx1_bgn_i, idx_2_int, len_1, 1)));
		}
		if (p2->get_type() == tcompo) {
			if (p2->get_v_tcompo()->get_compo_type_code() != compo_tpair)
				twarn(ErrRuntime_ParamsType).warn("tarr_general::idx", "");
			tpair * idx2 = reinterpret_cast<tpair *>(p2->get_v_tcompo());
			tobj idx2_bgn = idx2->get_first();
			tobj idx2_end = idx2->get_second();
			long idx2_bgn_i = idx2_bgn.get_v_tint();
			long idx2_end_i = idx2_end.get_v_tint();
			long len2 = idx2_end_i - idx2_bgn_i;

			if (len2 <= 0)
				twarn(ErrRuntime_IdxOutRange).warn("tarr_general::idx", "");
			vre.set_v(new tarr(this->block(idx1_bgn_i, idx2_bgn_i, len_1, len2)));
		}
	}
}

/// tright can only be `tbool` or `tdouble`
void iset(const tobj * params, uint_size_reg np, const tobj & vright, ttypes tright)
{
	if (np != 2)
		twarn(ErrRuntime_ParamsCtr).warn("tarr_general::iset", "");
	const tobj * p1 = params;
	const tobj * p2 = params + 1;

	if (p1->get_type() != tint && p1->get_type() != tcompo)
		twarn(ErrRuntime_ParamsType).warn("tarr_general::iset", "");
	if (p2->get_type() != tint && p2->get_type() != tcompo)
		twarn(ErrRuntime_ParamsType).warn("tarr_general::iset", "");

	if (p1->get_type() == tint) {
		long idx1 = p1->get_v_tint();

		if (p2->get_type() == tint) {
			long idx2 = p2->get_v_tint();

			if (tright == tbool) {
				if (vright.get_type() != tbool)
					twarn(ErrRuntime_RefType).warn("tarr_general::iset", "");
				(*this)(idx1, idx2) = vright.get_v_tbool();
			}
			if (tright == tdouble) {
				if (vright.get_type() == tint)
					(*this)(idx1, idx2) = vright.get_v_tint();
				else if (vright.get_type() == tdouble)
					(*this)(idx1, idx2) = vright.get_v_tdouble();
				else twarn(ErrRuntime_RefType).warn("tarr_general::iset", "");
			}
		}
		if (p2->get_type() == tcompo) {
			if (p2->get_v_tcompo()->get_compo_type_code() != compo_tpair)
				twarn(ErrRuntime_ParamsType).warn("tarr_general::iset", "");
			tpair * idx2 = reinterpret_cast<tpair *>(p2->get_v_tcompo());
			tobj idx2_bgn = idx2->get_first();
			tobj idx2_end = idx2->get_second();

			if (idx2_bgn.get_type() != tint || idx2_end.get_type() != tint)
				twarn(ErrRuntime_ParamsType).warn("tarr_general::iset", "");
			long idx2_bgn_i = idx2_bgn.get_v_tint();
			long idx2_end_i = idx2_end.get_v_tint();
			long len = idx2_end_i - idx2_bgn_i;

			if (len <= 0)
				twarn(ErrRuntime_IdxOutRange).warn("tarr_general::iset", "");

			if (vright.get_type() != tcompo)
				twarn(ErrRuntime_RefType).warn("tarr_general::iset", "");
			if (vright.get_v_tcompo()->get_compo_type_code() != compo_tbarr
			&& vright.get_v_tcompo()->get_compo_type_code() != compo_tdarr)
				twarn(ErrRuntime_RefType).warn("tarr_general::iset", "");
			tarr * barr = reinterpret_cast<tarr *>(vright.get_v_tcompo());

			if (barr->rows() != 1 || barr->cols() != len)
				twarn(ErrRuntime_LenInconsis).warn("tarr_general::iset", "");
			this->block(idx1, idx2_bgn_i, 1, len) = *barr;
		}
	}
	if (p1->get_type() == tcompo) {
		if (p1->get_v_tcompo()->get_compo_type_code() != compo_tpair)
			twarn(ErrRuntime_ParamsType).warn("tarr_general::iset", "");
		tpair * idx1 = reinterpret_cast<tpair *>(p1->get_v_tcompo());
		tobj idx1_bgn = idx1->get_first();
		tobj idx1_end = idx1->get_second();
		long idx1_bgn_i = idx1_bgn.get_v_tint();
		long idx1_end_i = idx1_end.get_v_tint();
		long nrow = idx1_end_i - idx1_bgn_i;

		if (nrow <= 0)
			twarn(ErrRuntime_IdxOutRange).warn("tarr_general::iset", "");
		if (p2->get_type() == tint) {
			if (vright.get_type() != tcompo)
				twarn(ErrRuntime_RefType).warn("tarr_general::iset", "");
			if (vright.get_v_tcompo()->get_compo_type_code() != compo_tbarr
			&& vright.get_v_tcompo()->get_compo_type_code() != compo_tdarr)
				twarn(ErrRuntime_RefType).warn("tarr_general::iset", "");
			tarr *barr = reinterpret_cast<tarr*>(vright.get_v_tcompo());

			if (barr->rows() != nrow || barr->cols() != 1)
				twarn(ErrRuntime_LenInconsis).warn("tarr_general::iset", "");
			this->block(idx1_bgn_i, p2->get_v_tint(), nrow, 1) = *barr;
		}
		if (p2->get_type() == tcompo) {
			if (p2->get_v_tcompo()->get_compo_type_code() != compo_tpair)
				twarn(ErrRuntime_ParamsType).warn("tarr_general::iset", "");
			tpair * idx2 = reinterpret_cast<tpair *>(p2->get_v_tcompo());
			tobj idx2_bgn = idx2->get_first();
			tobj idx2_end = idx2->get_second();
			long idx2_bgn_i = idx2_bgn.get_v_tint();
			long idx2_end_i = idx2_end.get_v_tint();
			long ncol = idx2_end_i - idx2_bgn_i;

			if (ncol <= 0)
				twarn(ErrRuntime_IdxOutRange).warn("tarr_general::iset", "");
			if (vright.get_type() != tcompo)
				twarn(ErrRuntime_RefType).warn("tarr_general::iset", "");
			if (vright.get_v_tcompo()->get_compo_type_code() != compo_tbarr
			&& vright.get_v_tcompo()->get_compo_type_code() != compo_tdarr)
				twarn(ErrRuntime_RefType).warn("tarr_general::iset", "");
			tarr * barr = reinterpret_cast<tarr *>(vright.get_v_tcompo());

			if (barr->rows() != nrow || barr->cols() != ncol)
				twarn(ErrRuntime_LenInconsis).warn("tarr_general::iset", "");
			this->block(idx1_bgn_i, idx2_bgn_i, nrow, ncol) = *barr;
		}
	}
}

};

/** @brief The wrapper of Eigen boolean array
 *  @details Inherit tarr of boolean values and support logical operators
 *           "and" and "or".
 */
class tbarr : public tarr<bool>, public tcompo_idx,
			  public top_and, public top_or
{
private:
void
array_ops(const class tobj & v, tobj & vre,
		const std::string & fname, bool ordered,
		eigbarr (*wrapper_op)(const tbarr &, const tbarr &))
{
	if (v.get_type() != tbool && v.get_type() != tcompo)
		twarn(ErrRuntime_ParamsType).warn(fname, "");

	if (v.get_type() == tbool) {
		tbarr varr(rows(), cols(), v.get_v_tbool());
		if (ordered)
			vre.set_v(new tbarr(wrapper_op(*this, varr)));
		else
			vre.set_v(new tbarr(wrapper_op(varr, *this)));
	}
	if (v.get_type() == tcompo) {
		if (v.get_v_tcompo()->get_compo_type_code() != compo_tbarr)
			twarn(ErrRuntime_ParamsType).warn(fname, "");
		tbarr * varr = reinterpret_cast<tbarr *>(v.get_v_tcompo());

		if (varr->rows() != rows() || varr->cols() != cols())
			twarn(ErrRuntime_LenInconsis).warn(fname, "");
		if (ordered)
			vre.set_v(new tbarr(wrapper_op(*this, *varr)));
		else
			vre.set_v(new tbarr(wrapper_op(*varr, *this)));
	}
}

public:
tbarr(const eigbarr & a) : tarr<bool>(a) {}

tbarr(const tarr<bool> & a) : tarr<bool>(a) {}

tbarr(const tbarr & a) : tarr<bool>(a) {}

tbarr(long rows, long cols) : tarr<bool>(rows, cols)
{
	setConstant(false);
}

tbarr(long rows, long cols, bool v) : tarr<bool>(rows, cols)
{
	setConstant(v);
}

tbarr(long rows, long cols, tlist * li) : tarr<bool>(rows, cols)
{
	if (li->len() != rows * cols)
		twarn(ErrRuntime_LenInconsis).warn("tbarr::tbarr", "");
	long idx_row = 0;
	long idx_col = 0;

	for (auto iter = li->begin(); iter != li->end(); iter++) {
		if (iter->get_type() != tbool)
			twarn(ErrRuntime_ParamsType).warn("tbarr::tbarr", "");
		(*this)(idx_row, idx_col) = iter->get_v_tbool();
		idx_col ++;

		if (idx_col == cols) {
			idx_col = 0;
			idx_row ++;
		}
	}
}

const char * get_type() const
{
	return "Eigen Boolean Array";
}

tcompo_type get_compo_type_code() const
{
	return compo_tbarr;
}

tbarr * copy()
{
	return new tbarr(*this);
}

void idx(const tobj * params, uint_size_reg np, tobj& vre)
{
	tarr::idx(params, np, vre);

	if (vre.get_type() != tcompo)
		return;
	tcompo_v * arr_v = vre.get_v_tcompo();
	tarr * arr_gen = reinterpret_cast<tarr *>(arr_v);
	tbarr * arr = new tbarr(*arr_gen);
	vre.try_clear();
	vre.set_v(arr);
}

void iset(const tobj * params, uint_size_reg np, const tobj& vright)
{
	tarr::iset(params, np, vright, tbool);
}

void operator_and(const class tobj & v, tobj & vre)
{
	array_ops(v, vre, "tarr::operator_and", true, &wrapper_and<eigbarr, tbarr, tbarr>);
}

void operator_rand(const class tobj & v, tobj & vre)
{
	array_ops(v, vre, "tarr::operator_rand", false, &wrapper_and<eigbarr, tbarr, tbarr>);
}

void operator_or(const class tobj & v, tobj & vre)
{
	array_ops(v, vre, "tarr::operator_or", true, &wrapper_or<eigbarr, tbarr, tbarr>);
}

void operator_ror(const class tobj & v, tobj & vre)
{
	array_ops(v, vre, "tarr::operator_ror", false, &wrapper_or<eigbarr, tbarr, tbarr>);
}

};

/** @brief The wrapper of Eigen double float array
 *  @details Inherit tarr of double float values and support numerical &
 *           logical binary operators.
 */
class tdarr : public tarr<double>, public tcompo_idx,
			 public top_add,  public top_sub,    public top_mul,
			 public top_div,  public top_pow,    public top_mmul,
			 public top_sg,   public top_sl,     public top_ge,
			 public top_le,   public top_eq,     public top_ne
{
private:
template<class TEigRe, class TTapRe>
void array_ops(const class tobj & v, tobj & vre,
		const std::string & fname, bool ordered,
		TEigRe (*wrapper_op_1)(const tdarr &, const double &),
		TEigRe (*wrapper_op_2)(const double &, const tdarr &),
		TEigRe (*wrapper_op_3)(const tdarr &, const tdarr &))
{
	if (v.get_type() != tint
	&& v.get_type() != tdouble
	&& v.get_type() != tcompo) twarn(ErrRuntime_ParamsType).warn(fname, "");

	if (v.get_type() == tint || v.get_type() == tdouble) {
		double vnum = v.get_type()==tint ? v.get_v_tint() : v.get_v_tdouble();
		if (ordered)
			vre.set_v(new TTapRe(wrapper_op_1(*this, vnum)));
		else
			vre.set_v(new TTapRe(wrapper_op_2(vnum, *this)));
	}
	if (v.get_type() == tcompo) {
		if (v.get_v_tcompo()->get_compo_type_code() != compo_tdarr)
			twarn(ErrRuntime_ParamsType).warn(fname, "");
		tdarr * varr = reinterpret_cast<tdarr *>(v.get_v_tcompo());

		if (varr->rows() != rows() || varr->cols() != cols())
			twarn(ErrRuntime_LenInconsis).warn(fname, "");
		if (ordered)
			vre.set_v(new TTapRe(wrapper_op_3(*this, *varr)));
		else
			vre.set_v(new TTapRe(wrapper_op_3(*varr, *this)));
	}
}

public:
tdarr(const eigdarr & a) : tarr<double>(a) {}

tdarr(const tarr<double> & a) : tarr<double>(a) {}

tdarr(long rows, long cols) : tarr<double>(rows, cols)
{
	setRandom(rows, cols);
	*this += 1; *this /= 2;
} // uniform in [0,1]

tdarr(long rows, long cols, double c) : tarr<double>(rows, cols)
{
	setConstant(rows, cols, c);
}

tdarr(long rows, long cols, tlist * li)
	: tarr<double>(rows, cols)
{
	if (li->len() != rows * cols)
		twarn(ErrRuntime_LenInconsis).warn("tarr::tdarr", "");
	long idx_row = 0;
	long idx_col = 0;

	for (auto iter = li->begin(); iter != li->end(); iter++) {
		double vi = 0;

		switch (iter->get_type()) {
		case tbool:
			vi = static_cast<double>(iter->get_v_tbool());
			break;
		case tint:
			vi = static_cast<double>(iter->get_v_tint());
			break;
		case tdouble:
			vi = iter->get_v_tdouble();
			break;
		default:
			twarn(ErrRuntime_RefType).warn("tdarr::tdarr", "");
			break;
		}
		(*this)(idx_row, idx_col) = vi;
		idx_col ++;

		if (idx_col == cols) {
			idx_col = 0;
			idx_row ++;
		}
	}
}

const char * get_type() const
{
	return "Eigen Real Array";
}

tcompo_type get_compo_type_code() const
{
	return compo_tdarr;
}

tdarr * copy()
{
	return new tdarr(*this);
}

void idx(const tobj * params, uint_size_reg np, tobj & vre)
{
	tarr::idx(params, np, vre);

	if (vre.get_type() == tcompo) {
		tcompo_v * arr_v = vre.get_v_tcompo();
		tarr * arr_gen = reinterpret_cast<tarr *>(arr_v);
		tdarr * arr = new tdarr(*arr_gen);
		vre.try_clear();
		vre.set_v(arr);
	}
}

void iset(const tobj * params, uint_size_reg np, const tobj & vright)
{
	tarr::iset(params, np, vright, tdouble);
}

void operator_add(const class tobj & v, tobj & vre)
{
	array_ops<eigdarr, tdarr>(v, vre, "tarr::operator_add", true,
				&wrapper_add<eigdarr, tdarr, double>,
				&wrapper_add<eigdarr, double, tdarr>,
				&wrapper_add<eigdarr, tdarr, tdarr>);
}

void operator_radd(const class tobj &v, tobj &vre)
{
	array_ops<eigdarr, tdarr>(v, vre, "tarr::operator_radd", false,
				&wrapper_add<eigdarr, tdarr, double>,
				&wrapper_add<eigdarr, double, tdarr>,
				&wrapper_add<eigdarr, tdarr, tdarr>);
}

void operator_sub(const class tobj & v, tobj & vre)
{
	array_ops<eigdarr, tdarr>(v, vre, "tarr::operator_sub", true,
				&wrapper_sub<eigdarr, tdarr, double>,
				&wrapper_sub<eigdarr, double, tdarr>,
				&wrapper_sub<eigdarr, tdarr, tdarr>);
}

void operator_rsub(const class tobj & v, tobj & vre)
{
	array_ops<eigdarr, tdarr>(v, vre, "tarr::operator_rsub", false,
				&wrapper_sub<eigdarr, tdarr, double>,
				&wrapper_sub<eigdarr, double, tdarr>,
				&wrapper_sub<eigdarr, tdarr, tdarr>);
}

void operator_mul(const class tobj & v, tobj & vre)
{
	array_ops<eigdarr, tdarr>(v, vre, "tarr::operator_mul", true,
				&wrapper_mul<eigdarr, tdarr, double>,
				&wrapper_mul<eigdarr, double, tdarr>,
				&wrapper_mul<eigdarr, tdarr, tdarr>);
}

void operator_rmul(const class tobj & v, tobj & vre)
{
	array_ops<eigdarr, tdarr>(v, vre, "tarr::operator_rmul", false,
				&wrapper_mul<eigdarr, tdarr, double>,
				&wrapper_mul<eigdarr, double, tdarr>,
				&wrapper_mul<eigdarr, tdarr, tdarr>);
}

void operator_div(const class tobj & v, tobj & vre)
{
	array_ops<eigdarr, tdarr>(v, vre, "tarr::operator_div", true,
				&wrapper_div<eigdarr, tdarr, double>,
				&wrapper_div<eigdarr, double, tdarr>,
				&wrapper_div<eigdarr, tdarr, tdarr>);
}

void operator_rdiv(const class tobj & v, tobj & vre)
{
	array_ops<eigdarr, tdarr>(v, vre, "tarr::operator_rdiv", false,
				&wrapper_div<eigdarr, tdarr, double>,
				&wrapper_div<eigdarr, double, tdarr>,
				&wrapper_div<eigdarr, tdarr, tdarr>);
}

void operator_pow(const class tobj & v, tobj & vre)
{
	if (v.get_type() != tint
	&& v.get_type() != tdouble
	&& v.get_type() != tcompo) {
		twarn(ErrRuntime_ParamsType).warn("tarr::operator_pow", "");
	}
	if (v.get_type() == tint || v.get_type() == tdouble) {
		double right = v.get_type()==tint ? v.get_v_tint() : v.get_v_tdouble();
		vre.set_v(new tdarr(this->pow(right)));
	}
	if (v.get_type() == tcompo) {
		if (v.get_v_tcompo()->get_compo_type_code() != compo_tdarr)
			twarn(ErrRuntime_ParamsType).warn("tarr::operator_pow", "");
		tdarr * right = reinterpret_cast<tdarr *>(v.get_v_tcompo());
		vre.set_v(new tdarr(this->pow(*right)));
	}
}

void operator_rpow(const class tobj & v, tobj & vre)
{
	if (v.get_type() != tint && v.get_type() != tdouble)
		twarn(ErrRuntime_ParamsType).warn("tarr::operator_rpow", "");
	double left = v.get_type()==tint ? v.get_v_tint() : v.get_v_tdouble();
	vre.set_v(new tdarr(Eigen::pow(left, *this)));
}

void operator_mmul(const class tobj & v, tobj & vre)
{
	if (v.get_type() != tcompo)
		twarn(ErrRuntime_ParamsType).warn("tarr::operator_mmul", "");
	if (v.get_v_tcompo()->get_compo_type_code() != compo_tdarr)
		twarn(ErrRuntime_ParamsType).warn("tarr::operator_mmul", "");

	tdarr * right = reinterpret_cast<tdarr *>(v.get_v_tcompo());

	if (right->rows() != this->cols())
		twarn(ErrRuntime_LenInconsis).warn("tarr::operator_mmul", "");
	vre.set_v(new tdarr(this->matrix() * right->matrix()));
}

void operator_rmmul(const class tobj & v, tobj & vre)
{
	if (v.get_type() != tcompo)
		twarn(ErrRuntime_ParamsType).warn("tarr::operator_mmul", "");
	if (v.get_v_tcompo()->get_compo_type_code() != compo_tdarr)
		twarn(ErrRuntime_ParamsType).warn("tarr::operator_mmul", "");

	tdarr * left = reinterpret_cast<tdarr *>(v.get_v_tcompo());

	if (left->cols() != this->rows())
		twarn(ErrRuntime_LenInconsis).warn("tarr::operator_mmul", "");
	vre.set_v(new tdarr(left->matrix() * this->matrix()));
}

void operator_eq(const class tobj & v, tobj & vre)
{
	array_ops<eigbarr, tbarr>(v, vre, "tarr::operator_eq", true,
					&wrapper_eq<eigbarr, tdarr, double>,
					&wrapper_eq<eigbarr, double, tdarr>,
					&wrapper_eq<eigbarr, tdarr, tdarr>);
}

void operator_req(const class tobj & v, tobj & vre)
{
	array_ops<eigbarr, tbarr>(v, vre, "tarr::operator_req", false,
					&wrapper_eq<eigbarr, tdarr, double>,
					&wrapper_eq<eigbarr, double, tdarr>,
					&wrapper_eq<eigbarr, tdarr, tdarr>);
}

void operator_ne(const class tobj & v, tobj & vre)
{
	array_ops<eigbarr, tbarr>(v, vre, "tarr::operator_ne", true,
					&wrapper_ne<eigbarr, tdarr, double>,
					&wrapper_ne<eigbarr, double, tdarr>,
					&wrapper_ne<eigbarr, tdarr, tdarr>);
}

void operator_rne(const class tobj & v, tobj & vre)
{
	array_ops<eigbarr, tbarr>(v, vre, "tarr::operator_rne", false,
					&wrapper_ne<eigbarr, tdarr, double>,
					&wrapper_ne<eigbarr, double, tdarr>,
					&wrapper_ne<eigbarr, tdarr, tdarr>);
}

void operator_sg(const class tobj & v, tobj & vre)
{
	array_ops<eigbarr, tbarr>(v, vre, "tarr::operator_sg", true,
					&wrapper_sg<eigbarr, tdarr, double>,
					&wrapper_sg<eigbarr, double, tdarr>,
					&wrapper_sg<eigbarr, tdarr, tdarr>);
}

void operator_rsg(const class tobj & v, tobj & vre)
{
	array_ops<eigbarr, tbarr>(v, vre, "tarr::operator_rsg", false,
					&wrapper_sg<eigbarr, tdarr, double>,
					&wrapper_sg<eigbarr, double, tdarr>,
					&wrapper_sg<eigbarr, tdarr, tdarr>);
}

void operator_ge(const class tobj & v, tobj & vre)
{
	array_ops<eigbarr, tbarr>(v, vre, "tarr::operator_ge", true,
					&wrapper_ge<eigbarr, tdarr, double>,
					&wrapper_ge<eigbarr, double, tdarr>,
					&wrapper_ge<eigbarr, tdarr, tdarr>);
}

void operator_rge(const class tobj & v, tobj & vre)
{
	array_ops<eigbarr, tbarr>(v, vre, "tarr::operator_rge", false,
					&wrapper_ge<eigbarr, tdarr, double>,
					&wrapper_ge<eigbarr, double, tdarr>,
					&wrapper_ge<eigbarr, tdarr, tdarr>);
}

void operator_sl(const class tobj & v, tobj & vre)
{
	array_ops<eigbarr, tbarr>(v, vre, "tarr::operator_sl", true,
					&wrapper_sl<eigbarr, tdarr, double>,
					&wrapper_sl<eigbarr, double, tdarr>,
					&wrapper_sl<eigbarr, tdarr, tdarr>);
}

void operator_rsl(const class tobj & v, tobj & vre)
{
	array_ops<eigbarr, tbarr>(v, vre, "tarr::operator_rsl", false,
					&wrapper_sl<eigbarr, tdarr, double>,
					&wrapper_sl<eigbarr, double, tdarr>,
					&wrapper_sl<eigbarr, tdarr, tdarr>);
}

void operator_le(const class tobj & v, tobj & vre)
{
	array_ops<eigbarr, tbarr>(v, vre, "tarr::operator_le", true,
					&wrapper_le<eigbarr, tdarr, double>,
					&wrapper_le<eigbarr, double, tdarr>,
					&wrapper_le<eigbarr, tdarr, tdarr>);
}

void operator_rle(const class tobj & v, tobj & vre)
{
	array_ops<eigbarr, tbarr>(v, vre, "tarr::operator_rle", false,
					&wrapper_le<eigbarr, tdarr, double>,
					&wrapper_le<eigbarr, double, tdarr>,
					&wrapper_le<eigbarr, tdarr, tdarr>);
}

};



/*===========================================================================*
 * 1. Array General Methods
 *===========================================================================*/

/// toarr(nrow, ncol, v)
/// v coule be a number or a list of numbers
inline void to_arr(tobj * const params, uint_size_reg len, tobj & vre)
{
	if (len != 3)
		twarn(ErrRuntime_ParamsCtr).warn("to_arr", "3 parameters");
	tobj * anrow = params;
	tobj * ancol = params + 1;
	tobj * value = params + 2;

	if (anrow->get_type() != tint || ancol->get_type() != tint)
		twarn(ErrRuntime_RefType).warn("to_arr", "");
	long nrow = anrow->get_v_tint();
	long ncol = ancol->get_v_tint();

	if (value->get_type() != tbool
	&& value->get_type() != tint
	&& value->get_type() != tdouble
	&& value->get_type() != tcompo)
		twarn(ErrRuntime_RefType).warn("to_arr", "");

	if (value->get_type() == tbool)
		vre.set_v(new tbarr(nrow, ncol, value->get_v_tbool()));
	if (value->get_type() == tint || value->get_type() == tdouble) {
		double c = 0;
		if (value->get_type() == tint)
			c = value->get_v_tint();
		else
			c = value->get_v_tdouble();
		vre.set_v(new tdarr(nrow, ncol, c));
	}
	if (value->get_type() == tcompo) {
		if (value->get_v_tcompo()->get_compo_type_code() != compo_tlist)
			twarn(ErrRuntime_RefType).warn("to_arr", "");
		tlist * li = reinterpret_cast<tlist *>(value->get_v_tcompo());
		ttypes type = li->get_first_ele_type();

		if (type != tbool && type != tint && type != tdouble)
			twarn(ErrRuntime_RefType).warn("to_arr", "");
		if (type == tbool)
			vre.set_v(new tbarr(nrow, ncol, li));
		if (type == tint || type == tdouble)
			vre.set_v(new tdarr(nrow, ncol, li));
	}
}

/// random(nrow, ncol)
inline void to_arr_random(tobj* const params, uint_size_reg len, tobj& vre)
{
	if (len != 2)
		twarn(ErrRuntime_ParamsCtr).warn("to_arr_random", "2 parameters");
	tobj* nrow = params;
	tobj* ncol = params + 1;

	if (nrow->get_type() != tint || ncol->get_type() != tint)
		twarn(ErrRuntime_RefType).warn("to_arr_random", "Should be (int, int)");
	vre.set_v(new tdarr(nrow->get_v_tint(), ncol->get_v_tint()));
}

/// rows(arr)
inline void arr_rows(tobj* const params, uint_size_reg len, tobj& vre)
{
	if (len != 1)
		twarn(ErrRuntime_ParamsCtr).warn("arr_rows", "1 parameter");
	if (params && params->get_type() != tcompo)
		twarn(ErrRuntime_ParamsType).warn("arr_rows", "");

	tcompo_v * v = params->get_v_tcompo();
	tcompo_type type = v->get_compo_type_code();

	if (type != compo_tdarr && type != compo_tbarr)
		twarn(ErrRuntime_ParamsType).warn("arr_rows", "");
	if (type == compo_tdarr)
		vre.set_v(static_cast<long>(reinterpret_cast<tdarr*>(v)->rows()));
	if (type == compo_tbarr)
		vre.set_v(static_cast<long>(reinterpret_cast<tbarr*>(v)->rows()));
}

/// cols(arr)
inline void arr_cols(tobj * const params, uint_size_reg len, tobj & vre)
{
	if (len != 1)
		twarn(ErrRuntime_ParamsCtr).warn("arr_cols", "1 parameter");
	if (params && params->get_type() != tcompo)
		twarn(ErrRuntime_ParamsType).warn("arr_cols", "");

	tcompo_v * v = params->get_v_tcompo();
	tcompo_type type = v->get_compo_type_code();

	if (type != compo_tdarr && type != compo_tbarr)
		twarn(ErrRuntime_ParamsType).warn("arr_cols", "");
	if (type == compo_tdarr)
		vre.set_v(static_cast<long>(reinterpret_cast<tdarr *>(v)->cols()));
	if (type == compo_tbarr)
		vre.set_v(static_cast<long>(reinterpret_cast<tbarr *>(v)->cols()));
}

/// t(arr)
inline void arr_transpose(tobj * const params, uint_size_reg len, tobj & vre)
{
	if (len != 1)
		twarn(ErrRuntime_ParamsCtr).warn("arr_transpose", "1 parameter");
	if (params && params->get_type() != tcompo)
		twarn(ErrRuntime_ParamsType).warn("arr_transpose", "");

	tcompo_v * v = params->get_v_tcompo();
	tcompo_type type = v->get_compo_type_code();

	if (type != compo_tdarr && type != compo_tbarr)
		twarn(ErrRuntime_ParamsType).warn("arr_transpose", "");

	if (type == compo_tdarr) {
		tdarr * arr = reinterpret_cast<tdarr *>(params->get_v_tcompo());
		vre.set_v(new tdarr(arr->transpose()));
	}
	if (type == compo_tbarr) {
		tbarr * arr = reinterpret_cast<tbarr *>(params->get_v_tcompo());
		vre.set_v(new tbarr(arr->transpose()));
	}
}

struct arr_corner {
	long ip;
	long iq;
	bool is_num;
	union {
		tdarr * arr;
		tbarr * arr_bool;
	} arr;
};

struct arr_border {
	long ip;
	bool is_num;
	union {
		tdarr * arr;
		tbarr * arr_bool;
	} arr;
};

inline arr_corner
get_arr_corner_params(tobj * const params, uint_size_reg len)
{
	if (len != 3)
		twarn(ErrRuntime_ParamsCtr).warn("arr_topright", "2 Parameters");

	tobj * arr_v = params;
	tobj * p     = params + 1;
	tobj * q     = params + 2;

	if (arr_v->get_type() != tcompo)
		twarn(ErrRuntime_ParamsType).warn("get_arr_corner_params", "");
	if (p->get_type() != tint || q->get_type() != tint)
		twarn(ErrRuntime_ParamsType).warn("get_arr_corner_params", "");

	tcompo_v * arr_p = arr_v->get_v_tcompo();
	tcompo_type arr_type = arr_p->get_compo_type_code();
	long ip = p->get_v_tint();
	long iq = q->get_v_tint();

	if (arr_type != compo_tdarr && arr_type != compo_tbarr)
		twarn(ErrRuntime_ParamsType).warn("arr_topright", "");
	if (arr_type == compo_tdarr) {
		arr_corner ps;
		ps.ip = ip;
		ps.iq = iq;
		ps.arr.arr = reinterpret_cast<tdarr *>(arr_p);
		ps.is_num = true;
		return ps;
	}
	if (arr_type == compo_tbarr) {
		arr_corner ps;
		ps.ip = ip;
		ps.iq = iq;
		ps.arr.arr_bool = reinterpret_cast<tbarr *>(arr_p);
		ps.is_num = false;
		return ps;
	}
	return arr_corner();
}

inline arr_border get_arr_border_params(tobj * const params, uint_size_reg len)
{
	if (len != 2)
		twarn(ErrRuntime_ParamsCtr).warn("arr_topright", "2 Parameters");

	tobj * arr_v = params;
	tobj * p     = params + 1;

	if (p->get_type() != tint)
		twarn(ErrRuntime_ParamsType).warn("arr_topright", "");
	if (arr_v->get_type() != tcompo)
		twarn(ErrRuntime_ParamsType).warn("arr_topright", "");

	long ip = p->get_v_tint();
	tcompo_v * arr_p = arr_v->get_v_tcompo();

	if (arr_p->get_compo_type_code() != compo_tdarr
	&& arr_p->get_compo_type_code() != compo_tbarr)
		twarn(ErrRuntime_ParamsType).warn("arr_topright", "");
	if (arr_p->get_compo_type_code() == compo_tdarr) {
		tdarr * arr = reinterpret_cast<tdarr *>(arr_p);
		arr_border ps;
		ps.ip = ip;
		ps.arr.arr = arr;
		ps.is_num = true;
		return ps;
	}
	if (arr_p->get_compo_type_code() == compo_tbarr) {
		tbarr * arr = reinterpret_cast<tbarr *>(arr_p);
		arr_border ps;
		ps.ip = ip;
		ps.arr.arr_bool = arr;
		ps.is_num = false;
		return ps;
	}
	return arr_border();
}

/// topright(arr, p, q)
inline void arr_topright(tobj * const params, uint_size_reg len, tobj & vre)
{
	arr_corner ps = get_arr_corner_params(params, len);

	if (ps.is_num)
		vre.set_v(new tdarr(ps.arr.arr->topRightCorner(ps.ip, ps.iq)));
	else
		vre.set_v(new tbarr(ps.arr.arr_bool->topRightCorner(ps.ip, ps.iq)));
}

/// topleft(arr, p, q)
inline void arr_topleft(tobj * const params, uint_size_reg len, tobj & vre)
{
	arr_corner ps = get_arr_corner_params(params, len);

	if (ps.is_num)
		vre.set_v(new tdarr(ps.arr.arr->topLeftCorner(ps.ip, ps.iq)));
	else
		vre.set_v(new tbarr(ps.arr.arr_bool->topLeftCorner(ps.ip, ps.iq)));
}

/// bottomright(arr, p, q)
inline void arr_bottomright(tobj * const params, uint_size_reg len, tobj & vre)
{
	arr_corner ps = get_arr_corner_params(params, len);

	if (ps.is_num)
		vre.set_v(new tdarr(ps.arr.arr->bottomRightCorner(ps.ip, ps.iq)));
	else
		vre.set_v(new tbarr(ps.arr.arr_bool->bottomRightCorner(ps.ip, ps.iq)));
}

/// bottomleft(arr, p, q)
inline void arr_bottomleft(tobj * const params, uint_size_reg len, tobj & vre)
{
	arr_corner ps = get_arr_corner_params(params, len);

	if (ps.is_num)
		vre.set_v(new tdarr(ps.arr.arr->bottomLeftCorner(ps.ip, ps.iq)));
	else
		vre.set_v(new tbarr(ps.arr.arr_bool->bottomLeftCorner(ps.ip, ps.iq)));
}

/// top(arr, p)
inline void arr_toprows(tobj * const params, uint_size_reg len, tobj & vre)
{
	arr_border ps = get_arr_border_params(params, len);

	if (ps.is_num)
		vre.set_v(new tdarr(ps.arr.arr->topRows(ps.ip)));
	else
		vre.set_v(new tbarr(ps.arr.arr_bool->topRows(ps.ip)));
}

/// bottom(arr, p)
inline void arr_bottomrows(tobj * const params, uint_size_reg len, tobj & vre)
{
	arr_border ps = get_arr_border_params(params, len);

	if (ps.is_num)
		vre.set_v(new tdarr(ps.arr.arr->bottomRows(ps.ip)));
	else
		vre.set_v(new tbarr(ps.arr.arr_bool->bottomRows(ps.ip)));
}

/// left(arr, p)
inline void arr_leftcols(tobj * const params, uint_size_reg len, tobj & vre)
{
	arr_border ps = get_arr_border_params(params, len);

	if (ps.is_num)
		vre.set_v(new tdarr(ps.arr.arr->leftCols(ps.ip)));
	else
		vre.set_v(new tbarr(ps.arr.arr_bool->leftCols(ps.ip)));
}

/// right(arr, p)
inline void arr_rightcols(tobj * const params, uint_size_reg len, tobj & vre)
{
	arr_border ps = get_arr_border_params(params, len);

	if (ps.is_num)
		vre.set_v(new tdarr(ps.arr.arr->rightCols(ps.ip)));
	else
		vre.set_v(new tbarr(ps.arr.arr_bool->rightCols(ps.ip)));
}



/*===========================================================================*
 * 2. Numerical and Array Math Methods
 *===========================================================================*/

inline tdarr * eigf_abs(tdarr * arr) { return new tdarr(arr->abs()); }
inline tdarr * eigf_exp(tdarr * arr) { return new tdarr(arr->exp()); }
inline tdarr * eigf_log(tdarr * arr) { return new tdarr(arr->log()); }
inline tdarr * eigf_log1p(tdarr * arr) { return new tdarr(arr->log1p()); }
inline tdarr * eigf_log10(tdarr * arr) { return new tdarr(arr->log10()); }
inline tdarr * eigf_sqrt(tdarr * arr) { return new tdarr(arr->sqrt()); }
inline tdarr * eigf_sin(tdarr * arr) { return new tdarr(arr->sin()); }
inline tdarr * eigf_cos(tdarr * arr) { return new tdarr(arr->cos()); }
inline tdarr * eigf_tan(tdarr * arr) { return new tdarr(arr->tan()); }
inline tdarr * eigf_asin(tdarr * arr) { return new tdarr(arr->asin()); }
inline tdarr * eigf_acos(tdarr * arr) { return new tdarr(arr->acos()); }
inline tdarr * eigf_atan(tdarr * arr) { return new tdarr(arr->atan()); }
inline tdarr * eigf_sinh(tdarr * arr) { return new tdarr(arr->sinh()); }
inline tdarr * eigf_cosh(tdarr * arr) { return new tdarr(arr->cosh()); }
inline tdarr * eigf_tanh(tdarr * arr) { return new tdarr(arr->tanh()); }
inline tdarr * eigf_ceil(tdarr * arr) { return new tdarr(arr->ceil()); }
inline tdarr * eigf_floor(tdarr * arr) { return new tdarr(arr->floor()); }
inline tdarr * eigf_round(tdarr * arr) { return new tdarr(arr->round()); }
inline tbarr * eigf_isfinite(tdarr * arr) { return new tbarr(arr->isFinite()); }
inline tbarr * eigf_isinf(tdarr * arr) { return new tbarr(arr->isInf()); }
inline tbarr * eigf_isnan(tdarr * arr) { return new tbarr(arr->isNaN()); }

template <class Tre1, class Tre2>
void eigf_wrapper(tobj * const params, uint_size_reg len, tobj & vre,
			const char * fname, Tre1 (*f)(double), Tre2 * (*eigf)(tdarr *))
{
	if (len != 1)
		twarn(ErrRuntime_ParamsCtr).warn(fname, "1 Parameter");
	if (params->get_type() != tint
	&& params->get_type() != tdouble
	&& params->get_type() != tcompo)
		twarn(ErrRuntime_ParamsType).warn(fname, "");

	if (params->get_type() == tint)
		vre.set_v(f(params->get_v_tint()));
	if (params->get_type() == tdouble)
		vre.set_v(f(params->get_v_tdouble()));
	if (params->get_type() == tcompo) {
		if (params->get_v_tcompo()->get_compo_type_code() != compo_tdarr)
			twarn(ErrRuntime_ParamsType).warn(fname, "");
		vre.set_v(eigf(reinterpret_cast<tdarr *>(params->get_v_tcompo())));
	}
}

/// abs(v)
inline void arr_abs(tobj * const params, uint_size_reg len, tobj & vre)
{
	eigf_wrapper<double, tdarr>(params, len, vre, "arr_abs", &std::abs, &eigf_abs);
}

/// eleinv(v)
inline void arr_eleinv(tobj * const params, uint_size_reg len, tobj & vre)
{
	if (len != 1)
		twarn(ErrRuntime_ParamsCtr).warn("arr_eleinv", "");
	if (params->get_type() != tint
	&& params->get_type() != tdouble
	&& params->get_type() != tcompo)
		twarn(ErrRuntime_ParamsType).warn("arr_eleinv", "");

	if (params->get_type() == tint)
		vre.set_v(1.0 / params->get_v_tint());
	if (params->get_type() == tdouble)
		vre.set_v(1.0 / params->get_v_tdouble());
	if (params->get_type() == tcompo) {
		if (params->get_v_tcompo()->get_compo_type_code() != compo_tdarr)
			twarn(ErrRuntime_ParamsType).warn("arr_eleinv", "");
		tdarr * arr = reinterpret_cast<tdarr *>(params->get_v_tcompo());
		vre.set_v(new tdarr(arr->inverse()));
	}
}

/// conjugate(v)
inline void arr_conjugate(tobj * const params, uint_size_reg len, tobj & vre)
{
	if (len != 1)
		twarn(ErrRuntime_ParamsCtr).warn("arr_conjugate", "");

	if (params && params->get_type() != tcompo)
		twarn(ErrRuntime_ParamsType).warn("arr_conjugate", "");
	if (params->get_v_tcompo()->get_compo_type_code() != compo_tdarr)
		twarn(ErrRuntime_ParamsType).warn("arr_conjugate", "");

	tdarr * arr = reinterpret_cast<tdarr *>(params->get_v_tcompo());
	vre.set_v(new tdarr(arr->conjugate()));
}

/// exp(v)
inline void arr_exp(tobj * const params, uint_size_reg len, tobj & vre)
{
	eigf_wrapper<double, tdarr>(params, len, vre, "arr_exp", &std::exp, &eigf_exp);
}

/// log(v)
inline void arr_log(tobj * const params, uint_size_reg len, tobj & vre)
{
	eigf_wrapper<double, tdarr>(params, len, vre, "arr_log", &std::log, &eigf_log);
}

/// log1p(v)
inline void arr_log1p(tobj * const params, uint_size_reg len, tobj & vre)
{
	eigf_wrapper<double, tdarr>(params, len, vre, "arr_log1p", &std::log1p, &eigf_log1p);
}

/// log10(v)
inline void arr_log10(tobj * const params, uint_size_reg len, tobj & vre)
{
	eigf_wrapper<double, tdarr>(params, len, vre, "arr_log10", &std::log1p, &eigf_log10);
}

/// pow(v)
inline void arr_pow(tobj * const params, uint_size_reg len, tobj & vre)
{
	if (len != 2)
		twarn(ErrRuntime_ParamsCtr).warn("arr_pow", "");

	tobj * object = params;
	tobj * order = params + 1;
	tdarr * a_order = nullptr;
	double d_order = 0;

	if (order->get_type() != tint
	&& order->get_type() != tdouble
	&& order->get_type() != tcompo)
		twarn(ErrRuntime_ParamsType).warn("arr_pow", "");
	if (order->get_type() == tint)
		d_order = order->get_v_tint();
	if (order->get_type() == tdouble)
		d_order = order->get_v_tdouble();
	if (order->get_type() == tcompo) {
		if (order->get_v_tcompo()->get_compo_type_code() != compo_tdarr)
			twarn(ErrRuntime_ParamsType).warn("arr_pow", "");
		a_order = reinterpret_cast<tdarr *>(order->get_v_tcompo());
	}

	if (object->get_type() != tint
	&& object->get_type() != tdouble
	&& object->get_type() != tcompo)
		twarn(ErrRuntime_ParamsType).warn("arr_pow", "");
	if (object->get_type() == tint) {
		long num = object->get_v_tint();

		if (a_order)
			vre.set_v(new tdarr(Eigen::pow(num, *a_order)));
		else
			vre.set_v(std::pow(num, d_order));
	}
	if (object->get_type() == tdouble) {
		double num = params->get_v_tdouble();

		if (a_order)
			vre.set_v(new tdarr(Eigen::pow(num, *a_order)));
		else
			vre.set_v(std::pow(num, d_order));
	}
	if (object->get_type() == tcompo) {
		if (object->get_v_tcompo()->get_compo_type_code() != compo_tdarr)
			twarn(ErrRuntime_ParamsType).warn("arr_pow", "");
		tdarr * arr = reinterpret_cast<tdarr *>(object->get_v_tcompo());

		if (a_order)
			vre.set_v(new tdarr(arr->pow(*a_order)));
		else
			vre.set_v(new tdarr(arr->pow(d_order)));
	}
}

/// sqrt(v)
inline void arr_sqrt(tobj * const params, uint_size_reg len, tobj & vre)
{
	eigf_wrapper<double, tdarr>(params, len, vre, "arr_sqrt", &std::sqrt, &eigf_sqrt);
}

/// rsqrt(v)
inline void arr_rsqrt(tobj * const params, uint_size_reg len, tobj & vre)
{
	if (len != 1) twarn(ErrRuntime_ParamsCtr).warn("arr_rsqrt", "");

	if (params->get_type() != tint
	&& params->get_type() != tdouble
	&& params->get_type() != tcompo)
		twarn(ErrRuntime_ParamsType).warn("arr_rsqrt", "");
	if (params->get_type() == tint)
		vre.set_v(1.0 / std::sqrt(params->get_v_tint()));
	if (params->get_type() == tdouble)
		vre.set_v(1.0 / std::sqrt(params->get_v_tdouble()));
	if (params->get_type() == tcompo) {
		if (params->get_v_tcompo()->get_compo_type_code() != compo_tdarr)
			twarn(ErrRuntime_ParamsType).warn("arr_rsqrt", "");
		tdarr * arr = reinterpret_cast<tdarr *>(params->get_v_tcompo());
		vre.set_v(new tdarr(arr->rsqrt()));
	}
}

/// sin(v)
inline void arr_sin(tobj * const params, uint_size_reg len, tobj & vre)
{
	eigf_wrapper<double, tdarr>(params, len, vre, "arr_sin", &std::sin, &eigf_sin);
}

/// asin(v)
inline void arr_asin(tobj * const params, uint_size_reg len, tobj & vre)
{
	eigf_wrapper<double, tdarr>(params, len, vre, "arr_asin", &std::asin, &eigf_asin);
}

///cos(v)
inline void arr_cos(tobj * const params, uint_size_reg len, tobj & vre)
{
	eigf_wrapper<double, tdarr>(params, len, vre, "arr_cos", &std::cos, &eigf_cos);
}

/// acos(v)
inline void arr_acos(tobj * const params, uint_size_reg len, tobj & vre)
{
	eigf_wrapper<double, tdarr>(params, len, vre, "arr_acos", &std::acos, &eigf_acos);
}

/// tan(v)
inline void arr_tan(tobj * const params, uint_size_reg len, tobj & vre)
{
	eigf_wrapper<double, tdarr>(params, len, vre, "arr_atn", &std::tan, &eigf_tan);
}

/// atan(v)
inline void arr_atan(tobj * const params, uint_size_reg len, tobj & vre)
{
	eigf_wrapper<double, tdarr>(params, len, vre, "arr_atan", &std::atan, &eigf_atan);
}

/// sinh(v)
inline void arr_sinh(tobj * const params, uint_size_reg len, tobj & vre)
{
	eigf_wrapper<double, tdarr>(params, len, vre, "arr_sinh", &std::sinh, &eigf_sinh);
}

/// cosh(v)
inline void arr_cosh(tobj * const params, uint_size_reg len, tobj & vre)
{
	eigf_wrapper<double, tdarr>(params, len, vre, "arr_cosh", &std::cosh, &eigf_cosh);
}

/// tanh(v)
inline void arr_tanh(tobj * const params, uint_size_reg len, tobj & vre)
{
	eigf_wrapper<double, tdarr>(params, len, vre, "arr_tanh", &std::tanh, &eigf_tanh);
}

/// ceil(v)
inline void arr_ceil(tobj * const params, uint_size_reg len, tobj & vre)
{
	eigf_wrapper<double, tdarr>(params, len, vre, "arr_ceil", &std::ceil, &eigf_ceil);
}

/// floor(v)
inline void arr_floor(tobj * const params, uint_size_reg len, tobj & vre)
{
	eigf_wrapper<double, tdarr>(params, len, vre, "arr_floor", &std::floor, &eigf_floor);
}

/// round(v)
inline void arr_round(tobj * const params, uint_size_reg len, tobj & vre)
{
	eigf_wrapper<double, tdarr>(params, len, vre, "arr_round", &std::round, &eigf_round);
}

/// isfinite(v)
inline void arr_isFinite(tobj * const params, uint_size_reg len, tobj & vre)
{
	eigf_wrapper<bool, tbarr>(params, len, vre, "arr_isFinite", &std::isfinite, &eigf_isfinite);
}

/// isinf(v)
inline void arr_isInf(tobj * const params, uint_size_reg len, tobj & vre)
{
	eigf_wrapper<bool, tbarr>(params, len, vre, "arr_isInf", &std::isinf, &eigf_isinf);
}

/// isnan(v)
inline void arr_isNaN(tobj * const params, uint_size_reg len, tobj & vre)
{
	eigf_wrapper<bool, tbarr>(params, len, vre, "arr_isNaN", &std::isnan, &eigf_isnan);
}

}

#endif // TEIGEN_H
