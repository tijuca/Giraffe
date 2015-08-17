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
class SOURCEKEY;


void CreateSessionID(unsigned int ulCapabilities, ECSESSIONID *lpSessionId);

enum { SESSION_STATE_PROCESSING, SESSION_STATE_SENDING };

typedef struct {
    const char *fname;
    struct timespec threadstart;
    double start;
    pthread_t threadid;
    int state;
} BUSYSTATE;

/*
  BaseType session
*/
class BTSession {
public:
	BTSession(const std::string& strSourceAddr, ECSESSIONID sessionID, ECDatabaseFactory *lpDatabaseFactory, ECSessionManager *lpSessionManager, unsigned int ulCapabilities);
	virtual ~BTSession();

	virtual ECRESULT Shutdown(unsigned int ulTimeout);

	virtual ECRESULT ValidateOriginator(struct soap *soap);
	virtual ECSESSIONID GetSessionId();

	virtual time_t GetSessionTime();
	virtual void UpdateSessionTime();
	virtual unsigned int GetCapabilities();
	virtual ECSessionManager* GetSessionManager();
	virtual ECUserManagement* GetUserManagement();
	virtual ECRESULT GetDatabase(ECDatabase **lppDatabase);
	virtual ECRESULT GetAdditionalDatabase(ECDatabase **lppDatabase);
	ECRESULT GetServerGUID(GUID* lpServerGuid);
	ECRESULT GetNewSourceKey(SOURCEKEY* lpSourceKey);

	virtual void SetClientMeta(const char *const lpstrClientVersion, const char *const lpstrClientMisc);
	virtual void GetClientApplicationVersion(std::string *lpstrClientApplicationVersion);
	virtual void GetClientApplicationMisc(std::string *lpstrClientApplicationMisc);

	virtual void Lock();
	virtual void Unlock();
	virtual bool IsLocked();
	
	virtual void RecordRequest(struct soap *soap);
	virtual unsigned int GetRequests();

	virtual void GetClientPort(unsigned int *lpulPort);
	virtual void GetRequestURL(std::string *lpstrURL);
	virtual void GetProxyHost(std::string *lpstrProxyHost);

	size_t GetInternalObjectSize();
	virtual size_t GetObjectSize() = 0;

	time_t GetIdleTime();
	std::string GetSourceAddr();

	typedef enum {
	    METHOD_NONE, METHOD_USERPASSWORD, METHOD_SOCKET, METHOD_SSO, METHOD_SSL_CERT
    } AUTHMETHOD;

protected:
	unsigned int		m_ulRefCount;

	std::string		m_strSourceAddr;
	ECSESSIONID		m_sessionID;
	bool			m_bCheckIP;

	time_t			m_sessionTime;
	unsigned int		m_ulSessionTimeout;

	ECDatabaseFactory	*m_lpDatabaseFactory;
	ECSessionManager	*m_lpSessionManager;
	ECUserManagement	*m_lpUserManagement;

	unsigned int		m_ulClientCapabilities;

	pthread_cond_t		m_hThreadReleased;
	pthread_mutex_t		m_hThreadReleasedMutex;	
	
	pthread_mutex_t		m_hRequestStats;
	unsigned int		m_ulRequests;
	std::string		m_strLastRequestURL;
	std::string		m_strProxyHost;
	unsigned int		m_ulLastRequestPort;

	std::string		m_strClientApplicationVersion, m_strClientApplicationMisc;
};

/*
  Normal session
*/
class ECSession : public BTSession
{
public:
	ECSession(const std::string& strSourceAddr, ECSESSIONID sessionID, ECSESSIONGROUPID ecSessionGroupId, ECDatabaseFactory *lpDatabaseFactory, ECSessionManager *lpSessionManager, unsigned int ulCapabilities, bool bIsOffline, AUTHMETHOD ulAuthMethod, int pid, std::string strClientVersion, std::string strClientApp, std::string strClientApplicationVersion, std::string strClientApplicationMisc);

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
	
	ECRESULT GetObjectFromEntryId(const entryId *lpEntryId, unsigned int *lpulObjId, unsigned int *lpulEidFlags = NULL);
	ECRESULT LockObject(unsigned int ulObjId);
	ECRESULT UnlockObject(unsigned int ulObjId);
	
	/* for ECStatsSessionTable */
	void AddBusyState(pthread_t threadId, const char *lpszState, struct timespec threadstart, double start);
	void UpdateBusyState(pthread_t threadId, int state);
	void RemoveBusyState(pthread_t threadId);
	void GetBusyStates(std::list<BUSYSTATE> *lpLstStates);
	
	void AddClocks(double dblUser, double dblSystem, double dblReal);
	void GetClocks(double *lpdblUser, double *lpdblSystem, double *lpdblReal);
	void GetClientVersion(std::string *lpstrVersion);
	void GetClientApp(std::string *lpstrClientApp);

	size_t GetObjectSize();

	unsigned int ClientVersion() const { return m_ulClientVersion; }

	AUTHMETHOD GetAuthMethod();

private:
	ECTableManager		*m_lpTableManager;
	ECSessionGroup		*m_lpSessionGroup;
	ECSecurity		*m_lpEcSecurity;

	pthread_mutex_t		m_hStateLock;
	typedef std::map<pthread_t, BUSYSTATE> BusyStateMap;
	BusyStateMap		m_mapBusyStates; /* which thread does what function */
	double			m_dblUser;
	double			m_dblSystem;
	double			m_dblReal;
	AUTHMETHOD		m_ulAuthMethod;
	int			m_ulConnectingPid;
	ECSESSIONGROUPID m_ecSessionGroupId;
	std::string		m_strClientVersion;
	unsigned int		m_ulClientVersion;
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
	ECAuthSession(const std::string& strSourceAddr, ECSESSIONID sessionID, ECDatabaseFactory *lpDatabaseFactory, ECSessionManager *lpSessionManager, unsigned int ulCapabilities);
	virtual ~ECAuthSession();

	ECRESULT ValidateUserLogon(const char* lpszName, const char* lpszPassword, const char* lpszImpersonateUser);
	ECRESULT ValidateUserSocket(int socket, const char* lpszName, const char* lpszImpersonateUser);
	ECRESULT ValidateUserCertificate(soap* soap, const char* lpszName, const char* lpszImpersonateUser);
	ECRESULT ValidateSSOData(struct soap* soap, const char* lpszName, const char* lpszImpersonateUser, const char* szClientVersion, const char* szClientApp, const char *szClientAppVersion, const char *szClientAppMisc, const struct xsd__base64Binary* lpInput, struct xsd__base64Binary** lppOutput);

	virtual ECRESULT CreateECSession(ECSESSIONGROUPID ecSessionGroupId, std::string strClientVersion, std::string strClientApp, std::string strClientAppVersion, std::string strClientAppMisc, ECSESSIONID *sessionID, ECSession **lppNewSession);

	size_t GetObjectSize();

protected:
	unsigned int m_ulUserID;
	unsigned int m_ulImpersonatorID;    // The ID of the user who's credentials were used to login when using impersonation
	bool m_bValidated;
	
	AUTHMETHOD m_ulValidationMethod;
	int m_ulConnectingPid;

private:
	/* SSO */
	ECRESULT ValidateSSOData_NTLM(struct soap* soap, const char* lpszName, const char* szClientVersion, const char* szClientApp, const char *szClientAppVersion, const char *szClientAppMisc, const struct xsd__base64Binary* lpInput, struct xsd__base64Binary** lppOutput);
	ECRESULT ValidateSSOData_KRB5(struct soap* soap, const char* lpszName, const char* szClientVersion, const char* szClientApp, const char *szClientAppVersion, const char *szClientAppMisc, const struct xsd__base64Binary* lpInput, struct xsd__base64Binary** lppOutput);
#ifdef HAVE_GSSAPI
	ECRESULT LogKRB5Error(ECLogger *lpLogger, const char *msg, OM_uint32 major, OM_uint32 minor); /* added logger to distinguish from the other function */
	ECRESULT LogKRB5Error(const char *msg, OM_uint32 major, OM_uint32 minor);
#endif

	ECRESULT ProcessImpersonation(const char* lpszImpersonateUser);

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
	ECAuthSessionOffline(const std::string& strSourceAddr, ECSESSIONID sessionID, ECDatabaseFactory *lpDatabaseFactory, ECSessionManager *lpSessionManager, unsigned int ulCapabilities);

	ECRESULT CreateECSession(ECSESSIONGROUPID ecSessionGroupId, std::string strClientVersion, std::string strClientApp, std::string strClientAppVersion, std::string strClientAppMisc, ECSESSIONID *sessionID, ECSession **lppNewSession);
};

#endif // #ifndef ECSESSION
