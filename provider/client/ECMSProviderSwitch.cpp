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

#include "ECMSProviderSwitch.h"
#include "ECMSProvider.h"
#include "ECOfflineState.h"

#include "ECGuid.h"

#include "Trace.h"
#include "ECDebug.h"


#include "ECMAPISupport.h"

#include "edkguid.h"
#include "EntryPoint.h"
#include "DLLGlobal.h"
#include "edkmdb.h"
#include "mapiext.h"

#include "ClientUtil.h"
#include "ECMsgStore.h"
#include "stringutil.h"

#include <signal.h>

#include "ProviderUtil.h"

#include <charset/convstring.h>

#ifdef swprintf
	#undef swprintf
#endif

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

ECMSProviderSwitch::ECMSProviderSwitch(ULONG ulFlags) : ECUnknown("ECMSProviderSwitch")
{
	m_ulFlags = ulFlags;
}

ECMSProviderSwitch::~ECMSProviderSwitch(void)
{
}

HRESULT ECMSProviderSwitch::Create(ULONG ulFlags, ECMSProviderSwitch **lppMSProvider)
{
	ECMSProviderSwitch *lpMSProvider = new ECMSProviderSwitch(ulFlags);

	return lpMSProvider->QueryInterface(IID_ECUnknown/*IID_ECMSProviderSwitch*/, (void **)lppMSProvider);
}

HRESULT ECMSProviderSwitch::QueryInterface(REFIID refiid, void **lppInterface)
{
	/*refiid == IID_ECMSProviderSwitch */
	REGISTER_INTERFACE(IID_ECUnknown, this);

	REGISTER_INTERFACE(IID_IMSProvider, &this->m_xMSProvider);
	REGISTER_INTERFACE(IID_IUnknown, &this->m_xMSProvider);

	REGISTER_INTERFACE(IID_ISelectUnicode, &this->m_xUnknown);

	return MAPI_E_INTERFACE_NOT_SUPPORTED;
}

ULONG ECMSProviderSwitch::Release()
{
	return ECUnknown::Release();
}

HRESULT ECMSProviderSwitch::Shutdown(ULONG * lpulFlags) 
{
	HRESULT hr = hrSuccess;

	//FIXME
	return hr;
}

HRESULT ECMSProviderSwitch::Logon(LPMAPISUP lpMAPISup, ULONG ulUIParam, LPTSTR lpszProfileName, ULONG cbEntryID, LPENTRYID lpEntryID, ULONG ulFlags, LPCIID lpInterface, ULONG *lpcbSpoolSecurity, LPBYTE *lppbSpoolSecurity, LPMAPIERROR *lppMAPIError, LPMSLOGON *lppMSLogon, LPMDB *lppMDB)
{
	HRESULT			hr = hrSuccess;
	IMAPISupport*	lpMAPISupport = NULL;
	ECMsgStore*		lpecMDB = NULL;

	sGlobalProfileProps	sProfileProps;
	LPPROFSECT		lpProfSect = NULL;
	LPSPropValue	lpsPropArray = NULL;
	LPSPropTagArray	lpsPropTagArray = NULL;
	ULONG			cValues = 0;

	char*			lpDisplayName = NULL;
	LPSPropValue	lpProp = NULL;
	bool			bIsDefaultStore = false;
	LPSPropValue	lpIdentityProps = NULL;

	LPMDB			lpMDB = NULL;
	LPMSLOGON		lpMSLogon = NULL;

	PROVIDER_INFO sProviderInfo;
	ULONG ulConnectType = CT_UNSPECIFIED;
	IMSProvider *lpOnline = NULL;
	convert_context converter;
	LPENTRYID		lpStoreID = NULL;
	ULONG			cbStoreID = 0;

	convstring			tstrProfileName(lpszProfileName, ulFlags);



	// Wrap Support object
	hr = GetWrappedSupportObject(lpMAPISup, NULL, &lpMAPISupport);
	if (hr != hrSuccess)
		goto exit;

	// Get the username and password from the profile settings
	hr = ClientUtil::GetGlobalProfileProperties(lpMAPISup, &sProfileProps);
	if (hr != hrSuccess)
		goto exit;

	// Open profile settings
	hr = lpMAPISup->OpenProfileSection(NULL, MAPI_MODIFY, &lpProfSect);
	if (hr != hrSuccess)
		goto exit;

	if (lpEntryID == NULL) {

		// Try to initialize the provider
		if (InitializeProvider(NULL, lpProfSect, sProfileProps, &cbStoreID, &lpStoreID) != hrSuccess) {
			hr = MAPI_E_UNCONFIGURED;
			goto exit;
		}

		lpEntryID = lpStoreID;
		cbEntryID = cbStoreID;
	}

	cValues = 1;
	hr = MAPIAllocateBuffer(CbNewSPropTagArray(cValues), (void **)&lpsPropTagArray);
	if (hr != hrSuccess)
		goto exit;

	lpsPropTagArray->cValues = 1;
	lpsPropTagArray->aulPropTag[0] = PR_MDB_PROVIDER;
	
	hr = lpProfSect->GetProps(lpsPropTagArray, 0, &cValues, &lpsPropArray);
	if (hr == hrSuccess)
	{
		if (lpsPropArray[0].ulPropTag == PR_MDB_PROVIDER){
			if (CompareMDBProvider(lpsPropArray[0].Value.bin.lpb, &ZARAFA_SERVICE_GUID) ||
				CompareMDBProvider(lpsPropArray[0].Value.bin.lpb, &MSEMS_SERVICE_GUID)) {
				bIsDefaultStore = true;
			}
		}
	}
	hr = hrSuccess;

	hr = GetProviders(&g_mapProviders, lpMAPISup, tstrProfileName.c_str(), ulFlags, &sProviderInfo);
	if (hr != hrSuccess)
		goto exit;

	hr = sProviderInfo.lpMSProviderOnline->QueryInterface(IID_IMSProvider, (void **)&lpOnline);
	if (hr != hrSuccess)
		goto exit;


	// Default error
	hr = MAPI_E_LOGON_FAILED; //or MAPI_E_FAILONEPROVIDER

	// Connect online if any of the following is true:
	// - Online store was specifically requested (MDB_ONLINE)
	// - Profile is not offline capable
	// - Store being opened is not the user's default store

	if ((ulFlags & MDB_ONLINE) == MDB_ONLINE || (sProviderInfo.ulProfileFlags&EC_PROFILE_FLAGS_OFFLINE) != EC_PROFILE_FLAGS_OFFLINE || bIsDefaultStore == false)
	{
		bool fDone = false;

		while(!fDone) {
			hr = lpOnline->Logon(lpMAPISupport, ulUIParam, lpszProfileName, cbEntryID, lpEntryID, ulFlags, lpInterface, NULL, NULL, NULL, &lpMSLogon, &lpMDB);
			ulConnectType = CT_ONLINE;
			{
				fDone = true;
			}
		}
	}

	// Set the provider in the right connection type
	if (bIsDefaultStore) {
		if (SetProviderMode(lpMAPISup, &g_mapProviders, tstrProfileName.c_str(), ulConnectType) != hrSuccess) {
			hr = MAPI_E_INVALID_PARAMETER;
			goto exit;
		}
	}

	if(hr != hrSuccess) {
		if(ulFlags & MDB_NO_DIALOG) {
			hr = MAPI_E_FAILONEPROVIDER;
			goto exit;
		} else if(hr == MAPI_E_NETWORK_ERROR) {
			hr = MAPI_E_FAILONEPROVIDER; //for disable public folders, so you can work offline
			goto exit;
		} else if (hr == MAPI_E_LOGON_FAILED) {
			hr = MAPI_E_UNCONFIGURED; // Linux error ??//
			//hr = MAPI_E_LOGON_FAILED;
			goto exit;
		}else{
			hr = MAPI_E_LOGON_FAILED;
			goto exit;
		}
	}
	

	hr = lpMDB->QueryInterface(IID_ECMsgStore, (void **)&lpecMDB);
	if (hr != hrSuccess)
		goto exit;

	// Register ourselves with mapisupport
	hr = lpMAPISup->SetProviderUID((MAPIUID *)&lpecMDB->GetStoreGuid(), 0); 
	if (hr != hrSuccess)
		goto exit;

	// Set profile identity
	hr = ClientUtil::HrSetIdentity(lpecMDB->lpTransport, lpMAPISup, &lpIdentityProps);
	if (hr != hrSuccess)
		goto exit;

	// Get store name
	// The server will return MAPI_E_UNCONFIGURED when an attempt is made to open a store
	// that does not exist on that server. However the store is only opened the first time
	// when it's actually needed.
	// Since this is the first call that actually needs information from the store, we need
	// to be prepared to handle the MAPI_E_UNCONFIGURED error as we want to propagate this
	// up to the caller so this 'error' can be resolved by reconfiguring the profile.
	hr = HrGetOneProp(lpMDB, PR_DISPLAY_NAME_A, &lpProp);
	if (hr == MAPI_E_UNCONFIGURED)
		goto exit;
	if (hr != hrSuccess || lpProp->ulPropTag != PR_DISPLAY_NAME_A) {
		lpDisplayName = _A("Unknown");
		hr = hrSuccess;
	} else {
		lpDisplayName = lpProp->Value.lpszA;
	}

	if (CompareMDBProvider(&lpecMDB->m_guidMDB_Provider, &ZARAFA_SERVICE_GUID) ||
		CompareMDBProvider(&lpecMDB->m_guidMDB_Provider, &ZARAFA_STORE_DELEGATE_GUID))
	{
		hr = ClientUtil::HrInitializeStatusRow(lpDisplayName, MAPI_STORE_PROVIDER, lpMAPISup, lpIdentityProps, 0);
		if (hr != hrSuccess)
			goto exit;
	}

	if (lppMSLogon) {
		hr = lpMSLogon->QueryInterface(IID_IMSLogon, (void **)lppMSLogon);
		if (hr != hrSuccess)
			goto exit;
	}
	
	if (lppMDB) {
		hr = lpMDB->QueryInterface(IID_IMsgStore, (void **)lppMDB);
		if (hr != hrSuccess)
			goto exit;
	}

	// Store username and password so SpoolerLogon can log on to the profile
	if(lppbSpoolSecurity)
	{
		ULONG cbSpoolSecurity = sizeof(wchar_t) * (sProfileProps.strUserName.length() + sProfileProps.strPassword.length() + 1 + 1);

		hr = MAPIAllocateBuffer(cbSpoolSecurity, (void **)lppbSpoolSecurity);
		if(hr != hrSuccess)
			goto exit;

		swprintf((wchar_t*)*lppbSpoolSecurity, cbSpoolSecurity, L"%s%c%s", sProfileProps.strUserName.c_str(), 0, sProfileProps.strPassword.c_str());
		*lpcbSpoolSecurity = cbSpoolSecurity;
	}


exit:
	if (lppMAPIError)
		*lppMAPIError = NULL;

	if (lpsPropTagArray)
		MAPIFreeBuffer(lpsPropTagArray);

	if (lpsPropArray)
		MAPIFreeBuffer(lpsPropArray);

	if (lpProp)
		MAPIFreeBuffer(lpProp);

	if (lpProfSect)
		lpProfSect->Release();

	
	if (lpMSLogon)
		lpMSLogon->Release();
	
	if (lpMDB)
		lpMDB->Release();

	if (lpecMDB)
		lpecMDB->Release();
    
	if (lpOnline)
		lpOnline->Release();


    if (lpIdentityProps)
        MAPIFreeBuffer(lpIdentityProps);

	if (lpMAPISupport)
		lpMAPISupport->Release();

	if (lpStoreID)
		MAPIFreeBuffer(lpStoreID);

	return hr;
}

HRESULT ECMSProviderSwitch::SpoolerLogon(LPMAPISUP lpMAPISup, ULONG ulUIParam, LPTSTR lpszProfileName, ULONG cbEntryID, LPENTRYID lpEntryID, ULONG ulFlags, LPCIID lpInterface, ULONG cbSpoolSecurity, LPBYTE lpbSpoolSecurity, LPMAPIERROR *lppMAPIError, LPMSLOGON *lppMSLogon, LPMDB *lppMDB)
{
	HRESULT hr = hrSuccess;
	IMAPISupport *lpMAPISupport = NULL;
	IMSProvider *lpProvider = NULL; // Do not release
	PROVIDER_INFO sProviderInfo;
	LPMDB lpMDB = NULL;
	LPMSLOGON lpMSLogon = NULL;
	ECMsgStore *lpecMDB = NULL;

	if (lpEntryID == NULL) {
		hr = MAPI_E_UNCONFIGURED;
		goto exit;
	}
	
	if (cbSpoolSecurity == 0 || lpbSpoolSecurity == NULL) {
		hr = MAPI_E_NO_ACCESS;
		goto exit;
	}

	// Wrap Support object
	hr = GetWrappedSupportObject(lpMAPISup, NULL, &lpMAPISupport);
	if (hr != hrSuccess)
		goto exit;

	hr = GetProviders(&g_mapProviders, lpMAPISup, convstring(lpszProfileName, ulFlags).c_str(), ulFlags, &sProviderInfo);
	if (hr != hrSuccess)
		goto exit;

		lpProvider = sProviderInfo.lpMSProviderOnline;

	hr = lpProvider->SpoolerLogon(lpMAPISupport, ulUIParam, lpszProfileName, cbEntryID, lpEntryID, ulFlags, lpInterface, cbSpoolSecurity, lpbSpoolSecurity, NULL, &lpMSLogon, &lpMDB);
	if (hr != hrSuccess)
		goto exit;

	hr = lpMDB->QueryInterface(IID_ECMsgStore, (void **)&lpecMDB);
	if (hr != hrSuccess)
		goto exit;

	// Register ourselves with mapisupport
	hr = lpMAPISupport->SetProviderUID((MAPIUID *)&lpecMDB->GetStoreGuid(), 0); 
	if (hr != hrSuccess)
		goto exit;


	if (lppMSLogon) {
		hr = lpMSLogon->QueryInterface(IID_IMSLogon, (void **)lppMSLogon);
		if (hr != hrSuccess)
			goto exit;
	}
	
	if (lppMDB) {
		hr = lpMDB->QueryInterface(IID_IMsgStore, (void **)lppMDB);
		if (hr != hrSuccess)
			goto exit;
	}


exit:
	if (lppMAPIError)
		*lppMAPIError = NULL;

	if (lpecMDB)
		lpecMDB->Release();

	if (lpMSLogon)
		lpMSLogon->Release();
	
	if (lpMDB)
		lpMDB->Release();

	if (lpMAPISupport)
		lpMAPISupport->Release();

	return hr;
}
	
HRESULT ECMSProviderSwitch::CompareStoreIDs(ULONG cbEntryID1, LPENTRYID lpEntryID1, ULONG cbEntryID2, LPENTRYID lpEntryID2, ULONG ulFlags, ULONG *lpulResult)
{
	return ::CompareStoreIDs(cbEntryID1, lpEntryID1, cbEntryID2, lpEntryID2, ulFlags, lpulResult);
}

ULONG ECMSProviderSwitch::xMSProvider::AddRef() 
{
	TRACE_MAPI(TRACE_ENTRY, "ECMSProviderSwitch::AddRef", "");
	METHOD_PROLOGUE_(ECMSProviderSwitch, MSProvider);
	return pThis->AddRef();
}

ULONG ECMSProviderSwitch::xMSProvider::Release()
{
	TRACE_MAPI(TRACE_ENTRY, "ECMSProviderSwitch::Release", "");
	METHOD_PROLOGUE_(ECMSProviderSwitch, MSProvider);
	return pThis->Release();
}

HRESULT ECMSProviderSwitch::xMSProvider::QueryInterface(REFIID refiid, void **lppInterface)
{
	TRACE_MAPI(TRACE_ENTRY, "ECMSProviderSwitch::QueryInterface", "%s", DBGGUIDToString(refiid).c_str());
	METHOD_PROLOGUE_(ECMSProviderSwitch, MSProvider);
	HRESULT hr = pThis->QueryInterface(refiid, lppInterface);
	TRACE_MAPI(TRACE_RETURN, "ECMSProviderSwitch::QueryInterface", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMSProviderSwitch::xMSProvider::Shutdown(ULONG *lpulFlags)
{
	TRACE_MAPI(TRACE_ENTRY, "ECMSProviderSwitch::Shutdown", "");
	METHOD_PROLOGUE_(ECMSProviderSwitch, MSProvider);
	return pThis->Shutdown(lpulFlags);
}

HRESULT ECMSProviderSwitch::xMSProvider::Logon(LPMAPISUP lpMAPISup, ULONG ulUIParam, LPTSTR lpszProfileName, ULONG cbEntryID, LPENTRYID lpEntryID, ULONG ulFlags, LPCIID lpInterface, ULONG *lpcbSpoolSecurity, LPBYTE *lppbSpoolSecurity, LPMAPIERROR *lppMAPIError, LPMSLOGON *lppMSLogon, LPMDB *lppMDB)
{
	TRACE_MAPI(TRACE_ENTRY, "ECMSProviderSwitch::Logon", "flags=%x, cbEntryID=%d", ulFlags, cbEntryID);
	METHOD_PROLOGUE_(ECMSProviderSwitch, MSProvider);
	HRESULT hr = pThis->Logon(lpMAPISup, ulUIParam, lpszProfileName, cbEntryID, lpEntryID,ulFlags, lpInterface, lpcbSpoolSecurity, lppbSpoolSecurity, lppMAPIError, lppMSLogon, lppMDB);
	TRACE_MAPI(TRACE_RETURN, "ECMSProviderSwitch::Logon", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMSProviderSwitch::xMSProvider::SpoolerLogon(LPMAPISUP lpMAPISup, ULONG ulUIParam, LPTSTR lpszProfileName, ULONG cbEntryID, LPENTRYID lpEntryID, ULONG ulFlags, LPCIID lpInterface, ULONG lpcbSpoolSecurity, LPBYTE lppbSpoolSecurity, LPMAPIERROR *lppMAPIError, LPMSLOGON *lppMSLogon, LPMDB *lppMDB)
{
	TRACE_MAPI(TRACE_ENTRY, "ECMSProviderSwitch::SpoolerLogon", "flags=%x", ulFlags);
	METHOD_PROLOGUE_(ECMSProviderSwitch, MSProvider);
	HRESULT hr = pThis->SpoolerLogon(lpMAPISup, ulUIParam, lpszProfileName, cbEntryID, lpEntryID,ulFlags, lpInterface, lpcbSpoolSecurity, lppbSpoolSecurity, lppMAPIError, lppMSLogon, lppMDB);
	TRACE_MAPI(TRACE_RETURN, "ECMSProviderSwitch::SpoolerLogon", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMSProviderSwitch::xMSProvider::CompareStoreIDs(ULONG cbEntryID1, LPENTRYID lpEntryID1, ULONG cbEntryID2, LPENTRYID lpEntryID2, ULONG ulFlags, ULONG *lpulResult)
{
	TRACE_MAPI(TRACE_ENTRY, "ECMSProviderSwitch::CompareStoreIDs", "flags: %d\ncb=%d  entryid1: %s\n cb=%d entryid2: %s", ulFlags, cbEntryID1, bin2hex(cbEntryID1, (BYTE*)lpEntryID1).c_str(), cbEntryID2, bin2hex(cbEntryID2, (BYTE*)lpEntryID2).c_str());
	METHOD_PROLOGUE_(ECMSProviderSwitch, MSProvider);
	HRESULT hr = pThis->CompareStoreIDs(cbEntryID1, lpEntryID1, cbEntryID2, lpEntryID2, ulFlags, lpulResult);
	TRACE_MAPI(TRACE_RETURN, "ECMSProviderSwitch::CompareStoreIDs", "%s %s", GetMAPIErrorDescription(hr).c_str(), (*lpulResult == TRUE)?"TRUE": "FALSE");
	return hr;
}

