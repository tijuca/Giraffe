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

#ifndef ECRULESTABLEPROXY_INCLUDED
#define ECRULESTABLEPROXY_INCLUDED

#include "ECUnknown.h"

#include <mapidefs.h>

class ECRulesTableProxy : public ECUnknown {
protected:
	ECRulesTableProxy(LPMAPITABLE lpTable);
	virtual ~ECRulesTableProxy();
    
public:
	static  HRESULT Create(LPMAPITABLE lpTable, ECRulesTableProxy **lppRulesTableProxy);

	virtual HRESULT QueryInterface(REFIID refiid, void **lppInterface);

	virtual HRESULT GetLastError(HRESULT hResult, ULONG ulFlags, LPMAPIERROR *lppMAPIError);
	virtual HRESULT Advise(ULONG ulEventMask, LPMAPIADVISESINK lpAdviseSink, ULONG_PTR *lpulConnection);
	virtual HRESULT Unadvise(ULONG_PTR ulConnection);
	virtual HRESULT GetStatus(ULONG *lpulTableStatus, ULONG *lpulTableType);
	virtual HRESULT SetColumns(LPSPropTagArray lpPropTagArray, ULONG ulFlags);
	virtual HRESULT QueryColumns(ULONG ulFlags, LPSPropTagArray *lpPropTagArray);
	virtual HRESULT GetRowCount(ULONG ulFlags, ULONG *lpulCount);
	virtual HRESULT SeekRow(BOOKMARK bkOrigin, LONG lRowCount, LONG *lplRowsSought) ;
	virtual HRESULT SeekRowApprox(ULONG ulNumerator, ULONG ulDenominator);
	virtual HRESULT QueryPosition(ULONG *lpulRow, ULONG *lpulNumerator, ULONG *lpulDenominator);
	virtual HRESULT FindRow(LPSRestriction lpRestriction, BOOKMARK bkOrigin, ULONG ulFlags);
	virtual HRESULT Restrict(LPSRestriction lpRestriction, ULONG ulFlags);
	virtual HRESULT CreateBookmark(BOOKMARK* lpbkPosition);
	virtual HRESULT FreeBookmark(BOOKMARK bkPosition);
	virtual HRESULT SortTable(LPSSortOrderSet lpSortCriteria, ULONG ulFlags);
	virtual HRESULT QuerySortOrder(LPSSortOrderSet *lppSortCriteria);
	virtual HRESULT QueryRows(LONG lRowCount, ULONG ulFlags, LPSRowSet *lppRows);
	virtual HRESULT Abort();
	virtual HRESULT ExpandRow(ULONG cbInstanceKey, LPBYTE pbInstanceKey, ULONG ulRowCount, ULONG ulFlags, LPSRowSet * lppRows, ULONG *lpulMoreRows);
	virtual HRESULT CollapseRow(ULONG cbInstanceKey, LPBYTE pbInstanceKey, ULONG ulFlags, ULONG *lpulRowCount);
	virtual HRESULT WaitForCompletion(ULONG ulFlags, ULONG ulTimeout, ULONG *lpulTableStatus);
	virtual HRESULT GetCollapseState(ULONG ulFlags, ULONG cbInstanceKey, LPBYTE lpbInstanceKey, ULONG *lpcbCollapseState, LPBYTE *lppbCollapseState);
	virtual HRESULT SetCollapseState(ULONG ulFlags, ULONG cbCollapseState, LPBYTE pbCollapseState, BOOKMARK *lpbkLocation);

protected:
	class xMAPITable : public IMAPITable {
		// IUnknown
		virtual ULONG __stdcall AddRef();
		virtual ULONG __stdcall Release();
		virtual HRESULT __stdcall QueryInterface(REFIID refiid, void **lppInterface);

		// From IMAPITable
		virtual HRESULT __stdcall GetLastError(HRESULT hResult, ULONG ulFlags, LPMAPIERROR *lppMAPIError);
		virtual HRESULT __stdcall Advise(ULONG ulEventMask, LPMAPIADVISESINK lpAdviseSink, ULONG_PTR * lpulConnection);
		virtual HRESULT __stdcall Unadvise(ULONG_PTR ulConnection);
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
		virtual HRESULT __stdcall ExpandRow(ULONG cbInstanceKey, LPBYTE pbInstanceKey, ULONG ulRowCount, ULONG ulFlags, LPSRowSet * lppRows, ULONG *lpulMoreRows);
		virtual HRESULT __stdcall CollapseRow(ULONG cbInstanceKey, LPBYTE pbInstanceKey, ULONG ulFlags, ULONG *lpulRowCount);
		virtual HRESULT __stdcall WaitForCompletion(ULONG ulFlags, ULONG ulTimeout, ULONG *lpulTableStatus);
		virtual HRESULT __stdcall GetCollapseState(ULONG ulFlags, ULONG cbInstanceKey, LPBYTE lpbInstanceKey, ULONG *lpcbCollapseState, LPBYTE *lppbCollapseState);
		virtual HRESULT __stdcall SetCollapseState(ULONG ulFlags, ULONG cbCollapseState, LPBYTE pbCollapseState, BOOKMARK *lpbkLocation);
	} m_xMAPITable;

private:
	LPMAPITABLE m_lpTable;
};

#endif // ndef ECRULESTABLEPROXY_INCLUDED
