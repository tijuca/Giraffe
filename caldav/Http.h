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

#ifndef _HTTP_H_
#define _HTTP_H_

#include <mapidefs.h>
#include <mapicode.h>
#include <base64.h>
#include "stringutil.h"
#include "CommonUtil.h"
#include "ECChannel.h"
#include "ECIConv.h"
#include <cstdio>
#include <cstdarg>
#include <cctype>
#include <libxml/xmlmemory.h>
#include <libxml/uri.h>
#include <libxml/globals.h>
#include "charset/convert.h"


#define HTTP_CHUNK_SIZE 10000

#define SERVICE_UNKNOWN	0x00
#define SERVICE_ICAL	0x01
#define SERVICE_CALDAV	0x02
#define REQ_PUBLIC		0x04
#define REQ_COLLECTION	0x08

HRESULT HrParseURL(const std::string &stUrl, ULONG *lpulFlag, std::string *lpstrUrlUser = NULL, std::string *lpstrFolder = NULL);

class Http
{
public:

	Http(ECChannel *lpChannel, ECLogger *lpLogger, ECConfig *lpConfig);
	~Http();

	HRESULT HrReadHeaders();
	HRESULT HrValidateReq();
	HRESULT HrReadBody();

	HRESULT HrGetHeaderValue(const std::string &strHeader, std::string *strValue);

	/* @todo, remove and use HrGetHeaderValue() */
	HRESULT HrGetMethod(std::string *strMethod);
	HRESULT HrGetUser(std::wstring *strUser);	
	HRESULT HrGetPass(std::wstring *strPass);
	HRESULT HrGetRequestUrl(std::string *strURL);
	HRESULT HrGetUrl(std::string *strURL);
	HRESULT HrGetBody(std::string *strBody);
	HRESULT HrGetDepth(ULONG *ulDepth);
	HRESULT HrGetCharSet(std::string *strCharset);
	HRESULT HrGetDestination(std::string *strDestination);
	HRESULT HrGetUserAgent(std::string *strUserAgent);
	HRESULT HrGetUserAgentVersion(std::string *strUserAgentVersion);

	HRESULT HrToHTTPCode(HRESULT hr);
	HRESULT HrResponseHeader(unsigned int ulCode, std::string strResponse);
	HRESULT HrResponseHeader(std::string strHeader, std::string strValue);
	HRESULT HrRequestAuth(std::string strMsg);
	HRESULT HrResponseBody(std::string strResponse);

	HRESULT HrSetKeepAlive(int ulKeepAlive);
	HRESULT HrFinalize();

	bool CheckIfMatch(LPMAPIPROP lpProp);

private:
	ECChannel *m_lpChannel;
	ECLogger *m_lpLogger;
	ECConfig *m_lpConfig;

	/* request */
	std::string m_strAction;	//!< full 1st-line
	std::string m_strMethod;	//!< HTTP method, eg. GET, PROPFIND, etc.
	std::string m_strURL;		//!< original action url
	std::string m_strPath;		//!< decoded url
	std::string m_strHttpVer;
	std::map<std::string, std::string> mapHeaders;

	std::string m_strUser;
	std::string m_strPass;
	std::string m_strReqBody;
	std::string m_strCharSet;

	std::string m_strUserAgent;
	std::string m_strUserAgentVersion;

	/* response */
	std::string m_strRespHeader;			//!< first header with http status code
	std::list<std::string> m_lstHeaders;	//!< other headers
	std::string m_strRespBody;
	ULONG m_ulRetCode;
	int m_ulKeepAlive;

	convert_context m_converter;

	HRESULT HrParseHeaders();

	HRESULT HrFlushHeaders();

	HRESULT X2W(const std::string &strIn, std::wstring *lpstrOut);
};

#endif
