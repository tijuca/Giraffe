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

#include <stringutil.h>

#include "threadutil.h"
#include "ECLogger.h"
#include "ECConfig.h"
#include "ECSession.h"
#include "ECSessionManager.h"
#include "ECDatabaseFactory.h"
#include "ECStatsCollector.h"
#include "ZarafaCode.h"

#include "ECTPropsPurge.h"

extern ECStatsCollector*     g_lpStatsCollector;

ECTPropsPurge::ECTPropsPurge(ECConfig *lpConfig, ECLogger *lpLogger, ECDatabaseFactory *lpDatabaseFactory)
{
    pthread_mutex_init(&m_hMutexExit, NULL);
    pthread_cond_init(&m_hCondExit, NULL);
    
    m_lpConfig = lpConfig;
    m_lpLogger = lpLogger;
    m_lpDatabaseFactory = lpDatabaseFactory;
    m_bExit = false;
    
    // Start our purge thread
    pthread_create(&m_hThread, NULL, Thread, (void *)this);
}

ECTPropsPurge::~ECTPropsPurge()
{
	// Signal thread to exit
	pthread_mutex_lock(&m_hMutexExit);
	m_bExit = true;
	pthread_cond_signal(&m_hCondExit);
	pthread_mutex_unlock(&m_hMutexExit);
	
	// Wait for the thread to exit
	pthread_join(m_hThread, NULL);
	
	// Cleanup
    pthread_mutex_destroy(&m_hMutexExit);
    pthread_cond_destroy(&m_hCondExit);
}

/**
 * This is just a pthread_create() wrapper which calls PurgeThread()
 *
 * @param param Pthread context param
 * @return pthread return code, 0 on success, 1 on error
 */
void * ECTPropsPurge::Thread(void *param)
{
	ECTPropsPurge *lpThis = (ECTPropsPurge *)param;
	lpThis->PurgeThread();
	
	return NULL;
}

/**
 * Main TProps purger loop
 *
 * This is a constantly running loop that checks the number of deferred updates in the
 * deferredupdate table, and starts purging them if it goes over a certain limit. The purged
 * items are from the largest folder first; A folder with 20 deferredupdates will be purged
 * before a folder with only 10 deferred updates.
 *
 * The loop (thread) will exit ASAP when m_bExit is set to TRUE.
 *
 * @return result
 */
ECRESULT ECTPropsPurge::PurgeThread()
{
    ECRESULT er = erSuccess;
    ECDatabase *lpDatabase = NULL;
	struct timespec deadline = {0};
    
    while(1) {
    	// Run in a loop constantly checking our deferred update table
    	
        if(!lpDatabase) {
            er = GetThreadLocalDatabase(this->m_lpDatabaseFactory, &lpDatabase);
            if(er != erSuccess) {
                m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to get database connection for delayed purge!");
                Sleep(60000);
                continue;
            }
        }

		// Wait a while before repolling the count, unless we are requested to exit        
        {
            scoped_lock lock(m_hMutexExit);
            
            if(m_bExit) break;
            clock_gettime(CLOCK_REALTIME, &deadline);
            deadline.tv_sec += 10;
            pthread_cond_timedwait(&m_hCondExit, &m_hMutexExit, &deadline);
            if(m_bExit) break;
        }
        
        PurgeOverflowDeferred(lpDatabase); // Ignore error, just retry
    }
    
    // Don't touch anything in *this from this point, we may have been delete()d by this time
    return er;
}

/**
 * Purge deferred updates
 *
 * This purges deferred updates until the total number of deferred updates drops below
 * the limit in max_deferred_records.
 *
 * @param lpDatabase Database to use
 * @return Result
 */
ECRESULT ECTPropsPurge::PurgeOverflowDeferred(ECDatabase *lpDatabase)
{
    ECRESULT er = erSuccess;
    unsigned int ulCount = 0;
    unsigned int ulFolderId = 0;
    unsigned int ulMaxDeferred = atoi(m_lpConfig->GetSetting("max_deferred_records"));
    
    if(ulMaxDeferred > 0) {
		while(!m_bExit) {
			er = GetDeferredCount(lpDatabase, &ulCount);
			if(er != erSuccess)
				goto exit;
				
			if(ulCount < ulMaxDeferred)
				break;
				
			er = lpDatabase->Begin();
			if(er != erSuccess)
				goto exit;
			
			er = GetLargestFolderId(lpDatabase, &ulFolderId);
			if(er != erSuccess) {
				lpDatabase->Rollback();
				goto exit;
			}
				
			er = PurgeDeferredTableUpdates(lpDatabase, ulFolderId);
			if(er != erSuccess) {
				lpDatabase->Rollback();
				goto exit;
			}
				
			er = lpDatabase->Commit();
			if(er != erSuccess)
				goto exit;
		}
	}
	    
exit:
    return er;
}

/**
 * Get the deferred record count
 *
 * This gets the total number of deferred records
 *
 * @param[in] lpDatabase Database pointer
 * @param[out] Number of records
 * @return Result
 */
ECRESULT ECTPropsPurge::GetDeferredCount(ECDatabase *lpDatabase, unsigned int *lpulCount)
{
    ECRESULT er = erSuccess;
    DB_RESULT lpResult = NULL; 
    DB_ROW lpRow = NULL;
    
    er = lpDatabase->DoSelect("SELECT count(*) FROM deferredupdate", &lpResult);
    if(er != erSuccess)
        goto exit;
    
    lpRow = lpDatabase->FetchRow(lpResult);
    if(!lpRow || !lpRow[0]) {
        er = ZARAFA_E_DATABASE_ERROR;
        goto exit;
    }
    
    *lpulCount = atoui(lpRow[0]);
    
exit:
    if(lpResult)
        lpDatabase->FreeResult(lpResult);
        
    return er;    
}

/**
 * Get the folder with the most deferred items in it
 *
 * Retrieves the hierarchy ID of the folder with the most deferred records in it. If two or more
 * folders tie, then one of these folders is returned. It is undefined exactly which one will be returned.
 *
 * @param[in] lpDatabase Database pointer
 * @param[out] lpulFolderId Hierarchy ID of folder
 * @return Result
 */
ECRESULT ECTPropsPurge::GetLargestFolderId(ECDatabase *lpDatabase, unsigned int *lpulFolderId)
{
    ECRESULT er = erSuccess;
    DB_RESULT lpResult = NULL;
    DB_ROW lpRow = NULL;
    
    er = lpDatabase->DoSelect("SELECT folderid, COUNT(*) as c FROM deferredupdate GROUP BY folderid ORDER BY c DESC LIMIT 1", &lpResult);
    if(er != erSuccess)
        goto exit;
        
    lpRow = lpDatabase->FetchRow(lpResult);
    if(!lpRow || !lpRow[0]) {
        // Could be that there are no deferred updates, so give an appropriate error
        er = ZARAFA_E_NOT_FOUND;
        goto exit;
    }
    
    *lpulFolderId = atoui(lpRow[0]);
exit:
    if(lpResult)
        lpDatabase->FreeResult(lpResult);
        
	return er;
}

/**
 * Purge deferred table updates stored for folder ulFolderId
 *
 * This purges deferred records for hierarchy and contents tables of ulFolderId, and removes
 * them from the deferredupdate table.
 *
 * @param[in] lpDatabase Database pointer
 * @param[in] Hierarchy ID of folder to purge
 * @return Result
 */
ECRESULT ECTPropsPurge::PurgeDeferredTableUpdates(ECDatabase *lpDatabase, unsigned int ulFolderId)
{
	ECRESULT er = erSuccess;
	unsigned int ulAffected;
	DB_RESULT lpDBResult = NULL;
	DB_ROW lpDBRow = NULL;

	std::string strQuery;
	
	// This makes sure that we lock the record in the hierarchy *first*. This helps in serializing access and avoiding deadlocks.
	strQuery = "SELECT hierarchyid FROM deferredupdate WHERE folderid=" + stringify(ulFolderId);
	er = lpDatabase->DoSelect(strQuery, &lpDBResult);
	if(er != erSuccess)
		goto exit;

	strQuery.clear();
	if(lpDatabase->GetNumRows(lpDBResult) > 0) {		
		strQuery = "SELECT id FROM hierarchy WHERE id IN(";
		while(lpDBRow = lpDatabase->FetchRow(lpDBResult)) {
			strQuery += lpDBRow[0];
			strQuery += ",";
		}
		strQuery.resize(strQuery.size()-1);
		strQuery += ") FOR UPDATE";
	}
			
	lpDatabase->FreeResult(lpDBResult);
	
	if(!strQuery.empty()) {
		er = lpDatabase->DoSelect(strQuery, &lpDBResult);
		if(er != erSuccess)
			goto exit;
			
		lpDatabase->FreeResult(lpDBResult);
	}
		
	strQuery = "REPLACE INTO tproperties (folderid, hierarchyid, tag, type, val_ulong, val_string, val_binary, val_double, val_longint, val_hi, val_lo) ";
	strQuery += "SELECT " + stringify(ulFolderId) + ", p.hierarchyid, p.tag, p.type, val_ulong, LEFT(val_string, " + stringify(TABLE_CAP_STRING) + "), LEFT(val_binary, " + stringify(TABLE_CAP_BINARY) + "), val_double, val_longint, val_hi, val_lo FROM properties AS p FORCE INDEX(primary) JOIN deferredupdate FORCE INDEX(folderid) ON deferredupdate.hierarchyid=p.hierarchyid WHERE tag NOT IN(0x1009, 0x1013) AND deferredupdate.folderid = " + stringify(ulFolderId);

	er = lpDatabase->DoInsert(strQuery);
	if(er != erSuccess)
		goto exit;
	
	strQuery = "DELETE FROM deferredupdate WHERE folderid=" + stringify(ulFolderId);
	er = lpDatabase->DoDelete(strQuery, &ulAffected);
	if(er != erSuccess)
		goto exit;
		
	g_lpStatsCollector->Increment(SCN_DATABASE_MERGES);
	g_lpStatsCollector->Increment(SCN_DATABASE_MERGED_RECORDS, (int)ulAffected);
	
exit:
	return er;
}

ECRESULT ECTPropsPurge::GetDeferredCount(ECDatabase *lpDatabase, unsigned int ulFolderId, unsigned int *lpulCount)
{
	ECRESULT er = erSuccess;
	DB_RESULT lpDBResult = NULL;
	DB_ROW lpDBRow = NULL;
	unsigned int ulCount = 0;
	std::string strQuery;
	
	strQuery = "SELECT count(*) FROM deferredupdate WHERE folderid = " + stringify(ulFolderId);
	er = lpDatabase->DoSelect(strQuery, &lpDBResult);
	if(er != erSuccess)
		goto exit;
		
	lpDBRow = lpDatabase->FetchRow(lpDBResult);
	
	if(!lpDBRow || !lpDBRow[0])
		ulCount = 0;
	else
		ulCount = atoui(lpDBRow[0]);
		
	*lpulCount = ulCount;
	
exit:
	if (lpDBResult)
		lpDatabase->FreeResult(lpDBResult);
		
	return er;
}

/**
 * Add a deferred update
 *
 * Adds a deferred update to the deferred updates table and purges the deferred updates for the folder if necessary.
 *
 * @param[in] lpSession Session that created the change
 * @param[in] lpDatabase Database handle
 * @param[in] ulFolderId Folder ID to add a deferred update to
 * @param[in] ulOldFolderId Previous folder ID if the message was moved (may be 0)
 * @param[in] ulObjId Object ID that should be added
 * @return result
 */
ECRESULT ECTPropsPurge::AddDeferredUpdate(ECSession *lpSession, ECDatabase *lpDatabase, unsigned int ulFolderId, unsigned int ulOldFolderId, unsigned int ulObjId)
{
	ECRESULT er = erSuccess;

	er = AddDeferredUpdateNoPurge(lpDatabase, ulFolderId, ulOldFolderId, ulObjId);
	if (er != erSuccess)
		goto exit;

	er = NormalizeDeferredUpdates(lpSession, lpDatabase, ulFolderId);

exit:
	return er;
}

/**
 * Add a deferred update
 *
 * Adds a deferred update to the deferred updates table but never purges the deferred updates for the folder.
 *
 * @param[in] lpDatabase Database handle
 * @param[in] ulFolderId Folder ID to add a deferred update to
 * @param[in] ulOldFolderId Previous folder ID if the message was moved (may be 0)
 * @param[in] ulObjId Object ID that should be added
 * @return result
 */
ECRESULT ECTPropsPurge::AddDeferredUpdateNoPurge(ECDatabase *lpDatabase, unsigned int ulFolderId, unsigned int ulOldFolderId, unsigned int ulObjId)
{
	ECRESULT er = erSuccess;
	std::string strQuery;

	if (ulOldFolderId)
		// Message has moved into a new folder. If the record is already there then just update the existing record so that srcfolderid from a previous move remains untouched
		strQuery = "INSERT INTO deferredupdate(hierarchyid, srcfolderid, folderid) VALUES(" + stringify(ulObjId) + "," + stringify(ulOldFolderId) + "," + stringify(ulFolderId) + ") ON DUPLICATE KEY UPDATE folderid = " + stringify(ulFolderId);
	else
		// Message has modified. If there is already a record for this message, we don't need to do anything
		strQuery = "INSERT IGNORE INTO deferredupdate(hierarchyid, srcfolderid, folderid) VALUES(" + stringify(ulObjId) + "," + stringify(ulFolderId) + "," + stringify(ulFolderId) + ")";
		
	er = lpDatabase->DoInsert(strQuery);
	if(er != erSuccess)
		goto exit;

exit:
	return er;
}

/**
 * Purge the deferred updates table if the count for the folder exceeds max_deferred_records_folder
 *
 * Purges the deferred updates for the folder if necessary.
 *
 * @param[in] lpSession Session that created the change
 * @param[in] lpDatabase Database handle
 * @param[in] ulFolderId Folder ID to add a deferred update to
 * @return result
 */
ECRESULT ECTPropsPurge::NormalizeDeferredUpdates(ECSession *lpSession, ECDatabase *lpDatabase, unsigned int ulFolderId)
{
	ECRESULT er = erSuccess;
	unsigned int ulMaxDeferred = 0;
	unsigned int ulCount = 0;
		
	ulMaxDeferred = atoui(lpSession->GetSessionManager()->GetConfig()->GetSetting("max_deferred_records_folder"));
	
	if (ulMaxDeferred) {
		er = GetDeferredCount(lpDatabase, ulFolderId, &ulCount);
		if (er != erSuccess)
			goto exit;
			
		if (ulCount >= ulMaxDeferred) {
			er = PurgeDeferredTableUpdates(lpDatabase, ulFolderId);
			if (er != erSuccess)
				goto exit;
		}
	}

exit:
	return er;
}
