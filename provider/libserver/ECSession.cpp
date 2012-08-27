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

//
//////////////////////////////////////////////////////////////////////

#include "platform.h"

#ifdef DEBUG
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <dirent.h>

#include <mapidefs.h>
#include <mapitags.h>

#include "ECSession.h"
#include "ECSessionManager.h"
#include "ECUserManagement.h"
#include "ECUserManagementOffline.h"
#include "ECSecurity.h"
#include "ECSecurityOffline.h"
#include "ECPluginFactory.h"
#include "base64.h"
#include "SSLUtil.h"
#include "stringutil.h"

#include "ECDatabaseMySQL.h"
#include "ECDatabaseUtils.h" // used for PR_INSTANCE_KEY
#include "SOAPUtils.h"
#include "ZarafaICS.h"
#include "ECICS.h"
#include "ECIConv.h"
#include "ZarafaVersions.h"

#include "pthreadutil.h"
#include "threadutil.h"
#include "boost_compat.h"

#include <boost/filesystem.hpp>
namespace bfs = boost::filesystem;

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define WHITESPACE " \t\n\r"

// hackish import of relocate_fd
int relocate_fd(int fd, ECLogger *lpLogger);

// possible missing ssl function
#ifndef HAVE_EVP_PKEY_CMP
int EVP_PKEY_cmp(EVP_PKEY *a, EVP_PKEY *b)
    {
    if (a->type != b->type)
        return -1;

    if (EVP_PKEY_cmp_parameters(a, b) == 0)
        return 0;

    switch (a->type)
        {
    case EVP_PKEY_RSA:
        if (BN_cmp(b->pkey.rsa->n,a->pkey.rsa->n) != 0
            || BN_cmp(b->pkey.rsa->e,a->pkey.rsa->e) != 0)
            return 0;
        break;
    case EVP_PKEY_DSA:
        if (BN_cmp(b->pkey.dsa->pub_key,a->pkey.dsa->pub_key) != 0)
            return 0;
        break;
    case EVP_PKEY_DH:
        return -2;
    default:
        return -2;
        }

    return 1;
    }
#endif

void CreateSessionID(unsigned int ulCapabilities, ECSESSIONID *lpSessionId)
{
	ssl_random(!!(ulCapabilities & ZARAFA_CAP_LARGE_SESSIONID), lpSessionId);
}

/*
  BaseType session
*/
BTSession::BTSession(const std::string& strSourceAddr, ECSESSIONID sessionID, ECDatabaseFactory *lpDatabaseFactory, ECSessionManager *lpSessionManager, unsigned int ulCapabilities) :
	m_strSourceAddr(strSourceAddr), m_sessionID(sessionID), m_lpDatabaseFactory(lpDatabaseFactory), m_lpSessionManager(lpSessionManager), m_ulClientCapabilities(ulCapabilities)
{
	m_ulRefCount = 0;
	m_sessionTime = GetProcessTime();

	m_ulSessionTimeout = 300;
	m_bCheckIP = true;

	m_lpUserManagement = NULL;
	m_ulRequests = 0;

	// Protects the object from deleting while a thread is running on a method in this object
	pthread_cond_init(&m_hThreadReleased, NULL);
	pthread_mutex_init(&m_hThreadReleasedMutex, NULL);
}

BTSession::~BTSession() {
	// derived destructor still uses these vars
	pthread_cond_destroy(&m_hThreadReleased);
	pthread_mutex_destroy(&m_hThreadReleasedMutex);
}

ECRESULT BTSession::Shutdown(unsigned int ulTimeout) {
	return erSuccess;
}

ECRESULT BTSession::ValidateOriginator(struct soap *soap)
{
	std::string strSourceAddr = ::GetSourceAddr(soap);
	
	if (!m_bCheckIP || m_strSourceAddr == strSourceAddr)
		return erSuccess;
		
	m_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_FATAL, "Denying access to session from source '%s' due to unmatched establishing source '%s'", strSourceAddr.c_str(), m_strSourceAddr.c_str());
	
	return ZARAFA_E_END_OF_SESSION;
}

ECSESSIONID BTSession::GetSessionId()
{
	return m_sessionID;
}

time_t BTSession::GetSessionTime()
{
	return m_sessionTime + m_ulSessionTimeout;
}

void BTSession::UpdateSessionTime()
{
	m_sessionTime = GetProcessTime();
}

unsigned int BTSession::GetCapabilities()
{
	return m_ulClientCapabilities;
}

ECSessionManager* BTSession::GetSessionManager()
{
	return m_lpSessionManager;
}

ECUserManagement* BTSession::GetUserManagement()
{
	return m_lpUserManagement;
}

ECRESULT BTSession::GetDatabase(ECDatabase **lppDatabase)
{
	return GetThreadLocalDatabase(this->m_lpDatabaseFactory, lppDatabase);
}

ECRESULT BTSession::GetAdditionalDatabase(ECDatabase **lppDatabase)
{
	std::string str;
	return this->m_lpDatabaseFactory->CreateDatabaseObject(lppDatabase, str);
}


ECRESULT BTSession::GetServerGUID(GUID* lpServerGuid){
	return 	m_lpSessionManager->GetServerGUID(lpServerGuid);
}

ECRESULT BTSession::GetNewSourceKey(SOURCEKEY* lpSourceKey){
	return m_lpSessionManager->GetNewSourceKey(lpSourceKey);
}

void BTSession::Lock()
{
	// Increase our refcount by one
	pthread_mutex_lock(&m_hThreadReleasedMutex);
	this->m_ulRefCount++;
	pthread_mutex_unlock(&m_hThreadReleasedMutex);
}

void BTSession::Unlock()
{
	// Decrease our refcount by one, signal ThreadReleased if RefCount == 0
	pthread_mutex_lock(&m_hThreadReleasedMutex);
	this->m_ulRefCount--;
	if(!IsLocked())
		pthread_cond_signal(&m_hThreadReleased);
	pthread_mutex_unlock(&m_hThreadReleasedMutex);
}

bool BTSession::IsLocked() {
	return (m_ulRefCount > 0);
}

time_t BTSession::GetIdleTime()
{
	return GetProcessTime() - m_sessionTime;
}

std::string BTSession::GetSourceAddr()
{
	return m_strSourceAddr;
}

void BTSession::RecordRequest(struct soap *soap)
{
	m_strLastRequestURL = soap->endpoint;
	m_ulLastRequestPort = soap->port;
	if (soap->proxy_from && ((SOAPINFO *)soap->user)->bProxy)
		m_strProxyHost = PrettyIP(soap->ip);
    m_ulRequests++;
}

unsigned int BTSession::GetRequests() 
{
    return m_ulRequests;
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

ECSession::ECSession(const std::string& strSourceAddr, ECSESSIONID sessionID, ECSESSIONGROUPID ecSessionGroupId, ECDatabaseFactory *lpDatabaseFactory, ECSessionManager *lpSessionManager, unsigned int ulCapabilities, bool bIsOffline, AUTHMETHOD ulAuthMethod, int pid, std::string strClientVersion, std::string strClientApp) :
	BTSession(strSourceAddr, sessionID, lpDatabaseFactory, lpSessionManager, ulCapabilities)
{
	m_lpTableManager		= new ECTableManager(this);
	m_lpEcSecurity			= NULL;
	m_dblUser				= 0;
	m_dblSystem				= 0;
	m_dblReal				= 0;
	m_ulAuthMethod			= ulAuthMethod;
	m_ulConnectingPid		= pid;
	m_ecSessionGroupId		= ecSessionGroupId;
	m_strClientVersion		= strClientVersion;
	m_ulClientVersion		= ZARAFA_VERSION_UNKNOWN;
	m_strClientApp			= strClientApp;


	ParseZarafaVersion(strClientVersion, &m_ulClientVersion);
	// Ignore result.

	m_ulSessionTimeout = atoi(lpSessionManager->GetConfig()->GetSetting("session_timeout"));
	if (m_ulSessionTimeout < 300)
		m_ulSessionTimeout = 300;

	m_bCheckIP = strcmp(lpSessionManager->GetConfig()->GetSetting("session_ip_check"), "no") != 0;

	// Offline implements it's own versions of these objects
	if (bIsOffline == false) {
		m_lpUserManagement = new ECUserManagement(this, m_lpSessionManager->GetPluginFactory(), m_lpSessionManager->GetConfig(), m_lpSessionManager->GetLogger());
		m_lpEcSecurity = new ECSecurity(this, m_lpSessionManager->GetConfig(), m_lpSessionManager->GetLogger(), m_lpSessionManager->GetAudit());
	} else {
		m_lpUserManagement = new ECUserManagementOffline(this, m_lpSessionManager->GetPluginFactory(), m_lpSessionManager->GetConfig(), m_lpSessionManager->GetLogger());

		m_lpEcSecurity = new ECSecurityOffline(this, m_lpSessionManager->GetConfig(), m_lpSessionManager->GetLogger());
	}

	// Atomically get and AddSession() on the sessiongroup. Needs a ReleaseSession() on the session group to clean up.
	m_lpSessionManager->GetSessionGroup(ecSessionGroupId, this, &m_lpSessionGroup);

	pthread_mutex_init(&m_hStateLock, NULL);
	pthread_mutex_init(&m_hLocksLock, NULL);
}


ECSession::~ECSession()
{
	Shutdown(0);

	/*
	 * Release our reference to the session group; none of the threads of this session are
	 * using the object since there are now 0 threads on this session (except this thread)
	 * Afterwards tell the session manager that the sessiongroup may be an orphan now.
	 */
	if (m_lpSessionGroup) {
		m_lpSessionGroup->ReleaseSession(this);
    	m_lpSessionManager->DeleteIfOrphaned(m_lpSessionGroup);
	}

	pthread_mutex_destroy(&m_hLocksLock);
	pthread_mutex_destroy(&m_hStateLock);

	if(m_lpTableManager)
		delete m_lpTableManager;

	if(m_lpUserManagement)
		delete m_lpUserManagement;

	if(m_lpEcSecurity)
		delete m_lpEcSecurity;
}

/**
 * Shut down the session:
 *
 * - Signal sessiongroup that long-running requests should be cancelled
 * - Wait for all users of the session to exit
 *
 * If the wait takes longer than ulTimeout milliseconds, ZARAFA_E_TIMEOUT is
 * returned. If this is the case, it is *not* safe to delete the session
 *
 * @param ulTimeout Timeout in milliseconds
 * @result erSuccess or ZARAFA_E_TIMEOUT
 */
ECRESULT ECSession::Shutdown(unsigned int ulTimeout)
{
	ECRESULT er = erSuccess;
	
	/* Shutdown blocking calls for this session on our session group */
	if (m_lpSessionGroup) {
		m_lpSessionGroup->ShutdownSession(this);
	}

	/* Wait until there are no more running threads using this session */
	pthread_mutex_lock(&m_hThreadReleasedMutex); 
	while(IsLocked())
		if(pthread_cond_timedwait(&m_hThreadReleased, &m_hThreadReleasedMutex, ulTimeout) == ETIMEDOUT)
			break;
	pthread_mutex_unlock(&m_hThreadReleasedMutex);
	
	if(IsLocked()) {
		er = ZARAFA_E_TIMEOUT;
	}
	
	return er;
}

ECSession::AUTHMETHOD ECSession::GetAuthMethod()
{
    return m_ulAuthMethod;
}

ECSESSIONGROUPID ECSession::GetSessionGroupId() {
    return m_ecSessionGroupId;
}

int ECSession::GetConnectingPid() {
    return m_ulConnectingPid;
}

ECRESULT ECSession::AddAdvise(unsigned int ulConnection, unsigned int ulKey, unsigned int ulEventMask)
{
	ECRESULT		hr = erSuccess;

	Lock();

	if (m_lpSessionGroup)
		hr = m_lpSessionGroup->AddAdvise(m_sessionID, ulConnection, ulKey, ulEventMask);
	else
		hr = ZARAFA_E_NOT_INITIALIZED;

	Unlock();

	return hr;
}

ECRESULT ECSession::AddChangeAdvise(unsigned int ulConnection, notifySyncState *lpSyncState)
{
	ECRESULT		er = erSuccess;
	string			strQuery;
	ECDatabase*		lpDatabase = NULL;
	DB_RESULT		lpDBResult	= NULL;
	DB_ROW			lpDBRow;
	ULONG			ulChangeId = 0;

	Lock();

	if (!m_lpSessionGroup) {
		er = ZARAFA_E_NOT_INITIALIZED;
		goto exit;
	}

	er = m_lpSessionGroup->AddChangeAdvise(m_sessionID, ulConnection, lpSyncState);
	if (er != hrSuccess)
		goto exit;

    er = GetDatabase(&lpDatabase);
    if (er != erSuccess)
        goto exit;

	strQuery =	"SELECT c.id FROM changes AS c JOIN syncs AS s "
					"ON s.sourcekey=c.parentsourcekey "
				"WHERE s.id=" + stringify(lpSyncState->ulSyncId) + " "
					"AND c.id>" + stringify(lpSyncState->ulChangeId) + " "
					"AND c.sourcesync!=" + stringify(lpSyncState->ulSyncId) + " "
					"AND c.change_type >=  " + stringify(ICS_MESSAGE) + " "
					"AND c.change_type & " + stringify(ICS_MESSAGE) + " !=  0 "
				"ORDER BY c.id DESC "
				"LIMIT 1";

	er = lpDatabase->DoSelect(strQuery, &lpDBResult);
	if (er != hrSuccess)
		goto exit;

	if (lpDatabase->GetNumRows(lpDBResult) == 0)
		goto exit;

    lpDBRow = lpDatabase->FetchRow(lpDBResult);
	if (lpDBRow == NULL || lpDBRow[0] == NULL) {
		er = ZARAFA_E_DATABASE_ERROR;
		goto exit;
	}

	ulChangeId = strtoul(lpDBRow[0], NULL, 0);
	er = m_lpSessionGroup->AddChangeNotification(m_sessionID, ulConnection, lpSyncState->ulSyncId, ulChangeId);

exit:
	 if (lpDBResult)
		 lpDatabase->FreeResult(lpDBResult);

	Unlock();

	return er;
}

ECRESULT ECSession::DelAdvise(unsigned int ulConnection)
{
	ECRESULT hr = erSuccess;

	Lock();

	if (m_lpSessionGroup)
		hr = m_lpSessionGroup->DelAdvise(m_sessionID, ulConnection);
	else
		hr = ZARAFA_E_NOT_INITIALIZED;
	
	Unlock();

	return hr;
}

ECRESULT ECSession::AddNotificationTable(unsigned int ulType, unsigned int ulObjType, unsigned int ulTableId, sObjectTableKey* lpsChildRow, sObjectTableKey* lpsPrevRow, struct propValArray *lpRow)
{
	ECRESULT		hr = hrSuccess;

	Lock();

	if (m_lpSessionGroup)
		hr = m_lpSessionGroup->AddNotificationTable(m_sessionID, ulType, ulObjType, ulTableId, lpsChildRow, lpsPrevRow, lpRow);
	else
		hr = ZARAFA_E_NOT_INITIALIZED;

	Unlock();

	return hr;
}

ECRESULT ECSession::GetNotifyItems(struct soap *soap, struct notifyResponse *notifications)
{
	ECRESULT		hr = erSuccess;

	Lock();

	if (m_lpSessionGroup)
		hr = m_lpSessionGroup->GetNotifyItems(soap, m_sessionID, notifications);
	else
		hr = ZARAFA_E_NOT_INITIALIZED;

	Unlock();
	
	return hr;
}

ECTableManager* ECSession::GetTableManager()
{
	return m_lpTableManager;
}

ECSecurity* ECSession::GetSecurity()
{
	return m_lpEcSecurity;
}

void ECSession::AddBusyState(pthread_t threadId, const char *lpszState)
{
	pthread_mutex_lock(&m_hStateLock);
	m_mapBusyStates[threadId] = lpszState;
	pthread_mutex_unlock(&m_hStateLock);
}

void ECSession::RemoveBusyState(pthread_t threadId)
{
	pthread_mutex_lock(&m_hStateLock);
	m_mapBusyStates.erase(threadId);
	pthread_mutex_unlock(&m_hStateLock);
}

void ECSession::GetBusyStates(list<string> *lpLstStates)
{
	map<pthread_t, const char*>::iterator iMap;

	// this map is very small, since a session only performs one or two functions at a time
	// so the lock time is short, which will block _all_ incoming functions
	pthread_mutex_lock(&m_hStateLock);
	for (iMap = m_mapBusyStates.begin(); iMap != m_mapBusyStates.end(); iMap++)
		lpLstStates->push_back(iMap->second);
	pthread_mutex_unlock(&m_hStateLock);
}

void ECSession::AddClocks(double dblUser, double dblSystem, double dblReal)
{
	m_dblUser += dblUser;
	m_dblSystem += dblSystem;
	m_dblReal += dblReal;
}

void ECSession::GetClocks(double *lpdblUser, double *lpdblSystem, double *lpdblReal)
{
	*lpdblUser = m_dblUser;
	*lpdblSystem = m_dblSystem;
	*lpdblReal = m_dblReal;
}

void ECSession::GetClientVersion(std::string *lpstrVersion)
{
    lpstrVersion->assign(m_strClientVersion);
}

void ECSession::GetClientApp(std::string *lpstrClientApp)
{
    lpstrClientApp->assign(m_strClientApp);
}

void ECSession::GetRequestURL(std::string *lpstrClientURL)
{
	lpstrClientURL->assign(m_strLastRequestURL);
}

void ECSession::GetProxyHost(std::string *lpstrProxyHost)
{
	lpstrProxyHost->assign(m_strProxyHost);
}

void ECSession::GetClientPort(unsigned int *lpulPort)
{
	*lpulPort = m_ulLastRequestPort;
}

/**
 * Get the object id of the object specified by the provided entryid.
 * This entryid can either be a short term or 'normal' entryid. If the entryid is a
 * short term entryid, the STE manager for this session will be queried for the object id.
 * If the entryid is a 'normal' entryid, the cache manager / database will be queried.
 * 
 * @param[in]	lpEntryID		The entryid to get an object id for.
 * @param[out]	lpulObjId		Pointer to an unsigned int that will be set to the returned object id.
 * @param[out]	lpbIsShortTerm	Optional pointer to a boolean that will be set to true when the entryid
 * 								is a short term entryid.
 * 
 * @retval	ZARAFA_E_INVALID_PARAMETER	lpEntryId or lpulObjId is NULL.
 * @retval	ZARAFA_E_INVALID_ENTRYID	The provided entryid is invalid.
 * @retval	ZARAFA_E_NOT_FOUND			No object was found for the provided entryid.
 */
ECRESULT ECSession::GetObjectFromEntryId(const entryId *lpEntryId, unsigned int *lpulObjId, unsigned int *lpulEidFlags)
{
	ECRESULT er = erSuccess;
	unsigned int ulObjId = 0;
	
	if (lpEntryId == NULL || lpulObjId == NULL) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}
	
	er = m_lpSessionManager->GetCacheManager()->GetObjectFromEntryId((entryId*)lpEntryId, &ulObjId);
	if (er != erSuccess)
		goto exit;
	
	*lpulObjId = ulObjId;
	
	if(lpulEidFlags)
		*lpulEidFlags = ((EID *)lpEntryId->__ptr)->usFlags;
		
exit:
	return er;
}

ECRESULT ECSession::LockObject(unsigned int ulObjId) 
{
	ECRESULT er = erSuccess;
	std::pair<LockMap::iterator, bool> res;
	scoped_lock lock(m_hLocksLock);

	res = m_mapLocks.insert(LockMap::value_type(ulObjId, ECObjectLock()));
	if (res.second == true)
		er = m_lpSessionManager->GetLockManager()->LockObject(ulObjId, m_sessionID, &res.first->second);

	return er;	
}

ECRESULT ECSession::UnlockObject(unsigned int ulObjId)
{
	ECRESULT er = erSuccess;
	LockMap::iterator i;
	scoped_lock lock(m_hLocksLock);

	i = m_mapLocks.find(ulObjId);
	if (i == m_mapLocks.end())
		goto exit;

	er = i->second.Unlock();
	if (er == erSuccess)
		m_mapLocks.erase(i);

exit:
	return er;
}


/*
  ECAuthSession
*/
ECAuthSession::ECAuthSession(const std::string& strSourceAddr, ECSESSIONID sessionID, ECDatabaseFactory *lpDatabaseFactory, ECSessionManager *lpSessionManager, unsigned int ulCapabilities) :
		BTSession(strSourceAddr, sessionID, lpDatabaseFactory, lpSessionManager, ulCapabilities)
{
	m_ulUserID = 0;
	m_bValidated = false;
	m_ulSessionTimeout = 30;	// authenticate within 30 seconds, or else!

	m_lpUserManagement = new ECUserManagement(this, m_lpSessionManager->GetPluginFactory(), m_lpSessionManager->GetConfig(), m_lpSessionManager->GetLogger());

	m_ulConnectingPid = 0;

	m_NTLM_pid = -1;
#ifdef HAVE_GSSAPI
	m_gssServerCreds = GSS_C_NO_CREDENTIAL;
	m_gssContext = GSS_C_NO_CONTEXT;
#endif
}

ECAuthSession::~ECAuthSession()
{
#ifdef HAVE_GSSAPI
	OM_uint32 status;

	if (m_gssServerCreds)
		gss_release_cred(&status, &m_gssServerCreds);

	if (m_gssContext)
		gss_delete_sec_context(&status, &m_gssContext, GSS_C_NO_BUFFER);
#endif

	/* Wait until all locks have been closed */
	pthread_mutex_lock(&m_hThreadReleasedMutex); 
	while (IsLocked())
		pthread_cond_wait(&m_hThreadReleased, &m_hThreadReleasedMutex);
	pthread_mutex_unlock(&m_hThreadReleasedMutex);

	if (m_NTLM_pid != -1) {
		int status;

		// close I/O to make ntlm_auth exit
		close(m_stdin);
		close(m_stdout);
		close(m_stderr);

		// wait for process status
		waitpid(m_NTLM_pid, &status, 0);
		m_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_INFO, "Removing ntlm_auth on pid %d. Exitstatus: %d", m_NTLM_pid, status);
		if (status == -1) {
			m_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_ERROR, string("System call `waitpid' failed: ") + strerror(errno));
		} else {
#ifdef WEXITSTATUS
				if(WIFEXITED(status)) { /* Child exited by itself */
					if(WEXITSTATUS(status))
						m_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_ERROR, "ntlm_auth exited with non-zero status %d", WEXITSTATUS(status));
				} else if(WIFSIGNALED(status)) {        /* Child was killed by a signal */
					m_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_ERROR, "ntlm_auth was killed by signal %d", WTERMSIG(status));

				} else {                        /* Something strange happened */
					m_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_ERROR, "ntlm_auth terminated abnormally");
				}
#else
				if (status)
					m_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_ERROR, "ntlm_auth exited with status %d", status);
#endif
		}
	}

	delete m_lpUserManagement;
}


ECRESULT ECAuthSession::CreateECSession(ECSESSIONGROUPID ecSessionGroupId, std::string strClientVersion, std::string strClientApp, ECSESSIONID *sessionID, ECSession **lppNewSession) {
	ECRESULT er = erSuccess;
	ECSession *lpSession = NULL;
	ECSESSIONID newSID;

	if (!m_bValidated) {
		er = ZARAFA_E_LOGON_FAILED;
		goto exit;
	}

	CreateSessionID(m_ulClientCapabilities, &newSID);

	// ECAuthSessionOffline creates offline version .. no bOverrideClass construction
	lpSession = new ECSession(m_strSourceAddr, newSID, ecSessionGroupId, m_lpDatabaseFactory, m_lpSessionManager, m_ulClientCapabilities, false, m_ulValidationMethod, m_ulConnectingPid, strClientVersion, strClientApp);
	if (!lpSession) {
		er = ZARAFA_E_NOT_ENOUGH_MEMORY;
		goto exit;
	}

	er = lpSession->GetSecurity()->SetUserContext(m_ulUserID);
	if (er != erSuccess)
		goto exit;				// user not found anymore, or error in getting groups

	*sessionID = newSID;
	*lppNewSession = lpSession;

exit:
	if (er != erSuccess && lpSession)
		delete lpSession;

	return er;
}

// This is a standard user/pass login.
// You always log in as the user you are authenticating with.
ECRESULT ECAuthSession::ValidateUserLogon(char *lpszName, char *lpszPassword)
{
	ECRESULT er = erSuccess;
	
	// SYSTEM can't login with user/pass
	if(stricmp(lpszName, ZARAFA_ACCOUNT_SYSTEM) == 0) {
		er = ZARAFA_E_NO_ACCESS;
		goto exit;
	}

	er = m_lpUserManagement->AuthUserAndSync(lpszName, lpszPassword, &m_ulUserID);
	if(er != erSuccess)
		goto exit;

	m_bValidated = true;
	m_ulValidationMethod = METHOD_USERPASSWORD;

exit:
	return er;
}

// Validate a user through the socket they are connecting through. This has the special feature
// that you can connect as a different user than you are specifying in the username. For example,
// you could be connecting as 'root' and being granted access because the zarafa-server process
// is also running as 'root', but you are actually loggin in as user 'user1'.
ECRESULT ECAuthSession::ValidateUserSocket(int socket, char *lpszName)
{
	ECRESULT 		er = erSuccess;
	char			*p = NULL;
	bool			allowLocalUsers = false;
	int				pid = 0;
	char			*ptr = NULL;
	char			*localAdminUsers = NULL;

	p = m_lpSessionManager->GetConfig()->GetSetting("allow_local_users");
	if (p && !stricmp(p, "yes")) {
		allowLocalUsers = true;
	}

	// Authentication stage
	localAdminUsers = strdup(m_lpSessionManager->GetConfig()->GetSetting("local_admin_users"));

	struct passwd pwbuf;
	struct passwd *pw;
	uid_t uid;
	char strbuf[1024];	
#ifdef SO_PEERCRED
	struct ucred cr;
	unsigned int cr_len;

	cr_len = sizeof(struct ucred);
	if(getsockopt(socket, SOL_SOCKET, SO_PEERCRED, &cr, &cr_len) != 0 || cr_len != sizeof(struct ucred)) {
		er = ZARAFA_E_LOGON_FAILED;
		goto exit;
	}

	uid = cr.uid; // uid is the uid of the user that is connecting
	pid = cr.pid;
#else // SO_PEERCRED
#ifdef HAVE_GETPEEREID
	gid_t gid;

	if (getpeereid(socket, &uid, &gid)) {
		er = ZARAFA_E_LOGON_FAILED;
		goto exit;
	}
#else // HAVE_GETPEEREID
#error I have no way to find out the remote user and I want to cry
#endif // HAVE_GETPEEREID
#endif // SO_PEERCRED

	if (geteuid() == uid) {
		// User connecting is connecting under same UID as the server is running under, allow this
		goto userok;
	}

	// Lookup user name
	pw = NULL;
#ifdef HAVE_GETPWNAM_R
	getpwnam_r(lpszName, &pwbuf, strbuf, sizeof(strbuf), &pw);
#else
	// OpenBSD does not have getpwnam_r() .. FIXME: threading issue!
	pw = getpwnam(lpszName);
#endif

	if (allowLocalUsers && pw && pw->pw_uid == uid)
		// User connected as himself
		goto userok;

	p = strtok_r(localAdminUsers, WHITESPACE, &ptr);
	
	while (p) {
	    pw = NULL;
#ifdef HAVE_GETPWNAM_R
		getpwnam_r(p, &pwbuf, strbuf, sizeof(strbuf), &pw);
#else
		pw = getpwnam(p);
#endif
		
		if (pw) {
			if (pw->pw_uid == uid) {
				// A local admin user connected - ok
				goto userok;
			}
		}
		p = strtok_r(NULL, WHITESPACE, &ptr);
	}

	
	er = ZARAFA_E_LOGON_FAILED;
	goto exit;

userok:
    // Check whether user exists in the user database
	er = m_lpUserManagement->ResolveObjectAndSync(OBJECTCLASS_USER, lpszName, &m_ulUserID);
	if (er != erSuccess)
	    goto exit;

	m_bValidated = true;
	m_ulValidationMethod = METHOD_SOCKET;
	m_ulConnectingPid = pid;

exit:
	if(localAdminUsers)
		free(localAdminUsers);

	return er;
}

ECRESULT ECAuthSession::ValidateUserCertificate(struct soap *soap, char *lpszName)
{
	ECRESULT		er = ZARAFA_E_LOGON_FAILED;
	X509			*cert = NULL;			// client certificate
	EVP_PKEY		*pubkey = NULL;			// client public key
	EVP_PKEY		*storedkey = NULL;
	int				res = -1;

	char			*sslkeys_path = m_lpSessionManager->GetConfig()->GetSetting("sslkeys_path","",NULL);
	BIO 			*biofile = NULL;

	bfs::path		keysdir;
	bfs::directory_iterator key_last;


	if (!sslkeys_path || sslkeys_path[0] == '\0') {
		m_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_WARNING, "No public keys directory defined in sslkeys_path.");
		goto exit;
	}

	cert = SSL_get_peer_certificate(soap->ssl);
	if (!cert) {
		// windows client without ssl certificate
		m_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_INFO, "No certificate in SSL connection.");
		goto exit;
	}
	pubkey = X509_get_pubkey(cert);	// need to free
	if (!pubkey) {
		// if you get here, please tell me how, 'cause I'd like to know :)
		m_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_INFO, "No public key in certificate.");
		goto exit;
	}

	try {
		keysdir = sslkeys_path;
		if (!bfs::exists(keysdir)) {
			m_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_INFO, "Certificate path '%s' is not present.", sslkeys_path);
			er = ZARAFA_E_LOGON_FAILED;
			goto exit;
		}

		for (bfs::directory_iterator key(keysdir); key != key_last; key++) {
			const char *lpFileName = NULL;

			if (is_directory(key->status()))
				continue;

			lpFileName = path_to_string(key->path()).c_str();

			biofile = BIO_new_file(lpFileName, "r");
			if (!biofile) {
				m_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_INFO, "Unable to create BIO for '%s': %s", lpFileName, ERR_error_string(ERR_get_error(), NULL));
				continue;
			}		

			storedkey = PEM_read_bio_PUBKEY(biofile, NULL, NULL, NULL);
			if (!storedkey) {
				m_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_INFO, "Unable to read PUBKEY from '%s': %s", lpFileName, ERR_error_string(ERR_get_error(), NULL));
				BIO_free(biofile);
				continue;
			}

			res = EVP_PKEY_cmp(pubkey, storedkey);

			BIO_free(biofile);
			EVP_PKEY_free(storedkey);

			if (res <= 0) {
				m_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_INFO, "Certificate '%s' does not match.", lpFileName);
			} else {
				er = erSuccess;
				m_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_INFO, "Accepted certificate '%s' from client.", lpFileName);
				break;
			}
		}
	} catch (const bfs::filesystem_error&) {
		// @todo: use get_error_info ?
		m_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_INFO, "Boost exception during certificate validation.");
	} catch (const std::exception& e) {
		m_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_INFO, "STD exception during certificate validation: %s", e.what());
	}
	if (er != erSuccess)
		goto exit;

    // Check whether user exists in the user database
	er = m_lpUserManagement->ResolveObjectAndSync(OBJECTCLASS_USER, lpszName, &m_ulUserID);
	if (er != erSuccess)
		goto exit;

	m_bValidated = true;
	m_ulValidationMethod = METHOD_SSL_CERT;

exit:
	if (cert)
		X509_free(cert);

	if (pubkey)
		EVP_PKEY_free(pubkey);

	return er;
}

#define NTLMBUFFER 8192
ECRESULT ECAuthSession::ValidateSSOData(struct soap *soap, char *lpszName, char *szClientVersion, char *szClientApp, struct xsd__base64Binary *lpInput, struct xsd__base64Binary **lppOutput)
{
	ECRESULT er = ZARAFA_E_LOGON_FAILED;

	// first NTLM package starts with that signature, continues are detected by the filedescriptor
	if (m_NTLM_pid != -1 || strncmp((const char*)lpInput->__ptr, "NTLM", 4) == 0)
		er = ValidateSSOData_NTLM(soap, lpszName, szClientVersion, szClientApp, lpInput, lppOutput);
	else
		er = ValidateSSOData_KRB5(soap, lpszName, szClientVersion, szClientApp, lpInput, lppOutput);

	return er;
}

#ifdef HAVE_GSSAPI
ECRESULT ECAuthSession::LogKRB5Error(ECLogger *lpLogger, const char *msg, OM_uint32 code, OM_uint32 type)
{
	gss_buffer_desc gssMessage = GSS_C_EMPTY_BUFFER;
	OM_uint32 retval = 0;
	OM_uint32 status = 0;
	OM_uint32 context = 0;

	while (true) {
		retval = gss_display_status(&status, code, type, GSS_C_NULL_OID, &context, &gssMessage);
		lpLogger->Log(EC_LOGLEVEL_ERROR, "%s: %s", msg, (char*)gssMessage.value);
		gss_release_buffer(&status, &gssMessage);
		if (!context)
			break;
	}
	return erSuccess;
}

ECRESULT ECAuthSession::LogKRB5Error(const char *msg, OM_uint32 major, OM_uint32 minor)
{
	ECLogger *lpLogger = m_lpSessionManager->GetLogger();
	LogKRB5Error(lpLogger, msg, major, GSS_C_GSS_CODE);
	LogKRB5Error(lpLogger, msg, minor, GSS_C_MECH_CODE);
	return erSuccess;
}
#endif

ECRESULT ECAuthSession::ValidateSSOData_KRB5(struct soap *soap, char *lpszName, char *szClientVersion, char *szClientApp, struct xsd__base64Binary *lpInput, struct xsd__base64Binary **lppOutput)
{
	ECRESULT er = ZARAFA_E_LOGON_FAILED;
#ifndef HAVE_GSSAPI
	m_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_FATAL, "Incoming kerberos request, but this server was build without GSSAPI support.");
#else
	OM_uint32 retval, status;

	gss_name_t gssServername = GSS_C_NO_NAME;
	gss_buffer_desc gssInputBuffer = GSS_C_EMPTY_BUFFER;
	char *szHostname = NULL;
	std::string principal;

	gss_name_t gssUsername = GSS_C_NO_NAME;
	gss_buffer_desc gssUserBuffer = GSS_C_EMPTY_BUFFER;
	gss_buffer_desc gssOutputToken = GSS_C_EMPTY_BUFFER;
	std::string strUsername;
	string::size_type pos;

	struct xsd__base64Binary *lpOutput = NULL;

	if (m_gssServerCreds == GSS_C_NO_CREDENTIAL) {
		m_gssContext = GSS_C_NO_CONTEXT;

		// ECServer made sure this setting option always contains the best hostname
		// If it's not there, that's unacceptable.
		szHostname = m_lpSessionManager->GetConfig()->GetSetting("server_hostname");
		if (!szHostname || szHostname[0] == '\0') {
			m_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_FATAL, "Hostname not found, required for Kerberos");
			goto exit;
		}
		principal = "zarafa@";
		principal += szHostname;

		m_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_DEBUG, "Kerberos principal: %s", (char*)principal.c_str());

		gssInputBuffer.value = (void*)principal.data();
		gssInputBuffer.length = principal.length() + 1;

		retval = gss_import_name(&status, &gssInputBuffer, GSS_C_NT_HOSTBASED_SERVICE, &gssServername);
		if (retval != GSS_S_COMPLETE) {
			LogKRB5Error("Unable to import server name", retval, status);
			goto exit;
		}

		retval = gss_acquire_cred(&status, gssServername, GSS_C_INDEFINITE, GSS_C_NO_OID_SET, GSS_C_ACCEPT, &m_gssServerCreds, NULL, NULL);
		if (retval != GSS_S_COMPLETE) {
			LogKRB5Error("Unable to acquire credentials handle", retval, status);
			goto exit;
		}
	}


	gssInputBuffer.length = lpInput->__size;
	gssInputBuffer.value = lpInput->__ptr;

	retval = gss_accept_sec_context(&status, &m_gssContext, m_gssServerCreds, &gssInputBuffer, GSS_C_NO_CHANNEL_BINDINGS, &gssUsername, NULL, &gssOutputToken, NULL, NULL, NULL);

	if (gssOutputToken.length) {
		// we need to send data back to the client, no need to consider retval
		lpOutput = s_alloc<struct xsd__base64Binary>(soap);
		lpOutput->__size = gssOutputToken.length;
		lpOutput->__ptr = s_alloc<unsigned char>(soap, gssOutputToken.length);
		memcpy(lpOutput->__ptr, gssOutputToken.value, gssOutputToken.length);

		gss_release_buffer(&status, &gssOutputToken);
	}

	if (retval == GSS_S_CONTINUE_NEEDED) {
		er = ZARAFA_E_SSO_CONTINUE;
		goto exit;
	} else if (retval != GSS_S_COMPLETE) {
		LogKRB5Error("Unable to accept security context", retval, status);
		LOG_AUDIT(m_lpSessionManager->GetAudit(), "authenticate failed user='%s' from='%s' method='kerberos sso' program='%s'",
				  lpszName, PrettyIP(soap->ip).c_str(), szClientApp);
		goto exit;
	}

	retval = gss_display_name(&status, gssUsername, &gssUserBuffer, NULL);
	if (retval) {
		LogKRB5Error("Unable to convert username", retval, status);
		goto exit;
	}

	m_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_DEBUG, "Kerberos username: %s", (char*)gssUserBuffer.value);
	// kerberos returns: username@REALM, username is case-insensitive
	strUsername.assign((char*)gssUserBuffer.value, gssUserBuffer.length);
	pos = strUsername.find_first_of('@');
	if (pos != string::npos)
		strUsername.erase(pos);

	if (stricmp(strUsername.c_str(), lpszName) == 0) {
		er = m_lpUserManagement->ResolveObjectAndSync(ACTIVE_USER, lpszName, &m_ulUserID);
		// don't check NONACTIVE, since those shouldn't be able to login
		if(er != erSuccess)
			goto exit;

		m_bValidated = true;
		m_ulValidationMethod = METHOD_SSO;
		m_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_INFO, "Kerberos Single signon: User Authenticated: %s", lpszName);
		LOG_AUDIT(m_lpSessionManager->GetAudit(), "authenticate ok user='%s' from='%s' method='kerberos sso' program='%s'",
				  lpszName, PrettyIP(soap->ip).c_str(), szClientApp);
	} else {
		m_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_ERROR, "Kerberos username %s authenticated, but user %s requested.", (char*)gssUserBuffer.value, lpszName);
		LOG_AUDIT(m_lpSessionManager->GetAudit(), "authenticate spoofed user='%s' requested='%s' from='%s' method='kerberos sso' program='%s'",
				  (char*)gssUserBuffer.value, lpszName, PrettyIP(soap->ip).c_str(), szClientApp);
	}

exit:
	if (gssUserBuffer.length)
		gss_release_buffer(&status, &gssUserBuffer);

	if (gssOutputToken.length)
		gss_release_buffer(&status, &gssOutputToken);

	if (gssUsername != GSS_C_NO_NAME)
		gss_release_name(&status, &gssUsername);

	if (gssServername != GSS_C_NO_NAME)
		gss_release_name(&status, &gssServername);

	*lppOutput = lpOutput;
#endif

	return er;
}

ECRESULT ECAuthSession::ValidateSSOData_NTLM(struct soap *soap, char *lpszName, char *szClientVersion, char *szClientApp, struct xsd__base64Binary *lpInput, struct xsd__base64Binary **lppOutput)
{
	ECRESULT er = ZARAFA_E_LOGON_FAILED;
	struct xsd__base64Binary *lpOutput = NULL;
	char buffer[NTLMBUFFER];
	std::string strEncoded, strDecoded, strAnswer;
	size_t bytes = 0;
	char separator = '\\';		// get config version
	fd_set fds;
	int max, ret;
	struct timeval tv;

	strEncoded = base64_encode(lpInput->__ptr, lpInput->__size);
	errno = 0;

	if (m_NTLM_pid == -1) {
		// start new ntlmauth pipe
		// TODO: configurable path?

		if (pipe(m_NTLM_stdin) == -1 || pipe(m_NTLM_stdout) == -1 || pipe(m_NTLM_stderr) == -1) {
			m_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_FATAL, string("Unable to create communication pipes for ntlm_auth: ") + strerror(errno));
			goto exit;
		}
		
		/*
		 * Why are we using vfork() ?
		 *
		 * You might as well use fork() here but vfork() is much faster in our case; this is because vfork() doesn't actually duplicate
		 * any pages, expecting you to call execl(). Watch out though, since data changes done in the client process before execl() WILL
		 * affect the mother process. (however, the file descriptor table is correctly cloned)
		 *
		 * The reason fork() is slow is that even though it is doing a Copy-On-Write copy, it still needs to do some page-copying to set up your
		 * process. This copying time increases with memory usage of the mother process; in fact, running 200 forks() on a process occupying
		 * 512MB of memory takes 15 seconds, while the same vfork()/exec() loop takes under .5 of a second.
		 *
		 * If vfork() is not available, or is broken on another platform, it is safe to simply replace it with fork(), but it will be quite slow!
		 */

		m_NTLM_pid = vfork();
		if (m_NTLM_pid == -1) {
			// broken
			m_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_FATAL, string("Unable to start new process for ntlm_auth: ") + strerror(errno));
			goto exit;
		} else if (m_NTLM_pid == 0) {
			// client
			int j, k;

			close(m_NTLM_stdin[1]);
			close(m_NTLM_stdout[0]);
			close(m_NTLM_stderr[0]);

			dup2(m_NTLM_stdin[0], 0);
			dup2(m_NTLM_stdout[1], 1);
			dup2(m_NTLM_stderr[1], 2);

			// close all other open file descriptors, so ntlm doesn't keep the zarafa-server sockets open
			j = getdtablesize();
			for (k=3; k<j; k++)
				close(k);

			execl("/bin/sh", "sh", "-c", "ntlm_auth -d0 --helper-protocol=squid-2.5-ntlmssp", NULL);

			m_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_FATAL, string("Cannot start ntlm_auth: ") + strerror(errno));
			_exit(2);
		} else {
			// parent
			m_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_INFO, "New ntlm_auth started on pid %d", m_NTLM_pid);
			close(m_NTLM_stdin[0]);
			close(m_NTLM_stdout[1]);
			close(m_NTLM_stderr[1]);
			m_stdin = relocate_fd(m_NTLM_stdin[1],  m_lpSessionManager->GetLogger());
			m_stdout = relocate_fd(m_NTLM_stdout[0],  m_lpSessionManager->GetLogger());
			m_stderr = relocate_fd(m_NTLM_stderr[0],  m_lpSessionManager->GetLogger());

			// Yo! Refresh!
			write(m_stdin, "YR ", 3);
			write(m_stdin, strEncoded.c_str(), strEncoded.length());
			write(m_stdin, "\n", 1);
		}
	} else {
		// Knock knock! who's there?
		write(m_stdin, "KK ", 3);
		write(m_stdin, strEncoded.c_str(), strEncoded.length());
		write(m_stdin, "\n", 1);
	}

	memset(buffer, 0, NTLMBUFFER);

	tv.tv_sec = 10;				// timeout of 10 seconds before ntlm_auth can respond too large?
	tv.tv_usec = 0;

	FD_ZERO(&fds);
	FD_SET(m_stdout, &fds);
	FD_SET(m_stderr, &fds);
	max = m_stderr > m_stdout ? m_stderr : m_stdout;

	ret = select(max+1, &fds, NULL, NULL, &tv);
	if (ret < 0) {
		m_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_FATAL, string("Error while waiting for data from ntlm_auth: ") + strerror(errno));
		goto exit;
	} else if (ret == 0) {
		// timeout
		m_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_FATAL, "Timeout while reading from ntlm_auth");
		goto exit;
	}

	// stderr is optional, and always written first
	if (FD_ISSET(m_stderr, &fds)) {
		// log stderr of ntlm_auth to logfile (loop?)
		bytes = read(m_stderr, buffer, NTLMBUFFER-1);
		m_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_FATAL, string("Received error from ntlm_auth:\n") + buffer);
		goto exit;
	}

	// stdout is mandatory, so always read from this pipe
	memset(buffer, 0, NTLMBUFFER);
	bytes = read(m_stdout, buffer, NTLMBUFFER-1);
	if (bytes == (size_t)-1) {
		m_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_FATAL, string("Unable to read data from ntlm_auth: ") + strerror(errno));
		goto exit;
	}

	// make answer all data after the command replay
	strAnswer.assign(buffer, 3, bytes-3-1); // -1 because of \n

	if (buffer[0] == 'B' && buffer[1] == 'H') {
		// Broken Helper
		m_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_FATAL, "Incorrect data fed to ntlm_auth");
		goto exit;
	} else if (buffer[0] == 'T' && buffer[1] == 'T') {
		// Try This
		strDecoded = base64_decode(strAnswer);

		lpOutput = s_alloc<struct xsd__base64Binary>(soap);
		lpOutput->__size = strDecoded.length();
		lpOutput->__ptr = s_alloc<unsigned char>(soap, strDecoded.length());
		memcpy(lpOutput->__ptr, strDecoded.data(), strDecoded.length());

		er = ZARAFA_E_SSO_CONTINUE;
		
	} else if (buffer[0] == 'A' && buffer[1] == 'F') {
		// Authentication Fine
		// Samba default runs in UTF-8 and setting 'unix charset' to windows-1252 in the samba config will break ntlm_auth
		// convert the username before we use it in Zarafa
		ECIConv iconv("windows-1252", "utf-8");
		strAnswer = iconv.convert(strAnswer);

		m_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_INFO, "Found username (%s)", strAnswer.c_str());

		// if the domain separator is not found, assume we only have the username (samba)
		string::size_type pos = strAnswer.find_first_of(separator);
		if (pos != string::npos) {
			pos++;
			strAnswer.assign(strAnswer, pos, strAnswer.length()-pos);
		}

		// Check whether user exists in the user database
		er = m_lpUserManagement->ResolveObjectAndSync(ACTIVE_USER, (char *)strAnswer.c_str(), &m_ulUserID);
		// don't check NONACTIVE, since those shouldn't be able to login
		if(er != erSuccess)
			goto exit;

		if (stricmp(lpszName, strAnswer.c_str()) != 0) {
			// cannot open another user without password
			// or should we check permissions ?
			m_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_WARNING, "Single sign on: User %s Authenticated, but User %s requested.", strAnswer.c_str(), lpszName);
			LOG_AUDIT(m_lpSessionManager->GetAudit(), "authenticate spoofed user='%s' requested='%s' from='%s' method='ntlm sso' program='%s'",
					  strAnswer.c_str(), lpszName, PrettyIP(soap->ip).c_str(), szClientApp);
			er = ZARAFA_E_LOGON_FAILED;
		} else {
			m_bValidated = true;
        	m_ulValidationMethod = METHOD_SSO;
			er = erSuccess;
			m_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_INFO, "Single sign on: User Authenticated: %s", strAnswer.c_str());
			LOG_AUDIT(m_lpSessionManager->GetAudit(), "authenticate ok user='%s' from='%s' method='ntlm sso' program='%s'",
					  lpszName, PrettyIP(soap->ip).c_str(), szClientApp);
		}

	} else if (buffer[0] == 'N' && buffer[1] == 'A') {
		// Not Authenticated
		m_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_INFO, "Requested user '%s' denied. Not Authenticated: %s", lpszName, strAnswer.c_str());
		LOG_AUDIT(m_lpSessionManager->GetAudit(), "authenticate failed user='%s' from='%s' method='ntlm sso' program='%s'",
				  lpszName, PrettyIP(soap->ip).c_str(), szClientApp);
		er = ZARAFA_E_LOGON_FAILED;
	} else {
		// unknown response?
		buffer[bytes-1] = '\0';	// strip enter
		m_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_FATAL, "Unknown response from ntlm_auth: %s", buffer);
		er = ZARAFA_E_CALL_FAILED;
		goto exit;
	}

	*lppOutput = lpOutput;

exit:
	return er;
}
#undef NTLMBUFFER

ECAuthSessionOffline::ECAuthSessionOffline(const std::string &strSourceAddr, ECSESSIONID sessionID, ECDatabaseFactory *lpDatabaseFactory, ECSessionManager *lpSessionManager, unsigned int ulCapabilities) : 
	ECAuthSession(strSourceAddr, sessionID, lpDatabaseFactory, lpSessionManager, ulCapabilities)
{
	// nothing todo
}

ECRESULT ECAuthSessionOffline::CreateECSession(ECSESSIONGROUPID ecSessionGroupId, std::string strClientVersion, std::string strClientApp, ECSESSIONID *sessionID, ECSession **lppNewSession) {
	ECRESULT er = erSuccess;
	ECSession *lpSession = NULL;
	ECSESSIONID newSID;

	if (!m_bValidated) {
		er = ZARAFA_E_LOGON_FAILED;
		goto exit;
	}

	CreateSessionID(m_ulClientCapabilities, &newSID);

	// Offline version
	lpSession = new ECSession(m_strSourceAddr, newSID, ecSessionGroupId, m_lpDatabaseFactory, m_lpSessionManager, m_ulClientCapabilities, true, m_ulValidationMethod, m_ulConnectingPid, strClientVersion, strClientApp);
	if (!lpSession) {
		er = ZARAFA_E_NOT_ENOUGH_MEMORY;
		goto exit;
	}

	er = lpSession->GetSecurity()->SetUserContext(m_ulUserID);
	if (er != erSuccess)
		goto exit;				// user not found anymore, or error in getting groups

	*sessionID = newSID;
	*lppNewSession = lpSession;

exit:
	if (er != erSuccess && lpSession)
		delete lpSession;

	return er;
}
