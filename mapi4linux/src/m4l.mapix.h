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

#ifndef __M4L_MAPIX_IMPL_H
#define __M4L_MAPIX_IMPL_H

#include <pthread.h>

#include "m4l.common.h"
#include "m4l.mapidefs.h"
#include "m4l.mapisvc.h"
#include <mapix.h>
#include <mapispi.h>
#include <string>
#include <list>
#include <map>

#include  <ECConfig.h>

using namespace std;

class M4LMsgServiceAdmin;

typedef struct _s_providerentry {
	MAPIUID uid;
	string servicename; // this provider belongs to service 'servicename'
	M4LProfSect *profilesection;
} providerEntry;

typedef struct _s_serviceentry {
    MAPIUID muid;
    string servicename;
	string displayname;
	M4LProviderAdmin *provideradmin;
	bool bInitialize;
	SVCService* service;
} serviceEntry;

typedef struct _s_profentry {
    string profname;
    string password;
    M4LMsgServiceAdmin *serviceadmin;
} profEntry;


class M4LProfAdmin : public M4LUnknown, public IProfAdmin {
private:
    // variables
    list<profEntry*> profiles;
    pthread_mutex_t m_mutexProfiles;

    // functions
    list<profEntry*>::iterator findProfile(LPTSTR lpszProfileName);

public:
    M4LProfAdmin();
    virtual ~M4LProfAdmin();

    virtual HRESULT __stdcall GetLastError(HRESULT hResult, ULONG ulFlags, LPMAPIERROR* lppMAPIError);
    virtual HRESULT __stdcall GetProfileTable(ULONG ulFlags, LPMAPITABLE* lppTable);
    virtual HRESULT __stdcall CreateProfile(LPTSTR lpszProfileName, LPTSTR lpszPassword, ULONG ulUIParam, ULONG ulFlags);
    virtual HRESULT __stdcall DeleteProfile(LPTSTR lpszProfileName, ULONG ulFlags);
    virtual HRESULT __stdcall ChangeProfilePassword(LPTSTR lpszProfileName, LPTSTR lpszOldPassword, LPTSTR lpszNewPassword, ULONG ulFlags);
    virtual HRESULT __stdcall CopyProfile(LPTSTR lpszOldProfileName, LPTSTR lpszOldPassword, LPTSTR lpszNewProfileName, ULONG ulUIParam,
				ULONG ulFlags);
    virtual HRESULT __stdcall RenameProfile(LPTSTR lpszOldProfileName, LPTSTR lpszOldPassword, LPTSTR lpszNewProfileName, ULONG ulUIParam,
				  ULONG ulFlags);
    virtual HRESULT __stdcall SetDefaultProfile(LPTSTR lpszProfileName, ULONG ulFlags);
    virtual HRESULT __stdcall AdminServices(LPTSTR lpszProfileName, LPTSTR lpszPassword, ULONG ulUIParam, ULONG ulFlags,
				  LPSERVICEADMIN* lppServiceAdmin);

    // iunknown passthru
    virtual ULONG __stdcall AddRef();
    virtual ULONG __stdcall Release();
    virtual HRESULT __stdcall QueryInterface(REFIID refiid, void **lpvoid);
};


class M4LMsgServiceAdmin : public M4LUnknown, public IMsgServiceAdmin {
private:

	list<providerEntry*> providers;
    list<serviceEntry*> services;

	M4LProfSect	*profilesection;  // Global Profile Section

	pthread_mutex_t m_mutexserviceadmin;

    // functions
    serviceEntry* findServiceAdmin(LPTSTR lpszServiceName);
    serviceEntry* findServiceAdmin(LPMAPIUID lpMUID);
	providerEntry* findProvider(LPMAPIUID lpUid);

public:
    M4LMsgServiceAdmin(M4LProfSect *profilesection);
    virtual ~M4LMsgServiceAdmin();

    virtual HRESULT __stdcall GetLastError(HRESULT hResult, ULONG ulFlags, LPMAPIERROR* lppMAPIError);
    virtual HRESULT __stdcall GetMsgServiceTable(ULONG ulFlags, LPMAPITABLE* lppTable);
    virtual HRESULT __stdcall CreateMsgService(LPTSTR lpszService, LPTSTR lpszDisplayName, ULONG ulUIParam, ULONG ulFlags);
    virtual HRESULT __stdcall DeleteMsgService(LPMAPIUID lpUID);
    virtual HRESULT __stdcall CopyMsgService(LPMAPIUID lpUID, LPTSTR lpszDisplayName, LPCIID lpInterfaceToCopy, LPCIID lpInterfaceDst,
								   LPVOID lpObjectDst, ULONG ulUIParam, ULONG ulFlags);
    virtual HRESULT __stdcall RenameMsgService(LPMAPIUID lpUID, ULONG ulFlags, LPTSTR lpszDisplayName);
    virtual HRESULT __stdcall ConfigureMsgService(LPMAPIUID lpUID, ULONG ulUIParam, ULONG ulFlags, ULONG cValues, LPSPropValue lpProps);
    virtual HRESULT __stdcall OpenProfileSection(LPMAPIUID lpUID, LPCIID lpInterface, ULONG ulFlags, LPPROFSECT* lppProfSect);
    virtual HRESULT __stdcall MsgServiceTransportOrder(ULONG cUID, LPMAPIUID lpUIDList, ULONG ulFlags);
    virtual HRESULT __stdcall AdminProviders(LPMAPIUID lpUID, ULONG ulFlags, LPPROVIDERADMIN* lppProviderAdmin);
    virtual HRESULT __stdcall SetPrimaryIdentity(LPMAPIUID lpUID, ULONG ulFlags);
    virtual HRESULT __stdcall GetProviderTable(ULONG ulFlags, LPMAPITABLE* lppTable);

    // iunknown passthru
    virtual ULONG __stdcall AddRef();
    virtual ULONG __stdcall Release();
    virtual HRESULT __stdcall QueryInterface(REFIID refiid, void **lpvoid);

	friend class M4LProviderAdmin;
	friend class M4LMAPISession;
};

inline bool operator <(const GUID &a, const GUID &b) {
    return memcmp(&a, &b, sizeof(GUID)) < 0;
}

class M4LMAPISession : public M4LUnknown, public IMAPISession {
private:
	// variables
	string profileName;
	M4LMsgServiceAdmin *serviceAdmin;

public:
	M4LMAPISession(LPTSTR new_profileName, M4LMsgServiceAdmin *new_serviceAdmin);
	virtual ~M4LMAPISession();

	virtual HRESULT __stdcall GetLastError(HRESULT hResult, ULONG ulFlags, LPMAPIERROR* lppMAPIError);
	virtual HRESULT __stdcall GetMsgStoresTable(ULONG ulFlags, LPMAPITABLE* lppTable);
	virtual HRESULT __stdcall OpenMsgStore(ULONG ulUIParam, ULONG cbEntryID, LPENTRYID lpEntryID, LPCIID lpInterface, ULONG ulFlags,
								 LPMDB* lppMDB);
	virtual HRESULT __stdcall OpenAddressBook(ULONG ulUIParam, LPCIID lpInterface, ULONG ulFlags, LPADRBOOK* lppAdrBook);
	virtual HRESULT __stdcall OpenProfileSection(LPMAPIUID lpUID, LPCIID lpInterface, ULONG ulFlags, LPPROFSECT* lppProfSect);
	virtual HRESULT __stdcall GetStatusTable(ULONG ulFlags, LPMAPITABLE* lppTable);
	virtual HRESULT __stdcall OpenEntry(ULONG cbEntryID, LPENTRYID lpEntryID, LPCIID lpInterface, ULONG ulFlags, ULONG* lpulObjType,
							  LPUNKNOWN* lppUnk);
	virtual HRESULT __stdcall CompareEntryIDs(ULONG cbEntryID1, LPENTRYID lpEntryID1, ULONG cbEntryID2, LPENTRYID lpEntryID2, ULONG ulFlags,
									ULONG* lpulResult);
	virtual HRESULT __stdcall Advise(ULONG cbEntryID, LPENTRYID lpEntryID, ULONG ulEventMask, LPMAPIADVISESINK lpAdviseSink,
						   ULONG* lpulConnection);
	virtual HRESULT __stdcall Unadvise(ULONG ulConnection);
	virtual HRESULT __stdcall MessageOptions(ULONG ulUIParam, ULONG ulFlags, LPTSTR lpszAdrType, LPMESSAGE lpMessage);
	virtual HRESULT __stdcall QueryDefaultMessageOpt(LPTSTR lpszAdrType, ULONG ulFlags, ULONG* lpcValues, LPSPropValue* lppOptions);
	virtual HRESULT __stdcall EnumAdrTypes(ULONG ulFlags, ULONG* lpcAdrTypes, LPTSTR** lpppszAdrTypes);
	virtual HRESULT __stdcall QueryIdentity(ULONG* lpcbEntryID, LPENTRYID* lppEntryID);
	virtual HRESULT __stdcall Logoff(ULONG ulUIParam, ULONG ulFlags, ULONG ulReserved);
	virtual HRESULT __stdcall SetDefaultStore(ULONG ulFlags, ULONG cbEntryID, LPENTRYID lpEntryID);
	virtual HRESULT __stdcall AdminServices(ULONG ulFlags, LPSERVICEADMIN* lppServiceAdmin);
	virtual HRESULT __stdcall ShowForm(ULONG ulUIParam, LPMDB lpMsgStore, LPMAPIFOLDER lpParentFolder, LPCIID lpInterface, ULONG ulMessageToken,
							 LPMESSAGE lpMessageSent, ULONG ulFlags, ULONG ulMessageStatus, ULONG ulMessageFlags, ULONG ulAccess,
							 LPSTR lpszMessageClass);
	virtual HRESULT __stdcall PrepareForm(LPCIID lpInterface, LPMESSAGE lpMessage, ULONG* lpulMessageToken);

    // iunknown passthru
	virtual ULONG __stdcall AddRef();
	virtual ULONG __stdcall Release();
	virtual HRESULT __stdcall QueryInterface(REFIID refiid, void **lpvoid);

	ULONG cValues;
	LPSPropValue lpProps;

private:
    std::map<GUID, IMsgStore *> mapStores;
};


class M4LAddrBook : public M4LMAPIProp, public IAddrBook {
public:
	M4LAddrBook(M4LMsgServiceAdmin *new_serviceAdmin, LPMAPISUP newlpMAPISup);
	virtual ~M4LAddrBook();

	virtual HRESULT __stdcall OpenEntry(ULONG cbEntryID, LPENTRYID lpEntryID, LPCIID lpInterface, ULONG ulFlags, ULONG * lpulObjType,
										LPUNKNOWN * lppUnk);
	virtual HRESULT __stdcall CompareEntryIDs(ULONG cbEntryID1, LPENTRYID lpEntryID1, ULONG cbEntryID2, LPENTRYID lpEntryID2,
											  ULONG ulFlags, ULONG* lpulResult);
	virtual HRESULT __stdcall Advise(ULONG cbEntryID, LPENTRYID lpEntryID, ULONG ulEventMask, LPMAPIADVISESINK lpAdviseSink,
									 ULONG* lpulConnection);
	virtual HRESULT __stdcall Unadvise(ULONG ulConnection);
	virtual HRESULT __stdcall CreateOneOff(LPTSTR lpszName, LPTSTR lpszAdrType, LPTSTR lpszAddress, ULONG ulFlags, ULONG* lpcbEntryID,
										   LPENTRYID* lppEntryID);
	virtual HRESULT __stdcall NewEntry(ULONG ulUIParam, ULONG ulFlags, ULONG cbEIDContainer, LPENTRYID lpEIDContainer,
									   ULONG cbEIDNewEntryTpl, LPENTRYID lpEIDNewEntryTpl, ULONG* lpcbEIDNewEntry,
									   LPENTRYID* lppEIDNewEntry);
	virtual HRESULT __stdcall ResolveName(ULONG ulUIParam, ULONG ulFlags, LPTSTR lpszNewEntryTitle, LPADRLIST lpAdrList);
	virtual HRESULT __stdcall Address(ULONG* lpulUIParam, LPADRPARM lpAdrParms, LPADRLIST* lppAdrList);
	virtual HRESULT __stdcall Details(ULONG* lpulUIParam, LPFNDISMISS lpfnDismiss, LPVOID lpvDismissContext, ULONG cbEntryID,
									  LPENTRYID lpEntryID, LPFNBUTTON lpfButtonCallback, LPVOID lpvButtonContext,
									  LPTSTR lpszButtonText, ULONG ulFlags);
	virtual HRESULT __stdcall RecipOptions(ULONG ulUIParam, ULONG ulFlags, LPADRENTRY lpRecip);
	virtual HRESULT __stdcall QueryDefaultRecipOpt(LPTSTR lpszAdrType, ULONG ulFlags, ULONG* lpcValues, LPSPropValue* lppOptions);
	virtual HRESULT __stdcall GetPAB(ULONG* lpcbEntryID, LPENTRYID* lppEntryID);
	virtual HRESULT __stdcall SetPAB(ULONG cbEntryID, LPENTRYID lpEntryID);
	virtual HRESULT __stdcall GetDefaultDir(ULONG* lpcbEntryID, LPENTRYID* lppEntryID);
	virtual HRESULT __stdcall SetDefaultDir(ULONG cbEntryID, LPENTRYID lpEntryID);
	virtual HRESULT __stdcall GetSearchPath(ULONG ulFlags, LPSRowSet* lppSearchPath);
	virtual HRESULT __stdcall SetSearchPath(ULONG ulFlags, LPSRowSet lpSearchPath);
	virtual HRESULT __stdcall PrepareRecips(ULONG ulFlags, LPSPropTagArray lpPropTagArray, LPADRLIST lpRecipList);

	// imapiprop passthru
	virtual HRESULT __stdcall GetLastError(HRESULT hResult, ULONG ulFlags, LPMAPIERROR* lppMAPIError);
	virtual HRESULT __stdcall SaveChanges(ULONG ulFlags);
	virtual HRESULT __stdcall GetProps(LPSPropTagArray lpPropTagArray, ULONG ulFlags, ULONG* lpcValues, LPSPropValue* lppPropArray);
	virtual HRESULT __stdcall GetPropList(ULONG ulFlags, LPSPropTagArray* lppPropTagArray);
	virtual HRESULT __stdcall OpenProperty(ULONG ulPropTag, LPCIID lpiid, ULONG ulInterfaceOptions, ULONG ulFlags, LPUNKNOWN* lppUnk);
	virtual HRESULT __stdcall SetProps(ULONG cValues, LPSPropValue lpPropArray, LPSPropProblemArray* lppProblems);
	virtual HRESULT __stdcall DeleteProps(LPSPropTagArray lpPropTagArray, LPSPropProblemArray* lppProblems);
	virtual HRESULT __stdcall CopyTo(ULONG ciidExclude, LPCIID rgiidExclude, LPSPropTagArray lpExcludeProps, ULONG ulUIParam,
									 LPMAPIPROGRESS lpProgress, LPCIID lpInterface, LPVOID lpDestObj, ULONG ulFlags,
									 LPSPropProblemArray* lppProblems);
	virtual HRESULT __stdcall CopyProps(LPSPropTagArray lpIncludeProps, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, LPCIID lpInterface,
										LPVOID lpDestObj, ULONG ulFlags, LPSPropProblemArray* lppProblems);
	virtual HRESULT __stdcall GetNamesFromIDs(LPSPropTagArray* lppPropTags, LPGUID lpPropSetGuid, ULONG ulFlags, ULONG* lpcPropNames,
											  LPMAPINAMEID** lpppPropNames);
	virtual HRESULT __stdcall GetIDsFromNames(ULONG cPropNames, LPMAPINAMEID* lppPropNames, ULONG ulFlags, LPSPropTagArray* lppPropTags);

	// iunknown passthru
	virtual ULONG __stdcall AddRef();
	virtual ULONG __stdcall Release();
	virtual HRESULT __stdcall QueryInterface(REFIID refiid, void **lpvoid);

private:
	// variables
	M4LMsgServiceAdmin *serviceAdmin; /* from session object */
	LPMAPISUP m_lpMAPISup;

	std::list<abEntry> m_lABProviders;

	LPSRowSet m_lpSavedSearchPath;
	HRESULT getDefaultSearchPath(ULONG ulFlags, LPSRowSet* lppSearchPath);

public:
	HRESULT __stdcall addProvider(const std::string &profilename, const std::string &displayname, LPMAPIUID lpUID, LPABPROVIDER newProvider);
};

extern ECConfig *m4l_lpConfig;

#endif
