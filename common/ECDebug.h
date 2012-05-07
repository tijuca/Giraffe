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

#ifndef ECDEBUGTOOLS
#define ECDEBUGTOOLS

#include <string>
#include <mapi.h>
#include <mapix.h>
#include <mapicode.h>

#include "ECDefs.h"

#ifndef DEBUGBUFSIZE
#define DEBUGBUFSIZE	1024
#endif

struct MAPIResultCodes
{
	HRESULT		hResult;
	const char* error;
};

struct INFOGUID {
	int		ulType; //0=mapi,1=exchange,2=new,3=zarafa,4=windows/other, 10=ontdekte
	GUID	*guid;
	char*	szguidname;
};

std::string GetMAPIErrorDescription( HRESULT hResult );

std::string DBGGUIDToString(REFIID iid);
std::string MapiNameIdListToString(ULONG cNames, LPMAPINAMEID * ppNames, LPSPropTagArray pptaga = NULL);
std::string MapiNameIdToString(LPMAPINAMEID pNameId);

std::string PropNameFromPropTagArray(LPSPropTagArray lpPropTagArray);
std::string PropNameFromPropArray(ULONG cValues, LPSPropValue lpPropArray);
std::string PropNameFromPropTag(ULONG ulPropTag);
std::string RestrictionToString(LPSRestriction lpRestriction, unsigned int indent=0);
std::string RowToString(LPSRow lpRow);
std::string RowSetToString(LPSRowSet lpRows);
std::string AdrRowSetToString(LPADRLIST lpAdrList, LPFlagList lpFlagList);

std::string SortOrderToString(LPSSortOrder lpSort);
std::string SortOrderSetToString(LPSSortOrderSet lpSortCriteria);

std::string NotificationToString(ULONG cNotification, LPNOTIFICATION lpNotification);

std::string ProblemArrayToString(LPSPropProblemArray lpProblemArray);
std::string unicodetostr(WCHAR *lpszW);

const char *MsgServiceContextToString(ULONG ulContext);
const char *ResourceTypeToString(ULONG ulResourceType);

//Internal used only
std::string RelationalOperatorToString(ULONG relop);
std::string FuzzyLevelToString(ULONG ulFuzzyLevel);
std::string PropValueToString(LPSPropValue  lpPropValue);

std::string ABFlags(ULONG ulFlag);
std::string EntryListToString(LPENTRYLIST lpMsgList);
std::string PermissionRulesToString(ULONG cPermissions, LPECPERMISSION lpECPermissions);

#endif
