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
#include "vevent.h"
#include <mapiutil.h>
#include "mapiext.h"
#include "CommonUtil.h"
#include "nameids.h"
#include "icaluid.h"
#include "stringutil.h"

/** 
 * VEvent constructor, implements VConverter
 */
VEventConverter::VEventConverter(LPADRBOOK lpAdrBook, timezone_map *mapTimeZones, LPSPropTagArray lpNamedProps, const std::string& strCharset, bool blCensor, bool bNoRecipients, IMailUser *lpMailUser)
	: VConverter(lpAdrBook, mapTimeZones, lpNamedProps, strCharset, blCensor, bNoRecipients, lpMailUser)
{
}

/** 
 * VEvent destructor
 */
VEventConverter::~VEventConverter()
{
}

/** 
 * Entrypoint to convert an ical object to MAPI object.
 * 
 * @param[in]  lpEventRoot The root component (VCALENDAR top object)
 * @param[in]  lpEvent The VEVENT object to convert
 * @param[in]  lpPrevItem Optional previous (top) item to use when lpEvent describes an exception
 * @param[out] lppRet The icalitem struct to finalize into a MAPI object
 * 
 * @return MAPI error code
 */
HRESULT VEventConverter::HrICal2MAPI(icalcomponent *lpEventRoot, icalcomponent *lpEvent, icalitem *lpPrevItem, icalitem **lppRet)
{
	HRESULT hr = hrSuccess;

	hr = VConverter::HrICal2MAPI(lpEventRoot, lpEvent, lpPrevItem, lppRet);
	if (hr != hrSuccess)
		goto exit;

	(*lppRet)->eType = VEVENT;
	
exit:
	return hr;
}

/** 
 * The properties set here are all required base properties for
 * different calender items and meeting requests.
 *
 * Finds the difference if we're handling this message as the
 * organiser or as an attendee. Uses that and the method to set the
 * correct response and meeting status. PR_MESSAGE_CLASS is only set
 * on the main message, not on exceptions.  For PUBLISH methods, it
 * will also set the appointment reply time property. Lastly, the icon
 * index (outlook icon displayed in list view) is set.
 *
 * @note We only handle the methods REQUEST, REPLY and CANCEL
 * according to dagent/spooler/gateway fashion (that is, meeting
 * requests in emails) and PUBLISH for iCal/CalDAV (that is, pure
 * calendar items). Meeting requests through the PUBLISH method (as
 * also described in the Microsoft documentations) is not supported.
 * 
 * @param[in]  icMethod Method of the ical event
 * @param[in]  lpicEvent The ical VEVENT to convert
 * @param[in]  base Used for the 'base' pointer for memory allocations
 * @param[in]  bisException Weather we're handling an exception or not
 * @param[in,out] lstMsgProps 
 * 
 * @return MAPI error code
 */
HRESULT VEventConverter::HrAddBaseProperties(icalproperty_method icMethod, icalcomponent *lpicEvent, void *base, bool bisException, std::list<SPropValue> *lstMsgProps)
{
	HRESULT hr = hrSuccess;
	icalproperty *icProp = NULL;
	icalparameter *icParam = NULL;
	SPropValue sPropVal;
	bool bMeeting = false;
	bool bMeetingOrganised = false;
	std::wstring strEmail;
	time_t tNow = 0;

	icProp = icalcomponent_get_first_property(lpicEvent, ICAL_ORGANIZER_PROPERTY);
	if (icProp) 
	{
		const char *lpszProp = icalproperty_get_organizer(icProp);
		strEmail = m_converter.convert_to<std::wstring>(lpszProp, rawsize(lpszProp), m_strCharset.c_str());
		if (wcsncasecmp(strEmail.c_str(), L"mailto:", 7) == 0)
			strEmail.erase(0, 7);
		
		if (bIsUserLoggedIn(strEmail))
			bMeetingOrganised = true;
	}

	// @note setting PR_MESSAGE_CLASS must be the last entry in the case.
	switch (icMethod) {
	case ICAL_METHOD_REQUEST:
		bMeeting = true;

		sPropVal.ulPropTag = PR_RESPONSE_REQUESTED;
		sPropVal.Value.b = true;
		lstMsgProps->push_back(sPropVal);

		sPropVal.ulPropTag = CHANGE_PROP_TYPE(m_lpNamedProps->aulPropTag[PROP_RESPONSESTATUS], PT_LONG);
		sPropVal.Value.ul = respNotResponded;
		lstMsgProps->push_back(sPropVal);

		HrCopyString(base, L"IPM.Schedule.Meeting.Request", &sPropVal.Value.lpszW);
		break;

	case ICAL_METHOD_REPLY:
		// This value with respAccepted/respDeclined/respTentative is only for imported items through the PUBLISH method, which we do not support.
		sPropVal.ulPropTag = CHANGE_PROP_TYPE(m_lpNamedProps->aulPropTag[PROP_RESPONSESTATUS], PT_LONG);
		sPropVal.Value.ul = respNone;
		lstMsgProps->push_back(sPropVal);
		
		// skip the meetingstatus property
		bMeeting = false;

		// A reply message must have only one attendee, the attendee replying
		if (icalcomponent_count_properties(lpicEvent, ICAL_ATTENDEE_PROPERTY) != 1) {
			hr = MAPI_E_CALL_FAILED;
			goto exit;
		}

		icProp = icalcomponent_get_first_property(lpicEvent, ICAL_ATTENDEE_PROPERTY);
		if (!icProp) {
			hr = MAPI_E_CALL_FAILED;
			goto exit;
		}

		icParam = icalproperty_get_first_parameter(icProp, ICAL_PARTSTAT_PARAMETER);
		if (!icParam) {
			hr = MAPI_E_CALL_FAILED;
			goto exit;
		}

		switch (icalparameter_get_partstat(icParam)) {
		case ICAL_PARTSTAT_ACCEPTED:
			HrCopyString(base, L"IPM.Schedule.Meeting.Resp.Pos", &sPropVal.Value.lpszW);
			break;

		case ICAL_PARTSTAT_DECLINED:
			HrCopyString(base, L"IPM.Schedule.Meeting.Resp.Neg", &sPropVal.Value.lpszW);
			break;

		case ICAL_PARTSTAT_TENTATIVE:
			HrCopyString(base, L"IPM.Schedule.Meeting.Resp.Tent", &sPropVal.Value.lpszW);
			break;

		default:
			hr = MAPI_E_TYPE_NO_SUPPORT;
			goto exit;
		}
		break;

	case ICAL_METHOD_CANCEL:
		sPropVal.ulPropTag = CHANGE_PROP_TYPE(m_lpNamedProps->aulPropTag[PROP_RESPONSESTATUS], PT_LONG);
		sPropVal.Value.ul = respNotResponded;
		lstMsgProps->push_back(sPropVal);

		// make sure the cancel flag gets set
		bMeeting = true;

		HrCopyString(base, L"IPM.Schedule.Meeting.Canceled", &sPropVal.Value.lpszW);
		break;

	case ICAL_METHOD_PUBLISH:
	default:
		
		// set ResponseStatus to 0 fix for BlackBerry.
		sPropVal.ulPropTag = CHANGE_PROP_TYPE(m_lpNamedProps->aulPropTag[PROP_RESPONSESTATUS], PT_LONG);
		if (m_ulUserStatus != 0)
			sPropVal.Value.ul = m_ulUserStatus;
		else if (bMeetingOrganised)
			sPropVal.Value.ul = 1;
		else
			sPropVal.Value.ul = 0;
		lstMsgProps->push_back(sPropVal);
		
		// time(NULL) returns UTC time as libical sets application to UTC time.
		tNow = time(NULL);
		sPropVal.ulPropTag = CHANGE_PROP_TYPE(m_lpNamedProps->aulPropTag[PROP_APPTREPLYTIME], PT_SYSTIME);		
		UnixTimeToFileTime(tNow, &sPropVal.Value.ft);
		lstMsgProps->push_back(sPropVal);

		// Publish is used when mixed events are in the vcalender
		// we should determine on other properties if this is a meeting request related item
		HrCopyString(base, L"IPM.Appointment", &sPropVal.Value.lpszW);

		// if we don't have attendees, skip the meeting request props
		if (icalcomponent_get_first_property(lpicEvent, ICAL_ATTENDEE_PROPERTY) == NULL)
			bMeeting = false;
		else
			bMeeting = true;	//if Attendee is present then set the PROP_MEETINGSTATUS property

		break;
	}

	if (!bisException) {
		sPropVal.ulPropTag = PR_MESSAGE_CLASS_W;
		lstMsgProps->push_back(sPropVal);
	}


	if (icMethod == ICAL_METHOD_CANCEL || icMethod == ICAL_METHOD_REQUEST)
	{
		sPropVal.ulPropTag = CHANGE_PROP_TYPE(m_lpNamedProps->aulPropTag[PROP_REQUESTSENT], PT_BOOLEAN); 
		sPropVal.Value.b = true;
		lstMsgProps->push_back(sPropVal);
	} else if (icMethod == ICAL_METHOD_REPLY) {
		// This is only because outlook and the examples say so in [MS-OXCICAL].pdf
		// Otherwise, it's completely contradictionary to what the documentation describes.
		sPropVal.ulPropTag = CHANGE_PROP_TYPE(m_lpNamedProps->aulPropTag[PROP_REQUESTSENT], PT_BOOLEAN); 
		sPropVal.Value.b = false;
		lstMsgProps->push_back(sPropVal);
	} else {
		// PUBLISH method, depends on if we're the owner of the object
		sPropVal.ulPropTag = CHANGE_PROP_TYPE(m_lpNamedProps->aulPropTag[PROP_REQUESTSENT], PT_BOOLEAN); 
		sPropVal.Value.b = bMeetingOrganised;
		lstMsgProps->push_back(sPropVal);
	}

	sPropVal.ulPropTag = CHANGE_PROP_TYPE(m_lpNamedProps->aulPropTag[PROP_MEETINGSTATUS], PT_LONG);
	if (bMeeting) {
		// Set meeting status flags
		sPropVal.Value.ul = 1 | 2; // this-is-a-meeting-object flag + received flag
		if (icMethod == ICAL_METHOD_CANCEL)
			sPropVal.Value.ul |= 4; // cancelled flag
		else if(bMeetingOrganised)
			sPropVal.Value.ul = 1;
	} else {
		sPropVal.Value.ul = 0; // this-is-a-meeting-object flag off
	}
	lstMsgProps->push_back(sPropVal);

	sPropVal.ulPropTag = PR_ICON_INDEX;
	if (bMeeting) {
		if (icMethod == ICAL_METHOD_CANCEL) {
			sPropVal.Value.ul = ICON_APPT_MEETING_CANCEL;
		} else {
			sPropVal.Value.ul = ICON_APPT_MEETING_SINGLE;
		}
	} else {
		sPropVal.Value.ul = ICON_APPT_APPOINTMENT;
	}

	// 1024: normal calendar item
	// 1025: recurring item
	// 1026: meeting request
	// 1027: recurring meeting request
	lstMsgProps->push_back(sPropVal);

exit:
	return hr;
}

/**
 * Set time properties in icalitem from the ical data
 *
 * @param[in]	lpicEventRoot	ical VCALENDAR component to set the timezone
 * @param[in]	lpicEvent		ical VEVENT component
 * @param[in]	bIsAllday		set times for normal or allday event
 * @param[out]	lpIcalItem		icalitem structure in which mapi properties are set
 * @return		MAPI error code
 * @retval		MAPI_E_INVALID_PARAMETER	start time or timezone not present in ical data
 */
HRESULT VEventConverter::HrAddTimes(icalcomponent *lpicEventRoot, icalcomponent *lpicEvent, bool bIsAllday, icalitem *lpIcalItem)
{
	HRESULT hr = hrSuccess;
	SPropValue sPropVal;
	icalproperty* lpicDTStartProp = NULL;
	icalproperty* lpicDTEndProp = NULL;
	time_t timeDTStartUTC = 0, timeDTEndUTC = 0;
	time_t timeDTStartLocal = 0, timeDTEndLocal = 0;
	time_t timeEndOffset = 0, timeStartOffset = 0;
	icalproperty* lpicDurationProp = NULL;
	icalproperty* lpicProp = NULL;

	// DTSTART must be available
	lpicDTStartProp = icalcomponent_get_first_property(lpicEvent, ICAL_DTSTART_PROPERTY);
	if (!lpicDTStartProp) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	hr = HrAddTimeZone(lpicDTStartProp, lpIcalItem);
	if (hr != hrSuccess)
		goto exit;

	// get timezone of DTSTART
	timeDTStartUTC = ICalTimeTypeToUTC(lpicEventRoot, lpicDTStartProp);
	timeDTStartLocal = ICalTimeTypeToLocal(lpicDTStartProp);
	timeStartOffset = timeDTStartUTC - timeDTStartLocal;

	if (bIsAllday)
		UnixTimeToFileTime(timeDTStartLocal, &sPropVal.Value.ft);
	else
		UnixTimeToFileTime(timeDTStartUTC, &sPropVal.Value.ft);

	// Set 0x820D / ApptStartWhole
	sPropVal.ulPropTag = CHANGE_PROP_TYPE(m_lpNamedProps->aulPropTag[PROP_APPTSTARTWHOLE], PT_SYSTIME);
	lpIcalItem->lstMsgProps.push_back(sPropVal);

	// Set 0x8516 / CommonStart
	sPropVal.ulPropTag = CHANGE_PROP_TYPE(m_lpNamedProps->aulPropTag[PROP_COMMONSTART], PT_SYSTIME);
	lpIcalItem->lstMsgProps.push_back(sPropVal);

	// Set PR_START_DATE
	sPropVal.ulPropTag = PR_START_DATE;
	lpIcalItem->lstMsgProps.push_back(sPropVal);

	// Set 0x8215 / AllDayEvent
	sPropVal.ulPropTag = CHANGE_PROP_TYPE(m_lpNamedProps->aulPropTag[PROP_ALLDAYEVENT], PT_BOOLEAN);
	sPropVal.Value.b = bIsAllday;
	lpIcalItem->lstMsgProps.push_back(sPropVal);

	// Set endtime / DTEND
	lpicDTEndProp = icalcomponent_get_first_property(lpicEvent, ICAL_DTEND_PROPERTY);
	if (lpicDTEndProp) {
		timeDTEndUTC = ICalTimeTypeToUTC(lpicEventRoot, lpicDTEndProp);
		timeDTEndLocal = ICalTimeTypeToLocal(lpicDTEndProp);

	} else {
		// @note not so sure if the following comment is 100% true. It may also be used to complement a missing DTSTART or DTEND, according to MS specs.
		// When DTEND is not in the ical, it should be a recurring message, which never ends!

		// use duration for "end"
		lpicDurationProp = icalcomponent_get_first_property(lpicEvent, ICAL_DURATION_PROPERTY);
		if (!lpicDurationProp) {
			// and then we get here when it never ends??
			hr = MAPI_E_INVALID_PARAMETER;
			goto exit;
		}

		icaldurationtype dur = icalproperty_get_duration(lpicDurationProp);
		int sec = (dur.is_neg?-1:1)*(dur.weeks*7*24*60*60 + dur.days*24*60*60 + dur.hours*60*60 + dur.minutes*60 + dur.seconds);
		// why is negative compensated?!
		// switch start <-> end if negative?
		timeDTEndLocal = timeDTStartLocal + sec;
		timeDTEndUTC = timeDTStartUTC + sec;
	}
	timeEndOffset = timeDTEndUTC - timeDTEndLocal;

	if (bIsAllday)
		UnixTimeToFileTime(timeDTEndLocal, &sPropVal.Value.ft);
	else
		UnixTimeToFileTime(timeDTEndUTC, &sPropVal.Value.ft);

	sPropVal.ulPropTag = CHANGE_PROP_TYPE(m_lpNamedProps->aulPropTag[PROP_APPTENDWHOLE], PT_SYSTIME);
	lpIcalItem->lstMsgProps.push_back(sPropVal);
	
	// Set 0x8517 / CommonEnd
	sPropVal.ulPropTag = CHANGE_PROP_TYPE(m_lpNamedProps->aulPropTag[PROP_COMMONEND], PT_SYSTIME);
	lpIcalItem->lstMsgProps.push_back(sPropVal);

	// Set PR_END_DATE
	sPropVal.ulPropTag = PR_END_DATE;
	lpIcalItem->lstMsgProps.push_back(sPropVal);

	// Set duration
	sPropVal.ulPropTag = CHANGE_PROP_TYPE(m_lpNamedProps->aulPropTag[PROP_APPTDURATION], PT_LONG);
	sPropVal.Value.ul = timeDTEndUTC - timeDTStartUTC;
	/**
	 * @note This isn't what you think. The MAPI duration has a very
	 * complicated story, when timezone compensation should be applied
	 * and when not. This is a simplified version, which seems to work ... for now.
	 *
	 * See chapter 3.1.5.5 of [MS-OXOCAL].pdf for a more detailed story.
	 */
	if (!bIsAllday)
		sPropVal.Value.ul += (timeEndOffset - timeStartOffset);

	// Convert from seconds to minutes.
	sPropVal.Value.ul /= 60;
	lpIcalItem->lstMsgProps.push_back(sPropVal);

	goto exit;
// @todo add flags not to add these props ??
//       or maybe use a exclude filter in ICalToMAPI::GetItem()

	// Set submit time / DTSTAMP
	lpicProp = icalcomponent_get_first_property(lpicEvent, ICAL_DTSTAMP_PROPERTY);
	if (lpicProp)
		UnixTimeToFileTime(ICalTimeTypeToUTC(lpicEventRoot, lpicProp), &sPropVal.Value.ft);
	else
		UnixTimeToFileTime(time(NULL), &sPropVal.Value.ft);

	sPropVal.ulPropTag = PR_CLIENT_SUBMIT_TIME;
	lpIcalItem->lstMsgProps.push_back(sPropVal);

	// Set delivery time / DTSTAMP
	sPropVal.ulPropTag = PR_MESSAGE_DELIVERY_TIME;
	UnixTimeToFileTime(time(NULL), &sPropVal.Value.ft);
	lpIcalItem->lstMsgProps.push_back(sPropVal);

exit:
	return hr;
}

/** 
 * Create a new ical VEVENT component and set all ical properties in
 * the returned object.
 * 
 * @param[in]  lpMessage The message to convert
 * @param[out] lpicMethod The ical method of the top VCALENDAR object (hint, can differ when mixed methods are present in one VCALENDAR)
 * @param[out] lppicTZinfo ical timezone struct, describes all times used in this ical component
 * @param[out] lpstrTZid The name of the timezone
 * @param[out] lppEvent The ical calendar event
 * 
 * @return MAPI error code
 */
HRESULT VEventConverter::HrMAPI2ICal(LPMESSAGE lpMessage, icalproperty_method *lpicMethod, icaltimezone **lppicTZinfo, std::string *lpstrTZid, icalcomponent **lppEvent)
{
	HRESULT hr = hrSuccess;
	icalcomponent *lpEvent = NULL;

	lpEvent = icalcomponent_new(ICAL_VEVENT_COMPONENT);

	hr = VConverter::HrMAPI2ICal(lpMessage, lpicMethod, lppicTZinfo, lpstrTZid, lpEvent);
	if (hr != hrSuccess)
		goto exit;

	if (lppEvent)
		*lppEvent = lpEvent;
	lpEvent = NULL;

exit:
	if (lpEvent)
		icalcomponent_free(lpEvent);

	return hr;
}

/** 
 * Extends the VConverter version to set 'appt start whole' and 'appt end whole' named properties.
 * 
 * @param[in]  lpMsgProps All (required) properties from the message to convert time properties
 * @param[in]  ulMsgProps number of properties in lpMsgProps
 * @param[in]  lpicTZinfo ical timezone object to set times in
 * @param[in]  strTZid name of the given ical timezone
 * @param[in,out] lpEvent The Ical object to modify
 * 
 * @return MAPI error code.
 */
HRESULT VEventConverter::HrSetTimeProperties(LPSPropValue lpMsgProps, ULONG ulMsgProps, icaltimezone *lpicTZinfo, const std::string &strTZid, icalcomponent *lpEvent)
{
	HRESULT hr = hrSuccess;
	bool bIsAllDay = false;
	LPSPropValue lpPropVal = NULL;


	hr = VConverter::HrSetTimeProperties(lpMsgProps, ulMsgProps, lpicTZinfo, strTZid, lpEvent);
	if (hr != hrSuccess)
		goto exit;

	// vevent extra

	lpPropVal = PpropFindProp(lpMsgProps, ulMsgProps, CHANGE_PROP_TYPE(m_lpNamedProps->aulPropTag[PROP_ALLDAYEVENT], PT_BOOLEAN));
	if (lpPropVal)
		bIsAllDay = (lpPropVal->Value.b == TRUE);
	// @note If bIsAllDay == true, the item is an allday event "in the timezone it was created in" (and the user selected the allday event option)
	// In another timezone, Outlook will display the item as a 24h event with times (and the allday event option disabled)
	// However, in ICal, you cannot specify a timezone for a date, so ICal will show this as an allday event in every timezone your client is in.

	// Set start time / DTSTART
	lpPropVal = PpropFindProp(lpMsgProps, ulMsgProps, CHANGE_PROP_TYPE(m_lpNamedProps->aulPropTag[PROP_APPTSTARTWHOLE], PT_SYSTIME));
	if (lpPropVal != NULL) {
		time_t ttTime = FileTimeToUnixTime(lpPropVal->Value.ft.dwHighDateTime, lpPropVal->Value.ft.dwLowDateTime);

		hr = HrSetTimeProperty(ttTime, bIsAllDay, lpicTZinfo, strTZid, ICAL_DTSTART_PROPERTY, lpEvent);
		if (hr != hrSuccess)
			goto exit;
	}

	// Set end time / DTEND
	lpPropVal = PpropFindProp(lpMsgProps, ulMsgProps, CHANGE_PROP_TYPE(m_lpNamedProps->aulPropTag[PROP_APPTENDWHOLE], PT_SYSTIME));
	if (lpPropVal) {
		time_t ttTime = FileTimeToUnixTime(lpPropVal->Value.ft.dwHighDateTime, lpPropVal->Value.ft.dwLowDateTime);

		hr = HrSetTimeProperty(ttTime, bIsAllDay, lpicTZinfo, strTZid, ICAL_DTEND_PROPERTY, lpEvent);
		if (hr != hrSuccess)
			goto exit;
	}
	// @note we never set the DURATION property: MAPI objects always should have the end property 

exit:
	return hr;
}
