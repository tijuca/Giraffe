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

#include <mapidefs.h>
#include <mapitags.h>

#include <algorithm>

#include "ECSession.h"
#include "ECSessionGroup.h"
#include "ECSessionManager.h"
#include "SOAPUtils.h"
#include "ECShortTermEntryIDManager.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

class FindChangeAdvise
{
public:
	FindChangeAdvise(ECSESSIONID ulSession, unsigned int ulConnection)
		: m_ulSession(ulSession)
		, m_ulConnection(ulConnection)
	{ }

	bool operator()(const CHANGESUBSCRIBEMAP::value_type &rhs) {
		return rhs.second.ulSession == m_ulSession && rhs.second.ulConnection == m_ulConnection;
	}

private:
	ECSESSIONID		m_ulSession;
	unsigned int	m_ulConnection;
};

ECSessionGroup::ECSessionGroup(ECSESSIONGROUPID sessionGroupId, ECSessionManager *lpSessionManager)
{
	m_sessionGroupId = sessionGroupId;

	m_dblLastQueryTime = 0;
	m_getNotifySession = 0;
	m_bExit = false;
	m_lpSessionManager = lpSessionManager;

	m_lpShortTermEntryIDManager = new ECShortTermEntryIDManager;

	m_ulRefCount = 0;
	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE_NP);
	
	pthread_mutex_init(&m_hThreadReleasedMutex, NULL);
	pthread_cond_init(&m_hThreadReleased, NULL);

	pthread_mutex_init(&m_hNotificationLock, NULL);
	pthread_cond_init(&m_hNewNotificationEvent, NULL);
	
	pthread_mutex_init(&m_hSessionMapLock, &attr);
	
	pthread_mutex_init(&m_mutexSubscribedStores, NULL);
	
	pthread_mutexattr_destroy(&attr);
}

ECSessionGroup::~ECSessionGroup()
{
	/* Unsubscribe any subscribed stores */
	std::multimap<unsigned int, unsigned int>::iterator i;
	for(i=m_mapSubscribedStores.begin(); i != m_mapSubscribedStores.end(); i++) {
		m_lpSessionManager->UnsubscribeObjectEvents(i->second, m_sessionGroupId);
	}

	delete m_lpShortTermEntryIDManager;

	/* Release any GetNotifyItems() threads */
	pthread_mutex_destroy(&m_hNotificationLock);
	pthread_cond_destroy(&m_hNewNotificationEvent);

	pthread_mutex_destroy(&m_hThreadReleasedMutex);
	pthread_cond_destroy(&m_hThreadReleased);
	
	pthread_mutex_destroy(&m_hSessionMapLock);
	pthread_mutex_destroy(&m_mutexSubscribedStores);
}

void ECSessionGroup::Lock()
{
	/* Increase our refcount by one */
	pthread_mutex_lock(&m_hThreadReleasedMutex);
	m_ulRefCount++;
	pthread_mutex_unlock(&m_hThreadReleasedMutex);
}

void ECSessionGroup::Unlock()
{
	// Decrease our refcount by one, signal ThreadReleased if RefCount == 0
	pthread_mutex_lock(&m_hThreadReleasedMutex);
	m_ulRefCount--;
	if (!IsLocked())
		pthread_cond_signal(&m_hThreadReleased);
	pthread_mutex_unlock(&m_hThreadReleasedMutex);
}

bool ECSessionGroup::IsLocked()
{
	return m_ulRefCount > 0;
}

ECSESSIONGROUPID ECSessionGroup::GetSessionGroupId()
{
	return m_sessionGroupId;
}

void ECSessionGroup::AddSession(ECSession *lpSession)
{
	pthread_mutex_lock(&m_hSessionMapLock);
	m_mapSessions.insert(SESSIONINFOMAP::value_type(lpSession->GetSessionId(), sessionInfo(lpSession)));
	pthread_mutex_unlock(&m_hSessionMapLock);
}

void ECSessionGroup::ReleaseSession(ECSession *lpSession)
{
	SUBSCRIBEMAP::iterator i, iRemove;

	pthread_mutex_lock(&m_hSessionMapLock);
	m_mapSessions.erase(lpSession->GetSessionId());
	pthread_mutex_unlock(&m_hSessionMapLock);

	pthread_mutex_lock(&m_hNotificationLock);

	for (i = m_mapSubscribe.begin(); i != m_mapSubscribe.end(); ) {
		if (i->second.ulSession != lpSession->GetSessionId()) {
			i++;
			continue;
		}

		iRemove = i;
		i++;

		m_mapSubscribe.erase(iRemove);

	}

	pthread_mutex_unlock(&m_hNotificationLock);
}

void ECSessionGroup::ShutdownSession(ECSession *lpSession)
{
    /* This session is used to get the notifications, stop GetNotifyItems() */
    if (m_getNotifySession == lpSession->GetSessionId())
        releaseListeners();
}

bool ECSessionGroup::isOrphan()
{
    bool bOrphan = false;
	pthread_mutex_lock(&m_hSessionMapLock);
	bOrphan = m_mapSessions.empty();
	pthread_mutex_unlock(&m_hSessionMapLock);
	
	return bOrphan;
}

void ECSessionGroup::UpdateSessionTime()
{
	pthread_mutex_lock(&m_hSessionMapLock);
	for (SESSIONINFOMAP::iterator i = m_mapSessions.begin(); i != m_mapSessions.end(); i++)
		i->second.lpSession->UpdateSessionTime();
	pthread_mutex_unlock(&m_hSessionMapLock);
}

ECRESULT ECSessionGroup::AddAdvise(ECSESSIONID ulSessionId, unsigned int ulConnection, unsigned int ulKey, unsigned int ulEventMask)
{
	ECRESULT		hr = erSuccess;
	subscribeItem	sSubscribeItem;

	sSubscribeItem.ulSession	= ulSessionId;
	sSubscribeItem.ulConnection	= ulConnection;
	sSubscribeItem.ulKey		= ulKey;
	sSubscribeItem.ulEventMask	= ulEventMask;

	pthread_mutex_lock(&m_hNotificationLock);

	m_mapSubscribe.insert(SUBSCRIBEMAP::value_type(ulConnection, sSubscribeItem));

	pthread_mutex_unlock(&m_hNotificationLock);
	
	if(ulEventMask & (fnevNewMail | fnevObjectModified | fnevObjectCreated | fnevObjectCopied | fnevObjectDeleted | fnevObjectMoved)) {
		// Object and new mail notifications should be subscribed at the session manager
		unsigned int ulStore = 0;

		m_lpSessionManager->GetCacheManager()->GetStore(ulKey, &ulStore, NULL);
		m_lpSessionManager->SubscribeObjectEvents(ulStore, this->m_sessionGroupId);
		
		pthread_mutex_lock(&m_mutexSubscribedStores);
		m_mapSubscribedStores.insert(std::make_pair(ulKey, ulStore));
		pthread_mutex_unlock(&m_mutexSubscribedStores);
	}

	return hr;
}

ECRESULT ECSessionGroup::AddChangeAdvise(ECSESSIONID ulSessionId, unsigned int ulConnection, notifySyncState *lpSyncState)
{
	ECRESULT			er = erSuccess;
	changeSubscribeItem sSubscribeItem = {ulSessionId, ulConnection, {0}};

	if (lpSyncState == NULL) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	sSubscribeItem.sSyncState = *lpSyncState;

	pthread_mutex_lock(&m_hNotificationLock);

	m_mapChangeSubscribe.insert(CHANGESUBSCRIBEMAP::value_type(lpSyncState->ulSyncId, sSubscribeItem));

	pthread_mutex_unlock(&m_hNotificationLock);

exit:
	return er;
}

ECRESULT ECSessionGroup::DelAdvise(ECSESSIONID ulSessionId, unsigned int ulConnection)
{
	ECRESULT		hr = erSuccess;

	CHANGESUBSCRIBEMAP::iterator iterItem;
	SUBSCRIBEMAP::iterator iterSubscription;
	std::multimap<unsigned int, unsigned int>::iterator iterSubscribed;
	
	pthread_mutex_lock(&m_hNotificationLock);

	iterSubscription = m_mapSubscribe.find(ulConnection);

	if (iterSubscription == m_mapSubscribe.end()) {
		// Apparently the connection was used for change notifications.
		iterItem = find_if(m_mapChangeSubscribe.begin(), m_mapChangeSubscribe.end(), FindChangeAdvise(ulSessionId, ulConnection));
		if (iterItem != m_mapChangeSubscribe.end())
			m_mapChangeSubscribe.erase(iterItem);
	} else {
		if(iterSubscription->second.ulEventMask & (fnevObjectModified | fnevObjectCreated | fnevObjectCopied | fnevObjectDeleted | fnevObjectMoved)) {
			// Object notification - remove our subscription to the store
			pthread_mutex_lock(&m_mutexSubscribedStores);
			// Find the store that the key was subscribed for
			iterSubscribed = m_mapSubscribedStores.find(iterSubscription->second.ulKey);
			if(iterSubscribed != m_mapSubscribedStores.end()) {
				// Unsubscribe the store
				m_lpSessionManager->UnsubscribeObjectEvents(iterSubscribed->second, this->m_sessionGroupId);
				// Remove from our list
				m_mapSubscribedStores.erase(iterSubscribed);
			} else
				ASSERT(false); // Unsubscribe for something that was not subscribed
			pthread_mutex_unlock(&m_mutexSubscribedStores);
		}
		m_mapSubscribe.erase(iterSubscription);

	}
	
	pthread_mutex_unlock(&m_hNotificationLock);

	return hr;
}

ECRESULT ECSessionGroup::AddNotification(notification *notifyItem, unsigned int ulKey, unsigned int ulStore, ECSESSIONID ulSessionId)
{
	ECRESULT		hr = erSuccess;
	SESSIONINFOMAP::iterator iterSessions;
	
	pthread_mutex_lock(&m_hNotificationLock);

	ECNotification notify(*notifyItem);

	for (SUBSCRIBEMAP::iterator i = m_mapSubscribe.begin(); i != m_mapSubscribe.end(); i++) {
		if ((ulSessionId && ulSessionId != i->second.ulSession) ||
		    (ulKey != i->second.ulKey && i->second.ulKey != ulStore) ||
			!(notifyItem->ulEventType & i->second.ulEventMask))
				continue;

		notify.SetConnection(i->second.ulConnection);

		m_listNotification.push_back(notify);
	}

	pthread_mutex_unlock(&m_hNotificationLock);
	
	// Since we now have a notification ready to send, tell the session manager that we have something to send. Since
	// a notification can be read from any session in the session group, we have to notify all of the sessions
	pthread_mutex_lock(&m_hSessionMapLock);
	for(iterSessions = m_mapSessions.begin(); iterSessions != m_mapSessions.end(); iterSessions++) {
    	m_lpSessionManager->NotifyNotificationReady(iterSessions->second.lpSession->GetSessionId());
    }
    
	pthread_mutex_unlock(&m_hSessionMapLock);

	return hr;
}

ECRESULT ECSessionGroup::AddNotificationTable(ECSESSIONID ulSessionId, unsigned int ulType, unsigned int ulObjType, unsigned int ulTableId,
											  sObjectTableKey* lpsChildRow, sObjectTableKey* lpsPrevRow, struct propValArray *lpRow)
{
	ECRESULT hr = erSuccess;

	Lock();

	struct notification *lpNotify = new struct notification;
	memset(lpNotify, 0, sizeof(notification));

	lpNotify->tab = new notificationTable;
	memset(lpNotify->tab, 0, sizeof(notificationTable));
	
	lpNotify->ulEventType			= fnevTableModified;
	lpNotify->tab->ulTableEvent		= ulType;


	if(lpsChildRow && (lpsChildRow->ulObjId > 0 || lpsChildRow->ulOrderId > 0)) {
		lpNotify->tab->propIndex.ulPropTag = PR_INSTANCE_KEY;
		lpNotify->tab->propIndex.__union = SOAP_UNION_propValData_bin;
		lpNotify->tab->propIndex.Value.bin = new struct xsd__base64Binary;
		lpNotify->tab->propIndex.Value.bin->__ptr = new unsigned char[sizeof(ULONG)*2];
		lpNotify->tab->propIndex.Value.bin->__size = sizeof(ULONG)*2;

		memcpy(lpNotify->tab->propIndex.Value.bin->__ptr, &lpsChildRow->ulObjId, sizeof(ULONG));
		memcpy(lpNotify->tab->propIndex.Value.bin->__ptr+sizeof(ULONG), &lpsChildRow->ulOrderId, sizeof(ULONG));
	}else {
		lpNotify->tab->propIndex.ulPropTag = PR_NULL;
		lpNotify->tab->propIndex.__union = SOAP_UNION_propValData_ul;
	}

	if(lpsPrevRow && (lpsPrevRow->ulObjId > 0 || lpsPrevRow->ulOrderId > 0))
	{
		lpNotify->tab->propPrior.ulPropTag = PR_INSTANCE_KEY;
		lpNotify->tab->propPrior.__union = SOAP_UNION_propValData_bin;
		lpNotify->tab->propPrior.Value.bin = new struct xsd__base64Binary;
		lpNotify->tab->propPrior.Value.bin->__ptr = new unsigned char[sizeof(ULONG)*2];
		lpNotify->tab->propPrior.Value.bin->__size = sizeof(ULONG)*2;

		memcpy(lpNotify->tab->propPrior.Value.bin->__ptr, &lpsPrevRow->ulObjId, sizeof(ULONG));
		memcpy(lpNotify->tab->propPrior.Value.bin->__ptr+sizeof(ULONG), &lpsPrevRow->ulOrderId, sizeof(ULONG));

	}else {
		lpNotify->tab->propPrior.__union = SOAP_UNION_propValData_ul;
		lpNotify->tab->propPrior.ulPropTag = PR_NULL;
	}
	
	lpNotify->tab->ulObjType = ulObjType;

	if(lpRow) {
		lpNotify->tab->pRow = new struct propValArray;
		lpNotify->tab->pRow->__ptr = lpRow->__ptr;
		lpNotify->tab->pRow->__size = lpRow->__size;
	}

	AddNotification(lpNotify, ulTableId, 0, ulSessionId);

	//Free by lpRow
	if(lpNotify->tab->pRow){
		lpNotify->tab->pRow->__ptr = NULL;
		lpNotify->tab->pRow->__size = 0;
	}

	//Free struct
	FreeNotificationStruct(lpNotify);

	Unlock();

	return hr;
}

ECRESULT ECSessionGroup::AddChangeNotification(const std::set<unsigned int> &syncIds, unsigned int ulChangeId, unsigned int ulChangeType)
{
	ECRESULT		er = erSuccess;
	notification	notifyItem = {0};
	notificationICS	ics = {0};
	entryId			syncStateBin = {0};
	notifySyncState	syncState = {0, ulChangeId};
	SESSIONINFOMAP::iterator iterSessions;

	std::map<ECSESSIONID,unsigned int> mapInserted;
	std::map<ECSESSIONID,unsigned int>::const_iterator iterInserted;
	std::set<unsigned int>::const_iterator iterSyncId;
	CHANGESUBSCRIBEMAP::const_iterator iterItem;
	std::pair<CHANGESUBSCRIBEMAP::iterator, CHANGESUBSCRIBEMAP::iterator> iterRange;

	notifyItem.ulEventType = fnevZarafaIcsChange;
	notifyItem.ics = &ics;
	notifyItem.ics->pSyncState = &syncStateBin;
	notifyItem.ics->pSyncState->__size = sizeof(syncState);
	notifyItem.ics->pSyncState->__ptr = (unsigned char*)&syncState;
	notifyItem.ics->ulChangeType = ulChangeType;

	Lock();
	pthread_mutex_lock(&m_hNotificationLock);

	// Iterate through all sync ids
	for (iterSyncId = syncIds.begin(); iterSyncId != syncIds.end(); ++iterSyncId) {

		// Iterate through all subscribed clients for the current sync id
		iterRange = m_mapChangeSubscribe.equal_range(*iterSyncId);
		for (iterItem = iterRange.first; iterItem != iterRange.second; ++iterItem) {
			// update sync state
			syncState.ulSyncId = *iterSyncId;
			
			// create ECNotification
			ECNotification notify(notifyItem);
			notify.SetConnection(iterItem->second.ulConnection);

			m_listNotification.push_back(notify);
			mapInserted[iterItem->second.ulSession]++;
		}
	}

	pthread_mutex_unlock(&m_hNotificationLock);

	// Since we now have a notification ready to send, tell the session manager that we have something to send. Since
	// a notifications can be read from any session in the session group, we have to notify all of the sessions
	pthread_mutex_lock(&m_hSessionMapLock);
	for(iterSessions = m_mapSessions.begin(); iterSessions != m_mapSessions.end(); iterSessions++) {
    	m_lpSessionManager->NotifyNotificationReady(iterSessions->second.lpSession->GetSessionId());
    }
    
	pthread_mutex_unlock(&m_hSessionMapLock);

	Unlock();

	return er;
}

ECRESULT ECSessionGroup::AddChangeNotification(ECSESSIONID ulSessionId, unsigned int ulConnection, unsigned int ulSyncId, unsigned long ulChangeId)
{
	ECRESULT		er = erSuccess;
	notification	notifyItem = {0};
	notificationICS	ics = {0};
	entryId			syncStateBin = {0};
	notifySyncState	syncState = {ulSyncId, ulChangeId};
	SESSIONINFOMAP::iterator iterSessions;

	notifyItem.ulEventType = fnevZarafaIcsChange;
	notifyItem.ics = &ics;
	notifyItem.ics->pSyncState = &syncStateBin;
	notifyItem.ics->pSyncState->__size = sizeof(syncState);
	notifyItem.ics->pSyncState->__ptr = (unsigned char*)&syncState;

	Lock();
	pthread_mutex_lock(&m_hNotificationLock);

	// create ECNotification
	ECNotification notify(notifyItem);
	notify.SetConnection(ulConnection);

	m_listNotification.push_back(notify);

	pthread_mutex_unlock(&m_hNotificationLock);

	// Since we now have a notification ready to send, tell the session manager that we have something to send. Since
	// a notifications can be read from any session in the session group, we have to notify all of the sessions
	pthread_mutex_lock(&m_hSessionMapLock);
	for(iterSessions = m_mapSessions.begin(); iterSessions != m_mapSessions.end(); iterSessions++) {
    	m_lpSessionManager->NotifyNotificationReady(iterSessions->second.lpSession->GetSessionId());
    }
    
	pthread_mutex_unlock(&m_hSessionMapLock);

	Unlock();

	return er;
}

ECRESULT ECSessionGroup::GetNotifyItems(struct soap *soap, ECSESSIONID ulSessionId, struct notifyResponse *notifications)
{
	ECRESULT		er = erSuccess;

	/* Start waiting for notifications */
	Lock();

	/*
	 * Store the session which requested the notifications.
	 * We need this in case the session is removed and the
	 * session must release all calls into ECSessionGroup.
	 */
	m_getNotifySession = ulSessionId;

	/*
	 * Update Session times for all sessions attached to this group.
	 * This prevents any of the sessions to timeout while it was waiting
	 * for notifications for the group.
	 */
	UpdateSessionTime();

	memset(notifications, 0,  sizeof(notifyResponse));
	pthread_mutex_lock(&m_hNotificationLock);

	/* May still be nothing in there, as the signal is also fired when we should exit */
	if (!m_listNotification.empty()) {
		ULONG ulSize = (ULONG)m_listNotification.size();

		notifications->pNotificationArray = s_alloc<notificationArray>(soap);
		notifications->pNotificationArray->__ptr = s_alloc<notification>(soap, ulSize);
		notifications->pNotificationArray->__size = ulSize;

		int nPos = 0;
		for (NOTIFICATIONLIST::iterator i = m_listNotification.begin(); i != m_listNotification.end(); i++)
			i->GetCopy(soap, notifications->pNotificationArray->__ptr[nPos++]);

		m_listNotification.clear();
	} else {
	    er = ZARAFA_E_NOT_FOUND;
    }

	pthread_mutex_unlock(&m_hNotificationLock);

	/* Reset GetNotifySession */
	m_getNotifySession = 0;

	Unlock();

	return er;
}

ECRESULT ECSessionGroup::releaseListeners()
{
	ECRESULT hr = erSuccess;

	pthread_mutex_lock(&m_hNotificationLock);
	m_bExit = true;
	pthread_cond_broadcast(&m_hNewNotificationEvent);
	pthread_mutex_unlock(&m_hNotificationLock);

	return hr;
}

/**
 * Get object size
 *
 * @return Object size in bytes
 */
unsigned int ECSessionGroup::GetObjectSize()
{
	NOTIFICATIONLIST::iterator  iterlNotify;
	unsigned int ulSize = 0;

	pthread_mutex_lock(&m_hNotificationLock);

	ulSize += m_listNotification.size() * sizeof(NOTIFICATIONLIST::value_type);
	ulSize += m_mapSubscribe.size() * sizeof(SUBSCRIBEMAP::value_type);
	ulSize += m_mapChangeSubscribe.size() * sizeof(CHANGESUBSCRIBEMAP::value_type);

	for(iterlNotify = m_listNotification.begin(); iterlNotify != m_listNotification.end(); iterlNotify++)
	{
		ulSize += iterlNotify->GetObjectSize();
	}

	pthread_mutex_unlock(&m_hNotificationLock);

	ulSize += sizeof(*this);

	pthread_mutex_lock(&m_hSessionMapLock);
	ulSize += m_mapSessions.size() * sizeof(SESSIONINFOMAP::value_type);
	pthread_mutex_unlock(&m_hSessionMapLock);

	pthread_mutex_lock(&m_mutexSubscribedStores);
	ulSize += m_mapSubscribedStores.size() * sizeof(unsigned int);
	pthread_mutex_unlock(&m_mutexSubscribedStores);

	return ulSize;
}

/**
 * Get the STE manager for the current session group.
 */
ECShortTermEntryIDManager* ECSessionGroup::GetShortTermEntryIDManager()
{
	return m_lpShortTermEntryIDManager;
}
