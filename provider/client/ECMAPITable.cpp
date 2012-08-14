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

#include "platform.h"

#include <mapicode.h>
#include <mapidefs.h>
#include <mapitags.h>
#include <mapiguid.h>
#include <mapiutil.h>

#include "Mem.h"
#include "ECMAPITable.h"
#include "edkguid.h"
#include "ECGuid.h"
#include "Util.h"

#include "ECDebug.h"
#include "ECInterfaceDefs.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

ECMAPITable::ECMAPITable(ECNotifyClient *lpNotifyClient, ULONG ulFlags) : ECUnknown("IMAPITable")
{
	TRACE_MAPI(TRACE_ENTRY, "ECMAPITable::ECMAPITable","");

	this->lpNotifyClient = lpNotifyClient;
	
	if(this->lpNotifyClient)
		this->lpNotifyClient->AddRef();

	this->lpsSortOrderSet = NULL;
	this->lpsPropTags = NULL;
	this->ulFlags = ulFlags;
	this->lpTableOps = NULL;
	
	m_lpSetColumns = NULL;
	m_lpRestrict = NULL;
	m_lpSortTable = NULL;
	m_ulRowCount = 0;
	m_ulFlags = 0;
	m_ulDeferredFlags = 0;

	pthread_mutexattr_t mattr;
	pthread_mutexattr_init(&mattr);
	pthread_mutexattr_settype(&mattr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&m_hLock, &mattr);
	pthread_mutex_init(&m_hMutexConnectionList, &mattr); 
}

HRESULT ECMAPITable::FlushDeferred(LPSRowSet *lppRowSet)
{
    HRESULT hr = hrSuccess;
    
	hr = lpTableOps->HrOpenTable();
	if(hr != hrSuccess)
		goto exit;

    // No deferred calls -> nothing to do
    if(!IsDeferred())
        goto exit;
        
    hr = lpTableOps->HrMulti(m_ulDeferredFlags, m_lpSetColumns, m_lpRestrict, m_lpSortTable, m_ulRowCount, m_ulFlags, lppRowSet);

    // Reset deferred items
    if(m_lpSetColumns)
        MAPIFreeBuffer(m_lpSetColumns);
	m_lpSetColumns = NULL;
	if(m_lpRestrict)
	    MAPIFreeBuffer(m_lpRestrict);
	m_lpRestrict = NULL;
	if(m_lpSortTable)
	    MAPIFreeBuffer(m_lpSortTable);
	m_lpSortTable = NULL;
	m_ulRowCount = 0;
	m_ulFlags = 0;
	m_ulDeferredFlags = 0;

exit: 
    return hr;
}

BOOL ECMAPITable::IsDeferred()
{
    if(m_lpSetColumns == NULL && m_lpRestrict == NULL && m_lpSortTable == NULL && m_ulRowCount == 0 && m_ulFlags == 0 && m_ulDeferredFlags == 0)
        return false;
    return true;
}

ECMAPITable::~ECMAPITable()
{
	TRACE_MAPI(TRACE_ENTRY, "ECMAPITable::~ECMAPITable","");
	std::set<ULONG>::iterator iterMapInt;
	std::set<ULONG>::iterator iterMapIntDel;

	// Remove all advises	
	iterMapInt = m_ulConnectionList.begin();
	while( iterMapInt != m_ulConnectionList.end() )
	{
		iterMapIntDel = iterMapInt;
		iterMapInt++;
		Unadvise(*iterMapIntDel);
	}

	if(lpsPropTags)
		delete [] this->lpsPropTags;

	if (m_lpRestrict)
		MAPIFreeBuffer(m_lpRestrict);

	if (m_lpSetColumns)
		MAPIFreeBuffer(m_lpSetColumns);

	if (m_lpSortTable)
		MAPIFreeBuffer(m_lpSortTable);

	if(lpNotifyClient)
		lpNotifyClient->Release();

	if(lpTableOps)
		lpTableOps->Release();	// closes the table on the server too

	if(lpsSortOrderSet)
		delete [] lpsSortOrderSet;

	pthread_mutex_destroy(&m_hMutexConnectionList);
	pthread_mutex_destroy(&m_hLock);
}

HRESULT ECMAPITable::Create(ECNotifyClient *lpNotifyClient, ULONG ulFlags, ECMAPITable **lppECMAPITable)
{
	HRESULT hr = hrSuccess;
	ECMAPITable *lpMAPITable = NULL;
	
	lpMAPITable = new ECMAPITable(lpNotifyClient, ulFlags);

	hr = lpMAPITable->QueryInterface(IID_ECMAPITable, (void **)lppECMAPITable);

	return hr;
}

HRESULT ECMAPITable::QueryInterface(REFIID refiid, void **lppInterface)
{
	REGISTER_INTERFACE(IID_ECMAPITable, this);
	REGISTER_INTERFACE(IID_ECUnknown, this);

	REGISTER_INTERFACE(IID_IMAPITable, &this->m_xMAPITable);
	REGISTER_INTERFACE(IID_IUnknown, &this->m_xMAPITable);
	
	REGISTER_INTERFACE(IID_ISelectUnicode, &this->m_xUnknown);

	return MAPI_E_INTERFACE_NOT_SUPPORTED;
}

HRESULT ECMAPITable::GetLastError(HRESULT hResult, ULONG ulFlags, LPMAPIERROR *lppMAPIError)
{
	HRESULT hr = hrSuccess;
	
	pthread_mutex_lock(&m_hLock);

	hr = MAPI_E_NO_SUPPORT;

	pthread_mutex_unlock(&m_hLock);

	return hr;
}

HRESULT ECMAPITable::Advise(ULONG ulEventMask, LPMAPIADVISESINK lpAdviseSink, ULONG * lpulConnection)
{
	HRESULT hr = hrSuccess;
	
	pthread_mutex_lock(&m_hLock);

	hr = FlushDeferred();
	if(hr != hrSuccess)
	    goto exit;

	if(lpNotifyClient == NULL) {
		hr = MAPI_E_NO_SUPPORT;
		goto exit;
	}

	if (lpulConnection == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	// FIXME: if a reconnection happens in another thread during the following call, the ulTableId sent here will be incorrect. The reconnection
	// code will not yet know about this connection since we don't insert it until later, so you may end up getting an Advise() on completely the wrong
	// table. 

	hr = lpNotifyClient->Advise(4, (BYTE *)&lpTableOps->ulTableId, ulEventMask, lpAdviseSink, lpulConnection);
	if(hr != hrSuccess)
		goto exit;

	// We lock the connection list seperately
	pthread_mutex_lock(&m_hMutexConnectionList);
	m_ulConnectionList.insert(*lpulConnection);
	pthread_mutex_unlock(&m_hMutexConnectionList); 

exit:
	pthread_mutex_unlock(&m_hLock);

	return hr;
}

HRESULT ECMAPITable::Unadvise(ULONG ulConnection)
{
	HRESULT hr = hrSuccess;
	std::set<ULONG>::iterator iterMapInt;

	pthread_mutex_lock(&m_hLock);

	hr = FlushDeferred();
	if(hr != hrSuccess)
	    goto exit;

	if(lpNotifyClient == NULL) {
		hr = MAPI_E_NO_SUPPORT;
		goto exit;
	}

	pthread_mutex_lock(&m_hMutexConnectionList); 
	m_ulConnectionList.erase(ulConnection);
	pthread_mutex_unlock(&m_hMutexConnectionList); 

	lpNotifyClient->Unadvise(ulConnection);

exit:
	pthread_mutex_unlock(&m_hLock);

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


HRESULT ECMAPITable::SetColumns(LPSPropTagArray lpPropTagArray, ULONG ulFlags)
{
	HRESULT hr = hrSuccess;

	if(lpPropTagArray == NULL || lpPropTagArray->cValues == 0)
		return MAPI_E_INVALID_PARAMETER;

	pthread_mutex_lock(&m_hLock);

	if(lpsPropTags)
		delete [] this->lpsPropTags;

	lpsPropTags = (LPSPropTagArray) new BYTE[CbNewSPropTagArray(lpPropTagArray->cValues)];

	lpsPropTags->cValues = lpPropTagArray->cValues;
	memcpy(&lpsPropTags->aulPropTag, &lpPropTagArray->aulPropTag, lpPropTagArray->cValues * sizeof(ULONG));

    if(m_lpSetColumns)
        MAPIFreeBuffer(m_lpSetColumns);
	m_lpSetColumns = NULL;

    hr = MAPIAllocateBuffer(CbNewSPropTagArray(lpPropTagArray->cValues), (void **)&m_lpSetColumns);
    if(hr != hrSuccess)
        goto exit;
        
    m_lpSetColumns->cValues = lpPropTagArray->cValues;
    memcpy(&m_lpSetColumns->aulPropTag, &lpPropTagArray->aulPropTag, lpPropTagArray->cValues * sizeof(ULONG));

    if(!(ulFlags & TBL_BATCH)) {
        hr = FlushDeferred();
	    if(hr != hrSuccess)
	        goto exit;
    }
    
exit:
	pthread_mutex_unlock(&m_hLock);

	return hr;
}

HRESULT ECMAPITable::QueryColumns(ULONG ulFlags, LPSPropTagArray *lppPropTagArray)
{
	HRESULT hr = hrSuccess;

	pthread_mutex_lock(&m_hLock);

	hr = FlushDeferred();
	if(hr != hrSuccess)
	    goto exit;

	// FIXME if the client has done SetColumns, we can handle this
	// call locally instead of querying the server (unless TBL_ALL_COLUMNS has been
	// specified)

	hr = this->lpTableOps->HrQueryColumns(ulFlags, lppPropTagArray);
exit:
	pthread_mutex_unlock(&m_hLock);

	return hr;
}

HRESULT ECMAPITable::GetRowCount(ULONG ulFlags, ULONG *lpulCount)
{
	HRESULT hr = hrSuccess;
	ULONG ulRow = 0; // discarded

	pthread_mutex_lock(&m_hLock);

	hr = FlushDeferred();
	if(hr != hrSuccess)
	    goto exit;

	hr = this->lpTableOps->HrGetRowCount(lpulCount, &ulRow);

exit:
	pthread_mutex_unlock(&m_hLock);

	return hr;
}


HRESULT ECMAPITable::SeekRow(BOOKMARK bkOrigin, LONG lRowCount, LONG *lplRowsSought)
{
	HRESULT hr = hrSuccess;

	pthread_mutex_lock(&m_hLock);

    hr = FlushDeferred();
    if(hr != hrSuccess)
        goto exit;

    hr = this->lpTableOps->HrSeekRow(bkOrigin, lRowCount, lplRowsSought);

exit:
	pthread_mutex_unlock(&m_hLock);

	return hr;
}


HRESULT ECMAPITable::SeekRowApprox(ULONG ulNumerator, ULONG ulDenominator)
{
	HRESULT hr = hrSuccess;
	ULONG ulRows = 0;
	ULONG ulCurrent = 0;

	pthread_mutex_lock(&m_hLock);

	hr = FlushDeferred();
	if(hr != hrSuccess)
	    goto exit;

	hr = lpTableOps->HrGetRowCount(&ulRows, &ulCurrent);

	if(hr != hrSuccess)
		goto exit;

	hr = SeekRow(BOOKMARK_BEGINNING, (ULONG)((double)ulRows * ((double)ulNumerator / ulDenominator)),NULL);
		
exit:
	pthread_mutex_unlock(&m_hLock);

	return hr;
}


HRESULT ECMAPITable::QueryPosition(ULONG *lpulRow, ULONG *lpulNumerator, ULONG *lpulDenominator)
{
	HRESULT hr = hrSuccess;
	ULONG ulRows = 0;
	ULONG ulCurrentRow = 0;

	pthread_mutex_lock(&m_hLock);

	hr = FlushDeferred();
	if(hr != hrSuccess)
	    goto exit;

	hr = lpTableOps->HrGetRowCount(&ulRows, &ulCurrentRow);

	if(hr != hrSuccess)
		goto exit;

	*lpulRow = ulCurrentRow;
	*lpulNumerator = ulCurrentRow;
	*lpulDenominator = (ulRows == 0)?1:ulRows;

exit:
	pthread_mutex_unlock(&m_hLock);

	return hr;
}


HRESULT ECMAPITable::FindRow(LPSRestriction lpRestriction, BOOKMARK bkOrigin, ULONG ulFlags)
{
	HRESULT hr = hrSuccess;

	pthread_mutex_lock(&m_hLock);

	if (!lpRestriction) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	hr = FlushDeferred();
	if(hr != hrSuccess)
	    goto exit;

	hr = this->lpTableOps->HrFindRow(lpRestriction, bkOrigin, ulFlags);
exit:
	pthread_mutex_unlock(&m_hLock);

	return hr;
}


HRESULT ECMAPITable::Restrict(LPSRestriction lpRestriction, ULONG ulFlags)
{
	HRESULT hr = hrSuccess;

	pthread_mutex_lock(&m_hLock);

    if(m_lpRestrict)
        MAPIFreeBuffer(m_lpRestrict);
    if(lpRestriction) {
        MAPIAllocateBuffer(sizeof(SRestriction), (void **)&m_lpRestrict);
        
        hr = Util::HrCopySRestriction(m_lpRestrict, lpRestriction, m_lpRestrict);

		m_ulDeferredFlags &= ~TABLE_MULTI_CLEAR_RESTRICTION;
    } else {
		// setting the restriction to NULL is not the same as not setting the restriction at all
		m_ulDeferredFlags |= TABLE_MULTI_CLEAR_RESTRICTION;
        m_lpRestrict = NULL;
    }

    if(!(ulFlags & TBL_BATCH)) {
	    hr = FlushDeferred();
	    if(hr != hrSuccess)
	        goto exit;
    }

exit:
	pthread_mutex_unlock(&m_hLock);

	return hr;
}


HRESULT ECMAPITable::CreateBookmark(BOOKMARK* lpbkPosition)
{
	HRESULT hr = hrSuccess;

	pthread_mutex_lock(&m_hLock);

	hr = FlushDeferred();
	if(hr != hrSuccess)
	    goto exit;

	hr = this->lpTableOps->CreateBookmark(lpbkPosition);
exit:
	pthread_mutex_unlock(&m_hLock);

	return hr;
}


HRESULT ECMAPITable::FreeBookmark(BOOKMARK bkPosition)
{
	HRESULT hr = hrSuccess;
	
	pthread_mutex_lock(&m_hLock);

	hr = FlushDeferred();
	if(hr != hrSuccess)
	    goto exit;

	hr = this->lpTableOps->FreeBookmark(bkPosition);
exit:
	pthread_mutex_unlock(&m_hLock);

	return hr;
}


HRESULT ECMAPITable::SortTable(LPSSortOrderSet lpSortCriteria, ULONG ulFlags)
{
	HRESULT hr = hrSuccess;

	pthread_mutex_lock(&m_hLock);

	if (!lpSortCriteria) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	if(lpsSortOrderSet)
		delete [] lpsSortOrderSet;

	lpsSortOrderSet = (LPSSortOrderSet) new BYTE[CbSSortOrderSet(lpSortCriteria)];

	memcpy(lpsSortOrderSet, lpSortCriteria, CbSSortOrderSet(lpSortCriteria));

    if(m_lpSortTable)
        MAPIFreeBuffer(m_lpSortTable);
    MAPIAllocateBuffer(CbSSortOrderSet(lpSortCriteria), (void **) &m_lpSortTable);
    memcpy(m_lpSortTable, lpSortCriteria, CbSSortOrderSet(lpSortCriteria));

    if(!(ulFlags & TBL_BATCH)) {
	    hr = FlushDeferred();
	    if(hr != hrSuccess)
	        goto exit;
    }

exit:
	pthread_mutex_unlock(&m_hLock);

	return hr;
}


HRESULT ECMAPITable::QuerySortOrder(LPSSortOrderSet *lppSortCriteria)
{
	HRESULT hr = hrSuccess;
	LPSSortOrderSet lpSortCriteria = NULL;

	pthread_mutex_lock(&m_hLock);

	hr = FlushDeferred();
	if(hr != hrSuccess)
	    goto exit;

	if(lpsSortOrderSet)
		hr = ECAllocateBuffer(CbSSortOrderSet(lpsSortOrderSet), (void **) &lpSortCriteria);
	else
		hr = ECAllocateBuffer(CbNewSSortOrderSet(0), (void **) &lpSortCriteria);

	if(hr != hrSuccess)
		goto exit;

	if(lpsSortOrderSet)
		memcpy(lpSortCriteria, lpsSortOrderSet, CbSSortOrderSet(lpsSortOrderSet));
	else
		memset(lpSortCriteria, 0, CbNewSSortOrderSet(0));

	*lppSortCriteria = lpSortCriteria;
exit:
	pthread_mutex_unlock(&m_hLock);

	return hr;
}
 

HRESULT ECMAPITable::Abort()
{
	HRESULT hr = hrSuccess;

	pthread_mutex_lock(&m_hLock);

	hr = FlushDeferred();
	if(hr != hrSuccess)
	    goto exit;
exit:
	hr = S_OK; // Fixme: sent this call to the server, and breaks the search! // OLK 2007 request

	pthread_mutex_unlock(&m_hLock);

	return hr;
}


HRESULT ECMAPITable::ExpandRow(ULONG cbInstanceKey, LPBYTE pbInstanceKey, ULONG ulRowCount, ULONG ulFlags, LPSRowSet * lppRows, ULONG *lpulMoreRows)
{
	HRESULT hr = hrSuccess;

	pthread_mutex_lock(&m_hLock);

	hr = FlushDeferred();
	if(hr != hrSuccess)
	    goto exit;

	hr = lpTableOps->HrExpandRow(cbInstanceKey, pbInstanceKey, ulRowCount, ulFlags, lppRows, lpulMoreRows);
exit:
	pthread_mutex_unlock(&m_hLock);
	return hr;
}


HRESULT ECMAPITable::CollapseRow(ULONG cbInstanceKey, LPBYTE pbInstanceKey, ULONG ulFlags, ULONG *lpulRowCount)
{
	HRESULT hr = hrSuccess;

	pthread_mutex_lock(&m_hLock);

	hr = FlushDeferred();
	if(hr != hrSuccess)
	    goto exit;

	hr = lpTableOps->HrCollapseRow(cbInstanceKey, pbInstanceKey, ulFlags, lpulRowCount);
exit:
	pthread_mutex_unlock(&m_hLock);

	return hr;
}

// @todo do we need lock here, currently we do. maybe we must return MAPI_E_TIMEOUT
HRESULT ECMAPITable::WaitForCompletion(ULONG ulFlags, ULONG ulTimeout, ULONG *lpulTableStatus)
{
	HRESULT hr = hrSuccess;

	pthread_mutex_lock(&m_hLock);

	hr = FlushDeferred();
	if(hr != hrSuccess)
	    goto exit;

	if(lpulTableStatus)
		*lpulTableStatus = S_OK;
exit:
	pthread_mutex_unlock(&m_hLock);
	return hr;
}


HRESULT ECMAPITable::GetCollapseState(ULONG ulFlags, ULONG cbInstanceKey, LPBYTE lpbInstanceKey, ULONG *lpcbCollapseState, LPBYTE *lppbCollapseState)
{
	HRESULT hr = hrSuccess;

	pthread_mutex_lock(&m_hLock);

	hr = FlushDeferred();
	if(hr != hrSuccess)
	    goto exit;

	hr = lpTableOps->HrGetCollapseState(lppbCollapseState,lpcbCollapseState, lpbInstanceKey, cbInstanceKey);
exit:
	pthread_mutex_unlock(&m_hLock);

	return hr;
}


HRESULT ECMAPITable::SetCollapseState(ULONG ulFlags, ULONG cbCollapseState, LPBYTE pbCollapseState, BOOKMARK *lpbkLocation)
{
	HRESULT hr = hrSuccess;

	pthread_mutex_lock(&m_hLock);

	hr = FlushDeferred();
	if(hr != hrSuccess)
	    goto exit;

	hr = lpTableOps->HrSetCollapseState(pbCollapseState, cbCollapseState, lpbkLocation);

	if(lpbkLocation)
		*lpbkLocation = BOOKMARK_BEGINNING;
exit:
	pthread_mutex_unlock(&m_hLock);
	return hr;
}

HRESULT ECMAPITable::HrSetTableOps(WSTableView *lpTableOps, bool fLoad)
{
	HRESULT hr = hrSuccess;
	SSortOrderSet sSort;

	sSort.cCategories = 0;
	sSort.cExpanded = 0;
	sSort.cSorts = 1;
	sSort.aSort[0].ulOrder = TABLE_SORT_ASCEND;
	sSort.aSort[0].ulPropTag = PR_DISPLAY_NAME;

	this->lpTableOps = lpTableOps;
	lpTableOps->AddRef();

	// Open the table on the server, ready for reading ..
	if(fLoad) {
    	hr = lpTableOps->HrOpenTable();

    	if(hr != hrSuccess)
		    goto exit;
    }

	lpTableOps->SetReloadCallback(Reload, this);

exit:
	return hr;
}

HRESULT ECMAPITable::QueryRows(LONG lRowCount, ULONG ulFlags, LPSRowSet *lppRows)
{
	HRESULT hr = hrSuccess;

	pthread_mutex_lock(&m_hLock);

	if(IsDeferred()) {
	    m_ulRowCount = lRowCount;
	    m_ulFlags = ulFlags;
	    
	    hr = FlushDeferred(lppRows);
    } else {
        
        // Send the request to the TableOps object, which will send the request to the server.
        hr = this->lpTableOps->HrQueryRows(lRowCount, ulFlags, lppRows);
    }
    
	if(hr != hrSuccess)
		goto exit;

exit:
	pthread_mutex_unlock(&m_hLock);

	return hr;
}

HRESULT ECMAPITable::Reload(void *lpParam)
{
	HRESULT hr = hrSuccess;
	ECMAPITable *lpThis = (ECMAPITable *)lpParam;
	std::set<ULONG>::iterator iter;

	// Locking m_hLock is not allowed here since when we are called, the SOAP transport in lpTableOps  
	// will be locked. Since normally m_hLock is locked before SOAP, locking m_hLock *after* SOAP here  
	// would be a lock-order violation causing deadlocks.  

	pthread_mutex_lock(&lpThis->m_hMutexConnectionList); 

	// The underlying data has been reloaded, therefore we must re-register the advises. This is called
	// after the transport has re-established its state
	for(iter = lpThis->m_ulConnectionList.begin(); iter != lpThis->m_ulConnectionList.end(); iter++) {
		hr = lpThis->lpNotifyClient->Reregister(*iter, 4, (BYTE *)&lpThis->lpTableOps->ulTableId);
		if(hr != hrSuccess)
			goto exit;
	}

exit:
	pthread_mutex_unlock(&lpThis->m_hMutexConnectionList); 

	return hr;
}

ULONG ECMAPITable::xMAPITable::AddRef()
{
	METHOD_PROLOGUE_(ECMAPITable, MAPITable);
	TRACE_MAPI(TRACE_ENTRY, "IMAPITable::AddRef", "table=%d", pThis->lpTableOps->ulTableId);
	return pThis->AddRef();
}

ULONG ECMAPITable::xMAPITable::Release()
{
	METHOD_PROLOGUE_(ECMAPITable, MAPITable);
	TRACE_MAPI(TRACE_ENTRY, "IMAPITable::Release", "table=%d", pThis->lpTableOps->ulTableId);	
	return pThis->Release();
}

DEF_HRMETHOD_EX(TRACE_MAPI, ECMAPITable, MAPITable, "table=%d", pThis->lpTableOps->ulTableId, QueryInterface, (REFIID, refiid), (void **, lppInterface))
DEF_HRMETHOD_EX(TRACE_MAPI, ECMAPITable, MAPITable, "table=%d", pThis->lpTableOps->ulTableId, GetLastError, (HRESULT, hResult), (ULONG, ulFlags), (LPMAPIERROR *, lppMAPIError))
DEF_HRMETHOD_EX(TRACE_MAPI, ECMAPITable, MAPITable, "table=%d", pThis->lpTableOps->ulTableId, Advise, (ULONG, ulEventMask), (LPMAPIADVISESINK, lpAdviseSink), (ULONG *, lpulConnection))
DEF_HRMETHOD_EX(TRACE_MAPI, ECMAPITable, MAPITable, "table=%d", pThis->lpTableOps->ulTableId, Unadvise, (ULONG, ulConnection))
DEF_HRMETHOD_EX(TRACE_MAPI, ECMAPITable, MAPITable, "table=%d", pThis->lpTableOps->ulTableId, GetStatus, (ULONG *, lpulTableStatus), (ULONG *, lpulTableType))
DEF_HRMETHOD_EX(TRACE_MAPI, ECMAPITable, MAPITable, "table=%d", pThis->lpTableOps->ulTableId, SetColumns, (LPSPropTagArray, lpPropTagArray), (ULONG, ulFlags))
DEF_HRMETHOD_EX(TRACE_MAPI, ECMAPITable, MAPITable, "table=%d", pThis->lpTableOps->ulTableId, QueryColumns, (ULONG, ulFlags), (LPSPropTagArray *, lppPropTagArray))
DEF_HRMETHOD_EX(TRACE_MAPI, ECMAPITable, MAPITable, "table=%d", pThis->lpTableOps->ulTableId, GetRowCount, (ULONG, ulFlags), (ULONG *, lpulCount))
DEF_HRMETHOD_EX(TRACE_MAPI, ECMAPITable, MAPITable, "table=%d", pThis->lpTableOps->ulTableId, SeekRow, (BOOKMARK, bkOrigin), (LONG, lRowCount), (LONG *, lplRowsSought))
DEF_HRMETHOD_EX(TRACE_MAPI, ECMAPITable, MAPITable, "table=%d", pThis->lpTableOps->ulTableId, SeekRowApprox, (ULONG, ulNumerator), (ULONG, ulDenominator))
DEF_HRMETHOD_EX(TRACE_MAPI, ECMAPITable, MAPITable, "table=%d", pThis->lpTableOps->ulTableId, QueryPosition, (ULONG *, lpulRow), (ULONG *, lpulNumerator), (ULONG *, lpulDenominator))
DEF_HRMETHOD_EX(TRACE_MAPI, ECMAPITable, MAPITable, "table=%d", pThis->lpTableOps->ulTableId, FindRow, (LPSRestriction, lpRestriction), (BOOKMARK, bkOrigin), (ULONG, ulFlags))
DEF_HRMETHOD_EX(TRACE_MAPI, ECMAPITable, MAPITable, "table=%d", pThis->lpTableOps->ulTableId, Restrict, (LPSRestriction, lpRestriction), (ULONG, ulFlags))
DEF_HRMETHOD_EX(TRACE_MAPI, ECMAPITable, MAPITable, "table=%d", pThis->lpTableOps->ulTableId, CreateBookmark, (BOOKMARK *, lpbkPosition))
DEF_HRMETHOD_EX(TRACE_MAPI, ECMAPITable, MAPITable, "table=%d", pThis->lpTableOps->ulTableId, FreeBookmark, (BOOKMARK, bkPosition))
DEF_HRMETHOD_EX(TRACE_MAPI, ECMAPITable, MAPITable, "table=%d", pThis->lpTableOps->ulTableId, SortTable, (LPSSortOrderSet, lpSortCriteria), (ULONG, ulFlags))
DEF_HRMETHOD_EX(TRACE_MAPI, ECMAPITable, MAPITable, "table=%d", pThis->lpTableOps->ulTableId, QuerySortOrder, (LPSSortOrderSet *, lppSortCriteria))
DEF_HRMETHOD_EX(TRACE_MAPI, ECMAPITable, MAPITable, "table=%d", pThis->lpTableOps->ulTableId, QueryRows, (LONG, lRowCount), (ULONG, ulFlags), (LPSRowSet *, lppRows))
DEF_HRMETHOD_EX(TRACE_MAPI, ECMAPITable, MAPITable, "table=%d", pThis->lpTableOps->ulTableId, Abort, (void))
DEF_HRMETHOD_EX(TRACE_MAPI, ECMAPITable, MAPITable, "table=%d", pThis->lpTableOps->ulTableId, ExpandRow, (ULONG, cbInstanceKey, LPBYTE, pbInstanceKey), (ULONG, ulRowCount), (ULONG, ulFlags), (LPSRowSet *, lppRows), (ULONG *, lpulMoreRows))
DEF_HRMETHOD_EX(TRACE_MAPI, ECMAPITable, MAPITable, "table=%d", pThis->lpTableOps->ulTableId, CollapseRow, (ULONG, cbInstanceKey, LPBYTE, pbInstanceKey), (ULONG, ulFlags), (ULONG *, lpulRowCount))
DEF_HRMETHOD_EX(TRACE_MAPI, ECMAPITable, MAPITable, "table=%d", pThis->lpTableOps->ulTableId, WaitForCompletion, (ULONG, ulFlags), (ULONG, ulTimeout), (ULONG *, lpulTableStatus))
DEF_HRMETHOD_EX(TRACE_MAPI, ECMAPITable, MAPITable, "table=%d", pThis->lpTableOps->ulTableId, GetCollapseState, (ULONG, ulFlags), (ULONG, cbInstanceKey, LPBYTE, lpbInstanceKey), (ULONG *, lpcbCollapseState, LPBYTE *, lppbCollapseState))
DEF_HRMETHOD_EX(TRACE_MAPI, ECMAPITable, MAPITable, "table=%d", pThis->lpTableOps->ulTableId, SetCollapseState, (ULONG, ulFlags), (ULONG, cbCollapseState, LPBYTE, pbCollapseState), (BOOKMARK *, lpbkLocation))
