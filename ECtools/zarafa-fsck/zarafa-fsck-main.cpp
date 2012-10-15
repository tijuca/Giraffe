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

#include <iostream>
#include <set>
#include <list>

#include <CommonUtil.h>
#include <mapiext.h>
#include <mapiguidext.h>
#include <mapiutil.h>
#include <mapix.h>
#include <stringutil.h>

#include "zarafa-fsck.h"

bool ReadYesNoMessage(string strMessage, string strAuto)
{
	string strReply;

	cout << strMessage << " [yes/no]: ";

	if (strAuto.empty())
		getline(cin, strReply);
	else {
		cout << strAuto << endl;
		strReply = strAuto;
	}

	return (strReply[0] == 'y' || strReply[0] == 'Y');
}

HRESULT DeleteEntry(LPMAPIFOLDER lpFolder, LPSPropValue lpItemProperty)
{
	HRESULT hr = hrSuccess;
	LPENTRYLIST lpEntryList = NULL;

	hr = MAPIAllocateBuffer(sizeof(ENTRYLIST), (void**)&lpEntryList);
	if (hr != hrSuccess)
		goto exit;

	hr = MAPIAllocateMore(sizeof(SBinary), lpEntryList, (void**)&lpEntryList->lpbin);
	if (hr != hrSuccess)
		goto exit;

	lpEntryList->cValues = 1;
	lpEntryList->lpbin[0].cb = lpItemProperty->Value.bin.cb;
	lpEntryList->lpbin[0].lpb = lpItemProperty->Value.bin.lpb;

	hr = lpFolder->DeleteMessages(lpEntryList, 0, NULL, 0);

exit:
	if (lpEntryList)
		MAPIFreeBuffer(lpEntryList);

	if (hr == hrSuccess)
		cout << "Item deleted." << endl;
	else
		cout << "Failed to delete entry." << endl;

	return hr;
}

HRESULT FixProperty(LPMESSAGE lpMessage, string strName, ULONG ulTag, __UPV Value)
{
	HRESULT hr = hrSuccess;
	SPropValue ErrorProp;

	ErrorProp.ulPropTag = ulTag;
	ErrorProp.Value = Value;
	/* NOTE: Named properties don't have the PT value set by default,
	   The caller of this function should have taken care of this. */
	if (PROP_ID(ulTag) && PROP_TYPE(ulTag)) {
		hr = lpMessage->SetProps(1, &ErrorProp, NULL);
		if (hr != hrSuccess) {
			cout << "Failed to fix broken property." << endl;
			goto exit;
		}

		hr = lpMessage->SaveChanges(KEEP_OPEN_READWRITE);
		if (hr != hrSuccess)
			cout << "Failed to save changes to fix broken property";
	} else {
		cout << "Invalid property tag: " << stringify(ulTag, true) << endl;
		hr = MAPI_E_INVALID_PARAMETER;
	}

exit:
	return hr;
}

HRESULT DetectFolderEntryDetails(LPMESSAGE lpMessage, string *lpName, string *lpClass)
{
	HRESULT hr = hrSuccess;
	LPSPropValue lpPropertyArray;
	ULONG ulPropertyCount;

	SizedSPropTagArray(3, PropertyTagArray) = {
		3,
		{
			PR_SUBJECT_A,
			PR_NORMALIZED_SUBJECT_A,
			PR_MESSAGE_CLASS_A,
		}
	};

	hr = lpMessage->GetProps((LPSPropTagArray)&PropertyTagArray,
				 0,
				 &ulPropertyCount,
				 &lpPropertyArray);
	if (FAILED(hr)) {
		cout << "Failed to obtain all properties." << endl;
		goto exit;
	}

	for (ULONG i = 0; i < ulPropertyCount; i++) {
		if (PROP_TYPE(lpPropertyArray[i].ulPropTag) == PT_ERROR)
			continue;
		else if (lpPropertyArray[i].ulPropTag == PR_SUBJECT_A)
			*lpName = lpPropertyArray[i].Value.lpszA;
		else if (lpPropertyArray[i].ulPropTag == PR_NORMALIZED_SUBJECT_A)
			*lpName = lpPropertyArray[i].Value.lpszA;
		else if (lpPropertyArray[i].ulPropTag == PR_MESSAGE_CLASS_A)
			*lpClass = lpPropertyArray[i].Value.lpszA;
	}

	/*
	 * The name is allowed to be empty, the class however not.
	 */
	if (lpClass->empty())
		cout << "Unable to detect message class.";
	else
		hr = hrSuccess;

exit:
	if (lpPropertyArray)
		MAPIFreeBuffer(lpPropertyArray);

	return hr;
}

HRESULT ProcessFolderEntry(ZarafaFsck *lpFsck, LPMAPIFOLDER lpFolder, LPSRow lpRow)
{
	HRESULT hr = hrSuccess;
	LPSPropValue lpItemProperty = NULL;
	LPMESSAGE lpMessage = NULL;
	ULONG ulObjectType = 0;
	string strName;
	string strClass;

	lpItemProperty = PpropFindProp(lpRow->lpProps, lpRow->cValues, PR_ENTRYID);
	if (!lpItemProperty) {
		cout << "Row does not contain an EntryID." << endl;
		goto exit;
	}

	hr = lpFolder->OpenEntry(lpItemProperty->Value.bin.cb,
			         (LPENTRYID)lpItemProperty->Value.bin.lpb,
			         &IID_IMessage,
			         MAPI_MODIFY,
			         &ulObjectType,
			         (IUnknown**)&lpMessage);
	if (hr != hrSuccess) {
		cout << "Failed to open EntryID." << endl;
		goto exit;
	}

	hr = DetectFolderEntryDetails(lpMessage, &strName, &strClass);
	if (hr != hrSuccess)
		goto exit;

	hr = lpFsck->ValidateMessage(lpMessage, strName, strClass);
	if (hr != hrSuccess)
		goto exit;

exit:
	if (lpMessage)
		lpMessage->Release();

	if (hr != hrSuccess)
		hr = lpFsck->DeleteMessage(lpFolder, lpItemProperty);

	return hr;
}

HRESULT ProcessFolder(ZarafaFsck *lpFsck, LPMAPIFOLDER lpFolder, string strName)
{
	HRESULT hr = hrSuccess;
	LPMAPITABLE lpTable = NULL;
	LPSRowSet lpRows = NULL;
	ULONG ulCount;

	hr = lpFolder->GetContentsTable(0, &lpTable);
 	if(hr != hrSuccess) {
		cout << "Failed to open Folder table." << endl;
		goto exit;
	}

	/*
	 * Check if we have found at least *something*.
	 */
	hr = lpTable->GetRowCount(0, &ulCount);
	if(hr != hrSuccess) {
		cout << "Failed to count number of rows." << endl;
		goto exit;
	} else if (!ulCount) {
		cout << "No entries inside folder." << endl;
		goto exit;
	}

	/*
	 * Loop through each row/entry and validate.
	 */
	while (true) {
		hr = lpTable->QueryRows(20, 0, &lpRows);
		if (hr != hrSuccess)
			break;

		if (lpRows->cRows == 0)
			break;

		for (ULONG i = 0; i < lpRows->cRows; i++) {
			hr = ProcessFolderEntry(lpFsck, lpFolder, &lpRows->aRow[i]);
			if (hr != hrSuccess) {
				cout << "Failed to validate entry." << endl;
				// Move along, nothing to see.
			}
		}

		if (lpRows) {
			FreeProws(lpRows);
			lpRows = NULL;
		}
	}

exit:
	if (lpRows) {
		FreeProws(lpRows);
		lpRows = NULL;
	}

	if (lpTable)
		lpTable->Release();

	return hr;
}

/*
 * ZarafaFsck implementation.
 */
ZarafaFsck::ZarafaFsck()
{
	ulFolders = 0;
	ulEntries = 0;
	ulProblems = 0;
	ulFixed = 0;
	ulDeleted = 0;
}

HRESULT ZarafaFsck::ValidateMessage(LPMESSAGE lpMessage,
				    string strName, string strClass)
{
	HRESULT hr = hrSuccess;

	cout << "Validating entry: \"" << strName << "\"" << endl;

	this->ulEntries++;
	hr = this->ValidateItem(lpMessage, strClass);

	cout << "Validating of entry \"" << strName << "\" ended" << endl;

	return hr;
}

HRESULT ZarafaFsck::ValidateFolder(LPMAPIFOLDER lpFolder, string strName)
{
	HRESULT hr = hrSuccess;

	cout << "Validating folder \"" << strName << "\"" << endl;

	this->ulFolders++;
	hr = ProcessFolder(this, lpFolder, strName);

	cout << "Validating of folder \"" << strName << "\" ended" << endl;

	return hr;
}

HRESULT ZarafaFsck::AddMissingProperty(LPMESSAGE lpMessage, string strName, ULONG ulTag, __UPV Value)
{
	HRESULT hr = hrSuccess;

	cout << "Missing property " << strName << endl;

	this->ulProblems++;
	if (ReadYesNoMessage("Add missing property?", auto_fix)) {
		hr = FixProperty(lpMessage, strName, ulTag, Value);
		if (hr == hrSuccess)
			this->ulFixed++;
	}

	return hr;
}

HRESULT ZarafaFsck::ReplaceProperty(LPMESSAGE lpMessage, string strName, ULONG ulTag, string strError, __UPV Value)
{
	HRESULT hr = hrSuccess;

	cout << "Invalid property " << strName << " - " << strError << endl;

	this->ulProblems++;
	if (ReadYesNoMessage("Fix broken property?", auto_fix)) {
		hr = FixProperty(lpMessage, strName, ulTag, Value);
		if (hr == hrSuccess)
			this->ulFixed++;
	}

	return hr;
}

HRESULT ZarafaFsck::DeleteRecipientList(LPMESSAGE lpMessage, std::list<unsigned int> &mapiReciptDel, bool &bChanged)
{
	HRESULT hr = hrSuccess;

	std::list<unsigned int>::iterator iter;
	SRowSet *lpMods = NULL;

	this->ulProblems++;

	cout << mapiReciptDel.size() << " duplicate or invalid recipients found. " << endl;

	if (ReadYesNoMessage("Remove duplicate or invalid recipients?", auto_fix) )
	{
		hr = MAPIAllocateBuffer(CbNewADRLIST(mapiReciptDel.size()), (void**)&lpMods);
		if (hr != hrSuccess)
			goto exit;

		lpMods->cRows = 0;
		for(iter = mapiReciptDel.begin(); iter != mapiReciptDel.end(); iter++) {
			lpMods->aRow[lpMods->cRows].cValues = 1;
			MAPIAllocateMore(sizeof(SPropValue), lpMods, (void**)&lpMods->aRow[lpMods->cRows].lpProps);
			lpMods->aRow[lpMods->cRows].lpProps->ulPropTag = PR_ROWID;
			lpMods->aRow[lpMods->cRows].lpProps->Value.ul = *iter;

			lpMods->cRows++;
		}

		hr = lpMessage->ModifyRecipients(MODRECIP_REMOVE, (LPADRLIST)lpMods);
		if (hr != hrSuccess)
			goto exit;

		hr = lpMessage->SaveChanges(KEEP_OPEN_READWRITE);
		if (hr != hrSuccess)
			goto exit;

		bChanged = true;

		this->ulFixed++;
	}
exit:

	if (lpMods)
		MAPIFreeBuffer(lpMods);

	return hr;
}

HRESULT ZarafaFsck::DeleteMessage(LPMAPIFOLDER lpFolder, LPSPropValue lpItemProperty)
{
	HRESULT hr = hrSuccess;

	if (ReadYesNoMessage("Delete message?", auto_del)) {
		hr = DeleteEntry(lpFolder, lpItemProperty);
		if (hr == hrSuccess)
			this->ulDeleted++;
	}

	return hr;
}

HRESULT ZarafaFsck::ValidateRecursiveDuplicateRecipients(LPMESSAGE lpMessage, bool &bChanged)
{
	HRESULT hr = hrSuccess;
	bool bSubChanged = false;
	LPATTACH lpAttach = NULL;
	LPMAPITABLE lpTable = NULL;
    ULONG cRows = 0;
    SRowSet *pRows = NULL;
	LPMESSAGE lpSubMessage = NULL;

	SizedSPropTagArray(2, sptaProps) = {2, {PR_ATTACH_NUM, PR_ATTACH_METHOD}};

	hr = lpMessage->GetAttachmentTable(0, &lpTable);
	if (hr != hrSuccess)
		goto exit;

	hr = lpTable->GetRowCount(0, &cRows);
	if (hr != hrSuccess)
		goto exit;

	if (cRows == 0)
		goto message;

	hr = lpTable->SetColumns((LPSPropTagArray)&sptaProps, 0);
	if (hr != hrSuccess)
		goto exit;

	while (true) {
		hr = lpTable->QueryRows(50, 0, &pRows);
		if (hr != hrSuccess)
			goto exit;

		if (pRows->cRows == 0)
			break;

		for (unsigned int i = 0; i < pRows->cRows; i++) {
			if (pRows->aRow[i].lpProps[1].ulPropTag == PR_ATTACH_METHOD && pRows->aRow[i].lpProps[1].Value.ul == ATTACH_EMBEDDED_MSG)
			{
				bSubChanged = false;

				hr = lpMessage->OpenAttach(pRows->aRow[i].lpProps[0].Value.ul, NULL, MAPI_BEST_ACCESS, &lpAttach);
				if (hr != hrSuccess)
					goto exit;

				hr = lpAttach->OpenProperty(PR_ATTACH_DATA_OBJ, &IID_IMessage, 0, MAPI_MODIFY, (LPUNKNOWN*)&lpSubMessage);
				if (hr != hrSuccess)
					goto exit;

				hr = ValidateRecursiveDuplicateRecipients(lpSubMessage, bSubChanged);
				if (hr != hrSuccess)
					goto exit;

				if (bSubChanged) {
					hr = lpAttach->SaveChanges(KEEP_OPEN_READWRITE);
					if (hr != hrSuccess)
						goto exit;

					bChanged = bSubChanged;
				}
				lpAttach->Release();
				lpAttach = NULL;
				lpSubMessage->Release();
				lpSubMessage = NULL;

			}

		}

		FreeProws(pRows);
		pRows = NULL;
	}


message:
	lpTable->Release();
	lpTable = NULL;

	hr = ValidateDuplicateRecipients(lpMessage, bChanged);
	if (hr != hrSuccess)
		goto exit;

	if (bChanged)
		lpMessage->SaveChanges(KEEP_OPEN_READWRITE);

exit:
	if (lpTable)
		lpTable->Release();

	if (lpAttach)
		lpAttach->Release();

	if (lpSubMessage)
		lpSubMessage->Release();

	if (pRows)
		FreeProws(pRows);

	return hr;
}

HRESULT ZarafaFsck::ValidateDuplicateRecipients(LPMESSAGE lpMessage, bool &bChanged)
{
	HRESULT hr = hrSuccess;
	LPMAPITABLE lpTable = NULL;
	ULONG cRows = 0;
	std::set<std::string> mapRecip;
	std::list<unsigned int> mapiReciptDel;
	std::list<unsigned int>::iterator iter;
	SRowSet *pRows = NULL;
	std::string strData;
	std::pair<std::set<std::string>::iterator, bool> res;
	unsigned int i = 0;

	SizedSPropTagArray(5, sptaProps) = {5, {PR_ROWID, PR_DISPLAY_NAME_A, PR_EMAIL_ADDRESS_A, PR_RECIPIENT_TYPE, PR_ENTRYID}};

	hr = lpMessage->GetRecipientTable(0, &lpTable);
	if (hr != hrSuccess)
		goto exit;

	hr = lpTable->GetRowCount(0, &cRows);
	if (hr != hrSuccess)
		goto exit;


	if (cRows < 1) {
		// 0 or 1 row not needed to check
		goto exit;
	}

	hr = lpTable->SetColumns((LPSPropTagArray)&sptaProps, 0);
	if (hr != hrSuccess)
		goto exit;

	while (true) {
		hr = lpTable->QueryRows(50, 0, &pRows);
		if (hr != hrSuccess)
			goto exit;

		if (pRows->cRows == 0)
			break;

		for (i = 0; i < pRows->cRows; i++) {

			if (pRows->aRow[i].lpProps[1].ulPropTag != PR_DISPLAY_NAME_A && pRows->aRow[i].lpProps[2].ulPropTag != PR_EMAIL_ADDRESS_A) {
				mapiReciptDel.push_back(pRows->aRow[i].lpProps[0].Value.ul);
				continue;
			}

			// Invalid or missing entryid 
			if (pRows->aRow[i].lpProps[4].ulPropTag != PR_ENTRYID || pRows->aRow[i].lpProps[4].Value.bin.cb == 0) {
				mapiReciptDel.push_back(pRows->aRow[i].lpProps[0].Value.ul);
				continue;
			}

			strData.clear();

			if (pRows->aRow[i].lpProps[1].ulPropTag == PR_DISPLAY_NAME_A)   strData += pRows->aRow[i].lpProps[1].Value.lpszA;
			if (pRows->aRow[i].lpProps[2].ulPropTag == PR_EMAIL_ADDRESS_A)  strData += pRows->aRow[i].lpProps[2].Value.lpszA;
			if (pRows->aRow[i].lpProps[3].ulPropTag == PR_RECIPIENT_TYPE)   strData += stringify(pRows->aRow[i].lpProps[3].Value.ul);

			res = mapRecip.insert(strData);
			if (res.second == false)
				mapiReciptDel.push_back(pRows->aRow[i].lpProps[0].Value.ul);
		}

		FreeProws(pRows);
		pRows = NULL;
	}

	lpTable->Release();
	lpTable = NULL;

	// modify
	if (!mapiReciptDel.empty()) {
		hr = DeleteRecipientList(lpMessage, mapiReciptDel, bChanged);
	}


	
exit:
	if (lpTable)
		lpTable->Release();

	if (pRows)
		FreeProws(pRows);
	return hr;
}
void ZarafaFsck::PrintStatistics(string title)
{
	cout << title << endl;
	cout << "\tFolders:\t" << ulFolders << endl;
	cout << "\tEntries:\t" << ulEntries << endl;
	cout << "\tProblems:\t" << ulProblems << endl;
	cout << "\tFixed:\t\t" << ulFixed << endl;
	cout << "\tDeleted:\t" << ulDeleted << endl;
}
