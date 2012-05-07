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

#include <mapicode.h>
#include <mapix.h>

#include "ECNotifyMaster.h"
#include "ECSessionGroupManager.h"
#include "SessionGroupData.h"
#include "SSLUtil.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif


/* std::algorithm helper structures/functions */
struct findSessionGroupId
{
	ECSESSIONGROUPID ecSessionGroupId;

	findSessionGroupId(ECSESSIONGROUPID ecSessionGroupId) : ecSessionGroupId(ecSessionGroupId)
	{
	}

	bool operator()(SESSIONGROUPMAP::value_type &entry)
	{
		return entry.second->GetSessionGroupId() == ecSessionGroupId;
	}
};

/* Global SessionManager for entire client */
ECSessionGroupManager g_ecSessionManager;

ECSessionGroupManager::ECSessionGroupManager()
{
	pthread_mutexattr_init(&m_hMutexAttrib);
	pthread_mutexattr_settype(&m_hMutexAttrib, PTHREAD_MUTEX_RECURSIVE);

	pthread_mutex_init(&m_hMutex, &m_hMutexAttrib);
}

ECSessionGroupManager::~ECSessionGroupManager()
{
	pthread_mutex_destroy(&m_hMutex);
	pthread_mutexattr_destroy(&m_hMutexAttrib);
}

ECSESSIONGROUPID ECSessionGroupManager::GetSessionGroupId(const sGlobalProfileProps &sProfileProps)
{
	std::pair<SESSIONGROUPIDMAP::iterator, bool> result;
	ECSESSIONGROUPID ecSessionGroupId;

	pthread_mutex_lock(&m_hMutex);

    ECSessionGroupInfo ecSessionGroup = ECSessionGroupInfo(sProfileProps.strServerPath, sProfileProps.strProfileName);

	result = m_mapSessionGroupIds.insert(SESSIONGROUPIDMAP::value_type(ecSessionGroup, 0));
	if (result.second == true) {
        // Not found, generate one now
    	ssl_random((sizeof(ecSessionGroupId) == 8), &ecSessionGroupId);
		// Register the new SessionGroupId, this is needed because we are not creating a SessionGroupData
		// object yet, and thus we are not putting anything in the m_mapSessionGroups yet. To prevent 2
		// threads to obtain 2 different SessionGroup ID's for the same server & profile combination we
		// use this seperate map containing SessionGroup ID's.
		result.first->second = ecSessionGroupId;
	}
	else {
		ecSessionGroupId = result.first->second;
	}

	pthread_mutex_unlock(&m_hMutex);

	return ecSessionGroupId;
}

/*
 * Threading comment:
 *
 * This function is safe since we hold the sessiongroup list lock, and we AddRef() the object before releasing the sessiongroup list
 * lock. This means that any Release can run at any time; if a release on the sessiongroup happens during the running of this function,
 * we are simply upping the refcount by 1, possibly leaving the refcount at 1 (the release in the other thread leaving the refcount at 0, does nothing)
 *
 * The object cannot be destroyed during the function since the deletion is done in DeleteSessionGroupDataIfOrphan requires the same lock
 * and that is the only place the object can be deleted.
 */
HRESULT ECSessionGroupManager::GetSessionGroupData(ECSESSIONGROUPID ecSessionGroupId, const sGlobalProfileProps &sProfileProps, SessionGroupData **lppData)
{
	HRESULT hr = hrSuccess;
	ECSessionGroupInfo ecSessionGroup = ECSessionGroupInfo(sProfileProps.strServerPath, sProfileProps.strProfileName);
	SessionGroupData *lpData = NULL;
	std::pair<SESSIONGROUPMAP::iterator, bool> result;

	pthread_mutex_lock(&m_hMutex);

	result = m_mapSessionGroups.insert(SESSIONGROUPMAP::value_type(ecSessionGroup, NULL));
	if (result.second == true) {
        hr = SessionGroupData::Create(ecSessionGroupId, &ecSessionGroup, sProfileProps, &lpData);
        if (hr == hrSuccess)
			result.first->second = lpData;
		else
			m_mapSessionGroups.erase(result.first);
	} else {
		lpData = result.first->second;
		lpData->AddRef();
	}

	pthread_mutex_unlock(&m_hMutex);

	*lppData = lpData;

	return hr;
}

HRESULT ECSessionGroupManager::DeleteSessionGroupDataIfOrphan(ECSESSIONGROUPID ecSessionGroupId)
{
	HRESULT hr = hrSuccess;
	SessionGroupData *lpSessionGroupData = NULL;

	pthread_mutex_lock(&m_hMutex);

	SESSIONGROUPMAP::iterator iter = find_if(m_mapSessionGroups.begin(), m_mapSessionGroups.end(), findSessionGroupId(ecSessionGroupId));
    if (iter != m_mapSessionGroups.end()) {
        if(iter->second->IsOrphan()) {
            // If the group is an orphan now, we can delete it safely since the only way
            // a new session would connect to the sessiongroup would be through us, and we 
            // hold the mutex.
            lpSessionGroupData = iter->second;
            m_mapSessionGroups.erase(iter);
        }
    }

	pthread_mutex_unlock(&m_hMutex);
	
	// Delete the object outside the lock; we can do this because nobody can access this group
	// now (since it is not in the map anymore), and the delete() will cause a pthread_join(), 
	// which could be blocked by the m_hMutex.
	if(lpSessionGroupData)
		delete lpSessionGroupData;

	return hr;
}
