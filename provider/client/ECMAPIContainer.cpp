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

// ECMAPIContainer.cpp: implementation of the ECMAPIContainer class.
//
//////////////////////////////////////////////////////////////////////
#include "platform.h"
#include "Zarafa.h"
#include "ECMAPIContainer.h"

#include "ECMAPITable.h"
#include "Mem.h"

#include "ECGuid.h"
#include "ECDebug.h"


//#include <edkmdb.h>
#include <mapiext.h>
#include <mapiutil.h>

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

ECMAPIContainer::ECMAPIContainer(ECMsgStore *lpMsgStore, ULONG ulObjType, BOOL fModify, char *szClassName) : ECMAPIProp(lpMsgStore, ulObjType, fModify, NULL, szClassName)
{

}

ECMAPIContainer::~ECMAPIContainer()
{

}

HRESULT	ECMAPIContainer::QueryInterface(REFIID refiid, void **lppInterface)
{
	REGISTER_INTERFACE(IID_ECMAPIContainer, this);
	REGISTER_INTERFACE(IID_ECMAPIProp, this);
	REGISTER_INTERFACE(IID_ECUnknown, this);

	REGISTER_INTERFACE(IID_IMAPIContainer, &this->m_xMAPIContainer);
	REGISTER_INTERFACE(IID_IMAPIProp, &this->m_xMAPIContainer);
	REGISTER_INTERFACE(IID_IUnknown, &this->m_xMAPIContainer);

	return MAPI_E_INTERFACE_NOT_SUPPORTED;
}

HRESULT ECMAPIContainer::CopyTo(ULONG ciidExclude, LPCIID rgiidExclude, LPSPropTagArray lpExcludeProps, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, LPCIID lpInterface, LPVOID lpDestObj, ULONG ulFlags, LPSPropProblemArray FAR * lppProblems)
{
	HRESULT hr = hrSuccess;
	
	hr = Util::DoCopyTo(&IID_IMAPIContainer, &this->m_xMAPIContainer, ciidExclude, rgiidExclude, lpExcludeProps, ulUIParam, lpProgress, lpInterface, lpDestObj, ulFlags, lppProblems);

	return hr;
}

HRESULT ECMAPIContainer::CopyProps(LPSPropTagArray lpIncludeProps, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, LPCIID lpInterface, LPVOID lpDestObj, ULONG ulFlags, LPSPropProblemArray FAR * lppProblems)
{
	HRESULT hr = hrSuccess;

	hr = Util::DoCopyProps(&IID_IMAPIContainer, &this->m_xMAPIContainer, lpIncludeProps, ulUIParam, lpProgress, lpInterface, lpDestObj, ulFlags, lppProblems);

	return hr;
}

HRESULT ECMAPIContainer::SetSearchCriteria(LPSRestriction lpRestriction, LPENTRYLIST lpContainerList, ULONG ulSearchFlags)
{
	HRESULT hr = hrSuccess;

	hr = MAPI_E_NO_SUPPORT;

	return hr;
}

HRESULT ECMAPIContainer::GetSearchCriteria(ULONG ulFlags, LPSRestriction *lppRestriction, LPENTRYLIST *lppContainerList, ULONG *lpulSearchState)
{
	HRESULT hr = hrSuccess;

	hr = MAPI_E_NO_SUPPORT;

	return hr;
}

HRESULT ECMAPIContainer::GetContentsTable(ULONG ulFlags, LPMAPITABLE *lppTable)
{
	HRESULT			hr = hrSuccess;
	ECMAPITable*	lpTable = NULL;
	WSTableView*	lpTableOps = NULL;
	std::string		strName = "Contents table";

#ifdef DEBUG
	{
		LPSPropValue lpDisplay;
		HrGetOneProp(&this->m_xMAPIProp, PR_DISPLAY_NAME_A, &lpDisplay);

		if(lpDisplay) {
			strName = lpDisplay->Value.lpszA;
		}
	}
#endif

	hr = ECMAPITable::Create(strName.c_str(), this->GetMsgStore()->m_lpNotifyClient, 0, &lpTable);

	if(hr != hrSuccess)
		goto exit;


	hr = this->GetMsgStore()->lpTransport->HrOpenTableOps(MAPI_MESSAGE, (ulFlags&(MAPI_UNICODE|SHOW_SOFT_DELETES|MAPI_ASSOCIATED|EC_TABLE_NOCAP)), m_cbEntryId, m_lpEntryId, this->GetMsgStore(), &lpTableOps);

	if(hr != hrSuccess)
		goto exit;

	hr = lpTable->HrSetTableOps(lpTableOps, !(ulFlags & MAPI_DEFERRED_ERRORS));

	if(hr != hrSuccess)
		goto exit;

	hr = lpTable->QueryInterface(IID_IMAPITable, (void **)lppTable);

	AddChild(lpTable);

exit:
	if(lpTable)
		lpTable->Release();

	if(lpTableOps)
		lpTableOps->Release();

	return hr;
}


HRESULT ECMAPIContainer::GetHierarchyTable(ULONG ulFlags, LPMAPITABLE *lppTable)
{
	HRESULT			hr = hrSuccess;
	ECMAPITable*	lpTable = NULL;
	WSTableView*	lpTableOps = NULL;
	SPropTagArray	sPropTagArray;
	ULONG			cValues = 0;
	LPSPropValue	lpPropArray = NULL; 
	std::string		strName = "Hierarchy table";
	
#ifdef DEBUG
	{
		LPSPropValue lpDisplay;
		HrGetOneProp(&this->m_xMAPIProp, PR_DISPLAY_NAME_A, &lpDisplay);

		if(lpDisplay) {
			strName = lpDisplay->Value.lpszA;
		}
	}
#endif

	sPropTagArray.aulPropTag[0] = PR_FOLDER_TYPE;
	sPropTagArray.cValues = 1;

	hr = GetProps(&sPropTagArray, 0, &cValues, &lpPropArray);
	if(FAILED(hr))
		goto exit;
	
	// block for searchfolders
	if(lpPropArray && lpPropArray[0].ulPropTag == PR_FOLDER_TYPE && lpPropArray[0].Value.l == FOLDER_SEARCH)
	{		
		hr= MAPI_E_NO_SUPPORT;
		goto exit;
	}

	hr = ECMAPITable::Create(strName.c_str(), this->GetMsgStore()->m_lpNotifyClient, 0, &lpTable);

	if(hr != hrSuccess)
		goto exit;

	hr = this->GetMsgStore()->lpTransport->HrOpenTableOps(MAPI_FOLDER, ulFlags & (MAPI_UNICODE | SHOW_SOFT_DELETES | CONVENIENT_DEPTH), m_cbEntryId, m_lpEntryId, this->GetMsgStore(), &lpTableOps);

	if(hr != hrSuccess)
		goto exit;

	hr = lpTable->HrSetTableOps(lpTableOps, !(ulFlags & MAPI_DEFERRED_ERRORS));

	if(hr != hrSuccess)
		goto exit;

	hr = lpTable->QueryInterface(IID_IMAPITable, (void **)lppTable);

	AddChild(lpTable);

exit:
	if(lpPropArray)
		ECFreeBuffer(lpPropArray);

	if(lpTable)
		lpTable->Release();

	if(lpTableOps)
		lpTableOps->Release();

	return hr;
}

HRESULT ECMAPIContainer::OpenEntry(ULONG cbEntryID, LPENTRYID lpEntryID, LPCIID lpInterface, ULONG ulFlags, ULONG *lpulObjType, LPUNKNOWN *lppUnk)
{
	HRESULT hr = hrSuccess;
	
	hr = this->GetMsgStore()->OpenEntry(cbEntryID, lpEntryID, lpInterface, ulFlags, lpulObjType, lppUnk);

	return hr;
}


// From IMAPIContainer
HRESULT ECMAPIContainer::xMAPIContainer::GetContentsTable(ULONG ulFlags, LPMAPITABLE *lppTable)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPIContainer::GetContentsTable", "");
	METHOD_PROLOGUE_(ECMAPIContainer, MAPIContainer);
	HRESULT hr = pThis->GetContentsTable(ulFlags, lppTable);
	TRACE_MAPI(TRACE_RETURN, "IMAPIContainer::GetContentsTable", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMAPIContainer::xMAPIContainer::GetHierarchyTable(ULONG ulFlags, LPMAPITABLE *lppTable)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPIContainer::GetHierarchyTable", ""); 
	METHOD_PROLOGUE_(ECMAPIContainer, MAPIContainer);
	HRESULT hr = pThis->GetHierarchyTable(ulFlags, lppTable);
	TRACE_MAPI(TRACE_RETURN, "IMAPIContainer::GetHierarchyTable", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMAPIContainer::xMAPIContainer::OpenEntry(ULONG cbEntryID, LPENTRYID lpEntryID, LPCIID lpInterface, ULONG ulFlags, ULONG *lpulObjType, LPUNKNOWN *lppUnk)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPIContainer::OpenEntry", "interface=%s", (lpInterface)?DBGGUIDToString(*lpInterface).c_str():"NULL");
	METHOD_PROLOGUE_(ECMAPIContainer, MAPIContainer);
	HRESULT hr = pThis->OpenEntry(cbEntryID, lpEntryID, lpInterface, ulFlags, lpulObjType, lppUnk);
	TRACE_MAPI(TRACE_RETURN, "IMAPIContainer::OpenEntry", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMAPIContainer::xMAPIContainer::SetSearchCriteria(LPSRestriction lpRestriction, LPENTRYLIST lpContainerList, ULONG ulSearchFlags)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPIContainer::SetSearchCriteria", "%s \nulSearchFlags=0x%08X", (lpRestriction)?RestrictionToString(lpRestriction).c_str():"NULL", ulSearchFlags);
	METHOD_PROLOGUE_(ECMAPIContainer, MAPIContainer);
	HRESULT hr = pThis->SetSearchCriteria(lpRestriction, lpContainerList, ulSearchFlags);
	TRACE_MAPI(TRACE_RETURN, "IMAPIContainer::SetSearchCriteria", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMAPIContainer::xMAPIContainer::GetSearchCriteria(ULONG ulFlags, LPSRestriction *lppRestriction, LPENTRYLIST *lppContainerList, ULONG *lpulSearchState)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPIContainer::GetSearchCriteria", "");
	METHOD_PROLOGUE_(ECMAPIContainer, MAPIContainer);
	HRESULT hr =pThis->GetSearchCriteria(ulFlags, lppRestriction, lppContainerList, lpulSearchState);;
	TRACE_MAPI(TRACE_RETURN, "IMAPIContainer::GetSearchCriteria", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

// From IUnknown
HRESULT ECMAPIContainer::xMAPIContainer::QueryInterface(REFIID refiid , void** lppInterface)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPIContainer::QueryInterface", "%s", DBGGUIDToString(refiid).c_str());
	METHOD_PROLOGUE_(ECMAPIContainer, MAPIContainer);
	HRESULT hr = pThis->QueryInterface(refiid, lppInterface);
	TRACE_MAPI(TRACE_RETURN, "IMAPIContainer::QueryInterface", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

ULONG ECMAPIContainer::xMAPIContainer::AddRef()
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPIContainer::AddRef", "");
	METHOD_PROLOGUE_(ECMAPIContainer, MAPIContainer);
	return pThis->AddRef();
}

ULONG ECMAPIContainer::xMAPIContainer::Release()
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPIContainer::Release", "");
	METHOD_PROLOGUE_(ECMAPIContainer, MAPIContainer);
	return pThis->Release();
}

// From IMAPIProp
HRESULT ECMAPIContainer::xMAPIContainer::GetLastError(HRESULT hError, ULONG ulFlags, LPMAPIERROR * lppMapiError)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPIContainer::GetLastError", "");
	METHOD_PROLOGUE_(ECMAPIContainer, MAPIContainer);
	HRESULT hr = pThis->GetLastError(hError, ulFlags, lppMapiError);
	TRACE_MAPI(TRACE_RETURN, "IMAPIContainer::GetLastError", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMAPIContainer::xMAPIContainer::SaveChanges(ULONG ulFlags)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPIContainer::SaveChanges", "");
	METHOD_PROLOGUE_(ECMAPIContainer, MAPIContainer);
	HRESULT hr = pThis->SaveChanges(ulFlags);
	TRACE_MAPI(TRACE_RETURN, "IMAPIContainer::SaveChanges", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMAPIContainer::xMAPIContainer::GetProps(LPSPropTagArray lpPropTagArray, ULONG ulFlags, ULONG FAR * lpcValues, LPSPropValue FAR * lppPropArray)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPIContainer::GetProps", "%s", PropNameFromPropTagArray(lpPropTagArray).c_str());
	METHOD_PROLOGUE_(ECMAPIContainer, MAPIContainer);
	HRESULT hr = pThis->GetProps(lpPropTagArray, ulFlags, lpcValues, lppPropArray);
	TRACE_MAPI(TRACE_RETURN, "IMAPIContainer::GetProps", "%s\n%s", GetMAPIErrorDescription(hr).c_str(), PropNameFromPropArray(*lpcValues, *lppPropArray).c_str());
	return hr;
}

HRESULT ECMAPIContainer::xMAPIContainer::GetPropList(ULONG ulFlags, LPSPropTagArray FAR * lppPropTagArray)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPIContainer::GetPropList", "");
	METHOD_PROLOGUE_(ECMAPIContainer, MAPIContainer);
	HRESULT hr = pThis->GetPropList(ulFlags, lppPropTagArray);
	TRACE_MAPI(TRACE_RETURN, "IMAPIContainer::GetPropList", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMAPIContainer::xMAPIContainer::OpenProperty(ULONG ulPropTag, LPCIID lpiid, ULONG ulInterfaceOptions, ULONG ulFlags, LPUNKNOWN FAR * lppUnk)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPIContainer::OpenProperty", "PropTag=%s, lpiid=%s", PropNameFromPropTag(ulPropTag).c_str(), (lpiid)?DBGGUIDToString(*lpiid).c_str():"NULL");
	METHOD_PROLOGUE_(ECMAPIContainer, MAPIContainer);
	HRESULT hr = pThis->OpenProperty(ulPropTag, lpiid, ulInterfaceOptions, ulFlags, lppUnk);
	TRACE_MAPI(TRACE_RETURN, "IMAPIContainer::OpenProperty", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMAPIContainer::xMAPIContainer::SetProps(ULONG cValues, LPSPropValue lpPropArray, LPSPropProblemArray FAR * lppProblems)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPIContainer::SetProps", "%s", PropNameFromPropArray(cValues, lpPropArray).c_str());
	METHOD_PROLOGUE_(ECMAPIContainer, MAPIContainer);
	HRESULT hr = pThis->SetProps(cValues, lpPropArray, lppProblems);
	TRACE_MAPI(TRACE_RETURN, "IMAPIContainer::SetProps", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMAPIContainer::xMAPIContainer::DeleteProps(LPSPropTagArray lpPropTagArray, LPSPropProblemArray FAR * lppProblems)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPIContainer::DeleteProps", "%s", PropNameFromPropTagArray(lpPropTagArray).c_str());
	METHOD_PROLOGUE_(ECMAPIContainer, MAPIContainer);
	HRESULT hr = pThis->DeleteProps(lpPropTagArray, lppProblems);
	TRACE_MAPI(TRACE_RETURN, "IMAPIContainer::DeleteProps", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMAPIContainer::xMAPIContainer::CopyTo(ULONG ciidExclude, LPCIID rgiidExclude, LPSPropTagArray lpExcludeProps, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, LPCIID lpInterface, LPVOID lpDestObj, ULONG ulFlags, LPSPropProblemArray FAR * lppProblems)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPIContainer::CopyTo", "");
	METHOD_PROLOGUE_(ECMAPIContainer, MAPIContainer);
	HRESULT hr = pThis->CopyTo(ciidExclude, rgiidExclude, lpExcludeProps, ulUIParam, lpProgress, lpInterface, lpDestObj, ulFlags, lppProblems);;
	TRACE_MAPI(TRACE_RETURN, "IMAPIContainer::CopyTo", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMAPIContainer::xMAPIContainer::CopyProps(LPSPropTagArray lpIncludeProps, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, LPCIID lpInterface, LPVOID lpDestObj, ULONG ulFlags, LPSPropProblemArray FAR * lppProblems)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPIContainer::CopyProps", "");
	METHOD_PROLOGUE_(ECMAPIContainer, MAPIContainer);
	HRESULT hr = pThis->CopyProps(lpIncludeProps, ulUIParam, lpProgress, lpInterface, lpDestObj, ulFlags, lppProblems);
	TRACE_MAPI(TRACE_RETURN, "IMAPIContainer::CopyProps", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMAPIContainer::xMAPIContainer::GetNamesFromIDs(LPSPropTagArray * pptaga, LPGUID lpguid, ULONG ulFlags, ULONG * pcNames, LPMAPINAMEID ** pppNames)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPIContainer::GetNamesFromIDs", "");
	METHOD_PROLOGUE_(ECMAPIContainer, MAPIContainer);
	HRESULT hr = pThis->GetNamesFromIDs(pptaga, lpguid, ulFlags, pcNames, pppNames);
	TRACE_MAPI(TRACE_RETURN, "IMAPIContainer::GetIDsFromNames", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMAPIContainer::xMAPIContainer::GetIDsFromNames(ULONG cNames, LPMAPINAMEID * ppNames, ULONG ulFlags, LPSPropTagArray * pptaga)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPIContainer::GetIDsFromNames", "");
	METHOD_PROLOGUE_(ECMAPIContainer, MAPIContainer);
	HRESULT hr = pThis->GetIDsFromNames(cNames, ppNames, ulFlags, pptaga);
	TRACE_MAPI(TRACE_RETURN, "IMAPIContainer::GetIDsFromNames", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}
