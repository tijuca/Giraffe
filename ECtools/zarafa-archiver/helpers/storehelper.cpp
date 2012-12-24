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

#include <platform.h>
#include <mapix.h>
#include <mapiutil.h>
#include <restrictionutil.h>

#include "storehelper.h"
using namespace std;

#include <mapi_ptr.h>

#include "mapiguidext.h"
#include "archiver-common.h"

#include "ECRestriction.h"

namespace za { namespace helpers {

/**
 * The version of the current search folder restrictions. The version is the same
 * for all three folders, so if one restriction changes, they all get a new version.
 *
 * version 1: The search used as in V1 of the archiver.
 * version 2: Check if the original source key and current sourcekey match when stubbed equals true.
 */
#define ARCHIVE_SEARCH_VERSION 2

StoreHelper::search_folder_info_t StoreHelper::s_infoSearchFolders[] = {
	{_T("Archive"), _T("This folder contains messages that are eligible for archiving"), &StoreHelper::SetupSearchArchiveFolder},
	{_T("Delete"), _T("This folder contains messages that are eligible for deletion"), &StoreHelper::SetupSearchDeleteFolder},
	{_T("Stub"), _T("This folder contains messages that are eligible for stubbing"), &StoreHelper::SetupSearchStubFolder}
};

/**
 * Create a new StoreHelper object.
 *
 * @param[in]	ptrMsgStore
 *					A MsgStorePtr that points to the message store for which to
 *					create a StoreHelper.
 * @param[out]	lpptrStoreHelper
 *					Pointer to a StoreHelperPtr that will be assigned the address
 *					of the new StoreHelper object.
 *
 * @return HRESULT
 */
HRESULT StoreHelper::Create(MsgStorePtr &ptrMsgStore, StoreHelperPtr *lpptrStoreHelper)
{
	HRESULT hr = hrSuccess;
	StoreHelperPtr ptrStoreHelper;
	
	try {
		ptrStoreHelper.reset(new StoreHelper(ptrMsgStore));
	} catch (bad_alloc &) {
		hr = MAPI_E_NOT_ENOUGH_MEMORY;
		goto exit;
	}
	
	hr = ptrStoreHelper->Init();
	if (hr != hrSuccess)
		goto exit;
	
	*lpptrStoreHelper = ptrStoreHelper;	// transfers ownership
	
exit:
	return hr;
}


/**
 * Constructor.
 */
StoreHelper::StoreHelper(MsgStorePtr &ptrMsgStore)
: MAPIPropHelper(ptrMsgStore.as<MAPIPropPtr>())
, m_ptrMsgStore(ptrMsgStore)
{ }

/**
 * Initialize a StoreHelper object.
 */
HRESULT StoreHelper::Init()
{
	HRESULT	hr = hrSuccess;
	
	hr = MAPIPropHelper::Init();
	if (hr != hrSuccess)
		goto exit;
	
	PROPMAP_INIT_NAMED_ID(ARCHIVE_STORE_ENTRYIDS, PT_MV_BINARY, PSETID_Archive, dispidStoreEntryIds)
	PROPMAP_INIT_NAMED_ID(ARCHIVE_ITEM_ENTRYIDS, PT_MV_BINARY, PSETID_Archive, dispidItemEntryIds)
	PROPMAP_INIT_NAMED_ID(ORIGINAL_SOURCEKEY, PT_BINARY, PSETID_Archive, dispidOrigSourceKey)
	PROPMAP_INIT_NAMED_ID(SEARCH_FOLDER_ENTRYIDS, PT_MV_BINARY, PSETID_Archive, dispidSearchFolderEntryIds)
	PROPMAP_INIT_NAMED_ID(STUBBED, PT_BOOLEAN, PSETID_Archive, dispidStubbed)
	PROPMAP_INIT_NAMED_ID(DIRTY, PT_BOOLEAN, PSETID_Archive, dispidDirty)
	PROPMAP_INIT_NAMED_ID(FLAGS, PT_LONG, PSETID_Archive, dispidFlags)
	PROPMAP_INIT_NAMED_ID(VERSION, PT_LONG, PSETID_Archive, dispidVersion)
	PROPMAP_INIT(m_ptrMsgStore)
	
exit:
	return hr;
}

/**
 * Destructor.
 */
StoreHelper::~StoreHelper()
{ }

/**
 * Open or optionaly create a folder by name from the IPM subtree.
 *
 * @param[in]	strFolder
 *					The name of the folder to open or create.
 * @param[in]	bCreate
 *					Set to true to create the folder if it doesn't exist.
 * @param[out]	lppFolder
 *					Pointer to a IMapiFolder pointer that will be assigned the address
 *					of the returned folderd.
 *
 * @return HRESULT
 */
HRESULT StoreHelper::GetFolder(const tstring &strFolder, bool bCreate, LPMAPIFOLDER *lppFolder)
{
	HRESULT hr = hrSuccess;
	MAPIFolderPtr ptrIpmSubtree;
	MAPIFolderPtr ptrFolder;
	
	hr = GetIpmSubtree(&ptrIpmSubtree);
	if (hr != hrSuccess)
		goto exit;
		
	hr = GetSubFolder(ptrIpmSubtree, strFolder, bCreate, &ptrFolder);
	if (hr != hrSuccess)
		goto exit;
		
	hr = ptrFolder->QueryInterface(IID_IMAPIFolder, (LPVOID*)lppFolder);
		
exit:
	return hr;
}

/**
 * Create or reinitialize the searchfolders used by the archiver. The search folders
 * need to be updated whenever the set of attached archives for a store change.
 *
 * @return HRESULT
 */
HRESULT StoreHelper::UpdateSearchFolders()
{
	HRESULT hr = hrSuccess;
	MAPIFolderPtr ptrSearchArchiveFolder;
	MAPIFolderPtr ptrSearchDeleteFolder;
	MAPIFolderPtr ptrSearchStubFolder;
	SPropValuePtr ptrSearchArchiveEntryId;
	SPropValuePtr ptrSearchDeleteEntryId;
	SPropValuePtr ptrSearchStubEntryId;
	SPropValuePtr ptrPropValue;

	hr = CreateSearchFolders(&ptrSearchArchiveFolder, &ptrSearchDeleteFolder, &ptrSearchStubFolder);
	if (hr != hrSuccess)
		goto exit;
	
	ASSERT(ptrSearchArchiveFolder);
	ASSERT(ptrSearchDeleteFolder);
	ASSERT(ptrSearchStubFolder);

	hr = HrGetOneProp(ptrSearchArchiveFolder, PR_ENTRYID, &ptrSearchArchiveEntryId);
	if (hr != hrSuccess)
		goto exit;
	
	hr = HrGetOneProp(ptrSearchDeleteFolder, PR_ENTRYID, &ptrSearchDeleteEntryId);
	if (hr != hrSuccess)
		goto exit;
		
	hr = HrGetOneProp(ptrSearchStubFolder, PR_ENTRYID, &ptrSearchStubEntryId);
	if (hr != hrSuccess)
		goto exit;
		
	hr = MAPIAllocateBuffer(sizeof(SPropValue), &ptrPropValue);
	if (hr != hrSuccess)
		goto exit;
		
	ptrPropValue->ulPropTag = PROP_SEARCH_FOLDER_ENTRYIDS;
	ptrPropValue->Value.MVbin.cValues = 3;
	
	hr = MAPIAllocateMore(3 * sizeof(*ptrPropValue->Value.MVbin.lpbin), ptrPropValue, (LPVOID*)&ptrPropValue->Value.MVbin.lpbin);
	if (hr != hrSuccess)
		goto exit;
	
	ptrPropValue->Value.MVbin.lpbin[0].cb = ptrSearchArchiveEntryId->Value.bin.cb;
	ptrPropValue->Value.MVbin.lpbin[0].lpb = ptrSearchArchiveEntryId->Value.bin.lpb;
	ptrPropValue->Value.MVbin.lpbin[1].cb = ptrSearchDeleteEntryId->Value.bin.cb;
	ptrPropValue->Value.MVbin.lpbin[1].lpb = ptrSearchDeleteEntryId->Value.bin.lpb;
	ptrPropValue->Value.MVbin.lpbin[2].cb = ptrSearchStubEntryId->Value.bin.cb;
	ptrPropValue->Value.MVbin.lpbin[2].lpb = ptrSearchStubEntryId->Value.bin.lpb;
	
	hr = HrSetOneProp(m_ptrMsgStore, ptrPropValue);
	if (hr != hrSuccess)
		goto exit;

exit:
	return hr;
}

/**
 * Get the IPM subtree.
 *
 * @param[out]	lppFolder
 *					Pointer to an IMAPIFolder pointer that will be assigned the
 *					address of the returned folder.
 *
 * @return HRESULT
 */
HRESULT StoreHelper::GetIpmSubtree(LPMAPIFOLDER *lppFolder)
{
	HRESULT	hr = hrSuccess;
	SPropValuePtr ptrPropValue;
	ULONG ulType = 0;

	if (!m_ptrIpmSubtree) {
		hr = HrGetOneProp(m_ptrMsgStore, PR_IPM_SUBTREE_ENTRYID, &ptrPropValue);
		if (hr != hrSuccess)
			goto exit;
		
		hr = m_ptrMsgStore->OpenEntry(ptrPropValue->Value.bin.cb, (LPENTRYID)ptrPropValue->Value.bin.lpb, &m_ptrIpmSubtree.iid, MAPI_BEST_ACCESS|fMapiDeferredErrors, &ulType, &m_ptrIpmSubtree);
		if (hr != hrSuccess)
			goto exit;
	}
	
	hr = m_ptrIpmSubtree->QueryInterface(IID_IMAPIFolder, (LPVOID*)lppFolder);
	
exit:
	return hr;
}

/**
 * Get the three search folders that are created with StoreHelper::UpdateSearchFolders.
 *
 * @param[out]	lppSearchArchiveFolder
 *					Pointer to an IMAPIFolder pointer that will be assigned the
 *					address of the searcholder containing the messages that are eligible
 * 					for archiving. These are basically unarchived messages that are not
 * 					flagged to be never archived.
 * @param[out]	lppSearchDeleteFolder
 *					Pointer to an IMAPIFolder pointer that will be assigned the
 *					address of the searcholder containing the messages that are eligible
 * 					for deletion. These are basically archived messages that are not flagged
 * 					to be never deleted.
 * @param[out]	lppSearchStubFolder
 *					Pointer to an IMAPIFolder pointer that will be assigned the
 *					address of the searcholder containing the messages that are eligible
 * 					for stubbing. These are basically all non-stubbed, archived messages that
 * 					are not flagged to be never stubbed.
 *
 * @return HRESULT
 */
HRESULT StoreHelper::GetSearchFolders(LPMAPIFOLDER *lppSearchArchiveFolder, LPMAPIFOLDER *lppSearchDeleteFolder, LPMAPIFOLDER *lppSearchStubFolder)
{
	HRESULT hr = hrSuccess;
	SPropValuePtr ptrPropValue;
	MAPIFolderPtr ptrSearchArchiveFolder;
	MAPIFolderPtr ptrSearchDeleteFolder;
	MAPIFolderPtr ptrSearchStubFolder;	// subset of SearchDeleteFolder.
	ULONG ulType = 0;
	bool bUpdateRef = false;
	
	hr = HrGetOneProp(m_ptrMsgStore, PROP_SEARCH_FOLDER_ENTRYIDS, &ptrPropValue);
	if (hr != hrSuccess && hr != MAPI_E_NOT_FOUND)
		goto exit;
		
	if (hr == hrSuccess && ptrPropValue->Value.MVbin.cValues == 3) {
		hr = m_ptrMsgStore->OpenEntry(ptrPropValue->Value.MVbin.lpbin[0].cb, (LPENTRYID)ptrPropValue->Value.MVbin.lpbin[0].lpb, &ptrSearchArchiveFolder.iid, fMapiDeferredErrors|MAPI_BEST_ACCESS, &ulType, &ptrSearchArchiveFolder);
		if (hr == hrSuccess)
			hr = CheckAndUpdateSearchFolder(ptrSearchArchiveFolder, esfArchive);
		else if (hr == MAPI_E_NOT_FOUND) {
			bUpdateRef = true;
			hr = CreateSearchFolder(esfArchive, &ptrSearchArchiveFolder);
		} if (hr != hrSuccess)
			goto exit;

		hr = m_ptrMsgStore->OpenEntry(ptrPropValue->Value.MVbin.lpbin[1].cb, (LPENTRYID)ptrPropValue->Value.MVbin.lpbin[1].lpb, &ptrSearchDeleteFolder.iid, fMapiDeferredErrors|MAPI_BEST_ACCESS, &ulType, &ptrSearchDeleteFolder);
		if (hr == hrSuccess)
			hr = CheckAndUpdateSearchFolder(ptrSearchDeleteFolder, esfDelete);
		else if (hr == MAPI_E_NOT_FOUND) {
			bUpdateRef = true;
			hr = CreateSearchFolder(esfDelete, &ptrSearchDeleteFolder);
		} if (hr != hrSuccess)
			goto exit;

		hr = m_ptrMsgStore->OpenEntry(ptrPropValue->Value.MVbin.lpbin[2].cb, (LPENTRYID)ptrPropValue->Value.MVbin.lpbin[2].lpb, &ptrSearchStubFolder.iid, fMapiDeferredErrors|MAPI_BEST_ACCESS, &ulType, &ptrSearchStubFolder);
		if (hr == hrSuccess)
			hr = CheckAndUpdateSearchFolder(ptrSearchStubFolder, esfStub);
		else if (hr == MAPI_E_NOT_FOUND) {
			bUpdateRef = true;
			hr = CreateSearchFolder(esfStub, &ptrSearchStubFolder);
		} if (hr != hrSuccess)
			goto exit;
	} else {
		bUpdateRef = true;
		hr = CreateSearchFolders(&ptrSearchArchiveFolder, &ptrSearchDeleteFolder, &ptrSearchStubFolder);
		if (hr != hrSuccess)
			goto exit;
	}

	if (bUpdateRef) {
		SPropValuePtr ptrSearchArchiveEntryId;
		SPropValuePtr ptrSearchDeleteEntryId;
		SPropValuePtr ptrSearchStubEntryId;
		
		ASSERT(ptrSearchArchiveFolder);
		ASSERT(ptrSearchDeleteFolder);
		ASSERT(ptrSearchStubFolder);

		hr = HrGetOneProp(ptrSearchArchiveFolder, PR_ENTRYID, &ptrSearchArchiveEntryId);
		if (hr != hrSuccess)
			goto exit;
		
		hr = HrGetOneProp(ptrSearchDeleteFolder, PR_ENTRYID, &ptrSearchDeleteEntryId);
		if (hr != hrSuccess)
			goto exit;
			
		hr = HrGetOneProp(ptrSearchStubFolder, PR_ENTRYID, &ptrSearchStubEntryId);
		if (hr != hrSuccess)
			goto exit;
			
		hr = MAPIAllocateBuffer(sizeof(SPropValue), &ptrPropValue);
		if (hr != hrSuccess)
			goto exit;
			
		ptrPropValue->ulPropTag = PROP_SEARCH_FOLDER_ENTRYIDS;
		ptrPropValue->Value.MVbin.cValues = 3;
		
		hr = MAPIAllocateMore(3 * sizeof(*ptrPropValue->Value.MVbin.lpbin), ptrPropValue, (LPVOID*)&ptrPropValue->Value.MVbin.lpbin);
		if (hr != hrSuccess)
			goto exit;
		
		ptrPropValue->Value.MVbin.lpbin[0].cb = ptrSearchArchiveEntryId->Value.bin.cb;
		ptrPropValue->Value.MVbin.lpbin[0].lpb = ptrSearchArchiveEntryId->Value.bin.lpb;
		ptrPropValue->Value.MVbin.lpbin[1].cb = ptrSearchDeleteEntryId->Value.bin.cb;
		ptrPropValue->Value.MVbin.lpbin[1].lpb = ptrSearchDeleteEntryId->Value.bin.lpb;
		ptrPropValue->Value.MVbin.lpbin[2].cb = ptrSearchStubEntryId->Value.bin.cb;
		ptrPropValue->Value.MVbin.lpbin[2].lpb = ptrSearchStubEntryId->Value.bin.lpb;
		
		hr = HrSetOneProp(m_ptrMsgStore, ptrPropValue);
		if (hr != hrSuccess)
			goto exit;
	}
	
	hr = ptrSearchArchiveFolder->QueryInterface(IID_IMAPIFolder, (LPVOID*)lppSearchArchiveFolder);
	if (hr != hrSuccess)
		goto exit;
	
	hr = ptrSearchDeleteFolder->QueryInterface(IID_IMAPIFolder, (LPVOID*)lppSearchDeleteFolder);
	if (hr != hrSuccess)
		goto exit;
	
	hr = ptrSearchStubFolder->QueryInterface(IID_IMAPIFolder, (LPVOID*)lppSearchStubFolder);
		
exit:
	return hr;
}

/**
 * Open or create a subfolder by name from another folder.
 *
 * @param[in]	ptrFolder
 *					MAPIFolderPtr that points to the folder from which a subfolder
 *					will be opened.
 * @param[in]	strFolder
 *					The name of the subfolder to open or create.
 * @param[in]	bCreate
 *					Set to true to create the subfolder if it doesn't exist.
 * @param[out]	lppFolder
 *					Pointer to an IMAPIFolder pointer that will be assigned the
 *					address of the returned folder.
 *
 * @return HRESULT
 */
HRESULT StoreHelper::GetSubFolder(MAPIFolderPtr &ptrFolder, const tstring &strFolder, bool bCreate, LPMAPIFOLDER *lppFolder)
{
	HRESULT hr = hrSuccess;
	MAPITablePtr ptrTable;
	SRowSetPtr ptrRowSet;
	MAPIFolderPtr ptrSubFolder;
	ULONG ulType = 0;
	
	SizedSPropTagArray(1, sptaFolderProps) = {1, {PR_ENTRYID}};
	
	SRestriction			sRestriction = {0};
	SPropValue				sResPropValue = {0};
	


	sRestriction.rt = RES_PROPERTY;
	sRestriction.res.resProperty.relop = RELOP_EQ;
	sRestriction.res.resProperty.ulPropTag = PR_DISPLAY_NAME;
	sRestriction.res.resProperty.lpProp = &sResPropValue;
	
	sResPropValue.ulPropTag = PR_DISPLAY_NAME;
	sResPropValue.Value.LPSZ = (LPTSTR)strFolder.c_str();
	
	
	
	hr = ptrFolder->GetHierarchyTable(fMapiDeferredErrors, &ptrTable);
	if (hr != hrSuccess)
		goto exit;
	
	hr = ptrTable->SetColumns((LPSPropTagArray)&sptaFolderProps, TBL_BATCH);
	if (hr != hrSuccess)
		goto exit;
	
	hr = ptrTable->FindRow(&sRestriction, BOOKMARK_BEGINNING, 0);
	if (hr == hrSuccess) {
	
		hr = ptrTable->QueryRows(1, TBL_NOADVANCE, &ptrRowSet);
		if (hr != hrSuccess)
			goto exit;
			
		if (ptrRowSet.size() != 1) {
			hr = MAPI_E_NOT_FOUND;
			goto exit;
		}
		
		hr = m_ptrMsgStore->OpenEntry(ptrRowSet[0].lpProps[0].Value.bin.cb, (LPENTRYID)ptrRowSet[0].lpProps[0].Value.bin.lpb, &ptrSubFolder.iid, MAPI_BEST_ACCESS, &ulType, &ptrSubFolder);
	
	} else if (hr == MAPI_E_NOT_FOUND && bCreate == true) {
	
		hr = ptrFolder->CreateFolder(FOLDER_GENERIC, (LPTSTR)strFolder.c_str(), NULL, &ptrSubFolder.iid, fMapiUnicode, &ptrSubFolder);
		if (hr != hrSuccess)
			goto exit;
		
		// Maybe set some class thingy!
	
	}
	if (hr != hrSuccess)
		goto exit;
		
		
	hr = ptrSubFolder->QueryInterface(IID_IMAPIFolder, (LPVOID*)lppFolder);

exit:
	return hr;
}

HRESULT StoreHelper::CheckAndUpdateSearchFolder(LPMAPIFOLDER lpSearchFolder, eSearchFolder esfWhich)
{
	HRESULT hr = hrSuccess;
	SPropValuePtr ptrVersion;

	if (lpSearchFolder == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	hr = HrGetOneProp(lpSearchFolder, PROP_VERSION, &ptrVersion);
	if (hr != hrSuccess && hr != MAPI_E_NOT_FOUND)
		goto exit;

	if (hr == hrSuccess && ptrVersion->Value.ul == ARCHIVE_SEARCH_VERSION)
		goto exit;

	hr = (this->*s_infoSearchFolders[esfWhich].fnSetup)(lpSearchFolder, NULL, NULL);

exit:
	return hr;
}

HRESULT StoreHelper::CreateSearchFolder(eSearchFolder esfWhich, LPMAPIFOLDER *lppSearchFolder)
{
	HRESULT hr = hrSuccess;
	ULONG ulType = 0;
	MAPIFolderPtr ptrRootContainer;
	MAPIFolderPtr ptrArchiveSearchRoot;

	if (lppSearchFolder == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	hr = m_ptrMsgStore->OpenEntry(0, NULL, &ptrRootContainer.iid, MAPI_BEST_ACCESS|fMapiDeferredErrors, &ulType, &ptrRootContainer);
	if (hr != hrSuccess)
		goto exit;
		
	hr = ptrRootContainer->CreateFolder(FOLDER_GENERIC, (LPTSTR)"Zarafa-Archive-Search-Root", NULL, &ptrArchiveSearchRoot.iid, fMapiDeferredErrors|OPEN_IF_EXISTS, &ptrArchiveSearchRoot);
	if (hr != hrSuccess)
		goto exit;

	hr = DoCreateSearchFolder(ptrArchiveSearchRoot, esfWhich, lppSearchFolder);

exit:
	return hr;
}

HRESULT StoreHelper::CreateSearchFolders(LPMAPIFOLDER *lppSearchArchiveFolder, LPMAPIFOLDER *lppSearchDeleteFolder, LPMAPIFOLDER *lppSearchStubFolder)
{
	HRESULT hr = hrSuccess;
	ULONG ulType = 0;
	MAPIFolderPtr ptrRootContainer;
	MAPIFolderPtr ptrArchiveSearchRoot;
	bool bWithErrors = false;

	if (lppSearchArchiveFolder == NULL || lppSearchDeleteFolder == NULL || lppSearchStubFolder == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	hr = m_ptrMsgStore->OpenEntry(0, NULL, &ptrRootContainer.iid, MAPI_BEST_ACCESS|fMapiDeferredErrors, &ulType, &ptrRootContainer);
	if (hr != hrSuccess)
		goto exit;
		
	hr = ptrRootContainer->CreateFolder(FOLDER_GENERIC, (LPTSTR)"Zarafa-Archive-Search-Root", NULL, &ptrArchiveSearchRoot.iid, fMapiDeferredErrors|OPEN_IF_EXISTS, &ptrArchiveSearchRoot);
	if (hr != hrSuccess)
		goto exit;

	// Create the common restrictions here and pass them through DoCreateSearchFolder

	hr = DoCreateSearchFolder(ptrArchiveSearchRoot, esfArchive, lppSearchArchiveFolder);
	if (hr != hrSuccess)
		bWithErrors = true;

	hr = DoCreateSearchFolder(ptrArchiveSearchRoot, esfDelete, lppSearchDeleteFolder);
	if (hr != hrSuccess)
		bWithErrors = true;

	hr = DoCreateSearchFolder(ptrArchiveSearchRoot, esfStub, lppSearchStubFolder);
	if (hr != hrSuccess)
		bWithErrors = true;

	hr = bWithErrors ? MAPI_W_ERRORS_RETURNED : hrSuccess;

exit:
	return hr;
}

HRESULT StoreHelper::DoCreateSearchFolder(LPMAPIFOLDER lpParent, eSearchFolder esfWhich, LPMAPIFOLDER *lppSearchFolder)
{
	HRESULT hr = hrSuccess;
	MAPIFolderPtr ptrSearchFolder;

	if (lpParent == NULL || lppSearchFolder == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	hr = lpParent->CreateFolder(FOLDER_SEARCH, (LPTSTR)s_infoSearchFolders[esfWhich].lpszName, (LPTSTR)s_infoSearchFolders[esfWhich].lpszDescription, &ptrSearchFolder.iid, OPEN_IF_EXISTS|fMapiUnicode, &ptrSearchFolder);
	if (hr != hrSuccess)
		goto exit;

	hr = (this->*s_infoSearchFolders[esfWhich].fnSetup)(ptrSearchFolder, NULL, NULL);
	if (hr != hrSuccess)
		goto exit;

	hr = ptrSearchFolder->QueryInterface(IID_IMAPIFolder, (LPVOID*)lppSearchFolder);

exit:
	return hr;
}

HRESULT StoreHelper::SetupSearchArchiveFolder(LPMAPIFOLDER lpSearchFolder, const ECRestriction *lpresClassCheck, const ECRestriction *lpresArchiveCheck)
{
	HRESULT hr = hrSuccess;
	ECOrRestriction resClassCheck;
	ECAndRestriction resArchiveCheck;
	MAPIFolderPtr ptrIpmSubtree;
	SPropValuePtr ptrPropEntryId;
	SPropValue sPropStubbed;
	EntryListPtr ptrEntryList;
	ECAndRestriction resArchiveFolder;
	SRestrictionPtr ptrRestriction;
	SPropValue sPropVersion;

	if (lpSearchFolder == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	if (!lpresClassCheck) {
		hr = GetClassCheckRestriction(&resClassCheck);
		if (hr != hrSuccess)
			goto exit;
		lpresClassCheck = &resClassCheck;
	}

	if (!lpresArchiveCheck) {
		hr = GetArchiveCheckRestriction(&resArchiveCheck);
		if (hr != hrSuccess)
			goto exit;
		lpresArchiveCheck = &resArchiveCheck;
	}

	hr = GetIpmSubtree(&ptrIpmSubtree);
	if (hr != hrSuccess)
		goto exit;
	
	hr = HrGetOneProp(ptrIpmSubtree, PR_ENTRYID, &ptrPropEntryId);
	if (hr != hrSuccess)
		goto exit;
	
	hr = MAPIAllocateBuffer(sizeof(ENTRYLIST), &ptrEntryList);
	if (hr != hrSuccess)
		goto exit;
	ptrEntryList->cValues = 1;
	
	hr = MAPIAllocateMore(sizeof(SBinary), ptrEntryList, (LPVOID*)&ptrEntryList->lpbin);
	if (hr != hrSuccess)
		goto exit;
	ptrEntryList->lpbin[0].cb = ptrPropEntryId->Value.bin.cb;
	ptrEntryList->lpbin[0].lpb = ptrPropEntryId->Value.bin.lpb;


	sPropStubbed.ulPropTag = PROP_STUBBED; sPropStubbed.Value.b = 1;

	// Create/Update the search folder that tracks unarchived message that are not flagged to be never archived
	resArchiveFolder.append(
		*lpresClassCheck +
		ECNotRestriction(
			ECAndRestriction(
				ECExistRestriction(PROP_STUBBED) +
				ECPropertyRestriction(RELOP_EQ, PROP_STUBBED, &sPropStubbed, ECRestriction::Cheap) +

				// The prop stub is only valid if the stub was not copied.
				ECExistRestriction(PROP_ORIGINAL_SOURCEKEY) +
				ECComparePropsRestriction(RELOP_EQ, PROP_ORIGINAL_SOURCEKEY, PR_SOURCE_KEY)
			)
		) +
		ECNotRestriction(
			ECAndRestriction(
				ECExistRestriction(PROP_FLAGS) +
				ECBitMaskRestriction(BMR_NEZ, PROP_FLAGS, ARCH_NEVER_ARCHIVE)
			)
		) +
		ECNotRestriction(*lpresArchiveCheck)
	);

	hr = resArchiveFolder.CreateMAPIRestriction(&ptrRestriction, ECRestriction::Cheap);
	if (hr != hrSuccess)
		goto exit;
		
	// Set the search criteria
	hr = lpSearchFolder->SetSearchCriteria(ptrRestriction, ptrEntryList, BACKGROUND_SEARCH|RECURSIVE_SEARCH);
	if (hr != hrSuccess)
		goto exit;

	sPropVersion.ulPropTag = PROP_VERSION;
	sPropVersion.Value.ul = ARCHIVE_SEARCH_VERSION;

	hr = HrSetOneProp(lpSearchFolder, &sPropVersion);

exit:
	return hr;
}

HRESULT StoreHelper::SetupSearchDeleteFolder(LPMAPIFOLDER lpSearchFolder, const ECRestriction *lpresClassCheck, const ECRestriction *lpresArchiveCheck)
{
	HRESULT hr = hrSuccess;
	ECOrRestriction resClassCheck;
	ECAndRestriction resArchiveCheck;
	MAPIFolderPtr ptrIpmSubtree;
	SPropValuePtr ptrPropEntryId;
	EntryListPtr ptrEntryList;
	ECAndRestriction resDeleteFolder;
	SRestrictionPtr ptrRestriction;
	SPropValue sPropVersion;

	if (lpSearchFolder == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	if (!lpresClassCheck) {
		hr = GetClassCheckRestriction(&resClassCheck);
		if (hr != hrSuccess)
			goto exit;
		lpresClassCheck = &resClassCheck;
	}

	if (!lpresArchiveCheck) {
		hr = GetArchiveCheckRestriction(&resArchiveCheck);
		if (hr != hrSuccess)
			goto exit;
		lpresArchiveCheck = &resArchiveCheck;
	}

	hr = GetIpmSubtree(&ptrIpmSubtree);
	if (hr != hrSuccess)
		goto exit;
	
	hr = HrGetOneProp(ptrIpmSubtree, PR_ENTRYID, &ptrPropEntryId);
	if (hr != hrSuccess)
		goto exit;
	
	hr = MAPIAllocateBuffer(sizeof(ENTRYLIST), &ptrEntryList);
	if (hr != hrSuccess)
		goto exit;
	ptrEntryList->cValues = 1;
	
	hr = MAPIAllocateMore(sizeof(SBinary), ptrEntryList, (LPVOID*)&ptrEntryList->lpbin);
	if (hr != hrSuccess)
		goto exit;
	ptrEntryList->lpbin[0].cb = ptrPropEntryId->Value.bin.cb;
	ptrEntryList->lpbin[0].lpb = ptrPropEntryId->Value.bin.lpb;


	// Create/Update the search folder that tracks all archived message that are not flagged to be never deleted or never stubbed
	resDeleteFolder.append(
		*lpresClassCheck +
		ECNotRestriction(
			ECAndRestriction(
				ECExistRestriction(PROP_FLAGS) +
				ECBitMaskRestriction(BMR_NEZ, PROP_FLAGS, ARCH_NEVER_DELETE)
			)
		) +
		*lpresArchiveCheck
	);

	hr = resDeleteFolder.CreateMAPIRestriction(&ptrRestriction, ECRestriction::Cheap);
	if (hr != hrSuccess)
		goto exit;
		
	// Set the search criteria
	hr = lpSearchFolder->SetSearchCriteria(ptrRestriction, ptrEntryList, BACKGROUND_SEARCH|RECURSIVE_SEARCH);
	if (hr != hrSuccess)
		goto exit;

	sPropVersion.ulPropTag = PROP_VERSION;
	sPropVersion.Value.ul = ARCHIVE_SEARCH_VERSION;

	hr = HrSetOneProp(lpSearchFolder, &sPropVersion);

exit:
	return hr;
}

HRESULT StoreHelper::SetupSearchStubFolder(LPMAPIFOLDER lpSearchFolder, const ECRestriction* /*lpresClassCheck*/, const ECRestriction *lpresArchiveCheck)
{
	HRESULT hr = hrSuccess;
	ECAndRestriction resArchiveCheck;
	MAPIFolderPtr ptrIpmSubtree;
	SPropValuePtr ptrPropEntryId;
	SPropValue sPropStubbed;
	SPropValue sPropMsgClass[2];
	EntryListPtr ptrEntryList;
	ECAndRestriction resStubFolder;
	SRestrictionPtr ptrRestriction;
	SPropValue sPropVersion;

	if (lpSearchFolder == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	if (!lpresArchiveCheck) {
		hr = GetArchiveCheckRestriction(&resArchiveCheck);
		if (hr != hrSuccess)
			goto exit;
		lpresArchiveCheck = &resArchiveCheck;
	}

	hr = GetIpmSubtree(&ptrIpmSubtree);
	if (hr != hrSuccess)
		goto exit;
	
	hr = HrGetOneProp(ptrIpmSubtree, PR_ENTRYID, &ptrPropEntryId);
	if (hr != hrSuccess)
		goto exit;
	
	hr = MAPIAllocateBuffer(sizeof(ENTRYLIST), &ptrEntryList);
	if (hr != hrSuccess)
		goto exit;
	ptrEntryList->cValues = 1;
	
	hr = MAPIAllocateMore(sizeof(SBinary), ptrEntryList, (LPVOID*)&ptrEntryList->lpbin);
	if (hr != hrSuccess)
		goto exit;
	ptrEntryList->lpbin[0].cb = ptrPropEntryId->Value.bin.cb;
	ptrEntryList->lpbin[0].lpb = ptrPropEntryId->Value.bin.lpb;


	sPropStubbed.ulPropTag = PROP_STUBBED; 
	sPropStubbed.Value.b = 1;

	sPropMsgClass[0].ulPropTag = PR_MESSAGE_CLASS; sPropMsgClass[0].Value.LPSZ = _T("IPM.Note");
	sPropMsgClass[1].ulPropTag = PR_MESSAGE_CLASS; sPropMsgClass[1].Value.LPSZ = _T("IPM.Note.");	// RES_CONTENT w FL_PREFIX

	// Create/Update the search folder that tracks non-stubbed archived message that are not flagged to be never stubbed.
	resStubFolder.append(
		ECOrRestriction(
			ECPropertyRestriction(RELOP_EQ, PR_MESSAGE_CLASS, &sPropMsgClass[0]) +
			ECContentRestriction(FL_PREFIX, PR_MESSAGE_CLASS, &sPropMsgClass[1])
		) +
		ECNotRestriction(
			ECAndRestriction(
				ECExistRestriction(PROP_STUBBED) +
				ECPropertyRestriction(RELOP_EQ, PROP_STUBBED, &sPropStubbed, ECRestriction::Cheap)
			)
		) +
		ECNotRestriction(
			ECAndRestriction(
				ECExistRestriction(PROP_FLAGS) +
				ECBitMaskRestriction(BMR_NEZ, PROP_FLAGS, ARCH_NEVER_STUB)
			)
		) +
		*lpresArchiveCheck
	);

	hr = resStubFolder.CreateMAPIRestriction(&ptrRestriction, ECRestriction::Cheap);
	if (hr != hrSuccess)
		goto exit;
		
	// Set the search criteria
	hr = lpSearchFolder->SetSearchCriteria(ptrRestriction, ptrEntryList, BACKGROUND_SEARCH|RECURSIVE_SEARCH);
	if (hr != hrSuccess)
		goto exit;

	sPropVersion.ulPropTag = PROP_VERSION;
	sPropVersion.Value.ul = ARCHIVE_SEARCH_VERSION;

	hr = HrSetOneProp(lpSearchFolder, &sPropVersion);

exit:
	return hr;
}

HRESULT StoreHelper::GetClassCheckRestriction(ECOrRestriction *lpresClassCheck)
{
	SPropValue sPropMsgClass[9];
	ECOrRestriction resClassCheck;

	sPropMsgClass[0].ulPropTag = PR_MESSAGE_CLASS; sPropMsgClass[0].Value.LPSZ = _T("IPM.Note");
	sPropMsgClass[1].ulPropTag = PR_MESSAGE_CLASS; sPropMsgClass[1].Value.LPSZ = _T("IPM.Note.");	// RES_CONTENT w FL_PREFIX
	sPropMsgClass[2].ulPropTag = PR_MESSAGE_CLASS; sPropMsgClass[2].Value.LPSZ = _T("IPM.Schedule.Meeting.Request");
	sPropMsgClass[3].ulPropTag = PR_MESSAGE_CLASS; sPropMsgClass[3].Value.LPSZ = _T("IPM.Schedule.Meeting.Resp.Pos");
	sPropMsgClass[4].ulPropTag = PR_MESSAGE_CLASS; sPropMsgClass[4].Value.LPSZ = _T("IPM.Schedule.Meeting.Resp.Neg");
	sPropMsgClass[5].ulPropTag = PR_MESSAGE_CLASS; sPropMsgClass[5].Value.LPSZ = _T("IPM.Schedule.Meeting.Resp.Tent");
	sPropMsgClass[6].ulPropTag = PR_MESSAGE_CLASS; sPropMsgClass[6].Value.LPSZ = _T("IPM.Schedule.Meeting.Canceled");
	sPropMsgClass[7].ulPropTag = PR_MESSAGE_CLASS; sPropMsgClass[7].Value.LPSZ = _T("Report.IPM.Note.NDR");
	sPropMsgClass[8].ulPropTag = PR_MESSAGE_CLASS; sPropMsgClass[8].Value.LPSZ = _T("Report.IPM.Note.IPNRN");

	// Build the message class restriction.
	resClassCheck.append(
		ECPropertyRestriction(RELOP_EQ, PR_MESSAGE_CLASS, &sPropMsgClass[0]) +
		ECContentRestriction(FL_PREFIX, PR_MESSAGE_CLASS, &sPropMsgClass[1]) +
		ECPropertyRestriction(RELOP_EQ, PR_MESSAGE_CLASS, &sPropMsgClass[2]) +
		ECPropertyRestriction(RELOP_EQ, PR_MESSAGE_CLASS, &sPropMsgClass[3]) +
		ECPropertyRestriction(RELOP_EQ, PR_MESSAGE_CLASS, &sPropMsgClass[4]) +
		ECPropertyRestriction(RELOP_EQ, PR_MESSAGE_CLASS, &sPropMsgClass[5]) +
		ECPropertyRestriction(RELOP_EQ, PR_MESSAGE_CLASS, &sPropMsgClass[6]) +
		ECPropertyRestriction(RELOP_EQ, PR_MESSAGE_CLASS, &sPropMsgClass[7]) +
		ECPropertyRestriction(RELOP_EQ, PR_MESSAGE_CLASS, &sPropMsgClass[8])
	);

	*lpresClassCheck = resClassCheck;

	return hrSuccess;
}

HRESULT StoreHelper::GetArchiveCheckRestriction(ECAndRestriction *lpresArchiveCheck)
{
	HRESULT hr = hrSuccess;
	SPropValue sPropDirty = {0};
	ObjectEntryList lstArchives;
	ECAndRestriction resArchiveCheck;

	sPropDirty.ulPropTag = PROP_DIRTY; 
	sPropDirty.Value.b = 1;

	resArchiveCheck.append(
		ECAndRestriction(
			ECExistRestriction(PROP_ORIGINAL_SOURCEKEY) +
			ECComparePropsRestriction(RELOP_EQ, PROP_ORIGINAL_SOURCEKEY, PR_SOURCE_KEY)
		) +
		ECNotRestriction(
			ECAndRestriction(
				ECExistRestriction(PROP_DIRTY) +
				ECPropertyRestriction(RELOP_EQ, PROP_DIRTY, &sPropDirty)
			)
		)
	);

	hr = GetArchiveList(&lstArchives);
	if (hr != hrSuccess)
		goto exit;

	// Build the restriction that checks that the message has been archived to all archives.
	resArchiveCheck.append(ECExistRestriction(PROP_ARCHIVE_STORE_ENTRYIDS));
	for (ObjectEntryList::const_iterator iArchive = lstArchives.begin(); iArchive != lstArchives.end(); ++iArchive) {
		SPropValue sPropFolderEntryId;

		sPropFolderEntryId.ulPropTag = (PROP_ARCHIVE_STORE_ENTRYIDS & ~MV_FLAG);
		sPropFolderEntryId.Value.bin.cb = iArchive->sStoreEntryId.size();
		sPropFolderEntryId.Value.bin.lpb = iArchive->sStoreEntryId;

		resArchiveCheck.append(ECPropertyRestriction(RELOP_EQ, PROP_ARCHIVE_STORE_ENTRYIDS, &sPropFolderEntryId));
	}

	*lpresArchiveCheck = resArchiveCheck;
	
exit:
	return hr;
}

}} // namespaces
