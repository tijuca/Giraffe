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

#include "platform.h"
#include "ProtocolBase.h"
#include "stringutil.h"
#include "CommonUtil.h"
#include "CalDavUtil.h"

ProtocolBase::ProtocolBase(Http *lpRequest, IMAPISession *lpSession, ECLogger *lpLogger, std::string strSrvTz, std::string strCharset) {
	m_lpRequest = lpRequest;
	m_lpSession = lpSession;
	m_lpLogger  = lpLogger;

	m_lpUsrFld = NULL;
	m_lpRootFld = NULL;
	m_lpDefStore = NULL;
	m_lpAddrBook = NULL;
	m_lpSharedStore = NULL;
	m_lpPubStore = NULL;
	m_lpActiveStore = NULL;
	m_lpImailUser = NULL;
	m_lpNamedProps = NULL;
	m_blFolderAccess = true;
	m_ulFolderFlag = 0;
	m_strSrvTz = strSrvTz;
	m_strCharset = strCharset;
	m_blIsCLMAC = false;
	m_blIsCLEVO = false;
}

ProtocolBase::~ProtocolBase()
{
	if (m_lpNamedProps)
		MAPIFreeBuffer(m_lpNamedProps);

	if (m_lpImailUser)
		m_lpImailUser->Release();

	if (m_lpRootFld)
		m_lpRootFld->Release();

	if (m_lpUsrFld)
		m_lpUsrFld->Release();

	if (m_lpAddrBook)
		m_lpAddrBook->Release();

	if (m_lpDefStore)
		m_lpDefStore->Release();

	if (m_lpSharedStore)
		m_lpSharedStore->Release();

	if(m_lpPubStore)
		m_lpPubStore->Release();
}

HRESULT ProtocolBase::HrHandleCommand(const std::string &strMethod)
{
	HRESULT hr = hrSuccess;
	std::string strAgent;

	m_lpRequest->HrGetUserAgent(&strAgent);

	// @todo we really need to get rid of this
	if (strAgent.find("CalendarStore") != string::npos)	// ical4 and 5 client
		m_blIsCLMAC = true;
	else if (strAgent.find("iOS") != string::npos)		// iphone
		m_blIsCLMAC = true;
	else if (strAgent.find("CoreDAV") != string::npos)	// found when listing calendars
		m_blIsCLMAC = true;

	if(strAgent.find("Evolution") != string::npos)
		m_blIsCLEVO = true;

	hr = HrGetFolder();

	return hr;
}

/**
 * Open Store and folders required for the Request
 *
 * @return	MAPI error code
 * @retval	MAPI_E_NO_ACCESS	Not enough permissions to open the folder or store
 * @retval	MAPI_E_NOT_FOUND	Folder requested by client not found
 */
HRESULT ProtocolBase::HrGetFolder()
{
	HRESULT hr = hrSuccess;
	std::wstring wstrUrl;
	std::string strMethod;
	LPSPropValue lpDefaultProp = NULL;
	LPSPropValue lpFldProp = NULL;
	IMAPIFolder *lpRootCont = NULL;
	bool blCreateIFNf = false;
	bool bIsPublic = false;

	m_lpRequest->HrGetUrl(&wstrUrl);
	m_lpRequest->HrGetUser(&m_wstrUser);

	HrParseURL(wstrUrl, &m_ulUrlFlag, &m_wstrFldOwner, &m_wstrFldName);
	bIsPublic = m_ulUrlFlag & REQ_PUBLIC;
	if (m_wstrFldOwner.empty())
		m_wstrFldOwner = m_wstrUser;

	m_lpRequest->HrGetMethod(&strMethod);	
	
	hr = m_lpSession->OpenAddressBook(0, NULL, 0, &m_lpAddrBook);
	if(hr != hrSuccess)
	{
		m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Error opening addressbook, error code : 0x%08X", hr);
		goto exit;
	}

	// default store required for various actions (delete, freebusy, ...)
	hr = HrOpenDefaultStore(m_lpSession, &m_lpDefStore);
	if(hr != hrSuccess)
	{
		m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Error opening default store of user %ls, error code : 0x%08X", m_wstrUser.c_str(), hr);
		goto exit;
	}
	
	m_lpActiveStore = m_lpDefStore;
	
	if(bIsPublic)
	{
		hr = HrOpenECPublicStore(m_lpSession, &m_lpPubStore);
		if (hr != hrSuccess)
			goto exit;
				
		m_lpActiveStore = m_lpPubStore;
		hr = OpenSubFolder(m_lpActiveStore, NULL, '/', m_lpLogger, true, false, &m_lpRootFld);
		if(hr != hrSuccess)
		{
			m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Error opening IPM_SUBTREE Folder of user %ls, error code : 0x%08X", m_wstrUser.c_str(), hr);
			goto exit;
		}
	}

	// Retreive named properties
	hr = HrLookupNames(m_lpActiveStore, &m_lpNamedProps);
	if (hr != hrSuccess)
		goto exit;

	if ( (!strMethod.compare("PUT")) && (m_ulFolderFlag & SERVICE_ICAL) )
		blCreateIFNf = true;


	// @todo this block of if/else needs to be rewritten .. it should be much easier than it is now.
	if(m_wstrFldOwner != m_wstrUser && !bIsPublic)
	{
		hr = HrGetSharedFolder(m_lpSession, m_lpDefStore, m_wstrFldOwner, m_wstrFldName, m_blIsCLMAC, false, m_lpLogger, &m_ulFolderFlag, &m_lpSharedStore, &m_lpUsrFld);
		if(hr == hrSuccess && m_lpSharedStore)
		{
			m_lpActiveStore = m_lpSharedStore;
			hr = OpenSubFolder(m_lpActiveStore, NULL, '/', m_lpLogger, false, false, &m_lpRootFld);
			if(hr != hrSuccess)
			{
				hr = hrSuccess;
				m_lpRootFld = NULL;
			}
		}

		if(hr == MAPI_E_NO_ACCESS)
		{
			m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Not enough permissions to user %ls to access Folder for user %ls", m_wstrUser.c_str(), m_wstrFldOwner.c_str());
			goto exit;
		}
		else if(hr != hrSuccess)
		{
			m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Error while accessing shared folder by user %ls, error code : 0x%08X", m_wstrUser.c_str(), hr);
			goto exit;
		}
		
	}	
	else if(m_blIsCLMAC ) //For Ical Client The folders are opened according to Folder_ID.
	{
		if(!m_wstrFldName.empty())
		{
			hr = HrFindFolder(m_lpActiveStore, m_lpNamedProps, m_lpLogger, bIsPublic, m_wstrFldName, &m_lpUsrFld);
			if(hr != hrSuccess)
			{
				hr = HrOpenUserFld(m_lpActiveStore, m_wstrFldName, blCreateIFNf, bIsPublic, m_lpLogger, &m_lpUsrFld);
				if(hr == hrSuccess) // do not exit , need to set the m_lpRootfolder if not set.
					m_ulFolderFlag |= SINGLE_FOLDER;
			}
		}
	}
	//For other Clients the Folder name is used to refer to folders.
	else
	{
		if (strMethod.compare("MKCALENDAR"))
		{
			hr = HrOpenUserFld(m_lpActiveStore, m_wstrFldName, blCreateIFNf, bIsPublic, m_lpLogger, &m_lpUsrFld);
			if(hr != hrSuccess)
			{
				m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Error opening folder of user %ls, error code : 0x%08X", m_wstrUser.c_str(), hr);
				goto exit;
			}
		}
	}
	
	hr = HrGetUserInfo(m_lpSession, m_lpDefStore, &m_strUserEmail, &m_wstrUserFullName, &m_lpImailUser);
	if(hr != hrSuccess)
		goto exit;

	if(!m_lpRootFld)
	{
		//Open IPM_SUBTREE
		hr = OpenSubFolder(m_lpDefStore, NULL, '/', m_lpLogger, false, false, &m_lpRootFld);
		if(hr != hrSuccess)
		{
			m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Error opening IPM_SUBTREE Folder of user %ls, error code : 0x%08X", m_wstrUser.c_str(), hr);
			goto exit;
		}
	}

	//Set Limited Access to Default Calendar Folder
	//Block Rename & Deletion of this folder.
	if( (!strMethod.compare("PROPPATCH") || !strMethod.compare("DELETE") || !strMethod.compare("PUT")) && !bIsPublic)
	{
		int ulCmp = 0;
		ULONG ulType = 0;

		hr = m_lpActiveStore->OpenEntry(0, NULL, NULL, 0, &ulType, (LPUNKNOWN*)&lpRootCont);
		if (hr != hrSuccess || ulType != MAPI_FOLDER)
		{
			m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Error opening root Container of user %ls, error code: (0x%08X)", m_wstrUser.c_str(), hr);
			goto exit;
		}

		m_blFolderAccess = CheckFolderAccess(m_lpActiveStore, lpRootCont, m_lpUsrFld);

		hr = HrGetOneProp(lpRootCont, PR_IPM_APPOINTMENT_ENTRYID, &lpDefaultProp);
		if(hr != hrSuccess)
		{
			m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Error retrieving Entry id of Default calendar for user %ls, error code: 0x%08X", m_wstrUser.c_str(), hr);
			goto exit;
		}

		if(m_lpUsrFld)
			hr = HrGetOneProp(m_lpUsrFld, PR_ENTRYID, &lpFldProp);
		else
		{
			m_blFolderAccess = false;
			goto exit;
		}

		if(hr != hrSuccess)
			goto exit;
		
		
		ulCmp = memcmp(lpDefaultProp->Value.bin.lpb, lpFldProp->Value.bin.lpb, lpFldProp->Value.bin.cb);
		if(!ulCmp) {
			m_blFolderAccess = false;
			m_ulFolderFlag |= DEFAULT_FOLDER;
		}
		
		MAPIFreeBuffer(lpFldProp);
		lpFldProp = NULL;

		hr = HrGetOneProp(m_lpUsrFld, PR_SUBFOLDERS, &lpFldProp);
		if(hr != hrSuccess)
			goto exit;

		if(lpFldProp->Value.b == (unsigned short)true && !strMethod.compare("DELETE"))
			m_blFolderAccess = false;
	}

exit:
	if(lpFldProp)
		MAPIFreeBuffer(lpFldProp);

	if(lpDefaultProp)
		MAPIFreeBuffer(lpDefaultProp);

	if(lpRootCont)
		lpRootCont->Release();

	return hr;
}

/**
 * converts widechar to utf-8 string
 * @param[in]	strWinChar	source string(windows-1252) to be converted
 * @return		string		converted string (utf-8)
 */
std::string ProtocolBase::W2U(std::wstring strWideChar)
{
	return m_converter.convert_to<string>(m_strCharset.c_str(), strWideChar, rawsize(strWideChar), CHARSET_WCHAR);
}

/**
 * converts utf-8 string to widechar
 * @param[in]	strUtfChar	source string(utf-8) to be converted
 * @return		string		converted string (windows-1252)
 */
std::wstring ProtocolBase::U2W(std::string strUtfChar)
{
	return m_converter.convert_to<wstring>(strUtfChar, rawsize(strUtfChar), m_strCharset.c_str());	
}

/**
 * Convert SPropValue to string
 * 
 * @param[in]	lpSprop		SPropValue to be converted
 * @return		string
 */
std::string ProtocolBase::SPropValToString(SPropValue * lpSprop)
{
	FILETIME ft;
	time_t tmUnixTime;
	std::string strRetVal;
	
	if (lpSprop == NULL) {
		return std::string();
	}

	if (PROP_TYPE(lpSprop->ulPropTag) == PT_SYSTIME)
	{
		ft = lpSprop->Value.ft;
		FileTimeToUnixTime(ft, &tmUnixTime);
		strRetVal = stringify_int64(tmUnixTime, false);

	}
	else if (PROP_TYPE(lpSprop->ulPropTag) == PT_STRING8)
	{
		strRetVal = lpSprop->Value.lpszA;
	}
	else if (PROP_TYPE(lpSprop->ulPropTag) == PT_UNICODE)
	{
		strRetVal = W2U(lpSprop->Value.lpszW);
	}
	//Global UID
	else if (PROP_TYPE(lpSprop->ulPropTag) == PT_BINARY)
	{
		HrGetICalUidFromBinUid(lpSprop->Value.bin, &strRetVal);	
	}
	else if (PROP_TYPE(lpSprop->ulPropTag) == PT_LONG)
	{
		strRetVal = stringify(lpSprop->Value.ul, false);
	}

	return strRetVal;
}

/**
 * Convert SPropValue to string
 * 
 * @param[in]	lpSprop		SPropValue to be converted
 * @return		wstring
 */
std::wstring ProtocolBase::SPropValToWString(SPropValue * lpSprop)
{
	FILETIME ft;
	time_t tmUnixTime;
	std::wstring strRetVal;	

	if (lpSprop == NULL) {
		return std::wstring();
	}

	if (PROP_TYPE(lpSprop->ulPropTag) == PT_SYSTIME)
	{
		ft = lpSprop->Value.ft;
		FileTimeToUnixTime(ft, &tmUnixTime);
		strRetVal = wstringify_int64(tmUnixTime, false);

	}
	else if (PROP_TYPE(lpSprop->ulPropTag) == PT_STRING8)
	{
		strRetVal = U2W(lpSprop->Value.lpszA);
	}
	else if (PROP_TYPE(lpSprop->ulPropTag) == PT_UNICODE)
	{
		strRetVal = lpSprop->Value.lpszW;
	}
	//Global UID
	else if (PROP_TYPE(lpSprop->ulPropTag) == PT_BINARY)
	{
		string strTmp;
		HrGetICalUidFromBinUid(lpSprop->Value.bin, &strTmp);	
		strRetVal = U2W(strTmp);
	}
	else if (PROP_TYPE(lpSprop->ulPropTag) == PT_LONG)
	{
		strRetVal = wstringify(lpSprop->Value.ul, false);
	}

	return strRetVal;
}

/** 
 * Checks in the given store if the given folder may be removed, or
 * should be treated as a special folder.
 * 
 * @param[in] lpStore Store of a user or the public
 * @param[in] lpRootFolder Root Folder in lpStore, can be NULL
 * @param[in] lpCurrentFolder Folder in lpStore to check for protection
 * 
 * @todo should we ignore some protected folders because it can be the public store?
 *
 * @return true if the lpCurrentFolder may be deleted, false if not
 */
bool ProtocolBase::CheckFolderAccess(IMsgStore *lpStore, IMAPIFolder *lpRootFolder, IMAPIFolder *lpCurrentFolder) const
{
	HRESULT hr = hrSuccess;
	LPSPropValue lpFolderEntryID = NULL;
	ULONG ulObjType;
	IMAPIFolder *lpRoot = NULL;

	// folders which cannot be deleted
	SizedSPropTagArray(3, sPropsStore) = { 3,
		{ PR_IPM_OUTBOX_ENTRYID, PR_IPM_SENTMAIL_ENTRYID, PR_IPM_WASTEBASKET_ENTRYID } };
	const char* sNamesStore[] = {"Outbox", "Sentmail", "Wastebasket"};
	SizedSPropTagArray(6, sPropsRoot) = { 6,
		{ PR_IPM_APPOINTMENT_ENTRYID, PR_IPM_CONTACT_ENTRYID, PR_IPM_DRAFTS_ENTRYID,
		  PR_IPM_JOURNAL_ENTRYID, PR_IPM_NOTE_ENTRYID, PR_IPM_TASK_ENTRYID } };
	const char* sNamesRoot[] = {"Calendar", "Contacts", "Drafts", "Journal", "Notes", "Tasks"};

	ULONG cbFolders;
	LPSPropValue lpProtectedFolders1 = NULL;
	LPSPropValue lpProtectedFolders2 = NULL;
	BOOL bFound = FALSE;
	ULONG ulPos = 0;

	if (!lpStore || !lpCurrentFolder) {
		hr = MAPI_E_NO_ACCESS;
		goto exit;
	}

	hr = HrGetOneProp(lpCurrentFolder, PR_ENTRYID, &lpFolderEntryID);
	if (hr != hrSuccess)
		goto exit;

	// Find protected folder from store level
	hr = lpStore->GetProps((LPSPropTagArray)&sPropsStore, 0, &cbFolders, &lpProtectedFolders1);
	if (FAILED(hr) || cbFolders < sPropsStore.cValues) {
		m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Error while retrieving protected folder list");
		goto exit;
	}

	hr = Util::HrFindEntryIDs(lpFolderEntryID->Value.bin.cb, (LPENTRYID)lpFolderEntryID->Value.bin.lpb, cbFolders, lpProtectedFolders1, &bFound, &ulPos);
	if (hr != hrSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Error while testing for protected folders");
		goto exit;
	}
	if (bFound == TRUE) {
		hr = MAPI_E_NO_ACCESS;
		m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Accessing Default folder named %s", sNamesStore[ulPos]);
		goto exit;
	}

	// Find protected folder from inbox level
	if (lpRootFolder) {
		hr = lpRootFolder->QueryInterface(IID_IMAPIFolder, (void**)&lpRoot);
		ulObjType = MAPI_FOLDER;
	} else {
		hr = lpStore->OpenEntry(0, NULL, NULL, 0, &ulObjType, (LPUNKNOWN*)&lpRoot);
	}
	if (hr != hrSuccess || ulObjType != MAPI_FOLDER) {
		m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Error while retrieving root container");
		goto exit;
	}

	hr = lpRoot->GetProps((LPSPropTagArray)&sPropsRoot, 0, &cbFolders, &lpProtectedFolders2);
	if (FAILED(hr) || cbFolders < sPropsRoot.cValues) {
		m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Error while retrieving protected folder list");
		goto exit;
	}

	hr = Util::HrFindEntryIDs(lpFolderEntryID->Value.bin.cb, (LPENTRYID)lpFolderEntryID->Value.bin.lpb, cbFolders, lpProtectedFolders2, &bFound, &ulPos);
	if (hr != hrSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Error while testing for protected folders");
		goto exit;
	}
	if (bFound == TRUE) {
		hr = MAPI_E_NO_ACCESS;
		m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Accessing Default folder named %s folder", sNamesRoot[ulPos]);
		goto exit;
	}

exit:
	if (lpFolderEntryID)
		MAPIFreeBuffer(lpFolderEntryID);

	if (lpRoot)
		lpRoot->Release();

	if (lpProtectedFolders1)
		MAPIFreeBuffer(lpProtectedFolders1);

	if (lpProtectedFolders2)
		MAPIFreeBuffer(lpProtectedFolders2);

	return hr == hrSuccess;
}
