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

#ifndef _HTTP_H_
#define _HTTP_H_

#include <kopano/zcdefs.h>
#include <mapidefs.h>
#include <mapicode.h>
#include <kopano/stringutil.h>
#include <kopano/CommonUtil.h>
#include <kopano/ECChannel.h>
#include <kopano/ECIConv.h>
#include <cstdio>
#include <cstdarg>
#include <cctype>
#include <libxml/xmlmemory.h>
#include <libxml/uri.h>
#include <libxml/globals.h>
#include <kopano/charset/convert.h>


#define HTTP_CHUNK_SIZE 10000

#define SERVICE_UNKNOWN	0x00
#define SERVICE_ICAL	0x01
#define SERVICE_CALDAV	0x02
#define REQ_PUBLIC		0x04
#define REQ_COLLECTION	0x08

HRESULT HrParseURL(const std::string &stUrl, ULONG *lpulFlag, std::string *lpstrUrlUser = NULL, std::string *lpstrFolder = NULL);

class Http _kc_final {
public:
	Http(ECChannel *lpChannel, ECConfig *lpConfig);
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
