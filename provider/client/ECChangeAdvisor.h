/*
 * Copyright 2005 - 2015  Zarafa B.V. and its licensors
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License, version 3,
 * as published by the Free Software Foundation with the following
 * additional terms according to sec. 7:
 * 
 * "Zarafa" is a registered trademark of Zarafa B.V.
 * The licensing of the Program under the AGPL does not imply a trademark 
 * license. Therefore any rights, title and interest in our trademarks 
 * remain entirely with us.
 * 
 * Our trademark policy (see TRADEMARKS.txt) allows you to use our trademarks
 * in connection with Propagation and certain other acts regarding the Program.
 * In any case, if you propagate an unmodified version of the Program you are
 * allowed to use the term "Zarafa" to indicate that you distribute the Program.
 * Furthermore you may use our trademarks where it is necessary to indicate the
 * intended purpose of a product or service provided you use it in accordance
 * with honest business practices. For questions please contact Zarafa at
 * trademark@zarafa.com.
 *
 * The interactive user interface of the software displays an attribution 
 * notice containing the term "Zarafa" and/or the logo of Zarafa. 
 * Interactive user interfaces of unmodified and modified versions must 
 * display Appropriate Legal Notices according to sec. 5 of the GNU Affero 
 * General Public License, version 3, when you propagate unmodified or 
 * modified versions of the Program. In accordance with sec. 7 b) of the GNU 
 * Affero General Public License, version 3, these Appropriate Legal Notices 
 * must retain the logo of Zarafa or display the words "Initial Development 
 * by Zarafa" if the display of the logo is not reasonably feasible for
 * technical reasons.
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

#ifndef ECCHANGEADVISOR_H
#define ECCHANGEADVISOR_H

#include "zcdefs.h"
#include <mapidefs.h>
#include <mapispi.h>

#include <pthread.h>	

#include <ECUnknown.h>
#include <IECChangeAdvisor.h>
#include "ECICS.h"
#include "ZarafaCode.h"

#include <map>

class ECMsgStore;
class ECLogger;

/**
 * ECChangeAdvisor: Implementation IECChangeAdvisor, which allows one to register for 
 *                  change notifications on folders.
 */
class ECChangeAdvisor _final : public ECUnknown
{
protected:
	/**
	 * Construct the ChangeAdvisor.
	 *
	 * @param[in]	lpMsgStore
	 *					The message store that contains the folder to be registered for
	 *					change notifications.
	 */
	ECChangeAdvisor(ECMsgStore *lpMsgStore);

	/**
	 * Destructor.
	 */
	virtual ~ECChangeAdvisor();

public:
	/**
	 * Construct the ChangeAdvisor.
	 *
	 * @param[in]	lpMsgStore
	 *					The message store that contains the folder to be registered for
	 *					change notifications.
	 * @param[out]	lppChangeAdvisor
	 *					The new change advisor.
	 * @return HRESULT
	 */
	static	HRESULT Create(ECMsgStore *lpMsgStore, ECChangeAdvisor **lppChangeAdvisor);

	/**
	 * Get an alternate interface to the same object.
	 *
	 * @param[in]	refiid
	 *					A reference to the IID of the requested interface.
	 *					Supported interfaces:
	 *						- IID_ECChangeAdvisor
	 *						- IID_IECChangeAdvisor
	 * @param[out]	lpvoid
	 *					Pointer to a pointer of the requested type, casted to a void pointer.
	 */
	virtual HRESULT QueryInterface(REFIID refiid, void **lpvoid);

	// IECChangeAdvisor
	virtual HRESULT GetLastError(HRESULT hResult, ULONG ulFlags, LPMAPIERROR *lppMAPIError);
	virtual HRESULT Config(LPSTREAM lpStream, LPGUID lpGUID, LPECCHANGEADVISESINK lpAdviseSink, ULONG ulFlags);
	virtual HRESULT UpdateState(LPSTREAM lpStream);
	virtual HRESULT AddKeys(LPENTRYLIST lpEntryList);
	virtual HRESULT RemoveKeys(LPENTRYLIST lpEntryList);
	virtual HRESULT IsMonitoringSyncId(syncid_t ulSyncId);
	virtual HRESULT UpdateSyncState(syncid_t ulSyncId, changeid_t ulChangeId);

private:
	typedef std::map<syncid_t, connection_t> ConnectionMap;
	typedef std::map<syncid_t, changeid_t> SyncStateMap;

	/**
	 * Get the sync id from a ConnectionMap entry.
	 *
	 * @param[in]	sConnection
	 *					The ConnectionMap entry from which to extract the sync id.
	 * @return The sync id extracted from the the ConnectionMap entry.
	 */
	static ULONG					GetSyncId(const ConnectionMap::value_type &sConnection);

	/**
	 * Create a SyncStateMap entry from an SSyncState structure.
	 *
	 * @param[in]	sSyncState
	 *					The SSyncState structure to be converted to a SyncStateMap entry.
	 * @return A new SyncStateMap entry.
	 */
	static SyncStateMap::value_type	ConvertSyncState(const SSyncState &sSyncState);

	static SSyncState				ConvertSyncStateMapEntry(const SyncStateMap::value_type &sMapEntry);

	/**
	 * Compare the sync ids from a ConnectionMap entry and a SyncStateMap entry.
	 *
	 * @param[in]	sConnection
	 *					The ConnectionMap entry to compare the sync id with.
	 * @param[in]	sSyncState
	 *					The SyncStateMap entry to compare the sync id with.
	 * @return true if the sync ids are equal, false otherwise.
	 */
	static bool						CompareSyncId(const ConnectionMap::value_type &sConnection, const SyncStateMap::value_type &sSyncState);

	/**
	 * Reload the change notifications.
	 *
	 * @param[in]	lpParam
	 *					The parameter passed to AddSessionReloadCallback.
	 * @param[in]	newSessionId
	 *					The sessionid of the new session.
	 *
	 * @return HRESULT.
	 */
	static HRESULT					Reload(void *lpParam, ECSESSIONID newSessionId);

	/**
	 * Purge all unused connections from advisor.
	 * @return HRESULT
	 */
	HRESULT							PurgeStates();

	class xECChangeAdvisor _final : public IECChangeAdvisor {
		// IUnknown
		virtual ULONG __stdcall AddRef(void) _override;
		virtual ULONG __stdcall Release(void) _override;
		virtual HRESULT __stdcall QueryInterface(REFIID refiid, void **pInterface) _override;

		// IECChangeAdvisor
		virtual HRESULT __stdcall GetLastError(HRESULT hResult, ULONG ulFlags, LPMAPIERROR *lppMAPIError) _override;
		virtual HRESULT __stdcall Config(LPSTREAM lpStream, LPGUID lpGUID, LPECCHANGEADVISESINK lpAdviseSink, ULONG ulFlags) _override;
		virtual HRESULT __stdcall UpdateState(LPSTREAM lpStream) _override;
		virtual HRESULT __stdcall AddKeys(LPENTRYLIST lpEntryList) _override;
		virtual HRESULT __stdcall RemoveKeys(LPENTRYLIST lpEntryList) _override;
		virtual HRESULT __stdcall IsMonitoringSyncId(ULONG ulSyncId) _override;
		virtual HRESULT __stdcall UpdateSyncState(ULONG ulSyncId, ULONG ulChangeId) _override;
	} m_xECChangeAdvisor;


	ECMsgStore				*m_lpMsgStore;
	LPECCHANGEADVISESINK	m_lpChangeAdviseSink;
	ULONG					m_ulFlags;
	pthread_mutex_t			m_hConnectionLock;
	ConnectionMap			m_mapConnections;
	SyncStateMap			m_mapSyncStates;
	ECLogger				*m_lpLogger;
	ULONG					m_ulReloadId;
};

#endif // ndef ECCHANGEADVISOR_H
