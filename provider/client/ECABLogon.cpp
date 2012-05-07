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

// ECABLogon.cpp: implementation of the ECABLogon class.
//
//////////////////////////////////////////////////////////////////////

#include "platform.h"

#include <mapiutil.h>

#include "Zarafa.h"
#include "ECGuid.h"
#include "edkguid.h"
#include "ECABLogon.h"

#include "ECABContainer.h"
#include "ECMailUser.h"
#include "ECDistList.h"

#include "ECDebug.h"

#include "WSTransport.h"

#include "Util.h"
#include "Mem.h"
#include "stringutil.h"
#include "ZarafaUtil.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

ECABLogon::ECABLogon(LPMAPISUP lpMAPISup, WSTransport* lpTransport, ULONG ulProfileFlags, GUID *lpGUID) : ECUnknown("IABLogon")
{
	// The 'legacy' guid used normally (all AB entryIDs have this GUID)
	m_guid = MUIDECSAB;

	// The specific GUID for *this* addressbook provider, if available
	if (lpGUID) {
		m_ABPGuid = *lpGUID;
	} else {
		m_ABPGuid = GUID_NULL;
	}

	m_lpNotifyClient = NULL;

	m_lpTransport = lpTransport;
	if(m_lpTransport)
		m_lpTransport->AddRef();

	m_lpMAPISup = lpMAPISup;
	if(m_lpMAPISup)
		m_lpMAPISup->AddRef();

	if (! (ulProfileFlags & EC_PROFILE_FLAGS_NO_NOTIFICATIONS))
		ECNotifyClient::Create(MAPI_ADDRBOOK, this, ulProfileFlags, lpMAPISup, &m_lpNotifyClient);
}

ECABLogon::~ECABLogon()
{
	if(m_lpTransport)
		m_lpTransport->HrLogOff();

	// Disable all advises
	if(m_lpNotifyClient)
		m_lpNotifyClient->ReleaseAll();

	if(m_lpNotifyClient)
		m_lpNotifyClient->Release();

	if(m_lpMAPISup) {
		m_lpMAPISup->Release();
		m_lpMAPISup = NULL;
	}

	if(m_lpTransport)
		m_lpTransport->Release();
}

HRESULT ECABLogon::Create(LPMAPISUP lpMAPISup, WSTransport* lpTransport, ULONG ulProfileFlags, GUID *lpGuid, ECABLogon **lppECABLogon)
{
	HRESULT hr = hrSuccess;

	ECABLogon *lpABLogon = new ECABLogon(lpMAPISup, lpTransport, ulProfileFlags, lpGuid);

	hr = lpABLogon->QueryInterface(IID_ECABLogon, (void **)lppECABLogon);

	if(hr != hrSuccess)
		delete lpABLogon;

	return hr;
}

HRESULT ECABLogon::QueryInterface(REFIID refiid, void **lppInterface)
{
	REGISTER_INTERFACE(IID_ECABLogon, this);
	REGISTER_INTERFACE(IID_ECUnknown, this);

	REGISTER_INTERFACE(IID_IABLogon, &this->m_xABLogon);
	REGISTER_INTERFACE(IID_IUnknown, &this->m_xABLogon);

	REGISTER_INTERFACE(IID_ISelectUnicode, &this->m_xUnknown);

	return MAPI_E_INTERFACE_NOT_SUPPORTED;
}

HRESULT ECABLogon::GetLastError(HRESULT hResult, ULONG ulFlags, LPMAPIERROR *lppMAPIError)
{
	HRESULT hr = hrSuccess;

	hr = MAPI_E_CALL_FAILED;

	return hr;
}

HRESULT ECABLogon::Logoff(ULONG ulFlags)
{
	HRESULT hr = hrSuccess;

	//FIXME: Release all Other open objects ?
	//Releases all open objects, such as any subobjects or the status object. 
	//Releases the provider's support object.

	if(m_lpMAPISup)
	{
		m_lpMAPISup->Release();
		m_lpMAPISup = NULL;
	}

	return hr;
}

HRESULT ECABLogon::OpenEntry(ULONG cbEntryID, LPENTRYID lpEntryID, LPCIID lpInterface, ULONG ulFlags, ULONG *lpulObjType, LPUNKNOWN *lppUnk)
{
	HRESULT			hr = hrSuccess;
	ECABContainer*	lpABContainer = NULL;
	BOOL			fModifyObject = FALSE;
	ABEID			eidRoot =  ABEID(MAPI_ABCONT, MUIDECSAB, 0);
	PABEID			lpABeid = NULL;
	IECPropStorage*	lpPropStorage = NULL;
	bool			bRoot = false;
	ECMailUser*		lpMailUser = NULL;
	ECDistList*		lpDistList = NULL;

	// Check input/output variables 
	if(lpulObjType == NULL || lppUnk == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	/*if(ulFlags & MAPI_MODIFY) {
		if(!fModify) {
			hr = MAPI_E_NO_ACCESS;
			goto exit;
		} else
			fModifyObject = TRUE;
	}

	if(ulFlags & MAPI_BEST_ACCESS)
		fModifyObject = fModify;
	*/

	if(cbEntryID == 0 && lpEntryID == NULL) {
		bRoot = true;
		lpABeid = &eidRoot;

		cbEntryID = CbABEID(lpABeid);
		lpEntryID = (LPENTRYID)lpABeid;
	} else {
		if (cbEntryID == 0 || lpEntryID == NULL) {
			hr = MAPI_E_UNKNOWN_ENTRYID;
			goto exit;
		}

		lpABeid = (PABEID)lpEntryID;

		// Check sane entryid
		if (lpABeid->ulType != MAPI_ABCONT && lpABeid->ulType != MAPI_MAILUSER && lpABeid->ulType != MAPI_DISTLIST) 
		{
			hr = MAPI_E_UNKNOWN_ENTRYID;
			goto exit;
		}

		// Check entryid GUID, must be either MUIDECSAB or m_ABPGuid
		if(memcmp(&lpABeid->guid, &MUIDECSAB, sizeof(MAPIUID)) != 0 && memcmp(&lpABeid->guid, &m_ABPGuid, sizeof(MAPIUID)) != 0) {
			hr = MAPI_E_UNKNOWN_ENTRYID;
			goto exit;
		}
	}
	//TODO: check entryid serverside?

	switch(lpABeid->ulType) {
		case MAPI_ABCONT:
			hr = ECABContainer::Create(this, MAPI_ABCONT, fModifyObject, &lpABContainer);
			if(hr != hrSuccess)
				goto exit;

			hr = lpABContainer->SetEntryId(cbEntryID, lpEntryID);
			if(hr != hrSuccess)
				goto exit;

			AddChild(lpABContainer);

			hr = m_lpTransport->HrOpenABPropStorage(cbEntryID, lpEntryID, &lpPropStorage);
			if(hr != hrSuccess)
				goto exit;

			hr = lpABContainer->HrSetPropStorage(lpPropStorage, TRUE);
			if(hr != hrSuccess)
				goto exit;

			if(lpInterface)
				hr = lpABContainer->QueryInterface(*lpInterface,(void **)lppUnk);
			else
				hr = lpABContainer->QueryInterface(IID_IABContainer, (void **)lppUnk);
			if(hr != hrSuccess)
				goto exit;
			break;
		case MAPI_MAILUSER:
			hr = ECMailUser::Create(this, fModifyObject, &lpMailUser);
			if(hr != hrSuccess)
				goto exit;
			
			hr = lpMailUser->SetEntryId(cbEntryID, lpEntryID);
			if(hr != hrSuccess)
				goto exit;

			AddChild(lpMailUser);

			hr = m_lpTransport->HrOpenABPropStorage(cbEntryID, lpEntryID, &lpPropStorage);
			if(hr != hrSuccess)
				goto exit;

			hr = lpMailUser->HrSetPropStorage(lpPropStorage, TRUE);
			if(hr != hrSuccess)
				goto exit;

			if(lpInterface)
				hr = lpMailUser->QueryInterface(*lpInterface,(void **)lppUnk);
			else
				hr = lpMailUser->QueryInterface(IID_IMailUser, (void **)lppUnk);

			if(hr != hrSuccess)
				goto exit;

			break;
		case MAPI_DISTLIST:
			hr = ECDistList::Create(this, fModifyObject, &lpDistList);
			if(hr != hrSuccess)
				goto exit;
			
			hr = lpDistList->SetEntryId(cbEntryID, lpEntryID);
			if(hr != hrSuccess)
				goto exit;

			AddChild(lpDistList);

			hr = m_lpTransport->HrOpenABPropStorage(cbEntryID, lpEntryID, &lpPropStorage);
			if(hr != hrSuccess)
				goto exit;

			hr = lpDistList->HrSetPropStorage(lpPropStorage, TRUE);
			if(hr != hrSuccess)
				goto exit;

			if(lpInterface)
				hr = lpDistList->QueryInterface(*lpInterface,(void **)lppUnk);
			else
				hr = lpDistList->QueryInterface(IID_IDistList, (void **)lppUnk);

			if(hr != hrSuccess)
				goto exit;

			break;
		default:
			hr = MAPI_E_NOT_FOUND;
			goto exit;
			break;
	}

	if(lpulObjType)
		*lpulObjType = lpABeid->ulType;

exit:
	if(lpABContainer)
		lpABContainer->Release();

	if(lpPropStorage)
		lpPropStorage->Release();

	if(lpMailUser)
		lpMailUser->Release();

	if(lpDistList)
		lpDistList->Release();

	return hr;
}

HRESULT ECABLogon::CompareEntryIDs(ULONG cbEntryID1, LPENTRYID lpEntryID1, ULONG cbEntryID2, LPENTRYID lpEntryID2, ULONG ulFlags, ULONG *lpulResult)
{
	if(lpulResult)
		*lpulResult = (CompareABEID(cbEntryID1, lpEntryID1, cbEntryID2, lpEntryID2) ? TRUE : FALSE);

	return hrSuccess;
}

HRESULT ECABLogon::Advise(ULONG cbEntryID, LPENTRYID lpEntryID, ULONG ulEventMask, LPMAPIADVISESINK lpAdviseSink, ULONG *lpulConnection)
{
	HRESULT hr = hrSuccess;

	if(lpAdviseSink == NULL || lpulConnection == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	if(lpEntryID == NULL) {
		//NOTE: Normal you must give the entryid of the addressbook toplevel
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	ASSERT(m_lpNotifyClient != NULL && (lpEntryID != NULL || TRUE));

	if(m_lpNotifyClient->Advise(cbEntryID, (LPBYTE)lpEntryID, ulEventMask, lpAdviseSink, lpulConnection) != S_OK)
		hr = MAPI_E_NO_SUPPORT;

exit:
	return hr;
}

HRESULT ECABLogon::Unadvise(ULONG ulConnection)
{
	HRESULT hr = hrSuccess;

	ASSERT(m_lpNotifyClient != NULL);

	m_lpNotifyClient->Unadvise(ulConnection);

	return hr;
}

HRESULT ECABLogon::OpenStatusEntry(LPCIID lpInterface, ULONG ulFlags, ULONG *lpulObjType, LPMAPISTATUS * lppMAPIStatus)
{
	HRESULT hr = hrSuccess;

	hr = MAPI_E_NO_SUPPORT;

	return hr;
}

HRESULT ECABLogon::OpenTemplateID(ULONG cbTemplateID, LPENTRYID lpTemplateID, ULONG ulTemplateFlags, LPMAPIPROP lpMAPIPropData, LPCIID lpInterface, LPMAPIPROP * lppMAPIPropNew, LPMAPIPROP lpMAPIPropSibling)
{
	HRESULT hr = hrSuccess;

	hr = MAPI_E_NO_SUPPORT;

	return hr;
}

HRESULT ECABLogon::GetOneOffTable(ULONG ulFlags, LPMAPITABLE * lppTable)
{
	HRESULT hr = hrSuccess;

	hr = MAPI_E_NO_SUPPORT;
	//hr = m_lpMAPISup->GetOneOffTable(ulFlags, lppTable);

	return hr;
}

HRESULT ECABLogon::PrepareRecips(ULONG ulFlags, LPSPropTagArray lpPropTagArray, LPADRLIST lpRecipList)
{
	HRESULT			hr = hrSuccess;
	ULONG			cPropsRecip;
	LPSPropValue	rgpropvalsRecip;
	LPSPropValue	lpPropVal = NULL;
	PABEID			lpABeid   = NULL;
	ULONG			cbABeid;
	ULONG			cValues;
	IMailUser*		lpIMailUser = NULL;
	LPSPropValue	lpPropArray = NULL;
	LPSPropValue	lpNewPropArray = NULL;
	unsigned int	j;
	ULONG			ulObjType;

	if(lpPropTagArray == NULL || lpPropTagArray->cValues == 0) // There is no work to do.
		goto exit;

	for(unsigned int i=0; i < lpRecipList->cEntries; i++)
	{
		rgpropvalsRecip	= lpRecipList->aEntries[i].rgPropVals;
		cPropsRecip		= lpRecipList->aEntries[i].cValues;

		// For each recipient, find its entryid
		lpPropVal = PpropFindProp( rgpropvalsRecip, cPropsRecip, PR_ENTRYID );
		if(!lpPropVal)
			continue; // no
		
		lpABeid = (PABEID) lpPropVal->Value.bin.lpb;
		cbABeid = lpPropVal->Value.bin.cb;

		/* Is it one of ours? */
		if ( cbABeid  < CbNewABEID("") || lpABeid == NULL)
			continue;	// no

		if ( memcmp( &(lpABeid->guid), &this->m_guid, sizeof(MAPIUID) ) != 0)
			continue;	// no

		hr = OpenEntry(cbABeid, (LPENTRYID)lpABeid, NULL, 0, &ulObjType, (LPUNKNOWN*)&lpIMailUser);
		if(hr != hrSuccess)
			continue;	// no
		
		hr = lpIMailUser->GetProps(lpPropTagArray, 0, &cValues, &lpPropArray);
		if(FAILED(hr) != hrSuccess)
			goto skip;	// no

		// merge the properties
		ECAllocateBuffer((cValues+cPropsRecip)*sizeof(SPropValue), (void**)&lpNewPropArray);

		for(j=0; j < cValues; j++) {
			lpPropVal = NULL;

			if(PROP_TYPE(lpPropArray[j].ulPropTag) == PT_ERROR)
				lpPropVal = PpropFindProp( rgpropvalsRecip, cPropsRecip, lpPropTagArray->aulPropTag[j]);

			if(lpPropVal == NULL)
				lpPropVal = &lpPropArray[j];


			hr = Util::HrCopyProperty(lpNewPropArray + j, lpPropVal, lpNewPropArray);
			if(hr != hrSuccess)
				goto exit;
		}

		for(j=0; j < cPropsRecip; j++)
		{
			if ( PpropFindProp(lpNewPropArray, cValues, rgpropvalsRecip[j].ulPropTag ) ||
				PROP_TYPE( rgpropvalsRecip[j].ulPropTag ) == PT_ERROR )
				continue;
			
			hr = Util::HrCopyProperty(lpNewPropArray + cValues, &rgpropvalsRecip[j], lpNewPropArray);
			if(hr != hrSuccess)
				goto exit;			
				
				
			cValues++;
		}

		lpRecipList->aEntries[i].rgPropVals	= lpNewPropArray;
		lpRecipList->aEntries[i].cValues	= cValues;

		if(rgpropvalsRecip) {
			ECFreeBuffer(rgpropvalsRecip); 
			rgpropvalsRecip = NULL;
		}
		
		lpNewPropArray = NULL; // Everthing oke, should not be freed..

	skip:
		if(lpPropArray){ ECFreeBuffer(lpPropArray); lpPropArray = NULL; }
		if(lpIMailUser){ lpIMailUser->Release(); lpIMailUser = NULL; }
	}

	// Always succeeded on this point
	hr = hrSuccess;

exit:
	if(lpPropArray)
		ECFreeBuffer(lpPropArray);

	if(lpNewPropArray)
		ECFreeBuffer(lpNewPropArray);
	
	if(lpIMailUser)
		lpIMailUser->Release();

	return hr;
}


HRESULT ECABLogon::xABLogon::QueryInterface(REFIID refiid, void ** lppInterface)
{
	TRACE_MAPI(TRACE_ENTRY, "IABLogon::QueryInterface", "%s", DBGGUIDToString(refiid).c_str());
	METHOD_PROLOGUE_(ECABLogon , ABLogon);
	HRESULT hr = pThis->QueryInterface(refiid, lppInterface);
	TRACE_MAPI(TRACE_RETURN, "IABLogon::QueryInterface", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

ULONG ECABLogon::xABLogon::AddRef()
{
	TRACE_MAPI(TRACE_ENTRY, "IABLogon::AddRef", "");
	METHOD_PROLOGUE_(ECABLogon , ABLogon);
	return pThis->AddRef();
}

ULONG ECABLogon::xABLogon::Release()
{
	TRACE_MAPI(TRACE_ENTRY, "IABLogon::Release", "");
	METHOD_PROLOGUE_(ECABLogon , ABLogon);
	ULONG ulRef = pThis->Release();
	TRACE_MAPI(TRACE_RETURN, "IABLogon::Release", "%d", ulRef);
	return ulRef;
}

HRESULT ECABLogon::xABLogon::GetLastError(HRESULT hResult, ULONG ulFlags, LPMAPIERROR *lppMAPIError)
{
	TRACE_MAPI(TRACE_ENTRY, "IABLogon::GetLastError", "");
	METHOD_PROLOGUE_(ECABLogon , ABLogon);
	HRESULT hr = pThis->GetLastError(hResult, ulFlags, lppMAPIError);
	TRACE_MAPI(TRACE_RETURN, "IABLogon::GetLastError", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECABLogon::xABLogon::Logoff(ULONG ulFlags)
{
	TRACE_MAPI(TRACE_ENTRY, "IABLogon::Logoff", "");
	METHOD_PROLOGUE_(ECABLogon, ABLogon);
	HRESULT hr = pThis->Logoff(ulFlags);
	TRACE_MAPI(TRACE_RETURN, "IABLogon::Logoff", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECABLogon::xABLogon::OpenEntry(ULONG cbEntryID, LPENTRYID lpEntryID, LPCIID lpInterface, ULONG ulFlags, ULONG *lpulObjType, LPUNKNOWN *lppUnk) 
{
	TRACE_MAPI(TRACE_ENTRY, "IABLogon::OpenEntry", "cbEntryID=%d, type=%d, id=%d, interface=%s, flags=0x%08X", cbEntryID, ABEID_TYPE(lpEntryID), ABEID_ID(lpEntryID), (lpInterface)?DBGGUIDToString(*lpInterface).c_str():"", ulFlags);
	METHOD_PROLOGUE_(ECABLogon, ABLogon);
	HRESULT hr = pThis->OpenEntry(cbEntryID, lpEntryID, lpInterface, ulFlags, lpulObjType, lppUnk);
	TRACE_MAPI(TRACE_RETURN, "IABLogon::OpenEntry", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECABLogon::xABLogon::CompareEntryIDs(ULONG cbEntryID1, LPENTRYID lpEntryID1, ULONG cbEntryID2, LPENTRYID lpEntryID2, ULONG ulFlags, ULONG *lpulResult) 
{
	TRACE_MAPI(TRACE_ENTRY, "IABLogon::CompareEntryIDs", "flags: %d\nentryid1: %s\nentryid2: %s", ulFlags, bin2hex(cbEntryID1, (BYTE*)lpEntryID1).c_str(), bin2hex(cbEntryID2, (BYTE*)lpEntryID2).c_str());
	METHOD_PROLOGUE_(ECABLogon, ABLogon);
	HRESULT hr = pThis->CompareEntryIDs(cbEntryID1, lpEntryID1, cbEntryID2, lpEntryID2, ulFlags, lpulResult);
	TRACE_MAPI(TRACE_RETURN, "IABLogon::CompareEntryIDs", "%s, result=%s", GetMAPIErrorDescription(hr).c_str(), (lpulResult)?((*lpulResult == TRUE)?"TRUE":"FALSE") : "NULL");
	return hr;
}

HRESULT ECABLogon::xABLogon::Advise(ULONG cbEntryID, LPENTRYID lpEntryID, ULONG ulEventMask, LPMAPIADVISESINK lpAdviseSink, ULONG *lpulConnection) 
{
	TRACE_MAPI(TRACE_ENTRY, "IABLogon::Advise", "");
	METHOD_PROLOGUE_(ECABLogon, ABLogon);
	HRESULT hr = pThis->Advise(cbEntryID, lpEntryID, ulEventMask, lpAdviseSink, lpulConnection);
	TRACE_MAPI(TRACE_RETURN, "IABLogon::Advise", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECABLogon::xABLogon::Unadvise(ULONG ulConnection)
{
	TRACE_MAPI(TRACE_ENTRY, "IABLogon::Unadvise", "");
	METHOD_PROLOGUE_(ECABLogon, ABLogon);
	HRESULT hr = pThis->Unadvise(ulConnection);
	TRACE_MAPI(TRACE_RETURN, "IABLogon::Unadvise", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECABLogon::xABLogon::OpenStatusEntry(LPCIID lpInterface, ULONG ulFlags, ULONG *lpulObjType, LPMAPISTATUS * lppMAPIStatus)
{
	TRACE_MAPI(TRACE_ENTRY, "IABLogon::OpenStatusEntry", "");
	METHOD_PROLOGUE_(ECABLogon, ABLogon);
	HRESULT hr = pThis->OpenStatusEntry(lpInterface, ulFlags, lpulObjType, lppMAPIStatus);
	TRACE_MAPI(TRACE_RETURN, "IABLogon::OpenStatusEntry", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECABLogon::xABLogon::OpenTemplateID(ULONG cbTemplateID, LPENTRYID lpTemplateID, ULONG ulTemplateFlags, LPMAPIPROP lpMAPIPropData, LPCIID lpInterface, LPMAPIPROP * lppMAPIPropNew, LPMAPIPROP lpMAPIPropSibling)
{
	TRACE_MAPI(TRACE_ENTRY, "IABLogon::OpenTemplateID", "");
	METHOD_PROLOGUE_(ECABLogon, ABLogon);
	HRESULT hr = pThis->OpenTemplateID(cbTemplateID, lpTemplateID, ulTemplateFlags, lpMAPIPropData, lpInterface, lppMAPIPropNew, lpMAPIPropSibling);
	TRACE_MAPI(TRACE_RETURN, "IABLogon::OpenTemplateID", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECABLogon::xABLogon::GetOneOffTable(ULONG ulFlags, LPMAPITABLE * lppTable)
{
	TRACE_MAPI(TRACE_ENTRY, "IABLogon::GetOneOffTable", "flags=0x%08X", ulFlags);
	METHOD_PROLOGUE_(ECABLogon, ABLogon);
	HRESULT hr = pThis->GetOneOffTable(ulFlags, lppTable);
	TRACE_MAPI(TRACE_RETURN, "IABLogon::GetOneOffTable", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECABLogon::xABLogon::PrepareRecips(ULONG ulFlags, LPSPropTagArray lpPropTagArray, LPADRLIST lpRecipList)
{
	TRACE_MAPI(TRACE_ENTRY, "IABLogon::PrepareRecips", "flags=0x%08X, PropTagArray=%s\nin=%s", ulFlags, PropNameFromPropTagArray(lpPropTagArray).c_str(), RowSetToString((LPSRowSet)lpRecipList).c_str());
	METHOD_PROLOGUE_(ECABLogon, ABLogon);
	HRESULT hr = pThis->PrepareRecips(ulFlags, lpPropTagArray, lpRecipList);
	TRACE_MAPI(TRACE_RETURN, "IABLogon::PrepareRecips", "%s\nout=%s", GetMAPIErrorDescription(hr).c_str(), RowSetToString((LPSRowSet)lpRecipList).c_str() );
	return hr;
}
