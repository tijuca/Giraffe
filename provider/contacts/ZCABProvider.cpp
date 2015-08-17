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
#include "ZCABProvider.h"
#include "ZCABLogon.h"

#include <mapidefs.h>
#include <mapicode.h>
#include <mapiguid.h>

#include "ECGuid.h"
#include "ECDebug.h"
#include "Trace.h"

ZCABProvider::ZCABProvider(ULONG ulFlags, const char *szClassName) :
    ECUnknown(szClassName), m_ulFlags(ulFlags)
{
}

ZCABProvider::~ZCABProvider()
{
}

HRESULT ZCABProvider::Create(ZCABProvider **lppZCABProvider)
{
	HRESULT hr = hrSuccess;
	ZCABProvider *lpZCABProvider = NULL;

	try {
		lpZCABProvider = new ZCABProvider(0, "ZCABProvider");
	} catch (...) {
		hr = MAPI_E_NOT_ENOUGH_MEMORY;
		goto exit;
	}

	hr = lpZCABProvider->QueryInterface(IID_ZCABProvider, (void **)lppZCABProvider);

	if(hr != hrSuccess)
		delete lpZCABProvider;

exit:
	return hr;
}

HRESULT ZCABProvider::QueryInterface(REFIID refiid, void **lppInterface)
{
	REGISTER_INTERFACE(IID_ZCABProvider, this);
	REGISTER_INTERFACE(IID_ECUnknown, this);

	REGISTER_INTERFACE(IID_IABProvider, &this->m_xABProvider);
	REGISTER_INTERFACE(IID_IUnknown, &this->m_xABProvider);

	return MAPI_E_INTERFACE_NOT_SUPPORTED;
}

HRESULT ZCABProvider::Shutdown(ULONG * lpulFlags)
{
	*lpulFlags = 0;
	return hrSuccess;
}

HRESULT ZCABProvider::Logon(LPMAPISUP lpMAPISup, ULONG ulUIParam, LPTSTR lpszProfileName, ULONG ulFlags, ULONG * lpulcbSecurity, LPBYTE * lppbSecurity, LPMAPIERROR * lppMAPIError, LPABLOGON * lppABLogon)
{
	HRESULT hr = hrSuccess;
	ZCABLogon* lpABLogon = NULL;

	if (!lpMAPISup || !lppABLogon) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	// todo: remove flags & guid .. probably add other stuff from profile?
	hr = ZCABLogon::Create(lpMAPISup, 0, NULL, &lpABLogon);
	if(hr != hrSuccess)
		goto exit;

	AddChild(lpABLogon);

	hr = lpABLogon->QueryInterface(IID_IABLogon, (void **)lppABLogon);
	if(hr != hrSuccess)
		goto exit;

	if (lpulcbSecurity)
		*lpulcbSecurity = 0;

	if (lppbSecurity)
		*lppbSecurity = NULL;

	if (lppMAPIError)
		*lppMAPIError = NULL;

exit:
	if (lpABLogon)
		lpABLogon->Release();

	return hr;
}

ULONG ZCABProvider::xABProvider::AddRef()
{
	TRACE_MAPI(TRACE_ENTRY, "IABProvider::AddRef", "");
	METHOD_PROLOGUE_(ZCABProvider, ABProvider);
	return pThis->AddRef();
}

ULONG ZCABProvider::xABProvider::Release()
{
	TRACE_MAPI(TRACE_ENTRY, "IABProvider::Release", "");
	METHOD_PROLOGUE_(ZCABProvider, ABProvider);
	return pThis->Release();
}

HRESULT ZCABProvider::xABProvider::QueryInterface(REFIID refiid, void **lppInterface)
{
	TRACE_MAPI(TRACE_ENTRY, "IABProvider::QueryInterface", "%s", DBGGUIDToString(refiid).c_str());
	METHOD_PROLOGUE_(ZCABProvider , ABProvider);
	HRESULT hr = pThis->QueryInterface(refiid, lppInterface);
	TRACE_MAPI(TRACE_RETURN, "IABProvider::QueryInterface", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ZCABProvider::xABProvider::Shutdown(ULONG * lpulFlags)
{
	TRACE_MAPI(TRACE_ENTRY, "IABProvider::Shutdown", "");
	METHOD_PROLOGUE_(ZCABProvider, ABProvider);
	HRESULT hr = pThis->Shutdown(lpulFlags);
	TRACE_MAPI(TRACE_RETURN, "IABProvider::Shutdown", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ZCABProvider::xABProvider::Logon(LPMAPISUP lpMAPISup, ULONG ulUIParam, LPTSTR lpszProfileName, ULONG ulFlags, ULONG * lpulcbSecurity, LPBYTE * lppbSecurity, LPMAPIERROR * lppMAPIError, LPABLOGON * lppABLogon)
{
	TRACE_MAPI(TRACE_ENTRY, "IABProvider::Logon", "");
	METHOD_PROLOGUE_(ZCABProvider, ABProvider);
	HRESULT hr = pThis->Logon(lpMAPISup, ulUIParam, lpszProfileName, ulFlags, lpulcbSecurity, lppbSecurity, lppMAPIError, lppABLogon);
	TRACE_MAPI(TRACE_RETURN, "IABProvider::Logon", "%s", GetMAPIErrorDescription(hr).c_str());
	return  hr;
}
