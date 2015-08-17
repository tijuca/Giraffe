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

#ifndef ECTHREADUTIL_H
#define ECTHREADUTIL_H

#include "zcdefs.h"
#include <pthread.h>

class CPthreadMutex _final {
public:
	CPthreadMutex(bool bRecurse = false);
	~CPthreadMutex();

	void Lock();
	void Unlock();

private:
	pthread_mutex_t m_hMutex;
};


class scoped_lock _final {
public:
	scoped_lock(pthread_mutex_t &mutex) : m_mutex(mutex) {
		pthread_mutex_lock(&m_mutex);
	}

	~scoped_lock() { 
		pthread_mutex_unlock(&m_mutex);
	}

private:
	// Make sure an object is not accidentally copied
	scoped_lock(const scoped_lock &);
	scoped_lock& operator=(const scoped_lock &);

	pthread_mutex_t &m_mutex;
};

#define WITH_LOCK(_lock)	\
	for (scoped_lock __lock(_lock), *ptr = NULL; ptr == NULL; ptr = &__lock)


template<int(*fnlock)(pthread_rwlock_t*)>
class scoped_rwlock _final {
public:
	scoped_rwlock(pthread_rwlock_t &rwlock) : m_rwlock(rwlock) {
		fnlock(&m_rwlock);
	}

	~scoped_rwlock() { 
		pthread_rwlock_unlock(&m_rwlock);
	}

private:
	// Make sure an object is not accidentally copied
	scoped_rwlock(const scoped_rwlock &);
	scoped_rwlock& operator=(const scoped_rwlock &);

	pthread_rwlock_t &m_rwlock;
};

typedef scoped_rwlock<pthread_rwlock_wrlock> scoped_exclusive_rwlock;
typedef scoped_rwlock<pthread_rwlock_rdlock> scoped_shared_rwlock;

#endif //#ifndef ECTHREADUTIL_H
