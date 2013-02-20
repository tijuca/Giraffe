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

// ECSession.h: interface for the ECSession class.
//
//////////////////////////////////////////////////////////////////////

#ifndef ECSESSION
#define ECSESSION

#include <list>
#include <map>
#include <set>

#include "soapH.h"
#include "ZarafaCode.h"
#include "ECNotification.h"
#include "ECTableManager.h"

#include "ECConfig.h"
#include "ECLogger.h"
#include "ECDatabaseFactory.h"
#include "ECPluginFactory.h"
#include "ECSessionGroup.h"
#include "ECLockManager.h"
#include "Zarafa.h"

#ifdef HAVE_GSSAPI
#include <gssapi/gssapi.h>
#endif


class ECSecurity;
class ECUserManagement;
class ECShortTermEntryIDManager;
class SOURCEKEY;


void CreateSessionID(unsigned int ulCapabilities, ECSESSIONID *lpSessionId);

/*
  BaseType session
*/
class BTSession {
public:
	BTSession(unsigned long ip, ECSESSIONID sessionID, ECDatabaseFactory *lpDatabaseFactory, ECSessionManager *lpSessionManager, unsigned int ulCapabilities);
	virtual ~BTSession();

	virtual ECRESULT Shutdown(unsigned int ulTimeout);

	virtual ECRESULT ValidateIp(unsigned long ip);
	virtual ECSESSIONID GetSessionId();

	virtual time_t GetSessionTime();
	virtual void UpdateSessionTime();
	virtual unsigned int GetCapabilities();
	virtual ECSessionManager* GetSessionManager();
	virtual ECUserManagement* GetUserManagement();
	virtual ECRESULT GetDatabase(ECDatabase **lppDatabase);
	ECRESULT GetServerGUID(GUID* lpServerGuid);
	ECRESULT GetNewSourceKey(SOURCEKEY* lpSourceKey);

	virtual void Lock();
	virtual void Unlock();
	virtual bool IsLocked();
	
	virtual void IncRequests();
    virtual unsigned int GetRequests();

	time_t GetIdleTime();
	unsigned long GetIpAddress();

	typedef enum {
	    METHOD_NONE, METHOD_USERPASSWORD, METHOD_SOCKET, METHOD_SSO, METHOD_SSL_CERT
    } AUTHMETHOD;
    
protected:
	unsigned int		m_ulRefCount;

	unsigned long		m_ip;
	ECSESSIONID			m_sessionID;
	bool				m_bCheckIP;

	time_t				m_sessionTime;
	unsigned int		m_ulSessionTimeout;

	ECDatabaseFactory	*m_lpDatabaseFactory;
	ECSessionManager	*m_lpSessionManager;
	ECUserManagement	*m_lpUserManagement;

	unsigned int		m_ulClientCapabilities;

	pthread_cond_t		m_hThreadReleased;
	pthread_mutex_t		m_hThreadReleasedMutex;	
	
	pthread_mutex_t		m_hRequestStats;
	unsigned int		m_ulRequests;
};

/*
  Normal session
*/
class ECSession : public BTSession
{
public:
	ECSession(unsigned long ip, ECSESSIONID sessionID, ECSESSIONGROUPID sessionGroupID, ECDatabaseFactory *lpDatabaseFactory, ECSessionManager *lpSessionManager, unsigned int ulCapabilities, bool bIsOffline, AUTHMETHOD ulAuthMethod, int pid, std::string strClientVersion, std::string strClientApp);

	virtual ECSESSIONGROUPID GetSessionGroupId();
	virtual int				 GetConnectingPid();

	virtual ~ECSession();

	virtual ECRESULT Shutdown(unsigned int ulTimeout);

	/* Notification functions all wrap directly to SessionGroup */
	ECRESULT AddAdvise(unsigned int ulConnection, unsigned int ulKey, unsigned int ulEventMask);
	ECRESULT AddChangeAdvise(unsigned int ulConnection, notifySyncState *lpSyncState);
	ECRESULT DelAdvise(unsigned int ulConnection);
	ECRESULT AddNotificationTable(unsigned int ulType, unsigned int ulObjType, unsigned int ulTableId, sObjectTableKey *lpsChildRow, sObjectTableKey *lpsPrevRow, struct propValArray *lpRow);
	ECRESULT GetNotifyItems(struct soap *soap, struct notifyResponse *notifications);

	ECTableManager* GetTableManager();
	ECSecurity* GetSecurity();
	ECShortTermEntryIDManager* GetShortTermEntryIDManager();
	
	ECRESULT GetObjectFromEntryId(const entryId *lpEntryId, unsigned int *lpulObjId, bool *lpbIsShortTerm = NULL);
	ECRESULT LockObject(unsigned int ulObjId);
	ECRESULT UnlockObject(unsigned int ulObjId);
	
	/* for ECStatsSessionTable */
	void AddBusyState(pthread_t threadId, const char *lpszState);
	void RemoveBusyState(pthread_t threadId);
	void GetBusyStates(std::list<std::string> *lpLstStates);
	
	void AddClocks(double dblUser, double dblSystem, double dblReal);
	void GetClocks(double *lpdblUser, double *lpdblSystem, double *lpdblReal);
	void GetClientVersion(std::string *lpstrVersion);
    void GetClientApp(std::string *lpstrClientApp);

	unsigned int ClientVersion() const { return m_ulClientVersion; }

	AUTHMETHOD GetAuthMethod();

private:
	ECTableManager		*m_lpTableManager;
	ECSessionGroup		*m_lpSessionGroup;
	ECSecurity			*m_lpEcSecurity;

	pthread_mutex_t		m_hStateLock;
	std::map<pthread_t, const char*> m_mapBusyStates; /* which thread does what function */
	double			m_dblUser;
	double			m_dblSystem;
	double			m_dblReal;
	AUTHMETHOD		m_ulAuthMethod;
	int				m_ulConnectingPid;
	ECSESSIONGROUPID m_ecSessionGroupId;
	std::string		m_strClientVersion;
	unsigned int	m_ulClientVersion;
	std::string		m_strClientApp;
	std::string		m_strUsername;

	typedef std::map<unsigned int, ECObjectLock>	LockMap;
	pthread_mutex_t	m_hLocksLock;
	LockMap			m_mapLocks;
};


/*
  Authentication session
*/
class ECAuthSession : public BTSession {
public:
	ECAuthSession(unsigned long ip, ECSESSIONID sessionID, ECDatabaseFactory *lpDatabaseFactory, ECSessionManager *lpSessionManager, unsigned int ulCapabilities);
	virtual ~ECAuthSession();

	ECRESULT ValidateUserLogon(char *lpszName, char *lpszPassword);
	ECRESULT ValidateUserSocket(int socket, char *lpszName);
	ECRESULT ValidateUserCertificate(struct soap *soap, char *lpszName);
	ECRESULT ValidateSSOData(struct soap *soap, char *lpszName, char *szClientVersion, char *szClientApp, struct xsd__base64Binary *lpInput, struct xsd__base64Binary **lppOutput);

	virtual ECRESULT CreateECSession(ECSESSIONGROUPID ecSessionGroupId, std::string strClientVersion, std::string strClientApp, ECSESSIONID *sessionID, ECSession **lppNewSession);

protected:
	unsigned int m_ulUserID;
	bool m_bValidated;
	
	AUTHMETHOD m_ulValidationMethod;
	int m_ulConnectingPid;

private:
	/* SSO */
	ECRESULT ValidateSSOData_NTLM(struct soap *soap, char *lpszName, char *szClientVersion, char *szClientApp, struct xsd__base64Binary *lpInput, struct xsd__base64Binary **lppOutput);
	ECRESULT ValidateSSOData_KRB5(struct soap *soap, char *lpszName, char *szClientVersion, char *szClientApp, struct xsd__base64Binary *lpInput, struct xsd__base64Binary **lppOutput);
#ifdef HAVE_GSSAPI
	ECRESULT LogKRB5Error(ECLogger *lpLogger, const char *msg, OM_uint32 major, OM_uint32 minor); /* added logger to distinguish from the other function */
	ECRESULT LogKRB5Error(const char *msg, OM_uint32 major, OM_uint32 minor);
#endif

	/* NTLM */
	pid_t m_NTLM_pid;
	int m_NTLM_stdin[2], m_NTLM_stdout[2], m_NTLM_stderr[2];
	int m_stdin, m_stdout, m_stderr; /* shortcuts to the above */

#ifdef HAVE_GSSAPI
	/* KRB5 */
	gss_cred_id_t m_gssServerCreds;
	gss_ctx_id_t m_gssContext;
#endif

};

/*
  Authentication for offline session
*/
class ECAuthSessionOffline : public ECAuthSession
{
public:
	ECAuthSessionOffline(unsigned long ip, ECSESSIONID sessionID, ECDatabaseFactory *lpDatabaseFactory, ECSessionManager *lpSessionManager, unsigned int ulCapabilities);

	ECRESULT CreateECSession(ECSESSIONGROUPID ecSessionGroupId, std::string strClientVersion, std::string strClientApp, ECSESSIONID *sessionID, ECSession **lppNewSession);

};

#endif // #ifndef ECSESSION
