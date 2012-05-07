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

// Damn windows header defines max which break C++ header files
#undef max

#include <pthread.h>
#include <mapix.h>
#include <time.h>
#include <iostream>

#include "ECMapiUtils.h"

// Returns the FILETIME as GM-time
FILETIME vmimeDatetimeToFiletime(vmime::datetime dt) {
	FILETIME sFiletime;
	struct tm when;
	int iYear, iMonth, iDay, iHour, iMinute, iSecond, iZone;
	time_t lTmpTime;

	dt.getDate( iYear, iMonth, iDay );	
	dt.getTime( iHour, iMinute, iSecond, iZone );

	when.tm_hour	= iHour;	
	when.tm_min		= iMinute - iZone; // Zone is expressed in minutes. mktime() will normalize negative values or values over 60
	when.tm_sec		= iSecond;
	when.tm_mon		= iMonth - 1;	
	when.tm_mday	= iDay;	
	when.tm_year	= iYear - 1900;
	when.tm_isdst	= -1;		// ignore dst

	lTmpTime = timegm(&when);

	UnixTimeToFileTime(lTmpTime, &sFiletime);
	return sFiletime;
}

vmime::datetime FiletimeTovmimeDatetime(FILETIME ft) {
	time_t tmp;
	struct tm convert;

	FileTimeToUnixTime(ft, &tmp);
	gmtime_safe(&tmp, &convert);
	return vmime::datetime(convert.tm_year + 1900, convert.tm_mon + 1, convert.tm_mday, convert.tm_hour, convert.tm_min, convert.tm_sec);
}
