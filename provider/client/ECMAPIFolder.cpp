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

#include "Zarafa.h"
#include "ZarafaICS.h"
#include "ZarafaUtil.h"
#include "ECMessage.h"
#include "ECMAPIFolder.h"
#include "ECMAPITable.h"
#include "ECExchangeModifyTable.h"
#include "ECExchangeImportHierarchyChanges.h"
#include "ECExchangeImportContentsChanges.h"
#include "ECExchangeExportChanges.h"
#include "WSTransport.h"
#include "WSMessageStreamExporter.h"
#include "WSMessageStreamImporter.h"

#include "Mem.h"
#include "ECGuid.h"
#include "edkguid.h"
#include "Util.h"
#include "ClientUtil.h"

#include "ECDebug.h"

#include <edkmdb.h>
#include <mapiext.h>
#include <mapiutil.h>
#include <stdio.h>

#include "stringutil.h"

#include <charset/convstring.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

LONG __stdcall AdviseECFolderCallback(void *lpContext, ULONG cNotif, LPNOTIFICATION lpNotif)
{
	if (lpContext == NULL) {
		return S_OK;
	}

	ECMAPIFolder *lpFolder = (ECMAPIFolder*)lpContext;

	lpFolder->m_bReload = TRUE;

	return S_OK;
}

ECMAPIFolder::ECMAPIFolder(ECMsgStore *lpMsgStore, BOOL fModify, WSMAPIFolderOps *lpFolderOps, char *szClassName) : ECMAPIContainer(lpMsgStore, MAPI_FOLDER, fModify, szClassName)
{
	// Folder counters
	HrAddPropHandlers(PR_ASSOC_CONTENT_COUNT,		GetPropHandler,	DefaultSetPropComputed, (void *)this);
	HrAddPropHandlers(PR_CONTENT_COUNT,				GetPropHandler,	DefaultSetPropComputed, (void *)this);
	HrAddPropHandlers(PR_CONTENT_UNREAD,			GetPropHandler,	DefaultSetPropComputed,	(void *)this);
	HrAddPropHandlers(PR_SUBFOLDERS,				GetPropHandler,	DefaultSetPropComputed,	(void *)this);
	HrAddPropHandlers(PR_FOLDER_CHILD_COUNT,		GetPropHandler,	DefaultSetPropComputed,	(void *)this);
	HrAddPropHandlers(PR_DELETED_MSG_COUNT,			GetPropHandler,	DefaultSetPropComputed, (void *)this);
	HrAddPropHandlers(PR_DELETED_FOLDER_COUNT,		GetPropHandler,	DefaultSetPropComputed, (void *)this);
	HrAddPropHandlers(PR_DELETED_ASSOC_MSG_COUNT,	GetPropHandler,	DefaultSetPropComputed, (void *)this);

	HrAddPropHandlers(PR_CONTAINER_CONTENTS,		GetPropHandler,	DefaultSetPropIgnore, (void *)this, FALSE, FALSE);
	HrAddPropHandlers(PR_FOLDER_ASSOCIATED_CONTENTS,GetPropHandler,	DefaultSetPropIgnore, (void *)this, FALSE, FALSE);
	HrAddPropHandlers(PR_CONTAINER_HIERARCHY,		GetPropHandler,	DefaultSetPropIgnore, (void *)this, FALSE, FALSE);

	HrAddPropHandlers(PR_ACCESS,			GetPropHandler,			DefaultSetPropComputed, (void *)this);
	HrAddPropHandlers(PR_RIGHTS,			DefaultMAPIGetProp,		DefaultSetPropComputed, (void*) this);
	HrAddPropHandlers(PR_MESSAGE_SIZE,		GetPropHandler,			DefaultSetPropComputed,	(void*) this, FALSE, FALSE);
	
	HrAddPropHandlers(PR_FOLDER_TYPE,		DefaultMAPIGetProp,		DefaultSetPropComputed, (void*) this);

	HrAddPropHandlers(PR_ACL_DATA,			GetPropHandler,			SetPropHandler,			(void*)this);
	

	this->lpFolderOps = lpFolderOps;
	if (lpFolderOps)
		lpFolderOps->AddRef();

	this->isTransactedObject = FALSE;

	m_lpFolderAdviseSink = NULL;
	m_ulConnection = 0;
}

ECMAPIFolder::~ECMAPIFolder()
{
	if(lpFolderOps)
		lpFolderOps->Release();

	if (m_ulConnection > 0)
		GetMsgStore()->m_lpNotifyClient->UnRegisterAdvise(m_ulConnection);

	if (m_lpFolderAdviseSink)
		m_lpFolderAdviseSink->Release();

}

HRESULT ECMAPIFolder::Create(ECMsgStore *lpMsgStore, BOOL fModify, WSMAPIFolderOps *lpFolderOps, ECMAPIFolder **lppECMAPIFolder)
{
	HRESULT hr = hrSuccess;
	ECMAPIFolder *lpMAPIFolder = NULL;

	lpMAPIFolder = new ECMAPIFolder(lpMsgStore, fModify, lpFolderOps, "IMAPIFolder");

	hr = lpMAPIFolder->QueryInterface(IID_ECMAPIFolder, (void **)lppECMAPIFolder);

	if(hr != hrSuccess)
		delete lpMAPIFolder;

	return hr;
}

HRESULT ECMAPIFolder::GetPropHandler(ULONG ulPropTag, void* lpProvider, ULONG ulFlags, LPSPropValue lpsPropValue, void *lpParam, void *lpBase)
{
	HRESULT hr = hrSuccess;
	ECMAPIFolder *lpFolder = (ECMAPIFolder *)lpParam;

	switch(ulPropTag) {
	case PR_CONTENT_COUNT:
	case PR_CONTENT_UNREAD:
	case PR_DELETED_MSG_COUNT:
	case PR_DELETED_FOLDER_COUNT:
	case PR_DELETED_ASSOC_MSG_COUNT:
	case PR_ASSOC_CONTENT_COUNT:
	case PR_FOLDER_CHILD_COUNT:
		if(lpFolder->HrGetRealProp(ulPropTag, ulFlags, lpBase, lpsPropValue) != hrSuccess)
		{
			// Don't return an error here: outlook is relying on PR_CONTENT_COUNT, etc being available at all times. Especially the
			// exit routine (which checks to see how many items are left in the outbox) will crash if PR_CONTENT_COUNT is MAPI_E_NOT_FOUND
			lpsPropValue->ulPropTag = ulPropTag;
			lpsPropValue->Value.ul = 0;
		}
		break;
	case PR_SUBFOLDERS:
		if(lpFolder->HrGetRealProp(ulPropTag, ulFlags, lpBase, lpsPropValue) != hrSuccess)
		{
			lpsPropValue->ulPropTag = PR_SUBFOLDERS;
			lpsPropValue->Value.b = FALSE;
		}
		break;
	case PR_ACCESS:
		if(lpFolder->HrGetRealProp(PR_ACCESS, ulFlags, lpBase, lpsPropValue) != hrSuccess)
		{
			lpsPropValue->ulPropTag = PR_ACCESS;
			lpsPropValue->Value.l = 0; // FIXME: tijdelijk voor test
		}
		break;
	case PR_CONTAINER_CONTENTS:
	case PR_FOLDER_ASSOCIATED_CONTENTS:
	case PR_CONTAINER_HIERARCHY:
		lpsPropValue->ulPropTag = ulPropTag;
		lpsPropValue->Value.x = 1;
		break;
	case PR_ACL_DATA:
		hr = lpFolder->GetSerializedACLData(lpBase, lpsPropValue);
		if (hr == hrSuccess)
			lpsPropValue->ulPropTag = PR_ACL_DATA;
		else {
			lpsPropValue->ulPropTag = CHANGE_PROP_TYPE(PR_ACL_DATA, PT_ERROR);
			lpsPropValue->Value.err = hr;
		}
		break;
	default:
		hr = MAPI_E_NOT_FOUND;
		break;
	}

	return hr;
}

HRESULT	ECMAPIFolder::SetPropHandler(ULONG ulPropTag, void* lpProvider, LPSPropValue lpsPropValue, void *lpParam)
{
	HRESULT hr = hrSuccess;
	ECMAPIFolder *lpFolder = (ECMAPIFolder *)lpParam;

	switch(ulPropTag) {
	case PR_ACL_DATA:
		hr = lpFolder->SetSerializedACLData(lpsPropValue);
		break;
	default:
		hr = MAPI_E_NOT_FOUND;
		break;
	}

	return hr;
}

// This is similar to GetPropHandler, but works is a static function, and therefore cannot access
// an open ECMAPIFolder object. (The folder is most probably also not open, so ...
HRESULT ECMAPIFolder::TableRowGetProp(void* lpProvider, struct propVal *lpsPropValSrc, LPSPropValue lpsPropValDst, void **lpBase, ULONG ulType) {
	HRESULT hr = hrSuccess;

	switch(lpsPropValSrc->ulPropTag) {

	case PROP_TAG(PT_ERROR,PROP_ID(PR_DISPLAY_TYPE)):
		lpsPropValDst->Value.l = DT_FOLDER;
		lpsPropValDst->ulPropTag = PR_DISPLAY_TYPE;
		break;
	
	default:
		hr = MAPI_E_NOT_FOUND;
	}

	return hr;
}


HRESULT	ECMAPIFolder::QueryInterface(REFIID refiid, void **lppInterface) 
{
	REGISTER_INTERFACE(IID_ECMAPIFolder, this);
	REGISTER_INTERFACE(IID_ECMAPIContainer, this);
	REGISTER_INTERFACE(IID_ECMAPIProp, this);
	REGISTER_INTERFACE(IID_ECUnknown, this);

	REGISTER_INTERFACE(IID_IMAPIFolder, &this->m_xMAPIFolder);
	REGISTER_INTERFACE(IID_IMAPIContainer, &this->m_xMAPIFolder);
	REGISTER_INTERFACE(IID_IMAPIProp, &this->m_xMAPIFolder);
	REGISTER_INTERFACE(IID_IUnknown, &this->m_xMAPIFolder);

	REGISTER_INTERFACE(IID_IFolderSupport, &this->m_xFolderSupport);

	REGISTER_INTERFACE(IID_IECSecurity, &this->m_xECSecurity);

	REGISTER_INTERFACE(IID_ISelectUnicode, &this->m_xUnknown);

	return MAPI_E_INTERFACE_NOT_SUPPORTED;
}

HRESULT ECMAPIFolder::HrSetPropStorage(IECPropStorage *lpStorage, BOOL fLoadProps)
{
	HRESULT hr = hrSuccess;
	ULONG ulEventMask = fnevObjectModified  | fnevObjectDeleted | fnevObjectMoved | fnevObjectCreated;
	WSMAPIPropStorage *lpMAPIPropStorage = NULL;
	ULONG cbEntryId;
	LPENTRYID lpEntryId = NULL;

	hr = HrAllocAdviseSink(AdviseECFolderCallback, this, &m_lpFolderAdviseSink);	
	if (hr != hrSuccess)
		goto exit;

	hr = lpStorage->QueryInterface(IID_WSMAPIPropStorage, (void**)&lpMAPIPropStorage);
	if (hr != hrSuccess)
		goto exit;

	hr = lpMAPIPropStorage->GetEntryIDByRef(&cbEntryId, &lpEntryId);
	if (hr != hrSuccess)
		goto exit;

	hr = GetMsgStore()->InternalAdvise(cbEntryId, lpEntryId, ulEventMask, m_lpFolderAdviseSink, &m_ulConnection);
	if (hr == MAPI_E_NO_SUPPORT){
		hr = hrSuccess;			// there is no spoon
	} else if (hr != hrSuccess) {
		goto exit;
	} else {

		
		lpMAPIPropStorage->RegisterAdvise(ulEventMask, m_ulConnection);
	}
	
	hr = ECGenericProp::HrSetPropStorage(lpStorage, fLoadProps);

exit:
	if(lpMAPIPropStorage)
		lpMAPIPropStorage->Release();

	return hr;
}

HRESULT ECMAPIFolder::SetEntryId(ULONG cbEntryId, LPENTRYID lpEntryId)
{
	HRESULT hr = hrSuccess;
	
	hr = ECGenericProp::SetEntryId(cbEntryId, lpEntryId);
	if (hr != hrSuccess)
		goto exit;

exit:
	return hr;
}

HRESULT ECMAPIFolder::OpenProperty(ULONG ulPropTag, LPCIID lpiid, ULONG ulInterfaceOptions, ULONG ulFlags, LPUNKNOWN FAR * lppUnk)
{
	HRESULT hr = MAPI_E_INTERFACE_NOT_SUPPORTED;

	if (lpiid == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	if(ulPropTag == PR_CONTAINER_CONTENTS) {
		if (*lpiid == IID_IMAPITable)
			hr = GetContentsTable(ulInterfaceOptions, (LPMAPITABLE*)lppUnk);
	} else if(ulPropTag == PR_FOLDER_ASSOCIATED_CONTENTS) {
		if (*lpiid == IID_IMAPITable)
			hr = GetContentsTable( (ulInterfaceOptions|MAPI_ASSOCIATED), (LPMAPITABLE*)lppUnk);
	} else if(ulPropTag == PR_CONTAINER_HIERARCHY) {
		if(*lpiid == IID_IMAPITable)
			hr = GetHierarchyTable(ulInterfaceOptions, (LPMAPITABLE*)lppUnk);
	} else if(ulPropTag == PR_RULES_TABLE) {
		if(*lpiid == IID_IExchangeModifyTable)
			hr = ECExchangeModifyTable::CreateRulesTable(this, ulInterfaceOptions, (LPEXCHANGEMODIFYTABLE*)lppUnk);
	} else if(ulPropTag == PR_ACL_TABLE) {
		if(*lpiid == IID_IExchangeModifyTable)
			hr = ECExchangeModifyTable::CreateACLTable(this, ulInterfaceOptions, (LPEXCHANGEMODIFYTABLE*)lppUnk);
	} else if(ulPropTag == PR_COLLECTOR) {
		if(*lpiid == IID_IExchangeImportHierarchyChanges)
			hr = ECExchangeImportHierarchyChanges::Create(this, (LPEXCHANGEIMPORTHIERARCHYCHANGES*)lppUnk);
		else if(*lpiid == IID_IExchangeImportContentsChanges)
			hr = ECExchangeImportContentsChanges::Create(this, (LPEXCHANGEIMPORTCONTENTSCHANGES*)lppUnk);
	} else if(ulPropTag == PR_HIERARCHY_SYNCHRONIZER) {
		if(*lpiid == IID_IExchangeExportChanges)
			hr = ECExchangeExportChanges::Create(this, ICS_SYNC_HIERARCHY, (LPEXCHANGEEXPORTCHANGES*) lppUnk);
	} else if(ulPropTag == PR_CONTENTS_SYNCHRONIZER) {
		if(*lpiid == IID_IExchangeExportChanges)
			hr = ECExchangeExportChanges::Create(this, ICS_SYNC_CONTENTS, (LPEXCHANGEEXPORTCHANGES*) lppUnk);
	} else {
		hr = ECMAPIProp::OpenProperty(ulPropTag, lpiid, ulInterfaceOptions, ulFlags, lppUnk);
	}

exit:
	return hr;
}

HRESULT ECMAPIFolder::CopyTo(ULONG ciidExclude, LPCIID rgiidExclude, LPSPropTagArray lpExcludeProps, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, LPCIID lpInterface, LPVOID lpDestObj, ULONG ulFlags, LPSPropProblemArray FAR * lppProblems)
{
	HRESULT hr = hrSuccess;
	
	hr = Util::DoCopyTo(&IID_IMAPIFolder, &this->m_xMAPIFolder, ciidExclude, rgiidExclude, lpExcludeProps, ulUIParam, lpProgress, lpInterface, lpDestObj, ulFlags, lppProblems);

	return hr;
}

HRESULT ECMAPIFolder::CopyProps(LPSPropTagArray lpIncludeProps, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, LPCIID lpInterface, LPVOID lpDestObj, ULONG ulFlags, LPSPropProblemArray FAR * lppProblems)
{
	HRESULT hr = hrSuccess;

	hr = Util::DoCopyProps(&IID_IMAPIFolder, &this->m_xMAPIFolder, lpIncludeProps, ulUIParam, lpProgress, lpInterface, lpDestObj, ulFlags, lppProblems);

	return hr;
}

HRESULT ECMAPIFolder::SetProps(ULONG cValues, LPSPropValue lpPropArray, LPSPropProblemArray *lppProblems)
{
	HRESULT hr = hrSuccess;

	hr = ECMAPIContainer::SetProps(cValues, lpPropArray, lppProblems);
	if (hr != hrSuccess)
		goto exit;

	hr = ECMAPIContainer::SaveChanges(KEEP_OPEN_READWRITE);
	if (hr != hrSuccess)
		goto exit;

exit:

	return hr;
}

HRESULT ECMAPIFolder::DeleteProps(LPSPropTagArray lpPropTagArray, LPSPropProblemArray FAR * lppProblems)
{
	HRESULT hr = hrSuccess;

	hr = ECMAPIContainer::DeleteProps(lpPropTagArray, lppProblems);
	if (hr != hrSuccess)
		goto exit;

	hr = ECMAPIContainer::SaveChanges(KEEP_OPEN_READWRITE);
	if (hr != hrSuccess)
		goto exit;

exit:

	return hr;
}

HRESULT ECMAPIFolder::SaveChanges(ULONG ulFlags)
{
	return hrSuccess;
}

HRESULT ECMAPIFolder::SetSearchCriteria(LPSRestriction lpRestriction, LPENTRYLIST lpContainerList, ULONG ulSearchFlags)
{
	HRESULT hr = hrSuccess;
	
	if (lpFolderOps == NULL) {
		hr = MAPI_E_NO_SUPPORT;
		goto exit;
	}

	hr = lpFolderOps->HrSetSearchCriteria(lpContainerList, lpRestriction, ulSearchFlags);

exit:
	return hr;
}

HRESULT ECMAPIFolder::GetSearchCriteria(ULONG ulFlags, LPSRestriction *lppRestriction, LPENTRYLIST *lppContainerList, ULONG *lpulSearchState)
{
	HRESULT hr = hrSuccess;

	if (lpFolderOps == NULL) {
		hr = MAPI_E_NO_SUPPORT;
		goto exit;
	}

	// FIXME ulFlags ignore
	hr = lpFolderOps->HrGetSearchCriteria(lppContainerList, lppRestriction, lpulSearchState);

exit:
	return hr;
}

HRESULT ECMAPIFolder::CreateMessage(LPCIID lpInterface, ULONG ulFlags, LPMESSAGE *lppMessage)
{
    return CreateMessageWithEntryID(lpInterface, ulFlags, 0, NULL, lppMessage);
}

HRESULT ECMAPIFolder::CreateMessageWithEntryID(LPCIID lpInterface, ULONG ulFlags, ULONG cbEntryID, LPENTRYID lpEntryID, LPMESSAGE *lppMessage)
{
	HRESULT		hr = hrSuccess;
	ECMessage	*lpMessage = NULL;	
	LPMAPIUID	lpMapiUID = NULL;
	ULONG		cbNewEntryId = 0;
	LPENTRYID	lpNewEntryId = NULL;
	SPropValue	sPropValue[3];
	IECPropStorage*	lpStorage = NULL;

	if(!fModify) {
		hr = MAPI_E_NO_ACCESS;
		goto exit;
	}

	hr = ECMessage::Create(this->GetMsgStore(), TRUE, TRUE, ulFlags & MAPI_ASSOCIATED, FALSE, NULL, &lpMessage);
	if(hr != hrSuccess)
		goto exit;

    if(cbEntryID == 0 || lpEntryID == NULL) {
		// No entryid passed, create one
    	hr = HrCreateEntryId(GetMsgStore()->GetStoreGuid(), MAPI_MESSAGE, &cbNewEntryId, &lpNewEntryId);
    	if(hr != hrSuccess)
		    goto exit;

    	hr = lpMessage->SetEntryId(cbNewEntryId, lpNewEntryId);
    	if(hr != hrSuccess)
    		goto exit;

		hr = this->GetMsgStore()->lpTransport->HrOpenPropStorage(m_cbEntryId, m_lpEntryId, cbNewEntryId, lpNewEntryId, ulFlags & MAPI_ASSOCIATED, &lpStorage);
		if(hr != hrSuccess)
			goto exit;

	} else {
		// use the passed entryid
        hr = lpMessage->SetEntryId(cbEntryID, lpEntryID);
        if(hr != hrSuccess)
            goto exit;

		hr = this->GetMsgStore()->lpTransport->HrOpenPropStorage(m_cbEntryId, m_lpEntryId, cbEntryID, lpEntryID, ulFlags & MAPI_ASSOCIATED, &lpStorage);
		if(hr != hrSuccess)
			goto exit;
    }

	hr = lpMessage->HrSetPropStorage(lpStorage, FALSE);
	if(hr != hrSuccess)
		goto exit;


	// Load an empty property set
	hr = lpMessage->HrLoadEmptyProps();
	if(hr != hrSuccess)
		goto exit;

	//Set defaults
	// Same as ECAttach::OpenProperty
	ECAllocateBuffer(sizeof(MAPIUID), (void **) &lpMapiUID);

	hr = this->GetMsgStore()->lpSupport->NewUID(lpMapiUID);
	if(hr != hrSuccess)
		goto exit;

	sPropValue[0].ulPropTag = PR_MESSAGE_FLAGS;
	sPropValue[0].Value.l = MSGFLAG_UNSENT | MSGFLAG_READ;

	sPropValue[1].ulPropTag = PR_MESSAGE_CLASS_A;
	sPropValue[1].Value.lpszA = "IPM";
		
	sPropValue[2].ulPropTag = PR_SEARCH_KEY;
	sPropValue[2].Value.bin.cb = sizeof(MAPIUID);
	sPropValue[2].Value.bin.lpb = (LPBYTE)lpMapiUID;

	lpMessage->SetProps(3, sPropValue, NULL);

	// We don't actually create the object until savechanges is called, so remember in which
	// folder it was created
	hr = Util::HrCopyEntryId(this->m_cbEntryId, this->m_lpEntryId, &lpMessage->m_cbParentID, &lpMessage->m_lpParentID);
	if(hr != hrSuccess)
		goto exit;

	if(lpInterface)
		hr = lpMessage->QueryInterface(*lpInterface, (void **)lppMessage);
	else
		hr = lpMessage->QueryInterface(IID_IMessage, (void **)lppMessage);

	AddChild(lpMessage);

exit:
	if (lpStorage)
		lpStorage->Release();

	if (lpNewEntryId)
		ECFreeBuffer(lpNewEntryId);

	if(lpMapiUID)
		ECFreeBuffer(lpMapiUID);

	if(lpMessage)
		lpMessage->Release();

	return hr;
}

HRESULT ECMAPIFolder::CopyMessages(LPENTRYLIST lpMsgList, LPCIID lpInterface, LPVOID lpDestFolder, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, ULONG ulFlags)
{
	HRESULT hr = hrSuccess;
	HRESULT hrEC = hrSuccess;
	PEID peidDest = NULL;
	IMAPIFolder	*lpMapiFolder = NULL;
	ULONG cValues = 0;
	LPSPropTagArray lpsPropTagArray = NULL;
	LPSPropValue lpDestPropArray = NULL;

	LPENTRYLIST lpMsgListEC = NULL;
	LPENTRYLIST lpMsgListSupport = NULL;
	unsigned int i;
	GUID		guidFolder;
	GUID		guidMsg;

	if(lpMsgList == NULL || lpMsgList->cValues == 0)
		goto exit;

	if (lpMsgList->lpbin == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	// FIXME progress bar
	
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
	cValues = 1;
	hr = ECAllocateBuffer(CbNewSPropTagArray(cValues), (void **)&lpsPropTagArray);
	if(hr != hrSuccess)
		goto exit;

	lpsPropTagArray->cValues = cValues;
	lpsPropTagArray->aulPropTag[0] = PR_ENTRYID;
	
	hr = lpMapiFolder->GetProps(lpsPropTagArray, 0, &cValues, &lpDestPropArray);
	if(hr != hrSuccess)
		goto exit;

	if(cValues != 1 || lpDestPropArray[0].ulPropTag != PR_ENTRYID)	
	{
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	if(lpsPropTagArray){ ECFreeBuffer(lpsPropTagArray); lpsPropTagArray = NULL; }

	// Check if the destination entryid is a zarafa entryid and if there is a folder transport
	if( IsZarafaEntryId(lpDestPropArray[0].Value.bin.cb, lpDestPropArray[0].Value.bin.lpb) &&
		lpFolderOps != NULL) 
	{
		hr = HrGetStoreGuidFromEntryId(lpDestPropArray[0].Value.bin.cb, lpDestPropArray[0].Value.bin.lpb, &guidFolder);
		if(hr != hrSuccess)
			goto exit;

		// Allocate memory for support list and zarafa list
		hr = ECAllocateBuffer(sizeof(ENTRYLIST), (void**)&lpMsgListEC);
		if(hr != hrSuccess)
			goto exit;
		
		lpMsgListEC->cValues = 0;

		hr = ECAllocateMore(sizeof(SBinary) * lpMsgList->cValues, lpMsgListEC, (void**)&lpMsgListEC->lpbin);
		if(hr != hrSuccess)
			goto exit;
		
		hr = ECAllocateBuffer(sizeof(ENTRYLIST), (void**)&lpMsgListSupport);
		if(hr != hrSuccess)
			goto exit;
		
		lpMsgListSupport->cValues = 0;

		hr = ECAllocateMore(sizeof(SBinary) * lpMsgList->cValues, lpMsgListSupport, (void**)&lpMsgListSupport->lpbin);
		if(hr != hrSuccess)
			goto exit;
	

		//FIXME
		//hr = lpMapiFolder->SetReadFlags(GENERATE_RECEIPT_ONLY);
		if(hr != hrSuccess)
			goto exit;

		// Check if right store	
		for(i=0; i < lpMsgList->cValues; i++)
		{
			hr = HrGetStoreGuidFromEntryId(lpMsgList->lpbin[i].cb, lpMsgList->lpbin[i].lpb, &guidMsg);
			// check if the message in the store of the folder (serverside copy possible)
			if(hr == hrSuccess && IsZarafaEntryId(lpMsgList->lpbin[i].cb, lpMsgList->lpbin[i].lpb) && memcmp(&guidMsg, &guidFolder, sizeof(MAPIUID)) == 0)
				lpMsgListEC->lpbin[lpMsgListEC->cValues++] = lpMsgList->lpbin[i];// cheap copy
			else
				lpMsgListSupport->lpbin[lpMsgListSupport->cValues++] = lpMsgList->lpbin[i];// cheap copy

			hr = hrSuccess;
		}
		
		if(lpMsgListEC->cValues > 0)
		{
			hr = this->lpFolderOps->HrCopyMessage(lpMsgListEC, lpDestPropArray[0].Value.bin.cb, (LPENTRYID)lpDestPropArray[0].Value.bin.lpb, ulFlags, 0);
			if(FAILED(hr))
				goto exit;
			hrEC = hr;
		}

		if(lpMsgListSupport->cValues > 0)
		{
			hr = this->GetMsgStore()->lpSupport->CopyMessages(&IID_IMAPIFolder, &this->m_xMAPIFolder, lpMsgListSupport, lpInterface, lpDestFolder, ulUIParam, lpProgress, ulFlags);
			if(FAILED(hr))
				goto exit;
		}

	}else
	{
		// Do copy with the storeobject
		// Copy between two or more different stores
		hr = this->GetMsgStore()->lpSupport->CopyMessages(&IID_IMAPIFolder, &this->m_xMAPIFolder, lpMsgList, lpInterface, lpDestFolder, ulUIParam, lpProgress, ulFlags);
	}	

exit:
    if(lpDestPropArray)
        ECFreeBuffer(lpDestPropArray);
        
	if(lpMsgListEC)
		ECFreeBuffer(lpMsgListEC);

	if(lpMsgListSupport)
		ECFreeBuffer(lpMsgListSupport);

	if(lpsPropTagArray)
		ECFreeBuffer(lpsPropTagArray);

	if(peidDest)
		ECFreeBuffer(peidDest);

	if(lpMapiFolder)
		lpMapiFolder->Release();

	return (hr == hrSuccess)?hrEC:hr;
}

HRESULT ECMAPIFolder::DeleteMessages(LPENTRYLIST lpMsgList, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, ULONG ulFlags)
{
	HRESULT hr = hrSuccess;

	if (lpMsgList == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}
	
	if(ValidateZarafaEntryList(lpMsgList, MAPI_MESSAGE) == false) {
		hr = MAPI_E_INVALID_ENTRYID;
		goto exit;
	}

	// FIXME progress bar
	hr = this->GetMsgStore()->lpTransport->HrDeleteObjects(ulFlags, lpMsgList, 0);	

exit:
	return hr;
}

HRESULT ECMAPIFolder::CreateFolder(ULONG ulFolderType, LPTSTR lpszFolderName, LPTSTR lpszFolderComment, LPCIID lpInterface, ULONG ulFlags, LPMAPIFOLDER *lppFolder)
{
	HRESULT			hr = hrSuccess;
	ULONG			cbEntryId = 0;
	LPENTRYID		lpEntryId = NULL;
	IMAPIFolder*	lpFolder = NULL;
	ULONG			ulObjType = 0;

	// SC TODO: new code:
	// create new lpFolder object (load empty props ?)
	// create entryid and set it
	// create storage and set it
	// set props (comment)
	// save changes(keep open readwrite)  <- the only call to the server

	if (lpFolderOps == NULL) {
		hr = MAPI_E_NO_SUPPORT;
		goto exit;
	}

	// Create the actual folder on the server
	hr = lpFolderOps->HrCreateFolder(ulFolderType, convstring(lpszFolderName, ulFlags), convstring(lpszFolderComment, ulFlags), ulFlags & OPEN_IF_EXISTS, 0, NULL, 0, NULL, &cbEntryId, &lpEntryId);

	if(hr != hrSuccess)
		goto exit;

	// Open the folder we just created
	hr = this->GetMsgStore()->OpenEntry(cbEntryId, lpEntryId, lpInterface, MAPI_MODIFY | MAPI_DEFERRED_ERRORS, &ulObjType, (IUnknown **)&lpFolder);
	
	if(hr != hrSuccess)
		goto exit;

	*lppFolder = lpFolder;

exit:
	if(hr != hrSuccess && lpFolder)
		lpFolder->Release();

	if(lpEntryId)
		ECFreeBuffer(lpEntryId);

	return hr;
}

// @note if you change this function please look also at ECMAPIFolderPublic::CopyFolder
HRESULT ECMAPIFolder::CopyFolder(ULONG cbEntryID, LPENTRYID lpEntryID, LPCIID lpInterface, LPVOID lpDestFolder, LPTSTR lpszNewFolderName, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, ULONG ulFlags)
{
	HRESULT hr = hrSuccess;
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

HRESULT ECMAPIFolder::DeleteFolder(ULONG cbEntryID, LPENTRYID lpEntryID, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, ULONG ulFlags)
{
	HRESULT hr = hrSuccess;

	if(ValidateZarafaEntryId(cbEntryID, (LPBYTE)lpEntryID, MAPI_FOLDER) == false) {
		hr = MAPI_E_INVALID_ENTRYID;
		goto exit;
	}

	if (lpFolderOps == NULL) {
		hr = MAPI_E_NO_SUPPORT;
		goto exit;
	}

	hr = this->lpFolderOps->HrDeleteFolder(cbEntryID, lpEntryID, ulFlags, 0);

exit:
	return hr;
}

HRESULT ECMAPIFolder::SetReadFlags(LPENTRYLIST lpMsgList, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, ULONG ulFlags)
{
	HRESULT		hr = hrSuccess;
	LPMESSAGE	lpMessage = NULL;
	BOOL		bError = FALSE;
	ULONG		ulObjType = 0;

	// Progress bar
	ULONG ulPGMin = 0;
	ULONG ulPGMax = 0;
	ULONG ulPGDelta = 0;
	ULONG ulPGFlags = 0;
	
	if((ulFlags &~ (CLEAR_READ_FLAG | CLEAR_NRN_PENDING | CLEAR_RN_PENDING | GENERATE_RECEIPT_ONLY | MAPI_DEFERRED_ERRORS | MESSAGE_DIALOG | SUPPRESS_RECEIPT)) != 0 ||
		(ulFlags & (SUPPRESS_RECEIPT | CLEAR_READ_FLAG)) == (SUPPRESS_RECEIPT | CLEAR_READ_FLAG) ||
		(ulFlags & (SUPPRESS_RECEIPT | CLEAR_READ_FLAG | GENERATE_RECEIPT_ONLY)) == (SUPPRESS_RECEIPT | CLEAR_READ_FLAG | GENERATE_RECEIPT_ONLY) ||
		(ulFlags & (CLEAR_READ_FLAG | GENERATE_RECEIPT_ONLY)) == (CLEAR_READ_FLAG | GENERATE_RECEIPT_ONLY)	)
	{
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	if (lpFolderOps == NULL) {
		hr = MAPI_E_NO_SUPPORT;
		goto exit;
	}

	//FIXME: (GENERATE_RECEIPT_ONLY | SUPPRESS_RECEIPT) not yet implement ok on the server (update PR_READ_RECEIPT_REQUESTED to false)
	if( (!(ulFlags & (SUPPRESS_RECEIPT|CLEAR_READ_FLAG|CLEAR_NRN_PENDING|CLEAR_RN_PENDING)) || (ulFlags&GENERATE_RECEIPT_ONLY))&& lpMsgList){
		if((ulFlags&MESSAGE_DIALOG ) && lpProgress) {
			lpProgress->GetMin(&ulPGMin);
			lpProgress->GetMax(&ulPGMax);
			ulPGDelta = (ulPGMax-ulPGMin);
			lpProgress->GetFlags(&ulPGFlags);
		}

		for(ULONG i = 0; i < lpMsgList->cValues; i++)
		{
			if(OpenEntry(lpMsgList->lpbin[i].cb, (LPENTRYID)lpMsgList->lpbin[i].lpb, &IID_IMessage, MAPI_MODIFY, &ulObjType, (LPUNKNOWN*)&lpMessage) == hrSuccess)
			{
				if(lpMessage->SetReadFlag(ulFlags&~MESSAGE_DIALOG) != hrSuccess)
					bError = TRUE;

				lpMessage->Release(); lpMessage = NULL;
			}else
				bError = TRUE;
			
			// Progress bar
			if((ulFlags&MESSAGE_DIALOG ) && lpProgress) {
				if(ulPGFlags & MAPI_TOP_LEVEL) {
					hr = lpProgress->Progress((int)((float)i * ulPGDelta / lpMsgList->cValues + ulPGMin), i, lpMsgList->cValues);
				} else {
					hr = lpProgress->Progress((int)((float)i * ulPGDelta / lpMsgList->cValues + ulPGMin), 0, 0);
				}

				if(hr == MAPI_E_USER_CANCEL) {// MAPI_E_USER_CANCEL is user click on the Cancel button.
					hr = hrSuccess;
					bError = TRUE;
					goto exit;
				}else if(hr != hrSuccess) {
					goto exit;
				}
			}

		}
	}else {
		hr = lpFolderOps->HrSetReadFlags(lpMsgList, ulFlags, 0);
	}

exit:
	if(hr == hrSuccess && bError == TRUE)
		hr = MAPI_W_PARTIAL_COMPLETION;

	return hr;
}

HRESULT ECMAPIFolder::GetMessageStatus(ULONG cbEntryID, LPENTRYID lpEntryID, ULONG ulFlags, ULONG *lpulMessageStatus)
{
	HRESULT hr = hrSuccess;

	if(lpEntryID == NULL || IsZarafaEntryId(cbEntryID, (LPBYTE)lpEntryID) == false) {
		hr = MAPI_E_INVALID_ENTRYID;
		goto exit;
	}

	if(lpulMessageStatus == NULL) {
		hr = MAPI_E_INVALID_OBJECT;
		goto exit;
	}

	if (lpFolderOps == NULL) {
		hr = MAPI_E_NO_SUPPORT;
		goto exit;
	}
	
	hr = lpFolderOps->HrGetMessageStatus(cbEntryID, lpEntryID, ulFlags, lpulMessageStatus);
	if(hr != hrSuccess)
		goto exit;

exit:

	return hr;
}

HRESULT ECMAPIFolder::SetMessageStatus(ULONG cbEntryID, LPENTRYID lpEntryID, ULONG ulNewStatus, ULONG ulNewStatusMask, ULONG *lpulOldStatus)
{
	HRESULT hr = hrSuccess;

	if(lpEntryID == NULL || IsZarafaEntryId(cbEntryID, (LPBYTE)lpEntryID) == false) {
		hr = MAPI_E_INVALID_ENTRYID;
		goto exit;
	}

	if (lpFolderOps == NULL) {
		hr = MAPI_E_NO_SUPPORT;
		goto exit;
	}

	hr = lpFolderOps->HrSetMessageStatus(cbEntryID, lpEntryID, ulNewStatus, ulNewStatusMask, 0, lpulOldStatus);
	if(hr != hrSuccess)
		goto exit;

exit:
	return hr;
}

HRESULT ECMAPIFolder::SaveContentsSort(LPSSortOrderSet lpSortCriteria, ULONG ulFlags)
{
	HRESULT hr = hrSuccess;

	hr = MAPI_E_NO_ACCESS;

	return hr;
}

HRESULT ECMAPIFolder::EmptyFolder(ULONG ulUIParam, LPMAPIPROGRESS lpProgress, ULONG ulFlags)
{
	HRESULT hr = hrSuccess;
	
	if((ulFlags &~ (DEL_ASSOCIATED | FOLDER_DIALOG | DELETE_HARD_DELETE)) != 0)
	{
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	if (lpFolderOps == NULL) {
		hr = MAPI_E_NO_SUPPORT;
		goto exit;
	}

	hr = lpFolderOps->HrEmptyFolder(ulFlags, 0);

exit:

	return hr;
}

HRESULT ECMAPIFolder::GetProps(LPSPropTagArray lpPropTagArray, ULONG ulFlags, ULONG FAR * lpcValues, LPSPropValue FAR * lppPropArray)
{
	HRESULT hr = hrSuccess;
	
	// Check if there is a storage needed because favorites and ipmsubtree of the public folder 
	// doesn't have a prop storage.
	if(lpStorage != NULL) {
		hr = HrLoadProps();
		if (hr != hrSuccess)
			goto exit;
	}

	hr = ECMAPIProp::GetProps(lpPropTagArray, ulFlags, lpcValues, lppPropArray);
	if (hr != hrSuccess)
		goto exit;

exit:
	return hr;
}

HRESULT ECMAPIFolder::GetSupportMask(DWORD * pdwSupportMask)
{
	HRESULT hr = hrSuccess;

	if (pdwSupportMask == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	*pdwSupportMask = FS_SUPPORTS_SHARING; //Indicates that the folder supports sharing.
exit:
	return hr;
}

/**
 * Export a set of messages as stream.
 *
 * @param[in]	ulFlags		Flags used to determine which messages and what data is to be exported.
 * @param[in]	sChanges	The complete set of changes available.
 * @param[in]	ulStart		The index in sChanges that specifies the first message to export.
 * @param[in]	ulCount		The number of messages to export, starting at ulStart. This number will be decreased if less messages are available.
 * @param[in]	lpsProps	The set of proptags that will be returned as regular properties outside the stream.
 * @param[out]	lppsStreamExporter	The streamexporter that must be used to get the individual streams.
 *
 * @retval	MAPI_E_INVALID_PARAMETER	ulStart is larger than the number of changes available.
 * @retval	MAPI_E_UNABLE_TO_COMPLETE	ulCount is 0 after trunctation.
 */
HRESULT ECMAPIFolder::ExportMessageChangesAsStream(ULONG ulFlags, std::vector<ICSCHANGE> &sChanges, ULONG ulStart, ULONG ulCount, LPSPropTagArray lpsProps, WSMessageStreamExporter **lppsStreamExporter)
{
	HRESULT hr = hrSuccess;
	WSMessageStreamExporterPtr ptrStreamExporter;
	WSTransportPtr ptrTransport;

	if (ulStart > sChanges.size()) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	if (ulStart + ulCount > sChanges.size())
		ulCount = sChanges.size() - ulStart;

	if (ulCount == 0) {
		hr = MAPI_E_UNABLE_TO_COMPLETE;
		goto exit;
	}

	// Need to clone the transport since we want to be able to use our own transport for other things
	// while the streaming is going on; you should be able to intermix Synchronize() calls on the exporter
	// with other MAPI calls which would normally be impossible since the stream is kept open between
	// Synchronize() calls.
	hr = GetMsgStore()->lpTransport->CloneAndRelogon(&ptrTransport);
	if (hr != hrSuccess)
		goto exit;
	
	hr = ptrTransport->HrExportMessageChangesAsStream(ulFlags, &sChanges.front(), ulStart, ulCount, lpsProps, &ptrStreamExporter);
	if (hr != hrSuccess)
		goto exit;

	*lppsStreamExporter = ptrStreamExporter.release();

exit:
	return hr;
}

HRESULT ECMAPIFolder::CreateMessageFromStream(ULONG ulFlags, ULONG ulSyncId, ULONG cbEntryID, LPENTRYID lpEntryID, WSMessageStreamImporter **lppsStreamImporter)
{
	HRESULT hr = hrSuccess;
	WSMessageStreamImporterPtr	ptrStreamImporter;

	hr = GetMsgStore()->lpTransport->HrGetMessageStreamImporter(ulFlags, ulSyncId, cbEntryID, lpEntryID, m_cbEntryId, m_lpEntryId, true, false, &ptrStreamImporter);
	if (hr != hrSuccess)
		goto exit;

	*lppsStreamImporter = ptrStreamImporter.release();

exit:
	return hr;
}

HRESULT ECMAPIFolder::GetChangeInfo(ULONG cbEntryID, LPENTRYID lpEntryID, LPSPropValue *lppPropPCL, LPSPropValue *lppPropCK)
{
	return lpFolderOps->HrGetChangeInfo(cbEntryID, lpEntryID, lppPropPCL, lppPropCK);
}

HRESULT ECMAPIFolder::UpdateMessageFromStream(ULONG ulSyncId, ULONG cbEntryID, LPENTRYID lpEntryID, LPSPropValue lpConflictItems, WSMessageStreamImporter **lppsStreamImporter)
{
	HRESULT hr = hrSuccess;
	WSMessageStreamImporterPtr	ptrStreamImporter;

	hr = GetMsgStore()->lpTransport->HrGetMessageStreamImporter(0, ulSyncId, cbEntryID, lpEntryID, m_cbEntryId, m_lpEntryId, false, lpConflictItems, &ptrStreamImporter);
	if (hr != hrSuccess)
		goto exit;

	*lppsStreamImporter = ptrStreamImporter.release();

exit:
	return hr;
}

// -----------
HRESULT ECMAPIFolder::xMAPIFolder::QueryInterface(REFIID refiid , void** lppInterface)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPIFolder::QueryInterface", "%s", DBGGUIDToString(refiid).c_str());
	METHOD_PROLOGUE_(ECMAPIFolder, MAPIFolder);
	HRESULT hr = pThis->QueryInterface(refiid, lppInterface);
	TRACE_MAPI(TRACE_RETURN, "IMAPIFolder::QueryInterface", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

ULONG ECMAPIFolder::xMAPIFolder::AddRef()
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPIFolder::AddRef", "");
	METHOD_PROLOGUE_(ECMAPIFolder, MAPIFolder);
	return pThis->AddRef();
}

ULONG ECMAPIFolder::xMAPIFolder::Release()
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPIFolder::Release", "");
	METHOD_PROLOGUE_(ECMAPIFolder, MAPIFolder);
	ULONG ulRef = pThis->Release();
	TRACE_MAPI(TRACE_RETURN, "IMAPIFolder::Release", "%d", ulRef);
	return ulRef;
}

HRESULT ECMAPIFolder::xMAPIFolder::GetLastError(HRESULT hError, ULONG ulFlags, LPMAPIERROR * lppMapiError)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPIFolder::GetLastError", "");
	METHOD_PROLOGUE_(ECMAPIFolder, MAPIFolder);
	HRESULT hr = pThis->GetLastError(hError, ulFlags, lppMapiError);
	TRACE_MAPI(TRACE_RETURN, "IMAPIFolder::GetLastError", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMAPIFolder::xMAPIFolder::SaveChanges(ULONG ulFlags)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPIFolder::SaveChanges", "");
	METHOD_PROLOGUE_(ECMAPIFolder, MAPIFolder);
	HRESULT hr = pThis->SaveChanges(ulFlags);
	TRACE_MAPI(TRACE_RETURN, "IMAPIFolder::SaveChanges", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMAPIFolder::xMAPIFolder::GetProps(LPSPropTagArray lpPropTagArray, ULONG ulFlags, ULONG FAR * lpcValues, LPSPropValue FAR * lppPropArray)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPIFolder::GetProps", "PropTagArray=%s\nfFlags=0x%08X", PropNameFromPropTagArray(lpPropTagArray).c_str(), ulFlags);
	METHOD_PROLOGUE_(ECMAPIFolder, MAPIFolder);
	HRESULT hr = pThis->GetProps(lpPropTagArray, ulFlags, lpcValues, lppPropArray);
	TRACE_MAPI(TRACE_RETURN, "IMAPIFolder::GetProps", "%s\n%s", GetMAPIErrorDescription(hr).c_str(), PropNameFromPropArray(*lpcValues, *lppPropArray).c_str());
	return hr;
}

HRESULT ECMAPIFolder::xMAPIFolder::GetPropList(ULONG ulFlags, LPSPropTagArray FAR * lppPropTagArray)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPIFolder::GetPropList", "flags=0x%08X", ulFlags);
	METHOD_PROLOGUE_(ECMAPIFolder, MAPIFolder);
	HRESULT hr = pThis->GetPropList(ulFlags, lppPropTagArray);
	TRACE_MAPI(TRACE_RETURN, "IMAPIFolder::GetPropList", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMAPIFolder::xMAPIFolder::OpenProperty(ULONG ulPropTag, LPCIID lpiid, ULONG ulInterfaceOptions, ULONG ulFlags, LPUNKNOWN FAR * lppUnk)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPIFolder::OpenProperty", "PropTag=%s, lpiid=%s", PropNameFromPropTag(ulPropTag).c_str(), (lpiid)?DBGGUIDToString(*lpiid).c_str():"NULL");
	METHOD_PROLOGUE_(ECMAPIFolder, MAPIFolder);
	HRESULT hr = pThis->OpenProperty(ulPropTag, lpiid, ulInterfaceOptions, ulFlags, lppUnk);
	TRACE_MAPI(TRACE_RETURN, "IMAPIFolder::OpenProperty", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMAPIFolder::xMAPIFolder::SetProps(ULONG cValues, LPSPropValue lpPropArray, LPSPropProblemArray FAR * lppProblems)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPIFolder::SetProps", "%s", PropNameFromPropArray(cValues, lpPropArray).c_str());
	METHOD_PROLOGUE_(ECMAPIFolder, MAPIFolder);
	HRESULT hr = pThis->SetProps(cValues, lpPropArray, lppProblems);
	TRACE_MAPI(TRACE_RETURN, "IMAPIFolder::SetProps", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMAPIFolder::xMAPIFolder::DeleteProps(LPSPropTagArray lpPropTagArray, LPSPropProblemArray FAR * lppProblems)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPIFolder::DeleteProps", "%s", PropNameFromPropTagArray(lpPropTagArray).c_str());
	METHOD_PROLOGUE_(ECMAPIFolder, MAPIFolder);
	HRESULT hr = pThis->DeleteProps(lpPropTagArray, lppProblems);
	TRACE_MAPI(TRACE_RETURN, "IMAPIFolder::DeleteProps", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMAPIFolder::xMAPIFolder::CopyTo(ULONG ciidExclude, LPCIID rgiidExclude, LPSPropTagArray lpExcludeProps, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, LPCIID lpInterface, LPVOID lpDestObj, ULONG ulFlags, LPSPropProblemArray FAR * lppProblems)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPIFolder::CopyTo", "");
	METHOD_PROLOGUE_(ECMAPIFolder, MAPIFolder);
	HRESULT hr = pThis->CopyTo(ciidExclude, rgiidExclude, lpExcludeProps, ulUIParam, lpProgress, lpInterface, lpDestObj, ulFlags, lppProblems);;
	TRACE_MAPI(TRACE_RETURN, "IMAPIFolder::CopyTo", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMAPIFolder::xMAPIFolder::CopyProps(LPSPropTagArray lpIncludeProps, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, LPCIID lpInterface, LPVOID lpDestObj, ULONG ulFlags, LPSPropProblemArray FAR * lppProblems)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPIFolder::CopyProps", "");
	METHOD_PROLOGUE_(ECMAPIFolder, MAPIFolder);
	HRESULT hr = pThis->CopyProps(lpIncludeProps, ulUIParam, lpProgress, lpInterface, lpDestObj, ulFlags, lppProblems);
	TRACE_MAPI(TRACE_RETURN, "IMAPIFolder::CopyProps", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMAPIFolder::xMAPIFolder::GetNamesFromIDs(LPSPropTagArray * pptaga, LPGUID lpguid, ULONG ulFlags, ULONG * pcNames, LPMAPINAMEID ** pppNames)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPIFolder::GetNamesFromIDs", "");
	METHOD_PROLOGUE_(ECMAPIFolder, MAPIFolder);
	HRESULT hr = pThis->GetNamesFromIDs(pptaga, lpguid, ulFlags, pcNames, pppNames);
	TRACE_MAPI(TRACE_RETURN, "IMAPIFolder::GetIDsFromNames", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMAPIFolder::xMAPIFolder::GetIDsFromNames(ULONG cNames, LPMAPINAMEID * ppNames, ULONG ulFlags, LPSPropTagArray * pptaga)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPIFolder::GetIDsFromNames", "");
	METHOD_PROLOGUE_(ECMAPIFolder, MAPIFolder);
	HRESULT hr = pThis->GetIDsFromNames(cNames, ppNames, ulFlags, pptaga);
	TRACE_MAPI(TRACE_RETURN, "IMAPIFolder::GetIDsFromNames", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMAPIFolder::xMAPIFolder::GetContentsTable(ULONG ulFlags, LPMAPITABLE *lppTable)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPIFolder::GetContentsTable", "Flags=0x%08X", ulFlags);
	METHOD_PROLOGUE_(ECMAPIFolder, MAPIFolder);
	HRESULT hr = pThis->GetContentsTable(ulFlags, lppTable);
	TRACE_MAPI(TRACE_RETURN, "IMAPIFolder::GetContentsTable", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMAPIFolder::xMAPIFolder::GetHierarchyTable(ULONG ulFlags, LPMAPITABLE *lppTable)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPIFolder::GetHierarchyTable", ""); 
	METHOD_PROLOGUE_(ECMAPIFolder, MAPIFolder);
	HRESULT hr = pThis->GetHierarchyTable(ulFlags, lppTable);
	TRACE_MAPI(TRACE_RETURN, "IMAPIFolder::GetHierarchyTable", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMAPIFolder::xMAPIFolder::OpenEntry(ULONG cbEntryID, LPENTRYID lpEntryID, LPCIID lpInterface, ULONG ulFlags, ULONG *lpulObjType, LPUNKNOWN *lppUnk)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPIFolder::OpenEntry", "interface=%s",(lpInterface)?DBGGUIDToString(*lpInterface).c_str():"NULL");
	METHOD_PROLOGUE_(ECMAPIFolder, MAPIFolder);
	HRESULT hr = pThis->OpenEntry(cbEntryID, lpEntryID, lpInterface, ulFlags, lpulObjType, lppUnk);
	TRACE_MAPI(TRACE_RETURN, "IMAPIFolder::OpenEntry", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMAPIFolder::xMAPIFolder::SetSearchCriteria(LPSRestriction lpRestriction, LPENTRYLIST lpContainerList, ULONG ulSearchFlags)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPIFolder::SetSearchCriteria", "%s \nulSearchFlags=0x%08X", (lpRestriction)?RestrictionToString(lpRestriction).c_str():"NULL", ulSearchFlags);
	METHOD_PROLOGUE_(ECMAPIFolder, MAPIFolder);
	HRESULT hr = pThis->SetSearchCriteria(lpRestriction, lpContainerList, ulSearchFlags);
	TRACE_MAPI(TRACE_RETURN, "IMAPIFolder::SetSearchCriteria", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMAPIFolder::xMAPIFolder::GetSearchCriteria(ULONG ulFlags, LPSRestriction *lppRestriction, LPENTRYLIST *lppContainerList, ULONG *lpulSearchState)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPIFolder::GetSearchCriteria", "");
	METHOD_PROLOGUE_(ECMAPIFolder, MAPIFolder);
	HRESULT hr =pThis->GetSearchCriteria(ulFlags, lppRestriction, lppContainerList, lpulSearchState);;
	TRACE_MAPI(TRACE_RETURN, "IMAPIFolder::GetSearchCriteria", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}


HRESULT ECMAPIFolder::xMAPIFolder::CreateMessage(LPCIID lpInterface, ULONG ulFlags, LPMESSAGE *lppMessage)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPIFolder::CreateMessage", "flags=%d", ulFlags);
	METHOD_PROLOGUE_(ECMAPIFolder, MAPIFolder);
	HRESULT hr = pThis->CreateMessage(lpInterface, ulFlags, lppMessage);
	TRACE_MAPI(TRACE_RETURN, "IMAPIFolder::CreateMessage", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMAPIFolder::xMAPIFolder::CopyMessages(LPENTRYLIST lpMsgList, LPCIID lpInterface, LPVOID lpDestFolder, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, ULONG ulFlags)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPIFolder::CopyMessages", "flags=%d", ulFlags);
	METHOD_PROLOGUE_(ECMAPIFolder, MAPIFolder);
	HRESULT hr = pThis->CopyMessages(lpMsgList, lpInterface, lpDestFolder, ulUIParam, lpProgress, ulFlags);
	TRACE_MAPI(TRACE_RETURN, "IMAPIFolder::CopyMessages", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMAPIFolder::xMAPIFolder::DeleteMessages(LPENTRYLIST lpMsgList, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, ULONG ulFlags)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPIFolder::DeleteMessages", "flags=0x%08X", ulFlags);
	METHOD_PROLOGUE_(ECMAPIFolder, MAPIFolder);
	HRESULT hr = pThis->DeleteMessages(lpMsgList, ulUIParam, lpProgress, ulFlags);
	TRACE_MAPI(TRACE_RETURN, "IMAPIFolder::DeleteMessages", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMAPIFolder::xMAPIFolder::CreateFolder(ULONG ulFolderType, LPTSTR lpszFolderName, LPTSTR lpszFolderComment, LPCIID lpInterface, ULONG ulFlags, LPMAPIFOLDER *lppFolder)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPIFolder::CreateFolder", "type=%d name=%s flags=%d", ulFolderType, (lpszFolderName)?(char*)lpszFolderName:"", ulFlags);
	METHOD_PROLOGUE_(ECMAPIFolder, MAPIFolder);
	HRESULT hr = pThis->CreateFolder(ulFolderType, lpszFolderName, lpszFolderComment, lpInterface, ulFlags, lppFolder);
	TRACE_MAPI(TRACE_RETURN, "IMAPIFolder::CreateFolder", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMAPIFolder::xMAPIFolder::CopyFolder(ULONG cbEntryID, LPENTRYID lpEntryID, LPCIID lpInterface, LPVOID lpDestFolder, LPTSTR lpszNewFolderName, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, ULONG ulFlags)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPIFolder::CopyFolder", "NewFolderName=%s, interface=%s, flags=%d", (lpszNewFolderName)?(char*)lpszNewFolderName:"NULL", (lpInterface)?DBGGUIDToString(*lpInterface).c_str():"NULL", ulFlags);
	METHOD_PROLOGUE_(ECMAPIFolder, MAPIFolder);
	HRESULT hr = pThis->CopyFolder(cbEntryID, lpEntryID, lpInterface, lpDestFolder, lpszNewFolderName, ulUIParam, lpProgress, ulFlags);
	TRACE_MAPI(TRACE_RETURN, "IMAPIFolder::CopyFolder", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMAPIFolder::xMAPIFolder::DeleteFolder(ULONG cbEntryID, LPENTRYID lpEntryID, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, ULONG ulFlags)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPIFolder::DeleteFolder", "flags=%d", ulFlags);
	METHOD_PROLOGUE_(ECMAPIFolder, MAPIFolder);
	HRESULT hr = pThis->DeleteFolder(cbEntryID, lpEntryID, ulUIParam, lpProgress, ulFlags);
	TRACE_MAPI(TRACE_RETURN, "IMAPIFolder::DeleteFolder", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMAPIFolder::xMAPIFolder::SetReadFlags(LPENTRYLIST lpMsgList, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, ULONG ulFlags)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPIFolder::SetReadFlags", "lpMsgList=%s lpProgress=%s\n flags=0x%08X", EntryListToString(lpMsgList).c_str(), (lpProgress)?"Yes":"No", ulFlags);
	METHOD_PROLOGUE_(ECMAPIFolder, MAPIFolder);
	HRESULT hr = pThis->SetReadFlags(lpMsgList, ulUIParam, lpProgress, ulFlags);
	TRACE_MAPI(TRACE_RETURN, "IMAPIFolder::SetReadFlags", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMAPIFolder::xMAPIFolder::GetMessageStatus(ULONG cbEntryID, LPENTRYID lpEntryID, ULONG ulFlags, ULONG *lpulMessageStatus)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPIFolder::GetMessageStatus", "");
	METHOD_PROLOGUE_(ECMAPIFolder, MAPIFolder);
	HRESULT hr = pThis->GetMessageStatus(cbEntryID, lpEntryID, ulFlags, lpulMessageStatus);
	TRACE_MAPI(TRACE_RETURN, "IMAPIFolder::GetMessageStatus", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMAPIFolder::xMAPIFolder::SetMessageStatus(ULONG cbEntryID, LPENTRYID lpEntryID, ULONG ulNewStatus, ULONG ulNewStatusMask, ULONG *lpulOldStatus)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPIFolder::SetMessageStatus", "");
	METHOD_PROLOGUE_(ECMAPIFolder, MAPIFolder);
	HRESULT hr = pThis->SetMessageStatus(cbEntryID, lpEntryID, ulNewStatus, ulNewStatusMask, lpulOldStatus);
	TRACE_MAPI(TRACE_RETURN, "IMAPIFolder::SetMessageStatus", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMAPIFolder::xMAPIFolder::SaveContentsSort(LPSSortOrderSet lpSortCriteria, ULONG ulFlags)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPIFolder::SaveContentsSort", "");
	METHOD_PROLOGUE_(ECMAPIFolder, MAPIFolder);
	HRESULT hr = pThis->SaveContentsSort(lpSortCriteria, ulFlags);
	TRACE_MAPI(TRACE_RETURN, "IMAPIFolder::SaveContentsSort", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECMAPIFolder::xMAPIFolder::EmptyFolder(ULONG ulUIParam, LPMAPIPROGRESS lpProgress, ULONG ulFlags)
{
	TRACE_MAPI(TRACE_ENTRY, "IMAPIFolder::EmptyFolder", "flags=%d", ulFlags);
	METHOD_PROLOGUE_(ECMAPIFolder, MAPIFolder);
	HRESULT hr = pThis->EmptyFolder(ulUIParam, lpProgress, ulFlags);
	TRACE_MAPI(TRACE_RETURN, "IMAPIFolder::EmptyFolder", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

////////////////////////////
// IFolderSupport
//

HRESULT ECMAPIFolder::xFolderSupport::QueryInterface(REFIID refiid , void** lppInterface)
{
	TRACE_MAPI(TRACE_ENTRY, "IFolderSupport::QueryInterface", "%s", DBGGUIDToString(refiid).c_str());
	METHOD_PROLOGUE_(ECMAPIFolder, FolderSupport);
	HRESULT hr = pThis->QueryInterface(refiid, lppInterface);
	TRACE_MAPI(TRACE_RETURN, "IFolderSupport::QueryInterface", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

ULONG ECMAPIFolder::xFolderSupport::AddRef()
{
	TRACE_MAPI(TRACE_ENTRY, "IFolderSupport::AddRef", "");
	METHOD_PROLOGUE_(ECMAPIFolder, FolderSupport);
	return pThis->AddRef();
}

ULONG ECMAPIFolder::xFolderSupport::Release()
{
	TRACE_MAPI(TRACE_ENTRY, "IFolderSupport::Release", "");
	METHOD_PROLOGUE_(ECMAPIFolder, FolderSupport);
	ULONG ulRef = pThis->Release();
	TRACE_MAPI(TRACE_RETURN, "IFolderSupport::Release", "%d", ulRef);
	return ulRef;
}

HRESULT ECMAPIFolder::xFolderSupport::GetSupportMask(DWORD * pdwSupportMask)
{
	TRACE_MAPI(TRACE_ENTRY, "IFolderSupport::GetSupportMask", "");
	METHOD_PROLOGUE_(ECMAPIFolder, FolderSupport);
	HRESULT hr = pThis->GetSupportMask(pdwSupportMask);
	TRACE_MAPI(TRACE_RETURN, "IFolderSupport::GetSupportMask", "%s SupportMask=%s", GetMAPIErrorDescription(hr).c_str(), (pdwSupportMask)?stringify(*pdwSupportMask, true).c_str():"");
	return hr;
}
