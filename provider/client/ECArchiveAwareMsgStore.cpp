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
#include "ECArchiveAwareMsgStore.h"
#include "ECArchiveAwareMessage.h"
#include "ECGuid.h"
#include "mapi_ptr.h"
#include "threadutil.h"



ECArchiveAwareMsgStore::ECArchiveAwareMsgStore(char *lpszProfname, LPMAPISUP lpSupport, WSTransport *lpTransport, BOOL fModify, ULONG ulProfileFlags, BOOL fIsSpooler, BOOL fIsDefaultStore, BOOL bOfflineStore)
: ECMsgStore(lpszProfname, lpSupport, lpTransport, fModify, ulProfileFlags, fIsSpooler, fIsDefaultStore, bOfflineStore)
{ }

HRESULT ECArchiveAwareMsgStore::Create(char *lpszProfname, LPMAPISUP lpSupport, WSTransport *lpTransport, BOOL fModify, ULONG ulProfileFlags, BOOL fIsSpooler, BOOL fIsDefaultStore, BOOL bOfflineStore, ECMsgStore **lppECMsgStore)
{
	HRESULT hr = hrSuccess;

	ECArchiveAwareMsgStore *lpStore = new ECArchiveAwareMsgStore(lpszProfname, lpSupport, lpTransport, fModify, ulProfileFlags, fIsSpooler, fIsDefaultStore, bOfflineStore);

	hr = lpStore->QueryInterface(IID_ECMsgStore, (void **)lppECMsgStore);

	if(hr != hrSuccess)
		delete lpStore;

	return hr;
}

HRESULT ECArchiveAwareMsgStore::OpenEntry(ULONG cbEntryID, LPENTRYID lpEntryID, LPCIID lpInterface, ULONG ulFlags, ULONG *lpulObjType, LPUNKNOWN *lppUnk)
{
	// By default we'll try to open an archive aware message when a message is opened. The exception
	// is when the client is not licensed to do so or when it's explicitly disabled by passing 
	// IID_IECMessageRaw as the lpInterface parameter. This is for instance needed for the archiver
	// itself becaus it needs to operate on the non-stubbed (or raw) message.
	// In this override, we only check for the presence of IID_IECMessageRaw. If that's found, we'll
	// pass an ECMessageFactory instance to our parents OpenEntry.
	// Otherwise we'll pass an ECArchiveAwareMessageFactory instance, which will check the license
	// create the appropriate message type. If the object turns out to be a message that is.

	const bool bRawMessage = (lpInterface && memcmp(lpInterface, &IID_IECMessageRaw, sizeof(IID)) == 0);
	HRESULT hr = hrSuccess;

	if (bRawMessage)
		hr = ECMsgStore::OpenEntry(cbEntryID, lpEntryID, &IID_IMessage, ulFlags, ECMessageFactory(), lpulObjType, lppUnk);
	else
		hr = ECMsgStore::OpenEntry(cbEntryID, lpEntryID, lpInterface, ulFlags, ECArchiveAwareMessageFactory(), lpulObjType, lppUnk);

	return hr;
}

HRESULT ECArchiveAwareMsgStore::OpenItemFromArchive(LPSPropValue lpPropStoreEIDs, LPSPropValue lpPropItemEIDs, ECMessage **lppMessage)
{
	HRESULT				hr = hrSuccess;
	BinaryList			lstStoreEIDs;
	BinaryList			lstItemEIDs;
	BinaryListIterator	iterStoreEID;
	BinaryListIterator	iterIterEID;
	mapi_object_ptr<ECMessage, IID_ECMessage>	ptrArchiveMessage;

	if (lpPropStoreEIDs == NULL || 
		lpPropItemEIDs == NULL || 
		lppMessage == NULL || 
		PROP_TYPE(lpPropStoreEIDs->ulPropTag) != PT_MV_BINARY ||
		PROP_TYPE(lpPropItemEIDs->ulPropTag) != PT_MV_BINARY ||
		lpPropStoreEIDs->Value.MVbin.cValues != lpPropItemEIDs->Value.MVbin.cValues)
	{
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	// First get a list of items that could be retrieved from cached archive stores.
	hr = CreateCacheBasedReorderedList(lpPropStoreEIDs->Value.MVbin, lpPropItemEIDs->Value.MVbin, &lstStoreEIDs, &lstItemEIDs);
	if (hr != hrSuccess)
		goto exit;

	iterStoreEID = lstStoreEIDs.begin();
	iterIterEID = lstItemEIDs.begin();
	for (; iterStoreEID != lstStoreEIDs.end(); ++iterStoreEID, ++iterIterEID) {
		ECMsgStorePtr	ptrArchiveStore;
		ULONG			ulType = 0;

		hr = GetArchiveStore(*iterStoreEID, &ptrArchiveStore);
		if (hr == MAPI_E_NO_SUPPORT)
			goto exit;	// No need to try any other archives.
		if (hr != hrSuccess) {
			continue;
		}

		hr = ptrArchiveStore->OpenEntry((*iterIterEID)->cb, (LPENTRYID)(*iterIterEID)->lpb, &IID_ECMessage, 0, &ulType, &ptrArchiveMessage);
		if (hr != hrSuccess) {
			continue;
		}

		break;
	}

	if (iterStoreEID == lstStoreEIDs.end()) {
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}

	if (ptrArchiveMessage)
		hr = ptrArchiveMessage->QueryInterface(IID_ECMessage, (LPVOID*)lppMessage);

exit:
	return hr;
}

HRESULT ECArchiveAwareMsgStore::CreateCacheBasedReorderedList(SBinaryArray sbaStoreEIDs, SBinaryArray sbaItemEIDs, BinaryList *lplstStoreEIDs, BinaryList *lplstItemEIDs)
{
	BinaryList lstStoreEIDs;
	BinaryList lstItemEIDs;

	BinaryList lstUncachedStoreEIDs;
	BinaryList lstUncachedItemEIDs;

	for (ULONG i = 0; i < sbaStoreEIDs.cValues; ++i) {
		const std::vector<BYTE> eid(sbaStoreEIDs.lpbin[i].lpb, sbaStoreEIDs.lpbin[i].lpb + sbaStoreEIDs.lpbin[i].cb);
		if (m_mapStores.find(eid) != m_mapStores.end()) {
			lstStoreEIDs.push_back(sbaStoreEIDs.lpbin + i);
			lstItemEIDs.push_back(sbaItemEIDs.lpbin + i);
		} else {
			lstUncachedStoreEIDs.push_back(sbaStoreEIDs.lpbin + i);
			lstUncachedItemEIDs.push_back(sbaItemEIDs.lpbin + i);
		}
	}

	lstStoreEIDs.splice(lstStoreEIDs.end(), lstUncachedStoreEIDs);
	lstItemEIDs.splice(lstItemEIDs.end(), lstUncachedItemEIDs);

	lplstStoreEIDs->swap(lstStoreEIDs);
	lplstItemEIDs->swap(lstItemEIDs);

	return hrSuccess;
}

HRESULT ECArchiveAwareMsgStore::GetArchiveStore(LPSBinary lpStoreEID, ECMsgStore **lppArchiveStore)
{
	HRESULT hr = hrSuccess;

	const std::vector<BYTE> eid(lpStoreEID->lpb, lpStoreEID->lpb + lpStoreEID->cb);
	MsgStoreMap::iterator iterStore = m_mapStores.find(eid);
	if (iterStore != m_mapStores.end()) {
		hr = iterStore->second->QueryInterface(IID_ECMsgStore, (LPVOID*)lppArchiveStore);
		if (hr != hrSuccess)
			goto exit;
	} 
	
	else {
		// @todo: Consolidate this with ECMSProvider::LogonByEntryID
		UnknownPtr ptrUnknown;
		ECMsgStorePtr ptrOnlineStore;
		ULONG cbEntryID = 0;
		EntryIdPtr ptrEntryID;
		char *lpszServer = NULL;
		bool bIsPseudoUrl = false;
		std::string strServer;
		bool bIsPeer = false;
		mapi_object_ptr<WSTransport> ptrTransport;
		ECMsgStorePtr ptrArchiveStore;
		mapi_object_ptr<IECPropStorage, IID_IECPropStorage> ptrPropStorage;

		hr = QueryInterface(IID_ECMsgStoreOnline, &ptrUnknown);
		if (hr != hrSuccess)
			goto exit;

		hr = ptrUnknown->QueryInterface(IID_ECMsgStore, &ptrOnlineStore);
		if (hr != hrSuccess)
			goto exit;

	
		hr = UnWrapStoreEntryID(lpStoreEID->cb, (LPENTRYID)lpStoreEID->lpb, &cbEntryID, &ptrEntryID);
		if (hr != hrSuccess)
			goto exit;

		hr = HrGetServerURLFromStoreEntryId(cbEntryID, ptrEntryID, &lpszServer, &bIsPseudoUrl);
		if (hr != hrSuccess)
			goto exit;

		if (bIsPseudoUrl) {
			hr = HrResolvePseudoUrl(ptrOnlineStore->lpTransport, lpszServer, &strServer, &bIsPeer);
			if (hr != hrSuccess)
				goto exit;
			
			if (!bIsPeer)
				lpszServer = (char*)strServer.c_str();
			
			else {
				// We can't just use the transport from ptrOnlineStore as that will be
				// logged off when ptrOnlineStore gets destroyed (at the end of this finction).
				hr = ptrOnlineStore->lpTransport->CloneAndRelogon(&ptrTransport);
				if (hr != hrSuccess)
					goto exit;
			}
		}

		if (!ptrTransport) {
			// We get here if lpszServer wasn't a pseudo URL or if it was and it resolved
			// to another server than the one we're connected with.
			hr = ptrOnlineStore->lpTransport->CreateAndLogonAlternate(lpszServer, &ptrTransport);
			if (hr != hrSuccess)
				goto exit;
		}

		hr = ECMsgStore::Create((char*)GetProfileName(), this->lpSupport, ptrTransport, FALSE, 0, FALSE, FALSE, FALSE, &ptrArchiveStore);
		if (hr != hrSuccess)
			goto exit;

		// Get a propstorage for the message store
		hr = ptrTransport->HrOpenPropStorage(0, NULL, cbEntryID, ptrEntryID, 0, &ptrPropStorage);
		if (hr != hrSuccess)
			goto exit;

		// Set up the message store to use this storage
		hr = ptrArchiveStore->HrSetPropStorage(ptrPropStorage, FALSE);
		if (hr != hrSuccess)
			goto exit;

		// Setup callback for session change
		hr = ptrTransport->AddSessionReloadCallback(ptrArchiveStore, ECMsgStore::Reload, NULL);
		if (hr != hrSuccess)
			goto exit;

		hr = ptrArchiveStore->SetEntryId(cbEntryID, ptrEntryID);
		if (hr != hrSuccess)
			goto exit;

		hr = ptrArchiveStore->QueryInterface(IID_ECMsgStore, (LPVOID*)lppArchiveStore);
		if (hr != hrSuccess)
			goto exit;

		m_mapStores.insert(MsgStoreMap::value_type(eid, ptrArchiveStore));
	}

exit:
	return hr;
}
