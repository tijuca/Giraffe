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
#include "ICalToMAPI.h"
#include "vconverter.h"
#include "vtimezone.h"
#include "vevent.h"
#include "vtodo.h"
#include "vfreebusy.h"
#include "nameids.h"
#include "icalrecurrence.h"
#include <mapix.h>
#include <mapiutil.h>
#include "mapiext.h"
#include "restrictionutil.h"
#include <libical/ical.h>
#include <algorithm>
#include <vector>
#include "charset/convert.h"

class ICalToMapiImpl : public ICalToMapi {
public:
	/*
	    - lpPropObj to lookup named properties
	    - Addressbook (Zarafa Global AddressBook for looking up users)
	 */
	ICalToMapiImpl(IMAPIProp *lpPropObj, LPADRBOOK lpAdrBook, bool bNoRecipients);
	virtual ~ICalToMapiImpl();

	HRESULT ParseICal(const std::string& strIcal, const std::string& strCharset, const std::string& strServerTZ, IMailUser *lpMailUser, ULONG ulFlags);
	ULONG GetItemCount();
	HRESULT GetItemInfo(ULONG ulPosition, eIcalType *lpType, time_t *lptLastModified, SBinary *lpUid);
	HRESULT GetItemInfo(time_t *lptstart, time_t *lptend, std::string *lpstrUId, std::list<std::string> **lplstUsers);
	HRESULT GetItem(ULONG ulPosition, ULONG ulFlags, LPMESSAGE lpMessage);
	
	time_t m_tFbStart;
	time_t m_tFbEnd;
	std::string m_strUID;
	std::list<std::string> m_lstUsers;

private:
	void Clean();

	HRESULT SaveAttendeesString(std::list<icalrecip> *lplstRecip, LPMESSAGE lpMessage);
	HRESULT SaveProps(std::list<SPropValue> *lpPropList, LPMAPIPROP lpMapiProp);
	HRESULT SaveRecipList(std::list<icalrecip> *lplstRecip, ULONG ulFlag, LPMESSAGE lpMessage);
	LPSPropTagArray m_lpNamedProps;
	ULONG m_ulErrorCount;

	TIMEZONE_STRUCT ttServerTZ;
	std::string strServerTimeZone;

	/* Contains a list of messages after ParseICal
	 * Use GetItem() to get one of these messages
	 */
	std::vector<icalitem*> m_vMessages;
};

/** 
 * Create a class implementing the ICalToMapi "interface".
 * 
 * @param[in]  lpPropObj MAPI object used to find named properties
 * @param[in]  lpAdrBook MAPI Addressbook
 * @param[in]  bNoRecipients Skip recipients from ical. Used for DAgent, which uses the mail recipients
 * @param[out] lppICalToMapi The ICalToMapi class
 */
HRESULT CreateICalToMapi(IMAPIProp *lpPropObj, LPADRBOOK lpAdrBook, bool bNoRecipients, ICalToMapi **lppICalToMapi)
{
	if (!lpPropObj || !lpAdrBook || !lppICalToMapi)
		return MAPI_E_INVALID_PARAMETER;
	try {
		*lppICalToMapi = new ICalToMapiImpl(lpPropObj, lpAdrBook, bNoRecipients);
	} catch (...) {
		return MAPI_E_NOT_ENOUGH_MEMORY;
	}
	return hrSuccess;
}

/** 
 * Init ICalToMapi class
 * 
 * @param[in] lpPropObj passed to super class
 * @param[in] lpAdrBook passed to super class
 * @param[in] bNoRecipients passed to super class
 */
ICalToMapiImpl::ICalToMapiImpl(IMAPIProp *lpPropObj, LPADRBOOK lpAdrBook, bool bNoRecipients) : ICalToMapi(lpPropObj, lpAdrBook, bNoRecipients)
{
	m_lpNamedProps = NULL;
	memset(&ttServerTZ, 0, sizeof(TIMEZONE_STRUCT));
}

/** 
 * Frees all used memory of the ICalToMapi class
 */
ICalToMapiImpl::~ICalToMapiImpl()
{
	Clean();

	if (m_lpNamedProps)
		MAPIFreeBuffer(m_lpNamedProps);
}

/** 
 * Frees and resets all used memory of the ICalToMapi class. The class
 * can be reused for another conversion after this call. Named
 * properties are kept, since you cannot switch items from a store.
 */
void ICalToMapiImpl::Clean()
{
	m_ulErrorCount = 0;
	for (std::vector<icalitem*>::iterator i = m_vMessages.begin(); i != m_vMessages.end(); i++) {
		if ((*i)->lpRecurrence)
			delete (*i)->lpRecurrence;
		MAPIFreeBuffer((*i)->base);
		delete (*i);
	}
	m_vMessages.clear();
	m_lstUsers.clear();
	m_tFbStart = 0;
	m_tFbEnd = 0;
	m_strUID.clear();
}

/** 
 * Parses an ICal string (with a certain charset) and converts the
 * data in memory. The real MAPI object can be retrieved using
 * GetItem().
 * 
 * @param[in] strIcal The ICal data to parse
 * @param[in] strCharset The charset of strIcal (usually UTF-8)
 * @param[in] strServerTZparam ID of default timezone to use if ICal data didn't specify
 * @param[in] lpMailUser IMailUser object of the current user (CalDav: the user logged in, DAgent: the user being delivered for)
 * @param[in] ulFlags Conversion flags - currently unused
 * 
 * @return MAPI error code
 */
HRESULT ICalToMapiImpl::ParseICal(const std::string& strIcal, const std::string& strCharset, const std::string& strServerTZparam, IMailUser *lpMailUser, ULONG ulFlags)
{
	HRESULT hr = hrSuccess;
	VConverter *lpVEC = NULL;
	icalcomponent *lpicCalendar = NULL;
	icalcomponent *lpicComponent = NULL;
	TIMEZONE_STRUCT ttTimeZone = {0};
	timezone_map tzMap;
	std::string strTZID;
	icalitem *item = NULL;
	icalitem *previtem = NULL;

	Clean();
	if (m_lpNamedProps == NULL) {
		hr = HrLookupNames(m_lpPropObj, &m_lpNamedProps);
		if (hr != hrSuccess)
			goto exit;
	}

	icalerror_clear_errno();

	lpicCalendar = icalparser_parse_string(strIcal.c_str());

	if (lpicCalendar == NULL || icalerrno != ICAL_NO_ERROR) {
		switch (icalerrno) {
		case ICAL_BADARG_ERROR:
		case ICAL_USAGE_ERROR:
			hr = MAPI_E_INVALID_PARAMETER;
			break;
		case ICAL_NEWFAILED_ERROR:
		case ICAL_ALLOCATION_ERROR:
			hr = MAPI_E_NOT_ENOUGH_MEMORY;
			break;
		case ICAL_MALFORMEDDATA_ERROR:
			hr = MAPI_E_CORRUPT_DATA;
			break;
		case ICAL_FILE_ERROR:
			hr = MAPI_E_DISK_ERROR;
			break;
		case ICAL_UNIMPLEMENTED_ERROR:
			hr = E_NOTIMPL;
		case ICAL_UNKNOWN_ERROR:
		case ICAL_PARSE_ERROR:
		case ICAL_INTERNAL_ERROR:
		case ICAL_NO_ERROR:
			hr = MAPI_E_CALL_FAILED;
			break;
		};
		goto exit;
	}

	if (icalcomponent_isa(lpicCalendar) != ICAL_VCALENDAR_COMPONENT && icalcomponent_isa(lpicCalendar) != ICAL_XROOT_COMPONENT) {
		hr = MAPI_E_INVALID_OBJECT;
		goto exit;
	}

	m_ulErrorCount = icalcomponent_count_errors(lpicCalendar);

	// * find all timezone's, place in map
	lpicComponent = icalcomponent_get_first_component(lpicCalendar, ICAL_VTIMEZONE_COMPONENT);
	while (lpicComponent) {
		hr = HrParseVTimeZone(lpicComponent, &strTZID, &ttTimeZone);
		if (hr != hrSuccess) {
			// log warning?
		} else {
			tzMap[strTZID] = ttTimeZone;
		}

		lpicComponent = icalcomponent_get_next_component(lpicCalendar, ICAL_VTIMEZONE_COMPONENT);
	}

	// ICal file did not send any timezone information
	if (tzMap.empty() && strServerTimeZone.empty())
	{
		// find timezone from given server timezone
		if (HrGetTzStruct(strServerTZparam, &ttServerTZ) == hrSuccess)
		{
			strServerTimeZone = strServerTZparam;
			tzMap[strServerTZparam] = ttServerTZ;
		}
	}

	// find all "messages" vevent, vtodo, vjournal, ...?
	lpicComponent = icalcomponent_get_first_component(lpicCalendar, ICAL_ANY_COMPONENT);
	while (lpicComponent) {
		switch (icalcomponent_isa(lpicComponent)) {
		case ICAL_VEVENT_COMPONENT:
			lpVEC = new VEventConverter(m_lpAdrBook, &tzMap, m_lpNamedProps, strCharset, false, m_bNoRecipients, lpMailUser);
			hr = hrSuccess;
			break;
		case ICAL_VTODO_COMPONENT:
			lpVEC = new VTodoConverter(m_lpAdrBook, &tzMap, m_lpNamedProps, strCharset, false, m_bNoRecipients, lpMailUser);
			hr = hrSuccess;
			break;
		case ICAL_VFREEBUSY_COMPONENT:
			hr = hrSuccess;
			break;
		case ICAL_VJOURNAL_COMPONENT:
		default:
			hr = MAPI_E_NO_SUPPORT;
			break;
		};

		if (hr != hrSuccess)
			goto next;

		switch(icalcomponent_isa(lpicComponent)) {			
		case ICAL_VFREEBUSY_COMPONENT:
			hr = HrGetFbInfo(lpicComponent, &m_tFbStart, &m_tFbEnd, &m_strUID, &m_lstUsers);
			break;
		case ICAL_VEVENT_COMPONENT:
		case ICAL_VTODO_COMPONENT:
			hr = lpVEC->HrICal2MAPI(lpicCalendar, lpicComponent, previtem, &item);			
			break;
		default:
			break;
		};

		if (hr == hrSuccess && item) {
			// previtem is equal to item when item was only updated (eg. vevent exception)
			if (previtem != item) {
				m_vMessages.push_back(item);
				previtem = item;
			}
		}
next:
		if (lpVEC) {
			delete lpVEC;
			lpVEC = NULL;
		}

		lpicComponent = icalcomponent_get_next_component(lpicCalendar, ICAL_ANY_COMPONENT);
	}
	hr = hrSuccess;

	// TODO: sort m_vMessages on sBinGuid in icalitem struct, so caldav server can use optimized algorithm for finding the same items in MAPI

	// seems this happens quite fast .. don't know what's wrong with exchange's ical
// 	if (m_ulErrorCount != 0)
// 		hr = MAPI_W_ERRORS_RETURNED;

exit:
	if (lpVEC)
		delete lpVEC;

	if (lpicCalendar)
		icalcomponent_free(lpicCalendar);

	return hr;
}

/** 
 * Returns the number of items parsed
 * 
 * @return Max number +1 to pass in GetItem and GetItemInfo ulPosition parameter.
 */
ULONG ICalToMapiImpl::GetItemCount()
{
	return m_vMessages.size();
}

/** 
 * Get some information about an ical item at a certain position.
 * 
 * @param[in]  ulPosition The position of the ical item (flat list)
 * @param[out] lpType Type of the object, VEVENT or VTODO. (VJOURNAL currently unsupported), NULL if not intrested
 * @param[out] lptLastModified Last modification timestamp of the object, NULL if not intrested
 * @param[out] lpUid UID of the object, NULL if not intrested
 * 
 * @return MAPI error code
 * @retval MAPI_E_INVALID_PARAMETER ulPosition out of range, or all return values NULL
 */
HRESULT ICalToMapiImpl::GetItemInfo(ULONG ulPosition, eIcalType *lpType, time_t *lptLastModified, SBinary *lpUid)
{
	HRESULT hr = hrSuccess;

	if (ulPosition >= m_vMessages.size()) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}
	if (lpType == NULL && lptLastModified == NULL && lpUid == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	if (lpType)
		*lpType = m_vMessages[ulPosition]->eType;

	if (lptLastModified)
		*lptLastModified = m_vMessages[ulPosition]->tLastModified;

	if (lpUid)
		*lpUid = m_vMessages[ulPosition]->sBinGuid.Value.bin;
	
exit:
	return hr;
}

/** 
 * Get information of the freebusy data in the parsed ical.
 * @todo return an error if no freebusy data was ever parsed.
 * 
 * @param[out] lptstart The start time of the freebusy data
 * @param[out] lptend  The end time of the freebusy data
 * @param[out] lpstrUID The UID of the freebusy data
 * @param[out] lplstUsers A list of the email addresses of users in freebusy data
 * 
 * @return always hrSuccess
 */
HRESULT ICalToMapiImpl::GetItemInfo(time_t *lptstart, time_t *lptend, std::string *lpstrUID, std::list<std::string> **lplstUsers)
{

	*lptend = m_tFbEnd;
	*lptstart = m_tFbStart;
	*lpstrUID = m_strUID;
	*lplstUsers = &m_lstUsers;

	return hrSuccess;
}

/**
 * Sets mapi properties in Imessage object from the icalitem.
 *
 * @param[in]		ulPosition		specifies the message that is to be retreived
 * @param[in]		ulFlags			conversion flags
 * @arg @c IC2M_NO_RECIPIENTS skip recipients in conversion from ICal to MAPI
 * @arg @c IC2M_APPEND_ONLY	do not delete properties in lpMessage that are not present in ICal, but possebly are in lpMessage
 * @param[in,out]	lpMessage		IMessage in which properties has to be set
 *
 * @return			MAPI error code
 * @retval			MAPI_E_INVALID_PARAMETER	invalid position set in ulPosition or NULL IMessage parameter
 */
HRESULT ICalToMapiImpl::GetItem(ULONG ulPosition, ULONG ulFlags, LPMESSAGE lpMessage)
{
	HRESULT hr = hrSuccess;
	ICalRecurrence cRec;
	icalitem *lpItem = NULL;
	std::vector<icalitem*>::iterator iItem;
	std::list<icalitem::exception>::iterator iEx;
	ULONG ulANr = 0;
	LPATTACH lpAttach = NULL;
	LPMESSAGE lpExMsg = NULL;
	LPSPropTagArray lpsPTA = NULL;
	LPMAPITABLE lpAttachTable = NULL;
	LPSRestriction lpAttachRestrict = NULL;
	LPSRestriction lpPRestrictAnd = NULL;
	LPSRestriction lpPRestrictOr = NULL;
	LPSRowSet lpRows = NULL;
	LPSPropValue lpPropVal = NULL;
	SPropValue sStart = {0};
	SPropValue sMethod = {0};

	if (ulPosition >= m_vMessages.size() || lpMessage == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	iItem = m_vMessages.begin() + ulPosition;
	lpItem = *iItem;

	if ((ulFlags & IC2M_APPEND_ONLY) == 0 && !lpItem->lstDelPropTags.empty()) {
		hr = MAPIAllocateBuffer(CbNewSPropTagArray(lpItem->lstDelPropTags.size()), (LPVOID *)&lpsPTA);
		if (hr != hrSuccess)
			goto exit;

		std::copy(lpItem->lstDelPropTags.begin(), lpItem->lstDelPropTags.end(), lpsPTA->aulPropTag);

		lpsPTA->cValues = lpItem->lstDelPropTags.size();

		hr = lpMessage->DeleteProps(lpsPTA, NULL);
		if (hr != hrSuccess)
			goto exit;
	}

	hr = SaveProps(&(lpItem->lstMsgProps), lpMessage);
	if (hr != hrSuccess)
		goto exit;
	
	if (!(ulFlags & IC2M_NO_RECIPIENTS))
		hr = SaveRecipList(&(lpItem->lstRecips), ulFlags, lpMessage);

	if (hr != hrSuccess)
		goto exit;

	hr = SaveAttendeesString(&(lpItem->lstRecips), lpMessage);
	if (hr != hrSuccess)
		goto exit;

	// remove all exception attachments from message, if any
	hr = lpMessage->GetAttachmentTable(0, &lpAttachTable);
	if (hr != hrSuccess)
		goto next;

	sStart.ulPropTag = PR_EXCEPTION_STARTTIME;
	// 1-1-4501 00:00
	sStart.Value.ft.dwLowDateTime = 0xA3DD4000;
	sStart.Value.ft.dwHighDateTime = 0x0CB34557;
	sMethod.ulPropTag = PR_ATTACH_METHOD;
	sMethod.Value.ul = ATTACH_EMBEDDED_MSG;

	// restrict to only exception attachments
	// (((!pr_exception_starttime) or (pr_exception_starttime != 1-1-4501)) and (have method) and (method == 5))
	// (AND( OR( NOT(pr_exception_start) (pr_exception_start!=time)) (have method)(method==5)))
	CREATE_RESTRICTION(lpAttachRestrict);

	CREATE_RES_AND(lpAttachRestrict, lpAttachRestrict, 3);
	lpPRestrictAnd = lpAttachRestrict->res.resAnd.lpRes;
	{
		CREATE_RES_OR(lpAttachRestrict, (&lpPRestrictAnd[0]), 2);
		lpPRestrictOr = lpPRestrictAnd[0].res.resOr.lpRes;

		// or
		CREATE_RES_NOT(lpAttachRestrict, (&lpPRestrictOr[0]));
		DATA_RES_EXIST(lpAttachRestrict, lpPRestrictOr[0].res.resNot.lpRes[0], sStart.ulPropTag);
	
		DATA_RES_PROPERTY(lpAttachRestrict, lpPRestrictOr[1], RELOP_NE, sStart.ulPropTag, &sStart);
	}

	// and
	DATA_RES_EXIST(lpAttachRestrict, lpPRestrictAnd[1], sMethod.ulPropTag);
	DATA_RES_PROPERTY(lpAttachRestrict, lpPRestrictAnd[2], RELOP_EQ, sMethod.ulPropTag, &sMethod);

	hr = lpAttachTable->Restrict(lpAttachRestrict, 0);
	if (hr != hrSuccess)
		goto exit;

	hr = lpAttachTable->QueryRows(-1, 0, &lpRows);
	if (hr != hrSuccess)
		goto exit;

	for (ULONG i = 0; i < lpRows->cRows; i++) {
		lpPropVal = PpropFindProp(lpRows->aRow[i].lpProps, lpRows->aRow[i].cValues, PR_ATTACH_NUM);
		if (lpPropVal == NULL)
			continue;

		hr = lpMessage->DeleteAttach(lpPropVal->Value.ul, 0, NULL, 0);
		if (hr != hrSuccess)
			goto exit;
	}

next:
	// add recurring properties & add exceptions
	if (lpItem->lpRecurrence) {
		hr = cRec.HrMakeMAPIRecurrence(lpItem->lpRecurrence, m_lpNamedProps, lpMessage);
		// TODO: log error if any?
		
		// check if all exceptions are valid
		for (iEx = lpItem->lstExceptionAttachments.begin(); iEx != lpItem->lstExceptionAttachments.end(); iEx++) {
			
			if (cRec.HrValidateOccurrence(lpItem, *iEx) == false) {
				hr = MAPI_E_INVALID_OBJECT;
				goto exit;
			}
		}

		for (iEx = lpItem->lstExceptionAttachments.begin(); iEx != lpItem->lstExceptionAttachments.end(); iEx++) {
			hr = lpMessage->CreateAttach(NULL, 0, &ulANr, &lpAttach);
			if (hr != hrSuccess)
				goto exit;

			hr = lpAttach->OpenProperty(PR_ATTACH_DATA_OBJ, &IID_IMessage, 0, MAPI_CREATE | MAPI_MODIFY, (LPUNKNOWN *)&lpExMsg);
			if (hr != hrSuccess)
				goto exit;
			
			if (!(ulFlags & IC2M_NO_RECIPIENTS))
				hr = SaveRecipList(&(iEx->lstRecips), ulFlags, lpExMsg);

			if (hr != hrSuccess)
				goto exit;
			
			hr = SaveAttendeesString(&(iEx->lstRecips), lpExMsg);
			if (hr != hrSuccess)
				goto exit;

			hr = SaveProps(&(iEx->lstMsgProps), lpExMsg);
			if (hr != hrSuccess)
				goto exit;

			hr = SaveProps(&(iEx->lstAttachProps), lpAttach);
			if (hr != hrSuccess)
				goto exit;

			hr = lpExMsg->SaveChanges(0);
			if (hr != hrSuccess)
				goto exit;

			hr = lpAttach->SaveChanges(0);
			if (hr != hrSuccess)
				goto exit;

			lpExMsg->Release();
			lpExMsg = NULL;

			lpAttach->Release();
			lpAttach = NULL;
		}
	}

exit:
	if (lpAttachRestrict)
		FREE_RESTRICTION(lpAttachRestrict);

	if (lpAttachTable)
		lpAttachTable->Release();

	if (lpRows)
		FreeProws(lpRows);

	if (lpsPTA)
		MAPIFreeBuffer(lpsPTA);

	if (lpAttach)
		lpAttach->Release();

	if (lpExMsg)
		lpExMsg->Release();

	return hr;
}

/** 
 * Helper function for GetItem. Saves all properties converted from
 * ICal to MAPI in the MAPI object. Does not call SaveChanges.
 * 
 * @param[in] lpPropList list of properties to save in lpMapiProp
 * @param[in] lpMapiProp The MAPI object to save properties in
 * 
 * @return MAPI error code
 */
HRESULT ICalToMapiImpl::SaveProps(std::list<SPropValue> *lpPropList, LPMAPIPROP lpMapiProp)
{
	HRESULT hr = hrSuccess;
	std::list<SPropValue>::iterator iProps;
	LPSPropValue lpsPropVals = NULL;
	int i;

	// all props to message
	hr = MAPIAllocateBuffer(lpPropList->size() * sizeof(SPropValue), (void**)&lpsPropVals);
	if (hr != hrSuccess)
		goto exit;

	// @todo: add exclude list or something? might set props the caller doesn't want (see vevent::HrAddTimes())
	for (i = 0, iProps = lpPropList->begin(); iProps != lpPropList->end(); iProps++, i++)
		lpsPropVals[i] = *iProps;

	hr = lpMapiProp->SetProps(i, lpsPropVals, NULL);
	if (FAILED(hr))
		goto exit;

exit:
	if (lpsPropVals)
		MAPIFreeBuffer(lpsPropVals);

	return hr;
}

/**
 * Helper function for GetItem. Converts recipient list to MAPI
 * recipients. Always replaces the complete recipient list in
 * lpMessage.
 * 
 * @param[in] lplstRecip List of parsed ical recipients from a certain item
 * @param[in] lpMessage MAPI message to modify recipient table for
 * 
 * @return MAPI error code
 */
HRESULT ICalToMapiImpl::SaveRecipList(std::list<icalrecip> *lplstRecip, ULONG ulFlag, LPMESSAGE lpMessage)
{
	HRESULT hr = hrSuccess;
	LPADRLIST lpRecipients = NULL;
	std::list<icalrecip>::iterator iRecip;
	std::string strSearch;
	ULONG i = 0;
	convert_context converter;

	hr = MAPIAllocateBuffer(CbNewADRLIST(lplstRecip->size()), (void**)&lpRecipients);
	if (hr != hrSuccess)
		goto exit;

	lpRecipients->cEntries = 0;

	for (iRecip = lplstRecip->begin(); iRecip != lplstRecip->end(); iRecip++) {
		// iRecip->ulRecipientType
		// strEmail
		// strName
		// cbEntryID, lpEntryID

		if ((ulFlag & IC2M_NO_ORGANIZER) && iRecip->ulRecipientType == MAPI_ORIG)
			continue;
			
		MAPIAllocateBuffer(sizeof(SPropValue)*10, (void**)&lpRecipients->aEntries[i].rgPropVals);
		lpRecipients->aEntries[i].cValues = 10;

		lpRecipients->aEntries[i].rgPropVals[0].ulPropTag = PR_RECIPIENT_TYPE;
		lpRecipients->aEntries[i].rgPropVals[0].Value.ul = iRecip->ulRecipientType;

		lpRecipients->aEntries[i].rgPropVals[1].ulPropTag = PR_DISPLAY_NAME_W;
		lpRecipients->aEntries[i].rgPropVals[1].Value.lpszW = (WCHAR*)iRecip->strName.c_str();

		lpRecipients->aEntries[i].rgPropVals[2].ulPropTag = PR_SMTP_ADDRESS_W;
		lpRecipients->aEntries[i].rgPropVals[2].Value.lpszW = (WCHAR*)iRecip->strEmail.c_str();
		
		lpRecipients->aEntries[i].rgPropVals[3].ulPropTag = PR_ENTRYID;
		lpRecipients->aEntries[i].rgPropVals[3].Value.bin.cb = iRecip->cbEntryID;
		MAPIAllocateMore(iRecip->cbEntryID, lpRecipients->aEntries[i].rgPropVals, (void**)&lpRecipients->aEntries[i].rgPropVals[3].Value.bin.lpb);
		memcpy(lpRecipients->aEntries[i].rgPropVals[3].Value.bin.lpb, iRecip->lpEntryID, iRecip->cbEntryID);
		
		lpRecipients->aEntries[i].rgPropVals[4].ulPropTag = PR_ADDRTYPE_W;
		lpRecipients->aEntries[i].rgPropVals[4].Value.lpszW = L"SMTP";

		strSearch = "SMTP:" + converter.convert_to<std::string>(iRecip->strEmail);
		transform(strSearch.begin(), strSearch.end(), strSearch.begin(), ::toupper);
		lpRecipients->aEntries[i].rgPropVals[5].ulPropTag = PR_SEARCH_KEY;
		lpRecipients->aEntries[i].rgPropVals[5].Value.bin.cb = strSearch.size() + 1;
		MAPIAllocateMore(strSearch.size()+1, lpRecipients->aEntries[i].rgPropVals, (void **)&lpRecipients->aEntries[i].rgPropVals[5].Value.bin.lpb);
		memcpy(lpRecipients->aEntries[i].rgPropVals[5].Value.bin.lpb, strSearch.c_str(), strSearch.size()+1);

		lpRecipients->aEntries[i].rgPropVals[6].ulPropTag = PR_EMAIL_ADDRESS_W;
		lpRecipients->aEntries[i].rgPropVals[6].Value.lpszW = (WCHAR*)iRecip->strEmail.c_str();

		lpRecipients->aEntries[i].rgPropVals[7].ulPropTag = PR_DISPLAY_TYPE;
		lpRecipients->aEntries[i].rgPropVals[7].Value.ul = DT_MAILUSER;

		lpRecipients->aEntries[i].rgPropVals[8].ulPropTag = PR_RECIPIENT_FLAGS;
		lpRecipients->aEntries[i].rgPropVals[8].Value.ul = iRecip->ulRecipientType == MAPI_ORIG? 3 : 1;

		lpRecipients->aEntries[i].rgPropVals[9].ulPropTag = PR_RECIPIENT_TRACKSTATUS;
		lpRecipients->aEntries[i].rgPropVals[9].Value.ul = iRecip->ulTrackStatus;

		lpRecipients->cEntries++;
		i++;
	}

	// flag 0: remove old recipient table, and add the new list
	hr = lpMessage->ModifyRecipients(0, lpRecipients);	
	if (hr != hrSuccess)
		goto exit;

exit:
	
	if (lpRecipients)
		FreePadrlist(lpRecipients);

	return hr;
}

/**
 * Save attendees strings in the message
 *
 * @param[in] 		lplstRecip	Pointer to a recipient list.
 * @param[in,out]   lpMessage	Pointer to the new MAPI message.
 *
 * @return	Returns always S_OK except when there is an error.
 *
 * @note  The properties dispidNonSendableCC, dispidNonSendableBCC are not set
 *
 */
HRESULT ICalToMapiImpl::SaveAttendeesString(std::list<icalrecip> *lplstRecip, LPMESSAGE lpMessage)
{
	HRESULT hr = hrSuccess;

	std::list<icalrecip>::iterator iRecip;
	std::wstring strAllAttendees;
	std::wstring strToAttendees;
	std::wstring strCCAttendees;
	LPSPropValue lpsPropValue = NULL;

	hr = MAPIAllocateBuffer(sizeof(SPropValue) * 3, (LPVOID*)&lpsPropValue);
	if (hr != hrSuccess)
		goto exit;

	// Create attendees string
	for (iRecip = lplstRecip->begin(); iRecip != lplstRecip->end(); iRecip++) {

		if (iRecip->ulRecipientType == MAPI_ORIG)
			continue;

		if (iRecip->ulRecipientType == MAPI_TO) {
			if (!strToAttendees.empty())
				strToAttendees += L"; ";

			strToAttendees += iRecip->strName;
		} else if(iRecip->ulRecipientType == MAPI_CC) {
			if (!strCCAttendees.empty())
				strCCAttendees += L"; ";

			strCCAttendees += iRecip->strName;
		}
		if (!strAllAttendees.empty())
			strAllAttendees += L"; ";

		strAllAttendees += iRecip->strName;
	}

	lpsPropValue[0].ulPropTag = CHANGE_PROP_TYPE(m_lpNamedProps->aulPropTag[PROP_TOATTENDEESSTRING], PT_UNICODE);
	lpsPropValue[0].Value.lpszW = (WCHAR*)strToAttendees.c_str();
	lpsPropValue[1].ulPropTag = CHANGE_PROP_TYPE(m_lpNamedProps->aulPropTag[PROP_CCATTENDEESSTRING], PT_UNICODE);
	lpsPropValue[1].Value.lpszW = (WCHAR*)strCCAttendees.c_str();
	lpsPropValue[2].ulPropTag = CHANGE_PROP_TYPE(m_lpNamedProps->aulPropTag[PROP_ALLATTENDEESSTRING], PT_UNICODE);
	lpsPropValue[2].Value.lpszW = (WCHAR*)strAllAttendees.c_str();

	hr = lpMessage->SetProps(3, lpsPropValue, NULL);
	if (hr != hrSuccess)
		goto exit;

exit:
	if(lpsPropValue)
		MAPIFreeBuffer(lpsPropValue);

	return hr;
}
