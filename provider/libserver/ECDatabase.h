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

#ifndef ECDATABASE_H
#define ECDATABASE_H

#include "ECConfig.h"
#include "ECLogger.h"
#include "ZarafaCode.h"

#include <string>

typedef void *			DB_RESULT;	
typedef char **			DB_ROW;	
typedef unsigned long *	DB_LENGTHS;
typedef unsigned int	DB_ERROR;


#define DB_E_UNKNOWN			DB_ERROR(-1)
#define DB_E_LOCK_WAIT_TIMEOUT	DB_ERROR(1)
#define DB_E_LOCK_DEADLOCK		DB_ERROR(2)


// Abstract base class for databases
class ECDatabase
{
protected:
	std::string error;
	ECLogger *m_lpLogger;
	bool m_bForceUpdate;

public:
	virtual 				~ECDatabase() {};
	
	virtual ECRESULT		Connect() = 0;
	virtual ECRESULT		Close() = 0;

	// Table functions
	virtual ECRESULT		DoSelect(const std::string &strQuery, DB_RESULT *lpResult, bool fStreamResult = false) = 0;
	virtual ECRESULT		DoUpdate(const std::string &strQuery, unsigned int *lpulAffectedRows = NULL) = 0;
	virtual ECRESULT		DoInsert(const std::string &strQuery, unsigned int *lpulInsertId = NULL, unsigned int *lpulAffectedRows = NULL) = 0;
	virtual ECRESULT		DoDelete(const std::string &strQuery, unsigned int *lpulAffectedRows = NULL) = 0;
	virtual	ECRESULT		DoSelectMulti(const std::string &strQuery) = 0;
	// Sequence generator - Do NOT CALL THIS FROM WITHIN A TRANSACTION.
	virtual ECRESULT		DoSequence(const std::string &strSeqName, unsigned int ulCount, unsigned long long *lpllFirstId) = 0;

	// Result functions
	virtual unsigned int	GetNumRows(DB_RESULT sResult) = 0;
	virtual unsigned int	GetNumRowFields(DB_RESULT sResult) = 0;
	virtual unsigned int	GetRowIndex(DB_RESULT sResult, const std::string &strFieldname) = 0;
	virtual ECRESULT		GetNextResult(DB_RESULT *sResult) = 0;
	virtual	ECRESULT		FinalizeMulti() = 0;

	virtual DB_ROW			FetchRow(DB_RESULT sResult) = 0;
	virtual DB_LENGTHS		FetchRowLengths(DB_RESULT sResult) = 0;

	virtual std::string		FilterBMP(const std::string &strToEscape) = 0;
	virtual std::string		Escape(const std::string &strToEscape) = 0;
	virtual std::string		EscapeBinary(unsigned char *lpData, unsigned int ulLen) = 0;
	virtual std::string		EscapeBinary(const std::string& strData) = 0;

	// Reset the result set so it can be iterated again
	virtual void			ResetResult(DB_RESULT sResult) = 0;

	virtual ECRESULT		ValidateTables() = 0;

	// Freememory functions
	virtual	void			FreeResult(DB_RESULT sResult) = 0;

	// Get error string
	virtual std::string		GetError() = 0;
	
	// Get last error code
	virtual DB_ERROR		GetLastError() = 0;

	// Enable/disable suppression of lock errors
	virtual bool			SuppressLockErrorLogging(bool bSuppress) = 0;

	// Database functions
	virtual ECRESULT		CreateDatabase() = 0;
	virtual ECRESULT		UpdateDatabase(bool bForceUpdate, std::string &strReport) = 0;
	virtual ECRESULT		InitializeDBState() = 0;

	// Get logger
	virtual ECLogger*		GetLogger() = 0;

	// Get Database path
	virtual std::string		GetDatabaseDir() = 0;
	
	// Transactions
	// These functions should be used to wrap blocks of queries into transactions. This will
	// speed up writes a lot, so try to use them as much as possible. If you don't start a transaction
	// then each INSERT or UPDATE will automatically be a single transaction, causing an fsync
	// after each write-query, which is not fast to say the least.
	
	virtual ECRESULT		Begin() = 0;
	virtual ECRESULT		Commit() = 0;
	virtual ECRESULT		Rollback() = 0;
	
	// Return the maximum size of any query we can send
	virtual unsigned int	GetMaxAllowedPacket() = 0;

	virtual void			ThreadInit() = 0;
	virtual void			ThreadEnd() = 0;
	
	virtual ECRESULT		CheckExistColumn(const std::string &strTable, const std::string &strColumn, bool *lpbExist) = 0;
	virtual ECRESULT		CheckExistIndex(const std::string strTable, const std::string &strKey, bool *lpbExist) = 0;

	// Function requires m_bForceUpdate variable
	friend ECRESULT UpdateDatabaseConvertToUnicode(ECDatabase *lpDatabase);
};



#endif // ECDATABASE_H
