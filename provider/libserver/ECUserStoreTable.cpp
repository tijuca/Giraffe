/*
 * Copyright 2005 - 2009  Zarafa B.V.
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
#include "edkmdb.h"

#include "ECSecurity.h"
#include "ECSessionManager.h"
#include "ECUserStoreTable.h"
#include "ECGenProps.h"
#include "ECSession.h"
#include "stringutil.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

// 1 == MAPI_STORE.. does it even matter?
ECUserStoreTable::ECUserStoreTable(ECSession *lpSession, unsigned int ulFlags, const ECLocale &locale) : 
	ECGenericObjectTable(lpSession, 1, ulFlags, locale)
{
	// Set callback function for queryrowdata (again?)
	m_lpfnQueryRowData = QueryRowData;
}

ECUserStoreTable::~ECUserStoreTable()
{
}

ECRESULT ECUserStoreTable::Create(ECSession *lpSession, unsigned int ulFlags, const ECLocale &locale, ECUserStoreTable **lppTable)
{
	*lppTable = new ECUserStoreTable(lpSession, ulFlags, locale);

	(*lppTable)->AddRef();

	return erSuccess;
}

ECRESULT ECUserStoreTable::QueryRowData(ECGenericObjectTable *lpThis, struct soap *soap, ECSession *lpSession, ECObjectTableList* lpRowList, struct propTagArray *lpsPropTagArray, void* lpObjectData, struct rowSet **lppRowSet, bool bCacheTableData, bool bTableLimit)
{
	ECRESULT er = erSuccess;
	ECUserStoreTable *pThis = dynamic_cast<ECUserStoreTable*>(lpThis);
	struct rowSet *lpsRowSet = NULL;
	ECObjectTableList::iterator iterRowList;
	int i, k;
	GUID sZeroGuid = {0};

	if (!lpThis) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	lpsRowSet = s_alloc<rowSet>(soap);
	lpsRowSet->__size = 0;
	lpsRowSet->__ptr = NULL;

	if(lpRowList->empty()) {
		*lppRowSet = lpsRowSet;
		goto exit; // success
	}

	// We return a square array with all the values
	lpsRowSet->__size = lpRowList->size();
	lpsRowSet->__ptr = s_alloc<propValArray>(soap, lpsRowSet->__size);
	memset(lpsRowSet->__ptr, 0, sizeof(propValArray) * lpsRowSet->__size);

	// Allocate memory for all rows
	for (i = 0; i < lpsRowSet->__size; i++) {
		lpsRowSet->__ptr[i].__size = lpsPropTagArray->__size;
		lpsRowSet->__ptr[i].__ptr = s_alloc<propVal>(soap, lpsPropTagArray->__size);
		memset(lpsRowSet->__ptr[i].__ptr, 0, sizeof(propVal) * lpsPropTagArray->__size);
	}

	for (i = 0, iterRowList = lpRowList->begin(); iterRowList != lpRowList->end(); iterRowList++, i++) {
		for (k = 0; k < lpsPropTagArray->__size; k++) {

			lpsRowSet->__ptr[i].__ptr[k].ulPropTag = PROP_TAG(PT_ERROR, lpsPropTagArray->__ptr[k]);
			lpsRowSet->__ptr[i].__ptr[k].__union = SOAP_UNION_propValData_ul;
			lpsRowSet->__ptr[i].__ptr[k].Value.ul = ZARAFA_E_NOT_FOUND;

			switch (PROP_ID(lpsPropTagArray->__ptr[k])) {
			case PROP_ID(PR_INSTANCE_KEY):
				// generate key 
				lpsRowSet->__ptr[i].__ptr[k].__union = SOAP_UNION_propValData_bin;
				lpsRowSet->__ptr[i].__ptr[k].ulPropTag = PR_INSTANCE_KEY;
				lpsRowSet->__ptr[i].__ptr[k].Value.bin = s_alloc<xsd__base64Binary>(soap);
				lpsRowSet->__ptr[i].__ptr[k].Value.bin->__size = sizeof(sObjectTableKey);
				lpsRowSet->__ptr[i].__ptr[k].Value.bin->__ptr = s_alloc<unsigned char>(soap, sizeof(sObjectTableKey));
				memcpy(lpsRowSet->__ptr[i].__ptr[k].Value.bin->__ptr, (void*)&(*iterRowList), sizeof(sObjectTableKey));
				break;

			case PROP_ID(PR_EC_USERNAME):
				if (!pThis->m_mapUserStoreData[iterRowList->ulObjId].strUsername.empty()) {
					lpsRowSet->__ptr[i].__ptr[k].ulPropTag = lpsPropTagArray->__ptr[k];

					lpsRowSet->__ptr[i].__ptr[k].__union = SOAP_UNION_propValData_lpszA;
					lpsRowSet->__ptr[i].__ptr[k].Value.lpszA = s_strcpy(soap, (char*)pThis->m_mapUserStoreData[iterRowList->ulObjId].strUsername.c_str());
				}
				break;
			case PROP_ID(PR_DISPLAY_NAME):
				if (!pThis->m_mapUserStoreData[iterRowList->ulObjId].strGuessname.empty()) {
					lpsRowSet->__ptr[i].__ptr[k].ulPropTag = lpsPropTagArray->__ptr[k];

					lpsRowSet->__ptr[i].__ptr[k].__union = SOAP_UNION_propValData_lpszA;
					lpsRowSet->__ptr[i].__ptr[k].Value.lpszA = s_strcpy(soap, (char*)pThis->m_mapUserStoreData[iterRowList->ulObjId].strGuessname.c_str());
				}
				break;
			case PROP_ID(PR_EC_STOREGUID):
				if (pThis->m_mapUserStoreData[iterRowList->ulObjId].sGuid != sZeroGuid) {
					lpsRowSet->__ptr[i].__ptr[k].ulPropTag = lpsPropTagArray->__ptr[k];

					lpsRowSet->__ptr[i].__ptr[k].__union = SOAP_UNION_propValData_bin;
					lpsRowSet->__ptr[i].__ptr[k].Value.bin = s_alloc<xsd__base64Binary>(soap);
					lpsRowSet->__ptr[i].__ptr[k].Value.bin->__size = sizeof(GUID);
					lpsRowSet->__ptr[i].__ptr[k].Value.bin->__ptr = s_alloc<unsigned char>(soap, sizeof(GUID));
					memcpy(lpsRowSet->__ptr[i].__ptr[k].Value.bin->__ptr, (unsigned char*)&pThis->m_mapUserStoreData[iterRowList->ulObjId].sGuid, sizeof(GUID));
				}
				break;
			case PROP_ID(PR_EC_STORETYPE):
				lpsRowSet->__ptr[i].__ptr[k].ulPropTag = lpsPropTagArray->__ptr[k];

				lpsRowSet->__ptr[i].__ptr[k].__union = SOAP_UNION_propValData_ul;
				lpsRowSet->__ptr[i].__ptr[k].Value.ul = pThis->m_mapUserStoreData[iterRowList->ulObjId].ulStoreType;
				break;
			case PROP_ID(PR_EC_COMPANYID):
				if (pThis->m_mapUserStoreData[iterRowList->ulObjId].ulCompanyId != 0) {
					lpsRowSet->__ptr[i].__ptr[k].ulPropTag = lpsPropTagArray->__ptr[k];

					lpsRowSet->__ptr[i].__ptr[k].__union = SOAP_UNION_propValData_ul;
					lpsRowSet->__ptr[i].__ptr[k].Value.ul = pThis->m_mapUserStoreData[iterRowList->ulObjId].ulCompanyId;
				}
				break;
			case PROP_ID(PR_EC_COMPANY_NAME):
				if (!pThis->m_mapUserStoreData[iterRowList->ulObjId].strCompanyName.empty()) {
					lpsRowSet->__ptr[i].__ptr[k].ulPropTag = lpsPropTagArray->__ptr[k];

					lpsRowSet->__ptr[i].__ptr[k].__union = SOAP_UNION_propValData_lpszA;
					lpsRowSet->__ptr[i].__ptr[k].Value.lpszA = s_strcpy(soap, (char*)pThis->m_mapUserStoreData[iterRowList->ulObjId].strCompanyName.c_str());
				}
				break;
			case PROP_ID(PR_STORE_ENTRYID):
				// ignore errors
				ECGenProps::GetPropComputedUncached(soap, lpSession, PR_STORE_ENTRYID, pThis->m_mapUserStoreData[iterRowList->ulObjId].ulObjId, 0,
							   				pThis->m_mapUserStoreData[iterRowList->ulObjId].ulObjId, 0, MAPI_STORE, &lpsRowSet->__ptr[i].__ptr[k]);
				break;
			case PROP_ID(PR_LAST_MODIFICATION_TIME):
				if (pThis->m_mapUserStoreData[iterRowList->ulObjId].tModTime != 0) {
					FILETIME ftTmp;
					UnixTimeToFileTime(pThis->m_mapUserStoreData[iterRowList->ulObjId].tModTime, &ftTmp);

					lpsRowSet->__ptr[i].__ptr[k].ulPropTag = lpsPropTagArray->__ptr[k];
					lpsRowSet->__ptr[i].__ptr[k].__union = SOAP_UNION_propValData_hilo;
					lpsRowSet->__ptr[i].__ptr[k].Value.hilo = s_alloc<struct hiloLong>(soap);
					lpsRowSet->__ptr[i].__ptr[k].Value.hilo->hi = ftTmp.dwHighDateTime;
					lpsRowSet->__ptr[i].__ptr[k].Value.hilo->lo = ftTmp.dwLowDateTime;
				}
				break;
			case PROP_ID(PR_MESSAGE_SIZE_EXTENDED):
				lpsRowSet->__ptr[i].__ptr[k].ulPropTag = lpsPropTagArray->__ptr[k];
				lpsRowSet->__ptr[i].__ptr[k].__union = SOAP_UNION_propValData_li;
				lpsRowSet->__ptr[i].__ptr[k].Value.li = pThis->m_mapUserStoreData[iterRowList->ulObjId].ullStoreSize;
				break;
			default:
				lpsRowSet->__ptr[i].__ptr[k].ulPropTag = PROP_TAG(PT_ERROR, lpsPropTagArray->__ptr[k]);

				lpsRowSet->__ptr[i].__ptr[k].__union = SOAP_UNION_propValData_ul;
				lpsRowSet->__ptr[i].__ptr[k].Value.ul = ZARAFA_E_NOT_FOUND;
				break;
			};
		}
	}

	*lppRowSet = lpsRowSet;

exit:
	return er;
}

ECRESULT ECUserStoreTable::Load() {
	ECRESULT er = erSuccess;
	ECListIntIterator i;
    ECDatabase *lpDatabase = NULL;
    DB_RESULT 	lpDBResult = NULL;
    DB_ROW		lpDBRow = NULL;
    DB_LENGTHS	lpDBLength = NULL;
	std::string strQuery;
    std::list<unsigned int> lstObjIds;
	ECUserStore sUserStore;
	int iRowId;
	ECUserManagement *lpUserManagement = lpSession->GetUserManagement();
	ECSecurity *lpSecurity = lpSession->GetSecurity();
	objectdetails_t sUserDetails;
	GUID sZeroGuid = {0};
	objectclass_t objclass;
	objectdetails_t sDetails;

	enum cols { USERID = 0, EXTERNID, OBJCLASS, UCOMPANY, STOREGUID, STORETYPE, USERNAME, SCOMPANY, HIERARCHYID, STORESIZE, MODTIME_HI, MODTIME_LO };

	er = lpSession->GetDatabase(&lpDatabase);
	if (er != erSuccess)
		goto exit;

    Clear();

	// we can't use WHERE to exclude wrong user types, because of NULL when there is a store, but not a user object
	strQuery =
		" SELECT u.id, u.externid, u.objectclass, u.company, s.guid, s.type, s.user_name, s.company, s.hierarchy_id, p.val_longint, m.val_hi, m.val_lo FROM users AS u"
		"  LEFT JOIN stores AS s ON s.user_id=u.id LEFT JOIN hierarchy AS h ON h.id=s.hierarchy_id"
		"  LEFT JOIN properties AS p ON p.hierarchyid=s.hierarchy_id and p.tag=0x0E08 and p.type=0x14"
		"  LEFT JOIN properties AS m ON m.hierarchyid=s.hierarchy_id and m.tag=0x3008 and m.type=0x40"
		" UNION"
		" SELECT u.id, u.externid, u.objectclass, u.company, s.guid, s.type, s.user_name, s.company, s.hierarchy_id, p.val_longint, m.val_hi, m.val_lo FROM users AS u"
		"  RIGHT JOIN stores AS s ON s.user_id=u.id LEFT JOIN hierarchy AS h ON h.id=s.hierarchy_id"
		"  LEFT JOIN properties AS p ON p.hierarchyid=s.hierarchy_id and p.tag=0x0E08 and p.type=0x14"
		"  LEFT JOIN properties AS m ON m.hierarchyid=s.hierarchy_id and m.tag=0x3008 and m.type=0x40";

	er = lpDatabase->DoSelect(strQuery, &lpDBResult);
	if(er != erSuccess)
		goto exit;

	iRowId = 0;
	while(1) {
		lpDBRow = lpDatabase->FetchRow(lpDBResult);

		if(lpDBRow == NULL)
			break;

		lpDBLength = lpDatabase->FetchRowLengths(lpDBResult);

		if (lpDBRow[OBJCLASS]) {
			objclass = (objectclass_t)atoi(lpDBRow[OBJCLASS]);
			if (objclass != ACTIVE_USER && objclass != NONACTIVE_USER &&
				objclass != NONACTIVE_ROOM && objclass != NONACTIVE_EQUIPMENT)
				continue;
		}

		if (lpDBRow[USERID]) {
			sUserStore.ulUserId = atoi(lpDBRow[USERID]);
			if (sUserStore.ulUserId == ZARAFA_UID_SYSTEM) // everyone already filtered by object type
				continue;
		} else {
			sUserStore.ulUserId = -1;
		}

		sUserStore.ulCompanyId = 0;
		if (lpDBRow[UCOMPANY])
			sUserStore.ulCompanyId = atoi(lpDBRow[UCOMPANY]);
		if (lpDBRow[SCOMPANY])
			sUserStore.ulCompanyId = atoi(lpDBRow[SCOMPANY]); // might override from user.company
		// check if we're admin over this company
		if (lpSecurity->IsAdminOverUserObject(sUserStore.ulCompanyId) != erSuccess)
			continue;

		if (lpDBRow[EXTERNID]) {
			sUserStore.sExternId.id.assign(lpDBRow[EXTERNID], lpDBLength[EXTERNID]);
			sUserStore.sExternId.objclass = objclass;
		} else {
			sUserStore.sExternId.id.clear();
			sUserStore.sExternId.objclass = OBJECTCLASS_UNKNOWN;
		}

		sUserStore.strUsername.clear();
		// find and override real username if possible
		if (sUserStore.ulUserId != -1 && lpUserManagement->GetObjectDetails(sUserStore.ulUserId, &sUserDetails) == erSuccess) {
			if (lpSession->GetSessionManager()->IsDistributedSupported()) {
				if (sUserDetails.GetPropString(OB_PROP_S_SERVERNAME).compare(lpSession->GetSessionManager()->GetConfig()->GetSetting("server_name")) != 0)
					continue;		// user not on this server
			}

			sUserStore.strUsername = sUserDetails.GetPropString(OB_PROP_S_LOGIN);
		}

		sUserStore.sGuid = sZeroGuid;
		if (lpDBRow[STOREGUID])
			memcpy(&sUserStore.sGuid, lpDBRow[STOREGUID], lpDBLength[STOREGUID]);

		if (lpDBRow[STORETYPE])
			sUserStore.ulStoreType = atoi(lpDBRow[STORETYPE]);
		else
			sUserStore.ulStoreType = ECSTORE_TYPE_PRIVATE; // or invalid value?
			
		if (lpDBRow[USERNAME])
			sUserStore.strGuessname = lpDBRow[USERNAME];
		else
			sUserStore.strGuessname.clear();

		if (sUserStore.ulCompanyId > 0 && lpUserManagement->GetObjectDetails(sUserStore.ulCompanyId, &sDetails) == erSuccess) {
			sUserStore.strCompanyName = sDetails.GetPropString(OB_PROP_S_LOGIN);
		}

		if(lpDBRow[HIERARCHYID])
			sUserStore.ulObjId = atoui(lpDBRow[HIERARCHYID]);
		else
			sUserStore.ulObjId = 0;

		sUserStore.tModTime = 0;
		if(lpDBRow[MODTIME_HI] && lpDBRow[MODTIME_LO]) {
			FILETIME ft;
			ft.dwHighDateTime = atoui(lpDBRow[MODTIME_HI]);
			ft.dwLowDateTime =  atoui(lpDBRow[MODTIME_LO]);
			sUserStore.tModTime = 0;
			FileTimeToUnixTime(ft, &sUserStore.tModTime);
		}

		if(lpDBRow[STORESIZE])
			sUserStore.ullStoreSize = atoll(lpDBRow[STORESIZE]);
		else
			sUserStore.ullStoreSize = 0;

		// add to table
		lstObjIds.push_back(iRowId);
		// remember details
		m_mapUserStoreData.insert(std::pair<unsigned int, ECUserStore>(iRowId++, sUserStore));
	}

	LoadRows(&lstObjIds, 0);

exit:	
	if (lpDBResult) {
		lpDatabase->FreeResult(lpDBResult);
		lpDBResult = NULL;
	}

	return er;
}
