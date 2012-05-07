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
#include "ZCABContainer.h"
#include "ZCMAPIProp.h"
#include "Trace.h"

#include <mapiutil.h>

#include "ECMemTable.h"
#include "ECGuid.h"
#include "ECDebug.h"
#include "CommonUtil.h"
#include "mapiext.h"
#include "mapiguidext.h"
#include "namedprops.h"
#include "charset/convert.h"
#include "mapi_ptr.h"
#include "ECGetText.h"
#include "EMSAbTag.h"
#include "ECRestriction.h"

#include <iostream>
#include "stringutil.h"
using namespace std;

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

ZCABContainer::ZCABContainer(std::vector<zcabFolderEntry> *lpFolders, IMAPIFolder *lpContacts, LPMAPISUP lpMAPISup, void* lpProvider, char *szClassName) : ECUnknown(szClassName)
{
	ASSERT(!(lpFolders && lpContacts));

	m_lpFolders = lpFolders;
	m_lpContactFolder = lpContacts;
	m_lpMAPISup = lpMAPISup;
	m_lpProvider = lpProvider;
	m_lpDistList = NULL;

	if (m_lpMAPISup)
		m_lpMAPISup->AddRef();
	if (m_lpContactFolder)
		m_lpContactFolder->AddRef();
}

ZCABContainer::~ZCABContainer()
{
	if (m_lpMAPISup)
		m_lpMAPISup->Release();
	if (m_lpContactFolder)
		m_lpContactFolder->Release();
	if (m_lpDistList)
		m_lpDistList->Release();
}

HRESULT	ZCABContainer::QueryInterface(REFIID refiid, void **lppInterface)
{
	if (m_lpDistList == NULL) {
		REGISTER_INTERFACE(IID_ZCABContainer, this);
	} else {
		REGISTER_INTERFACE(IID_ZCDistList, this);
	}
	REGISTER_INTERFACE(IID_ECUnknown, this);

	if (m_lpDistList == NULL) {
		REGISTER_INTERFACE(IID_IABContainer, &this->m_xABContainer);
	} else {
		REGISTER_INTERFACE(IID_IDistList, &this->m_xABContainer);
	}
	REGISTER_INTERFACE(IID_IMAPIProp, &this->m_xABContainer);
	REGISTER_INTERFACE(IID_IUnknown, &this->m_xABContainer);

	return MAPI_E_INTERFACE_NOT_SUPPORTED;
}

/** 
 * Create a ZCABContainer as either the top level (lpFolders is set) or
 * as a subfolder (lpContacts is set).
 * 
 * @param[in] lpFolders Only the top container has the list to the wanted Zarafa Contacts Folders, NULL otherwise.
 * @param[in] lpContacts Create this object as wrapper for the lpContacts folder, NULL if 
 * @param[in] lpMAPISup 
 * @param[in] lpProvider 
 * @param[out] lppABContainer The newly created ZCABContainer class
 * 
 * @return 
 */
HRESULT	ZCABContainer::Create(std::vector<zcabFolderEntry> *lpFolders, IMAPIFolder *lpContacts, LPMAPISUP lpMAPISup, void* lpProvider, ZCABContainer **lppABContainer)
{
	HRESULT			hr = hrSuccess;
	ZCABContainer*	lpABContainer = NULL;

	try {
		lpABContainer = new ZCABContainer(lpFolders, lpContacts, lpMAPISup, lpProvider, "IABContainer");

		hr = lpABContainer->QueryInterface(IID_ZCABContainer, (void **)lppABContainer);
	} catch (...) {
		hr = MAPI_E_NOT_ENOUGH_MEMORY;
	}

	return hr;
}

HRESULT	ZCABContainer::Create(IMessage *lpContact, ULONG cbEntryID, LPENTRYID lpEntryID, LPMAPISUP lpMAPISup, ZCABContainer **lppABContainer)
{
	HRESULT hr = hrSuccess;
	ZCABContainer*	lpABContainer = NULL;
	ZCMAPIProp* lpDistList = NULL;

	try {
		lpABContainer = new ZCABContainer(NULL, NULL, lpMAPISup, NULL, "IABContainer");
	} catch (...) {
		hr = MAPI_E_NOT_ENOUGH_MEMORY;
	}
	if (hr != hrSuccess)
		goto exit;

	hr = ZCMAPIProp::Create(lpContact, cbEntryID, lpEntryID, &lpDistList);
	if (hr != hrSuccess)
		goto exit;

	hr = lpDistList->QueryInterface(IID_IMAPIProp, (void **)&lpABContainer->m_lpDistList);
	if (hr != hrSuccess)
		goto exit;

	hr = lpABContainer->QueryInterface(IID_ZCDistList, (void **)lppABContainer);

exit:
	if (hr != hrSuccess)
		delete lpABContainer;

	if (lpDistList)
		lpDistList->Release();

	return hr;
}

/////////////////////////////////////////////////
// IMAPIContainer
//

HRESULT ZCABContainer::GetFolderContentsTable(ULONG ulFlags, LPMAPITABLE *lppTable)
{
	HRESULT hr = hrSuccess;
	MAPITablePtr ptrContents;
	mapi_rowset_ptr	ptrRows;
	ECMemTable*		lpTable = NULL;
	ECMemTableView*	lpTableView = NULL;
	ULONG i, j = 0;

#define I_NCOLS 7
	// data from the contact
	SizedSPropTagArray(I_NCOLS, inputCols) = {I_NCOLS, {PR_DISPLAY_NAME, PR_ADDRTYPE, PR_EMAIL_ADDRESS, PR_NORMALIZED_SUBJECT, PR_ENTRYID, PR_MESSAGE_CLASS, PR_ORIGINAL_DISPLAY_NAME}};
	// I_MV_INDEX is dispidABPEmailList from mnNamedProps
	enum {I_DISPLAY_NAME = 0, I_ADDRTYPE, I_EMAIL_ADDRESS, I_NORMALIZED_SUBJECT, I_ENTRYID, I_MESSAGE_CLASS, I_ORIGINAL_DISPLAY_NAME, I_MV_INDEX, I_NAMEDSTART};
	SPropTagArrayPtr ptrInputCols;

#define O_NCOLS 10
	// data for the table
	SizedSPropTagArray(O_NCOLS, outputCols) = {O_NCOLS, {PR_DISPLAY_NAME, PR_ADDRTYPE, PR_EMAIL_ADDRESS, PR_NORMALIZED_SUBJECT, PR_ENTRYID, PR_DISPLAY_TYPE, PR_OBJECT_TYPE, PR_ORIGINAL_DISPLAY_NAME, PR_INSTANCE_KEY, PR_ROWID}};
	enum {O_DISPLAY_NAME = 0, O_ADDRTYPE, O_EMAIL_ADDRESS, O_NORMALIZED_SUBJECT, O_ENTRYID, O_DISPLAY_TYPE, O_OBJECT_TYPE, O_ORIGINAL_DISPLAY_NAME, O_INSTANCE_KEY, O_ROWID};
	SPropTagArrayPtr ptrOutputCols;

	SPropTagArrayPtr ptrContactCols;
	SPropValue lpColData[O_NCOLS];

	// named properties
	SPropTagArrayPtr ptrNameTags;
	LPMAPINAMEID *lppNames = NULL;
	ULONG ulNames = (6 * 5) + 1;
	ULONG ulType = (ulFlags & MAPI_UNICODE) ? PT_UNICODE : PT_STRING8;
	MAPINAMEID mnNamedProps[(6 * 5) + 1] = {
		// index with MVI_FLAG
		{(LPGUID)&PSETID_Address, MNID_ID, {dispidABPEmailList}},

		// MVI offset 0: email1 set
		{(LPGUID)&PSETID_Address, MNID_ID, {dispidEmail1DisplayName}},
		{(LPGUID)&PSETID_Address, MNID_ID, {dispidEmail1AddressType}},
		{(LPGUID)&PSETID_Address, MNID_ID, {dispidEmail1Address}},
		{(LPGUID)&PSETID_Address, MNID_ID, {dispidEmail1OriginalDisplayName}},
		{(LPGUID)&PSETID_Address, MNID_ID, {dispidEmail1OriginalEntryID}},

		// MVI offset 1: email2 set
		{(LPGUID)&PSETID_Address, MNID_ID, {dispidEmail2DisplayName}},
		{(LPGUID)&PSETID_Address, MNID_ID, {dispidEmail2AddressType}},
		{(LPGUID)&PSETID_Address, MNID_ID, {dispidEmail2Address}},
		{(LPGUID)&PSETID_Address, MNID_ID, {dispidEmail2OriginalDisplayName}},
		{(LPGUID)&PSETID_Address, MNID_ID, {dispidEmail2OriginalEntryID}},

		// MVI offset 2: email3 set
		{(LPGUID)&PSETID_Address, MNID_ID, {dispidEmail3DisplayName}},
		{(LPGUID)&PSETID_Address, MNID_ID, {dispidEmail3AddressType}},
		{(LPGUID)&PSETID_Address, MNID_ID, {dispidEmail3Address}},
		{(LPGUID)&PSETID_Address, MNID_ID, {dispidEmail3OriginalDisplayName}},
		{(LPGUID)&PSETID_Address, MNID_ID, {dispidEmail3OriginalEntryID}},

		// MVI offset 3: business fax (fax2) set
		{(LPGUID)&PSETID_Address, MNID_ID, {dispidFax2DisplayName}},
		{(LPGUID)&PSETID_Address, MNID_ID, {dispidFax2AddressType}},
		{(LPGUID)&PSETID_Address, MNID_ID, {dispidFax2Address}},
		{(LPGUID)&PSETID_Address, MNID_ID, {dispidFax2OriginalDisplayName}},
		{(LPGUID)&PSETID_Address, MNID_ID, {dispidFax2OriginalEntryID}},

		// MVI offset 4: home fax (fax3) set
		{(LPGUID)&PSETID_Address, MNID_ID, {dispidFax3DisplayName}},
		{(LPGUID)&PSETID_Address, MNID_ID, {dispidFax3AddressType}},
		{(LPGUID)&PSETID_Address, MNID_ID, {dispidFax3Address}},
		{(LPGUID)&PSETID_Address, MNID_ID, {dispidFax3OriginalDisplayName}},
		{(LPGUID)&PSETID_Address, MNID_ID, {dispidFax3OriginalEntryID}},		

		// MVI offset 5: primary fax (fax1) set
		{(LPGUID)&PSETID_Address, MNID_ID, {dispidFax1DisplayName}},
		{(LPGUID)&PSETID_Address, MNID_ID, {dispidFax1AddressType}},
		{(LPGUID)&PSETID_Address, MNID_ID, {dispidFax1Address}},
		{(LPGUID)&PSETID_Address, MNID_ID, {dispidFax1OriginalDisplayName}},
		{(LPGUID)&PSETID_Address, MNID_ID, {dispidFax1OriginalEntryID}},
	};
	

	hr = Util::HrCopyUnicodePropTagArray(ulFlags, (LPSPropTagArray)&inputCols, &ptrInputCols);
	if (hr != hrSuccess)
		goto exit;
	hr = Util::HrCopyUnicodePropTagArray(ulFlags, (LPSPropTagArray)&outputCols, &ptrOutputCols);
	if (hr != hrSuccess)
		goto exit;

	hr = ECMemTable::Create(ptrOutputCols, PR_ROWID, &lpTable);
	if(hr != hrSuccess)
		goto exit;

	// root container has no contents, on hierarchy entries
	if (m_lpContactFolder == NULL)
		goto done;

	hr = m_lpContactFolder->GetContentsTable(ulFlags, &ptrContents);
	if (hr != hrSuccess)
		goto exit;

	hr = MAPIAllocateBuffer(sizeof(LPMAPINAMEID) * (ulNames), (void**)&lppNames);
	if (hr != hrSuccess)
		goto exit;

	for (i = 0; i < ulNames; i++)
		lppNames[i] = &mnNamedProps[i];

	hr = m_lpContactFolder->GetIDsFromNames(ulNames, lppNames, MAPI_CREATE, &ptrNameTags);
	if (FAILED(hr))
		goto exit;

	// fix types
	ptrNameTags->aulPropTag[0] = CHANGE_PROP_TYPE(ptrNameTags->aulPropTag[0], PT_MV_LONG | MV_INSTANCE);
	for (i = 0; i < (ulNames-1) / 5; i++) {
		ptrNameTags->aulPropTag[1+ (i*5) + 0] = CHANGE_PROP_TYPE(ptrNameTags->aulPropTag[1+ (i*5) + 0], ulType);
		ptrNameTags->aulPropTag[1+ (i*5) + 1] = CHANGE_PROP_TYPE(ptrNameTags->aulPropTag[1+ (i*5) + 1], ulType);
		ptrNameTags->aulPropTag[1+ (i*5) + 2] = CHANGE_PROP_TYPE(ptrNameTags->aulPropTag[1+ (i*5) + 2], ulType);
		ptrNameTags->aulPropTag[1+ (i*5) + 3] = CHANGE_PROP_TYPE(ptrNameTags->aulPropTag[1+ (i*5) + 3], ulType);
		ptrNameTags->aulPropTag[1+ (i*5) + 4] = CHANGE_PROP_TYPE(ptrNameTags->aulPropTag[1+ (i*5) + 4], PT_BINARY);
	}

	// add func HrCombinePropTagArrays(part1, part2, dest);
	hr = MAPIAllocateBuffer(CbNewSPropTagArray(ptrInputCols->cValues + ptrNameTags->cValues), &ptrContactCols);
	if (hr != hrSuccess)
		goto exit;
	j = 0;
	for (i = 0; i < ptrInputCols->cValues; i++)
		ptrContactCols->aulPropTag[j++] = ptrInputCols->aulPropTag[i];
	for (i = 0; i < ptrNameTags->cValues; i++)
		ptrContactCols->aulPropTag[j++] = ptrNameTags->aulPropTag[i];
	ptrContactCols->cValues = j;


	// set columns
	hr = ptrContents->SetColumns(ptrContactCols, 0);
	if (hr != hrSuccess)
		goto exit;

	while (true) {
		hr = ptrContents->QueryRows(256, ulFlags, &ptrRows);
		if (hr != hrSuccess)
			goto exit;

		if (ptrRows.empty())
			break;

		j = 0;
		for (i = 0; i < ptrRows.size(); i++) {
			ULONG ulOffset = 0;
			mapi_memory_ptr<cabEntryID> lpEntryID;
			ULONG cbEntryID = 0;

			if (ptrRows[i].lpProps[I_MV_INDEX].ulPropTag == (ptrNameTags->aulPropTag[0] & ~MVI_FLAG)) {
				// do not index outside named properties
				if (ptrRows[i].lpProps[I_MV_INDEX].Value.ul > 5)
					continue;
				ulOffset = ptrRows[i].lpProps[I_MV_INDEX].Value.ul * 5;
			}

			if (PROP_TYPE(ptrRows[i].lpProps[I_MESSAGE_CLASS].ulPropTag) == PT_ERROR)
				// no PR_MESSAGE_CLASS, unusable
				continue;

			if (
				((ulFlags & MAPI_UNICODE) && wcscasecmp(ptrRows[i].lpProps[I_MESSAGE_CLASS].Value.lpszW, L"IPM.Contact") == 0) ||
				((ulFlags & MAPI_UNICODE) == 0 && stricmp(ptrRows[i].lpProps[I_MESSAGE_CLASS].Value.lpszA, "IPM.Contact") == 0)
				)
			{
				lpColData[O_DISPLAY_TYPE].ulPropTag = PR_DISPLAY_TYPE;
				lpColData[O_DISPLAY_TYPE].Value.ul = DT_MAILUSER;

				lpColData[O_OBJECT_TYPE].ulPropTag = PR_OBJECT_TYPE;
				lpColData[O_OBJECT_TYPE].Value.ul = MAPI_MAILUSER;

				lpColData[O_ADDRTYPE].ulPropTag = CHANGE_PROP_TYPE(ptrOutputCols->aulPropTag[O_ADDRTYPE], PROP_TYPE(ptrRows[i].lpProps[I_NAMEDSTART + ulOffset + 1].ulPropTag));
				lpColData[O_ADDRTYPE].Value = ptrRows[i].lpProps[I_NAMEDSTART + ulOffset + 1].Value;
			} else if (
					   ((ulFlags & MAPI_UNICODE) && wcscasecmp(ptrRows[i].lpProps[I_MESSAGE_CLASS].Value.lpszW, L"IPM.DistList") == 0) ||
					   ((ulFlags & MAPI_UNICODE) == 0 && stricmp(ptrRows[i].lpProps[I_MESSAGE_CLASS].Value.lpszA, "IPM.DistList") == 0)
					   )
			{
				lpColData[O_DISPLAY_TYPE].ulPropTag = PR_DISPLAY_TYPE;
				lpColData[O_DISPLAY_TYPE].Value.ul = DT_PRIVATE_DISTLIST;

				lpColData[O_OBJECT_TYPE].ulPropTag = PR_OBJECT_TYPE;
				lpColData[O_OBJECT_TYPE].Value.ul = MAPI_DISTLIST;

				lpColData[O_ADDRTYPE].ulPropTag = PR_ADDRTYPE_W;
				lpColData[O_ADDRTYPE].Value.lpszW = L"MAPIPDL";
			} else {
				continue;
			}

			// make wrapped entryid...
			cbEntryID = CbNewCABENTRYID(ptrRows[i].lpProps[I_ENTRYID].Value.bin.cb);
			hr = MAPIAllocateBuffer(cbEntryID, (void**)&lpEntryID);
			if (hr != hrSuccess)
				goto exit;

			memset(lpEntryID, 0, cbEntryID);
			memcpy(&lpEntryID->muid, &MUIDZCSAB, sizeof(MAPIUID));
			lpEntryID->ulObjType = lpColData[O_OBJECT_TYPE].Value.ul;
			// devide by 5 since a block of properties on a contact is a set of 5 (see mnNamedProps above)
			lpEntryID->ulOffset = ulOffset/5;
			memcpy(lpEntryID->origEntryID, ptrRows[i].lpProps[I_ENTRYID].Value.bin.lpb, ptrRows[i].lpProps[I_ENTRYID].Value.bin.cb);

			lpColData[O_ENTRYID].ulPropTag = PR_ENTRYID;
			lpColData[O_ENTRYID].Value.bin.cb = cbEntryID;
			lpColData[O_ENTRYID].Value.bin.lpb = (LPBYTE)lpEntryID.get();

			ulOffset += I_NAMEDSTART;

			lpColData[O_DISPLAY_NAME].ulPropTag = CHANGE_PROP_TYPE(ptrOutputCols->aulPropTag[O_DISPLAY_NAME], PROP_TYPE(ptrRows[i].lpProps[ulOffset + 0].ulPropTag));
			lpColData[O_DISPLAY_NAME].Value = ptrRows[i].lpProps[ulOffset + 0].Value;

			lpColData[O_EMAIL_ADDRESS].ulPropTag = CHANGE_PROP_TYPE(ptrOutputCols->aulPropTag[O_EMAIL_ADDRESS], PROP_TYPE(ptrRows[i].lpProps[ulOffset + 2].ulPropTag));
			lpColData[O_EMAIL_ADDRESS].Value = ptrRows[i].lpProps[ulOffset + 2].Value;

			lpColData[O_NORMALIZED_SUBJECT].ulPropTag = CHANGE_PROP_TYPE(ptrOutputCols->aulPropTag[O_NORMALIZED_SUBJECT], PROP_TYPE(ptrRows[i].lpProps[ulOffset + 3].ulPropTag));
			lpColData[O_NORMALIZED_SUBJECT].Value = ptrRows[i].lpProps[ulOffset + 3].Value;

			lpColData[O_ORIGINAL_DISPLAY_NAME].ulPropTag = CHANGE_PROP_TYPE(ptrOutputCols->aulPropTag[O_ORIGINAL_DISPLAY_NAME], PROP_TYPE(ptrRows[i].lpProps[I_DISPLAY_NAME].ulPropTag));
			lpColData[O_ORIGINAL_DISPLAY_NAME].Value = ptrRows[i].lpProps[I_DISPLAY_NAME].Value;

			lpColData[O_INSTANCE_KEY].ulPropTag = PR_INSTANCE_KEY;
			lpColData[O_INSTANCE_KEY].Value.bin.cb = sizeof(ULONG);
			lpColData[O_INSTANCE_KEY].Value.bin.lpb = (LPBYTE)&i;

			lpColData[O_ROWID].ulPropTag = PR_ROWID;
			lpColData[O_ROWID].Value.ul = j++;
			hr = lpTable->HrModifyRow(ECKeyTable::TABLE_ROW_ADD, NULL, lpColData, O_NCOLS);
			if (hr != hrSuccess)
				goto exit;
		}
	}
		
done:
	AddChild(lpTable);

	hr = lpTable->HrGetView(createLocaleFromName(""), ulFlags, &lpTableView);
	if(hr != hrSuccess)
		goto exit;
	
	hr = lpTableView->QueryInterface(IID_IMAPITable, (void **)lppTable);

exit:
	if (lppNames)
		MAPIFreeBuffer(lppNames);

	if(lpTable)
		lpTable->Release();

	if(lpTableView)
		lpTableView->Release();

	return hr;
#undef TCOLS
}

HRESULT ZCABContainer::GetDistListContentsTable(ULONG ulFlags, LPMAPITABLE *lppTable)
{
	HRESULT hr = hrSuccess;
	SizedSPropTagArray(13, sptaCols) = {13, {PR_NULL /* reserve for PR_ROWID */,
											 PR_ADDRTYPE, PR_DISPLAY_NAME, PR_DISPLAY_TYPE, PR_EMAIL_ADDRESS, PR_ENTRYID,
											 PR_INSTANCE_KEY, PR_OBJECT_TYPE, PR_RECORD_KEY, PR_SEARCH_KEY, PR_SEND_INTERNET_ENCODING,
											 PR_SEND_RICH_INFO, PR_TRANSMITABLE_DISPLAY_NAME }};
	SPropTagArrayPtr ptrCols;
	ECMemTable* lpTable = NULL;
	ECMemTableView*	lpTableView = NULL;
	SPropValuePtr ptrEntries;
	MAPIPropPtr ptrUser;
	ULONG ulObjType;
	ULONG cValues;
	SPropArrayPtr ptrProps;
	SPropValue sKey;

	hr = Util::HrCopyUnicodePropTagArray(ulFlags, (LPSPropTagArray)&sptaCols, &ptrCols);
	if (hr != hrSuccess)
		goto exit;

	hr = ECMemTable::Create(ptrCols, PR_ROWID, &lpTable);
	if(hr != hrSuccess)
		goto exit;

	// getprops, open real contacts, make table
	hr = HrGetOneProp(m_lpDistList, 0x81051102, &ptrEntries); // Members "entryids" named property, see data layout below
	if (hr != hrSuccess)
		goto exit;


	sKey.ulPropTag = PR_ROWID;
	sKey.Value.ul = 0;
	for (ULONG i = 0; i < ptrEntries->Value.MVbin.cValues; i++) {
		ULONG ulOffset = 0;
		// Wrapped entryid's:
		// Flags: (ULONG) 0
		// Provider: (GUID) 0xC091ADD3519DCF11A4A900AA0047FAA4
		// Type: (BYTE) <value>, describes wrapped enrtyid
		//  lower 4 bits:
		//   0x00 = OneOff (use addressbook)
		//   0x03 = Contact (use folder / session?)
		//   0x04 = PDL  (use folder / session?)
		//   0x05 = GAB IMailUser (use addressbook)
		//   0x06 = GAB IDistList (use addressbook)
		//  next 3 bits: if lower is 0x03
		//   0x00 = business fax, or oneoff entryid
		//   0x10 = home fax
		//   0x20 = primary fax
		//   0x30 = no contact data
		//   0x40 = email 1
		//   0x50 = email 2
		//   0x60 = email 3
		//  top bit:
		//   0x80 default on, except for oneoff entryids

		// @todo, add provider guid test in entryid?

		// 0xC3 = 1100 0011 .. contact and email 1
		// 0xB5 = 1011 0101 .. GAB IMailUser, 1011 = 3, 3 not defined
		if (ptrEntries->Value.MVbin.lpbin[i].lpb[sizeof(ULONG)+sizeof(GUID)] & 0x80) {
			// handle wrapped entryids
			ulOffset = sizeof(ULONG) + sizeof(GUID) + sizeof(BYTE);
		}

		hr = m_lpMAPISup->OpenEntry(ptrEntries->Value.MVbin.lpbin[i].cb - ulOffset, (LPENTRYID)(ptrEntries->Value.MVbin.lpbin[i].lpb + ulOffset), NULL, 0, &ulObjType, &ptrUser);
		if (hr != hrSuccess)
			continue;

		hr = ptrUser->GetProps(ptrCols, 0, &cValues, &ptrProps);
		if (FAILED(hr))
			continue;

		ptrProps[0] = sKey;

		hr = lpTable->HrModifyRow(ECKeyTable::TABLE_ROW_ADD, NULL, ptrProps.get(), cValues);
		if (hr != hrSuccess)
			goto exit;

		sKey.Value.ul++;
	}
	hr = hrSuccess;

	AddChild(lpTable);

	hr = lpTable->HrGetView(createLocaleFromName(""), ulFlags, &lpTableView);
	if(hr != hrSuccess)
		goto exit;
	
	hr = lpTableView->QueryInterface(IID_IMAPITable, (void **)lppTable);

exit:
	if(lpTable)
		lpTable->Release();

	if(lpTableView)
		lpTableView->Release();

	return hr;
}


/** 
 * Returns an addressbook contents table of the IPM.Contacts folder in m_lpContactFolder.
 * 
 * @param[in] ulFlags MAPI_UNICODE for default unicode columns
 * @param[out] lppTable contents table of all items found in folder
 * 
 * @return 
 */
HRESULT ZCABContainer::GetContentsTable(ULONG ulFlags, LPMAPITABLE *lppTable)
{
	HRESULT hr = hrSuccess;

	if (m_lpDistList)
		hr = GetDistListContentsTable(ulFlags, lppTable);
	else
		hr = GetFolderContentsTable(ulFlags, lppTable);

	return hr;
}

/** 
 * Can return 3 kinds of tables:
 * 1. Root Container, contains one entry: the provider container
 * 2. Provider Container, contains user folders
 * 3. CONVENIENT_DEPTH: 1 + 2
 * 
 * @param[in] ulFlags MAPI_UNICODE | CONVENIENT_DEPTH
 * @param[out] lppTable root container table
 * 
 * @return MAPI Error code
 */
HRESULT ZCABContainer::GetHierarchyTable(ULONG ulFlags, LPMAPITABLE *lppTable)
{
	HRESULT			hr = hrSuccess;
	ECMemTable*		lpTable = NULL;
	ECMemTableView*	lpTableView = NULL;
#define TCOLS 9
	SizedSPropTagArray(TCOLS, sptaCols) = {TCOLS, {PR_ENTRYID, PR_STORE_ENTRYID, PR_DISPLAY_NAME_W, PR_OBJECT_TYPE, PR_CONTAINER_FLAGS, PR_DISPLAY_TYPE, PR_AB_PROVIDER_ID, PR_DEPTH, PR_INSTANCE_KEY}};
	enum {ENTRYID = 0, STORE_ENTRYID, DISPLAY_NAME, OBJECT_TYPE, CONTAINER_FLAGS, DISPLAY_TYPE, AB_PROVIDER_ID, DEPTH, INSTANCE_KEY, ROWID};
	std::vector<zcabFolderEntry>::iterator iter;
	ULONG ulInstance = 0;
	SPropValue sProps[TCOLS + 1];
	convert_context converter;

	if ((ulFlags & MAPI_UNICODE) == 0)
		sptaCols.aulPropTag[DISPLAY_NAME] = PR_DISPLAY_NAME_A;

	hr = ECMemTable::Create((LPSPropTagArray)&sptaCols, PR_ROWID, &lpTable);
	if(hr != hrSuccess)
		goto exit;

	/*
	  1. root container		: m_lpFolders = NULL, m_lpContactFolder = NULL, m_lpDistList = NULL, m_lpProvider = ZCABLogon (one entry: provider)
	  2. provider container	: m_lpFolders = data, m_lpContactFolder = NULL, m_lpDistList = NULL, m_lpProvider = ZCABLogon (N entries: folders)
	  3. contact folder		: m_lpFolders = NULL, m_lpContactFolder = data, m_lpDistList = NULL, m_lpProvider = ZCABContainer from (2), (no hierarchy entries)
	  4. distlist			: m_lpFolders = NULL, m_lpContactFolder = NULL, m_lpDistList = data, m_lpProvider = ZCABContainer from (3), (no hierarchy entries)

	  when ulFlags contains CONVENIENT_DEPTH, (1) also contains (2)
	  so we open (2) through the provider, which gives the m_lpFolders
	*/

	if (m_lpFolders) {
		// create hierarchy with folders from user stores
		for (iter = m_lpFolders->begin(); iter != m_lpFolders->end(); iter++, ulInstance++) {
			std::string strName;
			cabEntryID *lpEntryID = NULL;
			ULONG cbEntryID = CbNewCABENTRYID(iter->cbFolder);

			hr = MAPIAllocateBuffer(cbEntryID, (void**)&lpEntryID);
			if (hr != hrSuccess)
				goto exit;

			memset(lpEntryID, 0, cbEntryID);
			memcpy(&lpEntryID->muid, &MUIDZCSAB, sizeof(MAPIUID));
			lpEntryID->ulObjType = MAPI_ABCONT;
			lpEntryID->ulOffset = 0;
			memcpy(lpEntryID->origEntryID, iter->lpFolder, iter->cbFolder);

			sProps[ENTRYID].ulPropTag = sptaCols.aulPropTag[ENTRYID];
			sProps[ENTRYID].Value.bin.cb = cbEntryID;
			sProps[ENTRYID].Value.bin.lpb = (BYTE*)lpEntryID;

			sProps[STORE_ENTRYID].ulPropTag = CHANGE_PROP_TYPE(sptaCols.aulPropTag[STORE_ENTRYID], PT_ERROR);
			sProps[STORE_ENTRYID].Value.err = MAPI_E_NOT_FOUND;

			sProps[DISPLAY_NAME].ulPropTag = sptaCols.aulPropTag[DISPLAY_NAME];
			if ((ulFlags & MAPI_UNICODE) == 0) {
				sProps[DISPLAY_NAME].ulPropTag = PR_DISPLAY_NAME_A;
				strName = converter.convert_to<std::string>(iter->strwDisplayName);
				sProps[DISPLAY_NAME].Value.lpszA = (char*)strName.c_str();
			} else {
				sProps[DISPLAY_NAME].Value.lpszW = (WCHAR*)iter->strwDisplayName.c_str();
			}

			sProps[OBJECT_TYPE].ulPropTag = sptaCols.aulPropTag[OBJECT_TYPE];
			sProps[OBJECT_TYPE].Value.ul = MAPI_ABCONT;

			sProps[CONTAINER_FLAGS].ulPropTag = sptaCols.aulPropTag[CONTAINER_FLAGS];
			sProps[CONTAINER_FLAGS].Value.ul = AB_RECIPIENTS | AB_UNMODIFIABLE | AB_UNICODE_OK;

			sProps[DISPLAY_TYPE].ulPropTag = sptaCols.aulPropTag[DISPLAY_TYPE];
			sProps[DISPLAY_TYPE].Value.ul = DT_NOT_SPECIFIC;

			sProps[AB_PROVIDER_ID].ulPropTag = sptaCols.aulPropTag[AB_PROVIDER_ID];
			sProps[AB_PROVIDER_ID].Value.bin.cb = sizeof(GUID);
			sProps[AB_PROVIDER_ID].Value.bin.lpb = (BYTE*)&MUIDZCSAB;

			sProps[DEPTH].ulPropTag = PR_DEPTH;
			sProps[DEPTH].Value.ul = (ulFlags & CONVENIENT_DEPTH) ? 1 : 0;

			sProps[INSTANCE_KEY].ulPropTag = PR_INSTANCE_KEY;
			sProps[INSTANCE_KEY].Value.bin.cb = sizeof(ULONG);
			sProps[INSTANCE_KEY].Value.bin.lpb = (LPBYTE)&ulInstance;

			sProps[ROWID].ulPropTag = PR_ROWID;
			sProps[ROWID].Value.ul = ulInstance;

			hr = lpTable->HrModifyRow(ECKeyTable::TABLE_ROW_ADD, NULL, sProps, TCOLS + 1);

			MAPIFreeBuffer(lpEntryID);

			if (hr != hrSuccess)
				goto exit;
		}
	} else if (!m_lpContactFolder) {
		// only if not using a contacts folder, which should make the contents table. so this would return an empty hierarchy table, which is true.
		// create toplevel hierarchy. one entry: "Zarafa Contacts Folders"
		BYTE sEntryID[4 + sizeof(GUID)]; // minimum sized entryid. no extra info needed

		memset(sEntryID, 0, sizeof(sEntryID));
		memcpy(sEntryID + 4, &MUIDZCSAB, sizeof(GUID));

		sProps[ENTRYID].ulPropTag = sptaCols.aulPropTag[ENTRYID];
		sProps[ENTRYID].Value.bin.cb = sizeof(sEntryID);
		sProps[ENTRYID].Value.bin.lpb = sEntryID;

		sProps[STORE_ENTRYID].ulPropTag = CHANGE_PROP_TYPE(sptaCols.aulPropTag[STORE_ENTRYID], PT_ERROR);
		sProps[STORE_ENTRYID].Value.err = MAPI_E_NOT_FOUND;

		sProps[DISPLAY_NAME].ulPropTag = sptaCols.aulPropTag[DISPLAY_NAME];
		if ((ulFlags & MAPI_UNICODE) == 0) {
			sProps[DISPLAY_NAME].ulPropTag = PR_DISPLAY_NAME_A;
			sProps[DISPLAY_NAME].Value.lpszA = _A("Zarafa Contacts Folders");
		} else {
			sProps[DISPLAY_NAME].Value.lpszW = _W("Zarafa Contacts Folders");
		}

		sProps[OBJECT_TYPE].ulPropTag = sptaCols.aulPropTag[OBJECT_TYPE];
		sProps[OBJECT_TYPE].Value.ul = MAPI_ABCONT;

		// AB_SUBCONTAINERS flag causes this folder to be skipped in the IAddrBook::GetSearchPath()
		sProps[CONTAINER_FLAGS].ulPropTag = sptaCols.aulPropTag[CONTAINER_FLAGS];
		sProps[CONTAINER_FLAGS].Value.ul = AB_SUBCONTAINERS | AB_UNMODIFIABLE | AB_UNICODE_OK;

		sProps[DISPLAY_TYPE].ulPropTag = sptaCols.aulPropTag[DISPLAY_TYPE];
		sProps[DISPLAY_TYPE].Value.ul = DT_NOT_SPECIFIC;

		sProps[AB_PROVIDER_ID].ulPropTag = sptaCols.aulPropTag[AB_PROVIDER_ID];
		sProps[AB_PROVIDER_ID].Value.bin.cb = sizeof(GUID);
		sProps[AB_PROVIDER_ID].Value.bin.lpb = (BYTE*)&MUIDZCSAB;

		sProps[DEPTH].ulPropTag = PR_DEPTH;
		sProps[DEPTH].Value.ul = 0;

		sProps[INSTANCE_KEY].ulPropTag = PR_INSTANCE_KEY;
		sProps[INSTANCE_KEY].Value.bin.cb = sizeof(ULONG);
		sProps[INSTANCE_KEY].Value.bin.lpb = (LPBYTE)&ulInstance;

		sProps[ROWID].ulPropTag = PR_ROWID;
		sProps[ROWID].Value.ul = ulInstance++;

		hr = lpTable->HrModifyRow(ECKeyTable::TABLE_ROW_ADD, NULL, sProps, TCOLS + 1);
		if (hr != hrSuccess)
			goto exit;

		if (ulFlags & CONVENIENT_DEPTH) {
			ABContainerPtr ptrContainer;			
			ULONG ulObjType;
			MAPITablePtr ptrTable;
			mapi_rowset_ptr	ptrRows;

			hr = ((ZCABLogon*)m_lpProvider)->OpenEntry(sizeof(sEntryID), (LPENTRYID)sEntryID, NULL, 0, &ulObjType, &ptrContainer);
			if (hr != hrSuccess)
				goto exit;

			hr = ptrContainer->GetHierarchyTable(ulFlags, &ptrTable);
			if (hr != hrSuccess)
				goto exit;

			hr = ptrTable->QueryRows(-1, 0, &ptrRows);
			if (hr != hrSuccess)
				goto exit;

			for (mapi_rowset_ptr::size_type i = 0; i < ptrRows.size(); i++) {
				// use PR_STORE_ENTRYID field to set instance key, since that is always MAPI_E_NOT_FOUND (see above)
				LPSPropValue lpProp = PpropFindProp(ptrRows[i].lpProps, ptrRows[i].cValues, CHANGE_PROP_TYPE(PR_STORE_ENTRYID, PT_ERROR));
				lpProp->ulPropTag = PR_ROWID;
				lpProp->Value.ul = ulInstance++;

				hr = lpTable->HrModifyRow(ECKeyTable::TABLE_ROW_ADD, NULL, ptrRows[i].lpProps, ptrRows[i].cValues);
				if (hr != hrSuccess)
					goto exit;
			}
		}
	}

	AddChild(lpTable);

	hr = lpTable->HrGetView(createLocaleFromName(""), ulFlags, &lpTableView);
	if(hr != hrSuccess)
		goto exit;
		
	hr = lpTableView->QueryInterface(IID_IMAPITable, (void **)lppTable);

exit:
	if(lpTable)
		lpTable->Release();

	if(lpTableView)
		lpTableView->Release();

	return hr;
#undef TCOLS
}

/** 
 * Opens the contact from any given contact folder, and makes a ZCMAPIProp object for that contact.
 * 
 * @param[in] cbEntryID wrapped contact entryid bytes
 * @param[in] lpEntryID 
 * @param[in] lpInterface requested interface
 * @param[in] ulFlags unicode flags
 * @param[out] lpulObjType returned object type
 * @param[out] lppUnk returned object
 * 
 * @return MAPI Error code
 */
HRESULT ZCABContainer::OpenEntry(ULONG cbEntryID, LPENTRYID lpEntryID, LPCIID lpInterface, ULONG ulFlags, ULONG *lpulObjType, LPUNKNOWN *lppUnk)
{
	HRESULT hr = hrSuccess;
	cabEntryID *lpCABEntryID = (cabEntryID*)lpEntryID;
	ULONG cbNewCABEntryID = CbNewCABENTRYID(0);
	ULONG cbFolder = 0;
	LPENTRYID lpFolder = NULL;
	ULONG ulObjType = 0;
	MAPIFolderPtr ptrContactFolder;
	ZCABContainer *lpZCABContacts = NULL;
	MessagePtr ptrContact;
	ZCMAPIProp *lpZCMAPIProp = NULL;

	if (cbEntryID < cbNewCABEntryID || memcmp((LPBYTE)&lpCABEntryID->muid, &MUIDZCSAB, sizeof(MAPIUID)) != 0) {
		hr = MAPI_E_UNKNOWN_ENTRYID;
		goto exit;
	}

	if (m_lpDistList) {
		// there is nothing to open from the distlist point of view
		// @todo, passthrough to IMAPISupport object?
		hr = MAPI_E_NO_SUPPORT;
		goto exit;
	}

	cbFolder = cbEntryID - cbNewCABEntryID;
	lpFolder = (LPENTRYID)((LPBYTE)lpEntryID + cbNewCABEntryID);

	if (lpCABEntryID->ulObjType == MAPI_ABCONT) {
		hr = m_lpMAPISup->OpenEntry(cbFolder, lpFolder, NULL, 0, &ulObjType, &ptrContactFolder);
		if (hr != hrSuccess)
			goto exit;

		hr = ZCABContainer::Create(NULL, ptrContactFolder, m_lpMAPISup, m_lpProvider, &lpZCABContacts);
		if (hr != hrSuccess)
			goto exit;

		AddChild(lpZCABContacts);

		if (lpInterface)
			hr = lpZCABContacts->QueryInterface(*lpInterface, (void**)lppUnk);
		else
			hr = lpZCABContacts->QueryInterface(IID_IABContainer, (void**)lppUnk);
	} else if (lpCABEntryID->ulObjType == MAPI_DISTLIST) {
		// open the Original Message
		hr = m_lpMAPISup->OpenEntry(cbFolder, lpFolder, NULL, 0, &ulObjType, &ptrContact);
		if (hr != hrSuccess)
			goto exit;
		
		hr = ZCABContainer::Create(ptrContact, cbEntryID, lpEntryID, m_lpMAPISup, &lpZCABContacts);
		if (hr != hrSuccess)
			goto exit;

		AddChild(lpZCABContacts);

		if (lpInterface)
			hr = lpZCABContacts->QueryInterface(*lpInterface, (void**)lppUnk);
		else
			hr = lpZCABContacts->QueryInterface(IID_IDistList, (void**)lppUnk);
	} else if (lpCABEntryID->ulObjType == MAPI_MAILUSER) {
		// open the Original Message
		hr = m_lpMAPISup->OpenEntry(cbFolder, lpFolder, NULL, 0, &ulObjType, &ptrContact);
		if (hr != hrSuccess)
			goto exit;

		hr = ZCMAPIProp::Create(ptrContact, cbEntryID, lpEntryID, &lpZCMAPIProp);
		if (hr != hrSuccess)
			goto exit;

		AddChild(lpZCMAPIProp);

		if (lpInterface)
			hr = lpZCMAPIProp->QueryInterface(*lpInterface, (void**)lppUnk);
		else
			hr = lpZCMAPIProp->QueryInterface(IID_IMAPIProp, (void**)lppUnk);
	} else {
		hr = MAPI_E_UNKNOWN_ENTRYID;
		goto exit;
	}

	*lpulObjType = lpCABEntryID->ulObjType;

exit:
	if (lpZCMAPIProp)
		lpZCMAPIProp->Release();

	if (lpZCABContacts)
		lpZCABContacts->Release();

	return hr;
}

HRESULT ZCABContainer::SetSearchCriteria(LPSRestriction lpRestriction, LPENTRYLIST lpContainerList, ULONG ulSearchFlags)
{
	HRESULT hr = hrSuccess;

	hr = MAPI_E_NO_SUPPORT;

	return hr;
}

HRESULT ZCABContainer::GetSearchCriteria(ULONG ulFlags, LPSRestriction *lppRestriction, LPENTRYLIST *lppContainerList, ULONG *lpulSearchState)
{
	HRESULT hr = hrSuccess;

	hr = MAPI_E_NO_SUPPORT;

	return hr;
}

/////////////////////////////////////////////////
// IABContainer
//

HRESULT ZCABContainer::CreateEntry(ULONG cbEntryID, LPENTRYID lpEntryID, ULONG ulCreateFlags, LPMAPIPROP* lppMAPIPropEntry)
{
	HRESULT hr = hrSuccess;

	hr = MAPI_E_NO_SUPPORT;

	return hr;
}

HRESULT ZCABContainer::CopyEntries(LPENTRYLIST lpEntries, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, ULONG ulFlags)
{
	HRESULT hr = hrSuccess;

	hr = MAPI_E_NO_SUPPORT;

	return hr;
}

HRESULT ZCABContainer::DeleteEntries(LPENTRYLIST lpEntries, ULONG ulFlags)
{
	HRESULT hr = hrSuccess;

	hr = MAPI_E_NO_SUPPORT;

	return hr;
}

/** 
 * Resolve MAPI_UNRESOLVED items in lpAdrList and possebly add resolved
 * 
 * @param[in] lpPropTagArray properties to be included in lpAdrList
 * @param[in] ulFlags EMS_AB_ADDRESS_LOOKUP | MAPI_UNICODE
 * @param[in,out] lpAdrList 
 * @param[in,out] lpFlagList MAPI_AMBIGUOUS | MAPI_RESOLVED | MAPI_UNRESOLVED
 * 
 * @return 
 */
HRESULT ZCABContainer::ResolveNames(LPSPropTagArray lpPropTagArray, ULONG ulFlags, LPADRLIST lpAdrList, LPFlagList lpFlagList)
{
	HRESULT hr = hrSuccess;
	// only columns we can set from our contents table
	SizedSPropTagArray(7, sptaDefault) = {7, {PR_ADDRTYPE_A, PR_DISPLAY_NAME_A, PR_DISPLAY_TYPE, PR_EMAIL_ADDRESS_A, PR_ENTRYID, PR_INSTANCE_KEY, PR_OBJECT_TYPE}};
	SizedSPropTagArray(7, sptaUnicode) = {7, {PR_ADDRTYPE_W, PR_DISPLAY_NAME_W, PR_DISPLAY_TYPE, PR_EMAIL_ADDRESS_W, PR_ENTRYID, PR_INSTANCE_KEY, PR_OBJECT_TYPE}};
	ULONG i;
	mapi_rowset_ptr	ptrRows;

	if (lpPropTagArray == NULL) {
		if(ulFlags & MAPI_UNICODE)
			lpPropTagArray = (LPSPropTagArray)&sptaUnicode;
		else
			lpPropTagArray = (LPSPropTagArray)&sptaDefault;
	}

	// in this container table, find given PR_DISPLAY_NAME

	if (m_lpFolders) {
		// return MAPI_E_NO_SUPPORT ? since you should not query on this level

		// @todo search in all folders, loop over self, since we want this providers entry ids
		MAPITablePtr ptrHierarchy;

		if (m_lpFolders->empty())
			goto exit;
		
		hr = this->GetHierarchyTable(0, &ptrHierarchy);
		if (hr != hrSuccess)
			goto exit;

		hr = ptrHierarchy->QueryRows(m_lpFolders->size(), 0, &ptrRows);
		if (hr != hrSuccess)
			goto exit;

		for (i = 0; i < ptrRows.size(); i++) {
			ABContainerPtr ptrContainer;
			LPSPropValue lpEntryID = PpropFindProp(ptrRows[i].lpProps, ptrRows[i].cValues, PR_ENTRYID);
			ULONG ulObjType;

			if (!lpEntryID)
				continue;

			// this? provider?
			hr = this->OpenEntry(lpEntryID->Value.bin.cb, (LPENTRYID)lpEntryID->Value.bin.lpb, NULL, 0, &ulObjType, &ptrContainer);
			if (hr != hrSuccess)
				goto exit;

			hr = ptrContainer->ResolveNames(lpPropTagArray, ulFlags, lpAdrList, lpFlagList);
			if (hr != hrSuccess)
				goto exit;
		}
	} else if (m_lpContactFolder) {
		// only search within this folder and set entries in lpAdrList / lpFlagList
		MAPITablePtr ptrContents;
		std::set<ULONG> stProps;
		SPropTagArrayPtr ptrColumns;

		// make joint proptags
		std::copy(lpPropTagArray->aulPropTag, lpPropTagArray->aulPropTag + lpPropTagArray->cValues, std::inserter(stProps, stProps.begin()));
		for (i = 0; i < lpAdrList->aEntries[0].cValues; i++)
			stProps.insert(lpAdrList->aEntries[0].rgPropVals[i].ulPropTag);
		hr = MAPIAllocateBuffer(CbNewSPropTagArray(stProps.size()), &ptrColumns);
		if (hr != hrSuccess)
			goto exit;
		ptrColumns->cValues = stProps.size();
		std::copy(stProps.begin(), stProps.end(), ptrColumns->aulPropTag);


		hr = this->GetContentsTable(ulFlags & MAPI_UNICODE, &ptrContents);
		if (hr != hrSuccess)
			goto exit;

		hr = ptrContents->SetColumns(ptrColumns, 0);
		if (hr != hrSuccess)
			goto exit;

		for (i = 0; i < lpAdrList->cEntries; i++) {
			LPSPropValue lpDisplayNameA = PpropFindProp(lpAdrList->aEntries[i].rgPropVals, lpAdrList->aEntries[i].cValues, PR_DISPLAY_NAME_A);
			LPSPropValue lpDisplayNameW = PpropFindProp(lpAdrList->aEntries[i].rgPropVals, lpAdrList->aEntries[i].cValues, PR_DISPLAY_NAME_W);

			if (!lpDisplayNameA && !lpDisplayNameW)
				continue;

			ULONG ulResFlag = (ulFlags & EMS_AB_ADDRESS_LOOKUP) ? FL_FULLSTRING : FL_PREFIX | FL_IGNORECASE;
			ULONG ulStringType = lpDisplayNameW ? PT_UNICODE : PT_STRING8;
			SPropValue sProp = lpDisplayNameW ? *lpDisplayNameW : *lpDisplayNameA;

			ECOrRestriction resFind;
			ULONG ulSearchTags[] = {PR_DISPLAY_NAME, PR_EMAIL_ADDRESS, PR_ORIGINAL_DISPLAY_NAME};

			for (ULONG j = 0; j < arraySize(ulSearchTags); j++) {
				sProp.ulPropTag = CHANGE_PROP_TYPE(ulSearchTags[j], ulStringType);
				resFind.append( ECContentRestriction(ulResFlag, CHANGE_PROP_TYPE(ulSearchTags[j], ulStringType), &sProp, ECRestriction::Cheap) );
			}

			SRestrictionPtr ptrRestriction;
			hr = resFind.CreateMAPIRestriction(&ptrRestriction);
			if (hr != hrSuccess)
				goto exit;

			hr = ptrContents->Restrict(ptrRestriction, 0);
			if (hr != hrSuccess)
				goto exit;

			hr = ptrContents->QueryRows(-1, MAPI_UNICODE, &ptrRows);
			if (hr != hrSuccess)
				goto exit;

			if (ptrRows.size() == 1) {
				lpFlagList->ulFlag[i] = MAPI_RESOLVED;

				// release old row
				MAPIFreeBuffer(lpAdrList->aEntries[i].rgPropVals);
				lpAdrList->aEntries[i].rgPropVals = NULL;

				hr = Util::HrCopySRow((LPSRow)&lpAdrList->aEntries[i], &ptrRows[0], NULL);
				if (hr != hrSuccess)
					goto exit;

			} else if (ptrRows.size() > 1) {
				lpFlagList->ulFlag[i] = MAPI_AMBIGUOUS;
			}
		}
	} else {
		// not supported within MAPI_DISTLIST container
		hr = MAPI_E_NO_SUPPORT;
	}

exit:
	return hr;
}

// IMAPIProp for m_lpDistList
HRESULT ZCABContainer::GetProps(LPSPropTagArray lpPropTagArray, ULONG ulFlags, ULONG FAR * lpcValues, LPSPropValue FAR * lppPropArray)
{
	HRESULT hr = MAPI_E_NO_SUPPORT;
	if (m_lpDistList)
		hr = m_lpDistList->GetProps(lpPropTagArray, ulFlags, lpcValues, lppPropArray);
	return hr;
}

HRESULT ZCABContainer::GetPropList(ULONG ulFlags, LPSPropTagArray FAR * lppPropTagArray)
{
	HRESULT hr = MAPI_E_NO_SUPPORT;
	if (m_lpDistList)
		hr = m_lpDistList->GetPropList(ulFlags, lppPropTagArray);
	return hr;
}


//////////////////////////////////
// Interface IUnknown
//
HRESULT ZCABContainer::xABContainer::QueryInterface(REFIID refiid , void** lppInterface)
{
	TRACE_MAPI(TRACE_ENTRY, "IABContainer::QueryInterface", "%s", DBGGUIDToString(refiid).c_str());
	METHOD_PROLOGUE_(ZCABContainer, ABContainer);
	HRESULT hr = pThis->QueryInterface(refiid,lppInterface);
	TRACE_MAPI(TRACE_RETURN, "IABContainer::QueryInterface", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

ULONG ZCABContainer::xABContainer::AddRef()
{
	TRACE_MAPI(TRACE_ENTRY, "IABContainer::AddRef", "");
	METHOD_PROLOGUE_(ZCABContainer, ABContainer);
	return pThis->AddRef();
}

ULONG ZCABContainer::xABContainer::Release()
{
	TRACE_MAPI(TRACE_ENTRY, "IABContainer::Release", "");
	METHOD_PROLOGUE_(ZCABContainer, ABContainer);
	ULONG ulRef = pThis->Release();
	TRACE_MAPI(TRACE_RETURN, "IABContainer::Release", "%d", ulRef);
	return ulRef;
}

//////////////////////////////////
// Interface IABContainer
//
HRESULT ZCABContainer::xABContainer::CreateEntry(ULONG cbEntryID, LPENTRYID lpEntryID, ULONG ulCreateFlags, LPMAPIPROP* lppMAPIPropEntry)
{
	TRACE_MAPI(TRACE_ENTRY, "IABContainer::CreateEntry", "");
	METHOD_PROLOGUE_(ZCABContainer, ABContainer);
	HRESULT hr = pThis->CreateEntry(cbEntryID, lpEntryID, ulCreateFlags, lppMAPIPropEntry);
	TRACE_MAPI(TRACE_RETURN, "IABContainer::CreateEntry", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ZCABContainer::xABContainer::CopyEntries(LPENTRYLIST lpEntries, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, ULONG ulFlags)
{
	TRACE_MAPI(TRACE_ENTRY, "IABContainer::CopyEntries", "");
	METHOD_PROLOGUE_(ZCABContainer, ABContainer);
	HRESULT hr = pThis->CopyEntries(lpEntries, ulUIParam, lpProgress, ulFlags);
	TRACE_MAPI(TRACE_RETURN, "IABContainer::CopyEntries", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ZCABContainer::xABContainer::DeleteEntries(LPENTRYLIST lpEntries, ULONG ulFlags)
{
	TRACE_MAPI(TRACE_ENTRY, "IABContainer::DeleteEntries", "");
	METHOD_PROLOGUE_(ZCABContainer, ABContainer);
	HRESULT hr = pThis->DeleteEntries(lpEntries, ulFlags);
	TRACE_MAPI(TRACE_RETURN, "IABContainer::DeleteEntries", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ZCABContainer::xABContainer::ResolveNames(LPSPropTagArray lpPropTagArray, ULONG ulFlags, LPADRLIST lpAdrList, LPFlagList lpFlagList)
{
	TRACE_MAPI(TRACE_ENTRY, "IABContainer::ResolveNames", "\nlpPropTagArray:\t%s\nlpAdrList:\t%s", PropNameFromPropTagArray(lpPropTagArray).c_str(), AdrRowSetToString(lpAdrList, lpFlagList).c_str() );
	METHOD_PROLOGUE_(ZCABContainer, ABContainer);
	HRESULT hr = pThis->ResolveNames(lpPropTagArray, ulFlags, lpAdrList, lpFlagList);
	TRACE_MAPI(TRACE_RETURN, "IABContainer::ResolveNames", "%s, lpadrlist=\n%s", GetMAPIErrorDescription(hr).c_str(), AdrRowSetToString(lpAdrList, lpFlagList).c_str() );
	return hr;
}

//////////////////////////////////
// Interface IMAPIContainer
//
HRESULT ZCABContainer::xABContainer::GetContentsTable(ULONG ulFlags, LPMAPITABLE *lppTable)
{
	TRACE_MAPI(TRACE_ENTRY, "IABContainer::GetContentsTable", "");
	METHOD_PROLOGUE_(ZCABContainer, ABContainer);
	HRESULT hr = pThis->GetContentsTable(ulFlags, lppTable);
	TRACE_MAPI(TRACE_RETURN, "IABContainer::GetContentsTable", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ZCABContainer::xABContainer::GetHierarchyTable(ULONG ulFlags, LPMAPITABLE *lppTable)
{
	TRACE_MAPI(TRACE_ENTRY, "IABContainer::GetHierarchyTable", ""); 
	METHOD_PROLOGUE_(ZCABContainer, ABContainer);
	HRESULT hr = pThis->GetHierarchyTable(ulFlags, lppTable);
	TRACE_MAPI(TRACE_RETURN, "IABContainer::GetHierarchyTable", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ZCABContainer::xABContainer::OpenEntry(ULONG cbEntryID, LPENTRYID lpEntryID, LPCIID lpInterface, ULONG ulFlags, ULONG *lpulObjType, LPUNKNOWN *lppUnk)
{
	TRACE_MAPI(TRACE_ENTRY, "IABContainer::OpenEntry", "interface=%s", (lpInterface)?DBGGUIDToString(*lpInterface).c_str():"NULL");
	METHOD_PROLOGUE_(ZCABContainer, ABContainer);
	HRESULT hr = pThis->OpenEntry(cbEntryID, lpEntryID, lpInterface, ulFlags, lpulObjType, lppUnk);
	TRACE_MAPI(TRACE_RETURN, "IABContainer::OpenEntry", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ZCABContainer::xABContainer::SetSearchCriteria(LPSRestriction lpRestriction, LPENTRYLIST lpContainerList, ULONG ulSearchFlags)
{
	TRACE_MAPI(TRACE_ENTRY, "IABContainer::SetSearchCriteria", "");
	METHOD_PROLOGUE_(ZCABContainer, ABContainer);
	HRESULT hr = pThis->SetSearchCriteria(lpRestriction, lpContainerList, ulSearchFlags);
	TRACE_MAPI(TRACE_RETURN, "IABContainer::SetSearchCriteria", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ZCABContainer::xABContainer::GetSearchCriteria(ULONG ulFlags, LPSRestriction *lppRestriction, LPENTRYLIST *lppContainerList, ULONG *lpulSearchState)
{
	TRACE_MAPI(TRACE_ENTRY, "IABContainer::GetSearchCriteria", "");
	METHOD_PROLOGUE_(ZCABContainer, ABContainer);
	HRESULT hr = pThis->GetSearchCriteria(ulFlags, lppRestriction, lppContainerList, lpulSearchState);
	TRACE_MAPI(TRACE_RETURN, "IABContainer::GetSearchCriteria", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

////////////////////////////////
// Interface IMAPIProp
//
HRESULT ZCABContainer::xABContainer::GetLastError(HRESULT hError, ULONG ulFlags, LPMAPIERROR * lppMapiError)
{
	TRACE_MAPI(TRACE_ENTRY, "IABContainer::GetLastError", "");
	HRESULT hr = MAPI_E_NO_SUPPORT;
	TRACE_MAPI(TRACE_RETURN, "IABContainer::GetLastError", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ZCABContainer::xABContainer::SaveChanges(ULONG ulFlags)
{
	TRACE_MAPI(TRACE_ENTRY, "IABContainer::SaveChanges", "");
	HRESULT hr = MAPI_E_NO_SUPPORT;
	TRACE_MAPI(TRACE_RETURN, "IABContainer::SaveChanges", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ZCABContainer::xABContainer::GetProps(LPSPropTagArray lpPropTagArray, ULONG ulFlags, ULONG FAR * lpcValues, LPSPropValue FAR * lppPropArray)
{
	TRACE_MAPI(TRACE_ENTRY, "IABContainer::GetProps", "%s", PropNameFromPropTagArray(lpPropTagArray).c_str());
	METHOD_PROLOGUE_(ZCABContainer, ABContainer);
	HRESULT hr = pThis->GetProps(lpPropTagArray, ulFlags, lpcValues, lppPropArray);
	TRACE_MAPI(TRACE_RETURN, "IABContainer::GetProps", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ZCABContainer::xABContainer::GetPropList(ULONG ulFlags, LPSPropTagArray FAR * lppPropTagArray)
{
	TRACE_MAPI(TRACE_ENTRY, "IABContainer::GetPropList", "");
	METHOD_PROLOGUE_(ZCABContainer, ABContainer);
	HRESULT hr = pThis->GetPropList(ulFlags, lppPropTagArray);
	TRACE_MAPI(TRACE_RETURN, "IABContainer::GetPropList", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ZCABContainer::xABContainer::OpenProperty(ULONG ulPropTag, LPCIID lpiid, ULONG ulInterfaceOptions, ULONG ulFlags, LPUNKNOWN FAR * lppUnk)
{
	TRACE_MAPI(TRACE_ENTRY, "IABContainer::OpenProperty", "PropTag=%s, lpiid=%s", PropNameFromPropTag(ulPropTag).c_str(), DBGGUIDToString(*lpiid).c_str());
	HRESULT hr = MAPI_E_NO_SUPPORT;
	TRACE_MAPI(TRACE_RETURN, "IABContainer::OpenProperty", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ZCABContainer::xABContainer::SetProps(ULONG cValues, LPSPropValue lpPropArray, LPSPropProblemArray FAR * lppProblems)
{
	TRACE_MAPI(TRACE_ENTRY, "IABContainer::SetProps", "");
	HRESULT hr = MAPI_E_NO_SUPPORT;
	TRACE_MAPI(TRACE_RETURN, "IABContainer::SetProps", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ZCABContainer::xABContainer::DeleteProps(LPSPropTagArray lpPropTagArray, LPSPropProblemArray FAR * lppProblems)
{
	TRACE_MAPI(TRACE_ENTRY, "IABContainer::DeleteProps", "%s", PropNameFromPropTagArray(lpPropTagArray).c_str());
	HRESULT hr = MAPI_E_NO_SUPPORT;
	TRACE_MAPI(TRACE_RETURN, "IABContainer::DeleteProps", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ZCABContainer::xABContainer::CopyTo(ULONG ciidExclude, LPCIID rgiidExclude, LPSPropTagArray lpExcludeProps, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, LPCIID lpInterface, LPVOID lpDestObj, ULONG ulFlags, LPSPropProblemArray FAR * lppProblems)
{
	TRACE_MAPI(TRACE_ENTRY, "IABContainer::CopyTo", "");
	HRESULT hr = MAPI_E_NO_SUPPORT;
	TRACE_MAPI(TRACE_RETURN, "IABContainer::CopyTo", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ZCABContainer::xABContainer::CopyProps(LPSPropTagArray lpIncludeProps, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, LPCIID lpInterface, LPVOID lpDestObj, ULONG ulFlags, LPSPropProblemArray FAR * lppProblems)
{
	TRACE_MAPI(TRACE_ENTRY, "IABContainer::CopyProps", "");
	HRESULT hr = MAPI_E_NO_SUPPORT;
	TRACE_MAPI(TRACE_RETURN, "IABContainer::CopyProps", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ZCABContainer::xABContainer::GetNamesFromIDs(LPSPropTagArray * pptaga, LPGUID lpguid, ULONG ulFlags, ULONG * pcNames, LPMAPINAMEID ** pppNames)
{
	TRACE_MAPI(TRACE_ENTRY, "IABContainer::GetNamesFromIDs", "");
	HRESULT hr = MAPI_E_NO_SUPPORT;
	TRACE_MAPI(TRACE_RETURN, "IABContainer::GetIDsFromNames", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ZCABContainer::xABContainer::GetIDsFromNames(ULONG cNames, LPMAPINAMEID * ppNames, ULONG ulFlags, LPSPropTagArray * pptaga)
{
	TRACE_MAPI(TRACE_ENTRY, "IABContainer::GetIDsFromNames", "");
	HRESULT hr = MAPI_E_NO_SUPPORT;
	TRACE_MAPI(TRACE_RETURN, "IABContainer::GetIDsFromNames", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

