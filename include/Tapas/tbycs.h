#ifndef Tap_BYCODES_H
#define Tap_BYCODES_H

#include "tbasis.h"

namespace tapas
{

/** Single Tap bycode
 *  @details This class defines the basic type of bycodes in Tap.
 *  There are 5 basic types: no params, U, CP, LR and Lbi.
 *  The construction methods and parameters-picking methods
 *  of each bycodes are also defined here.
 *  There is no Limit checking in the definition of tbycode
 */
class tbycode
{
private:
/// Instruction - uint32_t value
uint32_t __basic_ins;

public:

/// Generate the bycode of OP_PASS
tbycode()
{
	__basic_ins = ((uint32_t(OP_PASS) << 26) >> 26);
}

/// Generate a bycode without parameters
tbycode(uint8_t ins)
{
	__basic_ins = 0; __basic_ins += ((ins << 2) >> 2);
}

/** Generate instructions of U type:
 *  U size = 26 bite.
 */
tbycode(uint8_t ins, uint32_t u)
{
	__basic_ins = 0;
	__basic_ins += (u << 6);
	__basic_ins += ((ins << 2) >> 2);
}

/// @return unsigned integer U of instruction Op-U
uint32_t get_U()
{
	return __basic_ins >> 6;
}

/// Add A to U for instructions Op-U
void plus_U_by(uint32_t A)
{
	__basic_ins += (A << 6);
}

/** Generate instructions of LR type:
 *  L size = 13 bite;
 *  R size = 13 bite.
 */
tbycode(uint8_t ins, uint16_t L, uint16_t R)
{
	__basic_ins = 0;
	uint32_t iL = L;
	uint32_t iR = R;
	__basic_ins += (iR << 19);
	__basic_ins += ((iL << 19) >> 13);
	__basic_ins += ((ins << 2) >> 2);
}

/// @return uint16_t of L of instruction Op-L-R / Op-L-b-i
uint16_t get_L()
{
	return ((__basic_ins << 13) >> 19);
}

/// @return uint16_t of R of instruction Op-L-R
uint16_t get_R()
{
	return (__basic_ins >> 19);
}

/** Generate instructions of CP type:
 *  C size = 18 bite;
 *  P size =  8 bite.
 */
tbycode(uint8_t ins, uint32_t C, uint8_t P)
{
	__basic_ins = 0;
	uint32_t iP = P;
	__basic_ins += (iP << 24);
	__basic_ins += ((C << 14) >> 8);
	__basic_ins += ((ins << 2) >> 2);
}

/// @return uint32_t of C of instruction Op-C-P
uint32_t get_C()
{
	return ((__basic_ins << 8) >> 14);
}

/// @return uint8_t of P of instruction Op-C-P
uint8_t get_P()
{
	return (__basic_ins >> 24);
}

/** Generate instructions of Lbi type:
 *  L size = 13 bite;
 *  b size = 8  bite;
 *  i size = 5  bite.
 */
tbycode(uint8_t ins, uint16_t L, uint8_t b, uint8_t i)
{
	__basic_ins = 0;
	uint32_t iL = L;
	uint32_t ib = b;
	uint32_t ii = i;
	__basic_ins += (ii << 27);
	__basic_ins += ((ib << 24) >> 5);
	__basic_ins += ((iL << 19) >> 13);
	__basic_ins += ((ins << 2) >> 2);
}

/// @return uint8_t of b of instruction Op-L-b-i
uint8_t get_b()
{
	return ((__basic_ins << 5) >> 24);
}

/// @return uint8_t of i of instruction Op-L-b-i
uint8_t get_i()
{
	return (__basic_ins >> 27);
}

/// @return instruction type
tins ins()
{
	return tins((__basic_ins << 26) >> 26);
}

/// Set instruction type
void set_ins(uint8_t ins)
{
	__basic_ins >>= 6;
	__basic_ins <<= 6;
	__basic_ins += ((ins << 2) >> 2);
}

/// @return a string of the instruction
std::string tostring()
{
	tins ins = this->ins();
	std::string is;

	switch (ins)
	{
	case OP_PASS:
		is += "OP_PASS     ";
		break;
	case OP_VCRT:
		is += "OP_VCRT     ";
		is += std::to_string(get_C()) + "  " + std::to_string(get_P());
		break;
	case OP_TMPDEL:
		is += "OP_TMPDEL   ";
		is += std::to_string(get_U());
		break;
	case OP_THIS:
		is += "OP_THIS     ";
		break;
	case OP_BASE:
		is += "OP_BASE     ";
		break;
	case OP_BREAK:
		is += "OP_BREAK    ";
		break;
	case OP_CONTI:
		is += "OP_CONTI    ";
		break;
	case OP_RET:
		is += "OP_RET      ";
		break;
	case OP_IN:
		is += "OP_IN       ";
		break;
	case OP_PAIR:
		is += "OP_PAIR     ";
		break;
	case OP_TO:
		is += "OP_TO       ";
		break;
	case OP_POPN:
		is += "OP_POPN     ";
		is += std::to_string(get_L()) + "  " + std::to_string(get_R());
		break;
	case OP_POPCOV:
		is += "OP_POPCOV   ";
		is += std::to_string(get_L()) + "  " + std::to_string(get_R());
		break;
	case OP_LOOPAS:
		is += "OP_LOOPAS   ";
		is += std::to_string(get_L()) + "  " + std::to_string(get_R());
		break;
	case OP_LOOPIAS:
		is += "OP_LOOPIAS  ";
		is += std::to_string(get_L()) + "  " + std::to_string(get_R());
		break;
	case OP_LOOPLAS:
		is += "OP_LOOPLS   ";
		is += std::to_string(get_L()) + "  " + std::to_string(get_R());
		break;
	case OP_LOOPGAS:
		is += "OP_LOOPGAS  ";
		is += std::to_string(get_L()) + "  " + std::to_string(get_R());
		break;
	case OP_JPF:
		is += "OP_JPF      ";
		is += std::to_string(get_U());
		break;
	case OP_JPB:
		is += "OP_JPB      ";
		is += std::to_string(get_U());
		break;
	case OP_CJPFPOP:
		is += "OP_CJPFPOP  ";
		is += std::to_string(get_U());
		break;
	case OP_CJPBPOP:
		is += "OP_CJPBPOP  ";
		is += std::to_string(get_U());
		break;
	case OP_PUSHX:
		is += "OP_PUSHX    ";
		is += std::to_string(get_L()) + "  " + std::to_string(get_R());
		break;
	case OP_PUSHI:
		is += "OP_PUSHI    ";
		is += std::to_string(get_U());
		break;
	case OP_PUSHD:
		is += "OP_PUSHD    ";
		is += std::to_string(get_U());
		break;
	case OP_PUSHB:
		is += "OP_PUSHB    ";
		is += std::to_string(get_U());
		break;
	case OP_PUSHS:
		is += "OP_PUSHS    ";
		is += std::to_string(get_U());
		break;
	case OP_PUSHDICT:
		is += "OP_PUSHDICT ";
		is += std::to_string(get_U());
		break;
	case OP_PUSHINFO:
		is += "OP_PUSHINFO ";
		is += std::to_string(get_U());
		break;
	case OP_IMPORT:
		is += "OP_IMPORT   ";
		is += std::to_string(get_U());
		break;
	case OP_IDXR:
		is += "OP_IDXR     ";
		is += std::to_string(get_U());
		break;
	case OP_EVAL:
		is += "OP_EVAL     ";
		is += std::to_string(get_U());
		break;
	case OP_EVALSF:
		is += "OP_EVALSF   ";
		is += std::to_string(get_U());
		break;
	case OP_EVALCF:
		is += "OP_EVALCF   ";
		is += std::to_string(get_U());
		break;
	case OP_EVALTF:
		is += "OP_EVALTF   ";
		is += std::to_string(get_U());
		break;
	case OP_IDXL:
		is += "OP_IDXL     ";
		is += std::to_string(get_L()) + "  " + std::to_string(get_b()) + \
						"  " + std::to_string(get_i());
		break;
	case OP_PUSHF:
		is += "OP_PUSHF    ";
		is += std::to_string(get_U());
		break;
	case OP_ADD:
		is += "OP_ADD      ";
		is += std::to_string(get_L()) + "  " + std::to_string(get_R());
		break;
	case OP_SUB:
		is += "OP_SUB      ";
		is += std::to_string(get_L()) + "  " + std::to_string(get_R());
		break;
	case OP_MUL:
		is += "OP_MUL      ";
		is += std::to_string(get_L()) + "  " + std::to_string(get_R());
		break;
	case OP_DIV:
		is += "OP_DIV      ";
		is += std::to_string(get_L()) + "  " + std::to_string(get_R());
		break;
	case OP_MOD:
		is += "OP_MOD      ";
		is += std::to_string(get_L()) + "  " + std::to_string(get_R());
		break;
	case OP_POW:
		is += "OP_POW      ";
		is += std::to_string(get_L()) + "  " + std::to_string(get_R());
		break;
	case OP_MMUL:
		is += "OP_MMUL     ";
		is += std::to_string(get_L()) + "  " + std::to_string(get_R());
		break;
	case OP_EQ:
		is += "OP_EQ       ";
		is += std::to_string(get_L()) + "  " + std::to_string(get_R());
		break;
	case OP_NE:
		is += "OP_NE       ";
		is += std::to_string(get_L()) + "  " + std::to_string(get_R());
		break;
	case OP_GE:
		is += "OP_GE       ";
		is += std::to_string(get_L()) + "  " + std::to_string(get_R());
		break;
	case OP_SG:
		is += "OP_SG       ";
		is += std::to_string(get_L()) + "  " + std::to_string(get_R());
		break;
	case OP_LE:
		is += "OP_LE       ";
		is += std::to_string(get_L()) + "  " + std::to_string(get_R());
		break;
	case OP_SL:
		is += "OP_SL       ";
		is += std::to_string(get_L()) + "  " + std::to_string(get_R());
		break;
	case OP_AND:
		is += "OP_AND      ";
		is += std::to_string(get_L()) + "  " + std::to_string(get_R());
		break;
	case OP_OR:
		is += "OP_OR       ";
		is += std::to_string(get_L()) + "  " + std::to_string(get_R());
		break;
	}
	return is;
}

};

/** STL vector of bycodes.
 *  @details Used only in compilation & optimization
 *  period. Once optimization period ends, the length bycodes is fixed and
 *  bycodes will be stored in a vector.
 */
class tvmcmd_vect : public std::vector<tbycode>
{
public:

/// @return the length of this bycode vector
uint_size_cmd size32()
{
	return static_cast<uint_size_cmd>(size());
}

/// Append a bycode to the end
void append(const tbycode cmd)
{
	if (size() == CMDLIST_SIZE_LIMIT - 1)
		twarn(ErrCompile_CMDOutOfLimit).warn("tvmcmd_vect::append", "");
	push_back(cmd);
}

};

/** STL vector of constant (literal) strings in Tap source code
 *  @details
 *  Only used in compilation period. Once the compilation is finished, all
 *  constant strings will be stored in a "char **" vector with length infos
 *  also stored.
 */
class consts_str_vect : public std::vector<std::string>
{
public:

/// @return the length of this vector of Constant of strings.
uint_size_cst size32()
{
	return static_cast<uint_size_cst>(size());
}

/// Append a string to the end
void append(const std::string str)
{
	if (size32() == LITLIST_SIZE_LIMIT - 1)
		twarn(ErrCompile_CSTOutOfLimit).warn("consts_str_vect::append", "");
	push_back(str);
}

};

/** STL vector of constant (literal) integers in Tap source code
 *  @details
 *  Only used in compilation period. Once the compilation is finished, all
 *  constant integers will be stored in a "int *" vector with length infos
 *  also stored.
 */
class consts_long_vect : public std::vector<long>
{
public:

/// @return the size of this vector of Constant integer
uint_size_cst size32()
{
	return static_cast<uint_size_cst>(size());
}

/// Append an integer to the end
void append(const long v)
{
	if (size32() == LITLIST_SIZE_LIMIT - 1)
		twarn(ErrCompile_CSTOutOfLimit).warn("consts_long_vect::append", "");
	push_back(v);
}

};

/** STL vector of constant (literal) floats in Tap source code
 *  @details
 *  Only used in compilation period. Once the compilation is finished, all
 *  constant floats will be stored in a "double *" vector with length infos
 *  also stored.
 */
class consts_double_vect : public std::vector<double>
{
public:

/// @return the size of this vector of double float
uint_size_cst size32()
{
	return static_cast<uint_size_cst>(size());
}

/// Append a double float to the end
void append(const double v)
{
	if (size32() == LITLIST_SIZE_LIMIT - 1)
		twarn(ErrCompile_CSTOutOfLimit).warn("consts_double_vect::append", "");
	push_back(v);
}

};

/** Combinnation of all constant vectors in Tap source codes
 *  @details
 *  Including constant integers, floats and strings. Methods of recording and
 *  managing the searching, indexing and appending of the constants are also
 *  defined here.
 */
struct tconsts
{
consts_str_vect __strcsts;    ///< all string consts
consts_long_vect __intcsts;   ///< all int consts
consts_double_vect __dblcsts; ///< all double consts

/// Add `str` to string constants list
/// @return the location of string `str` in string constant list
uint_size_cst add_str_const(const std::string & str)
{
	auto iter = __strcsts.cbegin();
	uint_size_cst idx = 0;

	for (; iter != __strcsts.cend(); iter++)
	{
		if (str == *iter) return idx;
		idx++;
	}
	__strcsts.append(str);
	return __strcsts.size32() - 1; // this value is positive for sure
}

/// Add `l` to integer constants list
/// @return the location of integer `l` in integer constant list
uint_size_cst add_int_const(const long & l)
{
	auto iter = __intcsts.cbegin();
	uint_size_cst idx = 0;

	for (; iter != __intcsts.cend(); iter++)
	{
		if (l == *iter) return idx;
		idx++;
	}
	__intcsts.append(l);
	return __intcsts.size32() - 1;
}

/// Add `d` to double float constant list
/// @return the location of double float `d` in double float constant list
uint_size_cst add_double_const(const double & d)
{
	auto iter = __dblcsts.cbegin();
	uint_size_cst idx = 0;

	for (; iter != __dblcsts.cend(); iter++)
	{
		if (d == *iter)
			return idx;
		idx++;
	}
	__dblcsts.append(d);
	return __dblcsts.size32() - 1;
}

struct tconsts copy()
{
	return {
		consts_str_vect(__strcsts),
		consts_long_vect(__intcsts),
		consts_double_vect(__dblcsts),
	};
}

};

/** Compilation infos
 *  @details Including the maximum number of object declared,
 *  and the maximum number of registers used (VM stack length).
 */
struct tcinfo
{
	uint_size_obj obj_max; ///< maximum number of environmental objects
	uint_size_obj tmp_max; ///< maximum number of temporary objects
	uint_size_reg reg_max; ///< maximum number of stack location used
	uint16_t padding_1;
};

/** Constant value lists of Tap bycode file
 *  @details After compilation, constants and their length infos will be
 *  stored here as the "plain" values. This is the data that Tap VM will
 *  load and execute.
 */
struct twrapper_consts
{
	uint_size_cst ncints; ///< number of constant integers
	uint_size_cst ncstrs; ///< number of constant strins
	uint_size_cst ncdbls; ///< number of double floats
	long * cints;     ///< integer constants list
	char ** cstrs;    ///< string constants list
	double * cdbls;   ///< double float constants list
};

/** Tap bycodes wrapper
 *  @details After compilation, all bycodes, constants and compilation infos
 *  will be stored here as the "plain" values. This is the data that Tap VM
 *  will load and execute.
 */
struct twrapper
{
	twrapper_consts consts; ///< constant lists
	tbycode * cmdarr;       ///< list of bycodes
	tcinfo info;            ///< other compilation informations
	uint_size_cmd ncmds;        ///< number of bycodes
};

/** Tap bycodes management class
 *  @details This class is used for
 *  (1) Static optimization: make bycodes of tvmcmd_vect better.
 *  (2) Wrapper generation: turn the STL constants into plain values.
 *  (3) Wrapper store: store the plain valued wrapper onto hard disk
 *      (as .tapc file).
 *  (4) Wrapper loading: load the .tapc file and get wrapper.
 *  (5) Wrapper clean: release wrapper.
 */
class tanalyser
{
private:

twrapper * make_wrapper(tvmcmd_vect & tcmds, tconsts & consts, tcinfo & info)
{
	twrapper * wrapper = new twrapper();
	wrapper->info = info;
	wrapper->ncmds = tcmds.size32();
	wrapper->cmdarr = new tbycode[wrapper->ncmds]();
	tbycode * cmd_i = wrapper->cmdarr;

	for (auto iter = tcmds.cbegin(); iter != tcmds.cend(); iter++) {
		*cmd_i = *iter;
		cmd_i ++;
	}
	wrapper->consts.ncstrs = consts.__strcsts.size32();
	wrapper->consts.ncints = consts.__intcsts.size32();
	wrapper->consts.ncdbls = consts.__dblcsts.size32();

	if (wrapper->consts.ncstrs > 0) {
		wrapper->consts.cstrs = new char*[wrapper->consts.ncstrs];
		uint_size_cst str_idx = 0;

		for (auto iter = consts.__strcsts.cbegin(); iter != consts.__strcsts.cend(); ) {
			std::string s = *iter;
			uint_size len = s.length() + 1;
			wrapper->consts.cstrs[str_idx] = new char[len];
			std::copy(s.c_str(), s.c_str() + len, wrapper->consts.cstrs[str_idx]);
			iter = consts.__strcsts.erase(iter); // iter - add by one.
			str_idx ++;
		}
	}
	else
		wrapper->consts.cstrs = nullptr;

	if (wrapper->consts.ncints > 0) {
		wrapper->consts.cints = new long[wrapper->consts.ncints];
		uint_size_cst int_idx = 0;

		for (auto iter = consts.__intcsts.cbegin(); iter != consts.__intcsts.cend(); iter++) {
			wrapper->consts.cints[int_idx] = *iter;
			int_idx++;
		}
	}
	else
		wrapper->consts.cints = nullptr;

	if (wrapper->consts.ncdbls > 0) {
		wrapper->consts.cdbls = new double[wrapper->consts.ncdbls];
		uint_size_cst dbl_idx = 0;

		for (auto iter = consts.__dblcsts.cbegin(); iter != consts.__dblcsts.cend(); iter++) {
			wrapper->consts.cdbls[dbl_idx] = *iter;
			dbl_idx++;
		}
	}
	else wrapper->consts.cdbls = nullptr;
	return wrapper;
}

public:

/// Do static analysis of bycodes and then make a wrapper
twrapper * wrap(tvmcmd_vect & tcmds, tconsts & consts, tcinfo & info)
{
	/// TO DO SOME OPTIMIMIZATION ......
	return make_wrapper(tcmds, consts, info);
}

/// Save wrapper onto hard disk
void save_bin_file(const twrapper * wrapper, const std::string & file)
{
	FILE * f = fopen(file.c_str(), "wb");

	if (nullptr == f)
		twarn(ErrSession_IO).warn("tanalyser::save_bin_file", "");

	// write 'wrapper'
	fwrite(wrapper, sizeof (twrapper), 1, f);

	// write 'wrapper.cmdarr'
	fwrite(wrapper->cmdarr, sizeof(tbycode), wrapper->ncmds, f);

	// write 'wrapper.consts.cints'
	fwrite(wrapper->consts.cints, sizeof(long), wrapper->consts.ncints, f);

	// write 'wrapper.consts.cdbls'
	fwrite(wrapper->consts.cdbls, sizeof(double), wrapper->consts.ncdbls, f);

	// write 'wrapper.consts.cstrs'
	char ** str = wrapper->consts.cstrs;

	for (uint_size_cst i = 0; i < wrapper->consts.ncstrs; i++) {
		uint_size len_i = std::string(*str).size();
		fwrite(&len_i, sizeof(uint_size), 1, f);
		fwrite(*str, 1, len_i + 1, f);
		str++;
	}
	fclose(f);
}

/// Load wrapper from hard disk
twrapper * load_bin_file(const std::string & file)
{
	FILE * binf = fopen(file.c_str(), "rb");

	if (nullptr == binf) {
		twarn(ErrSession_IO).warn("tanalyser::load_bin_file", file);
		return nullptr;
	}

	// read 'wrapper'
	twrapper * wrapper = new twrapper();

	if (1 != fread(wrapper, sizeof(twrapper), 1, binf))
		twarn(ErrSession_IO).warn("tanalyser::load_bin_file", "");

	// read 'wrapper.cmdarr'
	uint_size_cmd cmdlen = wrapper->ncmds;
	tbycode * cmdarr = new tbycode[cmdlen]();
	if (cmdlen != fread(cmdarr, sizeof(tbycode), cmdlen, binf))
		twarn(ErrSession_IO).warn("tanalyser::load_bin_file", "");
	wrapper->cmdarr = cmdarr;

	// read 'wrapper.consts.cints'
	uint_size_cst ncints = wrapper->consts.ncints;

	if (ncints > 0) {
		long * cints = new long[ncints]();
		if (ncints != fread(cints, sizeof(long), ncints, binf))
			twarn(ErrSession_IO).warn("tanalyser::load_bin_file", "");
		wrapper->consts.cints = cints;
	}

	// read 'wrapper.consts.cdbls'
	uint_size_cst ncdbls = wrapper->consts.ncdbls;

	if (ncdbls > 0) {
		double * cdbls = new double[ncdbls]();
		if (ncdbls != fread(cdbls, sizeof(double), ncdbls, binf))
			twarn(ErrSession_IO).warn("tanalyser::load_bin_file", "");
		wrapper->consts.cdbls = cdbls;
	}

	// read 'wrapper.consts.cstrs'
	uint_size_cst ncstrs = wrapper->consts.ncstrs;

	if (ncstrs > 0) {
		wrapper->consts.cstrs = new char*[ncstrs]();

		for (uint_size_cst i = 0; i<ncstrs; i++) {
			unsigned long len_i = 0;

			if (1 != fread(&len_i, sizeof (unsigned long), 1, binf))
				twarn(ErrSession_IO).warn("tanalyser::load_bin_file", "");
			char * str_i = new char[len_i + 1]();

			if (len_i + 1 != fread(str_i, 1, len_i + 1, binf))
				twarn(ErrSession_IO).warn("tanalyser::load_bin_file", "");
			wrapper->consts.cstrs[i] = str_i;
		}
	}
	fclose(binf);
	return wrapper;
}

/// Release wrapper
void clean_wrapper(twrapper * wrapper)
{
	if (nullptr == wrapper)
		return;
	if (wrapper->cmdarr != nullptr)
		delete [] wrapper->cmdarr;
	if (wrapper->consts.ncdbls > 0 && wrapper->consts.cdbls != nullptr)
		delete [] wrapper->consts.cdbls;
	if (wrapper->consts.ncints > 0 && wrapper->consts.cints != nullptr)
		delete [] wrapper->consts.cints;
	if (wrapper->consts.ncstrs > 0 && wrapper->consts.cstrs != nullptr) {
		for (uint_size_cst i = 0; i < wrapper->consts.ncstrs; i++) {
			if (wrapper->consts.cstrs[i] != nullptr)
				delete [] wrapper->consts.cstrs[i];
		}
		delete [] wrapper->consts.cstrs;
	}
	delete wrapper;
}

/// Display wrapper
void display_wrapper(const twrapper * wrapper)
{
	tbycode * iter = wrapper->cmdarr;

	for (uint_size_cmd i = 0; i < wrapper->ncmds; i++)
		printf("[%i]%s\n", i, iter[i].tostring().c_str());
	printf("Max Obj. Number: %u\n", wrapper->info.obj_max);
	printf("Max Tmp. Number: %u\n", wrapper->info.tmp_max);
	printf("Max Reg. Number: %u\n", wrapper->info.reg_max);
	printf("Const Value List (Integers): ");

	for (uint_size_cst i = 0; i < wrapper->consts.ncints; i++) {
		printf("%li", wrapper->consts.cints[i]);
		if (i < wrapper->consts.ncints - 1) printf(", ");
	}
	printf("\n");
	printf("Const Value List (Double Floats): ");

	for (uint_size_cst i = 0; i < wrapper->consts.ncdbls; i++) {
		printf("%f", wrapper->consts.cdbls[i]);
		if (i < wrapper->consts.ncdbls - 1) printf(", ");
	}
	printf("\n");
	printf("Const Value List (Character Strings): ");

	for (uint_size_cst i = 0; i < wrapper->consts.ncstrs; i++) {
		printf("%s", wrapper->consts.cstrs[i]);
		if (i < wrapper->consts.ncstrs - 1) printf(", ");
	}
	printf("\n");
}

};

}

#endif // Tap_BYCODES_H
