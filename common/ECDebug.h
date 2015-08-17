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

#ifndef ECDEBUGTOOLS
#define ECDEBUGTOOLS

#include <string>
#include <sstream>
#include <mapi.h>
#include <mapix.h>
#include <mapicode.h>
#include <edkmdb.h>

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
	const char *szguidname;
};

std::string GetMAPIErrorDescription( HRESULT hResult );

std::string DBGGUIDToString(REFIID iid);
std::string MapiNameIdListToString(ULONG cNames, const MAPINAMEID *const *ppNames, const SPropTagArray *pptaga = NULL);
std::string MapiNameIdToString(const MAPINAMEID *pNameId);

std::string PropNameFromPropTagArray(const SPropTagArray *lpPropTagArray);
std::string PropNameFromPropArray(ULONG cValues, const SPropValue *lpPropArray);
std::string PropNameFromPropTag(ULONG ulPropTag);
std::string RestrictionToString(const SRestriction *lpRestriction, unsigned int indent=0);
std::string RowToString(const SRow *lpRow);
std::string RowSetToString(const SRowSet *lpRows);
std::string AdrRowSetToString(const ADRLIST *lpAdrList, const FlagList *lpFlagList);
std::string RowEntryToString(const ROWENTRY *lpRowEntry);
std::string RowListToString(const ROWLIST *lprowList);
std::string ActionToString(const ACTION *lpAction);

std::string SortOrderToString(const SSortOrder *lpSort);
std::string SortOrderSetToString(const SSortOrderSet *lpSortCriteria);

std::string NotificationToString(ULONG cNotification, const NOTIFICATION *lpNotification);

std::string ProblemArrayToString(const SPropProblemArray *lpProblemArray);
std::string unicodetostr(const wchar_t *lpszW);

const char *MsgServiceContextToString(ULONG ulContext);
const char *ResourceTypeToString(ULONG ulResourceType);

//Internal used only
std::string RelationalOperatorToString(ULONG relop);
std::string FuzzyLevelToString(ULONG ulFuzzyLevel);
std::string PropValueToString(const SPropValue *lpPropValue);

std::string ABFlags(ULONG ulFlag);
std::string EntryListToString(const ENTRYLIST *lpMsgList);
std::string PermissionRulesToString(ULONG cPermissions, const ECPERMISSION *lpECPermissions);

#endif
