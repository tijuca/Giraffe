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

#ifndef __M4L_MAPIX_H_
#define __M4L_MAPIX_H_
#define MAPIX_H

/*
 * MAPI for linux
 *
 * mapix.h - Extended MAPI
 *
 * (C) Zarafa 2005
 *
 */

#include "platform.h"

/* Include common MAPI header files if they haven't been already. */
#include <mapidefs.h>
#include <mapicode.h>
#include <mapiguid.h>
#include <mapitags.h>


/* Forward interface declarations */
class IProfAdmin;
class IMsgServiceAdmin;
class IMAPISession;

typedef IProfAdmin* LPPROFADMIN;
typedef IMsgServiceAdmin* LPSERVICEADMIN;
typedef IMAPISession* LPMAPISESSION;


/* uhhh... already in mapi.h ? */
/* MAPILogon() flags.       */

//#define MAPI_LOGON_UI           0x00000001  /* Display logon UI                 */
//#define MAPI_NEW_SESSION        0x00000002  /* Don't use shared session         */
#define MAPI_ALLOW_OTHERS       0x00000008  /* Make this a shared session       */
#define MAPI_EXPLICIT_PROFILE   0x00000010  /* Don't use default profile        */
//#define MAPI_EXTENDED           0x00000020  /* Extended MAPI Logon              */
//#define MAPI_FORCE_DOWNLOAD     0x00001000  /* Get new mail before return       */
#define MAPI_SERVICE_UI_ALWAYS  0x00002000  /* Do logon UI in all providers     */
#define MAPI_NO_MAIL            0x00008000  /* Do not activate transports       */
/* #define MAPI_NT_SERVICE          0x00010000  Allow logon from an NT service  */
/* #ifndef MAPI_PASSWORD_UI */
/* #define MAPI_PASSWORD_UI        0x00020000  /\* Display password UI only         *\/ */
/* #endif */
#define MAPI_TIMEOUT_SHORT      0x00100000  /* Minimal wait for logon resources */

#define MAPI_SIMPLE_DEFAULT (MAPI_LOGON_UI | MAPI_FORCE_DOWNLOAD | MAPI_ALLOW_OTHERS)
#define MAPI_SIMPLE_EXPLICIT (MAPI_NEW_SESSION | MAPI_FORCE_DOWNLOAD | MAPI_EXPLICIT_PROFILE)

/* Structure passed to MAPIInitialize(), and its ulFlags values */

typedef struct
{
    ULONG           ulVersion;
    ULONG           ulFlags;
} MAPIINIT_0, *LPMAPIINIT_0;

typedef MAPIINIT_0 MAPIINIT;
typedef MAPIINIT *LPMAPIINIT;

#define MAPI_INIT_VERSION               0

#define MAPI_MULTITHREAD_NOTIFICATIONS  0x00000001
/* Reserved for MAPI                    0x40000000 */
#define MAPI_NT_SERVICE              0x00010000  /* Use from NT service */

/* MAPI base functions */

extern "C" {

typedef HRESULT (MAPIINITIALIZE)(LPVOID lpMapiInit);
typedef MAPIINITIALIZE* LPMAPIINITIALIZE;

typedef void (MAPIUNINITIALIZE)(void);
typedef MAPIUNINITIALIZE* LPMAPIUNINITIALIZE;

MAPIINITIALIZE      MAPIInitialize;
MAPIUNINITIALIZE    MAPIUninitialize;


/*  Extended MAPI Logon function */

typedef HRESULT (MAPILOGONEX)(
    ULONG ulUIParam,
    LPTSTR lpszProfileName,
    LPTSTR lpszPassword,
    ULONG ulFlags,
    LPMAPISESSION* lppSession
);
typedef MAPILOGONEX* LPMAPILOGONEX;
MAPILOGONEX MAPILogonEx;


typedef SCODE (MAPIALLOCATEBUFFER)(
    ULONG           cbSize,
    LPVOID *    lppBuffer
);
typedef SCODE (MAPIALLOCATEMORE)(
    ULONG           cbSize,
    LPVOID          lpObject,
    LPVOID *    lppBuffer
);
typedef ULONG (MAPIFREEBUFFER)(
    LPVOID          lpBuffer
);
typedef MAPIALLOCATEBUFFER  *LPMAPIALLOCATEBUFFER;
typedef MAPIALLOCATEMORE    *LPMAPIALLOCATEMORE;
typedef MAPIFREEBUFFER      *LPMAPIFREEBUFFER;
MAPIALLOCATEBUFFER MAPIAllocateBuffer;
MAPIALLOCATEMORE MAPIAllocateMore;
MAPIFREEBUFFER MAPIFreeBuffer;


typedef HRESULT (MAPIADMINPROFILES)(
    ULONG ulFlags,
    LPPROFADMIN *lppProfAdmin
);
typedef MAPIADMINPROFILES *LPMAPIADMINPROFILES;
MAPIADMINPROFILES MAPIAdminProfiles;

} // EXTERN "C"

/*
 * IMAPISession Interface
 */

/* Flags for OpenEntry and others */

/*#define MAPI_MODIFY               ((ULONG) 0x00000001) */

/* Flags for Logoff */

#define MAPI_LOGOFF_SHARED      0x00000001  /* Close all shared sessions    */
#define MAPI_LOGOFF_UI          0x00000002  /* It's OK to present UI        */

/* Flags for SetDefaultStore. They are mutually exclusive. */

#define MAPI_DEFAULT_STORE          0x00000001  /* for incoming messages */
#define MAPI_SIMPLE_STORE_TEMPORARY 0x00000002  /* for simple MAPI and CMC */
#define MAPI_SIMPLE_STORE_PERMANENT 0x00000003  /* for simple MAPI and CMC */
#define MAPI_PRIMARY_STORE          0x00000004  /* Used by some clients */
#define MAPI_SECONDARY_STORE        0x00000005  /* Used by some clients */

/* Flags for ShowForm. */

#define MAPI_POST_MESSAGE       0x00000001  /* Selects post/send semantics */
#define MAPI_NEW_MESSAGE        0x00000002  /* Governs copying during submission */

class IMAPISession : public IUnknown {
public:
    //    virtual ~IMAPISession() = 0;

    virtual HRESULT GetLastError(HRESULT hResult, ULONG ulFlags, LPMAPIERROR* lppMAPIError) = 0;
    virtual HRESULT GetMsgStoresTable(ULONG ulFlags, LPMAPITABLE* lppTable) = 0;
    virtual HRESULT OpenMsgStore(ULONG ulUIParam, ULONG cbEntryID, LPENTRYID lpEntryID, LPCIID lpInterface, ULONG ulFlags, LPMDB* lppMDB) = 0;
    virtual HRESULT OpenAddressBook(ULONG ulUIParam, LPCIID lpInterface, ULONG ulFlags, LPADRBOOK* lppAdrBook) = 0;
    virtual HRESULT OpenProfileSection(LPMAPIUID lpUID, LPCIID lpInterface, ULONG ulFlags, LPPROFSECT* lppProfSect) = 0;
    virtual HRESULT GetStatusTable(ULONG ulFlags, LPMAPITABLE* lppTable) = 0;
    virtual HRESULT OpenEntry(ULONG cbEntryID, LPENTRYID lpEntryID, LPCIID lpInterface, ULONG ulFlags, ULONG* lpulObjType,
		      LPUNKNOWN* lppUnk) = 0;
    virtual HRESULT CompareEntryIDs(ULONG cbEntryID1, LPENTRYID lpEntryID1, ULONG cbEntryID2, LPENTRYID lpEntryID2, ULONG ulFlags,
			    ULONG* lpulResult) = 0;
    virtual HRESULT Advise(ULONG cbEntryID, LPENTRYID lpEntryID, ULONG ulEventMask, LPMAPIADVISESINK lpAdviseSink, ULONG* lpulConnection) = 0;
    virtual HRESULT Unadvise(ULONG ulConnection) = 0;
    virtual HRESULT MessageOptions(ULONG ulUIParam, ULONG ulFlags, LPTSTR lpszAdrType, LPMESSAGE lpMessage) = 0;
    virtual HRESULT QueryDefaultMessageOpt(LPTSTR lpszAdrType, ULONG ulFlags, ULONG* lpcValues, LPSPropValue* lppOptions) = 0;
    virtual HRESULT EnumAdrTypes(ULONG ulFlags, ULONG* lpcAdrTypes, LPTSTR** lpppszAdrTypes) = 0;
    virtual HRESULT QueryIdentity(ULONG* lpcbEntryID, LPENTRYID* lppEntryID) = 0;
    virtual HRESULT Logoff(ULONG ulUIParam, ULONG ulFlags, ULONG ulReserved) = 0;
    virtual HRESULT SetDefaultStore(ULONG ulFlags, ULONG cbEntryID, LPENTRYID lpEntryID) = 0;
    virtual HRESULT AdminServices(ULONG ulFlags, LPSERVICEADMIN* lppServiceAdmin) = 0;
    virtual HRESULT ShowForm(ULONG ulUIParam, LPMDB lpMsgStore, LPMAPIFOLDER lpParentFolder, LPCIID lpInterface, ULONG ulMessageToken,
			     LPMESSAGE lpMessageSent, ULONG ulFlags, ULONG ulMessageStatus, ULONG ulMessageFlags, ULONG ulAccess,
			     LPSTR lpszMessageClass) = 0;
    virtual HRESULT PrepareForm(LPCIID lpInterface, LPMESSAGE lpMessage, ULONG* lpulMessageToken) = 0;
};


/*DECLARE_MAPI_INTERFACE_PTR(IMAPISession, LPMAPISESSION);*/

/* IAddrBook Interface ----------------------------------------------------- */


class IAddrBook : public IMAPIProp {
public:
    //    virtual ~IAddrBook() = 0;

    virtual HRESULT OpenEntry(ULONG cbEntryID, LPENTRYID lpEntryID, LPCIID lpInterface, ULONG ulFlags, ULONG * lpulObjType,
		      LPUNKNOWN * lppUnk) = 0;
    virtual HRESULT CompareEntryIDs(ULONG cbEntryID1, LPENTRYID lpEntryID1, ULONG cbEntryID2, LPENTRYID lpEntryID2, ULONG ulFlags,
			    ULONG* lpulResult) = 0;
    virtual HRESULT Advise(ULONG cbEntryID, LPENTRYID lpEntryID, ULONG ulEventMask, LPMAPIADVISESINK lpAdviseSink, ULONG* lpulConnection) = 0;
    virtual HRESULT Unadvise(ULONG ulConnection) = 0;
    virtual HRESULT CreateOneOff(LPTSTR lpszName, LPTSTR lpszAdrType, LPTSTR lpszAddress, ULONG ulFlags, ULONG* lpcbEntryID,
			 LPENTRYID* lppEntryID) = 0;
    virtual HRESULT NewEntry(ULONG ulUIParam, ULONG ulFlags, ULONG cbEIDContainer, LPENTRYID lpEIDContainer, ULONG cbEIDNewEntryTpl,
		     LPENTRYID lpEIDNewEntryTpl, ULONG* lpcbEIDNewEntry, LPENTRYID* lppEIDNewEntry) = 0;
    virtual HRESULT ResolveName(ULONG ulUIParam, ULONG ulFlags, LPTSTR lpszNewEntryTitle, LPADRLIST lpAdrList) = 0;
    virtual HRESULT Address(ULONG* lpulUIParam, LPADRPARM lpAdrParms, LPADRLIST* lppAdrList) = 0;
    virtual HRESULT Details(ULONG* lpulUIParam, LPFNDISMISS lpfnDismiss, LPVOID lpvDismissContext, ULONG cbEntryID, LPENTRYID lpEntryID,
		    LPFNBUTTON lpfButtonCallback, LPVOID lpvButtonContext, LPTSTR lpszButtonText, ULONG ulFlags) = 0;
    virtual HRESULT RecipOptions(ULONG ulUIParam, ULONG ulFlags, LPADRENTRY lpRecip) = 0;
    virtual HRESULT QueryDefaultRecipOpt(LPTSTR lpszAdrType, ULONG ulFlags, ULONG* lpcValues, LPSPropValue* lppOptions) = 0;
    virtual HRESULT GetPAB(ULONG* lpcbEntryID, LPENTRYID* lppEntryID) = 0;
    virtual HRESULT SetPAB(ULONG cbEntryID, LPENTRYID lpEntryID) = 0;
    virtual HRESULT GetDefaultDir(ULONG* lpcbEntryID, LPENTRYID* lppEntryID) = 0;
    virtual HRESULT SetDefaultDir(ULONG cbEntryID, LPENTRYID lpEntryID) = 0;
    virtual HRESULT GetSearchPath(ULONG ulFlags, LPSRowSet* lppSearchPath) = 0;
    virtual HRESULT SetSearchPath(ULONG ulFlags, LPSRowSet lpSearchPath) = 0;
    virtual HRESULT PrepareRecips(ULONG ulFlags, LPSPropTagArray lpPropTagArray, LPADRLIST lpRecipList) = 0;
};

typedef IAddrBook* LPADRBOOK;

/*
 * IProfAdmin Interface
 */

#define MAPI_DEFAULT_SERVICES           0x00000001


class IProfAdmin : public IUnknown {
public:
    //    virtual ~IProfAdmin() = 0;

    virtual HRESULT GetLastError(HRESULT hResult, ULONG ulFlags, LPMAPIERROR* lppMAPIError) = 0;
    virtual HRESULT GetProfileTable(ULONG ulFlags, LPMAPITABLE* lppTable) = 0;
    virtual HRESULT CreateProfile(LPTSTR lpszProfileName, LPTSTR lpszPassword, ULONG ulUIParam, ULONG ulFlags) = 0;
    virtual HRESULT DeleteProfile(LPTSTR lpszProfileName, ULONG ulFlags) = 0;
    virtual HRESULT ChangeProfilePassword(LPTSTR lpszProfileName, LPTSTR lpszOldPassword, LPTSTR lpszNewPassword, ULONG ulFlags) = 0;
    virtual HRESULT CopyProfile(LPTSTR lpszOldProfileName, LPTSTR lpszOldPassword, LPTSTR lpszNewProfileName, ULONG ulUIParam,
				ULONG ulFlags) = 0;
    virtual HRESULT RenameProfile(LPTSTR lpszOldProfileName, LPTSTR lpszOldPassword, LPTSTR lpszNewProfileName, ULONG ulUIParam,
				  ULONG ulFlags) = 0;
    virtual HRESULT SetDefaultProfile(LPTSTR lpszProfileName, ULONG ulFlags) = 0;
    virtual HRESULT AdminServices(LPTSTR lpszProfileName, LPTSTR lpszPassword, ULONG ulUIParam, ULONG ulFlags,
				  LPSERVICEADMIN* lppServiceAdmin) = 0;
};



/*
 * IMsgServiceAdmin Interface
 */

/* Values for PR_RESOURCE_FLAGS in message service table */

#define SERVICE_DEFAULT_STORE       0x00000001
#define SERVICE_SINGLE_COPY         0x00000002
#define SERVICE_CREATE_WITH_STORE   0x00000004
#define SERVICE_PRIMARY_IDENTITY    0x00000008
#define SERVICE_NO_PRIMARY_IDENTITY 0x00000020


class IMsgServiceAdmin : public IUnknown {
public:
    //    virtual ~IMsgServiceAdmin() = 0;

    virtual HRESULT GetLastError(HRESULT hResult, ULONG ulFlags, LPMAPIERROR* lppMAPIError) = 0;
    virtual HRESULT GetMsgServiceTable(ULONG ulFlags, LPMAPITABLE* lppTable) = 0;
    virtual HRESULT CreateMsgService(LPTSTR lpszService, LPTSTR lpszDisplayName, ULONG ulUIParam, ULONG ulFlags) = 0;
    virtual HRESULT DeleteMsgService(LPMAPIUID lpUID) = 0;
    virtual HRESULT CopyMsgService(LPMAPIUID lpUID, LPTSTR lpszDisplayName, LPCIID lpInterfaceToCopy, LPCIID lpInterfaceDst,
				   LPVOID lpObjectDst, ULONG ulUIParam, ULONG ulFlags) = 0;
    virtual HRESULT RenameMsgService(LPMAPIUID lpUID, ULONG ulFlags, LPTSTR lpszDisplayName) = 0;
    virtual HRESULT ConfigureMsgService(LPMAPIUID lpUID, ULONG ulUIParam, ULONG ulFlags, ULONG cValues, LPSPropValue lpProps) = 0;
    virtual HRESULT OpenProfileSection(LPMAPIUID lpUID, LPCIID lpInterface, ULONG ulFlags, LPPROFSECT* lppProfSect) = 0;
    virtual HRESULT MsgServiceTransportOrder(ULONG cUID, LPMAPIUID lpUIDList, ULONG ulFlags) = 0;
    virtual HRESULT AdminProviders(LPMAPIUID lpUID, ULONG ulFlags, LPPROVIDERADMIN* lppProviderAdmin) = 0;
    virtual HRESULT SetPrimaryIdentity(LPMAPIUID lpUID, ULONG ulFlags) = 0;
    virtual HRESULT GetProviderTable(ULONG ulFlags, LPMAPITABLE* lppTable) = 0;
};


#endif /* MAPIX_H */
