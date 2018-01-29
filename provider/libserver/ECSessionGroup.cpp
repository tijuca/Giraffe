/*
 * Copyright 2005 - 2016 Zarafa and its licensors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License, version 3,
 * as published by the Free Software Foundation.
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

#include <kopano/platform.h>

#include <mapidefs.h>
#include <mapitags.h>

#include <algorithm>
#include <kopano/lockhelper.hpp>

#include "ECSession.h"
#include "ECSessionGroup.h"
#include "ECSessionManager.h"
#include "SOAPUtils.h"

namespace KC {

class FindChangeAdvise {
public:
	FindChangeAdvise(ECSESSIONID ulSession, unsigned int ulConnection)
		: m_ulSession(ulSession)
		, m_ulConnection(ulConnection)
	{ }

	bool operator()(const CHANGESUBSCRIBEMAP::value_type &rhs) const
	{
		return rhs.second.ulSession == m_ulSession && rhs.second.ulConnection == m_ulConnection;
	}

private:
	ECSESSIONID		m_ulSession;
	unsigned int	m_ulConnection;
};

ECSessionGroup::ECSessionGroup(ECSESSIONGROUPID sessionGroupId,
    ECSessionManager *lpSessionManager) :
	m_sessionGroupId(sessionGroupId), m_lpSessionManager(lpSessionManager)
{
}

ECSessionGroup::~ECSessionGroup()
{
	/* Unsubscribe any subscribed stores */
	for (const auto &p : m_mapSubscribedStores)
		m_lpSessionManager->UnsubscribeObjectEvents(p.second, m_sessionGroupId);
}

void ECSessionGroup::Lock()
{
	/* Increase our refcount by one */
	scoped_lock lock(m_hThreadReleasedMutex);
	++m_ulRefCount;
}

void ECSessionGroup::Unlock()
{
	// Decrease our refcount by one, signal ThreadReleased if RefCount == 0
	scoped_lock lock(m_hThreadReleasedMutex);
	--m_ulRefCount;
	if (!IsLocked())
		m_hThreadReleased.notify_one();
}

void ECSessionGroup::AddSession(ECSession *lpSession)
{
	scoped_rlock lock(m_hSessionMapLock);
	m_mapSessions.emplace(lpSession->GetSessionId(), sessionInfo(lpSession));
}

void ECSessionGroup::ReleaseSession(ECSession *lpSession)
{
	ulock_rec l_map(m_hSessionMapLock);
	m_mapSessions.erase(lpSession->GetSessionId());
	l_map.unlock();

	scoped_lock l_note(m_hNotificationLock);
	for (auto i = m_mapSubscribe.cbegin(); i != m_mapSubscribe.cend(); ) {
		if (i->second.ulSession != lpSession->GetSessionId()) {
			++i;
			continue;
		}
		auto iRemove = i;
		++i;
		m_mapSubscribe.erase(iRemove);
	}
}

void ECSessionGroup::ShutdownSession(ECSession *lpSession)
{
    /* This session is used to get the notifications, stop GetNotifyItems() */
    if (m_getNotifySession == lpSession->GetSessionId())
        releaseListeners();
}

bool ECSessionGroup::isOrphan()
{
	scoped_rlock lock(m_hSessionMapLock);
	return m_mapSessions.empty();
}

void ECSessionGroup::UpdateSessionTime()
{
	scoped_rlock lock(m_hSessionMapLock);
	for (const auto &i : m_mapSessions)
		i.second.lpSession->UpdateSessionTime();
}

ECRESULT ECSessionGroup::AddAdvise(ECSESSIONID ulSessionId, unsigned int ulConnection, unsigned int ulKey, unsigned int ulEventMask)
{
	ECRESULT		hr = erSuccess;
	subscribeItem	sSubscribeItem;

	sSubscribeItem.ulSession	= ulSessionId;
	sSubscribeItem.ulConnection	= ulConnection;
	sSubscribeItem.ulKey		= ulKey;
	sSubscribeItem.ulEventMask	= ulEventMask;

	{
		scoped_lock lock(m_hNotificationLock);
		m_mapSubscribe.emplace(ulConnection, sSubscribeItem);
	}
	
	if(ulEventMask & (fnevNewMail | fnevObjectModified | fnevObjectCreated | fnevObjectCopied | fnevObjectDeleted | fnevObjectMoved)) {
		// Object and new mail notifications should be subscribed at the session manager
		unsigned int ulStore = 0;

		m_lpSessionManager->GetCacheManager()->GetStore(ulKey, &ulStore, NULL);
		m_lpSessionManager->SubscribeObjectEvents(ulStore, this->m_sessionGroupId);
		
		scoped_lock lock(m_mutexSubscribedStores);
		m_mapSubscribedStores.emplace(ulKey, ulStore);
	}

	return hr;
}

ECRESULT ECSessionGroup::AddChangeAdvise(ECSESSIONID ulSessionId, unsigned int ulConnection, notifySyncState *lpSyncState)
{
	changeSubscribeItem sSubscribeItem = {ulSessionId, ulConnection};

	if (lpSyncState == NULL)
		return KCERR_INVALID_PARAMETER;

	sSubscribeItem.sSyncState = *lpSyncState;

	scoped_lock lock(m_hNotificationLock);
	m_mapChangeSubscribe.emplace(lpSyncState->ulSyncId, sSubscribeItem);
	return erSuccess;
}

ECRESULT ECSessionGroup::DelAdvise(ECSESSIONID ulSessionId, unsigned int ulConnection)
{
	scoped_lock lock(m_hNotificationLock);
	auto iterSubscription = m_mapSubscribe.find(ulConnection);
	if (iterSubscription == m_mapSubscribe.cend()) {
		// Apparently the connection was used for change notifications.
		auto iterItem = find_if(m_mapChangeSubscribe.cbegin(),
			m_mapChangeSubscribe.cend(),
			FindChangeAdvise(ulSessionId, ulConnection));
		if (iterItem != m_mapChangeSubscribe.cend())
			m_mapChangeSubscribe.erase(iterItem);
	} else {
		if(iterSubscription->second.ulEventMask & (fnevObjectModified | fnevObjectCreated | fnevObjectCopied | fnevObjectDeleted | fnevObjectMoved)) {
			// Object notification - remove our subscription to the store
			scoped_lock lock(m_mutexSubscribedStores);
			// Find the store that the key was subscribed for
			auto iterSubscribed = m_mapSubscribedStores.find(iterSubscription->second.ulKey);
			if (iterSubscribed != m_mapSubscribedStores.cend()) {
				// Unsubscribe the store
				m_lpSessionManager->UnsubscribeObjectEvents(iterSubscribed->second, this->m_sessionGroupId);
				// Remove from our list
				m_mapSubscribedStores.erase(iterSubscribed);
			} else
				assert(false); // Unsubscribe for something that was not subscribed
		}
		m_mapSubscribe.erase(iterSubscription);
	}
	return hrSuccess;
}

ECRESULT ECSessionGroup::AddNotification(notification *notifyItem, unsigned int ulKey, unsigned int ulStore, ECSESSIONID ulSessionId)
{
	ulock_normal l_note(m_hNotificationLock);
	ECNotification notify(*notifyItem);

	for (const auto &i : m_mapSubscribe) {
		if ((ulSessionId != 0 && ulSessionId != i.second.ulSession) ||
		    (ulKey != i.second.ulKey && i.second.ulKey != ulStore) ||
			!(notifyItem->ulEventType & i.second.ulEventMask))
				continue;
		notify.SetConnection(i.second.ulConnection);
		m_listNotification.emplace_back(notify);
	}
	l_note.unlock();
	
	// Since we now have a notification ready to send, tell the session manager that we have something to send. Since
	// a notification can be read from any session in the session group, we have to notify all of the sessions
	scoped_rlock l_ses(m_hSessionMapLock);
	for (const auto &p : m_mapSessions)
		m_lpSessionManager->NotifyNotificationReady(p.second.lpSession->GetSessionId());
	return erSuccess;
}

ECRESULT ECSessionGroup::AddNotificationTable(ECSESSIONID ulSessionId, unsigned int ulType, unsigned int ulObjType, unsigned int ulTableId,
											  sObjectTableKey* lpsChildRow, sObjectTableKey* lpsPrevRow, struct propValArray *lpRow)
{
	Lock();
	auto lpNotify = s_alloc<notification>(nullptr);
	memset(lpNotify, 0, sizeof(notification));
	lpNotify->tab = s_alloc<notificationTable>(nullptr);
	memset(lpNotify->tab, 0, sizeof(notificationTable));
	
	lpNotify->ulEventType			= fnevTableModified;
	lpNotify->tab->ulTableEvent		= ulType;

	if(lpsChildRow && (lpsChildRow->ulObjId > 0 || lpsChildRow->ulOrderId > 0)) {
		lpNotify->tab->propIndex.ulPropTag = PR_INSTANCE_KEY;
		lpNotify->tab->propIndex.__union = SOAP_UNION_propValData_bin;
		lpNotify->tab->propIndex.Value.bin = s_alloc<xsd__base64Binary>(nullptr);
		lpNotify->tab->propIndex.Value.bin->__ptr = s_alloc<unsigned char>(nullptr, sizeof(ULONG) * 2);
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
		lpNotify->tab->propPrior.Value.bin = s_alloc<xsd__base64Binary>(nullptr);
		lpNotify->tab->propPrior.Value.bin->__ptr = s_alloc<unsigned char>(nullptr, sizeof(ULONG) * 2);
		lpNotify->tab->propPrior.Value.bin->__size = sizeof(ULONG)*2;

		memcpy(lpNotify->tab->propPrior.Value.bin->__ptr, &lpsPrevRow->ulObjId, sizeof(ULONG));
		memcpy(lpNotify->tab->propPrior.Value.bin->__ptr+sizeof(ULONG), &lpsPrevRow->ulOrderId, sizeof(ULONG));

	}else {
		lpNotify->tab->propPrior.__union = SOAP_UNION_propValData_ul;
		lpNotify->tab->propPrior.ulPropTag = PR_NULL;
	}
	
	lpNotify->tab->ulObjType = ulObjType;

	if(lpRow) {
		lpNotify->tab->pRow = s_alloc<propValArray>(nullptr);
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
	return erSuccess;
}

ECRESULT ECSessionGroup::AddChangeNotification(const std::set<unsigned int> &syncIds, unsigned int ulChangeId, unsigned int ulChangeType)
{
	notification notifyItem{__gszeroinit};
	notificationICS ics{__gszeroinit};
	entryId syncStateBin;
	notifySyncState	syncState;
	std::map<ECSESSIONID,unsigned int> mapInserted;

	syncState.ulChangeId = ulChangeId;
	notifyItem.ulEventType = fnevKopanoIcsChange;
	notifyItem.ics = &ics;
	notifyItem.ics->pSyncState = &syncStateBin;
	notifyItem.ics->pSyncState->__size = sizeof(syncState);
	notifyItem.ics->pSyncState->__ptr = (unsigned char*)&syncState;
	notifyItem.ics->ulChangeType = ulChangeType;

	Lock();
	ulock_normal l_note(m_hNotificationLock);
	// Iterate through all sync ids
	for (auto sync_id : syncIds) {
		// Iterate through all subscribed clients for the current sync id
		auto iterRange = m_mapChangeSubscribe.equal_range(sync_id);
		for (auto iterItem = iterRange.first;
		     iterItem != iterRange.second; ++iterItem) {
			// update sync state
			syncState.ulSyncId = sync_id;
			
			// create ECNotification
			ECNotification notify(notifyItem);
			notify.SetConnection(iterItem->second.ulConnection);
			m_listNotification.emplace_back(notify);
			mapInserted[iterItem->second.ulSession]++;
		}
	}
	l_note.unlock();

	// Since we now have a notification ready to send, tell the session manager that we have something to send. Since
	// a notifications can be read from any session in the session group, we have to notify all of the sessions
	ulock_rec l_ses(m_hSessionMapLock);
	for (const auto &p : m_mapSessions)
		m_lpSessionManager->NotifyNotificationReady(p.second.lpSession->GetSessionId());
	l_ses.unlock();
	Unlock();
	return erSuccess;
}

ECRESULT ECSessionGroup::AddChangeNotification(ECSESSIONID ulSessionId, unsigned int ulConnection, unsigned int ulSyncId, unsigned long ulChangeId)
{
	notification notifyItem{__gszeroinit};
	notificationICS ics{__gszeroinit};
	entryId syncStateBin;
	notifySyncState	syncState;

	syncState.ulSyncId = ulSyncId;
	syncState.ulChangeId = ulChangeId;
	notifyItem.ulEventType = fnevKopanoIcsChange;
	notifyItem.ics = &ics;
	notifyItem.ics->pSyncState = &syncStateBin;
	notifyItem.ics->pSyncState->__size = sizeof(syncState);
	notifyItem.ics->pSyncState->__ptr = (unsigned char*)&syncState;

	Lock();
	ulock_normal l_note(m_hNotificationLock);
	// create ECNotification
	ECNotification notify(notifyItem);
	notify.SetConnection(ulConnection);
	m_listNotification.emplace_back(notify);
	l_note.unlock();

	// Since we now have a notification ready to send, tell the session manager that we have something to send. Since
	// a notifications can be read from any session in the session group, we have to notify all of the sessions
	ulock_rec l_ses(m_hSessionMapLock);
	for (const auto &p : m_mapSessions)
		m_lpSessionManager->NotifyNotificationReady(p.second.lpSession->GetSessionId());
	l_ses.unlock();
	Unlock();
	return erSuccess;
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
	ulock_normal l_note(m_hNotificationLock);

	/* May still be nothing in there, as the signal is also fired when we should exit */
	if (!m_listNotification.empty()) {
		ULONG ulSize = (ULONG)m_listNotification.size();

		notifications->pNotificationArray = s_alloc<notificationArray>(soap);
		notifications->pNotificationArray->__ptr = s_alloc<notification>(soap, ulSize);
		notifications->pNotificationArray->__size = ulSize;

		size_t nPos = 0;
		for (const auto i : m_listNotification)
			i.GetCopy(soap, notifications->pNotificationArray->__ptr[nPos++]);
		m_listNotification.clear();
	} else {
	    er = KCERR_NOT_FOUND;
    }
	l_note.unlock();

	/* Reset GetNotifySession */
	m_getNotifySession = 0;

	Unlock();

	return er;
}

ECRESULT ECSessionGroup::releaseListeners()
{
	scoped_lock lock(m_hNotificationLock);
	m_bExit = true;
	m_hNewNotificationEvent.notify_all();
	return erSuccess;
}

/**
 * Get object size
 *
 * @return Object size in bytes
 */
size_t ECSessionGroup::GetObjectSize(void)
{
	size_t ulSize = 0;
	ulock_normal l_note(m_hNotificationLock);

	ulSize += MEMORY_USAGE_MAP(m_mapSubscribe.size(), SUBSCRIBEMAP);
	ulSize += MEMORY_USAGE_MAP(m_mapChangeSubscribe.size(), CHANGESUBSCRIBEMAP);

	size_t ulItems = 0;
	for (const auto &n : m_listNotification) {
		++ulItems;
		ulSize += n.GetObjectSize();
	}
	ulSize += MEMORY_USAGE_LIST(ulItems, ECNOTIFICATIONLIST);
	l_note.unlock();

	ulSize += sizeof(*this);

	ulock_rec l_ses(m_hSessionMapLock);
	ulSize += MEMORY_USAGE_MAP(m_mapSessions.size(), SESSIONINFOMAP);
	l_ses.unlock();

	ulock_normal l_sub(m_mutexSubscribedStores);
	ulSize += MEMORY_USAGE_MULTIMAP(m_mapSubscribedStores.size(), SUBSCRIBESTOREMULTIMAP);
	l_sub.unlock();
	return ulSize;
}

} /* namespace */
