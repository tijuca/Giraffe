/*
 * Copyright 2005 - 2016 Zarafa and its licensors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License, version 3,
 * as published by the Free Software Foundation.
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

#include <kopano/platform.h>
#include <memory>
#include <utility>
#include "mailer.h"
#include "archive.h"

#include <mapitags.h>
#include <kopano/mapiext.h>
#include <kopano/memory.hpp>
#include <kopano/tie.hpp>
#include <mapiutil.h>
#include <mapidefs.h>
#include <mapix.h>
#include <mapi.h>

#include <kopano/ECLogger.h>
#include <kopano/ECRestriction.h>
#include <kopano/ECConfig.h>
#include <kopano/IECUnknown.h>
#include <kopano/ecversion.h>
#include <kopano/IECSecurity.h>
#include <kopano/IECServiceAdmin.h>
#include <kopano/MAPIErrors.h>
#include "IECSpooler.h"
#include <kopano/ECGuid.h>
#include <edkguid.h>
#include <kopano/CommonUtil.h>
#include <kopano/Util.h>
#include <kopano/stringutil.h>
#include "mapicontact.h"
#include <kopano/mapiguidext.h>
#include <kopano/EMSAbTag.h>
#include <kopano/ECABEntryID.h>
#include <kopano/ECGetText.h>

#include <kopano/charset/convert.h>
#include <kopano/charset/convstring.h>

#include "PyMapiPlugin.h"

#include <list>
#include <algorithm>
#include "spmain.h"

using namespace std;
using namespace KCHL;

static HRESULT GetPluginObject(PyMapiPluginFactory *lpPyMapiPluginFactory,
    PyMapiPlugin **lppPyMapiPlugin)
{
    HRESULT hr = hrSuccess;
	std::unique_ptr<PyMapiPlugin> lpPyMapiPlugin;

    if (lpPyMapiPluginFactory == nullptr || lppPyMapiPlugin == nullptr) {
        assert(false);
		return MAPI_E_INVALID_PARAMETER;
    }
	hr = lpPyMapiPluginFactory->CreatePlugin("SpoolerPluginManager", &unique_tie(lpPyMapiPlugin));
	if (hr != hrSuccess) {
		ec_log_crit("Unable to initialize plugin system, please check your configuration: %s (%x).",
			GetMAPIErrorMessage(hr), hr);
		return MAPI_E_CALL_FAILED;
	}
	*lppPyMapiPlugin = lpPyMapiPlugin.release();
	return hrSuccess;
}

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
static HRESULT ExpandRecipientsRecursive(LPADRBOOK lpAddrBook,
    IMessage *lpMessage, IMAPITable *lpTable,
    LPSRestriction lpEntryRestriction, ULONG ulRecipType,
    list<SBinary> *lpExpandedGroups, bool recurrence = true)
{
	HRESULT			hr = hrSuccess;
	ULONG			ulObj = 0;
	bool			bExpandSub = recurrence;
	static constexpr const SizedSPropTagArray(7, sptaColumns) =
		{7, {PR_ROWID, PR_DISPLAY_NAME_W, PR_SMTP_ADDRESS_W,
		PR_RECIPIENT_TYPE, PR_OBJECT_TYPE, PR_DISPLAY_TYPE, PR_ENTRYID}};

	hr = lpTable->SetColumns(sptaColumns, 0);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "ExpandRecipientsRecursive(): SetColumns failed %x", hr);
		return hr;
	}

	while (true) {
		memory_ptr<SPropValue> lpSMTPAddress;
		rowset_ptr lpsRowSet;
		/* Request group from table */
		hr = lpTable->QueryRows(1, 0, &~lpsRowSet);
		if (hr != hrSuccess) {
			g_lpLogger->Log(EC_LOGLEVEL_ERROR, "ExpandRecipientsRecursive(): QueryRows failed %x", hr);
			return hr;
		}

		if (lpsRowSet->cRows != 1)
			break;

		/* From this point on we use 'continue' when something fails,
		 * since all errors are related to the current entry and we should
		 * make sure we resolve as many recipients as possible. */

		auto lpRowId = PCpropFindProp(lpsRowSet->aRow[0].lpProps, lpsRowSet->aRow[0].cValues, PR_ROWID);
		auto lpEntryId = PCpropFindProp(lpsRowSet->aRow[0].lpProps, lpsRowSet->aRow[0].cValues, PR_ENTRYID);
		auto lpDisplayType = PCpropFindProp(lpsRowSet->aRow[0].lpProps, lpsRowSet->aRow[0].cValues, PR_DISPLAY_TYPE);
		auto lpObjectType = PCpropFindProp(lpsRowSet->aRow[0].lpProps, lpsRowSet->aRow[0].cValues, PR_OBJECT_TYPE);
		auto lpRecipType = PCpropFindProp(lpsRowSet->aRow[0].lpProps, lpsRowSet->aRow[0].cValues, PR_RECIPIENT_TYPE);
		auto lpDisplayName = PCpropFindProp(lpsRowSet->aRow[0].lpProps, lpsRowSet->aRow[0].cValues, PR_DISPLAY_NAME_W);
		auto lpEmailAddress = PCpropFindProp(lpsRowSet->aRow[0].lpProps, lpsRowSet->aRow[0].cValues, PR_SMTP_ADDRESS_W);

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

			SizedADRLIST(1, sRowSMTProwSet);
			SPropValue p[4];

			sRowSMTProwSet.cEntries = 1;
			sRowSMTProwSet.aEntries[0].cValues = 4;
			sRowSMTProwSet.aEntries[0].rgPropVals = p;
			p[0].ulPropTag = PR_EMAIL_ADDRESS_W;
			p[0].Value.lpszW = lpEmailAddress->Value.lpszW;
			p[1].ulPropTag = PR_SMTP_ADDRESS_W;
			p[1].Value.lpszW = lpEmailAddress->Value.lpszW;
			p[2].ulPropTag = PR_RECIPIENT_TYPE;
			p[2].Value.ul = ulRecipType; /* Inherit from parent group */
			p[3].ulPropTag = PR_DISPLAY_NAME_W;
			p[3].Value.lpszW = lpDisplayName->Value.lpszW;
			hr = lpMessage->ModifyRecipients(MODRECIP_ADD, sRowSMTProwSet);
			if (hr != hrSuccess) {
				g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Unable to add e-mail address of %ls from group: %s (%x)",
					lpEmailAddress->Value.lpszW, GetMAPIErrorMessage(hr), hr);
				continue;
			}
		} else {
			SBinary sEntryId;
			object_ptr<IMAPITable> lpContentsTable;
			object_ptr<IDistList> lpDistlist;

			/* If we should recur further, just remove the group from the recipients list */
			if (!recurrence)
				goto remove_group;

			/* Only continue when this group has not yet been expanded previously */
			if (find(lpExpandedGroups->begin(), lpExpandedGroups->end(), lpEntryId->Value.bin) != lpExpandedGroups->end())
				goto remove_group;
			hr = lpAddrBook->OpenEntry(lpEntryId->Value.bin.cb, reinterpret_cast<ENTRYID *>(lpEntryId->Value.bin.lpb), nullptr, 0, &ulObj, &~lpDistlist);
			if (hr != hrSuccess)
				continue;
				
			if(ulObj != MAPI_DISTLIST)
				continue;
				
			/* Never expand groups with an email address. The whole point of the email address is that it can be used
			 * as a single entity */
			if (HrGetOneProp(lpDistlist, PR_SMTP_ADDRESS_W, &~lpSMTPAddress) == hrSuccess &&
			    wcslen(lpSMTPAddress->Value.lpszW) > 0)
				continue;
			hr = lpDistlist->GetContentsTable(MAPI_UNICODE, &~lpContentsTable);
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
						   wcscasecmp(lpDisplayName->Value.lpszW, L"Everyone") == 0);
			// @todo find everyone using it's static entryid?

			/* Start/Continue recursion */
			hr = ExpandRecipientsRecursive(lpAddrBook, lpMessage, lpContentsTable,
										   lpEntryRestriction, ulRecipType, lpExpandedGroups, bExpandSub);
			/* Ignore errors */

remove_group:
			/* Only delete row when the rowid is present */
			if (!lpRowId)
				continue;

			hr = lpMessage->ModifyRecipients(MODRECIP_REMOVE,
			     reinterpret_cast<ADRLIST *>(lpsRowSet.get()));
			if (hr != hrSuccess) {
				g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Unable to remove group %ls from recipient list: %s (%x).",
					lpDisplayName->Value.lpszW, GetMAPIErrorMessage(hr), hr);
				continue;
			}
		}
	}
	return hrSuccess;
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
static HRESULT ExpandRecipients(LPADRBOOK lpAddrBook, IMessage *lpMessage)
{
	HRESULT hr = hrSuccess;
	list<SBinary> lExpandedGroups;
	object_ptr<IMAPITable> lpTable;
	memory_ptr<SRestriction> lpRestriction, lpEntryRestriction;
	/*
	 * Setup group restriction:
	 * PR_OBJECT_TYPE == MAPI_DISTLIST && PR_ADDR_TYPE == "ZARAFA"
	 */
	hr = MAPIAllocateBuffer(sizeof(SRestriction), &~lpRestriction);
	if(hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "ExpandRecipients(): MAPIAllocateBuffer failed %x", hr);
		goto exit;
	}

	hr = MAPIAllocateMore(sizeof(SRestriction) * 2, lpRestriction, (LPVOID*)&lpRestriction->res.resAnd.lpRes);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "ExpandRecipients(): MAPIAllocateMore failed(1) %x", hr);
		goto exit;
	}

	lpRestriction->rt = RES_AND;
	lpRestriction->res.resAnd.cRes = 2;

	hr = MAPIAllocateMore(sizeof(SPropValue), lpRestriction, (LPVOID*)&lpRestriction->res.resAnd.lpRes[0].res.resProperty.lpProp);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "ExpandRecipients(): MAPIAllocateMore failed(2) %x", hr);
		goto exit;
	}

	lpRestriction->res.resAnd.lpRes[0].rt = RES_PROPERTY;
	lpRestriction->res.resAnd.lpRes[0].res.resProperty.relop = RELOP_EQ;
	lpRestriction->res.resAnd.lpRes[0].res.resProperty.ulPropTag = PR_OBJECT_TYPE;
	lpRestriction->res.resAnd.lpRes[0].res.resProperty.lpProp->ulPropTag = PR_OBJECT_TYPE;
	lpRestriction->res.resAnd.lpRes[0].res.resProperty.lpProp->Value.ul = MAPI_DISTLIST;

	hr = MAPIAllocateMore(sizeof(SPropValue), lpRestriction, (LPVOID*)&lpRestriction->res.resAnd.lpRes[1].res.resProperty.lpProp);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "ExpandRecipients(): MAPIAllocateMore failed(3) %x", hr);
		goto exit;
	}

	lpRestriction->res.resAnd.lpRes[1].rt = RES_PROPERTY;
	lpRestriction->res.resAnd.lpRes[1].res.resProperty.relop = RELOP_EQ;
	lpRestriction->res.resAnd.lpRes[1].res.resProperty.ulPropTag = PR_ADDRTYPE_W;
	lpRestriction->res.resAnd.lpRes[1].res.resProperty.lpProp->ulPropTag = PR_ADDRTYPE_W;
	lpRestriction->res.resAnd.lpRes[1].res.resProperty.lpProp->Value.lpszW = const_cast<wchar_t *>(L"ZARAFA");

	/*
	 * Setup entry restriction:
	 * PR_ADDR_TYPE == "ZARAFA"
	 */
	hr = MAPIAllocateBuffer(sizeof(SRestriction), &~lpEntryRestriction);
	if(hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "ExpandRecipients(): MAPIAllocateBuffer failed %x", hr);
		goto exit;
	}

	hr = MAPIAllocateMore(sizeof(SPropValue), lpEntryRestriction, (LPVOID*)&lpEntryRestriction->res.resProperty.lpProp);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "ExpandRecipients(): MAPIAllocateMore failed(4) %x", hr);
		goto exit;
	}

	lpEntryRestriction->rt = RES_PROPERTY;
	lpEntryRestriction->res.resProperty.relop = RELOP_EQ;
	lpEntryRestriction->res.resProperty.ulPropTag = PR_ADDRTYPE_W;
	lpEntryRestriction->res.resProperty.lpProp->ulPropTag = PR_ADDRTYPE_W;
	lpEntryRestriction->res.resProperty.lpProp->Value.lpszW = const_cast<wchar_t *>(L"ZARAFA");
	hr = lpMessage->GetRecipientTable(MAPI_UNICODE, &~lpTable);
	if(hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "ExpandRecipients(): GetRecipientTable failed %x", hr);
		goto exit;
	}

	/* The first table we send with ExpandRecipientsRecursive() is the RecipientTable itself,
	 * we need to put a restriction on this table since the first time only the groups
	 * should be added to the recipients list. Subsequent calls to ExpandRecipientsRecursive()
	 * will send the group member table and will correct add the members to the recipients
	 * table. */
	hr = lpTable->Restrict(lpRestriction, 0);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "ExpandRecipients(): Restrict failed %x", hr);
		goto exit;
	}

	/* ExpandRecipientsRecursive() will run recursively expanding each group
	 * it finds including all subgroups. It will use the lExpandedGroups list
	 * to protect itself for circular subgroup membership */
	hr = ExpandRecipientsRecursive(lpAddrBook, lpMessage, lpTable, lpEntryRestriction, MAPI_TO, &lExpandedGroups); 
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "ExpandRecipients(): ExpandRecipientsRecursive failed %x", hr);
		goto exit;
	}

exit:
	for (const auto &g : lExpandedGroups)
		MAPIFreeBuffer(g.lpb);
	return hr;
}

/**
 * Rewrites a FAX:number "email address" to a sendable email address.
 *
 * @param[in]	lpMAPISession	The session of the user
 * @param[in]	lpMessage		The message to send
 * @return		HRESULT
 */
static HRESULT RewriteRecipients(LPMAPISESSION lpMAPISession,
    IMessage *lpMessage)
{
	HRESULT		hr = hrSuccess;
	object_ptr<IMAPITable> lpTable;
	memory_ptr<SPropTagArray> lpRecipColumns;

	const char	*const lpszFaxDomain = g_lpConfig->GetSetting("fax_domain");
	const char	*const lpszFaxInternational = g_lpConfig->GetSetting("fax_international");
	string		strFaxMail;
	wstring		wstrFaxMail, wstrOldFaxMail;
	ULONG		ulObjType;
	ULONG		cValues;

	// contab email_offset: 0: business, 1: home, 2: primary (outlook uses string 'other')
	static constexpr const SizedSPropTagArray(3, sptaFaxNumbers) =
		{ 3, {PR_BUSINESS_FAX_NUMBER_A, PR_HOME_FAX_NUMBER_A,
		PR_PRIMARY_FAX_NUMBER_A}};

	if (!lpszFaxDomain || strcmp(lpszFaxDomain, "") == 0)
		return hr;
	hr = lpMessage->GetRecipientTable(MAPI_UNICODE, &~lpTable);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "RewriteRecipients(): GetRecipientTable failed %x", hr);
		return hr;
	}

	// we need all columns when rewriting FAX to SMTP
	hr = lpTable->QueryColumns(TBL_ALL_COLUMNS, &~lpRecipColumns);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "RewriteRecipients(): QueryColumns failed %x", hr);
		return hr;
	}
	
	hr = lpTable->SetColumns(lpRecipColumns, 0);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "RewriteRecipients(): SetColumns failed %x", hr);
		return hr;
	}

	while (TRUE) {
		rowset_ptr lpRowSet;
		hr = lpTable->QueryRows(1, 0, &~lpRowSet);
		if (hr != hrSuccess) {
			g_lpLogger->Log(EC_LOGLEVEL_ERROR, "RewriteRecipients(): QueryRows failed %x", hr);
			return hr;
		}

		if (lpRowSet->cRows == 0)
			break;

		auto lpEmailAddress = PpropFindProp(lpRowSet->aRow[0].lpProps, lpRowSet->aRow[0].cValues, PR_EMAIL_ADDRESS_W);
		auto lpEmailName = PCpropFindProp(lpRowSet->aRow[0].lpProps, lpRowSet->aRow[0].cValues, PR_DISPLAY_NAME_W);
		auto lpAddrType = PpropFindProp(lpRowSet->aRow[0].lpProps, lpRowSet->aRow[0].cValues, PR_ADDRTYPE_W);
		auto lpEntryID = PpropFindProp(lpRowSet->aRow[0].lpProps, lpRowSet->aRow[0].cValues, PR_ENTRYID);

		if (!(lpEmailAddress && lpAddrType && lpEntryID && lpEmailName))
			continue;

		if (wcscmp(lpAddrType->Value.lpszW, L"FAX") != 0)
			continue;

		// rewrite FAX address to <number>@<faxdomain>
		wstring wstrName, wstrType, wstrEmailAddress;
		memory_ptr<ENTRYID> lpNewEntryID;
		memory_ptr<SPropValue> lpFaxNumbers;
		ULONG cbNewEntryID;

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
				/*hr = MAPI_E_INVALID_PARAMETER;*/
				g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Unable to convert FAX recipient, using %ls", lpEmailAddress->Value.lpszW);
				continue;
			}

			// 0..2 == reply to email offsets
			// 3..5 == fax email offsets
			lpContabEntryID->email_offset -= 3;

			object_ptr<IMailUser> lpFaxMailuser;
			hr = lpMAPISession->OpenEntry(lpContabEntryID->cbeid, reinterpret_cast<ENTRYID *>(lpContabEntryID->abeid), nullptr, 0, &ulObjType, &~lpFaxMailuser);
			if (hr != hrSuccess) {
				g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Unable to convert FAX recipient, using %ls: %s (%x)",
					lpEmailAddress->Value.lpszW, GetMAPIErrorMessage(hr), hr);
				continue;
			}
			hr = lpFaxMailuser->GetProps(sptaFaxNumbers, 0, &cValues, &~lpFaxNumbers);
			if (FAILED(hr)) {
				g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Unable to convert FAX recipient, using %ls: %s (%x)",
					lpEmailAddress->Value.lpszW, GetMAPIErrorMessage(hr), hr);
				continue;
			}
			if (lpFaxNumbers[lpContabEntryID->email_offset].ulPropTag != sptaFaxNumbers.aulPropTag[lpContabEntryID->email_offset]) {
				g_lpLogger->Log(EC_LOGLEVEL_ERROR, "No suitable FAX number found, using %ls", lpEmailAddress->Value.lpszW);
				continue;
			}
			strFaxMail = lpFaxNumbers[lpContabEntryID->email_offset].Value.lpszA;
		}
		strFaxMail += string("@") + lpszFaxDomain;
		if (strFaxMail[0] == '+' && lpszFaxInternational != nullptr)
			strFaxMail = lpszFaxInternational + strFaxMail.substr(1, strFaxMail.length());

		wstrFaxMail = convert_to<wstring>(strFaxMail);
		wstrOldFaxMail = lpEmailAddress->Value.lpszW; // keep old string for logging
		// hack values in lpRowSet
		lpEmailAddress->Value.lpszW = (WCHAR*)wstrFaxMail.c_str();
		lpAddrType->Value.lpszW = const_cast<wchar_t *>(L"SMTP");
		// old value is stuck to the row allocation, so we can override it, but we also must free the new!
		ECCreateOneOff((LPTSTR)lpEmailName->Value.lpszW, (LPTSTR)L"SMTP", (LPTSTR)wstrFaxMail.c_str(), MAPI_UNICODE, &cbNewEntryID, &~lpNewEntryID);
		lpEntryID->Value.bin.lpb = reinterpret_cast<BYTE *>(lpNewEntryID.get());
		lpEntryID->Value.bin.cb = cbNewEntryID;

		hr = lpMessage->ModifyRecipients(MODRECIP_MODIFY,
		     reinterpret_cast<ADRLIST *>(lpRowSet.get()));
		if (hr != hrSuccess) {
			g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Unable to set new FAX mail address for '%ls' to '%s': %s (%x)",
				wstrOldFaxMail.c_str(), strFaxMail.c_str(), GetMAPIErrorMessage(hr), hr);
			continue;
		}

		g_lpLogger->Log(EC_LOGLEVEL_INFO, "Using new FAX mail address %s", strFaxMail.c_str());
	}
	return hrSuccess;
}

/**
 * Make the recipient table in the message unique. Key is the PR_SMTP_ADDRESS and PR_RECIPIENT_TYPE (To/Cc/Bcc).
 *
 * @param[in]	lpMessage	The message to fix the recipient table for.
 * @return		HRESULT
 */
static HRESULT UniqueRecipients(IMessage *lpMessage)
{
	HRESULT			hr = hrSuccess;
	object_ptr<IMAPITable> lpTable;
	string			strEmail;
	ULONG			ulRecipType = 0;
	static constexpr const SizedSPropTagArray(3, sptaColumns) =
		{3, {PR_ROWID, PR_SMTP_ADDRESS_A, PR_RECIPIENT_TYPE}};
	static constexpr const SizedSSortOrderSet(2, sosOrder) = {
		2, 0, 0, {
			{ PR_SMTP_ADDRESS_A, TABLE_SORT_ASCEND },
			{ PR_RECIPIENT_TYPE, TABLE_SORT_ASCEND },
		}
	};

	hr = lpMessage->GetRecipientTable(0, &~lpTable);
	if (hr != hrSuccess)
		return hr;
	hr = lpTable->SetColumns(sptaColumns, 0);
	if (hr != hrSuccess)
		return hr;
	hr = lpTable->SortTable(sosOrder, 0);
	if (hr != hrSuccess)
		return hr;

	while (TRUE) {
		rowset_ptr lpRowSet;
		hr = lpTable->QueryRows(1, 0, &~lpRowSet);
		if (hr != hrSuccess)
			return hr;
		if (lpRowSet->cRows == 0)
			break;

		auto lpEmailAddress = PCpropFindProp(lpRowSet->aRow[0].lpProps, lpRowSet->aRow[0].cValues, PR_SMTP_ADDRESS_A);
		auto lpRecipType = PCpropFindProp(lpRowSet->aRow[0].lpProps, lpRowSet->aRow[0].cValues, PR_RECIPIENT_TYPE);

		if (!lpEmailAddress || !lpRecipType)
			continue;

		/* Filter To, Cc, Bcc individually */
		if (strEmail == lpEmailAddress->Value.lpszA && ulRecipType == lpRecipType->Value.ul) {
			hr = lpMessage->ModifyRecipients(MODRECIP_REMOVE,
			     reinterpret_cast<ADRLIST *>(lpRowSet.get()));
			if (hr != hrSuccess)
				g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Failed to remove duplicate entry: %s (%x)",
					GetMAPIErrorMessage(hr), hr);
		} else {
			strEmail = string(lpEmailAddress->Value.lpszA);
			ulRecipType = lpRecipType->Value.ul;
		}
	}
	return hrSuccess;
}

static HRESULT RewriteQuotedRecipients(IMessage *lpMessage)
{
	HRESULT			hr = hrSuccess;
	object_ptr<IMAPITable> lpTable;
	wstring			strEmail;
	static constexpr const SizedSPropTagArray(3, sptaColumns) =
		{3, {PR_ROWID, PR_EMAIL_ADDRESS_W, PR_RECIPIENT_TYPE}};

	hr = lpMessage->GetRecipientTable(0, &~lpTable);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "RewriteQuotedRecipients(): GetRecipientTable failed %x", hr);
		return hr;
	}
	hr = lpTable->SetColumns(sptaColumns, 0);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "RewriteQuotedRecipients(): SetColumns failed %x", hr);
		return hr;
	}

	while (TRUE) {
		rowset_ptr lpRowSet;
		hr = lpTable->QueryRows(1, 0, &~lpRowSet);
		if (hr != hrSuccess) {
			g_lpLogger->Log(EC_LOGLEVEL_ERROR, "RewriteQuotedRecipients(): QueryRows failed %x", hr);
			return hr;
		}

		if (lpRowSet->cRows == 0)
			break;

		auto lpEmailAddress = PpropFindProp(lpRowSet->aRow[0].lpProps, lpRowSet->aRow[0].cValues, PR_EMAIL_ADDRESS_W);
		auto lpRecipType = PCpropFindProp(lpRowSet->aRow[0].lpProps, lpRowSet->aRow[0].cValues, PR_RECIPIENT_TYPE);

		if (!lpEmailAddress || !lpRecipType)
			continue;
   
        strEmail = lpEmailAddress->Value.lpszW;
        if((strEmail[0] == '\'' && strEmail[strEmail.size()-1] == '\'') ||
           (strEmail[0] == '"' && strEmail[strEmail.size()-1] == '"')) {

            g_lpLogger->Log(EC_LOGLEVEL_INFO, "Rewrite quoted recipient: %ls", strEmail.c_str());

            strEmail = strEmail.substr(1, strEmail.size()-2);
            lpEmailAddress->Value.lpszW = (WCHAR *)strEmail.c_str();
			hr = lpMessage->ModifyRecipients(MODRECIP_MODIFY,
			     reinterpret_cast<ADRLIST *>(lpRowSet.get()));
			if (hr != hrSuccess) {
				g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Failed to rewrite quoted recipient: %s (%x)",
					GetMAPIErrorMessage(hr), hr);
				return hr;
			}
		}
	}
	return hrSuccess;
}
/**
 * Removes all MAPI_P1 marked recipients from a message.
 *
 * @param[in]	lpMessage	Message to remove MAPI_P1 recipients from
 * @return		HRESULT
 */
static HRESULT RemoveP1Recipients(IMessage *lpMessage)
{
	HRESULT hr = hrSuccess;
	object_ptr<IMAPITable> lpTable;
	rowset_ptr lpRows;
	SPropValue sPropRestrict;
	
	sPropRestrict.ulPropTag = PR_RECIPIENT_TYPE;
	sPropRestrict.Value.ul = MAPI_P1;

	hr = lpMessage->GetRecipientTable(0, &~lpTable);
	if(hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "RemoveP1Recipients(): GetRecipientTable failed %x", hr);
		return hr;
	}
	
	hr = ECPropertyRestriction(RELOP_EQ, PR_RECIPIENT_TYPE,
	     &sPropRestrict, ECRestriction::Cheap).RestrictTable(lpTable, 0);
	if(hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "RemoveP1Recipients(): Restrict failed %x", hr);
		return hr;
	}
	hr = lpTable->QueryRows(-1, 0, &~lpRows);
	if(hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "RemoveP1Recipients(): QueryRows failed %x", hr);
		return hr;
	}
	hr = lpMessage->ModifyRecipients(MODRECIP_REMOVE, reinterpret_cast<ADRLIST *>(lpRows.get()));
	if (hr != hrSuccess)
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "RemoveP1Recipients(): ModifyRecipients failed %x", hr);
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
 * @param lpMailer Mailer object used to send the lpMessage message containing the errors
 * @param lpMessage Failed message
 */
HRESULT SendUndeliverable(ECSender *lpMailer, IMsgStore *lpStore,
    IMessage *lpMessage)
{
	HRESULT		hr = hrSuccess;
	object_ptr<IMAPIFolder> lpInbox;
	object_ptr<IMessage> lpErrorMsg;
	memory_ptr<ENTRYID> lpEntryID;
	ULONG			cbEntryID;
	ULONG			ulObjType;
	wstring			newbody;
	memory_ptr<SPropValue> lpPropValue, lpPropValueAttach, lpPropArrayOriginal;
	unsigned int	ulPropPos = 0;
	FILETIME		ft;
	object_ptr<IAttach> lpAttach;
	object_ptr<IMessage> lpOriginalMessage;
	ULONG			cValuesOriginal = 0;
	unsigned int	ulPropModsPos;
	object_ptr<IMAPITable> lpTableMods;
	ULONG			ulRows = 0;
	ULONG			cEntries = 0;
	string			strName, strType, strEmail;

	// CopyTo() var's
	unsigned int	ulPropAttachPos;
	ULONG			ulAttachNum;

	const std::vector<sFailedRecip> &temporaryFailedRecipients = lpMailer->getTemporaryFailedRecipients();
	const std::vector<sFailedRecip> &permanentFailedRecipients = lpMailer->getPermanentFailedRecipients();

	enum eORPos {
		OR_DISPLAY_TO, OR_DISPLAY_CC, OR_DISPLAY_BCC, OR_SEARCH_KEY, OR_SENDER_ADDRTYPE,
		OR_SENDER_EMAIL_ADDRESS, OR_SENDER_ENTRYID, OR_SENDER_NAME,
		OR_SENDER_SEARCH_KEY, OR_SENT_REPRESENTING_ADDRTYPE,
		OR_SENT_REPRESENTING_EMAIL_ADDRESS, OR_SENT_REPRESENTING_ENTRYID,
		OR_SENT_REPRESENTING_NAME, OR_SENT_REPRESENTING_SEARCH_KEY,
		OR_SUBJECT, OR_CLIENT_SUBMIT_TIME
	};

	// These props are on purpose without _A and _W
	static constexpr const SizedSPropTagArray(16, sPropsOriginal) = {
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
	static constexpr const SizedSPropTagArray(7, sPropTagRecipient) = {
		7,
		{ PR_RECIPIENT_TYPE, PR_DISPLAY_NAME, PR_DISPLAY_TYPE,
		  PR_ADDRTYPE, PR_EMAIL_ADDRESS,
		  PR_ENTRYID, PR_SEARCH_KEY }
	};

	// open inbox
	hr = lpStore->GetReceiveFolder((LPTSTR)"IPM", 0, &cbEntryID, &~lpEntryID, NULL);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_WARNING, "Unable to resolve incoming folder, error code: 0x%08X", hr);
		return hr;
	}
	hr = lpStore->OpenEntry(cbEntryID, lpEntryID, &IID_IMAPIFolder, MAPI_MODIFY, &ulObjType, &~lpInbox);
	if (hr != hrSuccess || ulObjType != MAPI_FOLDER) {
		g_lpLogger->Log(EC_LOGLEVEL_WARNING, "Unable to open inbox folder, error code: 0x%08X", hr);
		return MAPI_E_NOT_FOUND;
	}

	// make new message in inbox
	hr = lpInbox->CreateMessage(nullptr, 0, &~lpErrorMsg);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_WARNING, "Unable to create undeliverable message, error code: 0x%08X", hr);
		return hr;
	}

	// Get properties from the original message
	hr = lpMessage->GetProps(sPropsOriginal, 0, &cValuesOriginal, &~lpPropArrayOriginal);
	if (FAILED(hr)) {
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "SendUndeliverable(): GetPRops failed %x", hr);
		return hr;
	}
	hr = MAPIAllocateBuffer(sizeof(SPropValue) * 34, &~lpPropValue);
	if(hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "SendUndeliverable(): MAPIAllocateBuffers failed %x", hr);
		return hr;
	}

	// Subject
	lpPropValue[ulPropPos].ulPropTag = PR_SUBJECT_W;
	lpPropValue[ulPropPos++].Value.lpszW = const_cast<wchar_t *>(L"Undelivered Mail Returned to Sender");

	// Message flags
	lpPropValue[ulPropPos].ulPropTag = PR_MESSAGE_FLAGS;
	lpPropValue[ulPropPos++].Value.ul = 0;

	// Message class
	lpPropValue[ulPropPos].ulPropTag = PR_MESSAGE_CLASS_W;
	lpPropValue[ulPropPos++].Value.lpszW = const_cast<wchar_t *>(L"REPORT.IPM.Note.NDR");

	// Get the time to add to the message as PR_CLIENT_SUBMIT_TIME
	GetSystemTimeAsFileTime(&ft);

	// Submit time
	lpPropValue[ulPropPos].ulPropTag = PR_CLIENT_SUBMIT_TIME;
	lpPropValue[ulPropPos++].Value.ft = ft;

	// Delivery time
	lpPropValue[ulPropPos].ulPropTag = PR_MESSAGE_DELIVERY_TIME;
	lpPropValue[ulPropPos++].Value.ft = ft;

	lpPropValue[ulPropPos].ulPropTag = PR_SENDER_NAME_W;
	lpPropValue[ulPropPos++].Value.lpszW = (LPWSTR)L"Mail Delivery System";

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

	// Add the original message into the errorMessage
	hr = lpErrorMsg->CreateAttach(nullptr, 0, &ulAttachNum, &~lpAttach);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_WARNING, "Unable to create attachment, error code: 0x%08X", hr);
		return hr;
	}
	hr = lpAttach->OpenProperty(PR_ATTACH_DATA_OBJ, &IID_IMessage, 0, MAPI_CREATE | MAPI_MODIFY, &~lpOriginalMessage);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "SendUndeliverable(): OpenProperty failed %x", hr);
		return hr;
	}

	hr = lpMessage->CopyTo(0, NULL, NULL, 0, NULL, &IID_IMessage, (LPVOID)lpOriginalMessage, 0, NULL);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "SendUndeliverable(): CopyTo failed %x", hr);
		return hr;
	}

	// Remove MAPI_P1 recipients. These are present when you resend a resent message. They shouldn't be there since
	// we should be resending the original message
	hr = RemoveP1Recipients(lpOriginalMessage);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "SendUndeliverable(): RemoveP1Recipients failed %x", hr);
		return hr;
	}

	hr = lpOriginalMessage->SaveChanges(KEEP_OPEN_READWRITE);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "SendUndeliverable(): SaveChanges failed %x", hr);
		return hr;
	}

	ulPropAttachPos = 0;
	hr = MAPIAllocateBuffer(sizeof(SPropValue) * 4, &~lpPropValueAttach);
	if (hr != hrSuccess)
		return hr;

	lpPropValueAttach[ulPropAttachPos].ulPropTag = PR_ATTACH_METHOD;
	lpPropValueAttach[ulPropAttachPos++].Value.ul = ATTACH_EMBEDDED_MSG;

	lpPropValueAttach[ulPropAttachPos].ulPropTag = PR_ATTACH_MIME_TAG_W;
	lpPropValueAttach[ulPropAttachPos++].Value.lpszW = const_cast<wchar_t *>(L"message/rfc822");

	if(PROP_TYPE(lpPropArrayOriginal[OR_SUBJECT].ulPropTag) != PT_ERROR) {
		lpPropValueAttach[ulPropAttachPos].ulPropTag = CHANGE_PROP_TYPE(PR_DISPLAY_NAME, PROP_TYPE(lpPropArrayOriginal[OR_SUBJECT].ulPropTag));
		lpPropValueAttach[ulPropAttachPos++].Value.lpszA = lpPropArrayOriginal[OR_SUBJECT].Value.lpszA;
	}

	lpPropValueAttach[ulPropAttachPos].ulPropTag = PR_RENDERING_POSITION;
	lpPropValueAttach[ulPropAttachPos++].Value.ul = -1;

	hr = lpAttach->SetProps(ulPropAttachPos, lpPropValueAttach, NULL);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "SendUndeliverable(): SetProps failed %x", hr);
		return hr;
	}

	hr = lpAttach->SaveChanges(KEEP_OPEN_READWRITE);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "SendUndeliverable(): SaveChanges failed %x", hr);
		return hr;
	}

	// add failed recipients to error report
	hr = lpMessage->GetRecipientTable(MAPI_UNICODE, &~lpTableMods);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "SendUndeliverable(): GetRecipientTable failed %x", hr);
		return hr;
	}
	hr = lpTableMods->SetColumns(sPropTagRecipient, TBL_BATCH);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "SendUndeliverable(): SetColumns failed %x", hr);
		return hr;
	}

	hr = lpTableMods->GetRowCount(0, &ulRows);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "SendUndeliverable(): GetRowCount failed %x", hr);
		return hr;
	}

	if (ulRows == 0 || (permanentFailedRecipients.empty() && temporaryFailedRecipients.empty())) {
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
			rowset_ptr lpRows;
			hr = lpTableMods->QueryRows(-1, 0, &~lpRows);
			if (hr != hrSuccess) {
				g_lpLogger->Log(EC_LOGLEVEL_ERROR, "SendUndeliverable(): QueryRows failed %x", hr);
				return hr;
			}
			hr = lpErrorMsg->ModifyRecipients(MODRECIP_ADD, reinterpret_cast<ADRLIST *>(lpRows.get()));
			if (hr != hrSuccess) {
				g_lpLogger->Log(EC_LOGLEVEL_ERROR, "SendUndeliverable(): ModifyRecipients failed %x", hr);
				return hr;
			}
		}
	}
	else if (ulRows > 0)
	{
		convert_context converter;
		newbody = L"Unfortunately, I was unable to deliver your mail to the/some of the recipient(s).\n";
		newbody.append(L"You may need to contact your e-mail administrator to solve this problem.\n");

		if (!temporaryFailedRecipients.empty()) {
			newbody.append(L"\nRecipients that will be retried:\n");

			for (size_t i = 0; i < temporaryFailedRecipients.size(); ++i) {
				const sFailedRecip &cur = temporaryFailedRecipients.at(i);

				newbody.append(L"\t");
				newbody.append(cur.strRecipName.c_str());
				newbody.append(L" <");
				newbody.append(converter.convert_to<wchar_t *>(cur.strRecipEmail));
				newbody.append(L">\n");
			}
		}

		if (!permanentFailedRecipients.empty()) {
			newbody.append(L"\nRecipients that failed permanently:\n");

			for (size_t i = 0; i < permanentFailedRecipients.size(); ++i) {
				const sFailedRecip &cur = permanentFailedRecipients.at(i);

				newbody.append(L"\t");
				newbody.append(cur.strRecipName.c_str());
				newbody.append(L" <");
				newbody.append(converter.convert_to<wchar_t *>(cur.strRecipEmail));
				newbody.append(L">\n");
			}
		}

		lpPropValue[ulPropPos].ulPropTag = PR_BODY_W;
		lpPropValue[ulPropPos++].Value.lpszW = const_cast<wchar_t *>(newbody.c_str());

		// Only some recipients failed, so add only failed recipients to the MDN message. This causes
		// resends only to go to those recipients. This means we should add all error recipients to the
		// recipient list of the MDN message. 
		adrlist_ptr lpMods;
		hr = MAPIAllocateBuffer(CbNewADRLIST(temporaryFailedRecipients.size()), &~lpMods);
		if (hr != hrSuccess)
			return hr;

		lpMods->cEntries = 0;
		for (size_t j = 0; j < temporaryFailedRecipients.size(); ++j) {
			const sFailedRecip &cur = temporaryFailedRecipients.at(j);

			if ((hr = MAPIAllocateBuffer(sizeof(SPropValue) * 10, (void**)&lpMods->aEntries[cEntries].rgPropVals)) != hrSuccess)
				return hr;

			ulPropModsPos = 0;
			lpMods->cEntries = cEntries;

			lpMods->aEntries[cEntries].rgPropVals[ulPropModsPos].ulPropTag = PR_RECIPIENT_TYPE;
			lpMods->aEntries[cEntries].rgPropVals[ulPropModsPos++].Value.ul = MAPI_TO;

			lpMods->aEntries[cEntries].rgPropVals[ulPropModsPos].ulPropTag = PR_EMAIL_ADDRESS_A;
			lpMods->aEntries[cEntries].rgPropVals[ulPropModsPos++].Value.lpszA = const_cast<char *>(cur.strRecipEmail.c_str());

			lpMods->aEntries[cEntries].rgPropVals[ulPropModsPos].ulPropTag = PR_ADDRTYPE_W;
			lpMods->aEntries[cEntries].rgPropVals[ulPropModsPos++].Value.lpszW = const_cast<wchar_t *>(L"SMTP");

			lpMods->aEntries[cEntries].rgPropVals[ulPropModsPos].ulPropTag = PR_DISPLAY_NAME_W;

			if (!cur.strRecipName.empty())
				lpMods->aEntries[cEntries].rgPropVals[ulPropModsPos++].Value.lpszW = const_cast<wchar_t *>(cur.strRecipName.c_str());
			else
				lpMods->aEntries[cEntries].rgPropVals[ulPropModsPos++].Value.lpszW = converter.convert_to<wchar_t *>(cur.strRecipEmail);

			lpMods->aEntries[cEntries].rgPropVals[ulPropModsPos].ulPropTag = PR_REPORT_TEXT_A;
			lpMods->aEntries[cEntries].rgPropVals[ulPropModsPos++].Value.lpszA = const_cast<char *>(cur.strSMTPResponse.c_str());

			lpMods->aEntries[cEntries].rgPropVals[ulPropModsPos].ulPropTag = PR_REPORT_TIME;
			lpMods->aEntries[cEntries].rgPropVals[ulPropModsPos++].Value.ft = ft;

			lpMods->aEntries[cEntries].rgPropVals[ulPropModsPos].ulPropTag = PR_TRANSMITABLE_DISPLAY_NAME_A;
			lpMods->aEntries[cEntries].rgPropVals[ulPropModsPos++].Value.lpszA = const_cast<char *>(cur.strRecipEmail.c_str());

			lpMods->aEntries[cEntries].rgPropVals[ulPropModsPos].ulPropTag = 0x0C200003; // PR_NDR_STATUS_CODE;
			lpMods->aEntries[cEntries].rgPropVals[ulPropModsPos++].Value.ul = cur.ulSMTPcode;

			lpMods->aEntries[cEntries].rgPropVals[ulPropModsPos].ulPropTag = PR_NDR_DIAG_CODE;
			lpMods->aEntries[cEntries].rgPropVals[ulPropModsPos++].Value.ul = MAPI_DIAG_MAIL_RECIPIENT_UNKNOWN;

			lpMods->aEntries[cEntries].rgPropVals[ulPropModsPos].ulPropTag = PR_NDR_REASON_CODE;
			lpMods->aEntries[cEntries].rgPropVals[ulPropModsPos++].Value.ul = MAPI_REASON_TRANSFER_FAILED;

			lpMods->aEntries[cEntries].cValues = ulPropModsPos;
			++cEntries;
		}

		lpMods->cEntries = cEntries;

		hr = lpErrorMsg->ModifyRecipients(MODRECIP_ADD, lpMods);
		if (hr != hrSuccess)
			return hr;
	}

	// Add properties
	hr = lpErrorMsg->SetProps(ulPropPos, lpPropValue, NULL);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "SendUndeliverable(): SetProps failed %x", hr);
		return hr;
	}

	// save message
	hr = lpErrorMsg->SaveChanges(KEEP_OPEN_READONLY);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Unable to commit message: 0x%08X", hr);
		return hr;
	}

	// New mail notification
	if (HrNewMailNotification(lpStore, lpErrorMsg) != hrSuccess)
		g_lpLogger->Log(EC_LOGLEVEL_WARNING, "Unable to send 'New Mail' notification, error code: 0x%08X", hr);
	return hrSuccess;
}

/**
 * Converts a Contacts folder EntryID to a ZARAFA addressbook EntryID.
 *
 * A contacts folder EntryID contains an offset that is an index in three different possible EntryID named properties.
 *
 * @param[in]	lpUserStore	The store of the user where the contact is stored.
 * @param[in]	cbEntryId	The number of bytes in lpEntryId
 * @param[in]	lpEntryId	The contact EntryID
 * @param[in]	eid_size The number of bytes in eidp
 * @param[in]	eidp  The EntryID where the contact points to
 * @return		HRESULT
 */
static HRESULT ContactToKopano(IMsgStore *lpUserStore,
    ULONG cbEntryId, const ENTRYID *lpEntryId, ULONG *eid_size,
    LPENTRYID *eidp)
{
	HRESULT hr = hrSuccess;
	const CONTAB_ENTRYID *lpContabEntryID = (LPCONTAB_ENTRYID)lpEntryId;
	GUID* guid = (GUID*)&lpContabEntryID->muid;
	ULONG ulObjType;
	object_ptr<IMailUser> lpContact;
	ULONG cValues;
	LPSPropValue lpEntryIds = NULL;
	memory_ptr<SPropTagArray> lpPropTags;
	memory_ptr<MAPINAMEID> lpNames;
	memory_ptr<MAPINAMEID *> lppNames;

	if (sizeof(CONTAB_ENTRYID) > cbEntryId ||
	    *guid != PSETID_CONTACT_FOLDER_RECIPIENT ||
	    lpContabEntryID->email_offset > 2)
		return MAPI_E_NOT_FOUND;

	hr = lpUserStore->OpenEntry(lpContabEntryID->cbeid, reinterpret_cast<ENTRYID *>(const_cast<BYTE *>(lpContabEntryID->abeid)), nullptr, 0, &ulObjType, &~lpContact);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Unable to open contact entryid: %s (%x)",
			GetMAPIErrorMessage(hr), hr);
		return hr;
	}
	hr = MAPIAllocateBuffer(sizeof(MAPINAMEID) * 3, &~lpNames);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "No memory for named ids from contact: %s (%x)",
			GetMAPIErrorMessage(hr), hr);
		return hr;
	}
	hr = MAPIAllocateBuffer(sizeof(LPMAPINAMEID) * 3, &~lppNames);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "No memory for named ids from contact: %s (%x)",
			GetMAPIErrorMessage(hr), hr);
		return hr;
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

	hr = lpContact->GetIDsFromNames(3, lppNames, 0, &~lpPropTags);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Error while retrieving named data from contact: %s (%x)",
			GetMAPIErrorMessage(hr), hr);
		return hr;
	}

	hr = lpContact->GetProps(lpPropTags, 0, &cValues, &lpEntryIds);
	if (FAILED(hr)) {
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Unable to get named properties: %s (%x)",
			GetMAPIErrorMessage(hr), hr);
		return hr;
	}

	if (PROP_TYPE(lpEntryIds[lpContabEntryID->email_offset].ulPropTag) != PT_BINARY) {
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Offset %d not found in contact", lpContabEntryID->email_offset);
		return MAPI_E_NOT_FOUND;
	}

	hr = MAPIAllocateBuffer(lpEntryIds[lpContabEntryID->email_offset].Value.bin.cb, (void**)eidp);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "No memory for contact eid: %s (%x)",
			GetMAPIErrorMessage(hr), hr);
		return hr;
	}

	memcpy(*eidp, lpEntryIds[lpContabEntryID->email_offset].Value.bin.lpb, lpEntryIds[lpContabEntryID->email_offset].Value.bin.cb);
	*eid_size = lpEntryIds[lpContabEntryID->email_offset].Value.bin.cb;
	return hrSuccess;
}

/**
 * Converts an One-off EntryID to a ZARAFA addressbook EntryID.
 *
 * @param[in]	lpAddrBook	The Global Addressbook of the user sending the mail.
 * @param[in]	ulSMTPEID	The number of bytes in lpSMTPEID
 * @param[in]	lpSMTPEID	The One off EntryID.
 * @param[out]	eid_size	The number of bytes in eidp
 * @param[out]	eidp	The ZARAFA entryid of the user defined in the One off.
 * @return		HRESULT
 * @retval		MAPI_E_NOT_FOUND	User not a Kopano user, or lpSMTPEID is not an One-off EntryID
 */
static HRESULT SMTPToZarafa(LPADRBOOK lpAddrBook, ULONG ulSMTPEID,
    const ENTRYID *lpSMTPEID, ULONG *eid_size, LPENTRYID *eidp)
{
	HRESULT hr = hrSuccess;
	wstring wstrName, wstrType, wstrEmailAddress;
	adrlist_ptr lpAList;
	const SPropValue *lpSpoofEID;
	LPENTRYID lpSpoofBin = NULL;

	// representing entryid can also be a one off id, so search the user, and then get the entryid again ..
	// we then always should have yourself as the sender, otherwise: denied
	if (ECParseOneOff(lpSMTPEID, ulSMTPEID, wstrName, wstrType, wstrEmailAddress) != hrSuccess)
		return MAPI_E_NOT_FOUND;
	hr = MAPIAllocateBuffer(CbNewADRLIST(1), &~lpAList);
	if (hr != hrSuccess)
		return hrSuccess;
	lpAList->cEntries = 1;
	lpAList->aEntries[0].cValues = 1;
	if ((hr = MAPIAllocateBuffer(sizeof(SPropValue) * lpAList->aEntries[0].cValues, (void**)&lpAList->aEntries[0].rgPropVals)) != hrSuccess)
		return hrSuccess;
	lpAList->aEntries[0].rgPropVals[0].ulPropTag = PR_DISPLAY_NAME_W;
	lpAList->aEntries[0].rgPropVals[0].Value.lpszW = (WCHAR*)wstrEmailAddress.c_str();
	hr = lpAddrBook->ResolveName(0, EMS_AB_ADDRESS_LOOKUP, NULL, lpAList);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "SMTPToZarafa(): ResolveName failed %x", hr);
		return hrSuccess;
	}
	lpSpoofEID = PCpropFindProp(lpAList->aEntries[0].rgPropVals, lpAList->aEntries[0].cValues, PR_ENTRYID);
	if (!lpSpoofEID) {
		hr = MAPI_E_NOT_FOUND;
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "SMTPToZarafa(): PpropFindProp failed %x", hr);
		return hrSuccess;
	}
	hr = MAPIAllocateBuffer(lpSpoofEID->Value.bin.cb, (void**)&lpSpoofBin);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "SMTPToZarafa(): MAPIAllocateBuffer failed %x", hr);
		return hr;
	}
	memcpy(lpSpoofBin, lpSpoofEID->Value.bin.lpb, lpSpoofEID->Value.bin.cb);
	*eidp = lpSpoofBin;
	*eid_size = lpSpoofEID->Value.bin.cb;
	return hrSuccess;
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
static HRESULT HrFindUserInGroup(LPADRBOOK lpAdrBook, ULONG ulOwnerCB,
    LPENTRYID lpOwnerEID, ULONG ulDistListCB, LPENTRYID lpDistListEID,
    ULONG *lpulCmp, int level = 0)
{
	HRESULT hr = hrSuccess;
	ULONG ulCmp = 0;
	ULONG ulObjType = 0;
	object_ptr<IDistList> lpDistList;
	object_ptr<IMAPITable> lpMembersTable;
	static constexpr const SizedSPropTagArray(2, sptaIDProps) =
		{2, {PR_ENTRYID, PR_OBJECT_TYPE}};

	if (lpulCmp == nullptr)
		return MAPI_E_INVALID_PARAMETER;
	if (level > 10) {
		hr = MAPI_E_TOO_COMPLEX;
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "HrFindUserInGroup(): level too big %d: %s (%x)",
			level, GetMAPIErrorMessage(hr), hr);
		return hr;
	}
	hr = lpAdrBook->OpenEntry(ulDistListCB, lpDistListEID, nullptr, 0, &ulObjType, &~lpDistList);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "HrFindUserInGroup(): OpenEntry failed: %s (%x)",
			GetMAPIErrorMessage(hr), hr);
		return hr;
	}
	hr = lpDistList->GetContentsTable(0, &~lpMembersTable);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "HrFindUserInGroup(): GetContentsTable failed: %s (%x)",
			GetMAPIErrorMessage(hr), hr);
		return hr;
	}
	hr = lpMembersTable->SetColumns(sptaIDProps, 0);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "HrFindUserInGroup(): SetColumns failed: %s (%x)",
			GetMAPIErrorMessage(hr), hr);
		return hr;
	}

	// sort on PR_OBJECT_TYPE (MAILUSER < DISTLIST) ?

	while (TRUE) {
		rowset_ptr lpRowSet;
		hr = lpMembersTable->QueryRows(1, 0, &~lpRowSet);
		if (hr != hrSuccess) {
			g_lpLogger->Log(EC_LOGLEVEL_ERROR, "HrFindUserInGroup(): QueryRows failed: %s (%x)",
				GetMAPIErrorMessage(hr), hr);
			return hr;
		}

		if (lpRowSet->cRows == 0)
			break;

		if (lpRowSet->aRow[0].lpProps[0].ulPropTag != PR_ENTRYID || lpRowSet->aRow[0].lpProps[1].ulPropTag != PR_OBJECT_TYPE)
			continue;

		if (lpRowSet->aRow[0].lpProps[1].Value.ul == MAPI_MAILUSER)
			hr = lpAdrBook->CompareEntryIDs(ulOwnerCB, lpOwnerEID,
			     lpRowSet->aRow[0].lpProps[0].Value.bin.cb, (LPENTRYID)lpRowSet->aRow[0].lpProps[0].Value.bin.lpb,
			     0, &ulCmp);
		else if (lpRowSet->aRow[0].lpProps[1].Value.ul == MAPI_DISTLIST)
			hr = HrFindUserInGroup(lpAdrBook, ulOwnerCB, lpOwnerEID, 
			     lpRowSet->aRow[0].lpProps[0].Value.bin.cb, (LPENTRYID)lpRowSet->aRow[0].lpProps[0].Value.bin.lpb,
			     &ulCmp, level+1);
		if (hr == hrSuccess && ulCmp == TRUE)
			break;
	}
	*lpulCmp = ulCmp;
	return hrSuccess;
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
static HRESULT HrOpenRepresentStore(IAddrBook *lpAddrBook,
    IMsgStore *lpUserStore, IMAPISession *lpAdminSession, ULONG ulRepresentCB,
    LPENTRYID lpRepresentEID, LPMDB *lppRepStore)
{
	HRESULT hr = hrSuccess;
	ULONG ulObjType = 0;
	object_ptr<IMailUser> lpRepresenting;
	memory_ptr<SPropValue> lpRepAccount;
	object_ptr<IExchangeManageStore> lpExchangeManageStore;
	ULONG ulRepStoreCB = 0;
	memory_ptr<ENTRYID> lpRepStoreEID;
	object_ptr<IMsgStore> lpRepStore;

	hr = lpAddrBook->OpenEntry(ulRepresentCB, lpRepresentEID, nullptr, 0, &ulObjType, &~lpRepresenting);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_INFO, "Unable to open representing user in addressbook: %s (%x)",
			GetMAPIErrorMessage(hr), hr);
		return MAPI_E_NOT_FOUND;
	}
	hr = HrGetOneProp(lpRepresenting, PR_ACCOUNT, &~lpRepAccount);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_INFO, "Unable to find account name for representing user: %s (%x)",
			GetMAPIErrorMessage(hr), hr);
		return MAPI_E_NOT_FOUND;
	}

	hr = lpUserStore->QueryInterface(IID_IExchangeManageStore, &~lpExchangeManageStore);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_INFO, "IExchangeManageStore interface not found: %s (%x)",
			GetMAPIErrorMessage(hr), hr);
		return hr;
	}
	hr = lpExchangeManageStore->CreateStoreEntryID(NULL, lpRepAccount->Value.LPSZ, fMapiUnicode, &ulRepStoreCB, &~lpRepStoreEID);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Unable to create store entryid for representing user '" TSTRING_PRINTF "': %s (%x)",
			lpRepAccount->Value.LPSZ, GetMAPIErrorMessage(hr), hr);
		return hr;
	}

	// Use the admin session to open the store, so we have full rights
	hr = lpAdminSession->OpenMsgStore(0, ulRepStoreCB, lpRepStoreEID, nullptr, MAPI_BEST_ACCESS, &~lpRepStore);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Unable to open store of representing user '" TSTRING_PRINTF "': %s (%x)",
			lpRepAccount->Value.LPSZ, GetMAPIErrorMessage(hr), hr);
		return hr;
	}
	return lpRepStore->QueryInterface(IID_IMsgStore,
	       reinterpret_cast<void **>(lppRepStore));
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
static HRESULT HrCheckAllowedEntryIDArray(const char *szFunc,
    const wchar_t *lpszMailer, IAddrBook *lpAddrBook, ULONG ulOwnerCB,
    LPENTRYID lpOwnerEID, ULONG cValues, SBinary *lpEntryIDs,
    ULONG *lpulObjType, bool *lpbAllowed)
{
	HRESULT hr = hrSuccess;
	ULONG ulObjType;
	ULONG ulCmpRes;

	for (ULONG i = 0; i < cValues; ++i) {
		// quick way to see what object the entryid points to .. otherwise we need to call OpenEntry, which is slow
		if (GetNonPortableObjectType(lpEntryIDs[i].cb, (LPENTRYID)lpEntryIDs[i].lpb, &ulObjType))
			continue;

		if (ulObjType == MAPI_DISTLIST) {
			hr = HrFindUserInGroup(lpAddrBook, ulOwnerCB, lpOwnerEID, lpEntryIDs[i].cb, (LPENTRYID)lpEntryIDs[i].lpb, &ulCmpRes);
		} else if (ulObjType == MAPI_MAILUSER) {
			hr = lpAddrBook->CompareEntryIDs(ulOwnerCB, lpOwnerEID, lpEntryIDs[i].cb, (LPENTRYID)lpEntryIDs[i].lpb, 0, &ulCmpRes);
		} else {
			g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Invalid object %d in %s list of user '%ls': %s (%x)",
				ulObjType, szFunc, lpszMailer, GetMAPIErrorMessage(hr), hr);
			continue;
		}

		if (hr == hrSuccess && ulCmpRes == TRUE) {
			*lpulObjType = ulObjType;
			*lpbAllowed = true;
			// always return success, since lpbAllowed is always written
			return hrSuccess;
		}
	}

	*lpbAllowed = false;
	return hrSuccess;
}

/**
 * Checks if the current user is has send-as rights as specified user. Needs
 * admin rights to open the delegate store.
 *
 * @param[in]	lpAddrBook	The Global Addressbook of the user trying to send an email.
 * @param[in]	lpUserStore	The store of the user trying to send an email.
 * @param[in]	lpAdminSession MAPI session of the Kopano SYSTEM user.
 * @param[in]	lpMailer	ECSender object (inetmapi), used to set an error for an error mail if not allowed.
 * @param[in]	ulOwnerCB	Number of bytes in lpOwnerEID
 * @param[in]	lpOwnerEID	EntryID of the user sending the mail.
 * @param[in]	ulRepresentCB Number of bytes in lpRepresentEID.
 * @param[in]	lpRepresentEID EntryID of the user set in the From address. Can be a One-off entryid.
 * @param[out]	lpbAllowed	Set to true if the lpOwnerEID is a delegate of lpRepresentEID
 * @param[out]	lppRepStore	The store of the delegate when allowed.
 * @return		HRESULT
 */
static HRESULT CheckSendAs(IAddrBook *lpAddrBook, IMsgStore *lpUserStore,
    IMAPISession *lpAdminSession, ECSender *lpMailer, ULONG ulOwnerCB,
    LPENTRYID lpOwnerEID, ULONG ulRepresentCB, LPENTRYID lpRepresentEID,
    bool *lpbAllowed, LPMDB *lppRepStore)
{
	HRESULT hr = hrSuccess;
	bool bAllowed = false;
	bool bHasStore = false;
	ULONG ulObjType;
	object_ptr<IMailUser> lpMailboxOwner, lpRepresenting;
	memory_ptr<SPropValue> lpOwnerProps, lpRepresentProps;
	SPropValue sSpoofEID = {0};
	ULONG ulCmpRes = 0;
	static constexpr const SizedSPropTagArray(3, sptaIDProps) =
		{3, {PR_DISPLAY_NAME_W, PR_EC_SENDAS_USER_ENTRYIDS,
		PR_DISPLAY_TYPE}};
	ULONG cValues = 0;

	hr = SMTPToZarafa(lpAddrBook, ulRepresentCB, lpRepresentEID, &sSpoofEID.Value.bin.cb, (LPENTRYID*)&sSpoofEID.Value.bin.lpb);
	if (hr != hrSuccess)
		hr = ContactToKopano(lpUserStore, ulRepresentCB, lpRepresentEID, &sSpoofEID.Value.bin.cb, (LPENTRYID*)&sSpoofEID.Value.bin.lpb);
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

	// representing entryid is now always a Kopano Entry ID. Open the user so we can log the display name
	hr = lpAddrBook->OpenEntry(ulRepresentCB, lpRepresentEID, nullptr, 0, &ulObjType, &~lpRepresenting);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "CheckSendAs(): OpenEntry failed(1) %x", hr);
		goto exit;
	}
	hr = lpRepresenting->GetProps(sptaIDProps, 0, &cValues, &~lpRepresentProps);
	if (FAILED(hr)) {
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "CheckSendAs(): GetProps failed(1) %x", hr);
		goto exit;
	}

	hr = hrSuccess;

	// Open the owner to get the displayname for logging
	if (lpAddrBook->OpenEntry(ulOwnerCB, lpOwnerEID, nullptr, 0, &ulObjType, &~lpMailboxOwner) != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "CheckSendAs(): OpenEntry failed(2) %x", hr);
		goto exit;
	}
	hr = lpMailboxOwner->GetProps(sptaIDProps, 0, &cValues, &~lpOwnerProps);
	if (FAILED(hr)) {
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "CheckSendAs(): GetProps failed(2) %x", hr);
		goto exit;
	}

	hr = hrSuccess;

	if (lpRepresentProps[2].ulPropTag != PR_DISPLAY_TYPE) {	// Required property for a mailuser object
		hr = MAPI_E_NOT_FOUND;
		g_lpLogger->Log(EC_LOGLEVEL_NOTICE, "CheckSendAs(): PR_DISPLAY_TYPE missing %x", hr);
		goto exit;
	}

	bHasStore = (lpRepresentProps[2].Value.l == DT_MAILUSER);
	if (lpRepresentProps[1].ulPropTag != PR_EC_SENDAS_USER_ENTRYIDS)
		// No sendas, therefore no sendas permissions, but we don't fail
		goto exit;

	hr = HrCheckAllowedEntryIDArray("sendas",
	     lpRepresentProps[0].ulPropTag == PR_DISPLAY_NAME_W ? lpRepresentProps[0].Value.lpszW : L"<no name>",
	     lpAddrBook, ulOwnerCB, lpOwnerEID,
	     lpRepresentProps[1].Value.MVbin.cValues, lpRepresentProps[1].Value.MVbin.lpbin, &ulObjType, &bAllowed);
	if (bAllowed)
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Mail for user '%ls' is sent as %s '%ls'",
			lpOwnerProps[0].ulPropTag == PR_DISPLAY_NAME_W ? lpOwnerProps[0].Value.lpszW : L"<no name>",
			(ulObjType != MAPI_DISTLIST)?"user":"group",
			lpRepresentProps[0].ulPropTag == PR_DISPLAY_NAME_W ? lpRepresentProps[0].Value.lpszW : L"<no name>");
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

	if (bAllowed && bHasStore)
		hr = HrOpenRepresentStore(lpAddrBook, lpUserStore, lpAdminSession, ulRepresentCB, lpRepresentEID, lppRepStore);
	else
		*lppRepStore = NULL;

	*lpbAllowed = bAllowed;
	MAPIFreeBuffer(sSpoofEID.Value.bin.lpb);
	return hr;
}

/**
 * Checks if the current user is a delegate of a specified user. Needs
 * admin rights to open the delegate store.
 *
 * @param[in]	lpAddrBook	The Global Addressbook of the user trying to send an email.
 * @param[in]	lpUserStore	The store of the user trying to send an email.
 * @param[in]	lpAdminSession MAPI session of the Kopano SYSTEM user.
 * @param[in]	ulOwnerCB	Number of bytes in lpOwnerEID
 * @param[in]	lpOwnerEID	EntryID of the user sending the mail.
 * @param[in]	ulRepresentCB Number of bytes in lpRepresentEID.
 * @param[in]	lpRepresentEID EntryID of the user set in the From address. Can be a One-off entryid.
 * @param[out]	lpbAllowed	Set to true if the lpOwnerEID is a delegate of lpRepresentEID
 * @param[out]	lppRepStore	The store of the delegate when allowed.
 * @return		HRESULT
 * @retval		hrSuccess, always returned, actual return value in lpbAllowed.
 */
static HRESULT CheckDelegate(IAddrBook *lpAddrBook, IMsgStore *lpUserStore,
    IMAPISession *lpAdminSession, ULONG ulOwnerCB, LPENTRYID lpOwnerEID,
    ULONG ulRepresentCB, LPENTRYID lpRepresentEID, bool *lpbAllowed,
    LPMDB *lppRepStore)
{
	HRESULT hr = hrSuccess;
	bool bAllowed = false;
	ULONG ulObjType;
	object_ptr<IMsgStore> lpRepStore;
	memory_ptr<SPropValue> lpUserOwnerName, lpRepOwnerName;
	object_ptr<IMAPIFolder> lpRepSubtree;
	memory_ptr<SPropValue> lpRepFBProp, lpDelegates;
	object_ptr<IMessage> lpRepFBMessage;
	SPropValue sSpoofEID = {0};

	hr = SMTPToZarafa(lpAddrBook, ulRepresentCB, lpRepresentEID, &sSpoofEID.Value.bin.cb, (LPENTRYID*)&sSpoofEID.Value.bin.lpb);
	if (hr != hrSuccess)
		hr = ContactToKopano(lpUserStore, ulRepresentCB, lpRepresentEID, &sSpoofEID.Value.bin.cb, (LPENTRYID*)&sSpoofEID.Value.bin.lpb);
	if (hr == hrSuccess) {
		ulRepresentCB = sSpoofEID.Value.bin.cb;
		lpRepresentEID = (LPENTRYID)sSpoofEID.Value.bin.lpb;
	}
	hr = HrOpenRepresentStore(lpAddrBook, lpUserStore, lpAdminSession, ulRepresentCB, lpRepresentEID, &~lpRepStore);
	if (hr == MAPI_E_NOT_FOUND) {
		hr = hrSuccess;	// No store: no delegate allowed!
		goto exit;
	}
	else if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "CheckDelegate() HrOpenRepresentStore failed: %x", hr);
		goto exit;
	}
	hr = HrGetOneProp(lpUserStore, PR_MAILBOX_OWNER_NAME, &~lpUserOwnerName);
	if (hr != hrSuccess)
		g_lpLogger->Log(EC_LOGLEVEL_NOTICE, "CheckDelegate() PR_MAILBOX_OWNER_NAME(user) fetch failed %x", hr);

	hr = HrGetOneProp(lpRepStore, PR_MAILBOX_OWNER_NAME, &~lpRepOwnerName);
	if (hr != hrSuccess)
		g_lpLogger->Log(EC_LOGLEVEL_NOTICE, "CheckDelegate() PR_MAILBOX_OWNER_NAME(rep) fetch failed %x", hr);
	// ignore error, just a name for logging

	// open root container
	hr = lpRepStore->OpenEntry(0, nullptr, nullptr, 0, &ulObjType, &~lpRepSubtree);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_NOTICE, "CheckDelegate() OpenENtry(rep) failed %x", hr);
		goto exit;
	}
	hr = HrGetOneProp(lpRepSubtree, PR_FREEBUSY_ENTRYIDS, &~lpRepFBProp);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_NOTICE, "CheckDelegate() HrGetOneProp(rep) failed %x", hr);
		goto exit;
	}

	if (lpRepFBProp->Value.MVbin.cValues < 2) {
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}
	hr = lpRepSubtree->OpenEntry(lpRepFBProp->Value.MVbin.lpbin[1].cb, reinterpret_cast<ENTRYID *>(lpRepFBProp->Value.MVbin.lpbin[1].lpb), nullptr, 0, &ulObjType, &~lpRepFBMessage);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_NOTICE, "CheckDelegate() OpenEntry(rep) failed %x", hr);
		goto exit;
	}
	hr = HrGetOneProp(lpRepFBMessage, PR_SCHDINFO_DELEGATE_ENTRYIDS, &~lpDelegates);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_NOTICE, "CheckDelegate() HrGetOneProp failed %x", hr);
		goto exit;
	}

	hr = HrCheckAllowedEntryIDArray("delegate", lpRepOwnerName ? lpRepOwnerName->Value.lpszW : L"<no name>", lpAddrBook, ulOwnerCB, lpOwnerEID, lpDelegates->Value.MVbin.cValues, lpDelegates->Value.MVbin.lpbin, &ulObjType, &bAllowed);
	if (hr != hrSuccess) {
		ec_log_err("CheckDelegate() HrCheckAllowedEntryIDArray failed %x %s", hr, GetMAPIErrorMessage(hr));
		goto exit;
	}
	if (bAllowed)
		g_lpLogger->Log(EC_LOGLEVEL_INFO, "Mail for user '%ls' is allowed on behalf of user '%ls'%s",
						lpUserOwnerName ? lpUserOwnerName->Value.lpszW : L"<no name>",
						lpRepOwnerName ? lpRepOwnerName->Value.lpszW : L"<no name>",
						(ulObjType != MAPI_DISTLIST)?"":" because of group");
exit:
	*lpbAllowed = bAllowed;
	// when any step failed, delegate is not setup correctly, so bAllowed == false
	hr = hrSuccess;

	if (bAllowed)
		*lppRepStore = lpRepStore.release();
	MAPIFreeBuffer(sSpoofEID.Value.bin.lpb);
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
static HRESULT CopyDelegateMessageToSentItems(LPMESSAGE lpMessage,
    LPMDB lpRepStore, LPMESSAGE *lppRepMessage)
{
	HRESULT hr = hrSuccess;
	memory_ptr<SPropValue> lpSentItemsEntryID;
	object_ptr<IMAPIFolder> lpSentItems;
	ULONG ulObjType;
	object_ptr<IMessage> lpDestMsg;
	SPropValue sProp[1];

	hr = HrGetOneProp(lpRepStore, PR_IPM_SENTMAIL_ENTRYID, &~lpSentItemsEntryID);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_WARNING, "Unable to find representee's sent items folder: error 0x%08X", hr);
		return hr;
	}

	hr = lpRepStore->OpenEntry(lpSentItemsEntryID->Value.bin.cb, reinterpret_cast<ENTRYID *>(lpSentItemsEntryID->Value.bin.lpb),
	     &IID_IMAPIFolder, MAPI_BEST_ACCESS, &ulObjType, &~lpSentItems);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_WARNING, "Unable to open representee's sent items folder: error 0x%08X", hr);
		return hr;
	}
	hr = lpSentItems->CreateMessage(nullptr, 0, &~lpDestMsg);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_WARNING, "Unable to create representee's message: error 0x%08X", hr);
		return hr;
	}

	hr = lpMessage->CopyTo(0, NULL, NULL, 0, NULL, &IID_IMessage, (LPVOID)lpDestMsg, 0, NULL);
	if (FAILED(hr)) {
		g_lpLogger->Log(EC_LOGLEVEL_WARNING, "Unable to copy representee's message: error 0x%08X", hr);
		return hr;
	}

	sProp[0].ulPropTag = PR_MESSAGE_FLAGS;
	sProp[0].Value.ul = MSGFLAG_READ;

	hr = lpDestMsg->SetProps(1, sProp, NULL);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_WARNING, "Unable to edit representee's message: error 0x%08X", hr);
		return hr;
	}
	*lppRepMessage = lpDestMsg.release();
	g_lpLogger->Log(EC_LOGLEVEL_INFO, "Copy placed in representee's sent items folder");
	return hrSuccess;
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
static HRESULT PostSendProcessing(ULONG cbEntryId, const ENTRYID *lpEntryId,
    IMsgStore *lpMsgStore)
{
	HRESULT hr = hrSuccess;
	memory_ptr<SPropValue> lpObject;
	object_ptr<IECSpooler> lpSpooler;
	
	hr = HrGetOneProp(lpMsgStore, PR_EC_OBJECT, &~lpObject);
	if(hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Unable to get PR_EC_OBJECT in post-send processing: 0x%08X", hr);
		return hr;
	}
	hr = ((IECUnknown *)lpObject->Value.lpszA)->QueryInterface(IID_IECSpooler, &~lpSpooler);
	if(hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Unable to get spooler interface for message: 0x%08X", hr);
		return hr;
	}
	
	hr = lpSpooler->DeleteFromMasterOutgoingTable(cbEntryId, lpEntryId, EC_SUBMIT_MASTER);
	if (hr != hrSuccess)
		g_lpLogger->Log(EC_LOGLEVEL_WARNING, "Could not remove invalid message from queue, error code: 0x%08X", hr);
	return hr;
}

/**
 * Using the given resources, sends the mail to the SMTP server.
 *
 * @param[in]	lpAdminSession	Kopano SYSTEM user MAPI session.
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
static HRESULT ProcessMessage(IMAPISession *lpAdminSession,
    IMAPISession *lpUserSession, IECServiceAdmin *lpServiceAdmin,
    IECSecurity *lpSecurity, IMsgStore *lpUserStore, IAddrBook *lpAddrBook,
    ECSender *lpMailer, ULONG cbMsgEntryId, LPENTRYID lpMsgEntryId,
    IMessage **lppMessage)
{
	HRESULT 		hr 				= hrSuccess;
	object_ptr<IMessage> lpMessage;
	ULONG			ulObjType		= 0;

	ULONG			cbOwner			= 0;
	memory_ptr<ENTRYID> lpOwner;
	memory_ptr<ECUSER> lpUser;
	SPropValue		sPropSender[4];
	static constexpr const SizedSPropTagArray(5, sptaMoveReprProps) =
		{5, {PR_SENT_REPRESENTING_NAME_W,
		PR_SENT_REPRESENTING_ADDRTYPE_W,
		PR_SENT_REPRESENTING_EMAIL_ADDRESS_W,
		PR_SENT_REPRESENTING_ENTRYID, PR_SENT_REPRESENTING_SEARCH_KEY}};
	memory_ptr<SPropValue> lpMoveReprProps, lpPropOwner;
	ULONG			cValuesMoveProps = 0;
	bool			bAllowSendAs = false;
	bool			bAllowDelegate = false;
	ULONG			ulCmpRes = 0;
	
	object_ptr<IMsgStore> lpRepStore;
	object_ptr<IMessage> lpRepMessage;
	memory_ptr<SPropValue> lpRepEntryID, lpSubject, lpMsgSize;
	memory_ptr<SPropValue> lpAutoForward, lpMsgClass, lpDeferSendTime;

	PyMapiPluginFactory pyMapiPluginFactory;
	PyMapiPluginAPtr ptrPyMapiPlugin;
	ULONG ulResult = 0;

	ArchiveResult	archiveResult;

	sending_options sopt;

	imopt_default_sending_options(&sopt);

	// When sending messages, we want to minimize the use of tnef.
	// In case always_send_tnef is set to yes, we force tnef, otherwise we
	// minimize (set to no or minimal).
	if (!strcmp(g_lpConfig->GetSetting("always_send_tnef"), "minimal") ||
	    !parseBool(g_lpConfig->GetSetting("always_send_tnef")))
		sopt.use_tnef = -1;
	else
		sopt.use_tnef = 1;

	sopt.force_utf8 = parseBool(g_lpConfig->GetSetting("always_send_utf8"));
	sopt.allow_send_to_everyone = parseBool(g_lpConfig->GetSetting("allow_send_to_everyone"));

	// Enable SMTP Delivery Status Notifications
	sopt.enable_dsn = parseBool(g_lpConfig->GetSetting("enable_dsn"));

	sopt.always_expand_distr_list = parseBool(g_lpConfig->GetSetting("expand_groups"));

	// Init plugin system
	hr = pyMapiPluginFactory.Init(g_lpConfig, g_lpLogger);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to instantiate plugin factory, hr=0x%08x", hr);
		goto exit;
	}
	hr = GetPluginObject(&pyMapiPluginFactory, &~ptrPyMapiPlugin);
	if (hr != hrSuccess)
		goto exit; // Error logged in GetPluginObject

	// Get the owner of the store
	hr = lpSecurity->GetOwner(&cbOwner, &~lpOwner);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Unable to get owner information, error code: 0x%08X", hr);
		goto exit;
	}

	// We now have the owner ID, get the owner information through the ServiceAdmin
	hr = lpServiceAdmin->GetUser(cbOwner, lpOwner, MAPI_UNICODE, &~lpUser);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Unable to get user information from store, error code: 0x%08X", hr);
		goto exit;
	}

	// open the message we need to send
	hr = lpUserStore->OpenEntry(cbMsgEntryId, reinterpret_cast<ENTRYID *>(lpMsgEntryId), &IID_IMessage, MAPI_BEST_ACCESS, &ulObjType, &~lpMessage);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Could not open message in store from user %ls: %s (%x)",
			lpUser->lpszUsername, GetMAPIErrorMessage(hr), hr);
		goto exit;
	}

	// get subject for logging
	HrGetOneProp(lpMessage, PR_SUBJECT_W, &~lpSubject);
	HrGetOneProp(lpMessage, PR_MESSAGE_SIZE, &~lpMsgSize);
	HrGetOneProp(lpMessage, PR_DEFERRED_SEND_TIME, &~lpDeferSendTime);

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
	if (g_lpLogger->Log(EC_LOGLEVEL_DEBUG))
		g_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Sending e-mail for user %ls, subject: '%ls', size: %d",
			lpUser->lpszUsername, lpSubject ? lpSubject->Value.lpszW : L"<none>",
			lpMsgSize ? lpMsgSize->Value.ul : 0);
	else
		g_lpLogger->Log(EC_LOGLEVEL_INFO, "Sending e-mail for user %ls, size: %d",
			lpUser->lpszUsername, lpMsgSize ? lpMsgSize->Value.ul : 0);

	/* 

	   PR_SENDER_* maps to Sender:
	   PR_SENT_REPRESENTING_* maps to From:

	   Sender: field is optional, From: is mandatory
	   PR_SENDER_* is mandatory, and always set by us (will be overwritten if was set)
	   PR_SENT_REPRESENTING_* is optional, and set by outlook when the user modifies the From in outlook.

	*/

	// Set PR_SENT_REPRESENTING, as this is set on all 'sent' items and is the column
	// that is shown by default in Outlook's 'sent items' folder
	if (HrGetOneProp(lpMessage, PR_SENT_REPRESENTING_ENTRYID, &~lpRepEntryID) != hrSuccess) {
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

		HRESULT hr2 = lpMessage->SetProps(4, sPropSender, NULL);
		if (hr2 != hrSuccess) {
			g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Unable to set sender id for message: %s (%x)",
				GetMAPIErrorMessage(hr2), hr2);
			goto exit;
		}
	}
		// requested that mail is sent as somebody else
		// since we can have SMTP and ZARAFA entry IDs, we will open it, and get the

		// If this is a forwarded e-mail, then allow sending as the original sending e-mail address. Note that
		// this can be misused by MAPI client that just set PR_AUTO_FORWARDED. Since it would have been just as
		// easy for the client just to spoof their 'from' address via SMTP, we're allowing this for now. You can
		// completely turn it off via the 'allow_redirect_spoofing' setting.
	else if (strcmp(g_lpConfig->GetSetting("allow_redirect_spoofing"), "yes") == 0 &&
	    HrGetOneProp(lpMessage, PR_AUTO_FORWARDED, &~lpAutoForward) == hrSuccess &&
	    lpAutoForward->Value.b) {
			bAllowSendAs = true;
	} else {
		hr = HrGetOneProp(lpUserStore, PR_MAILBOX_OWNER_ENTRYID, &~lpPropOwner);
		if (hr != hrSuccess) {
			g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Unable to get Kopano mailbox owner id, error code: 0x%08X", hr);
			goto exit;
		}
		hr = lpAddrBook->CompareEntryIDs(lpPropOwner->Value.bin.cb, (LPENTRYID)lpPropOwner->Value.bin.lpb,
		     lpRepEntryID->Value.bin.cb, (LPENTRYID)lpRepEntryID->Value.bin.lpb, 0, &ulCmpRes);
		if (hr == hrSuccess && ulCmpRes == FALSE) {
			if (strcmp(g_lpConfig->GetSetting("always_send_delegates"), "yes") == 0) {
				// pre 6.20 behaviour
				bAllowDelegate = true;
				HrOpenRepresentStore(lpAddrBook, lpUserStore, lpAdminSession, lpRepEntryID->Value.bin.cb, reinterpret_cast<ENTRYID *>(lpRepEntryID->Value.bin.lpb), &~lpRepStore);
				// ignore error if unable to open, just the copy of the mail might possibily not be done.
			} else if(strcmp(g_lpConfig->GetSetting("allow_delegate_meeting_request"), "yes") == 0 &&
			    HrGetOneProp(lpMessage, PR_MESSAGE_CLASS_A, &~lpMsgClass) == hrSuccess &&
			    ((strcasecmp(lpMsgClass->Value.lpszA, "IPM.Schedule.Meeting.Request" ) == 0) ||
			    (strcasecmp(lpMsgClass->Value.lpszA, "IPM.Schedule.Meeting.Canceled" ) == 0))) {
				// Meeting request can always sent as 'on behalf of' (Zarafa and SMTP user).
				// This is needed if a user forward a meeting request. If you have permissions on a calendar,
				// you can always sent with 'on behalve of'. This behavior is like exchange.
				bAllowDelegate = true;
			} else {
				hr = CheckDelegate(lpAddrBook, lpUserStore, lpAdminSession, lpPropOwner->Value.bin.cb, reinterpret_cast<ENTRYID *>(lpPropOwner->Value.bin.lpb),
				     lpRepEntryID->Value.bin.cb, reinterpret_cast<ENTRYID *>(lpRepEntryID->Value.bin.lpb), &bAllowDelegate, &~lpRepStore);
				if (hr != hrSuccess)
					goto exit;
			}
			if (!bAllowDelegate) {
				hr = CheckSendAs(lpAddrBook, lpUserStore, lpAdminSession, lpMailer, lpPropOwner->Value.bin.cb, reinterpret_cast<ENTRYID *>(lpPropOwner->Value.bin.lpb),
				     lpRepEntryID->Value.bin.cb, reinterpret_cast<ENTRYID *>(lpRepEntryID->Value.bin.lpb), &bAllowSendAs, &~lpRepStore);
				if (hr != hrSuccess)
					goto exit;
				if (!bAllowSendAs) {
					g_lpLogger->Log(EC_LOGLEVEL_WARNING, "E-mail for user %ls may not be sent, notifying user", lpUser->lpszUsername);
					HRESULT hr2 = SendUndeliverable(lpMailer, lpUserStore, lpMessage);
					if (hr2 != hrSuccess)
						g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Unable to create undeliverable message for user %ls: %s (%x)",
							lpUser->lpszUsername, GetMAPIErrorMessage(hr2), hr2);
					// note: hr == hrSuccess, parent process will not send the undeliverable too
					goto exit;
				}
				// else {}: we are allowed to directly send
			}
			// else {}: allowed with 'on behalf of'
		}
		// else {}: owner and representing are the same, send as normal mail
	}

	// put storeowner info in PR_SENDER_ props, forces correct From data
	sPropSender[0].ulPropTag = PR_SENDER_NAME_W;
	sPropSender[0].Value.LPSZ = lpUser->lpszFullName;
	sPropSender[1].ulPropTag = PR_SENDER_ADDRTYPE_W;
	sPropSender[1].Value.LPSZ = const_cast<TCHAR *>(_T("ZARAFA"));
	sPropSender[2].ulPropTag = PR_SENDER_EMAIL_ADDRESS_W;
	sPropSender[2].Value.LPSZ = lpUser->lpszMailAddress;

	sPropSender[3].ulPropTag = PR_SENDER_ENTRYID;
	sPropSender[3].Value.bin.cb = lpUser->sUserId.cb;
	sPropSender[3].Value.bin.lpb = lpUser->sUserId.lpb;
	// @todo PR_SENDER_SEARCH_KEY

	hr = lpMessage->SetProps(4, sPropSender, NULL);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Unable to update message with sender: %s (%x)",
			GetMAPIErrorMessage(hr), hr);
		goto exit;
	}

	hr = lpMessage->SaveChanges(KEEP_OPEN_READWRITE);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Unable to save message before sending: %s (%x)",
			GetMAPIErrorMessage(hr), hr);
		goto exit;
	}

	if (lpRepStore != nullptr &&
	    parseBool(g_lpConfig->GetSetting("copy_delegate_mails", NULL, "yes")))
		// copy the original message with the actual sender data
		// so you see the "on behalf of" in the sent-items version, even when send-as is used (see below)
		CopyDelegateMessageToSentItems(lpMessage, lpRepStore, &~lpRepMessage);
		// possible error is logged in function.

	if (bAllowSendAs) {
		// move PR_REPRESENTING to PR_SENDER_NAME
		hr = lpMessage->GetProps(sptaMoveReprProps, 0, &cValuesMoveProps, &~lpMoveReprProps);
		if (FAILED(hr)) {
			g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Unable to find sender information: %s (%x)",
				GetMAPIErrorMessage(hr), hr);
			goto exit;
		}

		hr = lpMessage->DeleteProps(sptaMoveReprProps, NULL);
		if (FAILED(hr)) {
			g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Unable to remove sender information: %s (%x)",
				GetMAPIErrorMessage(hr), hr);
			goto exit;
		}

		lpMoveReprProps[0].ulPropTag = PROP_TAG(PROP_TYPE(lpMoveReprProps[0].ulPropTag), PROP_ID(PR_SENDER_NAME_W));
		lpMoveReprProps[1].ulPropTag = PROP_TAG(PROP_TYPE(lpMoveReprProps[1].ulPropTag), PROP_ID(PR_SENDER_ADDRTYPE_W));
		lpMoveReprProps[2].ulPropTag = PROP_TAG(PROP_TYPE(lpMoveReprProps[2].ulPropTag), PROP_ID(PR_SENDER_EMAIL_ADDRESS_W));
		lpMoveReprProps[3].ulPropTag = PROP_TAG(PROP_TYPE(lpMoveReprProps[3].ulPropTag), PROP_ID(PR_SENDER_ENTRYID));
		lpMoveReprProps[4].ulPropTag = PROP_TAG(PROP_TYPE(lpMoveReprProps[4].ulPropTag), PROP_ID(PR_SENDER_SEARCH_KEY));

		hr = lpMessage->SetProps(5, lpMoveReprProps, NULL);
		if (FAILED(hr)) {
			g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Unable to update sender information: %s (%x)",
				GetMAPIErrorMessage(hr), hr);
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
		 * from the message) and move it using its entryid. Since we
		 * didn't save these changes, the original unmodified version
		 * will be moved to the sent-items folder, and that will show
		 * the correct From/Sender data.
		 */
	}

	if (sopt.always_expand_distr_list) {
		// Expand recipients with ADDRTYPE=ZARAFA to multiple ADDRTYPE=SMTP recipients
		hr = ExpandRecipients(lpAddrBook, lpMessage);
		if(hr != hrSuccess)
			g_lpLogger->Log(EC_LOGLEVEL_WARNING, "Unable to expand message recipient groups: %s (%x)",
				GetMAPIErrorMessage(hr), hr);
	}

	hr = RewriteRecipients(lpUserSession, lpMessage);
	if (hr != hrSuccess)
		g_lpLogger->Log(EC_LOGLEVEL_WARNING, "Unable to rewrite recipients: %s (%x)",
			GetMAPIErrorMessage(hr), hr);

	if (sopt.always_expand_distr_list) {
		// Only touch recips if we're expanding groups; the rationale is here that the user
		// has typed a recipient twice if we have duplicates and expand_groups = no, so that's
		// what the user wanted apparently. What's more, duplicate recips are filtered for RCPT TO
		// later.
		hr = UniqueRecipients(lpMessage);
		if (hr != hrSuccess)
			g_lpLogger->Log(EC_LOGLEVEL_WARNING, "Unable to remove duplicate recipients: %s (%x)",
				GetMAPIErrorMessage(hr), hr);
	}

	RewriteQuotedRecipients(lpMessage);

	hr = ptrPyMapiPlugin->MessageProcessing("PreSending", lpUserSession, lpAddrBook, lpUserStore, NULL, lpMessage, &ulResult);
	if (hr != hrSuccess)
		goto exit;

	if (ulResult == MP_RETRY_LATER) {
		hr = MAPI_E_WAIT;
		goto exit;
	} else if (ulResult == MP_FAILED) {
		g_lpLogger->Log(EC_LOGLEVEL_CRIT, "Plugin error, hook gives a failed error: %s (%x).",
			GetMAPIErrorMessage(ulResult), ulResult);
		hr = MAPI_E_CANCEL;
		goto exit;
	}

	// Archive the message
	if (parseBool(g_lpConfig->GetSetting("archive_on_send"))) {
		ArchivePtr ptrArchive;
		
		hr = Archive::Create(lpAdminSession, &ptrArchive);
		if (hr != hrSuccess) {
			g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Unable to instantiate archive object: 0x%08X", hr);
			goto exit;
		}
		
		hr = ptrArchive->HrArchiveMessageForSending(lpMessage, &archiveResult);
		if (hr != hrSuccess) {
			if (ptrArchive->HaveErrorMessage())
				lpMailer->setError(ptrArchive->GetErrorMessage());
			goto exit;
		}
	}

	// Now hand message to library which will send it, inetmapi will handle addressbook
	hr = IMToINet(lpUserSession, lpAddrBook, lpMessage, lpMailer, sopt);

	// log using fatal, all other log messages are otherwise somewhat meaningless
	if (hr == MAPI_W_NO_SERVICE) {
		g_lpLogger->Log(EC_LOGLEVEL_WARNING, "Unable to connect to SMTP server, retrying mail for user %ls later", lpUser->lpszUsername);
		goto exit;
	} else if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_WARNING, "E-mail for user %ls could not be sent, notifying user: %s (%x)",
			lpUser->lpszUsername, GetMAPIErrorMessage(hr), hr);

		hr = SendUndeliverable(lpMailer, lpUserStore, lpMessage);
		if (hr != hrSuccess)
			g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Unable to create undeliverable message for user %ls: %s (%x)",
				lpUser->lpszUsername, GetMAPIErrorMessage(hr), hr);

		// we set hr to success, so the parent process does not create the undeliverable thing again
		hr = hrSuccess;
		goto exit;
	} else {
		g_lpLogger->Log(EC_LOGLEVEL_DEBUG, "E-mail for user %ls was accepted by SMTP server", lpUser->lpszUsername);
	}

	// If we have a repsenting message, save that now in the sent-items of that user
	if (lpRepMessage) {
		HRESULT hr2 = lpRepMessage->SaveChanges(0);

		if (hr2 != hrSuccess)
			g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Representee's mail copy could not be saved: %s (%x)",
				GetMAPIErrorMessage(hr2), hr2);
	}

exit:
	if (FAILED(hr))
		archiveResult.Undo(lpAdminSession);

	// We always return the processes message to the caller, whether it failed or not
	if (lpMessage)
		lpMessage->QueryInterface(IID_IMessage, (void**)lppMessage);
	return hr;
}

/**
 * Entry point, sends the mail for a user. Most of the time, it will
 * also move the sent mail to the "Sent Items" folder of the user.
 *
 * @param[in]	szUsername	The username to login as. This name is in unicode.
 * @param[in]	szSMTP		The SMTP server name or IP address to use.
 * @param[in]	szPath		The URI to the Kopano server.
 * @param[in]	cbMsgEntryId The number of bytes in lpMsgEntryId
 * @param[in]	lpMsgEntryId The EntryID of the message to send
 * @param[in]	bDoSentMail	true if the mail should be moved to the "Sent Items" folder of the user.
 * @return		HRESULT
 */
HRESULT ProcessMessageForked(const wchar_t *szUsername, const char *szSMTP,
    int ulPort, const char *szPath, ULONG cbMsgEntryId, LPENTRYID lpMsgEntryId,
    bool bDoSentMail)
{
	HRESULT			hr = hrSuccess;
	object_ptr<IMAPISession> lpAdminSession, lpUserSession;
	object_ptr<IAddrBook> lpAddrBook;
	std::unique_ptr<ECSender> lpMailer;
	object_ptr<IMsgStore> lpUserStore;
	object_ptr<IECServiceAdmin> lpServiceAdmin;
	object_ptr<IECSecurity> lpSecurity;
	memory_ptr<SPropValue> lpsProp;
	object_ptr<IMessage> lpMessage;
	
	lpMailer.reset(CreateSender(szSMTP, ulPort));
	if (!lpMailer) {
		hr = MAPI_E_NOT_ENOUGH_MEMORY;
		g_lpLogger->Log(EC_LOGLEVEL_NOTICE, "ProcessMessageForked(): CreateSender failed: %s (%x)",
			GetMAPIErrorMessage(hr), hr);
		goto exit;
	}

	// The Admin session is used for checking delegates and archiving
	hr = HrOpenECAdminSession(&~lpAdminSession, "spooler/mailer:admin",
	     PROJECT_SVN_REV_STR, szPath, EC_PROFILE_FLAGS_NO_PUBLIC_STORE,
	     g_lpConfig->GetSetting("sslkey_file", "", NULL),
	     g_lpConfig->GetSetting("sslkey_pass", "", NULL));
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Unable to open admin session: %s (%x)",
			GetMAPIErrorMessage(hr), hr);
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
	hr = HrOpenECSession(&~lpUserSession, "spooler/mailer",
	     PROJECT_SVN_REV_STR, szUsername, L"", szPath,
	     EC_PROFILE_FLAGS_NO_PUBLIC_STORE,
	     g_lpConfig->GetSetting("sslkey_file", "", NULL),
	     g_lpConfig->GetSetting("sslkey_pass", "", NULL));
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Unable to open user session: %s (%x)",
			GetMAPIErrorMessage(hr), hr);
		goto exit;
	}
	hr = lpUserSession->OpenAddressBook(0, nullptr, AB_NO_DIALOG, &~lpAddrBook);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Unable to open addressbook. %s (%x)",
			GetMAPIErrorMessage(hr), hr);
		goto exit;
	}
	hr = HrOpenDefaultStore(lpUserSession, &~lpUserStore);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Unable to open default store of user: %s (%x)",
			GetMAPIErrorMessage(hr), hr);
		goto exit;
	}
	hr = HrGetOneProp(lpUserStore, PR_EC_OBJECT, &~lpsProp);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Unable to get Kopano internal object: %s (%x)",
			GetMAPIErrorMessage(hr), hr);
		goto exit;
	}

	// NOTE: object is placed in Value.lpszA, not Value.x
	hr = ((IECUnknown *)lpsProp->Value.lpszA)->QueryInterface(IID_IECServiceAdmin, &~lpServiceAdmin);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "ServiceAdmin interface not supported: %s (%x)",
			GetMAPIErrorMessage(hr), hr);
		goto exit;
	}
	hr = ((IECUnknown *)lpsProp->Value.lpszA)->QueryInterface(IID_IECSecurity, &~lpSecurity);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "IID_IECSecurity not supported by store: %s (%x)",
			GetMAPIErrorMessage(hr), hr);
		goto exit;
	}

	hr = ProcessMessage(lpAdminSession, lpUserSession, lpServiceAdmin,
	     lpSecurity, lpUserStore, lpAddrBook, lpMailer.get(), cbMsgEntryId,
	     lpMsgEntryId, &~lpMessage);
	if (hr != hrSuccess && hr != MAPI_E_WAIT && hr != MAPI_W_NO_SERVICE && lpMessage) {
		// use lpMailer to set body in SendUndeliverable
		if (!lpMailer->haveError())
			lpMailer->setError(_("Error found while trying to send your message. Error code: ") + wstringify(hr,true));
		hr = SendUndeliverable(lpMailer.get(), lpUserStore, lpMessage);
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
		if (bDoSentMail && lpUserSession && lpMessage)
			DoSentMail(NULL, lpUserStore, 0, std::move(lpMessage));
	}
	return hr;
}
