/*
 * Copyright 2005-2016 Zarafa and its licensors
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License, version 3, as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include "config.h"
#include <memory>
#include <string>
#include <utility>
#include <cassert>
#include <cstring>
#include <mysql.h>
#include <mysqld_error.h>
#include <kopano/ECConfig.h>
#include <kopano/ECLogger.h>
#include <kopano/database.hpp>
#include <kopano/stringutil.h>
#define LOG_SQL_DEBUG(_msg, ...) \
	ec_log(EC_LOGLEVEL_DEBUG | EC_LOGLEVEL_SQL, _msg, ##__VA_ARGS__)

DB_RESULT::~DB_RESULT(void)
{
	if (m_res == nullptr)
		return;
	assert(m_db != nullptr);
	if (m_db == nullptr)
		return;
	m_db->FreeResult_internal(m_res);
	m_res = nullptr;
}

DB_RESULT &DB_RESULT::operator=(DB_RESULT &&o)
{
	if (m_res != nullptr) {
		assert(m_db != nullptr);
		m_db->FreeResult_internal(m_res);
	}
	m_res = o.m_res;
	m_db = o.m_db;
	o.m_res = nullptr;
	o.m_db = nullptr;
	return *this;
}

size_t DB_RESULT::get_num_rows(void) const
{
	return mysql_num_rows(static_cast<MYSQL_RES *>(m_res));
}

DB_ROW DB_RESULT::fetch_row(void)
{
	return mysql_fetch_row(static_cast<MYSQL_RES *>(m_res));
}

DB_LENGTHS DB_RESULT::fetch_row_lengths(void)
{
	return mysql_fetch_lengths(static_cast<MYSQL_RES *>(m_res));
}

KDatabase::KDatabase(void)
{
	memset(&m_lpMySQL, 0, sizeof(m_lpMySQL));
}

ECRESULT KDatabase::Connect(ECConfig *cfg, bool reconnect,
    unsigned int mysql_flags, unsigned int gcm)
{
	const char *mysql_port = cfg->GetSetting("mysql_port");
	const char *mysql_socket = cfg->GetSetting("mysql_socket");
	DB_RESULT result;
	DB_ROW row = nullptr;
	std::string query;

	if (*mysql_socket == '\0')
		mysql_socket = nullptr;
	auto er = InitEngine(reconnect);
	if (er != erSuccess) {
		ec_log_crit("KDatabase::Connect(): InitEngine failed %d", er);
		goto exit;
	}
	if (mysql_real_connect(&m_lpMySQL, cfg->GetSetting("mysql_host"),
	    cfg->GetSetting("mysql_user"), cfg->GetSetting("mysql_password"),
	    cfg->GetSetting("mysql_database"),
	    mysql_port ? atoi(mysql_port) : 0,
	    mysql_socket, mysql_flags) == nullptr) {
		if (mysql_errno(&m_lpMySQL) == ER_BAD_DB_ERROR)
			/* Database does not exist */
			er = KCERR_DATABASE_NOT_FOUND;
		else
			er = KCERR_DATABASE_ERROR;
		ec_log_err("KDatabase::Connect(): database access error %d, mysql error: %s",
			er, GetError());
		goto exit;
	}

	// Check if the database is available, but empty
	er = DoSelect("SHOW tables", &result);
	if (er != erSuccess) {
		ec_log_err("KDatabase::Connect(): \"SHOW tables\" failed %d", er);
		goto exit;
	}
	if (result.get_num_rows() == 0) {
		er = KCERR_DATABASE_NOT_FOUND;
		ec_log_err("KDatabase::Connect(): database missing %d", er);
		goto exit;
	}

	query = "SHOW variables LIKE 'max_allowed_packet'";
	er = DoSelect(query, &result);
	if (er != erSuccess) {
		ec_log_err("KDatabase::Connect(): max_allowed_packet retrieval failed %d", er);
		goto exit;
	}

	row = result.fetch_row();
	/* row[0] has the variable name, [1] the value */
	if (row == nullptr || row[0] == nullptr || row[1] == nullptr) {
		ec_log_warn("Unable to retrieve max_allowed_packet value. Assuming %d.", KC_DFL_MAX_PACKET_SIZE);
		m_ulMaxAllowedPacket = KC_DFL_MAX_PACKET_SIZE;
	} else {
		m_ulMaxAllowedPacket = atoui(row[1]);
	}
	/*
	 * Changing the per-session "max_allowed_packet" is not permitted since
	 * MySQL 5.1, and the global one is for superusers only. Give a warning
	 * instead, then.
	 */
	if (m_ulMaxAllowedPacket < KC_DFL_MAX_PACKET_SIZE)
		ec_log_warn("max_allowed_packet is smaller than 16M (%d). You are advised to increase this value by adding max_allowed_packet=16M in the [mysqld] section of my.cnf.", m_ulMaxAllowedPacket);

	m_bConnected = true;
	if (mysql_set_character_set(&m_lpMySQL, "utf8")) {
		ec_log_err("Unable to set character set to \"utf8\"");
		er = KCERR_DATABASE_ERROR;
		goto exit;
	}

	query = "SET SESSION group_concat_max_len = " + stringify(gcm);
	if (Query(query) != 0) {
		ec_log_crit("KDatabase::Connect(): group_concat_max_len set fail: %s", GetError());
		er = KCERR_DATABASE_ERROR;
		goto exit;
	}
 exit:
	if (er != erSuccess)
		Close();
	return er;
}

ECRESULT KDatabase::CreateDatabase(ECConfig *cfg, bool reconnect)
{
	const char *dbname = cfg->GetSetting("mysql_database");
	const char *mysql_port = cfg->GetSetting("mysql_port");
	const char *mysql_socket = cfg->GetSetting("mysql_socket");

	if (*mysql_socket == '\0')
		mysql_socket = nullptr;

	// Kopano archiver database tables
	auto er = InitEngine(reconnect);
	if (er != erSuccess)
		return er;

	// Connect
	// m_lpMySQL: address of an existing MYSQL, Before calling
	// mysql_real_connect(), call mysql_init() to initialize the
	// MYSQL structure.
	if (mysql_real_connect(&m_lpMySQL, cfg->GetSetting("mysql_host"),
	    cfg->GetSetting("mysql_user"), cfg->GetSetting("mysql_password"),
	    nullptr, mysql_port != nullptr ? atoi(mysql_port) : 0,
	    mysql_socket, 0) == nullptr) {
		ec_log_err("Failed to connect to database: %s", GetError());
		return KCERR_DATABASE_ERROR;
	}
	if (dbname == nullptr) {
		ec_log_crit("Unable to create database: Unknown database");
		return KCERR_DATABASE_ERROR;
	}
	ec_log_notice("Create database %s", dbname);
	er = IsInnoDBSupported();
	if (er != erSuccess)
		return er;

	std::string query;
	query = "CREATE DATABASE IF NOT EXISTS `" +
	        std::string(cfg->GetSetting("mysql_database")) + "`";
	if (Query(query) != erSuccess) {
		ec_log_err("Unable to create database: %s", GetError());
		return KCERR_DATABASE_ERROR;
	}
	query = "USE `" + std::string(cfg->GetSetting("mysql_database")) + "`";
	er = DoInsert(query);
	if (er != erSuccess)
		return er;

	ec_log_info("Database structure has been created");
	return erSuccess;
}

ECRESULT KDatabase::CreateTables(void)
{
	auto tables = GetDatabaseDefs();

	for (size_t i = 0; tables[i].lpSQL != nullptr; ++i) {
		DB_RESULT result;
		auto query = format("SHOW tables LIKE '%s'", tables[i].lpComment);
		auto er = DoSelect(query, &result);
		if (er != erSuccess) {
			ec_log_err("Error running query %s", query.c_str());
			return er;
		}

		if (result.get_num_rows() > 0) {
			ec_log_debug("Table \"%s\" exists", tables[i].lpComment);
			continue;
		}

		ec_log_info("Create table: %s", tables[i].lpComment);

		er = DoInsert(tables[i].lpSQL);
		if (er != erSuccess)
			return er;
	}

	return erSuccess;
}

ECRESULT KDatabase::Close(void)
{
	/* No locking here */
	m_bConnected = false;
	if (m_bMysqlInitialize)
		mysql_close(&m_lpMySQL);
	m_bMysqlInitialize = false;
	return erSuccess;
}

/**
 * Perform a DELETE operation on the database
 * @q: (in) INSERT query string
 * @aff: (out) (optional) Receives the number of deleted rows
 *
 * Sends the passed DELETE query to the MySQL server, and optionally the number
 * of deleted rows. Returns erSuccess or %KCERR_DATABASE_ERROR.
 */
ECRESULT KDatabase::DoDelete(const std::string &q, unsigned int *aff)
{
	autolock alk(*this);
	return I_Update(q, aff);
}

/**
 * Perform an INSERT operation on the database
 * @q: (in) INSERT query string
 * @idp: (out) (optional) Receives the last insert id
 * @aff: (out) (optional) Receives the number of inserted rows
 *
 * Sends the passed INSERT query to the MySQL server, and optionally returns
 * the new insert ID and the number of inserted rows.
 *
 * Returns erSuccess or %KCERR_DATABASE_ERROR.
 */
ECRESULT KDatabase::DoInsert(const std::string &q, unsigned int *idp,
    unsigned int *aff)
{
	autolock alk(*this);
	auto er = I_Update(q, aff);
	if (er == erSuccess && idp != nullptr)
		*idp = GetInsertId();
	return er;
}

/**
 * Perform a SELECT operation on the database
 * @q: (in) SELECT query string
 * @res_p: (out) Result output
 * @stream: (in) Whether data should be streamed instead of stored
 *
 * Sends the passed SELECT-like (any operation that outputs a result set) query
 * to the MySQL server and retrieves the result.
 *
 * Setting @stream will delay retrieving data from the network until FetchRow()
 * is called. The only drawback is that GetRowCount() can therefore not be used
 * unless all rows are fetched first. The main reason to use this is to
 * conserve memory and increase pipelining (the client can start processing
 * data before the server has completed the query)
 *
 * Returns erSuccess or %KCERR_DATABASE_ERROR.
 */
ECRESULT KDatabase::DoSelect(const std::string &q, DB_RESULT *res_p,
    bool stream)
{
	assert(q.length() != 0);
	autolock alk(*this);

	if (Query(q) != erSuccess) {
		ec_log_err("KDatabase::DoSelect(): query failed: %s: %s", q.c_str(), GetError());
		return KCERR_DATABASE_ERROR;
	}

	ECRESULT er = erSuccess;
	DB_RESULT res;
	if (stream)
		res = DB_RESULT(this, mysql_use_result(&m_lpMySQL));
	else
		res = DB_RESULT(this, mysql_store_result(&m_lpMySQL));
	if (res == nullptr) {
		if (!m_bSuppressLockErrorLogging ||
		    GetLastError() == DB_E_UNKNOWN)
			ec_log_err("SQL [%08lu] result failed: %s, Query: \"%s\"",
				m_lpMySQL.thread_id, mysql_error(&m_lpMySQL), q.c_str());
		er = KCERR_DATABASE_ERROR;
	}
	if (res_p != nullptr)
		*res_p = std::move(res);
	return er;
}

/**
 * This function updates a sequence in an atomic fashion - if called correctly;
 *
 * To make it work correctly, the state of the database connection should *NOT*
 * be in a transaction; this would delay committing of the data until a later
 * time, causing other concurrent threads to possibly generate the same ID or
 * lock while waiting for this transaction to end. So, do not call Begin()
 * before calling this function unless you really know what you are doing.
 *
 * TODO: Measure sequence update calls, currently it is an update.
 */
ECRESULT KDatabase::DoSequence(const std::string &seq, unsigned int count,
    unsigned long long *firstidp)
{
	unsigned int aff = 0;
	autolock alk(*this);

	/* Attempt to update the sequence in an atomic fashion */
	auto er = DoUpdate("UPDATE settings SET value=LAST_INSERT_ID(value+1)+" +
	          stringify(count - 1) + " WHERE name = '" + seq + "'", &aff);
	if (er != erSuccess) {
		ec_log_err("KDatabase::DoSequence() UPDATE failed %d", er);
		return er;
	}
	/*
	 * If the setting was missing, insert it now, starting at sequence 1
	 * (not 0 for safety - maybe there is some if(ulSequenceId) code
	 * somewhere).
	 */
	if (aff == 0) {
		er = Query("INSERT INTO settings (name, value) VALUES('" +
		     seq + "',LAST_INSERT_ID(1)+" + stringify(count - 1) + ")");
		if (er != erSuccess) {
			ec_log_crit("KDatabase::DoSequence() INSERT INTO failed %d", er);
			return er;
		}
	}
	*firstidp = mysql_insert_id(&m_lpMySQL);
	return er;
}

/**
 * Perform an UPDATE operation on the database
 * @q: (in) UPDATE query string
 * @aff: (out) (optional) Receives the number of affected rows
 *
 * Sends the passed UPDATE query to the MySQL server, and optionally returns
 * the number of affected rows. The affected rows is the number of rows that
 * have been MODIFIED, which is not necessarily the number of rows that MATCHED
 * the WHERE clause.
 *
 * Returns erSuccess or %KCERR_DATABASE_ERROR.
 */
ECRESULT KDatabase::DoUpdate(const std::string &q, unsigned int *aff)
{
	autolock alk(*this);
	return I_Update(q, aff);
}

std::string KDatabase::Escape(const std::string &s)
{
	auto size = s.length() * 2 + 1;
	std::unique_ptr<char[]> esc(new char[size]);

	memset(esc.get(), 0, size);
	mysql_real_escape_string(&m_lpMySQL, esc.get(), s.c_str(), s.length());
	return esc.get();
}

std::string KDatabase::EscapeBinary(const unsigned char *data, size_t len)
{
	auto size = len * 2 + 1;
	std::unique_ptr<char[]> esc(new char[size]);

	memset(esc.get(), 0, size);
	mysql_real_escape_string(&m_lpMySQL, esc.get(), reinterpret_cast<const char *>(data), len);
	return "'" + std::string(esc.get()) + "'";
}

std::string KDatabase::EscapeBinary(const std::string &s)
{
	return EscapeBinary(reinterpret_cast<const unsigned char *>(s.c_str()), s.size());
}

void KDatabase::FreeResult_internal(void *r)
{
	assert(r != nullptr);
	if (r != nullptr)
		mysql_free_result(static_cast<MYSQL_RES *>(r));
}

unsigned int KDatabase::GetAffectedRows(void)
{
	return mysql_affected_rows(&m_lpMySQL);
}

const char *KDatabase::GetError(void)
{
	if (!m_bMysqlInitialize)
		return "MYSQL not initialized";
	return mysql_error(&m_lpMySQL);
}

unsigned int KDatabase::GetInsertId(void)
{
	return mysql_insert_id(&m_lpMySQL);
}

DB_ERROR KDatabase::GetLastError(void)
{
	switch (mysql_errno(&m_lpMySQL)) {
	case ER_LOCK_WAIT_TIMEOUT:
		return DB_E_LOCK_WAIT_TIMEOUT;
	case ER_LOCK_DEADLOCK:
		return DB_E_LOCK_DEADLOCK;
	default:
		return DB_E_UNKNOWN;
	}
}

ECRESULT KDatabase::InitEngine(bool reconnect)
{
	assert(!m_bMysqlInitialize);
	if (!m_bMysqlInitialize && mysql_init(&m_lpMySQL) == nullptr) {
		ec_log_crit("KDatabase::InitEngine() mysql_init failed");
		return KCERR_DATABASE_ERROR;
	}
	m_bMysqlInitialize = true;
	my_bool xtrue = true;
	mysql_options(&m_lpMySQL, MYSQL_OPT_RECONNECT, &xtrue);
	return erSuccess;
}

ECRESULT KDatabase::IsInnoDBSupported(void)
{
	DB_RESULT res;
	DB_ROW row = nullptr;

	auto er = DoSelect("SHOW ENGINES", &res);
	if (er != erSuccess) {
		ec_log_crit("Unable to query supported database engines. Error: %s", GetError());
		return er;
	}

	while ((row = res.fetch_row()) != nullptr) {
		if (strcasecmp(row[0], "InnoDB") != 0)
			continue;
		if (strcasecmp(row[1], "DISABLED") == 0) {
			// mysql has run with innodb enabled once, but disabled this.. so check your log.
			ec_log_crit("INNODB engine is disabled. Please re-enable the INNODB engine. Check your MySQL log for more information or comment out skip-innodb in the mysql configuration file.");
			return KCERR_DATABASE_ERROR;
		} else if (strcasecmp(row[1], "YES") != 0 && strcasecmp(row[1], "DEFAULT") != 0) {
			// mysql is incorrectly configured or compiled.
			ec_log_crit("INNODB engine is not supported. Please enable the INNODB engine in the mysql configuration file.");
			return KCERR_DATABASE_ERROR;
		}
		break;
	}
	if (row == nullptr) {
		ec_log_crit("Unable to find 'InnoDB' engine from the mysql server. Probably INNODB is not supported.");
		return KCERR_DATABASE_ERROR;
	}
	return erSuccess;
}

ECRESULT KDatabase::Query(const std::string &q)
{
	LOG_SQL_DEBUG("SQL [%08lu]: \"%s;\"", m_lpMySQL.thread_id, q.c_str());
	/* Be binary safe (http://dev.mysql.com/doc/mysql/en/mysql-real-query.html) */
	auto err = mysql_real_query(&m_lpMySQL, q.c_str(), q.length());
	if (err == 0)
		return erSuccess;
	/* Callers without reconnect will emit different messages. */
	auto ers = mysql_error(&m_lpMySQL);
#ifdef HAVE_MYSQL_GET_OPTION
	my_bool reconn = false;
	if (mysql_get_option(&m_lpMySQL, MYSQL_OPT_RECONNECT, &reconn) == 0 && reconn)
#else
	if (m_lpMySQL.reconnect)
#endif
		ec_log_err("%p: SQL Failed: %s, Query: \"%s\"",
			static_cast<void *>(&m_lpMySQL), ers, q.c_str());
	return KCERR_DATABASE_ERROR;
}

ECRESULT KDatabase::I_Update(const std::string &q, unsigned int *aff)
{
	if (Query(q) != 0) {
		ec_log_err("KDatabase::I_Update() query failed: %s: %s",
			q.c_str(), GetError());
		return KCERR_DATABASE_ERROR;
	}
	if (aff != nullptr)
		*aff = GetAffectedRows();
	return erSuccess;
}
