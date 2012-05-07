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

#ifndef ECINDEXER_H
#define ECINDEXER_H

#include <list>
#include <string>
#include <vector>

#include <edkmdb.h>

#include <ECScheduler.h>
#include <ECUnknown.h>

#include "ECEntryData.h"
#include "zarafa-indexer.h"
#include "tstring.h"

class ECSynchronization;

/**
 * Main indexer handler
 *
 * This class manages all aspects of the indexing. It runs in its own
 * private thread and will periodically start the indexing process by
 * starting indexer threads.
 */
class ECIndexer : public ECUnknown {
private:
	/**
	 * Constructor
	 *
	 * @note Objects of ECIndexer must only be created using the Create() function.
	 *
	 * @param[in]	lpThreadData
	 * @param[in]	lpScheduler
	 */
	ECIndexer(ECThreadData *lpThreadData, ECScheduler *lpScheduler);

public:
	/**
	 * Create new ECIndexer object.
	 *
	 * @note Creating a new ECIndexer object must always occur through this function.
	 *
	 * @param[in]	lpThreadData
	 *					Reference to the ECThreadData object.
	 * @param[out]	lppIndexer
	 *					The created ECIndexer object.
	 * @return HRESULT
	 */
	static HRESULT Create(ECThreadData *lpThreadData, ECIndexer **lppIndexer);

	/**
	 * Destructor
	 */
	~ECIndexer();

	/**
	 * Update synchronization base for a particular folder to harddisk.
	 *
	 * After a folder has been synchronized, the updated synchronization base must be
	 * written to harddisk to make it possible to continue from this point after a restart.
	 *
	 * @param[in]   lpEntryData
	 *					Reference to ECEntryData object to which the folder belongs.
	 * @param[in]   lpFolderData
	 *					Reference to ECFolderData object for which we update the synchronization base.
	 * @return HRESULT
	 */
	HRESULT UpdateFolderSyncBase(ECEntryData *lpEntryData, ECFolderData *lpFolderData);

	/**
	 * Signal handler to indicate all synchronization threads have completed
	 *
	 * This signal allows ECIndexer to close continue processing and schedule the next synchronization  round.
	 *
	 * @return HRESULT
	 */
	HRESULT RunSynchronizationDone();

	HRESULT RunSynchronization();

	/**
	 * Optimize on-disk index
	 *
	 * @return result
	 */
	HRESULT OptimizeIndex(ECEntryData *lpEntryData);

private:
	/**
	 * Main thread handler
	 *
	 * This function will be run periodically to start the indexing process.
	 *
	 * @param[in]	lpVoid
	 *					Reference to ECIndexer object
	 * @return LPVOID
	 */
	static LPVOID RunThread(LPVOID lpVoid);

	/** 
	 * Enables index optimization for the next indexing run
	 * 
	 * @param[in] lpVoid pointer to instance of this
	 * 
	 * @return LPVOID
	 */
	static LPVOID EnableOptimizeIndex(LPVOID lpVoid);

	/**
	 * Main indexing function
	 *
	 * This function will be called by RunThread to start synchronizing the
	 * userlist and distribute all tasks over multiple indexing threads.
	 *
	 * @return HRESULT
	 */
	HRESULT RunIndexer();

	/**
	 * Optimize memory consumption
	 *
	 * Run Optimize() on multiple components which are performing caching. Each of those
	 * handlers can determine if the cached should be purged to reduce memory consumption.
	 *
	 * @return HRESULT
	 */
	HRESULT Optimize();

	/**
	 * Start synchronization of user changes
	 *
	 * This function must be run before the indexing threads are started. It will synchronize
	 * with the address book to add or remove users from the userlist.
	 *
	 * @return HRESULT
	 */
	HRESULT LoadUserList();

	/**
	 * Synchronize with Address Book
	 *
	 * Synchronize all creations/deletions in the address book with the storelist.
	 *
	 * @param[in]	lpSyncer
	 *					Reference to ECSynchronization object which contains all synchronized changes.
	 * @param[in]	lpAdminSession
	 *					IMAPISession reference for the SYSTEM user.
	 * @param[in]	lpAdminStore
	 *					IMsgStore reference for the SYSTEM user.
	 * @return HRESULT
	 */
	HRESULT LoadUserChanges(ECSynchronization *lpSyncer, IMAPISession *lpAdminSession, IMsgStore *lpAdminStore);

	/**
	 * Create ECEntryData object from Address Book change notification
	 *
	 * This will create a ECEntryData object and add it to the list of ECEntryData objects.
	 *
	 * @param[in]	lpAdminSession
	 *					IMAPISession reference for the SYSTEM user.
	 * @param[in]	lpIEMS
	 *					IExchangeManageStore reference for the SYSTEM user
	 * @param[in]	bPublicFolder
	 *					Is store for user a public store (user is group Everyone or company)
	 * @param[in]	cbProps
	 *					Number of SPropValue elements in lpProps
	 * @param[in]	lpProps
	 *					Array of SPropValue elements containing all user information
	 * @param[out]	lppEntry
	 *					Reference to the ECEntryData object which was synchronized.
	 * @return HRESULT
	 */
	HRESULT LoadUser(IMAPISession *lpAdminSession, IExchangeManageStore *lpIEMS, BOOL bPublicFolder,
					 ULONG cbProps, LPSPropValue lpProps, ECEntryData **lppEntry);

	/**
	 * Check if a given name matches the provided filter list.
	 *
	 * Some filters require the name to be present in the filter list, while others
	 * require the name not to be in the list. In the former case @bMustContain must
	 * be set to true, while in the latter case @bMustContain must be set to false.
	 * This function will check if the name is available in the list, and will
	 * will MAPI_E_NOT_ME when the name did not pass the filter.
	 *
	 * @param[in]	lFilter
	 *					List containing all strings which should be filtered.
	 * @param[in]	bMustContain
	 *					True if the provided name must be present in the filter,
	 *					False is the provided name must not be present in the filter.
	 * @param[in]	lpszName
	 *					Name to search in filter list.
	 * @return HRESULT
	 */
	HRESULT CheckFilterList(const std::vector<tstring> &lFilter, bool bMustContain, LPCTSTR lpszName);

	/**
	 * Collect all changes from the synchronization and update the ECEntryData object
	 *
	 * @param[in]	lpSyncer
	 *					Reference to ECSynchronization object which contains all synchronized changes.
	 * @param[in]	lpEntry
	 *					Reference to ECEntryData object to which the folders must be attached.
	 * @param[in]	lpRootFolder
	 *					Root folder from where the the hierarchy is being synchronized
	 * @return HRESULT
	 */
	HRESULT LoadUserFolderChanges(ECSynchronization *lpSyncer, ECEntryData *lpEntry, IMAPIFolder *lpRootFolder);

	/**
	 * Create ECFolderData object and add it to the ECEntryData object.
	 *
	 * Create a ECFolderData object based on the given properties, and attach the folder
	 * to the ECEntryData object.
	 *
	 * @param[in]	lpEntry
	 *					Reference to ECEntryData object to which the folder must be attached.
	 * @param[in]	ulProps
	 *					Number of SPropValue elements in lpProps parameter.
	 * @param[in]	lpProps
	 *					Array of SPropValue containing all required information about the folder.
	 * @return HRESULT
	 */
	HRESULT LoadUserFolder(ECEntryData *lpEntry, ULONG ulProps, LPSPropValue lpProps);

	/**
	 * Collect all changes from the synchronization and update the ECEntryData object
	 *
	 * @param[in]	lpSyncer
	 *					Reference to ECSynchronization object which contains all synchronized changes.
	 * @param[in]	lpEntry
	 *					Reference to ECEntryData object with all folders which have been synced
	 * @param[in]	lpMsgStore
	 *					Reference to IMsgStore object which is being synchronized
	 * @param[in]	lpAdminSession
	 * 					The MAPI admin session.
	 * @return HRESULT
	 */
	HRESULT LoadUserFolderContentChanges(ECSynchronization *lpSyncer, ECEntryData *lpEntry, IMsgStore *lpMsgStore, IMAPISession *lpAdminSession);

private:
	ECThreadData  *m_lpThreadData;
	ECScheduler *m_lpScheduler;
	enum { DISABLE_OPTIMIZE, NEXT_OPTIMIZE, RUN_OPTIMIZE } m_eOptimize;

	pthread_mutexattr_t m_hThreadLockAttr;
	pthread_mutex_t m_hThreadLock;
	pthread_cond_t m_hThreadCond;

	pthread_mutex_t m_hRunSynchronizationLock;

	LPSTREAM m_lpABSyncStream;
	entrydata_list_t m_lUsers;

	std::vector<tstring> m_lBlockUsers;
	std::vector<tstring> m_lBlockCompanies;
	std::vector<tstring> m_lAllowServers;

	friend class ECIndexerData;
};

#endif /* ECINDEXER_H */
