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

#ifndef ECINDEXERDATA_H
#define ECINDEXERDATA_H

#include <list>
#include <map>
#include <queue>

#include "tstring.h"

class ECIndexer;
class ECSynchronizationThread;

typedef std::queue<ECEntryData *, entrydata_list_t> task_queue_t;
typedef std::map<tstring, task_queue_t> server_task_queue_t;
typedef std::list<ECSynchronizationThread *> syncthread_list_t;

/**
 * Communication layer between ECIndexer and ECSynchronizationThread
 *
 * This class manages the creation of ECSynchronizationThread objects,
 * as well as the distribution of all tasks over the given threads.
 */
class ECIndexerData {
public:
	/**
	 * Constructor
	 *
	 * @param[in]	lpIndexer
	 *					Reference to the ECIndexer object
	 * @param[in]	lTasks
	 *					list of all tasks to be distributed over the threads
	 */
	ECIndexerData(ECIndexer *lpIndexer, entrydata_list_t &lTasks);

	/**
	 * Destructor
	 */
	~ECIndexerData();

	/**
	 * Create a new ECSynchronizationThread object
	 *
	 * This will create a new ECSynchronizationThread object to start a new Indexer thread,
	 * the thread will be started immediately after creation and will be assigned work.
	 *
	 * @param[in]	lpThreadData
	 *					Reference to ECThreadData object needed for ECSynchronizationThread constructor
	 * @return HRESULT
	 */
	HRESULT CreateSyncThread(ECThreadData *lpThreadData);

	/**
	 * Get distribution factor
	 *
	 * The distribution factor indicates over how many threads the work can be distributed,
	 * this factor depends on the 'index_threads' configuration option and the number of
	 * detected zarafa-servers in the network.
	 *
	 * @param[out]	lpulFactor
	 *					The distribution factor
	 * @return HRESULT
	 */
	HRESULT GetDistributionFactor(ULONG *lpulFactor);

	/**
	 * Request new task for indexer thread
	 *
	 * When a indexer thread is done with its previously assign tasks it should use
	 * this function to request a new task. If no task is available an error will be
	 * returned and the thread should exit.
	 *
	 * @param[in]	lpSyncThread
	 *					Reference to ECSynchronizationThread object which is requesting the new task.
	 * @param[out]	lpTasks
	 *					The new task which should be performed by the indexing thread
	 * @return HRESULT
	 */
	HRESULT GetNextTaskQueue(ECSynchronizationThread *lpSyncThread, task_queue_t *lpTasks);

	/**
	 * Signal that a indexing thread is exiting
	 *
	 * When an ECSynchronizationThread is closing down the thread (and thus will no longer request
	 * any work from the ECIndexerData object) it should call this function to remove itself
	 * from the available thread list.
	 *
	 * @param[in]	lpSyncThread
	 *					Reference to ECSynchronizationThread object which is shutting down
	 * @return HRESULT
	 */
	HRESULT ThreadCompleted(ECSynchronizationThread *lpSyncThread);

	/**
	 * Check if all threads have completed their tasks
	 *
	 * @return BOOL
	 */
	BOOL AllThreadsCompleted();

	/**
	 * Load hierarchy changes and update the ECEntryData object.
	 *
	 * During the hierarchy synchronization new folders might have been detected,
	 * and other folders might have been deleted. Update the ECEntryData object
	 * with these changes.
	 *
	 * @param[in]	lpSyncer
	 *					Reference to ECSynchronization which contains all detected changes.
	 * @param[in]	lpEntry
	 *					Reference to ECEntryData object to which the folder must be attached.
	 * @param[in]	lpRootFolder
	 *					Root folder from where the the hierarchy is being synchronized
	 * @return HRESULT
	 */
	HRESULT UpdateHierarchy(ECSynchronization *lpSyncer, ECEntryData *lpEntry, IMAPIFolder *lpRootFolder);

	/**
	 * Update synchronization base for a particular folder to harddisk.
	 *
	 * After a folder has been synchronized, the updated synchronization base must be
	 * written to harddisk to make it possible to continue from this point after a restart.
	 *
	 * @param[in]	lpSyncer
	 *					Reference to ECSynchronization which contains all detected changes.
	 * @param[in]	lpEntry
	 *					Reference to ECEntryData object to which the folder belongs.
	 * @param[in]	lpMsgStore
	 *					Reference to IMsgStore object for the store which is being synchronized.
	 * @param[in]	lpAdminSession
	 * 					The admin MAPI session.
	 * @return HRESULT
	 */
	HRESULT UpdateContents(ECSynchronization *lpSyncer, ECEntryData *lpEntry, IMsgStore *lpMsgStore, IMAPISession *lpAdminSession);

	/**
	 * Optimize on-disk index
	 *
	 * @return result
	 */
	HRESULT OptimizeIndex(ECEntryData *lpEntry);

private:
	/**
	 * Distribute tasks over multiple servers
	 *
	 * Distribute all tasks to a map of lists. Each map key indicates the servername,
	 * where the corresponding list of tasks indicates the tasks belonging to this server.
	 * This is used to correctly distribute all tasks per server and ensures that no zarafa-server
	 * will be processed by multiple indexing threads concurrently.
	 *
	 * @param[in]	lTasks
	 *					List of all tasks which should be distributed.
	 * @param[out]	lpServerTasks
	 *					Map of all tasks distributed over the detected servers.
	 * @return VOID
	 */
	VOID DistributeTasks(entrydata_list_t &lTasks, server_task_queue_t *lpServerTasks);

private:
	ECIndexer *m_lpIndexer;

	pthread_mutexattr_t m_hLockAttr;

	pthread_mutex_t m_hSyncThreadsLock;
	pthread_cond_t m_hSyncThreadsCond;
	syncthread_list_t m_lSyncThreads;
	syncthread_list_t m_lSyncThreadsDone;

	pthread_mutex_t m_hTasksLock;
	server_task_queue_t m_mapServerTasks;
};

#endif /* ECINDEXERDATA_H */
