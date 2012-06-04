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

#ifndef RECURRENCE_H
#define RECURRENCE_H

#include "RecurrenceState.h"
#include "mapidefs.h"
#include "mapix.h"
#include "ECLogger.h"
#include "Util.h"
#include <list>
#include "TimeUtil.h"
#include "freebusy.h"
#include "freebusyutil.h"

class recurrence {
public:
	recurrence();
	~recurrence();

	HRESULT HrLoadRecurrenceState(char *lpData, unsigned int ulLen, ULONG ulFlags);
	HRESULT HrGetRecurrenceState(char **lppData, unsigned int *lpulLen, void *base = NULL);

	HRESULT HrGetHumanReadableString(std::string *lpstrHRS);

	HRESULT HrGetItems(time_t tsStart, time_t tsEnd, ECLogger *lpLogger, TIMEZONE_STRUCT ttZinfo, ULONG ulBusyStatus, OccrInfo **lppFbBlock, ULONG *lpcValues);

	typedef enum freq_type {DAILY, WEEKLY, MONTHLY, YEARLY} freq_type;
	typedef enum term_type {DATE, NUMBER, NEVER} term_type;


	freq_type getFrequency();
	HRESULT setFrequency(freq_type ft);

	time_t getStartDate();
	HRESULT setStartDate(time_t tStart);

	time_t getEndDate();
	HRESULT setEndDate(time_t tEnd);

	ULONG getStartTimeOffset();
	HRESULT setStartTimeOffset(ULONG ulMinutesSinceMidnight);

	ULONG getEndTimeOffset();
	HRESULT setEndTimeOffset(ULONG ulMinutesSinceMidnight);

	time_t getStartDateTime();
	HRESULT setStartDateTime(time_t tStart);
	time_t getEndDateTime();
	HRESULT setEndDateTime(time_t tStart);

	ULONG getCount();
	HRESULT setCount(ULONG ulCount);

	term_type getEndType();
	HRESULT setEndType(term_type);

	ULONG getInterval();
	HRESULT setInterval(ULONG);

	ULONG getSlidingFlag();
	HRESULT setSlidingFlag(ULONG);

	ULONG getFirstDOW();
	HRESULT setFirstDOW(ULONG);

	UCHAR getWeekDays();
	HRESULT setWeekDays(UCHAR);

	UCHAR getDayOfMonth();
	HRESULT setDayOfMonth(UCHAR);

	UCHAR getMonth();
 	HRESULT setMonth(UCHAR);

	UCHAR getWeekNumber();		/* 1..4 and 5 (last) */
	HRESULT setWeekNumber(UCHAR);

	/* exception handling */

	HRESULT addDeletedException(time_t);
	std::list<time_t> getDeletedExceptions();

	ULONG getModifiedCount();
	ULONG getModifiedFlags(ULONG id); /* 0..getModifiedCount() */
	time_t getModifiedStartDateTime(ULONG id);
	time_t getModifiedEndDateTime(ULONG id);
	time_t getModifiedOriginalDateTime(ULONG id); /* used as recurrence-id */
	std::wstring getModifiedSubject(ULONG id);
	ULONG getModifiedMeetingType(ULONG id);
	LONG getModifiedReminderDelta(ULONG id);
	ULONG getModifiedReminder(ULONG id);
	std::wstring getModifiedLocation(ULONG id);
	ULONG getModifiedBusyStatus(ULONG id);
	ULONG getModifiedAttachment(ULONG id);
	ULONG getModifiedSubType(ULONG id);

	HRESULT addModifiedException(time_t tStart, time_t tEnd, time_t tOriginalStart, ULONG *id);
	HRESULT setModifiedSubject(ULONG id, std::wstring strSubject);
	HRESULT setModifiedMeetingType(ULONG id, ULONG type);
	HRESULT setModifiedReminderDelta(ULONG id, LONG delta);
	HRESULT setModifiedReminder(ULONG id, ULONG set);
	HRESULT setModifiedLocation(ULONG id, std::wstring strLocation);
	HRESULT setModifiedBusyStatus(ULONG id, ULONG status);
	HRESULT setModifiedAttachment(ULONG id);
	HRESULT setModifiedSubType(ULONG id, ULONG subtype);
	HRESULT setModifiedApptColor(ULONG id, ULONG color);
	HRESULT setModifiedBody(ULONG id);
	
	HRESULT AddValidOccr(time_t tsOccrStart, time_t tsOccrEnd, ULONG ulBusyStatus, OccrInfo **lpFBBlocksAll, ULONG *lpcValues);
	bool isOccurrenceValid(time_t tsPeriodStart, time_t tsPeriodEnd, time_t tsNewOcc);
	bool isDeletedOccurrence(time_t ttOccDate);
	bool isException(time_t tsOccDate);

	ULONG countDaysOfMonth(time_t tsDate);
	ULONG DaysTillMonth(time_t tsDate, ULONG ulMonth);
	std::list<time_t> getModifiedOccurrences();

	/* TODO: */
/*
	HRESULT setDeletedOccurrence(time_t);
	HRESULT removeDeletedOccurrence(time_t);
	std::list<time_t> getDeletedOccurrences();

	HRESULT getChangedOccurrence(time_t, RecurrenceState::Exception *);
	HRESULT setChangedOccurrence(RecurrenceState::Exception);
	HRESULT removeChangedOccurrence(time_t);
	std::list<RecurrenceState::Exception> getChangedOccurrences();

	std::list<time_t> getExceptions();

	bool isOccurrence(time_t);
	bool isRuleOccurrence(time_t);

	bool isAfter(time_t);
*/
	time_t calcStartDate();
	time_t calcEndDate();
	ULONG calcCount();

	static time_t MonthInSeconds(ULONG year, ULONG month);
	static time_t MonthsInSeconds(ULONG months);

	static time_t Minutes2Time(ULONG);
	static ULONG Time2Minutes(time_t);
	static ULONG Minutes2Month(ULONG);
	static time_t StartOfDay(time_t);
	static time_t StartOfWeek(time_t);
	static time_t StartOfMonth(time_t);
	static time_t StartOfYear(time_t);


	static bool isLeapYear(ULONG year);

	static ULONG DaysInMonth(ULONG);
	static ULONG DaysInMonth(ULONG, ULONG);
	static ULONG DaysInYear(ULONG);
	static ULONG MonthFromTime(time_t);
	static ULONG YearFromTime(time_t);
	static ULONG AllMonthsFromTime(time_t);
	static ULONG WeekDayFromTime(time_t);
	static ULONG MonthDayFromTime(time_t);

private:
	RecurrenceState m_sRecState;
	unsigned int m_ulMonth;       // yearly, 1...12 (Not stored in the struct)

	std::vector<std::wstring> vExceptionsSubject;
	std::vector<std::wstring> vExceptionsLocation;

/*
	std::list<time_t> exceptions;
	std::list<time_t> deleted_occurrences;	
	std::list<RecurrenceState::Exception> changed_occurrences;
*/

	ULONG calcBits(ULONG x);
};

#endif
