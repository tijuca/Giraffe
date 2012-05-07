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
#include "ECDatabase.h"

#include <mapidefs.h>

#include "ECSecurity.h"
#include "ECSessionManager.h"
#include "ECConvenientDepthObjectTable.h"
#include "ECSession.h"
#include "ECMAPI.h"
#include "stringutil.h"

#include <list>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

ECConvenientDepthObjectTable::ECConvenientDepthObjectTable(ECSession *lpSession, unsigned int ulStoreId, LPGUID lpGuid, unsigned int ulFolderId, unsigned int ulObjType, unsigned int ulFlags, const ECLocale &locale) : ECStoreObjectTable(lpSession, ulStoreId, lpGuid, 0, ulObjType, ulFlags, locale) {
    m_ulFolderId = ulFolderId;
}

/*
 * Loads an entire multi-depth hierarchy table recursively.
 *
 * The only way to do this nicely is by recursively getting all the hierarchy id's for all folders under the root folder X
 *
 * Because these queries are really light and fast, the main goals is to limit the amount of calls to mysql to an absolute minimum. We do
 * this by querying all the information we know until now; We first request the subfolders for folder X. In the next call, we request all
 * the subfolders for *ALL* those subfolders. After that, we get all the subfolders for all those subfolders, etc.
 *
 * This means that the number of SQL calls we have to do is equal to the maximum depth level in a given tree hierarchy, which is usually
 * around 5 or so.
 *
 */

typedef std::list<ECSortKey> SortKey;
typedef struct FOLDERINFO {
    unsigned int ulFolderId;		// Actual folder id in the DB
    std::string strFolderName;		// Folder name like 'inbox'
    SortKey sortKey;				// List of collation keys of the folder names.
    
    bool operator < (const FOLDERINFO &a) {
		SortKey::const_iterator iKeyThis = sortKey.begin();
		SortKey::const_iterator iKeyOther = a.sortKey.begin();

		while (iKeyThis != sortKey.end() && iKeyOther != a.sortKey.end()) {
			int res = iKeyThis->compareTo(*iKeyOther);
			if (res < 0) return true;
			if (res > 0) return false;

			++iKeyThis;
			++iKeyOther;
		}

		// If we get this far, all collation keys were equal. So we should only return true if this.sortKey has less items than a.sortKey.
		return sortKey.size() < a.sortKey.size();
    }
} FOLDERINFO;

ECRESULT ECConvenientDepthObjectTable::Create(ECSession *lpSession, unsigned int ulStoreId, GUID *lpGuid, unsigned int ulFolderId, unsigned int ulObjType, unsigned int ulFlags, const ECLocale &locale, ECConvenientDepthObjectTable **lppTable)
{
	ECRESULT er = erSuccess;

	*lppTable = new ECConvenientDepthObjectTable(lpSession, ulStoreId, lpGuid, ulFolderId, ulObjType, ulFlags, locale);

	(*lppTable)->AddRef();

	return er;
}

ECRESULT ECConvenientDepthObjectTable::Load() {
	ECRESULT er = erSuccess;
	ECDatabase *lpDatabase = NULL;
	DB_RESULT 	lpDBResult = NULL;
	DB_ROW		lpDBRow = NULL;
	std::string	strQuery;
	ECODStore	*lpData = (ECODStore *)m_lpObjectData;
	sObjectTableKey		sRowItem;
	unsigned int ulDepth = 0;
	
	std::list<FOLDERINFO> lstFolders;	// The list of folders
	std::list<FOLDERINFO>::iterator iterFolders;

	std::map<unsigned int, SortKey> mapSortKey;	// map a known folder to its sortkey. This is used to derive a subfolder's sort key
	std::list<unsigned int> lstObjIds;

	unsigned int ulFlags = lpData->ulFlags;
	unsigned int ulFolderId = m_ulFolderId;

	FOLDERINFO sRoot;

	er = lpSession->GetDatabase(&lpDatabase);
	if (er != erSuccess)
		goto exit;

	sRoot.ulFolderId = ulFolderId;
	sRoot.strFolderName.clear();
	//sRoot.strSortKey = "root";
	
	lstFolders.push_back(sRoot);
	mapSortKey[ulFolderId] = sRoot.sortKey;

	iterFolders = lstFolders.begin();
	while (iterFolders != lstFolders.end()) {
		strQuery = "SELECT hierarchy.id, hierarchy.parent, hierarchy.owner, hierarchy.flags, hierarchy.type, properties.val_string FROM hierarchy LEFT JOIN properties ON properties.hierarchyid = hierarchy.id AND properties.tag = 12289  AND properties.type = 30 WHERE hierarchy.type = " +  stringify(MAPI_FOLDER) + " AND hierarchy.flags & "+stringify(MSGFLAG_DELETED)+" = " + stringify(ulFlags&MSGFLAG_DELETED);

		strQuery += " AND hierarchy.parent IN(";
		
		while(iterFolders != lstFolders.end()) {
		    strQuery += stringify(iterFolders->ulFolderId);
		    iterFolders++;
		    if(iterFolders != lstFolders.end())
    		    strQuery += ",";
        }
        
        strQuery += ")";
		
		er = lpDatabase->DoSelect(strQuery, &lpDBResult);
		if (er != erSuccess)
			goto exit;

		while (1) {
		    FOLDERINFO sFolderInfo;
		    
			lpDBRow = lpDatabase->FetchRow(lpDBResult);

			if (lpDBRow == NULL)
				break;

			if (lpDBRow[0] == NULL || lpDBRow[1] == NULL || lpDBRow[2] == NULL || lpDBRow[3] == NULL || lpDBRow[4] == NULL || lpDBRow[5] == NULL)
				continue;

            sFolderInfo.ulFolderId = atoui(lpDBRow[0]);
            sFolderInfo.strFolderName = lpDBRow[5];

			sFolderInfo.sortKey = mapSortKey[atoui(lpDBRow[1])];
			sFolderInfo.sortKey.push_back(createSortKeyFromUTF8(sFolderInfo.strFolderName.c_str(), 0, GetLocale()));
            
            mapSortKey[sFolderInfo.ulFolderId] = sFolderInfo.sortKey;
            
            // Since we have this information, give the cache manager the hierarchy information for this object
			lpSession->GetSessionManager()->GetCacheManager()->SetObject(atoui(lpDBRow[0]), atoui(lpDBRow[1]), atoui(lpDBRow[2]), atoui(lpDBRow[3]), atoui(lpDBRow[4]));

			if (lpSession->GetSecurity()->CheckPermission(sFolderInfo.ulFolderId, ecSecurityFolderVisible) != erSuccess) {
				continue;
			}
			
			// Push folders onto end of list
            lstFolders.push_back(sFolderInfo);
            
            // If we were pointing at the last item, point at the freshly inserted item
            if(iterFolders == lstFolders.end())
                iterFolders--;
		}

		if (lpDBResult) {
			lpDatabase->FreeResult(lpDBResult);
			lpDBResult = NULL;
		}
		
		// If you're insane enough to have more than 256 levels over folders than we cut it off here because this function's
		// memory usage goes up exponentially ..
		ulDepth++;
		if(ulDepth > 256)
		    break;
	}
	
	// Our lstFolders now contains all folders, and a sortkey. All we need to do is sort by that sortkey ...
	lstFolders.sort();
	
	// ... and put the data into the row system

	for(iterFolders = lstFolders.begin(); iterFolders != lstFolders.end(); iterFolders++) {
		if(iterFolders->ulFolderId == m_ulFolderId)
			continue;

		lstObjIds.push_back(iterFolders->ulFolderId);
    }
    
    LoadRows(&lstObjIds, 0);
    
exit:   
    if (lpDBResult)
        lpDatabase->FreeResult(lpDBResult);
    
    return er;
}

ECRESULT ECConvenientDepthObjectTable::GetComputedDepth(struct soap *soap, ECSession* lpSession, unsigned int ulObjId, struct propVal *lpPropVal){
	ECRESULT		er = erSuccess;
	unsigned int ulObjType;

	lpPropVal->ulPropTag = PR_DEPTH;
	lpPropVal->__union = SOAP_UNION_propValData_ul;
	lpPropVal->Value.ul = 0;

	while(ulObjId != m_ulFolderId && lpPropVal->Value.ul < 50){
		er = lpSession->GetSessionManager()->GetCacheManager()->GetObject(ulObjId, &ulObjId, NULL, NULL, &ulObjType);
		if(er != erSuccess) {
			// should never happen
			ASSERT(FALSE);
			er = ZARAFA_E_NOT_FOUND;
			goto exit;
		}
		if(ulObjType != MAPI_FOLDER){
			er = ZARAFA_E_NOT_FOUND;
			goto exit;
		}
		lpPropVal->Value.ul++;
	}
	
exit:
	return er;
}
