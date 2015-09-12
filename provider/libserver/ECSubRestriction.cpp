/*
 * Copyright 2005 - 2015  Zarafa B.V. and its licensors
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License, version 3,
 * as published by the Free Software Foundation with the following
 * additional terms according to sec. 7:
 * 
 * "Zarafa" is a registered trademark of Zarafa B.V.
 * The licensing of the Program under the AGPL does not imply a trademark 
 * license. Therefore any rights, title and interest in our trademarks 
 * remain entirely with us.
 * 
 * Our trademark policy (see TRADEMARKS.txt) allows you to use our trademarks
 * in connection with Propagation and certain other acts regarding the Program.
 * In any case, if you propagate an unmodified version of the Program you are
 * allowed to use the term "Zarafa" to indicate that you distribute the Program.
 * Furthermore you may use our trademarks where it is necessary to indicate the
 * intended purpose of a product or service provided you use it in accordance
 * with honest business practices. For questions please contact Zarafa at
 * trademark@zarafa.com.
 *
 * The interactive user interface of the software displays an attribution 
 * notice containing the term "Zarafa" and/or the logo of Zarafa. 
 * Interactive user interfaces of unmodified and modified versions must 
 * display Appropriate Legal Notices according to sec. 5 of the GNU Affero 
 * General Public License, version 3, when you propagate unmodified or 
 * modified versions of the Program. In accordance with sec. 7 b) of the GNU 
 * Affero General Public License, version 3, these Appropriate Legal Notices 
 * must retain the logo of Zarafa or display the words "Initial Development 
 * by Zarafa" if the display of the logo is not reasonably feasible for
 * technical reasons.
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
#include <cassert>
#include "ECSubRestriction.h"

#include <mapidefs.h>
#include <mapitags.h>

#include "stringutil.h"
#include "ECSession.h"
#include "ECStoreObjectTable.h"
#include "ECGenericObjectTable.h"
#include "SOAPUtils.h"

#include "ECSessionManager.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static const char THIS_FILE[] = __FILE__;
#endif

static ECRESULT GetSubRestrictionRecursive(struct restrictTable *lpRestrict,
    unsigned int *lpulCount, unsigned int ulSubRestriction,
    struct restrictSub **lppSubRestrict, unsigned int maxdepth)
{
    ECRESULT er = erSuccess;
    unsigned int i = 0;
    unsigned int ulCount = 0;
    
    if(maxdepth == 0)
        return ZARAFA_E_TOO_COMPLEX;
        
    if(lpulCount == NULL) // If the caller didn't want to count restrictions, we still want to count internally
        lpulCount = &ulCount;
    assert(lpRestrict != NULL);
    
    switch(lpRestrict->ulType) {
        case RES_AND:
            for(i=0;i<lpRestrict->lpAnd->__size;i++) {
                er = GetSubRestrictionRecursive(lpRestrict->lpAnd->__ptr[i], lpulCount, ulSubRestriction, lppSubRestrict, maxdepth-1);
                if(er != erSuccess)
                    goto exit;
            }        
            break;
        case RES_OR:
            for(i=0;i<lpRestrict->lpOr->__size;i++) {
                er = GetSubRestrictionRecursive(lpRestrict->lpOr->__ptr[i], lpulCount, ulSubRestriction, lppSubRestrict, maxdepth-1);
                if(er != erSuccess)
                    goto exit;
            }        
            break;        
        case RES_NOT:
            er = GetSubRestrictionRecursive(lpRestrict->lpNot->lpNot, lpulCount, ulSubRestriction, lppSubRestrict, maxdepth-1);
            break;
        case RES_CONTENT:
        case RES_PROPERTY:
        case RES_COMPAREPROPS:
        case RES_BITMASK:
        case RES_SIZE:
        case RES_EXIST:
        case RES_COMMENT:
            break;
        case RES_SUBRESTRICTION:
            if(lpulCount && lppSubRestrict) {
                // Looking for a subrestriction
                if(*lpulCount == ulSubRestriction) {
                    *lppSubRestrict = lpRestrict->lpSub;
                }
            }
            // Counting subrestrictions
            if(lpulCount)
                (*lpulCount)++;
                    
            break;
        
    }
    
exit:
    return er;
}

ECRESULT GetSubRestrictionCount(struct restrictTable *lpRestrict, unsigned int *lpulCount)
{
    ECRESULT er = erSuccess;
    
	// Recursively get the amount of subqueries in the given restriction
	er = GetSubRestrictionRecursive(lpRestrict, lpulCount, 0, NULL, SUBRESTRICTION_MAXDEPTH);

	return er;
}

ECRESULT GetSubRestriction(struct restrictTable *lpBase, unsigned int ulCount, struct restrictSub **lppSubRestrict)
{
    ECRESULT er = erSuccess;
    
    er = GetSubRestrictionRecursive(lpBase, NULL, ulCount, lppSubRestrict, SUBRESTRICTION_MAXDEPTH);
    
    return er;
}

// Get results for all subqueries for a set of objects (should be freed with FreeSubRestrictionResults() )
ECRESULT RunSubRestrictions(ECSession *lpSession, void *lpECODStore, struct restrictTable *lpRestrict, ECObjectTableList *lpObjects, const ECLocale &locale, SUBRESTRICTIONRESULTS **lppResults)
{
    ECRESULT er = erSuccess;
    unsigned int i = 0;
    unsigned int ulCount = 0;
    SUBRESTRICTIONRESULTS *lpResults = NULL;
    SUBRESTRICTIONRESULT *lpResult = NULL;
    struct restrictSub *lpSubRestrict = NULL;
    
    er = GetSubRestrictionCount(lpRestrict, &ulCount);
    if(er != erSuccess)
        goto exit;
    
    lpResults = new SUBRESTRICTIONRESULTS;
    
    for(i=0;i<ulCount;i++) {
        er = GetSubRestriction(lpRestrict, i, &lpSubRestrict);
        if(er != erSuccess)
            goto exit;
            
        er = RunSubRestriction(lpSession, lpECODStore, lpSubRestrict, lpObjects, locale, &lpResult);
        if(er != erSuccess)
            goto exit;
            
        lpResults->push_back(lpResult);
    }
    
    *lppResults = lpResults;
    
exit:
    return er;
}

// Run a single subquery on a set of objects
ECRESULT RunSubRestriction(ECSession *lpSession, void *lpECODStore, struct restrictSub *lpRestrict, ECObjectTableList *lpObjects, const ECLocale &locale, SUBRESTRICTIONRESULT **lppResult)
{
    ECRESULT er = erSuccess;
    unsigned int ulType = 0;
    std::string strQuery;
    DB_RESULT lpDBResult = NULL;
    DB_ROW lpRow = NULL;
    struct propTagArray *lpPropTags = NULL;
    ECObjectTableList::iterator iterObject;
    ECObjectTableList lstSubObjects;
    std::map<unsigned int, unsigned int> mapParent;
    std::map<unsigned int, unsigned int>::iterator iterParent;
    SUBRESTRICTIONRESULT *lpResult = new SUBRESTRICTIONRESULT;
    struct rowSet *lpRowSet = NULL;
    bool fMatch = false;
    unsigned int ulSubObject = 0;
    unsigned int ulParent = 0;
    sObjectTableKey sKey;
    int i = 0;
    ECDatabase *lpDatabase = NULL;

	er = lpSession->GetDatabase(&lpDatabase);
    if(er != erSuccess)
        goto exit;

	if (lpObjects->empty())
		goto exit;				// nothing to search in, return success.
    
    assert(lpRestrict != NULL);
    switch(lpRestrict->ulSubObject) {
        case PR_MESSAGE_RECIPIENTS:
            ulType = MAPI_MAILUSER;
            break;
        case PR_MESSAGE_ATTACHMENTS:
            ulType = MAPI_ATTACH;
            break;
        default:
            er = ZARAFA_E_INVALID_PARAMETER;
            goto exit;
    }
    
    // Get property tags we'll be needing to evaluate the restriction
    er = ECGenericObjectTable::GetRestrictPropTags(lpRestrict->lpSubObject, NULL, &lpPropTags);
    if(er != erSuccess)
        goto exit;
    
    // Get the subobject id's we're querying from the database
    strQuery = "SELECT hierarchy.parent, hierarchy.id FROM hierarchy WHERE hierarchy.type = " + stringify(ulType) + " AND hierarchy.parent IN (";
    
    for(iterObject = lpObjects->begin(); iterObject != lpObjects->end(); iterObject++) {
        strQuery += stringify(iterObject->ulObjId);
        strQuery += ",";
    }
    
    // Remove trailing comma
    strQuery.resize(strQuery.size()-1);
    
    strQuery += ")";
    
    er = lpDatabase->DoSelect(strQuery, &lpDBResult);
    if(er != erSuccess)
        goto exit;
        
    while(1) {
        
        lpRow = lpDatabase->FetchRow(lpDBResult);
        
        if(lpRow == NULL)
            break;
            
        if(lpRow[0] == NULL || lpRow[1] == NULL)
            break;
            
        ulParent = atoi(lpRow[0]);
        ulSubObject = atoi(lpRow[1]);
        
        // Remember which subobject belongs to which object
        mapParent[ulSubObject] = ulParent;
        
        // Add an item to the rows we want to be querying
        sKey.ulObjId = ulSubObject;
        sKey.ulOrderId = 0;
        
        lstSubObjects.push_back(sKey);
    }

	if (lstSubObjects.empty())
		goto exit;				// no objects found, return success

    // lstSubObjects contains list of objects to match, mapParent contains mapping to parent
    
    // Get the actual row data from the database
    er = ECStoreObjectTable::QueryRowData(NULL, NULL, lpSession, &lstSubObjects, lpPropTags, lpECODStore, &lpRowSet, false, false, true);
    if(er != erSuccess)
        goto exit;
        
    iterObject = lstSubObjects.begin();
    // Loop through all the rows, see if they match
    for(i=0;i<lpRowSet->__size;i++) {
        er = ECGenericObjectTable::MatchRowRestrict(lpSession->GetSessionManager()->GetCacheManager(), &lpRowSet->__ptr[i], lpRestrict->lpSubObject, NULL, locale, &fMatch);
        if(er != erSuccess)
            goto exit;
            
        if(fMatch) {
            iterParent = mapParent.find(iterObject->ulObjId);
            if(iterParent != mapParent.end()) {
                // Remember the id of the message one of whose subobjects matched

                lpResult->insert(iterParent->second);
            }
        }
        
        // Optimisation possibility: if one of the subobjects matches, we shouldn't bother checking
        // other subobjects. This is a rather minor optimisation though.
        
        iterObject++; // lstSubObjects will always be in the same order as lpRowSet
    }

exit:
	if (er == erSuccess)
		*lppResult = lpResult;
	else
		delete lpResult;
    
    if(lpRowSet)
        FreeRowSet(lpRowSet, true);
        
    if(lpDBResult)
        lpDatabase->FreeResult(lpDBResult);
        
    if(lpPropTags)
        FreePropTagArray(lpPropTags);
        
    return er;
}

// Frees a SUBRESTRICTIONRESULTS object
ECRESULT FreeSubRestrictionResults(SUBRESTRICTIONRESULTS *lpResults) {
    ECRESULT er = erSuccess;
    
    SUBRESTRICTIONRESULTS::iterator iterResults;
    
    for(iterResults = lpResults->begin(); iterResults != lpResults->end(); iterResults++) {
        delete *iterResults;
    }
    
    delete lpResults;
    
    return er;
}

