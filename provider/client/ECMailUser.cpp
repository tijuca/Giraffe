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

// ECMailUser.cpp: implementation of the ECMailUser class.
//
//////////////////////////////////////////////////////////////////////
#include "platform.h"

#include "resource.h"
#include "mapiutil.h"
#include "Zarafa.h"
#include "CommonUtil.h"
#include "ECMailUser.h"

#include "Mem.h"
#include "ECGuid.h"
#include "ECDebug.h"

#include "ECDisplayTable.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

ECMailUser::ECMailUser(void* lpProvider, BOOL fModify) : ECABProp(lpProvider, MAPI_MAILUSER, fModify, "IMailUser")
{
}

ECMailUser::~ECMailUser()
{

}

HRESULT ECMailUser::Create(void* lpProvider, BOOL fModify, ECMailUser** lppMailUser)
{

	HRESULT hr = hrSuccess;
	ECMailUser *lpMailUser = NULL;

	lpMailUser = new ECMailUser(lpProvider, fModify);

	hr = lpMailUser->QueryInterface(IID_ECMailUser, (void **)lppMailUser);

	if(hr != hrSuccess)
		delete lpMailUser;

	return hr;
}

HRESULT	ECMailUser::QueryInterface(REFIID refiid, void **lppInterface) 
{
	REGISTER_INTERFACE(IID_ECMailUser, this);
	REGISTER_INTERFACE(IID_ECABProp, this);
	REGISTER_INTERFACE(IID_ECUnknown, this);

	REGISTER_INTERFACE(IID_IMailUser, &this->m_xMailUser);
	REGISTER_INTERFACE(IID_IMAPIProp, &this->m_xMailUser);
	REGISTER_INTERFACE(IID_IUnknown, &this->m_xMailUser);

	return MAPI_E_INTERFACE_NOT_SUPPORTED;
}

HRESULT ECMailUser::TableRowGetProp(void* lpProvider, struct propVal *lpsPropValSrc, LPSPropValue lpsPropValDst, void **lpBase, ULONG ulType)
{
	HRESULT hr = hrSuccess;

	hr = MAPI_E_NOT_FOUND;

	return hr;
}

HRESULT ECMailUser::OpenProperty(ULONG ulPropTag, LPCIID lpiid, ULONG ulInterfaceOptions, ULONG ulFlags, LPUNKNOWN FAR * lppUnk)
{
	HRESULT			hr = MAPI_E_NOT_FOUND;

	if (lpiid == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	if (ulFlags & MAPI_CREATE)
    {
		// Don't support creating any sub-objects
		hr = MAPI_E_NO_ACCESS;
        goto exit;
    }

	switch(ulPropTag) {
	default:
		hr = ECABProp::OpenProperty(ulPropTag, lpiid, ulInterfaceOptions, ulFlags, lppUnk);
		break;
	}

exit:
	return hr;
}

HRESULT ECMailUser::CopyTo(ULONG ciidExclude, LPCIID rgiidExclude, LPSPropTagArray lpExcludeProps, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, LPCIID lpInterface, LPVOID lpDestObj, ULONG ulFlags, LPSPropProblemArray FAR * lppProblems)
{
	HRESULT hr = hrSuccess;
	
	hr = this->GetABStore()->m_lpMAPISup->DoCopyTo(&IID_IMailUser, &this->m_xMailUser, ciidExclude, rgiidExclude, lpExcludeProps, ulUIParam, lpProgress, lpInterface, lpDestObj, ulFlags, lppProblems);

	return hr;
}

HRESULT ECMailUser::CopyProps(LPSPropTagArray lpIncludeProps, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, LPCIID lpInterface, LPVOID lpDestObj, ULONG ulFlags, LPSPropProblemArray FAR * lppProblems)
{
	HRESULT hr = hrSuccess;

	hr = this->GetABStore()->m_lpMAPISup->DoCopyProps(&IID_IMailUser, &this->m_xMailUser, lpIncludeProps, ulUIParam, lpProgress, lpInterface, lpDestObj, ulFlags, lppProblems);

	return hr;
}

//////////////////////////////////////////////
// IMailUser
//

HRESULT ECMailUser::xMailUser::QueryInterface(REFIID refiid , void** lppInterface)
{
	TRACE_MAPI(TRACE_ENTRY, "IMailUser::QueryInterface", "%s", DBGGUIDToString(refiid).c_str());
	METHOD_PROLOGUE_(ECMailUser, MailUser);
	HRESULT hr = pThis->QueryInterface(refiid, lppInterface);
	TRACE_MAPI(TRACE_RETURN, "IMailUser::QueryInterface", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

ULONG ECMailUser::xMailUser::AddRef()
{
	TRACE_MAPI(TRACE_ENTRY, "IMailUser::AddRef", "");
	METHOD_PROLOGUE_(ECMailUser, MailUser);
	return pThis->AddRef();
}

ULONG ECMailUser::xMailUser::Release()
{
	TRACE_MAPI(TRACE_ENTRY, "IMailUser::Release", "");
	METHOD_PROLOGUE_(ECMailUser, MailUser);
	return pThis->Release();
}

HRESULT ECMailUser::xMailUser::GetLastError(HRESULT hError, ULONG ulFlags, LPMAPIERROR * lppMapiError)
{
	TRACE_MAPI(TRACE_ENTRY, "IMailUser::GetLastError", "herror=0x%08x", hError);
	METHOD_PROLOGUE_(ECMailUser, MailUser);
	HRESULT hr = pThis->GetLastError(hError, ulFlags, lppMapiError);
	TRACE_MAPI(TRACE_RETURN, "IMailUser::GetLastError", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMailUser::xMailUser::SaveChanges(ULONG ulFlags)
{
	TRACE_MAPI(TRACE_ENTRY, "IMailUser::SaveChanges", "");
	METHOD_PROLOGUE_(ECMailUser, MailUser);
	HRESULT hr = pThis->SaveChanges(ulFlags);
	TRACE_MAPI(TRACE_RETURN, "IMailUser::SaveChanges", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMailUser::xMailUser::GetProps(LPSPropTagArray lpPropTagArray, ULONG ulFlags, ULONG FAR * lpcValues, LPSPropValue FAR * lppPropArray)
{
	TRACE_MAPI(TRACE_ENTRY, "IMailUser::GetProps", "%s, flags=%08X", PropNameFromPropTagArray(lpPropTagArray).c_str(), ulFlags);
	METHOD_PROLOGUE_(ECMailUser, MailUser);
	HRESULT hr = pThis->GetProps(lpPropTagArray, ulFlags, lpcValues, lppPropArray);
	TRACE_MAPI(TRACE_RETURN, "IMailUser::GetProps", "%s, %s", GetMAPIErrorDescription(hr).c_str(), (lpcValues && lppPropArray)?PropNameFromPropArray(*lpcValues, *lppPropArray).c_str():"NULL");
	return hr;
}

HRESULT ECMailUser::xMailUser::GetPropList(ULONG ulFlags, LPSPropTagArray FAR * lppPropTagArray)
{
	TRACE_MAPI(TRACE_ENTRY, "IMailUser::GetPropList", "");
	METHOD_PROLOGUE_(ECMailUser, MailUser);
	HRESULT hr = pThis->GetPropList(ulFlags, lppPropTagArray);
	TRACE_MAPI(TRACE_RETURN, "IMailUser::GetPropList", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMailUser::xMailUser::OpenProperty(ULONG ulPropTag, LPCIID lpiid, ULONG ulInterfaceOptions, ULONG ulFlags, LPUNKNOWN FAR * lppUnk)
{
	TRACE_MAPI(TRACE_ENTRY, "IMailUser::OpenProperty", "PropTag=%s, lpiid=%s", PropNameFromPropTag(ulPropTag).c_str(), DBGGUIDToString(*lpiid).c_str());
	METHOD_PROLOGUE_(ECMailUser, MailUser);
	HRESULT hr = pThis->OpenProperty(ulPropTag, lpiid, ulInterfaceOptions, ulFlags, lppUnk);
	TRACE_MAPI(TRACE_RETURN, "IMailUser::OpenProperty", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMailUser::xMailUser::SetProps(ULONG cValues, LPSPropValue lpPropArray, LPSPropProblemArray FAR * lppProblems)
{
	TRACE_MAPI(TRACE_ENTRY, "IMailUser::SetProps", "");
	METHOD_PROLOGUE_(ECMailUser, MailUser);
	HRESULT hr = pThis->SetProps(cValues, lpPropArray, lppProblems);
	TRACE_MAPI(TRACE_RETURN, "IMailUser::SetProps", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMailUser::xMailUser::DeleteProps(LPSPropTagArray lpPropTagArray, LPSPropProblemArray FAR * lppProblems)
{
	TRACE_MAPI(TRACE_ENTRY, "IMailUser::DeleteProps", "%s", PropNameFromPropTagArray(lpPropTagArray).c_str());
	METHOD_PROLOGUE_(ECMailUser, MailUser);
	HRESULT hr = pThis->DeleteProps(lpPropTagArray, lppProblems);
	TRACE_MAPI(TRACE_RETURN, "IMailUser::DeleteProps", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMailUser::xMailUser::CopyTo(ULONG ciidExclude, LPCIID rgiidExclude, LPSPropTagArray lpExcludeProps, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, LPCIID lpInterface, LPVOID lpDestObj, ULONG ulFlags, LPSPropProblemArray FAR * lppProblems)
{
	TRACE_MAPI(TRACE_ENTRY, "IMailUser::CopyTo", "");
	METHOD_PROLOGUE_(ECMailUser, MailUser);
	HRESULT hr = pThis->CopyTo(ciidExclude, rgiidExclude, lpExcludeProps, ulUIParam, lpProgress, lpInterface, lpDestObj, ulFlags, lppProblems);;
	TRACE_MAPI(TRACE_RETURN, "IMailUser::CopyTo", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMailUser::xMailUser::CopyProps(LPSPropTagArray lpIncludeProps, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, LPCIID lpInterface, LPVOID lpDestObj, ULONG ulFlags, LPSPropProblemArray FAR * lppProblems)
{
	TRACE_MAPI(TRACE_ENTRY, "IMailUser::CopyProps", "");
	METHOD_PROLOGUE_(ECMailUser, MailUser);
	HRESULT hr = pThis->CopyProps(lpIncludeProps, ulUIParam, lpProgress, lpInterface, lpDestObj, ulFlags, lppProblems);
	TRACE_MAPI(TRACE_RETURN, "IMailUser::CopyProps", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMailUser::xMailUser::GetNamesFromIDs(LPSPropTagArray * pptaga, LPGUID lpguid, ULONG ulFlags, ULONG * pcNames, LPMAPINAMEID ** pppNames)
{
	TRACE_MAPI(TRACE_ENTRY, "IMailUser::GetNamesFromIDs", "");
	METHOD_PROLOGUE_(ECMailUser, MailUser);
	HRESULT hr = pThis->GetNamesFromIDs(pptaga, lpguid, ulFlags, pcNames, pppNames);
	TRACE_MAPI(TRACE_RETURN, "IMailUser::GetIDsFromNames", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMailUser::xMailUser::GetIDsFromNames(ULONG cNames, LPMAPINAMEID * ppNames, ULONG ulFlags, LPSPropTagArray * pptaga)
{
	TRACE_MAPI(TRACE_ENTRY, "IMailUser::GetIDsFromNames", "");
	METHOD_PROLOGUE_(ECMailUser, MailUser);
	HRESULT hr = pThis->GetIDsFromNames(cNames, ppNames, ulFlags, pptaga);
	TRACE_MAPI(TRACE_RETURN, "IMailUser::GetIDsFromNames", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}
