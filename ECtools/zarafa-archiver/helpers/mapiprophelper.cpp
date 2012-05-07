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

#include <platform.h>
#include "mapiprophelper.h"

#include "archiver-session.h"
#include "archiver-common.h"

#include <mapiutil.h>
#include <Util.h>
#include <mapi_ptr.h>
#include "mapiguidext.h"

using namespace std;

namespace za { namespace helpers {

/**
 * Create a MAPIPropHelper object.
 *
 * @param[in]	ptrMapiProp
 *					The MAPIPropPtr that points to the IMAPIProp object for which to create
 *					a MAPIPropHelper.
 * @param[out]	lppptrMAPIPropHelper
 *					Pointer to a MAPIPropHelperPtr that will be assigned the returned
 *					MAPIPropHelper object.
 *
 * @return HRESULT
 */
HRESULT MAPIPropHelper::Create(MAPIPropPtr ptrMapiProp, MAPIPropHelperPtr *lpptrMAPIPropHelper)
{
	HRESULT hr = hrSuccess;
	MAPIPropHelperPtr ptrMAPIPropHelper;
	
	try {
		ptrMAPIPropHelper.reset(new MAPIPropHelper(ptrMapiProp));
	} catch (std::bad_alloc &) {
		hr = MAPI_E_NOT_ENOUGH_MEMORY;
		goto exit;
	}
	
	hr = ptrMAPIPropHelper->Init();
	if (hr != hrSuccess)
		goto exit;
		
	*lpptrMAPIPropHelper = ptrMAPIPropHelper;	// transfers ownership
	
exit:
	return hr;
}

/**
 * Constructor
 */
MAPIPropHelper::MAPIPropHelper(MAPIPropPtr ptrMapiProp)
: m_ptrMapiProp(ptrMapiProp)
{ }

/**
 * Initialize a MAPIPropHelper object.
 */
HRESULT MAPIPropHelper::Init()
{
	HRESULT	hr = hrSuccess;

	PROPMAP_INIT_NAMED_ID(ARCHIVE_STORE_ENTRYIDS, PT_MV_BINARY, PSETID_Archive, dispidStoreEntryIds)
	PROPMAP_INIT_NAMED_ID(ARCHIVE_ITEM_ENTRYIDS, PT_MV_BINARY, PSETID_Archive, dispidItemEntryIds)
	PROPMAP_INIT_NAMED_ID(ORIGINAL_SOURCEKEY, PT_BINARY, PSETID_Archive, dispidOrigSourceKey)
	PROPMAP_INIT_NAMED_ID(STUBBED, PT_BOOLEAN, PSETID_Archive, dispidStubbed)
	PROPMAP_INIT_NAMED_ID(DIRTY, PT_BOOLEAN, PSETID_Archive, dispidDirty)
	PROPMAP_INIT_NAMED_ID(REF_STORE_ENTRYID, PT_BINARY, PSETID_Archive, dispidRefStoreEntryId)
	PROPMAP_INIT_NAMED_ID(REF_ITEM_ENTRYID, PT_BINARY, PSETID_Archive, dispidRefItemEntryId)
	PROPMAP_INIT_NAMED_ID(REF_PREV_ENTRYID, PT_BINARY, PSETID_Archive, dispidRefPrevEntryId)
	PROPMAP_INIT(m_ptrMapiProp)
	
exit:
	return hr;
}

/**
 * Destructor
 */
MAPIPropHelper::~MAPIPropHelper()
{ }

/**
 * Determine the state of the message. With this state one can determine if a
 * message is stubbed or dirty and copied or moved.
 *
 * @param[in]	ptrSession
 * 					The session needed to open the archive message(s) to determine
 * 					if a message was copied or moved.
 * @param[out]	lpState
 * 					The state that will be setup according to the message state.
 */
HRESULT MAPIPropHelper::GetMessageState(SessionPtr ptrSession, MessageState *lpState)
{
	HRESULT hr = hrSuccess;
	ULONG cMessageProps = 0;
	SPropArrayPtr ptrMessageProps;
	ULONG ulState = 0;
	int result = 0;

	SizedSPropTagArray(6, sptaMessageProps) = {6, {PR_ENTRYID, PROP_STUBBED, PROP_DIRTY, PR_SOURCE_KEY, PROP_ORIGINAL_SOURCEKEY, PR_RECORD_KEY}};
	enum {IDX_ENTRYID, IDX_STUBBED, IDX_DIRTY, IDX_SOURCE_KEY, IDX_ORIGINAL_SOURCEKEY, IDX_RECORD_KEY};

	if (!lpState) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	hr = m_ptrMapiProp->GetProps((LPSPropTagArray)&sptaMessageProps, 0, &cMessageProps, &ptrMessageProps);
	if (FAILED(hr))
		goto exit;


	if (PROP_TYPE(ptrMessageProps[IDX_ENTRYID].ulPropTag) == PT_ERROR) {
		hr = ptrMessageProps[IDX_ENTRYID].Value.err;
		goto exit;
	}
	if (PROP_TYPE(ptrMessageProps[IDX_SOURCE_KEY].ulPropTag) == PT_ERROR) {
		hr = ptrMessageProps[IDX_SOURCE_KEY].Value.err;
		goto exit;
	}
	if (PROP_TYPE(ptrMessageProps[IDX_STUBBED].ulPropTag) == PT_ERROR && ptrMessageProps[IDX_STUBBED].Value.err != MAPI_E_NOT_FOUND) {
		hr = ptrMessageProps[IDX_STUBBED].Value.err;
		goto exit;
	}
	if (PROP_TYPE(ptrMessageProps[IDX_DIRTY].ulPropTag) == PT_ERROR && ptrMessageProps[IDX_DIRTY].Value.err != MAPI_E_NOT_FOUND) {
		hr = ptrMessageProps[IDX_DIRTY].Value.err;
		goto exit;
	}
	if (PROP_TYPE(ptrMessageProps[IDX_ORIGINAL_SOURCEKEY].ulPropTag) == PT_ERROR && ptrMessageProps[IDX_ORIGINAL_SOURCEKEY].Value.err != MAPI_E_NOT_FOUND) {
		hr = ptrMessageProps[IDX_ORIGINAL_SOURCEKEY].Value.err;
		goto exit;
	}
	if (PROP_TYPE(ptrMessageProps[IDX_RECORD_KEY].ulPropTag) == PT_ERROR) {
		hr = ptrMessageProps[IDX_RECORD_KEY].Value.err;
		goto exit;
	}
	hr = hrSuccess;


	// Determine stubbed / dirty state.
	if (PROP_TYPE(ptrMessageProps[IDX_STUBBED].ulPropTag) != PT_ERROR && ptrMessageProps[IDX_STUBBED].Value.b == TRUE)
		ulState |= MessageState::msStubbed;

	if (PROP_TYPE(ptrMessageProps[IDX_DIRTY].ulPropTag) != PT_ERROR && ptrMessageProps[IDX_DIRTY].Value.b == TRUE) {
		// If for some reason both dirty and stubbed are set it's safest to mark the message
		// as stubbed. That might cause the archive to miss out some changes, but if we'd mark
		// it as dirty we might be rearchiving a stub, loosing all interesting information.
		if ((ulState & MessageState::msStubbed) == 0)
			ulState |= MessageState::msDirty;
	}


	// Determine copy / move state.
	if (PROP_TYPE(ptrMessageProps[IDX_ORIGINAL_SOURCEKEY].ulPropTag) == PT_ERROR) {
		ASSERT(ptrMessageProps[IDX_ORIGINAL_SOURCEKEY].Value.err == MAPI_E_NOT_FOUND);
		// No way to determine of message was copied/moved, so assume it's not.
		goto exit;
	}

	hr = Util::CompareProp(&ptrMessageProps[IDX_SOURCE_KEY], &ptrMessageProps[IDX_ORIGINAL_SOURCEKEY], createLocaleFromName(""), &result);
	if (hr != hrSuccess)
		goto exit;

	if (result != 0) {
		// The message is copied. Now check if it was moved.
		ObjectEntryList lstArchives;
		ObjectEntryList::iterator iArchive;
		ULONG ulType;
		MessagePtr ptrArchiveMsg;
		MAPIPropHelperPtr ptrArchiveHelper;
		SObjectEntry refEntry;
		MsgStorePtr ptrStore;
		MessagePtr ptrMessage;

		hr = GetArchiveList(&lstArchives, true);
		if (hr != hrSuccess)
			goto exit;

		for (iArchive = lstArchives.begin(); iArchive != lstArchives.end(); ++iArchive) {
			HRESULT hrTmp;
			MsgStorePtr ptrArchiveStore;

			hrTmp = ptrSession->OpenReadOnlyStore(iArchive->sStoreEntryId, &ptrArchiveStore);
			if (hrTmp != hrSuccess)
				continue;

			hrTmp = ptrArchiveStore->OpenEntry(iArchive->sItemEntryId.size(), iArchive->sItemEntryId, &ptrArchiveMsg.iid, 0, &ulType, &ptrArchiveMsg);
			if (hrTmp != hrSuccess)
				continue;

			break;
		}

		if (!ptrArchiveMsg) {
			if (ulState & MessageState::msStubbed) {
				hr = MAPI_E_NOT_FOUND;
				goto exit;
			} else {
				/*
				 * We were unable to open any archived message, but the message is
				 * not stubbed anyway. Just mark it as a copy.
				 */
				ulState |= MessageState::msCopy;
			}
		} else {
			hr = MAPIPropHelper::Create(ptrArchiveMsg.as<MAPIPropPtr>(), &ptrArchiveHelper);
			if (hr != hrSuccess)
				goto exit;

			hr = ptrArchiveHelper->GetReference(&refEntry);
			if (hr != hrSuccess)
				goto exit;

			hr = ptrSession->OpenReadOnlyStore(refEntry.sStoreEntryId, &ptrStore);
			if (hr != hrSuccess)
				goto exit;

			hr = ptrStore->OpenEntry(refEntry.sItemEntryId.size(), refEntry.sItemEntryId, &ptrArchiveMsg.iid, 0, &ulType, &ptrMessage);
			if (hr == hrSuccess) {
				/*
				 * One would expect that if the message was opened properly here, the message that's being
				 * processed was copied because we were able to open the original reference, which should
				 * have been removed either way.
				 * However, because of a currently (13-07-2011) unknown issue, the moved message can be
				 * opened with it's old entryid. This is probably a cache issue.
				 * If this happens, the message just opened is the same message as the one that's being
				 * processed. That can be easily verified by comparing the record key.
				 */
				SPropValuePtr ptrRecordKey;

				hr = HrGetOneProp(ptrMessage, PR_RECORD_KEY, &ptrRecordKey);
				if (hr != hrSuccess)
					goto exit;

				if (Util::CompareSBinary(ptrMessageProps[IDX_RECORD_KEY].Value.bin, ptrRecordKey->Value.bin) == 0) {
					// We opened the same message through the reference, which shouldn't be possible. This
					// must have been a move operation.
					ulState |= MessageState::msMove;
				} else
					ulState |= MessageState::msCopy;
			} else if (hr == MAPI_E_NOT_FOUND) {
				hr = hrSuccess;
				ulState |= MessageState::msMove;
			} else
				goto exit;
		}
	}

	lpState->m_ulState = ulState;

exit:
	return hr;
}

/**
 * Get the list of archives for the object.
 * This has a different meaning for different objects:
 * Message store: A list of folders that are the root folders of the attached archives.
 * Folders: A list of folders that are the corresponding folders in the attached archives.
 * Messages: A list of messages that are archived versions of the current message.
 *
 * @param[out]	lplstArchives
 *					Pointer to a list that will be populated with the archive references.
 *
 * @param[in]	bIgnoreSourceKey
 * 					Don't try to detect a copy/move and return an empty list in that case.
 *
 * @return HRESULT
 */
HRESULT MAPIPropHelper::GetArchiveList(ObjectEntryList *lplstArchives, bool bIgnoreSourceKey)
{
	HRESULT hr = hrSuccess;
	ULONG cbValues = 0;
	SPropArrayPtr ptrPropArray;
	ObjectEntryList lstArchives;
	int result = 0;
	
	SizedSPropTagArray (4, sptaArchiveProps) = {4, {PROP_ARCHIVE_STORE_ENTRYIDS, PROP_ARCHIVE_ITEM_ENTRYIDS, PROP_ORIGINAL_SOURCEKEY, PR_SOURCE_KEY}};

	enum 
	{
		IDX_ARCHIVE_STORE_ENTRYIDS, 
		IDX_ARCHIVE_ITEM_ENTRYIDS, 
		IDX_ORIGINAL_SOURCEKEY,
		IDX_SOURCE_KEY
	};
	
	hr = m_ptrMapiProp->GetProps((LPSPropTagArray)&sptaArchiveProps, 0, &cbValues, &ptrPropArray);
	if (FAILED(hr))
		goto exit;
		
	if (hr == MAPI_W_ERRORS_RETURNED) {
		/**
		 * We expect all three PROP_* properties to be present or all three to be absent, with
		 * one exception: If PR_SOURCE_KEY is missing PROP_ORIGINAL_SOURCEKEY is not needed.
		 **/
		if (PROP_TYPE(ptrPropArray[IDX_ARCHIVE_STORE_ENTRYIDS].ulPropTag) == PT_ERROR &&
			PROP_TYPE(ptrPropArray[IDX_ARCHIVE_ITEM_ENTRYIDS].ulPropTag) == PT_ERROR)
		{
			// No entry ids exist. So that's fine
			hr = hrSuccess;
			goto exit;
		}
		else if (PROP_TYPE(ptrPropArray[IDX_ARCHIVE_STORE_ENTRYIDS].ulPropTag) != PT_ERROR &&
				 PROP_TYPE(ptrPropArray[IDX_ARCHIVE_ITEM_ENTRYIDS].ulPropTag) != PT_ERROR)
		{
			// Both exist. So if PR_SOURCEKEY_EXISTS and PROP_ORIGINAL_SOURCEKEY doesn't
			// the entry is corrupt
			if (PROP_TYPE(ptrPropArray[IDX_SOURCE_KEY].ulPropTag) != PT_ERROR) {
				if (PROP_TYPE(ptrPropArray[IDX_ORIGINAL_SOURCEKEY].ulPropTag) == PT_ERROR) {
					hr = MAPI_E_CORRUPT_DATA;
					goto exit;
				} else if (!bIgnoreSourceKey) {
					// @todo: Create correct locale.
					hr = Util::CompareProp(&ptrPropArray[IDX_SOURCE_KEY], &ptrPropArray[IDX_ORIGINAL_SOURCEKEY], createLocaleFromName(""), &result);
					if (hr != hrSuccess)
						goto exit;

					if (result != 0)
						// The archive list was apparently copied into this message. So it's not valid (not an error).
						goto exit;
				}
			} else
				hr = hrSuccess;
		}
		else
		{
			// One exists, one doesn't.
			hr = MAPI_E_CORRUPT_DATA;
			goto exit;
		}
	}

	if (ptrPropArray[IDX_ARCHIVE_STORE_ENTRYIDS].Value.MVbin.cValues != ptrPropArray[IDX_ARCHIVE_ITEM_ENTRYIDS].Value.MVbin.cValues) {
		hr = MAPI_E_CORRUPT_DATA;
		goto exit;
	}
	
	for (ULONG i = 0; i < ptrPropArray[0].Value.MVbin.cValues; ++i) {
		SObjectEntry objectEntry;
		
		objectEntry.sStoreEntryId.assign(ptrPropArray[IDX_ARCHIVE_STORE_ENTRYIDS].Value.MVbin.lpbin[i]);
		objectEntry.sItemEntryId.assign(ptrPropArray[IDX_ARCHIVE_ITEM_ENTRYIDS].Value.MVbin.lpbin[i]);
		
		lstArchives.push_back(objectEntry);
	}
	
	swap(*lplstArchives, lstArchives);
		
exit:
	return hr;
}

/**
 * Set or replace the list of archives for the current object.
 *
 * @param[in]	lstArchives
 *					The list of archive references that should be stored in the object.
 * @param[in]	bExplicitCommit
 *					If set to true, the changes are committed before this function returns.
 *
 * @return HRESULT
 */
HRESULT MAPIPropHelper::SetArchiveList(const ObjectEntryList &lstArchives, bool bExplicitCommit)
{
	HRESULT hr = hrSuccess;
	ULONG cValues = lstArchives.size();
	SPropArrayPtr ptrPropArray;
	SPropValuePtr ptrSourceKey;
	ObjectEntryList::const_iterator iArchive;
	ULONG cbProps = 2;

	hr = MAPIAllocateBuffer(3 * sizeof(SPropValue), (LPVOID*)&ptrPropArray);
	if (hr != hrSuccess)
		goto exit;

	ptrPropArray[0].ulPropTag = PROP_ARCHIVE_STORE_ENTRYIDS;
	ptrPropArray[0].Value.MVbin.cValues = cValues;
	hr = MAPIAllocateMore(cValues * sizeof(SBinary), ptrPropArray, (LPVOID*)&ptrPropArray[0].Value.MVbin.lpbin);
	if (hr != hrSuccess)
		goto exit;
	
	ptrPropArray[1].ulPropTag = PROP_ARCHIVE_ITEM_ENTRYIDS;
	ptrPropArray[1].Value.MVbin.cValues = cValues;
	hr = MAPIAllocateMore(cValues * sizeof(SBinary), ptrPropArray, (LPVOID*)&ptrPropArray[1].Value.MVbin.lpbin);
	if (hr != hrSuccess)
		goto exit;
	
	iArchive = lstArchives.begin();
	for (ULONG i = 0; i < cValues; ++i, ++iArchive) {
		ptrPropArray[0].Value.MVbin.lpbin[i].cb = iArchive->sStoreEntryId.size();
		hr = MAPIAllocateMore(iArchive->sStoreEntryId.size(), ptrPropArray, (LPVOID*)&ptrPropArray[0].Value.MVbin.lpbin[i].lpb);
		if (hr != hrSuccess)
			goto exit;
		memcpy(ptrPropArray[0].Value.MVbin.lpbin[i].lpb, iArchive->sStoreEntryId, iArchive->sStoreEntryId.size());
		
		ptrPropArray[1].Value.MVbin.lpbin[i].cb = iArchive->sItemEntryId.size();
		hr = MAPIAllocateMore(iArchive->sItemEntryId.size(), ptrPropArray, (LPVOID*)&ptrPropArray[1].Value.MVbin.lpbin[i].lpb);
		if (hr != hrSuccess)
			goto exit;
		memcpy(ptrPropArray[1].Value.MVbin.lpbin[i].lpb, iArchive->sItemEntryId, iArchive->sItemEntryId.size());
	}

	/**
	 * We store the sourcekey of the item for which the list of archives is valid. This way if the
	 * item gets moved everything is fine. But when it gets copied a new archive will be created
	 * for it.
	 **/
	hr = HrGetOneProp(m_ptrMapiProp, PR_SOURCE_KEY, &ptrSourceKey);
	if (hr == hrSuccess) {
		ptrPropArray[2].ulPropTag = PROP_ORIGINAL_SOURCEKEY;
		ptrPropArray[2].Value.bin = ptrSourceKey->Value.bin;	// Cheap copy

		cbProps = 3;
	}

	hr = m_ptrMapiProp->SetProps(cbProps, ptrPropArray.get(), NULL);
	if (hr != hrSuccess)
		goto exit;
	
	if (bExplicitCommit)
		hr = m_ptrMapiProp->SaveChanges(KEEP_OPEN_READWRITE);
	
exit:
	return hr;
}

/**
 * Set a reference to a primary object in an archived object. A reference is set on archive
 * folders and archive messages. They reference to the original folder or message for which the
 * archived version exists.
 *
 * @param[in]	sEntryId
 *					The id of the referenced object.
 * @param[in]	bExplicitCommit
 *					If set to true, the changes are committed before this function returns.
 *
 * @return HRESULT
 */
HRESULT  MAPIPropHelper::SetReference(const SObjectEntry &sEntry, bool bExplicitCommit)
{
	HRESULT hr = hrSuccess;
	SPropValue sPropArray[2] = {{0}};

	sPropArray[0].ulPropTag = PROP_REF_STORE_ENTRYID;
	sPropArray[0].Value.bin.cb = sEntry.sStoreEntryId.size();
	sPropArray[0].Value.bin.lpb = sEntry.sStoreEntryId;

	sPropArray[1].ulPropTag = PROP_REF_ITEM_ENTRYID;
	sPropArray[1].Value.bin.cb = sEntry.sItemEntryId.size();
	sPropArray[1].Value.bin.lpb = sEntry.sItemEntryId;

	hr = m_ptrMapiProp->SetProps(2, sPropArray, NULL);
	if (hr != hrSuccess)
		goto exit;

	if (bExplicitCommit)
		hr = m_ptrMapiProp->SaveChanges(KEEP_OPEN_READWRITE);
	
exit:
	return hr;
}


HRESULT MAPIPropHelper::ClearReference(bool bExplicitCommit)
{
	HRESULT hr = hrSuccess;
	SizedSPropTagArray(2, sptaReferenceProps) = {2, {PROP_REF_STORE_ENTRYID, PROP_REF_ITEM_ENTRYID}};

	hr = m_ptrMapiProp->DeleteProps((LPSPropTagArray)&sptaReferenceProps, NULL);
	if (hr != hrSuccess)
		goto exit;

	if (bExplicitCommit)
		hr = m_ptrMapiProp->SaveChanges(KEEP_OPEN_READWRITE);

exit:
	return hr;
}


HRESULT MAPIPropHelper::GetReference(SObjectEntry *lpEntry)
{
	HRESULT hr = hrSuccess;
	ULONG cMessageProps = 0;
	SPropArrayPtr ptrMessageProps;

	SizedSPropTagArray(2, sptaMessageProps) = {2, {PROP_REF_STORE_ENTRYID, PROP_REF_ITEM_ENTRYID}};
	enum {IDX_REF_STORE_ENTRYID, IDX_REF_ITEM_ENTRYID};

	if (lpEntry == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	hr = m_ptrMapiProp->GetProps((LPSPropTagArray)&sptaMessageProps, 0, &cMessageProps, &ptrMessageProps);
	if (FAILED(hr))
		goto exit;

	if (PROP_TYPE(ptrMessageProps[IDX_REF_STORE_ENTRYID].ulPropTag) == PT_ERROR) {
		hr = ptrMessageProps[IDX_REF_STORE_ENTRYID].Value.err;
		goto exit;
	}
	if (PROP_TYPE(ptrMessageProps[IDX_REF_ITEM_ENTRYID].ulPropTag) == PT_ERROR) {
		hr = ptrMessageProps[IDX_REF_ITEM_ENTRYID].Value.err;
		goto exit;
	}

	lpEntry->sStoreEntryId.assign(ptrMessageProps[IDX_REF_STORE_ENTRYID].Value.bin);
	lpEntry->sItemEntryId.assign(ptrMessageProps[IDX_REF_ITEM_ENTRYID].Value.bin);

exit:
	return hr;
}


HRESULT MAPIPropHelper::ReferencePrevious(const SObjectEntry &sEntry)
{
	HRESULT hr = hrSuccess;
	SPropValue sPropValue = {0};

	sPropValue.ulPropTag = PROP_REF_PREV_ENTRYID;
	sPropValue.Value.bin.cb = sEntry.sItemEntryId.size();
	sPropValue.Value.bin.lpb = sEntry.sItemEntryId;

	hr = HrSetOneProp(m_ptrMapiProp, &sPropValue);

	return hr;
}

HRESULT MAPIPropHelper::OpenPrevious(SessionPtr ptrSession, LPMESSAGE *lppMessage)
{
	HRESULT hr = hrSuccess;
	SPropValuePtr ptrEntryID;
	ULONG ulType;
	MessagePtr ptrMessage;

	if (lppMessage == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	hr = HrGetOneProp(m_ptrMapiProp, PROP_REF_PREV_ENTRYID, &ptrEntryID);
	if (hr != hrSuccess)
		goto exit;

	hr = ptrSession->GetMAPISession()->OpenEntry(ptrEntryID->Value.bin.cb, (LPENTRYID)ptrEntryID->Value.bin.lpb, &ptrMessage.iid, 0, &ulType, &ptrMessage);
	if (hr == MAPI_E_NOT_FOUND) {
		SPropValuePtr ptrStoreEntryID;
		MsgStorePtr ptrStore;

		hr = HrGetOneProp(m_ptrMapiProp, PR_STORE_ENTRYID, &ptrStoreEntryID);
		if (hr != hrSuccess)
			goto exit;

		hr = ptrSession->OpenStore(ptrStoreEntryID->Value.bin, &ptrStore);
		if (hr != hrSuccess)
			goto exit;

		hr = ptrStore->OpenEntry(ptrEntryID->Value.bin.cb, (LPENTRYID)ptrEntryID->Value.bin.lpb, &ptrMessage.iid, 0, &ulType, &ptrMessage);
	}
	if (hr != hrSuccess)
		goto exit;

	hr = ptrMessage->QueryInterface(IID_IMessage, (LPVOID*)lppMessage);

exit:
	return hr;
}

/**
 * Remove the {72e98ebc-57d2-4ab5-b0aad50a7b531cb9}/stubbed property. Note that IsStubbed can still
 * return true if the message class is not updated properly. However, this is done in the caller
 * of this function, which has no notion of the set of named properies that are needed to remove this
 * property.
 *
 * @return HRESULT.
 */
HRESULT MAPIPropHelper::RemoveStub()
{
	HRESULT hr = hrSuccess;
	SizedSPropTagArray(1, sptaArchiveProps) = {1, {PROP_STUBBED}};

	hr = m_ptrMapiProp->DeleteProps((LPSPropTagArray)&sptaArchiveProps, NULL);
	
	return hr;
}

HRESULT MAPIPropHelper::SetClean()
{
	HRESULT hr = hrSuccess;
	SizedSPropTagArray(1, sptaDirtyProps) = {1, {PROP_DIRTY}};

	hr = m_ptrMapiProp->DeleteProps((LPSPropTagArray)&sptaDirtyProps, NULL);
	
	return hr;
}


/**
 * Detach an object from it's archived version.
 * This does not cause the reference in the archived version to be removed.
 *
 * @return HRESULT
 */
HRESULT MAPIPropHelper::DetachFromArchives()
{
	HRESULT hr = hrSuccess;
	SizedSPropTagArray(5, sptaArchiveProps) = {5, {PROP_ARCHIVE_STORE_ENTRYIDS, PROP_ARCHIVE_ITEM_ENTRYIDS, PROP_STUBBED, PROP_DIRTY, PROP_ORIGINAL_SOURCEKEY}};

	hr = m_ptrMapiProp->DeleteProps((LPSPropTagArray)&sptaArchiveProps, NULL);
	
	return hr;
}

/**
 * Get the parent folder of an object.
 * 
 * @param[in]	lpSession
 *					Pointer to a session object that's used to open the folder with.
 * @param[in]	lppFolder
 *					Pointer to a IMAPIFolder pointer that will be assigned the address
 *					of the returned folder.
 *
 * @return HRESULT
 */
HRESULT MAPIPropHelper::GetParentFolder(SessionPtr ptrSession, LPMAPIFOLDER *lppFolder)
{
	HRESULT hr = hrSuccess;
	SPropArrayPtr ptrPropArray;
	MsgStorePtr ptrMsgStore;
	MAPIFolderPtr ptrFolder;
	ULONG cValues = 0;
	ULONG ulType = 0;
	
	SizedSPropTagArray(2, sptaProps) = {2, {PR_PARENT_ENTRYID, PR_STORE_ENTRYID}};
	
	if (!ptrSession) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}
	
	// We can't just open a folder on the session (at least not in Linux). So we open the store first
	hr = m_ptrMapiProp->GetProps((LPSPropTagArray)&sptaProps, 0, &cValues, &ptrPropArray);
	if (hr != hrSuccess)
		goto exit;
	
	hr = ptrSession->OpenStore(ptrPropArray[1].Value.bin, &ptrMsgStore);
	if (hr != hrSuccess)
		goto exit;
		
	hr = ptrMsgStore->OpenEntry(ptrPropArray[0].Value.bin.cb, (LPENTRYID)ptrPropArray[0].Value.bin.lpb, &ptrFolder.iid, MAPI_BEST_ACCESS|fMapiDeferredErrors, &ulType, &ptrFolder);
	if (hr != hrSuccess)
		goto exit;
	
	hr = ptrFolder->QueryInterface(IID_IMAPIFolder, (LPVOID*)lppFolder);
	
exit:
	return hr;
}

/**
 * Get the list of properties needed to be able to determine if the message from which
 * the properties are obtained is stubbed and if so to get a list of archived versions.
 *
 * @param[in]	ptrMapiProp		An IMAPIProp that lives on the same server as the the
 * 								message from which the properties will be obtained. This
 * 								is needed to properly resolve the named properties.
 * @param[in]	lpExtra			Optional pointer to a PropTagArray containing additional
 * 								properties that will also be placed in the resulting
 * 								PropTagArray. This is for convenience when building a list
 * 								of properties where not only the archive properties are
 * 								required.
 * @param[out]	lppProps		The resulting PropTagArray.
 */
HRESULT MAPIPropHelper::GetArchiverProps(MAPIPropPtr ptrMapiProp, LPSPropTagArray lpExtra, LPSPropTagArray *lppProps)
{
	HRESULT hr = hrSuccess;

	SizedSPropTagArray(2, sptaFixedProps) = {2, {PR_MESSAGE_CLASS, PR_SOURCE_KEY}};
	
	PROPMAP_START
		PROPMAP_NAMED_ID(STUBBED, PT_BOOLEAN, PSETID_Archive, dispidStubbed)
	PROPMAP_INIT(ptrMapiProp)

	 hr = MAPIAllocateBuffer(CbNewSPropTagArray(sptaFixedProps.cValues + 1 + (lpExtra ? lpExtra->cValues : 0)), (LPVOID*)lppProps);
	 if (hr != hrSuccess)
		goto exit;

	for (ULONG i = 0; i < sptaFixedProps.cValues; ++i)
		(*lppProps)->aulPropTag[i] = sptaFixedProps.aulPropTag[i];

	(*lppProps)->cValues = sptaFixedProps.cValues;
	(*lppProps)->aulPropTag[(*lppProps)->cValues++] = PROP_STUBBED;

	for (ULONG i = 0; lpExtra && i < lpExtra->cValues; ++i)
		(*lppProps)->aulPropTag[(*lppProps)->cValues++] = lpExtra->aulPropTag[i];

exit:
	return hr;
}

/**
 * Check if a message is stubbed.
 * This is accomplished by checking if the named property {72e98ebc-57d2-4ab5-b0aad50a7b531cb9}/stubbed is set to true or false.
 * If that property is absent the PR_MESSAGE_CLASS is compared to "IPM.Zarafa.Stub". If that matches the message is
 * considered stubbed as well.
 *
 * @param[in]	ptrMapiProp		An IMAPIProp that lives on the same server as the the
 * 								message from which the properties were obtained. This
 * 								is needed to properly resolve the named properties.
 * @param[in]	lpProps			The list of properties to use to determine if the message
 * 								from which they were obtained is stubbed.
 * @param[in]	cbProps			The amount of properties in lpProps.
 * @param[out]	lpbResult		Pointer to a boolean that will be set to true if the
 * 								message is stubbed, and false otherwise.
 *
 * @return HRESULT
 */
HRESULT MAPIPropHelper::IsStubbed(MAPIPropPtr ptrMapiProp, LPSPropValue lpProps, ULONG cbProps, bool *lpbResult)
{
	HRESULT hr = hrSuccess;
	LPSPropValue lpPropStubbed = NULL;
	LPSPropValue lpPropMessageClass = NULL;

	PROPMAP_START
		PROPMAP_NAMED_ID(STUBBED, PT_BOOLEAN, PSETID_Archive, dispidStubbed)
	PROPMAP_INIT(ptrMapiProp)

	lpPropStubbed = PpropFindProp(lpProps, cbProps, PROP_STUBBED);

	if (!lpPropStubbed) {
		// PROP_STUBBED doesn't exist, check the message class to be sure
		lpPropMessageClass = PpropFindProp(lpProps, cbProps, PR_MESSAGE_CLASS);
		if (lpPropMessageClass)
			*lpbResult = (_tcsicmp(lpPropMessageClass->Value.LPSZ, _T("IPM.Zarafa.Stub")) == 0);
		else
			*lpbResult = false;
	} else
		*lpbResult = (lpPropStubbed->Value.b != 0);
		
exit:
	return hr;
}

/**
 * Get the list of archives for the object.
 * This has a different meaning for different objects:
 * Message store: A list of folders that are the root folders of the attached archives.
 * Folders: A list of folders that are the corresponding folders in the attached archives.
 * Messages: A list of messages that are archived versions of the current message.
 *
 * @param[in]	ptrMapiProp		An IMAPIProp that lives on the same server as the the
 * 								message from which the properties were obtained. This
 * 								is needed to properly resolve the named properties.
 * @param[in]	lpProps			The list of properties to use to get the list of archives
 * 								for the message from which they were obtained.
 * @param[in]	cbProps			The amount of properties in lpProps.
 * @param[out]	lplstArchives	Pointer to a list that will be populated with the archive references.
 *
 * @return HRESULT
 */
HRESULT MAPIPropHelper::GetArchiveList(MAPIPropPtr ptrMapiProp, LPSPropValue lpProps, ULONG cbProps, ObjectEntryList *lplstArchives)
{
	HRESULT hr = hrSuccess;
	ObjectEntryList lstArchives;
	int result = 0;

	LPSPropValue lpPropStoreEIDs = NULL;
	LPSPropValue lpPropItemEIDs = NULL;
	LPSPropValue lpPropOrigSK = NULL;
	LPSPropValue lpPropSourceKey = NULL;

	PROPMAP_START
		PROPMAP_NAMED_ID(ARCHIVE_STORE_ENTRYIDS, PT_MV_BINARY, PSETID_Archive, dispidStoreEntryIds)
		PROPMAP_NAMED_ID(ARCHIVE_ITEM_ENTRYIDS, PT_MV_BINARY, PSETID_Archive, dispidItemEntryIds)
		PROPMAP_NAMED_ID(ORIGINAL_SOURCEKEY, PT_BINARY, PSETID_Archive, dispidOrigSourceKey)
	PROPMAP_INIT(ptrMapiProp)

	lpPropStoreEIDs = PpropFindProp(lpProps, cbProps, PROP_ARCHIVE_STORE_ENTRYIDS);
	lpPropItemEIDs = PpropFindProp(lpProps, cbProps, PROP_ARCHIVE_ITEM_ENTRYIDS);
	lpPropOrigSK = PpropFindProp(lpProps, cbProps, PROP_ORIGINAL_SOURCEKEY);
	lpPropSourceKey = PpropFindProp(lpProps, cbProps, PR_SOURCE_KEY);

	if (!lpPropStoreEIDs || !lpPropItemEIDs || !lpPropOrigSK || !lpPropSourceKey) {
		/**
		 * We expect all three PROP_* properties to be present or all three to be absent, with
		 * one exception: If PR_SOURCE_KEY is missing PROP_ORIGINAL_SOURCEKEY is not needed.
		 **/
		if (!lpPropStoreEIDs && !lpPropItemEIDs)
		{
			// No entry ids exist. So that's fine
			hr = hrSuccess;
			goto exit;
		}
		else if (lpPropStoreEIDs && lpPropItemEIDs)
		{
			// Both exist. So if PR_SOURCEKEY_EXISTS and PROP_ORIGINAL_SOURCEKEY doesn't
			// the entry is corrupt
			if (lpPropSourceKey) {
				if (!lpPropOrigSK) {
					hr = MAPI_E_CORRUPT_DATA;
					goto exit;
				} else {
					// @todo: Create correct locale.
					hr = Util::CompareProp(lpPropSourceKey, lpPropOrigSK, createLocaleFromName(""), &result);
					if (hr != hrSuccess)
						goto exit;

					if (result != 0)
						// The archive list was apparently copied into this message. So it's not valid (not an error).
						goto exit;
				}
			} else
				hr = hrSuccess;
		}
		else
		{
			// One exists, one doesn't.
			hr = MAPI_E_CORRUPT_DATA;
			goto exit;
		}
	}

	if (lpPropStoreEIDs->Value.MVbin.cValues != lpPropItemEIDs->Value.MVbin.cValues) {
		hr = MAPI_E_CORRUPT_DATA;
		goto exit;
	}
	
	for (ULONG i = 0; i < lpPropStoreEIDs->Value.MVbin.cValues; ++i) {
		SObjectEntry objectEntry;
		
		objectEntry.sStoreEntryId.assign(lpPropStoreEIDs->Value.MVbin.lpbin[i]);
		objectEntry.sItemEntryId.assign(lpPropItemEIDs->Value.MVbin.lpbin[i]);
		
		lstArchives.push_back(objectEntry);
	}
	
	swap(*lplstArchives, lstArchives);
		
exit:
	return hr;
}

}} // namespaces
