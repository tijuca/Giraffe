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
#include <mapidefs.h>
#include "WSTransport.h"
#include "ECGenericProp.h"

#include "Zarafa.h"
#include "ZarafaUtil.h"
#include "Mem.h"
#include "Util.h"

#include "ECGuid.h"

#include "ECDebug.h"

#include "charset/convert.h"
#include "EntryPoint.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

ECGenericProp::ECGenericProp(void *lpProvider, ULONG ulObjType, BOOL fModify, char *szClassName ) : ECUnknown(szClassName)
{
	this->lstProps		= NULL;
	this->lpStorage		= NULL;
	this->fSaved		= false; // not saved until we either read or write from/to disk
	this->ulObjType		= ulObjType;
	this->fModify		= fModify;
	this->dwLastError	= hrSuccess;
	this->lpProvider	= lpProvider;
	this->isTransactedObject = TRUE; // only ECMsgStore and ECMAPIFolder are not transacted
	this->ulObjFlags	= 0;
	this->m_sMapiObject = NULL;

	pthread_mutexattr_t mattr;
	pthread_mutexattr_init(&mattr);
	pthread_mutexattr_settype(&mattr, PTHREAD_MUTEX_RECURSIVE);

	pthread_mutex_init(&m_hMutexMAPIObject, &mattr);

	m_lpEntryId = NULL;
	m_cbEntryId = 0;
	m_bReload = FALSE;
	m_bLoading = FALSE;

	this->HrAddPropHandlers(PR_EC_OBJECT,				DefaultGetProp,			DefaultSetPropComputed, (void*) this, FALSE, TRUE);
	this->HrAddPropHandlers(PR_NULL,					DefaultGetProp,			DefaultSetPropIgnore,	(void*) this, FALSE, TRUE);
	this->HrAddPropHandlers(PR_OBJECT_TYPE,				DefaultGetProp,			DefaultSetPropComputed, (void*) this);
	this->HrAddPropHandlers(PR_ENTRYID,					DefaultGetProp,			DefaultSetPropComputed, (void*) this);
}

ECGenericProp::~ECGenericProp()
{
	ECPropertyEntryIterator iterProps;

	if (m_sMapiObject)
		FreeMapiObject(m_sMapiObject);

	if(lstProps) {
		for(iterProps = lstProps->begin(); iterProps != lstProps->end(); iterProps++)
			iterProps->second.DeleteProperty();

		delete lstProps;
	}

	if(lpStorage)
		lpStorage->Release();

	if(m_lpEntryId)
		MAPIFreeBuffer(m_lpEntryId);

	pthread_mutex_destroy(&m_hMutexMAPIObject);
}

HRESULT ECGenericProp::QueryInterface(REFIID refiid, void **lppInterface)
{
	REGISTER_INTERFACE(IID_ECUnknown, this);

	return MAPI_E_INTERFACE_NOT_SUPPORTED;
}

HRESULT ECGenericProp::SetProvider(void* lpProvider)
{
	HRESULT hr = hrSuccess;
	
	ASSERT(this->lpProvider == NULL);

	this->lpProvider = lpProvider;
	
	return hr;
}

HRESULT ECGenericProp::SetEntryId(ULONG cbEntryId, LPENTRYID lpEntryId)
{
	HRESULT hr;

	ASSERT(m_lpEntryId == NULL);
	
	hr = Util::HrCopyEntryId(cbEntryId, lpEntryId, &m_cbEntryId, &m_lpEntryId);
	if(hr != hrSuccess)
		goto exit;

exit:
	return hr;
}

// Add a property handler. Usually called by a subclass
HRESULT ECGenericProp::HrAddPropHandlers(ULONG ulPropTag, GetPropCallBack lpfnGetProp, SetPropCallBack lpfnSetProp, void *lpParam, BOOL fRemovable, BOOL fHidden)
{
	HRESULT					hr = hrSuccess;
	ECPropCallBackIterator	iterCallBack;
	PROPCALLBACK			sCallBack;

	// Check if the handler defines the right type, If Unicode you should never define a PT_STRING8 as handler!
	ASSERT( (PROP_TYPE(ulPropTag) == PT_STRING8 || PROP_TYPE(ulPropTag) == PT_UNICODE)?PROP_TYPE(ulPropTag) == PT_TSTRING: TRUE);

	// Only Support properties on ID, different types are not supported.
	iterCallBack = lstCallBack.find(PROP_ID(ulPropTag));
	if(iterCallBack != lstCallBack.end())
		lstCallBack.erase(iterCallBack);

	sCallBack.lpfnGetProp = lpfnGetProp;
	sCallBack.lpfnSetProp = lpfnSetProp;
	sCallBack.ulPropTag = ulPropTag;
	sCallBack.lpParam = lpParam;
	sCallBack.fRemovable = fRemovable;
	sCallBack.fHidden = fHidden;

	lstCallBack.insert(std::make_pair(PROP_ID(ulPropTag), sCallBack));

	dwLastError = hr;
	return hr;
}

// sets an actual value in memory
HRESULT ECGenericProp::HrSetRealProp(SPropValue *lpsPropValue)
{
	HRESULT					hr = hrSuccess;
	ECProperty*				lpProperty = NULL;
	ECPropertyEntryIterator	iterProps;
	ECPropertyEntryIterator iterPropsFound;
	ULONG ulPropId = 0;
	
	//FIXME: check the property structure -> lpsPropValue

	if (m_bLoading == FALSE && m_sMapiObject) {
		// Only reset instance id when we're being modified, not being reloaded
		HrSIEntryIDToID(m_sMapiObject->cbInstanceID, (LPBYTE)m_sMapiObject->lpInstanceID, NULL, NULL, (unsigned int *)&ulPropId);

		if (ulPropId == PROP_ID(lpsPropValue->ulPropTag)) {
			SetSingleInstanceId(0, NULL);
		}
	}

	if(lstProps == NULL) {
		hr = HrLoadProps();
		if(hr != hrSuccess)
			goto exit;
	}			

	iterPropsFound = lstProps->end();
	// Loop through all properties, get the first EXACT matching property, but delete ALL
	// other properties with this PROP_ID and the wrong type - this makes sure you can SetProps() with 0x60010003,
	// then with 0x60010102 and then with 0x60010040 for example.

	iterProps = lstProps->find(PROP_ID(lpsPropValue->ulPropTag));
	if (iterProps != lstProps->end()) {
		if (iterProps->second.GetPropTag() != lpsPropValue->ulPropTag) {
			// type is different, remove the property and insert a new item
			m_setDeletedProps.insert(lpsPropValue->ulPropTag);

			iterProps->second.DeleteProperty();

			lstProps->erase(iterProps);
		} else {
			iterPropsFound = iterProps;
		}
	}

	// Changing an existing property
	if(iterPropsFound != lstProps->end()) {
		iterPropsFound->second.HrSetProp(lpsPropValue);
	} else { // Add new property
		lpProperty = new ECProperty(lpsPropValue);

		if(lpProperty->GetLastError() != 0) {
			hr = lpProperty->GetLastError();
			goto exit;
		}

		ECPropertyEntry entry(lpProperty);

		lstProps->insert(std::make_pair(PROP_ID(lpsPropValue->ulPropTag), entry));
	}

	// Property is now added/modified and marked 'dirty' for saving
exit:
	if(hr != hrSuccess && lpProperty)
		delete lpProperty;

	dwLastError = hr;
	return hr;
}

// Get an actual value from the property array and saves it to the given address. Any extra memory required
// is allocated with MAPIAllocMore with lpBase as the base pointer
//
// Properties are always returned unless:
//
// 1) The underlying system didn't return them in the initial HrReadProps call, and as such are 'too large' and
//    the property has not been force-loaded with OpenProperty
// or
// 2) A MaxSize was specified and the property is larger than that size (normally 8k or so)
//

HRESULT ECGenericProp::HrGetRealProp(ULONG ulPropTag, ULONG ulFlags, void *lpBase, LPSPropValue lpsPropValue, ULONG ulMaxSize)
{
	HRESULT					hr = hrSuccess;
	ECPropertyEntryIterator iterProps;
	
	if(lstProps == NULL || m_bReload == TRUE) {
		hr = HrLoadProps();
		if(hr != hrSuccess)
			goto exit;
		m_bReload = FALSE;
	}			

	// Find the property in our list
	iterProps = lstProps->find(PROP_ID(ulPropTag));

	// Not found or property is not matching
	if(iterProps == lstProps->end() || !( PROP_TYPE(ulPropTag) == PT_UNSPECIFIED || PROP_TYPE(ulPropTag) == PROP_TYPE(iterProps->second.GetPropTag()) ||
		(((ulPropTag & MV_FLAG) == (iterProps->second.GetPropTag( ) & MV_FLAG)) && PROP_TYPE(ulPropTag&~MV_FLAG) == PT_STRING8 && PROP_TYPE(iterProps->second.GetPropTag()&~MV_FLAG) == PT_UNICODE) ))
	{
		lpsPropValue->ulPropTag = PROP_TAG(PT_ERROR,PROP_ID(ulPropTag));
		lpsPropValue->Value.err = MAPI_E_NOT_FOUND;
		hr = MAPI_W_ERRORS_RETURNED;
		goto exit;
	}

	if(!iterProps->second.FIsLoaded()) {
		lpsPropValue->ulPropTag = PROP_TAG(PT_ERROR, PROP_ID(ulPropTag));
		lpsPropValue->Value.err = MAPI_E_NOT_ENOUGH_MEMORY;
		hr = MAPI_W_ERRORS_RETURNED;
		goto exit;

		// The load should have loaded into the value pointed to by iterProps, so we can use that now
	}

	// Check if a max. size was requested, if so, dont return unless smaller than max. size
	if(ulMaxSize) {
		if(iterProps->second.GetProperty()->GetSize() > ulMaxSize) {
			lpsPropValue->ulPropTag = PROP_TAG(PT_ERROR, PROP_ID(ulPropTag));
			lpsPropValue->Value.err = MAPI_E_NOT_ENOUGH_MEMORY;
			hr = MAPI_W_ERRORS_RETURNED;
			goto exit;
		}
	}

	if (PROP_TYPE(ulPropTag) == PT_UNSPECIFIED) {
		if (PROP_TYPE(iterProps->second.GetPropTag()) == PT_UNICODE) {
			ulPropTag = CHANGE_PROP_TYPE(ulPropTag, ((ulFlags & MAPI_UNICODE) ? PT_UNICODE : PT_STRING8));
		} else if (PROP_TYPE(iterProps->second.GetPropTag()) == PT_MV_UNICODE) {
			ulPropTag = CHANGE_PROP_TYPE(ulPropTag, ((ulFlags & MAPI_UNICODE) ? PT_MV_UNICODE : PT_MV_STRING8));
		} else
			ulPropTag = iterProps->second.GetPropTag();
	}

	// Copy the property to its final destination, with base pointer for extra allocations, if required.
	iterProps->second.GetProperty()->CopyTo(lpsPropValue, lpBase, ulPropTag);

exit:
	dwLastError = hr;

	return hr;
}

/** 
 * Deletes a property from the internal system
 * 
 * @param ulPropTag The requested ulPropTag to remove. Property type is ignored; only the property identifier is used.
 * @param fOverwriteRO Set to TRUE if the object is to be modified eventhough it's read-only. Currently unused! @todo parameter should be removed.
 * 
 * @return MAPI error code
 * @retval hrSuccess PropTag is set to be removed during the next SaveChanges call
 * @retval MAPI_E_NOT_FOUND The PropTag is not found in the list (can be type mismatch too)
 */
HRESULT ECGenericProp::HrDeleteRealProp(ULONG ulPropTag, BOOL fOverwriteRO)
{
	HRESULT					hr = hrSuccess;
	ECPropertyEntryIterator iterProps;

	if(lstProps == NULL) {
		hr = HrLoadProps();
		if(hr != hrSuccess)
			goto exit;
	}			

	// Now find the real value
	iterProps = lstProps->find(PROP_ID(ulPropTag));
	if(iterProps == lstProps->end()) {
		// Couldn't find it!
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}

	m_setDeletedProps.insert(iterProps->second.GetPropTag());

	iterProps->second.DeleteProperty();

	lstProps->erase(iterProps);

exit:
	dwLastError = hr;
	return hr;
}

////////////////////////////////////////////////////////////
// Default property handles
//

HRESULT	ECGenericProp::DefaultGetProp(ULONG ulPropTag,  void* lpProvider, ULONG ulFlags, LPSPropValue lpsPropValue, void *lpParam, void *lpBase)
{
	HRESULT			hr = hrSuccess;
	ECGenericProp*	lpProp = (ECGenericProp *)lpParam;

	switch(PROP_ID(ulPropTag))
	{
		case PROP_ID(PR_ENTRYID):
			if(lpProp->m_cbEntryId) {
				lpsPropValue->ulPropTag = PR_ENTRYID;
				lpsPropValue->Value.bin.cb = lpProp->m_cbEntryId;
				if(lpBase == NULL)
					ASSERT(FALSE);

				ECAllocateMore(lpProp->m_cbEntryId, lpBase, (void **)&lpsPropValue->Value.bin.lpb);
				memcpy(lpsPropValue->Value.bin.lpb, lpProp->m_lpEntryId, lpProp->m_cbEntryId);
			} else {
				hr = MAPI_E_NOT_FOUND;
			}
			break;

		// Gives access to the actual ECUnknown underlying object
		case PROP_ID(PR_EC_OBJECT):
			// NOTE: we place the object pointer in lpszA to make sure it's on the same offset as Value.x on 32bit as 64bit machines
			lpsPropValue->ulPropTag = PR_EC_OBJECT;
			lpsPropValue->Value.lpszA = (LPSTR)lpProp;
			break;

		case PROP_ID(PR_NULL):
			// outlook with export contacts to csv (IMessage)(0x00000000) <- skip this one
			// Palm used PR_NULL (IMAPIFolder)(0x00000001)
			if(ulPropTag == PR_NULL) {
				lpsPropValue->ulPropTag = PR_NULL;
				memset(&lpsPropValue->Value, 0, sizeof(lpsPropValue->Value)); // make sure all bits, 32 or 64, are 0
			} else {
				hr = MAPI_E_NOT_FOUND;
			}
			break;

		case PROP_ID(PR_OBJECT_TYPE): 
			lpsPropValue->Value.l = lpProp->ulObjType;
			lpsPropValue->ulPropTag = PR_OBJECT_TYPE;
			break;

		default:
			hr = lpProp->HrGetRealProp(ulPropTag, ulFlags, lpBase, lpsPropValue);
			break;
	}

	return hr;
}

HRESULT	ECGenericProp::DefaultGetPropGetReal(ULONG ulPropTag, void* lpProvider, ULONG ulFlags, LPSPropValue lpsPropValue, void *lpParam, void *lpBase)
{
	ECGenericProp *lpProp = (ECGenericProp *)lpParam;

	return lpProp->HrGetRealProp(ulPropTag, ulFlags, lpBase, lpsPropValue, 8192);
}

HRESULT ECGenericProp::DefaultSetPropSetReal(ULONG ulPropTag, void* lpProvider, LPSPropValue lpsPropValue, void *lpParam)
{
	ECGenericProp *lpProp = (ECGenericProp *)lpParam;

	return lpProp->HrSetRealProp(lpsPropValue);
}

HRESULT	ECGenericProp::DefaultSetPropComputed(ULONG ulPropTag, void* lpProvider, LPSPropValue lpsPropValue, void *lpParam)
{
	return MAPI_E_COMPUTED;
}

HRESULT	ECGenericProp::DefaultSetPropIgnore(ULONG ulPropTag, void* lpProvider, LPSPropValue lpsPropValue, void *lpParam)
{
	return hrSuccess;
}

HRESULT ECGenericProp::TableRowGetProp(void* lpProvider, struct propVal *lpsPropValSrc, LPSPropValue lpsPropValDst, void **lpBase, ULONG ulType)
{
	HRESULT hr = hrSuccess;

	switch(lpsPropValSrc->ulPropTag) {
		case PROP_TAG(PT_ERROR,PROP_ID(PR_NULL)): 
			lpsPropValDst->Value.l = 0;
			lpsPropValDst->ulPropTag = PR_NULL;
			break;
		default:
			hr = MAPI_E_NOT_FOUND;
			break;
	}

	return hr;
}

// Sets all the properties 'clean', ie. un-dirty
HRESULT ECGenericProp::HrSetClean()
{
	HRESULT hr = hrSuccess;
	ECPropertyEntryIterator iterProps;

	// also remove deleted marked properties, since the object isn't reloaded from the server anymore
	for(iterProps = lstProps->begin(); iterProps != lstProps->end(); iterProps++) {
			iterProps->second.HrSetClean();
	}

	m_setDeletedProps.clear();

	return hr;
}

HRESULT ECGenericProp::HrSaveChild(ULONG ulFlags, MAPIOBJECT *lpsMapiObject)
{
	// ECMessage implements saving an attachment
	// ECAttach implements saving a sub-message
	return MAPI_E_INVALID_OBJECT;
}

HRESULT ECGenericProp::HrRemoveModifications(MAPIOBJECT *lpsMapiObject, ULONG ulPropTag)
{
	HRESULT hr = hrSuccess;
	std::list<ECProperty>::iterator iterProps;

	lpsMapiObject->lstDeleted->remove(ulPropTag);

	for(iterProps = lpsMapiObject->lstModified->begin(); iterProps != lpsMapiObject->lstModified->end(); iterProps++) {
		if(iterProps->GetPropTag() == ulPropTag) {
			lpsMapiObject->lstModified->erase(iterProps);
			break;
		}
	}

	return hr;
}


HRESULT ECGenericProp::GetLastError(HRESULT hResult, ULONG ulFlags, LPMAPIERROR FAR * lppMAPIError)
{
	HRESULT		hr = hrSuccess;
	LPMAPIERROR	lpMapiError = NULL;
	LPTSTR		lpszErrorMsg = NULL;
	
	hr = Util::HrMAPIErrorToText((hResult == hrSuccess)?MAPI_E_NO_ACCESS : hResult, &lpszErrorMsg);
	if (hr != hrSuccess)
		goto exit;

	hr = ECAllocateBuffer(sizeof(MAPIERROR),(void **)&lpMapiError);
	if(hr != hrSuccess)
		goto exit;
		
	if ((ulFlags & MAPI_UNICODE) == MAPI_UNICODE) {
		std::wstring wstrErrorMsg = convert_to<std::wstring>(lpszErrorMsg);
		std::wstring wstrCompName = convert_to<std::wstring>(g_strProductName.c_str());

		MAPIAllocateMore(sizeof(std::wstring::value_type) * (wstrErrorMsg.size() + 1), lpMapiError, (void**)&lpMapiError->lpszError);
		wcscpy((wchar_t*)lpMapiError->lpszError, wstrErrorMsg.c_str());

		MAPIAllocateMore(sizeof(std::wstring::value_type) * (wstrCompName.size() + 1), lpMapiError, (void**)&lpMapiError->lpszComponent);
		wcscpy((wchar_t*)lpMapiError->lpszComponent, wstrCompName.c_str()); 

	} else {
		std::string strErrorMsg = convert_to<std::string>(lpszErrorMsg);
		std::string strCompName = convert_to<std::string>(g_strProductName.c_str());

		MAPIAllocateMore(strErrorMsg.size() + 1, lpMapiError, (void**)&lpMapiError->lpszError);
		strcpy((char*)lpMapiError->lpszError, strErrorMsg.c_str());

		MAPIAllocateMore(strCompName.size() + 1, lpMapiError, (void**)&lpMapiError->lpszComponent);
		strcpy((char*)lpMapiError->lpszComponent, strCompName.c_str());
	}

	lpMapiError->ulContext		= 0;
	lpMapiError->ulLowLevelError= 0;
	lpMapiError->ulVersion		= 0;

	*lppMAPIError = lpMapiError;

exit:
	if (lpszErrorMsg)
		MAPIFreeBuffer(lpszErrorMsg);

	if( hr != hrSuccess && lpMapiError)
		ECFreeBuffer(lpMapiError);

	return hr;
}

// Differential save of changed properties
HRESULT ECGenericProp::SaveChanges(ULONG ulFlags)
{
	HRESULT			hr = hrSuccess;
	ECPropertyEntryIterator iterProps;
	std::list<ULONG>::iterator iterPropTags;
	std::list<ECProperty>::iterator iterPropVals;
	std::set<ULONG>::iterator iterDelProps;

	pthread_mutex_lock(&m_hMutexMAPIObject);

	if (!fModify) {
		hr = MAPI_E_NO_ACCESS;
		goto exit;
	}

	if (!m_sMapiObject || !lstProps) {
		hr = MAPI_E_CALL_FAILED;
		goto exit;
	}

	// no props -> succeed (no changes made)
	if(lstProps->empty())
		goto exit;

	if(lpStorage == NULL) {
		// no way to save our properties !
		hr = MAPI_E_NO_ACCESS;
		goto exit;
	}

	// Note: m_sMapiObject->lstProperties and m_sMapiObject->lstAvailable are empty
	// here, because they are cleared after HrLoadProps and SaveChanges

	// save into m_sMapiObject
	
	for (iterDelProps = m_setDeletedProps.begin(); iterDelProps != m_setDeletedProps.end(); iterDelProps++) {
		// Make sure the property is not present in deleted/modified list
		HrRemoveModifications(m_sMapiObject, *iterDelProps);

		m_sMapiObject->lstDeleted->push_back(*iterDelProps);
	}

	for (iterProps = lstProps->begin(); iterProps != lstProps->end(); iterProps++) {

		// Property is dirty, so we have to save it
		if (iterProps->second.FIsDirty()) {
			// Save in the 'modified' list

			// Make sure the property is not present in deleted/modified list
			HrRemoveModifications(m_sMapiObject, iterProps->second.GetPropTag());

			// Save modified property
			m_sMapiObject->lstModified->push_back(*iterProps->second.GetProperty());
			
			// Save in the normal properties list
			m_sMapiObject->lstProperties->push_back(*iterProps->second.GetProperty());
			continue;
		}

		// Normal property: either non-loaded or loaded
		if (!iterProps->second.FIsLoaded())	// skip pt_error anyway
			m_sMapiObject->lstAvailable->push_back(iterProps->second.GetPropTag());
		else
			m_sMapiObject->lstProperties->push_back(*iterProps->second.GetProperty());
	}

	m_sMapiObject->bChanged = true;

	// Our s_MapiObject now contains its full property list in lstProperties and lstAvailable,
	// and its modifications in lstModified and lstDeleted.

	// save to parent or server
	hr = lpStorage->HrSaveObject(this->ulObjFlags, m_sMapiObject);
	if (hr != hrSuccess)
		goto exit;

	// HrSaveObject() has appended any new properties in lstAvailable and lstProperties. We need to load the 
	// new properties. The easiest way to do this is to simply load all properties. Note that in embedded objects
	// that save to ECParentStorage, the object will be untouched. The code below will do nothing.

	// Large properties received
	for(iterPropTags = m_sMapiObject->lstAvailable->begin(); iterPropTags != m_sMapiObject->lstAvailable->end(); iterPropTags++) {
		
		// ONLY if not present
		iterProps = lstProps->find(PROP_ID(*iterPropTags));
		if (iterProps == lstProps->end() || iterProps->second.GetPropTag() != *iterPropTags) {
			ECPropertyEntry entry(*iterPropTags);
			lstProps->insert(std::make_pair(PROP_ID(*iterPropTags), entry));
		}
	}
	m_sMapiObject->lstAvailable->clear();

	// Normal properties with value
	for (iterPropVals = m_sMapiObject->lstProperties->begin(); iterPropVals != m_sMapiObject->lstProperties->end(); iterPropVals++) {
		// don't add any 'error' types ... (the storage object shouldn't really give us these anyway ..)
		if (PROP_TYPE((*iterPropVals).GetPropTag()) != PT_ERROR) {
			SPropValue tmp = iterPropVals->GetMAPIPropValRef();
			HrSetRealProp(&tmp);
		}
	}

	// Note that we currently don't support the server removing properties after the SaveObject call

	// We have loaded all properties, so clear the properties in the m_sMapiObject
	m_sMapiObject->lstProperties->clear();
	m_sMapiObject->lstAvailable->clear();

	// We are now in sync with the server again, so set everything as clean
	HrSetClean();

	fSaved = true;

exit:
	if (hr == hrSuccess)
	{
		// Unless the user requests to continue with modify access, switch
		// down to read-only access. This means that specifying neither of
		// the KEEP_OPEN flags means the same thing as KEEP_OPEN_READONLY.
		if (!(ulFlags & (KEEP_OPEN_READWRITE|FORCE_SAVE)))
			fModify = FALSE;
	}

	pthread_mutex_unlock(&m_hMutexMAPIObject);

	return hr;
}

// Check if property is dirty (delete properties gives MAPI_E_NOT_FOUND)
HRESULT ECGenericProp::IsPropDirty(ULONG ulPropTag, BOOL *lpbDirty)
{
	HRESULT					hr = hrSuccess;
	ECPropertyEntryIterator iterProps;

	iterProps = lstProps->find(PROP_ID(ulPropTag));
	if(iterProps == lstProps->end() || (PROP_TYPE(ulPropTag) != PT_UNSPECIFIED && ulPropTag != iterProps->second.GetPropTag()) ) {
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}
	
	*lpbDirty = iterProps->second.FIsDirty();

exit: 
	return hr;
}

/**
 * Clears the 'dirty' flag for a property
 *
 * This means that during a save the property will not be sent to the server. The dirty flag will be set
 * again after a HrSetRealProp() again.
 *
 * @param[in] ulPropTag Property to mark un-dirty
 * @result HRESULT
 */
HRESULT ECGenericProp::HrSetCleanProperty(ULONG ulPropTag)
{
	HRESULT					hr = hrSuccess;
	ECPropertyEntryIterator iterProps;

	iterProps = lstProps->find(PROP_ID(ulPropTag));
	if(iterProps == lstProps->end() || (PROP_TYPE(ulPropTag) != PT_UNSPECIFIED && ulPropTag != iterProps->second.GetPropTag()) ) {
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}
	
	iterProps->second.HrSetClean();

exit: 
	return hr;
}

// Get the handler(s) for a given property tag
HRESULT	ECGenericProp::HrGetHandler(ULONG ulPropTag, SetPropCallBack *lpfnSetProp, GetPropCallBack *lpfnGetProp, void **lpParam)
{
	HRESULT					hr = hrSuccess;
	ECPropCallBackIterator iterCallBack;

	iterCallBack = lstCallBack.find(PROP_ID(ulPropTag));
	if(iterCallBack == lstCallBack.end() || 
		(ulPropTag != iterCallBack->second.ulPropTag && PROP_TYPE(ulPropTag) != PT_UNSPECIFIED &&
		!(PROP_TYPE(iterCallBack->second.ulPropTag) == PT_TSTRING && (PROP_TYPE(ulPropTag) == PT_STRING8 || PROP_TYPE(ulPropTag) == PT_UNICODE) )
		) ) {
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}

	if(lpfnSetProp)
		*lpfnSetProp = iterCallBack->second.lpfnSetProp;

	if(lpfnGetProp)
		*lpfnGetProp = iterCallBack->second.lpfnGetProp;

	if(lpParam)
		*lpParam = iterCallBack->second.lpParam;
	
exit:
	dwLastError = hr;
	return hr;
}

HRESULT ECGenericProp::HrSetPropStorage(IECPropStorage *lpStorage, BOOL fLoadProps)
{
	HRESULT hr = hrSuccess;
	SPropValue sPropValue;

	if(this->lpStorage)
		this->lpStorage->Release();

	this->lpStorage = lpStorage;

	if(lpStorage)
		lpStorage->AddRef();

	if(fLoadProps) {
		hr = HrLoadProps();
		if(hr != hrSuccess)
			goto exit;
			
		if(HrGetRealProp(PR_OBJECT_TYPE, 0, NULL, &sPropValue, 8192) == hrSuccess) {
			// The server sent a PR_OBJECT_TYPE, check if it is correct
			if(this->ulObjType != sPropValue.Value.ul) {
				// Return NOT FOUND because the entryid given was the incorrect type. This means
				// that the object was basically not found.
				hr = MAPI_E_NOT_FOUND;
				goto exit;
			}
		}
	}
exit:
		
	return hr;
}

HRESULT ECGenericProp::HrLoadEmptyProps()
{
	pthread_mutex_lock(&m_hMutexMAPIObject);

	ASSERT(lstProps == NULL);
	ASSERT(m_sMapiObject == NULL);

	lstProps = new ECPropertyEntryMap;
	AllocNewMapiObject(0, 0, ulObjType, &m_sMapiObject);

	pthread_mutex_unlock(&m_hMutexMAPIObject);

	return hrSuccess;
}

// Loads the properties of the saved message for use
HRESULT ECGenericProp::HrLoadProps()
{
	HRESULT			hr = hrSuccess;
	ECPropertyEntryIterator iterProps;
	std::list<ULONG>::iterator iterPropTags;
	std::list<ECProperty>::iterator iterPropVals;

	if(lpStorage == NULL)
		return MAPI_E_CALL_FAILED;

	pthread_mutex_lock(&m_hMutexMAPIObject);

	if(lstProps != NULL && m_bReload == FALSE) {
		goto exit; // already loaded
	}

	m_bLoading = TRUE;

	if (m_sMapiObject != NULL) {
		// remove what we know, (scenario: keep open r/w, drop props, get all again causes to know the server changes, incl. the hierarchy id)
		FreeMapiObject(m_sMapiObject);
		m_sMapiObject = NULL;

		// only remove my own properties: keep recipients and attachment tables
		for(iterProps = lstProps->begin(); iterProps != lstProps->end(); iterProps++)
			iterProps->second.DeleteProperty();

		lstProps->clear();
		m_setDeletedProps.clear();
	}

	hr = lpStorage->HrLoadObject(&m_sMapiObject);
	if (hr != hrSuccess)
		goto exit;

	if (lstProps == NULL)
		lstProps = new ECPropertyEntryMap;

	// Add *all* the entries as with empty values; values for these properties will be
	// retrieved on-demand
	for(iterPropTags = m_sMapiObject->lstAvailable->begin(); iterPropTags != m_sMapiObject->lstAvailable->end(); iterPropTags++) {
		ECPropertyEntry entry(*iterPropTags);

		lstProps->insert(std::make_pair(PROP_ID(*iterPropTags), entry));
	}

	// Load properties
	for (iterPropVals = m_sMapiObject->lstProperties->begin(); iterPropVals != m_sMapiObject->lstProperties->end(); iterPropVals++) {
		// don't add any 'error' types ... (the storage object shouldn't really give us these anyway ..)
		if (PROP_TYPE((*iterPropVals).GetPropTag()) != PT_ERROR) {
			SPropValue tmp = iterPropVals->GetMAPIPropValRef();
			HrSetRealProp(&tmp);
		}
	}

	// remove copied proptags, subobjects are still present
	m_sMapiObject->lstAvailable->clear();
	m_sMapiObject->lstProperties->clear(); // pointers are now only present in lstProps (this removes memory usage!)

	// at this point: children still known, ulObjId and ulObjType too

	// Mark all properties now in memory as 'clean' (need not be saved)
	hr = HrSetClean();

	if(hr != hrSuccess)
		goto exit;

	// We just read the properties from the disk, so it is a 'saved' (ie on-disk) message
	fSaved = true;

exit:
	dwLastError = hr;
	m_bReload = FALSE;
	m_bLoading = FALSE;

	pthread_mutex_unlock(&m_hMutexMAPIObject);

	return hr;
}

// Load a single (large) property from the storage
HRESULT ECGenericProp::HrLoadProp(ULONG ulPropTag)
{
	HRESULT			hr = hrSuccess;
	LPSPropValue	lpsPropVal = NULL;

	ECPropertyEntryIterator	iterProps;

	if(lpStorage == NULL)
		return MAPI_E_CALL_FAILED;

	ulPropTag = NormalizePropTag(ulPropTag);

	pthread_mutex_lock(&m_hMutexMAPIObject);

	if(lstProps == NULL || m_bReload == TRUE) {
		hr = HrLoadProps();
		if(hr != hrSuccess)
			goto exit;
	}			

	iterProps = lstProps->find(PROP_ID(ulPropTag));
	if(iterProps == lstProps->end() || (PROP_TYPE(ulPropTag) != PT_UNSPECIFIED && PROP_TYPE(ulPropTag) != PROP_TYPE(iterProps->second.GetPropTag())) ) {
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}

	// Don't load the data if it was already loaded
	if(iterProps->second.FIsLoaded()) {
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}

  	// The property was not loaded yet, demand-load it now
	hr = lpStorage->HrLoadProp(m_sMapiObject->ulObjId, iterProps->second.GetPropTag(), &lpsPropVal);
	if(hr != hrSuccess)
		goto exit;

	hr = iterProps->second.HrSetProp(new ECProperty(lpsPropVal));
	if(hr != hrSuccess)
		goto exit;

	// It's clean 'cause we just loaded it
	iterProps->second.HrSetClean();

exit:

	if(lpsPropVal)
		ECFreeBuffer(lpsPropVal);

	pthread_mutex_unlock(&m_hMutexMAPIObject);

	return hr;
}

HRESULT ECGenericProp::GetProps(LPSPropTagArray lpPropTagArray, ULONG ulFlags, ULONG FAR * lpcValues, LPSPropValue FAR * lppPropArray)
{
	HRESULT			hr = hrSuccess;
	HRESULT			hrT = hrSuccess;
	LPSPropTagArray	lpGetPropTagArray = lpPropTagArray;
	GetPropCallBack	lpfnGetProp = NULL;
	void*			lpParam = NULL;
	LPSPropValue	lpsPropValue = NULL;
	unsigned int	i;

	//FIXME: check lpPropTagArray on PROP_TYPE()
	if((lpPropTagArray != NULL && lpPropTagArray->cValues == 0) || Util::ValidatePropTagArray(lpPropTagArray) == false)
		return MAPI_E_INVALID_PARAMETER;

	if(lpGetPropTagArray == NULL) {
		hr = GetPropList(ulFlags, &lpGetPropTagArray);

		if(hr != hrSuccess)
			goto exit;
	}

	ECAllocateBuffer(sizeof(SPropValue) * lpGetPropTagArray->cValues, (LPVOID *)&lpsPropValue);

	for(i=0;i < lpGetPropTagArray->cValues; i++) {
		if(HrGetHandler(lpGetPropTagArray->aulPropTag[i], NULL, &lpfnGetProp, &lpParam) == hrSuccess) {
			lpsPropValue[i].ulPropTag = lpGetPropTagArray->aulPropTag[i];

			hrT = lpfnGetProp(lpGetPropTagArray->aulPropTag[i], this->lpProvider, ulFlags, &lpsPropValue[i], lpParam, lpsPropValue);
		} else {
			hrT = HrGetRealProp(lpGetPropTagArray->aulPropTag[i], ulFlags, lpsPropValue, &lpsPropValue[i], 8192);
			if(hrT != hrSuccess && hrT != MAPI_E_NOT_FOUND && hrT != MAPI_E_NOT_ENOUGH_MEMORY && hrT != MAPI_W_ERRORS_RETURNED) {
				hr = hrT;
				goto exit;
			}
		}

		if(HR_FAILED(hrT)) {
			lpsPropValue[i].ulPropTag = PROP_TAG(PT_ERROR,PROP_ID(lpGetPropTagArray->aulPropTag[i]));
			lpsPropValue[i].Value.err = hrT;
			hr = MAPI_W_ERRORS_RETURNED;
		} else if(hrT != hrSuccess) {
			hr = MAPI_W_ERRORS_RETURNED;
		}
	}

	*lppPropArray = lpsPropValue;
	*lpcValues = lpGetPropTagArray->cValues;
exit:

	if(lpPropTagArray == NULL)
		ECFreeBuffer(lpGetPropTagArray);

	return hr;

}

HRESULT ECGenericProp::GetPropList(ULONG ulFlags, LPSPropTagArray FAR * lppPropTagArray)
{
	HRESULT				hr = hrSuccess;
	LPSPropTagArray		lpPropTagArray = NULL;
	int					n = 0;

	ECPropCallBackIterator	iterCallBack;
	ECPropertyEntryIterator	iterProps;
	
	if(lstProps == NULL) {
		hr = HrLoadProps();
		if(hr != hrSuccess)
			goto exit;
	}			

	// The size of the property tag array is never larger than (static properties + generated properties)
	ECAllocateBuffer(CbNewSPropTagArray(lstProps->size() + lstCallBack.size()), (LPVOID *)&lpPropTagArray);

	// Some will overlap so we've actually allocated slightly too much memory

	// Add the callback types first
	for(iterCallBack = lstCallBack.begin(); iterCallBack != lstCallBack.end(); iterCallBack++) {

		// Don't add 'hidden' properties
		if(iterCallBack->second.fHidden)
			continue;

		// Check if the callback actually returns OK
		// a bit wasteful but fine for now.

		LPSPropValue lpsPropValue = NULL;
		HRESULT hrT = hrSuccess;

		ECAllocateBuffer(sizeof(SPropValue), (LPVOID *)&lpsPropValue);

		hrT = iterCallBack->second.lpfnGetProp(iterCallBack->second.ulPropTag, this->lpProvider, ulFlags, lpsPropValue, this, lpsPropValue);

		if((!HR_FAILED(hrT) || hrT == MAPI_E_NOT_ENOUGH_MEMORY) && (PROP_TYPE(lpsPropValue->ulPropTag) != PT_ERROR || lpsPropValue->Value.err == MAPI_E_NOT_ENOUGH_MEMORY)) {
			ULONG ulPropTag = iterCallBack->second.ulPropTag;
			
			if(PROP_TYPE(ulPropTag) == PT_UNICODE || PROP_TYPE(ulPropTag) == PT_STRING8) {
				ulPropTag = CHANGE_PROP_TYPE(ulPropTag, ((ulFlags & MAPI_UNICODE) ? PT_UNICODE : PT_STRING8));
			}
			
			lpPropTagArray->aulPropTag[n++] = ulPropTag;
		}

		if(lpsPropValue)
			ECFreeBuffer(lpsPropValue);

	}

	// Then add the others, if not added yet
	for(iterProps = lstProps->begin(); iterProps != lstProps->end() ; iterProps++) {
		if(HrGetHandler(iterProps->second.GetPropTag(),NULL,NULL,NULL) != 0) {
			ULONG ulPropTag = iterProps->second.GetPropTag();

			if(!(ulFlags & MAPI_UNICODE)) {
				// Downgrade to ansi
				if(PROP_TYPE(ulPropTag) == PT_UNICODE)
					ulPropTag = PROP_TAG(PT_STRING8, PROP_ID(ulPropTag));
				else if(PROP_TYPE(ulPropTag) == PT_MV_UNICODE)
					ulPropTag = PROP_TAG(PT_MV_STRING8, PROP_ID(ulPropTag));
			}

			lpPropTagArray->aulPropTag[n++] = ulPropTag;
		}
	}

	lpPropTagArray->cValues = n;

	*lppPropTagArray = lpPropTagArray;

exit:

	return hr;
}

HRESULT ECGenericProp::OpenProperty(ULONG ulPropTag, LPCIID lpiid, ULONG ulInterfaceOptions, ULONG ulFlags, LPUNKNOWN FAR * lppUnk)
{
	return  MAPI_E_NO_SUPPORT;
}

HRESULT ECGenericProp::SetProps(ULONG cValues, LPSPropValue lpPropArray, LPSPropProblemArray FAR * lppProblems)
{
	HRESULT				hr = hrSuccess;
	HRESULT				hrT = hrSuccess;
	LPSPropProblemArray	lpProblems = NULL;
	int					nProblem = 0;
	SetPropCallBack		lpfnSetProp = NULL;
	void*				lpParam = NULL;

	if (lpPropArray == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}
	
	hr = ECAllocateBuffer(CbNewSPropProblemArray(cValues), (LPVOID *)&lpProblems);
	if(hr != hrSuccess)
		goto exit;

	for(unsigned int i=0;i<cValues;i++) {
		// Ignore the PR_NULL property tag and all properties with a type of PT_ERROR;
		// no changes and report no problems in the SPropProblemArray structure. 
		if(PROP_TYPE(lpPropArray[i].ulPropTag) == PR_NULL ||
			PROP_TYPE(lpPropArray[i].ulPropTag) == PT_ERROR)
			continue;

		if(HrGetHandler(lpPropArray[i].ulPropTag, &lpfnSetProp, NULL, &lpParam) == hrSuccess) {
			hrT = lpfnSetProp(lpPropArray[i].ulPropTag, this->lpProvider, &lpPropArray[i], lpParam);
		} else {
			hrT = HrSetRealProp(&lpPropArray[i]); // SC: TODO: this does a ref copy ?!
		}

		if(hrT != hrSuccess) {
			lpProblems->aProblem[nProblem].scode = hrT;
			lpProblems->aProblem[nProblem].ulIndex = i;
			lpProblems->aProblem[nProblem].ulPropTag = lpPropArray[i].ulPropTag; // Hold here the real property
			nProblem++;
		}
	}

	lpProblems->cProblem = nProblem;

	if(lppProblems && nProblem) {
		*lppProblems = lpProblems;
		lpProblems = NULL; // Don't delete lpProblems
	} else if(lppProblems) {
		*lppProblems = NULL;
	}

exit:
	if(lpProblems)
		ECFreeBuffer(lpProblems);

	return hr;
}

/** 
 * Delete properties from the current object
 * 
 * @param lpPropTagArray PropTagArray with properties to remove from the object. If a property from this list is not found, it will be added to the lppProblems array.
 * @param lppProblems An array of proptags that could not be removed from the object. Returns NULL if everything requested was removed. Can be NULL not to want the problem array.
 * 
 * @remark Property types are ignored; only the property identifiers are used.
 *
 * @return MAPI error code
 * @retval hrSuccess 0 or more properties are set to be removed on SaveChanges
 * @retval MAPI_E_NO_ACCESS the object is read-only
 * @retval MAPI_E_NOT_ENOUGH_MEMORY unable to allocate memory to remove the problem array
 */
HRESULT ECGenericProp::DeleteProps(LPSPropTagArray lpPropTagArray, LPSPropProblemArray FAR * lppProblems)
{
	ECRESULT				er = erSuccess;
	HRESULT					hr = hrSuccess;
	HRESULT					hrT = hrSuccess;
	ECPropCallBackIterator	iterCallBack;
	LPSPropProblemArray		lpProblems = NULL;
	int						nProblem = 0;

	// over-allocate the problem array
	er = ECAllocateBuffer(CbNewSPropProblemArray(lpPropTagArray->cValues), (LPVOID *)&lpProblems);
	if (er != erSuccess) {
		hr = MAPI_E_NOT_ENOUGH_MEMORY;
		goto exit;
	}

	for(unsigned int i=0;i<lpPropTagArray->cValues;i++) {

		// See if it's computed
		iterCallBack = lstCallBack.find(PROP_ID(lpPropTagArray->aulPropTag[i]));

		// Ignore removable callbacks
		if(iterCallBack != lstCallBack.end() && !iterCallBack->second.fRemovable) {
			// This is a computed value
			lpProblems->aProblem[nProblem].scode = MAPI_E_COMPUTED;
			lpProblems->aProblem[nProblem].ulIndex = i;
			lpProblems->aProblem[nProblem].ulPropTag = lpPropTagArray->aulPropTag[i];
			nProblem++;
		} else {

			hrT = HrDeleteRealProp(lpPropTagArray->aulPropTag[i],FALSE);

			if(hrT != hrSuccess) {
				// Add the error
				lpProblems->aProblem[nProblem].scode = hrT;
				lpProblems->aProblem[nProblem].ulIndex = i;
				lpProblems->aProblem[nProblem].ulPropTag = lpPropTagArray->aulPropTag[i];
				nProblem++;
			}
		}
	}

	lpProblems->cProblem = nProblem;

	if(lppProblems && nProblem)
		*lppProblems = lpProblems;
	else if(lppProblems) {
		*lppProblems = NULL;
		ECFreeBuffer(lpProblems);
	} else {
		ECFreeBuffer(lpProblems);
	}
	
exit:
	return hr;
}

HRESULT ECGenericProp::CopyTo(ULONG ciidExclude, LPCIID rgiidExclude, LPSPropTagArray lpExcludeProps, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, LPCIID lpInterface, LPVOID lpDestObj, ULONG ulFlags, LPSPropProblemArray FAR * lppProblems)
{
	return MAPI_E_NO_SUPPORT;
}

HRESULT ECGenericProp::CopyProps(LPSPropTagArray lpIncludeProps, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, LPCIID lpInterface, LPVOID lpDestObj, ULONG ulFlags, LPSPropProblemArray FAR * lppProblems)
{
	return MAPI_E_NO_SUPPORT;
}

HRESULT ECGenericProp::GetNamesFromIDs(LPSPropTagArray FAR * lppPropTags, LPGUID lpPropSetGuid, ULONG ulFlags, ULONG FAR * lpcPropNames, LPMAPINAMEID FAR * FAR * lpppPropNames)
{
	return MAPI_E_NO_SUPPORT;
}
 
HRESULT ECGenericProp::GetIDsFromNames(ULONG cPropNames, LPMAPINAMEID FAR * lppPropNames, ULONG ulFlags, LPSPropTagArray FAR * lppPropTags)
{
	return MAPI_E_NO_SUPPORT;
}

////////////////////////////////////////////
// Interface IECSingleInstance
//

HRESULT ECGenericProp::GetSingleInstanceId(ULONG *lpcbInstanceID, LPSIEID *lppInstanceID)
{
	HRESULT hr = hrSuccess;

	pthread_mutex_lock(&m_hMutexMAPIObject);

	if (!m_sMapiObject) {
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}

	if (!lpcbInstanceID || !lppInstanceID) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	hr = Util::HrCopyEntryId(m_sMapiObject->cbInstanceID, (LPENTRYID)m_sMapiObject->lpInstanceID,
							 lpcbInstanceID, (LPENTRYID *)lppInstanceID);
	if (hr != hrSuccess)
		goto exit;

exit:

	pthread_mutex_unlock(&m_hMutexMAPIObject);

	return hr;
}

HRESULT ECGenericProp::SetSingleInstanceId(ULONG cbInstanceID, LPSIEID lpInstanceID)
{
	HRESULT hr = hrSuccess;

	pthread_mutex_lock(&m_hMutexMAPIObject);

	if (!m_sMapiObject) {
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}

	if (m_sMapiObject->lpInstanceID)
		ECFreeBuffer(m_sMapiObject->lpInstanceID);

	m_sMapiObject->lpInstanceID = NULL;
	m_sMapiObject->cbInstanceID = 0;
	m_sMapiObject->bChangedInstance = false;

	hr = Util::HrCopyEntryId(cbInstanceID, (LPENTRYID)lpInstanceID,
							 &m_sMapiObject->cbInstanceID, (LPENTRYID *)&m_sMapiObject->lpInstanceID);
	if (hr != hrSuccess)
		goto exit;

	m_sMapiObject->bChangedInstance = true;

exit:
	pthread_mutex_unlock(&m_hMutexMAPIObject);

	return hr;
}

////////////////////////////////////////////
// Interface IMAPIProp
//

HRESULT __stdcall ECGenericProp::xMAPIProp::QueryInterface(REFIID refiid, void ** lppInterface)
{
	METHOD_PROLOGUE_(ECGenericProp , MAPIProp);
	return pThis->QueryInterface(refiid, lppInterface);
}

ULONG __stdcall ECGenericProp::xMAPIProp::AddRef()
{
	METHOD_PROLOGUE_(ECGenericProp , MAPIProp);
	return pThis->AddRef();
}

ULONG __stdcall ECGenericProp::xMAPIProp::Release()
{
	METHOD_PROLOGUE_(ECGenericProp , MAPIProp);
	return pThis->Release();
}

HRESULT __stdcall ECGenericProp::xMAPIProp::GetLastError(HRESULT hError, ULONG ulFlags,
    LPMAPIERROR * lppMapiError)
{
	METHOD_PROLOGUE_(ECGenericProp , MAPIProp);
	return pThis->GetLastError(hError, ulFlags, lppMapiError);
}

HRESULT __stdcall ECGenericProp::xMAPIProp::SaveChanges(ULONG ulFlags)
{
	METHOD_PROLOGUE_(ECGenericProp , MAPIProp);
	return pThis->SaveChanges(ulFlags);
}

HRESULT __stdcall ECGenericProp::xMAPIProp::GetProps(LPSPropTagArray lpPropTagArray, ULONG ulFlags, ULONG FAR * lpcValues, LPSPropValue FAR * lppPropArray)
{
	METHOD_PROLOGUE_(ECGenericProp , MAPIProp);
	return pThis->GetProps(lpPropTagArray, ulFlags, lpcValues, lppPropArray);
}

HRESULT __stdcall ECGenericProp::xMAPIProp::GetPropList(ULONG ulFlags, LPSPropTagArray FAR * lppPropTagArray)
{
	METHOD_PROLOGUE_(ECGenericProp , MAPIProp);
	return pThis->GetPropList(ulFlags, lppPropTagArray);
}

HRESULT __stdcall ECGenericProp::xMAPIProp::OpenProperty(ULONG ulPropTag, LPCIID lpiid, ULONG ulInterfaceOptions, ULONG ulFlags, LPUNKNOWN FAR * lppUnk)
{
	METHOD_PROLOGUE_(ECGenericProp , MAPIProp);
	return pThis->OpenProperty(ulPropTag, lpiid, ulInterfaceOptions, ulFlags, lppUnk);
}

HRESULT __stdcall ECGenericProp::xMAPIProp::SetProps(ULONG cValues, LPSPropValue lpPropArray, LPSPropProblemArray FAR * lppProblems)
{
	METHOD_PROLOGUE_(ECGenericProp , MAPIProp);
	return pThis->SetProps(cValues, lpPropArray, lppProblems);
}

HRESULT __stdcall ECGenericProp::xMAPIProp::DeleteProps(LPSPropTagArray lpPropTagArray, LPSPropProblemArray FAR * lppProblems)
{
	METHOD_PROLOGUE_(ECGenericProp , MAPIProp);
	return pThis->DeleteProps(lpPropTagArray, lppProblems);
}

HRESULT __stdcall ECGenericProp::xMAPIProp::CopyTo(ULONG ciidExclude, LPCIID rgiidExclude, LPSPropTagArray lpExcludeProps, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, LPCIID lpInterface, LPVOID lpDestObj, ULONG ulFlags, LPSPropProblemArray FAR * lppProblems)
{
	METHOD_PROLOGUE_(ECGenericProp , MAPIProp);
	return pThis->CopyTo(ciidExclude, rgiidExclude, lpExcludeProps, ulUIParam, lpProgress, lpInterface, lpDestObj, ulFlags, lppProblems);
}

HRESULT __stdcall ECGenericProp::xMAPIProp::CopyProps(LPSPropTagArray lpIncludeProps, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, LPCIID lpInterface, LPVOID lpDestObj, ULONG ulFlags, LPSPropProblemArray FAR * lppProblems)
{
	METHOD_PROLOGUE_(ECGenericProp , MAPIProp);
	return pThis->CopyProps(lpIncludeProps, ulUIParam, lpProgress, lpInterface, lpDestObj, ulFlags, lppProblems);
}

HRESULT __stdcall ECGenericProp::xMAPIProp::GetNamesFromIDs(LPSPropTagArray * pptaga, LPGUID lpguid, ULONG ulFlags, ULONG * pcNames, LPMAPINAMEID ** pppNames)
{
	METHOD_PROLOGUE_(ECGenericProp , MAPIProp);
	return pThis->GetNamesFromIDs(pptaga, lpguid, ulFlags, pcNames, pppNames);
}

HRESULT __stdcall ECGenericProp::xMAPIProp::GetIDsFromNames(ULONG cNames, LPMAPINAMEID * ppNames, ULONG ulFlags, LPSPropTagArray * pptaga)
{
	METHOD_PROLOGUE_(ECGenericProp , MAPIProp);
	return pThis->GetIDsFromNames(cNames, ppNames, ulFlags, pptaga);
}

// Proxy routines for IECSingleInstance
HRESULT __stdcall ECGenericProp::xECSingleInstance::QueryInterface(REFIID refiid, void ** lppInterface)
{
	TRACE_MAPI(TRACE_ENTRY, "IECSingleInstance::QueryInterface", "%s", DBGGUIDToString(refiid).c_str());
	METHOD_PROLOGUE_(ECGenericProp , ECSingleInstance);
	HRESULT hr = pThis->QueryInterface(refiid, lppInterface);
	TRACE_MAPI(TRACE_RETURN, "IECSingleInstance::QueryInterface", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

ULONG __stdcall ECGenericProp::xECSingleInstance::AddRef()
{
	TRACE_MAPI(TRACE_ENTRY, "IECSingleInstance::AddRef", "");
	METHOD_PROLOGUE_(ECGenericProp , ECSingleInstance);
	return pThis->AddRef();
}

ULONG __stdcall ECGenericProp::xECSingleInstance::Release()
{
	TRACE_MAPI(TRACE_ENTRY, "IECSingleInstance::Release", "");
	METHOD_PROLOGUE_(ECGenericProp , ECSingleInstance);
	return pThis->Release();
}

HRESULT __stdcall ECGenericProp::xECSingleInstance::GetSingleInstanceId(ULONG *lpcbInstanceID, LPENTRYID *lppInstanceID)
{
	TRACE_MAPI(TRACE_ENTRY, "IECSingleInstance::GetSingleInstanceId", "");
	METHOD_PROLOGUE_(ECGenericProp , ECSingleInstance);
	HRESULT hr = pThis->GetSingleInstanceId(lpcbInstanceID, (LPSIEID *)lppInstanceID);
	TRACE_MAPI(TRACE_RETURN, "IECSingleInstance::GetSingleInstanceId", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT __stdcall ECGenericProp::xECSingleInstance::SetSingleInstanceId(ULONG cbInstanceID, LPENTRYID lpInstanceID)
{
	TRACE_MAPI(TRACE_ENTRY, "IECSingleInstance::SetSingleInstanceId", "");
	METHOD_PROLOGUE_(ECGenericProp , ECSingleInstance);
	HRESULT hr = pThis->SetSingleInstanceId(cbInstanceID, (LPSIEID)lpInstanceID);
	TRACE_MAPI(TRACE_RETURN, "IECSingleInstance::SetSingleInstanceId", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}
