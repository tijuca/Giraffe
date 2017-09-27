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
	CopyMAPIEntryIdToSOAPEntryId(cbEntryId, lpEntryId, &m_sEntryId);

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

HRESULT WSABPropStorage::HrReadProps(LPSPropTagArray *lppPropTags,ULONG *cValues, LPSPropValue *ppValues)
{
	HRESULT			hr = hrSuccess;
	ECRESULT		er = hrSuccess;
	convert_context	converter;

	struct readPropsResponse sResponse;

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

	// Convert the property tags to a MAPI proptagarray
	hr = ECAllocateBuffer(CbNewSPropTagArray(sResponse.aPropTag.__size), (void **)lppPropTags);

	if(hr != hrSuccess)
		goto exit;

	(*lppPropTags)->cValues = sResponse.aPropTag.__size;
	for (gsoap_size_t i = 0; i < sResponse.aPropTag.__size; ++i)
		(*lppPropTags)->aulPropTag[i] = sResponse.aPropTag.__ptr[i];

	// Convert the property values to a MAPI propvalarray
	*cValues = sResponse.aPropVal.__size;

	if(sResponse.aPropTag.__size == 0) {
		*ppValues = NULL;
	} else {
		hr = ECAllocateBuffer(sizeof(SPropValue) * sResponse.aPropVal.__size, (void **)ppValues);

		if(hr != hrSuccess)
			goto exit;
	}

	for (gsoap_size_t i = 0; i < sResponse.aPropVal.__size; ++i) {
		hr = CopySOAPPropValToMAPIPropVal(&(*ppValues)[i],&sResponse.aPropVal.__ptr[i], *ppValues, &converter);

		if(hr != hrSuccess)
			goto exit;
	}

exit:
	UnLockSoap();

	if(hr != hrSuccess) {
		if(*lppPropTags)
			ECFreeBuffer(*lppPropTags);

		if(*ppValues)
			ECFreeBuffer(*ppValues);
	}

	return hr;
}

HRESULT WSABPropStorage::HrLoadProp(ULONG ulObjId, ULONG ulPropTag, LPSPropValue *lppsPropValue)
{
	ECRESULT		er = erSuccess;
	HRESULT			hr = hrSuccess;
	LPSPropValue	lpsPropValDst = NULL;

	struct loadPropResponse	sResponse;

	LockSoap();

	START_SOAP_CALL
	{
        if(SOAP_OK != lpCmd->ns__loadABProp(ecSessionId, m_sEntryId, ulPropTag, &sResponse))
            er = KCERR_NETWORK_ERROR;
        else
            er = sResponse.er;
    }
    END_SOAP_CALL

	hr = ECAllocateBuffer(sizeof(SPropValue), (void **)&lpsPropValDst);

	if(hr != hrSuccess)
		goto exit;

	if(sResponse.lpPropVal == NULL) {
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}

	hr = CopySOAPPropValToMAPIPropVal(lpsPropValDst, sResponse.lpPropVal, lpsPropValDst);

	*lppsPropValue = lpsPropValDst;

exit:
	UnLockSoap();

	return hr;
}

HRESULT WSABPropStorage::HrWriteProps(ULONG cValues, LPSPropValue pValues, ULONG ulFlags)
{
	HRESULT			hr = hrSuccess;
	ECRESULT		er = erSuccess;
	unsigned int	i = 0;
	unsigned int	j = 0;
	convert_context	converter;

	struct propValArray sPropVals;

	sPropVals.__ptr = s_alloc<propVal>(nullptr, cValues);
	for (i = 0; i < cValues; ++i) {
		hr = CopyMAPIPropValToSOAPPropVal(&sPropVals.__ptr[j], &pValues[i], &converter);
		if(hr == hrSuccess)
			++j;
	}

	hr = hrSuccess;

	sPropVals.__size = j;

	LockSoap();
	
	START_SOAP_CALL
	{
    	if(SOAP_OK != lpCmd->ns__writeABProps(ecSessionId, m_sEntryId, &sPropVals, &er))
    		er = KCERR_NETWORK_ERROR;
    }
    END_SOAP_CALL

exit:
	UnLockSoap();

	if(sPropVals.__ptr)
		FreePropValArray(&sPropVals);

	return hr;
}

HRESULT WSABPropStorage::HrDeleteProps(const SPropTagArray *lpsPropTagArray)
{
	ECRESULT	er = erSuccess;
	HRESULT		hr = hrSuccess;

	struct propTagArray sPropTags;

	sPropTags.__size = lpsPropTagArray->cValues;
	sPropTags.__ptr = (unsigned int *)lpsPropTagArray->aulPropTag;

	LockSoap();
	
	START_SOAP_CALL
	{
    	if(SOAP_OK != lpCmd->ns__deleteABProps(ecSessionId, m_sEntryId, &sPropTags, &er))
	    	er = KCERR_NETWORK_ERROR;
    }
    END_SOAP_CALL

exit:
	UnLockSoap();

	return hr;
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
	LPSPropValue lpProp = NULL;
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
	
	hr = ECAllocateBuffer(sizeof(SPropValue) * sResponse.aPropVal.__size, (void **)&lpProp);
	if (hr != hrSuccess)
		goto exit;

	for (gsoap_size_t i = 0; i < sResponse.aPropTag.__size; ++i)
		mo->lstAvailable.push_back(sResponse.aPropTag.__ptr[i]);

	for (gsoap_size_t i = 0; i < sResponse.aPropVal.__size; ++i) {
		hr = CopySOAPPropValToMAPIPropVal(lpProp, &sResponse.aPropVal.__ptr[i], lpProp, &converter);
		if (hr != hrSuccess)
			goto exit;
		mo->lstProperties.push_back(lpProp);
	}

	*lppsMapiObject = mo;

exit:
	UnLockSoap();

	if (hr != hrSuccess && mo)
		FreeMapiObject(mo);

	if (lpProp)
		ECFreeBuffer(lpProp);

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
