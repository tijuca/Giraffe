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

#ifndef PLATFORM_H
#define PLATFORM_H

/* About _FORTIFY_SOURCE a.k.a. _BREAKIFY_SOURCE_IN_INSANE_WAYS
 *
 * This has the insane feature that it will assert() when you attempt
 * to FD_SET(n, &set) with n > 1024, irrespective of your compile-time FD_SETSIZE
 * Even stranger, this should be easily fixed but nobody is interested apparently.
 *
 * Although you could just not use select() anywhere, we depend on some libs that
 * are doing select(), so we can't just remove them. 
 *
 * This will also disable a few other valid buffer-overflow checks, but we'll have
 * to live with that for now.
 */

#ifdef _FORTIFY_SOURCE
#undef _FORTIFY_SOURCE
#endif


  // We have to include this now in case select.h is included too soon.
  // Increase our maximum amount of file descriptors to 8192
  #include <bits/types.h>
  #undef __FD_SETSIZE
  #define __FD_SETSIZE 8192

  // Log the pthreads locks
  #define DEBUG_PTHREADS 0

  #ifdef HAVE_CONFIG_H
  #include "config.h"
  #endif
  #include "platform.linux.h"

#define ZARAFA_SYSTEM_USER		"SYSTEM"
#define ZARAFA_SYSTEM_USER_W	L"SYSTEM"

static const LONGLONG UnitsPerMinute = 600000000;
static const LONGLONG UnitsPerHalfMinute = 300000000;

/*
 * Platform independent functions
 */
HRESULT	UnixTimeToFileTime(time_t t, FILETIME *ft);
HRESULT	FileTimeToUnixTime(FILETIME &ft, time_t *t);
void	UnixTimeToFileTime(time_t t, int *hi, unsigned int *lo);
time_t	FileTimeToUnixTime(unsigned int hi, unsigned int lo);
void	RTimeToFileTime(LONG rtime, FILETIME *pft);
void	FileTimeToRTime(FILETIME *pft, LONG* prtime);
HRESULT	UnixTimeToRTime(time_t unixtime, LONG *rtime);
HRESULT	RTimeToUnixTime(LONG rtime, time_t *unixtime);
time_t SystemTimeToUnixTime(SYSTEMTIME stime);
SYSTEMTIME UnixTimeToSystemTime(time_t unixtime);
SYSTEMTIME TMToSystemTime(struct tm t);
struct tm SystemTimeToTM(SYSTEMTIME stime);
double GetTimeOfDay();
ULONG	CreateIntDate(ULONG day, ULONG month, ULONG year);
ULONG	CreateIntTime(ULONG seconds, ULONG minutes, ULONG hours);
ULONG	FileTimeToIntDate(FILETIME &ft);
ULONG	SecondsToIntTime(ULONG seconds);


inline double difftimeval(struct timeval *ptstart, struct timeval *ptend) {
	return 1000000 * (ptend->tv_sec - ptstart->tv_sec) + (ptend->tv_usec - ptstart->tv_usec);
}

struct tm* gmtime_safe(const time_t* timer, struct tm *result);

/**
 * Creates the deadline timespec, which is the current time plus the specified
 * amount of milliseconds.
 *
 * @param[in]	ulTimeoutMs		The timeout in ms.
 * @return		The required timespec.
 */
struct timespec GetDeadline(unsigned int ulTimeoutMs);

bool operator ==(FILETIME a, FILETIME b);
bool operator >(FILETIME a, FILETIME b);
bool operator <(FILETIME a, FILETIME b);
time_t operator -(FILETIME a, FILETIME b);

/* convert struct tm to time_t in timezone UTC0 (GM time) */
#ifndef HAVE_TIMEGM
time_t timegm(struct tm *t);
#endif

// mkdir -p 
int CreatePath(const char *createpath);

// Random-number generators
#define RAND_MAX_MT RAND_MAX
void	rand_init();
int		rand_mt();
void	rand_free();

char *	get_password(const char *prompt);

void	sleep_ms(unsigned int millis);

 #define ZARAFA_API

#endif // PLATFORM_H
