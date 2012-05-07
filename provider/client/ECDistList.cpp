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

#include "Zarafa.h"
#include "ECDistList.h"

#include "Mem.h"

#include "ECGuid.h"
#include "CommonUtil.h"
#include "ECDebug.h"


#include "ECMAPITable.h"

#include "ECDisplayTable.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

ECDistList::ECDistList(void* lpProvider, BOOL fModify) : ECABContainer(lpProvider, MAPI_DISTLIST, fModify, "IDistList")
{

}

ECDistList::~ECDistList()
{

}

HRESULT	ECDistList::QueryInterface(REFIID refiid, void **lppInterface) 
{
	REGISTER_INTERFACE(IID_ECDistList, this);
	REGISTER_INTERFACE(IID_ECABContainer, this);
	REGISTER_INTERFACE(IID_ECABProp, this);
	REGISTER_INTERFACE(IID_ECUnknown, this);

	REGISTER_INTERFACE(IID_IDistList, &this->m_xDistList);
	REGISTER_INTERFACE(IID_IABContainer, &this->m_xDistList);
	REGISTER_INTERFACE(IID_IMAPIProp, &this->m_xDistList);
	REGISTER_INTERFACE(IID_IUnknown, &this->m_xDistList);

	return MAPI_E_INTERFACE_NOT_SUPPORTED;
}

HRESULT ECDistList::Create(void* lpProvider, BOOL fModify, ECDistList** lppDistList)
{

	HRESULT hr = hrSuccess;
	ECDistList *lpDistList = NULL;

	lpDistList = new ECDistList(lpProvider, fModify);

	hr = lpDistList->QueryInterface(IID_ECDistList, (void **)lppDistList);

	if(hr != hrSuccess)
		delete lpDistList;

	return hr;
}

HRESULT ECDistList::TableRowGetProp(void* lpProvider, struct propVal *lpsPropValSrc, LPSPropValue lpsPropValDst, void **lpBase, ULONG ulType)
{
	HRESULT hr = hrSuccess;

	hr = MAPI_E_NOT_FOUND;

	return hr;
}

HRESULT ECDistList::OpenProperty(ULONG ulPropTag, LPCIID lpiid, ULONG ulInterfaceOptions, ULONG ulFlags, LPUNKNOWN FAR * lppUnk)
{
	HRESULT hr = MAPI_E_NOT_FOUND;

	if (lpiid == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	// FIXME: check variables

	switch(ulPropTag) {
		default:
			hr = ECABProp::OpenProperty(ulPropTag, lpiid, ulInterfaceOptions, ulFlags, lppUnk);
			break;
	}//switch(ulPropTag)
	
exit:
	return hr;
}

HRESULT ECDistList::CopyTo(ULONG ciidExclude, LPCIID rgiidExclude, LPSPropTagArray lpExcludeProps, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, LPCIID lpInterface, LPVOID lpDestObj, ULONG ulFlags, LPSPropProblemArray FAR * lppProblems)
{
	HRESULT hr = hrSuccess;
	
	hr = this->GetABStore()->m_lpMAPISup->DoCopyTo(&IID_IDistList, &this->m_xDistList, ciidExclude, rgiidExclude, lpExcludeProps, ulUIParam, lpProgress, lpInterface, lpDestObj, ulFlags, lppProblems);

	return hr;
}

HRESULT ECDistList::CopyProps(LPSPropTagArray lpIncludeProps, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, LPCIID lpInterface, LPVOID lpDestObj, ULONG ulFlags, LPSPropProblemArray FAR * lppProblems)
{
	HRESULT hr = hrSuccess;

	hr = this->GetABStore()->m_lpMAPISup->DoCopyProps(&IID_IDistList, &this->m_xDistList, lpIncludeProps, ulUIParam, lpProgress, lpInterface, lpDestObj, ulFlags, lppProblems);

	return hr;
}

////////////////////////////////////////////
// Interface IUnknown
//

HRESULT ECDistList::xDistList::QueryInterface(REFIID refiid , void** lppInterface)
{
	TRACE_MAPI(TRACE_ENTRY, "IDistList::QueryInterface", "%s", DBGGUIDToString(refiid).c_str());
	METHOD_PROLOGUE_(ECDistList, DistList);
	HRESULT hr = pThis->QueryInterface(refiid, lppInterface);
	TRACE_MAPI(TRACE_RETURN, "IDistList::QueryInterface", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

ULONG ECDistList::xDistList::AddRef()
{
	TRACE_MAPI(TRACE_ENTRY, "IDistList::AddRef", "");
	METHOD_PROLOGUE_(ECDistList, DistList);
	return pThis->AddRef();
}

ULONG ECDistList::xDistList::Release()
{
	TRACE_MAPI(TRACE_ENTRY, "IDistList::Release", "");
	METHOD_PROLOGUE_(ECDistList, DistList);
	return pThis->Release();
}

////////////////////////////////////////////
// Interface IABContainer
//

HRESULT ECDistList::xDistList::CreateEntry(ULONG cbEntryID, LPENTRYID lpEntryID, ULONG ulCreateFlags, LPMAPIPROP* lppMAPIPropEntry)
{
	TRACE_MAPI(TRACE_ENTRY, "IDistList::CreateEntry", "");
	METHOD_PROLOGUE_(ECDistList, DistList);
	HRESULT hr = pThis->CreateEntry(cbEntryID, lpEntryID, ulCreateFlags, lppMAPIPropEntry);
	TRACE_MAPI(TRACE_RETURN, "IDistList::CreateEntry", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECDistList::xDistList::CopyEntries(LPENTRYLIST lpEntries, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, ULONG ulFlags)
{
	TRACE_MAPI(TRACE_ENTRY, "IDistList::CopyEntries", "");
	METHOD_PROLOGUE_(ECDistList, DistList);
	HRESULT hr = pThis->CopyEntries(lpEntries, ulUIParam, lpProgress, ulFlags);
	TRACE_MAPI(TRACE_RETURN, "IDistList::CopyEntries", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECDistList::xDistList::DeleteEntries(LPENTRYLIST lpEntries, ULONG ulFlags)
{
	TRACE_MAPI(TRACE_ENTRY, "IDistList::DeleteEntries", "");
	METHOD_PROLOGUE_(ECDistList, DistList);
	HRESULT hr = pThis->DeleteEntries(lpEntries, ulFlags);
	TRACE_MAPI(TRACE_RETURN, "IDistList::DeleteEntries", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECDistList::xDistList::ResolveNames(LPSPropTagArray lpPropTagArray, ULONG ulFlags, LPADRLIST lpAdrList, LPFlagList lpFlagList)
{
	TRACE_MAPI(TRACE_ENTRY, "IDistList::ResolveNames", "");
	METHOD_PROLOGUE_(ECDistList, DistList);
	HRESULT hr = pThis->ResolveNames(lpPropTagArray, ulFlags, lpAdrList, lpFlagList);
	TRACE_MAPI(TRACE_RETURN, "IDistList::ResolveNames", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

////////////////////////////////////////////
// Interface IMAPIContainer
//
HRESULT ECDistList::xDistList::GetContentsTable(ULONG ulFlags, LPMAPITABLE *lppTable)
{
	TRACE_MAPI(TRACE_ENTRY, "IDistList::GetContentsTable", "");
	METHOD_PROLOGUE_(ECDistList, DistList);
	HRESULT hr = pThis->GetContentsTable(ulFlags, lppTable);
	TRACE_MAPI(TRACE_RETURN, "IDistList::GetContentsTable", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECDistList::xDistList::GetHierarchyTable(ULONG ulFlags, LPMAPITABLE *lppTable)
{
	TRACE_MAPI(TRACE_ENTRY, "IDistList::GetHierarchyTable", ""); 
	METHOD_PROLOGUE_(ECDistList, DistList);
	HRESULT hr = pThis->GetHierarchyTable(ulFlags, lppTable);
	TRACE_MAPI(TRACE_RETURN, "IDistList::GetHierarchyTable", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECDistList::xDistList::OpenEntry(ULONG cbEntryID, LPENTRYID lpEntryID, LPCIID lpInterface, ULONG ulFlags, ULONG *lpulObjType, LPUNKNOWN *lppUnk)
{
	TRACE_MAPI(TRACE_ENTRY, "IDistList::OpenEntry", "interface=%s",(lpInterface)?DBGGUIDToString(*lpInterface).c_str():"NULL");
	METHOD_PROLOGUE_(ECDistList, DistList);
	HRESULT hr = pThis->OpenEntry(cbEntryID, lpEntryID, lpInterface, ulFlags, lpulObjType, lppUnk);
	TRACE_MAPI(TRACE_RETURN, "IDistList::OpenEntry", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECDistList::xDistList::SetSearchCriteria(LPSRestriction lpRestriction, LPENTRYLIST lpContainerList, ULONG ulSearchFlags)
{
	TRACE_MAPI(TRACE_ENTRY, "IDistList::SetSearchCriteria", "");
	METHOD_PROLOGUE_(ECDistList, DistList);
	HRESULT hr = pThis->SetSearchCriteria(lpRestriction, lpContainerList, ulSearchFlags);
	TRACE_MAPI(TRACE_RETURN, "IDistList::SetSearchCriteria", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECDistList::xDistList::GetSearchCriteria(ULONG ulFlags, LPSRestriction *lppRestriction, LPENTRYLIST *lppContainerList, ULONG *lpulSearchState)
{
	TRACE_MAPI(TRACE_ENTRY, "IDistList::GetSearchCriteria", "");
	METHOD_PROLOGUE_(ECDistList, DistList);
	HRESULT hr =pThis->GetSearchCriteria(ulFlags, lppRestriction, lppContainerList, lpulSearchState);;
	TRACE_MAPI(TRACE_RETURN, "IDistList::GetSearchCriteria", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

////////////////////////////////////////////
// Interface IMAPIProp
//
HRESULT ECDistList::xDistList::GetLastError(HRESULT hError, ULONG ulFlags, LPMAPIERROR * lppMapiError)
{
	TRACE_MAPI(TRACE_ENTRY, "IDistList::GetLastError", "");
	METHOD_PROLOGUE_(ECDistList, DistList);
	HRESULT hr = pThis->GetLastError(hError, ulFlags, lppMapiError);
	TRACE_MAPI(TRACE_RETURN, "IDistList::GetLastError", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECDistList::xDistList::SaveChanges(ULONG ulFlags)
{
	TRACE_MAPI(TRACE_ENTRY, "IDistList::SaveChanges", "");
	METHOD_PROLOGUE_(ECDistList, DistList);
	HRESULT hr = pThis->SaveChanges(ulFlags);
	TRACE_MAPI(TRACE_RETURN, "IDistList::SaveChanges", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECDistList::xDistList::GetProps(LPSPropTagArray lpPropTagArray, ULONG ulFlags, ULONG FAR * lpcValues, LPSPropValue FAR * lppPropArray)
{
	TRACE_MAPI(TRACE_ENTRY, "IDistList::GetProps", "%s", PropNameFromPropTagArray(lpPropTagArray).c_str());
	METHOD_PROLOGUE_(ECDistList, DistList);
	HRESULT hr = pThis->GetProps(lpPropTagArray, ulFlags, lpcValues, lppPropArray);
	TRACE_MAPI(TRACE_RETURN, "IDistList::GetProps", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECDistList::xDistList::GetPropList(ULONG ulFlags, LPSPropTagArray FAR * lppPropTagArray)
{
	TRACE_MAPI(TRACE_ENTRY, "IDistList::GetPropList", "");
	METHOD_PROLOGUE_(ECDistList, DistList);
	HRESULT hr = pThis->GetPropList(ulFlags, lppPropTagArray);
	TRACE_MAPI(TRACE_RETURN, "IDistList::GetPropList", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECDistList::xDistList::OpenProperty(ULONG ulPropTag, LPCIID lpiid, ULONG ulInterfaceOptions, ULONG ulFlags, LPUNKNOWN FAR * lppUnk)
{
	TRACE_MAPI(TRACE_ENTRY, "IDistList::OpenProperty", "PropTag=%s, lpiid=%s", PropNameFromPropTag(ulPropTag).c_str(), DBGGUIDToString(*lpiid).c_str());
	METHOD_PROLOGUE_(ECDistList, DistList);
	HRESULT hr = pThis->OpenProperty(ulPropTag, lpiid, ulInterfaceOptions, ulFlags, lppUnk);
	TRACE_MAPI(TRACE_RETURN, "IDistList::OpenProperty", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECDistList::xDistList::SetProps(ULONG cValues, LPSPropValue lpPropArray, LPSPropProblemArray FAR * lppProblems)
{
	TRACE_MAPI(TRACE_ENTRY, "IDistList::SetProps", "");
	METHOD_PROLOGUE_(ECDistList, DistList);
	HRESULT hr = pThis->SetProps(cValues, lpPropArray, lppProblems);
	TRACE_MAPI(TRACE_RETURN, "IDistList::SetProps", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECDistList::xDistList::DeleteProps(LPSPropTagArray lpPropTagArray, LPSPropProblemArray FAR * lppProblems)
{
	TRACE_MAPI(TRACE_ENTRY, "IDistList::DeleteProps", "%s", PropNameFromPropTagArray(lpPropTagArray).c_str());
	METHOD_PROLOGUE_(ECDistList, DistList);
	HRESULT hr = pThis->DeleteProps(lpPropTagArray, lppProblems);
	TRACE_MAPI(TRACE_RETURN, "IDistList::DeleteProps", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECDistList::xDistList::CopyTo(ULONG ciidExclude, LPCIID rgiidExclude, LPSPropTagArray lpExcludeProps, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, LPCIID lpInterface, LPVOID lpDestObj, ULONG ulFlags, LPSPropProblemArray FAR * lppProblems)
{
	TRACE_MAPI(TRACE_ENTRY, "IDistList::CopyTo", "");
	METHOD_PROLOGUE_(ECDistList, DistList);
	HRESULT hr = pThis->CopyTo(ciidExclude, rgiidExclude, lpExcludeProps, ulUIParam, lpProgress, lpInterface, lpDestObj, ulFlags, lppProblems);;
	TRACE_MAPI(TRACE_RETURN, "IDistList::CopyTo", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECDistList::xDistList::CopyProps(LPSPropTagArray lpIncludeProps, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, LPCIID lpInterface, LPVOID lpDestObj, ULONG ulFlags, LPSPropProblemArray FAR * lppProblems)
{
	TRACE_MAPI(TRACE_ENTRY, "IDistList::CopyProps", "");
	METHOD_PROLOGUE_(ECDistList, DistList);
	HRESULT hr = pThis->CopyProps(lpIncludeProps, ulUIParam, lpProgress, lpInterface, lpDestObj, ulFlags, lppProblems);
	TRACE_MAPI(TRACE_RETURN, "IDistList::CopyProps", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECDistList::xDistList::GetNamesFromIDs(LPSPropTagArray * pptaga, LPGUID lpguid, ULONG ulFlags, ULONG * pcNames, LPMAPINAMEID ** pppNames)
{
	TRACE_MAPI(TRACE_ENTRY, "IDistList::GetNamesFromIDs", "");
	METHOD_PROLOGUE_(ECDistList, DistList);
	HRESULT hr = pThis->GetNamesFromIDs(pptaga, lpguid, ulFlags, pcNames, pppNames);
	TRACE_MAPI(TRACE_RETURN, "IDistList::GetIDsFromNames", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECDistList::xDistList::GetIDsFromNames(ULONG cNames, LPMAPINAMEID * ppNames, ULONG ulFlags, LPSPropTagArray * pptaga)
{
	TRACE_MAPI(TRACE_ENTRY, "IDistList::GetIDsFromNames", "");
	METHOD_PROLOGUE_(ECDistList, DistList);
	HRESULT hr = pThis->GetIDsFromNames(cNames, ppNames, ulFlags, pptaga);
	TRACE_MAPI(TRACE_RETURN, "IDistList::GetIDsFromNames", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}
