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

// ECXPProvider.cpp: implementation of the ECXPProvider class.
//
//////////////////////////////////////////////////////////////////////

#include "platform.h"
#include <mapi.h>
#include <mapispi.h>
#include <mapiutil.h>
#include "ECGuid.h"


#include "Zarafa.h"
#include "ECXPProvider.h"
#include "ECXPLogon.h"

#include "WSTransport.h"
#include "Mem.h"

#include "Util.h"

#include "ECDebug.h"

#include "ClientUtil.h"
#include "EntryPoint.h"

#include <charset/convstring.h>
#include "ECGetText.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

ECXPProvider::ECXPProvider() : ECUnknown("IXPProvider")
{
	m_lpIdentityProps = NULL;
}

ECXPProvider::~ECXPProvider()
{
	if(m_lpIdentityProps)
		ECFreeBuffer(m_lpIdentityProps);
}

HRESULT ECXPProvider::Create(ECXPProvider **lppECXPProvider) {
	ECXPProvider *lpECXPProvider = new ECXPProvider();

	return lpECXPProvider->QueryInterface(IID_ECXPProvider, (void **)lppECXPProvider);
}

HRESULT ECXPProvider::QueryInterface(REFIID refiid, void **lppInterface)
{
	REGISTER_INTERFACE(IID_ECXPProvider, this);

	REGISTER_INTERFACE(IID_IXPProvider, &this->m_xXPProvider);

	return MAPI_E_INTERFACE_NOT_SUPPORTED;
}

HRESULT ECXPProvider::Shutdown(ULONG * lpulFlags)
{
	HRESULT hr = hrSuccess;

	return hr;
}

HRESULT ECXPProvider::TransportLogon(LPMAPISUP lpMAPISup, ULONG ulUIParam, LPTSTR lpszProfileName, ULONG * lpulFlags, LPMAPIERROR * lppMAPIError, LPXPLOGON * lppXPLogon)
{
	HRESULT			hr = hrSuccess;
	ECXPLogon		*lpXPLogon = NULL;
	WSTransport		*lpTransport = NULL;
	ECMapProvider::iterator iterMap;
	std::string		strServerURL;
	std::string		strUniqueId;
	BOOL			bOffline = FALSE;
	convstring			tstrProfileName(lpszProfileName, *lpulFlags);
	std::string		strDisplayName;

	// Get transport by looking at how we have logged on. We assume here that a message store or addressbook has been
	// logged on before calling TransportLogon and therefore the connection type is never CT_UNSPECIFIED.
	iterMap = g_mapProviders.find(tstrProfileName);

	// Online if: no entry in map, OR map specifies online mode
	if(iterMap == g_mapProviders.end() || iterMap->second.ulConnectType == CT_ONLINE) {
		// Online
		hr = WSTransport::HrOpenTransport(lpMAPISup, &lpTransport, FALSE);
		bOffline = FALSE;
	} else {
		// Offline
		hr = WSTransport::HrOpenTransport(lpMAPISup, &lpTransport, TRUE);
		bOffline = TRUE;
	}

	if(hr != hrSuccess) {
		hr = MAPI_E_FAILONEPROVIDER;
		goto exit;
	}

	hr = ECXPLogon::Create(tstrProfileName, bOffline, this, lpMAPISup, &lpXPLogon);
	if(hr != hrSuccess)
		goto exit;

	hr = lpXPLogon->QueryInterface(IID_IXPLogon, (void **)lppXPLogon);
	if(hr != hrSuccess)
		goto exit;

	AddChild(lpXPLogon);

	// Set profile identity
	hr = ClientUtil::HrSetIdentity(lpTransport, lpMAPISup, &m_lpIdentityProps);
	if(hr != hrSuccess)
		goto exit;

	// Initialize statusrow
	strDisplayName = convert_to<std::string>(g_strManufacturer.c_str()) + _A(" Transport");

	hr = ClientUtil::HrInitializeStatusRow(strDisplayName.c_str(), MAPI_TRANSPORT_PROVIDER, lpMAPISup, m_lpIdentityProps, 0);
	if(hr != hrSuccess)
		goto exit;

	*lpulFlags = 0;
	*lppMAPIError = NULL;
	
exit:
	if(lpTransport)
		lpTransport->Release();

	if(lpXPLogon)
		lpXPLogon->Release();

	return hr;
}

HRESULT __stdcall ECXPProvider::xXPProvider::QueryInterface(REFIID refiid, void ** lppInterface)
{
	TRACE_MAPI(TRACE_ENTRY, "IXPProvider::QueryInterface", "%s", DBGGUIDToString(refiid).c_str());
	METHOD_PROLOGUE_(ECXPProvider , XPProvider);
	HRESULT hr = pThis->QueryInterface(refiid, lppInterface);
	TRACE_MAPI(TRACE_RETURN, "IXPProvider::QueryInterface", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

ULONG __stdcall ECXPProvider::xXPProvider::AddRef()
{
	TRACE_MAPI(TRACE_ENTRY, "IXPProvider::AddRef", "");
	METHOD_PROLOGUE_(ECXPProvider , XPProvider);
	return pThis->AddRef();
}

ULONG __stdcall ECXPProvider::xXPProvider::Release()
{
	TRACE_MAPI(TRACE_ENTRY, "IXPProvider::Release", "");
	METHOD_PROLOGUE_(ECXPProvider , XPProvider);
	return pThis->Release();
}

HRESULT ECXPProvider::xXPProvider::Shutdown(ULONG *lpulFlags)
{
	TRACE_MAPI(TRACE_ENTRY, "IXPProvider::Shutdown", "");
	METHOD_PROLOGUE_(ECXPProvider , XPProvider);
	return pThis->Shutdown(lpulFlags);
}

HRESULT ECXPProvider::xXPProvider::TransportLogon(LPMAPISUP lpMAPISup, ULONG ulUIParam, LPTSTR lpszProfileName, ULONG FAR * lpulFlags, LPMAPIERROR FAR * lppMAPIError, LPXPLOGON FAR * lppXPLogon)
{
	TRACE_MAPI(TRACE_ENTRY, "IXPProvider::TransportLogon", "");
	METHOD_PROLOGUE_(ECXPProvider , XPProvider);
	return pThis->TransportLogon(lpMAPISup, ulUIParam, lpszProfileName, lpulFlags, lppMAPIError, lppXPLogon);
}
