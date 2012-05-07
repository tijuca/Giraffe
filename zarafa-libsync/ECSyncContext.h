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

#ifndef ECSYNCCONTEXT_H
#define ECSYNCCONTEXT_H

#include <mapidefs.h>

#include <pthread.h>
#include "../provider/client/ECICS.h"

#include <map>
#include <set>
#include <string>

#include <IECChangeAdvisor.h>
#include <IECChangeAdviseSink.h>

typedef	std::map<std::string,LPSTREAM>		StatusStreamMap;
typedef std::map<std::string,SSyncState>	SyncStateMap;
typedef	std::map<ULONG,ULONG>				NotifiedSyncIdMap;

class ECLogger;
class ECSyncSettings;

/**
 * ECSyncContext:	This class encapsulates all synchronization related information that is
 *					only related to one side of the sync process (online or offline).
 */
class ECSyncContext
{
public:
	/**
	 * Construct a sync context.
	 *
	 * @param[in]	lpStore
	 *					The store for which to create the sync context.
	 * @param[in]	lpLogger
	 *					The logger to log to.
	 */
	ECSyncContext(LPMDB lpStore, ECLogger *lpLogger);

	/**
	 * Destructor.
	 */
	~ECSyncContext();

	/**
	 * Get a pointer to the message store on which this sync context operates.
	 * The underlying objects reference will be incremented, so the obtained
	 * pointer needs to be released when it's not needed anymore.
	 *
	 * @param[out]	lppMsgStore
	 *					Pointer to a IMsgStore pointer, which will contain
	 *					a pointer to the requested messages store upon successful
	 *					completion.
	 * @return HRESULT
	 */
	HRESULT HrGetMsgStore(LPMDB *lppMsgStore);

	/**
	 * Get the receive folder of the message store on which this sync context operates.
	 * The underlying objects reference will be incremented, so the obtained
	 * pointer needs to be released when it's not needed anymore.
	 * 
	 * @param[out]	lppInboxFolder
	 *					Pointer to a IMAPIFolder pointer, which will contain a
	 *					pointer to the requested folder upon successful completion.
	 * @return HRESULT
	 */
	HRESULT HrGetReceiveFolder(LPMAPIFOLDER *lppInboxFolder);

	/**
	 * Get the change advisor for this sync context.
	 * The underlying objects reference will be incremented, so the obtained
	 * pointer needs to be released when it's not needed anymore.
	 *
	 * @param[out]	lppChangeAdvisor
	 *					Pointer to a IECChangeAdvisor pointer, which will contain a
	 *					pointer to the change advisor upon successful completion.
	 * @return MAPI_E_NO_SUPPORT if the change notification system is disabled.
	 */
	HRESULT HrGetChangeAdvisor(LPECCHANGEADVISOR *lppChangeAdvisor);

	/**
	 * Replace the change advisor. This causes all the registered change advises
	 * to be dropped. Also the map with received states is cleared.
	 */
	HRESULT HrResetChangeAdvisor();

	/**
	 * Get the change advise sink for this sync context.
	 * The underlying objects reference will be incremented, so the obtained
	 * pointer needs to be released when it's not needed anymore.
	 *
	 * @param[out]	lppChangeAdviseSink
	 *					Pointer to a IECChangeAdviseSInk pointer, which will contain a
	 *					pointer to the change advise sink upon successful completion.
	 * @return HRESULT.
	 */
	HRESULT HrGetChangeAdviseSink(LPECCHANGEADVISESINK *lppChangeAdviseSink);

	/**
	 * Get the full hierarchy for the store on which this sync context operates.
	 *
	 * @param[in]	lpsPropTags
	 *					The proptags of the properties that should be obtained
	 *					from the hierarchy table.
	 * @param[out]	lppRows
	 *					Pointer to a SRowSet pointer, which will be populated with
	 *					the rows from the hierarchy table. Needs to be freed with
	 *					FreePRows by the caller.
	 * @return HRESULT
	 */
	HRESULT HrQueryHierarchyTable(LPSPropTagArray lpsPropTags, LPSRowSet *lppRows);

	/**
	 * Get the root folder for the current sync context.
	 *
	 * @param[in]	lppRootFolder
	 *					Pointer to a IMAPIFolder pointer that will contain a pointer
	 *					to the root folder upon successful completion.
	 * @param[in]	lppMsgStore
	 *					Pointer to a IMsgStore pointer that will contain a pointer to
	 *					the message store upon successful completion. Passing NULL will
	 *					cause no MsgStore pointer to be returned.
	 * @return HRESULT
	 */
	HRESULT HrOpenRootFolder(LPMAPIFOLDER *lppRootFolder, LPMDB *lppMsgStore = NULL);

	/**
	 * Open a folder using the store used by the current sync context.
	 *
	 * @param[in]	lpsEntryID
	 *					Pointer to a SBinary structure that will be interpreted as the
	 *					entry id of the folder to open.
	 * @param[out]	lppFolder
	 *					Pointer to a IMAPIFolder pointer that will contain a pointer to
	 *					the requested folder upon successful completion.
	 * @return HRESULT
	 */
	HRESULT HrOpenFolder(SBinary *lpsEntryID, LPMAPIFOLDER *lppFolder);

	/**
	 * Send a new mail notification through the current sync context.
	 *
	 * @Param[in]	lpNotification
	 *					Pointer to a NOTIFICATION structure that will be send as the
	 *					new mail notification.
	 * @return HRESULT
	 */
	HRESULT HrNotifyNewMail(LPNOTIFICATION lpNotification);

	/**
	 * Get the number of steps necessary to complete a sync on a particular folder.
	 *
	 * @param[in]	lpEntryID
	 *					Pointer to a SBinary structure that will be interpreted as the
	 *					entry id of the folder to synchronize.
	 * @param[in]	lpSourceKey
	 *					Pointer to a SBinary structure that will be interpreted as the
	 *					source key of the folder to synchronize.
	 * @param[in]	ulSyncFlags
	 *					Flags that control the behavior of the sync operation.
	 * @param[out]	lpulSteps
	 *					Pointer to a ULONG variable that will contain the number of steps
	 *					to complete a synchronization on the selected folder upon successful
	 *					completion.
	 * @return HRESULT
	 */
	HRESULT HrGetSteps(SBinary *lpEntryID, SBinary *lpSourceKey, ULONG ulSyncFlags, ULONG *lpulSteps);

	/**
	 * Update the change id for a particular sync id, based on a state stream.
	 * This will cause folders that have pending changes to be removed if the change id
	 * in the stream is greater or equal to the change id for which the pending change
	 * was queued.
	 *
	 * @param[in]	lpStream
	 *					The state stream from which the sync id and change id will be
	 *					extracted.
	 * @return HRESULT
	 */
	HRESULT HrUpdateChangeId(LPSTREAM lpStream);

	/**
	 * Check if the sync status streams have been loaded.
	 *
	 * @return true if sync status streams have been loaded, false otherwise.
	 */
	bool    SyncStatusLoaded() const;

	/**
	 * Clear the sync status streams.
	 *
	 * @return HRESULT
	 */
	HRESULT HrClearSyncStatus();

	/**
	 * Load the sync status streams.
	 *
	 * @param[in]	lpsSyncState
	 *					The SBinary structure containing the data to be decoded.
	 * @return HRESULT
	 */
	HRESULT HrLoadSyncStatus(SBinary *lpsSyncState);

	/**
	 * Save the sync status streams.
	 *
	 * @param[in]	lppSyncStatusProp
	 *					Pointer to a SPropValue pointer that will be populated with
	 *					the binary data that's made out of the status streams.
	 * @return HRESULT
	 */
	HRESULT HrSaveSyncStatus(LPSPropValue *lppSyncStatusProp);

	/**
	 * Get the sync status stream for a particular folder.
	 *
	 * @param[in]	lpFolder
	 *					The folder for which to get the sync status stream.
	 * @param[out]	lppStream
	 *					Pointer to a IStream pointer that will contain the
	 *					sync status stream on successful completion.
	 * @return HRESULT
	 */
	HRESULT HrGetSyncStatusStream(LPMAPIFOLDER lpFolder, LPSTREAM *lppStream);

	/**
	 * Get the sync status stream for a particular folder.
	 *
	 * @param[in]	lpSourceKey
	 *					An SBinary structure that will be interprested as a
	 *					sourcekey that specifies a folder.
	 * @param[out]	lppStream
	 *					Pointer to a IStream pointer that will contain the
	 *					sync status stream on successful completion.
	 * @return HRESULT
	 */
	HRESULT HrGetSyncStatusStream(SBinary *lpsSourceKey, LPSTREAM *lppStream);

	/**
	 * Get the resync id from the store.
	 * This id is incremented with zarafa-admin on the online store if a folder
	 * resync is required. If the online and offline id differ, a resync will be
	 * initiated. Afterwards the offline id is copied from the online id.
	 *
	 * @param[out]	lpulResyncID	The requested id.
	 */
	HRESULT GetResyncID(ULONG *lpulResyncID);

	/**
	 * Set the resync id on the store.
	 * @see GetResyncID
	 *
	 * @param[in]	ulResyncID		The id to set.
	 */
	HRESULT SetResyncID(ULONG ulResyncID);

	/**
	 * Get stored server UID.
	 * Get the stored onlinse server UID. This is compared to the current online
	 * server UID in order to determine if the online store was relocated. In that
	 * case a resync must be performed in order for ICS to function properly.
	 * This server UID is stored during the first folder sync step or whenever it's
	 * absent (for older profiles).
	 * Only applicable on offline stores.
	 *
	 * @param[out]	lpServerUid		The requested server UID.
	 */
	HRESULT GetStoredServerUid(LPGUID lpServerUid);

	/**
	 * Set stored server UID.
	 * @see GetStoredServerUid
	 *
	 * @param[in]	lpServerUid		The server uid to set.
	 */
	HRESULT SetStoredServerUid(LPGUID lpServerUid);

	/**
	 * Get the server UID.
	 * This is used to compare with the stores server UID.
	 * @see GetStoredServerUid
	 *
	 * @param[out]	lpServerUid		The requested server UID.
	 */
	HRESULT GetServerUid(LPGUID lpServerUid);

private:	// methods
	/**
	 * Get the sync state for a sourcekey
	 *
	 * @param[in]	lpSourceKey
	 *					The sourcekey for which to get the sync state.
	 * @param[out]	lpsSyncState
	 *					Pointer to a SSyncState structure that will be populated
	 *					with the retrieved sync state.
	 * @return HRESULT
	 */
	HRESULT HrGetSyncStateFromSourceKey(SBinary *lpSourceKey, SSyncState *lpsSyncState);

	/**
	 * Handle change events (through the ChangeAdviseSink).
	 *
	 * @param[in]	ulFlags
	 *					Unused
	 * @param[in]	lpEntryList
	 *					List of sync states that have changes pending.
	 * @return 0
	 */
	ULONG	OnChange(ULONG ulFlags, LPENTRYLIST lpEntryList);

	/**
	 * Release the change advisor and clear the map with received states.
	 */
	HRESULT HrReleaseChangeAdvisor();

private:	// members
	LPMDB					m_lpStore;
	ECLogger				*m_lpLogger;
	ECSyncSettings			*m_lpSettings;

	LPECCHANGEADVISOR		m_lpChangeAdvisor;
	LPECCHANGEADVISESINK	m_lpChangeAdviseSink;

	StatusStreamMap			m_mapSyncStatus;
	SyncStateMap			m_mapStates;
	NotifiedSyncIdMap		m_mapNotifiedSyncIds;

	pthread_mutex_t			m_hMutex;
};

#endif // ndef ECSYNCCONTEXT_H
