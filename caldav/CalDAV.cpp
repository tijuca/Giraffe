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
#include "CalDAV.h"
#include "Http.h"
#include "CalDavUtil.h"
#include "iCal.h"
#include "WebDav.h"
#include "CalDavProto.h"
#include "ProtocolBase.h"
#include <signal.h>

#include <iostream>
#include <string>

#include "ECLogger.h"
#include "ECChannel.h"
#include "my_getopt.h"
#include "ecversion.h"
#include "CommonUtil.h"
#include "SSLUtil.h"

using namespace std;

#include <execinfo.h>
#include "UnixUtil.h"

#if HAVE_ICU
#include "unicode/uclean.h"
#endif


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


struct HandlerArgs {
    ECChannel *lpChannel;
	bool bUseSSL;
};

bool g_bDaemonize = true;
bool g_bQuit = false;
bool g_bThreads = false;
ECLogger *g_lpLogger = NULL;
ECConfig *g_lpConfig = NULL;
pthread_t mainthread;
int nChildren = 0;

#define KEEP_ALIVE_TIME 300

void sigterm(int) {
	g_bQuit = true;
}

void sighup(int) {
	// In Win32, the signal is sent in a seperate, special signal thread. So this test is
	// not needed or required.
	if (g_bThreads && pthread_equal(pthread_self(), mainthread)==0)
		return;
	if (g_lpConfig) {
		if (!g_lpConfig->ReloadSettings() && g_lpLogger)
			g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to reload configuration file, continuing with current settings.");
	}

	if (g_lpLogger) {
		if (g_lpConfig) {
			char *ll = g_lpConfig->GetSetting("log_level");
			int new_ll = ll ? atoi(ll) : 2;
			g_lpLogger->SetLoglevel(new_ll);
		}

		g_lpLogger->Reset();
		g_lpLogger->Log(EC_LOGLEVEL_WARNING, "Log connection was reset");
	}
}


void sigchld(int) {
	int stat;
	while (waitpid (-1, &stat, WNOHANG) > 0) nChildren--;
}

void sigsegv(int signr)
{
	void *bt[64];
	int i, n;
	char **btsymbols;

	if (!g_lpLogger)
		goto exit;

	switch (signr) {
	case SIGSEGV:
		g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Pid %d caught SIGSEGV (%d), traceback:", getpid(), signr);
		break;
	case SIGBUS:
		g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Pid %d caught SIGBUS (%d), possible invalid mapped memory access, traceback:", getpid(), signr);
		break;
	case SIGABRT:
		g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Pid %d caught SIGABRT (%d), out of memory or unhandled exception, traceback:", getpid(), signr);
		break;
	};

	n = backtrace(bt, 64);

	btsymbols = backtrace_symbols(bt, n);

	for (i = 0; i < n; i++) {
		if (btsymbols)
			g_lpLogger->Log(EC_LOGLEVEL_FATAL, "%016p %s", bt[i], btsymbols[i]);
		else
			g_lpLogger->Log(EC_LOGLEVEL_FATAL, "%016p", bt[i]);
	}

	g_lpLogger->Log(EC_LOGLEVEL_FATAL, "When reporting this traceback, please include Linux distribution name, system architecture and Zarafa version.");

exit:
	kill(getpid(), signr);
}

void PrintHelp(char *name) {
	cout << "Usage:\n" << endl;
	cout << name << " [-h] [-F] [-V] [-c <configfile>]" << endl;
	cout << "  -F\t\tDo not run in the background" << endl;
	cout << "  -h\t\tShows this help." << endl;
	cout << "  -V\t\tPrint version info." << endl;
	cout << "  -c filename\tUse alternate config file (e.g. /etc/zarafa/ical.cfg)\n\t\tDefault: /etc/zarafa/ical.cfg" << endl;
	cout << endl;
	cout << "  --ignore-unknown-config-options\tStart even if the configuration file contains invalid config options" << endl;
	cout << endl;
}

void PrintVersion() {
	cout << "Product version:\t"  <<  PROJECT_VERSION_CALDAV_STR << endl << "File version:\t\t" << PROJECT_SVN_REV_STR << endl;
}


int main(int argc, char **argv) {
	HRESULT hr = hrSuccess;
	int ulListenCalDAV = 0;
	int ulListenCalDAVs = 0;
	bool bIgnoreUnknownConfigOptions = false;

	ECChannel *lpChannel = NULL;
    stack_t st = {0};
    struct sigaction act = {{0}};

	// Configuration
	char opt = '\0';
	const char *lpszCfg = ECConfig::GetDefaultPath("ical.cfg");
	const configsetting_t lpDefaults[] = {
		{ "run_as_user", "" },
		{ "run_as_group", "" },
		{ "pid_file", "/var/run/zarafa-ical.pid" },
		{ "running_path", "/" },
		{ "process_model", "fork" },
		{ "server_bind", "0.0.0.0" },
		{ "ical_port", "8080" },
		{ "ical_enable", "yes" },
		{ "icals_port", "8443" },
		{ "icals_enable", "no" },
		{ "enable_ical_get", "yes", CONFIGSETTING_RELOADABLE },
		{ "server_socket", "http://localhost:236/zarafa" },
		{ "server_timezone","Europe/Amsterdam"},
		{ "default_charset","utf-8"},
		{ "log_method", "file" },
		{ "log_file", "/var/log/zarafa/ical.log" },
		{ "log_level", "3", CONFIGSETTING_RELOADABLE },
		{ "log_timestamp", "1" },
        { "ssl_private_key_file", "/etc/zarafa/ical/privkey.pem" },
        { "ssl_certificate_file", "/etc/zarafa/ical/cert.pem" },
        { "ssl_verify_client", "no" },
        { "ssl_verify_file", "" },
        { "ssl_verify_path", "" },
		{ NULL, NULL },
	};
	enum {
		OPT_IGNORE_UNKNOWN_CONFIG_OPTIONS
	};

	struct option long_options[] = {
		{"help", no_argument, NULL, 'h'},
		{"config", required_argument, NULL, 'c'},
		{"version", no_argument, NULL, 'v'},
		{"foreground", no_argument, NULL, 'F'},
		{"ignore-unknown-config-options", 0, NULL, OPT_IGNORE_UNKNOWN_CONFIG_OPTIONS },
		{NULL, 0, NULL, 0}
	};

	setlocale(LC_CTYPE, "");

	while (1) {
		opt = my_getopt_long_permissive(argc, argv, "Fhc:V", long_options, NULL);

		if (opt == -1)
			break;

		switch (opt) {
			case 'c': 
				lpszCfg = my_optarg; 
				break;
			case 'F': 
				g_bDaemonize = false;
				break;
			case OPT_IGNORE_UNKNOWN_CONFIG_OPTIONS:
				bIgnoreUnknownConfigOptions = true;
				break;
			case 'V': 
				PrintVersion(); 
				goto exit;
			case 'h':
			default:
				PrintHelp(argv[0]);
				goto exit;
		}
	}
	
	// init xml parser
	xmlInitParser();

	g_lpConfig = ECConfig::Create(lpDefaults);
	if (!g_lpConfig->LoadSettings(lpszCfg) || !g_lpConfig->ParseParams(argc-my_optind, &argv[my_optind], NULL) || (!bIgnoreUnknownConfigOptions && g_lpConfig->HasErrors())) {
		g_lpLogger = new ECLogger_File(1, 0, "-");
		LogConfigErrors(g_lpConfig, g_lpLogger);
		goto exit;
	}

	g_lpLogger = CreateLogger(g_lpConfig, argv[0], "ZarafaICal");
	if (!g_lpLogger) {
		fprintf(stderr, "Error loading configuration or parsing commandline arguments.\n");
		goto exit;
	}

	if ((bIgnoreUnknownConfigOptions && g_lpConfig->HasErrors()) || g_lpConfig->HasWarnings())
		LogConfigErrors(g_lpConfig, g_lpLogger);

	if (strncmp(g_lpConfig->GetSetting("process_model"), "thread", strlen("thread")) == 0)
		g_bThreads = true;

	// initialize SSL threading
    ssl_threading_setup();

	hr = MAPIInitialize(NULL);
	if (hr != hrSuccess) {
		fprintf(stderr, "Messaging API could not be initialized.");
		goto exit;
	}

	hr = HrSetupListners(&ulListenCalDAV, &ulListenCalDAVs);
	if (hr != hrSuccess)
		goto exit;


	// setup signals
	signal(SIGTERM, sigterm);
	signal(SIGINT, sigterm);
	signal(SIGHUP, sighup);
	signal(SIGCHLD, sigchld);
	signal(SIGPIPE, SIG_IGN);

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
	if (g_bDaemonize && unix_daemonize(g_lpConfig, g_lpLogger))
		goto exit;
	if (!g_bDaemonize)
		setsid();
	unix_create_pidfile(argv[0], g_lpConfig, g_lpLogger);
	if (unix_runas(g_lpConfig, g_lpLogger))
		goto exit;

	if (g_bThreads == false)
		g_lpLogger = StartLoggerProcess(g_lpConfig, g_lpLogger);
	else
		g_lpLogger->SetLogprefix(LP_TID);

	if (g_bThreads)
		mainthread = pthread_self();

	g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Starting zarafa-ical version " PROJECT_VERSION_CALDAV_STR " (" PROJECT_SVN_REV_STR "), pid %d", getpid());

	hr = HrProcessConnections(ulListenCalDAV, ulListenCalDAVs);
	if (hr != hrSuccess)
		goto exit;


	g_lpLogger->Log(EC_LOGLEVEL_FATAL, "CalDAV Gateway will now exit");

	// in forked mode, send all children the exit signal
	if (g_bThreads == false) {
		int i;

		signal(SIGTERM, SIG_IGN);
		kill(0, SIGTERM);
		i = 30;						// wait max 30 seconds
		while (nChildren && i) {
			if (i % 5 == 0)
				g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Waiting for %d processes to exit", nChildren);
			sleep(1);
			i--;
		}

		if (nChildren)
			g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Forced shutdown with %d procesess left", nChildren);
		else
			g_lpLogger->Log(EC_LOGLEVEL_FATAL, "CalDAV Gateway shutdown complete");
	}

exit:

	if(st.ss_sp)
		free(st.ss_sp);

	ECChannel::HrFreeCtx();

	if (lpChannel)
		delete lpChannel;

	if (g_lpConfig)
		delete g_lpConfig;

	DeleteLogger(g_lpLogger);

	MAPIUninitialize();

	SSL_library_cleanup(); // Remove ssl data for the main application and other related libraries

	// Cleanup ssl parts
	ssl_threading_cleanup();

	// Cleanup libxml2 library
	xmlCleanupParser();

#if HAVE_ICU
	// cleanup ICU data so valgrind is happy
	u_cleanup();
#endif

	return hr;

}

HRESULT HrSetupListners(int *lpulNormal, int *lpulSecure)
{
	HRESULT hr = hrSuccess;
	bool bListen = false;
	bool bListenSecure = false;
	int ulPortICal = 0;
	int ulPortICalS = 0;
	int ulNormalSocket = 0;
	int ulSecureSocket = 0;

	// setup sockets
	bListenSecure = (stricmp(g_lpConfig->GetSetting("icals_enable"), "yes") == 0);
	bListen = (stricmp(g_lpConfig->GetSetting("ical_enable"), "yes") == 0);

	if (!bListen && !bListenSecure) {
		g_lpLogger->Log(EC_LOGLEVEL_FATAL, "No ports to open for listening.");
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	ulPortICal = atoi(g_lpConfig->GetSetting("ical_port"));
	ulPortICalS = atoi(g_lpConfig->GetSetting("icals_port"));

	// start listening on normal port
	if (bListen) {
		hr = HrListen(g_lpLogger, g_lpConfig->GetSetting("server_bind"), ulPortICal, &ulNormalSocket);
		if (hr != hrSuccess) {
			g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Could not listen on port %d. (0x%08X)", ulPortICal, hr);
			bListen = false;
		} else {
			g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Listening on port %d.", ulPortICal);
		}
	}

	// start listening on secure port
	if (bListenSecure) {
		hr = ECChannel::HrSetCtx(g_lpConfig, g_lpLogger);
		if (hr == hrSuccess) {
			hr = HrListen(g_lpLogger, g_lpConfig->GetSetting("server_bind"), ulPortICalS, &ulSecureSocket);
			if (hr != hrSuccess) {
				g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Could not listen on secure port %d. (0x%08X)", ulPortICalS, hr);
				bListenSecure = false;
			}
			g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Listening on secure port %d.", ulPortICalS);
		} else {
			g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Could not listen on secure port %d. (0x%08X)", ulPortICalS, hr);
			bListenSecure = false;
		}
	}

	if (!bListen && !bListenSecure) {
		g_lpLogger->Log(EC_LOGLEVEL_FATAL, "No ports have been opened for listening, exiting.");
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	hr = hrSuccess;
	*lpulNormal = ulNormalSocket;
	*lpulSecure = ulSecureSocket;

exit:
	return hr;
}

/**
 * Listen to the passed sockets and calls HrStartHandlerClient for
 * every incoming connection.
 *
 * @param[in]	ulNormalSocket	Listening socket of incoming HTTP connections
 * @param[in]	ulSecureSocket	Listening socket of incoming HTTPS connections
 * @retval MAPI error code
 */
HRESULT HrProcessConnections(int ulNormalSocket, int ulSecureSocket)
{
	HRESULT hr = hrSuccess;
	fd_set readfds = {{0}};
	int err = 0;
	bool bUseSSL;
	struct timeval timeout = {0};
	ECChannel *lpChannel = NULL;
	int nCloseFDs = 0;
	int pCloseFDs[2] = {0};

	if (ulNormalSocket)
		pCloseFDs[nCloseFDs++] = ulNormalSocket;
	if (ulSecureSocket)
		pCloseFDs[nCloseFDs++] = ulSecureSocket;

	// main program loop
	while (!g_bQuit) {
		FD_ZERO(&readfds);
		if (ulNormalSocket)
			FD_SET(ulNormalSocket, &readfds);
		if (ulSecureSocket)
			FD_SET(ulSecureSocket, &readfds);

		timeout.tv_sec = 10;
		timeout.tv_usec = 0;

		// Check whether there are incoming connections.
		err = select(max(ulNormalSocket, ulSecureSocket) + 1, &readfds, NULL, NULL, &timeout);
		if (err < 0) {
			if (errno != EINTR) {
				g_lpLogger->Log(EC_LOGLEVEL_FATAL, "An unknown socket error has occurred.");
				g_bQuit = true;
				hr = MAPI_E_NETWORK_ERROR;
			}
			continue;
		} else if (err == 0) {
			continue;
		}

		if (g_bQuit) {
			hr = hrSuccess;
			break;
		}

		// Check if a normal connection is waiting.
		if (ulNormalSocket && FD_ISSET(ulNormalSocket, &readfds)) {
			g_lpLogger->Log(EC_LOGLEVEL_INFO, "Connection waiting on port %d.", atoi(g_lpConfig->GetSetting("ical_port")));
			bUseSSL = false;
			hr = HrAccept(g_lpLogger, ulNormalSocket, &lpChannel);
			if (hr != hrSuccess) {
				g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Could not accept incoming connection on port %d. (0x%08X)", atoi(g_lpConfig->GetSetting("ical_port")), hr);
				continue;
			}
		// Check if a secure connection is waiting.
		} else if (ulSecureSocket && FD_ISSET(ulSecureSocket, &readfds)) {
			g_lpLogger->Log(EC_LOGLEVEL_INFO, "Connection waiting on secure port %d.", atoi(g_lpConfig->GetSetting("icals_port")));
			bUseSSL = true;
			hr = HrAccept(g_lpLogger, ulSecureSocket, &lpChannel);
			if (hr != hrSuccess) {
				g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Could not accept incoming secure connection on port %d. (0x%08X)", atoi(g_lpConfig->GetSetting("ical_port")), hr);
				continue;
			}
		} else {
			continue;
		}

		hr = HrStartHandlerClient(lpChannel, bUseSSL, nCloseFDs, pCloseFDs);
		if (hr != hrSuccess) {
			delete lpChannel;	// destructor closes sockets
			g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Handling client connection failed. (0x%08X)", hr);
			continue;
		}
		if (g_bThreads == false)
			delete lpChannel;	// always cleanup channel in main process
	}

	return hr;
}

/**
 * Starts a new thread or forks a child to process the incoming
 * connection.
 *
 * @param[in]	lpChannel	The accepted connection in ECChannel object
 * @param[in]	bUseSSL		The ECChannel object is an SSL connection
 * @param[in]	nCloseFDs	Number of FDs in pCloseFDs, used on forks only
 * @param[in]	pCloseFDs	Array of FDs to close in child process
 * @retval E_FAIL when thread of child did not start
 */
HRESULT HrStartHandlerClient(ECChannel *lpChannel, bool bUseSSL, int nCloseFDs, int *pCloseFDs)
{
	HRESULT hr = hrSuccess;
	pthread_attr_t pThreadAttr;
	pthread_t pThread;
	HandlerArgs *lpHandlerArgs = new HandlerArgs;

	lpHandlerArgs->lpChannel = lpChannel;
	lpHandlerArgs->bUseSSL = bUseSSL;

	if (g_bThreads) {
		pthread_attr_init(&pThreadAttr);

		if (pthread_attr_setdetachstate(&pThreadAttr, PTHREAD_CREATE_DETACHED) != 0) {
			g_lpLogger->Log(EC_LOGLEVEL_WARNING, "Could not set thread attribute to detached.");
		}

		if (pthread_create(&pThread, &pThreadAttr, HandlerClient, lpHandlerArgs) != 0) {
			g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Could not create thread.");
			hr = E_FAIL;
			goto exit;
		}
	} else {
		if (unix_fork_function(HandlerClient, lpHandlerArgs, nCloseFDs, pCloseFDs) < 0) {
			g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Could not create process.");
			hr = E_FAIL;
			goto exit;
		}
		nChildren++;
	}

exit:
	if (hr != hrSuccess && lpHandlerArgs)
		delete lpHandlerArgs;

	return hr;
}

void *HandlerClient(void *lpArg)
{
	HRESULT hr = hrSuccess;
	HandlerArgs *lpHandlerArgs = (HandlerArgs *) lpArg;
	ECChannel *lpChannel = lpHandlerArgs->lpChannel;
	bool bUseSSL = lpHandlerArgs->bUseSSL;	

	delete lpHandlerArgs;

	if (bUseSSL) {
		if (lpChannel->HrEnableTLS() != hrSuccess) {
			g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Unable to negotiate SSL connection");
			goto exit;
		}
    }

	while(1)
	{
		hr = lpChannel->HrSelect(KEEP_ALIVE_TIME);
		if (hr != hrSuccess) {
			g_lpLogger->Log(EC_LOGLEVEL_INFO, "Request timeout, closing connection");
			break;
		}

		//Save mapi session between Requests
		hr = HrHandleRequest(lpChannel);
		if (hr != hrSuccess)
			break;
	}

exit:
	g_lpLogger->Log(EC_LOGLEVEL_INFO, "Connection closed");
		
	if(lpChannel)
		delete lpChannel;

	return NULL;
}

HRESULT HrHandleRequest(ECChannel *lpChannel)
{
	HRESULT hr = hrSuccess;
	std::wstring wstrUser;
	std::wstring wstrPass;
	std::string strUrl;
	std::string strMethod;
	std::string strServerTZ = g_lpConfig->GetSetting("server_timezone");
	std::string strCharset;
	Http *lpRequest = new Http(lpChannel, g_lpLogger, g_lpConfig);
	ProtocolBase *lpBase = NULL;
	IMAPISession *lpSession = NULL;
	ULONG ulFlag = 0;


	g_lpLogger->Log(EC_LOGLEVEL_DEBUG, "New Request");

	hr = lpRequest->HrReadHeaders();
	if(hr != hrSuccess) {
		hr = MAPI_E_USER_CANCEL; // connection is closed by client no data to be read
		goto exit;
	}

	hr = lpRequest->HrValidateReq();
	if(hr != hrSuccess) {
		lpRequest->HrResponseHeader(501, "Not Implemented");
		lpRequest->HrResponseBody("\nRequest not implemented");
		goto exit;
	}

	// ignore Empty Body
	lpRequest->HrReadBody();

	// no error, defaults to utf-8
	lpRequest->HrGetCharSet(&strCharset);
	// ignore Empty User field.
	lpRequest->HrGetUser(&wstrUser);
	// ignore Empty Password field
	lpRequest->HrGetPass(&wstrPass);
	// no checks required as HrValidateReq() checks Method
	lpRequest->HrGetMethod(&strMethod);
	// 
	lpRequest->HrSetKeepAlive(KEEP_ALIVE_TIME);

	hr = lpRequest->HrGetUrl(&strUrl);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Url is empty for method : %s",strMethod.c_str());
		lpRequest->HrResponseHeader(400,"Bad Request");
		lpRequest->HrResponseBody("Bad Request");
		goto exit;
	}

	hr = HrParseURL(strUrl, &ulFlag);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Client request is invalid: 0x%08X", hr);
		lpRequest->HrResponseHeader(400, "Bad Request: " + stringify(hr,true));
		goto exit;
	}

	if (ulFlag & SERVICE_CALDAV)
		// this header is always present in a caldav response, but not in ical.
		lpRequest->HrResponseHeader("DAV", "1, access-control, calendar-access, calendar-schedule, calendarserver-principal-property-search");

	if(!strMethod.compare("OPTIONS"))
	{
		lpRequest->HrResponseHeader(200, "OK");
		// @todo, if ical get is disabled, do not add GET as allowed option
		// @todo, should check write access on url and only return read actions if not available
		lpRequest->HrResponseHeader("Allow", "OPTIONS, GET, POST, PUT, DELETE, MOVE");
		lpRequest->HrResponseHeader("Allow", "PROPFIND, PROPPATCH, REPORT, MKCALENDAR");
		// most clients do not login with this action, no need to complain.
		hr = hrSuccess;
		goto exit;
	}
	
	if (wstrUser.empty() || wstrPass.empty()) {
		g_lpLogger->Log(EC_LOGLEVEL_INFO, "Sending authentication request");
		hr = MAPI_E_CALL_FAILED;
	} else {
		hr = HrAuthenticate(wstrUser, wstrPass, g_lpConfig->GetSetting("server_socket"), &lpSession);
		if (hr != hrSuccess)
			g_lpLogger->Log(EC_LOGLEVEL_WARNING, "Login failed (0x%08X), resending authentication request", hr);
	}
	if (hr != hrSuccess) {
		if(ulFlag & SERVICE_ICAL)
			lpRequest->HrRequestAuth("Zarafa iCal Gateway");
		else
			lpRequest->HrRequestAuth("Zarafa CalDav Gateway");
		hr = hrSuccess; //keep connection open.
		goto exit;
	}

	//GET & ical Requests
	// @todo fix caldav GET request
	if( !strMethod.compare("GET") || !strMethod.compare("HEAD") || ((ulFlag & SERVICE_ICAL) && strMethod.compare("PROPFIND")) )
	{
		lpBase = new iCal(lpRequest, lpSession, g_lpLogger, strServerTZ, strCharset);
	}
	//CALDAV Requests
	else if((ulFlag & SERVICE_CALDAV) || ( !strMethod.compare("PROPFIND") && !(ulFlag & SERVICE_ICAL)))
	{
		lpBase = new CalDAV(lpRequest, lpSession, g_lpLogger, strServerTZ, strCharset);		
	} 
	else
	{
		hr = MAPI_E_CALL_FAILED;
		lpRequest->HrResponseHeader(404, "Request not valid for ical or caldav services");
		goto exit;
	}

	hr = lpBase->HrInitializeClass();
	if (hr != hrSuccess) {
		if (hr != MAPI_E_NOT_ME)
			hr = lpRequest->HrToHTTPCode(hr);
		goto exit;
	}

	hr = lpBase->HrHandleCommand(strMethod);

exit:
	if(hr != hrSuccess && !strMethod.empty() && hr != MAPI_E_NOT_ME)
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Error processing %s request, error code 0x%08x", strMethod.c_str(), hr);

	if ( lpRequest && hr != MAPI_E_USER_CANCEL ) // do not send response to client if connection closed by client.
		hr = lpRequest->HrFinalize();

	g_lpLogger->Log(EC_LOGLEVEL_DEBUG, "End Of Request");

	if(lpRequest)
		delete lpRequest;
	
	if(lpSession)
		// do not keep the session alive, can receive different (or missing) Auth headers in keep-open requests!
		lpSession->Release();

	if(lpBase)
		delete lpBase;

	return hr;
}
