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

/* Returns the rows for a contents- or hierarchytable
 *
 * objtype == MAPI_MESSAGE, then contents table
 * objtype == MAPI_MESSAGE, flags == MAPI_ASSOCIATED, then associated contents table
 * objtype == MAPI_FOLDER, then hierarchy table
 *
 * Tables are generated from SQL in the following way:
 *
 * Tables are constructed by joining the hierarchy table with the property table multiple
 * times, once for each requested property (column). Because each column of each row can always have
 * only one or zero records, a unique index is created on the property table, indexing (hierarchyid, type, tag).
 *
 * This means that for each cell that we request, the index needs to be accessed by the SQL
 * engine only once, which makes the actual query extremely fast.
 *
 * In tests, this has shown to required around 60ms for 30 rows and 10 columns from a table of 10000
 * rows. Also, this is a O(n log n) operation and therefore not prone to large scaling problems. (Yay!)
 * (with respect to the amount of columns, it is O(n), but that's quite constant, and so is the
 * actual amount of rows requested per query (also O(n)).
 *
 */

#include "soapH.h"
#include "ZarafaCode.h"

#include <mapidefs.h>
#include <mapitags.h>

#include "Zarafa.h"
#include "ECDatabaseUtils.h"
#include "ECKeyTable.h"
#include "ECGenProps.h"
#include "ECStoreObjectTable.h"
#include "ECStatsCollector.h"
#include "ECSecurity.h"
#include "ECIndexer.h"
#include "ECSearchClient.h"
#include "ECTPropsPurge.h"
#include "SOAPUtils.h"
#include "stringutil.h"
#include <charset/utf8string.h>
#include <charset/convert.h>

#include "Trace.h"
#include "ECSessionManager.h"

#include "ECSession.h"

#include <map>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

extern ECStatsCollector*  g_lpStatsCollector;

bool IsTruncatableType(unsigned int ulTag)
{
    switch(PROP_TYPE(ulTag)) {
        case PT_STRING8:
        case PT_UNICODE:
        case PT_BINARY:
            return true;
        default:
            return false;
    }
    
    return false;
}

bool IsTruncated(struct propVal *lpsPropVal)
{
	if(!IsTruncatableType(lpsPropVal->ulPropTag))
		return false;
		
	switch(PROP_TYPE(lpsPropVal->ulPropTag)) {
		case PT_STRING8:
		case PT_UNICODE:
			return u8_len(lpsPropVal->Value.lpszA) == TABLE_CAP_STRING;
		case PT_BINARY:
			// previously we capped on 255 bytes, upgraded to 511.
			return lpsPropVal->Value.bin->__size == TABLE_CAP_STRING || lpsPropVal->Value.bin->__size == TABLE_CAP_BINARY;
	}
	
	return false;		
}

ECStoreObjectTable::ECStoreObjectTable(ECSession *lpSession, unsigned int ulStoreId, GUID *lpGuid, unsigned int ulFolderId,unsigned int ulObjType, unsigned int ulFlags, const ECLocale &locale) : ECGenericObjectTable(lpSession, ulObjType, ulFlags, locale)
{
	ECODStore* lpODStore = new ECODStore;

	lpODStore->ulStoreId = ulStoreId;
	lpODStore->ulFolderId = ulFolderId;
	lpODStore->ulObjType = ulObjType;
	lpODStore->ulFlags = ulFlags;

	if(lpGuid) {
		lpODStore->lpGuid = new GUID;
		*lpODStore->lpGuid = *lpGuid;
	} else {
		lpODStore->lpGuid = NULL; // When NULL is specified, we get the store ID & guid for each seperate row
	}

	// Set dataobject
	m_lpObjectData = lpODStore;

	// Set callback function for queryrowdata
	m_lpfnQueryRowData = QueryRowData;
	
	ulPermission = 0;
	fPermissionRead = false;
}

ECStoreObjectTable::~ECStoreObjectTable()
{
	if(m_lpObjectData) {
		ECODStore* lpODStore = (ECODStore*)m_lpObjectData;
		if(lpODStore->lpGuid)
			delete lpODStore->lpGuid;

		delete lpODStore;
	}
}

ECRESULT ECStoreObjectTable::Create(ECSession *lpSession, unsigned int ulStoreId, GUID *lpGuid, unsigned int ulFolderId, unsigned int ulObjType, unsigned int ulFlags, const ECLocale &locale, ECStoreObjectTable **lppTable)
{
	ECRESULT er = erSuccess;

	*lppTable = new ECStoreObjectTable(lpSession, ulStoreId, lpGuid, ulFolderId, ulObjType, ulFlags, locale);

	(*lppTable)->AddRef();

	return er;
}

ECRESULT ECStoreObjectTable::GetColumnsAll(ECListInt* lplstProps)
{
	ECRESULT		er = erSuccess;
	DB_RESULT		lpDBResult = NULL;
	DB_ROW			lpDBRow = NULL;
	std::string		strQuery;
	ECDatabase*		lpDatabase = NULL;
	ECODStore*		lpODStore = (ECODStore*)m_lpObjectData;
	ULONG			ulPropID = 0;
	ECObjectTableMap::iterator		iterObjects;

	pthread_mutex_lock(&m_hLock);

	ASSERT(lplstProps != NULL);

	er = lpSession->GetDatabase(&lpDatabase);
	if (er != erSuccess)
		goto exit;
	
	//List always emtpy
	lplstProps->clear();

	if(!mapObjects.empty() && lpODStore->ulFolderId)
	{
		// Properties
		strQuery = "SELECT DISTINCT tproperties.tag, tproperties.type FROM tproperties WHERE folderid = " + stringify(lpODStore->ulFolderId);

		er = lpDatabase->DoSelect(strQuery, &lpDBResult);
		if(er != erSuccess)
			goto exit;
		
		// Put the results into a STL list
		while((lpDBRow = lpDatabase->FetchRow(lpDBResult)) != NULL) {

			if(lpDBRow == NULL || lpDBRow[0] == NULL || lpDBRow[1] == NULL)
				continue;

			ulPropID = atoi(lpDBRow[0]);

			lplstProps->push_back(PROP_TAG(atoi(lpDBRow[1]), ulPropID));
		}
	}// if(!mapObjects.empty())

	// Add some generated and standard properties
	lplstProps->push_back(PR_ENTRYID);
	lplstProps->push_back(PR_INSTANCE_KEY);
	lplstProps->push_back(PR_RECORD_KEY);
	lplstProps->push_back(PR_OBJECT_TYPE);
	lplstProps->push_back(PR_LAST_MODIFICATION_TIME);
	lplstProps->push_back(PR_CREATION_TIME);
	lplstProps->push_back(PR_PARENT_ENTRYID);
	lplstProps->push_back(PR_MAPPING_SIGNATURE);

	// Add properties only in the contents tables
	if(lpODStore->ulObjType == MAPI_MESSAGE && (lpODStore->ulFlags&MSGFLAG_ASSOCIATED) == 0)
	{
		lplstProps->push_back(PR_MSG_STATUS);
	}
	
	lplstProps->push_back(PR_ACCESS_LEVEL);

	//FIXME: only in folder or message table	
	lplstProps->push_back(PR_ACCESS);

exit:
	pthread_mutex_unlock(&m_hLock);

	if(lpDBResult)
		lpDatabase->FreeResult(lpDBResult);

	return er;
}

ECRESULT ECStoreObjectTable::ReloadTableMVData(ECObjectTableList* lplistRows, ECListInt* lplistMVPropTag)
{
	ECRESULT			er = erSuccess;
	DB_RESULT			lpDBResult = NULL;
	DB_ROW				lpDBRow = NULL;
	std::string			strQuery;
	std::string			strColName;
	ECDatabase*			lpDatabase = NULL;
	int					j;
	ECListIntIterator	iterListMVPropTag;
	sObjectTableKey		sRowItem;
	size_t				cListRows = 0;

	ECObjectTableList::iterator	iterListRows;

	er = lpSession->GetDatabase(&lpDatabase);
	if (er != erSuccess)
		goto exit;

	ASSERT(lplistMVPropTag->size() <2); //FIXME: Limit of one 1 MV column

	// scan for MV-props and add rows
	strQuery = "SELECT h.id, orderid FROM hierarchy as h";
	j = 0;
	for(iterListMVPropTag = lplistMVPropTag->begin(); iterListMVPropTag != lplistMVPropTag->end(); iterListMVPropTag++)
	{
		strColName = "col"+stringify(j);
		strQuery += " LEFT JOIN mvproperties as "+strColName+" ON h.id="+strColName+".hierarchyid AND "+strColName+".tag="+stringify(PROP_ID(*iterListMVPropTag))+" AND "+strColName+".type="+stringify(PROP_TYPE(NormalizeDBPropTag(*iterListMVPropTag) &~MV_INSTANCE));
		j++;
		break; //FIXME: limit 1 column, multi MV cols
	}//for(iterListMVPropTag
		
	strQuery += " WHERE h.id IN(";

	j = 0;
	cListRows = lplistRows->size();
	for(iterListRows = lplistRows->begin(); iterListRows != lplistRows->end(); iterListRows++)
	{
		strQuery += stringify(iterListRows->ulObjId);

		if(j != (int)(cListRows-1))
			strQuery += ",";
		j++;
	}
	strQuery += ") ORDER by id, orderid";

	er = lpDatabase->DoSelect(strQuery, &lpDBResult);
	if(er != erSuccess)
		goto exit;

	while(1)
	{
		lpDBRow = lpDatabase->FetchRow(lpDBResult);
		if(lpDBRow == NULL)
			break;

		sRowItem.ulObjId = atoui(lpDBRow[0]);
		sRowItem.ulOrderId = (lpDBRow[1] == NULL)? 0 : atoi(lpDBRow[1]);

		if(sRowItem.ulOrderId > 0)
			lplistRows->push_back(sRowItem);
	}

exit:
	if(lpDBResult)
		lpDatabase->FreeResult(lpDBResult);

	return er;
}

// Interface to main row engine (bSubObjects is false)
ECRESULT ECStoreObjectTable::QueryRowData(ECGenericObjectTable *lpThis, struct soap *soap, ECSession *lpSession, ECObjectTableList* lpRowList, struct propTagArray *lpsPropTagArray, void* lpObjectData, struct rowSet **lppRowSet, bool bCacheTableData, bool bTableLimit)
{
	return ECStoreObjectTable::QueryRowData(lpThis, soap, lpSession, lpRowList, lpsPropTagArray, lpObjectData, lppRowSet, bCacheTableData, bTableLimit, false);
}

// Direct interface
ECRESULT ECStoreObjectTable::QueryRowData(ECGenericObjectTable *lpThis, struct soap *soap, ECSession *lpSession, ECObjectTableList* lpRowList, struct propTagArray *lpsPropTagArray, void* lpObjectData, struct rowSet **lppRowSet, bool bCacheTableData, bool bTableLimit, bool bSubObjects)
{
	ECRESULT		er = erSuccess;
	int	i = 0;
	int	k = 0;
	unsigned int	ulFolderId;
	unsigned int 	ulRowStoreId = 0;
	GUID			sRowGuid;

	struct rowSet	*lpsRowSet = NULL;

	ECODStore*		lpODStore = (ECODStore*)lpObjectData;

	ECDatabase		*lpDatabase = NULL;

	std::map<unsigned int, std::map<sObjectTableKey, unsigned int> > mapStoreIdObjIds;
	std::map<unsigned int, std::map<sObjectTableKey, unsigned int> >::iterator iterStoreIdObjIds;
	std::map<sObjectTableKey, unsigned int> mapIncompleteRows;
	std::map<sObjectTableKey, unsigned int>::iterator iterIncomplete;
	std::map<sObjectTableKey, unsigned int> mapRows;
	std::map<sObjectTableKey, unsigned int>::iterator iterRows;
	std::multimap<unsigned int, unsigned int> mapColumns;
	std::list<unsigned int> lstDeferred;
	std::list<unsigned int>::iterator iterDeferred;
	std::set<unsigned int> setColumnIDs;
	std::set<unsigned int>::iterator iterColumnIDs;
	ECObjectTableList lstRowOrder;
	ECObjectTableMap::iterator		iterIDs, iterIDsErase;
	ECObjectTableList::iterator		iterRowList;
	sObjectTableKey					sMapKey;

	ECListInt			listMVSortCols;//Other mvprops then normal column set
	ECListInt::iterator	iterListInt;
	ECListInt			listMVIColIds;
	string				strCol;
    sObjectTableKey sKey;
	
	std::set<std::pair<unsigned int, unsigned int> > setCellDone;

	std::list<unsigned int> propList;

	ASSERT(lpRowList != NULL);

	er = lpSession->GetDatabase(&lpDatabase);
	if (er != erSuccess)
		goto exit;

	lpsRowSet = s_alloc<rowSet>(soap);
	lpsRowSet->__size = 0;
	lpsRowSet->__ptr = NULL;

	if(lpRowList->empty()) {
		*lppRowSet = lpsRowSet;
		lpsRowSet = NULL;
		goto exit; // success
	}

    if (lpODStore->ulFlags & EC_TABLE_NOCAP)
            bTableLimit = false;
	
	// We return a square array with all the values
	lpsRowSet->__size = lpRowList->size();
	lpsRowSet->__ptr = s_alloc<propValArray>(soap, lpsRowSet->__size);
	memset(lpsRowSet->__ptr, 0, sizeof(propValArray) * lpsRowSet->__size);

	// Allocate memory for all rows
	for(i=0; i<lpsRowSet->__size; i++) {
		lpsRowSet->__ptr[i].__size = lpsPropTagArray->__size;
		lpsRowSet->__ptr[i].__ptr = s_alloc<propVal>(soap, lpsPropTagArray->__size);
		memset(lpsRowSet->__ptr[i].__ptr, 0, sizeof(propVal) * lpsPropTagArray->__size);
	}

	// Scan cache for anything that we can find, and generate any properties that don't come from normal database queries.
	for(i=0, iterRowList = lpRowList->begin(); iterRowList != lpRowList->end(); iterRowList++, i++)
	{
	    bool bRowComplete = true;
	    
	    for(k=0; k < lpsPropTagArray->__size; k++) {
	    	unsigned int ulPropTag;
	    	
	    	if(iterRowList->ulObjId == 0 || ECGenProps::GetPropSubstitute(lpODStore->ulObjType, lpsPropTagArray->__ptr[k], &ulPropTag) != erSuccess)
	    		ulPropTag = lpsPropTagArray->__ptr[k];
	    	
            // Get StoreId if needed
            if(lpODStore->lpGuid == NULL) {
                // No store specified, so determine the store ID & guid from the object id
                lpSession->GetSessionManager()->GetCacheManager()->GetStore(iterRowList->ulObjId, &ulRowStoreId, &sRowGuid);
            } else {
                ulRowStoreId = lpODStore->ulStoreId;
            }

            // Handle category header rows
            if(iterRowList->ulObjId == 0) {
            	if(lpThis->GetPropCategory(soap, lpsPropTagArray->__ptr[k], *iterRowList, &lpsRowSet->__ptr[i].__ptr[k]) != erSuccess) {
            		// Other properties are not found
            		lpsRowSet->__ptr[i].__ptr[k].__union = SOAP_UNION_propValData_ul;
            		lpsRowSet->__ptr[i].__ptr[k].Value.ul = ZARAFA_E_NOT_FOUND;
            		lpsRowSet->__ptr[i].__ptr[k].ulPropTag = PROP_TAG(PT_ERROR, PROP_ID(ulPropTag));
            	}
    	        setCellDone.insert(std::make_pair(i,k));
            	continue;
            }

			if (ECGenProps::IsPropComputedUncached(ulPropTag, lpODStore->ulObjType) == erSuccess) {
				if (ECGenProps::GetPropComputedUncached(soap, lpSession, ulPropTag, iterRowList->ulObjId, iterRowList->ulOrderId, ulRowStoreId, lpODStore->ulFolderId, lpODStore->ulObjType, &lpsRowSet->__ptr[i].__ptr[k]) != erSuccess)
					CopyEmptyCellToSOAPPropVal(soap, ulPropTag, &lpsRowSet->__ptr[i].__ptr[k]);
				setCellDone.insert(std::make_pair(i,k));
				continue;
			}

			// Handle PR_DEPTH
			if(ulPropTag == PR_DEPTH) {
				if(!lpThis || lpThis->GetComputedDepth(soap, lpSession, iterRowList->ulObjId, &lpsRowSet->__ptr[i].__ptr[k]) != erSuccess) { 
					lpsRowSet->__ptr[i].__ptr[k].__union = SOAP_UNION_propValData_ul;
					lpsRowSet->__ptr[i].__ptr[k].ulPropTag = PR_DEPTH;
					lpsRowSet->__ptr[i].__ptr[k].Value.ul = 0;
				}
				setCellDone.insert(std::make_pair(i,k));
				continue;
			}
			
    	    if(ulPropTag == PR_PARENT_DISPLAY_A || ulPropTag == PR_PARENT_DISPLAY_W || ulPropTag == PR_EC_OUTGOING_FLAGS) {
    	    	bRowComplete = false;
    	    	continue; // These are not in cache, even if cache is complete for an item.
			}
			
			if(PROP_TYPE(ulPropTag) & MV_FLAG) {
				// Currently not caching MV values
				bRowComplete = false;
				continue;
			}
    	    
    	    // FIXME bComputed always false
    	    // FIXME optimisation possible to GetCell: much more efficient to get all cells in one row at once
	        if(lpSession->GetSessionManager()->GetCacheManager()->GetCell(&(*iterRowList), ulPropTag, &lpsRowSet->__ptr[i].__ptr[k], soap, false) == erSuccess) {
	            setCellDone.insert(std::make_pair(i,k));
	            continue;
			}

			// None of the property generation tactics helped us, we need to get at least something from the database for this row.
            bRowComplete = false;
        }
        
        // Remember if we didn't have all cells in this row in the cache, and remember the row number
        if(!bRowComplete)
	        mapIncompleteRows[*iterRowList] = i;
	}
	
	if(!bSubObjects) {
		// Query from contents or hierarchy table, only do row-order processing for rows that have to be done via the row engine
		
		if(!mapIncompleteRows.empty()) {
			// Find rows that are in the deferred processing queue, and we have rows to be processed
			if(lpODStore->ulFolderId) {
				er = GetDeferredTableUpdates(lpDatabase, lpODStore->ulFolderId, &lstDeferred);
				if(er != erSuccess)
					goto exit;
			} else {
				er = GetDeferredTableUpdates(lpDatabase, lpRowList, &lstDeferred);
				if(er != erSuccess)
					goto exit;
			}
				
			// Build list of rows that are incomplete (not in cache) AND deferred
			for(iterDeferred = lstDeferred.begin(); iterDeferred != lstDeferred.end(); iterDeferred++) {
				sKey.ulObjId = *iterDeferred;
				sKey.ulOrderId = 0;
				iterIncomplete = mapIncompleteRows.lower_bound(sKey);
				while(iterIncomplete != mapIncompleteRows.end() && iterIncomplete->first.ulObjId == *iterDeferred) {
					g_lpStatsCollector->Increment(SCN_DATABASE_DEFERRED_FETCHES);
					mapRows[iterIncomplete->first] = iterIncomplete->second;
					iterIncomplete++;
				}
			}
		}
    } else {
		// Do row-order query for all incomplete rows 
        mapRows = mapIncompleteRows;
    }
        
    if(!mapRows.empty()) {
        // Get rows from row order engine
        for(iterRows = mapRows.begin(); iterRows != mapRows.end(); iterRows++) {
            mapColumns.clear();

            // Find out which columns we need
            for(k=0; k < lpsPropTagArray->__size; k++) {
                if(setCellDone.count(std::make_pair(iterRows->second, k)) == 0) {
                    // Not done yet, remember that we need to get this column
                    unsigned int ulPropTag;
                    
                    if(ECGenProps::GetPropSubstitute(lpODStore->ulObjType, lpsPropTagArray->__ptr[k], &ulPropTag) != erSuccess)
                    	ulPropTag = lpsPropTagArray->__ptr[k];
                    	
                    mapColumns.insert(std::make_pair(ulPropTag, k));
                    setCellDone.insert(std::make_pair(iterRows->second, k)); // Done now
                }
            }
            
            // Get actual data
            er = QueryRowDataByRow(lpThis, soap, lpSession, (sObjectTableKey&)iterRows->first, iterRows->second, mapColumns, bTableLimit, lpsRowSet);
            if(er != erSuccess)
                goto exit;
        }
    }

    if(setCellDone.size() != (unsigned int)i*k) {
        // Some cells are not done yet, do them in column-order.
       	mapStoreIdObjIds.clear();

        // Split requests into same-folder blocks
        for(k=0; k < lpsPropTagArray->__size; k++) {
        	
            for(i=0, iterRowList = lpRowList->begin(); iterRowList != lpRowList->end(); iterRowList++, i++) {
            	if(setCellDone.count(std::make_pair(i,k)) != 0)
            		continue; // already done
            	
            	// Remember that there was some data int this column that we need to get
				setColumnIDs.insert(k);
				
                if(lpODStore->ulFolderId == 0) {
                    // Get parent folder
                    if(lpSession->GetSessionManager()->GetCacheManager()->GetParent(iterRowList->ulObjId, &ulFolderId) != erSuccess)
                    	/* This will cause the request to fail, since no items are in folder id 0. However, this is what we want since
                    	 * the only thing we can do is return NOT_FOUND for each cell.*/
                    	ulFolderId = 0;
                } else {
                	ulFolderId = lpODStore->ulFolderId;
                }
                mapStoreIdObjIds[ulFolderId][*iterRowList] = i;
                setCellDone.insert(std::make_pair(i,k));
            }
            
        }

        // FIXME could fill setColumns per set of items instead of the whole table request
        
        mapColumns.clear();
        // Convert the set of column IDs to a map from property tag to column ID
        for(iterColumnIDs = setColumnIDs.begin(); iterColumnIDs != setColumnIDs.end(); iterColumnIDs++) {
        	unsigned int ulPropTag;
        	
        	if(ECGenProps::GetPropSubstitute(lpODStore->ulObjType, lpsPropTagArray->__ptr[*iterColumnIDs], &ulPropTag) != erSuccess)
        		ulPropTag = lpsPropTagArray->__ptr[*iterColumnIDs];
        		
        	mapColumns.insert(std::make_pair(ulPropTag, *iterColumnIDs));
        }
		for(iterStoreIdObjIds = mapStoreIdObjIds.begin(); iterStoreIdObjIds != mapStoreIdObjIds.end(); iterStoreIdObjIds++) {
			er = QueryRowDataByColumn(lpThis, soap, lpSession, mapColumns, iterStoreIdObjIds->first, iterStoreIdObjIds->second, lpsRowSet);
		}
    }
    
    if(!bTableLimit) {
    	/* If no table limit was specified (so entire string requested, not just < 255 bytes), we have to do some more processing:
    	 *
    	 * - Check each column to see if it is truncatable at all (only string and binary columns are truncatable)
    	 * - Check each output value that we have already retrieved to see if it was truncated (value may have come from cache or column engine)
    	 * - Get any additional data via QueryRowDataByRow() if needed since that is the only method to get > 255 bytes
    	 */
		for(k=0; k < lpsPropTagArray->__size; k++) {
			if(IsTruncatableType(lpsPropTagArray->__ptr[k])) {
				for(i=0, iterRowList = lpRowList->begin(); iterRowList != lpRowList->end(); iterRowList++, i++) {
					if(IsTruncated(&lpsRowSet->__ptr[i].__ptr[k])) {
						
						// We only want one column in this row
						mapColumns.clear();
						mapColumns.insert(std::make_pair(lpsPropTagArray->__ptr[k], k));
						
						// Un-truncate this value
						er = QueryRowDataByRow(lpThis, soap, lpSession, *iterRowList, i, mapColumns, false, lpsRowSet);
						if (er != erSuccess)
							goto exit;
					}
				}
			}
		}
    }
    
    for(k=0; k < lpsPropTagArray->__size; k++) {
        // Do any post-processing operations
        if(ECGenProps::IsPropComputed(lpsPropTagArray->__ptr[k], lpODStore->ulObjType) == erSuccess) {
            for(i=0, iterRowList = lpRowList->begin(); iterRowList != lpRowList->end(); iterRowList++, i++) {
				if (iterRowList->ulObjId != 0) // Do not change category values!
	                ECGenProps::GetPropComputed(soap, lpODStore->ulObjType, lpsPropTagArray->__ptr[k], iterRowList->ulObjId, &lpsRowSet->__ptr[i].__ptr[k]);
            }
        }
    }
    
    *lppRowSet = lpsRowSet;
	lpsRowSet = NULL;

exit:
	if (soap == NULL && lpsRowSet)
		FreeRowSet(lpsRowSet, true);
		
    return er;
}

ECRESULT ECStoreObjectTable::QueryRowDataByRow(ECGenericObjectTable *lpThis, struct soap *soap, ECSession *lpSession, sObjectTableKey &sKey, unsigned int ulRowNum, std::multimap<unsigned int, unsigned int> &mapColumns, bool bTableLimit, struct rowSet *lpsRowSet)
{
	ECRESULT		er = erSuccess;
	DB_RESULT		lpDBResult = NULL;
	DB_ROW			lpDBRow = NULL;
	DB_LENGTHS		lpDBLen = NULL;
	std::string		strQuery;
	std::string		strSubquery;

	ECDatabase		*lpDatabase = NULL;

	bool			bNeedProps = false;
	bool			bNeedMVProps = false;
	bool			bNeedMVIProps = false;
	bool			bNeedSubQueries = false;
	
    std::string strSubQuery;
	std::string strCol;

    std::set<unsigned int> setSubQueries;
	std::multimap<unsigned int, unsigned int>::iterator iterColumns;
	std::multimap<unsigned int, unsigned int>::iterator iterDelete;

	// Select correct property column query according to whether we want to truncate or not
	std::string strPropColOrder = bTableLimit ? PROPCOLORDER_TRUNCATED : PROPCOLORDER;
	std::string strMVIPropColOrder = bTableLimit ? MVIPROPCOLORDER_TRUNCATED : MVIPROPCOLORDER;
	
	ASSERT(lpsRowSet != NULL);

	er = lpSession->GetDatabase(&lpDatabase);
	if (er != erSuccess)
		goto exit;
	
	g_lpStatsCollector->Increment(SCN_DATABASE_ROW_READS, 1);

    for(iterColumns = mapColumns.begin(); iterColumns != mapColumns.end(); iterColumns++) {
        unsigned int ulPropTag = iterColumns->first;
        
        // Remember the types of columns being requested for later use
        if(ECGenProps::GetPropSubquery(ulPropTag, strSubQuery) == erSuccess) {
            setSubQueries.insert(ulPropTag);
            bNeedSubQueries = true;
        } else if(ulPropTag & MV_FLAG) {
            if(ulPropTag & MV_INSTANCE)
                bNeedMVIProps = true;
            else
                bNeedMVProps = true;
        } else {
            bNeedProps = true;
        }
    }
    
    // Do normal properties
    if(bNeedProps) {
        // mapColumns now contains the data that we still need to request from the database for this row.
        strQuery = "SELECT " + strPropColOrder + " FROM properties WHERE hierarchyid="+stringify(sKey.ulObjId) + " AND properties.tag IN (";
        
        for(iterColumns = mapColumns.begin(); iterColumns != mapColumns.end(); iterColumns++) {
            // A subquery is defined for this type, we don't have to get the data in the normal way.
            if(setSubQueries.find(iterColumns->first) != setSubQueries.end())
                continue;
            if((iterColumns->first & MV_FLAG) == 0) {
                strQuery += stringify(PROP_ID(iterColumns->first));
                strQuery += ",";
            }
        }
        
        // Remove trailing ,
        strQuery.resize(strQuery.size()-1);
        strQuery += ")";
    }
    
    // Do MV properties
    if(bNeedMVProps) {
        if(!strQuery.empty())
            strQuery += " UNION ";
            
        strQuery += "SELECT " + (std::string)MVPROPCOLORDER + " FROM mvproperties WHERE hierarchyid="+stringify(sKey.ulObjId) + " AND mvproperties.tag IN (";
        
        for(iterColumns = mapColumns.begin(); iterColumns != mapColumns.end(); iterColumns++) {
            if(setSubQueries.find(iterColumns->first) != setSubQueries.end())
                continue;
            if((iterColumns->first & MV_FLAG) && !(iterColumns->first & MV_INSTANCE)) {
                strQuery += stringify(PROP_ID(iterColumns->first));
                strQuery += ",";
            }
        }
        
        strQuery.resize(strQuery.size()-1);
        strQuery += ")";
        strQuery += "  GROUP BY hierarchyid, tag";
    }
    
    // Do MVI properties. Output from this part is handled exactly the same as normal properties
    if(bNeedMVIProps) {
        if(!strQuery.empty())
            strQuery += " UNION ";
            
        strQuery += "SELECT " + strMVIPropColOrder + " FROM mvproperties WHERE hierarchyid="+stringify(sKey.ulObjId);
        
        for(iterColumns = mapColumns.begin(); iterColumns != mapColumns.end(); iterColumns++) {
            if(setSubQueries.find(iterColumns->first) != setSubQueries.end())
                continue;
            if((iterColumns->first & MV_FLAG) && (iterColumns->first & MV_INSTANCE)) {
                strQuery += " AND (";
                strQuery += "orderid = " + stringify(sKey.ulOrderId) + " AND ";
                strQuery += "tag = " + stringify(PROP_ID(iterColumns->first));
                strQuery += ")";
            }
        }
    }

    // Do generated properties        
    if(bNeedSubQueries) {
        std::string strPropColOrder;
        
        for(iterColumns = mapColumns.begin(); iterColumns != mapColumns.end(); iterColumns++) {
            if(ECGenProps::GetPropSubquery(iterColumns->first, strSubQuery) == erSuccess) {
                strPropColOrder = GetPropColOrder(iterColumns->first, strSubQuery);

                if(!strQuery.empty())
                    strQuery += " UNION ";
                    
                strQuery += " SELECT " + strPropColOrder + " FROM hierarchy WHERE hierarchy.id = " + stringify(sKey.ulObjId);
            
            }
        }
    }
    
    if(!strQuery.empty()) {    
        er = lpDatabase->DoSelect(strQuery, &lpDBResult);
        if(er != erSuccess)
            goto exit;
            
        while((lpDBRow = lpDatabase->FetchRow(lpDBResult)) != 0) {
            if(lpDBRow[1] == NULL || lpDBRow[2] == NULL) {
                ASSERT(false);
                continue;
            }
            
            lpDBLen = lpDatabase->FetchRowLengths(lpDBResult);
                
            unsigned int ulPropTag = PROP_TAG(atoui(lpDBRow[2]), atoui(lpDBRow[1]));
            
            // The same column may have been requested multiple times. If that is the case, SQL will give us one result for all columns. This
            // means we have to loop through all the same-property columns and add the same data everywhere.
            for(iterColumns = mapColumns.lower_bound(NormalizeDBPropTag(ulPropTag)); iterColumns != mapColumns.end() && CompareDBPropTag(iterColumns->first, ulPropTag); ) {

				// free prop if we're not allocing by soap
				if(soap == NULL && lpsRowSet->__ptr[ulRowNum].__ptr[iterColumns->second].ulPropTag != 0) {
					FreePropVal(&lpsRowSet->__ptr[ulRowNum].__ptr[iterColumns->second], false);
					memset(&lpsRowSet->__ptr[ulRowNum].__ptr[iterColumns->second], 0, sizeof(lpsRowSet->__ptr[ulRowNum].__ptr[iterColumns->second]));
				}


                if(CopyDatabasePropValToSOAPPropVal(soap, lpDBRow, lpDBLen, &lpsRowSet->__ptr[ulRowNum].__ptr[iterColumns->second]) != erSuccess) {
                	// This can happen if a subquery returned a NULL field or if your database contains bad data (eg a NULL field where there shouldn't be)
                    iterColumns++;
                    continue;
                }
                
                // Update property tag to requested property tag; requested type may have been PT_UNICODE while database contains PT_STRING8
                lpsRowSet->__ptr[ulRowNum].__ptr[iterColumns->second].ulPropTag = iterColumns->first;

                if ((lpsRowSet->__ptr[ulRowNum].__ptr[iterColumns->second].ulPropTag & MV_FLAG) == 0) {
					// Cache value
					lpSession->GetSessionManager()->GetCacheManager()->SetCell(&sKey, iterColumns->first, &lpsRowSet->__ptr[ulRowNum].__ptr[iterColumns->second]);
				} else if ((lpsRowSet->__ptr[ulRowNum].__ptr[iterColumns->second].ulPropTag & MVI_FLAG) == MVI_FLAG) {
					// Get rid of the MVI_FLAG
					lpsRowSet->__ptr[ulRowNum].__ptr[iterColumns->second].ulPropTag &= ~MVI_FLAG;
				}

                // Remove from mapColumns so we know that we got a response from SQL
                mapColumns.erase(iterColumns++);
            }
        }
        
        if(lpDBResult) lpDatabase->FreeResult(lpDBResult);
        lpDBResult = NULL;
    }


    for(iterColumns = mapColumns.begin(); iterColumns != mapColumns.end(); iterColumns++) {
    	ASSERT(lpsRowSet->__ptr[ulRowNum].__ptr[iterColumns->second].ulPropTag == 0);
		CopyEmptyCellToSOAPPropVal(soap, iterColumns->first, &lpsRowSet->__ptr[ulRowNum].__ptr[iterColumns->second]);
		lpSession->GetSessionManager()->GetCacheManager()->SetCell(&sKey, iterColumns->first, &lpsRowSet->__ptr[ulRowNum].__ptr[iterColumns->second]);
	}

	er = erSuccess;

exit:

	if(lpDBResult)
		lpDatabase->FreeResult(lpDBResult);

	return er;
}

/**
 * Read rows from tproperties table
 *
 * Since data in tproperties is truncated, this engine can only provide truncated property values.
 *
 *
 * @param[in] lpThis Pointer to main table object, optional (needed for categorization)
 * @param[in] soap Soap object to use for memory allocations, may be NULL for malloc() allocations
 * @param[in] lpSession Pointer to session for security context
 * @param[in] mapColumns Map of columns to retrieve with key = ulPropTag, value = column number
 * @param[in] ulFolderId Folder ID in which the rows exist
 * @param[in] mapObjIds Map of objects to retrieve with key = sObjectTableKey, value = row number
 * @param[out] lpsRowSet Row set where data will be written. Must be pre-allocated to hold all columns and rows requested
 */
ECRESULT ECStoreObjectTable::QueryRowDataByColumn(ECGenericObjectTable *lpThis, struct soap *soap, ECSession *lpSession, std::multimap<unsigned int, unsigned int> &mapColumns, unsigned int ulFolderId, std::map<sObjectTableKey, unsigned int> &mapObjIds, struct rowSet *lpsRowSet)
{
    ECRESULT er = erSuccess;
    std::string strQuery;
    std::string strHierarchyIds;
    std::string strTags, strMVTags, strMVITags;
    std::multimap<unsigned int, unsigned int>::iterator iterColumns;
    std::map<sObjectTableKey, unsigned int>::iterator iterObjIds;
    std::set<std::pair<unsigned int, unsigned int> > setDone;
    sObjectTableKey key;
    DB_RESULT lpDBResult = NULL;
    DB_ROW lpDBRow = NULL;
    DB_LENGTHS lpDBLen = NULL;
    unsigned int ulTag = 0;
    unsigned int ulType = 0;
    ULONG ulMin, ulMax;
    ECDatabase      *lpDatabase = NULL;
    std::set<unsigned int> setSubQueries;
    std::set<unsigned int>::iterator iterSubQueries;
    std::string		strSubquery;
    std::string		strPropColOrder;


    if(mapColumns.empty() || mapObjIds.empty())
    	goto exit;

	er = lpSession->GetDatabase(&lpDatabase);
	if (er != erSuccess)
		goto exit;

	ulMin = ulMax = PROP_ID(mapColumns.begin()->first);

    // Split columns into MV columns and non-MV columns
    for(iterColumns = mapColumns.begin(); iterColumns != mapColumns.end(); iterColumns++) {
    	if((iterColumns->first & MVI_FLAG) == MVI_FLAG) {
    		if(!strMVITags.empty())
    			strMVITags += ",";
    		strMVITags += stringify(PROP_ID(iterColumns->first));
    	}
    	if((iterColumns->first & MVI_FLAG) == MV_FLAG) {
    		if(!strMVTags.empty())
    			strMVTags += ",";
    		strMVTags += stringify(PROP_ID(iterColumns->first));
    	}
    	if((iterColumns->first & MVI_FLAG) == 0) {
    		if(ECGenProps::GetPropSubquery(iterColumns->first, strSubquery) == erSuccess) {
    			setSubQueries.insert(iterColumns->first);
			} else {
	    		if(!strTags.empty())
    				strTags += ",";
    			strTags += stringify(PROP_ID(iterColumns->first));
			}
    	}
    	
    	ulMin = min(ulMin, PROP_ID(iterColumns->first));
    	ulMax = max(ulMax, PROP_ID(iterColumns->first));
    }
    
    for(iterObjIds = mapObjIds.begin(); iterObjIds != mapObjIds.end(); iterObjIds++) {
    	if(!strHierarchyIds.empty())
    		strHierarchyIds += ",";
    	strHierarchyIds += stringify(iterObjIds->first.ulObjId);
    }

	// Get data
	if(!strTags.empty()) {
		strQuery = "SELECT " PROPCOLORDER ", hierarchyid, 0 FROM tproperties AS properties FORCE INDEX(PRIMARY) WHERE folderid=" + stringify(ulFolderId) + " AND hierarchyid IN(" + strHierarchyIds + ") AND tag IN (" + strTags +") AND tag >= " + stringify(ulMin) + " AND tag <= " + stringify(ulMax);
	}
	if(!strMVTags.empty()) {
		if(!strQuery.empty())
			strQuery += " UNION ";
		strQuery += "SELECT " MVPROPCOLORDER ", hierarchyid, 0 FROM mvproperties WHERE hierarchyid IN(" + strHierarchyIds + ") AND tag IN (" + strMVTags +") GROUP BY hierarchyid, tag";
	}
	if(!strMVITags.empty()) {
		if(!strQuery.empty())
			strQuery += " UNION ";
		strQuery += "SELECT " MVIPROPCOLORDER ", hierarchyid, orderid FROM mvproperties WHERE hierarchyid IN(" + strHierarchyIds + ") AND tag IN (" +strMVITags +")";
	}
	if(!setSubQueries.empty()) {
		for(iterSubQueries = setSubQueries.begin(); iterSubQueries != setSubQueries.end(); iterSubQueries++) {
			if(!strQuery.empty())
				strQuery += " UNION ";
			if(ECGenProps::GetPropSubquery(*iterSubQueries, strSubquery) == erSuccess) {
				strPropColOrder = GetPropColOrder(*iterSubQueries, strSubquery);

				strQuery += " SELECT " + strPropColOrder + ", hierarchy.id, 0 FROM hierarchy WHERE hierarchy.id IN (" + strHierarchyIds + ")";
			}
		}
	}

	er = lpDatabase->DoSelect(strQuery, &lpDBResult);
	if (er != erSuccess)
		goto exit;
		
	while((lpDBRow = lpDatabase->FetchRow(lpDBResult)) != NULL) {
		if(lpDBRow == NULL)
			break;
			
		lpDBLen = lpDatabase->FetchRowLengths(lpDBResult);
			
		if(lpDBRow[FIELD_NR_MAX] == NULL || lpDBRow[FIELD_NR_MAX+1] == NULL || lpDBRow[FIELD_NR_TAG] == NULL || lpDBRow[FIELD_NR_TYPE] == NULL)
			continue; // No hierarchyid, tag or orderid (?)
			
		key.ulObjId = atoui(lpDBRow[FIELD_NR_MAX]);
		key.ulOrderId = atoui(lpDBRow[FIELD_NR_MAX+1]);
		ulTag = atoui(lpDBRow[FIELD_NR_TAG]);
		ulType = atoui(lpDBRow[FIELD_NR_TYPE]);

		// Find the right place to put things. 

		// In an MVI column, we need to find the exact row to put the data in. We could get order id 0 from the database
		// while it was not requested. If that happens, then we should just discard the data.
		// In a non-MVI column, orderID from the DB is 0, and we should write that value into all rows with this object ID.
		// The lower_bound makes sure that if the requested row had order ID 1, we can still find it.
		if(ulType & MVI_FLAG)
			iterObjIds = mapObjIds.find(key);
		else {
			ASSERT(key.ulOrderId == 0);
			iterObjIds = mapObjIds.lower_bound(key);
		}
			
		if(iterObjIds == mapObjIds.end())
			continue; // Got data for a row we didn't request ? (Possible for MVI queries)
			
		while(iterObjIds != mapObjIds.end() && iterObjIds->first.ulObjId == key.ulObjId) {
			// WARNING. For PT_UNICODE columns, ulTag contains PT_STRING8, since that is the tag in the database. We rely
			// on PT_UNICODE = PT_STRING8 + 1 here since we do a lower_bound to scan for either PT_STRING8 or PT_UNICODE
			// and then use CompareDBPropTag to check the actual type in the while loop later. Same goes for PT_MV_UNICODE.
			iterColumns = mapColumns.lower_bound(PROP_TAG(ulType, ulTag));
			while(iterColumns != mapColumns.end() && CompareDBPropTag(iterColumns->first, PROP_TAG(ulType, ulTag))) {

				// free prop if we're not allocing by soap
				if(soap == NULL && lpsRowSet->__ptr[iterObjIds->second].__ptr[iterColumns->second].ulPropTag != 0) {
					FreePropVal(&lpsRowSet->__ptr[iterObjIds->second].__ptr[iterColumns->second], false);
					memset(&lpsRowSet->__ptr[iterObjIds->second].__ptr[iterColumns->second], 0, sizeof(lpsRowSet->__ptr[iterObjIds->second].__ptr[iterColumns->second]));
				}

				// Handle requesting the same tag multiple times; the data is returned only once, so we need to copy it to all the columns in which it was
				// requested. Note that requesting the same ROW more than once is not supported (it is a map, not a multimap)
				if(CopyDatabasePropValToSOAPPropVal(soap, lpDBRow, lpDBLen, &lpsRowSet->__ptr[iterObjIds->second].__ptr[iterColumns->second]) != erSuccess) {
					CopyEmptyCellToSOAPPropVal(soap, iterColumns->first, &lpsRowSet->__ptr[iterObjIds->second].__ptr[iterColumns->second]);
				}
				
				// Update propval to correct value. We have to do this because we may have requested PT_UNICODE while the database
				// contains PT_STRING8.
				if (PROP_TYPE(lpsRowSet->__ptr[iterObjIds->second].__ptr[iterColumns->second].ulPropTag) == PT_ERROR)
					lpsRowSet->__ptr[iterObjIds->second].__ptr[iterColumns->second].ulPropTag = PROP_TAG(PT_ERROR, PROP_ID(iterColumns->first));
				else
					lpsRowSet->__ptr[iterObjIds->second].__ptr[iterColumns->second].ulPropTag = iterColumns->first;

				if ((lpsRowSet->__ptr[iterObjIds->second].__ptr[iterColumns->second].ulPropTag & MV_FLAG) == 0)
					lpSession->GetSessionManager()->GetCacheManager()->SetCell((sObjectTableKey*)&iterObjIds->first, iterColumns->first, &lpsRowSet->__ptr[iterObjIds->second].__ptr[iterColumns->second]);

				else if ((lpsRowSet->__ptr[iterObjIds->second].__ptr[iterColumns->second].ulPropTag & MVI_FLAG) == MVI_FLAG)
					lpsRowSet->__ptr[iterObjIds->second].__ptr[iterColumns->second].ulPropTag &= ~MVI_FLAG;
				
				setDone.insert(std::make_pair(iterObjIds->second, iterColumns->second));
				iterColumns++;
			}
			
			// We may have more than one row to fill in an MVI table; if we're handling a non-MVI property, then we have to duplicate that
			// value on each row
			if(ulType & MVI_FLAG)
				break; // For the MVI column, we should get one result for each row
			else
				iterObjIds++; // For non-MVI columns, we get one result for all expanded rows 
		}
	}
	
	for(iterColumns = mapColumns.begin(); iterColumns != mapColumns.end(); iterColumns++) {
		for(iterObjIds = mapObjIds.begin(); iterObjIds != mapObjIds.end(); iterObjIds++) {
			if(setDone.count(std::make_pair(iterObjIds->second, iterColumns->second)) == 0) {
				// We may be overwriting a value that was retrieved from the cache before.
				if(soap == NULL && lpsRowSet->__ptr[iterObjIds->second].__ptr[iterColumns->second].ulPropTag != 0) {
					FreePropVal(&lpsRowSet->__ptr[iterObjIds->second].__ptr[iterColumns->second], false);
				}
				CopyEmptyCellToSOAPPropVal(soap, iterColumns->first, &lpsRowSet->__ptr[iterObjIds->second].__ptr[iterColumns->second]);
				lpSession->GetSessionManager()->GetCacheManager()->SetCell((sObjectTableKey*)&iterObjIds->first, iterColumns->first, &lpsRowSet->__ptr[iterObjIds->second].__ptr[iterColumns->second]);
			}
		}
	}
	
exit:
	if(lpDBResult)
		lpDatabase->FreeResult(lpDBResult);
		     
	return er;
}

ECRESULT ECStoreObjectTable::CopyEmptyCellToSOAPPropVal(struct soap *soap, unsigned int ulPropTag, struct propVal *lpPropVal)
{
	ECRESULT	er = erSuccess;

	lpPropVal->ulPropTag = PROP_TAG(PT_ERROR, PROP_ID(ulPropTag));
	lpPropVal->__union = SOAP_UNION_propValData_ul;
	lpPropVal->Value.ul = ZARAFA_E_NOT_FOUND;

	return er;
}


ECRESULT ECStoreObjectTable::GetMVRowCount(unsigned int ulObjId, unsigned int *lpulCount)
{
	ECRESULT er = erSuccess;
	DB_RESULT		lpDBResult = NULL;
	DB_ROW			lpRow = NULL;
	std::string		strQuery, strColName;
	int j;
    ECObjectTableList			listRows;
	ECObjectTableList::iterator iterListRows;
	ECObjectTableMap::iterator		iterIDs;
	ECDatabase *lpDatabase = NULL;
	ECListInt::iterator	iterListMVPropTag;

	pthread_mutex_lock(&m_hLock);

	er = lpSession->GetDatabase(&lpDatabase);
	if (er != erSuccess)
		goto exit;

	// scan for MV-props and add rows
	strQuery = "SELECT count(h.id) FROM hierarchy as h";
	j=0;
	for(iterListMVPropTag = m_listMVSortCols.begin(); iterListMVPropTag != m_listMVSortCols.end(); iterListMVPropTag++)
	{
		strColName = "col"+stringify(j);
		strQuery += " LEFT JOIN mvproperties as "+strColName+" ON h.id="+strColName+".hierarchyid AND "+strColName+".tag="+stringify(PROP_ID(*iterListMVPropTag))+" AND "+strColName+".type="+stringify(PROP_TYPE(NormalizeDBPropTag(*iterListMVPropTag) &~MV_INSTANCE));
		j++;
	}
	
	strQuery += " WHERE h.id="+stringify(ulObjId)+" ORDER by h.id, orderid";
	er = lpDatabase->DoSelect(strQuery, &lpDBResult);
	if(er != erSuccess)
		goto exit;
		
    lpRow = lpDatabase->FetchRow(lpDBResult);
    
    if(lpRow == NULL || lpRow[0] == NULL) {
        er = ZARAFA_E_DATABASE_ERROR;
        goto exit;
    }
	
	*lpulCount = atoi(lpRow[0]);

exit:
	pthread_mutex_unlock(&m_hLock);

	if(lpDBResult)
           lpDatabase->FreeResult(lpDBResult);
	return er;
}

ECRESULT ECStoreObjectTable::Load()
{
    ECRESULT er = erSuccess;
    ECDatabase *lpDatabase = NULL;
    DB_RESULT 	lpDBResult = NULL;
    DB_ROW		lpDBRow = NULL;
    std::string	strQuery;
    ECODStore	*lpData = (ECODStore *)m_lpObjectData;
    sObjectTableKey		sRowItem;
    struct sortOrderArray*      lpsPrevSortOrderArray = NULL;
    
    unsigned int ulFlags = lpData->ulFlags;
    unsigned int ulFolderId = lpData->ulFolderId;
    unsigned int ulObjType = lpData->ulObjType;
    
    unsigned int ulMaxItems = atoui(lpSession->GetSessionManager()->GetConfig()->GetSetting("folder_max_items"));
    unsigned int i;
    
    std::list<unsigned int> lstObjIds;

	er = lpSession->GetDatabase(&lpDatabase);
	if (er != erSuccess)
		goto exit;
    
    if(ulFolderId) {
        // Clear old entries
        Clear();

        // Load the table with all the objects of type ulObjType and flags ulFlags in container ulParent
        
		strQuery = "SELECT hierarchy.id FROM hierarchy WHERE hierarchy.parent=" + stringify(ulFolderId);

        if(ulObjType == MAPI_MESSAGE)
        {
			strQuery += " AND hierarchy.type = " +  stringify(ulObjType);

            if((ulFlags&MSGFLAG_DELETED) == 0)// Normal message and associated message
                strQuery += " AND hierarchy.flags & "+stringify(MSGFLAG_ASSOCIATED)+" = " + stringify(ulFlags&MSGFLAG_ASSOCIATED) + " AND hierarchy.flags & "+stringify(MSGFLAG_DELETED)+" = 0";
            else
                strQuery += " AND hierarchy.flags & "+stringify(MSGFLAG_ASSOCIATED)+" = " + stringify(ulFlags&MSGFLAG_ASSOCIATED) + " AND hierarchy.flags & "+stringify(MSGFLAG_DELETED)+" = " + stringify(MSGFLAG_DELETED);
            
        }
		else if(ulObjType == MAPI_FOLDER) {
            strQuery += " AND hierarchy.type = " +  stringify(ulObjType);
			strQuery += " AND hierarchy.flags & "+stringify(MSGFLAG_DELETED)+" = " + stringify(ulFlags&MSGFLAG_DELETED);
		}else if(ulObjType == MAPI_MAILUSER) { //Read MAPI_MAILUSER and MAPI_DISTLIST
			strQuery += " AND (hierarchy.type = " +  stringify(ulObjType) + " OR hierarchy.type = " +  stringify(MAPI_DISTLIST) + ")";
		}else {
			 strQuery += " AND hierarchy.type = " +  stringify(ulObjType);
		}

        er = lpDatabase->DoSelect(strQuery, &lpDBResult);
        if(er != erSuccess)
            goto exit;

        i = 0;
        while(1) {
            lpDBRow = lpDatabase->FetchRow(lpDBResult);

            if(lpDBRow == NULL)
                break;

            if(lpDBRow[0] == NULL)
                continue;
                
            // Altough we don't want more than ulMaxItems entries, keep looping to get all the results from MySQL. We need to do this
            // because otherwise we can get out of sync with mysql which is still sending us results while we have already stopped
            // reading data.
            if(i > ulMaxItems)
                continue;
                
			lstObjIds.push_back(atoi(lpDBRow[0]));
			i++;
        }

        LoadRows(&lstObjIds, 0);
        
    }
    
exit:
    if(lpsPrevSortOrderArray)
        FreeSortOrderArray(lpsPrevSortOrderArray);
        
    if(lpDBResult)
        lpDatabase->FreeResult(lpDBResult);
    
    return er;
}

ECRESULT ECStoreObjectTable::CheckPermissions(unsigned int ulObjId)
{
    ECRESULT er = erSuccess;
    unsigned int ulParent = 0;
    unsigned int ulFolderFlags = 0;
    ECODStore	*lpData = (ECODStore *)m_lpObjectData;

    if(m_ulObjType == MAPI_MESSAGE) {
        if(lpData->ulFolderId) {
            // We can either see all messages or none at all. Do this check once only as an optimisation.
            if(!this->fPermissionRead) {
                this->ulPermission = lpSession->GetSecurity()->CheckPermission(lpData->ulFolderId, ecSecurityRead);
                this->fPermissionRead = true;
            }

            er = this->ulPermission;                    
        } else {
            // Get the parent id of the row we're inserting (this is very probably cached from Load())
            er = lpSession->GetSessionManager()->GetCacheManager()->GetParent(ulObjId, &ulParent);
            if(er != erSuccess)
                goto exit;
            
            // This will be cached after the first row in the table is added
            er = lpSession->GetSecurity()->CheckPermission(ulParent, ecSecurityRead);
            if(er != erSuccess)
                goto exit;
        }
    } else if(m_ulObjType == MAPI_FOLDER) {
	    // Check the folder type for searchfolders
		er = lpSession->GetSessionManager()->GetCacheManager()->GetObject(ulObjId, NULL, NULL, &ulFolderFlags, NULL);
		if(er != erSuccess)
		    goto exit;
		
		if(ulFolderFlags == FOLDER_SEARCH) {
		    // Searchfolders are only visible in the home store
		    if(lpSession->GetSecurity()->IsAdminOverOwnerOfObject(ulObjId) != erSuccess && lpSession->GetSecurity()->IsStoreOwner(ulObjId) != erSuccess) {
				er = ZARAFA_E_NO_ACCESS;
		        goto exit;
            }
		}

        er = lpSession->GetSecurity()->CheckPermission(ulObjId, ecSecurityFolderVisible);
        if(er != erSuccess)
            goto exit;
    }
            
exit:
    return er;

}

ECRESULT ECStoreObjectTable::AddRowKey(ECObjectTableList* lpRows, unsigned int *lpulLoaded, unsigned int ulFlags, bool bLoad)
{
    ECRESULT er = erSuccess;
    GUID guidServer;
    ECODStore* lpODStore = (ECODStore*)m_lpObjectData;
    std::list<unsigned int> lstFolders;
    ECSearchResultArray *lpIndexerResults = NULL;
    std::set<unsigned int> setMatches;
    ECObjectTableList sQueryRows;
    ECObjectTableList::iterator iterQueryRows;
    ECObjectTableList::iterator iterRows;
    struct rowSet *lpRowSet = NULL;
    unsigned int ulMatches = 0;
    bool fHidden = false;
    struct propTagArray sPropTagArray = {0,0};
    int n = 0;
    ECCategory *lpCategory = NULL;
    unsigned int ulFirstCol = 0;
    ECDatabase *lpDatabase = NULL;
    
	pthread_mutex_lock(&m_hLock);
	
    // Use normal table update code if:
    //  - not an initial load (but a table update)
    //  - no restriction
    //  - not a restriction on a folder (eg searchfolder)
    if(!bLoad || !lpsRestrict || !lpODStore->ulFolderId || !lpODStore->ulStoreId || (lpODStore->ulFlags & MAPI_ASSOCIATED)) {
        er = ECGenericObjectTable::AddRowKey(lpRows, lpulLoaded, ulFlags, bLoad);
   } else {
		er = lpSession->GetDatabase(&lpDatabase);
        if(er != erSuccess)
            goto exit;

        // Attempt to use the indexer
        er = lpSession->GetSessionManager()->GetServerGUID(&guidServer);
        if(er != erSuccess)
            goto exit;
        
        lstFolders.push_back(lpODStore->ulFolderId);
        
    	if(GetIndexerResults(lpDatabase, lpSession->GetSessionManager()->GetConfig(), lpSession->GetSessionManager()->GetLogger(), lpSession->GetSessionManager()->GetCacheManager(), &guidServer, lpODStore->ulStoreId, lstFolders, lpsRestrict, &lpIndexerResults) != erSuccess) {
    	    // Cannot handle this restriction with the indexer, use 'normal' restriction code
    	    // Reasons can be:
    	    //  - restriction too complex
    	    //  - folder not indexed
    	    //  - indexer not running / not configured
    	    er = ECGenericObjectTable::AddRowKey(lpRows, lpulLoaded, ulFlags, bLoad);
    	    goto exit;
        }
    
        // Put the results in setMatches	
    	for(unsigned int i=0; i < lpIndexerResults->__size ; i++) {
    	    unsigned int ulObjectId = 0;
    	    
    	    if(lpSession->GetSessionManager()->GetCacheManager()->GetObjectFromEntryId(&lpIndexerResults->__ptr[i].sEntryId, &ulObjectId) == erSuccess) {
    	        setMatches.insert(ulObjectId);
    	    }	
    	}
    	
    	// Set up sPropTagArray
    	sPropTagArray.__size = lpsSortOrderArray->__size + 1;
    	sPropTagArray.__ptr = new unsigned int[sPropTagArray.__size];
    	if(m_ulCategories > 0) {
        	sPropTagArray.__ptr[n++] = PR_MESSAGE_FLAGS;
        	ulFirstCol = 1;
        }
    	for(int i=0; i < lpsSortOrderArray->__size; i++) {
    	    sPropTagArray.__ptr[n++] = lpsSortOrderArray->__ptr[i].ulPropTag;
    	}
    	sPropTagArray.__size = n;

    	// Loop through requested rows, match them to setMatches, get the sort data for the rows and add them to the table
    	iterRows = lpRows->begin();
    	
    	while(iterRows != lpRows->end()) {
    	    sQueryRows.clear();
    	    
    	    // Get at most 20 matching rows
    	    ulMatches = 0;
    	    while(iterRows != lpRows->end() && ulMatches < 20) {
    	        if(setMatches.find(iterRows->ulObjId) != setMatches.end()) {
        	        sQueryRows.push_back(*iterRows); 
        	        ulMatches++;
                }
                iterRows++;
    	    }
    	    
    	    if(ulMatches == 0)
    	        break; // This will only happen if iterRows == lpRows->end()
    	    
            // Get the row data for sorting for all 20 rows
    		er = m_lpfnQueryRowData(this, NULL, lpSession, &sQueryRows, &sPropTagArray, m_lpObjectData, &lpRowSet, true, false);
            if(er != erSuccess)
                goto exit;
            
            // Add each row to the table
            iterQueryRows = sQueryRows.begin();
            for(int i=0; i< lpRowSet->__size; i++, iterQueryRows++) {
                if(m_ulCategories > 0) {
                    bool bUnread = false;
                    
                    if((lpRowSet->__ptr[i].__ptr[0].Value.ul & MSGFLAG_READ) == 0) // FIXME
                        bUnread = true;

                    // Update category for this row if required, and send notification if required
                    AddCategoryBeforeAddRow(*iterQueryRows, lpRowSet->__ptr[i].__ptr+ulFirstCol, lpsSortOrderArray->__size, ulFlags, bUnread, &fHidden, &lpCategory);
                }

                // Put the row into the key table and send notification if required
                AddRow(*iterQueryRows, lpRowSet->__ptr[i].__ptr+ulFirstCol, lpsSortOrderArray->__size, ulFlags, fHidden, lpCategory);
            }
            
            FreeRowSet(lpRowSet, true);
            lpRowSet = NULL;
    	}
    }
    
exit:
	pthread_mutex_unlock(&m_hLock);

	if (lpIndexerResults)
		FreeSearchResults(lpIndexerResults);
	
	if(lpRowSet)
		FreeRowSet(lpRowSet, true);

	if(sPropTagArray.__ptr)
		delete [] sPropTagArray.__ptr;

	return er;
    
}


/**
 * Get all deferred changes for a specific folder
 */
ECRESULT GetDeferredTableUpdates(ECDatabase *lpDatabase, unsigned int ulFolderId, std::list<unsigned int> *lpDeferred)
{
	ECRESULT er = erSuccess;
	DB_RESULT lpDBResult = NULL;
	DB_ROW lpDBRow = NULL;
	
	lpDeferred->clear();
	
	std::string strQuery = "SELECT hierarchyid FROM deferredupdate WHERE folderid = " + stringify(ulFolderId);
	
	er = lpDatabase->DoSelect(strQuery, &lpDBResult);
	if(er != erSuccess)
		goto exit;
		
	while((lpDBRow = lpDatabase->FetchRow(lpDBResult)) != NULL) {
		lpDeferred->push_back(atoui(lpDBRow[0]));
	}

exit:
	if(lpDBResult)
		lpDatabase->FreeResult(lpDBResult);
		
	return er;
}

/**
 * Get all deferred changes for a specific folder
 */
ECRESULT GetDeferredTableUpdates(ECDatabase *lpDatabase, ECObjectTableList* lpRowList, std::list<unsigned int> *lpDeferred)
{
	ECRESULT er = erSuccess;
	DB_RESULT lpDBResult = NULL;
	DB_ROW lpDBRow = NULL;
	ECObjectTableList::iterator     iterRowList;
	
	lpDeferred->clear();
	
	std::string strQuery = "SELECT hierarchyid FROM deferredupdate WHERE hierarchyid IN( ";

	for(iterRowList = lpRowList->begin(); iterRowList != lpRowList->end(); iterRowList++) {
		strQuery += stringify(iterRowList->ulObjId);
		strQuery += ",";
	}
	
	// Remove trailing comma
	strQuery.resize(strQuery.size()-1);
	strQuery += ")";
	
	er = lpDatabase->DoSelect(strQuery, &lpDBResult);
	if(er != erSuccess)
		goto exit;
		
	while((lpDBRow = lpDatabase->FetchRow(lpDBResult)) != NULL) {
		lpDeferred->push_back(atoui(lpDBRow[0]));
	}

exit:
	if(lpDBResult)
		lpDatabase->FreeResult(lpDBResult);
		
	return er;
}

