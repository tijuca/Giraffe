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

#include <platform.h>
#include <ecversion.h>

#include "ECSyncLog.h"
#include "ECSyncSettings.h"
#include "stringutil.h"
#include <ECLogger.h>

#include <cstdlib>
#include <mapidefs.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static const char THIS_FILE[] = __FILE__;
#endif

// for LOG_MAIL
#include <syslog.h>

HRESULT ECSyncLog::GetLogger(ECLogger **lppLogger)
{
	HRESULT		hr = hrSuccess;

	*lppLogger = NULL;

	pthread_mutex_lock(&s_hMutex);

	if (s_lpLogger == NULL) {
		ECSyncSettings *lpSettings = ECSyncSettings::GetInstance();

		if (lpSettings->SyncLogEnabled()) {
			char dummy[MAX_PATH + 1] = { 0 };

			if (GetTempPathA(sizeof dummy, dummy) >= sizeof dummy)
				dummy[0] = 0x00;

			std::string strPath = dummy + std::string("/");

			if (lpSettings->ContinuousLogging()) {
				time_t now = time(NULL);

				strPath += "synclog-";
				strPath += stringify(now);
				strPath += ".txt.gz";

				s_lpLogger = new ECLogger_File(lpSettings->SyncLogLevel(), 1, (char*)strPath.c_str(), true, 4096);
			} else {
				strPath += "synclog.txt";
				s_lpLogger = new ECLogger_File(lpSettings->SyncLogLevel(), 1, (char*)strPath.c_str(), false, 4096);
			}

			s_lpLogger->Log(EC_LOGLEVEL_FATAL, "********************");
			s_lpLogger->Log(EC_LOGLEVEL_FATAL, "New sync log session openend (Zarafa-" PROJECT_VERSION_CLIENT_STR ")");
			s_lpLogger->Log(EC_LOGLEVEL_FATAL, " - Log level: %u", lpSettings->SyncLogLevel());
			s_lpLogger->Log(EC_LOGLEVEL_FATAL, " - Sync stream: %s", lpSettings->SyncStreamEnabled() ? "enabled" : "disabled");
			s_lpLogger->Log(EC_LOGLEVEL_FATAL, " - Change notifications: %s", lpSettings->ChangeNotificationsEnabled() ? "enabled" : "disabled");
			s_lpLogger->Log(EC_LOGLEVEL_FATAL, " - State collector: %s", lpSettings->StateCollectorEnabled() ? "enabled" : "disabled");
			s_lpLogger->Log(EC_LOGLEVEL_FATAL, "********************");
		}
		else {
			s_lpLogger = new ECLogger_Null();
		}
	}

	if (!s_lpLogger) {
		s_lpLogger = new ECLogger_Syslog(EC_LOGLEVEL_DEBUG, "zarafa-libsync", LOG_MAIL);
	}

	*lppLogger = s_lpLogger;

	s_lpLogger->AddRef();

	pthread_mutex_unlock(&s_hMutex);

	return hr;
}

HRESULT ECSyncLog::SetLogger(ECLogger *lpLogger)
{
	pthread_mutex_lock(&s_hMutex);

	if (s_lpLogger)
		s_lpLogger->Release();

	s_lpLogger = lpLogger;
	if (s_lpLogger)
		s_lpLogger->AddRef();

	pthread_mutex_unlock(&s_hMutex);

	return hrSuccess;
}

pthread_mutex_t	ECSyncLog::s_hMutex;
ECLogger		*ECSyncLog::s_lpLogger = NULL;


ECSyncLog::__initializer::__initializer() {
	pthread_mutex_init(&ECSyncLog::s_hMutex, NULL);
}

ECSyncLog::__initializer::~__initializer() {
	if (ECSyncLog::s_lpLogger) {
		unsigned ulRef = ECSyncLog::s_lpLogger->Release();
		// Make sure all references are released so compressed logs don't get corrupted.
		while (ulRef)
			ulRef = ECSyncLog::s_lpLogger->Release();
	}

	pthread_mutex_destroy(&ECSyncLog::s_hMutex);
}

ECSyncLog::__initializer ECSyncLog::__i;
