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

#include "../platform.h"
#include "convert.h"

#include <mapicode.h>

#include <numeric>
#include <vector>
#include <string>
#include <stringutil.h>
#include <errno.h>
#define BUFSIZE 4096

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

convert_exception::convert_exception(enum exception_type type, const std::string &message)
	: std::runtime_error(message)
	, m_type(type)
{}

unknown_charset_exception::unknown_charset_exception(const std::string &message) 
	: convert_exception(eUnknownCharset, message) 
{}

illegal_sequence_exception::illegal_sequence_exception(const std::string &message)
	: convert_exception(eIllegalSequence, message)
{}


namespace details {

	HRESULT HrFromException(const convert_exception &ce)
	{
		switch (ce.type()) {
			case convert_exception::eUnknownCharset:	return MAPI_E_NOT_FOUND;
			case convert_exception::eIllegalSequence:	return MAPI_E_INVALID_PARAMETER;
			default:									return MAPI_E_CALL_FAILED;
		}
	}

	// HACK: prototypes may differ depending on the compiler and/or system (the
	// second parameter may or may not be 'const'). This redeclaration is a hack
	// to have a common prototype "iconv_cast".
	class ICONV_HACK
	{
	public:
		ICONV_HACK(const char** ptr) : m_ptr(ptr) { }

		// the compiler will choose the right operator
		operator const char**() { return m_ptr; }
		operator char**() { return const_cast <char**>(m_ptr); }

	private:
		const char** m_ptr;
	};
	
	
	/**
	 * Constructor for iconv_context_base
	 *
	 * The conversion context for iconv charset conversions takes a fromcode and a tocode,
	 * which are the source and destination charsets, respectively. The 'tocode' may take
	 * some extra options, separated with '//' from the charset, and then separated by commas
	 *
	 * This function accepts values accepted by GNU iconv, plus it also accepts the FORCE
	 * modifier, eg.:
	 *
	 * iso-8859-1//TRANSLIT,FORCE
	 * windows-1252//TRANSLIT
	 *
	 * The 'fromcode' can also take modifiers but they are ignored by iconv.
	 *
	 * Also, instead of FORCE, the HTMLENTITY modifier can be used, eg:
	 *
	 * iso-8859-1//HTMLENTITY
	 *
	 * This works much like TRANSLIT, except that characters that cannot be represented in the
	 * output character set are not represented by '?' but by the HTML entity '&#xxxx;'. This is useful
	 * for generating HTML in which as many characters as possible are directly represented, but
	 * other characters are represented by an HTML entity. Note: the HTMLENTITY modifier may only
	 * be applied when the fromcode is CHARSET_WCHAR (this is purely an implementation limitation)
	 *
     * Note that in release builds, the FORCE flag is implicit. This means that a conversion error
     * is never generated. If you require an error on conversion problems, you must pass the NOFORCE
     * modifier. This has no effect in debug builds.
	 *
	 * @param tocode Destination charset
	 * @param fromcode Source charset
	 */
	iconv_context_base::iconv_context_base(const char* tocode, const char* fromcode)
	{
#ifdef FORCE_CHARSET_CONVERSION		
		// We now default to forcing conversion; this makes sure that we don't SIGABORT
		// when some bad input from a user fails to convert. This means that the 'FORCE'
		// flag is on by default; specifying it is not useful.
		m_bForce = true;
#else
		// In debug builds, SIGABRT will be triggered in most cases due to the throw() 
		// in doconvert()
		m_bForce = false;
#endif
		m_bHTML = false;
		
        std::string strto = tocode;
        size_t pos = strto.find("//");

        if(pos != std::string::npos) {
            std::string options = strto.substr(pos+2);
            strto = strto.substr(0,pos);
            std::vector<std::string> vOptions = tokenize(options, ",");
            std::vector<std::string> vOptionsFiltered;
            std::vector<std::string>::iterator i;

            i = vOptions.begin();
            while(i != vOptions.end()) {
                if(*i == "FORCE") {
                    m_bForce = true;
                } else if(*i == "NOFORCE") {
                    m_bForce = false;
                } else if(*i == "HTMLENTITIES" && stricmp(fromcode, CHARSET_WCHAR) == 0) {
                	m_bHTML = true;
                } else vOptionsFiltered.push_back(*i);
				i++;
            }

			if(!vOptionsFiltered.empty()) {
	            strto += "//";
				strto += join(vOptionsFiltered.begin(), vOptionsFiltered.end(), std::string(","));
			}
        }


		m_cd = iconv_open(strto.c_str(), fromcode);
		if (m_cd == (iconv_t)(-1))
			throw unknown_charset_exception(strerror(errno));
	}

	iconv_context_base::~iconv_context_base()
	{
		if (m_cd != (iconv_t)(-1))
			iconv_close(m_cd);
	}

	void iconv_context_base::doconvert(const char *lpFrom, size_t cbFrom)
	{
		char buf[BUFSIZE];
		const char *lpSrc = NULL;
		char *lpDst = NULL;
		size_t cbSrc = 0;
		size_t cbDst = 0;
		size_t err;
		
		lpSrc = lpFrom;
		cbSrc = cbFrom;
		
		while(cbSrc) {
			lpDst = buf;
			cbDst = sizeof(buf);
			err = iconv(m_cd, ICONV_HACK(&lpSrc), &cbSrc, &lpDst, &cbDst);
			
			if (err == (size_t)(-1) && cbDst == sizeof(buf)) {
				if(m_bHTML) {
					if(cbSrc < sizeof(wchar_t)) {
						// Do what //FORCE would have done
						lpSrc++;
						cbSrc--;
					} else {
						// Convert the codepoint to '&#12345;'
						std::wstring wstrEntity = L"&#";
						size_t cbEntity;
						unsigned int code = *(wchar_t *)lpSrc;
						const char *lpEntity;
						
						wstrEntity += wstringify(code);
						wstrEntity += L";";
						cbEntity = wstrEntity.size() * sizeof(wchar_t);
						lpEntity = (const char *)wstrEntity.c_str();
						
						// Since we don't know in what charset we are outputting, we have to send
						// the entity through iconv so that it can convert it to the target charset.
						
						err = iconv(m_cd, ICONV_HACK(&lpEntity), &cbEntity, &lpDst, &cbDst);
						
						if(err == (size_t)(-1)) {
							ASSERT(false); // This will should never fail
						}
						
						lpSrc += sizeof(wchar_t);
						cbSrc -= sizeof(wchar_t);
					}
				} else if(m_bForce) {
					// Force conversion by skipping this character
					if(cbSrc) {
						lpSrc++;
						cbSrc--;
					}
				} else {
					throw illegal_sequence_exception(strerror(errno));
				}
			}			
			// buf now contains converted chars, append them to output
			append(buf, sizeof(buf) - cbDst);
		}

		// Finalize (needed for stateful conversion)	
		lpDst = buf;
		cbDst = sizeof(buf);
		err = iconv(m_cd, NULL, NULL, &lpDst, &cbDst);
		append(buf, sizeof(buf) - cbDst);
	}
	
} // namespace details

convert_context::convert_context() throw()
{}

convert_context::~convert_context()
{
	context_map::iterator iContext;
	for (iContext = m_contexts.begin(); iContext != m_contexts.end(); ++iContext)
		delete iContext->second;
		
	code_set::iterator iCode;
	for (iCode = m_codes.begin(); iCode != m_codes.end(); ++iCode)
		delete[] *iCode;
}

void convert_context::persist_code(context_key &key, unsigned flags)
{
	if (flags & pfToCode) {
		code_set::iterator iCode = m_codes.find(key.tocode);
		if (iCode == m_codes.end()) {
			char *tocode = new char[strlen(key.tocode) + 1];
			memcpy(tocode, key.tocode, strlen(key.tocode) + 1);
			iCode = m_codes.insert(tocode).first;
		}
		key.tocode = *iCode;
	}
	if (flags & pfFromCode) {
		code_set::iterator iCode = m_codes.find(key.fromcode);
		if (iCode == m_codes.end()) {
			char *fromcode = new char[strlen(key.fromcode) + 1];
			memcpy(fromcode, key.fromcode, strlen(key.fromcode) + 1);
			iCode = m_codes.insert(fromcode).first;
		}
		key.fromcode = *iCode;
	}
}

char* convert_context::persist_string(const std::string &strValue)
{
	m_lstStrings.push_back(strValue);
	return const_cast<char*>(m_lstStrings.back().c_str());
}

wchar_t* convert_context::persist_string(const std::wstring &wstrValue)
{
	m_lstWstrings.push_back(wstrValue);
	return const_cast<wchar_t*>(m_lstWstrings.back().c_str());
}
