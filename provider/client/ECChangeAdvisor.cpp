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

#include <ECGuid.h>
#include <ECSyncLog.h>
#include <ECDebug.h>
#include <ECLogger.h>

#include "ECChangeAdvisor.h"
#include "ECMsgStore.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

ULONG ECChangeAdvisor::GetSyncId(const ConnectionMap::value_type &sConnection)
{
	return sConnection.first;
}

ECChangeAdvisor::SyncStateMap::value_type ECChangeAdvisor::ConvertSyncState(const SSyncState &sSyncState)
{
	return SyncStateMap::value_type(sSyncState.ulSyncId, sSyncState.ulChangeId);
}

SSyncState ECChangeAdvisor::ConvertSyncStateMapEntry(const SyncStateMap::value_type &sMapEntry)
{
	SSyncState tmp = {sMapEntry.first, sMapEntry.second};
	return tmp;
}

bool ECChangeAdvisor::CompareSyncId(const ConnectionMap::value_type &sConnection, const SyncStateMap::value_type &sSyncState)
{
	return sConnection.first < sSyncState.first;
}

ECChangeAdvisor::ECChangeAdvisor(ECMsgStore *lpMsgStore)
	: m_lpMsgStore(lpMsgStore)
	, m_lpChangeAdviseSink(NULL)
	, m_ulFlags(0)
	, m_ulReloadId(0)
{ 
	pthread_mutexattr_t attr;

	ECSyncLog::GetLogger(&m_lpLogger);

	m_lpMsgStore->AddRef();

	// Need MUTEX RECURSIVE because with a reconnection the funtion PurgeStates called indirect the function Reload again:
	// ECChangeAdvisor::Reload(....)
 	// WSTransport::HrReLogon()
 	// WSTransport::HrGetSyncStates(....)
 	// ECChangeAdvisor::PurgeStates()
 	// ECChangeAdvisor::UpdateState(IStream * lpStream)

	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE_NP);
	pthread_mutex_init(&m_hConnectionLock, &attr);
}

ECChangeAdvisor::~ECChangeAdvisor()
{
	ConnectionMap::iterator	iterConnection;

	if (m_ulReloadId)
		m_lpMsgStore->lpTransport->RemoveSessionReloadCallback(m_ulReloadId);

	// Unregister notifications
	if (!(m_ulFlags & SYNC_CATCHUP))
		m_lpMsgStore->m_lpNotifyClient->Unadvise(ECLISTCONNECTION(m_mapConnections.begin(), m_mapConnections.end()));

	if (m_lpChangeAdviseSink)
		m_lpChangeAdviseSink->Release();
		
	if (m_lpLogger)
		m_lpLogger->Release();

	pthread_mutex_destroy(&m_hConnectionLock);
	m_lpMsgStore->Release();
}

HRESULT ECChangeAdvisor::QueryInterface(REFIID refiid, void **lppInterface)
{
	REGISTER_INTERFACE(IID_ECChangeAdvisor, this);
	REGISTER_INTERFACE(IID_ECUnknown, this);

	REGISTER_INTERFACE(IID_IECChangeAdvisor, &this->m_xECChangeAdvisor);
	REGISTER_INTERFACE(IID_IUnknown, &this->m_xECChangeAdvisor);

	return MAPI_E_INTERFACE_NOT_SUPPORTED;
}

/**
 * Create a ECChangeAdvisor instance.
 *
 * @param[in]	lpMsgStore			The store to register the change notifications on
 * @param[out]	lppChangeAdvisor	The newly create change advisor
 *
 * @retval	MAPI_E_INVALID_PARAMETER	lpMsgStore or lppChangeAdvisor are NULL pointers
 * @retval	MAPI_E_NO_SUPPORT			The profile was create with notification disabled or
 * 										enhanced ICS is not enabled.
 */
HRESULT ECChangeAdvisor::Create(ECMsgStore *lpMsgStore, ECChangeAdvisor **lppChangeAdvisor)
{
	HRESULT			hr = hrSuccess;
	ECChangeAdvisor	*lpChangeAdvisor = NULL;
	BOOL			fEnhancedICS = false;

	if (lpMsgStore == NULL || lppChangeAdvisor == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	if (lpMsgStore->m_lpNotifyClient == NULL) {
		hr = MAPI_E_NO_SUPPORT;
		goto exit;
	}

	hr = lpMsgStore->lpTransport->HrCheckCapabilityFlags(ZARAFA_CAP_ENHANCED_ICS, &fEnhancedICS);
	if (hr != hrSuccess)
		goto exit;
	if (!fEnhancedICS) {
		hr = MAPI_E_NO_SUPPORT;
		goto exit;
	}

	lpChangeAdvisor = new ECChangeAdvisor(lpMsgStore);
	hr = lpChangeAdvisor->QueryInterface(IID_ECChangeAdvisor, (void**)lppChangeAdvisor);
	if (hr != hrSuccess)
		goto exit;

	hr = lpMsgStore->lpTransport->AddSessionReloadCallback(lpChangeAdvisor, &Reload, &lpChangeAdvisor->m_ulReloadId);
	if (hr != hrSuccess)
		goto exit;

	lpChangeAdvisor = NULL;

exit:
	if (lpChangeAdvisor)
		lpChangeAdvisor->Release();

	return hr;
}



HRESULT ECChangeAdvisor::GetLastError(HRESULT hResult, ULONG ulFlags, LPMAPIERROR *lppMAPIError)
{
	return MAPI_E_NO_SUPPORT;
}

HRESULT ECChangeAdvisor::Config(LPSTREAM lpStream, LPGUID /*lpGUID*/, LPECCHANGEADVISESINK lpAdviseSink, ULONG ulFlags)
{
	HRESULT					hr = hrSuccess;
	ULONG					ulVal = 0;
	LPENTRYLIST				lpEntryList = NULL;
	ULONG					ulRead = {0};
	ConnectionMap::iterator	iterConnection;
	LARGE_INTEGER			liSeekStart = {{0}};

	if (lpAdviseSink == NULL && !(ulFlags & SYNC_CATCHUP)) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	// Unregister notifications
	if (!(m_ulFlags & SYNC_CATCHUP))
		m_lpMsgStore->m_lpNotifyClient->Unadvise(ECLISTCONNECTION(m_mapConnections.begin(), m_mapConnections.end()));
	m_mapConnections.clear();

	if (m_lpChangeAdviseSink) {
		m_lpChangeAdviseSink->Release();
		m_lpChangeAdviseSink = NULL;
	}

	m_ulFlags = ulFlags;

	if (lpAdviseSink) {
		m_lpChangeAdviseSink = lpAdviseSink;
		m_lpChangeAdviseSink->AddRef();
	}

	if (lpStream == NULL)
		goto exit;

	hr = lpStream->Seek(liSeekStart, SEEK_SET, NULL);
	if (hr != hrSuccess)
		goto exit;

	hr = lpStream->Read(&ulVal, sizeof(ulVal), &ulRead);
	if (hr != hrSuccess)
		goto exit;
	if (ulRead != sizeof(ulVal)) {
		hr = MAPI_E_CALL_FAILED;
		goto exit;
	}

	if (ulVal > 0) {
		hr = MAPIAllocateBuffer(sizeof *lpEntryList, (void**)&lpEntryList);
		if (hr != hrSuccess)
			goto exit;

		hr = MAPIAllocateMore(ulVal * sizeof *lpEntryList->lpbin, lpEntryList, (void**)&lpEntryList->lpbin);
		if (hr != hrSuccess)
			goto exit;

		lpEntryList->cValues = ulVal;
		for (ULONG i = 0; i < lpEntryList->cValues; ++i) {
			hr = lpStream->Read(&ulVal, sizeof(ulVal), &ulRead);
			if (hr != hrSuccess)
				goto exit;
			if (ulRead != sizeof(ulVal)) {
				hr = MAPI_E_CALL_FAILED;
				goto exit;
			}	

			hr = MAPIAllocateMore(ulVal, lpEntryList, (void**)&lpEntryList->lpbin[i].lpb);
			if (hr != hrSuccess)
				goto exit;

			lpEntryList->lpbin[i].cb = ulVal;
			hr = lpStream->Read(lpEntryList->lpbin[i].lpb, ulVal, &ulRead);
			if (hr != hrSuccess)
				goto exit;
			if (ulRead != ulVal) {
				hr = MAPI_E_CALL_FAILED;
				goto exit;
			}
		}

		hr = AddKeys(lpEntryList);
		if (hr != hrSuccess)
			goto exit;
	}

exit:
	if (lpEntryList)
		MAPIFreeBuffer(lpEntryList);

	return hr;
}

HRESULT ECChangeAdvisor::PurgeStates()
{
	HRESULT				hr = hrSuccess;
	ECLISTSYNCID		lstSyncId;
	ECLISTSYNCSTATE		lstSyncState;
	SyncStateMap		mapChangeId;

	std::list<ConnectionMap::value_type>			lstObsolete;
	std::list<ConnectionMap::value_type>::iterator	iterObsolete;

	ASSERT(pthread_mutex_trylock(&m_hConnectionLock) == EBUSY);

	// First get the most up to date change ids for all registered sync ids (we'll ignore the changeid's since we don't know if we actually got that far)
	std::transform(m_mapConnections.begin(), m_mapConnections.end(), std::back_inserter(lstSyncId), &GetSyncId);
	hr = m_lpMsgStore->m_lpNotifyClient->UpdateSyncStates(lstSyncId, &lstSyncState);
	if (hr != hrSuccess)
		goto exit;

	// Create a map based on the returned sync states
	std::transform(lstSyncState.begin(), lstSyncState.end(), std::inserter(mapChangeId, mapChangeId.begin()), &ConvertSyncState);
	
	// Find all connections that are not used for the returned set of sync states and remove them
	std::set_difference(m_mapConnections.begin(), m_mapConnections.end(), mapChangeId.begin(), mapChangeId.end(), std::back_inserter(lstObsolete), &CompareSyncId);

	// Get rid of the obsolete connections (suboptimal)
	for (iterObsolete = lstObsolete.begin(); iterObsolete != lstObsolete.end(); ++iterObsolete) {
		m_lpMsgStore->m_lpNotifyClient->Unadvise(iterObsolete->second);
		m_mapConnections.erase(iterObsolete->first);
		m_mapSyncStates.erase(iterObsolete->first);
	}

exit:
	return hr;
}

HRESULT ECChangeAdvisor::UpdateState(LPSTREAM lpStream)
{
	HRESULT					hr = hrSuccess;
	ConnectionMap::iterator	iterConnection;
	LARGE_INTEGER			liPos = {{0}};
	ULARGE_INTEGER			uliSize = {{0}};
	ULONG					ulVal = 0;
	SyncStateMap			mapChangeId;

	pthread_mutex_lock(&m_hConnectionLock);

	if (m_lpChangeAdviseSink == NULL && !(m_ulFlags & SYNC_CATCHUP)) {
		hr = MAPI_E_UNCONFIGURED;
		goto exit;
	}

	if (lpStream == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	hr = PurgeStates();
	if (hr != hrSuccess)
		goto exit;

	// Since m_mapSyncStates are related m_mapConnection the maps should
	// be equal in size.
	ASSERT(m_mapConnections.size() == m_mapSyncStates.size());

	// Create the status stream
	lpStream->Seek(liPos, STREAM_SEEK_SET, NULL);
	lpStream->SetSize(uliSize);

	// First the amount of items in the stream
	ulVal = (ULONG)m_mapConnections.size();
	lpStream->Write(&ulVal, sizeof(ulVal), NULL);

	for (iterConnection = m_mapConnections.begin(); iterConnection != m_mapConnections.end(); ++iterConnection) {
		// The size of the sync state
		ulVal = 2 * sizeof(ULONG);		// syncid, changeid
		lpStream->Write(&ulVal, sizeof(ulVal), NULL);

		// syncid
		lpStream->Write(&iterConnection->first, sizeof(iterConnection->first), NULL);

		// changeid
		lpStream->Write(&m_mapSyncStates[iterConnection->first], sizeof(SyncStateMap::key_type), NULL);
	}


exit:
	pthread_mutex_unlock(&m_hConnectionLock);
	return hr;
}

HRESULT ECChangeAdvisor::AddKeys(LPENTRYLIST lpEntryList)
{
	HRESULT						hr = hrSuccess;
	SSyncState					*lpsSyncState = NULL;
	ECLISTCONNECTION			listConnections;
	ECLISTCONNECTION::iterator	iterConnection;
	ECLISTSYNCSTATE				listSyncStates;

	if (m_lpChangeAdviseSink == NULL && !(m_ulFlags & SYNC_CATCHUP)) {
		hr = MAPI_E_UNCONFIGURED;
		goto exit;
	}

	if (lpEntryList == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	pthread_mutex_lock(&m_hConnectionLock);

	LOG_DEBUG(m_lpLogger, "Adding %u keys", lpEntryList->cValues);
	
	for (ULONG i = 0; hr == hrSuccess && i < lpEntryList->cValues; ++i) {
		if (lpEntryList->lpbin[i].cb >= sizeof(SSyncState)) {
			lpsSyncState = (SSyncState*)lpEntryList->lpbin[i].lpb;

			LOG_DEBUG(m_lpLogger, " - Key %u: syncid=%u, changeid=%u", i, lpsSyncState->ulSyncId, lpsSyncState->ulChangeId);

			// Check if we don't have this sync state already
			if (m_mapConnections.find(lpsSyncState->ulSyncId) != m_mapConnections.end()) {
				LOG_DEBUG(m_lpLogger, " - Key %u: duplicate!", lpsSyncState->ulSyncId);
				continue;
			}

			if (!(m_ulFlags & SYNC_CATCHUP))
				listSyncStates.push_back(*lpsSyncState);
			else
				listConnections.push_back(ConnectionMap::value_type(lpsSyncState->ulSyncId, 0));
		} else {
			m_lpLogger->Log(EC_LOGLEVEL_ERROR, " - Key %u: Invalid size=%u", i, lpEntryList->lpbin[i].cb);
			hr = MAPI_E_INVALID_PARAMETER;
		}
	}

	if (!(m_ulFlags & SYNC_CATCHUP))
		hr = m_lpMsgStore->m_lpNotifyClient->Advise(listSyncStates, m_lpChangeAdviseSink, &listConnections);

	if (hr == hrSuccess) {
		m_mapConnections.insert(listConnections.begin(), listConnections.end());
		std::transform(listSyncStates.begin(), listSyncStates.end(), std::inserter(m_mapSyncStates, m_mapSyncStates.begin()), &ConvertSyncState);
	}

	pthread_mutex_unlock(&m_hConnectionLock);

exit:
	return hr;
}

HRESULT ECChangeAdvisor::RemoveKeys(LPENTRYLIST lpEntryList)
{
	HRESULT					hr = hrSuccess;
	SSyncState				*lpsSyncState = NULL;
	ConnectionMap::iterator	iterConnection;
	ECLISTCONNECTION		listConnections;

	if (m_lpChangeAdviseSink == NULL && !(m_ulFlags & SYNC_CATCHUP)) {
		hr = MAPI_E_UNCONFIGURED;
		goto exit;
	}

	if (lpEntryList == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	pthread_mutex_lock(&m_hConnectionLock);
	
	for (ULONG i = 0; hr == hrSuccess && i < lpEntryList->cValues; ++i) {
		if (lpEntryList->lpbin[i].cb >= sizeof(SSyncState)) {
			lpsSyncState = (SSyncState*)lpEntryList->lpbin[i].lpb;

			// Try to delete the sync state from state map anyway
			m_mapSyncStates.erase(lpsSyncState->ulSyncId);

			// Check if we even have the sync state
			iterConnection = m_mapConnections.find(lpsSyncState->ulSyncId);
			if (iterConnection == m_mapConnections.end())
				continue;

			// Unregister the sync state.
			if (!(m_ulFlags & SYNC_CATCHUP))
				listConnections.push_back(*iterConnection);

			// Remove from map
			m_mapConnections.erase(iterConnection);
		}
	}

	hr = m_lpMsgStore->m_lpNotifyClient->Unadvise(listConnections);
	pthread_mutex_unlock(&m_hConnectionLock);

exit:
	return hr;
}

HRESULT ECChangeAdvisor::IsMonitoringSyncId(syncid_t ulSyncId)
{
	HRESULT	hr = hrSuccess;

	if (m_mapConnections.find(ulSyncId) == m_mapConnections.end())
		hr = MAPI_E_NOT_FOUND;

	return hr;
}

HRESULT ECChangeAdvisor::UpdateSyncState(syncid_t ulSyncId, changeid_t ulChangeId)
{
	HRESULT hr = hrSuccess;
	SyncStateMap::iterator iSyncState;

	pthread_mutex_lock(&m_hConnectionLock);

	iSyncState = m_mapSyncStates.find(ulSyncId);
	if (iSyncState == m_mapSyncStates.end()) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	iSyncState->second = ulChangeId;

exit:
	pthread_mutex_unlock(&m_hConnectionLock);

	return hr;
}

HRESULT ECChangeAdvisor::Reload(void *lpParam, ECSESSIONID /*newSessionId*/)
{
	HRESULT				hr = hrSuccess;
	ECChangeAdvisor		*lpChangeAdvisor = (ECChangeAdvisor*)lpParam;
	ECLISTSYNCSTATE		listSyncStates;
	ECLISTCONNECTION	listConnections;

	if (lpParam == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	pthread_mutex_lock(&lpChangeAdvisor->m_hConnectionLock);

	if ((lpChangeAdvisor->m_ulFlags & SYNC_CATCHUP))
		goto exit;

	/**
	 * Here we will reregister all change notifications.
	 **/

	// Unregister notifications first
	lpChangeAdvisor->m_lpMsgStore->m_lpNotifyClient->Unadvise(ECLISTCONNECTION(lpChangeAdvisor->m_mapConnections.begin(), lpChangeAdvisor->m_mapConnections.end()));
	lpChangeAdvisor->m_mapConnections.clear();

	
	// Now re-register the notifications
	std::transform(lpChangeAdvisor->m_mapSyncStates.begin(), lpChangeAdvisor->m_mapSyncStates.end(), std::back_inserter(listSyncStates), &ConvertSyncStateMapEntry);
	hr = lpChangeAdvisor->m_lpMsgStore->m_lpNotifyClient->Advise(listSyncStates, lpChangeAdvisor->m_lpChangeAdviseSink, &listConnections);
	if (hr == hrSuccess)
		lpChangeAdvisor->m_mapConnections.insert(listConnections.begin(), listConnections.end());

	
exit:
	if (lpChangeAdvisor)
		pthread_mutex_unlock(&lpChangeAdvisor->m_hConnectionLock);

	return hr;
}



//////////////////////////////////
// IECChangeAdvisor interface

ULONG ECChangeAdvisor::xECChangeAdvisor::AddRef() {
	TRACE_MAPI(TRACE_ENTRY, "IECChangeAdvisor::AddRef", "");
	METHOD_PROLOGUE_(ECChangeAdvisor, ECChangeAdvisor);
	return pThis->AddRef();
}

ULONG ECChangeAdvisor::xECChangeAdvisor::Release() {
	TRACE_MAPI(TRACE_ENTRY, "IECChangeAdvisor::Release", "");
	METHOD_PROLOGUE_(ECChangeAdvisor, ECChangeAdvisor);
	return pThis->Release();
}

HRESULT ECChangeAdvisor::xECChangeAdvisor::QueryInterface(REFIID refiid, void **lppInterface) {
	TRACE_MAPI(TRACE_ENTRY, "IECChangeAdvisor::QueryInterface", "");
	METHOD_PROLOGUE_(ECChangeAdvisor, ECChangeAdvisor);
	return pThis->QueryInterface(refiid, lppInterface);
}

HRESULT ECChangeAdvisor::xECChangeAdvisor::GetLastError(HRESULT hResult, ULONG ulFlags, LPMAPIERROR *lppMAPIError) {
	TRACE_MAPI(TRACE_ENTRY, "IECChangeAdvisor::GetLastError", "%s, %x", GetMAPIErrorDescription(hResult).c_str(), ulFlags);
	METHOD_PROLOGUE_(ECChangeAdvisor, ECChangeAdvisor);
	HRESULT hr = pThis->GetLastError(hResult, ulFlags, lppMAPIError);
	TRACE_MAPI(TRACE_RETURN, "IECChangeAdvisor::GetLastError", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECChangeAdvisor::xECChangeAdvisor::Config(LPSTREAM lpStream, LPGUID lpGUID, LPECCHANGEADVISESINK lpAdviseSink, ULONG ulFlags) {
	TRACE_MAPI(TRACE_ENTRY, "IECChangeAdvisor::Config", "%s, %x", lpGUID ? DBGGUIDToString(*lpGUID).c_str() : "NULL", ulFlags);
	METHOD_PROLOGUE_(ECChangeAdvisor, ECChangeAdvisor);
	HRESULT hr = pThis->Config(lpStream, lpGUID, lpAdviseSink, ulFlags);
	TRACE_MAPI(TRACE_RETURN, "IECChangeAdvisor::Config", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECChangeAdvisor::xECChangeAdvisor::UpdateState(LPSTREAM lpStream) {
	TRACE_MAPI(TRACE_ENTRY, "IECChangeAdvisor::UpdateState", "");
	METHOD_PROLOGUE_(ECChangeAdvisor, ECChangeAdvisor);
	HRESULT hr = pThis->UpdateState(lpStream);
	TRACE_MAPI(TRACE_RETURN, "IECChangeAdvisor::UpdateState", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECChangeAdvisor::xECChangeAdvisor::AddKeys(LPENTRYLIST lpEntryList) {
	TRACE_MAPI(TRACE_ENTRY, "IECChangeAdvisor::AddKeys", "");
	METHOD_PROLOGUE_(ECChangeAdvisor, ECChangeAdvisor);
	HRESULT hr = pThis->AddKeys(lpEntryList);
	TRACE_MAPI(TRACE_RETURN, "IECChangeAdvisor::AddKeys", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECChangeAdvisor::xECChangeAdvisor::RemoveKeys(LPENTRYLIST lpEntryList) {
	TRACE_MAPI(TRACE_ENTRY, "IECChangeAdvisor::RemoveKeys", "");
	METHOD_PROLOGUE_(ECChangeAdvisor, ECChangeAdvisor);
	HRESULT hr = pThis->RemoveKeys(lpEntryList);
	TRACE_MAPI(TRACE_RETURN, "IECChangeAdvisor::RemoveKeys", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECChangeAdvisor::xECChangeAdvisor::IsMonitoringSyncId(syncid_t ulSyncId) {
	TRACE_MAPI(TRACE_ENTRY, "IECChangeAdvisor::IsMonitoringSyncId", "%u", ulSyncId);
	METHOD_PROLOGUE_(ECChangeAdvisor, ECChangeAdvisor);
	return pThis->IsMonitoringSyncId(ulSyncId);
}

HRESULT ECChangeAdvisor::xECChangeAdvisor::UpdateSyncState(syncid_t ulSyncId, changeid_t ulChangeId) {
	TRACE_MAPI(TRACE_ENTRY, "IECChangeAdvisor::UpdateSyncState", "%u, %u", ulSyncId, ulChangeId);
	METHOD_PROLOGUE_(ECChangeAdvisor, ECChangeAdvisor);
	return pThis->UpdateSyncState(ulSyncId, ulChangeId);
}
