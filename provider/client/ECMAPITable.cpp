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
#include <new>
#include <kopano/platform.h>
#include <kopano/lockhelper.hpp>
#include <mapicode.h>
#include <mapidefs.h>
#include <mapitags.h>
#include <mapiguid.h>
#include <mapiutil.h>

#include "Mem.h"
#include "ECMAPITable.h"
#include <edkguid.h>
#include <kopano/ECGuid.h>
#include <kopano/Util.h>

#include <kopano/ECDebug.h>

using namespace KCHL;

ECMAPITable::ECMAPITable(const std::string &strName,
    ECNotifyClient *lpNotifyClient, ULONG ulFlags) :
	ECUnknown("IMAPITable")
{
	this->lpNotifyClient = lpNotifyClient;
	
	if(this->lpNotifyClient)
		this->lpNotifyClient->AddRef();

	this->ulFlags = ulFlags;
	this->lpTableOps = NULL;
	
	m_ulRowCount = 0;
	m_ulFlags = 0;
	m_ulDeferredFlags = 0;
	m_strName = strName;
}

HRESULT ECMAPITable::FlushDeferred(LPSRowSet *lppRowSet)
{
	auto hr = lpTableOps->HrOpenTable();
	if(hr != hrSuccess)
		return hr;

	// No deferred calls -> nothing to do
	if (!IsDeferred())
		return hr;
        
	hr = lpTableOps->HrMulti(m_ulDeferredFlags, m_lpSetColumns, m_lpRestrict, m_lpSortTable, m_ulRowCount, m_ulFlags, lppRowSet);

	// Reset deferred items
	m_lpSetColumns.reset();
	m_lpRestrict.reset();
	m_lpSortTable.reset();
	m_ulRowCount = 0;
	m_ulFlags = 0;
	m_ulDeferredFlags = 0;
	return hr;
}

BOOL ECMAPITable::IsDeferred()
{
	return m_lpSetColumns != NULL || m_lpRestrict != NULL ||
	       m_lpSortTable != NULL || m_ulRowCount != 0 ||
	       m_ulFlags != 0 || m_ulDeferredFlags != 0;
}

ECMAPITable::~ECMAPITable()
{
	// Remove all advises	
	auto iterMapInt = m_ulConnectionList.cbegin();
	while (iterMapInt != m_ulConnectionList.cend()) {
		auto iterMapIntDel = iterMapInt;
		++iterMapInt;
		Unadvise(*iterMapIntDel);
	}

	if(lpNotifyClient)
		lpNotifyClient->Release();

	if(lpTableOps)
		lpTableOps->Release();	// closes the table on the server too
}

HRESULT ECMAPITable::Create(const std::string &strName,
    ECNotifyClient *lpNotifyClient, ULONG ulFlags, ECMAPITable **lppECMAPITable)
{
	return alloc_wrap<ECMAPITable>(strName, lpNotifyClient, ulFlags)
	       .put(lppECMAPITable);
}

HRESULT ECMAPITable::QueryInterface(REFIID refiid, void **lppInterface)
{
	REGISTER_INTERFACE2(ECMAPITable, this);
	REGISTER_INTERFACE2(ECUnknown, this);
	REGISTER_INTERFACE2(IMAPITable, this);
	REGISTER_INTERFACE2(IUnknown, this);
	return MAPI_E_INTERFACE_NOT_SUPPORTED;
}

HRESULT ECMAPITable::GetLastError(HRESULT hResult, ULONG ulFlags, LPMAPIERROR *lppMAPIError)
{
	return MAPI_E_NO_SUPPORT;
}

HRESULT ECMAPITable::Advise(ULONG ulEventMask, LPMAPIADVISESINK lpAdviseSink, ULONG * lpulConnection)
{
	scoped_rlock lock(m_hLock);

	HRESULT hr = FlushDeferred();
	if(hr != hrSuccess)
		return hr;
	if (lpNotifyClient == NULL)
		return MAPI_E_NO_SUPPORT;
	if (lpulConnection == NULL)
		return MAPI_E_INVALID_PARAMETER;

	// FIXME: if a reconnection happens in another thread during the following call, the ulTableId sent here will be incorrect. The reconnection
	// code will not yet know about this connection since we don't insert it until later, so you may end up getting an Advise() on completely the wrong
	// table. 

	hr = lpNotifyClient->Advise(4, (BYTE *)&lpTableOps->ulTableId, ulEventMask, lpAdviseSink, lpulConnection);
	if(hr != hrSuccess)
		return hr;

	// We lock the connection list separately
	scoped_rlock l_conn(m_hMutexConnectionList);
	m_ulConnectionList.emplace(*lpulConnection);
	return hrSuccess;
}

HRESULT ECMAPITable::Unadvise(ULONG ulConnection)
{
	scoped_rlock lock(m_hLock);

	HRESULT hr = FlushDeferred();
	if(hr != hrSuccess)
		return hr;
	if (lpNotifyClient == NULL)
		return MAPI_E_NO_SUPPORT;

	ulock_rec l_conn(m_hMutexConnectionList);
	m_ulConnectionList.erase(ulConnection);
	l_conn.unlock();
	lpNotifyClient->Unadvise(ulConnection);
	return hr;
}

// @fixme Do we need to lock here or just update the status?
HRESULT ECMAPITable::GetStatus(ULONG *lpulTableStatus, ULONG *lpulTableType)
{
	HRESULT hr = hrSuccess;

	*lpulTableStatus = TBLSTAT_COMPLETE;
	*lpulTableType = TBLTYPE_DYNAMIC;

	return hr;
}

HRESULT ECMAPITable::SetColumns(const SPropTagArray *lpPropTagArray,
    ULONG ulFlags)
{
	if(lpPropTagArray == NULL || lpPropTagArray->cValues == 0)
		return MAPI_E_INVALID_PARAMETER;

	scoped_rlock lock(m_hLock);

	HRESULT hr = MAPIAllocateBuffer(CbNewSPropTagArray(lpPropTagArray->cValues), &~m_lpSetColumns);
	if (hr != hrSuccess)
		return hr;
        
    m_lpSetColumns->cValues = lpPropTagArray->cValues;
    memcpy(&m_lpSetColumns->aulPropTag, &lpPropTagArray->aulPropTag, lpPropTagArray->cValues * sizeof(ULONG));

	if (!(ulFlags & TBL_BATCH))
		hr = FlushDeferred();
	return hr;
}

HRESULT ECMAPITable::QueryColumns(ULONG ulFlags, LPSPropTagArray *lppPropTagArray)
{
	scoped_rlock lock(m_hLock);

	HRESULT hr = FlushDeferred();
	if(hr != hrSuccess)
		return hr;

	// FIXME if the client has done SetColumns, we can handle this
	// call locally instead of querying the server (unless TBL_ALL_COLUMNS has been
	// specified)
	return this->lpTableOps->HrQueryColumns(ulFlags, lppPropTagArray);
}

HRESULT ECMAPITable::GetRowCount(ULONG ulFlags, ULONG *lpulCount)
{
	scoped_rlock lock(m_hLock);

	HRESULT hr = FlushDeferred();
	if(hr != hrSuccess)
		return hr;
	ULONG ulRow = 0; // discarded
	return this->lpTableOps->HrGetRowCount(lpulCount, &ulRow);
}

HRESULT ECMAPITable::SeekRow(BOOKMARK bkOrigin, LONG lRowCount, LONG *lplRowsSought)
{
	scoped_rlock lock(m_hLock);

	HRESULT hr = FlushDeferred();
	if (hr != hrSuccess)
		return hr;
	return this->lpTableOps->HrSeekRow(bkOrigin, lRowCount, lplRowsSought);
}

HRESULT ECMAPITable::SeekRowApprox(ULONG ulNumerator, ULONG ulDenominator)
{
	scoped_rlock lock(m_hLock);

	HRESULT hr = FlushDeferred();
	if(hr != hrSuccess)
		return hr;
	ULONG ulRows = 0;
	ULONG ulCurrent = 0;
	hr = lpTableOps->HrGetRowCount(&ulRows, &ulCurrent);
	if(hr != hrSuccess)
		return hr;
	return SeekRow(BOOKMARK_BEGINNING, static_cast<ULONG>(static_cast<double>(ulRows) * (static_cast<double>(ulNumerator) / ulDenominator)), NULL);
}

HRESULT ECMAPITable::QueryPosition(ULONG *lpulRow, ULONG *lpulNumerator, ULONG *lpulDenominator)
{
	scoped_rlock lock(m_hLock);

	HRESULT hr = FlushDeferred();
	if(hr != hrSuccess)
		return hr;
	ULONG ulRows = 0;
	ULONG ulCurrentRow = 0;
	hr = lpTableOps->HrGetRowCount(&ulRows, &ulCurrentRow);
	if(hr != hrSuccess)
		return hr;

	*lpulRow = ulCurrentRow;
	*lpulNumerator = ulCurrentRow;
	*lpulDenominator = (ulRows == 0)?1:ulRows;
	return hr;
}

HRESULT ECMAPITable::FindRow(const SRestriction *lpRestriction,
    BOOKMARK bkOrigin, ULONG ulFlags)
{
	if (lpRestriction == NULL)
		return MAPI_E_INVALID_PARAMETER;

	scoped_rlock lock(m_hLock);
	HRESULT hr = FlushDeferred();
	if(hr != hrSuccess)
		return hr;
	return this->lpTableOps->HrFindRow(lpRestriction, bkOrigin, ulFlags);
}

HRESULT ECMAPITable::Restrict(const SRestriction *lpRestriction, ULONG ulFlags)
{
	HRESULT hr = hrSuccess;

	scoped_rlock lock(m_hLock);
    if(lpRestriction) {
        if ((hr = MAPIAllocateBuffer(sizeof(SRestriction), &~m_lpRestrict)) != hrSuccess)
			return hr;
        
        hr = Util::HrCopySRestriction(m_lpRestrict, lpRestriction, m_lpRestrict);

		m_ulDeferredFlags &= ~TABLE_MULTI_CLEAR_RESTRICTION;
    } else {
		// setting the restriction to NULL is not the same as not setting the restriction at all
		m_ulDeferredFlags |= TABLE_MULTI_CLEAR_RESTRICTION;
        m_lpRestrict.reset();
    }
	if (!(ulFlags & TBL_BATCH))
		hr = FlushDeferred();
	return hr;
}

HRESULT ECMAPITable::CreateBookmark(BOOKMARK* lpbkPosition)
{
	scoped_rlock lock(m_hLock);

	HRESULT hr = FlushDeferred();
	if(hr != hrSuccess)
		return hr;
	return this->lpTableOps->CreateBookmark(lpbkPosition);
}

HRESULT ECMAPITable::FreeBookmark(BOOKMARK bkPosition)
{
	scoped_rlock lock(m_hLock);

	HRESULT hr = FlushDeferred();
	if(hr != hrSuccess)
		return hr;
	return this->lpTableOps->FreeBookmark(bkPosition);
}

HRESULT ECMAPITable::SortTable(const SSortOrderSet *lpSortCriteria,
    ULONG ulFlags)
{
	if (lpSortCriteria == NULL)
		return MAPI_E_INVALID_PARAMETER;

	scoped_rlock lock(m_hLock);

	HRESULT hr = MAPIAllocateBuffer(CbSSortOrderSet(lpSortCriteria), &~lpsSortOrderSet);
	if (hr != hrSuccess)
		return hr;

	memcpy(lpsSortOrderSet, lpSortCriteria, CbSSortOrderSet(lpSortCriteria));
	hr = MAPIAllocateBuffer(CbSSortOrderSet(lpSortCriteria), &~m_lpSortTable);
	if (hr != hrSuccess)
		return hr;
    memcpy(m_lpSortTable, lpSortCriteria, CbSSortOrderSet(lpSortCriteria));

	if (!(ulFlags & TBL_BATCH))
		hr = FlushDeferred();
	return hr;
}

HRESULT ECMAPITable::QuerySortOrder(LPSSortOrderSet *lppSortCriteria)
{
	memory_ptr<SSortOrderSet> lpSortCriteria;
	scoped_rlock lock(m_hLock);

	HRESULT hr = FlushDeferred();
	if(hr != hrSuccess)
		return hr;

	if(lpsSortOrderSet)
		hr = ECAllocateBuffer(CbSSortOrderSet(lpsSortOrderSet), &~lpSortCriteria);
	else
		hr = ECAllocateBuffer(CbNewSSortOrderSet(0), &~lpSortCriteria);

	if(hr != hrSuccess)
		return hr;
	if(lpsSortOrderSet)
		memcpy(lpSortCriteria, lpsSortOrderSet, CbSSortOrderSet(lpsSortOrderSet));
	else
		memset(lpSortCriteria, 0, CbNewSSortOrderSet(0));

	*lppSortCriteria = lpSortCriteria.release();
	return hr;
}

HRESULT ECMAPITable::Abort()
{
	scoped_rlock biglock(m_hLock);
	FlushDeferred();
	/*
	 * Fixme: sent this call to the server, and breaks the search!
	 * OLK 2007 request.
	 */
	return S_OK;
}

HRESULT ECMAPITable::ExpandRow(ULONG cbInstanceKey, LPBYTE pbInstanceKey, ULONG ulRowCount, ULONG ulFlags, LPSRowSet * lppRows, ULONG *lpulMoreRows)
{
	scoped_rlock lock(m_hLock);
	HRESULT hr = FlushDeferred();
	if(hr != hrSuccess)
		return hr;
	return lpTableOps->HrExpandRow(cbInstanceKey, pbInstanceKey,
	       ulRowCount, ulFlags, lppRows, lpulMoreRows);
}

HRESULT ECMAPITable::CollapseRow(ULONG cbInstanceKey, LPBYTE pbInstanceKey, ULONG ulFlags, ULONG *lpulRowCount)
{
	scoped_rlock lock(m_hLock);
	HRESULT hr = FlushDeferred();
	if(hr != hrSuccess)
		return hr;
	return lpTableOps->HrCollapseRow(cbInstanceKey, pbInstanceKey, ulFlags,
	       lpulRowCount);
}

// @todo do we need lock here, currently we do. maybe we must return MAPI_E_TIMEOUT
HRESULT ECMAPITable::WaitForCompletion(ULONG ulFlags, ULONG ulTimeout, ULONG *lpulTableStatus)
{
	scoped_rlock lock(m_hLock);

	HRESULT hr = FlushDeferred();
	if(hr != hrSuccess)
		return hr;
	if(lpulTableStatus)
		*lpulTableStatus = S_OK;
	return hrSuccess;
}

HRESULT ECMAPITable::GetCollapseState(ULONG ulFlags, ULONG cbInstanceKey, LPBYTE lpbInstanceKey, ULONG *lpcbCollapseState, LPBYTE *lppbCollapseState)
{
	scoped_rlock lock(m_hLock);
	HRESULT hr = FlushDeferred();
	if(hr != hrSuccess)
		return hr;
	return lpTableOps->HrGetCollapseState(lppbCollapseState,
	       lpcbCollapseState, lpbInstanceKey, cbInstanceKey);
}

HRESULT ECMAPITable::SetCollapseState(ULONG ulFlags, ULONG cbCollapseState, LPBYTE pbCollapseState, BOOKMARK *lpbkLocation)
{
	scoped_rlock lock(m_hLock);

	HRESULT hr = FlushDeferred();
	if(hr != hrSuccess)
		return hr;

	hr = lpTableOps->HrSetCollapseState(pbCollapseState, cbCollapseState, lpbkLocation);
	if(lpbkLocation)
		*lpbkLocation = BOOKMARK_BEGINNING;
	return hr;
}

HRESULT ECMAPITable::HrSetTableOps(WSTableView *lpTableOps, bool fLoad)
{
	HRESULT hr;

	this->lpTableOps = lpTableOps;
	lpTableOps->AddRef();

	// Open the table on the server, ready for reading ..
	if(fLoad) {
		hr = lpTableOps->HrOpenTable();
		if (hr != hrSuccess)
			return hr;
	}

	lpTableOps->SetReloadCallback(Reload, this);
	return hrSuccess;
}

HRESULT ECMAPITable::QueryRows(LONG lRowCount, ULONG ulFlags, LPSRowSet *lppRows)
{
	HRESULT hr = hrSuccess;

	scoped_rlock lock(m_hLock);

	if(IsDeferred()) {
	    m_ulRowCount = lRowCount;
	    m_ulFlags = ulFlags;
	    
	    hr = FlushDeferred(lppRows);
    } else {
        
        // Send the request to the TableOps object, which will send the request to the server.
        hr = this->lpTableOps->HrQueryRows(lRowCount, ulFlags, lppRows);
    }
	return hr;
}

HRESULT ECMAPITable::Reload(void *lpParam)
{
	auto lpThis = static_cast<ECMAPITable *>(lpParam);

	// Locking m_hLock is not allowed here since when we are called, the SOAP transport in lpTableOps  
	// will be locked. Since normally m_hLock is locked before SOAP, locking m_hLock *after* SOAP here  
	// would be a lock-order violation causing deadlocks.  

	scoped_rlock lock(lpThis->m_hMutexConnectionList);

	// The underlying data has been reloaded, therefore we must re-register the advises. This is called
	// after the transport has re-established its state
	for (auto conn_id : lpThis->m_ulConnectionList) {
		HRESULT hr = lpThis->lpNotifyClient->Reregister(conn_id, 4,
			reinterpret_cast<BYTE *>(&lpThis->lpTableOps->ulTableId));
		if(hr != hrSuccess)
			return hr;
	}
	return hrSuccess;
}
