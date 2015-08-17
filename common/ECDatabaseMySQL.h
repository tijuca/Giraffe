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

#include "platform.h"
#include "ECConfig.h"
#include "ECLogger.h"
#include "ZarafaCode.h"

#include <pthread.h>
#include <mysql.h>
#include <string>

using namespace std;

typedef void *			DB_RESULT;	
typedef char **			DB_ROW;	
typedef unsigned long *	DB_LENGTHS;

// The max length of a group_concat function
#define MAX_GROUP_CONCAT_LEN		32768

typedef struct _sDatabase {
	const char *lpComment;
	const char *lpSQL;
} sSQLDatabase_t;

class ECDatabaseMySQL
{
public:
	ECDatabaseMySQL(ECLogger *lpLogger);
	virtual ~ECDatabaseMySQL();

	ECRESULT		Connect(ECConfig *lpConfig);
	ECRESULT		Close();
	ECRESULT		DoSelect(const std::string &strQuery, DB_RESULT *lpResult, bool bStream = false);
	ECRESULT		DoUpdate(const std::string &strQuery, unsigned int *lpulAffectedRows = NULL);
	ECRESULT		DoInsert(const std::string &strQuery, unsigned int *lpulInsertId = NULL, unsigned int *lpulAffectedRows = NULL);
	ECRESULT		DoDelete(const std::string &strQuery, unsigned int *lpulAffectedRows = NULL);
		ECRESULT		DoSequence(const std::string &strSeqName, unsigned int ulCount, uint64_t *lpllFirstId);
	
	virtual const sSQLDatabase_t *GetDatabaseDefs(void) = 0;

	//Result functions
	unsigned int	GetNumRows(DB_RESULT sResult);
	unsigned int	GetNumRowFields(DB_RESULT sResult);

	DB_ROW			FetchRow(DB_RESULT sResult);
	DB_LENGTHS		FetchRowLengths(DB_RESULT sResult);

	std::string		Escape(const std::string &strToEscape);
	std::string		EscapeBinary(const unsigned char *lpData, unsigned int ulLen);
	std::string		EscapeBinary(const std::string &strData);

	std::string		GetError();
	
	ECRESULT		Begin();
	ECRESULT		Commit();
	ECRESULT		Rollback();
	
	unsigned int	GetMaxAllowedPacket();

	// Database maintenance function(s)
	ECRESULT		CreateDatabase(ECConfig *lpConfig);

	ECLogger*		GetLogger();

	// Freememory method(s)
	void			FreeResult(DB_RESULT sResult);

private:
	ECRESULT InitEngine();
	ECRESULT IsInnoDBSupported();

	ECRESULT _Update(const string &strQuery, unsigned int *lpulAffectedRows);
	int Query(const string &strQuery);
	unsigned int GetAffectedRows();
	unsigned int GetInsertId();

	// Datalocking methods
	bool Lock();
	bool UnLock();

	// Connection methods
	bool isConnected();

private:
	bool				m_bMysqlInitialize;
	bool				m_bConnected;
	MYSQL				m_lpMySQL;
	pthread_mutex_t		m_hMutexMySql;
	bool				m_bAutoLock;
	unsigned int 		m_ulMaxAllowedPacket;
	bool				m_bLocked;
	ECLogger*			m_lpLogger;
};

#endif
