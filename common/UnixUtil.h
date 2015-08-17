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

#ifndef __UNIXUTIL_H
#define __UNIXUTIL_H


#include <sys/resource.h>

#include "ECLogger.h"
#include "ECConfig.h"

struct popen_rlimit {
	int resource;
	struct rlimit limit;
};

struct popen_rlimit_array {
	unsigned int cValues;
	struct popen_rlimit sLimit[1];
};

#define sized_popen_rlimit_array(_climit, _name) \
struct _popen_rlimit_array_ ## _name \
{ \
	unsigned int cValues; \
	struct popen_rlimit sLimit[_climit]; \
} _name

int unix_runas(ECConfig *lpConfig, ECLogger *lpLogger);
int unix_chown(const char *filename, const char *username, const char *groupname);
extern void unix_coredump_enable(ECLogger *);
int unix_create_pidfile(const char *argv0, ECConfig *lpConfig, ECLogger *lpLogger, bool bForce = true);
int unix_daemonize(ECConfig *lpConfig, ECLogger *lpLogger);
int unix_fork_function(void*(func)(void*), void *param, int nCloseFDs, int *pCloseFDs);
pid_t unix_popen_rw(ECLogger *lpLogger, const char *command, int *infp, int *outfp, popen_rlimit_array *lpLimits, const char **env, bool bNonBlocking, bool bStdErr);
int unix_pclose(int rfd, int wfd, pid_t pid);
bool unix_system(ECLogger *lpLogger, const char *szLogName, const char *command, const char **env);



#endif
