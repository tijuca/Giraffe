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
#include <mapitags.h>
#include "EMSAbTag.h"

#include "ECSessionManager.h"
#include "ECConvenientDepthABObjectTable.h"
#include "ECSession.h"
#include "ECMAPI.h"
#include "stringutil.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

ECConvenientDepthABObjectTable::ECConvenientDepthABObjectTable(ECSession *lpSession, unsigned int ulABId, unsigned int ulABType, unsigned int ulABParentId, unsigned int ulABParentType, unsigned int ulFlags, const ECLocale &locale) : ECABObjectTable(lpSession, ulABId, ulABType, ulABParentId, ulABParentType, ulFlags, locale) {
    m_lpfnQueryRowData = ECConvenientDepthABObjectTable::QueryRowData;

	/* We require the details to construct the PR_EMS_AB_HIERARCHY_PATH */
	m_ulUserManagementFlags &= ~USERMANAGEMENT_IDS_ONLY;
}

ECRESULT ECConvenientDepthABObjectTable::Create(ECSession *lpSession, unsigned int ulABId, unsigned int ulABType, unsigned int ulABParentId, unsigned int ulABParentType, unsigned int ulFlags, const ECLocale &locale, ECConvenientDepthABObjectTable **lppTable)
{
	*lppTable = new ECConvenientDepthABObjectTable(lpSession, ulABId, ulABType, ulABParentId, ulABParentType, ulFlags, locale);

	(*lppTable)->AddRef();

	return erSuccess;
}

/*
 * We override the standard QueryRowData call so that we can correctly generate PR_DEPTH and PR_EMS_AB_HIERARCHY_PARENT. Since this is
 * dependent on data which is not available for ECUserManagement, we have to do those properties here.
 */
ECRESULT ECConvenientDepthABObjectTable::QueryRowData(ECGenericObjectTable *lpGenTable, struct soap *soap, ECSession *lpSession, ECObjectTableList* lpRowList, struct propTagArray *lpsPropTagArray, void* lpObjectData, struct rowSet **lppRowSet, bool bTableData,bool bTableLimit)
{
    ECRESULT er = erSuccess;
    ECObjectTableList::iterator iterRow;
    unsigned int n = 0;
    struct propVal *lpProp = NULL;
    ECConvenientDepthABObjectTable *lpThis = (ECConvenientDepthABObjectTable *)lpGenTable;
    
    er = ECABObjectTable::QueryRowData(lpThis, soap, lpSession, lpRowList, lpsPropTagArray, lpObjectData, lppRowSet, bTableData, bTableLimit);
    if(er != erSuccess)
        goto exit;

    // Insert the PR_DEPTH for all the rows since the row engine has no knowledge of depth
    for(iterRow=lpRowList->begin(); iterRow != lpRowList->end(); iterRow++, n++) {
        lpProp = FindProp(&(*lppRowSet)->__ptr[n], PROP_TAG(PT_ERROR, PROP_ID(PR_DEPTH)));
        
        if(lpProp) {
            lpProp->Value.ul = lpThis->m_mapDepth[iterRow->ulObjId];
            lpProp->ulPropTag = PR_DEPTH;
            lpProp->__union = SOAP_UNION_propValData_ul;
        }
        
        lpProp = FindProp(&(*lppRowSet)->__ptr[n], PROP_TAG(PT_ERROR, PROP_ID(PR_EMS_AB_HIERARCHY_PATH)));
        
        if(lpProp) {
            lpProp->Value.lpszA = s_strcpy(soap, lpThis->m_mapPath[iterRow->ulObjId].c_str());
            lpProp->ulPropTag = PR_EMS_AB_HIERARCHY_PATH;
            lpProp->__union = SOAP_UNION_propValData_lpszA;
        }
    }
        
exit:
    return er;
}


/*
 * Loads an entire multi-depth hierarchy table recursively.
 */
ECRESULT ECConvenientDepthABObjectTable::Load()
{
	ECRESULT er = erSuccess;
	ECODAB *lpODAB = (ECODAB*)m_lpObjectData;
	sObjectTableKey	sRowItem;
	std::list<localobjectdetails_t> *lpSubObjects = NULL;
	std::list<localobjectdetails_t>::iterator iterSubObjects;
	
	std::list<CONTAINERINFO> lstObjects;
	std::list<CONTAINERINFO>::iterator objectIter;
	CONTAINERINFO root;

	if (lpODAB->ulABType != MAPI_ABCONT) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	// Load this container
	root.ulId = lpODAB->ulABParentId;
	root.ulDepth = -1; // Our children are at depth 0, so the root object is depth -1. Note that this object is not actually added as a row in the end.
	root.strPath = "";
	
	lstObjects.push_back(root);

    // 'Recursively' loop through all our containers and add each of those children to our object list
    for(objectIter = lstObjects.begin(); objectIter != lstObjects.end(); objectIter++) {
        if(LoadHierarchyContainer(objectIter->ulId, 0, &lpSubObjects) == erSuccess) {
            for(iterSubObjects = lpSubObjects->begin(); iterSubObjects != lpSubObjects->end(); iterSubObjects++) {
                CONTAINERINFO folder;
                folder.ulId = iterSubObjects->ulId;
                folder.ulDepth = objectIter->ulDepth+1;
                folder.strPath = objectIter->strPath + "/" + iterSubObjects->GetPropString(OB_PROP_S_LOGIN);
                
                lstObjects.push_back(folder);
            }
            delete lpSubObjects;
            lpSubObjects = NULL;
        }
    }

    // Add all the rows into the row engine, except the root object (the folder itself does not show in it's own hierarchy table)
	for (objectIter = lstObjects.begin(); objectIter != lstObjects.end(); objectIter++) {
	    if(objectIter->ulId != lpODAB->ulABParentId) {
    	    m_mapDepth[objectIter->ulId] = objectIter->ulDepth;
    	    m_mapPath[objectIter->ulId] = objectIter->strPath;
	    	UpdateRow(ECKeyTable::TABLE_ROW_ADD, objectIter->ulId, 0);
        }
	}

exit:
    if (lpSubObjects)
        delete lpSubObjects;

    return er;
}
