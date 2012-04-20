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

/// ECSessionManager.cpp: implementation of the ECSessionManager class.
//
//////////////////////////////////////////////////////////////////////
#include "platform.h"
 
 
#include <algorithm>
#include <typeinfo>

#include <mapidefs.h>
#include <mapitags.h>

#include "ECMAPI.h"
#include "ECDatabase.h"
#include "ECSessionGroup.h"
#include "ECSessionManager.h"
#include "ECStatsCollector.h"
#include "ECTPropsPurge.h"
#include "ECLicenseClient.h"

#include "ECDatabaseUtils.h"
#include "ECSecurity.h"
#include "SSLUtil.h"

#include "Trace.h"
#include "Zarafa.h"

#include "ECICS.h"
#include "edkmdb.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

ECSessionManager::ECSessionManager(ECConfig *lpConfig, ECLogger *lpLogger, ECLogger *lpAudit, bool bHostedZarafa, bool bDistributedZarafa)
{
	int err = 0;

	bExit					= FALSE;
	m_lpConfig				= lpConfig;
	m_lpLogger				= lpLogger;
	m_lpAudit				= lpAudit;
	m_bHostedZarafa			= bHostedZarafa;
	m_bDistributedZarafa	= bDistributedZarafa;
	m_lpDatabase			= NULL;

	// Create a rwlock with no initial owner.
	pthread_rwlock_init(&m_hCacheRWLock, NULL);
	pthread_rwlock_init(&m_hGroupLock, NULL);
	pthread_mutex_init(&m_hExitMutex, NULL);
	pthread_mutex_init(&m_mutexPersistent, NULL);
	pthread_mutex_init(&m_mutexTableSubscriptions, NULL);
	pthread_mutex_init(&m_mutexObjectSubscriptions, NULL);

	//Terminate Event
	pthread_cond_init(&m_hExitSignal, NULL);

	m_lpDatabaseFactory = new ECDatabaseFactory(lpConfig, lpLogger);
	m_lpPluginFactory = new ECPluginFactory(lpConfig, lpLogger, g_lpStatsCollector, bHostedZarafa, bDistributedZarafa);

	m_lpECCacheManager = new ECCacheManager(lpConfig, m_lpDatabaseFactory, lpLogger);
	m_lpSearchFolders = new ECSearchFolders(this, m_lpDatabaseFactory, lpLogger);

	m_lpTPropsPurge = new ECTPropsPurge(lpConfig, lpLogger, m_lpDatabaseFactory);

	m_ptrLockManager = ECLockManager::Create();
	
	m_lpServerGuid = NULL;
	m_ullSourceKeyAutoIncrement = 0;
	m_ulSourceKeyQueue = 0;

	m_ulSeqIMAP = 0;
	m_ulSeqIMAPQueue = 0;

	pthread_mutex_init(&m_hSourceKeyAutoIncrementMutex, NULL);
	pthread_mutex_init(&m_hSeqMutex, NULL);

	// init ssl randomness for session id's
	ssl_random_init();

	//Create session clean up thread
	err = pthread_create(&m_hSessionCleanerThread, NULL, SessionCleaner, (void*)this);
	
	if(err != 0) {
		m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to spawn thread for session cleaner! Sessions will live forever!: %s", strerror(err));
	}

    m_lpNotificationManager = new ECNotificationManager(m_lpLogger, m_lpConfig);

    m_ulLicensedUsers = (unsigned int)-1;
}

ECSessionManager::~ECSessionManager()
{
	int err = 0;
	SESSIONMAP::iterator iSession, iSessionNext;

	pthread_mutex_lock(&m_hExitMutex);
	bExit = TRUE;
	pthread_cond_signal(&m_hExitSignal);

	pthread_mutex_unlock(&m_hExitMutex);

	if(m_lpTPropsPurge)
		delete m_lpTPropsPurge;

	if(m_lpDatabase)
		delete m_lpDatabase;

	if(m_lpDatabaseFactory)
		delete m_lpDatabaseFactory;
		
	err = pthread_join(m_hSessionCleanerThread, NULL);
	
	if(err != 0) {
		m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to join session cleaner thread: %s", strerror(err));
	}

	pthread_rwlock_wrlock(&m_hCacheRWLock);
			
	/* Clean up all sessions */
	iSession = m_mapSessions.begin();
	while(iSession != m_mapSessions.end()) {
		delete iSession->second;
		iSessionNext = iSession;
		iSessionNext++;

		m_lpLogger->Log(EC_LOGLEVEL_INFO, "End of session (shutdown) %llu", iSession->first);

		m_mapSessions.erase(iSession);

		iSession = iSessionNext;
	}
	
	if(m_lpNotificationManager)
	    delete m_lpNotificationManager;

	if(m_lpECCacheManager)
		delete m_lpECCacheManager;

	if(m_lpSearchFolders)
		delete m_lpSearchFolders;

	if(m_lpPluginFactory)
		delete m_lpPluginFactory;

	if(m_lpServerGuid)
		delete m_lpServerGuid;

	// Release ownership of the rwlock object.
	pthread_rwlock_unlock(&m_hCacheRWLock);

	pthread_rwlock_destroy(&m_hCacheRWLock);
	pthread_rwlock_destroy(&m_hGroupLock);

	pthread_mutex_destroy(&m_hSourceKeyAutoIncrementMutex);
	pthread_mutex_destroy(&m_hSeqMutex);

	pthread_mutex_destroy(&m_hExitMutex);
	pthread_mutex_destroy(&m_mutexPersistent);
	
	pthread_mutex_destroy(&m_mutexTableSubscriptions);
	pthread_mutex_destroy(&m_mutexObjectSubscriptions);	

	pthread_cond_destroy(&m_hExitSignal);
}

ECRESULT ECSessionManager::LoadSettings(){
	ECRESULT		er = erSuccess;
		
	ECDatabase *	lpDatabase = NULL;
	DB_RESULT		lpDBResult = NULL;
	DB_ROW			lpDBRow = NULL;
	DB_LENGTHS		lpDBLenths = NULL;
	std::string		strQuery;

	if(m_lpServerGuid != NULL){
		er = ZARAFA_E_BAD_VALUE;
		goto exit;
	}

	er = GetThreadLocalDatabase(m_lpDatabaseFactory, &lpDatabase);
	if(er != erSuccess)
		goto exit;
	
	strQuery = "SELECT `value` FROM settings WHERE `name` = 'server_guid'";
	er = lpDatabase->DoSelect(strQuery, &lpDBResult);
	if(er != erSuccess)
		goto exit;

	lpDBRow = lpDatabase->FetchRow(lpDBResult);
	lpDBLenths = lpDatabase->FetchRowLengths(lpDBResult);
	if(lpDBRow == NULL || lpDBRow[0] == NULL || lpDBLenths == NULL || lpDBLenths[0] != sizeof(GUID)) {
		er = ZARAFA_E_NOT_FOUND;
		goto exit;
	}

	m_lpServerGuid = new GUID;

	memcpy(m_lpServerGuid, lpDBRow[0], sizeof(GUID));

	if (lpDBResult)
		lpDatabase->FreeResult(lpDBResult);
	lpDBResult = NULL;

	strQuery = "SELECT `value` FROM settings WHERE `name` = 'source_key_auto_increment'";
	er = lpDatabase->DoSelect(strQuery, &lpDBResult);
	if(er != erSuccess)
		goto exit;

	lpDBRow = lpDatabase->FetchRow(lpDBResult);
	lpDBLenths = lpDatabase->FetchRowLengths(lpDBResult);
	if(lpDBRow == NULL || lpDBRow[0] == NULL || lpDBLenths == NULL || lpDBLenths[0] != 8) {
		er = ZARAFA_E_NOT_FOUND;
		goto exit;
	}

	memcpy(&m_ullSourceKeyAutoIncrement, lpDBRow[0], sizeof(m_ullSourceKeyAutoIncrement));

exit:
	if (lpDBResult)
		lpDatabase->FreeResult(lpDBResult);

	return er;
}

ECRESULT ECSessionManager::CheckUserLicense()
{
	ECRESULT er = erSuccess;
	ECSession *lpecSession = NULL;
	unsigned int ulLicense = 0;

	er = this->CreateSessionInternal(&lpecSession);
	if (er != erSuccess)
		goto exit;

	lpecSession->Lock();

	er = lpecSession->GetUserManagement()->CheckUserLicense(&ulLicense);
	if (er != erSuccess)
		goto exit;

	if (ulLicense & USERMANAGEMENT_USER_LICENSE_EXCEEDED) {
		m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Failed to start server: Your license doesn't permit this amount of users.");
		m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Stop your zarafa-license service, start the zarafa-server and remove users to resolve the problem.");
		er = ZARAFA_E_NO_ACCESS;
		goto exit;
	}

exit:
	if(lpecSession) {
		lpecSession->Unlock(); // Lock the session
		this->RemoveSessionInternal(lpecSession);
	}

	return er;
}

/*
 * This function is threadsafe since we hold the lock the the group list, and the session retrieved from the grouplist
 * is locked so it cannot be deleted by other sessions, while we hold the lock for the group list.
 *
 * Other sessions may release the session group, even if they are the last, while we are in this function since
 * deletion of the session group only occurs within DeleteIfOrphaned(), and this function guarantees that the caller
 * will receive a sessiongroup that is not an orphan unless the caller releases the session group.
 */
ECRESULT ECSessionManager::GetSessionGroup(ECSESSIONGROUPID sessionGroupID, ECSession *lpSession, ECSessionGroup **lppSessionGroup)
{
	ECRESULT er = erSuccess;
	ECSessionGroup *lpSessionGroup = NULL;

	pthread_rwlock_rdlock(&m_hGroupLock);

	/* Workaround for old clients, when sessionGroupID is 0 each session is its own group */
	if (sessionGroupID == 0) {
		lpSessionGroup = new ECSessionGroup(sessionGroupID, this);
		g_lpStatsCollector->Increment(SCN_SESSIONGROUPS_CREATED);
	} else {
		SESSIONGROUPMAP::iterator iter = m_mapSessionGroups.find(sessionGroupID);
		/* Check if the SessionGroup already exists on the server */
		if (iter == m_mapSessionGroups.end()) {
			// "upgrade" lock to insert new session
			pthread_rwlock_unlock(&m_hGroupLock);
			pthread_rwlock_wrlock(&m_hGroupLock);
			lpSessionGroup = new ECSessionGroup(sessionGroupID, this);
			m_mapSessionGroups.insert(SESSIONGROUPMAP::value_type(sessionGroupID, lpSessionGroup));
			g_lpStatsCollector->Increment(SCN_SESSIONGROUPS_CREATED);
		} else
			lpSessionGroup = iter->second;
	}
	
	lpSessionGroup->AddSession(lpSession);

	pthread_rwlock_unlock(&m_hGroupLock);

	*lppSessionGroup = lpSessionGroup;

	return er;
}

ECRESULT ECSessionManager::DeleteIfOrphaned(ECSessionGroup *lpGroup)
{
	ECRESULT er = erSuccess;
	ECSessionGroup *lpSessionGroup = NULL;
	ECSESSIONGROUPID id = lpGroup->GetSessionGroupId();

	if (id != 0) {
		pthread_rwlock_wrlock(&m_hGroupLock);

    	/* Check if the SessionGroup actually exists, if it doesn't just return without error */
    	SESSIONGROUPMAP::iterator i = m_mapSessionGroups.find(id);
    	if (i == m_mapSessionGroups.end()) {
    	    pthread_rwlock_unlock(&m_hGroupLock);
    	    goto exit;
    	}

    	/* If this was the last Session, delete the SessionGroup */
    	if (i->second->isOrphan()) {
    	    lpSessionGroup = i->second;
    	    m_mapSessionGroups.erase(i);
    	}

		pthread_rwlock_unlock(&m_hGroupLock);
	} else
		lpSessionGroup = lpGroup;

	if (lpSessionGroup) {
		delete lpSessionGroup;
		g_lpStatsCollector->Increment(SCN_SESSIONGROUPS_DELETED);
	}

exit:
	return er;
}

BTSession* ECSessionManager::GetSession(ECSESSIONID sessionID, bool fLockSession) {

	SESSIONMAP::iterator iIterator;
	BTSession *lpSession = NULL;
		
	//TRACE_INTERNAL(TRACE_ENTRY, "ECSessionManager", "GetSession", "%lu", sessionID);

	iIterator = m_mapSessions.find(sessionID);

	if(iIterator != m_mapSessions.end()){
		lpSession = iIterator->second;
		lpSession->UpdateSessionTime();
		
		if(fLockSession)
			lpSession->Lock();
			
        lpSession->IncRequests();
	}else{
		//EC_SESSION_LOST
	}
	
	//TRACE_INTERNAL(TRACE_RETURN, "ECSessionManager", "GetSession", "%lu", sessionID);
	return lpSession;
}

// Clean up all current sessions
ECRESULT ECSessionManager::RemoveAllSessions()
{
	ECRESULT		er = erSuccess;
	BTSession		*lpSession = NULL;
	SESSIONMAP::iterator iIterSession, iSessionNext;
	std::list<BTSession *> lstSessions;
	std::list<BTSession *>::iterator iterSessionList;
	
	// Lock the session map since we're going to remove all the sessions.
	pthread_rwlock_wrlock(&m_hCacheRWLock);

	m_lpLogger->Log(EC_LOGLEVEL_INFO, "Shutdown all current sessions");

	iIterSession = m_mapSessions.begin();
	while(iIterSession != m_mapSessions.end())
	{
		lpSession = iIterSession->second;
		iSessionNext = iIterSession;
		iSessionNext++;

		m_mapSessions.erase(iIterSession);

		iIterSession = iSessionNext;

        lstSessions.push_back(lpSession);
	}

	// Release ownership of the mutex object.
	pthread_rwlock_unlock(&m_hCacheRWLock);
	
	// Do the actual session deletes, while the session map is not locked (!)
	for(iterSessionList = lstSessions.begin(); iterSessionList != lstSessions.end(); iterSessionList++) {
	    delete *iterSessionList;
	}
	
	return er;
}

// call a function for all sessions available
// used by ECStatsTable
ECRESULT ECSessionManager::ForEachSession(void(*callback)(ECSession*, void*), void *obj)
{
	ECRESULT er = erSuccess;
	SESSIONMAP::iterator iIterSession;

	pthread_rwlock_rdlock(&m_hCacheRWLock);

	for (iIterSession = m_mapSessions.begin(); iIterSession != m_mapSessions.end(); iIterSession++) {
		callback(dynamic_cast<ECSession*>(iIterSession->second), obj);
	}

	pthread_rwlock_unlock(&m_hCacheRWLock);

	return er;
}


// Locking of sessions works as follows:
//
// - A session is requested by the caller thread through ValidateSession. ValidateSession
//   Locks the complete session table, then acquires a lock on the session, and then
//   frees the lock on the session table. This makes sure that when a session is returned,
//   it is guaranteed not to be deleted by another thread (due to a shutdown or logoff).
//   The caller of 'ValidateSession' is therefore responsible for unlocking the session
//   when it is finished.
//
// - When a session is terminated, a lock is opened on the session table, making sure no
//   new session can be opened, or session can be deleted. Then, the session is searched
//   in the table, and directly deleted from the table, making sure that no new threads can
//   open the session in question after this point. Then, the session is deleted, but the
//   session itself waits in the destructor until all threads holding a lock on the session
//   through Lock or ValidateSession have released their lock, before actually deleting the
//   session object.
//
// This means that exiting the server must wait until all client requests have exited. For
// most operations, this is not a problem, but for some long requests (ie large deletes or
// copies, or GetNextNotifyItem) may take quite a while to exit. This is compensated for, by
// having the session call a 'cancel' request to long-running calls, which makes the calls
// exit prematurely.
//

ECRESULT ECSessionManager::ValidateSession(struct soap *soap, ECSESSIONID sessionID, ECAuthSession **lppSession, bool fLockSession)
{
	ECRESULT er = erSuccess;
	BTSession *lpSession = NULL;

	er = this->ValidateBTSession(soap, sessionID, &lpSession, fLockSession);
	if (er != erSuccess)
		goto exit;

	*lppSession = dynamic_cast<ECAuthSession*>(lpSession);

exit:
	return er;
}

ECRESULT ECSessionManager::ValidateSession(struct soap *soap, ECSESSIONID sessionID, ECSession **lppSession, bool fLockSession)
{
	ECRESULT er = erSuccess;
	BTSession *lpSession = NULL;

	er = this->ValidateBTSession(soap, sessionID, &lpSession, fLockSession);
	if (er != erSuccess)
		goto exit;

	*lppSession = dynamic_cast<ECSession*>(lpSession);

exit:
	return er;
}

ECRESULT ECSessionManager::ValidateBTSession(struct soap *soap, ECSESSIONID sessionID, BTSession **lppSession, bool fLockSession)
{
	ECRESULT		er			= erSuccess;
	BTSession*		lpSession	= NULL;
	
	// Read lock
	pthread_rwlock_rdlock(&m_hCacheRWLock);
	
	lpSession = GetSession(sessionID, fLockSession);
	
	pthread_rwlock_unlock(&m_hCacheRWLock);

	if(lpSession == NULL) {
		er = ZARAFA_E_END_OF_SESSION;
		goto exit;
	}
	
	er = lpSession->ValidateIp(soap->ip);
	if (er != erSuccess) {
		if (fLockSession)
			lpSession->Unlock();
		lpSession = NULL;
		goto exit;
	}

	// Enable compression if client is capable
	if (lpSession->GetCapabilities() & ZARAFA_CAP_COMPRESSION) {
		soap_set_imode(soap, SOAP_ENC_ZLIB);
		soap_set_omode(soap, SOAP_ENC_ZLIB | SOAP_IO_CHUNK);
	}

	// Enable streaming support if client is capable
	if (lpSession->GetCapabilities() & ZARAFA_CAP_ENHANCED_ICS) {
		soap_set_omode(soap, SOAP_ENC_MTOM | SOAP_IO_CHUNK);
		soap_set_imode(soap, SOAP_ENC_MTOM);
		soap_post_check_mime_attachments(soap);	
	}

	*lppSession = lpSession;

exit:
	return er;
}

ECRESULT ECSessionManager::CreateAuthSession(struct soap *soap, unsigned int ulCapabilities, ECSESSIONID *sessionID, ECAuthSession **lppAuthSession, bool bRegisterSession, bool bLockSession)
{
	ECRESULT er = erSuccess;
	ECAuthSession *lpAuthSession = NULL;
	ECSESSIONID newSessionID;

	CreateSessionID(ulCapabilities, &newSessionID);

	lpAuthSession = new ECAuthSession(soap->ip, newSessionID, m_lpDatabaseFactory, this, ulCapabilities);
	if (lpAuthSession) {
	    if (bLockSession) {
	        lpAuthSession->Lock();
	    }
		if (bRegisterSession) {
			pthread_rwlock_wrlock(&m_hCacheRWLock);
			m_mapSessions.insert( SESSIONMAP::value_type(newSessionID, lpAuthSession) );
			pthread_rwlock_unlock(&m_hCacheRWLock);
			g_lpStatsCollector->Increment(SCN_SESSIONS_CREATED);
		}

		*sessionID = newSessionID;
		*lppAuthSession = lpAuthSession;
	} else
		er = ZARAFA_E_NOT_ENOUGH_MEMORY;

	return er;
}

ECRESULT ECSessionManager::CreateSession(struct soap *soap, char *szName, char *szPassword, char *szClientVersion, char *szClientApp, unsigned int ulCapabilities, ECSESSIONGROUPID sessionGroupID, ECSESSIONID *lpSessionID, ECSession **lppSession, bool fLockSession)
{
	ECRESULT		er			= erSuccess;
	ECAuthSession	*lpAuthSession	= NULL;
	ECSession		*lpSession	= NULL;
	const char		*method = "error";
	std::string		from;
	CONNECTION_TYPE ulType;

	zarafa_get_soap_connection_type(soap, &ulType);
	if (ulType == CONNECTION_TYPE_NAMED_PIPE_PRIORITY) {
		from = string("file://") + m_lpConfig->GetSetting("server_pipe_priority");
	} else if (ulType == CONNECTION_TYPE_NAMED_PIPE) {
		// connected through unix socket
		from = string("file://") + m_lpConfig->GetSetting("server_pipe_name");
	} else {
		// connected over network
		from = PrettyIP(soap->ip);
	}

	er = this->CreateAuthSession(soap, ulCapabilities, lpSessionID, &lpAuthSession, false, false);
	if (er != erSuccess)
		goto exit;

	// If we've connected with SSL, check if there is a certificate, and check if we accept that certificate for that user
	if (soap->ssl && lpAuthSession->ValidateUserCertificate(soap, szName) == erSuccess) {
		g_lpStatsCollector->Increment(SCN_LOGIN_SSL);
		method = "SSL Certificate";
		goto authenticated;
	}

	// First, try socket authentication (dagent, won't print error)
	if(lpAuthSession->ValidateUserSocket(soap->socket, szName) == erSuccess) {
		g_lpStatsCollector->Increment(SCN_LOGIN_SOCKET);
		method = "Pipe socket";
		goto authenticated;
	}

	// If that fails, try logon with supplied username/password (clients, may print logon error)
	if(lpAuthSession->ValidateUserLogon(szName, szPassword) == erSuccess) {
		g_lpStatsCollector->Increment(SCN_LOGIN_PASSWORD);
		method = "User supplied password";
		goto authenticated;
	}

	// whoops, out of auth options.
	m_lpLogger->Log(EC_LOGLEVEL_WARNING, "Failed to authenticate user %s from %s using program %s",
					szName, from.c_str(), szClientApp ? szClientApp : "<unknown>");

	LOG_AUDIT(m_lpAudit, "authenticate failed user='%s' from='%s' program='%s'",
			  szName, from.c_str(), szClientApp ? szClientApp : "<unknown>");

	er = ZARAFA_E_LOGON_FAILED;			
	g_lpStatsCollector->Increment(SCN_LOGIN_DENIED);
	goto exit;


authenticated:
	m_lpLogger->Log(EC_LOGLEVEL_NOTICE, "User %s from %s authenticated through %s using program %s",
					szName, from.c_str(), method, szClientApp ? szClientApp : "<unknown>");
	if (strcmp(ZARAFA_SYSTEM_USER, szName) != 0) {
		// don't log successfull SYSTEM logins
		LOG_AUDIT(m_lpAudit, "authenticate ok user='%s' from='%s' method='%s' program='%s'",
				  szName, from.c_str(), method, szClientApp ? szClientApp : "<unknown>");
	}

	er = RegisterSession(lpAuthSession, sessionGroupID, szClientVersion, szClientApp, lpSessionID, &lpSession, fLockSession);
	if (er != erSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_FATAL, "User %s authenticated, but failed to create session. Error 0x%08X", szName, er);
		goto exit;
	}
	m_lpLogger->Log(EC_LOGLEVEL_INFO, "User %s receives session %llu", szName, *lpSessionID);

exit:
	if (lpAuthSession)
		delete lpAuthSession;

	*lppSession = lpSession;

	return er;
}

ECRESULT ECSessionManager::RegisterSession(ECAuthSession *lpAuthSession, ECSESSIONGROUPID sessionGroupID, char *szClientVersion, char *szClientApp, ECSESSIONID *lpSessionID, ECSession **lppSession, bool fLockSession)
{
	ECRESULT	er = erSuccess;
	ECSession	*lpSession = NULL;
	ECSessionGroup *lpSessionGroup = NULL;
	ECSESSIONID	newSID = 0;

	er = lpAuthSession->CreateECSession(sessionGroupID, szClientVersion ? szClientVersion : "", szClientApp ? szClientApp : "", &newSID, &lpSession);
	if (er != erSuccess)
		goto exit;

	if (fLockSession)
		lpSession->Lock();

	pthread_rwlock_wrlock(&m_hCacheRWLock);
	m_mapSessions.insert( SESSIONMAP::value_type(newSID, lpSession) );
	pthread_rwlock_unlock(&m_hCacheRWLock);

	*lpSessionID = newSID;
	*lppSession = lpSession;

	g_lpStatsCollector->Increment(SCN_SESSIONS_CREATED);
	
exit:
	/* SessionGroup could have been created for this session, if creation failed, just kill it immediately */
	if (er != erSuccess && lpSessionGroup)
		DeleteIfOrphaned(lpSessionGroup);

	return er;
}

ECRESULT ECSessionManager::CreateSessionInternal(ECSession **lppSession, unsigned int ulUserId)
{
	ECRESULT	er			= erSuccess;
	ECSession	*lpSession	= NULL;
	ECSESSIONID	newSID;

	CreateSessionID(ZARAFA_CAP_LARGE_SESSIONID, &newSID);

	lpSession = new ECSession(0, newSID, 0, m_lpDatabaseFactory, this, 0, false, ECSession::METHOD_NONE, 0, "internal", "zarafa-server");
	if(lpSession == NULL) {
		er = ZARAFA_E_LOGON_FAILED;
		goto exit;
	}

	er = lpSession->GetSecurity()->SetUserContext(ulUserId);
	if (er != erSuccess) {
		delete lpSession;
		goto exit;
	}

	m_lpLogger->Log(EC_LOGLEVEL_INFO, "New internal session (%llu)", newSID);

	g_lpStatsCollector->Increment(SCN_SESSIONS_INTERNAL_CREATED);

	*lppSession = lpSession;

exit:
	return er;
}

void ECSessionManager::RemoveSessionInternal(ECSession *lpSession)
{
	if (lpSession != NULL) {
		g_lpStatsCollector->Increment(SCN_SESSIONS_INTERNAL_DELETED);
		delete lpSession;
	}
}

ECRESULT ECSessionManager::RemoveSession(ECSESSIONID sessionID){

	ECRESULT	hr			= erSuccess;
	BTSession	*lpSession	= NULL;
	
	m_lpLogger->Log(EC_LOGLEVEL_INFO, "End of session (logoff) %llu", sessionID);
	g_lpStatsCollector->Increment(SCN_SESSIONS_DELETED);

	// Make sure no other thread can read or write the sessions list
	pthread_rwlock_wrlock(&m_hCacheRWLock);

	// Get a session, don't lock it ourselves
	lpSession = GetSession(sessionID, false);

	// Remove the session from the list. No other threads can start new
	// requests on the session after this point
	m_mapSessions.erase(sessionID);

	// Release the mutex, other threads can now access the (updated) sessions list
	pthread_rwlock_unlock(&m_hCacheRWLock);

	// We know for sure that no other thread is attempting to remove the session
	// at this time because it would not have been in the m_mapSessions map

	// Delete the session. This will block until all requesters on the session
	// have released their lock on the session
	if(lpSession != NULL) {
		if(lpSession->Shutdown(5 * 60 * 1000) == erSuccess)
			delete lpSession;
		else
			m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Session failed to shut down: skipping logoff");
	}
		
    // Tell the notification manager to wake up anyone waiting for this session
    m_lpNotificationManager->NotifyChange(sessionID);

	return hr;
}

/** 
 * Add notification to a session group.
 * @note This function can't handle table notifications!
 * 
 * @param[in] notifyItem The notification data to send
 * @param[in] ulKey The object (hierarchyid) the notification acts on
 * @param[in] ulStore The store the ulKey object resides in. 0 for unknown (default).
 * @param[in] ulFolderId Parent folder object for ulKey. 0 for unknown or not required (default).
 * @param[in] ulFlags Hierarchy flags for ulKey. 0 for unknown (default).
 * 
 * @return Zarafa error code
 */
ECRESULT ECSessionManager::AddNotification(notification *notifyItem, unsigned int ulKey, unsigned int ulStore, unsigned int ulFolderId, unsigned int ulFlags) {
	
	SESSIONGROUPMAP::iterator	iIterator;
	std::multimap<unsigned int, ECSESSIONGROUPID>::iterator iterObjectSubscription;
	std::set<ECSESSIONGROUPID> setGroups;
	std::set<ECSESSIONGROUPID>::iterator iterGroups;
	
	ECRESULT				hr = erSuccess;
	
	if(ulStore == 0) {
		hr = m_lpECCacheManager->GetStore(ulKey, &ulStore, NULL);
		if(hr != erSuccess)
			goto exit;
	}

	pthread_mutex_lock(&m_mutexObjectSubscriptions);

	// Send notification to subscribed sessions
	iterObjectSubscription = m_mapObjectSubscriptions.lower_bound(ulStore);
	while(iterObjectSubscription != m_mapObjectSubscriptions.end() && iterObjectSubscription->first == ulStore) {
		// Send a notification only once to a session group, even if it has subscribed multiple times
		setGroups.insert(iterObjectSubscription->second);
		iterObjectSubscription++;
	}
	
	pthread_mutex_unlock(&m_mutexObjectSubscriptions);

	// Send each subscribed session group one notification
	for (iterGroups = setGroups.begin(); iterGroups != setGroups.end(); iterGroups++){
		pthread_rwlock_rdlock(&m_hGroupLock);
		iIterator = m_mapSessionGroups.find(*iterGroups);
		if(iIterator != m_mapSessionGroups.end())
			iIterator->second->AddNotification(notifyItem, ulKey, ulStore);
		pthread_rwlock_unlock(&m_hGroupLock);
	}
	
	// Next, do an internal notification to update searchfolder views for message updates.
	if(notifyItem->obj && notifyItem->obj->ulObjType == MAPI_MESSAGE) {
		if (ulFolderId == 0 && ulFlags == 0 ) {
			if(GetCacheManager()->GetObject(ulKey, &ulFolderId, NULL, &ulFlags, NULL) != erSuccess) {
				ASSERT(FALSE);
				goto exit;
			}
		}
            
        // Skip changes on associated messages, and changes on deleted item. (but include DELETE of deleted items)
        if((ulFlags & MAPI_ASSOCIATED) || (notifyItem->ulEventType != fnevObjectDeleted && (ulFlags & MSGFLAG_DELETED)))
            goto exit;

		switch(notifyItem->ulEventType) {
		case fnevObjectMoved:
		    // Only update the item in the new folder. The system will automatically delete the item from folders that were not in the search path
			m_lpSearchFolders->UpdateSearchFolders(ulStore, ulFolderId, ulKey, ECKeyTable::TABLE_ROW_MODIFY);
			break;
		case fnevObjectDeleted:
			m_lpSearchFolders->UpdateSearchFolders(ulStore, ulFolderId, ulKey, ECKeyTable::TABLE_ROW_DELETE);
			break;
		case fnevObjectCreated:
			m_lpSearchFolders->UpdateSearchFolders(ulStore, ulFolderId, ulKey, ECKeyTable::TABLE_ROW_ADD);
			break;
		case fnevObjectCopied:
			m_lpSearchFolders->UpdateSearchFolders(ulStore, ulFolderId, ulKey, ECKeyTable::TABLE_ROW_ADD);
			break;
		case fnevObjectModified:
			m_lpSearchFolders->UpdateSearchFolders(ulStore, ulFolderId, ulKey, ECKeyTable::TABLE_ROW_MODIFY);
			break;
		}
	}

exit:
	return hr;
}

void* ECSessionManager::SessionCleaner(void *lpTmpSessionManager)
{
	SESSIONMAP::iterator	iIterator, iRemove;
	time_t					lCurTime;
	ECSessionManager*		lpSessionManager = (ECSessionManager *)lpTmpSessionManager;
	int						lResult;
	struct timeval			now;
	struct timespec			timeout;
	list<BTSession*>		lstSessions;


	if(lpSessionManager == NULL) {
		return 0;
	}

	while(true){
		pthread_rwlock_wrlock(&lpSessionManager->m_hCacheRWLock);

		lCurTime = GetProcessTime();
		
		// Find a session that has timed out
		iIterator = lpSessionManager->m_mapSessions.begin();
		while( iIterator != lpSessionManager->m_mapSessions.end() ) {
			if((iIterator->second->GetSessionTime()) < lCurTime && !lpSessionManager->IsSessionPersistent(iIterator->first)){
				// Remember all the session to be deleted
				lstSessions.push_back(iIterator->second);

				iRemove = iIterator++;
				// Remove the session from the list, no new threads can start on this session after this point.
				g_lpStatsCollector->Increment(SCN_SESSIONS_TIMEOUT);
				lpSessionManager->m_lpLogger->Log(EC_LOGLEVEL_INFO, "End of session (timeout) %llu", iRemove->first);
				lpSessionManager->m_mapSessions.erase(iRemove);
			} else {
				iIterator++;
			}
		}

		// Release ownership of the rwlock object. This makes sure all threads are free to run (and exit).
		pthread_rwlock_unlock(&lpSessionManager->m_hCacheRWLock);

		// Now, remove all the session. It will wait until all running threads for that session have exited.
		for (list<BTSession*>::iterator iSessions = lstSessions.begin(); iSessions != lstSessions.end(); iSessions++) {
			if((*iSessions)->Shutdown(5 * 60 * 1000) == erSuccess) 
				delete *iSessions;
			else {
				// The session failed to shut down within our timeout period. This means we probably hit a bug; this
				// should only happen if some bit of code has locked the session and failed to unlock it. There are now
				// two options: delete the session anyway and hope we don't segfault, or leak the session. We choose
				// the latter.
				lpSessionManager->m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Session failed to shut down: skipping clean");
			}
		}

		lstSessions.clear();

		// Wait for a terminate signal or return after a few minutes
		pthread_mutex_lock(&lpSessionManager->m_hExitMutex);
		if(lpSessionManager->bExit) {
			pthread_mutex_unlock(&lpSessionManager->m_hExitMutex);
			break;
		}

		gettimeofday(&now,NULL); // null==timezone
		timeout.tv_sec = now.tv_sec + 5;
		timeout.tv_nsec = now.tv_usec * 1000;

		lResult = pthread_cond_timedwait(&lpSessionManager->m_hExitSignal, &lpSessionManager->m_hExitMutex, &timeout);

		if (lResult != ETIMEDOUT) {
			pthread_mutex_unlock(&lpSessionManager->m_hExitMutex);
			break;
		}
		pthread_mutex_unlock(&lpSessionManager->m_hExitMutex);
	}

	// Do not pthread_exit() because linuxthreads is broken and will not free any objects
	// pthread_exit(0);

	return NULL;
}

ECRESULT ECSessionManager::UpdateOutgoingTables(ECKeyTable::UpdateType ulType, unsigned int ulStoreId, unsigned int ulObjId, unsigned int ulFlags, unsigned int ulObjType)
{
    ECRESULT er = erSuccess;
	TABLESUBSCRIPTION sSubscription;

	sSubscription.ulType = TABLE_ENTRY::TABLE_TYPE_OUTGOINGQUEUE;
	sSubscription.ulRootObjectId = ulFlags & EC_SUBMIT_MASTER ? 0 : ulStoreId; // in the master queue, use 0 as root object id
	sSubscription.ulObjectType = ulObjType;
	sSubscription.ulObjectFlags = ulFlags & EC_SUBMIT_MASTER; // Only use MASTER flag as differentiator

	er = UpdateSubscribedTables(ulType, sSubscription, ulObjId);
	if(er != erSuccess)
	    goto exit;

exit:
    return er;
}

ECRESULT ECSessionManager::UpdateTables(ECKeyTable::UpdateType ulType, unsigned int ulFlags, unsigned ulObjId, unsigned ulChildId, unsigned int ulObjType)
{
    ECRESULT er = erSuccess;
	TABLESUBSCRIPTION sSubscription;
	
	if(ulObjType != MAPI_MESSAGE && ulObjType != MAPI_FOLDER)
		goto exit;

	sSubscription.ulType = TABLE_ENTRY::TABLE_TYPE_GENERIC;
	sSubscription.ulRootObjectId = ulObjId;
	sSubscription.ulObjectType = ulObjType;
	sSubscription.ulObjectFlags = ulFlags;

	er = UpdateSubscribedTables(ulType, sSubscription, ulChildId);

exit:
    return er;
}

ECRESULT ECSessionManager::UpdateSubscribedTables(ECKeyTable::UpdateType ulType, TABLESUBSCRIPTION sSubscription, unsigned int ulChildId)
{
	SESSIONMAP::iterator	iterSession;
	ECRESULT		er = erSuccess;
	std::set<ECSESSIONID> setSessions;
	std::set<ECSESSIONID>::iterator iterSubscribedSession;
	std::multimap<TABLESUBSCRIPTION, ECSESSIONID>::iterator iterSubscriptions;

	BTSession	*lpBTSession = NULL;
		
    // Find out which sessions our interested in this event by looking at our subscriptions
    pthread_mutex_lock(&m_mutexTableSubscriptions);
    
    iterSubscriptions = m_mapTableSubscriptions.find(sSubscription);
    while(iterSubscriptions != m_mapTableSubscriptions.end() && iterSubscriptions->first == sSubscription) {
        setSessions.insert(iterSubscriptions->second);
        iterSubscriptions++;
    }
    
    pthread_mutex_unlock(&m_mutexTableSubscriptions);

    // We now have a set of sessions that are interested in the notification. This list is normally quite small since not that many
    // sessions have the same table opened at one time.

    // For each of the sessions that are interested, send the table change
	for(iterSubscribedSession = setSessions.begin(); iterSubscribedSession != setSessions.end(); iterSubscribedSession++) {
		// Get session
		pthread_rwlock_rdlock(&m_hCacheRWLock);
		lpBTSession = GetSession(*iterSubscribedSession, true);
		pthread_rwlock_unlock(&m_hCacheRWLock);
	    
	    // Send the change notification
	    if(lpBTSession != NULL) {
			ECSession *lpSession = dynamic_cast<ECSession*>(lpBTSession);
	    	if (lpSession == NULL) {
				lpBTSession->Unlock();
	    	    continue;
			}
	    	
	    	if (sSubscription.ulType == TABLE_ENTRY::TABLE_TYPE_GENERIC) 
                lpSession->GetTableManager()->UpdateTables(ulType, sSubscription.ulObjectFlags, sSubscription.ulRootObjectId, ulChildId, sSubscription.ulObjectType);
            else if(sSubscription.ulType == TABLE_ENTRY::TABLE_TYPE_OUTGOINGQUEUE)
    			lpSession->GetTableManager()->UpdateOutgoingTables(ulType, sSubscription.ulRootObjectId, ulChildId, sSubscription.ulObjectFlags, sSubscription.ulObjectType);

			lpBTSession->Unlock();
        }
	}

	return er;
}
// FIXME: ulFolderId should be an entryid, because the parent is already deleted!
// You must specify which store the object was deleted from, 'cause we can't find out afterwards
ECRESULT ECSessionManager::NotificationDeleted(unsigned int ulObjType, unsigned int ulObjId, unsigned int ulStoreId, entryId* lpEntryId, unsigned int ulFolderId, unsigned int ulFlags)
{
	ECRESULT er = erSuccess;
	struct notification notify;

	memset(&notify, 0, sizeof(notification));

	if(ulObjType != MAPI_MESSAGE && ulObjType != MAPI_FOLDER && ulObjType != MAPI_STORE)
		goto exit;

	notify.obj = new notificationObject;
	memset(notify.obj, 0, sizeof(notificationObject));	
	
	notify.ulEventType			= fnevObjectDeleted;
	
	notify.obj->ulObjType		= ulObjType;
	notify.obj->pEntryId		= lpEntryId;

	if(ulFolderId > 0) {
		er = GetCacheManager()->GetEntryIdFromObject(ulFolderId, NULL, &notify.obj->pParentId);
		if(er != erSuccess)
			goto exit;
	}

	AddNotification(&notify, ulObjId, ulStoreId, ulFolderId, ulFlags);

exit:
	notify.obj->pEntryId = NULL;
	FreeNotificationStruct(&notify, false);

	return er;
}


ECRESULT ECSessionManager::NotificationModified(unsigned int ulObjType, unsigned int ulObjId, unsigned int ulParentId)
{
	ECRESULT er = erSuccess;
	struct notification notify;

	memset(&notify, 0, sizeof(notification));

	if(ulObjType != MAPI_MESSAGE && ulObjType != MAPI_FOLDER && ulObjType != MAPI_STORE)
		goto exit;

	notify.obj = new notificationObject;
	memset(notify.obj, 0, sizeof(notificationObject));
	
	notify.ulEventType			= fnevObjectModified;
	notify.obj->ulObjType		= ulObjType;

	er = GetCacheManager()->GetEntryIdFromObject(ulObjId, NULL, &notify.obj->pEntryId);
	if(er != erSuccess)
		goto exit;

	if(ulParentId > 0) {
		er = GetCacheManager()->GetEntryIdFromObject(ulParentId, NULL, &notify.obj->pParentId);
		if(er != erSuccess)
			goto exit;
	}

	AddNotification(&notify, ulObjId);

exit:
	FreeNotificationStruct(&notify, false);

	return er;
}

ECRESULT ECSessionManager::NotificationCreated(unsigned int ulObjType, unsigned int ulObjId, unsigned int ulParentId)
{
	ECRESULT er = erSuccess;
	struct notification notify;

	memset(&notify, 0, sizeof(notification));

	if(ulObjType != MAPI_MESSAGE && ulObjType != MAPI_FOLDER && ulObjType != MAPI_STORE)
		goto exit;

	notify.obj = new notificationObject;
	memset(notify.obj, 0, sizeof(notificationObject));

	notify.ulEventType			= fnevObjectCreated;
	notify.obj->ulObjType		= ulObjType;

	er = GetCacheManager()->GetEntryIdFromObject(ulObjId, NULL, &notify.obj->pEntryId);
	if(er != erSuccess)
		goto exit;

	er = GetCacheManager()->GetEntryIdFromObject(ulParentId, NULL, &notify.obj->pParentId);
	if(er != erSuccess)
		goto exit;
	

	AddNotification(&notify, ulObjId);

exit:
	FreeNotificationStruct(&notify, false);

	return er;
}

ECRESULT ECSessionManager::NotificationMoved(unsigned int ulObjType, unsigned int ulObjId, unsigned int ulParentId, unsigned int ulOldParentId, entryId *lpOldEntryId)
{
	ECRESULT er = erSuccess;
	struct notification notify;

	memset(&notify, 0, sizeof(notification));

	if(ulObjType != MAPI_MESSAGE && ulObjType != MAPI_FOLDER && ulObjType != MAPI_STORE)
		goto exit;

	notify.obj = new notificationObject;
	memset(notify.obj, 0, sizeof(notificationObject));

	notify.ulEventType				= fnevObjectMoved;	
	notify.obj->ulObjType			= ulObjType;
	
	er = GetCacheManager()->GetEntryIdFromObject(ulObjId, NULL, &notify.obj->pEntryId);
	if(er != erSuccess)
		goto exit;

	er = GetCacheManager()->GetEntryIdFromObject(ulParentId, NULL, &notify.obj->pParentId);
	if(er != erSuccess)
		goto exit;

	er = GetCacheManager()->GetEntryIdFromObject(ulOldParentId, NULL, &notify.obj->pOldParentId);
	if(er != erSuccess)
		goto exit;

	notify.obj->pOldId = lpOldEntryId;

	AddNotification(&notify, ulObjId);

	notify.obj->pOldId = NULL;

exit:
	FreeNotificationStruct(&notify, false);

	return er;
}

ECRESULT ECSessionManager::NotificationCopied(unsigned int ulObjType, unsigned int ulObjId, unsigned int ulParentId, unsigned int ulOldObjId, unsigned int ulOldParentId)
{
	ECRESULT er = erSuccess;
	struct notification notify;

	memset(&notify, 0, sizeof(notification));

	if(ulObjType != MAPI_MESSAGE && ulObjType != MAPI_FOLDER && ulObjType != MAPI_STORE)
		goto exit;

	notify.obj = new notificationObject;
	memset(notify.obj, 0, sizeof(notificationObject));

	notify.ulEventType				= fnevObjectCopied;
	
	notify.obj->ulObjType			= ulObjType;

	er = GetCacheManager()->GetEntryIdFromObject(ulObjId, NULL, &notify.obj->pEntryId);
	if(er != erSuccess)
		goto exit;

	er = GetCacheManager()->GetEntryIdFromObject(ulParentId, NULL, &notify.obj->pParentId);
	if(er != erSuccess)
		goto exit;

	if(ulOldObjId > 0) {
		er = GetCacheManager()->GetEntryIdFromObject(ulOldObjId, NULL, &notify.obj->pOldId);
		if(er != erSuccess)
			goto exit;
	}

	if(ulOldParentId > 0) {
		er = GetCacheManager()->GetEntryIdFromObject(ulOldParentId, NULL, &notify.obj->pOldParentId);
		if(er != erSuccess)
			goto exit;
	}

	AddNotification(&notify, ulObjId);

exit:
	FreeNotificationStruct(&notify, false);

	return er;
}

/** 
 * Send "Search complete" notification to the client.
 * 
 * @param ulObjId object id of the search folder
 * 
 * @return Zarafa error code
 */
ECRESULT ECSessionManager::NotificationSearchComplete(unsigned int ulObjId, unsigned int ulStoreId)
{
	ECRESULT er = erSuccess;
	struct notification notify;

	memset(&notify, 0, sizeof(notification));

	notify.obj = new notificationObject;
	memset(notify.obj, 0, sizeof(notificationObject));

	notify.ulEventType				= fnevSearchComplete;
	notify.obj->ulObjType			= MAPI_FOLDER;

	er = GetCacheManager()->GetEntryIdFromObject(ulObjId, NULL, &notify.obj->pEntryId);
	if(er != erSuccess)
		goto exit;

	AddNotification(&notify, ulObjId, ulStoreId);

exit:
	FreeNotificationStruct(&notify, false);

	return er;
}

ECRESULT ECSessionManager::NotificationChange(const set<unsigned int> &syncIds, unsigned int ulChangeId, unsigned int ulChangeType)
{
	ECRESULT					er = erSuccess;
	SESSIONGROUPMAP::iterator	iIterator;
	
	pthread_rwlock_rdlock(&m_hGroupLock);

	// Send the notification to all sessionsgroups so that any client listening for these
	// notifications can receive them
	for(iIterator = m_mapSessionGroups.begin(); iIterator != m_mapSessionGroups.end(); iIterator++)
		iIterator->second->AddChangeNotification(syncIds, ulChangeId, ulChangeType);
	
	pthread_rwlock_unlock(&m_hGroupLock);

	return er;
}

ECCacheManager*	ECSessionManager::GetCacheManager()
{
	return m_lpECCacheManager;
}

ECSearchFolders* ECSessionManager::GetSearchFolders()
{
	return m_lpSearchFolders;
}

/**
 * Get the sessionmanager statistics
 * 
 * @param[in] callback	Callback to the statistics collector
 * @param[in] obj pointer to the statistics collector
 */
void ECSessionManager::GetStats(void(callback)(const std::string &, const std::string &, const std::string &, void*), void *obj)
{
	sSessionManagerStats sSessionStats;
	sSearchFolderStats sSearchStats;

	GetStats(sSessionStats);

	callback("sessions", "Number of sessions", stringify(sSessionStats.session.ulItems), obj);
	callback("sessions_locked", "Number of locked sessions", stringify(sSessionStats.session.ulLocked), obj);
	callback("sessions_size", "Memory usage of sessions", stringify_int64(sSessionStats.session.ullSize), obj);
	callback("sessiongroups", "Number of session groups", stringify(sSessionStats.group.ulItems), obj);
	callback("sessiongroups_size", "Memory usage of session groups", stringify_int64(sSessionStats.group.ullSize), obj);

	callback("persist_conn", "Persistent connections", stringify(sSessionStats.ulPersistentByConnection), obj);
	callback("persist_conn_size", "Memory usage of persistent connections", stringify(sSessionStats.ulPersistentByConnectionSize), obj);
	callback("persist_sess", "Persistent sessions", stringify(sSessionStats.ulPersistentBySession), obj);
	callback("persist_sess_size", "Memory usage of persistent sessions", stringify(sSessionStats.ulPersistentBySessionSize), obj);

	callback("tables_subscr", "Tables subscribed", stringify(sSessionStats.ulTableSubscriptions), obj);
	callback("tables_subscr_size", "Memory usage of subscribed tables", stringify(sSessionStats.ulTableSubscriptionSize), obj);
	callback("object_subscr", "Objects subscribed", stringify(sSessionStats.ulObjectSubscriptions), obj);
	callback("object_subscr_size", "Memory usage of subscribed objects", stringify(sSessionStats.ulObjectSubscriptionSize), obj);

	callback("tables_open", "Number of open tables", stringify(sSessionStats.session.ulOpenTables), obj);
	callback("tables_open_size", "Memory usage of open tables", stringify_int64(sSessionStats.session.ulTableSize), obj);

	m_lpSearchFolders->GetStats(sSearchStats);

	callback("searchfld_stores", "Number of stores in use by search folders", stringify(sSearchStats.ulStores), obj);
	callback("searchfld_folders", "Number of folders in use by search folders", stringify(sSearchStats.ulFolders), obj);
	callback("searchfld_events", "Number of events waiting for searchfolder updates", stringify(sSearchStats.ulEvents), obj);
	callback("searchfld_size", "Memory usage of search folders", stringify_int64(sSearchStats.ullSize), obj);
}

/**
 * Collect session statistics
 *
 * @param[out] sStats	The statistics
 *
 */
void ECSessionManager::GetStats(sSessionManagerStats &sStats)
{
	unsigned int ulTmpTables, ulTmpTableSize;
	SESSIONMAP::iterator		iIterator;
	SESSIONGROUPMAP::iterator	itersg;
	ECSession *lpSess;
	list<ECSession*> vSessions;

	memset(&sStats, 0, sizeof(sSessionManagerStats));

	// Get session data
	pthread_rwlock_rdlock(&m_hCacheRWLock);

	sStats.session.ulItems = m_mapSessions.size();
	sStats.session.ullSize = sStats.session.ulItems * sizeof(SESSIONMAP::value_type);

	// lock and copy sessions so we can release the main sessionmanager lock before we call the tablemanager to avoid other locks

	for(iIterator = m_mapSessions.begin(); iIterator != m_mapSessions.end(); iIterator++)
	{
		if(iIterator->second->IsLocked() == true)
			sStats.session.ulLocked++;

		lpSess = dynamic_cast<ECSession*>(iIterator->second);
		if (lpSess) {
			lpSess->Lock();
			vSessions.push_back(lpSess);
		}
	}

	pthread_rwlock_unlock(&m_hCacheRWLock);

	for (list<ECSession*>::iterator i = vSessions.begin(); i != vSessions.end(); i++)
	{
		lpSess = *i;
		lpSess->GetTableManager()->GetStats(&ulTmpTables, &ulTmpTableSize);

		sStats.session.ullSize += lpSess->GetSecurity()->GetObjectSize();
		sStats.session.ulOpenTables += ulTmpTables;
		sStats.session.ulTableSize += ulTmpTableSize;

		lpSess->Unlock();
	}

	// Get group data
	pthread_rwlock_rdlock(&m_hGroupLock);
	
	sStats.group.ulItems = m_mapSessionGroups.size();
	sStats.group.ullSize = sStats.group.ulItems * sizeof(SESSIONGROUPMAP::value_type);

	for (itersg = m_mapSessionGroups.begin(); itersg != m_mapSessionGroups.end(); itersg++) {
		sStats.group.ullSize += itersg->second->GetObjectSize();
	}

	pthread_rwlock_unlock(&m_hGroupLock);

	// persistent connections/sessions
	pthread_mutex_lock(&m_mutexPersistent);

	sStats.ulPersistentByConnection = m_mapPersistentByConnection.size();
	sStats.ulPersistentByConnectionSize = sStats.ulPersistentByConnection * sizeof(PERSISTENTBYCONNECTION::value_type);

	sStats.ulPersistentBySession = m_mapPersistentBySession.size();
	sStats.ulPersistentBySessionSize = sStats.ulPersistentBySession * sizeof(PERSISTENTBYSESSION::value_type);

	pthread_mutex_unlock(&m_mutexPersistent);

	// Table subscriptions
	pthread_mutex_lock(&m_mutexTableSubscriptions);
	
	sStats.ulTableSubscriptions = m_mapTableSubscriptions.size();
	sStats.ulTableSubscriptionSize = sStats.ulTableSubscriptions * sizeof(TABLESUBSCRIPTIONMULTIMAP::value_type);

	pthread_mutex_unlock(&m_mutexTableSubscriptions);

	// Object subscriptions
	pthread_mutex_lock(&m_mutexObjectSubscriptions);

	sStats.ulObjectSubscriptions = m_mapObjectSubscriptions.size();
	sStats.ulObjectSubscriptionSize = sStats.ulObjectSubscriptions * sizeof(OBJECTSUBSCRIPTIONSMULTIMAP::value_type);

	pthread_mutex_unlock(&m_mutexObjectSubscriptions);

}

/**
 * Dump statistics
 */
ECRESULT ECSessionManager::DumpStats()
{
	sSessionManagerStats sSessionStats;
	sSearchFolderStats sSearchStats;

	GetStats(sSessionStats);

	m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Session stats:");
	m_lpLogger->Log(EC_LOGLEVEL_FATAL, "  Sessions : %u (%llu bytes)", sSessionStats.session.ulItems, sSessionStats.session.ullSize);
	m_lpLogger->Log(EC_LOGLEVEL_FATAL, "  Locked   : %d", sSessionStats.session.ulLocked);
	m_lpLogger->Log(EC_LOGLEVEL_FATAL, "  Groups   : %u (%llu bytes)" ,sSessionStats.group.ulItems, sSessionStats.group.ullSize);
	m_lpLogger->Log(EC_LOGLEVEL_FATAL, "  PersistentByConnection : %u (%u bytes)" ,sSessionStats.ulPersistentByConnection, sSessionStats.ulPersistentByConnectionSize);
	m_lpLogger->Log(EC_LOGLEVEL_FATAL, "  PersistentBySession    : %u (%u bytes)" , sSessionStats.ulPersistentBySession, sSessionStats.ulPersistentBySessionSize);
	m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Subscription stats:");
	m_lpLogger->Log(EC_LOGLEVEL_FATAL, "  Table : %u (%u bytes)", sSessionStats.ulTableSubscriptions,  sSessionStats.ulTableSubscriptionSize);
	m_lpLogger->Log(EC_LOGLEVEL_FATAL, "  Object: %u (%u bytes)", sSessionStats.ulObjectSubscriptions, sSessionStats.ulObjectSubscriptionSize);

	m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Table stats:");
	m_lpLogger->Log(EC_LOGLEVEL_FATAL, "  Open tables: %u (%llu bytes)", sSessionStats.session.ulOpenTables, sSessionStats.session.ulTableSize);

	m_lpSearchFolders->GetStats(sSearchStats);

	m_lpLogger->Log(EC_LOGLEVEL_FATAL, "SearchFolders:");
	m_lpLogger->Log(EC_LOGLEVEL_FATAL, "  Stores    : %u", sSearchStats.ulStores);
	m_lpLogger->Log(EC_LOGLEVEL_FATAL, "  Folders   : %u", sSearchStats.ulFolders);
	m_lpLogger->Log(EC_LOGLEVEL_FATAL, "  Queue     : %u", sSearchStats.ulEvents);
	m_lpLogger->Log(EC_LOGLEVEL_FATAL, "  Mem usage : %llu Bytes", sSearchStats.ullSize);

	return this->m_lpECCacheManager->DumpStats();
}

ECConfig* ECSessionManager::GetConfig()
{
	return m_lpConfig;
}

ECLogger* ECSessionManager::GetLogger()
{
	return m_lpLogger;
}

ECLogger* ECSessionManager::GetAudit()
{
	return m_lpAudit;
}

ECRESULT ECSessionManager::GetLicensedUsers(unsigned int ulServiceType, unsigned int* lpulLicensedUsers)
{
	ECRESULT er = erSuccess;
	unsigned int ulLicensedUsers = 0;

    ECLicenseClient *lpLicenseClient = NULL;
	lpLicenseClient = new ECLicenseClient(GetConfig()->GetSetting("license_socket"), atoui(GetConfig()->GetSetting("license_timeout")) );
	
	er = lpLicenseClient->GetInfo(ulServiceType, &ulLicensedUsers);
	
	if(er != erSuccess) {
	    ulLicensedUsers = 0;
	    er = erSuccess;
	}
	
	delete lpLicenseClient;


	m_ulLicensedUsers = ulLicensedUsers;
	*lpulLicensedUsers = ulLicensedUsers;

	return er;
}

//@fixme If the license is changed, this data is Invalid!
ECRESULT ECSessionManager::GetLicensedUsersCached(unsigned int* lpulLicensedUsers)
{
	ECRESULT er = erSuccess;
	
	if (m_ulLicensedUsers == (unsigned int)-1)
		er = GetLicensedUsers(0/*SERVICE_TYPE_ZCP*/, lpulLicensedUsers);
	else
		*lpulLicensedUsers = m_ulLicensedUsers;

	return er;
}

bool ECSessionManager::IsHostedSupported()
{
	return m_bHostedZarafa;
}

bool ECSessionManager::IsDistributedSupported()
{
	return m_bDistributedZarafa;
}

ECPluginFactory* ECSessionManager::GetPluginFactory()
{
	return m_lpPluginFactory;
}

ECLockManager* ECSessionManager::GetLockManager()
{
	return m_ptrLockManager.get();
}

ECRESULT ECSessionManager::GetServerGUID(GUID* lpServerGuid){
	ECRESULT		er = erSuccess;
	
	if(lpServerGuid == NULL){
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	memcpy(lpServerGuid, m_lpServerGuid, sizeof(GUID));

exit:
	return er;
}

ECRESULT ECSessionManager::GetNewSourceKey(SOURCEKEY* lpSourceKey){
	ECRESULT		er = erSuccess;
	
	if(lpSourceKey == NULL){
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}
	
	pthread_mutex_lock(&m_hSourceKeyAutoIncrementMutex);
	
	if (m_ulSourceKeyQueue == 0) {
		er = SaveSourceKeyAutoIncrement(m_ullSourceKeyAutoIncrement + 50);
		if(er != erSuccess) {
			pthread_mutex_unlock(&m_hSourceKeyAutoIncrementMutex);
			goto exit;
		}
		m_ulSourceKeyQueue = 50;
	}

	*lpSourceKey = SOURCEKEY(*m_lpServerGuid, m_ullSourceKeyAutoIncrement + 1);
    m_ullSourceKeyAutoIncrement++;
	m_ulSourceKeyQueue--;

	pthread_mutex_unlock(&m_hSourceKeyAutoIncrementMutex);

exit:
	return er;
}

ECRESULT ECSessionManager::SaveSourceKeyAutoIncrement(unsigned long long ullNewSourceKeyAutoIncrement){
	ECRESULT		er = erSuccess;
	std::string		strQuery;
	
	er = CreateDatabaseConnection();
	if(er != erSuccess)
	    goto exit;

	strQuery = "UPDATE `settings` SET `value` = "+ m_lpDatabase->EscapeBinary((unsigned char*)&ullNewSourceKeyAutoIncrement, 8) + " WHERE `name` = 'source_key_auto_increment'";
	er = m_lpDatabase->DoUpdate(strQuery);
	// @TODO if this failed we want to retry this
exit:
	return er;
}

ECRESULT ECSessionManager::SetSessionPersistentConnection(ECSESSIONID sessionID, unsigned int ulPersistentConnectionId)
{
	ECRESULT er = erSuccess;

	pthread_mutex_lock(&m_mutexPersistent);

	// maintain a bi-map of connection <-> session here
	m_mapPersistentByConnection[ulPersistentConnectionId] = sessionID;
	m_mapPersistentBySession[sessionID] = ulPersistentConnectionId;

	pthread_mutex_unlock(&m_mutexPersistent);

	return er;
}

ECRESULT ECSessionManager::RemoveSessionPersistentConnection(unsigned int ulPersistentConnectionId)
{
	ECRESULT er = erSuccess;
	PERSISTENTBYCONNECTION::iterator iterConnection;
	PERSISTENTBYSESSION::iterator iterSession;

	pthread_mutex_lock(&m_mutexPersistent);

	iterConnection = m_mapPersistentByConnection.find(ulPersistentConnectionId);
	if(iterConnection == m_mapPersistentByConnection.end()) {
		er = ZARAFA_E_NOT_FOUND; // shouldn't really happen
		goto exit;
	}

	iterSession = m_mapPersistentBySession.find(iterConnection->second);
	if(iterSession == m_mapPersistentBySession.end()) {
		er = ZARAFA_E_NOT_FOUND; // really really shouldn't happen
		goto exit;
	}

	m_mapPersistentBySession.erase(iterSession);
	m_mapPersistentByConnection.erase(iterConnection);

exit:
	pthread_mutex_unlock(&m_mutexPersistent);

	return er;
}

BOOL ECSessionManager::IsSessionPersistent(ECSESSIONID sessionID)
{
	PERSISTENTBYSESSION::iterator iterSession;

	pthread_mutex_lock(&m_mutexPersistent);
	iterSession = m_mapPersistentBySession.find(sessionID);
	pthread_mutex_unlock(&m_mutexPersistent);

	if(iterSession == m_mapPersistentBySession.end()) {
		return FALSE;
	} else {
		return TRUE;
	}
}

// @todo make this function with a map of seq ids
ECRESULT ECSessionManager::GetNewSequence(SEQUENCE seq, unsigned long long *lpllSeqId)
{
	ECRESULT er = erSuccess;
	std::string strSeqName;

	er = CreateDatabaseConnection();
	if(er != erSuccess)
	    goto exit;

	if(seq == SEQ_IMAP) {
		strSeqName = "imapseq";
	} else {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	pthread_mutex_lock(&m_hSeqMutex);
	if (m_ulSeqIMAPQueue == 0)
	{
		er = m_lpDatabase->DoSequence(strSeqName, 50, &m_ulSeqIMAP);
		if (er != erSuccess) {
			pthread_mutex_unlock(&m_hSeqMutex);
			goto exit;
		}
		m_ulSeqIMAPQueue = 50;
	}
	m_ulSeqIMAPQueue--;
	*lpllSeqId = m_ulSeqIMAP++;

	pthread_mutex_unlock(&m_hSeqMutex);
	
exit:
	return er;
}

ECRESULT ECSessionManager::CreateDatabaseConnection()
{
    ECRESULT er = erSuccess;
	std::string strError;
    
	if(m_lpDatabase == NULL) {
		er = m_lpDatabaseFactory->CreateDatabaseObject(&m_lpDatabase, strError);
		if(er != erSuccess) {
			m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to open connection to database: %s", strError.c_str());
			goto exit;
		}
	}
exit:
    return er;	
}

ECRESULT ECSessionManager::SubscribeTableEvents(TABLE_ENTRY::TABLE_TYPE ulType, unsigned int ulTableRootObjectId, unsigned int ulObjectType, unsigned int ulObjectFlags, ECSESSIONID sessionID)
{
    ECRESULT er = erSuccess;
    TABLESUBSCRIPTION sSubscription;
    
    pthread_mutex_lock(&m_mutexTableSubscriptions);
    
    sSubscription.ulType = ulType;
    sSubscription.ulRootObjectId = ulTableRootObjectId;
    sSubscription.ulObjectType = ulObjectType;
    sSubscription.ulObjectFlags = ulObjectFlags;
    
    m_mapTableSubscriptions.insert(std::pair<TABLESUBSCRIPTION, ECSESSIONID>(sSubscription, sessionID));
    
    pthread_mutex_unlock(&m_mutexTableSubscriptions);
    
    return er;
}

ECRESULT ECSessionManager::UnsubscribeTableEvents(TABLE_ENTRY::TABLE_TYPE ulType, unsigned int ulTableRootObjectId, unsigned int ulObjectType, unsigned int ulObjectFlags, ECSESSIONID sessionID)
{
    ECRESULT er = erSuccess;
    TABLESUBSCRIPTION sSubscription;
    std::multimap<TABLESUBSCRIPTION, ECSESSIONID>::iterator iter;
    
    pthread_mutex_lock(&m_mutexTableSubscriptions);
    
    sSubscription.ulType = ulType;
    sSubscription.ulRootObjectId = ulTableRootObjectId;
    sSubscription.ulObjectType = ulObjectType;
    sSubscription.ulObjectFlags = ulObjectFlags;
    
    iter = m_mapTableSubscriptions.find(sSubscription);
    while(iter != m_mapTableSubscriptions.end() && iter->first == sSubscription) {
        if(iter->second == sessionID)
            break;
        iter++;
    }
    
    if(iter != m_mapTableSubscriptions.end()) {
        m_mapTableSubscriptions.erase(iter);
    } else {
        er = ZARAFA_E_NOT_FOUND;
    }
    
    pthread_mutex_unlock(&m_mutexTableSubscriptions);
    
    return er;
}

// Subscribes for all object notifications in store ulStoreID for session group sessionID
ECRESULT ECSessionManager::SubscribeObjectEvents(unsigned int ulStoreId, ECSESSIONGROUPID sessionID)
{
    ECRESULT er = erSuccess;
    
    pthread_mutex_lock(&m_mutexObjectSubscriptions);
    m_mapObjectSubscriptions.insert(std::pair<unsigned int, ECSESSIONGROUPID>(ulStoreId, sessionID));
    pthread_mutex_unlock(&m_mutexObjectSubscriptions);
    
    return er;
}

ECRESULT ECSessionManager::UnsubscribeObjectEvents(unsigned int ulStoreId, ECSESSIONGROUPID sessionID)
{
    ECRESULT er = erSuccess;
    std::multimap<unsigned int, ECSESSIONGROUPID>::iterator i;
    
    pthread_mutex_lock(&m_mutexObjectSubscriptions);
    i = m_mapObjectSubscriptions.find(ulStoreId);
    
    while(i != m_mapObjectSubscriptions.end() && i->first == ulStoreId && i->second != sessionID) i++;
    
    if(i != m_mapObjectSubscriptions.end()) {
        m_mapObjectSubscriptions.erase(i);
    }

    pthread_mutex_unlock(&m_mutexObjectSubscriptions);
    
    return er;
}

ECRESULT ECSessionManager::DeferNotificationProcessing(ECSESSIONID ecSessionId, struct soap *soap)
{
    // Let the notification  manager handle this request. We don't do anything more with the notification
    // request since the notification manager will handle it all
    
    return m_lpNotificationManager->AddRequest(ecSessionId, soap);
}

// Called when a notification is ready for a session group
ECRESULT ECSessionManager::NotifyNotificationReady(ECSESSIONID ecSessionId)
{
    return m_lpNotificationManager->NotifyChange(ecSessionId);
}

ECRESULT ECSessionManager::GetStoreSortLCID(ULONG ulStoreId, ULONG *lpLcid)
{
	ECRESULT		er = erSuccess;
	ECDatabase		*lpDatabase = NULL;
	DB_RESULT		lpDBResult = NULL;
	DB_ROW			lpDBRow = NULL;
	std::string		strQuery;

	if (lpLcid == NULL) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	er = GetThreadLocalDatabase(m_lpDatabaseFactory, &lpDatabase);
	if(er != erSuccess)
		goto exit;

	strQuery = "SELECT val_ulong FROM properties WHERE hierarchyid=" + stringify(ulStoreId) +
				" AND tag=" + stringify(PROP_ID(PR_SORT_LOCALE_ID)) + " AND type=" + stringify(PROP_TYPE(PR_SORT_LOCALE_ID));
	er = lpDatabase->DoSelect(strQuery, &lpDBResult);
	if(er != erSuccess)
		goto exit;

	lpDBRow = lpDatabase->FetchRow(lpDBResult);
	if (lpDBRow == NULL || lpDBRow[0] == NULL) {
		er = ZARAFA_E_NOT_FOUND;
		goto exit;
	}

	*lpLcid = strtoul(lpDBRow[0], NULL, 10);

exit:
	if (lpDBResult)
		lpDatabase->FreeResult(lpDBResult);

	return er;
}

LPCSTR ECSessionManager::GetDefaultSortLocaleID()
{
	return GetConfig()->GetSetting("default_sort_locale_id");
}

ULONG ECSessionManager::GetSortLCID(ULONG ulStoreId)
{
	ECRESULT er = erSuccess;
	ULONG ulLcid = 0;
	LPCSTR lpszLocaleId = NULL;

	er = GetStoreSortLCID(ulStoreId, &ulLcid);
	if (er == erSuccess)
		goto exit;

	lpszLocaleId = GetDefaultSortLocaleID();
	if (lpszLocaleId == NULL || *lpszLocaleId == '\0') {
		ulLcid = 0;	// Select default LCID
		goto exit;
	}

	er = LocaleIdToLCID(lpszLocaleId, &ulLcid);
	if (er != erSuccess) {
		ulLcid = 0;	// Select default LCID
		goto exit;
	}

exit:
	return ulLcid;
}

ECLocale ECSessionManager::GetSortLocale(ULONG ulStoreId)
{
	ECRESULT		er = erSuccess;
	ULONG			ulLcid = 0;
	LPCSTR			lpszLocaleId = NULL;

	er = GetStoreSortLCID(ulStoreId, &ulLcid);
	if (er == erSuccess) {
		er = LCIDToLocaleId(ulLcid, &lpszLocaleId);
	}
	if (er == erSuccess)
		goto exit;

	lpszLocaleId = GetDefaultSortLocaleID();
	if (lpszLocaleId == NULL || *lpszLocaleId == '\0') {
		lpszLocaleId = "";	// Select default localeid
		goto exit;
	}

exit:
	return createLocaleFromName(lpszLocaleId);
}
