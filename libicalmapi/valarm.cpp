/*
 * Copyright 2005 - 2015  Zarafa B.V. and its licensors
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License, version 3,
 * as published by the Free Software Foundation with the following
 * additional terms according to sec. 7:
 * 
 * "Zarafa" is a registered trademark of Zarafa B.V.
 * The licensing of the Program under the AGPL does not imply a trademark 
 * license. Therefore any rights, title and interest in our trademarks 
 * remain entirely with us.
 * 
 * Our trademark policy (see TRADEMARKS.txt) allows you to use our trademarks
 * in connection with Propagation and certain other acts regarding the Program.
 * In any case, if you propagate an unmodified version of the Program you are
 * allowed to use the term "Zarafa" to indicate that you distribute the Program.
 * Furthermore you may use our trademarks where it is necessary to indicate the
 * intended purpose of a product or service provided you use it in accordance
 * with honest business practices. For questions please contact Zarafa at
 * trademark@zarafa.com.
 *
 * The interactive user interface of the software displays an attribution 
 * notice containing the term "Zarafa" and/or the logo of Zarafa. 
 * Interactive user interfaces of unmodified and modified versions must 
 * display Appropriate Legal Notices according to sec. 5 of the GNU Affero 
 * General Public License, version 3, when you propagate unmodified or 
 * modified versions of the Program. In accordance with sec. 7 b) of the GNU 
 * Affero General Public License, version 3, these Appropriate Legal Notices 
 * must retain the logo of Zarafa or display the words "Initial Development 
 * by Zarafa" if the display of the logo is not reasonably feasible for
 * technical reasons.
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

#include <mapi.h>
#include <mapidefs.h>
#include <mapix.h>
#include <mapiutil.h>
#include <mapiext.h>
#include <mapiguidext.h>

#include "valarm.h"
#include "recurrence.h"
#include "namedprops.h"
#include "CommonUtil.h"
#include "Util.h"
#include "stringutil.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static const char THIS_FILE[] = __FILE__;
#endif

/**
 * Generates ical VALARM component from reminderbefore minutes.
 * Mapi -> Ical conversion
 *
 * @param[in]	lRemindBefore				reminder before minutes.
 * @param[in]	ttReminderTime				Reminder time in UTC format. 
 * @param[in]	bTask						If true, the output value is for a task item
 * @param[out]	lppAlarm					ical VALARM component.
 * @return		MAPI error code
 * @retval		MAPI_E_INVALID_PARAMETER	invalid parameters
 */
HRESULT HrParseReminder(LONG lRemindBefore, time_t ttReminderTime, bool bTask, icalcomponent **lppAlarm)
{
	HRESULT hr = hrSuccess;
	icalcomponent *lpVAlarm = NULL;
	icaltriggertype sittTrigger;

	if (lppAlarm == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	if (lRemindBefore == 1525252321) // OL sets this value for default 15 mins time.
		lRemindBefore = 15;

	memset((void *) &sittTrigger, 0, sizeof(icaltriggertype));

	if (ttReminderTime && bTask) {
		sittTrigger.time = icaltime_from_timet(ttReminderTime, false);			// given in UTC
		sittTrigger.time.is_utc = 1;
	} else
		sittTrigger.duration = icaldurationtype_from_int(-1 * lRemindBefore * 60);	// set seconds

	lpVAlarm = icalcomponent_new_valarm();
	icalcomponent_add_property(lpVAlarm, icalproperty_new_trigger(sittTrigger));
	icalcomponent_add_property(lpVAlarm, icalproperty_new_action(ICAL_ACTION_DISPLAY));
	icalcomponent_add_property(lpVAlarm, icalproperty_new_description("Reminder"));

	*lppAlarm = lpVAlarm;
	lpVAlarm = NULL;

exit:
	if (lpVAlarm)
		icalcomponent_free(lpVAlarm);

	return hr;
}

/**
 * Gets reminder info from the ical VTIMEZONE component.
 * ICAL -> MAPI conversion
 *
 * @param[in]	lpicAlarm			ical VTIMEZONE component
 * @param[out]	lplRemindBefore		reminder before minutes
 * @param[out]	lpttReminderTime	timestamp reminder trigger time in UTC
 * @param[out]	lpbReminderSet		boolean to specify if reminder is set
 * @return		MAPI error code
 */
HRESULT HrParseVAlarm(icalcomponent *lpicAlarm, LONG *lplRemindBefore, time_t *lpttReminderTime, bool *lpbReminderSet) {
	HRESULT hr = hrSuccess;
	icalproperty *lpTrigger = NULL;
	icalproperty *lpAction = NULL;
	icaltriggertype sittTrigger;
	icalproperty_action eipaAction;
	std::string strValue;

	LONG lRemindBefore = 0;
	time_t ttReminderTime = 0;
	bool bReminderSet = false;


	lpTrigger = icalcomponent_get_first_property(lpicAlarm, ICAL_TRIGGER_PROPERTY);
	lpAction = icalcomponent_get_first_property(lpicAlarm, ICAL_ACTION_PROPERTY);

	if (lpTrigger != NULL) {
		sittTrigger = icalproperty_get_trigger(lpTrigger);

		ttReminderTime = icaltime_as_timet(sittTrigger.time); // is in utc

		lRemindBefore = -1 * (icaldurationtype_as_int(sittTrigger.duration) / 60);

		// In iCal an remind before can be both negative (meaning alarm BEFORE startdate) or positive (meaning
		// alarm AFTER startdate). In MAPI an remind before can only be positive (meaning alarm BEFORE startdate).
		// If (after inverting iCal remind before so it's compatible with MAPI) remind before is negative we need
		// to set it to 0.
		if (lRemindBefore < 0)
			lRemindBefore = 0;
	}

	if (lpAction != NULL) {
		eipaAction = icalproperty_get_action(lpAction);

		// iMac Calendar 6.0 sends ACTION:NONE, which libical doesn't parse correcty to the ICAL_ACTION_NONE enum value
		if (eipaAction > ICAL_ACTION_X && eipaAction < ICAL_ACTION_NONE)
			bReminderSet = true;
	}

	if (lplRemindBefore)
		*lplRemindBefore = lRemindBefore;

	if (lpttReminderTime)
		*lpttReminderTime = ttReminderTime;

	if (lpbReminderSet)
		*lpbReminderSet = bReminderSet;

	return hr;
}
