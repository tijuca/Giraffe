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
#ifndef IECINTERFACES_HPP
#define IECINTERFACES_HPP 1

#include <edkmdb.h>
#include <kopano/ECDefs.h>
#include <kopano/ECGuid.h>
#include <kopano/platform.h>
#include <mapidefs.h>

namespace KC {

class IECChangeAdviseSink : public virtual IUnknown {
	public:
	virtual ULONG OnNotify(ULONG ulFLags, LPENTRYLIST lpEntryList) = 0;
};

/**
 * IECChangeAdvisor: Interface for registering change notifications on folders.
 */
class IECChangeAdvisor : public virtual IUnknown {
	public:
	virtual HRESULT GetLastError(HRESULT hResult, ULONG ulFlags, LPMAPIERROR *lppMAPIError) = 0;

	/**
	 * Configure the change change advisor based on a stream.
	 *
	 * @param[in]	lpStream
	 *					The stream containing the state of the state of the change advisor. This stream
	 *					is obtained by a call to UpdateState.
	 *					If lpStream is NULL, an empty state is assumed.
	 * @param[in]	lpGuid
	 *					Unused. Set to NULL.
	 * @param[in]	lpAdviseSink
	 *					The advise sink that will receive the change notifications.
	 * @param[in]	ulFlags
	 *					- SYNC_CATCHUP	Update the internal state, but don't perform any operations
	 *									on the server.
	 */
	virtual HRESULT Config(LPSTREAM lpStream, LPGUID lpGUID, IECChangeAdviseSink *lpAdviseSink, ULONG ulFlags) = 0;

	/**
	 * Store the current internal state in the stream pointed to by lpStream.
	 *
	 * @param[in]	lpStream
	 *					The stream in which the current state will be stored.
	 */
	virtual HRESULT UpdateState(LPSTREAM lpStream) = 0;

	/**
	 * Register one or more folders for change notifications through this change advisor.
	 *
	 * @param[in]	lpEntryList
	 *					A list of keys specifying the folders to monitor. A key is an 8 byte
	 *					block of data. The first 4 bytes specify the sync id of the folder to
	 *					monitor. The second 4 bytes apecify the change id of the folder to monitor.
	 *					Use the SSyncState structure to easily create and access this data.
	 */
	virtual HRESULT AddKeys(LPENTRYLIST lpEntryList) = 0;

	/**
	 * Unregister one or more folder for change notifications.
	 *
	 * @param[in]	lpEntryList
	 *					A list of keys specifying the folders to monitor. See AddKeys for
	 *					information about the key format.
	 */
	virtual HRESULT RemoveKeys(LPENTRYLIST lpEntryList) = 0;

	/**
	 * Check if the change advisor is monitoring the folder specified by a particular sync id.
	 *
	 * @param[in]	ulSyncId
	 *					The sync id of the folder.
	 * @return hrSuccess if the folder is being monitored.
	 */
	virtual HRESULT IsMonitoringSyncId(ULONG ulSyncId) = 0;

	/**
	 * Update the changeid for a particular syncid.
	 *
	 * This is used to update the state of the changeadvisor. This is also vital information
	 * when a reconnect is required.
	 *
	 * @param[in]	ulSyncId
	 *					The syncid for which to update the changeid.
	 * @param[in]	ulChangeId
	 *					The new changeid for the specified syncid.
	 */
	virtual HRESULT UpdateSyncState(ULONG ulSyncId, ULONG ulChangeId) = 0;
};

class IECExchangeModifyTable : public virtual IExchangeModifyTable {
	public:
	virtual HRESULT DisablePushToServer() = 0;
};

class IECImportAddressbookChanges;

class IECExportAddressbookChanges : public virtual IUnknown {
	public:
	virtual HRESULT Config(LPSTREAM lpState, ULONG ulFlags, IECImportAddressbookChanges *lpCollector) = 0;
	virtual HRESULT Synchronize(ULONG *lpulSteps, ULONG *lpulProgress) = 0;
	virtual HRESULT UpdateState(LPSTREAM lpState) = 0;
};

class ECLogger;

class IECExportChanges : public IExchangeExportChanges {
	public:
	virtual HRESULT ConfigSelective(ULONG ulPropTag, LPENTRYLIST lpEntries, LPENTRYLIST lpParents, ULONG ulFlags, LPUNKNOWN lpCollector, LPSPropTagArray lpIncludeProps, LPSPropTagArray lpExcludeProps, ULONG ulBufferSize) = 0;
	virtual HRESULT GetChangeCount(ULONG *lpcChanges) = 0;
	virtual HRESULT SetMessageInterface(REFIID refiid) = 0;
	virtual HRESULT SetLogger(ECLogger *lpLogger) = 0;
};

class IECImportAddressbookChanges : public IUnknown {
	public:
	virtual HRESULT GetLastError(HRESULT hr, ULONG ulFlags, LPMAPIERROR *lppMAPIError) = 0;
	virtual HRESULT Config(LPSTREAM lpState, ULONG ulFlags) = 0;
	virtual HRESULT UpdateState(LPSTREAM lpState) = 0;
	virtual HRESULT ImportABChange(ULONG type, ULONG cbObjId, LPENTRYID lpObjId) = 0;
	virtual HRESULT ImportABDeletion(ULONG type, ULONG cbObjId, LPENTRYID lpObjId) = 0;
};

class IECImportContentsChanges : public IExchangeImportContentsChanges {
	public:
	virtual HRESULT ConfigForConversionStream(LPSTREAM lpStream, ULONG ulFlags, ULONG cValuesConversion, LPSPropValue lpPropArrayConversion) = 0;
	virtual HRESULT ImportMessageChangeAsAStream(ULONG cpvalChanges, LPSPropValue ppvalChanges, ULONG ulFlags, LPSTREAM *lppstream) = 0;
};

class IECImportHierarchyChanges : public IExchangeImportHierarchyChanges {
	public:
	virtual HRESULT ImportFolderChangeEx(ULONG cValues, LPSPropValue lpPropArray, BOOL fNew) = 0;
};

class IECLicense : public virtual IUnknown {
	public:
	virtual HRESULT LicenseAuth(unsigned char *lpData, unsigned int ulSize, unsigned char **lpAuthResponse, unsigned int *lpulResponseSize) = 0;
	virtual HRESULT LicenseCapa(unsigned int ulServiceType, char ***lppszCapabilities, unsigned int *lpulCapabilities) = 0;
	virtual HRESULT LicenseUsers(unsigned int ulServiceType, unsigned int *ulUsers) = 0;
};

class IECMultiStoreTable : public virtual IUnknown {
	public:
	/* ulFlags is currently unused */
	virtual HRESULT OpenMultiStoreTable(LPENTRYLIST lpMsgList, ULONG ulFlags, LPMAPITABLE *lppTable) = 0;
};

class IECSecurity : public virtual IUnknown {
	public:
	virtual HRESULT GetOwner(ULONG *lpcbOwner, LPENTRYID *lppOwner) = 0;
	virtual HRESULT GetUserList(ULONG cbCompanyId, LPENTRYID lpCompanyId, ULONG ulFlags, ULONG *lpcUsers, ECUSER **lppsUsers) = 0;
	virtual HRESULT GetGroupList(ULONG cbCompanyId, LPENTRYID lpCompanyId, ULONG ulFlags, ULONG *lpcGroups, ECGROUP **lppsGroups) = 0;
	virtual HRESULT GetCompanyList(ULONG ulFlags, ULONG *lpcCompanies, ECCOMPANY **lppsCompanies) = 0;
	virtual HRESULT GetPermissionRules(int ulType, ULONG* lpcPermissions, ECPERMISSION **lppECPermissions) = 0;
	virtual HRESULT SetPermissionRules(ULONG cPermissions, ECPERMISSION *lpECPermissions) = 0;
};

class IECServiceAdmin : public virtual IUnknown {
	public:
	/* Create/Delete stores */
	virtual HRESULT CreateStore(ULONG ulStoreType, ULONG cbUserId, LPENTRYID lpUserId, ULONG *lpcbStoreId, LPENTRYID *lppStoreId, ULONG *lpcbRootId, LPENTRYID *lppRootId) = 0;
	virtual HRESULT CreateEmptyStore(ULONG ulStoreType, ULONG cbUserId, LPENTRYID lpUserId, ULONG ulFlags, ULONG *lpcbStoreId, LPENTRYID *lppStoreId, ULONG *lpcbRootId, LPENTRYID *lppRootId) = 0;
	virtual HRESULT ResolveStore(LPGUID lpGuid, ULONG *lpulUserID, ULONG *lpcbStoreID, LPENTRYID *lppStoreID) = 0;
	virtual HRESULT HookStore(ULONG ulStoreType, ULONG cbUserId, LPENTRYID lpUserId, LPGUID lpGuid) = 0;
	virtual HRESULT UnhookStore(ULONG ulStoreType, ULONG cbUserId, LPENTRYID lpUserId) = 0;
	virtual HRESULT RemoveStore(LPGUID lpGuid) = 0;

	/* User functions */
	virtual HRESULT CreateUser(ECUSER *lpECUser, ULONG ulFlags, ULONG *lpcbUserId, LPENTRYID *lppUserId) = 0;
	virtual HRESULT DeleteUser(ULONG cbUserId, LPENTRYID lpUserId) = 0;
	virtual HRESULT SetUser(ECUSER *lpECUser, ULONG ulFlags) = 0;
	virtual HRESULT GetUser(ULONG cbUserId, LPENTRYID lpUserId, ULONG ulFlags, ECUSER **lppECUser) = 0;
	virtual HRESULT ResolveUserName(LPCTSTR lpszUserName, ULONG ulFlags, ULONG *lpcbUserId, LPENTRYID *lppUserId) = 0;
	virtual HRESULT GetUserList(ULONG cbCompanyId, LPENTRYID lpCompanyId, ULONG ulFlags, ULONG *lpcUsers, ECUSER **lppsUsers) = 0;
	virtual HRESULT GetSendAsList(ULONG cbUserId, LPENTRYID lpUserId, ULONG ulFlags, ULONG *lpcSenders, ECUSER **lppSenders) = 0;
	virtual HRESULT AddSendAsUser(ULONG cbUserId, LPENTRYID lpUserId, ULONG cbSenderId, LPENTRYID lpSenderId) = 0;
	virtual HRESULT DelSendAsUser(ULONG cbUserId, LPENTRYID lpUserId, ULONG cbSenderId, LPENTRYID lpSenderId) = 0;
	virtual HRESULT GetUserClientUpdateStatus(ULONG cbUserId, LPENTRYID lpUserId, ULONG ulFlags, ECUSERCLIENTUPDATESTATUS **lppECUCUS) = 0;

	/* Remove all users EXCEPT the passed user */
	virtual HRESULT RemoveAllObjects(ULONG cbUserId, LPENTRYID lpUserId) = 0;

	/* Group functions */
	virtual HRESULT CreateGroup(ECGROUP *lpECGroup, ULONG ulFlags, ULONG *lpcbGroupId, LPENTRYID *lppGroupId) = 0;
	virtual HRESULT DeleteGroup(ULONG cbGroupId, LPENTRYID lpGroupId) = 0;
	virtual HRESULT SetGroup(ECGROUP *lpECGroup, ULONG ulFlags) = 0;
	virtual HRESULT GetGroup(ULONG cbGroupId, LPENTRYID lpGroupId, ULONG ulFlags, ECGROUP **lppECGroup) = 0;
	virtual HRESULT ResolveGroupName(LPCTSTR lpszGroupName, ULONG ulFlags, ULONG *lpcbGroupId, LPENTRYID *lppGroupId) = 0;
	virtual HRESULT GetGroupList(ULONG cbCompanyId, LPENTRYID lpCompanyId, ULONG ulFlags, ULONG *lpcGroups, ECGROUP **lppsGroups) = 0;
	virtual HRESULT DeleteGroupUser(ULONG cbGroupId, LPENTRYID lpGroupId, ULONG cbUserId, LPENTRYID lpUserId) = 0;
	virtual HRESULT AddGroupUser(ULONG cbGroupId, LPENTRYID lpGroupId, ULONG cbUserId, LPENTRYID lpUserId) = 0;
	virtual HRESULT GetUserListOfGroup(ULONG cbGroupId, LPENTRYID lpGroupId, ULONG ulFlags, ULONG *lpcUsers, ECUSER **lppsUsers) = 0;
	virtual HRESULT GetGroupListOfUser(ULONG cbUserId, LPENTRYID lpUserId, ULONG ulFlags, ULONG *lpcGroups, ECGROUP **lppsGroups) = 0;

	/* Company functions */
	virtual HRESULT CreateCompany(ECCOMPANY *lpECCompany, ULONG ulFlags, ULONG *lpcbCompanyId, LPENTRYID *lppCompanyId) = 0;
	virtual HRESULT DeleteCompany(ULONG cbCompanyId, LPENTRYID lpCompanyId) = 0;
	virtual HRESULT SetCompany(ECCOMPANY *lpECCompany, ULONG ulFlags) = 0;
	virtual HRESULT GetCompany(ULONG cbCompanyId, LPENTRYID lpCompanyId, ULONG ulFlags, ECCOMPANY **lppECCompany) = 0;
	virtual HRESULT ResolveCompanyName(LPCTSTR lpszCompanyName, ULONG ulFlags, ULONG *lpcbCompanyId, LPENTRYID *lppCompanyId) = 0;
	virtual HRESULT GetCompanyList(ULONG ulFlags, ULONG *lpcCompanies, ECCOMPANY **lppsCompanies) = 0;
	virtual HRESULT AddCompanyToRemoteViewList(ULONG cbSetCompanyId, LPENTRYID lpSetCompanyId, ULONG cbCompanyId, LPENTRYID lpCompanyId) = 0;
	virtual HRESULT DelCompanyFromRemoteViewList(ULONG cbSetCompanyId, LPENTRYID lpSetCompanyId, ULONG cbCompanyId, LPENTRYID lpCompanyId) = 0;
	virtual HRESULT GetRemoteViewList(ULONG cbCompanyId, LPENTRYID lpCompanyId, ULONG ulFlags, ULONG *lpcCompanies, ECCOMPANY **lppsCompanies) = 0;
	virtual HRESULT AddUserToRemoteAdminList(ULONG cbUserId, LPENTRYID lpUserId, ULONG cbCompanyId, LPENTRYID lpCompanyId) = 0;
	virtual HRESULT DelUserFromRemoteAdminList(ULONG cbUserId, LPENTRYID lpUserId, ULONG cbCompanyId, LPENTRYID lpCompanyId) = 0;
	virtual HRESULT GetRemoteAdminList(ULONG cbCompanyId, LPENTRYID lpCompanyId, ULONG ulFlags, ULONG *lpcUsers, ECUSER **lppsUsers) = 0;
	virtual HRESULT SyncUsers(ULONG cbCompanyId, LPENTRYID lpCOmpanyId) = 0;

	/* Quota functions */
	virtual HRESULT GetQuota(ULONG cbUserId, LPENTRYID lpUserId, bool bGetUserDefaultQuota, ECQUOTA **lppsQuota) = 0;
	virtual HRESULT SetQuota(ULONG cbUserId, LPENTRYID lpUserId, ECQUOTA *lpsQuota) = 0;
	virtual HRESULT AddQuotaRecipient(ULONG cbCompanyId, LPENTRYID lpCompanyId, ULONG cbRecipientId, LPENTRYID lpRecipientId, ULONG ulType) = 0;
	virtual HRESULT DeleteQuotaRecipient(ULONG cbCompanyId, LPENTRYID lpCmopanyId, ULONG cbRecipientId, LPENTRYID lpRecipientId, ULONG ulType) = 0;
	virtual HRESULT GetQuotaRecipients(ULONG cbUserId, LPENTRYID lpUserId, ULONG ulFlags, ULONG *lpcUsers, ECUSER **lppsUsers) = 0;
	virtual HRESULT GetQuotaStatus(ULONG cbUserId, LPENTRYID lpUserId, ECQUOTASTATUS **lppsQuotaStatus) = 0;
	virtual HRESULT PurgeSoftDelete(ULONG ulDays) = 0;
	virtual HRESULT PurgeCache(ULONG ulFlags) = 0;
	virtual HRESULT OpenUserStoresTable(ULONG ulFlags, LPMAPITABLE *lppTable) = 0;
	virtual HRESULT PurgeDeferredUpdates(ULONG *lpulDeferredRemaining) = 0;

	/* Multiserver functions */
	virtual HRESULT GetServerDetails(ECSVRNAMELIST *lpServerNameList, ULONG ulFlags, ECSERVERLIST **lppsServerList) = 0;
	virtual HRESULT ResolvePseudoUrl(const char *url, char **path, bool *ispeer) = 0;

	/* Public store function(s) */
	virtual HRESULT GetPublicStoreEntryID(ULONG ulFlags, ULONG *lpcbStoreID, LPENTRYID *lppStoreID) = 0;

	/* Archive store function(s) */
	virtual HRESULT GetArchiveStoreEntryID(LPCTSTR lpszUserName, LPCTSTR lpszServerName, ULONG ulFlags, ULONG *lpcbStoreID, LPENTRYID *lppStoreID) = 0;
	virtual HRESULT ResetFolderCount(ULONG cbEntryId, LPENTRYID lpEntryId, ULONG *lpulUpdates) = 0;
};

class IECSingleInstance : public virtual IUnknown {
	public:
	virtual HRESULT GetSingleInstanceId(ULONG *lpcbInstanceID, LPENTRYID *lppInstanceID) = 0;
	virtual HRESULT SetSingleInstanceId(ULONG cbInstanceID, LPENTRYID lpInstanceID) = 0;
};

// This is our special spooler interface
class IECSpooler : public virtual IUnknown {
	public:
	// Gets an IMAPITable containing all the outgoing messages on the server
	virtual HRESULT GetMasterOutgoingTable(ULONG ulFlags, IMAPITable **lppTable) = 0;

	// Removes a message from the master outgoing table
	virtual HRESULT DeleteFromMasterOutgoingTable(ULONG cbEntryID, const ENTRYID *lpEntryID, ULONG ulFlags) = 0;
};

class IECTestProtocol : public virtual IUnknown {
	public:
	virtual HRESULT TestPerform(const char *cmd, unsigned int argc, char **args) = 0;
	virtual HRESULT TestSet(const char *name, const char *value) = 0;
	virtual HRESULT TestGet(const char *name, char **value) = 0;
};

} /* namespace */

IID_OF2(KC::IECChangeAdvisor, IECChangeAdvisor)
IID_OF2(KC::IECServiceAdmin, IECServiceAdmin)
IID_OF2(KC::IECSingleInstance, IECSingleInstance)

#endif /* IECINTERFACES_HPP */
