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

// STL defines
#include <set>
#include <string>
#include <list>
#include <map>

// mapi defines
#include <mapidefs.h>
#include <mapitags.h>
#include "mapiext.h"
#include <EMSAbTag.h>
#include <edkmdb.h>
#include "ECMAPI.h"

#include "soapH.h"

#include "ECSessionManager.h"
#include "ECSecurity.h"
#include "ZarafaICS.h"
#include "ECICS.h"
#include "StorageUtil.h"
#include "ECAttachmentStorage.h"
#include "ECStatsCollector.h"
#include "ECStringCompat.h"
#include "ECTPropsPurge.h"

#include "ZarafaCmdUtil.h"

#define FIELD_NR_NAMEID		(FIELD_NR_MAX + 1)
#define FIELD_NR_NAMESTR	(FIELD_NR_MAX + 2)
#define FIELD_NR_NAMEGUID	(FIELD_NR_MAX + 3)

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

extern ECSessionManager*    g_lpSessionManager; // FIXME: remove this global and change the depended source code!
extern ECStatsCollector* 	g_lpStatsCollector;

ECRESULT GetSourceKey(unsigned int ulObjId, SOURCEKEY *lpSourceKey)
{
	ECRESULT er = erSuccess;
	unsigned char *lpData = NULL;
	unsigned int cbData = 0;

	er = g_lpSessionManager->GetCacheManager()->GetPropFromObject(PROP_ID(PR_SOURCE_KEY), ulObjId, NULL, &cbData, &lpData);
	if(er != erSuccess)
		goto exit;

	*lpSourceKey = SOURCEKEY(cbData, (char *)lpData);

exit:
	if(lpData)
		delete [] lpData;
	return er;
}

/*
 * This is a generic delete function that is called from
 *
 * ns__deleteObjects
 * ns__emptyFolder
 * ns__deleteFolder
 * purgeSoftDelete
 *
 * Functions which using sub set of the delete system are:
 * ns__saveObject
 * importMessageFromStream
 *
 * It does a recursive delete of objects in the hierarchytable, according to the flags given
 * which is any combination of
 *
 * EC_DELETE_FOLDERS		- Delete subfolders
 * EC_DELETE_MESSAGES		- Delete messages
 * EC_DELETE_RECIPIENTS		- Delete recipients of messages
 * EC_DELETE_ATTACHMENTS	- Delete attachments of messages
 * EC_DELETE_CONTAINER		- Delete the container specified in the first place (otherwise only subobjects)
 *
 * This is done by first recusively retrieving the object ID's, then checking the types of objects in that
 * list. If there is any subobject that has *not* been specified for deletion, the function fails. Else,
 * all properties in the subobjects and the subobjects themselves are deleted. If EC_DELETE_CONTAINER
 * is specified, then the objects passed in lpEntryList are also deleted (together with their properties).
 *
 */

/**
 * Validate permissions and match object type
 *
 * @param[in] lpSession Reference to a session object; cannot be NULL.
 * @param[in] bCheckPermission Check if the object folder or message has delete permissions.
 * @param[in] ulFlags Bitmask of flags that controls how the objects will deleted. 
 * @param[in] sItem Reference to a DELETEITEM structure that contains object information which identifying the folder, message, reciptient and attachment.
 *
 * @return Zarafa error code
 */
ECRESULT ValidateDeleteObject(ECSession *lpSession, bool bCheckPermission, unsigned int ulFlags, DELETEITEM &sItem)
{
	ECRESULT er = erSuccess;

	if (lpSession == NULL) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	// Check permission for each folder and messages
	if (bCheckPermission && ((sItem.ulObjType == MAPI_MESSAGE && sItem.ulParentType == MAPI_FOLDER) || sItem.ulObjType == MAPI_FOLDER)) {
		er = lpSession->GetSecurity()->CheckPermission(sItem.ulId, ecSecurityDelete);
		if(er != erSuccess)
			goto exit;
	}

	if (sItem.fRoot == true)
		goto exit; // Not for a root

	switch(sItem.ulObjType) {
	case MAPI_MESSAGE:
		if(! (ulFlags & EC_DELETE_MESSAGES) ) {
			er = ZARAFA_E_HAS_MESSAGES;
			goto exit;
		}
		break;
	case MAPI_FOLDER:
		if(! (ulFlags & EC_DELETE_FOLDERS) ) {
			er = ZARAFA_E_HAS_FOLDERS;
			goto exit;
		}
		break;
	case MAPI_MAILUSER:
	case MAPI_DISTLIST:
		if(! (ulFlags & EC_DELETE_RECIPIENTS) ) {
			er = ZARAFA_E_HAS_RECIPIENTS;
			goto exit;
		}
		break;
	case MAPI_ATTACH:
		if(! (ulFlags & EC_DELETE_ATTACHMENTS) ) {
			er = ZARAFA_E_HAS_ATTACHMENTS;
			goto exit;
		}
		break;
	case MAPI_STORE: // only admins can delete a store, rights checked in ns__removeStore
		if(! (ulFlags & EC_DELETE_STORE) ) {
			er = ZARAFA_E_NOT_FOUND;
			goto exit;
		}
		break;
	default:
		// Unknown object type? We'll delete it anyway so we don't get frustrating non-deletable items
		ASSERT(FALSE); // Only frustrating developers!
		break;
	}

exit:
	return er;
}

/**
 * Expand a list of objects, validate permissions and object types
 * This function returns a list of all items that need to be
 * deleted. This may or may not include the given list in
 * lpsObjectList, because of EC_DELETE_CONTAINER.
 *
 * If the ulFLags includes EC_DELETE_NOT_ASSOCIATED_MSG only the associated messages for the container folder is 
 * not deleted. If ulFlags EC_DELETE_CONTAINER included, the EC_DELETE_NOT_ASSOCIATED_MSG flag will be ignored.
 *
 * @param[in] lpSession Reference to a session object; cannot be NULL.
 * @param[in] lpDatabase Reference to a database object; cannot be NULL.
 * @param[in] lpsObjectList Reference to a list of objects that contains itemid and must be expanded.
 * @param[in] ulFlags Bitmask of flags that controls how the objects will deleted.
 * @param[in] bCheckPermission Check the objects delete permissions.
 * @param[out] lplstDeleteItems Recursive list with objects
 *
 * @return Zarafa error code
 */
ECRESULT ExpandDeletedItems(ECSession *lpSession, ECDatabase *lpDatabase, ECListInt *lpsObjectList, unsigned int ulFlags, bool bCheckPermission, ECListDeleteItems *lplstDeleteItems)
{
	ECRESULT er = erSuccess;
	
	ECListIntIterator	iListObjectId;

	DB_RESULT lpDBResult = NULL;
	DB_ROW lpDBRow = NULL;
	std::string strQuery;
	std::set<unsigned int> setIDs;
	ECListDeleteItems lstDeleteItems;
	ECListDeleteItems::iterator iterDeleteItems;
	ECListDeleteItems lstContainerItems;
	DELETEITEM sItem;
	ECSessionManager *lpSessionManager = NULL;
	ECCacheManager *lpCacheManager = NULL;
	unsigned int ulParent = 0;
	
	if (lpSession == NULL || lpDatabase == NULL || lpsObjectList == NULL || lplstDeleteItems == NULL) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	lpSessionManager = lpSession->GetSessionManager();
	lpCacheManager = lpSessionManager->GetCacheManager();

	// First, put all the root objects in the list
	for(iListObjectId = lpsObjectList->begin(); iListObjectId != lpsObjectList->end(); iListObjectId++) {

		sItem.fRoot = true;
		
		//Free database results
		if(lpDBResult) { lpDatabase->FreeResult(lpDBResult); lpDBResult = NULL; }

		// Lock the root records's parent counter to maintain locking order (counters/content/storesize/committimemax)
		er  = lpCacheManager->GetObject(*iListObjectId, &ulParent, NULL, NULL, NULL);
		if(er != erSuccess)
		    goto exit;
		    
        er = lpDatabase->DoSelect("SELECT properties.val_ulong FROM properties WHERE hierarchyid = " + stringify(ulParent) + " FOR UPDATE", NULL);
        if(er != erSuccess)
            goto exit;

        // Lock the root records to make sure that we don't interfere with modifies or deletes on the same record
        er = lpDatabase->DoSelect("SELECT hierarchy.flags FROM hierarchy WHERE id = " + stringify(*iListObjectId) + " FOR UPDATE", NULL);
        if(er != erSuccess)
            goto exit;
                
		strQuery = "SELECT h.id, h.parent, h.type, h.flags, p.type, properties.val_ulong, (SELECT hierarchy_id FROM outgoingqueue WHERE outgoingqueue.hierarchy_id = h.id LIMIT 1) FROM hierarchy as h LEFT JOIN properties ON properties.hierarchyid=h.id AND properties.tag = " + stringify(PROP_ID(PR_MESSAGE_FLAGS)) + " AND properties.type = " + stringify(PROP_TYPE(PR_MESSAGE_FLAGS)) + " LEFT JOIN hierarchy AS p ON h.parent=p.id WHERE ";
		if((ulFlags & EC_DELETE_CONTAINER) == 0)
			strQuery += "h.parent=" + stringify(*iListObjectId);
		else
			strQuery += "h.id=" + stringify(*iListObjectId);

		if((ulFlags & EC_DELETE_HARD_DELETE) != EC_DELETE_HARD_DELETE)
			strQuery += " AND (h.flags&"+stringify(MSGFLAG_DELETED)+") !="+stringify(MSGFLAG_DELETED);

		if ((ulFlags & EC_DELETE_CONTAINER) == 0 && (ulFlags & EC_DELETE_NOT_ASSOCIATED_MSG) == EC_DELETE_NOT_ASSOCIATED_MSG)
			strQuery += " AND (h.flags&"+stringify(MSGFLAG_ASSOCIATED)+") !="+stringify(MSGFLAG_ASSOCIATED);

		er = lpDatabase->DoSelect(strQuery, &lpDBResult);
		if(er != erSuccess)
			goto exit;

		while ( (lpDBRow = lpDatabase->FetchRow(lpDBResult)) )
		{
			// No type or flags exist
			if(lpDBRow[2] == NULL || lpDBRow[3] == NULL) {
				//er = ZARAFA_E_DATABASE_ERROR;
				//goto exit;
				continue;
			}

			// When you delete a store the parent id is NULL, object type must be MAPI_STORE
			if(lpDBRow[1] == NULL && atoi(lpDBRow[2]) != MAPI_STORE) {
				//er = ZARAFA_E_DATABASE_ERROR;
				//goto exit;
				continue;
			}

			// Loop protection, don't insert duplicates.
			if (setIDs.insert(atoui(lpDBRow[0])).second == false)
				continue;
		
			sItem.ulId = atoui(lpDBRow[0]);
			sItem.ulParent = (lpDBRow[1])?atoui(lpDBRow[1]) : 0;
			sItem.ulObjType = atoi(lpDBRow[2]);
			sItem.ulFlags = atoui(lpDBRow[3]);
			sItem.ulObjSize = 0;
			sItem.ulStoreId = 0;
			sItem.ulParentType = (lpDBRow[4])?atoui(lpDBRow[4]) : 0; 
			sItem.sEntryId.__size = 0;
			sItem.sEntryId.__ptr = NULL;
			sItem.ulMessageFlags = lpDBRow[5] ? atoui(lpDBRow[5]) : 0;
			sItem.fInOGQueue = lpDBRow[6] ? true : false;

			// Validate deleted object, if not valid, break directly
			er = ValidateDeleteObject(lpSession, bCheckPermission, ulFlags, sItem);
			if (er != erSuccess)
				goto exit;

			// Get extended data
			if(sItem.ulObjType == MAPI_STORE || sItem.ulObjType == MAPI_FOLDER || sItem.ulObjType == MAPI_MESSAGE) {
			
				lpCacheManager->GetStore(sItem.ulId, &sItem.ulStoreId , NULL); //CHECKme:"oude gaf geen errors
			
				if (!(sItem.ulFlags&MSGFLAG_DELETED) ) {
					GetObjectSize(lpDatabase, sItem.ulId, &sItem.ulObjSize);
				}

				lpCacheManager->GetEntryIdFromObject(sItem.ulId, NULL, 0, &sItem.sEntryId);//CHECKme:"oude gaf geen errors

				GetSourceKey(sItem.ulId, &sItem.sSourceKey);
				GetSourceKey(sItem.ulParent, &sItem.sParentSourceKey);
			}

			lstDeleteItems.push_back(sItem);
		}
	}

	// Now, run through the list, adding children to the bottom of the list. This means
	// we're actually running width-first, and don't have to do anything recursive.
	for(iterDeleteItems=lstDeleteItems.begin(); iterDeleteItems != lstDeleteItems.end(); iterDeleteItems++) 
	{

		// Free database results
		if(lpDBResult) { lpDatabase->FreeResult(lpDBResult); lpDBResult = NULL; }

		strQuery = "SELECT id, type, flags, (SELECT hierarchy_id FROM outgoingqueue WHERE outgoingqueue.hierarchy_id = hierarchy.id LIMIT 1) FROM hierarchy WHERE parent=" + stringify(iterDeleteItems->ulId);
		if((ulFlags & EC_DELETE_HARD_DELETE) != EC_DELETE_HARD_DELETE)
			strQuery += " AND (flags&"+stringify(MSGFLAG_DELETED)+") !="+stringify(MSGFLAG_DELETED);

		er = lpDatabase->DoSelect(strQuery, &lpDBResult);
		if(er != erSuccess)
			goto exit;

		while((lpDBRow = lpDatabase->FetchRow(lpDBResult)) != NULL )
		{

			// No id, type or flags exist
			if(lpDBRow[0] == NULL || lpDBRow[1] == NULL || lpDBRow[2] == NULL)
				continue;

			// Loop protection, don't insert duplicates.
			if (setIDs.insert(atoui(lpDBRow[0])).second == false)
				continue;

			// Add this object as a node to the end of the list
			sItem.fRoot = false;
			sItem.ulObjSize = 0;
			sItem.ulStoreId = 0;
			sItem.sEntryId.__size = 0;
			sItem.sEntryId.__ptr = NULL;

			sItem.ulId = atoui(lpDBRow[0]);
			sItem.ulParent = iterDeleteItems->ulId;
			sItem.ulParentType = iterDeleteItems->ulObjType;
			sItem.ulObjType = atoi(lpDBRow[1]);
			sItem.ulFlags = atoui(lpDBRow[2]) | (iterDeleteItems->ulFlags&MSGFLAG_DELETED); // Add the parent delete flag, because only the top-level object is marked for deletion
			sItem.fInOGQueue = lpDBRow[3] ? true : false;

			// Validate deleted object, if no valid, break directly
			er = ValidateDeleteObject(lpSession, bCheckPermission, ulFlags, sItem);
			if (er != erSuccess)
				goto exit;

			if(sItem.ulObjType == MAPI_STORE || sItem.ulObjType == MAPI_FOLDER || (sItem.ulObjType == MAPI_MESSAGE && sItem.ulParentType == MAPI_FOLDER) ) {
			
				lpCacheManager->GetStore(sItem.ulId, &sItem.ulStoreId , NULL); //CHECKme:"oude gaf geen errors

				if (!(sItem.ulFlags&MSGFLAG_DELETED) ) {
					GetObjectSize(lpDatabase, sItem.ulId, &sItem.ulObjSize);
				}

				lpCacheManager->GetEntryIdFromObject(sItem.ulId, NULL, 0, &sItem.sEntryId);//CHECKme:"oude gaf geen errors

				GetSourceKey(sItem.ulId, &sItem.sSourceKey);
				GetSourceKey(sItem.ulParent, &sItem.sParentSourceKey);
			}
			
			lstDeleteItems.push_back(sItem);
		}
	}
	
	// Move list
	std::swap(lstDeleteItems, *lplstDeleteItems);

exit:
	FreeDeletedItems(&lstDeleteItems);

	if(lpDBResult)
		lpDatabase->FreeResult(lpDBResult);

	return er;
}

/*
 * Add changes into the ICS system.
 * 
 * This adds a change for each removed folder and message. The change could be a soft- or hard delete.
 * It is possible to gives a list with different deleted objects, all not supported object types will be skipped.
 *
 * @param[in] lpSession Reference to a session object; cannot be NULL.
 * @param[in] ulFlags Bitmask of flags that controls how the objects will deleted.
 * @param[in] lstDeleted List with deleted objects
 * @param[in] ulSyncId ???
 *
 */
ECRESULT DeleteObjectUpdateICS(ECSession *lpSession, unsigned int ulFlags, ECListDeleteItems &lstDeleted, unsigned int ulSyncId)
{
	ECRESULT er = erSuccess;
	ECListDeleteItems::iterator iterDeleteItems;

	for(iterDeleteItems=lstDeleted.begin(); iterDeleteItems != lstDeleted.end(); iterDeleteItems++)
	{
		// ICS update
		if (iterDeleteItems->ulObjType == MAPI_MESSAGE && iterDeleteItems->ulParentType == MAPI_FOLDER)
			AddChange(lpSession, ulSyncId, iterDeleteItems->sSourceKey, iterDeleteItems->sParentSourceKey, ulFlags & EC_DELETE_HARD_DELETE ? ICS_MESSAGE_HARD_DELETE : ICS_MESSAGE_SOFT_DELETE);
		else if (iterDeleteItems->ulObjType == MAPI_FOLDER && !(iterDeleteItems->ulFlags & FOLDER_SEARCH)) {
			AddChange(lpSession, ulSyncId, iterDeleteItems->sSourceKey, iterDeleteItems->sParentSourceKey, ulFlags & EC_DELETE_HARD_DELETE ? ICS_FOLDER_HARD_DELETE : ICS_FOLDER_SOFT_DELETE);
		}
	}

	return er;
}

/*
 * Calculate and update the store size for deleted items
 *
 * The DeleteObjectStoreSize methode calculate and update the store size. Only top-level messages will
 * be calculate, all other objects are not supported and will be skipped. If a message has the 
 * MSGFLAG_DELETED flag, the size will ignored because it is already subtract from the store size.
 * The deleted object list may include more than one store.
 *
 * @param[in] lpSession Reference to a session object; cannot be NULL.
 * @param[in] lpDatabase Reference to a database object; cannot be NULL.
 * @param[in] ulFlags Bitmask of flags that controls how the objects will deleted.
 * @param[in] lstDeleted List with deleted objects
 */
ECRESULT DeleteObjectStoreSize(ECSession *lpSession, ECDatabase *lpDatabase, unsigned int ulFlags, ECListDeleteItems &lstDeleted)
{
	ECRESULT er = erSuccess;
	std::map<unsigned int, long long> mapStoreSize;

	ECListDeleteItems::iterator iterDeleteItems;
	std::map<unsigned int, long long>::iterator iterStoreSizeItems;

//TODO: check or foldersize also is used

	for(iterDeleteItems=lstDeleted.begin(); iterDeleteItems != lstDeleted.end(); iterDeleteItems++) {
		// Get size of all the messages
		if( iterDeleteItems->ulObjType == MAPI_MESSAGE && 
			iterDeleteItems->ulParentType == MAPI_FOLDER && 
			(iterDeleteItems->ulFlags&MSGFLAG_DELETED) != MSGFLAG_DELETED)
		{
			ASSERT(iterDeleteItems->ulStoreId != 0);

			if (mapStoreSize.find(iterDeleteItems->ulStoreId) != mapStoreSize.end() )
				mapStoreSize[iterDeleteItems->ulStoreId] += iterDeleteItems->ulObjSize;
			else
				mapStoreSize[iterDeleteItems->ulStoreId] = iterDeleteItems->ulObjSize;
		}
	}

	// Update store size for each store
	for(iterStoreSizeItems=mapStoreSize.begin(); iterStoreSizeItems != mapStoreSize.end(); iterStoreSizeItems++) {
		if ( iterStoreSizeItems->second > 0)	
			UpdateObjectSize(lpDatabase, iterStoreSizeItems->first, MAPI_STORE, UPDATE_SUB, iterStoreSizeItems->second);
	}

	return er;
}

/*
 * Soft delete objects, mark the root objects as deleted
 *
 * Mark the root objects as deleted, add deleted on date on the root objects.
 * Since this is a fairly simple operation, we are doing soft deletes in a single transaction. Once the SQL has gone OK, 
 * we know that all items were successfully mark as deleted and we can therefore add all soft-deleted items into 
 * the 'lstDeleted' list at once
 *
 * @param[in] lpSession Reference to a session object; cannot be NULL.
 * @param[in] lpDatabase Reference to a database object; cannot be NULL.
 * @param[in] ulFlags Bitmask of flags that controls how the objects will deleted.
 * @param[in] lstDeleteItems List with objecta which must be deleted.
 * @param[out] lstDeleted List with deleted objects.
 *
 */
ECRESULT DeleteObjectSoft(ECSession *lpSession, ECDatabase *lpDatabase, unsigned int ulFlags, ECListDeleteItems &lstDeleteItems, ECListDeleteItems &lstDeleted)
{
	ECRESULT er = erSuccess;
	ECListDeleteItems::iterator iterDeleteItems;

	FILETIME ft;
	std::string strInclauseOQQ;
	std::string strInclause;
	std::string strQuery;
	
	PARENTINFO pi;
	
	std::map<unsigned int, PARENTINFO> mapFolderCounts;
	std::map<unsigned int, PARENTINFO>::iterator iterFolderCounts;

	// Build where condition
	for(iterDeleteItems=lstDeleteItems.begin(); iterDeleteItems != lstDeleteItems.end(); iterDeleteItems++) {

		if( (iterDeleteItems->ulObjType == MAPI_MESSAGE && iterDeleteItems->ulParentType == MAPI_FOLDER) || 
			  iterDeleteItems->ulObjType == MAPI_FOLDER  || iterDeleteItems->ulObjType == MAPI_STORE) 
		{
			if (iterDeleteItems->fInOGQueue) {
				if(!strInclauseOQQ.empty())
					strInclauseOQQ += ",";

				strInclauseOQQ += stringify(iterDeleteItems->ulId);
			}
			
			if (iterDeleteItems->fRoot == true)
			{
				if(!strInclause.empty())
					strInclause += ",";

				strInclause += stringify(iterDeleteItems->ulId);
				
                // Track counter changes
                if(iterDeleteItems->ulParentType == MAPI_FOLDER) {
                	// Ignore already-softdeleted items
                	if((iterDeleteItems->ulFlags & MSGFLAG_DELETED) == 0) {
						if(iterDeleteItems->ulObjType == MAPI_MESSAGE) {
							if(iterDeleteItems->ulFlags & MAPI_ASSOCIATED) {
								mapFolderCounts[iterDeleteItems->ulParent].lAssoc--;
								mapFolderCounts[iterDeleteItems->ulParent].lDeletedAssoc++;
							} else {
									mapFolderCounts[iterDeleteItems->ulParent].lItems--;
								mapFolderCounts[iterDeleteItems->ulParent].lDeleted++;
								if((iterDeleteItems->ulMessageFlags & MSGFLAG_READ) == 0)
									mapFolderCounts[iterDeleteItems->ulParent].lUnread--;
							}
						}
						if(iterDeleteItems->ulObjType == MAPI_FOLDER) {
							mapFolderCounts[iterDeleteItems->ulParent].lFolders--;
							mapFolderCounts[iterDeleteItems->ulParent].lDeletedFolders++;
						}
					}
				}
			}
		}
	}

	// Mark all items as deleted, if a item in the outgoingqueue and remove the submit flag
	if (!strInclauseOQQ.empty())
	{
		// Remove any entries in the outgoing queue for deleted items
		strQuery = "DELETE FROM outgoingqueue WHERE hierarchy_id IN ( " + strInclauseOQQ + ")";
		er = lpDatabase->DoDelete(strQuery);
		if(er!= erSuccess)
			goto exit;
		
		// Remove the submit flag
		strQuery = "UPDATE properties SET val_ulong=val_ulong&~" + stringify(MSGFLAG_SUBMIT)+" WHERE hierarchyid IN(" + strInclauseOQQ + ") AND tag = " + stringify(PROP_ID(PR_MESSAGE_FLAGS)) + " and type = " + stringify(PROP_TYPE(PR_MESSAGE_FLAGS));
		er = lpDatabase->DoUpdate(strQuery);
		if(er!= erSuccess)
			goto exit;
	}

	if(!strInclause.empty())
	{
		// Mark item as deleted
		strQuery = "UPDATE hierarchy SET flags=flags|"+stringify(MSGFLAG_DELETED)+" WHERE id IN(" + strInclause + ")";
		er = lpDatabase->DoUpdate(strQuery);
		if(er!= erSuccess)
			goto exit;

		// Remove the MSGSTATUS_DELMARKED flag (IMAP gateway set)
		strQuery = "UPDATE properties SET val_ulong=val_ulong&~" + stringify(MSGSTATUS_DELMARKED) +
			" WHERE hierarchyid IN(" + strInclause + ") AND tag = " + stringify(PROP_ID(PR_MSG_STATUS)) + " and type = " + stringify(PROP_TYPE(PR_MSG_STATUS));
		er = lpDatabase->DoUpdate(strQuery);
		if(er!= erSuccess)
			goto exit;
	}

	er = ApplyFolderCounts(lpDatabase, mapFolderCounts);
	if(er != erSuccess)
	    goto exit;
	

	// Add properties: PR_DELETED_ON
	GetSystemTimeAsFileTime(&ft);

	for(iterDeleteItems=lstDeleteItems.begin(); iterDeleteItems != lstDeleteItems.end(); iterDeleteItems++)
	{
		if( iterDeleteItems->fRoot == true && 
			( (iterDeleteItems->ulObjType == MAPI_MESSAGE && iterDeleteItems->ulParentType == MAPI_FOLDER) || 
			iterDeleteItems->ulObjType == MAPI_FOLDER  || iterDeleteItems->ulObjType == MAPI_STORE) )
		{
			strQuery = "REPLACE INTO properties(hierarchyid, tag, type, val_lo, val_hi) VALUES("+stringify(iterDeleteItems->ulId)+","+stringify(PROP_ID(PR_DELETED_ON))+","+stringify(PROP_TYPE(PR_DELETED_ON))+","+stringify(ft.dwLowDateTime)+","+stringify(ft.dwHighDateTime)+")";
			er = lpDatabase->DoUpdate(strQuery);
			if(er!= erSuccess)
				goto exit;

			er = ECTPropsPurge::AddDeferredUpdateNoPurge(lpDatabase, iterDeleteItems->ulParent, 0, iterDeleteItems->ulId);
			if (er != erSuccess)
				goto exit;
		}
	}

	lstDeleted = lstDeleteItems;

exit:

	return er;
}

/**
 * Hard delete objects, remove the data from storage
 *
 * This means we should be really deleting the actual data from the database and storage. This will be done in 
 * bachtches of 32 items each because deleteing records is generally fairly slow. Also, very large delete batches 
 * can taking up to more than an hour to process. We don't want to have a transaction lasting an hour because it 
 * would cause lots of locking problems. Also, each item successfully deleted and committed to the database will 
 * added into a list. So, If something fails we notify the items in the 'deleted items list' only.
 * 
 * @param[in] lpSession Reference to a session object; cannot be NULL.
 * @param[in] lpDatabase Reference to a database object; cannot be NULL.
 * @param[in] lpAttachmentStorage Reference to an Attachment object. could not NULL if bNoTransaction is true.
 * @param[in] ulFlags Bitmask of flags that controls how the objects will deleted.
 * @param[in] lstDeleteItems List with objects which must be deleted.
 * @param[in] bNoTransaction Disable the database transactions.
 * @param[in] lstDeleted List with deleted objects.
 *
 * @return Zarafa error code
 */
ECRESULT DeleteObjectHard(ECSession *lpSession, ECDatabase *lpDatabase, ECAttachmentStorage *lpAttachmentStorage, unsigned int ulFlags, ECListDeleteItems &lstDeleteItems, bool bNoTransaction, ECListDeleteItems &lstDeleted)
{
	ECRESULT er = erSuccess;
	ECAttachmentStorage *lpInternalAttachmentStorage = NULL;
	std::list<ULONG> lstDeleteAttachments;
	std::string strInclause;
	std::string strOGQInclause;
	std::string strQuery;
	ECListDeleteItems lstToBeDeleted;
	ECListDeleteItems::reverse_iterator iterDeleteItems;

	PARENTINFO pi;
	
	std::map<unsigned int, PARENTINFO> mapFolderCounts;
	std::map<unsigned int, PARENTINFO>::iterator iterFolderCounts;

	int i;

	if(!(ulFlags & EC_DELETE_HARD_DELETE)) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	if (bNoTransaction && lpAttachmentStorage == NULL) {
		ASSERT(FALSE);
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	if (!lpAttachmentStorage) {
		er = CreateAttachmentStorage(lpDatabase, &lpInternalAttachmentStorage);
		if (er != erSuccess)
			goto exit;

		lpAttachmentStorage = lpInternalAttachmentStorage;
	}

	for (iterDeleteItems = lstDeleteItems.rbegin(); iterDeleteItems != lstDeleteItems.rend(); )
	{
		strInclause.clear();
		strOGQInclause.clear();
		lstDeleteAttachments.clear();
		lstToBeDeleted.clear();
		i = 0;

		// Delete max 32 items per query
		while (i<32 && iterDeleteItems != lstDeleteItems.rend())
		{
			if(!strInclause.empty())
				strInclause += ",";

			strInclause += stringify(iterDeleteItems->ulId);

			if(iterDeleteItems->fInOGQueue) {
				if(!strOGQInclause.empty())
					strOGQInclause += ",";
					
				strOGQInclause += stringify(iterDeleteItems->ulId);
			}

			// make new list for attachment deletes. messages can have imap "attachment".
			if (iterDeleteItems->ulObjType == MAPI_ATTACH || (iterDeleteItems->ulObjType == MAPI_MESSAGE && iterDeleteItems->ulParentType == MAPI_FOLDER))
				lstDeleteAttachments.push_back(iterDeleteItems->ulId);

			lstToBeDeleted.push_front(*iterDeleteItems);

			if(!(ulFlags&EC_DELETE_STORE) && iterDeleteItems->ulParentType == MAPI_FOLDER && iterDeleteItems->fRoot) {
				// Track counter changes
				memset(&pi, 0, sizeof(pi));
				pi.ulStoreId = iterDeleteItems->ulStoreId;
				mapFolderCounts.insert(std::make_pair(iterDeleteItems->ulParent, pi));

				if(iterDeleteItems->ulObjType == MAPI_MESSAGE) {
					if(iterDeleteItems->ulFlags == MAPI_ASSOCIATED) {
						// Delete associated
						mapFolderCounts[iterDeleteItems->ulParent].lAssoc--;
					} else if(iterDeleteItems->ulFlags == 0) {
						// Deleting directly from normal item, count normal and unread items
						mapFolderCounts[iterDeleteItems->ulParent].lItems--;
						if((iterDeleteItems->ulMessageFlags & MSGFLAG_READ) == 0)
							mapFolderCounts[iterDeleteItems->ulParent].lUnread--;
					} else if(iterDeleteItems->ulFlags == (MAPI_ASSOCIATED | MSGFLAG_DELETED)) {
						// Deleting softdeleted associated item
						mapFolderCounts[iterDeleteItems->ulParent].lDeletedAssoc--;
					} else {
						// Deleting normal softdeleted item
						mapFolderCounts[iterDeleteItems->ulParent].lDeleted--;
					}
				}
				if(iterDeleteItems->ulObjType == MAPI_FOLDER) {
					if((iterDeleteItems->ulFlags & MSGFLAG_DELETED) == 0) {
						mapFolderCounts[iterDeleteItems->ulParent].lFolders--;
					} else {
						mapFolderCounts[iterDeleteItems->ulParent].lDeletedFolders--;
					}
				}
			}
			i++;
			iterDeleteItems++;
		}

		// Start transaction
		if (!bNoTransaction) {
			er = lpAttachmentStorage->Begin();
			if (er != erSuccess)
				goto exit;

			er = lpDatabase->Begin();
			if (er != erSuccess)
				goto exit;
		}

		if(!strInclause.empty()) {
			// First, Remove any entries in the outgoing queue for deleted items
			if(!strOGQInclause.empty()) {
				strQuery = "DELETE FROM outgoingqueue WHERE hierarchy_id IN ( " + strOGQInclause + ")";
				er = lpDatabase->DoDelete(strQuery);
				if(er!= erSuccess)
					goto exit;
			}
						
			// Then, the hierarchy entries of all the objects
			strQuery = "DELETE FROM hierarchy WHERE id IN (" + strInclause + ")";
			er = lpDatabase->DoDelete(strQuery);
			if(er!= erSuccess)
				goto exit;

			// Then, the table properties for the objects we just deleted
			strQuery = "DELETE FROM tproperties WHERE hierarchyid IN (" + strInclause + ")";
			er = lpDatabase->DoDelete(strQuery);
			if(er!= erSuccess)
				goto exit;

			// Then, the properties for the objects we just deleted
			strQuery = "DELETE FROM properties WHERE hierarchyid IN (" + strInclause + ")";
			er = lpDatabase->DoDelete(strQuery);
			if(er!= erSuccess)
				goto exit;

			// Then, the MVproperties for the objects we just deleted
			strQuery = "DELETE FROM mvproperties WHERE hierarchyid IN (" + strInclause + ")";
			er = lpDatabase->DoDelete(strQuery);
			if(er!= erSuccess)
				goto exit;

			// Then, the acls for the objects we just deleted (if exist)
			strQuery = "DELETE FROM acl WHERE hierarchy_id IN (" + strInclause + ")";
			er = lpDatabase->DoDelete(strQuery);
			if(er != erSuccess)
				goto exit;

			// remove indexedproperties
			strQuery = "DELETE FROM indexedproperties WHERE hierarchyid IN (" + strInclause + ")";
			er = lpDatabase->DoDelete(strQuery);
			if(er != erSuccess)
				goto exit;

			// remove deferred table updates				
			strQuery = "DELETE FROM deferredupdate WHERE hierarchyid IN (" + strInclause + ")";
			er = lpDatabase->DoDelete(strQuery);
			if(er != erSuccess)
				goto exit;
				
		}
		// list may contain non-attachment object id's!
		if (!lstDeleteAttachments.empty()) {
			er = lpAttachmentStorage->DeleteAttachments(lstDeleteAttachments);
			if (er != erSuccess)
				goto exit;
		}

		er = ApplyFolderCounts(lpDatabase, mapFolderCounts);
		if(er != erSuccess)
		    goto exit;

		// Clear map for next round
		mapFolderCounts.clear();

		// Commit the transaction
		if (!bNoTransaction) {
			er = lpAttachmentStorage->Commit();
			if (er != erSuccess)
				goto exit;

			er = lpDatabase->Commit();
			if(er != erSuccess)
				goto exit;
		}

		// Deletes have been committed, add the deleted items to the list of items we have deleted
		lstDeleted.splice(lstDeleted.begin(),lstToBeDeleted);

	} // while iterDeleteItems != end()

exit:
	if (er != erSuccess && !bNoTransaction) {
		if(lpInternalAttachmentStorage)
			lpInternalAttachmentStorage->Rollback();

		lpDatabase->Rollback();
	}

	if (lpInternalAttachmentStorage)
		lpInternalAttachmentStorage->Release();

	return er;
}

/*
 * Deleted object cache updates
 *
 * @param[in] lpSession Reference to a session object; cannot be NULL.
 * @param[in] ulFlags Bitmask of flags that controls how the objects will deleted.
 * @param[in] lstDeleted List with deleted objects. 
 */
ECRESULT DeleteObjectCacheUpdate(ECSession *lpSession, unsigned int ulFlags, ECListDeleteItems &lstDeleted)
{
	ECRESULT er = erSuccess;
	ECSessionManager *lpSessionManager = NULL;
	ECCacheManager *lpCacheManager = NULL;
	ECListDeleteItems::iterator iterDeleteItems;

	if (lpSession == NULL) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	lpSessionManager = lpSession->GetSessionManager();
	lpCacheManager = lpSessionManager->GetCacheManager();

	// Remove items from cache and update the outgoing queue
	for(iterDeleteItems=lstDeleted.begin(); iterDeleteItems != lstDeleted.end(); iterDeleteItems++) {

		// update the cache
		lpCacheManager->Update(fnevObjectDeleted, iterDeleteItems->ulId);

		if (iterDeleteItems->fRoot)
			lpCacheManager->Update(fnevObjectModified, iterDeleteItems->ulParent);

		// Update cache, Remove index properties
		if((ulFlags & EC_DELETE_HARD_DELETE) == EC_DELETE_HARD_DELETE)
			lpCacheManager->RemoveIndexData(iterDeleteItems->ulId);
	}

exit:
	
	return er;
}

/*
 * Deleted object notifications
 *
 * @param[in] lpSession Reference to a session object; cannot be NULL.
 * @param[in] ulFlags Bitmask of flags that controls how the objects will deleted.
 * @param[in] lstDeleted List with deleted objects.
 */

ECRESULT DeleteObjectNotifications(ECSession *lpSession, unsigned int ulFlags, ECListDeleteItems &lstDeleted)
{
	ECRESULT er = erSuccess;
	ECSessionManager *lpSessionManager = NULL;
	ECListDeleteItems::iterator iterDeleteItems;

	std::list<unsigned int> lstParent;
	std::list<unsigned int>::iterator iterParent;
	ECMapTableChangeNotifications mapTableChangeNotifications;
	//std::set<unsigned int>	setFolderParents;
	size_t cDeleteditems = lstDeleted.size();
	unsigned int ulGrandParent = 0;

	if (lpSession == NULL) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	lpSessionManager = lpSession->GetSessionManager();

	// Now, send the notifications for MAPI_MESSAGE and MAPI_FOLDER types
	for(iterDeleteItems=lstDeleted.begin(); iterDeleteItems != lstDeleted.end(); iterDeleteItems++)
	{
		// Update the outgoing queue
		// Remove the item from both the local and master outgoing queues
		if( (iterDeleteItems->ulFlags & MSGFLAG_SUBMIT) && iterDeleteItems->ulParentType == MAPI_FOLDER && iterDeleteItems->ulObjType == MAPI_MESSAGE) {
			lpSessionManager->UpdateOutgoingTables(ECKeyTable::TABLE_ROW_DELETE, iterDeleteItems->ulStoreId, iterDeleteItems->ulId, EC_SUBMIT_LOCAL, MAPI_MESSAGE);
			lpSessionManager->UpdateOutgoingTables(ECKeyTable::TABLE_ROW_DELETE, iterDeleteItems->ulStoreId, iterDeleteItems->ulId, EC_SUBMIT_MASTER, MAPI_MESSAGE);
		}

		if( (iterDeleteItems->ulParentType == MAPI_FOLDER && iterDeleteItems->ulObjType == MAPI_MESSAGE) || 
			  iterDeleteItems->ulObjType == MAPI_FOLDER ) 
		{
			// Notify that the message has been deleted
			lpSessionManager->NotificationDeleted(iterDeleteItems->ulObjType, iterDeleteItems->ulId, iterDeleteItems->ulStoreId, &iterDeleteItems->sEntryId, iterDeleteItems->ulParent, iterDeleteItems->ulFlags& (MSGFLAG_ASSOCIATED|MSGFLAG_DELETED) );

			// Update all tables viewing this message
			if(cDeleteditems < EC_TABLE_CHANGE_THRESHOLD) {
				lpSessionManager->UpdateTables(ECKeyTable::TABLE_ROW_DELETE, (iterDeleteItems->ulFlags & (MSGFLAG_ASSOCIATED|MSGFLAG_DELETED)), iterDeleteItems->ulParent, iterDeleteItems->ulId, iterDeleteItems->ulObjType);
		
				if((ulFlags & EC_DELETE_HARD_DELETE) != EC_DELETE_HARD_DELETE) {
					// Update all tables viewing this message
					lpSessionManager->UpdateTables(ECKeyTable::TABLE_ROW_ADD, MSGFLAG_DELETED, iterDeleteItems->ulParent, iterDeleteItems->ulId, iterDeleteItems->ulObjType);
				}
			} else {
				// We need to send a table change notifications later on
				mapTableChangeNotifications[iterDeleteItems->ulParent].insert(TABLECHANGENOTIFICATION(iterDeleteItems->ulObjType, iterDeleteItems->ulFlags & MSGFLAG_NOTIFY_FLAGS));
				if((ulFlags & EC_DELETE_HARD_DELETE) != EC_DELETE_HARD_DELETE) {
					mapTableChangeNotifications[iterDeleteItems->ulParent].insert(TABLECHANGENOTIFICATION(iterDeleteItems->ulObjType, (iterDeleteItems->ulFlags & MSGFLAG_NOTIFY_FLAGS) | MSGFLAG_DELETED));
				}
			}

			// @todo: Is this correct ???
			if (iterDeleteItems->fRoot)
				 lstParent.push_back(iterDeleteItems->ulParent);
		}
	}

	// We have a list of all the folders in which something was deleted, so get a unique list
	lstParent.sort();
	lstParent.unique();

	// Now, send each parent folder a notification that it has been altered and send
	// its parent a notification (ie the grandparent of the deleted object) that its
	// hierarchy table has been changed.
	for(iterParent = lstParent.begin(); iterParent != lstParent.end(); iterParent++) {

		if(cDeleteditems >= EC_TABLE_CHANGE_THRESHOLD) {

			// Find the set of notifications to send for the current parent.
			ECMapTableChangeNotifications::const_iterator iParentNotifications = mapTableChangeNotifications.find(*iterParent);
			if (iParentNotifications != mapTableChangeNotifications.end()) {
				ECSetTableChangeNotifications::const_iterator iNotification;

				// Iterate the set and send notifications.
				for (iNotification = iParentNotifications->second.begin(); iNotification != iParentNotifications->second.end(); ++iNotification)
					lpSessionManager->UpdateTables(ECKeyTable::TABLE_CHANGE, iNotification->ulFlags, *iterParent, 0, iNotification->ulType);
			}
		}

		lpSessionManager->NotificationModified(MAPI_FOLDER, *iterParent);

		if(lpSessionManager->GetCacheManager()->GetParent(*iterParent, &ulGrandParent) == erSuccess)
			lpSessionManager->UpdateTables(ECKeyTable::TABLE_ROW_MODIFY, 0, ulGrandParent, *iterParent, MAPI_FOLDER);
	}

exit:
	return er;
}

/**
 * Mark a store as deleted
 *
 * @param[in] lpSession Reference to a session object; cannot be NULL.
 * @param[in] lpDatabase Reference to a database object; cannot be NULL.
 * @param[in] ulStoreHierarchyId Store id to be delete
 * @param[in] ulSyncId ??????
 *
 * @return Zarafa error code
 */
ECRESULT MarkStoreAsDeleted(ECSession *lpSession, ECDatabase *lpDatabase, unsigned int ulStoreHierarchyId, unsigned int ulSyncId)
{
	ECRESULT er = erSuccess;
	std::string strQuery;
	ECSessionManager *lpSessionManager = NULL;
	ECSearchFolders *lpSearchFolders = NULL;
	ECCacheManager *lpCacheManager = NULL;
	FILETIME ft;

	if (lpSession == NULL || lpDatabase == NULL) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	lpSessionManager = lpSession->GetSessionManager();
	lpSearchFolders = lpSessionManager->GetSearchFolders();
	lpCacheManager = lpSessionManager->GetCacheManager();

	// Remove search results for deleted store
	lpSearchFolders->RemoveSearchFolder(ulStoreHierarchyId);

	// Mark item as deleted
	strQuery = "UPDATE hierarchy SET flags=flags|"+stringify(MSGFLAG_DELETED)+" WHERE id="+stringify(ulStoreHierarchyId);
	er = lpDatabase->DoUpdate(strQuery);
	if(er!= erSuccess)
		goto exit;

	// Add properties: PR_DELETED_ON
	GetSystemTimeAsFileTime(&ft);

	strQuery = "REPLACE INTO properties(hierarchyid, tag, type, val_lo, val_hi) VALUES("+stringify(ulStoreHierarchyId)+","+stringify(PROP_ID(PR_DELETED_ON))+","+stringify(PROP_TYPE(PR_DELETED_ON))+","+stringify(ft.dwLowDateTime)+","+stringify(ft.dwHighDateTime)+")";
	er = lpDatabase->DoUpdate(strQuery);
	if(er!= erSuccess)
		goto exit;


	lpCacheManager->Update(fnevObjectDeleted, ulStoreHierarchyId);

exit:
	return er;
}

/*
 * Delete objects from different stores.
 *
 * Delete a store, folders, messages, reciepints and attachments.
 *
 * @param[in] lpSession Reference to a session object; cannot be NULL.
 * @param[in] lpDatabase Reference to a database object; cannot be NULL.
 * @param[in] ulObjectId Itemid which must be expand.
 * @param[in] ulFlags Bitmask of flags that controls how the objects will deleted.
 * @param[in] ulSyncId ??????
 * @param[in] bNoTransaction Disable the database transactions.
 * @param[in] bCheckPermission Check the objects delete permissions.
 *
 * @return Zarafa error code
 */
ECRESULT DeleteObjects(ECSession *lpSession, ECDatabase *lpDatabase, unsigned int ulObjectId, unsigned int ulFlags, unsigned int ulSyncId, bool bNoTransaction, bool bCheckPermission)
{
	ECListInt sObjectList;

	sObjectList.push_back(ulObjectId);

	return DeleteObjects(lpSession, lpDatabase, &sObjectList, ulFlags, ulSyncId, bNoTransaction, bCheckPermission);
}

/*
 * Delete objects from different stores.
 *
 * Delete a store, folders, messages, reciepints and attachments.
 *
 * @param[in] lpSession Reference to a session object; cannot be NULL.
 * @param[in] lpDatabase Reference to a database object; cannot be NULL.
 * @param[in] lpsObjectList Reference to a list of objects that contains itemid and must be expanded.
 * @param[in] ulFlags Bitmask of flags that controls how the objects will deleted.
 * @param[in] ulSyncId ??????
 * @param[in] bNoTransaction Disable the database transactions.
 * @param[in] bCheckPermission Check the objects delete permissions.
 *
 * @return Zarafa error code
 */
ECRESULT DeleteObjects(ECSession *lpSession, ECDatabase *lpDatabase, ECListInt *lpsObjectList, unsigned int ulFlags, unsigned int ulSyncId, bool bNoTransaction, bool bCheckPermission)
{
	ECRESULT er = erSuccess;
	ECListDeleteItems lstDeleteItems;
	ECListDeleteItems lstDeleted;
	ECListDeleteItems::iterator iterDeleteItems;
	ECSearchFolders *lpSearchFolders = NULL;
	ECSessionManager *lpSessionManager = NULL;
	
	if (lpSession == NULL || lpDatabase == NULL || lpsObjectList == NULL) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	// Make sure we're only deleting things once
	lpsObjectList->sort();
	lpsObjectList->unique();

	lpSessionManager = lpSession->GetSessionManager();
	lpSearchFolders = lpSessionManager->GetSearchFolders();

	if ((bNoTransaction && (ulFlags & EC_DELETE_HARD_DELETE)) || 
		(bNoTransaction && (ulFlags&EC_DELETE_STORE)) ) {
		ASSERT(FALSE); // This means that the caller has a transaction but that's not allowed
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	if(!(ulFlags & EC_DELETE_HARD_DELETE) && !bNoTransaction) {
		er = lpDatabase->Begin();
		if (er != erSuccess)
			goto exit;
	}

	// Collect recursive parent objects, validate item and check the permissions
	er = ExpandDeletedItems(lpSession, lpDatabase, lpsObjectList, ulFlags, bCheckPermission, &lstDeleteItems);
	if (er != erSuccess) {
		g_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_INFO, "Error while expanding delete item list, error code: %u", er);
		goto exit;
	}

	// Remove search results for deleted folders
	if ((ulFlags&EC_DELETE_STORE)) {
		lpSearchFolders->RemoveSearchFolder(*lpsObjectList->begin());
	} else {
		for(iterDeleteItems=lstDeleteItems.begin(); iterDeleteItems != lstDeleteItems.end(); iterDeleteItems++) {
			if(iterDeleteItems->ulObjType == MAPI_FOLDER && iterDeleteItems->ulFlags == FOLDER_SEARCH) {
				lpSearchFolders->RemoveSearchFolder(iterDeleteItems->ulStoreId, iterDeleteItems->ulId);
			}
		}
	}
	
	// Mark or delete objects
	if(ulFlags & EC_DELETE_HARD_DELETE)
		er = DeleteObjectHard(lpSession, lpDatabase, NULL, ulFlags, lstDeleteItems, bNoTransaction, lstDeleted);
	else
		er = DeleteObjectSoft(lpSession, lpDatabase, ulFlags, lstDeleteItems, lstDeleted);
	if (er != erSuccess) {
		g_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_INFO, "Error while deleting expanded item list, error code: %u", er);
		goto exit;
	}

	if (!(ulFlags&EC_DELETE_STORE)) {
		//Those functions are not called with a store delete

		// Update store size
		er = DeleteObjectStoreSize(lpSession, lpDatabase, ulFlags, lstDeleted);
		if(er!= erSuccess) {
			g_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_INFO, "Error while updating store sizes after delete, error code: %u", er);
			goto exit;
		}

		// Update ICS
		er = DeleteObjectUpdateICS(lpSession, ulFlags, lstDeleted, ulSyncId);
		if (er != erSuccess) {
			g_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_INFO, "Error while updating ICS after delete, error code: %u", er);
			goto exit;
		}

		// Update local commit time on top level folders
		for(iterDeleteItems=lstDeleteItems.begin(); iterDeleteItems != lstDeleteItems.end(); iterDeleteItems++) {
			if( !(ulFlags & EC_DELETE_HARD_DELETE) && iterDeleteItems->fRoot &&
				iterDeleteItems->ulParentType == MAPI_FOLDER && iterDeleteItems->ulObjType == MAPI_MESSAGE)
			{
				// directly hard-delete the item is not supported for updating PR_LOCAL_COMMIT_TIME_MAX
				er = WriteLocalCommitTimeMax(NULL, lpDatabase, iterDeleteItems->ulParent, NULL);
				if (er != erSuccess) {
					g_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_INFO, "Error while updating folder access time after delete, error code: %u", er);
					goto exit;
				}
				// the folder will receive a changed notification anyway, since items are being deleted from it
			}
		}
	}
	
	// Finish transaction
	if(!(ulFlags & EC_DELETE_HARD_DELETE) && !bNoTransaction) {
		er = lpDatabase->Commit();
		if (er != erSuccess)
			goto exit;
	}

	// Update cache
	DeleteObjectCacheUpdate(lpSession, ulFlags, lstDeleted);

	// Send notifications
	if (!(ulFlags&EC_DELETE_STORE)) {
		DeleteObjectNotifications(lpSession, ulFlags, lstDeleted);
	}

exit:
	if(er != erSuccess && !bNoTransaction && !(ulFlags & EC_DELETE_HARD_DELETE))
		lpDatabase->Rollback();

	FreeDeletedItems(&lstDeleteItems);

	return er;
}

/** 
 * Update PR_LOCAL_COMMIT_TIME_MAX property on a folder which contents has changed.
 *
 * This function should be called when the contents of a folder change
 * Affected: saveObject, emptyFolder, deleteObjects, (not done: copyObjects, copyFolder)
 * 
 * @param[in]  soap soap struct used for allocating memory for return value, can be NULL
 * @param[in]  lpDatabase database pointer, should be in transaction already
 * @param[in]  ulFolderId folder to update property in
 * @param[out] ppvTime time property that was written on the folder, can be NULL
 * 
 * @return Zarafa error code
 * @retval ZARAFA_E_DATABASE_ERROR database could not be updated
 */
// @todo add parameter to pass ulFolderIdType, to check that it contains MAPI_FOLDER.
ECRESULT WriteLocalCommitTimeMax(struct soap *soap, ECDatabase *lpDatabase, unsigned int ulFolderId, propVal **ppvTime)
{
	ECRESULT er = erSuccess;
	FILETIME ftNow;
	std::string strQuery;
	propVal *pvTime = NULL;

	GetSystemTimeAsFileTime(&ftNow);

	if (soap && ppvTime) {
		pvTime = s_alloc<propVal>(soap);
		pvTime->ulPropTag = PR_LOCAL_COMMIT_TIME_MAX;
		pvTime->__union = SOAP_UNION_propValData_hilo;
		pvTime->Value.hilo = s_alloc<hiloLong>(soap);
		pvTime->Value.hilo->hi = ftNow.dwHighDateTime;
		pvTime->Value.hilo->lo = ftNow.dwLowDateTime;
	}

	strQuery = "REPLACE INTO properties (hierarchyid, tag, type, val_hi, val_lo) VALUES ("
		+stringify(ulFolderId)+","+stringify(PROP_ID(PR_LOCAL_COMMIT_TIME_MAX))+","+stringify(PROP_TYPE(PR_LOCAL_COMMIT_TIME_MAX))+","
		+stringify(ftNow.dwHighDateTime)+","+stringify(ftNow.dwLowDateTime)+")";

	er = lpDatabase->DoInsert(strQuery);
	if (er != erSuccess)
		goto exit;

	if (ppvTime)
		*ppvTime = pvTime;

exit:
	return er;
}

void FreeDeleteItem(DELETEITEM *src)
{
	if(src->sEntryId.__ptr)
		delete [] src->sEntryId.__ptr;
}

void FreeDeletedItems(ECListDeleteItems *lplstDeleteItems)
{
	ECListDeleteItems::iterator iterDeleteItems;
	
	for(iterDeleteItems=lplstDeleteItems->begin(); iterDeleteItems != lplstDeleteItems->end(); iterDeleteItems++) {
		FreeDeleteItem(&(*iterDeleteItems));
	}

	lplstDeleteItems->clear();
}

/**
 * Update value in tproperties by taking value from properties for a list of objects
 *
 * This should be called whenever a value is changed in the 'properties' table outside WriteProps(). It updates the value
 * in tproperties if necessary (it may not be in tproperties at all).
 *
 * @param[in] lpDatabase Database handle
 * @param[in] ulPropTag Property tag to update in tproperties
 * @param[in] ulFolderId Folder ID for all objects in lpObjectIDs
 * @param[in] lpObjectIDs List of object IDs to update
 * @return result
 */
ECRESULT UpdateTProp(ECDatabase *lpDatabase, unsigned int ulPropTag, unsigned int ulFolderId, ECListInt *lpObjectIDs) {
    ECRESULT er = erSuccess;
    std::string strQuery;
    ECListInt::iterator iObjectid;
    
    if(lpObjectIDs->empty())
        goto exit; // Nothing to do

    // Update tproperties by taking value from properties
    strQuery = "UPDATE tproperties JOIN properties on properties.hierarchyid=tproperties.hierarchyid AND properties.tag=tproperties.tag AND properties.type=tproperties.type SET tproperties.val_ulong = properties.val_ulong "
    			"WHERE properties.tag = " + stringify(PROP_ID(ulPropTag)) + " AND properties.type = " + stringify(PROP_TYPE(ulPropTag)) + " AND tproperties.folderid = " + stringify(ulFolderId) + " AND properties.hierarchyid IN (";
    			
    for(iObjectid = lpObjectIDs->begin(); iObjectid != lpObjectIDs->end(); iObjectid++) {

        if(iObjectid != lpObjectIDs->begin())
            strQuery += ",";

        strQuery += stringify(*iObjectid);
    }
    strQuery += ")";

   	er = lpDatabase->DoUpdate(strQuery);
   	if(er != erSuccess)
    	goto exit;
    
exit:
    return er;
}

/**
 * Update value in tproperties by taking value from properties for a single object
 *
 * This should be called whenever a value is changed in the 'properties' table outside WriteProps(). It updates the value
 * in tproperties if necessary (it may not be in tproperties at all).
 *
 * @param[in] lpDatabase Database handle
 * @param[in] ulPropTag Property tag to update in tproperties
 * @param[in] ulFolderId Folder ID for all objects in lpObjectIDs
 * @param[in] ulObjId Object ID to update
 * @return result
 */
ECRESULT UpdateTProp(ECDatabase *lpDatabase, unsigned int ulPropTag, unsigned int ulFolderId, unsigned int ulObjId) {
    ECListInt list;
    
    list.push_back(ulObjId);
    
    return UpdateTProp(lpDatabase, ulPropTag, ulFolderId, &list);
}

/**
 * Update folder count for a folder by adding or removing a certain amount
 *
 * This function can be used to incrementally update a folder count of a folder. The lDelta can be positive (counter increases)
 * or negative (counter decreases)
 *
 * @param lpDatabase Database handle
 * @param ulFolderId Folder ID to update
 * @param ulPropTag Counter property to update (must be type that uses val_ulong (PT_LONG or PT_BOOLEAN))
 * @param lDelta Signed integer change
 * @return result
 */
ECRESULT UpdateFolderCount(ECDatabase *lpDatabase, unsigned int ulFolderId, unsigned int ulPropTag, int lDelta)
{
	ECRESULT er = erSuccess;
	std::string strQuery;
	unsigned int ulParentId;
	unsigned int ulType;
	
	if(lDelta == 0)
		goto exit; // No change

	er = g_lpSessionManager->GetCacheManager()->GetObject(ulFolderId, &ulParentId, NULL, NULL, &ulType);
	if(er != erSuccess)
		goto exit;
	if (ulType != MAPI_FOLDER) {
		g_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_INFO, "Not updating folder count %d for non-folder object %d type %d", lDelta, ulFolderId, ulType);
		ASSERT(ulType == MAPI_FOLDER);
		goto exit;
	}
	
	strQuery = "UPDATE properties SET val_ulong = val_ulong + " + stringify(lDelta,false,true) + " WHERE hierarchyid = " + stringify(ulFolderId) + " AND tag = " + stringify(PROP_ID(ulPropTag)) + " AND type = " + stringify(PROP_TYPE(ulPropTag));
	er = lpDatabase->DoUpdate(strQuery);
	if(er != erSuccess)
		goto exit;

	er = UpdateTProp(lpDatabase, ulPropTag, ulParentId, ulFolderId);
	if(er != erSuccess)
		goto exit;		
		
exit:
	return er;
}


ECRESULT CheckQuota(ECSession *lpecSession, ULONG ulStoreId)
{
	ECRESULT er = erSuccess;
	long long llStoreSize = 0;
	eQuotaStatus QuotaStatus;
	
	er = lpecSession->GetSecurity()->GetStoreSize(ulStoreId, &llStoreSize);
	if (er != erSuccess)
		goto exit;

	er = lpecSession->GetSecurity()->CheckQuota(ulStoreId, llStoreSize, &QuotaStatus);
	if (er != erSuccess)
		goto exit;

	if (QuotaStatus == QUOTA_HARDLIMIT)
		er = ZARAFA_E_STORE_FULL;

exit:
	return er;
}

ECRESULT MapEntryIdToObjectId(ECSession *lpecSession, ECDatabase *lpDatabase, ULONG ulObjId, const entryId &sEntryId)
{
	ECRESULT 	er = erSuccess;
	DB_RESULT	lpDBResult = NULL; 
	std::string	strQuery;

	er = RemoveStaleIndexedProp(lpDatabase, PR_ENTRYID, sEntryId.__ptr, sEntryId.__size);
	if(er != erSuccess) {
		g_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_FATAL, "ERROR: Collision detected while setting entryid. objectid=%u, entryid=%s, user=%u", ulObjId, bin2hex(sEntryId.__size, (unsigned char *)sEntryId.__ptr).c_str(), lpecSession->GetSecurity()->GetUserId());
		er = ZARAFA_E_DATABASE_ERROR;
		goto exit;
	}

	strQuery = "INSERT INTO indexedproperties (hierarchyid,tag,val_binary) VALUES("+stringify(ulObjId)+", 0x0FFF, "+lpDatabase->EscapeBinary(sEntryId.__ptr, sEntryId.__size)+")";
	er = lpDatabase->DoInsert(strQuery);
	if(er != erSuccess)
		goto exit;

	g_lpSessionManager->GetCacheManager()->SetObjectEntryId((entryId*)&sEntryId, ulObjId);

exit:
	if (lpDBResult)
		lpDatabase->FreeResult(lpDBResult);

	return er;
}

ECRESULT UpdateFolderCounts(ECDatabase *lpDatabase, ULONG ulParentId, ULONG ulFlags, propValArray *lpModProps)
{
	ECRESULT er = erSuccess;
	struct propVal *lpPropMessageFlags = NULL;	// non-free
	
	if (ulFlags & MAPI_ASSOCIATED)
		er = UpdateFolderCount(lpDatabase, ulParentId, PR_ASSOC_CONTENT_COUNT, 1);
	else {
		er = UpdateFolderCount(lpDatabase, ulParentId, PR_CONTENT_COUNT, 1);
	
		lpPropMessageFlags = FindProp(lpModProps, PR_MESSAGE_FLAGS);
		if (er == erSuccess && (!lpPropMessageFlags || (lpPropMessageFlags->Value.ul & MSGFLAG_READ) == 0))
			er = UpdateFolderCount(lpDatabase, ulParentId, PR_CONTENT_UNREAD, 1);
	}

	return er;
}

/** 
 * Handles the outgoingqueue table according to the PR_MESSAGE_FLAGS
 * of a message. This function does not do transactions, so you must
 * already be in a database transaction.
 * 
 * @param[in] lpDatabase Database object
 * @param[in] ulSyncId syncid of the message
 * @param[in] ulStoreId storeid of the message
 * @param[in] ulObjId hierarchyid of the message
 * @param[in] bNewItem message is new
 * @param[in] lpModProps properties of the message
 * 
 * @return Zarafa error code
 */
ECRESULT ProcessSubmitFlag(ECDatabase *lpDatabase, ULONG ulSyncId, ULONG ulStoreId, ULONG ulObjId, bool bNewItem, propValArray *lpModProps)
{
	ECRESULT er = erSuccess;
	DB_RESULT	lpDBResult = NULL; 
	std::string	strQuery;
	struct propVal *lpPropMessageFlags = NULL;	// non-free
	ULONG ulPrevSubmitFlag = 0;
	
	// If the messages was saved by an ICS syncer, then we need to sync the PR_MESSAGE_FLAGS for MSGFLAG_SUBMIT if it
	// was included in the save.
	lpPropMessageFlags = FindProp(lpModProps, PR_MESSAGE_FLAGS);

	if (ulSyncId > 0 && lpPropMessageFlags) {
		if (bNewItem) {
			// Item is new, so it's not in the queue at the moment
			ulPrevSubmitFlag = 0;
		} else {
			// Existing item. Check it's current submit flag by looking at the outgoing queue
			strQuery = "SELECT hierarchy_id FROM outgoingqueue WHERE hierarchy_id=" + stringify(ulObjId) + " AND flags & " + stringify(EC_SUBMIT_MASTER) + " = 0";
			er = lpDatabase->DoSelect(strQuery, &lpDBResult);
			if (er != erSuccess)
				goto exit;

			if (lpDatabase->GetNumRows(lpDBResult) > 0) {
				// Item is in the outgoing queue at the moment
				ulPrevSubmitFlag = 1;
			} else {
				// Item is not in the queue at the moment
				ulPrevSubmitFlag = 0;
			}

			lpDatabase->FreeResult(lpDBResult);
			lpDBResult = NULL;

		}

		if ((lpPropMessageFlags->Value.ul & MSGFLAG_SUBMIT) && ulPrevSubmitFlag == 0) {
			// Message was previously not submitted, but it is now, so add it to the outgoing queue and set the correct flags.

			strQuery = "INSERT INTO outgoingqueue (store_id, hierarchy_id, flags) VALUES("+stringify(ulStoreId)+", "+stringify(ulObjId)+"," + stringify(EC_SUBMIT_LOCAL) + ")";
			er = lpDatabase->DoInsert(strQuery);
			if (er != erSuccess)
				goto exit;

			strQuery = "UPDATE properties SET val_ulong = val_ulong | " + stringify(MSGFLAG_SUBMIT) +
							" WHERE hierarchyid = " + stringify(ulObjId) +
							" AND type = " + stringify(PROP_TYPE(PR_MESSAGE_FLAGS)) +
							" AND tag = " + stringify(PROP_ID(PR_MESSAGE_FLAGS));
			er = lpDatabase->DoUpdate(strQuery);
			if (er != erSuccess)
				goto exit;

			// The object has changed, update the cache.
		    g_lpSessionManager->GetCacheManager()->Update(fnevObjectModified, ulObjId);
			// Update in-memory outgoing tables
			g_lpSessionManager->UpdateOutgoingTables(ECKeyTable::TABLE_ROW_ADD, ulStoreId, ulObjId, EC_SUBMIT_LOCAL, MAPI_MESSAGE);

		} else if (((lpPropMessageFlags->Value.ul & MSGFLAG_SUBMIT) == 0) && ulPrevSubmitFlag == 1) {
			// Message was previously submitted, but is not submitted any more. Remove it from the outgoing queue and remove the flags.

			strQuery = "DELETE FROM outgoingqueue WHERE hierarchy_id = " + stringify(ulObjId);
			er = lpDatabase->DoDelete(strQuery);
			if (er != erSuccess)
				goto exit;

			strQuery = "UPDATE properties SET val_ulong = val_ulong & ~" + stringify(MSGFLAG_SUBMIT) +
							" WHERE hierarchyid = " + stringify(ulObjId) +
							" AND type = " + stringify(PROP_TYPE(PR_MESSAGE_FLAGS)) +
							" AND tag = " + stringify(PROP_ID(PR_MESSAGE_FLAGS));
			er = lpDatabase->DoUpdate(strQuery);
			if (er != erSuccess)
				goto exit;

			// The object has changed, update the cache.
		    g_lpSessionManager->GetCacheManager()->Update(fnevObjectModified, ulObjId);
			// Update in-memory outgoing tables
			g_lpSessionManager->UpdateOutgoingTables(ECKeyTable::TABLE_ROW_DELETE, ulStoreId, ulObjId, EC_SUBMIT_LOCAL, MAPI_MESSAGE);
		}
	}

exit:
	if (lpDBResult)
		lpDatabase->FreeResult(lpDBResult);

	return er;
}

ECRESULT CreateNotifications(ULONG ulObjId, ULONG ulObjType, ULONG ulParentId, ULONG ulGrandParentId, bool bNewItem, propValArray *lpModProps, struct propVal *lpvCommitTime)
{
	unsigned int ulObjFlags = 0;
	unsigned int ulParentFlags = 0;
	
	if(!((ulObjType == MAPI_ATTACH || ulObjType == MAPI_MESSAGE) && ulObjType == MAPI_STORE) &&
		(ulObjType == MAPI_MESSAGE || ulObjType == MAPI_FOLDER || ulObjType == MAPI_STORE))

	{
		g_lpSessionManager->GetCacheManager()->GetObjectFlags(ulObjId, &ulObjFlags);

		// update PR_LOCAL_COMMIT_TIME_MAX in cache for disconnected clients who want to know if the folder contents changed
		if (lpvCommitTime) {
			sObjectTableKey key(ulParentId, 0);
			g_lpSessionManager->GetCacheManager()->SetCell(&key, PR_LOCAL_COMMIT_TIME_MAX, lpvCommitTime);
		}

		if (bNewItem) {
			// Notify that the message has been created

			g_lpSessionManager->NotificationCreated(ulObjType, ulObjId, ulParentId);

			g_lpSessionManager->UpdateTables(ECKeyTable::TABLE_ROW_ADD, ulObjFlags & MSGFLAG_NOTIFY_FLAGS, ulParentId, ulObjId, ulObjType);

			// Notify that the folder in which the message resided has changed (PR_CONTENT_COUNT, PR_CONTENT_UNREAD)
			
			if(ulObjFlags & MAPI_ASSOCIATED)
				g_lpSessionManager->GetCacheManager()->UpdateCell(ulParentId, PR_ASSOC_CONTENT_COUNT, 1);
			else {
				g_lpSessionManager->GetCacheManager()->UpdateCell(ulParentId, PR_CONTENT_COUNT, 1);

				struct propVal *lpPropMessageFlags = FindProp(lpModProps, PR_MESSAGE_FLAGS);			
				if (lpPropMessageFlags && (lpPropMessageFlags->Value.ul & MSGFLAG_READ) == 0) {
				    // Unread message
    				g_lpSessionManager->GetCacheManager()->UpdateCell(ulParentId, PR_CONTENT_UNREAD, 1);
				}
			}
			
			g_lpSessionManager->NotificationModified(MAPI_FOLDER, ulParentId);
			if (ulGrandParentId) {
				g_lpSessionManager->GetCacheManager()->GetObjectFlags(ulParentId, &ulParentFlags);
				g_lpSessionManager->UpdateTables(ECKeyTable::TABLE_ROW_MODIFY, ulParentFlags & MSGFLAG_NOTIFY_FLAGS, ulGrandParentId, ulParentId, MAPI_FOLDER);
			}

		} else if (ulObjType == MAPI_STORE) {
			g_lpSessionManager->NotificationModified(ulObjType, ulObjId);
		} else {
			// Notify that the message has been modified
			if (ulObjType == MAPI_MESSAGE)
				g_lpSessionManager->NotificationModified(ulObjType, ulObjId, ulParentId);
			else
				g_lpSessionManager->NotificationModified(ulObjType, ulObjId);

			if (ulParentId)
				g_lpSessionManager->UpdateTables(ECKeyTable::TABLE_ROW_MODIFY, ulObjFlags & MSGFLAG_NOTIFY_FLAGS, ulParentId, ulObjId, ulObjType);

		}
	}

	return erSuccess;
}

ECRESULT WriteSingleProp(ECDatabase *lpDatabase, unsigned int ulObjId, unsigned int ulFolderId, struct propVal *lpPropVal, bool bColumnProp, unsigned int ulMaxQuerySize, std::string &strInsertQuery)
{
	ECRESULT er = erSuccess;
	std::string		strColData;
	std::string		strQueryAppend;
	unsigned int ulColId = 0;
	
	ASSERT(PROP_TYPE(lpPropVal->ulPropTag) != PT_UNICODE);

	er = CopySOAPPropValToDatabasePropVal(lpPropVal, &ulColId, strColData, lpDatabase, bColumnProp);
	if(er != erSuccess) {
		er = erSuccess; // Data from client was bogus, ignore it.
		goto exit;
	}
	
	if(strInsertQuery.empty()) {
		if (bColumnProp) {
			strQueryAppend = "REPLACE INTO tproperties (hierarchyid,tag,type,folderid," + (std::string)PROPCOLVALUEORDER(tproperties) + ") VALUES";
		} else {
			strQueryAppend = "REPLACE INTO properties (hierarchyid,tag,type," + (std::string)PROPCOLVALUEORDER(properties) + ") VALUES";
		}
	} else {
		strQueryAppend = ",";
	}
		
	strQueryAppend += "(" + stringify(ulObjId) + "," +
							stringify(PROP_ID(lpPropVal->ulPropTag)) + "," +
							stringify(PROP_TYPE(lpPropVal->ulPropTag)) + ",";
	if (bColumnProp)
		strQueryAppend += stringify(ulFolderId) + ",";

	for(unsigned int k=0; k<VALUE_NR_MAX; k++) {
		if(k==ulColId) {
			strQueryAppend += strColData;
		} else {
			if(k == VALUE_NR_HILO)
				strQueryAppend += "null,null";
			else
				strQueryAppend += "null";
		}
		if(k != VALUE_NR_MAX-1) {
			strQueryAppend += ",";
		}
	}

	strQueryAppend += ")";

	if (ulMaxQuerySize > 0 && strInsertQuery.size() + strQueryAppend.size() > ulMaxQuerySize) {
		er = ZARAFA_E_TOO_BIG;
		goto exit;
	}

	strInsertQuery.append(strQueryAppend);
	
exit:
	return er;
}

ECRESULT WriteProp(ECDatabase *lpDatabase, unsigned int ulObjId, unsigned int ulParentId, struct propVal *lpPropVal) {
	ECRESULT er = erSuccess;
	std::string strQuery;
	
	strQuery.clear();
	WriteSingleProp(lpDatabase, ulObjId, ulParentId, lpPropVal, false, 0, strQuery);
	er = lpDatabase->DoInsert(strQuery);
	if(er != erSuccess)
		goto exit;
	
	if(ulParentId > 0) {
		strQuery.clear();
		WriteSingleProp(lpDatabase, ulObjId, ulParentId, lpPropVal, true, 0, strQuery);
		er = lpDatabase->DoInsert(strQuery);
		if(er != erSuccess)
			goto exit;
	}
		
exit:
	return er;
}

ECRESULT GetNamesFromIDs(struct soap *soap, ECDatabase *lpDatabase, struct propTagArray *lpPropTags, struct namedPropArray *lpsNames)
{
    ECRESULT er = erSuccess;
    DB_RESULT lpDBResult = NULL;
    DB_ROW lpDBRow = NULL;
    DB_LENGTHS lpDBLen = NULL;
    int i = 0;
    std::string strQuery;

	// Allocate memory for return object
	lpsNames->__ptr = s_alloc<namedProp>(soap, lpPropTags->__size);
	lpsNames->__size = lpPropTags->__size;
	memset(lpsNames->__ptr, 0, sizeof(struct namedProp) * lpPropTags->__size);

	for(i=0;i<lpPropTags->__size;i++) {
		strQuery = "SELECT nameid, namestring, guid FROM names WHERE id=" + stringify(lpPropTags->__ptr[i]-1);

		er = lpDatabase->DoSelect(strQuery, &lpDBResult);
		if(er != erSuccess)
			goto exit;

		if(lpDatabase->GetNumRows(lpDBResult) == 1) {

			lpDBRow = lpDatabase->FetchRow(lpDBResult);
			lpDBLen = lpDatabase->FetchRowLengths(lpDBResult);

			if(lpDBRow != NULL) {
				if(lpDBRow[0] != NULL) {
					// It's an ID type
					lpsNames->__ptr[i].lpId = s_alloc<unsigned int>(soap);
					*lpsNames->__ptr[i].lpId = atoi(lpDBRow[0]);
				} else if(lpDBRow[1] != NULL) {
					// It's a String type
					lpsNames->__ptr[i].lpString = s_alloc<char>(soap, strlen(lpDBRow[1])+1);
					strcpy(lpsNames->__ptr[i].lpString, lpDBRow[1]);
				}

				if(lpDBRow[2] != NULL) {
					// Got a GUID (should always do so ...)
					lpsNames->__ptr[i].lpguid = s_alloc<struct xsd__base64Binary>(soap);
					lpsNames->__ptr[i].lpguid->__size = lpDBLen[2];
					lpsNames->__ptr[i].lpguid->__ptr = s_alloc<unsigned char>(soap, lpDBLen[2]);
					memcpy(lpsNames->__ptr[i].lpguid->__ptr, lpDBRow[2], lpDBLen[2]);
				}
			} else {
				er = ZARAFA_E_DATABASE_ERROR;
				goto exit;
			}
		} else {
			// No entry
			lpsNames->__ptr[i].lpguid = NULL;
			lpsNames->__ptr[i].lpId = NULL;
			lpsNames->__ptr[i].lpString = NULL;
		}

		//Free database results
		if (lpDBResult)
			lpDatabase->FreeResult(lpDBResult);
		lpDBResult = NULL;
	}

exit:
	if (lpDBResult)
		lpDatabase->FreeResult(lpDBResult);

	return er;
}

/**
 * Resets the folder counts of a folder
 *
 * This function resets the counts of a folder by recalculating them from the actual
 * database child entries. If any of the current counts is out-of-date, they are updated to the
 * correct value and the foldercount_reset counter is increased. Note that in theory, foldercount_reset
 * should always remain at 0. If not, this means that a bug somewhere has failed to update the folder
 * count correctly at some point.
 *
 * WARNING this function creates its own transaction!
 *
 * @param[in] lpSession Session to get database handle, etc
 * @param[in] ulObjId ID of the folder to recalc
 * @param[out] lpulUpdates Will be set to number of counters that were updated (may be NULL)
 * @return result
 */
ECRESULT ResetFolderCount(ECSession *lpSession, unsigned int ulObjId, unsigned int *lpulUpdates)
{
	ECRESULT er = erSuccess;
	DB_RESULT lpDBResult = NULL;
	DB_ROW lpDBRow = NULL;
	std::string strQuery;
	string strCC;		// Content count
	string strACC;		// Assoc. content count
	string strDMC;		// Deleted message count
	string strDAC;		// Deleted assoc message count
	string strCFC;		// Child folder count
	string strDFC;		// Deleted folder count
	string strCU;		// Content unread
	
	unsigned int ulAffected = 0;
	unsigned int ulParent = 0;
	
	ECDatabase *lpDatabase = NULL;

	er = lpSession->GetDatabase(&lpDatabase);
	if(er != erSuccess)
		goto exit;

	er = lpDatabase->Begin();
	if(er != erSuccess)
		goto exit;

    // Lock the counters now since the locking order is normally counters/foldercontent/storesize/localcommittimemax. So our lock order
    // is now counters/foldercontent/counters which is compatible (*cough* in theory *cough*)
    strQuery = "SELECT val_ulong FROM properties WHERE hierarchyid = " + stringify(ulObjId) + " FOR UPDATE";
    er = lpDatabase->DoSelect(strQuery, NULL); // don't care about the result
    if (er != erSuccess)
        goto exit;
	
	// Gets counters from hierarchy: cc, acc, dmc, dac, cfc, dfc
	// use for update, since the update query below must see the same values, mysql should already block here.
	strQuery = "SELECT count(if(flags & 0x440 = 0 && type = 5, 1, null)) AS cc, count(if(flags & 0x440 = 0x40 and type = 5, 1, null)) AS acc, count(if(flags & 0x440 = 0x400 and type = 5, 1, null)) AS dmc, count(if(flags & 0x440 = 0x440 and type = 5, 1, null)) AS dac, count(if(flags & 0x400 = 0 and type = 3, 1, null)) AS cfc, count(if(flags & 0x400 and type = 3, 1, null)) AS dfc from hierarchy where parent=" + stringify(ulObjId) + " FOR UPDATE";
	er = lpDatabase->DoSelect(strQuery, &lpDBResult);
	if (er != erSuccess)
		goto exit;

	lpDBRow = lpDatabase->FetchRow(lpDBResult);
	if(lpDBRow == NULL || lpDBRow[0] == NULL || lpDBRow[1] == NULL || lpDBRow[2] == NULL || lpDBRow[3] == NULL || lpDBRow[4] == NULL) {
		er = ZARAFA_E_DATABASE_ERROR;
		goto exit;
	}
	
	strCC = lpDBRow[0];
	strACC = lpDBRow[1];
	strDMC = lpDBRow[2];
	strDAC = lpDBRow[3];
	strCFC = lpDBRow[4];
	strDFC = lpDBRow[5];
	
	lpDatabase->FreeResult(lpDBResult);
	lpDBResult = NULL;
	
	// Gets unread counters from hierarchy / properties / tproperties
	strQuery = "SELECT "
	          // Part one, unread count from non-deferred rows (get the flags from tproperties)
	          "(SELECT count(if(tproperties.val_ulong & 1,null,1)) from hierarchy left join tproperties on tproperties.folderid=" + stringify(ulObjId) + " and tproperties.tag = 0x0e07 and tproperties.type = 3 and tproperties.hierarchyid=hierarchy.id left join deferredupdate on deferredupdate.hierarchyid=hierarchy.id where parent=" + stringify(ulObjId) + " and hierarchy.type=5 and hierarchy.flags = 0 and deferredupdate.hierarchyid is null FOR UPDATE)"
	          " + "
	          // Part two, unread count from deferred rows (get the flags from properties)
	          "(SELECT count(if(properties.val_ulong & 1,null,1)) from hierarchy left join properties on properties.tag = 0x0e07 and properties.type = 3 and properties.hierarchyid=hierarchy.id join deferredupdate on deferredupdate.hierarchyid=hierarchy.id and deferredupdate.folderid = parent where parent=" + stringify(ulObjId) + " and hierarchy.type=5 and hierarchy.flags = 0 FOR UPDATE)"
	          ;
	er = lpDatabase->DoSelect(strQuery, &lpDBResult);
	if (er != erSuccess)
		goto exit;

	lpDBRow = lpDatabase->FetchRow(lpDBResult);
	if(lpDBRow == NULL || lpDBRow[0] == NULL) {
		er = ZARAFA_E_DATABASE_ERROR;
		goto exit;
	}
	
	strCU = lpDBRow[0];
	
	lpDatabase->FreeResult(lpDBResult);
	lpDBResult = NULL;

    strQuery = "UPDATE properties SET val_ulong = CASE tag "
      " WHEN " + stringify(PROP_ID(PR_CONTENT_COUNT)) + " THEN + " + strCC +
      " WHEN " + stringify(PROP_ID(PR_ASSOC_CONTENT_COUNT)) + " THEN + " + strACC +
      " WHEN " + stringify(PROP_ID(PR_DELETED_MSG_COUNT)) + " THEN + " + strDMC +
      " WHEN " + stringify(PROP_ID(PR_DELETED_ASSOC_MSG_COUNT)) + " THEN + " + strDAC +
      " WHEN " + stringify(PROP_ID(PR_FOLDER_CHILD_COUNT)) + " THEN + " + strCFC +
      " WHEN " + stringify(PROP_ID(PR_SUBFOLDERS)) + " THEN + " + strCFC +
      " WHEN " + stringify(PROP_ID(PR_DELETED_FOLDER_COUNT)) + " THEN + " + strDFC +
      " WHEN " + stringify(PROP_ID(PR_CONTENT_UNREAD)) + " THEN + " + strCU +
      " END WHERE hierarchyid = " + stringify(ulObjId) + " AND TAG in (" + 
          stringify(PROP_ID(PR_CONTENT_COUNT)) + "," + 
          stringify(PROP_ID(PR_ASSOC_CONTENT_COUNT)) + "," + 
          stringify(PROP_ID(PR_DELETED_MSG_COUNT)) + "," + 
          stringify(PROP_ID(PR_DELETED_ASSOC_MSG_COUNT)) + "," + 
          stringify(PROP_ID(PR_FOLDER_CHILD_COUNT)) + "," + 
          stringify(PROP_ID(PR_SUBFOLDERS)) + "," + 
          stringify(PROP_ID(PR_DELETED_FOLDER_COUNT)) + "," + 
          stringify(PROP_ID(PR_CONTENT_UNREAD)) +
      ")";
      
    er = lpDatabase->DoUpdate(strQuery, &ulAffected);
    if(er != erSuccess)
        goto exit;

    if (ulAffected == 0) {
        // Nothing updated
        goto exit;
    }
    
    // Trigger an assertion since in practice this should never happen
//    ASSERT(false);
	g_lpStatsCollector->Increment(SCN_DATABASE_COUNTER_RESYNCS);

	er = lpSession->GetSessionManager()->GetCacheManager()->GetParent(ulObjId, &ulParent);
	if(er != erSuccess) {
		// No parent -> root folder. Nothing else we need to do now.
		er = erSuccess;
		goto exit;
	}
    
    // Update tprops
    strQuery = "REPLACE INTO tproperties (folderid, hierarchyid, tag, type, val_ulong) "
        "SELECT " + stringify(ulParent) + ", properties.hierarchyid, properties.tag, properties.type, properties.val_ulong "
        "FROM properties "
        "WHERE tag IN (" +
          stringify(PROP_ID(PR_CONTENT_COUNT)) + "," + 
          stringify(PROP_ID(PR_ASSOC_CONTENT_COUNT)) + "," + 
          stringify(PROP_ID(PR_DELETED_MSG_COUNT)) + "," + 
          stringify(PROP_ID(PR_DELETED_ASSOC_MSG_COUNT)) + "," + 
          stringify(PROP_ID(PR_FOLDER_CHILD_COUNT)) + "," + 
          stringify(PROP_ID(PR_SUBFOLDERS)) + "," + 
          stringify(PROP_ID(PR_DELETED_FOLDER_COUNT)) + "," + 
          stringify(PROP_ID(PR_CONTENT_UNREAD)) +
        ") AND hierarchyid = " + stringify(ulObjId);
        
    er = lpDatabase->DoInsert(strQuery);
    if(er != erSuccess)
        goto exit;
        
    // Clear cache and update table entries. We do not send an object notification since the object hasn't really changed and
    // this is normally called just before opening an entry anyway, so the counters retrieved there will be ok.
    lpSession->GetSessionManager()->GetCacheManager()->Update(fnevObjectModified, ulObjId);
    er = lpSession->GetSessionManager()->UpdateTables(ECKeyTable::TABLE_ROW_MODIFY, 0, ulParent, ulObjId, MAPI_FOLDER);
    if(er != erSuccess)
    	goto exit;
    	
	
exit:
	if(er != erSuccess)
		lpDatabase->Rollback();
	else {
		lpDatabase->Commit();
		if (lpulUpdates)
			*lpulUpdates = ulAffected;
	}
		
	return er;	
}

/**
 * Removes stale indexed properties
 *
 * In some cases, the database can contain stale (old) indexed properties. One example is when
 * you replicate a store onto a server, then remove that store with zarafa-admin --remove-store
 * and then do the replication again. The second replication will attempt to create items with
 * equal entryids and sourcekeys. Since the softdelete purge will not have removed the data from
 * the system yet, we check to see if the indexedproperty that is in the database is actually in
 * use by checking if the store it belongs to is deleted. If so, we remove the entry. If the
 * item is used by a non-deleted store, then an error occurs since you cannot use the same indexed
 * property for two items.
 *
 * @param[in] lpDatabase Database handle
 * @param[in] ulPropTag Property tag to scan for 
 * @param[in] lpData Data if the indexed property
 * @param[in] cbSize Bytes in lpData
 * @return result
 */
ECRESULT RemoveStaleIndexedProp(ECDatabase *lpDatabase, unsigned int ulPropTag, unsigned char *lpData, unsigned int cbSize)
{
	ECRESULT er = erSuccess;
	DB_RESULT lpDBResult = NULL;
	DB_ROW lpDBRow = NULL;
	std::string strQuery;
	unsigned int ulObjId = 0;
	unsigned int ulStoreId = 0;
	bool bStale = false;

	strQuery = "SELECT hierarchyid FROM indexedproperties WHERE tag= " + stringify(PROP_ID(ulPropTag)) + " AND val_binary=" + lpDatabase->EscapeBinary(lpData, cbSize);
	er = lpDatabase->DoSelect(strQuery, &lpDBResult);
	if(er != erSuccess)
	    goto exit;
	    
    lpDBRow = lpDatabase->FetchRow(lpDBResult);
    if(!lpDBRow || lpDBRow[0] == NULL)
        goto exit; // Nothing there, no need to do anything
        
    ulObjId = atoui(lpDBRow[0]);
        
    // Check if the found item is in a deleted store
    if(g_lpSessionManager->GetCacheManager()->GetStore(ulObjId, &ulStoreId, NULL) == erSuccess) {
        lpDatabase->FreeResult(lpDBResult);
        lpDBResult = NULL;
        
        // Find the store
        strQuery = "SELECT hierarchy_id FROM stores WHERE hierarchy_id = " + stringify(ulStoreId);
        er = lpDatabase->DoSelect(strQuery, &lpDBResult);
        if(er != erSuccess)
            goto exit;

        lpDBRow = lpDatabase->FetchRow(lpDBResult);
        if(!lpDBRow || lpDBRow[0] == NULL) {
            bStale = true;
        }        
    } else {
        // The item has no store. This means it's safe to re-use the indexed prop. Possibly the store is half-deleted at this time.
        bStale = true;
    }

    if(bStale) {
        // Item is in a deleted store. This means we can delete it
        er = lpDatabase->DoDelete("DELETE FROM indexedproperties WHERE hierarchyid = " + stringify(ulObjId));
        if(er != erSuccess)
            goto exit;
            
        // Remove it from the cache
        g_lpSessionManager->GetCacheManager()->RemoveIndexData(ulPropTag, cbSize, lpData);
    } else {
        // Caller wanted to remove the entry, but we can't since it is in use
        er = ZARAFA_E_COLLISION;
    }

exit:
	if (lpDBResult)
		lpDatabase->FreeResult(lpDBResult);

	return er;
}

ECRESULT ApplyFolderCounts(ECDatabase *lpDatabase, unsigned int ulFolderId, const PARENTINFO &pi) {
    ECRESULT er = erSuccess;
    
    er = UpdateFolderCount(lpDatabase, ulFolderId, PR_CONTENT_COUNT,    			pi.lItems);
    if (er == erSuccess)
        er = UpdateFolderCount(lpDatabase, ulFolderId, PR_CONTENT_UNREAD,   		pi.lUnread);
    if (er == erSuccess)
        er = UpdateFolderCount(lpDatabase, ulFolderId, PR_ASSOC_CONTENT_COUNT,   	pi.lAssoc);
    if (er == erSuccess)
        er = UpdateFolderCount(lpDatabase, ulFolderId, PR_DELETED_MSG_COUNT, 		pi.lDeleted);
    if (er == erSuccess)
        er = UpdateFolderCount(lpDatabase, ulFolderId, PR_DELETED_ASSOC_MSG_COUNT, 	pi.lDeletedAssoc);
    if (er == erSuccess)
        er = UpdateFolderCount(lpDatabase, ulFolderId, PR_SUBFOLDERS,  				pi.lFolders);
    if (er == erSuccess)
        er = UpdateFolderCount(lpDatabase, ulFolderId, PR_FOLDER_CHILD_COUNT,		pi.lFolders);
    if (er == erSuccess)
        er = UpdateFolderCount(lpDatabase, ulFolderId, PR_DELETED_FOLDER_COUNT,		pi.lDeletedFolders);
    if (er != erSuccess)
        goto exit;
        
exit:
    return er;
}

ECRESULT ApplyFolderCounts(ECDatabase *lpDatabase, const std::map<unsigned int, PARENTINFO> &mapFolderCounts) {
    ECRESULT er = erSuccess;
    
	// Update folder counts
	for(std::map<unsigned int, PARENTINFO>::const_iterator iterFolderCounts = mapFolderCounts.begin(); iterFolderCounts != mapFolderCounts.end(); iterFolderCounts++) {
	    er = ApplyFolderCounts(lpDatabase, iterFolderCounts->first, iterFolderCounts->second);
	    if(er != erSuccess)
	        goto exit;
	}
	
exit:
	return er;
}

ECRESULT LockFolders(ECDatabase *lpDatabase, bool bShared, const std::set<unsigned int> &setParents)
{
    ECRESULT er = erSuccess;
    std::string strQuery;
    std::set<unsigned int>::const_iterator i;

    if(setParents.empty())
        goto exit;
    
    strQuery = "SELECT * FROM properties WHERE hierarchyid IN(";
    
    for(i = setParents.begin(); i != setParents.end(); i++) {
        strQuery += stringify(*i);
        strQuery += ",";
    }
    strQuery.resize(strQuery.size()-1);
    strQuery += ") ";
    
    if(bShared)
        strQuery += "LOCK IN SHARE MODE";
    else
        strQuery += "FOR UPDATE";
    
    er = lpDatabase->DoSelect(strQuery, NULL);
    
exit:
    return er;
}

ECRESULT BeginLockFolders(ECDatabase *lpDatabase, unsigned int ulTag, const std::set<std::string>& setIds, unsigned int ulFlags)
{
    ECRESULT er = erSuccess;
    DB_RESULT lpDBResult = NULL;
    DB_ROW lpDBRow = NULL;
    std::set<unsigned int> setMessages;
    std::set<unsigned int> setFolders;
    std::set<std::string> setUncached;
    std::set<unsigned int> setUncachedMessages;
    unsigned int ulId;
    std::string strQuery;
    
    // See if we can get the object IDs for the passed objects from the cache
    for(std::set<std::string>::const_iterator i = setIds.begin(); i != setIds.end(); i++) {
        if(g_lpSessionManager->GetCacheManager()->QueryObjectFromProp(ulTag, i->size(), (unsigned char *)i->data(), &ulId) == erSuccess) {
            if(ulTag == PROP_ID(PR_SOURCE_KEY))
                setFolders.insert(ulId);
            else if(ulTag == PROP_ID(PR_ENTRYID)) {
                EntryId eid(*i);
                
                if(eid.type() == MAPI_FOLDER) {
                    setFolders.insert(ulId);
                } else if(eid.type() == MAPI_MESSAGE) {
                    setMessages.insert(ulId);
                } else {
                    ASSERT(false); // You should not attempt to lock things other than messages and folders
                }
            }
            else {
                ASSERT(false);
            }
        } else {
            setUncached.insert(*i);
        }
    }

    if(!setUncached.empty()) {    
        // For the items that were uncached, go directly to their parent (or the item itself for folders) in the DB
        strQuery = "SELECT hierarchyid, hierarchy.type, hierarchy.parent FROM indexedproperties JOIN hierarchy ON hierarchy.id=indexedproperties.hierarchyid WHERE tag = " + stringify(ulTag) + " AND val_binary IN(";
        for(std::set<std::string>::iterator i= setUncached.begin(); i != setUncached.end(); i++) {
            if(i != setUncached.begin())
                strQuery += ",";
            strQuery += lpDatabase->EscapeBinary(*i);
        }
        strQuery += ")";
        
        er = lpDatabase->DoSelect(strQuery, &lpDBResult);
        if(er != erSuccess)
            goto exit;
        
        while((lpDBRow = lpDatabase->FetchRow(lpDBResult))) {
            if(lpDBRow[0] == NULL || lpDBRow[1] == NULL || lpDBRow[2] == NULL)
                continue;
                
            if(atoui(lpDBRow[1]) == MAPI_MESSAGE)
                setFolders.insert(atoui(lpDBRow[2]));
            else if(atoui(lpDBRow[1]) == MAPI_FOLDER)
                setFolders.insert(atoui(lpDBRow[0]));
        }

        lpDatabase->FreeResult(lpDBResult);
        lpDBResult = NULL;
    }
        
    // For the items that were cached, but messages, find their parents in the cache first
    for(std::set<unsigned int>::iterator i = setMessages.begin(); i != setMessages.end(); i++) {
        unsigned int ulParent = 0;
        
        if(g_lpSessionManager->GetCacheManager()->QueryParent(*i, &ulParent) == erSuccess) {
            setFolders.insert(ulParent);
        } else {
            setUncachedMessages.insert(*i);
        }
    }
    
    // Query uncached parents from the database
    if(!setUncachedMessages.empty()) {
        strQuery = "SELECT parent FROM hierarchy WHERE id IN(";
        for(std::set<unsigned int>::iterator i = setUncachedMessages.begin(); i != setUncachedMessages.end(); i++) {
            if(i != setUncachedMessages.begin())
                strQuery += ",";
            strQuery += stringify(*i);
        }
        strQuery += ")";
        
        er = lpDatabase->DoSelect(strQuery, &lpDBResult);
        if(er != erSuccess)
            goto exit;

        while((lpDBRow = lpDatabase->FetchRow(lpDBResult))) {
            if(lpDBRow[0] == NULL)
                continue;
                
            setFolders.insert(atoui(lpDBRow[0]));
        }    
        
        lpDatabase->FreeResult(lpDBResult);
        lpDBResult = NULL;
    }
        
    // Query objectid -> parentid for messages
    if(setFolders.empty()) {
        // No objects found that we can lock, fail.
        er = ZARAFA_E_NOT_FOUND;
        goto exit;
    }
    
    er = lpDatabase->Begin();
    if(er != erSuccess)
        goto exit;
        
    er = LockFolders(lpDatabase, ulFlags & LOCK_SHARED, setFolders);
    if(er != erSuccess)
        goto exit;
    
exit:
    if(lpDBResult)
        lpDatabase->FreeResult(lpDBResult);
        
    return er;
}

/**
 * Begin a new transaction and lock folders
 *
 * Sourcekey of folders should be passed in setFolders.
 *
 */
ECRESULT BeginLockFolders(ECDatabase *lpDatabase, const std::set<SOURCEKEY>& setFolders, unsigned int ulFlags)
{
    std::set<std::string> setIds;
    
    std::copy(setFolders.begin(), setFolders.end(), std::inserter(setIds, setIds.begin()));
    return BeginLockFolders(lpDatabase, PROP_ID(PR_SOURCE_KEY), setIds, ulFlags);
}

/**
 * Begin a new transaction and lock folders
 *
 * EntryID of messages and folders to lock can be passed in setEntryIds. In practice, only the folders
 * in which the messages reside are locked.
 */
ECRESULT BeginLockFolders(ECDatabase *lpDatabase, const std::set<EntryId>& setEntryIds, unsigned int ulFlags)
{
    std::set<std::string> setIds;
    
    std::copy(setEntryIds.begin(), setEntryIds.end(), std::inserter(setIds, setIds.begin()));
    return BeginLockFolders(lpDatabase, PROP_ID(PR_ENTRYID), setIds, ulFlags);
}

ECRESULT BeginLockFolders(ECDatabase *lpDatabase, const EntryId &entryid, unsigned int ulFlags)
{
    std::set<EntryId> set;
    
    // No locking needed for stores
    if(entryid.type() == MAPI_STORE) {
        return lpDatabase->Begin();
    }
    
    set.insert(entryid);
    
    return BeginLockFolders(lpDatabase, set, ulFlags);
}

ECRESULT BeginLockFolders(ECDatabase *lpDatabase, const SOURCEKEY &sourcekey, unsigned int ulFlags)
{
    std::set<SOURCEKEY> set;
    
    set.insert(sourcekey);
    
    return BeginLockFolders(lpDatabase, set, ulFlags);
}

// Prepares child property data. This can be passed to ReadProps(). This allows the properties of child objects of object ulObjId to be
// retrieved with far less SQL queries, since this function bulk-receives the data. You may pass EITHER ulObjId OR ulParentId to retrieve an object itself, or
// children of an object.
ECRESULT PrepareReadProps(struct soap *soap, ECDatabase *lpDatabase, bool fDoQuery, bool fUnicode, unsigned int ulObjId, unsigned int ulParentId, unsigned int ulMaxSize, ChildPropsMap *lpChildProps, NamedPropDefMap *lpNamedPropDefs)
{
    ECRESULT er = erSuccess;
	ChildPropsMap::iterator iterChild;
	unsigned int ulSize;
	struct propVal sPropVal;
    unsigned int ulChildId;
	ECStringCompat stringCompat(fUnicode);
	std::string strQuery;
	DB_RESULT lpDBResult = NULL;
	DB_ROW lpDBRow = NULL;
	DB_LENGTHS lpDBLen = NULL;

	if(ulObjId == 0 && ulParentId == 0) {
	    er = ZARAFA_E_INVALID_PARAMETER;
	    goto exit;
    }

    if(fDoQuery) {
        if(ulObjId)
            strQuery = "SELECT " PROPCOLORDER ",hierarchyid FROM properties FORCE INDEX (PRIMARY) WHERE hierarchyid="+stringify(ulObjId);
        else
            strQuery = "SELECT " PROPCOLORDER ", hierarchy.id, names.nameid, names.namestring, names.guid "
                       "FROM properties FORCE INDEX (PRIMARY) "
                       "JOIN hierarchy FORCE INDEX (parenttypeflags) "
                           "ON properties.hierarchyid=hierarchy.id "
                       "LEFT JOIN names "
                           "ON (properties.tag-0x8501)=names.id "
                       "WHERE hierarchy.parent="+stringify(ulParentId)+" AND (tag <= 0x8500 OR names.id IS NOT NULL)";

        er = lpDatabase->DoSelect(strQuery, &lpDBResult);
        if(er != erSuccess)
            goto exit;
    } else {
        er = lpDatabase->GetNextResult(&lpDBResult);
        if(er != erSuccess)
            goto exit;
    }

    while((lpDBRow = lpDatabase->FetchRow(lpDBResult)) != NULL) {
        unsigned int ulPropTag;
        
        lpDBLen = lpDatabase->FetchRowLengths(lpDBResult);

        if(lpDBLen == NULL) {
            er = ZARAFA_E_DATABASE_ERROR; // this should never happen
            goto exit;
        }

        ulPropTag = PROP_TAG(atoi(lpDBRow[FIELD_NR_TYPE]),atoi(lpDBRow[FIELD_NR_TAG]));
        
        if (PROP_ID(ulPropTag) > 0x8500 && lpNamedPropDefs) {
            std::pair<NamedPropDefMap::iterator, bool> resInsert = lpNamedPropDefs->insert(NamedPropDefMap::value_type(ulPropTag, NAMEDPROPDEF()));
            if (resInsert.second) {
                // New entry
                if (lpDBLen[FIELD_NR_NAMEGUID] != sizeof(resInsert.first->second.guid)) {
                    er = ZARAFA_E_DATABASE_ERROR;
                    goto exit;
                }
                memcpy(&resInsert.first->second.guid, lpDBRow[FIELD_NR_NAMEGUID], sizeof(resInsert.first->second.guid));
                
                if (lpDBRow[FIELD_NR_NAMEID] != NULL) {
                    resInsert.first->second.ulKind = MNID_ID;
                    resInsert.first->second.ulId = atoui((char*)lpDBRow[FIELD_NR_NAMEID]);
                } else if (lpDBRow[FIELD_NR_NAMESTR] != NULL) {
                    resInsert.first->second.ulKind = MNID_STRING;
                    resInsert.first->second.strName.assign((char*)lpDBRow[FIELD_NR_NAMESTR], lpDBLen[FIELD_NR_NAMESTR]);
                } else {
                    er = ZARAFA_E_INVALID_TYPE;
                    goto exit;
                }
            }
        }

		// server strings are always unicode, for unicode clients.
		if (fUnicode) {
			if (PROP_TYPE(ulPropTag) == PT_STRING8)
				ulPropTag = CHANGE_PROP_TYPE(ulPropTag, PT_UNICODE);
			else if (PROP_TYPE(ulPropTag) == PT_MV_STRING8)
				ulPropTag = CHANGE_PROP_TYPE(ulPropTag, PT_MV_UNICODE);
		}

        ulChildId = atoui(lpDBRow[FIELD_NR_MAX]);

        iterChild = lpChildProps->find(ulChildId);
        
        if(iterChild == lpChildProps->end()) {
            CHILDPROPS sChild;
            
            sChild.lpPropTags = new DynamicPropTagArray(soap);
            sChild.lpPropVals = new DynamicPropValArray(soap, 20);
            
            // First property for this child
            iterChild = lpChildProps->insert(ChildPropsMap::value_type(ulChildId, sChild)).first;
        }
        
        er = iterChild->second.lpPropTags->AddPropTag(ulPropTag);
        if(er != erSuccess)
            goto exit;

        er = GetPropSize(lpDBRow, lpDBLen, &ulSize);

        if(er == erSuccess && (ulMaxSize == 0 || ulSize < ulMaxSize)) {
            // the size of this property is small enough to send in the initial loading sequence
            
            er = CopyDatabasePropValToSOAPPropVal(soap, lpDBRow, lpDBLen, &sPropVal);
            if(er != erSuccess)
                continue;

			er = FixPropEncoding(soap, stringCompat, Out, &sPropVal);
			if (er != erSuccess)
				continue;
                
            iterChild->second.lpPropVals->AddPropVal(sPropVal);

			if (!soap)
				FreePropVal(&sPropVal, false);
        }
    }

    lpDatabase->FreeResult(lpDBResult);
    lpDBResult = NULL;

    if(fDoQuery) {
        if(ulObjId)
            strQuery = "SELECT " MVPROPCOLORDER ", hierarchyid FROM mvproperties WHERE hierarchyid="+stringify(ulObjId)+" GROUP BY hierarchyid, tag";
        else
            strQuery = "SELECT " MVPROPCOLORDER ", hierarchy.id, names.nameid, names.namestring, names.guid "
                       "FROM mvproperties "
                       "JOIN hierarchy "
                           "ON mvproperties.hierarchyid=hierarchy.id "
                       "LEFT JOIN names "
                           "ON (mvproperties.tag-0x8501)=names.id "
                       "WHERE hierarchy.parent="+stringify(ulParentId)+" AND (tag <= 0x8500 OR names.id IS NOT NULL) "
                       "GROUP BY tag, mvproperties.type";
                       

        er = lpDatabase->DoSelect(strQuery, &lpDBResult);
        if(er != erSuccess)
            goto exit;
    } else {
        er = lpDatabase->GetNextResult(&lpDBResult);
        if(er != erSuccess)
            goto exit;
    }
    
    // Do MV props
    while((lpDBRow = lpDatabase->FetchRow(lpDBResult)) != NULL) {
        lpDBLen = lpDatabase->FetchRowLengths(lpDBResult);

        if(lpDBLen == NULL) {
            er = ZARAFA_E_DATABASE_ERROR; // this should never happen
            goto exit;
        }
        
        if (lpNamedPropDefs) {
            unsigned int ulPropTag = PROP_TAG(atoi(lpDBRow[FIELD_NR_TYPE]),atoi(lpDBRow[FIELD_NR_TAG]));
            if (PROP_ID(ulPropTag) > 0x8500) {
                std::pair<NamedPropDefMap::iterator, bool> resInsert = lpNamedPropDefs->insert(NamedPropDefMap::value_type(ulPropTag, NAMEDPROPDEF()));
                if (resInsert.second) {
                    // New entry
                    if (lpDBLen[FIELD_NR_NAMEGUID] != sizeof(resInsert.first->second.guid)) {
                        er = ZARAFA_E_DATABASE_ERROR;
                        goto exit;
                    }
                    memcpy(&resInsert.first->second.guid, lpDBRow[FIELD_NR_NAMEGUID], sizeof(resInsert.first->second.guid));
                    
                    if (lpDBRow[FIELD_NR_NAMEID] != NULL) {
                        resInsert.first->second.ulKind = MNID_ID;
                        resInsert.first->second.ulId = atoui((char*)lpDBRow[FIELD_NR_NAMEID]);
                    } else if (lpDBRow[FIELD_NR_NAMESTR] != NULL) {
                        resInsert.first->second.ulKind = MNID_STRING;
                        resInsert.first->second.strName.assign((char*)lpDBRow[FIELD_NR_NAMESTR], lpDBLen[FIELD_NR_NAMESTR]);
                    } else {
                        er = ZARAFA_E_INVALID_TYPE;
                        goto exit;
                    }
                }
            }
        }

        ulChildId = atoui(lpDBRow[FIELD_NR_MAX]);

        iterChild = lpChildProps->find(ulChildId);
        
        if(iterChild == lpChildProps->end()) {
            CHILDPROPS sChild;
            
            sChild.lpPropTags = new DynamicPropTagArray(soap);
            sChild.lpPropVals = new DynamicPropValArray(soap, 20);
            
            // First property for this child
            iterChild = lpChildProps->insert(ChildPropsMap::value_type(ulChildId, sChild)).first;
        }
        
        er = CopyDatabasePropValToSOAPPropVal(soap, lpDBRow, lpDBLen, &sPropVal);
        if(er != erSuccess)
            continue;

		er = FixPropEncoding(soap, stringCompat, Out, &sPropVal);
		if (er != erSuccess)
			continue;
            
        er = iterChild->second.lpPropTags->AddPropTag(sPropVal.ulPropTag);
        if(er != erSuccess)
            continue;

        iterChild->second.lpPropVals->AddPropVal(sPropVal);

		if (!soap)
			FreePropVal(&sPropVal, false);
    }

exit:
    if(lpDBResult)
        lpDatabase->FreeResult(lpDBResult);

	return er;
}

ECRESULT FreeChildProps(std::map<unsigned int, CHILDPROPS> *lpChildProps)
{
    std::map<unsigned int, CHILDPROPS>::iterator iterChild;
    
    for(iterChild = lpChildProps->begin(); iterChild != lpChildProps->end(); iterChild++)
    {
        if(iterChild->second.lpPropVals)
            delete iterChild->second.lpPropVals;
        if(iterChild->second.lpPropTags)
            delete iterChild->second.lpPropTags;
    }
    
    lpChildProps->clear();
    
    return erSuccess;
}

