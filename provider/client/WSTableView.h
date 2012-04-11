/*
 * Copyright 2005 - 2009  Zarafa B.V.
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

/*
	class WSTableView
*/

#ifndef WSTABLEVIEW_H
#define WSTABLEVIEW_H

#include "ECUnknown.h"
#include "Zarafa.h"

#include "ZarafaCode.h"
#include <mapi.h>
#include <mapispi.h>

#include <pthread.h>
#include "soapZarafaCmdProxy.h"
class WSTransport;

typedef HRESULT (*RELOADCALLBACK)(void *lpParam);

class WSTableView : public ECUnknown
{
protected:
	WSTableView(ULONG ulType, ULONG ulFlags, ZarafaCmd *lpCmd, pthread_mutex_t mDataLock, ECSESSIONID ecSessionId, ULONG cbEntryId, LPENTRYID lpEntryId, WSTransport *lpTransport, char *szClassName = NULL);
	virtual ~WSTableView();

public:
	virtual	HRESULT	QueryInterface(REFIID refiid, void **lppInstanceID);

	virtual HRESULT HrOpenTable();
	virtual HRESULT HrCloseTable();

	// You must call HrOpenTable before calling the following methods
	virtual HRESULT HrSetColumns(LPSPropTagArray lpsPropTagArray);
	virtual HRESULT HrFindRow(LPSRestriction lpsRestriction, BOOKMARK bkOrigin, ULONG ulFlags);
	virtual HRESULT HrQueryColumns(ULONG ulFlags, LPSPropTagArray *lppsPropTags);
	virtual HRESULT HrSortTable(LPSSortOrderSet lpsSortOrderSet);
	virtual HRESULT HrRestrict(LPSRestriction lpsRestriction);
	virtual HRESULT HrQueryRows(ULONG ulRowCount, ULONG ulFlags, LPSRowSet *lppRowSet);
	virtual HRESULT HrGetRowCount(ULONG *lpulRowCount, ULONG *lpulCurrentRow);
	virtual HRESULT HrSeekRow(BOOKMARK bkOrigin, LONG ulRows, LONG *lplRowsSought);
	virtual HRESULT HrExpandRow(ULONG cbInstanceKey, LPBYTE pbInstanceKey, ULONG ulRowCount, ULONG ulFlags, LPSRowSet * lppRows, ULONG *lpulMoreRows);
	virtual HRESULT HrCollapseRow(ULONG cbInstanceKey, LPBYTE pbInstanceKey, ULONG ulFlags, ULONG *lpulRowCount);
	virtual HRESULT HrGetCollapseState(BYTE **lppCollapseState, ULONG *lpcbCollapseState, BYTE *lpbInstanceKey, ULONG cbInstanceKey);
	virtual HRESULT HrSetCollapseState(BYTE *lpCollapseState, ULONG cbCollapseState, BOOKMARK *lpbkPosition);

	virtual HRESULT HrMulti(ULONG ulDeferredFlags, LPSPropTagArray lpsPropTagArray, LPSRestriction lpsRestriction, LPSSortOrderSet lpsSortOrderSet, ULONG ulRowCount, ULONG ulFlags, LPSRowSet *lppRowSet);

	virtual HRESULT FreeBookmark(BOOKMARK bkPosition);
	virtual HRESULT CreateBookmark(BOOKMARK* lpbkPosition);

	static HRESULT Reload(void *lpParam, ECSESSIONID sessionID);
	virtual HRESULT SetReloadCallback(RELOADCALLBACK callback, void *lpParam);

public:
	ULONG		ulTableId;

protected:
	virtual HRESULT LockSoap();
	virtual HRESULT UnLockSoap();

protected:
	ZarafaCmd*		lpCmd;
	pthread_mutex_t	mDataLock;
	ECSESSIONID		ecSessionId;
	entryId			m_sEntryId;
	void *			m_lpProvider;
	ULONG			m_ulTableType;
	ULONG			m_ulSessionReloadCallback;
	WSTransport*	m_lpTransport;

	LPSPropTagArray m_lpsPropTagArray;
	LPSSortOrderSet m_lpsSortOrderSet;
	LPSRestriction	m_lpsRestriction;

	ULONG		ulFlags;
	ULONG		ulType;

	void *			m_lpParam;
	RELOADCALLBACK  m_lpCallback;
};

#endif
