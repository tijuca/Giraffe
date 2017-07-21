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

#ifndef traits_INCLUDED
#define traits_INCLUDED

#include <kopano/zcdefs.h>
#include <string>
#include <cstring>

namespace KC {

template<typename _Type> class iconv_charset _kc_final {
};

#define CHARSET_CHAR "//TRANSLIT"
#define CHARSET_WCHAR "UTF-32LE"
#define CHARSET_TCHAR (iconv_charset<TCHAR*>::name())

extern _kc_export void setCharsetBestAttempt(std::string &);

// Multibyte character specializations
template<> class iconv_charset<std::string> _kc_final {
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

template<> class iconv_charset<char *> _kc_final {
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

template<> class iconv_charset<const char *> _kc_final {
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

template<size_t _N> class iconv_charset<char[_N]> _kc_final {
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

template<size_t _N> class iconv_charset<const char[_N]> _kc_final {
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
template<> class iconv_charset<std::wstring> _kc_final {
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

template<> class iconv_charset<wchar_t *> _kc_final {
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

template<> class iconv_charset<const wchar_t *> _kc_final {
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

template<size_t _N> class iconv_charset<wchar_t[_N]> _kc_final {
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

template<size_t _N> class iconv_charset<const wchar_t[_N]> _kc_final {
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

template<> class iconv_charset<std::u16string> _kc_final {
public:
	static const char *name() {
		return "UTF-16LE";
	}
	static const char *rawptr(const std::u16string &from)
	{
		return reinterpret_cast<const char *>(from.c_str());
	}
	static size_t rawsize(const std::u16string &from)
	{
		return from.size() * sizeof(std::u16string::value_type);
	}
};

template<typename _Type>
size_t rawsize(const _Type &_x) {
	return iconv_charset<_Type>::rawsize(_x);
}

} /* namespace */

#endif // ndef traits_INCLUDED
