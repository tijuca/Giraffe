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
#include "charset/convert.h"
#include <sstream>

#include "ECIConv.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

std::string stringify(unsigned int x, bool usehex, bool _signed) {
	char szBuff[33];

	if(usehex)
		sprintf(szBuff, "0x%08X", x);
	else {
		if(_signed)
			sprintf(szBuff, "%d", x);
		else
			sprintf(szBuff, "%u", x);
	}
	
	return szBuff;
}

std::string stringify_int64(long long x, bool usehex) {
	std::ostringstream s;

	if (usehex) {
		s.flags(std::ios::showbase);
		s.setf(std::ios::hex, std::ios::basefield);	// showbase && basefield: add 0x prefix
		s.setf(std::ios::uppercase);
	}
	s << x;

	return s.str();
}

std::string stringify_float(float x) {
	std::ostringstream s;

	s << x;

	return s.str();
}

std::string stringify_double(double x, int prec, bool bLocale) {
	std::ostringstream s;

	s.precision(prec);
	s.setf(std::ios::fixed,std::ios::floatfield);
	if (bLocale) {
		try {
			std::locale l("");
			s.imbue(l);
		} catch (std::runtime_error &e) {
			// locale not available, print in C
		}
		s << x;
	} else
		s << x;

	return s.str();
}

/* Convert time_t to string representation
 *
 * String is in format YYYY-MM-DD mm:hh in the local timezone
 *
 * @param time_t x Timestamp to convert
 * @return string String representation
 */
std::string stringify_datetime(time_t x) {
	char date[128];
	struct tm *tm;
	
	tm = localtime(&x);
	if(!tm){
		x = 0;
		tm = localtime(&x);
	}
	
	//strftime(date, 128, "%Y-%m-%d %H:%M:%S", tm);
	snprintf(date,128,"%d-%02d-%02d %.2d:%.2d:%.2d",tm->tm_year+1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
	
	return date;
}

// FIXME support only unsigned int!!!
std::wstring wstringify(unsigned int x, bool usehex, bool _signed)
{
	std::wostringstream s;

	if (usehex) {
		s.flags(std::ios::showbase);
		s.setf(std::ios::hex, std::ios::basefield); // showbase && basefield: add 0x prefix
		s.setf(std::ios::uppercase);
	}
	s << x;

	return s.str();
}

std::wstring wstringify_int64(long long x, bool usehex)
{
	std::wostringstream s;

	if (usehex) {
		s.flags(std::ios::showbase);
		s.setf(std::ios::hex, std::ios::basefield);	// showbase && basefield: add 0x prefix
		s.setf(std::ios::uppercase);
	}
	s << x;

	return s.str();
}

std::wstring wstringify_uint64(unsigned long long x, bool usehex)
{
	std::wostringstream s;

	if (usehex) {
		s.flags(std::ios::showbase);
		s.setf(std::ios::hex, std::ios::basefield);	// showbase && basefield: add 0x prefix
		s.setf(std::ios::uppercase);
	}
	s << x;

	return s.str();
}

std::wstring wstringify_float(float x)
{
	std::wostringstream s;

	s << x;

	return s.str();
}

std::wstring wstringify_double(double x, int prec)
{
	std::wostringstream s;

	s.precision(prec);
	s << x;

	return s.str();
}

unsigned int xtoi(const char *lpszHex)
{
	unsigned int ulHex = 0;

	sscanf(lpszHex, "%X", &ulHex);

	return ulHex;
}

int memsubstr(const void* haystack, size_t haystackSize, const void* needle, size_t needleSize)
{
	size_t pos = 0;
	size_t match = 0;
	BYTE* searchbuf = (BYTE*)needle;
	BYTE* databuf = (BYTE*)haystack;

	if(haystackSize < needleSize)
		return (haystackSize-needleSize);

	while(pos < haystackSize)
	{
		if(*databuf == *searchbuf){
			searchbuf++;
			match++;

			if(match == needleSize)
				return 0;
		}else{
			databuf -= match;
			pos -= match;

			searchbuf = (BYTE*)needle;
			match = 0;
		}

		databuf++;
		pos++;
	}

	return 1;
}

std::string str_storage(unsigned long long ulBytes, bool bUnlimited) {
	static double MB = 1024.0 * 1024.0;

	if (ulBytes == 0 && bUnlimited)
		return "unlimited";

	return stringify_double((double)ulBytes / MB, 2) + " MB";
}

std::string PrettyIP(long unsigned int ip) {
    std::string strPretty;
    
    strPretty += stringify((ip >> 24)&0xFF);
    strPretty += ".";
    strPretty += stringify((ip >> 16)&0xFF);
    strPretty += ".";
    strPretty += stringify((ip >> 8)&0xFF);
    strPretty += ".";
    strPretty += stringify(ip&0xFF);
    
    return strPretty;
}

std::string GetServerNameFromPath(const char *szPath) {
	std::string path = szPath;
	size_t pos = 0;

	pos = path.find("://");
	if (pos != std::string::npos) {
		/* Remove prefixed type information */
		path.erase(0, pos + 3);
	}

	pos = path.find(':');
	if (pos != std::string::npos)
		path.erase(pos, std::string::npos);

	return path;
}

std::string GetServerTypeFromPath(const char *szPath) {
	std::string path = szPath;
	size_t pos;

	pos = path.find("://");
	if (pos != std::string::npos)
		return path.substr(0, pos);
	return std::string();
}

std::string GetServerPortFromPath(const char *szPath) {
	std::string path = szPath;
	size_t pos = 0;

	if (strncmp(path.c_str(), "http", 4) != 0)
		return std::string();

	pos = path.rfind(':');
	if (pos == std::string::npos)
		return std::string();
	
	pos += 1; /* Skip ':' */

	/* Remove all leading characters */
	path.erase(0, pos);

	/* Strip additional path */
	pos = path.rfind('/');
	if (pos != std::string::npos)
		path.erase(pos, std::string::npos);

	return path.c_str();
}

std::string ServerNamePortToURL(const char *lpszType, const char *lpszServerName, const char *lpszServerPort, const char *lpszExtra) {
	std::string strURL;

	if (lpszType && strlen(lpszType) > 0) {
		strURL.append(lpszType);
		strURL.append("://");
	}

	strURL.append(lpszServerName);

	if (lpszServerPort && strlen(lpszServerPort) > 0) {
		strURL.append(":");
		strURL.append(lpszServerPort);
	}

	if (strnicmp(lpszType,"http", 4) == 0 && lpszExtra && strlen(lpszExtra) > 0) {
		strURL.append("/");
		strURL.append(lpszExtra);
	}

	return strURL;
}

std::string shell_escape(std::string str)
{
	std::string::iterator start;
	std::string::iterator ptr;
	std::string escaped;

	start = ptr = str.begin();
	while (ptr != str.end()) {
		while (ptr != str.end() && *ptr != '\'')
			ptr++;

		escaped += std::string(start, ptr);
		if (ptr == str.end())
			break;

		start = ++ptr;          // skip single quote
		escaped += "'\\''";     // shell escape sequence
	}

	return escaped;
}

std::string shell_escape(std::wstring wstr)
{
	std::string strLocale = convert_to<std::string>(wstr);
	return shell_escape(strLocale);
}

std::vector<std::wstring> tokenize(const std::wstring &strInput, const WCHAR sep, bool bFilterEmpty) {
	const WCHAR *begin, *end = NULL;
	std::vector<std::wstring> vct;

	begin = strInput.c_str();
	while (*begin != '\0') {
		end = wcschr(begin, sep);
		if (!end) {
			vct.push_back(begin);
			break;
		}
		if (!bFilterEmpty || std::distance(begin,end) > 0)
			vct.push_back(std::wstring(begin,end));
		begin = end+1;
	}

	return vct;
}

std::vector<std::string> tokenize(const std::string &strInput, const char sep, bool bFilterEmpty) {
	const char *begin, *last, *end = NULL;
	std::vector<std::string> vct;

	begin = strInput.c_str();
	last = begin + strInput.length();
	while (begin < last) {
		end = strchr(begin, sep);
		if (!end) {
			vct.push_back(begin);
			break;
		}
		if (!bFilterEmpty || std::distance(begin,end) > 0)
			vct.push_back(std::string(begin,end));
		begin = end+1;
	}

	return vct;
}

std::string concatenate(std::vector<std::string> &elements, const std::string &delimeters)
{
	std::vector<std::string>::iterator iter;
	std::string concat;

    if (!elements.empty()) {
		for (iter = elements.begin(); iter != elements.end(); iter++)
			concat += *iter + delimeters;
		concat.erase(concat.end() - delimeters.size());
	}

	return concat;
}

std::string trim(const std::string &strInput, const std::string &strTrim)
{
	std::string s = strInput;
	size_t pos;

	if (s.empty())
		return s;

	pos = s.find_first_not_of(strTrim); 
	s.erase(0, pos); 
	 	 
	pos = s.find_last_not_of(strTrim);
	if (pos != std::string::npos) 
		s.erase(pos + 1, std::string::npos); 
 	 
	return s;
}

unsigned char x2b(char c)
{
	if (c >= '0' && c <= '9')
	// expects sensible input
		return c - '0';
	else if (c >= 'a')
		return c - 'a' + 10;
	return c - 'A' + 10;
}

std::string hex2bin(const std::string &input)
{
	std::string buffer;

	if (input.length() % 2 != 0)
		return buffer;

	buffer.reserve(input.length() / 2);
	for (unsigned int i = 0; i < input.length(); ) {
		unsigned char c;
		c = x2b(input[i++]) << 4;
		c |= x2b(input[i++]);
		buffer += c;
	}

	return buffer;
}

std::string hex2bin(const std::wstring &input)
{
	std::string buffer;

	if (input.length() % 2 != 0)
		return buffer;

	buffer.reserve(input.length() / 2);
	for (unsigned int i = 0; i < input.length(); ) {
		unsigned char c;
		c = x2b((char)input[i++]) << 4;
		c |= x2b((char)input[i++]);
		buffer += c;
	}

	return buffer;
}

std::string bin2hex(unsigned int inLength, const unsigned char *input)
{
	const char digits[] = "0123456789ABCDEF";
	std::string buffer;

	if (!input)
		return buffer;

	buffer.reserve(inLength * 2);
	for (unsigned int i = 0; i < inLength; i++) {
		buffer += digits[input[i]>>4];
		buffer += digits[input[i]&0x0F];
	}

	return buffer;
}

std::string bin2hex(const std::string &input)
{
    return bin2hex((unsigned int)input.size(), (const unsigned char*)input.c_str());
}

std::wstring bin2hexw(unsigned int inLength, const unsigned char *input)
{
	const wchar_t digits[] = L"0123456789ABCDEF";
	std::wstring buffer;

	if (!input)
		return buffer;

	buffer.reserve(inLength * 2);
	for (unsigned int i = 0; i < inLength; i++) {
		buffer += digits[input[i]>>4];
		buffer += digits[input[i]&0x0F];
	}

	return buffer;
}

std::wstring bin2hexw(const std::string &input)
{
    return bin2hexw((unsigned int)input.size(), (const unsigned char*)input.c_str());
}

std::string StringEscape(const char* input, const char *tokens, const char escape)
 {
	std::string strEscaped;
	int i = 0;
	int t;

	while (true) {
		if(input[i] == 0)
			break;
		
		for(t=0; tokens[t] != 0; t++) {
			if (input[i] == tokens[t])
				strEscaped += escape;
		}

		strEscaped += input[i];
		i++;
	}

	return strEscaped;
}


/** 
 * Encodes a string for inclusion into an url.
 *
 * @note this does not encode an url to another more valid url (since / would get encoded!)
 * @note watch the locale of the string, make sure it's the same as the rest of the url.
 * 
 * @param[in] input string to encode
 * 
 * @return encoded string valid to include in an url
 */
std::string urlEncode(const std::string &input)
{
	std::string output;
	const char digits[] = "0123456789ABCDEF";

	output.reserve(input.length());
	for (size_t i = 0; i < input.length(); i++) 
	{
		if (input[i] <= 127) {
			switch (input[i]) {
			case ':':
			case '/':
			case '?':
			case '#':
			case '[':
			case ']':
			case '@':
			case '!':
			case '$':
			case '&':
			case '\'':
			case '(':
			case ')':
			case '*':
			case '+':
			case ',':
			case ';':
			case '=':
				output += '%';
				output += digits[input[i]>>4];
				output += digits[input[i]&0x0F];
				break;
			default:
				output += input[i];
			}
		} else {
			output += '%';
			output += digits[input[i]>>4];
			output += digits[input[i]&0x0F];
		}
	}

	return output;
}

/** 
 * encode an url part, input in wide char, and destination charset in encoded characters
 * 
 * @param[in] input wide string to convert to valid url encoded ascii string
 * @param[in] charset non-ascii characters will be encoded for this charset
 * 
 * @return url valid encoded string
 */
std::string urlEncode(const std::wstring &input, const char* charset)
{
	std::string output = convert_to<std::string>(charset, input, rawsize(input), CHARSET_WCHAR);
	return urlEncode(output);
}

std::string urlEncode(const WCHAR* input, const char* charset)
{
	std::string output = convert_to<std::string>(charset, input, rawsize(input), CHARSET_WCHAR);
	return urlEncode(output);
}

/** 
 * replaces %## values by ascii values
 * i.e Amsterdam%2C -> Amsterdam,
 * @note 1. this can take a full url, since it just replaces the %## 
 * @note 2. you need to handle the locale of the string yourself!
 * 
 * @param[in] input url encoded string
 * 
 * @return decoded url in the locale it was encoded in
 */
std::string urlDecode(const std::string &input)
{
	std::string output;

	output.reserve(input.length());
	for (size_t i = 0; i < input.length(); i++) 
	{
		if (input[i] == '%' && input.length() > i + 2)
		{
			unsigned char c;
			c = x2b(input[++i]) << 4;
			c |= x2b(input[++i]);
			output += c;
		} 
		else 
			output += input[i];
	}

	return output;
}


/** 
 * Convert a memory buffer with strings with Unix \n enters to DOS
 * \r\n enters.
 * 
 * @param[in] size length of the input
 * @param[in] input buffer containing strings with enters to convert
 * @param[out] output buffer with enough space to hold input + extra \r characters
 * @param[out] outsize number of characters written to output
 */
void BufferLFtoCRLF(size_t size, const char *input, char *output, size_t *outsize) {
	size_t j = 0;
	for (size_t i = 0; i < size; i++) {
		if (input[i] == '\r') {
			if ((i+1) < size && input[i+1] == '\n') {
				output[j++] = '\r';
				output[j++] = '\n';
				i++;
			} else {
				output[j++] = '\r';
				output[j++] = '\n';
			}
		} else if (input[i] == '\n') {
			output[j++] = '\r';
			output[j++] = '\n';
		} else {
			output[j++] = input[i];
		}
	}
	output[j] = '\0';
	*outsize = j;
}

/**
 * converts Tabs in a string to spaces
 *
 * @param[in] 	strInput		input string to be converted
 * @param[out] 	strOutput		return converted string
 */
void StringTabtoSpaces(const std::wstring &strInput, std::wstring *lpstrOutput) {

	std::wstring::const_iterator iInput(strInput.begin());
	std::wstring strOutput;

	strOutput.reserve(strInput.length());

	for (; iInput != strInput.end(); ++iInput) {
		if (*iInput == '\t') {
			strOutput.append(4, ' ');
		} else {
			strOutput.append(1, *iInput);
		}
	}

	lpstrOutput->swap(strOutput);
}

/**
 * converts CRLF in a string to LF
 *
 * @param[in] strInput		input string to be converted
 * @param[out] strOutput	return converted string
 */
void StringCRLFtoLF(const std::wstring &strInput, std::wstring *lpstrOutput) {
	std::wstring::const_iterator iInput(strInput.begin());
	std::wstring strOutput;

	strOutput.reserve(strInput.length());

	for (; iInput != strInput.end(); ++iInput) {
		
		// skips /r if /r/n found together in the text
		if (*iInput == '\r' && (iInput + 1 != strInput.end() && *(iInput + 1) == '\n')) 
			continue;
		else
			strOutput.append(1, *iInput);
		
	}
	lpstrOutput->swap(strOutput);
}

/** 
 * converts a string inline from \n enters to \r\n
 * 
 * @param strInOut string to edit
 */
void StringLFtoCRLF(std::string &strInOut)
{
	std::string strOutput;
	std::string::iterator i;

	strOutput.reserve(strInOut.size());

	for (i = strInOut.begin(); i != strInOut.end(); i++) {
		if (*i == '\n' && i != strInOut.begin() && *(i-1) != '\r')
			strOutput.append("\r\n");
		else
			strOutput.append(1, *i);
	}

	swap(strInOut, strOutput);
}


/**
 * Force a string to contain alphanumerics only
 *
 * Replaces all other characters with _ unless they are in szAdditional
 *
 * @param[in] str Input string
 * @param[in] szAdditional Additional characters to accept
 * @return Modified string
 */
std::string forcealnum(const std::string& str, const char *szAdditional)
{
    std::string out;

    for(std::string::const_iterator i = str.begin(); i != str.end(); i++) {
        if(isalnum(*i))
            out += *i;
        else if(szAdditional && strchr(szAdditional, *i) != NULL)
            out += *i;
        else
            out += '_';
    }

    return out;
}
