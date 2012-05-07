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
#include "ECLockManager.h"
#include "threadutil.h"

#include <boost/utility.hpp>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

using namespace std;

///////////////////
// ECObjectLockImpl
///////////////////
class ECObjectLockImpl : private boost::noncopyable {
public:
	ECObjectLockImpl(ECLockManagerPtr ptrLockManager, unsigned int ulObjId, ECSESSIONID sessionId);
	~ECObjectLockImpl();

	ECRESULT Unlock();

private:
	boost::weak_ptr<ECLockManager> m_ptrLockManager;
	unsigned int m_ulObjId;
	ECSESSIONID m_sessionId;
};

//////////////////////////////////
// ECLockObjectImpl Implementation
//////////////////////////////////
ECObjectLockImpl::ECObjectLockImpl(ECLockManagerPtr ptrLockManager, unsigned int ulObjId, ECSESSIONID sessionId)
: m_ptrLockManager(ptrLockManager)
, m_ulObjId(ulObjId)
, m_sessionId(sessionId)
{ }

ECObjectLockImpl::~ECObjectLockImpl() {
	Unlock();
}

ECRESULT ECObjectLockImpl::Unlock() {
	ECRESULT er = erSuccess;

	ECLockManagerPtr ptrLockManager = m_ptrLockManager.lock();
	if (ptrLockManager) {
		er = ptrLockManager->UnlockObject(m_ulObjId, m_sessionId);
		if (er == erSuccess)
			m_ptrLockManager.reset();
	}

	return er;
}



//////////////////////////////
// ECLockObject Implementation
//////////////////////////////
ECObjectLock::ECObjectLock(ECLockManagerPtr ptrLockManager, unsigned int ulObjId, ECSESSIONID sessionId)
: m_ptrImpl(new ECObjectLockImpl(ptrLockManager, ulObjId, sessionId))
{ }

ECRESULT ECObjectLock::Unlock() {
	ECRESULT er = erSuccess;

	er = m_ptrImpl->Unlock();
	if (er == erSuccess)
		m_ptrImpl.reset();

	return er;
}



///////////////////////////////
// ECLockManager Implementation
///////////////////////////////
ECLockManagerPtr ECLockManager::Create() {
	return ECLockManagerPtr(new ECLockManager());
}

ECLockManager::ECLockManager() {
	pthread_rwlock_init(&m_hRwLock, NULL);
}

ECLockManager::~ECLockManager() {
	pthread_rwlock_destroy(&m_hRwLock);
}

ECRESULT ECLockManager::LockObject(unsigned int ulObjId, ECSESSIONID sessionId, ECObjectLock *lpObjectLock)
{
	ECRESULT er = erSuccess;
	pair<LockMap::iterator, bool> res;
	scoped_exclusive_rwlock lock(m_hRwLock);

	res = m_mapLocks.insert(LockMap::value_type(ulObjId, sessionId));
	if (res.second == false && res.first->second != sessionId)
		er = ZARAFA_E_NO_ACCESS;

	if (lpObjectLock)
		*lpObjectLock = ECObjectLock(shared_from_this(), ulObjId, sessionId);
	
	return er;
}

ECRESULT ECLockManager::UnlockObject(unsigned int ulObjId, ECSESSIONID sessionId)
{
	ECRESULT er = erSuccess;
	LockMap::iterator i;
	scoped_exclusive_rwlock lock(m_hRwLock);

	i = m_mapLocks.find(ulObjId);
	if (i == m_mapLocks.end())
		er = ZARAFA_E_NOT_FOUND;
	else if (i->second != sessionId)
		er = ZARAFA_E_NO_ACCESS;
	else
		m_mapLocks.erase(i);

	return er;
}

bool ECLockManager::IsLocked(unsigned int ulObjId, ECSESSIONID *lpSessionId)
{
	LockMap::const_iterator i;
	
	scoped_shared_rwlock lock(m_hRwLock);
	i = m_mapLocks.find(ulObjId);
	if (i != m_mapLocks.end() && lpSessionId)
		*lpSessionId = i->second;

	return i != m_mapLocks.end();
}
