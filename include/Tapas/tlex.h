#ifndef TLEX_H
#define TLEX_H

#include "tbasis.h"

namespace tapas
{

namespace utils
{

/// trim the ending of a given string and return it.
inline std::string trim_back(const std::string & str)
{
	if (str.empty())
		return str;
	char str_end = str.back();

	if (str_end == '\n' || str_end == '\r'
	|| str_end == '\t' || str_end == ' ' || str_end == EOF)
		return trim_back(str.substr(0, str.length() - 1));
	return str;
}

/// trim the beginning of a given string and return it.
inline std::string trim_front(const std::string & str)
{
	if (str.empty())
		return str;
	char str_bgn = str[0];

	if (str_bgn == '\n' || str_bgn == '\r'
	|| str_bgn == '\t' || str_bgn == ' ' || str_bgn == EOF)
		return trim_front(str.substr(1));
	return str;
}

/// trim a given string and return it.
inline std::string trim(const std::string & str)
{
	return trim_back(trim_front(str));
}

/// Get the folder path where a file is located.
///   @param filepath - the full location of a file
///   @return string - the folder of filepath
inline std::string get_folderpath_from_filepath(const std::string & filepath)
{
	std::string path;
	const uint_size last_slash_idx1 = filepath.rfind('/');
	const uint_size last_slash_idx2 = filepath.rfind('\\');
	if (std::string::npos != last_slash_idx1)
		path = filepath.substr(0, last_slash_idx1);
	if (std::string::npos != last_slash_idx2)
		path = filepath.substr(0, last_slash_idx2);
	return path + ';';
}

/// Append 'paths' (with comma at end as splitter) into 'pathpool' if it is not already in there
inline void append_to_pathpool(std::vector<std::string> & pathpool, const std::string & pathstr)
{
	uint_size loc = 0;
	uint_size len = pathstr.length();

	// scanning the input `pathstr`
	for (uint_size i = 0; i < len; i++) {
		if (pathstr[i] == ';') {
			std::string path = pathstr.substr(loc, i - loc);

			for (auto it = pathpool.begin(); it != pathpool.end(); it++) {
				if (0 == it->compare(path)) // two strings are equal
					goto LOOPEND;
			}
			pathpool.push_back(path);
			loc = i + 1;
		}
		LOOPEND: ;
	}
}

inline bool str_to_long_int(const std::string & cmds, long & it)
{
	return std::string::npos == cmds.find('.')
		&& std::string::npos == cmds.find('e')
		&& std::string::npos == cmds.find('E')
		&& sscanf(cmds.c_str(), "%li", &it) == 1;
}

inline bool str_to_double(const std::string & cmds, double & dt)
{
	char* end = nullptr;
	dt = strtof(cmds.c_str(), &end);
	return end != cmds.c_str() && *end == '\0';
}

}


/*===========================================================================*
 * 1. Lexical Analysis (1): split a string into compilation units
 *===========================================================================*/

// Counting parenthesis, brackets, curly braces and quotes.
//
// Note that here all these counters are signed, since Tapas users may write
// script wrongly like `} ... {`.
class tunit_ctr
{
private:
	int_unit_ctr __parenthesis = 0;
	int_unit_ctr __bracket     = 0;
	int_unit_ctr __curlybrace  = 0;
	int_unit_ctr __singlequote = 0;
	int_unit_ctr __doublequote = 0;

public:

/// @return the number of parenthesis counter
int_unit_ctr get_parenthesis() const
{
	return __parenthesis;
}

/// @return the number of brackets counter
int_unit_ctr get_bracket() const
{
	return __bracket;
}

/// @return the number of curly brackets counter
int_unit_ctr get_curlybrace() const
{
	return __curlybrace;
}

/// Let all fields to be zero
void restore_lex_ctrs()
{
	__parenthesis = 0;
	__bracket     = 0;
	__curlybrace  = 0;
	__singlequote = 0;
	__doublequote = 0;
}

/// Update fields by checking the char `c`
bool update_lex_ctrs(const char c)
{
	switch (c) {
	case '(':
		if (out_of_string()) __parenthesis ++;
		break;
	case ')':
		if (out_of_string()) __parenthesis --;
		break;
	case '[':
		if (out_of_string()) __bracket ++;
		break;
	case ']':
		if (out_of_string()) __bracket --;
		break;
	case '{':
		if (out_of_string()) __curlybrace ++;
		break;
	case '}':
		if (out_of_string()) __curlybrace --;
		break;
	case '\'':
		if (out_of_double_string()) __singlequote = 1 - __singlequote;
		break;
	case '"':
		if (out_of_single_string()) __doublequote = 1 - __doublequote;
		break;
	}
	return 1;
}

/// @return a boolean of Is out of parenthesis
bool out_of_parenthesis()
{
	return __parenthesis == 0;
}

/// @return a boolean of Is out of brackets
bool out_of_bracket()
{
	return __bracket == 0;
}

/// @return a boolean of Is out of curly brackets
bool out_of_curlybrace()
{
	return __curlybrace == 0;
}

/// @return a boolean of Is out of single quotation
bool out_of_single_string()
{
	return __singlequote == 0;
}

/// @return a boolean of Is out of double quotation
bool out_of_double_string()
{
	return __doublequote == 0;
}

/// @return a boolean of Is out of quotation
bool out_of_string()
{
	return out_of_single_string() && out_of_double_string();
}

/// @return a boolean of Is out of the bracket system
bool independent()
{
	return out_of_parenthesis() && out_of_bracket()
		&& out_of_curlybrace()  && out_of_string();
}

};


namespace processor
{

/// remove all characters of a line since '//'
inline void cancel_from_comments(std::string & buffer)
{
	for (uint_size i = 0; i < buffer.size(); i++)
		if (buffer[i] == '/' && buffer[i+1] == '/')
			buffer = buffer.substr(0, i);
}

/// turn negative ('-a') into subtraction ('(0-a)')
inline void negative_to_subtraction(std::string & cmds)
{
	cmds = utils::trim(cmds);

	// negative turned into subtraction
	if (cmds[0] == '-')
		cmds = "(0" + cmds + ")";
}

/// remove the outer parenthesis of 'cmds' if any
inline bool remove_outer_parenthesis(std::string & cmds)
{
	cmds = utils::trim(cmds);
	uint_size len = cmds.length();

	if (cmds[0] != '(' || cmds[len - 1] != ')')
		return 0;
	tunit_ctr lctr;

	// avoid to remove parenthesis when "() { ... }.print()" happens
	for (uint_size i = 1; i < len - 1; i++)
		if (0 == lctr.update_lex_ctrs(cmds[i]))
			return 0;
	cmds = utils::trim(cmds.substr(1, len - 2));
	return 1;
}

/// remove the outer brackets of 'cmds' if any
inline bool remove_outer_brackets(std::string & cmds)
{
	cmds = utils::trim(cmds);
	uint_size len = cmds.length();

	if (cmds[0] != '[' || cmds[len - 1] != ']')
		return 0;
	tunit_ctr lctr;

	// incase [[1, 2], [1, 2, 3]]
	for (uint_size i = 1; i < len - 1; i++)
		if (0 == lctr.update_lex_ctrs(cmds[i]))
			return 0;
	cmds = utils::trim(cmds.substr(1, len - 2));
	return 1;
}

/// remove the outer curly brackets of block if needed
inline bool remove_outer_curlybrackets(std::string & cmds)
{
	cmds = utils::trim(cmds);
	uint_size len = cmds.length();

	if (cmds[0] != '{' || cmds[len - 1] != '}')
		return 0;
	tunit_ctr lctr;

	// in case { key1:{...} , key2:{...}}
	for (uint_size i = 1; i < len - 1; i++)
		if (0 == lctr.update_lex_ctrs(cmds[i]))
			return 0;
	cmds = utils::trim(cmds.substr(1, len - 2));
	return 1;
}

inline bool check_vname_validity(const std::string & str)
{
	// Keywords are not allowed as variable name
	if (str == "var" || str == "let" || str == "of")
		return false;
	if (str == "nil")
		return false;
	if (str == "true" || str == "false")
		return false;
	if (str == "this" || str == "base")
		return false;
	if (str == "to" || str == "in" || str == "and" || str == "or")
		return false;
	if (str == "if" || str == "elif" || str == "else")
		return false;
	if (str == "for" || str == "while")
		return false;
	if (str == "break" || str == "continue")
		return false;
	if (str == "return")
		return false;
	if (str == "import" || str == "as")
		return false;

	// digital is not allowed at the beginning of variable name
	if (isdigit(str[0]))
		return false;

	uint_size i = 0;

	for (; i<str.length(); i++) {
		// only alphabet, digital number or '_' is allowed in variable name
		if (!isalpha(str[i]) && !isdigit(str[i]) && str[i] != '_')
			return false;
	}
	return true;
}

inline bool check_unit_complete(const std::string & cmd)
{
	tunit_ctr ctr;
	for (uint_size i = 0; i < cmd.size(); i++)
		ctr.update_lex_ctrs(cmd[i]);
	return ctr.independent();
}

/// transform a.f(x) into f(a, x)
inline void reform_pipline(std::string & fname, std::string & params)
{
	uint_size loc = fname.rfind(".");

	if (std::string::npos != loc) {
		std::string p1 = fname.substr(0, loc);
		fname = fname.substr(loc + 1);
		params = p1.append(",").append(params);
	}
}

};


/// Split inputs into compilation units
class tunit_splitter
{
private:

/// Get `xxx\0` out of `zzzxxx\nyyy\0` where the cursor starts at the first `x`
/// @param str - input
/// @param line - output
/// @param cursor  - input and output
bool extract_next_line(const std::string & str, std::string & line, uint_size & cursor)
{
	uint_size len = str.length();

	if (cursor >= len)
		return false;
	for (uint_size i = 0; i < len - cursor + 1; i++) {
		char c = str[cursor + i];

		if (c == '\n' || c == '\0') {
			line = str.substr(cursor, i);
			cursor += (i + 1);
			return true;
		}
	}
	return false;
}

/// check whether a unit is complete (independent in bracket / quotation system)
/// @param line - input, to be checked
/// @param unit - output, appended line by line until ctr is complete
/// @param ctr - input / output
/// Note: The `fullcmd` may not be a unique unit since we don't check comma `;`.
bool append_line_to_unit(const std::string & line, std::string & unit, tunit_ctr & ctr)
{
	for (uint_size i = 0; i < line.size(); i++)
		ctr.update_lex_ctrs(line[i]);
	unit.append(line + '\n');
	return ctr.independent();
}

/// Split string by a spliter
void split(const std::string & str, char spliter, std::vector<std::string> & units)
{
	std::string cmds = utils::trim(str);
	uint_size len = cmds.length();
	uint_size records = 0;
	tunit_ctr lctr;

	for (uint_size i = 0; i<len; i++) {
		lctr.update_lex_ctrs(str[i]);

		if (!lctr.independent())
			continue;
		if (cmds[i] == spliter) {
			std::string snap = utils::trim(cmds.substr(records, i-records));

			if (snap.length() > 0) {
				units.push_back(snap); // can't add_str_consts();
				records = i + 1;
			}
		}
	}
	std::string snap = utils::trim(cmds.substr(records));
	if (snap.length() > 0) units.push_back(snap);
}

public:

/// Get `xxx` out of `(xxx)yyy`
bool get_first_parenthesis(const std::string & str, std::string & snap, uint_size & loc)
{
	if (str[0] != '(') return false;
	tunit_ctr ctr;

	for (uint_size i = 0; i<str.length(); i++) {
		ctr.update_lex_ctrs(str[i]);

		if (ctr.independent()) {
			snap = str.substr(1, i-1);
			loc = i;
			return true;
		}
	}
	return false;
}

/// Get `xxx` out of `yyy(xxx)`
bool get_last_parenthesis(const std::string & str, std::string & snap, uint_size & loc)
{
	if (str.back() != ')') return false;
	tunit_ctr ctr;
	uint_size len = str.length();

	for (uint_size i = len - 1; i>=0; i--) {
		ctr.update_lex_ctrs(str[i]);

		if (ctr.independent()) {
			snap = str.substr(i + 1, len - i - 2);
			loc = i;
			return true;
		}
	}
	return false;
}

/// Get `xxx` out of `[xxx]yyy`
bool get_first_bracket(const std::string & str, std::string & snap, uint_size & loc)
{
	if (str[0] != '[') return false;
	tunit_ctr ctr;

	for (uint_size i = 0; i<str.length(); i++) {
		ctr.update_lex_ctrs(str[i]);

		if (ctr.independent()) {
			snap = str.substr(1, i-1);
			loc = i;
			return true;
		}
	}
	return false;
}

/// Get `xxx` out of `yyy[xxx]`
bool get_last_bracket(const std::string & str, std::string & snap, uint_size & loc)
{
	if (str.back() != ']') return false;
	tunit_ctr ctr;
	uint_size len = str.length();

	for (uint_size i = len - 1; i>=0; i--) {
		ctr.update_lex_ctrs(str[i]);

		if (ctr.independent()) {
			snap = str.substr(i + 1, len - i - 2);
			loc = i;
			return true;
		}
	}
	return false;
}

/// Get `xxx` out of `{xxx}yyy`
bool get_first_block(const std::string & str, std::string & snap, uint_size & loc)
{
	if (str[0] != '{') return false;
	tunit_ctr ctr;

	for (uint_size i = 0; i<str.length(); i++) {
		ctr.update_lex_ctrs(str[i]);

		if (ctr.independent()) {
			snap = str.substr(1, i-1);
			loc = i;
			return true;
		}
	}
	return false;
}

/// Get `xxx` out of `yyy{xxx}`
bool get_last_block(const std::string & str, std::string & snap, uint_size & loc)
{
	if (str.back() != '}') return false;
	tunit_ctr ctr;
	uint_size len = str.length();

	for (uint_size i = len - 1; i>=0; i--) {
		ctr.update_lex_ctrs(str[i]);

		if (ctr.independent()) {
			snap = str.substr(i + 1, len - i - 2);
			loc = i;
			return true;
		}
	}
	return false;
}

/// Get `'xxx'` out of `'xxx'yyy`
bool get_first_single_quote(const std::string & str, std::string & snap, uint_size & loc)
{
	if (str[0] != '\'') return false;
	tunit_ctr ctr;

	for (uint_size i = 0; i<str.length(); i++) {
		ctr.update_lex_ctrs(str[i]);

		if (ctr.independent()) {
			snap = str.substr(1, i-1);
			loc = i;
			return true;
		}
	}
	return false;
}

/// Get `"xxx"` out of `"xxx"yyy`
bool get_first_double_quote(const std::string & str, std::string & snap, uint_size & loc)
{
	if (str[0] != '"') return false;
	tunit_ctr ctr;

	for (uint_size i = 0; i<str.length(); i++) {
		ctr.update_lex_ctrs(str[i]);

		if (ctr.independent()) {
			snap = str.substr(1, i-1);
			loc = i;
			return true;
		}
	}
	return false;
}

/// Do the following things:
///   1. "-a -> 0-a",
///   2. remove outer-parenthesis
///   3. trim
void preprocessing(std::string & fullcmd)
{
	processor::negative_to_subtraction(fullcmd);
	processor::remove_outer_parenthesis(fullcmd);
	fullcmd = utils::trim(fullcmd);
}

/// Split by Semicolon `;`
void split_units_by_semicolon(std::string & str, std::vector<std::string> & units)
{
	split(str, ';', units);

	for (auto iter = units.begin(); iter != units.end(); iter++) {
		std::string unit = utils::trim(*iter);
		preprocessing(unit);

		if (unit.length() == 0)
			continue;
		*iter = unit;
	}
}

/// splite a parameter list 'str' by comma `,`
void split_params_by_comma(const std::string & str, std::vector<std::string> & params)
{
	split(str, ',', params);

	// In this case nparams cannot be represented by an unsigned char
	if (params.size() >= REGLIST_SIZE_LIMIT)
		twarn(ErrCompile_REGOutOfLimit).warn("tsyner::split_params_by_comma", "too many parameters");

	// Preprocessing each parameter
	for (auto iter = params.begin(); iter != params.end(); iter++) {
		std::string cmd = utils::trim(*iter);
		preprocessing(cmd);

		if (cmd.length() == 0)
			twarn(ErrCompile_InvalidLiter).warn("split_params_by_comma", "empty liter");
		*iter = cmd;
	}
}

/// split str to get units (independent in bracket/quotation system)
void lex_str(const std::string & str, std::vector<std::string> & units)
{
	std::string tmpline;
	std::string tmpunit;
	uint_size cursor = 0;
	tunit_ctr lctr;

	while (extract_next_line(str, tmpline, cursor)) {
		processor::cancel_from_comments(tmpline);
		std::string trimmed_back_line = utils::trim_back(tmpline);
		tmpline = "";

		if (append_line_to_unit(trimmed_back_line, tmpunit, lctr)) {
			tmpunit = utils::trim(tmpunit);
			split_units_by_semicolon(tmpunit, units);
			tmpunit = "";
		}
	}
}

/// split contents of tap source code file to get units
void lex_file(FILE * f, std::vector<std::string> & units)
{
	std::string fullcmd;
	std::string line;
	tunit_ctr lctr;
	char c;

	while (true) {
		c = fgetc(f);
		line += c;

		if (c == '\n' || c == EOF || c == ';') {
			processor::cancel_from_comments(line);
			std::string line_backtrimmed = utils::trim_back(line);
			line = "";

			if (append_line_to_unit(line_backtrimmed, fullcmd, lctr)) {
				fullcmd = utils::trim(fullcmd);
				split_units_by_semicolon(fullcmd, units);
				fullcmd = "";
			}
		}
		if (c == EOF)
			break;
	}
}

/// split contents of md file to get units
void lex_md_file(FILE * f, std::vector<std::string> & units)
{
	std::string fullcmd;
	std::string tmpline;
	tunit_ctr lctr;
	char c;

	bool in_tap_code_blk = false;
	std::string tap_code_part_mark_1 = "```";
	std::string tap_code_part_mark_2 = "```tapas";
	std::string tap_code_part_mark_3 = "```Tapas";
	std::string tap_code_blk_enter_4 = "```{tapas}";
	std::string tap_code_blk_enter_5 = "```{Tapas}";

	while (true) {
		c = fgetc(f);
		tmpline += c;

		if (c == '\n' || c == EOF) {
			processor::cancel_from_comments(tmpline);
			std::string trimmed_line = utils::trim(tmpline);
			std::string backtrimmed_line = utils::trim_back(tmpline);
			tmpline = "";

			// entering code block
			if (0 == in_tap_code_blk &&
			 ((0 == tap_code_part_mark_1.compare(trimmed_line))
			||(0 == tap_code_part_mark_2.compare(trimmed_line))
			||(0 == tap_code_part_mark_3.compare(trimmed_line))
			||(0 == tap_code_blk_enter_4.compare(trimmed_line))
			||(0 == tap_code_blk_enter_5.compare(trimmed_line)))) {
				in_tap_code_blk = true;

				if (c == '\n')
					continue;
				else
					twarn(ErrCompile_InvalidLiter).warn("tunit_splitter::lex_md_file", "");
			}
			// leaving code block
			if (1 == in_tap_code_blk && 0 == tap_code_part_mark_1.compare(trimmed_line)) {
				in_tap_code_blk = false;

				if (c == '\n')
					continue;
				else
					break;
			}
			if (1 == in_tap_code_blk && append_line_to_unit(backtrimmed_line, fullcmd, lctr)) {
				fullcmd = utils::trim(fullcmd);
				split_units_by_semicolon(fullcmd, units);
				fullcmd = "";
			}
			if (c == EOF)
				break;
		}
	}
}

};


/*===========================================================================*
 * 2. Lexical Analysis (2): transform a unit into tokens
 *===========================================================================*/

/// Type of tokens
enum token_type : uint8_t
{
// statement
	token_continue,  ///< continue
	token_break,     ///< break
	token_return,    ///< return [value]
	token_var,       ///< var name: type [= value]
	token_let,       ///< let name: type [= value]
	token_import,    ///< import lib.tap [as lib]
	token_while,     ///< whole(cond) { blk }
	token_for,       ///< for (i in iter) { blk }
	token_if,        ///< if (cond) { blk }
	token_elif,      ///< elif (cond) { blk }
	token_else,      ///< else{ blk }
	token_asg,       ///< name = value
	token_idxl,      ///< arr[idx] = value

// expression: binary operators
	token_in,        ///< ele in set
	token_pair,      ///< v1 : v2
	token_to,        ///< v1 to v2
	token_or,        ///< logi1 or logi2
	token_and,       ///< logi1 and logi2
	token_eq,        ///< logi1 == logi2
	token_ne,        ///< logi1 != logi2
	token_ge,        ///< logi1 >= logi2
	token_le,        ///< logi1 <= logi2
	token_sg,        ///< logi1 > logi2
	token_sl,        ///< logi1 < logi2
	token_add,       ///< logi1 + logi2
	token_sub,       ///< logi1 - logi2
	token_mul,       ///< logi1 * logi2
	token_div,       ///< logi1 / logi2
	token_mod,       ///< logi1 % logi2
	token_mmul,      ///< logi1 @ logi2
	token_pow,       ///< logi1 ^ logi2

// expression: calling and indexing
	token_eval,      ///< func(params)
	token_idx,       ///< arr[idx]
	token_idx2,      ///< dict::key

// expression: values
	token_true,      ///< true
	token_false,     ///< false
	token_this,      ///< this
	token_base,      ///< base
	token_sstr,      ///< '....'
	token_dstr,      ///< "...."
	token_dict,      ///< {key:value, ...}
	token_func,      ///< (params) { blk }
	token_kappa,    ///< #{ blk }

// expression: values
	token_v,         ///< value
};

/// A compilation unit will be transformed into a set of tokens
struct ttoken
{
	token_type  type;     ///< the type of token `token_type`
	uint8_t     nval;     ///< the number of parameters
	std::string value_1;  ///< parameter 1
	std::string value_2;  ///< parameter 2
	std::string value_3;  ///< parameter 3
};

/// Split a syntax unit into tokens
inline void get_tokens(const std::string & str, std::vector<ttoken> & tokens)
{
	std::string unit = str;
	tunit_splitter splitter;
	uint_size genloc;
	std::string gencontents;
	// printf("unit = %s\n", unit.c_str());

//=============================================================================
// Simple values
//===========================================================================//
	if (unit == "true") {
		tokens.push_back({ token_true, 0, "", "", "" });
		return;
	}
	if (unit == "false") {
		tokens.push_back({ token_false, 0, "", "", "" });
		return;
	}
	if (unit == "this") {
		tokens.push_back({ token_this, 0, "", "", "" });
		return;
	}
	if (unit == "base") {
		tokens.push_back({ token_base, 0, "", "", "" });
		return;
	}

//=============================================================================
// Simple statements
//===========================================================================//
	if (unit == "continue") {
		tokens.push_back({ token_continue, 0, "", "", "" });
		return;
	}
	if (unit == "break") {
		tokens.push_back({ token_break, 0, "", "", "" });
		return;
	}
	// Statement return
	if (unit.length() >= 6 && unit.substr(0, 6) == "return") {
		std::string v = utils::trim(unit.substr(6));
		if (v.length() > 0)
			tokens.push_back({ token_return, 1, v, "", "" });
		else
			tokens.push_back({ token_return, 0, "",  "", "" });
		return;
	}
	// Statement var declare
	if (unit.length() >= 4 && unit.substr(0, 4) == "var ") {
		std::string vclm_expr = utils::trim(unit.substr(4));
		uint_size idx_asg = vclm_expr.find("=");
		std::string vsig = utils::trim(vclm_expr.substr(0, idx_asg));
		uint_size idx_of_type = vsig.find(":");
		std::string vname = utils::trim(vsig.substr(0, idx_of_type));
		std::string vtype = "";

		if (vname.length() == 0)
			twarn(ErrCompile_VarNoType).warn("get_token", vname);
		if (!processor::check_vname_validity(vname))
			twarn(ErrCompile_InvalidVname).warn("get_token", vname);
		if (std::string::npos != idx_of_type)
			vtype = utils::trim(vsig.substr(idx_of_type + 1));

		// Deal with assignment
		if (std::string::npos == idx_asg)  // no assignment (=)
			tokens.push_back({token_var, 2, vname, vtype, ""});
		else {                             // there is assignment (=)
			std::string right = utils::trim(vclm_expr.substr(idx_asg + 1));

			if (right.length() > 0)
				tokens.push_back({token_var, 3, vname, vtype, right});
			else
				twarn(ErrCompile_InvalidLiter).warn("get_token", vname);
		}
		return;
	}
	// Statement let declare
	if (unit.length() >= 4 && unit.substr(0, 4) == "let ") {
		std::string vclm_expr = utils::trim(unit.substr(4));
		uint_size idx_asg = vclm_expr.find("=");
		std::string vsig = utils::trim(vclm_expr.substr(0, idx_asg));
		uint_size idx_of_type = vsig.find(":");
		std::string vname = utils::trim(vsig.substr(0, idx_of_type));
		std::string vtype = "";

		if (vname.length() == 0)
			twarn(ErrCompile_VarNoType).warn("get_token", vname);
		if (!processor::check_vname_validity(vname))
			twarn(ErrCompile_InvalidVname).warn("get_token", vname);
		if (std::string::npos != idx_of_type)
			vtype = utils::trim(vsig.substr(idx_of_type + 1));

		// Deal with assignment
		if (std::string::npos == idx_asg)  // no assignment (=)
			tokens.push_back({token_let, 2, vname, vtype, ""});
		else {                             // there is assignment (=)
			std::string right = utils::trim(vclm_expr.substr(idx_asg + 1));

			if (right.length() > 0)
				tokens.push_back({token_let, 3, vname, vtype, right});
			else
				twarn(ErrCompile_InvalidLiter).warn("get_token", vname);
		}
		return;
	}
	// Statement import
	if (unit.length() >= 7 && unit.substr(0, 7) == "import ") {
		std::string import_body = utils::trim(unit.substr(7));
		uint_size spliter = import_body.find(" as ");
		std::string file = utils::trim(import_body.substr(0, spliter));

		if ((file[0] == '"'  && file.back() == '"')
		 || (file[0] == '\'' && file.back() == '\''))
			file = utils::trim(file.substr(1, file.length() - 2));
		if (std::string::npos != spliter) {  // there is alias name
			std::string name = utils::trim(import_body.substr(spliter + 4));
			tokens.push_back({token_import, 2, file, name, ""});
		} else                               // there is no alias name
			tokens.push_back({token_import, 1, file, "", ""});
		return;
	}
	// Statement while loop
	if (unit.length() >= 5 && unit.substr(0, 5) == "while") {
		std::string contents = utils::trim(unit.substr(5));
		std::string cond;
		std::string blk;
		uint_size idx_cond;
		uint_size idx_blk;

		if (splitter.get_first_parenthesis(contents, cond, idx_cond)) {
			std::string blk_full = utils::trim(contents.substr(idx_cond + 1));

			if (splitter.get_first_block(blk_full, blk, idx_blk)) {
				if (idx_blk == blk_full.length()-1) {
					cond = utils::trim(cond);
					blk = utils::trim(blk);
					tokens.push_back({ token_while, 2, cond, blk, "" });
					return;
				}
			}
		}
	}
	// Statement for loop
	if (unit.length() >= 3 && unit.substr(0, 3) == "for") {
		std::string contents = utils::trim(unit.substr(3));
		std::string cond;
		std::string blk;
		uint_size idx_cond;
		uint_size idx_blk;

		if (splitter.get_first_parenthesis(contents, cond, idx_cond)) {
			std::string blk_full = utils::trim(contents.substr(idx_cond + 1));

			if (splitter.get_first_block(blk_full, blk, idx_blk)) {
				if (idx_blk == blk_full.length()-1) {
					cond = utils::trim(cond);
					blk = utils::trim(blk);
					uint_size idx_in = cond.find(" in ");

					std::string in_l = utils::trim(cond.substr(0, idx_in));
					std::string in_r = utils::trim(cond.substr(idx_in + 4));

					tokens.push_back({ token_for, 3, in_l, in_r, blk });
					return;
				}
			}
		}
	}
	// Statement if
	if (unit.length() >= 2 && unit.substr(0, 2) == "if") {
		std::string contents = utils::trim(unit.substr(2));
		std::string cond;
		std::string blk;
		uint_size idx_cond;
		uint_size idx_blk;

		if (splitter.get_first_parenthesis(contents, cond, idx_cond)) {
			std::string blk_full = utils::trim(contents.substr(idx_cond + 1));

			if (splitter.get_first_block(blk_full, blk, idx_blk)) {
				if (idx_blk == blk_full.length()-1) {
					cond = utils::trim(cond);
					blk = utils::trim(blk);
					tokens.push_back({ token_if, 2, cond, blk, "" });
					return;
				}
			}
		}
	}
	// Statement elif
	if (unit.length() >= 4 && unit.substr(0, 4) == "elif") {
		std::string contents = utils::trim(unit.substr(4));
		std::string cond;
		std::string blk;
		uint_size idx_cond;
		uint_size idx_blk;

		if (splitter.get_first_parenthesis(contents, cond, idx_cond)) {
			std::string blk_full = utils::trim(contents.substr(idx_cond + 1));

			if (splitter.get_first_block(blk_full, blk, idx_blk)) {
				if (idx_blk == blk_full.length()-1) {
					cond = utils::trim(cond);
					blk = utils::trim(blk);
					tokens.push_back({ token_elif, 2, cond, blk, "" });
					return;
				}
			}
		}
	}
	// Statement else
	if (unit.length() >= 4 && unit.substr(0, 4) == "else") {
		std::string contents = utils::trim(unit.substr(4));
		std::string blk;
		uint_size idx_blk;

		if (splitter.get_first_block(contents, blk, idx_blk)) {
			if (idx_blk == contents.length()-1) {
				blk = utils::trim(blk);
				tokens.push_back({ token_else, 1, blk, "", "" });
				return;
			}
		}
	}
	// Statement: a = b, arr[idx] = b
	if (std::string::npos != (genloc = unit.find("="))) {
		if (genloc > 0 && genloc < unit.length() - 1
		&& unit[genloc + 1] != '='
		&& unit[genloc - 1] != '='
		&& unit[genloc - 1] != '>'
		&& unit[genloc - 1] != '<'
		&& unit[genloc - 1] != '!') {
			std::string right = utils::trim(unit.substr(genloc + 1));
			std::string left = utils::trim(unit.substr(0, genloc));

			if (processor::check_unit_complete(left)
			 && processor::check_unit_complete(right)) {
				uint_size idxl_loc;
				std::string idxl_params;

				if (splitter.get_last_bracket(left, idxl_params, idxl_loc)) {
					std::string arr = utils::trim(left.substr(0, idxl_loc));
					std::string params = utils::trim(idxl_params);
					tokens.push_back({ token_idxl, 3, arr, params, right });
				} else
					tokens.push_back({ token_asg, 2, left, right, "" });
				return;
			}
		}
	}

//=============================================================================
// Complex values
//===========================================================================//
	// Expression: single string   ''
	if (splitter.get_first_single_quote(unit, gencontents, genloc)) {
		if (genloc == unit.length() - 1) {
			tokens.push_back({ token_sstr, 1, gencontents, "", "" });
			return;
		}
	}
	// Expression: double string   ""
	if (splitter.get_first_double_quote(unit, gencontents, genloc)) {
		if (genloc == unit.length() - 1) {
			tokens.push_back({ token_sstr, 1, gencontents, "", "" });
			return;
		}
	}
	// Expression: dict  {}
	if (splitter.get_first_block(unit, gencontents, genloc)) {
		if (genloc == unit.length() - 1) {
			std::string params = utils::trim(gencontents);
			tokens.push_back({ token_dict, 1, params, "", "" });
			return;
		}
	}
	// Expression: Kappa #{}
	if (unit.length() >= 1 && unit.substr(0, 1) == "#") {
		std::string contents = utils::trim(unit.substr(1));
		std::string blk;
		uint_size idx_blk;

		if (splitter.get_first_block(contents, blk, idx_blk)) {
			if (idx_blk == contents.length()-1) {
				blk = utils::trim(blk);
				tokens.push_back({ token_kappa, 1, blk, "", "" });
				return;
			}
		}
	}
	// Expression: function () {}
	if (splitter.get_first_parenthesis(unit, gencontents, genloc)) {
		std::string blk_full = utils::trim(unit.substr(genloc + 1));
		std::string blk;
		uint_size idx_blk;

		if (splitter.get_first_block(blk_full, blk, idx_blk)) {
			if (idx_blk == blk_full.length() - 1) {
				std::string params = utils::trim(gencontents);
				blk = utils::trim(blk);
				tokens.push_back({ token_func, 2, params, blk, "" });
				return;
			}
		}
	}

//=============================================================================
// Binary Expressions
//===========================================================================//
	ttoken binary_token = {token_v, 0, "", "", ""};
	double test_sci_not = 0;

	for (auto rit = unit.rbegin(); rit != unit.rend(); ++rit) {
		auto it = rit.base();

		// Expression a in b
		if (std::distance(it, unit.end()) > 4 && std::distance(unit.begin(), it) > 0
		 && std::string(it, it + 4) == " in ") {
			// printf("in\n");
			std::string left = utils::trim(std::string(unit.begin(), it));
			std::string right = utils::trim(std::string(it + 4, unit.end()));

			if (processor::check_unit_complete(left)
			 && processor::check_unit_complete(right)
			 && token_in < binary_token.type)
				binary_token = { token_in, 2, left, right, "" };
		}
		// Expression a:b
		if (std::distance(it, unit.end()) > 1 && std::distance(unit.begin(), it) > 0
		 && std::string(it, it + 1) == ":"
		 && std::string(it - 1, it) != ":"
		 && std::string(it + 1, it + 2) != ":") {
			// printf("pair\n");
			std::string left = utils::trim(std::string(unit.begin(), it));
			std::string right = utils::trim(std::string(it + 1, unit.end()));

			if (processor::check_unit_complete(left)
			 && processor::check_unit_complete(right)
			 && token_pair < binary_token.type)
				binary_token = { token_pair, 2, left, right, "" };
		}
		// Expression a to b
		if (std::distance(it, unit.end()) > 4 && std::distance(unit.begin(), it) > 0
		 && std::string(it, it + 4) == " to ") {
			// printf("to\n");
			std::string left = utils::trim(std::string(unit.begin(), it));
			std::string right = utils::trim(std::string(it + 4, unit.end()));

			if (processor::check_unit_complete(left)
			 && processor::check_unit_complete(right)
			 && token_to < binary_token.type)
				binary_token = { token_to, 2, left, right, "" };
		}
		// Expression a or b
		if (std::distance(it, unit.end()) > 4 && std::distance(unit.begin(), it) > 0
		 && std::string(it, it + 4) == " or ") {
			// printf("or\n");
			std::string left = utils::trim(std::string(unit.begin(), it));
			std::string right = utils::trim(std::string(it + 4, unit.end()));

			if (processor::check_unit_complete(left)
			 && processor::check_unit_complete(right)
			 && token_or < binary_token.type)
				binary_token = { token_or, 2, left, right, "" };
		}
		// Expression a and b
		if (std::distance(it, unit.end()) > 5 && std::distance(unit.begin(), it) > 0
		 && std::string(it, it + 5) == " and ") {
			// printf("and\n");
			std::string left = utils::trim(std::string(unit.begin(), it));
			std::string right = utils::trim(std::string(it + 5, unit.end()));

			if (processor::check_unit_complete(left)
			 && processor::check_unit_complete(right)
			 && token_and < binary_token.type)
				binary_token = { token_and, 2, left, right, "" };
		}
		// Expression a == b
		if (std::distance(it, unit.end()) > 2 && std::distance(unit.begin(), it) > 0
		 && std::string(it, it + 2) == "==") {
			// printf("==\n");
			std::string left = utils::trim(std::string(unit.begin(), it));
			std::string right = utils::trim(std::string(it + 2, unit.end()));

			if (processor::check_unit_complete(left)
			 && processor::check_unit_complete(right)
			 && token_sl < binary_token.type)
				binary_token = { token_eq, 2, left, right, "" };
		}
		// Expression a != b
		if (std::distance(it, unit.end()) > 2 && std::distance(unit.begin(), it) > 0
		 && std::string(it, it + 2) == "!=") {
			// printf("!=\n");
			std::string left = utils::trim(std::string(unit.begin(), it));
			std::string right = utils::trim(std::string(it + 2, unit.end()));

			if (processor::check_unit_complete(left)
			 && processor::check_unit_complete(right)
			 && token_sl < binary_token.type)
				binary_token = { token_ne, 2, left, right, "" };
		}
		// Expression a >= b
		if (std::distance(it, unit.end()) > 2 && std::distance(unit.begin(), it) > 0
		 && std::string(it, it + 2) == ">=") {
			// printf(">=\n");
			std::string left = utils::trim(std::string(unit.begin(), it));
			std::string right = utils::trim(std::string(it + 2, unit.end()));

			if (processor::check_unit_complete(left)
			 && processor::check_unit_complete(right)
			 && token_sl < binary_token.type)
				binary_token = { token_ge, 2, left, right, "" };
		}
		// Expression a <= b
		if (std::distance(it, unit.end()) > 2 && std::distance(unit.begin(), it) > 0
		 && std::string(it, it + 2) == "<=") {
			// printf("<=\n");
			std::string left = utils::trim(std::string(unit.begin(), it));
			std::string right = utils::trim(std::string(it + 2, unit.end()));

			if (processor::check_unit_complete(left)
			 && processor::check_unit_complete(right)
			 && token_sl < binary_token.type)
				binary_token = { token_le, 2, left, right, "" };
		}
		// Expression a > b
		if (std::distance(it, unit.end()) > 1 && std::distance(unit.begin(), it) > 0
		 && std::string(it, it + 1) == ">") {
			// printf(">\n");
			std::string left = utils::trim(std::string(unit.begin(), it));
			std::string right = utils::trim(std::string(it + 1, unit.end()));

			if (processor::check_unit_complete(left)
			 && processor::check_unit_complete(right)
			 && token_sl < binary_token.type)
				binary_token = { token_sg, 2, left, right, "" };
		}
		// Expression a < b
		if (std::distance(it, unit.end()) > 1 && std::distance(unit.begin(), it) > 0
		 && std::string(it, it + 1) == "<") {
			// printf("<\n");
			std::string left = utils::trim(std::string(unit.begin(), it));
			std::string right = utils::trim(std::string(it + 1, unit.end()));

			if (processor::check_unit_complete(left)
			 && processor::check_unit_complete(right)
			 && token_sl < binary_token.type)
				binary_token = { token_sl, 2, left, right, "" };
		}
		// Expression first level algorithmic: a + b
		if (std::distance(it, unit.end()) > 1 && std::distance(unit.begin(), it) > 0
		 && std::string(it, it + 1) == "+") {
			// printf("add\n");
			std::string left = utils::trim(std::string(unit.begin(), it));
			std::string right = utils::trim(std::string(it + 1, unit.end()));

			if (processor::check_unit_complete(left)
			 && processor::check_unit_complete(right)
			 && token_sub < binary_token.type)
				binary_token = { token_add, 2, left, right, "" };
		}
		// Expression first level algorithmic: a - b
		if (std::distance(it, unit.end()) > 1 && std::distance(unit.begin(), it) > 0
		 && std::string(it, it + 1) == "-" && false == utils::str_to_double(unit, test_sci_not)) {
			// printf("sub\n");
			std::string left = utils::trim(std::string(unit.begin(), it));
			std::string right = utils::trim(std::string(it + 1, unit.end()));

			if (processor::check_unit_complete(left)
			 && processor::check_unit_complete(right)
			 && token_sub < binary_token.type)
				binary_token = { token_sub, 2, left, right, "" };
		}
		// Expression first level algorithmic: a * b
		if (std::distance(it, unit.end()) > 1 && std::distance(unit.begin(), it) > 0
		 && std::string(it, it + 1) == "*") {
			// printf("mul\n");
			std::string left = utils::trim(std::string(unit.begin(), it));
			std::string right = utils::trim(std::string(it + 1, unit.end()));

			if (processor::check_unit_complete(left)
			 && processor::check_unit_complete(right)
			 && token_mmul < binary_token.type)
				binary_token = { token_mul, 2, left, right, "" };
		}
		// Expression first level algorithmic: a / b
		if (std::distance(it, unit.end()) > 1 && std::distance(unit.begin(), it) > 0
		 && std::string(it, it + 1) == "/") {
			// printf("div\n");
			std::string left = utils::trim(std::string(unit.begin(), it));
			std::string right = utils::trim(std::string(it + 1, unit.end()));

			if (processor::check_unit_complete(left)
			 && processor::check_unit_complete(right)
			 && token_mmul < binary_token.type)
				binary_token = { token_div, 2, left, right, "" };
		}
		// Expression first level algorithmic: a % b
		if (std::distance(it, unit.end()) > 1 && std::distance(unit.begin(), it) > 0
		 && std::string(it, it + 1) == "%") {
			// printf("mod\n");
			std::string left = utils::trim(std::string(unit.begin(), it));
			std::string right = utils::trim(std::string(it + 1, unit.end()));

			if (processor::check_unit_complete(left)
			 && processor::check_unit_complete(right)
			 && token_mmul < binary_token.type)
				binary_token = { token_mod, 2, left, right, "" };
		}
		// Expression first level algorithmic: a @ b
		if (std::distance(it, unit.end()) > 1 && std::distance(unit.begin(), it) > 0
		 && std::string(it, it + 1) == "@") {
			// printf("mmul\n");
			std::string left = utils::trim(std::string(unit.begin(), it));
			std::string right = utils::trim(std::string(it + 1, unit.end()));

			if (processor::check_unit_complete(left)
			 && processor::check_unit_complete(right)
			 && token_mmul < binary_token.type)
				binary_token = { token_mmul, 2, left, right, "" };
		}
		// Expression first level algorithmic: a ^ b
		if (std::distance(it, unit.end()) > 1 && std::distance(unit.begin(), it) > 0
		 && std::string(it, it + 1) == "^") {
			// printf("pow\n");
			std::string left = utils::trim(std::string(unit.begin(), it));
			std::string right = utils::trim(std::string(it + 1, unit.end()));

			if (processor::check_unit_complete(left)
			 && processor::check_unit_complete(right)
			 && token_pow < binary_token.type)
				binary_token = { token_pow, 2, left, right, "" };
		}
	}
	if (binary_token.type < token_v) {
		tokens.push_back(binary_token);
		return;
	}

//=============================================================================
// Values with Complex Forms
//===========================================================================//

	// Expression eval f()
	if (splitter.get_last_parenthesis(unit, gencontents, genloc)) {
		std::string fname = utils::trim(unit.substr(0, genloc));

		if (processor::check_unit_complete(fname)) {
			std::string params = utils::trim(gencontents);
			processor::reform_pipline(fname, params);
			tokens.push_back({ token_eval, 2, fname, params, "" });
			return;
		}
	}
	// Expression idx arr[]
	if (splitter.get_last_bracket(unit, gencontents, genloc)) {
		std::string arr = utils::trim(unit.substr(0, genloc));

		if (processor::check_unit_complete(arr)) {
			std::string params = utils::trim(gencontents);
			tokens.push_back({ token_idx, 2, arr, params, "" });
			return;
		}
	}
	// Expression a::b
	if (std::string::npos != (genloc = unit.rfind("::"))) {
		std::string left = utils::trim(unit.substr(0, genloc));
		std::string right = utils::trim(unit.substr(genloc + 2));

		if (processor::check_unit_complete(left) && processor::check_unit_complete(right)) {
			tokens.push_back({ token_idx2, 2, left, right, "" });
			return;
		}
	}

	// unrecognized
	tokens.push_back({ token_v, 1, unit, "", "" });
}

}

#endif // TC_H
