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
#include "ECParentStorage.h"

#include "Mem.h"
#include "ECGuid.h"

#include <mapiutil.h>

// Utils
#include "SOAPUtils.h"
#include "WSUtil.h"
#include "Util.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/*
 * ECParentStorage implementation
 */

ECParentStorage::ECParentStorage(ECGenericProp *lpParentObject, ULONG ulUniqueId, ULONG ulObjId, IECPropStorage *lpServerStorage)
{
	m_lpParentObject = lpParentObject;
	if (m_lpParentObject)
		m_lpParentObject->AddRef();

	m_ulObjId = ulObjId;
	m_ulUniqueId = ulUniqueId;

	m_lpServerStorage = lpServerStorage;
	if (m_lpServerStorage)
		m_lpServerStorage->AddRef();
}

ECParentStorage::~ECParentStorage()
{
	if (m_lpParentObject)
		m_lpParentObject->Release();

	if (m_lpServerStorage)
		m_lpServerStorage->Release();
}

HRESULT ECParentStorage::QueryInterface(REFIID refiid, void **lppInterface)
{
	REGISTER_INTERFACE(IID_ECParentStorage, this);

	REGISTER_INTERFACE(IID_IECPropStorage, &this->m_xECPropStorage);

	return MAPI_E_INTERFACE_NOT_SUPPORTED;
}

HRESULT ECParentStorage::Create(ECGenericProp *lpParentObject, ULONG ulUniqueId, ULONG ulObjId, IECPropStorage *lpServerStorage, ECParentStorage **lppParentStorage)
{
	HRESULT hr = hrSuccess;
	ECParentStorage *lpParentStorage = NULL;

	lpParentStorage = new ECParentStorage(lpParentObject, ulUniqueId, ulObjId, lpServerStorage);

	hr = lpParentStorage->QueryInterface(IID_ECParentStorage, (void **)lppParentStorage); //FIXME: Use other interface

	if(hr != hrSuccess)
		goto exit;
	
exit:
	return hr;
}

HRESULT ECParentStorage::HrReadProps(LPSPropTagArray *lppPropTags, ULONG *lpcValues, LPSPropValue *lppValues)
{
	// this call should disappear
	return MAPI_E_NO_SUPPORT;
}

HRESULT ECParentStorage::HrLoadProp(ULONG ulObjId, ULONG ulPropTag, LPSPropValue *lppsPropValue)
{
	HRESULT hr = hrSuccess;

	if (m_lpServerStorage == NULL) {
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}

	hr = m_lpServerStorage->HrLoadProp(ulObjId, ulPropTag, lppsPropValue);
	if (hr != hrSuccess)
		goto exit;

exit:
	return hr;
}

HRESULT	ECParentStorage::HrWriteProps(ULONG cValues, LPSPropValue pValues, ULONG ulFlags)
{
	// this call should disappear
	return MAPI_E_NO_SUPPORT;
}

HRESULT ECParentStorage::HrDeleteProps(LPSPropTagArray lpsPropTagArray)
{
	// this call should disappear
	return MAPI_E_NO_SUPPORT;
}

HRESULT ECParentStorage::HrSaveObject(ULONG ulFlags, MAPIOBJECT *lpsMapiObject)
{
	HRESULT hr = hrSuccess;
	ECMapiObjects::iterator iterSObj;

	if (!m_lpParentObject) {
		hr = MAPI_E_INVALID_OBJECT;
		goto exit;
	}

	lpsMapiObject->ulUniqueId = m_ulUniqueId;
	lpsMapiObject->ulObjId = m_ulObjId;

	hr = m_lpParentObject->HrSaveChild(ulFlags, lpsMapiObject);
	if (hr != hrSuccess)
		goto exit;
	
exit:
	return hr;
}

HRESULT ECParentStorage::HrLoadObject(MAPIOBJECT **lppsMapiObject)
{
	HRESULT hr = hrSuccess;
	ECMapiObjects::iterator iterSObj;

	if (!m_lpParentObject)
		return MAPI_E_INVALID_OBJECT;
		
	pthread_mutex_lock(&m_lpParentObject->m_hMutexMAPIObject);
		
	if (!m_lpParentObject->m_sMapiObject) {
		hr = MAPI_E_INVALID_OBJECT;
		goto exit;
	}

	// type is either attachment or message-in-message
	{
		MAPIOBJECT find(MAPI_MESSAGE, m_ulUniqueId);
		MAPIOBJECT findAtt(MAPI_ATTACH, m_ulUniqueId);
	    iterSObj = m_lpParentObject->m_sMapiObject->lstChildren->find(&find);
	    if(iterSObj == m_lpParentObject->m_sMapiObject->lstChildren->end())
    		iterSObj = m_lpParentObject->m_sMapiObject->lstChildren->find(&findAtt);
	}
    	
	if (iterSObj == m_lpParentObject->m_sMapiObject->lstChildren->end()) {
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}

	// make a complete copy of the object, because of close / re-open
	*lppsMapiObject = new MAPIOBJECT(*iterSObj);

exit:
	pthread_mutex_unlock(&m_lpParentObject->m_hMutexMAPIObject);

	return hr;
}

IECPropStorage* ECParentStorage::GetServerStorage() {
	return m_lpServerStorage;
}

////////////////////////////////////////////////
// Interface IECPropStorage
//

ULONG ECParentStorage::xECPropStorage::AddRef()
{
	METHOD_PROLOGUE_(ECParentStorage, ECPropStorage);
	return pThis->AddRef();
}

ULONG ECParentStorage::xECPropStorage::Release()
{
	METHOD_PROLOGUE_(ECParentStorage, ECPropStorage);
	return pThis->Release();
}

HRESULT ECParentStorage::xECPropStorage::QueryInterface(REFIID refiid , void** lppInterface)
{
	METHOD_PROLOGUE_(ECParentStorage, ECPropStorage);
	return pThis->QueryInterface(refiid, lppInterface);
}

HRESULT ECParentStorage::xECPropStorage::HrReadProps(LPSPropTagArray *lppPropTags,ULONG *cValues, LPSPropValue *lppValues)
{
	METHOD_PROLOGUE_(ECParentStorage, ECPropStorage);
	return pThis->HrReadProps(lppPropTags,cValues, lppValues);
}
			
HRESULT ECParentStorage::xECPropStorage::HrLoadProp(ULONG ulObjId, ULONG ulPropTag, LPSPropValue *lppsPropValue)
{
	METHOD_PROLOGUE_(ECParentStorage, ECPropStorage);
	return pThis->HrLoadProp(ulObjId, ulPropTag, lppsPropValue);
}

HRESULT ECParentStorage::xECPropStorage::HrWriteProps(ULONG cValues, LPSPropValue lpValues, ULONG ulFlags)
{
	METHOD_PROLOGUE_(ECParentStorage, ECPropStorage);
	return pThis->HrWriteProps(cValues, lpValues, ulFlags);
}	

HRESULT ECParentStorage::xECPropStorage::HrDeleteProps(LPSPropTagArray lpsPropTagArray)
{
	METHOD_PROLOGUE_(ECParentStorage, ECPropStorage);
	return pThis->HrDeleteProps(lpsPropTagArray);
}

HRESULT ECParentStorage::xECPropStorage::HrSaveObject(ULONG ulFlags, MAPIOBJECT *lpsMapiObject)
{
	METHOD_PROLOGUE_(ECParentStorage, ECPropStorage);
	return pThis->HrSaveObject(ulFlags, lpsMapiObject);
}

HRESULT ECParentStorage::xECPropStorage::HrLoadObject(MAPIOBJECT **lppsMapiObject)
{
	METHOD_PROLOGUE_(ECParentStorage, ECPropStorage);
	return pThis->HrLoadObject(lppsMapiObject);
}

IECPropStorage* ECParentStorage::xECPropStorage::GetServerStorage() {
	METHOD_PROLOGUE_(ECParentStorage, ECPropStorage);
	return pThis->GetServerStorage();
}
