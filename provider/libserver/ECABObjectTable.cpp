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
#include "ZarafaCode.h"

#include <mapidefs.h>
#include <mapitags.h>
#include "mapiext.h"

#include <EMSAbTag.h>

#include "SOAPUtils.h"
#include "ECABObjectTable.h"
#include "ECSecurity.h"

#include "ECSession.h"
#include "ECSessionManager.h"
#include "stringutil.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

ECABObjectTable::ECABObjectTable(ECSession *lpSession, unsigned int ulABId, unsigned int ulABType, unsigned int ulABParentId, unsigned int ulABParentType, unsigned int ulFlags, const ECLocale &locale) : ECGenericObjectTable(lpSession, ulABType, ulFlags, locale)
{
	ECODAB* lpODAB = new ECODAB;
	lpODAB->ulABId = ulABId;
	lpODAB->ulABType = ulABType;
	lpODAB->ulABParentId = ulABParentId;
	lpODAB->ulABParentType = ulABParentType;

	// Set dataobject
	m_lpObjectData = lpODAB;

	// Set callback function for queryrowdata
	m_lpfnQueryRowData = QueryRowData;

	// Filter out all objects which are hidden from addressbook. We don't need details either.
	m_ulUserManagementFlags = USERMANAGEMENT_IDS_ONLY | USERMANAGEMENT_ADDRESSBOOK;

	// Set the default sort order for Address Book entries
	struct sortOrder sObjectType[] = {
		{
			/*
			 * The Global Address Book must be the first entry
			 * in getHierarchyTable() otherwise it will not
			 * be set as default directory!
			 *
			 * To support future hierarchy additions like
			 * company subcontainers, or other global containers
			 * besides the GAB, we should sort the table using the
			 * PR_EMS_AB_HIERARCHY_PATH property which is formatted
			 * as followed for the GAB and Address Lists :
			 *
			 * 		/Global Address Book
			 *		/Global Address Book/Company A
			 *		/Global Address Book/Company B
			 *		/Global Address Lists/Addresslist A
			 *		/Global Address Lists/Addresslist B
			 */
			PR_EMS_AB_HIERARCHY_PATH,
			EC_TABLE_SORT_ASCEND,
		},
	};

	struct sortOrderArray sDefaultSortOrder = {
		sObjectType,
		sizeof(sObjectType) / sizeof(sObjectType[0]),
	};

	SetSortOrder(&sDefaultSortOrder, 0, 0);
}

ECABObjectTable::~ECABObjectTable()
{
	if(m_lpObjectData)
		delete (ECODAB*)m_lpObjectData;
}

ECRESULT ECABObjectTable::Create(ECSession *lpSession, unsigned int ulABId, unsigned int ulABType, unsigned int ulABParentId, unsigned int ulABParentType, unsigned int ulFlags, const ECLocale &locale, ECABObjectTable **lppTable)
{
	*lppTable = new ECABObjectTable(lpSession, ulABId, ulABType, ulABParentId, ulABParentType, ulFlags, locale);

	(*lppTable)->AddRef();

	return erSuccess;
}

ECRESULT ECABObjectTable::GetColumnsAll(ECListInt* lplstProps)
{
	ECRESULT		er = erSuccess;
	ECObjectTableMap::iterator		iterObjects;
	ECODAB *lpODAB = (ECODAB*)m_lpObjectData;

	pthread_mutex_lock(&m_hLock);

	ASSERT(lplstProps != NULL);
	
	//List always empty
	lplstProps->clear();

	// Add some generated and standard properties
	lplstProps->push_back(PR_DISPLAY_NAME);
	lplstProps->push_back(PR_ENTRYID);
	lplstProps->push_back(PR_DISPLAY_TYPE);
	lplstProps->push_back(PR_DISPLAY_TYPE_EX);
	lplstProps->push_back(PR_INSTANCE_KEY);
	lplstProps->push_back(PR_RECORD_KEY);
	lplstProps->push_back(PR_OBJECT_TYPE);
	lplstProps->push_back(PR_AB_PROVIDER_ID);
	lplstProps->push_back(PR_LAST_MODIFICATION_TIME);
	lplstProps->push_back(PR_CREATION_TIME);

	if(lpODAB->ulABType == MAPI_ABCONT) {
	    // Hierarchy table
    	lplstProps->push_back(PR_CONTAINER_FLAGS);
    	lplstProps->push_back(PR_DEPTH);
    	lplstProps->push_back(PR_EMS_AB_CONTAINERID);
	} else {
	    // Contents table
	    lplstProps->push_back(PR_SMTP_ADDRESS);
	}

	pthread_mutex_unlock(&m_hLock);


	return er;
}

ECRESULT ECABObjectTable::ReloadTableMVData(ECObjectTableList* lplistRows, ECListInt* lplistMVPropTag)
{
	ECRESULT			er = erSuccess;
	ECListIntIterator	iterListMVPropTag;

	ECObjectTableList::iterator	iterListRows;

	ASSERT(lplistMVPropTag->size() <2); //FIXME: Limit of one 1 MV column

	// scan for MV-props and add rows

	// Add items to list

	return er;
}


ECRESULT ECABObjectTable::GetMVRowCount(unsigned int ulObjId, unsigned int *lpulCount)
{
	ECRESULT er = erSuccess;
    ECObjectTableList			listRows;
	ECObjectTableList::iterator iterListRows;
	ECObjectTableMap::iterator		iterIDs;

	ECListInt::iterator	iterListMVPropTag;

	pthread_mutex_lock(&m_hLock);

	*lpulCount = 0;

	pthread_mutex_unlock(&m_hLock);

	return er;
}

ECRESULT ECABObjectTable::QueryRowData(ECGenericObjectTable *lpThis, struct soap *soap, ECSession *lpSession, ECObjectTableList* lpRowList, struct propTagArray *lpsPropTagArray, void* lpObjectData, struct rowSet **lppRowSet, bool bTableData, bool bTableLimit)
{
	ECRESULT er = erSuccess;
	ECODAB *lpODAB = (ECODAB*)lpObjectData;
	struct rowSet	*lpsRowSet = NULL;

	ASSERT(lpRowList != NULL);

	if (lpODAB->ulABType == MAPI_ABCONT)
		er = lpSession->GetUserManagement()->QueryHierarchyRowData(soap, lpRowList, lpsPropTagArray, &lpsRowSet);
	else if (lpODAB->ulABType == MAPI_MAILUSER || lpODAB->ulABType == MAPI_DISTLIST)
		er = lpSession->GetUserManagement()->QueryContentsRowData(soap, lpRowList, lpsPropTagArray, &lpsRowSet);
	else
		er = ZARAFA_E_INVALID_TYPE;
	if(er != erSuccess)
		goto exit;

	*lppRowSet = lpsRowSet;

exit:

	return er;
}

struct filter_objects {
	unsigned int m_ulFlags;
	filter_objects(unsigned int ulFlags) : m_ulFlags(ulFlags) {}

	bool operator()(const localobjectdetails_t &details)
	{
		return
			((m_ulFlags & AB_FILTER_SYSTEM) &&
			 (details.GetClass() == DISTLIST_SECURITY && details.ulId == ZARAFA_UID_EVERYONE)) ||
			((m_ulFlags & AB_FILTER_SYSTEM) &&
			 (details.GetClass() == ACTIVE_USER && details.ulId == ZARAFA_UID_SYSTEM)) ||
			((m_ulFlags & AB_FILTER_ADDRESSLIST) &&
			 (details.GetClass() == CONTAINER_ADDRESSLIST)) ||
			((m_ulFlags & AB_FILTER_CONTACTS) &&
			 (details.GetClass() == NONACTIVE_CONTACT));
	}
};

ECRESULT ECABObjectTable::LoadHierarchyAddressList(unsigned int ulObjectId, unsigned int ulFlags,
												   list<localobjectdetails_t> **lppObjects)
{
	ECRESULT er = erSuccess;
	list<localobjectdetails_t> *lpObjects = NULL;

	if (ulObjectId == ZARAFA_UID_GLOBAL_ADDRESS_BOOK ||
		ulObjectId == ZARAFA_UID_GLOBAL_ADDRESS_LISTS)
	{
		/* Global Address Book, load addresslist of the users own company */
		er = lpSession->GetSecurity()->GetUserCompany(&ulObjectId);
		if (er != erSuccess)
			goto exit;
	}

	er = lpSession->GetUserManagement()->GetCompanyObjectListAndSync(CONTAINER_ADDRESSLIST, ulObjectId, &lpObjects,
																	 m_ulUserManagementFlags);
	if (er != erSuccess)
		goto exit;

	/* Filter objects */
	if (ulFlags)
		lpObjects->remove_if(filter_objects(ulFlags));

	if (lppObjects) {
		*lppObjects = lpObjects;
		lpObjects = NULL;
	}

exit:
	if (lpObjects)
		delete lpObjects;

	return er;
}

ECRESULT ECABObjectTable::LoadHierarchyCompany(unsigned int ulObjectId, unsigned int ulFlags,
											   list<localobjectdetails_t> **lppObjects)
{
	ECRESULT er = erSuccess;
	list<localobjectdetails_t> *lpObjects = NULL;

	er = lpSession->GetSecurity()->GetViewableCompanyIds(m_ulUserManagementFlags, &lpObjects);
	if (er != erSuccess)
		goto exit;

	/*
	 * This looks a bit hackish, and it probably can be considered as such.
	 * If the size of lpObjects is 1 then the user can only view a single company space (namely his own).
	 * There are 2 reasons this could happen, either when working in a non-hosted environment or
	 * when the company does not have enough permissions to view any other company spaces.
	 * Having a Global Address Book _and_ a container for the company will only look strange and
	 * confusing for the user. So lets delete the entry altogether.
	 */
	if (lpObjects->size() == 1)
		lpObjects->clear();

	if (lppObjects) {
		*lppObjects = lpObjects;
		lpObjects = NULL;
	}

exit:
	if (lpObjects)
		delete lpObjects;

	return er;
}

ECRESULT ECABObjectTable::LoadHierarchyContainer(unsigned int ulObjectId, unsigned int ulFlags,
												 list<localobjectdetails_t> **lppObjects)
{
	ECRESULT er = erSuccess;
	list<localobjectdetails_t> *lpObjects = NULL;
	objectid_t objectid;

	if (ulObjectId == ZARAFA_UID_ADDRESS_BOOK) {
		/*
		 * Zarafa Address Book
		 * The Zarafa Address Book contains 2 containers,
		 * the first container is the Global Address Book,
		 * the second is the Global Address Lists container.
		 */
		lpObjects = new std::list<localobjectdetails_t>();
		lpObjects->push_back(localobjectdetails_t(ZARAFA_UID_GLOBAL_ADDRESS_BOOK, CONTAINER_COMPANY));
		if (!(m_ulUserManagementFlags & USERMANAGEMENT_IDS_ONLY))
			lpObjects->back().SetPropString(OB_PROP_S_LOGIN, ZARAFA_ACCOUNT_GLOBAL_ADDRESS_BOOK);

		lpObjects->push_back(localobjectdetails_t(ZARAFA_UID_GLOBAL_ADDRESS_LISTS, CONTAINER_ADDRESSLIST));
		if (!(m_ulUserManagementFlags & USERMANAGEMENT_IDS_ONLY))
			lpObjects->back().SetPropString(OB_PROP_S_LOGIN, ZARAFA_ACCOUNT_GLOBAL_ADDRESS_LISTS);

	} else if (ulObjectId == ZARAFA_UID_GLOBAL_ADDRESS_BOOK) {
		/*
		 * Global Address Book
		 * The hierarchy for the Global Address Book contains all visible
		 * companies and the addresslists belonging to this company.
		 *
		 * When in offline mode we only support the Global Address Book container,
		 * there are 2 reasons for this:
		 * 1) Currently offline mode does not assign users to companies, which means the lists are empty
		 *    except for the company itself and the System user.
		 * 2) Outlook generates the hierarchy view once during startup, when permissions change and
		 *    during sync one of the companies becomes unavailable, it will still be present
		 *    in the hierarchy view. The user will not be allowed to open it, so it isn't a real security risk,
		 *    but since we still have issue (1) open, we might as well disable the hierarchy view
		 *    containers completely. */
		er = LoadHierarchyCompany(ulObjectId, ulFlags, &lpObjects);
		if (er != erSuccess)
			goto exit;
	} else if (ulObjectId == ZARAFA_UID_GLOBAL_ADDRESS_LISTS) {
		if (lpSession->GetSecurity()->GetUserId() == ZARAFA_UID_SYSTEM) {
			er = ZARAFA_E_INVALID_PARAMETER;
			goto exit;
		}

		er = LoadHierarchyAddressList(ulObjectId, ulFlags, &lpObjects);
		if (er != erSuccess)
			goto exit;
	} else {
		/*
		 * Normal container
		 * Containers don't have additional subcontainers
		 */
		lpObjects = new std::list<localobjectdetails_t>();
	}

	if (lppObjects) {
		*lppObjects = lpObjects;
		lpObjects = NULL;
	}

exit:
	if (lpObjects)
		delete lpObjects;

	return er;
}

ECRESULT ECABObjectTable::LoadContentsAddressList(unsigned int ulObjectId, unsigned int ulFlags,
												  list<localobjectdetails_t> **lppObjects)
{
	ECRESULT er = erSuccess;
	list<localobjectdetails_t> *lpObjects = NULL;

	er = lpSession->GetUserManagement()->GetSubObjectsOfObjectAndSync(OBJECTRELATION_ADDRESSLIST_MEMBER, ulObjectId, &lpObjects,
																	  m_ulUserManagementFlags);
	if (er != erSuccess)
		goto exit;

	/* Filter objects */
	if (ulFlags)
		lpObjects->remove_if(filter_objects(ulFlags));

	if (lppObjects) {
		*lppObjects = lpObjects;
		lpObjects = NULL;
	}

exit:
	if (lpObjects)
		delete lpObjects;

	return er;
}

ECRESULT ECABObjectTable::LoadContentsCompany(unsigned int ulObjectId, unsigned int ulFlags,
											  list<localobjectdetails_t> **lppObjects)
{
	ECRESULT er = erSuccess;
	list<localobjectdetails_t> *lpObjects = NULL;

	er = lpSession->GetUserManagement()->GetCompanyObjectListAndSync(OBJECTCLASS_UNKNOWN, ulObjectId, &lpObjects,
																	 m_ulUserManagementFlags);
	if (er != erSuccess)
		goto exit;

	/* Filter objects */
	if (ulFlags)
		lpObjects->remove_if(filter_objects(ulFlags));

	if (lppObjects) {
		*lppObjects = lpObjects;
		lpObjects = NULL;
	}

exit:
	if (lpObjects)
		delete lpObjects;

	return er;
}

ECRESULT ECABObjectTable::LoadContentsDistlist(unsigned int ulObjectId, unsigned int ulFlags,
											   list<localobjectdetails_t> **lppObjects)
{
	ECRESULT er = erSuccess;
	list<localobjectdetails_t> *lpObjects = NULL;

	er = lpSession->GetUserManagement()->GetSubObjectsOfObjectAndSync(OBJECTRELATION_GROUP_MEMBER, ulObjectId, &lpObjects,
																	  m_ulUserManagementFlags);
	if (er != erSuccess)
		goto exit;

	/* Filter objects */
	if (ulFlags)
		lpObjects->remove_if(filter_objects(ulFlags));

	if (lppObjects) {
		*lppObjects = lpObjects;
		lpObjects = NULL;
	}

exit:
	if (lpObjects)
		delete lpObjects;

	return er;
}

ECRESULT ECABObjectTable::Load()
{
	ECRESULT er = erSuccess;
	ECODAB *lpODAB = (ECODAB*)m_lpObjectData;
	sObjectTableKey sRowItem;

	std::list<localobjectdetails_t> *lpObjects = NULL;
	std::list<localobjectdetails_t>::iterator iterObjects;
	std::list<unsigned int> lstObjects;
	unsigned int ulObjectId = 0;
	unsigned int ulObjectFilter = 0;
	objectid_t objectid;

	// If the GAB is disabled, don't show any entries except the top-level object
	if(lpODAB->ulABParentId != 0 && parseBool(lpSession->GetSessionManager()->GetConfig()->GetSetting("enable_gab")) == false && lpSession->GetSecurity()->GetAdminLevel() == 0)
	    goto exit;

    er = lpSession->GetSecurity()->IsUserObjectVisible(lpODAB->ulABParentId);
    if (er != erSuccess)
        goto exit;

	/*
	 * Check if we are loading the contents or hierarchy
	 */
	if (lpODAB->ulABType == MAPI_ABCONT) {
		/*
		 * Load hierarchy containing companies and addresslists
		 */
		if (lpODAB->ulABParentType != MAPI_ABCONT) {
			er = ZARAFA_E_INVALID_PARAMETER;
			goto exit;
		}

		er = LoadHierarchyContainer(lpODAB->ulABParentId, 0, &lpObjects);
		if (er != erSuccess)
			goto exit;
	} else if (lpODAB->ulABParentId == ZARAFA_UID_GLOBAL_ADDRESS_BOOK && lpODAB->ulABParentType == MAPI_ABCONT) {
		/*
		 * Load contents of Global Address Book
		 */
		er = lpSession->GetSecurity()->GetUserCompany(&ulObjectId);
		if (er != erSuccess)
			goto exit;

		er = LoadContentsCompany(ulObjectId, AB_FILTER_ADDRESSLIST, &lpObjects);
		if (er != erSuccess)
			goto exit;

		/*
		 * The company container always contains itself as distlist,
		 * unless ulCompanyId is 0 which is the default company and
		 * that one isn't visible.
		 */
		if (!ulObjectId)
			lpObjects->push_front(localobjectdetails_t(ulObjectId, CONTAINER_COMPANY));
	} else if (lpODAB->ulABParentId == ZARAFA_UID_GLOBAL_ADDRESS_LISTS && lpODAB->ulABParentType == MAPI_ABCONT) {
		/*
		 * Load contents of Global Address Lists
		 */
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	} else {
		/*
		 * Load contents of distlist, company or addresslist
		 */
		if (!lpSession->GetUserManagement()->IsInternalObject(lpODAB->ulABParentId)) {
			er = lpSession->GetUserManagement()->GetExternalId(lpODAB->ulABParentId, &objectid);
			if (er != erSuccess)
				goto exit;

			/* Security distribution lists (i.e. companies) should not contain contacts. Note that containers
			 * *do* contain contacts
			 */
			if (objectid.objclass == CONTAINER_COMPANY && lpODAB->ulABParentType == MAPI_DISTLIST)
				ulObjectFilter |= AB_FILTER_CONTACTS;
		} else {
			if (lpODAB->ulABParentId == ZARAFA_UID_SYSTEM) {
				objectid.objclass = ACTIVE_USER;
			} else if (lpODAB->ulABParentId == ZARAFA_UID_EVERYONE) {
				objectid.objclass = DISTLIST_SECURITY;
				/* Security distribution lists should not contain contacts */
				ulObjectFilter |= AB_FILTER_CONTACTS;
			}
		}

		/*
		 * System objects don't belong in container/distlist contents.
		 * Addresslists should only appear in hierarchy lists.
		 */
		ulObjectFilter |= AB_FILTER_SYSTEM | AB_FILTER_ADDRESSLIST;

		switch (objectid.objclass) {
		case DISTLIST_GROUP:
		case DISTLIST_SECURITY:
		case DISTLIST_DYNAMIC:
			er = LoadContentsDistlist(lpODAB->ulABParentId, ulObjectFilter, &lpObjects);
			if (er != erSuccess)
				goto exit;
			break;
		case CONTAINER_COMPANY:
			er = LoadContentsCompany(lpODAB->ulABParentId, ulObjectFilter, &lpObjects);
			if (er != erSuccess)
				goto exit;
			break;
		case CONTAINER_ADDRESSLIST:
			er = LoadContentsAddressList(lpODAB->ulABParentId, ulObjectFilter, &lpObjects);
			if (er != erSuccess)
				goto exit;
			break;
		default:
			er = ZARAFA_E_INVALID_PARAMETER;
			goto exit;
		}
	}

	for (iterObjects = lpObjects->begin(); iterObjects != lpObjects->end(); iterObjects++) {
		/* Only add visible items */
		if (lpSession->GetSecurity()->IsUserObjectVisible(iterObjects->ulId) != erSuccess)
			continue;

		lstObjects.push_back(iterObjects->ulId);
	}

	er = LoadRows(&lstObjects, 0);

exit:
	if(lpObjects)
		delete lpObjects;

	return er;
}
