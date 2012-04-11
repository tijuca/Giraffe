/*
 * Copyright 2005 - 2009  Zarafa B.V.
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

#include <pthread.h>
#include <mysql.h>
#include <string>

#include "ECDatabase.h"

class ECConfig;

class ECDatabaseMySQL : public ECDatabase
{
public:
	ECDatabaseMySQL(ECLogger *lpLogger, ECConfig *lpConfig);
	virtual ~ECDatabaseMySQL();

	// Embedded mysql
	static ECRESULT	InitLibrary(char* lpDatabaseDir, char *lpConfigFile, ECLogger *lpLogger);
	static void UnloadLibrary();

	ECRESULT		Connect();
	ECRESULT		Close();
	ECRESULT		DoSelect(const std::string &strQuery, DB_RESULT *lpResult, bool fStreamResult = false);
	ECRESULT		DoUpdate(const std::string &strQuery, unsigned int *lpulAffectedRows = NULL);
	ECRESULT		DoInsert(const std::string &strQuery, unsigned int *lpulInsertId = NULL, unsigned int *lpulAffectedRows = NULL);
	ECRESULT		DoDelete(const std::string &strQuery, unsigned int *lpulAffectedRows = NULL);
	ECRESULT		DoSequence(const std::string &strSeqName, unsigned int ulCount, unsigned long long *lpllFirstId);

	//Result functions
	unsigned int	GetNumRows(DB_RESULT sResult);
	unsigned int	GetNumRowFields(DB_RESULT sResult);
	unsigned int	GetRowIndex(DB_RESULT sResult, const std::string &strFieldname);

	DB_ROW			FetchRow(DB_RESULT sResult);
	DB_LENGTHS		FetchRowLengths(DB_RESULT sResult);

	std::string		Escape(const std::string &strToEscape);
	std::string		EscapeBinary(unsigned char *lpData, unsigned int ulLen);
	std::string		EscapeBinary(std::string& strData);
    std::string 	FilterBMP(const std::string &strToFilter);

	void			ResetResult(DB_RESULT sResult);

	std::string		GetError();
	
	ECRESULT		Begin();
	ECRESULT		Commit();
	ECRESULT		Rollback();
	
	unsigned int	GetMaxAllowedPacket();

	// Database maintenance functions
	ECRESULT		CreateDatabase();
	// Main update unit
	ECRESULT		UpdateDatabase(bool bForceUpdate, std::string &strReport);

	ECLogger*		GetLogger();

	std::string		GetDatabaseDir();

public:
	// Freememory methods
	void			FreeResult(DB_RESULT sResult);

private:
    
	ECRESULT InitEngine();
	ECRESULT IsInnoDBSupported();
	
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
	static std::string	m_strDatabaseDir;
	ECConfig *			m_lpConfig;
#ifdef DEBUG
    unsigned int		m_ulTransactionState;
#endif
};

#endif // #ifndef ECDATABASEMYSQL_H
