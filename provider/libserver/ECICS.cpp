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

#include "Zarafa.h"
#include "mapidefs.h"
#include "edkmdb.h"

#include "stringutil.h"
#include "ECSessionManager.h"
#include "ECSecurity.h"

#include "ZarafaICS.h"
#include "ZarafaCmdUtil.h"
#include "ECStoreObjectTable.h"

#include "ECGuid.h"
#include "ECICS.h"
#include "ECICSHelpers.h"
#include "ECMAPI.h"
#include "soapH.h"
#include "SOAPUtils.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

extern ECSessionManager*	g_lpSessionManager;

bool isICSChange(unsigned int ulChange){
	switch(ulChange){
		case ICS_MESSAGE_CHANGE:
		case ICS_MESSAGE_FLAG:
		case ICS_MESSAGE_SOFT_DELETE:
		case ICS_MESSAGE_HARD_DELETE:
		case ICS_MESSAGE_NEW:
		case ICS_FOLDER_CHANGE:
		case ICS_FOLDER_SOFT_DELETE:
		case ICS_FOLDER_HARD_DELETE:
		case ICS_FOLDER_NEW:
			return true;
		default:
			return false;
	}
}

ECRESULT ConvertABEntryIDToSoapSourceKey(struct soap *soap, ECSession *lpSession, bool bUseV1, unsigned int cbEntryId, char *lpEntryId, unsigned int ulCompanyFilter, xsd__base64Binary *lpSourceKey)
{
	ECRESULT			er			= erSuccess;
	unsigned int		cbAbeid		= cbEntryId;
	PABEID				lpAbeid		= (PABEID)lpEntryId;
	entryId				sEntryId	= {0};
	SOURCEKEY			sSourceKey;
	objectid_t			sExternId;

	if (cbEntryId < CbNewABEID("") || lpEntryId == NULL || lpSourceKey == NULL)
	{
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	if (ulCompanyFilter != 0) {
		string strQuery = "SELECT 0 from users where id = " + stringify(lpAbeid->ulId) + " AND company = " + stringify(ulCompanyFilter);
		DB_RESULT		lpDBResult = NULL;
		DB_ROW			lpDBRow = NULL;
		ECDatabase *lpDatabase = NULL;

		er = lpSession->GetDatabase(&lpDatabase);
		if (er != erSuccess)
			goto exit;

		er = lpDatabase->DoSelect(strQuery, &lpDBResult);
		if (er != erSuccess)
			goto exit;
		
		lpDBRow = lpDatabase->FetchRow(lpDBResult);
		lpDatabase->FreeResult(lpDBResult);
		if (lpDBRow == NULL) {
			// no rows, so filtered object
			er = ZARAFA_E_NO_ACCESS;
			goto exit;
		}
	}

	if (lpAbeid->ulVersion == 1 && !bUseV1)
	{
		er = MAPITypeToType(lpAbeid->ulType, &sExternId.objclass);
		if (er != erSuccess)
			goto exit;

		er = ABIDToEntryID(soap, lpAbeid->ulId, sExternId, &sEntryId);	// Creates a V0 EntryID, because sExternId.id is empty
		if (er != erSuccess)
			goto exit;

		lpAbeid = (PABEID)sEntryId.__ptr;
		cbAbeid = sEntryId.__size;
	} 
	else if (lpAbeid->ulVersion == 0 && bUseV1) 
	{
		er = lpSession->GetUserManagement()->GetABSourceKeyV1(lpAbeid->ulId, &sSourceKey);
		if (er != erSuccess)
			goto exit;

		lpAbeid = (PABEID)(unsigned char*)sSourceKey;
		cbAbeid = sSourceKey.size();
	}

	lpSourceKey->__size = cbAbeid;
	lpSourceKey->__ptr = (unsigned char*)s_memcpy(soap, (char*)lpAbeid, cbAbeid);

exit:
	return er;
}

void AddChangeKeyToChangeList(std::string * strChangeList, ULONG cbChangeKey, const char * lpChangeKey){

	if(cbChangeKey <= sizeof(GUID) || cbChangeKey > 255)
		return;

	ULONG ulPos = 0;
	ULONG ulSize = 0;
	bool bFound = false;

	std::string strChangeKey;

	if(cbChangeKey > 0xFF)
		return;

	strChangeKey.assign((char *)&cbChangeKey,1);
	strChangeKey.append(lpChangeKey, cbChangeKey);

	while(ulPos < strChangeList->size()){
		ulSize = strChangeList->at(ulPos);
		if(ulSize <= sizeof(GUID) || ulSize == (ULONG)-1){
			break;
		}else if(memcmp(strChangeList->substr(ulPos+1, ulSize).c_str(), lpChangeKey, sizeof(GUID)) == 0){
			strChangeList->replace(ulPos, ulSize+1, strChangeKey);
			bFound = true;
		}
		ulPos += ulSize + 1;
	}
	if(!bFound){
		strChangeList->append(strChangeKey);
	}
}

ECRESULT AddChange(BTSession *lpSession, unsigned int ulSyncId, SOURCEKEY sSourceKey, SOURCEKEY sParentSourceKey, unsigned int ulChange, unsigned int ulFlags, bool fForceNewChangeKey, std::string * lpstrChangeKey, std::string * lpstrChangeList){
	ECRESULT		er = erSuccess;
	std::string		strQuery;
	ECDatabase*		lpDatabase = NULL;
	DB_RESULT		lpDBResult = NULL;
	DB_ROW			lpDBRow = NULL;
	DB_LENGTHS		lpDBLen = NULL;
	unsigned int	changeid = 0;
	unsigned int	ulObjId = 0;

	char			szChangeKey[20];
	std::string		strChangeList;
	bool			bLogAllChanges = false;

	std::set<unsigned int>	syncids;
	bool					bIgnored = false;

	if(!isICSChange(ulChange)){
		er = ZARAFA_E_INVALID_TYPE;
		goto exit;
	}

	if(sSourceKey == sParentSourceKey || sSourceKey.empty() || sParentSourceKey.empty()) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	er = lpSession->GetDatabase(&lpDatabase);
	if (er != erSuccess)
		goto exit;

	bLogAllChanges = parseBool(g_lpSessionManager->GetConfig()->GetSetting("sync_log_all_changes"));

	// Always log folder changes, and when "sync_log_all_changes" is enabled.
	if(ulChange & ICS_MESSAGE) {
		// See if anybody is interested in this change. If nobody has subscribed to this folder (ie nobody has got a state on this folder)
		// then we can ignore the change.

        strQuery = 	"SELECT id FROM syncs "
                    "WHERE sourcekey=" + lpDatabase->EscapeBinary(sParentSourceKey, sParentSourceKey.size());

		er = lpDatabase->DoSelect(strQuery, &lpDBResult);
		if(er != erSuccess)
			goto exit;

		while (true) {
			lpDBRow = lpDatabase->FetchRow(lpDBResult);
			if (lpDBRow == NULL)
				break;
			unsigned int ulTmp = atoui((char*)lpDBRow[0]);
			if (ulTmp != ulSyncId)
				syncids.insert(ulTmp);
			else
				bIgnored = true;
		}

		lpDatabase->FreeResult(lpDBResult);
		lpDBResult = NULL;

		if (!bLogAllChanges && !bIgnored && syncids.empty()) {
			// nothing to do
			goto exit;
		}
    }

	// Record the change
    strQuery = 	"REPLACE INTO changes(change_type, sourcekey, parentsourcekey, sourcesync, flags) "
				"VALUES (" + stringify(ulChange) +
				  ", " + lpDatabase->EscapeBinary(sSourceKey, sSourceKey.size()) +
				  ", " + lpDatabase->EscapeBinary(sParentSourceKey, sParentSourceKey.size()) +
				  ", " + stringify(ulSyncId) +
				  ", " + stringify(ulFlags) +
				")";

	er = lpDatabase->DoInsert(strQuery, &changeid , NULL);
	if(er != erSuccess)
		goto exit;

	if (ulChange == ICS_MESSAGE_HARD_DELETE || ulChange == ICS_FOLDER_HARD_DELETE) {
		if (ulSyncId != 0)
			er = RemoveFromLastSyncedMessagesSet(lpDatabase, ulSyncId, sSourceKey, sParentSourceKey);
		// The real item is removed from the database, So no further actions needed
		goto exit;
	}

	if (ulChange == ICS_MESSAGE_NEW && ulSyncId != 0) {
		er = AddToLastSyncedMessagesSet(lpDatabase, ulSyncId, sSourceKey, sParentSourceKey);
		if (er != erSuccess)
			goto exit;
	}

	// Add change key and predecessor change list
	er = g_lpSessionManager->GetCacheManager()->GetObjectFromProp(PROP_ID(PR_SOURCE_KEY), sSourceKey.size(), sSourceKey, &ulObjId);
	if(er == ZARAFA_E_NOT_FOUND){
		er = erSuccess; //hard deleted items can't be updated
		goto exit;
	}
	if(er != erSuccess)
        goto exit;

	strChangeList = "";
	strQuery = "SELECT val_binary FROM properties "
				"WHERE tag = " + stringify(PROP_ID(PR_PREDECESSOR_CHANGE_LIST)) +
				" AND type = " + stringify(PROP_TYPE(PR_PREDECESSOR_CHANGE_LIST)) +
				" AND hierarchyid = " + stringify(ulObjId);

	er = lpDatabase->DoSelect(strQuery, &lpDBResult);
	if(er != erSuccess)
		goto exit;

	if(lpDatabase->GetNumRows(lpDBResult) > 0){
		lpDBRow = lpDatabase->FetchRow(lpDBResult);
		lpDBLen = lpDatabase->FetchRowLengths(lpDBResult);

		if(lpDBRow && lpDBRow[0] && lpDBLen && lpDBLen[0] >16){
			strChangeList.assign(lpDBRow[0], lpDBLen[0]);
		}
	}

	if(lpDBResult){
		lpDatabase->FreeResult(lpDBResult);
		lpDBResult = NULL;
	}

	strQuery = "SELECT val_binary FROM properties "
				"WHERE tag = " + stringify(PROP_ID(PR_CHANGE_KEY)) +
				" AND type = " + stringify(PROP_TYPE(PR_CHANGE_KEY)) +
				" AND hierarchyid = " + stringify(ulObjId);

	er = lpDatabase->DoSelect(strQuery, &lpDBResult);
	if(er != erSuccess)
		goto exit;

	if(lpDatabase->GetNumRows(lpDBResult) > 0){
		lpDBRow = lpDatabase->FetchRow(lpDBResult);
		lpDBLen = lpDatabase->FetchRowLengths(lpDBResult);

		if(lpDBRow && lpDBRow[0] && lpDBLen && lpDBLen[0] >16){
			AddChangeKeyToChangeList(&strChangeList, lpDBLen[0], lpDBRow[0]);
		}
	}

	/**
	 * There are two reasons for generating a new change key:
	 * 1. ulSyncId == 0. When this happens we have a modification initiated from the online server.
	 *    Since we got this far, there are other servers/devices monitoring this object, so a new
	 *    change key needs to be generated.
	 * 2. fForceNewChangeKey == true. When this happens the caller has determined that a new change key
	 *    needs to be generated. (We can't do this much earlier as we need the changeid, which we only
	 *    get once the change has been registed in the database.) At this time this will only happen when
	 *    no change key was send by the client when calling ns__saveObject. This is the case when a change
	 *    occurs on the local server (actually making check 1 superfluous) or when a change is received
	 *    through z-push.
	 **/
	if(ulSyncId == 0 || fForceNewChangeKey == true)
	{
		struct propVal sProp;
		struct xsd__base64Binary sBin;
		struct sObjectTableKey key;
		
		// Add the new change key to the predecessor change list
		lpSession->GetServerGUID((GUID*)szChangeKey);
		memcpy(szChangeKey + sizeof(GUID), &changeid, 4);

		AddChangeKeyToChangeList(&strChangeList, sizeof(szChangeKey), szChangeKey);

		// Write PR_PREDECESSOR_CHANGE_LIST and PR_CHANGE_KEY
		sProp.ulPropTag = PR_PREDECESSOR_CHANGE_LIST;
		sProp.__union = SOAP_UNION_propValData_bin;
		sProp.Value.bin = &sBin;
		sProp.Value.bin->__ptr = (BYTE *)strChangeList.c_str();
		sProp.Value.bin->__size = strChangeList.size();
		
		er = WriteProp(lpDatabase, ulObjId, 0, &sProp);
		if(er != erSuccess)
			goto exit;
			
		key.ulObjId = ulObjId;
		key.ulOrderId = 0;
		lpSession->GetSessionManager()->GetCacheManager()->SetCell(&key, PR_PREDECESSOR_CHANGE_LIST, &sProp);

		sProp.ulPropTag = PR_CHANGE_KEY;
		sProp.__union = SOAP_UNION_propValData_bin;
		sProp.Value.bin = &sBin;
		sProp.Value.bin->__ptr = (BYTE *)szChangeKey;
		sProp.Value.bin->__size = sizeof(szChangeKey);
		
		er = WriteProp(lpDatabase, ulObjId, 0, &sProp);
		if(er != erSuccess)
			goto exit;
			
		key.ulObjId = ulObjId;
		key.ulOrderId = 0;
		lpSession->GetSessionManager()->GetCacheManager()->SetCell(&key, PR_CHANGE_KEY, &sProp);
		
		if(lpstrChangeKey){
			lpstrChangeKey->assign(szChangeKey, sizeof(szChangeKey));
		}
		if(lpstrChangeList){
			lpstrChangeList->assign(strChangeList);
		}
	}

exit:
	// FIXME: We should not send notifications while we're in the middle of a transaction.
	if (er == erSuccess && !syncids.empty())
		g_lpSessionManager->NotificationChange(syncids, changeid, ulChange);

	if(lpDBResult)
		lpDatabase->FreeResult(lpDBResult);

	return er;
}

void* CleanupSyncsTable(void* lpTmpMain){
	ECRESULT		er = erSuccess;
	std::string		strQuery;
	ECDatabase*		lpDatabase = NULL;
	ECSession*		lpSession = NULL;
	unsigned int	ulSyncLifeTime = 0;
	unsigned int	ulDeletedSyncs = 0;

	g_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_INFO, "Start syncs table clean up");

	ulSyncLifeTime = atoui(g_lpSessionManager->GetConfig()->GetSetting("sync_lifetime"));

	if(ulSyncLifeTime == 0)
		goto exit;

	er = g_lpSessionManager->CreateSessionInternal(&lpSession);
	if(er != erSuccess)
		goto exit;

	lpSession->Lock();

	er = lpSession->GetDatabase(&lpDatabase);
	if (er != erSuccess)
		goto exit;
	
	strQuery = "DELETE FROM syncs WHERE sync_time < DATE_SUB(FROM_UNIXTIME("+stringify(time(NULL))+"), INTERVAL " + stringify(ulSyncLifeTime) + " DAY)";
	er = lpDatabase->DoDelete(strQuery, &ulDeletedSyncs);
	if(er != erSuccess)
		goto exit;

exit:
	if(er == erSuccess)
		g_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_INFO, "syncs table clean up done: removed syncs: %d", ulDeletedSyncs);
	else
		g_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_INFO, "syncs table clean up failed: error 0x%08X, removed syncs: %d", er, ulDeletedSyncs);

	if(lpSession) {
		lpSession->Unlock(); // Lock the session
		g_lpSessionManager->RemoveSessionInternal(lpSession);
	}

	// ECScheduler does nothing with the returned value
	return NULL;
}

// @note, this cleanup takes a *very* long time, and clears very little,
// thus this function is not scheduled.
void* CleanupChangesTable(void* lpTmpMain){
	ECRESULT		er = erSuccess;
	std::string		strQuery;
	ECDatabase*		lpDatabase = NULL;
	ECSession*		lpSession = NULL;
	DB_RESULT		lpDBResult = NULL;
	DB_ROW			lpDBRow = NULL;
	unsigned int	ulDeletedChanges = 0;
	unsigned int	ulDeletedChangesJoin = 0;

	g_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_INFO, "Start changes table clean up");

	er = g_lpSessionManager->CreateSessionInternal(&lpSession);
	if(er != erSuccess)
		goto exit;

	lpSession->Lock();

	er = lpSession->GetDatabase(&lpDatabase);
	if (er != erSuccess)
		goto exit;

	//delete all changes < oldest/lowest sync
	strQuery = "SELECT MIN(change_id) FROM syncs WHERE change_id > 0";
	er = lpDatabase->DoSelect(strQuery, &lpDBResult);
	if(er != erSuccess)
		goto exit;

	lpDBRow = lpDatabase->FetchRow(lpDBResult);
	if(lpDBRow && lpDBRow[0]){
		strQuery = "DELETE FROM changes WHERE id < " + stringify(atoui(lpDBRow[0])) + " AND changes.change_type & " + stringify(ICS_MESSAGE | ICS_FOLDER);
	}else{
		//no oldest/lowest sync, delete all changes
		strQuery = "DELETE FROM changes WHERE changes.change_type & " + stringify(ICS_MESSAGE | ICS_FOLDER);
		er = lpDatabase->DoDelete(strQuery, &ulDeletedChanges);
		goto exit;
	}
	er = lpDatabase->DoDelete(strQuery, &ulDeletedChanges);
	if(er != erSuccess)
		goto exit;

    strQuery =  "DELETE changes.* FROM changes " /* delete changes */
                 "LEFT JOIN syncs ON syncs.sourcekey = changes.parentsourcekey AND syncs.sync_type = 1 " /* join with ICS_MESSAGE syncs */
                 "WHERE syncs.sourcekey IS NULL "								/* with no existing sync, and therefore not being tracked */
                 "AND changes.change_type & " + stringify(ICS_MESSAGE);		/* and with ICS_MESSAGE change type */

	er = lpDatabase->DoDelete(strQuery, &ulDeletedChangesJoin);
	if(er != erSuccess)
		goto exit;

    //TODO: delete all synced contents changes (removed for speed problems with previous query)
	//TODO: delete all synced hierarchy changes
	//loop from store up, delete every synced folder

exit:
	if(er == erSuccess)
		g_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_INFO, "changes table clean up done, %d entries removed", ulDeletedChanges + ulDeletedChangesJoin);
	else
		g_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_INFO, "changes table clean up failed, error: 0x%08X, %d entries removed", er, ulDeletedChanges + ulDeletedChangesJoin);

	if(lpDBResult)
		lpDatabase->FreeResult(lpDBResult);

	if(lpSession) {
		lpSession->Unlock(); // Lock the session
		g_lpSessionManager->RemoveSessionInternal(lpSession);
	}

	// ECScheduler does nothing with the returned value
	return NULL;
}

void *CleanupSyncedMessagesTable(void *lpTmpMain)
{
	ECRESULT		er = erSuccess;
	std::string		strQuery;
	ECDatabase*		lpDatabase = NULL;
	ECSession*		lpSession = NULL;
	DB_RESULT		lpDBResult = NULL;
	unsigned int	ulDeleted;

	g_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_INFO, "Start syncedmessages table clean up");

	er = g_lpSessionManager->CreateSessionInternal(&lpSession);
	if(er != erSuccess)
		goto exit;

	lpSession->Lock();

	er = lpSession->GetDatabase(&lpDatabase);
	if (er != erSuccess)
		goto exit;

    strQuery =  "DELETE syncedmessages.* FROM syncedmessages "
                 "LEFT JOIN syncs ON syncs.id = syncedmessages.sync_id " 	/* join with syncs */
                 "WHERE syncs.id IS NULL";									/* with no existing sync, and therefore not being tracked */

	er = lpDatabase->DoDelete(strQuery, &ulDeleted);
	if(er != erSuccess)
		goto exit;

exit:
	if(er == erSuccess)
		g_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_INFO, "syncedmessages table clean up done, %d entries removed", ulDeleted);
	else
		g_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_INFO, "syncedmessages table clean up failed, error: 0x%08X, %d entries removed", er, ulDeleted);

	if(lpDBResult)
		lpDatabase->FreeResult(lpDBResult);

	if(lpSession) {
		lpSession->Unlock(); // Lock the session
		g_lpSessionManager->RemoveSessionInternal(lpSession);
	}

	// ECScheduler does nothing with the returned value
	return NULL;
}

ECRESULT GetChanges(struct soap *soap, ECSession *lpSession, SOURCEKEY sFolderSourceKey, unsigned int ulSyncId, unsigned int ulChangeId, unsigned int ulChangeType, unsigned int ulFlags, struct restrictTable *lpsRestrict, unsigned int *lpulMaxChangeId, icsChangesArray **lppChanges){
	ECRESULT		er = erSuccess;
	ECDatabase*		lpDatabase = NULL;
	DB_RESULT		lpDBResult	= NULL;
	DB_ROW			lpDBRow;
	DB_LENGTHS		lpDBLen;
	unsigned int	ulMaxChange = 0;
	unsigned int	i = 0;
	unsigned int	ulChanges = 0;
	std::string		strQuery;
	unsigned int	ulFolderId = 0;
	icsChangesArray*lpChanges = NULL;
	bool			bAcceptABEID = false;
	unsigned char	*lpSourceKeyData = NULL;
	unsigned int	cbSourceKeyData = 0;
	
	list<unsigned int> lstFolderIds;
	list<unsigned int>::iterator lpFolderId;

	// Contains a list of change IDs
	list<unsigned int> lstChanges;
	list<unsigned int>::iterator iterChanges;

	ECGetContentChangesHelper *lpHelper = NULL;
	
    // Get database object
    er = lpSession->GetDatabase(&lpDatabase);
    if (er != erSuccess)
        goto exit;

    er = lpDatabase->Begin();
    if (er != erSuccess)
		goto exit;

    // CHeck if the client understands the new ABEID.
	if ((lpSession->GetCapabilities() & ZARAFA_CAP_MULTI_SERVER) == ZARAFA_CAP_MULTI_SERVER)
		bAcceptABEID = true;


	if(ulChangeType != ICS_SYNC_AB) {
        // You must first register your sync via setSyncStatus()
        if(ulSyncId == 0) {
            er = ZARAFA_E_INVALID_PARAMETER;
            goto exit;
        }

        // Add change key and predecessor change list
        if(ulChangeId > 0) {
            // Change ID > 0, which means we will be doing a real differential sync starting from the
            // given change ID.
            strQuery = 
				"SELECT change_id,sourcekey,sync_type "
				"FROM syncs "
				"WHERE id=" + stringify(ulSyncId) + " FOR UPDATE";

            er = lpDatabase->DoSelect(strQuery, &lpDBResult);
            if(er != erSuccess)
                goto exit;

            if(lpDatabase->GetNumRows(lpDBResult) == 0){
                er = ZARAFA_E_NOT_FOUND;
                goto exit;
            }

            lpDBRow = lpDatabase->FetchRow(lpDBResult);
            lpDBLen = lpDatabase->FetchRowLengths(lpDBResult);

            if( lpDBRow == NULL || lpDBRow[0] == NULL || lpDBRow[1] == NULL || lpDBRow[2] == NULL){
                er = ZARAFA_E_DATABASE_ERROR; // this should never happen
                goto exit;
            }

            if(atoui(lpDBRow[2]) != ulChangeType){
                er = ZARAFA_E_COLLISION;
                goto exit;
            }

		} else {

			strQuery = 
				"SELECT change_id,sourcekey "
				"FROM syncs "
				"WHERE id=" + stringify(ulSyncId) + " FOR UPDATE";
			er = lpDatabase->DoSelect(strQuery, &lpDBResult);
			if(er != erSuccess)
				goto exit;

			lpDBRow = lpDatabase->FetchRow(lpDBResult);
            lpDBLen = lpDatabase->FetchRowLengths(lpDBResult);

			if( lpDBRow == NULL || lpDBRow[0] == NULL || lpDBRow[1] == NULL) {
				er = ZARAFA_E_DATABASE_ERROR;
				goto exit;
			}
		}

		ulMaxChange = atoui(lpDBRow[0]);
		sFolderSourceKey = SOURCEKEY(lpDBLen[1], lpDBRow[1]);
		
        if(lpDBResult)
            lpDatabase->FreeResult(lpDBResult);
        lpDBResult = NULL;

        if(!sFolderSourceKey.empty()) {
            er = g_lpSessionManager->GetCacheManager()->GetObjectFromProp(PROP_ID(PR_SOURCE_KEY), sFolderSourceKey.size(), sFolderSourceKey, &ulFolderId);
            if(er != erSuccess)
                goto exit;
        } else {
            ulFolderId = 0;
        }
        
        if(ulFolderId == 0) {
            if(lpSession->GetSecurity()->GetAdminLevel() != ADMIN_LEVEL_SYSADMIN)
                er = ZARAFA_E_NO_ACCESS;
        } else {
            if(ulChangeType == ICS_SYNC_CONTENTS) {
                er = lpSession->GetSecurity()->CheckPermission(ulFolderId, ecSecurityRead);
            }else if(ulChangeType == ICS_SYNC_HIERARCHY){
                er = lpSession->GetSecurity()->CheckPermission(ulFolderId, ecSecurityFolderVisible);
            }else{
                er = ZARAFA_E_INVALID_TYPE;
            }
        }
        
        if(er != erSuccess)
            goto exit;
	}

	if(ulChangeType == ICS_SYNC_CONTENTS){
		er = ECGetContentChangesHelper::Create(soap, lpSession, lpDatabase, sFolderSourceKey, ulSyncId, ulChangeId, ulFlags, lpsRestrict, &lpHelper);
		if (er != erSuccess)
			goto exit;
		
		er = lpHelper->QueryDatabase(&lpDBResult);
		if (er != erSuccess)
			goto exit;
			
		while (lpDBResult && (lpDBRow = lpDatabase->FetchRow(lpDBResult))) {
			lpDBLen = lpDatabase->FetchRowLengths(lpDBResult);
		
			er = lpHelper->ProcessRow(lpDBRow, lpDBLen);
			if (er != erSuccess)
				goto exit;
		}
		
		er = lpHelper->ProcessResidualMessages();
		if (er != erSuccess)
			goto exit;
			
		er = lpHelper->Finalize(&ulMaxChange, &lpChanges);
		if (er != erSuccess)
			goto exit;

		// Do stuff with lppChanges etc.

	}else if(ulChangeType == ICS_SYNC_HIERARCHY){

		// We traverse the tree by just looking at the current hierarchy. This means we will not traverse into deleted
		// folders and changes within those deleted folders will therefore never reach whoever is requesting changes. In
		// practice this shouldn't matter because the folder above will be deleted correctly.
        lstFolderIds.push_back(ulFolderId);

		// Recursive loop through all folders
		for(lpFolderId = lstFolderIds.begin(); lpFolderId != lstFolderIds.end(); lpFolderId++){
			if(*lpFolderId == 0) {
			    if(lpSession->GetSecurity()->GetAdminLevel() != ADMIN_LEVEL_SYSADMIN) {
			        er = ZARAFA_E_NO_ACCESS;
			        goto exit;
                }
            } else if(lpSession->GetSecurity()->CheckPermission(*lpFolderId, ecSecurityFolderVisible) != erSuccess){
                // We cannot traverse folders that we have no permission to. This means that changes under the inaccessible folder
                // will disappear - this will cause the sync peer to go out-of-sync. Fix would be to remember changes in security
                // as deletes and adds.
				continue;
			}

			if(ulChangeId != 0){
			
			    if(*lpFolderId != 0) {
    				if(g_lpSessionManager->GetCacheManager()->GetPropFromObject(PROP_ID(PR_SOURCE_KEY), *lpFolderId, NULL, &cbSourceKeyData, &lpSourceKeyData) != erSuccess)
	    				goto nextFolder; // Item is hard deleted?
                }

				// Search folder changed folders
				strQuery = 	"SELECT changes.id "
							"FROM changes "
							"WHERE "
							"  changes.id > " + stringify(ulChangeId) + 									// Change ID is N or later
							"  AND changes.change_type >= " + stringify(ICS_FOLDER) +						// query optimizer
							"  AND changes.change_type & " + stringify(ICS_FOLDER) + " != 0" +				// Change is a folder change
							"  AND changes.sourcesync != " + stringify(ulSyncId);
							
                if(*lpFolderId != 0) {
                    strQuery += "  AND parentsourcekey=" + lpDatabase->EscapeBinary(lpSourceKeyData, cbSourceKeyData);
                }

				er = lpDatabase->DoSelect(strQuery, &lpDBResult);
				if(er != erSuccess)
					goto exit;

				while( (lpDBRow = lpDatabase->FetchRow(lpDBResult)) ){
					if( lpDBRow == NULL || lpDBRow[0] == NULL){
						er = ZARAFA_E_DATABASE_ERROR; // this should never happen
						goto exit;
					}
					lstChanges.push_back(atoui(lpDBRow[0]));
				}

				if(lpDBResult) {
					lpDatabase->FreeResult(lpDBResult);
					lpDBResult = NULL;
				}

nextFolder:
				if(lpSourceKeyData) {
					delete []lpSourceKeyData;
					lpSourceKeyData = NULL;
				}
			}

			if(*lpFolderId != 0) {
                // Get subfolders for recursion
                strQuery = "SELECT id FROM hierarchy WHERE parent = " + stringify(*lpFolderId) + " AND type = " + stringify(MAPI_FOLDER) + " AND flags = " + stringify(FOLDER_GENERIC);

                if(ulFlags & SYNC_NO_SOFT_DELETIONS) {
                    strQuery += " AND hierarchy.flags & 1024 = 0";
                }

                er = lpDatabase->DoSelect(strQuery, &lpDBResult);
                if(er != erSuccess)
                    goto exit;

                while( (lpDBRow = lpDatabase->FetchRow(lpDBResult)) ){

                    if( lpDBRow == NULL || lpDBRow[0] == NULL){
                        er = ZARAFA_E_DATABASE_ERROR; // this should never happen
                        goto exit;
                    }

                    lstFolderIds.push_back(atoui(lpDBRow[0]));
                }

                if(lpDBResult)
                    lpDatabase->FreeResult(lpDBResult);
                lpDBResult = NULL;
            }
		}

		lstChanges.sort();
		lstChanges.unique();
		
		// We now have both a list of all folders, and and list of changes.

		if(ulChangeId == 0) {
		    if(ulFolderId == 0 && (ulFlags & SYNC_CATCHUP) == 0) {
		        // Do not allow initial sync of all server folders
		        er = ZARAFA_E_NO_SUPPORT;
		        goto exit;
            }
            
			// New sync, just return all the folders as changes
			lpChanges = (icsChangesArray *)soap_malloc(soap, sizeof(icsChangesArray));
			lpChanges->__ptr = (icsChange *)soap_malloc(soap, sizeof(icsChange) * lstFolderIds.size());

			// Search folder changed folders
			strQuery = "SELECT MAX(id) FROM changes";
			er = lpDatabase->DoSelect(strQuery, &lpDBResult);
			if(er != erSuccess)
				goto exit;

			lpDBRow = lpDatabase->FetchRow(lpDBResult);
			if( lpDBRow == NULL){
				er = ZARAFA_E_DATABASE_ERROR; // this should never happen
				goto exit;
			}
			ulMaxChange = (lpDBRow[0] == NULL ? 0 : atoui(lpDBRow[0]));

			i = 0;
			
			if((ulFlags & SYNC_CATCHUP) == 0) {
                for(lpFolderId = lstFolderIds.begin(); lpFolderId != lstFolderIds.end(); lpFolderId++) {

                    if(*lpFolderId == ulFolderId)
                        continue; // don't send the folder itself as a change

                    strQuery = 	"SELECT sourcekey.val_binary, parentsourcekey.val_binary "
                                "FROM hierarchy "
                                "JOIN indexedproperties AS sourcekey ON hierarchy.id=sourcekey.hierarchyid AND sourcekey.tag=" + stringify(PROP_ID(PR_SOURCE_KEY)) + " "
                                "JOIN indexedproperties AS parentsourcekey ON hierarchy.parent=parentsourcekey.hierarchyid AND parentsourcekey.tag=" + stringify(PROP_ID(PR_SOURCE_KEY)) + " "
                                "WHERE hierarchy.id=" + stringify(*lpFolderId);

                    if(lpDBResult)
                        lpDatabase->FreeResult(lpDBResult);
                    lpDBResult = NULL;

                    er = lpDatabase->DoSelect(strQuery, &lpDBResult);
                    if(er != erSuccess)
                        goto exit;

                    lpDBRow = lpDatabase->FetchRow(lpDBResult);
                    lpDBLen = lpDatabase->FetchRowLengths(lpDBResult);

                    if(lpDBRow == NULL || lpDBRow[0] == NULL || lpDBRow[1] == NULL)
                        continue;

                    lpChanges->__ptr[i].ulChangeId = ulMaxChange; // All items have the latest change ID because this is an initial sync

                    lpChanges->__ptr[i].sSourceKey.__ptr = (unsigned char *)soap_malloc(soap, lpDBLen[0]);
                    lpChanges->__ptr[i].sSourceKey.__size = lpDBLen[0];
                    memcpy(lpChanges->__ptr[i].sSourceKey.__ptr, lpDBRow[0], lpDBLen[0]);

                    lpChanges->__ptr[i].sParentSourceKey.__ptr = (unsigned char *)soap_malloc(soap, lpDBLen[1]);
                    lpChanges->__ptr[i].sParentSourceKey.__size = lpDBLen[1];
                    memcpy(lpChanges->__ptr[i].sParentSourceKey.__ptr, lpDBRow[1], lpDBLen[1]);

                    lpChanges->__ptr[i].ulChangeType = ICS_FOLDER_CHANGE;

                    lpChanges->__ptr[i].ulFlags = 0;

                    if(lpDBResult)
                        lpDatabase->FreeResult(lpDBResult);
                    lpDBResult = NULL;

                    i++;
                }
            }
			lpChanges->__size = i;
		} else {
			// Return all the found changes
			lpChanges = (icsChangesArray *)soap_malloc(soap, sizeof(icsChangesArray));
			lpChanges->__ptr = (icsChange *)soap_malloc(soap, sizeof(icsChange) * lstChanges.size());
			lpChanges->__size = lstChanges.size();

			i = 0;
			for(iterChanges = lstChanges.begin(); iterChanges != lstChanges.end(); iterChanges++) {
				strQuery = "SELECT changes.id, changes.sourcekey, changes.parentsourcekey, changes.change_type, changes.flags FROM changes WHERE changes.id="+stringify(*iterChanges);

				er = lpDatabase->DoSelect(strQuery, &lpDBResult);
				if(er != erSuccess)
					goto exit;

				lpDBRow = lpDatabase->FetchRow(lpDBResult);
				lpDBLen = lpDatabase->FetchRowLengths(lpDBResult);

				if(lpDBRow == NULL || lpDBRow[0] == NULL || lpDBRow[1] == NULL || lpDBRow[2] == NULL || lpDBRow[3] == NULL) {
					er = ZARAFA_E_DATABASE_ERROR;
					goto exit;
				}

				lpChanges->__ptr[i].ulChangeId = atoui(lpDBRow[0]);
				if(lpChanges->__ptr[i].ulChangeId > ulMaxChange)
					ulMaxChange = lpChanges->__ptr[i].ulChangeId;

				lpChanges->__ptr[i].sSourceKey.__ptr = (unsigned char *)soap_malloc(soap, lpDBLen[1]);
				lpChanges->__ptr[i].sSourceKey.__size = lpDBLen[1];
				memcpy(lpChanges->__ptr[i].sSourceKey.__ptr, lpDBRow[1], lpDBLen[1]);

				lpChanges->__ptr[i].sParentSourceKey.__ptr = (unsigned char *)soap_malloc(soap, lpDBLen[2]);
				lpChanges->__ptr[i].sParentSourceKey.__size = lpDBLen[2];
				memcpy(lpChanges->__ptr[i].sParentSourceKey.__ptr, lpDBRow[2], lpDBLen[2]);

				lpChanges->__ptr[i].ulChangeType = atoui(lpDBRow[3]);

				if(lpDBRow[4]) {
					lpChanges->__ptr[i].ulFlags = atoui(lpDBRow[4]);
				} else {
					 lpChanges->__ptr[i].ulFlags = 0;
				}

				if(lpDBResult)
					lpDatabase->FreeResult(lpDBResult);
				lpDBResult = NULL;

				i++;
			}
			lpChanges->__size = i;
		}
	} else if(ulChangeType == ICS_SYNC_AB) {
		// filter on the current logged on company (0 when non-hosted server)
		// since we need to filter, we can't filter on "viewable
		// companies" because after updating the allowed viewable, we
		// won't see the changes as newly created anymore, and thus
		// still don't have that company in the offline gab.
		unsigned int ulCompanyId = 0;

		// returns 0 for ZARAFA_UID_SYSTEM on hosted environments, and then the filter correctly doesn't filter anything.
		lpSession->GetSecurity()->GetUserCompany(&ulCompanyId);

	    if(ulChangeId > 0) {
			// sourcekey is actually an entryid .. don't let the naming confuse you
            strQuery = "SELECT id, sourcekey, parentsourcekey, change_type FROM abchanges WHERE change_type & " + stringify(ICS_AB) + " AND id > " + stringify(ulChangeId) + " ORDER BY id";

            er = lpDatabase->DoSelect(strQuery, &lpDBResult);
            if(er != erSuccess)
                goto exit;

            ulChanges = lpDatabase->GetNumRows(lpDBResult);

            lpChanges = (icsChangesArray *)soap_malloc(soap, sizeof(icsChangesArray));
            lpChanges->__ptr = (icsChange *)soap_malloc(soap, sizeof(icsChange) * ulChanges);
            lpChanges->__size = 0;

            memset(lpChanges->__ptr, 0, sizeof(icsChange) * ulChanges);

            i=0;
            while(1) {
                lpDBRow = lpDatabase->FetchRow(lpDBResult);
                lpDBLen = lpDatabase->FetchRowLengths(lpDBResult);

                if(lpDBRow == NULL)
                    break;

                if( lpDBRow == NULL || lpDBRow[0] == NULL || lpDBRow[1] == NULL || lpDBRow[2] == NULL){
                    er = ZARAFA_E_DATABASE_ERROR; // this should never happen
                    goto exit;
                }

                lpChanges->__ptr[i].ulChangeId = atoui(lpDBRow[0]);
                er = ConvertABEntryIDToSoapSourceKey(soap, lpSession, bAcceptABEID, lpDBLen[1], lpDBRow[1], ulCompanyId, &lpChanges->__ptr[i].sSourceKey);
                if (er != erSuccess) {
					// upgrade ulMaxChange on filtered object to skip in the future earlier.
					if (er == ZARAFA_E_NO_ACCESS && atoui(lpDBRow[0]) > ulMaxChange)
						ulMaxChange = atoui(lpDBRow[0]);
					
                    er = erSuccess;
                    continue;
                }

                lpChanges->__ptr[i].sParentSourceKey.__ptr = (unsigned char *)s_memcpy(soap, lpDBRow[2], lpDBLen[2]);
                lpChanges->__ptr[i].sParentSourceKey.__size = lpDBLen[2];
                lpChanges->__ptr[i].ulChangeType = atoui(lpDBRow[3]);

                if(atoui(lpDBRow[0]) > ulMaxChange)
                    ulMaxChange = atoui(lpDBRow[0]);

                i++;
            }

            lpChanges->__size = i;
        } else {
            // During initial sync, just get all the current users from the database and output them as ICS_AB_NEW changes
            ABEID eid(MAPI_ABCONT, *(GUID*)&MUIDECSAB_SERVER, 1);
            
            strQuery = "SELECT max(id) FROM abchanges";
            er = lpDatabase->DoSelect(strQuery, &lpDBResult);
            if (er != erSuccess)
                goto exit;
                
            lpDBRow = lpDatabase->FetchRow(lpDBResult);
            if(!lpDBRow || !lpDBRow[0]) {
                // You *should* always have something there if you have more than 0 users. However, just to make us compatible with
                // people truncating the table and then doing a resync, we'll go to change ID 0. This means that at the next sync,
                // we will do another full sync. We can't really fix that at the moment since going to change ID 1 would be even worse,
                // since it would skip the first change in the table. This could cause the problem that deletes after the first initial
                // sync and before the second would skip any deletes. This is acceptable for now since it is rare to do this, and it's
                // better than any other alternative.
                ulMaxChange = 0;
            } else {
                ulMaxChange = atoui(lpDBRow[0]);
            }
            
            lpDatabase->FreeResult(lpDBResult);
            lpDBResult = NULL;

			// Skip 'everyone', 'system' and users outside our company space, except when we're system (zarafa-indexer loads full addressbook)
            strQuery = "SELECT id, objectclass, externid FROM users WHERE id not in (1,2)";
			if (lpSession->GetSecurity()->GetUserId() != ZARAFA_UID_SYSTEM)
				strQuery += " AND company = " + stringify(ulCompanyId);
            
            er = lpDatabase->DoSelect(strQuery, &lpDBResult);
            if (er != erSuccess)
                goto exit;
                
            ulChanges = lpDatabase->GetNumRows(lpDBResult);
            lpChanges = (icsChangesArray *)soap_malloc(soap, sizeof(icsChangesArray));
            lpChanges->__ptr = (icsChange *)soap_malloc(soap, sizeof(icsChange) * ulChanges);
            lpChanges->__size = 0;

            memset(lpChanges->__ptr, 0, sizeof(icsChange) * ulChanges);

            i=0;
                
            while(1) {
                objectid_t id;
                unsigned int ulType;
                
                lpDBRow = lpDatabase->FetchRow(lpDBResult);
                lpDBLen = lpDatabase->FetchRowLengths(lpDBResult);
                
                if(lpDBRow == NULL)
                    break;
                    
                if(lpDBRow[0] == NULL || lpDBRow[1] == NULL || lpDBRow[2] == NULL) {
                    er = ZARAFA_E_DATABASE_ERROR;
                    goto exit;
                }
                
                id.id.assign(lpDBRow[2], lpDBLen[2]);
                id.objclass = (objectclass_t)atoui(lpDBRow[1]);
                
                er = TypeToMAPIType((objectclass_t)atoui(lpDBRow[1]), (ULONG *)&ulType);
                if(er != erSuccess)
                    goto exit;

                lpChanges->__ptr[i].ulChangeId = ulMaxChange;
                er = lpSession->GetUserManagement()->CreateABEntryID(soap, bAcceptABEID ? 1 : 0, atoui(lpDBRow[0]), ulType, &id, &lpChanges->__ptr[i].sSourceKey.__size, (PABEID *)&lpChanges->__ptr[i].sSourceKey.__ptr);
                if (er != erSuccess)
                    goto exit;
                lpChanges->__ptr[i].sParentSourceKey.__size = sizeof(eid);
                lpChanges->__ptr[i].sParentSourceKey.__ptr = s_alloc<unsigned char>(soap, sizeof(eid));
                memcpy(lpChanges->__ptr[i].sParentSourceKey.__ptr, &eid, sizeof(eid));
                lpChanges->__ptr[i].ulChangeType = ICS_AB_NEW;
                
                i++;
            }
            
            lpChanges->__size = i;
        }
	}

	er = lpDatabase->Commit();
	if (er != erSuccess)
		goto exit;

	*lpulMaxChangeId = ulMaxChange;
	*lppChanges = lpChanges;

exit:
	delete lpHelper;

	if(lpDBResult)
		lpDatabase->FreeResult(lpDBResult);

	if (lpDatabase && er != erSuccess)
		lpDatabase->Rollback();

	if(lpSourceKeyData) {
		delete []lpSourceKeyData;
		lpSourceKeyData = NULL;
	}
	return er;
}

ECRESULT AddABChange(BTSession *lpSession, unsigned int ulChange, SOURCEKEY sSourceKey, SOURCEKEY sParentSourceKey)
{
	ECRESULT		er = erSuccess;
	std::string		strQuery;
	ECDatabase*		lpDatabase = NULL;

	er = lpSession->GetDatabase(&lpDatabase);
	if (er != erSuccess)
		goto exit;

	// Add/Replace new change
	strQuery = "REPLACE INTO abchanges (sourcekey, parentsourcekey, change_type) VALUES(" + lpDatabase->EscapeBinary(sSourceKey, sSourceKey.size()) + "," + lpDatabase->EscapeBinary(sParentSourceKey, sParentSourceKey.size()) + "," + stringify(ulChange) + ")";
	er = lpDatabase->DoInsert(strQuery);
	if(er != erSuccess)
	    goto exit;

exit:
    return er;
}

ECRESULT GetSyncStates(struct soap *soap, ECSession *lpSession, mv_long ulaSyncId, syncStateArray *lpsaSyncState)
{
	ECRESULT		er = erSuccess;
	std::string		strQuery;
	ECDatabase*		lpDatabase = NULL;
	DB_RESULT		lpDBResult	= NULL;
	unsigned int	ulResults = 0;
	DB_ROW			lpDBRow;

	if (ulaSyncId.__size == 0) {
		memset(lpsaSyncState, 0, sizeof *lpsaSyncState);
		goto exit;
	}

	er = lpSession->GetDatabase(&lpDatabase);
	if (er != erSuccess)
		goto exit;

	strQuery = "SELECT id,change_id FROM syncs WHERE id IN (" + stringify(ulaSyncId.__ptr[0]);
	for (int i = 1; i < ulaSyncId.__size; ++i)
		strQuery += "," + stringify(ulaSyncId.__ptr[i]);
	strQuery += ")";

	er = lpDatabase->DoSelect(strQuery, &lpDBResult);
    if (er != erSuccess)
        goto exit;

	ulResults = lpDatabase->GetNumRows(lpDBResult);
    if (ulResults == 0){
		memset(lpsaSyncState, 0, sizeof *lpsaSyncState);
        goto exit;
    }

	lpsaSyncState->__size = 0;
	lpsaSyncState->__ptr = s_alloc<syncState>(soap, ulResults);

	while ((lpDBRow = lpDatabase->FetchRow(lpDBResult)) != NULL) {
		if (lpDBRow[0] == NULL || lpDBRow[1] == NULL) {
			er = ZARAFA_E_DATABASE_ERROR;
			goto exit;
		}

		lpsaSyncState->__ptr[lpsaSyncState->__size].ulSyncId = atoui(lpDBRow[0]);
		lpsaSyncState->__ptr[lpsaSyncState->__size++].ulChangeId = atoui(lpDBRow[1]);
	}
	ASSERT(lpsaSyncState->__size == ulResults);

exit:
    if(lpDBResult)
        lpDatabase->FreeResult(lpDBResult);

	return er;
}

ECRESULT AddToLastSyncedMessagesSet(ECDatabase *lpDatabase, unsigned int ulSyncId, const SOURCEKEY &sSourceKey, const SOURCEKEY &sParentSourceKey)
{
	ECRESULT	er = erSuccess;
	std::string	strQuery;
	DB_RESULT	lpDBResult	= NULL;
	DB_ROW		lpDBRow;
	
	strQuery = "SELECT MAX(change_id) FROM syncedmessages WHERE sync_id=" + stringify(ulSyncId);	
	er = lpDatabase->DoSelect(strQuery, &lpDBResult);
	if (er != erSuccess)
		goto exit;
		
	lpDBRow = lpDatabase->FetchRow(lpDBResult);
	if (lpDBRow == NULL) {
		er = ZARAFA_E_DATABASE_ERROR;
		goto exit;
	}
	
	if (lpDBRow[0] == NULL)	// none set for this sync id.
		goto exit;
	
	strQuery = "INSERT INTO syncedmessages (sync_id,change_id,sourcekey,parentsourcekey) VALUES (" +
				stringify(ulSyncId) + "," +
				lpDBRow[0] + "," + 
				lpDatabase->EscapeBinary(sSourceKey, sSourceKey.size()) + "," +
				lpDatabase->EscapeBinary(sParentSourceKey, sParentSourceKey.size()) + ")";
	
	// cleanup the previous query result
	lpDatabase->FreeResult(lpDBResult);
	lpDBResult = NULL;
	
	er = lpDatabase->DoInsert(strQuery);
	if (er != erSuccess)
		goto exit;
		
exit:
	if (lpDBResult)
		lpDatabase->FreeResult(lpDBResult);
		
	return er;
}

ECRESULT RemoveFromLastSyncedMessagesSet(ECDatabase *lpDatabase, unsigned int ulSyncId, const SOURCEKEY &sSourceKey, const SOURCEKEY &sParentSourceKey)
{
	ECRESULT	er = erSuccess;
	std::string	strQuery;
	DB_RESULT	lpDBResult	= NULL;
	DB_ROW		lpDBRow;
	
	strQuery = "SELECT MAX(change_id) FROM syncedmessages WHERE sync_id=" + stringify(ulSyncId);	
	er = lpDatabase->DoSelect(strQuery, &lpDBResult);
	if (er != erSuccess)
		goto exit;
		
	lpDBRow = lpDatabase->FetchRow(lpDBResult);
	if (lpDBRow == NULL) {
		er = ZARAFA_E_DATABASE_ERROR;
		goto exit;
	}
	
	if (lpDBRow[0] == NULL)	// none set for this sync id.
		goto exit;
	
	strQuery = "DELETE FROM syncedmessages WHERE sync_id=" + stringify(ulSyncId) +
				" AND change_id=" + lpDBRow[0] +
				" AND sourcekey=" + lpDatabase->EscapeBinary(sSourceKey, sSourceKey.size());
	
	// cleanup the previous query result
	lpDatabase->FreeResult(lpDBResult);
	lpDBResult = NULL;
	
	er = lpDatabase->DoDelete(strQuery);
	if (er != erSuccess)
		goto exit;
		
exit:
	if (lpDBResult)
		lpDatabase->FreeResult(lpDBResult);
		
	return er;
}
