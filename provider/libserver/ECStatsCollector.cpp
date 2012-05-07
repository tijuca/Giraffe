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
#include "ECStatsCollector.h"
#include "stringutil.h"

using namespace std;

ECStatsCollector::ECStatsCollector() {
	pthread_mutex_init(&m_StringsLock, NULL);

	// the 'name' parameter may not be longer than 19 characters, since we want to use those in RRDtool
 	AddStat(SCN_SERVER_STARTTIME, SCDT_TIMESTAMP, "server_start_date", "Time when the server was started");
 	AddStat(SCN_SERVER_LAST_CACHECLEARED, SCDT_TIMESTAMP, "cache_purge_date", "Time when the cache was cleared");
 	AddStat(SCN_SERVER_LAST_CONFIGRELOAD, SCDT_TIMESTAMP, "config_reload_date", "Time when the configuration file was reloaded / logrotation (SIGHUP)");
 	AddStat(SCN_SERVER_CONNECTIONS, SCDT_LONGLONG, "connections", "Number of handled incoming connections");
 	AddStat(SCN_MAX_SOCKET_NUMBER, SCDT_LONGLONG, "max_socket", "Highest socket number used");
 	AddStat(SCN_REDIRECT_COUNT, SCDT_LONGLONG, "redirections", "Number of redirected requests");
 	AddStat(SCN_SEARCHFOLDER_COUNT, SCDT_LONGLONG, "searchfld_loaded", "Total number of searchfolders");
 	AddStat(SCN_SEARCHFOLDER_THREADS, SCDT_LONGLONG, "searchfld_threads", "Current number of running searchfolder threads");
 	AddStat(SCN_SEARCHFOLDER_UPDATE_RETRY, SCDT_LONGLONG, "searchupd_retry", "The number of times a search folder update was restarted");
 	AddStat(SCN_SEARCHFOLDER_UPDATE_FAIL, SCDT_LONGLONG, "searchupd_fail", "The number of failed search folder updates after retrying");
 	AddStat(SCN_SOAP_REQUESTS, SCDT_LONGLONG, "soap_request", "Number of soap requests handled by server");
 	AddStat(SCN_RESPONSE_TIME, SCDT_LONGLONG, "response_time", "Response time of soap requests handled in milliseconds (includes time in queue)");
 	AddStat(SCN_PROCESSING_TIME, SCDT_LONGLONG, "processing_time", "Time taken to process soap requests in milliseconds (wallclock time)");
 
 	AddStat(SCN_DATABASE_CONNECTS, SCDT_LONGLONG, "sql_connect", "Number of connections made to SQL server");
 	AddStat(SCN_DATABASE_SELECTS, SCDT_LONGLONG, "sql_select", "Number of SQL Select commands executed");
 	AddStat(SCN_DATABASE_INSERTS, SCDT_LONGLONG, "sql_insert", "Number of SQL Insert commands executed");
 	AddStat(SCN_DATABASE_UPDATES, SCDT_LONGLONG, "sql_update", "Number of SQL Update commands executed");
 	AddStat(SCN_DATABASE_DELETES, SCDT_LONGLONG, "sql_delete", "Number of SQL Delete commands executed");
 	AddStat(SCN_DATABASE_FAILED_CONNECTS, SCDT_LONGLONG, "sql_connect_fail", "Number of failed connections made to SQL server");
 	AddStat(SCN_DATABASE_FAILED_SELECTS, SCDT_LONGLONG, "sql_select_fail", "Number of failed SQL Select commands");
 	AddStat(SCN_DATABASE_FAILED_INSERTS, SCDT_LONGLONG, "sql_insert_fail", "Number of failed SQL Insert commands");
 	AddStat(SCN_DATABASE_FAILED_UPDATES, SCDT_LONGLONG, "sql_update_fail", "Number of failed SQL Update commands");
 	AddStat(SCN_DATABASE_FAILED_DELETES, SCDT_LONGLONG, "sql_delete_fail", "Number of failed SQL Delete commands");
 	AddStat(SCN_DATABASE_LAST_FAILED, SCDT_TIMESTAMP, "sql_last_fail_time", "Timestamp of last failed SQL command");
 	AddStat(SCN_DATABASE_MWOPS, SCDT_LONGLONG, "mwops", "MAPI Write Operations");
 	AddStat(SCN_DATABASE_MROPS, SCDT_LONGLONG, "mrops", "MAPI Read Operations");
 	AddStat(SCN_DATABASE_DEFERRED_FETCHES, SCDT_LONGLONG, "deferred_fetches", "Number rows retrieved via deferred write table");
 	AddStat(SCN_DATABASE_MERGES, SCDT_LONGLONG, "deferred_merges", "Number of merges applied to the deferred write table");
 	AddStat(SCN_DATABASE_MERGED_RECORDS, SCDT_LONGLONG, "deferred_records", "Number records merged in the deferred write table");
 	AddStat(SCN_DATABASE_ROW_READS, SCDT_LONGLONG, "row_reads", "Number of table rows read in row order");
 	AddStat(SCN_DATABASE_COUNTER_RESYNCS, SCDT_LONGLONG, "counter_resyncs", "Number of time a counter resync was required");
 
 	AddStat(SCN_LOGIN_PASSWORD, SCDT_LONGLONG, "login_password", "Number of logins through password authentication");
 	AddStat(SCN_LOGIN_SSL, SCDT_LONGLONG, "login_ssl", "Number of logins through SSL certificate authentication");
 	AddStat(SCN_LOGIN_SSO, SCDT_LONGLONG, "login_sso", "Number of logins through Single Sign-on");
 	AddStat(SCN_LOGIN_SOCKET, SCDT_LONGLONG, "login_unix", "Number of logins through Unix socket");
 	AddStat(SCN_LOGIN_DENIED, SCDT_LONGLONG, "login_failed", "Number of failed logins");
 
 	AddStat(SCN_SESSIONS_CREATED, SCDT_LONGLONG, "sessions_created", "Number of created sessions");
 	AddStat(SCN_SESSIONS_DELETED, SCDT_LONGLONG, "sessions_deleted", "Number of deleted sessions");
 	AddStat(SCN_SESSIONS_TIMEOUT, SCDT_LONGLONG, "sessions_timeout", "Number of timed-out sessions");
 
 	AddStat(SCN_SESSIONS_INTERNAL_CREATED, SCDT_LONGLONG, "sess_int_created", "Number of created internal sessions");
 	AddStat(SCN_SESSIONS_INTERNAL_DELETED, SCDT_LONGLONG, "sess_int_deleted", "Number of deleted internal sessions");
 
 	AddStat(SCN_SESSIONGROUPS_CREATED, SCDT_LONGLONG, "sess_grp_created", "Number of created sessiongroups");
 	AddStat(SCN_SESSIONGROUPS_DELETED, SCDT_LONGLONG, "sess_grp_deleted", "Number of deleted sessiongroups");
 
 	AddStat(SCN_LDAP_CONNECTS, SCDT_LONGLONG, "ldap_connect", "Number of connections made to LDAP server");
 	AddStat(SCN_LDAP_RECONNECTS, SCDT_LONGLONG, "ldap_reconnect", "Number of re-connections made to LDAP server");
 	AddStat(SCN_LDAP_CONNECT_FAILED, SCDT_LONGLONG, "ldap_connect_fail", "Number of failed connections made to LDAP server");
 	AddStat(SCN_LDAP_CONNECT_TIME, SCDT_LONGLONG, "ldap_connect_time", "Total duration of connections made to LDAP server");
 	AddStat(SCN_LDAP_CONNECT_TIME_MAX, SCDT_LONGLONG, "ldap_max_connect", "Longest connection time made to LDAP server");
 	
 	/* maybe usesless because SCN_LOGIN_* */
 	AddStat(SCN_LDAP_AUTH_LOGINS, SCDT_LONGLONG, "ldap_auth", "Number of LDAP authentications");
 	AddStat(SCN_LDAP_AUTH_DENIED, SCDT_LONGLONG, "ldap_auth_fail", "Number of failed authentications");
 	AddStat(SCN_LDAP_AUTH_TIME, SCDT_LONGLONG, "ldap_auth_time", "Total authentication time");
 	AddStat(SCN_LDAP_AUTH_TIME_MAX, SCDT_LONGLONG, "ldap_max_auth", "Longest duration of authentication made to LDAP server");
 	AddStat(SCN_LDAP_AUTH_TIME_AVG, SCDT_LONGLONG, "ldap_avg_auth", "Average duration of authentication made to LDAP server");
 
 	AddStat(SCN_LDAP_SEARCH, SCDT_LONGLONG, "ldap_search", "Number of searches made to LDAP server");
 	AddStat(SCN_LDAP_SEARCH_FAILED, SCDT_LONGLONG, "ldap_search_fail", "Number of failed searches made to LDAP server");
 	AddStat(SCN_LDAP_SEARCH_TIME, SCDT_LONGLONG, "ldap_search_time", "Total duration of LDAP searches");
 	AddStat(SCN_LDAP_SEARCH_TIME_MAX, SCDT_LONGLONG, "ldap_max_search", "Longest duration of LDAP search");

	AddStat(SCN_INDEXER_SEARCH_ERRORS, SCDT_LONGLONG, "index_search_errors", "Number of failed indexer queries");
	AddStat(SCN_INDEXER_SEARCH_MAX, SCDT_LONGLONG, "index_search_max", "Maximum duration of an indexed search query");
	AddStat(SCN_INDEXER_SEARCH_AVG, SCDT_LONGLONG, "index_search_avg", "Average duration of an indexed search query");
	AddStat(SCN_INDEXED_SEARCHES, SCDT_LONGLONG, "search_indexed", "Number of indexed searches performed");
	AddStat(SCN_DATABASE_SEARCHES, SCDT_LONGLONG, "search_database", "Number of database searches performed");
}

ECStatsCollector::~ECStatsCollector() {
	SCMap::iterator iSD;

	for (iSD = m_StatData.begin(); iSD != m_StatData.end(); iSD++) {
		pthread_mutex_destroy(&iSD->second.lock);
	}
	pthread_mutex_destroy(&m_StringsLock);
}

void ECStatsCollector::AddStat(SCName index, SCType type, const char *name, const char *description) {
	ECStat newStat;

	newStat.data.ll = 0;		// reset largest data var in union
	newStat.avginc = 1;
	newStat.type = type;
	pthread_mutex_init(&newStat.lock, NULL);
	newStat.name = name;
	newStat.description = description;

	m_StatData[index] = newStat;
}

void ECStatsCollector::Increment(SCName name, float inc) {
	map<SCName, ECStat>::iterator iSD = m_StatData.find(name);
	if (iSD == m_StatData.end())
		return;

	ASSERT(iSD->second.type == SCDT_FLOAT);

	pthread_mutex_lock(&iSD->second.lock);
	iSD->second.data.f += inc;
	pthread_mutex_unlock(&iSD->second.lock);
}

void ECStatsCollector::Increment(SCName name, int inc) {
	Increment(name, (LONGLONG)inc);
}

void ECStatsCollector::Increment(SCName name, LONGLONG inc) {
	map<SCName, ECStat>::iterator iSD = m_StatData.find(name);
	if (iSD == m_StatData.end())
		return;

	ASSERT(iSD->second.type == SCDT_LONGLONG);

	pthread_mutex_lock(&iSD->second.lock);
	iSD->second.data.ll += inc;
	pthread_mutex_unlock(&iSD->second.lock);
}

void ECStatsCollector::Set(SCName name, float set) {
	map<SCName, ECStat>::iterator iSD = m_StatData.find(name);
	if (iSD == m_StatData.end())
		return;

	ASSERT(iSD->second.type == SCDT_FLOAT);

	pthread_mutex_lock(&iSD->second.lock);
	iSD->second.data.f = set;
	pthread_mutex_unlock(&iSD->second.lock);
}

void ECStatsCollector::Set(SCName name, LONGLONG set) {
	map<SCName, ECStat>::iterator iSD = m_StatData.find(name);
	if (iSD == m_StatData.end())
		return;

	ASSERT(iSD->second.type == SCDT_LONGLONG);

	pthread_mutex_lock(&iSD->second.lock);
	iSD->second.data.ll = set;
	pthread_mutex_unlock(&iSD->second.lock);
}

void ECStatsCollector::SetTime(SCName name, time_t set) {
	map<SCName, ECStat>::iterator iSD = m_StatData.find(name);
	if (iSD == m_StatData.end())
		return;

	ASSERT(iSD->second.type == SCDT_TIMESTAMP);

	pthread_mutex_lock(&iSD->second.lock);
	iSD->second.data.ts = set;
	pthread_mutex_unlock(&iSD->second.lock);
}

void ECStatsCollector::Min(SCName name, float min)
{
	map<SCName, ECStat>::iterator iSD = m_StatData.find(name);
	if (iSD == m_StatData.end())
		return;

	ASSERT(iSD->second.type == SCDT_FLOAT);

	pthread_mutex_lock(&iSD->second.lock);
	if (iSD->second.data.f > min)
		iSD->second.data.f = min;
	pthread_mutex_unlock(&iSD->second.lock);	
}

void ECStatsCollector::Min(SCName name, LONGLONG min)
{
	map<SCName, ECStat>::iterator iSD = m_StatData.find(name);
	if (iSD == m_StatData.end())
		return;

	ASSERT(iSD->second.type == SCDT_LONGLONG);

	pthread_mutex_lock(&iSD->second.lock);
	if (iSD->second.data.ll > min)
		iSD->second.data.ll = min;
	pthread_mutex_unlock(&iSD->second.lock);	
}

void ECStatsCollector::MinTime(SCName name, time_t min)
{
	map<SCName, ECStat>::iterator iSD = m_StatData.find(name);
	if (iSD == m_StatData.end())
		return;

	ASSERT(iSD->second.type == SCDT_TIMESTAMP);

	pthread_mutex_lock(&iSD->second.lock);
	if (iSD->second.data.ts > min)
		iSD->second.data.ts = min;
	pthread_mutex_unlock(&iSD->second.lock);	
}


void ECStatsCollector::Max(SCName name, float max)
{
	map<SCName, ECStat>::iterator iSD = m_StatData.find(name);
	if (iSD == m_StatData.end())
		return;

	ASSERT(iSD->second.type == SCDT_FLOAT);

	pthread_mutex_lock(&iSD->second.lock);
	if (iSD->second.data.f < max)
		iSD->second.data.f = max;
	pthread_mutex_unlock(&iSD->second.lock);	
}

void ECStatsCollector::Max(SCName name, LONGLONG max)
{
	map<SCName, ECStat>::iterator iSD = m_StatData.find(name);
	if (iSD == m_StatData.end())
		return;

	ASSERT(iSD->second.type == SCDT_LONGLONG);

	pthread_mutex_lock(&iSD->second.lock);
	if (iSD->second.data.ll < max)
		iSD->second.data.ll = max;
	pthread_mutex_unlock(&iSD->second.lock);	
}

void ECStatsCollector::MaxTime(SCName name, time_t max)
{
	map<SCName, ECStat>::iterator iSD = m_StatData.find(name);
	if (iSD == m_StatData.end())
		return;

	ASSERT(iSD->second.type == SCDT_TIMESTAMP);

	pthread_mutex_lock(&iSD->second.lock);
	if (iSD->second.data.ts < max)
		iSD->second.data.ts = max;
	pthread_mutex_unlock(&iSD->second.lock);	
}


void ECStatsCollector::Avg(SCName name, float add)
{
	map<SCName, ECStat>::iterator iSD = m_StatData.find(name);
	if (iSD == m_StatData.end())
		return;

	ASSERT(iSD->second.type == SCDT_FLOAT);

	pthread_mutex_lock(&iSD->second.lock);
	iSD->second.data.f = ((add - iSD->second.data.f) / iSD->second.avginc) + iSD->second.data.f;
	iSD->second.avginc++;
	if (iSD->second.avginc == 0)
		iSD->second.avginc = 1;
	pthread_mutex_unlock(&iSD->second.lock);	
}

void ECStatsCollector::Avg(SCName name, LONGLONG add)
{
	map<SCName, ECStat>::iterator iSD = m_StatData.find(name);
	if (iSD == m_StatData.end())
		return;

	ASSERT(iSD->second.type == SCDT_LONGLONG);

	pthread_mutex_lock(&iSD->second.lock);
	iSD->second.data.ll = ((add - iSD->second.data.ll) / iSD->second.avginc) + iSD->second.data.ll;
	iSD->second.avginc++;
	if (iSD->second.avginc == 0)
		iSD->second.avginc = 1;
	pthread_mutex_unlock(&iSD->second.lock);	
}

void ECStatsCollector::AvgTime(SCName name, time_t add)
{
	map<SCName, ECStat>::iterator iSD = m_StatData.find(name);
	if (iSD == m_StatData.end())
		return;

	ASSERT(iSD->second.type == SCDT_TIMESTAMP);

	pthread_mutex_lock(&iSD->second.lock);
	iSD->second.data.ts = ((add - iSD->second.data.ts) / iSD->second.avginc) + iSD->second.data.ts;
	iSD->second.avginc++;
	if (iSD->second.avginc == 0)
		iSD->second.avginc = 1;
	pthread_mutex_unlock(&iSD->second.lock);	
}



void ECStatsCollector::Set(const std::string &name, const std::string &description, const std::string &value)
{
	ECStrings data;

	data.description = description;
	data.value = value;

	pthread_mutex_lock(&m_StringsLock);
	m_StatStrings[name] = data;
	pthread_mutex_unlock(&m_StringsLock);
}

void ECStatsCollector::Remove(const std::string &name)
{
	pthread_mutex_lock(&m_StringsLock);
	m_StatStrings.erase(name);
	pthread_mutex_unlock(&m_StringsLock);
}

std::string ECStatsCollector::GetValue(SCMap::iterator iSD)
{
	std::string rv;
	char timestamp[32];			// man page: at least 26
	int len;

	switch(iSD->second.type) {
	case SCDT_FLOAT:
		rv = stringify_float(iSD->second.data.f);
		break;
	case SCDT_LONGLONG:
		rv = stringify_int64(iSD->second.data.ll);
		break;
	case SCDT_TIMESTAMP:
		if (iSD->second.data.ts > 0) {
			ctime_r(&iSD->second.data.ts, timestamp);
			len = strlen(timestamp);
			timestamp[len-1] = '\0'; // chop off trailing \n
			rv = timestamp;
		}
		break;
	}

	return rv;
}

std::string ECStatsCollector::GetValue(SCName name) {
	std::string rv;
	SCMap::iterator iSD = m_StatData.find(name);

	if (iSD != m_StatData.end())
		rv = GetValue(iSD);

	return rv;
}

void ECStatsCollector::ForEachStat(void(callback)(const std::string &, const std::string &, const std::string &, void*), void *obj)
{
	SCMap::iterator iSD;

	for (iSD = m_StatData.begin(); iSD != m_StatData.end(); iSD++) {
		pthread_mutex_lock(&iSD->second.lock);
		callback(iSD->second.name, iSD->second.description, GetValue(iSD), obj);
		pthread_mutex_unlock(&iSD->second.lock);
	}
}

void ECStatsCollector::ForEachString(void(callback)(const std::string &, const std::string &, const std::string &, void*), void *obj)
{
	std::map<std::string, ECStrings>::iterator iSS;

	pthread_mutex_lock(&m_StringsLock);
	for (iSS = m_StatStrings.begin(); iSS != m_StatStrings.end(); iSS++) {
		callback(iSS->first, iSS->second.description, iSS->second.value, obj);
	}
	pthread_mutex_unlock(&m_StringsLock);
}

void ECStatsCollector::Reset() {
	SCMap::iterator iSD;

	for (iSD = m_StatData.begin(); iSD != m_StatData.end(); iSD++) {
		// reset largest var in union
		iSD->second.data.ll = 0;
	}
}

void ECStatsCollector::Reset(SCName name) {
	SCMap::iterator iSD = m_StatData.find(name);

	if (iSD != m_StatData.end()) {
		// reset largest var in union
		iSD->second.data.ll = 0;
	}
}
