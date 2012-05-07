/*
 * Copyright 2005 - 2012  Zarafa B.V.
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License, version 3, 
 * as published by the Free Software Foundation with the following additional 
 * term according to sec. 7:
 *  
 * According to sec. 7 of the GNU Affero General Public License, version
 * 3, the terms of the AGPL are supplemented with the following terms:
 * 
 * "Zarafa" is a registered trademark of Zarafa B.V. The licensing of
 * the Program under the AGPL does not imply a trademark license.
 * Therefore any rights, title and interest in our trademarks remain
 * entirely with us.
 * 
 * However, if you propagate an unmodified version of the Program you are
 * allowed to use the term "Zarafa" to indicate that you distribute the
 * Program. Furthermore you may use our trademarks where it is necessary
 * to indicate the intended purpose of a product or service provided you
 * use it in accordance with honest practices in industrial or commercial
 * matters.  If you want to propagate modified versions of the Program
 * under the name "Zarafa" or "Zarafa Server", you may only do so if you
 * have a written permission by Zarafa B.V. (to acquire a permission
 * please contact Zarafa at trademark@zarafa.com).
 * 
 * The interactive user interface of the software displays an attribution
 * notice containing the term "Zarafa" and/or the logo of Zarafa.
 * Interactive user interfaces of unmodified and modified versions must
 * display Appropriate Legal Notices according to sec. 5 of the GNU
 * Affero General Public License, version 3, when you propagate
 * unmodified or modified versions of the Program. In accordance with
 * sec. 7 b) of the GNU Affero General Public License, version 3, these
 * Appropriate Legal Notices must retain the logo of Zarafa or display
 * the words "Initial Development by Zarafa" if the display of the logo
 * is not reasonably feasible for technical reasons. The use of the logo
 * of Zarafa in Legal Notices is allowed for unmodified and modified
 * versions of the software.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *  
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * 
 */

#include "platform.h"

#include "stringutil.h"
#include "ZarafaUser.h"

#include <sstream>

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

template<int(*fnCmp)(const char*, const char*)>
class StringComparer {
public:
	StringComparer(const std::string &str): m_str(str) {}
	bool operator()(const std::string &other) {
		return m_str.size() == other.size() && fnCmp(m_str.c_str(), other.c_str()) == 0;
	}

private:
	const std::string &m_str;
};

objectid_t::objectid_t(const std::string &id, objectclass_t objclass)
{
	this->id = id;
	this->objclass = objclass;
}

objectid_t::objectid_t(const std::string &str)
{
	std::string objclass;
	std::string objid;
	size_t pos;

	// sendas users are "encoded" like this in a string
	pos = str.find_first_of(';');
	if (pos == std::string::npos) {
		this->id = hex2bin(str);
		this->objclass = ACTIVE_USER;
	} else {
		objid.assign(str, pos + 1, str.size() - pos);
		objclass.assign(str, 0, pos);
		this->id = hex2bin(objid);
		this->objclass = (objectclass_t)atoi(objclass.c_str());
	}
}

objectid_t::objectid_t(objectclass_t objclass)
{
	this->objclass = objclass;
}

objectid_t::objectid_t()
{
	objclass = OBJECTCLASS_UNKNOWN;
}

bool objectid_t::operator==(const objectid_t &x)
{
	return this->objclass == x.objclass && this->id == x.id;
}

bool objectid_t::operator!=(const objectid_t &x)
{
	return this->objclass != x.objclass || this->id != x.id;
}

std::string objectid_t::tostring() const
{
	return stringify(this->objclass) + ";" + bin2hex(this->id);
}

objectdetails_t::objectdetails_t(objectclass_t objclass) : m_objclass(objclass) {}
objectdetails_t::objectdetails_t() : m_objclass(OBJECTCLASS_UNKNOWN) {}

objectdetails_t::objectdetails_t(const objectdetails_t &objdetails) {
	m_objclass = objdetails.m_objclass;
	m_mapProps = objdetails.m_mapProps;
	m_mapMVProps = objdetails.m_mapMVProps;
}

unsigned int objectdetails_t::GetPropInt(const property_key_t &propname) const {
	property_map::const_iterator item = m_mapProps.find(propname);
    if(item != m_mapProps.end()) {
        return atoi(item->second.c_str());
    } else return 0;
}

bool objectdetails_t::GetPropBool(const property_key_t &propname) const {
	property_map::const_iterator item = m_mapProps.find(propname);
    if(item != m_mapProps.end()) {
        return atoi(item->second.c_str());
    } else return false;
}

std::string	objectdetails_t::GetPropString(const property_key_t &propname) const {
	property_map::const_iterator item = m_mapProps.find(propname);
    if(item != m_mapProps.end()) {
        return item->second;
    } else return std::string();
}

objectid_t objectdetails_t::GetPropObject(const property_key_t &propname) const {
	property_map::const_iterator item = m_mapProps.find(propname);
	if (item != m_mapProps.end()) {
		return objectid_t(item->second);
	} else return objectid_t();
}

void objectdetails_t::SetPropInt(const property_key_t &propname, unsigned int value) {
    m_mapProps[propname].assign(stringify(value));
}

void objectdetails_t::SetPropBool(const property_key_t &propname, bool value) {
    m_mapProps[propname].assign(value ? "1" : "0");
}

void objectdetails_t::SetPropString(const property_key_t &propname, const std::string &value) {
    m_mapProps[propname].assign(value);
}

void objectdetails_t::SetPropListString(const property_key_t &propname, const std::list<std::string> &value) {
	m_mapMVProps[propname].assign(value.begin(), value.end());
}

void objectdetails_t::SetPropObject(const property_key_t &propname, const objectid_t &value) {
	m_mapProps[propname].assign(((objectid_t)value).tostring());
}

void objectdetails_t::AddPropInt(const property_key_t &propname, unsigned int value) {
	m_mapMVProps[propname].push_back(stringify(value));
}

void objectdetails_t::AddPropString(const property_key_t &propname, const std::string &value) {
	m_mapMVProps[propname].push_back(value);
}

void objectdetails_t::AddPropObject(const property_key_t &propname, const objectid_t &value) {
	m_mapMVProps[propname].push_back(((objectid_t)value).tostring());
}

std::list<unsigned int> objectdetails_t::GetPropListInt(const property_key_t &propname) const {
	property_mv_map::const_iterator mvitem = m_mapMVProps.find(propname);
	if (mvitem != m_mapMVProps.end()) {
		std::list<unsigned int> l;
		for (std::list<std::string>::const_iterator i = mvitem->second.begin(); i != mvitem->second.end(); i++)
			l.push_back(atoui(i->c_str()));
		return l;
	} else return std::list<unsigned int>();
}

std::list<std::string> objectdetails_t::GetPropListString(const property_key_t &propname) const {
	property_mv_map::const_iterator mvitem = m_mapMVProps.find(propname);
	if (mvitem != m_mapMVProps.end()) return mvitem->second;
	else return std::list<std::string>();
}

std::list<objectid_t> objectdetails_t::GetPropListObject(const property_key_t &propname) const {
	property_mv_map::const_iterator mvitem = m_mapMVProps.find(propname);
	if (mvitem != m_mapMVProps.end()) {
		std::list<objectid_t> l;
		for (std::list<std::string>::const_iterator i = mvitem->second.begin(); i != mvitem->second.end(); i++)
			l.push_back(objectid_t(*i));
		return l;
	} else return std::list<objectid_t>();
}

property_map objectdetails_t::GetPropMapAnonymous() const {
	property_map anonymous;
	property_map::const_iterator iter;

	for (iter = m_mapProps.begin(); iter != m_mapProps.end(); iter++) {
		if (((unsigned int)iter->first) & 0xffff0000)
			anonymous.insert(*iter);
	}

	return anonymous;
}

property_mv_map objectdetails_t::GetPropMapListAnonymous() const {
	property_mv_map anonymous;
	property_mv_map::const_iterator iter;

	for (iter = m_mapMVProps.begin(); iter != m_mapMVProps.end(); iter++) {
		if (((unsigned int)iter->first) & 0xffff0000)
			anonymous.insert(*iter);
	}

	return anonymous;
}

bool objectdetails_t::PropListStringContains(const property_key_t &propname, const std::string &value, bool ignoreCase) const {
	const std::list<std::string> list = GetPropListString(propname);
	if (ignoreCase)
		return std::find_if(list.begin(), list.end(), StringComparer<stricmp>(value)) != list.end();
	return std::find_if(list.begin(), list.end(), StringComparer<strcmp>(value)) != list.end();
}

void objectdetails_t::ClearPropList(const property_key_t &propname) {
	m_mapMVProps[propname].clear();
}

void objectdetails_t::SetClass(objectclass_t objclass)
{
	m_objclass = objclass;
}

objectclass_t objectdetails_t::GetClass() const {
    return m_objclass;
}

void objectdetails_t::MergeFrom(const objectdetails_t &from) {
	property_map::const_iterator i, fi;
	property_mv_map::const_iterator mvi, fmvi;

	ASSERT(this->m_objclass == from.m_objclass);

	for (fi = from.m_mapProps.begin(); fi != from.m_mapProps.end(); fi++)
		this->m_mapProps[fi->first].assign(fi->second);

	for (fmvi = from.m_mapMVProps.begin(); fmvi != from.m_mapMVProps.end(); fmvi++)
		this->m_mapMVProps[fmvi->first].assign(fmvi->second.begin(), fmvi->second.end());
}

/**
 * Get the size of this object
 *
 * @return Memory usage of this object in bytes
 */
unsigned int objectdetails_t::GetObjectSize()
{
	unsigned int ulSize = sizeof(*this);
	property_map::const_iterator i;
	property_mv_map::iterator mvi;
	std::list<std::string>::iterator istr;

	ulSize += sizeof(property_map::value_type) * m_mapProps.size();
	for (i = m_mapProps.begin(); i != m_mapProps.end(); i++)
		ulSize += i->second.size();

	ulSize += sizeof(property_mv_map::value_type) * m_mapMVProps.size();

	for (mvi = m_mapMVProps.begin(); mvi != m_mapMVProps.end(); mvi++) {
		ulSize += mvi->second.size() * sizeof(std::string);
		for (istr = mvi->second.begin(); istr != mvi->second.end(); istr++)
			ulSize += (*istr).size();
	}

	return ulSize;
}

serverdetails_t::serverdetails_t(const std::string &servername)
: m_strServerName(servername)
, m_ulHttpPort(0)
, m_ulSslPort(0)
{ }

void serverdetails_t::SetHostAddress(const std::string &hostaddress) {
	m_strHostAddress = hostaddress;
}

void serverdetails_t::SetFilePath(const std::string &filepath) {
	m_strFilePath = filepath;
}

void serverdetails_t::SetHttpPort(unsigned port) {
	m_ulHttpPort = port;
}

void serverdetails_t::SetSslPort(unsigned port) {
	m_ulSslPort = port;
}

const std::string& serverdetails_t::GetServerName() const {
	return m_strServerName;
}

const std::string& serverdetails_t::GetHostAddress() const {
	return m_strHostAddress;
}

unsigned serverdetails_t::GetHttpPort() const {
	return m_ulHttpPort;
}

unsigned serverdetails_t::GetSslPort() const {
	return m_ulSslPort;
}

std::string serverdetails_t::GetFilePath() const {
	if (!m_strFilePath.empty())
		return "file://"+m_strFilePath;
	return std::string();
}

std::string serverdetails_t::GetHttpPath() const {
	if (!m_strHostAddress.empty() && m_ulHttpPort > 0) {
		std::ostringstream oss;
		oss << "http://" << m_strHostAddress << ":" << m_ulHttpPort << "/zarafa";
		return oss.str();
	}
	return std::string();	
}

std::string serverdetails_t::GetSslPath() const {
	if (!m_strHostAddress.empty() && m_ulSslPort > 0) {
		std::ostringstream oss;
		oss << "https://" << m_strHostAddress << ":" << m_ulSslPort << "/zarafa";
		return oss.str();
	}
	return std::string();	
}
