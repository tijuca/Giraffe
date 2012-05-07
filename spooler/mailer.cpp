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
#include "mailer.h"
#include "archive.h"

#include <mapitags.h>
#include <mapiext.h>
#include <mapiutil.h>
#include <mapidefs.h>
#include <mapix.h>
#include <mapi.h>

#include "ECLogger.h"
#include "ECConfig.h"
#include "IECUnknown.h"
#include "IECSecurity.h"
#include "IECServiceAdmin.h"
#include "IECSpooler.h"
#include "ECGuid.h"
#include "edkguid.h"
#include "CommonUtil.h"
#include "Util.h"
#include "stringutil.h"
#include "mapicontact.h"
#include "mapiguidext.h"
#include "EMSAbTag.h"
#include "ECABEntryID.h"
#include "ECGetText.h"

#include "charset/convert.h"
#include "charset/convstring.h"

#include <list>
#include <algorithm>
using namespace std;

extern ECConfig *g_lpConfig;
extern ECLogger *g_lpLogger;

/**
 * Expand all rows in the lpTable to normal user recipient
 * entries. When a group is expanded from a group, this function will
 * be called recursively.
 *
 * @param[in]	lpAddrBook	The Global Addressbook of the user sending lpMessage
 * @param[in]	lpMessage	The message to expand groups for
 * @param[in]	lpTable		The restricted recipient table of lpMessage
 * @param[in]	lpEntryRestriction The restriction used on lpTable
 * @param[in]	ulRecipType	The recipient type (To/Cc/Bcc), default is MAPI_TO
 * @param[in]	lpExpandedGroups	List of EntryIDs of groups already expanded. Double groups will just be removed.
 * @param[in]	recurrence	true if this function should recurse further.
 */
HRESULT ExpandRecipientsRecursive(LPADRBOOK lpAddrBook, IMessage *lpMessage,
								  IMAPITable *lpTable, LPSRestriction lpEntryRestriction, ULONG ulRecipType,
								  list<SBinary> *lpExpandedGroups, bool recurrence = true)
{
	HRESULT			hr = hrSuccess;
	IMAPITable		*lpContentsTable = NULL;
	LPSRowSet		lpsRowSet = NULL;
	LPDISTLIST		lpDistlist = NULL;
	ULONG			ulObj = 0;
	bool			bExpandSub = recurrence;
	LPSPropValue	lpSMTPAddress = NULL;

	SizedSPropTagArray(7, sptaColumns) = {
		7, {
			PR_ROWID,
			PR_DISPLAY_NAME_W,
			PR_SMTP_ADDRESS_W,
			PR_RECIPIENT_TYPE,
			PR_OBJECT_TYPE,
			PR_DISPLAY_TYPE,
			PR_ENTRYID,
		}
	};

	hr = lpTable->SetColumns((LPSPropTagArray)&sptaColumns, 0);
	if (hr != hrSuccess)
		goto exit;

	while (true) {
		LPSPropValue lpRowId;
		LPSPropValue lpEntryId;
		LPSPropValue lpDisplayType;
		LPSPropValue lpObjectType;
		LPSPropValue lpRecipType;
		LPSPropValue lpDisplayName;
		LPSPropValue lpEmailAddress;

		/* Perform cleanup first. */
		if (lpsRowSet) {
			FreeProws(lpsRowSet);
			lpsRowSet = NULL;
		}

		if (lpDistlist) {
			lpDistlist->Release();
			lpDistlist = NULL;
		}

		if (lpContentsTable) {
			lpContentsTable->Release();
			lpContentsTable = NULL;
		}
		
		if (lpSMTPAddress) {
			MAPIFreeBuffer(lpSMTPAddress);
			lpSMTPAddress = NULL;
		}

		/* Request group from table */
		hr = lpTable->QueryRows(1, 0, &lpsRowSet);
		if (hr != hrSuccess)
			goto exit;

		if (lpsRowSet->cRows != 1)
			break;

		/* From this point on we use 'continue' when something fails,
		 * since all errors are related to the current entry and we should
		 * make sure we resolve as many recipients as possible. */

		lpRowId = PpropFindProp(lpsRowSet->aRow[0].lpProps, lpsRowSet->aRow[0].cValues, PR_ROWID);
		lpEntryId = PpropFindProp(lpsRowSet->aRow[0].lpProps, lpsRowSet->aRow[0].cValues, PR_ENTRYID);
		lpDisplayType = PpropFindProp(lpsRowSet->aRow[0].lpProps, lpsRowSet->aRow[0].cValues, PR_DISPLAY_TYPE);
		lpObjectType = PpropFindProp(lpsRowSet->aRow[0].lpProps, lpsRowSet->aRow[0].cValues, PR_OBJECT_TYPE);
		lpRecipType = PpropFindProp(lpsRowSet->aRow[0].lpProps, lpsRowSet->aRow[0].cValues, PR_RECIPIENT_TYPE);

		lpDisplayName = PpropFindProp(lpsRowSet->aRow[0].lpProps, lpsRowSet->aRow[0].cValues, PR_DISPLAY_NAME_W);
		lpEmailAddress = PpropFindProp(lpsRowSet->aRow[0].lpProps, lpsRowSet->aRow[0].cValues, PR_SMTP_ADDRESS_W);

		/* lpRowId, lpRecipType, and lpDisplayType are optional.
		 * lpEmailAddress is only mandatory for MAPI_MAILUSER */
		if (!lpEntryId || !lpObjectType || !lpDisplayName)
			continue;

		/* By default we inherit the recipient type from parent */
		if (lpRecipType)
			ulRecipType = lpRecipType->Value.ul;

		if (lpObjectType->Value.ul == MAPI_MAILUSER) {
			if (!lpEmailAddress)
				continue;

			SRowSet sRowSMTProwSet;
			SPropValue sPropSMTPProperty[4];

			sRowSMTProwSet.cRows = 1;
			sRowSMTProwSet.aRow[0].cValues = 4;
			sRowSMTProwSet.aRow[0].lpProps = sPropSMTPProperty;

			sRowSMTProwSet.aRow[0].lpProps[0].ulPropTag = PR_EMAIL_ADDRESS_W;
			sRowSMTProwSet.aRow[0].lpProps[0].Value.lpszW = lpEmailAddress->Value.lpszW;

			sRowSMTProwSet.aRow[0].lpProps[1].ulPropTag = PR_SMTP_ADDRESS_W;
			sRowSMTProwSet.aRow[0].lpProps[1].Value.lpszW = lpEmailAddress->Value.lpszW;

			sRowSMTProwSet.aRow[0].lpProps[2].ulPropTag = PR_RECIPIENT_TYPE;
			sRowSMTProwSet.aRow[0].lpProps[2].Value.ul = ulRecipType; /* Inherit from parent group */

			sRowSMTProwSet.aRow[0].lpProps[3].ulPropTag = PR_DISPLAY_NAME_W;
			sRowSMTProwSet.aRow[0].lpProps[3].Value.lpszW = lpDisplayName->Value.lpszW;

			hr = lpMessage->ModifyRecipients(MODRECIP_ADD, (LPADRLIST)&sRowSMTProwSet);
			if (hr != hrSuccess) {
				g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Unable to add e-mail address of %ls from group", lpEmailAddress->Value.lpszW);
				continue;
			}
		} else {
			SBinary sEntryId;

			/* If we should recur further, just remove the group from the recipients list */
			if (!recurrence)
				goto remove_group;

			/* Only continue when this group has not yet been expanded previously */
			if (find(lpExpandedGroups->begin(), lpExpandedGroups->end(), lpEntryId->Value.bin) != lpExpandedGroups->end())
				goto remove_group;

			hr = lpAddrBook->OpenEntry(lpEntryId->Value.bin.cb, (LPENTRYID)lpEntryId->Value.bin.lpb, NULL, 0, &ulObj, (LPUNKNOWN*)&lpDistlist);
			if (hr != hrSuccess)
				continue;
				
			if(ulObj != MAPI_DISTLIST)
				continue;
				
			/* Never expand groups with an email address. The whole point of the email address is that it can be used
			 * as a single entity */
			if(HrGetOneProp(lpDistlist, PR_SMTP_ADDRESS_W, &lpSMTPAddress) == hrSuccess && wcslen(lpSMTPAddress->Value.lpszW) > 0)
				continue;

			hr = lpDistlist->GetContentsTable(MAPI_UNICODE, &lpContentsTable);
			if (hr != hrSuccess)
				continue;

			hr = lpContentsTable->Restrict(lpEntryRestriction, 0);
			if (hr != hrSuccess)
				continue;

			/* Group has been expanded (because we successfully have the contents table) time
			 * to add it to our expanded group list. This has to be done or at least before the
			 * recursive call to ExpandRecipientsRecursive().*/
			hr = Util::HrCopyEntryId(lpEntryId->Value.bin.cb, (LPENTRYID)lpEntryId->Value.bin.lpb,
									 &sEntryId.cb, (LPENTRYID*)&sEntryId.lpb);
			lpExpandedGroups->push_back(sEntryId);

			/* Don't expand group Everyone or companies since both already contain all users
			 * which should be put in the recipient list. */
			bExpandSub = !(((lpDisplayType) ? lpDisplayType->Value.ul == DT_ORGANIZATION : false) ||
						   ((lpDisplayName) ? wcscasecmp(lpDisplayName->Value.lpszW, L"Everyone") == 0 : false));
			// @todo find everyone using it's static entryid?

			/* Start/Continue recursion */
			hr = ExpandRecipientsRecursive(lpAddrBook, lpMessage, lpContentsTable,
										   lpEntryRestriction, ulRecipType, lpExpandedGroups, bExpandSub);
			/* Ignore errors */

remove_group:
			/* Only delete row when the rowid is present */
			if (!lpRowId)
				continue;

			hr = lpMessage->ModifyRecipients(MODRECIP_REMOVE, (LPADRLIST)lpsRowSet);
			if (hr != hrSuccess) {
				g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Unable to remove group %ls from recipient list.", lpDisplayName->Value.lpszW);
				continue;
			}
		}
	}

exit:
	if (lpSMTPAddress)
		MAPIFreeBuffer(lpSMTPAddress);
		
	if (lpDistlist)
		lpDistlist->Release();

	if (lpsRowSet)
		FreeProws(lpsRowSet);

	if (lpContentsTable)
		lpContentsTable->Release();

	return hr;
}

/**
 * Expands groups in normal recipients.
 *
 * This function builds the restriction, and calls the recursion
 * function, since we can have group-in-groups.
 *
 * @todo use restriction macro's for readability.
 *
 * @param[in]	lpAddrBook	The Global Addressbook of the user sending lpMessage
 * @param[in]	lpMessage	The message to expand groups for.
 * @return		HRESULT
 */
HRESULT ExpandRecipients(LPADRBOOK lpAddrBook, IMessage *lpMessage)
{
	HRESULT hr = hrSuccess;
	list<SBinary> lExpandedGroups;
	list<SBinary>::iterator iterGroups;
	IMAPITable *lpTable = NULL;
	LPSRestriction lpRestriction = NULL;
	LPSRestriction lpEntryRestriction = NULL;

	/*
	 * Setup group restriction:
	 * PR_OBJECT_TYPE == MAPI_DISTLIST && PR_ADDR_TYPE == "ZARAFA"
	 */
	hr = MAPIAllocateBuffer(sizeof(SRestriction), (LPVOID*)&lpRestriction);
	if(hr != hrSuccess)
		goto exit;

	hr = MAPIAllocateMore(sizeof(SRestriction) * 2, lpRestriction, (LPVOID*)&lpRestriction->res.resAnd.lpRes);
	if (hr != hrSuccess)
		goto exit;

	lpRestriction->rt = RES_AND;
	lpRestriction->res.resAnd.cRes = 2;

	hr = MAPIAllocateMore(sizeof(SPropValue), lpRestriction, (LPVOID*)&lpRestriction->res.resAnd.lpRes[0].res.resProperty.lpProp);
	if (hr != hrSuccess)
		goto exit;

	lpRestriction->res.resAnd.lpRes[0].rt = RES_PROPERTY;
	lpRestriction->res.resAnd.lpRes[0].res.resProperty.relop = RELOP_EQ;
	lpRestriction->res.resAnd.lpRes[0].res.resProperty.ulPropTag = PR_OBJECT_TYPE;
	lpRestriction->res.resAnd.lpRes[0].res.resProperty.lpProp->ulPropTag = PR_OBJECT_TYPE;
	lpRestriction->res.resAnd.lpRes[0].res.resProperty.lpProp->Value.ul = MAPI_DISTLIST;

	hr = MAPIAllocateMore(sizeof(SPropValue), lpRestriction, (LPVOID*)&lpRestriction->res.resAnd.lpRes[1].res.resProperty.lpProp);
	if (hr != hrSuccess)
		goto exit;

	lpRestriction->res.resAnd.lpRes[1].rt = RES_PROPERTY;
	lpRestriction->res.resAnd.lpRes[1].res.resProperty.relop = RELOP_EQ;
	lpRestriction->res.resAnd.lpRes[1].res.resProperty.ulPropTag = PR_ADDRTYPE_W;
	lpRestriction->res.resAnd.lpRes[1].res.resProperty.lpProp->ulPropTag = PR_ADDRTYPE_W;
	lpRestriction->res.resAnd.lpRes[1].res.resProperty.lpProp->Value.lpszW = L"ZARAFA";

	/*
	 * Setup entry restriction:
	 * PR_ADDR_TYPE == "ZARAFA"
	 */
	hr = MAPIAllocateBuffer(sizeof(SRestriction), (LPVOID*)&lpEntryRestriction);
	if(hr != hrSuccess)
		goto exit;

	hr = MAPIAllocateMore(sizeof(SPropValue), lpEntryRestriction, (LPVOID*)&lpEntryRestriction->res.resProperty.lpProp);
	if (hr != hrSuccess)
		goto exit;

	lpEntryRestriction->rt = RES_PROPERTY;
	lpEntryRestriction->res.resProperty.relop = RELOP_EQ;
	lpEntryRestriction->res.resProperty.ulPropTag = PR_ADDRTYPE_W;
	lpEntryRestriction->res.resProperty.lpProp->ulPropTag = PR_ADDRTYPE_W;
	lpEntryRestriction->res.resProperty.lpProp->Value.lpszW = L"ZARAFA";

	hr = lpMessage->GetRecipientTable(MAPI_UNICODE, &lpTable);
	if(hr != hrSuccess)
		goto exit;

	/* The first table we send with ExpandRecipientsRecursive() is the RecipientTable itself,
	 * we need to put a restriction on this table since the first time only the groups
	 * should be added to the recipients list. Subsequent calls to ExpandRecipientsRecursive()
	 * will send the group member table and will correct add the members to the recipients
	 * table. */
	hr = lpTable->Restrict(lpRestriction, 0);
	if (hr != hrSuccess)
		goto exit;

	/* ExpandRecipientsRecursive() will run recursively expanding each group
	 * it finds including all subgroups. It will use the lExpandedGroups list
	 * to protect itself for circular subgroup membership */
	hr = ExpandRecipientsRecursive(lpAddrBook, lpMessage, lpTable, lpEntryRestriction, MAPI_TO, &lExpandedGroups); 
	if (hr != hrSuccess)
		goto exit;

exit:
	for (iterGroups = lExpandedGroups.begin(); iterGroups != lExpandedGroups.end(); iterGroups++)
		MAPIFreeBuffer(iterGroups->lpb);

	if (lpTable)
		lpTable->Release();

	if (lpRestriction)
		MAPIFreeBuffer(lpRestriction);

	if (lpEntryRestriction)
		MAPIFreeBuffer(lpEntryRestriction);

	return hr;
}

/**
 * Rewrites a FAX:number "email address" to a sendable email address.
 *
 * @param[in]	lpMAPISession	The session of the user
 * @param[in]	lpMessage		The message to send
 * @return		HRESULT
 */
HRESULT RewriteRecipients(LPMAPISESSION lpMAPISession, IMessage *lpMessage)
{
	HRESULT			hr = hrSuccess;
	IMAPITable		*lpTable = NULL;
	LPSRowSet		lpRowSet = NULL;
	LPSPropTagArray	lpRecipColumns = NULL;
	LPSPropValue	lpEmailAddress = NULL;
	LPSPropValue	lpEmailName = NULL;
	LPSPropValue	lpAddrType = NULL;
	LPSPropValue	lpEntryID = NULL;
	ULONG			cbNewEntryID;
	LPENTRYID		lpNewEntryID = NULL;

	char			*lpszFaxDomain = g_lpConfig->GetSetting("fax_domain");
	char			*lpszFaxInternational = g_lpConfig->GetSetting("fax_international");
	string			strFaxMail;
	wstring			wstrFaxMail, wstrOldFaxMail;
	LPMAILUSER		lpFaxMailuser = NULL;
	LPSPropValue	lpFaxNumbers = NULL;
	ULONG			ulObjType;
	ULONG			cValues;

	// contab email_offset: 0: business, 1: home, 2: primary (outlook uses string 'other')
	SizedSPropTagArray(3, sptaFaxNumbers) = { 3, {PR_BUSINESS_FAX_NUMBER_A, PR_HOME_FAX_NUMBER_A, PR_PRIMARY_FAX_NUMBER_A } };

	if (!lpszFaxDomain || strcmp(lpszFaxDomain, "") == 0)
		goto exit;

	hr = lpMessage->GetRecipientTable(MAPI_UNICODE, &lpTable);
	if (hr != hrSuccess)
		goto exit;

	// we need all columns when rewriting FAX to SMTP
	hr = lpTable->QueryColumns(TBL_ALL_COLUMNS, &lpRecipColumns);
	if (hr != hrSuccess)
		goto exit;
	
	hr = lpTable->SetColumns(lpRecipColumns, 0);
	if (hr != hrSuccess)
		goto exit;

	while (TRUE) {
		if (lpRowSet) {
			FreeProws(lpRowSet);
			lpRowSet = NULL;
		}

		hr = lpTable->QueryRows(1, 0, &lpRowSet);
		if (hr != hrSuccess)
			goto exit;

		if (lpRowSet->cRows == 0)
			break;

		lpEmailAddress = PpropFindProp(lpRowSet->aRow[0].lpProps, lpRowSet->aRow[0].cValues, PR_EMAIL_ADDRESS_W);
		lpEmailName = PpropFindProp(lpRowSet->aRow[0].lpProps, lpRowSet->aRow[0].cValues, PR_DISPLAY_NAME_W);
		lpAddrType = PpropFindProp(lpRowSet->aRow[0].lpProps, lpRowSet->aRow[0].cValues, PR_ADDRTYPE_W);
		lpEntryID = PpropFindProp(lpRowSet->aRow[0].lpProps, lpRowSet->aRow[0].cValues, PR_ENTRYID);

		if (!(lpEmailAddress && lpAddrType && lpEntryID && lpEmailName))
			continue;

		if (wcscmp(lpAddrType->Value.lpszW, L"FAX") == 0)
		{
			// rewrite FAX address to <number>@<faxdomain>
			wstring wstrName, wstrType, wstrEmailAddress;

			if (ECParseOneOff((LPENTRYID)lpEntryID->Value.bin.lpb, lpEntryID->Value.bin.cb, wstrName, wstrType, wstrEmailAddress) == hrSuccess) {
				// user entered manual fax address
				strFaxMail = convert_to<string>(wstrEmailAddress);
			} else {
				// check if entry is in contacts folder
				LPCONTAB_ENTRYID lpContabEntryID = (LPCONTAB_ENTRYID)lpEntryID->Value.bin.lpb;
				GUID* guid = (GUID*)&lpContabEntryID->muid;

				// check validity of lpContabEntryID
				if (sizeof(CONTAB_ENTRYID) > lpEntryID->Value.bin.cb ||
					*guid != PSETID_CONTACT_FOLDER_RECIPIENT ||
					lpContabEntryID->email_offset < 3 ||
					lpContabEntryID->email_offset > 5)
				{
					hr = MAPI_E_INVALID_PARAMETER;
					g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Unable to convert FAX recipient, using %ls", lpEmailAddress->Value.lpszW);
					goto nextfax;
				}

				// 0..2 == reply to email offsets
				// 3..5 == fax email offsets
				lpContabEntryID->email_offset -= 3;

				hr = lpMAPISession->OpenEntry(lpContabEntryID->cbeid, (LPENTRYID)lpContabEntryID->abeid, NULL, 0, &ulObjType, (LPUNKNOWN*)&lpFaxMailuser);
				if (hr != hrSuccess) {
					g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Unable to convert FAX recipient, using %ls", lpEmailAddress->Value.lpszW);
					goto nextfax;
				}

				hr = lpFaxMailuser->GetProps((LPSPropTagArray)&sptaFaxNumbers, 0, &cValues, &lpFaxNumbers);
				if (FAILED(hr)) {
					g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Unable to convert FAX recipient, using %ls", lpEmailAddress->Value.lpszW);
					goto nextfax;
				}

				if (lpFaxNumbers[lpContabEntryID->email_offset].ulPropTag != sptaFaxNumbers.aulPropTag[lpContabEntryID->email_offset]) {
					g_lpLogger->Log(EC_LOGLEVEL_ERROR, "No suitable FAX number found, using %ls", lpEmailAddress->Value.lpszW);
					goto nextfax;
				}

				strFaxMail = lpFaxNumbers[lpContabEntryID->email_offset].Value.lpszA;
			}

			strFaxMail += string("@") + lpszFaxDomain;
			if (strFaxMail[0] == '+' && lpszFaxInternational) {
				strFaxMail = lpszFaxInternational + strFaxMail.substr(1, strFaxMail.length());
			}

			wstrFaxMail = convert_to<wstring>(strFaxMail);
			wstrOldFaxMail = lpEmailAddress->Value.lpszW; // keep old string for logging
			// hack values in lpRowSet
			lpEmailAddress->Value.lpszW = (WCHAR*)wstrFaxMail.c_str();
			lpAddrType->Value.lpszW = L"SMTP";
			// old value is stuck to the row allocation, so we can override it, but we also must free the new!
			ECCreateOneOff((LPTSTR)lpEmailName->Value.lpszW, (LPTSTR)L"SMTP", (LPTSTR)wstrFaxMail.c_str(), MAPI_UNICODE, &cbNewEntryID, &lpNewEntryID);
			lpEntryID->Value.bin.lpb = (LPBYTE)lpNewEntryID;
			lpEntryID->Value.bin.cb = cbNewEntryID;

			hr = lpMessage->ModifyRecipients(MODRECIP_MODIFY, (LPADRLIST)lpRowSet);
			if (hr != hrSuccess) {
				g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Unable to set new FAX mail address for '%ls' to '%s'", wstrOldFaxMail.c_str(), strFaxMail.c_str());
				goto nextfax;
			}

			g_lpLogger->Log(EC_LOGLEVEL_INFO, "Using new FAX mail address %s", strFaxMail.c_str());

nextfax:
			hr = hrSuccess;

			if (lpNewEntryID)
				MAPIFreeBuffer(lpNewEntryID);
			lpNewEntryID = NULL;

			if (lpFaxMailuser)
				lpFaxMailuser->Release();
			lpFaxMailuser = NULL;

			if (lpFaxNumbers)
				MAPIFreeBuffer(lpFaxNumbers);
			lpFaxNumbers = NULL;
		}
	}

exit:
	if (lpNewEntryID)
		MAPIFreeBuffer(lpNewEntryID);

	if (lpFaxMailuser)
		lpFaxMailuser->Release();

	if (lpFaxNumbers)
		MAPIFreeBuffer(lpFaxNumbers);

	if (lpRecipColumns)
		MAPIFreeBuffer(lpRecipColumns);

	if (lpRowSet)
		FreeProws(lpRowSet);

	if (lpTable)
		lpTable->Release();

	return hr;
}

/**
 * Make the recipient table in the message unique. Key is the PR_SMTP_ADDRESS and PR_RECIPIENT_TYPE (To/Cc/Bcc).
 *
 * @param[in]	lpMessage	The message to fix the recipient table for.
 * @return		HRESULT
 */
HRESULT UniqueRecipients(IMessage *lpMessage)
{
	HRESULT			hr = hrSuccess;
	IMAPITable		*lpTable = NULL;
	LPSRowSet		lpRowSet = NULL;
	LPSPropValue	lpEmailAddress = NULL;
	LPSPropValue	lpRecipType = NULL;
	string			strEmail;
	ULONG			ulRecipType = 0;

	SizedSPropTagArray(3, sptaColumns) = {
		3, {
			PR_ROWID,
			PR_SMTP_ADDRESS_A,
			PR_RECIPIENT_TYPE,
		}
	};

	SizedSSortOrderSet(2, sosOrder) = {
		2, 0, 0, {
			{ PR_SMTP_ADDRESS_A, TABLE_SORT_ASCEND },
			{ PR_RECIPIENT_TYPE, TABLE_SORT_ASCEND },
		}
	};

	hr = lpMessage->GetRecipientTable(0, &lpTable);
	if (hr != hrSuccess)
		goto exit;

	hr = lpTable->SetColumns((LPSPropTagArray)&sptaColumns, 0);
	if (hr != hrSuccess)
		goto exit;

	hr = lpTable->SortTable((LPSSortOrderSet)&sosOrder, 0);
	if (hr != hrSuccess)
		goto exit;

	while (TRUE) {
		if (lpRowSet) {
			FreeProws(lpRowSet);
			lpRowSet = NULL;
		}

		hr = lpTable->QueryRows(1, 0, &lpRowSet);
		if (hr != hrSuccess)
			goto exit;

		if (lpRowSet->cRows == 0)
			break;

		lpEmailAddress = PpropFindProp(lpRowSet->aRow[0].lpProps, lpRowSet->aRow[0].cValues, PR_SMTP_ADDRESS_A);
		lpRecipType = PpropFindProp(lpRowSet->aRow[0].lpProps, lpRowSet->aRow[0].cValues, PR_RECIPIENT_TYPE);

		if (!lpEmailAddress || !lpRecipType)
			continue;

		/* Filter To, Cc, Bcc individually */
		if (strEmail == lpEmailAddress->Value.lpszA && ulRecipType == lpRecipType->Value.ul) {
			hr = lpMessage->ModifyRecipients(MODRECIP_REMOVE, (LPADRLIST)lpRowSet);
			if (hr != hrSuccess)
				g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Failed to remove duplicate entry");
		} else {
			strEmail = string(lpEmailAddress->Value.lpszA);
			ulRecipType = lpRecipType->Value.ul;
		}
	}

exit:
	if (lpRowSet)
		FreeProws(lpRowSet);

	if (lpTable)
		lpTable->Release();

	return hr;
}

/**
 * Removes all MAPI_P1 marked recipients from a message.
 *
 * @param[in]	lpMessage	Message to remove MAPI_P1 recipients from
 * @return		HRESULT
 */
HRESULT RemoveP1Recipients(IMessage *lpMessage)
{
	HRESULT hr = hrSuccess;
	IMAPITable *lpTable = NULL;
	LPSRowSet lpRows = NULL;
	SRestriction sRestriction;
	SPropValue sPropRestrict;
	
	sPropRestrict.ulPropTag = PR_RECIPIENT_TYPE;
	sPropRestrict.Value.ul = MAPI_P1;
	
	sRestriction.rt = RES_PROPERTY;
	sRestriction.res.resProperty.relop = RELOP_EQ;
	sRestriction.res.resProperty.ulPropTag = PR_RECIPIENT_TYPE;
	sRestriction.res.resProperty.lpProp = &sPropRestrict;
	
	hr = lpMessage->GetRecipientTable(0, &lpTable);
	if(hr != hrSuccess)
		goto exit;
		
	hr = lpTable->Restrict(&sRestriction, 0);
	if(hr != hrSuccess)
		goto exit;
		
	hr = lpTable->QueryRows(-1, 0, &lpRows);
	if(hr != hrSuccess)
		goto exit;
		
	hr = lpMessage->ModifyRecipients(MODRECIP_REMOVE, (LPADRLIST)lpRows);
	if(hr != hrSuccess)
		goto exit;
	
exit:
	if(lpRows)
		FreeProws(lpRows);
		
	if(lpTable)
		lpTable->Release();
		
	return hr;
}

/**
 * Creates an MDN message in the inbox of the given store for the passed message.
 *
 * This creates an MDN message in the inbox of the store passed, setting the correct properties and recipients. The most
 * important part of this function is to report errors of why sending failed. Sending can fail due to an overall problem
 * (when the entire message could not be sent) or when only some recipient didn't receive the message.
 *
 * In the case of partial failure (some recipients did not receive the email), the MDN message is populated with a recipient
 * table for all the recipients that failed. An error is attached to each of these recipients. The error information is
 * retrieved from the passed lpMailer object.
 *
 * @param lpAddrBook Pointer to addressbook object
 * @param lpMailer Mailer object used to send the lpMessage message containing the errors
 * @param lpUserAdmin User object which specifies the 'from' address of the MDN message to be created
 * @param lpMessage Failed message
 */
HRESULT SendUndeliverable(LPADRBOOK lpAddrBook, ECSender *lpMailer, LPMDB lpStore, LPECUSER lpUserAdmin, LPMESSAGE lpMessage)
{
	HRESULT			hr;
	LPMAPIFOLDER	lpInbox = NULL;
	LPMESSAGE		lpErrorMsg = NULL;
	LPENTRYID		lpEntryID = NULL;
	ULONG			cbEntryID;
	ULONG			ulObjType;
	wstring			newbody;
	LPSPropValue	lpPropValue = NULL;
	unsigned int	ulPropPos = 0;
	FILETIME		ft;
	LPENTRYID		lpEntryIdSender	= NULL;
	ULONG			cbEntryIdSender;
	LPATTACH		lpAttach = NULL;
	LPMESSAGE		lpOriginalMessage = NULL;
	ULONG			cValuesOriginal = 0;
	LPSPropValue	lpPropArrayOriginal = NULL;
	LPSPropValue	lpPropValueAttach = NULL;
	unsigned int	ulPropModsPos;
	LPADRLIST		lpMods = NULL;
	IMAPITable*		lpTableMods = NULL;
	LPSRowSet		lpRows = NULL;
	ULONG			ulRows = 0;
	ULONG			cEntries = 0;
	string			strName, strType, strEmail;

	// CopyTo() var's
	unsigned int	ulPropAttachPos;
	ULONG			ulAttachNum;

	enum eORPos {
		OR_DISPLAY_TO, OR_DISPLAY_CC, OR_DISPLAY_BCC, OR_SEARCH_KEY, OR_SENDER_ADDRTYPE,
		OR_SENDER_EMAIL_ADDRESS, OR_SENDER_ENTRYID, OR_SENDER_NAME,
		OR_SENDER_SEARCH_KEY, OR_SENT_REPRESENTING_ADDRTYPE,
		OR_SENT_REPRESENTING_EMAIL_ADDRESS, OR_SENT_REPRESENTING_ENTRYID,
		OR_SENT_REPRESENTING_NAME, OR_SENT_REPRESENTING_SEARCH_KEY,
		OR_SUBJECT, OR_CLIENT_SUBMIT_TIME
	};

	// These props are on purpose without _A and _W
	SizedSPropTagArray(16, sPropsOriginal) = {
		16,
		{ PR_DISPLAY_TO, PR_DISPLAY_CC,
		  PR_DISPLAY_BCC, PR_SEARCH_KEY,
		  PR_SENDER_ADDRTYPE_W, PR_SENDER_EMAIL_ADDRESS,
		  PR_SENDER_ENTRYID, PR_SENDER_NAME,
		  PR_SENDER_SEARCH_KEY, PR_SENT_REPRESENTING_ADDRTYPE,
		  PR_SENT_REPRESENTING_EMAIL_ADDRESS, PR_SENT_REPRESENTING_ENTRYID,
		  PR_SENT_REPRESENTING_NAME, PR_SENT_REPRESENTING_SEARCH_KEY,
		  PR_SUBJECT_W, PR_CLIENT_SUBMIT_TIME }
	};

	SizedSPropTagArray(7, sPropTagRecipient) = {
		7,
		{ PR_RECIPIENT_TYPE, PR_DISPLAY_NAME, PR_DISPLAY_TYPE,
		  PR_ADDRTYPE, PR_EMAIL_ADDRESS,
		  PR_ENTRYID, PR_SEARCH_KEY }
	};

	// open inbox
	hr = lpStore->GetReceiveFolder((LPTSTR)"IPM", 0, &cbEntryID, &lpEntryID, NULL);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_WARNING, "Unable to resolve incoming folder, error code: 0x%08X", hr);
		goto exit;
	}
	hr = lpStore->OpenEntry(cbEntryID, lpEntryID, &IID_IMAPIFolder, MAPI_MODIFY, &ulObjType, (LPUNKNOWN *)&lpInbox);
	if (hr != hrSuccess || ulObjType != MAPI_FOLDER) {
		g_lpLogger->Log(EC_LOGLEVEL_WARNING, "Unable to open inbox folder, error code: 0x%08X", hr);
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}

	// make new message in inbox
	hr = lpInbox->CreateMessage(NULL, 0, &lpErrorMsg);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_WARNING, "Unable to create undeliverable message, error code: 0x%08X", hr);
		goto exit;
	}

	// Get properties from the orinal message
	hr = lpMessage->GetProps((LPSPropTagArray)&sPropsOriginal, 0, &cValuesOriginal, &lpPropArrayOriginal);
	if (FAILED(hr))
		goto exit;

	// set props, 50 is a wild guess, more than enough
	hr = MAPIAllocateBuffer(sizeof(SPropValue) * 50, (void **)&lpPropValue);
	if(hr != hrSuccess)
		goto exit;

	// Subject
	lpPropValue[ulPropPos].ulPropTag = PR_SUBJECT_W;
	lpPropValue[ulPropPos++].Value.lpszW = L"Undelivered Mail Returned to Sender";

	// Message flags
	lpPropValue[ulPropPos].ulPropTag = PR_MESSAGE_FLAGS;
	lpPropValue[ulPropPos++].Value.ul = 0;

	// Message class
	lpPropValue[ulPropPos].ulPropTag = PR_MESSAGE_CLASS_W;
	lpPropValue[ulPropPos++].Value.lpszW = L"REPORT.IPM.Note.NDR";

	// Get the time to add to the message as PR_CLIENT_SUBMIT_TIME
	GetSystemTimeAsFileTime(&ft);

	// Submit time
	lpPropValue[ulPropPos].ulPropTag = PR_CLIENT_SUBMIT_TIME;
	lpPropValue[ulPropPos++].Value.ft = ft;

	// Delivery time
	lpPropValue[ulPropPos].ulPropTag = PR_MESSAGE_DELIVERY_TIME;
	lpPropValue[ulPropPos++].Value.ft = ft;

	// FROM: set the properties PR_SENT_REPRESENTING_* and PR_SENDER_*
	if(lpUserAdmin->lpszFullName) {
		lpPropValue[ulPropPos].ulPropTag = PR_SENDER_NAME_W;
		lpPropValue[ulPropPos++].Value.lpszW = (LPWSTR)lpUserAdmin->lpszFullName;

		lpPropValue[ulPropPos].ulPropTag = PR_SENT_REPRESENTING_NAME_W;
		lpPropValue[ulPropPos++].Value.lpszW = (LPWSTR)lpUserAdmin->lpszFullName;
	}

	if(lpUserAdmin->lpszMailAddress) {
		std::string strMailAddress;

		// PR_SENDER_EMAIL_ADDRESS
		lpPropValue[ulPropPos].ulPropTag = PR_SENDER_EMAIL_ADDRESS_W;
		lpPropValue[ulPropPos++].Value.lpszW = (LPWSTR)lpUserAdmin->lpszMailAddress;

		// PR_SENT_REPRESENTING_EMAIL_ADDRESS
		lpPropValue[ulPropPos].ulPropTag = PR_SENT_REPRESENTING_EMAIL_ADDRESS_W;
		lpPropValue[ulPropPos++].Value.lpszW = (LPWSTR)lpUserAdmin->lpszMailAddress;

		// PR_SENDER_ADDRTYPE
		lpPropValue[ulPropPos].ulPropTag = PR_SENDER_ADDRTYPE_W;
		lpPropValue[ulPropPos++].Value.lpszW = L"SMTP";

		// PR_SENT_REPRESENTING_ADDRTYPE
		lpPropValue[ulPropPos].ulPropTag = PR_SENT_REPRESENTING_ADDRTYPE_W;
		lpPropValue[ulPropPos++].Value.lpszW = L"SMTP";

		// PR_SENDER_SEARCH_KEY
		strMailAddress = convert_to<std::string>(lpUserAdmin->lpszMailAddress);
		transform(strMailAddress.begin(), strMailAddress.end(), strMailAddress.begin(), ::toupper);

		lpPropValue[ulPropPos].ulPropTag = PR_SENDER_SEARCH_KEY;
		lpPropValue[ulPropPos].Value.bin.cb = 5 + strMailAddress.length(); // 5 = "SMTP:"

		MAPIAllocateMore(lpPropValue[ulPropPos].Value.bin.cb, lpPropValue, (void**)&lpPropValue[ulPropPos].Value.bin.lpb);
		memcpy(lpPropValue[ulPropPos].Value.bin.lpb, "SMTP:", 5);
		if(lpPropValue[ulPropPos].Value.bin.cb > 5)
			memcpy(lpPropValue[ulPropPos].Value.bin.lpb+5, strMailAddress.data(), lpPropValue[ulPropPos].Value.bin.cb - 5);
		ulPropPos++;

		// PR_SENT_REPRESENTING_SEARCH_KEY
		lpPropValue[ulPropPos].ulPropTag = PR_SENT_REPRESENTING_SEARCH_KEY;

		lpPropValue[ulPropPos].Value.bin.cb = 5 + strMailAddress.length(); // 5 = "SMTP:"

		MAPIAllocateMore(lpPropValue[ulPropPos].Value.bin.cb, lpPropValue, (void**)&lpPropValue[ulPropPos].Value.bin.lpb);
		memcpy(lpPropValue[ulPropPos].Value.bin.lpb, "SMTP:", 5);
		if(lpPropValue[ulPropPos].Value.bin.cb > 5)
			memcpy(lpPropValue[ulPropPos].Value.bin.lpb+5, strMailAddress.data(), lpPropValue[ulPropPos].Value.bin.cb - 5);
		ulPropPos++;

	}

	hr = ECCreateOneOff((LPTSTR)lpUserAdmin->lpszFullName, (LPTSTR)L"SMTP", (LPTSTR)lpUserAdmin->lpszMailAddress,
						MAPI_SEND_NO_RICH_INFO | MAPI_UNICODE, &cbEntryIdSender, &lpEntryIdSender);
	if(hr == hrSuccess)	{
		// PR_SENDER_ENTRYID
		lpPropValue[ulPropPos].ulPropTag		= PR_SENDER_ENTRYID;
		lpPropValue[ulPropPos].Value.bin.cb		= cbEntryIdSender;
		lpPropValue[ulPropPos++].Value.bin.lpb	= (LPBYTE)lpEntryIdSender;

		// PR_SENT_REPRESENTING_ENTRYID
		lpPropValue[ulPropPos].ulPropTag		= PR_SENT_REPRESENTING_ENTRYID;
		lpPropValue[ulPropPos].Value.bin.cb		= cbEntryIdSender;
		lpPropValue[ulPropPos++].Value.bin.lpb	= (LPBYTE)lpEntryIdSender;
	}else {
		hr = hrSuccess;
	}

	// Although lpszA is used, we just copy pointers. By not forcing _A or _W, this works in unicode and normal compile mode.

	// Set the properties PR_RCVD_REPRESENTING_* and PR_RECEIVED_BY_* and
	// PR_ORIGINAL_SENDER_* and PR_ORIGINAL_SENT_*

	if(PROP_TYPE(lpPropArrayOriginal[OR_SENDER_NAME].ulPropTag) != PT_ERROR) {
		lpPropValue[ulPropPos].ulPropTag = PR_RECEIVED_BY_NAME;
		lpPropValue[ulPropPos++].Value.lpszA = lpPropArrayOriginal[OR_SENDER_NAME].Value.lpszA;

		lpPropValue[ulPropPos].ulPropTag = PR_ORIGINAL_SENDER_NAME;
		lpPropValue[ulPropPos++].Value.lpszA = lpPropArrayOriginal[OR_SENDER_NAME].Value.lpszA;
	}

	if(PROP_TYPE(lpPropArrayOriginal[OR_SENDER_EMAIL_ADDRESS].ulPropTag) != PT_ERROR) {
		lpPropValue[ulPropPos].ulPropTag = PR_RECEIVED_BY_EMAIL_ADDRESS;
		lpPropValue[ulPropPos++].Value.lpszA = lpPropArrayOriginal[OR_SENDER_EMAIL_ADDRESS].Value.lpszA;

		lpPropValue[ulPropPos].ulPropTag = PR_ORIGINAL_SENDER_EMAIL_ADDRESS;
		lpPropValue[ulPropPos++].Value.lpszA = lpPropArrayOriginal[OR_SENDER_EMAIL_ADDRESS].Value.lpszA;
	}

	if(PROP_TYPE(lpPropArrayOriginal[OR_SENDER_ADDRTYPE].ulPropTag) != PT_ERROR) {
		lpPropValue[ulPropPos].ulPropTag = PR_RECEIVED_BY_ADDRTYPE;
		lpPropValue[ulPropPos++].Value.lpszA = lpPropArrayOriginal[OR_SENDER_ADDRTYPE].Value.lpszA;

		lpPropValue[ulPropPos].ulPropTag = PR_ORIGINAL_SENDER_ADDRTYPE;
		lpPropValue[ulPropPos++].Value.lpszA = lpPropArrayOriginal[OR_SENDER_ADDRTYPE].Value.lpszA;
	}

	if(PROP_TYPE(lpPropArrayOriginal[OR_SENDER_SEARCH_KEY].ulPropTag) != PT_ERROR) {
		lpPropValue[ulPropPos].ulPropTag = PR_RECEIVED_BY_SEARCH_KEY;
		lpPropValue[ulPropPos].Value.bin.cb = lpPropArrayOriginal[OR_SENDER_SEARCH_KEY].Value.bin.cb;
		lpPropValue[ulPropPos++].Value.bin.lpb = lpPropArrayOriginal[OR_SENDER_SEARCH_KEY].Value.bin.lpb;

		lpPropValue[ulPropPos].ulPropTag = PR_ORIGINAL_SENDER_SEARCH_KEY;
		lpPropValue[ulPropPos].Value.bin.cb = lpPropArrayOriginal[OR_SENDER_SEARCH_KEY].Value.bin.cb;
		lpPropValue[ulPropPos++].Value.bin.lpb = lpPropArrayOriginal[OR_SENDER_SEARCH_KEY].Value.bin.lpb;
	}

	if(PROP_TYPE(lpPropArrayOriginal[OR_SENDER_ENTRYID].ulPropTag) != PT_ERROR) {
		lpPropValue[ulPropPos].ulPropTag		= PR_RECEIVED_BY_ENTRYID;
		lpPropValue[ulPropPos].Value.bin.cb		= lpPropArrayOriginal[OR_SENDER_ENTRYID].Value.bin.cb;
		lpPropValue[ulPropPos++].Value.bin.lpb	= lpPropArrayOriginal[OR_SENDER_ENTRYID].Value.bin.lpb;

		lpPropValue[ulPropPos].ulPropTag		= PR_ORIGINAL_SENDER_ENTRYID;
		lpPropValue[ulPropPos].Value.bin.cb		= lpPropArrayOriginal[OR_SENDER_ENTRYID].Value.bin.cb;
		lpPropValue[ulPropPos++].Value.bin.lpb	= lpPropArrayOriginal[OR_SENDER_ENTRYID].Value.bin.lpb;
	}

	if(PROP_TYPE(lpPropArrayOriginal[OR_SENT_REPRESENTING_NAME].ulPropTag) != PT_ERROR) {
		lpPropValue[ulPropPos].ulPropTag = PR_RCVD_REPRESENTING_NAME;
		lpPropValue[ulPropPos++].Value.lpszA = lpPropArrayOriginal[OR_SENT_REPRESENTING_NAME].Value.lpszA;

		lpPropValue[ulPropPos].ulPropTag = PR_ORIGINAL_SENT_REPRESENTING_NAME;
		lpPropValue[ulPropPos++].Value.lpszA = lpPropArrayOriginal[OR_SENT_REPRESENTING_NAME].Value.lpszA;
	}

	if(PROP_TYPE(lpPropArrayOriginal[OR_SENT_REPRESENTING_EMAIL_ADDRESS].ulPropTag) != PT_ERROR) {
		lpPropValue[ulPropPos].ulPropTag = PR_RCVD_REPRESENTING_EMAIL_ADDRESS;
		lpPropValue[ulPropPos++].Value.lpszA = lpPropArrayOriginal[OR_SENT_REPRESENTING_EMAIL_ADDRESS].Value.lpszA;

		lpPropValue[ulPropPos].ulPropTag = PR_ORIGINAL_SENT_REPRESENTING_EMAIL_ADDRESS;
		lpPropValue[ulPropPos++].Value.lpszA = lpPropArrayOriginal[OR_SENT_REPRESENTING_EMAIL_ADDRESS].Value.lpszA;
	}

	if(PROP_TYPE(lpPropArrayOriginal[OR_SENT_REPRESENTING_ADDRTYPE].ulPropTag) != PT_ERROR) {
		lpPropValue[ulPropPos].ulPropTag = PR_RCVD_REPRESENTING_ADDRTYPE;
		lpPropValue[ulPropPos++].Value.lpszA = lpPropArrayOriginal[OR_SENT_REPRESENTING_ADDRTYPE].Value.lpszA;

		lpPropValue[ulPropPos].ulPropTag = PR_ORIGINAL_SENT_REPRESENTING_ADDRTYPE;
		lpPropValue[ulPropPos++].Value.lpszA = lpPropArrayOriginal[OR_SENT_REPRESENTING_ADDRTYPE].Value.lpszA;
	}

	if(PROP_TYPE(lpPropArrayOriginal[OR_SENT_REPRESENTING_SEARCH_KEY].ulPropTag) != PT_ERROR) {
		lpPropValue[ulPropPos].ulPropTag = PR_RCVD_REPRESENTING_SEARCH_KEY;
		lpPropValue[ulPropPos].Value.bin.cb = lpPropArrayOriginal[OR_SENT_REPRESENTING_SEARCH_KEY].Value.bin.cb;
		lpPropValue[ulPropPos++].Value.bin.lpb = lpPropArrayOriginal[OR_SENT_REPRESENTING_SEARCH_KEY].Value.bin.lpb;

		lpPropValue[ulPropPos].ulPropTag = PR_ORIGINAL_SENT_REPRESENTING_SEARCH_KEY;
		lpPropValue[ulPropPos].Value.bin.cb = lpPropArrayOriginal[OR_SENT_REPRESENTING_SEARCH_KEY].Value.bin.cb;
		lpPropValue[ulPropPos++].Value.bin.lpb = lpPropArrayOriginal[OR_SENT_REPRESENTING_SEARCH_KEY].Value.bin.lpb;
	}

	if(PROP_TYPE(lpPropArrayOriginal[OR_SENT_REPRESENTING_ENTRYID].ulPropTag) != PT_ERROR) {
		lpPropValue[ulPropPos].ulPropTag		= PR_RCVD_REPRESENTING_ENTRYID;
		lpPropValue[ulPropPos].Value.bin.cb		= lpPropArrayOriginal[OR_SENT_REPRESENTING_ENTRYID].Value.bin.cb;
		lpPropValue[ulPropPos++].Value.bin.lpb	= lpPropArrayOriginal[OR_SENT_REPRESENTING_ENTRYID].Value.bin.lpb;

		lpPropValue[ulPropPos].ulPropTag		= PR_ORIGINAL_SENT_REPRESENTING_ENTRYID;
		lpPropValue[ulPropPos].Value.bin.cb		= lpPropArrayOriginal[OR_SENT_REPRESENTING_ENTRYID].Value.bin.cb;
		lpPropValue[ulPropPos++].Value.bin.lpb	= lpPropArrayOriginal[OR_SENT_REPRESENTING_ENTRYID].Value.bin.lpb;
	}

	// Original display to
	if(PROP_TYPE(lpPropArrayOriginal[OR_DISPLAY_TO].ulPropTag) != PT_ERROR) {
		lpPropValue[ulPropPos].ulPropTag = PR_ORIGINAL_DISPLAY_TO;
		lpPropValue[ulPropPos++].Value.lpszA = lpPropArrayOriginal[OR_DISPLAY_TO].Value.lpszA;
	}

	// Original display cc
	if(PROP_TYPE(lpPropArrayOriginal[OR_DISPLAY_CC].ulPropTag) != PT_ERROR) {
		lpPropValue[ulPropPos].ulPropTag = PR_ORIGINAL_DISPLAY_CC;
		lpPropValue[ulPropPos++].Value.lpszA = lpPropArrayOriginal[OR_DISPLAY_CC].Value.lpszA;
	}

	// Original display bcc
	if(PROP_TYPE(lpPropArrayOriginal[OR_DISPLAY_BCC].ulPropTag) != PT_ERROR) {
		lpPropValue[ulPropPos].ulPropTag = PR_ORIGINAL_DISPLAY_BCC;
		lpPropValue[ulPropPos++].Value.lpszA = lpPropArrayOriginal[OR_DISPLAY_BCC].Value.lpszA;
	}

	// Original subject
	if(PROP_TYPE(lpPropArrayOriginal[OR_SUBJECT].ulPropTag) != PT_ERROR) {
		lpPropValue[ulPropPos].ulPropTag = PR_ORIGINAL_SUBJECT;
		lpPropValue[ulPropPos++].Value.lpszA = lpPropArrayOriginal[OR_SUBJECT].Value.lpszA;
	}

	// Original submit time
	if(PROP_TYPE(lpPropArrayOriginal[OR_CLIENT_SUBMIT_TIME].ulPropTag) != PT_ERROR) {
		lpPropValue[ulPropPos].ulPropTag = PR_ORIGINAL_SUBMIT_TIME;
		lpPropValue[ulPropPos++].Value.ft = lpPropArrayOriginal[OR_CLIENT_SUBMIT_TIME].Value.ft;
	}

	// Original searchkey
	if(PROP_TYPE(lpPropArrayOriginal[OR_SEARCH_KEY].ulPropTag) != PT_ERROR) {
		lpPropValue[ulPropPos].ulPropTag		= PR_ORIGINAL_SEARCH_KEY;
		lpPropValue[ulPropPos].Value.bin.cb		= lpPropArrayOriginal[OR_SEARCH_KEY].Value.bin.cb;
		lpPropValue[ulPropPos++].Value.bin.lpb	= lpPropArrayOriginal[OR_SEARCH_KEY].Value.bin.lpb;
	}


	// Add the orinal message into the errorMessage
	hr = lpErrorMsg->CreateAttach(NULL, 0, &ulAttachNum, &lpAttach);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_WARNING, "Unable to create attachment, error code: 0x%08X", hr);
		goto exit;
	}

	hr = lpAttach->OpenProperty(PR_ATTACH_DATA_OBJ, &IID_IMessage, 0, MAPI_CREATE | MAPI_MODIFY, (LPUNKNOWN *)&lpOriginalMessage);
	if (hr != hrSuccess)
		goto exit;

	hr = lpMessage->CopyTo(0, NULL, NULL, 0, NULL, &IID_IMessage, (LPVOID)lpOriginalMessage, 0, NULL);
	if (hr != hrSuccess)
		goto exit;

	// Remove MAPI_P1 recipients. These are present when you resend a resent message. They shouldn't be there since
	// we should be resending the original message
	hr = RemoveP1Recipients(lpOriginalMessage);
	if (hr != hrSuccess)
		goto exit;

	hr = lpOriginalMessage->SaveChanges(KEEP_OPEN_READWRITE);
	if (hr != hrSuccess)
		goto exit;

	ulPropAttachPos = 0;
	MAPIAllocateBuffer(sizeof(SPropValue) * 4, (void**)&lpPropValueAttach);

	lpPropValueAttach[ulPropAttachPos].ulPropTag = PR_ATTACH_METHOD;
	lpPropValueAttach[ulPropAttachPos++].Value.ul = ATTACH_EMBEDDED_MSG;

	lpPropValueAttach[ulPropAttachPos].ulPropTag = PR_ATTACH_MIME_TAG_W;
	lpPropValueAttach[ulPropAttachPos++].Value.lpszW = L"message/rfc822";

	if(PROP_TYPE(lpPropArrayOriginal[OR_SUBJECT].ulPropTag) != PT_ERROR) {
		lpPropValueAttach[ulPropAttachPos].ulPropTag = CHANGE_PROP_TYPE(PR_DISPLAY_NAME, PROP_TYPE(lpPropArrayOriginal[OR_SUBJECT].ulPropTag));
		lpPropValueAttach[ulPropAttachPos++].Value.lpszA = lpPropArrayOriginal[OR_SUBJECT].Value.lpszA;
	}

	lpPropValueAttach[ulPropAttachPos].ulPropTag = PR_RENDERING_POSITION;
	lpPropValueAttach[ulPropAttachPos++].Value.ul = -1;

	hr = lpAttach->SetProps(ulPropAttachPos, lpPropValueAttach, NULL);
	if (hr != hrSuccess)
		goto exit;

	hr = lpAttach->SaveChanges(KEEP_OPEN_READWRITE);
	if (hr != hrSuccess)
		goto exit;


	// add failed recipients to error report

	hr = lpMessage->GetRecipientTable(MAPI_UNICODE, &lpTableMods);
	if (hr != hrSuccess)
		goto exit;

	hr = lpTableMods->SetColumns((LPSPropTagArray)&sPropTagRecipient, TBL_BATCH);
	if (hr != hrSuccess)
		goto exit;

	hr = lpTableMods->GetRowCount(0, &ulRows);
	if (hr != hrSuccess)
		goto exit;

	if (ulRows == 0 || lpMailer->getRecipientErrorCount() == 0) {
		// No specific failed recipients, so the entire message failed
		
		// If there's a pr_body, outlook will display that, and not the 'default' outlook error report

		// Message error
		newbody = L"Unfortunately, I was unable to deliver your mail.\nThe error given was:\n\n";
		newbody.append(lpMailer->getErrorString());
		newbody.append(L"\n\nYou may need to contact your e-mail administrator to solve this problem.\n");

		lpPropValue[ulPropPos].ulPropTag = PR_BODY_W;
		lpPropValue[ulPropPos++].Value.lpszW = (WCHAR*)newbody.c_str();

		if (ulRows > 0) {
			// All recipients failed, therefore all recipient need to be in the MDN recipient table
			hr = lpTableMods->QueryRows(-1, 0, &lpRows);
			if (hr != hrSuccess)
				goto exit;

			hr = lpErrorMsg->ModifyRecipients(MODRECIP_ADD, (LPADRLIST)lpRows);
			if (hr != hrSuccess)
				goto exit;
		}
	}
	else if (ulRows > 0)
	{
		convert_context converter;
		
		// Only some recipients failed, so add only failed recipients to the MDN message. This causes
		// resends only to go to those recipients. This means we should add all error recipients to the
		// recipient list of the MDN message. 
		MAPIAllocateBuffer(CbNewADRLIST(lpMailer->getRecipientErrorCount()), (void**)&lpMods);
		lpMods->cEntries = 0;

		for (ULONG j = 0; j < lpMailer->getRecipientErrorCount(); j++)
		{
			MAPIAllocateBuffer(sizeof(SPropValue) * 10, (void**)&lpMods->aEntries[cEntries].rgPropVals);
			ulPropModsPos = 0;
			lpMods->cEntries = cEntries;

			lpMods->aEntries[cEntries].rgPropVals[ulPropModsPos].ulPropTag = PR_RECIPIENT_TYPE;
			lpMods->aEntries[cEntries].rgPropVals[ulPropModsPos++].Value.ul = MAPI_TO;

			lpMods->aEntries[cEntries].rgPropVals[ulPropModsPos].ulPropTag = PR_EMAIL_ADDRESS_A;
			lpMods->aEntries[cEntries].rgPropVals[ulPropModsPos++].Value.lpszA = (char*)lpMailer->getRecipientErrorEmailAddress(j).c_str();

			lpMods->aEntries[cEntries].rgPropVals[ulPropModsPos].ulPropTag = PR_ADDRTYPE_W;
			lpMods->aEntries[cEntries].rgPropVals[ulPropModsPos++].Value.lpszW = L"SMTP";

			lpMods->aEntries[cEntries].rgPropVals[ulPropModsPos].ulPropTag = PR_DISPLAY_NAME_W;
			if(!lpMailer->getRecipientErrorDisplayName(j).empty()) {
				lpMods->aEntries[cEntries].rgPropVals[ulPropModsPos++].Value.lpszW = (WCHAR*)lpMailer->getRecipientErrorDisplayName(j).c_str();
			} else {
				lpMods->aEntries[cEntries].rgPropVals[ulPropModsPos++].Value.lpszW = converter.convert_to<WCHAR*>(lpMailer->getRecipientErrorEmailAddress(j));
			}

			lpMods->aEntries[cEntries].rgPropVals[ulPropModsPos].ulPropTag = PR_REPORT_TEXT_A;
			lpMods->aEntries[cEntries].rgPropVals[ulPropModsPos++].Value.lpszA = (char*)lpMailer->getRecipientErrorText(j).c_str();

			lpMods->aEntries[cEntries].rgPropVals[ulPropModsPos].ulPropTag = PR_REPORT_TIME;
			lpMods->aEntries[cEntries].rgPropVals[ulPropModsPos++].Value.ft = ft;

			lpMods->aEntries[cEntries].rgPropVals[ulPropModsPos].ulPropTag = PR_TRANSMITABLE_DISPLAY_NAME_A;
			lpMods->aEntries[cEntries].rgPropVals[ulPropModsPos++].Value.lpszA = (char*)lpMailer->getRecipientErrorEmailAddress(j).c_str();

			lpMods->aEntries[cEntries].rgPropVals[ulPropModsPos].ulPropTag = 0x0C200003;//PR_NDR_STATUS_CODE;
			lpMods->aEntries[cEntries].rgPropVals[ulPropModsPos++].Value.ul = lpMailer->getRecipientErrorSMTPCode(j);

			lpMods->aEntries[cEntries].rgPropVals[ulPropModsPos].ulPropTag = PR_NDR_DIAG_CODE;
			lpMods->aEntries[cEntries].rgPropVals[ulPropModsPos++].Value.ul = MAPI_DIAG_MAIL_RECIPIENT_UNKNOWN;

			lpMods->aEntries[cEntries].rgPropVals[ulPropModsPos].ulPropTag = PR_NDR_REASON_CODE;
			lpMods->aEntries[cEntries].rgPropVals[ulPropModsPos++].Value.ul = MAPI_REASON_TRANSFER_FAILED;

			lpMods->aEntries[cEntries].cValues = ulPropModsPos;
			cEntries++;
		}

		lpMods->cEntries = cEntries;

		hr = lpErrorMsg->ModifyRecipients(MODRECIP_ADD, lpMods);
		if (hr != hrSuccess)
			goto exit;
	}

	// Add properties
	hr = lpErrorMsg->SetProps(ulPropPos, lpPropValue, NULL);
	if (hr != hrSuccess)
		goto exit;

	// save message
	hr = lpErrorMsg->SaveChanges(KEEP_OPEN_READONLY);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to commit message: 0x%08X", hr);
		goto exit;
	}

	// New mail notification
	if (HrNewMailNotification(lpStore, lpErrorMsg) != hrSuccess)
		g_lpLogger->Log(EC_LOGLEVEL_WARNING, "Unable to send 'New Mail' notification, error code: 0x%08X", hr);

exit:
	if(lpOriginalMessage)
		lpOriginalMessage->Release();

	if(lpAttach)
		lpAttach->Release();

	if (lpInbox)
		lpInbox->Release();

	if (lpErrorMsg)
		lpErrorMsg->Release();

	if(lpTableMods)
		lpTableMods->Release();

	if (lpPropValue)
		MAPIFreeBuffer(lpPropValue);

	if(lpPropValueAttach)
		MAPIFreeBuffer(lpPropValueAttach);

	if(lpEntryIdSender)
		MAPIFreeBuffer(lpEntryIdSender);

	if(lpPropArrayOriginal)
		MAPIFreeBuffer(lpPropArrayOriginal);

	if(lpRows)
		FreeProws(lpRows);

	if(lpMods)
		FreePadrlist(lpMods);

	if(lpEntryID)
		MAPIFreeBuffer(lpEntryID);

	return hr;
}

/**
 * Converts a Contacts folder EntryID to a ZARAFA addressbook EntryID.
 *
 * A contacts folder EntryID contains an offset that is an index in three different possible EntryID named properties.
 *
 * @param[in]	lpUserStore	The store of the user where the contact is stored.
 * @param[in]	lpAddrBook	The Global Addressbook of the user.
 * @param[in]	cbEntryId	The number of bytes in lpEntryId
 * @param[in]	lpEntryId	The contact EntryID
 * @param[in]	lpulZarafaEID The number of bytes in lppZarafaEID
 * @param[in]	lppZarafaEID  The EntryID where the contact points to
 * @return		HRESULT
 */
HRESULT ContactToZarafa(IMsgStore *lpUserStore, LPADRBOOK lpAddrBook, ULONG cbEntryId, LPENTRYID lpEntryId, ULONG *lpulZarafaEID, LPENTRYID *lppZarafaEID)
{
	HRESULT hr = hrSuccess;
	LPCONTAB_ENTRYID lpContabEntryID = (LPCONTAB_ENTRYID)lpEntryId;
	GUID* guid = (GUID*)&lpContabEntryID->muid;
	ULONG ulObjType;
	LPMAILUSER lpContact = NULL;
	ULONG cValues;
	LPSPropValue lpEntryIds = NULL;
	LPSPropTagArray lpPropTags = NULL;
	LPMAPINAMEID lpNames = NULL;
	LPMAPINAMEID *lppNames = NULL;

	if (sizeof(CONTAB_ENTRYID) > cbEntryId ||
		*guid != PSETID_CONTACT_FOLDER_RECIPIENT ||
		lpContabEntryID->email_offset > 2)
	{
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}

	hr = lpUserStore->OpenEntry(lpContabEntryID->cbeid, (LPENTRYID)lpContabEntryID->abeid, NULL, 0, &ulObjType, (LPUNKNOWN*)&lpContact);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Unable to open contact entryid: 0x%X",hr);
		goto exit;
	}

	hr = MAPIAllocateBuffer(sizeof(MAPINAMEID) * 3, (void**)&lpNames);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "No memory for named ids from contact");
		goto exit;
	}

	hr = MAPIAllocateBuffer(sizeof(LPMAPINAMEID) * 3, (void**)&lppNames);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "No memory for named ids from contact");
		goto exit;
	}

	// Email1EntryID
	lpNames[0].lpguid = (GUID*)&PSETID_Address;
	lpNames[0].ulKind = MNID_ID;
	lpNames[0].Kind.lID = 0x8085;
	lppNames[0] = &lpNames[0];

	// Email2EntryID
	lpNames[1].lpguid = (GUID*)&PSETID_Address;
	lpNames[1].ulKind = MNID_ID;
	lpNames[1].Kind.lID = 0x8095;
	lppNames[1] = &lpNames[1];

	// Email3EntryID
	lpNames[2].lpguid = (GUID*)&PSETID_Address;
	lpNames[2].ulKind = MNID_ID;
	lpNames[2].Kind.lID = 0x80A5;
	lppNames[2] = &lpNames[2];

	hr = lpContact->GetIDsFromNames(3, lppNames, 0, &lpPropTags);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Error while retrieving named data from contact: 0x%08X", hr);
		goto exit;
	}

	hr = lpContact->GetProps(lpPropTags, 0, &cValues, &lpEntryIds);
	if (FAILED(hr)) {
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Unable to get named properties: 0x%08X", hr);
		goto exit;
	}

	if (PROP_TYPE(lpEntryIds[lpContabEntryID->email_offset].ulPropTag) != PT_BINARY) {
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Offset %d not found in contact", lpContabEntryID->email_offset);
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}

	hr = MAPIAllocateBuffer(lpEntryIds[lpContabEntryID->email_offset].Value.bin.cb, (void**)lppZarafaEID);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "No memory for contact eid");
		goto exit;
	}

	memcpy(*lppZarafaEID, lpEntryIds[lpContabEntryID->email_offset].Value.bin.lpb, lpEntryIds[lpContabEntryID->email_offset].Value.bin.cb);
	*lpulZarafaEID = lpEntryIds[lpContabEntryID->email_offset].Value.bin.cb;

exit:
	if (lpPropTags)
		MAPIFreeBuffer(lpPropTags);

	if (lppNames)
		MAPIFreeBuffer(lppNames);

	if (lpNames)
		MAPIFreeBuffer(lpNames);

	if (lpEntryIds)
		MAPIFreeBuffer(lpEntryIds);

	if (lpContact)
		lpContact->Release();

	return hr;
}

/**
 * Converts an One-off EntryID to a ZARAFA addressbook EntryID.
 *
 * @param[in]	lpAddrBook	The Global Addressbook of the user sending the mail.
 * @param[in]	ulSMTPEID	The number of bytes in lpSMTPEID
 * @param[in]	lpSMTPEID	The One off EntryID.
 * @param[out]	lpulZarafaEID	The number of bytes in lppZarafaEID
 * @param[out]	lppZarafaEID	The ZARAFA entryid of the user defined in the One off.
 * @return		HRESULT
 * @retval		MAPI_E_NOT_FOUND	User not a Zarafa user, or lpSMTPEID is not an One-off EntryID
 */
HRESULT SMTPToZarafa(LPADRBOOK lpAddrBook, ULONG ulSMTPEID, LPENTRYID lpSMTPEID, ULONG *lpulZarafaEID, LPENTRYID *lppZarafaEID)
{
	HRESULT hr = hrSuccess;
	wstring wstrName, wstrType, wstrEmailAddress;
	LPADRLIST lpAList = NULL;
	LPSPropValue lpSpoofEID = NULL;
	LPENTRYID lpSpoofBin = NULL;

	// representing entryid can also be a one off id, so search the user, and then get the entryid again ..
	// we then always should have yourself as the sender, otherwise: denied
	if (ECParseOneOff(lpSMTPEID, ulSMTPEID, wstrName, wstrType, wstrEmailAddress) == hrSuccess) {
		MAPIAllocateBuffer(CbNewADRLIST(1), (void**)&lpAList);
		lpAList->cEntries = 1;

		lpAList->aEntries[0].cValues = 1;
		MAPIAllocateBuffer(sizeof(SPropValue) * lpAList->aEntries[0].cValues, (void**)&lpAList->aEntries[0].rgPropVals);

		lpAList->aEntries[0].rgPropVals[0].ulPropTag = PR_DISPLAY_NAME_W;
		lpAList->aEntries[0].rgPropVals[0].Value.lpszW = (WCHAR*)wstrEmailAddress.c_str();
	
		hr = lpAddrBook->ResolveName(0, EMS_AB_ADDRESS_LOOKUP, NULL, lpAList);
		if (hr != hrSuccess)
			goto exit;

		lpSpoofEID = PpropFindProp(lpAList->aEntries[0].rgPropVals, lpAList->aEntries[0].cValues, PR_ENTRYID);
		if (!lpSpoofEID) {
			hr = MAPI_E_NOT_FOUND;
			goto exit;
		}

		hr = MAPIAllocateBuffer(lpSpoofEID->Value.bin.cb, (void**)&lpSpoofBin);
		if (hr != hrSuccess)
			goto exit;

		memcpy(lpSpoofBin, lpSpoofEID->Value.bin.lpb, lpSpoofEID->Value.bin.cb);
		*lppZarafaEID = lpSpoofBin;
		*lpulZarafaEID = lpSpoofEID->Value.bin.cb;
	} else {
		hr = MAPI_E_NOT_FOUND;
	}

exit:
	if (lpAList)
		FreePadrlist(lpAList);

	return hr;
}

/**
 * Find a user in a group. Used when checking for send-as users.
 *
 * @param[in]	lpAdrBook		The Global Addressbook of the user sending the mail.
 * @param[in]	ulOwnerCB		Number of bytes in lpOwnerEID
 * @param[in]	lpOwnerEID		The EntryID of the user to find in the group
 * @param[in]	ulDistListCB	The number of bytes in lpDistlistEID
 * @param[in]	lpDistlistEID	The EntryID of the group
 * @param[out]	lpulCmp			The result of the comparison of CompareEntryID. FALSE if not found, TRUE if found.
 * @param[in]	level			Internal parameter to keep track of recursion. Max is 10 levels deep before it gives up.
 * @return		HRESULT
 */
HRESULT HrFindUserInGroup(LPADRBOOK lpAdrBook, ULONG ulOwnerCB, LPENTRYID lpOwnerEID, ULONG ulDistListCB, LPENTRYID lpDistListEID, ULONG *lpulCmp, int level = 0)
{
	HRESULT hr = hrSuccess;
	ULONG ulCmp = 0;
	ULONG ulObjType = 0;
	LPDISTLIST lpDistList = NULL;
	LPMAPITABLE lpMembersTable = NULL;
	LPSRowSet lpRowSet = NULL;
	SizedSPropTagArray(2, sptaIDProps) = { 2, { PR_ENTRYID, PR_OBJECT_TYPE } };

	if (lpulCmp == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}
	if (level > 10) {
		hr = MAPI_E_TOO_COMPLEX;
		goto exit;
	}

	hr = lpAdrBook->OpenEntry(ulDistListCB, lpDistListEID, NULL, 0, &ulObjType, (LPUNKNOWN*)&lpDistList);
	if (hr != hrSuccess)
		goto exit;

	hr = lpDistList->GetContentsTable(0, &lpMembersTable);
	if (hr != hrSuccess)
		goto exit;

	hr = lpMembersTable->SetColumns((LPSPropTagArray)&sptaIDProps, 0);
	if (hr != hrSuccess)
		goto exit;

	// sort on PR_OBJECT_TYPE (MAILUSER < DISTLIST) ?

	while (TRUE) {
		if (lpRowSet) {
			FreeProws(lpRowSet);
			lpRowSet = NULL;
		}

		hr = lpMembersTable->QueryRows(1, 0, &lpRowSet);
		if (hr != hrSuccess)
			goto exit;

		if (lpRowSet->cRows == 0)
			break;

		if (lpRowSet->aRow[0].lpProps[0].ulPropTag != PR_ENTRYID || lpRowSet->aRow[0].lpProps[1].ulPropTag != PR_OBJECT_TYPE)
			continue;

		if (lpRowSet->aRow[0].lpProps[1].Value.ul == MAPI_MAILUSER) {
			hr = lpAdrBook->CompareEntryIDs(ulOwnerCB, lpOwnerEID,
											lpRowSet->aRow[0].lpProps[0].Value.bin.cb, (LPENTRYID)lpRowSet->aRow[0].lpProps[0].Value.bin.lpb,
											0, &ulCmp);
		} else if (lpRowSet->aRow[0].lpProps[1].Value.ul == MAPI_DISTLIST) {
			hr = HrFindUserInGroup(lpAdrBook, ulOwnerCB, lpOwnerEID, 
								   lpRowSet->aRow[0].lpProps[0].Value.bin.cb, (LPENTRYID)lpRowSet->aRow[0].lpProps[0].Value.bin.lpb,
								   &ulCmp, level+1);
		} else {
			// unknown row
		}
		if (hr == hrSuccess && ulCmp == TRUE)
			break;
	}
	hr = hrSuccess;

	*lpulCmp = ulCmp;

exit:
	if (lpRowSet)
		FreeProws(lpRowSet);

	if (lpMembersTable)
		lpMembersTable->Release();

	if (lpDistList)
		lpDistList->Release();

	return hr;
}

/**
 * Looks up a user in the addressbook, and opens the store of that user.
 *
 * @param[in]	lpAddrBook		The Global Addressbook of the user
 * @param[in]	lpUserStore		The store of the user, just to create the deletegate store entry id
 * @param[in]	lpAdminSession	We need full rights on the delegate store, so use the admin session to open it
 * @param[in]	ulRepresentCB	Number of bytes in lpRepresentEID
 * @param[in]	lpRepresentEID	EntryID of the delegate user
 * @param[out]	lppRepStore		The store of the delegate
 * @return		HRESULT
 */
HRESULT HrOpenRepresentStore(IAddrBook *lpAddrBook, IMsgStore *lpUserStore, IMAPISession *lpAdminSession,
							 ULONG ulRepresentCB, LPENTRYID lpRepresentEID, LPMDB *lppRepStore)
{
	HRESULT hr = hrSuccess;
	ULONG ulObjType = 0;
	LPMAILUSER lpRepresenting = NULL;
	LPSPropValue lpRepAccount = NULL;
	LPEXCHANGEMANAGESTORE lpExchangeManageStore = NULL;
	ULONG ulRepStoreCB = 0;
	LPENTRYID lpRepStoreEID = NULL;
	LPMDB lpRepStore = NULL;

	if (lpAddrBook->OpenEntry(ulRepresentCB, lpRepresentEID, NULL, 0, &ulObjType, (LPUNKNOWN*)&lpRepresenting) != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_INFO, "Unable to open representing user in addressbook");
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}

	if (HrGetOneProp(lpRepresenting, PR_ACCOUNT, &lpRepAccount) != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_INFO, "Unable to find account name for representing user");
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}

	hr = lpUserStore->QueryInterface(IID_IExchangeManageStore, (void **)&lpExchangeManageStore);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_INFO, "IExchangeManageStore interface not found");
		goto exit;
	}

	hr = lpExchangeManageStore->CreateStoreEntryID(NULL, lpRepAccount->Value.LPSZ, fMapiUnicode, &ulRepStoreCB, &lpRepStoreEID);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Unable to create store entryid for representing user '"TSTRING_PRINTF"', error 0x%08x", lpRepAccount->Value.LPSZ, hr);
		goto exit;
	}

	// Use the admin session to open the store, so we have full rights
	hr = lpAdminSession->OpenMsgStore(0, ulRepStoreCB, lpRepStoreEID, NULL, MAPI_BEST_ACCESS, &lpRepStore);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Unable to open store of representing user '"TSTRING_PRINTF"', error 0x%08x", lpRepAccount->Value.LPSZ, hr);
		goto exit;
	}

	hr = lpRepStore->QueryInterface(IID_IMsgStore, (void**)lppRepStore);

exit:
	if (lpRepresenting)
		lpRepresenting->Release();

	if (lpRepAccount)
		MAPIFreeBuffer(lpRepAccount);

	if (lpExchangeManageStore)
		lpExchangeManageStore->Release();

	if (lpRepStoreEID)
		MAPIFreeBuffer(lpRepStoreEID);

	if (lpRepStore)
		lpRepStore->Release();

	return hr;
}

/** 
 * Checks for the presence of an Addressbook EntryID in a given
 * array. If the array contains a group EntryID, it is opened, and
 * searched within the group for the presence of the given EntryID.
 * 
 * @param[in] szFunc Context name how this function is used. Used in logging.
 * @param[in] lpszMailer The name of the user sending the email.
 * @param[in] lpAddrBook The Global Addressbook.
 * @param[in] ulOwnerCB number of bytes in lpOwnerEID
 * @param[in] lpOwnerEID EntryID of the "Owner" object, which is searched in the array
 * @param[in] cValues Number of EntryIDs in lpEntryIds
 * @param[in] lpEntryIDs Array of EntryIDs to search in
 * @param[out] lpulObjType lpOwnerEID was found in this type of object (user or group)
 * @param[out] lpbAllowed User is (not) found in array
 * 
 * @return hrSuccess
 */
HRESULT HrCheckAllowedEntryIDArray(const char *szFunc, const WCHAR* lpszMailer, IAddrBook *lpAddrBook, ULONG ulOwnerCB, LPENTRYID lpOwnerEID, ULONG cValues, SBinary *lpEntryIDs, ULONG *lpulObjType, bool *lpbAllowed)
{
	HRESULT hr = hrSuccess;
	ULONG ulObjType;
	ULONG ulCmpRes;

	for (ULONG i = 0; i < cValues; i++) {
		// quick way to see what object the entryid points to .. otherwise we need to call OpenEntry, which is slow
		if (GetNonPortableObjectType(lpEntryIDs[i].cb, (LPENTRYID)lpEntryIDs[i].lpb, &ulObjType))
			continue;

		if (ulObjType == MAPI_DISTLIST) {
			hr = HrFindUserInGroup(lpAddrBook, ulOwnerCB, lpOwnerEID, lpEntryIDs[i].cb, (LPENTRYID)lpEntryIDs[i].lpb, &ulCmpRes);
		} else if (ulObjType == MAPI_MAILUSER) {
			hr = lpAddrBook->CompareEntryIDs(ulOwnerCB, lpOwnerEID, lpEntryIDs[i].cb, (LPENTRYID)lpEntryIDs[i].lpb, 0, &ulCmpRes);
		} else {
			g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Invalid object %d in %s list of user '%ls'", ulObjType, szFunc, lpszMailer);
			continue;
		}

		if (hr == hrSuccess && ulCmpRes == TRUE) {
			*lpulObjType = ulObjType;
			*lpbAllowed = true;
			goto exit;
		}
	}

	*lpbAllowed = false;

exit:
	// always return success, since lpbAllowed is always written
	return hrSuccess;
}

/**
 * Checks if the current user is has send-as rights as specified user. Needs
 * admin rights to open the delegate store.
 *
 * @param[in]	lpAddrBook	The Global Addressbook of the user trying to send an email.
 * @param[in]	lpUserStore	The store of the user trying to send an email.
 * @param[in]	lpAdminSession MAPI session of the Zarafa SYSTEM user.
 * @param[in]	lpMailer	ECSender object (inetmapi), used to set an error for an error mail if not allowed.
 * @param[in]	ulOwnerCB	Number of bytes in lpOwnerEID
 * @param[in]	lpOwnerEID	EntryID of the user sending the mail.
 * @param[in]	ulRepresentCB Number of bytes in lpRepresentEID.
 * @param[in]	lpRepresentEID EntryID of the user set in the From address. Can be a One-off entryid.
 * @param[out]	lpbAllowed	Set to true if the lpOwnerEID is a delegate of lpRepresentEID
 * @param[out]	lppRepStore	The store of the delegate when allowed.
 * @return		HRESULT
 */
HRESULT CheckSendAs(IAddrBook *lpAddrBook, IMsgStore *lpUserStore, IMAPISession *lpAdminSession, ECSender *lpMailer,
					ULONG ulOwnerCB, LPENTRYID lpOwnerEID, ULONG ulRepresentCB, LPENTRYID lpRepresentEID,
					bool *lpbAllowed, LPMDB *lppRepStore)
{
	HRESULT hr = hrSuccess;
	bool bAllowed = false;
	bool bHasStore = false;
	ULONG ulObjType;
	LPMAILUSER lpMailboxOwner = NULL;
	LPSPropValue lpOwnerProps = NULL;
	LPMAILUSER lpRepresenting = NULL;
	LPSPropValue lpRepresentProps = NULL;
	SPropValue sSpoofEID = {0};
	ULONG ulCmpRes = 0;
	SizedSPropTagArray(3, sptaIDProps) = { 3, { PR_DISPLAY_NAME_W, PR_EC_SENDAS_USER_ENTRYIDS, PR_DISPLAY_TYPE } };
	ULONG cValues = 0;


	hr = SMTPToZarafa(lpAddrBook, ulRepresentCB, lpRepresentEID, &sSpoofEID.Value.bin.cb, (LPENTRYID*)&sSpoofEID.Value.bin.lpb);
	if (hr != hrSuccess)
		hr = ContactToZarafa(lpUserStore, lpAddrBook, ulRepresentCB, lpRepresentEID, &sSpoofEID.Value.bin.cb, (LPENTRYID*)&sSpoofEID.Value.bin.lpb);
	if (hr == hrSuccess) {
		ulRepresentCB = sSpoofEID.Value.bin.cb;
		lpRepresentEID = (LPENTRYID)sSpoofEID.Value.bin.lpb;
	}

	// you can always send as yourself
	if (lpAddrBook->CompareEntryIDs(ulOwnerCB, lpOwnerEID, ulRepresentCB, lpRepresentEID, 0, &ulCmpRes)	== hrSuccess && ulCmpRes == TRUE)
	{
		bAllowed = true;
		goto exit;
	}

	// representing entryid is now always a Zarafa Entry ID. Open the user so we can log the display name
	hr = lpAddrBook->OpenEntry(ulRepresentCB, lpRepresentEID, NULL, 0, &ulObjType, (LPUNKNOWN*)&lpRepresenting);
	if (hr != hrSuccess)
		goto exit;

	hr = lpRepresenting->GetProps((LPSPropTagArray)&sptaIDProps, 0, &cValues, &lpRepresentProps);
	if (FAILED(hr))
		goto exit;
	hr = hrSuccess;

	// Open the owner to get the displayname for logging
	if (lpAddrBook->OpenEntry(ulOwnerCB, lpOwnerEID, NULL, 0, &ulObjType, (LPUNKNOWN*)&lpMailboxOwner) != hrSuccess)
		goto exit;

	hr = lpMailboxOwner->GetProps((LPSPropTagArray)&sptaIDProps, 0, &cValues, &lpOwnerProps);
	if (FAILED(hr))
		goto exit;
	hr = hrSuccess;

	if (lpRepresentProps[2].ulPropTag != PR_DISPLAY_TYPE) {	// Required property for a mailuser object
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}
	bHasStore = (lpRepresentProps[2].Value.l == DT_MAILUSER);

	if (lpRepresentProps[1].ulPropTag != PR_EC_SENDAS_USER_ENTRYIDS) {
		// No sendas, therefore no sendas permissions, but we don't fail
		goto exit;
	}

	hr = HrCheckAllowedEntryIDArray("sendas",
									lpRepresentProps[0].ulPropTag == PR_DISPLAY_NAME_W ? lpRepresentProps[0].Value.lpszW : L"<no name>",
									lpAddrBook, ulOwnerCB, lpOwnerEID,
									lpRepresentProps[1].Value.MVbin.cValues, lpRepresentProps[1].Value.MVbin.lpbin, &ulObjType, &bAllowed);
	if (bAllowed) {
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Mail for user '%ls' is sent as %s '%ls'",
						lpOwnerProps[0].ulPropTag == PR_DISPLAY_NAME_W ? lpOwnerProps[0].Value.lpszW : L"<no name>",
						(ulObjType != MAPI_DISTLIST)?"user":"group",
						lpRepresentProps[0].ulPropTag == PR_DISPLAY_NAME_W ? lpRepresentProps[0].Value.lpszW : L"<no name>");
	}

exit:
	if (!bAllowed) {
		if (lpRepresentProps && PROP_TYPE(lpRepresentProps[0].ulPropTag) != PT_ERROR)
			lpMailer->setError(_("You are not allowed to send as user or group ")+wstring(lpRepresentProps[0].Value.lpszW));
		else
			lpMailer->setError(_("The user or group you try to send as could not be found."));

		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "User '%ls' is not allowed to send as user or group '%ls'. "
						"You may enable all outgoing addresses by enabling the always_send_delegates option.",
						(lpOwnerProps && PROP_TYPE(lpOwnerProps[0].ulPropTag) != PT_ERROR) ? lpOwnerProps[0].Value.lpszW : L"<unknown>",
						(lpRepresentProps && PROP_TYPE(lpRepresentProps[0].ulPropTag) != PT_ERROR) ? lpRepresentProps[0].Value.lpszW : L"<unknown>");
	}

	if (bAllowed && bHasStore) {
		hr = HrOpenRepresentStore(lpAddrBook, lpUserStore, lpAdminSession, ulRepresentCB, lpRepresentEID, lppRepStore);
	} else
		*lppRepStore = NULL;

	*lpbAllowed = bAllowed;

	if (sSpoofEID.Value.bin.lpb)
		MAPIFreeBuffer(sSpoofEID.Value.bin.lpb);

	if (lpOwnerProps)
		MAPIFreeBuffer(lpOwnerProps);

	if (lpRepresentProps)
		MAPIFreeBuffer(lpRepresentProps);

	if (lpMailboxOwner)
		lpMailboxOwner->Release();

	if (lpRepresenting)
		lpRepresenting->Release();

	return hr;
}

/**
 * Checks if the current user is a delegate of a specified user. Needs
 * admin rights to open the delegate store.
 *
 * @param[in]	lpAddrBook	The Global Addressbook of the user trying to send an email.
 * @param[in]	lpUserStore	The store of the user trying to send an email.
 * @param[in]	lpAdminSession MAPI session of the Zarafa SYSTEM user.
 * @param[in]	ulOwnerCB	Number of bytes in lpOwnerEID
 * @param[in]	lpOwnerEID	EntryID of the user sending the mail.
 * @param[in]	ulRepresentCB Number of bytes in lpRepresentEID.
 * @param[in]	lpRepresentEID EntryID of the user set in the From address. Can be a One-off entryid.
 * @param[out]	lpbAllowed	Set to true if the lpOwnerEID is a delegate of lpRepresentEID
 * @param[out]	lppRepStore	The store of the delegate when allowed.
 * @return		HRESULT
 * @retval		hrSuccess, always returned, actual return value in lpbAllowed.
 */
HRESULT CheckDelegate(IAddrBook *lpAddrBook, IMsgStore *lpUserStore, IMAPISession *lpAdminSession, ULONG ulOwnerCB, LPENTRYID lpOwnerEID, ULONG ulRepresentCB, LPENTRYID lpRepresentEID, bool *lpbAllowed, LPMDB *lppRepStore)
{
	HRESULT hr = hrSuccess;
	bool bAllowed = false;
	ULONG ulObjType;
	LPMDB lpRepStore = NULL;
	LPSPropValue lpUserOwnerName = NULL;
	LPSPropValue lpRepOwnerName = NULL;
	LPMAPIFOLDER lpRepSubtree = NULL;
	LPSPropValue lpRepFBProp = NULL;
	LPMESSAGE lpRepFBMessage = NULL;
	LPSPropValue lpDelegates = NULL;
	SPropValue sSpoofEID = {0};


	hr = SMTPToZarafa(lpAddrBook, ulRepresentCB, lpRepresentEID, &sSpoofEID.Value.bin.cb, (LPENTRYID*)&sSpoofEID.Value.bin.lpb);
	if (hr != hrSuccess)
		hr = ContactToZarafa(lpUserStore, lpAddrBook, ulRepresentCB, lpRepresentEID, &sSpoofEID.Value.bin.cb, (LPENTRYID*)&sSpoofEID.Value.bin.lpb);
	if (hr == hrSuccess) {
		ulRepresentCB = sSpoofEID.Value.bin.cb;
		lpRepresentEID = (LPENTRYID)sSpoofEID.Value.bin.lpb;
	}

	hr = HrOpenRepresentStore(lpAddrBook, lpUserStore, lpAdminSession, ulRepresentCB, lpRepresentEID, &lpRepStore);
	if (hr == MAPI_E_NOT_FOUND) {
		hr = hrSuccess;	// No store: no delegate allowed!
		goto exit;
	}
	else if (hr != hrSuccess)
		goto exit;

	hr = HrGetOneProp(lpUserStore, PR_MAILBOX_OWNER_NAME, &lpUserOwnerName);
	hr = HrGetOneProp(lpRepStore, PR_MAILBOX_OWNER_NAME, &lpRepOwnerName);
	// ignore error, just a name for logging

	// open root container
	hr = lpRepStore->OpenEntry(0, NULL, NULL, 0, &ulObjType, (LPUNKNOWN*)&lpRepSubtree);
	if (hr != hrSuccess)
		goto exit;


	hr = HrGetOneProp(lpRepSubtree, PR_FREEBUSY_ENTRYIDS, &lpRepFBProp);
	if (hr != hrSuccess)
		goto exit;

	if (lpRepFBProp->Value.MVbin.cValues < 2) {
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}

	hr = lpRepSubtree->OpenEntry(lpRepFBProp->Value.MVbin.lpbin[1].cb, (LPENTRYID)lpRepFBProp->Value.MVbin.lpbin[1].lpb, NULL, 0, &ulObjType, (LPUNKNOWN*)&lpRepFBMessage);
	if (hr != hrSuccess)
		goto exit;


	hr = HrGetOneProp(lpRepFBMessage, PR_SCHDINFO_DELEGATE_ENTRYIDS, &lpDelegates);
	if (hr != hrSuccess)
		goto exit;

	hr = HrCheckAllowedEntryIDArray("delegate",
									lpRepOwnerName ? lpRepOwnerName->Value.lpszW : L"<no name>",
									lpAddrBook, ulOwnerCB, lpOwnerEID,
									lpDelegates->Value.MVbin.cValues, lpDelegates->Value.MVbin.lpbin, &ulObjType, &bAllowed);
	if (bAllowed) {
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Mail for user '%ls' is allowed on behalf of user '%ls'%s",
						lpUserOwnerName ? lpUserOwnerName->Value.lpszW : L"<no name>",
						lpRepOwnerName ? lpRepOwnerName->Value.lpszW : L"<no name>",
						(ulObjType != MAPI_DISTLIST)?"":" because of group");
	}

exit:
	*lpbAllowed = bAllowed;
	// when any step failed, delegate is not setup correctly, so bAllowed == false
	hr = hrSuccess;

	if (bAllowed) {
		*lppRepStore = lpRepStore;
		lpRepStore = NULL;
	}

	if (sSpoofEID.Value.bin.lpb)
		MAPIFreeBuffer(sSpoofEID.Value.bin.lpb);

	if (lpRepStore)
		lpRepStore->Release();

	if (lpRepOwnerName)
		MAPIFreeBuffer(lpRepOwnerName);

	if (lpUserOwnerName)
		MAPIFreeBuffer(lpUserOwnerName);

	if (lpRepSubtree)
		lpRepSubtree->Release();

	if (lpRepFBProp)
		MAPIFreeBuffer(lpRepFBProp);

	if (lpRepFBMessage)
		lpRepFBMessage->Release();

	if (lpDelegates)
		MAPIFreeBuffer(lpDelegates);

	return hr;
}

/**
 * Copies the sent message to the delegate store. Returns the copy of lpMessage.
 *
 * @param[in]	lpMessage	The message to be copied to the delegate store in that "Sent Items" folder.
 * @param[in]	lpRepStore	The store of the delegate where the message will be copied.
 * @param[out]	lppRepMessage The new message in the delegate store.
 * @return		HRESULT
 */
HRESULT CopyDelegateMessageToSentItems(LPMESSAGE lpMessage, LPMDB lpRepStore, LPMESSAGE *lppRepMessage) {
	HRESULT hr = hrSuccess;
	LPSPropValue lpSentItemsEntryID = NULL;
	LPMAPIFOLDER lpSentItems = NULL;
	ULONG ulObjType;
	LPMESSAGE lpDestMsg = NULL;
	SPropValue sProp[1];

	hr = HrGetOneProp(lpRepStore, PR_IPM_SENTMAIL_ENTRYID, &lpSentItemsEntryID);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_WARNING, "Unable to find representee's sent items folder: error 0x%08X", hr);
		goto exit;
	}

	hr = lpRepStore->OpenEntry(lpSentItemsEntryID->Value.bin.cb, (LPENTRYID)lpSentItemsEntryID->Value.bin.lpb,
							   &IID_IMAPIFolder, MAPI_BEST_ACCESS, &ulObjType, (LPUNKNOWN*)&lpSentItems);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_WARNING, "Unable to open representee's sent items folder: error 0x%08X", hr);
		goto exit;
	}

	hr = lpSentItems->CreateMessage(NULL, 0, &lpDestMsg);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_WARNING, "Unable to create representee's message: error 0x%08X", hr);
		goto exit;
	}

	hr = lpMessage->CopyTo(0, NULL, NULL, 0, NULL, &IID_IMessage, (LPVOID)lpDestMsg, 0, NULL);
	if (FAILED(hr)) {
		g_lpLogger->Log(EC_LOGLEVEL_WARNING, "Unable to copy representee's message: error 0x%08X", hr);
		goto exit;
	}

	sProp[0].ulPropTag = PR_MESSAGE_FLAGS;
	sProp[0].Value.ul = MSGFLAG_READ;

	hr = lpDestMsg->SetProps(1, sProp, NULL);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_WARNING, "Unable to edit representee's message: error 0x%08X", hr);
		goto exit;
	}

	*lppRepMessage = lpDestMsg;
	lpDestMsg = NULL;
	g_lpLogger->Log(EC_LOGLEVEL_INFO, "Copy placed in representee's sent items folder");

exit:
	if (lpSentItemsEntryID)
		MAPIFreeBuffer(lpSentItemsEntryID);

	if (lpSentItems)
		lpSentItems->Release();

	if (lpDestMsg)
		lpDestMsg->Release();

	return hr;
}

/**
 * Delete the message from the outgoing queue. Should always be
 * called, unless the message should be retried later (SMTP server
 * temporarily not available or timed message).
 *
 * @param[in]	cbEntryId	Number of bytes in lpEntryId
 * @param[in]	lpEntryId	EntryID of the message to remove from outgoing queue.
 * @param[in]	lpMsgStore	Message store of the user containing the message of lpEntryId
 * @return		HRESULT
 */
HRESULT PostSendProcessing(ULONG cbEntryId, LPENTRYID lpEntryId, IMsgStore *lpMsgStore)
{
	HRESULT hr = hrSuccess;
	LPSPropValue lpObject = NULL;
	IECSpooler *lpSpooler = NULL;
	
	hr = HrGetOneProp(lpMsgStore, PR_EC_OBJECT, &lpObject);
	if(hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to get PR_EC_OBJECT in post-send processing: 0x%08X", hr);
		goto exit;
	}
	
	hr = ((IECUnknown *)lpObject->Value.lpszA)->QueryInterface(IID_IECSpooler, (void **)&lpSpooler);
	if(hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to get spooler interface for message: 0x%08X", hr);
		goto exit;
	}
	
	hr = lpSpooler->DeleteFromMasterOutgoingTable(cbEntryId, lpEntryId, EC_SUBMIT_MASTER);
	if (hr != hrSuccess)
		g_lpLogger->Log(EC_LOGLEVEL_WARNING, "Could not remove invalid message from queue, error code: 0x%08X", hr);

exit:
	if(lpObject)
		MAPIFreeBuffer(lpObject);
		
	if(lpSpooler)
		lpSpooler->Release();
		
	return hr;
}

/**
 * Using the given resources, sends the mail to the SMTP server.
 *
 * @param[in]	lpAdminSession	Zarafa SYSTEM user MAPI session.
 * @param[in]	lpUserSession	MAPI Session of the user sending the mail.
 * @param[in]	lpServiceAdmin	IECServiceAdmin interface on the user's store.
 * @param[in]	lpSecurity		IECSecurity interface on the user's store.
 * @param[in]	lpUserStore		The IMsgStore interface of the user's store.
 * @param[in]	lpAddrBook		The Global Addressbook of the user.
 * @param[in]	lpMailer		ECSender object (inetmapi), used to send the mail.
 * @param[in]	cbMsgEntryId	Number of bytes in lpMsgEntryId
 * @param[in]	lpMsgEntryId	EntryID of the message to be send.
 * @param[out]	lppMessage		The message that processed. Always returned if opened.
 *
 * @note The mail will be removed by the calling process when we return an error, except for the errors/warnings listed below.
 * @retval	hrSuccess	Mail was successful sent moved when when needed.
 * @retval	MAPI_E_WAIT	Mail has a specific timestamp when it should be sent.
 * @retval	MAPI_W_NO_SERVICE	The SMTP server is not responding correctly.
 */
HRESULT ProcessMessage(IMAPISession *lpAdminSession, IMAPISession *lpUserSession,
					   IECServiceAdmin *lpServiceAdmin, IECSecurity *lpSecurity,
					   IMsgStore *lpUserStore, IAddrBook *lpAddrBook,
					   ECSender *lpMailer, ULONG cbMsgEntryId, LPENTRYID lpMsgEntryId,
					   IMessage **lppMessage)
{
	HRESULT 		hr 				= hrSuccess;
	LPECUSER		lpUserAdmin		= NULL;

	LPMESSAGE		lpMessage		= NULL;
	ULONG			ulObjType		= 0;

	ULONG			cbOwner			= 0;
	LPENTRYID		lpOwner			= NULL;
	ECUSER			*lpUser			= NULL;
	SPropValue		sPropSender[4];

	SizedSPropTagArray(5, sptaMoveReprProps) = {
		5, { PR_SENT_REPRESENTING_NAME_W, PR_SENT_REPRESENTING_ADDRTYPE_W,
			 PR_SENT_REPRESENTING_EMAIL_ADDRESS_W, PR_SENT_REPRESENTING_ENTRYID, PR_SENT_REPRESENTING_SEARCH_KEY }
	};
	LPSPropValue	lpMoveReprProps = NULL;
	ULONG			cValuesMoveProps = 0;

	LPSPropValue	lpPropOwner = NULL;
	bool			bAllowSendAs = false;
	bool			bAllowDelegate = false;
	ULONG			ulCmpRes = 0;
	
	LPMDB			lpRepStore		= NULL;
	LPMESSAGE		lpRepMessage	= NULL;

	LPSPropValue	lpPropSRNam		= NULL;
	LPSPropValue	lpPropSREma		= NULL;
	LPSPropValue	lpPropSREid		= NULL;
	LPSPropValue	lpRepEntryID	= NULL;
	LPSPropValue	lpSubject		= NULL;
	LPSPropValue	lpMsgSize		= NULL;
	LPSPropValue	lpDeferSendTime	= NULL;
	LPSPropValue	lpAutoForward	= NULL;
	LPSPropValue	lpMsgClass		= NULL;

	ArchiveResult	archiveResult;

	sending_options sopt;

	imopt_default_sending_options(&sopt);
	// optional force sending with TNEF
	sopt.force_tnef = parseBool(g_lpConfig->GetSetting("always_send_tnef"));
	sopt.force_utf8 = parseBool(g_lpConfig->GetSetting("always_send_utf8"));
	sopt.allow_send_to_everyone = parseBool(g_lpConfig->GetSetting("allow_send_to_everyone"));

	// so we require admin stuff now
	hr = lpServiceAdmin->GetUser(g_cbDefaultEid, (LPENTRYID)g_lpDefaultEid, MAPI_UNICODE, &lpUserAdmin);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to get user admin information from store. error code: 0x%08X", hr);
		goto exit;
	}

	// Get the owner of the store
	hr = lpSecurity->GetOwner(&cbOwner, &lpOwner);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to get owner information, error code: 0x%08X", hr);
		goto exit;
	}

	// We now have the owner ID, get the owner information through the ServiceAdmin
	hr = lpServiceAdmin->GetUser(cbOwner, lpOwner, MAPI_UNICODE, &lpUser);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to get user information from store, error code: 0x%08X", hr);
		goto exit;
	}

	// open the message we need to send
	hr = lpUserStore->OpenEntry(cbMsgEntryId, (LPENTRYID)lpMsgEntryId, &IID_IMessage, MAPI_BEST_ACCESS, &ulObjType, (LPUNKNOWN*)&lpMessage);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Could not open message in store from user %ls", lpUser->lpszUsername);
		goto exit;
	}

	// get subject for logging
	HrGetOneProp(lpMessage, PR_SUBJECT_W, &lpSubject);
	HrGetOneProp(lpMessage, PR_MESSAGE_SIZE, &lpMsgSize);
	HrGetOneProp(lpMessage, PR_DEFERRED_SEND_TIME, &lpDeferSendTime);

	// do we need to send the message already?
	if (lpDeferSendTime) {
		// check time
		time_t now = time(NULL);
		time_t sendat;

		FileTimeToUnixTime(lpDeferSendTime->Value.ft, &sendat);

		if (now < sendat) {
			// should actually be logged just once .. but how?
			struct tm tmp;
			char timestring[256];

			localtime_r(&sendat, &tmp);
			strftime(timestring, 256, "%c", &tmp);

			g_lpLogger->Log(EC_LOGLEVEL_INFO, "E-mail for user %ls, subject '%ls', should be sent later at '%s'",
							lpUser->lpszUsername, lpSubject ? lpSubject->Value.lpszW : L"<none>", timestring);

			hr = MAPI_E_WAIT;
			goto exit;
		}
	}

	// fatal, all other log messages are otherwise somewhat meaningless
	g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Sending e-mail for user %ls, subject: '%ls', size: %d",
					lpUser->lpszUsername, lpSubject ? lpSubject->Value.lpszW : L"<none>",
					lpMsgSize ? lpMsgSize->Value.ul : 0);

	/* 

	   PR_SENDER_* maps to Sender:
	   PR_SENT_REPRESENTING_* maps to From:

	   Sender: field is optional, From: is mandatory
	   PR_SENDER_* is mandatory, and always set by us (will be overwritten if was set)
	   PR_SENT_REPRESENTING_* is optional, and set by outlook when the user modifies the From in outlook.

	*/

	// Set PR_SENT_REPRESENTING, as this is set on all 'sent' items and is the column
	// that is shown by default in Outlook's 'sent items' folder
	if (HrGetOneProp(lpMessage, PR_SENT_REPRESENTING_ENTRYID, &lpRepEntryID) != hrSuccess) {
		// set current user as sender (From header)
		sPropSender[0].ulPropTag = PR_SENT_REPRESENTING_NAME_W;
		sPropSender[0].Value.lpszW = (LPTSTR)lpUser->lpszFullName;
		sPropSender[1].ulPropTag = PR_SENT_REPRESENTING_ADDRTYPE_W;
		sPropSender[1].Value.lpszW = (LPTSTR)L"ZARAFA";
		sPropSender[2].ulPropTag = PR_SENT_REPRESENTING_EMAIL_ADDRESS_W;
		sPropSender[2].Value.lpszW = (LPTSTR)lpUser->lpszMailAddress;

		sPropSender[3].ulPropTag = PR_SENT_REPRESENTING_ENTRYID;
		sPropSender[3].Value.bin.cb = lpUser->sUserId.cb;
		sPropSender[3].Value.bin.lpb = lpUser->sUserId.lpb;

		if (lpMessage->SetProps(4, sPropSender, NULL) != hrSuccess) {
			g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Unable to set sender id for message");
			goto exit;
		}
	} else {
		// requested that mail is sent as somebody else
		// since we can have SMTP and ZARAFA entry id's, we'll open it, and get the

		// If this is a forwarded e-mail, then allow sending as the original sending e-mail address. Note that
		// this can be misused by MAPI client that just set PR_AUTO_FORWARDED. Since it would have been just as
		// easy for the client just to spoof their 'from' address via SMTP, we're allowing this for now. You can
		// completely turn it off via the 'allow_redirect_spoofing' setting.
		if (strcmp(g_lpConfig->GetSetting("allow_redirect_spoofing"), "yes") == 0 &&
			HrGetOneProp(lpMessage, PR_AUTO_FORWARDED, &lpAutoForward) == hrSuccess && lpAutoForward->Value.b)
		{
			bAllowSendAs = true;
		} else {

			hr = HrGetOneProp(lpUserStore, PR_MAILBOX_OWNER_ENTRYID, &lpPropOwner);
			if (hr != hrSuccess) {
				g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to get Zarafa mailbox owner id, error code: 0x%08X", hr);
				goto exit;
			}

			hr = lpAddrBook->CompareEntryIDs(lpPropOwner->Value.bin.cb, (LPENTRYID)lpPropOwner->Value.bin.lpb,
											 lpRepEntryID->Value.bin.cb, (LPENTRYID)lpRepEntryID->Value.bin.lpb, 0, &ulCmpRes);
			if (hr == hrSuccess && ulCmpRes == FALSE) {

				if (strcmp(g_lpConfig->GetSetting("always_send_delegates"), "yes") == 0) {
					// pre 6.20 behaviour
					bAllowDelegate = true;
					HrOpenRepresentStore(lpAddrBook, lpUserStore, lpAdminSession, lpRepEntryID->Value.bin.cb, (LPENTRYID)lpRepEntryID->Value.bin.lpb, &lpRepStore);
					// ignore error if unable to open, just the copy of the mail might possibily not be done.
				} else 	if(strcmp(g_lpConfig->GetSetting("allow_delegate_meeting_request"), "yes") == 0 &&
							HrGetOneProp(lpMessage, PR_MESSAGE_CLASS_A, &lpMsgClass) == hrSuccess &&
							((stricmp(lpMsgClass->Value.lpszA, "IPM.Schedule.Meeting.Request" ) == 0) ||
							 (stricmp(lpMsgClass->Value.lpszA, "IPM.Schedule.Meeting.Canceled" ) == 0))) {
					// Meeting request can always sent as 'on behalf of' (Zarafa and SMTP user).
					// This is needed if a user forward a meeting request. If you have permissions on a calendar,
					// you can always sent with 'on behalve of'. This behavior is like exchange.

					bAllowDelegate = true;
				} else {
					hr = CheckDelegate(lpAddrBook, lpUserStore, lpAdminSession, lpPropOwner->Value.bin.cb, (LPENTRYID)lpPropOwner->Value.bin.lpb,
									  lpRepEntryID->Value.bin.cb, (LPENTRYID)lpRepEntryID->Value.bin.lpb, &bAllowDelegate, &lpRepStore);
					if (hr != hrSuccess)
						goto exit;
				}

				if (!bAllowDelegate) {

					hr = CheckSendAs(lpAddrBook, lpUserStore, lpAdminSession, lpMailer, lpPropOwner->Value.bin.cb, (LPENTRYID)lpPropOwner->Value.bin.lpb,
									lpRepEntryID->Value.bin.cb, (LPENTRYID)lpRepEntryID->Value.bin.lpb, &bAllowSendAs, &lpRepStore);
					if (hr != hrSuccess)
						goto exit;

					if (!bAllowSendAs) {

						g_lpLogger->Log(EC_LOGLEVEL_FATAL, "E-mail for user %ls may not be sent, notifying user", lpUser->lpszUsername);

						if (SendUndeliverable(lpAddrBook, lpMailer, lpUserStore, lpUserAdmin, lpMessage) != hrSuccess) {
							g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Unable to create undeliverable message for user %ls", lpUser->lpszUsername);
						}
						// note: hr == hrSuccess, parent process will not send the undeliverable too
						goto exit;
					}
					// else {}: we are allowed to directly send
				}
				// else {}: allowed with 'on behalf of'
			}
			// else {}: owner and representing are the same, send as normal mail
		}
	}

	// put storeowner info in PR_SENDER_ props, forces correct From data
	sPropSender[0].ulPropTag = PR_SENDER_NAME_W;
	sPropSender[0].Value.LPSZ = lpUser->lpszFullName;
	sPropSender[1].ulPropTag = PR_SENDER_ADDRTYPE_W;
	sPropSender[1].Value.LPSZ = _T("ZARAFA");
	sPropSender[2].ulPropTag = PR_SENDER_EMAIL_ADDRESS_W;
	sPropSender[2].Value.LPSZ = lpUser->lpszMailAddress;

	sPropSender[3].ulPropTag = PR_SENDER_ENTRYID;
	sPropSender[3].Value.bin.cb = lpUser->sUserId.cb;
	sPropSender[3].Value.bin.lpb = lpUser->sUserId.lpb;
	// @todo PR_SENDER_SEARCH_KEY

	hr = lpMessage->SetProps(4, sPropSender, NULL);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Unable to update message with sender");
		goto exit;
	}

	hr = lpMessage->SaveChanges(KEEP_OPEN_READWRITE);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Unable to save message before sending");
		goto exit;
	}

	if (parseBool(g_lpConfig->GetSetting("archive_on_send"))) {
		ArchivePtr ptrArchive;
		
		hr = Archive::Create(lpAdminSession, g_lpLogger, &ptrArchive);
		if (hr != hrSuccess) {
			g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to instantiate archive object: 0x%08X", hr);
			goto exit;
		}
		
		hr = ptrArchive->HrArchiveMessageForSending(lpMessage, &archiveResult);
		if (hr != hrSuccess) {
			if (ptrArchive->HaveErrorMessage())
				lpMailer->setError(ptrArchive->GetErrorMessage());
			goto exit;
		}
	}

	if (lpRepStore && parseBool(g_lpConfig->GetSetting("copy_delegate_mails",NULL,"yes"))) {
		// copy the original message with the actual sender data
		// so you see the "on behalf of" in the sent-items version, even when send-as is used (see below)
		CopyDelegateMessageToSentItems(lpMessage, lpRepStore, &lpRepMessage);
		// possible error is logged in function.
	}

	if (bAllowSendAs) {
		// move PR_REPRESENTING to PR_SENDER_NAME
		hr = lpMessage->GetProps((LPSPropTagArray)&sptaMoveReprProps, 0, &cValuesMoveProps, &lpMoveReprProps);
		if (FAILED(hr)) {
			g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Unable to find sender information");
			goto exit;
		}

		hr = lpMessage->DeleteProps((LPSPropTagArray)&sptaMoveReprProps, NULL);
		if (FAILED(hr)) {
			g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Unable to remove sender information");
			goto exit;
		}

		lpMoveReprProps[0].ulPropTag = PROP_TAG(PROP_TYPE(lpMoveReprProps[0].ulPropTag), PROP_ID(PR_SENDER_NAME_W));
		lpMoveReprProps[1].ulPropTag = PROP_TAG(PROP_TYPE(lpMoveReprProps[1].ulPropTag), PROP_ID(PR_SENDER_ADDRTYPE_W));
		lpMoveReprProps[2].ulPropTag = PROP_TAG(PROP_TYPE(lpMoveReprProps[2].ulPropTag), PROP_ID(PR_SENDER_EMAIL_ADDRESS_W));
		lpMoveReprProps[3].ulPropTag = PROP_TAG(PROP_TYPE(lpMoveReprProps[3].ulPropTag), PROP_ID(PR_SENDER_ENTRYID));
		lpMoveReprProps[4].ulPropTag = PROP_TAG(PROP_TYPE(lpMoveReprProps[4].ulPropTag), PROP_ID(PR_SENDER_SEARCH_KEY));

		hr = lpMessage->SetProps(5, lpMoveReprProps, NULL);
		if (FAILED(hr)) {
			g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Unable to update sender information");
			goto exit;
		}

		/*
		 * Note: do not save these changes!
		 *
		 * If we're sending through Outlook, we're sending a copy of
		 * the message from the root container. Changes to this
		 * message make no sense, since it's deleted anyway.
		 *
		 * If we're sending through WebAccess, we're sending the real
		 * message, and bDoSentMail is true. This will move the
		 * message to the users sent-items folder (using the entryid
		 * from the message) and move it using it's entryid. Since we
		 * didn't save these changes, the original unmodified version
		 * will be moved to the sent-items folder, and that will show
		 * the correct From/Sender data.
		 */
	}

	if(parseBool(g_lpConfig->GetSetting("expand_groups"))) {
		// Expand recipients with ADDRTYPE=ZARAFA to multiple ADDRTYPE=SMTP recipients
		hr = ExpandRecipients(lpAddrBook, lpMessage);
		if(hr != hrSuccess)
			g_lpLogger->Log(EC_LOGLEVEL_WARNING, "Unable to expand message recipient groups");
	}

	hr = RewriteRecipients(lpUserSession, lpMessage);
	if (hr != hrSuccess)
		g_lpLogger->Log(EC_LOGLEVEL_WARNING, "Unable to rewrite recipients");

	if(parseBool(g_lpConfig->GetSetting("expand_groups"))) {
		// Only touch recips if we're expanding groups; the rationale is here that the user
		// has typed a recipient twice if we have duplicates and expand_groups = no, so that's
		// what the user wanted apparently. What's more, duplicate recips are filtered for RCPT TO
		// later.
		hr = UniqueRecipients(lpMessage);
		if (hr != hrSuccess)
			g_lpLogger->Log(EC_LOGLEVEL_WARNING, "Unable to remove duplicate recipients");
	}

	// Now hand message to library which will send it, inetmapi will handle addressbook
	hr = IMToINet(lpUserSession, lpAddrBook, lpMessage, lpMailer, sopt, g_lpLogger);

	// log using fatal, all other log messages are otherwise somewhat meaningless
	if (hr == MAPI_W_NO_SERVICE) {
		g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to connect to SMTP server, retrying mail for user %ls later", lpUser->lpszUsername);
		goto exit;
	} else if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_FATAL, "E-mail for user %ls could not be sent, notifying user", lpUser->lpszUsername);
		if (SendUndeliverable(lpAddrBook, lpMailer, lpUserStore, lpUserAdmin, lpMessage) != hrSuccess) {
			g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Unable to create undeliverable message for user %ls", lpUser->lpszUsername);
		}
		// we set hr to success, so the parent process does not create the undeliverable thing again
		hr = hrSuccess;
		goto exit;
	} else {
		g_lpLogger->Log(EC_LOGLEVEL_FATAL, "E-mail for user %ls was accepted by SMTP server", lpUser->lpszUsername);
	}

	// If we have a repsenting message, save that now in the sent-items of that user
	if (lpRepMessage) {
		if (lpRepMessage->SaveChanges(0) != hrSuccess)
			g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Representee's mail copy could not be saved: error 0x%08X", hr);
	}

exit:
	if (FAILED(hr))
		archiveResult.Undo(lpAdminSession);

	// We always return the processes message to the caller, whether it failed or not
	if (lpMessage)
		lpMessage->QueryInterface(IID_IMessage, (void**)lppMessage);


	if (lpMessage)
		lpMessage->Release();

	if (lpUserAdmin)
		MAPIFreeBuffer(lpUserAdmin);

	if (lpRepMessage)
		lpRepMessage->Release();

	if (lpRepStore)
		lpRepStore->Release();

	if (lpAutoForward)
		MAPIFreeBuffer(lpAutoForward);

	if (lpMsgClass)
		MAPIFreeBuffer(lpMsgClass);

	if (lpOwner)
		MAPIFreeBuffer(lpOwner);

	if (lpPropOwner)
		MAPIFreeBuffer(lpPropOwner);

	if (lpMoveReprProps)
		MAPIFreeBuffer(lpMoveReprProps);

	// Free checked properties
	if (lpPropSRNam)
		MAPIFreeBuffer(lpPropSRNam);

	if (lpPropSREma)
		MAPIFreeBuffer(lpPropSREma);

	if (lpPropSREid)
		MAPIFreeBuffer(lpPropSREid);

	if (lpRepEntryID)
		MAPIFreeBuffer(lpRepEntryID);

	// Free everything else..
	if (lpSecurity)
		lpSecurity->Release();

	if (lpUser)
		MAPIFreeBuffer(lpUser);

	if (lpSubject)
		MAPIFreeBuffer(lpSubject);

	if (lpMsgSize)
		MAPIFreeBuffer(lpMsgSize);

	if (lpDeferSendTime)
		MAPIFreeBuffer(lpDeferSendTime);

	return hr;
}

/**
 * Entry point, sends the mail for a user. Most of the time, it will
 * also move the sent mail to the "Sent Items" folder of the user.
 *
 * @param[in]	szUsername	The username to login as. This name is in unicode.
 * @param[in]	szSMTP		The SMTP server name or IP address to use.
 * @param[in]	szPath		The URI to the Zarafa server.
 * @param[in]	cbMsgEntryId The number of bytes in lpMsgEntryId
 * @param[in]	lpMsgEntryId The EntryID of the message to send
 * @param[in]	bDoSentMail	true if the mail should be moved to the "Sent Items" folder of the user.
 * @return		HRESULT
 */
HRESULT ProcessMessageForked(const wchar_t *szUsername, char *szSMTP, int ulPort, char *szPath, ULONG cbMsgEntryId, LPENTRYID lpMsgEntryId, bool bDoSentMail)
{
	HRESULT			hr = hrSuccess;
	IMAPISession	*lpAdminSession = NULL;
	IMAPISession	*lpUserSession = NULL;
	IAddrBook		*lpAddrBook = NULL;
	ECSender		*lpMailer = NULL;
	IMsgStore		*lpUserStore = NULL;
	IECServiceAdmin	*lpServiceAdmin = NULL;
	IECSecurity		*lpSecurity = NULL;
	SPropValue		*lpsProp = NULL;
	IMessage		*lpMessage = NULL;
	LPECUSER		lpUserAdmin = NULL;	// for error message
	
	lpMailer = CreateSender(g_lpLogger, szSMTP, ulPort);
	if (!lpMailer) {
		hr = MAPI_E_NOT_ENOUGH_MEMORY;
		goto exit;
	}

	// The Admin session is used for checking delegates and archiving
	hr = HrOpenECAdminSession(&lpAdminSession, szPath, EC_PROFILE_FLAGS_NO_PUBLIC_STORE,
							  g_lpConfig->GetSetting("sslkey_file", "", NULL),
							  g_lpConfig->GetSetting("sslkey_pass", "", NULL));
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to open admin session. Error 0x%08X", hr);
		goto exit;
	}

	/* 
	 * For proper group expansion, we'll need to login as the
	 * user. When sending an email to group 'Everyone' it should not
	 * be possible to send the email to users that cannot be viewed
	 * (because they are in a different company).  By using a
	 * usersession for email sending we will let the server handle all
	 * permissions and can correctly resolve everything.
	 */
	hr = HrOpenECSession(&lpUserSession, szUsername, L"", szPath, EC_PROFILE_FLAGS_NO_PUBLIC_STORE,
						 g_lpConfig->GetSetting("sslkey_file", "", NULL),
						 g_lpConfig->GetSetting("sslkey_pass", "", NULL));
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to open user session. Error 0x%08X", hr);
		goto exit;
	}

	hr = lpUserSession->OpenAddressBook(0, NULL, AB_NO_DIALOG, &lpAddrBook);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to open addressbook. Error 0x%08X", hr);
		goto exit;
	}

	hr = HrOpenDefaultStore(lpUserSession, &lpUserStore);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to open default store of user. Error 0x%08X", hr);
		goto exit;
	}

	hr = HrGetOneProp(lpUserStore, PR_EC_OBJECT, &lpsProp);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to get Zarafa internal object");
		goto exit;
	}

	// NOTE: object is placed in Value.lpszA, not Value.x
	hr = ((IECUnknown*)lpsProp->Value.lpszA)->QueryInterface(IID_IECServiceAdmin, (void **)&lpServiceAdmin);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_FATAL, "ServiceAdmin interface not supported");
		goto exit;
	}

	hr = ((IECUnknown*)lpsProp->Value.lpszA)->QueryInterface(IID_IECSecurity, (void **)&lpSecurity);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_FATAL, "IID_IECSecurity not supported by store, error code: 0x%08X", hr);
		goto exit;
	}

	hr = ProcessMessage(lpAdminSession, lpUserSession, lpServiceAdmin, lpSecurity, lpUserStore, lpAddrBook, lpMailer, cbMsgEntryId, lpMsgEntryId, &lpMessage);
	if (hr != hrSuccess && hr != MAPI_E_WAIT && hr != MAPI_W_NO_SERVICE && lpMessage) {
		// use lpMailer to set body in SendUndeliverable
		if (! lpMailer->haveError())
			lpMailer->setError(_("Error found while trying to send your message. Error code: ")+wstringify(hr,true));
		
		hr = lpServiceAdmin->GetUser(g_cbDefaultEid, (LPENTRYID)g_lpDefaultEid, MAPI_UNICODE, &lpUserAdmin);
		if (hr != hrSuccess)
			goto exit;

		hr = SendUndeliverable(lpAddrBook, lpMailer, lpUserStore, lpUserAdmin, lpMessage);
		if (hr != hrSuccess) {
			// dont make parent complain too
			hr = hrSuccess;
			goto exit;
		}
	}

exit:
	// The following code is after the exit tag because we *always* want to clean up the message from the outgoing queue, not
	// just when it was sent correctly. This also means we should do post-sending processing (DoSentMail()).
	// Ignore error, we want to give the possible failed hr back to the main process. Logging is already done.
	if (hr != MAPI_W_NO_SERVICE && hr != MAPI_E_WAIT) {
		if (lpMsgEntryId && lpUserStore)
			PostSendProcessing(cbMsgEntryId, lpMsgEntryId, lpUserStore);
	
		if (bDoSentMail && lpUserSession && lpMessage) {
			DoSentMail(NULL, lpUserStore, 0, lpMessage);
			lpMessage = NULL; // DoSentMail releases lpMessage for us
		}
	}

	if (lpUserSession)
		lpUserSession->Release();

	if (lpAddrBook)
		lpAddrBook->Release();

	if (lpUserStore)
		lpUserStore->Release();

	if (lpUserAdmin)
		MAPIFreeBuffer(lpUserAdmin);

	if (lpMessage)
		lpMessage->Release();

	if (lpsProp)
		MAPIFreeBuffer(lpsProp);

	if (lpServiceAdmin)
		lpServiceAdmin->Release();

	if (lpAdminSession)
		lpAdminSession->Release();

	if (lpMailer)
		delete lpMailer;

	return hr;
}
