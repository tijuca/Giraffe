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
#include "ECScheduler.h"
// ETIMEDOUT in linux is in errno, windows has this though pthread.h
#include <errno.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define SCHEDULER_POLL_FREQUENCY	5

ECScheduler::ECScheduler(ECLogger *lpLogger)
{
	m_bExit = FALSE;
	m_lpLogger = lpLogger;

	// Create a mutex with no initial owner.
	pthread_mutexattr_t mattr;
	pthread_mutexattr_init(&mattr);
	pthread_mutexattr_settype(&mattr, PTHREAD_MUTEX_RECURSIVE);

	pthread_mutex_init(&m_hSchedulerMutex, &mattr);
	pthread_mutex_init(&m_hExitMutex, NULL);

	//Terminate Event

	pthread_cond_init(&m_hExitSignal, NULL);
	//Create Scheduler thread
	pthread_create(&m_hMainThread, NULL, ScheduleThread, (void*)this);
}

ECScheduler::~ECScheduler(void)
{
	pthread_mutex_lock( &m_hExitMutex);
	m_bExit = TRUE;
	pthread_cond_signal(&m_hExitSignal);

	pthread_mutex_unlock(&m_hExitMutex);
	pthread_join(m_hMainThread, NULL);

	// Lock whole Scheduler
	//pthread_mutex_lock(&m_hSchedulerMutex);
	
	//Clean up something

	// Unlock whole Scheduler
	//pthread_mutex_unlock(&m_hSchedulerMutex);

	// Remove pthread things
	pthread_mutex_destroy(&m_hSchedulerMutex);
	pthread_mutex_destroy(&m_hExitMutex);
	pthread_cond_destroy(&m_hExitSignal);
}

HRESULT ECScheduler::AddSchedule(eSchedulerType eType, unsigned int ulBeginCycle, void* (*lpFunction)(void*), void* lpData)
{
	HRESULT	hr = S_OK;
	ECSCHEDULE	sECSchedule;

	// Lock whole Scheduler
	pthread_mutex_lock(&m_hSchedulerMutex);

	if(lpFunction == NULL) {
		hr = E_INVALIDARG;
		goto exit;
	}

	sECSchedule.eType = eType;
	sECSchedule.ulBeginCycle = ulBeginCycle;
	sECSchedule.lpFunction = lpFunction;
	sECSchedule.lpData = lpData;
	sECSchedule.tLastRunTime = 0;

	m_listScheduler.push_back(sECSchedule);

exit:
	// Unlock whole Scheduler
	pthread_mutex_unlock(&m_hSchedulerMutex);

	return hr;
}

bool ECScheduler::hasExpired(time_t ttime, LPECSCHEDULE lpSchedule)
{
	struct tm tmLastRunTime;
	struct tm tmtime;

	localtime_r(&ttime, &tmtime);

	if(lpSchedule->tLastRunTime > 0)
		localtime_r(&lpSchedule->tLastRunTime, &tmLastRunTime);
	else
		memset(&tmLastRunTime, 0, sizeof(tmLastRunTime));

	switch (lpSchedule->eType) {
	case SCHEDULE_SECONDS:
		return
			(((tmLastRunTime.tm_min != tmtime.tm_min) ||
			  ((tmLastRunTime.tm_min == tmtime.tm_min) &&
			   (tmLastRunTime.tm_sec != tmtime.tm_sec))) &&
			 ((tmtime.tm_sec == (int)lpSchedule->ulBeginCycle) ||
			  ((lpSchedule->ulBeginCycle > 0) &&
			   ((tmtime.tm_sec % (int)lpSchedule->ulBeginCycle) < SCHEDULER_POLL_FREQUENCY))));
	case SCHEDULE_MINUTES:
		return
			(((tmLastRunTime.tm_hour != tmtime.tm_hour) ||
			  ((tmLastRunTime.tm_hour == tmtime.tm_hour) &&
			   (tmLastRunTime.tm_min != tmtime.tm_min))) &&
			 ((tmtime.tm_min == (int)lpSchedule->ulBeginCycle) ||
			  ((lpSchedule->ulBeginCycle > 0) &&
			   ((tmtime.tm_min % (int)lpSchedule->ulBeginCycle) == 0))));
	case SCHEDULE_HOUR:
		return
			((tmLastRunTime.tm_hour != tmtime.tm_hour) &&
			 ((int)lpSchedule->ulBeginCycle >= tmtime.tm_min) &&
			 ((int)lpSchedule->ulBeginCycle <= (tmtime.tm_min + 2)));
	case SCHEDULE_DAY:
		return
			((tmLastRunTime.tm_mday != tmtime.tm_mday) &&
			 ((int)lpSchedule->ulBeginCycle == tmtime.tm_hour));
	case SCHEDULE_MONTH:
		return
			((tmLastRunTime.tm_mon != tmtime.tm_mon) &&
			 ((int)lpSchedule->ulBeginCycle == tmtime.tm_mday));
	case SCHEDULE_NONE:
		return false;
	}

	return false;
}

void* ECScheduler::ScheduleThread(void* lpTmpScheduler)
{
	ECScheduleList::iterator	iterScheduleList;

	ECScheduler*		lpScheduler = (ECScheduler*)lpTmpScheduler;
	int					lResult;
	HRESULT*			lperThread = NULL;
	struct timeval		now;
	struct timespec		timeout;
	pthread_t			hThread;

	time_t				ttime;

	if(lpScheduler == NULL) {
	    // Do not pthread_exit() because linuxthreads is broken and will not free any objects
		// pthread_exit((void*)-1);
		return 0;
    }

	while(TRUE)
	{
		// Wait for a terminate signal or return after a few minutes
		pthread_mutex_lock(&lpScheduler->m_hExitMutex);
		if(lpScheduler->m_bExit) {
			pthread_mutex_unlock(&lpScheduler->m_hExitMutex);
			break;
		}

		gettimeofday(&now,NULL); // null==timezone
		timeout.tv_sec = now.tv_sec + SCHEDULER_POLL_FREQUENCY;
		timeout.tv_nsec = now.tv_usec * 1000;

		lResult = pthread_cond_timedwait(&lpScheduler->m_hExitSignal, &lpScheduler->m_hExitMutex, &timeout);
		if (lResult != ETIMEDOUT) {
			pthread_mutex_unlock(&lpScheduler->m_hExitMutex);
			break;
		}
		pthread_mutex_unlock(&lpScheduler->m_hExitMutex);

		for(iterScheduleList = lpScheduler->m_listScheduler.begin(); iterScheduleList != lpScheduler->m_listScheduler.end(); iterScheduleList++)
		{
			pthread_mutex_lock(&lpScheduler->m_hSchedulerMutex);

			//TODO If load on server high, check only items with a high priority

			time(&ttime);
 
			if (hasExpired(ttime, &(*iterScheduleList))) {
				//Create task thread
				int err = 0;
				
				if((err = pthread_create(&hThread, NULL, iterScheduleList->lpFunction, (void*)iterScheduleList->lpData)) != 0) {
				    lpScheduler->m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to spawn new thread: %s", strerror(err));
                    continue;
                }

				iterScheduleList->tLastRunTime = ttime;

				if((err = pthread_join(hThread, (void**)&lperThread)) != 0) {
				    lpScheduler->m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to join thread: %s", strerror(err));
				    continue;
				}

				if(lperThread){delete lperThread; lperThread = NULL;}
			}

			pthread_mutex_unlock(&lpScheduler->m_hSchedulerMutex);

			// check for a exit signal
			pthread_mutex_lock(&lpScheduler->m_hExitMutex);
			if(lpScheduler->m_bExit) {
				pthread_mutex_unlock(&lpScheduler->m_hExitMutex);
				break;
			}
			pthread_mutex_unlock(&lpScheduler->m_hExitMutex);
		}
	}

	// Do not pthread_exit() because linuxthreads is broken and will not free any objects
    // pthread_exit(0);

    return NULL;
}
