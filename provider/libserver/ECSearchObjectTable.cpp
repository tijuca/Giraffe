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

#include "ECSessionManager.h"
#include "ECSearchObjectTable.h"
#include "ECSession.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

ECSearchObjectTable::ECSearchObjectTable(ECSession *lpSession, unsigned int ulStoreId, LPGUID lpGuid, unsigned int ulFolderId, unsigned int ulObjType, unsigned int ulFlags, const ECLocale &locale) : ECStoreObjectTable(lpSession, ulStoreId, lpGuid, 0, ulObjType, ulFlags, locale) {
	// We don't pass ulFolderId to ECStoreObjectTable (see constructor above passing '0'), because 
	// it will assume that all rows are in that folder if we do that. But we still want to 
	// remember the folder ID for our own use.
	
	m_ulFolderId = ulFolderId;
	m_ulStoreId = ulStoreId;
}

ECRESULT ECSearchObjectTable::Create(ECSession *lpSession, unsigned int ulStoreId, GUID *lpGuid, unsigned int ulFolderId, unsigned int ulObjType, unsigned int ulFlags, const ECLocale &locale, ECSearchObjectTable **lppTable)
{
	ECRESULT er = erSuccess;

	*lppTable = new ECSearchObjectTable(lpSession, ulStoreId, lpGuid, ulFolderId, ulObjType, ulFlags, locale);

	(*lppTable)->AddRef();
	
	return er;
}

ECRESULT ECSearchObjectTable::Load() {
    ECRESULT er = erSuccess;
    sObjectTableKey		sRowItem;

    struct sortOrderArray*      lpsPrevSortOrderArray = NULL;
    struct sortOrderArray	sDefaultSortOrder = { 0, 0};
    unsigned int ulCount = 0;
    unsigned int *lpulObjIdList = NULL;
    
    unsigned int ulFolderId = m_ulFolderId;
    unsigned int ulStoreId = m_ulStoreId;

	pthread_mutex_lock(&m_hLock);

    if(ulFolderId) {
        if(lpsSortOrderArray) {
            // It is faster to switch to a default sort order, then load, and then re-sort
            lpsPrevSortOrderArray = new struct sortOrderArray;
            lpsPrevSortOrderArray->__size = lpsSortOrderArray->__size;
            if(lpsSortOrderArray->__size == 0 ) {
                lpsPrevSortOrderArray->__ptr = NULL;
            } else {
                lpsPrevSortOrderArray->__ptr = new sortOrder[lpsSortOrderArray->__size];
                memcpy(lpsPrevSortOrderArray->__ptr, lpsSortOrderArray->__ptr, sizeof(struct sortOrder) * lpsSortOrderArray->__size);
            }
        
            SetSortOrder(&sDefaultSortOrder, 0, 0);
        } else {
            lpsPrevSortOrderArray = NULL;
        }
        
        // Get the search results
        er = lpSession->GetSessionManager()->GetSearchFolders()->GetSearchResults(ulStoreId, ulFolderId, &ulCount, &lpulObjIdList);
        if(er != erSuccess)
            goto exit;

        // Add all objects to the table
        for(ULONG i=0; i<ulCount; i++) {
            UpdateRow(ECKeyTable::TABLE_ROW_ADD, lpulObjIdList[i], 0);
        }

        // Set sort order
        if(lpsPrevSortOrderArray)
            SetSortOrder(lpsPrevSortOrderArray, 0 ,0);

    }
    
exit:
	pthread_mutex_unlock(&m_hLock);

	if (lpulObjIdList)
		delete [] lpulObjIdList;

    if(lpsPrevSortOrderArray)
        FreeSortOrderArray(lpsPrevSortOrderArray);
        
    return er;
}
