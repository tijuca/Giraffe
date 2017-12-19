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
#include <stdexcept>
#include <kopano/platform.h>
#include "WSABPropStorage.h"
#include "Mem.h"
#include <kopano/ECGuid.h>
#include "SOAPUtils.h"
#include "WSUtil.h"
#include <kopano/charset/convert.h>

#define START_SOAP_CALL retry:
#define END_SOAP_CALL   \
	if (er == KCERR_END_OF_SESSION && this->m_lpTransport->HrReLogon() == hrSuccess) \
		goto retry; \
	hr = kcerr_to_mapierr(er, MAPI_E_NOT_FOUND); \
    if(hr != hrSuccess) \
        goto exit;
                    

/*
 * This is a PropStorage object for use with the WebServices storage platform
 */

WSABPropStorage::WSABPropStorage(ULONG cbEntryId, LPENTRYID lpEntryId,
    KCmd *lpCmd, std::recursive_mutex &data_lock, ECSESSIONID ecSessionId,
    WSTransport *lpTransport) :
	ECUnknown("WSABPropStorage"), lpDataLock(data_lock),
	m_lpTransport(lpTransport)
{
	auto ret = CopyMAPIEntryIdToSOAPEntryId(cbEntryId, lpEntryId, &m_sEntryId);
	if (ret != hrSuccess)
		throw std::runtime_error("CopyMAPIEntryIdToSOAPEntryId");
	this->lpCmd = lpCmd;
	this->ecSessionId = ecSessionId;
    lpTransport->AddSessionReloadCallback(this, Reload, &m_ulSessionReloadCallback);
	    
}

WSABPropStorage::~WSABPropStorage()
{
    m_lpTransport->RemoveSessionReloadCallback(m_ulSessionReloadCallback);
    
	FreeEntryId(&m_sEntryId, false);
}

HRESULT WSABPropStorage::QueryInterface(REFIID refiid, void **lppInterface)
{
	REGISTER_INTERFACE2(WSABPropStorage, this);
	REGISTER_INTERFACE2(IECPropStorage, this);
	return MAPI_E_INTERFACE_NOT_SUPPORTED;
}

HRESULT WSABPropStorage::Create(ULONG cbEntryId, LPENTRYID lpEntryId,
    KCmd *lpCmd, std::recursive_mutex &lpDataLock, ECSESSIONID ecSessionId,
    WSTransport *lpTransport, WSABPropStorage **lppPropStorage)
{
	return alloc_wrap<WSABPropStorage>(cbEntryId, lpEntryId, lpCmd,
	       lpDataLock, ecSessionId, lpTransport).put(lppPropStorage);
}

HRESULT WSABPropStorage::HrLoadProp(ULONG ulObjId, ULONG ulPropTag, LPSPropValue *lppsPropValue)
{
	return MAPI_E_NO_SUPPORT;
}

HRESULT WSABPropStorage::HrSaveObject(ULONG ulFlags, MAPIOBJECT *lpsMapiObject)
{
	return MAPI_E_NO_SUPPORT;
	// TODO: this should be supported eventually
}

HRESULT WSABPropStorage::HrLoadObject(MAPIOBJECT **lppsMapiObject)
{
	HRESULT		hr = hrSuccess;
	ECRESULT	er = hrSuccess;
	MAPIOBJECT  *mo = NULL;
	ecmem_ptr<SPropValue> lpProp;
	struct readPropsResponse sResponse;
	convert_context	converter;

	LockSoap();

	START_SOAP_CALL
	{
    	// Read the properties from the server
    	if(SOAP_OK != lpCmd->ns__readABProps(ecSessionId, m_sEntryId, &sResponse))
    		er = KCERR_NETWORK_ERROR;
    	else
    		er = sResponse.er;
    }
    END_SOAP_CALL
    
	// Convert the property tags to a MAPIOBJECT
	//(type,objectid)
	AllocNewMapiObject(0, 0, 0, &mo);

	/*
	 * This is only done to have a base for AllocateMore, otherwise a local
	 * automatic variable would have sufficed.
	 */
	hr = ECAllocateBuffer(sizeof(SPropValue), &~lpProp);
	if (hr != hrSuccess)
		goto exit;

	for (gsoap_size_t i = 0; i < sResponse.aPropTag.__size; ++i)
		mo->lstAvailable.emplace_back(sResponse.aPropTag.__ptr[i]);

	for (gsoap_size_t i = 0; i < sResponse.aPropVal.__size; ++i) {
		/* can call AllocateMore on lpProp */
		hr = CopySOAPPropValToMAPIPropVal(lpProp, &sResponse.aPropVal.__ptr[i], lpProp, &converter);
		if (hr != hrSuccess)
			goto exit;
		/*
		 * The ECRecipient ctor makes a deep copy of *lpProp, so it is
		 * ok to have *lpProp overwritten on the next iteration.
		 */
		mo->lstProperties.emplace_back(lpProp);
	}

	*lppsMapiObject = mo;

exit:
	UnLockSoap();

	if (hr != hrSuccess && mo)
		FreeMapiObject(mo);
	return hr;
}

IECPropStorage* WSABPropStorage::GetServerStorage()
{
	return this;
}

HRESULT WSABPropStorage::LockSoap()
{
	lpDataLock.lock();
	return erSuccess;
}

HRESULT WSABPropStorage::UnLockSoap()
{
	// Clean up data create with soap_malloc
	if(lpCmd->soap) {
		soap_destroy(lpCmd->soap);
		soap_end(lpCmd->soap);
	}
	lpDataLock.unlock();
	return erSuccess;
}

// Called when the session ID has changed
HRESULT WSABPropStorage::Reload(void *lpParam, ECSESSIONID sessionId) {
	static_cast<WSABPropStorage *>(lpParam)->ecSessionId = sessionId;
	return hrSuccess;
}

WSABTableView::WSABTableView(ULONG ulType, ULONG ulFlags, KCmd *lpCmd,
    std::recursive_mutex &lpDataLock, ECSESSIONID ecSessionId, ULONG cbEntryId,
    LPENTRYID lpEntryId, ECABLogon* lpABLogon, WSTransport *lpTransport) :
	WSTableView(ulType, ulFlags, lpCmd, lpDataLock, ecSessionId, cbEntryId,
	    lpEntryId, lpTransport, "WSABTableView")
{
	m_lpProvider = lpABLogon;
	m_ulTableType = TABLETYPE_AB;
}

HRESULT WSABTableView::Create(ULONG ulType, ULONG ulFlags, KCmd *lpCmd,
    std::recursive_mutex &lpDataLock, ECSESSIONID ecSessionId, ULONG cbEntryId,
    LPENTRYID lpEntryId, ECABLogon* lpABLogon, WSTransport *lpTransport,
    WSTableView **lppTableView)
{
	return alloc_wrap<WSABTableView>(ulType, ulFlags, lpCmd, lpDataLock,
	       ecSessionId, cbEntryId, lpEntryId, lpABLogon, lpTransport)
	       .as(IID_ECTableView, lppTableView);
}

HRESULT WSABTableView::QueryInterface(REFIID refiid, void **lppInterface)
{
	REGISTER_INTERFACE3(ECTableView, WSTableView, this);
	return MAPI_E_INTERFACE_NOT_SUPPORTED;
}
