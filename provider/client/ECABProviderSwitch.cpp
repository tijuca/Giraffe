/*
 * Copyright 2005 - 2015  Zarafa B.V. and its licensors
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License, version 3,
 * as published by the Free Software Foundation with the following
 * additional terms according to sec. 7:
 * 
 * "Zarafa" is a registered trademark of Zarafa B.V.
 * The licensing of the Program under the AGPL does not imply a trademark 
 * license. Therefore any rights, title and interest in our trademarks 
 * remain entirely with us.
 * 
 * Our trademark policy (see TRADEMARKS.txt) allows you to use our trademarks
 * in connection with Propagation and certain other acts regarding the Program.
 * In any case, if you propagate an unmodified version of the Program you are
 * allowed to use the term "Zarafa" to indicate that you distribute the Program.
 * Furthermore you may use our trademarks where it is necessary to indicate the
 * intended purpose of a product or service provided you use it in accordance
 * with honest business practices. For questions please contact Zarafa at
 * trademark@zarafa.com.
 *
 * The interactive user interface of the software displays an attribution 
 * notice containing the term "Zarafa" and/or the logo of Zarafa. 
 * Interactive user interfaces of unmodified and modified versions must 
 * display Appropriate Legal Notices according to sec. 5 of the GNU Affero 
 * General Public License, version 3, when you propagate unmodified or 
 * modified versions of the Program. In accordance with sec. 7 b) of the GNU 
 * Affero General Public License, version 3, these Appropriate Legal Notices 
 * must retain the logo of Zarafa or display the words "Initial Development 
 * by Zarafa" if the display of the logo is not reasonably feasible for
 * technical reasons.
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

#include <memory.h>
#include <mapi.h>
#include <mapiutil.h>
#include <mapispi.h>
#include "ECABProvider.h"

#include "ECGetText.h"

#include "ECGuid.h"
#include "edkguid.h"

#include "Trace.h"
#include "ECDebug.h"

#include "EntryPoint.h"
#include "mapiext.h"

#include "ECABProviderSwitch.h"
#include "ECOfflineState.h"


#include "ProviderUtil.h"

#include "charset/convstring.h"

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

ECABProviderSwitch::ECABProviderSwitch(void) : ECUnknown("ECABProviderSwitch")
{
}

ECABProviderSwitch::~ECABProviderSwitch(void)
{
}

HRESULT ECABProviderSwitch::Create(ECABProviderSwitch **lppECABProvider)
{
	HRESULT hr = hrSuccess;

	ECABProviderSwitch *lpECABProvider = new ECABProviderSwitch();

	hr = lpECABProvider->QueryInterface(IID_ECABProvider, (void **)lppECABProvider);

	if(hr != hrSuccess)
		delete lpECABProvider;

	return hr;
}

HRESULT ECABProviderSwitch::QueryInterface(REFIID refiid, void **lppInterface)
{
	REGISTER_INTERFACE(IID_ECABProvider, this);
	REGISTER_INTERFACE(IID_ECUnknown, this);

	REGISTER_INTERFACE(IID_IABProvider, &this->m_xABProvider);
	REGISTER_INTERFACE(IID_IUnknown, &this->m_xABProvider);

	REGISTER_INTERFACE(IID_ISelectUnicode, &this->m_xUnknown);

	return MAPI_E_INTERFACE_NOT_SUPPORTED;
}

HRESULT ECABProviderSwitch::Shutdown(ULONG * lpulFlags)
{
	HRESULT hr = hrSuccess;

	return hr;
}

HRESULT ECABProviderSwitch::Logon(LPMAPISUP lpMAPISup, ULONG ulUIParam, LPTSTR lpszProfileName, ULONG ulFlags, ULONG * lpulcbSecurity, LPBYTE * lppbSecurity, LPMAPIERROR * lppMAPIError, LPABLOGON * lppABLogon)
{
	HRESULT hr = hrSuccess;
	PROVIDER_INFO sProviderInfo;
	ULONG ulConnectType = CT_UNSPECIFIED;

	IABLogon *lpABLogon = NULL;
	IABProvider *lpOnline = NULL;

	convstring tstrProfileName(lpszProfileName, ulFlags);


	hr = GetProviders(&g_mapProviders, lpMAPISup, convstring(lpszProfileName, ulFlags).c_str(), ulFlags, &sProviderInfo);
	if (hr != hrSuccess)
		goto exit;

	hr = sProviderInfo.lpABProviderOnline->QueryInterface(IID_IABProvider, (void **)&lpOnline);
	if (hr != hrSuccess)
		goto exit;

		// Online
		hr = lpOnline->Logon(lpMAPISup, ulUIParam, lpszProfileName, ulFlags, NULL, NULL, NULL, &lpABLogon);
		ulConnectType = CT_ONLINE;

	// Set the provider in the right connection type
	if (SetProviderMode(lpMAPISup, &g_mapProviders, convstring(lpszProfileName, ulFlags).c_str(), ulConnectType) != hrSuccess) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
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

	hr = lpMAPISup->SetProviderUID((LPMAPIUID)&MUIDECSAB, 0);
	if(hr != hrSuccess)
		goto exit;
	
	hr = lpABLogon->QueryInterface(IID_IABLogon, (void **)lppABLogon);
	if(hr != hrSuccess)
		goto exit;

	if(lpulcbSecurity)
		*lpulcbSecurity = 0;

	if(lppbSecurity)
		*lppbSecurity = NULL;

	if (lppMAPIError)
		*lppMAPIError = NULL;

exit:
	if (lpABLogon)
		lpABLogon->Release();

	if (lpOnline)
		lpOnline->Release();


	return hr;
}

HRESULT __stdcall ECABProviderSwitch::xABProvider::QueryInterface(REFIID refiid, void ** lppInterface)
{
	TRACE_MAPI(TRACE_ENTRY, "ECABProviderSwitch::QueryInterface", "%s", DBGGUIDToString(refiid).c_str());
	METHOD_PROLOGUE_(ECABProviderSwitch , ABProvider);
	HRESULT hr = pThis->QueryInterface(refiid, lppInterface);
	TRACE_MAPI(TRACE_RETURN, "ECABProviderSwitch::QueryInterface", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

ULONG __stdcall ECABProviderSwitch::xABProvider::AddRef()
{
	TRACE_MAPI(TRACE_ENTRY, "ECABProviderSwitch::AddRef", "");
	METHOD_PROLOGUE_(ECABProviderSwitch , ABProvider);
	return pThis->AddRef();
}

ULONG __stdcall ECABProviderSwitch::xABProvider::Release()
{
	TRACE_MAPI(TRACE_ENTRY, "ECABProviderSwitch::Release", "");
	METHOD_PROLOGUE_(ECABProviderSwitch , ABProvider);
	ULONG ulRef = pThis->Release();
	TRACE_MAPI(TRACE_RETURN, "ECABProviderSwitch::Release", "%d", ulRef);
	return ulRef;
}

HRESULT ECABProviderSwitch::xABProvider::Shutdown(ULONG *lpulFlags)
{
	TRACE_MAPI(TRACE_ENTRY, "ECABProviderSwitch::Shutdown", "");
	METHOD_PROLOGUE_(ECABProviderSwitch , ABProvider);
	return pThis->Shutdown(lpulFlags);
}

HRESULT ECABProviderSwitch::xABProvider::Logon(LPMAPISUP lpMAPISup, ULONG ulUIParam, LPTSTR lpszProfileName, ULONG ulFlags, ULONG * lpulcbSecurity, LPBYTE * lppbSecurity, LPMAPIERROR * lppMAPIError, LPABLOGON * lppABLogon)
{
	TRACE_MAPI(TRACE_ENTRY, "ECABProviderSwitch::Logon", "");
	METHOD_PROLOGUE_(ECABProviderSwitch , ABProvider);
	HRESULT hr = pThis->Logon(lpMAPISup, ulUIParam, lpszProfileName, ulFlags, lpulcbSecurity, lppbSecurity, lppMAPIError, lppABLogon);
	TRACE_MAPI(TRACE_RETURN, "ECABProviderSwitch::Logon", "%s", GetMAPIErrorDescription(hr).c_str());
	return  hr;
}
