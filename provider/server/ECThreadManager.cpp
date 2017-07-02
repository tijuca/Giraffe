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
#include <mutex>
#include "ECThreadManager.h"

#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <kopano/lockhelper.hpp>
#include <kopano/stringutil.h>

#ifdef HAVE_EPOLL_CREATE
#include <sys/epoll.h>
#endif

#include <kopano/CommonUtil.h>
#include "ECSessionManager.h"
#include "ECStatsCollector.h"
#include "ECServerEntrypoint.h"
#include "ECSoapServerConnection.h"

// errors from stdsoap2.h, differs per gSOAP release
#define RETURN_CASE(x) \
	case x: \
		return #x;

static string GetSoapError(int err)
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

static void kcsrv_blocksigs(void)
{
	sigset_t m;
	sigemptyset(&m);
	sigaddset(&m, SIGINT);
	sigaddset(&m, SIGHUP);
	sigaddset(&m, SIGTERM);
}

ECWorkerThread::ECWorkerThread(ECThreadManager *lpManager,
    ECDispatcher *lpDispatcher, bool bDoNotStart)
{
	m_lpManager = lpManager;
	m_lpDispatcher = lpDispatcher;

	if (bDoNotStart) {
		memset(&m_thread, 0, sizeof(m_thread));
		return;
	}
	if (pthread_create(&m_thread, NULL, ECWorkerThread::Work, this) != 0) {
		ec_log_crit("Unable to start thread: %s", strerror(errno));
		return;
	}
	set_thread_name(m_thread, "ECWorkerThread");
	pthread_detach(m_thread);
}

ECPriorityWorkerThread::ECPriorityWorkerThread(ECThreadManager *lpManager,
    ECDispatcher *lpDispatcher) :
	ECWorkerThread(lpManager, lpDispatcher, true)
{
    if (pthread_create(&m_thread, NULL, ECWorkerThread::Work, this) != 0)
		ec_log_crit("Unable to start thread: %s", strerror(errno));
    else
	set_thread_name(m_thread, "ECPriorityWorkerThread");
	// do not detach
}

ECPriorityWorkerThread::~ECPriorityWorkerThread()
{
	pthread_join(m_thread, NULL);
}

void *ECWorkerThread::Work(void *lpParam)
{
	kcsrv_blocksigs();
    ECWorkerThread *lpThis = (ECWorkerThread *)lpParam;
	ECPriorityWorkerThread *lpPrio = dynamic_cast<ECPriorityWorkerThread*>(lpThis);
    WORKITEM *lpWorkItem = NULL;
    ECRESULT er = erSuccess;
    bool fStop = false;
	int err = 0;

	ec_log_debug("Started%sthread %08x", lpPrio ? " priority " : " ", (ULONG)pthread_self());
    
    while(1) {
		set_thread_name(pthread_self(), "z-s: idle thread");

        // Get the next work item, don't wait for new items
        if(lpThis->m_lpDispatcher->GetNextWorkItem(&lpWorkItem, false, lpPrio != NULL) != erSuccess) {
            // Nothing in the queue, notify that we're idle now
            lpThis->m_lpManager->NotifyIdle(lpThis, &fStop);
            
            // We were requested to exit due to idle state
            if(fStop) {
				ec_log_debug("Thread %08x idle and requested to exit", (ULONG)pthread_self());
                break;
            }
                
            // Wait for next work item in the queue
            er = lpThis->m_lpDispatcher->GetNextWorkItem(&lpWorkItem, true, lpPrio != NULL);
            if (er != erSuccess)
                // This could happen because we were waken up because we are exiting
                continue;
        }

		set_thread_name(pthread_self(), format("z-s: %s", lpWorkItem->soap->host).c_str());

		// For SSL connections, we first must do the handshake and pass it back to the queue
		if (lpWorkItem->soap->ctx && !lpWorkItem->soap->ssl) {
			err = soap_ssl_accept(lpWorkItem->soap);
			if (err) {
				ec_log_warn("%s", soap_faultdetail(lpWorkItem->soap)[0]);
				ec_log_debug("%s: %s", GetSoapError(err).c_str(), soap_faultstring(lpWorkItem->soap)[0]);
			}
        } else {
			err = 0;

			// Record start of handling of this request
			double dblStart = GetTimeOfDay(), dblEnd = 0;
			
			// Reset last session ID so we can use it reliably after the call is done
            ((SOAPINFO *)lpWorkItem->soap->user)->ulLastSessionId = 0;
            // Pass information on start time of the request into soap->user, so that it can be applied to the correct
            // session after XML parsing
            clock_gettime(CLOCK_THREAD_CPUTIME_ID, &((SOAPINFO *)lpWorkItem->soap->user)->threadstart);
            ((SOAPINFO *)lpWorkItem->soap->user)->start = GetTimeOfDay();
            ((SOAPINFO *)lpWorkItem->soap->user)->szFname = NULL;

			((SOAPINFO *)lpWorkItem->soap->user)->fdone = NULL;

            // Do processing of work item
            soap_begin(lpWorkItem->soap);
            if(soap_begin_recv(lpWorkItem->soap)) {
                if(lpWorkItem->soap->error < SOAP_STOP) {
					// Client Updater returns 404 to the client to say it doesn't need to update, so skip this HTTP error
					if (lpWorkItem->soap->error != SOAP_EOF && lpWorkItem->soap->error != 404)
						ec_log_debug("gSOAP error on receiving request: %s", GetSoapError(lpWorkItem->soap->error).c_str());
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
			ec_log_debug("gSOAP error on processing request: %s", GetSoapError(err).c_str());
                soap_send_fault(lpWorkItem->soap);
                goto done;
            }

done:	
			if(((SOAPINFO *)lpWorkItem->soap->user)->fdone)
				((SOAPINFO *)lpWorkItem->soap->user)->fdone(lpWorkItem->soap, ((SOAPINFO *)lpWorkItem->soap->user)->fdoneparam);

            dblEnd = GetTimeOfDay();

            // Tell the session we're done processing the request for this session. This will also tell the session that this
            // thread is done processing the item, so any time spent in this thread until now can be accounted in that session.
            g_lpSessionManager->RemoveBusyState(((SOAPINFO *)lpWorkItem->soap->user)->ulLastSessionId, pthread_self());
            
		// Track cpu usage server-wide
		g_lpStatsCollector->Increment(SCN_SOAP_REQUESTS);
		g_lpStatsCollector->Increment(SCN_PROCESSING_TIME, int64_t((dblEnd - dblStart) * 1000));
		g_lpStatsCollector->Increment(SCN_RESPONSE_TIME, int64_t((dblEnd - lpWorkItem->dblReceiveStamp) * 1000));

        }

	// Clear memory used by soap calls. Note that this does not actually
	// undo our soap_new2() call so the soap object is still valid after these calls
	soap_destroy(lpWorkItem->soap);
	soap_end(lpWorkItem->soap);

        // We're done processing the item, the workitem's socket is returned to the queue
        lpThis->m_lpDispatcher->NotifyDone(lpWorkItem->soap);
        delete lpWorkItem;
    }

	/** free ssl error data **/
	ERR_remove_state(0);

    // We're detached, so we should clean up ourselves
	if (lpPrio == NULL)
		delete lpThis;
    
    return NULL;
}

ECThreadManager::ECThreadManager(ECDispatcher *lpDispatcher,
    unsigned int ulThreads) :
	m_lpDispatcher(lpDispatcher), m_ulThreads(ulThreads)
{
	scoped_lock l_thr(m_mutexThreads);
    // Start our worker threads
	m_lpPrioWorker = new ECPriorityWorkerThread(this, lpDispatcher);
	for (unsigned int i = 0; i < ulThreads; ++i)
		m_lstThreads.push_back(new ECWorkerThread(this, lpDispatcher));
}

ECThreadManager::~ECThreadManager()
{
	unsigned int ulThreads;

    // Wait for the threads to exit
    while(1) {
		ulock_normal l_thr(m_mutexThreads);
		ulThreads = m_lstThreads.size();
		l_thr.unlock();
        if(ulThreads > 0) {
			ec_log_notice("Still waiting for %d worker threads to exit", ulThreads);
            Sleep(1000);
        }
        else
            break;
    }    
	delete m_lpPrioWorker;
}
    
ECRESULT ECThreadManager::ForceAddThread(int nThreads)
{
	scoped_lock l_thr(m_mutexThreads);
	for (int i = 0; i < nThreads; ++i)
		m_lstThreads.push_back(new ECWorkerThread(this, m_lpDispatcher));
	return erSuccess;
}

ECRESULT ECThreadManager::GetThreadCount(unsigned int *lpulThreads)
{
	unsigned int ulThreads;
	scoped_lock l_thr(m_mutexThreads);
	ulThreads = m_lstThreads.size();
	*lpulThreads = ulThreads;
	return erSuccess;
}

ECRESULT ECThreadManager::SetThreadCount(unsigned int ulThreads)
{
    // If we're under the number of threads at the moment, start new ones
	scoped_lock l_thr(m_mutexThreads);

    // Set the default thread count
    m_ulThreads = ulThreads;

    while(ulThreads > m_lstThreads.size())
		m_lstThreads.push_back(new ECWorkerThread(this, m_lpDispatcher));
    // If we are OVER the number of threads, then the code in NotifyIdle() will bring this down
    return erSuccess;
}

// Called by worker threads only when it is idle. This is the only place where the worker thread can be
// deleted.
ECRESULT ECThreadManager::NotifyIdle(ECWorkerThread *lpThread, bool *lpfStop)
{
	std::list<ECWorkerThread *>::iterator iterThreads;
    *lpfStop = false;
        
	scoped_lock l_thr(m_mutexThreads);
	// special case for priority worker
	if (lpThread == m_lpPrioWorker) {
		// exit requested?
		*lpfStop = (m_ulThreads == 0);
		return erSuccess;
	}
	if (m_ulThreads >= m_lstThreads.size())
		return erSuccess;
        // We are currently running more threads than we want, so tell the thread to stop
        iterThreads = std::find(m_lstThreads.begin(), m_lstThreads.end(), lpThread);
	if (iterThreads == m_lstThreads.end()) {
		ec_log_crit("A thread that we don't know is idle ...");
		return KCERR_NOT_FOUND;
	}
        // Remove the thread from our running thread list
        m_lstThreads.erase(iterThreads);
        
        // Tell the thread to exit. The thread will self-cleanup; we therefore needn't delete the object nor join with the running thread
        *lpfStop = true;
	return erSuccess;
}

ECWatchDog::ECWatchDog(ECConfig *lpConfig, ECDispatcher *lpDispatcher,
    ECThreadManager *lpThreadManager) :
	m_lpConfig(lpConfig), m_lpDispatcher(lpDispatcher),
	m_lpThreadManager(lpThreadManager)
{
    if (pthread_create(&m_thread, NULL, ECWatchDog::Watch, this) != 0)
		ec_log_crit("Unable to start watchdog thread: %s", strerror(errno));
    else
	set_thread_name(m_thread, "ECWatchDog");
}

ECWatchDog::~ECWatchDog()
{
    void *ret;
    
	ulock_normal l_exit(m_mutexExit);
	m_bExit = true;
	m_condExit.notify_one();
	l_exit.unlock();
    
    pthread_join(m_thread, &ret);
}

void *ECWatchDog::Watch(void *lpParam)
{
    ECWatchDog *lpThis = (ECWatchDog *)lpParam;
    double dblAge;
	kcsrv_blocksigs();
    
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
		ulock_normal l_exit(lpThis->m_mutexExit);
        if(lpThis->m_bExit == false) {
			lpThis->m_condExit.wait_for(l_exit, std::chrono::duration<double>(1 / dblMaxFreq));
			if (lpThis->m_bExit == true)
				break;
        }
    }
    
    return NULL;
}

ECDispatcher::ECDispatcher(ECConfig *lpConfig,
    CREATEPIPESOCKETCALLBACK lpCallback, void *lpParam)
{
	m_lpConfig = lpConfig;

	// Default socket settings
	m_nMaxKeepAlive = atoi(m_lpConfig->GetSetting("server_max_keep_alive_requests"));
	m_nRecvTimeout = atoi(m_lpConfig->GetSetting("server_recv_timeout"));
	m_nReadTimeout = atoi(m_lpConfig->GetSetting("server_read_timeout"));
	m_nSendTimeout = atoi(m_lpConfig->GetSetting("server_send_timeout"));
	m_lpCreatePipeSocketCallback = lpCallback;
	m_lpCreatePipeSocketParam = lpParam;
}

ECRESULT ECDispatcher::GetThreadCount(unsigned int *lpulThreads, unsigned int *lpulIdleThreads)
{
	ECRESULT er = m_lpThreadManager->GetThreadCount(lpulThreads);
	if (er != erSuccess)
		return er;
	*lpulIdleThreads = m_ulIdle;
	return erSuccess;
}

// Get the age (in seconds) of the next-in-line item in the queue, or 0 if the queue is empty
ECRESULT ECDispatcher::GetFrontItemAge(double *lpdblAge)
{
    double dblNow = GetTimeOfDay();
    double dblAge = 0;
    
	scoped_lock lock(m_mutexItems);
    if(m_queueItems.empty() && m_queuePrioItems.empty())
        dblAge = 0;
    else if (m_queueItems.empty()) // normal items queue is more important when checking queue age
        dblAge = dblNow - m_queuePrioItems.front()->dblReceiveStamp;
	else
        dblAge = dblNow - m_queueItems.front()->dblReceiveStamp;
    *lpdblAge = dblAge;
    return erSuccess;
}

ECRESULT ECDispatcher::GetQueueLength(unsigned int *lpulLength)
{
    unsigned int ulLength = 0;
	scoped_lock lock(m_mutexItems);
    ulLength = m_queueItems.size() + m_queuePrioItems.size();
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
	ulType = SOAP_CONNECTION_TYPE(soap);

	scoped_lock lock(m_mutexItems);
	if (ulType == CONNECTION_TYPE_NAMED_PIPE_PRIORITY) {
		m_queuePrioItems.push(item);
		m_condPrioItems.notify_one();
	} else {
		m_queueItems.push(item);
		m_condItems.notify_one();
	}
	return erSuccess;
}

/** 
 * Called by worker threads to get an item to work on
 * 
 * @param[out] lppItem soap call to process
 * @param[in] bWait wait for an item until present or return immediately
 * @param[in] bPrio handle priority or normal queue
 * 
 * @return error code
 * @retval KCERR_NOT_FOUND no soap call in the queue present
 */
ECRESULT ECDispatcher::GetNextWorkItem(WORKITEM **lppItem, bool bWait, bool bPrio)
{
    WORKITEM *lpItem = NULL;
    ECRESULT er = erSuccess;
	std::queue<WORKITEM *>* queue = bPrio ? &m_queuePrioItems : &m_queueItems;
	auto &condItems = bPrio ? m_condPrioItems : m_condItems;
	ulock_normal l_item(m_mutexItems);

    // Check the queue
    if(!queue->empty()) {
        // Item is waiting, return that
        lpItem = queue->front();
        queue->pop();
    } else if (!bWait || m_bExit) {
        // No wait requested, return not found
		return KCERR_NOT_FOUND;
    } else {
        // No item waiting
		ulock_normal l_idle(m_mutexIdle);
		++m_ulIdle;
		l_idle.unlock();

		/* If requested, wait until item is available */
		condItems.wait(l_item);
		l_idle.lock();
		--m_ulIdle;
		l_idle.unlock();

        if (queue->empty() || m_bExit)
            // Condition fired, but still nothing there. Probably exit requested or wrong queue signal
			return KCERR_NOT_FOUND;

        lpItem = queue->front();
        queue->pop();
    }
    
    *lppItem = lpItem;
    return er;
}

// Called by a worker thread when it's done with an item
ECRESULT ECDispatcher::NotifyDone(struct soap *soap)
{
    // During exit, don't requeue active sockets, but close them
    if(m_bExit) {	
		kopano_end_soap_connection(soap);
        soap_free(soap);
    } else {
		--soap->max_keep_alive;
		if (soap->max_keep_alive == 0)
			soap->keep_alive = 0;
        if(soap->socket != SOAP_INVALID_SOCKET) {
			SOAP_SOCKET	socket;
			socket = soap->socket;
            ACTIVESOCKET sActive;
            
            sActive.soap = soap;
            time(&sActive.ulLastActivity);
            
			ulock_normal l_sock(m_mutexSockets);
			m_setSockets.insert(std::make_pair(soap->socket, sActive));
			l_sock.unlock();
            // Notify select restart, send socket number which is done
			NotifyRestart(socket);
        } else {
            // SOAP has closed the socket, no need to requeue
			kopano_end_soap_connection(soap);
            soap_free(soap);
        }
    }
    
    return erSuccess;
}

// Set the nominal thread count
ECRESULT ECDispatcher::SetThreadCount(unsigned int ulThreads)
{
	// if we receive a signal before the MainLoop() has started, we don't have thread manager yet
	if (m_lpThreadManager == NULL)
		return erSuccess;
	ECRESULT er = m_lpThreadManager->SetThreadCount(ulThreads);
	if (er != erSuccess)
		return er;
        
    // Since the threads may be blocking while waiting for the next queue item, broadcast
    // a wakeup for all threads so that they re-check their idle state (and exit if the thread count
    // is now lower)
	scoped_lock l_item(m_mutexItems);
	m_condItems.notify_all();
	return erSuccess;
}

ECRESULT ECDispatcher::DoHUP()
{
	m_nMaxKeepAlive = atoi(m_lpConfig->GetSetting("server_max_keep_alive_requests"));
	m_nRecvTimeout = atoi(m_lpConfig->GetSetting("server_recv_timeout"));
	m_nReadTimeout = atoi(m_lpConfig->GetSetting("server_read_timeout"));
	m_nSendTimeout = atoi(m_lpConfig->GetSetting("server_send_timeout"));

	ECRESULT er = SetThreadCount(atoi(m_lpConfig->GetSetting("threads")));
	if (er != erSuccess)
		return er;

	for (auto const &p : m_setListenSockets) {
		auto ulType = SOAP_CONNECTION_TYPE(p.second);

		if (ulType == CONNECTION_TYPE_SSL) {
			if (soap_ssl_server_context(p.second, SOAP_SSL_DEFAULT,
						   m_lpConfig->GetSetting("server_ssl_key_file"),
						   m_lpConfig->GetSetting("server_ssl_key_pass","",NULL),
						   m_lpConfig->GetSetting("server_ssl_ca_file","",NULL),
						   m_lpConfig->GetSetting("server_ssl_ca_path","",NULL),
						   NULL, NULL, "EC")) {
				ec_log_crit("K-3904: Unable to setup ssl context: %s", *soap_faultdetail(p.second));
				return KCERR_CALL_FAILED;
			}

			char *server_ssl_protocols = strdup(m_lpConfig->GetSetting("server_ssl_protocols"));
			er = kc_ssl_options(p.second, server_ssl_protocols,
				m_lpConfig->GetSetting("server_ssl_ciphers"),
				m_lpConfig->GetSetting("server_ssl_prefer_server_ciphers"));
			free(server_ssl_protocols);
		}
	}
	return erSuccess;
}

ECRESULT ECDispatcher::ShutDown()
{
    m_bExit = true;

    return erSuccess;
}

ECDispatcherSelect::ECDispatcherSelect(ECConfig *lpConfig,
    CREATEPIPESOCKETCALLBACK lpCallback, void *lpCallbackParam) :
	ECDispatcher(lpConfig, lpCallback, lpCallbackParam)
{
    int pipes[2];
    pipe(pipes);

	// Create a pipe that we can use to trigger select() to return
    m_fdRescanRead = pipes[0];
    m_fdRescanWrite = pipes[1];
}

ECRESULT ECDispatcherSelect::MainLoop()
{
	ECRESULT er = erSuccess;
	ECWatchDog *lpWatchDog = NULL;
    int maxfds = 0;
    char s = 0;
    time_t now;
    struct timeval tv;
	CONNECTION_TYPE ulType;
	int n = 0;

    // This will start the threads
	m_lpThreadManager = new ECThreadManager(this, atoui(m_lpConfig->GetSetting("threads")));
    
    // Start the watchdog
	lpWatchDog = new ECWatchDog(m_lpConfig, this, m_lpThreadManager);

    // Main loop
    while(!m_bExit) {
        time(&now);
        
        fd_set readfds;
        
        FD_ZERO(&readfds);
        
        // Listen on rescan trigger
        FD_SET(m_fdRescanRead, &readfds);
        maxfds = m_fdRescanRead;

        // Listen on active sockets
		ulock_normal l_sock(m_mutexSockets);
		for (const auto &p : m_setSockets) {
			ulType = SOAP_CONNECTION_TYPE(p.second.soap);
			if (ulType != CONNECTION_TYPE_NAMED_PIPE &&
			    ulType != CONNECTION_TYPE_NAMED_PIPE_PRIORITY &&
				now - static_cast<time_t>(p.second.ulLastActivity) > m_nRecvTimeout)
				// Socket has been inactive for more than server_recv_timeout seconds, close the socket
				shutdown(p.second.soap->socket, SHUT_RDWR);
            
			FD_SET(p.second.soap->socket, &readfds);
			maxfds = max(maxfds, p.second.soap->socket);
        }
        // Listen on listener sockets
		for (const auto &p : m_setListenSockets) {
			FD_SET(p.second->socket, &readfds);
			maxfds = max(maxfds, p.second->socket);
        }
		l_sock.unlock();
        		
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

        // Search for activity on active sockets
		l_sock.lock();
	auto iterSockets = m_setSockets.cbegin();
	while (n != 0 && iterSockets != m_setSockets.cend()) {
		if (!FD_ISSET(iterSockets->second.soap->socket, &readfds)) {
			++iterSockets;
			continue;
		}
		// Activity on a TCP/pipe socket
		// First, check for EOF
		if (recv(iterSockets->second.soap->socket, &s, 1, MSG_PEEK) == 0) {
			// EOF occurred, just close the socket and remove it from the socket list
			kopano_end_soap_connection(iterSockets->second.soap);
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
		--n;
	}
		l_sock.unlock();

        // Search for activity on listen sockets
		for (const auto &p : m_setListenSockets) {
			if (!FD_ISSET(p.second->socket, &readfds))
				continue;
			ACTIVESOCKET sActive;
			auto newsoap = soap_copy(p.second);
			if (newsoap == NULL) {
				ec_log_crit("Unable to accept new connection: out of memory");
				continue;
			}
			kopano_new_soap_connection(SOAP_CONNECTION_TYPE(p.second), newsoap);
			// Record last activity (now)
			time(&sActive.ulLastActivity);
			ulType = SOAP_CONNECTION_TYPE(p.second);
			if (ulType == CONNECTION_TYPE_NAMED_PIPE ||
			    ulType == CONNECTION_TYPE_NAMED_PIPE_PRIORITY) {
				int socket = accept(newsoap->master, NULL, 0);
				newsoap->socket = socket;
			} else {
				soap_accept(newsoap);
			}
			if (newsoap->socket == SOAP_INVALID_SOCKET) {
				if (ulType == CONNECTION_TYPE_NAMED_PIPE)
					ec_log_debug("Error accepting incoming connection from file://%s", m_lpConfig->GetSetting("server_pipe_name"));
				else if (ulType == CONNECTION_TYPE_NAMED_PIPE_PRIORITY)
					ec_log_debug("Error accepting incoming connection from file://%s", m_lpConfig->GetSetting("server_pipe_priority"));
				else
					ec_log_debug("Error accepting incoming connection from network.");
				kopano_end_soap_connection(newsoap);
				soap_free(newsoap);
				continue;
			}
			if (ulType == CONNECTION_TYPE_NAMED_PIPE)
				ec_log_debug("Accepted incoming connection from file://%s", m_lpConfig->GetSetting("server_pipe_name"));
			else if (ulType == CONNECTION_TYPE_NAMED_PIPE_PRIORITY)
				ec_log_debug("Accepted incoming connection from file://%s", m_lpConfig->GetSetting("server_pipe_priority"));
			else
				ec_log_debug("Accepted incoming%sconnection from %s",
					ulType == CONNECTION_TYPE_SSL ? " SSL ":" ",
					newsoap->host);
			newsoap->socket = ec_relocate_fd(newsoap->socket);
			g_lpStatsCollector->Max(SCN_MAX_SOCKET_NUMBER, (LONGLONG)newsoap->socket);
			g_lpStatsCollector->Increment(SCN_SERVER_CONNECTIONS);
			sActive.soap = newsoap;
			l_sock.lock();
			m_setSockets.insert(std::make_pair(sActive.soap->socket, sActive));
			l_sock.unlock();
		}
	}

    // Delete the watchdog. This makes sure no new threads will be started.
    delete lpWatchDog;
    
    // Set the thread count to zero so that threads will exit
    m_lpThreadManager->SetThreadCount(0);

    // Notify threads that they should re-query their idle state (and exit)
    ulock_normal l_item(m_mutexItems);
    m_condItems.notify_all();
    m_condPrioItems.notify_all();
    l_item.unlock();
    
    // Delete thread manager (waits for threads to become idle). During this time
    // the threads may report back a workitem as being done. If this is the case, we directly close that socket too.
    delete m_lpThreadManager;
    
    // Empty the queue
	l_item.lock();
    while(!m_queueItems.empty()) { kopano_end_soap_connection(m_queueItems.front()->soap); soap_free(m_queueItems.front()->soap); m_queueItems.pop(); }
    while(!m_queuePrioItems.empty()) { kopano_end_soap_connection(m_queuePrioItems.front()->soap); soap_free(m_queuePrioItems.front()->soap); m_queuePrioItems.pop(); }
	l_item.unlock();

	// Close all listener sockets. 
	for (const auto &p : m_setListenSockets) {
		kopano_end_soap_listener(p.second);
		soap_free(p.second);
	}
	// Close all sockets. This will cause all that we were listening on clients to get an EOF
	ulock_normal l_sock(m_mutexSockets);
	for (const auto &p : m_setSockets) {
		kopano_end_soap_connection(p.second.soap);
		soap_free(p.second.soap);
	}
	l_sock.unlock();
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
ECDispatcherEPoll::ECDispatcherEPoll(ECConfig *lpConfig,
    CREATEPIPESOCKETCALLBACK lpCallback, void *lpCallbackParam) :
	ECDispatcher(lpConfig, lpCallback, lpCallbackParam)
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
	std::map<int, struct soap *>::const_iterator iterListenSockets;
	CONNECTION_TYPE ulType;

	epoll_event epevent;
	epoll_event *epevents;
	int n;

	epevents = new epoll_event[m_fdMax];

	// setup epoll for listen sockets
	memset(&epevent, 0, sizeof(epoll_event));

	epevent.events = EPOLLIN | EPOLLPRI; // wait for input and priority (?) events

	for (iterListenSockets = m_setListenSockets.begin();
	     iterListenSockets != m_setListenSockets.end();
	     ++iterListenSockets) {
		epevent.data.fd = iterListenSockets->second->socket; 
		epoll_ctl(m_epFD, EPOLL_CTL_ADD, iterListenSockets->second->socket, &epevent);
	}

	// This will start the threads
	m_lpThreadManager = new ECThreadManager(this, atoui(m_lpConfig->GetSetting("threads")));

	// Start the watchdog
	lpWatchDog = new ECWatchDog(m_lpConfig, this, m_lpThreadManager);

	while (!m_bExit) {
		time(&now);

		// find timedout sockets once per second
		ulock_normal l_sock(m_mutexSockets);
		if(now > last) {
            iterSockets = m_setSockets.begin();
            while (iterSockets != m_setSockets.end()) {
                ulType = SOAP_CONNECTION_TYPE(iterSockets->second.soap);
                if (ulType != CONNECTION_TYPE_NAMED_PIPE &&
                    ulType != CONNECTION_TYPE_NAMED_PIPE_PRIORITY &&
                    now - static_cast<time_t>(iterSockets->second.ulLastActivity) > m_nRecvTimeout)
                    // Socket has been inactive for more than server_recv_timeout seconds, close the socket
                    shutdown(iterSockets->second.soap->socket, SHUT_RDWR);
                ++iterSockets;
            }
            last = now;
        }
		l_sock.unlock();

		n = epoll_wait(m_epFD, epevents, m_fdMax, 1000); // timeout -1 is wait indefinitely
		l_sock.lock();
		for (int i = 0; i < n; ++i) {
			iterListenSockets = m_setListenSockets.find(epevents[i].data.fd);

			if (iterListenSockets != m_setListenSockets.end()) {
				// this was a listen socket .. accept and continue
				struct soap *newsoap;
				ACTIVESOCKET sActive;

				newsoap = soap_copy(iterListenSockets->second);
                kopano_new_soap_connection(SOAP_CONNECTION_TYPE(iterListenSockets->second), newsoap);

				// Record last activity (now)
				time(&sActive.ulLastActivity);

				ulType = SOAP_CONNECTION_TYPE(iterListenSockets->second);
				if (ulType == CONNECTION_TYPE_NAMED_PIPE || ulType == CONNECTION_TYPE_NAMED_PIPE_PRIORITY)
					newsoap->socket = accept(newsoap->master, NULL, 0);
				else
					soap_accept(newsoap);

				if(newsoap->socket == SOAP_INVALID_SOCKET) {
					if (ulType == CONNECTION_TYPE_NAMED_PIPE)
						ec_log_debug("Error accepting incoming connection from file://%s", m_lpConfig->GetSetting("server_pipe_name"));
					else if (ulType == CONNECTION_TYPE_NAMED_PIPE_PRIORITY)
						ec_log_debug("Error accepting incoming connection from file://%s", m_lpConfig->GetSetting("server_pipe_priority"));
					else
						ec_log_debug("Error accepting incoming connection from network.");
					kopano_end_soap_connection(newsoap);
					soap_free(newsoap);
				} else {
					if (ulType == CONNECTION_TYPE_NAMED_PIPE)
						ec_log_debug("Accepted incoming connection from file://%s", m_lpConfig->GetSetting("server_pipe_name"));
					else if (ulType == CONNECTION_TYPE_NAMED_PIPE_PRIORITY)
						ec_log_debug("Accepted incoming connection from file://%s", m_lpConfig->GetSetting("server_pipe_priority"));
					else
						ec_log_debug("Accepted incoming%sconnection from %s",
							ulType == CONNECTION_TYPE_SSL ? " SSL ":" ",
							newsoap->host);
					newsoap->socket = ec_relocate_fd(newsoap->socket);
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

				if (epevents[i].events & EPOLLHUP) {
					kopano_end_soap_connection(iterSockets->second.soap);
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
		l_sock.unlock();
	}

	// Delete the watchdog. This makes sure no new threads will be started.
	delete lpWatchDog;

    // Set the thread count to zero so that threads will exit
    m_lpThreadManager->SetThreadCount(0);

    // Notify threads that they should re-query their idle state (and exit)
	ulock_normal l_item(m_mutexItems);
	m_condItems.notify_all();
	m_condPrioItems.notify_all();
	l_item.unlock();
	delete m_lpThreadManager;

    // Empty the queue
	l_item.lock();
    while(!m_queueItems.empty()) { kopano_end_soap_connection(m_queueItems.front()->soap); soap_free(m_queueItems.front()->soap); m_queueItems.pop(); }
    while(!m_queuePrioItems.empty()) { kopano_end_soap_connection(m_queuePrioItems.front()->soap); soap_free(m_queuePrioItems.front()->soap); m_queuePrioItems.pop(); }
	l_item.unlock();

	// Close all listener sockets.
	for (iterListenSockets = m_setListenSockets.begin();
	     iterListenSockets != m_setListenSockets.end();
	     ++iterListenSockets) {
        kopano_end_soap_listener(iterListenSockets->second);
        soap_free(iterListenSockets->second);
    }
    // Close all sockets. This will cause all that we were listening on clients to get an EOF
	ulock_normal l_sock(m_mutexSockets);
    for (iterSockets = m_setSockets.begin(); iterSockets != m_setSockets.end(); ++iterSockets) {
        kopano_end_soap_connection(iterSockets->second.soap);
        soap_free(iterSockets->second.soap);
    }
	l_sock.unlock();
	delete [] epevents;
	return er;
}

ECRESULT ECDispatcherEPoll::NotifyRestart(SOAP_SOCKET s)
{
	// add soap socket in epoll fd
	epoll_event epevent;
	memset(&epevent, 0, sizeof(epoll_event));

	epevent.events = EPOLLIN | EPOLLPRI; // wait for input and priority (?) events
	epevent.data.fd = s;
	epoll_ctl(m_epFD, EPOLL_CTL_ADD, epevent.data.fd, &epevent);
	return erSuccess;
}
#endif
