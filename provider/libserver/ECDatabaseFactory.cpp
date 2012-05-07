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

#include "ECDatabaseMySQL.h"
#include "ECDatabaseFactory.h"

#include "ECServerEntrypoint.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

// The ECDatabaseFactory creates database objects connected to the server database. Which
// database is returned is chosen by the database_engine configuration setting.

ECDatabaseFactory::ECDatabaseFactory(ECConfig *lpConfig, ECLogger *lpLogger)
{
	this->m_lpConfig = lpConfig;
	this->m_lpLogger = lpLogger;
}

ECDatabaseFactory::~ECDatabaseFactory()
{
}

ECRESULT ECDatabaseFactory::GetDatabaseFactory(ECDatabase **lppDatabase)
{
	ECRESULT		er = erSuccess;
	char*			szEngine = m_lpConfig->GetSetting("database_engine");

	if(stricmp(szEngine, "mysql") == 0) {
		*lppDatabase = new ECDatabaseMySQL(m_lpLogger, m_lpConfig);
	} else {
		er = ZARAFA_E_DATABASE_ERROR;
		goto exit;
	}

exit:
	return er;
}

ECRESULT ECDatabaseFactory::CreateDatabaseObject(ECDatabase **lppDatabase, std::string &ConnectError)
{
	ECRESULT		er = erSuccess;
	ECDatabase*		lpDatabase = NULL;

	er = GetDatabaseFactory(&lpDatabase);
	if(er != erSuccess) {
		ConnectError = "Invalid database engine";
		goto exit;
	}

	er = lpDatabase->Connect();
	if(er != erSuccess) {
		ConnectError = lpDatabase->GetError();
		delete lpDatabase;
		goto exit;
	}

	*lppDatabase = lpDatabase;

exit:
	return er;
}

ECRESULT ECDatabaseFactory::CreateDatabase()
{
	ECRESULT	er = erSuccess;
	ECDatabase*	lpDatabase = NULL;
	std::string	strQuery;
	
	er = GetDatabaseFactory(&lpDatabase);
	if(er != erSuccess)
		goto exit;

	er = lpDatabase->CreateDatabase();
	if(er != erSuccess)
		goto exit;
	
exit:
	if(lpDatabase)
		delete lpDatabase;

	return er;
}

ECRESULT ECDatabaseFactory::UpdateDatabase(bool bForceUpdate, std::string &strReport)
{
	ECRESULT		er = erSuccess;
	ECDatabase*		lpDatabase = NULL;
	
	er = CreateDatabaseObject(&lpDatabase, strReport);
	if(er != erSuccess)
		goto exit;

	er = lpDatabase->UpdateDatabase(bForceUpdate, strReport);
	if(er != erSuccess)
		goto exit;

exit:
	if(lpDatabase)
		delete lpDatabase;

	return er;
}

ECLogger * ECDatabaseFactory::GetLogger() {
    return m_lpLogger;
}

extern pthread_key_t database_key;

ECRESULT GetThreadLocalDatabase(ECDatabaseFactory *lpFactory, ECDatabase **lppDatabase)
{
	ECRESULT er = erSuccess;
	ECDatabase *lpDatabase = NULL;
	std::string error;

	// We check to see whether the calling thread already
	// has an open database connection. If so, we return that, or otherwise
	// we create a new one.

	// database_key is defined in ECServer.cpp, and allocated in the running_server routine
	lpDatabase = (ECDatabase *)pthread_getspecific(database_key);
	
	if(lpDatabase == NULL) {
		er = lpFactory->CreateDatabaseObject(&lpDatabase, error);

		if(er != erSuccess) {
		    lpFactory->GetLogger()->Log(EC_LOGLEVEL_FATAL, "Unable to get database connection: %s", error.c_str());
			lpDatabase = NULL;
			goto exit;
		}
		
		// Add database into a list, for close all database connections
		AddDatabaseObject(lpDatabase);

		pthread_setspecific(database_key, (void *)lpDatabase);
	}

	*lppDatabase = lpDatabase;

exit:
	return er;
}

