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

// ECABProvider.cpp: implementation of the ECABProvider class.
//
//////////////////////////////////////////////////////////////////////

#include "platform.h"
#include <mapi.h>
#include <mapiext.h>
#include <mapispi.h>
#include <mapiutil.h>
#include "Zarafa.h"
#include "ECGuid.h"
#include "edkguid.h"
#include "ECABProvider.h"
#include "ECABLogon.h"

#include "ECDebug.h"


#include "Util.h"

#include "WSTransport.h"
#include "ClientUtil.h"
#include "EntryPoint.h"
#include "ZarafaUtil.h"

#include <mapi_ptr/mapi_memory_ptr.h>
typedef mapi_memory_ptr<ECUSER>	ECUserPtr;

#include "ECGetText.h"

using namespace std;

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

ECABProvider::ECABProvider(ULONG ulFlags, char *szClassName) : ECUnknown(szClassName)
{
	m_ulFlags = ulFlags;
}

ECABProvider::~ECABProvider()
{

}

HRESULT ECABProvider::Create(ECABProvider **lppECABProvider)
{
	HRESULT hr = hrSuccess;

	ECABProvider *lpECABProvider = new ECABProvider(0, "ECABProvider");

	hr = lpECABProvider->QueryInterface(IID_ECABProvider, (void **)lppECABProvider);

	if(hr != hrSuccess)
		delete lpECABProvider;

	return hr;
}

HRESULT ECABProvider::QueryInterface(REFIID refiid, void **lppInterface)
{
	REGISTER_INTERFACE(IID_ECABProvider, this);
	REGISTER_INTERFACE(IID_ECUnknown, this);

	REGISTER_INTERFACE(IID_IABProvider, &this->m_xABProvider);
	REGISTER_INTERFACE(IID_IUnknown, &this->m_xABProvider);

	REGISTER_INTERFACE(IID_ISelectUnicode, &this->m_xUnknown);

	return MAPI_E_INTERFACE_NOT_SUPPORTED;
}

HRESULT ECABProvider::Shutdown(ULONG * lpulFlags)
{
	HRESULT hr = hrSuccess;

	return hr;
}

HRESULT ECABProvider::Logon(LPMAPISUP lpMAPISup, ULONG ulUIParam, LPTSTR lpszProfileName, ULONG ulFlags, ULONG * lpulcbSecurity, LPBYTE * lppbSecurity, LPMAPIERROR * lppMAPIError, LPABLOGON * lppABLogon)
{
	HRESULT			hr = hrSuccess;
	ECABLogon*		lpABLogon = NULL;
	LPSPropValue lpProviderUid = NULL;
	LPSPropValue lpSectionUid = NULL;
	IProfSect *lpProfSect = NULL;
	IProfSect *lpProfSectSection = NULL;
	IProfSect *lpProfSectService = NULL;
	LPSPropValue lpUidService = NULL;
	sGlobalProfileProps	sProfileProps;
	LPMAPIUID	lpGuid = NULL;

	WSTransport*	lpTransport = NULL;


	if (!lpMAPISup || !lppABLogon) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	// Get the username and password from the profile settings
	hr = ClientUtil::GetGlobalProfileProperties(lpMAPISup, &sProfileProps);
	if(hr != hrSuccess)
		goto exit;

	// Create a transport for this provider
	hr = WSTransport::Create(ulFlags, &lpTransport);
	if(hr != hrSuccess)
		goto exit;


	// Log on the transport to the server
	hr = lpTransport->HrLogon(sProfileProps);


	if(hr != hrSuccess)
		goto exit;


	hr = ECABLogon::Create(lpMAPISup, lpTransport, sProfileProps.ulProfileFlags, (GUID *)lpGuid, &lpABLogon);
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
	if (lpProfSectService)
		lpProfSectService->Release();

	if (lpUidService)
		MAPIFreeBuffer(lpUidService);

	if (lpProviderUid)
		MAPIFreeBuffer(lpProviderUid);

	if (lpSectionUid)
		MAPIFreeBuffer(lpSectionUid);

	if (lpProfSect)
		lpProfSect->Release();

	if (lpProfSectSection)
		lpProfSectSection->Release();

	if(lpABLogon)
		lpABLogon->Release();

	if(lpTransport)
		lpTransport->Release();

	return hr;
}


HRESULT __stdcall ECABProvider::xABProvider::QueryInterface(REFIID refiid, void ** lppInterface)
{
	TRACE_MAPI(TRACE_ENTRY, "IABProvider::QueryInterface", "%s", DBGGUIDToString(refiid).c_str());
	METHOD_PROLOGUE_(ECABProvider , ABProvider);
	HRESULT hr = pThis->QueryInterface(refiid, lppInterface);
	TRACE_MAPI(TRACE_RETURN, "IABProvider::QueryInterface", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

ULONG __stdcall ECABProvider::xABProvider::AddRef()
{
	TRACE_MAPI(TRACE_ENTRY, "IABProvider::AddRef", "");
	METHOD_PROLOGUE_(ECABProvider , ABProvider);
	return pThis->AddRef();
}

ULONG __stdcall ECABProvider::xABProvider::Release()
{
	TRACE_MAPI(TRACE_ENTRY, "IABProvider::Release", "");
	METHOD_PROLOGUE_(ECABProvider , ABProvider);
	return pThis->Release();
}

HRESULT ECABProvider::xABProvider::Shutdown(ULONG *lpulFlags)
{
	TRACE_MAPI(TRACE_ENTRY, "IABProvider::Shutdown", "");
	METHOD_PROLOGUE_(ECABProvider , ABProvider);
	return pThis->Shutdown(lpulFlags);
}

HRESULT ECABProvider::xABProvider::Logon(LPMAPISUP lpMAPISup, ULONG ulUIParam, LPTSTR lpszProfileName, ULONG ulFlags, ULONG * lpulcbSecurity, LPBYTE * lppbSecurity, LPMAPIERROR * lppMAPIError, LPABLOGON * lppABLogon)
{
	TRACE_MAPI(TRACE_ENTRY, "IABProvider::Logon", "");
	METHOD_PROLOGUE_(ECABProvider , ABProvider);
	HRESULT hr = pThis->Logon(lpMAPISup, ulUIParam, lpszProfileName, ulFlags, lpulcbSecurity, lppbSecurity, lppMAPIError, lppABLogon);
	TRACE_MAPI(TRACE_RETURN, "IABProvider::Logon", "%s", GetMAPIErrorDescription(hr).c_str());
	return  hr;
}
