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

#ifndef __M4L_MAPIDEFS_IMPL_H
#define __M4L_MAPIDEFS_IMPL_H

#include "m4l.common.h"
#include <mapidefs.h>
#include <mapispi.h>
#include <list>
#include <map>

using namespace std;

class M4LMsgServiceAdmin;

class M4LMAPIProp : public M4LUnknown, public IMAPIProp {
private:
    // variables
    list<LPSPropValue> properties;

public:
    M4LMAPIProp();
    virtual ~M4LMAPIProp();

    virtual HRESULT __stdcall GetLastError(HRESULT hResult, ULONG ulFlags, LPMAPIERROR* lppMAPIError);
    virtual HRESULT __stdcall SaveChanges(ULONG ulFlags);
    virtual HRESULT __stdcall GetProps(LPSPropTagArray lpPropTagArray, ULONG ulFlags, ULONG* lpcValues, LPSPropValue* lppPropArray);
    virtual HRESULT __stdcall GetPropList(ULONG ulFlags, LPSPropTagArray* lppPropTagArray);
    virtual HRESULT __stdcall OpenProperty(ULONG ulPropTag, LPCIID lpiid, ULONG ulInterfaceOptions, ULONG ulFlags, LPUNKNOWN* lppUnk);
    virtual HRESULT __stdcall SetProps(ULONG cValues, LPSPropValue lpPropArray, LPSPropProblemArray* lppProblems);
    virtual HRESULT __stdcall DeleteProps(LPSPropTagArray lpPropTagArray, LPSPropProblemArray* lppProblems);
    virtual HRESULT __stdcall CopyTo(ULONG ciidExclude, LPCIID rgiidExclude, LPSPropTagArray lpExcludeProps, ULONG ulUIParam,
			   LPMAPIPROGRESS lpProgress, LPCIID lpInterface, LPVOID lpDestObj, ULONG ulFlags,
			   LPSPropProblemArray* lppProblems);
    virtual HRESULT __stdcall CopyProps(LPSPropTagArray lpIncludeProps, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, LPCIID lpInterface,
			      LPVOID lpDestObj, ULONG ulFlags, LPSPropProblemArray* lppProblems);
    virtual HRESULT __stdcall GetNamesFromIDs(LPSPropTagArray* lppPropTags, LPGUID lpPropSetGuid, ULONG ulFlags, ULONG* lpcPropNames,
				    LPMAPINAMEID** lpppPropNames);
    virtual HRESULT __stdcall GetIDsFromNames(ULONG cPropNames, LPMAPINAMEID* lppPropNames, ULONG ulFlags, LPSPropTagArray* lppPropTags);

    // iunknown passthru
    virtual ULONG __stdcall AddRef();
    virtual ULONG __stdcall Release();
    virtual HRESULT __stdcall QueryInterface(REFIID refiid, void **lpvoid);
};


class M4LProfSect : public IProfSect, public M4LMAPIProp {
private:
	BOOL bGlobalProf;
public:
    M4LProfSect(BOOL bGlobalProf = FALSE);
    virtual ~M4LProfSect();

    virtual HRESULT __stdcall ValidateState(ULONG ulUIParam, ULONG ulFlags);
    virtual HRESULT __stdcall SettingsDialog(ULONG ulUIParam, ULONG ulFlags);
    virtual HRESULT __stdcall ChangePassword(LPTSTR lpOldPass, LPTSTR lpNewPass, ULONG ulFlags);
    virtual HRESULT __stdcall FlushQueues(ULONG ulUIParam, ULONG cbTargetTransport, LPENTRYID lpTargetTransport, ULONG ulFlags);

    // imapiprop passthru
    virtual HRESULT __stdcall GetLastError(HRESULT hResult, ULONG ulFlags, LPMAPIERROR* lppMAPIError);
    virtual HRESULT __stdcall SaveChanges(ULONG ulFlags);
    virtual HRESULT __stdcall GetProps(LPSPropTagArray lpPropTagArray, ULONG ulFlags, ULONG* lpcValues, LPSPropValue* lppPropArray);
    virtual HRESULT __stdcall GetPropList(ULONG ulFlags, LPSPropTagArray* lppPropTagArray);
    virtual HRESULT __stdcall OpenProperty(ULONG ulPropTag, LPCIID lpiid, ULONG ulInterfaceOptions, ULONG ulFlags, LPUNKNOWN* lppUnk);
    virtual HRESULT __stdcall SetProps(ULONG cValues, LPSPropValue lpPropArray, LPSPropProblemArray* lppProblems);
    virtual HRESULT __stdcall DeleteProps(LPSPropTagArray lpPropTagArray, LPSPropProblemArray* lppProblems);
    virtual HRESULT __stdcall CopyTo(ULONG ciidExclude, LPCIID rgiidExclude, LPSPropTagArray lpExcludeProps, ULONG ulUIParam,
			   LPMAPIPROGRESS lpProgress, LPCIID lpInterface, LPVOID lpDestObj, ULONG ulFlags,
			   LPSPropProblemArray* lppProblems);
    virtual HRESULT __stdcall CopyProps(LPSPropTagArray lpIncludeProps, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, LPCIID lpInterface,
			      LPVOID lpDestObj, ULONG ulFlags, LPSPropProblemArray* lppProblems);
    virtual HRESULT __stdcall GetNamesFromIDs(LPSPropTagArray* lppPropTags, LPGUID lpPropSetGuid, ULONG ulFlags, ULONG* lpcPropNames,
				    LPMAPINAMEID** lpppPropNames);
    virtual HRESULT __stdcall GetIDsFromNames(ULONG cPropNames, LPMAPINAMEID* lppPropNames, ULONG ulFlags, LPSPropTagArray* lppPropTags);

    // iunknown passthru
    virtual ULONG __stdcall AddRef();
    virtual ULONG __stdcall Release();
    virtual HRESULT __stdcall QueryInterface(REFIID refiid, void **lpvoid);
};


class M4LMAPITable : public M4LUnknown, public IMAPITable {
private:

public:
    M4LMAPITable();
    virtual ~M4LMAPITable();

    virtual HRESULT __stdcall GetLastError(HRESULT hResult, ULONG ulFlags, LPMAPIERROR *lppMAPIError);
    virtual HRESULT __stdcall Advise(ULONG ulEventMask, LPMAPIADVISESINK lpAdviseSink, ULONG * lpulConnection);
    virtual HRESULT __stdcall Unadvise(ULONG ulConnection);
    virtual HRESULT __stdcall GetStatus(ULONG *lpulTableStatus, ULONG *lpulTableType);
    virtual HRESULT __stdcall SetColumns(LPSPropTagArray lpPropTagArray, ULONG ulFlags);
    virtual HRESULT __stdcall QueryColumns(ULONG ulFlags, LPSPropTagArray *lpPropTagArray);
    virtual HRESULT __stdcall GetRowCount(ULONG ulFlags, ULONG *lpulCount);
    virtual HRESULT __stdcall SeekRow(BOOKMARK bkOrigin, LONG lRowCount, LONG *lplRowsSought);
    virtual HRESULT __stdcall SeekRowApprox(ULONG ulNumerator, ULONG ulDenominator);
    virtual HRESULT __stdcall QueryPosition(ULONG *lpulRow, ULONG *lpulNumerator, ULONG *lpulDenominator);
    virtual HRESULT __stdcall FindRow(LPSRestriction lpRestriction, BOOKMARK bkOrigin, ULONG ulFlags);
    virtual HRESULT __stdcall Restrict(LPSRestriction lpRestriction, ULONG ulFlags);
    virtual HRESULT __stdcall CreateBookmark(BOOKMARK* lpbkPosition);
    virtual HRESULT __stdcall FreeBookmark(BOOKMARK bkPosition);
    virtual HRESULT __stdcall SortTable(LPSSortOrderSet lpSortCriteria, ULONG ulFlags);
    virtual HRESULT __stdcall QuerySortOrder(LPSSortOrderSet *lppSortCriteria);
    virtual HRESULT __stdcall QueryRows(LONG lRowCount, ULONG ulFlags, LPSRowSet *lppRows);
    virtual HRESULT __stdcall Abort();
    virtual HRESULT __stdcall ExpandRow(ULONG cbInstanceKey, LPBYTE pbInstanceKey, ULONG ulRowCount,
			      ULONG ulFlags, LPSRowSet * lppRows, ULONG *lpulMoreRows);
    virtual HRESULT __stdcall CollapseRow(ULONG cbInstanceKey, LPBYTE pbInstanceKey, ULONG ulFlags, ULONG *lpulRowCount);
    virtual HRESULT __stdcall WaitForCompletion(ULONG ulFlags, ULONG ulTimeout, ULONG *lpulTableStatus);
    virtual HRESULT __stdcall GetCollapseState(ULONG ulFlags, ULONG cbInstanceKey, LPBYTE lpbInstanceKey, ULONG *lpcbCollapseState,
				     LPBYTE *lppbCollapseState);
    virtual HRESULT __stdcall SetCollapseState(ULONG ulFlags, ULONG cbCollapseState, LPBYTE pbCollapseState, BOOKMARK *lpbkLocation);

    // iunknown passthru
    virtual ULONG __stdcall AddRef();
    virtual ULONG __stdcall Release();
    virtual HRESULT __stdcall QueryInterface(REFIID refiid, void **lpvoid);
};


class M4LProviderAdmin : public M4LUnknown , public IProviderAdmin {
private:
	M4LMsgServiceAdmin* msa;
	char *szService;

public:
    M4LProviderAdmin(M4LMsgServiceAdmin* new_ma, char *szService);
    virtual ~M4LProviderAdmin();

    virtual HRESULT __stdcall GetLastError(HRESULT hResult, ULONG ulFlags, LPMAPIERROR* lppMAPIError);
    virtual HRESULT __stdcall GetProviderTable(ULONG ulFlags, LPMAPITABLE* lppTable);
    virtual HRESULT __stdcall CreateProvider(LPTSTR lpszProvider, ULONG cValues, LPSPropValue lpProps, ULONG ulUIParam,
								   ULONG ulFlags, MAPIUID* lpUID);
    virtual HRESULT __stdcall DeleteProvider(LPMAPIUID lpUID);
    virtual HRESULT __stdcall OpenProfileSection(LPMAPIUID lpUID, LPCIID lpInterface, ULONG ulFlags, LPPROFSECT* lppProfSect);

    // iunknown passthru
    virtual ULONG __stdcall AddRef();
    virtual ULONG __stdcall Release();
    virtual HRESULT __stdcall QueryInterface(REFIID refiid, void **lpvoid);
};


class M4LMAPIAdviseSink : public M4LUnknown, public IMAPIAdviseSink {
private:
    void *lpContext;
    LPNOTIFCALLBACK lpFn;

public:
    M4LMAPIAdviseSink(LPNOTIFCALLBACK lpFn, void *lpContext);
    virtual ~M4LMAPIAdviseSink();

    virtual ULONG __stdcall OnNotify(ULONG cNotif, LPNOTIFICATION lpNotifications);

    // iunknown passthru
    virtual ULONG __stdcall AddRef();
    virtual ULONG __stdcall Release();
    virtual HRESULT __stdcall QueryInterface(REFIID refiid, void **lpvoid);
};


/* for ABContainer */
class M4LMAPIContainer : public IMAPIContainer, public M4LMAPIProp {
public:
	M4LMAPIContainer();
    virtual ~M4LMAPIContainer();

    virtual HRESULT __stdcall GetContentsTable(ULONG ulFlags, LPMAPITABLE* lppTable);
    virtual HRESULT __stdcall GetHierarchyTable(ULONG ulFlags, LPMAPITABLE* lppTable);
    virtual HRESULT __stdcall OpenEntry(ULONG cbEntryID, LPENTRYID lpEntryID, LPCIID lpInterface, ULONG ulFlags, ULONG* lpulObjType, LPUNKNOWN* lppUnk);
    virtual HRESULT __stdcall SetSearchCriteria(LPSRestriction lpRestriction, LPENTRYLIST lpContainerList, ULONG ulSearchFlags);
    virtual HRESULT __stdcall GetSearchCriteria(ULONG ulFlags, LPSRestriction* lppRestriction, LPENTRYLIST* lppContainerList, ULONG* lpulSearchState);

    // imapiprop passthru
    virtual HRESULT __stdcall GetLastError(HRESULT hResult, ULONG ulFlags, LPMAPIERROR* lppMAPIError);
    virtual HRESULT __stdcall SaveChanges(ULONG ulFlags);
    virtual HRESULT __stdcall GetProps(LPSPropTagArray lpPropTagArray, ULONG ulFlags, ULONG* lpcValues, LPSPropValue* lppPropArray);
    virtual HRESULT __stdcall GetPropList(ULONG ulFlags, LPSPropTagArray* lppPropTagArray);
    virtual HRESULT __stdcall OpenProperty(ULONG ulPropTag, LPCIID lpiid, ULONG ulInterfaceOptions, ULONG ulFlags, LPUNKNOWN* lppUnk);
    virtual HRESULT __stdcall SetProps(ULONG cValues, LPSPropValue lpPropArray, LPSPropProblemArray* lppProblems);
    virtual HRESULT __stdcall DeleteProps(LPSPropTagArray lpPropTagArray, LPSPropProblemArray* lppProblems);
    virtual HRESULT __stdcall CopyTo(ULONG ciidExclude, LPCIID rgiidExclude, LPSPropTagArray lpExcludeProps, ULONG ulUIParam,
			   LPMAPIPROGRESS lpProgress, LPCIID lpInterface, LPVOID lpDestObj, ULONG ulFlags,
			   LPSPropProblemArray* lppProblems);
    virtual HRESULT __stdcall CopyProps(LPSPropTagArray lpIncludeProps, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, LPCIID lpInterface,
			      LPVOID lpDestObj, ULONG ulFlags, LPSPropProblemArray* lppProblems);
    virtual HRESULT __stdcall GetNamesFromIDs(LPSPropTagArray* lppPropTags, LPGUID lpPropSetGuid, ULONG ulFlags, ULONG* lpcPropNames,
				    LPMAPINAMEID** lpppPropNames);
    virtual HRESULT __stdcall GetIDsFromNames(ULONG cPropNames, LPMAPINAMEID* lppPropNames, ULONG ulFlags, LPSPropTagArray* lppPropTags);

    // iunknown passthru
    virtual ULONG __stdcall AddRef();
    virtual ULONG __stdcall Release();
    virtual HRESULT __stdcall QueryInterface(REFIID refiid, void **lpvoid);
};


typedef struct _s_abentry {
	MAPIUID muid;
	string displayname;
	LPABPROVIDER lpABProvider;
	LPABLOGON lpABLogon;
} abEntry;

class M4LABContainer : public IABContainer, public M4LMAPIContainer {
private:
	/*  */
	const std::list<abEntry> &m_lABEntries;

public:
	M4LABContainer(const std::list<abEntry> &lABEntries);
	virtual ~M4LABContainer();

    virtual HRESULT __stdcall CreateEntry(ULONG cbEntryID, LPENTRYID lpEntryID, ULONG ulCreateFlags, LPMAPIPROP* lppMAPIPropEntry);
    virtual HRESULT __stdcall CopyEntries(LPENTRYLIST lpEntries, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, ULONG ulFlags);
    virtual HRESULT __stdcall DeleteEntries(LPENTRYLIST lpEntries, ULONG ulFlags);
    virtual HRESULT __stdcall ResolveNames(LPSPropTagArray lpPropTagArray, ULONG ulFlags, LPADRLIST lpAdrList, LPFlagList lpFlagList);

	// imapicontainer passthu
    virtual HRESULT __stdcall GetContentsTable(ULONG ulFlags, LPMAPITABLE* lppTable);
    virtual HRESULT __stdcall GetHierarchyTable(ULONG ulFlags, LPMAPITABLE* lppTable);
    virtual HRESULT __stdcall OpenEntry(ULONG cbEntryID, LPENTRYID lpEntryID, LPCIID lpInterface, ULONG ulFlags, ULONG* lpulObjType, LPUNKNOWN* lppUnk);
    virtual HRESULT __stdcall SetSearchCriteria(LPSRestriction lpRestriction, LPENTRYLIST lpContainerList, ULONG ulSearchFlags);
    virtual HRESULT __stdcall GetSearchCriteria(ULONG ulFlags, LPSRestriction* lppRestriction, LPENTRYLIST* lppContainerList, ULONG* lpulSearchState);

    // imapiprop passthru
    virtual HRESULT __stdcall GetLastError(HRESULT hResult, ULONG ulFlags, LPMAPIERROR* lppMAPIError);
    virtual HRESULT __stdcall SaveChanges(ULONG ulFlags);
    virtual HRESULT __stdcall GetProps(LPSPropTagArray lpPropTagArray, ULONG ulFlags, ULONG* lpcValues, LPSPropValue* lppPropArray);
    virtual HRESULT __stdcall GetPropList(ULONG ulFlags, LPSPropTagArray* lppPropTagArray);
    virtual HRESULT __stdcall OpenProperty(ULONG ulPropTag, LPCIID lpiid, ULONG ulInterfaceOptions, ULONG ulFlags, LPUNKNOWN* lppUnk);
    virtual HRESULT __stdcall SetProps(ULONG cValues, LPSPropValue lpPropArray, LPSPropProblemArray* lppProblems);
    virtual HRESULT __stdcall DeleteProps(LPSPropTagArray lpPropTagArray, LPSPropProblemArray* lppProblems);
    virtual HRESULT __stdcall CopyTo(ULONG ciidExclude, LPCIID rgiidExclude, LPSPropTagArray lpExcludeProps, ULONG ulUIParam,
			   LPMAPIPROGRESS lpProgress, LPCIID lpInterface, LPVOID lpDestObj, ULONG ulFlags,
			   LPSPropProblemArray* lppProblems);
    virtual HRESULT __stdcall CopyProps(LPSPropTagArray lpIncludeProps, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, LPCIID lpInterface,
			      LPVOID lpDestObj, ULONG ulFlags, LPSPropProblemArray* lppProblems);
    virtual HRESULT __stdcall GetNamesFromIDs(LPSPropTagArray* lppPropTags, LPGUID lpPropSetGuid, ULONG ulFlags, ULONG* lpcPropNames,
				    LPMAPINAMEID** lpppPropNames);
    virtual HRESULT __stdcall GetIDsFromNames(ULONG cPropNames, LPMAPINAMEID* lppPropNames, ULONG ulFlags, LPSPropTagArray* lppPropTags);

    // iunknown passthru
    virtual ULONG __stdcall AddRef();
    virtual ULONG __stdcall Release();
    virtual HRESULT __stdcall QueryInterface(REFIID refiid, void **lpvoid);
};

#endif
