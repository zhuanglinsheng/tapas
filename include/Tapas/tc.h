#ifndef TC_H
#define TC_H

#include "tbasis.h"
#include "tbycs.h"


namespace tapas
{

namespace utils
{

/// trim the ending of a given string and return it.
inline std::string trim_back(const std::string & str)
{
	if (str.empty()) return str;
	char str_end = str.back();

	if (str_end == '\n' || str_end == '\r'
	|| str_end == '\t' || str_end == ' ' || str_end == EOF)
		return trim_back(str.substr(0, str.length() - 1));
	return str;
}

/// trim the beginning of a given string and return it.
inline std::string trim_front(const std::string & str)
{
	if (str.empty()) return str;
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
// Units with Fixed Forms
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
		uint_size idx_of = vsig.find(":");
		std::string vname = utils::trim(vsig.substr(0, idx_of));
		std::string vtype = "";

		if (vname.length() == 0)
			twarn(ErrCompile_VarNoType).warn("get_token", vname);
		if (!processor::check_vname_validity(vname))
			twarn(ErrCompile_InvalidVname).warn("get_token", vname);
		if (std::string::npos != idx_of)
			vtype = utils::trim(vsig.substr(idx_of + 1));

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

		if ((file[0] == '"' && file.back() == '"')
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


/*===========================================================================*
 * 3. Compilation: compile a unit (a set of tokens) into bycodes
 *===========================================================================*/

/// Registers counter
class treg_ctr
{
private:
	uint_size_reg __reg_ctr = 0;
	uint_size_reg __reg_max = 0;

void update_reg_max()
{
	__reg_max = __reg_max >= __reg_ctr ? __reg_max : __reg_ctr;
}

public:

/// Add register counter by 1
void add_reg_ctr()
{
	if (__reg_ctr + 1 > REGLIST_SIZE_LIMIT)
		twarn(ErrCompile_REGOutOfLimit).warn("treg_ctr::add_reg_ctr", "");
	__reg_ctr++;
	update_reg_max();
}

/// Add register counter by n
void add_reg_ctr_n(uint_size_reg n)
{
	if (__reg_ctr + n > REGLIST_SIZE_LIMIT)
		twarn(ErrCompile_REGOutOfLimit).warn("treg_ctr::add_reg_ctr_n", "");
	__reg_ctr += n;
	update_reg_max();
}

/// Deduct register counter by 1
void ddt_reg_ctr()
{
	if (__reg_ctr == 0)
		twarn(ErrCompile_REGOutOfLimit).warn("treg_ctr::ddt_reg_ctr", "");
	__reg_ctr--;
}

/// Deduct register counter by n
void ddt_reg_ctr_n(uint_size_reg n)
{
	if (__reg_ctr < n)
		twarn(ErrCompile_REGOutOfLimit).warn("treg_ctr::ddt_reg_ctr_n", "");
	__reg_ctr -= n;
}

/// Get register counter
uint_size_reg get_reg_ctr() const
{
	return __reg_ctr;
}

/// Get the maximum usage of registers
uint_size_reg get_reg_max() const
{
	return __reg_max;
}

};


/// Objects (variables) counter
class tobj_ctr
{
private:
	std::vector<std::string> __objs;
	uint_size_obj  __current_env_objmax;
	tobj_ctr * __father;
	uint_size_obj  __npreload;

/// Update __current_env_objmax, the maximum length of object array used
void update_obj_max()
{
	if (__current_env_objmax < __objs.size())
		__current_env_objmax = __objs.size();
}

public:

/// Constructor: at top level with no preclude objects contained
tobj_ctr()
{
	__father = nullptr; __current_env_objmax = 0; __npreload = 0;
}

/// Constructor: has father
tobj_ctr(tobj_ctr * father)
{
	__father = father; __current_env_objmax = 0; __npreload = 0;
}

/** Constructor: general
 *  @details Preclude objects `default_objs` could be
 *               - Tapas objects;
 *               - Parameters of function in a function environment.
 */
tobj_ctr(const std::vector<std::string> & precludes, tobj_ctr * father)
{
	__father = father;
	__objs.insert(__objs.cend(), precludes.cbegin(), precludes.cend());
	__current_env_objmax = __objs.size();
	__npreload = __objs.size();  // the number of default objects

	if (obj_len_in_all() >= OBJLIST_SIZE_LIMIT)
		twarn(ErrCompile_OBJOutOfLimit).warn("tobj_ctr::tobj_ctr", "");
}

/// @return a string list of first `n` objects
consts_str_vect first_n_objs(uint_size_obj n)
{
	consts_str_vect objs;
	objs.insert(objs.cend(), __objs.cbegin(), __objs.cbegin() + n);
	return objs;
}

/// @return the number of objects in current level
uint_size_obj obj_len_in_current_env()
{
	return __objs.size();
}

/// @return the number of objects in all level tree
uint_size_obj obj_len_in_all()
{
	if (!__father)
		return __objs.size();
	return __objs.size() + __father->obj_len_in_all();
}

/// @return the maximum object occurs in current level
uint_size_obj obj_max_in_current_env() const
{
	return __current_env_objmax;
}

/** Searching from local environment.
 *  @details If the current environment does not have the referred object,
 *               then searching the father environment.
 *  @param objname - a string reference
 *  @return loc in some environment + all objs in the searching path
 */
uint_size_obj obj_loc(const std::string & objname)
{
	auto iter = __objs.cbegin();
	uint_size_obj loc = 0;

	for (; iter != __objs.cend(); iter++) {
		if (*iter == objname) break;
		loc++;
	}
	if (__father && loc == __objs.size())
		return loc + __father->obj_loc(objname);

	return loc;
}

/// @return locotion of `left`
uint_size_obj obj_create(const std::string & left, bool inblk, tconsts & consts, uint_size_cst & nameloc)
{
	uint_size_obj loc = obj_loc(left);
	uint_size_obj len_in_all = obj_len_in_all();
	uint_size_obj len_current = obj_len_in_current_env();

	if (len_in_all + 1 >= OBJLIST_SIZE_LIMIT)
		twarn(ErrCompile_OBJOutOfLimit).warn("tobj_ctr::obj_crt", left);
	if (loc < len_current)
		twarn(ErrCompile_DblVDeclare).warn("tobj_ctr::obj_crt", left);
	if (inblk)
		twarn(ErrCompile_InBlkVarDef).warn("tobj_ctr::obj_crt", left);

	// record variable name
	nameloc = consts.add_str_const(left);

	// create variable
	__objs.push_back(left);
	update_obj_max();
	return len_current;
}

/// Delete last `n` objects
void obj_del_last_n(uint_size_obj n)
{
	while (n > 0) {
		if (__objs.size() > __npreload)
			__objs.pop_back();
		else
			twarn(ErrCompile_OBJOutOfLimit).warn("tobj_ctr::tmpobj_del", "");
		n--;
	}
}

/// @return a boolean of Is the object at `loc` is default object
bool is_preload(uint_size_obj loc) const
{
	if (!__father)
		return loc < __npreload;
	return __father->is_preload(loc - __objs.size());
}

};


/// Structure of binary expressions. e.g. a + b
struct tbin_expr
{
	std::string left;      ///< **std::string** of left
	std::string right;     ///< **std::string** of right
	uint8_t     al_type;   ///< 0 1 2 or 3
	uint_size_obj   lloc;      ///< left object's loc in object list
	uint_size_obj   rloc;      ///< right object's loc in object list
};


/// Compile a string/file/md file into bycodes
class tcp
{
private:
	tobj_ctr  __objctr;          ///< environmental object counter
	tobj_ctr  __tmpctr;          ///< temporary object counter
	tunit_ctr __lexctr;          ///< lexing counter
	treg_ctr  __regctr;          ///< register counter
	uint_size_obj __n_default_objs;  ///< (preload) default objects
	bool      __interactive;     ///< UI

bool find_imported_file(std::string & file, std::vector<std::string> & paths)
{
	// Absolute location: Try dir
	std::string dir_init_file = file + "/__init__.tap";
	FILE * dir_init_file_ss = nullptr;

	if (nullptr != (dir_init_file_ss = fopen(dir_init_file.c_str(), "r"))) {
		fclose(dir_init_file_ss);
		file = dir_init_file;
		return true;
	}// Otherwise, no need to close since it is nullptr

	// Absolute location: Try file
	FILE * file_ss = nullptr;

	if (nullptr != (file_ss = fopen(file.c_str(), "r"))) {
		fclose(file_ss);
		return true;
	}// Otherwise, no need to close since it is nullptr

	// Relative location: Try different paths
	for (auto it = paths.begin(); it != paths.end(); it++) {
		// First, try dir
		std::string file_i2 = *it + "/" + file + "/__init__.tap";
		FILE * f_i2 = nullptr;

		if (nullptr != (f_i2 = fopen(file_i2.c_str(), "r"))) {
			fclose(f_i2);
			file = file_i2;
			return true;
		}// Otherwise, no need to close since it is nullptr

		// Then, try file
		std::string file_i1 = *it;
		file_i1.append("/" + file);
		FILE * f_i = nullptr;

		if (nullptr != (f_i = fopen(file_i1.c_str(), "r"))) {
			fclose(f_i);
			file = file_i1;
			return true;
		}// Otherwise, no need to close since it is nullptr
	}
	return false;
}

void parse_v(const ttoken & tok, tvmcmd_vect & tcmds, tconsts & consts)
{
	if (tok.value_1.length() == 0)
		twarn(ErrCompile_InvalidLiter).warn("tcp::parse_v", "empty liter");
	long   it;
	double dt;

	if (utils::str_to_long_int(tok.value_1, it)) {
		uint_size_cst loc = consts.add_int_const(it);
		tcmds.append(tbycode(OP_PUSHI, loc));
		__regctr.add_reg_ctr();
	} else if (utils::str_to_double(tok.value_1, dt)) {
		uint_size_cst loc = consts.add_double_const(dt);
		tcmds.append(tbycode(OP_PUSHD, loc));
		__regctr.add_reg_ctr();
	} else {
		uint_size_obj loc_tmp = __tmpctr.obj_loc(tok.value_1);
		uint_size_obj loc_env = __objctr.obj_loc(tok.value_1);

		if (loc_tmp < __tmpctr.obj_len_in_all()) {
			tcmds.append(tbycode(OP_PUSHX, loc_tmp, 0));
			__regctr.add_reg_ctr();
		} else if (loc_env < __objctr.obj_len_in_all()) {
			tcmds.append(tbycode(OP_PUSHX, loc_env, 1));
			__regctr.add_reg_ctr();
		} else
			twarn(ErrCompile_InvalidLiter).warn("tcp::parse_v", tok.value_1);
	}
}

/// Split 'cmd' and generate a 'tbin_expr'
tbin_expr binop_split(const ttoken & toc)
{
	tbin_expr expr;
	expr.left = toc.value_1;
	expr.right = toc.value_2;
	uint_size_obj obj_left_loc = __objctr.obj_loc(expr.left);
	uint_size_obj tmp_left_loc = __tmpctr.obj_loc(expr.left);
	uint_size_obj obj_right_loc = __objctr.obj_loc(expr.right);
	uint_size_obj tmp_right_loc = __tmpctr.obj_loc(expr.right);
	uint_size_obj obj_size_all = __objctr.obj_len_in_all();
	uint_size_obj tmp_size_all = __tmpctr.obj_len_in_all();
	expr.al_type = 0; // value value

	// type 1: env value
	if (obj_left_loc < obj_size_all && tmp_right_loc == tmp_size_all && obj_right_loc == obj_size_all) {
		expr.lloc = obj_left_loc;
		expr.al_type = 1;
	}
	// type 2: value env
	if (tmp_left_loc == tmp_size_all && obj_left_loc == obj_size_all && obj_right_loc < obj_size_all) {
		expr.rloc = obj_right_loc;
		expr.al_type = 2;
	}
	// type 3: env env
	if (obj_left_loc < obj_size_all && obj_right_loc < obj_size_all) {
		expr.lloc = obj_left_loc;
		expr.rloc = obj_right_loc;
		expr.al_type = 3;
	}
	// type 4: tmp value
	if (tmp_left_loc < tmp_size_all && tmp_right_loc == tmp_size_all && obj_right_loc == obj_size_all) {
		expr.lloc = tmp_left_loc;
		expr.al_type = 4;
	}
	// type 5: value tmp
	if (tmp_left_loc == tmp_size_all && obj_left_loc == obj_size_all && tmp_right_loc < tmp_size_all) {
		expr.rloc = tmp_right_loc;
		expr.al_type = 5;
	}
	// type 6: tmp tmp
	if (tmp_left_loc < tmp_size_all && tmp_right_loc < tmp_size_all) {
		expr.lloc = tmp_left_loc;
		expr.rloc = tmp_right_loc;
		expr.al_type = 6;
	}
	// type 7: env tmp
	if (obj_left_loc < obj_size_all && tmp_right_loc < tmp_size_all) {
		expr.lloc = obj_left_loc;
		expr.rloc = tmp_right_loc;
		expr.al_type = 7;
	}
	// type 8: tmp env
	if (tmp_left_loc < tmp_size_all && obj_right_loc < obj_size_all) {
		expr.lloc = tmp_left_loc;
		expr.rloc = obj_right_loc;
		expr.al_type = 8;
	}
	return expr;
}

/// Parse binary expression generated by "binop_split".
void parse_binop(tins ins, const tbin_expr & expr, tvmcmd_vect & tcmds, tconsts & consts,
		std::vector<std::string> & paths, bool inblk)
{
	switch (expr.al_type) {
	case 0: // value value
		parse_unit(expr.right, tcmds, consts, paths, 0, inblk);
		parse_unit(expr.left, tcmds, consts, paths, 0, inblk);
		tcmds.append(tbycode(OP_PUSHINFO, expr.al_type));
		__regctr.add_reg_ctr(); // push info
		tcmds.append(tbycode(ins, uint16_t(0), uint16_t(1)));
		__regctr.ddt_reg_ctr(); // pop info
		__regctr.ddt_reg_ctr(); // pop top
		break;
	case 1: // env value
		parse_unit(expr.right, tcmds, consts, paths, 0, inblk);
		tcmds.append(tbycode(OP_PUSHINFO, expr.al_type));
		__regctr.add_reg_ctr(); // push info
		tcmds.append(tbycode(ins, expr.lloc, 0));
		__regctr.ddt_reg_ctr(); // pop info
		break;
	case 2: // value env
		parse_unit(expr.left, tcmds, consts, paths, 0, inblk);
		tcmds.append(tbycode(OP_PUSHINFO, expr.al_type));
		__regctr.add_reg_ctr(); // push info
		tcmds.append(tbycode(ins, 0, expr.rloc));
		__regctr.ddt_reg_ctr(); // pop info
		break;
	case 3: // env env
		tcmds.append(tbycode(OP_PUSHINFO, expr.al_type));
		__regctr.add_reg_ctr(); // push info
		tcmds.append(tbycode(ins, expr.lloc, expr.rloc));
		__regctr.ddt_reg_ctr(); // pop info
		__regctr.add_reg_ctr(); // push return
		break;
	case 4: // tmp value
		parse_unit(expr.right, tcmds, consts, paths, 0, inblk);
		tcmds.append(tbycode(OP_PUSHINFO, expr.al_type));
		__regctr.add_reg_ctr(); // push info
		tcmds.append(tbycode(ins, expr.lloc, 0));
		__regctr.ddt_reg_ctr(); // pop info
		break;
	case 5: // value tmp
		parse_unit(expr.left, tcmds, consts, paths, 0, inblk);
		tcmds.append(tbycode(OP_PUSHINFO, expr.al_type));
		__regctr.add_reg_ctr(); // push info
		tcmds.append(tbycode(ins, 0, expr.rloc));
		__regctr.ddt_reg_ctr(); // pop info
		break;
	case 6: // tmp tmp
		tcmds.append(tbycode(OP_PUSHINFO, expr.al_type));
		__regctr.add_reg_ctr(); // push info
		tcmds.append(tbycode(ins, expr.lloc, expr.rloc));
		__regctr.ddt_reg_ctr(); // pop info
		__regctr.add_reg_ctr(); // push return
		break;
	case 7: // env tmp
		tcmds.append(tbycode(OP_PUSHINFO, expr.al_type));
		__regctr.add_reg_ctr(); // push info
		tcmds.append(tbycode(ins, expr.lloc, expr.rloc));
		__regctr.ddt_reg_ctr(); // pop info
		__regctr.add_reg_ctr(); // push return
		break;
	case 8: // tmp env
		tcmds.append(tbycode(OP_PUSHINFO, expr.al_type));
		__regctr.add_reg_ctr(); // push info
		tcmds.append(tbycode(ins, expr.lloc, expr.rloc));
		__regctr.ddt_reg_ctr(); // pop info
		__regctr.add_reg_ctr(); // push return
		break;
	}
}

/// Parse binary expressions by passing in token
void parse_binop(tins ins, const ttoken & tok, tvmcmd_vect & tcmds, tconsts & consts,
		std::vector<std::string> & paths, bool inblk)
{
	parse_unit(tok.value_2, tcmds, consts, paths, 0, inblk);
	parse_unit(tok.value_1, tcmds, consts, paths, 0, inblk);
	tcmds.append(tbycode(ins));
	__regctr.ddt_reg_ctr_n(2);
	__regctr.add_reg_ctr();
}

/** Parse return statement
 *
 *  @code
 *      return [value]
 *  @endcode
 *
 *  Case 1: If there is no `value` attched, then return (and then pop)
 *      the top value in virtual machine (VM) stack. If VM stack is
 *      empty then return a nil value.
 *  Case 2: If there is `value` attched, then
 *      Case 2.1: If `value` is not a temporary  variable, first
 *                execute `value`, then do Case 1.
 *      Case 2.2: If `value` is a variable, then throw a Compilation
 *                error `ErrCompile_ReturnTmpObj`.
 */
void parse_return(const ttoken & tok, tvmcmd_vect & tcmds, tconsts & consts,
		std::vector<std::string> & paths, bool inblk)
{
	if (tok.nval == 1) {
		if (__tmpctr.obj_loc(tok.value_1) != __tmpctr.obj_len_in_all())
			twarn(ErrCompile_ReturnTmpObj).warn("tvm::parse_return", "");
		parse_unit(tok.value_1, tcmds, consts, paths, 0, inblk);
	}
	tcmds.append(tbycode(OP_RET));
	__regctr.ddt_reg_ctr_n(__regctr.get_reg_ctr());
}

/** Parse var statement
 *
 *  @code
 *      var name [: type] [= value]
 *  @endcode
 *
 *  Case 1: If `name` is not declared as any environmental or temporary
 *      variable in current environment, then var will create a new
 *      environmntal variable.
 *  Case 2: If `name` is already declared as environmental or temporary
 *      variable in current environment, then `Duplicated Declaration`
 *      compilation error will be throw out.
 *  Case 3: If declaration of environmental variable happens in block,
 *      then `In Block Declaration` compilation error will be throw out.
 *  Case 4: If declaration is successful and a `value` is attached, then
 *      execute `value` and assign the returned to the variable `name`.
 */
void parse_var(const ttoken & tok, tvmcmd_vect & tcmds, tconsts & consts,
		std::vector<std::string> & paths, bool inblk)
{
	// If the name is declared in current environment temporarily
	uint_size_obj loc_tmp = __tmpctr.obj_loc(tok.value_1);

	if (loc_tmp < __tmpctr.obj_len_in_current_env())
		twarn(ErrCompile_DblVDeclare).warn("tcp::parse_var", tok.value_1);

	uint_size_cst nameloc = UNDEF_NAMELOC;
	uint_size_obj loc = __objctr.obj_create(tok.value_1, inblk, consts, nameloc); // duplicated checking here
	tcmds.append(tbycode(OP_VCRT, nameloc, 1));

	if (tok.nval == 3) {
		try {
			parse_unit(tok.value_3, tcmds, consts, paths, 0, inblk);
			tcmds.append(tbycode(OP_POPCOV, loc, 1));
			__regctr.ddt_reg_ctr();
		} catch (...) {
			__objctr.obj_del_last_n(1);
			twarn(ErrCompile_InvalidLiter).warn("tcp::parse_var", tok.value_3);
		}
	}
}

/** Parse let statement
 *
 *  @code
 *      let name [: type] [= value]
 *  @endcode
 *
 *  Case 1: If `name` is not declared as any environmental or temporary
 *      variable in current environment, then let will create a new
 *      temporary variable.
 *  Case 2: If `name` is already declared as environmental or temporary
 *      variable in current environment, then `Duplicated Declaration`
 *      Compilation Error will be throw out.
 *  Case 3: If declaration is successful and a `value` is attached, then
 *      execute `value` and assign the returned to the variable `name`.
 *
 *  Note:
 *      All temporary variable will be deleted at the end of block, so
 *      it can be declared in block.
 */
void parse_let(const ttoken & tok, tvmcmd_vect & tcmds, tconsts & consts,
		std::vector<std::string> & paths, bool inblk)
{
	uint_size_obj loc_obj = __objctr.obj_loc(tok.value_1);
	uint_size_cst nameloc = UNDEF_NAMELOC;

	// If the name is declared in current environment
	if (loc_obj < __objctr.obj_len_in_current_env())
		twarn(ErrCompile_DblVDeclare).warn("tcp::parse_let", tok.value_1);

	uint_size_obj loc_tmp = __tmpctr.obj_create(tok.value_1, 0, consts, nameloc); // duplicated checking here
	tcmds.append(tbycode(OP_VCRT, nameloc, 0));

	// If there is value to be assigned
	if (tok.nval == 3) {
		try {
			parse_unit(tok.value_3, tcmds, consts, paths, 0, inblk);
			tcmds.append(tbycode(OP_POPCOV, loc_tmp, 0));
			__regctr.ddt_reg_ctr();
		} catch (...) {
			__tmpctr.obj_del_last_n(1);
			twarn(ErrCompile_InvalidLiter).warn("tcp::parse_let", tok.value_3);
		}
	}
}

/** Parse import statement
 */
void parse_import(const ttoken & tok, tvmcmd_vect & tcmds, tconsts & consts,
		std::vector<std::string> & paths, bool inblk)
{
	std::string file = tok.value_1;

	if (file.length() == 0)
		twarn(ErrCompile_InvalidLiter).warn("tcp::parse_import", "empty liter");
	if (!find_imported_file(file, paths))
		twarn(ErrCompile_UnfoundFile).warn("tcp::parse_import", tok.value_1);
	tcp comp(__objctr.first_n_objs(__n_default_objs), nullptr);
	comp.compile_file(file, paths);

	if (tok.nval ==2) {
		uint_size_cst nameloc = UNDEF_NAMELOC;
		uint_size_obj loc = __objctr.obj_create(tok.value_2, inblk, consts, nameloc);
		tcmds.append(tbycode(OP_VCRT, nameloc, 1));
		uint_size_cst sloc = consts.add_str_const(file);
		tcmds.append(tbycode(OP_IMPORT, sloc));
		__regctr.add_reg_ctr();
		tcmds.append(tbycode(OP_POPCOV, loc, 1));
		__regctr.ddt_reg_ctr();
	} else {
		uint_size_cst sloc = consts.add_str_const(file);
		tcmds.append(tbycode(OP_IMPORT, sloc));
		__regctr.add_reg_ctr();
		tcmds.append(tbycode(OP_POPN, uint16_t(1), uint16_t(0)));
		__regctr.ddt_reg_ctr();
	}
}

/** Parse while statement
 */
void parse_while (const ttoken & tok, tvmcmd_vect & tcmds, tconsts & consts, std::vector<std::string> & paths)
{
	uint_size_cmd ncmds_ori = tcmds.size32();
	tvmcmd_vect tcmds_blk;

	// Parse blk_s
	parse_unit(tok.value_1, tcmds, consts, paths, 0, 1);
	__regctr.ddt_reg_ctr(); // this is for the CJPFPOP ins below
	parse_blk(tok.value_2, tcmds_blk, consts, paths, 1, 1);

	// Conditional Jump Forward
	uint_size_cmd cjpfpop_n = tcmds_blk.size32() + 1;
	tcmds.append(tbycode(OP_CJPFPOP, cjpfpop_n));
	tcmds.insert(tcmds.end(), tcmds_blk.begin(), tcmds_blk.end());

	// Jump Back
	uint_size_cmd jpb_n = 1 + tcmds.size32() - ncmds_ori;
	tcmds.append(tbycode(OP_JPB, jpb_n));
}

/** Parse for statement
 */
void parse_for (const ttoken & tok, tvmcmd_vect & tcmds, tconsts & consts,
		std::vector<std::string> & paths, bool cleanstk, bool inblk)
{
	// Define bycode vector
	tvmcmd_vect tcmds_blk;
	// Compile Right Part of 'in' Condition
	parse_unit(tok.value_2, tcmds, consts, paths, 0, inblk);
	// Find element var loc
	uint_size_obj loc = 0;
	bool isenv = false;
	uint_size_obj loc_left = __objctr.obj_loc(tok.value_1);
	uint_size_obj tmp_left = __tmpctr.obj_loc(tok.value_1);
	uint_size_obj nobjs = __objctr.obj_len_in_current_env();
	uint_size_obj ntmps = __tmpctr.obj_len_in_current_env();

	if (tmp_left != __tmpctr.obj_len_in_all())
		loc = tmp_left;
	else if (loc_left != __objctr.obj_len_in_all()) {
		loc = loc_left;
		isenv = true;
	} else {
		parse_unit(tok.value_1, tcmds, consts, paths, cleanstk, inblk);
		// In loop declaration of environmental variable is NOT allowed
		if (__objctr.obj_len_in_current_env() >= nobjs + 1)
			twarn(ErrCompile_InBlkVarDef).warn("tcp::parse_for", tok.value_1);
		// It has to be a temporary variable to be declared, otherwise
		if (__tmpctr.obj_len_in_current_env() < ntmps + 1)
			twarn(ErrCompile_ObjUnfound).warn("tcp::parse_for", "");
		loc = __tmpctr.obj_len_in_current_env() - 1;
	}

	// Prepare Loop Assignment
	tcmds.append(tbycode(OP_LOOPAS, loc, isenv));
	__regctr.add_reg_ctr();

	// Compile Block
	__regctr.ddt_reg_ctr();

	try {
		parse_blk(tok.value_3, tcmds_blk, consts, paths, 1, 1);
		uint_size_cmd cjpfpop_n = 1 + tcmds_blk.size32();
		tcmds.append(tbycode(OP_CJPFPOP, cjpfpop_n));
		tcmds.insert(tcmds.end(), tcmds_blk.begin(), tcmds_blk.end());
		uint_size_cmd jpb_n = 3 + tcmds_blk.size32();
		tcmds.append(tbycode(OP_JPB, jpb_n));
		tcmds.append(tbycode(OP_POPN, uint16_t(1), uint16_t(0)));
		__regctr.ddt_reg_ctr();
		uint_size_obj newtmps = __tmpctr.obj_len_in_current_env() - ntmps;

		// Delete temporary variables if there is any declare in for statement
		if (newtmps > 0) {
			__tmpctr.obj_del_last_n(newtmps);
			tcmds.append(tbycode(OP_TMPDEL, newtmps));
		}
	} catch(...) {
		__tmpctr.obj_del_last_n(1);
		twarn(ErrCompile_InvalidLiter).warn("tcp::parse_for", tok.value_3);
	}
}

/** Parse if statement
 */
void parse_if (const ttoken & tok, tvmcmd_vect & tcmds, tconsts & consts, std::vector<std::string> & paths, bool inblk)
{
	// Compile con
	parse_unit(tok.value_1, tcmds, consts, paths, 0, inblk);
	// CJPFPOP
	tcmds.append(tbycode(OP_CJPFPOP, 0));
	uint_size_cmd loc_CJPFPOP = tcmds.size32() - 1;
	__regctr.ddt_reg_ctr();
	// Parse BLK
	tvmcmd_vect tcmds_blk;
	parse_blk(tok.value_2, tcmds_blk, consts, paths, 1, 1);
	tcmds.insert(tcmds.end(), tcmds_blk.begin(), tcmds_blk.end());
	tcmds.append(tbycode(OP_PASS));
	// Revise
	tcmds[loc_CJPFPOP] = tbycode(OP_CJPFPOP, tcmds_blk.size32() + 1);
}

/** Parse elif statement
 */
void parse_elif (const ttoken & tok, tvmcmd_vect & tcmds, tconsts & consts, std::vector<std::string> & paths)
{
	// Check tcmd[-1] == OP_PASS
	if (tcmds.size() > 0 && tcmds.back().ins() == OP_PASS)
		tcmds.pop_back();
	// JPF
	tcmds.append(tbycode(OP_JPF, 0));
	uint_size_cmd loc_JPF = tcmds.size32() - 1;
	// Compile con
	tvmcmd_vect tcmds_con;
	parse_unit(tok.value_1, tcmds_con, consts, paths, 0, 1);
	tcmds.insert(tcmds.end(), tcmds_con.begin(), tcmds_con.end());
	// CJPFPOP
	tcmds.append(tbycode(OP_CJPFPOP, 0));
	__regctr.ddt_reg_ctr();
	uint_size_cmd loc_CJPFPOP = tcmds.size32() - 1;
	// Compile blk
	tvmcmd_vect tcmds_blk;
	parse_blk(tok.value_2, tcmds_blk, consts, paths, 1, 1);
	tcmds.insert(tcmds.end(), tcmds_blk.begin(), tcmds_blk.end());
	tcmds.append(tbycode(OP_PASS));
	// Revise
	tcmds[loc_JPF] = tbycode(OP_JPF, tcmds_con.size32() + 1 + tcmds_blk.size32());
	tcmds[loc_CJPFPOP] = tbycode(OP_CJPFPOP, tcmds_blk.size32() + 1);
}

/** Parse else statement
 */
void parse_else(const ttoken & tok, tvmcmd_vect & tcmds, tconsts & consts, std::vector<std::string> & paths)
{
	// Check tcmd[-1] == OP_PASS
	if (tcmds.size() > 0 && tcmds.back().ins() == OP_PASS)
		tcmds.pop_back();
	// Compile blk
	tvmcmd_vect tcmds_blk;
	parse_blk(tok.value_1, tcmds_blk, consts, paths, 1, 1);
	// Combine
	tcmds.append(tbycode(OP_JPF, tcmds_blk.size32()));
	tcmds.insert(tcmds.end(), tcmds_blk.begin(), tcmds_blk.end());
}

/** Parse assignment statement
 *
 *  @code
 *      name = value
 *  @endcode
 *
 *  Case 1: If `name` is a temporary variable, then assign value to it.
 *  Case 2: If `name` is a environmental variable, then
 *      Case 2.1: `name` is a default environmental variable, then
 *                throw `ErrCompile_AsgDefault` Compilation error.
 *      Case 2.2: `name` is not a default environmental variable,
 *                then assign value to it.
 *  Case 3: If `name` is unfound, then throw `ErrCompile_ObjUnfound`
 *          compilation error.
 */
void parse_asg(const ttoken & tok, tvmcmd_vect & tcmds, tconsts & consts, std::vector<std::string> & paths, bool inblk)
{
	uint_size_obj loc = 0;
	uint_size_obj loc_env = __objctr.obj_loc(tok.value_1);
	uint_size_obj loc_tmp = __tmpctr.obj_loc(tok.value_1);
	bool isenv = false;

	if (loc_tmp != __tmpctr.obj_len_in_all())
		loc = loc_tmp;
	else if (loc_env != __objctr.obj_len_in_all()) {
		if (__objctr.is_preload(loc_env))
			twarn(ErrCompile_AsgDefault).warn("tcp::parse_asg", tok.value_1);
		loc = loc_env;
		isenv = true;
	} else
		twarn(ErrCompile_ObjUnfound).warn("tcp::parse_asg", tok.value_1);

	parse_unit(tok.value_2, tcmds, consts, paths, 0, inblk);
	tcmds.append(tbycode(OP_POPCOV, loc, isenv));
	__regctr.ddt_reg_ctr();
}

/** Parse left-value indexing statement
 *
 *  @code
 *     set[idx] = value
 *  @endcode
 */
void parse_idxl(const ttoken & tok, tvmcmd_vect & tcmds, tconsts & consts,
		std::vector<std::string> & paths, bool inblk)
{
	// Search the left value
	uint_size_obj loc = 0;
	uint_size_obj loc_tmp = __tmpctr.obj_loc(tok.value_1);
	uint_size_obj loc_env = __objctr.obj_loc(tok.value_1);
	bool isenv = false;

	if (loc_tmp != __tmpctr.obj_len_in_all())
		loc = loc_tmp;
	else if (loc_env != __objctr.obj_len_in_all()) {
		if (__objctr.is_preload(loc_env))
			twarn(ErrCompile_AsgDefault).warn("tcp::parse_idxl", tok.value_1);
		loc = loc_env;
		isenv = true;
	} else
		twarn(ErrCompile_ObjUnfound).warn("tcp::parse_idxl", tok.value_1);

	// Compile the right value
	parse_unit(tok.value_3, tcmds, consts, paths, 0, inblk);
	// Compile the parameters as indexes
	uint_size_reg n = parse_params(tok.value_2, tcmds, consts, paths, inblk);
	// OP_IDXL
	tcmds.append(tbycode(OP_IDXL, loc, n, isenv));
	__regctr.ddt_reg_ctr_n(n + 1);
}

/** Parse right-value indexing statement
 *
 *  Case 1: Generate list
 *  @code
 *      [obj_1, obj_2, ...]
 *  @endcode
 *  Reform the above into `tolist(obj_1, obj_2, ...)`.
 *
 *  Case 2: Normal indexing
 *  @code
 *      arr[obj_1, obj_2, ...]
 *  @endcode
 */
void parse_idx(const ttoken & tok, tvmcmd_vect & tcmds, tconsts & consts,
		std::vector<std::string> & paths, bool cleanstk, bool inblk)
{
	if (tok.value_1.size() == 0) {
		std::string reform = "std::tolist(" + tok.value_2 + ")";
		parse_unit(reform, tcmds, consts, paths, cleanstk, inblk);
	} else {
		// Compile parameters
		uint_size_reg n = parse_params(tok.value_2, tcmds, consts, paths, inblk);
		// Compile the object to be indexed
		parse_unit(tok.value_1, tcmds, consts, paths, 0, inblk);
		// OP_IDXR
		tcmds.append(tbycode(OP_IDXR, n));
		__regctr.ddt_reg_ctr_n(1 + n); // arr + params
		__regctr.add_reg_ctr_n(1);     // returned value
	}
}

/** Parse read-only string indexing.
 *
 *  @code
 *      arr::key
 *  @endcode
 *  Equivalent to `arr['key']`
 */
void parse_idx2(const ttoken & tok, tvmcmd_vect & tcmds, tconsts & consts,
		std::vector<std::string> & paths, bool inblk)
{
	parse_unit("'" + tok.value_2 + "'", tcmds, consts, paths, 0, inblk);
	parse_unit(tok.value_1, tcmds, consts, paths, 0, inblk);
	tcmds.append(tbycode(OP_IDXR, 1));
	__regctr.ddt_reg_ctr(); // 1 parameter
}

/** Parse evaluation expression
 */
void parse_eval(const ttoken & tok, tvmcmd_vect & tcmds, tconsts & consts,
		std::vector<std::string> & paths, bool inblk)
{
	// Compile parameters
	uint_size_reg n = parse_params(tok.value_2, tcmds, consts, paths, inblk);
	// Compile the object to be evalated
	parse_unit(tok.value_1, tcmds, consts, paths, 0, inblk);
	// OP_EVAL
	tcmds.append(tbycode(OP_EVAL, n));
	__regctr.ddt_reg_ctr_n(1 + n);  // f + params
	__regctr.add_reg_ctr_n(1);      // returned value
}

/** Parse dictionary
 */
void parse_dict(const ttoken & tok, tvmcmd_vect & tcmds, tconsts & consts,
		std::vector<std::string> & paths, bool inblk)
{
	uint_size_reg n = parse_params(tok.value_1, tcmds, consts, paths, inblk);
	tcmds.append(tbycode(OP_PUSHDICT, n));
	__regctr.ddt_reg_ctr_n(n);
	__regctr.add_reg_ctr();
}

/** Parse function
 */
void parse_func(const ttoken & tok, tvmcmd_vect & tcmds, tconsts & consts, std::vector<std::string> & paths)
{
	// get params (string_vect)
	std::vector<std::string> params;
	uint_size_reg nparams = UNDEF_NPARAMS; // undef by default

	if (tok.value_1 != "...") {
		tunit_splitter().split_params_by_comma(tok.value_1, params);
		nparams = static_cast<uint_size_reg>(params.size());
	}

	// get new_syner
	tcp new_syner(params, &__objctr);

	// parse blk
	tvmcmd_vect tcmdblk;
	new_syner.parse_blk(tok.value_2, tcmdblk, consts, paths, 1, 0);
	tcinfo info = new_syner.get_compile_info();

	uint_size_cmd ncmds = tcmdblk.size32();
	uint_size_obj nobjs = info.obj_max;
	uint_size_obj ntmps = info.tmp_max;
	uint_size_reg nregs = info.reg_max;

	tcmds.append(tbycode(OP_PUSHINFO, nobjs));
	tcmds.append(tbycode(OP_PUSHINFO, ntmps));
	tcmds.append(tbycode(OP_PUSHINFO, nregs));
	tcmds.append(tbycode(OP_PUSHINFO, nparams));
	__regctr.add_reg_ctr_n(4); // push infos
	tcmds.append(tbycode(OP_PUSHF, ncmds));
	__regctr.ddt_reg_ctr_n(4); // pop infos
	__regctr.add_reg_ctr();    // push function
	tcmds.insert(tcmds.end(), tcmdblk.begin(), tcmdblk.end());
}

/** Parse Kapps expression
 *
 *  @code
 *      #{ ... block codes ... }
 *  @endcode
 *
 *  The VM stack would not be cleaned during excution, and the top value of
 *      the VM stack would be returned after excution.
 */
void parse_kappa(const ttoken & tok, tvmcmd_vect & tcmds, tconsts & consts, std::vector<std::string> & paths)
{
	tcp new_syner(&__objctr);
	tvmcmd_vect tcmdblk;
	new_syner.parse_blk(tok.value_1, tcmdblk, consts, paths, 0, 0);
	tcmdblk.append(OP_RET);
	tcinfo info = new_syner.get_compile_info();

	uint_size_cmd ncmds = tcmdblk.size32();
	uint_size_obj nobjs = info.obj_max;
	uint_size_obj ntmps = info.tmp_max;
	uint_size_reg nregs = info.reg_max;

	tcmds.append(tbycode(OP_PUSHINFO, nobjs));
	tcmds.append(tbycode(OP_PUSHINFO, ntmps));
	tcmds.append(tbycode(OP_PUSHINFO, nregs));
	tcmds.append(tbycode(OP_PUSHINFO, 0)); // nparams
	__regctr.add_reg_ctr_n(4); // push infos
	tcmds.append(tbycode(OP_PUSHF, ncmds));
	__regctr.ddt_reg_ctr_n(4); // pop infos
	__regctr.add_reg_ctr();    // push function
	tcmds.insert(tcmds.end(), tcmdblk.begin(), tcmdblk.end());
}

/// Key method: parse a token
void parse_token(const ttoken & tok, tvmcmd_vect & tcmds, tconsts & consts,
		std::vector<std::string> & paths, bool cleanstk, bool inblk)
{
	switch (tok.type) {
	// statement
	case token_continue:
		tcmds.append(tbycode(OP_CONTI));
		break;
	case token_break:
		tcmds.append(tbycode(OP_BREAK));
		break;
	case token_return:
		parse_return(tok, tcmds, consts, paths, inblk);
		break;
	case token_var:
		parse_var(tok, tcmds, consts, paths, inblk);
		break;
	case token_let:
		parse_let(tok, tcmds, consts, paths, inblk);
		break;
	case token_import:
		parse_import(tok, tcmds, consts, paths, inblk);
		break;
	case token_while:
		parse_while (tok, tcmds, consts, paths);
		break;
	case token_for:
		parse_for (tok, tcmds, consts, paths, cleanstk, inblk);
		break;
	case token_if:
		parse_if (tok, tcmds, consts, paths, inblk);
		break;
	case token_elif:
		parse_elif (tok, tcmds, consts, paths);
		break;
	case token_else:
		parse_else(tok, tcmds, consts, paths);
		break;
	case token_asg:
		parse_asg(tok, tcmds, consts, paths, inblk);
		break;
	case token_idxl:
		parse_idxl(tok, tcmds, consts, paths, inblk);
		break;

	// expression: binary operators
	case token_in:
		parse_binop(OP_IN, tok, tcmds, consts, paths, inblk);
		break;
	case token_pair:
		parse_binop(OP_PAIR, tok, tcmds, consts, paths, inblk);
		break;
	case token_to:
		parse_binop(OP_TO, tok, tcmds, consts, paths, inblk);
		break;
	case token_and:
		parse_binop(OP_AND, binop_split(tok), tcmds, consts, paths, inblk);
		break;
	case token_or:
		parse_binop(OP_OR, binop_split(tok), tcmds, consts, paths, inblk);
		break;
	case token_eq:
		parse_binop(OP_EQ, binop_split(tok), tcmds, consts, paths, inblk);
		break;
	case token_ne:
		parse_binop(OP_NE, binop_split(tok), tcmds, consts, paths, inblk);
		break;
	case token_ge:
		parse_binop(OP_GE, binop_split(tok), tcmds, consts, paths, inblk);
		break;
	case token_le:
		parse_binop(OP_LE, binop_split(tok), tcmds, consts, paths, inblk);
		break;
	case token_sg:
		parse_binop(OP_SG, binop_split(tok), tcmds, consts, paths, inblk);
		break;
	case token_sl:
		parse_binop(OP_SL, binop_split(tok), tcmds, consts, paths, inblk);
		break;
	case token_add:
		parse_binop(OP_ADD, binop_split(tok), tcmds, consts, paths, inblk);
		break;
	case token_sub:
		parse_binop(OP_SUB, binop_split(tok), tcmds, consts, paths, inblk);
		break;
	case token_mul:
		parse_binop(OP_MUL, binop_split(tok), tcmds, consts, paths, inblk);
		break;
	case token_div:
		parse_binop(OP_DIV, binop_split(tok), tcmds, consts, paths, inblk);
		break;
	case token_mod:
		parse_binop(OP_MOD, binop_split(tok), tcmds, consts, paths, inblk);
		break;
	case token_mmul:
		parse_binop(OP_MMUL, binop_split(tok), tcmds, consts, paths, inblk);
		break;
	case token_pow:
		parse_binop(OP_POW, binop_split(tok), tcmds, consts, paths, inblk);
		break;

	// expression: calling and indexing
	case token_eval:
		parse_eval(tok, tcmds, consts, paths, inblk);
		break;
	case token_idx:
		parse_idx(tok, tcmds, consts, paths, cleanstk, inblk);
		break;
	case token_idx2:
		parse_idx2(tok, tcmds, consts, paths, inblk);
		break;

	// expression: values
	case token_true:
		tcmds.append(tbycode(OP_PUSHB, 1));
		__regctr.add_reg_ctr();
		break;
	case token_false:
		tcmds.append(tbycode(OP_PUSHB, 0));
		__regctr.add_reg_ctr();
		break;
	case token_this:
		tcmds.append(tbycode(OP_THIS));
		__regctr.add_reg_ctr();
		break;
	case token_base:
		tcmds.append(tbycode(OP_BASE));
		__regctr.add_reg_ctr();
		break;
	case token_sstr:
		tcmds.append(tbycode(OP_PUSHS, consts.add_str_const(tok.value_1)));
		__regctr.add_reg_ctr();
		break;
	case token_dstr:
		tcmds.append(tbycode(OP_PUSHS, consts.add_str_const(tok.value_1)));
		__regctr.add_reg_ctr();
		break;
	case token_dict:
		parse_dict(tok, tcmds, consts, paths, inblk);
		break;
	case token_func:
		parse_func(tok, tcmds, consts, paths);
		break;
	case token_kappa:
		parse_kappa(tok, tcmds, consts, paths);
		break;
	case token_v:
		parse_v(tok, tcmds, consts);
		break;
	}
}

void clean_stk(tvmcmd_vect & tcmds, bool isroot, uint_size_reg regs_ori)
{
	uint_size_reg regs_now = __regctr.get_reg_ctr();

	if (regs_now < regs_ori)
		twarn(ErrCompile_REGOutOfLimit).warn("tcp::cmd_cleanStk", "");
	if (false == isroot || regs_now == regs_ori)
		return;

	uint_size_reg regs_ddt = regs_now - regs_ori;
	tcmds.append(tbycode(OP_POPN, uint16_t(regs_ddt), uint16_t(__interactive)));
	__regctr.ddt_reg_ctr_n(regs_ddt);
}

public:

/** Constructor of the compiler
 *  @param father_objctr  father object counter
 *  @param interactive    compile in interactive mode
 */
tcp(tobj_ctr * father_objctr, bool interactive = true)
{
	__objctr = tobj_ctr(father_objctr);
	__n_default_objs = 0;
	__interactive = interactive;
}

/** Constructor of the compiler
 *  @param default_objs   default object counter in current environment
 *  @param father_objctr  father object counter
 *  @param interactive    compile in interactive mode
 */
tcp(const std::vector<std::string> & default_objs, tobj_ctr * father_objctr, bool interactive = true)
{
	__objctr = tobj_ctr(default_objs, father_objctr);
	__n_default_objs = default_objs.size();
	__interactive = interactive;
}

/// @return Compilation information incluing environmental/temporary objects
/// numbers and register usages.
tcinfo get_compile_info()
{
	return tcinfo { __objctr.obj_max_in_current_env(),
			__tmpctr.obj_max_in_current_env(),
			__regctr.get_reg_max(), 0 };
}

/// Parse a syntax unit
tcinfo parse_unit(const std::string & str, tvmcmd_vect & tcmds, tconsts & consts,
		std::vector<std::string> & paths, bool cleanstk, bool inblk)
{
	// split by ';'
	std::string cmd = str;
	tunit_splitter splitter;
	std::vector<std::string> cmdvec;
	tunit_splitter().split_units_by_semicolon(cmd, cmdvec);

	if (cmdvec.size() > 1) {
		for (auto it = cmdvec.begin(); it != cmdvec.end(); it++)
			parse_unit(*it, tcmds, consts, paths, cleanstk, inblk);
		return get_compile_info();
	}

	// preprocessing of compiling : change source codes
	splitter.preprocessing(cmd);
	uint_size len = cmd.length();
	if (len == 0)
		return get_compile_info();

	// lexing
	std::vector<ttoken> tokens;
	get_tokens(cmd, tokens);

	// the register number becore compilation of cmds
	uint_size_reg regs_ori = __regctr.get_reg_ctr();

	// compilation
	for (auto iter = tokens.begin(); iter != tokens.end(); iter++)
		parse_token(*iter, tcmds, consts, paths, cleanstk, inblk);
	clean_stk(tcmds, cleanstk, regs_ori);
	__lexctr.restore_lex_ctrs();
	return get_compile_info();
}

/// Parse a sequence of syntax units respectively
void parse_unit_seqs(std::vector<std::string> & units, tvmcmd_vect & tcmds, tconsts & consts,
			std::vector<std::string> & paths, bool cleanstk, bool inblk)
{
	for (auto iter = units.begin(); iter != units.end(); iter++) {
		try {
			parse_unit(*iter, tcmds, consts, paths, cleanstk, inblk);
		} catch (...) {
			twarn(ErrCompile_Other).warn("tcp::parse_units", *iter);
		}
	}
}

/// Parse parameter list and return nparams
uint_size_reg parse_params(const std::string & str, tvmcmd_vect & tcmds, tconsts & consts,
			std::vector<std::string> & paths, bool inblk)
{
	std::vector<std::string> params;
	tunit_splitter().split_params_by_comma(str, params);
	parse_unit_seqs(params, tcmds, consts, paths, 0, inblk);
	return params.size();
}

/** Split and parse any 'str' and get bycodes 'tcmds'.
 *  @details Delete all temporary variables that is created in the block
 *           after execution.
 *  @return Compilation info.
 */
tcinfo parse_blk(const std::string & str, tvmcmd_vect & tcmds, tconsts & consts,
		std::vector<std::string> & paths, bool cleanstk, bool inblk)
{
	std::vector<std::string> units;
	tunit_splitter().lex_str(str, units);
	uint_size_obj ntmps = __tmpctr.obj_len_in_all();
	parse_unit_seqs(units, tcmds, consts, paths, cleanstk, inblk);
	uint_size_obj newtmps = __tmpctr.obj_len_in_all() - ntmps;

	if (newtmps > 0) {
		__tmpctr.obj_del_last_n(newtmps);
		tcmds.append(tbycode(OP_TMPDEL, newtmps));
	}
	return get_compile_info();
}

/// Parse a tap source code file and get bycodes 'tcmds'
tcinfo parse_file(FILE * f, tvmcmd_vect & tcmds, tconsts & consts, std::vector<std::string> & paths)
{
	std::vector<std::string> units;
	tunit_splitter().lex_file(f, units);
	parse_unit_seqs(units, tcmds, consts, paths, 1, 0);
	return get_compile_info();
}

/// Parse a markdown style tap source code fule and get bycode 'tcmds'
tcinfo parse_md_file(FILE * f, tvmcmd_vect & tcmds, tconsts & consts, std::vector<std::string> & paths)
{
	std::vector<std::string> units;
	tunit_splitter().lex_md_file(f, units);
	parse_unit_seqs(units, tcmds, consts, paths, 1, 0);
	return get_compile_info();
}

/// Parse a string and return wrapper.
twrapper * compile_str(const std::string & str, std::vector<std::string> & paths)
{
	tconsts consts;
	tvmcmd_vect tcmds;
	twrapper * wrapper = nullptr;

	try {
		tcinfo info = parse_blk(str, tcmds, consts, paths, 1, 0);
		wrapper = tanalyser().wrap(tcmds, consts, info);
	} catch(...) {
		tanalyser().clean_wrapper(wrapper);
		twarn(ErrCompile_Other).warn("tcp::compile_str", str);
	}
	return wrapper;
}

/** Compile file and return wrapper.
 *  @details If a file is compiled under a lib, its path is added to the lib.
 */
twrapper * compile_file_2(const std::string & file, std::vector<std::string> & paths)
{
	FILE * f = nullptr;
	twrapper * wrapper = nullptr;

	// Check is md file
	bool ismd = false;
	std::string suffix = file.substr(file.find_last_of("."));

	if (suffix == ".tap" || suffix == ".Tap" || suffix == ".TAP")
		ismd = false;
	else if (suffix == ".md" || suffix == ".Md" || suffix == ".MD")
		ismd = true;
	else
		twarn(ErrCompile_InvalidFile).warn("tcp::compile_file_2", file);

	if (nullptr == (f = fopen(file.c_str(), "r")))
		twarn(ErrCompile_UnfoundFile).warn("tcp::compile_file_2", file);

	try {
		tconsts consts;
		tvmcmd_vect tcmds;
		utils::append_to_pathpool(paths, utils::get_folderpath_from_filepath(file));
		tcinfo info = ismd ? \
				  parse_md_file(f, tcmds, consts, paths)
				: parse_file(f, tcmds, consts, paths);
		wrapper = tanalyser().wrap(tcmds, consts, info);
	} catch(...) {
		fclose(f);
		tanalyser().clean_wrapper(wrapper);
		twarn(ErrCompile_Other).warn("tcp::compile_file_2", file);
	}
	fclose(f);
	return wrapper;
}

/** Compile file and save binary codes to local disk.
 *  @details If a file is compiled under a lib, its path is added to the lib.
 */
void compile_file(const std::string & file, std::vector<std::string> & paths)
{
	twrapper * wrapper = compile_file_2(file, paths);
	std::string filename = file.substr(0, file.find_last_of("."));
	tanalyser analyser;
	analyser.save_bin_file(wrapper, filename + ".tapc");
	analyser.clean_wrapper(wrapper);
}

};

}

#endif // TC_H
