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

#include <algorithm>

#include <mapi.h>
#include <mapix.h>
#include <mapiutil.h>
#include <mapidefs.h>
#include <mapiguid.h>

#include <edkguid.h>

#include <CommonUtil.h>
#include <ECABEntryID.h>
#include <restrictionutil.h>
#include <stringutil.h>

#include "ECFileIndex.h"
#include "ECIndexer.h"
#include "ECIndexerData.h"
#include "ECIndexerUtil.h"
#include "ECLucene.h"
#include "ECLuceneIndexer.h"
#include "ECSynchronization.h"
#include "ECSynchronizationThread.h"
#include "zarafa-indexer.h"
#include "threadutil.h"

ECIndexer::ECIndexer(ECThreadData *lpThreadData, ECScheduler *lpScheduler)
{
	m_lpThreadData = lpThreadData;
	m_lpScheduler = lpScheduler;
	m_eOptimize = DISABLE_OPTIMIZE;

	m_lpABSyncStream = NULL;

	pthread_mutexattr_init(&m_hThreadLockAttr);
	pthread_mutexattr_settype(&m_hThreadLockAttr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&m_hThreadLock, &m_hThreadLockAttr);
	pthread_cond_init(&m_hThreadCond, NULL);

	pthread_mutex_init(&m_hRunSynchronizationLock, NULL);
	
	m_lBlockUsers = tokenize(GetConfigSetting(m_lpThreadData->lpConfig, "index_block_users"), ' ');
	m_lBlockCompanies = tokenize(GetConfigSetting(m_lpThreadData->lpConfig, "index_block_companies"), ' ');
	m_lAllowServers = tokenize(GetConfigSetting(m_lpThreadData->lpConfig, "index_allow_servers"), ' ');
}

ECIndexer::~ECIndexer()
{
	entrydata_list_t::iterator iter;

	pthread_mutex_lock(&m_hThreadLock);
	pthread_cond_broadcast(&m_hThreadCond);
	pthread_mutex_unlock(&m_hThreadLock);

	if (m_lpScheduler)
		delete m_lpScheduler;

	if (m_lpABSyncStream)
		m_lpABSyncStream->Release();

	for (iter = m_lUsers.begin(); iter != m_lUsers.end(); iter++)
		(*iter)->Release();

	pthread_cond_destroy(&m_hThreadCond);
	pthread_mutex_destroy(&m_hThreadLock);
	pthread_mutexattr_destroy(&m_hThreadLockAttr);

	pthread_mutex_destroy(&m_hRunSynchronizationLock);
}

HRESULT ECIndexer::Create(ECThreadData *lpThreadData, ECIndexer **lppIndexer)
{
	HRESULT hr = hrSuccess;
	ECScheduler *lpScheduler = NULL;
	ECIndexer *lpIndexer = NULL;
	const char *lpszInterval = NULL;
	ULONG ulInterval = 0;

	try {
		lpScheduler = new ECScheduler(lpThreadData->lpLogger);
	}
	catch (...) {
		lpScheduler = NULL;
	}
	if (!lpScheduler) {
		hr = MAPI_E_NOT_ENOUGH_MEMORY;
		goto exit;
	}

	try {
		lpIndexer = new ECIndexer(lpThreadData, lpScheduler);
	}
	catch (...) {
		lpIndexer = NULL;
	}
	if (!lpIndexer) {
		hr = MAPI_E_NOT_ENOUGH_MEMORY;
		goto exit;
	}

	lpIndexer->AddRef();

	lpszInterval = lpThreadData->lpConfig->GetSetting("index_interval");
	ulInterval = lpszInterval ? atoi(lpszInterval) : 0;
	if (!ulInterval) {
		lpThreadData->lpLogger->Log(EC_LOGLEVEL_FATAL, "Invalid value for \"index_interval\", using default of 5 minutes.");
		ulInterval = 5;
	}
	if (ulInterval > 60) {
		lpThreadData->lpLogger->Log(EC_LOGLEVEL_FATAL, "Invalid value for \"index_interval\". You can only set a maximum of 60 minutes interval.");
		ulInterval = 60;
	}

	hr = lpScheduler->AddSchedule(SCHEDULE_MINUTES, ulInterval, ECIndexer::RunThread, lpIndexer);
	if (hr != hrSuccess) {
		lpThreadData->lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to add indexer schedule");
		goto exit;
	}

	// enable disk index optimization in indexing run at 2 am
	hr = lpScheduler->AddSchedule(SCHEDULE_DAY, 2, ECIndexer::EnableOptimizeIndex, lpIndexer);
	if (hr != hrSuccess) {
		lpThreadData->lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to schedule index optimizition");
		goto exit;
	}

	if (lppIndexer)
		*lppIndexer = lpIndexer;

exit:
	if (hr != hrSuccess) {
		if (lpIndexer)
			lpIndexer->Release();
		if (lpScheduler)
			delete lpScheduler;
	}

	return hr;
}

HRESULT ECIndexer::RunSynchronization()
{
	HRESULT hr = hrSuccess;
	scoped_lock lock(m_hRunSynchronizationLock);

	if (m_lpThreadData->bShutdown)
		return hrSuccess;

	if (m_eOptimize == NEXT_OPTIMIZE)
		m_eOptimize = RUN_OPTIMIZE;

	m_lpThreadData->lpLogger->Log(EC_LOGLEVEL_ERROR, "Zarafa Indexer thread started");

	hr = MAPIInitialize(NULL);
	if (hr != hrSuccess)
		goto exit;

	hr = RunIndexer();
	if (hr != hrSuccess)
		goto exit;

exit:
	MAPIUninitialize();
	if (m_eOptimize == RUN_OPTIMIZE)
		m_eOptimize = DISABLE_OPTIMIZE;

	if (hr == hrSuccess)
		m_lpThreadData->lpLogger->Log(EC_LOGLEVEL_ERROR, "Zarafa Indexer thread completed");
	else
		m_lpThreadData->lpLogger->Log(EC_LOGLEVEL_ERROR, "Zarafa Indexer thread failed, error 0x%08X", hr);

	return hr;

}

LPVOID ECIndexer::RunThread(LPVOID lpVoid)
{
	ECIndexer *lpIndexer = (ECIndexer *)lpVoid;
	lpIndexer->RunSynchronization();
	return NULL;
}

LPVOID ECIndexer::EnableOptimizeIndex(LPVOID lpVoid)
{
	ECIndexer *lpIndexer = (ECIndexer *)lpVoid;
	if (lpIndexer->m_eOptimize < NEXT_OPTIMIZE)
		lpIndexer->m_eOptimize = NEXT_OPTIMIZE;
	return NULL;
}

HRESULT ECIndexer::Optimize()
{
	HRESULT hr = hrSuccess;

	hr = m_lpThreadData->lpLucene->Optimize(FALSE);
	if (hr != hrSuccess)
		goto exit;

exit:
	return hr;
}

HRESULT ECIndexer::RunIndexer()
{
	HRESULT hr = hrSuccess;
	ECIndexerData *lpIndexerData = NULL;
	const char *lpszThreads = NULL;
	ULONG ulThreads = 0;
	ULONG ulDistribution = 0;
	struct timeval now;
	struct timespec timeout;

	hr = LoadUserList();
	if (hr != hrSuccess)
		goto exit;

	if (m_lUsers.empty()) {
		m_lpThreadData->lpLogger->Log(EC_LOGLEVEL_WARNING, "No users found to index");
		goto exit;
	}

	lpIndexerData = new ECIndexerData(this, m_lUsers);
	if (!lpIndexerData) {
		hr = MAPI_E_NOT_ENOUGH_MEMORY;
		goto exit;
	}

	lpszThreads = m_lpThreadData->lpConfig->GetSetting("index_threads");
	ulThreads = lpszThreads ? atoui(lpszThreads) : 0;
	if (!ulThreads)
		ulThreads = 1;

	hr = lpIndexerData->GetDistributionFactor(&ulDistribution);
	if (hr != hrSuccess)
		goto exit;

	if (ulDistribution < ulThreads)
		ulThreads = ulDistribution;

	for (ULONG i = 0; i < ulThreads; i++) {
		hr = lpIndexerData->CreateSyncThread(m_lpThreadData);
		if (hr != hrSuccess)
			goto exit;
	}

	pthread_mutex_lock(&m_hThreadLock);
	while (!lpIndexerData->AllThreadsCompleted()) {
		gettimeofday(&now, NULL);

		/* Wakeup every 5 minutes to peform optimizations */
		timeout.tv_sec = now.tv_sec + (5 * 60);
		timeout.tv_nsec = now.tv_usec * 1000;

		pthread_cond_timedwait(&m_hThreadCond, &m_hThreadLock, &timeout);

		if (Optimize() != hrSuccess)
			m_lpThreadData->lpLogger->Log(EC_LOGLEVEL_WARNING, "Optimization of cache memory failed");
	}
	pthread_mutex_unlock(&m_hThreadLock);

exit:
	if (lpIndexerData)
		delete lpIndexerData; 

	return hr;
}

HRESULT ECIndexer::UpdateFolderSyncBase(ECEntryData *lpEntryData, ECFolderData *lpFolderData)
{
	HRESULT hr = hrSuccess;
	STATSTG sStat;
	LARGE_INTEGER liPos = {{0, 0}};
	ULONG ulSize = 0;
	LPBYTE lpBuff = NULL;
	ULONG ulBuff = 0;

	hr = lpFolderData->m_lpContentsSyncBase->Stat(&sStat, STATFLAG_DEFAULT);
	if (hr != hrSuccess)
		goto exit;

	ulSize = sStat.cbSize.LowPart;
	if (sStat.cbSize.HighPart) {
		hr = MAPI_E_TOO_BIG;
		goto exit;
	}

	hr = MAPIAllocateBuffer(ulSize, (LPVOID *)&lpBuff);
	if (hr != hrSuccess)
		goto exit;

	hr = lpFolderData->m_lpContentsSyncBase->Seek(liPos, STREAM_SEEK_SET, NULL);
	if (hr != hrSuccess)
		goto exit;

	hr = lpFolderData->m_lpContentsSyncBase->Read(lpBuff, ulSize, &ulBuff);
	if (hr != hrSuccess)
		goto exit;

	if (ulSize != ulBuff) {
		hr = MAPI_E_CALL_FAILED;
		goto exit;
	}

	m_lpThreadData->lpFileIndex->SetStoreFolderSyncBase(lpFolderData->m_strFolderPath, ulBuff, lpBuff);
	/* Ignore return code from SetStoreFolderSyncBase, we don't care... */

exit:
	if (lpBuff)
		MAPIFreeBuffer(lpBuff);

	return hr;
}

HRESULT ECIndexer::RunSynchronizationDone()
{
	pthread_mutex_lock(&m_hThreadLock);
	pthread_cond_broadcast(&m_hThreadCond);
	pthread_mutex_unlock(&m_hThreadLock);

	return hrSuccess;
}

HRESULT ECIndexer::LoadUserList()
{
	HRESULT hr = hrSuccess;
	ECSynchronization *lpSyncer = NULL;
	IMAPISession *lpAdminSession = NULL;
	IMsgStore *lpAdminStore = NULL;

	hr = HrOpenECAdminSession(&lpAdminSession,
							  m_lpThreadData->lpConfig->GetSetting("server_socket"),
							  EC_PROFILE_FLAGS_NO_NOTIFICATIONS,
							  m_lpThreadData->lpConfig->GetSetting("sslkey_file", "", NULL),
							  m_lpThreadData->lpConfig->GetSetting("sslkey_pass", "", NULL));
	if (hr != hrSuccess) {
		m_lpThreadData->lpLogger->Log(EC_LOGLEVEL_FATAL, "Failed to connect to %s, error 0x%08X", m_lpThreadData->lpConfig->GetSetting("server_socket"), hr);
		goto exit;
	}

	hr = HrOpenDefaultStore(lpAdminSession, &lpAdminStore);
	if (hr != hrSuccess) {
		m_lpThreadData->lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to open admin store on  %s, error 0x%08X", m_lpThreadData->lpConfig->GetSetting("server_socket"), hr);
		goto exit;
	}

	if (!m_lpABSyncStream) {
		ULONG ulTmp[2] = { 0, 0 };

		hr = CreateSyncStream(&m_lpABSyncStream, sizeof(ulTmp), (LPBYTE)ulTmp);
		if (hr != hrSuccess)
			goto exit;
	}

	hr = ECSynchronization::Create(m_lpThreadData, NULL, &lpSyncer);
	if (hr != hrSuccess)
		goto exit;

	hr = lpSyncer->GetAddressBookChanges(lpAdminStore, m_lpABSyncStream);
	if (hr != hrSuccess)
		goto exit;

	hr = LoadUserChanges(lpSyncer, lpAdminSession, lpAdminStore);

exit:
	if (hr != hrSuccess) {
		m_lpThreadData->lpLogger->Log(EC_LOGLEVEL_FATAL, "Failed to load userlist, error 0x%08X", hr);

		// remove sync stream, so the next time we start over to get the complete userlist
		if (m_lpABSyncStream) {
			m_lpABSyncStream->Release();
			m_lpABSyncStream = NULL;
		}
	}

	if (lpAdminStore)
		lpAdminStore->Release();

	if (lpAdminSession)
		lpAdminSession->Release();

	if (lpSyncer)
		lpSyncer->Release();

	return hr;
}

struct finduser_if {
	const sourceid_list_t::value_type &m_entry;

	finduser_if(const sourceid_list_t::value_type &entry)
		: m_entry(entry) {}

	bool operator()(const entrydata_list_t::value_type &entry)
	{
		return m_entry->lpProps[0].Value.bin == entry->m_sUserEntryId;
	}
};

HRESULT ECIndexer::LoadUserChanges(ECSynchronization *lpSyncer, IMAPISession *lpAdminSession, IMsgStore *lpAdminStore)
{
	HRESULT hr = hrSuccess;
	ECEntryData *lpEntry = NULL;
	IExchangeManageStore *lpIEMS = NULL;
	LPADRBOOK lpAddrBook = NULL;
	IABContainer *lpABEveryone = NULL;
	IMAPITable *lpTable = NULL;
	LPSRestriction lpRestrict = NULL;
	LPSRowSet lpRows = NULL;
	ECChanges *lpChanges = NULL;
	sourceid_list_t::iterator iter;
	LPSPropValue lpProps = NULL;
	ULONG ulProps = 0;
	ULONG ulBatchSize = 0;
	ULONG ulObjType = 0;
	ULONG ulDuplicate = 0;
	ULONG ulInvalid = 0;
	ULONG ulBlocked = 0; 
	SizedSPropTagArray(4, sUserProps) = { 4, { PR_ENTRYID, PR_ACCOUNT, PR_EC_HOMESERVER_NAME, PR_EC_COMPANY_NAME } };

	SPropValue sContacts;
	sContacts.ulPropTag = PR_DISPLAY_TYPE;
	sContacts.Value.ul = DT_REMOTE_MAILUSER;

	hr = lpSyncer->GetNextChanges(&lpChanges);
	if (hr != hrSuccess)
		goto exit;

	m_lpThreadData->lpLogger->Log(EC_LOGLEVEL_WARNING, "Synchronizing Address Book - %lu creations, %lu deletions",
								  lpChanges->lCreate.size(), lpChanges->lDelete.size());

	if (lpChanges->lDelete.empty() && lpChanges->lCreate.empty())
		goto exit;

	hr = lpAdminSession->OpenAddressBook(0, &IID_IAddrBook, 0, &lpAddrBook);
	if (hr != hrSuccess)
		goto exit;

	hr = lpAdminStore->QueryInterface(IID_IExchangeManageStore, (LPVOID *)&lpIEMS);
	if (hr != hrSuccess)
		goto exit;

	if (!lpChanges->lDelete.empty() && !m_lUsers.empty()) {
		for (iter = lpChanges->lDelete.begin(); iter != lpChanges->lDelete.end(); iter++) {
			entrydata_list_t::iterator entry = find_if(m_lUsers.begin(), m_lUsers.end(), finduser_if(*iter));
			if (entry != m_lUsers.end()) {
				m_lpThreadData->lpLucene->Delete((*entry)->m_strStorePath);

				(*entry)->Release();
				m_lUsers.erase(entry);
			}
		}
	}

	if (!lpChanges->lCreate.empty()) {
		/*
		 * Only group everyone contains users from all different companies,
		 * so use the contents table of this group as basis.
		 */
		hr = lpAddrBook->OpenEntry(g_cbEveryoneEid, (LPENTRYID)g_lpEveryoneEid,
								   &IID_IABContainer, 0, &ulObjType, (LPUNKNOWN *)&lpABEveryone);
		if (hr != hrSuccess) {
			m_lpThreadData->lpLogger->Log(EC_LOGLEVEL_WARNING, "Unable to open group Everyone for public store, error 0x%08X", hr);
			goto exit;
		}

		/*
		 * Only during initialization group Everyone should be added as well,
		 * this ensures the Public Store in single tenancy environments will still be synced.
		 */
		if (m_lUsers.empty()) {
			m_lpThreadData->lpLogger->Log(EC_LOGLEVEL_WARNING, "Synchronizing Address Book - Adding public store");

			hr = lpABEveryone->GetProps((LPSPropTagArray)&sUserProps, 0, &ulProps, &lpProps);
			if (hr != hrSuccess) {
				/* Don't care if not all properties have been provided */
				if (hr == MAPI_W_ERRORS_RETURNED)
					hr = hrSuccess;
				else
					goto exit;
			}

			hr = LoadUser(lpAdminSession, lpIEMS, TRUE, ulProps, lpProps, &lpEntry);
			if (hr != hrSuccess) {
				if (hr != MAPI_E_NOT_FOUND && hr != MAPI_E_NOT_ME)
					goto exit;
				hr = hrSuccess;
			} else {
				m_lUsers.push_back(lpEntry);
				lpEntry = NULL;
			}
		}

        hr = lpABEveryone->GetContentsTable(0, &lpTable);
		if (hr != hrSuccess)
			goto exit;

		hr = lpTable->SetColumns((LPSPropTagArray)&sUserProps, TBL_BATCH);
		if (hr != hrSuccess)
			goto exit;

		/*
		 * It is possible that we will have to sync a lot of users,
		 * we won't use a single restriction for all of them since that
		 * could stress out the server.
		 */
		CREATE_RESTRICTION(lpRestrict);
		CREATE_RES_AND(lpRestrict, lpRestrict, 2);

		/* Contacts don't have a store, don't synchronize them */
		DATA_RES_PROPERTY_CHEAP(lpRestrict, lpRestrict->res.resAnd.lpRes[0], RELOP_NE, sContacts.ulPropTag, &sContacts);
		CREATE_RES_OR(lpRestrict, &lpRestrict->res.resAnd.lpRes[1], 250);

		iter = lpChanges->lCreate.begin();

		while (iter!= lpChanges->lCreate.end()) {
			lpRestrict->res.resAnd.lpRes[1].res.resOr.cRes = 0;

			for (unsigned int n = 0; iter != lpChanges->lCreate.end() && n < 250; iter++) {
				/* Prevent duplicate creation */
				entrydata_list_t::iterator entry = find_if(m_lUsers.begin(), m_lUsers.end(), finduser_if(*iter));
				if (entry != m_lUsers.end()) {
					ulDuplicate++;
					continue;
				}

				/**
				 * Cheap copy, no need for allocation/copying.
				 * Use PR_ENTRYID here, which is what the server knows
				 * of a user. lpProps[0] (Set in
				 * ECSynchronizationImportChanges.cpp) will contain
				 * PR_ADDRESS_BOOK_ENTRYID to let the server use the
				 * correct compare function.
				 */
				DATA_RES_PROPERTY_CHEAP(lpRestrict,
										lpRestrict->res.resAnd.lpRes[1].res.resOr.lpRes[n],
										RELOP_EQ, PR_ENTRYID, &(*iter)->lpProps[0]);
				n++;
				lpRestrict->res.resAnd.lpRes[1].res.resOr.cRes++;
			}

			hr = lpTable->Restrict(lpRestrict, TBL_BATCH);
			if (hr != hrSuccess)
				goto exit;

			hr = lpTable->GetRowCount(0, &ulBatchSize);
			if (hr != hrSuccess)
				goto exit;

			/* Calculate number of invalid entries */
			ulInvalid += lpRestrict->res.resAnd.lpRes[1].res.resOr.cRes - ulBatchSize;

			while (TRUE) {
				hr = lpTable->QueryRows(25, 0, &lpRows);
				if (hr != hrSuccess)
					goto exit;

				if (lpRows->cRows == 0)
					break;

				for (ULONG i = 0; i < lpRows->cRows; i++) {
					hr = LoadUser(lpAdminSession, lpIEMS, FALSE, lpRows->aRow[i].cValues, lpRows->aRow[i].lpProps, &lpEntry);
					if (hr != hrSuccess) {
						if (hr == MAPI_E_NOT_FOUND) {
							ulInvalid++;
						} else if (hr == MAPI_E_NOT_ME) {
							ulBlocked++;
						}
						// LoadUser logged the error, we just continue with the other users
						hr = hrSuccess;
					} else {
						m_lUsers.push_back(lpEntry);
						lpEntry = NULL;
					}
				}

				if (lpRows)
					FreeProws(lpRows);
				lpRows = NULL;
			}
			
			if (lpRows)
				FreeProws(lpRows);
			lpRows = NULL;
		}
	}

	m_lpThreadData->lpLogger->Log(EC_LOGLEVEL_WARNING, "Synchronizing Address Book - Ignored creations: %d duplicate, %d invalid, %d blocked",
								  ulDuplicate, ulInvalid, ulBlocked);

exit:
	if (lpEntry)
		lpEntry->Release();

	if (lpProps)
		MAPIFreeBuffer(lpProps);

	if (lpRows)
		FreeProws(lpRows);

	if (lpRestrict)
		MAPIFreeBuffer(lpRestrict);

	if (lpTable)
		lpTable->Release();

	if (lpABEveryone)
		lpABEveryone->Release();

	if (lpAddrBook)
		lpAddrBook->Release();

	if (lpIEMS)
		lpIEMS->Release();

	if (lpChanges)
		delete lpChanges;

	return hr;
}

HRESULT ECIndexer::CheckFilterList(const std::vector<tstring> &lFilter, bool bMustContain, LPCTSTR lpszName)
{
	HRESULT hr = hrSuccess;
	std::vector<tstring>::const_iterator iter;

	if (_tcslen(lpszName) == 0)
		goto exit;

	iter = find(lFilter.begin(), lFilter.end(), tstring(lpszName));
	if ((iter == lFilter.end()) != bMustContain) {
		hr = MAPI_E_NOT_ME;
		goto exit;
	}

exit:
	return hr;
}

HRESULT ECIndexer::LoadUser(IMAPISession *lpAdminSession, IExchangeManageStore *lpIEMS, BOOL bPublicFolder,
							ULONG cbProps, LPSPropValue lpProps, ECEntryData **lppEntry)
{
	HRESULT hr = hrSuccess;
	ECEntryData *lpEntry = NULL;
	LPSPropValue lpAccount = NULL;
	LPSPropValue lpEntryID = NULL;
	LPSPropValue lpServerName = NULL;
	LPSPropValue lpCompanyName = NULL;
	ULONG ulTmp[2] = { 0, 0 };

	SPropValue sStoreEntryId;
	sStoreEntryId.ulPropTag = PR_STORE_ENTRYID;
	sStoreEntryId.Value.bin.cb = 0;
	sStoreEntryId.Value.bin.lpb = NULL;

	SPropValue sTmpServer;
	sTmpServer.ulPropTag = PR_EC_HOMESERVER_NAME;
	sTmpServer.Value.LPSZ = _T("");

	SPropValue sTmpCompany;
	sTmpCompany.ulPropTag = PR_EC_COMPANY_NAME;
	sTmpCompany.Value.LPSZ = _T("");

	lpAccount = PpropFindProp(lpProps, cbProps, PR_ACCOUNT);
	if (!lpAccount) {
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}

	lpEntryID = PpropFindProp(lpProps, cbProps, PR_ENTRYID);
	if (!lpEntryID) {
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}

	lpServerName = PpropFindProp(lpProps, cbProps, PR_EC_HOMESERVER_NAME);
	if (!lpServerName)
		lpServerName = &sTmpServer;

	lpCompanyName = PpropFindProp(lpProps, cbProps, PR_EC_COMPANY_NAME);
	if (!lpCompanyName)
		lpCompanyName = &sTmpCompany;

	/* Check is this server is allowed */
	if (!m_lAllowServers.empty()) {
		/* A filter was applied, check if this server is allowed */
		hr = CheckFilterList(m_lAllowServers, false, lpServerName->Value.LPSZ);
		if (hr != hrSuccess)
			goto exit;
	}

	/* Check if user has been filtered out because of the server/company/username */
	hr = CheckFilterList(m_lBlockCompanies, true, lpCompanyName->Value.LPSZ);
	if (hr != hrSuccess)
		goto exit;

	hr = CheckFilterList(m_lBlockUsers, true, lpAccount->Value.LPSZ);
	if (hr != hrSuccess)
		goto exit;

	hr = ECEntryData::Create(&lpEntry);
	if (hr != hrSuccess)
		goto exit;

	lpEntry->m_strUserName = lpAccount->Value.LPSZ;

	hr = HrAddEntryData(&lpEntry->m_sUserEntryId, lpEntry, &lpEntryID->Value.bin);
	if (hr != hrSuccess)
		goto exit;

	if (bPublicFolder) {
		hr = HrSearchECStoreEntryId(lpAdminSession, TRUE, &sStoreEntryId.Value.bin.cb, (LPENTRYID *)&sStoreEntryId.Value.bin.lpb);
	} else {
		hr = lpIEMS->CreateStoreEntryID((LPTSTR)_T(""), (LPTSTR)lpEntry->m_strUserName.c_str(), OPENSTORE_HOME_LOGON|fMapiUnicode,
										&sStoreEntryId.Value.bin.cb, (LPENTRYID *)&sStoreEntryId.Value.bin.lpb);
	}
	if (hr != hrSuccess)
		goto exit;

	hr = HrAddEntryData(&lpEntry->m_sStoreEntryId, lpEntry, &sStoreEntryId.Value.bin);
	if (hr != hrSuccess)
		goto exit;

	lpEntry->m_strServerName = lpServerName->Value.LPSZ;
	lpEntry->m_bPublicFolder = bPublicFolder;

	hr = CreateSyncStream(&lpEntry->m_lpHierarchySyncBase, sizeof(ulTmp), (LPBYTE)ulTmp);
	if (hr != hrSuccess)
		goto exit;

	if (lppEntry)
		*lppEntry = lpEntry;

exit:
	if (bPublicFolder && hr == MAPI_E_NOT_FOUND)
		m_lpThreadData->lpLogger->Log(EC_LOGLEVEL_WARNING, "Synchronizing Address Book - Public store doesn't exist or multi-tenancy is enabled");
	else if (hr == MAPI_E_NOT_ME) {
		m_lpThreadData->lpLogger->Log(EC_LOGLEVEL_WARNING, "Synchronizing Address Book - Skipping blocked user '%ls'",
									  lpAccount ? lpAccount->Value.LPSZ: _T("<Unknown user>"));
	} else if (hr != hrSuccess) {
		if (!lpServerName || lpServerName->Value.LPSZ[0] == '\0')
			m_lpThreadData->lpLogger->Log(EC_LOGLEVEL_ERROR, "Synchronizing Address Book - Failed to synchronize user '%ls', error 0x%08X",
										  lpAccount ? lpAccount->Value.LPSZ: _T("<Unknown user>"), hr);
		else
			m_lpThreadData->lpLogger->Log(EC_LOGLEVEL_ERROR, "Synchronizing Address Book - Failed to synchronize user '%ls' on server '%ls', error 0x%08X",
										  lpAccount ? lpAccount->Value.LPSZ: _T("<Unknown user>"), lpServerName->Value.LPSZ, hr);
	}

	if (sStoreEntryId.Value.bin.lpb)
		MAPIFreeBuffer(sStoreEntryId.Value.bin.lpb);

	if ((hr != hrSuccess) && lpEntry)
		lpEntry->Release();

	return hr;
}

struct findfolder_if {
	sourceid_list_t::value_type m_sValue;

	findfolder_if(sourceid_list_t::value_type sValue)
		: m_sValue(sValue) {}

	bool operator()(folderdata_list_t::value_type &entry)
	{
		return m_sValue->lpProps[0].Value.bin == entry->m_sFolderEntryId;
	}
};

HRESULT ECIndexer::LoadUserFolderChanges(ECSynchronization *lpSyncer, ECEntryData *lpEntry, IMAPIFolder *lpRootFolder)
{
	HRESULT hr = hrSuccess;
	ECChanges *lpChanges = NULL;
	IMAPITable *lpTable = NULL;
	LPSRestriction lpRestrict = NULL;
	LPSRowSet lpRows = NULL;
	sourceid_list_t::iterator iter;
	SizedSPropTagArray(3, sFolderProps) = { 3, { PR_SOURCE_KEY, PR_ENTRYID, PR_DISPLAY_NAME } };

	hr = lpSyncer->GetNextChanges(&lpChanges);
	if (hr != hrSuccess)
		goto exit;

	if (!lpChanges->lDelete.empty()) {
		for (iter = lpChanges->lDelete.begin(); iter != lpChanges->lDelete.end(); iter++) {
			folderdata_list_t::iterator entry = find_if(lpEntry->m_lFolders.begin(), lpEntry->m_lFolders.end(), findfolder_if(*iter));
			if (entry !=lpEntry->m_lFolders.end()) {
				(*entry)->Release();
				lpEntry->m_lFolders.erase(entry);
			}
		}
	}

	if (!lpChanges->lCreate.empty()) {
		hr = lpRootFolder->GetHierarchyTable(CONVENIENT_DEPTH, &lpTable);
		if (hr != hrSuccess)
			goto exit;

		CREATE_RESTRICTION(lpRestrict);
		CREATE_RES_OR(lpRestrict, lpRestrict, lpChanges->lCreate.size());
		lpRestrict->res.resOr.cRes = 0;

		for (iter = lpChanges->lCreate.begin(); iter != lpChanges->lCreate.end(); iter++) {
			/* Cheap copy, no need for allocation/copying */
			DATA_RES_PROPERTY_CHEAP(lpRestrict,
									lpRestrict->res.resOr.lpRes[lpRestrict->res.resOr.cRes],
									RELOP_EQ, (*iter)->lpProps[0].ulPropTag, &(*iter)->lpProps[0]);
			lpRestrict->res.resOr.cRes++;
		}

		hr = lpTable->Restrict(lpRestrict, TBL_BATCH);
		if (hr != hrSuccess)
			goto exit;

		hr = lpTable->SetColumns((LPSPropTagArray)&sFolderProps, TBL_BATCH);
		if (hr != hrSuccess)
			goto exit;

		while (TRUE) {
			hr = lpTable->QueryRows(25, 0, &lpRows);
			if (hr != hrSuccess)
				goto exit;

			if (lpRows->cRows == 0)
				break;

			for (ULONG i = 0; i < lpRows->cRows; i++) {
				hr = LoadUserFolder(lpEntry, lpRows->aRow[i].cValues, lpRows->aRow[i].lpProps);
				if (hr != hrSuccess)
					goto exit;
			}

			if (lpRows)
				FreeProws(lpRows);
			lpRows = NULL;
		}
	}

exit:
	if (lpRows)
		FreeProws(lpRows);

	if (lpRestrict)
		MAPIFreeBuffer(lpRestrict);

	if (lpTable)
		lpTable->Release();

	if (lpChanges)
		delete lpChanges;

	return hr;
}

HRESULT ECIndexer::LoadUserFolder(ECEntryData *lpEntry, ULONG ulProps, LPSPropValue lpProps)
{
	HRESULT hr = hrSuccess;
	ECFolderData *lpFolderData = NULL;
	LPSPropValue lpFolderName = NULL;
	LPSPropValue lpSourceKey = NULL;
	LPSPropValue lpEntryId = NULL;
	LPBYTE lpStreamData = NULL;
	ULONG ulStreamSize = 0;
	std::string strTmp;

	hr = ECFolderData::Create(lpEntry, &lpFolderData);
	if (hr != hrSuccess)
		goto exit;

	lpFolderName = PpropFindProp(lpProps, ulProps, PR_DISPLAY_NAME);
	if (!lpFolderName) {
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}

	lpFolderData->m_strFolderName = lpFolderName->Value.LPSZ;

	lpSourceKey = PpropFindProp(lpProps, ulProps, PR_SOURCE_KEY);
	if (!lpSourceKey) {
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}
				
	hr = HrAddEntryData(&lpFolderData->m_sFolderSourceKey, lpEntry, &lpSourceKey->Value.bin);
	if (hr != hrSuccess)
		goto exit;

	lpEntryId = PpropFindProp(lpProps, ulProps, PR_ENTRYID);
	if (!lpEntryId) {
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}

	hr = HrAddEntryData(&lpFolderData->m_sFolderEntryId, lpEntry, &lpEntryId->Value.bin);
	if (hr != hrSuccess)
		goto exit;

	strTmp = bin2hex(lpEntryId->Value.bin.cb, lpEntryId->Value.bin.lpb);
	hr = m_lpThreadData->lpFileIndex->GetStoreFolderInfoPath(lpEntry->m_strStorePath, strTmp, &lpFolderData->m_strFolderPath);
	if (hr != hrSuccess)
		goto exit;

	hr = m_lpThreadData->lpFileIndex->GetStoreFolderSyncBase(lpFolderData->m_strFolderPath, &ulStreamSize, &lpStreamData);
	if (hr != hrSuccess)
		goto exit;

	hr = CreateSyncStream(&lpFolderData->m_lpContentsSyncBase, ulStreamSize, lpStreamData);
	if (hr != hrSuccess)
		goto exit;

exit:
	if (lpStreamData)
		MAPIFreeBuffer(lpStreamData);

	if ((hr != hrSuccess) && lpFolderData) {
		lpEntry->m_lFolders.remove(lpFolderData);
		lpFolderData->Release();
	}

	return hr;
}

HRESULT ECIndexer::LoadUserFolderContentChanges(ECSynchronization *lpSyncer, ECEntryData *lpEntry, IMsgStore *lpMsgStore, IMAPISession *lpAdminSession)
{
	HRESULT hr = hrSuccess;
	ECLuceneIndexer *lpLucene = NULL;
	ECChanges *lpChanges = NULL;
	folderdata_list_t::iterator iter;

	hr = m_lpThreadData->lpLucene->CreateIndexer(m_lpThreadData, lpEntry->m_strStorePath, lpMsgStore, lpAdminSession, &lpLucene);
	if (hr != hrSuccess)
		goto exit;

	/* Indexing wasn't done in a seperate thread, so we must run the action now */
	hr = lpSyncer->GetNextChanges(&lpChanges);
	if (hr != hrSuccess)
		goto exit;

	hr = lpLucene->IndexEntries(lpChanges->lCreate, lpChanges->lChange, lpChanges->lDelete);
	if (hr != hrSuccess)
		goto exit;

	// quit before updating sync base. next run, user will be reindexed
	if (m_lpThreadData->bShutdown)
		goto exit;

	for (iter = lpEntry->m_lFolders.begin(); iter != lpEntry->m_lFolders.end(); iter++) {
		if (UpdateFolderSyncBase(lpEntry, (*iter)) != hrSuccess)
			continue;
	}

exit:
	if (lpChanges)
		delete lpChanges;

	if (lpLucene)
		lpLucene->Release();

	return hr;
}

HRESULT ECIndexer::OptimizeIndex(ECEntryData *lpEntry)
{
	HRESULT hr = hrSuccess;

	if (m_eOptimize == RUN_OPTIMIZE) {
		m_lpThreadData->lpLogger->Log(EC_LOGLEVEL_WARNING, "Optimizing disk index");
		hr = m_lpThreadData->lpLucene->OptimizeIndex(m_lpThreadData, lpEntry->m_strStorePath);
	}

	return hr;
}
