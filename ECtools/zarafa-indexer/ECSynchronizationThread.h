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

#ifndef ECSYNCHRONIZATIONTHREAD_H
#define ECSYNCHRONIZATIONTHREAD_H

#include <ECUnknown.h>

#include "ECChangeData.h"
#include "ECIndexerData.h"
#include "zarafa-indexer.h"

class ECEntryData;
class ECLuceneIndexer;
class ECSynchronization;

/**
 * Synchronization thread handler
 *
 * This class is created by the ECIndexerData class, and is designed to
 * requests tasks from the ECIndexerData structure to perform the
 * synchronization in its own private thread.
 */
class ECSynchronizationThread : public ECUnknown {
private:
	/**
	 * Constructor
	 *
	 * @note Objects of ECSynchronizationThread must only be created using the Create() function.
	 *
	 * @param[in]	lpThreadData
	 * @param[in]	lpIndexerData
	 */
	ECSynchronizationThread(ECThreadData *lpThreadData, ECIndexerData *lpIndexerData);

public:
	/**
	 * Create new ECSynchronizationThread object
	 *
	 * @note Creating a new ECSynchronizationThread object must always occur through this function.
	 *
	 * @param[in]	lpThreadData
	 *					Reference to the ECThreadData object.
	 * @param[in]	lpIndexerData
	 *					Reference to the ECIndexerData object.
	 * @param[out]	lppSynchronizationThread
	 *					The created ECSynchronizationThread object.
	 * @return HRESULT
	 */
	static HRESULT Create(ECThreadData *lpThreadData, ECIndexerData *lpIndexerData, ECSynchronizationThread **lppSynchronizationThread);

	/**
	 * Destructor
	 */
	~ECSynchronizationThread();

private:
	/**
	 * Main thread handler
	 *
	 * This function will run once the dedicated thread has started and will start the polling
	 * process of new tasks and handle those tasks accordingly.
	 *
	 * @param[in]   lpVoid
	 *                  Reference to ECIndexer object
	 * @return LPVOID
	 */
	static LPVOID RunThread(LPVOID lpVoid);

	/**
	 * Poll ECIndexerData for new tasks
	 *
	 * @param[out]	lppEntry
	 *					The returned task as retrieved from ECIndexerData.
	 * @return HRESULT
	 */
	HRESULT GetNextTask(ECEntryData **lppEntry);

	/**
	 * Continuously poll for ECIndexerData for new task and call Synchronize() for each task.
	 *
	 * @return HRESULT
	 */
	HRESULT RunSynchronizer();

	/**
	 * Perfom the synchronization for the given ECEntryData
	 *
	 * @param[in]	lpAdminSession
	 *					Reference to IMAPISession object for SYSTEM user.
	 * @param[in]	lpEntry
	 *					Reference to ECEntryData object which should be synchronized.
	 * @return HRESULT
	 */
	HRESULT Synchronize(IMAPISession *lpAdminSession, ECEntryData *lpEntry);

	/**
	 * Synchronize hierarchy changes received by ECIndexerSyncer and update the ECEntryData data
	 *
	 * @param[in]	lpEntry
	 *					Reference to ECEntryData object.
	 * @param[in]	lpSyncer
	 *					Reference to ECSynchronization object which owns the collection of changes
	 * @param[in]	lpMsgStore
	 *					Reference to IMsgStore object for the store which is indexed.
	 * @param[in]	lpRootFolder
	 *					Reference to IMAPIFolder object of the root folder of the indexed store.
	 * @return HRESULT
	 */
	HRESULT SyncHierarchy(ECEntryData *lpEntry, ECSynchronization *lpSyncer, IMsgStore *lpMsgStore, IMAPIFolder *lpRootFolder);

	/**
	 * Synchronize contents changes received by ECIndexerSyncer and update the lucene index
	 *
	 * @param[in]	lpEntry
	 *					Reference to ECEntryData object.
	 * @param[in]	lpSyncer
	 *					Reference to ECSynchronization object which owns the collection of changes
	 * @param[in]	lpLucene
	 *					Reference to the ECLuceneIndexer object which will perform the index operation
	 * @return HRESULT
	 */
	HRESULT SyncContents(ECEntryData *lpEntry, ECSynchronization *lpSyncer, ECLuceneIndexer *lpLucene);

	/**
	 * Detect properties required during the indexing process from the IMsgStore object.
	 *
	 * @param[in]	lpEntry
	 *					Reference to ECEntryData object.
	 * @param[in]	lpMsgStore
	 *					Reference to IMsgStore object for the store which is indexed.
	 * @return HRESULT
	 */
	HRESULT AddMsgStoreProps(ECEntryData *lpEntry, IMsgStore *lpMsgStore);

	/**
	 * Detect properties required during the indexing process from the IMAPIFolder object.
	 *
	 * @param[in]	lpEntry
	 *					Reference to ECEntryData object.
	 * @param[in]	lpFolder
	 *					Reference to IMAPIFolder object of the root folder of the indexed store.
	 * @return HRESULT
	 */
	HRESULT AddRootFolderProps(ECEntryData *lpEntry, IMAPIFolder *lpFolder);

private:
	ECThreadData *m_lpThreadData;
	ECIndexerData *m_lpIndexerData;

	/* Thread attribute */
	pthread_attr_t m_hThreadAttr;

	task_queue_t m_qTasks;
};

#endif /* ECSYNCHRONIZATIONTHREAD_H */
