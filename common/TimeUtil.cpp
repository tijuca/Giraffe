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

#include "TimeUtil.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/**
 * Get a timestamp for the given date/time point
 *
 * Gets a timestamp for 'Nth [weekday] in month X, year Y at HH:00:00 GMT'
 *
 * @param year Full year (eg 2008, 2010)
 * @param month Month 1-12
 * @param week Week 1-5 (1 == first, 2 == second, 5 == last)
 * @param day Day 0-6 (0 == sunday, 1 == monday, ...)
 * @param hour Hour 0-23 (0 == 00:00, 1 == 01:00, ...)
 */
time_t getDateByYearMonthWeekDayHour(WORD year, WORD month, WORD week, WORD day, WORD hour)
{
	struct tm tm = {0};
	time_t date;

	// get first day of month
	tm.tm_year = year;
	tm.tm_mon = month-1;
	tm.tm_mday = 1;
	date = timegm(&tm);

	// convert back to struct to get wday info
	gmtime_safe(&date, &tm);

	date -= tm.tm_wday * 24 * 60 * 60; // back up to start of week
	date += week * 7 * 24 * 60 * 60;   // go to correct week nr
	date += day * 24 * 60 * 60;
	date += hour * 60 * 60;

	// if we are in the next month, then back up a week, because week '5' means
	// 'last week of month'
	gmtime_safe(&date, &tm);

	if (tm.tm_mon != month-1)
		date -= 7 * 24 * 60 * 60;

	return date;
}

LONG getTZOffset(time_t date, TIMEZONE_STRUCT sTimeZone)
{
	struct tm tm;
	time_t dststart, dstend;
	WORD endyear;

	if (sTimeZone.lStdBias == sTimeZone.lDstBias || sTimeZone.stDstDate.wMonth == 0 || sTimeZone.stStdDate.wMonth == 0)
		return -(sTimeZone.lBias) * 60;

	gmtime_safe(&date, &tm);

	dststart = getDateByYearMonthWeekDayHour(tm.tm_year, sTimeZone.stDstDate.wMonth, sTimeZone.stDstDate.wDay, sTimeZone.stDstDate.wDayOfWeek, sTimeZone.stDstDate.wHour);
	// end year is one further when start > end (e.g. on the southern hemisphere)
	endyear = sTimeZone.stDstDate.wMonth > sTimeZone.stStdDate.wMonth ? 1 : 0;
	dstend = getDateByYearMonthWeekDayHour(tm.tm_year + endyear, sTimeZone.stStdDate.wMonth, sTimeZone.stStdDate.wDay, sTimeZone.stStdDate.wDayOfWeek, sTimeZone.stStdDate.wHour);

	if (date > dststart && date < dstend)
		return -(sTimeZone.lBias + sTimeZone.lDstBias) * 60;
	return -(sTimeZone.lBias + sTimeZone.lStdBias) * 60;
}

time_t UTCToLocal(time_t utc, TIMEZONE_STRUCT sTimeZone)
{
	LONG offset;

	offset = getTZOffset(utc, sTimeZone);

	return utc + offset;
}

time_t LocalToUTC(time_t local, TIMEZONE_STRUCT sTimeZone)
{
	LONG offset;

	offset = getTZOffset(local, sTimeZone);

	return local - offset;
}