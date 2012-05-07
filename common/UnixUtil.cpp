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

#include <platform.h>
#include "UnixUtil.h"

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>

#include <string>
using namespace std;

int unix_runas(ECConfig *lpConfig, ECLogger *lpLogger) {
	if (strcmp(lpConfig->GetSetting("run_as_group"),"")) {
		struct group *gr;
		gr = (struct group *) getgrnam(lpConfig->GetSetting("run_as_group"));
		if (!gr) {
			lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to run as group '%s'", lpConfig->GetSetting("run_as_group"));
			return -1;
		}
		if (getgid() != gr->gr_gid) {
			if (setgid(gr->gr_gid) != 0) {
				switch (errno) {
				case EPERM:
					lpLogger->Log(EC_LOGLEVEL_FATAL, "Not enough permissions to change to group '%s'.", gr->gr_name);
					break;
				default:
					lpLogger->Log(EC_LOGLEVEL_FATAL, "Unknown error while setting user id: %d", errno);
					break;
				};
				return -1;
			}
		}
	}

	if (strcmp(lpConfig->GetSetting("run_as_user"),"")) {
		struct passwd *pw;
		pw = (struct passwd *) getpwnam(lpConfig->GetSetting("run_as_user"));
		if (!pw) {
			lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to run as user '%s'", lpConfig->GetSetting("run_as_user"));
			return -1;
		}
		if (getuid() != pw->pw_uid) {
			if (setuid(pw->pw_uid) != 0) {
				switch (errno) {
				case EAGAIN:
					lpLogger->Log(EC_LOGLEVEL_FATAL, "EGAIN error while setting user id.");
					break;
				case EPERM:
					lpLogger->Log(EC_LOGLEVEL_FATAL, "Not enough permissions to change to user '%s'.", pw->pw_name);
					break;
				default:
					lpLogger->Log(EC_LOGLEVEL_FATAL, "Unknown error while setting user id: %d", errno);
					break;
				};
				return -1;
			}
		}
	}

	return 0;
}

int unix_chown(const char *filename, const char *username, const char *groupname) {
	struct group *gr = NULL;
	struct passwd *pw = NULL;
	uid_t uid;
	gid_t gid;

	gid = getgid();

	if (strcmp(groupname,"")) {
		gr = (struct group *) getgrnam(groupname);
		if (gr)
			gid = gr->gr_gid;
	}

	uid = getuid();

	if (strcmp(username,"")) {
		pw = (struct passwd *) getpwnam(username);
		if (pw)
			uid = pw->pw_uid;
	}

	return chown(filename, uid, gid);
}

int unix_create_pidfile(char *argv0, ECConfig *lpConfig, ECLogger *lpLogger, bool bForce) {
	string pidfilename = string("/var/run/")+string(argv0)+string(".pid");
	FILE *pidfile;
	int oldpid;
	char tmp[255];
	bool running = false;

	if (strcmp(lpConfig->GetSetting("pid_file"), "")) {
		pidfilename = lpConfig->GetSetting("pid_file");
	}

	// test for existing and running process
	pidfile = fopen(pidfilename.c_str(), "r");
	if (pidfile) {
		fscanf(pidfile, "%d", &oldpid);
		fclose(pidfile);

		snprintf(tmp, 255, "/proc/%d/cmdline", oldpid);
		pidfile = fopen(tmp, "r");
		if (pidfile) {
			fscanf(pidfile, "%s", tmp);
			fclose(pidfile);

			if (strlen(tmp) < strlen(argv0)) {
				if (strstr(argv0, tmp))
					running = true;
			} else {
				if (strstr(tmp, argv0))
					running = true;
			}

			if (running) {
				lpLogger->Log(EC_LOGLEVEL_FATAL, "Warning: Process %s is probably already running.", argv0);
				if (!bForce) {
					lpLogger->Log(EC_LOGLEVEL_FATAL, "If you are sure the process is stopped, please remove pidfile %s", pidfilename.c_str());
					return -1;
				}
			}
		}
	}

	pidfile = fopen(pidfilename.c_str(), "w");
	if (!pidfile) {
		lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to open pidfile '%s'", pidfilename.c_str());
		return 1;
	}

	fprintf(pidfile, "%d\n", getpid());
	fclose(pidfile);
	return 0;
}

int unix_daemonize(ECConfig *lpConfig, ECLogger *lpLogger) {
	int ret;
	char *path = lpConfig->GetSetting("running_path");

	ret = fork();
	if (ret == -1) {
		lpLogger->Log(EC_LOGLEVEL_FATAL, "Daemonizing failed on 1st step");
		return -1;
	}
	if (ret)
		_exit(0);				// close parent process

	setsid();					// start new session

	ret = fork();
	if (ret == -1) {
		lpLogger->Log(EC_LOGLEVEL_FATAL, "Daemonizing failed on 2nd step");
		return -1;
	}
	if (ret)
		_exit(0);				// close parent process

	// close output to console. a logger which logged to the console is now diverted to /dev/null
	fclose(stdin);
	freopen("/dev/null", "a+", stdout);
	freopen("/dev/null", "a+", stderr);

	// make sure we daemonize in an always existing directory
	if (path) {
		ret = chdir(path);
		if (ret)
			lpLogger->Log(EC_LOGLEVEL_ERROR, "Unable to run in given path '%s': %s", path, strerror(errno));
	}

	if (!path || ret)
		chdir("/");

	return 0;
}

/**
 * Starts a new unix process and calls the given function. Optionally
 * closes some given file descriptors. The child process does not
 * return from this function.
 *
 * @param[in]	func	Pointer to a function with one void* parameter and returning a void* that should run in the child process.
 * @param[in]	param	Parameter to pass to the func function.
 * @param[in]	nCloseFDs	Number of file descriptors in pCloseFDs.
 * @param[in]	pCloseFDs	Array of file descriptors to close in the child process.
 * @retval	processid of the started child, or a negative value on error.
 */
int unix_fork_function(void*(func)(void*), void *param, int nCloseFDs, int *pCloseFDs)
{
	int pid;

	if (!func)
		return -1;

	pid = fork();
	if (pid < 0)
		return pid;

	if (pid == 0) {
		// reset the SIGHUP signal to default, not to trigger the config/logfile reload signal too often on 'killall <process>'
		signal(SIGHUP, SIG_DFL);
		// close filedescriptors
		for (int n = 0; n < nCloseFDs && pCloseFDs; n++)
			if (pCloseFDs[n] >= 0)
				close(pCloseFDs[n]);
		func(param);
		_exit(0);
	}

	return pid;
}

/** 
 * Starts a new process with a read and write channel. Optionally sets
 * resource limites and environment variables.
 * 
 * @param lpLogger[in] Logger object where error messages during the function may be sent to. Cannot be NULL.
 * @param lpszCommand[in] The command to execute in the new subprocess.
 * @param lpulIn[out] The filedescriptor to read data of the command from.
 * @param lpulOut[out] The filedescriptor to write data to the command to.
 * @param lpLimits[in] Optional resource limits to set for the new subprocess.
 * @param env[in] Optional environment variables to set in the new subprocess.
 * @param bNonBlocking[in] Make the in and out pipes non-blocking on read and write calls.
 * @param bStdErr[in] Add STDERR output to *lpulOut
 * 
 * @return new process pid, or -1 on failure.
 */
pid_t unix_popen_rw(ECLogger *lpLogger, const char *lpszCommand, int *lpulIn, int *lpulOut, popen_rlimit_array *lpLimits, const char **env, bool bNonBlocking, bool bStdErr)
{
	int ulIn[2];
	int ulOut[2];
	pid_t pid;

	if (!lpszCommand || !lpulIn || !lpulOut)
		return -1;

	if (pipe(ulIn) || pipe(ulOut))
		return -1;

	if (bNonBlocking) {
		if (fcntl(ulIn[0], F_SETFL, O_NONBLOCK) < 0 || fcntl(ulIn[1], F_SETFL, O_NONBLOCK) < 0 ||
			fcntl(ulOut[0], F_SETFL, O_NONBLOCK) < 0 || fcntl(ulOut[1], F_SETFL, O_NONBLOCK) < 0)
			return -1;
	}

	pid = vfork();
	if (pid < 0)
		return pid;

	if (pid == 0) {
		/* Close pipes we aren't going to use */
		close(ulIn[STDOUT_FILENO]);
		dup2(ulIn[STDIN_FILENO], STDIN_FILENO);
		close(ulOut[STDIN_FILENO]);
		dup2(ulOut[STDOUT_FILENO], STDOUT_FILENO);
		if (bStdErr)
			dup2(ulOut[STDOUT_FILENO], STDERR_FILENO);

		// give the process a new group id, so we can easely kill all sub processes of this child too when needed.
		setsid();

		/* If provided set rlimit settings */
		if (lpLimits) {
			for (unsigned int i = 0; i < lpLimits->cValues; i++) {
				if (setrlimit(lpLimits->sLimit[i].resource, &lpLimits->sLimit[i].limit) != 0)
					lpLogger->Log(EC_LOGLEVEL_ERROR, "Unable to set rlimit for popen - resource %d, errno %d",
								  lpLimits->sLimit[i].resource, errno);
			}
		}

		if (execle("/bin/sh", "sh", "-c", lpszCommand, NULL, env) == 0)
			_exit(EXIT_SUCCESS);
		else
			_exit(EXIT_FAILURE);
		return 0;
	}

	*lpulIn = ulIn[STDOUT_FILENO];
	close(ulIn[STDIN_FILENO]);

	*lpulOut = ulOut[STDIN_FILENO];
	close(ulOut[STDOUT_FILENO]);

	return pid;
}

/** 
 * Closes the filedescriptors opened by unix_popen_rw, and waits for
 * the given pid to exit. If pid did not yet exit, a TERM signal will
 * be sent to the pid, and wait for it to close.
 * 
 * @param rfd The read part of the pipe, will be closed, except when -1 is passed
 * @param wfd The write part of the pipe, will be closed, except when -1 is passed
 * @param pid The pid to handle on
 * 
 * @return The exit code of the pid, or -1 for failure of this function
 */
int unix_pclose(int rfd, int wfd, pid_t pid)
{
	int rc = 0;
	int retval = 0;
	int signal = SIGTERM;
	int kills = 0;

	// close the filedescriptors, makes the child exit if it didn't already.
	if (wfd >= 0)
		close(wfd);

	if (rfd >= 0)
		close(rfd);

	while (true) {
		// we need to send the pid a signal if waitpid fails, so add WNOHANG
		rc = waitpid(pid, &retval, WNOHANG | WUNTRACED);
		if (rc == 0) {
			if (kills > 1) {
				// we've tried everything, so let this slide and maybe leave some zombie hanging around
				retval = -1;
				break;
			}
			// The child pid has not exited yet. Send it the TERM signal to the complete group and try again
			kill(-pid, signal);
			kills++;
			// next time this happens, it really needs to die!
			signal = SIGKILL;
			// we need to sleep to give the kernel some time to progress the death of the child
			sleep(1);
			continue;
		} else {
			break;
		}
	}

	return retval;
}

/**
 * Start an external process
 *
 * This function is used to start an external process that requires no STDIN input. The output will be
 * logged via lpLogger if provided.
 *
 * @param lpLogger[in] 		NULL or pointer to logger object to log to (will be logged via EC_LOGLEVEL_INFO)
 * @param lsszLogName[in]	Name to show in the log. Will show NAME[pid]: DATA
 * @param lpszCommand[in] 	String to command to be started, which will be executed with /bin/sh -c "string"
 * @param env[in] 			NULL-terminated array of strings with environment settings in the form ENVNAME=VALUE, see 
 *                			execlp(3) for details
 *
 * @return Returns TRUE on success, FALSE on failure
 */
bool unix_system(ECLogger *lpLogger, const char *lpszLogName, const char *lpszCommand, const char **env)
{
	int fdin = 0, fdout = 0;
	int pid = unix_popen_rw(lpLogger, lpszCommand, &fdin, &fdout, NULL, env, false, true);
	char buffer[1024];
	int status = 0;
	bool rv = true;
	FILE *fp = fdopen(fdout, "rb");
	close(fdin);
	
	while (fgets(buffer, sizeof(buffer), fp)) {
		buffer[strlen(buffer) - 1] = '\0'; // strip enter
		if(lpLogger)
			lpLogger->Log(EC_LOGLEVEL_ERROR, "%s[%d]: %s", lpszLogName, pid, buffer);
	}
	
	fclose(fp);
	
	waitpid(pid, &status, 0);

	if(status != -1) {
#ifdef WEXITSTATUS
		if(WIFEXITED(status)) { /* Child exited by itself */
			if(WEXITSTATUS(status)) {
				lpLogger->Log(EC_LOGLEVEL_ERROR, "Command '%s' exited with non-zero status %d", lpszCommand, WEXITSTATUS(status));
				rv = false;
			}
			else
				lpLogger->Log(EC_LOGLEVEL_INFO, "Command '%s' run successful", lpszCommand);
		} else if(WIFSIGNALED(status)) {        /* Child was killed by a signal */
			lpLogger->Log(EC_LOGLEVEL_ERROR, "Command '%s' was killed by signal %d", lpszCommand, WTERMSIG(status));
			rv = false;
		} else {                        /* Something strange happened */
			lpLogger->Log(EC_LOGLEVEL_ERROR, string("Command ") + lpszCommand + " terminated abnormally");
			rv = false;
		}
#else
		if (status)
			lpLogger->Log(EC_LOGLEVEL_ERROR, "Command '%s' exited with status %d", lpszCommand, status);
		else
			lpLogger->Log(EC_LOGLEVEL_INFO, "Command '%s' run successful", lpszCommand);
#endif
	} else {
		lpLogger->Log(EC_LOGLEVEL_ERROR, string("System call `system' failed: ") + strerror(errno));
		rv = false;
	}
	
	return rv;
}
