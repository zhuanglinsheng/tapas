#ifndef Tap_CONSTS_H
#define Tap_CONSTS_H

// if Visual C++ is used
#ifdef _MSC_VER
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#endif

/* ctypes */
#include <cstdio>
#include <cstdint>
#include <cmath>

/* cpp stl */
#include <string>
#include <vector>

namespace tapas
{

#define Tap_Version     "1.0"
#define Tap_Year        "2021"
#define Tap_Author      "zhuanglinsheng@outlook.com"
#define Tap_Basic_Info  Tap_Version, Tap_Year, Tap_Author

//=============================================================================
// Limits in Tapas
//===========================================================================//

/// `int_unit_ctr` is used in `tapas::tunit_ctr` for lexing
/// Note that we use signed integers in counter since Tapas users may write
/// script wronly, like `) ... (`, which leads to a negative counting of
/// parenthesis.
typedef int64_t int_unit_ctr;

/// uint_size is an alias of `size_t`
typedef size_t uint_size;

/// The maximum number of parameter `U` of the bycode (U tyoe) can express.
/// Alias for (2^26) - 1 = 67,108,863
#define Limit_U static_cast<uint32_t>(67108863)

/// The maximum number of parameter `L` and `R` of the bycode (LR type) can express.
/// Alias for (2^13) - 1 = 8,191
#define Limit_LR static_cast<uint16_t>(8191)

/// The maximum number of parameter `C` of the bycode (CP tyoe) can express.
/// Alias for (2^18) - 1 = 262,143
#define Limit_C static_cast<uint32_t>(262143)

/// The maximum number of parameter `P` of the bycode (CP tyoe) can express.
/// Alias for (2^08) - 1 = 255
#define Limit_P static_cast<uint8_t>(255)

/// The size type for bycode list.
/// Alias for uint32_t, the location in of bycodes list
typedef uint32_t uint_size_cmd;

/// The size type for constant list.
/// Alias for uint16_t, the location in constant list
typedef uint32_t uint_size_cst;

/// The size type for object list.
/// Alias for uint16_t, the location in variable list
typedef uint16_t uint_size_obj;

/// The size type for stack.
/// Alias for uint8_t, the location in vm stack
typedef uint8_t  uint_size_stk;

/// Each module can contains at most this number of bycodes.
/// Alias for C_Limit (67,108,863), the limit of a commands
#define CMDLIST_SIZE_LIMIT Limit_U

/// Each module can contains at most this number of literals in its literal table.
/// Alias for R_Limit (262,143), the limit of literals list
#define LITLIST_SIZE_LIMIT Limit_C

/// Each module can contains at most this number of objects in its
/// Alias for R_Limit (8,191), the limit of Tap object list
#define OBJLIST_SIZE_LIMIT Limit_LR

/// Alias for Limit_P (255), the limit of VM stack
#define REGLIST_SIZE_LIMIT Limit_P

/// Mark for cpp functions whose nparams is undetermined
#define UNDEF_NPARAMS REGLIST_SIZE_LIMIT

/// Mark that the object has no name
#define UNDEF_NAMELOC LITLIST_SIZE_LIMIT

/// Mark that the object is not an environmental object
#define UNDEF_ENVLOC OBJLIST_SIZE_LIMIT

/** Type of Tap bycodes
 *  @details See the file tap_bycs.h for more details of their operations.
 *  The type of bycodes in Tap includes
 *  - **no params**
 *  - **U** (1 parameter)
 *  - **CP** (2 parameters)
 *  - **LR** (2 parameters)
 *  - **Lbi** (3 parameters)
 *
 *  Explanations of the parameters of Tap bycodes:
 *  - **nreg**    - number of objects pop out of stack.
 *  - **oloc**    - location of object in object list.
 *  - **cloc**    - location of constant in constant list.
 *  - **ncmd**    - number of commands in command list.
 *  - **nparams** - number of parameters to be pushed in stack.
 */
enum tins : uint8_t
{
	OP_PASS,      ///< no params
	OP_VCRT,      ///< CP  - cloc, isenv
	OP_TMPDEL,    ///< U   - nobj
	OP_THIS,      ///< no params
	OP_BASE,      ///< no params
	OP_BREAK,     ///< no params
	OP_CONTI,     ///< no params
	OP_RET,       ///< no params
	OP_IN,        ///< no params
	OP_PAIR,      ///< no params
	OP_TO,        ///< no params
	OP_POPN,      ///< LR  - nreg, interactive
	OP_POPCOV,    ///< LR  - oloc, isenv
	OP_LOOPAS,    ///< LR  - oloc, isenv
	OP_LOOPIAS,   ///< LR  - oloc, isenv
	OP_LOOPLAS,   ///< LR  - oloc, isenv
	OP_LOOPGAS,   ///< LR  - oloc, isenv
	OP_JPF,       ///< U   - ncmd
	OP_JPB,       ///< U   - ncmd
	OP_CJPFPOP,   ///< U   - ncmd
	OP_CJPBPOP,   ///< U   - ncmd
	OP_PUSHX,     ///< LR  - oloc, isenv
	OP_PUSHI,     ///< U   - cloc
	OP_PUSHD,     ///< U   - cloc
	OP_PUSHB,     ///< U   - cloc
	OP_PUSHS,     ///< U   - cloc
	OP_PUSHDICT,  ///< U   - nparams
	OP_PUSHINFO,  ///< U   - an unsigned integer
	OP_IMPORT,    ///< U   - cloc
	OP_IDXR,      ///< U   - nparams
	OP_EVAL,      ///< U   - nparams
	OP_EVALSF,    ///< U   - nparams
	OP_EVALCF,    ///< U   - nparams
	OP_EVALTF,    ///< U   - nparams
	OP_IDXL,      ///< Lbi - oloc, nreg, isenv
	OP_PUSHF,     ///< U   - ncmd
	OP_ADD,       ///< LR  - oloc, oloc
	OP_SUB,       ///< LR  - oloc, oloc
	OP_MUL,       ///< LR  - oloc, oloc
	OP_DIV,       ///< LR  - oloc, oloc
	OP_MOD,       ///< LR  - oloc, oloc
	OP_POW,       ///< LR  - oloc, oloc
	OP_MMUL,      ///< LR  - oloc, oloc
	OP_EQ,        ///< LR  - oloc, oloc
	OP_NE,        ///< LR  - oloc, oloc
	OP_GE,        ///< LR  - oloc, oloc
	OP_SG,        ///< LR  - oloc, oloc
	OP_LE,        ///< LR  - oloc, oloc
	OP_SL,        ///< LR  - oloc, oloc
	OP_AND,       ///< LR  - oloc, oloc
	OP_OR,        ///< LR  - oloc, oloc
};

/**  The errors in Tap are emitted whenever there is something wrong
 *   @details
 *   Errors could be devided into three categories, which are
 *   - **Compiling error**: Errors detected in compiling process
 *   - **Session error**:   Errors when Tap virtual machine (VM) breaks down
 *   - **Runtime error**:   Errors that are only detected in runtime
 */
enum terror_type : uint8_t
{
	ErrCompile_Other,           ///< Compile Error
	ErrCompile_UnfoundFile,     ///< Compile Error - Unfound File
	ErrCompile_BracketsOpen,    ///< Compile Error - Bracket Open
	ErrCompile_VarNoType,       ///< Compile Error - Variable Declaration of no Type
	ErrCompile_DblVDeclare,     ///< Compile Error - Duplicate Variable Declaration
	ErrCompile_InBlkVarDef,     ///< Compile Error - Variable Declaration in Block
	ErrCompile_ObjUnfound,      ///< Compile Error - Object Unfound
	ErrCompile_InvalidVname,    ///< Compile Error - Invalid Variable Name
	ErrCompile_InvalidLiter,    ///< Compile Error - Invalid Literal Value
	ErrCompile_AsgDefault,      ///< Compile Error - Assign Values to Defaults
	ErrCompile_REGOutOfLimit,   ///< Compile Error - Parameters Overflow
	ErrCompile_CMDOutOfLimit,   ///< Compile Error - Command Overflow
	ErrCompile_OBJOutOfLimit,   ///< Compile Error - Variable Overflow
	ErrCompile_CSTOutOfLimit,   ///< Compile Error - Constants Overflow
	ErrCompile_ReturnTmpObj,    ///< Compile Error - Return Temporary Object
	ErrCompile_InvalidFile,     ///< Compile Error - Invalid File Name/Suffix

	ErrSession_IO,              ///< Session Error - IO

	ErrRuntime_Other,           ///< Runtime Error
	ErrRuntime_DivIntZero,      ///< Runtime Error - Divided by Integer Zero
	ErrRuntime_ParamsCtr,       ///< Runtime Error - Parameters Count Inconsistency
	ErrRuntime_ParamsType,      ///< Runtime Error - Parameters Type Inconsistency
	ErrRuntime_IdxOutRange,     ///< Runtime Error - Index out of Range
	ErrRuntime_InvalidIndex,    ///< Runtime Error - Invalid Index
	ErrRuntime_LoopRef,         ///< Runtime Error - Looping Reference
	ErrRuntime_RefType,         ///< Runtime Error - Referred Type Inconsistency
	ErrRuntime_LenInconsis,     ///< Runtime Error - Length Inconsistency
	ErrRuntime_AssignNil,       ///< Runtime Error - Try to Assign Nil
	ErrRuntime_ObjUnfound,      ///< Runtime Error - Object Unfound
	ErrRuntime_IntOutOfRange,   ///< Runtime Error - Integer Value out of Range
	ErrRuntime_RefEmptySet,     ///< Runtime Error - Refer to the Value of Empty Set
	ErrRuntime_StringEval,      ///< Runtime Error - String Evaluation
	ErrRuntime_EnvInconsis,     ///< Runtime Error - Environment Inconsistecy
	ErrRuntime_RecurseRefRet,   ///< Runtime Error - Return Local Reference in Recursion
};

/// Throw a warning and stop the Tap process
class twarn
{
private:
terror_type __type;

public:
/// Generate a warning of terror_type being 'type'
twarn(terror_type type)
{
	__type = type;
}

/** Dealing with error signals in Tap.
 *  @details Print info and exit the program.
 *  @param fname  The name of the Cpp function where error signal is emitted.
 *  @param info   The error info that you want to print out.
 */
void warn(const std::string & fname, const std::string & info)
{
	switch (__type)
	{
	case ErrCompile_Other:
		printf("Compile Error");
		break;
	case ErrCompile_UnfoundFile:
		printf("Compile Error - Unfound File");
		break;
	case ErrCompile_BracketsOpen:
		printf("Compile Error - Bracket Open");
		break;
	case ErrCompile_VarNoType:
		printf("Compile Error - Variable Declaration of no Type");
		break;
	case ErrCompile_DblVDeclare:
		printf("Compile Error - Duplicate Variable Declaration");
		break;
	case ErrCompile_InBlkVarDef:
		printf("Compile Error - Variable Declaration in Block");
		break;
	case ErrCompile_ObjUnfound:
		printf("Compile Error - Object Unfound");
		break;
	case ErrCompile_InvalidVname:
		printf("Compile Error - Invalid Variable Name");
		break;
	case ErrCompile_InvalidLiter:
		printf("Compile Error - Invalid Literal Value");
		break;
	case ErrCompile_AsgDefault:
		printf("Compile Error - Assign Values to Defaults");
		break;
	case ErrCompile_REGOutOfLimit:
		printf("Compile Error - Parameters Overflow");
		break;
	case ErrCompile_CMDOutOfLimit:
		printf("Compile Error - Command Overflow");
		break;
	case ErrCompile_OBJOutOfLimit:
		printf("Compile Error - Variable Overflow");
		break;
	case ErrCompile_CSTOutOfLimit:
		printf("Compile Error - Constants Overflow");
		break;
	case ErrCompile_ReturnTmpObj:
		printf("Compile Error - Return Temporary Object");
		break;
	case ErrCompile_InvalidFile:
		printf("Compile Error - Invalid File Name/Suffix");
		break;

	case ErrSession_IO:
		printf("Session Error - IO");
		break;

	case ErrRuntime_Other:
		printf("Runtime Error");
		break;
	case ErrRuntime_DivIntZero:
		printf("Runtime Error - Divided by Integer Zero");
		break;
	case ErrRuntime_ParamsCtr:
		printf("Runtime Error - Parameters Count Inconsistency");
		break;
	case ErrRuntime_ParamsType:
		printf("Runtime Error - Parameters Type Inconsistency");
		break;
	case ErrRuntime_RefType:
		printf("Runtime Error - Referred Type Inconsistency");
		break;
	case ErrRuntime_IdxOutRange:
		printf("Runtime Error - Index out of Range");
		break;
	case ErrRuntime_InvalidIndex:
		printf("Runtime Error - Invalid Index");
		break;
	case ErrRuntime_LoopRef:
		printf("Runtime Error - Looping Reference");
		break;
	case ErrRuntime_LenInconsis:
		printf("Runtime Error - Length Inconsistency");
		break;
	case ErrRuntime_AssignNil:
		printf("Runtime Error - Try to Assign Nil");
		break;
	case ErrRuntime_ObjUnfound:
		printf("Runtime Error - Object Unfound");
		break;
	case ErrRuntime_IntOutOfRange:
		printf("Runtime Error - Integer Value out of Range");
		break;
	case ErrRuntime_RefEmptySet:
		printf("Runtime Error - Refer to the Value of Empty Set");
		break;
	case ErrRuntime_StringEval:
		printf("Runtime Error - String Evaluation");
		break;
	case ErrRuntime_EnvInconsis:
		printf("Runtime Error - Environment Inconsistecy");
		break;
	case ErrRuntime_RecurseRefRet:
		printf("Runtime Error - Return Local Reference in Recursion");
		break;
	}
	printf(" - tapas::%s.\n", fname.c_str());
	printf("  %s\n", info.c_str());
	throw "twarn::warn";
}

};


}

#endif // Tap_CONSTS_H
