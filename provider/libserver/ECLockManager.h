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

#ifndef ECLockManager_INCLUDED
#define ECLockManager_INCLUDED

#include "ZarafaCode.h"

#include <map>
#include <pthread.h>
#include <boost/smart_ptr.hpp>

class ECLockManager;
class ECObjectLockImpl;

typedef boost::shared_ptr<ECLockManager> ECLockManagerPtr;

///////////////
// ECObjectLock
///////////////
class ECObjectLock {
public:
	ECObjectLock();
	ECObjectLock(ECLockManagerPtr ptrLockManager, unsigned int ulObjId, ECSESSIONID sessionId);
	ECObjectLock(const ECObjectLock &other);

	ECObjectLock& operator=(const ECObjectLock &other);
	void swap(ECObjectLock &other);

	ECRESULT Unlock();

private:
	typedef boost::shared_ptr<ECObjectLockImpl> ImplPtr;
	ImplPtr	m_ptrImpl;
};

///////////////////////
// ECObjectLock inlines
///////////////////////
inline ECObjectLock::ECObjectLock() {}

inline ECObjectLock::ECObjectLock(const ECObjectLock &other): m_ptrImpl(other.m_ptrImpl) {}

inline ECObjectLock& ECObjectLock::operator=(const ECObjectLock &other) {
	if (&other != this) {
		ECObjectLock tmp(other);
		swap(tmp);
	}
	return *this;
}

inline void ECObjectLock::swap(ECObjectLock &other) {
	m_ptrImpl.swap(other.m_ptrImpl);
}



////////////////
// ECLockManager
////////////////
class ECLockManager : public boost::enable_shared_from_this<ECLockManager> {
public:
	static ECLockManagerPtr Create();
	~ECLockManager();

	ECRESULT LockObject(unsigned int ulObjId, ECSESSIONID sessionId, ECObjectLock *lpOjbectLock);
	ECRESULT UnlockObject(unsigned int ulObjId, ECSESSIONID sessionId);
	bool IsLocked(unsigned int ulObjId, ECSESSIONID *lpSessionId);

private:
	ECLockManager();

private:
	// Map object ids to session IDs.
	typedef std::map<unsigned int, ECSESSIONID>	LockMap;

	pthread_rwlock_t	m_hRwLock;
	LockMap				m_mapLocks;
};

#endif // ndef ECLockManager_INCLUDED
