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
#include "vtimezone.h"
#include <mapidefs.h>
#include <mapicode.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

using namespace std;

/**
 * Converts icaltimetype to unix timestamp.
 * Here server zone refers to timezone with which the server started,
 * not the config file option in ical.cfg
 *
 * @param[in]	tt		icaltimetype
 * @return		unix timestamp
 */
time_t icaltime_as_timet_with_server_zone(const struct icaltimetype tt)
{
	struct tm stm;
	time_t t;

	/* If the time is the special null time, return 0. */
	if (icaltime_is_null_time(tt)) {
		return 0;
	}

	/* Copy the icaltimetype to a struct tm. */
	memset (&stm, 0, sizeof (struct tm));

	if (icaltime_is_date(tt)) {
		stm.tm_sec = stm.tm_min = stm.tm_hour = 0;
	} else {
		stm.tm_sec = tt.second;
		stm.tm_min = tt.minute;
		stm.tm_hour = tt.hour;
	}

	stm.tm_mday = tt.day;
	stm.tm_mon = tt.month-1;
	stm.tm_year = tt.year-1900;
	stm.tm_isdst = -1;

	t = mktime(&stm);

	return t;
}

/**
 * Converts icaltimetype to UTC unix timestamp
 *
 * @param[in]	lpicRoot		root icalcomponent to get timezone
 * @param[in]	lpicProp		icalproperty containing time
 * @return		UTC unix timestamp
 */
time_t ICalTimeTypeToUTC(icalcomponent *lpicRoot, icalproperty *lpicProp)
{
	time_t tRet = 0;
	icalparameter *lpicTZParam = NULL;
	const char *lpszTZID = NULL;
	icaltimezone *lpicTimeZone = NULL;

	lpicTZParam = icalproperty_get_first_parameter(lpicProp, ICAL_TZID_PARAMETER);
	if (lpicTZParam) {
		lpszTZID = icalparameter_get_tzid(lpicTZParam);
		lpicTimeZone = icalcomponent_get_timezone(lpicRoot, lpszTZID);
	}

	tRet = icaltime_as_timet_with_zone(icalvalue_get_datetime(icalproperty_get_value(lpicProp)), lpicTimeZone);

	return tRet;
}

/**
 * Converts icaltimetype to local unix timestamp.
 * Here local refers to timezone with which the server started, 
 * not the config file option in ical.cfg
 *
 * @param[in]	lpicProp	icalproperty containing time
 * @return		local unix timestamp
 */
time_t ICalTimeTypeToLocal(icalproperty *lpicProp)
{
	return icaltime_as_timet_with_server_zone(icalvalue_get_datetime(icalproperty_get_value(lpicProp)));
}

/**
 * Converts icaltimetype to tm structure
 *
 * @param[in]	tt		icaltimetype time
 * @return		tm structure
 */
struct tm UTC_ICalTime2UnixTime(icaltimetype tt)
{
	struct tm stm = {0};

	memset(&stm, 0, sizeof(struct tm));

	if (icaltime_is_null_time(tt))
		return stm;

	stm.tm_sec = tt.second;
	stm.tm_min = tt.minute;
	stm.tm_hour = tt.hour;
	stm.tm_mday = tt.day;
	stm.tm_mon = tt.month-1;
	stm.tm_year = tt.year-1900;
	stm.tm_isdst = -1;

	return stm;
}

/**
 * Converts icaltimetype to TIMEZONE_STRUCT structure
 *
 * @param[in]	kind				icalcomponent kind, either STD or DST time component (ICAL_XSTANDARD_COMPONENT, ICAL_XDAYLIGHT_COMPONENT)
 * @param[in]	lpVTZ				vtimezone icalcomponent
 * @param[out]	lpsTimeZone			returned TIMEZONE_STRUCT structure
 * @return		MAPI error code
 * @retval		MAPI_E_NOT_FOUND	icalcomponent kind not found in vtimezone component, or some part of timezone not found
 */
HRESULT HrZoneToStruct(icalcomponent_kind kind, icalcomponent* lpVTZ, TIMEZONE_STRUCT *lpsTimeZone)
{
	HRESULT hr = hrSuccess;
	icalcomponent *icComp = NULL;
	icalproperty *tzFrom, *tzTo, *rRule, *dtStart;
	icaltimetype icTime;
	SYSTEMTIME *lpSysTime = NULL;
	SYSTEMTIME stRecurTime;
	icalrecurrencetype recur;

	icComp = icalcomponent_get_first_component(lpVTZ, kind);
	if (!icComp) {
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}

	dtStart = icalcomponent_get_first_property(icComp, ICAL_DTSTART_PROPERTY);
	tzFrom = icalcomponent_get_first_property(icComp, ICAL_TZOFFSETFROM_PROPERTY);
	tzTo = icalcomponent_get_first_property(icComp, ICAL_TZOFFSETTO_PROPERTY);
	rRule = icalcomponent_get_first_property(icComp, ICAL_RRULE_PROPERTY);

	if (!tzFrom || !tzTo || !dtStart) {
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}

	icTime = icalcomponent_get_dtstart(icComp);
	icTime.is_utc = 1;

	if (kind == ICAL_XSTANDARD_COMPONENT) {
		// this is set when we request the STD timezone part.
		lpsTimeZone->lBias    = -(icalproperty_get_tzoffsetto(tzTo) / 60); // STD time is set as bias for timezone
		lpsTimeZone->lStdBias = 0;
		lpsTimeZone->lDstBias =  (icalproperty_get_tzoffsetto(tzTo) - icalproperty_get_tzoffsetfrom(tzFrom)) / 60; // DST bias == standard from

		lpsTimeZone->wStdYear = 0;
		lpSysTime = &lpsTimeZone->stStdDate;
	} else {
		lpsTimeZone->wDstYear = 0;
		lpSysTime = &lpsTimeZone->stDstDate;
	}

	memset(lpSysTime, 0, sizeof(SYSTEMTIME));

	// eg. japan doesn't have daylight saving switches.
	if (rRule) {
		recur = icalproperty_get_rrule(rRule);

		// can daylight saving really be !yearly ??
		if (recur.freq != ICAL_YEARLY_RECURRENCE ||	recur.by_month[0] == ICAL_RECURRENCE_ARRAY_MAX || recur.by_month[1] != ICAL_RECURRENCE_ARRAY_MAX)
			goto exit;

		stRecurTime = TMToSystemTime(UTC_ICalTime2UnixTime(icTime));
		lpSysTime->wHour = stRecurTime.wHour;
		lpSysTime->wMinute = stRecurTime.wMinute;

		lpSysTime->wMonth = recur.by_month[0];

		if (icalrecurrencetype_day_position(recur.by_day[0]) == -1)
			lpSysTime->wDay = 5;	// last day of month
		else
			lpSysTime->wDay = icalrecurrencetype_day_position(recur.by_day[0]); // 1..4

		lpSysTime->wDayOfWeek = icalrecurrencetype_day_day_of_week(recur.by_day[0]) -1;
	}

exit:
	return hr;
}

/**
 * Converts VTIMEZONE block in to TIMEZONE_STRUCT structure
 *
 * @param[in]	lpVTZ				VTIMEZONE icalcomponent
 * @param[out]	lpstrTZID			timezone string
 * @param[out]	lpTimeZone			returned TIMEZONE_STRUCT structure
 * @return		MAPI error code
 * @retval		MAPI_E_NOT_FOUND	standard component not found
 * @retval		MAPI_E_CALL_FAILED	TZID property not found
 */
HRESULT HrParseVTimeZone(icalcomponent* lpVTZ, std::string* lpstrTZID, TIMEZONE_STRUCT* lpTimeZone)
{
	HRESULT hr = hrSuccess;
	std::string strTZID;
	TIMEZONE_STRUCT tzRet;
	icalproperty *icProp = NULL;

	memset(&tzRet, 0, sizeof(TIMEZONE_STRUCT));

	icProp = icalcomponent_get_first_property(lpVTZ, ICAL_TZID_PROPERTY);
	if (!icProp) {
		hr = MAPI_E_CALL_FAILED;
		goto exit;
	}

	strTZID = icalproperty_get_tzid(icProp);
	if (strTZID.at(0) == '\"') {
		// strip "" around timezone name
		strTZID.erase(0, 1);
		strTZID.erase(strTZID.size()-1);
	}

	hr = HrZoneToStruct(ICAL_XSTANDARD_COMPONENT, lpVTZ, &tzRet);
	if (hr != hrSuccess)
		goto exit;

	// if the timezone does no switching, daylight is not given, so we ignore the error (which is only MAPI_E_NOT_FOUND)
	HrZoneToStruct(ICAL_XDAYLIGHT_COMPONENT, lpVTZ, &tzRet);

	if (lpstrTZID)
		*lpstrTZID = strTZID;
	if (lpTimeZone)
		*lpTimeZone = tzRet;

exit:
	return hr;
}

/**
 * Converts TIMEZONE_STRUCT structure to VTIMEZONE component
 *
 * @param[in]	strTZID		timezone string
 * @param[in]	tsTimeZone	TIMEZONE_STRUCT to be converted
 * @param[out]	lppVTZComp	returned VTIMEZONE component
 * @return		MAPI error code
 * @retval		MAPI_E_INVALID_PARAMETER timezone contains invalid data for a yearly daylightsaving
 */
HRESULT HrCreateVTimeZone(const std::string &strTZID, TIMEZONE_STRUCT &tsTimeZone, icalcomponent** lppVTZComp)
{
	HRESULT hr = hrSuccess;
	icalcomponent *icTZComp = NULL;
	icalcomponent *icComp = NULL;
	icaltimetype icTime;
	icalrecurrencetype icRec;

	// wDay in a timezone context means "week in month", 5 for last week in month
	if (tsTimeZone.stStdDate.wYear > 0 || tsTimeZone.stStdDate.wDay > 5 || tsTimeZone.stStdDate.wDayOfWeek > 7 ||
		tsTimeZone.stDstDate.wYear > 0 || tsTimeZone.stDstDate.wDay > 5 || tsTimeZone.stDstDate.wDayOfWeek > 7)
	{
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	// make a new timezone
	icTZComp = icalcomponent_new(ICAL_VTIMEZONE_COMPONENT);
	icalcomponent_add_property(icTZComp, icalproperty_new_tzid(strTZID.c_str()));

	// STD
	icComp = icalcomponent_new_xstandard();
	icTime = icaltime_from_timet(SystemTimeToUnixTime(tsTimeZone.stStdDate), 0);
	icalcomponent_add_property(icComp, icalproperty_new_dtstart(icTime));
	if (tsTimeZone.lStdBias == tsTimeZone.lDstBias || tsTimeZone.stStdDate.wMonth == 0 || tsTimeZone.stDstDate.wMonth == 0) {
		// std == dst
		icalcomponent_add_property(icComp, icalproperty_new_tzoffsetfrom(-tsTimeZone.lBias *60));
		icalcomponent_add_property(icComp, icalproperty_new_tzoffsetto(-tsTimeZone.lBias *60));
	} else {
		icalcomponent_add_property(icComp, icalproperty_new_tzoffsetfrom( ((-tsTimeZone.lBias) + (-tsTimeZone.lDstBias)) *60) );
		icalcomponent_add_property(icComp, icalproperty_new_tzoffsetto(-tsTimeZone.lBias *60));

		// create rrule for STD zone
		icalrecurrencetype_clear(&icRec);
		icRec.freq = ICAL_YEARLY_RECURRENCE;
		icRec.interval = 1;

		icRec.by_month[0] = tsTimeZone.stStdDate.wMonth;
		icRec.by_month[1] = ICAL_RECURRENCE_ARRAY_MAX;

		icRec.week_start = ICAL_SUNDAY_WEEKDAY;

		// by_day[0] % 8 = weekday, by_day[0]/8 = Nth week, 0 is 'any', and -1 = last
		icRec.by_day[0] = tsTimeZone.stStdDate.wDay == 5 ? -1*(8+tsTimeZone.stStdDate.wDayOfWeek+1) : (tsTimeZone.stStdDate.wDay)*8+tsTimeZone.stStdDate.wDayOfWeek+1;
		icRec.by_day[1] = ICAL_RECURRENCE_ARRAY_MAX;
		
		icalcomponent_add_property(icComp, icalproperty_new_rrule(icRec));
	}
	icalcomponent_add_component(icTZComp, icComp);

	// DST, optional
	if (tsTimeZone.lStdBias != tsTimeZone.lDstBias && tsTimeZone.stStdDate.wMonth != 0 && tsTimeZone.stDstDate.wMonth != 0) {
		icComp = icalcomponent_new_xdaylight();
		icTime = icaltime_from_timet(SystemTimeToUnixTime(tsTimeZone.stDstDate), 0);
		icalcomponent_add_property(icComp, icalproperty_new_dtstart(icTime));

		icalcomponent_add_property(icComp, icalproperty_new_tzoffsetfrom(-tsTimeZone.lBias *60));
		icalcomponent_add_property(icComp, icalproperty_new_tzoffsetto( ((-tsTimeZone.lBias) + (-tsTimeZone.lDstBias)) *60) );

		// create rrule for DST zone
		icalrecurrencetype_clear(&icRec);
		icRec.freq = ICAL_YEARLY_RECURRENCE;
		icRec.interval = 1;

		icRec.by_month[0] = tsTimeZone.stDstDate.wMonth;
		icRec.by_month[1] = ICAL_RECURRENCE_ARRAY_MAX;

		icRec.week_start = ICAL_SUNDAY_WEEKDAY;

		icRec.by_day[0] = tsTimeZone.stDstDate.wDay == 5 ? -1*(8+tsTimeZone.stDstDate.wDayOfWeek+1) : (tsTimeZone.stDstDate.wDay)*8+tsTimeZone.stDstDate.wDayOfWeek+1;
		icRec.by_day[1] = ICAL_RECURRENCE_ARRAY_MAX;
		
		icalcomponent_add_property(icComp, icalproperty_new_rrule(icRec));

		icalcomponent_add_component(icTZComp, icComp);
	}

	*lppVTZComp = icTZComp;

exit:
	return hr;
}

/**
 * Returns TIMEZONE_STRUCT structure from the string Olson city name.
 * Function searches zoneinfo data in linux
 *
 * @param[in]	strTimezone					timezone string
 * @param[out]	ttTimeZone					TIMEZONE_STRUCT of the cityname
 * @return		MAPI error code
 * @retval		MAPI_E_INVALID_PARAMETER	strTimezone is empty
 * @retval		MAPI_E_NOT_FOUND			cannot find the timezone string in zoneinfo
 */
HRESULT HrGetTzStruct(const std::string &strTimezone, TIMEZONE_STRUCT *ttTimeZone)
{
	HRESULT hr = hrSuccess;
	icaltimezone *lpicTimeZone = NULL;
	icalcomponent *lpicComponent = NULL;
	

	if (strTimezone.empty())
	{
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	lpicTimeZone = icaltimezone_get_builtin_timezone(strTimezone.c_str());
	if (!lpicTimeZone) {
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}

	lpicComponent = icaltimezone_get_component(lpicTimeZone);
	if (!lpicComponent) {
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}

	hr = HrParseVTimeZone(lpicComponent, NULL, ttTimeZone);
	if (hr != hrSuccess)
		goto exit;
	
exit:
	return hr;
}
