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

#include "ECGetText.h"

#include <memory.h>
#include <mapi.h>
#include <mapiutil.h>
#include <mapispi.h>

#include "ECMAPISupport.h"
#include "ClientUtil.h"
#include "Mem.h"
#include "stringutil.h"

#include "ECGuid.h"

#include "ECABProvider.h"
#include "ECMSProvider.h"
#include "ECABProviderOffline.h"
#include "ECMSProviderOffline.h"
#include "ECMsgStore.h"
#include "ECArchiveAwareMsgStore.h"
#include "ECMsgStorePublic.h"
#include "charset/convstring.h"



#include "DLLGlobal.h"
#include "EntryPoint.h"
#include "ProviderUtil.h"

#include "charset/convert.h"

#include <boost/filesystem.hpp>
namespace bfs = boost::filesystem;

#ifdef UNICODE
typedef bfs::wpath path;
#else
typedef bfs::path path;
#endif

using namespace std;

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

// Wrap Support object
HRESULT GetWrappedSupportObject(IMAPISupport* lpMAPISup, LPPROFSECT lpProfileObj, IMAPISupport** lppWrappedMAPISupport)
{
	HRESULT			hr = hrSuccess;
	
	ECMAPISupport*	lpECMAPISupport = NULL;

	if(lpMAPISup == NULL || lppWrappedMAPISupport == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	hr = ECMAPISupport::Create(lpMAPISup, lpProfileObj, &lpECMAPISupport);
	if(hr != hrSuccess)
		goto exit;

	hr = lpECMAPISupport->QueryInterface(IID_IMAPISup, (void **)lppWrappedMAPISupport);
	if(hr != hrSuccess)
		goto exit;

exit:
	if(lpECMAPISupport)
		lpECMAPISupport->Release();

	return hr;
}

HRESULT CompareStoreIDs(ULONG cbEntryID1, LPENTRYID lpEntryID1, ULONG cbEntryID2, LPENTRYID lpEntryID2, ULONG ulFlags, ULONG *lpulResult)
{
	HRESULT hr = hrSuccess;
	BOOL fTheSame = FALSE;
	PEID peid1 = (PEID)lpEntryID1;
	PEID peid2 = (PEID)lpEntryID2;

	if(lpEntryID1 == NULL || lpEntryID2 == NULL || lpulResult == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	if (cbEntryID1 < (sizeof(GUID) + 4 + 4) || cbEntryID2 < (sizeof(GUID) + 4 + 4)) {
		hr = MAPI_E_INVALID_ENTRYID;
		goto exit;
	}

	if(memcmp(&peid1->guid, &peid2->guid, sizeof(GUID)) != 0)
		goto exit;

	if(peid1->ulVersion != peid2->ulVersion)
		goto exit;

	if(peid1->ulType != peid2->ulType)
		goto exit;

	if(peid1->ulVersion == 0) {

		if(cbEntryID1 < sizeof(EID_V0))
			goto exit;

		if( ((EID_V0*)lpEntryID1)->ulId != ((EID_V0*)lpEntryID2)->ulId )
			goto exit;

	}else {
		if(cbEntryID1 < CbNewEID(""))
			goto exit;

		if(peid1->uniqueId != peid2->uniqueId) //comp. with the old ulId
			goto exit;
	}

	fTheSame = TRUE;

exit:
	if(lpulResult)
		*lpulResult = fTheSame;

	return hr;
}



// Get MAPI unique guid, guaranteed by MAPI to be unique for all profiles.
HRESULT GetMAPIUniqueProfileId(LPMAPISUP lpMAPISup, tstring* lpstrUniqueId)
{
	HRESULT			hr = hrSuccess;
	LPPROFSECT		lpProfSect = NULL;
	LPSPropValue	lpsPropValue = NULL;

	hr = lpMAPISup->OpenProfileSection((LPMAPIUID)&MUID_PROFILE_INSTANCE, 0, &lpProfSect);
	if(hr != hrSuccess)
		goto exit;

	hr = HrGetOneProp(lpProfSect, PR_SEARCH_KEY, &lpsPropValue);
	if(hr != hrSuccess)
		goto exit;

	*lpstrUniqueId = bin2hext(lpsPropValue->Value.bin.cb, lpsPropValue->Value.bin.lpb);
exit:

	if(lpsPropValue)
		MAPIFreeBuffer(lpsPropValue);

	if(lpProfSect)
		lpProfSect->Release();

	return hr;
}

HRESULT RemoveAllProviders(ECMapProvider* lpmapProvider)
{
	HRESULT hr = hrSuccess;
	ECMapProvider::iterator iterProvider;
	
	if (lpmapProvider == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	for (iterProvider = lpmapProvider->begin(); iterProvider != lpmapProvider->end(); iterProvider++)
	{
		if (iterProvider->second.lpMSProviderOnline)
			iterProvider->second.lpMSProviderOnline->Release();

		if (iterProvider->second.lpABProviderOnline)
			iterProvider->second.lpABProviderOnline->Release();

	}

exit:
	return hr;
}

HRESULT SetProviderMode(IMAPISupport *lpMAPISup, ECMapProvider* lpmapProvider, LPCSTR lpszProfileName, ULONG ulConnectType)
{
	HRESULT hr = hrSuccess;

	return hr;
}

HRESULT GetLastConnectionType(IMAPISupport *lpMAPISup, ULONG *lpulType) {
	HRESULT hr = hrSuccess;
	*lpulType = CT_ONLINE;

	return hr;
}

HRESULT GetProviders(ECMapProvider* lpmapProvider, IMAPISupport *lpMAPISup, const char *lpszProfileName, ULONG ulFlags, PROVIDER_INFO* lpsProviderInfo)
{
	HRESULT hr = hrSuccess;
	ECMapProvider::iterator iterProvider;
	PROVIDER_INFO sProviderInfo;
	ECMSProvider *lpECMSProvider = NULL;
	ECABProvider *lpECABProvider = NULL;
	sGlobalProfileProps	sProfileProps;

	if (lpmapProvider == NULL || lpMAPISup == NULL || lpszProfileName == NULL || lpsProviderInfo == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	iterProvider = lpmapProvider->find(lpszProfileName);
	if (iterProvider != lpmapProvider->end())
	{
		*lpsProviderInfo = iterProvider->second;
		goto exit;
	}
		
	// Get the username and password from the profile settings
	hr = ClientUtil::GetGlobalProfileProperties(lpMAPISup, &sProfileProps);
	if(hr != hrSuccess)
		goto exit;

	//////////////////////////////////////////////////////
	// Init providers

	// Message store online
	hr = ECMSProvider::Create(ulFlags, &lpECMSProvider);
	if(hr != hrSuccess)
		goto exit;

	// Addressbook online
	hr = ECABProvider::Create(&lpECABProvider);
	if(hr != hrSuccess)
		goto exit;


	//////////////////////////////////////////////////////
	// Fill in the Provider info struct
	
	//Init only the firsttime the flags
	sProviderInfo.ulProfileFlags = sProfileProps.ulProfileFlags;

	sProviderInfo.ulConnectType = CT_ONLINE;

	hr = lpECMSProvider->QueryInterface(IID_IMSProvider, (void **)&sProviderInfo.lpMSProviderOnline);
	if(hr != hrSuccess)
		goto exit;

	hr = lpECABProvider->QueryInterface(IID_IABProvider, (void **)&sProviderInfo.lpABProviderOnline);
	if(hr != hrSuccess)
		goto exit;

	

	//Add provider in map
	lpmapProvider->insert(std::map<string, PROVIDER_INFO>::value_type(lpszProfileName, sProviderInfo));

	*lpsProviderInfo = sProviderInfo;

exit:
	if (lpECMSProvider)
		lpECMSProvider->Release();

	if (lpECABProvider)
		lpECABProvider->Release();


	return hr;
}

// Create an anonymous message store, linked to transport and support object
//
// NOTE
//  Outlook will stay 'alive' when the user shuts down until the support
//  object is released, so we have to make sure that when the users has released
//  all the msgstore objects, we also release the support object.
//
HRESULT CreateMsgStoreObject(char * lpszProfname, LPMAPISUP lpMAPISup, ULONG cbEntryID, LPENTRYID lpEntryID, ULONG ulMsgFlags, ULONG ulProfileFlags, WSTransport* lpTransport,
							MAPIUID* lpguidMDBProvider, BOOL bSpooler, BOOL fIsDefaultStore, BOOL bOfflineStore,
							ECMsgStore** lppECMsgStore)
{
	HRESULT	hr = hrSuccess;
	
	BOOL fModify = FALSE;

	ECMsgStore *lpMsgStore = NULL;
	IECPropStorage *lpStorage = NULL;
		

	fModify = ulMsgFlags & MDB_WRITE || ulMsgFlags & MAPI_BEST_ACCESS; // FIXME check access at server

	if (CompareMDBProvider(lpguidMDBProvider, &ZARAFA_STORE_PUBLIC_GUID) == TRUE)
		hr = ECMsgStorePublic::Create(lpszProfname, lpMAPISup, lpTransport, fModify, ulProfileFlags, bSpooler, bOfflineStore, &lpMsgStore);
	else if (CompareMDBProvider(lpguidMDBProvider, &ZARAFA_STORE_ARCHIVE_GUID) == TRUE)
		hr = ECMsgStore::Create(lpszProfname, lpMAPISup, lpTransport, fModify, ulProfileFlags, bSpooler, FALSE, bOfflineStore, &lpMsgStore);
	else
		hr = ECArchiveAwareMsgStore::Create(lpszProfname, lpMAPISup, lpTransport, fModify, ulProfileFlags, bSpooler, fIsDefaultStore, bOfflineStore, &lpMsgStore);

	if (hr != hrSuccess)
		goto exit;

	memcpy(&lpMsgStore->m_guidMDB_Provider, lpguidMDBProvider,sizeof(MAPIUID));

	// Get a propstorage for the message store
	hr = lpTransport->HrOpenPropStorage(0, NULL, cbEntryID, lpEntryID, 0, &lpStorage);
	if (hr != hrSuccess)
		goto exit;

	// Set up the message store to use this storage
	hr = lpMsgStore->HrSetPropStorage(lpStorage, FALSE);
	if (hr != hrSuccess)
		goto exit;

	// Setup callback for session change
	hr = lpTransport->AddSessionReloadCallback(lpMsgStore, ECMsgStore::Reload, NULL);
	if (hr != hrSuccess)
		goto exit;

	hr = lpMsgStore->SetEntryId(cbEntryID, lpEntryID);
	if (hr != hrSuccess)
		goto exit;

	hr = lpMsgStore->QueryInterface(IID_ECMsgStore, (void **)lppECMsgStore);
	if (hr != hrSuccess)
		goto exit;

exit:
	if (lpMsgStore)
		lpMsgStore->Release();

	if(lpStorage)
		lpStorage->Release();

	return hr;
}



HRESULT GetTransportToNamedServer(WSTransport *lpTransport, LPCTSTR lpszServerName, ULONG ulFlags, WSTransport **lppTransport)
{
	HRESULT hr = hrSuccess;
	utf8string strServerName;
	utf8string strPseudoUrl = utf8string::from_string("pseudo://");
	char *lpszServerPath = NULL;
	bool bIsPeer = false;
	WSTransport *lpNewTransport = NULL;

	if (lpszServerName == NULL || lpTransport == NULL || lppTransport == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	if ((ulFlags & ~MAPI_UNICODE) != 0) {
		hr = MAPI_E_UNKNOWN_FLAGS;
		goto exit;
	}

	strServerName = convstring(lpszServerName, ulFlags);
	strPseudoUrl.append(strServerName);
	hr = lpTransport->HrResolvePseudoUrl(strPseudoUrl.c_str(), &lpszServerPath, &bIsPeer);
	if (hr != hrSuccess)
		goto exit;

	if (bIsPeer) {
		lpNewTransport = lpTransport;
		lpNewTransport->AddRef();
	} else {
		hr = lpTransport->CreateAndLogonAlternate(lpszServerPath, &lpNewTransport);
		if (hr != hrSuccess)
			goto exit;
	}

	*lppTransport = lpNewTransport;

exit:
	return hr;
}
