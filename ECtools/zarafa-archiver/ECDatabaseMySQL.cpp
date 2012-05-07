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

#include <iostream>
#include "ECDatabaseMySQL.h"
#include "mysqld_error.h"

#include "stringutil.h"
#include "ECDefs.h"
#include "ecversion.h"
#include "mapidefs.h"
#include "CommonUtil.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif
#ifdef DEBUG
#define DEBUG_SQL 0
#define DEBUG_TRANSACTION 0
#endif

// The maximum packet size. This is automatically also the maximum
// size of a single entry in the database.
#define MAX_ALLOWED_PACKET			16777216

#define ZA_TABLEDEF_SERVERS			"CREATE TABLE `za_servers` ( \
										`id` int(11) unsigned NOT NULL auto_increment, \
										`guid` binary(16) NOT NULL, \
										PRIMARY KEY (`id`), \
										UNIQUE KEY `guid` (`guid`) \
									) ENGINE=InnoDB"

#define ZA_TABLEDEF_INSTANCES		"CREATE TABLE `za_instances` ( \
										`id` int(11) unsigned NOT NULL auto_increment, \
										`tag` smallint(6) unsigned NOT NULL, \
										PRIMARY KEY (`id`), \
										UNIQUE KEY `instance` (`id`, `tag`) \
									) ENGINE=InnoDB"

#define ZA_TABLEDEF_MAPPINGS		"CREATE TABLE `za_mappings` ( \
										`server_id` int(11) unsigned NOT NULL, \
										`val_binary` blob NOT NULL, \
										`tag` smallint(6) unsigned NOT NULL, \
										`instance_id` int(11) unsigned NOT NULL, \
										PRIMARY KEY (`server_id`, `val_binary`(64), `tag`), \
										UNIQUE KEY `instance` (`instance_id`, `tag`, `server_id`), \
										FOREIGN KEY (`server_id`) REFERENCES za_servers(`id`) ON DELETE CASCADE, \
										FOREIGN KEY (`instance_id`, `tag`) REFERENCES za_instances(`id`, `tag`) ON UPDATE RESTRICT ON DELETE CASCADE \
									) ENGINE=InnoDB"

typedef struct _sDatabase {
	char *lpComment;
	char *lpSQL;
} sSQLDatabase_t;


ECDatabaseMySQL::ECDatabaseMySQL(ECLogger *lpLogger)
{
	m_bMysqlInitialize	= false;
	m_bConnected		= false;
	m_bLocked			= false;
	m_bAutoLock			= true;
	m_lpLogger			= lpLogger;

	// Create a mutex handle for mysql
	pthread_mutexattr_t mattr;
	pthread_mutexattr_init(&mattr);
	pthread_mutexattr_settype(&mattr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&m_hMutexMySql, &mattr);
}

ECDatabaseMySQL::~ECDatabaseMySQL()
{
	Close();

	// Close the mutex handle of mysql
	pthread_mutex_destroy(&m_hMutexMySql);
}

ECRESULT ECDatabaseMySQL::InitEngine()
{
	ECRESULT er = erSuccess;

	//Init mysql and make a connection
	if (!m_bMysqlInitialize && mysql_init(&m_lpMySQL) == NULL) {
		er = ZARAFA_E_DATABASE_ERROR;
		goto exit;
	}

	m_bMysqlInitialize = true; 

	// Set auto reconnect
	// mysql < 5.0.4 default on, mysql 5.0.4 > reconnection default off
	// Zarafa always wants to reconnect
	m_lpMySQL.reconnect = 1;

exit:
	return er;
}

ECLogger* ECDatabaseMySQL::GetLogger()
{
	ASSERT(m_lpLogger);
	return m_lpLogger;
}

ECRESULT ECDatabaseMySQL::Connect(ECConfig *lpConfig)
{
	ECRESULT		er = erSuccess;
	std::string		strQuery;
	char*			lpMysqlPort = lpConfig->GetSetting("mysql_port");
	DB_RESULT		lpDBResult = NULL;
	DB_ROW			lpDBRow = NULL;
	
	er = InitEngine();
	if(er != erSuccess)
		goto exit;

	if(mysql_real_connect(&m_lpMySQL, 
			lpConfig->GetSetting("mysql_host"), 
			lpConfig->GetSetting("mysql_user"), 
			lpConfig->GetSetting("mysql_password"), 
			lpConfig->GetSetting("mysql_database"), 
			(lpMysqlPort)?atoi(lpMysqlPort):0, NULL, 0) == NULL)
	{
		if(mysql_errno(&m_lpMySQL) == ER_BAD_DB_ERROR){ // Database does not exist
			er = ZARAFA_E_DATABASE_NOT_FOUND;
		}else
			er = ZARAFA_E_DATABASE_ERROR;
		goto exit;
	}
	
	// Check if the database is available, but empty
	strQuery = "SHOW tables";
	er = DoSelect(strQuery, &lpDBResult);
	if(er != erSuccess)
		goto exit;
		
	if(GetNumRows(lpDBResult) == 0) {
		er = ZARAFA_E_DATABASE_NOT_FOUND;
		goto exit;
	}
	
	if(lpDBResult) {
		FreeResult(lpDBResult);
		lpDBResult = NULL;
	}
	
	strQuery = "SHOW variables LIKE 'max_allowed_packet'";
	er = DoSelect(strQuery, &lpDBResult);
	if(er != erSuccess)
	    goto exit;
	
	lpDBRow = FetchRow(lpDBResult);
	if(lpDBRow == NULL || lpDBRow[0] == NULL) {
	    m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to retrieve max_allowed_packet value. Assuming 16M");
	    m_ulMaxAllowedPacket = (unsigned int)MAX_ALLOWED_PACKET;
    } else {
        m_ulMaxAllowedPacket = atoui(lpDBRow[1]);
    }

	m_bConnected = true;

	strQuery = "SET SESSION group_concat_max_len = " + stringify((unsigned int)MAX_GROUP_CONCAT_LEN);
	if(Query(strQuery) != 0 ) {
		er = ZARAFA_E_DATABASE_ERROR;
	}

exit:
	if(lpDBResult)
		FreeResult(lpDBResult);
		
	return er;
}

ECRESULT ECDatabaseMySQL::Close()
{
	ECRESULT er = erSuccess;

	_ASSERT(m_bLocked == false);
	
	//INFO: No locking here

	m_bConnected = false;

	// Close mysql data connection and deallocate data
	if(m_bMysqlInitialize)
		mysql_close(&m_lpMySQL);

	m_bMysqlInitialize = false;

	return er;
}

// Get database ownership
bool ECDatabaseMySQL::Lock()
{

	pthread_mutex_lock(&m_hMutexMySql);

	m_bLocked = true;

	return m_bLocked;
}

// Release the database ownership
bool ECDatabaseMySQL::UnLock()
{
	pthread_mutex_unlock(&m_hMutexMySql);

	m_bLocked = false;
		
	return m_bLocked;
}

bool ECDatabaseMySQL::isConnected() {

	return m_bConnected;
}

int ECDatabaseMySQL::Query(const string &strQuery) {
	int err;
	
#ifdef DEBUG_SQL
#if DEBUG_SQL
	m_lpLogger->Log(EC_LOGLEVEL_FATAL, "%p: DO_SQL: \"%s;\"", (void*)&m_lpMySQL, strQuery.c_str()); 
#endif
#endif

	// use mysql_real_query to be binary safe ( http://dev.mysql.com/doc/mysql/en/mysql-real-query.html )
	err = mysql_real_query( &m_lpMySQL, strQuery.c_str(), strQuery.length() );

	if(err) {
		m_lpLogger->Log(EC_LOGLEVEL_FATAL, "%p: SQL Failed: %s, Query: \"%s\"", (void*)&m_lpMySQL, mysql_error(&m_lpMySQL), strQuery.c_str()); 
	}

	return err;
}

ECRESULT ECDatabaseMySQL::DoSelect(const string &strQuery, DB_RESULT *lpResult) {

	ECRESULT er = erSuccess;

	_ASSERT(strQuery.length()!= 0 && lpResult != NULL);

	// Autolock, lock data
	if(m_bAutoLock)
		Lock();
		
	if( Query(strQuery) != 0 ) {
		er = ZARAFA_E_DATABASE_ERROR;
		goto exit;
	}

	*lpResult = mysql_store_result( &m_lpMySQL );
	if( *lpResult == NULL ) {
		er = ZARAFA_E_DATABASE_ERROR;
		m_lpLogger->Log(EC_LOGLEVEL_FATAL, "%p: SQL result failed: %s, Query: \"%s\"", (void*)&m_lpMySQL, mysql_error(&m_lpMySQL), strQuery.c_str()); 
	}

exit:
	// Autolock, unlock data
	if(m_bAutoLock)
		UnLock();

	return er;
}

ECRESULT ECDatabaseMySQL::DoUpdate(const string &strQuery, unsigned int *lpulAffectedRows) {
	
	ECRESULT er = erSuccess;

	// Autolock, lock data
	if(m_bAutoLock)
		Lock();
	
	er = _Update(strQuery, lpulAffectedRows);

	// Autolock, unlock data
	if(m_bAutoLock)
		UnLock();

	return er;
}

ECRESULT ECDatabaseMySQL::_Update(const string &strQuery, unsigned int *lpulAffectedRows)
{
	ECRESULT er = erSuccess;
					
	if( Query(strQuery) != 0 ) {
		// FIXME: Add the mysql error system ?
		// er = nMysqlError;
		er = ZARAFA_E_DATABASE_ERROR;
		goto exit;
	}
	
	if(lpulAffectedRows)
		*lpulAffectedRows = GetAffectedRows();
	
exit:
	return er;
}

ECRESULT ECDatabaseMySQL::DoInsert(const string &strQuery, unsigned int *lpulInsertId, unsigned int *lpulAffectedRows)
{
	ECRESULT er = erSuccess;

	// Autolock, lock data
	if(m_bAutoLock)
		Lock();
	
	er = _Update(strQuery, lpulAffectedRows);

	if (er == erSuccess) {
		if (lpulInsertId)
			*lpulInsertId = GetInsertId();
	}

	// Autolock, unlock data
	if(m_bAutoLock)
		UnLock();

	return er;
}

ECRESULT ECDatabaseMySQL::DoDelete(const string &strQuery, unsigned int *lpulAffectedRows) {

	ECRESULT er = erSuccess;

	// Autolock, lock data
	if(m_bAutoLock)
		Lock();
	
	er = _Update(strQuery, lpulAffectedRows);

	// Autolock, unlock data
	if(m_bAutoLock)
		UnLock();

	return er;
}

/*
 * This function updates a sequence in an atomic fashion - if called correctly;
 *
 * To make it work correctly, the state of the database connection should *NOT* be in a transaction; this would delay
 * committing of the data until a later time, causing other concurrent threads to possibly generate the same ID or lock
 * while waiting for this transaction to end. So, don't call Begin() before calling this function unless you really
 * know what you're doing.
 */
ECRESULT ECDatabaseMySQL::DoSequence(const std::string &strSeqName, unsigned int ulCount, unsigned long long *lpllFirstId) {
	ECRESULT er = erSuccess;
	unsigned int ulAffected = 0;

	// Attempt to update the sequence in an atomic fashion
	er = DoUpdate("UPDATE settings SET value=LAST_INSERT_ID(value+1)+" + stringify(ulCount-1) + " WHERE name = '" + strSeqName + "'", &ulAffected);
	if(er != erSuccess)
		goto exit;
	
	// If the setting was missing, insert it now, starting at sequence 1 (not 0 for safety - maybe there's some if(ulSequenceId) code somewhere)
	if(ulAffected == 0) {
		er = Query("INSERT INTO settings (name, value) VALUES('" + strSeqName + "',LAST_INSERT_ID(1)+" + stringify(ulCount-1) + ")");
		if(er != erSuccess)
			goto exit;
	}
			
	*lpllFirstId = mysql_insert_id(&m_lpMySQL);
	
exit:
	return er;
}

unsigned int ECDatabaseMySQL::GetAffectedRows() {

	return (unsigned int)mysql_affected_rows(&m_lpMySQL);
}

unsigned int ECDatabaseMySQL::GetInsertId() {

	return (unsigned int)mysql_insert_id(&m_lpMySQL);
}

void ECDatabaseMySQL::FreeResult(DB_RESULT sResult) {

	_ASSERT(sResult != NULL);

	if(sResult)
		mysql_free_result((MYSQL_RES *)sResult);
}

unsigned int ECDatabaseMySQL::GetNumRows(DB_RESULT sResult) {

	return (unsigned int)mysql_num_rows((MYSQL_RES *)sResult);
}

unsigned int ECDatabaseMySQL::GetNumRowFields(DB_RESULT sResult) {

	return mysql_num_fields((MYSQL_RES *)sResult);
}

DB_ROW ECDatabaseMySQL::FetchRow(DB_RESULT sResult) {

	return mysql_fetch_row((MYSQL_RES *)sResult);
}

DB_LENGTHS ECDatabaseMySQL::FetchRowLengths(DB_RESULT sResult) {

	return (DB_LENGTHS)mysql_fetch_lengths((MYSQL_RES *)sResult);
}

std::string ECDatabaseMySQL::Escape(const std::string &strToEscape)
{
	ULONG size = strToEscape.length()*2+1;
	char *szEscaped = new char[size];
	std::string escaped;

	memset(szEscaped, 0, size);

	mysql_real_escape_string(&this->m_lpMySQL, szEscaped, strToEscape.c_str(), strToEscape.length());

	escaped = szEscaped;

	delete [] szEscaped;

	return escaped;
}

std::string ECDatabaseMySQL::EscapeBinary(unsigned char *lpData, unsigned int ulLen)
{
	ULONG size = ulLen*2+1;
	char *szEscaped = new char[size];
	std::string escaped;
	
	memset(szEscaped, 0, size);

	mysql_real_escape_string(&this->m_lpMySQL, szEscaped, (const char *)lpData, ulLen);

	escaped = szEscaped;

	delete [] szEscaped;

	return "'" + escaped + "'";
}

std::string ECDatabaseMySQL::GetError()
{
	if(m_bMysqlInitialize == false)
		return "MYSQL not initialized";

	return mysql_error(&m_lpMySQL);
}

ECRESULT ECDatabaseMySQL::Begin() {
	int err;
	err = Query("BEGIN");
	
	if(err)
		return ZARAFA_E_DATABASE_ERROR;
	
	return erSuccess;
}

ECRESULT ECDatabaseMySQL::Commit() {
	int err;
	err = Query("COMMIT");

	if(err)
		return ZARAFA_E_DATABASE_ERROR;
	
	return erSuccess;
}

ECRESULT ECDatabaseMySQL::Rollback() {
	int err;
	err = Query("ROLLBACK");

	if(err)
		return ZARAFA_E_DATABASE_ERROR;
	
	return erSuccess;
}

unsigned int ECDatabaseMySQL::GetMaxAllowedPacket() {
    return m_ulMaxAllowedPacket;
}

ECRESULT ECDatabaseMySQL::IsInnoDBSupported()
{
	ECRESULT	er = erSuccess;
	DB_RESULT	lpResult = NULL;
	DB_ROW		lpDBRow = NULL;

	er = DoSelect("SHOW VARIABLES LIKE \"have_innodb\"", &lpResult);
	if(er != erSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_FATAL,"Unable to get value 'have_innodb' from the mysql server. Probably INNODB is not supported. Error: %s", GetError().c_str());
		goto exit;
	}

	lpDBRow = FetchRow(lpResult);
	if (lpDBRow == NULL || lpDBRow[1] == NULL) {
		m_lpLogger->Log(EC_LOGLEVEL_FATAL,"Unable to get value 'have_innodb' from the mysql server. Probably INNODB is not supported");
		er = ZARAFA_E_DATABASE_ERROR;
		goto exit;
	}

	if (stricmp(lpDBRow[1], "DISABLED") == 0) {
		m_lpLogger->Log(EC_LOGLEVEL_FATAL,"INNODB engine is disabled. Please enable the INNODB engine. Check your mysql log for more information or comment out skip-innodb in the mysql configuration file");
		er = ZARAFA_E_DATABASE_ERROR;
		goto exit;
	} else if(stricmp(lpDBRow[1], "YES") != 0 && stricmp(lpDBRow[1], "DEFAULT") != 0) {
		m_lpLogger->Log(EC_LOGLEVEL_FATAL,"INNODB engine is not support. Please enable the INNODB engine.");
		er = ZARAFA_E_DATABASE_ERROR;
		goto exit;
	}

exit:
	if(lpResult)
		FreeResult(lpResult);

	return er;
}

ECRESULT ECDatabaseMySQL::CreateDatabase(ECConfig *lpConfig)
{
	ECRESULT	er = erSuccess;
	string		strQuery;
	char*		lpDatabase = lpConfig->GetSetting("mysql_database");
	char*		lpMysqlPort = lpConfig->GetSetting("mysql_port");
	char*		lpMysqlSocket = lpConfig->GetSetting("mysql_socket");

	if(*lpMysqlSocket == '\0')
		lpMysqlSocket = NULL;

	// Zarafa archiver database tables
	sSQLDatabase_t sDatabaseTables[] = {
		{"servers", ZA_TABLEDEF_SERVERS},
		{"instances", ZA_TABLEDEF_INSTANCES},
		{"mappings", ZA_TABLEDEF_MAPPINGS}
	};

	er = InitEngine();
	if(er != erSuccess)
		goto exit;

	// Connect
	if(mysql_real_connect(&m_lpMySQL, 
			lpConfig->GetSetting("mysql_host"), 
			lpConfig->GetSetting("mysql_user"), 
			lpConfig->GetSetting("mysql_password"), 
			NULL, 
			(lpMysqlPort)?atoi(lpMysqlPort):0, 
			lpMysqlSocket, 0) == NULL)
	{
		er = ZARAFA_E_DATABASE_ERROR;
		goto exit;
	}

	if(lpDatabase == NULL) {
		m_lpLogger->Log(EC_LOGLEVEL_FATAL,"Unable to create database: Unknown database");
		er = ZARAFA_E_DATABASE_ERROR;
		goto exit;
	}

	m_lpLogger->Log(EC_LOGLEVEL_NOTICE,"Create database %s", lpDatabase);

	er = IsInnoDBSupported();
	if(er != erSuccess)
		goto exit;

	strQuery = "CREATE DATABASE IF NOT EXISTS `"+std::string(lpConfig->GetSetting("mysql_database"))+"`";
	if(Query(strQuery) != erSuccess){
		m_lpLogger->Log(EC_LOGLEVEL_FATAL,"Unable to create database: %s", GetError().c_str());
		er = ZARAFA_E_DATABASE_ERROR;
		goto exit;
	}

	strQuery = "USE `"+std::string(lpConfig->GetSetting("mysql_database"))+"`";
	er = DoInsert(strQuery);
	if(er != erSuccess)
		goto exit;

	// Database tables
	for (unsigned int i=0; i < arraySize(sDatabaseTables); i++)
	{
		m_lpLogger->Log(EC_LOGLEVEL_NOTICE,"Create table: %s", sDatabaseTables[i].lpComment);
		er = DoInsert(sDatabaseTables[i].lpSQL);
		if(er != erSuccess)
			goto exit;	
	}

	m_lpLogger->Log(EC_LOGLEVEL_NOTICE,"Database is created");

exit:
	return er;
}
