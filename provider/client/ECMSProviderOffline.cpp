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

#include <memory.h>
#include <mapi.h>
#include <mapiutil.h>
#include <mapispi.h>

#include "ECMSProviderOffline.h"
#include "ECMSProvider.h"

#include "ECGuid.h"

#include "Trace.h"
#include "ECDebug.h"


#include "edkguid.h"
#include "EntryPoint.h"
#include "DLLGlobal.h"
#include "edkmdb.h"

#include "ClientUtil.h"
#include "ECMsgStore.h"
#include "stringutil.h"

#include "ProviderUtil.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


ECMSProviderOffline::ECMSProviderOffline(ULONG ulFlags) : 
	ECMSProvider(ulFlags|EC_PROVIDER_OFFLINE, "ECMSProviderOffline")
{

}

HRESULT ECMSProviderOffline::Create(ULONG ulFlags, ECMSProviderOffline **lppMSProvider)
{
	ECMSProviderOffline *lpMSProvider = new ECMSProviderOffline(ulFlags);

	return lpMSProvider->QueryInterface(IID_ECUnknown/*IID_ECMSProviderOffline*/, (void **)lppMSProvider);
}

HRESULT ECMSProviderOffline::QueryInterface(REFIID refiid, void **lppInterface)
{
	/* refiid == IID_ECMSProviderOffline */
	REGISTER_INTERFACE(IID_ECUnknown, this);

	REGISTER_INTERFACE(IID_IMSProvider, &this->m_xMSProvider);
	REGISTER_INTERFACE(IID_IUnknown, &this->m_xMSProvider);

	return MAPI_E_INTERFACE_NOT_SUPPORTED;
}

ULONG ECMSProviderOffline::xMSProvider::AddRef() 
{
	TRACE_MAPI(TRACE_ENTRY, "ECMSProviderOffline::AddRef", "");
	METHOD_PROLOGUE_(ECMSProviderOffline, MSProvider);
	return pThis->AddRef();
}

ULONG ECMSProviderOffline::xMSProvider::Release()
{
	TRACE_MAPI(TRACE_ENTRY, "ECMSProviderOffline::Release", "");
	METHOD_PROLOGUE_(ECMSProviderOffline, MSProvider);
	return pThis->Release();
}

HRESULT ECMSProviderOffline::xMSProvider::QueryInterface(REFIID refiid, void **lppInterface)
{
	TRACE_MAPI(TRACE_ENTRY, "ECMSProviderOffline::QueryInterface", "%s", DBGGUIDToString(refiid).c_str());
	METHOD_PROLOGUE_(ECMSProviderOffline, MSProvider);
	HRESULT hr = pThis->QueryInterface(refiid, lppInterface);
	TRACE_MAPI(TRACE_RETURN, "ECMSProviderOffline::QueryInterface", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMSProviderOffline::xMSProvider::Shutdown(ULONG *lpulFlags)
{
	TRACE_MAPI(TRACE_ENTRY, "ECMSProviderOffline::Shutdown", "");
	METHOD_PROLOGUE_(ECMSProviderOffline, MSProvider);
	return pThis->Shutdown(lpulFlags);
}

HRESULT ECMSProviderOffline::xMSProvider::Logon(LPMAPISUP lpMAPISup, ULONG ulUIParam, LPTSTR lpszProfileName, ULONG cbEntryID, LPENTRYID lpEntryID, ULONG ulFlags, LPCIID lpInterface, ULONG *lpcbSpoolSecurity, LPBYTE *lppbSpoolSecurity, LPMAPIERROR *lppMAPIError, LPMSLOGON *lppMSLogon, LPMDB *lppMDB)
{
	TRACE_MAPI(TRACE_ENTRY, "ECMSProviderOffline::Logon", "flags=%x, cbEntryID=%d", ulFlags, cbEntryID);
	METHOD_PROLOGUE_(ECMSProviderOffline, MSProvider);
	HRESULT hr = pThis->Logon(lpMAPISup, ulUIParam, lpszProfileName, cbEntryID, lpEntryID,ulFlags, lpInterface, lpcbSpoolSecurity, lppbSpoolSecurity, lppMAPIError, lppMSLogon, lppMDB);
	TRACE_MAPI(TRACE_RETURN, "ECMSProviderOffline::Logon", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMSProviderOffline::xMSProvider::SpoolerLogon(LPMAPISUP lpMAPISup, ULONG ulUIParam, LPTSTR lpszProfileName, ULONG cbEntryID, LPENTRYID lpEntryID, ULONG ulFlags, LPCIID lpInterface, ULONG lpcbSpoolSecurity, LPBYTE lppbSpoolSecurity, LPMAPIERROR *lppMAPIError, LPMSLOGON *lppMSLogon, LPMDB *lppMDB)
{
	TRACE_MAPI(TRACE_ENTRY, "ECMSProviderOffline::SpoolerLogon", "flags=%x", ulFlags);
	METHOD_PROLOGUE_(ECMSProviderOffline, MSProvider);
	HRESULT hr = pThis->SpoolerLogon(lpMAPISup, ulUIParam, lpszProfileName, cbEntryID, lpEntryID,ulFlags, lpInterface, lpcbSpoolSecurity, lppbSpoolSecurity, lppMAPIError, lppMSLogon, lppMDB);
	TRACE_MAPI(TRACE_RETURN, "ECMSProviderOffline::SpoolerLogon", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMSProviderOffline::xMSProvider::CompareStoreIDs(ULONG cbEntryID1, LPENTRYID lpEntryID1, ULONG cbEntryID2, LPENTRYID lpEntryID2, ULONG ulFlags, ULONG *lpulResult)
{
	TRACE_MAPI(TRACE_ENTRY, "ECMSProviderOffline::CompareStoreIDs", "flags: %d\ncb=%d  entryid1: %s\n cb=%d entryid2: %s", ulFlags, cbEntryID1, bin2hex(cbEntryID1, (BYTE*)lpEntryID1).c_str(), cbEntryID2, bin2hex(cbEntryID2, (BYTE*)lpEntryID2).c_str());
	METHOD_PROLOGUE_(ECMSProviderOffline, MSProvider);
	HRESULT hr = pThis->CompareStoreIDs(cbEntryID1, lpEntryID1, cbEntryID2, lpEntryID2, ulFlags, lpulResult);
	TRACE_MAPI(TRACE_RETURN, "ECMSProviderOffline::CompareStoreIDs", "%s %s",GetMAPIErrorDescription(hr).c_str(), (*lpulResult == TRUE)?"TRUE": "FALSE");
	return hr;
}
