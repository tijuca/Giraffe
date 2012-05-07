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

// zarafa-monitor.cpp : Defines the entry point for the console application.
//

#include "platform.h"


#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <signal.h>

#include <mapi.h>
#include <mapix.h>
#include <mapiutil.h>
#include <mapidefs.h>

#include <mapiguid.h>
#include <ctype.h>
#include "my_getopt.h"

#include "ECScheduler.h"
#include "ECMonitorDefs.h"
#include "ECQuotaMonitor.h"

#include "CommonUtil.h"
#include "UnixUtil.h"
#include "ecversion.h"
#include "charset/localeutil.h"

using namespace std;

void deleteThreadMonitor(LPECTHREADMONITOR	lpThreadMonitor, bool base = false) {

	if(lpThreadMonitor == NULL)
		return;

	if(lpThreadMonitor->lpConfig)
		delete lpThreadMonitor->lpConfig;

	if(lpThreadMonitor->lpLogger)
		lpThreadMonitor->lpLogger->Release();

	if(base)
		delete lpThreadMonitor;
}

LPECTHREADMONITOR	m_lpThreadMonitor;

pthread_mutex_t		m_hExitMutex;
pthread_cond_t		m_hExitSignal;
pthread_t			mainthread;

HRESULT running_service( char* szPath );

void sighandle(int sig) {
	// Win32 has unix semantics and therefore requires us to reset the signal handler.
	signal(SIGTERM , sighandle);
	signal(SIGINT  , sighandle);	// CTRL+C

	if (m_lpThreadMonitor && m_lpThreadMonitor->bShutdown == false)// do not log multimple shutdown messages
		m_lpThreadMonitor->lpLogger->Log(EC_LOGLEVEL_FATAL, "Termination requested, shutting down.");
	
	m_lpThreadMonitor->bShutdown = true;

	pthread_cond_signal(&m_hExitSignal);
}

void sighup(int signr) {
	// In Win32, the signal is sent in a seperate, special signal thread. So this test is
	// not needed or required.
	if (pthread_equal(pthread_self(), mainthread)==0)
		return;
	if (m_lpThreadMonitor) {
		if (m_lpThreadMonitor->lpConfig) {
			if (!m_lpThreadMonitor->lpConfig->ReloadSettings() && m_lpThreadMonitor->lpLogger)
				m_lpThreadMonitor->lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to reload configuration file, continuing with current settings.");
		}

		if (m_lpThreadMonitor->lpLogger) {
			if (m_lpThreadMonitor->lpConfig) {
				char *ll = m_lpThreadMonitor->lpConfig->GetSetting("log_level");
				int new_ll = ll ? atoi(ll) : 2;
				m_lpThreadMonitor->lpLogger->SetLoglevel(new_ll);
			}

			m_lpThreadMonitor->lpLogger->Reset();
			m_lpThreadMonitor->lpLogger->Log(EC_LOGLEVEL_WARNING, "Log connection was reset");
		}
	}
}

// SIGSEGV catcher
#include <execinfo.h>

void sigsegv(int signr)
{
	void *bt[64];
	int i, n;
	char **btsymbols;

	if(!m_lpThreadMonitor || !m_lpThreadMonitor->lpLogger)
		goto exit;

	switch (signr) {
	case SIGSEGV:
		m_lpThreadMonitor->lpLogger->Log(EC_LOGLEVEL_FATAL, "Caught SIGSEGV (%d), traceback:", signr);
		break;
	case SIGBUS:
		m_lpThreadMonitor->lpLogger->Log(EC_LOGLEVEL_FATAL, "Caught SIGBUS (%d), possible invalid mapped memory access, traceback:", signr);
		break;
	case SIGABRT:
		m_lpThreadMonitor->lpLogger->Log(EC_LOGLEVEL_FATAL, "Caught SIGABRT (%d), out of memory or unhandled exception, traceback:", signr);
		break;
	};

	n = backtrace(bt, 64);

	btsymbols = backtrace_symbols(bt, n);

	for (i = 0; i < n; i++) {
		if (btsymbols)
			m_lpThreadMonitor->lpLogger->Log(EC_LOGLEVEL_FATAL, "%016p %s", bt[i], btsymbols[i]);
		else
			m_lpThreadMonitor->lpLogger->Log(EC_LOGLEVEL_FATAL, "%016p", bt[i]);
	}

	m_lpThreadMonitor->lpLogger->Log(EC_LOGLEVEL_FATAL, "When reporting this traceback, please include Linux distribution name, system architecture and Zarafa version.");

exit:
	kill(0, signr);
}


void print_help(char *name) {
	cout << "Usage:\n" << endl;
	cout << name << " [-F] [-h|--host <serverpath>] [-c|--config <configfile>]" << endl;
	cout << "  -F\t\tDo not run in the background" << endl;
	cout << "  -h path\tUse alternate connect path (e.g. file:///var/run/socket).\n\t\tDefault: file:///var/run/zarafa" << endl;
	cout << "  -c filename\tUse alternate config file (e.g. /etc/zarafa-monitor.cfg)\n\t\tDefault: /etc/zarafa/monitor.cfg" << endl;
	cout << endl;
}

int main(int argc, char *argv[]) {

	HRESULT hr = hrSuccess;
	const char *szConfig = ECConfig::GetDefaultPath("monitor.cfg");
	char *szPath = NULL;
	int c;
	int daemonize = 1;


	// Default settings
	const configsetting_t lpDefaults[] = {
		{ "smtp_server","localhost" },
		{ "server_socket", CLIENT_ADMIN_SOCKET },
		{ "run_as_user", "" },
		{ "run_as_group", "" },
		{ "pid_file", "/var/run/zarafa-monitor.pid" },
		{ "running_path", "/" },
		{ "log_method","file" },
		{ "log_file","/var/log/zarafa/monitor.log" },
		{ "log_level","2", CONFIGSETTING_RELOADABLE },
		{ "log_timestamp","1" },
		{ "sslkey_file", "" },
		{ "sslkey_pass", "", CONFIGSETTING_EXACT },
		{ "quota_check_interval", "15" },
		{ "mailquota_resend_interval", "1", CONFIGSETTING_RELOADABLE },
		{ "userquota_warning_template", "/etc/zarafa/quotamail/userwarning.mail", CONFIGSETTING_RELOADABLE },
		{ "userquota_soft_template", "/etc/zarafa/quotamail/usersoft.mail", CONFIGSETTING_RELOADABLE },
		{ "userquota_hard_template", "/etc/zarafa/quotamail/userhard.mail", CONFIGSETTING_RELOADABLE },
		{ "companyquota_warning_template", "/etc/zarafa/quotamail/companywarning.mail", CONFIGSETTING_RELOADABLE },
		{ "companyquota_soft_template", "/etc/zarafa/quotamail/companysoft.mail", CONFIGSETTING_RELOADABLE },
		{ "companyquota_hard_template", "/etc/zarafa/quotamail/companyhard.mail", CONFIGSETTING_RELOADABLE },
		{ NULL, NULL },
	};

	enum {
		OPT_HELP,
		OPT_HOST,
		OPT_CONFIG,
		OPT_FOREGROUND
	};
	struct option long_options[] = {
		{ "help", 0, NULL, OPT_HELP },
		{ "host", 1, NULL, OPT_HOST },
		{ "config", 1, NULL, OPT_CONFIG },
		{ "foreground", 1, NULL, OPT_FOREGROUND },
		{ NULL, 0, NULL, 0 }
	};

	if (!forceUTF8Locale(true))
		goto exit;

	while(1) {
		c = my_getopt_long(argc, argv, "c:h:iuFV", long_options, NULL);
		
		if(c == -1)
			break;
			
		switch(c) {
		case OPT_CONFIG:
		case 'c':
			szConfig = my_optarg;
			break;
		case OPT_HOST:
		case 'h':
			szPath = my_optarg;
			break;
		case 'i': // Install service
		case 'u': // Uninstall service
			break;
		case OPT_FOREGROUND:
		case 'F':
			daemonize = 0;
			break;
		case 'V':
			cout << "Product version:\t" <<  PROJECT_VERSION_MONITOR_STR << endl
				 << "File version:\t\t" << PROJECT_SVN_REV_STR << endl;
			return 1;
		case OPT_HELP:
		default:
			print_help(argv[0]);
			return 1;
		}
	}

	m_lpThreadMonitor = new ECTHREADMONITOR;

	m_lpThreadMonitor->lpConfig = ECConfig::Create(lpDefaults);
	if (!m_lpThreadMonitor->lpConfig->LoadSettings(szConfig) || m_lpThreadMonitor->lpConfig->HasErrors()) {
		m_lpThreadMonitor->lpLogger = new ECLogger_File(EC_LOGLEVEL_FATAL, 0, "-"); // create fatal logger without a timestamp to stderr
		LogConfigErrors(m_lpThreadMonitor->lpConfig, m_lpThreadMonitor->lpLogger);
		hr = E_FAIL;
		goto exit;
	}

	mainthread = pthread_self();

	// setup logging
	m_lpThreadMonitor->lpLogger = CreateLogger(m_lpThreadMonitor->lpConfig, argv[0], "Zarafa-Monitor");

	if (m_lpThreadMonitor->lpConfig->HasWarnings())
		LogConfigErrors(m_lpThreadMonitor->lpConfig, m_lpThreadMonitor->lpLogger);


	// set socket filename
	if (!szPath)
		szPath = m_lpThreadMonitor->lpConfig->GetSetting("server_socket");

	signal(SIGTERM, sighandle);
	signal(SIGINT, sighandle);
	signal(SIGHUP, sighup);

	// SIGSEGV backtrace support
	stack_t st;
	struct sigaction act;

	memset(&st, 0, sizeof(st));
	memset(&act, 0, sizeof(act));

	st.ss_sp = malloc(65536);
	st.ss_flags = 0;
	st.ss_size = 65536;

	act.sa_handler = sigsegv;
	act.sa_flags = SA_ONSTACK | SA_RESETHAND;

	sigaltstack(&st, NULL);
	sigaction(SIGSEGV, &act, NULL);
	sigaction(SIGBUS, &act, NULL);
	sigaction(SIGABRT, &act, NULL);

	// fork if needed and drop privileges as requested.
	// this must be done before we do anything with pthreads
	if (daemonize && unix_daemonize(m_lpThreadMonitor->lpConfig, m_lpThreadMonitor->lpLogger))
		goto exit;
	if (!daemonize)
		setsid();
	if (unix_create_pidfile(argv[0], m_lpThreadMonitor->lpConfig, m_lpThreadMonitor->lpLogger, false) < 0)
		goto exit;
	if (unix_runas(m_lpThreadMonitor->lpConfig, m_lpThreadMonitor->lpLogger))
		goto exit;

	// Init exit threads
	pthread_mutex_init(&m_hExitMutex, NULL);
	pthread_cond_init(&m_hExitSignal, NULL);

		hr = running_service(szPath);


exit:
	if(m_lpThreadMonitor)
		deleteThreadMonitor(m_lpThreadMonitor, true);

	return hr == hrSuccess ? 0 : 1;
}

HRESULT running_service( char* szPath)
{
	HRESULT			hr = hrSuccess;
	ECScheduler*	lpECScheduler = NULL;
	unsigned int	ulInterval = 0;
	bool			bMapiInit = false;
	
	hr = MAPIInitialize(NULL);
	if (hr != hrSuccess) {
		m_lpThreadMonitor->lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to initialize MAPI");
		goto exit;
	}
	bMapiInit = true;

	lpECScheduler = new ECScheduler(m_lpThreadMonitor->lpLogger);

	ulInterval = atoi(m_lpThreadMonitor->lpConfig->GetSetting("quota_check_interval", NULL, "15"));
	if (ulInterval == 0)
		ulInterval = 15;

	m_lpThreadMonitor->lpLogger->Log(EC_LOGLEVEL_FATAL, "Starting zarafa-monitor version " PROJECT_VERSION_MONITOR_STR " (" PROJECT_SVN_REV_STR "), pid %d", getpid());

	// Add Quota monitor
	hr = lpECScheduler->AddSchedule(SCHEDULE_MINUTES, ulInterval, ECQuotaMonitor::Create, m_lpThreadMonitor);
	if(hr != hrSuccess) {
		m_lpThreadMonitor->lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to add quota monitor schedule");
		goto exit;
	}

	pthread_mutex_lock(&m_hExitMutex);

	pthread_cond_wait(&m_hExitSignal, &m_hExitMutex);

	pthread_mutex_unlock(&m_hExitMutex);

exit:
	if (lpECScheduler)
		delete lpECScheduler;
		
	if (bMapiInit)
		MAPIUninitialize();
		
	return hr;
}
