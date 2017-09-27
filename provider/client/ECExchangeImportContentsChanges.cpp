/*
 * Copyright 2005 - 2016 Zarafa and its licensors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License, version 3,
 * as published by the Free Software Foundation.
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
#include <new>
#include <kopano/zcdefs.h>
#include <kopano/platform.h>
#include <kopano/memory.hpp>
#include "ECExchangeImportContentsChanges.h"
#include "WSMessageStreamImporter.h"
#include "ECMessageStreamImporterIStreamAdapter.h"
#include <kopano/ECLogger.h>
#include "ECSyncLog.h"

#include <kopano/Util.h>
#include <edkguid.h>
#include <kopano/ECGuid.h>
#include <mapiguid.h>
#include "ECMessage.h"
#include <kopano/ECDebug.h>

#include "ics.h"
#include <kopano/mapiext.h>
#include <kopano/mapi_ptr.h>
#include <kopano/ECRestriction.h>

#include <mapiutil.h>

#include <kopano/charset/convert.h>
#include <kopano/ECGetText.h>
#include "EntryPoint.h"

#include <list>

using namespace KCHL;

ECExchangeImportContentsChanges::ECExchangeImportContentsChanges(ECMAPIFolder *lpFolder) :
	m_lpFolder(lpFolder)
{
	ECSyncLog::GetLogger(&m_lpLogger);
	m_lpFolder->AddRef();
}

ECExchangeImportContentsChanges::~ECExchangeImportContentsChanges(){
	m_lpFolder->Release();
	m_lpLogger->Release();
	MAPIFreeBuffer(m_lpSourceKey);
}

HRESULT ECExchangeImportContentsChanges::Create(ECMAPIFolder *lpFolder, LPEXCHANGEIMPORTCONTENTSCHANGES* lppExchangeImportContentsChanges){
	HRESULT hr;
	if(!lpFolder)
		return MAPI_E_INVALID_PARAMETER;
	object_ptr<ECExchangeImportContentsChanges> lpEICC(new(std::nothrow) ECExchangeImportContentsChanges(lpFolder));
	if (lpEICC == nullptr)
		return MAPI_E_NOT_ENOUGH_MEMORY;
	hr = HrGetOneProp(lpFolder, PR_SOURCE_KEY, &lpEICC->m_lpSourceKey);
	if (hr != hrSuccess)
		return hr;
	return lpEICC->QueryInterface(IID_IExchangeImportContentsChanges,
	       reinterpret_cast<void **>(lppExchangeImportContentsChanges));
}

HRESULT	ECExchangeImportContentsChanges::QueryInterface(REFIID refiid, void **lppInterface)
{
	BOOL	bCanStream = FALSE;

	REGISTER_INTERFACE2(ECExchangeImportContentsChanges, this);
	REGISTER_INTERFACE2(ECUnknown, this);

	if (refiid == IID_IECImportContentsChanges) {
		m_lpFolder->GetMsgStore()->lpTransport->HrCheckCapabilityFlags(KOPANO_CAP_ENHANCED_ICS, &bCanStream);
		if (bCanStream == FALSE)
			return MAPI_E_INTERFACE_NOT_SUPPORTED;
		REGISTER_INTERFACE2(IECImportContentsChanges, this);
	}
	REGISTER_INTERFACE2(IExchangeImportContentsChanges, this);
	REGISTER_INTERFACE2(IUnknown, this);
	return MAPI_E_INTERFACE_NOT_SUPPORTED;
}

HRESULT ECExchangeImportContentsChanges::GetLastError(HRESULT hResult, ULONG ulFlags, LPMAPIERROR *lppMAPIError){
	HRESULT		hr = hrSuccess;
	LPMAPIERROR	lpMapiError = NULL;
	memory_ptr<TCHAR> lpszErrorMsg;
	
	//FIXME: give synchronization errors messages
	hr = Util::HrMAPIErrorToText((hResult == hrSuccess)?MAPI_E_NO_ACCESS : hResult, &~lpszErrorMsg);
	if (hr != hrSuccess)
		return hr;
	if ((hr = MAPIAllocateBuffer(sizeof(MAPIERROR),(void **)&lpMapiError)) != hrSuccess)
		return hr;
	
	if (ulFlags & MAPI_UNICODE) {
		std::wstring wstrErrorMsg = convert_to<std::wstring>(lpszErrorMsg.get());
		std::wstring wstrCompName = convert_to<std::wstring>(g_strProductName.c_str());

		if ((hr = MAPIAllocateMore(sizeof(std::wstring::value_type) * (wstrErrorMsg.size() + 1), lpMapiError, (void**)&lpMapiError->lpszError)) != hrSuccess)
			return hr;
		wcscpy((wchar_t*)lpMapiError->lpszError, wstrErrorMsg.c_str());

		if ((hr = MAPIAllocateMore(sizeof(std::wstring::value_type) * (wstrCompName.size() + 1), lpMapiError, (void**)&lpMapiError->lpszComponent)) != hrSuccess)
			return hr;
		wcscpy((wchar_t*)lpMapiError->lpszComponent, wstrCompName.c_str()); 

	} else {
		std::string strErrorMsg = convert_to<std::string>(lpszErrorMsg.get());
		std::string strCompName = convert_to<std::string>(g_strProductName.c_str());

		if ((hr = MAPIAllocateMore(strErrorMsg.size() + 1, lpMapiError, (void**)&lpMapiError->lpszError)) != hrSuccess)
			return hr;
		strcpy((char*)lpMapiError->lpszError, strErrorMsg.c_str());

		if ((hr = MAPIAllocateMore(strCompName.size() + 1, lpMapiError, (void**)&lpMapiError->lpszComponent)) != hrSuccess)
			return hr;
		strcpy((char*)lpMapiError->lpszComponent, strCompName.c_str());
	}

	lpMapiError->ulContext		= 0;
	lpMapiError->ulLowLevelError= 0;
	lpMapiError->ulVersion		= 0;

	*lppMAPIError = lpMapiError;
	return hrSuccess;
}

HRESULT ECExchangeImportContentsChanges::Config(LPSTREAM lpStream, ULONG ulFlags){
	HRESULT hr = hrSuccess;
	LARGE_INTEGER zero = {{0,0}};
	ULONG ulLen = 0;
	
	m_lpStream = lpStream;

	if(lpStream == NULL) {
		m_ulSyncId = 0;
		m_ulChangeId = 0;
	} else {
		hr = lpStream->Seek(zero, STREAM_SEEK_SET, NULL);
		if(hr != hrSuccess)
			return hr;
		
		hr = lpStream->Read(&m_ulSyncId, 4, &ulLen);
		if(hr != hrSuccess)
			return hr;
		if (ulLen != 4)
			return MAPI_E_INVALID_PARAMETER;
		
		hr = lpStream->Read(&m_ulChangeId, 4, &ulLen);
		if(hr != hrSuccess)
			return hr;
		if (ulLen != 4)
			return MAPI_E_INVALID_PARAMETER;
		
		// The user specified the special sync key '0000000000000000', get a sync key from the server.
		if(m_ulSyncId == 0) {
			hr = m_lpFolder->GetMsgStore()->lpTransport->HrSetSyncStatus(std::string((char *)m_lpSourceKey->Value.bin.lpb, m_lpSourceKey->Value.bin.cb), m_ulSyncId, m_ulChangeId, ICS_SYNC_CONTENTS, 0, &m_ulSyncId);
			if(hr != hrSuccess)
				return hr;
		}
		
		// The sync key we got from the server can be used to retrieve all items in the database now when given to IEEC->Config(). At the same time, any
		// items written to this importer will send the sync ID to the server so that any items written here will not be returned by the exporter,
		// preventing local looping of items.
	}		
		
	m_ulFlags = ulFlags;
	return hrSuccess;
}

HRESULT ECExchangeImportContentsChanges::UpdateState(LPSTREAM lpStream){
	HRESULT hr;
	LARGE_INTEGER zero = {{0,0}};
	ULONG ulLen = 0;

	if(lpStream == NULL) {
		if (m_lpStream == NULL)
			return hrSuccess;
		lpStream = m_lpStream;
	}

	if(m_ulSyncId == 0)
		return hrSuccess; // config() called with NULL stream, so we'll ignore the UpdateState()
	
	hr = lpStream->Seek(zero, STREAM_SEEK_SET, NULL);
	if(hr != hrSuccess)
		return hr;

	hr = lpStream->Write(&m_ulSyncId, 4, &ulLen);
	if(hr != hrSuccess)
		return hr;
	return lpStream->Write(&m_ulChangeId, 4, &ulLen);
}

HRESULT ECExchangeImportContentsChanges::ImportMessageChange(ULONG cValue, LPSPropValue lpPropArray, ULONG ulFlags, LPMESSAGE * lppMessage){
	HRESULT hr = hrSuccess; 
	memory_ptr<SPropValue> lpPropPCL, lpPropCK;
	ULONG cbEntryId = 0;
	memory_ptr<ENTRYID> lpEntryId;
	auto lpMessageSourceKey = PCpropFindProp(lpPropArray, cValue, PR_SOURCE_KEY);
	auto lpMessageFlags = PCpropFindProp(lpPropArray, cValue, PR_MESSAGE_FLAGS);
	auto lpMessageAssociated = PCpropFindProp(lpPropArray, cValue, PR_ASSOCIATED);
	auto lpRemotePCL = PCpropFindProp(lpPropArray, cValue, PR_PREDECESSOR_CHANGE_LIST);
	auto lpRemoteCK = PCpropFindProp(lpPropArray, cValue, PR_CHANGE_KEY);

	ULONG ulObjType = 0;
	bool bAssociatedMessage = false;
	object_ptr<IMessage> lpMessage;
	object_ptr<ECMessage> lpECMessage;
	ULONG ulNewFlags = 0;

	if(lpMessageSourceKey != NULL) {
		hr = m_lpFolder->GetMsgStore()->lpTransport->HrEntryIDFromSourceKey(m_lpFolder->GetMsgStore()->m_cbEntryId, m_lpFolder->GetMsgStore()->m_lpEntryId, m_lpSourceKey->Value.bin.cb, m_lpSourceKey->Value.bin.lpb, lpMessageSourceKey->Value.bin.cb, lpMessageSourceKey->Value.bin.lpb, &cbEntryId, &~lpEntryId);
		if(hr != MAPI_E_NOT_FOUND && hr != hrSuccess)
			return hr;
	} else {
	    // Source key not specified, therefore the message must be new since this is the only thing
	    // we can do if there is no sourcekey. Z-Push uses this, while offline ICS does not (it always
	    // passes a source key)
	    ulFlags |= SYNC_NEW_MESSAGE;
		hr = MAPI_E_NOT_FOUND;
	}

	if (hr == MAPI_E_NOT_FOUND && (ulFlags & SYNC_NEW_MESSAGE) == 0)
		// This is a change, but we don't already have the item. This can only mean
		// that the item has been deleted on our side. 
		return SYNC_E_OBJECT_DELETED;

	if ((lpMessageFlags != NULL &&
	    (lpMessageFlags->Value.ul & MSGFLAG_ASSOCIATED)) ||
	    (lpMessageAssociated != NULL && lpMessageAssociated->Value.b))
		bAssociatedMessage = true;

	if(hr == MAPI_E_NOT_FOUND){
		if (bAssociatedMessage)
		    ulNewFlags = MAPI_ASSOCIATED;
		else
		    ulNewFlags = 0;

		auto lpPassedEntryId = PCpropFindProp(lpPropArray, cValue, PR_ENTRYID);
		// Create the message with the passed entry ID
		if(lpPassedEntryId)
			hr = m_lpFolder->CreateMessageWithEntryID(&IID_IMessage, ulNewFlags, lpPassedEntryId->Value.bin.cb, reinterpret_cast<ENTRYID *>(lpPassedEntryId->Value.bin.lpb), &~lpMessage);
		else
			hr = m_lpFolder->CreateMessage(&IID_IMessage, ulNewFlags, &~lpMessage);

		if(hr != hrSuccess)
			return hr;
	}else{
		hr = m_lpFolder->OpenEntry(cbEntryId, lpEntryId, &IID_IMessage, MAPI_MODIFY, &ulObjType, &~lpMessage);
		if (hr == MAPI_E_NOT_FOUND)
			// The item was soft-deleted; sourcekey is known, but we cannot open the item. It has therefore been deleted.
			return SYNC_E_OBJECT_DELETED;
		if(hr != hrSuccess)
			return hr;

		if (IsProcessed(lpRemoteCK, lpPropPCL))
			//we already have this change
			return SYNC_E_IGNORE;
		
		// Check for conflicts except for associated messages, take always the lastone
		if (bAssociatedMessage == false &&
		    HrGetOneProp(lpMessage, PR_CHANGE_KEY, &~lpPropCK) == hrSuccess &&
		    IsConflict(lpPropCK, lpRemotePCL) &&
		    CreateConflictMessage(lpMessage) == MAPI_E_NOT_FOUND){
			CreateConflictFolders();
			CreateConflictMessage(lpMessage);
		}
	}

	hr = lpMessage->QueryInterface(IID_ECMessage, &~lpECMessage);
	if(hr != hrSuccess)
		return hr;
	hr = lpECMessage->HrSetSyncId(m_ulSyncId);
	if(hr != hrSuccess)
		return hr;

	// Mark as ICS object
	hr = lpECMessage->SetICSObject(TRUE);
	if(hr != hrSuccess)
		return hr;
	hr = lpMessage->SetProps(cValue, lpPropArray, NULL);
	if(hr != hrSuccess)
		return hr;
	*lppMessage = lpMessage;
	lpMessage->AddRef();
	return hrSuccess;
}

//ulFlags = SYNC_SOFT_DELETE, SYNC_EXPIRY
HRESULT ECExchangeImportContentsChanges::ImportMessageDeletion(ULONG ulFlags, LPENTRYLIST lpSourceEntryList){
	HRESULT hr = hrSuccess;
	ENTRYLIST EntryList;
	ULONG ulSKNr;
	EntryList.lpbin = NULL;
	EntryList.cValues = 0;
	
	if ((hr = MAPIAllocateBuffer(sizeof(SBinary)* lpSourceEntryList->cValues, (LPVOID*)&EntryList.lpbin)) != hrSuccess)
		goto exit;

	for (ulSKNr = 0; ulSKNr < lpSourceEntryList->cValues; ++ulSKNr) {
		hr = m_lpFolder->GetMsgStore()->lpTransport->HrEntryIDFromSourceKey(m_lpFolder->GetMsgStore()->m_cbEntryId, m_lpFolder->GetMsgStore()->m_lpEntryId, m_lpSourceKey->Value.bin.cb, m_lpSourceKey->Value.bin.lpb, lpSourceEntryList->lpbin[ulSKNr].cb, lpSourceEntryList->lpbin[ulSKNr].lpb, &EntryList.lpbin[EntryList.cValues].cb, (LPENTRYID*)&EntryList.lpbin[EntryList.cValues].lpb);
		if(hr == MAPI_E_NOT_FOUND){
			hr = hrSuccess;
			continue;
		}
		if(hr != hrSuccess)
			goto exit;
		++EntryList.cValues;
	}
	
	if(EntryList.cValues == 0)
		goto exit;

	hr = m_lpFolder->GetMsgStore()->lpTransport->HrDeleteObjects(ulFlags & SYNC_SOFT_DELETE ? 0 : DELETE_HARD_DELETE, &EntryList, m_ulSyncId);
	if(hr != hrSuccess)
		goto exit;

exit:
	if(EntryList.lpbin){
		for (ulSKNr = 0; ulSKNr < EntryList.cValues; ++ulSKNr)
			MAPIFreeBuffer(EntryList.lpbin[ulSKNr].lpb);
		MAPIFreeBuffer(EntryList.lpbin);
	}
	return hr;
}

HRESULT ECExchangeImportContentsChanges::ImportPerUserReadStateChange(ULONG cElements, LPREADSTATE lpReadState){
	HRESULT hr = hrSuccess;	
	ULONG ulSKNr, cbEntryId;

	for (ulSKNr = 0; ulSKNr < cElements; ++ulSKNr) {
		memory_ptr<ENTRYID> lpEntryId;

		hr = m_lpFolder->GetMsgStore()->lpTransport->HrEntryIDFromSourceKey(m_lpFolder->GetMsgStore()->m_cbEntryId, m_lpFolder->GetMsgStore()->m_lpEntryId , m_lpSourceKey->Value.bin.cb, m_lpSourceKey->Value.bin.lpb, lpReadState[ulSKNr].cbSourceKey, lpReadState[ulSKNr].pbSourceKey, &cbEntryId, &~lpEntryId);
		if(hr == MAPI_E_NOT_FOUND){
			hr = hrSuccess;
			continue; // Message is delete or moved
		}
		if (hr != hrSuccess)
			return hr;
		hr = m_lpFolder->GetMsgStore()->lpTransport->HrSetReadFlag(cbEntryId, lpEntryId, lpReadState[ulSKNr].ulFlags & MSGFLAG_READ ? 0 : CLEAR_READ_FLAG, m_ulSyncId);
		if(hr != hrSuccess)
			return hr;
	}
	return hrSuccess;
}

HRESULT ECExchangeImportContentsChanges::ImportMessageMove(ULONG cbSourceKeySrcFolder, BYTE *pbSourceKeySrcFolder, ULONG cbSourceKeySrcMessage, BYTE *pbSourceKeySrcMessage, ULONG cbPCLMessage, BYTE *pbPCLMessage, ULONG cbSourceKeyDestMessage, BYTE *pbSourceKeyDestMessage, ULONG cbChangeNumDestMessage, BYTE *pbChangeNumDestMessage){
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
bool ECExchangeImportContentsChanges::IsProcessed(const SPropValue *lpRemoteCK,
    const SPropValue *lpLocalPCL)
{
	if (!lpRemoteCK || !lpLocalPCL)
		return false;

	assert(lpRemoteCK->ulPropTag == PR_CHANGE_KEY);
	assert(lpLocalPCL->ulPropTag == PR_PREDECESSOR_CHANGE_LIST);

	const std::string strChangeList((char*)lpLocalPCL->Value.bin.lpb, lpLocalPCL->Value.bin.cb);
	size_t ulPos = 0;
	while (ulPos < strChangeList.size()) {
		size_t ulSize = strChangeList.at(ulPos++);
		if (ulSize <= sizeof(GUID))
			break;
		else if (lpRemoteCK->Value.bin.cb > sizeof(GUID) &&
		    memcmp(strChangeList.data() + ulPos, lpRemoteCK->Value.bin.lpb, sizeof(GUID)) == 0 &&
		    ulSize == lpRemoteCK->Value.bin.cb &&
		    memcmp(strChangeList.data() + ulPos, lpRemoteCK->Value.bin.lpb, ulSize) == 0)
			//remote changekey in our changelist
			//we already have this change
			return true;
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
bool ECExchangeImportContentsChanges::IsConflict(const SPropValue *lpLocalCK,
    const SPropValue *lpRemotePCL)
{
	if (!lpLocalCK || !lpRemotePCL)
		return false;

	assert(lpLocalCK->ulPropTag == PR_CHANGE_KEY);
	assert(lpRemotePCL->ulPropTag == PR_PREDECESSOR_CHANGE_LIST);
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
	memory_ptr<SPropValue> lpConflictItems;

	hr = CreateConflictMessageOnly(lpMessage, &~lpConflictItems);
	if (hr != hrSuccess)
		return hr;
	hr = HrSetOneProp(lpMessage, lpConflictItems);
	if(hr != hrSuccess)
		return hr;
	return lpMessage->SaveChanges(KEEP_OPEN_READWRITE);
}

HRESULT ECExchangeImportContentsChanges::CreateConflictMessageOnly(LPMESSAGE lpMessage, LPSPropValue *lppConflictItems)
{
	HRESULT hr = hrSuccess;
	object_ptr<IMAPIFolder> lpRootFolder, lpConflictFolder;
	object_ptr<IMessage> lpConflictMessage;
	memory_ptr<SPropValue> lpPropAdditionalREN;
	memory_ptr<SPropValue> lpConflictItems, lpEntryIdProp;
	LPSBinary lpEntryIds = NULL;
	ULONG ulCount = 0;
	ULONG ulObjType = 0;
	static constexpr const SizedSPropTagArray(5, excludeProps) =
		{5, {PR_ENTRYID, PR_CONFLICT_ITEMS, PR_SOURCE_KEY,
		PR_CHANGE_KEY, PR_PREDECESSOR_CHANGE_LIST}};

	//open the conflicts folder
	hr = m_lpFolder->GetMsgStore()->OpenEntry(0, nullptr, &IID_IMAPIFolder, 0, &ulObjType, &~lpRootFolder);
	if(hr != hrSuccess)
		return hr;
	hr = HrGetOneProp(lpRootFolder, PR_ADDITIONAL_REN_ENTRYIDS, &~lpPropAdditionalREN);
	if(hr != hrSuccess)
		return hr;
	if (lpPropAdditionalREN->Value.MVbin.cValues == 0 ||
	    lpPropAdditionalREN->Value.MVbin.lpbin[0].cb == 0)
		return MAPI_E_NOT_FOUND;
	hr = m_lpFolder->GetMsgStore()->OpenEntry(lpPropAdditionalREN->Value.MVbin.lpbin[0].cb, reinterpret_cast<ENTRYID *>(lpPropAdditionalREN->Value.MVbin.lpbin[0].lpb), &IID_IMAPIFolder, MAPI_MODIFY, &ulObjType, &~lpConflictFolder);
	if(hr != hrSuccess)
		return hr;

	//create the conflict message
	hr = lpConflictFolder->CreateMessage(nullptr, 0, &~lpConflictMessage);
	if(hr != hrSuccess)
		return hr;
	hr = lpMessage->CopyTo(0, NULL, excludeProps, 0, NULL, &IID_IMessage,
	     lpConflictMessage, 0, NULL);
	if(hr != hrSuccess)
		return hr;

	//set the entryid from original message in PR_CONFLICT_ITEMS of conflict message
	hr = HrGetOneProp(lpMessage, PR_ENTRYID, &~lpEntryIdProp);
	if(hr != hrSuccess)
		return hr;
	hr = MAPIAllocateBuffer(sizeof(SPropValue), &~lpConflictItems);
	if(hr != hrSuccess)
		return hr;

	lpConflictItems->ulPropTag = PR_CONFLICT_ITEMS;
	lpConflictItems->Value.MVbin.cValues = 1;
	lpConflictItems->Value.MVbin.lpbin = &lpEntryIdProp->Value.bin;

	hr = HrSetOneProp(lpConflictMessage, lpConflictItems);
	if(hr != hrSuccess)
		return hr;
	hr = lpConflictMessage->SaveChanges(KEEP_OPEN_READWRITE);
	if(hr != hrSuccess)
		return hr;

	//add the entryid from the conflict message to the PR_CONFLICT_ITEMS of the original message
	hr = HrGetOneProp(lpConflictMessage, PR_ENTRYID, &~lpEntryIdProp);
	if(hr != hrSuccess)
		return hr;
	if (HrGetOneProp(lpMessage, PR_CONFLICT_ITEMS, &~lpConflictItems) != hrSuccess) {
		hr = MAPIAllocateBuffer(sizeof(SPropValue), &~lpConflictItems);
		if(hr != hrSuccess)
			return hr;
		lpConflictItems->ulPropTag = PR_CONFLICT_ITEMS;
		lpConflictItems->Value.MVbin.cValues = 0;
		lpConflictItems->Value.MVbin.lpbin = NULL;
	}
	
	hr = MAPIAllocateMore(sizeof(SBinary)*(lpConflictItems->Value.MVbin.cValues+1), lpConflictItems, (LPVOID*)&lpEntryIds);
	if(hr != hrSuccess)
		return hr;
	for (ulCount = 0; ulCount < lpConflictItems->Value.MVbin.cValues; ++ulCount) {
		lpEntryIds[ulCount].cb = lpConflictItems->Value.MVbin.lpbin[ulCount].cb;
		lpEntryIds[ulCount].lpb = lpConflictItems->Value.MVbin.lpbin[ulCount].lpb;
	}
	lpEntryIds[ulCount].cb = lpEntryIdProp->Value.bin.cb;
	lpEntryIds[ulCount].lpb = lpEntryIdProp->Value.bin.lpb;

	lpConflictItems->Value.MVbin.lpbin = lpEntryIds;
	++lpConflictItems->Value.MVbin.cValues;
	if (lppConflictItems)
		*lppConflictItems = lpConflictItems.release();
	return hrSuccess;
}

HRESULT ECExchangeImportContentsChanges::CreateConflictFolders(){
	HRESULT hr = hrSuccess;
	object_ptr<IMAPIFolder> lpRootFolder, lpParentFolder, lpInbox, lpConflictFolder;
	memory_ptr<SPropValue> lpAdditionalREN, lpNewAdditionalREN;
	memory_ptr<SPropValue> lpIPMSubTree;
	memory_ptr<ENTRYID> lpEntryId;
	ULONG cbEntryId = 0;
	ULONG ulObjType = 0;
	ULONG ulCount = 0;

	hr = m_lpFolder->OpenEntry(0, nullptr, &IID_IMAPIFolder, MAPI_MODIFY, &ulObjType, &~lpRootFolder);
	if(hr != hrSuccess) {
		ZLOG_DEBUG(m_lpLogger, "Failed to open root folder, hr = 0x%08x", hr);
		return hr;
	}
	hr = m_lpFolder->GetMsgStore()->GetReceiveFolder((TCHAR*)"IPM", 0, &cbEntryId, &~lpEntryId, NULL);
	if(hr != hrSuccess) {
		ZLOG_DEBUG(m_lpLogger, "Failed to get 'IPM' receive folder id, hr = 0x%08x", hr);
		return hr;
	}
	hr = m_lpFolder->OpenEntry(cbEntryId, lpEntryId, &IID_IMAPIFolder, MAPI_MODIFY, &ulObjType, &~lpInbox);
	if(hr != hrSuccess) {
		ZLOG_DEBUG(m_lpLogger, "Failed to open 'IPM' receive folder, hr = 0x%08x", hr);
		return hr;
	}
	hr = HrGetOneProp(m_lpFolder->GetMsgStore(), PR_IPM_SUBTREE_ENTRYID, &~lpIPMSubTree);
	if(hr != hrSuccess) {
		ZLOG_DEBUG(m_lpLogger, "Failed to get ipm subtree id, hr = 0x%08x", hr);
		return hr;
	}
	hr = m_lpFolder->OpenEntry(lpIPMSubTree->Value.bin.cb, reinterpret_cast<ENTRYID *>(lpIPMSubTree->Value.bin.lpb), &IID_IMAPIFolder, MAPI_MODIFY, &ulObjType, &~lpParentFolder);
	if(hr != hrSuccess) {
		ZLOG_DEBUG(m_lpLogger, "Failed to open ipm subtree folder, hr = 0x%08x", hr);
		return hr;
	}

	HrGetOneProp(lpRootFolder, PR_ADDITIONAL_REN_ENTRYIDS, &~lpAdditionalREN);

	//make new PR_ADDITIONAL_REN_ENTRYIDS
	hr = MAPIAllocateBuffer(sizeof(SPropValue), &~lpNewAdditionalREN);
	if(hr != hrSuccess)
		return hr;

	lpNewAdditionalREN->ulPropTag = PR_ADDITIONAL_REN_ENTRYIDS;
	lpNewAdditionalREN->Value.MVbin.cValues = (lpAdditionalREN == nullptr || lpAdditionalREN->Value.MVbin.cValues < 4) ? 4 : lpAdditionalREN->Value.MVbin.cValues;
	hr = MAPIAllocateMore(sizeof(SBinary)*lpNewAdditionalREN->Value.MVbin.cValues, lpNewAdditionalREN, (LPVOID*)&lpNewAdditionalREN->Value.MVbin.lpbin);
	if(hr != hrSuccess)
		return hr;

	//copy from original PR_ADDITIONAL_REN_ENTRYIDS
	if(lpAdditionalREN)
		for (ulCount = 0; ulCount < lpAdditionalREN->Value.MVbin.cValues; ++ulCount)
			lpNewAdditionalREN->Value.MVbin.lpbin[ulCount] = lpAdditionalREN->Value.MVbin.lpbin[ulCount];

	hr = CreateConflictFolder(_("Sync Issues"), lpNewAdditionalREN, 1, lpParentFolder, &~lpConflictFolder);
	if(hr != hrSuccess) {
		ZLOG_DEBUG(m_lpLogger, "Failed to create 'Sync Issues' folder, hr = 0x%08x", hr);
		return hr;
	}
	
	hr = CreateConflictFolder(_("Conflicts"), lpNewAdditionalREN, 0, lpConflictFolder, NULL);
	if(hr != hrSuccess) {
		ZLOG_DEBUG(m_lpLogger, "Failed to create 'Conflicts' folder, hr = 0x%08x", hr);
		return hr;
	}
	
	hr = CreateConflictFolder(_("Local Failures"), lpNewAdditionalREN, 2, lpConflictFolder, NULL);
	if(hr != hrSuccess) {
		ZLOG_DEBUG(m_lpLogger, "Failed to create 'Local Failures' folder, hr = 0x%08x", hr);
		return hr;
	}
	
	hr = CreateConflictFolder(_("Server Failures"), lpNewAdditionalREN, 3, lpConflictFolder, NULL);
	if(hr != hrSuccess) {
		ZLOG_DEBUG(m_lpLogger, "Failed to create 'Server Failures' folder, hr = 0x%08x", hr);
		return hr;
	}

	hr = HrSetOneProp(lpRootFolder, lpNewAdditionalREN);
	if(hr != hrSuccess)
		return hr;
	hr = HrSetOneProp(lpInbox, lpNewAdditionalREN);
	if(hr != hrSuccess)
		return hr;
	hr = HrUpdateSearchReminders(lpRootFolder, lpNewAdditionalREN);
	if (hr == MAPI_E_NOT_FOUND) {
		m_lpLogger->Log(EC_LOGLEVEL_INFO, "No reminder searchfolder found, nothing to update");
	} else if (hr != hrSuccess) {
		ZLOG_DEBUG(m_lpLogger, "Failed to update search reminders, hr = 0x%08x", hr);
		return hr;
	}
	return hrSuccess;
}

HRESULT ECExchangeImportContentsChanges::CreateConflictFolder(LPTSTR lpszName, LPSPropValue lpAdditionalREN, ULONG ulMVPos, LPMAPIFOLDER lpParentFolder, LPMAPIFOLDER * lppConflictFolder){
	HRESULT hr = hrSuccess;
	object_ptr<IMAPIFolder> lpConflictFolder;
	memory_ptr<SPropValue> lpEntryId;
	SPropValue sPropValue;
	ULONG ulObjType = 0;

	if (lpAdditionalREN->Value.MVbin.lpbin[ulMVPos].cb > 0 &&
	    lpParentFolder->OpenEntry(lpAdditionalREN->Value.MVbin.lpbin[ulMVPos].cb, reinterpret_cast<ENTRYID *>(lpAdditionalREN->Value.MVbin.lpbin[ulMVPos].lpb), &IID_IMAPIFolder, MAPI_MODIFY, &ulObjType, &~lpConflictFolder) == hrSuccess) {
		if(lppConflictFolder)
			*lppConflictFolder = lpConflictFolder.release();
		return hr;
	}
	hr = lpParentFolder->CreateFolder(FOLDER_GENERIC, lpszName, nullptr, &IID_IMAPIFolder, OPEN_IF_EXISTS | fMapiUnicode, &~lpConflictFolder);
	if(hr != hrSuccess)
		return hr;

	sPropValue.ulPropTag = PR_FOLDER_DISPLAY_FLAGS;
	sPropValue.Value.bin.cb = 6;
	sPropValue.Value.bin.lpb = (LPBYTE)"\x01\x04\x0A\x80\x1E\x00";

	hr = HrSetOneProp(lpConflictFolder, &sPropValue);
	if(hr != hrSuccess)
		return hr;
	hr = HrGetOneProp(lpConflictFolder, PR_ENTRYID, &~lpEntryId);
	if(hr != hrSuccess)
		return hr;
	hr = MAPIAllocateMore(lpEntryId->Value.bin.cb, lpAdditionalREN, (LPVOID*)&lpAdditionalREN->Value.MVbin.lpbin[ulMVPos].lpb);
	if(hr != hrSuccess)
		return hr;
	memcpy(lpAdditionalREN->Value.MVbin.lpbin[ulMVPos].lpb, lpEntryId->Value.bin.lpb, lpEntryId->Value.bin.cb);
	lpAdditionalREN->Value.MVbin.lpbin[ulMVPos].cb = lpEntryId->Value.bin.cb;

	if(lppConflictFolder)
		*lppConflictFolder = lpConflictFolder.release();
	return hrSuccess;
}

HRESULT ECExchangeImportContentsChanges::ConfigForConversionStream(LPSTREAM lpStream, ULONG ulFlags, ULONG /*cValuesConversion*/, LPSPropValue /*lpPropArrayConversion*/)
{
	HRESULT hr;
	BOOL	bCanStream = FALSE;

	// Since we don't use the cValuesConversion and lpPropArrayConversion arguments, we'll just check
	// if the server suppors streaming and if so call the 'normal' config.

	hr = m_lpFolder->GetMsgStore()->lpTransport->HrCheckCapabilityFlags(KOPANO_CAP_ENHANCED_ICS, &bCanStream);
	if (hr != hrSuccess)
		return hr;
	if (bCanStream == FALSE)
		return MAPI_E_NO_SUPPORT;
	return Config(lpStream, ulFlags);
}

HRESULT ECExchangeImportContentsChanges::ImportMessageChangeAsAStream(ULONG cValue, LPSPropValue lpPropArray, ULONG ulFlags, LPSTREAM *lppStream)
{
	HRESULT hr;
	ULONG cbEntryId = 0;
	EntryIdPtr ptrEntryId;
	WSMessageStreamImporterPtr ptrMessageImporter;
	StreamPtr ptrStream;

	auto lpMessageSourceKey = PCpropFindProp(lpPropArray, cValue, PR_SOURCE_KEY);
	if (lpMessageSourceKey != NULL) {
		hr = m_lpFolder->GetMsgStore()->lpTransport->HrEntryIDFromSourceKey(m_lpFolder->GetMsgStore()->m_cbEntryId, m_lpFolder->GetMsgStore()->m_lpEntryId, m_lpSourceKey->Value.bin.cb, m_lpSourceKey->Value.bin.lpb, lpMessageSourceKey->Value.bin.cb, lpMessageSourceKey->Value.bin.lpb, &cbEntryId, &~ptrEntryId);
		if (hr != MAPI_E_NOT_FOUND && hr != hrSuccess) {
			ZLOG_DEBUG(m_lpLogger, "ImportFast: Failed to get entryid from sourcekey, hr = 0x%08x", hr);
			return hr;
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
		ZLOG_DEBUG(m_lpLogger, "ImportFast: %s", "Destination message deleted");
		return SYNC_E_OBJECT_DELETED;
	}

	if (hr == MAPI_E_NOT_FOUND)
		hr = ImportMessageCreateAsStream(cValue, lpPropArray, &~ptrMessageImporter);
	else
		hr = ImportMessageUpdateAsStream(cbEntryId, ptrEntryId, cValue, lpPropArray, &~ptrMessageImporter);
	if (hr != hrSuccess) {
		if (hr != SYNC_E_IGNORE && hr != SYNC_E_OBJECT_DELETED)
			ZLOG_DEBUG(m_lpLogger, "ImportFast: Failed to get MessageImporter, hr = 0x%08x", hr);
		return hr;
	}

	ZLOG_DEBUG(m_lpLogger, "ImportFast: %s", "Wrapping MessageImporter in IStreamAdapter");
	hr = ECMessageStreamImporterIStreamAdapter::Create(ptrMessageImporter, &~ptrStream);
	if (hr != hrSuccess) {
		ZLOG_DEBUG(m_lpLogger, "ImportFast: Failed to wrap message importer, hr = 0x%08x" ,hr);
		return hr;
	}

	*lppStream = ptrStream.release();
	return hrSuccess;
}

HRESULT ECExchangeImportContentsChanges::ImportMessageCreateAsStream(ULONG cValue, LPSPropValue lpPropArray, WSMessageStreamImporter **lppMessageImporter)
{
	HRESULT hr;
	ULONG ulNewFlags = 0;
	ULONG cbEntryId = 0;
	LPENTRYID lpEntryId = NULL;
	WSMessageStreamImporterPtr ptrMessageImporter;

	if (lpPropArray == NULL || lppMessageImporter == NULL)
		return MAPI_E_INVALID_PARAMETER;

	auto lpMessageFlags = PCpropFindProp(lpPropArray, cValue, PR_MESSAGE_FLAGS);
	auto lpMessageAssociated = PCpropFindProp(lpPropArray, cValue, PR_ASSOCIATED);
	auto lpPropEntryId = PCpropFindProp(lpPropArray, cValue, PR_ENTRYID);

	if ((lpMessageFlags != NULL && (lpMessageFlags->Value.ul & MSGFLAG_ASSOCIATED)) || (lpMessageAssociated != NULL && lpMessageAssociated->Value.b))
		ulNewFlags = MAPI_ASSOCIATED;

	if (lpPropEntryId != NULL && HrCompareEntryIdWithStoreGuid(lpPropEntryId->Value.bin.cb, (LPENTRYID)lpPropEntryId->Value.bin.lpb, &m_lpFolder->GetMsgStore()->GetStoreGuid()) == hrSuccess) {
		cbEntryId = lpPropEntryId->Value.bin.cb;
		lpEntryId = (LPENTRYID)lpPropEntryId->Value.bin.lpb;
	} else {
		ZLOG_DEBUG(m_lpLogger, "CreateFast: %s", "Creating new entryid");
		hr = HrCreateEntryId(m_lpFolder->GetMsgStore()->GetStoreGuid(), MAPI_MESSAGE, &cbEntryId, &lpEntryId);
		if (hr != hrSuccess) {
			ZLOG_DEBUG(m_lpLogger, "CreateFast: Failed to create entryid, hr = 0x%08x", hr);
			return hr;
		}
	}

	hr = m_lpFolder->CreateMessageFromStream(ulNewFlags, m_ulSyncId, cbEntryId, lpEntryId, &~ptrMessageImporter);
	if(hr != hrSuccess) {
		ZLOG_DEBUG(m_lpLogger, "CreateFast: Failed to create message from stream, hr = 0x%08x", hr);
		return hr;
	}

	*lppMessageImporter = ptrMessageImporter.release();
	return hrSuccess;
}

HRESULT ECExchangeImportContentsChanges::ImportMessageUpdateAsStream(ULONG cbEntryId, LPENTRYID lpEntryId, ULONG cValue, LPSPropValue lpPropArray, WSMessageStreamImporter **lppMessageImporter)
{
	HRESULT hr;
	SPropValuePtr ptrPropPCL;
	SPropValuePtr ptrPropCK;
	bool bAssociated = false;
	SPropValuePtr ptrConflictItems;
	WSMessageStreamImporterPtr ptrMessageImporter;

	if (lpEntryId == NULL || lpPropArray == NULL || lppMessageImporter == NULL)
		return MAPI_E_INVALID_PARAMETER;
	hr = m_lpFolder->GetChangeInfo(cbEntryId, lpEntryId, &~ptrPropPCL, &~ptrPropCK);
	if (hr != hrSuccess) {
		if (hr == MAPI_E_NOT_FOUND) {
			// The item was soft-deleted; sourcekey is known, but we cannot open the item. It has therefore been deleted.
			ZLOG_DEBUG(m_lpLogger, "UpdateFast: %s", "The destination item was deleted");
			hr = SYNC_E_OBJECT_DELETED;
		} else
			ZLOG_DEBUG(m_lpLogger, "UpdateFast: Failed to get change info, hr = 0x%08x", hr);
		return hr;
	}

	auto lpRemoteCK = PCpropFindProp(lpPropArray, cValue, PR_CHANGE_KEY);
	if (IsProcessed(lpRemoteCK, ptrPropPCL)) {
		//we already have this change
		ZLOG_DEBUG(m_lpLogger, "UpdateFast: %s", "The item was previously synchronized");
		return SYNC_E_IGNORE;
	}

	auto lpMessageFlags = PCpropFindProp(lpPropArray, cValue, PR_MESSAGE_FLAGS);
	auto lpMessageAssociated = PCpropFindProp(lpPropArray, cValue, PR_ASSOCIATED);
	if ((lpMessageFlags != NULL && (lpMessageFlags->Value.ul & MSGFLAG_ASSOCIATED)) || (lpMessageAssociated != NULL && lpMessageAssociated->Value.b))
		bAssociated = true;

	auto lpRemotePCL = PCpropFindProp(lpPropArray, cValue, PR_PREDECESSOR_CHANGE_LIST);
	if (!bAssociated && IsConflict(ptrPropCK, lpRemotePCL)) {
		MessagePtr ptrMessage;
		ULONG ulType = 0;

		ZLOG_DEBUG(m_lpLogger, "UpdateFast: %s", "The item seems to be in conflict");
		hr = m_lpFolder->OpenEntry(cbEntryId, lpEntryId, &iid_of(ptrMessage), MAPI_MODIFY, &ulType, &~ptrMessage);
		if (hr == MAPI_E_NOT_FOUND) {
			// This shouldn't happen as we just got a conflict.
			ZLOG_DEBUG(m_lpLogger, "UpdateFast: %s", "The destination item seems to have disappeared");
			return SYNC_E_OBJECT_DELETED;
		} else if (hr != hrSuccess) {
			ZLOG_DEBUG(m_lpLogger, "UpdateFast: Failed to open conflicting message, hr = 0x%08x", hr);
			return hr;
		}
		if (CreateConflictMessageOnly(ptrMessage, &~ptrConflictItems) == MAPI_E_NOT_FOUND) {
			CreateConflictFolders();
			CreateConflictMessageOnly(ptrMessage, &~ptrConflictItems);
		}
	}

	hr = m_lpFolder->UpdateMessageFromStream(m_ulSyncId, cbEntryId, lpEntryId, ptrConflictItems, &~ptrMessageImporter);
	if (hr != hrSuccess) {
		ZLOG_DEBUG(m_lpLogger, "UpdateFast: Failed to update message from stream, hr = 0x%08x", hr);
		return hr;
	}

	*lppMessageImporter = ptrMessageImporter.release();
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
static HRESULT HrRestrictionContains(const SRestriction *lpRestriction,
    std::list<SBinary> &lstEntryIds)
{
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
		return HrRestrictionContains(lpRestriction->res.resNot.lpRes, lstEntryIds);
	case RES_PROPERTY:
		if (lpRestriction->res.resProperty.ulPropTag == PR_PARENT_ENTRYID) {
			for (auto i = lstEntryIds.begin(); i != lstEntryIds.cend(); ++i) {
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
 * folders as specified in lpAdditionalREN. If either of the three entryids in
 * lpAdditionalREN is empty, the restriction won't be checked and it will be assumed
 * to be valid.
 *
 * @param[in]	lpRestriction		The restriction that is to be verified.
 * @param[in]	lpAdditionalREN		A MV_BINARY property that contains the entryids of the three
 *									three conflict folders.
 *
 * @retval	hrSuccess			The restriction is valid for the passed AdditionalREN. This means that
 *								it either contains the three entryids or that at least one of the entryids
 *								is empty.
 * @retval	MAPI_E_NOT_FOUND	lpAdditionalREN contains all three entryids, but not all of them
 *								were found in lpRestriction.
 */
static HRESULT
HrVerifyRemindersRestriction(const SRestriction *lpRestriction,
    const SPropValue *lpAdditionalREN)
{
	std::list<SBinary> lstEntryIds;

	if (lpAdditionalREN->Value.MVbin.lpbin[0].cb == 0 || lpAdditionalREN->Value.MVbin.lpbin[2].cb == 0 || lpAdditionalREN->Value.MVbin.lpbin[3].cb == 0)
		return hrSuccess;

	lstEntryIds.push_back(lpAdditionalREN->Value.MVbin.lpbin[0]);
	lstEntryIds.push_back(lpAdditionalREN->Value.MVbin.lpbin[2]);
	lstEntryIds.push_back(lpAdditionalREN->Value.MVbin.lpbin[3]);

	return HrRestrictionContains(lpRestriction, lstEntryIds);
}

HRESULT ECExchangeImportContentsChanges::HrUpdateSearchReminders(LPMAPIFOLDER lpRootFolder,
    const SPropValue *lpAdditionalREN)
{
	HRESULT hr;
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
	static constexpr const SizedSPropTagArray(2, sptaREMProps) =
		{2, {PR_REM_ONLINE_ENTRYID, PR_REM_OFFLINE_ENTRYID}};

	hr = lpRootFolder->GetProps(sptaREMProps, 0, &cREMProps, &~ptrREMProps);
	if (FAILED(hr))
		return hr;

	// Find the correct reminders folder.
	if (PROP_TYPE(ptrREMProps[1].ulPropTag) != PT_ERROR)
		lpREMEntryID = &ptrREMProps[1];
	else if (PROP_TYPE(ptrREMProps[0].ulPropTag) != PT_ERROR)
		lpREMEntryID = &ptrREMProps[0];
	else
		return MAPI_E_NOT_FOUND;

	hr = lpRootFolder->OpenEntry(lpREMEntryID->Value.bin.cb, reinterpret_cast<ENTRYID *>(lpREMEntryID->Value.bin.lpb), &iid_of(ptrRemindersFolder), MAPI_BEST_ACCESS, &ulType, &~ptrRemindersFolder);
	if (hr != hrSuccess)
		return hr;
	hr = ptrRemindersFolder->GetSearchCriteria(0, &~ptrOrigRestriction, &~ptrOrigContainerList, &ulOrigSearchState);
	if (hr != hrSuccess)
		return hr;

	// First check if the SearchCriteria needs updating by seeing if we can find the restrictions that
	// contain the entryids of the folders to exclude. We assume that when they're found they're used
	// as expected: as excludes.
	hr = HrVerifyRemindersRestriction(ptrOrigRestriction, lpAdditionalREN);
	if (hr == hrSuccess)
		return hr;

	sPropValConflicts.Value.bin = lpAdditionalREN->Value.MVbin.lpbin[0];
	sPropValLocalFailures.Value.bin = lpAdditionalREN->Value.MVbin.lpbin[2];
	sPropValServerFailures.Value.bin = lpAdditionalREN->Value.MVbin.lpbin[3];

	resPre +=
		ECPropertyRestriction(RELOP_NE, PR_PARENT_ENTRYID, &sPropValConflicts, ECRestriction::Cheap) +
		ECPropertyRestriction(RELOP_NE, PR_PARENT_ENTRYID, &sPropValLocalFailures, ECRestriction::Cheap) +
		ECPropertyRestriction(RELOP_NE, PR_PARENT_ENTRYID, &sPropValServerFailures, ECRestriction::Cheap) +
		ECRawRestriction(ptrOrigRestriction.get(), ECRestriction::Cheap);
	hr = resPre.CreateMAPIRestriction(&~ptrPreRestriction, ECRestriction::Cheap);
	if (hr != hrSuccess)
		return hr;

	return ptrRemindersFolder->SetSearchCriteria(ptrPreRestriction, ptrOrigContainerList, RESTART_SEARCH | (ulOrigSearchState & (SEARCH_FOREGROUND | SEARCH_RECURSIVE)));
}
