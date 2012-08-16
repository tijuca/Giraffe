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

#ifndef _STRINGUTIL_H
#define _STRINGUTIL_H

#include <string>
#include <vector>
#include <algorithm>

/*
 * Comparison handler for case-insensitive keys in maps
 */
struct stricmp_comparison {
	bool operator()(const std::string &left, const std::string &right) const
	{
		return ((left.size() < right.size()) || ((left.size() == right.size()) && (stricmp(left.c_str(), right.c_str()) < 0)));
	}
};

struct wcscasecmp_comparison {
	bool operator()(const std::wstring &left, const std::wstring &right) const
	{
		return ((left.size() < right.size()) || ((left.size() == right.size()) && (wcscasecmp(left.c_str(), right.c_str()) < 0)));
	}
};

static inline std::string strToUpper(std::string f) {
	transform(f.begin(), f.end(), f.begin(), ::toupper);
	return f;
}

static inline std::string strToLower(std::string f) {
	transform(f.begin(), f.end(), f.begin(), ::tolower);
	return f;
}

// Use casting if passing hard coded values.
std::string stringify(unsigned int x, bool usehex = false, bool _signed = false);
std::string stringify_int64(long long x, bool usehex = false);
std::string stringify_float(float x);
std::string stringify_double(double x, int prec = 18, bool bLocale = false);
std::string stringify_datetime(time_t x);

std::wstring wstringify(unsigned int x, bool usehex = false, bool _signed = false);
std::wstring wstringify_int64(long long x, bool usehex = false);
std::wstring wstringify_uint64(unsigned long long x, bool usehex = false);
std::wstring wstringify_float(float x);
std::wstring wstringify_double(double x, int prec = 18);

#ifdef UNICODE
	#define tstringify			wstringify
	#define tstringify_int64	wstringify_int64
	#define tstringify_uint64	wstringify_uint64
	#define tstringify_float	wstringify_float
	#define tstringify_double	wstringify_double
#else
	#define tstringify			stringify
	#define tstringify_int64	stringify_int64
	#define tstringify_uint64	stringify_uint64
	#define tstringify_float	stringify_float
	#define tstringify_double	stringify_double
#endif

inline unsigned int	atoui(const char *szString) { return strtoul(szString, NULL, 10); }
unsigned int xtoi(const char *lpszHex);

int memsubstr(const void* haystack, size_t haystackSize, const void* needle, size_t needleSize);

std::string striconv(const std::string &strinput, const char *lpszFromCharset, const char *lpszToCharset);

std::string str_storage(unsigned long long ulBytes, bool bUnlimited = true);

std::string PrettyIP(long unsigned int ip);
std::string GetServerNameFromPath(const char *szPath);
std::string GetServerTypeFromPath(const char *szPath);
std::string GetServerPortFromPath(const char *szPath);
std::string ServerNamePortToURL(const char *lpszType, const char *lpszServerName, const char *lpszServerPort, const char *lpszExtra = "zarafa");

static inline bool parseBool(const std::string &s) {
	return !(s == "0" || s == "false" || s == "no");
}

std::string shell_escape(std::string str);
std::string shell_escape(std::wstring wstr);
std::string forcealnum(const std::string& str, const char *additional = NULL);

std::vector<std::wstring> tokenize(const std::wstring &strInput, const WCHAR sep, bool bFilterEmpty = false);
std::vector<std::string> tokenize(const std::string &strInput, const char sep, bool bFilterEmpty = false);
std::string concatenate(std::vector<std::string> &elements, const std::string &delimeters);

std::string trim(const std::string &strInput, const std::string &strTrim = " ");

unsigned char x2b(char c);
std::string hex2bin(const std::string &input);
std::string hex2bin(const std::wstring &input);

std::string bin2hex(const std::string &input);
std::wstring bin2hexw(const std::string &input);

std::string bin2hex(unsigned int inLength, const unsigned char *input);
std::wstring bin2hexw(unsigned int inLength, const unsigned char *input);

#ifdef UNICODE
#define bin2hext bin2hexw
#else
#define bin2hext bin2hex
#endif

std::string urlEncode(const std::string &input);
std::string urlEncode(const std::wstring &input, const char* charset);
std::string urlEncode(const WCHAR* input, const char* charset);
std::string urlDecode(const std::string &input);

std::string StringEscape(const char* input, const char *tokens, const char escape);

void BufferLFtoCRLF(size_t size, const char *input, char *output, size_t *outsize);
void StringCRLFtoLF(const std::wstring &strInput, std::wstring *lpstrOutput);
void StringLFtoCRLF(std::string &strInOut);
void StringTabtoSpaces(const std::wstring &strInput, std::wstring *lpstrOutput);

template<typename T>
std::vector<T> tokenize(const T &str, const T &delimiters)
{
	std::vector<T> tokens;
    	
	// skip delimiters at beginning.
   	typename T::size_type lastPos = str.find_first_not_of(delimiters, 0);
    	
	// find first "non-delimiter".
   	typename T::size_type pos = str.find_first_of(delimiters, lastPos);

   	while (std::string::npos != pos || std::string::npos != lastPos)
   	{
       	// found a token, add it to the std::vector.
       	tokens.push_back(str.substr(lastPos, pos - lastPos));
		
       	// skip delimiters.  Note the "not_of"
       	lastPos = str.find_first_not_of(delimiters, pos);
		
       	// find next "non-delimiter"
       	pos = str.find_first_of(delimiters, lastPos);
   	}

	return tokens;
}

template<typename T>
std::vector<T> tokenize(const T &str, const typename T::value_type *delimiters)
{
	return tokenize(str, (T)delimiters);
}

template<typename T>
std::vector<std::basic_string<T> > tokenize(const T* str, const T* delimiters)
{
	return tokenize(std::basic_string<T>(str), std::basic_string<T>(delimiters));
}

template<typename _InputIterator, typename _Tp>
_Tp join(_InputIterator __first, _InputIterator __last, _Tp __sep)
{
    _Tp s;
    for (; __first != __last; ++__first) {
        if(!s.empty())
            s += __sep;
        s += *__first;
    }
    return s;
}


#endif
