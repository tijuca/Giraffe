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

#ifndef _CALDAVPROTO_H_
#define _CALDAVPROTO_H_

#include "WebDav.h"
#include "CalDavUtil.h"
#include <libxml/uri.h>

#include "restrictionutil.h"
#include "mapiext.h"
#include "MAPIToICal.h"
#include "ICalToMAPI.h"
#include "icaluid.h"

#define FB_PUBLISH_DURATION 6

class CalDAV : public WebDav
{
public:
	CalDAV(Http *lpRequest, IMAPISession *lpSession, ECLogger *lpLogger, std::string strSrvTz, std::string strCharset);
	virtual ~CalDAV();

	HRESULT HrHandleCommand(const std::string &strMethod);

protected:
	/* entry points in webdav class */
	/* HRESULT HrPropfind(); */
	/* HRESULT HrReport(); */
	/* HRESULT HrMkCalendar(); */
	/* HRESULT HrPropPatch(); */
	HRESULT HrPut();
	HRESULT HrMove();
	HRESULT HrHandleMeeting(ICalToMapi *lpIcalToMapi);
	HRESULT HrHandleFreebusy(ICalToMapi *lpIcalToMapi);

	virtual HRESULT HrHandlePropfind(WEBDAVREQSTPROPS *sDavProp, WEBDAVMULTISTATUS *lpsDavMulStatus);
	virtual HRESULT HrListCalEntries(WEBDAVREQSTPROPS *sWebRCalQry,WEBDAVMULTISTATUS *sWebMStatus);	// Used By both PROPFIND & Report Calendar-query
	virtual	HRESULT HrHandleReport(WEBDAVRPTMGET *sWebRMGet, WEBDAVMULTISTATUS *sWebMStatus);
	virtual HRESULT HrHandlePropPatch(WEBDAVPROP *lpsDavProp, WEBDAVMULTISTATUS *sWebMStatus);
	virtual HRESULT HrHandleMkCal(WEBDAVPROP *lpsDavProp);
	virtual HRESULT HrHandlePropertySearch(WEBDAVRPTMGET *sWebRMGet, WEBDAVMULTISTATUS *sWebMStatus);
	virtual HRESULT HrHandlePropertySearchSet(WEBDAVMULTISTATUS *sWebMStatus);
	virtual HRESULT HrHandleDelete();
	HRESULT HrHandlePost();

private:
	HRESULT HrMoveEntry(const std::string &strGuid, LPMAPIFOLDER lpDestFolder);

	HRESULT HrHandlePropfindRoot(WEBDAVREQSTPROPS *sDavProp, WEBDAVMULTISTATUS *lpsDavMulStatus);	
	
	HRESULT CreateAndGetGuid(SBinary sbEid, ULONG ulPropTag, std::string *lpstrGuid);
	HRESULT HrListCalendar(WEBDAVREQSTPROPS *sDavProp, WEBDAVMULTISTATUS *lpsMulStatus);

	HRESULT HrConvertToIcal(LPSPropValue lpEid, MapiToICal *lpMtToIcal, ULONG ulFlags, std::string *strIcal);
	HRESULT HrMapValtoStruct(LPMAPIPROP lpObj, LPSPropValue lpProps, ULONG ulPropCount, MapiToICal *lpMtIcal, ULONG ulFlags, bool bPropsFirst, std::list<WEBDAVPROPERTY> *lstDavProps, WEBDAVRESPONSE *lpsResponse);
	HRESULT	HrGetCalendarOrder(SBinary sbEid, std::string *lpstrCalendarOrder);
};

#endif
