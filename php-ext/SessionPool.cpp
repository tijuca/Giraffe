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

#include "SessionPool.h"

/*
 * How the session pooling works
 * =============================
 *
 * Basically, we have a list of active sessions. Each active session (Session object)
 * contains a 'tag' (an identifier referring to, say, the name of a profile), and
 * a valid IMAPISession pointer.
 *
 * When a new session is created (int main.cpp through the mapi_logon_* functions), a new
 * session object is created and added to the pool. The SessionPool class manages these 
 * sessions and controls their lifetime. Basic lifetime rules:
 *
 * - A Session can never die while it is being used in a running page request (ie ulRef > 0)
 * - Multiple threads *can* use the same session object (ie ulRef > 1)
 * - When too many sessions are active, we kill an unused session (ie ulRef == 0)
 * - When too many sessions are active and they are *all* locked (this would mean that there would
 *   be N simultaneous page requests, each of them using a different session object, where N
 *   is our maximum session count), we add an extra session anyway (instead of waiting for a
 *   session slot to come free)
 *
 * The SessionPool object is thread-safe:
 * - All actions are mutex-locked (and will never take very long!)
 * 
 */

#undef NO_LOCKING

#ifdef NO_LOCKING
#define WaitForSingleObject
#define ReleaseMutex
#endif

SessionPool::SessionPool(ULONG ulPoolSize, ULONG ulLifeTime) {
	lstSession = new std::list<Session *>;

	this->ulPoolSize = ulPoolSize;
	this->ulLifeTime = ulLifeTime;
	pthread_mutex_init(&this->hMutex, NULL);
}

// we assume not threads will be doing anything else to this object
// when the destructor is called
SessionPool::~SessionPool() {
	std::list<Session *>::iterator iterSession;
	
	if(lstSession) {
		// Delete all the list items
		for(iterSession = lstSession->begin(); iterSession != lstSession->end(); iterSession++) {
			delete *iterSession;
		}

		// Delete the list itself
		delete lstSession;
	}

	pthread_mutex_destroy(&hMutex);
}

// Return TRUE when we had to remove another session, else FALSE
BOOL SessionPool::AddSession(Session *lpSession) {
	BOOL fRemoved = FALSE;
	
	pthread_mutex_lock(&hMutex);

	if(lstSession->size() >= ulPoolSize) {
		// We have a full pool, decide which session to kill

		// For now, just the first non-locked session
		std::list<Session *>::iterator iterSession;
		
		for(iterSession = lstSession->begin(); iterSession != lstSession->end(); iterSession++) {
			if(!(*iterSession)->IsLocked()) {
				delete *iterSession;
				lstSession->erase(iterSession);
				fRemoved = TRUE;
				break;
			}
		}

		// If everything was locked, we add a session anyway ! (this is quite ok, better than waiting for a slot to come free)

	}

	lstSession->push_back(lpSession);

	pthread_mutex_unlock(&hMutex);

	return fRemoved;
}

// Search through our pool of sessions to see if we have the session
// identified by sTag in our pool already, if so, return the session,
// else return NULL
Session * SessionPool::GetSession(SessionTag *sTag) {
	std::list<Session *>::iterator iterSession;
	Session *lpSession = NULL;
	
	pthread_mutex_lock(&hMutex);

	// Delete all the list items
	for(iterSession = lstSession->begin(); iterSession != lstSession->end(); iterSession++) {
		if((*iterSession)->IsEqual(sTag)) {
			lpSession = *iterSession;
			break;
		}
	}

	if(lpSession) {
		if (lpSession->GetAge() >= ulLifeTime) {
			delete *iterSession;
			lstSession->erase(iterSession);
			lpSession = NULL;
		} else {
			lpSession->Lock();
		}
	}

	pthread_mutex_unlock(&hMutex);

	return lpSession;
}


ULONG SessionPool::GetPoolSize()
{
	ULONG ulSize = 0;

	pthread_mutex_lock(&hMutex);
	ulSize = lstSession->size();
	pthread_mutex_unlock(&hMutex);

	return ulSize;
}

ULONG SessionPool::GetLocked()
{
	std::list<Session *>::iterator iterSession;
	ULONG ulLocked = 0;

	pthread_mutex_lock(&hMutex);

	for(iterSession = lstSession->begin(); iterSession != lstSession->end(); iterSession++) {
		if((*iterSession)->IsLocked())
			ulLocked++;
	}

	pthread_mutex_unlock(&hMutex);

	return ulLocked;
}
