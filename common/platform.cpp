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


#include <mapidefs.h>
#include <mapicode.h>
#include <limits.h>
#include <pthread.h>

#include <sys/stat.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

HRESULT UnixTimeToFileTime(time_t t, FILETIME *ft)
{
    __int64 l;

    l = (__int64)t * 10000000 + NANOSECS_BETWEEN_EPOCHS;
    ft->dwLowDateTime = (unsigned int)l;
    ft->dwHighDateTime = (unsigned int)(l >> 32);

	return hrSuccess;
}

HRESULT FileTimeToUnixTime(FILETIME &ft, time_t *t)
{
	__int64 l;

	l = ((__int64)ft.dwHighDateTime << 32) + ft.dwLowDateTime;
	l -= NANOSECS_BETWEEN_EPOCHS;
	l /= 10000000;
	
	if(sizeof(time_t) < 8) {
		// On 32-bit systems, we cap the values at MAXINT and MININT
		if(l < (__int64)INT_MIN) {
			l = INT_MIN;
		}
		if(l > (__int64)INT_MAX) {
			l = INT_MAX;
		}
	}

	*t = (time_t)l;

	return hrSuccess;
}

void UnixTimeToFileTime(time_t t, int *hi, unsigned int *lo)
{
	__int64 ll;

	ll = (__int64)t * 10000000 + NANOSECS_BETWEEN_EPOCHS;
	*lo = (unsigned int)ll;
	*hi = (unsigned int)(ll >> 32);
}

time_t FileTimeToUnixTime(unsigned int hi, unsigned int lo)
{
	time_t t = 0;
	FILETIME ft;
	ft.dwHighDateTime = hi;
	ft.dwLowDateTime = lo;
	
	if(FileTimeToUnixTime(ft, &t) != hrSuccess)
		return 0;
	
	return t;
}

void RTimeToFileTime(LONG rtime, FILETIME *pft)
{
	// ASSERT(pft != NULL);
	ULARGE_INTEGER *puli = (ULARGE_INTEGER *)pft;
	puli->QuadPart = rtime * UnitsPerMinute;
}
 
void FileTimeToRTime(FILETIME *pft, LONG* prtime)
{
	// ASSERT(pft != NULL);
	// ASSERT(prtime != NULL);

	ULARGE_INTEGER uli = *(ULARGE_INTEGER *)pft;

	uli.QuadPart += UnitsPerHalfMinute;
	uli.QuadPart /= UnitsPerMinute;
	*prtime = uli.LowPart;
}

HRESULT RTimeToUnixTime(LONG rtime, time_t *unixtime)
{
	HRESULT hr = hrSuccess;
	FILETIME ft;

	if(unixtime == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	RTimeToFileTime(rtime, &ft);
	FileTimeToUnixTime(ft, unixtime);

exit:
	return hr;
}

HRESULT UnixTimeToRTime(time_t unixtime, LONG *rtime)
{
	HRESULT hr = hrSuccess;
	FILETIME ft;

	if(rtime == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	UnixTimeToFileTime(unixtime, &ft);
	FileTimeToRTime(&ft, rtime);

exit:
	return hr;
}

// time only, not date!
time_t SystemTimeToUnixTime(SYSTEMTIME stime)
{
	return stime.wSecond + (stime.wMinute*60) + ((stime.wHour)*60*60);
}

SYSTEMTIME UnixTimeToSystemTime(time_t unixtime)
{
	SYSTEMTIME stime = {0};
	stime.wSecond = unixtime%60;
	unixtime /= 60;
	stime.wMinute = unixtime%60;
	unixtime /= 60;
	stime.wHour = unixtime;
	return stime;
}

SYSTEMTIME TMToSystemTime(struct tm t)
{
	SYSTEMTIME stime = {0};
	stime.wYear = t.tm_year;
	stime.wMonth = t.tm_mon;
	stime.wDayOfWeek = t.tm_wday;
	stime.wDay = t.tm_mday;
	stime.wHour = t.tm_hour;
	stime.wMinute = t.tm_min;
	stime.wSecond = t.tm_sec;
	stime.wMilliseconds = 0;
	return stime;	
}

struct tm SystemTimeToTM(SYSTEMTIME stime)
{
	// not quite, since we miss tm_yday
	struct tm t = {0};
	t.tm_year = stime.wYear;
	t.tm_mon = stime.wMonth;
	t.tm_wday = stime.wDayOfWeek;
	t.tm_mday = stime.wDay;
	t.tm_hour = stime.wHour;
	t.tm_min = stime.wMinute;
	t.tm_sec = stime.wSecond;
	t.tm_isdst = -1;
	return t;	
}

/* The 'IntDate' and 'IntTime' date and time encoding are used for some CDO calculations. They
 * are basically a date or time encoded in a bitshifted way, packed so that it uses the least amount
 * of bits. Eg. a date (day,month,year) is encoded as 5 bits for the day (1-31), 4 bits for the month (1-12),
 * and the rest of the bits (32-4-5 = 23) for the year. The same goes for time, with seconds and minutes
 * each using 6 bits and 32-6-6=20 bits for the hours.
 *
 * For dates, everything is 1-index (1st January is 1-1) and year is full (2008)
 */
 
/**
 * Create INT date
 *
 * @param[in] day Day of month 1-31
 * @param[in] month Month of year 1-12
 * @param[in] year Full year (eg 2008)
 * @return ULONG Calculated INT date
 */
ULONG   CreateIntDate(ULONG day, ULONG month, ULONG year)
{
	return day + month * 32 + year * 32 * 16;
}

/**
 * Create INT time
 *
 * @param[in] seconds Seconds 0-59
 * @param[in] minutes Minutes 0-59
 * @param[in] hours Hours
 * @return Calculated INT time
 */
ULONG   CreateIntTime(ULONG seconds, ULONG minutes, ULONG hours)
{
	return seconds + minutes * 64 + hours * 64 * 64;
}

/**
 * Create INT date from filetime
 *
 * Discards time information from the passed FILETIME stamp, and returns the date
 * part as an INT date. The passed FILETIME is interpreted in GMT.
 *
 * @param[in] ft FileTime to convert
 * @return Converted DATE part of the file time.
 */
ULONG FileTimeToIntDate(FILETIME &ft)
{
	struct tm date;
	time_t t;
	FileTimeToUnixTime(ft, &t);
	gmtime_safe(&t, &date);
	
	return CreateIntDate(date.tm_mday, date.tm_mon+1, date.tm_year+1900);
}

/**
 * Create INT time from offset in seconds
 *
 * Creates an INT time value for the moment at which the passed amount of seconds
 * has passed on a day.
 *
 * @param[in] seconds Number of seconds since beginning of day
 * @return Converted INT time
 */
ULONG SecondsToIntTime(ULONG seconds)
{
	ULONG hours = seconds / (60*60);
	seconds -= hours * 60 * 60;
	ULONG minutes = seconds / 60;
	seconds -= minutes * 60;
	
	return CreateIntTime(seconds, minutes, hours);
}

bool operator ==(FILETIME a, FILETIME b) {
	return a.dwLowDateTime == b.dwLowDateTime && a.dwHighDateTime == b.dwHighDateTime;
}

bool operator >(FILETIME a, FILETIME b)
{
	return ((a.dwHighDateTime > b.dwHighDateTime) ||
		((a.dwHighDateTime == b.dwHighDateTime) &&
		 (a.dwLowDateTime > b.dwLowDateTime)));
}

bool operator <(FILETIME a, FILETIME b)
{
	return ((a.dwHighDateTime < b.dwHighDateTime) ||
		((a.dwHighDateTime == b.dwHighDateTime) &&
		 (a.dwLowDateTime < b.dwLowDateTime)));
}

time_t operator -(FILETIME a, FILETIME b)
{
	time_t aa, bb;

	FileTimeToUnixTime(a, &aa);
	FileTimeToUnixTime(b, &bb);

	return aa - bb;
}

#ifndef HAVE_TIMEGM
time_t timegm(struct tm *t) {
	time_t convert;
	char *tz = NULL;
	char *s_tz = NULL;

	tz = getenv("TZ");
	if(tz)
		s_tz = strdup(tz);

	// SuSE 9.1 segfaults when putenv() is used in a detached thread on the next getenv() call.
	// so use setenv() on linux, putenv() on others.

	setenv("TZ", "UTC0", 1);
	tzset();

	convert = mktime(t);

	if (s_tz) {
		setenv("TZ", s_tz, 1);
		tzset();
	} else {
		unsetenv("TZ");
		tzset();
	}

	if(s_tz)
		free(s_tz);

	return convert;
}
#endif

struct tm* gmtime_safe(const time_t* timer, struct tm *result)
{
	struct tm *tmp = NULL;
	tmp = gmtime_r(timer, result);

	if(tmp == NULL)
		memset(result, 0, sizeof(struct tm));

	return tmp;
}

struct timespec GetDeadline(unsigned int ulTimeoutMs)
{
	struct timespec	deadline;
	struct timeval	now;
	gettimeofday(&now, NULL);

	now.tv_sec += ulTimeoutMs / 1000;
	now.tv_usec += 1000 * (ulTimeoutMs % 1000);
	if (now.tv_usec >= 1000000) {
		now.tv_sec++;
		now.tv_usec -= 1000000;
	}

	deadline.tv_sec = now.tv_sec;
	deadline.tv_nsec = now.tv_usec * 1000;

	return deadline;
}

// Does mkdir -p <path>
int CreatePath(const char *createpath)
{
	struct stat s;
	char *path = strdup(createpath);

	// Remove trailing slashes
	while(path[strlen(path)-1] == '/' || path[strlen(path)-1] == '\\') {
		path[strlen(path)-1] = 0;
	}


	if(stat(path, &s) == 0) {
		if(s.st_mode & S_IFDIR) {
			free(path);
			return 0; // Directory is already there
		} else {
			free(path);
			return -1; // Item is not a directory
		}
	} else {
		// We need to create the directory

		// First, create parent directories
		char *trail = strrchr(path, '/') > strrchr(path, '\\') ? strrchr(path, '/') : strrchr(path, '\\');

		if(!trail) {
		    // Should only happen if you're trying to create /path/to/dir in win32
		    // or \path\to\dir in linux
		    free(path);
		    return -1;
		}
		
		*trail = 0;

		if(CreatePath(path) != 0) {
			free(path);
			return -1;
		}

		// Create the actual directory
		int ret = CreateDir(createpath, 0700);
		free(path);
		return ret;
	}
}

double GetTimeOfDay()
{
	struct timeval tv;

	gettimeofday(&tv, NULL);

	return (double)tv.tv_sec + ((double)tv.tv_usec / 1000000); // usec = microsec = 1 millionth of a second
}

