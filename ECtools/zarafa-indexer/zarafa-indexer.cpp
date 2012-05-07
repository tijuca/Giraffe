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

// zarafa-indexer.cpp : Defines the entry point for the console application.
//

#include "platform.h"


#include <execinfo.h>
#include <dirent.h>
#include <sys/stat.h>

#include <CLucene/CLConfig.h>
#include <CLucene.h>
#include <cctype>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <sstream>

#include <my_getopt.h>

#include <CommonUtil.h>
#include <ecversion.h>
#include <UnixUtil.h>
#include <stringutil.h>
#include <ECChannel.h>
#include <md5.h>

#include "ECFileIndex.h"
#include "ECIndexer.h"
#include "ECLucene.h"
#include "ECSearcher.h"
#include "zarafa-indexer.h"

#include <boost/algorithm/string.hpp>
namespace ba = boost::algorithm;

ECThreadData*		g_lpThreadData;

pthread_mutex_t     g_hExitMutex;
pthread_cond_t      g_hExitSignal;
pthread_t			g_hMainThread;

void sigusr2(int sig)
{
	if (pthread_equal(pthread_self(), g_hMainThread)==0)
		return;

	if (!g_lpThreadData)
		return;

	g_lpThreadData->lpLucene->Optimize(TRUE);
	g_lpThreadData->lpLogger->Log(EC_LOGLEVEL_WARNING, "All caches flushed");
}

void sighandle(int sig)
{
	// Win32 has unix semantics and therefore requires us to reset the signal handler.
	signal(SIGTERM , sighandle);
	signal(SIGINT  , sighandle);	// CTRL+C

	if (g_lpThreadData) {
		pthread_mutex_lock(&g_hExitMutex);
		if (g_lpThreadData->bShutdown == false)// do not log multimple shutdown messages
			g_lpThreadData->lpLogger->Log(EC_LOGLEVEL_FATAL, "Termination requested, shutting down.");
		g_lpThreadData->bShutdown = true;
		pthread_cond_broadcast(&g_hExitSignal);
		pthread_mutex_unlock(&g_hExitMutex);
	}
}

void sighup(int signr)
{
	// In Win32, the signal is sent in a seperate, special signal thread. So this test is
	// not needed or required.
	if (pthread_equal(pthread_self(), g_hMainThread) == 0)
		return;

	if (!g_lpThreadData)
		return;

	if (g_lpThreadData->lpConfig) {
		if (!g_lpThreadData->lpConfig->ReloadSettings()) {
			if (g_lpThreadData->lpLogger)
				g_lpThreadData->lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to reload configuration file, continuing with current settings.");
		} else
			g_lpThreadData->ReloadConfigOptions();
	}

	if (g_lpThreadData->lpLogger) {
		if (g_lpThreadData->lpConfig) {
			const char *ll = g_lpThreadData->lpConfig->GetSetting("log_level");
			int new_ll = ll ? atoi(ll) : 2;
			g_lpThreadData->lpLogger->SetLoglevel(new_ll);
		}

		g_lpThreadData->lpLogger->Reset();
		g_lpThreadData->lpLogger->Log(EC_LOGLEVEL_WARNING, "Log connection was reset");
	}
}

// SIGSEGV catcher

void sigsegv(int signr)
{
	void *bt[64];
	int i, n;
	char **btsymbols;

	if(!g_lpThreadData || !g_lpThreadData->lpLogger)
		goto exit;

	switch (signr) {
	case SIGSEGV:
		g_lpThreadData->lpLogger->Log(EC_LOGLEVEL_FATAL, "Caught SIGSEGV (%d), traceback:", signr);
		break;
	case SIGBUS:
		g_lpThreadData->lpLogger->Log(EC_LOGLEVEL_FATAL, "Caught SIGBUS (%d), possible invalid mapped memory access, traceback:", signr);
		break;
	case SIGABRT:
		g_lpThreadData->lpLogger->Log(EC_LOGLEVEL_FATAL, "Caught SIGABRT (%d), out of memory or unhandled exception, traceback:", signr);
		break;
	};

	n = backtrace(bt, 64);

	btsymbols = backtrace_symbols(bt, n);

	for (i = 0; i < n; i++) {
		if (btsymbols)
			g_lpThreadData->lpLogger->Log(EC_LOGLEVEL_FATAL, "%016p %s", bt[i], btsymbols[i]);
		else
			g_lpThreadData->lpLogger->Log(EC_LOGLEVEL_FATAL, "%016p", bt[i]);
	}

	g_lpThreadData->lpLogger->Log(EC_LOGLEVEL_FATAL, "When reporting this traceback, please include Linux distribution name, system architecture and Zarafa version.");

exit:
	kill(0, signr);
}

ECThreadData::ECThreadData()
	: lpLogger(NULL), lpConfig(NULL), lpFileIndex(NULL), lpLucene(NULL), bShutdown(FALSE),
	  m_strCommand(), m_ulAttachMaxSize(0), m_ulParserMaxMemory(0), m_ulParserMaxCpuTime(0)
{
}

ECThreadData::~ECThreadData()
{
	if (lpLucene)
		delete lpLucene;
	if (lpFileIndex)
		delete lpFileIndex;
	if (lpLogger)
		lpLogger->Release();
	if (lpConfig)
		delete lpConfig;
}

void ECThreadData::ReloadConfigOptions()
{
	const char *lpszConfig;

	if (!lpConfig)
		return;

	lpszConfig = lpConfig->GetSetting("index_attachment_max_size");
	if (lpszConfig)
		m_ulAttachMaxSize = atoi(lpszConfig) * 1024;
	if (!m_ulAttachMaxSize)
		m_ulAttachMaxSize = 5 * 1024 * 1024;

	lpszConfig = lpConfig->GetSetting("index_attachment_parser");
	if (lpszConfig)
		m_strCommand = lpszConfig;

	lpszConfig = lpConfig->GetSetting("index_attachment_parser_max_memory");
	if (lpszConfig)
		m_ulParserMaxMemory = atoll(lpszConfig);
	if (!m_ulParserMaxMemory)
		m_ulParserMaxMemory = RLIM_INFINITY;

	lpszConfig = lpConfig->GetSetting("index_attachment_parser_max_cputime");
	if (lpszConfig)
		m_ulParserMaxCpuTime = atoll(lpszConfig);
	if (!m_ulParserMaxCpuTime)
		m_ulParserMaxCpuTime = RLIM_INFINITY;

	lpszConfig = lpConfig->GetSetting("index_attachment_mime_filter");
	if (lpszConfig)
		ba::split(m_setMimeFilter, lpszConfig, ba::is_any_of("\t "), ba::token_compress_on);

	lpszConfig = lpConfig->GetSetting("index_attachment_extension_filter");
	if (lpszConfig)
		ba::split(m_setExtFilter, lpszConfig, ba::is_any_of("\t "), ba::token_compress_on);
}

HRESULT OpenSearchSocket(int *lpulSocket, bool *lpbUseSsl)
{
	HRESULT hr = hrSuccess;
	const char *lpPath = g_lpThreadData->lpConfig->GetSetting("server_bind_name");
	std::string strName = GetServerNameFromPath(lpPath);
	unsigned int ulPort = atoi(GetServerPortFromPath(lpPath).c_str());
	bool bUseSsl = false;
	int ulSocket = 0;

	if ((strncmp(lpPath, "file", 4) == 0) || (lpPath[0] == PATH_SEPARATOR)) {
		hr = HrListen(g_lpThreadData->lpLogger, strName.c_str(), &ulSocket);
		if (hr != hrSuccess)
			goto exit;
	} else {
		bUseSsl = !!(strncmp(lpPath, "https", 5) == 0);
		if (bUseSsl) {
			hr = ECChannel::HrSetCtx(g_lpThreadData->lpConfig, g_lpThreadData->lpLogger);
			if (hr != hrSuccess)
				goto exit;
		}

		hr = HrListen(g_lpThreadData->lpLogger, strName.c_str(), ulPort, &ulSocket);
		if (hr != hrSuccess)
			goto exit;
	}

	*lpulSocket = ulSocket;
	*lpbUseSsl = bUseSsl;

exit:
	return hr;
}

HRESULT running_service(const char *szPath, int ulSearchSocket, bool bUseSsl)
{
	HRESULT			hr = hrSuccess;
	ECIndexer*		lpIndexer = NULL;
	ECSearcher*		lpSearcher = NULL;

	pthread_mutex_init(&g_hExitMutex, NULL);
	pthread_cond_init(&g_hExitSignal, NULL);
	g_hMainThread = pthread_self();

	g_lpThreadData->lpLogger->Log(EC_LOGLEVEL_FATAL, "Starting zarafa-indexer version " PROJECT_VERSION_INDEXER_STR " (" PROJECT_SVN_REV_STR "), pid %d", getpid());

	/*
	 * Set the maxClauses value to something more useful
	 *
	 * The reason we need this is that apparently clucene is rather insanely expanding prefix
	 * queries like 'sender: a*' into a large OR with all the possible values like 'sender: aardvark OR
	 * sender: apple'. This quickly makes large queries which are normally limited to 1024 clauses.
	 */

	lucene::search::BooleanQuery::setMaxClauseCount(atoui(g_lpThreadData->lpConfig->GetSetting("index_max_clauses")));

	hr = ECIndexer::Create(g_lpThreadData, &lpIndexer);
	if (hr != hrSuccess)
		goto exit;

	hr = ECSearcher::Create(g_lpThreadData, lpIndexer, ulSearchSocket, bUseSsl, &lpSearcher);
	if (hr != hrSuccess)
		goto exit;

	pthread_mutex_lock(&g_hExitMutex);
	if (!g_lpThreadData->bShutdown)
		pthread_cond_wait(&g_hExitSignal, &g_hExitMutex);
	pthread_mutex_unlock(&g_hExitMutex);

exit:
	g_lpThreadData->lpLogger->Log(EC_LOGLEVEL_FATAL, "Stopping Zarafa indexer...");

	pthread_cond_destroy(&g_hExitSignal);
	pthread_mutex_destroy(&g_hExitMutex);

	if (lpIndexer)
		lpIndexer->Release();

	if (lpSearcher)
		lpSearcher->Release();

	return hr;
}

HRESULT clean_lock_files(bool bRemove)
{
	HRESULT hr = hrSuccess;
	char *path = NULL;
	DIR *dir = NULL;
	struct dirent *file = NULL;
	struct stat fileinfo;
	map<string, string> mapHashes;
	map<string, string>::iterator ihash;
	list<string> lstServers;
	vector<string> vTokens;
	string subdir;
	string fullpath;
	int s;

	// find all in /var/lib/zarafa/index/*/*
	path = g_lpThreadData->lpConfig->GetSetting("index_path");
	s = strlen(path) -1;
	if (path[s] == '/')
		path[s] = '\0';
	dir = opendir(path);
	if (!dir) {
		g_lpThreadData->lpLogger->Log(EC_LOGLEVEL_FATAL, "unable to open index_path");
		hr = MAPI_E_CALL_FAILED;
		goto exit;
	}
	while ((file = readdir(dir)) != NULL) {
		if (file->d_name[0] == '.') continue;
		fullpath = string(path) + "/" + file->d_name;
		stat(fullpath.c_str(), &fileinfo);
		if (! S_ISDIR(fileinfo.st_mode)) continue;
		lstServers.push_back(file->d_name);
	}
	closedir(dir);

	for (list<string>::iterator i = lstServers.begin(); i != lstServers.end(); i++) {
		subdir = string(path) + "/" + *i;
		dir = opendir(subdir.c_str());
		if (!dir)
			continue;
		while ((file = readdir(dir)) != NULL) {
			class MD5 hash;

			if (file->d_name[0] == '.') continue;
			stat(file->d_name, &fileinfo);
			if (! S_ISDIR(fileinfo.st_mode)) continue;

			fullpath = subdir + "/" + file->d_name + "/index";
			hash.update((unsigned char*)fullpath.c_str(), fullpath.length());
			hash.finalize();
			char *hex = hash.hex_digest();
			mapHashes.insert(pair<string,string>(hex, fullpath));
			delete [] hex;
		}
		closedir(dir);
	}

	path = getenv(LUCENE_LOCK_DIR_ENV_1);
	if (!path)
		path = getenv(LUCENE_LOCK_DIR_ENV_2);
	if (!path)
		path = LUCENE_LOCK_DIR_ENV_FALLBACK;

	dir = opendir(path);
	if (!dir)
		goto exit;

	while ((file = readdir(dir)) != NULL) {
		int lock_start = strlen(file->d_name) - strlen("-write.lock");
		// lucene-<hash>-write.lock
		// hash is md5sum of location + /index
		if (lock_start <= 0 || strncmp(file->d_name, "lucene-", strlen("lucene-")) != 0 || strcmp(file->d_name+lock_start, "-write.lock") != 0)
			continue;
		
		if (bRemove) {
		    std::string strFullPath = path;
			g_lpThreadData->lpLogger->Log(EC_LOGLEVEL_FATAL, "Removing lockfile: %s. User may receive wrong search results!", file->d_name);
			strFullPath += "/";
			strFullPath += (char *)file->d_name;
			unlink(strFullPath.c_str());
		}
		else if (hr == hrSuccess) {
			g_lpThreadData->lpLogger->Log(EC_LOGLEVEL_FATAL, "Lucene lockfiles found in %s. Some index databases may be corrupt and return invalid search results.", path);
			hr = MAPI_E_CALL_FAILED;
		}

		vTokens = tokenize(file->d_name, '-');
		ihash = mapHashes.find(vTokens[1]);
		if (ihash != mapHashes.end()) {
			g_lpThreadData->lpLogger->Log(EC_LOGLEVEL_FATAL, "Lockfile %s belongs to lucene database in %s", file->d_name, ihash->second.c_str());
		}
	}

exit:
	if (dir)
		closedir(dir);

	return hr;
}

HRESULT service_start(int argc, char *argv[], const char *lpszPath, bool daemonize)
{
	HRESULT hr = hrSuccess;

	bool bUseSsl = false;
	int ulSocket = 0;
	stack_t st;
	struct sigaction act;

	signal(SIGTERM, sighandle);
	signal(SIGINT, sighandle);

	signal(SIGHUP, sighup);
	signal(SIGUSR2, sigusr2);

	// Globally ignore SIGPIPE, which will often trigger due to attachment parser process
	// Since we login without notifications, the zarafa-client doesn't ignore them for us
	signal(SIGPIPE, SIG_IGN);

    // SIGSEGV backtrace support
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

	// remove lock files (hopefully none) if indexer wasn't shutdown properly
	clean_lock_files(parseBool(g_lpThreadData->lpConfig->GetSetting("cleanup_lockfiles")));
	if (hr != hrSuccess)
		goto exit;


	// listen before we daemonize and change to a different user
	hr = OpenSearchSocket(&ulSocket, &bUseSsl);
	if (hr != hrSuccess)
		goto exit;


	// Try to set max open file descriptors to 8192
	// non-optimized indexes can use a very high number of open files. When we would go over 1024 files,
	// lucene will crash, because of throws in a base constructor with pure virtual members. See ZCP-9260.
	struct rlimit file_limit;
#define OFLIMIT 8192
	file_limit.rlim_cur = OFLIMIT;
	file_limit.rlim_max = OFLIMIT;

	if (setrlimit(RLIMIT_NOFILE, &file_limit) < 0) {
		g_lpThreadData->lpLogger->Log(EC_LOGLEVEL_FATAL, "WARNING: setrlimit(RLIMIT_NOFILE, %d) failed. Crashes may occur when open files exceed current limit of %d.", OFLIMIT, getdtablesize());
		g_lpThreadData->lpLogger->Log(EC_LOGLEVEL_FATAL, "WARNING: Either start the process as root, or increase user limits for open file descriptors.");
	}

	// fork if needed and drop privileges as requested.
	// this must be done before we do anything with pthreads
	if (daemonize && unix_daemonize(g_lpThreadData->lpConfig, g_lpThreadData->lpLogger))
		goto exit;
	if (!daemonize)
		setsid();
	if (unix_create_pidfile(argv[0], g_lpThreadData->lpConfig, g_lpThreadData->lpLogger, false) < 0)
		goto exit;
	if (unix_runas(g_lpThreadData->lpConfig, g_lpThreadData->lpLogger))
		goto exit;

	hr = running_service(lpszPath, ulSocket, bUseSsl);
	if (hr != hrSuccess)
		goto exit;

exit:

	if (st.ss_sp)
		free(st.ss_sp);

	return hr;
}

void print_help(char *name) {
	cout << "Usage:\n" << endl;
	cout << name << " [-F] [-h|--host <serverpath>] [-c|--config <configfile>]" << endl;
	cout << "  -F\t\tDo not run in the background" << endl;
	cout << "  -h path\tUse alternate connect path (e.g. file:///var/run/socket).\n\t\tDefault: file:///var/run/zarafa" << endl;
	cout << "  -c filename\tUse alternate config file (e.g. /etc/zarafa-indexer.cfg)\n\t\tDefault: /etc/zarafa/indexer.cfg" << endl;
	cout << "  -V\t\tPrint version information" << endl;
	cout << endl;
}

#define INDEXER_DEFAULT_BIND	"file:///var/run/zarafa-indexer"
#define INDEXER_DEFAULT_CONFIG	"/etc/zarafa/indexer.cfg"
#define INDEXER_DEFAULT_LOGFILE	"/var/log/zarafa/indexer.log"
#define INDEXER_ATTACH_PARSER	"/etc/zarafa/indexerscripts/attachments_parser"

int main(int argc, char *argv[]) {

	HRESULT hr = hrSuccess;
	const char *szConfig = INDEXER_DEFAULT_CONFIG;
	const char *szPath = NULL;
	bool daemonize = true;

	// Default settings
	const configsetting_t lpDefaults[] = {
		/* Connection settings indexer -> server */
		{ "server_socket", CLIENT_ADMIN_SOCKET },
		/* Connection settings server -> indexer */
		{ "server_bind_name", INDEXER_DEFAULT_BIND },
		/* Linux process settings */
		{ "pid_file", "/var/run/zarafa-indexer.pid" },
		{ "run_as_user", "" },
		{ "run_as_group", "" },
		{ "running_path", "/" },
		/* Logging options */
		{ "log_method","file" },
		{ "log_file", INDEXER_DEFAULT_LOGFILE },
		{ "log_level", "2", CONFIGSETTING_RELOADABLE },
		{ "log_timestamp", "1" },
		/* SSL settings for indexer -> server */
		{ "sslkey_file", "" },
		{ "sslkey_pass", "", CONFIGSETTING_EXACT },
		/* SSL settings for server -> indexer */
		{ "ssl_private_key_file", "" },
		{ "ssl_certificate_file", "" },
		// unused, but maybe in the future?
		{ "ssl_verify_client", "no" },
		{ "ssl_verify_file", "" },
		{ "ssl_verify_path", "" },
		/* operational settings */
		{ "cleanup_lockfiles", "no" },
		{ "limit_results", "0", CONFIGSETTING_RELOADABLE },
		/* Indexer settings */
		{ "index_path", "/var/lib/zarafa/index/" },
		{ "index_sync_stream", "yes", CONFIGSETTING_UNUSED },
		{ "index_interval", "5" },
		{ "index_threads", "1", CONFIGSETTING_RELOADABLE },
		{ "index_max_field_length", "10000", CONFIGSETTING_RELOADABLE },
		{ "index_merge_factor", "10", CONFIGSETTING_RELOADABLE },
		{ "index_max_buffered_docs", "10", CONFIGSETTING_RELOADABLE },
		{ "index_min_merge_docs", "10", CONFIGSETTING_RELOADABLE },
		{ "index_max_merge_docs", "2147483647", CONFIGSETTING_RELOADABLE },
		{ "index_term_interval", "128", CONFIGSETTING_RELOADABLE },
		{ "index_cache_timeout", "0" },
		{ "index_attachments", "no", CONFIGSETTING_RELOADABLE },
		{ "index_attachment_max_size", "5120", CONFIGSETTING_RELOADABLE },
		{ "index_attachment_parser", INDEXER_ATTACH_PARSER, CONFIGSETTING_RELOADABLE },
		{ "index_attachment_parser_max_memory", "0", CONFIGSETTING_RELOADABLE },
		{ "index_attachment_parser_max_cputime", "0", CONFIGSETTING_RELOADABLE },
		{ "index_attachment_mime_filter", "", CONFIGSETTING_RELOADABLE },
		{ "index_attachment_extension_filter", "", CONFIGSETTING_RELOADABLE },
		{ "index_block_users", "" },
		{ "index_block_companies", "" },
		{ "index_allow_servers", "" },
		{ "index_max_clauses", "50000" },
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

	while (true) {
		char c = my_getopt_long(argc, argv, "c:h:iuFV", long_options, NULL);
		
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
		case 'i': // Install service
		case 'u': // Uninstall service
			break;
		case OPT_FOREGROUND:
		case 'F':
			daemonize = 0;
			break;
		case 'V':
			cout << "Product version:\t" <<  PROJECT_VERSION_INDEXER_STR << endl
				 << "File version:\t\t" << PROJECT_SVN_REV_STR << endl;
			return 1;
		case OPT_HELP:
		default:
			print_help(argv[0]);
			return 1;
		}
	}

	setlocale(LC_CTYPE, "");

	g_lpThreadData = new ECThreadData();

	g_lpThreadData->lpConfig = ECConfig::Create(lpDefaults);
	if (!g_lpThreadData->lpConfig->LoadSettings(szConfig) || g_lpThreadData->lpConfig->HasErrors()) {
		g_lpThreadData->lpLogger = new ECLogger_File(EC_LOGLEVEL_FATAL, 0, "-"); // create fatal logger without a timestamp to stderr
		LogConfigErrors(g_lpThreadData->lpConfig, g_lpThreadData->lpLogger);
		hr = E_FAIL;
		goto exit;
	}
	g_lpThreadData->ReloadConfigOptions();

	// setup logging
	g_lpThreadData->lpLogger = CreateLogger(g_lpThreadData->lpConfig, argv[0], "Zarafa-indexer");

	if (g_lpThreadData->lpConfig->HasWarnings())
		LogConfigErrors(g_lpThreadData->lpConfig, g_lpThreadData->lpLogger);

	g_lpThreadData->lpFileIndex = new ECFileIndex(g_lpThreadData);
	g_lpThreadData->lpLucene = new ECLucene(g_lpThreadData);

	g_lpThreadData->lpLogger->SetLogprefix(LP_TID);

	// set socket filename
	if (!szPath)
		szPath = g_lpThreadData->lpConfig->GetSetting("server_socket");

	hr = service_start(argc, argv, szPath, daemonize);

exit:
	if(g_lpThreadData)
		delete g_lpThreadData;

	return hr == hrSuccess ? 0 : 1;
}
