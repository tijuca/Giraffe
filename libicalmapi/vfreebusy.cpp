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
#include "vfreebusy.h"
#include <mapiutil.h>
#include "mapiext.h"
#include "nameids.h"

using namespace std;

/** 
 * Converts a VFREEBUSY object to seperate parts
 * 
 * @param[in]  lpFbcomp The ical component containing the VFREEBUSY
 * @param[out] lptStart Returns the DTSTART property
 * @param[out] lptEnd Returns the DTEND property
 * @param[out] lpstrUID Returns the UID property
 * @param[out] lstUsers Appends any email addresses of ATTENDEEs listed
 * 
 * @return Always hrSuccess
 */
HRESULT HrGetFbInfo(icalcomponent *lpFbcomp, time_t *lptStart, time_t *lptEnd, std::string *lpstrUID, std::list<std::string> *lstUsers)
{
	HRESULT hr = hrSuccess;
	icalproperty *lpicProp = NULL;
	std::string strEmail;

	// DTSTART
	lpicProp = icalcomponent_get_first_property(lpFbcomp, ICAL_DTSTART_PROPERTY);
	if (lpicProp)
		*lptStart = icaltime_as_timet (icalproperty_get_dtstart (lpicProp));

	// DTEND
	lpicProp = icalcomponent_get_first_property(lpFbcomp, ICAL_DTEND_PROPERTY);
	if (lpicProp)
		*lptEnd = icaltime_as_timet (icalproperty_get_dtend (lpicProp));

	// UID
	lpicProp = icalcomponent_get_first_property(lpFbcomp, ICAL_UID_PROPERTY);
	if (lpicProp)
		*lpstrUID = icalproperty_get_uid(lpicProp);

	// ATTENDEE
	lpicProp = icalcomponent_get_first_property(lpFbcomp, ICAL_ATTENDEE_PROPERTY);
	while (lpicProp) {
		strEmail = icalproperty_get_attendee(lpicProp);
		if (strnicmp(strEmail.c_str(), "mailto:", 7) == 0) {
			strEmail.erase(0, 7);
		}
		lstUsers->push_back(strEmail);
		lpicProp = icalcomponent_get_next_property(lpFbcomp, ICAL_ATTENDEE_PROPERTY);
	}

	return hr;
}

/** 
 * Converts a MAPI freebusy block to a VFREEBUSY ical component.
 * 
 * @param[in]  lpsFbblk MAPI freebusy info blocks
 * @param[in]  ulBlocks Number of blocks in lpsFbblk
 * @param[in]  tDtStart Unix timestamp with the start date
 * @param[in]  tDtEnd Unix timestamp with the end date
 * @param[in]  strOrganiser The email address of the organiser, @todo make const ref
 * @param[in]  strUser The email address of an attendee, @todo make list? make const ref
 * @param[in]  strUID UID of the freebusy data
 * @param[out] lpicFbComponent new VFREEBUSY ical component
 * 
 * @return MAPI error code
 */
HRESULT HrFbBlock2ICal(FBBlock_1 *lpsFbblk, LONG ulBlocks, time_t tDtStart, time_t tDtEnd, const std::string &strOrganiser, const std::string &strUser, const std::string &strUID, icalcomponent **lpicFbComponent)
{
	HRESULT hr = hrSuccess;
	icalcomponent *lpFbComp = NULL;
	icaltimetype ittStamp;
	icalproperty *lpicProp = NULL;
	icalperiodtype icalPeriod;
	icalparameter *icalParam = NULL;
	time_t tStart = 0;
	time_t tEnd = 0;
	std::string strEmail;


	lpFbComp = icalcomponent_new(ICAL_VFREEBUSY_COMPONENT);
	if (!lpFbComp) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}
	
	//DTSTART
	ittStamp = icaltime_from_timet_with_zone(tDtStart, false, icaltimezone_get_utc_timezone());	
	lpicProp = icalproperty_new(ICAL_DTSTART_PROPERTY);
	if (!lpicProp) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}
	icalproperty_set_value(lpicProp, icalvalue_new_datetime(ittStamp));
	icalcomponent_add_property(lpFbComp, lpicProp);

	//DTEND
	ittStamp = icaltime_from_timet_with_zone(tDtEnd, false, icaltimezone_get_utc_timezone());	
	lpicProp = icalproperty_new(ICAL_DTEND_PROPERTY);
	if (!lpicProp) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}	
	icalproperty_set_value(lpicProp, icalvalue_new_datetime(ittStamp));
	icalcomponent_add_property(lpFbComp, lpicProp);

	//DTSTAMP
	ittStamp = icaltime_from_timet_with_zone(time(NULL), false, icaltimezone_get_utc_timezone());	
	lpicProp = icalproperty_new(ICAL_DTSTAMP_PROPERTY);
	if (!lpicProp) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}	
	icalproperty_set_value(lpicProp, icalvalue_new_datetime(ittStamp));
	icalcomponent_add_property(lpFbComp, lpicProp);
	
	//UID
	lpicProp = icalproperty_new(ICAL_UID_PROPERTY);
	if (!lpicProp) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}
	icalproperty_set_uid(lpicProp, strUID.c_str());
	icalcomponent_add_property(lpFbComp, lpicProp);

	//ORGANIZER
	lpicProp = icalproperty_new(ICAL_ORGANIZER_PROPERTY);
	if (!lpicProp) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}	
	icalproperty_set_organizer(lpicProp, strOrganiser.c_str());
	icalcomponent_add_property(lpFbComp, lpicProp);
	
	//ATTENDEE
	strEmail = "mailto:" + strUser;
	lpicProp = icalproperty_new_attendee(strEmail.c_str());
	if (!lpicProp) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}
	
	// param PARTSTAT
	icalParam = icalparameter_new_partstat(ICAL_PARTSTAT_ACCEPTED);
	icalproperty_add_parameter(lpicProp, icalParam);
	
	// param CUTYPE
	icalParam = icalparameter_new_cutype(ICAL_CUTYPE_INDIVIDUAL);
	icalproperty_add_parameter(lpicProp, icalParam);

	icalcomponent_add_property(lpFbComp, lpicProp);

	// add all freebusy blocks
	for(int i = 0; i < ulBlocks ; i++) {
		
		// FREEBUSY
		lpicProp = icalproperty_new(ICAL_FREEBUSY_PROPERTY);
		if (!lpicProp) {
			hr = MAPI_E_INVALID_PARAMETER;
			goto exit;
		}

		RTimeToUnixTime(lpsFbblk[i].m_tmStart, &tStart);
		RTimeToUnixTime(lpsFbblk[i].m_tmEnd, &tEnd);
		icalPeriod.start = icaltime_from_timet_with_zone(tStart, false, icaltimezone_get_utc_timezone());
		icalPeriod.end = icaltime_from_timet_with_zone(tEnd, false, icaltimezone_get_utc_timezone());
	
		icalproperty_set_freebusy(lpicProp, icalPeriod);

		switch (lpsFbblk[i].m_fbstatus)
		{	
		case fbBusy:
			icalParam = icalparameter_new_fbtype (ICAL_FBTYPE_BUSY);
			break;
		case fbTentative:
			icalParam = icalparameter_new_fbtype (ICAL_FBTYPE_BUSYTENTATIVE);
			break;
		case fbOutOfOffice:
			icalParam = icalparameter_new_fbtype (ICAL_FBTYPE_BUSYUNAVAILABLE);
			break;
		default:
			icalParam = icalparameter_new_fbtype (ICAL_FBTYPE_FREE);
			break;
		}
		
		icalproperty_add_parameter (lpicProp, icalParam);
		icalcomponent_add_property(lpFbComp,lpicProp);
	}
	
	icalcomponent_end_component(lpFbComp, ICAL_VFREEBUSY_COMPONENT);
	*lpicFbComponent = lpFbComp;

exit:

	return hr;
}
