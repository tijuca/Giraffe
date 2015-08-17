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
#include <mapidefs.h>
#include <mapispi.h>
#include "ECDebug.h"
#include "Trace.h"

#include "ZCABProvider.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static const char THIS_FILE[] = __FILE__;
#endif

extern "C" MSGSERVICEENTRY __stdcall MSGServiceEntry;

HRESULT __stdcall MSGServiceEntry(HINSTANCE hInst, LPMALLOC lpMalloc,
    LPMAPISUP psup, ULONG ulUIParam, ULONG ulFlags, ULONG ulContext,
    ULONG cvals, LPSPropValue pvals, LPPROVIDERADMIN lpAdminProviders,
    MAPIERROR **lppMapiError)
{
	TRACE_MAPI(TRACE_ENTRY, "MSGServiceEntry", "flags=0x%08X, context=%s", ulFlags, MsgServiceContextToString(ulContext));

	HRESULT hr = hrSuccess;

	switch(ulContext) {
	case MSG_SERVICE_INSTALL:
		hr = hrSuccess;
		break;
	case MSG_SERVICE_UNINSTALL:
		hr = hrSuccess;
		break;
	case MSG_SERVICE_DELETE:
		hr = hrSuccess;
		break;
	case MSG_SERVICE_PROVIDER_CREATE:
		// we never get here in linux (see M4LProviderAdmin::CreateProvider)
		ASSERT(FALSE);
		hr = hrSuccess;
		break;
	case MSG_SERVICE_PROVIDER_DELETE:
		hr = hrSuccess;
		break;
	case MSG_SERVICE_CONFIGURE:
	case MSG_SERVICE_CREATE:
		hr = hrSuccess;
		break;
	};

	if (lppMapiError)
		*lppMapiError = NULL;


	TRACE_MAPI(TRACE_RETURN, "MSGServiceEntry", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

extern "C" HRESULT  __cdecl ABProviderInit(HINSTANCE hInstance, LPMALLOC lpMalloc, LPALLOCATEBUFFER lpAllocateBuffer, LPALLOCATEMORE lpAllocateMore, LPFREEBUFFER lpFreeBuffer, ULONG ulFlags, ULONG ulMAPIVer, ULONG * lpulProviderVer, LPABPROVIDER * lppABProvider)
{
	TRACE_MAPI(TRACE_ENTRY, "ZContacts::ABProviderInit", "");

	HRESULT hr = hrSuccess;
	ZCABProvider *lpABProvider = NULL;

	if (ulMAPIVer < CURRENT_SPI_VERSION)
	{
		hr = MAPI_E_VERSION;
		goto exit;
	}

	// create provider and query interface.
	hr = ZCABProvider::Create(&lpABProvider);
	if (hr != hrSuccess)
		goto exit;

	hr = lpABProvider->QueryInterface(IID_IABProvider, (void **)lppABProvider);
	if (hr != hrSuccess)
		goto exit;

	*lpulProviderVer = CURRENT_SPI_VERSION;

exit:
	if (lpABProvider)
		lpABProvider->Release();

	TRACE_MAPI(TRACE_RETURN, "ZContacts::ABProviderInit", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}
