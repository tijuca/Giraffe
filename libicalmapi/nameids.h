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

#ifndef NAMEIDS_H
#define NAMEIDS_H

#include "mapidefs.h"
#include "namedprops.h"
#include "mapiguidext.h"
#include <mapiguid.h>
#include "icalmapi.h"

/* MAPINAMEID strings are in UCS-2LE, so cannot be static initialized in linux
   We first define all strings, so we can use the nmStringNames[] lookup array
 */
enum ICALMAPI_API eIDNamedProps {
	PROP_KEYWORDS = 0,
// End of String names
	PROP_MEETINGLOCATION, PROP_GOID, PROP_ISRECURRING, PROP_CLEANID, PROP_OWNERCRITICALCHANGE, PROP_ATTENDEECRITICALCHANGE, PROP_OLDSTART, PROP_ISEXCEPTION, PROP_RECURSTARTTIME,
	PROP_RECURENDTIME, PROP_MOZGEN, PROP_MOZLASTACK, PROP_MOZ_SNOOZE_SUFFIX, PROP_MOZSENDINVITE , PROP_APPTTSREF, PROP_FLDID, PROP_SENDASICAL, PROP_APPTSEQNR, PROP_APPTSEQTIME,
	PROP_BUSYSTATUS, PROP_APPTAUXFLAGS, PROP_LOCATION, PROP_LABEL, PROP_APPTSTARTWHOLE, PROP_APPTENDWHOLE, PROP_APPTDURATION,
	PROP_ALLDAYEVENT, PROP_RECURRENCESTATE, PROP_MEETINGSTATUS, PROP_RESPONSESTATUS, PROP_RECURRING, PROP_INTENDEDBUSYSTATUS,
	PROP_RECURRINGBASE, PROP_REQUESTSENT, PROP_8230, PROP_RECURRENCETYPE, PROP_RECURRENCEPATTERN, PROP_TIMEZONEDATA, PROP_TIMEZONE,
	PROP_RECURRENCE_START, PROP_RECURRENCE_END, PROP_ALLATTENDEESSTRING, PROP_TOATTENDEESSTRING, PROP_CCATTENDEESSTRING, PROP_NETMEETINGTYPE,
	PROP_NETMEETINGSERVER, PROP_NETMEETINGORGANIZERALIAS, PROP_NETMEETINGAUTOSTART, PROP_AUTOSTARTWHEN, PROP_CONFERENCESERVERALLOWEXTERNAL, PROP_NETMEETINGDOCPATHNAME,
	PROP_NETSHOWURL, PROP_CONVERENCESERVERPASSWORD, PROP_APPTREPLYTIME, PROP_REMINDERMINUTESBEFORESTART, PROP_REMINDERTIME, PROP_REMINDERSET, PROP_PRIVATE,
	PROP_NOAGING, PROP_SIDEEFFECT, PROP_REMOTESTATUS, PROP_COMMONSTART, PROP_COMMONEND, PROP_COMMONASSIGN,
	PROP_CONTACTS, PROP_OUTLOOKINTERNALVERSION, PROP_OUTLOOKVERSION, PROP_REMINDERNEXTTIME, PROP_HIDE_ATTACH,
	PROP_TASK_STATUS, PROP_TASK_COMPLETE, PROP_TASK_PERCENTCOMPLETE, PROP_TASK_STARTDATE, PROP_TASK_DUEDATE,
	PROP_TASK_RECURRSTATE, PROP_TASK_ISRECURRING, PROP_TASK_COMPLETED_DATE,
// End if ID names
	SIZE_NAMEDPROPS
};

/* call this function to get the id's from the listed namedprops above */
HRESULT ICALMAPI_API HrLookupNames(IMAPIProp *lpPropObj, LPSPropTagArray *lppNamedProps);

#endif
