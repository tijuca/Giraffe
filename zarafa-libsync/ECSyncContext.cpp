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

#include "ECSyncContext.h"
#include "ECOfflineABImporter.h"
#include "ECSyncUtil.h"
#include "ECSyncSettings.h"
#include "threadutil.h"

#include <IECExportAddressbookChanges.h>
#include <IECExportChanges.h>
#include <IECChangeAdvisor.h>

#include <ECUnknown.h>
#include <ECGuid.h>
#include <ECTags.h>
#include <ECLogger.h>
#include <stringutil.h>

#include <mapix.h>
#include <mapiext.h>
#include <mapiutil.h>
#include <edkguid.h>
#include <edkmdb.h>

#include <mapi_ptr.h>
DEFINEMAPIPTR(ECChangeAdvisor);
DEFINEMAPIPTR(ECChangeAdviseSink);

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define EC_SYNC_STATUS_VERSION			1


#define CALL_MEMBER_FN(object,ptrToMember)  ((object).*(ptrToMember))
class ECChangeAdviseSink : public ECUnknown
{
public:
	typedef ULONG(ECSyncContext::*NOTIFYCALLBACK)(ULONG,LPENTRYLIST);

	// Constructor
	ECChangeAdviseSink(ECSyncContext *lpsSyncContext, NOTIFYCALLBACK fnCallback)
		: m_lpsSyncContext(lpsSyncContext)
		, m_fnCallback(fnCallback)
	{ }

	// IUnknown
	HRESULT QueryInterface(REFIID refiid, void **lpvoid) {
		if (refiid == IID_ECUnknown || refiid == IID_ECChangeAdviseSink) {
			AddRef();
			*lpvoid = (void *)this;
			return hrSuccess;
		}
		if (refiid == IID_IUnknown || refiid == IID_IECChangeAdviseSink) {
			AddRef();
			*lpvoid = (void *)&this->m_xECChangeAdviseSink;
			return hrSuccess;
		}

		return MAPI_E_INTERFACE_NOT_SUPPORTED;
	}

	// IExchangeChangeAdviseSink
	ULONG OnNotify(ULONG ulFlags, LPENTRYLIST lpEntryList) { 
		return CALL_MEMBER_FN(*m_lpsSyncContext, m_fnCallback)(ulFlags, lpEntryList); 
	}

private:
	class xECChangeAdviseSink : public IECChangeAdviseSink {
	public:
		// IUnknown
		virtual ULONG __stdcall AddRef() {
			METHOD_PROLOGUE_(ECChangeAdviseSink, ECChangeAdviseSink);
			return pThis->AddRef();
		}

		virtual ULONG __stdcall Release() {
			METHOD_PROLOGUE_(ECChangeAdviseSink, ECChangeAdviseSink);
			return pThis->Release();
		}

		virtual HRESULT __stdcall QueryInterface(REFIID refiid, void **pInterface) {
			METHOD_PROLOGUE_(ECChangeAdviseSink, ECChangeAdviseSink);
			return pThis->QueryInterface(refiid, pInterface);
		}

		// IExchangeChangeAdviseSink
		virtual ULONG __stdcall OnNotify(ULONG ulFlags, LPENTRYLIST lpEntryList) {
			METHOD_PROLOGUE_(ECChangeAdviseSink, ECChangeAdviseSink);
			return pThis->OnNotify(ulFlags, lpEntryList);
		}
	} m_xECChangeAdviseSink;

private:
	ECSyncContext	*m_lpsSyncContext;
	NOTIFYCALLBACK	m_fnCallback;
};

HRESULT HrCreateECChangeAdviseSink(ECSyncContext *lpsSyncContext, ECChangeAdviseSink::NOTIFYCALLBACK fnCallback, LPECCHANGEADVISESINK *lppAdviseSink)
{
	HRESULT				hr = hrSuccess;
	ECChangeAdviseSink	*lpAdviseSink = NULL;

	// we cannot use nothrow new as it clashes with MFC's DEBUG_NEW
	try { lpAdviseSink = new ECChangeAdviseSink(lpsSyncContext, fnCallback); }
	catch (std::bad_alloc &) {
		hr = MAPI_E_NOT_ENOUGH_MEMORY;
		goto exit;
	}

	hr = lpAdviseSink->QueryInterface(IID_IECChangeAdviseSink, (void**)lppAdviseSink);
	if (hr == hrSuccess)
		lpAdviseSink = NULL;

exit:
	if (lpAdviseSink)
		lpAdviseSink->Release();

	return hr;
}



ECSyncContext::ECSyncContext(LPMDB lpStore, ECLogger *lpLogger)
	: m_lpStore(lpStore)
	, m_lpLogger(lpLogger)
	, m_lpSettings(ECSyncSettings::GetInstance())
	, m_lpChangeAdvisor(NULL)
	, m_lpChangeAdviseSink(NULL)
{
	pthread_mutex_init(&m_hMutex, NULL);

	m_lpStore->AddRef();

	if (m_lpSettings->ChangeNotificationsEnabled())
		HrCreateECChangeAdviseSink(this, &ECSyncContext::OnChange, &m_lpChangeAdviseSink);
}

ECSyncContext::~ECSyncContext()
{
	if (m_lpChangeAdvisor)
		m_lpChangeAdvisor->Release();

	if (m_lpChangeAdviseSink)
		m_lpChangeAdviseSink->Release();

	if (m_lpStore)
		m_lpStore->Release();

	pthread_mutex_destroy(&m_hMutex);
}


HRESULT ECSyncContext::HrGetMsgStore(LPMDB *lppMsgStore)
{
	HRESULT hr = hrSuccess;

	if (!lppMsgStore) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	if (m_lpStore == NULL) {
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}

	hr = m_lpStore->QueryInterface(IID_IMsgStore, (void**)lppMsgStore);

exit:
	return hr;
}


HRESULT ECSyncContext::HrGetReceiveFolder(LPMAPIFOLDER *lppInboxFolder)
{
	HRESULT			hr = hrSuccess;
	ULONG			cbEntryID = 0;
	LPENTRYID		lpEntryID = NULL;
	ULONG			ulObjType = 0;
	LPMAPIFOLDER	lpInboxFolder = NULL;

	hr = m_lpStore->GetReceiveFolder((LPTSTR)"IPM", 0, &cbEntryID, &lpEntryID, NULL);
	if (hr != hrSuccess)
		goto exit;

	hr = m_lpStore->OpenEntry(cbEntryID, lpEntryID, &IID_IMAPIFolder, MAPI_MODIFY, &ulObjType, (LPUNKNOWN*)&lpInboxFolder);
	if (hr != hrSuccess)
		goto exit;

	hr = lpInboxFolder->QueryInterface(IID_IMAPIFolder, (void**)lppInboxFolder);

exit:
	if (lpInboxFolder)
		lpInboxFolder->Release();

	if (lpEntryID)
		MAPIFreeBuffer(lpEntryID);

	return hr;
}


HRESULT ECSyncContext::HrGetChangeAdvisor(LPECCHANGEADVISOR *lppChangeAdvisor)
{
	HRESULT	hr = hrSuccess;

	pthread_mutex_lock(&m_hMutex);

	if (!m_lpSettings->ChangeNotificationsEnabled())
		hr = MAPI_E_NO_SUPPORT;

	else if (!m_lpChangeAdvisor)
		hr = m_lpStore->OpenProperty(PR_EC_CHANGE_ADVISOR, &IID_IECChangeAdvisor, 0, 0, (LPUNKNOWN*)&m_lpChangeAdvisor);
		// Check for error after releasing lock

	pthread_mutex_unlock(&m_hMutex);

	if (hr != hrSuccess)
		goto exit;

	hr = m_lpChangeAdvisor->QueryInterface(IID_IECChangeAdvisor, (void**)lppChangeAdvisor);
	if (hr != hrSuccess)
		goto exit;

exit:
	return hr;
}

HRESULT ECSyncContext::HrReleaseChangeAdvisor()
{
	HRESULT hr = hrSuccess;
	ECChangeAdvisorPtr ptrReleaseMe;

	// WARNING:
	// This must come after the declaration of ptrReleaseMe to
	// ensure the mutex is unlocked before the change advisor
	// is released.
	scoped_lock lock(m_hMutex);

	if (!m_lpSettings->ChangeNotificationsEnabled()) {
		hr = MAPI_E_NO_SUPPORT;
		goto exit;
	}

	if (m_lpChangeAdvisor) {
		// Don't release while holding the lock as that might
		// cause a deadlock if a notification is being delivered.
		ptrReleaseMe.reset(m_lpChangeAdvisor);
		m_lpChangeAdvisor = NULL;
	}

	m_mapNotifiedSyncIds.clear();

exit:
	return hr;
}

HRESULT ECSyncContext::HrResetChangeAdvisor()
{
	HRESULT hr = hrSuccess;
	ECChangeAdvisorPtr ptrChangeAdvisor;
	ECChangeAdviseSinkPtr ptrChangeAdviseSink;

	hr = HrReleaseChangeAdvisor();
	if (hr != hrSuccess)
		goto exit;

	hr = HrGetChangeAdvisor(&ptrChangeAdvisor);
	if (hr != hrSuccess)
		goto exit;

	hr = HrGetChangeAdviseSink(&ptrChangeAdviseSink);
	if (hr != hrSuccess)
		goto exit;

	hr = ptrChangeAdvisor->Config(NULL, NULL, ptrChangeAdviseSink, 0);
	if (hr != hrSuccess)
		goto exit;

exit:
	return hr;
}


HRESULT ECSyncContext::HrGetChangeAdviseSink(LPECCHANGEADVISESINK *lppChangeAdviseSink)
{
	ASSERT(m_lpChangeAdviseSink != NULL);
	return m_lpChangeAdviseSink->QueryInterface(IID_IECChangeAdviseSink, (void**)lppChangeAdviseSink);
}


HRESULT ECSyncContext::HrQueryHierarchyTable(LPSPropTagArray lpsPropTags, LPSRowSet *lppRows)
{
	HRESULT			hr = hrSuccess;
	LPMAPIFOLDER	lpRootFolder = NULL;
	ULONG			ulType = 0;
	LPMAPITABLE		lpTable = NULL;

	ASSERT(lppRows != NULL);

	hr = m_lpStore->OpenEntry(0, NULL, &IID_IMAPIFolder, MAPI_DEFERRED_ERRORS, &ulType, (LPUNKNOWN*)&lpRootFolder);
	if (hr != hrSuccess)
		goto exit;

	hr = lpRootFolder->GetHierarchyTable(CONVENIENT_DEPTH, &lpTable);
	if (hr != hrSuccess)
		goto exit;

	hr = HrQueryAllRows(lpTable, lpsPropTags, NULL, NULL, 0, lppRows);
	if(hr != hrSuccess)
		goto exit;

exit:
	if (lpTable)
		lpTable->Release();

	if (lpRootFolder)
		lpRootFolder->Release();

	return hr;
}


HRESULT ECSyncContext::HrOpenRootFolder(LPMAPIFOLDER *lppRootFolder, LPMDB *lppMsgStore)
{
	HRESULT			hr = hrSuccess;
	LPMAPIFOLDER	lpRootFolder = NULL;
	SBinary			sEntryID = {0};

	ASSERT(lppRootFolder != NULL);

	hr = HrOpenFolder(&sEntryID, &lpRootFolder);
	if (hr != hrSuccess)
		goto exit;

	if (lppMsgStore) {
		hr = HrGetMsgStore(lppMsgStore);
		if (hr != hrSuccess)
			goto exit;
	}

	*lppRootFolder = lpRootFolder;
	lpRootFolder = NULL;

exit:
	if (lpRootFolder)
		lpRootFolder->Release();

	return hr;
}


HRESULT ECSyncContext::HrOpenFolder(SBinary *lpsEntryID, LPMAPIFOLDER *lppFolder)
{
	HRESULT			hr = hrSuccess;
	LPMAPIFOLDER	lpFolder = NULL;
	ULONG			ulType = 0;

	ASSERT(lpsEntryID != NULL);
	ASSERT(lppFolder != NULL);

	hr = m_lpStore->OpenEntry(lpsEntryID->cb, (LPENTRYID)lpsEntryID->lpb, &IID_IMAPIFolder, MAPI_DEFERRED_ERRORS|MAPI_MODIFY, &ulType, (LPUNKNOWN*)&lpFolder);
	if (hr != hrSuccess)
		goto exit;

	*lppFolder = lpFolder;
	lpFolder = NULL;

exit:
	if (lpFolder)
		lpFolder->Release();

	return hr;
}


HRESULT ECSyncContext::HrNotifyNewMail(LPNOTIFICATION lpNotification)
{
	return m_lpStore->NotifyNewMail(lpNotification);
}


HRESULT ECSyncContext::HrGetSteps(SBinary *lpEntryID, SBinary *lpSourceKey, ULONG ulSyncFlags, ULONG *lpulSteps)
{
	HRESULT hr = hrSuccess;
	IMAPIFolder *lpFolder = NULL;
	LPSPropValue lpPropVal = NULL;
	LPSTREAM lpStream = NULL;
	IExchangeExportChanges *lpIEEC = NULL;
	IECExportChanges *lpECEC = NULL;
	ULONG ulChangeCount = 0;
	ULONG ulChangeId = 0;
	ULONG ulType = 0;
	SSyncState sSyncState = {0};
	IECChangeAdvisor *lpECA = NULL;
	NotifiedSyncIdMap::iterator iterNotifiedSyncId;
	bool bNotified = false;

	ASSERT(lpulSteps != NULL);

	// First see if the changeadvisor is monitoring the requested folder.
	if (m_lpChangeAdvisor == NULL)
		goto fallback;

	hr = HrGetSyncStateFromSourceKey(lpSourceKey, &sSyncState);
	if (hr == MAPI_E_NOT_FOUND)
		goto fallback;
	else if (hr != hrSuccess)
		goto exit;

	hr = m_lpChangeAdvisor->QueryInterface(IID_IECChangeAdvisor, (void**)&lpECA);
	if (hr == MAPI_E_INTERFACE_NOT_SUPPORTED)
		goto fallback;
	else if (hr != hrSuccess)
		goto exit;

	hr = lpECA->IsMonitoringSyncId(sSyncState.ulSyncId);
	if (hr == hrSuccess) {
		pthread_mutex_lock(&m_hMutex);

		iterNotifiedSyncId = m_mapNotifiedSyncIds.find(sSyncState.ulSyncId);
		if (iterNotifiedSyncId == m_mapNotifiedSyncIds.end()) {
			*lpulSteps = 0;
			m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "GetSteps: sourcekey=%s, syncid=%u, notified=yes, steps=0 (unsignalled)", bin2hex(lpSourceKey->cb, lpSourceKey->lpb).c_str(), sSyncState.ulSyncId);

			pthread_mutex_unlock(&m_hMutex);
			goto exit;
		}

		ulChangeId = iterNotifiedSyncId->second;	// Remember for later.
		bNotified = true;

		pthread_mutex_unlock(&m_hMutex);
	} else if (hr == MAPI_E_NOT_FOUND) {
		SBinary	sEntry = { sizeof(sSyncState), (LPBYTE)&sSyncState };
		SBinaryArray sEntryList = { 1, &sEntry };

		hr = m_lpChangeAdvisor->AddKeys(&sEntryList);
		if (hr != hrSuccess)
			goto exit;
	} else
		goto exit;

fallback:
	// The current folder is not being monitored, so get steps the old fashioned way.
	hr = m_lpStore->OpenEntry(lpEntryID->cb, (LPENTRYID)lpEntryID->lpb, 0, MAPI_DEFERRED_ERRORS, &ulType, (IUnknown **)&lpFolder);
	if (hr != hrSuccess)
		goto exit;

	hr = HrGetSyncStatusStream(lpSourceKey, &lpStream);
	if (FAILED(hr))
		goto exit;

	hr = lpFolder->OpenProperty(PR_CONTENTS_SYNCHRONIZER, &IID_IExchangeExportChanges, 0, 0, (LPUNKNOWN *)&lpIEEC);
	if (hr != hrSuccess)
		goto exit;

	hr = lpIEEC->Config(lpStream, SYNC_CATCHUP | ulSyncFlags, NULL, NULL, NULL, NULL, 1);
	if (hr != hrSuccess)
		goto exit;

	hr = lpIEEC->QueryInterface(IID_IECExportChanges, (void **)&lpECEC);
	if (hr != hrSuccess)
		goto exit;

	hr = lpECEC->GetChangeCount(&ulChangeCount);
	if (hr != hrSuccess)
		goto exit;

	// If the change notification system was signalled for this syncid, but the server returns no results, we need
	// to remove it from the list. However there could have been a change in the mean time, so we need to check if
	// m_mapNotifiedSyncIds was updated for this syncid.
	if (bNotified && ulChangeCount == 0) {
		pthread_mutex_lock(&m_hMutex);

		if (m_mapNotifiedSyncIds[sSyncState.ulSyncId] <= ulChangeId)
			m_mapNotifiedSyncIds.erase(sSyncState.ulSyncId);

		pthread_mutex_unlock(&m_hMutex);
	}

	*lpulSteps = ulChangeCount;
	m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "GetSteps: sourcekey=%s, syncid=%u, notified=%s, steps=%u", bin2hex(lpSourceKey->cb, lpSourceKey->lpb).c_str(), sSyncState.ulSyncId, (bNotified ? "yes" : "no"), *lpulSteps);

exit:
	if (lpECA)
		lpECA->Release();

	if (lpECEC)
		lpECEC->Release();

	if (lpIEEC)
		lpIEEC->Release();

	if (lpStream)
		lpStream->Release();

	if (lpPropVal)
		MAPIFreeBuffer(lpPropVal);

	if (lpFolder)
		lpFolder->Release();

	return hr;
}


HRESULT ECSyncContext::HrUpdateChangeId(LPSTREAM lpStream)
{
	HRESULT		hr = hrSuccess;
	syncid_t	ulSyncId = 0;
	changeid_t	ulChangeId = 0;
	ECChangeAdvisorPtr	ptrECA;

	ASSERT(lpStream != NULL);

	hr = HrDecodeSyncStateStream(lpStream, &ulSyncId, &ulChangeId);
	if (hr != hrSuccess)
		goto exit;


	pthread_mutex_lock(&m_hMutex);

	if (m_mapNotifiedSyncIds[ulSyncId] <= ulChangeId)
		m_mapNotifiedSyncIds.erase(ulSyncId);

	pthread_mutex_unlock(&m_hMutex);

	if(m_lpChangeAdvisor) {
		// Now inform the change advisor of our accomplishment
		hr = m_lpChangeAdvisor->QueryInterface(ptrECA.iid, &ptrECA);
		if (hr == MAPI_E_INTERFACE_NOT_SUPPORTED)
			goto exit;

		hr = ptrECA->UpdateSyncState(ulSyncId, ulChangeId);
		if (hr == MAPI_E_INVALID_PARAMETER) {
			// We're apparently not tracking this syncid.
			hr = hrSuccess;
		}
	}
	
exit:
	return hr;
}


HRESULT ECSyncContext::HrGetSyncStateFromSourceKey(SBinary *lpSourceKey, SSyncState *lpsSyncState)
{
	HRESULT							hr = hrSuccess;
	std::string						strSourceKey((char*)lpSourceKey->lpb, lpSourceKey->cb);
	SyncStateMap::const_iterator	iterSyncState;
	LPSTREAM						lpStream = NULL;
	SSyncState						sSyncState = {0};

	// First check the sourcekey to syncid map.
	iterSyncState = m_mapStates.find(strSourceKey);
	if (iterSyncState != m_mapStates.end()) {
		ASSERT(iterSyncState->second.ulSyncId != 0);
		*lpsSyncState = iterSyncState->second;
		goto exit;
	}


	// Try to get the information from the status stream.
	hr = HrGetSyncStatusStream(lpSourceKey, &lpStream);
	if (FAILED(hr))
		goto exit;

	hr = HrDecodeSyncStateStream(lpStream, &sSyncState.ulSyncId, &sSyncState.ulChangeId, NULL);
	if (hr != hrSuccess)
		goto exit;

	if (sSyncState.ulSyncId == 0) {
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}

	// update the sourcekey to syncid map.
	m_mapStates.insert(SyncStateMap::value_type(strSourceKey, sSyncState));
	*lpsSyncState = sSyncState;

exit:
	if (lpStream)
		lpStream->Release();

	return hr;
}

bool ECSyncContext::SyncStatusLoaded() const
{
	return !m_mapSyncStatus.empty();
}


HRESULT ECSyncContext::HrClearSyncStatus()
{
	m_mapSyncStatus.clear();
	return hrSuccess;
}


HRESULT ECSyncContext::HrLoadSyncStatus(SBinary *lpsSyncState)
{
	HRESULT hr = hrSuccess;
	ULONG ulStatusCount = 0;
	ULONG ulStatusNumber = 0;
	ULONG ulVersion = 0;
	ULONG ulSize = 0;
	ULONG ulPos = 0;
	std::string strSourceKey;
	LPSTREAM lpStream = NULL;

	ASSERT(lpsSyncState != NULL);

	if (lpsSyncState->cb < 8) {
		hr = MAPI_E_CORRUPT_DATA;
		goto exit;
	}
	
	HrClearSyncStatus();

	ulVersion = *((ULONG*)(lpsSyncState->lpb));
	if (ulVersion != EC_SYNC_STATUS_VERSION)
		goto exit;

	ulStatusCount = *((ULONG*)(lpsSyncState->lpb+4));

	LOG_DEBUG(m_lpLogger, "Loading sync status stream: version=%u, items=%u", ulVersion, ulStatusCount);

	ulPos = 8;
	for (ulStatusNumber = 0; ulStatusNumber < ulStatusCount; ++ulStatusNumber) {
		ulSize = *((ULONG*)(lpsSyncState->lpb + ulPos));
		ulPos += 4;

		if (ulSize <= 16 || ulPos + ulSize + 4 > lpsSyncState->cb) {
			hr = MAPI_E_CORRUPT_DATA;
			goto exit;
		}
		
		strSourceKey.assign((char*)(lpsSyncState->lpb + ulPos), ulSize);
		ulPos += ulSize;
		ulSize = *((ULONG*)(lpsSyncState->lpb + ulPos));
		ulPos += 4;

		if (ulSize < 8 || ulPos + ulSize > lpsSyncState->cb) {
			hr = MAPI_E_CORRUPT_DATA;
			goto exit;
		}

		LOG_DEBUG(m_lpLogger, "  Stream %u: size=%u, sourcekey=%s", ulStatusNumber, ulSize, bin2hex(strSourceKey.size(), (unsigned char*)strSourceKey.data()).c_str());	

		hr = CreateStreamOnHGlobal(GlobalAlloc(GPTR, ulSize), true, &lpStream);
		if (hr != hrSuccess)
			goto exit;
		hr = lpStream->Write(lpsSyncState->lpb + ulPos, ulSize, &ulSize);
		if (hr != hrSuccess)
			goto exit;

		m_mapSyncStatus[strSourceKey] = lpStream;
		lpStream = NULL;

		ulPos += ulSize;
	}

exit:
	return hr;
}


HRESULT ECSyncContext::HrSaveSyncStatus(LPSPropValue *lppSyncStatusProp)
{
	HRESULT hr = hrSuccess;
	StatusStreamMap::iterator iSyncStatus;
	std::string strSyncStatus;
	ULONG ulSize = 0;
	ULONG ulVersion = EC_SYNC_STATUS_VERSION;
	char* lpszStream = NULL;
	LARGE_INTEGER liPos = {{0, 0}};
	STATSTG sStat;
	LPSPropValue lpSyncStatusProp = NULL;

	ASSERT(lppSyncStatusProp != NULL);

	strSyncStatus.assign((char*)&ulVersion, 4);
	ulSize = m_mapSyncStatus.size();
	strSyncStatus.append((char*)&ulSize, 4);

	LOG_DEBUG(m_lpLogger, "Saving sync status stream: items=%u", ulSize);

	for (iSyncStatus = m_mapSyncStatus.begin(); iSyncStatus != m_mapSyncStatus.end(); ++iSyncStatus) {
		ulSize = iSyncStatus->first.size();
		strSyncStatus.append((char*)&ulSize, 4);
		strSyncStatus.append(iSyncStatus->first);

		hr = iSyncStatus->second->Stat(&sStat, STATFLAG_NONAME);
		if (hr != hrSuccess)
			goto exit;
		
		ulSize = sStat.cbSize.LowPart;
		strSyncStatus.append((char*)&ulSize, 4);

		LOG_DEBUG(m_lpLogger, "  Stream: size=%u, sourcekey=%s", ulSize, bin2hex(iSyncStatus->first.size(), (unsigned char*)iSyncStatus->first.data()).c_str());	
		
		hr = iSyncStatus->second->Seek(liPos, STREAM_SEEK_SET, NULL);
		if (hr != hrSuccess)
			goto exit;
	
		lpszStream = new char[sStat.cbSize.LowPart];

		hr = iSyncStatus->second->Read(lpszStream, sStat.cbSize.LowPart, &ulSize);
		if (hr != hrSuccess)
			goto exit;

		strSyncStatus.append(lpszStream, sStat.cbSize.LowPart);
		delete[] lpszStream;
		lpszStream = NULL;
	}

	hr = MAPIAllocateBuffer(sizeof *lpSyncStatusProp, (void**)&lpSyncStatusProp);
	if (hr != hrSuccess)
		goto exit;
	memset(lpSyncStatusProp, 0, sizeof *lpSyncStatusProp);

	lpSyncStatusProp->Value.bin.cb = strSyncStatus.size();
	hr = MAPIAllocateMore(strSyncStatus.size(), lpSyncStatusProp, (void**)&lpSyncStatusProp->Value.bin.lpb);
	if (hr != hrSuccess)
		goto exit;
	memcpy(lpSyncStatusProp->Value.bin.lpb, strSyncStatus.data(), strSyncStatus.size());

	*lppSyncStatusProp = lpSyncStatusProp;
	lpSyncStatusProp = NULL;

exit:
	if (lpSyncStatusProp)
		MAPIFreeBuffer(lpSyncStatusProp);

	if (lpszStream)
		delete[] lpszStream;

	return hr;
}


HRESULT ECSyncContext::HrGetSyncStatusStream(LPMAPIFOLDER lpFolder, LPSTREAM *lppStream)
{
	HRESULT hr = hrSuccess;
	LPSPropValue lpPropVal = NULL;

	hr = HrGetOneProp(lpFolder, PR_SOURCE_KEY, &lpPropVal);
	if(hr != hrSuccess)
		goto exit;

	hr = HrGetSyncStatusStream(&lpPropVal->Value.bin, lppStream);
	if (hr != hrSuccess)
		goto exit;

exit:
	if (lpPropVal)
		MAPIFreeBuffer(lpPropVal);

	return hr;
}


HRESULT ECSyncContext::HrGetSyncStatusStream(SBinary *lpsSourceKey, LPSTREAM *lppStream)
{
	HRESULT hr = hrSuccess;
	LPSTREAM lpStream = NULL;
	std::string strSourceKey;
	StatusStreamMap::iterator iStatusStream;
	ULONG ulSize;

	strSourceKey.assign((char*)lpsSourceKey->lpb, lpsSourceKey->cb);

	ulSize = m_mapSyncStatus.size();
	iStatusStream = m_mapSyncStatus.find(strSourceKey);

	if (iStatusStream != m_mapSyncStatus.end()) {
		*lppStream = iStatusStream->second;
	} else {
		hr = CreateNullStatusStream(&lpStream);
		if (hr != hrSuccess)
			goto exit;

		hr = MAPI_W_POSITION_CHANGED;
		m_mapSyncStatus[strSourceKey] = lpStream;
		lpStream->AddRef();
		*lppStream = lpStream;
	}
	(*lppStream)->AddRef();

exit:
	if(lpStream)
		lpStream->Release();

	return hr;
}


HRESULT ECSyncContext::GetResyncID(ULONG *lpulResyncID)
{
	HRESULT hr = hrSuccess;
	MAPIFolderPtr ptrRoot;
	SPropValuePtr ptrResyncID;

	if (lpulResyncID == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	hr = HrOpenRootFolder(&ptrRoot, NULL);
	if (hr != hrSuccess)
		goto exit;

	hr = HrGetOneProp(ptrRoot, PR_EC_RESYNC_ID, &ptrResyncID);
	if (hr == hrSuccess)
		*lpulResyncID = ptrResyncID->Value.ul;
	else if (hr == MAPI_E_NOT_FOUND) {
		*lpulResyncID = 0;
		hr = hrSuccess;
	}

exit:
	return hr;
}

HRESULT ECSyncContext::SetResyncID(ULONG ulResyncID)
{
	HRESULT hr = hrSuccess;
	MAPIFolderPtr ptrRoot;
	SPropValue sPropResyncID;

	hr = HrOpenRootFolder(&ptrRoot, NULL);
	if (hr != hrSuccess)
		goto exit;

	sPropResyncID.ulPropTag = PR_EC_RESYNC_ID;
	sPropResyncID.Value.ul = ulResyncID;

	hr = HrSetOneProp(ptrRoot, &sPropResyncID);

exit:
	return hr;
}

HRESULT ECSyncContext::GetStoredServerUid(LPGUID lpServerUid)
{
	HRESULT hr = hrSuccess;
	MAPIFolderPtr ptrRoot;
	SPropValuePtr ptrServerUid;

	if (lpServerUid == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	hr = HrOpenRootFolder(&ptrRoot, NULL);
	if (hr != hrSuccess)
		goto exit;

	hr = HrGetOneProp(ptrRoot, PR_EC_STORED_SERVER_UID, &ptrServerUid);
	if (hr != hrSuccess)
		goto exit;

	if (ptrServerUid->Value.bin.lpb == NULL || ptrServerUid->Value.bin.cb != sizeof *lpServerUid) {
		hr = MAPI_E_CORRUPT_DATA;
		goto exit;
	}

	memcpy(lpServerUid, ptrServerUid->Value.bin.lpb, sizeof *lpServerUid);

exit:
	return hr;
}

HRESULT ECSyncContext::SetStoredServerUid(LPGUID lpServerUid)
{
	HRESULT hr = hrSuccess;
	MAPIFolderPtr ptrRoot;
	SPropValue sPropServerUid;

	hr = HrOpenRootFolder(&ptrRoot, NULL);
	if (hr != hrSuccess)
		goto exit;

	sPropServerUid.ulPropTag = PR_EC_STORED_SERVER_UID;
	sPropServerUid.Value.bin.cb = sizeof *lpServerUid;
	sPropServerUid.Value.bin.lpb = (LPBYTE)lpServerUid;

	hr = HrSetOneProp(ptrRoot, &sPropServerUid);

exit:
	return hr;
}

HRESULT ECSyncContext::GetServerUid(LPGUID lpServerUid)
{
	HRESULT hr = hrSuccess;
	MsgStorePtr ptrStore;
	SPropValuePtr ptrServerUid;

	if (lpServerUid == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	hr = HrGetMsgStore(&ptrStore);
	if (hr != hrSuccess)
		goto exit;

	hr = HrGetOneProp(ptrStore, PR_EC_SERVER_UID, &ptrServerUid);
	if (hr != hrSuccess)
		goto exit;

	if (ptrServerUid->Value.bin.lpb == NULL || ptrServerUid->Value.bin.cb != sizeof *lpServerUid) {
		hr = MAPI_E_CORRUPT_DATA;
		goto exit;
	}

	memcpy(lpServerUid, ptrServerUid->Value.bin.lpb, sizeof *lpServerUid);

exit:
	return hr;
}

ULONG ECSyncContext::OnChange(ULONG ulFlags, LPENTRYLIST lpEntryList)
{
	ULONG ulSyncId = 0;
	ULONG ulChangeId = 0;

	pthread_mutex_lock(&m_hMutex);

	for (unsigned i = 0; i < lpEntryList->cValues; ++i) {
		if (lpEntryList->lpbin[i].cb < 8) {
			m_lpLogger->Log(EC_LOGLEVEL_INFO, "change notification: [Invalid]");
			continue;
		}

		ulSyncId = SYNCID(lpEntryList->lpbin[i].lpb);
		ulChangeId = CHANGEID(lpEntryList->lpbin[i].lpb);
		m_mapNotifiedSyncIds[ulSyncId] = ulChangeId;

		m_lpLogger->Log(EC_LOGLEVEL_INFO, "change notification: syncid=%u, changeid=%u", ulSyncId, ulChangeId);
	}

	pthread_mutex_unlock(&m_hMutex);

	return 0;
}
