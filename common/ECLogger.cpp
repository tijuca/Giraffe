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
#include "ECLogger.h"
#include <locale.h>
#include <stdarg.h>
#include <zlib.h>
#include <assert.h>
#include "stringutil.h"

#include "config.h"
#if HAVE_SYSLOG_H
#include <syslog.h>
#endif
#include <libgen.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

using namespace std;

ECLogger::ECLogger(int max_ll) {
	pthread_mutex_init(&msgbuflock, NULL);
	max_loglevel = max_ll;
	msgbuffer = new char[_LOG_BUFSIZE];
	// get system locale for time, NULL is returned if locale was not found.
	timelocale = createlocale(LC_TIME, "");
	prefix = LP_NONE;
	m_ulRef = 1;
}

ECLogger::~ECLogger() {
	delete [] msgbuffer;
	if (timelocale)
		freelocale(timelocale);

	pthread_mutex_destroy(&msgbuflock);
}

void ECLogger::SetLoglevel(int max_ll) {
	max_loglevel = max_ll;
}

char* ECLogger::MakeTimestamp() {
	time_t now = time(NULL);
	tm local;

	localtime_r(&now, &local);

	if (timelocale)
		strftime_l(timestring, _LOG_TSSIZE, "%c", &local, timelocale);
	else
		strftime(timestring, _LOG_TSSIZE, "%c", &local);

	return timestring;
}

bool ECLogger::Log(int loglevel) {
	return loglevel <= max_loglevel;
}

void ECLogger::SetLogprefix(logprefix lp)
{
	prefix = lp;
}

int ECLogger::GetFileDescriptor()
{
	return -1;
}

unsigned ECLogger::AddRef() {
	return ++m_ulRef;
}

unsigned ECLogger::Release() {
	unsigned ulRef = --m_ulRef;
	if (ulRef == 0)
		delete this;
	return ulRef;
}

ECLogger_Null::ECLogger_Null() : ECLogger(EC_LOGLEVEL_NONE) {}
ECLogger_Null::~ECLogger_Null() {}
void ECLogger_Null::Reset() {}
void ECLogger_Null::Log(int loglevel, const string &message) {}
void ECLogger_Null::Log(int loglevel, const char *format, ...) {}
void ECLogger_Null::LogVA(int loglevel, const char *format, va_list& va) {}

/**
 * ECLogger_File constructor
 *
 * @param[in]	max_ll			max loglevel passed to ECLogger
 * @param[in]	add_timestamp	1 if a timestamp before the logmessage is wanted
 * @param[in]	filename		filename of log in current locale
 */
ECLogger_File::ECLogger_File(int max_ll, int add_timestamp, const char *filename, bool compress) : ECLogger(max_ll) {
	pthread_mutex_init(&filelock, NULL);
	logname = strdup(filename);
	timestamp = add_timestamp;

	prevcount = 0;
	prevmsg.clear();

	if (strcmp(logname, "-") == 0) {
		log = stderr;
		fnOpen = NULL;
		fnClose = NULL;
		fnPrintf = (printf_func)&fprintf;
		fnFileno = (fileno_func)&fileno;
		fnFlush = (flush_func)&fflush;
		szMode = NULL;
	} else {
		if (compress) {
			fnOpen = &gzopen;
			fnClose = &gzclose;
			fnPrintf = &gzprintf;
			fnFileno = NULL;
			fnFlush = NULL;	// gzflush does exist, but degrades performance
			szMode = "wb";
		} else {
			fnOpen = (open_func)&fopen;
			fnClose = (close_func)&fclose;
			fnPrintf = (printf_func)&fprintf;
			fnFileno = (fileno_func)&fileno;
			fnFlush = (flush_func)&fflush;
			szMode = "a";
		}
	
		log = fnOpen(logname, szMode);
	}
}

ECLogger_File::~ECLogger_File() {
	if (prevcount > 1) {
		DoPrefix();
		fnPrintf(log, "Previous message logged %d times\n", prevcount);
	}
	if (log && fnClose)
		fnClose(log);
	pthread_mutex_destroy(&filelock);
	if (logname)
		free(logname);
}

void ECLogger_File::Reset() {
	if (log == stderr)
		return;

	pthread_mutex_lock(&filelock);
	if (log && fnClose)
		fnClose(log);
		
	assert(fnOpen);		
	log = fnOpen(logname, szMode);
	pthread_mutex_unlock(&filelock);
}

int ECLogger_File::GetFileDescriptor() {
	if (log && fnFileno)
		return fnFileno(log);
		
	return -1;
}

/**
 * Prints the optional timestamp and prefix to the log.
 */
void ECLogger_File::DoPrefix() {
	if (timestamp)
		fnPrintf(log, "%s: ", MakeTimestamp());
	if (prefix == LP_TID)
		fnPrintf(log, "[0x%08x] ", (unsigned int)pthread_self());
	else if (prefix == LP_PID)
		fnPrintf(log, "[%5d] ", getpid());
}

/**
 * Check if the ECLogger_File instance is logging to stderr.
 * @retval	true	This instance is logging to stderr.
 * @retval	false	This instance is not logging to stderr.
 */
bool ECLogger_File::IsStdErr() {
	return strcmp(logname, "-") == 0;
}

bool ECLogger_File::DupFilter(const std::string &message) {
	if (prevmsg == message) {
		prevcount++;
		if (prevcount < 100)
			return true;
	}
	if (prevcount > 1) {
		DoPrefix();
		fnPrintf(log, "Previous message logged %d times\n", prevcount);
	}
	prevmsg = message;
	prevcount = 0;
	return false;
}

void ECLogger_File::Log(int loglevel, const string &message) {
	if (!log)
		return;
	if (!ECLogger::Log(loglevel))
		return;

	pthread_mutex_lock(&filelock);
	if (!DupFilter(message)) {
		DoPrefix();
		fnPrintf(log, "%s\n", message.c_str());
		if (fnFlush)
			fnFlush(log);
	}
	pthread_mutex_unlock(&filelock);
}

void ECLogger_File::Log(int loglevel, const char *format, ...) {
	va_list va;

	if (!log)
		return;
	if (!ECLogger::Log(loglevel))
		return;

	va_start(va, format);
	LogVA(loglevel, format, va);
	va_end(va);
}

void ECLogger_File::LogVA(int loglevel, const char *format, va_list& va) {
	pthread_mutex_lock(&msgbuflock);
	_vsnprintf(msgbuffer, _LOG_BUFSIZE, format, va);

	pthread_mutex_lock(&filelock);

	if (!DupFilter(msgbuffer)) {
		DoPrefix();
		fnPrintf(log, "%s\n", msgbuffer);
		if (fnFlush)
			fnFlush(log);
	}
	pthread_mutex_unlock(&filelock);
	pthread_mutex_unlock(&msgbuflock);
}

ECLogger_Syslog::ECLogger_Syslog(int max_ll, const char *ident, int facility) : ECLogger(max_ll) {
	openlog(ident, LOG_PID, facility);
	levelmap[EC_LOGLEVEL_NONE] = LOG_DEBUG;
	levelmap[EC_LOGLEVEL_FATAL] = LOG_CRIT;
	levelmap[EC_LOGLEVEL_ERROR] = LOG_ERR;
	levelmap[EC_LOGLEVEL_WARNING] = LOG_WARNING;
	levelmap[EC_LOGLEVEL_NOTICE] = LOG_NOTICE;
	levelmap[EC_LOGLEVEL_INFO] = LOG_INFO;
	levelmap[EC_LOGLEVEL_DEBUG] = LOG_DEBUG;
}

ECLogger_Syslog::~ECLogger_Syslog() {
	closelog();
}

void ECLogger_Syslog::Reset() {
	// not needed.
}

void ECLogger_Syslog::Log(int loglevel, const string &message) {
	if (!ECLogger::Log(loglevel))
		return;
	syslog(levelmap[loglevel], "%s", message.c_str());
}

void ECLogger_Syslog::Log(int loglevel, const char *format, ...) {
	va_list va;

	if (!ECLogger::Log(loglevel))
		return;

	va_start(va, format);
	LogVA(loglevel, format, va);
	va_end(va);
}

void ECLogger_Syslog::LogVA(int loglevel, const char *format, va_list& va) {
#if HAVE_VSYSLOG
	vsyslog(levelmap[loglevel], format, va);
#else
	pthread_mutex_lock(&msgbuflock);
	_vsnprintf(msgbuffer, _LOG_BUFSIZE, format, va);
	syslog(levelmap[loglevel], "%s", msgbuffer);
	pthread_mutex_unlock(&msgbuflock);
#endif
}



/**
 * Consructor
 */
ECLogger_Tee::ECLogger_Tee(): ECLogger(EC_LOGLEVEL_DEBUG) {
}

/**
 * Destructor
 *
 * The destructor calls Release on each attached logger so
 * they'll be deleted if it was the last reference.
 */
ECLogger_Tee::~ECLogger_Tee() {
	LoggerList::iterator iLogger;

	for (iLogger = m_loggers.begin(); iLogger != m_loggers.end(); ++iLogger)
		(*iLogger)->Release();
}

/**
 * Reset all loggers attached to this logger.
 */
void ECLogger_Tee::Reset() {
	LoggerList::iterator iLogger;

	for (iLogger = m_loggers.begin(); iLogger != m_loggers.end(); ++iLogger)
		(*iLogger)->Reset();
}

/**
 * Check if anything would be logged with the requested loglevel.
 * Effectively this call is delegated to all attached loggers until
 * one logger is found that returns true.
 *
 * @param[in]	loglevel	The loglevel to test.
 *
 * @retval	true when at least one of the attached loggers would produce output
 */
bool ECLogger_Tee::Log(int loglevel) {
	LoggerList::iterator iLogger;
	bool bResult = false;

	for (iLogger = m_loggers.begin(); !bResult && iLogger != m_loggers.end(); ++iLogger)
		bResult = (*iLogger)->Log(loglevel);
	
	return bResult;
}	

/**
 * Log a message at the reuiqred loglevel to all attached loggers.
 *
 * @param[in]	loglevel	The requierd loglevel
 * @param[in]	message		The message to log
 */
void ECLogger_Tee::Log(int loglevel, const std::string &message) {
	LoggerList::iterator iLogger;

	for (iLogger = m_loggers.begin(); iLogger != m_loggers.end(); ++iLogger)
		(*iLogger)->Log(loglevel, message);
}

/**
 * Log a formatted message (printf style) to all attached loggers.
 *
 * @param[in]	loglevel	The required loglevel
 * @param[in]	format		The format string.
 */
void ECLogger_Tee::Log(int loglevel, const char *format, ...) {
	va_list va;

	va_start(va, format);
	LogVA(loglevel, format, va);
	va_end(va);
}

void ECLogger_Tee::LogVA(int loglevel, const char *format, va_list& va) {
	LoggerList::iterator iLogger;

	pthread_mutex_lock(&msgbuflock);
	_vsnprintf(msgbuffer, _LOG_BUFSIZE, format, va);

	for (iLogger = m_loggers.begin(); iLogger != m_loggers.end(); ++iLogger)
		(*iLogger)->Log(loglevel, std::string(msgbuffer));
	
	pthread_mutex_unlock(&msgbuflock);
}

/**
 * Add a logger to the list of loggers to log to.
 * @note The passed loggers reference will be increased, so
 *       make sure to release the logger in the caller function
 *       if it's going to be 'forgotten' there.
 *
 * @param[in]	lpLogger	The logger to attach.
 */
void ECLogger_Tee::AddLogger(ECLogger *lpLogger) {
	if (lpLogger) {
		lpLogger->AddRef();
		m_loggers.push_back(lpLogger);
	}
}

ECLogger_Pipe::ECLogger_Pipe(int fd, pid_t childpid, int loglevel) : ECLogger(loglevel) {
	m_fd = fd;
	m_childpid = childpid;
}

ECLogger_Pipe::~ECLogger_Pipe() {
	close(m_fd);						// this will make the log child exit
	if (m_childpid)
		waitpid(m_childpid, NULL, 0);	// wait for the child if we're the one that forked it
}

void ECLogger_Pipe::Reset() {
	// send the log process HUP signal again
	kill(m_childpid, SIGHUP);
}

void ECLogger_Pipe::Log(int loglevel, const std::string &message) {
	int len = 0;
	int off = 0;

	pthread_mutex_lock(&msgbuflock);

	msgbuffer[0] = loglevel;
	off += 1;

	if (prefix == LP_TID)
		len = snprintf(msgbuffer+off, _LOG_BUFSIZE -off, "[0x%08x] ", (unsigned int)pthread_self());
	else if (prefix == LP_PID)
		len = snprintf(msgbuffer+off, _LOG_BUFSIZE -off, "[%5d] ", getpid());
	off += len;

	len = min((int)message.length(), _LOG_BUFSIZE -off -1);
	memcpy(msgbuffer+off, message.c_str(), len);
	off += len;

	msgbuffer[off] = '\0';
	off++;

	// write as one block to get it to the real logger
	write(m_fd, msgbuffer, off);
	pthread_mutex_unlock(&msgbuflock);
}

void ECLogger_Pipe::Log(int loglevel, const char *format, ...) {
	va_list va;

	va_start(va, format);
	LogVA(loglevel, format, va);
	va_end(va);
}

void ECLogger_Pipe::LogVA(int loglevel, const char *format, va_list& va) {
	int len = 0;
	int off = 0;

	pthread_mutex_lock(&msgbuflock);
	msgbuffer[0] = loglevel;
	off += 1;

	if (prefix == LP_TID)
		len = snprintf(msgbuffer+off, _LOG_BUFSIZE -off, "[0x%08x] ", (unsigned int)pthread_self());
	else if (prefix == LP_PID)
		len = snprintf(msgbuffer+off, _LOG_BUFSIZE -off, "[%5d] ", getpid());
	off += len;

	// return value is what WOULD have been written if enough space were available in the buffer
	len = _vsnprintf(msgbuffer+off, _LOG_BUFSIZE -off -1, format, va);
	len = min(len, _LOG_BUFSIZE -off -2); // yes, -2, otherwise we could have 2 \0 at the end of the buffer
	off += len;

	msgbuffer[off] = '\0';
	off++;

	// write as one block to get it to the real logger
	write(m_fd, msgbuffer, off);
	pthread_mutex_unlock(&msgbuflock);
}

int ECLogger_Pipe::GetFileDescriptor()
{
	return m_fd;
}

/** 
 * Make sure we do not close the log process when this object is cleaned.
 */
void ECLogger_Pipe::Disown()
{
	m_childpid = 0;
}

namespace PrivatePipe {
	ECLogger_File *m_lpFileLogger;
	ECConfig *m_lpConfig;
	pthread_t signal_thread;
	sigset_t signal_mask;
	void sighup(int s) {
		if (m_lpConfig) {
			char *ll;
			m_lpConfig->ReloadSettings();
			ll = m_lpConfig->GetSetting("log_level");
			if (ll)
				m_lpFileLogger->SetLoglevel(atoi(ll));
		}

		m_lpFileLogger->Reset();
		m_lpFileLogger->Log(EC_LOGLEVEL_INFO, "[%5d] Log process received sighup", getpid());
	}
	void sigpipe(int s) {
		m_lpFileLogger->Log(EC_LOGLEVEL_INFO, "[%5d] Log process received sigpipe", getpid());
	}
	void* signal_handler(void*)
	{
		int sig;
		m_lpFileLogger->Log(EC_LOGLEVEL_DEBUG, "[%5d] Log signal thread started", getpid());
		while (sigwait(&signal_mask, &sig) == 0) {
			switch(sig) {
			case SIGHUP:
				sighup(sig);
				break;
			case SIGPIPE:
				sigpipe(sig);
				return NULL;
			};
		}
		return NULL;
	}	
	int PipePassLoop(int readfd, ECLogger_File *lpFileLogger, ECConfig* lpConfig) {
		int ret = 0;
		fd_set readfds;
		char buffer[_LOG_BUFSIZE] = {0};
		std::string complete;
		const char *p = NULL;
		int s;
		int l;
		bool bNPTL = true;

		confstr(_CS_GNU_LIBPTHREAD_VERSION, buffer, sizeof(buffer));
		if (strncmp(buffer, "linuxthreads", strlen("linuxthreads")) == 0)
			bNPTL = false;
		
		m_lpConfig = lpConfig;
		m_lpFileLogger = lpFileLogger;

		if (bNPTL) {
			sigemptyset(&signal_mask);
			sigaddset(&signal_mask, SIGHUP);
			sigaddset(&signal_mask, SIGPIPE);
			pthread_sigmask(SIG_BLOCK, &signal_mask, NULL);
			pthread_create(&signal_thread, NULL, signal_handler, NULL);
		} else {
			signal(SIGHUP, sighup);
			signal(SIGPIPE, sigpipe);
		}
		// ignore stop signals to keep logging until the very end
		signal(SIGTERM, SIG_IGN);
		signal(SIGINT, SIG_IGN);
		// close signals we don't want to see from the main program anymore
		signal(SIGCHLD, SIG_IGN);
		signal(SIGUSR1, SIG_IGN);
		signal(SIGUSR2, SIG_IGN);

		// We want the prefix of each individual thread/fork, so don't add that of the Pipe version.
		m_lpFileLogger->SetLogprefix(LP_NONE);

		while (true) {
			FD_ZERO(&readfds);
			FD_SET(readfd, &readfds);

			// blocking wait, returns on error or data waiting to log
			ret = select(readfd + 1, &readfds, NULL, NULL, NULL);
			if (ret <= 0) {
				if (errno == EINTR)
					continue;	// logger received SIGHUP, which wakes the select
				break;
			}

			complete.clear();
			do {
				// if we don't read anything from the fd, it was the end
				ret = read(readfd, buffer, _LOG_BUFSIZE);
				complete.append(buffer,ret);
			} while (ret == _LOG_BUFSIZE);
			if (ret <= 0)
				break;

			p = complete.data();
			ret = complete.size();
			while (ret && p) {
				// first char in buffer is loglevel
				l = *p++;
				ret--;
				s = strlen(p);	// find string in read buffer
				if (s) {
					lpFileLogger->Log(l, string(p, s));
					s++;		// add \0
					p += s;		// skip string
					ret -= s;
				} else {
					p = NULL;
				}
			}
		}
		// we need to stop fetching signals
		kill(getpid(), SIGPIPE);

		if (bNPTL)
			pthread_join(signal_thread, NULL);
		m_lpFileLogger->Log(EC_LOGLEVEL_INFO, "[%5d] Log process is done", getpid());
		return ret;
	}
}

/**
 * Starts a new process when needed for forked model programs. Only
 * actually replaces your ECLogger object if it's logging to a file.
 *
 * @param[in]	lpConfig	Pointer to your ECConfig object. Cannot be NULL.
 * @param[in]	lpLogger	Pointer to your current ECLogger object.
 * @return		ECLogger	Returns the same or new ECLogger object to use in your program.
 */
ECLogger* StartLoggerProcess(ECConfig* lpConfig, ECLogger* lpLogger) {
	ECLogger_File *lpFileLogger = dynamic_cast<ECLogger_File*>(lpLogger);
	ECLogger_Pipe *lpPipeLogger = NULL;
	int filefd;
	int pipefds[2];
	int t, i;
	pid_t child = 0;

	if (lpFileLogger == NULL)
		return lpLogger;

	filefd = lpFileLogger->GetFileDescriptor();

	child = pipe(pipefds);
	if (child < 0)
		return NULL;

	child = fork();
	if (child < 0)
		return NULL;

	if (child == 0) {
		// close all files except the read pipe and the logfile
		t = getdtablesize();
		for (i = 3; i<t; i++) {
			if (i == pipefds[0] || i == filefd) continue;
			close(i);
		}
		PrivatePipe::PipePassLoop(pipefds[0], lpFileLogger, lpConfig);
		close(pipefds[0]);
		delete lpFileLogger;
		delete lpConfig;
		_exit(0);
	}

	// this is the main fork, which doesn't log anything anymore, except through the pipe version.
	// we should release the lpFileLogger
	delete lpFileLogger;

	close(pipefds[0]);
	lpPipeLogger = new ECLogger_Pipe(pipefds[1], child, atoi(lpConfig->GetSetting("log_level"))); // let destructor wait on child
	lpPipeLogger->SetLogprefix(LP_PID);
	lpPipeLogger->Log(EC_LOGLEVEL_INFO, "Logger process started on pid %d", child);

	return lpPipeLogger;
}

/** 
 * Create ECLogger object from configuration.
 * 
 * @param[in] lpConfig ECConfig object with config settings from config file. Must have all log_* options.
 * @param argv0 name of the logger
 * @param lpszServiceName service name for windows event logger
 * @param bAudit prepend "audit_" before log settings to create an audit logger (zarafa-server)
 * 
 * @return Log object, or NULL on error
 */
ECLogger* CreateLogger(ECConfig *lpConfig, char *argv0, const char *lpszServiceName, bool bAudit) {
	ECLogger *lpLogger = NULL;
	string prepend;
	int syslog_facility = LOG_MAIL;

	if (bAudit) {
		if (parseBool(lpConfig->GetSetting("audit_log_enabled")) == false)
			return NULL;
		prepend = "audit_";
		syslog_facility = LOG_AUTHPRIV;
	}

	if (stricmp(lpConfig->GetSetting((prepend+"log_method").c_str()), "syslog") == 0) {
		lpLogger = new ECLogger_Syslog(atoi(lpConfig->GetSetting((prepend+"log_level").c_str())), basename(argv0), syslog_facility);
	} else if (stricmp(lpConfig->GetSetting((prepend+"log_method").c_str()), "eventlog") == 0) {
		fprintf(stderr, "eventlog logging is only available on windows.\n");
	} else if (stricmp(lpConfig->GetSetting((prepend+"log_method").c_str()), "file") == 0) {
		int ret = 0;
		struct passwd *pw = NULL;
		struct group *gr = NULL;
		if (strcmp(lpConfig->GetSetting((prepend+"log_file").c_str()), "-")) {
			if (lpConfig->GetSetting("run_as_user") && strcmp(lpConfig->GetSetting("run_as_user"),""))
				pw = (struct passwd *) getpwnam(lpConfig->GetSetting("run_as_user"));
			else
				pw = (struct passwd *) getpwuid(getuid());
			if (lpConfig->GetSetting("run_as_group") && strcmp(lpConfig->GetSetting("run_as_group"),""))
				gr = (struct group *) getgrnam(lpConfig->GetSetting("run_as_group"));
			else
				gr = (struct group *) getgrgid(getgid());

			// see if we can open the file as the user we're supposed to run as
			if (pw || gr) {
				ret = fork();
				if (ret == 0) {
					// client test program
					if (gr)
						setgid(gr->gr_gid);
					if (pw)
						setuid(pw->pw_uid);
					FILE *test = fopen(lpConfig->GetSetting((prepend+"log_file").c_str()), "a");
					if (!test) {
						fprintf(stderr, "Unable to open logfile '%s' as user '%s'\n",
								lpConfig->GetSetting((prepend+"log_file").c_str()), pw->pw_name);
						_exit(1);
					} else {
						fclose(test);
					}
					// free known alloced memory in parent before exiting, keep valgrind from complaining
					delete lpConfig;
					_exit(0);
				}
				if (ret > 0) {	// correct parent, (fork != -1)
					wait(&ret);
					ret = WEXITSTATUS(ret);
				}
			}
		}
		if (ret == 0) {
			lpLogger = new ECLogger_File(atoi(lpConfig->GetSetting((prepend+"log_level").c_str())),
										 atoi(lpConfig->GetSetting((prepend+"log_timestamp").c_str())),
										 lpConfig->GetSetting((prepend+"log_file").c_str()), false);
			// chown file
			if (pw || gr) {
				uid_t uid = -1;
				gid_t gid = -1;
				if (pw)
					uid = pw->pw_uid;
				if (gr)
					gid = gr->gr_gid;
				chown(lpConfig->GetSetting((prepend+"log_file").c_str()), uid, gid);
			}
		} else {
			fprintf(stderr, "Not enough permissions to append logfile '%s'. Reverting to stderr.\n", lpConfig->GetSetting((prepend+"log_file").c_str()));
			lpLogger = new ECLogger_File(atoi(lpConfig->GetSetting("log_level")), atoi(lpConfig->GetSetting((prepend+"log_timestamp").c_str())), "-", false);
		}
	}
	if (!lpLogger) {
		fprintf(stderr, "Incorrect logging method selected. Reverting to stderr.\n");
		lpLogger = new ECLogger_File(atoi(lpConfig->GetSetting((prepend+"log_level").c_str())), atoi(lpConfig->GetSetting((prepend+"log_timestamp").c_str())), "-", false);
	}

	return lpLogger;
}

int DeleteLogger(ECLogger *lpLogger) {
	if (lpLogger)
		lpLogger->Release();
	return 0;
}

void LogConfigErrors(ECConfig *lpConfig, ECLogger *lpLogger) {
	list<string> *strings;
	list<string>::iterator i;

	if (!lpConfig || !lpLogger)
		return;

	strings = lpConfig->GetWarnings();
	for (i = strings->begin() ; i != strings->end(); i++)
		lpLogger->Log(EC_LOGLEVEL_WARNING, "Config warning: " + *i);

	strings = lpConfig->GetErrors();
	for (i = strings->begin() ; i != strings->end(); i++)
		lpLogger->Log(EC_LOGLEVEL_FATAL, "Config error: " + *i);
}
