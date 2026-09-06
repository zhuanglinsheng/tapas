#include "tapas/textension.h"

#include "../arguments.h"

#include "tapas/objects/tpair.h"

#include <math.h>
#include <stdlib.h>


static double math_arg_double(const tobj *v, const char *fname)
{
	if (v->type == tint)
		return (double)v->val.v_tint;
	if (v->type == tfloat)
		return v->val.v_tfloat;
	twarn(ErrRuntime_ParamsType, fname, "");
	return 0.0;
}

static long math_arg_long(const tobj *v, const char *fname)
{
	if (v->type == tint)
		return v->val.v_tint;
	if (v->type == tfloat)
		return (long)v->val.v_tfloat;
	twarn(ErrRuntime_ParamsType, fname, "");
	return 0;
}

#define DEF_MATH_UNARY_FLOAT(name, fn)                                            \
	static void math_##name(tobj *params, uint_regs len, tobj *vre)       \
	{                                                                         \
		tstdlib_require_arguments("math::" #name, len, 1);                        \
		tobj_set_float(vre, fn(math_arg_double(&params[0], "math::" #name))); \
	}

#define DEF_MATH_UNARY_INT(name, fn)                                             \
	static void math_##name(tobj *params, uint_regs len, tobj *vre)      \
	{                                                                        \
		tstdlib_require_arguments("math::" #name, len, 1);                       \
		tobj_set_int(vre, (long)fn(math_arg_double(&params[0], "math::" #name))); \
	}

#define DEF_MATH_BINARY_FLOAT(name, fn)                                           \
	static void math_##name(tobj *params, uint_regs len, tobj *vre)       \
	{                                                                         \
		tstdlib_require_arguments("math::" #name, len, 2);                        \
		tobj_set_float(vre, fn(math_arg_double(&params[0], "math::" #name), \
				       math_arg_double(&params[1], "math::" #name))); \
	}

#define DEF_MATH_BINARY_INT(name, fn)                                             \
	static void math_##name(tobj *params, uint_regs len, tobj *vre)       \
	{                                                                         \
		tstdlib_require_arguments("math::" #name, len, 2);                        \
		tobj_set_bool(vre, fn(math_arg_double(&params[0], "math::" #name), \
				      math_arg_double(&params[1], "math::" #name)));  \
	}

DEF_MATH_UNARY_FLOAT(fabs, fabs)
DEF_MATH_UNARY_FLOAT(sqrt, sqrt)
DEF_MATH_UNARY_FLOAT(cbrt, cbrt)
DEF_MATH_BINARY_FLOAT(pow, pow)
DEF_MATH_BINARY_FLOAT(hypot, hypot)
DEF_MATH_UNARY_FLOAT(sin, sin)
DEF_MATH_UNARY_FLOAT(cos, cos)
DEF_MATH_UNARY_FLOAT(tan, tan)
DEF_MATH_UNARY_FLOAT(asin, asin)
DEF_MATH_UNARY_FLOAT(acos, acos)
DEF_MATH_UNARY_FLOAT(atan, atan)
DEF_MATH_BINARY_FLOAT(atan2, atan2)
DEF_MATH_UNARY_FLOAT(sinh, sinh)
DEF_MATH_UNARY_FLOAT(cosh, cosh)
DEF_MATH_UNARY_FLOAT(tanh, tanh)
DEF_MATH_UNARY_FLOAT(asinh, asinh)
DEF_MATH_UNARY_FLOAT(acosh, acosh)
DEF_MATH_UNARY_FLOAT(atanh, atanh)
DEF_MATH_UNARY_FLOAT(exp, exp)
DEF_MATH_UNARY_FLOAT(exp2, exp2)
DEF_MATH_UNARY_FLOAT(expm1, expm1)
DEF_MATH_UNARY_FLOAT(log, log)
DEF_MATH_UNARY_FLOAT(log2, log2)
DEF_MATH_UNARY_FLOAT(log10, log10)
DEF_MATH_UNARY_FLOAT(log1p, log1p)
DEF_MATH_UNARY_FLOAT(logb, logb)
DEF_MATH_UNARY_INT(ilogb, ilogb)
DEF_MATH_UNARY_FLOAT(erf, erf)
DEF_MATH_UNARY_FLOAT(erfc, erfc)
DEF_MATH_UNARY_FLOAT(lgamma, lgamma)
DEF_MATH_UNARY_FLOAT(tgamma, tgamma)
DEF_MATH_UNARY_FLOAT(ceil, ceil)
DEF_MATH_UNARY_FLOAT(floor, floor)
DEF_MATH_UNARY_FLOAT(nearbyint, nearbyint)
DEF_MATH_UNARY_FLOAT(rint, rint)
DEF_MATH_UNARY_INT(lrint, lrint)
DEF_MATH_UNARY_INT(llrint, llrint)
DEF_MATH_UNARY_FLOAT(round, round)
DEF_MATH_UNARY_INT(lround, lround)
DEF_MATH_UNARY_INT(llround, llround)
DEF_MATH_UNARY_FLOAT(trunc, trunc)
DEF_MATH_BINARY_FLOAT(fmod, fmod)
DEF_MATH_BINARY_FLOAT(remainder, remainder)
DEF_MATH_BINARY_FLOAT(copysign, copysign)
DEF_MATH_BINARY_FLOAT(nextafter, nextafter)
DEF_MATH_BINARY_FLOAT(fdim, fdim)
DEF_MATH_BINARY_FLOAT(fmax, fmax)
DEF_MATH_BINARY_FLOAT(fmin, fmin)
DEF_MATH_BINARY_INT(isgreater, isgreater)
DEF_MATH_BINARY_INT(isgreaterequal, isgreaterequal)
DEF_MATH_BINARY_INT(isless, isless)
DEF_MATH_BINARY_INT(islessequal, islessequal)
DEF_MATH_BINARY_INT(islessgreater, islessgreater)
DEF_MATH_BINARY_INT(isunordered, isunordered)

static void math_abs(tobj *params, uint_regs len, tobj *vre)
{
	tstdlib_require_arguments("math::abs", len, 1);
	if (params[0].type == tint)
		tobj_set_int(vre, labs(params[0].val.v_tint));
	else
		tobj_set_float(vre, fabs(math_arg_double(&params[0], "math::abs")));
}

static void math_rsqrt(tobj *params, uint_regs len, tobj *vre)
{
	tstdlib_require_arguments("math::rsqrt", len, 1);
	tobj_set_float(vre, 1.0 / sqrt(math_arg_double(&params[0], "math::rsqrt")));
}

static void math_fma(tobj *params, uint_regs len, tobj *vre)
{
	tstdlib_require_arguments("math::fma", len, 3);
	tobj_set_float(vre, fma(math_arg_double(&params[0], "math::fma"),
				math_arg_double(&params[1], "math::fma"),
				math_arg_double(&params[2], "math::fma")));
}

static void math_ldexp(tobj *params, uint_regs len, tobj *vre)
{
	tstdlib_require_arguments("math::ldexp", len, 2);
	tobj_set_float(vre, ldexp(math_arg_double(&params[0], "math::ldexp"),
				  (int)math_arg_long(&params[1], "math::ldexp")));
}

static void math_scalbn(tobj *params, uint_regs len, tobj *vre)
{
	tstdlib_require_arguments("math::scalbn", len, 2);
	tobj_set_float(vre, scalbn(math_arg_double(&params[0], "math::scalbn"),
				   (int)math_arg_long(&params[1], "math::scalbn")));
}

static void math_scalbln(tobj *params, uint_regs len, tobj *vre)
{
	tstdlib_require_arguments("math::scalbln", len, 2);
	tobj_set_float(vre, scalbln(math_arg_double(&params[0], "math::scalbln"),
				    math_arg_long(&params[1], "math::scalbln")));
}

static void math_eleinv(tobj *params, uint_regs len, tobj *vre)
{
	tstdlib_require_arguments("math::eleinv", len, 1);
	tobj_set_float(vre, 1.0 / math_arg_double(&params[0], "math::eleinv"));
}

static void math_make_nan(tobj *params, uint_regs len, tobj *vre)
{
	(void)params;
	tstdlib_require_arguments("math::make_nan", len, 0);
	tobj_set_float(vre, NAN);
}

static void math_frexp(tobj *params, uint_regs len, tobj *vre)
{
	int expv = 0;
	double mant;
	tobj a, b;
	tstdlib_require_arguments("math::frexp", len, 1);
	mant = frexp(math_arg_double(&params[0], "math::frexp"), &expv);
	tobj_set_nil(&a);
	tobj_set_nil(&b);
	tobj_set_float(&a, mant);
	tobj_set_int(&b, expv);
	tobj_set_compo(vre, (tcompo_v *)tpair_new(&a, &b));
}

static void math_modf(tobj *params, uint_regs len, tobj *vre)
{
	double intpart = 0.0;
	double frac;
	tobj a, b;
	tstdlib_require_arguments("math::modf", len, 1);
	frac = modf(math_arg_double(&params[0], "math::modf"), &intpart);
	tobj_set_nil(&a);
	tobj_set_nil(&b);
	tobj_set_float(&a, frac);
	tobj_set_float(&b, intpart);
	tobj_set_compo(vre, (tcompo_v *)tpair_new(&a, &b));
}

static void math_remquo(tobj *params, uint_regs len, tobj *vre)
{
	int quo = 0;
	double rem;
	tobj a, b;
	tstdlib_require_arguments("math::remquo", len, 2);
	rem = remquo(math_arg_double(&params[0], "math::remquo"),
		     math_arg_double(&params[1], "math::remquo"),
		     &quo);
	tobj_set_nil(&a);
	tobj_set_nil(&b);
	tobj_set_float(&a, rem);
	tobj_set_int(&b, quo);
	tobj_set_compo(vre, (tcompo_v *)tpair_new(&a, &b));
}

static void math_isfinite(tobj *params, uint_regs len, tobj *vre)
{
	tstdlib_require_arguments("math::isfinite", len, 1);
	tobj_set_bool(vre, isfinite(math_arg_double(&params[0], "math::isfinite")));
}

static void math_isinf(tobj *params, uint_regs len, tobj *vre)
{
	tstdlib_require_arguments("math::isinf", len, 1);
	tobj_set_bool(vre, isinf(math_arg_double(&params[0], "math::isinf")));
}

static void math_isnan(tobj *params, uint_regs len, tobj *vre)
{
	tstdlib_require_arguments("math::isnan", len, 1);
	tobj_set_bool(vre, isnan(math_arg_double(&params[0], "math::isnan")));
}

static void math_isnormal(tobj *params, uint_regs len, tobj *vre)
{
	tstdlib_require_arguments("math::isnormal", len, 1);
	tobj_set_bool(vre, isnormal(math_arg_double(&params[0], "math::isnormal")));
}

static void math_fpclassify(tobj *params, uint_regs len, tobj *vre)
{
	tstdlib_require_arguments("math::fpclassify", len, 1);
	tobj_set_int(vre, fpclassify(math_arg_double(&params[0], "math::fpclassify")));
}

static void math_signbit(tobj *params, uint_regs len, tobj *vre)
{
	tstdlib_require_arguments("math::signbit", len, 1);
	tobj_set_bool(vre, signbit(math_arg_double(&params[0], "math::signbit")));
}


static const textension_symbol symbols[] = {
	{
		.name = "abs",
		.type = "Function[Union[Int, Float]] -> Unknown",
		.detail = "math::abs(value: Union[Int, Float]) -> Unknown",
		.kind = textension_function,
		.function = math_abs,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	},
	{
		.name = "fabs",
		.type = "Function[Union[Int, Float]] -> Float",
		.detail = "math::fabs(value: Union[Int, Float]) -> Float",
		.kind = textension_function,
		.function = math_fabs,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	},
	{
		.name = "sqrt",
		.type = "Function[Union[Int, Float]] -> Float",
		.detail = "math::sqrt(value: Union[Int, Float]) -> Float",
		.kind = textension_function,
		.function = math_sqrt,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	},
	{
		.name = "rsqrt",
		.type = "Function[Union[Int, Float]] -> Float",
		.detail = "math::rsqrt(value: Union[Int, Float]) -> Float",
		.kind = textension_function,
		.function = math_rsqrt,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	},
	{
		.name = "cbrt",
		.type = "Function[Union[Int, Float]] -> Float",
		.detail = "math::cbrt(value: Union[Int, Float]) -> Float",
		.kind = textension_function,
		.function = math_cbrt,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	},
	{
		.name = "pow",
		.type = "Function[Union[Int, Float], Union[Int, Float]] -> Float",
		.detail = "math::pow(left: Union[Int, Float], right: Union[Int, Float]) -> Float",
		.kind = textension_function,
		.function = math_pow,
		.minimum_arguments = 2,
		.maximum_arguments = 2
	},
	{
		.name = "hypot",
		.type = "Function[Union[Int, Float], Union[Int, Float]] -> Float",
		.detail = "math::hypot(left: Union[Int, Float], right: Union[Int, Float]) -> Float",
		.kind = textension_function,
		.function = math_hypot,
		.minimum_arguments = 2,
		.maximum_arguments = 2
	},
	{
		.name = "sin",
		.type = "Function[Union[Int, Float]] -> Float",
		.detail = "math::sin(value: Union[Int, Float]) -> Float",
		.kind = textension_function,
		.function = math_sin,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	},
	{
		.name = "cos",
		.type = "Function[Union[Int, Float]] -> Float",
		.detail = "math::cos(value: Union[Int, Float]) -> Float",
		.kind = textension_function,
		.function = math_cos,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	},
	{
		.name = "tan",
		.type = "Function[Union[Int, Float]] -> Float",
		.detail = "math::tan(value: Union[Int, Float]) -> Float",
		.kind = textension_function,
		.function = math_tan,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	},
	{
		.name = "asin",
		.type = "Function[Union[Int, Float]] -> Float",
		.detail = "math::asin(value: Union[Int, Float]) -> Float",
		.kind = textension_function,
		.function = math_asin,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	},
	{
		.name = "acos",
		.type = "Function[Union[Int, Float]] -> Float",
		.detail = "math::acos(value: Union[Int, Float]) -> Float",
		.kind = textension_function,
		.function = math_acos,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	},
	{
		.name = "atan",
		.type = "Function[Union[Int, Float]] -> Float",
		.detail = "math::atan(value: Union[Int, Float]) -> Float",
		.kind = textension_function,
		.function = math_atan,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	},
	{
		.name = "atan2",
		.type = "Function[Union[Int, Float], Union[Int, Float]] -> Float",
		.detail = "math::atan2(left: Union[Int, Float], right: Union[Int, Float]) -> Float",
		.kind = textension_function,
		.function = math_atan2,
		.minimum_arguments = 2,
		.maximum_arguments = 2
	},
	{
		.name = "sinh",
		.type = "Function[Union[Int, Float]] -> Float",
		.detail = "math::sinh(value: Union[Int, Float]) -> Float",
		.kind = textension_function,
		.function = math_sinh,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	},
	{
		.name = "cosh",
		.type = "Function[Union[Int, Float]] -> Float",
		.detail = "math::cosh(value: Union[Int, Float]) -> Float",
		.kind = textension_function,
		.function = math_cosh,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	},
	{
		.name = "tanh",
		.type = "Function[Union[Int, Float]] -> Float",
		.detail = "math::tanh(value: Union[Int, Float]) -> Float",
		.kind = textension_function,
		.function = math_tanh,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	},
	{
		.name = "asinh",
		.type = "Function[Union[Int, Float]] -> Float",
		.detail = "math::asinh(value: Union[Int, Float]) -> Float",
		.kind = textension_function,
		.function = math_asinh,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	},
	{
		.name = "acosh",
		.type = "Function[Union[Int, Float]] -> Float",
		.detail = "math::acosh(value: Union[Int, Float]) -> Float",
		.kind = textension_function,
		.function = math_acosh,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	},
	{
		.name = "atanh",
		.type = "Function[Union[Int, Float]] -> Float",
		.detail = "math::atanh(value: Union[Int, Float]) -> Float",
		.kind = textension_function,
		.function = math_atanh,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	},
	{
		.name = "exp",
		.type = "Function[Union[Int, Float]] -> Float",
		.detail = "math::exp(value: Union[Int, Float]) -> Float",
		.kind = textension_function,
		.function = math_exp,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	},
	{
		.name = "exp2",
		.type = "Function[Union[Int, Float]] -> Float",
		.detail = "math::exp2(value: Union[Int, Float]) -> Float",
		.kind = textension_function,
		.function = math_exp2,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	},
	{
		.name = "expm1",
		.type = "Function[Union[Int, Float]] -> Float",
		.detail = "math::expm1(value: Union[Int, Float]) -> Float",
		.kind = textension_function,
		.function = math_expm1,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	},
	{
		.name = "log",
		.type = "Function[Union[Int, Float]] -> Float",
		.detail = "math::log(value: Union[Int, Float]) -> Float",
		.kind = textension_function,
		.function = math_log,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	},
	{
		.name = "log2",
		.type = "Function[Union[Int, Float]] -> Float",
		.detail = "math::log2(value: Union[Int, Float]) -> Float",
		.kind = textension_function,
		.function = math_log2,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	},
	{
		.name = "log10",
		.type = "Function[Union[Int, Float]] -> Float",
		.detail = "math::log10(value: Union[Int, Float]) -> Float",
		.kind = textension_function,
		.function = math_log10,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	},
	{
		.name = "log1p",
		.type = "Function[Union[Int, Float]] -> Float",
		.detail = "math::log1p(value: Union[Int, Float]) -> Float",
		.kind = textension_function,
		.function = math_log1p,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	},
	{
		.name = "logb",
		.type = "Function[Union[Int, Float]] -> Float",
		.detail = "math::logb(value: Union[Int, Float]) -> Float",
		.kind = textension_function,
		.function = math_logb,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	},
	{
		.name = "ilogb",
		.type = "Function[Union[Int, Float]] -> Int",
		.detail = "math::ilogb(value: Union[Int, Float]) -> Int",
		.kind = textension_function,
		.function = math_ilogb,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	},
	{
		.name = "frexp",
		.type = "Function[Union[Int, Float]] -> Pair",
		.detail = "math::frexp(value: Union[Int, Float]) -> Pair",
		.kind = textension_function,
		.function = math_frexp,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	},
	{
		.name = "modf",
		.type = "Function[Union[Int, Float]] -> Pair",
		.detail = "math::modf(value: Union[Int, Float]) -> Pair",
		.kind = textension_function,
		.function = math_modf,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	},
	{
		.name = "ldexp",
		.type = "Function[Union[Int, Float], Int] -> Float",
		.detail = "math::ldexp(value: Union[Int, Float], exponent: Int) -> Float",
		.kind = textension_function,
		.function = math_ldexp,
		.minimum_arguments = 2,
		.maximum_arguments = 2
	},
	{
		.name = "scalbn",
		.type = "Function[Union[Int, Float], Int] -> Float",
		.detail = "math::scalbn(value: Union[Int, Float], exponent: Int) -> Float",
		.kind = textension_function,
		.function = math_scalbn,
		.minimum_arguments = 2,
		.maximum_arguments = 2
	},
	{
		.name = "scalbln",
		.type = "Function[Union[Int, Float], Int] -> Float",
		.detail = "math::scalbln(value: Union[Int, Float], exponent: Int) -> Float",
		.kind = textension_function,
		.function = math_scalbln,
		.minimum_arguments = 2,
		.maximum_arguments = 2
	},
	{
		.name = "erf",
		.type = "Function[Union[Int, Float]] -> Float",
		.detail = "math::erf(value: Union[Int, Float]) -> Float",
		.kind = textension_function,
		.function = math_erf,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	},
	{
		.name = "erfc",
		.type = "Function[Union[Int, Float]] -> Float",
		.detail = "math::erfc(value: Union[Int, Float]) -> Float",
		.kind = textension_function,
		.function = math_erfc,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	},
	{
		.name = "lgamma",
		.type = "Function[Union[Int, Float]] -> Float",
		.detail = "math::lgamma(value: Union[Int, Float]) -> Float",
		.kind = textension_function,
		.function = math_lgamma,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	},
	{
		.name = "tgamma",
		.type = "Function[Union[Int, Float]] -> Float",
		.detail = "math::tgamma(value: Union[Int, Float]) -> Float",
		.kind = textension_function,
		.function = math_tgamma,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	},
	{
		.name = "ceil",
		.type = "Function[Union[Int, Float]] -> Float",
		.detail = "math::ceil(value: Union[Int, Float]) -> Float",
		.kind = textension_function,
		.function = math_ceil,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	},
	{
		.name = "floor",
		.type = "Function[Union[Int, Float]] -> Float",
		.detail = "math::floor(value: Union[Int, Float]) -> Float",
		.kind = textension_function,
		.function = math_floor,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	},
	{
		.name = "nearbyint",
		.type = "Function[Union[Int, Float]] -> Float",
		.detail = "math::nearbyint(value: Union[Int, Float]) -> Float",
		.kind = textension_function,
		.function = math_nearbyint,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	},
	{
		.name = "rint",
		.type = "Function[Union[Int, Float]] -> Float",
		.detail = "math::rint(value: Union[Int, Float]) -> Float",
		.kind = textension_function,
		.function = math_rint,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	},
	{
		.name = "lrint",
		.type = "Function[Union[Int, Float]] -> Int",
		.detail = "math::lrint(value: Union[Int, Float]) -> Int",
		.kind = textension_function,
		.function = math_lrint,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	},
	{
		.name = "llrint",
		.type = "Function[Union[Int, Float]] -> Int",
		.detail = "math::llrint(value: Union[Int, Float]) -> Int",
		.kind = textension_function,
		.function = math_llrint,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	},
	{
		.name = "round",
		.type = "Function[Union[Int, Float]] -> Float",
		.detail = "math::round(value: Union[Int, Float]) -> Float",
		.kind = textension_function,
		.function = math_round,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	},
	{
		.name = "lround",
		.type = "Function[Union[Int, Float]] -> Int",
		.detail = "math::lround(value: Union[Int, Float]) -> Int",
		.kind = textension_function,
		.function = math_lround,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	},
	{
		.name = "llround",
		.type = "Function[Union[Int, Float]] -> Int",
		.detail = "math::llround(value: Union[Int, Float]) -> Int",
		.kind = textension_function,
		.function = math_llround,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	},
	{
		.name = "trunc",
		.type = "Function[Union[Int, Float]] -> Float",
		.detail = "math::trunc(value: Union[Int, Float]) -> Float",
		.kind = textension_function,
		.function = math_trunc,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	},
	{
		.name = "fmod",
		.type = "Function[Union[Int, Float], Union[Int, Float]] -> Float",
		.detail = "math::fmod(left: Union[Int, Float], right: Union[Int, Float]) -> Float",
		.kind = textension_function,
		.function = math_fmod,
		.minimum_arguments = 2,
		.maximum_arguments = 2
	},
	{
		.name = "remainder",
		.type = "Function[Union[Int, Float], Union[Int, Float]] -> Float",
		.detail = "math::remainder(left: Union[Int, Float], right: Union[Int, Float]) -> Float",
		.kind = textension_function,
		.function = math_remainder,
		.minimum_arguments = 2,
		.maximum_arguments = 2
	},
	{
		.name = "remquo",
		.type = "Function[Union[Int, Float], Union[Int, Float]] -> Pair",
		.detail = "math::remquo(left: Union[Int, Float], right: Union[Int, Float]) -> Pair",
		.kind = textension_function,
		.function = math_remquo,
		.minimum_arguments = 2,
		.maximum_arguments = 2
	},
	{
		.name = "copysign",
		.type = "Function[Union[Int, Float], Union[Int, Float]] -> Float",
		.detail = "math::copysign(left: Union[Int, Float], right: Union[Int, Float]) -> Float",
		.kind = textension_function,
		.function = math_copysign,
		.minimum_arguments = 2,
		.maximum_arguments = 2
	},
	{
		.name = "nextafter",
		.type = "Function[Union[Int, Float], Union[Int, Float]] -> Float",
		.detail = "math::nextafter(left: Union[Int, Float], right: Union[Int, Float]) -> Float",
		.kind = textension_function,
		.function = math_nextafter,
		.minimum_arguments = 2,
		.maximum_arguments = 2
	},
	{
		.name = "fdim",
		.type = "Function[Union[Int, Float], Union[Int, Float]] -> Float",
		.detail = "math::fdim(left: Union[Int, Float], right: Union[Int, Float]) -> Float",
		.kind = textension_function,
		.function = math_fdim,
		.minimum_arguments = 2,
		.maximum_arguments = 2
	},
	{
		.name = "fmax",
		.type = "Function[Union[Int, Float], Union[Int, Float]] -> Float",
		.detail = "math::fmax(left: Union[Int, Float], right: Union[Int, Float]) -> Float",
		.kind = textension_function,
		.function = math_fmax,
		.minimum_arguments = 2,
		.maximum_arguments = 2
	},
	{
		.name = "fmin",
		.type = "Function[Union[Int, Float], Union[Int, Float]] -> Float",
		.detail = "math::fmin(left: Union[Int, Float], right: Union[Int, Float]) -> Float",
		.kind = textension_function,
		.function = math_fmin,
		.minimum_arguments = 2,
		.maximum_arguments = 2
	},
	{
		.name = "fma",
		.type = "Function[Union[Int, Float], Union[Int, Float], Union[Int, Float]] -> Float",
		.detail = "math::fma(first: Union[Int, Float], second: Union[Int, Float], third: Union[Int, Float]) -> Float",
		.kind = textension_function,
		.function = math_fma,
		.minimum_arguments = 3,
		.maximum_arguments = 3
	},
	{
		.name = "eleinv",
		.type = "Function[Union[Int, Float]] -> Float",
		.detail = "math::eleinv(value: Union[Int, Float]) -> Float",
		.kind = textension_function,
		.function = math_eleinv,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	},
	{
		.name = "make_nan",
		.type = "Function[] -> Float",
		.detail = "math::make_nan() -> Float",
		.kind = textension_function,
		.function = math_make_nan,
		.minimum_arguments = 0,
		.maximum_arguments = 0
	},
	{
		.name = "isfinite",
		.type = "Function[Union[Int, Float]] -> Bool",
		.detail = "math::isfinite(value: Union[Int, Float]) -> Bool",
		.kind = textension_function,
		.function = math_isfinite,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	},
	{
		.name = "isinf",
		.type = "Function[Union[Int, Float]] -> Bool",
		.detail = "math::isinf(value: Union[Int, Float]) -> Bool",
		.kind = textension_function,
		.function = math_isinf,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	},
	{
		.name = "isnan",
		.type = "Function[Union[Int, Float]] -> Bool",
		.detail = "math::isnan(value: Union[Int, Float]) -> Bool",
		.kind = textension_function,
		.function = math_isnan,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	},
	{
		.name = "isnormal",
		.type = "Function[Union[Int, Float]] -> Bool",
		.detail = "math::isnormal(value: Union[Int, Float]) -> Bool",
		.kind = textension_function,
		.function = math_isnormal,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	},
	{
		.name = "fpclassify",
		.type = "Function[Union[Int, Float]] -> Int",
		.detail = "math::fpclassify(value: Union[Int, Float]) -> Int",
		.kind = textension_function,
		.function = math_fpclassify,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	},
	{
		.name = "signbit",
		.type = "Function[Union[Int, Float]] -> Bool",
		.detail = "math::signbit(value: Union[Int, Float]) -> Bool",
		.kind = textension_function,
		.function = math_signbit,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	},
	{
		.name = "isgreater",
		.type = "Function[Union[Int, Float], Union[Int, Float]] -> Bool",
		.detail = "math::isgreater(left: Union[Int, Float], right: Union[Int, Float]) -> Bool",
		.kind = textension_function,
		.function = math_isgreater,
		.minimum_arguments = 2,
		.maximum_arguments = 2
	},
	{
		.name = "isgreaterequal",
		.type = "Function[Union[Int, Float], Union[Int, Float]] -> Bool",
		.detail = "math::isgreaterequal(left: Union[Int, Float], right: Union[Int, Float]) -> Bool",
		.kind = textension_function,
		.function = math_isgreaterequal,
		.minimum_arguments = 2,
		.maximum_arguments = 2
	},
	{
		.name = "isless",
		.type = "Function[Union[Int, Float], Union[Int, Float]] -> Bool",
		.detail = "math::isless(left: Union[Int, Float], right: Union[Int, Float]) -> Bool",
		.kind = textension_function,
		.function = math_isless,
		.minimum_arguments = 2,
		.maximum_arguments = 2
	},
	{
		.name = "islessequal",
		.type = "Function[Union[Int, Float], Union[Int, Float]] -> Bool",
		.detail = "math::islessequal(left: Union[Int, Float], right: Union[Int, Float]) -> Bool",
		.kind = textension_function,
		.function = math_islessequal,
		.minimum_arguments = 2,
		.maximum_arguments = 2
	},
	{
		.name = "islessgreater",
		.type = "Function[Union[Int, Float], Union[Int, Float]] -> Bool",
		.detail = "math::islessgreater(left: Union[Int, Float], right: Union[Int, Float]) -> Bool",
		.kind = textension_function,
		.function = math_islessgreater,
		.minimum_arguments = 2,
		.maximum_arguments = 2
	},
	{
		.name = "isunordered",
		.type = "Function[Union[Int, Float], Union[Int, Float]] -> Bool",
		.detail = "math::isunordered(left: Union[Int, Float], right: Union[Int, Float]) -> Bool",
		.kind = textension_function,
		.function = math_isunordered,
		.minimum_arguments = 2,
		.maximum_arguments = 2
	}
};

const textension_module tstdlib_math_module = {
	.scope = textension_package,
	.name = "math",
	.detail = "Mathematical functions",
	.symbols = symbols,
	.symbol_count = sizeof(symbols) / sizeof(symbols[0])
};
