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

#include "ECLogger.h"
#include "ECSessionManager.h"
#include "ECStatsCollector.h"

#include "ECDatabaseFactory.h"
#include "ECDatabaseMySQL.h"
#include "ECServerEntrypoint.h"

#include "ECSessionManagerOffline.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

pthread_key_t database_key;
pthread_key_t plugin_key;

ULONG					g_ulServerInitFlags = 0;	// Libary init flags
ECSessionManager*		g_lpSessionManager = NULL;
ECStatsCollector*		g_lpStatsCollector = NULL;
std::set<ECDatabase*>	g_lpDBObjectList;
pthread_mutex_t			g_hMutexDBObjectList;
bool					g_bInitLib = false;

void AddDatabaseObject(ECDatabase* lpDatabase)
{
	pthread_mutex_lock(&g_hMutexDBObjectList);

	g_lpDBObjectList.insert(std::set<ECDatabase*>::value_type(lpDatabase));

	pthread_mutex_unlock(&g_hMutexDBObjectList);
}

void database_destroy(void *lpParam)
{
	ECDatabase *lpDatabase = (ECDatabase *)lpParam;

	pthread_mutex_lock(&g_hMutexDBObjectList);

	g_lpDBObjectList.erase(std::set<ECDatabase*>::key_type(lpDatabase));

	pthread_mutex_unlock(&g_hMutexDBObjectList);

	delete lpDatabase;
}

void plugin_destroy(void *lpParam)
{
	UserPlugin *lpPlugin = (UserPlugin *)lpParam;

	delete lpPlugin;
}

ECRESULT zarafa_initlibrary(char* lpDatabaseDir, char* lpConfigFile, ECLogger *lpLogger)
{
	ECRESULT er = erSuccess;

	if(g_bInitLib == true) {
		er = ZARAFA_E_CALL_FAILED;
		goto exit;
	}

	// This is a global key that we can reference from each thread with a different value. The
	// database_destroy routine is called when the thread terminates.

	pthread_key_create(&database_key, database_destroy);
	pthread_key_create(&plugin_key, plugin_destroy); // same goes for the userDB-plugin

	// Init mutex for database object list
	pthread_mutex_init(&g_hMutexDBObjectList, NULL);

    er = ECDatabaseMySQL::InitLibrary(lpDatabaseDir, lpConfigFile, lpLogger);

	g_lpStatsCollector = new ECStatsCollector();
	
	//TODO: with an error remove all variables and g_bInitLib = false
	g_bInitLib = true;

exit:
	return er;
}

ECRESULT zarafa_unloadlibrary() 
{
	ECRESULT er = erSuccess;
	std::set<ECDatabase*>::iterator	iterDBObject, iNext;

	if(!g_bInitLib) {
		er = ZARAFA_E_NOT_INITIALIZED;
		goto exit;
	}

	// Delete the global key,  
	//  on this position, there are zero or more threads exist. 
	//  As you delete the keys, the function database_destroy and plugin_destroy will never called
	//
	pthread_key_delete(database_key);
	pthread_key_delete(plugin_key);

	// Remove all exist database objects
	pthread_mutex_lock(&g_hMutexDBObjectList);

	iterDBObject = g_lpDBObjectList.begin();
	while( iterDBObject != g_lpDBObjectList.end())
	{
		iNext = iterDBObject;
		iNext++;

		delete (*iterDBObject);

		g_lpDBObjectList.erase(iterDBObject);

		iterDBObject = iNext;
	}

	pthread_mutex_unlock(&g_hMutexDBObjectList);

	// remove mutex for database object list
	pthread_mutex_destroy(&g_hMutexDBObjectList);
	
	ECDatabaseMySQL::UnloadLibrary();

	//FIXME: do also something with delete of objects of plugin_key

	g_bInitLib = false;

exit:
	return er;
}

ECRESULT zarafa_init(ECConfig* lpConfig, ECLogger* lpLogger, ECLogger* lpAudit, bool bHostedZarafa, bool bDistributedZarafa)
{
	ECRESULT er = erSuccess;

	if(!g_bInitLib) {
		er = ZARAFA_E_NOT_INITIALIZED;
		goto exit;
	}

    g_lpSessionManager = new ECSessionManager(lpConfig, lpLogger, lpAudit, bHostedZarafa, bDistributedZarafa);
	
	er = g_lpSessionManager->LoadSettings();
	if(er != erSuccess)
		goto exit;

	er = g_lpSessionManager->CheckUserLicense();
	if (er != erSuccess)
		goto exit;

exit:
	return er;
}

void zarafa_removeallsessions()
{
	if(g_lpSessionManager) {
		g_lpSessionManager->RemoveAllSessions();
	}
}

ECRESULT zarafa_exit()
{
	ECRESULT er = erSuccess;
	std::set<ECDatabase*>::iterator	iterDBObject;

	if(!g_bInitLib) {
		er = ZARAFA_E_NOT_INITIALIZED;
		goto exit;
	}

	if (g_lpSessionManager)
		delete g_lpSessionManager;
	g_lpSessionManager = NULL;

	if (g_lpStatsCollector)
		delete g_lpStatsCollector;
	g_lpStatsCollector = NULL;

	// Close all database connections
	pthread_mutex_lock(&g_hMutexDBObjectList);

	for(iterDBObject = g_lpDBObjectList.begin(); iterDBObject != g_lpDBObjectList.end(); iterDBObject++)
	{
		(*iterDBObject)->Close();		
	}
	pthread_mutex_unlock(&g_hMutexDBObjectList);

exit:
	return er;
}

void zarafa_resetstats() {
	if (g_lpStatsCollector)
		g_lpStatsCollector->Reset();
}

// Called just after a new soap connection is established
void zarafa_new_soap_connection(CONNECTION_TYPE ulType, struct soap *soap, THREADSTATUSCALLBACK lpCallBack, void* ulParam)
{
	SOAPINFO *lpInfo = new SOAPINFO;
	lpInfo->ulConnectionType = ulType;
	lpInfo->lpCallBack = lpCallBack;
	lpInfo->ulCallBackParam = ulParam;
	soap->user = (void *)lpInfo;
}

// Called just before ending a soap connection and freeing soap data. Should free any
// data allocated in zarafa_new_soap_connection()
void zarafa_end_soap_connection(struct soap *soap)
{
	if(soap->user) 
		delete (SOAPINFO *) soap->user;
	soap_destroy(soap);
	soap_end(soap);
}

bool zarafa_get_soap_connection_type(struct soap *soap, CONNECTION_TYPE *lpulType)
{
	if (!soap->user)
		return false;

	*lpulType = SOAP_CONNECTION_TYPE(soap);
	return true;
}

// Called just before the socket is reset, with the server-side socket still
// open (before end_soap_connection)
void zarafa_disconnect_soap_connection(struct soap *soap)
{
	if(SOAP_CONNECTION_TYPE_NAMED_PIPE(soap)) {
		// Mark the persistent session as exited
		g_lpSessionManager->RemoveSessionPersistentConnection((unsigned int)soap->socket);
	}
}

/////////////////////////////////////////////////////
// Export functions
//

ECRESULT GetDatabaseObject(ECDatabase **lppDatabase)
{
	if(g_lpSessionManager == NULL) {
		return ZARAFA_E_UNKNOWN;
	}

	if(lppDatabase == NULL) {
		return ZARAFA_E_INVALID_PARAMETER;
	}

	ECDatabaseFactory db(g_lpSessionManager->GetConfig(), g_lpSessionManager->GetLogger());

	return GetThreadLocalDatabase(&db, lppDatabase);
}
