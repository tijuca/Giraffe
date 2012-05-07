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

#ifndef ECMSGSTORE_H
#define ECMSGSTORE_H

#include <mapidefs.h>
#include <mapispi.h>

//NOTE: windows in $TOP/common/windows, linux in $TOP/mapi4linux/include
#include <edkmdb.h>

#include "ECUnknown.h"
#include "ECMAPIProp.h"
#include "WSTransport.h"
#include "ECNotifyClient.h"
#include "ECNamedProp.h"

#include "IECServiceAdmin.h"
#include "IECSpooler.h"
#include "IECMultiStoreTable.h"
#include "IECLicense.h"
#include "IECTestProtocol.h"

#include "IMAPIOffline.h"

#include <set>

class convstring;
class utf8string;
class ECMessage;
class ECMAPIFolder;

class IMessageFactory {
public:
	virtual HRESULT Create(ECMsgStore *lpMsgStore, BOOL fNew, BOOL fModify, ULONG ulFlags, BOOL bEmbedded, ECMAPIProp *lpRoot, ECMessage **lppMessage) const = 0;
};


class ECMsgStore : public ECMAPIProp {
	typedef void (* RELEASECALLBACK)(ECUnknown *lpObject, ECMsgStore *lpMsgStore);
protected:
	ECMsgStore(char *lpszProfname, LPMAPISUP lpSupport, WSTransport *lpTransport, BOOL fModify, ULONG ulProfileFlags, BOOL fIsSpooler, BOOL fIsDefaultStore, BOOL bOfflineStore);
	virtual ~ECMsgStore();

	static HRESULT GetPropHandler(ULONG ulPropTag, void* lpProvider, ULONG ulFlags, LPSPropValue lpsPropValue, void *lpParam, void *lpBase);
	static HRESULT SetPropHandler(ULONG ulPropTag, void* lpProvider, LPSPropValue lpsPropValue, void *lpParam);

public:
	virtual HRESULT QueryInterface(REFIID refiid, void **lppInterface);
	virtual HRESULT QueryInterfaceProxy(REFIID refiid, void **lppInterface);

	static HRESULT Create(char *lpszProfname, LPMAPISUP lpSupport, WSTransport *lpTransport, BOOL fModify, ULONG ulProfileFlags, BOOL bIsSpooler, BOOL fIsDefaultStore, BOOL bOfflineStore, ECMsgStore **lppECMsgStore);

	virtual HRESULT SaveChanges(ULONG ulFlags);
	virtual HRESULT SetProps(ULONG cValues, LPSPropValue lpPropArray, LPSPropProblemArray *lppProblems);
	virtual HRESULT DeleteProps(LPSPropTagArray lpPropTagArray, LPSPropProblemArray FAR * lppProblems);

	virtual HRESULT OpenProperty(ULONG ulPropTag, LPCIID lpiid, ULONG ulInterfaceOptions, ULONG ulFlags, LPUNKNOWN FAR * lppUnk);
	virtual HRESULT Advise(ULONG cbEntryID, LPENTRYID lpEntryID, ULONG ulEventMask, LPMAPIADVISESINK lpAdviseSink, ULONG *lpulConnection);
	virtual HRESULT Unadvise(ULONG ulConnection);
	virtual HRESULT CompareEntryIDs(ULONG cbEntryID1, LPENTRYID lpEntryID1, ULONG cbEntryID2, LPENTRYID lpEntryID2, ULONG ulFlags, ULONG *lpulResult);
	virtual HRESULT OpenEntry(ULONG cbEntryID, LPENTRYID lpEntryID, LPCIID lpInterface, ULONG ulFlags, ULONG *lpulObjType, LPUNKNOWN *lppUnk);
	virtual HRESULT SetReceiveFolder(LPTSTR lpszMessageClass, ULONG ulFlags, ULONG cbEntryID, LPENTRYID lpEntryID);
	virtual HRESULT GetReceiveFolder(LPTSTR lpszMessageClass, ULONG ulFlags, ULONG *lpcbEntryID, LPENTRYID *lppEntryID, LPTSTR *lppszExplicitClass);
	virtual HRESULT GetReceiveFolderTable(ULONG ulFlags, LPMAPITABLE *lppTable);
	virtual HRESULT StoreLogoff(ULONG *lpulFlags);
	virtual HRESULT AbortSubmit(ULONG cbEntryID, LPENTRYID lpEntryID, ULONG ulFlags);
	virtual HRESULT GetOutgoingQueue(ULONG ulFlags, LPMAPITABLE *lppTable);
	virtual HRESULT SetLockState(LPMESSAGE lpMessage,ULONG ulLockState);
	virtual HRESULT FinishedMsg(ULONG ulFlags, ULONG cbEntryID, LPENTRYID lpEntryID);
	virtual HRESULT NotifyNewMail(LPNOTIFICATION lpNotification);


	virtual HRESULT CreateStoreEntryID(LPTSTR lpszMsgStoreDN, LPTSTR lpszMailboxDN, ULONG ulFlags, ULONG *lpcbEntryID, LPENTRYID *lppEntryID);
	virtual HRESULT EntryIDFromSourceKey(ULONG cFolderKeySize, BYTE *lpFolderSourceKey,	ULONG cMessageKeySize, BYTE *lpMessageSourceKey, ULONG *lpcbEntryID, LPENTRYID *lppEntryID);
	virtual HRESULT GetRights(ULONG cbUserEntryID, LPENTRYID lpUserEntryID, ULONG cbEntryID, LPENTRYID lpEntryID, ULONG *lpulRights);
	virtual HRESULT GetMailboxTable(LPTSTR lpszServerName, LPMAPITABLE *lppTable, ULONG ulFlags);
	virtual HRESULT GetPublicFolderTable(LPTSTR lpszServerName, LPMAPITABLE *lppTable, ULONG ulFlags);

	virtual HRESULT SetEntryId(ULONG cbEntryId, LPENTRYID lpEntryId);

	virtual ULONG	Release();
	virtual HRESULT HrSetReleaseCallback(ECUnknown *lpObject, RELEASECALLBACK lpfnCallback);

	// IECSpooler
	virtual HRESULT GetMasterOutgoingTable(ULONG ulFlags, IMAPITable ** lppOutgoingTable);
	virtual HRESULT DeleteFromMasterOutgoingTable(ULONG cbEntryId, LPENTRYID lpEntryId, ULONG ulFlags);

	// IECServiceAdmin
	virtual HRESULT CreateStore(ULONG ulStoreType, ULONG cbUserId, LPENTRYID lpUserId, ULONG* lpcbStoreId, LPENTRYID* lppStoreId, ULONG* lpcbRootId, LPENTRYID *lppRootId);
	virtual HRESULT CreateEmptyStore(ULONG ulStoreType, ULONG cbUserId, LPENTRYID lpUserId, ULONG ulFlags, ULONG* lpcbStoreId, LPENTRYID* lppStoreId, ULONG* lpcbRootId, LPENTRYID* lppRootId);
	virtual HRESULT HookStore(ULONG ulStoreType, ULONG cbUserId, LPENTRYID lpUserId, LPGUID lpGuid);
	virtual HRESULT UnhookStore(ULONG ulStoreType, ULONG cbUserId, LPENTRYID lpUserId);
	virtual HRESULT RemoveStore(LPGUID lpGuid);
	virtual HRESULT ResolveStore(LPGUID lpGuid, ULONG *lpulUserID, ULONG* lpcbStoreID, LPENTRYID* lppStoreID);
	virtual HRESULT CreateUser(LPECUSER lpECUser, ULONG ulFlags, ULONG *lpcbUserId, LPENTRYID *lppUserId);
	virtual HRESULT DeleteUser(ULONG cbUserId, LPENTRYID lpUserId);
	virtual HRESULT SetUser(LPECUSER lpECUser, ULONG ulFlags);
	virtual HRESULT GetUser(ULONG cbUserId, LPENTRYID lpUserId, ULONG ulFlags, LPECUSER *lppECUser);
	virtual HRESULT ResolveUserName(LPCTSTR lpszUserName, ULONG ulFlags, ULONG *lpcbUserId, LPENTRYID *lppUserId);
	// virtual HRESULT GetUserList(ULONG *lpcUsers, LPECUSER *lppsUsers); // inherited from ECMAPIProp
	virtual HRESULT GetSendAsList(ULONG cbUserId, LPENTRYID lpUserId, ULONG ulFlags, ULONG *lpcSenders, LPECUSER *lppSenders);
	virtual HRESULT AddSendAsUser(ULONG cbUserId, LPENTRYID lpUserId, ULONG cbSenderId, LPENTRYID lpSenderId);
	virtual HRESULT DelSendAsUser(ULONG cbUserId, LPENTRYID lpUserId, ULONG cbSenderId, LPENTRYID lpSenderId);
	virtual HRESULT GetUserClientUpdateStatus(ULONG cbUserId, LPENTRYID lpUserId, ULONG ulFlags, LPECUSERCLIENTUPDATESTATUS *lppECUCUS);
	virtual HRESULT RemoveAllObjects(ULONG cbUserId, LPENTRYID lpUserId);
	virtual HRESULT CreateGroup(LPECGROUP lpECGroup, ULONG ulFlags, ULONG *lpcbGroupId, LPENTRYID *lppGroupId);
	virtual HRESULT DeleteGroup(ULONG cbGroupId, LPENTRYID lpGroupId);
	virtual HRESULT SetGroup(LPECGROUP lpECGroup, ULONG ulFlags);
	virtual HRESULT GetGroup(ULONG cbGroupId, LPENTRYID lpGroupId, ULONG ulFlags, LPECGROUP *lppECGroup);
	virtual HRESULT ResolveGroupName(LPCTSTR lpszGroupName, ULONG ulFlags, ULONG *lpcbGroupId, LPENTRYID *lppGroupId);
	// virtual HRESULT GetGroupList(ULONG *lpcGroups, LPECGROUP *lppsGroups); // inherited from ECMAPIProp
	virtual HRESULT DeleteGroupUser(ULONG cbGroupId, LPENTRYID lpGroupId, ULONG cbUserId, LPENTRYID lpUserId);
	virtual HRESULT AddGroupUser(ULONG cbGroupId, LPENTRYID lpGroupId, ULONG cbUserId, LPENTRYID lpUserId);
	virtual HRESULT GetUserListOfGroup(ULONG cbGroupId, LPENTRYID lpGroupId, ULONG ulFlags, ULONG *lpcUsers, LPECUSER *lppsUsers);
	virtual HRESULT GetGroupListOfUser(ULONG cbUserId, LPENTRYID lpUserId, ULONG ulFlags, ULONG *lpcGroups, LPECGROUP *lppsGroups);
	virtual HRESULT CreateCompany(LPECCOMPANY lpECCompany, ULONG ulFlags, ULONG *lpcbCompanyId, LPENTRYID *lppCompanyId);
	virtual HRESULT DeleteCompany(ULONG cbCompanyId, LPENTRYID lpCompanyId);
	virtual HRESULT SetCompany(LPECCOMPANY lpECCompany, ULONG ulFlags);
	virtual HRESULT GetCompany(ULONG cbCompanyId, LPENTRYID lpCompanyId, ULONG ulFlags, LPECCOMPANY *lppECCompany);
	virtual HRESULT ResolveCompanyName(LPCTSTR lpszCompanyName, ULONG ulFlags, ULONG *lpcbCompanyId, LPENTRYID *lppCompanyId);
	virtual HRESULT GetCompanyList(ULONG ulFlags, ULONG *lpcCompanies, LPECCOMPANY *lppsCompanies);
	virtual HRESULT AddCompanyToRemoteViewList(ULONG cbSetCompanyId, LPENTRYID lpSetCompanyId, ULONG cbCompanyId, LPENTRYID lpCompanyId);
	virtual HRESULT DelCompanyFromRemoteViewList(ULONG cbSetCompanyId, LPENTRYID lpSetCompanyId, ULONG cbCompanyId, LPENTRYID lpCompanyId);
	virtual HRESULT GetRemoteViewList(ULONG cbCompanyId, LPENTRYID lpCompanyId, ULONG ulFlags, ULONG *lpcCompanies, LPECCOMPANY *lppsCompanies);
	virtual HRESULT AddUserToRemoteAdminList(ULONG cbUserId, LPENTRYID lpUserId, ULONG cbCompanyId, LPENTRYID lpCompanyId);
	virtual HRESULT DelUserFromRemoteAdminList(ULONG cbUserId, LPENTRYID lpUserId, ULONG cbCompanyId, LPENTRYID lpCompanyId);
	virtual HRESULT GetRemoteAdminList(ULONG cbCompanyId, LPENTRYID lpCompanyId, ULONG ulFlags, ULONG *lpcUsers, LPECUSER *lppsUsers);
	virtual HRESULT SyncUsers(ULONG cbCompanyId, LPENTRYID lpCompanyId);
	virtual HRESULT GetQuota(ULONG cbUserId, LPENTRYID lpUserId, bool bGetUserDefault, LPECQUOTA* lppsQuota);
	virtual HRESULT SetQuota(ULONG cbUserId, LPENTRYID lpUserId, LPECQUOTA lpsQuota);
	virtual HRESULT AddQuotaRecipient(ULONG cbCompanyId, LPENTRYID lpCompanyId, ULONG cbRecipientId, LPENTRYID lpRecipientId, ULONG ulType);
	virtual HRESULT DeleteQuotaRecipient(ULONG cbCompanyId, LPENTRYID lpCmopanyId, ULONG cbRecipientId, LPENTRYID lpRecipientId, ULONG ulType);
	virtual HRESULT GetQuotaRecipients(ULONG cbUserId, LPENTRYID lpUserId, ULONG ulFlags, ULONG *lpcUsers, LPECUSER *lppsUsers);
	virtual HRESULT GetQuotaStatus(ULONG cbUserId, LPENTRYID lpUserId, LPECQUOTASTATUS* lppsQuotaStatus);
	virtual HRESULT PurgeCache(ULONG ulFlags);
	virtual HRESULT PurgeSoftDelete(ULONG ulDays);	
	virtual HRESULT PurgeDeferredUpdates(ULONG *lpulDeferredRemaining);
	virtual HRESULT GetServerDetails(LPECSVRNAMELIST lpServerNameList, ULONG ulFlags, LPECSERVERLIST* lppsServerList);
	virtual HRESULT OpenUserStoresTable(ULONG ulFlags, LPMAPITABLE *lppTable);
	virtual HRESULT ResolvePseudoUrl(char *lpszPseudoUrl, char **lppszServerPath, bool *lpbIsPeer);
	virtual HRESULT GetPublicStoreEntryID(ULONG ulFlags, ULONG* lpcbStoreID, LPENTRYID* lppStoreID);
	virtual HRESULT GetArchiveStoreEntryID(LPCTSTR lpszUserName, LPCTSTR lpszServerName, ULONG ulFlags, ULONG* lpcbStoreID, LPENTRYID* lppStoreID);
	virtual HRESULT ResetFolderCount(ULONG cbEntryId, LPENTRYID lpEntryId, ULONG *lpulUpdates);
	
	// MAPIOfflineMgr
	virtual HRESULT SetCurrentState(ULONG ulFlags, ULONG ulMask, ULONG ulState, void* pReserved);
	virtual HRESULT GetCapabilities(ULONG *pulCapabilities);
	virtual HRESULT GetCurrentState(ULONG* pulState);

	virtual HRESULT Advise(ULONG ulFlags, MAPIOFFLINE_ADVISEINFO* pAdviseInfo, ULONG* pulAdviseToken);
	virtual HRESULT Unadvise(ULONG ulFlags,ULONG ulAdviseToken);

	virtual HRESULT UnwrapNoRef(LPVOID *ppvObject);

	// ECMultiStoreTable
	virtual HRESULT OpenMultiStoreTable(LPENTRYLIST lpMsgList, ULONG ulFlags, LPMAPITABLE *lppTable);

    // ECLicense
    virtual HRESULT LicenseAuth(unsigned char *lpData, unsigned int ulSize, unsigned char **lppResponse, unsigned int * lpulResponseData);
    virtual HRESULT LicenseCapa(unsigned int ulServiceType, char ***lppszChars, unsigned int *lpulSize);
	virtual HRESULT LicenseUsers(unsigned int ulServiceType, unsigned int *lpulUsers);

    // ECTestProtocol
	virtual HRESULT TestPerform(char *szCommand, unsigned int ulArgs, char **szArgs);
	virtual HRESULT TestSet(char *szName, char *szValue);
	virtual HRESULT TestGet(char *szName, char **szValue);

	// Called when session is reloaded
	static HRESULT Reload(void *lpParam, ECSESSIONID sessionid);

	static HRESULT TableRowGetProp(void* lpProvider, struct propVal *lpsPropValSrc, LPSPropValue lpsPropValDst, void **lpBase, ULONG ulType);

protected:
	HRESULT OpenEntry(ULONG cbEntryID, LPENTRYID lpEntryID, LPCIID lpInterface, ULONG ulFlags, const IMessageFactory &refMessageFactory, ULONG *lpulObjType, LPUNKNOWN *lppUnk);

public:
	BOOL IsSpooler(){ return m_fIsSpooler; }
	BOOL IsDefaultStore() { return m_fIsDefaultStore; }
	BOOL IsPublicStore();
	BOOL IsDelegateStore();
	BOOL IsOfflineStore() { return m_bOfflineStore; }
	LPCSTR GetProfileName() const { return m_strProfname.c_str(); }

public:
	const GUID& GetStoreGuid();
	HRESULT GetWrappedStoreEntryID(ULONG* lpcbWrapped, LPENTRYID* lppWrapped);
	//Special wrapper for the spooler vs outgoing queue
	HRESULT GetWrappedServerStoreEntryID(ULONG cbEntryId, LPBYTE lpEntryId, ULONG* lpcbWrapped, LPENTRYID* lppWrapped);

	HRESULT InternalAdvise(ULONG cbEntryID, LPENTRYID lpEntryID, ULONG ulEventMask, LPMAPIADVISESINK lpAdviseSink, ULONG *lpulConnection);
private:
	HRESULT CreateSpecialFolder(LPMAPIFOLDER lpFolderParent, ECMAPIProp *lpFolderPropSet, LPTSTR lpszFolderName, LPTSTR lpszFolderComment, unsigned int ulPropTag, unsigned int ulMVPos, LPTSTR lpszContainerClass, LPMAPIFOLDER *lppMAPIFolder);
	HRESULT SetSpecialEntryIdOnFolder(LPMAPIFOLDER lpFolder, ECMAPIProp *lpFolderPropSet, unsigned int ulPropTag, unsigned int ulMVPos);
	HRESULT OpenStatsTable(unsigned int ulTableType, LPMAPITABLE *lppTable);
	HRESULT CreateAdditionalFolder(IMAPIFolder *lpRootFolder, IMAPIFolder *lpInboxFolder, IMAPIFolder *lpSubTreeFolder, ULONG ulType, LPTSTR lpszFolderName, LPTSTR lpszComment, LPTSTR lpszContainerType, bool fHidden);
	HRESULT AddRenAdditionalFolder(IMAPIFolder *lpFolder, ULONG ulType, SBinary *lpEntryID);



	static HRESULT MsgStoreDnToPseudoUrl(const utf8string &strMsgStoreDN, utf8string *lpstrPseudoUrl);

public:
			
	class xMsgStore : public IMsgStore {
		// IUnknown
		virtual ULONG __stdcall AddRef();
		virtual ULONG __stdcall Release();
		virtual HRESULT __stdcall QueryInterface(REFIID refiid, void **lppInterface);

		// IMsgStore
		virtual HRESULT __stdcall Advise(ULONG cbEntryID, LPENTRYID lpEntryID, ULONG ulEventMask, LPMAPIADVISESINK lpAdviseSink, ULONG *lpulConnection);
		virtual HRESULT __stdcall Unadvise(ULONG ulConnection);
		virtual HRESULT __stdcall CompareEntryIDs(ULONG cbEntryID1, LPENTRYID lpEntryID1, ULONG cbEntryID2, LPENTRYID lpEntryID2, ULONG ulFlags, ULONG *lpulResult);
		virtual HRESULT __stdcall OpenEntry(ULONG cbEntryID, LPENTRYID lpEntryID, LPCIID lpInterface, ULONG ulFlags, ULONG *lpulObjType, LPUNKNOWN *lppUnk);
		virtual HRESULT __stdcall SetReceiveFolder(LPTSTR lpszMessageClass, ULONG ulFlags, ULONG cbEntryID, LPENTRYID lpEntryID);
		virtual HRESULT __stdcall GetReceiveFolder(LPTSTR lpszMessageClass, ULONG ulFlags, ULONG *lpcbEntryID, LPENTRYID *lppEntryID, LPTSTR *lppszExplicitClass);
		virtual HRESULT __stdcall GetReceiveFolderTable(ULONG ulFlags, LPMAPITABLE *lppTable);
		virtual HRESULT __stdcall StoreLogoff(ULONG *lpulFlags);
		virtual HRESULT __stdcall AbortSubmit(ULONG cbEntryID, LPENTRYID lpEntryID, ULONG ulFlags);
		virtual HRESULT __stdcall GetOutgoingQueue(ULONG ulFlags, LPMAPITABLE *lppTable);
		virtual HRESULT __stdcall SetLockState(LPMESSAGE lpMessage,ULONG ulLockState);
		virtual HRESULT __stdcall FinishedMsg(ULONG ulFlags, ULONG cbEntryID, LPENTRYID lpEntryID);
		virtual HRESULT __stdcall NotifyNewMail(LPNOTIFICATION lpNotification);

		// IMAPIProp
		virtual HRESULT __stdcall GetLastError(HRESULT hError, ULONG ulFlags, LPMAPIERROR * lppMapiError);
		virtual HRESULT __stdcall SaveChanges(ULONG ulFlags);
		virtual HRESULT __stdcall GetProps(LPSPropTagArray lpPropTagArray, ULONG ulFlags, ULONG FAR * lpcValues, LPSPropValue FAR * lppPropArray);
		virtual HRESULT __stdcall GetPropList(ULONG ulFlags, LPSPropTagArray FAR * lppPropTagArray);
		virtual HRESULT __stdcall OpenProperty(ULONG ulPropTag, LPCIID lpiid, ULONG ulInterfaceOptions, ULONG ulFlags, LPUNKNOWN FAR * lppUnk);
		virtual HRESULT __stdcall SetProps(ULONG cValues, LPSPropValue lpPropArray, LPSPropProblemArray FAR * lppProblems);
		virtual HRESULT __stdcall DeleteProps(LPSPropTagArray lpPropTagArray, LPSPropProblemArray FAR * lppProblems);
		virtual HRESULT __stdcall CopyTo(ULONG ciidExclude, LPCIID rgiidExclude, LPSPropTagArray lpExcludeProps, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, LPCIID lpInterface, LPVOID lpDestObj, ULONG ulFlags, LPSPropProblemArray FAR * lppProblems);
		virtual HRESULT __stdcall CopyProps(LPSPropTagArray lpIncludeProps, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, LPCIID lpInterface, LPVOID lpDestObj, ULONG ulFlags, LPSPropProblemArray FAR * lppProblems);
		virtual HRESULT __stdcall GetNamesFromIDs(LPSPropTagArray * pptaga, LPGUID lpguid, ULONG ulFlags, ULONG * pcNames, LPMAPINAMEID ** pppNames);
		virtual HRESULT __stdcall GetIDsFromNames(ULONG cNames, LPMAPINAMEID * ppNames, ULONG ulFlags, LPSPropTagArray * pptaga);

	} m_xMsgStore;

	class xExchangeManageStore : public IExchangeManageStore {
		// IUnknown
		virtual ULONG __stdcall AddRef();
		virtual ULONG __stdcall Release();
		virtual HRESULT __stdcall QueryInterface(REFIID refiid, void **lppInterface);

		virtual HRESULT __stdcall CreateStoreEntryID(LPTSTR lpszMsgStoreDN, LPTSTR lpszMailboxDN, ULONG ulFlags, ULONG *lpcbEntryID, LPENTRYID *lppEntryID);
		virtual HRESULT __stdcall EntryIDFromSourceKey(ULONG cFolderKeySize, BYTE *lpFolderSourceKey, ULONG cMessageKeySize, BYTE *lpMessageSourceKey, ULONG *lpcbEntryID, LPENTRYID *lppEntryID);
		virtual HRESULT __stdcall GetRights(ULONG cbUserEntryID, LPENTRYID lpUserEntryID, ULONG cbEntryID, LPENTRYID lpEntryID, ULONG *lpulRights);
		virtual HRESULT __stdcall GetMailboxTable(LPTSTR lpszServerName, LPMAPITABLE *lppTable, ULONG ulFlags);
		virtual HRESULT __stdcall GetPublicFolderTable(LPTSTR lpszServerName, LPMAPITABLE *lppTable, ULONG ulFlags);
	} m_xExchangeManageStore;

	class xECServiceAdmin : public IECServiceAdmin {
		// From IUnknown
		virtual HRESULT __stdcall QueryInterface(REFIID refiid, void** lppInterface);
		virtual ULONG __stdcall AddRef();
		virtual ULONG __stdcall Release();	

		virtual HRESULT __stdcall CreateStore(ULONG ulStoreType, ULONG cbUserId, LPENTRYID lpUserId, ULONG* lpcbStoreId, LPENTRYID* lppStoreId, ULONG* lpcbRootId, LPENTRYID *lppRootId);
		virtual HRESULT __stdcall CreateEmptyStore(ULONG ulStoreType, ULONG cbUserId, LPENTRYID lpUserId, ULONG ulFlags, ULONG* lpcbStoreId, LPENTRYID* lppStoreId, ULONG* lpcbRootId, LPENTRYID *lppRootId);
		virtual HRESULT __stdcall HookStore(ULONG ulStoreType, ULONG cbUserId, LPENTRYID lpUserId, LPGUID lpGuid);
		virtual HRESULT __stdcall UnhookStore(ULONG ulStoreType, ULONG cbUserId, LPENTRYID lpUserId);
		virtual HRESULT __stdcall RemoveStore(LPGUID lpGuid);
		virtual HRESULT __stdcall ResolveStore(LPGUID lpGuid, ULONG *lpulUserID, ULONG* lpcbStoreID, LPENTRYID* lppStoreID);
		virtual HRESULT __stdcall CreateUser(LPECUSER lpECUser, ULONG ulFlags, ULONG *lpcbUserId, LPENTRYID *lppUserId);
		virtual HRESULT __stdcall DeleteUser(ULONG cbUserId, LPENTRYID lpUserId);
		virtual HRESULT __stdcall SetUser(LPECUSER lpECUser, ULONG ulFlags);
		virtual HRESULT __stdcall GetUser(ULONG cbUserId, LPENTRYID lpUserId, ULONG ulFlags, LPECUSER *lppECUser);
		virtual HRESULT __stdcall ResolveUserName(LPCTSTR lpszUserName, ULONG ulFlags, ULONG *lpcbUserId, LPENTRYID *lppUserId);
		virtual HRESULT __stdcall GetUserList(ULONG cbCompanyId, LPENTRYID lpCompanyId, ULONG ulFlags, ULONG *lpcUsers, LPECUSER *lppsUsers);
		virtual HRESULT __stdcall GetSendAsList(ULONG cbUserId, LPENTRYID lpUserId, ULONG ulFlags, ULONG *lpcSenders, LPECUSER *lppSenders);
		virtual HRESULT __stdcall AddSendAsUser(ULONG cbUserId, LPENTRYID lpUserId, ULONG cbSenderId, LPENTRYID lpSenderId);
		virtual HRESULT __stdcall DelSendAsUser(ULONG cbUserId, LPENTRYID lpUserId, ULONG cbSenderId, LPENTRYID lpSenderId);
		virtual HRESULT __stdcall GetUserClientUpdateStatus(ULONG cbUserId, LPENTRYID lpUserId, ULONG ulFlags, LPECUSERCLIENTUPDATESTATUS *lppECUCUS);
		virtual HRESULT __stdcall RemoveAllObjects(ULONG cbUserId, LPENTRYID lpUserId);
		virtual HRESULT __stdcall CreateGroup(LPECGROUP lpECGroup, ULONG ulFlags, ULONG *lpcbGroupId, LPENTRYID *lppGroupId);
		virtual HRESULT __stdcall DeleteGroup(ULONG cbGroupId, LPENTRYID lpGroupId);
		virtual HRESULT __stdcall SetGroup(LPECGROUP lpECGroup, ULONG ulFlags);
		virtual HRESULT __stdcall GetGroup(ULONG cbGroupId, LPENTRYID lpGroupId, ULONG ulFlags, LPECGROUP *lppECGroup);
		virtual HRESULT __stdcall ResolveGroupName(LPCTSTR lpszGroupName, ULONG ulFlags, ULONG *lpcbGroupId, LPENTRYID *lppGroupId);
		virtual HRESULT __stdcall GetGroupList(ULONG cbCompanyId, LPENTRYID lpCompanyId, ULONG ulFlags, ULONG *lpcGroups, LPECGROUP *lppsGroups);
		virtual HRESULT __stdcall DeleteGroupUser(ULONG cbGroupId, LPENTRYID lpGroupId, ULONG cbUserId, LPENTRYID lpUserId);
		virtual HRESULT __stdcall AddGroupUser(ULONG cbGroupId, LPENTRYID lpGroupId, ULONG cbUserId, LPENTRYID lpUserId);
		virtual HRESULT __stdcall GetUserListOfGroup(ULONG cbGroupId, LPENTRYID lpGroupId, ULONG ulFlags, ULONG *lpcUsers, LPECUSER *lppsUsers);
		virtual HRESULT __stdcall GetGroupListOfUser(ULONG cbUserId, LPENTRYID lpUserId, ULONG ulFlags, ULONG *lpcGroups, LPECGROUP *lppsGroups);
		virtual HRESULT __stdcall CreateCompany(LPECCOMPANY lpECCompany, ULONG ulFlags, ULONG *lpcbCompanyId, LPENTRYID *lppCompanyId);
		virtual HRESULT __stdcall DeleteCompany(ULONG cbCompanyId, LPENTRYID lpCompanyId);
		virtual HRESULT __stdcall SetCompany(LPECCOMPANY lpECCompany, ULONG ulFlags);
		virtual HRESULT __stdcall GetCompany(ULONG cbCompanyId, LPENTRYID lpCompanyId, ULONG ulFlags, LPECCOMPANY *lppECCompany);
		virtual HRESULT __stdcall ResolveCompanyName(LPCTSTR lpszCompanyName, ULONG ulFlags, ULONG *lpcbCompanyId, LPENTRYID *lppCompanyId);
		virtual HRESULT __stdcall GetCompanyList(ULONG ulFlags, ULONG *lpcCompanies, LPECCOMPANY *lppsCompanies);
		virtual HRESULT __stdcall AddCompanyToRemoteViewList(ULONG cbSetCompanyId, LPENTRYID lpSetCompanyId, ULONG cbCompanyId, LPENTRYID lpCompanyId);
		virtual HRESULT __stdcall DelCompanyFromRemoteViewList(ULONG cbSetCompanyId, LPENTRYID lpSetCompanyId, ULONG cbCompanyId, LPENTRYID lpCompanyId);
		virtual HRESULT __stdcall GetRemoteViewList(ULONG cbCompanyId, LPENTRYID lpCompanyId, ULONG ulFlags, ULONG *lpcCompanies, LPECCOMPANY *lppsCompanies);
		virtual HRESULT __stdcall AddUserToRemoteAdminList(ULONG cbUserId, LPENTRYID lpUserId, ULONG cbCompanyId, LPENTRYID lpCompanyId);
		virtual HRESULT __stdcall DelUserFromRemoteAdminList(ULONG cbUserId, LPENTRYID lpUserId, ULONG cbCompanyId, LPENTRYID lpCompanyId);
		virtual HRESULT __stdcall GetRemoteAdminList(ULONG cbCompanyId, LPENTRYID lpCompanyId, ULONG ulFlags, ULONG *lpcUsers, LPECUSER *lppsUsers);
		virtual HRESULT __stdcall SyncUsers(ULONG cbCompanyId, LPENTRYID lpCompanyId);
		virtual HRESULT __stdcall GetQuota(ULONG cbUserId, LPENTRYID lpUserId, bool bGetUserDefault, LPECQUOTA* lppsQuota);
		virtual HRESULT __stdcall SetQuota(ULONG cbUserId, LPENTRYID lpUserId, LPECQUOTA lpsQuota);
		virtual HRESULT __stdcall AddQuotaRecipient(ULONG cbCompanyId, LPENTRYID lpCompanyId, ULONG cbRecipientId, LPENTRYID lpRecipientId, ULONG ulType);
		virtual HRESULT __stdcall DeleteQuotaRecipient(ULONG cbCompanyId, LPENTRYID lpCmopanyId, ULONG cbRecipientId, LPENTRYID lpRecipientId, ULONG ulType);
		virtual HRESULT __stdcall GetQuotaRecipients(ULONG cbUserId, LPENTRYID lpUserId, ULONG ulFlags, ULONG *lpcUsers, LPECUSER *lppsUsers);
		virtual HRESULT __stdcall GetQuotaStatus(ULONG cbUserId, LPENTRYID lpUserId, LPECQUOTASTATUS* lppsQuotaStatus);
		virtual HRESULT __stdcall PurgeCache(ULONG ulFlags);
		virtual HRESULT __stdcall PurgeSoftDelete(ULONG ulDays);
		virtual HRESULT __stdcall PurgeDeferredUpdates(ULONG *lpulDeferredRemaining);
		virtual HRESULT __stdcall GetServerDetails(LPECSVRNAMELIST lpServerNameList, ULONG ulFlags, LPECSERVERLIST* lppsServerList);
		virtual HRESULT __stdcall OpenUserStoresTable(ULONG ulFlags, LPMAPITABLE *lppTable);
		virtual HRESULT __stdcall ResolvePseudoUrl(char *lpszPseudoUrl, char **lppszServerPath, bool *lpbIsPeer);
		virtual HRESULT __stdcall GetPublicStoreEntryID(ULONG ulFlags, ULONG* lpcbStoreID, LPENTRYID* lppStoreID);
		virtual HRESULT __stdcall GetArchiveStoreEntryID(LPCTSTR lpszUserName, LPCTSTR lpszServerName, ULONG ulFlags, ULONG* lpcbStoreID, LPENTRYID* lppStoreID);
		virtual HRESULT __stdcall ResetFolderCount(ULONG cbEntryId, LPENTRYID lpEntryId, ULONG *lpulUpdates);
	} m_xECServiceAdmin;

	class xECSpooler : public IECSpooler {
		// From IUnknown
		virtual HRESULT __stdcall QueryInterface(REFIID refiid, void** lppInterface);
		virtual ULONG __stdcall AddRef();
		virtual ULONG __stdcall Release();	

		// From IECSpooler
		virtual HRESULT __stdcall GetMasterOutgoingTable(ULONG ulFlags, IMAPITable ** lppOutgoingTable);
		virtual HRESULT __stdcall DeleteFromMasterOutgoingTable(ULONG cbEntryID, LPENTRYID lpEntryID, ULONG ulFlags);

	} m_xECSpooler;

	class xMAPIOfflineMgr : public IMAPIOfflineMgr {
		// IUnknown
		virtual ULONG __stdcall AddRef();
		virtual ULONG __stdcall Release();
		virtual HRESULT __stdcall QueryInterface(REFIID refiid, void **lppInterface);

		// IMAPIOffline
		virtual HRESULT __stdcall SetCurrentState(ULONG ulFlags, ULONG ulMask, ULONG ulState, void* pReserved);
		virtual HRESULT __stdcall GetCapabilities(ULONG *pulCapabilities);
		virtual HRESULT __stdcall GetCurrentState(ULONG* pulState);
		virtual HRESULT __stdcall Placeholder1(void*);

		// IMAPIOfflineMgr
		virtual HRESULT __stdcall Advise(ULONG ulFlags, MAPIOFFLINE_ADVISEINFO* pAdviseInfo, ULONG* pulAdviseToken);
		virtual HRESULT __stdcall Unadvise(ULONG ulFlags,ULONG ulAdviseToken);
		virtual HRESULT __stdcall Placeholder2();
		virtual HRESULT __stdcall Placeholder3();
		virtual HRESULT __stdcall Placeholder4();
		virtual HRESULT __stdcall Placeholder5();
		virtual HRESULT __stdcall Placeholder6();
		virtual HRESULT __stdcall Placeholder7();
		virtual HRESULT __stdcall Placeholder8();
	} m_xMAPIOfflineMgr;

	class xProxyStoreObject : public IProxyStoreObject {
		// IUnknown
		virtual ULONG __stdcall AddRef();
		virtual ULONG __stdcall Release();
		virtual HRESULT __stdcall QueryInterface(REFIID refiid, void **lppInterface);

		// IProxyStoreObject
		virtual HRESULT __stdcall PlaceHolder1();
		virtual HRESULT __stdcall UnwrapNoRef(LPVOID *ppvObject);
		virtual HRESULT __stdcall PlaceHolder2();
	}m_xProxyStoreObject;

	class xMsgStoreProxy : public IMsgStore {
		// IUnknown
		virtual ULONG __stdcall AddRef();
		virtual ULONG __stdcall Release();
		virtual HRESULT __stdcall QueryInterface(REFIID refiid, void **lppInterface);

		// IMsgStore
		virtual HRESULT __stdcall Advise(ULONG cbEntryID, LPENTRYID lpEntryID, ULONG ulEventMask, LPMAPIADVISESINK lpAdviseSink, ULONG *lpulConnection);
		virtual HRESULT __stdcall Unadvise(ULONG ulConnection);
		virtual HRESULT __stdcall CompareEntryIDs(ULONG cbEntryID1, LPENTRYID lpEntryID1, ULONG cbEntryID2, LPENTRYID lpEntryID2, ULONG ulFlags, ULONG *lpulResult);
		virtual HRESULT __stdcall OpenEntry(ULONG cbEntryID, LPENTRYID lpEntryID, LPCIID lpInterface, ULONG ulFlags, ULONG *lpulObjType, LPUNKNOWN *lppUnk);
		virtual HRESULT __stdcall SetReceiveFolder(LPTSTR lpszMessageClass, ULONG ulFlags, ULONG cbEntryID, LPENTRYID lpEntryID);
		virtual HRESULT __stdcall GetReceiveFolder(LPTSTR lpszMessageClass, ULONG ulFlags, ULONG *lpcbEntryID, LPENTRYID *lppEntryID, LPTSTR *lppszExplicitClass);
		virtual HRESULT __stdcall GetReceiveFolderTable(ULONG ulFlags, LPMAPITABLE *lppTable);
		virtual HRESULT __stdcall StoreLogoff(ULONG *lpulFlags);
		virtual HRESULT __stdcall AbortSubmit(ULONG cbEntryID, LPENTRYID lpEntryID, ULONG ulFlags);
		virtual HRESULT __stdcall GetOutgoingQueue(ULONG ulFlags, LPMAPITABLE *lppTable);
		virtual HRESULT __stdcall SetLockState(LPMESSAGE lpMessage,ULONG ulLockState);
		virtual HRESULT __stdcall FinishedMsg(ULONG ulFlags, ULONG cbEntryID, LPENTRYID lpEntryID);
		virtual HRESULT __stdcall NotifyNewMail(LPNOTIFICATION lpNotification);

		// IMAPIProp
		virtual HRESULT __stdcall GetLastError(HRESULT hError, ULONG ulFlags, LPMAPIERROR * lppMapiError);
		virtual HRESULT __stdcall SaveChanges(ULONG ulFlags);
		virtual HRESULT __stdcall GetProps(LPSPropTagArray lpPropTagArray, ULONG ulFlags, ULONG FAR * lpcValues, LPSPropValue FAR * lppPropArray);
		virtual HRESULT __stdcall GetPropList(ULONG ulFlags, LPSPropTagArray FAR * lppPropTagArray);
		virtual HRESULT __stdcall OpenProperty(ULONG ulPropTag, LPCIID lpiid, ULONG ulInterfaceOptions, ULONG ulFlags, LPUNKNOWN FAR * lppUnk);
		virtual HRESULT __stdcall SetProps(ULONG cValues, LPSPropValue lpPropArray, LPSPropProblemArray FAR * lppProblems);
		virtual HRESULT __stdcall DeleteProps(LPSPropTagArray lpPropTagArray, LPSPropProblemArray FAR * lppProblems);
		virtual HRESULT __stdcall CopyTo(ULONG ciidExclude, LPCIID rgiidExclude, LPSPropTagArray lpExcludeProps, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, LPCIID lpInterface, LPVOID lpDestObj, ULONG ulFlags, LPSPropProblemArray FAR * lppProblems);
		virtual HRESULT __stdcall CopyProps(LPSPropTagArray lpIncludeProps, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, LPCIID lpInterface, LPVOID lpDestObj, ULONG ulFlags, LPSPropProblemArray FAR * lppProblems);
		virtual HRESULT __stdcall GetNamesFromIDs(LPSPropTagArray * pptaga, LPGUID lpguid, ULONG ulFlags, ULONG * pcNames, LPMAPINAMEID ** pppNames);
		virtual HRESULT __stdcall GetIDsFromNames(ULONG cNames, LPMAPINAMEID * ppNames, ULONG ulFlags, LPSPropTagArray * pptaga);
	} m_xMsgStoreProxy;
	
	class xECMultiStoreTable : public IECMultiStoreTable {
	public:
		// IUnknown
		virtual ULONG __stdcall AddRef();
		virtual ULONG __stdcall Release();
		virtual HRESULT __stdcall QueryInterface(REFIID refiid, void **lppInterface);

		// IECMultiStoreTable
		virtual HRESULT __stdcall OpenMultiStoreTable(LPENTRYLIST lpMsgList, ULONG ulFlags, LPMAPITABLE *lppTable);
	} m_xECMultiStoreTable;

    class xECLicense : public IECLicense {
    public:
        // IUnknown
		virtual ULONG __stdcall AddRef();
		virtual ULONG __stdcall Release();
		virtual HRESULT __stdcall QueryInterface(REFIID refiid, void **lppInterface);

        virtual HRESULT __stdcall LicenseAuth(unsigned char *lpData, unsigned int ulSize, unsigned char **lpResponseData, unsigned int *lpulResponseSize);
        virtual HRESULT __stdcall LicenseCapa(unsigned int ulServiceType, char ***lppszCapas, unsigned int *lpulSize);
		virtual HRESULT __stdcall LicenseUsers(unsigned int ulServiceType, unsigned int *lpulUsers);
    } m_xECLicense;

    class xECTestProtocol : public IECTestProtocol {
    public:
        // IUnknown
		virtual ULONG __stdcall AddRef();
		virtual ULONG __stdcall Release();
		virtual HRESULT __stdcall QueryInterface(REFIID refiid, void **lppInterface);

		virtual HRESULT __stdcall TestPerform(char *szCommand, unsigned int ulArgs, char **szArgs);
		virtual HRESULT __stdcall TestSet(char *szName, char *szValue);
		virtual HRESULT __stdcall TestGet(char *szName, char **szValue);
    } m_xECTestProtocol;
	
public:
	LPMAPISUP			lpSupport;
	WSTransport*		lpTransport;
	ECNotifyClient*		m_lpNotifyClient;
	ECNamedProp*		lpNamedProp;
	ULONG				m_ulProfileFlags;
	MAPIUID				m_guidMDB_Provider;

	unsigned int		m_ulClientVersion;

private:
	BOOL				m_fIsSpooler;
	BOOL				m_fIsDefaultStore;
	BOOL				m_bOfflineStore;
	RELEASECALLBACK		lpfnCallback;
	ECUnknown*			lpCallbackObject;
	std::string			m_strProfname;
	std::set<ULONG>		m_setAdviseConnections;
};

class ECMSLogon : public ECUnknown {
private:
	ECMSLogon(ECMsgStore *lpStore);
	~ECMSLogon();

	ECMsgStore *m_lpStore;
	
public:
	static HRESULT Create(ECMsgStore *lpStore, ECMSLogon **lppECMSLogon);
	
	HRESULT QueryInterface(REFIID refiid, void **lppInterface);
	HRESULT GetLastError(HRESULT hResult, ULONG ulFlags, LPMAPIERROR *lppMAPIError);
	HRESULT Logoff(ULONG *lpulFlags);
	HRESULT OpenEntry(ULONG cbEntryID, LPENTRYID lpEntryID, LPCIID lpInterface, ULONG ulFlags, ULONG *lpulObjType, LPUNKNOWN *lppUnk);
	HRESULT CompareEntryIDs(ULONG cbEntryID1, LPENTRYID lpEntryID1, ULONG cbEntryID2, LPENTRYID lpEntryID2, ULONG ulFlags, ULONG *lpulResult);
	HRESULT Advise(ULONG cbEntryID, LPENTRYID lpEntryID, ULONG ulEventMask, LPMAPIADVISESINK lpAdviseSink, ULONG *lpulConnection);
	HRESULT Unadvise(ULONG ulConnection);
	HRESULT OpenStatusEntry(LPCIID lpInterface, ULONG ulFlags, ULONG *lpulObjType, LPVOID *lppEntry);
	
	class xMSLogon : public IMSLogon {
		// From IUnknown
		virtual HRESULT __stdcall QueryInterface(REFIID refiid, void** lppInterface);
		virtual ULONG __stdcall AddRef();
		virtual ULONG __stdcall Release();	

		// From IMSLogon
		virtual HRESULT __stdcall GetLastError(HRESULT hResult, ULONG ulFlags, LPMAPIERROR *lppMAPIError);
		virtual HRESULT __stdcall Logoff(ULONG *lpulFlags);
		virtual HRESULT __stdcall OpenEntry(ULONG cbEntryID, LPENTRYID lpEntryID, LPCIID lpInterface, ULONG ulFlags, ULONG *lpulObjType, LPUNKNOWN *lppUnk);
		virtual HRESULT __stdcall CompareEntryIDs(ULONG cbEntryID1, LPENTRYID lpEntryID1, ULONG cbEntryID2, LPENTRYID lpEntryID2, ULONG ulFlags, ULONG *lpulResult);
		virtual HRESULT __stdcall Advise(ULONG cbEntryID, LPENTRYID lpEntryID, ULONG ulEventMask, LPMAPIADVISESINK lpAdviseSink, ULONG *lpulConnection);
		virtual HRESULT __stdcall Unadvise(ULONG ulConnection);
		virtual HRESULT __stdcall OpenStatusEntry(LPCIID lpInterface, ULONG ulFlags, ULONG *lpulObjType, LPVOID *lppEntry);
	} m_xMSLogon;


};

#endif // ECMSGSTORE_H

