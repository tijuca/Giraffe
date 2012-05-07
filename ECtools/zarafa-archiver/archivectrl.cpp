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
#include <mapiext.h>

#include "archivectrl.h"
#include "archiver-session.h"
#include "helpers/storehelper.h"
#include "operations/copier.h"
#include "operations/stubber.h"
#include "operations/deleter.h"
#include "archivestatecollector.h"
#include "archivestateupdater.h"

#include "ECLogger.h"
#include "ECConfig.h"
#include "restrictionutil.h"
#include "ECRestriction.h"
#include "stringutil.h"
#include "userutil.h"

#include <iostream>
#include <algorithm>
using namespace std;
using namespace za::helpers;
using namespace za::operations;

#include "mapi_ptr.h"
#include "ECRestriction.h"
#include "mapiguidext.h"

#include "ECIterators.h"
#include "HrException.h"

/**
 * Create a new Archive object.
 *
 * @param[in]	lpSession
 *					Pointer to the Session.
 * @param[in]	lpConfig
 *					Pointer to an ECConfig object that determines the operational options.
 * @param[in]	lpLogger
 *					Pointer to an ECLogger object that's used for logging.
 * @param[out]	lpptrArchiver
 *					Pointer to a ArchivePtr that will be assigned the address of the returned object.
 *
 * @return HRESULT
 */
HRESULT ArchiveControlImpl::Create(SessionPtr ptrSession, ECConfig *lpConfig, ECLogger *lpLogger, ArchiveControlPtr *lpptrArchiveControl)
{
	HRESULT hr = hrSuccess;
	std::auto_ptr<ArchiveControlImpl> ptrArchiveControl;

	try {
		ptrArchiveControl.reset(new ArchiveControlImpl(ptrSession, lpConfig, lpLogger));
	} catch (bad_alloc &) {
		hr = MAPI_E_NOT_ENOUGH_MEMORY;
		goto exit;
	}

	hr = ptrArchiveControl->Init();
	if (hr != hrSuccess)
		goto exit;

	*lpptrArchiveControl = ptrArchiveControl;	// transfers ownership

exit:
	return hr;
}

/**
 * Constructor
 *
 * @param[in]	lpSession
 *					Pointer to the Session.
 * @param[in]	lpConfig
 *					Pointer to an ECConfig object that determines the operational options.
 * @param[in]	lpLogger
 *					Pointer to an ECLogger object that's used for logging.
 *
 * @return HRESULT
 */
ArchiveControlImpl::ArchiveControlImpl(SessionPtr ptrSession, ECConfig *lpConfig, ECLogger *lpLogger)
: m_ptrSession(ptrSession)
, m_lpConfig(lpConfig)
, m_lpLogger(lpLogger)
, m_bArchiveEnable(true)
, m_ulArchiveAfter(30)
, m_bDeleteEnable(false)
, m_bDeleteUnread(false)
, m_ulDeleteAfter(0)
, m_bStubEnable(false)
, m_bStubUnread(false)
, m_ulStubAfter(0)
{
	if (m_lpLogger)
		m_lpLogger->AddRef();
	else
		m_lpLogger = new ECLogger_Null();
}

/**
 * Destructor
 */
ArchiveControlImpl::~ArchiveControlImpl()
{
	m_lpLogger->Release();
}

/**
 * Initialize the Archiver object.
 */
HRESULT ArchiveControlImpl::Init()
{
	HRESULT hr = hrSuccess;

	m_bArchiveEnable = parseBool(m_lpConfig->GetSetting("archive_enable", "", "no"));
	m_ulArchiveAfter = atoi(m_lpConfig->GetSetting("archive_after", "", "30"));

	m_bDeleteEnable = parseBool(m_lpConfig->GetSetting("delete_enable", "", "no"));
	m_bDeleteUnread = parseBool(m_lpConfig->GetSetting("delete_unread", "", "no"));
	m_ulDeleteAfter = atoi(m_lpConfig->GetSetting("delete_after", "", "0"));

	m_bStubEnable = parseBool(m_lpConfig->GetSetting("stub_enable", "", "no"));
	m_bStubUnread = parseBool(m_lpConfig->GetSetting("stub_unread", "", "no"));
	m_ulStubAfter = atoi(m_lpConfig->GetSetting("stub_after", "", "0"));

	m_bPurgeEnable = parseBool(m_lpConfig->GetSetting("purge_enable", "", "no"));
	m_ulPurgeAfter = atoi(m_lpConfig->GetSetting("purge_after", "", "0"));

	const char *lpszCleanupAction = m_lpConfig->GetSetting("cleanup_action");
	if (lpszCleanupAction == NULL || *lpszCleanupAction == '\0') {
		m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Empty cleanup_action specified in config.");
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	if (stricmp(lpszCleanupAction, "delete") == 0)
		m_cleanupAction = caDelete;
	else if (stricmp(lpszCleanupAction, "store") == 0)
		m_cleanupAction = caStore;
	else {
		m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unknown cleanup_action specified in config: '%s'", lpszCleanupAction);
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	GetSystemTimeAsFileTime(&m_ftCurrent);

exit:
	return hr;
}

/**
 * Archive messages for all users. Optionaly only user that have their store on the server
 * to which the archiver is connected will have their messages archived.
 *
 * @param[in]	bLocalOnly
 *					If set to true only  messsages for users that have their store on the local server
 *					will be archived.
 *
 * @return HRESULT
 */
eResult ArchiveControlImpl::ArchiveAll(bool bLocalOnly, bool bAutoAttach, unsigned int ulFlags)
{
	HRESULT hr = hrSuccess;

	if (ulFlags != ArchiveManage::Writable && ulFlags != ArchiveManage::ReadOnly && ulFlags != 0) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	if (bAutoAttach || parseBool(m_lpConfig->GetSetting("enable_auto_attach"))) {
		ArchiveStateCollectorPtr ptrArchiveStateCollector;
		ArchiveStateUpdaterPtr ptrArchiveStateUpdater;

		hr = ArchiveStateCollector::Create(m_ptrSession, m_lpLogger, &ptrArchiveStateCollector);
		if (hr != hrSuccess)
			goto exit;

		hr = ptrArchiveStateCollector->GetArchiveStateUpdater(&ptrArchiveStateUpdater);
		if (hr != hrSuccess)
			goto exit;

		if (ulFlags == 0) {
			if (parseBool(m_lpConfig->GetSetting("auto_attach_writable")))
				ulFlags = ArchiveManage::Writable;
			else
				ulFlags = ArchiveManage::ReadOnly;
		}

		hr = ptrArchiveStateUpdater->UpdateAll(ulFlags);
		if (hr != hrSuccess)
			goto exit;
	}

	hr = ProcessAll(bLocalOnly, &ArchiveControlImpl::DoArchive);

exit:
	return MAPIErrorToArchiveError(hr);
}

/**
 * Archive the messages of a particular user.
 *
 * @param[in]	lpszUser
 *					The username for which to archive the messages.
 *
 * @return HRESULT
 */
eResult ArchiveControlImpl::Archive(const TCHAR *lpszUser, bool bAutoAttach, unsigned int ulFlags)
{
	HRESULT hr = hrSuccess;

	if (ulFlags != ArchiveManage::Writable && ulFlags != ArchiveManage::ReadOnly && ulFlags != 0) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	if (bAutoAttach || parseBool(m_lpConfig->GetSetting("enable_auto_attach"))) {
		ArchiveStateCollectorPtr ptrArchiveStateCollector;
		ArchiveStateUpdaterPtr ptrArchiveStateUpdater;

		hr = ArchiveStateCollector::Create(m_ptrSession, m_lpLogger, &ptrArchiveStateCollector);
		if (hr != hrSuccess)
			goto exit;

		hr = ptrArchiveStateCollector->GetArchiveStateUpdater(&ptrArchiveStateUpdater);
		if (hr != hrSuccess)
			goto exit;

		if (ulFlags == 0) {
			if (parseBool(m_lpConfig->GetSetting("auto_attach_writable")))
				ulFlags = ArchiveManage::Writable;
			else
				ulFlags = ArchiveManage::ReadOnly;
		}

		hr = ptrArchiveStateUpdater->Update(lpszUser, ulFlags);
		if (hr != hrSuccess)
			goto exit;
	}

	hr = DoArchive(lpszUser);

exit:
	return MAPIErrorToArchiveError(hr);
}

/**
 * Cleanup the archive(s) of all users. Optionaly only user that have their store on the server
 * to which the archiver is connected will have their messages archived.
 *
 * @param[in]	bLocalOnly
 *					If set to true only  messsages for users that have their store on the local server
 *					will be archived.
 *
 * @return HRESULT
 */
eResult ArchiveControlImpl::CleanupAll(bool bLocalOnly)
{
	HRESULT hr = hrSuccess;

	hr = ProcessAll(bLocalOnly, &ArchiveControlImpl::DoCleanup);
	return MAPIErrorToArchiveError(hr);
}

/**
 * Cleanup the archive(s) of a particular user.
 * Cleaning up is currently defined as detecting which messages were deleted
 * from the primary store and moving the archives of those messages to the
 * special deleted folder.
 *
 * @param[in]	lpszUser
 *					The username for which to archive the messages.
 *
 * @return HRESULT
 */
eResult ArchiveControlImpl::Cleanup(const TCHAR *lpszUser)
{
	HRESULT hr = hrSuccess;

	hr = DoCleanup(lpszUser);
	return MAPIErrorToArchiveError(hr);
}


/**
 * Process all users.
 *
 * @param[in]	bLocalOnly	Limit to users that have a store on the local server.
 * @param[in]	fnProcess	The method to execute to do the actual processing.
 */ 
HRESULT ArchiveControlImpl::ProcessAll(bool bLocalOnly, fnProcess_t fnProcess)
{
	typedef std::list<tstring> StringList;
	
	HRESULT hr = hrSuccess;
	StringList lstUsers;
	UserList lstUserEntries;
	StringList::const_iterator iUser;
	bool bHaveErrors = false;

	hr = GetArchivedUserList(m_lpLogger, 
							 m_ptrSession->GetMAPISession(),
							 m_ptrSession->GetSSLPath(),
							 m_ptrSession->GetSSLPass(),
							 &lstUsers, bLocalOnly);
	if (hr != hrSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Failed to obtain user list. (hr=0x%08x)", hr);
		goto exit;
	}

	m_lpLogger->Log(EC_LOGLEVEL_INFO, "Processing "SIZE_T_PRINTF"%s users.", lstUsers.size(), (bLocalOnly ? " local" : ""));
	for (iUser = lstUsers.begin(); iUser != lstUsers.end(); ++iUser) {
		m_lpLogger->Log(EC_LOGLEVEL_INFO, "Processing user '" TSTRING_PRINTF "'.", iUser->c_str());
		HRESULT hrTmp = (this->*fnProcess)(iUser->c_str());
		if (FAILED(hrTmp)) {
			m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Failed to process user '" TSTRING_PRINTF "'. (hr=0x%08x)", iUser->c_str(), hrTmp);
			bHaveErrors = true;
		} else if (hrTmp == MAPI_W_PARTIAL_COMPLETION) {
			m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Errors occured while processing user '" TSTRING_PRINTF "'.", iUser->c_str());
			bHaveErrors = true;
		}
	}

exit:
	if (hr == hrSuccess && bHaveErrors)
		hr = MAPI_W_PARTIAL_COMPLETION;

	return hr;
}


/**
 * Perform the actual archive operation for a specific user.
 * 
 * @param[in]	lpszUser	The username of the user to process.
 */
HRESULT ArchiveControlImpl::DoArchive(const TCHAR *lpszUser)
{
	HRESULT hr = hrSuccess;
	MsgStorePtr ptrUserStore;
	StoreHelperPtr ptrStoreHelper;
	MAPIFolderPtr ptrSearchArchiveFolder;
	MAPIFolderPtr ptrSearchDeleteFolder;
	MAPIFolderPtr ptrSearchStubFolder;
	ObjectEntryList lstArchives;
	bool bHaveErrors = false;

	CopierPtr	ptrCopyOp;
	DeleterPtr	ptrDeleteOp;
	StubberPtr	ptrStubOp;

	if (lpszUser == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	m_lpLogger->Log(EC_LOGLEVEL_INFO, "Archiving store for user '" TSTRING_PRINTF "'", lpszUser);

	hr = m_ptrSession->OpenStoreByName(lpszUser, &ptrUserStore);
	if (hr != hrSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Failed to open store. (hr=%s)", stringify(hr, true).c_str());
		goto exit;
	}

	PROPMAP_INIT_NAMED_ID(ARCHIVE_STORE_ENTRYIDS, PT_MV_BINARY, PSETID_Archive, dispidStoreEntryIds)
	PROPMAP_INIT_NAMED_ID(ARCHIVE_ITEM_ENTRYIDS, PT_MV_BINARY, PSETID_Archive, dispidItemEntryIds)
	PROPMAP_INIT_NAMED_ID(ORIGINAL_SOURCEKEY, PT_BINARY, PSETID_Archive, dispidOrigSourceKey)
	PROPMAP_INIT_NAMED_ID(STUBBED, PT_BOOLEAN, PSETID_Archive, dispidStubbed)
	PROPMAP_INIT_NAMED_ID(DIRTY, PT_BOOLEAN, PSETID_Archive, dispidDirty)
	PROPMAP_INIT(ptrUserStore)

	hr = StoreHelper::Create(ptrUserStore, &ptrStoreHelper);
	if (hr != hrSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Failed to create store helper. (hr=%s)", stringify(hr, true).c_str());
		goto exit;
	}

	hr = ptrStoreHelper->GetArchiveList(&lstArchives);
	if (hr != hrSuccess) {
		if (hr == MAPI_E_CORRUPT_DATA) {
			m_lpLogger->Log(EC_LOGLEVEL_ERROR, "List of archives is corrupt for user '" TSTRING_PRINTF "', skipping user.", lpszUser);
			hr = hrSuccess;
		} else
			m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Failed to get list of archives. (hr=%s)", stringify(hr, true).c_str());
		goto exit;
	}

	if (lstArchives.empty()) {
		m_lpLogger->Log(EC_LOGLEVEL_INFO, "'" TSTRING_PRINTF "' has no attached archives", lpszUser);
		goto exit;
	}

	hr = ptrStoreHelper->GetSearchFolders(&ptrSearchArchiveFolder, &ptrSearchDeleteFolder, &ptrSearchStubFolder);
	if (hr != hrSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Failed to get the search folders. (hr=%s)", stringify(hr, true).c_str());
		goto exit;
	}

	// Create and hook the three dependant steps
	if (m_bArchiveEnable && m_ulArchiveAfter >= 0) {
		SizedSPropTagArray(5, sptaExcludeProps) = {5, {PROP_ARCHIVE_STORE_ENTRYIDS, PROP_ARCHIVE_ITEM_ENTRYIDS, PROP_STUBBED, PROP_DIRTY, PROP_ORIGINAL_SOURCEKEY}};
		ptrCopyOp.reset(new Copier(m_ptrSession, m_lpConfig, m_lpLogger, lstArchives, (LPSPropTagArray)&sptaExcludeProps, m_ulArchiveAfter, true));
	}

	if (m_bDeleteEnable && m_ulDeleteAfter >= 0) {
		ptrDeleteOp.reset(new Deleter(m_lpLogger, m_ulDeleteAfter, m_bDeleteUnread));
		if (ptrCopyOp)
			ptrCopyOp->SetDeleteOperation(ptrDeleteOp);
	}

	if (m_bStubEnable && m_ulStubAfter >= 0) {
		ptrStubOp.reset(new Stubber(m_lpLogger, PROP_STUBBED, m_ulStubAfter, m_bStubUnread));
		if (ptrCopyOp)
			ptrCopyOp->SetStubOperation(ptrStubOp);
	}

	// Now execute them
	if (ptrCopyOp) {
		// Archive all unarchived messages that are old enough
		m_lpLogger->Log(EC_LOGLEVEL_INFO, "Archiving messages");
		hr = ProcessFolder(ptrSearchArchiveFolder, ptrCopyOp);
		if (FAILED(hr)) {
			m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Failed to archive messages. (hr=%s)", stringify(hr, true).c_str());
			goto exit;
		} else if (hr == MAPI_W_PARTIAL_COMPLETION) {
			m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Some message could not be archived");
			bHaveErrors = true;
			hr = hrSuccess;
		}
		m_lpLogger->Log(EC_LOGLEVEL_INFO, "Done archiving messages");
	}


	if (ptrDeleteOp) {
		// First delete all messages that are elegible for deletion, so we don't unneccesary stub them first
		m_lpLogger->Log(EC_LOGLEVEL_INFO, "Deleting old messages");
		hr = ProcessFolder(ptrSearchDeleteFolder, ptrDeleteOp);
		if (FAILED(hr)) {
			m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Failed to delete old messages. (hr=%s)", stringify(hr, true).c_str());
			goto exit;
		} else if (hr == MAPI_W_PARTIAL_COMPLETION) {
			m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Some message could not be deleted");
			bHaveErrors = true;
			hr = hrSuccess;
		}
		m_lpLogger->Log(EC_LOGLEVEL_INFO, "Done deleting messages");
	}


	if (ptrStubOp) {
		// Now stub the remaing messages (if they're old enough)
		m_lpLogger->Log(EC_LOGLEVEL_INFO, "Stubbing messages");
		hr = ProcessFolder(ptrSearchStubFolder, ptrStubOp);
		if (FAILED(hr)) {
			m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Failed to stub messages. (hr=%s)", stringify(hr, true).c_str());
			goto exit;
		} else if (hr == MAPI_W_PARTIAL_COMPLETION) {
			m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Some message could not be stubbed");
			bHaveErrors = true;
			hr = hrSuccess;
		}
		m_lpLogger->Log(EC_LOGLEVEL_INFO, "Done stubbing messages");
	}

	if (m_bPurgeEnable) {
		m_lpLogger->Log(EC_LOGLEVEL_INFO, "Purging archive(s)");
		hr = PurgeArchives(lstArchives);
		if (FAILED(hr)) {
			m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Failed to purge archive(s). (hr=%s)", stringify(hr, true).c_str());
			goto exit;
		} else if (hr == MAPI_W_PARTIAL_COMPLETION) {
			m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Some archives could not be purged");
			bHaveErrors = true;
			hr = hrSuccess;
		}
		m_lpLogger->Log(EC_LOGLEVEL_INFO, "Done purging archive(s)");
	}

exit:
	if (hr == hrSuccess && bHaveErrors)
		hr = MAPI_W_PARTIAL_COMPLETION;

	return hr;
}

/**
 * Perform the actual cleanup operation for a specific user.
 * 
 * @param[in]	lpszUser	The username of the user to process.
 */
HRESULT ArchiveControlImpl::DoCleanup(const TCHAR *lpszUser)
{
	HRESULT hr = hrSuccess;
	MsgStorePtr ptrUserStore;
	StoreHelperPtr ptrStoreHelper;
	ObjectEntryList lstArchives;

	if (lpszUser == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	m_lpLogger->Log(EC_LOGLEVEL_INFO, "Cleanup store for user '" TSTRING_PRINTF "'", lpszUser);

	hr = m_ptrSession->OpenStoreByName(lpszUser, &ptrUserStore);
	if (hr != hrSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Failed to open store. (hr=0x%08x)", hr);
		goto exit;
	}

	hr = StoreHelper::Create(ptrUserStore, &ptrStoreHelper);
	if (hr != hrSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Failed to create store helper. (hr=0x%08x)", hr);
		goto exit;
	}

	hr = ptrStoreHelper->GetArchiveList(&lstArchives);
	if (hr != hrSuccess) {
		if (hr == MAPI_E_CORRUPT_DATA) {
			m_lpLogger->Log(EC_LOGLEVEL_ERROR, "List of archives is corrupt for user '" TSTRING_PRINTF "', skipping user.", lpszUser);
			hr = hrSuccess;
		} else
			m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Failed to get list of archives. (hr=0x%08x)", hr);
		goto exit;
	}

	if (lstArchives.empty()) {
		m_lpLogger->Log(EC_LOGLEVEL_INFO, "'" TSTRING_PRINTF "' has no attached archives", lpszUser);
		goto exit;
	}

	for (ObjectEntryList::iterator iArchive = lstArchives.begin(); iArchive != lstArchives.end(); ++iArchive) {
		HRESULT hrTmp = hrSuccess;

		hrTmp = CleanupArchive(*iArchive, ptrUserStore);
		if (hrTmp != hrSuccess)
			m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Failed to cleanup archive. (hr=0x%08x)", hr);
	}

exit:
	return hr;
}

/**
 * Process a search folder and place an additional restriction on it to get the messages
 * that should really be archived.
 *
 * @param[in]	ptrFolder
 *					A MAPIFolderPtr that points to the search folder to be processed.
 * @param[in]	lpArchiveOperation
 *					The pointer to a IArchiveOperation derived object that's used to perform
 *					the actual processing.
 * @param[in]	ulAge
 *					The age in days since the message was delivered, that a message must be before
 *					it will be processed.
 * @param[in]	bProcessUnread
 *					If set to true, unread messages will also be processed. Otherwise unread message
 *					will be left untouched.
 *
 * @return HRESULT
 */
HRESULT ArchiveControlImpl::ProcessFolder(MAPIFolderPtr &ptrFolder, ArchiveOperationPtr ptrArchiveOperation)
{
	HRESULT hr = hrSuccess;
	MAPITablePtr ptrTable;
	SRestrictionPtr ptrRestriction;
	SSortOrderSetPtr ptrSortOrder;
	mapi_rowset_ptr ptrRowSet;
	MessagePtr ptrMessage;
	bool bHaveErrors = false;

	SizedSPropTagArray(3, sptaProps) = {3, {PR_ENTRYID, PR_PARENT_ENTRYID, PR_STORE_ENTRYID}};

	hr = ptrFolder->GetContentsTable(fMapiDeferredErrors, &ptrTable);
	if (hr != hrSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Failed to get search folder contents table. (hr=%s)", stringify(hr, true).c_str());
		goto exit;
	}

	hr = ptrTable->SetColumns((LPSPropTagArray)&sptaProps, TBL_BATCH);
	if (hr != hrSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Failed to set columns on table. (hr=%s)", stringify(hr, true).c_str());
		goto exit;
	}

	hr = ptrArchiveOperation->GetRestriction(PR_NULL, &ptrRestriction);
	if (hr != hrSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Failed to get restriction from operation. (hr=%s)", stringify(hr, true).c_str());
		goto exit;
	}

	hr = ptrTable->Restrict(ptrRestriction, TBL_BATCH);
	if (hr != hrSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Failed to set restriction on table. (hr=%s)", stringify(hr, true).c_str());
		goto exit;
	}

	hr = MAPIAllocateBuffer(CbNewSSortOrderSet(1), &ptrSortOrder);
	if (hr != hrSuccess)
		goto exit;

	ptrSortOrder->cSorts = 1;
	ptrSortOrder->cCategories = 0;
	ptrSortOrder->cExpanded = 0;
	ptrSortOrder->aSort[0].ulPropTag = PR_PARENT_ENTRYID;
	ptrSortOrder->aSort[0].ulOrder = TABLE_SORT_ASCEND ;

	hr = ptrTable->SortTable(ptrSortOrder, TBL_BATCH);
	if (hr != hrSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Failed to sort table. (hr=%s)", stringify(hr, true).c_str());
		goto exit;
	}

	do {
		hr = ptrTable->QueryRows(50, 0, &ptrRowSet);
		if (hr != hrSuccess) {
			m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Failed to get rows from table. (hr=%s)", stringify(hr, true).c_str());
			goto exit;
		}

		m_lpLogger->Log(EC_LOGLEVEL_INFO, "Processing batch of %u messages", ptrRowSet.size());
		for (ULONG i = 0; i < ptrRowSet.size(); ++i) {
			hr = ptrArchiveOperation->ProcessEntry(ptrFolder, ptrRowSet[i].cValues, ptrRowSet[i].lpProps);
			if (hr != hrSuccess) {
				bHaveErrors = true;
				m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Failed to process entry. (hr=%s)", stringify(hr, true).c_str());
				if (hr == MAPI_E_STORE_FULL) {
					m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Disk full or over quota.");
					goto exit;
				}
				continue;
			}
		}
		m_lpLogger->Log(EC_LOGLEVEL_INFO, "Done processing batch");
	} while (ptrRowSet.size() == 50);

exit:
	if (hr == hrSuccess && bHaveErrors)
		hr = MAPI_W_PARTIAL_COMPLETION;

	return hr;
}

/**
 * Purge a set of archives. Purging an archive is defined as deleting all
 * messages that are older than a set amount of days.
 *
 * @param[in]	lstArchives		The list of archives to purge.
 */
HRESULT ArchiveControlImpl::PurgeArchives(const ObjectEntryList &lstArchives)
{
	HRESULT hr = hrSuccess;
	ObjectEntryList::const_iterator iArchive;
	bool bErrorOccurred = false;
	LPSRestriction lpRestriction = NULL;
	SPropValue sPropCreationTime;
	ULARGE_INTEGER li;
	mapi_rowset_ptr ptrRowSet;

	SizedSPropTagArray(1, sptaFolderProps) = {1, {PR_ENTRYID}};

	// Create the common restriction that determines which messages are old enough to purge.
	CREATE_RESTRICTION(lpRestriction);
	DATA_RES_PROPERTY_CHEAP(lpRestriction, *lpRestriction, RELOP_LT, PR_MESSAGE_DELIVERY_TIME, &sPropCreationTime);

	li.LowPart = m_ftCurrent.dwLowDateTime;
	li.HighPart = m_ftCurrent.dwHighDateTime;

	li.QuadPart -= (m_ulPurgeAfter * _DAY);

	sPropCreationTime.ulPropTag = PR_MESSAGE_DELIVERY_TIME;
	sPropCreationTime.Value.ft.dwLowDateTime = li.LowPart;
	sPropCreationTime.Value.ft.dwHighDateTime = li.HighPart;

	for (iArchive = lstArchives.begin(); iArchive != lstArchives.end(); ++iArchive) {
		MsgStorePtr ptrArchiveStore;
		MAPIFolderPtr ptrArchiveRoot;
		ULONG ulType = 0;
		MAPITablePtr ptrFolderTable;
		mapi_rowset_ptr ptrFolderRows;

		hr = m_ptrSession->OpenStore(iArchive->sStoreEntryId, &ptrArchiveStore);
		if (hr != hrSuccess) {
			m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Failed to open archive. (entryid=%s, hr=%s)", iArchive->sStoreEntryId.tostring().c_str(), stringify(hr, true).c_str());
			bErrorOccurred = true;
			continue;
		}

		// Purge root of archive
		hr = PurgeArchiveFolder(ptrArchiveStore, iArchive->sItemEntryId, lpRestriction);
		if (hr != hrSuccess) {
			m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Failed to purge archive root. (entryid=%s, hr=%s)", iArchive->sItemEntryId.tostring().c_str(), stringify(hr, true).c_str());
			bErrorOccurred = true;
			continue;
		}

		// Get all subfolders and purge those as well.
		hr = ptrArchiveStore->OpenEntry(iArchive->sItemEntryId.size(), iArchive->sItemEntryId, &ptrArchiveRoot.iid, MAPI_BEST_ACCESS|fMapiDeferredErrors, &ulType, &ptrArchiveRoot);
		if (hr != hrSuccess) {
			m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Failed to open archive root. (entryid=%s, hr=%s)", iArchive->sItemEntryId.tostring().c_str(), stringify(hr, true).c_str());
			bErrorOccurred = true;
			continue;
		}

		hr = ptrArchiveRoot->GetHierarchyTable(CONVENIENT_DEPTH|fMapiDeferredErrors, &ptrFolderTable);
		if (hr != hrSuccess) {
			m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Failed to get archive hierarchy table. (hr=%s)", stringify(hr, true).c_str());
			bErrorOccurred = true;
			continue;
		}

		hr = ptrFolderTable->SetColumns((LPSPropTagArray)&sptaFolderProps, TBL_BATCH);
		if (hr != hrSuccess) {
			m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Failed to select folder table columns. (hr=%s)", stringify(hr, true).c_str());
			bErrorOccurred = true;
			continue;
		}

		while (true) {
			hr = ptrFolderTable->QueryRows(50, 0, &ptrFolderRows);
			if (hr != hrSuccess) {
				m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Failed to get rows from folder table. (hr=%s)", stringify(hr, true).c_str());
				goto exit;
			}

			for (ULONG i = 0; i < ptrFolderRows.size(); ++i) {
				hr = PurgeArchiveFolder(ptrArchiveStore, ptrFolderRows[i].lpProps[0].Value.bin, lpRestriction);
				if (hr != hrSuccess) {
					m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Failed to purge archive folder. (entryid=%s, hr=%s)", bin2hex(ptrFolderRows[i].lpProps[0].Value.bin.cb, ptrFolderRows[i].lpProps[0].Value.bin.lpb).c_str(), stringify(hr, true).c_str());
					bErrorOccurred = true;
				}
			}

			if (ptrFolderRows.size() < 50)
				break;
		}
	}

exit:
	if (lpRestriction)
		MAPIFreeBuffer(lpRestriction);

	if (hr == hrSuccess && bErrorOccurred)
		hr = MAPI_W_PARTIAL_COMPLETION;

	return hr;
}

/**
 * Purge an archive folder.
 *
 * @param[in]	ptrArchive		The archive store containing the folder to purge.
 * @param[in]	folderEntryID	The entryid of the folder to purge.
 * @param[in]	lpRestriction	The restriction to use to determine which messages to delete.
 */
HRESULT ArchiveControlImpl::PurgeArchiveFolder(MsgStorePtr &ptrArchive, const entryid_t &folderEntryID, const LPSRestriction lpRestriction)
{
	HRESULT hr = hrSuccess;
	ULONG ulType = 0;
	MAPIFolderPtr ptrFolder;
	MAPITablePtr ptrContentsTable;
	list<entryid_t>::const_iterator iEntryId;
	list<entryid_t> lstEntries;
	mapi_rowset_ptr ptrRows;
	EntryListPtr ptrEntryList;
	ULONG ulIdx = 0;

	SizedSPropTagArray(1, sptaTableProps) = {1, {PR_ENTRYID}};

	hr = ptrArchive->OpenEntry(folderEntryID.size(), folderEntryID, &ptrFolder.iid, MAPI_BEST_ACCESS|fMapiDeferredErrors, &ulType, &ptrFolder);
	if (hr != hrSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Failed to open archive folder. (entryid=%s, hr=%s)", folderEntryID.tostring().c_str(), stringify(hr, true).c_str());
		goto exit;
	}

	hr = ptrFolder->GetContentsTable(fMapiDeferredErrors, &ptrContentsTable);
	if (hr != hrSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Failed to open contents table. (hr=%s)", stringify(hr, true).c_str());
		goto exit;
	}

	hr = ptrContentsTable->SetColumns((LPSPropTagArray)&sptaTableProps, TBL_BATCH);
	if (hr != hrSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Failed to select table columns. (hr=%s)", stringify(hr, true).c_str());
		goto exit;
	}

	hr = ptrContentsTable->Restrict(lpRestriction, TBL_BATCH);
	if (hr != hrSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Failed to restrict contents table. (hr=%s)", stringify(hr, true).c_str());
		goto exit;
	}


	while (true) {
		hr = ptrContentsTable->QueryRows(50, 0, &ptrRows);
		if (hr != hrSuccess) {
			m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Failed to get rows from contents table. (hr=%s)", stringify(hr, true).c_str());
			goto exit;
		}

		for (ULONG i = 0; i < ptrRows.size(); ++i)
			lstEntries.push_back(ptrRows[i].lpProps[0].Value.bin);

		if (ptrRows.size() < 50)
			break;
	}


	m_lpLogger->Log(EC_LOGLEVEL_INFO, "Purging %lu messaged from archive folder", lstEntries.size());

	hr = MAPIAllocateBuffer(sizeof(ENTRYLIST), &ptrEntryList);
	if (hr != hrSuccess)
		goto exit;

	hr = MAPIAllocateMore(lstEntries.size() * sizeof(SBinary), ptrEntryList, (LPVOID*)&ptrEntryList->lpbin);
	if (hr != hrSuccess)
		goto exit;

	ptrEntryList->cValues = lstEntries.size();
	for (iEntryId = lstEntries.begin(); iEntryId != lstEntries.end(); ++iEntryId, ++ulIdx) {
		ptrEntryList->lpbin[ulIdx].cb = iEntryId->size();
		ptrEntryList->lpbin[ulIdx].lpb = *iEntryId;
	}

	hr = ptrFolder->DeleteMessages(ptrEntryList, 0, NULL, 0);
	if (hr != hrSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Failed to delete %u messages. (hr=%s)", ptrEntryList->cValues, stringify(hr, true).c_str());
		goto exit;
	}

exit:
	return hr;
}

/**
 * Cleanup an archive.
 *
 * @param[in]	archiveEntry	SObjectEntry specifyinf the archive to cleanup
 * @param[in]	lpUserStore		The primary store, used to check the references
 */
HRESULT ArchiveControlImpl::CleanupArchive(const SObjectEntry &archiveEntry, LPMDB lpUserStore)
{
	HRESULT hr = hrSuccess;
	ArchiveHelperPtr ptrArchiveHelper;
	MAPIFolderPtr ptrArchiveFolder;
	ECFolderIterator iEnd;

	hr = ArchiveHelper::Create(m_ptrSession, archiveEntry, m_lpLogger, &ptrArchiveHelper);
	if (hr != hrSuccess)
		goto exit;

	hr = ptrArchiveHelper->GetArchiveFolder(true, &ptrArchiveFolder);
	if (hr != hrSuccess)
		goto exit;

	// First process root of archive
	m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Cleaning root of archive");
	hr = CleanupArchiveFolder(ptrArchiveHelper, ptrArchiveFolder, lpUserStore);
	if (hr != hrSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Failed to cleanup archive root. (hr=0x%08x)", hr);
		hr = hrSuccess;
	}

	try {
		for (ECFolderIterator i = ECFolderIterator(ptrArchiveFolder, MAPI_MODIFY|fMapiDeferredErrors, 0); i != iEnd; ++i) {
			m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Cleaning subfolder of archive");
			hr = CleanupArchiveFolder(ptrArchiveHelper, *i, lpUserStore);
			if (hr != hrSuccess) {
				m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Failed to cleanup archive folder. (hr=0x%08x)", hr);
				hr = hrSuccess;
			}
		}
	} catch (const HrException &he) {
		hr = he.hr();
		m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Failed to iterate archive folders. (hr=0x%08x)", hr);
		goto exit;
	}

exit:
	return hr;
}

/**
 * Cleanup an archive folder. Currently this means that all messages will be tested to
 * see if the message in the primary store still exists.
 * The approach to detect this is:
 * 1. Create a list of all messages in the primary folder that is referenced by the
 *    current folder.
 * 2. Open the contentstable of the current folder and see if the referenced entryid
 *    is available in the list. If it is, the reference is valid.
 * 3. If the reference entryid is not found in the list, try to open the the entry on
 *    the primary store, just to be sure the message is realy deleted. If this fails
 *    the message is considered deleted and the archive message will be moved to the
 *    special 'Deleted Items' folder.
 *
 * @param[in]	lpArchiveHelper		An ArchiveHelper object containing the archive store that's being processed.
 * @param[in]	lpArchiveFolder		The archive folder to cleanup.
 * @param[in]	lpUserStore			The primary store, used to check the references.
 */
HRESULT ArchiveControlImpl::CleanupArchiveFolder(ArchiveHelperPtr ptrArchiveHelper, LPMAPIFOLDER lpArchiveFolder, LPMDB lpUserStore)
{
	HRESULT hr = hrSuccess;
	MAPIPropHelperPtr ptrFolderHelper;
	SObjectEntry primaryEntry;
	MAPIFolderPtr ptrPrimaryFolder;
	ULONG ulType;
	ReferenceSet setReferences;
	ReferenceSet::const_iterator iReference;
	EntryIDSet setPrimaryEIDs;
	EntryIDSet::const_iterator iEntryID;
	MAPIFolderPtr ptrDeletedFolder;
	EntryIDSet setDeleteEIDs;

	if (lpArchiveFolder == NULL || lpUserStore == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	if (m_lpLogger->Log(EC_LOGLEVEL_INFO)) {
		SPropValuePtr ptrDisplayName;

		hr = HrGetOneProp(lpArchiveFolder, PR_DISPLAY_NAME_A, &ptrDisplayName);
		m_lpLogger->Log(EC_LOGLEVEL_INFO, "Cleaning up folder '%s'", hr == hrSuccess ? ptrDisplayName->Value.lpszA : "<Unknown>");
	}

	hr = MAPIPropHelper::Create(MAPIPropPtr(lpArchiveFolder, true), &ptrFolderHelper);
	if (hr != hrSuccess)
		goto exit;

	hr = ptrFolderHelper->GetReference(&primaryEntry);
	if (hr == MAPI_E_NOT_FOUND) {
		m_lpLogger->Log(EC_LOGLEVEL_INFO, "Folder does not reference primary folder.");
		hr = hrSuccess;
		goto exit;
	} else if (hr != hrSuccess)
		goto exit;

	hr = GetReferenceSet(lpArchiveFolder, &setReferences);
	if (hr != hrSuccess)
		goto exit;

	if (setReferences.empty()) {
		m_lpLogger->Log(EC_LOGLEVEL_INFO, "No references found in folder.");
		goto exit;
	}

	hr = lpUserStore->OpenEntry(primaryEntry.sItemEntryId.size(), primaryEntry.sItemEntryId, &ptrPrimaryFolder.iid, 0, &ulType, &ptrPrimaryFolder);
	if (hr == MAPI_E_NOT_FOUND) {
		m_lpLogger->Log(EC_LOGLEVEL_INFO, "Primary message seems to have been deleted");
		if (m_cleanupAction == caStore)
			hr = MoveAndDetachFolder(ptrArchiveHelper, lpArchiveFolder);
		else
			hr = DeleteFolder(lpArchiveFolder);
		goto exit;
	} else if (hr != hrSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Failed to open primary folder. (hr=0x%08x)", hr);
		goto exit;
	}

	hr = GetEntryIDSet(ptrPrimaryFolder, &setPrimaryEIDs);
	if (hr != hrSuccess)
		goto exit;

	iReference = setReferences.begin();
	iEntryID = setPrimaryEIDs.begin();

	while (iReference != setReferences.end() && iEntryID != setPrimaryEIDs.end()) {
		if (iReference->second == *iEntryID) {
			m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Found message in archive and primary list");
			++iReference;
			++iEntryID;
		}

		else if (iReference->second > *iEntryID) {
			// Skip a message in the primary folder that has no reference from the archive (unarchived)
			m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Skip unreferences message in primary list");
			++iEntryID;
		}

		else {
			// We found a message in the archive folder referencing a message that was not found
			// in the primary folder. Try to open it to determine if we need to move this message
			// to the deleted items folder.
			MessagePtr ptrMessage;
			
			m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Processing dead reference from archive list");
			HRESULT hrTmp = lpUserStore->OpenEntry(iReference->second.size(), iReference->second, &ptrMessage.iid, 0, &ulType, &ptrMessage);
			if (hrTmp == MAPI_E_NOT_FOUND) {
				// This message should be moved
				m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Adding message to list of messages to 'delete'.");
				setDeleteEIDs.insert(iReference->first);
			} else if (hrTmp != hrSuccess) {
				m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Unexpected error while trying to open message. (hr=0x%08x)", hrTmp);
			}

			++iReference;
		}
	}

	// if setPrimaryEIDs is not fully processed, it means those messages are just not archived.
	// if setReferences is not fully processed, it means they didn't reference an item in setEIDs.
	for (; iReference != setReferences.end(); ++iReference) {
		// @todo: Get rid of duplicate code.
		
		// We found a message in the archive folder referencing a message that was not found
		// in the primary folder. Try to open it to determine if we need to move this message
		// to the deleted items folder.
		MessagePtr ptrMessage;
		
		HRESULT hrTmp = lpUserStore->OpenEntry(iReference->second.size(), iReference->second, &ptrMessage.iid, 0, &ulType, &ptrMessage);
		if (hrTmp == MAPI_E_NOT_FOUND) {
			// This message should be moved
			m_lpLogger->Log(EC_LOGLEVEL_NOTICE, "Adding message to list of messages to 'delete'.");
			setDeleteEIDs.insert(iReference->first);
		} else if (hrTmp != hrSuccess) {
			m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Unexpected error while trying to open message. (hr=0x%08x)", hrTmp);
		}
	}

	if (!setDeleteEIDs.empty()) {
		if (m_cleanupAction == caStore)
			hr = MoveAndDetachMessages(ptrArchiveHelper, lpArchiveFolder, setDeleteEIDs);
		else
			hr = DeleteMessages(lpArchiveFolder, setDeleteEIDs);
	}

exit:
	return hr;
}

/**
 * Move a set of messages to the special 'Deleted Items' folder and remove their reference to a
 * primary message that was deleted.
 *
 * @param[in]	ptrArchiveHelper	An ArchiveHelper object containing the archive store that's being processed.
 * @param[in]	lpArchiveFolder		The archive folder containing the messages to move.
 * @param[in]	setEIDs				The set with entryids of the messages to process.
 */
HRESULT ArchiveControlImpl::MoveAndDetachMessages(ArchiveHelperPtr ptrArchiveHelper, LPMAPIFOLDER lpArchiveFolder, const EntryIDSet &setEIDs)
{
	HRESULT hr = hrSuccess;
	MAPIFolderPtr ptrDelItemsFolder;
	EntryListPtr ptrMessageList;

	m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Moving messages to the special 'Deleted Items' folder...");

	hr = ptrArchiveHelper->GetDeletedItemsFolder(&ptrDelItemsFolder);
	if (hr != hrSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Failed to get deleted items folder. (hr=0x%08x)", hr);
		goto exit;
	}

	hr = MAPIAllocateBuffer(sizeof(ENTRYLIST), &ptrMessageList);
	if (hr != hrSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Failed to allocate "SIZE_T_PRINTF" bytes of memory. (hr=0x%08x)", sizeof(ENTRYLIST), hr);
		goto exit;
	}

	ptrMessageList->cValues = 0;

	hr = MAPIAllocateMore(sizeof(SBinary) * setEIDs.size(), ptrMessageList, (LPVOID*)&ptrMessageList->lpbin);
	if (hr != hrSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Failed to allocate "SIZE_T_PRINTF" bytes of memory. (hr=0x%08x)", sizeof(SBinary) * setEIDs.size(), hr);
		goto exit;
	}

	m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Processing "SIZE_T_PRINTF" messages", setEIDs.size());
	for (EntryIDSet::const_iterator i = setEIDs.begin(); i != setEIDs.end(); ++i) {
		ULONG ulType;
		MAPIPropPtr ptrMessage;
		MAPIPropHelperPtr ptrHelper;

		hr = lpArchiveFolder->OpenEntry(i->size(), *i, &ptrMessage.iid, MAPI_MODIFY, &ulType, &ptrMessage);
		if (hr != hrSuccess) {
			m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Failed to open message. (hr=0x%08x)", hr);
			goto exit;
		}

		hr = MAPIPropHelper::Create(ptrMessage, &ptrHelper);
		if (hr != hrSuccess) {
			m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Failed to create helper object. (hr=0x%08x)", hr);
			goto exit;
		}

		hr = ptrHelper->ClearReference(true);
		if (hr != hrSuccess) {
			m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Failed to clear back reference. (hr=0x%08x)", hr);
			goto exit;
		}
		
		ptrMessageList->lpbin[ptrMessageList->cValues].cb = i->size();
		ptrMessageList->lpbin[ptrMessageList->cValues++].lpb = *i;
		
		ASSERT(ptrMessageList->cValues <= setEIDs.size());
	}

	hr = lpArchiveFolder->CopyMessages(ptrMessageList, &ptrDelItemsFolder.iid, ptrDelItemsFolder, 0, NULL, MESSAGE_MOVE);
	if (hr != hrSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Failed to move messages. (hr=0x%08x)", hr);
		goto exit;
	}

exit:
	return hr;
}

/**
 * Move a folder to the special 'Deleted Items' folder and remove its reference to the
 * primary folder that was deleted.
 *
 * @param[in]	ptrArchiveHelper	An ArchiveHelper object containing the archive store that's being processed.
 * @param[in]	lpArchiveFolder		The archive folder to move.
 */
HRESULT ArchiveControlImpl::MoveAndDetachFolder(ArchiveHelperPtr ptrArchiveHelper, LPMAPIFOLDER lpArchiveFolder)
{
	HRESULT hr = hrSuccess;
	SPropValuePtr ptrEntryID;
	MAPIFolderPtr ptrDelItemsFolder;
	MAPIPropHelperPtr ptrHelper;

	m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Moving folder to the special 'Deleted Items' folder...");

	hr = HrGetOneProp(lpArchiveFolder, PR_ENTRYID, &ptrEntryID);
	if (hr != hrSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Failed to get folder entryid. (hr=0x%08x)", hr);
		goto exit;
	}

	hr = ptrArchiveHelper->GetDeletedItemsFolder(&ptrDelItemsFolder);
	if (hr != hrSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Failed to get deleted items folder. (hr=0x%08x)", hr);
		goto exit;
	}

	hr = MAPIPropHelper::Create(MAPIPropPtr(lpArchiveFolder, true), &ptrHelper);
	if (hr != hrSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Failed to create helper object. (hr=0x%08x)", hr);
		goto exit;
	}

	hr = ptrHelper->ClearReference(true);
	if (hr != hrSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Failed to clear back reference. (hr=0x%08x)", hr);
		goto exit;
	}

	hr = lpArchiveFolder->CopyFolder(ptrEntryID->Value.bin.cb, (LPENTRYID)ptrEntryID->Value.bin.lpb, &ptrDelItemsFolder.iid, ptrDelItemsFolder, NULL, 0, NULL, FOLDER_MOVE);
	if (hr != hrSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Failed to move messages. (hr=0x%08x)", hr);
		goto exit;
	}

exit:
	return hr;
}

/**
 * Delete the messages in setEIDs from the folder lpArchiveFolder.
 *
 * @param[in]	lpArchiveFolder		The folder to delete the messages from.
 * @param[in]	setEIDs				The set of entryids of the messages to delete.
 */
HRESULT ArchiveControlImpl::DeleteMessages(LPMAPIFOLDER lpArchiveFolder, const EntryIDSet &setEIDs)
{
	HRESULT hr = hrSuccess;
	EntryListPtr ptrMessageList;

	hr = MAPIAllocateBuffer(sizeof(ENTRYLIST), &ptrMessageList);
	if (hr != hrSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Failed to allocate "SIZE_T_PRINTF" bytes of memory. (hr=0x%08x)", sizeof(ENTRYLIST), hr);
		goto exit;
	}

	ptrMessageList->cValues = 0;

	hr = MAPIAllocateMore(sizeof(SBinary) * setEIDs.size(), ptrMessageList, (LPVOID*)&ptrMessageList->lpbin);
	if (hr != hrSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Failed to allocate "SIZE_T_PRINTF" bytes of memory. (hr=0x%08x)", sizeof(SBinary) * setEIDs.size(), hr);
		goto exit;
	}

	m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Processing "SIZE_T_PRINTF" messages", setEIDs.size());
	for (EntryIDSet::const_iterator i = setEIDs.begin(); i != setEIDs.end(); ++i) {
		ptrMessageList->lpbin[ptrMessageList->cValues].cb = i->size();
		ptrMessageList->lpbin[ptrMessageList->cValues++].lpb = *i;
	}

	hr = lpArchiveFolder->DeleteMessages(ptrMessageList, 0, NULL, 0);

exit:
	return hr;
}

/**
 * Delete the folder specified by lpArchiveFolder
 *
 * @param[in]	lpArchiveFolder		Folder to delete.
 */
HRESULT ArchiveControlImpl::DeleteFolder(LPMAPIFOLDER lpArchiveFolder)
{
	HRESULT hr = hrSuccess;
	SPropValuePtr ptrEntryId;

	hr = HrGetOneProp(lpArchiveFolder, PR_ENTRYID, &ptrEntryId);
	if (hr != hrSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Failed to get folder entryid (hr=0x%08x)", hr);
		goto exit;
	}

	// Delete yourself!
	hr = lpArchiveFolder->DeleteFolder(ptrEntryId->Value.bin.cb, (LPENTRYID)ptrEntryId->Value.bin.lpb, 0, NULL, DEL_FOLDERS|DEL_MESSAGES|DEL_ASSOCIATED);
	if (FAILED(hr)) {
		m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Failed to delete folder (hr=0x%08x)", hr);
		goto exit;
	} else if (hr != hrSuccess)
		m_lpLogger->Log(EC_LOGLEVEL_WARNING, "Folder only got partially deleted (hr=0x%08x)", hr);

exit:
	return hr;
}

/**
 * Get a set of entryids, containing the entryids of all the message in a folder.
 *
 * @param[in]		lpFolder	The folder to get the entryids from.
 * @param[in,out]	lpSetEIDs	The set of entryids.
 */
HRESULT ArchiveControlImpl::GetEntryIDSet(LPMAPIFOLDER lpFolder, EntryIDSet *lpSetEIDs)
{
	HRESULT hr = hrSuccess;
	MAPITablePtr ptrContentsTable;
	mapi_rowset_ptr ptrRows;
	EntryIDSet setEIDs;

	SizedSPropTagArray(1, sptaColumnProps) = {1, {PR_ENTRYID}};

	if (lpFolder == NULL || lpSetEIDs == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Getting list of messages...");

	hr = lpFolder->GetContentsTable(0, &ptrContentsTable);
	if (hr != hrSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Failed to get contents table. (hr=0x%08x)", hr);
		goto exit;
	}

	hr = ptrContentsTable->SetColumns((LPSPropTagArray)&sptaColumnProps, TBL_BATCH);
	if (hr != hrSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Failed to select columns on contents table. (hr=0x%08x)", hr);
		goto exit;
	}

	while (true) {
		hr = ptrContentsTable->QueryRows(64, 0, &ptrRows);
		if (hr != hrSuccess) {
			m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Failed to query contents table. (hr=0x%08x)", hr);
			goto exit;
		}

		if (ptrRows.empty())
			break;

		m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Got %u rows from contents table", ptrRows.size());
		for (mapi_rowset_ptr::size_type i = 0; i < ptrRows.size(); ++i)
			setEIDs.insert(ptrRows[i].lpProps[0].Value.bin);
	}

	m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Got "SIZE_T_PRINTF" rows from contents table", setEIDs.size());
	lpSetEIDs->swap(setEIDs);

exit:
	return hr;
}

/**
 * Get a set of entryids and references, containing the entryids of all the message in a folder
 * that have a referenec to a primary message.
 *
 * @param[in]		lpFolder	The folder to get the data from.
 * @param[in,out]	lpSetRefs	The set of entryids and references.
 */
HRESULT ArchiveControlImpl::GetReferenceSet(LPMAPIFOLDER lpFolder, ReferenceSet *lpSetRefs)
{
	HRESULT hr = hrSuccess;
	MAPITablePtr ptrContentsTable;
	mapi_rowset_ptr ptrRows;
	ReferenceSet setRefs;

	SizedSPropTagArray(2, sptaColumnProps) = {2, {PR_ENTRYID, PR_NULL}};
	enum {IDX_ENTRYID, IDX_REF_ITEM_ENTRYID};

	PROPMAP_START
		PROPMAP_NAMED_ID(REF_ITEM_ENTRYID, PT_BINARY, PSETID_Archive, dispidRefItemEntryId)
	PROPMAP_INIT(lpFolder)

	sptaColumnProps.aulPropTag[IDX_REF_ITEM_ENTRYID] = PROP_REF_ITEM_ENTRYID;

	if (lpFolder == NULL || lpSetRefs == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Getting list of messages with back references...");

	hr = lpFolder->GetContentsTable(0, &ptrContentsTable);
	if (hr != hrSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Failed to get contents table. (hr=0x%08x)", hr);
		goto exit;
	}

	hr = ptrContentsTable->SetColumns((LPSPropTagArray)&sptaColumnProps, TBL_BATCH);
	if (hr != hrSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Failed to select columns on contents table. (hr=0x%08x)", hr);
		goto exit;
	}

	while (true) {
		hr = ptrContentsTable->QueryRows(64, 0, &ptrRows);
		if (hr != hrSuccess) {
			m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Failed to query contents table. (hr=0x%08x)", hr);
			goto exit;
		}

		if (ptrRows.empty())
			break;

		m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Got %u rows from contents table", ptrRows.size());
		for (mapi_rowset_ptr::size_type i = 0; i < ptrRows.size(); ++i) {
			if (PROP_TYPE(ptrRows[i].lpProps[IDX_REF_ITEM_ENTRYID].ulPropTag) != PT_ERROR) {
				m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Adding row %u", i);
				setRefs.insert(make_pair(ptrRows[i].lpProps[IDX_ENTRYID].Value.bin, ptrRows[i].lpProps[IDX_REF_ITEM_ENTRYID].Value.bin));
			}
		}
	}

	m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Got "SIZE_T_PRINTF" rows from contents table", setRefs.size());
	lpSetRefs->swap(setRefs);

exit:
	return hr;
}
