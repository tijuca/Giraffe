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
#include <mapiext.h>

#include <sys/types.h>
#include <regex.h>

#include <iostream>

#include "Zarafa.h"
#include "ZarafaUtil.h"
#include "ECSecurity.h"
#include "ECDatabaseUtils.h"
#include "ECKeyTable.h"
#include "ECGenProps.h"
#include "ECGenericObjectTable.h"
#include "SOAPUtils.h"
#include "stringutil.h"

#include "Trace.h"
#include "ECSessionManager.h"
       
#include "ECSession.h"

struct sortOrderArray sDefaultSortOrder = {0,0};

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

ULONG sANRProps[] = { PR_DISPLAY_NAME, PR_SMTP_ADDRESS, PR_ACCOUNT, PR_DEPARTMENT_NAME, PR_OFFICE_TELEPHONE_NUMBER, PR_OFFICE_LOCATION, PR_PRIMARY_FAX_NUMBER, PR_SURNAME};

#define ISMINMAX(x) ((x) == EC_TABLE_SORT_CATEG_MIN || (x) == EC_TABLE_SORT_CATEG_MAX)

/**
 * Apply RELOP_* rules to equality value from CompareProp
 *
 * 'equality' is a value from CompareProp which can be -1, 0 or 1. This function
 * returns TRUE when the passed relop matches the equality value. (Eg equality=0
 * and RELOP_EQ, then returns TRUE)
 * @param relop RELOP
 * @param equality Equality value from CompareProp
 * @return TRUE if the relop matches
 */
static inline bool match(unsigned int relop, int equality)
{
	bool fMatch = false;
	
	switch(relop) {
	case RELOP_GE:
		fMatch = equality >= 0;
		break;
	case RELOP_GT:
		fMatch = equality > 0;
		break;
	case RELOP_LE:
		fMatch = equality <= 0;
		break;
	case RELOP_LT:
		fMatch = equality < 0;
		break;
	case RELOP_NE:
		fMatch = equality != 0;
		break;
	case RELOP_RE:
		fMatch = false; // FIXME ?? how should this work ??
		break;
	case RELOP_EQ:
		fMatch = equality == 0;
		break;
	}
	
	return fMatch;
}



/**
 * Constructor of the Generic Object Table
 *
 * @param[in] lpSession
 *					Reference to a session object; cannot be NULL.
 * @param[in] ulObjType
 *					The Object type of the objects in the table
 */
ECGenericObjectTable::ECGenericObjectTable(ECSession *lpSession, unsigned int ulObjType, unsigned int ulFlags, const ECLocale &locale)
{
	this->lpSession			= lpSession;
	this->lpKeyTable		= new ECKeyTable;
	this->lpsPropTagArray	= NULL;
	this->lpsRestrict		= NULL;
	this->m_lpObjectData	= NULL;			// Must be set on the parent class
	this->m_lpfnQueryRowData= NULL;			// Must be set on the parent class
	this->m_ulCategories 	= 0;
	this->m_ulExpanded		= 0;
	this->m_ulCategory		= 1;
	this->m_ulObjType		= ulObjType;
	this->m_bPopulated		= false;
	this->m_ulFlags			= ulFlags;

	this->m_locale = locale;
	
	// No sort order by default
	this->lpsSortOrderArray	= NULL;

	// No columns by default
	this->lpsPropTagArray = new struct propTagArray;
	this->lpsPropTagArray->__size = 0;
	this->lpsPropTagArray->__ptr = NULL;

	m_bMVCols = false;
	m_bMVSort = false;

	m_ulTableId = -1;

	pthread_mutexattr_t mattr;
	pthread_mutexattr_init(&mattr);
	pthread_mutexattr_settype(&mattr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&m_hLock, &mattr);
}

/**
 * Destructor of the Generic Object Table
 */
ECGenericObjectTable::~ECGenericObjectTable()
{
    ECCategoryMap::iterator iterCategories;
        
	if(this->lpKeyTable)
		delete lpKeyTable;

	if(this->lpsPropTagArray)
		FreePropTagArray(this->lpsPropTagArray);

	if(this->lpsSortOrderArray)
		FreeSortOrderArray(this->lpsSortOrderArray);

	if(this->lpsRestrict)
		FreeRestrictTable(this->lpsRestrict);
		
    for(iterCategories = m_mapCategories.begin(); iterCategories != m_mapCategories.end(); iterCategories++)
        delete iterCategories->second;
		
	pthread_mutex_destroy(&m_hLock);
}

/**
 * Moves the cursor to a specific row in the table.
 *
 * @param[in] ulBookmark
 *				Identifying the starting position for the seek action. A bookmark can be created with 
 *				ECGenericObjectTable::CreateBookmark call, or use one of the following bookmark predefines:
 *				\arg BOOKMARK_BEGINNING		Start seeking from the beginning of the table.
 *				\arg BOOKMARK_CURRENT		Start seeking from the current position of the table.
 *				\arg BOOKMARK_END			Start seeking from the end of the table.
 * @param[in] lSeekTo
 *				Positive or negative number of rows moved starting from the bookmark.
 * @param[in] lplRowsSought
 *				Pointer to the number or rows that were processed in the seek action. If lplRowsSought is NULL, 
 *				the caller iss not interested in the returned output.
 *
 * @return Zarafa error code
 */
ECRESULT ECGenericObjectTable::SeekRow(unsigned int ulBookmark, int lSeekTo, int *lplRowsSought)
{
	ECRESULT er = erSuccess;

	pthread_mutex_lock(&m_hLock);

	er = Populate();
	if(er != erSuccess)
	    goto exit;

	if(lpsSortOrderArray == NULL) {
		er = SetSortOrder(&sDefaultSortOrder, 0, 0);

		if(er != erSuccess)
			goto exit;
	}

	er = lpKeyTable->SeekRow(ulBookmark, lSeekTo, lplRowsSought);

exit:
	pthread_mutex_unlock(&m_hLock);

	return er;
}

/**
 * Finds the next row in the table that matches specific search criteria.
 *
 *
 * @param[in] lpsRestrict
 * @param[in] ulBookmark
 * @param[in] ulFlags
 *
 * @return Zarafa error code
 */
ECRESULT ECGenericObjectTable::FindRow(struct restrictTable *lpsRestrict, unsigned int ulBookmark, unsigned int ulFlags)
{
	ECRESULT		er = erSuccess;
	bool			fMatch = false;
	int				ulSeeked = 0;
	int				i;
	unsigned int	ulRow = 0;
	unsigned int	ulCount = 0;
	int				ulTraversed = 0;
	SUBRESTRICTIONRESULTS *lpSubResults = NULL;
	
	struct propTagArray	*lpPropTags = NULL;
	struct rowSet		*lpRowSet = NULL;

	ECObjectTableList	ecRowList;
	sObjectTableKey		sRowItem;

	entryId				sEntryId;

	pthread_mutex_lock(&m_hLock);
	
	er = Populate();
	if(er != erSuccess)
	    goto exit;
	
    // We may need the table position later (ulCount is not used)
	er = lpKeyTable->GetRowCount(&ulCount, &ulRow);
    if(er != erSuccess)
        goto exit;

	// Start searching at the right place
	if(ulBookmark == BOOKMARK_END && ulFlags & DIR_BACKWARD) {
		er = SeekRow(ulBookmark, -1, NULL);
	} else {
		er = SeekRow(ulBookmark, 0, NULL);
	}
	
    if(er != erSuccess)
        goto exit;


	// Special optimisation case: if you're searching the PR_INSTANCE_KEY, we can
	// look this up directly!
	if( ulBookmark == BOOKMARK_BEGINNING &&
		lpsRestrict->ulType == RES_PROPERTY && lpsRestrict->lpProp->ulType == RELOP_EQ && 
		lpsRestrict->lpProp->lpProp && lpsRestrict->lpProp->ulPropTag == PR_INSTANCE_KEY &&
		lpsRestrict->lpProp->lpProp->ulPropTag == PR_INSTANCE_KEY &&
		lpsRestrict->lpProp->lpProp->Value.bin && lpsRestrict->lpProp->lpProp->Value.bin->__size == sizeof(unsigned int)*2) 
	{
		sRowItem.ulObjId = *(unsigned int *)lpsRestrict->lpProp->lpProp->Value.bin->__ptr;
		sRowItem.ulOrderId = *(unsigned int *)(lpsRestrict->lpProp->lpProp->Value.bin->__ptr+sizeof(LONG));

		er = this->lpKeyTable->SeekId(&sRowItem);
		goto exit;
	}

	// We can do the same with PR_ENTRYID
	if( ulBookmark == BOOKMARK_BEGINNING && 
		lpsRestrict->ulType == RES_PROPERTY && lpsRestrict->lpProp->ulType == RELOP_EQ && 
		lpsRestrict->lpProp->lpProp && lpsRestrict->lpProp->ulPropTag == PR_ENTRYID &&
		lpsRestrict->lpProp->lpProp->ulPropTag == PR_ENTRYID &&
		lpsRestrict->lpProp->lpProp->Value.bin && IsZarafaEntryId(lpsRestrict->lpProp->lpProp->Value.bin->__size, lpsRestrict->lpProp->lpProp->Value.bin->__ptr)) 
	{
		sEntryId.__ptr = lpsRestrict->lpProp->lpProp->Value.bin->__ptr;
		sEntryId.__size = lpsRestrict->lpProp->lpProp->Value.bin->__size;

		er = lpSession->GetSessionManager()->GetCacheManager()->GetObjectFromEntryId(&sEntryId, &sRowItem.ulObjId);
		if(er != erSuccess)
			goto exit;

		sRowItem.ulOrderId = 0; // FIXME: this is incorrect when MV_INSTANCE is specified on a column, but this won't happen often.

		er = this->lpKeyTable->SeekId(&sRowItem);
		goto exit;
	}

	// Get the columns we will be needing for this search
	er = GetRestrictPropTags(lpsRestrict, NULL, &lpPropTags);

	if(er != erSuccess)
		goto exit;

	// Loop through the rows, matching it with the search criteria
	while(1) {
		ecRowList.clear();

		// Get the row ID of the next row
		er = lpKeyTable->QueryRows(20, &ecRowList, (ulFlags & DIR_BACKWARD)?true:false, TBL_NOADVANCE);

		if(er != erSuccess)
			goto exit;

		if(ecRowList.empty())
			break;

		// Get the rowdata from the QueryRowData function
		er = m_lpfnQueryRowData(this, NULL, lpSession, &ecRowList, lpPropTags, m_lpObjectData, &lpRowSet, true, false);
		if(er != erSuccess)
			goto exit;
			
        er = RunSubRestrictions(lpSession, m_lpObjectData, lpsRestrict, &ecRowList, m_locale, &lpSubResults);
        if(er != erSuccess)
            goto exit;

		ASSERT(lpRowSet->__size == (int)ecRowList.size());

		for(i=0; i < lpRowSet->__size; i++)
		{
			// Match the row
			er = MatchRowRestrict(lpSession->GetSessionManager()->GetCacheManager(), &lpRowSet->__ptr[i], lpsRestrict, lpSubResults, m_locale, &fMatch);
			if(er != erSuccess)
				goto exit;

			if(fMatch)
			{
				// A Match, seek the cursor
				lpKeyTable->SeekRow(BOOKMARK_CURRENT, ulFlags & DIR_BACKWARD ? -i : i, &ulSeeked);
				break;
			}
		}
		if(fMatch)
			break;

		// No match, then advance the cursor
		lpKeyTable->SeekRow(BOOKMARK_CURRENT, ulFlags & DIR_BACKWARD ? -(int)ecRowList.size() : ecRowList.size(), &ulSeeked);

		// No advance possible, break the loop
		if(ulSeeked == 0)
			break;

		// Free memory
		if(lpRowSet){
			FreeRowSet(lpRowSet, true);
			lpRowSet = NULL;
		}
		
		if(lpSubResults)
			FreeSubRestrictionResults(lpSubResults);
        lpSubResults = NULL;

	}

	if(!fMatch) {
		er = ZARAFA_E_NOT_FOUND;
		lpKeyTable->SeekRow(ECKeyTable::EC_SEEK_SET, ulRow, &ulTraversed);
    }

exit:
	pthread_mutex_unlock(&m_hLock);

	if(lpSubResults)
		FreeSubRestrictionResults(lpSubResults);
	    
	if(lpRowSet)
		FreeRowSet(lpRowSet, true);

	if(lpPropTags)
		FreePropTagArray(lpPropTags);
			

	return er;
}

/**
 * Returns the total number of rows in the table.
 *
 * @param[out] lpulRowCount
 *					Pointer to the number of rows in the table.
 * @param[out] lpulCurrentRow
 *					Pointer to the current row id in the table.
 *
 * @return Zarafa error code
 */
ECRESULT ECGenericObjectTable::GetRowCount(unsigned int *lpulRowCount, unsigned int *lpulCurrentRow)
{
	ECRESULT er = erSuccess;

	pthread_mutex_lock(&m_hLock);

	er = Populate();
	if(er != erSuccess)
	    goto exit;
	    
	if(lpsSortOrderArray == NULL) {
		er = SetSortOrder(&sDefaultSortOrder, 0, 0);

		if(er != erSuccess)
			goto exit;
	}

	er = lpKeyTable->GetRowCount(lpulRowCount, lpulCurrentRow);

exit:
	pthread_mutex_unlock(&m_hLock);

	return er;
}

ECRESULT ECGenericObjectTable::ReloadTableMVData(ECObjectTableList* lplistRows, ECListInt* lplistMVPropTag)
{
	// default ignore MV-view, show as an normal view
	return erSuccess;
}

/**
 * Returns a list of columns for the table.
 *
 * If the function is not overridden, it returns always an empty column set.
 *
 * @param[in,out] lplstProps
 *						a list of columns for the table
 *
 * @return Zarafa error code
 */
ECRESULT ECGenericObjectTable::GetColumnsAll(ECListInt* lplstProps)
{
	return erSuccess;
}

/**
 * Reload the table objects.
 *
 * Rebuild the whole table with the current restriction and sort order. If the sort order 
 * includes a multi-valued property, a single row appearing in multiple rows. ReloadTable
 * may expand or contract expanded MVI rows if the sort order or column set have changed. If there
 * is no change in MVI-related expansion, it will call ReloadKeyTable which only does a
 * resort/refilter of the existing rows.
 *
 * @param[in] eType
 *				The reload type determines how it should reload.
 *
 * @return Zarafa error code
 */
ECRESULT ECGenericObjectTable::ReloadTable(enumReloadType eType)
{
	ECRESULT			er = erSuccess;
	bool				bMVColsNew = false;
	bool				bMVSortNew = false;

	ECObjectTableList			listRows;
	ECObjectTableList::iterator	iterListRows;
	ECObjectTableMap::iterator	iterIDs;
	ECListInt					listMVPropTag;

	
	pthread_mutex_lock(&m_hLock);

	//Scan for MVI columns
	for(int i=0; lpsPropTagArray != NULL && i < lpsPropTagArray->__size; i++) {
		if((PROP_TYPE(lpsPropTagArray->__ptr[i]) &MVI_FLAG) == MVI_FLAG) {
			if(bMVColsNew == true)
				ASSERT(FALSE); //FIXME: error 1 mv prop set!!!

			bMVColsNew = true;
			listMVPropTag.push_back(lpsPropTagArray->__ptr[i]);
		}
	}
	
	//Check for mvi props
	for(int i=0; lpsSortOrderArray != NULL && i < lpsSortOrderArray->__size; i++) {
		if((PROP_TYPE(lpsSortOrderArray->__ptr[i].ulPropTag)&MVI_FLAG) == MVI_FLAG) {
			if(bMVSortNew == true)
				ASSERT(FALSE);

			bMVSortNew = true;
			listMVPropTag.push_back(lpsSortOrderArray->__ptr[i].ulPropTag);
		}
	}

	listMVPropTag.sort();
	listMVPropTag.unique();

	if((m_bMVCols == false && m_bMVSort == false && bMVColsNew == false && bMVSortNew == false) ||
		(listMVPropTag == m_listMVSortCols && (m_bMVCols == bMVColsNew || m_bMVSort == bMVSortNew)) )
	{
		if(eType == RELOAD_TYPE_SORTORDER)
			er = ReloadKeyTable();

		goto exit; 				// no MVprops or already sorted, skip MV sorts
	}

	m_listMVSortCols = listMVPropTag;

	// Get all the Single Row ID's from the ID map
	for(iterIDs = mapObjects.begin(); iterIDs != mapObjects.end(); iterIDs++) {
		if(iterIDs->first.ulOrderId == 0)
			listRows.push_back(iterIDs->first);
	}

	if(mapObjects.empty())
		goto skip;

	if(bMVColsNew == true || bMVSortNew == true)
	{
		// Expand rows to contain all MVI expansions (listRows is appended to)
		er = ReloadTableMVData(&listRows, &listMVPropTag);
		if(er != erSuccess)
			goto exit;
	}

	// Clear row data	
	Clear();

	//Add items
	for(iterListRows = listRows.begin(); iterListRows != listRows.end(); iterListRows++)
	{	
		mapObjects[*iterListRows] = 1;
	}

	// Load the keys with sort data from the table
	er = AddRowKey(&listRows, NULL, 0, true);

skip:
	m_bMVCols = bMVColsNew;
	m_bMVSort = bMVSortNew;

exit:
	pthread_mutex_unlock(&m_hLock);

	return er;
}

/**
 * Returns the total number of multi value rows of a specific object.
 *
 * This methode should be overridden and should return the total number of multi value rows of a specific object.
 *
 * @param[in] ulObjId
 *					Object id to receive the number of multi value rows
 * @param[out] lpulCount
 *					Pointer to the number of multi value rows of the object ulObjId
 *
 * @return Zarafa error code
 */
ECRESULT ECGenericObjectTable::GetMVRowCount(unsigned int ulObjId, unsigned int *lpulCount)
{
	ECRESULT er = ZARAFA_E_NO_SUPPORT;
	return er;
}

/**
 * Defines the properties and order of properties to appear as columns in the table.
 *
 * @param[in]	lpsPropTags
 *					Pointer to an array of property tags with a specific order.
 *					The lpsPropTags parameter cannot be set to NULL; table must have at least one column.
 *
 * @return Zarafa error code
 */
ECRESULT ECGenericObjectTable::SetColumns(struct propTagArray *lpsPropTags, bool bDefaultSet)
{
	ECRESULT er = erSuccess;
	//FIXME: check the lpsPropTags array, 0x????xxxx -> xxxx must be checked

	// Remember the columns for later use (in QueryRows)
	// This is a very very quick operation, as we only save the information.

	pthread_mutex_lock(&m_hLock);

	// Delete the old column set
	if(this->lpsPropTagArray)
		FreePropTagArray(this->lpsPropTagArray);

	lpsPropTagArray = new struct propTagArray;
	lpsPropTagArray->__size = lpsPropTags->__size;
	lpsPropTagArray->__ptr = new unsigned int[lpsPropTags->__size];
	if (bDefaultSet) {
		for (int n = 0; n < lpsPropTags->__size; n++) {
			if (PROP_TYPE(lpsPropTags->__ptr[n]) == PT_STRING8 || PROP_TYPE(lpsPropTags->__ptr[n]) == PT_UNICODE)
				lpsPropTagArray->__ptr[n] = CHANGE_PROP_TYPE(lpsPropTags->__ptr[n], ((m_ulFlags & MAPI_UNICODE) ? PT_UNICODE : PT_STRING8));
			else if (PROP_TYPE(lpsPropTags->__ptr[n]) == PT_MV_STRING8 || PROP_TYPE(lpsPropTags->__ptr[n]) == PT_MV_UNICODE)
				lpsPropTagArray->__ptr[n] = CHANGE_PROP_TYPE(lpsPropTags->__ptr[n], ((m_ulFlags & MAPI_UNICODE) ? PT_MV_UNICODE : PT_MV_STRING8));
			else
				lpsPropTagArray->__ptr[n] = lpsPropTags->__ptr[n];
		}
	} else
		memcpy(lpsPropTagArray->__ptr, lpsPropTags->__ptr, sizeof(unsigned int) * lpsPropTags->__size);
	

	er = ReloadTable(RELOAD_TYPE_SETCOLUMNS);

	pthread_mutex_unlock(&m_hLock);
	
	return er;
}

ECRESULT ECGenericObjectTable::GetColumns(struct soap *soap, ULONG ulFlags, struct propTagArray **lppsPropTags)
{
	ECRESULT			er = erSuccess;
	int					n = 0;
	ECListInt			lstProps;
	ECListIntIterator	iterProps;
	struct propTagArray *lpsPropTags;

	ECObjectTableMap::iterator		iterObjects;
	
	pthread_mutex_lock(&m_hLock);


	if(ulFlags & TBL_ALL_COLUMNS) {
		// All columns were requested. Simply get a unique list of all the proptags used in all the objects in this table
        er = Populate();
        if(er != erSuccess)
            goto exit;

		er = GetColumnsAll(&lstProps);
		if(er != erSuccess)
			goto exit;
	
		// Make sure we have a unique list
		lstProps.sort();
		lstProps.unique();

		// Convert them all over to a struct propTagArray
        lpsPropTags = s_alloc<propTagArray>(soap);
        lpsPropTags->__size = lstProps.size();
        lpsPropTags->__ptr = s_alloc<unsigned int>(soap, lstProps.size());

		for (n = 0, iterProps = lstProps.begin(); iterProps != lstProps.end(); iterProps++, n++) {
			lpsPropTags->__ptr[n] = *iterProps;
			if (PROP_TYPE(lpsPropTags->__ptr[n]) == PT_STRING8 || PROP_TYPE(lpsPropTags->__ptr[n]) == PT_UNICODE)
				lpsPropTags->__ptr[n] = CHANGE_PROP_TYPE(lpsPropTags->__ptr[n], ((m_ulFlags & MAPI_UNICODE) ? PT_UNICODE : PT_STRING8));
			else if (PROP_TYPE(lpsPropTags->__ptr[n]) == PT_MV_STRING8 || PROP_TYPE(lpsPropTags->__ptr[n]) == PT_MV_UNICODE)
				lpsPropTags->__ptr[n] = CHANGE_PROP_TYPE(lpsPropTags->__ptr[n], ((m_ulFlags & MAPI_UNICODE) ? PT_MV_UNICODE : PT_MV_STRING8));
		}
	} else {
		lpsPropTags = s_alloc<propTagArray>(soap);

		if(lpsPropTagArray) {

			lpsPropTags->__size = lpsPropTagArray->__size;

			lpsPropTags->__ptr = s_alloc<unsigned int>(soap, lpsPropTagArray->__size);
			memcpy(lpsPropTags->__ptr, lpsPropTagArray->__ptr, sizeof(unsigned int) * lpsPropTagArray->__size);
		} else {
			lpsPropTags->__size = 0;
			lpsPropTags->__ptr = NULL;
		}
	}

	*lppsPropTags = lpsPropTags;

exit:

	pthread_mutex_unlock(&m_hLock);

	return er;
}

ECRESULT ECGenericObjectTable::ReloadKeyTable()
{
	ECRESULT		er = erSuccess;
	ECObjectTableList listRows;
	ECObjectTableMap::iterator iterMapObject;
	ECCategoryMap::iterator iterCategories;

	pthread_mutex_lock(&m_hLock);

	// Get all the Row ID's from the ID map
	for(iterMapObject = mapObjects.begin(); iterMapObject != mapObjects.end(); iterMapObject++)
		listRows.push_back(iterMapObject->first);

	// Reset the key table
	lpKeyTable->Clear();
	m_mapLeafs.clear();

	for(iterCategories = m_mapCategories.begin(); iterCategories != m_mapCategories.end(); iterCategories++)
		delete iterCategories->second;
	m_mapCategories.clear();
	m_mapSortedCategories.clear();

	// Load the keys with sort data from the table
	er = AddRowKey(&listRows, NULL, 0, true);

	if(er != erSuccess)
		goto exit;

exit:
	//Auto delete
	pthread_mutex_unlock(&m_hLock);

	return er;
}

ECRESULT ECGenericObjectTable::SetSortOrder(struct sortOrderArray *lpsSortOrder, unsigned int ulCategories, unsigned int ulExpanded)
{
	ECRESULT er = erSuccess;
	
	// Set the sort order, re-read the data from the database, and reset the current row
	// The current row is reset to point to the row it was pointing to in the first place.
	// This is pretty easy as it is pointing at the same object ID as it was before we
	// reloaded.

	pthread_mutex_lock(&m_hLock);

	if(m_ulCategories == ulCategories && m_ulExpanded == ulExpanded && this->lpsSortOrderArray && CompareSortOrderArray(this->lpsSortOrderArray, lpsSortOrder) == 0) {
		// Sort requested was already set, return OK
		this->SeekRow(BOOKMARK_BEGINNING, 0, NULL);
		goto exit;
	}
	
	// Check validity of tags
	if(lpsSortOrder) {
		for(int i = 0; i < lpsSortOrder->__size; i++) {
			if((PROP_TYPE(lpsSortOrder->__ptr[i].ulPropTag) & MVI_FLAG) == MV_FLAG) {
				er = ZARAFA_E_TOO_COMPLEX;
				goto exit;
			}
		}
	}
	
	m_ulCategories = ulCategories;
	m_ulExpanded = ulExpanded;

	// Save the sort order requested
	if(this->lpsSortOrderArray)
		FreeSortOrderArray(this->lpsSortOrderArray);

	this->lpsSortOrderArray = new struct sortOrderArray;
	this->lpsSortOrderArray->__size = lpsSortOrder->__size;
	if(lpsSortOrder->__size == 0 ) {
		this->lpsSortOrderArray->__ptr = NULL;
	} else {
		this->lpsSortOrderArray->__ptr = new sortOrder[lpsSortOrder->__size];
		memcpy(this->lpsSortOrderArray->__ptr, lpsSortOrder->__ptr, sizeof(struct sortOrder) * lpsSortOrder->__size);
	}

	er = ReloadTable(RELOAD_TYPE_SORTORDER);
	
	if(er != erSuccess)
		goto exit;

	// FIXME When you change the sort order, current row should be equal to previous row ID
	er = lpKeyTable->SeekRow(0, 0, NULL);

exit:

	pthread_mutex_unlock(&m_hLock);

	return er;
}

ECRESULT ECGenericObjectTable::GetBinarySortKey(struct propVal *lpsPropVal, unsigned int *lpSortLen, unsigned char **lppSortData)
{
	ECRESULT		er = erSuccess;
	unsigned char	*lpSortData = NULL;
	unsigned int	ulSortLen = 0;

	switch(PROP_TYPE(lpsPropVal->ulPropTag)) {
	case PT_BOOLEAN:
	case PT_I2:
		ulSortLen = 2;
		lpSortData = new unsigned char[2];
		*(unsigned short *)lpSortData = htons(lpsPropVal->Value.b);
		break;
	case PT_LONG:
		ulSortLen = 4;
		lpSortData = new unsigned char[4];
		*(unsigned int *)lpSortData = htonl(lpsPropVal->Value.ul);
		break;
	case PT_R4:
	    ulSortLen = sizeof(double);
	    lpSortData = new unsigned char[sizeof(double)];
	    *(double *)lpSortData = lpsPropVal->Value.flt;
		break;
	case PT_APPTIME:
	case PT_DOUBLE:
	    ulSortLen = sizeof(double);
	    lpSortData = new unsigned char[sizeof(double)];
	    *(double *)lpSortData = lpsPropVal->Value.dbl;
	    break;
	case PT_CURRENCY:
	    ulSortLen = 0;
	    lpSortData = NULL;
		break;
	case PT_SYSTIME:
		ulSortLen = 8;
		lpSortData = new unsigned char[8];
		*(unsigned int *)lpSortData = htonl(lpsPropVal->Value.hilo->hi);
		*(unsigned int *)(lpSortData+4) = htonl(lpsPropVal->Value.hilo->lo);
		break;
	case PT_I8:
		ulSortLen = 8;
		lpSortData = new unsigned char[8];
		*(unsigned int *)lpSortData = htonl((unsigned int)(lpsPropVal->Value.li >> 32));
		*(unsigned int *)(lpSortData+4) = htonl((unsigned int)lpsPropVal->Value.li);
		break;
	case PT_STRING8:
	case PT_UNICODE: {
			// is this check needed here, or is it already checked 50 times along the way?
			if (!lpsPropVal->Value.lpszA) {
				ulSortLen = 0;
				lpSortData = NULL;
				break;
			}
			
			createSortKeyDataFromUTF8(lpsPropVal->Value.lpszA, 255, m_locale, &ulSortLen, &lpSortData);
		}
		break;
	case PT_CLSID:
	case PT_BINARY:
		ulSortLen = lpsPropVal->Value.bin->__size;
		lpSortData = new unsigned char [ulSortLen];
		memcpy(lpSortData, lpsPropVal->Value.bin->__ptr, ulSortLen); // could be optimized to one func
		break;
	case PT_ERROR:
		ulSortLen = 0;
		lpSortData = NULL;
		break;
	default:
		er = ZARAFA_E_INVALID_TYPE;
		break;
	}

	if(er != erSuccess)
		goto exit;

	*lpSortLen = ulSortLen;
	*lppSortData = lpSortData;
exit:
	return er;
}

/**
 * The ECGenericObjectTable::GetSortFlags method gets tablerow flags for a property.
 * 
 * This flag alters the comparison behaviour of the ECKeyTable. This behaviour only needs
 * to be altered for float/double values and strings.
 * 
 * @param[in]	ulPropTag	The PropTag for which to get the flags.
 * @param[out]	lpFlags		The flags needed to properly compare properties for the provided PropTag.
 * 
 * @return Zarafa error code
 */
ECRESULT ECGenericObjectTable::GetSortFlags(unsigned int ulPropTag, unsigned char *lpFlags)
{
    ECRESULT er = erSuccess;
    unsigned int ulFlags = 0;
    
    switch(PROP_TYPE(ulPropTag)) {
        case PT_DOUBLE:
        case PT_APPTIME:
        case PT_R4:
            ulFlags = TABLEROW_FLAG_FLOAT;
            break;
        case PT_STRING8:
        case PT_UNICODE:
			ulFlags = TABLEROW_FLAG_STRING;
			break;
        default:
            break;
    }
    
    *lpFlags = ulFlags;
    
    return er;
}

/**
 * The ECGenericObjectTable::Restrict methode applies a filter to a table
 *
 * The ECGenericObjectTable::Restrict methode applies a filter to a table, reducing 
 * the row set to only those rows matching the specified criteria.
 *
 * @param[in] lpsRestrict
 *				Pointer to a restrictTable structure defining the conditions of the filter. 
 *				Passing NULL in the lpsRestrict parameter removes the current filter.
 *
 * @return Zarafa error code
 */
ECRESULT ECGenericObjectTable::Restrict(struct restrictTable *lpsRestrict)
{
	ECRESULT er = erSuccess;

	pthread_mutex_lock(&m_hLock);

	if(lpsSortOrderArray == NULL) {
		er = SetSortOrder(&sDefaultSortOrder, 0, 0);

		if(er != erSuccess)
			goto exit;
	}

	// No point turning off a restriction that's already off
	if(this->lpsRestrict == NULL && lpsRestrict == NULL) {
		goto exit;
    }

	// Copy the restriction so we can remember it
	if(this->lpsRestrict)
		FreeRestrictTable(this->lpsRestrict);
	this->lpsRestrict = NULL; // turn off restriction

	if(lpsRestrict) {
		er = CopyRestrictTable(NULL, lpsRestrict, &this->lpsRestrict);

		if(er != erSuccess)
			goto exit;
	}

	er = ReloadKeyTable();

	if(er != erSuccess)
		goto exit;

	// Seek to row 0 (according to spec)
	this->SeekRow(BOOKMARK_BEGINNING, 0, NULL);

exit:
	pthread_mutex_unlock(&m_hLock);

	return er;
}

/*
 * Adds a set of rows to the key table, with the correct sort keys
 *
 * This function attempts to add the set of rows passed in lpRows to the table. The rows are added according
 * to sorting currently set and are tested against the current restriction. If bFilter is FALSE, then rows are not
 * tested against the current restriction and always added. A row may also not be added if data for the row is no
 * longer available, in which case the row is silently ignored.
 *
 * The bLoad parameter is not used here, but may be used be subclasses to determine if the rows are being added
 * due to an initial load of the table, or due to a later update.
 *
 * @param[in] lpRows Candidate rows to add to the table
 * @param[out] lpulLoaded Number of rows added to the table
 * @param[in] ulFlags Type of rows being added (May be 0, MSGFLAG_ASSOCIATED, MSGFLAG_DELETED or combination)
 * @param[in] bLoad TRUE if the rows being added are being added for an initial load or reload of the table, false for an update
 */
ECRESULT ECGenericObjectTable::AddRowKey(ECObjectTableList* lpRows, unsigned int *lpulLoaded, unsigned int ulFlags, bool bLoad)
{
	TRACE_INTERNAL(TRACE_ENTRY, "Table call:", "ECGenericObjectTable::AddRowKey", "");

	ECRESULT		er = erSuccess;
	int				i = 0; 
	int				j = 0;
	int				n = 0;
	bool			fMatch = true;
	unsigned int	ulFirstCol = 0;
	unsigned int	ulLoaded = 0;
	bool			bExist;
	bool			fHidden = false;
	SUBRESTRICTIONRESULTS *lpSubResults = NULL;
	ECObjectTableList sQueryRows;

	struct propTagArray	sPropTagArray = {0, 0};
	struct rowSet		*lpRowSet = NULL;
	struct propTagArray	*lpsRestrictPropTagArray = NULL;

	ECObjectTableList::iterator		iterRows;
	sObjectTableKey					sRowItem;
	
	ECCategory		*lpCategory = NULL;
	
	pthread_mutex_lock(&m_hLock);

	if (lpRows->empty()) {
		// nothing todo
		if(lpulLoaded)
			*lpulLoaded = 0;

		goto exit;
	}



	// We want all columns of the sort data, plus all the columns needed for restriction, plus the ID of the row
	if(this->lpsSortOrderArray)
		sPropTagArray.__size = this->lpsSortOrderArray->__size; // sort columns
	else
		sPropTagArray.__size = 0;

	if(this->lpsRestrict) {
		er = GetRestrictPropTags(this->lpsRestrict, NULL, &lpsRestrictPropTagArray);

		if(er != erSuccess)
			goto exit;

		sPropTagArray.__size += lpsRestrictPropTagArray->__size; // restrict columns
	}
	
	sPropTagArray.__size++;	// for PR_INSTANCE_KEY
	sPropTagArray.__size++; // for PR_MESSAGE_FLAGS
	sPropTagArray.__ptr		= new unsigned int[sPropTagArray.__size];
	sPropTagArray.__ptr[n++]= PR_INSTANCE_KEY;
	if(m_ulCategories > 0)
		sPropTagArray.__ptr[n++]= PR_MESSAGE_FLAGS;

	ulFirstCol = n;
	
	// Put all the proptags of the sort columns in a proptag array
	if(lpsSortOrderArray)
		for(i=0;i<this->lpsSortOrderArray->__size;i++)
			sPropTagArray.__ptr[n++] = this->lpsSortOrderArray->__ptr[i].ulPropTag;

	// Same for restrict columns
	// Check if an item already exist
	if(lpsRestrictPropTagArray) {
		for(i=0;i<lpsRestrictPropTagArray->__size;i++)
		{
			bExist = false;
			for(j=0; j < n; j++)
			{
				if(sPropTagArray.__ptr[j] == lpsRestrictPropTagArray->__ptr[i])
					bExist = true;
			}
			if(bExist == false)
				sPropTagArray.__ptr[n++] = lpsRestrictPropTagArray->__ptr[i];
		}

	}

	sPropTagArray.__size = n;

	for(iterRows = lpRows->begin(); iterRows != lpRows->end(); )
	{
		sQueryRows.clear();

		for(i=0; i < 20 && iterRows != lpRows->end(); i++) {
			sQueryRows.push_back(*iterRows);
			iterRows++;
		}

		// Now, query the database for the actual data
		er = m_lpfnQueryRowData(this, NULL, lpSession, &sQueryRows, &sPropTagArray, m_lpObjectData, &lpRowSet, true, lpsRestrictPropTagArray ? false : true /* FIXME */);
		if(er != erSuccess)
			goto exit;
			
		if(this->lpsRestrict) {
			er = RunSubRestrictions(lpSession, m_lpObjectData, lpsRestrict, &sQueryRows, m_locale, &lpSubResults);
			if(er != erSuccess)
				goto exit;
		}

		// Send all this data to the internal key table
		for(i=0; i<lpRowSet->__size; i++) {
			lpCategory = NULL;

			if (lpRowSet->__ptr[i].__ptr[0].ulPropTag != PR_INSTANCE_KEY) // Row completely not found
				continue;

			// is PR_INSTANCE_KEY
			memcpy(&sRowItem.ulObjId, lpRowSet->__ptr[i].__ptr[0].Value.bin->__ptr, sizeof(ULONG));
			memcpy(&sRowItem.ulOrderId, lpRowSet->__ptr[i].__ptr[0].Value.bin->__ptr+sizeof(ULONG), sizeof(ULONG));

			// Match the row with the restriction, if any
			if(this->lpsRestrict) {
				MatchRowRestrict(lpSession->GetSessionManager()->GetCacheManager(), &lpRowSet->__ptr[i], this->lpsRestrict, lpSubResults, m_locale, &fMatch);

				if(fMatch == false) {
					// this row isn't in the table, as it does not match the restrict criteria. Remove it as if it had
					// been deleted if it was already in the table.
					DeleteRow(sRowItem, ulFlags);
					
					RemoveCategoryAfterRemoveRow(sRowItem, ulFlags);
					continue;
				}
			}

			if(m_ulCategories > 0) {
				bool bUnread = false;
				
				if((lpRowSet->__ptr[i].__ptr[1].Value.ul & MSGFLAG_READ) == 0)
					bUnread = true;

				// Update category for this row if required, and send notification if required
				AddCategoryBeforeAddRow(sRowItem, lpRowSet->__ptr[i].__ptr+ulFirstCol, lpsSortOrderArray->__size, ulFlags, bUnread, &fHidden, &lpCategory);
			}

			// Put the row into the key table and send notification if required
			AddRow(sRowItem, lpRowSet->__ptr[i].__ptr+ulFirstCol, lpsSortOrderArray->__size, ulFlags, fHidden, lpCategory);

			// Loaded one row
			ulLoaded++;
		}

		if(lpSubResults) {
			FreeSubRestrictionResults(lpSubResults);
			lpSubResults = NULL;
		}

		if(lpRowSet) {
			FreeRowSet(lpRowSet, true);
			lpRowSet = NULL;
		}
	}

	if(lpulLoaded)
		*lpulLoaded = ulLoaded;

exit:
	pthread_mutex_unlock(&m_hLock);

	if(lpSubResults)
		FreeSubRestrictionResults(lpSubResults);

	if(lpRowSet)
		FreeRowSet(lpRowSet, true);

	if(lpsRestrictPropTagArray != NULL && lpsRestrictPropTagArray->__ptr)
		delete [] lpsRestrictPropTagArray->__ptr;

	if(lpsRestrictPropTagArray)
		delete lpsRestrictPropTagArray;

	if(sPropTagArray.__ptr)
		delete [] sPropTagArray.__ptr;

	return er;
}

// Actually add a row to the table
ECRESULT ECGenericObjectTable::AddRow(sObjectTableKey sRowItem, struct propVal *lpProps, unsigned int cProps, unsigned int ulFlags, bool fHidden, ECCategory *lpCategory)
{
    ECRESULT er = erSuccess;
    ECKeyTable::UpdateType ulAction;
    sObjectTableKey sPrevRow;

	UpdateKeyTableRow(lpCategory, &sRowItem, lpProps, cProps, fHidden, &sPrevRow, &ulAction);

    // Send notification if required
    if(ulAction && !fHidden && (ulFlags & OBJECTTABLE_NOTIFY)) {
        er = AddTableNotif(ulAction, sRowItem, &sPrevRow);
        if(er != erSuccess)
            goto exit;
    }

exit:
    return er;
}

// Actually remove a row from the table
ECRESULT ECGenericObjectTable::DeleteRow(sObjectTableKey sRow, unsigned int ulFlags)
{
    ECRESULT		er = erSuccess;
    ECKeyTable::UpdateType ulAction;

    // Delete the row from the key table    
    er = lpKeyTable->UpdateRow(ECKeyTable::TABLE_ROW_DELETE, &sRow, 0, NULL, NULL, NULL, NULL, false, &ulAction);
    if(er != erSuccess)
        goto exit;
    
    // Send notification if required
    if((ulFlags & OBJECTTABLE_NOTIFY) && ulAction == ECKeyTable::TABLE_ROW_DELETE ) {
        AddTableNotif(ulAction, sRow, NULL);
    }
exit:    
    return er;
}

// Add a table notification by getting row data and sending it
ECRESULT ECGenericObjectTable::AddTableNotif(ECKeyTable::UpdateType ulAction, sObjectTableKey sRowItem, sObjectTableKey *lpsPrevRow)
{
    ECRESULT er = erSuccess;
    std::list<sObjectTableKey> lstItems;
	struct rowSet		*lpRowSetNotif = NULL;
    
    if(ulAction == ECKeyTable::TABLE_ROW_ADD || ulAction == ECKeyTable::TABLE_ROW_MODIFY) {
        lstItems.push_back(sRowItem);
        
        er = m_lpfnQueryRowData(this, NULL, lpSession, &lstItems, this->lpsPropTagArray, m_lpObjectData, &lpRowSetNotif, true, true);
        if(er != erSuccess)
            goto exit;
            
        if(lpRowSetNotif->__size != 1) {
            er = ZARAFA_E_NOT_FOUND;
            goto exit;
        }

        lpSession->AddNotificationTable(ulAction, m_ulObjType, m_ulTableId, &sRowItem, lpsPrevRow, &lpRowSetNotif->__ptr[0]);
    } else if(ulAction == ECKeyTable::TABLE_ROW_DELETE) {
        lpSession->AddNotificationTable(ulAction, m_ulObjType, m_ulTableId, &sRowItem, NULL, NULL);
    } else {
        er = ZARAFA_E_NOT_FOUND;
        goto exit;
    }
        
exit:
    if(lpRowSetNotif)
        FreeRowSet(lpRowSetNotif, true);
    return er;
}

ECRESULT ECGenericObjectTable::QueryRows(struct soap *soap, unsigned int ulRowCount, unsigned int ulFlags, struct rowSet **lppRowSet)
{
	// Retrieve the keyset from our KeyTable, and use that to retrieve the other columns
	// specified by SetColumns

	ECRESULT		er = erSuccess;
	struct rowSet	*lpRowSet = NULL;

	ECObjectTableList	ecRowList;

	pthread_mutex_lock(&m_hLock);

    er = Populate();
    if(er != erSuccess)
        goto exit;

	if(lpsSortOrderArray == NULL) {
		er = SetSortOrder(&sDefaultSortOrder, 0, 0);

		if(er != erSuccess)
			goto exit;
	}

	// Get the keys per row
	er = lpKeyTable->QueryRows(ulRowCount, &ecRowList, false, ulFlags);

	if(er != erSuccess)
		goto exit;

	ASSERT(ecRowList.size() <= this->mapObjects.size() + this->m_mapCategories.size());

	if(ecRowList.empty()) {
		lpRowSet = s_alloc<rowSet>(soap);
		lpRowSet->__size = 0;
		lpRowSet->__ptr = NULL;
	} else {
		
		// We now have the ordering of the rows, all we have to do now is get the data. 
		er = m_lpfnQueryRowData(this, soap, lpSession, &ecRowList, this->lpsPropTagArray, m_lpObjectData, &lpRowSet, true, true);

	}

	if(er != erSuccess)
		goto exit;
		
	*lppRowSet = lpRowSet;

exit:
	pthread_mutex_unlock(&m_hLock);

	return er;
}

ECRESULT ECGenericObjectTable::CreateBookmark(unsigned int* lpulbkPosition)
{
	ECRESULT		er = erSuccess;
	
	pthread_mutex_lock(&m_hLock);

	er = lpKeyTable->CreateBookmark(lpulbkPosition);
	if(er != erSuccess)
		goto exit;

exit:
	pthread_mutex_unlock(&m_hLock);

	return er;
}

ECRESULT ECGenericObjectTable::FreeBookmark(unsigned int ulbkPosition)
{
	ECRESULT		er = erSuccess;
	
	pthread_mutex_lock(&m_hLock);

	er = lpKeyTable->FreeBookmark(ulbkPosition);
	if(er != erSuccess)
		goto exit;

exit:
	pthread_mutex_unlock(&m_hLock);

	return er;
}

// Expand the category identified by sInstanceKey
ECRESULT ECGenericObjectTable::ExpandRow(struct soap *soap, xsd__base64Binary sInstanceKey, unsigned int ulRowCount, unsigned int ulFlags, struct rowSet **lppRowSet, unsigned int *lpulRowsLeft)
{
    ECRESULT er = erSuccess;
    sObjectTableKey sKey;
    sObjectTableKey sPrevRow;
    ECCategoryMap::iterator iterCategory;
    ECCategory *lpCategory = NULL;
    ECObjectTableList lstUnhidden;
    unsigned int ulRowsLeft = 0;
    struct rowSet *lpRowSet = NULL;
    
    pthread_mutex_lock(&m_hLock);
    
    er = Populate();
    if(er != erSuccess)
        goto exit;

    if(sInstanceKey.__size != sizeof(sObjectTableKey)) {
        er = ZARAFA_E_INVALID_PARAMETER;
        goto exit;
    }

    sKey.ulObjId = *((unsigned int *)sInstanceKey.__ptr);
    sKey.ulOrderId = *((unsigned int *)sInstanceKey.__ptr+1);
    
    iterCategory = m_mapCategories.find(sKey);
    if(iterCategory == m_mapCategories.end()) {
        er = ZARAFA_E_NOT_FOUND;
        goto exit;
    }

    lpCategory = iterCategory->second;

    // Unhide all rows under this category
    er = lpKeyTable->UnhideRows(&sKey, &lstUnhidden);
    if(er != erSuccess)
        goto exit;

    // Only return a maximum of ulRowCount rows
    if(ulRowCount < lstUnhidden.size()) {
        ulRowsLeft = lstUnhidden.size() - ulRowCount;
        lstUnhidden.resize(ulRowCount);
        
        // Put the keytable cursor just after the rows we will be returning, so the next queryrows() would return the remaining rows
        lpKeyTable->SeekRow(1, -ulRowsLeft, NULL);
    }
    
    // Get the row data to return, if required
    if(lppRowSet) {
        if(lstUnhidden.empty()){
    		lpRowSet = s_alloc<rowSet>(soap);
    		lpRowSet->__size = 0;
    		lpRowSet->__ptr = NULL;
    	} else {
    	    // Get data for unhidden rows
    		er = m_lpfnQueryRowData(this, soap, lpSession, &lstUnhidden, this->lpsPropTagArray, m_lpObjectData, &lpRowSet, true, true);
    	}

    	if(er != erSuccess)
	    	goto exit;
    }

    lpCategory->m_fExpanded = true;

    if(lppRowSet)
        *lppRowSet = lpRowSet;
    if(lpulRowsLeft)
        *lpulRowsLeft = ulRowsLeft;
    
exit:

    pthread_mutex_unlock(&m_hLock);
    
    return er;
}

// Collapse the category row identified by sInstanceKey
ECRESULT ECGenericObjectTable::CollapseRow(xsd__base64Binary sInstanceKey, unsigned int ulFlags, unsigned int *lpulRows)
{
    ECRESULT er = erSuccess;
    sObjectTableKey sKey;
    sObjectTableKey sPrevRow;
    ECCategoryMap::iterator iterCategory;
    ECCategory *lpCategory = NULL;
    ECObjectTableList lstHidden;
    ECObjectTableList::iterator iterHidden;

	pthread_mutex_lock(&m_hLock);
    
    if(sInstanceKey.__size != sizeof(sObjectTableKey)) {
        er = ZARAFA_E_INVALID_PARAMETER;
        goto exit;
    }
    
    er = Populate();
    if(er != erSuccess)
        goto exit;

    sKey.ulObjId = *((unsigned int *)sInstanceKey.__ptr);
    sKey.ulOrderId = *((unsigned int *)sInstanceKey.__ptr+1);
    
    iterCategory = m_mapCategories.find(sKey);
    if(iterCategory == m_mapCategories.end()) {
        er = ZARAFA_E_NOT_FOUND;
        goto exit;
    }

    lpCategory = iterCategory->second;

    // Hide the rows under this category
    er = lpKeyTable->HideRows(&sKey, &lstHidden);
    if(er != erSuccess)
        goto exit;
    
    // Mark the category as collapsed
    lpCategory->m_fExpanded = false;
    
    // Loop through the hidden rows to see if we have hidden any categories. If so, mark them as
    // collapsed
    for(iterHidden = lstHidden.begin(); iterHidden != lstHidden.end(); iterHidden++) {
        iterCategory = m_mapCategories.find(*iterHidden);
        
        if(iterCategory != m_mapCategories.end()) {
            iterCategory->second->m_fExpanded = false;
        }
    }

    if(lpulRows)
        *lpulRows = lstHidden.size();

exit:    
	pthread_mutex_unlock(&m_hLock);

    return er;
}

ECRESULT ECGenericObjectTable::GetCollapseState(struct soap *soap, struct xsd__base64Binary sBookmark, struct xsd__base64Binary *lpsCollapseState)
{
    ECRESULT er = erSuccess;
    ECCategoryMap::iterator iterCategory;
    struct collapseState sCollapseState;
    int n = 0;
    std::ostringstream os;
    sObjectTableKey sKey;
    struct rowSet *lpsRowSet = NULL;
    
    struct soap xmlsoap;
    soap_init(&xmlsoap);

	pthread_mutex_lock(&m_hLock);
    
    er = Populate();
    if(er != erSuccess)
        goto exit;

    memset(&sCollapseState, 0, sizeof(sCollapseState));

    // Generate a binary collapsestate which is simply an XML stream of all categories with their collapse state
    sCollapseState.sCategoryStates.__size = m_mapCategories.size();
    sCollapseState.sCategoryStates.__ptr = s_alloc<struct categoryState>(soap, sCollapseState.sCategoryStates.__size);

    memset(sCollapseState.sCategoryStates.__ptr, 0, sizeof(struct categoryState) * sCollapseState.sCategoryStates.__size);

    for(iterCategory = m_mapCategories.begin(); iterCategory != m_mapCategories.end(); iterCategory++) {
        sCollapseState.sCategoryStates.__ptr[n].fExpanded = iterCategory->second->m_fExpanded;
        sCollapseState.sCategoryStates.__ptr[n].sProps.__ptr = s_alloc<struct propVal>(soap, iterCategory->second->m_cProps);
        memset(sCollapseState.sCategoryStates.__ptr[n].sProps.__ptr, 0, sizeof(struct propVal) * iterCategory->second->m_cProps);
        for(unsigned int i=0; i<iterCategory->second->m_cProps; i++) {
            er = CopyPropVal(&iterCategory->second->m_lpProps[i], &sCollapseState.sCategoryStates.__ptr[n].sProps.__ptr[i], soap);
            if (er != erSuccess)
                goto exit;
        }
        sCollapseState.sCategoryStates.__ptr[n].sProps.__size = iterCategory->second->m_cProps;
        n++;
    }

    // We also need to save the sort keys for the given bookmark, so that we can return a bookmark when SetCollapseState is called
    if(sBookmark.__size == 8) {
        sKey.ulObjId = *((unsigned int *)sBookmark.__ptr);
        sKey.ulOrderId = *((unsigned int *)sBookmark.__ptr+1);
        
        // Go the the row requested
        if(lpKeyTable->SeekId(&sKey) == erSuccess) {
            // If the row exists, we simply get the data from the properties of this row, including all properties used
            // in the current sort.
            ECObjectTableList list;
            
            list.push_back(sKey);
            
            er = m_lpfnQueryRowData(this, &xmlsoap, lpSession, &list, lpsPropTagArray, m_lpObjectData, &lpsRowSet, false, true);
            if(er != erSuccess)
                goto exit;
                
            // Copy row 1 from rowset into our bookmark props.
            sCollapseState.sBookMarkProps = lpsRowSet->__ptr[0];
            
            // Free of lpsRowSet coupled to xmlsoap so not explicitly needed
        }
    }
    
	soap_set_mode(&xmlsoap, SOAP_XML_TREE | SOAP_C_UTFSTRING);
    xmlsoap.os = &os;
    
    soap_serialize_collapseState(&xmlsoap, &sCollapseState);
    soap_begin_send(&xmlsoap);
    soap_put_collapseState(&xmlsoap, &sCollapseState, "CollapseState", NULL);
    soap_end_send(&xmlsoap);
    
    // os.str() now contains serialized objects, copy into return structure
    lpsCollapseState->__size = os.str().size();
    lpsCollapseState->__ptr = s_alloc<unsigned char>(soap, os.str().size());
    memcpy(lpsCollapseState->__ptr, os.str().c_str(), os.str().size());

exit:
    soap_end(&xmlsoap);
    soap_done(&xmlsoap);
	pthread_mutex_unlock(&m_hLock);

    return er;
}

ECRESULT ECGenericObjectTable::SetCollapseState(struct xsd__base64Binary sCollapseState, unsigned int *lpulBookmark)
{
    ECRESULT er = erSuccess;
    ECCategoryMap::iterator iterCategory;
    struct soap xmlsoap;
    struct collapseState *lpCollapseState = NULL;
    std::istringstream is(std::string((const char *)sCollapseState.__ptr, sCollapseState.__size));
    unsigned int *lpSortLen = NULL;
    unsigned char **lpSortData = NULL;
    unsigned char *lpSortFlags = NULL;
	sObjectTableKey sKey;
    struct xsd__base64Binary sInstanceKey;

	pthread_mutex_lock(&m_hLock);
    
    lpCollapseState = new collapseState;
    soap_init(&xmlsoap);
    
    er = Populate();
    if(er != erSuccess)
        goto exit;

    // The collapse state is the serialized collapse state as returned by GetCollapseState(), which we need to parse here
	soap_set_mode(&xmlsoap, SOAP_XML_TREE | SOAP_C_UTFSTRING);
    xmlsoap.is = &is;
    
    soap_default_collapseState(&xmlsoap, lpCollapseState);
    soap_begin_recv(&xmlsoap);
    soap_get_collapseState(&xmlsoap, lpCollapseState, "CollapseState", NULL);
    
    if(xmlsoap.error) {
        er = ZARAFA_E_DATABASE_ERROR;
        goto exit;
    }
    
    // lpCollapseState now contains the collapse state for all categories, apply them now
    
    for(unsigned int i=0; i < lpCollapseState->sCategoryStates.__size; i++) {
        lpSortLen = new unsigned int[lpCollapseState->sCategoryStates.__ptr[i].sProps.__size];
        lpSortData = new unsigned char* [lpCollapseState->sCategoryStates.__ptr[i].sProps.__size];
        lpSortFlags = new unsigned char [lpCollapseState->sCategoryStates.__ptr[i].sProps.__size];
    
        memset(lpSortData, 0, lpCollapseState->sCategoryStates.__ptr[i].sProps.__size * sizeof(unsigned char *));
        
        // Get the binary sortkeys for all properties
        for(int n=0; n < lpCollapseState->sCategoryStates.__ptr[i].sProps.__size; n++) {
            if(GetBinarySortKey(&lpCollapseState->sCategoryStates.__ptr[i].sProps.__ptr[n], &lpSortLen[n], &lpSortData[n]) != erSuccess)
                goto next;
                
            if(GetSortFlags(lpCollapseState->sCategoryStates.__ptr[i].sProps.__ptr[n].ulPropTag, &lpSortFlags[n]) != erSuccess)
                goto next;
        }

        // Find the category and expand or collapse it. If it's not there anymore, just ignore it.
        if(lpKeyTable->Find(lpCollapseState->sCategoryStates.__ptr[i].sProps.__size, (int *)lpSortLen, lpSortData, lpSortFlags, &sKey) == erSuccess) {

            sInstanceKey.__size = 8;
			sInstanceKey.__ptr = (unsigned char *)&sKey;
            
            if(lpCollapseState->sCategoryStates.__ptr[i].fExpanded) {
                ExpandRow(NULL, sInstanceKey, 0, 0, NULL, NULL);
            } else {
                CollapseRow(sInstanceKey, 0, NULL);
            }
        }
next:        
        delete [] lpSortLen;
        for(int j=0;j<lpCollapseState->sCategoryStates.__ptr[i].sProps.__size;j++)
            if(lpSortData[j])
                delete [] lpSortData[j];
        delete [] lpSortData;
        delete [] lpSortFlags;
        
        lpSortLen = NULL;
        lpSortData = NULL;
        lpSortFlags = NULL;
    }
    
    // There is also a row stored in the collapse state which we have to create a bookmark at and return that. If it is not found,
    // we return a bookmark to the nearest next row.
    if (lpCollapseState->sBookMarkProps.__size > 0) {
        int n;
        lpSortLen = new unsigned int[lpCollapseState->sBookMarkProps.__size];
        lpSortData = new unsigned char* [lpCollapseState->sBookMarkProps.__size];
        lpSortFlags = new unsigned char [lpCollapseState->sBookMarkProps.__size];
        
        memset(lpSortData, 0, lpCollapseState->sBookMarkProps.__size * sizeof(unsigned char *));
        
        for(n=0; n < lpCollapseState->sBookMarkProps.__size; n++) {
            if(GetBinarySortKey(&lpCollapseState->sBookMarkProps.__ptr[n], &lpSortLen[n], &lpSortData[n]) != erSuccess)
                break;
            
            if(GetSortFlags(lpCollapseState->sBookMarkProps.__ptr[n].ulPropTag, &lpSortFlags[n]) != erSuccess)
                break;
        }
    
        // If an error occurred in the previous loop, just ignore the whole bookmark thing, just return bookmark 0 (BOOKMARK_BEGINNING)    
        if(n == lpCollapseState->sBookMarkProps.__size) {
            lpKeyTable->LowerBound(lpCollapseState->sBookMarkProps.__size, (int *)lpSortLen, lpSortData, lpSortFlags);
            
            lpKeyTable->CreateBookmark(lpulBookmark);
        }

        delete [] lpSortLen;
        lpSortLen = NULL;
        for(int j=0 ; j<lpCollapseState->sBookMarkProps.__size; j++)
			if (lpSortData[j])
				delete [] lpSortData[j];
        delete [] lpSortData;
        lpSortData = NULL;
        delete [] lpSortFlags;
        lpSortFlags = NULL;
    }
    
    // We don't generate notifications for this event, just like ExpandRow and CollapseRow. You just need to reload the table yourself.
    soap_end_recv(&xmlsoap);
    
    
exit:
    soap_end(&xmlsoap);
    soap_done(&xmlsoap);
	pthread_mutex_unlock(&m_hLock);

    if(lpCollapseState)
        delete lpCollapseState;
    
    if(lpSortLen)
        delete [] lpSortLen;
        
    if(lpSortData)
        delete [] lpSortData;
        
    if(lpSortFlags)
        delete [] lpSortFlags;
        
    return er;
}

ECRESULT ECGenericObjectTable::UpdateRow(unsigned int ulType, unsigned int ulObjId, unsigned int ulFlags)
{
    ECRESULT er = erSuccess;
    std::list<unsigned int> lstObjId;
    
    lstObjId.push_back(ulObjId);
    
    er = UpdateRows(ulType, &lstObjId, ulFlags, false);
    
    return er;
}

/**
 * Load a set of rows into the table
 *
 * This is called to populate a table initially, it is functionally equivalent to calling UpdateRow() repeatedly
 * for each item in lstObjId with ulType set to ECKeyTable::TABLE_ROW_ADD.
 *
 * @param lstObjId List of hierarchy IDs for the objects to load
 * @param ulFlags 0, MSGFLAG_DELETED, MAPI_ASSOCIATED or combination
 */
ECRESULT ECGenericObjectTable::LoadRows(std::list<unsigned int> *lstObjId, unsigned int ulFlags)
{
	return UpdateRows(ECKeyTable::TABLE_ROW_ADD, lstObjId, ulFlags, true);
}

/**
 * Update one or more rows in a table
 *
 * This function adds, modifies or removes objects from a table. The normal function of this is that normally
 * either multiple objects are added, or a single object is removed or updated. The result of such an update can
 * be complex, for example adding an item to a table may cause multiple rows to be added when using categorization
 * or multi-valued properties. In the same way, an update may generate multiple notifications if category headers are
 * involved or when the update modifies the sorting position of the row.
 *
 * Rows are also checked for read permissions here before being added to the table.
 *
 * The bLoad parameter is not used in the ECGenericObjectTable implementation, but simply passed to AddRowKey to indicate
 * whether the rows are being updated due to a change or due to initial loading. The parameter is also not used in AddRowKey
 * but can be used by subclasses to generate different behaviour on the initial load compared to later updates.
 *
 * @param ulType ECKeyTable::TABLE_ROW_ADD, TABLE_ROW_DELETE or TABLE_ROW_MODIFY
 * @param lstObjId List of objects to add, modify or delete
 * @param ulFlags Flags for the objects in lstObjId (0, MSGFLAG_DELETED, MAPI_ASSOCIATED)
 * @param bLoad Indicates that this is the initial load or reload of the table, and not an update
 */
ECRESULT ECGenericObjectTable::UpdateRows(unsigned int ulType, std::list<unsigned int> *lstObjId, unsigned int ulFlags, bool bLoad)
{
	ECRESULT				er = erSuccess;
	unsigned int			ulRead = 0;
	unsigned int			cMVOld = 0,
							cMVNew = 1;
	unsigned int			i;
	
	std::list<unsigned int>::iterator iterObjId;
	std::list<unsigned int> lstFilteredIds;
	
	ECObjectTableList		ecRowsItem;
	ECObjectTableList		ecRowsDeleted;

	ECObjectTableList::iterator iterRows;
	ECObjectTableMap::iterator	iterMapObject;
	ECObjectTableMap::iterator	iterToDelete;

	sObjectTableKey		sRow;
	
	pthread_mutex_lock(&m_hLock);

	// Perform security checks for this object
	switch(ulType) {
    case ECKeyTable::TABLE_CHANGE:
        // Accept table change in all cases
        break;
    case ECKeyTable::TABLE_ROW_MODIFY:
    case ECKeyTable::TABLE_ROW_ADD:
        // Filter out any item we cannot access (for example, in search-results tables)
        for(iterObjId = lstObjId->begin(); iterObjId != lstObjId->end(); iterObjId++) {
        	if(CheckPermissions(*iterObjId) == erSuccess)
        	    lstFilteredIds.push_back(*iterObjId);
        }

        // Use our filtered list now
        lstObjId = &lstFilteredIds;
    	break;
    case ECKeyTable::TABLE_ROW_DELETE:
	    // You may always delete a row
        break;
    }

	if(lpsSortOrderArray == NULL) {
		er = SetSortOrder(&sDefaultSortOrder, 0, 0);

		if(er != erSuccess)
			goto exit;
	}

	// Update a row in the keyset as having changed. Get the data from the DB and send it to the KeyTable.

	switch(ulType) {
	case ECKeyTable::TABLE_ROW_DELETE:
		// Delete the object ID from our object list, and all items with that object ID (including various order IDs)
		for(iterObjId = lstObjId->begin(); iterObjId != lstObjId->end(); iterObjId++) {
            iterMapObject = this->mapObjects.find(sObjectTableKey(*iterObjId, 0));
            while(iterMapObject != this->mapObjects.end()) {
                if(iterMapObject->first.ulObjId == *iterObjId)
                    ecRowsItem.push_back(iterMapObject->first);
                else if(iterMapObject->first.ulObjId != *iterObjId)
                    break;

                iterMapObject++;
            }
            
            for(iterRows = ecRowsItem.begin(); iterRows != ecRowsItem.end(); iterRows++)
            {
                this->mapObjects.erase(*iterRows);
                
                // Delete the object from the active keyset
                DeleteRow(*iterRows, ulFlags);
                
                RemoveCategoryAfterRemoveRow(*iterRows, ulFlags);
            }
        }
		break;

	case ECKeyTable::TABLE_ROW_MODIFY:
	case ECKeyTable::TABLE_ROW_ADD:
	    for(iterObjId = lstObjId->begin(); iterObjId != lstObjId->end(); iterObjId++) {
            // Add the object to our list of objects
            ecRowsItem.push_back(sObjectTableKey(*iterObjId, 0));

            if(IsMVSet() == true) {
                // get new mvprop count
                er = GetMVRowCount(*iterObjId, &cMVNew);
                if(er != erSuccess){
                    ASSERT(FALSE);// What now???
                }

                // get old mvprops count
                cMVOld = 0;
                iterMapObject = this->mapObjects.find(sObjectTableKey(*iterObjId, 0));
                while(iterMapObject != this->mapObjects.end())
                {
                    if(iterMapObject->first.ulObjId == *iterObjId)
                    {
                        cMVOld++;
                        if(cMVOld > cMVNew && (ulFlags&OBJECTTABLE_NOTIFY) == OBJECTTABLE_NOTIFY) {

                            iterToDelete = iterMapObject;
                            iterMapObject--;
                            sRow = iterToDelete->first;
                            //Delete of map
                            this->mapObjects.erase(iterToDelete->first);
                            
                            DeleteRow(sRow, ulFlags);
                            
                            RemoveCategoryAfterRemoveRow(sRow, ulFlags);
                        }//if(cMVOld > cMVNew)
                    }else if(iterMapObject->first.ulObjId != *iterObjId)
                        break;

                    iterMapObject++;
                }
                
                sRow = sObjectTableKey(*iterObjId, 0);
                for(i=1; i<cMVNew; i++) {// 0 already added
                    sRow.ulOrderId = i;
                    ecRowsItem.push_back(sRow);
                }
            }
        }
        
        // Remember that the specified row is available		
        for(iterRows = ecRowsItem.begin(); iterRows != ecRowsItem.end(); iterRows++)
            this->mapObjects[*iterRows] = 1;
            
		// Add/modify the key in the keytable
		er = AddRowKey(&ecRowsItem, &ulRead, ulFlags, bLoad);
		if(er != erSuccess)
			goto exit;

		break;
	case ECKeyTable::TABLE_CHANGE:
		// The whole table needs to be reread
		this->Clear();
		er = this->Load();

		lpSession->AddNotificationTable(ulType, m_ulObjType, m_ulTableId, NULL, NULL, NULL);

		break;
	}
	
exit:
	pthread_mutex_unlock(&m_hLock);

	return er;
}

ECRESULT ECGenericObjectTable::GetRestrictPropTagsRecursive(struct restrictTable *lpsRestrict, list<ULONG> *lpPropTags, ULONG ulLevel)
{
	ECRESULT		er = erSuccess;
	unsigned int	i=0;

	if(ulLevel > RESTRICT_MAX_DEPTH) {
		er = ZARAFA_E_TOO_COMPLEX;
		goto exit;
	}

	switch(lpsRestrict->ulType) {
	case RES_COMMENT:
	    er = GetRestrictPropTagsRecursive(lpsRestrict->lpComment->lpResTable, lpPropTags, ulLevel+1);
	    if(er != erSuccess)
	        goto exit;
	    break;
	    
	case RES_OR:
		for(i=0;i<lpsRestrict->lpOr->__size;i++) {
			er = GetRestrictPropTagsRecursive(lpsRestrict->lpOr->__ptr[i], lpPropTags, ulLevel+1);

			if(er != erSuccess)
				goto exit;
		}
		break;	
		
	case RES_AND:
		for(i=0;i<lpsRestrict->lpAnd->__size;i++) {
			er = GetRestrictPropTagsRecursive(lpsRestrict->lpAnd->__ptr[i], lpPropTags, ulLevel+1);

			if(er != erSuccess)
				goto exit;
		}
		break;	

	case RES_NOT:
		er = GetRestrictPropTagsRecursive(lpsRestrict->lpNot->lpNot, lpPropTags, ulLevel+1);
		if(er != erSuccess)
			goto exit;
		break;

	case RES_CONTENT:
		lpPropTags->push_back(lpsRestrict->lpContent->ulPropTag);
		break;

	case RES_PROPERTY:
		if(PROP_ID(lpsRestrict->lpProp->ulPropTag) == PROP_ID(PR_ANR))
			lpPropTags->insert(lpPropTags->end(), sANRProps, sANRProps + arraySize(sANRProps));
			
		else {
			lpPropTags->push_back(lpsRestrict->lpProp->lpProp->ulPropTag);
			lpPropTags->push_back(lpsRestrict->lpProp->ulPropTag);
		}
		break;

	case RES_COMPAREPROPS:
		lpPropTags->push_back(lpsRestrict->lpCompare->ulPropTag1);
		lpPropTags->push_back(lpsRestrict->lpCompare->ulPropTag2);
		break;

	case RES_BITMASK:
		lpPropTags->push_back(lpsRestrict->lpBitmask->ulPropTag);
		break;

	case RES_SIZE:
		lpPropTags->push_back(lpsRestrict->lpSize->ulPropTag);
		break;

	case RES_EXIST:
		lpPropTags->push_back(lpsRestrict->lpExist->ulPropTag);
		break;

	case RES_SUBRESTRICTION:
	    lpPropTags->push_back(PR_ENTRYID); // we need the entryid in subrestriction searches, because we need to know which object to subsearch
		break;
	}

exit:
	return er;
}

/**
 * Generate a list of all properties required to evaluate a restriction
 *
 * The list of properties returned are the maximum set of properties required to evaluate the given restriction. Additionally
 * a list of properties can be added to the front of the property set. If the property is required both through the prefix list
 * and through the restriction, it is included only once in the property list.
 *
 * The order of the first N properties in the returned proptag array are guaranteed to be equal to the N items in lstPrefix
 *
 * @param[in] lpsRestrict Restriction tree to evaluate
 * @param[in] lstPrefix NULL or list of property tags to prefix
 * @param[out] lppPropTags PropTagArray with proptags from lpsRestrict and lstPrefix
 * @return ECRESULT
 */
ECRESULT ECGenericObjectTable::GetRestrictPropTags(struct restrictTable *lpsRestrict, std::list<ULONG> *lstPrefix, struct propTagArray **lppPropTags)
{
	ECRESULT			er = erSuccess;
	struct propTagArray *lpPropTagArray;

	std::list<ULONG> 	lstPropTags;

	// Just go through all the properties, adding the properties one-by-one 
	er = GetRestrictPropTagsRecursive(lpsRestrict, &lstPropTags, 0);
	if (er != erSuccess)
		goto exit;

	// Sort and unique-ize the properties (order is not important in the returned array)
	lstPropTags.sort();
	lstPropTags.unique();
	
	// Prefix if needed
	if(lstPrefix)
		lstPropTags.insert(lstPropTags.begin(), lstPrefix->begin(), lstPrefix->end());

	lpPropTagArray = new propTagArray;
	// Put the data into an array
	lpPropTagArray->__size = lstPropTags.size();
	lpPropTagArray->__ptr = new unsigned int [lpPropTagArray->__size];

	copy(lstPropTags.begin(), lstPropTags.end(), lpPropTagArray->__ptr);
	*lppPropTags = lpPropTagArray;

exit:
	return er;
}

// Simply matches the restriction with the given data. Make sure you pass all the data
// needed for the restriction in lpPropVals. (missing columns do not match, ever.)
//

ECRESULT ECGenericObjectTable::MatchRowRestrict(ECCacheManager* lpCacheManager, propValArray *lpPropVals, restrictTable *lpsRestrict, SUBRESTRICTIONRESULTS *lpSubResults, const ECLocale &locale, bool *lpfMatch, unsigned int *lpulSubRestriction)
{
	ECRESULT		er = erSuccess;
	bool			fMatch = false;
	unsigned int	i = 0;
	int				lCompare = 0;
	unsigned int	ulSize = 0;
	struct propVal	*lpProp = NULL;
	struct propVal	*lpProp2 = NULL;

	char* lpSearchString;
	char* lpSearchData;
	unsigned int ulSearchDataSize;
	unsigned int ulSearchStringSize;
	ULONG ulPropType;
	ULONG ulFuzzyLevel;
	unsigned int ulScan, ulPos;
	unsigned int ulSubRestrict = 0;
	SUBRESTRICTIONRESULT::iterator iterSubResult;
	entryId sEntryId;
	unsigned int ulResId = 0;
	unsigned int ulPropTagRestrict;
	unsigned int ulPropTagValue;
	
	if(lpulSubRestriction == NULL) // called externally
	    lpulSubRestriction = &ulSubRestrict;
	    
	switch(lpsRestrict->ulType) {
	case RES_COMMENT:
	    if(lpsRestrict->lpComment == NULL) {
	        er = ZARAFA_E_INVALID_TYPE;
	        goto exit;
        }
        er = MatchRowRestrict(lpCacheManager, lpPropVals, lpsRestrict->lpComment->lpResTable, lpSubResults, locale, &fMatch, lpulSubRestriction);
        break;
        
	case RES_OR:
		if(lpsRestrict->lpOr == NULL) {
			er = ZARAFA_E_INVALID_TYPE;
			goto exit;
		}

		fMatch = false;

		for(i=0;i<lpsRestrict->lpOr->__size;i++) {
			er = MatchRowRestrict(lpCacheManager, lpPropVals, lpsRestrict->lpOr->__ptr[i], lpSubResults, locale, &fMatch, lpulSubRestriction);

			if(er != erSuccess)
				goto exit;

			if(fMatch) // found a restriction in an OR which matches, ignore the rest of the query
				break;
		}
		break;
	case RES_AND:
		if(lpsRestrict->lpAnd == NULL) {
			er = ZARAFA_E_INVALID_TYPE;
			goto exit;
		}
		
		fMatch = true;

		for(i=0;i<lpsRestrict->lpAnd->__size;i++) {
			er = MatchRowRestrict(lpCacheManager, lpPropVals, lpsRestrict->lpAnd->__ptr[i], lpSubResults, locale, &fMatch, lpulSubRestriction);

			if(er != erSuccess)
				goto exit;

			if(!fMatch) // found a restriction in an AND which doesn't match, ignore the rest of the query
				break;
		}
		break;

	case RES_NOT:
		if(lpsRestrict->lpNot == NULL) {
			er = ZARAFA_E_INVALID_TYPE;
			goto exit;
		}
		
		er = MatchRowRestrict(lpCacheManager, lpPropVals, lpsRestrict->lpNot->lpNot, lpSubResults, locale, &fMatch, lpulSubRestriction);
		if(er != erSuccess)
			goto exit;

		fMatch = !fMatch;
		break;

	case RES_CONTENT:
		if(lpsRestrict->lpContent == NULL || lpsRestrict->lpContent->lpProp == NULL) {
			er = ZARAFA_E_INVALID_TYPE;
			goto exit;
		}
		// FIXME: FL_IGNORENONSPACE and FL_LOOSE are ignored
		ulPropTagRestrict = lpsRestrict->lpContent->ulPropTag;
		ulPropTagValue = lpsRestrict->lpContent->lpProp->ulPropTag;

		// use the same string type in compares
		if ((PROP_TYPE(ulPropTagRestrict) & PT_MV_STRING8) == PT_STRING8)
			ulPropTagRestrict = CHANGE_PROP_TYPE(ulPropTagRestrict, PT_TSTRING);
		else if ((PROP_TYPE(ulPropTagRestrict) & PT_MV_STRING8) == PT_MV_STRING8)
			ulPropTagRestrict = CHANGE_PROP_TYPE(ulPropTagRestrict, PT_MV_TSTRING);

		// @todo are MV properties in the compare prop allowed?
		if ((PROP_TYPE(ulPropTagValue) & PT_MV_STRING8) == PT_STRING8)
			ulPropTagValue = CHANGE_PROP_TYPE(ulPropTagValue, PT_TSTRING);
		else if ((PROP_TYPE(ulPropTagValue) & PT_MV_STRING8) == PT_MV_STRING8)
			ulPropTagValue = CHANGE_PROP_TYPE(ulPropTagValue, PT_MV_TSTRING);

		if( PROP_TYPE(ulPropTagRestrict) != PT_TSTRING && 
			PROP_TYPE(ulPropTagRestrict) != PT_BINARY &&
			PROP_TYPE(ulPropTagRestrict) != PT_MV_TSTRING &&
			PROP_TYPE(ulPropTagRestrict) != PT_MV_BINARY &&
			lpsRestrict->lpContent->lpProp != NULL)
		{
			ASSERT(FALSE);
			fMatch = false;
			break;
		}

		// find using original proptag from restriction
		lpProp = FindProp(lpPropVals, lpsRestrict->lpContent->ulPropTag);

		if(lpProp == NULL) {
			fMatch = false;
			break;
		} else {
			ulScan = 1;
			if(ulPropTagRestrict & MV_FLAG)
			{
				if(PROP_TYPE(ulPropTagRestrict) == PT_MV_TSTRING)
					ulScan = lpProp->Value.mvszA.__size;
				else
					ulScan = lpProp->Value.mvbin.__size;
			}

			ulPropType = PROP_TYPE(ulPropTagRestrict)&~MVI_FLAG;
			

			if(PROP_TYPE(ulPropTagValue) == PT_TSTRING) {
				lpSearchString = lpsRestrict->lpContent->lpProp->Value.lpszA;
				ulSearchStringSize = (lpSearchString)?strlen(lpSearchString):0;
			}else {
				lpSearchString = (char*)lpsRestrict->lpContent->lpProp->Value.bin->__ptr;
				ulSearchStringSize = lpsRestrict->lpContent->lpProp->Value.bin->__size;
			}
					
			// Default match is false
			fMatch = false;

			for(ulPos=0; ulPos < ulScan; ulPos++)
			{
				if(ulPropTagRestrict & MV_FLAG)
				{
					if(PROP_TYPE(ulPropTagRestrict) == PT_MV_TSTRING)	{
						lpSearchData = lpProp->Value.mvszA.__ptr[ulPos];
						ulSearchDataSize = (lpSearchData)?strlen(lpSearchData):0;
					}else {
						lpSearchData = (char*)lpProp->Value.mvbin.__ptr[ulPos].__ptr;
						ulSearchDataSize = lpProp->Value.mvbin.__ptr[ulPos].__size;
					}
				}else {
					if(PROP_TYPE(ulPropTagRestrict) == PT_TSTRING)	{
						lpSearchData = lpProp->Value.lpszA;
						ulSearchDataSize = (lpSearchData)?strlen(lpSearchData):0;
					}else {
						lpSearchData = (char*)lpProp->Value.bin->__ptr;
						ulSearchDataSize = lpProp->Value.bin->__size;
					}
				}

				ulFuzzyLevel = lpsRestrict->lpContent->ulFuzzyLevel;
				switch(ulFuzzyLevel & 0xFFFF) {
				case FL_FULLSTRING:
					if(ulSearchDataSize == ulSearchStringSize)
					{
						if ((ulPropType == PT_TSTRING && (ulFuzzyLevel & FL_IGNORECASE) && u8_iequals(lpSearchData, lpSearchString, locale)) ||
							(ulPropType == PT_TSTRING && u8_equals(lpSearchData, lpSearchString, locale)) ||
							(memcmp(lpSearchData, lpSearchString, ulSearchDataSize) == 0))
						{
							fMatch = true;							
						}
					}
					break;

				case FL_PREFIX: 
					if(ulSearchDataSize >= ulSearchStringSize)
					{
						if ((ulPropType == PT_TSTRING && (ulFuzzyLevel & FL_IGNORECASE) && u8_istartswith(lpSearchData, lpSearchString, locale)) ||
							(ulPropType == PT_TSTRING && u8_startswith(lpSearchData, lpSearchString, locale)) ||
							(memcmp(lpSearchData, lpSearchString, ulSearchStringSize) == 0))
						{
							fMatch = true;
						}
					}
					break;

				case FL_SUBSTRING: 
					if ((ulPropType == PT_TSTRING && (ulFuzzyLevel & FL_IGNORECASE) && u8_icontains(lpSearchData, lpSearchString, locale)) ||
						(ulPropType == PT_TSTRING && u8_contains(lpSearchData, lpSearchString, locale)) ||
						(memsubstr(lpSearchData, ulSearchDataSize, lpSearchString, ulSearchStringSize) == 0))
					{
						fMatch = true;
					}
					break;
				}

				if(fMatch)
					break;

			}// for(ulPos=0; ulPos < ulScan; ulPos++)
		}
		break;

	case RES_PROPERTY:
		if(lpsRestrict->lpProp == NULL || lpsRestrict->lpProp->lpProp == NULL) {
			er = ZARAFA_E_INVALID_TYPE;
			goto exit;
		}

		ulPropTagRestrict = lpsRestrict->lpProp->ulPropTag;
		ulPropTagValue = lpsRestrict->lpProp->lpProp->ulPropTag;

		// use the same string type in compares
		if ((PROP_TYPE(ulPropTagRestrict) & PT_MV_STRING8) == PT_STRING8)
			ulPropTagRestrict = CHANGE_PROP_TYPE(ulPropTagRestrict, PT_TSTRING);
		else if ((PROP_TYPE(ulPropTagRestrict) & PT_MV_STRING8) == PT_MV_STRING8)
			ulPropTagRestrict = CHANGE_PROP_TYPE(ulPropTagRestrict, PT_MV_TSTRING);

		if (PROP_TYPE(ulPropTagValue) == PT_STRING8)
			ulPropTagValue = CHANGE_PROP_TYPE(ulPropTagValue, PT_TSTRING);

		if((PROP_TYPE(ulPropTagRestrict)&~MV_FLAG) != PROP_TYPE(ulPropTagValue)) {
			// cannot compare two different types, except mvprop -> prop
			er = ZARAFA_E_INVALID_TYPE;
			goto exit;
		}
		if(lpsRestrict->lpProp->ulType == RELOP_RE) {
		    regex_t reg;

			// find using original restriction proptag
			lpProp = FindProp(lpPropVals, lpsRestrict->lpProp->ulPropTag);
			if(lpProp == NULL) {
				fMatch = false;
				break;
			}

			// @todo add support for ulPropTagRestrict PT_MV_TSTRING
		    if(PROP_TYPE(ulPropTagValue) != PT_TSTRING || PROP_TYPE(ulPropTagRestrict) != PT_TSTRING) {
                er = ZARAFA_E_INVALID_TYPE;
                goto exit;
            }
            
            if(regcomp(&reg, lpsRestrict->lpProp->lpProp->Value.lpszA, REG_NOSUB | REG_NEWLINE | REG_ICASE) != 0) {
                fMatch = false;
                break;
            }
            
            if(regexec(&reg, lpProp->Value.lpszA, 0, NULL, 0) == 0)
                fMatch = true;
                
            regfree(&reg);
            
            // Finished for this restriction
            break;
        }

		if(PROP_ID(ulPropTagRestrict) == PROP_ID(PR_ANR))
		{
		    for (unsigned int j = 0; j < arraySize(sANRProps); j++) {
    			lpProp = FindProp(lpPropVals, sANRProps[j]);

                // We need this because CompareProp will fail if the types are not the same
                if(lpProp) {
                    lpProp->ulPropTag = lpsRestrict->lpProp->lpProp->ulPropTag;
                    CompareProp(lpProp, lpsRestrict->lpProp->lpProp, locale, &lCompare); //IGNORE error
                } else
                	continue;
                	
				// PR_ANR has special semantics, lCompare is 1 if the substring is found, 0 if not
				
				// Note that RELOP_EQ will work as expected, but RELOP_GT and RELOP_LT will
				// not work. Use of these is undefined anyway. RELOP_NE is useless since one of the
				// strings will definitely not match, so RELOP_NE will almost match.
				lCompare = lCompare ? 0 : -1;
                    
                fMatch = match(lpsRestrict->lpProp->ulType, lCompare);
                
                if(fMatch)
                    break;
            }
            
            // Finished for this restriction
            break;
		}else {

			// find using original restriction proptag
			lpProp = FindProp(lpPropVals, lpsRestrict->lpProp->ulPropTag);
			if(lpProp == NULL) {
				if(lpsRestrict->lpProp->ulType == RELOP_NE)
					fMatch = true;
				else
					fMatch = false;
				break;
			}
			
			if((ulPropTagRestrict&MV_FLAG)) {
				er = CompareMVPropWithProp(lpProp, lpsRestrict->lpProp->lpProp, lpsRestrict->lpProp->ulType, locale, &fMatch);
				if(er != erSuccess)
				{
					ASSERT(FALSE);
					er = erSuccess;
					fMatch = false;
					break;	
				}
			} else {
				er = CompareProp(lpProp, lpsRestrict->lpProp->lpProp, locale, &lCompare);
				if(er != erSuccess)
				{
					ASSERT(FALSE);
					er = erSuccess;
					fMatch = false;
					break;	
				}
				
				fMatch = match(lpsRestrict->lpProp->ulType, lCompare);
			}
		}// if(ulPropTagRestrict == PR_ANR)
		break;
		
	case RES_COMPAREPROPS:
		if(lpsRestrict->lpCompare == NULL) {
			er = ZARAFA_E_INVALID_TYPE;
			goto exit;
		}

		unsigned int ulPropTag1;
		unsigned int ulPropTag2;

		ulPropTag1 = lpsRestrict->lpCompare->ulPropTag1;
		ulPropTag2 = lpsRestrict->lpCompare->ulPropTag2;

		// use the same string type in compares
		if ((PROP_TYPE(ulPropTag1) & PT_MV_STRING8) == PT_STRING8)
			ulPropTag1 = CHANGE_PROP_TYPE(ulPropTag1, PT_TSTRING);
		else if ((PROP_TYPE(ulPropTag1) & PT_MV_STRING8) == PT_MV_STRING8)
			ulPropTag1 = CHANGE_PROP_TYPE(ulPropTag1, PT_MV_TSTRING);

		// use the same string type in compares
		if ((PROP_TYPE(ulPropTag2) & PT_MV_STRING8) == PT_STRING8)
			ulPropTag2 = CHANGE_PROP_TYPE(ulPropTag2, PT_TSTRING);
		else if ((PROP_TYPE(ulPropTag2) & PT_MV_STRING8) == PT_MV_STRING8)
			ulPropTag2 = CHANGE_PROP_TYPE(ulPropTag2, PT_MV_TSTRING);

		// FIXME: Is this check correct, PT_STRING8 vs PT_ERROR == false and not a error? (RELOP_NE == true)
		if(PROP_TYPE(ulPropTag1) != PROP_TYPE(ulPropTag2)) {
			// cannot compare two different types
			er = ZARAFA_E_INVALID_TYPE;
			goto exit;
		}

		// find using original restriction proptag
		lpProp = FindProp(lpPropVals, lpsRestrict->lpCompare->ulPropTag1);
		lpProp2 = FindProp(lpPropVals, lpsRestrict->lpCompare->ulPropTag2);

		if(lpProp == NULL || lpProp2 == NULL) {
			fMatch = false;
			break;
		}

		er = CompareProp(lpProp, lpProp2, locale, &lCompare);
		if(er != erSuccess)
		{
			ASSERT(FALSE);
			er = erSuccess;
			fMatch = false;
			break;
		}

		switch(lpsRestrict->lpCompare->ulType) {
		case RELOP_GE:
			fMatch = lCompare >= 0;
			break;
		case RELOP_GT:
			fMatch = lCompare > 0;
			break;
		case RELOP_LE:
			fMatch = lCompare <= 0;
			break;
		case RELOP_LT:
			fMatch = lCompare < 0;
			break;
		case RELOP_NE:
			fMatch = lCompare != 0;
			break;
		case RELOP_RE:
			fMatch = false; // FIXME ?? how should this work ??
			break;
		case RELOP_EQ:
			fMatch = lCompare == 0;
			break;
		}
		break;

	case RES_BITMASK:
		if(lpsRestrict->lpBitmask == NULL) {
			er = ZARAFA_E_INVALID_TYPE;
			goto exit;
		}

		// We can only bitmask 32-bit LONG values (aka ULONG)
		if(PROP_TYPE(lpsRestrict->lpBitmask->ulPropTag) != PT_LONG) {
			er = ZARAFA_E_INVALID_TYPE;
			goto exit;
		}

		lpProp = FindProp(lpPropVals, lpsRestrict->lpBitmask->ulPropTag);

		if(lpProp == NULL) {
			fMatch = false;
			break;
		}

		fMatch = (lpProp->Value.ul & lpsRestrict->lpBitmask->ulMask) > 0;

		if(lpsRestrict->lpBitmask->ulType == BMR_EQZ)
			fMatch = !fMatch;

		break;
		
	case RES_SIZE:
		if(lpsRestrict->lpSize == NULL) {
			er = ZARAFA_E_INVALID_TYPE;
			goto exit;
		}

		lpProp = FindProp(lpPropVals, lpsRestrict->lpSize->ulPropTag);

		if(lpProp == NULL) {
			er = ZARAFA_E_INVALID_TYPE;
			goto exit;
		}

		ulSize = PropSize(lpProp);

		lCompare = ulSize - lpsRestrict->lpSize->cb;

		switch(lpsRestrict->lpSize->ulType) {
		case RELOP_GE:
			fMatch = lCompare >= 0;
			break;
		case RELOP_GT:
			fMatch = lCompare > 0;
			break;
		case RELOP_LE:
			fMatch = lCompare <= 0;
			break;
		case RELOP_LT:
			fMatch = lCompare < 0;
			break;
		case RELOP_NE:
			fMatch = lCompare != 0;
			break;
		case RELOP_RE:
			fMatch = false; // FIXME ?? how should this work ??
			break;
		case RELOP_EQ:
			fMatch = lCompare == 0;
			break;
		}
		break;

	case RES_EXIST:
		if(lpsRestrict->lpExist == NULL) {
			er = ZARAFA_E_INVALID_TYPE;
			goto exit;
		}

		lpProp = FindProp(lpPropVals, lpsRestrict->lpExist->ulPropTag);

		fMatch = (lpProp != NULL);
		break;
	case RES_SUBRESTRICTION:
	    lpProp = FindProp(lpPropVals, PR_ENTRYID);
	    
	    if(lpProp == NULL) {
	        er = ZARAFA_E_INVALID_TYPE;
	        goto exit;
        }
        
	    if(lpSubResults == NULL) {
	        fMatch = false;
        } else {
            // Find out if this object matches this subrestriction with the passed
            // subrestriction results.
         
            if(lpSubResults->size() <= ulSubRestrict) {
                fMatch = false; // No results in the results list for this subquery ??
            } else {
				fMatch = false;

				sEntryId.__ptr = lpProp->Value.bin->__ptr;
				sEntryId.__size = lpProp->Value.bin->__size;
				if(lpCacheManager->GetObjectFromEntryId(&sEntryId, &ulResId) == erSuccess)
				{
	                iterSubResult = (*lpSubResults)[ulSubRestrict]->find(ulResId); // If the item is in the set, it matched
                
					if(iterSubResult != (*lpSubResults)[ulSubRestrict]->end()) {
						fMatch = true;
					}
				}
            }
        }
		break;

	default:
		er = ZARAFA_E_INVALID_TYPE;
		goto exit;
	}

	*lpfMatch = fMatch;

exit:
	return er;
}

bool ECGenericObjectTable::IsMVSet()
{
	return (m_bMVSort | m_bMVCols);
}

void ECGenericObjectTable::SetTableId(unsigned int ulTableId)
{
	m_ulTableId = ulTableId;
}

ECRESULT ECGenericObjectTable::Clear()
{
	ECCategoryMap::iterator iterCategories;

	// Clear old entries
	mapObjects.clear();
	lpKeyTable->Clear();
	m_mapLeafs.clear();

	for(iterCategories = m_mapCategories.begin(); iterCategories != m_mapCategories.end(); iterCategories++)
		delete iterCategories->second;
	m_mapCategories.clear();
	m_mapSortedCategories.clear();
    
	return hrSuccess;
}

ECRESULT ECGenericObjectTable::Load()
{
    return hrSuccess;
}

ECRESULT ECGenericObjectTable::Populate()
{
	ECRESULT er = erSuccess;

	pthread_mutex_lock(&m_hLock);

	if(m_bPopulated)
		goto exit;

	m_bPopulated = true;

	er = Load();

exit:
	pthread_mutex_unlock(&m_hLock);

	return er;
}

/////////////////////////////////////////////////////////
// Sort functions, overide this functions as you used a caching system
//

ECRESULT ECGenericObjectTable::IsSortKeyExist(const sObjectTableKey* lpsRowItem, unsigned int ulPropTag)
{
	return ZARAFA_E_NOT_FOUND;
}

ECRESULT ECGenericObjectTable::GetSortKey(sObjectTableKey* lpsRowItem, unsigned int ulPropTag, unsigned int *lpSortLen, unsigned char **lppSortData)
{
	ASSERT(FALSE);
	return ZARAFA_E_NO_SUPPORT;
}

ECRESULT ECGenericObjectTable::SetSortKey(sObjectTableKey* lpsRowItem, unsigned int ulPropTag, unsigned int ulSortLen, unsigned char *lpSortData)
{
	return ZARAFA_E_NO_SUPPORT;
}

////////////////////////////////////////////////////////////
// Category handling functions
//

/*
 * GENERAL workings of categorization
 *
 * Due to min/max categories, the order of rows in the key table (m_lpKeyTable) in not predictable by looking
 * only at a new row, since we don't know what the min/max value for the category is. We therefore have a second
 * sorted list of categories which is purely for looking up if a category exists, and what it's min/max value is.
 * This list is m_mapSortedCategories.
 *
 * The order of rows in m_lpKeyTable is the actual order of rows that will be seen by the MAPI Client.
 *
 * quick overview:
 *
 * When a new row is added, we do the following:
 * - Look in m_mapSortedCategories with the categorized properties to see if we already have the category
 * -on new:
 *   - Add category to both mapSortedCategories and mapCategories
 *   - Initialize counters and possibly min/max value
 *   -on existing:
 *   - Update counters (unread,count)
 *   - Update min/max value
 *   -on change of min/max value:
 *     - reorder *all* rows of the category
 * - Track the row in m_mapLeafs
 *
 * When a row is removed, we do the following
 * - Find the row in m_mapLeafs
 * - Get the category of the row
 * - Update counters of the category
 * - Update min/max value of the category
 * -on change of min/max value and non-empty category:
 *   - reorder *all* rows of the category
 * - If count == 0, remove category
 *
 * We currently support only one min/max category in the table. This is actually only enforced in ECCategory which
 * tracks only one sCurMinMax, the rest of the code should be pretty much able to handle multiple levels of min/max
 * categories.
 */

/**
 * Add or modify a category row
 *
 * Called just before the actual message row is added to the table.
 *
 * Due to min/max handling, this function may modify MANY rows in the table because the entire category needed to be relocated.
 *
 * @param sObjKey Object key of new (non-category) row
 * @param lpProps Properties of the new or modified row
 * @param cProps Number of properties in lpProps
 * @param ulFlags Notification flags
 * @param fUnread TRUE if the new state of the object in sObjKey is UNREAD
 * @param lpfHidden Returns if the new row should be hidden because the category is collapsed
 * @param lppCategory Returns a reference to the new or existing category that the item sObjKey should be added to
 */
ECRESULT ECGenericObjectTable::AddCategoryBeforeAddRow(sObjectTableKey sObjKey, struct propVal *lpProps, unsigned int cProps, unsigned int ulFlags, bool fUnread, bool *lpfHidden, ECCategory **lppCategory)
{
    ECRESULT er = erSuccess;
    bool fPrevUnread = false;
    bool fNewLeaf = false;
    unsigned int i = 0;
    unsigned int *lpSortLen = NULL;
    unsigned char **lppSortKeys = NULL;
    unsigned char *lpSortFlags = NULL;
    sObjectTableKey sPrevRow(0,0);
    ECCategory *lpCategory = NULL;
    LEAFINFO sLeafInfo;
    ECCategory *lpParent = NULL;
    ECKeyTable::UpdateType ulAction;
    sObjectTableKey sCatRow;
    ECLeafMap::iterator iterLeafs;
    int fResult = 0;
    bool fCollapsed = false;
    bool fHidden = false;
	ECCategoryMap::iterator iterCategory;
	ECSortedCategoryMap::iterator iterCategoriesSorted;
    
    if(m_ulCategories == 0)
        goto exit;
    
    lpSortLen = new unsigned int[cProps];
    lppSortKeys = new unsigned char *[cProps];
    lpSortFlags = new unsigned char [cProps];

    // Build binary sort keys
    
    // +1 because we may have a trailing category followed by a MINMAX column
    for(i=0; i < m_ulCategories + 1 && i < cProps; i++) {
        if(GetBinarySortKey(&lpProps[i], &lpSortLen[i], &lppSortKeys[i]) != erSuccess)
        	lppSortKeys[i] = NULL;
        if(GetSortFlags(lpProps[i].ulPropTag, &lpSortFlags[i]) != erSuccess)
        	lpSortFlags[i] = 0;
    }

    // See if we're dealing with a changed row, not a new row
    iterLeafs = m_mapLeafs.find(sObjKey);
    if(iterLeafs != m_mapLeafs.end()) {
    	fPrevUnread = iterLeafs->second.fUnread;
        // The leaf was already in the table, compare new properties of the leaf with those of the category it
        // was in.
        for(i=0; i < iterLeafs->second.lpCategory->m_cProps && i < cProps; i++) {
            // If the type is different (ie PT_ERROR first, PT_STRING8 now, then it has definitely changed ..)
            if(PROP_TYPE(lpProps[i].ulPropTag) != PROP_TYPE(iterLeafs->second.lpCategory->m_lpProps[i].ulPropTag))
                break;
                
            // Otherwise, compare the properties
            er = CompareProp(&iterLeafs->second.lpCategory->m_lpProps[i], &lpProps[i], m_locale, &fResult);
            if(er != erSuccess) {
                goto exit;
            }
                
            if(fResult != 0)
                break;
        }
            
        if(iterLeafs->second.lpCategory->m_cProps && i < cProps) {
            // One of the category properties has changed, remove the row from the old category
            RemoveCategoryAfterRemoveRow(sObjKey, ulFlags);
            fNewLeaf = true; // We're re-adding the leaf
        } else {
            if(fUnread == iterLeafs->second.fUnread) {
	            // Nothing to do, the leaf was already in the correct category, and the readstate has not changed
	            goto exit;
			}
        }
    } else {
    	fNewLeaf = true;
	}
    
    // For each level, check if category already exists in key table (LowerBound), gives sObjectTableKey
    for(i=0; i < m_ulCategories && i < cProps; i++) {
    	unsigned int ulDepth = i;
        bool fCategoryMoved = false; // TRUE if the entire category has moved somewhere (due to CATEG_MIN / CATEG_MAX change)
        ECTableRow row(sObjectTableKey(0,0), i+1, lpSortLen, lpSortFlags, lppSortKeys, false);

        // Find the actual category in our sorted category map
    	iterCategoriesSorted = m_mapSortedCategories.find(row);

        // Include the next sort order if it s CATEG_MIN or CATEG_MAX
        if(lpsSortOrderArray->__size > (int)i+1 && ISMINMAX(lpsSortOrderArray->__ptr[i+1].ulOrder))
        	i++;
    	
        if(iterCategoriesSorted == m_mapSortedCategories.end()) {
     		ASSERT(fNewLeaf); // The leaf must be new if the category is new       

            // Category not available yet, add it now
            sCatRow.ulObjId = 0;
            sCatRow.ulOrderId = m_ulCategory;
            
            // We are hidden if our parent was collapsed
            fHidden = fCollapsed;
            
            // This category is itself collapsed if our parent was collapsed, or if we should be collapsed due to m_ulExpanded
            fCollapsed = fCollapsed || (ulDepth >= m_ulExpanded);
            
            lpCategory = new ECCategory(m_ulCategory, lpProps, ulDepth+1, i+1, lpParent, ulDepth, !fCollapsed, m_locale);
            
            m_ulCategory++;

            lpCategory->IncLeaf(); // New category has 1 leaf
            
            // Make sure the category has the current row as min/max value
            er = UpdateCategoryMinMax(sObjKey, lpCategory, i, lpProps, cProps, NULL);
            if(er != erSuccess)
            	goto exit;
            
            if(fUnread)
            	lpCategory->IncUnread();

			// Add the category into our sorted-category list and numbered-category list
            ASSERT(m_mapSortedCategories.find(row) == m_mapSortedCategories.end());

            m_mapCategories[sCatRow] = lpCategory;
            lpCategory->iSortedCategory = m_mapSortedCategories.insert(std::make_pair(row, sCatRow)).first;

			// Update the keytable with the effective sort columns
			er = UpdateKeyTableRow(lpCategory, &sCatRow, lpProps, i+1, fHidden, &sPrevRow, &ulAction);
			if (er != erSuccess)
				goto exit;

            lpParent = lpCategory;
        } else {
            // Category already available
            sCatRow = iterCategoriesSorted->second;

            // Get prev row for notification purposes
            if(lpKeyTable->GetPreviousRow(&sCatRow, &sPrevRow) != erSuccess) {
                sPrevRow.ulObjId = 0;
                sPrevRow.ulOrderId = 0;
            }
            
			iterCategory = m_mapCategories.find(sCatRow);
			if(iterCategory == m_mapCategories.end()) {
				ASSERT(FALSE);
				er = ZARAFA_E_NOT_FOUND;
				goto exit;
			}

			lpCategory = iterCategory->second;

            // Increase leaf count of category (each level must be increased by one) for a new leaf
            if(fNewLeaf) {
	            lpCategory->IncLeaf();
	            if(fUnread)
	            	lpCategory->IncUnread();
			} else {
				// Increase or decrease unread counter depending on change of the leaf's unread state
				if(fUnread && !fPrevUnread)
					lpCategory->IncUnread();
				
				if(!fUnread && fPrevUnread)
					lpCategory->DecUnread(); 
			}
			            
            // This category was hidden if the parent was collapsed
            fHidden = fCollapsed;
            // Remember if this category was collapsed
            fCollapsed = !lpCategory->m_fExpanded;
            
            // Update category min/max values
            er = UpdateCategoryMinMax(sObjKey, lpCategory, i, lpProps, cProps, &fCategoryMoved); 
            if(er != erSuccess)
            	goto exit;
            	
			ulAction = ECKeyTable::TABLE_ROW_MODIFY;
        }

		if (fCategoryMoved) {
			ECObjectTableList lstObjects;
			ECObjectTableList::iterator iterObject;
			// The min/max value of this category has changed. We have to move all the rows in the category
			// somewhere else in the table.
			
			// Get the rows that are affected
			er = lpKeyTable->GetRowsBySortPrefix(&sCatRow, &lstObjects);
			if (er != erSuccess)
				goto exit;
				
			// Update the keytable to reflect the new change
			for(iterObject = lstObjects.begin(); iterObject != lstObjects.end(); iterObject++) {
				// Update the keytable with the new effective sort data for this column
				
				bool bDescend = lpsSortOrderArray->__ptr[ulDepth].ulOrder == EC_TABLE_SORT_DESCEND; // Column we're updating is descending
				
				er = lpKeyTable->UpdatePartialSortKey(&(*iterObject), ulDepth, lppSortKeys[i], lpSortLen[i], lpSortFlags[i] | (bDescend ? TABLEROW_FLAG_DESC : 0), &sPrevRow, &fHidden, &ulAction);
				if (er != erSuccess)
					goto exit;
					
				if((ulFlags & OBJECTTABLE_NOTIFY) && !fHidden) {
					AddTableNotif(ulAction, *iterObject, &sPrevRow);
				}
			}
		} else {
	        // Send notification if required (only the category header has changed)
    	    if((ulFlags & OBJECTTABLE_NOTIFY) && !fHidden) {
        	    AddTableNotif(ulAction, sCatRow, &sPrevRow);
	        }
		}
    }
    
    // lpCategory is now the deepest category, and therefore the category we're adding the leaf to

    // Add sObjKey to leaf list via LEAFINFO

    sLeafInfo.lpCategory = lpCategory;
    sLeafInfo.fUnread = fUnread;
    
    m_mapLeafs[sObjKey] = sLeafInfo;
    
    // The item that the request was for is hidden if the deepest category was collapsed
    if(lpfHidden)
        *lpfHidden = fCollapsed;
        
	if(lppCategory)
		*lppCategory = lpCategory;

    ASSERT(m_mapCategories.size() == m_mapSortedCategories.size());
    
exit:
    if(lpSortLen)
        delete [] lpSortLen;
    if(lppSortKeys) {
        for(i=0;i < m_ulCategories + 1 && i < cProps;i++)
            delete [] lppSortKeys[i];
            
        delete [] lppSortKeys;
    }
    if(lpSortFlags)
        delete [] lpSortFlags;
        
    return er;
}

/**
 * Update a category min/max value if needed
 *
 * This function updates the min/max value if the category is part of a min/max sorting scheme.
 *
 * @param sKey Key of the category
 * @param lpCategory Category to update
 * @param i Column id of the possible min/max sort
 * @param lpProps Properties for the category
 * @param cProps Number of properties in lpProps
 * @param lpfModified Returns whether the category min/max value was changed
 * @return result
 */
ECRESULT ECGenericObjectTable::UpdateCategoryMinMax(sObjectTableKey &sKey, ECCategory *lpCategory, unsigned int i, struct propVal *lpProps, unsigned int cProps, bool *lpfModified)
{
	ECRESULT er = erSuccess;

	if(lpsSortOrderArray->__size <= i || !ISMINMAX(lpsSortOrderArray->__ptr[i].ulOrder))
		goto exit;

	lpCategory->UpdateMinMax(sKey, i, &lpProps[i], lpsSortOrderArray->__ptr[i].ulOrder == EC_TABLE_SORT_CATEG_MAX, lpfModified);
	
exit:
	return er;
}

/**
 * Creates a row in the key table
 *
 * The only complexity of this function is when doing min/max categorization; consider the sort order
 *
 * CONVERSATION_TOPIC ASC, DATE CATEG_MAX, DATE DESC
 * with ulCategories = 1
 *
 * This effectively generates the following category row in the key table:
 *
 * MAX_DATE, CONVERSATION_TOPIC 				for the category and
 * MAX_DATE, CONVERSATION_TOPIC, DATE			for the data row
 *
 * This means we have to get the 'max date' part, generate a sortkey, and switch the order for the first
 * two columns, and add that to the key table.
 *
 * @param lpCategory For a normal row, the category to which it belongs
 * @param lpsRowKey The row key of the row
 * @param ulDepth Number properties in lpProps/cValues to process. For normal rows, ulDepth == cValues
 * @param lpProps Properties from the database of the row
 * @param cValues Number of properties in lpProps
 * @param fHidden TRUE if the row is to be hidden
 * @param sPrevRow Previous row ID
 * @param lpulAction Action performed
 * @return result
 */
ECRESULT ECGenericObjectTable::UpdateKeyTableRow(ECCategory *lpCategory, sObjectTableKey *lpsRowKey, struct propVal *lpProps, unsigned int cValues, bool fHidden, sObjectTableKey *lpsPrevRow, ECKeyTable::UpdateType *lpulAction)
{
	ECRESULT er = erSuccess;
	struct propVal *lpOrderedProps = NULL;
    unsigned int *lpSortLen = NULL;
    unsigned char **lppSortKeys = NULL;
    unsigned char *lpSortFlags = NULL;
    struct propVal sProp;
    struct sortOrderArray *lpsSortOrderArray = this->lpsSortOrderArray;
    struct sortOrder sSortHierarchy = { PR_EC_HIERARCHYID, EC_TABLE_SORT_DESCEND };
    struct sortOrderArray sSortSimple = { &sSortHierarchy, 1 };
    int n = 0;
    
    ASSERT(cValues <= (unsigned int)lpsSortOrderArray->__size);

    if(cValues == 0) {
		// No sort columns. We use the object ID as the sorting
		// key. This is fairly arbitrary as any sort order would be okay seen as no sort order was specified. However, sorting
		// in this way makes sure that new items are sorted FIRST by default, which is a logical view when debugging. Also, if
		// any algorithm does assumptions based on the first row, it is best to have the first row be the newest row; this is what
		// happens when you export messages from OLK to a PST and it does a memory calculation of nItems * size_of_first_entryid
		// for the memory requirements of all entryids. (which crashes if you don't do it properly)
		sProp.ulPropTag = PR_EC_HIERARCHYID;
		sProp.Value.ul = lpsRowKey->ulObjId;
		sProp.__union = SOAP_UNION_propValData_ul;
		
		cValues = 1;
		lpProps = &sProp;
		lpsSortOrderArray = &sSortSimple;
    }
	
	lpOrderedProps = new struct propVal[cValues];
	memset(lpOrderedProps, 0, sizeof(struct propVal) * cValues);
    lpSortLen = new unsigned int[cValues];
    memset(lpSortLen, 0, sizeof(unsigned int) * cValues);
    lppSortKeys = new unsigned char *[cValues];
    memset(lppSortKeys, 0, sizeof(unsigned char *) * cValues);
    lpSortFlags = new unsigned char [cValues];
    memset(lpSortFlags, 0, sizeof(unsigned char) * cValues);

	for(unsigned int i = 0; i < cValues; i++) {
		if (ISMINMAX(lpsSortOrderArray->__ptr[i].ulOrder)) {
			if(n == 0 || !lpCategory) {
				// Min/max ignored if the row is not in a category
				continue;
			}
			
			// Swap around the current and the previous sorting order
			lpOrderedProps[n] = lpOrderedProps[n-1];
			// Get actual sort order from category
			if(lpCategory->GetProp(NULL, lpsSortOrderArray->__ptr[n].ulPropTag, &lpOrderedProps[n-1]) != erSuccess) {
				lpOrderedProps[n-1].ulPropTag = PROP_TAG(PT_ERROR, PROP_ID(lpsSortOrderArray->__ptr[n].ulPropTag));
				lpOrderedProps[n-1].Value.ul = ZARAFA_E_NOT_FOUND;
				lpOrderedProps[n-1].__union = SOAP_UNION_propValData_ul;
			}
		} else {
			er = CopyPropVal(&lpProps[n], &lpOrderedProps[n], NULL, false);
			if(er != erSuccess)
				goto exit;
		}
		
		n++;
	}
	
    // Build binary sort keys from updated data
    for(int i=0; i < n; i++) {
        if(GetBinarySortKey(&lpOrderedProps[i], &lpSortLen[i], &lppSortKeys[i]) != erSuccess)
        	lppSortKeys[i] = NULL;
        if(GetSortFlags(lpOrderedProps[i].ulPropTag, &lpSortFlags[i]) != erSuccess)
        	lpSortFlags[i] = 0;
        if(lpsSortOrderArray->__ptr[i].ulOrder == EC_TABLE_SORT_DESCEND)
            lpSortFlags[i] |= TABLEROW_FLAG_DESC;
    }

    // Update row
    er = lpKeyTable->UpdateRow(ECKeyTable::TABLE_ROW_ADD, lpsRowKey, cValues, lpSortLen, lpSortFlags, lppSortKeys, lpsPrevRow, fHidden, lpulAction);

exit:
	if(lpOrderedProps) {
		for(unsigned int i=0; i < cValues; i++) {
			FreePropVal(&lpOrderedProps[i], false);
		}
		delete [] lpOrderedProps;
	}
		
    if(lpSortLen)
        delete [] lpSortLen;
    if(lppSortKeys) {
        for(unsigned int i=0; i < cValues; i++)
            delete [] lppSortKeys[i];
            
        delete [] lppSortKeys;
    }
    if(lpSortFlags)
        delete [] lpSortFlags;
        
    return er;
}

/**
 * Updates a category after a non-category row has been removed
 *
 * This function updates the category to contain the correct counters, and possibly removes the category if it is empty.
 *
 * Many row changes may be generated in a min/max category when the min/max row is removed from the category, which triggers
 * a reorder of the category in the table.
 *
 * @param sOjbKey Object that was deleted
 * @param ulFlags Notification flags
 * @return result
 */
ECRESULT ECGenericObjectTable::RemoveCategoryAfterRemoveRow(sObjectTableKey sObjKey, unsigned int ulFlags)
{
    ECRESULT er = erSuccess;
    sObjectTableKey sCatRow;
    sObjectTableKey sPrevRow(0,0);
    ECLeafMap::iterator iterLeafs;
    ECKeyTable::UpdateType ulAction;
    ECCategory *lpCategory = NULL;
    ECCategory *lpParent = NULL;
    bool fModified = false;
    bool fHidden = false;
    unsigned int ulDepth = 0;
	unsigned char *lpSortKey = NULL;
	unsigned int ulSortLen = 0;
	unsigned char ulSortFlags = 0;
	struct propVal sProp;
	
	sProp.ulPropTag = PR_NULL;
    
    // Find information for the deleted leaf
    iterLeafs = m_mapLeafs.find(sObjKey);
    if(iterLeafs == m_mapLeafs.end()) {
        er = ZARAFA_E_NOT_FOUND;
        goto exit;
    }
    
    lpCategory = iterLeafs->second.lpCategory;

    // Loop through this category and all its parents
    while(lpCategory) {
    	ulDepth = lpCategory->m_ulDepth;
    	
        lpParent = lpCategory->m_lpParent;
        
        // Build the row key for this category
        sCatRow.ulObjId = 0;
        sCatRow.ulOrderId = lpCategory->m_ulCategory;
        
        // Decrease the number of leafs in the category    
        lpCategory->DecLeaf();    
        if(iterLeafs->second.fUnread)
            lpCategory->DecUnread();
            
		if(ulDepth+1 < lpsSortOrderArray->__size && ISMINMAX(lpsSortOrderArray->__ptr[ulDepth+1].ulOrder)) {
			// Removing from a min/max category
			er = lpCategory->UpdateMinMaxRemove(sObjKey, ulDepth+1, lpsSortOrderArray->__ptr[ulDepth+1].ulOrder == EC_TABLE_SORT_CATEG_MAX, &fModified);
			if(er != erSuccess) {
				ASSERT(false);
				goto exit;
			}
			
			if(fModified && lpCategory->GetCount() > 0) {
				// We have removed the min or max value for the category, so reorder is needed (unless category is empty, since it will be removed)
				ECObjectTableList lstObjects;
				ECObjectTableList::iterator iterObject;
				
				// Get the rows that are affected
				er = lpKeyTable->GetRowsBySortPrefix(&sCatRow, &lstObjects);
				if (er != erSuccess)
					goto exit;
					
				// Update the keytable to reflect the new change
				for(iterObject = lstObjects.begin(); iterObject != lstObjects.end(); iterObject++) {
					// Update the keytable with the new effective sort data for this column
					
					if(lpCategory->GetProp(NULL, lpsSortOrderArray->__ptr[ulDepth+1].ulPropTag, &sProp) != erSuccess) {
						sProp.ulPropTag = PROP_TAG(PT_ERROR, PROP_ID(lpsSortOrderArray->__ptr[ulDepth+1].ulPropTag));
						sProp.Value.ul = ZARAFA_E_NOT_FOUND;
					}

					if(GetBinarySortKey(&sProp, &ulSortLen, &lpSortKey) != erSuccess)
						lpSortKey = NULL;
					if(GetSortFlags(sProp.ulPropTag, &ulSortFlags) != erSuccess)
						ulSortFlags = 0;
					
					ulSortFlags |=  lpsSortOrderArray->__ptr[ulDepth].ulOrder == EC_TABLE_SORT_DESCEND ? TABLEROW_FLAG_DESC : 0;
					
					er = lpKeyTable->UpdatePartialSortKey(&(*iterObject), ulDepth, lpSortKey, ulSortLen, ulSortFlags, &sPrevRow, &fHidden, &ulAction);
					if (er != erSuccess)
						goto exit;
						
					if((ulFlags & OBJECTTABLE_NOTIFY) && !fHidden) {
						AddTableNotif(ulAction, *iterObject, &sPrevRow);
					}
					
					if(lpSortKey)
						delete [] lpSortKey;
					lpSortKey = NULL;
						
					FreePropVal(&sProp, false);
					sProp.ulPropTag = PR_NULL;
				}
			}
		}
            
        if(lpCategory->GetCount() == 0) {
            // The category row is empty and must be removed
            ECTableRow *lpRow = NULL; // reference to the row in the keytable
            
            er = lpKeyTable->GetRow(&sCatRow, &lpRow);
            if(er != erSuccess) {
            	ASSERT(false);
            	goto exit;
			}
        	
        	// Remove the category from the sorted categories map
        	m_mapSortedCategories.erase(lpCategory->iSortedCategory);
        	
        	// Remove the category from the keytable
            lpKeyTable->UpdateRow(ECKeyTable::TABLE_ROW_DELETE, &sCatRow, 0, NULL, NULL, NULL, NULL, false, &ulAction);

            // Remove the category from the category map
            ASSERT(m_mapCategories.find(sCatRow) != m_mapCategories.end());
            m_mapCategories.erase(sCatRow);
            
            // Free the category itself
            delete lpCategory;
            
            // Send the notification
            if(ulAction == ECKeyTable::TABLE_ROW_DELETE && (ulFlags & OBJECTTABLE_NOTIFY)) {
                AddTableNotif(ulAction, sCatRow, NULL);
            }
        } else {    
            if(ulFlags & OBJECTTABLE_NOTIFY) {
                // The category row has changed; the counts have updated, send a notification
                
                if(lpKeyTable->GetPreviousRow(&sCatRow, &sPrevRow) != erSuccess) {
                    sPrevRow.ulOrderId = 0;
                    sPrevRow.ulObjId = 0;
                }
                
                AddTableNotif(ECKeyTable::TABLE_ROW_MODIFY, sCatRow, &sPrevRow);
            }
        }
        
        lpCategory = lpParent;
    }

    // Remove the leaf from the leaf map
    m_mapLeafs.erase(iterLeafs);
        
    // All done
    ASSERT(m_mapCategories.size() == m_mapSortedCategories.size());

exit:
	if(lpSortKey)
		delete [] lpSortKey;
		
	FreePropVal(&sProp, false);
	sProp.ulPropTag = PR_NULL;
	
    return er;
}

/**
 * Get a table properties for a category
 *
 * @param soap SOAP object for memory allocation of data in lpPropVal
 * @param ulPropTag Requested property tag
 * @param sKey Key of the category to be retrieved
 * @param lpPropVal Output location of the property
 * @return result
 */
ECRESULT ECGenericObjectTable::GetPropCategory(struct soap *soap, unsigned int ulPropTag, sObjectTableKey sKey, struct propVal *lpPropVal)
{
    ECRESULT er = erSuccess;
    ECCategoryMap::iterator iterCategories;
    unsigned int i = 0;
    
    iterCategories = m_mapCategories.find(sKey);
    if(iterCategories == m_mapCategories.end()) {
        er = ZARAFA_E_NOT_FOUND;
        goto exit;
    }
    
    switch(ulPropTag) {
        case PR_INSTANCE_KEY:
            lpPropVal->__union = SOAP_UNION_propValData_bin;
            lpPropVal->Value.bin = s_alloc<struct xsd__base64Binary>(soap);
            lpPropVal->Value.bin->__size = sizeof(unsigned int) * 2;
            lpPropVal->Value.bin->__ptr = s_alloc<unsigned char>(soap, sizeof(unsigned int) * 2);
            *((unsigned int *)lpPropVal->Value.bin->__ptr) = sKey.ulObjId;
            *((unsigned int *)lpPropVal->Value.bin->__ptr+1) = sKey.ulOrderId;
            lpPropVal->ulPropTag = PR_INSTANCE_KEY;
            break;
        case PR_ROW_TYPE:
            lpPropVal->__union = SOAP_UNION_propValData_ul;
            lpPropVal->Value.ul = iterCategories->second->m_fExpanded ? TBL_EXPANDED_CATEGORY : TBL_COLLAPSED_CATEGORY;
            lpPropVal->ulPropTag = PR_ROW_TYPE;
            break;
        case PR_DEPTH:
            lpPropVal->__union = SOAP_UNION_propValData_ul;
            lpPropVal->Value.ul = iterCategories->second->m_ulDepth;
            lpPropVal->ulPropTag = PR_DEPTH;
            break;
        case PR_CONTENT_COUNT:
            lpPropVal->__union = SOAP_UNION_propValData_ul;
            lpPropVal->Value.ul = iterCategories->second->m_ulLeafs;
            lpPropVal->ulPropTag = PR_CONTENT_COUNT;
            break;
        case PR_CONTENT_UNREAD:
            lpPropVal->__union = SOAP_UNION_propValData_ul;
            lpPropVal->Value.ul = iterCategories->second->m_ulUnread;
            lpPropVal->ulPropTag = PR_CONTENT_UNREAD;
            break;
        default:
            for(i=0;i<iterCategories->second->m_cProps; i++) {
                // If MVI is set, search for the property as non-MV property, as this is how we will have
                // received it when the category was added.
                if(NormalizePropTag(iterCategories->second->m_lpProps[i].ulPropTag) == NormalizePropTag(ulPropTag & ~MVI_FLAG)) {
                    if(CopyPropVal(&iterCategories->second->m_lpProps[i], lpPropVal, soap) == erSuccess) {
						lpPropVal->ulPropTag = (ulPropTag & ~MVI_FLAG);
                        break;
					}
                }
            }
            
            if(i == iterCategories->second->m_cProps)
                er = ZARAFA_E_NOT_FOUND;
        }

exit:    
    return er;
}

unsigned int ECGenericObjectTable::GetCategories()
{
    return this->m_ulCategories;
}

// Normally overridden by subclasses
ECRESULT ECGenericObjectTable::CheckPermissions(unsigned int ulObjId)
{
    return hrSuccess;
}

/**
 * Get object size
 *
 * @return Object size in bytes
 */
unsigned int ECGenericObjectTable::GetObjectSize()
{
	unsigned int ulSize = sizeof(*this);
	ECCategoryMap::iterator iterCat;

	pthread_mutex_lock(&m_hLock);

	ulSize += SortOrderArraySize(lpsSortOrderArray);
	ulSize += PropTagArraySize(lpsPropTagArray);
	ulSize += RestrictTableSize(lpsRestrict);
	ulSize += m_listMVSortCols.size() * sizeof(ECListInt::value_type);

	ulSize += mapObjects.size() * sizeof(ECObjectTableMap::value_type);
	ulSize += lpKeyTable->GetObjectSize();

	ulSize += m_mapCategories.size() * sizeof(ECCategoryMap::value_type);
	for(iterCat = m_mapCategories.begin(); iterCat != m_mapCategories.end(); iterCat++) {
		ulSize += iterCat->second->GetObjectSize();
	}
	
	ulSize += m_mapLeafs.size() * sizeof(ECLeafMap::value_type);

	pthread_mutex_unlock(&m_hLock);

	return ulSize;
}

ECCategory::ECCategory(unsigned int ulCategory, struct propVal *lpProps, unsigned int cProps, unsigned int nProps, ECCategory *lpParent, unsigned int ulDepth, bool fExpanded, const ECLocale &locale) : m_locale(locale)
{
    unsigned int i;

    m_lpProps = new propVal[nProps];
    for(i=0;i<cProps;i++) {
        CopyPropVal(&lpProps[i], &m_lpProps[i]);
    }
    for(;i<nProps;i++) {
    	m_lpProps[i].ulPropTag = PR_NULL;
    	m_lpProps[i].Value.ul = 0;
    	m_lpProps[i].__union = SOAP_UNION_propValData_ul;
    }
        
    m_cProps = nProps;
    
    m_lpParent = lpParent;
    m_ulDepth = ulDepth;
    m_ulUnread = 0;
    m_ulLeafs = 0;
    m_fExpanded = fExpanded;
    m_ulCategory = ulCategory;
}

ECCategory::~ECCategory()
{
    unsigned int i;
    
    for(i=0;i<m_cProps;i++) {
        FreePropVal(&m_lpProps[i], false);
    }
    
    std::map<sObjectTableKey, struct propVal *>::iterator iterMinMax;
    
    for(iterMinMax = m_mapMinMax.begin(); iterMinMax != m_mapMinMax.end(); iterMinMax++) {
    	FreePropVal(iterMinMax->second, true);
    }

    if (m_lpProps)
	    delete [] m_lpProps;
}

void ECCategory::IncLeaf()
{
    m_ulLeafs++;
}

void ECCategory::DecLeaf()
{
    m_ulLeafs--;
}

ECRESULT ECCategory::GetProp(struct soap *soap, unsigned int ulPropTag, struct propVal* lpPropVal)
{
    ECRESULT er = erSuccess;
    unsigned int i;
    
    for(i=0;i<m_cProps;i++) {
        if(m_lpProps[i].ulPropTag == ulPropTag) {
            er = CopyPropVal(&m_lpProps[i], lpPropVal, soap);
            break;
        }
    }
    
    if(i == m_cProps)
        er = ZARAFA_E_NOT_FOUND;
    
    return er;
}

ECRESULT ECCategory::SetProp(unsigned int i, struct propVal* lpPropVal)
{
    ECRESULT er = erSuccess;
    
    ASSERT(i < m_cProps);
    
    FreePropVal(&m_lpProps[i], false);
    
    er = CopyPropVal(lpPropVal, &m_lpProps[i], NULL);
    
    return er;
}

/**
 * Updates a min/max value:
 *
 * Checks if the value passed is more or less than the current min/max value. Currently we treat 
 * an error value as a 'worse' value than ANY new value. This means that min(ERROR, 1) == 1, and max(ERROR, 1) == 1.
 *
 * The new value is also tracked in a list of min/max value so that UpdateMinMaxRemove() (see below) can update
 * the min/max value when a row is removed.
 *
 * @param sKey Key of the new row
 * @param i Column id of the min/max value
 * @param lpNewValue New value for the column (may also be PT_ERROR)
 * @param bool fMax TRUE if the column is a EC_TABLE_SORT_CATEG_MAX, FALSE if the column is EC_TABLE_SORT_CATEG_MIN
 * @param lpfModified Returns TRUE if the new value updated the min/max value, false otherwise
 * @return result
 */
ECRESULT ECCategory::UpdateMinMax(const sObjectTableKey &sKey, unsigned int i, struct propVal *lpNewValue, bool fMax, bool *lpfModified)
{
	ECRESULT er = erSuccess;
	bool fModified = false;
	int result = 0;
	std::map<sObjectTableKey, struct propVal *>::iterator iterMinMax;

	struct propVal *lpOldValue;
	struct propVal *lpNew;
	
	lpOldValue = &m_lpProps[i];
	
	if(PROP_TYPE(lpOldValue->ulPropTag) != PT_ERROR && PROP_TYPE(lpOldValue->ulPropTag) != PT_NULL) {
		// Compare old with new
		er = CompareProp(lpOldValue, lpNewValue, m_locale, &result);
		if (er != erSuccess)
			goto exit;
	}
	
	// Copy the value so we can track it for later (in UpdateMinMaxRemove) if we didn't have it yet
	er = CopyPropVal(lpNewValue, &lpNew);
	if(er != erSuccess)
		goto exit;
		
	iterMinMax = m_mapMinMax.find(sKey);
	if(iterMinMax == m_mapMinMax.end()) {
		m_mapMinMax.insert(std::make_pair(sKey, lpNew));
	} else {
		FreePropVal(iterMinMax->second, true); // NOTE this may free lpNewValue, so you can't use that anymore now
		iterMinMax->second = lpNew;
	}
	
	if(PROP_TYPE(lpOldValue->ulPropTag) == PT_ERROR || PROP_TYPE(lpOldValue->ulPropTag) == PT_NULL || (!fMax && result > 0) || (fMax && result < 0)) {
		// Either there was no old value, or the new value is larger or smaller than the old one
		er = SetProp(i, lpNew);
		if(er != erSuccess)
			goto exit;
	
		m_sCurMinMax = sKey;
					
		fModified = true;
	}
	
	if(lpfModified)
		*lpfModified = fModified;
	
exit:	
	return er;
}

/**
 * Update the min/max value to a row removal
 *
 * This function removes the value from the internal list of values, and checks if the new min/max value
 * differs from the last. It updates the category properties accordingly if needed.
 *
 * @param sKey Key of row that was removed
 * @param i Column id of min/max value
 * @param fMax TRUE if the column is a EC_TABLE_SORT_CATEG_MAX, FALSE if the column is EC_TABLE_SORT_CATEG_MIN
 * @param lpfModified TRUE if a new min/max value came into play due to the deletion
 * @return result
 */
ECRESULT ECCategory::UpdateMinMaxRemove(const sObjectTableKey &sKey, unsigned int i, bool fMax, bool *lpfModified)
{
	ECRESULT er = erSuccess;
	std::map<sObjectTableKey, struct propVal *>::iterator iterMinMax;
	bool fModified = false;
	
	
	iterMinMax = m_mapMinMax.find(sKey);
	
	if(iterMinMax == m_mapMinMax.end()) {
		er = ZARAFA_E_NOT_FOUND;
		goto exit;
	}
	
	FreePropVal(iterMinMax->second, true);
	m_mapMinMax.erase(iterMinMax);
	
	if(m_sCurMinMax == sKey) {
		fModified = true;
		
		// Reset old value
		FreePropVal(&this->m_lpProps[i], false);
		this->m_lpProps[i].ulPropTag = PR_NULL;
		
		// The min/max value until now was updated. Find the next min/max value.
		for(iterMinMax = m_mapMinMax.begin(); iterMinMax != m_mapMinMax.end(); iterMinMax++) {
			// Re-feed the values we had until now
			UpdateMinMax(iterMinMax->first, i, iterMinMax->second, fMax, NULL); // FIXME this 
		}
	}
	
	if(lpfModified)
		*lpfModified = fModified;
	
exit:
	return er;
}


void ECCategory::DecUnread() {
    m_ulUnread--;
}

void ECCategory::IncUnread() {
    m_ulUnread++;
}

unsigned int ECCategory::GetCount() {
    return m_ulLeafs;
}

/**
 * Get object size
 *
 * @return Object size in bytes
 */
unsigned int ECCategory::GetObjectSize()
{
	unsigned int ulSize = 0;
	unsigned int i;
	
	if (m_cProps > 0) {
		ulSize += sizeof(struct propVal) * m_cProps;

		for(i=0; i<m_cProps; i++) {
			ulSize += PropSize(&m_lpProps[i]);
		}
	}

	if (m_lpParent)
		ulSize += m_lpParent->GetObjectSize();

	return sizeof(*this) + ulSize;
}

/**
 * Get PR_DEPTH for an object in the table
 *
 * @param lpThis Pointer to generic object table instance
 * @param soap SOAP object for memory allocation
 * @param lpSession Session assiociated with the table
 * @param ulObjId Object ID of the object to get PR_DEPTH for
 * @param lpProp PropVal to write to
 * @return result
 */
ECRESULT ECGenericObjectTable::GetComputedDepth(struct soap *soap, ECSession *lpSession, unsigned int ulObjId, struct propVal *lpProp)
{
	lpProp->__union = SOAP_UNION_propValData_ul;
	lpProp->ulPropTag = PR_DEPTH;

	if(m_ulObjType == MAPI_MESSAGE) {
		// For contents tables, depth is equal to number of categories
		lpProp->Value.ul = GetCategories();
	} else {
		// For hierarchy tables, depth is 1 (see ECConvenientDepthTable.cpp for exception)
		lpProp->Value.ul = 1;
	}
		
	return erSuccess;
}
