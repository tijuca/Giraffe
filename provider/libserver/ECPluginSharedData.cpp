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
#include "ECPluginSharedData.h"

ECPluginSharedData *ECPluginSharedData::m_lpSingleton = NULL;
pthread_mutex_t ECPluginSharedData::m_SingletonLock = PTHREAD_MUTEX_INITIALIZER;

ECPluginSharedData::ECPluginSharedData(ECConfig *lpParent, ECLogger *lpLogger, IECStatsCollector *lpStatsCollector, bool bHosted, bool bDistributed)
{
	m_ulRefCount = 0;
	m_lpConfig = NULL;
	m_lpDefaults = NULL;
	m_lpszDirectives = NULL;
	m_lpParentConfig = lpParent;
	m_lpLogger = lpLogger;
	m_lpStatsCollector = lpStatsCollector;
	m_bHosted = bHosted;
	m_bDistributed = bDistributed;
}

ECPluginSharedData::~ECPluginSharedData()
{
	if (m_lpConfig)
		delete m_lpConfig;
	if (m_lpDefaults) {
		for (int n = 0; m_lpDefaults[n].szName; n++) {
			free((void*)m_lpDefaults[n].szName);
			free((void*)m_lpDefaults[n].szValue);
		}
		delete [] m_lpDefaults;
	}
	if (m_lpszDirectives) {
		for (int n = 0; m_lpszDirectives[n]; n++)
			free(m_lpszDirectives[n]);
		delete [] m_lpszDirectives;
	}
}

void ECPluginSharedData::GetSingleton(ECPluginSharedData **lppSingleton, ECConfig *lpParent, ECLogger *lpLogger,
									  IECStatsCollector *lpStatsCollector, bool bHosted, bool bDistributed)
{
	pthread_mutex_lock(&m_SingletonLock);

	if (!m_lpSingleton)
		m_lpSingleton = new ECPluginSharedData(lpParent, lpLogger, lpStatsCollector, bHosted, bDistributed); 

	m_lpSingleton->m_ulRefCount++;
	*lppSingleton = m_lpSingleton;

	pthread_mutex_unlock(&m_SingletonLock);
}

void ECPluginSharedData::AddRef()
{
	pthread_mutex_lock(&m_SingletonLock);
	m_ulRefCount++;
	pthread_mutex_unlock(&m_SingletonLock);
}

void ECPluginSharedData::Release()
{
	pthread_mutex_lock(&m_SingletonLock);
	if (!--m_ulRefCount) {
		delete m_lpSingleton;
		m_lpSingleton = NULL;
	}
	pthread_mutex_unlock(&m_SingletonLock);
}

ECConfig* ECPluginSharedData::CreateConfig(const configsetting_t *lpDefaults, const char **lpszDirectives)
{
	if (!m_lpConfig)
	{
		int n;
		/*
		 * Store all the defaults and directives in the singleton,
		 * so it isn't removed from memory when the plugin unloads.
		 */
		if (lpDefaults) {
			for (n = 0; lpDefaults[n].szName; n++) ;
			m_lpDefaults = new configsetting_t[n+1];
			for (n = 0; lpDefaults[n].szName; n++) {
				m_lpDefaults[n].szName = strdup(lpDefaults[n].szName);
				m_lpDefaults[n].szValue = strdup(lpDefaults[n].szValue);
				m_lpDefaults[n].ulFlags = lpDefaults[n].ulFlags;
				m_lpDefaults[n].ulGroup = lpDefaults[n].ulGroup;
			}
			m_lpDefaults[n].szName = NULL;
			m_lpDefaults[n].szValue = NULL;
		}

		if (lpszDirectives) {
			for (n = 0; lpszDirectives[n]; n++) ;
			m_lpszDirectives = new char*[n+1];
			for (n = 0; lpszDirectives[n]; n++)
				m_lpszDirectives[n] = strdup(lpszDirectives[n]);
			m_lpszDirectives[n] = NULL;
		}

		m_lpConfig = ECConfig::Create((const configsetting_t *)m_lpDefaults, (const char **)m_lpszDirectives);
		if (!m_lpConfig->LoadSettings(m_lpParentConfig->GetSetting("user_plugin_config")))
			m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Failed to open plugin configuration file, using defaults.");
		if (m_lpConfig->HasErrors() || m_lpConfig->HasWarnings()) {
			LogConfigErrors(m_lpConfig, m_lpLogger);
			if(m_lpConfig->HasErrors()) {
				delete m_lpConfig;
				m_lpConfig = NULL;
			}
		}
	}

	return m_lpConfig;
}

ECLogger* ECPluginSharedData::GetLogger()
{
	return m_lpLogger;
}

IECStatsCollector* ECPluginSharedData::GetStatsCollector()
{
	return m_lpStatsCollector;
}

bool ECPluginSharedData::IsHosted()
{
	return m_bHosted;
}

bool ECPluginSharedData::IsDistributed()
{
	return m_bDistributed;
}

void ECPluginSharedData::Signal(int signal)
{
	if (!m_lpConfig)
		return;

	switch (signal) {
	case SIGHUP:
		if (!m_lpConfig->ReloadSettings())
			m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to reload plugin configuration file, continuing with current settings.");
			
		if (m_lpConfig->HasErrors()) {
			m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Unable to reload plugin configuration file.");
			LogConfigErrors(m_lpConfig, m_lpLogger);
		}
		break;
	default:
		break;
	}
}

