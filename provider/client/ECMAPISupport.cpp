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
#include "ECGuid.h"
#include "ECMAPISupport.h"

#include "ECDebug.h"


#include "ECProfSect.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

ECMAPISupport::ECMAPISupport(LPMAPISUP pMAPISupObj, LPPROFSECT lpProfSect)
{
	m_lpMAPISupObj = pMAPISupObj;

	//Wrap iProfSec
/*	
	ECProfSect* lpProfSectWrap = NULL;

	m_lpProfSect = NULL;

	if(lpProfSect)
	{
		ECProfSect::Create(lpProfSect, &lpProfSectWrap);
		lpProfSectWrap->QueryInterface(IID_IProfSect, (void **)&m_lpProfSect);
	
		lpProfSectWrap->Release();
	}
*/	

	m_lpMAPISupObj->AddRef();
}

ECMAPISupport::~ECMAPISupport(void)
{
	if(m_lpMAPISupObj)
		m_lpMAPISupObj->Release();
}

HRESULT	ECMAPISupport::Create(LPMAPISUP	pMAPISupObj, LPPROFSECT lpProfSect, ECMAPISupport **lppMAPISupport)
{
	HRESULT hr = hrSuccess;
	ECMAPISupport *lpMapiSupport = NULL;

	if(pMAPISupObj == NULL){
		hr = MAPI_E_INVALID_OBJECT;
		goto exit;
	}

	lpMapiSupport = new ECMAPISupport(pMAPISupObj, lpProfSect);

	hr = lpMapiSupport->QueryInterface(IID_ECMAPISupport, (void **)lppMAPISupport);

exit:
	return hr;
}

HRESULT	ECMAPISupport::QueryInterface(REFIID refiid, void **lppInterface)
{
	REGISTER_INTERFACE(IID_ECMAPISupport, this);
	REGISTER_INTERFACE(IID_ECUnknown, this);

	REGISTER_INTERFACE(IID_IMAPISup, &this->m_xMAPISupport);
	REGISTER_INTERFACE(IID_IUnknown, &this->m_xMAPISupport);

	return MAPI_E_INTERFACE_NOT_SUPPORTED;
}

HRESULT	ECMAPISupport::GetLastError(HRESULT hResult, ULONG ulFlags, LPMAPIERROR* lppMAPIError)
{
	return m_lpMAPISupObj->GetLastError(hResult, ulFlags, lppMAPIError);
}

HRESULT	ECMAPISupport::GetMemAllocRoutines(LPALLOCATEBUFFER* lppAllocateBuffer, LPALLOCATEMORE* lppAllocateMore, LPFREEBUFFER* lppFreeBuffer)
{
	return m_lpMAPISupObj->GetMemAllocRoutines(lppAllocateBuffer, lppAllocateMore, lppFreeBuffer);
}

HRESULT	ECMAPISupport::Subscribe(LPNOTIFKEY lpKey, ULONG ulEventMask, ULONG ulFlags, LPMAPIADVISESINK lpAdviseSink, ULONG* lpulConnection)
{
	return m_lpMAPISupObj->Subscribe(lpKey, ulEventMask, ulFlags, lpAdviseSink, lpulConnection);
}

HRESULT	ECMAPISupport::Unsubscribe(ULONG ulConnection)
{
	return m_lpMAPISupObj->Unsubscribe(ulConnection);
}

HRESULT	ECMAPISupport::Notify(LPNOTIFKEY lpKey, ULONG cNotification, LPNOTIFICATION lpNotifications, ULONG* lpulFlags)
{
	return m_lpMAPISupObj->Notify(lpKey, cNotification, lpNotifications, lpulFlags);
}

HRESULT	ECMAPISupport::ModifyStatusRow(ULONG cValues, LPSPropValue lpColumnVals, ULONG ulFlags)
{
	return m_lpMAPISupObj->ModifyStatusRow(cValues, lpColumnVals, ulFlags);
}
// FIXME: Use this function to block profile section / or remove he whole code
HRESULT	ECMAPISupport::OpenProfileSection(LPMAPIUID lpUid, ULONG ulFlags, LPPROFSECT* lppProfileObj)
{
	/*if (lpUid && 
		(IsEqualMAPIUID(lpUid, (void *)&pbNSTGlobalProfileSectionGuid)
		//|| IsEqualMAPIUID(lpUid, (void *)&pbGlobalProfileSectionGuid)
		)&& 
		m_lpProfSect)
	{	 
		// Allow the opening of the Global Section
		if (m_lpProfSect)
		{
			*lppProfileObj = m_lpProfSect;
			(*lppProfileObj)->AddRef();
			return S_OK;
		}
	}else*/
	{
		HRESULT hr = S_OK;
		//LPPROFSECT lpProfileObj = NULL;
		hr = m_lpMAPISupObj->OpenProfileSection(lpUid, ulFlags, lppProfileObj);
		/*if(hr == 0)
		{
			ECProfSect* lpProfSectWrap = NULL;

			ECProfSect::Create(lpProfileObj, &lpProfSectWrap);
			lpProfSectWrap->QueryInterface(IID_IProfSect, (void **)lppProfileObj);
		
			lpProfSectWrap->Release();
			lpProfileObj->Release();
		}*/

		return hr;
	}
}

HRESULT	ECMAPISupport::RegisterPreprocessor(LPMAPIUID lpMuid, LPTSTR lpszAdrType, LPTSTR lpszDLLName, LPSTR lpszPreprocess, LPSTR lpszRemovePreprocessInfo, ULONG ulFlags)
{
	return m_lpMAPISupObj->RegisterPreprocessor(lpMuid, lpszAdrType, lpszDLLName, lpszPreprocess, lpszRemovePreprocessInfo, ulFlags);
}

HRESULT	ECMAPISupport::NewUID(LPMAPIUID lpMuid)
{
	return m_lpMAPISupObj->NewUID(lpMuid);
}

HRESULT	ECMAPISupport::MakeInvalid(ULONG ulFlags, LPVOID lpObject, ULONG ulRefCount, ULONG cMethods)
{
	return m_lpMAPISupObj->MakeInvalid(ulFlags, lpObject, ulRefCount, cMethods);
}

HRESULT	ECMAPISupport::SpoolerYield(ULONG ulFlags)
{
	return m_lpMAPISupObj->SpoolerYield(ulFlags);
}

HRESULT	ECMAPISupport::SpoolerNotify(ULONG ulFlags, LPVOID lpvData)
{
	return m_lpMAPISupObj->SpoolerNotify(ulFlags, lpvData);
}

HRESULT	ECMAPISupport::CreateOneOff(LPTSTR lpszName, LPTSTR lpszAdrType, LPTSTR lpszAddress, ULONG ulFlags, ULONG* lpcbEntryID, LPENTRYID* lppEntryID)
{
	return m_lpMAPISupObj->CreateOneOff(lpszName, lpszAdrType, lpszAddress, ulFlags, lpcbEntryID, lppEntryID);
}

HRESULT	ECMAPISupport::SetProviderUID(LPMAPIUID lpProviderID, ULONG ulFlags)
{
	return m_lpMAPISupObj->SetProviderUID(lpProviderID, ulFlags);
}

HRESULT	ECMAPISupport::CompareEntryIDs(ULONG cbEntryID1, LPENTRYID lpEntryID1, ULONG cbEntryID2, LPENTRYID lpEntryID2, ULONG ulFlags, ULONG* lpulResult)
{
	return m_lpMAPISupObj->CompareEntryIDs(cbEntryID1, lpEntryID1, cbEntryID2, lpEntryID2, ulFlags, lpulResult);
}

HRESULT	ECMAPISupport::OpenTemplateID(ULONG cbTemplateID, LPENTRYID lpTemplateID, ULONG ulTemplateFlags, LPMAPIPROP lpMAPIPropData, LPCIID lpInterface, LPMAPIPROP* lppMAPIPropNew, LPMAPIPROP lpMAPIPropSibling)
{
	return m_lpMAPISupObj->OpenTemplateID(cbTemplateID, lpTemplateID, ulTemplateFlags, lpMAPIPropData, lpInterface, lppMAPIPropNew, lpMAPIPropSibling);
}

HRESULT	ECMAPISupport::OpenEntry(ULONG cbEntryID, LPENTRYID lpEntryID, LPCIID lpInterface, ULONG ulOpenFlags, ULONG* lpulObjType, LPUNKNOWN* lppUnk)
{
	return m_lpMAPISupObj->OpenEntry(cbEntryID, lpEntryID, lpInterface, ulOpenFlags, lpulObjType, lppUnk);
}

HRESULT	ECMAPISupport::GetOneOffTable(ULONG ulFlags, LPMAPITABLE* lppTable)
{
	return m_lpMAPISupObj->GetOneOffTable(ulFlags, lppTable);
}

HRESULT	ECMAPISupport::Address(ULONG* lpulUIParam, LPADRPARM lpAdrParms, LPADRLIST* lppAdrList)
{
	return m_lpMAPISupObj->Address(lpulUIParam, lpAdrParms, lppAdrList);
}

HRESULT	ECMAPISupport::Details(ULONG* lpulUIParam, LPFNDISMISS lpfnDismiss, LPVOID lpvDismissContext, ULONG cbEntryID, LPENTRYID lpEntryID, LPFNBUTTON lpfButtonCallback, LPVOID lpvButtonContext, LPTSTR lpszButtonText, ULONG ulFlags)
{
	return m_lpMAPISupObj->Details(lpulUIParam, lpfnDismiss, lpvDismissContext, cbEntryID, lpEntryID, lpfButtonCallback, lpvButtonContext, lpszButtonText, ulFlags);
}

HRESULT	ECMAPISupport::NewEntry(ULONG ulUIParam, ULONG ulFlags, ULONG cbEIDContainer, LPENTRYID lpEIDContainer, ULONG cbEIDNewEntryTpl, LPENTRYID lpEIDNewEntryTpl, ULONG* lpcbEIDNewEntry, LPENTRYID* lppEIDNewEntry)
{
	return m_lpMAPISupObj->NewEntry(ulUIParam, ulFlags, cbEIDContainer, lpEIDContainer, cbEIDNewEntryTpl, lpEIDNewEntryTpl, lpcbEIDNewEntry, lppEIDNewEntry);
}

HRESULT	ECMAPISupport::DoConfigPropsheet(ULONG ulUIParam, ULONG ulFlags, LPTSTR lpszTitle, LPMAPITABLE lpDisplayTable, LPMAPIPROP lpConfigData, ULONG ulTopPage)
{
	return m_lpMAPISupObj->DoConfigPropsheet(ulUIParam, ulFlags, lpszTitle, lpDisplayTable, lpConfigData, ulTopPage);
}

HRESULT	ECMAPISupport::CopyMessages(LPCIID lpSrcInterface, LPVOID lpSrcFolder, LPENTRYLIST lpMsgList, LPCIID lpDestInterface, LPVOID lpDestFolder, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, ULONG ulFlags)
{
	return m_lpMAPISupObj->CopyMessages(lpSrcInterface, lpSrcFolder, lpMsgList, lpDestInterface, lpDestFolder, ulUIParam, lpProgress, ulFlags);
}

HRESULT	ECMAPISupport::CopyFolder(LPCIID lpSrcInterface, LPVOID lpSrcFolder, ULONG cbEntryID, LPENTRYID lpEntryID, LPCIID lpInterface, LPVOID lpDestFolder, LPTSTR lpszNewFolderName, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, ULONG ulFlags)
{
	return m_lpMAPISupObj->CopyFolder(lpSrcInterface, lpSrcFolder, cbEntryID, lpEntryID, lpInterface, lpDestFolder, lpszNewFolderName, ulUIParam, lpProgress, ulFlags);
}

HRESULT	ECMAPISupport::DoCopyTo(LPCIID lpSrcInterface, LPVOID lpSrcObj, ULONG ciidExclude, LPCIID rgiidExclude, LPSPropTagArray lpExcludeProps, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, LPCIID lpDestInterface, LPVOID lpDestObj, ULONG ulFlags, LPSPropProblemArray* lppProblems)
{
	return m_lpMAPISupObj->DoCopyTo(lpSrcInterface, lpSrcObj, ciidExclude, rgiidExclude, lpExcludeProps, ulUIParam, lpProgress, lpDestInterface, lpDestObj, ulFlags, lppProblems);
}

HRESULT	ECMAPISupport::DoCopyProps(LPCIID lpSrcInterface, LPVOID lpSrcObj, LPSPropTagArray lpIncludeProps, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, LPCIID lpDestInterface, LPVOID lpDestObj, ULONG ulFlags, LPSPropProblemArray* lppProblems)
{
	return m_lpMAPISupObj->DoCopyProps(lpSrcInterface, lpSrcObj, lpIncludeProps, ulUIParam, lpProgress, lpDestInterface, lpDestObj, ulFlags, lppProblems);
}

HRESULT	ECMAPISupport::DoProgressDialog(ULONG ulUIParam, ULONG ulFlags, LPMAPIPROGRESS* lppProgress)
{
	return m_lpMAPISupObj->DoProgressDialog(ulUIParam, ulFlags, lppProgress);
}

HRESULT	ECMAPISupport::ReadReceipt(ULONG ulFlags, LPMESSAGE lpReadMessage, LPMESSAGE* lppEmptyMessage)
{
	return m_lpMAPISupObj->ReadReceipt(ulFlags, lpReadMessage, lppEmptyMessage);
}

HRESULT	ECMAPISupport::PrepareSubmit(LPMESSAGE lpMessage, ULONG* lpulFlags)
{
	return m_lpMAPISupObj->PrepareSubmit(lpMessage, lpulFlags);
}

HRESULT	ECMAPISupport::ExpandRecips(LPMESSAGE lpMessage, ULONG* lpulFlags)
{
	return m_lpMAPISupObj->ExpandRecips(lpMessage, lpulFlags);
}

HRESULT	ECMAPISupport::UpdatePAB(ULONG ulFlags, LPMESSAGE lpMessage)
{
	return m_lpMAPISupObj->UpdatePAB(ulFlags, lpMessage);
}

HRESULT	ECMAPISupport::DoSentMail(ULONG ulFlags, LPMESSAGE lpMessage)
{
	return m_lpMAPISupObj->DoSentMail(ulFlags, lpMessage);
}

HRESULT	ECMAPISupport::OpenAddressBook(LPCIID lpInterface, ULONG ulFlags, LPADRBOOK* lppAdrBook)
{
	return m_lpMAPISupObj->OpenAddressBook(lpInterface, ulFlags, lppAdrBook);
}

HRESULT	ECMAPISupport::Preprocess(ULONG ulFlags, ULONG cbEntryID, LPENTRYID lpEntryID)
{
	return m_lpMAPISupObj->Preprocess(ulFlags, cbEntryID, lpEntryID);
}

HRESULT	ECMAPISupport::CompleteMsg(ULONG ulFlags, ULONG cbEntryID, LPENTRYID lpEntryID)
{
	return m_lpMAPISupObj->CompleteMsg(ulFlags, cbEntryID, lpEntryID);
}

HRESULT	ECMAPISupport::StoreLogoffTransports(ULONG* lpulFlags)
{
	return m_lpMAPISupObj->StoreLogoffTransports(lpulFlags);
}

HRESULT	ECMAPISupport::StatusRecips(LPMESSAGE lpMessage, LPADRLIST lpRecipList)
{
	return m_lpMAPISupObj->StatusRecips(lpMessage, lpRecipList);
}

HRESULT	ECMAPISupport::WrapStoreEntryID(ULONG cbOrigEntry, LPENTRYID lpOrigEntry, ULONG* lpcbWrappedEntry, LPENTRYID* lppWrappedEntry)
{
	return m_lpMAPISupObj->WrapStoreEntryID(cbOrigEntry, lpOrigEntry, lpcbWrappedEntry, lppWrappedEntry);
}

HRESULT	ECMAPISupport::ModifyProfile(ULONG ulFlags)
{
	return m_lpMAPISupObj->ModifyProfile(ulFlags);
}

HRESULT	ECMAPISupport::IStorageFromStream(LPUNKNOWN lpUnkIn, LPCIID lpInterface, ULONG ulFlags, LPSTORAGE* lppStorageOut)
{
	return m_lpMAPISupObj->IStorageFromStream(lpUnkIn, lpInterface, ulFlags, lppStorageOut);
}

HRESULT	ECMAPISupport::GetSvcConfigSupportObj(ULONG ulFlags, LPMAPISUP* lppSvcSupport)
{
	return m_lpMAPISupObj->GetSvcConfigSupportObj(ulFlags, lppSvcSupport);
}


////////////////////////////////////////////////////////////////////////
// IUnknown
//
ULONG ECMAPISupport::xMAPISupport::AddRef()
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPISupport::AddRef", "");
	METHOD_PROLOGUE_(ECMAPISupport, MAPISupport);
	return pThis->AddRef();
}

ULONG ECMAPISupport::xMAPISupport::Release()
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPISupport::Release", "");
	METHOD_PROLOGUE_(ECMAPISupport, MAPISupport);
	return pThis->Release();
}

HRESULT ECMAPISupport::xMAPISupport::QueryInterface(REFIID refiid, void **lppInterface)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPISupport::QueryInterface", "%s", DBGGUIDToString(refiid).c_str());
	METHOD_PROLOGUE_(ECMAPISupport, MAPISupport);
	HRESULT hr = pThis->QueryInterface(refiid, lppInterface);
	TRACE_MAPI(TRACE_RETURN, "IMAPISupport::QueryInterface", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

////////////////////////////////////////////////////////////////////////
// IMAPISupport
//
HRESULT ECMAPISupport::xMAPISupport::GetLastError(HRESULT hResult, ULONG ulFlags, LPMAPIERROR* lppMAPIError)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPISupport::GetLastError", "");
	METHOD_PROLOGUE_(ECMAPISupport, MAPISupport);
	return pThis->GetLastError(hResult, ulFlags, lppMAPIError);
}

HRESULT ECMAPISupport::xMAPISupport::GetMemAllocRoutines(LPALLOCATEBUFFER* lppAllocateBuffer, LPALLOCATEMORE* lppAllocateMore, LPFREEBUFFER* lppFreeBuffer)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPISupport::GetMemAllocRoutines", "");
	METHOD_PROLOGUE_(ECMAPISupport, MAPISupport);
	HRESULT hr = pThis->GetMemAllocRoutines(lppAllocateBuffer, lppAllocateMore, lppFreeBuffer);
	TRACE_MAPI(TRACE_RETURN, "IMAPISupport::GetMemAllocRoutines", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMAPISupport::xMAPISupport::Subscribe(LPNOTIFKEY lpKey, ULONG ulEventMask, ULONG ulFlags, LPMAPIADVISESINK lpAdviseSink, ULONG* lpulConnection)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPISupport::Subscribe", "");
	METHOD_PROLOGUE_(ECMAPISupport, MAPISupport);
	HRESULT hr = pThis->Subscribe(lpKey, ulEventMask, ulFlags, lpAdviseSink, lpulConnection);
	TRACE_MAPI(TRACE_RETURN, "IMAPISupport::Subscribe", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMAPISupport::xMAPISupport::Unsubscribe(ULONG ulConnection)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPISupport::Unsubscribe", "");
	METHOD_PROLOGUE_(ECMAPISupport, MAPISupport);
	HRESULT hr = pThis->Unsubscribe(ulConnection);
	TRACE_MAPI(TRACE_RETURN, "IMAPISupport::Unsubscribe", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMAPISupport::xMAPISupport::Notify(LPNOTIFKEY lpKey, ULONG cNotification, LPNOTIFICATION lpNotifications, ULONG* lpulFlags)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPISupport::Notify", "");
	METHOD_PROLOGUE_(ECMAPISupport, MAPISupport);
	HRESULT hr = pThis->Notify(lpKey, cNotification, lpNotifications, lpulFlags);
	TRACE_MAPI(TRACE_RETURN, "IMAPISupport::Notify", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMAPISupport::xMAPISupport::ModifyStatusRow(ULONG cValues, LPSPropValue lpColumnVals, ULONG ulFlags)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPISupport::ModifyStatusRow", "");
	METHOD_PROLOGUE_(ECMAPISupport, MAPISupport);
	HRESULT hr = pThis->ModifyStatusRow(cValues, lpColumnVals, ulFlags);
	TRACE_MAPI(TRACE_RETURN, "IMAPISupport::ModifyStatusRow", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMAPISupport::xMAPISupport::OpenProfileSection(LPMAPIUID lpUid, ULONG ulFlags, LPPROFSECT* lppProfileObj)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPISupport::OpenProfileSection", "");
	METHOD_PROLOGUE_(ECMAPISupport, MAPISupport);
	HRESULT hr = pThis->OpenProfileSection(lpUid, ulFlags, lppProfileObj);
	TRACE_MAPI(TRACE_RETURN, "IMAPISupport::OpenProfileSection", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMAPISupport::xMAPISupport::RegisterPreprocessor(LPMAPIUID lpMuid, LPTSTR lpszAdrType, LPTSTR lpszDLLName, LPSTR lpszPreprocess, LPSTR lpszRemovePreprocessInfo, ULONG ulFlags)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPISupport::RegisterPreprocessor", "");
	METHOD_PROLOGUE_(ECMAPISupport, MAPISupport);
	HRESULT hr = pThis->RegisterPreprocessor(lpMuid, lpszAdrType, lpszDLLName, lpszPreprocess, lpszRemovePreprocessInfo, ulFlags);
	TRACE_MAPI(TRACE_RETURN, "IMAPISupport::RegisterPreprocessor", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMAPISupport::xMAPISupport::NewUID(LPMAPIUID lpMuid)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPISupport::NewUID", "");
	METHOD_PROLOGUE_(ECMAPISupport, MAPISupport);
	HRESULT hr = pThis->NewUID(lpMuid);
	TRACE_MAPI(TRACE_RETURN, "IMAPISupport::NewUID", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMAPISupport::xMAPISupport::MakeInvalid(ULONG ulFlags, LPVOID lpObject, ULONG ulRefCount, ULONG cMethods)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPISupport::MakeInvalid", "");
	METHOD_PROLOGUE_(ECMAPISupport, MAPISupport);
	HRESULT hr = pThis->MakeInvalid(ulFlags, lpObject, ulRefCount, cMethods);
	TRACE_MAPI(TRACE_RETURN, "IMAPISupport::MakeInvalid", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMAPISupport::xMAPISupport::SpoolerYield(ULONG ulFlags)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPISupport::SpoolerYield", "");
	METHOD_PROLOGUE_(ECMAPISupport, MAPISupport);
	HRESULT hr = pThis->SpoolerYield(ulFlags);
	TRACE_MAPI(TRACE_RETURN, "IMAPISupport::SpoolerYield", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMAPISupport::xMAPISupport::SpoolerNotify(ULONG ulFlags, LPVOID lpvData)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPISupport::SpoolerNotify", "");
	METHOD_PROLOGUE_(ECMAPISupport, MAPISupport);
	HRESULT hr = pThis->SpoolerNotify(ulFlags, lpvData);
	TRACE_MAPI(TRACE_RETURN, "IMAPISupport::SpoolerNotify", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMAPISupport::xMAPISupport::CreateOneOff(LPTSTR lpszName, LPTSTR lpszAdrType, LPTSTR lpszAddress, ULONG ulFlags, ULONG* lpcbEntryID, LPENTRYID* lppEntryID)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPISupport::CreateOneOff", "");
	METHOD_PROLOGUE_(ECMAPISupport, MAPISupport);
	HRESULT hr = pThis->CreateOneOff(lpszName, lpszAdrType, lpszAddress, ulFlags, lpcbEntryID, lppEntryID);
	TRACE_MAPI(TRACE_RETURN, "IMAPISupport::CreateOneOff", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMAPISupport::xMAPISupport::SetProviderUID(LPMAPIUID lpProviderID, ULONG ulFlags)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPISupport::SetProviderUID", "");
	METHOD_PROLOGUE_(ECMAPISupport, MAPISupport);
	HRESULT hr = pThis->SetProviderUID(lpProviderID, ulFlags);
	TRACE_MAPI(TRACE_RETURN, "IMAPISupport::SetProviderUID", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMAPISupport::xMAPISupport::CompareEntryIDs(ULONG cbEntryID1, LPENTRYID lpEntryID1, ULONG cbEntryID2, LPENTRYID lpEntryID2, ULONG ulFlags, ULONG* lpulResult)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPISupport::CompareEntryIDs", "");
	METHOD_PROLOGUE_(ECMAPISupport, MAPISupport);
	HRESULT hr = pThis->CompareEntryIDs(cbEntryID1, lpEntryID1, cbEntryID2, lpEntryID2, ulFlags, lpulResult);
	TRACE_MAPI(TRACE_RETURN, "IMAPISupport::CompareEntryIDs", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMAPISupport::xMAPISupport::OpenTemplateID(ULONG cbTemplateID, LPENTRYID lpTemplateID, ULONG ulTemplateFlags, LPMAPIPROP lpMAPIPropData, LPCIID lpInterface, LPMAPIPROP* lppMAPIPropNew, LPMAPIPROP lpMAPIPropSibling)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPISupport::OpenTemplateID", "");
	METHOD_PROLOGUE_(ECMAPISupport, MAPISupport);
	HRESULT hr = pThis->OpenTemplateID(cbTemplateID, lpTemplateID, ulTemplateFlags, lpMAPIPropData, lpInterface, lppMAPIPropNew, lpMAPIPropSibling);
	TRACE_MAPI(TRACE_RETURN, "IMAPISupport::OpenTemplateID", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMAPISupport::xMAPISupport::OpenEntry(ULONG cbEntryID, LPENTRYID lpEntryID, LPCIID lpInterface, ULONG ulOpenFlags, ULONG* lpulObjType, LPUNKNOWN* lppUnk)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPISupport::OpenEntry", "");
	METHOD_PROLOGUE_(ECMAPISupport, MAPISupport);
	HRESULT hr = pThis->OpenEntry(cbEntryID, lpEntryID, lpInterface, ulOpenFlags, lpulObjType, lppUnk);
	TRACE_MAPI(TRACE_RETURN, "IMAPISupport::OpenEntry", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMAPISupport::xMAPISupport::GetOneOffTable(ULONG ulFlags, LPMAPITABLE* lppTable)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPISupport::GetOneOffTable", "");
	METHOD_PROLOGUE_(ECMAPISupport, MAPISupport);
	HRESULT hr = pThis->GetOneOffTable(ulFlags, lppTable);
	TRACE_MAPI(TRACE_RETURN, "IMAPISupport::GetOneOffTable", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMAPISupport::xMAPISupport::Address(ULONG* lpulUIParam, LPADRPARM lpAdrParms, LPADRLIST* lppAdrList)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPISupport::Address", "");
	METHOD_PROLOGUE_(ECMAPISupport, MAPISupport);
	HRESULT hr = pThis->Address(lpulUIParam, lpAdrParms, lppAdrList);
	TRACE_MAPI(TRACE_RETURN, "IMAPISupport::Address", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMAPISupport::xMAPISupport::Details(ULONG* lpulUIParam, LPFNDISMISS lpfnDismiss, LPVOID lpvDismissContext, ULONG cbEntryID, LPENTRYID lpEntryID, LPFNBUTTON lpfButtonCallback, LPVOID lpvButtonContext, LPTSTR lpszButtonText, ULONG ulFlags)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPISupport::Details", "");
	METHOD_PROLOGUE_(ECMAPISupport, MAPISupport);
	HRESULT hr = pThis->Details(lpulUIParam, lpfnDismiss, lpvDismissContext, cbEntryID, lpEntryID, lpfButtonCallback, lpvButtonContext, lpszButtonText, ulFlags);
	TRACE_MAPI(TRACE_RETURN, "IMAPISupport::Details", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMAPISupport::xMAPISupport::NewEntry(ULONG ulUIParam, ULONG ulFlags, ULONG cbEIDContainer, LPENTRYID lpEIDContainer, ULONG cbEIDNewEntryTpl, LPENTRYID lpEIDNewEntryTpl, ULONG* lpcbEIDNewEntry, LPENTRYID* lppEIDNewEntry)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPISupport::NewEntry", "");
	METHOD_PROLOGUE_(ECMAPISupport, MAPISupport);
	HRESULT hr = pThis->NewEntry(ulUIParam, ulFlags, cbEIDContainer, lpEIDContainer, cbEIDNewEntryTpl, lpEIDNewEntryTpl, lpcbEIDNewEntry, lppEIDNewEntry);
	TRACE_MAPI(TRACE_RETURN, "IMAPISupport::NewEntry", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMAPISupport::xMAPISupport::DoConfigPropsheet(ULONG ulUIParam, ULONG ulFlags, LPTSTR lpszTitle, LPMAPITABLE lpDisplayTable, LPMAPIPROP lpConfigData, ULONG ulTopPage)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPISupport::DoConfigPropsheet", "");
	METHOD_PROLOGUE_(ECMAPISupport, MAPISupport);
	HRESULT hr = pThis->DoConfigPropsheet(ulUIParam, ulFlags, lpszTitle, lpDisplayTable, lpConfigData, ulTopPage);
	TRACE_MAPI(TRACE_RETURN, "IMAPISupport::DoConfigPropsheet", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMAPISupport::xMAPISupport::CopyMessages(LPCIID lpSrcInterface, LPVOID lpSrcFolder, LPENTRYLIST lpMsgList, LPCIID lpDestInterface, LPVOID lpDestFolder, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, ULONG ulFlags)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPISupport::CopyMessages", "");
	METHOD_PROLOGUE_(ECMAPISupport, MAPISupport);
	HRESULT hr = pThis->CopyMessages(lpSrcInterface, lpSrcFolder, lpMsgList, lpDestInterface, lpDestFolder, ulUIParam, lpProgress, ulFlags);
	TRACE_MAPI(TRACE_RETURN, "IMAPISupport::CopyMessages", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMAPISupport::xMAPISupport::CopyFolder(LPCIID lpSrcInterface, LPVOID lpSrcFolder, ULONG cbEntryID, LPENTRYID lpEntryID, LPCIID lpInterface, LPVOID lpDestFolder, LPTSTR lpszNewFolderName, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, ULONG ulFlags)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPISupport::CopyFolder", "");
	METHOD_PROLOGUE_(ECMAPISupport, MAPISupport);
	HRESULT hr = pThis->CopyFolder(lpSrcInterface, lpSrcFolder, cbEntryID, lpEntryID, lpInterface, lpDestFolder, lpszNewFolderName, ulUIParam, lpProgress, ulFlags);
	TRACE_MAPI(TRACE_RETURN, "IMAPISupport::CopyFolder", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMAPISupport::xMAPISupport::DoCopyTo(LPCIID lpSrcInterface, LPVOID lpSrcObj, ULONG ciidExclude, LPCIID rgiidExclude, LPSPropTagArray lpExcludeProps, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, LPCIID lpDestInterface, LPVOID lpDestObj, ULONG ulFlags, LPSPropProblemArray* lppProblems)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPISupport::DoCopyTo", "");
	METHOD_PROLOGUE_(ECMAPISupport, MAPISupport);
	HRESULT hr = pThis->DoCopyTo(lpSrcInterface, lpSrcObj, ciidExclude, rgiidExclude, lpExcludeProps, ulUIParam, lpProgress, lpDestInterface, lpDestObj, ulFlags, lppProblems);
	TRACE_MAPI(TRACE_RETURN, "IMAPISupport::DoCopyTo", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMAPISupport::xMAPISupport::DoCopyProps(LPCIID lpSrcInterface, LPVOID lpSrcObj, LPSPropTagArray lpIncludeProps, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, LPCIID lpDestInterface, LPVOID lpDestObj, ULONG ulFlags, LPSPropProblemArray* lppProblems)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPISupport::DoCopyProps", "");
	METHOD_PROLOGUE_(ECMAPISupport, MAPISupport);
	HRESULT hr = pThis->DoCopyProps(lpSrcInterface, lpSrcObj, lpIncludeProps, ulUIParam, lpProgress, lpDestInterface, lpDestObj, ulFlags, lppProblems);
	TRACE_MAPI(TRACE_RETURN, "IMAPISupport::DoCopyProps", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMAPISupport::xMAPISupport::DoProgressDialog(ULONG ulUIParam, ULONG ulFlags, LPMAPIPROGRESS* lppProgress)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPISupport::DoProgressDialog", "");
	METHOD_PROLOGUE_(ECMAPISupport, MAPISupport);
	HRESULT hr = pThis->DoProgressDialog(ulUIParam, ulFlags, lppProgress);
	TRACE_MAPI(TRACE_RETURN, "IMAPISupport::DoProgressDialog", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMAPISupport::xMAPISupport::ReadReceipt(ULONG ulFlags, LPMESSAGE lpReadMessage, LPMESSAGE* lppEmptyMessage)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPISupport::ReadReceipt", "");
	METHOD_PROLOGUE_(ECMAPISupport, MAPISupport);
	HRESULT hr = pThis->ReadReceipt(ulFlags, lpReadMessage, lppEmptyMessage);
	TRACE_MAPI(TRACE_RETURN, "IMAPISupport::ReadReceipt", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMAPISupport::xMAPISupport::PrepareSubmit(LPMESSAGE lpMessage, ULONG* lpulFlags)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPISupport::PrepareSubmit", "");
	METHOD_PROLOGUE_(ECMAPISupport, MAPISupport);
	HRESULT hr = pThis->PrepareSubmit(lpMessage, lpulFlags);
	TRACE_MAPI(TRACE_RETURN, "IMAPISupport::PrepareSubmit", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMAPISupport::xMAPISupport::ExpandRecips(LPMESSAGE lpMessage, ULONG* lpulFlags)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPISupport::ExpandRecips", "");
	METHOD_PROLOGUE_(ECMAPISupport, MAPISupport);
	HRESULT hr = pThis->ExpandRecips(lpMessage, lpulFlags);
	TRACE_MAPI(TRACE_RETURN, "IMAPISupport::ExpandRecips", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMAPISupport::xMAPISupport::UpdatePAB(ULONG ulFlags, LPMESSAGE lpMessage)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPISupport::UpdatePAB", "");
	METHOD_PROLOGUE_(ECMAPISupport, MAPISupport);
	HRESULT hr = pThis->UpdatePAB(ulFlags, lpMessage);
	TRACE_MAPI(TRACE_RETURN, "IMAPISupport::UpdatePAB", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMAPISupport::xMAPISupport::DoSentMail(ULONG ulFlags, LPMESSAGE lpMessage)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPISupport::DoSentMail", "");
	METHOD_PROLOGUE_(ECMAPISupport, MAPISupport);
	HRESULT hr = pThis->DoSentMail(ulFlags, lpMessage);
	TRACE_MAPI(TRACE_RETURN, "IMAPISupport::DoSentMail", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMAPISupport::xMAPISupport::OpenAddressBook(LPCIID lpInterface, ULONG ulFlags, LPADRBOOK* lppAdrBook)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPISupport::OpenAddressBook", "");
	METHOD_PROLOGUE_(ECMAPISupport, MAPISupport);
	HRESULT hr = pThis->OpenAddressBook(lpInterface, ulFlags, lppAdrBook);
	TRACE_MAPI(TRACE_RETURN, "IMAPISupport::OpenAddressBook", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMAPISupport::xMAPISupport::Preprocess(ULONG ulFlags, ULONG cbEntryID, LPENTRYID lpEntryID)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPISupport::Preprocess", "");
	METHOD_PROLOGUE_(ECMAPISupport, MAPISupport);
	HRESULT hr = pThis->Preprocess(ulFlags, cbEntryID, lpEntryID);
	TRACE_MAPI(TRACE_RETURN, "IMAPISupport::Preprocess", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMAPISupport::xMAPISupport::CompleteMsg(ULONG ulFlags, ULONG cbEntryID, LPENTRYID lpEntryID)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPISupport::CompleteMsg", "");
	METHOD_PROLOGUE_(ECMAPISupport, MAPISupport);
	HRESULT hr = pThis->CompleteMsg(ulFlags, cbEntryID, lpEntryID);
	TRACE_MAPI(TRACE_RETURN, "IMAPISupport::CompleteMsg", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMAPISupport::xMAPISupport::StoreLogoffTransports(ULONG* lpulFlags)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPISupport::StoreLogoffTransports", "");
	METHOD_PROLOGUE_(ECMAPISupport, MAPISupport);
	HRESULT hr = pThis->StoreLogoffTransports(lpulFlags);
	TRACE_MAPI(TRACE_RETURN, "IMAPISupport::StoreLogoffTransports", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMAPISupport::xMAPISupport::StatusRecips(LPMESSAGE lpMessage, LPADRLIST lpRecipList)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPISupport::StatusRecips", "");
	METHOD_PROLOGUE_(ECMAPISupport, MAPISupport);
	HRESULT hr = pThis->StatusRecips(lpMessage, lpRecipList);
	TRACE_MAPI(TRACE_RETURN, "IMAPISupport::StatusRecips", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMAPISupport::xMAPISupport::WrapStoreEntryID(ULONG cbOrigEntry, LPENTRYID lpOrigEntry, ULONG* lpcbWrappedEntry, LPENTRYID* lppWrappedEntry)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPISupport::WrapStoreEntryID", "");
	METHOD_PROLOGUE_(ECMAPISupport, MAPISupport);
	HRESULT hr = pThis->WrapStoreEntryID(cbOrigEntry, lpOrigEntry, lpcbWrappedEntry, lppWrappedEntry);
	TRACE_MAPI(TRACE_RETURN, "IMAPISupport::WrapStoreEntryID", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMAPISupport::xMAPISupport::ModifyProfile(ULONG ulFlags)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPISupport::ModifyProfile", "");
	METHOD_PROLOGUE_(ECMAPISupport, MAPISupport);
	HRESULT hr = pThis->ModifyProfile(ulFlags);
	TRACE_MAPI(TRACE_RETURN, "IMAPISupport::ModifyProfile", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMAPISupport::xMAPISupport::IStorageFromStream(LPUNKNOWN lpUnkIn, LPCIID lpInterface, ULONG ulFlags, LPSTORAGE* lppStorageOut)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPISupport::IStorageFromStream", "");
	METHOD_PROLOGUE_(ECMAPISupport, MAPISupport);
	HRESULT hr = pThis->IStorageFromStream(lpUnkIn, lpInterface, ulFlags, lppStorageOut);
	TRACE_MAPI(TRACE_RETURN, "IMAPISupport::IStorageFromStream", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMAPISupport::xMAPISupport::GetSvcConfigSupportObj(ULONG ulFlags, LPMAPISUP* lppSvcSupport)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPISupport::GetSvcConfigSupportObj", "");
	METHOD_PROLOGUE_(ECMAPISupport, MAPISupport);
	HRESULT hr = pThis->GetSvcConfigSupportObj(ulFlags, lppSvcSupport);
	TRACE_MAPI(TRACE_RETURN, "IMAPISupport::GetSvcConfigSupportObj", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}
