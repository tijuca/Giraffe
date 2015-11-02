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

// ECDatabaseMySQL.h: interface for the ECDatabaseMySQL class.
//
//////////////////////////////////////////////////////////////////////

#ifndef ECDATABASEMYSQL_H
#define ECDATABASEMYSQL_H

#include "zcdefs.h"
#include <pthread.h>
#include <mysql.h>
#include <string>

#include "ECDatabase.h"

class ECConfig;

class ECDatabaseMySQL _final : public ECDatabase
{
public:
	ECDatabaseMySQL(ECLogger *lpLogger, ECConfig *lpConfig);
	virtual ~ECDatabaseMySQL();

	// Embedded mysql
	static ECRESULT	InitLibrary(const char *lpDatabaseDir, const char *lpConfigFile, ECLogger *lpLogger);
	static void UnloadLibrary(ECLogger * = NULL);

	ECRESULT Connect(void) _override;
	ECRESULT Close(void) _override;
	ECRESULT DoSelect(const std::string &strQuery, DB_RESULT *lpResult, bool fStreamResult = false) _override;
	ECRESULT DoSelectMulti(const std::string &strQuery) _override;
	ECRESULT DoUpdate(const std::string &strQuery, unsigned int *lpulAffectedRows = NULL) _override;
	ECRESULT DoInsert(const std::string &strQuery, unsigned int *lpulInsertId = NULL, unsigned int *lpulAffectedRows = NULL) _override;
	ECRESULT DoDelete(const std::string &strQuery, unsigned int *lpulAffectedRows = NULL) _override;
	ECRESULT DoSequence(const std::string &strSeqName, unsigned int ulCount, unsigned long long *lpllFirstId) _override;

	//Result functions
	unsigned int GetNumRows(DB_RESULT sResult) _override;
	unsigned int GetNumRowFields(DB_RESULT sResult) _override;
	unsigned int GetRowIndex(DB_RESULT sResult, const std::string &strFieldname) _override;
	virtual ECRESULT GetNextResult(DB_RESULT *sResult) _override;
	virtual ECRESULT FinalizeMulti(void) _override;

	DB_ROW FetchRow(DB_RESULT sResult) _override;
	DB_LENGTHS FetchRowLengths(DB_RESULT sResult) _override;

	std::string Escape(const std::string &strToEscape) _override;
	std::string EscapeBinary(unsigned char *lpData, unsigned int ulLen) _override;
	std::string EscapeBinary(const std::string& strData) _override;
	std::string FilterBMP(const std::string &strToFilter) _override;

	void ResetResult(DB_RESULT sResult) _override;

	ECRESULT ValidateTables(void) _override;

	std::string GetError(void) _override;
	DB_ERROR GetLastError(void) _override;
	bool SuppressLockErrorLogging(bool bSuppress) _override;
	
	ECRESULT Begin(void) _override;
	ECRESULT Commit(void) _override;
	ECRESULT Rollback(void) _override;
	
	unsigned int GetMaxAllowedPacket(void) _override;

	void ThreadInit(void) _override;
	void ThreadEnd(void) _override;

	// Database maintenance functions
	ECRESULT CreateDatabase(void) _override;
	// Main update unit
	ECRESULT UpdateDatabase(bool bForceUpdate, std::string &strReport) _override;
	ECRESULT InitializeDBState(void) _override;

	ECLogger *GetLogger(void) _override;

	std::string GetDatabaseDir(void) _override;
	
	ECRESULT CheckExistColumn(const std::string &strTable, const std::string &strColumn, bool *lpbExist) _override;
	ECRESULT CheckExistIndex(const std::string strTable, const std::string &strKey, bool *lpbExist) _override;

public:
	// Freememory methods
	void			FreeResult(DB_RESULT sResult);

private:
    
	ECRESULT InitEngine();
	ECRESULT IsInnoDBSupported();
	ECRESULT InitializeDBStateInner(void);
	
	ECRESULT _Update(const std::string &strQuery, unsigned int *lpulAffectedRows);
	ECRESULT Query(const std::string &strQuery);
	unsigned int GetAffectedRows();
	unsigned int GetInsertId();

	// Datalocking methods
	bool Lock();
	bool UnLock();

	// Connection methods
	bool isConnected();


// Database maintenance
	ECRESULT GetDatabaseVersion(unsigned int *lpulMajor, unsigned int *lpulMinor, unsigned int *lpulRevision, unsigned int *lpulDatabaseRevision);

	// Add a new database version record
	ECRESULT UpdateDatabaseVersion(unsigned int ulDatabaseRevision);
	ECRESULT IsUpdateDone(unsigned int ulDatabaseRevision, unsigned int ulRevision=0);
	ECRESULT GetFirstUpdate(unsigned int *lpulDatabaseRevision);

private:
	bool				m_bMysqlInitialize;
	bool				m_bConnected;
	MYSQL				m_lpMySQL;
	pthread_mutex_t		m_hMutexMySql;
	bool				m_bAutoLock;
	unsigned int 		m_ulMaxAllowedPacket;
	bool				m_bLocked;
	bool				m_bFirstResult;
	static std::string	m_strDatabaseDir;
	ECConfig *			m_lpConfig;
	bool				m_bSuppressLockErrorLogging;
#ifdef DEBUG
    unsigned int		m_ulTransactionState;
#endif
};

#endif // #ifndef ECDATABASEMYSQL_H
