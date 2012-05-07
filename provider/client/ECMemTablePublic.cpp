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
#include "ECMemTablePublic.h"

#include "Mem.h"
#include "ECGuid.h"
#include "edkguid.h"
#include "Util.h"
#include "ClientUtil.h"

#include <edkmdb.h>
#include <mapiext.h>

#include "ECMsgStorePublic.h"
#include "restrictionutil.h"
#include "favoritesutil.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


//FIXME: add the classname "ECMemTablePublic"
ECMemTablePublic::ECMemTablePublic(ECMAPIFolderPublic *lpECParentFolder, SPropTagArray *lpsPropTags, ULONG ulRowPropTag) : ECMemTable(lpsPropTags, ulRowPropTag)
{
	m_lpECParentFolder = lpECParentFolder;
	if (m_lpECParentFolder)
		m_lpECParentFolder->AddRef();

	m_lpShortCutAdviseSink = NULL;
	m_lpShortcutTable = NULL;
	m_ulFlags = 0;
	m_ulRowId = 1;
}

ECMemTablePublic::~ECMemTablePublic(void)
{
	ECMAPFolderRelation::iterator	iterFolder;

	if (m_lpShortcutTable)
		m_lpShortcutTable->Release();

	if (m_lpShortCutAdviseSink)
		m_lpShortCutAdviseSink->Release();

	for (iterFolder = m_mapRelation.begin(); iterFolder != m_mapRelation.end(); iterFolder++)
	{
		if (iterFolder->second.ulAdviseConnectionId > 0)
			m_lpECParentFolder->GetMsgStore()->Unadvise(iterFolder->second.ulAdviseConnectionId);

		FreeRelation(&iterFolder->second);
	}

	if (m_lpECParentFolder)
		m_lpECParentFolder->Release();

}


HRESULT ECMemTablePublic::Create(ECMAPIFolderPublic *lpECParentFolder, ECMemTablePublic **lppECMemTable)
{
	HRESULT hr = hrSuccess;
	ECMemTablePublic *lpMemTable = NULL;
	
	SizedSPropTagArray(12, sPropsHierarchyColumns) = {12, { PR_ENTRYID, PR_DISPLAY_NAME, PR_CONTENT_COUNT, PR_CONTENT_UNREAD, PR_STORE_ENTRYID, PR_STORE_RECORD_KEY, PR_STORE_SUPPORT_MASK, PR_INSTANCE_KEY, PR_RECORD_KEY, PR_ACCESS, PR_ACCESS_LEVEL, PR_CONTAINER_CLASS} };

	lpMemTable = new ECMemTablePublic(lpECParentFolder, (LPSPropTagArray)&sPropsHierarchyColumns, PR_ROWID);

	hr = lpMemTable->QueryInterface(IID_ECMemTablePublic, (void **)lppECMemTable);

	return hr;
}

HRESULT ECMemTablePublic::QueryInterface(REFIID refiid, void **lppInterface)
{
	REGISTER_INTERFACE(IID_ECMemTable, this);
	REGISTER_INTERFACE(IID_ECMemTablePublic, this);

	return MAPI_E_INTERFACE_NOT_SUPPORTED;
}

/*
	Advise function to check if there something is changed in the shortcut folder of you private store.
	
	This is used to build the favorits tree
*/

LONG __stdcall AdviseShortCutCallback(void *lpContext, ULONG cNotif, LPNOTIFICATION lpNotif)
{
	if (lpContext == NULL) {
		return S_OK;
	}

	HRESULT hr = hrSuccess;
	LPSRowSet lpRows = NULL;
	ECMemTablePublic *lpMemTablePublic = (ECMemTablePublic*)lpContext;

	lpMemTablePublic->AddRef(); // Besure we have the object

	for(ULONG i=0; i < cNotif; i++)
	{
		if(lpNotif[i].ulEventType != fnevTableModified)
		{
			ASSERT(FALSE);
			continue;
		}

		// NOTE: ignore errors at all.
		switch (lpNotif[i].info.tab.ulTableEvent)
		{
			case TABLE_ROW_ADDED:
			case TABLE_ROW_MODIFIED:
				lpMemTablePublic->ModifyRow(&lpNotif[i].info.tab.propIndex.Value.bin, &lpNotif[i].info.tab.row);
				break;
			case TABLE_ROW_DELETED:
				lpMemTablePublic->DelRow(&lpNotif[i].info.tab.propIndex.Value.bin);
				break;
			case TABLE_CHANGED:

				lpMemTablePublic->HrClear();

				hr = lpMemTablePublic->m_lpShortcutTable->SeekRow(BOOKMARK_BEGINNING, 0, NULL);
				if (hr != hrSuccess)
					continue; // Next notification

				while(true)
				{
					hr = lpMemTablePublic->m_lpShortcutTable->QueryRows (1, 0, &lpRows);
					if (hr != hrSuccess)
						break; // Next notification

					if (lpRows->cRows == 0)
						break;

					lpMemTablePublic->ModifyRow(&lpRows->aRow[0].lpProps[SC_INSTANCE_KEY].Value.bin, &lpRows->aRow[0]);

					if (lpRows){ FreeProws(lpRows); lpRows = NULL; }

				} //while(true)
				
				break;
			default:
				break;
		}

	}

	if (lpRows)
		FreeProws(lpRows);

	lpMemTablePublic->Release();

	return S_OK;
}

LONG __stdcall AdviseFolderCallback(void *lpContext, ULONG cNotif, LPNOTIFICATION lpNotif)
{
	if (lpContext == NULL) {
		return S_OK;
	}

	ECMemTablePublic::ECMAPFolderRelation::iterator	iterFolder;
	ECMemTablePublic *lpMemTablePublic = (ECMemTablePublic*)lpContext;
	ULONG ulResult;
	SBinary sInstanceKey;

	lpMemTablePublic->AddRef(); // Besure we have the object

	for(ULONG i=0; i < cNotif; i++)
	{
		switch (lpNotif[i].ulEventType)
		{
			case fnevObjectModified:
			case fnevObjectDeleted:
				for (iterFolder = lpMemTablePublic->m_mapRelation.begin(); iterFolder != lpMemTablePublic->m_mapRelation.end(); iterFolder++)
				{
					if (lpMemTablePublic->m_lpECParentFolder->GetMsgStore()->CompareEntryIDs(iterFolder->second.cbEntryID, iterFolder->second.lpEntryID,lpNotif[i].info.obj.cbEntryID, lpNotif[i].info.obj.lpEntryID, 0, &ulResult) == hrSuccess && ulResult == TRUE)
					{

						sInstanceKey.cb = iterFolder->first.size();
						sInstanceKey.lpb = (LPBYTE)iterFolder->first.c_str();

						switch (lpNotif[i].ulEventType)
						{
							case fnevObjectModified:
								lpMemTablePublic->ModifyRow(&sInstanceKey, NULL);
								TRACE_MAPI(TRACE_ENTRY, "AdviseFolderCallback", "fnevObjectModified    fnevObjectModified");
								break;
							case fnevObjectDeleted:
								TRACE_MAPI(TRACE_ENTRY, "AdviseFolderCallback", "fnevObjectDeleted    fnevObjectDeleted");
								lpMemTablePublic->DelRow(&sInstanceKey);
								break;
						}
						
						break;
					}
				}
				break;
			//TODO: Move (Unknown what to update)
		}
	}
	
	lpMemTablePublic->Release();

	return S_OK;
}

HRESULT ECMemTablePublic::Init(ULONG ulFlags)
{
	HRESULT hr = hrSuccess;
	LPSPropValue lpProps = NULL;
	ULONG ulRowId = 0;
	IMAPIFolder *lpShortcutFolder = NULL;
	LPMAPITABLE lpShortcutTable = NULL;
	LPSRestriction lpRestriction = NULL;
	LPSRowSet lpRows = NULL;
	LPSPropValue lpPropTmp = NULL;
	ULONG ulConnection;

	m_ulFlags = ulFlags;

	// Get the messages to build a folder list
	if ( ((ECMsgStorePublic*)m_lpECParentFolder->GetMsgStore())->GetDefaultShortcutFolder(&lpShortcutFolder) == hrSuccess)
	{
		hr = lpShortcutFolder->GetContentsTable(ulFlags | MAPI_DEFERRED_ERRORS, &lpShortcutTable);
		if(hr != hrSuccess)
			goto exit;

		hr = lpShortcutTable->SetColumns(GetShortCutTagArray(), MAPI_DEFERRED_ERRORS);
		if(hr != hrSuccess)
			goto exit;


		// build restriction
		CREATE_RESTRICTION(lpRestriction);
		CREATE_RES_AND(lpRestriction, lpRestriction, 1);

		if (HrGetOneProp(&m_lpECParentFolder->m_xMAPIFolder, PR_SOURCE_KEY, &lpPropTmp) != hrSuccess)
		{
			CREATE_RES_NOT(lpRestriction, (&lpRestriction->res.resAnd.lpRes[0]));
			DATA_RES_EXIST(lpRestriction, lpRestriction->res.resAnd.lpRes[0].res.resNot.lpRes[0], PR_FAV_PARENT_SOURCE_KEY);
		}else {
			DATA_FP_RES_PROPERTY(lpRestriction, lpRestriction->res.resAnd.lpRes[0], RELOP_EQ, PR_FAV_PARENT_SOURCE_KEY, &m_lpECParentFolder->m_xMAPIFolder, PR_SOURCE_KEY);
		}

		if (lpPropTmp) { MAPIFreeBuffer(lpPropTmp); lpPropTmp = NULL; }


		hr  = lpShortcutTable->Restrict(lpRestriction, MAPI_DEFERRED_ERRORS);
		if (hr != hrSuccess)
			goto exit;
	
		// No advise needed because the client disable notifications
		// If you remove this check the webaccess favorites doesn't work.
		if(! (m_lpECParentFolder->GetMsgStore()->m_ulProfileFlags & EC_PROFILE_FLAGS_NO_NOTIFICATIONS) )
		{

			hr = HrAllocAdviseSink(AdviseShortCutCallback, this, &m_lpShortCutAdviseSink);
			if (hr != hrSuccess)
				goto exit;

			// NOTE: the advise will destruct at release time
			hr = lpShortcutTable->Advise(fnevTableModified, m_lpShortCutAdviseSink, &ulConnection);
			if (hr != hrSuccess)
				goto exit;
		}

		ulRowId = 1;

		while(true)
		{
			hr = lpShortcutTable->QueryRows (1, 0, &lpRows);
			if (hr != hrSuccess)
				goto exit;

			if (lpRows->cRows == 0)
				break;

			ModifyRow(&lpRows->aRow[0].lpProps[SC_INSTANCE_KEY].Value.bin, &lpRows->aRow[0]);

			if (lpRows){ FreeProws(lpRows); lpRows = NULL; }

		} //while(true)

		hr = lpShortcutTable->QueryInterface(IID_IMAPITable, (void **)&m_lpShortcutTable);
		if (hr != hrSuccess)
			goto exit;
	}

exit:
	if (lpProps)
		MAPIFreeBuffer(lpProps);

	if (lpPropTmp) 
		MAPIFreeBuffer(lpPropTmp);

	if (lpShortcutTable)
		lpShortcutTable->Release();

	if (lpShortcutFolder)
		lpShortcutFolder->Release();

	if (lpRestriction)
		MAPIFreeBuffer(lpRestriction);

	if (lpPropTmp)
		MAPIFreeBuffer(lpPropTmp);

	if (lpRows)
		FreeProws(lpRows);

	return hr;
}

/*
	lpInstanceKey	Instance key of the item
	lpsRow			is a propertie array from the shortcuts
*/
HRESULT ECMemTablePublic::ModifyRow(SBinary* lpInstanceKey, LPSRow lpsRow)
{
	HRESULT hr = hrSuccess;
	LPSPropValue lpProps = NULL;
	ULONG cProps = 0;
	SPropValue sKeyProp;
	IMAPIFolder *lpFolderReal = NULL;
	ULONG ulPropsFolder;
	LPSPropValue lpPropsFolder = NULL;
	ULONG ulObjType;
	ULONG cbEntryID = 0;
	ULONG cbFolderID = 0;
	LPENTRYID lpEntryID = NULL; //Do not free this
	LPENTRYID lpFolderID = NULL;
	std::string strInstanceKey;
	ECMAPFolderRelation::iterator	iterRel;
	ECKeyTable::UpdateType	ulUpdateType; 
	ULONG ulRowId;
	ULONG ulConnection = 0;
	LPMAPIADVISESINK lpFolderAdviseSink = NULL;

	SRestriction *lpRestriction = NULL;
	LPSRowSet lpsRowsInternal = NULL;
	SPropValue sPropTmp;

	t_sRelation sRelFolder = {0};

	SizedSPropTagArray(11, sPropsFolderReal) = {11, { PR_ACCESS, PR_ACCESS_LEVEL, PR_STORE_ENTRYID, PR_STORE_RECORD_KEY, PR_STORE_SUPPORT_MASK, PR_ACCESS_LEVEL, PR_CONTENT_COUNT, PR_CONTENT_UNREAD, PR_CONTAINER_CLASS, PR_ENTRYID } };

	if (lpInstanceKey == NULL) {
		ASSERT(FALSE);
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	strInstanceKey.assign((char*)lpInstanceKey->lpb, lpInstanceKey->cb);

	iterRel = m_mapRelation.find(strInstanceKey);

	if (iterRel != m_mapRelation.end() ) {

		sRelFolder = iterRel->second;
		ulRowId = sRelFolder.ulRowID;

		ulUpdateType = ECKeyTable::TABLE_ROW_MODIFY;

		cbEntryID = sRelFolder.cbEntryID;
		lpEntryID = sRelFolder.lpEntryID;
	} else {
		ulRowId = m_ulRowId;
		ulUpdateType = ECKeyTable::TABLE_ROW_ADD;

		if (lpsRow == NULL || lpsRow->lpProps[SC_FAV_PUBLIC_SOURCE_KEY].ulPropTag != PR_FAV_PUBLIC_SOURCE_KEY) {
			ASSERT(FALSE);
			hr = MAPI_E_INVALID_PARAMETER;
			goto exit;
		}

		hr = ((ECMsgStorePublic*)m_lpECParentFolder->GetMsgStore())->EntryIDFromSourceKey(lpsRow->lpProps[SC_FAV_PUBLIC_SOURCE_KEY].Value.bin.cb, lpsRow->lpProps[SC_FAV_PUBLIC_SOURCE_KEY].Value.bin.lpb, 0, NULL, &cbFolderID, &lpFolderID);
		if (hr != hrSuccess)
			goto exit;

		cbEntryID = cbFolderID;
		lpEntryID = lpFolderID;
	}


	cProps = 0;

	hr = MAPIAllocateBuffer(sizeof(SPropValue) * 20, (void**)&lpProps);
	if(hr != hrSuccess)
		goto exit;

	// Default table rows
	lpProps[cProps].ulPropTag = PR_ROWID;
	lpProps[cProps++].Value.ul = ulRowId;

	lpProps[cProps].ulPropTag = PR_RECORD_KEY;
	lpProps[cProps].Value.bin.cb = cbEntryID;
	lpProps[cProps].Value.bin.lpb = (LPBYTE)lpEntryID;
	cProps++;

	// Set this folder as parent
	if (ECGenericProp::DefaultGetProp(PR_ENTRYID, m_lpECParentFolder->GetMsgStore(), 0, &lpProps[cProps], m_lpECParentFolder, lpProps) == hrSuccess) {
		lpProps[cProps].ulPropTag = PR_PARENT_ENTRYID;

		((LPENTRYID)lpProps[cProps].Value.bin.lpb)->abFlags[3] =  ZARAFA_FAVORITE;
		cProps++;
	}

	lpProps[cProps].ulPropTag = PR_DISPLAY_TYPE;
	lpProps[cProps++].Value.ul = DT_FOLDER_LINK;

	//FIXME: check if there are subfolders. Do a restriction on the shortcut folder with this folder sourcekey as parent source

	lpProps[cProps].ulPropTag = PR_SUBFOLDERS;
	lpProps[cProps++].Value.b = TRUE;

	/////////////////////////////////////////////////////
	// Properties from the real folder
	//
	if (ulUpdateType == ECKeyTable::TABLE_ROW_ADD) {
		hr = m_lpECParentFolder->OpenEntry(cbEntryID, lpEntryID, &IID_IMAPIFolder, MAPI_BEST_ACCESS, &ulObjType, (LPUNKNOWN *)&lpFolderReal);
		if(hr != hrSuccess)
			goto exit;
		
		// No advise needed because the client disable notifications
		// If you remove this check the webaccess favorites doesn't work.
		if(! (m_lpECParentFolder->GetMsgStore()->m_ulProfileFlags & EC_PROFILE_FLAGS_NO_NOTIFICATIONS) )
		{
			hr = HrAllocAdviseSink(AdviseFolderCallback, this, &lpFolderAdviseSink);	
			if (hr != hrSuccess)
				goto exit;

			hr = m_lpECParentFolder->GetMsgStore()->Advise(cbEntryID, lpEntryID, fnevObjectModified|fnevObjectCreated|fnevObjectMoved|fnevObjectDeleted, lpFolderAdviseSink, &ulConnection);
			if (hr != hrSuccess)
				goto exit;
		}

	}else {
		if (sRelFolder.lpFolder)
			hr = sRelFolder.lpFolder->QueryInterface(IID_IMAPIFolder, (void **)&lpFolderReal);
		else
			hr = MAPI_E_CALL_FAILED;

		if(hr != hrSuccess)
			goto exit;

		// Get shortcut folder information
		if(lpsRow == NULL)
		{
			sPropTmp.ulPropTag = PR_INSTANCE_KEY;
			sPropTmp.Value.bin = *lpInstanceKey;
			
			CREATE_RESTRICTION(lpRestriction);
			CREATE_RES_AND(lpRestriction, lpRestriction, 1);
			
			DATA_RES_PROPERTY(lpRestriction, lpRestriction->res.resAnd.lpRes[0], RELOP_EQ, PR_INSTANCE_KEY, &sPropTmp);

			hr = m_lpShortcutTable->FindRow(lpRestriction, BOOKMARK_BEGINNING, 0);
			if (hr != hrSuccess)
				goto exit;

			hr = m_lpShortcutTable->QueryRows (1, 0, &lpsRowsInternal);
			if (hr != hrSuccess)
				goto exit;

			if (lpsRowsInternal->cRows == 0) {
				hr = MAPI_E_CALL_FAILED;
				goto exit;
			}

			lpsRow = lpsRowsInternal->aRow;
		}
	}

	// Set the name of the folder, use alias if available otherwise displayname
	lpProps[cProps].ulPropTag = PR_DISPLAY_NAME;
	if (lpsRow != NULL && lpsRow->cValues == SHORTCUT_NUM && lpsRow->lpProps[SC_FAV_DISPLAY_ALIAS].ulPropTag == PR_FAV_DISPLAY_ALIAS) {
		lpProps[cProps++].Value.lpszA = lpsRow->lpProps[SC_FAV_DISPLAY_ALIAS].Value.lpszA;
	}else if (lpsRow != NULL && lpsRow->cValues == SHORTCUT_NUM && lpsRow->lpProps[SC_FAV_DISPLAY_NAME].ulPropTag == PR_FAV_DISPLAY_NAME) {
		lpProps[cProps++].Value.lpszA = lpsRow->lpProps[SC_FAV_DISPLAY_NAME].Value.lpszA;
	} else {
		ASSERT(FALSE);
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	hr = lpFolderReal->GetProps((LPSPropTagArray)&sPropsFolderReal, m_ulFlags, &ulPropsFolder, &lpPropsFolder);
	if (FAILED(hr))
		goto exit;
	else
		hr = hrSuccess;

	for (ULONG i=0; i < ulPropsFolder; i++)
	{
		if (PROP_TYPE(lpPropsFolder[i].ulPropTag) == PT_ERROR)
			continue;

		if (lpPropsFolder[i].ulPropTag == PR_ACCESS) {
			lpPropsFolder[i].Value.ul &=~(MAPI_ACCESS_CREATE_HIERARCHY | MAPI_ACCESS_CREATE_ASSOCIATED);
			lpPropsFolder[i].Value.ul |=MAPI_ACCESS_DELETE;
		}

		if (lpPropsFolder[i].ulPropTag == PR_ENTRYID) {
			((LPENTRYID)lpPropsFolder[i].Value.bin.lpb)->abFlags[3] = ZARAFA_FAVORITE;
		}

		lpProps[cProps].ulPropTag = lpPropsFolder[i].ulPropTag;
		lpProps[cProps++].Value = lpPropsFolder[i].Value;
	}

	/////////////////////////////////////////////////////
	// Add the row in the list
	//
	sKeyProp.ulPropTag = PR_ROWID;
	sKeyProp.Value.ul = ulRowId;

	hr = this->HrModifyRow(ulUpdateType, &sKeyProp, lpProps, cProps);
	if (hr != hrSuccess)
		goto exit;

	// Add relation id
	if (ulUpdateType == ECKeyTable::TABLE_ROW_ADD) {
		sRelFolder.ulRowID = ulRowId;
		sRelFolder.cbEntryID = cbEntryID;

		hr = MAPIAllocateBuffer(sRelFolder.cbEntryID, (void**)&sRelFolder.lpEntryID);
		if (hr != hrSuccess)
			goto exit;

		memcpy(sRelFolder.lpEntryID, lpEntryID, sRelFolder.cbEntryID);

		hr = lpFolderReal->QueryInterface(IID_IMAPIFolder, (void **)&sRelFolder.lpFolder);
		if (hr != hrSuccess)
			goto exit;

		if (lpFolderAdviseSink) { // is NULL when the notification is disabled
			hr = lpFolderAdviseSink->QueryInterface(IID_IMAPIAdviseSink, (void **)&sRelFolder.lpAdviseSink);
			if (hr != hrSuccess)
				goto exit;

			sRelFolder.ulAdviseConnectionId = ulConnection;
		} else {
			sRelFolder.lpAdviseSink = NULL;
			sRelFolder.ulAdviseConnectionId = 0;
		}

		m_mapRelation.insert(ECMAPFolderRelation::value_type(strInstanceKey, sRelFolder));
		
		m_ulRowId++;
	}

exit:

	if (lpFolderReal)
		lpFolderReal->Release(); 

	if (lpPropsFolder) 
		MAPIFreeBuffer(lpPropsFolder);

	if (lpProps) 
		MAPIFreeBuffer(lpProps);

	if (lpFolderID) 
		MAPIFreeBuffer(lpFolderID);

	if (hr != hrSuccess && ulConnection > 0)
		m_lpECParentFolder->GetMsgStore()->Unadvise(ulConnection);

	if (lpFolderAdviseSink)
		lpFolderAdviseSink->Release();

	if (lpsRowsInternal)
		FreeProws(lpsRowsInternal);

	FREE_RESTRICTION(lpRestriction);

	return hr;
}

HRESULT ECMemTablePublic::DelRow(SBinary* lpInstanceKey)
{
	HRESULT hr = hrSuccess;
	std::string strInstanceKey;
	SPropValue sKeyProp;
	ECMAPFolderRelation::iterator	iterRel;

	if (lpInstanceKey == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	strInstanceKey.assign((char*)lpInstanceKey->lpb, lpInstanceKey->cb);

	iterRel = m_mapRelation.find(strInstanceKey);

	if (iterRel == m_mapRelation.end() )
		goto exit;


	sKeyProp.ulPropTag = PR_ROWID;
	sKeyProp.Value.ul = iterRel->second.ulRowID;
	
	this->HrModifyRow(ECKeyTable::TABLE_ROW_DELETE, NULL, &sKeyProp, 1); //ignore error
	
	if (iterRel->second.ulAdviseConnectionId > 0)
		m_lpECParentFolder->GetMsgStore()->Unadvise(iterRel->second.ulAdviseConnectionId);

	FreeRelation(&iterRel->second);

	m_mapRelation.erase(iterRel);
	
exit:
	return hr;
}

void ECMemTablePublic::FreeRelation(t_sRelation* lpRelation)
{
	if (lpRelation == NULL)
		return;

	if (lpRelation->lpFolder)
		lpRelation->lpFolder->Release();

	if (lpRelation->lpAdviseSink)
		lpRelation->lpAdviseSink->Release();

	if (lpRelation->lpEntryID)
		MAPIFreeBuffer(lpRelation->lpEntryID);

}
