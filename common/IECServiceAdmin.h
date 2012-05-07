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

// IECServiceAdmin.h: interface for the IECServiceAdmin class.
//
//////////////////////////////////////////////////////////////////////

#ifndef IECSERVICEADMIN
#define IECSERVICEADMIN

#include "ECDefs.h"

class IECServiceAdmin : public IUnknown {
public:
	// Create/Delete stores
	virtual HRESULT __stdcall CreateStore(ULONG ulStoreType, ULONG cbUserId, LPENTRYID lpUserId, ULONG* lpcbStoreId, LPENTRYID* lppStoreId, ULONG* lpcbRootId, LPENTRYID *lppRootId) = 0;
	virtual HRESULT __stdcall CreateEmptyStore(ULONG ulStoreType, ULONG cbUserId, LPENTRYID lpUserId, ULONG ulFlags, ULONG* lpcbStoreId, LPENTRYID* lppStoreId, ULONG* lpcbRootId, LPENTRYID *lppRootId) = 0;
	virtual HRESULT __stdcall ResolveStore(LPGUID lpGuid, ULONG *lpulUserID, ULONG* lpcbStoreID, LPENTRYID* lppStoreID) = 0;
	virtual HRESULT __stdcall HookStore(ULONG ulStoreType, ULONG cbUserId, LPENTRYID lpUserId, LPGUID lpGuid) = 0;
	virtual HRESULT __stdcall UnhookStore(ULONG ulStoreType, ULONG cbUserId, LPENTRYID lpUserId) = 0;
	virtual HRESULT __stdcall RemoveStore(LPGUID lpGuid) = 0;

	// User functions
	virtual HRESULT __stdcall CreateUser(LPECUSER lpECUser, ULONG ulFlags, ULONG *lpcbUserId, LPENTRYID *lppUserId) = 0;
	virtual HRESULT __stdcall DeleteUser(ULONG cbUserId, LPENTRYID lpUserId) = 0;
	virtual HRESULT __stdcall SetUser(LPECUSER lpECUser, ULONG ulFlags) = 0;
	virtual HRESULT __stdcall GetUser(ULONG cbUserId, LPENTRYID lpUserId, ULONG ulFlags, LPECUSER *lppECUser) = 0;
	virtual HRESULT __stdcall ResolveUserName(LPCTSTR lpszUserName, ULONG ulFlags, ULONG *lpcbUserId, LPENTRYID *lppUserId) = 0;
	virtual HRESULT __stdcall GetUserList(ULONG cbCompanyId, LPENTRYID lpCompanyId, ULONG ulFlags, ULONG *lpcUsers, LPECUSER *lppsUsers) = 0;
	virtual HRESULT __stdcall GetSendAsList(ULONG cbUserId, LPENTRYID lpUserId, ULONG ulFlags, ULONG *lpcSenders, LPECUSER *lppSenders) = 0;
	virtual HRESULT __stdcall AddSendAsUser(ULONG cbUserId, LPENTRYID lpUserId, ULONG cbSenderId, LPENTRYID lpSenderId) = 0;
	virtual HRESULT __stdcall DelSendAsUser(ULONG cbUserId, LPENTRYID lpUserId, ULONG cbSenderId, LPENTRYID lpSenderId) = 0;
	virtual HRESULT __stdcall GetUserClientUpdateStatus(ULONG cbUserId, LPENTRYID lpUserId, ULONG ulFlags, LPECUSERCLIENTUPDATESTATUS *lppECUCUS) = 0;
	
	// Remove all users EXCEPT the passed user
	virtual HRESULT __stdcall RemoveAllObjects(ULONG cbUserId, LPENTRYID lpUserId) = 0;

	// Group functions
	virtual HRESULT __stdcall CreateGroup(LPECGROUP lpECGroup, ULONG ulFlags, ULONG *lpcbGroupId, LPENTRYID *lppGroupId) = 0;
	virtual HRESULT __stdcall DeleteGroup(ULONG cbGroupId, LPENTRYID lpGroupId) = 0;
	virtual HRESULT __stdcall SetGroup(LPECGROUP lpECGroup, ULONG ulFlags) = 0;
	virtual HRESULT __stdcall GetGroup(ULONG cbGroupId, LPENTRYID lpGroupId, ULONG ulFlags, LPECGROUP *lppECGroup) = 0;
	virtual HRESULT __stdcall ResolveGroupName(LPCTSTR lpszGroupName, ULONG ulFlags, ULONG *lpcbGroupId, LPENTRYID *lppGroupId) = 0;
	virtual HRESULT __stdcall GetGroupList(ULONG cbCompanyId, LPENTRYID lpCompanyId, ULONG ulFlags, ULONG *lpcGroups, LPECGROUP *lppsGroups) = 0;

	virtual HRESULT __stdcall DeleteGroupUser(ULONG cbGroupId, LPENTRYID lpGroupId, ULONG cbUserId, LPENTRYID lpUserId) = 0;
	virtual HRESULT __stdcall AddGroupUser(ULONG cbGroupId, LPENTRYID lpGroupId, ULONG cbUserId, LPENTRYID lpUserId) = 0;
	virtual HRESULT __stdcall GetUserListOfGroup(ULONG cbGroupId, LPENTRYID lpGroupId, ULONG ulFlags, ULONG *lpcUsers, LPECUSER *lppsUsers) = 0;
	virtual HRESULT __stdcall GetGroupListOfUser(ULONG cbUserId, LPENTRYID lpUserId, ULONG ulFlags, ULONG *lpcGroups, LPECGROUP *lppsGroups) = 0;

	// Company functions
	virtual HRESULT __stdcall CreateCompany(LPECCOMPANY lpECCompany, ULONG ulFlags, ULONG *lpcbCompanyId, LPENTRYID *lppCompanyId) = 0;
	virtual HRESULT __stdcall DeleteCompany(ULONG cbCompanyId, LPENTRYID lpCompanyId) = 0;
	virtual HRESULT __stdcall SetCompany(LPECCOMPANY lpECCompany, ULONG ulFlags) = 0;
	virtual HRESULT __stdcall GetCompany(ULONG cbCompanyId, LPENTRYID lpCompanyId, ULONG ulFlags, LPECCOMPANY *lppECCompany) = 0;
	virtual HRESULT __stdcall ResolveCompanyName(LPCTSTR lpszCompanyName, ULONG ulFlags, ULONG *lpcbCompanyId, LPENTRYID *lppCompanyId) = 0;
	virtual HRESULT __stdcall GetCompanyList(ULONG ulFlags, ULONG *lpcCompanies, LPECCOMPANY *lppsCompanies) = 0;

	virtual HRESULT __stdcall AddCompanyToRemoteViewList(ULONG cbSetCompanyId, LPENTRYID lpSetCompanyId, ULONG cbCompanyId, LPENTRYID lpCompanyId) = 0;
	virtual HRESULT __stdcall DelCompanyFromRemoteViewList(ULONG cbSetCompanyId, LPENTRYID lpSetCompanyId, ULONG cbCompanyId, LPENTRYID lpCompanyId) = 0;
	virtual HRESULT __stdcall GetRemoteViewList(ULONG cbCompanyId, LPENTRYID lpCompanyId, ULONG ulFlags, ULONG *lpcCompanies, LPECCOMPANY *lppsCompanies) = 0;
	virtual HRESULT __stdcall AddUserToRemoteAdminList(ULONG cbUserId, LPENTRYID lpUserId, ULONG cbCompanyId, LPENTRYID lpCompanyId) = 0;
	virtual HRESULT __stdcall DelUserFromRemoteAdminList(ULONG cbUserId, LPENTRYID lpUserId, ULONG cbCompanyId, LPENTRYID lpCompanyId) = 0;
	virtual HRESULT __stdcall GetRemoteAdminList(ULONG cbCompanyId, LPENTRYID lpCompanyId, ULONG ulFlags, ULONG *lpcUsers, LPECUSER *lppsUsers) = 0;

	virtual HRESULT __stdcall SyncUsers(ULONG cbCompanyId, LPENTRYID lpCOmpanyId) = 0;

	// Quota functions
	
	virtual HRESULT __stdcall GetQuota(ULONG cbUserId, LPENTRYID lpUserId, bool bGetUserDefaultQuota, LPECQUOTA* lppsQuota) = 0;
	virtual HRESULT __stdcall SetQuota(ULONG cbUserId, LPENTRYID lpUserId, LPECQUOTA lpsQuota) = 0;

	virtual HRESULT __stdcall AddQuotaRecipient(ULONG cbCompanyId, LPENTRYID lpCompanyId, ULONG cbRecipientId, LPENTRYID lpRecipientId, ULONG ulType) = 0;
	virtual HRESULT __stdcall DeleteQuotaRecipient(ULONG cbCompanyId, LPENTRYID lpCmopanyId, ULONG cbRecipientId, LPENTRYID lpRecipientId, ULONG ulType) = 0;
	virtual HRESULT __stdcall GetQuotaRecipients(ULONG cbUserId, LPENTRYID lpUserId, ULONG ulFlags, ULONG *lpcUsers, LPECUSER *lppsUsers) = 0;

	virtual HRESULT __stdcall GetQuotaStatus(ULONG cbUserId, LPENTRYID lpUserId, LPECQUOTASTATUS* lppsQuotaStatus) = 0;
	
	virtual HRESULT __stdcall PurgeSoftDelete(ULONG ulDays) = 0;
	virtual HRESULT __stdcall PurgeCache(ULONG ulFlags) = 0;
	virtual HRESULT __stdcall OpenUserStoresTable(ULONG ulFlags, LPMAPITABLE *lppTable) = 0;
	virtual HRESULT __stdcall PurgeDeferredUpdates(ULONG *lpulDeferredRemaining) = 0;

	// Multiserver functions
	virtual HRESULT __stdcall GetServerDetails(LPECSVRNAMELIST lpServerNameList, ULONG ulFlags, LPECSERVERLIST* lppsServerList) = 0;
	virtual HRESULT __stdcall ResolvePseudoUrl(char *lpszPseudoUrl, char **lppszServerPath, bool *lpbIsPeer) = 0;

	// Public store function(s)
	virtual HRESULT __stdcall GetPublicStoreEntryID(ULONG ulFlags, ULONG* lpcbStoreID, LPENTRYID* lppStoreID) = 0;

	// Archive store function(s)
	virtual HRESULT __stdcall GetArchiveStoreEntryID(LPCTSTR lpszUserName, LPCTSTR lpszServerName, ULONG ulFlags, ULONG* lpcbStoreID, LPENTRYID* lppStoreID) = 0;

	virtual HRESULT __stdcall ResetFolderCount(ULONG cbEntryId, LPENTRYID lpEntryId, ULONG *lpulUpdates) = 0;
};

#endif // #ifndef IECSERVICEADMIN
