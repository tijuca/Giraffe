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
#include "ECSyncSettings.h"

#include <pthread.h>
#include <mapix.h>

#include <ECLogger.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


ECSyncSettings* ECSyncSettings::GetInstance()
{
	pthread_mutex_lock(&s_hMutex);

	if (s_lpInstance == NULL)
		s_lpInstance = new ECSyncSettings;

	pthread_mutex_unlock(&s_hMutex);

	return s_lpInstance;
}

ECSyncSettings::ECSyncSettings()
: m_ulSyncLog(0)
, m_ulSyncLogLevel(EC_LOGLEVEL_INFO)
, m_ulSyncOpts(EC_SYNC_OPT_ALL)
, m_ulStreamTimeout(30000)
, m_ulStreamBufferSize(131072)
, m_ulStreamBatchSize(256)
{
	char *env = getenv("ZARAFA_SYNC_LOGLEVEL");
	if (env && env[0] != '\0') {
		unsigned loglevel = strtoul(env, NULL, 10);
		if (loglevel > 0) {
			m_ulSyncLog = 1;
			m_ulSyncLogLevel = loglevel;
		}
	}

	env = getenv("ZARAFA_STREAM_TIMEOUT");
	if (env && env[0] != '\0')
		m_ulStreamTimeout = strtoul(env, NULL, 10);

	env = getenv("ZARAFA_STREAM_BUFFERSIZE");
	if (env && env[0] != '\0')
		m_ulStreamBufferSize = strtoul(env, NULL, 10);

	env = getenv("ZARAFA_STREAM_BATCHSIZE");
	if (env && env[0] != '\0')
		m_ulStreamBatchSize = strtoul(env, NULL, 10);
}

bool ECSyncSettings::SyncLogEnabled() const {
	return ContinuousLogging() ? true : m_ulSyncLog != 0;
}

ULONG ECSyncSettings::SyncLogLevel() const {
	return ContinuousLogging() ? EC_LOGLEVEL_DEBUG : m_ulSyncLogLevel;
}

bool ECSyncSettings::ContinuousLogging() const {
	return (m_ulSyncOpts & EC_SYNC_OPT_CONTINUOUS) == EC_SYNC_OPT_CONTINUOUS;
}

bool ECSyncSettings::SyncStreamEnabled() const {
	return (m_ulSyncOpts & EC_SYNC_OPT_STREAM) == EC_SYNC_OPT_STREAM;
}

bool ECSyncSettings::ChangeNotificationsEnabled() const {
	return (m_ulSyncOpts & EC_SYNC_OPT_CHANGENOTIF) == EC_SYNC_OPT_CHANGENOTIF;
}

bool ECSyncSettings::StateCollectorEnabled() const {
	return (m_ulSyncOpts & EC_SYNC_OPT_STATECOLLECT) == EC_SYNC_OPT_STATECOLLECT;
}

ULONG ECSyncSettings::StreamTimeout() const {
	return m_ulStreamTimeout;
}

ULONG ECSyncSettings::StreamBufferSize() const {
	return m_ulStreamBufferSize;
}

ULONG ECSyncSettings::StreamBatchSize() const {
	return m_ulStreamBatchSize;
}

/**
 * Enable/disable the synclog.
 * @param[in]	bEnable		Set to true to enable the synclog.
 * @retval		The previous value.
 */
bool ECSyncSettings::EnableSyncLog(bool bEnable) {
	bool bPrev = SyncLogEnabled();
	m_ulSyncLog = (bEnable ? 1 : 0);
	return bPrev;
}

/**
 * Set the synclog loglevel.
 * @param[in]	ulLogLevel	The new loglevel.
 * @retval		The previous loglevel.
 * @note		The loglevel returned by this function can differ
 *				from the level returned by SyncLogLevel() when
 *				continuous logging is enabled, in which case 
 *				SyncLogLevel() will always return EC_LOGLEVEL_DEBUG.
 */
ULONG ECSyncSettings::SetSyncLogLevel(ULONG ulLogLevel) {
	ULONG ulPrev = m_ulSyncLogLevel;
	if (ulLogLevel >= EC_LOGLEVEL_FATAL && ulLogLevel <= EC_LOGLEVEL_DEBUG)
		m_ulSyncLogLevel = ulLogLevel;
	return ulPrev;
}

/**
 * Set the sync options.
 * @param[in]	ulOptions	The options to enable
 * @retval		The previous value.
 */
ULONG ECSyncSettings::SetSyncOptions(ULONG ulOptions) {
	ULONG ulPrev = m_ulSyncOpts;
	m_ulSyncOpts = ulOptions;
	return ulPrev;
}

/**
 * Set the stream timeout.
 * This is the timeout used by the asynchronous components. This has nothing
 * to do with network timeouts.
 * @param[in]	ulTimeout	The new timeout in milliseconds
 * @retval		The previous value.
 * @note	No validation of the passed value is performed. So setting
 * 			this to an insane value will result in insane behavior.
 */
ULONG ECSyncSettings::SetStreamTimeout(ULONG ulTimeout) {
	ULONG ulPrev = m_ulStreamTimeout;
	m_ulStreamTimeout = ulTimeout;
	return ulPrev;
}

/**
 * Set the stream buffer size.
 * This is the size of the buffer that is used to transfer data from the
 * exporter to the importer.
 * @param[in]	ulBufferSize	The new buffer size in bytes.
 * @retval		The previous value.
 * @note	No validation of the passed value is performed. So setting
 * 			this to an insane value will result in insane behavior.
 */
ULONG ECSyncSettings::SetStreamBufferSize(ULONG ulBufferSize) {
	ULONG ulPrev = m_ulStreamBufferSize;
	m_ulStreamBufferSize = ulBufferSize;
	return ulPrev;
}

/**
 * Set the stream batch size.
 * This is the number of messages that is requested by the exporter from the
 * server in one batch.
 * @param[in]	ulTimeout	The new size in messages
 * @retval		The previous value.
 * @note	No validation of the passed value is performed. So setting
 * 			this to an insane value will result in insane behavior.
 */
ULONG ECSyncSettings::SetStreamBatchSize(ULONG ulBatchSize) {
	ULONG ulPrev = m_ulStreamBatchSize;
	m_ulStreamBatchSize = ulBatchSize;
	return ulPrev;
}



pthread_mutex_t ECSyncSettings::s_hMutex;
ECSyncSettings* ECSyncSettings::s_lpInstance = NULL;


ECSyncSettings::__initializer::__initializer() {
	pthread_mutex_init(&ECSyncSettings::s_hMutex, NULL);
}

ECSyncSettings::__initializer::~__initializer() {
	if (ECSyncSettings::s_lpInstance)
		delete ECSyncSettings::s_lpInstance;

	pthread_mutex_destroy(&ECSyncSettings::s_hMutex);
}

ECSyncSettings::__initializer ECSyncSettings::__i;
