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
#include "ECShortTermEntryIDManager.h"
#include "ECCacheManager.h"
#include "soapStub.h"
#include "Zarafa.h"

#include <algorithm>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/**
 * Frees the entryid memory that was allocated in ECCacheManager::GetEntryIdFromObject.
 * 
 * @param[in]	eid		The entryid structure who's __ptr member will be freed.
 */
inline void ECShortTermEntryIDManager::freeEid(IdToEidMap::value_type &eid) {
	delete[] eid.second.__ptr;
}

/**
 * Compare two entryid structures and determine if the left-hand-side compares less than
 * the right-hand-side.
 * 
 * @param[in]	lhs		The left-hand-side entryid.
 * @param[in]	rhs		The right-hand-side entryid.
 * 
 * @retval	true	The length of the left-hand-side entryid is less than the length
 * 					of the right-hand-side entryid or the length of the two entryids
 * 					is equal and a binary compare of the left-hand-side entryid compares
 * 					less than the right-hand-side entryid.
 * @retval	false	Otherwise.
 */
inline bool ECShortTermEntryIDManager::compareLessEid::operator ()(const entryId &lhs, const entryId &rhs) const {
	return lhs.__size < rhs.__size || (lhs.__size == rhs.__size && memcmp(lhs.__ptr, rhs.__ptr, lhs.__size) < 0);
}

ECShortTermEntryIDManager::ECShortTermEntryIDManager() {
	pthread_mutex_init(&m_hMutex, NULL);
}

ECShortTermEntryIDManager::~ECShortTermEntryIDManager() {
	pthread_mutex_lock(&m_hMutex);

	std::for_each(m_mapIdToEid.begin(), m_mapIdToEid.end(), &freeEid);
	m_mapIdToEid.clear();
	m_mapEidToId.clear();

	pthread_mutex_unlock(&m_hMutex);
	pthread_mutex_destroy(&m_hMutex);
}

/**
 * Get a short term entryid for the object identified by ulObjId.
 * 
 * @param[in]		ulObjId			The object id of the object for which to return the
 * 									short term entryid.
 * @param[in]		lpCacheManager	Pointer to the cache manager. The cache manager is required
 * 									to obtain the real entryid for the object, which in turn is
 * 									required to get the object type and store guid. Without the
 * 									correct store guid certain client side checks would fail on
 * 									short term entryids.
 * @param[in,out]	lpEntryId		Pointer to an entryId structure, which will be filled with
 * 									the short term entryid for the object. Memory returned from
 * 									this method must not be freed.
 *
 * @retval	ZARAFA_E_NOT_FOUND		No object was found with ulObjId as its id.
 */
ECRESULT ECShortTermEntryIDManager::GetEntryIdForObjectId(unsigned int ulObjId, ECCacheManager *lpCacheManager, entryId *lpEntryId) {
	ECRESULT	er = erSuccess;
	entryId		nullEid = {NULL, 0};

	pthread_mutex_lock(&m_hMutex);

	std::pair<IdToEidMap::iterator, bool> res = m_mapIdToEid.insert(IdToEidMap::value_type(ulObjId, nullEid));
	if (res.second == true) {
		EID *lpEid = NULL;
		
		er = lpCacheManager->GetEntryIdFromObject(ulObjId, NULL, &res.first->second);
		if (er != erSuccess) {
			m_mapIdToEid.erase(res.first);
			goto exit;
		}
			
		lpEid = (EID*)res.first->second.__ptr;
		if (lpEid->ulVersion == 0) {
			// Create a new EntryID.
			EID_V0 *lpOldEid = (EID_V0*)lpEid;

			res.first->second.__size = CbNewEID("");
			res.first->second.__ptr = new BYTE[res.first->second.__size];
			memset(res.first->second.__ptr, 0, res.first->second.__size);

			lpEid = (EID*)res.first->second.__ptr;
			memcpy(lpEid->abFlags, lpOldEid->abFlags, sizeof(lpEid->abFlags));
			lpEid->guid = lpOldEid->guid;
			lpEid->ulType = lpOldEid->ulType;
			lpEid->ulVersion = 1;

			// Cleanup
			delete[] lpOldEid;
		}
		
		// update abFlags to mark it as short term.
		lpEid->abFlags[0] = 0xff;
		
		// Update unique id.
		CoCreateGuid(&lpEid->uniqueId);

		// Update reverse map.
		m_mapEidToId[res.first->second] = ulObjId;
	}

	*lpEntryId = res.first->second;	// Lazy copy

exit:
	pthread_mutex_unlock(&m_hMutex);

	return er;
}

/**
 * Get the object id of the object specified by the provided short term entryid.
 * 
 * @param[in]	lpEntryId		The short term entryid for which to find the actual object id.
 * @param[out]	lpulObjId		Pointer to an unsigned int that will be set to the correct object id.
 * 
 * @retval	ZARAFA_E_NOT_FOUND	The short term entryid is unknown.
 */
ECRESULT ECShortTermEntryIDManager::GetObjectIdFromEntryId(const entryId *lpEntryId, unsigned int *lpulObjId) {
	ECRESULT	er = erSuccess;

	pthread_mutex_lock(&m_hMutex);

	EidToIdMap::iterator iter = m_mapEidToId.find(*lpEntryId);
	if (iter == m_mapEidToId.end()) {
		er = ZARAFA_E_NOT_FOUND;
		goto exit;
	}

	*lpulObjId = iter->second;

exit:
	pthread_mutex_unlock(&m_hMutex);

	return er;
}

/**
 * Check if an entryid is a short term entryid.
 * 
 * @param[in]	lpEntryId	The entryid to check,
 * @param[out]	lpbResult	Pointer to the boolean that will be set to the result.
 * 
 * @retval	ZARAFA_E_INVALID_PARAMETER	lpEntryID or lpbResult is NULL.
 * @retval	ZARAFA_E_INVALID_ENTRYID	lpEntryID contains a NULL pointer to its internal storage.
 */
ECRESULT ECShortTermEntryIDManager::IsShortTermEntryId(const entryId *lpEntryId, bool *lpbResult) {
	ECRESULT	er = erSuccess;

	if (lpEntryId == NULL || lpbResult == NULL) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	if (lpEntryId->__ptr == NULL) {
		er = ZARAFA_E_INVALID_ENTRYID;
		goto exit;
	}
	
	if (lpEntryId->__size < (int)CbNewEID(""))
		*lpbResult = false;
	else {
		const EID *lpEid = (const EID*)lpEntryId->__ptr;
		*lpbResult = (lpEid->abFlags[0] == 0xff);
	}

exit:
	return er;
}
