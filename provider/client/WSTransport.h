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

#ifndef WSTRANSPORT_H
#define WSTRANSPORT_H

#include <mapi.h>
#include <mapispi.h>

#include <map>
#include <pthread.h>

#include "Zarafa.h"
#include "ECMAPIProp.h"
#include "soapZarafaCmdProxy.h"

#include "ZarafaCode.h"

#include "WSStoreTableView.h"
//#include "WSTableOutGoingQueue.h"
#include "WSMAPIFolderOps.h"
#include "WSMAPIPropStorage.h"
#include "ECParentStorage.h"
#include "ECABLogon.h"
#include "ECICS.h"
#include <ECCache.h>

class utf8string;
class WSMessageStreamExporter;
class WSMessageStreamImporter;

typedef HRESULT (*SESSIONRELOADCALLBACK)(void *lpParam, ECSESSIONID newSessionId);
typedef std::map<ULONG, std::pair<void *, SESSIONRELOADCALLBACK> > SESSIONRELOADLIST;

class ECsResolveResult : public ECsCacheEntry {
public:
	HRESULT	hr;
	std::string serverPath;
	bool isPeer;
};
typedef std::map<std::string, ECsResolveResult> ECMapResolveResults;

// Array offsets for Receive folder table
enum
{
    RFT_ROWID,
    RFT_INST_KEY,
    RFT_ENTRYID,
    RFT_RECORD_KEY,
    RFT_MSG_CLASS,
    NUM_RFT_PROPS
};

class WSTransport : public ECUnknown {
protected:
	WSTransport(ULONG ulUIFlags);
	virtual ~WSTransport();

public:
	static HRESULT Create(ULONG ulUIFlags, WSTransport **lppTransport);

	virtual HRESULT QueryInterface(REFIID refiid, void **lppInterface);

	static	HRESULT	HrOpenTransport(LPMAPISUP lpMAPISup, WSTransport **lppTransport, BOOL bOffline = FALSE);

	virtual HRESULT HrLogon(const sGlobalProfileProps &sProfileProps);
	virtual HRESULT HrReLogon();
	virtual HRESULT HrClone(WSTransport **lppTransport);

	virtual HRESULT HrLogOff();
	virtual HRESULT HrSetRecvTimeout(unsigned int ulSeconds);

	virtual HRESULT CreateAndLogonAlternate(LPCSTR szServer, WSTransport **lppTransport) const;
	virtual HRESULT CloneAndRelogon(WSTransport **lppTransport) const;

	virtual HRESULT HrGetStore(ULONG cbMasterID, LPENTRYID lpMasterID, ULONG* lppcbStoreID, LPENTRYID* lppStoreID, ULONG* lppcbRootID, LPENTRYID* lppRootID, std::string *lpstrRedirServer = NULL);
	virtual HRESULT HrGetStoreName(ULONG cbStoreID, LPENTRYID lpStoreID, ULONG ulFlags, LPTSTR *lppszStoreName);
	virtual HRESULT HrGetStoreType(ULONG cbStoreID, LPENTRYID lpStoreID, ULONG *lpulStoreType);
	virtual HRESULT HrGetPublicStore(ULONG ulFlags, ULONG* lpcbStoreID, LPENTRYID* lppStoreID, std::string *lpstrRedirServer = NULL);

	// Check item exist with flags
	virtual HRESULT HrCheckExistObject(ULONG cbEntryID, LPENTRYID lpEntryID, ULONG ulFlags);

	// Interface to get/set properties
	virtual HRESULT HrOpenPropStorage(ULONG cbParentEntryID, LPENTRYID lpParentEntryID, ULONG cbEntryID, LPENTRYID lpEntryID, ULONG ulFlags, IECPropStorage **lppPropStorage);
	virtual HRESULT HrOpenParentStorage(ECGenericProp *lpParentObject, ULONG ulUniqueId, ULONG ulObjId, IECPropStorage *lpServerStorage, IECPropStorage **lppPropStorage);
	virtual HRESULT HrOpenABPropStorage(ULONG cbEntryID, LPENTRYID lpEntryID, IECPropStorage **lppPropStorage);

	// Interface for folder operations (create/delete)
	virtual HRESULT HrOpenFolderOps(ULONG cbEntryID, LPENTRYID lpEntryID, WSMAPIFolderOps **lppFolderOps);
	virtual HRESULT HrExportMessageChangesAsStream(ULONG ulFlags, ICSCHANGE *lpChanges, ULONG ulStart, ULONG ulChanges, LPSPropTagArray lpsProps, WSMessageStreamExporter **lppsStreamExporter);
	virtual HRESULT HrGetMessageStreamImporter(ULONG ulFlags, ULONG ulSyncId, ULONG cbEntryID, LPENTRYID lpEntryID, ULONG cbFolderEntryID, LPENTRYID lpFolderEntryID, bool bNewMessage, LPSPropValue lpConflictItems, WSMessageStreamImporter **lppStreamImporter);

	// Interface for table operations
	virtual HRESULT HrOpenTableOps(ULONG ulType, ULONG ulFlags, ULONG cbEntryID, LPENTRYID lpEntryID, ECMsgStore *lpMsgStore, WSTableView **lppTableOps);

	virtual HRESULT HrOpenABTableOps(ULONG ulType, ULONG ulFlags, ULONG cbEntryID, LPENTRYID lpEntryID, ECABLogon* lpABLogon, WSTableView **lppTableOps);
	virtual HRESULT HrOpenMailBoxTableOps(ULONG ulFlags, ECMsgStore *lpMsgStore, WSTableView **lppTableOps);

	//Interface for outgoigqueue
	virtual HRESULT HrOpenTableOutGoingQueueOps(ULONG cbStoreEntryID, LPENTRYID lpStoreEntryID, ECMsgStore *lpMsgStore, WSTableOutGoingQueue **lppTableOutGoingQueueOps);

	// Delete objects
	virtual HRESULT HrDeleteObjects(ULONG ulFlags, LPENTRYLIST lpMsgList, ULONG ulSyncId);

	// Notification
	virtual HRESULT HrSubscribe(ULONG cbKey, LPBYTE lpKey, ULONG ulConnection, ULONG ulEventMask);
	virtual HRESULT HrSubscribe(ULONG ulSyncId, ULONG ulChangeId, ULONG ulConnection, ULONG ulEventMask);
	virtual HRESULT HrSubscribeMulti(const ECLISTSYNCADVISE &lstSyncAdvises, ULONG ulEventMask);
	virtual HRESULT HrUnSubscribe(ULONG ulConnection);
	virtual HRESULT HrUnSubscribeMulti(const ECLISTCONNECTION &lstConnections);
	virtual	HRESULT HrNotify(LPNOTIFICATION lpNotification);

	// Named properties
	virtual HRESULT HrGetIDsFromNames(LPMAPINAMEID *lppPropNamesUnresolved, ULONG cUnresolved, ULONG ulFlags, ULONG **lpServerIDs);

	virtual HRESULT HrGetNamesFromIDs(LPSPropTagArray lpsPropTags, LPMAPINAMEID ** lpppNames, ULONG *cResolved);
	
	// ReceiveFolder
	virtual HRESULT HrGetReceiveFolder(ULONG cbStoreEntryID, LPENTRYID lpStoreEntryID, const utf8string &strMessageClass, ULONG* lpcbEntryID, LPENTRYID* lppEntryID, utf8string *lpstrExplicitClass);
	virtual HRESULT HrSetReceiveFolder(ULONG cbStoreEntryID, LPENTRYID lpStoreEntryID, const utf8string &strMessageClass, ULONG cbEntryID, LPENTRYID lpEntryID);
	virtual HRESULT HrGetReceiveFolderTable(ULONG ulFlags, ULONG cbStoreEntryID, LPENTRYID lpStoreEntryID, LPSRowSet* lppsRowSet);

	// Read / Unread
	virtual HRESULT HrSetReadFlag(ULONG cbEntryID, LPENTRYID lpEntryID, ULONG ulFlags, ULONG ulSyncId);

	// Add message into the Outgoing Queue
	virtual HRESULT HrSubmitMessage(ULONG cbMessageID, LPENTRYID lpMessageID, ULONG ulFlags);

	// Outgoing Queue Finished message
	virtual HRESULT HrFinishedMessage(ULONG cbEntryID, LPENTRYID lpEntryID, ULONG ulFlags);
	virtual HRESULT HrAbortSubmit(ULONG cbEntryID, LPENTRYID lpEntryID);
	virtual HRESULT HrIsMessageInQueue(ULONG cbEntryID, LPENTRYID lpEntryID);

	// Get user information
	virtual HRESULT HrResolveStore(LPGUID lpGuid, ULONG *lpulUserID, ULONG* lpcbStoreID, LPENTRYID* lppStoreID);
	virtual HRESULT HrResolveUserStore(const utf8string &strUserName, ULONG ulFlags, ULONG *lpulUserID, ULONG* lpcbStoreID, LPENTRYID* lppStoreID, std::string *lpstrRedirServer = NULL);
	virtual HRESULT HrResolveTypedStore(const utf8string &strUserName, ULONG ulStoreType, ULONG* lpcbStoreID, LPENTRYID* lppStoreID);

	// IECServiceAdmin functions
	virtual HRESULT HrCreateUser(LPECUSER lpECUser, ULONG ulFlags, ULONG *lpcbUserId, LPENTRYID *lppUserId);
	virtual HRESULT HrDeleteUser(ULONG cbUserId, LPENTRYID lpUserId);
	virtual HRESULT HrSetUser(LPECUSER lpECUser, ULONG ulFlags);
	virtual HRESULT HrGetUser(ULONG cbUserID, LPENTRYID lpUserID, ULONG ulFlags, LPECUSER *lpECUser);

	virtual HRESULT HrCreateStore(ULONG ulStoreType, ULONG cbUserId, LPENTRYID lpUserId, ULONG cbStoreID, LPENTRYID lpStoreID, ULONG cbRootID, LPENTRYID lpRootID, ULONG ulFLags);
	virtual HRESULT HrHookStore(ULONG ulStoreType, ULONG cbUserId, LPENTRYID lpUserId, LPGUID lpGuid, ULONG ulSyncId);
	virtual HRESULT HrUnhookStore(ULONG ulStoreType, ULONG cbUserId, LPENTRYID lpUserId, ULONG ulSyncId);
	virtual HRESULT HrRemoveStore(LPGUID lpGuid, ULONG ulSyncId);

	virtual HRESULT HrGetUserList(ULONG cbCompanyId, LPENTRYID lpCompanyId, ULONG ulFlags, ULONG *lpcUsers, LPECUSER* lppsUsers);
	virtual HRESULT HrResolveUserName(LPCTSTR lpszUserName, ULONG ulFlags, ULONG *lpcbUserId, LPENTRYID *lppUserId);

	virtual HRESULT HrGetSendAsList(ULONG cbUserId, LPENTRYID lpUserId, ULONG ulFlags, ULONG *lpcSenders, LPECUSER *lppSenders);
	virtual HRESULT HrAddSendAsUser(ULONG cbUserId, LPENTRYID lpUserId, ULONG cbSenderId, LPENTRYID lpSenderId);
	virtual HRESULT HrDelSendAsUser(ULONG cbUserId, LPENTRYID lpUserId, ULONG cbSenderId, LPENTRYID lpSenderId);
	
	virtual HRESULT HrRemoveAllObjects(ULONG cbUserId, LPENTRYID lpUserId);

	virtual HRESULT HrGetUserClientUpdateStatus(ULONG cbUserId, LPENTRYID lpUserId, ULONG ulFlags, LPECUSERCLIENTUPDATESTATUS *lppECUCUS);

	// Quota
	virtual HRESULT GetQuota(ULONG cbUserId, LPENTRYID lpUserId, bool bGetUserDefault, LPECQUOTA* lppsQuota);
	virtual HRESULT SetQuota(ULONG cbUserId, LPENTRYID lpUserId, LPECQUOTA lpsQuota);
	virtual HRESULT AddQuotaRecipient(ULONG cbCompanyId, LPENTRYID lpCompanyId, ULONG cbRecipientId, LPENTRYID lpRecipientId, ULONG ulType);
	virtual HRESULT DeleteQuotaRecipient(ULONG cbCompanyId, LPENTRYID lpCmopanyId, ULONG cbRecipientId, LPENTRYID lpRecipientId, ULONG ulType);
	virtual HRESULT GetQuotaRecipients(ULONG cbUserId, LPENTRYID lpUserId, ULONG ulFlags, ULONG *lpcUsers, LPECUSER *lppsUsers);
	virtual HRESULT GetQuotaStatus(ULONG cbUserId, LPENTRYID lpUserId, LPECQUOTASTATUS* lppsQuotaStatus);

	virtual HRESULT HrPurgeSoftDelete(ULONG ulDays);
	virtual HRESULT HrPurgeCache(ULONG ulFlags);
	virtual HRESULT HrPurgeDeferredUpdates(ULONG *lpulRemaining);

	// MultiServer
	virtual HRESULT HrResolvePseudoUrl(const char *lpszPseudoUrl, char **lppszServerPath, bool *lpbIsPeer);
	virtual HRESULT HrGetServerDetails(LPECSVRNAMELIST lpServerNameList, ULONG ulFlags, LPECSERVERLIST* lppsServerList);

	// IECServiceAdmin group functions
	virtual HRESULT HrResolveGroupName(LPCTSTR lpszGroupName, ULONG ulFlags, ULONG *lpcbGroupId, LPENTRYID *lppGroupId);

	virtual HRESULT HrCreateGroup(LPECGROUP lpECGroup, ULONG ulFlags, ULONG *lpcbGroupId, LPENTRYID *lppGroupId);
	virtual HRESULT HrSetGroup(LPECGROUP lpECGroup, ULONG ulFlags);
	virtual HRESULT HrGetGroup(ULONG cbGroupID, LPENTRYID lpGroupID, ULONG ulFlags, LPECGROUP *lppECGroup);
	virtual HRESULT HrDeleteGroup(ULONG cbGroupId, LPENTRYID lpGroupId);
	virtual HRESULT HrGetGroupList(ULONG cbCompanyId, LPENTRYID lpCompanyId, ULONG ulFlags, ULONG *lpcGroups, LPECGROUP *lppsGroups);

	// IECServiceAdmin Group and user functions
	virtual HRESULT HrDeleteGroupUser(ULONG cbGroupId, LPENTRYID lpGroupId, ULONG cbUserId, LPENTRYID lpUserId);
	virtual HRESULT HrAddGroupUser(ULONG cbGroupId, LPENTRYID lpGroupId, ULONG cbUserId, LPENTRYID lpUserId);
	virtual HRESULT HrGetUserListOfGroup(ULONG cbGroupId, LPENTRYID lpGroupId, ULONG ulFlags, ULONG *lpcUsers, LPECUSER *lppsUsers);
	virtual HRESULT HrGetGroupListOfUser(ULONG cbUserId, LPENTRYID lpUserId, ULONG ulFlags, ULONG *lpcGroups, LPECGROUP *lppsGroups);

	// IECServiceAdmin company functions
	virtual HRESULT HrCreateCompany(LPECCOMPANY lpECCompany, ULONG ulFlags, ULONG *lpcbCompanyId, LPENTRYID *lppCompanyId);
	virtual HRESULT HrDeleteCompany(ULONG cbCompanyId, LPENTRYID lpCompanyId);
	virtual HRESULT HrSetCompany(LPECCOMPANY lpECCompany, ULONG ulFlags);
	virtual HRESULT HrGetCompany(ULONG cbCompanyId, LPENTRYID lpCompanyId, ULONG ulFlags, LPECCOMPANY *lppECCompany);
	virtual HRESULT HrResolveCompanyName(LPCTSTR lpszCompanyName, ULONG ulFlags, ULONG *lpcbCompanyId, LPENTRYID *lppCompanyId);
	virtual HRESULT HrGetCompanyList(ULONG ulFlags, ULONG *lpcCompanies, LPECCOMPANY *lppsCompanies);
	virtual HRESULT HrAddCompanyToRemoteViewList(ULONG cbSetCompanyId, LPENTRYID lpSetCompanyId, ULONG cbCompanyId, LPENTRYID lpCompanyId);
	virtual HRESULT HrDelCompanyFromRemoteViewList(ULONG cbSetCompanyId, LPENTRYID lpSetCompanyId, ULONG cbCompanyId, LPENTRYID lpCompanyId);
	virtual HRESULT HrGetRemoteViewList(ULONG cbCompanyId, LPENTRYID lpCompanyId, ULONG ulFlags, ULONG *lpcCompanies, LPECCOMPANY *lppsCompanies);
	virtual HRESULT HrAddUserToRemoteAdminList(ULONG cbUserId, LPENTRYID lpUserId, ULONG cbCompanyId, LPENTRYID lpCompanyId);
	virtual HRESULT HrDelUserFromRemoteAdminList(ULONG cbUserId, LPENTRYID lpUserId, ULONG cbCompanyId, LPENTRYID lpCompanyId);
	virtual HRESULT HrGetRemoteAdminList(ULONG cbCompanyId, LPENTRYID lpCompanyId, ULONG ulFlags, ULONG *lpcUsers, LPECUSER *lppsUsers);
	
	// IECServiceAdmin company and user functions

	// Get the object rights
	virtual HRESULT HrGetPermissionRules(int ulType, ULONG cbEntryID, LPENTRYID lpEntryID, ULONG* lpcPermissions, LPECPERMISSION* lppECPermissions);

	// Set the object rights
	virtual HRESULT HrSetPermissionRules(ULONG cbEntryID, LPENTRYID lpEntryID, ULONG cPermissions, LPECPERMISSION lpECPermissions);

	// Get owner information
	virtual HRESULT HrGetOwner(ULONG cbEntryID, LPENTRYID lpEntryID, ULONG *lpcbOwnerId, LPENTRYID *lppOwnerId);

	//Addressbook function
	virtual HRESULT HrResolveNames(LPSPropTagArray lpPropTagArray, ULONG ulFlags, LPADRLIST lpAdrList, LPFlagList lpFlagList);

	virtual HRESULT HrSyncUsers(ULONG cbCompanyId, LPENTRYID lpCompanyId);


	// Incremental Change Synchronization
	virtual HRESULT HrGetChanges(SBinary sSourceKeyFolder, ULONG ulSyncId, ULONG ulChangeId, ULONG ulSyncType, ULONG ulFlags, LPSRestriction lpRestrict, ULONG *lpulMaxChangeId, ULONG* lpcChanges, ICSCHANGE **lpsChanges);
	virtual HRESULT HrSetSyncStatus(SBinary sSourceKeyFolder, ULONG ulSyncId, ULONG ulChangeId, ULONG ulSyncType, ULONG ulFlags, ULONG* lpulSyncId);

	virtual HRESULT HrEntryIDFromSourceKey(ULONG cbStoreID, LPENTRYID lpStoreID, ULONG ulFolderSourceKeySize, BYTE * lpFolderSourceKey, ULONG ulMessageSourceKeySize, BYTE * lpMessageSourceKey, ULONG * lpcbEntryID, LPENTRYID * lppEntryID);
	virtual HRESULT HrGetSyncStates(const ECLISTSYNCID &lstSyncId, ECLISTSYNCSTATE *lplstSyncState);

	virtual const char* GetServerName();
	virtual bool IsConnected();

	/* multi store table functions */
	virtual HRESULT HrOpenMultiStoreTable(LPENTRYLIST lpMsgList, ULONG ulFlags, ULONG cbEntryID, LPENTRYID lpEntryID, ECMsgStore *lpMsgStore, WSTableView **lppTableOps);

	/* statistics tables (system, threads, users), ulTableType is ZarafaProto.h TABLETYPE_STATS_... */
	/* userstores table TABLETYPE_USERSTORE */
	virtual HRESULT HrOpenMiscTable(ULONG ulTableType, ULONG ulFlags, ULONG cbEntryID, LPENTRYID lpEntryID, ECMsgStore *lpMsgStore, WSTableView **lppTableView);

	/* Message locking */
	virtual HRESULT HrSetLockState(ULONG cbEntryID, LPENTRYID lpEntryID, bool bLocked);

	// License information

	/* Exchange encrypted auth info with zarafa-licensed */
	virtual HRESULT HrLicenseAuth(unsigned char *lpData, unsigned int ulSize, unsigned char **lppResponseData, unsigned int *lpulSize);

	/* Get license capabilities from zarafa-licensed */
	virtual HRESULT HrLicenseCapa(unsigned int ulServiceType, char ***lppszCapas, unsigned int *lpulSize);
	
	virtual HRESULT HrLicenseUsers(unsigned int ulServiceType, unsigned int *lpulUsers);

	/* expose capabilities */
	virtual HRESULT HrCheckCapabilityFlags(ULONG ulFlags, BOOL *lpbResult);
	
	/* Get flags received on logon */
	virtual HRESULT GetLicenseFlags(unsigned long long *lpllFlags);

	/* Test protocol */
	virtual HRESULT HrTestPerform(char *szCommand, unsigned int ulArgs, char *lpszArgs[]);
	virtual HRESULT HrTestSet(char *szName, char *szValue);
	virtual HRESULT HrTestGet(char *szName, char **szValue);

	/* Return Session information */
	virtual HRESULT HrGetSessionId(ECSESSIONID *lpSessionId, ECSESSIONGROUPID *lpSessionGroupId);
	
	/* Get profile properties (connect info) */
	virtual sGlobalProfileProps GetProfileProps();
	
	/* Get the server GUID obtained at logon */
	virtual HRESULT GetServerGUID(LPGUID lpsServerGuid);

	/* These are called by other WS* classes to register themselves for session changes */
	virtual HRESULT AddSessionReloadCallback(void *lpParam, SESSIONRELOADCALLBACK callback, ULONG * lpulId);
	virtual HRESULT RemoveSessionReloadCallback(ULONG ulId);

	/* notifications */
	virtual HRESULT HrGetNotify(struct notificationArray **lppsArrayNotifications);
	virtual HRESULT HrCancelIO();

private:
	static SOAP_SOCKET RefuseConnect(struct soap*, const char*, const char*, int);

	virtual HRESULT LockSoap();
	virtual HRESULT UnLockSoap();

	//TODO: Move this function to the right file
	static ECRESULT TrySSOLogon(ZarafaCmd* lpCmd, LPCSTR szServer, utf8string strUsername, unsigned int ulCapabilities, ECSESSIONGROUPID ecSessionGroupId, char *szAppName, ECSESSIONID* lpSessionId, unsigned int* lpulServerCapabilities, unsigned long long *lpllFlags, LPGUID lpsServerGuid);

	// Returns name of calling application (eg 'program.exe' or 'httpd')
	std::string GetAppName();

protected:
	ZarafaCmd*		m_lpCmd;
	pthread_mutex_t m_hDataLock;
	ECSESSIONID		m_ecSessionId;
	ECSESSIONGROUPID m_ecSessionGroupId;
	SESSIONRELOADLIST m_mapSessionReload;
	pthread_mutex_t m_mutexSessionReload;
	unsigned int	m_ulReloadId;
	unsigned int	m_ulServerCapabilities;
	unsigned long long m_llFlags;	// license flags
	ULONG			m_ulUIFlags;	// UI flags for logon
	sGlobalProfileProps m_sProfileProps;
	std::string		m_strAppName;
	GUID			m_sServerGuid;

private:
	pthread_mutex_t					m_ResolveResultCacheMutex;
	ECCache<ECMapResolveResults>	m_ResolveResultCache;

friend class WSMessageStreamExporter;
friend class WSMessageStreamImporter;
};

#endif // WSTRANSPORT_H
