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

#ifndef COMMONUTIL_H
#define COMMONUTIL_H

#include <mapidefs.h>
#include <mapix.h>
#include <string>
#include "ECTags.h"
#include "charset/utf32string.h"
#include <list>

#include "ustringutil.h"

class ECLogger;

// Newmail Notify columns
const static SizedSPropTagArray(4, sPropNewMailColumns) =
{
	4,
	{
		PR_ENTRYID,
		PR_PARENT_ENTRYID,
		PR_MESSAGE_CLASS_A,
		PR_MESSAGE_FLAGS
	}
};

// Indexes of the sPropNewMailColumns property array
enum
{
	NEWMAIL_ENTRYID,		// Array Indexes
	NEWMAIL_PARENT_ENTRYID,
	NEWMAIL_MESSAGE_CLASS,
	NEWMAIL_MESSAGE_FLAGS,
	NUM_NEWMAIL_PROPS		// Array size
};

// Version of GetClientVersion
#define CLIENT_VERSION_OLK2000			9
#define CLIENT_VERSION_OLK2002			10
#define CLIENT_VERSION_OLK2003			11
#define CLIENT_VERSION_OLK2007			12
#define CLIENT_VERSION_OLK2010			14
#define CLIENT_VERSION_LATEST			CLIENT_VERSION_OLK2010 /* UPDATE ME */

/* darn, no sane place because of depend include on mapidefs.h */
bool operator ==(SBinary a, SBinary b);
bool operator <(SBinary a, SBinary b);

#define CLIENT_ADMIN_SOCKET "file:///var/run/zarafa"
char* GetServerUnixSocket(char* szPreferred = NULL);
std::string GetServerFQDN();

HRESULT HrOpenECAdminSession(IMAPISession **lppSession, const char *szPath = NULL, ULONG ulProfileFlags = 0, const char *sslkey_file = NULL, const char *sslkey_password = NULL);
HRESULT HrOpenECSession(IMAPISession **lppSession, const WCHAR *szUsername, const WCHAR *szPassword, const char *szPath = NULL, ULONG ulProfileFlags = 0, const char *sslkey_file = NULL, const char *sslkey_password = NULL, const char *profname = NULL);
HRESULT HrSearchECStoreEntryId(IMAPISession *lpMAPISession, BOOL bPublic, ULONG *lpcbEntryID, LPENTRYID *lppEntryID);
HRESULT HrOpenDefaultStore(IMAPISession *lpMAPISession, IMsgStore **lppMsgStore);
HRESULT HrOpenDefaultStore(IMAPISession *lpMAPISession, ULONG ulFlags, IMsgStore **lppMsgStore);
HRESULT HrOpenECPublicStore(IMAPISession *lpMAPISession, IMsgStore **lppMsgStore);
HRESULT HrOpenECPublicStore(IMAPISession *lpMAPISession, ULONG ulFlags, IMsgStore **lppMsgStore);

HRESULT HrGetECProviderAdmin(LPMAPISESSION lpSession, LPPROVIDERADMIN *lppProviderAdmin);

HRESULT HrOpenDefaultStoreOffline(IMAPISession *lpMAPISession, IMsgStore **lppMsgStore);
HRESULT HrOpenDefaultStoreOnline(IMAPISession *lpMAPISession, IMsgStore **lppMsgStore);
HRESULT HrOpenStoreOnline(IMAPISession *lpMAPISession, ULONG cbEntryID, LPENTRYID lpEntryID, IMsgStore **lppMsgStore);
HRESULT HrOpenECPublicStoreOnline(IMAPISession *lpMAPISession, IMsgStore **lppMsgStore);

HRESULT GetProxyStoreObject(IMsgStore *lpMsgStore, IMsgStore **lppMsgStore);

HRESULT HrAddECMailBox(LPMAPISESSION lpSession, LPCWSTR lpszUserName);
HRESULT HrAddECMailBox(LPPROVIDERADMIN lpProviderAdmin, LPCWSTR lpszUserName);

HRESULT HrAddArchiveMailBox(LPPROVIDERADMIN lpProviderAdmin, LPCWSTR lpszUserName, LPCWSTR lpszServerName, LPMAPIUID lpProviderUID);

HRESULT HrRemoveECMailBox(LPMAPISESSION lpSession, LPMAPIUID lpsProviderUID);
HRESULT HrRemoveECMailBox(LPPROVIDERADMIN lpProviderAdmin, LPMAPIUID lpsProviderUID);

HRESULT ECCreateOneOff(LPTSTR lpszName, LPTSTR lpszAdrType, LPTSTR lpszAddress, ULONG ulFlags, ULONG* lpcbEntryID, LPENTRYID* lppEntryID);
HRESULT ECParseOneOff(LPENTRYID lpEntryID, ULONG cbEntryID, std::wstring &strWName, std::wstring &strWType, std::wstring &strWAddress);

HRESULT HrGetAddress(IMAPISession *lpSession, LPSPropValue lpProps, ULONG cValues, ULONG ulPropTagEntryID, ULONG ulPropTagName, ULONG ulPropTagType, ULONG ulPropTagEmailAddress,
					 std::wstring &strName, std::wstring &strType, std::wstring &strEmailAddress);
HRESULT HrGetAddress(IMAPISession *lpSession, IMessage *lpMessage, ULONG ulPropTagEntryID, ULONG ulPropTagName, ULONG ulPropTagType, ULONG ulPropTagEmailAddress,
					 std::wstring &strName, std::wstring &strType, std::wstring &strEmailAddress);
HRESULT HrGetAddress(LPADRBOOK lpAdrBook, IMessage *lpMessage, ULONG ulPropTagEntryID, ULONG ulPropTagName, ULONG ulPropTagType, ULONG ulPropTagEmailAddress,
					 std::wstring &strName, std::wstring &strType, std::wstring &strEmailAddress);
HRESULT HrGetAddress(LPADRBOOK lpAdrBook, LPSPropValue lpProps, ULONG cValues, ULONG ulPropTagEntryID, ULONG ulPropTagName, ULONG ulPropTagType, ULONG ulPropTagEmailAddress,
					 std::wstring &strName, std::wstring &strType, std::wstring &strEmailAddress);
HRESULT HrGetAddress(LPADRBOOK lpAdrBook, LPENTRYID lpEntryID, ULONG cbEntryID, std::wstring &strName, std::wstring &strType, std::wstring &strEmailAddress);

std::string ToQuotedPrintable(const std::string &input, std::string charset, bool header = true, bool imap = false);
std::string ToQuotedBase64Header(const std::string &input, std::string charset);
std::string ToQuotedBase64Header(const std::wstring &input);

HRESULT HrNewMailNotification(IMsgStore* lpMDB, IMessage* lpMessage);
HRESULT HrCreateEmailSearchKey(char* lpszEmailType, char* lpszEmail, ULONG* cb, LPBYTE* lppByte);

HRESULT DoSentMail(IMAPISession *lpSession, IMsgStore *lpMDB, ULONG ulFlags, IMessage *lpMessage);

HRESULT TestRestriction(LPSRestriction lpCondition, ULONG cValues, LPSPropValue lpPropVals, const ECLocale &locale, ULONG ulLevel = 0);
HRESULT TestRestriction(LPSRestriction lpCondition, IMAPIProp *lpMessage, const ECLocale &locale, ULONG ulLevel = 0);

HRESULT GetClientVersion(unsigned int* ulVersion);

HRESULT CreateProfileTemp(const WCHAR *username, const WCHAR *password, const char *path, const char* szProfName, ULONG ulProfileFlags,
						  const char *sslkey_file, const char *sslkey_password);
HRESULT DeleteProfileTemp(char *szProfName);

HRESULT OpenSubFolder(LPMDB lpMDB, const WCHAR *folder, WCHAR psep, ECLogger *lpLogger, bool bIsPublic, bool bCreateFolder, LPMAPIFOLDER *lppSubFolder);
HRESULT FindFolder(LPMAPITABLE lpTable, const WCHAR *folder, LPSPropValue *lppFolderProp);

HRESULT HrOpenUserMsgStore(LPMAPISESSION lpSession, WCHAR *lpszWUser, LPMDB *lppStore);

HRESULT HrOpenDefaultCalendar(LPMDB lpMsgStore, ECLogger *lpLogger, LPMAPIFOLDER *lpDefFolder);

HRESULT HrGetPropTags(char **names, IMAPIProp *lpProp, LPSPropTagArray *lppPropTagArray);

HRESULT HrGetAllProps(IMAPIProp *lpProp, ULONG ulFlags, ULONG *lpcValues, LPSPropValue *lppProps);

HRESULT __stdcall UnWrapStoreEntryID(ULONG cbOrigEntry, LPENTRYID lpOrigEntry, ULONG *lpcbUnWrappedEntry, LPENTRYID *lppUnWrappedEntry);

HRESULT DoAddress(IAddrBook *lpAdrBook, ULONG* hWnd, LPADRPARM lpAdrParam, LPADRLIST *lpResult);

// Auto-accept settings
HRESULT SetAutoAcceptSettings(IMsgStore *lpMsgStore, bool bAutoAccept, bool bDeclineConflict, bool bDeclineRecurring);
HRESULT GetAutoAcceptSettings(IMsgStore *lpMsgStore, bool *lpbAutoAccept, bool *lpbDeclineConflict, bool *lpbDeclineRecurring);

HRESULT HrGetRemoteAdminStore(IMAPISession *lpMAPISession, IMsgStore *lpMsgStore, LPCTSTR lpszServerName, ULONG ulFlags, IMsgStore **lppMsgStore);

HRESULT HrGetGAB(LPMAPISESSION lpSession, LPABCONT *lppGAB);
HRESULT HrGetGAB(LPADRBOOK lpAddrBook, LPABCONT *lppGAB);


/**
 * NAMED PROPERTY utilities
 *
 * HOW TO USE
 * Make sure you have an IMAPIProp interface to pass to PROPMAP_INIT
 * All properties are allocated an ULONG with name PROP_XXXXX (XXXXX passed in first param of PROPMAP_NAMED_ID
 *
 * EXAMPLE
 *
 * PROPMAP_START
 *  PROPMAP_NAMED_ID(RECURRING, PT_BOOLEAN, PSETID_Appointment, dispidRecurring)
 *  PROPMAP_NAMED_ID(START, 	PT_SYSTIME, PSETID_Appointment, dispidStart)
 * PROPMAP_INIT(lpMessage)
 *
 * printf("%X %X\n", PROP_RECURRING, PROP_START);
 *
 */
class ECPropMapEntry  {
public:
    ECPropMapEntry(GUID guid, ULONG ulId);
    ECPropMapEntry(GUID guid, char *strName);
    ECPropMapEntry(const ECPropMapEntry &other);
    ~ECPropMapEntry();
    
    MAPINAMEID* GetMAPINameId();
private:
    MAPINAMEID m_sMAPINameId;
    GUID m_sGuid;
};

class ECPropMap {
public:
    ECPropMap();
    void AddProp(ULONG *lpId, ULONG ulType, ECPropMapEntry entry);
    HRESULT Resolve(IMAPIProp *lpMAPIProp);
private:
    std::list<ECPropMapEntry> lstNames;
    std::list<ULONG *> lstVars;
    std::list<ULONG> lstTypes;
};

#define PROPMAP_START ECPropMap __propmap;
#define PROPMAP_NAMED_ID(name,type,guid,id) ULONG PROP_##name; __propmap.AddProp(&PROP_##name, type, ECPropMapEntry(guid, id));
#define PROPMAP_INIT(lpObject) hr = __propmap.Resolve(lpObject); if(hr != hrSuccess) goto exit;

#define PROPMAP_DEF_NAMED_ID(name) ULONG PROP_##name;
#define PROPMAP_INIT_NAMED_ID(name,type,guid,id) __propmap.AddProp(&PROP_##name, type, ECPropMapEntry(guid, id));

// Determine the size of an array
template <typename T, unsigned N>
inline unsigned  arraySize(T (&)[N])   { return N; }

// Get the one-past-end item of an array
template <typename T, unsigned N>
inline T* arrayEnd(T (&array)[N])	{ return array + N; }

#endif // COMMONUTIL_H
