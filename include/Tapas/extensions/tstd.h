#ifndef TSTD_H
#define TSTD_H

#include <unordered_map>
#include <ctime>
#include "../trts.h"

namespace tapas
{

/// String. Created by single or double quotes.
class tstr : public tcompo_v, public std::string
{
private:

void idx_int(long idx, tobj & vre)
{
	unsigned long idxu = static_cast<unsigned long>(idx);

	if (idx < 0 || idxu >= length())
		twarn(ErrRuntime_IdxOutRange).warn("tlist::idx_int", "");

	vre.set_v(new tstr(substr(idxu, 1)));
}

void idx_pair(tpair * const pair, tobj & idxre)
{
	tobj && first = pair->get_first();
	tobj && second = pair->get_second();

	if (first.get_type() != tint || second.get_type() != tint)
		twarn(ErrRuntime_ParamsType).warn("tstr::idx_iter", "Should be 'tint'");

	long v1 = first.get_v_tint();
	long v2 = second.get_v_tint();

	if (v1 < 0 || v2 < 0)
		twarn(ErrRuntime_IdxOutRange).warn("tlist::idx_iter", "");

	unsigned long uv1 = static_cast<unsigned long>(v1);
	unsigned long uv2 = static_cast<unsigned long>(v2);

	if (uv1 > uv2)
		twarn(ErrRuntime_InvalidIndex).warn("tlist::idx_iter", "");
	if (uv2 > length()) // v2 could = length
		twarn(ErrRuntime_IdxOutRange).warn("tlist::idx_iter", "");

	idxre.set_v(new tstr(this->substr(uv1, uv2 - uv1)));
}

void iset_int(const long idx, const tstr * const str)
{
	unsigned long idxu = static_cast<unsigned long>(idx);

	if (idx < 0 || idxu >= length())
		twarn(ErrRuntime_IdxOutRange).warn("tstr::iset_int", "idx out of scope");
	if (str->length() != 1)
		twarn(ErrRuntime_LenInconsis).warn("tstr::iset_int", "len inconsistency");

	this->replace(idxu, 1, *str);
}

void iset_pair(tpair * const pair, const tstr * const str)
{
	uint_size len = str->length();
	tobj && first = pair->get_first();
	tobj && second = pair->get_second();
	long v1 = first.get_v_tint();
	long v2 = second.get_v_tint();

	if (v1 < 0 || v2 < 0)
		twarn(ErrRuntime_IdxOutRange).warn("tstr::iset_pair", "");

	unsigned long uv1 = static_cast<unsigned long>(v1);
	unsigned long uv2 = static_cast<unsigned long>(v2);

	if (uv1 > length() || uv2 > length())
		twarn(ErrRuntime_IdxOutRange).warn("tstr::iset_pair", "");
	if (uv1 > uv2)
		twarn(ErrRuntime_InvalidIndex).warn("tstr::iset_pair", "");
	if (uv2 - uv1 != len)
		twarn(ErrRuntime_LenInconsis).warn("tstr::iset_pair", "");

	replace(uv1, len, *str);
}

public:
tstr(const char * str) : std::string(str) {}

tstr(std::string str) : std::string(str) {}

~tstr() {}

std::string tostring_abbr() const
{
	return *this;
}

std::string tostring_full() const
{
	return *this;
}

tstr * copy()
{
	return new tstr(c_str());
}

const char * get_type() const
{
	return "String";
}

tcompo_type get_compo_type_code() const
{
	return compo_tstr;
}

void idx(const tobj * params, uint_size_reg nparams, tobj & idxre)
{
	if (nparams != 1)
		twarn(ErrRuntime_ParamsCtr).warn("tstr::idx", "1 parameter");
	if (params->get_type() != tint && params->get_type() != tcompo)
		twarn(ErrRuntime_ParamsType).warn("tstr::idx", "Type Unsupported");

	if (params->get_type() == tcompo) {
		tcompo_v * pv = params->get_v_tcompo();

		if (pv->get_compo_type_code() != compo_tpair)
			twarn(ErrRuntime_ParamsType).warn("tstr::idx", "Type Unsupported");
		tpair * ptype = reinterpret_cast<tpair *>(pv);
		idx_pair(ptype, idxre);
	}
	if (params->get_type() == tint)
		idx_int(params->get_v_tint(), idxre);
}

void iset(const tobj * params, uint_size_reg nparams, const tobj & v)
{
	// Determine v
	if (v.get_type() != tcompo)
		twarn(ErrRuntime_RefType).warn("tstr::iset", "Should be 'tcompo'");
	tcompo_v * pv = v.get_v_tcompo();

	if (pv->get_compo_type_code() != compo_tstr)
		twarn(ErrRuntime_RefType).warn("tstr::iset", "Should be 'tstr'");
	tstr * str = reinterpret_cast<tstr *>(pv);

	// Determine params
	if (nparams != 1)
		twarn(ErrRuntime_ParamsCtr).warn("istring::iset", "1 parameter");
	if (params->get_type() != tint && params->get_type() != tcompo)
		twarn(ErrRuntime_ParamsType).warn("istring::iset", "Unsupported");

	// Assignment
	if (params->get_type() == tcompo)
	{
		tcompo_v * compo = params->get_v_tcompo();

		if (compo->get_compo_type_code() != compo_tpair)
			twarn(ErrRuntime_ParamsType).warn("istring::iset", "Unsupported");
		tpair * pair = reinterpret_cast<tpair *>(compo);
		iset_pair(pair, str);
	}
	if (params->get_type() == tint)
		iset_int(params->get_v_tint(), str);
}

long len() const
{
	return static_cast<long>(length());
}

bool identical(tcompo_v * v) const
{
	if (v->get_compo_type_code() != compo_tstr)
		return false;
	tstr * str = reinterpret_cast<tstr *>(v);
	return str->compare(*this) == 0;
}

void set_append(const tobj * ele)
{
	switch (ele->get_type()) {
	case tnil:
		break;
	case tbool:
		switch (ele->get_v_tbool()) {
		case 0:
			this->append("false");
			break;
		case 1:
			this->append("true");
			break;
		}
		break;
	case tint:
		this->append(std::to_string(ele->get_v_tint()));
		break;
	case tdouble:
		this->append(std::to_string(ele->get_v_tdouble()));
		break;
	case tcompo:
		this->append(ele->get_v_tcompo()->tostring_abbr());
		break;
	}
}

void set_insert(const tobj * ele, const long loc)
{
	if (loc < 0 || static_cast<unsigned long>(loc) > size())
		twarn(ErrRuntime_IdxOutRange).warn("tstr::set_insert", "");

	switch (ele->get_type()) {
	case tnil:
		break;
	case tbool:
		switch (ele->get_v_tbool()) {
		case 0:
			this->insert(loc, "false");
			break;
		case 1:
			this->insert(loc, "true");
			break;
		}
		break;
	case tint:
		this->insert(loc, std::to_string(ele->get_v_tint()));
		break;
	case tdouble:
		this->insert(loc, std::to_string(ele->get_v_tdouble()));
		break;
	case tcompo:
		this->insert(loc, ele->get_v_tcompo()->tostring_abbr());
		break;
	}
}

void set_pop()
{
	if (size() == 0)
		twarn(ErrRuntime_RefEmptySet).warn("tstr::set_pop", "");
	this->erase(size()-1);
}

void set_delete(const long loc)
{
	if (loc < 0 || static_cast<unsigned long>(loc) >= size())
		twarn(ErrRuntime_IdxOutRange).warn("tstr::set_delete", "");
	this->erase(loc, 1);
}

void set_delete(const tobj * key)
{
	if (key->get_type() != tint && key->get_type() != tcompo)
		twarn(ErrRuntime_RefType).warn("tstr::set_pop", "");
	if (key->get_type() == tint)
		set_delete(key->get_v_tint());
	if (key->get_type() == tcompo)
	{
		tcompo_v * v = key->get_v_tcompo();

		if (v->get_compo_type_code() == compo_titer) {
			titer * iter = reinterpret_cast<titer *>(v);
			int ndeleted = 0;

			while (iter->next()) {
				long idx =  iter->get_locidx();

				if (idx - ndeleted < size()) {
					this->erase(idx - ndeleted, 1);
					ndeleted++;
				}
			}
		}
	}
}

void set_delete(const tobj & start, const tobj & to)
{
	if (start.get_type() != tint || to.get_type() != tint)
		twarn(ErrRuntime_RefType).warn("tstr::set_pop", "");
	long i_start = start.get_v_tint();
	long i_to    = to.get_v_tint();

	if (i_start < 0 || i_to < 0)
		twarn(ErrRuntime_IdxOutRange).warn("tstr::set_delete", "");
	if (i_start > i_to || static_cast<unsigned long>(i_to) > size())
		twarn(ErrRuntime_IdxOutRange).warn("tstr::set_delete", "");
	this->erase(i_start, i_to - i_start);
}

bool to_bool()
{
	if (0 == this->compare("true")) return true;
	if (0 == this->compare("false")) return false;
	twarn(ErrRuntime_StringEval).warn("tstr::to_bool", this->c_str());
	return false;
}

long to_int()
{
	long it;

	if (std::string::npos == this->find('.')
	&& std::string::npos == this->find('e')
	&& std::string::npos == this->find('E')
	&& sscanf(this->c_str(), "%li", &it) == 1)
		return it;
	twarn(ErrRuntime_StringEval).warn("tstr::to_int", "");
	return 0;
}

double to_double()
{
	double dt;

	if (sscanf(this->c_str(), "%lf", &dt) == 1)
		return dt;
	twarn(ErrRuntime_StringEval).warn("tstr::to_double", "");
	return 0.0;
}

};

/// List. Created by `[...]`
class tlist : public tcompo_v, public std::vector<tobj>
{
private:
	long   __idxi = 0;
	ttypes __first_ele_type = tnil;

void idx_int(const long idxi, tobj & idxre)
{
	unsigned long idxu = static_cast<unsigned long>(idxi);

	if (idxi < 0 || idxu >= size())
		twarn(ErrRuntime_IdxOutRange).warn("tlist::idx_int", "");
	idxre = at(idxu);
}

void idx_pair(tpair * const pair, tobj & idxre)
{
	std::vector<tobj> sublst;
	tobj && first  = pair->get_first();
	tobj && second = pair->get_second();

	if (first.get_type() != tint || second.get_type() != tint)
		twarn(ErrRuntime_ParamsType).warn("tlist::idx_pair", "");

	long v1 = first.get_v_tint();
	long v2 = second.get_v_tint();

	if (v1 < 0 || v2 < 0)
		twarn(ErrRuntime_IdxOutRange).warn("tlist::idx_pair", "");

	unsigned long uv1 = static_cast<unsigned long>(v1);
	unsigned long uv2 = static_cast<unsigned long>(v2);

	if (uv1 > size() || uv2 > size())
		twarn(ErrRuntime_IdxOutRange).warn("tlist::idx_pair", "");
	if (uv1 > uv2)
		twarn(ErrRuntime_InvalidIndex).warn("tlist::idx_pair", "");
	for (unsigned long ui = uv1; ui < uv2; ui++)
		sublst.push_back(at(ui));

	idxre.set_v(new tlist(sublst)); // for each ele, refctr ++
}

void iset_int(const long idx, const tobj & v)
{
	unsigned long idxu = static_cast<unsigned long>(idx);

	if (idx < 0 || idxu >= size())
		twarn(ErrRuntime_IdxOutRange).warn("tlist::idx_int", "");

	at(idxu).ddc_ref_clear();
	at(idxu) = v;

	if (v.get_type() == tcompo)
		v.get_v_tcompo()->add_refctr();
}

void iset_pair(tpair * iter, const tlist * list)
{
	unsigned long li = 0;
	tobj && first = iter->get_first();
	tobj && second = iter->get_second();

	if (first.get_type() != tint || second.get_type() != tint)
		twarn(ErrRuntime_ParamsType).warn("tlist::iset_pair", "");

	long v1 = first.get_v_tint();
	long v2 = second.get_v_tint();

	if (v1 < 0 || v2 < 0)
		twarn(ErrRuntime_IdxOutRange).warn("tlist::iset_pair", "");
	unsigned long uv1 = static_cast<unsigned long>(v1);
	unsigned long uv2 = static_cast<unsigned long>(v2);

	if (uv1 > uv2)
		twarn(ErrRuntime_InvalidIndex).warn("tlist::iset_pair", "");
	if (uv2 - uv1 != list->size())
		twarn(ErrRuntime_LenInconsis).warn("tlist::iset_pair", "");
	if (uv2 > size()) // v2 could = size
		twarn(ErrRuntime_IdxOutRange).warn("tlist::iset_pair", "");

	for (unsigned long ui = uv1; ui < uv2; ui++) {
		at(ui).ddc_ref_clear();
		at(ui) = list->at(li);

		if (list->at(li).get_type() == tcompo)
			list->at(li).get_v_tcompo()->add_refctr();
		li++;
	}
}

void update_first_ele_type()
{
	if (size() > 0)
		__first_ele_type = at(0).get_type();
	if (size() == 0)
		__first_ele_type = tnil;
}


public:
tlist() {}

tlist(const std::vector<tobj> & params)
{
	for (auto iter = params.cbegin(); iter != params.cend(); iter++)
		set_append(*iter);
}

tlist(tobj * const params, uint_size_reg len)
{
	for (uint_size_reg i = 0; i < len; i++)
		set_append(params + i);
}

~tlist()
{
	for (auto iter = begin(); iter != end(); iter++)
		iter->ddc_ref_clear();
}

std::string tostring_abbr() const
{
	return tostring_pointer(get_type(), this);
}

std::string tostring_full() const
{
	std::string is;
	std::vector<tobj>::const_iterator iter = cbegin();
	is += "[";

	for (; iter != cend(); iter++) {
		tobj v = *iter;
		is += v.tostring_abbr();
		if (iter != cend() - 1) is += ", ";
	}
	is += "]";
	return is;
}

tlist * copy()
{
	tlist * list = new tlist();

	for (auto iter = cbegin(); iter != cend(); iter++) {
		if (iter->get_type() == tnil)
			twarn(ErrRuntime_AssignNil).warn("tlist::copy", "");
		if (iter->get_type() == tcompo)
			iter->get_v_tcompo()->add_refctr();
		list->push_back(*iter);
	}
	list->update_first_ele_type();
	return list;
}

ttypes get_first_ele_type()
{
	return __first_ele_type;
}

tcompo_type get_compo_type_code() const
{
	return compo_tlist;
}

const char * get_type() const
{
	return "List";
}

long len() const
{
	return static_cast<long>(size());
}

void idx(const tobj * params, uint_size_reg nparams, tobj & idxre)
{
	if (nparams != 1)
		twarn(ErrRuntime_ParamsCtr).warn("tlist::idx", "1 parameter");
	if (params->get_type() != tint && params->get_type() != tcompo)
		twarn(ErrRuntime_ParamsType).warn("tlist::idx", "Type Unsupported");

	if (params->get_type() == tint)
		idx_int(params->get_v_tint(), idxre);
	if (params->get_type() == tcompo) {
		if (params->get_v_tcompo()->get_compo_type_code() != compo_tpair)
			twarn(ErrRuntime_ParamsType).warn("tlist::idx", "Type Unsupported");
		tpair * ptype = reinterpret_cast<tpair *>(params->get_v_tcompo());
		idx_pair(ptype, idxre);
	}
}

void iset(const tobj * params, uint_size_reg nparams, const tobj & v)
{
	if (nparams != 1)
		twarn(ErrRuntime_ParamsCtr).warn("tlist::iset", "1 parameter");
	if (params->get_type() == tnil)
		twarn(ErrRuntime_AssignNil).warn("tlist::iset", "");
	if (params->get_type() != tint && params->get_type() != tcompo)
		twarn(ErrRuntime_ParamsType).warn("tlist::iset", "Type Unsupported");

	if (params->get_type() == tint) {
		iset_int(params->get_v_tint(), v);
		update_first_ele_type();
	}
	if (params->get_type() == tcompo) {
		if (v.get_type() != tcompo)
			twarn(ErrRuntime_ParamsType).warn("tlist::iset", "Type Unsupported");
		if (params->get_v_tcompo()->get_compo_type_code() != compo_tpair
		|| v.get_v_tcompo()->get_compo_type_code()       != compo_tlist)
			twarn(ErrRuntime_ParamsType).warn("tlist::iset", "Type Unsupported");

		tpair * l1 = reinterpret_cast<tpair *>(params->get_v_tcompo());
		tlist * l2 = reinterpret_cast<tlist *>(v.get_v_tcompo());
		iset_pair(l1, l2);
		update_first_ele_type();
	}
}

void get_v_at_loc(tobj & vre)
{
	if (size() == 0) {
		vre.set_nil();
		return;
	}

	auto loc = cbegin() + (__idxi > 1 ? __idxi - 1 : 0);
	ttypes type = loc->get_type();

	switch (type) {
	case tint:
		vre.set_v(loc->get_v_tint());
		break;
	case tbool:
		vre.set_v(loc->get_v_tbool());
		break;
	case tdouble:
		vre.set_v(loc->get_v_tdouble());
		break;
	case tcompo:
		vre.set_v(loc->get_v_tcompo());
		break;
	default: break;
	}
}

bool next()
{
	__idxi++;
	uint32_t idxu = static_cast<uint32_t>(__idxi);

	if (idxu <= size())
		return 1;
	else {
		iter_restore();
		return 0;
	}
}

bool in(const tobj & e)
{
	for (auto iter = begin(); iter != end(); iter++)
		if (iter->identical(e) == true) return true;
	return false;
}

void iter_restore()
{
	__idxi = 0;
}

void set_append(const tobj * ele)
{
	if (ele->get_type() == tnil)
		twarn(ErrRuntime_AssignNil).warn("tlist::append", "");
	if (ele->get_type() == tcompo)
		ele->get_v_tcompo()->add_refctr();
	push_back(*ele);
	update_first_ele_type();
}

void set_append(const tobj & ele)
{
	if (ele.get_type() == tnil)
		twarn(ErrRuntime_AssignNil).warn("tlist::append", "");
	if (ele.get_type() == tcompo)
		ele.get_v_tcompo()->add_refctr();
	push_back(ele);
	update_first_ele_type();
}

void set_insert(const tobj * ele, const long loc)
{
	if (loc < 0 || static_cast<unsigned long>(loc) > size())
		twarn(ErrRuntime_IdxOutRange).warn("tlist::append", "");
	if (ele->get_type() == tnil)
		twarn(ErrRuntime_AssignNil).warn("tlist::append", "");
	if (ele->get_type() == tcompo)
		ele->get_v_tcompo()->add_refctr();

	insert(begin() + loc, *ele);
	update_first_ele_type();
}

void set_pop()
{
	if (size() == 0)
		twarn(ErrRuntime_RefEmptySet).warn("tlist::set_pop", "");
	tobj v = back();
	v.ddc_ref_clear();
	pop_back();
	update_first_ele_type();
}

void set_delete(const tobj * key)
{
	if (key->get_type() != tint)
		twarn(ErrRuntime_ParamsType).warn("tlist::set_delete", "");

	long loc = key->get_v_tint();

	if (loc < 0 || static_cast<unsigned long>(loc) >= size())
		twarn(ErrRuntime_IdxOutRange).warn("tlist::set_delete", "");

	tobj v = at(loc);
	v.ddc_ref_clear();
	erase(begin() + loc);
	update_first_ele_type();
}

bool identical(tcompo_v * v) const
{
	if (v->get_compo_type_code() != compo_tlist)
		return false;
	tlist * list = reinterpret_cast<tlist *>(v);

	if (list->len() != this->len())
		return false;
	auto it1 = list->begin();
	auto it2 = this->begin();

	for (; it1 != list->end(); it1++) {
		if (it1->identical(*it2) == false) return false;
		it2++;
	}
	return true;
}

};

/// Dict. Created by `{key:value, ...}`
class tdict : public tcompo_v, public std::unordered_map<std::string, tobj>
{
public:
tdict() {}

~tdict()
{
	for (iterator iter = begin(); iter != end(); iter++) {
		std::pair<std::string, tobj> p = *iter;
		p.second.ddc_ref_clear();
	}
}

std::string tostring_abbr() const
{
	return tostring_pointer(get_type(), this);
}

std::string tostring_full() const
{
	std::string is;
	is += "{\n";

	for (auto iter = cbegin(); iter != cend(); iter++) {
		std::pair<std::string, tobj> p = *iter;
		is += "\t\"" + p.first + "\" : ";
		is += p.second.tostring_abbr() + ",\n";
	}
	is += "}";
	return is;
}

tdict * copy()
{
	tdict * dict = new tdict();

	for (iterator iter = begin(); iter != end(); iter++) {
		std::pair<std::string, tobj> p = *iter;
		std::string idx = p.first;
		tobj v = p.second;
		(*dict)[idx] = v;
		if (v.get_type() == tcompo) v.get_v_tcompo()->add_refctr();
	}
	return dict;
}

const char * get_type() const
{
	return "Dictionary";
}

tcompo_type get_compo_type_code() const
{
	return compo_tdict;
}

long len() const
{
	return static_cast<long>(size());
}

/// tdict is uncomparable
bool identical(tcompo_v * v) const
{
	return v == this;
}

void idx(const tobj * params, uint_size_reg nparams, tobj & idxre)
{
	if (nparams != 1)
		twarn(ErrRuntime_ParamsCtr).warn("tdict::idx", "1 parameter");
	if (params->get_type() != tcompo)
		twarn(ErrRuntime_ParamsType).warn("tdict::idx", "Should be 'tstr'");
	if (params->get_v_tcompo()->get_compo_type_code() != compo_tstr)
		twarn(ErrRuntime_ParamsType).warn("tdict::idx", "Should be 'tstr'");

	tstr * str = reinterpret_cast<tstr *>(params->get_v_tcompo());
	iterator iter = find(*str);

	if (iter != end())
		idxre = iter->second;
	else
		idxre.set_nil();
}

void set(const std::string & key, const tobj & v)
{
	if (this->find(key) != this->end()) {
		if ((*this)[key].get_v_tcompo() == v.get_v_tcompo()) return;
		(*this)[key].ddc_ref_clear();
	}
	(*this)[key] = v;

	if (v.get_type() == tcompo)
		v.get_v_tcompo()->add_refctr();
}

void iset(const tobj * params, uint_size_reg nparams, const tobj & v)
{
	if (nparams != 1)
		twarn(ErrRuntime_ParamsCtr).warn("tdict::iset", "1 parameter");
	if (params->get_type() != tcompo)
		twarn(ErrRuntime_ParamsType).warn("tdict::iset", "Should be 'tstr'");
	if (params->get_v_tcompo()->get_compo_type_code() != compo_tstr)
		twarn(ErrRuntime_ParamsType).warn("tdict::iset", "Should be 'tstr'");
	tstr * str = reinterpret_cast<tstr *>(params->get_v_tcompo());
	set(*str, v);
}

void set_append(const tobj * ele)
{
	if (ele->get_type() != tcompo)
		twarn(ErrRuntime_ParamsType).warn("tdict::append", "Shoud be 'tpair'");
	if (ele->get_v_tcompo()->get_compo_type_code() != compo_tpair)
		twarn(ErrRuntime_ParamsType).warn("tdict::append", "Shoud be 'tpair'");

	tpair * pair = reinterpret_cast<tpair *>(ele->get_v_tcompo());
	tobj first = pair->get_first();
	tobj second = pair->get_second();
	iset(&first, 1, second);
}

void set_delete(const tobj * idx)
{
	if (idx->get_type() != tcompo)
		twarn(ErrRuntime_RefType).warn("tdict::pop", "Should be 'tstr'");
	if (idx->get_v_tcompo()->get_compo_type_code() != compo_tstr)
		twarn(ErrRuntime_RefType).warn("tdict::pop", "Should be 'tstr'");

	tstr * str = reinterpret_cast<tstr *>(idx->get_v_tcompo());
	iterator iter = find(*str);

	if (iter != end()) {
		iter->second.ddc_ref_clear(); erase(iter);
	}
}

/// keys are deep copy
tlist * keys()
{
	tlist * mykeys = new tlist();

	for (iterator iter = begin(); iter != end(); iter++) {
		tobj v(new tstr(iter->first));
		mykeys->set_append(&v);
	}
	return mykeys;
}

/// values are shallow copy
tlist * values()
{
	tlist * myvalues = new tlist();

	for (iterator iter = begin(); iter != end(); iter++) {
		std::pair<std::string, tobj> p = *iter;
		tobj value = p.second;
		myvalues->set_append(&value);
	}
	return myvalues;
}

/// add cpp function
void add_cppf(const char * fname, cppf f, uint_size_reg nparams)
{
	tobj v_cppf;
	v_cppf.set_v(new tcppgenf(f, fname, nparams));
	set(fname, v_cppf);
}

void add_obj(const char * fname, tobj o)
{
	set(fname, o);
}

};

class ttime : public tcompo_v, public top_sub
{
time_t __t;

public:
ttime()
{
	__t = time(nullptr);
}

ttime(time_t t)
{
	__t = t;
}

time_t get_time() const
{
	return __t;
}

std::string tostring_abbr() const
{
	return ctime(&__t);
}

/// Generate a detailed string (pure virtual)
std::string tostring_full() const
{
	return tostring_abbr();
}

/// Copy `this` (pure virtual)
tcompo_v * copy()
{
	return new ttime();
}

/// Generate a string of the type of `this` (pure virtual)
const char * get_type() const
{
	return "Time";
}

/// Get the length of `this` (pure virtual)
virtual long len() const
{
	return 0;
}

/// Get the type code of the composite object `this` (pure virtual)
tcompo_type get_compo_type_code() const
{
	return compo_time;
}

/// Returns a boolean of Is `this` identical to `v` (pure virtual)
bool identical(tcompo_v * v) const
{
	if (v->get_compo_type_code() != compo_time)
		twarn(ErrRuntime_ParamsType).warn("", "");
	ttime * time = reinterpret_cast<ttime *>(v);
	return __t == time->get_time();
}

void operator_sub(const tobj & v, tobj & vre)
{
	if (v.get_type() != tcompo)
		twarn(ErrRuntime_ParamsType).warn("ttime::operator_sub", "");
	if (v.get_v_tcompo()->get_compo_type_code() != compo_time)
		twarn(ErrRuntime_ParamsType).warn("ttime::operator_sub", "");
	ttime * p = reinterpret_cast<ttime *>(v.get_v_tcompo());
	time_t pt = p->get_time();
	if (__t < pt)
		twarn(ErrRuntime_Other).warn("ttime::operator_sub", "");
	vre.set_v(difftime(__t, pt));
}

void operator_rsub(const tobj & v, tobj & vre)
{
	if (v.get_type() != tcompo)
		twarn(ErrRuntime_ParamsType).warn("ttime::operator_sub", "");
	if (v.get_v_tcompo()->get_compo_type_code() != compo_time)
		twarn(ErrRuntime_ParamsType).warn("ttime::operator_sub", "");
	ttime * p = reinterpret_cast<ttime *>(v.get_v_tcompo());
	time_t pt = p->get_time();
	if (pt < __t)
		twarn(ErrRuntime_Other).warn("ttime::operator_sub", "");
	vre.set_v(difftime(pt, __t));
}

};



/// Transform string into boolean value
inline void str_to_bool(tobj * const params, uint_size_reg len, tobj & vre)
{
	if (len != 1)
		twarn(ErrRuntime_ParamsCtr).warn("str_to_bool", "");
	if (params->get_type() != tcompo)
		twarn(ErrRuntime_ParamsType).warn("str_to_bool", "");
	if (params->get_v_tcompo()->get_compo_type_code() != compo_tstr)
		twarn(ErrRuntime_ParamsType).warn("str_to_bool", "");
	vre.set_v(reinterpret_cast<tstr *>(params->get_v_tcompo())->to_bool());
}

/// Transform string into integer value
inline void str_to_int(tobj * const params, uint_size_reg len, tobj & vre)
{
	if (len != 1)
		twarn(ErrRuntime_ParamsCtr).warn("str_to_int", "");
	if (params->get_type() != tcompo)
		twarn(ErrRuntime_ParamsType).warn("str_to_int", "");
	if (params->get_v_tcompo()->get_compo_type_code() != compo_tstr)
		twarn(ErrRuntime_ParamsType).warn("str_to_int", "");
	vre.set_v(reinterpret_cast<tstr *>(params->get_v_tcompo())->to_int());
}

/// Transform string into double float value
inline void str_to_double(tobj * const params, uint_size_reg len, tobj & vre)
{
	if (len != 1)
		twarn(ErrRuntime_ParamsCtr).warn("str_to_double", "");
	if (params->get_type() != tcompo)
		twarn(ErrRuntime_ParamsType).warn("str_to_double", "");
	if (params->get_v_tcompo()->get_compo_type_code() != compo_tstr)
		twarn(ErrRuntime_ParamsType).warn("str_to_double", "");
	vre.set_v(reinterpret_cast<tstr *>(params->get_v_tcompo())->to_double());
}

/// tostr(value)
inline void to_str(tobj * const params, uint_size_reg len, tobj & vre)
{
	if (len != 1)
		twarn(ErrRuntime_ParamsCtr).warn("to_str", "");
	vre.set_v(new tstr(params->tostring_full()));
}

/// tolist(v1, v2, ...)
inline void to_list(tobj * const params, uint_size_reg len, tobj & vre)
{
	vre.set_v(new tlist(params, len));
}

/// append(set, ele)
inline void set_append(tobj * const params, uint_size_reg len, tobj & vre)
{
	vre.set_nil();

	if (len != 2)
		twarn(ErrRuntime_ParamsCtr).warn("set_append", "2 parameter");
	tobj * atom_des = params;
	tobj * atom_ele = params + 1;

	if (atom_des->get_type() != tcompo)
		twarn(ErrRuntime_ParamsType).warn("set_append", "");
	tcompo_v * p_des = params->get_v_tcompo();

	switch (p_des->get_compo_type_code()) {
	case compo_tdict:
		reinterpret_cast<tdict *>(p_des)->set_append(atom_ele);
		break;
	case compo_tlist:
		reinterpret_cast<tlist *>(p_des)->set_append(atom_ele);
		break;
	case compo_tstr:
		reinterpret_cast<tstr *>(p_des)->set_append(atom_ele);
		break;
	default:
		twarn(ErrRuntime_ParamsType).warn("set_append", "");
		break;
	}
}

/// append(set, ele, loc)
inline void set_insert(tobj * const params, uint_size_reg len, tobj & vre)
{
	vre.set_nil();

	if (len != 3)
		twarn(ErrRuntime_ParamsCtr).warn("set_insert", "3 parameters");
	tobj * atom_des = params;
	tobj * atom_ele = params + 1;
	tobj * atom_idx = params + 2;

	if (atom_des->get_type() != tcompo
	|| atom_idx->get_type() != tint)
		twarn(ErrRuntime_ParamsType).warn("set_insert", "");

	tcompo_v * p_des = params->get_v_tcompo();
	long idx = atom_idx->get_v_tint();

	switch (p_des->get_compo_type_code()) {
	case compo_tlist:
		reinterpret_cast<tlist *>(p_des)->set_insert(atom_ele, idx);
		break;
	case compo_tstr:
		reinterpret_cast<tstr *>(p_des)->set_insert(atom_ele, idx);
		break;
	default:
		twarn(ErrRuntime_ParamsType).warn("set_insert", "");
		break;
	}
}

/// pop(set)
inline void set_pop(tobj * const params, uint_size_reg len, tobj & vre)
{
	vre.set_nil();

	if (len != 1)
		twarn(ErrRuntime_ParamsCtr).warn("set_pop", "1 parameter");
	if (params->get_type() != tcompo)
		twarn(ErrRuntime_ParamsType).warn("set_pop", "");
	tcompo_v * v = params->get_v_tcompo();

	switch (v->get_compo_type_code()) {
	case compo_tlist:
		reinterpret_cast<tlist *>(v)->set_pop();
		break;
	case compo_tstr:
		reinterpret_cast<tstr *>(v)->set_pop();
		break;
	default:
		twarn(ErrRuntime_ParamsType).warn("set_pop", "");
		break;
	}
}

/// delete(set, loc)
inline void set_delete(tobj * const params, uint_size_reg len, tobj & vre)
{
	vre.set_nil();

	if (len != 2)
		twarn(ErrRuntime_ParamsCtr).warn("set_delete", "2 parameters");

	tobj * atom_obj = params;
	tobj * atom_idx = params + 1;

	if (atom_obj->get_type() != tcompo)
		twarn(ErrRuntime_ParamsType).warn("set_delete", "");
	tcompo_v * v = atom_obj->get_v_tcompo();

	switch (v->get_compo_type_code())
	{
	case compo_tlist:
		reinterpret_cast<tlist *>(v)->set_delete(atom_idx);
		break;
	case compo_tstr:
		reinterpret_cast<tstr *>(v)->set_delete(atom_idx);
		break;
	case compo_tdict:
		reinterpret_cast<tdict *>(v)->set_delete(atom_idx);
		break;
	default:
		twarn(ErrRuntime_ParamsType).warn("set_delete", "");
		break;
	}
}

/// union(set1, set2)
inline void set_union(tobj * const params, uint_size_reg len, tobj & vre)
{
	if (len != 2)
		twarn(ErrRuntime_ParamsCtr).warn("set_union", "2 parameters");
	tobj * p1 = params;
	tobj * p2 = params + 1;

	if (p1->get_type() != tcompo || p2->get_type() != tcompo)
		twarn(ErrRuntime_ParamsType).warn("gen_cppfs::setunion", "");
	tcompo_v * v1 = p1->get_v_tcompo();
	tcompo_v * v2 = p2->get_v_tcompo();

	if (v1->get_compo_type_code() == compo_tlist && v2->get_compo_type_code() == compo_tlist) {
		tlist * li_1 = reinterpret_cast<tlist *>(v1);
		tlist * li_2 = reinterpret_cast<tlist *>(v2);
		tlist * li_new = new tlist();

		for (auto iter = li_1->begin(); iter != li_1->end(); iter++) {
			tobj v = *iter;
			li_new->set_append(&v);
			if (v.get_type() == tcompo) v.get_v_tcompo()->add_refctr();
		}
		for (auto iter = li_2->begin(); iter != li_2->end(); iter++) {
			tobj v = *iter;
			li_new->set_append(&v);
			if (v.get_type() == tcompo) v.get_v_tcompo()->add_refctr();
		}
		vre.set_v(li_new);
	}

	if (v1->get_compo_type_code() == compo_tdict && v2->get_compo_type_code() == compo_tdict) {
		tdict * dict_1 = reinterpret_cast<tdict *>(v1);
		tdict * dict_2 = reinterpret_cast<tdict *>(v2);
		tdict * dict_new = new tdict();

		for (auto iter = dict_1->begin(); iter != dict_1->end(); iter++) {
			std::pair<std::string, tobj> e = *iter;
			(*dict_new)[e.first] = e.second;
			if (e.second.get_type() == tcompo)
				e.second.get_v_tcompo()->add_refctr();
		}
		for (auto iter = dict_2->begin(); iter != dict_2->end(); iter++) {
			std::pair<std::string, tobj> p = *iter;
			(*dict_new)[p.first] = p.second;
			if (p.second.get_type() == tcompo)
				p.second.get_v_tcompo()->add_refctr();
		}
		vre.set_v(dict_new);
	}
}

/// keys(tdict): Get a string list of tdict keys.
inline void dict_keys(tobj* const params, uint_size_reg len, tobj& vre)
{
	if (len != 1)
		twarn(ErrRuntime_ParamsCtr).warn("dict_keys", "");
	if (params->get_type() != tcompo)
		twarn(ErrRuntime_ParamsType).warn("dict_keys", "");
	if (params->get_v_tcompo()->get_compo_type_code() != compo_tdict)
		twarn(ErrRuntime_ParamsType).warn("dict_keys", "");

	tdict* dict = reinterpret_cast<tdict *>(params->get_v_tcompo());
	vre.set_v(dict->keys());
}

/// values(dict): Get a list of tdict values.
inline void dict_values(tobj* const params, uint_size_reg len, tobj& vre)
{
	if (len != 1)
		twarn(ErrRuntime_ParamsCtr).warn("dict_values", "");
	if (params->get_type() != tcompo)
		twarn(ErrRuntime_ParamsType).warn("dict_values", "");
	if (params->get_v_tcompo()->get_compo_type_code() != compo_tdict)
		twarn(ErrRuntime_ParamsType).warn("dict_values", "");

	tdict* dict = reinterpret_cast<tdict *>(params->get_v_tcompo());
	vre.set_v(dict->values());
}

/// now()
inline void time_now(tobj* const params, uint_size_reg len, tobj& vre)
{
	if (len != 0 || params->get_type() != tcompo)
		twarn(ErrRuntime_ParamsCtr).warn("time_now", "");
	vre.set_v(new ttime);
}

}

#endif // TSTD_H
