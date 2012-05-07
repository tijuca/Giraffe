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

#ifndef _WEBDAV_H_
#define _WEBDAV_H_

#include "libxml/tree.h"
#include "libxml/xmlwriter.h"
#include "libxml/parser.h"
#include "libxml/tree.h"

#include <mapi.h>
#include <map>

#include "ProtocolBase.h"
#include "Http.h"

typedef struct {
	std::string strNS;
	std::string strPropname;
	std::string strPropAttribName;
	std::string strPropAttribValue;
} WEBDAVPROPNAME;


typedef struct {
	WEBDAVPROPNAME sPropName;
	std::wstring wstrValue;
} WEBDAVVALUE;

typedef struct {
	WEBDAVVALUE sDavValue;
	ULONG ulDepth;
} WEBDAVITEM;

typedef struct {
	WEBDAVPROPNAME sPropName;
	std::list<WEBDAVVALUE> lstValues;
	std::wstring wstrValue;
	std::list<WEBDAVITEM> lstItems;
} WEBDAVPROPERTY;

typedef struct {
	WEBDAVPROPNAME sPropName;
	std::list<WEBDAVPROPERTY> lstProps;
} WEBDAVPROP;

typedef struct {
	WEBDAVPROPNAME sPropName;
	WEBDAVPROP sProp;
	WEBDAVVALUE sStatus;
} WEBDAVPROPSTAT;

typedef struct {
	WEBDAVPROPNAME sPropName;
	std::list<WEBDAVPROPSTAT> lstsPropStat;
	std::list<WEBDAVPROPERTY> lstProps;
	WEBDAVVALUE sHRef;
	WEBDAVVALUE sStatus;
} WEBDAVRESPONSE;

typedef struct {
	WEBDAVPROPNAME sPropName;
	std::list<WEBDAVRESPONSE> lstResp;
} WEBDAVMULTISTATUS;

typedef struct {
	WEBDAVPROPNAME sPropName;
	std::list<std::string> lstFilters;
	time_t tStart;
} WEBDAVFILTER;

typedef struct {
	WEBDAVPROPNAME sPropName;
	WEBDAVPROP sProp;
	WEBDAVFILTER sFilter;
} WEBDAVREQSTPROPS;

typedef struct {
	WEBDAVPROPNAME sPropName;
	WEBDAVPROP sProp;
	std::list<WEBDAVVALUE> lstWebVal;
} WEBDAVRPTMGET;

typedef struct {
	std::string strUser;
	std::string strIcal;
}WEBDAVFBUSERINFO;

typedef struct {
	time_t tStart;
	time_t tEnd;
	std::string strOrganiser;
	std::string strUID;
	std::list<WEBDAVFBUSERINFO> lstFbUserInfo;
}WEBDAVFBINFO;

#define CALDAVNS "urn:ietf:params:xml:ns:caldav"
#define CALDAVNSDEF "DAV:"

class WebDav: public ProtocolBase
{

public:
	WebDav(Http *lpRequest, IMAPISession *lpSession, ECLogger *lpLogger, std::string strSrvTz, std::string strCharset);
	virtual ~WebDav();

	HRESULT HrPropfind();
	HRESULT HrDelete();
	HRESULT HrPut();
	HRESULT HrPropPatch();
	HRESULT HrMkCalendar();
	HRESULT HrPost(WEBDAVFBINFO *lpsWebFbInfo);

protected:
	xmlDoc *m_lpXmlDoc;
	std::map <std::string,std::string> m_mapNs;


	HRESULT HrParseXml();
	virtual	HRESULT HrHandlePropfind(WEBDAVREQSTPROPS *sProp,WEBDAVMULTISTATUS *lpsDavMulStatus);
	virtual HRESULT HrListCalEntries(WEBDAVREQSTPROPS *sWebRCalQry,WEBDAVMULTISTATUS *sWebMStatus);
	virtual	HRESULT HrHandleReport(WEBDAVRPTMGET *sWebRMGet, WEBDAVMULTISTATUS *sWebMStatus);
	virtual HRESULT HrHandlePropPatch(WEBDAVPROP *lpsDavProp);
	virtual HRESULT HrHandleMkCal(WEBDAVPROP *lpsDavProp);
	virtual HRESULT HrHandlePropertySearch(WEBDAVRPTMGET *sWebRMGet, WEBDAVMULTISTATUS *sWebMStatus);
	virtual HRESULT HrHandlePropertySearchSet(WEBDAVMULTISTATUS *sWebMStatus);
	HRESULT HrReport();
	HRESULT HrHandleRptMulGet();
	HRESULT HrPropertySearch();
	HRESULT HrPropertySearchSet();
	HRESULT HrHandleRptCalQry();
	HRESULT HrHandleDelete();

	HRESULT RespStructToXml(WEBDAVMULTISTATUS *sDavMStatus, std::string *strXml);
	HRESULT GetNs(std::string *szPrefx, std::string *strNs);
	HRESULT RegisterNs(std::string strNs, std::string *strPrefix);
	HRESULT WriteData(xmlTextWriterPtr xmlWriter, WEBDAVVALUE sWebVal,std::string *szNsPrefix);
	HRESULT WriteNode(xmlTextWriterPtr xmlWriter, WEBDAVPROPNAME sWebPrName,std::string *szNsPrefix);
	HRESULT HrWriteSResponse(xmlTextWriterPtr xmlWriter,std::string *lpstrNsPrefix, WEBDAVRESPONSE sResponse);
	HRESULT HrWriteResponseProps(xmlTextWriterPtr xmlWriter, std::string *lpstrNsPrefix, std::list<WEBDAVPROPERTY> *lstProps);
	HRESULT HrWriteSPropStat(xmlTextWriterPtr xmlWriter,std::string *lpstrNsPrefix, WEBDAVPROPSTAT sPropStat);
	HRESULT HrWriteItems(xmlTextWriterPtr xmlWriter, std::string *lpstrNsPrefix,WEBDAVPROPERTY *lpsWebProprty);

	HRESULT HrSetDavPropName(WEBDAVPROPNAME *lpsDavPropName,xmlNode *lpXmlNode);
	HRESULT HrSetDavPropName(WEBDAVPROPNAME *lpsDavPropName,std::string strPropName,std::string strNs);
	HRESULT HrSetDavPropName(WEBDAVPROPNAME *lpsDavPropName,std::string strPropName, std::string strPropAttribName, std::string strPropAttribValue, std::string strNs);

};

#endif
