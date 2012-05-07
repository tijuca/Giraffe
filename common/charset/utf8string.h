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

#ifndef utf8string_INCLUDED
#define utf8string_INCLUDED

#include <string>
#include <stdexcept>

#include "traits.h"

/**
 * @brief	This class represents an UTF-8 string.
 *
 * This class does not expose the same methods as STL's std::string as most of those don't make
 * much sense.
 */
class utf8string 
{
public:
	typedef std::string::value_type		value_type;
	typedef std::string::const_pointer	const_pointer;
	typedef std::string::size_type		size_type;
	
	static utf8string from_string(const std::string &str) {
		utf8string s;
		s.m_str.assign(str);
		return s;
	}

	static utf8string null_string() {
		utf8string s;
		s.set_null();
		return s;
	}

	utf8string(): m_bNull(false) {}
	utf8string(const utf8string &other): m_bNull(other.m_bNull), m_str(other.m_str) {}
	utf8string(size_t n, char c): m_bNull(false), m_str(n, c) {}
	
	utf8string &operator=(const utf8string &other) {
		if (this != &other) {
			m_bNull = other.m_bNull;
			m_str = other.m_str;
		}
			
		return *this;
	}

	const_pointer c_str() const {
		return m_bNull ? NULL : m_str.c_str();
	}

	const_pointer data() const {
		return m_bNull ? NULL : m_str.data();
	}
	
	const std::string &str() const {
		return m_str;
	}

	size_type size() const {
		return m_str.size();
	}
	
	bool empty() const {
		return m_str.empty();
	}

	size_type length() const {
		return m_str.length();
	}
	
	void append(const_pointer lpData, size_t cbData) {
		m_str.append(lpData, cbData);
		m_bNull = false;
	}

	void append(const utf8string &str) {
		m_str.append(str.m_str);
		m_bNull = false;
	}
	
	void clear() {
		m_str.clear();
	}

	void set_null() {
		clear();
		m_bNull = true;
	}
	
private:
	bool m_bNull;
	std::string	m_str;
};

template <>
class iconv_charset<utf8string> {
public:
	static const char *name() {
		return "UTF-8";
	}
	static const char *rawptr(const utf8string &from) {
		return reinterpret_cast<const char*>(from.c_str());
	}
	static size_t rawsize(const utf8string &from) {
		return from.size();
	}
};


#endif //ndef utf8string_INCLUDED
