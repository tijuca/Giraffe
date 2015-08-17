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

#ifndef _WEBDAV_H_
#define _WEBDAV_H_

#include <libxml/tree.h>
#include <libxml/xmlwriter.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

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
	std::string strValue;
} WEBDAVVALUE;

typedef struct {
	WEBDAVVALUE sDavValue;
	ULONG ulDepth;
} WEBDAVITEM;

typedef struct {
	WEBDAVPROPNAME sPropName;
	std::list<WEBDAVVALUE> lstValues;
	std::string strValue;
	std::list<WEBDAVITEM> lstItems;
} WEBDAVPROPERTY;

typedef struct {
	WEBDAVPROPNAME sPropName;
	std::list<WEBDAVPROPERTY> lstProps;
} WEBDAVPROP;

typedef struct {
	WEBDAVPROPNAME sPropName;	/* always propstat */
	WEBDAVPROP sProp;
	WEBDAVVALUE sStatus;		/* always status */
} WEBDAVPROPSTAT;

typedef struct {
	WEBDAVPROPNAME sPropName;
	std::list<WEBDAVPROPSTAT> lstsPropStat;
	std::list<WEBDAVPROPERTY> lstProps;
	WEBDAVVALUE sHRef;
	WEBDAVVALUE sStatus;		/* possible on delete (but we don't use that) */
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

#define WEBDAVNS "DAV:"
#define CALDAVNS "urn:ietf:params:xml:ns:caldav"

class WebDav: public ProtocolBase
{
public:
	WebDav(Http *lpRequest, IMAPISession *lpSession, ECLogger *lpLogger, std::string strSrvTz, std::string strCharset);
	virtual ~WebDav();

protected:
	/* preprocesses xml for HrHandle* functions */
	HRESULT HrPropfind();
	HRESULT HrReport();
	HRESULT HrPut();
	HRESULT HrMkCalendar();
	HRESULT HrPropPatch();
	HRESULT HrPostFreeBusy(WEBDAVFBINFO *lpsWebFbInfo);

	/* caldav implements real processing */
	virtual	HRESULT HrHandlePropfind(WEBDAVREQSTPROPS *sProp,WEBDAVMULTISTATUS *lpsDavMulStatus) = 0;
	virtual HRESULT HrListCalEntries(WEBDAVREQSTPROPS *sWebRCalQry,WEBDAVMULTISTATUS *sWebMStatus) = 0;
	virtual	HRESULT HrHandleReport(WEBDAVRPTMGET *sWebRMGet, WEBDAVMULTISTATUS *sWebMStatus) = 0;
	virtual HRESULT HrHandlePropPatch(WEBDAVPROP *lpsDavProp, WEBDAVMULTISTATUS *sWebMStatus) = 0;
	virtual HRESULT HrHandleMkCal(WEBDAVPROP *lpsDavProp) = 0;
	virtual HRESULT HrHandlePropertySearch(WEBDAVRPTMGET *sWebRMGet, WEBDAVMULTISTATUS *sWebMStatus) = 0;
	virtual HRESULT HrHandlePropertySearchSet(WEBDAVMULTISTATUS *sWebMStatus) = 0;
	virtual HRESULT HrHandleDelete() = 0;

private:
	xmlDoc *m_lpXmlDoc;
	std::map <std::string,std::string> m_mapNs;

	HRESULT HrParseXml();

	/* more processing xml, but not as direct entrypoint */
	HRESULT HrHandleRptMulGet();
	HRESULT HrPropertySearch();
	HRESULT HrPropertySearchSet();
	HRESULT HrHandleRptCalQry();

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
protected:
	HRESULT HrSetDavPropName(WEBDAVPROPNAME *lpsDavPropName,std::string strPropName,std::string strNs);
	HRESULT HrSetDavPropName(WEBDAVPROPNAME *lpsDavPropName,std::string strPropName, std::string strPropAttribName, std::string strPropAttribValue, std::string strNs);

};

#endif
