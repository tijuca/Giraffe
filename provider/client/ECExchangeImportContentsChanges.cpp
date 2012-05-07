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

#include "ECExchangeImportContentsChanges.h"
#include "WSMessageStreamImporter.h"
#include "ECMessageStreamImporterIStreamAdapter.h"
#include "ECLogger.h"
#include "ECSyncLog.h"

#include "Util.h"
#include "edkguid.h"
#include "ECGuid.h"
#include "mapiguid.h"
#include "ECMessage.h"
#include "ECDebug.h"

#include "ZarafaICS.h"
#include "mapiext.h"
#include "mapi_ptr.h"
#include "ECRestriction.h"

#include <mapiutil.h>

#include "charset/convert.h"
#include "ECGetText.h"
#include "EntryPoint.h"

#include <list>

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

#ifdef _DEBUG
class NullStream : public IStream
{
public:
	NullStream() : m_cRef(1) {}

	// IUnknown
	ULONG __stdcall AddRef() { return ++m_cRef; }
	ULONG __stdcall Release() { 
		ULONG cRef = --m_cRef; 
		if (!m_cRef) 
			delete this; 
		return cRef; 
	}
	HRESULT __stdcall QueryInterface(REFIID refiid, void **lpvoid) {
		if (refiid == IID_IUnknown || refiid == IID_IStream) {
			AddRef();
			*lpvoid = this;
			return hrSuccess;
		}
		return MAPI_E_INTERFACE_NOT_SUPPORTED;
	}
	
	// ISequentialStream
	HRESULT __stdcall Read(void *pv, ULONG cb, ULONG *pcbRead) { 
		if (pcbRead)
			*pcbRead = cb;
		return hrSuccess; 
	}
	HRESULT __stdcall Write(const void *pv, ULONG cb, ULONG *pcbWritten) { 
		if (pcbWritten)
			*pcbWritten = cb;
		return hrSuccess; 
	}

	// IStream
	HRESULT __stdcall Seek(LARGE_INTEGER dlibMove, DWORD dwOrigin, ULARGE_INTEGER *plibNewPosition) { return MAPI_E_NO_SUPPORT; }
	HRESULT __stdcall SetSize(ULARGE_INTEGER libNewSize) { return MAPI_E_NO_SUPPORT; }
	HRESULT __stdcall CopyTo(IStream *pstm, ULARGE_INTEGER cb, ULARGE_INTEGER *pcbRead, ULARGE_INTEGER *pcbWritten) { return MAPI_E_NO_SUPPORT; }
	HRESULT __stdcall Commit(DWORD grfCommitFlags) { return MAPI_E_NO_SUPPORT; }
	HRESULT __stdcall Revert(void) { return MAPI_E_NO_SUPPORT; }
	HRESULT __stdcall LockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType) { return MAPI_E_NO_SUPPORT; }
	HRESULT __stdcall UnlockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType) { return MAPI_E_NO_SUPPORT; }
	HRESULT __stdcall Stat(STATSTG *pstatstg, DWORD grfStatFlag) { return MAPI_E_NO_SUPPORT; }
	HRESULT __stdcall Clone(IStream **ppstm) { return MAPI_E_NO_SUPPORT; }

private:
	ULONG	m_cRef;
};
#endif

ECExchangeImportContentsChanges::ECExchangeImportContentsChanges(ECMAPIFolder *lpFolder)
: m_iidMessage(IID_IMessage)
{
	ECSyncLog::GetLogger(&m_lpLogger);

	m_lpFolder = lpFolder;
	m_lpStream = NULL;
	m_lpFolder->AddRef();
}

ECExchangeImportContentsChanges::~ECExchangeImportContentsChanges(){
	m_lpFolder->Release();
	m_lpLogger->Release();
}

HRESULT ECExchangeImportContentsChanges::Create(ECMAPIFolder *lpFolder, LPEXCHANGEIMPORTCONTENTSCHANGES* lppExchangeImportContentsChanges){
	HRESULT hr = hrSuccess;
	ECExchangeImportContentsChanges *lpEICC = NULL;

	if(!lpFolder)
		return MAPI_E_INVALID_PARAMETER;

	lpEICC = new ECExchangeImportContentsChanges(lpFolder);

	hr = lpEICC->QueryInterface(IID_IExchangeImportContentsChanges, (void **)lppExchangeImportContentsChanges);

	return hr;

}

HRESULT	ECExchangeImportContentsChanges::QueryInterface(REFIID refiid, void **lppInterface)
{
	BOOL	bCanStream = FALSE;

	REGISTER_INTERFACE(IID_ECExchangeImportContentsChanges, this);
	REGISTER_INTERFACE(IID_ECUnknown, this);

	if (refiid == IID_IECImportContentsChanges) {
		m_lpFolder->GetMsgStore()->lpTransport->HrCheckCapabilityFlags(ZARAFA_CAP_ENHANCED_ICS, &bCanStream);
		if (bCanStream == FALSE)
			return MAPI_E_INTERFACE_NOT_SUPPORTED;

		REGISTER_INTERFACE(IID_IECImportContentsChanges, &this->m_xECImportContentsChanges);
	}

	REGISTER_INTERFACE(IID_IExchangeImportContentsChanges, &this->m_xECImportContentsChanges);
	REGISTER_INTERFACE(IID_IUnknown, &this->m_xECImportContentsChanges);

	return MAPI_E_INTERFACE_NOT_SUPPORTED;
}

HRESULT ECExchangeImportContentsChanges::GetLastError(HRESULT hResult, ULONG ulFlags, LPMAPIERROR *lppMAPIError){
	HRESULT		hr = hrSuccess;
	LPMAPIERROR	lpMapiError = NULL;
	LPTSTR		lpszErrorMsg = NULL;
	
	//FIXME: give synchronization errors messages
	hr = Util::HrMAPIErrorToText((hResult == hrSuccess)?MAPI_E_NO_ACCESS : hResult, &lpszErrorMsg);
	if (hr != hrSuccess)
		goto exit;

	MAPIAllocateBuffer(sizeof(MAPIERROR),(void **)&lpMapiError);
	
	if ((ulFlags & MAPI_UNICODE) == MAPI_UNICODE) {
		std::wstring wstrErrorMsg = convert_to<std::wstring>(lpszErrorMsg);
		std::wstring wstrCompName = convert_to<std::wstring>(g_strProductName.c_str());

		MAPIAllocateMore(sizeof(std::wstring::value_type) * (wstrErrorMsg.size() + 1), lpMapiError, (void**)&lpMapiError->lpszError);
		wcscpy((wchar_t*)lpMapiError->lpszError, wstrErrorMsg.c_str());

		MAPIAllocateMore(sizeof(std::wstring::value_type) * (wstrCompName.size() + 1), lpMapiError, (void**)&lpMapiError->lpszComponent);
		wcscpy((wchar_t*)lpMapiError->lpszComponent, wstrCompName.c_str()); 

	} else {
		std::string strErrorMsg = convert_to<std::string>(lpszErrorMsg);
		std::string strCompName = convert_to<std::string>(g_strProductName.c_str());

		MAPIAllocateMore(strErrorMsg.size() + 1, lpMapiError, (void**)&lpMapiError->lpszError);
		strcpy((char*)lpMapiError->lpszError, strErrorMsg.c_str());

		MAPIAllocateMore(strCompName.size() + 1, lpMapiError, (void**)&lpMapiError->lpszComponent);
		strcpy((char*)lpMapiError->lpszComponent, strCompName.c_str());
	}

	lpMapiError->ulContext		= 0;
	lpMapiError->ulLowLevelError= 0;
	lpMapiError->ulVersion		= 0;

	*lppMAPIError = lpMapiError;

exit:
	if (lpszErrorMsg)
		MAPIFreeBuffer(lpszErrorMsg);

	return hr;
}

HRESULT ECExchangeImportContentsChanges::Config(LPSTREAM lpStream, ULONG ulFlags){
	HRESULT hr = hrSuccess;
	LARGE_INTEGER zero = {{0,0}};
	ULONG ulLen = 0;
	LPSPropValue lpPropSourceKey = NULL;
	
	m_lpStream = lpStream;

	if(lpStream == NULL) {
		m_ulSyncId = 0;
		m_ulChangeId = 0;
	} else {
		hr = lpStream->Seek(zero, STREAM_SEEK_SET, NULL);
		if(hr != hrSuccess)
			goto exit;
		
		hr = lpStream->Read(&m_ulSyncId, 4, &ulLen);
		if(hr != hrSuccess)
			goto exit;
			
		if(ulLen != 4) {
			hr = MAPI_E_INVALID_PARAMETER;
			goto exit;
		}
		
		hr = lpStream->Read(&m_ulChangeId, 4, &ulLen);
		if(hr != hrSuccess)
			goto exit;
			
		if(ulLen != 4) {
			hr = MAPI_E_INVALID_PARAMETER;
			goto exit;
		}
		
		hr = HrGetOneProp(&m_lpFolder->m_xMAPIFolder, PR_SOURCE_KEY, &lpPropSourceKey);
		if(hr != hrSuccess)
			goto exit;
		
		// The user specified the special sync key '0000000000000000', get a sync key from the server.
		if(m_ulSyncId == 0) {
			hr = m_lpFolder->GetMsgStore()->lpTransport->HrSetSyncStatus(lpPropSourceKey->Value.bin, m_ulSyncId, m_ulChangeId, ICS_SYNC_CONTENTS, 0, &m_ulSyncId);
			if(hr != hrSuccess)
				goto exit;
		}
		
		// The sync key we got from the server can be used to retrieve all items in the database now when given to IEEC->Config(). At the same time, any
		// items written to this importer will send the sync ID to the server so that any items written here will not be returned by the exporter,
		// preventing local looping of items.
	}		
		
	m_ulFlags = ulFlags;
exit:	
	if(lpPropSourceKey)
		MAPIFreeBuffer(lpPropSourceKey);
		
	return hrSuccess;
}

HRESULT ECExchangeImportContentsChanges::UpdateState(LPSTREAM lpStream){
	HRESULT hr = hrSuccess;
	LARGE_INTEGER zero = {{0,0}};
	ULONG ulLen = 0;

	if(lpStream == NULL) {
		if(m_lpStream == NULL){
			goto exit;
		}
		lpStream = m_lpStream;
	}

	if(m_ulSyncId == 0)
		goto exit; // config() called with NULL stream, so we'll ignore the UpdateState()
	
	hr = lpStream->Seek(zero, STREAM_SEEK_SET, NULL);
	if(hr != hrSuccess)
		goto exit;
		
	hr = lpStream->Write(&m_ulSyncId, 4, &ulLen);
	if(hr != hrSuccess)
		goto exit;
		
	hr = lpStream->Write(&m_ulChangeId, 4, &ulLen);
	if(hr != hrSuccess)
		goto exit;
	 
exit: 
	return hr;
}


HRESULT ECExchangeImportContentsChanges::ImportMessageChange(ULONG cValue, LPSPropValue lpPropArray, ULONG ulFlags, LPMESSAGE * lppMessage){
	HRESULT hr = hrSuccess; 

	SizedSPropTagArray(1, sptSourceKey) = { 1, {PR_SOURCE_KEY} };
	
	LPSPropValue lpFolderSourceKey = NULL;
	LPSPropValue lpPropPCL = NULL;
	LPSPropValue lpPropCK = NULL;
	ULONG cbEntryId = 0;
	LPENTRYID lpEntryId = NULL;
	LPSPropValue lpPassedEntryId = NULL;

	LPSPropValue lpMessageSourceKey = PpropFindProp(lpPropArray, cValue, PR_SOURCE_KEY);
	LPSPropValue lpMessageFlags = PpropFindProp(lpPropArray, cValue, PR_MESSAGE_FLAGS);
	LPSPropValue lpMessageAssociated = PpropFindProp(lpPropArray, cValue, 0x67AA000B); //PR_ASSOCIATED
	
	LPSPropValue lpRemotePCL = PpropFindProp(lpPropArray, cValue, PR_PREDECESSOR_CHANGE_LIST);
	LPSPropValue lpRemoteCK = PpropFindProp(lpPropArray, cValue, PR_CHANGE_KEY);

	ULONG ulObjType = 0;
	ULONG ulCount = 0;
	bool bAssociatedMessage = false;
	IMessage *lpMessage = NULL;
	ECMessage *lpECMessage = NULL;
	ULONG ulNewFlags = 0;

	hr = m_lpFolder->GetProps((LPSPropTagArray)&sptSourceKey,0, &ulCount, &lpFolderSourceKey);
	if(hr != hrSuccess)
		goto exit;

	if(lpFolderSourceKey->ulPropTag != PR_SOURCE_KEY) {
		hr = MAPI_E_CALL_FAILED;
		goto exit;
	}

	if(lpMessageSourceKey != NULL) {
		hr = m_lpFolder->GetMsgStore()->lpTransport->HrEntryIDFromSourceKey(m_lpFolder->GetMsgStore()->m_cbEntryId, m_lpFolder->GetMsgStore()->m_lpEntryId, lpFolderSourceKey->Value.bin.cb, lpFolderSourceKey->Value.bin.lpb, lpMessageSourceKey->Value.bin.cb, lpMessageSourceKey->Value.bin.lpb, &cbEntryId, &lpEntryId);
		if(hr != MAPI_E_NOT_FOUND && hr != hrSuccess)
			goto exit;
	} else {
	    // Source key not specified, therefore the message must be new since this is the only thing
	    // we can do if there is no sourcekey. Z-Push uses this, while offline ICS does not (it always
	    // passes a source key)
	    ulFlags |= SYNC_NEW_MESSAGE;
		hr = MAPI_E_NOT_FOUND;
	}

	if((hr == MAPI_E_NOT_FOUND) && ((ulFlags & SYNC_NEW_MESSAGE) == 0)) {
		// This is a change, but we don't already have the item. This can only mean
		// that the item has been deleted on our side. 
		hr = SYNC_E_OBJECT_DELETED;
		goto exit;
	}

	if((lpMessageFlags != NULL && (lpMessageFlags->Value.ul & MSGFLAG_ASSOCIATED)) || (lpMessageAssociated != NULL && lpMessageAssociated->Value.b)) {
		bAssociatedMessage = true;
	}

	if(hr == MAPI_E_NOT_FOUND){
		if(bAssociatedMessage == true){
		    ulNewFlags = MAPI_ASSOCIATED;
		}else{
		    ulNewFlags = 0;
		}

		lpPassedEntryId = PpropFindProp(lpPropArray, cValue, PR_ENTRYID);

		// Create the message with the passed entry ID
		if(lpPassedEntryId)
			hr = m_lpFolder->CreateMessageWithEntryID(&IID_IMessage, ulNewFlags, lpPassedEntryId->Value.bin.cb, (LPENTRYID)lpPassedEntryId->Value.bin.lpb, &lpMessage);
		else
			hr = m_lpFolder->CreateMessage(&IID_IMessage, ulNewFlags, &lpMessage);

		if(hr != hrSuccess)
			goto exit;
	}else{
		hr = m_lpFolder->OpenEntry(cbEntryId, lpEntryId, &m_iidMessage, MAPI_MODIFY, &ulObjType, (LPUNKNOWN*)&lpMessage);
		if(hr == MAPI_E_NOT_FOUND) {
			// The item was soft-deleted; sourcekey is known, but we cannot open the item. It has therefore been deleted.
			hr = SYNC_E_OBJECT_DELETED;
			goto exit;
		}

		if(hr != hrSuccess)
			goto exit;
		
		if (IsProcessed(lpRemoteCK, lpPropPCL)) {
			//we already have this change
			hr = SYNC_E_IGNORE;
			goto exit;
		}
		
		// Check for conflicts except for associated messages, take always the lastone
		if(bAssociatedMessage == false && HrGetOneProp(lpMessage, PR_CHANGE_KEY, &lpPropCK) == hrSuccess && IsConflict(lpPropCK, lpRemotePCL)){
			if(CreateConflictMessage(lpMessage) == MAPI_E_NOT_FOUND){
				CreateConflictFolders();
				CreateConflictMessage(lpMessage);
			}
		}
	}

	hr = lpMessage->QueryInterface(IID_ECMessage, (void **)&lpECMessage);
	if(hr != hrSuccess)
		goto exit;
		
	hr = lpECMessage->HrSetSyncId(m_ulSyncId);
	if(hr != hrSuccess)
		goto exit;

	// Mark as ICS object
	hr = lpECMessage->SetICSObject(TRUE);
	if(hr != hrSuccess)
		goto exit;

	hr = lpMessage->SetProps(cValue, lpPropArray, NULL);
	if(hr != hrSuccess)
		goto exit;
		
	*lppMessage = lpMessage;

exit:
	if(lpECMessage)
		lpECMessage->Release();
		
	if(lpFolderSourceKey)
		MAPIFreeBuffer(lpFolderSourceKey);

	if(lpPropPCL)
		MAPIFreeBuffer(lpPropPCL);

	if(lpPropCK)
		MAPIFreeBuffer(lpPropCK);

	if(lpEntryId)
		MAPIFreeBuffer(lpEntryId);

	return hr;
}

//ulFlags = SYNC_SOFT_DELETE, SYNC_EXPIRY
HRESULT ECExchangeImportContentsChanges::ImportMessageDeletion(ULONG ulFlags, LPENTRYLIST lpSourceEntryList){
	HRESULT hr = hrSuccess;
	ENTRYLIST EntryList;
	LPSPropValue lpPropVal = NULL;
	ULONG ulSKNr, ulCount;
	SizedSPropTagArray(1, spt) = { 1, {PR_SOURCE_KEY} };
	EntryList.lpbin = NULL;
	EntryList.cValues = 0;
	
	hr = m_lpFolder->GetProps((LPSPropTagArray)&spt,0, &ulCount, &lpPropVal);
	if(hr != hrSuccess)
		goto exit;


	MAPIAllocateBuffer(sizeof(SBinary)* lpSourceEntryList->cValues, (LPVOID*)&EntryList.lpbin);
	for(ulSKNr = 0; ulSKNr < lpSourceEntryList->cValues; ulSKNr++){
		hr = m_lpFolder->GetMsgStore()->lpTransport->HrEntryIDFromSourceKey(m_lpFolder->GetMsgStore()->m_cbEntryId, m_lpFolder->GetMsgStore()->m_lpEntryId, lpPropVal->Value.bin.cb, lpPropVal->Value.bin.lpb, lpSourceEntryList->lpbin[ulSKNr].cb, lpSourceEntryList->lpbin[ulSKNr].lpb, &EntryList.lpbin[EntryList.cValues].cb, (LPENTRYID*)&EntryList.lpbin[EntryList.cValues].lpb);
		if(hr == MAPI_E_NOT_FOUND){
			hr = hrSuccess;
			continue;
		}
		if(hr != hrSuccess)
			goto exit;
		EntryList.cValues++;
	}
	
	if(EntryList.cValues == 0)
		goto exit;

	hr = m_lpFolder->GetMsgStore()->lpTransport->HrDeleteObjects(ulFlags & SYNC_SOFT_DELETE ? 0 : DELETE_HARD_DELETE, &EntryList, m_ulSyncId);
	if(hr != hrSuccess)
		goto exit;

exit:
	if(EntryList.lpbin){
		for(ulSKNr = 0; ulSKNr < EntryList.cValues; ulSKNr++){
			MAPIFreeBuffer(EntryList.lpbin[ulSKNr].lpb);
		}
		MAPIFreeBuffer(EntryList.lpbin);
	}
	return hr;
}

HRESULT ECExchangeImportContentsChanges::ImportPerUserReadStateChange(ULONG cElements, LPREADSTATE lpReadState){
	HRESULT hr = hrSuccess;	
	LPSPropValue lpPropVal = NULL;
	ULONG ulSKNr, ulCount, cbEntryId;
	LPENTRYID lpEntryId = NULL;
	SizedSPropTagArray(1, spt) = { 1, {PR_SOURCE_KEY} };
	
	hr = m_lpFolder->GetProps((LPSPropTagArray)&spt,0, &ulCount, &lpPropVal);
	if(hr != hrSuccess)
		goto exit;

	for(ulSKNr = 0; ulSKNr < cElements; ulSKNr++){
		hr = m_lpFolder->GetMsgStore()->lpTransport->HrEntryIDFromSourceKey(m_lpFolder->GetMsgStore()->m_cbEntryId, m_lpFolder->GetMsgStore()->m_lpEntryId , lpPropVal->Value.bin.cb, lpPropVal->Value.bin.lpb, lpReadState[ulSKNr].cbSourceKey, lpReadState[ulSKNr].pbSourceKey, &cbEntryId, &lpEntryId);
		if(hr == MAPI_E_NOT_FOUND){
			hr = hrSuccess;
			continue; // Message is delete or moved
		}

		if(hr != hrSuccess) {
			goto exit;
		}

		hr = m_lpFolder->GetMsgStore()->lpTransport->HrSetReadFlag(cbEntryId, lpEntryId, lpReadState[ulSKNr].ulFlags & MSGFLAG_READ ? 0 : CLEAR_READ_FLAG, m_ulSyncId);
		if(hr != hrSuccess)
			goto exit;

		if(lpEntryId){
			MAPIFreeBuffer(lpEntryId);
			lpEntryId = NULL;
		}
	}

exit:
	if(lpEntryId)
		MAPIFreeBuffer(lpEntryId);

	return hr;
}

HRESULT ECExchangeImportContentsChanges::ImportMessageMove(ULONG cbSourceKeySrcFolder, BYTE FAR * pbSourceKeySrcFolder, ULONG cbSourceKeySrcMessage, BYTE FAR * pbSourceKeySrcMessage, ULONG cbPCLMessage, BYTE FAR * pbPCLMessage, ULONG cbSourceKeyDestMessage, BYTE FAR * pbSourceKeyDestMessage, ULONG cbChangeNumDestMessage, BYTE FAR * pbChangeNumDestMessage){
	return MAPI_E_NO_SUPPORT;
}

/** Check if the imported change has already been processed
 *
 * This is done by checking if the remote change key (or a newer change from the same source) is present
 * in the local predecessor change list.
 *
 * @param[in]	lpRemoteCK	The remote change key
 * @param[in]	lpLocalPCL	The local predecessor change list
 *
 * @return 	boolean
 * @return	true	The change has been processed before.
 * @return	false	The change hasn't been processed yet.
 */
bool ECExchangeImportContentsChanges::IsProcessed(LPSPropValue lpRemoteCK, LPSPropValue lpLocalPCL)
{
	if (!lpRemoteCK || !lpLocalPCL)
		return false;

	ASSERT(lpRemoteCK->ulPropTag == PR_CHANGE_KEY);
	ASSERT(lpLocalPCL->ulPropTag == PR_PREDECESSOR_CHANGE_LIST);

	const std::string strChangeList((char*)lpLocalPCL->Value.bin.lpb, lpLocalPCL->Value.bin.cb);
	size_t ulPos = 0;
	while (ulPos < strChangeList.size()) {
		size_t ulSize = strChangeList.at(ulPos++);
		if (ulSize <= sizeof(GUID) ){
			break;
		} else if (lpRemoteCK->Value.bin.cb > sizeof(GUID) && memcmp(strChangeList.data() + ulPos, lpRemoteCK->Value.bin.lpb, sizeof(GUID)) == 0){
			if (ulSize == lpRemoteCK->Value.bin.cb && memcmp(strChangeList.data() + ulPos, lpRemoteCK->Value.bin.lpb, ulSize) == 0) {
				//remote changekey in our changelist
				//we already have this change
				return true;
			}
		}
		ulPos += ulSize;
	}

	return false;
}

/** Check if the imported change conflicts with a local change.
 *
 * How this works
 *
 * We get
 * 1) the remote (exporter's) predecessor change list
 * 2) the item that will be modified (importer's) change key
 *
 * We then look at the remote change list, and find a change with the same GUID as the local change
 *
 * - If the trailing part (which increases with each change) is higher locally than is on the remote change list,
 *   then there's a conflict since we have a newer version than the remote server has, and the remote server is sending
 *   us a change.
 * - If the remote PCL does not contain an entry with the same GUID as our local change key, than there's a conflict since
 *   we get a change from the other side while we have a change which is not seen yet by the other side.
 * 
 * @param[in]	lpLocalCK	The local change key
 * @param[in]	lpRemotePCL	The remote predecessor change list
 *
 * @return	boolean
 * @retval	true	The change conflicts with a local change
 * @retval	false	The change doesn't conflict with a local change.
 */
bool ECExchangeImportContentsChanges::IsConflict(LPSPropValue lpLocalCK, LPSPropValue lpRemotePCL)
{
	if (!lpLocalCK || !lpRemotePCL)
		return false;

	ASSERT(lpLocalCK->ulPropTag == PR_CHANGE_KEY);
	ASSERT(lpRemotePCL->ulPropTag == PR_PREDECESSOR_CHANGE_LIST);

	bool bConflict = false;
	bool bGuidFound = false;
	const std::string strChangeList((char*)lpRemotePCL->Value.bin.lpb, lpRemotePCL->Value.bin.cb);
	size_t ulPos = 0;

	while (!bConflict && ulPos < strChangeList.size()) {
		size_t ulSize = strChangeList.at(ulPos++);
		if (ulSize <= sizeof(GUID)) {
			break;
		 } else if (lpLocalCK->Value.bin.cb > sizeof(GUID) && memcmp(strChangeList.data() + ulPos, lpLocalCK->Value.bin.lpb, sizeof(GUID)) == 0) {
			bGuidFound = true;	// Track if we found the GUID from our local change key

			unsigned int ulRemoteChangeNumber = 0;
			unsigned int ulLocalChangeNumber = 0;

			ulRemoteChangeNumber = *(unsigned int *)(strChangeList.data() + ulPos + sizeof(GUID)); 
			ulLocalChangeNumber = *(unsigned int *)(lpLocalCK->Value.bin.lpb + sizeof(GUID));

			// We have a conflict if we have a newer change locally than the remove server is sending us.
			bConflict = ulLocalChangeNumber > ulRemoteChangeNumber;
		}
		ulPos += ulSize;
	}

	if (!bGuidFound)
		bConflict = true;

	return bConflict;
}


HRESULT ECExchangeImportContentsChanges::CreateConflictMessage(LPMESSAGE lpMessage)
{
	HRESULT hr = hrSuccess;
	LPSPropValue lpConflictItems = NULL;

	hr = CreateConflictMessageOnly(lpMessage, &lpConflictItems);
	if (hr != hrSuccess)
		goto exit;

	hr = HrSetOneProp(lpMessage, lpConflictItems);
	if(hr != hrSuccess)
		goto exit;

	hr = lpMessage->SaveChanges(KEEP_OPEN_READWRITE);
	if(hr != hrSuccess)
		goto exit;


exit:
	if(lpConflictItems)
		MAPIFreeBuffer(lpConflictItems);

	return hr;
}

HRESULT ECExchangeImportContentsChanges::CreateConflictMessageOnly(LPMESSAGE lpMessage, LPSPropValue *lppConflictItems)
{
	HRESULT hr = hrSuccess;
	LPMAPIFOLDER lpRootFolder = NULL;
	LPMAPIFOLDER lpConflictFolder = NULL;
	LPMESSAGE lpConflictMessage = NULL;
	LPSPropValue lpPropAdditionalREN = NULL;
	LPSPropValue lpConflictItems = NULL;
	LPSPropValue lpEntryIdProp = NULL;
	LPSBinary lpEntryIds = NULL;
	ULONG ulCount = 0;
	ULONG ulObjType = 0;
	SizedSPropTagArray(5, excludeProps) = { 5, {PR_ENTRYID, PR_CONFLICT_ITEMS, PR_SOURCE_KEY, PR_CHANGE_KEY, PR_PREDECESSOR_CHANGE_LIST} };

	//open the conflicts folder
	hr = m_lpFolder->GetMsgStore()->OpenEntry(0, NULL, &IID_IMAPIFolder, 0, &ulObjType, (LPUNKNOWN*)&lpRootFolder);
	if(hr != hrSuccess)
		goto exit;

	hr = HrGetOneProp(lpRootFolder, PR_ADDITIONAL_REN_ENTRYIDS, &lpPropAdditionalREN);
	if(hr != hrSuccess)
		goto exit;

	if(lpPropAdditionalREN->Value.MVbin.cValues == 0 || lpPropAdditionalREN->Value.MVbin.lpbin[0].cb == 0){
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}

	hr = m_lpFolder->GetMsgStore()->OpenEntry(lpPropAdditionalREN->Value.MVbin.lpbin[0].cb, (LPENTRYID)lpPropAdditionalREN->Value.MVbin.lpbin[0].lpb, &IID_IMAPIFolder, MAPI_MODIFY, &ulObjType, (LPUNKNOWN*)&lpConflictFolder);
	if(hr != hrSuccess)
		goto exit;

	//create the conflict message
	hr = lpConflictFolder->CreateMessage(NULL, 0, &lpConflictMessage);
	if(hr != hrSuccess)
		goto exit;

	hr = lpMessage->CopyTo(0, NULL, (LPSPropTagArray)&excludeProps, 0, NULL, &IID_IMessage, lpConflictMessage, 0, NULL);
	if(hr != hrSuccess)
		goto exit;

	//set the entryid from original message in PR_CONFLICT_ITEMS of conflict message
	hr = HrGetOneProp(lpMessage, PR_ENTRYID ,&lpEntryIdProp);
	if(hr != hrSuccess)
		goto exit;

	hr = MAPIAllocateBuffer(sizeof(SPropValue), (LPVOID*)&lpConflictItems);
	if(hr != hrSuccess)
		goto exit;

	lpConflictItems->ulPropTag = PR_CONFLICT_ITEMS;
	lpConflictItems->Value.MVbin.cValues = 1;
	lpConflictItems->Value.MVbin.lpbin = &lpEntryIdProp->Value.bin;

	hr = HrSetOneProp(lpConflictMessage, lpConflictItems);
	if(hr != hrSuccess)
		goto exit;

	hr = lpConflictMessage->SaveChanges(KEEP_OPEN_READWRITE);
	if(hr != hrSuccess)
		goto exit;

	if(lpEntryIdProp){
		MAPIFreeBuffer(lpEntryIdProp);
		lpEntryIdProp = NULL;
	}

	if(lpConflictItems){
		MAPIFreeBuffer(lpConflictItems);
		lpConflictItems = NULL;
	}

	//add the entryid from the conflict message to the PR_CONFLICT_ITEMS of the original message
	hr = HrGetOneProp(lpConflictMessage, PR_ENTRYID, &lpEntryIdProp);
	if(hr != hrSuccess)
		goto exit;

	if(hrSuccess != HrGetOneProp(lpMessage, PR_CONFLICT_ITEMS, &lpConflictItems)){
		hr = MAPIAllocateBuffer(sizeof(SPropValue), (LPVOID*)&lpConflictItems);
		if(hr != hrSuccess)
			goto exit;

		lpConflictItems->ulPropTag = PR_CONFLICT_ITEMS;
		lpConflictItems->Value.MVbin.cValues = 0;
		lpConflictItems->Value.MVbin.lpbin = NULL;
	}
	
	hr = MAPIAllocateMore(sizeof(SBinary)*(lpConflictItems->Value.MVbin.cValues+1), lpConflictItems, (LPVOID*)&lpEntryIds);
	if(hr != hrSuccess)
		goto exit;

	for(ulCount = 0; ulCount < lpConflictItems->Value.MVbin.cValues; ulCount++){
		lpEntryIds[ulCount].cb = lpConflictItems->Value.MVbin.lpbin[ulCount].cb;
		lpEntryIds[ulCount].lpb = lpConflictItems->Value.MVbin.lpbin[ulCount].lpb;
	}
	lpEntryIds[ulCount].cb = lpEntryIdProp->Value.bin.cb;
	lpEntryIds[ulCount].lpb = lpEntryIdProp->Value.bin.lpb;

	lpConflictItems->Value.MVbin.lpbin = lpEntryIds;
	lpConflictItems->Value.MVbin.cValues++;

	if (lppConflictItems) {
		*lppConflictItems = lpConflictItems;
		lpConflictItems = NULL;
	}

exit:
	if(lpRootFolder)
		lpRootFolder->Release();

	if(lpConflictFolder)
		lpConflictFolder->Release();

	if(lpConflictMessage)
		lpConflictMessage->Release();

	if(lpPropAdditionalREN)
		MAPIFreeBuffer(lpPropAdditionalREN);

	if(lpConflictItems)
		MAPIFreeBuffer(lpConflictItems);

	if(lpEntryIdProp)
		MAPIFreeBuffer(lpEntryIdProp);

	return hr;
}


HRESULT ECExchangeImportContentsChanges::CreateConflictFolders(){
	HRESULT hr = hrSuccess;
	LPMAPIFOLDER lpRootFolder = NULL;
	LPMAPIFOLDER lpParentFolder = NULL;
	LPMAPIFOLDER lpInbox = NULL;
	LPMAPIFOLDER lpConflictFolder = NULL;
	LPSPropValue lpAdditionalREN = NULL;
	LPSPropValue lpNewAdditionalREN = NULL;
	LPSPropValue lpIPMSubTree = NULL;
	LPENTRYID lpEntryId = NULL;
	ULONG cbEntryId = 0;
	ULONG ulObjType = 0;
	ULONG ulCount = 0;

	hr = m_lpFolder->OpenEntry(0, NULL, &IID_IMAPIFolder, MAPI_MODIFY, &ulObjType, (LPUNKNOWN*)&lpRootFolder);
	if(hr != hrSuccess) {
		LOG_DEBUG(m_lpLogger, "Failed to open root folder, hr = 0x%08x", hr);
		goto exit;
	}

	hr = m_lpFolder->GetMsgStore()->GetReceiveFolder((TCHAR*)"IPM", 0, &cbEntryId, &lpEntryId, NULL);
	if(hr != hrSuccess) {
		LOG_DEBUG(m_lpLogger, "Failed to get 'IPM' receive folder id, hr = 0x%08x", hr);
		goto exit;
	}

	hr = m_lpFolder->OpenEntry(cbEntryId, lpEntryId, &IID_IMAPIFolder, MAPI_MODIFY, &ulObjType, (LPUNKNOWN*)&lpInbox);
	if(hr != hrSuccess) {
		LOG_DEBUG(m_lpLogger, "Failed to open 'IPM' receive folder, hr = 0x%08x", hr);
		goto exit;
	}

	hr = HrGetOneProp(&m_lpFolder->GetMsgStore()->m_xMsgStore, PR_IPM_SUBTREE_ENTRYID, &lpIPMSubTree);
	if(hr != hrSuccess) {
		LOG_DEBUG(m_lpLogger, "Failed to get ipm subtree id, hr = 0x%08x", hr);
		goto exit;
	}

	hr = m_lpFolder->OpenEntry(lpIPMSubTree->Value.bin.cb, (LPENTRYID)lpIPMSubTree->Value.bin.lpb, &IID_IMAPIFolder, MAPI_MODIFY, &ulObjType, (LPUNKNOWN*)&lpParentFolder);
	if(hr != hrSuccess) {
		LOG_DEBUG(m_lpLogger, "Failed to open ipm subtree folder, hr = 0x%08x", hr);
		goto exit;
	}

	HrGetOneProp(lpRootFolder, PR_ADDITIONAL_REN_ENTRYIDS, &lpAdditionalREN);

	//make new PR_ADDITIONAL_REN_ENTRYIDS
	hr = MAPIAllocateBuffer(sizeof(SPropValue), (LPVOID*)&lpNewAdditionalREN);
	if(hr != hrSuccess)
		goto exit;

	lpNewAdditionalREN->ulPropTag = PR_ADDITIONAL_REN_ENTRYIDS;
	lpNewAdditionalREN->Value.MVbin.cValues = (lpAdditionalREN && lpAdditionalREN->Value.MVbin.cValues<4)?4:lpAdditionalREN->Value.MVbin.cValues;
	hr = MAPIAllocateMore(sizeof(SBinary)*lpNewAdditionalREN->Value.MVbin.cValues, lpNewAdditionalREN, (LPVOID*)&lpNewAdditionalREN->Value.MVbin.lpbin);
	if(hr != hrSuccess)
		goto exit;

	//copy from original PR_ADDITIONAL_REN_ENTRYIDS
	if(lpAdditionalREN){
		for(ulCount = 0; ulCount < lpAdditionalREN->Value.MVbin.cValues; ulCount++){
			lpNewAdditionalREN->Value.MVbin.lpbin[ulCount] = lpAdditionalREN->Value.MVbin.lpbin[ulCount];
		}
	}

	hr = CreateConflictFolder(_("Sync Issues"), lpNewAdditionalREN, 1, lpParentFolder, &lpConflictFolder);
	if(hr != hrSuccess) {
		LOG_DEBUG(m_lpLogger, "Failed to create 'Sync Issues' folder, hr = 0x%08x", hr);
		goto exit;
	}
	
	hr = CreateConflictFolder(_("Conflicts"), lpNewAdditionalREN, 0, lpConflictFolder, NULL);
	if(hr != hrSuccess) {
		LOG_DEBUG(m_lpLogger, "Failed to create 'Conflicts' folder, hr = 0x%08x", hr);
		goto exit;
	}
	
	hr = CreateConflictFolder(_("Local Failures"), lpNewAdditionalREN, 2, lpConflictFolder, NULL);
	if(hr != hrSuccess) {
		LOG_DEBUG(m_lpLogger, "Failed to create 'Local Failures' folder, hr = 0x%08x", hr);
		goto exit;
	}
	
	hr = CreateConflictFolder(_("Server Failures"), lpNewAdditionalREN, 3, lpConflictFolder, NULL);
	if(hr != hrSuccess) {
		LOG_DEBUG(m_lpLogger, "Failed to create 'Server Failures' folder, hr = 0x%08x", hr);
		goto exit;
	}

	hr = HrSetOneProp(lpRootFolder, lpNewAdditionalREN);
	if(hr != hrSuccess)
		goto exit;

	hr = HrSetOneProp(lpInbox, lpNewAdditionalREN);
	if(hr != hrSuccess)
		goto exit;

	hr = HrUpdateSearchReminders(lpRootFolder, lpNewAdditionalREN);
	if (hr == MAPI_E_NOT_FOUND) {
		m_lpLogger->Log(EC_LOGLEVEL_INFO, "No reminder searchfolder found, nothing to update");
		hr = hrSuccess;
	} else if (hr != hrSuccess) {
		LOG_DEBUG(m_lpLogger, "Failed to update search reminders, hr = 0x%08x", hr);
		goto exit;
	}

exit:
	if(lpRootFolder)
		lpRootFolder->Release();
	if(lpParentFolder)
		lpParentFolder->Release();
	if(lpInbox)
		lpInbox->Release();

	if(lpConflictFolder)
		lpConflictFolder->Release();
	if(lpAdditionalREN)
		MAPIFreeBuffer(lpAdditionalREN);
	if(lpNewAdditionalREN)
		MAPIFreeBuffer(lpNewAdditionalREN);
	if(lpIPMSubTree)
		MAPIFreeBuffer(lpIPMSubTree);
	if(lpEntryId)
		MAPIFreeBuffer(lpEntryId);

	return hr;
}

HRESULT ECExchangeImportContentsChanges::CreateConflictFolder(LPTSTR lpszName, LPSPropValue lpAdditionalREN, ULONG ulMVPos, LPMAPIFOLDER lpParentFolder, LPMAPIFOLDER * lppConflictFolder){
	HRESULT hr = hrSuccess;
	LPMAPIFOLDER lpConflictFolder = NULL;
	LPSPropValue lpEntryId = NULL;
	SPropValue sPropValue;
	ULONG ulObjType = 0;

	if(lpAdditionalREN->Value.MVbin.lpbin[ulMVPos].cb > 0 && hrSuccess == lpParentFolder->OpenEntry(lpAdditionalREN->Value.MVbin.lpbin[ulMVPos].cb, (LPENTRYID)lpAdditionalREN->Value.MVbin.lpbin[ulMVPos].lpb, &IID_IMAPIFolder, MAPI_MODIFY, &ulObjType, (LPUNKNOWN*)&lpConflictFolder)){
		if(lppConflictFolder)
			*lppConflictFolder = lpConflictFolder;
		goto exit;
	}

	hr = lpParentFolder->CreateFolder(FOLDER_GENERIC, lpszName, NULL, &IID_IMAPIFolder, OPEN_IF_EXISTS | fMapiUnicode, &lpConflictFolder);
	if(hr != hrSuccess)
		goto exit;

	sPropValue.ulPropTag = PR_FOLDER_DISPLAY_FLAGS;
	sPropValue.Value.bin.cb = 6;
	sPropValue.Value.bin.lpb = (LPBYTE)"\x01\x04\x0A\x80\x1E\x00";

	hr = HrSetOneProp(lpConflictFolder, &sPropValue);
	if(hr != hrSuccess)
		goto exit;

	hr = HrGetOneProp(lpConflictFolder, PR_ENTRYID, &lpEntryId);
	if(hr != hrSuccess)
		goto exit;

	hr = MAPIAllocateMore(lpEntryId->Value.bin.cb, lpAdditionalREN, (LPVOID*)&lpAdditionalREN->Value.MVbin.lpbin[ulMVPos].lpb);
	if(hr != hrSuccess)
		goto exit;
	memcpy(lpAdditionalREN->Value.MVbin.lpbin[ulMVPos].lpb, lpEntryId->Value.bin.lpb, lpEntryId->Value.bin.cb);
	lpAdditionalREN->Value.MVbin.lpbin[ulMVPos].cb = lpEntryId->Value.bin.cb;

	if(lppConflictFolder)
		*lppConflictFolder = lpConflictFolder;

exit:
	if((hr != hrSuccess || lppConflictFolder == NULL) && lpConflictFolder)
		lpConflictFolder->Release();
	
	if(lpEntryId)
		MAPIFreeBuffer(lpEntryId);

	return hr;
}

HRESULT ECExchangeImportContentsChanges::ConfigForConversionStream(LPSTREAM lpStream, ULONG ulFlags, ULONG /*cValuesConversion*/, LPSPropValue /*lpPropArrayConversion*/)
{
	HRESULT hr = hrSuccess;
	BOOL	bCanStream = FALSE;

	// Since we don't use the cValuesConversion and lpPropArrayConversion arguments, we'll just check
	// if the server suppors streaming and if so call the 'normal' config.

	hr = m_lpFolder->GetMsgStore()->lpTransport->HrCheckCapabilityFlags(ZARAFA_CAP_ENHANCED_ICS, &bCanStream);
	if (hr != hrSuccess)
		goto exit;

	if (bCanStream == FALSE) {
		hr = MAPI_E_NO_SUPPORT;
		goto exit;
	}

	hr = Config(lpStream, ulFlags);

exit:
	return hr;
}

HRESULT ECExchangeImportContentsChanges::ImportMessageChangeAsAStream(ULONG cValue, LPSPropValue lpPropArray, ULONG ulFlags, LPSTREAM *lppStream)
{
	HRESULT hr = hrSuccess; 
	SPropValuePtr ptrFolderSourceKey;
	ULONG cbEntryId = 0;
	EntryIdPtr ptrEntryId;
	WSMessageStreamImporterPtr ptrMessageImporter;
	StreamPtr ptrStream;

	LPSPropValue lpMessageSourceKey = PpropFindProp(lpPropArray, cValue, PR_SOURCE_KEY);

	hr = HrGetOneProp(&m_lpFolder->m_xMAPIProp, PR_SOURCE_KEY, &ptrFolderSourceKey);
	if (hr != hrSuccess)
		goto exit;

	if (lpMessageSourceKey != NULL) {
		hr = m_lpFolder->GetMsgStore()->lpTransport->HrEntryIDFromSourceKey(m_lpFolder->GetMsgStore()->m_cbEntryId, m_lpFolder->GetMsgStore()->m_lpEntryId, ptrFolderSourceKey->Value.bin.cb, ptrFolderSourceKey->Value.bin.lpb, lpMessageSourceKey->Value.bin.cb, lpMessageSourceKey->Value.bin.lpb, &cbEntryId, &ptrEntryId);
		if (hr != MAPI_E_NOT_FOUND && hr != hrSuccess) {
			LOG_DEBUG(m_lpLogger, "ImportFast: Failed to get entryid from sourcekey, hr = 0x%08x", hr);
			goto exit;
		}
	} else {
	    // Source key not specified, therefore the message must be new since this is the only thing
	    // we can do if there is no sourcekey. Z-Push uses this, while offline ICS does not (it always
	    // passes a source key)
	    ulFlags |= SYNC_NEW_MESSAGE;
		hr = MAPI_E_NOT_FOUND;
	}

	if (hr == MAPI_E_NOT_FOUND && ((ulFlags & SYNC_NEW_MESSAGE) == 0)) {
		// This is a change, but we don't already have the item. This can only mean
		// that the item has been deleted on our side. 
		LOG_DEBUG(m_lpLogger, "ImportFast: %s", "Destination message deleted");
		hr = SYNC_E_OBJECT_DELETED;
		goto exit;
	}

	if (hr == MAPI_E_NOT_FOUND)
		hr = ImportMessageCreateAsStream(cValue, lpPropArray, &ptrMessageImporter);
	else
		hr = ImportMessageUpdateAsStream(cbEntryId, ptrEntryId, cValue, lpPropArray, &ptrMessageImporter);
	if (hr != hrSuccess) {
		if (hr != SYNC_E_IGNORE && hr != SYNC_E_OBJECT_DELETED)
			LOG_DEBUG(m_lpLogger, "ImportFast: Failed to get MessageImporter, hr = 0x%08x", hr);
		goto exit;
	}

	LOG_DEBUG(m_lpLogger, "ImportFast: %s", "Wrapping MessageImporter in IStreamAdapter");
	hr = ECMessageStreamImporterIStreamAdapter::Create(ptrMessageImporter, &ptrStream);
	if (hr != hrSuccess) {
		LOG_DEBUG(m_lpLogger, "ImportFast: Failed to wrap message importer, hr = 0x%08x" ,hr);
		goto exit;
	}

	*lppStream = ptrStream.release();

exit:
	return hr;
}

HRESULT ECExchangeImportContentsChanges::ImportMessageCreateAsStream(ULONG cValue, LPSPropValue lpPropArray, WSMessageStreamImporter **lppMessageImporter)
{
	HRESULT hr = hrSuccess;
	LPSPropValue lpMessageFlags = NULL;
	LPSPropValue lpMessageAssociated = NULL;
	LPSPropValue lpPropEntryId = NULL;
	ULONG ulNewFlags = 0;
	ULONG cbEntryId = 0;
	LPENTRYID lpEntryId = NULL;
	WSMessageStreamImporterPtr ptrMessageImporter;

	if (lpPropArray == NULL || lppMessageImporter == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	lpMessageFlags = PpropFindProp(lpPropArray, cValue, PR_MESSAGE_FLAGS);
	lpMessageAssociated = PpropFindProp(lpPropArray, cValue, 0x67AA000B); //PR_ASSOCIATED
	lpPropEntryId = PpropFindProp(lpPropArray, cValue, PR_ENTRYID);

	if ((lpMessageFlags != NULL && (lpMessageFlags->Value.ul & MSGFLAG_ASSOCIATED)) || (lpMessageAssociated != NULL && lpMessageAssociated->Value.b))
		ulNewFlags = MAPI_ASSOCIATED;

	if (lpPropEntryId != NULL) {
		cbEntryId = lpPropEntryId->Value.bin.cb;
		lpEntryId = (LPENTRYID)lpPropEntryId->Value.bin.lpb;
	} else {
		LOG_DEBUG(m_lpLogger, "CreateFast: %s", "Creating new entryid");
		hr = HrCreateEntryId(m_lpFolder->GetMsgStore()->GetStoreGuid(), MAPI_MESSAGE, &cbEntryId, &lpEntryId);
		if (hr != hrSuccess) {
			LOG_DEBUG(m_lpLogger, "CreateFast: Failed to create entryid, hr = 0x%08x", hr);
			goto exit;
		}
	}

	hr = m_lpFolder->CreateMessageFromStream(ulNewFlags, m_ulSyncId, cbEntryId, lpEntryId, &ptrMessageImporter);
	if(hr != hrSuccess) {
		LOG_DEBUG(m_lpLogger, "CreateFast: Failed to create message from stream, hr = 0x%08x", hr);
		goto exit;
	}

	*lppMessageImporter = ptrMessageImporter.release();

exit:
	return hr;
}

HRESULT ECExchangeImportContentsChanges::ImportMessageUpdateAsStream(ULONG cbEntryId, LPENTRYID lpEntryId, ULONG cValue, LPSPropValue lpPropArray, WSMessageStreamImporter **lppMessageImporter)
{
	HRESULT hr = hrSuccess;
	SPropValuePtr ptrPropPCL;
	SPropValuePtr ptrPropCK;
	LPSPropValue lpRemoteCK = NULL;
	LPSPropValue lpRemotePCL = NULL;
	LPSPropValue lpMessageFlags = NULL;
	LPSPropValue lpMessageAssociated = NULL;
	bool bAssociated = false;
	SPropValuePtr ptrConflictItems;
	WSMessageStreamImporterPtr ptrMessageImporter;

	if (lpEntryId == NULL || lpPropArray == NULL || lppMessageImporter == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	hr = m_lpFolder->GetChangeInfo(cbEntryId, lpEntryId, &ptrPropPCL, &ptrPropCK);
	if (hr != hrSuccess) {
		if (hr == MAPI_E_NOT_FOUND) {
			// The item was soft-deleted; sourcekey is known, but we cannot open the item. It has therefore been deleted.
			LOG_DEBUG(m_lpLogger, "UpdateFast: %s", "The destination item was deleted");
			hr = SYNC_E_OBJECT_DELETED;
		} else
			LOG_DEBUG(m_lpLogger, "UpdateFast: Failed to get change info, hr = 0x%08x", hr);
		goto exit;
	}

	lpRemoteCK = PpropFindProp(lpPropArray, cValue, PR_CHANGE_KEY);
	if (IsProcessed(lpRemoteCK, ptrPropPCL)) {
		//we already have this change
		LOG_DEBUG(m_lpLogger, "UpdateFast: %s", "The item was previously synchronized");
		hr = SYNC_E_IGNORE;
		goto exit;
	}

	lpMessageFlags = PpropFindProp(lpPropArray, cValue, PR_MESSAGE_FLAGS);
	lpMessageAssociated = PpropFindProp(lpPropArray, cValue, 0x67AA000B); //PR_ASSOCIATED
	if ((lpMessageFlags != NULL && (lpMessageFlags->Value.ul & MSGFLAG_ASSOCIATED)) || (lpMessageAssociated != NULL && lpMessageAssociated->Value.b))
		bAssociated = true;

	lpRemotePCL = PpropFindProp(lpPropArray, cValue, PR_PREDECESSOR_CHANGE_LIST);
	if (!bAssociated && IsConflict(ptrPropCK, lpRemotePCL)) {
		MessagePtr ptrMessage;
		ULONG ulType = 0;

		LOG_DEBUG(m_lpLogger, "UpdateFast: %s", "The item seems to be in conflict");

		hr = m_lpFolder->OpenEntry(cbEntryId, lpEntryId, &ptrMessage.iid, MAPI_MODIFY, &ulType, &ptrMessage);
		if (hr == MAPI_E_NOT_FOUND) {
			// This shouldn't happen as we just got a conflict.
			LOG_DEBUG(m_lpLogger, "UpdateFast: %s", "The destination item seems to have disappeared");
			hr = SYNC_E_OBJECT_DELETED;
			goto exit;
		} else if (hr != hrSuccess) {
			LOG_DEBUG(m_lpLogger, "UpdateFast: Failed to open conflicting message, hr = 0x%08x", hr);
			goto exit;
		}

		if (CreateConflictMessageOnly(ptrMessage, &ptrConflictItems) == MAPI_E_NOT_FOUND) {
			hr = CreateConflictFolders();
			if (hr != hrSuccess) {
				LOG_DEBUG(m_lpLogger, "UpdateFast: Failed to create conflict folders, hr = 0x%08x", hr);
				goto exit;
			}

			hr = CreateConflictMessageOnly(ptrMessage, &ptrConflictItems);
			if (hr != hrSuccess) {
				LOG_DEBUG(m_lpLogger, "UpdateFast: Failed to create conflict message, hr = 0x%08x", hr);
				goto exit;
			}
		}
	}

	hr = m_lpFolder->UpdateMessageFromStream(m_ulSyncId, cbEntryId, lpEntryId, ptrConflictItems, &ptrMessageImporter);
	if (hr != hrSuccess) {
		LOG_DEBUG(m_lpLogger, "UpdateFast: Failed to update message from stream, hr = 0x%08x", hr);
		goto exit;
	}

	*lppMessageImporter = ptrMessageImporter.release();

exit:
	return hr;
}

HRESULT ECExchangeImportContentsChanges::SetMessageInterface(REFIID refiid)
{
	m_iidMessage = refiid;
	return hrSuccess;
}


/**
 * Check if the passed entryids can be found in the RES_PROPERTY restrictions with the proptag
 * set to PR_PARENT_ENTRYID at any level in the passed restriction.
 *
 * @param[in]		lpRestriction	The restriction in which to look for the entryids.
 * @param[in,out]	lstEntryIds		The list of entryids to find. If an entryid is found it
 *									will be removed from the list.
 *
 * @retval	hrSuccess			All entries from the list are found. The list will be empty on exit.
 * @retval	MAPI_E_NOT_FOUND	Not all entries from the list were found.
 */
HRESULT HrRestrictionContains(LPSRestriction lpRestriction, std::list<SBinary> &lstEntryIds)
{
	typedef std::list<SBinary>::iterator iterator;

	HRESULT hr = MAPI_E_NOT_FOUND;

	switch (lpRestriction->rt) {
		case RES_AND:
			for (ULONG i = 0; hr != hrSuccess && i < lpRestriction->res.resAnd.cRes; ++i)
				hr = HrRestrictionContains(&lpRestriction->res.resAnd.lpRes[i], lstEntryIds);
			break;

		case RES_OR:
			for (ULONG i = 0; hr != hrSuccess && i < lpRestriction->res.resOr.cRes; ++i)
				hr = HrRestrictionContains(&lpRestriction->res.resOr.lpRes[i], lstEntryIds);
			break;

		case RES_NOT:
			hr = HrRestrictionContains(lpRestriction->res.resNot.lpRes, lstEntryIds);
			break;

		case RES_PROPERTY:
			if (lpRestriction->res.resProperty.ulPropTag == PR_PARENT_ENTRYID) {
				for (iterator i = lstEntryIds.begin(); i != lstEntryIds.end(); ++i) {
					if (Util::CompareSBinary(lpRestriction->res.resProperty.lpProp->Value.bin, *i) == 0) {
						lstEntryIds.erase(i);
						break;
					}
				}
				if (lstEntryIds.empty())
					hr = hrSuccess;
			}
			break;

		default:
			break;
	}

	return hr;
}

/**
 * Check if the restriction passed in lpRestriction contains the three conflict
 * folders as specified in lpAdditionalREN. If either of the three entryid's in
 * lpAdditionalREN is empty, the restriction won't be checked and it will be assumed
 * to be valid.
 *
 * @param[in]	lpRestriction		The restriction that is to be verified.
 * @param[in]	lpAdditionalREN		An MV_BINARY property that contains the entryids of the three
 *									three conflict folders.
 *
 * @retval	hrSuccess			The restriction is valid for the passed AdditionalREN. This means that
 *								it either contains the three entryids or that at least one of the entryids
 *								is empty.
 * @retval	MAPI_E_NOT_FOUND	lpAdditionalREN contains all three entryids, but not all of them
 *								were found in lpRestriction.
 */
HRESULT HrVerifyRemindersRestriction(LPSRestriction lpRestriction, LPSPropValue lpAdditionalREN)
{
	std::list<SBinary> lstEntryIds;

	if (lpAdditionalREN->Value.MVbin.lpbin[0].cb == 0 || lpAdditionalREN->Value.MVbin.lpbin[2].cb == 0 || lpAdditionalREN->Value.MVbin.lpbin[3].cb == 0)
		return hrSuccess;

	lstEntryIds.push_back(lpAdditionalREN->Value.MVbin.lpbin[0]);
	lstEntryIds.push_back(lpAdditionalREN->Value.MVbin.lpbin[2]);
	lstEntryIds.push_back(lpAdditionalREN->Value.MVbin.lpbin[3]);

	return HrRestrictionContains(lpRestriction, lstEntryIds);
}

HRESULT ECExchangeImportContentsChanges::HrUpdateSearchReminders(LPMAPIFOLDER lpRootFolder, LPSPropValue lpAdditionalREN)
{
	HRESULT hr = hrSuccess;
	ULONG cREMProps;
	SPropArrayPtr ptrREMProps;
	LPSPropValue lpREMEntryID = NULL;
	MAPIFolderPtr ptrRemindersFolder;
	ULONG ulType = 0;
	SRestrictionPtr ptrOrigRestriction;
	EntryListPtr ptrOrigContainerList;
	ULONG ulOrigSearchState = 0;
	SRestrictionPtr ptrPreRestriction;
	ECAndRestriction resPre;
	SPropValue sPropValConflicts = {PR_PARENT_ENTRYID, 0};
	SPropValue sPropValLocalFailures = {PR_PARENT_ENTRYID, 0};
	SPropValue sPropValServerFailures = {PR_PARENT_ENTRYID, 0};

	SizedSPropTagArray(2, sptaREMProps) = {2, {PR_REM_ONLINE_ENTRYID, PR_REM_OFFLINE_ENTRYID}};

	hr = lpRootFolder->GetProps((LPSPropTagArray)&sptaREMProps, 0, &cREMProps, &ptrREMProps);
	if (FAILED(hr))
		goto exit;

	// Find the correct reminders folder.
	if (PROP_TYPE(ptrREMProps[1].ulPropTag) != PT_ERROR)
		lpREMEntryID = &ptrREMProps[1];
	else if (PROP_TYPE(ptrREMProps[0].ulPropTag) != PT_ERROR)
		lpREMEntryID = &ptrREMProps[0];
	else {
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}

	hr = lpRootFolder->OpenEntry(lpREMEntryID->Value.bin.cb, (LPENTRYID)lpREMEntryID->Value.bin.lpb, &ptrRemindersFolder.iid, MAPI_BEST_ACCESS, &ulType, &ptrRemindersFolder);
	if (hr != hrSuccess)
		goto exit;


	hr = ptrRemindersFolder->GetSearchCriteria(0, &ptrOrigRestriction, &ptrOrigContainerList, &ulOrigSearchState);
	if (hr != hrSuccess)
		goto exit;


	// First check if the SearchCriteria needs updating by seeing if we can find the restrictions that
	// contain the entryids of the folders to exclude. We assume that when they're found they're used
	// as expected: as excludes.
	hr = HrVerifyRemindersRestriction(ptrOrigRestriction, lpAdditionalREN);
	if (hr == hrSuccess)
		goto exit;


	sPropValConflicts.Value.bin = lpAdditionalREN->Value.MVbin.lpbin[0];
	sPropValLocalFailures.Value.bin = lpAdditionalREN->Value.MVbin.lpbin[2];
	sPropValServerFailures.Value.bin = lpAdditionalREN->Value.MVbin.lpbin[3];

	resPre.append(
		ECPropertyRestriction(RELOP_NE, PR_PARENT_ENTRYID, &sPropValConflicts, ECRestriction::Cheap) +
		ECPropertyRestriction(RELOP_NE, PR_PARENT_ENTRYID, &sPropValLocalFailures, ECRestriction::Cheap) +
		ECPropertyRestriction(RELOP_NE, PR_PARENT_ENTRYID, &sPropValServerFailures, ECRestriction::Cheap) +
		ECRawRestriction(ptrOrigRestriction.get(), ECRestriction::Cheap)
	);

	hr = resPre.CreateMAPIRestriction(&ptrPreRestriction, ECRestriction::Cheap);
	if (hr != hrSuccess)
		goto exit;

	hr = ptrRemindersFolder->SetSearchCriteria(ptrPreRestriction, ptrOrigContainerList, RESTART_SEARCH | (ulOrigSearchState & (SEARCH_FOREGROUND | SEARCH_RECURSIVE)));
	if (hr != hrSuccess)
		goto exit;

exit:
	return hr;
}

ULONG ECExchangeImportContentsChanges::xECImportContentsChanges::AddRef(){
	TRACE_MAPI(TRACE_ENTRY, "IECImportContentsChanges::AddRef", "");
	METHOD_PROLOGUE_(ECExchangeImportContentsChanges, ECImportContentsChanges);
	return pThis->AddRef();
}

ULONG ECExchangeImportContentsChanges::xECImportContentsChanges::Release()
{
	TRACE_MAPI(TRACE_ENTRY, "IECImportContentsChanges::Release", "");
	METHOD_PROLOGUE_(ECExchangeImportContentsChanges, ECImportContentsChanges);
	return pThis->Release();
}

HRESULT ECExchangeImportContentsChanges::xECImportContentsChanges::QueryInterface(REFIID refiid, void **lppInterface)
{
	TRACE_MAPI(TRACE_ENTRY, "IECImportContentsChanges::QueryInterface", "%s", DBGGUIDToString(refiid).c_str());
	METHOD_PROLOGUE_(ECExchangeImportContentsChanges, ECImportContentsChanges);
	return pThis->QueryInterface(refiid, lppInterface);
}

HRESULT ECExchangeImportContentsChanges::xECImportContentsChanges::GetLastError(HRESULT hError, ULONG ulFlags, LPMAPIERROR * lppMapiError)
{
	TRACE_MAPI(TRACE_ENTRY, "IECImportContentsChanges::GetLastError", "");
	METHOD_PROLOGUE_(ECExchangeImportContentsChanges, ECImportContentsChanges);
	return pThis->GetLastError(hError, ulFlags, lppMapiError);
}

HRESULT ECExchangeImportContentsChanges::xECImportContentsChanges::Config(LPSTREAM lpStream, ULONG ulFlags){
	TRACE_MAPI(TRACE_ENTRY, "IECImportContentsChanges::Config", "");
	METHOD_PROLOGUE_(ECExchangeImportContentsChanges, ECImportContentsChanges);
	return pThis->Config(lpStream, ulFlags);
}

HRESULT ECExchangeImportContentsChanges::xECImportContentsChanges::UpdateState(LPSTREAM lpStream){
	TRACE_MAPI(TRACE_ENTRY, "IECImportContentsChanges::UpdateState", "");
	METHOD_PROLOGUE_(ECExchangeImportContentsChanges, ECImportContentsChanges);
	return pThis->UpdateState(lpStream);
}

HRESULT ECExchangeImportContentsChanges::xECImportContentsChanges::ImportMessageChange(ULONG cValue, LPSPropValue lpPropArray, ULONG ulFlags, LPMESSAGE * lppMessage){
	TRACE_MAPI(TRACE_ENTRY, "IECImportContentsChanges::ImportMessageChange", "lpPropArray\n%s", PropNameFromPropArray(cValue,lpPropArray).c_str());
	METHOD_PROLOGUE_(ECExchangeImportContentsChanges, ECImportContentsChanges);
	return pThis->ImportMessageChange(cValue, lpPropArray, ulFlags, lppMessage);
}

HRESULT ECExchangeImportContentsChanges::xECImportContentsChanges::ImportMessageDeletion(ULONG ulFlags, LPENTRYLIST lpSourceEntryList){
	TRACE_MAPI(TRACE_ENTRY, "IECImportContentsChanges::ImportMessageDeletion", "lpSourceEntryList\n%s", EntryListToString(lpSourceEntryList).c_str());
	METHOD_PROLOGUE_(ECExchangeImportContentsChanges, ECImportContentsChanges);
	return pThis->ImportMessageDeletion(ulFlags, lpSourceEntryList);
}

HRESULT ECExchangeImportContentsChanges::xECImportContentsChanges::ImportPerUserReadStateChange(ULONG cElements, LPREADSTATE lpReadState){
	TRACE_MAPI(TRACE_ENTRY, "IECImportContentsChanges::ImportPerUserReadStateChange", "");
	METHOD_PROLOGUE_(ECExchangeImportContentsChanges, ECImportContentsChanges);
	return pThis->ImportPerUserReadStateChange(cElements, lpReadState);
}

HRESULT ECExchangeImportContentsChanges::xECImportContentsChanges::ImportMessageMove(ULONG cbSourceKeySrcFolder, BYTE FAR * pbSourceKeySrcFolder, ULONG cbSourceKeySrcMessage, BYTE FAR * pbSourceKeySrcMessage, ULONG cbPCLMessage, BYTE FAR * pbPCLMessage, ULONG cbSourceKeyDestMessage, BYTE FAR * pbSourceKeyDestMessage, ULONG cbChangeNumDestMessage, BYTE FAR * pbChangeNumDestMessage){
	TRACE_MAPI(TRACE_ENTRY, "IECImportContentsChanges::ImportMessageMove", "");
	METHOD_PROLOGUE_(ECExchangeImportContentsChanges, ECImportContentsChanges);
	return pThis->ImportMessageMove(cbSourceKeySrcFolder, pbSourceKeySrcFolder, cbSourceKeySrcMessage, pbSourceKeySrcMessage, cbPCLMessage, pbPCLMessage, cbSourceKeyDestMessage, pbSourceKeyDestMessage, cbChangeNumDestMessage, pbChangeNumDestMessage);
}

HRESULT ECExchangeImportContentsChanges::xECImportContentsChanges::ConfigForConversionStream(LPSTREAM lpStream, ULONG ulFlags, ULONG cValuesConversion, LPSPropValue lpPropArrayConversion)
{
	TRACE_MAPI(TRACE_ENTRY, "IECImportContentsChanges::ConfigureForConversionStream", "");
	METHOD_PROLOGUE_(ECExchangeImportContentsChanges, ECImportContentsChanges);
	return pThis->ConfigForConversionStream(lpStream, ulFlags, cValuesConversion, lpPropArrayConversion);
}

HRESULT ECExchangeImportContentsChanges::xECImportContentsChanges::ImportMessageChangeAsAStream(ULONG cpvalChanges, LPSPropValue ppvalChanges, ULONG ulFlags, LPSTREAM *lppstream)
{
	TRACE_MAPI(TRACE_ENTRY, "IECImportContentsChanges2::ImportMessageChangeAsAStream", "");
	METHOD_PROLOGUE_(ECExchangeImportContentsChanges, ECImportContentsChanges);
	return pThis->ImportMessageChangeAsAStream(cpvalChanges, ppvalChanges, ulFlags, lppstream);
}

HRESULT ECExchangeImportContentsChanges::xECImportContentsChanges::SetMessageInterface(REFIID refiid)
{
	HRESULT hr;
	TRACE_MAPI(TRACE_ENTRY, "IExchangeImportContentsChanges::SetMessageInterface", "%s", DBGGUIDToString(refiid).c_str());
	METHOD_PROLOGUE_(ECExchangeImportContentsChanges, ECImportContentsChanges);
	hr = pThis->SetMessageInterface(refiid);
	return hr;
}
