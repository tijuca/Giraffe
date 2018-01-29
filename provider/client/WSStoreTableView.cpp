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
 */
#include <new>
#include <kopano/platform.h>
#include "ECMsgStore.h"
#include "WSStoreTableView.h"
#include "Mem.h"
#include <kopano/ECGuid.h>
#include "SOAPUtils.h"
#include "WSUtil.h"

WSStoreTableView::WSStoreTableView(ULONG ulType, ULONG ulFlags, KCmd *lpCmd,
    std::recursive_mutex &lpDataLock, ECSESSIONID ecSessionId, ULONG cbEntryId,
    LPENTRYID lpEntryId, ECMsgStore *lpMsgStore, WSTransport *lpTransport) :
	WSTableView(ulType, ulFlags, lpCmd, lpDataLock, ecSessionId, cbEntryId,
	    lpEntryId, lpTransport, "WSStoreTableView")
{

	// OK, this is ugly, but the static row-wrapper routines need this object
	// to get the guid and other information that has to be inlined into the table row. Really, 
	// the whole transport layer should have no references to the ECMAPI* classes, because that's
	// upside-down in the layer model, but having the transport layer first deliver the properties,
	// and have a different routine then go through all the properties is more memory intensive AND
	// slower as we have 2 passes to fill the queried rows.

	this->m_lpProvider = (void *)lpMsgStore;
	this->m_ulTableType = TABLETYPE_MS;
}

HRESULT WSStoreTableView::Create(ULONG ulType, ULONG ulFlags, KCmd *lpCmd,
    std::recursive_mutex &lpDataLock, ECSESSIONID ecSessionId, ULONG cbEntryId,
    LPENTRYID lpEntryId, ECMsgStore *lpMsgStore, WSTransport *lpTransport,
    WSTableView **lppTableView)
{
	return alloc_wrap<WSStoreTableView>(ulType, ulFlags, lpCmd, lpDataLock,
	       ecSessionId, cbEntryId, lpEntryId, lpMsgStore, lpTransport)
	       .as(IID_ECTableView, lppTableView);
}

HRESULT WSStoreTableView::QueryInterface(REFIID refiid, void **lppInterface)
{
	REGISTER_INTERFACE3(ECTableView, WSTableView, this);
	return MAPI_E_INTERFACE_NOT_SUPPORTED;
}

// WSTableMultiStore view
WSTableMultiStore::WSTableMultiStore(ULONG ulFlags, KCmd *lpCmd,
    std::recursive_mutex &lpDataLock, ECSESSIONID ecSessionId, ULONG cbEntryId,
    LPENTRYID lpEntryId, ECMsgStore *lpMsgStore, WSTransport *lpTransport) :
	WSStoreTableView(MAPI_MESSAGE, ulFlags, lpCmd, lpDataLock, ecSessionId,
	    cbEntryId, lpEntryId, lpMsgStore, lpTransport)
{
    memset(&m_sEntryList, 0, sizeof(m_sEntryList));

	m_ulTableType = TABLETYPE_MULTISTORE;
	ulTableId = 0;
}

WSTableMultiStore::~WSTableMultiStore()
{
	FreeEntryList(&m_sEntryList, false);
}

HRESULT WSTableMultiStore::Create(ULONG ulFlags, KCmd *lpCmd,
    std::recursive_mutex &lpDataLock, ECSESSIONID ecSessionId, ULONG cbEntryId,
    LPENTRYID lpEntryId, ECMsgStore *lpMsgStore, WSTransport *lpTransport,
    WSTableMultiStore **lppTableMultiStore)
{
	return alloc_wrap<WSTableMultiStore>(ulFlags, lpCmd, lpDataLock,
	       ecSessionId, cbEntryId, lpEntryId, lpMsgStore, lpTransport)
	       .put(lppTableMultiStore);
}

HRESULT WSTableMultiStore::HrOpenTable()
{
	ECRESULT		er = erSuccess;
	HRESULT			hr = hrSuccess;

	struct tableOpenResponse sResponse;

	LockSoap();

	if(this->ulTableId != 0)
	    goto exit;

	//m_sEntryId is the id of a store
	if(SOAP_OK != lpCmd->ns__tableOpen(ecSessionId, m_sEntryId, m_ulTableType, MAPI_MESSAGE, this->ulFlags, &sResponse))
		er = KCERR_NETWORK_ERROR;
	else
		er = sResponse.er;

	hr = kcerr_to_mapierr(er);
	if(hr != hrSuccess)
		goto exit;

	this->ulTableId = sResponse.ulTableId;

	if (SOAP_OK != lpCmd->ns__tableSetMultiStoreEntryIDs(ecSessionId, ulTableId, &m_sEntryList, &er))
		er = KCERR_NETWORK_ERROR;

	hr = kcerr_to_mapierr(er);
	if(hr != hrSuccess)
		goto exit;

exit:
	UnLockSoap();

	return hr;
}

HRESULT WSTableMultiStore::HrSetEntryIDs(LPENTRYLIST lpMsgList)
{
	// Not really a transport function, but this is the best place for it for now

	return CopyMAPIEntryListToSOAPEntryList(lpMsgList, &m_sEntryList);
}

/*
  Miscellaneous tables are not really store tables, but the is the same, so it inherits from the store table
  Supported tables are the stats tables, and userstores table.
*/
WSTableMisc::WSTableMisc(ULONG ulTableType, ULONG ulFlags, KCmd *lpCmd,
    std::recursive_mutex &lpDataLock, ECSESSIONID ecSessionId, ULONG cbEntryId,
    LPENTRYID lpEntryId, ECMsgStore *lpMsgStore, WSTransport *lpTransport) :
	// is MAPI_STATUS even valid here?
	WSStoreTableView(MAPI_STATUS, ulFlags, lpCmd, lpDataLock, ecSessionId,
	    cbEntryId, lpEntryId, lpMsgStore, lpTransport)
{
	m_ulTableType = ulTableType;
	ulTableId = 0;
}

HRESULT WSTableMisc::Create(ULONG ulTableType, ULONG ulFlags, KCmd *lpCmd,
    std::recursive_mutex &lpDataLock, ECSESSIONID ecSessionId, ULONG cbEntryId,
    LPENTRYID lpEntryId, ECMsgStore *lpMsgStore, WSTransport *lpTransport,
    WSTableMisc **lppTableMisc)
{
	return alloc_wrap<WSTableMisc>(ulTableType, ulFlags, lpCmd, lpDataLock,
	       ecSessionId, cbEntryId, lpEntryId, lpMsgStore, lpTransport)
	       .put(lppTableMisc);
}

HRESULT WSTableMisc::HrOpenTable()
{
	ECRESULT er = erSuccess;
	HRESULT hr = hrSuccess;
	struct tableOpenResponse sResponse;

	LockSoap();
	
	if(ulTableId != 0)
	    goto exit;

	// the class is actually only to call this function with the correct ulTableType .... hmm.
	if(SOAP_OK != lpCmd->ns__tableOpen(ecSessionId, m_sEntryId, m_ulTableType, ulType, this->ulFlags, &sResponse))
		er = KCERR_NETWORK_ERROR;
	else
		er = sResponse.er;

	hr = kcerr_to_mapierr(er);
	if(hr != hrSuccess)
		goto exit;

	this->ulTableId = sResponse.ulTableId;

exit:
	UnLockSoap();

	return hr;
}

// WSTableMailBox view
WSTableMailBox::WSTableMailBox(ULONG ulFlags, KCmd *lpCmd,
    std::recursive_mutex &lpDataLock, ECSESSIONID ecSessionId,
    ECMsgStore *lpMsgStore, WSTransport *lpTransport) :
	WSStoreTableView(MAPI_STORE, ulFlags, lpCmd, lpDataLock, ecSessionId,
	    0, NULL, lpMsgStore, lpTransport)
{
	m_ulTableType = TABLETYPE_MAILBOX;
}

HRESULT WSTableMailBox::Create(ULONG ulFlags, KCmd *lpCmd,
    std::recursive_mutex &lpDataLock, ECSESSIONID ecSessionId,
    ECMsgStore *lpMsgStore, WSTransport *lpTransport, WSTableMailBox **lppTable)
{
	return alloc_wrap<WSTableMailBox>(ulFlags, lpCmd, lpDataLock,
	       ecSessionId, lpMsgStore, lpTransport).put(lppTable);
}
