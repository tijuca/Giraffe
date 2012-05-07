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

#include <algorithm>

#include <mapi.h>
#include <mapix.h>
#include <mapiutil.h>
#include <mapidefs.h>
#include <mapiguid.h>

#include "ECIndexer.h"
#include "ECIndexerData.h"
#include "ECSynchronizationThread.h"

ECIndexerData::ECIndexerData(ECIndexer *lpIndexer, entrydata_list_t &lTasks)
{
	m_lpIndexer = lpIndexer;
	DistributeTasks(lTasks, &m_mapServerTasks);

	pthread_mutexattr_init(&m_hLockAttr);
	pthread_mutexattr_settype(&m_hLockAttr, PTHREAD_MUTEX_RECURSIVE);

	pthread_mutex_init(&m_hSyncThreadsLock, &m_hLockAttr);
	pthread_cond_init(&m_hSyncThreadsCond, NULL);

	pthread_mutex_init(&m_hTasksLock, &m_hLockAttr);
}

ECIndexerData::~ECIndexerData()
{
	syncthread_list_t::iterator iter;

	/* Clear all pending tasks */
	pthread_mutex_lock(&m_hTasksLock);
	m_mapServerTasks.clear();
	pthread_mutex_unlock(&m_hTasksLock);

	/* Wait until all threads have completed their tasks, afterwards delete threads */
	pthread_mutex_lock(&m_hSyncThreadsLock);
	while (!AllThreadsCompleted())
		pthread_cond_wait(&m_hSyncThreadsCond, &m_hSyncThreadsLock);

	for (iter = m_lSyncThreadsDone.begin(); iter != m_lSyncThreadsDone.end(); iter++)
		(*iter)->Release();
	pthread_mutex_unlock(&m_hSyncThreadsLock);

	pthread_mutex_destroy(&m_hSyncThreadsLock);
	pthread_mutex_destroy(&m_hTasksLock);
	pthread_mutexattr_destroy(&m_hLockAttr);
}

VOID ECIndexerData::DistributeTasks(entrydata_list_t &lTasks, server_task_queue_t *lpServerTasks)
{
	entrydata_list_t::iterator iter;

	for (iter = lTasks.begin(); iter != lTasks.end(); iter++)
		(*lpServerTasks)[((*iter)->m_strServerName)].push(*iter);
}

HRESULT ECIndexerData::CreateSyncThread(ECThreadData *lpThreadData)
{
	HRESULT hr = hrSuccess;
	ECSynchronizationThread *lpSyncThread = NULL;

	pthread_mutex_lock(&m_hSyncThreadsLock);

	hr = ECSynchronizationThread::Create(lpThreadData, this, &lpSyncThread);
	if (hr != hrSuccess)
		goto exit;

	m_lSyncThreads.push_back(lpSyncThread);

exit:
	pthread_mutex_unlock(&m_hSyncThreadsLock);

	return hr;
}

HRESULT ECIndexerData::GetDistributionFactor(ULONG *lpulFactor)
{
	HRESULT hr = hrSuccess;

	if (lpulFactor)
		*lpulFactor = m_mapServerTasks.size();
	else
		hr = MAPI_E_INVALID_PARAMETER;

	return hr; 
}

HRESULT ECIndexerData::GetNextTaskQueue(ECSynchronizationThread *lpSyncThread, task_queue_t *lpTasks)
{
	HRESULT hr = hrSuccess;
	syncthread_list_t::iterator iter;

	if (!lpSyncThread || !lpTasks) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	pthread_mutex_lock(&m_hSyncThreadsLock);

	iter = find(m_lSyncThreads.begin(), m_lSyncThreads.end(), lpSyncThread);
	if (iter == m_lSyncThreads.end()) {
		/* Thread was not found */
		hr = MAPI_E_CANCEL;
	} else if (m_mapServerTasks.empty()) {
		/* All tasks have been completed */
		hr = MAPI_E_CANCEL;
	} else {
		*lpTasks = m_mapServerTasks.begin()->second;
		m_mapServerTasks.erase(m_mapServerTasks.begin());
	}

	pthread_mutex_unlock(&m_hSyncThreadsLock);

exit:
	return hr;
}

HRESULT ECIndexerData::ThreadCompleted(ECSynchronizationThread *lpSyncThread)
{
	HRESULT hr = hrSuccess;
	syncthread_list_t::iterator iter;

	if (!lpSyncThread) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	pthread_mutex_lock(&m_hSyncThreadsLock);

	iter = find(m_lSyncThreads.begin(), m_lSyncThreads.end(), lpSyncThread);
	if (iter != m_lSyncThreads.end()) {
		m_lSyncThreads.erase(iter); 
		m_lSyncThreadsDone.push_back(lpSyncThread); 
		pthread_cond_broadcast(&m_hSyncThreadsCond); 
	 	 
		if (AllThreadsCompleted()) 
			m_lpIndexer->RunSynchronizationDone();
	}

	pthread_mutex_unlock(&m_hSyncThreadsLock);

exit:
	return hr;
}

BOOL ECIndexerData::AllThreadsCompleted()
{
	return m_lSyncThreads.empty();
}

HRESULT ECIndexerData::UpdateHierarchy(ECSynchronization *lpSyncer, ECEntryData *lpEntry, IMAPIFolder *lpRootFolder)
{
	HRESULT hr = hrSuccess;

	if (!lpSyncer || !lpEntry || !lpRootFolder) {
		hr = MAPI_E_CALL_FAILED;
		goto exit;
	}

	hr = m_lpIndexer->LoadUserFolderChanges(lpSyncer, lpEntry, lpRootFolder);
	if (hr != hrSuccess)
		goto exit;

exit:
	return hr;
}

HRESULT ECIndexerData::UpdateContents(ECSynchronization *lpSyncer, ECEntryData *lpEntry, IMsgStore *lpMsgStore, IMAPISession *lpAdminSession)
{
	HRESULT hr = hrSuccess;

	if (!lpSyncer || !lpEntry || !lpMsgStore) {
		hr = MAPI_E_CALL_FAILED;
		goto exit;
	}

	hr = m_lpIndexer->LoadUserFolderContentChanges(lpSyncer, lpEntry, lpMsgStore, lpAdminSession);
	if (hr != hrSuccess)
		goto exit;

exit:
	return hr;
}

HRESULT ECIndexerData::OptimizeIndex(ECEntryData *lpEntry)
{
	return m_lpIndexer->OptimizeIndex(lpEntry);
}