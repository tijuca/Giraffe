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

#include "TimeUtil.h"

namespace KC {

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
	bool dst = false;

	if (sTimeZone.lStdBias == sTimeZone.lDstBias || sTimeZone.stDstDate.wMonth == 0 || sTimeZone.stStdDate.wMonth == 0)
		return -(sTimeZone.lBias) * 60;

	gmtime_safe(&date, &tm);

	dststart = getDateByYearMonthWeekDayHour(tm.tm_year, sTimeZone.stDstDate.wMonth, sTimeZone.stDstDate.wDay, sTimeZone.stDstDate.wDayOfWeek, sTimeZone.stDstDate.wHour);
	dstend = getDateByYearMonthWeekDayHour(tm.tm_year, sTimeZone.stStdDate.wMonth, sTimeZone.stStdDate.wDay, sTimeZone.stStdDate.wDayOfWeek, sTimeZone.stStdDate.wHour);

	if (dststart <= dstend) {
		// Northern hemisphere, eg DST is during Mar-Oct
		if (date > dststart && date < dstend)
			dst = true;
	} else {
		// Southern hemisphere, eg DST is during Oct-Mar
		if (date < dstend || date > dststart)
			dst = true;
	}
	if (dst)
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

} /* namespace */
