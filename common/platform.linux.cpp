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

#include <sys/select.h>
#include <sys/time.h>
#include <iconv.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <mapicode.h>			// return codes
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>

#include <string>
#include <map>

#ifdef __APPLE__
// bsd
#define ICONV_CONST const
#elif OPENBSD
// bsd
#define ICONV_CONST const
#else
// linux
#define ICONV_CONST
#endif

unsigned int seed = 0;
bool rand_init_done = false;

bool operator!=(GUID a, GUID b) {
	if (memcmp((void*)&a, (void*)&b, sizeof(GUID)) == 0)
		return false;
	return true;
}

bool operator==(REFIID a, GUID b) {
	if (memcmp((void*)&a, (void*)&b, sizeof(GUID)) == 0)
		return true;
	return false;
}

HRESULT CoCreateGuid(LPGUID pNewGUID) {
	if (!pNewGUID)
		return MAPI_E_INVALID_PARAMETER;

#if HAVE_UUID_CREATE
#ifdef OPENBSD
	uuid_t *g = NULL;
	void *vp = NULL;
	size_t n = 0;
	// error codes are not checked!
	uuid_create(&g);
	uuid_make(g, UUID_MAKE_V1);
	uuid_export(g, UUID_FMT_BIN, &vp, &n);
	memcpy(pNewGUID, &vp, UUID_LEN_BIN);
	uuid_destroy(g);
#else
	uuid_t g;
	uint32_t uid_ret;
	uuid_create(&g, &uid_ret);
	memcpy(pNewGUID, &g, sizeof(g));
#endif // OPENBSD
#else
	uuid_t g;
	uuid_generate(g);
	memcpy(pNewGUID, g, sizeof(g));
#endif

	return S_OK;
}

void strupr(char* a) {
	while (*a != '\0') {
		*a = toupper (*a);
		++a;
	}
}

__int64_t Int32x32To64(ULONG a, ULONG b) {
	return (__int64_t)a*(__int64_t)b;
}

void GetSystemTimeAsFileTime(FILETIME *ft) {
	struct timeval now;
	__int64_t l;
	gettimeofday(&now,NULL); // null==timezone
	l = ((__int64_t)now.tv_sec * 10000000) + ((__int64_t)now.tv_usec * 10) + (__int64_t)NANOSECS_BETWEEN_EPOCHS;
	ft->dwLowDateTime = (unsigned int)(l & 0xffffffff);
	ft->dwHighDateTime = l >> 32;
}

DWORD GetTempPath(DWORD inLen, char *lpBuffer)
{
	char *env = NULL;
	DWORD len;

	env = getenv("TMP");
	if (!env || env[0] == '\0')
		env = getenv("TEMP");
	if (!env || env[0] == '\0')
		env = "/tmp/";

	len = strlen(env);
	if ((len+1) > inLen)
		return 0;

	// add trailing / at end
	if (env[len-1] != '/') {
		env[len] = '/';
		len++;
		env[len] = '\0';
	}

	strcpy(lpBuffer, env);

	return len;
}

void Sleep(unsigned int msec) {
	struct timespec ts;
	unsigned int rsec;
	ts.tv_sec = msec/1000;
	rsec = msec - (ts.tv_sec*1000);
	ts.tv_nsec = rsec*1000*1000;
	nanosleep(&ts, NULL);
}

void rand_init() {
	int fd;
	
	if (rand_init_done)
		return; // Already init

	fd = open("/dev/urandom", O_RDONLY);
	
	if(fd == -1)
		seed = time(NULL);
	else {
		read(fd, (char *)&seed, sizeof(unsigned int));
		close(fd);
	}
	
	rand_init_done = true;
}

int rand_mt() {
	if(!rand_init_done)
		rand_init();
		
	return rand_r(&seed);
}

void rand_free() {
	//Nothing to free
}

char * get_password(const char *prompt) {
	return getpass(prompt);
}

HGLOBAL GlobalAlloc(UINT uFlags, ULONG ulSize)
{
	// always returns NULL, as required by CreateStreamOnHGlobal implementation in mapi4linux/src/mapiutil.cpp
	return NULL;
}

time_t GetProcessTime()
{
	time_t t;

	time(&t);

	return t;
}

void sleep_ms(unsigned int millis) 
{
	struct timeval tv;
	tv.tv_sec = millis / 1000;
	tv.tv_usec = millis * 1000;
	select(0,NULL,NULL,NULL,&tv);
}

#if DEBUG_PTHREADS

class Lock {
public:
       Lock() { locks = 0; busy = 0; dblTime = 0; };
       ~Lock() {};

       std::string strLocation;
       unsigned int locks;
       unsigned int busy;
       double dblTime;
};

std::map<std::string, Lock> my_pthread_map;
pthread_mutex_t my_mutex;
int init = 0;

#undef pthread_mutex_lock
int my_pthread_mutex_lock(const char *file, unsigned int line, pthread_mutex_t *__mutex)
{
       char s[1024];
       snprintf(s, sizeof(s), "%s:%d", file, line);
       double dblTime;
       int err = 0;

       if(!init) {
               init = 1;
               pthread_mutex_init(&my_mutex, NULL);
       }

       pthread_mutex_lock(&my_mutex);
       my_pthread_map[s].strLocation = s;
       my_pthread_map[s].locks++;
       pthread_mutex_unlock(&my_mutex);

       if(( err = pthread_mutex_trylock(__mutex)) == EBUSY) {
               pthread_mutex_lock(&my_mutex);
               my_pthread_map[s].busy++;
               pthread_mutex_unlock(&my_mutex);
               dblTime = GetTimeOfDay();
               err = pthread_mutex_lock(__mutex);
               pthread_mutex_lock(&my_mutex);
               my_pthread_map[s].dblTime += GetTimeOfDay() - dblTime;
               pthread_mutex_unlock(&my_mutex);
       }

       return err;
}

#undef pthread_rwlock_rdlock
int my_pthread_rwlock_rdlock(const char *file, unsigned int line, pthread_rwlock_t *__mutex)
{
       char s[1024];
       snprintf(s, sizeof(s), "%s:%d", file, line);
       double dblTime;
       int err = 0;

       if(!init) {
               init = 1;
               pthread_mutex_init(&my_mutex, NULL);
       }

       pthread_mutex_lock(&my_mutex);
       my_pthread_map[s].strLocation = s;
       my_pthread_map[s].locks++;
       pthread_mutex_unlock(&my_mutex);

       if(( err = pthread_rwlock_tryrdlock(__mutex)) == EBUSY) {
               pthread_mutex_lock(&my_mutex);
               my_pthread_map[s].busy++;
               pthread_mutex_unlock(&my_mutex);
               dblTime = GetTimeOfDay();
               err = pthread_rwlock_rdlock(__mutex);
               pthread_mutex_lock(&my_mutex);
               my_pthread_map[s].dblTime += GetTimeOfDay() - dblTime;
               pthread_mutex_unlock(&my_mutex);
       }

       return err;
}

#undef pthread_rwlock_wrlock
int my_pthread_rwlock_wrlock(const char *file, unsigned int line, pthread_rwlock_t *__mutex)
{
       char s[1024];
       snprintf(s, sizeof(s), "%s:%d", file, line);
       double dblTime;
       int err = 0;

       if(!init) {
               init = 1;
               pthread_mutex_init(&my_mutex, NULL);
       }

       pthread_mutex_lock(&my_mutex);
       my_pthread_map[s].strLocation = s;
       my_pthread_map[s].locks++;
       pthread_mutex_unlock(&my_mutex);

       if(( err = pthread_rwlock_trywrlock(__mutex)) == EBUSY) {
               pthread_mutex_lock(&my_mutex);
               my_pthread_map[s].busy++;
               pthread_mutex_unlock(&my_mutex);
               dblTime = GetTimeOfDay();
               err = pthread_rwlock_wrlock(__mutex);
               pthread_mutex_lock(&my_mutex);
               my_pthread_map[s].dblTime += GetTimeOfDay() - dblTime;
               pthread_mutex_unlock(&my_mutex);
       }

       return err;
}

std::string dump_pthread_locks()
{
       std::map<std::string, Lock>::iterator i;
       std::string strLog;
       char s[2048];

       for(i=my_pthread_map.begin(); i!= my_pthread_map.end(); i++) {
               snprintf(s,sizeof(s), "%s\t\t%d\t\t%d\t\t%f\n", i->second.strLocation.c_str(), i->second.locks, i->second.busy, (float)i->second.dblTime);
               strLog += s;
       }

       return strLog;
}
#endif

