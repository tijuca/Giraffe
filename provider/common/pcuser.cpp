/*
 * Copyright 2005 - 2016 Zarafa and its licensors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License, version 3,
 * as published by the Free Software Foundation.
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

#include <kopano/platform.h>

#include <kopano/stringutil.h>
#include <kopano/pcuser.hpp>

#include <sstream>

namespace KC {

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

bool objectid_t::operator==(const objectid_t &x) const noexcept
{
	return this->objclass == x.objclass && this->id == x.id;
}

bool objectid_t::operator!=(const objectid_t &x) const noexcept
{
	return this->objclass != x.objclass || this->id != x.id;
}

std::string objectid_t::tostring() const
{
	return stringify(this->objclass) + ";" + bin2hex(this->id);
}

unsigned int objectdetails_t::GetPropInt(property_key_t propname) const
{
	property_map::const_iterator item = m_mapProps.find(propname);
	return item == m_mapProps.cend() ? 0 : atoi(item->second.c_str());
}

bool objectdetails_t::GetPropBool(property_key_t propname) const
{
	property_map::const_iterator item = m_mapProps.find(propname);
	return item == m_mapProps.cend() ? false : atoi(item->second.c_str());
}

std::string objectdetails_t::GetPropString(property_key_t propname) const
{
	property_map::const_iterator item = m_mapProps.find(propname);
	return item == m_mapProps.cend() ? std::string() : item->second;
}

objectid_t objectdetails_t::GetPropObject(property_key_t propname) const
{
	property_map::const_iterator item = m_mapProps.find(propname);
	return item == m_mapProps.cend() ? objectid_t() : objectid_t(item->second);
}

void objectdetails_t::SetPropInt(property_key_t propname, unsigned int value)
{
    m_mapProps[propname].assign(stringify(value));
}

void objectdetails_t::SetPropBool(property_key_t propname, bool value)
{
    m_mapProps[propname].assign(value ? "1" : "0");
}

void objectdetails_t::SetPropString(property_key_t propname,
    const std::string &value)
{
    m_mapProps[propname].assign(value);
}

void objectdetails_t::SetPropListString(property_key_t propname,
    const std::list<std::string> &value)
{
	m_mapMVProps[propname].assign(value.begin(), value.end());
}

void objectdetails_t::SetPropObject(property_key_t propname,
    const objectid_t &value)
{
	m_mapProps[propname].assign(((objectid_t)value).tostring());
}

void objectdetails_t::AddPropInt(property_key_t propname, unsigned int value)
{
	m_mapMVProps[propname].emplace_back(stringify(value));
}

void objectdetails_t::AddPropString(property_key_t propname,
    const std::string &value)
{
	m_mapMVProps[propname].emplace_back(value);
}

void objectdetails_t::AddPropObject(property_key_t propname,
    const objectid_t &value)
{
	m_mapMVProps[propname].emplace_back(value.tostring());
}

std::list<unsigned int>
objectdetails_t::GetPropListInt(property_key_t propname) const
{
	property_mv_map::const_iterator mvitem = m_mapMVProps.find(propname);
	if (mvitem == m_mapMVProps.cend())
		return std::list<unsigned int>();
	std::list<unsigned int> l;
	for (const auto &i : mvitem->second)
		l.emplace_back(atoui(i.c_str()));
	return l;
}

std::list<std::string>
objectdetails_t::GetPropListString(property_key_t propname) const
{
	property_mv_map::const_iterator mvitem = m_mapMVProps.find(propname);
	if (mvitem != m_mapMVProps.cend())
		return mvitem->second;
	return std::list<std::string>();
}

std::list<objectid_t>
objectdetails_t::GetPropListObject(property_key_t propname) const
{
	property_mv_map::const_iterator mvitem = m_mapMVProps.find(propname);
	if (mvitem == m_mapMVProps.cend())
		return std::list<objectid_t>();
	std::list<objectid_t> l;
	for (const auto &i : mvitem->second)
		l.emplace_back(i);
	return l;
}

property_map objectdetails_t::GetPropMapAnonymous() const {
	property_map anonymous;

	for (const auto &iter : m_mapProps)
		if (((unsigned int)iter.first) & 0xffff0000)
			anonymous.emplace(iter);
	return anonymous;
}

property_mv_map objectdetails_t::GetPropMapListAnonymous() const {
	property_mv_map anonymous;
	for (const auto &iter : m_mapMVProps)
		if (((unsigned int)iter.first) & 0xffff0000)
			anonymous.emplace(iter);
	return anonymous;
}

bool objectdetails_t::HasProp(property_key_t propname) const
{
	return m_mapProps.find(propname) != m_mapProps.end() || m_mapMVProps.find(propname) != m_mapMVProps.end();
}

bool objectdetails_t::PropListStringContains(property_key_t propname,
    const std::string &value, bool ignoreCase) const
{
	const std::list<std::string> list = GetPropListString(propname);
	if (ignoreCase)
		return std::find_if(list.begin(), list.end(),
			[&](const std::string &o) {
				return value.size() == o.size() && strcasecmp(value.c_str(), o.c_str()) == 0;
			}) != list.end();
	return std::find_if(list.begin(), list.end(),
		[&](const std::string &o) {
			return value.size() == o.size() && strcmp(value.c_str(), o.c_str()) == 0;
		}) != list.end();
}

void objectdetails_t::ClearPropList(property_key_t propname)
{
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
	assert(this->m_objclass == from.m_objclass);
	for (const auto &p : from.m_mapProps)
		this->m_mapProps[p.first].assign(p.second);
	for (const auto &p : from.m_mapMVProps)
		this->m_mapMVProps[p.first].assign(p.second.cbegin(), p.second.cend());
}

/**
 * Get the size of this object
 *
 * @return Memory usage of this object in bytes
 */
size_t objectdetails_t::GetObjectSize(void) const
{
	size_t ulSize = sizeof(*this);

	ulSize += MEMORY_USAGE_MAP(m_mapProps.size(), property_map);
	for (const auto &p : m_mapProps)
		ulSize += MEMORY_USAGE_STRING(p.second);

	ulSize += MEMORY_USAGE_MAP(m_mapMVProps.size(), property_mv_map);
	for (const auto &p : m_mapMVProps)
		for (const auto &s : p.second)
			ulSize += MEMORY_USAGE_STRING(s);
	return ulSize;
}

std::string objectdetails_t::ToStr(void) const
{
	std::string str;

	str = "propmap: ";
	for (auto i = m_mapProps.cbegin(); i != m_mapProps.cend(); ++i) {
		if (i != m_mapProps.cbegin())
			str += ", ";
		str+= stringify(i->first) + "='";
		str+= i->second + "'";
	}

	str += " mvpropmap: ";
	for (auto mvi = m_mapMVProps.cbegin();
	     mvi != m_mapMVProps.cend(); ++mvi) {
		if (mvi != m_mapMVProps.begin())
			str += ", ";
		str += stringify(mvi->first) + "=(";
		for (auto istr = mvi->second.cbegin();
		     istr != mvi->second.cend(); ++istr) {
			if (istr != mvi->second.cbegin())
				str += ", ";
			str += *istr;
		}
		str +=")";
	}

	return str;
}

serverdetails_t::serverdetails_t(const std::string &servername)
: m_strServerName(servername)
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

void serverdetails_t::SetProxyPath(const std::string &proxy) {
	m_strProxyPath = proxy;
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
		oss << "http://" << m_strHostAddress << ":" << m_ulHttpPort << "/";
		return oss.str();
	}
	return std::string();	
}

std::string serverdetails_t::GetSslPath() const {
	if (!m_strHostAddress.empty() && m_ulSslPort > 0) {
		std::ostringstream oss;
		oss << "https://" << m_strHostAddress << ":" << m_ulSslPort << "/";
		return oss.str();
	}
	return std::string();	
}

const std::string &serverdetails_t::GetProxyPath() const {
	return m_strProxyPath;
}

} /* namespace */
