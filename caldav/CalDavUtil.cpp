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
#include "CalDavUtil.h"
#include "charset/convert.h"

using namespace std;

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/**
 * Open MAPI session
 *
 * @param[in]	strUser		User's login name
 * @param[in]	strPass		User's password
 * @param[in]	strPath		Zarafa server's path
 * @param[out]	lppSession	IMAPISession object if login is successful
 * @return		HRESULT
 * @retval		MAPI_E_LOGON_FAILED		Unable to login with the specified user-name and password
 */
HRESULT HrAuthenticate(const std::wstring &wstrUser, const std::wstring &wstrPass, std::string strPath, IMAPISession **lppSession)
{
	HRESULT hr = hrSuccess;	

	// @todo: if login with utf8 username is not possible, lookup user from addressbook? but how?
	hr = HrOpenECSession(lppSession, wstrUser.c_str(), wstrPass.c_str(), strPath.c_str(), 0, NULL, NULL, NULL);

	return hr;
}

/**
 * Searches the store for a folder by using its name
 * @param[in]	lpMsgStore		The store in which the folder is be searched
 * @param[in]	strFolderParam	The full path of the folder(eg. 'Calendar/SUB_FOLDER')
 * @param[in]	blCreateIfNF	Boolean that states to create the folder if not found
 * @param[in]	blIsPublic		Boolean that states if the store passed in lpMsgStore is public
 * @param[in]	lpLogger		Pointer to ECLogger object
 * @param[out]	lpUsrFld		IMAPIFolder object of the opened folder
 * 
 * @return		HRESULT
 * @retval		MAPI_E_NOT_FOUND	Folder specified in  strFolderPath not found
 */
HRESULT HrOpenUserFld(IMsgStore *lpMsgStore, std::wstring wstrFolderParam, bool blCreateIfNF, bool blIsPublic, ECLogger *lpLogger, IMAPIFolder **lpUsrFld)
{
	HRESULT hr = hrSuccess;	
	WCHAR lpszpsep = '/';
	LPMAPIFOLDER  lpSubFolder = NULL;
	SPropValue sPropVal;
	bool bCreated = false;

	if (wstrFolderParam.empty() && !blIsPublic)
	{
		hr = HrOpenDefaultCalendar(lpMsgStore, lpLogger, &lpSubFolder);
		if (hr != hrSuccess)
		{
			lpLogger->Log(EC_LOGLEVEL_ERROR, "Cannot open Users Default Folder : 0x%08X", hr);
			goto exit;
		}		
	}
	else
	{
		hr = OpenSubFolder(lpMsgStore, wstrFolderParam.c_str(), lpszpsep, lpLogger, blIsPublic, false, &lpSubFolder);
		if(hr == MAPI_E_NOT_FOUND && blCreateIfNF)
		{
			hr = OpenSubFolder(lpMsgStore, wstrFolderParam.c_str(), lpszpsep, lpLogger, blIsPublic, blCreateIfNF, &lpSubFolder);
			if (hr == hrSuccess)
				bCreated = true;
		}

		if (hr != hrSuccess)
		{
			lpLogger->Log(EC_LOGLEVEL_ERROR, "Cannot open users folder '%ls': 0x%08X", wstrFolderParam.c_str(), hr);
			goto exit;
		}
	}
	
	// set container class only for newly created folders
	if(blCreateIfNF && bCreated)
	{
		sPropVal.ulPropTag = PR_CONTAINER_CLASS_A;
		sPropVal.Value.lpszA = "IPF.Appointment";

		HrSetOneProp(lpSubFolder, &sPropVal); // ignoring if property is not set.		
	}

	*lpUsrFld = lpSubFolder;

exit:
	
	return hr;
}

/**
 * Add Property to the folder or message
 *
 * @param[in]		lpMapiProp		IMAPIProp object pointer to which property is to be added
 * @param[in]		ulPropTag		Property Tag of the property to be added
 * @param[in]		bFldId			Boolean to state if the property to be added is FolderID or not
 * @param[in,out]	wstrProperty		String value of the property, if empty then GUID is created and added
 *
 * @return			HRESULT 
 */
HRESULT HrAddProperty(IMAPIProp *lpMapiProp, ULONG ulPropTag, bool bFldId, std::wstring *wstrProperty)
{
	HRESULT hr = hrSuccess;
	SPropValue sPropVal;
	LPSPropValue lpMsgProp = NULL;
	LPSPropValue lpsPropSrch = NULL;
	LPSPropValue lpsPropValEid = NULL;
	GUID sGuid ;

	if(wstrProperty->empty())
	{
		CoCreateGuid(&sGuid);
		wstrProperty->assign(convert_to<wstring>(bin2hex(sizeof(GUID), (LPBYTE)&sGuid)));
	}

	ASSERT(PROP_TYPE(ulPropTag) != PT_STRING8);
	sPropVal.ulPropTag = ulPropTag;
	sPropVal.Value.lpszW = (LPWSTR)wstrProperty->c_str();

	hr = HrGetOneProp(lpMapiProp, sPropVal.ulPropTag, &lpMsgProp);
	if (hr == MAPI_E_NOT_FOUND) {
		hr = HrSetOneProp(lpMapiProp,&sPropVal);
		if (hr == E_ACCESSDENIED && bFldId)
		{
			hr = HrGetOneProp(lpMapiProp,PR_ENTRYID,&lpsPropValEid);
			if(hr != hrSuccess)
				goto exit;

			wstrProperty->assign(convert_to<wstring>(bin2hex(lpsPropValEid[0].Value.bin.cb,lpsPropValEid[0].Value.bin.lpb)));
		}
	} else if (hr != hrSuccess)
		goto exit;
	else
		wstrProperty->assign(lpsPropSrch->Value.lpszW);
	
	hr = lpMapiProp->SaveChanges(KEEP_OPEN_READWRITE);

exit:
	if(lpsPropValEid)
		MAPIFreeBuffer(lpsPropValEid);

	if(lpMsgProp)
		MAPIFreeBuffer(lpMsgProp);

	if(lpsPropSrch)
		MAPIFreeBuffer(lpsPropSrch);

	return hr;
}

/**
 * Add Property to the folder or message
 *
 * @param[in]		lpMsgStore		Pointer to store of the user
 * @param[in]		sbEid			EntryID of the folder to which property has to be added
 * @param[in]		ulPropertyId	Property Tag of the property to be added
 * @param[in]		bIsFldID		Boolean to state if the property to be added is FolderID or not
 * @param[in,out]	lpwstrProperty	String value of the property, if empty then GUID is created and added
 *
 * @return			HRESULT 
 */
HRESULT HrAddProperty(IMsgStore *lpMsgStore, SBinary sbEid, ULONG ulPropertyId, bool bIsFldID, std::wstring *lpwstrProperty )
{
	HRESULT hr = hrSuccess;
	IMAPIFolder *lpUsrFld = NULL;
	ULONG ulObjType = 0;
	
	hr = lpMsgStore->OpenEntry(sbEid.cb, (LPENTRYID)sbEid.lpb,NULL, MAPI_BEST_ACCESS, &ulObjType, (LPUNKNOWN *) &lpUsrFld);
	if(hr != hrSuccess)
		goto exit;

	hr = HrAddProperty(lpUsrFld, ulPropertyId, bIsFldID, lpwstrProperty);

exit:
	if(lpUsrFld)
		lpUsrFld->Release();

	return hr;
}
/**
 * Finds the folder in the store by using the its Folder ID
 *
 * Folder-id is could be either named property (dispidFldID) or PR_ENTRYID
 *
 * @param[in]	lpMsgStore		Pointer to users store
 * @param[in]	lpNamedProps	Named property tag array
 * @param[in]	lpLogger		Pointer to ECLogger	object 
 * @param[in]	blIsPublic		boolean value that states if store is public
 * @param[in]	wstrFldId		Folder-id of the folder to be searched
 * @param[out]	lppUsrFld		Return pointer for the folder found
 *
 * @return		mapi error codes
 * @retval		MAPI_E_NOT_FOUND	Folder refrenced by folder-id not found
 *
 * @todo	add some check to remove the dirty >50 length check
 * @note this function is only called from mac clients, so some more hacks are introduced here.
 */
HRESULT HrFindFolder(IMsgStore *lpMsgStore, LPSPropTagArray lpNamedProps, ECLogger *lpLogger, bool blIsPublic, std::wstring wstrFldId, IMAPIFolder **lppUsrFld)
{
	HRESULT hr = hrSuccess;
	std::string strBinEid;
	SRestriction *lpRestrict = NULL;
	IMAPITable *lpHichyTable = NULL;
	SPropValue sSpropVal ;
	ULONG ulPropTagFldId = 0;
	SPropTagArray *lpPropTagArr = NULL;
	SRowSet *lpRows = NULL;
	SBinary sbEid = {0,0};
	IMAPIFolder *lpUsrFld = NULL;
	IMAPIFolder *lpRootFld = NULL;
	ULONG cbsize = 0;
	ULONG ulObjType = 0;
	size_t ulFound = 0;
	convert_context converter;
	ULONG cbEntryID = 0;
	LPENTRYID lpEntryID = NULL;
	LPSPropValue lpOutbox = NULL;

	ulFound = wstrFldId.find(FOLDER_PREFIX);
	if(ulFound != wstring::npos)
		wstrFldId = wstrFldId.substr(ulFound + wcslen(FOLDER_PREFIX), wstrFldId.length() - ulFound);

	//TODO: public 
	hr = OpenSubFolder(lpMsgStore, NULL, '/', lpLogger, blIsPublic, false, &lpRootFld);
	if(hr != hrSuccess)
	{
		lpLogger->Log(EC_LOGLEVEL_ERROR, "Cannot open IPM_SUBTREE Folder");
		goto exit;
	}

	// Hack Alert #47 -- get Inbox and Outbox as special folders
	if (wstrFldId.compare(L"Inbox") == 0) {
		hr = lpMsgStore->GetReceiveFolder(_T("IPM"), fMapiUnicode, &cbEntryID, &lpEntryID, NULL);
		if (hr != hrSuccess) {
			lpLogger->Log(EC_LOGLEVEL_ERROR, "Cannot open Inbox Folder, no Receive Folder EntryID: 0x%08X", hr);
			goto exit;
		}
	} else if (wstrFldId.compare(L"Outbox") == 0) {
		hr = HrGetOneProp(lpMsgStore, PR_IPM_OUTBOX_ENTRYID, &lpOutbox);
		if (hr != hrSuccess) {
			lpLogger->Log(EC_LOGLEVEL_ERROR, "Cannot open Outbox Folder, no PR_IPM_OUTBOX_ENTRYID: 0x%08X", hr);
			goto exit;
		}
		cbEntryID = lpOutbox->Value.bin.cb;
		lpEntryID = (LPENTRYID)lpOutbox->Value.bin.lpb;
	}
	if (cbEntryID && lpEntryID) {
		hr = lpMsgStore->OpenEntry(cbEntryID, lpEntryID, &IID_IMAPIFolder, MAPI_BEST_ACCESS, &ulObjType, (LPUNKNOWN*)lppUsrFld);
		if (hr != hrSuccess)
			lpLogger->Log(EC_LOGLEVEL_ERROR, "Cannot open %ls Folder: 0x%08X", wstrFldId.c_str(), hr);
		goto exit;
	}


	hr = lpRootFld->GetHierarchyTable(CONVENIENT_DEPTH, &lpHichyTable);
	if(hr != hrSuccess)
		goto exit;
	//When ENTRY_ID is use For Read Only Calendars assuming the folder id string 
	//not larger than lenght 50
	//FIXME: include some Entry-id identifier
	if(wstrFldId.size()> 50)
	{
		ulPropTagFldId = PR_ENTRYID;
		strBinEid = hex2bin(wstrFldId);
		sSpropVal.Value.bin.cb = strBinEid.size();
		sSpropVal.Value.bin.lpb = (LPBYTE)strBinEid.c_str();
	}
	else
	{
		// note: this is a custom zarafa named property, defined in libicalmapi/names.*
		ulPropTagFldId = CHANGE_PROP_TYPE(lpNamedProps->aulPropTag[PROP_FLDID], PT_UNICODE);
		sSpropVal.Value.lpszW = (LPWSTR)wstrFldId.c_str();
	}

	sSpropVal.ulPropTag = ulPropTagFldId;

	CREATE_RESTRICTION(lpRestrict);
	DATA_RES_PROPERTY(lpRestrict, (*lpRestrict), RELOP_EQ, ulPropTagFldId, &sSpropVal);

	hr = lpHichyTable->Restrict(lpRestrict,TBL_BATCH);
	if(hr != hrSuccess)
		goto exit;

	cbsize = 1;

	hr = MAPIAllocateBuffer(CbNewSPropTagArray(cbsize), (void **)&lpPropTagArr);
	if(hr != hrSuccess)
	{
		lpLogger->Log(EC_LOGLEVEL_ERROR,"Cannot allocate memory");
		goto exit;
	}

	//add PR_ENTRYID in setcolumns along with requested data.
	lpPropTagArr->cValues = 1;
	lpPropTagArr->aulPropTag[0] = PR_ENTRYID;

	hr = lpHichyTable->SetColumns(lpPropTagArr,0);
	if(hr != hrSuccess)
		goto exit;
	
	hr = lpHichyTable->QueryRows(1,0,&lpRows);
	if (hr != hrSuccess)
		goto exit;

	if (lpRows->cRows != 1) {
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}
	
	sbEid = lpRows->aRow[0].lpProps[0].Value.bin;
	
	hr = lpMsgStore->OpenEntry(sbEid.cb, (LPENTRYID)sbEid.lpb,NULL, MAPI_BEST_ACCESS, &ulObjType, (LPUNKNOWN *) &lpUsrFld);
	if(hr != hrSuccess)
		goto exit;

	*lppUsrFld = lpUsrFld;

exit:
	// Free either one. lpEntryID may point to lpOutbox memory.
	if (lpOutbox)
		MAPIFreeBuffer(lpOutbox);
	else if (lpEntryID)
		MAPIFreeBuffer(lpEntryID);

	if(lpRows)
		FreeProws(lpRows);

	if(lpRootFld)
		lpRootFld->Release();

	if(lpHichyTable)
		lpHichyTable->Release();
	
	if(lpPropTagArr)
		MAPIFreeBuffer(lpPropTagArr);

	if(lpRestrict)
		FREE_RESTRICTION(lpRestrict);

	return hr;
}

/**
 * Set supported-report-set properties, used by mac ical.app client
 *
 * @param[in,out]	lpsProperty		Pointer to WEBDAVPROPERTY structure to store the supported-report-set properties
 *
 * @return			HRESULT			Always set to hrSuccess
 */
HRESULT HrBuildReportSet(WEBDAVPROPERTY *lpsProperty)
{
	HRESULT hr = hrSuccess;
	int ulDepth = 0 ;
	WEBDAVITEM sDavItem;

	sDavItem.sDavValue.sPropName.strPropname = "supported-report";
	sDavItem.sDavValue.sPropName.strNS = CALDAVNSDEF;
	sDavItem.ulDepth = ulDepth ;
	lpsProperty->lstItems.push_back(sDavItem);

	sDavItem.sDavValue.sPropName.strPropname = "report";
	sDavItem.ulDepth = ulDepth + 1;
	lpsProperty->lstItems.push_back(sDavItem);

	sDavItem.sDavValue.sPropName.strPropname = "acl-principal-prop-set";
	sDavItem.ulDepth = ulDepth + 2 ;
	lpsProperty->lstItems.push_back(sDavItem);

	sDavItem.sDavValue.sPropName.strPropname = "supported-report";
	sDavItem.sDavValue.sPropName.strNS = CALDAVNSDEF;
	sDavItem.ulDepth = ulDepth ;
	lpsProperty->lstItems.push_back(sDavItem);

	sDavItem.sDavValue.sPropName.strPropname = "report";
	sDavItem.ulDepth = ulDepth + 1;
	lpsProperty->lstItems.push_back(sDavItem);

	sDavItem.sDavValue.sPropName.strPropname = "principal-match";
	sDavItem.ulDepth = ulDepth + 2 ;
	lpsProperty->lstItems.push_back(sDavItem);
	
	sDavItem.sDavValue.sPropName.strPropname = "supported-report";	
	sDavItem.sDavValue.sPropName.strNS = CALDAVNSDEF;
	sDavItem.ulDepth = ulDepth ;
	lpsProperty->lstItems.push_back(sDavItem);

	sDavItem.sDavValue.sPropName.strPropname = "report";
	sDavItem.ulDepth = ulDepth + 1;
	lpsProperty->lstItems.push_back(sDavItem);

	sDavItem.sDavValue.sPropName.strPropname = "principal-property-search";
	sDavItem.ulDepth = ulDepth + 2 ;
	lpsProperty->lstItems.push_back(sDavItem);
	
	sDavItem.sDavValue.sPropName.strPropname = "supported-report";	
	sDavItem.sDavValue.sPropName.strNS = CALDAVNSDEF;
	sDavItem.ulDepth = ulDepth ;
	lpsProperty->lstItems.push_back(sDavItem);

	sDavItem.sDavValue.sPropName.strPropname = "report";
	sDavItem.ulDepth = ulDepth + 1;
	lpsProperty->lstItems.push_back(sDavItem);

	sDavItem.sDavValue.sPropName.strPropname = "expand-property";
	sDavItem.ulDepth = ulDepth + 2 ;
	lpsProperty->lstItems.push_back(sDavItem);

	return hr;
}

/**
 * Set Acl properties, used by mac ical.app client
 *
 * @param[in,out]	lpsProperty		Pointer to WEBDAVPROPERTY structure to store the acl properties
 *
 * @return			HRESULT			Always set to hrSuccess
 */
HRESULT HrBuildACL(WEBDAVPROPERTY *lpsProperty)
{
	HRESULT hr = hrSuccess;
	int ulDepth = 0 ;
	WEBDAVITEM sDavItem;

	sDavItem.sDavValue.sPropName.strPropname = "privilege";
	sDavItem.sDavValue.sPropName.strNS = CALDAVNSDEF;
	sDavItem.ulDepth = ulDepth ;
	lpsProperty->lstItems.push_back(sDavItem);

	sDavItem.sDavValue.sPropName.strPropname = "all";
	sDavItem.ulDepth = ulDepth + 1;
	lpsProperty->lstItems.push_back(sDavItem);

	sDavItem.sDavValue.sPropName.strPropname = "privilege";
	sDavItem.ulDepth = ulDepth ;
	lpsProperty->lstItems.push_back(sDavItem);

	sDavItem.sDavValue.sPropName.strPropname = "read-current-user-privilege-set";
	sDavItem.ulDepth = ulDepth + 1;
	lpsProperty->lstItems.push_back(sDavItem);

	sDavItem.sDavValue.sPropName.strPropname = "privilege";
	sDavItem.ulDepth = ulDepth ;
	lpsProperty->lstItems.push_back(sDavItem);

	sDavItem.sDavValue.sPropName.strPropname = "read";
	sDavItem.ulDepth = ulDepth + 1;
	lpsProperty->lstItems.push_back(sDavItem);

	sDavItem.sDavValue.sPropName.strPropname = "privilege";
	sDavItem.ulDepth = ulDepth ;
	lpsProperty->lstItems.push_back(sDavItem);

	sDavItem.sDavValue.sPropName.strPropname = "write";
	sDavItem.ulDepth = ulDepth + 1;
	lpsProperty->lstItems.push_back(sDavItem);

	sDavItem.sDavValue.sPropName.strPropname = "privilege";
	sDavItem.ulDepth = ulDepth;
	lpsProperty->lstItems.push_back(sDavItem);

	sDavItem.sDavValue.sPropName.strPropname = "write-content";
	sDavItem.ulDepth = ulDepth + 1;
	lpsProperty->lstItems.push_back(sDavItem);

	sDavItem.sDavValue.sPropName.strPropname = "privilege";
	sDavItem.ulDepth = ulDepth;
	lpsProperty->lstItems.push_back(sDavItem);

	sDavItem.sDavValue.sPropName.strPropname = "write-properties";
	sDavItem.ulDepth = ulDepth + 1;
	lpsProperty->lstItems.push_back(sDavItem);

	sDavItem.sDavValue.sPropName.strPropname = "privilege";
	sDavItem.ulDepth = ulDepth;
	lpsProperty->lstItems.push_back(sDavItem);

	sDavItem.sDavValue.sPropName.strPropname = "unlock";
	sDavItem.ulDepth = ulDepth + 1;
	lpsProperty->lstItems.push_back(sDavItem);

	sDavItem.sDavValue.sPropName.strPropname = "privilege";
	sDavItem.ulDepth = ulDepth;
	lpsProperty->lstItems.push_back(sDavItem);

	sDavItem.sDavValue.sPropName.strPropname = "read-acl";
	sDavItem.ulDepth = ulDepth + 1;
	lpsProperty->lstItems.push_back(sDavItem);

	sDavItem.sDavValue.sPropName.strPropname = "privilege";
	sDavItem.ulDepth = ulDepth;
	lpsProperty->lstItems.push_back(sDavItem);

	sDavItem.sDavValue.sPropName.strPropname = "bind";
	sDavItem.ulDepth = ulDepth + 1;
	lpsProperty->lstItems.push_back(sDavItem);
	sDavItem.sDavValue.sPropName.strPropname = "privilege";
	sDavItem.ulDepth = ulDepth;
	lpsProperty->lstItems.push_back(sDavItem);

	sDavItem.sDavValue.sPropName.strPropname = "unbind";
	sDavItem.ulDepth = ulDepth + 1;
	lpsProperty->lstItems.push_back(sDavItem);

	return hr;
}

/**
 * Check if the input value is of type Timestamp, used by evolution client(eg. 20091211T152412Z)
 *
 * @param[in]	strValue	Input string to be checked
 * @return		bool		True if the string is of timestamp type
 */
bool IsValueTs(std::string strValue)
{
	bool retVal = false;
	
	if(strValue.length() == 16 && strValue.at(8) == 'T' && strValue.at(15) == 'Z' )
		retVal = true;
		
	return retVal;
}

/**
 * Convert a wchar_t to a char by simply masking the lower 7 bit.
 * Only the lower 7 bit are used since that's the only part thats
 * compatible.
 *
 * @param[in]	wc	The wide character to 'convert'.
 * @return		The lower 7 bit of wc.
 */
static inline char w2c(wchar_t wc) {
	ASSERT((wc & ~0x7f) == 0);
	return wc & 0x7f;
}

/**
 * Return the GUID value from the input string
 *
 * Input string is of format '/Calendar/asbxjk3-3980434-xn49cn4930.ics',
 * function returns 'asbxjk3-3980434-xn49cn4930'
 *
 * @param[in]	strInput	Input string contaning guid
 * @return		string		string countaing guid
 */
std::string StripGuid(const std::wstring &wstrInput)
{
	size_t ulFound = -1;
	size_t ulFoundSlash = -1;
	std::string strRetVal;

	ulFoundSlash = wstrInput.rfind('/');
	if(ulFoundSlash == string::npos)
		ulFoundSlash = 0;
	else
		ulFoundSlash++;

	ulFound = wstrInput.rfind(L".ics");
	if(ulFound != wstring::npos) {
		strRetVal.reserve(ulFound - ulFoundSlash);
		transform(wstrInput.begin() + ulFoundSlash, wstrInput.begin() + ulFound, back_inserter(strRetVal), &w2c);
	}

	return strRetVal;
}

/**
 * Get Users information like user's name, email address
 * @param[in]	lpSession			IMAPISession object pointer of the user
 * @param[in]	lpDefStore			Default store of the user
 * @param[out]	strEmailaddress		Email address of the user
 * @param[out]	strUserName			Name of the user
 * @param[out]	lppImailUser		IMailUser Object pointer of the user
 *
 * @return		HRESULT
 */
HRESULT HrGetUserInfo(IMAPISession *lpSession, IMsgStore *lpDefStore, std::string *lpstrEmailaddress, std::wstring *lpstrUserName, IMailUser **lppImailUser)
{
	HRESULT hr = hrSuccess;
	LPSPropValue lpSProp = NULL;
	IMailUser *lpMailUser = NULL;
	ULONG ulObjType = 0;
	ULONG cProps = 0;

	SizedSPropTagArray(2, sptaUserProps) = {2, {PR_SMTP_ADDRESS_A, PR_DISPLAY_NAME_W}};

	hr = HrGetOneProp(lpDefStore, PR_MAILBOX_OWNER_ENTRYID, &lpSProp);
	if(hr != hrSuccess)
		goto exit;
	
	hr = lpSession->OpenEntry(lpSProp->Value.bin.cb, (LPENTRYID)lpSProp->Value.bin.lpb, NULL, MAPI_BEST_ACCESS, &ulObjType, (LPUNKNOWN*)&lpMailUser);
	if(hr != hrSuccess)
		goto exit;

	MAPIFreeBuffer(lpSProp);
	lpSProp = NULL;

	hr = lpMailUser->GetProps((LPSPropTagArray)&sptaUserProps, 0, &cProps, &lpSProp);
	if(hr != hrSuccess)
		goto exit;

	if(lpstrEmailaddress)
		lpstrEmailaddress->assign(lpSProp[0].Value.lpszA);

	if(lpstrUserName)
		lpstrUserName->assign(lpSProp[1].Value.lpszW);

	if(lpMailUser && lppImailUser)
	{
		*lppImailUser = lpMailUser;
		lpMailUser = NULL;
	}

exit:
	if(lpSProp)
		MAPIFreeBuffer(lpSProp);

	if(lpMailUser)
		lpMailUser->Release();

	return hr;
}

/**
 * Get all calendar folder of a specified folder, also includes all sub folders
 * 
 * @param[in]	lpSession		IMAPISession object of the user
 * @param[in]	lpFolderIn		IMAPIFolder object for which all calendar are to returned(Optional, can be set to NULL if lpsbEid != NULL)
 * @param[in]	lpsbEid			EntryID of the Folder for which all calendar are to be returned(can be NULL if lpFolderIn != NULL)
 * @param[out]	lppTable		IMAPITable of the sub calendar of the folder
 * 
 * @return		HRESULT 
 */
HRESULT HrGetSubCalendars(IMAPISession *lpSession, IMAPIFolder *lpFolderIn, SBinary *lpsbEid, IMAPITable **lppTable)
{
	HRESULT hr = hrSuccess;
	IMAPIFolder *lpFolder = NULL;
	ULONG ulObjType = 0;
	IMAPITable *lpTable = NULL;
	SPropValue sPropVal;
	SRestriction *lpRestrict = NULL;
	bool FreeFolder = false;

	if(!lpFolderIn)
	{
		hr = lpSession->OpenEntry(lpsbEid->cb, (LPENTRYID)lpsbEid->lpb, NULL, MAPI_BEST_ACCESS, &ulObjType, (LPUNKNOWN *)&lpFolder);
		if(hr != hrSuccess)
			goto exit;
		FreeFolder = true;
	}
	else
		lpFolder = lpFolderIn;

	hr = lpFolder->GetHierarchyTable(CONVENIENT_DEPTH,&lpTable);
	if(hr != hrSuccess)
		goto exit;

	sPropVal.ulPropTag = PR_CONTAINER_CLASS_A;
	sPropVal.Value.lpszA = "IPF.Appointment";

	CREATE_RESTRICTION(lpRestrict);
	
	CREATE_RES_OR(lpRestrict, lpRestrict, 2);
	DATA_RES_PROPERTY(lpRestrict, lpRestrict->res.resOr.lpRes[0], RELOP_EQ, sPropVal.ulPropTag, &sPropVal);

	sPropVal.Value.lpszA = "IPF.Task";
	DATA_RES_PROPERTY(lpRestrict, lpRestrict->res.resOr.lpRes[1], RELOP_EQ, sPropVal.ulPropTag, &sPropVal);

	hr = lpTable->Restrict(lpRestrict,TBL_BATCH);
	if(hr != hrSuccess)
		goto exit;

	*lppTable = lpTable;

exit:
	if(lpRestrict)
		FREE_RESTRICTION(lpRestrict);
	
	if(FreeFolder && lpFolder)
		lpFolder->Release();

	return hr;
}

/**
 * Opens the store and folder of other user while accessing shared folders
 * 
 * @param[in]	lpSession		IMAPISession object of the user
 * @param[in]	lpUsrStore		Users default store
 * @param[in]	strUser			Name of the user whose store to be opened
 * @param[in]	strFolderPath	Full Path of the folder to be opened(eg.. "Calendar/Sub_folder")
 * @param[in]	blIsClMac		Boolean specifying if the client is mac
 * @param[in]	blCreateIfNF	Boolean to state for creation of folder if not found
 * @param[in]	lpLogger		Pointer to ECLogger	object
 * @param[out]	lpulFolderFlag	Flag which states the type of folder
 * @param[out]	lppSharedStore	Store of the other user
 * @param[out]	lppUsrFld		Folder of the other user
 *
 * @return		HRESULT
 * @retval		MAPI_E_NOT_FOUND	Folder specified in strFolderPath not found
 * @retval		MAPI_E_NO_ACCESS	Not enough permissions to open other user's folder
 */
HRESULT HrGetSharedFolder(IMAPISession *lpSession, IMsgStore *lpUsrStore, std::wstring wstrUser, std::wstring wstrFolderPath, bool blIsClMac, bool blCreateIfNF, ECLogger *lpLogger, ULONG *lpulFolderFlag, IMsgStore **lppSharedStore, IMAPIFolder **lppUsrFld)
{
	HRESULT hr = hrSuccess;
	LPEXCHANGEMANAGESTORE lpExchangeManageStore = NULL;
	SBinary sbSharedStoreEid = {0, 0};
	IMsgStore *lpSharedStore = NULL;
	LPSPropValue lpPropIpmEid = NULL;
	LPSPropTagArray lpNamedProps = NULL;
	IMAPIFolder *lpUsrFld = NULL;
	ULONG ulObjType = 0;
	ULONG ulFolderFlag = 0;

	hr = lpUsrStore->QueryInterface(IID_IExchangeManageStore, (void **)&lpExchangeManageStore);
	if(hr != hrSuccess)
	{
		lpLogger->Log(EC_LOGLEVEL_ERROR, "Error in QueryInterface while opening shared folder ,error code : 0x%08X", hr);
		goto exit;
	}

	hr = lpExchangeManageStore->CreateStoreEntryID(NULL, (LPTSTR)wstrUser.c_str(), OPENSTORE_USE_ADMIN_PRIVILEGE | OPENSTORE_TAKE_OWNERSHIP | MAPI_UNICODE, &sbSharedStoreEid.cb, (LPENTRYID *)&sbSharedStoreEid.lpb);
	if(hr != hrSuccess)
	{
		lpLogger->Log(EC_LOGLEVEL_ERROR, "Error in CreateStoreEntryID while opening shared folder ,error code : 0x%08X", hr);
		goto exit;
	}

	hr = lpSession->OpenMsgStore(0, sbSharedStoreEid.cb, (LPENTRYID)sbSharedStoreEid.lpb, &IID_IMsgStore, MAPI_BEST_ACCESS, &lpSharedStore);
	if(hr != hrSuccess)
	{
		lpLogger->Log(EC_LOGLEVEL_ERROR, "Error opening store of user %ls ,error code : 0x%08X", wstrUser.c_str(), hr);
		goto exit;
	}
	
	if(blIsClMac)
	{	
		hr = HrLookupNames(lpSharedStore, &lpNamedProps);
		if (hr != hrSuccess)
			goto exit;

		hr = HrGetOneProp(lpSharedStore, PR_IPM_SUBTREE_ENTRYID, &lpPropIpmEid);
		if (hr != hrSuccess)
			lpLogger->Log(EC_LOGLEVEL_DEBUG, "Unable to get Entryid of IPM_SUBTREE folder of user %ls, error code : 0x%08X", wstrUser.c_str(), hr);
		else
			hr = lpSharedStore->OpenEntry(lpPropIpmEid[0].Value.bin.cb, (LPENTRYID)lpPropIpmEid[0].Value.bin.lpb, &IID_IMAPIFolder, MAPI_BEST_ACCESS, &ulObjType, (LPUNKNOWN*)&lpUsrFld); 	
		
		//Open root Folder to get all subfolders from this folder
		if(hr == hrSuccess && wstrFolderPath.empty())
			goto done;
		//Open Default Calendar if IPM
		else if(wstrFolderPath.empty())
		{
			hr = HrOpenDefaultCalendar(lpSharedStore, lpLogger, &lpUsrFld);
			if(hr == hrSuccess)
				ulFolderFlag = DEFAULT_FOLDER | SHARED_FOLDER;
		}
		else 
		{
			hr = HrFindFolder(lpSharedStore, lpNamedProps, lpLogger, false, wstrFolderPath, &lpUsrFld);
			if(hr == hrSuccess)
				ulFolderFlag = SHARED_FOLDER;
		}

		if(hr != hrSuccess && !wstrFolderPath.empty())
		{
			hr = HrOpenUserFld(lpSharedStore, wstrFolderPath, false, false, lpLogger, &lpUsrFld);
			if(hr == hrSuccess)
				ulFolderFlag = SINGLE_FOLDER | SHARED_FOLDER;
		}

		if(hr == E_ACCESSDENIED)
		{
			// Do we really want to open the default calendar if we don't have access
			// to the calendar that was requested?
			lpLogger->Log(EC_LOGLEVEL_FATAL, "No access to requested folder, trying default folder.");
			hr = HrOpenDefaultCalendar(lpSharedStore, lpLogger, &lpUsrFld);
			if(hr == hrSuccess)
				ulFolderFlag = DEFAULT_FOLDER | SHARED_FOLDER;
		}
	}
	else if(wstrFolderPath.empty())
	{
		hr = HrOpenDefaultCalendar(lpSharedStore, lpLogger, &lpUsrFld);
		if(hr == hrSuccess)
			ulFolderFlag = DEFAULT_FOLDER | SHARED_FOLDER;
	}
	else
	{
		hr = HrOpenUserFld(lpSharedStore, wstrFolderPath, blCreateIfNF, false, lpLogger, &lpUsrFld);
		if(hr == hrSuccess)
			ulFolderFlag = SINGLE_FOLDER | SHARED_FOLDER;
	}

	if (hr != hrSuccess)
		goto exit;

done:
	if(lppUsrFld && lpUsrFld)
	{
		*lppUsrFld = lpUsrFld;
		lpUsrFld = NULL;
	}
	
	if(lppSharedStore && lpSharedStore)
	{
		*lppSharedStore = lpSharedStore;
		lpSharedStore = NULL;
	}

	if (lpulFolderFlag)
		*lpulFolderFlag = ulFolderFlag;
	
exit:
	if (lpNamedProps)
		MAPIFreeBuffer(lpNamedProps);

	if(sbSharedStoreEid.lpb)
		MAPIFreeBuffer(sbSharedStoreEid.lpb);

	if(lpSharedStore)
		lpSharedStore->Release();

	if(lpUsrFld)
		lpUsrFld->Release();

	if(lpPropIpmEid)
		MAPIFreeBuffer(lpPropIpmEid);

	if(lpExchangeManageStore)
		lpExchangeManageStore->Release();

	return hr;
}
/**
 * Check for delegate permissions on the store
 * @param[in]	lpDefStore		The users default store
 * @param[in]	lpSharedStore	The store of the other user
 *
 * @return		bool			True if permissions are set or false
 */
bool HasDelegatePerm(IMsgStore *lpDefStore, IMsgStore *lpSharedStore)
{
	HRESULT hr = hrSuccess;
	LPMESSAGE lpFbMessage = NULL;
	LPSPropValue lpProp = NULL;
	LPSPropValue lpMailBoxEid = NULL;
	IMAPIContainer *lpRootCont = NULL;
	ULONG ulType = 0;
	ULONG ulPos = 0;
	SBinary sbEid = {0,0};
	bool blFound = false;
	bool blDelegatePerm = false; // no permission

	hr = HrGetOneProp(lpDefStore, PR_MAILBOX_OWNER_ENTRYID, &lpMailBoxEid);
	if (hr != hrSuccess)
		goto exit;

	hr = lpSharedStore->OpenEntry(0, NULL, NULL, 0, &ulType, (LPUNKNOWN*)&lpRootCont);
	if (hr != hrSuccess)
		goto exit;

	hr = HrGetOneProp(lpRootCont, PR_FREEBUSY_ENTRYIDS, &lpProp);
	if (hr != hrSuccess)
		goto exit;

	if (lpProp->Value.MVbin.cValues > 1 && lpProp->Value.MVbin.lpbin[1].cb != 0)
		sbEid = lpProp->Value.MVbin.lpbin[1];
	else
		goto exit;

	hr = lpSharedStore->OpenEntry(sbEid.cb, (LPENTRYID)sbEid.lpb, NULL, MAPI_BEST_ACCESS, &ulType, (LPUNKNOWN*)&lpFbMessage);
	if (hr != hrSuccess)
		goto exit;

	if (lpProp)
	{
		MAPIFreeBuffer(lpProp);
		lpProp = NULL;
	}

	hr = HrGetOneProp(lpFbMessage, PR_SCHDINFO_DELEGATE_ENTRYIDS, &lpProp);
	if (hr != hrSuccess)
		goto exit;

	for (ULONG i = 0; i < lpProp->Value.MVbin.cValues ; i++)
	{
		if (lpProp->Value.MVbin.lpbin[i].cb == lpMailBoxEid->Value.bin.cb
			 && !memcmp((const void *) lpProp->Value.MVbin.lpbin[i].lpb, (const void *)lpMailBoxEid->Value.bin.lpb, lpMailBoxEid->Value.bin.cb))
		{
			blFound = true;
			ulPos = i;
			break;
		}
	}
	
	if (!blFound)
		goto exit;
	
	if (lpProp)
	{
		MAPIFreeBuffer(lpProp);
		lpProp = NULL;
	}

	hr = HrGetOneProp(lpFbMessage, PR_DELEGATE_FLAGS, &lpProp);
	if (hr != hrSuccess)
		goto exit;

	if(lpProp->Value.MVl.cValues >= ulPos && lpProp->Value.MVl.lpl[ulPos])
		blDelegatePerm = true;
	else
		blDelegatePerm = false;

exit:
	
	if (lpMailBoxEid)
		MAPIFreeBuffer(lpMailBoxEid);

	if (lpProp)
		MAPIFreeBuffer(lpProp);

	if (lpFbMessage)
		lpFbMessage->Release();

	if (lpRootCont)
		lpRootCont->Release();

	return blDelegatePerm;
}
/**
 * Check if the message is a private item
 *
 * @param[in]	lpMessage			message to be checked
 * @param[in]	ulPropIDPrivate		named property tag
 * @return		bool
 */
bool IsPrivate(LPMESSAGE lpMessage, ULONG ulPropIDPrivate)
{
	HRESULT hr = hrSuccess;
	LPSPropValue lpPropPrivate = NULL;
	bool bIsPrivate = false;

	hr = HrGetOneProp(lpMessage, ulPropIDPrivate, &lpPropPrivate);
	if (hr != hrSuccess)
		goto exit;
	
	if(lpPropPrivate->Value.b == TRUE)
		bIsPrivate = true;
	
exit:
	
	if(lpPropPrivate)
		MAPIFreeBuffer(lpPropPrivate);

	return bIsPrivate;
}

/**
 * Creates restriction to find calendar entries refrenced by strGuid.
 *
 * @param[in]	strGuid			Guid string of calendar entry requested by caldav client, in url-base64 mode
 * @param[in]	lpNamedProps	Named property tag array
 * @param[out]	lpsRectrict		Pointer to the restriction created
 *
 * @return		HRESULT
 * @retval		MAPI_E_INVALID_PARAMETER	null parameter is passed in lpsRectrict
 */
HRESULT HrMakeRestriction(const std::string &strGuid, LPSPropTagArray lpNamedProps, LPSRestriction *lpsRectrict)
{
	HRESULT hr = hrSuccess;
	LPSRestriction lpsRoot = NULL;
	std::string strFldPath;
	std::string strWebBase64(strGuid);
	std::string strBinGuid;
	std::string strBinOtherUID;
	std::wstring wstrGuid;
	SPropValue sSpropVal = {0};

	if (lpsRectrict == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}
	CREATE_RESTRICTION(lpsRoot);
	CREATE_RES_OR(lpsRoot, lpsRoot, 4);

	// convert guid to outlook format
	if (IsOutlookUid(strWebBase64))
		strBinGuid = hex2bin(strWebBase64);
	else
		HrMakeBinUidFromICalUid(strWebBase64, &strBinGuid);
	
	sSpropVal.Value.bin.cb = (ULONG)strBinGuid.size();
	sSpropVal.Value.bin.lpb = (LPBYTE)strBinGuid.c_str();
	sSpropVal.ulPropTag = CHANGE_PROP_TYPE(lpNamedProps->aulPropTag[PROP_GOID], PT_BINARY);		
	DATA_RES_PROPERTY(lpsRoot, lpsRoot->res.resOr.lpRes[0], RELOP_EQ, sSpropVal.ulPropTag, &sSpropVal);
	
	// converting guid to hex
	strBinOtherUID = hex2bin(strGuid);
	sSpropVal.ulPropTag = PR_ENTRYID;
	sSpropVal.Value.bin.cb = (ULONG)strBinOtherUID.size();
	sSpropVal.Value.bin.lpb = (LPBYTE)strBinOtherUID.c_str();
	
	// When CreateAndGetGuid() fails PR_ENTRYID is used as guid.
	DATA_RES_PROPERTY(lpsRoot, lpsRoot->res.resOr.lpRes[1], RELOP_EQ, PR_ENTRYID, &sSpropVal);

	// z-push iphone UIDs are not in Outlook format		
	sSpropVal.ulPropTag = CHANGE_PROP_TYPE(lpNamedProps->aulPropTag[PROP_GOID], PT_BINARY);
	DATA_RES_PROPERTY(lpsRoot, lpsRoot->res.resOr.lpRes[2], RELOP_EQ, sSpropVal.ulPropTag, &sSpropVal);

	wstrGuid.reserve(strGuid.size());
	copy(strGuid.begin(), strGuid.end(), back_inserter(wstrGuid));
	// Evolution UIDs
	sSpropVal.ulPropTag = CHANGE_PROP_TYPE(lpNamedProps->aulPropTag[PROP_APPTTSREF], PT_UNICODE);
	sSpropVal.Value.lpszW = (LPWSTR)wstrGuid.c_str();
	DATA_RES_PROPERTY(lpsRoot, lpsRoot->res.resOr.lpRes[3], RELOP_EQ, sSpropVal.ulPropTag, &sSpropVal);
	
exit:
	if (lpsRoot && lpsRectrict)
		*lpsRectrict = lpsRoot;

	return hr;
}
/**
 * Finds mapi message in the folder using the UID string.
 * UID string can be PR_ENTRYID/GlobalOjectId of the message.
 *
 * @param[in]	strGuid			Guid value of the message to be searched
 * @param[in]	lpUsrFld		Mapi folder in which the message has to be searched
 * @param[in]	lpNamedProps	Named property tag array
 * @param[out]	lppMessage		if found the mapi message is returned
 * @return		HRESULT
 * @retval		MAPI_E_NOT_FOUND	No message found containing the guid value. 
 */
HRESULT HrFindAndGetMessage(std::string strGuid, IMAPIFolder *lpUsrFld, LPSPropTagArray lpNamedProps, IMessage **lppMessage)
{
	HRESULT hr = hrSuccess;
	SBinary sbEid = {0,0};
	SRestriction *lpsRoot = NULL;
	SRowSet *lpValRows = NULL;
	IMAPITable *lpTable = NULL;
	IMessage *lpMessage = NULL;	
	ULONG ulObjType = 0;
	SizedSPropTagArray(1, lpPropTagArr)= {1, {PR_ENTRYID}};
	
	hr = HrMakeRestriction(strGuid, lpNamedProps, &lpsRoot);
	if (hr != hrSuccess)
		goto exit;
	
	hr = lpUsrFld->GetContentsTable(0, &lpTable);
	if(hr != hrSuccess)
		goto exit;

	hr = lpTable->SetColumns((LPSPropTagArray)&lpPropTagArr, 0);
	if(hr != hrSuccess)
		goto exit;

	hr = lpTable->Restrict(lpsRoot, TBL_BATCH);
	if(hr != hrSuccess)
		goto exit;
	
	hr = lpTable->QueryRows(1, 0, &lpValRows);
	if (hr != hrSuccess)
		goto exit;

	if (lpValRows->cRows != 1)
	{
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}
	
	if (PROP_TYPE(lpValRows->aRow[0].lpProps[0].ulPropTag) != PT_BINARY)
	{
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}
	sbEid = lpValRows->aRow[0].lpProps[0].Value.bin;

	hr = lpUsrFld->OpenEntry(sbEid.cb, (LPENTRYID)sbEid.lpb, NULL, MAPI_MODIFY, &ulObjType, (LPUNKNOWN*)&lpMessage);
	if (hr != hrSuccess)
		goto exit;
	
	*lppMessage = lpMessage;
	lpMessage = NULL;

exit:
	if(lpMessage)
		lpMessage->Release();

	if(lpTable)
		lpTable->Release();
	
	if(lpsRoot)
		FREE_RESTRICTION(lpsRoot);

	if(lpValRows)
		FreeProws(lpValRows);
	
	return hr;
}
/**
 * Retrieves users's entry id from global address book
 *
 * @param[in]	lpGabTable			Mapi table containing Global address book rows
 * @param[in]	strUser				User's email address
 * @param[out]	lppsbEid			EntryID of the user
 * @return		HRESULT
 * @retval		MAPI_E_NOT_FOUND	No user found to the corresponding email address
 */
HRESULT HrGetUserEid(IMAPITable *lpGabTable, const std::string &strUser, SBinary **lppsbEid)
{
	HRESULT hr = hrSuccess;
	SRestriction *lpRestrict = NULL;
	SPropValue sPropVal;
	SRowSet *lpValRows = NULL;
	convert_context converter;

	sPropVal.ulPropTag = PR_SMTP_ADDRESS_A;
	sPropVal.Value.lpszA = (LPSTR)strUser.c_str();
	SBinary *lpSbEid = NULL;

	CREATE_RESTRICTION(lpRestrict);
	DATA_RES_PROPERTY(lpRestrict, (*lpRestrict), RELOP_EQ, PR_SMTP_ADDRESS_A, &sPropVal);
	
	hr = lpGabTable->Restrict(NULL,TBL_BATCH);
	if (hr != hrSuccess)
		goto exit;

	hr = lpGabTable->Restrict(lpRestrict,TBL_BATCH);
	if (hr != hrSuccess)
		goto exit;

	hr = lpGabTable->QueryRows(1, 0, &lpValRows);
	if (hr != hrSuccess)
		goto exit;

	if (lpValRows->cRows != 1)
	{
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}
	hr = MAPIAllocateBuffer(sizeof(SBinary), (void**)&lpSbEid);
	if (hr != hrSuccess)
		goto exit;

	lpSbEid->cb = lpValRows->aRow[0].lpProps[0].Value.bin.cb;
	hr = MAPIAllocateMore(lpSbEid->cb,lpSbEid,  (void**)&lpSbEid->lpb);
	if (hr != hrSuccess)
		goto exit;

	memcpy(lpSbEid->lpb, lpValRows->aRow[0].lpProps[0].Value.bin.lpb,lpSbEid->cb);

	*lppsbEid = lpSbEid;
exit:
	if (lpValRows)
		FreeProws(lpValRows);
	
	if (lpRestrict)
		FREE_RESTRICTION(lpRestrict);

	
	return hr;

}

/**
 * Retrieves freebusy information of attendees and converts it to ical data
 *
 * @param[in]	lpMapiToIcal	Mapi to ical conversion object
 * @param[in]	lpFBSupport		IFreebusySupport object used to retrive freebusy information of attendee
 * @param[in]	lpGabTable		Mapi table containing users of global address book
 * @param[in]	lplstUsers		List of attendees whose freebusy is requested
 * @param[out]	lpFbInfo		Structure which stores the retrieved ical data for the attendees
 *
 * @return		HRESULT
 * @retval						Always returns hrSuccess, invalid users have empty string in ical data.
 */
HRESULT HrGetFreebusy(MapiToICal *lpMapiToIcal, IFreeBusySupport* lpFBSupport, IMAPITable *lpGabTable, std::list<std::string> *lplstUsers, WEBDAVFBINFO *lpFbInfo)
{
	HRESULT hr = hrSuccess;
	FBUser *lpUsers = NULL;
	IEnumFBBlock *lpEnumBlock = NULL;
	IFreeBusyData **lppFBData = NULL;
	FBBlock_1 *lpsFBblks = NULL;
	std::string strUser;
	std::string strMethod;
	std::string strIcal;
	ULONG cUsers = 0;
	ULONG cFBData = 0;
	ULONG ulFlag = M2IC_NO_VTIMEZONE; // 1 specifies to eliminated VTIMEZONE block
	FILETIME ftStart = {0,0};
	FILETIME ftEnd = {0,0};
	LONG lMaxblks = 100;
	LONG lblkFetched = 0;
	WEBDAVFBUSERINFO sWebFbUserInfo;
	SBinary *lpsbEid = NULL;
	std::list<std::string>::iterator itUsers;

	cUsers = lplstUsers->size();

	hr = MAPIAllocateBuffer(sizeof(FBUser)*cUsers, (void**)&lpUsers);
	if (hr != hrSuccess)
		goto exit;

	// Get the user entryid's
	itUsers = lplstUsers->begin();
	for (cUsers = 0; itUsers != lplstUsers->end(); itUsers++) {

		hr = HrGetUserEid(lpGabTable, *itUsers, &lpsbEid);
		if (hr != hrSuccess)
		{
			sWebFbUserInfo.strUser = *itUsers;
			sWebFbUserInfo.strIcal.clear();

			lpFbInfo->lstFbUserInfo.push_back(sWebFbUserInfo);
			hr = hrSuccess;		// reset hr 
			continue;
		}

		lpUsers[cUsers].m_cbEid = lpsbEid->cb;
		MAPIAllocateMore(lpsbEid->cb, lpUsers, (void**)&lpUsers[cUsers].m_lpEid);

		memcpy(lpUsers[cUsers].m_lpEid, lpsbEid->lpb, lpsbEid->cb);
		cUsers++;
	}
	
	if (cUsers == 0)
		goto exit;

	hr = MAPIAllocateBuffer(sizeof(IFreeBusyData*)*cUsers, (void**)&lppFBData);
	if (hr != hrSuccess)
		goto exit;
	
	// retrieve freebusy for the attendees
	hr = lpFBSupport->LoadFreeBusyData(cUsers, lpUsers, lppFBData, NULL, &cFBData);
	if (hr != hrSuccess)
		goto exit;

	UnixTimeToFileTime(lpFbInfo->tStart, &ftStart);
	UnixTimeToFileTime(lpFbInfo->tEnd, &ftEnd);
	strIcal.clear();

	itUsers = lplstUsers->begin();
	// iterate through all users
	for(ULONG i = 0; i < cUsers; i++)
	{
		if (!lppFBData[i])
			goto next;
		
		hr = lppFBData[i]->EnumBlocks(&lpEnumBlock, ftStart, ftEnd);
		if (hr != hrSuccess)
			goto next;
		
		hr = MAPIAllocateBuffer(sizeof(FBBlock_1)*lMaxblks, (void**)&lpsFBblks);
		if (hr != hrSuccess)
			goto next;
		
		// restrict the freebusy blocks
		hr = lpEnumBlock->Restrict(ftStart, ftEnd);
		if (hr != hrSuccess)
			goto next;
		
		hr = lpEnumBlock->Next(lMaxblks, lpsFBblks, &lblkFetched);
		if (hr != hrSuccess)
			goto next;
		
		// add freebusy blocks to ical data
		if (lblkFetched == 0)
			hr = lpMapiToIcal->AddBlocks(NULL, lblkFetched, lpFbInfo->tStart, lpFbInfo->tEnd, lpFbInfo->strOrganiser, *itUsers, lpFbInfo->strUID);		
		else
			hr = lpMapiToIcal->AddBlocks(lpsFBblks, lblkFetched, lpFbInfo->tStart, lpFbInfo->tEnd, lpFbInfo->strOrganiser, *itUsers, lpFbInfo->strUID);
		
		if (hr != hrSuccess)
			goto next;
		
		// retrieve VFREEBUSY ical data 
		hr = lpMapiToIcal->Finalize(ulFlag , &strMethod, &strIcal);
		
next:
		// ignoring ical data for unknown users.
		hr = hrSuccess;
		sWebFbUserInfo.strUser = *itUsers;
		sWebFbUserInfo.strIcal = strIcal ;
		lpFbInfo->lstFbUserInfo.push_back(sWebFbUserInfo);
		itUsers++;

		lpMapiToIcal->ResetObject();
		
		if(lpEnumBlock)
			lpEnumBlock->Release();

		if(lpsFBblks)
			MAPIFreeBuffer(lpsFBblks);
		
		lpEnumBlock = NULL;
		lpsFBblks = NULL;
		lblkFetched = 0;
		strIcal.clear();
	}

exit:
	if(lpUsers)
		MAPIFreeBuffer(lpUsers);
	
	if (lppFBData)
		MAPIFreeBuffer(lppFBData);
	return hr;
}

