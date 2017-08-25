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
#include <utility>
#include <kopano/ECScheduler.h>
#include <kopano/lockhelper.hpp>
#include <cerrno>
#include <sys/time.h> /* gettimeofday */

#define SCHEDULER_POLL_FREQUENCY	5

namespace KC {

ECScheduler::ECScheduler(ECLogger *lpLogger) :
	m_lpLogger(lpLogger)
{
	m_lpLogger->AddRef();
	//Create Scheduler thread
	pthread_create(&m_hMainThread, NULL, ScheduleThread, (void*)this);
	set_thread_name(m_hMainThread, "ECScheduler:main");
}

ECScheduler::~ECScheduler(void)
{
	ulock_normal l_exit(m_hExitMutex);
	m_bExit = TRUE;
	m_hExitSignal.notify_one();
	l_exit.unlock();
	pthread_join(m_hMainThread, NULL);

	//Clean up something
	m_lpLogger->Release();
}

HRESULT ECScheduler::AddSchedule(eSchedulerType eType, unsigned int ulBeginCycle, void* (*lpFunction)(void*), void* lpData)
{
	scoped_rlock l_sched(m_hSchedulerMutex);

	if (lpFunction == NULL)
		return E_INVALIDARG;

	ECSCHEDULE sECSchedule;
	sECSchedule.eType = eType;
	sECSchedule.ulBeginCycle = ulBeginCycle;
	sECSchedule.lpFunction = lpFunction;
	sECSchedule.lpData = lpData;
	sECSchedule.tLastRunTime = 0;
	m_listScheduler.push_back(std::move(sECSchedule));
	return S_OK;
}

bool ECScheduler::hasExpired(time_t ttime, ECSCHEDULE *lpSchedule)
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
	HRESULT*			lperThread = NULL;
	pthread_t			hThread;

	time_t				ttime;

	if (lpScheduler == NULL)
		return NULL;

	while(TRUE)
	{
		// Wait for a terminate signal or return after a few minutes
		ulock_normal l_exit(lpScheduler->m_hExitMutex);
		if (lpScheduler->m_bExit)
			break;
		if (lpScheduler->m_hExitSignal.wait_for(l_exit, std::chrono::seconds(SCHEDULER_POLL_FREQUENCY)) !=
		    std::cv_status::timeout)
			break;
		l_exit.unlock();

		for (auto &sl : lpScheduler->m_listScheduler) {
			ulock_rec l_sched(lpScheduler->m_hSchedulerMutex);

			//TODO If load on server high, check only items with a high priority

			time(&ttime);
 
			if (hasExpired(ttime, &sl)) {
				//Create task thread
				int err = 0;
				
				if((err = pthread_create(&hThread, NULL, sl.lpFunction, static_cast<void *>(sl.lpData))) != 0) {
				    lpScheduler->m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to spawn new thread: %s", strerror(err));
					goto task_fail;
				}

				set_thread_name(hThread, "ECScheduler:worker");

				sl.tLastRunTime = ttime;

				if((err = pthread_join(hThread, (void**)&lperThread)) != 0) {
				    lpScheduler->m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to join thread: %s", strerror(err));
					goto task_fail;
				}

				delete lperThread;
				lperThread = NULL;
			}

 task_fail:
			l_sched.unlock();
			// check for a exit signal
			l_exit.lock();
			if(lpScheduler->m_bExit) {
				l_exit.unlock();
				break;
			}
			l_exit.unlock();
		}
	}

	return NULL;
}

} /* namespace */
