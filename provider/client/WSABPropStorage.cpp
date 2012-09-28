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

#include "WSABPropStorage.h"

#include "Mem.h"
#include "ECGuid.h"

// Utils
#include "SOAPUtils.h"
#include "WSUtil.h"

#include <charset/convert.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define START_SOAP_CALL retry:
#define END_SOAP_CALL   \
	if(er == ZARAFA_E_END_OF_SESSION) { if(this->m_lpTransport->HrReLogon() == hrSuccess) goto retry; } \
	hr = ZarafaErrorToMAPIError(er, MAPI_E_NOT_FOUND); \
    if(hr != hrSuccess) \
        goto exit;
                    

/*
 * This is a PropStorage object for use with the WebServices storage platform
 */

WSABPropStorage::WSABPropStorage(ULONG cbEntryId, LPENTRYID lpEntryId, ZarafaCmd *lpCmd, pthread_mutex_t *lpDataLock, ECSESSIONID ecSessionId, WSTransport *lpTransport) : ECUnknown("WSABPropStorage")
{
	CopyMAPIEntryIdToSOAPEntryId(cbEntryId, lpEntryId, &m_sEntryId);

	this->lpCmd = lpCmd;
	this->lpDataLock = lpDataLock;
	this->ecSessionId = ecSessionId;
	this->m_lpTransport = lpTransport;
	
    lpTransport->AddSessionReloadCallback(this, Reload, &m_ulSessionReloadCallback);
	    
}

WSABPropStorage::~WSABPropStorage()
{
    m_lpTransport->RemoveSessionReloadCallback(m_ulSessionReloadCallback);
    
	FreeEntryId(&m_sEntryId, false);
}

HRESULT WSABPropStorage::QueryInterface(REFIID refiid, void **lppInterface)
{
	REGISTER_INTERFACE(IID_WSABPropStorage, this);

	REGISTER_INTERFACE(IID_IECPropStorage, &this->m_xECPropStorage);

	return MAPI_E_INTERFACE_NOT_SUPPORTED;
}

HRESULT WSABPropStorage::Create(ULONG cbEntryId, LPENTRYID lpEntryId, ZarafaCmd *lpCmd, pthread_mutex_t *lpDataLock, ECSESSIONID ecSessionId, WSTransport *lpTransport, WSABPropStorage **lppPropStorage)
{
	HRESULT hr = hrSuccess;
	WSABPropStorage *lpStorage = NULL;
	
	lpStorage = new WSABPropStorage(cbEntryId, lpEntryId, lpCmd, lpDataLock, ecSessionId, lpTransport);

	hr = lpStorage->QueryInterface(IID_WSABPropStorage, (void **)lppPropStorage);

	if(hr != hrSuccess)
		delete lpStorage;

	return hr;
}

HRESULT WSABPropStorage::HrReadProps(LPSPropTagArray *lppPropTags,ULONG *cValues, LPSPropValue *ppValues)
{
	HRESULT			hr = hrSuccess;
	ECRESULT		er = hrSuccess;
	int				i;
	convert_context	converter;

	struct readPropsResponse sResponse;

	LockSoap();
	
	START_SOAP_CALL 
	{
        // Read the properties from the server
        if(SOAP_OK != lpCmd->ns__readABProps(ecSessionId, m_sEntryId, &sResponse))
            er = ZARAFA_E_NETWORK_ERROR;
        else
            er = sResponse.er;
    }
    END_SOAP_CALL

	// Convert the property tags to a MAPI proptagarray
	hr = ECAllocateBuffer(CbNewSPropTagArray(sResponse.aPropTag.__size), (void **)lppPropTags);

	if(hr != hrSuccess)
		goto exit;

	(*lppPropTags)->cValues = sResponse.aPropTag.__size;

	for(i=0;i<sResponse.aPropTag.__size;i++) {
		(*lppPropTags)->aulPropTag[i] = sResponse.aPropTag.__ptr[i];
	}

	// Convert the property values to a MAPI propvalarray
	*cValues = sResponse.aPropVal.__size;

	if(sResponse.aPropTag.__size == 0) {
		*ppValues = NULL;
	} else {
		hr = ECAllocateBuffer(sizeof(SPropValue) * sResponse.aPropVal.__size, (void **)ppValues);

		if(hr != hrSuccess)
			goto exit;
	}

	for(i=0;i<sResponse.aPropVal.__size;i++) {
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
            er = ZARAFA_E_NETWORK_ERROR;
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

	sPropVals.__ptr = new propVal[cValues];

	for(i=0;i<cValues;i++) {
		hr = CopyMAPIPropValToSOAPPropVal(&sPropVals.__ptr[j], &pValues[i], &converter);
		if(hr == hrSuccess)
			j++;
	}

	hr = hrSuccess;

	sPropVals.__size = j;

	LockSoap();
	
	START_SOAP_CALL
	{
    	if(SOAP_OK != lpCmd->ns__writeABProps(ecSessionId, m_sEntryId, &sPropVals, &er))
    		er = ZARAFA_E_NETWORK_ERROR;
    }
    END_SOAP_CALL

exit:
	UnLockSoap();

	if(sPropVals.__ptr)
		FreePropValArray(&sPropVals);

	return hr;
}

HRESULT WSABPropStorage::HrDeleteProps(LPSPropTagArray lpsPropTagArray)
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
	    	er = ZARAFA_E_NETWORK_ERROR;
    }
    END_SOAP_CALL

exit:
	UnLockSoap();

	return hr;
}

HRESULT WSABPropStorage::HrSaveObject(ULONG ulFlags, MAPIOBJECT *lpsMapiObject)
{
	HRESULT		hr = MAPI_E_NO_SUPPORT;
	// TODO: this should be supported eventually
	return hr;
}

HRESULT WSABPropStorage::HrLoadObject(MAPIOBJECT **lppsMapiObject)
{
	HRESULT		hr = hrSuccess;
	ECRESULT	er = hrSuccess;
	int			i;
	MAPIOBJECT  *mo = NULL;
	LPSPropValue lpProp = NULL;
	struct readPropsResponse sResponse;
	convert_context	converter;

	LockSoap();

	START_SOAP_CALL
	{
    	// Read the properties from the server
    	if(SOAP_OK != lpCmd->ns__readABProps(ecSessionId, m_sEntryId, &sResponse))
    		er = ZARAFA_E_NETWORK_ERROR;
    	else
    		er = sResponse.er;
    }
    END_SOAP_CALL
    
	// Convert the property tags to a MAPIOBJECT
	//(type,objectid)
	AllocNewMapiObject(0, 0, 0, &mo);
	
	ECAllocateBuffer(sizeof(SPropValue) * sResponse.aPropVal.__size, (void **)&lpProp);

	for (i = 0; i < sResponse.aPropTag.__size; i++)
		mo->lstAvailable->push_back(sResponse.aPropTag.__ptr[i]);

	for (i = 0; i < sResponse.aPropVal.__size; i++) {
		hr = CopySOAPPropValToMAPIPropVal(lpProp, &sResponse.aPropVal.__ptr[i], lpProp, &converter);
		if (hr != hrSuccess)
			goto exit;
		mo->lstProperties->push_back(lpProp);
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
	return &this->m_xECPropStorage;
}

HRESULT WSABPropStorage::LockSoap()
{
	pthread_mutex_lock(lpDataLock);
	return erSuccess;
}

HRESULT WSABPropStorage::UnLockSoap()
{
	// Clean up data create with soap_malloc
	if(lpCmd->soap)
		soap_end(lpCmd->soap);

	pthread_mutex_unlock(lpDataLock);
	return erSuccess;
}

// Called when the session ID has changed
HRESULT WSABPropStorage::Reload(void *lpParam, ECSESSIONID sessionId) {
    WSABPropStorage *lpThis = (WSABPropStorage *)lpParam;
    lpThis->ecSessionId = sessionId;
        
    return hrSuccess;
}
            

////////////////////////////////////////////////
// Interface IECPropStorage
//

ULONG WSABPropStorage::xECPropStorage::AddRef()
{
	METHOD_PROLOGUE_(WSABPropStorage, ECPropStorage);
	return pThis->AddRef();
}

ULONG WSABPropStorage::xECPropStorage::Release()
{
	METHOD_PROLOGUE_(WSABPropStorage, ECPropStorage);
	return pThis->Release();
}

HRESULT WSABPropStorage::xECPropStorage::QueryInterface(REFIID refiid , void** lppInterface)
{
	METHOD_PROLOGUE_(WSABPropStorage, ECPropStorage);
	return pThis->QueryInterface(refiid, lppInterface);
}

HRESULT WSABPropStorage::xECPropStorage::HrReadProps(LPSPropTagArray *lppPropTags,ULONG *cValues, LPSPropValue *lppValues)
{
	METHOD_PROLOGUE_(WSABPropStorage, ECPropStorage);
	return pThis->HrReadProps(lppPropTags,cValues, lppValues);
}
			
HRESULT WSABPropStorage::xECPropStorage::HrLoadProp(ULONG ulObjId, ULONG ulPropTag, LPSPropValue *lppsPropValue)
{
	METHOD_PROLOGUE_(WSABPropStorage, ECPropStorage);
	return pThis->HrLoadProp(ulObjId, ulPropTag, lppsPropValue);
}

HRESULT WSABPropStorage::xECPropStorage::HrWriteProps(ULONG cValues, LPSPropValue lpValues, ULONG ulFlags)
{
	METHOD_PROLOGUE_(WSABPropStorage, ECPropStorage);
	return pThis->HrWriteProps(cValues, lpValues, ulFlags);
}	

HRESULT WSABPropStorage::xECPropStorage::HrDeleteProps(LPSPropTagArray lpsPropTagArray)
{
	METHOD_PROLOGUE_(WSABPropStorage, ECPropStorage);
	return pThis->HrDeleteProps(lpsPropTagArray);
}

HRESULT WSABPropStorage::xECPropStorage::HrSaveObject(ULONG ulFlags, MAPIOBJECT *lpsMapiObject)
{
	METHOD_PROLOGUE_(WSABPropStorage, ECPropStorage);
	return pThis->HrSaveObject(ulFlags, lpsMapiObject);
}

HRESULT WSABPropStorage::xECPropStorage::HrLoadObject(MAPIOBJECT **lppsMapiObject)
{
	METHOD_PROLOGUE_(WSABPropStorage, ECPropStorage);
	return pThis->HrLoadObject(lppsMapiObject);
}

IECPropStorage* WSABPropStorage::xECPropStorage::GetServerStorage()
{
	METHOD_PROLOGUE_(WSABPropStorage, ECPropStorage);
	return pThis->GetServerStorage();
}
