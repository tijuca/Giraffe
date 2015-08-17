/*
 * Copyright 2005 - 2015  Zarafa B.V. and its licensors
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License, version 3,
 * as published by the Free Software Foundation with the following
 * additional terms according to sec. 7:
 * 
 * "Zarafa" is a registered trademark of Zarafa B.V.
 * The licensing of the Program under the AGPL does not imply a trademark 
 * license. Therefore any rights, title and interest in our trademarks 
 * remain entirely with us.
 * 
 * Our trademark policy (see TRADEMARKS.txt) allows you to use our trademarks
 * in connection with Propagation and certain other acts regarding the Program.
 * In any case, if you propagate an unmodified version of the Program you are
 * allowed to use the term "Zarafa" to indicate that you distribute the Program.
 * Furthermore you may use our trademarks where it is necessary to indicate the
 * intended purpose of a product or service provided you use it in accordance
 * with honest business practices. For questions please contact Zarafa at
 * trademark@zarafa.com.
 *
 * The interactive user interface of the software displays an attribution 
 * notice containing the term "Zarafa" and/or the logo of Zarafa. 
 * Interactive user interfaces of unmodified and modified versions must 
 * display Appropriate Legal Notices according to sec. 5 of the GNU Affero 
 * General Public License, version 3, when you propagate unmodified or 
 * modified versions of the Program. In accordance with sec. 7 b) of the GNU 
 * Affero General Public License, version 3, these Appropriate Legal Notices 
 * must retain the logo of Zarafa or display the words "Initial Development 
 * by Zarafa" if the display of the logo is not reasonably feasible for
 * technical reasons.
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

#ifndef convstring_INCLUDED
#define convstring_INCLUDED

#include "zcdefs.h"
#include "convert.h"
#include "tstring.h"
#include <string>
#include <charset/utf8string.h>

#include <mapidefs.h>

class convstring _final {
public:
	static convstring from_SPropValue(const SPropValue *lpsPropVal, bool bCheapCopy = true);
	static convstring from_SPropValue(const SPropValue &sPropVal, bool bCheapCopy = true);

	convstring();
	convstring(const convstring &other);
	convstring(const char *lpsz, bool bCheapCopy = true);
	convstring(const wchar_t *lpsz, bool bCheapCopy = true);
	convstring(const TCHAR *lpsz, ULONG ulFlags, bool bCheapCopy = true);
	
	bool null_or_empty() const;
	
	operator utf8string() const;
	operator std::string() const;
	operator std::wstring() const;
	const char *c_str() const;
	const wchar_t *wc_str() const;
	const char *u8_str() const;

#ifdef UNICODE
	#define t_str	wc_str
#else
	#define t_str	c_str
#endif
	
private:
	template<typename T>
	T convert_to() const;
	
	template<typename T>
	T convert_to(const char *tocode) const;

private:
	const TCHAR *m_lpsz;
	ULONG		m_ulFlags;
	tstring		m_str;

	mutable convert_context	m_converter;
};

#endif // ndef convstring_INCLUDED
