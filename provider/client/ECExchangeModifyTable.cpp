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

#include "WSUtil.h"
#include "WSTransport.h"
#include "SOAPUtils.h"

#include <sstream>
#include <mapi.h>
#include <mapidefs.h>
#include <mapiutil.h>

#include "Util.h"
#include "ECExchangeModifyTable.h"
#include "mapicode.h"
#include "edkguid.h"
#include "ECGuid.h"
#include "mapiguid.h"

#include "Trace.h"
#include "ECDebug.h"

#include "ZarafaUtil.h"
#include "charset/convert.h"
#include "utf8.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

static LPWSTR WTF1252_to_WCHAR(LPCSTR szWTF1252, LPVOID lpBase, convert_context *lpConverter)
{
	HRESULT hr = hrSuccess;
	LPWSTR lpszResult = NULL;

	if (!szWTF1252)
		return NULL;

	std::string str1252;
	str1252.reserve(strlen(szWTF1252));

	while (*szWTF1252) {
		utf8::uint32_t cp = utf8::unchecked::next(szWTF1252);

		// Since the string was originally windows-1252, all code points
		// should be in the range 0 <= cp < 256.
		str1252.append(1, cp < 256 ? cp : '?');
	}

	// Now convert the windows-1252 string to proper UTF8.
	std::wstring strConverted;
	if (lpConverter)
		strConverted = lpConverter->convert_to<std::wstring>(str1252, rawsize(str1252), "WINDOWS-1252");
	else
		strConverted = convert_to<std::wstring>(str1252, rawsize(str1252), "WINDOWS-1252");

	if (lpBase)
		hr = MAPIAllocateMore((strConverted.size() + 1) * sizeof *lpszResult, lpBase, (LPVOID*)&lpszResult);
	else
		hr = MAPIAllocateBuffer((strConverted.size() + 1) * sizeof *lpszResult, (LPVOID*)&lpszResult);
	if (hr == hrSuccess)
		wcscpy(lpszResult, strConverted.c_str());

	return lpszResult;
}


ECExchangeModifyTable::ECExchangeModifyTable(ULONG ulUniqueTag, ECMemTable *table, ECMAPIProp *lpParent, ULONG ulStartUniqueId, ULONG ulFlags) {
	m_ecTable = table;
	m_ecTable->AddRef();
	m_ulUniqueId = ulStartUniqueId;
	m_ulUniqueTag = ulUniqueTag;
	m_ulFlags = ulFlags;
	m_lpParent = lpParent;

	m_lpParent->AddRef();
}

ECExchangeModifyTable::~ECExchangeModifyTable() {
	if (m_ecTable)
		m_ecTable->Release();

	if(m_lpParent)
		m_lpParent->Release();
}


HRESULT __stdcall ECExchangeModifyTable::CreateACLTable(ECMAPIProp *lpParent, ULONG ulFlags, LPEXCHANGEMODIFYTABLE *lppObj) {
	HRESULT hr = hrSuccess;
	ECExchangeModifyTable *obj = NULL;
	ECMemTable *lpecTable = NULL;
	ULONG ulUniqueId = 1;
	SizedSPropTagArray(4, sPropACLs) = {4, 
										 { PR_MEMBER_ID, PR_MEMBER_ENTRYID, 
										 PR_MEMBER_RIGHTS, PR_MEMBER_NAME } };

	// Although PR_RULE_ID is PT_I8, it does not matter, since the low count comes first in memory
	// This will break on a big-endian system though
	hr = ECMemTable::Create((LPSPropTagArray)&sPropACLs, PR_MEMBER_ID, &lpecTable);
	if (hr!=hrSuccess)
		goto exit;

	hr = OpenACLS(lpParent, ulFlags, lpecTable, &ulUniqueId);
	if(hr != hrSuccess)
		goto exit;

	hr = lpecTable->HrSetClean();
	if(hr != hrSuccess)
		goto exit;

	obj = new ECExchangeModifyTable(PR_MEMBER_ID, lpecTable, lpParent, ulUniqueId, ulFlags);

	hr = obj->QueryInterface(IID_IExchangeModifyTable, (void **)lppObj);

exit:
	if (lpecTable)
		lpecTable->Release();

	return hr;
}

HRESULT __stdcall ECExchangeModifyTable::CreateRulesTable(ECMAPIProp *lpParent, ULONG ulFlags, LPEXCHANGEMODIFYTABLE *lppObj) {
	HRESULT hr = hrSuccess;
	char *szXML = NULL;
	ECExchangeModifyTable *obj = NULL;
	IStream *lpRulesData = NULL;
	STATSTG statRulesData;
	ULONG ulRead;
	ECMemTable *ecTable = NULL;
	ULONG ulRuleId = 1;
	SizedSPropTagArray(7, sPropRules) = {7, 
										 { PR_RULE_ID, PR_RULE_SEQUENCE, PR_RULE_STATE, PR_RULE_CONDITION,
										 PR_RULE_ACTIONS, PR_RULE_USER_FLAGS, PR_RULE_PROVIDER } };

	// Although PR_RULE_ID is PT_I8, it does not matter, since the low count comes first in memory
	// This will break on a big-endian system though
	hr = ECMemTable::Create((LPSPropTagArray)&sPropRules, PR_RULE_ID, &ecTable);
	if (hr!=hrSuccess)
		goto exit;

	if(lpParent) {
		// PR_RULES_DATA can grow quite large. GetProps() only supports until size 8192, larger is not returned
		if(lpParent->OpenProperty(PR_RULES_DATA, &IID_IStream, 0, 0, (LPUNKNOWN *)&lpRulesData) == hrSuccess) {
			lpRulesData->Stat(&statRulesData, 0);
			szXML = new char [statRulesData.cbSize.LowPart+1];
			// TODO: Loop to read all data?
			hr = lpRulesData->Read(szXML, statRulesData.cbSize.LowPart, &ulRead);
			if (hr != hrSuccess || ulRead == 0)
				goto empty;
			szXML[statRulesData.cbSize.LowPart] = 0;
			hr = HrDeserializeTable(szXML, ecTable, &ulRuleId);
			// if the data was corrupted, or imported from exchange, it's incompatible, so return an empty table
			if (hr != hrSuccess) {
				ecTable->HrClear(); // just to be sure
				goto empty;
			}
		}
	}
	
empty:
	hr = ecTable->HrSetClean();
	if(hr != hrSuccess)
		goto exit;

	obj = new ECExchangeModifyTable(PR_RULE_ID, ecTable, lpParent, ulRuleId, ulFlags);

	hr = obj->QueryInterface(IID_IExchangeModifyTable, (void **)lppObj);

exit:
	if (ecTable)
		ecTable->Release();

	if (szXML)
		delete [] szXML;

	if (lpRulesData)
		lpRulesData->Release();

	return hr;
}

HRESULT ECExchangeModifyTable::QueryInterface(REFIID refiid, void **lppInterface) {
	REGISTER_INTERFACE(IID_ECExchangeModifyTable, this);
	REGISTER_INTERFACE(IID_ECUnknown, this);

	REGISTER_INTERFACE(IID_IExchangeModifyTable, &this->m_xExchangeModifyTable);
	REGISTER_INTERFACE(IID_IUnknown, &this->m_xExchangeModifyTable);

	return MAPI_E_INTERFACE_NOT_SUPPORTED;
}

HRESULT __stdcall ECExchangeModifyTable::GetLastError(HRESULT hResult, ULONG ulFlags, LPMAPIERROR *lppMAPIError) {
	return MAPI_E_NO_SUPPORT;
}

HRESULT __stdcall ECExchangeModifyTable::GetTable(ULONG ulFlags, LPMAPITABLE *lppTable) {
	ECMemTableView *lpView = NULL;
	HRESULT hr;

	hr = m_ecTable->HrGetView(createLocaleFromName(""), m_ulFlags, &lpView);
	if(hr != hrSuccess)
		goto exit;

	hr = lpView->QueryInterface(IID_IMAPITable, (void **)lppTable);

	lpView->Release();

exit:
	return hr;
}

HRESULT __stdcall ECExchangeModifyTable::ModifyTable(ULONG ulFlags, LPROWLIST lpMods) {
	HRESULT			hr = hrSuccess;
	SPropValue		sRowId;
	LPSPropValue	lpProps = NULL;
	LPSPropValue	lpFind = NULL;
	LPSPropValue	lpPropRemove = NULL;
	ULONG			cValues = 0;
	char *			szXML = NULL;
	SPropValue		sPropXML;
	ULONG			ulFlagsRow = 0;

	unsigned int i = 0;

	if(ulFlags == ROWLIST_REPLACE) {
		hr = m_ecTable->HrDeleteAll();
		if(hr != hrSuccess)
			goto exit;
	}

	for(i=0;i<lpMods->cEntries;i++) {
		switch(lpMods->aEntries[i].ulRowFlags) {
			case ROW_ADD:
			case ROW_MODIFY:
				// Note: the ECKeyTable only uses an ULONG as the key.
				//       Information placed in the HighPart of this PT_I8 is lost!

				lpFind = PpropFindProp(lpMods->aEntries[i].rgPropVals, lpMods->aEntries[i].cValues, m_ulUniqueTag);
				if (lpFind == NULL) {
					sRowId.ulPropTag = m_ulUniqueTag;
					sRowId.Value.li.QuadPart = this->m_ulUniqueId++;

					hr = Util::HrAddToPropertyArray(lpMods->aEntries[i].rgPropVals, lpMods->aEntries[i].cValues, &sRowId, &lpPropRemove, &cValues);
					if(hr != hrSuccess)
						goto exit;

					lpProps = lpPropRemove;
				} else {
					lpProps = lpMods->aEntries[i].rgPropVals;
					cValues = lpMods->aEntries[i].cValues;
				}

				if (lpMods->aEntries[i].ulRowFlags == ROW_ADD)
					ulFlagsRow = ECKeyTable::TABLE_ROW_ADD;
				else
					ulFlagsRow = ECKeyTable::TABLE_ROW_MODIFY;

				hr = m_ecTable->HrModifyRow(ulFlagsRow, lpFind, lpProps, cValues);
				if(hr != hrSuccess)
					goto exit;


				if (lpPropRemove) {
					MAPIFreeBuffer(lpPropRemove);
					lpPropRemove = NULL;
				}

				break;
			case ROW_REMOVE:
				hr = m_ecTable->HrModifyRow(ECKeyTable::TABLE_ROW_DELETE, NULL, lpMods->aEntries[i].rgPropVals, lpMods->aEntries[i].cValues);
				if(hr != hrSuccess)
					goto exit;
				break;
			case ROW_EMPTY:
				break;
		}
	}

	// The data has changed now, so save the data in the parent folder
	if(m_ulUniqueTag == PR_RULE_ID)
	{
		hr = HrSerializeTable(m_ecTable, &szXML);
		if(hr != hrSuccess)
			goto exit;

		sPropXML.ulPropTag = PR_RULES_DATA;
		sPropXML.Value.bin.lpb = (BYTE *)szXML;
		sPropXML.Value.bin.cb = strlen(szXML);

		hr = m_lpParent->SetProps(1, &sPropXML, NULL);
		if(hr != hrSuccess)
			goto exit;
	} else if (m_ulUniqueTag == PR_MEMBER_ID) {
		
		hr = SaveACLS(m_lpParent, m_ecTable);
		if(hr != hrSuccess)
			goto exit;

		// FIXME: if username not exist, just resolve

	} else {
		ASSERT(FALSE);
		hr = MAPI_E_CALL_FAILED;
	}

	// Mark all as saved
	hr = m_ecTable->HrSetClean();
	if(hr != hrSuccess)
		goto exit;

exit:
	if(szXML)
		delete [] szXML;

	if(lpPropRemove)
		MAPIFreeBuffer(lpPropRemove);

	return hr;
}


HRESULT ECExchangeModifyTable::OpenACLS(ECMAPIProp *lpecMapiProp, ULONG ulFlags, ECMemTable *lpTable, ULONG *lpulUniqueID)
{
	HRESULT hr = hrSuccess;
	IECSecurity *lpSecurity = NULL;
	ULONG cPerms = 0;
	LPECPERMISSION lpECPerms = NULL;
	SPropValue	lpsPropMember[4];
	LPECUSER lpECUser = NULL;
	LPECGROUP lpECGroup = NULL;
	WCHAR* lpMemberName = NULL;
	unsigned int ulUserid = 0;

	if(lpecMapiProp == NULL || lpTable == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}
	
	hr = lpecMapiProp->QueryInterface(IID_IECSecurity, (void**)&lpSecurity);
	if (hr != hrSuccess)
		goto exit;

	hr = lpSecurity->GetPermissionRules(ACCESS_TYPE_GRANT, &cPerms, &lpECPerms);
	if (hr != hrSuccess)
		goto exit;

	// Default exchange PR_MEMBER_ID ids
	//  0 = default acl
	// -1 = Anonymous acl
	for (ULONG i=0; i<cPerms; i++) {
		if (lpECPerms[i].ulType == ACCESS_TYPE_GRANT)
		{
			
			if(lpecMapiProp->GetMsgStore()->lpTransport->HrGetUser(lpECPerms[i].sUserId.cb, (LPENTRYID)lpECPerms[i].sUserId.lpb, MAPI_UNICODE, &lpECUser) != hrSuccess)
			{
				if(lpecMapiProp->GetMsgStore()->lpTransport->HrGetGroup(lpECPerms[i].sUserId.cb, (LPENTRYID)lpECPerms[i].sUserId.lpb, MAPI_UNICODE, &lpECGroup) != hrSuccess)
					continue;
			}

			if (lpECGroup) {
				lpMemberName = (LPTSTR)((lpECGroup->lpszFullname)?lpECGroup->lpszFullname:lpECGroup->lpszGroupname);
			} else {
				lpMemberName = (LPTSTR)((lpECUser->lpszFullName)?lpECUser->lpszFullName:lpECUser->lpszUsername);
			}

			lpsPropMember[0].ulPropTag = PR_MEMBER_ID;

			if (ABEntryIDToID(lpECPerms[i].sUserId.cb, (LPBYTE)lpECPerms[i].sUserId.lpb, &ulUserid, NULL, NULL) == erSuccess && ulUserid == 1)
				lpsPropMember[0].Value.li.QuadPart= 0; //everyone / exchange default
			else
				lpsPropMember[0].Value.li.QuadPart= (*lpulUniqueID)++;

			lpsPropMember[1].ulPropTag = PR_MEMBER_RIGHTS;
			lpsPropMember[1].Value.ul = lpECPerms[i].ulRights;

			lpsPropMember[2].ulPropTag = PR_MEMBER_NAME;
			lpsPropMember[2].Value.lpszW = (WCHAR*)lpMemberName;

			lpsPropMember[3].ulPropTag = PR_MEMBER_ENTRYID;
			lpsPropMember[3].Value.bin.cb = lpECPerms[i].sUserId.cb;
			lpsPropMember[3].Value.bin.lpb= (LPBYTE)lpECPerms[i].sUserId.lpb;

			hr = lpTable->HrModifyRow(ECKeyTable::TABLE_ROW_ADD, &lpsPropMember[0], lpsPropMember, 4);
			if(hr != hrSuccess)
				goto exit;

			if (lpECUser) { MAPIFreeBuffer(lpECUser); lpECUser = NULL;}
			if (lpECGroup) { MAPIFreeBuffer(lpECGroup); lpECGroup = NULL;}
		}
	}

exit:
	if (lpSecurity)
		lpSecurity->Release();

	if (lpECUser)
		MAPIFreeBuffer(lpECUser);

	if (lpECGroup) 
		MAPIFreeBuffer(lpECGroup);

	return hr;
}

HRESULT ECExchangeModifyTable::SaveACLS(ECMAPIProp *lpecMapiProp, ECMemTable *lpTable)
{
	HRESULT hr = hrSuccess;
	LPSRowSet		lpRowSet = NULL;
	LPSPropValue	lpIDs = NULL;
	LPULONG			lpulStatus = NULL;

	LPSPropValue lpMemberEntryID = NULL; //do not free
	LPSPropValue lpMemberRights = NULL; //do not free
	LPSPropValue lpMemberID = NULL; //do not free
	
	LPECPERMISSION lpECPermissions = NULL;
	ULONG			cECPerm = 0;

	entryId sEntryId = {0};
	IECSecurity *lpSecurity = NULL;

	// Get the ACLS
	hr = lpecMapiProp->QueryInterface(IID_IECSecurity, (void**)&lpSecurity);
	if (hr != hrSuccess)
		goto exit;

	// Get a data  
	hr = lpTable->HrGetAllWithStatus(&lpRowSet, &lpIDs, &lpulStatus);
	if (hr != hrSuccess)
		goto exit;

	hr = MAPIAllocateBuffer(sizeof(ECPERMISSION)*lpRowSet->cRows, (void**)&lpECPermissions);
	if (hr != hrSuccess)
		goto exit;

	for(ULONG i=0; i < lpRowSet->cRows; i++)
	{
		if (lpulStatus[i]  == ECROW_NORMAL)
			continue;

		lpECPermissions[cECPerm].ulState = RIGHT_AUTOUPDATE_DENIED;
		lpECPermissions[cECPerm].ulType = ACCESS_TYPE_GRANT;

		if (lpulStatus[i] == ECROW_DELETED) {
			lpECPermissions[cECPerm].ulState |= RIGHT_DELETED;
		} else if (lpulStatus[i] == ECROW_ADDED) {
			lpECPermissions[cECPerm].ulState |= RIGHT_NEW;
		}else if (lpulStatus[i] == ECROW_MODIFIED) {
			lpECPermissions[cECPerm].ulState |= RIGHT_MODIFY;
		}

		lpMemberID = PpropFindProp(lpRowSet->aRow[i].lpProps, lpRowSet->aRow[i].cValues, PR_MEMBER_ID);
		lpMemberEntryID = PpropFindProp(lpRowSet->aRow[i].lpProps, lpRowSet->aRow[i].cValues, PR_MEMBER_ENTRYID);
		lpMemberRights = PpropFindProp(lpRowSet->aRow[i].lpProps, lpRowSet->aRow[i].cValues, PR_MEMBER_RIGHTS);

		if (lpMemberID == NULL || lpMemberRights == NULL || (lpMemberID->Value.ul != 0 && lpMemberEntryID == NULL))
			continue;

		if (lpMemberID->Value.ul != 0) {
			lpECPermissions[cECPerm].sUserId.cb = lpMemberEntryID->Value.bin.cb;
			lpECPermissions[cECPerm].sUserId.lpb = lpMemberEntryID->Value.bin.lpb;
		} else {
			// Create everyone entryid
			// NOTE: still makes a V0 entry id, because externid id part is empty
			if(ABIDToEntryID(NULL, 1, objectid_t(DISTLIST_GROUP), &sEntryId) != erSuccess) {
				hr = MAPI_E_CALL_FAILED;
				goto exit;
			}

			lpECPermissions[cECPerm].sUserId.cb = sEntryId.__size;
			MAPIAllocateMore(lpECPermissions[cECPerm].sUserId.cb, lpECPermissions, (void**)&lpECPermissions[cECPerm].sUserId.lpb);
			memcpy(lpECPermissions[cECPerm].sUserId.lpb, sEntryId.__ptr, sEntryId.__size);

			FreeEntryId(&sEntryId, false);
		}

		lpECPermissions[cECPerm].ulRights = lpMemberRights->Value.ul&ecRightsAll;

		cECPerm++;
	}

	if (cECPerm > 0)
	{
		hr = lpSecurity->SetPermissionRules(cECPerm, lpECPermissions);
		if (hr != hrSuccess)
			goto exit;
	}

exit:
	if (lpSecurity)
		lpSecurity->Release();

	if (lpECPermissions)
		MAPIFreeBuffer(lpECPermissions);
	
	if(lpIDs)
		MAPIFreeBuffer(lpIDs);

	if(lpRowSet)
		FreeProws(lpRowSet);

	if(lpulStatus)
		MAPIFreeBuffer(lpulStatus);

	return hr;
}

// Serializes the rules ECMemTable data into an XML stream.
HRESULT	ECExchangeModifyTable::HrSerializeTable(ECMemTable *lpTable, char **lppSerialized)
{
	HRESULT hr = hrSuccess;
	ECMemTableView *lpView = NULL;
	LPSPropTagArray lpCols = NULL;
	LPSRowSet		lpRowSet = NULL;
	std::ostringstream os;
	struct rowSet *	lpSOAPRowSet = NULL;
	char *szXML = NULL;
	struct soap soap;

	// Get a view
	hr = lpTable->HrGetView(createLocaleFromName(""), MAPI_UNICODE, &lpView);
	if(hr != hrSuccess)
		goto exit;

	// Get all Columns
	hr = lpView->QueryColumns(TBL_ALL_COLUMNS, &lpCols);
	if(hr != hrSuccess)
		goto exit;

	hr = lpView->SetColumns(lpCols, 0);
	if(hr != hrSuccess)
		goto exit;

	// Get all rows
	hr = lpView->QueryRows(0x7fffffff, 0, &lpRowSet);
	if(hr != hrSuccess)
		goto exit;

	// we need to convert data from clients which save PT_STRING8 inside PT_SRESTRICTION and PT_ACTIONS structures,
	// because unicode clients won't be able to understand those anymore.
	hr = ConvertString8ToUnicode(lpRowSet);
	if(hr != hrSuccess)
		goto exit;


	// Convert to SOAP rows
	hr = CopyMAPIRowSetToSOAPRowSet(lpRowSet, &lpSOAPRowSet);
	if(hr != hrSuccess)
		goto exit;

	// Convert to XML
	soap_set_omode(&soap, SOAP_C_UTFSTRING);
	soap_begin(&soap);
	soap.os = &os;
	soap_serialize_rowSet(&soap, lpSOAPRowSet);
	soap_begin_send(&soap);
	soap_put_rowSet(&soap, lpSOAPRowSet,"tableData","rowSet");
	soap_end_send(&soap);

	// os now contains XML for row data
	szXML = new char [ os.str().size()+1 ];
	strcpy(szXML, os.str().c_str());
	szXML[os.str().size()] = 0;

	*lppSerialized = szXML;

exit:
	if(lpSOAPRowSet)
		FreeRowSet(lpSOAPRowSet, true);
	if(lpRowSet)
		FreeProws(lpRowSet);
	if(lpCols)
		MAPIFreeBuffer(lpCols);
	if(lpView)
		lpView->Release();
		
	soap_end(&soap); // clean up allocated temporaries 

	return hr;
}

// Deserialize the rules xml data to ECMemtable
HRESULT ECExchangeModifyTable::HrDeserializeTable(char *lpSerialized, ECMemTable *lpTable, ULONG *ulRuleId)
{
	HRESULT hr = hrSuccess;
	std::istringstream is(lpSerialized);
	struct rowSet sSOAPRowSet;
	LPSRowSet lpsRowSet = NULL;
	LPSPropValue lpProps = NULL;
	ULONG cValues;
	SPropValue		sRowId;
	ULONG ulHighestRuleID = 1;
	unsigned int i, n;
	struct soap soap;
	convert_context converter;

	soap.is = &is;
	soap_set_imode(&soap, SOAP_C_UTFSTRING);
	soap_begin(&soap);
	soap_begin_recv(&soap);
	if (!soap_get_rowSet(&soap, &sSOAPRowSet, "tableData", "rowSet")) {
		hr = MAPI_E_CORRUPT_DATA;
		goto exit;
	}
	soap_end_recv(&soap); 

	hr = CopySOAPRowSetToMAPIRowSet(NULL, &sSOAPRowSet, &lpsRowSet, 0);
	if(hr != hrSuccess)
		goto exit;

	for (i = 0; i < lpsRowSet->cRows; i++) {
		// Note: the ECKeyTable only uses an ULONG as the key.
		//       Information placed in the HighPart of this PT_I8 is lost!
		sRowId.ulPropTag = PR_RULE_ID;
		sRowId.Value.li.QuadPart = ulHighestRuleID++;

		hr = Util::HrAddToPropertyArray(lpsRowSet->aRow[i].lpProps, lpsRowSet->aRow[i].cValues, &sRowId, &lpProps, &cValues);
		if(hr != hrSuccess)
			goto exit;

		for (n = 0; n < cValues; n++) {
			// If a string type is PT_STRING8, it's old and assumed to be WTF-1252.
			if (PROP_TYPE(lpProps[n].ulPropTag) == PT_STRING8) {
				lpProps[n].ulPropTag = CHANGE_PROP_TYPE(lpProps[n].ulPropTag, PT_UNICODE);
				lpProps[n].Value.lpszW = WTF1252_to_WCHAR(lpProps[n].Value.lpszA, lpProps, &converter);
			}
		}

		hr = lpTable->HrModifyRow(ECKeyTable::TABLE_ROW_ADD, &sRowId, lpProps, cValues);
		if(hr != hrSuccess)
			goto exit;

		MAPIFreeBuffer(lpProps);
		lpProps = NULL;
	}
	*ulRuleId = ulHighestRuleID;

exit:
	if(lpsRowSet)
		FreeProws(lpsRowSet);

	if (lpProps)
		MAPIFreeBuffer(lpProps);

	soap_end(&soap); // clean up allocated temporaries 

	return hr;
}

// wrappers for ExchangeModifyTable

HRESULT __stdcall ECExchangeModifyTable::xExchangeModifyTable::QueryInterface(REFIID refiid , void** lppInterface) {
	TRACE_MAPI(TRACE_ENTRY, "IExchangeModifyTable::QueryInterface", "%s", DBGGUIDToString(refiid).c_str());
	METHOD_PROLOGUE_(ECExchangeModifyTable , ExchangeModifyTable);
	HRESULT hr = pThis->QueryInterface(refiid, lppInterface);
	TRACE_MAPI(TRACE_RETURN, "IExchangeModifyTable::QueryInterface", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

ULONG __stdcall ECExchangeModifyTable::xExchangeModifyTable::AddRef() {
	TRACE_MAPI(TRACE_ENTRY, "IExchangeModifyTable::AddRef", "");
	METHOD_PROLOGUE_(ECExchangeModifyTable , ExchangeModifyTable);
	return pThis->AddRef();
}

ULONG __stdcall ECExchangeModifyTable::xExchangeModifyTable::Release() {
	TRACE_MAPI(TRACE_ENTRY, "IExchangeModifyTable::Release", "");
	METHOD_PROLOGUE_(ECExchangeModifyTable , ExchangeModifyTable);
	return pThis->Release();

}

HRESULT __stdcall ECExchangeModifyTable::xExchangeModifyTable::GetLastError(HRESULT hResult, ULONG ulFlags, LPMAPIERROR *lppMAPIError) {
	TRACE_MAPI(TRACE_ENTRY, "IExchangeModifyTable::GetLastError", "result=0x%08x", hResult);
	METHOD_PROLOGUE_(ECExchangeModifyTable , ExchangeModifyTable);
	HRESULT hr = pThis->GetLastError(hResult, ulFlags, lppMAPIError);
	TRACE_MAPI(TRACE_RETURN, "IExchangeModifyTable::GetLastError", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT __stdcall ECExchangeModifyTable::xExchangeModifyTable::GetTable(ULONG ulFlags, LPMAPITABLE *lppTable) {
	TRACE_MAPI(TRACE_ENTRY, "IExchangeModifyTable::GetTable", "flags=%d", ulFlags);
	METHOD_PROLOGUE_(ECExchangeModifyTable , ExchangeModifyTable);
	HRESULT hr = pThis->GetTable(ulFlags, lppTable);
	TRACE_MAPI(TRACE_RETURN, "IExchangeModifyTable::GetTable", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT __stdcall ECExchangeModifyTable::xExchangeModifyTable::ModifyTable(ULONG ulFlags, LPROWLIST lpMods) {
	TRACE_MAPI(TRACE_ENTRY, "IExchangeModifyTable::ModifyTable", "flags=%d, rows: %s", ulFlags, RowSetToString((LPSRowSet)lpMods).c_str());
	METHOD_PROLOGUE_(ECExchangeModifyTable , ExchangeModifyTable);
	HRESULT hr = pThis->ModifyTable(ulFlags, lpMods);
	TRACE_MAPI(TRACE_RETURN, "IExchangeModifyTable::ModifyTable", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}



// ExchangeRuleAction object

ECExchangeRuleAction::ECExchangeRuleAction() {
}

ECExchangeRuleAction::~ECExchangeRuleAction() {
}

HRESULT __stdcall ECExchangeRuleAction::ActionCount(ULONG *lpcActions) {
	*lpcActions = 0;
	return hrSuccess;
}

HRESULT __stdcall ECExchangeRuleAction::GetAction(ULONG ulActionNumber, LARGE_INTEGER *lpruleid, LPACTION *lppAction) {
	return MAPI_E_NO_SUPPORT;
}


// wrappers for ExchageRuleAction class

HRESULT __stdcall ECExchangeRuleAction::xExchangeRuleAction::QueryInterface(REFIID refiid , void** lppInterface) {
 	TRACE_MAPI(TRACE_ENTRY, "IExchangeRuleAction::QueryInterface", "%s", DBGGUIDToString(refiid).c_str());
	METHOD_PROLOGUE_(ECExchangeRuleAction , ExchangeRuleAction);
	return pThis->QueryInterface(refiid, lppInterface);
}

ULONG __stdcall ECExchangeRuleAction::xExchangeRuleAction::AddRef() {
	TRACE_MAPI(TRACE_ENTRY, "IExchangeRuleAction::AddRef", "");
	METHOD_PROLOGUE_(ECExchangeRuleAction , ExchangeRuleAction);
	return pThis->AddRef();
}

ULONG __stdcall ECExchangeRuleAction::xExchangeRuleAction::Release() {
	TRACE_MAPI(TRACE_ENTRY, "IExchangeRuleAction::Release", "");
	METHOD_PROLOGUE_(ECExchangeRuleAction , ExchangeRuleAction);
	return pThis->Release();
}

HRESULT __stdcall ECExchangeRuleAction::xExchangeRuleAction::ActionCount(ULONG *lpcActions) {
	TRACE_MAPI(TRACE_ENTRY, "IExchangeRuleAction::ActionCount", "");
	METHOD_PROLOGUE_(ECExchangeRuleAction , ExchangeRuleAction);
	return pThis->ActionCount(lpcActions);
}

HRESULT __stdcall ECExchangeRuleAction::xExchangeRuleAction::GetAction(ULONG ulActionNumber, LARGE_INTEGER *lpruleid,
																		 LPACTION *lppAction) {
	TRACE_MAPI(TRACE_ENTRY, "IExchangeRuleAction::GetAction", "");
	METHOD_PROLOGUE_(ECExchangeRuleAction , ExchangeRuleAction);
	return pThis->GetAction(ulActionNumber, lpruleid, lppAction);
}
