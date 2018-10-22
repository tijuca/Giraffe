/*
 * SPDX-License-Identifier: AGPL-3.0-only
 * Copyright 2005 - 2016 Zarafa and its licensors
 */

// ECSession.h: interface for the ECSession class.
//
//////////////////////////////////////////////////////////////////////

#ifndef ECSESSION
#define ECSESSION

#include <kopano/zcdefs.h>
#include <atomic>
#include <condition_variable>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <pthread.h>
#include "soapH.h"
#include <kopano/kcodes.h>
#include "ECNotification.h"
#include "ECTableManager.h"
#include <kopano/ECConfig.h>
#include <kopano/ECLogger.h>
#include <kopano/timeutil.hpp>
#include "ECDatabaseFactory.h"
#include "ECPluginFactory.h"
#include "ECSessionGroup.h"
#include "ECLockManager.h"
#include "kcore.hpp"
#ifdef HAVE_GSSAPI
#include <gssapi/gssapi.h>
#endif

struct soap;

namespace KC {

class ECSecurity;
class ECUserManagement;
class SOURCEKEY;

void CreateSessionID(unsigned int ulCapabilities, ECSESSIONID *lpSessionId);

enum { SESSION_STATE_PROCESSING, SESSION_STATE_SENDING };

struct BUSYSTATE {
    const char *fname;
    struct timespec threadstart;
	KC::time_point start;
    pthread_t threadid;
    int state;
};

/*
  BaseType session
*/
class _kc_export BTSession {
public:
	_kc_hidden BTSession(const char *addr, ECSESSIONID, ECDatabaseFactory *, ECSessionManager *, unsigned int caps);
	_kc_hidden virtual ~BTSession(void) = default;
	_kc_hidden virtual ECRESULT Shutdown(unsigned int timeout);
	_kc_hidden virtual ECRESULT ValidateOriginator(struct soap *);
	_kc_hidden virtual ECSESSIONID GetSessionId() const final { return m_sessionID; }
	_kc_hidden virtual time_t GetSessionTime() const final { return m_sessionTime + m_ulSessionTimeout; }
	_kc_hidden virtual void UpdateSessionTime(void);
	_kc_hidden virtual unsigned int GetCapabilities() const final { return m_ulClientCapabilities; }
	_kc_hidden virtual ECSessionManager *GetSessionManager() const final { return m_lpSessionManager; }
	_kc_hidden virtual ECUserManagement *GetUserManagement(void) const = 0;
	virtual ECRESULT GetDatabase(ECDatabase **);
	_kc_hidden virtual ECRESULT GetAdditionalDatabase(ECDatabase **);
	_kc_hidden ECRESULT GetServerGUID(GUID *);
	_kc_hidden ECRESULT GetNewSourceKey(SOURCEKEY *);
	_kc_hidden virtual void SetClientMeta(const char *cl_vers, const char *cl_misc);
	_kc_hidden virtual void GetClientApplicationVersion(std::string *);
	_kc_hidden virtual void GetClientApplicationMisc(std::string *);
	virtual void lock();
	virtual void unlock();
	_kc_hidden virtual bool IsLocked() const final { return m_ulRefCount > 0; }
	_kc_hidden virtual void RecordRequest(struct soap *);
	_kc_hidden virtual unsigned int GetRequests(void);
	_kc_hidden virtual unsigned int GetClientPort();
	_kc_hidden virtual std::string GetRequestURL();
	_kc_hidden virtual std::string GetProxyHost();
	_kc_hidden size_t GetInternalObjectSize(void);
	_kc_hidden virtual size_t GetObjectSize(void) = 0;
	_kc_hidden time_t GetIdleTime() const;
	_kc_hidden const std::string &GetSourceAddr(void) const { return m_strSourceAddr; }

	enum AUTHMETHOD {
	    METHOD_NONE, METHOD_USERPASSWORD, METHOD_SOCKET, METHOD_SSO, METHOD_SSL_CERT
	};

protected:
	std::atomic<unsigned int> m_ulRefCount{0};
	std::string		m_strSourceAddr;
	ECSESSIONID		m_sessionID;
	bool m_bCheckIP = true;
	time_t			m_sessionTime;
	unsigned int m_ulSessionTimeout = 300, m_ulClientCapabilities;
	unsigned int m_ulRequests = 0, m_ulLastRequestPort = 0;
	ECDatabaseFactory	*m_lpDatabaseFactory;
	ECSessionManager	*m_lpSessionManager;
	/*
	 * Protects the object from deleting while a thread is running on a
	 * method in this object.
	 */
	std::condition_variable m_hThreadReleased;
	std::mutex m_hThreadReleasedMutex, m_hRequestStats;
	std::string m_strLastRequestURL, m_strProxyHost;
	std::string		m_strClientApplicationVersion, m_strClientApplicationMisc;
};

/*
  Normal session
*/
class _kc_export_dycast ECSession final : public BTSession {
public:
	_kc_hidden ECSession(const char *addr, ECSESSIONID, ECSESSIONGROUPID, ECDatabaseFactory *, ECSessionManager *, unsigned int caps, AUTHMETHOD, int pid, const std::string &cl_vers, const std::string &cl_app, const std::string &cl_app_ver, const std::string &cl_app_misc);
	_kc_hidden virtual ECSESSIONGROUPID GetSessionGroupId() const final { return m_ecSessionGroupId; }
	_kc_hidden virtual int GetConnectingPid() const final { return m_ulConnectingPid; }
	_kc_hidden virtual ~ECSession(void);
	_kc_hidden virtual ECRESULT Shutdown(unsigned int timeout);
	_kc_hidden virtual ECUserManagement *GetUserManagement(void) const override final { return m_lpUserManagement.get(); }

	/* Notification functions all wrap directly to SessionGroup */
	_kc_hidden ECRESULT AddAdvise(unsigned int conn, unsigned int key, unsigned int event_mask);
	_kc_hidden ECRESULT AddChangeAdvise(unsigned int conn, notifySyncState *);
	_kc_hidden ECRESULT DelAdvise(unsigned int conn);
	_kc_hidden ECRESULT AddNotificationTable(unsigned int type, unsigned int obj_type, unsigned int table, sObjectTableKey *child_row, sObjectTableKey *prev_row, struct propValArray *row);
	_kc_hidden ECRESULT GetNotifyItems(struct soap *, struct notifyResponse *notifications);
	_kc_hidden ECTableManager *GetTableManager(void) const { return m_lpTableManager.get(); }
	_kc_hidden ECSecurity *GetSecurity(void) const { return m_lpEcSecurity.get(); }
	_kc_hidden ECRESULT GetObjectFromEntryId(const entryId *, unsigned int *obj_id, unsigned int *eid_flags = nullptr);
	_kc_hidden ECRESULT LockObject(unsigned int obj_id);
	_kc_hidden ECRESULT UnlockObject(unsigned int obj_id);

	/* for ECStatsSessionTable */
	_kc_hidden void AddBusyState(pthread_t, const char *state, const struct timespec &threadstart, const KC::time_point &start);
	_kc_hidden void UpdateBusyState(pthread_t, int state);
	_kc_hidden void RemoveBusyState(pthread_t);
	_kc_hidden void GetBusyStates(std::list<BUSYSTATE> *);
	_kc_hidden void AddClocks(double user, double system, double real);
	_kc_hidden void GetClocks(double *user, double *system, double *real);
	_kc_hidden void GetClientVersion(std::string *version);
	_kc_hidden void GetClientApp(std::string *client_app);
	_kc_hidden size_t GetObjectSize(void);
	_kc_hidden unsigned int ClientVersion(void) const { return m_ulClientVersion; }
	_kc_hidden AUTHMETHOD GetAuthMethod(void) const { return m_ulAuthMethod; }

private:
	ECSessionGroup		*m_lpSessionGroup;
	std::mutex m_hStateLock;
	typedef std::map<pthread_t, BUSYSTATE> BusyStateMap;
	BusyStateMap		m_mapBusyStates; /* which thread does what function */
	double m_dblUser = 0, m_dblSystem = 0, m_dblReal = 0;
	AUTHMETHOD		m_ulAuthMethod;
	int			m_ulConnectingPid;
	ECSESSIONGROUPID m_ecSessionGroupId;
	std::string m_strClientVersion, m_strClientApp, m_strUsername;
	unsigned int		m_ulClientVersion;

	typedef std::map<unsigned int, ECObjectLock>	LockMap;
	std::mutex m_hLocksLock;
	LockMap			m_mapLocks;
	std::unique_ptr<ECSecurity> m_lpEcSecurity;
	std::unique_ptr<ECUserManagement> m_lpUserManagement;
	std::unique_ptr<ECTableManager> m_lpTableManager;
};

/*
  Authentication session
*/
class _kc_export_dycast ECAuthSession final : public BTSession {
public:
	_kc_hidden ECAuthSession(const char *addr, ECSESSIONID, ECDatabaseFactory *, ECSessionManager *, unsigned int caps);
	_kc_hidden virtual ~ECAuthSession(void);
	_kc_hidden ECRESULT ValidateUserLogon(const char *name, const char *pass, const char *imp_user);
	_kc_hidden ECRESULT ValidateUserSocket(int socket, const char *name, const char *imp_user);
	_kc_hidden ECRESULT ValidateUserCertificate(struct soap *, const char *name, const char *imp_user);
	_kc_hidden ECRESULT ValidateSSOData(struct soap *, const char *name, const char *imp_user, const char *cl_ver, const char *cl_app, const char *cl_app_ver, const char *cl_app_misc, const struct xsd__base64Binary *input, struct xsd__base64Binary **output);
	_kc_hidden virtual ECRESULT CreateECSession(ECSESSIONGROUPID, const std::string &cl_ver, const std::string &cl_app, const std::string &cl_app_ver, const std::string &cl_app_misc, ECSESSIONID *retid, ECSession **ret);
	_kc_hidden size_t GetObjectSize(void);
	_kc_hidden virtual ECUserManagement *GetUserManagement(void) const override final { return m_lpUserManagement.get(); }

protected:
	unsigned int m_ulUserID = 0;
	unsigned int m_ulImpersonatorID = 0; // The ID of the user whose credentials were used to login when using impersonation
	bool m_bValidated = false;
	AUTHMETHOD m_ulValidationMethod = METHOD_NONE;
	int m_ulConnectingPid = 0;

private:
	/* SSO */
	_kc_hidden ECRESULT ValidateSSOData_NTLM(struct soap *, const char *name, const char *cl_ver, const char *cl_app, const char *cl_app_ver, const char *cl_app_misc, const struct xsd__base64Binary *input, struct xsd__base64Binary **out);
	_kc_hidden ECRESULT ValidateSSOData_KRB5(struct soap *, const char *name, const char *cl_ver, const char *cl_app, const char *cl_app_ver, const char *cl_app_misc, const struct xsd__base64Binary *input, struct xsd__base64Binary **out);
	_kc_hidden ECRESULT ValidateSSOData_KCOIDC(struct soap *, const char *name, const char *cl_ver, const char *cl_app, const char *cl_app_ver, const char *cl_app_misc, const struct xsd__base64Binary *input, struct xsd__base64Binary **output);
#ifdef HAVE_GSSAPI
	_kc_hidden ECRESULT LogKRB5Error(const char *msg, OM_uint32 major, OM_uint32 minor);
#endif
	_kc_hidden ECRESULT ProcessImpersonation(const char *imp_user);

	/* NTLM */
	pid_t m_NTLM_pid = -1;
	int m_NTLM_stdin[2], m_NTLM_stdout[2], m_NTLM_stderr[2];
	int m_stdin = -1, m_stdout = -1, m_stderr = -1; /* shortcuts to the above */

#ifdef HAVE_GSSAPI
	/* KRB5 */
	gss_cred_id_t m_gssServerCreds;
	gss_ctx_id_t m_gssContext;
#endif
	std::unique_ptr<ECUserManagement> m_lpUserManagement;
};

} /* namespace */

#endif // #ifndef ECSESSION
