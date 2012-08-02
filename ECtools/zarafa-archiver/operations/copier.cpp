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
#include "ECLogger.h"
#include "ECConfig.h"
#include "ECRestriction.h"
#include "copier.h"
#include "deleter.h"
#include "stubber.h"
#include "instanceidmapper.h"
#include "transaction.h"
#include "postsaveiidupdater.h"
#include "helpers/mapiprophelper.h"
#include "helpers/archivehelper.h"
#include "archiver-session.h"
#include "Util.h"
#include "mapiguidext.h"

#include <list>
#include <string>

using namespace za::helpers;
using namespace std;

namespace za { namespace operations {

Copier::Helper::Helper(SessionPtr ptrSession, ECLogger *lpLogger, const InstanceIdMapperPtr &ptrMapper, LPSPropTagArray lpExcludeProps, LPMAPIFOLDER lpFolder)
: m_ptrSession(ptrSession)
, m_lpLogger(lpLogger)
, m_lpExcludeProps(lpExcludeProps)
, m_ptrFolder(lpFolder, true)	// do an AddRef so we don't take ownership of the folder
, m_ptrMapper(ptrMapper)
{ }

HRESULT Copier::Helper::CreateArchivedMessage(LPMESSAGE lpSource, const SObjectEntry &archiveEntry, const SObjectEntry &refMsgEntry, LPMESSAGE *lppArchivedMsg, PostSaveActionPtr *lpptrPSAction)
{
	HRESULT hr = hrSuccess;
	MAPIFolderPtr ptrArchiveFolder;
	MessagePtr ptrNewMessage;
	PostSaveActionPtr ptrPSAction;
	
	hr = GetArchiveFolder(archiveEntry, &ptrArchiveFolder);
	if (hr != hrSuccess)
		goto exit;

	hr = ptrArchiveFolder->CreateMessage(&ptrNewMessage.iid, fMapiDeferredErrors, &ptrNewMessage);
	if (hr != hrSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Failed to create archive message. (hr=%s)", stringify(hr, true).c_str());
		goto exit;
	}

	hr = ArchiveMessage(lpSource, &refMsgEntry, ptrNewMessage, &ptrPSAction);
	if (hr != hrSuccess)
		goto exit;

	hr = ptrNewMessage->QueryInterface(IID_IMessage, (LPVOID*)lppArchivedMsg);
	if (hr != hrSuccess)
		goto exit;

	lpptrPSAction->swap(ptrPSAction);

exit:
	return hr;
}

HRESULT Copier::Helper::GetArchiveFolder(const SObjectEntry &archiveEntry, LPMAPIFOLDER *lppArchiveFolder)
{
	HRESULT hr = hrSuccess;
	ArchiveFolderMap::const_iterator iArchiveFolder;
	MAPIFolderPtr ptrArchiveFolder;

	if (lppArchiveFolder == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}
	
	iArchiveFolder = m_mapArchiveFolders.find(archiveEntry.sStoreEntryId);
	if (iArchiveFolder == m_mapArchiveFolders.end()) {
		ArchiveHelperPtr ptrArchiveHelper;

		m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Archive folder not found in cache");
	
		// Find the associated archive folder 
		hr = ArchiveHelper::Create(m_ptrSession, archiveEntry, m_lpLogger, &ptrArchiveHelper);
		if (hr != hrSuccess)
			goto exit;
		
		hr = ptrArchiveHelper->GetArchiveFolderFor(m_ptrFolder, m_ptrSession, &ptrArchiveFolder);
		if (hr != hrSuccess) {
			m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Failed to get archive folder. (hr=%s)", stringify(hr, true).c_str());
			goto exit;
		}

		m_mapArchiveFolders.insert(ArchiveFolderMap::value_type(archiveEntry.sStoreEntryId, ptrArchiveFolder)); 			
	} else {
		m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Archive folder found in cache");
		ptrArchiveFolder = iArchiveFolder->second;
	}

	{
		SizedSPropTagArray(2, sptaProps) = {2, {PR_DISPLAY_NAME_A, PR_ENTRYID}};
		
		SPropArrayPtr props;
		ULONG cb;
		HRESULT hrTmp = ptrArchiveFolder->GetProps((LPSPropTagArray)&sptaProps, 0, &cb, &props);
		if (!FAILED(hrTmp)) {
			if (PROP_TYPE(props[0].ulPropTag) != PT_ERROR)
				m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Archive folder: %s", props[0].Value.lpszA);
			else
				m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Archive folder: has no name");
			m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Archive folder entryid: %s", bin2hex(props[1].Value.bin.cb, props[1].Value.bin.lpb).c_str());
		}
	}


	hr = ptrArchiveFolder->QueryInterface(IID_IMAPIFolder, (LPVOID*)lppArchiveFolder);

exit:
	return hr;
}

HRESULT Copier::Helper::ArchiveMessage(LPMESSAGE lpSource, const SObjectEntry *lpMsgEntry, LPMESSAGE lpDest, PostSaveActionPtr *lpptrPSAction)
{
	HRESULT hr = hrSuccess;
	MAPIPropHelperPtr ptrMsgHelper;
	SPropValuePtr ptrEntryId;
	SPropValue sPropArchFlags = {0};
	PostSaveActionPtr ptrPSAction;

	if (lpSource == NULL || lpDest == NULL)
		return MAPI_E_INVALID_PARAMETER;	// Don't use goto so we can use the PROPMAP macros after checking lpDest

	PROPMAP_START
	PROPMAP_NAMED_ID(FLAGS, PT_LONG, PSETID_Archive, dispidFlags)
	PROPMAP_INIT(lpDest)
	
	hr = lpSource->CopyTo(0, NULL, m_lpExcludeProps, 0, NULL, &IID_IMessage, lpDest, 0, NULL);
	// @todo: What to do with warnings?
	if (FAILED(hr)) {
		m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Failed to copy message. (hr=%s)", stringify(hr, true).c_str());
		goto exit;
	}

	hr = UpdateIIDs(lpSource, lpDest, &ptrPSAction);
	if (hr != hrSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Failed to update single instance IDs, continuing with copies. (hr=0x%08x)", hr);
		hr = hrSuccess;
	}

	sPropArchFlags.ulPropTag = PROP_FLAGS;
	sPropArchFlags.Value.ul = ARCH_NEVER_DELETE | ARCH_NEVER_STUB;

	hr = lpDest->SetProps(1, &sPropArchFlags, NULL);
	if (hr != hrSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Failed to set flags on archive message. (hr=%s)", stringify(hr, true).c_str());
		goto exit;
	}

	hr = MAPIPropHelper::Create(MAPIPropPtr(lpDest, true), &ptrMsgHelper);
	if (hr != hrSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Failed to create prop helper. (hr=%s)", stringify(hr, true).c_str());
		goto exit;
	}

	if (lpMsgEntry) {
		hr = ptrMsgHelper->SetReference(*lpMsgEntry);
		if (hr != hrSuccess) {
			m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Failed to set reference to original message. (hr=%s)", stringify(hr, true).c_str());
			goto exit;
		}
	}

	lpptrPSAction->swap(ptrPSAction);

exit:
	return hr;
}

HRESULT Copier::Helper::UpdateIIDs(LPMESSAGE lpSource, LPMESSAGE lpDest, PostSaveActionPtr *lpptrPSAction)
{
	HRESULT hr = hrSuccess;
	MAPITablePtr ptrSourceTable;
	MAPITablePtr ptrDestTable;
	ULONG ulSourceRows = 0;
	ULONG ulDestRows = 0;
	SPropValuePtr ptrSourceServerUID;
	SPropValuePtr ptrDestServerUID;
	TaskList lstDeferred;

	SizedSPropTagArray(1, sptaAttachProps) = {1, {PR_ATTACH_NUM}};
	enum {IDX_ATTACH_NUM};

	if (lpSource == NULL || lpDest == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	hr = HrGetOneProp(lpSource, PR_EC_SERVER_UID, &ptrSourceServerUID);
	if (hr != hrSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Failed to get source server UID, hr=0x%08x", hr);
		goto exit;
	}

	hr = HrGetOneProp(lpDest, PR_EC_SERVER_UID, &ptrDestServerUID);
	if (hr != hrSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Failed to get dest server UID, hr=0x%08x", hr);
		goto exit;
	}

	if (Util::CompareSBinary(ptrSourceServerUID->Value.bin, ptrDestServerUID->Value.bin) == 0) {
		m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Source and destination live on the same server, no explicit deduplication required.");
		goto exit;
	}

	hr = lpSource->GetAttachmentTable(0, &ptrSourceTable);
	if (hr != hrSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Failed to get source attachment table. hr=0x%08x", hr);
		goto exit;
	}

	hr = ptrSourceTable->SetColumns((LPSPropTagArray)&sptaAttachProps, TBL_BATCH);
	if (hr != hrSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Failed to set source attachment columns. hr=0x%08x", hr);
		goto exit;
	}

	hr = ptrSourceTable->GetRowCount(0, &ulSourceRows);
	if (hr != hrSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Failed to get source attachment count. hr=0x%08x", hr);
		goto exit;
	}

	if (ulSourceRows == 0) {
		m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "No attachments in source message, nothing to deduplicate.");
		goto exit;
	}

	hr = lpDest->GetAttachmentTable(0, &ptrDestTable);
	if (hr != hrSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Failed to get dest attachment table. hr=0x%08x", hr);
		goto exit;
	}

	hr = ptrDestTable->SetColumns((LPSPropTagArray)&sptaAttachProps, TBL_BATCH);
	if (hr != hrSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Failed to set dest attachment columns. hr=0x%08x", hr);
		goto exit;
	}

	hr = ptrDestTable->GetRowCount(0, &ulDestRows);
	if (hr != hrSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Failed to get dest attachment count. hr=0x%08x", hr);
		goto exit;
	}

	if (ulSourceRows != ulDestRows) {
		m_lpLogger->Log(EC_LOGLEVEL_WARNING, "Source has %u attachments, destination has %u. No idea how to match them...", ulSourceRows, ulDestRows);
		hr = MAPI_E_NO_SUPPORT;
		goto exit;
	}

	// We'll go through the table one row at a time (from each table) and assume the attachments
	// are sorted the same. We'll do a sanity check on the size propertie though.
	while (true) {
		mapi_rowset_ptr ptrSourceRows;
		mapi_rowset_ptr ptrDestRows;

		hr = ptrSourceTable->QueryRows(16, 0, &ptrSourceRows);
		if (hr != hrSuccess) {
			m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Failed to query source rows. hr=0x%08x", hr);
			goto exit;
		}

		if (ptrSourceRows.empty())
			break;

		hr = ptrDestTable->QueryRows(16, 0, &ptrDestRows);
		if (hr != hrSuccess) {
			m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Failed to query source rows. hr=0x%08x", hr);
			goto exit;
		}

		ASSERT(ptrSourceRows.size() == ptrDestRows.size());
		for (mapi_rowset_ptr::size_type i = 0; i < ptrSourceRows.size(); ++i) {
			HRESULT hrTmp = hrSuccess;
			AttachPtr ptrSourceAttach;
			SPropValuePtr ptrAttachMethod;
			AttachPtr ptrDestAttach;
			ECSingleInstancePtr ptrInstance;
			ULONG cbSourceSIID;
			EntryIdPtr ptrSourceSIID;
			ULONG cbDestSIID;
			EntryIdPtr ptrDestSIID;

			hrTmp = lpSource->OpenAttach(ptrSourceRows[i].lpProps[IDX_ATTACH_NUM].Value.ul, NULL, MAPI_DEFERRED_ERRORS, &ptrSourceAttach);
			if (hrTmp != hrSuccess) {
				m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Failed to open source attachment %u. Skipping attachment. hr=0x%08x", i, hrTmp);
				continue;
			}

			hrTmp = HrGetOneProp(ptrSourceAttach, PR_ATTACH_METHOD, &ptrAttachMethod);
			if (hrTmp == MAPI_E_NOT_FOUND) {
				m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "No PR_ATTACH_METHOD found for attachment %u, assuming NO_ATTACHMENT. So nothing to deduplicate.", i);
				continue;
			}
			if (hrTmp != hrSuccess) {
				m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Failed to get PR_ATTACH_METHOD for attachment %u. Skipping attachment. hr=0x%08x", i, hrTmp);
				continue;
			}
			if (ptrAttachMethod->Value.ul != ATTACH_BY_VALUE) {
				m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Attachment method for attachment %u is not ATTACH_BY_VALUE. So nothing to deduplicate.", i);
				continue;
			}

			hrTmp = ptrSourceAttach->QueryInterface(ptrInstance.iid, &ptrInstance);
			if (hrTmp != hrSuccess) {
				m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Unable to get single instance interface for source attachment %u. Skipping attachment. hr=0x%08x", i, hrTmp);
				continue;
			}

			hrTmp = ptrInstance->GetSingleInstanceId(&cbSourceSIID, &ptrSourceSIID);
			if (hrTmp != hrSuccess) {
				m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Unable to get single instance ID for source attachment %u. Skipping attachment. hr=0x%08x", i, hrTmp);
				continue;
			}
			if (cbSourceSIID == 0 || !ptrSourceSIID) {
				m_lpLogger->Log(EC_LOGLEVEL_WARNING, "Got empty single instance ID for attachment %u. That's not suitable for deduplication.", i);
				continue;
			}

			hrTmp = m_ptrMapper->GetMappedInstanceId(ptrSourceServerUID->Value.bin, cbSourceSIID, ptrSourceSIID, ptrDestServerUID->Value.bin, &cbDestSIID, &ptrDestSIID);
			if (hrTmp == MAPI_E_NOT_FOUND) {
				m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "No mapped IID found, list message for deferred creation of mapping");

				lstDeferred.push_back(TaskPtr(new TaskMapInstanceId(ptrSourceAttach, MessagePtr(lpDest, true), i)));
				continue;
			}
			if (hrTmp != hrSuccess) {
				m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Unable to get mapped instance ID for attachment %u. Skipping attachment. hr=0x%08x", i, hrTmp);
				continue;
			}

			hrTmp = lpDest->OpenAttach(ptrDestRows[i].lpProps[IDX_ATTACH_NUM].Value.ul, NULL, MAPI_DEFERRED_ERRORS, &ptrDestAttach);
			if (hrTmp != hrSuccess) {
				m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Failed to open dest attachment %u. Skipping attachment. hr=0x%08x", i, hrTmp);
				continue;
			}

			hrTmp = ptrDestAttach->QueryInterface(ptrInstance.iid, &ptrInstance);
			if (hrTmp != hrSuccess) {
				m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Unable to get single instance interface for dest attachment %u. Skipping attachment. hr=0x%08x", i, hrTmp);
				continue;
			}

			hrTmp = ptrInstance->SetSingleInstanceId(cbDestSIID, ptrDestSIID);
			if (hrTmp != hrSuccess) {
				m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Unable to set single instance ID for dest attachment %u. hr=0x%08x", i, hrTmp);
				continue;
			}

			hrTmp = ptrDestAttach->SaveChanges(0);
			if (hrTmp != hrSuccess) {
				m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Unable to save single instance ID for dest attachment %u. hr=0x%08x", i, hrTmp);
				continue;
			}

			lstDeferred.push_back(TaskPtr(new TaskVerifyAndUpdateInstanceId(ptrSourceAttach, MessagePtr(lpDest, true), i, cbDestSIID, ptrDestSIID)));
		}
	}

	if (lstDeferred.empty())
		lpptrPSAction->reset();
	else
		lpptrPSAction->reset(new PostSaveInstanceIdUpdater(PR_ATTACH_DATA_BIN, m_ptrMapper, lstDeferred), boost::checked_deleter<PostSaveInstanceIdUpdater>());

exit:
	return hr;
}




/**
 * Constructor
 *
 * @param[in]	lpSession
 *					Pointer to the session.
 * @param[in]	lpLogger
 *					Pointer to the logger.
 * @param[in]	lstArchives
 *					The list of attached archives for this store.
 * @param[in]	lpExcludeProps
 *					The list of properties that will not be copied during the archive operation.
 *
 * @return HRESULT
 */
Copier::Copier(SessionPtr ptrSession, ECConfig *lpConfig, ECLogger *lpLogger, const ObjectEntryList &lstArchives, LPSPropTagArray lpExcludeProps, int ulAge, bool bProcessUnread)
: ArchiveOperationBaseEx(lpLogger, ulAge, bProcessUnread, ARCH_NEVER_ARCHIVE)
, m_ptrSession(ptrSession)
, m_lpConfig(lpConfig)
, m_lstArchives(lstArchives)
, m_ptrTransaction(new Transaction(SObjectEntry()))
{
	MAPIAllocateBuffer(CbNewSPropTagArray(lpExcludeProps->cValues), &m_ptrExcludeProps);
	memcpy(m_ptrExcludeProps, lpExcludeProps, CbNewSPropTagArray(lpExcludeProps->cValues));

	// If the next call fails, m_ptrMapper will have NULL ptr, which we'll check later.
	InstanceIdMapper::Create(lpLogger, lpConfig, &m_ptrMapper);
}

Copier::~Copier()
{
	m_ptrTransaction->PurgeDeletes(m_ptrSession);
}

HRESULT Copier::GetRestriction(LPMAPIPROP lpMapiProp, LPSRestriction *lppRestriction)
{
	HRESULT hr = hrSuccess;
	ECOrRestriction resResult;
	SRestrictionPtr ptrRestriction;

	PROPMAP_START
	PROPMAP_NAMED_ID(ORIGINAL_SOURCE_KEY, PT_BINARY, PSETID_Archive, dispidOrigSourceKey)
	PROPMAP_INIT(lpMapiProp)

	// Start out with the base restriction, which checks if a message is
	// old enough to be processed.
	hr = ArchiveOperationBaseEx::GetRestriction(lpMapiProp, &ptrRestriction);
	if (hr != hrSuccess)
		goto exit;
	resResult.append(ECRawRestriction(ptrRestriction));

	// A reason to process a message before being old enough is when
	// it's already archived (by archive-on-delivery or because the required
	// age has changed). We'll check that by checking if PROP_ORIGINAL_SOURCE_KEY
	// is present.
	resResult.append(ECExistRestriction(PROP_ORIGINAL_SOURCE_KEY));

	hr = resResult.CreateMAPIRestriction(lppRestriction);

exit:
	return hr;
}

HRESULT Copier::EnterFolder(LPMAPIFOLDER lpFolder)
{
	if (!m_ptrMapper)
		return MAPI_E_UNCONFIGURED;

	m_ptrHelper.reset(new Helper(m_ptrSession, Logger(), m_ptrMapper, m_ptrExcludeProps, lpFolder));
	return hrSuccess;
}

HRESULT Copier::LeaveFolder()
{
	if (!m_ptrMapper)
		return MAPI_E_UNCONFIGURED;

	m_ptrHelper.reset();
	return hrSuccess;
}

HRESULT Copier::DoProcessEntry(ULONG cProps, const LPSPropValue &lpProps)
{
	HRESULT hr = hrSuccess;
	LPSPropValue lpEntryId = NULL;
	LPSPropValue lpStoreEntryId = NULL;
	SObjectEntry refObjectEntry;
	MessagePtr ptrMessageRaw;
	MessagePtr ptrMessage;
	ULONG ulType = 0;
	MAPIPropHelperPtr ptrMsgHelper;
	MessageState state;
	ObjectEntryList lstMsgArchives;
	ObjectEntryList::iterator iArchive;
	HRESULT hrTemp;
	ObjectEntryList lstNewMsgArchives;
	TransactionList lstTransactions;
	RollbackList lstRollbacks;

	if (!m_ptrMapper) {
		hr = MAPI_E_UNCONFIGURED;
		goto exit;
	}

	lpEntryId = PpropFindProp(lpProps, cProps, PR_ENTRYID);
	if (lpEntryId == NULL) {
		Logger()->Log(EC_LOGLEVEL_FATAL, "PR_ENTRYID missing");
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}

	lpStoreEntryId = PpropFindProp(lpProps, cProps, PR_STORE_ENTRYID);
	if (lpStoreEntryId == NULL) {
		Logger()->Log(EC_LOGLEVEL_FATAL, "PR_STORE_ENTRYID missing");
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}

	// Set the reference to the original message
	refObjectEntry.sStoreEntryId.assign(lpStoreEntryId->Value.bin);
	refObjectEntry.sItemEntryId.assign(lpEntryId->Value.bin);

	Logger()->Log(EC_LOGLEVEL_DEBUG, "Opening message (%s)", bin2hex(lpEntryId->Value.bin.cb, lpEntryId->Value.bin.lpb).c_str());
	hr = CurrentFolder()->OpenEntry(lpEntryId->Value.bin.cb, (LPENTRYID)lpEntryId->Value.bin.lpb, &IID_IECMessageRaw, MAPI_MODIFY|fMapiDeferredErrors, &ulType, &ptrMessageRaw);
	if (hr != hrSuccess) {
		Logger()->Log(EC_LOGLEVEL_FATAL, "Failed to open message. (hr=%s)", stringify(hr, true).c_str());
		goto exit;
	}

	hr = VerifyRestriction(ptrMessageRaw);
	if (hr == MAPI_E_NOT_FOUND) {
		Logger()->Log(EC_LOGLEVEL_WARNING, "Ignoring message because it doesn't match the criteria for begin archived.");
		Logger()->Log(EC_LOGLEVEL_WARNING, "This can happen when huge amounts of message are being processed.");

		// This is not an error
		hr = hrSuccess;

		// We could process subrestrictions for this message, but that will be picked up at a later stage anyway.
		goto exit;
	} else if (hr != hrSuccess) {
		Logger()->Log(EC_LOGLEVEL_WARNING, "Failed to verify message criteria. (hr=%s)", stringify(hr, true).c_str());
		goto exit;
	}
	
	hr = MAPIPropHelper::Create(ptrMessageRaw.as<MAPIPropPtr>(), &ptrMsgHelper);
	if (hr != hrSuccess) {
		Logger()->Log(EC_LOGLEVEL_FATAL, "Failed to create prop helper. (hr=%s)", stringify(hr, true).c_str());
		goto exit;
	}

	hr = ptrMsgHelper->GetMessageState(m_ptrSession, &state);
	if (hr != hrSuccess) {
		Logger()->Log(EC_LOGLEVEL_FATAL, "Failed to determine message state. (hr=%s)", stringify(hr, true).c_str());
		goto exit;
	}

	if (state.isCopy() || state.isMove()) {
		Logger()->Log(EC_LOGLEVEL_INFO, "Found %s archive, treating it as a new message.", (state.isCopy() ? "copied" : "moved"));

		// Here we reopen the source message with destubbing enabled. This message
		// will be used as source for creating the new archive message. Note that
		// we leave the ptrMsgHelper as is, operating on the raw message.
		hr = CurrentFolder()->OpenEntry(lpEntryId->Value.bin.cb, (LPENTRYID)lpEntryId->Value.bin.lpb, &IID_IMessage, fMapiDeferredErrors, &ulType, &ptrMessage);
		if (hr != hrSuccess) {
			Logger()->Log(EC_LOGLEVEL_FATAL, "Failed to reopen message. (hr=%s)", stringify(hr, true).c_str());
			goto exit;
		}
	} else
		ptrMessage = ptrMessageRaw;

	// From here on we work on ptrMessage, except for ExecuteSubOperations.
	if (!state.isCopy()) {		// Include state.isMove()
		hr = ptrMsgHelper->GetArchiveList(&lstMsgArchives);
		if (hr != hrSuccess) {
			if (hr == MAPI_E_CORRUPT_DATA) {
				Logger()->Log(EC_LOGLEVEL_ERROR, "Existing list of archives is corrupt, skipping message.");
				hr = hrSuccess;
			} else
				Logger()->Log(EC_LOGLEVEL_FATAL, "Failed to get list of archives. (hr=%s)", stringify(hr, true).c_str());
			goto exit;
		}
	}
		
	for (iArchive = m_lstArchives.begin(); iArchive != m_lstArchives.end(); ++iArchive) {
		TransactionPtr ptrTransaction;
		ObjectEntryList::iterator iArchivedMsg = find_if(lstMsgArchives.begin(), lstMsgArchives.end(), StoreCompare(*iArchive));
		// Check if this message is archived to the current archive.
		if (iArchivedMsg == lstMsgArchives.end()) {
			// It's possible to have a dirty message that has not been archived to the current archive if at the time of the
			// previous archiver run, this archive was unavailable for some reason.
			// If this happens, we'll can't do much more than just archive the current version of the message to the archive.
			//
			// Alternatively we could get the previous version from another archive and copy that to this archive if the
			// configuration dictates that old archives are linked. We won't do that though!

			Logger()->Log(EC_LOGLEVEL_DEBUG, "Message not yet archived to archive (%s)", iArchive->sStoreEntryId.tostring().c_str());
			hr = DoInitialArchive(ptrMessage, *iArchive, refObjectEntry, &ptrTransaction);

		} else if (state.isDirty()) {
			// We found an archived version for the current message. However, the message is marked dirty...
			// There are two things we can do:
			// 1. Update the current archived version.
			// 2. Make a new archived message and link that to the old version, basically tracking history.
			//
			// This will be configurable through the track_history option.

			Logger()->Log(EC_LOGLEVEL_INFO, "Found archive for dirty message.");
			if (parseBool(m_lpConfig->GetSetting("track_history"))) {
				Logger()->Log(EC_LOGLEVEL_DEBUG, "Creating new archived message.");
				// DoTrackAndRearchive will allways place the new archive in the
				// correct folder and place the old ones in the history folder.
				// However, when the message has moved, the ref entryids need to
				// be updated in the old messages.
				// DoTrackAndRearchive will do that when it detects that the passed
				// refObjectEntry is different than the stored reference it the most
				// recent archive.
				hr = DoTrackAndRearchive(ptrMessage, *iArchive, *iArchivedMsg, refObjectEntry, state.isMove(), &ptrTransaction);

			} else if (!state.isMove()) {
				Logger()->Log(EC_LOGLEVEL_DEBUG, "Updating archived message.");
				hr = DoUpdateArchive(ptrMessage, *iArchivedMsg, refObjectEntry, &ptrTransaction);
			} else {	// Moved, dirty and not tracking history
				Logger()->Log(EC_LOGLEVEL_DEBUG, "Updating and moving archived message.");
				// We could do a move and update here, but since we're not tracking history
				// we can just make a new archived message and get rid of the old one.
				hr = DoInitialArchive(ptrMessage, *iArchive, refObjectEntry, &ptrTransaction);
				if (hr == hrSuccess)
					hr = ptrTransaction->Delete(*iArchivedMsg);
			}

		} else if (!state.isMove()) {
			Logger()->Log(EC_LOGLEVEL_WARNING, "Ignoring already archived message.");
			ptrTransaction.reset(new Transaction(*iArchivedMsg));
		} else {	// Moved
			Logger()->Log(EC_LOGLEVEL_DEBUG, "Moving archived message.");
			hr = DoMoveArchive(*iArchive, *iArchivedMsg, refObjectEntry, &ptrTransaction);
		}
		if (hr != hrSuccess)
			goto exit;

		lstTransactions.push_back(ptrTransaction);
	}

	// Once we reach this point all messages have been created and/or updated. We need to
	// save them now. When a transaction is saved it will return a Rollback object we can
	// use to undo the changes when a later save fails.
	for (TransactionList::const_iterator iTransaction = lstTransactions.begin(); iTransaction != lstTransactions.end(); ++iTransaction) {
		RollbackPtr ptrRollback;
		hr = (*iTransaction)->SaveChanges(m_ptrSession, &ptrRollback);
		if (FAILED(hr)) {
			Logger()->Log(EC_LOGLEVEL_FATAL, "Failed to save changes into archive, Rolling back. hr=0x%08x", hr);

			// Rollback
			for (RollbackList::const_iterator iRollback = lstRollbacks.begin(); iRollback != lstRollbacks.end(); ++iRollback) {
				HRESULT hrTmp = (*iRollback)->Execute(m_ptrSession);
				if (hrTmp != hrSuccess)
					Logger()->Log(EC_LOGLEVEL_ERROR, "Failed to rollback transaction. The archive is consistent, but possibly cluttered. hr=0x%08x", hrTmp);
			}
			goto exit;
		}
		hr = hrSuccess;
		lstRollbacks.push_back(ptrRollback);
		lstNewMsgArchives.push_back((*iTransaction)->GetObjectEntry());
	}

	if (state.isDirty()) {
		hr = ptrMsgHelper->SetClean();
		if (hr != hrSuccess) {
			Logger()->Log(EC_LOGLEVEL_WARNING, "Failed to unmark message as dirty.");
			goto exit;
		}
	}

	lstNewMsgArchives.sort();
	lstNewMsgArchives.unique();

	hr = ptrMsgHelper->SetArchiveList(lstNewMsgArchives, true);
	if (hr != hrSuccess) {
		Logger()->Log(EC_LOGLEVEL_FATAL, "Failed to save list of archives for this message. (hr=0x%08x)", hr);
		goto exit;
	}

	for (TransactionList::const_iterator iTransaction = lstTransactions.begin(); iTransaction != lstTransactions.end(); ++iTransaction) {
		HRESULT hrTmp = (*iTransaction)->PurgeDeletes(m_ptrSession, m_ptrTransaction);
		if (hrTmp != hrSuccess)
			Logger()->Log(EC_LOGLEVEL_ERROR, "Failed to remove old archives. (hr=0x%08x)", hrTmp);
	}
	
	hrTemp = ExecuteSubOperations(ptrMessageRaw, CurrentFolder(), cProps, lpProps);
	if (hrTemp != hrSuccess)
		Logger()->Log(EC_LOGLEVEL_WARNING, "Unable to execute next operation, hr=%08x. The operation is postponed, not cancelled", hrTemp);

exit:
	return hr;
}

void Copier::SetDeleteOperation(DeleterPtr ptrDeleteOp)
{
	m_ptrDeleteOp = ptrDeleteOp;
}

void Copier::SetStubOperation(StubberPtr ptrStubOp)
{
	m_ptrStubOp = ptrStubOp;
}

HRESULT Copier::DoInitialArchive(LPMESSAGE lpMessage, const SObjectEntry &archiveRootEntry, const SObjectEntry &refMsgEntry, TransactionPtr *lpptrTransaction)
{
	HRESULT hr = hrSuccess;
	MessagePtr ptrNewArchive;
	SPropValuePtr ptrEntryId;
	SObjectEntry objectEntry;
	PostSaveActionPtr ptrPSAction;
	TransactionPtr ptrTransaction;

	ASSERT(lpMessage != NULL);
	ASSERT(lpptrTransaction != NULL);
	
	hr = m_ptrHelper->CreateArchivedMessage(lpMessage, archiveRootEntry, refMsgEntry, &ptrNewArchive, &ptrPSAction);
	if (hr != hrSuccess)
		goto exit;
	
	hr = HrGetOneProp(ptrNewArchive, PR_ENTRYID, &ptrEntryId);
	if (hr != hrSuccess) {
		Logger()->Log(EC_LOGLEVEL_FATAL, "Failed to get entry id of archive message. (hr=0x%08x", hr);
		goto exit;
	}

	// Update the list of archive messages for this message.
	objectEntry.sStoreEntryId.assign(archiveRootEntry.sStoreEntryId);
	objectEntry.sItemEntryId.assign(ptrEntryId->Value.bin);

	ptrTransaction.reset(new Transaction(objectEntry));
	hr = ptrTransaction->Save(ptrNewArchive, true, ptrPSAction);
	if (hr != hrSuccess) {
		Logger()->Log(EC_LOGLEVEL_FATAL, "Failed to add archive message to transaction. (hr=0x%08x", hr);
		goto exit;
	}

	*lpptrTransaction = ptrTransaction;

exit:
	return hr;
}

HRESULT Copier::DoTrackAndRearchive(LPMESSAGE lpMessage, const SObjectEntry &archiveRootEntry, const SObjectEntry &archiveMsgEntry, const SObjectEntry &refMsgEntry, bool bUpdateHistory, TransactionPtr *lpptrTransaction)
{
	HRESULT hr = hrSuccess;
	MessagePtr ptrNewArchive;
	SObjectEntry newArchiveEntry;
	MAPIPropHelperPtr ptrMsgHelper;
	SPropValuePtr ptrEntryId;
	SObjectEntry movedEntry;
	MessagePtr ptrMovedMessage;
	PostSaveActionPtr ptrPSAction;
	TransactionPtr ptrTransaction;

	ASSERT(lpMessage != NULL);
	ASSERT(lpptrTransaction != NULL);


	hr = m_ptrHelper->CreateArchivedMessage(lpMessage, archiveRootEntry, refMsgEntry, &ptrNewArchive, &ptrPSAction);
	if (hr != hrSuccess)
		goto exit;

	hr = HrGetOneProp(ptrNewArchive, PR_ENTRYID, &ptrEntryId);
	if (hr != hrSuccess) {
		Logger()->Log(EC_LOGLEVEL_FATAL, "Failed to get entry id of archive message. (hr=0x%08x)", hr);
		goto exit;
	}

	// Create the transaction, which is needed by CopyToHistory, now.
	newArchiveEntry.sStoreEntryId.assign(archiveRootEntry.sStoreEntryId);
	newArchiveEntry.sItemEntryId.assign(ptrEntryId->Value.bin);
	ptrTransaction.reset(new Transaction(newArchiveEntry));

	hr = MoveToHistory(archiveRootEntry, archiveMsgEntry, ptrTransaction, &movedEntry, &ptrMovedMessage);
	if (hr != hrSuccess) {
		Logger()->Log(EC_LOGLEVEL_ERROR, "Failed to move old archive to history folder. (hr=0x%08x)", hr);
		goto exit;
	}

	hr = MAPIPropHelper::Create(ptrNewArchive.as<MAPIPropPtr>(), &ptrMsgHelper);
	if (hr != hrSuccess) {
		Logger()->Log(EC_LOGLEVEL_FATAL, "Failed to create prop helper. (hr=0x%08x)", hr);
		goto exit;
	}

	hr = ptrMsgHelper->ReferencePrevious(movedEntry);
	if (hr != hrSuccess) {
		Logger()->Log(EC_LOGLEVEL_FATAL, "Failed to set reference to previous archive. (hr=0x%08x)", hr);
		goto exit;
	}
	
	hr = ptrTransaction->Save(ptrNewArchive, true, ptrPSAction);
	if (hr != hrSuccess) {
		Logger()->Log(EC_LOGLEVEL_FATAL, "Failed to add new archive message to transaction. (hr=0x%08x", hr);
		goto exit;
	}

	if (bUpdateHistory) {
		ASSERT(ptrMovedMessage);

		// Since the first history message was just moved but not yet saved, we'll set that
		// reference here. The other history messages do exist on the server, so those can
		// be updated through UpdateHistoryRefs.

		hr = MAPIPropHelper::Create(ptrMovedMessage.as<MAPIPropPtr>(), &ptrMsgHelper);
		if (hr != hrSuccess) {
			Logger()->Log(EC_LOGLEVEL_FATAL, "Failed to create prop helper. (hr=0x%08x)", hr);
			goto exit;
		}

		hr = ptrMsgHelper->SetReference(refMsgEntry);
		if (hr != hrSuccess) {
			Logger()->Log(EC_LOGLEVEL_FATAL, "Failed to set reference. (hr=0x%08x)", hr);
			goto exit;
		}
		
		hr = UpdateHistoryRefs(ptrMovedMessage, refMsgEntry, ptrTransaction);
		if (hr != hrSuccess)
			goto exit;
	}

	*lpptrTransaction = ptrTransaction;

exit:
	return hr;
}

HRESULT Copier::DoUpdateArchive(LPMESSAGE lpMessage, const SObjectEntry &archiveMsgEntry, const SObjectEntry &refMsgEntry, TransactionPtr *lpptrTransaction)
{
	HRESULT hr = hrSuccess;
	MsgStorePtr ptrArchiveStore;
	ULONG ulType;
	MessagePtr ptrArchivedMsg;
	SPropTagArrayPtr ptrPropList;
	PostSaveActionPtr ptrPSAction;
	TransactionPtr ptrTransaction;

	ASSERT(lpMessage != NULL);
	ASSERT(lpptrTransaction != NULL);

	hr = m_ptrSession->OpenStore(archiveMsgEntry.sStoreEntryId, &ptrArchiveStore);
	if (hr != hrSuccess) {
		Logger()->Log(EC_LOGLEVEL_FATAL, "Failed to open archive store. (hr=%s)", stringify(hr, true).c_str());
		goto exit;
	}

	/**
	 * We used to get the raw message here to ensure we're not getting the extra destubbing layer here. However, the messages
	 * in the archive should never be stubbed, so there's no problem.
	 * The main reason to not try to get the raw message through IID_IECMessageRaw is that archive stores don't support it.
	 * @todo Should we verify if the item is really not stubbed?
	 */
	hr = ptrArchiveStore->OpenEntry(archiveMsgEntry.sItemEntryId.size(), archiveMsgEntry.sItemEntryId, &ptrArchivedMsg.iid, MAPI_BEST_ACCESS|fMapiDeferredErrors, &ulType, &ptrArchivedMsg);
	if (hr != hrSuccess) {
		Logger()->Log(EC_LOGLEVEL_FATAL, "Failed to open existing archived message. (hr=%s)", stringify(hr, true).c_str());
		goto exit;
	}

	hr = ptrArchivedMsg->GetPropList(fMapiUnicode, &ptrPropList);
	if (hr != hrSuccess) {
		Logger()->Log(EC_LOGLEVEL_FATAL, "Failed to get property list. (hr=%s)", stringify(hr, true).c_str());
		goto exit;
	}

	hr = ptrArchivedMsg->DeleteProps(ptrPropList, NULL);
	if (hr != hrSuccess) {
		Logger()->Log(EC_LOGLEVEL_FATAL, "Failed to delete properties. (hr=%s)", stringify(hr, true).c_str());
		goto exit;
	}

	hr = Util::HrDeleteAttachments(ptrArchivedMsg);
	if (hr != hrSuccess) {
		Logger()->Log(EC_LOGLEVEL_FATAL, "Failed to delete attachments. (hr=%s)", stringify(hr, true).c_str());
		goto exit;
	}

	hr = Util::HrDeleteRecipients(ptrArchivedMsg);
	if (hr != hrSuccess) {
		Logger()->Log(EC_LOGLEVEL_FATAL, "Failed to delete recipients. (hr=%s)", stringify(hr, true).c_str());
		goto exit;
	}

	hr = m_ptrHelper->ArchiveMessage(lpMessage, &refMsgEntry, ptrArchivedMsg, &ptrPSAction);
	if (hr != hrSuccess)
		goto exit;

	ptrTransaction.reset(new Transaction(archiveMsgEntry));
	hr = ptrTransaction->Save(ptrArchivedMsg, false, ptrPSAction);
	if (hr != hrSuccess) {
		Logger()->Log(EC_LOGLEVEL_FATAL, "Failed to add archive message to transaction. (hr=0x%08x", hr);
		goto exit;
	}

	*lpptrTransaction = ptrTransaction;
	
exit:
	return hr;
}

HRESULT Copier::DoMoveArchive(const SObjectEntry &archiveRootEntry, const SObjectEntry &archiveMsgEntry, const SObjectEntry &refMsgEntry, TransactionPtr *lpptrTransaction)
{
	HRESULT hr = hrSuccess;
	MAPIFolderPtr ptrArchiveFolder;
	MsgStorePtr ptrArchiveStore;
	ULONG ulType;
	MessagePtr ptrArchive;
	MessagePtr ptrArchiveCopy;
	MAPIPropHelperPtr ptrPropHelper;
	SPropValuePtr ptrEntryId;
	SObjectEntry objectEntry;
	TransactionPtr ptrTransaction;

	ASSERT(lpptrTransaction != NULL);


	hr = m_ptrHelper->GetArchiveFolder(archiveRootEntry, &ptrArchiveFolder);
	if (hr != hrSuccess)
		goto exit;


	hr = m_ptrSession->OpenStore(archiveMsgEntry.sStoreEntryId, &ptrArchiveStore);
	if (hr != hrSuccess)
		goto exit;

	hr = ptrArchiveStore->OpenEntry(archiveMsgEntry.sItemEntryId.size(), archiveMsgEntry.sItemEntryId, &ptrArchive.iid, 0, &ulType, &ptrArchive);
	if (hr != hrSuccess)
		goto exit;


	hr = ptrArchiveFolder->CreateMessage(&ptrArchiveCopy.iid, 0, &ptrArchiveCopy);
	if (hr != hrSuccess)
		goto exit;

	hr = ptrArchive->CopyTo(0, NULL, NULL, 0, NULL, &ptrArchiveCopy.iid, ptrArchiveCopy, 0, NULL);
	if (hr != hrSuccess)
		goto exit;

	hr = MAPIPropHelper::Create(ptrArchiveCopy.as<MAPIPropPtr>(), &ptrPropHelper);
	if (hr != hrSuccess)
		goto exit;

	hr = ptrPropHelper->SetReference(refMsgEntry);
	if (hr != hrSuccess)
		goto exit;

	hr = HrGetOneProp(ptrArchiveCopy, PR_ENTRYID, &ptrEntryId);
	if (hr != hrSuccess) {
		Logger()->Log(EC_LOGLEVEL_FATAL, "Failed to get entry id of archive message. (hr=0x%08x)", hr);
		goto exit;
	}

	// Create the transaction, which is needed by CopyToHistory, now.
	objectEntry.sStoreEntryId.assign(archiveRootEntry.sStoreEntryId);
	objectEntry.sItemEntryId.assign(ptrEntryId->Value.bin);

	ptrTransaction.reset(new Transaction(objectEntry));
	hr = ptrTransaction->Save(ptrArchiveCopy, true);
	if (hr == hrSuccess)
		hr = ptrTransaction->Delete(archiveMsgEntry);
	if (hr != hrSuccess)
		goto exit;

	hr = UpdateHistoryRefs(ptrArchiveCopy, refMsgEntry, ptrTransaction);
	if (hr != hrSuccess)
		goto exit;

	*lpptrTransaction = ptrTransaction;

exit:
	return hr;
}

HRESULT Copier::ExecuteSubOperations(LPMESSAGE lpMessage, LPMAPIFOLDER lpFolder, ULONG cProps, const LPSPropValue lpProps)
{
	HRESULT hr = hrSuccess;
	list<ArchiveOperationPtr>::iterator iOp;

	ASSERT(lpMessage != NULL);
	ASSERT(lpFolder != NULL);
	if (lpMessage == NULL || lpFolder == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	if (!m_ptrDeleteOp && !m_ptrStubOp)
		goto exit;

	// First see if the deleter restriction matches, in that case we run the deleter
	// and be done with it.
	if (m_ptrDeleteOp) {
		hr = m_ptrDeleteOp->VerifyRestriction(lpMessage);
		if (hr == hrSuccess) {
			Logger()->Log(EC_LOGLEVEL_DEBUG, "Executing delete operation.");
			hr = m_ptrDeleteOp->ProcessEntry(lpFolder, cProps, lpProps);
			if (hr != hrSuccess)
				Logger()->Log(EC_LOGLEVEL_WARNING, "Delete operation failed, postponing next attempt. hr=0x%08x", hr);
			else
				Logger()->Log(EC_LOGLEVEL_DEBUG, "Delete operation executed.");
			goto exit;	// No point in trying to stub a deleted message.
		} else if (hr != MAPI_E_NOT_FOUND)
			goto exit;

		hr = hrSuccess;
		Logger()->Log(EC_LOGLEVEL_DEBUG, "Message is not elegible for deletion.");
	}

	// Now see if we need to stub the message.
	if (m_ptrStubOp) {
		hr = m_ptrStubOp->VerifyRestriction(lpMessage);
		if (hr == hrSuccess) {
			Logger()->Log(EC_LOGLEVEL_DEBUG, "Executing stub operation.");
			hr = m_ptrStubOp->ProcessEntry(lpMessage);
			if (hr != hrSuccess)
				Logger()->Log(EC_LOGLEVEL_WARNING, "Stub operation failed, postponing next attempt. hr=0x%08x", hr);
			else
				Logger()->Log(EC_LOGLEVEL_DEBUG, "Stub operation executed.");
		} else if (hr == MAPI_E_NOT_FOUND) {
			hr = hrSuccess;
			Logger()->Log(EC_LOGLEVEL_DEBUG, "Message is not elegible for stubbing.");
		}
	}

exit:
	return hr;
}

HRESULT Copier::MoveToHistory(const SObjectEntry &sourceArchiveRoot, const SObjectEntry &sourceMsgEntry, TransactionPtr ptrTransaction, SObjectEntry *lpNewEntry, LPMESSAGE *lppNewMessage)
{
	HRESULT hr = hrSuccess;
	ArchiveHelperPtr ptrArchiveHelper;
	MAPIFolderPtr ptrHistoryFolder;
	ULONG ulType;
	MsgStorePtr ptrArchiveStore;
	MessagePtr ptrArchive;
	MessagePtr ptrArchiveCopy;
	SPropValuePtr ptrEntryID;

	hr = ArchiveHelper::Create(m_ptrSession, sourceArchiveRoot, Logger(), &ptrArchiveHelper);
	if (hr != hrSuccess)
		goto exit;

	hr = ptrArchiveHelper->GetHistoryFolder(&ptrHistoryFolder);
	if (hr != hrSuccess)
		goto exit;

	hr = m_ptrSession->OpenStore(sourceMsgEntry.sStoreEntryId, &ptrArchiveStore);
	if (hr != hrSuccess)
		goto exit;

	hr = ptrArchiveStore->OpenEntry(sourceMsgEntry.sItemEntryId.size(), sourceMsgEntry.sItemEntryId, &ptrArchive.iid, 0, &ulType, &ptrArchive);
	if (hr != hrSuccess)
		goto exit;

	hr = ptrHistoryFolder->CreateMessage(&ptrArchiveCopy.iid, 0, &ptrArchiveCopy);
	if (hr != hrSuccess)
		goto exit;

	hr = HrGetOneProp(ptrArchiveCopy, PR_ENTRYID, &ptrEntryID);
	if (hr != hrSuccess)
		goto exit;

	hr = ptrArchive->CopyTo(0, NULL, NULL, 0, NULL, &ptrArchiveCopy.iid, ptrArchiveCopy, 0, NULL);
	if (hr != hrSuccess)
		goto exit;

	hr = ptrTransaction->Save(ptrArchiveCopy, true);
	if (hr == hrSuccess)
		hr = ptrTransaction->Delete(sourceMsgEntry, true);
	if (hr != hrSuccess)
		goto exit;

	if (lppNewMessage) {
		hr = ptrArchiveCopy->QueryInterface(IID_IMessage, (LPVOID*)lppNewMessage);
		if (hr != hrSuccess)
			goto exit;
	}

	lpNewEntry->sStoreEntryId.assign(sourceMsgEntry.sStoreEntryId);
	lpNewEntry->sItemEntryId.assign(ptrEntryID->Value.bin);

exit:
	return hr;
}

HRESULT Copier::UpdateHistoryRefs(LPMESSAGE lpArchivedMsg, const SObjectEntry &refMsgEntry, TransactionPtr ptrTransaction)
{
	HRESULT hr = hrSuccess;
	MAPIPropHelperPtr ptrPropHelper;
	MessagePtr ptrMessage;

	hr = MAPIPropHelper::Create(MAPIPropPtr(lpArchivedMsg, true), &ptrPropHelper);
	if (hr != hrSuccess)
		goto exit;

	hr = ptrPropHelper->OpenPrevious(m_ptrSession, &ptrMessage);
	if (hr == MAPI_E_NOT_FOUND) {
		hr = hrSuccess;
		goto exit;
	} else if (hr != hrSuccess)
		goto exit;

	hr = MAPIPropHelper::Create(ptrMessage.as<MAPIPropPtr>(), &ptrPropHelper);
	if (hr != hrSuccess)
		goto exit;

	hr = ptrPropHelper->SetReference(refMsgEntry);
	if (hr != hrSuccess)
		goto exit;

	hr = ptrTransaction->Save(ptrMessage, false);
	if (hr != hrSuccess)
		goto exit;

	hr = UpdateHistoryRefs(ptrMessage, refMsgEntry, ptrTransaction);

exit:
	return hr;
}

}} // namespaces 
