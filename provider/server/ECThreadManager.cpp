/*
 * Copyright 2005 - 2009  Zarafa B.V.
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
#include "ECThreadManager.h"

#include <stdlib.h>
#include <algorithm>
#include <stringutil.h>

#ifdef HAVE_EPOLL_CREATE
#include <sys/epoll.h>
#endif

#include "CommonUtil.h"
#include "ECSessionManager.h"
#include "ECStatsCollector.h"
#include "ECServerEntrypoint.h"

// errors from stdsoap2.h, differs per gSOAP release
#define RETURN_CASE(x) \
	case x: \
		return #x;
string GetSoapError(int err)
{
	switch (err) {
		RETURN_CASE(SOAP_EOF)
		RETURN_CASE(SOAP_CLI_FAULT)
		RETURN_CASE(SOAP_SVR_FAULT)
		RETURN_CASE(SOAP_TAG_MISMATCH)
		RETURN_CASE(SOAP_TYPE)
		RETURN_CASE(SOAP_SYNTAX_ERROR)
		RETURN_CASE(SOAP_NO_TAG)
		RETURN_CASE(SOAP_IOB)
		RETURN_CASE(SOAP_MUSTUNDERSTAND)
		RETURN_CASE(SOAP_NAMESPACE)
		RETURN_CASE(SOAP_USER_ERROR)
		RETURN_CASE(SOAP_FATAL_ERROR)
		RETURN_CASE(SOAP_FAULT)
		RETURN_CASE(SOAP_NO_METHOD)
		RETURN_CASE(SOAP_NO_DATA)
		RETURN_CASE(SOAP_GET_METHOD)
		RETURN_CASE(SOAP_PUT_METHOD)
		RETURN_CASE(SOAP_DEL_METHOD)
		RETURN_CASE(SOAP_HEAD_METHOD)
		RETURN_CASE(SOAP_HTTP_METHOD)
		RETURN_CASE(SOAP_EOM)
		RETURN_CASE(SOAP_MOE)
		RETURN_CASE(SOAP_HDR)
		RETURN_CASE(SOAP_NULL)
		RETURN_CASE(SOAP_DUPLICATE_ID)
		RETURN_CASE(SOAP_MISSING_ID)
		RETURN_CASE(SOAP_HREF)
		RETURN_CASE(SOAP_UDP_ERROR)
		RETURN_CASE(SOAP_TCP_ERROR)
		RETURN_CASE(SOAP_HTTP_ERROR)
		RETURN_CASE(SOAP_SSL_ERROR)
		RETURN_CASE(SOAP_ZLIB_ERROR)
		RETURN_CASE(SOAP_DIME_ERROR)
		RETURN_CASE(SOAP_DIME_HREF)
		RETURN_CASE(SOAP_DIME_MISMATCH)
		RETURN_CASE(SOAP_DIME_END)
		RETURN_CASE(SOAP_MIME_ERROR)
		RETURN_CASE(SOAP_MIME_HREF)
		RETURN_CASE(SOAP_MIME_END)
		RETURN_CASE(SOAP_VERSIONMISMATCH)
		RETURN_CASE(SOAP_PLUGIN_ERROR)
		RETURN_CASE(SOAP_DATAENCODINGUNKNOWN)
		RETURN_CASE(SOAP_REQUIRED)
		RETURN_CASE(SOAP_PROHIBITED)
		RETURN_CASE(SOAP_OCCURS)
		RETURN_CASE(SOAP_LENGTH)
		RETURN_CASE(SOAP_FD_EXCEEDED)
	}
	return stringify(err);
}

int relocate_fd(int fd, ECLogger *lpLogger);

ECWorkerThread::ECWorkerThread(ECLogger *lpLogger, ECThreadManager *lpManager, ECDispatcher *lpDispatcher)
{
    m_lpLogger = lpLogger;
    m_lpManager = lpManager;
    m_lpDispatcher = lpDispatcher;
    
    
    if(pthread_create(&m_thread, NULL, ECWorkerThread::Work, this) != 0) {
        m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to start thread: %s", strerror(errno));
    } else {
        pthread_detach(m_thread);
    }
}

ECWorkerThread::~ECWorkerThread()
{
}

void *ECWorkerThread::Work(void *lpParam)
{
    ECWorkerThread *lpThis = (ECWorkerThread *)lpParam;
    WORKITEM *lpWorkItem = NULL;
    ECRESULT er = erSuccess;
    bool fStop = false;
	int err = 0;

    lpThis->m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Started thread %08x", (ULONG)pthread_self());
    
    while(1) {
        // Get the next work item, don't wait for new items
        if(lpThis->m_lpDispatcher->GetNextWorkItem(&lpWorkItem, false) != erSuccess) {
            // Nothing in the queue, notify that we're idle now
            lpThis->m_lpManager->NotifyIdle(lpThis, &fStop);
            
            // We were requested to exit due to idle state
            if(fStop) {
                lpThis->m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Thread %08x idle and requested to exit", (ULONG)pthread_self());
                break;
            }
                
            // Wait for next work item in the queue
            er = lpThis->m_lpDispatcher->GetNextWorkItem(&lpWorkItem, true);
            if(er != erSuccess) {
                // This could happen because we were waken up because we are exiting
                continue;
            }
        }

		// For SSL connections, we first must do the handshake and pass it back to the queue
		if (lpWorkItem->soap->ctx && !lpWorkItem->soap->ssl) {
			err = soap_ssl_accept(lpWorkItem->soap);
			if (err) {
				lpThis->m_lpLogger->Log(EC_LOGLEVEL_WARNING, "%s", soap_faultdetail(lpWorkItem->soap)[0]);
				lpThis->m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "%s: %s", GetSoapError(err).c_str(), soap_faultstring(lpWorkItem->soap)[0]);
			}
        } else {
			err = 0;

			// Record start of handling of this request
			double dblStart = GetTimeOfDay();

            // Do processing of work item
            soap_begin(lpWorkItem->soap);
            if(soap_begin_recv(lpWorkItem->soap)) {
                if(lpWorkItem->soap->error < SOAP_STOP) {
					// Client Updater returns 404 to the client to say it doesn't need to update, so skip this HTTP error
					if (lpWorkItem->soap->error != SOAP_EOF && lpWorkItem->soap->error != 404)
						lpThis->m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "gSOAP error on receiving request: %s", GetSoapError(lpWorkItem->soap->error).c_str());
                    soap_send_fault(lpWorkItem->soap);
                    goto done;
                }
                soap_closesock(lpWorkItem->soap);
                goto done;
            }

            // WARNING
            //
            // From the moment we call soap_serve_request, the soap object MAY be handled
            // by another thread. In this case, soap_serve_request() returns SOAP_NULL. We
            // can NOT rely on soap->error being this value since the other thread may already
            // have overwritten the error value.            
            if(soap_envelope_begin_in(lpWorkItem->soap)
              || soap_recv_header(lpWorkItem->soap)
              || soap_body_begin_in(lpWorkItem->soap)) 
            {
                err = lpWorkItem->soap->error;
            } else {
                try {
                    err = soap_serve_request(lpWorkItem->soap);
                } catch(int) {
                    // Reply processing is handled by the callee, totally ignore the rest of processing for this item
                    delete lpWorkItem;
                    continue;
                }
            }

            if(err)
            {
                lpThis->m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "gSOAP error on processing request: %s", GetSoapError(err).c_str());
                soap_send_fault(lpWorkItem->soap);
                goto done;
            }

done:	
			// Record statistics for number of requests and processing time
			g_lpStatsCollector->Increment(SCN_SOAP_REQUESTS);
		    g_lpStatsCollector->Increment(SCN_PROCESSING_TIME, (long long)((GetTimeOfDay() - dblStart) * 1000));
		    g_lpStatsCollector->Increment(SCN_RESPONSE_TIME, (long long)((GetTimeOfDay() - lpWorkItem->dblReceiveStamp) * 1000));

            // Clear memory used by soap calls. Note that this does not actually
            // undo our soap_new2() call so the soap object is still valid after these calls
            soap_destroy(lpWorkItem->soap);
            soap_end(lpWorkItem->soap);

        }
        // We're done processing the item, the workitem's socket is returned to the queue
        lpThis->m_lpDispatcher->NotifyDone(lpWorkItem->soap);
        delete lpWorkItem;
    }

	/** free ssl error data **/
	ERR_remove_state(0);

    // We're detached, so we should clean up ourselves
    delete lpThis;
    
    return NULL;
}

ECThreadManager::ECThreadManager(ECLogger *lpLogger, ECDispatcher *lpDispatcher, unsigned int ulThreads)
{
    m_lpLogger = lpLogger;
    m_lpDispatcher = lpDispatcher;
    m_ulThreads = ulThreads;

    pthread_mutex_init(&m_mutexThreads, NULL);
    
    // Start our worker threads
    for(unsigned int i=0;i<ulThreads;i++) {
        ECWorkerThread *lpWorker = new ECWorkerThread(m_lpLogger, this, lpDispatcher);
        m_lstThreads.push_back(lpWorker);
    }
}

ECThreadManager::~ECThreadManager()
{
    unsigned int ulThreads;
    
    std::list<ECWorkerThread *>::iterator iterThreads;

    // Wait for the threads to exit
    while(1) {
        pthread_mutex_lock(&m_mutexThreads);
        ulThreads = m_lstThreads.size();
        pthread_mutex_unlock(&m_mutexThreads);
        if(ulThreads > 0) {
            m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Still waiting for %d threads to exit", ulThreads);
            Sleep(1000);
        }
        else
            break;
    }    
    
    pthread_mutex_destroy(&m_mutexThreads);
}
    
ECRESULT ECThreadManager::ForceAddThread(int nThreads)
{
    pthread_mutex_lock(&m_mutexThreads);
    for(int i=0;i<nThreads;i++) 
        m_lstThreads.push_back(new ECWorkerThread(m_lpLogger, this, m_lpDispatcher));
    pthread_mutex_unlock(&m_mutexThreads);
    
    return erSuccess;
}

ECRESULT ECThreadManager::GetThreadCount(unsigned int *lpulThreads)
{
    unsigned int ulThreads;
    
    pthread_mutex_lock(&m_mutexThreads);
    ulThreads = m_lstThreads.size();
    pthread_mutex_unlock(&m_mutexThreads);
    
    *lpulThreads = ulThreads;
    
    return erSuccess;
}

ECRESULT ECThreadManager::SetThreadCount(unsigned int ulThreads)
{
    // If we're under the number of threads at the moment, start new ones
    pthread_mutex_lock(&m_mutexThreads);

    // Set the default thread count
    m_ulThreads = ulThreads;

    while(ulThreads > m_lstThreads.size())
        m_lstThreads.push_back(new ECWorkerThread(m_lpLogger, this, m_lpDispatcher));
    
    pthread_mutex_unlock(&m_mutexThreads);
    
    // If we are OVER the number of threads, then the code in NotifyIdle() will bring this down
    
    return erSuccess;
}

// Called by worker threads only when it is idle. This is the only place where the worker thread can be
// deleted.
ECRESULT ECThreadManager::NotifyIdle(ECWorkerThread *lpThread, bool *lpfStop)
{
    ECRESULT er = erSuccess;
    std::list<ECWorkerThread *>::iterator iterThreads;
    *lpfStop = false;
        
    pthread_mutex_lock(&m_mutexThreads);
    if(m_ulThreads < m_lstThreads.size()) {
        // We are currently running more threads than we want, so tell the thread to stop
        iterThreads = std::find(m_lstThreads.begin(), m_lstThreads.end(), lpThread);
        if(iterThreads == m_lstThreads.end()) {
            // HUH
            m_lpLogger->Log(EC_LOGLEVEL_FATAL, "A thread that we don't know is idle ...");
            er = ZARAFA_E_NOT_FOUND;
            goto exit;
        }
        // Remove the thread from our running thread list
        m_lstThreads.erase(iterThreads);
        
        // Tell the thread to exit. The thread will self-cleanup; we therefore needn't delete the object nor join with the running thread
        *lpfStop = true;
    }
            
exit:
    pthread_mutex_unlock(&m_mutexThreads);
        
    return er;
}

ECWatchDog::ECWatchDog(ECConfig *lpConfig, ECLogger *lpLogger, ECDispatcher *lpDispatcher, ECThreadManager *lpThreadManager)
{
    m_lpConfig = lpConfig;
    m_lpLogger = lpLogger;
    m_lpDispatcher = lpDispatcher;
    m_lpThreadManager = lpThreadManager;
    m_bExit = false;
    
    pthread_mutex_init(&m_mutexExit, NULL);
    pthread_cond_init(&m_condExit, NULL);
    
    if(pthread_create(&m_thread, NULL, ECWatchDog::Watch, this) != 0) {
        m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to start watchdog thread: %s", strerror(errno));
    }
}

ECWatchDog::~ECWatchDog()
{
    void *ret;
    
    pthread_mutex_lock(&m_mutexExit);
    m_bExit = true;
    pthread_cond_signal(&m_condExit);
    pthread_mutex_unlock(&m_mutexExit);
    
    pthread_join(m_thread, &ret);
    
    pthread_mutex_destroy(&m_mutexExit);
    pthread_cond_destroy(&m_condExit);
}

void *ECWatchDog::Watch(void *lpParam)
{
    ECWatchDog *lpThis = (ECWatchDog *)lpParam;
    double dblAge;
    struct timespec timeout;
    
    while(1) {
		if(lpThis->m_bExit == true)
			break;

        double dblMaxFreq = atoi(lpThis->m_lpConfig->GetSetting("watchdog_frequency"));
        double dblMaxAge = atoi(lpThis->m_lpConfig->GetSetting("watchdog_max_age")) / 1000.0;
        
        // If the age of the front item in the queue is older than the specified maximum age, force
        // a new thread to be started
        if(lpThis->m_lpDispatcher->GetFrontItemAge(&dblAge) == erSuccess && dblAge > dblMaxAge)
            lpThis->m_lpThreadManager->ForceAddThread(1);

        // Check to see if exit flag is set, and limit rate to dblMaxFreq Hz
        pthread_mutex_lock(&lpThis->m_mutexExit);
        if(lpThis->m_bExit == false) {
            double dblWait = 1/dblMaxFreq;
            clock_gettime(CLOCK_REALTIME, &timeout);
            timeout.tv_sec += dblWait;
            timeout.tv_nsec += 1000000000.0*(dblWait-floor(dblWait));
            pthread_cond_timedwait(&lpThis->m_condExit, &lpThis->m_mutexExit, &timeout);
            if(lpThis->m_bExit == true) {
                pthread_mutex_unlock(&lpThis->m_mutexExit);
                break;
            }
        }
        pthread_mutex_unlock(&lpThis->m_mutexExit);
    }
    
    return NULL;
}

ECDispatcher::ECDispatcher(ECLogger *lpLogger, ECConfig *lpConfig, CREATEPIPESOCKETCALLBACK lpCallback, void *lpParam)
{
    m_lpLogger = lpLogger;
    m_lpConfig = lpConfig;

	// Default socket settings
	m_nMaxKeepAlive = atoi(m_lpConfig->GetSetting("server_max_keep_alive_requests"));
	m_nRecvTimeout = atoi(m_lpConfig->GetSetting("server_recv_timeout"));
	m_nReadTimeout = atoi(m_lpConfig->GetSetting("server_read_timeout"));
	m_nSendTimeout = atoi(m_lpConfig->GetSetting("server_send_timeout"));
    
    m_ulIdle = 0;
	m_nPrioDone = 0;
        
    pthread_mutex_init(&m_mutexItems, NULL);
    pthread_mutex_init(&m_mutexSockets, NULL);
    pthread_mutex_init(&m_mutexIdle, NULL);
    pthread_cond_init(&m_condItems, NULL);
    
    m_bExit = false;
	m_lpCreatePipeSocketCallback = lpCallback;
	m_lpCreatePipeSocketParam = lpParam;
}

ECDispatcher::~ECDispatcher()
{
    // Cleanup
    pthread_mutex_destroy(&m_mutexItems);
    pthread_mutex_destroy(&m_mutexSockets);
    pthread_mutex_destroy(&m_mutexIdle);
    pthread_cond_destroy(&m_condItems);
}

ECRESULT ECDispatcher::GetThreadCount(unsigned int *lpulThreads, unsigned int *lpulIdleThreads)
{
    ECRESULT er = erSuccess;
    
    er = m_lpThreadManager->GetThreadCount(lpulThreads);
    if(er != erSuccess)
        goto exit;
        
    *lpulIdleThreads = m_ulIdle;
    
exit:
    return er;
}

// Get the age (in seconds) of the next-in-line item in the queue, or 0 if the queue is empty
ECRESULT ECDispatcher::GetFrontItemAge(double *lpdblAge)
{
    double dblNow = GetTimeOfDay();
    double dblAge = 0;
    
    pthread_mutex_lock(&m_mutexItems);
    if(m_queueItems.empty() && m_queuePrioItems.empty())
        dblAge = 0;
    else if (m_queueItems.empty()) // normal items queue is more important when checking queue age
        dblAge = dblNow - m_queuePrioItems.front()->dblReceiveStamp;
	else
        dblAge = dblNow - m_queueItems.front()->dblReceiveStamp;
        
    pthread_mutex_unlock(&m_mutexItems);
    
    *lpdblAge = dblAge;
    
    return erSuccess;
}

ECRESULT ECDispatcher::GetQueueLength(unsigned int *lpulLength)
{
    unsigned int ulLength = 0;
    pthread_mutex_lock(&m_mutexItems);
    ulLength = m_queueItems.size() + m_queuePrioItems.size();
    pthread_mutex_unlock(&m_mutexItems);
    
    *lpulLength = ulLength;
    return erSuccess;
}

ECRESULT ECDispatcher::AddListenSocket(struct soap *soap)
{
	soap->max_keep_alive = m_nMaxKeepAlive;
	soap->recv_timeout = m_nReadTimeout; // Use m_nReadTimeout, the value for timeouts during XML reads
	soap->send_timeout = m_nSendTimeout;

    m_setListenSockets.insert(std::make_pair(soap->socket, soap));
    
    return erSuccess;
}

ECRESULT ECDispatcher::QueueItem(struct soap *soap)
{
	WORKITEM *item = new WORKITEM;
	CONNECTION_TYPE ulType;

	item->soap = soap;
	item->dblReceiveStamp = GetTimeOfDay();
	zarafa_get_soap_connection_type(soap, &ulType);

	pthread_mutex_lock(&m_mutexItems);
	if (ulType == CONNECTION_TYPE_NAMED_PIPE_PRIORITY)
		m_queuePrioItems.push(item);
	else
		m_queueItems.push(item);
	pthread_cond_signal(&m_condItems);
	pthread_mutex_unlock(&m_mutexItems);

	return erSuccess;
}

// Called by worker threads to get an item to work on
ECRESULT ECDispatcher::GetNextWorkItem(WORKITEM **lppItem, bool bWait)
{
    WORKITEM *lpItem = NULL;
    ECRESULT er = erSuccess;
    
    pthread_mutex_lock(&m_mutexItems);
	// 1 out of 5 items is forced from normal to avoid starvation
	if (!m_queuePrioItems.empty() && (m_queueItems.empty() || (m_nPrioDone % 5 != 0))) {
		*lppItem = m_queuePrioItems.front();
		m_queuePrioItems.pop();
		m_nPrioDone++;
		goto exit;
	}

    // Check the queue
    if(!m_queueItems.empty()) {
        // Item is waiting, return that
		m_nPrioDone = 0;
        lpItem = m_queueItems.front();
        m_queueItems.pop();
    } else {
        // No item waiting
        if(bWait && !m_bExit) {
            pthread_mutex_lock(&m_mutexIdle); m_ulIdle++; pthread_mutex_unlock(&m_mutexIdle);
            
            // If requested, wait until item is available
            pthread_cond_wait(&m_condItems, &m_mutexItems);
            
            pthread_mutex_lock(&m_mutexIdle); m_ulIdle--; pthread_mutex_unlock(&m_mutexIdle);

			// 1 out of 5 items is forced from normal to avoid starvation
			if (!m_queuePrioItems.empty() && (m_queueItems.empty() || (m_nPrioDone % 5 != 0))) {
				*lppItem = m_queuePrioItems.front();
				m_queuePrioItems.pop();
				m_nPrioDone++;
				goto exit;
			}
			m_nPrioDone = 0;

            if(!m_queueItems.empty()) {
                lpItem = m_queueItems.front();
                m_queueItems.pop();
            } else {
                // Condition fired, but still nothing there. Probably exit requested
                er = ZARAFA_E_NOT_FOUND;
                goto exit;
            }
        } else {
            // No wait requested, return not found
            er = ZARAFA_E_NOT_FOUND;
            goto exit;
        }
    }
    
    *lppItem = lpItem;
exit:
    pthread_mutex_unlock(&m_mutexItems);
    
    return er;
}

// Called by a worker thread when it's done with an item
ECRESULT ECDispatcher::NotifyDone(struct soap *soap)
{
    // During exit, don't requeue active sockets, but close them
    if(m_bExit) {	
        soap_free(soap);
    } else {
		soap->max_keep_alive--;
		if (soap->max_keep_alive == 0)
			soap->keep_alive = 0;
        if(soap->socket != SOAP_INVALID_SOCKET) {
			SOAP_SOCKET	socket;
			socket = soap->socket;
            ACTIVESOCKET sActive;
            
            sActive.soap = soap;
            time(&sActive.ulLastActivity);
            
            pthread_mutex_lock(&m_mutexSockets);
            m_setSockets.insert(std::make_pair(soap->socket, sActive));
            pthread_mutex_unlock(&m_mutexSockets);
        
            // Notify select restart, send socket number which is done
			NotifyRestart(socket);
        } else {
            // SOAP has closed the socket, no need to requeue
            soap_free(soap);
        }
    }
    
    return erSuccess;
}

// Set the nominal thread count
ECRESULT ECDispatcher::SetThreadCount(unsigned int ulThreads)
{
    ECRESULT er = erSuccess;
    
    er = m_lpThreadManager->SetThreadCount(ulThreads);
    if(er != erSuccess)
        goto exit;
        
    // Since the threads may be blocking while waiting for the next queue item, broadcast
    // a wakeup for all threads so that they re-check their idle state (and exit if the thread count
    // is now lower)
    pthread_mutex_lock(&m_mutexItems);
    pthread_cond_broadcast(&m_condItems);
    pthread_mutex_unlock(&m_mutexItems);
        
exit:
    return er;
}

ECRESULT ECDispatcher::DoHUP()
{
	m_nMaxKeepAlive = atoi(m_lpConfig->GetSetting("server_max_keep_alive_requests"));
	m_nRecvTimeout = atoi(m_lpConfig->GetSetting("server_recv_timeout"));
	m_nReadTimeout = atoi(m_lpConfig->GetSetting("server_read_timeout"));
	m_nSendTimeout = atoi(m_lpConfig->GetSetting("server_send_timeout"));
	return SetThreadCount(atoi(m_lpConfig->GetSetting("threads")));
}

ECRESULT ECDispatcher::ShutDown()
{
    m_bExit = true;

    return erSuccess;
}

ECDispatcherSelect::ECDispatcherSelect(ECLogger *lpLogger, ECConfig *lpConfig, CREATEPIPESOCKETCALLBACK lpCallback, void *lpCallbackParam) : ECDispatcher(lpLogger, lpConfig, lpCallback, lpCallbackParam)
{
    int pipes[2];
    pipe(pipes);

	// Create a pipe that we can use to trigger select() to return
    m_fdRescanRead = pipes[0];
    m_fdRescanWrite = pipes[1];
}

ECDispatcherSelect::~ECDispatcherSelect()
{
}

ECRESULT ECDispatcherSelect::MainLoop()
{
    ECRESULT er = erSuccess;
    ECWatchDog *lpWatchDog = NULL;
    std::map<int, ACTIVESOCKET>::iterator iterSockets;
    std::map<int, ACTIVESOCKET>::iterator iterErase;
    std::map<int, struct soap *>::iterator iterListenSockets;
    int maxfds = 0;
    char s = 0;
    time_t now;
    struct timeval tv;
	CONNECTION_TYPE ulType;
	int n = 0;


    // This will start the threads
    m_lpThreadManager = new ECThreadManager(m_lpLogger, this, atoui(m_lpConfig->GetSetting("threads")));
    
    // Start the watchdog
    lpWatchDog = new ECWatchDog(m_lpConfig, m_lpLogger, this, m_lpThreadManager);

    // Main loop
    while(!m_bExit) {
        time(&now);
        
        fd_set readfds;
        
        FD_ZERO(&readfds);
        
        // Listen on rescan trigger
        FD_SET(m_fdRescanRead, &readfds);
        maxfds = m_fdRescanRead;
        pthread_mutex_lock(&m_mutexSockets);

        // Listen on active sockets
        iterSockets = m_setSockets.begin();
        while(iterSockets != m_setSockets.end()) {
            zarafa_get_soap_connection_type(iterSockets->second.soap, &ulType);
            if(ulType != CONNECTION_TYPE_NAMED_PIPE && ulType != CONNECTION_TYPE_NAMED_PIPE_PRIORITY && (now - (time_t)iterSockets->second.ulLastActivity > m_nRecvTimeout)) {
                // Socket has been inactive for more than server_recv_timeout seconds, close the socket
				shutdown(iterSockets->second.soap->socket, SHUT_RDWR);
            }
            
            FD_SET(iterSockets->second.soap->socket, &readfds);
            maxfds = max(maxfds, iterSockets->second.soap->socket);
            iterSockets++;
        }
        // Listen on listener sockets
        for(iterListenSockets = m_setListenSockets.begin(); iterListenSockets != m_setListenSockets.end(); iterListenSockets++) {
			FD_SET(iterListenSockets->second->socket, &readfds);
            maxfds = max(maxfds, iterListenSockets->second->socket);
        }
        pthread_mutex_unlock(&m_mutexSockets);
        		
        // Wait for at most 1 second, so that we can close inactive sockets
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        
        // Wait for activity
        if(( n = select(maxfds+1, &readfds, NULL, NULL, &tv)) < 0)
            continue; // signal caught, restart

        if(FD_ISSET(m_fdRescanRead, &readfds)) {
            char s[128];
            // A socket rescan has been triggered, we don't need to do anything, just read the data, discard it
            // and restart the select call
            read(m_fdRescanRead, s, sizeof(s));
        } 

        pthread_mutex_lock(&m_mutexSockets);
        
        // Search for activity on active sockets
        iterSockets = m_setSockets.begin();
        while(n && iterSockets != m_setSockets.end()) {
            if(FD_ISSET(iterSockets->second.soap->socket, &readfds)) {
                // Activity on a TCP/pipe socket
                
                // First, check for EOF
                if(recv(iterSockets->second.soap->socket, &s, 1, MSG_PEEK) == 0) {
                    // EOF occurred, just close the socket and remove it from the socket list
                    soap_free(iterSockets->second.soap);
                    m_setSockets.erase(iterSockets++);
                } else {
                    // Actual data waiting, push it on the processing queue
					QueueItem(iterSockets->second.soap);
                    
                    // Remove socket from listen list for now, since we're already handling data there and don't
                    // want to interfere with the thread that is now handling that socket. It will be passed back
                    // to us when the request is done.
                    m_setSockets.erase(iterSockets++);
                }
                
                // N holds the number of descriptors set in readfds, so decrease by one since we handled that one.
                n--;
            } else {
                iterSockets++;
            }
        }

        pthread_mutex_unlock(&m_mutexSockets);

        // Search for activity on listen sockets
        for(iterListenSockets = m_setListenSockets.begin(); iterListenSockets != m_setListenSockets.end(); iterListenSockets++) {
            if(FD_ISSET(iterListenSockets->second->socket, &readfds)) {
                struct soap *newsoap;
                ACTIVESOCKET sActive;
                
                newsoap = soap_copy(iterListenSockets->second);
                
                if(newsoap == NULL) {
                    m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to accept new connection: out of memory");
                    continue;
                }
                
                // Record last activity (now)
                time(&sActive.ulLastActivity);

        		zarafa_get_soap_connection_type(iterListenSockets->second, &ulType);
                if(ulType == CONNECTION_TYPE_NAMED_PIPE || ulType == CONNECTION_TYPE_NAMED_PIPE_PRIORITY) {
                    int socket = accept(newsoap->master, NULL, 0);
                    newsoap->socket = socket;
                } else {
                    soap_accept(newsoap);
                }
                    
                if(newsoap->socket == SOAP_INVALID_SOCKET) {
					if (m_lpLogger->Log(EC_LOGLEVEL_DEBUG)) {
						if (ulType == CONNECTION_TYPE_NAMED_PIPE)
							m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Error accepting incoming connection from file://%s", m_lpConfig->GetSetting("server_pipe_name"));
						else if (ulType == CONNECTION_TYPE_NAMED_PIPE_PRIORITY)
							m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Error accepting incoming connection from file://%s", m_lpConfig->GetSetting("server_pipe_priority"));
						else
							m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Error accepting incoming connection from network.");
					}
                    soap_free(newsoap);
                } else {
					if (m_lpLogger->Log(EC_LOGLEVEL_DEBUG)) {
						if (ulType == CONNECTION_TYPE_NAMED_PIPE)
							m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Accepted incoming connection from file://%s", m_lpConfig->GetSetting("server_pipe_name"));
						else if (ulType == CONNECTION_TYPE_NAMED_PIPE_PRIORITY)
							m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Accepted incoming connection from file://%s", m_lpConfig->GetSetting("server_pipe_priority"));
						else
							m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Accepted incoming%sconnection from %d.%d.%d.%d",
											ulType == CONNECTION_TYPE_SSL ? " SSL ":" ",
											(int)((newsoap->ip >> 24)&0xFF), (int)((newsoap->ip >> 16)&0xFF), (int)((newsoap->ip >> 8)&0xFF), (int)(newsoap->ip&0xFF));
					}
                    newsoap->socket = relocate_fd(newsoap->socket, m_lpLogger);
					g_lpStatsCollector->Max(SCN_MAX_SOCKET_NUMBER, (LONGLONG)newsoap->socket);

					g_lpStatsCollector->Increment(SCN_SERVER_CONNECTIONS);

                    sActive.soap = newsoap;
                    pthread_mutex_lock(&m_mutexSockets);
                    m_setSockets.insert(std::make_pair(sActive.soap->socket, sActive));
                    pthread_mutex_unlock(&m_mutexSockets);
                }
            }
        }
    }

    // Delete the watchdog. This makes sure no new threads will be started.
    delete lpWatchDog;
    
    // Set the thread count to zero so that threads will exit
    m_lpThreadManager->SetThreadCount(0);

    // Notify threads that they should re-query their idle state (and exit)
    pthread_mutex_lock(&m_mutexItems);
    pthread_cond_broadcast(&m_condItems);
    pthread_mutex_unlock(&m_mutexItems);
    
    // Delete thread manager (waits for threads to become idle). During this time
    // the threads may report back a workitem as being done. If this is the case, we directly close that socket too.
    delete m_lpThreadManager;
    
    // Empty the queue
    pthread_mutex_lock(&m_mutexItems);
    while(!m_queueItems.empty()) {soap_free(m_queueItems.front()->soap); m_queueItems.pop(); }
    while(!m_queuePrioItems.empty()) {soap_free(m_queuePrioItems.front()->soap); m_queuePrioItems.pop(); }
    pthread_mutex_unlock(&m_mutexItems);

    // Close all listener sockets. 
    for(iterListenSockets = m_setListenSockets.begin(); iterListenSockets != m_setListenSockets.end(); iterListenSockets++) {
		zarafa_end_soap_connection(iterListenSockets->second);
        soap_free(iterListenSockets->second);
    }
    // Close all sockets. This will cause all that we were listening on clients to get an EOF
	pthread_mutex_lock(&m_mutexSockets);
    for(iterSockets = m_setSockets.begin(); iterSockets != m_setSockets.end(); iterSockets++) {
        soap_free(iterSockets->second.soap);
    }
	pthread_mutex_unlock(&m_mutexSockets);
    
    return er;
}

ECRESULT ECDispatcherSelect::ShutDown()
{
	ECDispatcher::ShutDown();

    char s = 0;
    // Notify select wakeup
    write(m_fdRescanWrite, &s, 1);

    return erSuccess;
}

ECRESULT ECDispatcherSelect::NotifyRestart(SOAP_SOCKET s)
{
	write(m_fdRescanWrite, &s, sizeof(SOAP_SOCKET));
	return erSuccess;
}

#ifdef HAVE_EPOLL_CREATE
ECDispatcherEPoll::ECDispatcherEPoll(ECLogger *lpLogger, ECConfig *lpConfig, CREATEPIPESOCKETCALLBACK lpCallback, void *lpCallbackParam) : ECDispatcher(lpLogger, lpConfig, lpCallback, lpCallbackParam)
{
	m_fdMax = getdtablesize();
	m_epFD = epoll_create(m_fdMax);
}

ECDispatcherEPoll::~ECDispatcherEPoll()
{
	close(m_epFD);
}

ECRESULT ECDispatcherEPoll::MainLoop()
{
	ECRESULT er = erSuccess;
	ECWatchDog *lpWatchDog = NULL;
	time_t now = 0;
	time_t last = 0;
	std::map<int, ACTIVESOCKET>::iterator iterSockets;
	std::map<int, struct soap *>::iterator iterListenSockets;
	CONNECTION_TYPE ulType;

	epoll_event epevent;
	epoll_event *epevents;
	int n;

	epevents = new epoll_event[m_fdMax];

	// setup epoll for listen sockets
	memset(&epevent, 0, sizeof(epoll_event));

	epevent.events = EPOLLIN | EPOLLPRI; // wait for input and priority (?) events

	for (iterListenSockets = m_setListenSockets.begin(); iterListenSockets != m_setListenSockets.end(); iterListenSockets++) {
		epevent.data.fd = iterListenSockets->second->socket; 
		epoll_ctl(m_epFD, EPOLL_CTL_ADD, iterListenSockets->second->socket, &epevent);
	}

	// This will start the threads
	m_lpThreadManager = new ECThreadManager(m_lpLogger, this, atoui(m_lpConfig->GetSetting("threads")));

	// Start the watchdog
	lpWatchDog = new ECWatchDog(m_lpConfig, m_lpLogger, this, m_lpThreadManager);

	while (!m_bExit) {
		time(&now);

		// find timedout sockets once per second
		pthread_mutex_lock(&m_mutexSockets);
		if(now > last) {
            iterSockets = m_setSockets.begin();
            while (iterSockets != m_setSockets.end()) {
                zarafa_get_soap_connection_type(iterSockets->second.soap, &ulType);
                if(ulType != CONNECTION_TYPE_NAMED_PIPE && ulType != CONNECTION_TYPE_NAMED_PIPE_PRIORITY && (now - (time_t)iterSockets->second.ulLastActivity > m_nRecvTimeout)) {
                    // Socket has been inactive for more than server_recv_timeout seconds, close the socket
                    shutdown(iterSockets->second.soap->socket, SHUT_RDWR);
                }
                iterSockets++;
            }
            last = now;
        }
		pthread_mutex_unlock(&m_mutexSockets);

		n = epoll_wait(m_epFD, epevents, m_fdMax, 1000); // timeout -1 is wait indefinitely
		pthread_mutex_lock(&m_mutexSockets);
		for (int i = 0; i < n; i++) {
			iterListenSockets = m_setListenSockets.find(epevents[i].data.fd);

			if (iterListenSockets != m_setListenSockets.end()) {
				// this was a listen socket .. accept and continue
				struct soap *newsoap;
				ACTIVESOCKET sActive;

				newsoap = soap_copy(iterListenSockets->second);

				// Record last activity (now)
				time(&sActive.ulLastActivity);

				zarafa_get_soap_connection_type(iterListenSockets->second, &ulType);
				if(ulType == CONNECTION_TYPE_NAMED_PIPE || ulType == CONNECTION_TYPE_NAMED_PIPE_PRIORITY) {
					newsoap->socket = accept(newsoap->master, NULL, 0);
				} else {
					soap_accept(newsoap);
				}

				if(newsoap->socket == SOAP_INVALID_SOCKET) {
					if (m_lpLogger->Log(EC_LOGLEVEL_DEBUG)) {
						if (ulType == CONNECTION_TYPE_NAMED_PIPE)
							m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Error accepting incoming connection from file://%s", m_lpConfig->GetSetting("server_pipe_name"));
						else if (ulType == CONNECTION_TYPE_NAMED_PIPE_PRIORITY)
							m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Error accepting incoming connection from file://%s", m_lpConfig->GetSetting("server_pipe_priority"));
						else
							m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Error accepting incoming connection from network.");
					}
					soap_free(newsoap);
				} else {
					if (m_lpLogger->Log(EC_LOGLEVEL_DEBUG)) {
						if (ulType == CONNECTION_TYPE_NAMED_PIPE)
							m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Accepted incoming connection from file://%s", m_lpConfig->GetSetting("server_pipe_name"));
						else if (ulType == CONNECTION_TYPE_NAMED_PIPE_PRIORITY)
							m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Accepted incoming connection from file://%s", m_lpConfig->GetSetting("server_pipe_priority"));
						else
							m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Accepted incoming%sconnection from %d.%d.%d.%d",
											ulType == CONNECTION_TYPE_SSL ? " SSL ":" ",
											(int)((newsoap->ip >> 24)&0xFF), (int)((newsoap->ip >> 16)&0xFF), (int)((newsoap->ip >> 8)&0xFF), (int)(newsoap->ip&0xFF));
					}
					newsoap->socket = relocate_fd(newsoap->socket, m_lpLogger);
					g_lpStatsCollector->Max(SCN_MAX_SOCKET_NUMBER, (LONGLONG)newsoap->socket);

					g_lpStatsCollector->Increment(SCN_SERVER_CONNECTIONS);

					// directly make worker thread active
                    sActive.soap = newsoap;
                    m_setSockets.insert(std::make_pair(sActive.soap->socket, sActive));

					NotifyRestart(newsoap->socket);
				}

			} else {
				// this is a new request from an existing client
				iterSockets = m_setSockets.find(epevents[i].data.fd);

				// remove from epfd, either close socket, or it will be reactivated later in the epfd
				epevent.data.fd = iterSockets->second.soap->socket; 
				epoll_ctl(m_epFD, EPOLL_CTL_DEL, iterSockets->second.soap->socket, &epevent);

				if ((epevents[i].events & EPOLLHUP) == EPOLLHUP) {
					soap_free(iterSockets->second.soap);
					m_setSockets.erase(iterSockets);
				} else {
					QueueItem(iterSockets->second.soap);

					// Remove socket from listen list for now, since we're already handling data there and don't
					// want to interfere with the thread that is now handling that socket. It will be passed back
					// to us when the request is done.
					m_setSockets.erase(iterSockets);
				}
			}
		}
		pthread_mutex_unlock(&m_mutexSockets);
	}

	// Delete the watchdog. This makes sure no new threads will be started.
	delete lpWatchDog;

    // Set the thread count to zero so that threads will exit
    m_lpThreadManager->SetThreadCount(0);

    // Notify threads that they should re-query their idle state (and exit)
    pthread_mutex_lock(&m_mutexItems);
    pthread_cond_broadcast(&m_condItems);
    pthread_mutex_unlock(&m_mutexItems);

	delete m_lpThreadManager;

    // Empty the queue
    pthread_mutex_lock(&m_mutexItems);
    while(!m_queueItems.empty()) {soap_free(m_queueItems.front()->soap); m_queueItems.pop(); }
    while(!m_queuePrioItems.empty()) {soap_free(m_queuePrioItems.front()->soap); m_queuePrioItems.pop(); }
    pthread_mutex_unlock(&m_mutexItems);

    // Close all listener sockets. 
    for(iterListenSockets = m_setListenSockets.begin(); iterListenSockets != m_setListenSockets.end(); iterListenSockets++) {
		zarafa_end_soap_connection(iterListenSockets->second);
        soap_free(iterListenSockets->second);
    }
    // Close all sockets. This will cause all that we were listening on clients to get an EOF
	pthread_mutex_lock(&m_mutexSockets);
    for(iterSockets = m_setSockets.begin(); iterSockets != m_setSockets.end(); iterSockets++) {
        soap_free(iterSockets->second.soap);
    }
	pthread_mutex_unlock(&m_mutexSockets);
    

	delete [] epevents;

	return er;
}

ECRESULT ECDispatcherEPoll::NotifyRestart(SOAP_SOCKET s)
{
	// add soap socket in epoll fd
	epoll_event epevent;
	epevent.events = EPOLLIN | EPOLLPRI; // wait for input and priority (?) events
	epevent.data.fd = s;
	epoll_ctl(m_epFD, EPOLL_CTL_ADD, epevent.data.fd, &epevent);
	return erSuccess;
}
#endif

