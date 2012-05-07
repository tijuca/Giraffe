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

#ifndef ECPLUGINSHAREDDATA_H
#define ECPLUGINSHAREDDATA_H

#include <pthread.h>

#include <ECConfig.h>
#include <ECLogger.h>
#include <IECStatsCollector.h>

/**
 * Shared plugin data
 *
 * Each Server thread owns its own UserPlugin object.
 * Each instance of the UserPlugin share the contents
 * of ECPluginSharedData.
 */
class ECPluginSharedData {
private:
	/**
	 * Singleton instance of ECPluginSharedData
	 */
	static ECPluginSharedData *m_lpSingleton;

	/**
	 * Lock for m_lpSingleton access
	 */
	static pthread_mutex_t m_SingletonLock;

	/**
	 * Reference count, used to destroy object when no users are left.
	 */
	unsigned int m_ulRefCount;

	/**
	 * Constructor
	 *
	 * @param[in]	lpParent
	 *					Pointer to ECConfig to read configuration option from the server
	 * @param[in]	lpLogger
	 *					Pointer to ECLogger to print debug information to the log
	 * @param[in]	lpStatsCollector
	 *					Pointer to IECStatsCollector to collect statistics about 
	 *					plugin specific tasks (i.e. the number of SQL or LDAP queries)
	 * @param[in]   bHosted
	 *					Boolean to indicate if multi-company support should be enabled.
	 *					Plugins are allowed to throw an exception when bHosted is true
	 *					while the plugin doesn't support multi-company.
	 * @param[in]	bDistributed
	 *					Boolean to indicate if multi-server support should be enabled.
	 * 					Plugins are allowed to throw an exception when bDistributed is true
	 *					while the plugin doesn't support multi-server.
	 */
	ECPluginSharedData(ECConfig *lpParent, ECLogger *lpLogger, IECStatsCollector *lpStatsCollector, bool bHosted, bool bDistributed);

	/**
	 * Default destructor
	 */
	~ECPluginSharedData();

public:
	/**
	 * Obtain singleton pointer to ECPluginSharedData.
	 *
	 * @param[out]	lppSingleton
	 *					The singleton ECPluginSharedData pointer
	 * @param[in]	lpParent
	 *					Server configuration file
	 * @param[in]	lpLogger
	 *					Server logger
	 * @param[in]	lpStatsCollector
	 *					Statistics collector
	 * @param[in]   bHosted
	 *					Boolean to indicate if multi-company support should be enabled.
	 *					Plugins are allowed to throw an exception when bHosted is true
	 *					while the plugin doesn't support multi-company.
	 * @param[in]	bDistributed
	 *					Boolean to indicate if multi-server support should be enabled.
	 * 					Plugins are allowed to throw an exception when bDistributed is true
	 *					while the plugin doesn't support multi-server.
	 */
	static void GetSingleton(ECPluginSharedData **lppSingleton, ECConfig *lpParent, ECLogger *lpLogger,
							 IECStatsCollector *lpStatsCollector, bool bHosted, bool bDistributed);

	/**
	 * Increase reference count
	 */
	virtual void AddRef();

	/**
	 * Decrease reference count, object might be destroyed before this function returns.
	 */
	virtual void Release();

	/**
	 * Load plugin configuration file
	 *
	 * @param[in]	lpDefaults
	 *					Default values for configuration options.
	 * @param[in]	lpszDirectives
	 *					Supported configuration file directives.
	 * @return The ECConfig pointer. NULL if configuration file could not be loaded.
	 */
	virtual ECConfig* CreateConfig(const configsetting_t *lpDefaults, const char **lpszDirectives = lpszDEFAULTDIRECTIVES);

	/**
	 * Obtain the ECLogger
	 *
	 * @return The ECLogger pointer
	 */
	virtual ECLogger* GetLogger();

	/**
	 * Obtain the Stats collector
	 *
	 * @return the IECStatsCollector pointer
	 */
	virtual IECStatsCollector* GetStatsCollector();

	/**
	 * Check for multi-company support
	 *
	 * @return True if multi-company support is enabled.
	 */
	virtual bool IsHosted();

	/**
	 * Check for multi-server support
	 * 
	 * @return True if multi-server support is enabled.
	 */
	virtual bool IsDistributed();

	/**
	 * Signal handler for userspace signals like SIGHUP
	 *
	 * @param[in]	signal
	 *					The signal ID to be handled
	 */
	virtual void Signal(int signal);

private:
	/**
	 * Plugin configuration file
	 */
	ECConfig *m_lpConfig;

	/**
	 * Server configuration file
	 */
	ECConfig *m_lpParentConfig;

	/**
	 * Server logger
	 */
	ECLogger *m_lpLogger;

	/**
	 * Statistics collector
	 */
	IECStatsCollector *m_lpStatsCollector;

	/**
	 * True if multi-company support is enabled.
	 */
	bool m_bHosted;

	/**
	 * True if multi-server support is enabled.
	 */
	bool m_bDistributed;

	/**
	 * Copy of plugin defaults, stored in the singleton
	 */
	configsetting_t *m_lpDefaults;

	/**
	 * Copy of plugin directives, stored in the singleton
	 */
	char **m_lpszDirectives;
};

#endif
