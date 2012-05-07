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

#ifndef ECNOTIFICATIONMANAGER_H
#define ECNOTIFICATIONMANAGER_H

#include "ECSession.h"
#include "ECLogger.h"
#include "ECConfig.h"

#include <map>
#include <set>

/*
 * The notification manager runs in a single thread, servicing notifications to ALL clients
 * that are waiting for a notification. We simply store all waiting soap connection objects together
 * with their getNextNotify() request, and once we are signalled that something has changed for one
 * of those queues, we send the reply, and requeue the soap connection for the next request.
 *
 * So, basically we only handle the SOAP-reply part of the soap request.
 */
 
struct NOTIFREQUEST {
    struct soap *soap;
    time_t ulRequestTime;
};

class ECNotificationManager {
public:
    ECNotificationManager(ECLogger *lpLogger, ECConfig *lpConfig);
    ~ECNotificationManager();
    
    // Called by the SOAP handler
    HRESULT AddRequest(ECSESSIONID ecSessionId, struct soap *soap);

    // Called by a session when it has a notification to send
    HRESULT NotifyChange(ECSESSIONID ecSessionId);
    
private:
    // Just a wrapper to Work()
    static void * Thread(void *lpParam);
    void * Work();
    
    bool		m_bExit;
    pthread_t 	m_thread;
    
    unsigned int m_ulTimeout;

    ECLogger *m_lpLogger;
    ECConfig *m_lpConfig;

    // A map of all sessions that are waiting for a SOAP response to be sent (an item can be in here for up to 60 seconds)
    std::map<ECSESSIONID, NOTIFREQUEST> 	m_mapRequests;
    // A set of all sessions that have reported notification activity, but are yet to be processed.
    // (a session is in here for only very short periods of time, and contains only a few sessions even if the load is high)
    std::set<ECSESSIONID> 					m_setActiveSessions;
    
    pthread_mutex_t m_mutexRequests;
    pthread_mutex_t m_mutexSessions;
    pthread_cond_t m_condSessions;
};

#endif
