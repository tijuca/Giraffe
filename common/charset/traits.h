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

#ifndef traits_INCLUDED
#define traits_INCLUDED

#include <string>
#include <string.h>

template <typename _Type>
class iconv_charset {
};

#define CHARSET_CHAR "//TRANSLIT"
#define CHARSET_WCHAR "UTF-32LE"

#define CHARSET_TCHAR (iconv_charset<TCHAR*>::name())

void setCharsetBestAttempt(std::string &strCharset);

// Multibyte character specializations
template <>
class iconv_charset<std::string> {
public:
	static const char *name() {
		return CHARSET_CHAR;	// Current locale
	}
	static const char *rawptr(const std::string &from) {
		return from.c_str();
	}
	static size_t rawsize(const std::string &from) {
		return from.size();
	}
};

template <>
class iconv_charset<char*> {
public:
	static const char *name() {
		return CHARSET_CHAR;	// Current locale
	}
	static const char *rawptr(const char *from) {
		return from;
	}
	static size_t rawsize(const char *from) {
		return strlen(from);
	}
};

template <>
class iconv_charset<const char*> {
public:
	static const char *name() {
		return CHARSET_CHAR;	// Current locale
	}
	static const char *rawptr(const char *from) {
		return from;
	}
	static size_t rawsize(const char *from) {
		return strlen(from);
	}
};

template <size_t _N>
class iconv_charset<char [_N]> {
public:
	static const char *name() {
		return CHARSET_CHAR;	// Current locale
	}
	static const char *rawptr(const char (&from) [_N]) {
		return from;
	}
	static size_t rawsize(const char (&) [_N]) {
		return _N - 1;
	}
};

template <size_t _N>
class iconv_charset<const char [_N]> {
public:
	static const char *name() {
		return CHARSET_CHAR;	// Current locale
	}
	static const char *rawptr(const char (&from) [_N]) {
		return from;
	}
	static size_t rawsize(const char (&) [_N]) {
		return _N - 1;
	}
};


// Wide character specializations
template <>
class iconv_charset<std::wstring> {
public:
	static const char *name() {
		return CHARSET_WCHAR;
	}
	static const char *rawptr(const std::wstring &from) {
		return reinterpret_cast<const char*>(from.c_str());
	}
	static size_t rawsize(const std::wstring &from) {
		return from.size() * sizeof(std::wstring::value_type);
	}
};

template <>
class iconv_charset<wchar_t*> {
public:
	static const char *name() {
		return CHARSET_WCHAR;
	}
	static const char *rawptr(const wchar_t *from) {
		return reinterpret_cast<const char*>(from);
	}
	static size_t rawsize(const wchar_t *from) {
		return wcslen(from) * sizeof(wchar_t);
	}
};

template <>
class iconv_charset<const wchar_t*> {
public:
	static const char *name() {
		return CHARSET_WCHAR;
	}
	static const char *rawptr(const wchar_t *from) {
		return reinterpret_cast<const char*>(from);
	}
	static size_t rawsize(const wchar_t *from) {
		return wcslen(from) * sizeof(wchar_t);
	}
};

template <size_t _N>
class iconv_charset<wchar_t [_N]> {
public:
	static const char *name() {
		return CHARSET_WCHAR;	// Current locale
	}
	static const char *rawptr(const wchar_t (&from) [_N]) {
		return reinterpret_cast<const char*>(from);
	}
	static size_t rawsize(const wchar_t (&) [_N]) {
		return (_N - 1) * sizeof(wchar_t);
	}
};

template <size_t _N>
class iconv_charset<const wchar_t [_N]> {
public:
	static const char *name() {
		return CHARSET_WCHAR;	// Current locale
	}
	static const char *rawptr(const wchar_t (&from) [_N]) {
		return reinterpret_cast<const char*>(from);
	}
	static size_t rawsize(const wchar_t (&) [_N]) {
		return (_N - 1) * sizeof(wchar_t);
	}
};


template<typename _Type>
size_t rawsize(const _Type &_x) {
	return iconv_charset<_Type>::rawsize(_x);
}

#endif // ndef traits_INCLUDED
