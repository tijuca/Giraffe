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
#include "nameids.h"
#include <mapix.h>

const WCHAR* nmStringNames[SIZE_NAMEDPROPS] = {
	L"Keywords", NULL
};

MAPINAMEID mnNamedProps[SIZE_NAMEDPROPS] = {
// lpwstrName:L"Keywords" may work, but gives a compile warning: ISO C++ does not allow designated initializers
	{(LPGUID)&PS_PUBLIC_STRINGS, MNID_STRING, { 0 }},
// ID names
	{(LPGUID)&PSETID_Meeting, MNID_ID, {dispidMeetingLocation}},
	{(LPGUID)&PSETID_Meeting, MNID_ID, {dispidGlobalObjectID}},
	{(LPGUID)&PSETID_Meeting, MNID_ID, {dispidIsRecurring}},
	{(LPGUID)&PSETID_Meeting, MNID_ID, {dispidCleanGlobalObjectID}},
	{(LPGUID)&PSETID_Meeting, MNID_ID, {dispidOwnerCriticalChange}},
	{(LPGUID)&PSETID_Meeting, MNID_ID, {dispidAttendeeCriticalChange}},
	{(LPGUID)&PSETID_Meeting, MNID_ID, {dispidOldWhenStartWhole}},
	{(LPGUID)&PSETID_Meeting, MNID_ID, {dispidIsException}},
	{(LPGUID)&PSETID_Meeting, MNID_ID, {dispidStartRecurrenceTime}},
	{(LPGUID)&PSETID_Meeting, MNID_ID, {dispidEndRecurrenceTime}},
	{(LPGUID)&PSETID_Zarafa_CalDav, MNID_ID, {dispidMozGen}}, //X-MOZ-GENERATION
	{(LPGUID)&PSETID_Zarafa_CalDav, MNID_ID, {dispidMozLastAck}}, //X-MOZ-LAST-ACK
	{(LPGUID)&PSETID_Zarafa_CalDav, MNID_ID, {dispidMozSnoozeSuffixTime}}, //X-MOZ-SNOOZE-TIME suffix
	{(LPGUID)&PSETID_Zarafa_CalDav, MNID_ID, {dispidMozSendInvite}}, //X-MOZ-SEND-INVITATIONS 
	{(LPGUID)&PSETID_Zarafa_CalDav, MNID_ID, {dispidApptTsRef}},
	{(LPGUID)&PSETID_Zarafa_CalDav, MNID_ID, {dispidFldID}},
	{(LPGUID)&PSETID_Appointment, MNID_ID, {dispidSendAsICAL}},
	{(LPGUID)&PSETID_Appointment, MNID_ID, {dispidAppointmentSequenceNumber}},
	{(LPGUID)&PSETID_Appointment, MNID_ID, {dispidApptSeqTime}},
	{(LPGUID)&PSETID_Appointment, MNID_ID, {dispidBusyStatus}},
	{(LPGUID)&PSETID_Appointment, MNID_ID, {dispidApptAuxFlags}},
	{(LPGUID)&PSETID_Appointment, MNID_ID, {dispidLocation}},
	{(LPGUID)&PSETID_Appointment, MNID_ID, {dispidLabel}},
	{(LPGUID)&PSETID_Appointment, MNID_ID, {dispidApptStartWhole}},
	{(LPGUID)&PSETID_Appointment, MNID_ID, {dispidApptEndWhole}},
	{(LPGUID)&PSETID_Appointment, MNID_ID, {dispidApptDuration}},
	{(LPGUID)&PSETID_Appointment, MNID_ID, {dispidAllDayEvent}},
	{(LPGUID)&PSETID_Appointment, MNID_ID, {dispidRecurrenceState}},
	{(LPGUID)&PSETID_Appointment, MNID_ID, {dispidMeetingStatus}},
	{(LPGUID)&PSETID_Appointment, MNID_ID, {dispidResponseStatus}},
	{(LPGUID)&PSETID_Appointment, MNID_ID, {dispidRecurring}},
	{(LPGUID)&PSETID_Appointment, MNID_ID, {dispidIntendedBusyStatus}},
	{(LPGUID)&PSETID_Appointment, MNID_ID, {dispidRecurringBase}},
	{(LPGUID)&PSETID_Appointment, MNID_ID, {dispidRequestSent}},		// aka PidLidFInvited
	{(LPGUID)&PSETID_Appointment, MNID_ID, {0x8230}},
	{(LPGUID)&PSETID_Appointment, MNID_ID, {dispidRecurrenceType}},
	{(LPGUID)&PSETID_Appointment, MNID_ID, {dispidRecurrencePattern}},
	{(LPGUID)&PSETID_Appointment, MNID_ID, {dispidTimeZoneData}},
	{(LPGUID)&PSETID_Appointment, MNID_ID, {dispidTimeZone}},
	{(LPGUID)&PSETID_Appointment, MNID_ID, {dispidClipStart}},
	{(LPGUID)&PSETID_Appointment, MNID_ID, {dispidClipEnd}},
	{(LPGUID)&PSETID_Appointment, MNID_ID, {dispidAllAttendeesString}},	// AllAttendees (Exluding self, ';' seperated)
	{(LPGUID)&PSETID_Appointment, MNID_ID, {dispidToAttendeesString}},	// RequiredAttendees (Including self)
	{(LPGUID)&PSETID_Appointment, MNID_ID, {dispidCCAttendeesString}},	// OptionalAttendees
	{(LPGUID)&PSETID_Appointment, MNID_ID, {dispidNetMeetingType}},
	{(LPGUID)&PSETID_Appointment, MNID_ID, {dispidNetMeetingServer}},
	{(LPGUID)&PSETID_Appointment, MNID_ID, {dispidNetMeetingOrganizerAlias}},
	{(LPGUID)&PSETID_Appointment, MNID_ID, {dispidNetMeetingAutoStart}},
	{(LPGUID)&PSETID_Appointment, MNID_ID, {dispidAutoStartWhen}},
	{(LPGUID)&PSETID_Appointment, MNID_ID, {dispidConferenceServerAllowExternal}},
	{(LPGUID)&PSETID_Appointment, MNID_ID, {dispidNetMeetingDocPathName}},
	{(LPGUID)&PSETID_Appointment, MNID_ID, {dispidNetShowURL}},
	{(LPGUID)&PSETID_Appointment, MNID_ID, {dispidConferenceServerPassword}},
	{(LPGUID)&PSETID_Appointment, MNID_ID, {dispidApptReplyTime}},
	{(LPGUID)&PSETID_Common, MNID_ID, {dispidReminderMinutesBeforeStart}},
	{(LPGUID)&PSETID_Common, MNID_ID, {dispidReminderTime}},
	{(LPGUID)&PSETID_Common, MNID_ID, {dispidReminderSet}},
	{(LPGUID)&PSETID_Common, MNID_ID, {dispidPrivate}},
	{(LPGUID)&PSETID_Common, MNID_ID, {dispidNoAging}},
	{(LPGUID)&PSETID_Common, MNID_ID, {dispidSideEffect}},
	{(LPGUID)&PSETID_Common, MNID_ID, {dispidRemoteStatus}},
	{(LPGUID)&PSETID_Common, MNID_ID, {dispidCommonStart}},
	{(LPGUID)&PSETID_Common, MNID_ID, {dispidCommonEnd}},
	{(LPGUID)&PSETID_Common, MNID_ID, {dispidCommonAssign}},
	{(LPGUID)&PSETID_Common, MNID_ID, {dispidContacts}},
	{(LPGUID)&PSETID_Common, MNID_ID, {dispidOutlookInternalVersion}},
	{(LPGUID)&PSETID_Common, MNID_ID, {dispidOutlookVersion}},
	{(LPGUID)&PSETID_Common, MNID_ID, {dispidReminderNextTime}},
	{(LPGUID)&PSETID_Common, MNID_ID, {dispidSmartNoAttach}},	
	{(LPGUID)&PSETID_Task, MNID_ID, {dispidTaskStatus}},
	{(LPGUID)&PSETID_Task, MNID_ID, {dispidTaskComplete}},
	{(LPGUID)&PSETID_Task, MNID_ID, {dispidTaskPercentComplete}},
	{(LPGUID)&PSETID_Task, MNID_ID, {dispidTaskStartDate}},
	{(LPGUID)&PSETID_Task, MNID_ID, {dispidTaskDueDate}},
	{(LPGUID)&PSETID_Task, MNID_ID, {dispidTaskRecurrenceState}},
	{(LPGUID)&PSETID_Task, MNID_ID, {dispidTaskIsRecurring}},
	{(LPGUID)&PSETID_Task, MNID_ID, {dispidTaskDateCompleted}}
};

/** 
 * Lookup all required named properties actual IDs for a given MAPI object.
 * 
 * @param[in]  lpPropObj Call GetIDsFromNames on this object.
 * @param[out] lppNamedProps Array of all named properties used in mapi ical conversions.
 * 
 * @return MAPI error code
 */
HRESULT HrLookupNames(IMAPIProp *lpPropObj, LPSPropTagArray *lppNamedProps)
{
	HRESULT hr = hrSuccess;
	LPMAPINAMEID *lppNameIds = NULL;
	LPSPropTagArray lpNamedProps = NULL;

	hr = MAPIAllocateBuffer(sizeof(LPMAPINAMEID) * SIZE_NAMEDPROPS, (void**)&lppNameIds);
	if (hr != hrSuccess)
		goto exit;

	for (int i = 0; i < SIZE_NAMEDPROPS; i++) {
		hr = MAPIAllocateMore(sizeof(MAPINAMEID), lppNameIds, (void**)&lppNameIds[i] );
		if (hr != hrSuccess)
			goto exit;

		memcpy(lppNameIds[i], &mnNamedProps[i], sizeof(MAPINAMEID) );
		if (mnNamedProps[i].ulKind == MNID_STRING && nmStringNames[i])
			lppNameIds[i]->Kind.lpwstrName = (WCHAR*)nmStringNames[i];
	}

	hr = lpPropObj->GetIDsFromNames(SIZE_NAMEDPROPS, lppNameIds, MAPI_CREATE, &lpNamedProps);
	if (FAILED(hr))
		goto exit;

	*lppNamedProps = lpNamedProps;
	hr = hrSuccess;

exit:
	if (lppNameIds)
		MAPIFreeBuffer(lppNameIds);

	return hr;
}
