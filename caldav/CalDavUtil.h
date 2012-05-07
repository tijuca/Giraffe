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

#ifndef _UTIL_H_
#define _UTIL_H_

#include "WebDav.h"
#include "mapiext.h"
#include "mapiguidext.h"
#include <string.h>
#include <algorithm>
#include "restrictionutil.h"
#include "namedprops.h"
#include "nameids.h"
#include "icaluid.h"
#include "edkguid.h"
#include "freebusy.h"
#include "ECFreeBusySupport.h"
#include "MAPIToICal.h"

#define FOLDER_PREFIX L"FLDPRFX_"

// folder type according to URL
#define SINGLE_FOLDER	0x01
#define DEFAULT_FOLDER	0x02
#define SHARED_FOLDER	0x04

// folder type according to content
#define OTHER_FOLDER	0x01
#define CALENDAR_FOLDER 0x02
#define TASKS_FOLDER	0x03

//Performs login to the Zarafa server and returns Session.
HRESULT HrAuthenticate(const std::wstring &wstrUser, const std::wstring &wstrPass, std::string strPath, IMAPISession **lpSession);

//Opens user's folder specified in path.
//lpSharedStore can be NULL for accessing users own folders.
HRESULT HrOpenUserFld(IMsgStore *lpSharedStore, std::wstring wstrFolderPath, bool blCreateIfNF, bool blIsPublic, ECLogger *lpLogger, IMAPIFolder **lpUsrFld);

//Adds property FolderID to the folder if not present else returns it.
HRESULT HrAddProperty(IMsgStore *lpMsgStore, SBinary sbEid, ULONG ulPropertyId, bool bIsFldID, std::wstring *wstrProperty);

//Adds property FolderID && dispidApptTsRef to the folder & message respectively, if not present else returns it.
HRESULT HrAddProperty(IMAPIProp *lpMapiProp, ULONG ulPropertyId, bool bIsFldID, std::wstring *wstrProperty);

//Finds folder from hierarchy table refering to the Folder ID
HRESULT HrFindFolder(IMsgStore *lpMsgStore, LPSPropTagArray lpNamedProps, ECLogger *lpLogger, bool blIsPublic, std::wstring wstrFldId,IMAPIFolder **lppUsrFld);

//Adds data to structure for acl request.
HRESULT HrBuildACL(WEBDAVPROPERTY *lpsProperty);

// Generate supported report set.
HRESULT HrBuildReportSet(WEBDAVPROPERTY *lpsProperty);

//Retrieve the User's Email address.
HRESULT HrGetUserInfo(IMAPISession *lpSession, IMsgStore *lpDefStore, std::string *strEmailaddresss, std::wstring *strUserName, IMailUser **lppMailUser);

//Check if the input value is a Timestamp
//eg. 20092020T121212Z
bool IsValueTs(std::string strValue);

//Strip the input to get Guid Value
//eg input: caldav/Calendar/ai-43873034lakljk403-3245.ics
//return: ai-43873034lakljk403-3245
std::string StripGuid(const std::wstring &wstrInput);
//Returns Calendars of Folder and sorted by PR_ENTRY_ID.
HRESULT HrGetSubCalendars(IMAPISession *lpSession, IMAPIFolder *lpFolderIn, SBinary *lpsbEid, IMAPITable **lppTable);
//Open Shared Folders
HRESULT HrGetSharedFolder(IMAPISession *lpSession, IMsgStore *lpUsrStore, std::wstring wstrUser, std::wstring wstrFolderPath, bool blIsClMac, bool blCreateIfNF, ECLogger *lpLogger, ULONG *ulFolderFlag, IMsgStore **lppSharedStore, IMAPIFolder **lppUsrFld);
// Checks for private message.
bool IsPrivate(LPMESSAGE lpMessage, ULONG ulPropIDPrivate);

bool HasDelegatePerm(IMsgStore *lpDefStore, IMsgStore *lpSharedStore);

HRESULT HrMakeRestriction (const std::string &strGuid, LPSPropTagArray lpNamedProps, LPSRestriction *lpsRectrict);

HRESULT HrFindAndGetMessage(std::string strGuid, IMAPIFolder *lpUsrFld, LPSPropTagArray lpNamedProps, IMessage **lppMessage);

HRESULT HrGetUserEid(IMAPITable *lpGabTable, const std::wstring &wstrUser, SBinary *lpsbEid);

HRESULT HrGetFreebusy(MapiToICal *lpMapiToIcal, IFreeBusySupport* lpFBSupport, IMAPITable *lpGabTable, std::list<std::string> *lplstUsers, WEBDAVFBINFO *lpFbInfo);

#endif
