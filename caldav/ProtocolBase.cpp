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
#include "mapi_ptr.h"

using namespace std;

ProtocolBase::ProtocolBase(Http *lpRequest, IMAPISession *lpSession, ECLogger *lpLogger, std::string strSrvTz, std::string strCharset) {
	m_lpRequest = lpRequest;
	m_lpSession = lpSession;
	m_lpLogger  = lpLogger;

	m_lpUsrFld = NULL;
	m_lpIPMSubtree = NULL;
	m_lpDefStore = NULL;
	m_lpAddrBook = NULL;
	m_lpActiveStore = NULL;
	m_lpLoginUser = NULL;
	m_lpActiveUser = NULL;
	m_lpNamedProps = NULL;
	m_blFolderAccess = true;
	m_ulFolderFlag = 0;
	m_strSrvTz = strSrvTz;
	m_strCharset = strCharset;
}

ProtocolBase::~ProtocolBase()
{
	if (m_lpNamedProps)
		MAPIFreeBuffer(m_lpNamedProps);

	if (m_lpLoginUser)
		m_lpLoginUser->Release();

	if (m_lpActiveUser)
		m_lpActiveUser->Release();

	if (m_lpIPMSubtree)
		m_lpIPMSubtree->Release();

	if (m_lpUsrFld)
		m_lpUsrFld->Release();

	if (m_lpAddrBook)
		m_lpAddrBook->Release();

	if (m_lpDefStore)
		m_lpDefStore->Release();

	if (m_lpActiveStore)
		m_lpActiveStore->Release();
}

/**
 * Opens the store and folders required for the Request. Also checks
 * if DELETE or RENAME actions are allowed on the folder.
 *
 * @return	MAPI error code
 * @retval	MAPI_E_NO_ACCESS	Not enough permissions to open the folder or store
 * @retval	MAPI_E_NOT_FOUND	Folder requested by client not found
 */
HRESULT ProtocolBase::HrInitializeClass()
{
	HRESULT hr = hrSuccess;
	std::string strUrl;
	std::string strMethod;
	std::string strAgent;
	string strFldOwner;
	string strFldName;
	LPSPropValue lpDefaultProp = NULL;
	LPSPropValue lpFldProp = NULL;
	SPropValuePtr lpEntryID;
	ULONG ulRes = 0;
	bool blCreateIFNf = false;
	bool bIsPublic = false;
	ULONG ulType = 0;
	MAPIFolderPtr lpRoot;

	/* URLs
	 * 
	 * /caldav/							| depth: 0 results in root props, depth: 1 results in 0 + calendar FOLDER list current user. eg: the same happens /caldav/self/
	 * /caldav/self/					| depth: 0 results in root props, depth: 1 results in 0 + calendar FOLDER list current user. see ^^
	 * /caldav/self/Calendar/			| depth: 0 results in root props, depth: 1 results in 0 + _GIVEN_ calendar CONTENTS list current user.
	 * /caldav/self/_NAMED_FOLDER_ID_/ (evo has date? still??) (we also do hexed entryids?)
	 * /caldav/other/[...]
	 * /caldav/public/[...]
	 *
	 * /caldav/user/folder/folder/		| subfolders are not allowed!
	 * /caldav/user/folder/id.ics		| a message (note: not ending in /)
	 */

	m_lpRequest->HrGetUrl(&strUrl);
	m_lpRequest->HrGetUser(&m_wstrUser);
	m_lpRequest->HrGetMethod(&strMethod);	

	HrParseURL(strUrl, &m_ulUrlFlag, &strFldOwner, &strFldName);
	m_wstrFldOwner = U2W(strFldOwner);
	m_wstrFldName = U2W(strFldName);
	bIsPublic = m_ulUrlFlag & REQ_PUBLIC;
	if (m_wstrFldOwner.empty())
		m_wstrFldOwner = m_wstrUser;

	// we may (try to) create the folder on new ical data writes
	if ( (!strMethod.compare("PUT")) && (m_ulUrlFlag & SERVICE_ICAL))
		blCreateIFNf = true;

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

	hr = HrGetOwner(m_lpSession, m_lpDefStore, &m_lpLoginUser);
	if(hr != hrSuccess)
		goto exit;

	/*
	 * Set m_lpActiveStore
	 */
	if (bIsPublic)
	{
		// open public
		hr = HrOpenECPublicStore(m_lpSession, &m_lpActiveStore);
		if (hr != hrSuccess) {
			m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Unable to open public store with user %ls, error code : 0x%08X", m_wstrUser.c_str(), hr);
			goto exit;
		}
	} else if (wcscasecmp(m_wstrUser.c_str(), m_wstrFldOwner.c_str())) {
		// open shared store
		hr = HrOpenUserMsgStore(m_lpSession, (WCHAR*)m_wstrFldOwner.c_str(), &m_lpActiveStore);
		if (hr != hrSuccess) {
			m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Unable to open store of user %ls with user %ls, error code : 0x%08X", m_wstrFldOwner.c_str(), m_wstrUser.c_str(), hr);
			goto exit;
		}
		m_ulFolderFlag |= SHARED_FOLDER;
	} else {
		// @todo, make auto pointers
		hr = m_lpDefStore->QueryInterface(IID_IMsgStore, (void**)&m_lpActiveStore);
	}

	// Retrieve named properties
	hr = HrLookupNames(m_lpActiveStore, &m_lpNamedProps);
	if (hr != hrSuccess)
		goto exit;

	// get active user info
	if (bIsPublic) {
		hr = m_lpLoginUser->QueryInterface(IID_IMailUser, (void**)&m_lpActiveUser);
	} else {
		hr = HrGetOwner(m_lpSession, m_lpActiveStore, &m_lpActiveUser);
	}
	if(hr != hrSuccess)
		goto exit;


	/*
	 * Set m_lpIPMSubtree, used for CopyFolder, CreateFolder, DeleteFolder
	 */
	hr = OpenSubFolder(m_lpActiveStore, NULL, '/', m_lpLogger, bIsPublic, false, &m_lpIPMSubtree);
	if(hr != hrSuccess)
	{
		m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Error opening IPM SUBTREE, using user %ls, error code : 0x%08X", m_wstrUser.c_str(), hr);
		goto exit;
	}


	// Get active store default calendar to prevent delete action on this folder
	hr = m_lpActiveStore->OpenEntry(0, NULL, NULL, 0, &ulType, &lpRoot);
	if(hr != hrSuccess)
	{
		m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Error opening root container, using user %ls, error code : 0x%08X", m_wstrUser.c_str(), hr);
		goto exit;
	}

	if (!bIsPublic) {
		// get default calendar entryid for non-public stores
		hr = HrGetOneProp(lpRoot, PR_IPM_APPOINTMENT_ENTRYID, &lpDefaultProp);
		if(hr != hrSuccess)
		{
			m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Error retrieving Entry id of Default calendar for user %ls, error code: 0x%08X", m_wstrUser.c_str(), hr);
			goto exit;
		}
	}

	/*
	 * Set m_lpUsrFld
	 */
	if (strMethod.compare("MKCALENDAR") == 0 && (m_ulUrlFlag & SERVICE_CALDAV))
	{
		// created in the IPM_SUBTREE
		hr = OpenSubFolder(m_lpActiveStore, NULL, '/', m_lpLogger, bIsPublic, false, &m_lpUsrFld);
		if(hr != hrSuccess)
		{
			m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Error opening IPM_SUBTREE folder of user %ls, error code: 0x%08X", m_wstrUser.c_str(), hr);
			goto exit;
		}
	}
	else if(!m_wstrFldName.empty())
	{
		// @note, caldav allows creation of calendars for non-existing urls, but since this can also use id's, I'm not sure we want to.
		hr = HrFindFolder(m_lpActiveStore, m_lpIPMSubtree, m_lpNamedProps, m_lpLogger, m_wstrFldName, &m_lpUsrFld);
		if(hr != hrSuccess)
		{
			m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Error opening named folder of user %ls, folder %ls, error code: 0x%08X", m_wstrUser.c_str(), m_wstrFldName.c_str(), hr);
			goto exit;
		}
		m_ulFolderFlag |= SINGLE_FOLDER;

		// check if this is the default calendar folder to enable freebusy publishing
		if (HrGetOneProp(m_lpUsrFld, PR_ENTRYID, &lpEntryID) == hrSuccess &&
			m_lpActiveStore->CompareEntryIDs(lpEntryID->Value.bin.cb, (LPENTRYID)lpEntryID->Value.bin.lpb,
											 lpDefaultProp->Value.bin.cb, (LPENTRYID)lpDefaultProp->Value.bin.lpb, 0, &ulRes) == hrSuccess &&
			ulRes == TRUE)
		{
			// disable delete default folder, and enable fb publish
			m_blFolderAccess = false;
			m_ulFolderFlag |= DEFAULT_FOLDER;
		}
	} else {
		// default calendar
		if (bIsPublic) {
			hr = m_lpIPMSubtree->QueryInterface(IID_IMAPIFolder, (void**)&m_lpUsrFld);
			if (hr != hrSuccess)
				goto exit;
		} else {
			// open default calendar
			hr = m_lpActiveStore->OpenEntry(lpDefaultProp->Value.bin.cb, (LPENTRYID)lpDefaultProp->Value.bin.lpb, NULL, MAPI_BEST_ACCESS, &ulType, (LPUNKNOWN*)&m_lpUsrFld);
			if (hr != hrSuccess)
			{
				m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Unable to open default calender for user %ls, error code: 0x%08X", m_wstrUser.c_str(), hr);
				goto exit;
			}

			// we already know we don't want to delete this folder
			m_blFolderAccess = false;
			m_ulFolderFlag |= DEFAULT_FOLDER;
		}
	}


	/*
	 * Workaround for old users with sunbird / lightning on old url base.
	 */
	{
		vector<string> parts;
		parts = tokenize(strUrl, '/', true);

		m_lpRequest->HrGetHeaderValue("User-Agent", &strAgent);

		// /caldav/
		// /caldav/username/ (which we return in XML data! (and shouldn't)), since this isn't a calender, but /caldav/username/Calendar/ is.
		if ((strAgent.find("Sunbird/1") != string::npos || strAgent.find("Lightning/1") != string::npos) && parts.size() <= 2) {
			// Mozilla Sunbird / Lightning doesn't handle listing of calendars, only contents.
			// We therefore redirect them to the default calendar url.
			SPropValuePtr ptrDisplayName;
			string strLocation = "/caldav/" + urlEncode(m_wstrFldOwner, "utf-8");

			if (HrGetOneProp(m_lpUsrFld, PR_DISPLAY_NAME_W, &ptrDisplayName) == hrSuccess) {
				std::string part = urlEncode(ptrDisplayName->Value.lpszW, "UTF-8"); 
				strLocation += "/" + part + "/";
			} else {
				// return 404 ?
				strLocation += "/Calendar/";
			}

			m_lpRequest->HrResponseHeader(301, "Moved Permanently");
			m_lpRequest->HrResponseHeader("Location", m_converter.convert_to<string>(strLocation));
			hr = MAPI_E_NOT_ME;
			goto exit;
		}
	}

	/*
	 * Check delete / rename access on folder, if not already blocked.
	 */
	if (m_blFolderAccess) {
		// lpDefaultProp already should contain PR_IPM_APPOINTMENT_ENTRYID
		if (lpDefaultProp) {
			ULONG ulCmp;

			hr = HrGetOneProp(m_lpUsrFld, PR_ENTRYID, &lpFldProp);
			if (hr != hrSuccess)
				goto exit;

			hr = m_lpSession->CompareEntryIDs(lpDefaultProp->Value.bin.cb, (LPENTRYID)lpDefaultProp->Value.bin.lpb,
											  lpFldProp->Value.bin.cb, (LPENTRYID)lpFldProp->Value.bin.lpb, 0, &ulCmp);
			if (hr != hrSuccess || ulCmp == TRUE)
				m_blFolderAccess = false;

			MAPIFreeBuffer(lpFldProp);
			lpFldProp = NULL;
		}
	}
	if (m_blFolderAccess) {
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

	return hr;
}

/**
 * converts widechar to utf-8 string
 * @param[in]	strWinChar	source string(windows-1252) to be converted
 * @return		string		converted string (utf-8)
 */
std::string ProtocolBase::W2U(const std::wstring &strWideChar)
{
	return m_converter.convert_to<string>(m_strCharset.c_str(), strWideChar, rawsize(strWideChar), CHARSET_WCHAR);
}

/**
 * converts widechar to utf-8 string
 * @param[in]	strWinChar	source string(windows-1252) to be converted
 * @return		string		converted string (utf-8)
 */
std::string ProtocolBase::W2U(const WCHAR* lpwWideChar)
{
	return m_converter.convert_to<string>(m_strCharset.c_str(), lpwWideChar, rawsize(lpwWideChar), CHARSET_WCHAR);
}

/**
 * converts utf-8 string to widechar
 * @param[in]	strUtfChar	source string(utf-8) to be converted
 * @return		wstring		converted wide string
 */
std::wstring ProtocolBase::U2W(const std::string &strUtfChar)
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
	time_t tmUnixTime;
	std::string strRetVal;
	
	if (lpSprop == NULL) {
		return std::string();
	}

	if (PROP_TYPE(lpSprop->ulPropTag) == PT_SYSTIME)
	{
		FileTimeToUnixTime(lpSprop->Value.ft, &tmUnixTime);
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
