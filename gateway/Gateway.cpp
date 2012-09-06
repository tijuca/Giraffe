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
#include <signal.h>


#include <inetmapi.h>

#include <mapi.h>
#include <mapix.h>
#include <mapidefs.h>
#include <mapicode.h>
#include <mapiext.h>
#include <mapiguid.h>

#include <CommonUtil.h>
#include <stringutil.h>
#include <iostream>
#include <stdio.h>
#include <string>
#include <vector>
#include <algorithm>
#include <stdlib.h>
#include <errno.h>

#include "ECLogger.h"
#include "ECConfig.h"
#include "my_getopt.h"

#include "ECChannel.h"
#include "POP3.h"
#include "IMAP.h"
#include "ecversion.h"

#include "SSLUtil.h"
#include "stringutil.h"

#include "UnixUtil.h"

#if HAVE_ICU
#include "unicode/uclean.h"
#endif

/**
 * @defgroup gateway Gateway for IMAP and POP3 
 * @{
 */

#define max(x,y)	(x > y ? x : y)

int daemonize = 1;
int quit = 0;
bool bThreads = false;
char *szPath = NULL;
ECLogger *g_lpLogger = NULL;
ECConfig *g_lpConfig = NULL;
pthread_t mainthread;
int nChildren = 0;
std::string g_strHostString;

void sigterm(int s) {
	quit = 1;
}

void sighup(int sig) {
	// In Win32, the signal is sent in a seperate, special signal thread. So this test is
	// not needed or required.
	if (bThreads && pthread_equal(pthread_self(), mainthread)==0)
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

void sigchld(int)
{
	int stat;
	while (waitpid (-1, &stat, WNOHANG) > 0) nChildren--;
}

// SIGSEGV catcher
#include <execinfo.h>

void sigsegv(int signr)
{
	void *bt[64];
	int i, n;
	char **btsymbols;

	if(!g_lpLogger)
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

HRESULT running_service(char *szPath, char *servicename);

void print_help(char *name) {
	cout << "Usage:\n" << endl;
	cout << name << " [-F] [-h|--host <serverpath>] [-c|--config <configfile>]" << endl;
	cout << "  -F\t\tDo not run in the background" << endl;
	cout << "  -h path\tUse alternate connect path (e.g. file:///var/run/socket).\n\t\tDefault: file:///var/run/zarafa" << endl;
	cout << "  -V Print version info." << endl;
	cout << "  -c filename\tUse alternate config file (e.g. /etc/zarafa-gateway.cfg)\n\t\tDefault: /etc/zarafa/gateway.cfg" << endl;
	cout << endl;
	cout << "  --ignore-unknown-config-options\tStart even if the configuration file contains invalid config options" << endl;
	cout << endl;
}

enum serviceType { ST_POP3 = 0, ST_IMAP };

struct HandlerArgs {
	serviceType type;
	ECChannel *lpChannel;
	ECLogger *lpLogger;
	ECConfig *lpConfig;
	bool bUseSSL;
};

void *Handler(void *lpArg) {
	HandlerArgs *lpHandlerArgs = (HandlerArgs *) lpArg;
	ECChannel *lpChannel = lpHandlerArgs->lpChannel;
	ECLogger *lpLogger = lpHandlerArgs->lpLogger;
	ECConfig *lpConfig = lpHandlerArgs->lpConfig;
	bool bUseSSL = lpHandlerArgs->bUseSSL;

	// szPath is global, pointing to argv variable, or lpConfig variable
	ClientProto *client;
	if (lpHandlerArgs->type == ST_POP3)
		client = new POP3(szPath, lpChannel, lpLogger, lpConfig);
	else
		client = new IMAP(szPath, lpChannel, lpLogger, lpConfig);
	// not required anymore
	delete lpHandlerArgs;

	// make sure the pipe logger does not exit when this handler exits, but only frees the memory.
	if (dynamic_cast<ECLogger_Pipe*>(lpLogger) != NULL)
		dynamic_cast<ECLogger_Pipe*>(lpLogger)->Disown();

	std::string inBuffer;
	HRESULT hr;
	bool bQuit = false;
	int timeouts = 0;

	if (bUseSSL) {
		if (lpChannel->HrEnableTLS() != hrSuccess) {
			lpLogger->Log(EC_LOGLEVEL_ERROR, "Unable to negotiate SSL connection");
			goto exit;
		}
	}

	hr = client->HrSendGreeting(g_strHostString);
	if (hr != hrSuccess)
		goto exit;

	// Main command loop
	while (!bQuit && !quit) {
		// check for data
		hr = lpChannel->HrSelect(60);
		if (hr == MAPI_E_TIMEOUT) {
			timeouts++;
			if (timeouts < client->getTimeoutMinutes()) {
				// ignore select() timeout for 5 (POP3) or 30 (IMAP) minutes
				continue;
			}
			// close idle first, so we don't have a race condition with the channel
			client->HrCloseConnection("BYE Connection closed because of timeout");
			lpLogger->Log(EC_LOGLEVEL_ERROR, "Connection closed because of timeout");
			bQuit = true;
			break;
		} else if (hr == MAPI_E_NETWORK_ERROR) {
			lpLogger->Log(EC_LOGLEVEL_ERROR, "Socket error: %s", strerror(errno));
			bQuit = true;
			break;
		}

		timeouts = 0;

		inBuffer.clear();
		hr = lpChannel->HrReadLine(&inBuffer);
		if (hr != hrSuccess) {
			if (errno)
				lpLogger->Log(EC_LOGLEVEL_ERROR, "Failed to read line: %s", strerror(errno));
			else
				lpLogger->Log(EC_LOGLEVEL_ERROR, "Client disconnected");
			bQuit = true;
			break;
		}

		if (quit) {
			client->HrCloseConnection("BYE server shutting down");
			hr = MAPI_E_CALL_FAILED;
			bQuit = true;
			break;
		}

		if (client->isContinue()) {
			// we asked the client for more data, do not parse the buffer, but send it "to the previous command"
			// that last part is currently only HrCmdAuthenticate(), so no difficulties here.
			// also, PLAIN is the only supported auth method.
			hr = client->HrProcessContinue(inBuffer);
			// no matter what happens, we continue handling the connection.
			continue;
		}

		// Process IMAP command
		hr = client->HrProcessCommand(inBuffer);
		if (hr == MAPI_E_NETWORK_ERROR) {
			lpLogger->Log(EC_LOGLEVEL_ERROR, "Connection error.");
			bQuit = true;
		}
		if (hr == MAPI_E_END_OF_SESSION) {
			lpLogger->Log(EC_LOGLEVEL_ERROR, "Disconnecting client.");
			bQuit = true;
		}
	}

exit:
	lpLogger->Log(EC_LOGLEVEL_FATAL, "Client %s thread exiting", lpChannel->GetIPAddress().c_str());

	client->HrDone(false);	// HrDone does not send an error string to the client
	delete client;

	delete lpChannel;
	if (bThreads == false) {
		g_lpLogger->Release();
		delete g_lpConfig;
	}

	/** free ssl error data **/
	ERR_remove_state(0);

	if (bThreads) nChildren--;

	// Do not pthread_exit() because linuxthreads is broken and will not free any objects
	// pthread_exit(NULL);
	return NULL;
}

int main(int argc, char *argv[]) {
	HRESULT hr = hrSuccess;
	int c = 0;
	bool bIgnoreUnknownConfigOptions = false;

	ssl_threading_setup();

	const char *szConfig = ECConfig::GetDefaultPath("gateway.cfg");

	const configsetting_t lpDefaults[] = {
		{ "server_bind", "0.0.0.0" },
		{ "run_as_user", "" },
		{ "run_as_group", "" },
		{ "pid_file", "/var/run/zarafa-gateway.pid" },
		{ "running_path", "/" },
		{ "process_model", "fork" },
		{ "pop3_enable", "yes" },
		{ "pop3_port", "110" },
		{ "pop3s_enable", "no" },
		{ "pop3s_port", "995" },
		{ "imap_enable", "yes" },
		{ "imap_port", "143" },
		{ "imaps_enable", "no" },
		{ "imaps_port", "993" },
		{ "imap_only_mailfolders", "yes", CONFIGSETTING_RELOADABLE },
		{ "imap_public_folders", "yes", CONFIGSETTING_RELOADABLE },
		{ "imap_capability_idle", "yes", CONFIGSETTING_RELOADABLE },
		{ "imap_always_generate", "no", CONFIGSETTING_UNUSED },
		{ "imap_max_messagesize", "128M", CONFIGSETTING_RELOADABLE | CONFIGSETTING_SIZE },
		{ "imap_generate_utf8", "no", CONFIGSETTING_RELOADABLE },
		{ "imap_expunge_on_delete", "no", CONFIGSETTING_RELOADABLE },
		{ "imap_store_rfc822", "yes", CONFIGSETTING_RELOADABLE },
		{ "server_socket", "http://localhost:236/zarafa" },
		{ "server_hostname", "" },
		{ "server_hostname_greeting", "no", CONFIGSETTING_RELOADABLE },
		{ "ssl_private_key_file", "/etc/zarafa/gateway/privkey.pem" },
		{ "ssl_certificate_file", "/etc/zarafa/gateway/cert.pem" },
		{ "ssl_verify_client", "no" },
		{ "ssl_verify_file", "" },
		{ "ssl_verify_path", "" },
		{ "log_method", "file" },
		{ "log_file", "-" },
		{ "log_level", "2", CONFIGSETTING_RELOADABLE },
		{ "log_timestamp", "1" },
		{ NULL, NULL },
	};
	enum {
		OPT_HELP,
		OPT_HOST,
		OPT_CONFIG,
		OPT_FOREGROUND,
		OPT_IGNORE_UNKNOWN_CONFIG_OPTIONS
	};
	struct option long_options[] = {
		{"help", 0, NULL, OPT_HELP},
		{"host", 1, NULL, OPT_HOST},
		{"config", 1, NULL, OPT_CONFIG},
		{"foreground", 1, NULL, OPT_FOREGROUND},
		{ "ignore-unknown-config-options", 0, NULL, OPT_IGNORE_UNKNOWN_CONFIG_OPTIONS },
		{NULL, 0, NULL, 0}
	};

	// Get commandline options
	while (1) {
		c = my_getopt_long_permissive(argc, argv, "c:h:iuFV", long_options, NULL);

		if (c == -1)
			break;

		switch (c) {
		case OPT_CONFIG:
		case 'c':
			szConfig = my_optarg;
			break;
		case OPT_HOST:
		case 'h':
			szPath = my_optarg;
			break;
		case 'i':				// Install service
		case 'u':				// Uninstall service
			break;
		case OPT_FOREGROUND:
		case 'F':
			daemonize = 0;
			break;
		case OPT_IGNORE_UNKNOWN_CONFIG_OPTIONS:
			bIgnoreUnknownConfigOptions = true;
			break;
		case 'V':
			cout << "Product version:\t" <<  PROJECT_VERSION_GATEWAY_STR << endl
				 << "File version:\t\t" << PROJECT_SVN_REV_STR << endl;
			return 1;
		case OPT_HELP:
		default:
			print_help(argv[0]);
			return 1;
		}
	}
	// Setup config
	g_lpConfig = ECConfig::Create(lpDefaults);
	if (!g_lpConfig->LoadSettings(szConfig) || !g_lpConfig->ParseParams(argc-my_optind, &argv[my_optind], NULL) || (!bIgnoreUnknownConfigOptions && g_lpConfig->HasErrors())) {
		g_lpLogger = new ECLogger_File(EC_LOGLEVEL_FATAL, 0, "-");	// create fatal logger without a timestamp to stderr
		LogConfigErrors(g_lpConfig, g_lpLogger);
		hr = E_FAIL;
		goto exit;
	}
	// Setup logging
	g_lpLogger = CreateLogger(g_lpConfig, argv[0], "ZarafaGateway");

	if ((bIgnoreUnknownConfigOptions && g_lpConfig->HasErrors()) || g_lpConfig->HasWarnings())
		LogConfigErrors(g_lpConfig, g_lpLogger);

	if (strncmp(g_lpConfig->GetSetting("process_model"), "thread", strlen("thread")) == 0) {
		bThreads = true;
		g_lpLogger->SetLogprefix(LP_TID);
	}

	if (bThreads)
		mainthread = pthread_self();

	if (!szPath)
		szPath = g_lpConfig->GetSetting("server_socket");


	g_strHostString = g_lpConfig->GetSetting("server_hostname", NULL, "");
	if (g_strHostString.empty())
		g_strHostString = GetServerFQDN();
	g_strHostString = string(" on ") + g_strHostString;

	hr = running_service(szPath, argv[0]);

exit:
    ssl_threading_cleanup();
    
	if (g_lpConfig)
		delete g_lpConfig;

	DeleteLogger(g_lpLogger);

	return hr == hrSuccess ? 0 : 1;
}

/**
 * Runs the gateway service, starting a new thread or fork child for
 * incoming connections on any configured service.
 *
 * @param[in]	szPath		Unused, should be removed.
 * @param[in]	servicename	Name of the service, used to create a unix pidfile.
 */
HRESULT running_service(char *szPath, char *servicename) {
	HRESULT hr = hrSuccess;
	int ulListenPOP3 = 0, ulListenPOP3s = 0;
	int ulListenIMAP = 0, ulListenIMAPs = 0;
	bool bListenPOP3, bListenPOP3s;
	bool bListenIMAP, bListenIMAPs;
	int nCloseFDs = 0, pCloseFDs[4] = {0};
	fd_set readfds;
	int err = 0;
	pthread_attr_t ThreadAttr;

	// SIGSEGV backtrace support
	stack_t st;
	struct sigaction act;

	memset(&st, 0, sizeof(st));
	memset(&act, 0, sizeof(act));

	if (bThreads) {
		pthread_attr_init(&ThreadAttr);
		if (pthread_attr_setdetachstate(&ThreadAttr, PTHREAD_CREATE_DETACHED) != 0) {
			g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Could not set thread attribute to detached");
			goto exit;
		}
		// 1Mb of stack space per thread
		if (pthread_attr_setstacksize(&ThreadAttr, 1024 * 1024)) {
			g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Could not set thread stack size to 1Mb");
			goto exit;
		}
	}


	bListenPOP3 = (strcmp(g_lpConfig->GetSetting("pop3_enable"), "yes") == 0);
	bListenPOP3s = (strcmp(g_lpConfig->GetSetting("pop3s_enable"), "yes") == 0);
	bListenIMAP = (strcmp(g_lpConfig->GetSetting("imap_enable"), "yes") == 0);
	bListenIMAPs = (strcmp(g_lpConfig->GetSetting("imaps_enable"), "yes") == 0);


	// Setup ssl context
	if ((bListenPOP3s || bListenIMAPs) && ECChannel::HrSetCtx(g_lpConfig, g_lpLogger) != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Error loading SSL context, POP3S and IMAPS will be disabled");
		bListenPOP3s = false;
		bListenIMAPs = false;
	}
	
	if (!bListenPOP3 && !bListenPOP3s && !bListenIMAP && !bListenIMAPs) {
		g_lpLogger->Log(EC_LOGLEVEL_FATAL, "POP3, POP3S, IMAP and IMAPS are all four disabled");
		hr = E_FAIL;
		goto exit;
	}

	// Setup sockets
	if (bListenPOP3) {
		hr = HrListen(g_lpLogger, g_lpConfig->GetSetting("server_bind"), atoi(g_lpConfig->GetSetting("pop3_port")), &ulListenPOP3);
		if (hr != hrSuccess) {
			g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to listen on port %s", g_lpConfig->GetSetting("pop3_port"));
			hr = E_FAIL;
			goto exit;
		}
		g_lpLogger->Log(EC_LOGLEVEL_INFO, "Listening on port %s for POP3", g_lpConfig->GetSetting("pop3_port"));
		pCloseFDs[nCloseFDs++] = ulListenPOP3;
	}

	if (bListenPOP3s) {
		hr = HrListen(g_lpLogger, g_lpConfig->GetSetting("server_bind"), atoi(g_lpConfig->GetSetting("pop3s_port")), &ulListenPOP3s);
		if (hr != hrSuccess) {
			g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to listen on port %s", g_lpConfig->GetSetting("pop3s_port"));
			hr = E_FAIL;
			goto exit;
		}
		g_lpLogger->Log(EC_LOGLEVEL_INFO, "Listening on port %s for POP3s", g_lpConfig->GetSetting("pop3s_port"));
		pCloseFDs[nCloseFDs++] = ulListenPOP3s;
	}

	if (bListenIMAP) {
		hr = HrListen(g_lpLogger, g_lpConfig->GetSetting("server_bind"), atoi(g_lpConfig->GetSetting("imap_port")), &ulListenIMAP);
		if (hr != hrSuccess) {
			g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to listen on port %s", g_lpConfig->GetSetting("imap_port"));
			hr = E_FAIL;
			goto exit;
		}
		g_lpLogger->Log(EC_LOGLEVEL_INFO, "Listening on port %s for IMAP", g_lpConfig->GetSetting("imap_port"));
		pCloseFDs[nCloseFDs++] = ulListenIMAP;
	}

	if (bListenIMAPs) {
		hr = HrListen(g_lpLogger, g_lpConfig->GetSetting("server_bind"), atoi(g_lpConfig->GetSetting("imaps_port")), &ulListenIMAPs);
		if (hr != hrSuccess) {
			g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to listen on port %s", g_lpConfig->GetSetting("imaps_port"));
			hr = E_FAIL;
			goto exit;
		}
		g_lpLogger->Log(EC_LOGLEVEL_INFO, "Listening on port %s for IMAPs", g_lpConfig->GetSetting("imaps_port"));
		pCloseFDs[nCloseFDs++] = ulListenIMAPs;
	}

	// Setup signals
	signal(SIGTERM, sigterm);
	signal(SIGINT, sigterm);
	signal(SIGHUP, sighup);
	signal(SIGCHLD, sigchld);
	signal(SIGPIPE, SIG_IGN);
  
    st.ss_sp = malloc(65536);
    st.ss_flags = 0;
    st.ss_size = 65536;
  
    act.sa_handler = sigsegv;
    act.sa_flags = SA_ONSTACK | SA_RESETHAND;
  
    sigaltstack(&st, NULL);
    sigaction(SIGSEGV, &act, NULL);
    sigaction(SIGBUS, &act, NULL);
    sigaction(SIGABRT, &act, NULL);

	// Setup MAPI
	hr = MAPIInitialize(NULL);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to initialize MAPI");
		goto exit;
	}
    // Set max open file descriptors to FD_SETSIZE .. higher than this number
    // is a bad idea, as it will start breaking select() calls.
    struct rlimit file_limit;
    file_limit.rlim_cur = FD_SETSIZE;
    file_limit.rlim_max = FD_SETSIZE;
    
    if(setrlimit(RLIMIT_NOFILE, &file_limit) < 0) {
        g_lpLogger->Log(EC_LOGLEVEL_FATAL, "WARNING: setrlimit(RLIMIT_NOFILE, %d) failed, you will only be able to connect up to %d sockets. Either start the process as root, or increase user limits for open file descriptors", FD_SETSIZE, getdtablesize());
    }        

	// fork if needed and drop privileges as requested.
	// this must be done before we do anything with pthreads
	if (daemonize && unix_daemonize(g_lpConfig, g_lpLogger))
		goto exit;
	if (!daemonize)
		setsid();
	unix_create_pidfile(servicename, g_lpConfig, g_lpLogger);
	if (unix_runas(g_lpConfig, g_lpLogger))
		goto exit;

	if (bThreads == false)
		g_lpLogger = StartLoggerProcess(g_lpConfig, g_lpLogger); // maybe replace logger

	g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Starting zarafa-gateway version " PROJECT_VERSION_GATEWAY_STR " (" PROJECT_SVN_REV_STR "), pid %d", getpid());

	// Mainloop
	while (!quit) {
		FD_ZERO(&readfds);
		if (bListenPOP3)
			FD_SET(ulListenPOP3, &readfds);
		if (bListenPOP3s)
			FD_SET(ulListenPOP3s, &readfds);
		if (bListenIMAP)
			FD_SET(ulListenIMAP, &readfds);
		if (bListenIMAPs)
			FD_SET(ulListenIMAPs, &readfds);

		struct timeval timeout;
		timeout.tv_sec = 10;
		timeout.tv_usec = 0;

		err = select(max(max(ulListenPOP3, ulListenIMAP), max(ulListenPOP3s, ulListenIMAPs)) + 1, &readfds, NULL, NULL, &timeout);
		if (err < 0) {
			if (errno != EINTR) {
				g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Socket error!");
				quit = 1;
				hr = MAPI_E_NETWORK_ERROR;
			}
			continue;
		} else if (err == 0) {
			continue;
		}

		// One socket has signalled a new incoming connection

		HandlerArgs *lpHandlerArgs = new HandlerArgs;

		lpHandlerArgs->lpLogger = g_lpLogger;
		lpHandlerArgs->lpConfig = g_lpConfig;

		if ((bListenPOP3 && FD_ISSET(ulListenPOP3, &readfds)) || (bListenPOP3s && FD_ISSET(ulListenPOP3s, &readfds))) {
			bool usessl;

			lpHandlerArgs->type = ST_POP3;

			// Incoming POP3(s) connection
			if (bListenPOP3s && FD_ISSET(ulListenPOP3s, &readfds)) {
				usessl = true;
				hr = HrAccept(g_lpLogger, ulListenPOP3s, &lpHandlerArgs->lpChannel);
			} else {
				usessl = false;
				hr = HrAccept(g_lpLogger, ulListenPOP3, &lpHandlerArgs->lpChannel);
			}
			if (hr != hrSuccess) {
				g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Unable to accept POP3 socket connection.");
				// just keep running
				delete lpHandlerArgs;
				hr = hrSuccess;
				continue;
			}

			lpHandlerArgs->bUseSSL = usessl;

			pthread_t POP3Thread;
			const char *method = usessl ? "POP3s" : "POP3";
			const char *model = bThreads ? "thread" : "process";
			g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Starting worker %s for %s request", model, method);
			if (bThreads) {
				if (pthread_create(&POP3Thread, &ThreadAttr, Handler, lpHandlerArgs) != 0) {
					g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Could not create %s %s.", method, model);
					// just keep running
					delete lpHandlerArgs->lpChannel;
					delete lpHandlerArgs;
					hr = hrSuccess;
				} else {
					nChildren++;
				}

			} else {
				if (unix_fork_function(Handler, lpHandlerArgs, nCloseFDs, pCloseFDs) < 0) {
					g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Could not create %s %s.", method, model);
					// just keep running
				} else
					nChildren++;
				// main handler always closes information it doesn't need
				delete lpHandlerArgs->lpChannel;
				delete lpHandlerArgs;
				hr = hrSuccess;
			}
			continue;
		}

		if ((bListenIMAP && FD_ISSET(ulListenIMAP, &readfds)) || (bListenIMAPs && FD_ISSET(ulListenIMAPs, &readfds))) {
			bool usessl;

			lpHandlerArgs->type = ST_IMAP;

			// Incoming IMAP(s) connection
			if (bListenIMAPs && FD_ISSET(ulListenIMAPs, &readfds)) {
				usessl = true;
				hr = HrAccept(g_lpLogger, ulListenIMAPs, &lpHandlerArgs->lpChannel);
			} else {
				usessl = false;
				hr = HrAccept(g_lpLogger, ulListenIMAP, &lpHandlerArgs->lpChannel);
			}
			if (hr != hrSuccess) {
				g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Unable to accept IMAP socket connection.");
				// just keep running
				delete lpHandlerArgs;
				hr = hrSuccess;
				continue;
			}

			lpHandlerArgs->bUseSSL = usessl;

			pthread_t IMAPThread;
			const char *method = usessl ? "IMAPs" : "IMAP";
			const char *model = bThreads ? "thread" : "process";
			g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Starting worker %s for %s request", model, method);
			if (bThreads) {
				if (pthread_create(&IMAPThread, &ThreadAttr, Handler, lpHandlerArgs) != 0) {
					g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Could not create %s %s.", method, model);
					// just keep running
					delete lpHandlerArgs->lpChannel;
					delete lpHandlerArgs;
					hr = hrSuccess;
				} else {
					nChildren++;
				}

			} else {
				if (unix_fork_function(Handler, lpHandlerArgs, nCloseFDs, pCloseFDs) < 0) {
					g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Could not create %s %s.", method, model);
					// just keep running
				} else
					nChildren++;
				// main handler always closes information it doesn't need
				delete lpHandlerArgs->lpChannel;
				delete lpHandlerArgs;
				hr = hrSuccess;
			}
			continue;
		}

		// should not be able to get here because of continues
		g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Incoming traffic was not for me??");
	}

	g_lpLogger->Log(EC_LOGLEVEL_FATAL, "POP3/IMAP Gateway will now exit");

	// in forked mode, send all children the exit signal
	if (bThreads == false) {
		signal(SIGTERM, SIG_IGN);
		kill(0, SIGTERM);
	}

	// wait max 10 seconds (init script waits 15 seconds)
	for(int i = 10; (nChildren && i); i--) {
		if (i % 5 == 0)
			g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Waiting for %d processes to exit", nChildren);
		sleep(1);
	}

	if (nChildren)
		g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Forced shutdown with %d procesess left", nChildren);
	else
		g_lpLogger->Log(EC_LOGLEVEL_FATAL, "POP3/IMAP Gateway shutdown complete");


	MAPIUninitialize();

exit:
	ECChannel::HrFreeCtx();

	SSL_library_cleanup(); // Remove ssl data for the main application and other related libraries

	if (bThreads)
		pthread_attr_destroy(&ThreadAttr);

	if(st.ss_sp)
		free(st.ss_sp);


#if HAVE_ICU
	// cleanup ICU data so valgrind is happy
	u_cleanup();
#endif

	return hr;
}

/** @} */
