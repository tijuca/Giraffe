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

#include "ECMAPIFolderPublic.h"

#include "Mem.h"
#include "ECGuid.h"
#include "edkguid.h"
#include "CommonUtil.h"
#include "Util.h"
#include "ClientUtil.h"
#include "ZarafaUtil.h"

#include "ECDebug.h"

#include <edkmdb.h>
#include <mapiext.h>

#include "stringutil.h"
#include "ECMsgStorePublic.h"
#include "ECMemTablePublic.h"

#include "favoritesutil.h"
#include "restrictionutil.h"

#include <charset/convstring.h>

#include "ECGetText.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


ECMAPIFolderPublic::ECMAPIFolderPublic(ECMsgStore *lpMsgStore, BOOL fModify, WSMAPIFolderOps *lpFolderOps, enumPublicEntryID ePublicEntryID) : 
		ECMAPIFolder(lpMsgStore, fModify, lpFolderOps, "IMAPIFolderPublic") 
{
	HrAddPropHandlers(PR_ACCESS,		GetPropHandler,		DefaultSetPropComputed, (void *)this);
	HrAddPropHandlers(PR_ACCESS_LEVEL,	GetPropHandler,		DefaultSetPropComputed, (void *)this);
	HrAddPropHandlers(PR_RIGHTS,		GetPropHandler,		DefaultSetPropComputed, (void *)this);

	HrAddPropHandlers(PR_ENTRYID,		GetPropHandler,		DefaultSetPropComputed, (void *)this);
	
	// FIXME: special for publicfolders, save in global profile
	HrAddPropHandlers(PR_DISPLAY_NAME,	GetPropHandler,		SetPropHandler, (void *)this);
	HrAddPropHandlers(PR_COMMENT,		GetPropHandler,		SetPropHandler, (void *)this);

	HrAddPropHandlers(PR_RECORD_KEY,	GetPropHandler, 	DefaultSetPropComputed, (void*) this);
	HrAddPropHandlers(PR_PARENT_ENTRYID,GetPropHandler, 	DefaultSetPropComputed, (void*) this);
	HrAddPropHandlers(PR_FOLDER_TYPE,	GetPropHandler, 	DefaultSetPropSetReal, (void*) this);
	
	HrAddPropHandlers(PR_FOLDER_CHILD_COUNT,	GetPropHandler,		DefaultSetPropComputed, (void *)this);
	HrAddPropHandlers(PR_SUBFOLDERS,			GetPropHandler,		DefaultSetPropComputed, (void *)this);

	HrAddPropHandlers(PR_ORIGINAL_ENTRYID,		GetPropHandler,		DefaultSetPropComputed, (void *)this, FALSE, TRUE);
	

	m_ePublicEntryID = ePublicEntryID;
}

ECMAPIFolderPublic::~ECMAPIFolderPublic(void)
{

}

HRESULT	ECMAPIFolderPublic::QueryInterface(REFIID refiid, void **lppInterface)
{
	REGISTER_INTERFACE(IID_ECMAPIFolderPublic, this);

	return ECMAPIFolder::QueryInterface(refiid, lppInterface);
}

HRESULT ECMAPIFolderPublic::Create(ECMsgStore *lpMsgStore, BOOL fModify, WSMAPIFolderOps *lpFolderOps, enumPublicEntryID ePublicEntryID, ECMAPIFolder **lppECMAPIFolder)
{
	HRESULT hr = hrSuccess;
	ECMAPIFolderPublic *lpMAPIFolder = NULL;

	lpMAPIFolder = new ECMAPIFolderPublic(lpMsgStore, fModify, lpFolderOps, ePublicEntryID);

	hr = lpMAPIFolder->QueryInterface(IID_ECMAPIFolder, (void **)lppECMAPIFolder);

	if(hr != hrSuccess)
		delete lpMAPIFolder;

	return hr;
}

HRESULT ECMAPIFolderPublic::GetPropHandler(ULONG ulPropTag, void* lpProvider, ULONG ulFlags, LPSPropValue lpsPropValue, void *lpParam, void *lpBase)
{
	HRESULT hr = hrSuccess;
	LPCTSTR lpszName = NULL;
	ECMAPIFolderPublic *lpFolder = (ECMAPIFolderPublic *)lpParam;

	switch(PROP_ID(ulPropTag)) {

	case PROP_ID(PR_FOLDER_TYPE):
		if (lpFolder->m_ePublicEntryID == ePE_PublicFolders || lpFolder->m_ePublicEntryID == ePE_IPMSubtree || lpFolder->m_ePublicEntryID == ePE_Favorites) {
			lpsPropValue->ulPropTag = PR_FOLDER_TYPE;
			lpsPropValue->Value.l = FOLDER_GENERIC;
		} else {
			hr = lpFolder->HrGetRealProp(PR_FOLDER_TYPE, ulFlags, lpBase, lpsPropValue);
		}
		break;
	case PROP_ID(PR_ACCESS):
		// FIXME: use MAPI_ACCESS_MODIFY for favorites, public, favo sub  folders to change the displayname and comment
		if (lpFolder->m_ePublicEntryID == ePE_IPMSubtree) {
			lpsPropValue->ulPropTag = PR_ACCESS;
			lpsPropValue->Value.l = MAPI_ACCESS_READ;
		}else if (lpFolder->m_ePublicEntryID == ePE_Favorites) {
			lpsPropValue->ulPropTag = PR_ACCESS;
			lpsPropValue->Value.l = MAPI_ACCESS_READ;
		} else {
			hr = lpFolder->HrGetRealProp(PR_ACCESS, ulFlags, lpBase, lpsPropValue);
			
			if (hr == hrSuccess && lpFolder->m_ePublicEntryID == ePE_FavoriteSubFolder)
				lpsPropValue->Value.l |= MAPI_ACCESS_READ | MAPI_ACCESS_DELETE; 
			else if(hr == hrSuccess && lpFolder->m_ePublicEntryID == ePE_PublicFolders)
				lpsPropValue->Value.l &= ~(MAPI_ACCESS_CREATE_CONTENTS | MAPI_ACCESS_CREATE_ASSOCIATED);
			
		}
		break;
	case PROP_ID(PR_ACCESS_LEVEL):
		if (lpFolder->m_ePublicEntryID == ePE_IPMSubtree || lpFolder->m_ePublicEntryID == ePE_FavoriteSubFolder) {
			lpsPropValue->ulPropTag = PR_ACCESS_LEVEL;
			lpsPropValue->Value.l = MAPI_MODIFY;
		} else if(lpFolder->m_ePublicEntryID == ePE_Favorites) {
			lpsPropValue->ulPropTag = PR_ACCESS_LEVEL;
			lpsPropValue->Value.l = 0;
		} else {
			hr = lpFolder->HrGetRealProp(PR_ACCESS_LEVEL, ulFlags, lpBase, lpsPropValue);
		}
		break;
	case PROP_ID(PR_RIGHTS):
		if (lpFolder->m_ePublicEntryID == ePE_IPMSubtree) {
			lpsPropValue->ulPropTag = PR_RIGHTS;
			lpsPropValue->Value.l = ecRightsFolderVisible | ecRightsReadAny;
		} else if (lpFolder->m_ePublicEntryID == ePE_Favorites) {
			lpsPropValue->ulPropTag = PR_RIGHTS;
			lpsPropValue->Value.l = ecRightsAll;
		} else if (lpFolder->m_ePublicEntryID == ePE_PublicFolders) {
			lpsPropValue->ulPropTag = PR_RIGHTS;
			lpsPropValue->Value.l = ecRightsAll &~ecRightsCreate;
		} else {
			hr = lpFolder->HrGetRealProp(PR_RIGHTS, ulFlags, lpBase, lpsPropValue);
		}
		break;
	case PROP_ID(PR_ENTRYID):
		if (lpFolder->m_ePublicEntryID == ePE_PublicFolders) {
			lpsPropValue->ulPropTag = PR_ENTRYID;
			hr = ::GetPublicEntryId(ePE_PublicFolders, ((ECMsgStorePublic*)lpFolder->GetMsgStore())->GetStoreGuid(), lpBase, &lpsPropValue->Value.bin.cb, (LPENTRYID*)&lpsPropValue->Value.bin.lpb);
		} else {
			hr = ECGenericProp::DefaultGetProp(PR_ENTRYID, lpProvider, ulFlags, lpsPropValue, lpParam, lpBase);
			if(hr == hrSuccess && lpFolder->m_ePublicEntryID == ePE_FavoriteSubFolder)
				((LPENTRYID)lpsPropValue->Value.bin.lpb)->abFlags[3] = ZARAFA_FAVORITE;
		}
		break;
	case PROP_ID(PR_DISPLAY_NAME):

		// FIXME: Should be from the global profile and/or gettext (PR_FAVORITES_DEFAULT_NAME)
		if (lpFolder->m_ePublicEntryID == ePE_PublicFolders) {
			lpszName = _("Public Folders");
		} else if (lpFolder->m_ePublicEntryID == ePE_Favorites) {
			lpszName = _("Favorites");
		} else if (lpFolder->m_ePublicEntryID == ePE_IPMSubtree) {
			lpszName = _T("IPM_SUBTREE");
		}

		if (lpszName)
		{
			if (PROP_TYPE(ulPropTag) == PT_UNICODE) {
				const std::wstring strTmp = convert_to<std::wstring>(lpszName);

				hr = MAPIAllocateMore((strTmp.size() + 1) * sizeof(WCHAR), lpBase, (void**)&lpsPropValue->Value.lpszW);
				if (hr != hrSuccess) 
					goto exit;

				wcscpy(lpsPropValue->Value.lpszW, strTmp.c_str());
				lpsPropValue->ulPropTag = PR_DISPLAY_NAME_W;
			} else {
				const std::string strTmp = convert_to<std::string>(lpszName);

				hr = MAPIAllocateMore(strTmp.size() + 1, lpBase, (void**)&lpsPropValue->Value.lpszA);
				if (hr != hrSuccess) 
					goto exit;

				strcpy(lpsPropValue->Value.lpszA, strTmp.c_str());
				lpsPropValue->ulPropTag = PR_DISPLAY_NAME_A;
			}
			
		} else {
			hr = lpFolder->HrGetRealProp(ulPropTag, ulFlags, lpBase, lpsPropValue);
		}
		break;
	case PROP_ID(PR_COMMENT):
		// FIXME: load the message class from shortcut (favorite) folder, see setprops
		hr = lpFolder->HrGetRealProp(ulPropTag, ulFlags, lpBase, lpsPropValue);
		break;
	case PROP_ID(PR_RECORD_KEY):
		// Use entryid as record key because it should be global unique in outlook.
		hr = ECMAPIFolderPublic::GetPropHandler(PR_ENTRYID, lpProvider, ulFlags, lpsPropValue, lpParam, lpBase);
		if (hr == hrSuccess) {
			if(lpFolder->m_ePublicEntryID == ePE_FavoriteSubFolder)
				((LPENTRYID)lpsPropValue->Value.bin.lpb)->abFlags[3] = ZARAFA_FAVORITE;

			lpsPropValue->ulPropTag = PR_RECORD_KEY;
		}
		break;
	case PROP_ID(PR_PARENT_ENTRYID):
		if (lpFolder->m_ePublicEntryID == ePE_IPMSubtree || lpFolder->m_ePublicEntryID == ePE_PublicFolders || lpFolder->m_ePublicEntryID == ePE_Favorites) {
			lpsPropValue->ulPropTag = PR_PARENT_ENTRYID;
			hr = ::GetPublicEntryId(ePE_IPMSubtree, ((ECMsgStorePublic*)lpFolder->GetMsgStore())->GetStoreGuid(), lpBase, &lpsPropValue->Value.bin.cb, (LPENTRYID*)&lpsPropValue->Value.bin.lpb);
		} else {
			hr = ECMAPIFolder::DefaultMAPIGetProp(PR_PARENT_ENTRYID, lpProvider, ulFlags, lpsPropValue, lpParam, lpBase);
		}
		break;
	case PROP_ID(PR_FOLDER_CHILD_COUNT):
		if (lpFolder->m_ePublicEntryID == ePE_IPMSubtree) {
			lpsPropValue->ulPropTag = PR_FOLDER_CHILD_COUNT;
			lpsPropValue->Value.ul = 2;
		} else {
			hr = ECMAPIFolder::GetPropHandler(PR_FOLDER_CHILD_COUNT, lpProvider, ulFlags, lpsPropValue, lpParam, lpBase);
		}
		break;
	case PROP_ID(PR_SUBFOLDERS):
		if (lpFolder->m_ePublicEntryID == ePE_IPMSubtree) {
			lpsPropValue->ulPropTag = PR_SUBFOLDERS;
			lpsPropValue->Value.b = TRUE;
		} else {
			hr = ECMAPIFolder::GetPropHandler(PR_SUBFOLDERS, lpProvider, ulFlags, lpsPropValue, lpParam, lpBase);
		}
		break;
	case PROP_ID(PR_DISPLAY_TYPE):
		if (lpFolder->m_ePublicEntryID == ePE_FavoriteSubFolder) {
			lpsPropValue->ulPropTag = PR_DISPLAY_TYPE;
			lpsPropValue->Value.ul = DT_FOLDER_LINK;
		}else {
			hr = lpFolder->HrGetRealProp(PR_DISPLAY_TYPE, ulFlags, lpBase, lpsPropValue);
		}
		break;
	case PROP_ID(PR_ORIGINAL_ENTRYID):
		// entryid on the server (only used for "Public Folders" folder)
		if (lpFolder->m_lpEntryId) {
			MAPIAllocateMore(lpFolder->m_cbEntryId, lpBase, (LPVOID*)&lpsPropValue->Value.bin.lpb);
			memcpy(lpsPropValue->Value.bin.lpb, lpFolder->m_lpEntryId, lpFolder->m_cbEntryId);

			lpsPropValue->Value.bin.cb = lpFolder->m_cbEntryId;

			hr = hrSuccess;
		} else {
			hr = MAPI_E_NOT_FOUND;
		}
		break;
	default:
		hr = MAPI_E_NOT_FOUND;
		break;
	}

exit:
	return hr;
}

HRESULT ECMAPIFolderPublic::SetPropHandler(ULONG ulPropTag, void* lpProvider, LPSPropValue lpsPropValue, void *lpParam)
{
	HRESULT hr = hrSuccess;
	ECMAPIFolderPublic *lpFolder = (ECMAPIFolderPublic *)lpParam;

	switch(PROP_ID(ulPropTag)) {
	case PROP_ID(PR_DISPLAY_NAME):
		if (lpFolder->m_ePublicEntryID == ePE_PublicFolders) {
			hr = MAPI_E_COMPUTED;
			// FIXME: save in profile, #define PR_PROFILE_ALLPUB_DISPLAY_NAME    PROP_TAG(PT_STRING8, pidProfileMin+0x16)
		} else if (lpFolder->m_ePublicEntryID == ePE_Favorites) {
			hr = MAPI_E_COMPUTED;
			// FIXME: save in profile, #define PR_PROFILE_FAVFLD_DISPLAY_NAME    PROP_TAG(PT_STRING8, pidProfileMin+0x0F)
		} else if(lpFolder->m_ePublicEntryID == ePE_FavoriteSubFolder) {
			hr = MAPI_E_COMPUTED;
			// FIXME: Save the property to private shortcut folder message
		}else {
			hr = lpFolder->HrSetRealProp(lpsPropValue);
		}
		break;
	case PROP_ID(PR_COMMENT):
		if (lpFolder->m_ePublicEntryID == ePE_PublicFolders) {
			hr = MAPI_E_COMPUTED;
			// FIXME: save in profile, #define PR_PROFILE_FAVFLD_COMMENT        PROP_TAG(PT_STRING8, pidProfileMin+0x15)
		} else if (lpFolder->m_ePublicEntryID == ePE_Favorites) {
			hr = MAPI_E_COMPUTED;
			// FIXME: save in profile, #define PR_PROFILE_FAVFLD_COMMENT        PROP_TAG(PT_STRING8, pidProfileMin+0x15)
		} else {
			hr = lpFolder->HrSetRealProp(lpsPropValue);
		}
		break;
	
	default:
		hr = MAPI_E_NOT_FOUND;
		break;
	}

	return hr;
}

HRESULT ECMAPIFolderPublic::GetContentsTable(ULONG ulFlags, LPMAPITABLE *lppTable)
{
	HRESULT hr = hrSuccess;
	ECMemTable *lpMemTable = NULL;
	ECMemTableView *lpView = NULL;
	LPSPropTagArray lpPropTagArray = NULL;
	SizedSPropTagArray(11, sPropsContentColumns) = {11, {PR_ENTRYID, PR_DISPLAY_NAME, PR_MESSAGE_FLAGS, PR_SUBJECT, PR_STORE_ENTRYID, PR_STORE_RECORD_KEY, PR_STORE_SUPPORT_MASK, PR_INSTANCE_KEY, PR_RECORD_KEY, PR_ACCESS, PR_ACCESS_LEVEL } };

	if( m_ePublicEntryID == ePE_IPMSubtree || m_ePublicEntryID == ePE_Favorites)
	{
		if (ulFlags & SHOW_SOFT_DELETES) {
			hr = MAPI_E_NO_SUPPORT;
			goto exit;
		}

		hr = Util::HrCopyUnicodePropTagArray(ulFlags, (LPSPropTagArray)&sPropsContentColumns, &lpPropTagArray);
		if(hr != hrSuccess)
			goto exit;

		hr = ECMemTable::Create(lpPropTagArray, PR_ROWID, &lpMemTable);
		if(hr != hrSuccess)
			goto exit;

		hr = lpMemTable->HrGetView(createLocaleFromName(""), ulFlags & MAPI_UNICODE, &lpView);
		if(hr != hrSuccess)
			goto exit;

		hr = lpView->QueryInterface(IID_IMAPITable, (void **)lppTable);
		if(hr != hrSuccess)
			goto exit;

	} else {
		hr = ECMAPIFolder::GetContentsTable(ulFlags, lppTable);
	}

exit:
	if (lpPropTagArray)
		MAPIFreeBuffer(lpPropTagArray);

	if (lpMemTable)
		lpMemTable->Release();

	if (lpView)
		lpView->Release();

	return hr;
}

HRESULT ECMAPIFolderPublic::GetHierarchyTable(ULONG ulFlags, LPMAPITABLE *lppTable)
{
	HRESULT hr = hrSuccess;
	ECMemTableView *lpView = NULL;
	ECMemTablePublic *lpMemTable = NULL;
	LPSPropValue lpProps = NULL;

	if( m_ePublicEntryID == ePE_IPMSubtree)
	{
		// FIXME: if exchange support CONVENIENT_DEPTH than we must implement this
		if ((ulFlags & SHOW_SOFT_DELETES) || (ulFlags & CONVENIENT_DEPTH)) {
			hr= MAPI_E_NO_SUPPORT;
			goto exit;
		}

		hr = ((ECMsgStorePublic*)GetMsgStore())->GetIPMSubTree()->HrGetView(createLocaleFromName(""), ulFlags, &lpView);
		if(hr != hrSuccess)
			goto exit;

		hr = lpView->QueryInterface(IID_IMAPITable, (void **)lppTable);
		if(hr != hrSuccess)
			goto exit;
	} else if( m_ePublicEntryID == ePE_Favorites || m_ePublicEntryID == ePE_FavoriteSubFolder) {

		// FIXME: if exchange support CONVENIENT_DEPTH than we must implement this
		if ((ulFlags & SHOW_SOFT_DELETES) || (ulFlags & CONVENIENT_DEPTH)) {
			hr= MAPI_E_NO_SUPPORT;
			goto exit;
		}

		hr = ECMemTablePublic::Create(this, &lpMemTable);
		if(hr != hrSuccess)
			goto exit;

		hr = lpMemTable->Init(ulFlags&MAPI_UNICODE);
		if(hr != hrSuccess)
			goto exit;

		hr = lpMemTable->HrGetView(createLocaleFromName(""), ulFlags & MAPI_UNICODE, &lpView);
		if(hr != hrSuccess)
			goto exit;

		hr = lpView->QueryInterface(IID_IMAPITable, (void **)lppTable);
		if(hr != hrSuccess)
			goto exit;
	} else {
		hr = ECMAPIFolder::GetHierarchyTable(ulFlags, lppTable);
	}

exit:
	if (lpView)
		lpView->Release();

	if (lpMemTable)
		lpMemTable->Release();

	if (lpProps)
		MAPIFreeBuffer(lpProps);

	return hr;
}

HRESULT ECMAPIFolderPublic::SaveChanges(ULONG ulFlags)
{
	HRESULT hr = hrSuccess;

	// Nothing to do

	return hr;
}

HRESULT ECMAPIFolderPublic::SetProps(ULONG cValues, LPSPropValue lpPropArray, LPSPropProblemArray *lppProblems)
{
	HRESULT hr = hrSuccess;

	hr = ECMAPIContainer::SetProps(cValues, lpPropArray, lppProblems);
	if (hr != hrSuccess)
		goto exit;

	if (lpStorage)
	{
		hr = ECMAPIContainer::SaveChanges(KEEP_OPEN_READWRITE);
		if (hr != hrSuccess)
			goto exit;
	}

exit:

	return hr;
}

HRESULT ECMAPIFolderPublic::DeleteProps(LPSPropTagArray lpPropTagArray, LPSPropProblemArray FAR * lppProblems)
{
	HRESULT hr = hrSuccess;

	hr = ECMAPIContainer::DeleteProps(lpPropTagArray, lppProblems);
	if (hr != hrSuccess)
		goto exit;

	if (lpStorage)
	{
		hr = ECMAPIContainer::SaveChanges(KEEP_OPEN_READWRITE);
		if (hr != hrSuccess)
			goto exit;
	}

exit:

	return hr;
}

HRESULT ECMAPIFolderPublic::OpenEntry(ULONG cbEntryID, LPENTRYID lpEntryID, LPCIID lpInterface, ULONG ulFlags, ULONG *lpulObjType, LPUNKNOWN *lppUnk)
{
	HRESULT hr = hrSuccess;
	unsigned int ulObjType = 0;

	if (cbEntryID > 0)
	{
		hr = HrGetObjTypeFromEntryId(cbEntryID, (LPBYTE)lpEntryID, &ulObjType);
		if(hr != hrSuccess)
			goto exit;

		if (ulObjType == MAPI_FOLDER && m_ePublicEntryID == ePE_FavoriteSubFolder)
			lpEntryID->abFlags[3] = ZARAFA_FAVORITE;
	}

	hr = ECMAPIFolder::OpenEntry(cbEntryID, lpEntryID, lpInterface, ulFlags, lpulObjType, lppUnk);
	
exit:

	return hr;
}

HRESULT ECMAPIFolderPublic::SetEntryId(ULONG cbEntryId, LPENTRYID lpEntryId)
{
	HRESULT hr = hrSuccess;
	
	if (m_ePublicEntryID == ePE_Favorites || m_ePublicEntryID == ePE_IPMSubtree) {
		hr = ECGenericProp::SetEntryId(cbEntryId, lpEntryId);
	}else {
		// With notification handler
		hr = ECMAPIFolder::SetEntryId(cbEntryId, lpEntryId);
	}
	
	return hr;
}

// @note if you change this function please look also at ECMAPIFolder::CopyFolder
HRESULT ECMAPIFolderPublic::CopyFolder(ULONG cbEntryID, LPENTRYID lpEntryID, LPCIID lpInterface, LPVOID lpDestFolder, LPTSTR lpszNewFolderName, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, ULONG ulFlags)
{
	HRESULT hr = hrSuccess;
	ULONG ulResult = 0;
	IMAPIFolder	*lpMapiFolder = NULL;
	LPSPropValue lpPropArray = NULL;
	GUID guidDest;
	GUID guidFrom;

	//Get the interface of destinationfolder
	if(lpInterface == NULL || *lpInterface == IID_IMAPIFolder)
		hr = ((IMAPIFolder*)lpDestFolder)->QueryInterface(IID_IMAPIFolder, (void**)&lpMapiFolder);
	else if(*lpInterface == IID_IMAPIContainer)
		hr = ((IMAPIContainer*)lpDestFolder)->QueryInterface(IID_IMAPIFolder, (void**)&lpMapiFolder);
	else if(*lpInterface == IID_IUnknown)
		hr = ((IUnknown*)lpDestFolder)->QueryInterface(IID_IMAPIFolder, (void**)&lpMapiFolder);
	else if(*lpInterface == IID_IMAPIProp)
		hr = ((IMAPIProp*)lpDestFolder)->QueryInterface(IID_IMAPIFolder, (void**)&lpMapiFolder);
	else
		hr = MAPI_E_INTERFACE_NOT_SUPPORTED;
	
	if(hr != hrSuccess)
		goto exit;

	// Get the destination entry ID
	hr = HrGetOneProp(lpMapiFolder, PR_ENTRYID, &lpPropArray);
	if(hr != hrSuccess)
		goto exit;

	// Check if it's  the same store of zarafa so we can copy/move fast
	if( IsZarafaEntryId(cbEntryID, (LPBYTE)lpEntryID) && 
		IsZarafaEntryId(lpPropArray[0].Value.bin.cb, lpPropArray[0].Value.bin.lpb) &&
		HrGetStoreGuidFromEntryId(cbEntryID, (LPBYTE)lpEntryID, &guidFrom) == hrSuccess && 
		HrGetStoreGuidFromEntryId(lpPropArray[0].Value.bin.cb, lpPropArray[0].Value.bin.lpb, &guidDest) == hrSuccess &&
		memcmp(&guidFrom, &guidDest, sizeof(GUID)) == 0 &&
		lpFolderOps != NULL)
	{
		// if the entryid a a publicfolders entryid just change the entryid to a server entryid
		if(((ECMsgStorePublic*)GetMsgStore())->ComparePublicEntryId(ePE_PublicFolders, lpPropArray[0].Value.bin.cb, (LPENTRYID)lpPropArray[0].Value.bin.lpb, &ulResult) == hrSuccess && ulResult == TRUE)
		{
			if(lpPropArray) {
				ECFreeBuffer(lpPropArray);
				lpPropArray = NULL;
			}

			hr = HrGetOneProp(lpMapiFolder, PR_ORIGINAL_ENTRYID, &lpPropArray);
			if(hr != hrSuccess)
				goto exit;
		}
		//FIXME: Progressbar
		hr = this->lpFolderOps->HrCopyFolder(cbEntryID, lpEntryID, lpPropArray[0].Value.bin.cb, (LPENTRYID)lpPropArray[0].Value.bin.lpb, convstring(lpszNewFolderName, ulFlags), ulFlags, 0);
			
	}else
	{
		// Support object handled de copy/move
		hr = this->GetMsgStore()->lpSupport->CopyFolder(&IID_IMAPIFolder, &this->m_xMAPIFolder, cbEntryID, lpEntryID, lpInterface, lpDestFolder, lpszNewFolderName, ulUIParam, lpProgress, ulFlags);
	}


exit:
	if(lpMapiFolder)
		lpMapiFolder->Release();
		
	if(lpPropArray)
		ECFreeBuffer(lpPropArray);

	return hr;
}

HRESULT ECMAPIFolderPublic::DeleteFolder(ULONG cbEntryID, LPENTRYID lpEntryID, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, ULONG ulFlags)
{
	HRESULT hr = hrSuccess;
	ULONG ulObjType = 0;
	LPMAPIFOLDER lpFolder = NULL;
	LPMAPIFOLDER lpShortcutFolder = NULL;
	LPSPropValue lpProp = NULL;

	if(ValidateZarafaEntryId(cbEntryID, (LPBYTE)lpEntryID, MAPI_FOLDER) == false) {
		hr = MAPI_E_INVALID_ENTRYID;
		goto exit;
	}

	if (cbEntryID > 4 && (lpEntryID->abFlags[3] & ZARAFA_FAVORITE) )
	{
		// remove the shortcut from the shortcut folder
		hr = OpenEntry(cbEntryID, lpEntryID, NULL, 0, &ulObjType, (LPUNKNOWN *)&lpFolder);
		if (hr != hrSuccess)
			goto exit;

		hr = HrGetOneProp(lpFolder, PR_SOURCE_KEY, &lpProp);
		if (hr != hrSuccess)
			goto exit;

		if (lpFolder) {
			lpFolder->Release();
			lpFolder = NULL;
		}

		hr = ((ECMsgStorePublic*)GetMsgStore())->GetDefaultShortcutFolder(&lpShortcutFolder);
		if (hr != hrSuccess)
			goto exit;

		hr = DelFavoriteFolder(lpShortcutFolder, lpProp);
		if (hr != hrSuccess)
			goto exit;
	} else {

		hr = ECMAPIFolder::DeleteFolder(cbEntryID, lpEntryID, ulUIParam, lpProgress, ulFlags);
	}

exit:
	if (lpFolder)
		lpFolder->Release();

	if (lpShortcutFolder)
		lpShortcutFolder->Release();

	if (lpProp)
		MAPIFreeBuffer(lpProp);

	return hr;
}

HRESULT ECMAPIFolderPublic::CopyMessages(LPENTRYLIST lpMsgList, LPCIID lpInterface, LPVOID lpDestFolder, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, ULONG ulFlags)
{
	HRESULT hr = hrSuccess;
	ULONG ulResult = 0;
	IMAPIFolder	*lpMapiFolder = NULL;
	LPSPropValue lpPropArray = NULL;

	if(lpMsgList == NULL || lpMsgList->cValues == 0)
		goto exit;

	if (lpMsgList->lpbin == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}
	
	//Get the interface of destinationfolder
	if(lpInterface == NULL || *lpInterface == IID_IMAPIFolder)
		hr = ((IMAPIFolder*)lpDestFolder)->QueryInterface(IID_IMAPIFolder, (void**)&lpMapiFolder);
	else if(*lpInterface == IID_IMAPIContainer)
		hr = ((IMAPIContainer*)lpDestFolder)->QueryInterface(IID_IMAPIFolder, (void**)&lpMapiFolder);
	else if(*lpInterface == IID_IUnknown)
		hr = ((IUnknown*)lpDestFolder)->QueryInterface(IID_IMAPIFolder, (void**)&lpMapiFolder);
	else if(*lpInterface == IID_IMAPIProp)
		hr = ((IMAPIProp*)lpDestFolder)->QueryInterface(IID_IMAPIFolder, (void**)&lpMapiFolder);
	else
		hr = MAPI_E_INTERFACE_NOT_SUPPORTED;
	
	if(hr != hrSuccess)
		goto exit;

	// Get the destination entry ID
	hr = HrGetOneProp(lpMapiFolder, PR_ENTRYID, &lpPropArray);
	if(hr != hrSuccess)
		goto exit;

	// if the destination is the publicfolders entryid, just block
	if(((ECMsgStorePublic*)GetMsgStore())->ComparePublicEntryId(ePE_PublicFolders, lpPropArray[0].Value.bin.cb, (LPENTRYID)lpPropArray[0].Value.bin.lpb, &ulResult) == hrSuccess && ulResult == TRUE)
	{
		hr = MAPI_E_NO_ACCESS;
		goto exit;
	}

	hr = ECMAPIFolder::CopyMessages(lpMsgList, lpInterface, lpDestFolder, ulUIParam, lpProgress, ulFlags);

exit:
	if (lpMapiFolder)
		lpMapiFolder->Release();

	if(lpPropArray)
		MAPIFreeBuffer(lpPropArray);

	return hr;
}

HRESULT ECMAPIFolderPublic::CreateMessage(LPCIID lpInterface, ULONG ulFlags, LPMESSAGE *lppMessage)
{
	if (m_ePublicEntryID == ePE_PublicFolders)
		return MAPI_E_NO_ACCESS;

	return ECMAPIFolder::CreateMessage(lpInterface, ulFlags, lppMessage);
}
