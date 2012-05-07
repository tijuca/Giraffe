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

#include <edkguid.h>

#include <CommonUtil.h>
#include <restrictionutil.h>
#include <stringutil.h>
#include <Util.h>

#include "ECFileIndex.h"
#include "ECIndexer.h"
#include "ECIndexerData.h"
#include "ECSynchronization.h"
#include "ECSynchronizationThread.h"
#include "zarafa-indexer.h"

ECSynchronizationThread::ECSynchronizationThread(ECThreadData *lpThreadData, ECIndexerData *lpIndexerData)
{
	m_lpThreadData = lpThreadData;
	m_lpIndexerData = lpIndexerData;

	pthread_attr_init(&m_hThreadAttr);
	pthread_attr_setdetachstate(&m_hThreadAttr, PTHREAD_CREATE_DETACHED);
}

ECSynchronizationThread::~ECSynchronizationThread()
{
	pthread_attr_destroy(&m_hThreadAttr);
}

HRESULT ECSynchronizationThread::Create(ECThreadData *lpThreadData, ECIndexerData *lpIndexerData, ECSynchronizationThread **lppSynchronizeThread)
{
	HRESULT hr = hrSuccess;
	ECSynchronizationThread *lpSynchronizeThread = NULL;
	pthread_t thread;

	try {
		lpSynchronizeThread = new ECSynchronizationThread(lpThreadData, lpIndexerData);
	}
	catch (...) {
		lpSynchronizeThread = NULL;
	}
	if (!lpSynchronizeThread) {
		hr = MAPI_E_NOT_ENOUGH_MEMORY;
		goto exit;
	}

	lpSynchronizeThread->AddRef();

	if (pthread_create(&thread, &lpSynchronizeThread->m_hThreadAttr, ECSynchronizationThread::RunThread, lpSynchronizeThread) != 0) {
		hr = MAPI_E_CALL_FAILED;
		goto exit;
	}

	if (lppSynchronizeThread)
		*lppSynchronizeThread = lpSynchronizeThread;

exit:
	if (hr != hrSuccess && lpSynchronizeThread)
		lpSynchronizeThread->Release();

	return hr;
}

LPVOID ECSynchronizationThread::RunThread(LPVOID lpVoid)
{
	HRESULT hr = hrSuccess;
	ECSynchronizationThread *lpSyncThread = (ECSynchronizationThread *)lpVoid;

	if (lpSyncThread->m_lpThreadData->bShutdown)
		return NULL;

	hr = MAPIInitialize(NULL);
	if (hr != hrSuccess) {
		lpSyncThread->m_lpThreadData->lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to initialize");
		goto exit;
	}

	hr = lpSyncThread->RunSynchronizer();
	if (hr != hrSuccess)
		goto exit;

exit:
	MAPIUninitialize();

	return NULL;
}

HRESULT ECSynchronizationThread::GetNextTask(ECEntryData **lppEntry)
{
	HRESULT hr = hrSuccess;
	ECEntryData *lpTask = NULL;

	if (m_qTasks.empty()) {
		hr = m_lpIndexerData->GetNextTaskQueue(this, &m_qTasks);
		if (hr != hrSuccess)
			goto exit;

		m_lpThreadData->lpLogger->Log(EC_LOGLEVEL_WARNING, "Assigned server '%ls' (%lu tasks) to synchronization thread",
									  m_qTasks.front()->m_strServerName.c_str(), m_qTasks.size());
	}

	lpTask = m_qTasks.front();
	m_qTasks.pop();

	lpTask->AddRef();
	*lppEntry = lpTask;

exit:
	return hr;
}

HRESULT ECSynchronizationThread::RunSynchronizer()
{
	HRESULT hr = hrSuccess;
	IMAPISession *lpAdminSession = NULL;
	ECEntryData *lpEntry = NULL;

	hr = HrOpenECAdminSession(&lpAdminSession,
							  m_lpThreadData->lpConfig->GetSetting("server_socket"),
							  EC_PROFILE_FLAGS_NO_NOTIFICATIONS,
							  m_lpThreadData->lpConfig->GetSetting("sslkey_file", "", NULL),
							  m_lpThreadData->lpConfig->GetSetting("sslkey_pass", "", NULL));
	if (hr != hrSuccess)
		goto exit;

	while (!m_lpThreadData->bShutdown) {
		hr = GetNextTask(&lpEntry);
		if (hr != hrSuccess) {
			if (hr == MAPI_E_CANCEL)
				break;
			continue;
		}

		// no need to check error, because we want to process the complete task list
		// details should already have been logged
		Synchronize(lpAdminSession, lpEntry);

		if (lpEntry)
			lpEntry->Release();
		lpEntry = NULL;
	}

exit:
	if (lpEntry)
		lpEntry->Release();

	if (lpAdminSession)
		lpAdminSession->Release();

	m_lpIndexerData->ThreadCompleted(this);

	return hr;
}

HRESULT ECSynchronizationThread::AddMsgStoreProps(ECEntryData *lpEntry, IMsgStore *lpMsgStore)
{
	HRESULT hr = hrSuccess;
	SPropValue sUnWrapStoreEntryId = { 0, 0 };
	LPSPropValue lpRootEntryId = NULL;
	LPSPropValue lpMappingSignature = NULL;
	LPSPropValue lpStoreProps = NULL;
	ULONG ulStoreProps = 0;

	std::string strUnwrapStoreEntryId;
	std::string strMappingSignature;

	LPSPropTagArray lpMsgStoreProps = NULL;
	SizedSPropTagArray(2, sMsgStoreProps) = { 2, { PR_MAPPING_SIGNATURE, PR_IPM_SUBTREE_ENTRYID } };
	SizedSPropTagArray(2, sPublicStoreProps) = { 2, { PR_MAPPING_SIGNATURE, PR_IPM_PUBLIC_FOLDERS_ENTRYID } };

	if (lpEntry->m_bPublicFolder)
		lpMsgStoreProps = (LPSPropTagArray)&sPublicStoreProps;
	else
		lpMsgStoreProps = (LPSPropTagArray)&sMsgStoreProps;

	hr = lpMsgStore->GetProps(lpMsgStoreProps, 0, &ulStoreProps, &lpStoreProps);
	if (hr != hrSuccess)
		goto exit;

	lpRootEntryId = PpropFindProp(lpStoreProps, ulStoreProps, lpMsgStoreProps->aulPropTag[1]);
	if (!lpRootEntryId) {
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}

	hr = HrAddEntryData(&lpEntry->m_sRootEntryId, lpEntry, &lpRootEntryId->Value.bin);
	if (hr != hrSuccess)
		goto exit;

	hr = UnWrapStoreEntryID(lpEntry->m_sStoreEntryId.cb, (LPENTRYID)lpEntry->m_sStoreEntryId.lpb,
							&sUnWrapStoreEntryId.Value.bin.cb, (LPENTRYID *)&sUnWrapStoreEntryId.Value.bin.lpb);
	if (hr != hrSuccess)
		goto exit;

	lpMappingSignature = PpropFindProp(lpStoreProps, ulStoreProps, lpMsgStoreProps->aulPropTag[0]);
	if (!lpMappingSignature) {
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}

	strUnwrapStoreEntryId = bin2hex(sUnWrapStoreEntryId.Value.bin.cb, sUnWrapStoreEntryId.Value.bin.lpb);
	strMappingSignature = bin2hex(lpMappingSignature->Value.bin.cb, lpMappingSignature->Value.bin.lpb);
	hr = m_lpThreadData->lpFileIndex->GetStorePath(strMappingSignature, strUnwrapStoreEntryId, &lpEntry->m_strStorePath);
	if (hr != hrSuccess)
		goto exit;

exit:
	if (sUnWrapStoreEntryId.Value.bin.lpb)
		MAPIFreeBuffer(sUnWrapStoreEntryId.Value.bin.lpb);

	if (lpStoreProps)
		MAPIFreeBuffer(lpStoreProps);

	return hr;
}

HRESULT ECSynchronizationThread::AddRootFolderProps(ECEntryData *lpEntry, IMAPIFolder *lpFolder)
{
	HRESULT hr = hrSuccess;
	LPSPropValue lpSourceKey = NULL;
	LPSPropValue lpFolderProps = NULL;
	ULONG ulFolderProps = 0;

	SizedSPropTagArray(1, sFolderProps) = { 1, { PR_SOURCE_KEY } };

	hr = lpFolder->GetProps((LPSPropTagArray)&sFolderProps, 0, &ulFolderProps, &lpFolderProps);
	if (hr != hrSuccess)
		goto exit;

	lpSourceKey = PpropFindProp(lpFolderProps, ulFolderProps, PR_SOURCE_KEY);
	if (!lpSourceKey) {
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}

	hr = HrAddEntryData(&lpEntry->m_sRootSourceKey, lpEntry, &lpSourceKey->Value.bin);
	if (hr != hrSuccess)
		goto exit;

exit:
	if (lpFolderProps)
		MAPIFreeBuffer(lpFolderProps);

	return hr;
}

HRESULT ECSynchronizationThread::Synchronize(IMAPISession *lpAdminSession, ECEntryData *lpEntry)
{
	HRESULT hr = hrSuccess;
	IMsgStore *lpMsgStore = NULL;
	IMAPIFolder *lpRootFolder = NULL;
	ULONG ulObjType = 0;
	ECSynchronization *lpSyncer = NULL;

	m_lpThreadData->lpLogger->Log(EC_LOGLEVEL_INFO, "Indexing of store '%ls' started",
								  lpEntry->m_strUserName.c_str());

	hr = ECSynchronization::Create(m_lpThreadData, m_lpIndexerData, &lpSyncer);
	if (hr != hrSuccess)
		goto exit;

	hr = lpAdminSession->OpenMsgStore(0, lpEntry->m_sStoreEntryId.cb, (LPENTRYID)lpEntry->m_sStoreEntryId.lpb,
									  &IID_IMsgStore, MDB_WRITE | MDB_TEMPORARY | MDB_NO_DIALOG, &lpMsgStore);
	if (hr != hrSuccess) {
		m_lpThreadData->lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to open store, error 0x%08X", hr);
		goto exit;
	}

	if (!lpEntry->m_sRootEntryId.lpb) {
		hr = AddMsgStoreProps(lpEntry, lpMsgStore);
		if (hr != hrSuccess)
			goto exit;
	}


	hr = lpMsgStore->OpenEntry(lpEntry->m_sRootEntryId.cb, (LPENTRYID)lpEntry->m_sRootEntryId.lpb,
							   &IID_IMAPIFolder, 0, &ulObjType, (LPUNKNOWN *)&lpRootFolder);
	if (hr != hrSuccess) {
		m_lpThreadData->lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to open root folder");
		goto exit;
	}

	/* Check if we should exit now */
	if (m_lpThreadData->bShutdown)
		goto exit;

	/* Check if Sourcekey for the root folder is already known, or if we still nee to check it */
	if (!lpEntry->m_sRootSourceKey.lpb) {
		hr = AddRootFolderProps(lpEntry, lpRootFolder);
		if (hr != hrSuccess)
			goto exit;
	}

	/* Check if we should exit now */
	if (m_lpThreadData->bShutdown)
		goto exit;

	hr = lpSyncer->GetHierarchyChanges(lpEntry, lpMsgStore, lpRootFolder);
	if (hr != hrSuccess) {
		m_lpThreadData->lpLogger->Log(EC_LOGLEVEL_ERROR, "Indexing of store '%ls' failed while synchronizing hierarchy, error 0x%08X",
									  lpEntry->m_strUserName.c_str(), hr);
		goto exit;
	}

	/* Check if we should exit now */
	if (m_lpThreadData->bShutdown)
		goto exit;

	hr = lpSyncer->GetContentsChanges(lpEntry, lpMsgStore, lpRootFolder, lpAdminSession);
	if (hr != hrSuccess) {
		m_lpThreadData->lpLogger->Log(EC_LOGLEVEL_ERROR, "Indexing of store '%ls' failed while synchronizing contents, error 0x%08X",
									  lpEntry->m_strUserName.c_str(), hr);
		goto exit;
	}

	m_lpThreadData->lpLogger->Log(EC_LOGLEVEL_INFO, "Indexing of store '%ls' succeeded", lpEntry->m_strUserName.c_str());

exit:
	if (lpSyncer)
		lpSyncer->Release();

	if (lpRootFolder)
		lpRootFolder->Release();

	if (lpMsgStore)
		lpMsgStore->Release();

	return hr;
}
