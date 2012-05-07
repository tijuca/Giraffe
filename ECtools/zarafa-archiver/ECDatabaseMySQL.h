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

class ECDatabaseMySQL
{
public:
	ECDatabaseMySQL(ECLogger *lpLogger);
	virtual ~ECDatabaseMySQL();

	ECRESULT		Connect(ECConfig *lpConfig);
	ECRESULT		Close();
	ECRESULT		DoSelect(const std::string &strQuery, DB_RESULT *lpResult);
	ECRESULT		DoUpdate(const std::string &strQuery, unsigned int *lpulAffectedRows = NULL);
	ECRESULT		DoInsert(const std::string &strQuery, unsigned int *lpulInsertId = NULL, unsigned int *lpulAffectedRows = NULL);
	ECRESULT		DoDelete(const std::string &strQuery, unsigned int *lpulAffectedRows = NULL);
	ECRESULT		DoSequence(const std::string &strSeqName, unsigned int ulCount, unsigned long long *lpllFirstId);

	//Result functions
	unsigned int	GetNumRows(DB_RESULT sResult);
	unsigned int	GetNumRowFields(DB_RESULT sResult);

	DB_ROW			FetchRow(DB_RESULT sResult);
	DB_LENGTHS		FetchRowLengths(DB_RESULT sResult);

	std::string		Escape(const std::string &strToEscape);
	std::string		EscapeBinary(unsigned char *lpData, unsigned int ulLen);

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
