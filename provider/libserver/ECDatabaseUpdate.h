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

#ifndef ECDATABASEUPDATE_H
#define ECDATABASEUPDATE_H

#include "ECLogger.h"

typedef struct _sUpdateList {
	unsigned int ulVersion;
	unsigned int ulVersionMin; // Version to start the update
	char *lpszLogComment;
	ECRESULT (*lpFunction)(ECDatabase* lpDatabase);
}sUpdateList_t;

typedef struct _sNewDatabase {
	char *lpComment;
	char *lpSQL;
}sSQLDatabase_t;

ECRESULT UpdateDatabaseCreateVersionsTable(ECDatabase *lpDatabase);
ECRESULT UpdateDatabaseCreateSearchFolders(ECDatabase *lpDatabase);
ECRESULT UpdateDatabaseFixUserNonActive(ECDatabase *lpDatabase);
ECRESULT UpdateDatabaseCreateSearchFoldersFlags(ECDatabase *lpDatabase);
ECRESULT UpdateDatabasePopulateSearchFolders(ECDatabase *lpDatabase);

ECRESULT UpdateDatabaseCreateChangesTable(ECDatabase *lpDatabase);
ECRESULT UpdateDatabaseCreateSyncsTable(ECDatabase *lpDatabase);
ECRESULT UpdateDatabaseCreateIndexedPropertiesTable(ECDatabase *lpDatabase);
ECRESULT UpdateDatabaseCreateSettingsTable(ECDatabase *lpDatabase);
ECRESULT InsertServerGUID(ECDatabase *lpDatabase);
ECRESULT UpdateDatabaseCreateServerGUID(ECDatabase *lpDatabase);
ECRESULT UpdateDatabaseCreateSourceKeys(ECDatabase *lpDatabase);

ECRESULT UpdateDatabaseConvertEntryIDs(ECDatabase *lpDatabase);
ECRESULT CreateRecursiveStoreEntryIds(ECDatabase *lpDatabase, unsigned int ulStoreHierarchyId, unsigned char* lpStoreGuid);
ECRESULT UpdateDatabaseSearchCriteria(ECDatabase *lpDatabase);
ECRESULT UpdateDatabaseAddUserObjectType(ECDatabase *lpDatabase);
ECRESULT UpdateDatabaseAddUserSignature(ECDatabase *lpDatabase);
ECRESULT UpdateDatabaseAddSourceKeySetting(ECDatabase *lpDatabase);
ECRESULT UpdateDatabaseRestrictExternId(ECDatabase *lpDatabase);

ECRESULT UpdateDatabaseAddUserCompany(ECDatabase *lpDatabase);
ECRESULT UpdateDatabaseAddObjectRelationType(ECDatabase *lpDatabase);
ECRESULT UpdateDatabaseDelUserCompany(ECDatabase *lpDatabase);
ECRESULT UpdateDatabaseAddCompanyToStore(ECDatabase *lpDatabase);

ECRESULT UpdateDatabaseAddIMAPSequenceNumber(ECDatabase *lpDatabase);
ECRESULT UpdateDatabaseKeysChanges(ECDatabase *lpDatabase);

ECRESULT UpdateDatabaseMoveFoldersInPublicFolder(ECDatabase *lpDatabase);

ECRESULT UpdateDatabaseAddExternIdToObject(ECDatabase *lpDatabase);
ECRESULT UpdateDatabaseCreateReferences(ECDatabase *lpDatabase);
ECRESULT UpdateDatabaseLockDistributed(ECDatabase *lpDatabase);
ECRESULT UpdateDatabaseCreateABChangesTable(ECDatabase *lpDatabase);
ECRESULT UpdateDatabaseSetSingleinstanceTag(ECDatabase *lpDatabase);

ECRESULT UpdateDatabaseCreateSyncedMessagesTable(ECDatabase *lpDatabase);

ECRESULT UpdateDatabaseForceAbResync(ECDatabase *lpDatabase);

ECRESULT UpdateDatabaseRenameObjectTypeToObjectClass(ECDatabase *lpDatabase);
ECRESULT UpdateDatabaseConvertObjectTypeToObjectClass(ECDatabase *lpDatabase);
ECRESULT UpdateDatabaseAddMVPropertyTable(ECDatabase *lpDatabase);
ECRESULT UpdateDatabaseCompanyNameToCompanyId(ECDatabase *lpDatabase);
ECRESULT UpdateDatabaseOutgoingQueuePrimarykey(ECDatabase *lpDatabase);
ECRESULT UpdateDatabaseACLPrimarykey(ECDatabase *lpDatabase);

ECRESULT UpdateDatabaseBlobExternId(ECDatabase *lpDatabase);

ECRESULT UpdateDatabaseKeysChanges2(ECDatabase *lpDatabase);
ECRESULT UpdateDatabaseMVPropertiesPrimarykey(ECDatabase *lpDatabase);
ECRESULT UpdateDatabaseFixDBPluginGroups(ECDatabase *lpDatabase);
ECRESULT UpdateDatabaseFixDBPluginSendAs(ECDatabase *lpDatabase);

ECRESULT UpdateDatabaseMoveSubscribedList(ECDatabase *lpDatabase);
ECRESULT UpdateDatabaseSyncTimeIndex(ECDatabase *lpDatabase);
ECRESULT UpdateDatabaseAddStateKey(ECDatabase *lpDatabase);

ECRESULT UpdateDatabaseConvertToUnicode(ECDatabase *lpDatabase);

ECRESULT UpdateDatabaseConvertStoreUsername(ECDatabase *lpDatabase);
ECRESULT UpdateDatabaseConvertRules(ECDatabase *lpDatabase);
ECRESULT UpdateDatabaseConvertSearchFolders(ECDatabase *lpDatabase);

ECRESULT UpdateDatabaseConvertProperties(ECDatabase *lpDatabase);
ECRESULT UpdateDatabaseCreateCounters(ECDatabase *lpDatabase);
ECRESULT UpdateDatabaseCreateCommonProps(ECDatabase *lpDatabase);
ECRESULT UpdateDatabaseCheckAttachments(ECDatabase *lpDatabase);
ECRESULT UpdateDatabaseCreateTProperties(ECDatabase *lpDatabase);
ECRESULT UpdateDatabaseConvertHierarchy(ECDatabase *lpDatabase);
ECRESULT UpdateDatabaseCreateDeferred(ECDatabase *lpDatabase);
ECRESULT UpdateDatabaseConvertChanges(ECDatabase *lpDatabase);
ECRESULT UpdateDatabaseConvertNames(ECDatabase *lpDatabase);

ECRESULT UpdateDatabaseReceiveFolderToUnicode(ECDatabase *lpDatabase);
ECRESULT UpdateDatabaseClientUpdateStatus(ECDatabase *lpDatabase);
ECRESULT UpdateDatabaseConvertStores(ECDatabase *lpDatabase);
ECRESULT UpdateDatabaseUpdateStores(ECDatabase *lpDatabase);

#endif // #ifndef ECDATABASEUPDATE_H
