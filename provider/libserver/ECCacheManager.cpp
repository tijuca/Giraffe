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

//
//////////////////////////////////////////////////////////////////////
#include "platform.h"

#include <mapidefs.h>
#include <mapitags.h>

#include "ECDatabaseMySQL.h"
#include "ECSessionManager.h"
#include "ECDatabaseUtils.h"

#include "ECCacheManager.h"

#include "ECMAPI.h"
#include "stringutil.h"
#include "ECGenericObjectTable.h"
#include "threadutil.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#include <algorithm>

// Specialization for ECsACL
template<>
unsigned int GetCacheAdditionalSize(const ECsACLs &val) {
	return val.ulACLs * sizeof(val.aACL[0]);
}

template<>
unsigned int GetCacheAdditionalSize(const ECsIndexProp &val) {
	return val.cbData;
}

// Specialization for ECsCell
template<>
unsigned int GetCacheAdditionalSize(const ECsCells &val) {
	return val.GetSize();
}


ECCacheManager::ECCacheManager(ECConfig *lpConfig, ECDatabaseFactory *lpDatabaseFactory, ECLogger *lpLogger)
: m_QuotaCache("quota", atoi(lpConfig->GetSetting("cache_quota_size")), atoi(lpConfig->GetSetting("cache_quota_lifetime")) * 60)
, m_QuotaUserDefaultCache("uquota", atoi(lpConfig->GetSetting("cache_quota_size")), atoi(lpConfig->GetSetting("cache_quota_lifetime")) * 60)
, m_ObjectsCache("obj", atoll(lpConfig->GetSetting("cache_object_size")), 0)
, m_StoresCache("store", atoi(lpConfig->GetSetting("cache_store_size")), 0)
, m_UserObjectCache("userid", atoi(lpConfig->GetSetting("cache_user_size")), atoi(lpConfig->GetSetting("cache_userdetails_lifetime")) * 60)
, m_UEIdObjectCache("extern", atoi(lpConfig->GetSetting("cache_user_size")), atoi(lpConfig->GetSetting("cache_userdetails_lifetime")) * 60)
, m_UserObjectDetailsCache("abinfo", atoi(lpConfig->GetSetting("cache_userdetails_size")), atoi(lpConfig->GetSetting("cache_userdetails_lifetime")) * 60)
, m_AclCache("acl", atoi(lpConfig->GetSetting("cache_acl_size")), 0)
, m_CellCache("cell", atoll(lpConfig->GetSetting("cache_cell_size")), 0)
, m_ServerDetailsCache("server", atoi(lpConfig->GetSetting("cache_server_size")), atoi(lpConfig->GetSetting("cache_server_lifetime")) * 60)
, m_PropToObjectCache("index1", atoll(lpConfig->GetSetting("cache_indexedobject_size")), 0)
, m_ObjectToPropCache("index2", atoll(lpConfig->GetSetting("cache_indexedobject_size")), 0)
{
	pthread_mutexattr_t mattr;
	pthread_mutexattr_init(&mattr);
	pthread_mutexattr_settype(&mattr, PTHREAD_MUTEX_RECURSIVE);

	pthread_mutex_init(&m_hCacheMutex, &mattr);
	pthread_mutex_init(&m_hCacheCellsMutex, &mattr);
	pthread_mutex_init(&m_hCacheIndPropMutex, &mattr);
	
	pthread_mutex_init(&m_hIndexedPropertiesMutex, NULL);

	pthread_mutexattr_destroy(&mattr);

	/* Initial cleaning/initialization of cache */
	PurgeCache(PURGE_CACHE_ALL);

#ifdef HAVE_SPARSEHASH
	// This is a requirement of sparse_hash_map. Since the value 0 is unused, we can use it as 'deleted' key
	m_mapObjects.set_deleted_key(-1);
	m_mapStores.set_deleted_key(-1);
	m_mapACLs.set_deleted_key(-1);
	m_mapQuota.set_deleted_key(-1);
	m_mapUserObject.set_deleted_key(-1);
	m_mapUserObjectDetails.set_deleted_key(-1);
	m_mapCells.set_deleted_key(-1);
#endif

 	/* Initialization of constants */
 	m_lpDatabaseFactory = lpDatabaseFactory;
 	m_lpLogger = lpLogger;
 	
 	m_bCellCacheDisabled = false;
}

ECCacheManager::~ECCacheManager()
{
	pthread_mutex_destroy(&m_hCacheIndPropMutex);
	pthread_mutex_destroy(&m_hCacheMutex);
	pthread_mutex_destroy(&m_hCacheCellsMutex);
	pthread_mutex_destroy(&m_hIndexedPropertiesMutex);
}

ECRESULT ECCacheManager::PurgeCache(unsigned int ulFlags)
{
	ECRESULT er = erSuccess;

	// cache mutex items
	pthread_mutex_lock(&m_hCacheMutex);

	if(ulFlags & PURGE_CACHE_QUOTA)
    	m_QuotaCache.ClearCache();
    if(ulFlags & PURGE_CACHE_QUOTADEFAULT)
    	m_QuotaUserDefaultCache.ClearCache();
    if(ulFlags & PURGE_CACHE_OBJECTS)
    	m_ObjectsCache.ClearCache();
    if(ulFlags & PURGE_CACHE_STORES)
    	m_StoresCache.ClearCache();
    if(ulFlags & PURGE_CACHE_ACL)
    	m_AclCache.ClearCache();

	pthread_mutex_unlock(&m_hCacheMutex);
	
	// Cell cache mutex
	pthread_mutex_lock(&m_hCacheCellsMutex);

	if(ulFlags & PURGE_CACHE_CELL)
    	m_CellCache.ClearCache();

	pthread_mutex_unlock(&m_hCacheCellsMutex);

	// Indexed properties mutex
	pthread_mutex_lock(&m_hCacheIndPropMutex);
	
	if(ulFlags & PURGE_CACHE_INDEX1)
    	m_PropToObjectCache.ClearCache();
    if(ulFlags & PURGE_CACHE_INDEX2)
    	m_ObjectToPropCache.ClearCache();
	
	pthread_mutex_unlock(&m_hCacheIndPropMutex);

	pthread_mutex_lock(&m_hIndexedPropertiesMutex);
	if(ulFlags & PURGE_CACHE_INDEXEDPROPERTIES)
    	m_mapIndexedProperties.clear();
	pthread_mutex_unlock(&m_hIndexedPropertiesMutex);

	pthread_mutex_lock(&m_hCacheMutex);

	if(ulFlags & PURGE_CACHE_USEROBJECT)
    	m_UserObjectCache.ClearCache();
    if(ulFlags & PURGE_CACHE_EXTERNID)
    	m_UEIdObjectCache.ClearCache();
    if(ulFlags & PURGE_CACHE_USERDETAILS)
    	m_UserObjectDetailsCache.ClearCache();
    if(ulFlags & PURGE_CACHE_SERVER)
    	m_ServerDetailsCache.ClearCache();

	pthread_mutex_unlock(&m_hCacheMutex);

	return er;
}

ECRESULT ECCacheManager::Update(unsigned int ulType, unsigned int ulObjId)
{
	ECRESULT		er = erSuccess;

	switch(ulType)
	{
		case fnevObjectModified:
			_DelACLs(ulObjId);
			_DelCell(ulObjId);
			_DelObject(ulObjId);
			break;
		case fnevObjectDeleted:
			_DelObject(ulObjId);
			_DelStore(ulObjId);
			_DelACLs(ulObjId);
			_DelCell(ulObjId);
			break;
		case fnevObjectMoved:
			_DelStore(ulObjId);
			_DelObject(ulObjId);
			_DelCell(ulObjId);
			break;
		default:
			//Do nothing
			break;
	}

	return er;
}

ECRESULT ECCacheManager::UpdateUser(unsigned int ulUserId)
{
	std::string strExternId;
	objectclass_t ulClass;

	if (_GetUserObject(ulUserId, &ulClass, NULL, &strExternId, NULL) == erSuccess)
		_DelUEIdObject(strExternId, ulClass);

	_DelUserObject(ulUserId);
	_DelUserObjectDetails(ulUserId);
	_DelQuota(ulUserId, false);
	_DelQuota(ulUserId, true);

	return erSuccess;
}

ECRESULT ECCacheManager::_GetObject(unsigned int ulObjId, unsigned int *ulParent, unsigned int *ulOwner, unsigned int *ulFlags, unsigned int *ulType)
{
	ECRESULT	er = erSuccess;
	ECsObjects	*sObject;
	scoped_lock lock(m_hCacheMutex);

	er = m_ObjectsCache.GetCacheItem(ulObjId, &sObject);
	if(er != erSuccess)
		goto exit;

	ASSERT((sObject->ulType != MAPI_FOLDER && (sObject->ulFlags & ~(MAPI_ASSOCIATED | MSGFLAG_DELETED)) == 0) || (sObject->ulType == MAPI_FOLDER));

	if(ulParent)
		*ulParent = sObject->ulParent;

	if(ulOwner)
		*ulOwner = sObject->ulOwner;

	if(ulFlags)
		*ulFlags = sObject->ulFlags;

	if(ulType)
		*ulType = sObject->ulType;

exit:
	return er;
}

ECRESULT ECCacheManager::SetObject(unsigned int ulObjId, unsigned int ulParent, unsigned int ulOwner, unsigned int ulFlags, unsigned int ulType)
{
	ECRESULT		er = erSuccess;
	ECsObjects		sObjects;

	if(ulParent == 0 || ulObjId == 0 || ulOwner == 0)
		return 1;

	ASSERT((ulType != MAPI_FOLDER && (ulFlags & ~(MAPI_ASSOCIATED | MSGFLAG_DELETED)) == 0) || (ulType == MAPI_FOLDER));

	sObjects.ulParent	= ulParent;
	sObjects.ulOwner	= ulOwner;
	sObjects.ulFlags	= ulFlags;
	sObjects.ulType		= ulType;

	scoped_lock lock(m_hCacheMutex);
	er = m_ObjectsCache.AddCacheItem(ulObjId, sObjects);

	return er;
}

ECRESULT ECCacheManager::_DelObject(unsigned int ulObjId)
{
	ECRESULT		er = erSuccess;
	scoped_lock		lock(m_hCacheMutex);

	er = m_ObjectsCache.RemoveCacheItem(ulObjId);

	return er;
}

ECRESULT ECCacheManager::_GetStore(unsigned int ulObjId, unsigned int *ulStore, GUID *lpGuid, unsigned int *lpulType)
{
	ECRESULT	er = erSuccess;
	ECsStores	*sStores;

	scoped_lock lock(m_hCacheMutex);

	er = m_StoresCache.GetCacheItem(ulObjId, &sStores);
	if(er != erSuccess)
		goto exit;

	if(ulStore)
		*ulStore = sStores->ulStore;
		
    if(lpulType)
        *lpulType = sStores->ulType;

	if(lpGuid)
		memcpy(lpGuid, &sStores->guidStore, sizeof(GUID) );
		
exit:
	return er;
}

ECRESULT ECCacheManager::SetStore(unsigned int ulObjId, unsigned int ulStore, GUID *lpGuid, unsigned int ulType)
{
	ECRESULT		er = erSuccess;
	ECsStores		sStores;
	sStores.ulStore = ulStore;
	sStores.guidStore = *lpGuid;
	sStores.ulType = ulType;

	scoped_lock lock(m_hCacheMutex);

	er = m_StoresCache.AddCacheItem(ulObjId, sStores);

	return er;
}

ECRESULT ECCacheManager::_DelStore(unsigned int ulObjId)
{
	ECRESULT		er = erSuccess;
	scoped_lock		lock(m_hCacheMutex);

	er = m_StoresCache.RemoveCacheItem(ulObjId);

	return er;
}

ECRESULT ECCacheManager::GetOwner(unsigned int ulObjId, unsigned int *ulOwner)
{
	ECRESULT	er = erSuccess;

	if(_GetObject(ulObjId, NULL, ulOwner, NULL, NULL) == erSuccess)
		goto exit;

	er = GetObject(ulObjId, NULL, ulOwner, NULL);

exit:
	return er;
}

ECRESULT ECCacheManager::GetParent(unsigned int ulObjId, unsigned int *lpulParent)
{
	ECRESULT er = erSuccess;
	unsigned int ulParent = 0;

	er = GetObject(ulObjId, &ulParent, NULL, NULL);

	if(er != erSuccess)
		goto exit;

	if(ulParent == CACHE_NO_PARENT) {
		er = ZARAFA_E_NOT_FOUND;
		goto exit;
	}

	*lpulParent = ulParent;
exit:
	return er;
}

ECRESULT ECCacheManager::QueryParent(unsigned int ulObjId, unsigned int *lpulParent)
{
    ECRESULT er = erSuccess;

    er = _GetObject(ulObjId, lpulParent, NULL, NULL, NULL);
    
    return er;
}

// Get the parent of the specified object
ECRESULT ECCacheManager::GetObject(unsigned int ulObjId, unsigned int *lpulParent, unsigned int *lpulOwner, unsigned int *lpulFlags, unsigned int *lpulType)
{
	ECRESULT	er = erSuccess;
	DB_RESULT	lpDBResult = NULL;
	DB_ROW		lpDBRow = NULL;
	std::string strQuery;
	ECDatabase	*lpDatabase = NULL;
	unsigned int	ulParent = 0, ulOwner = 0, ulFlags = 0, ulType = 0;

	er = GetThreadLocalDatabase(this->m_lpDatabaseFactory, &lpDatabase);

	if(er != erSuccess)
		goto exit;

	// first check the cache if the item exists
	if(_GetObject(ulObjId, &ulParent, &ulOwner, &ulFlags, &ulType) == erSuccess) {
		if(lpulParent)
			*lpulParent = ulParent;
		if(lpulOwner)
			*lpulOwner = ulOwner;
		if(lpulFlags)
			*lpulFlags = ulFlags;
		if(lpulType)
			*lpulType = ulType;
		goto exit;
	}

	strQuery = "SELECT hierarchy.parent, hierarchy.owner, hierarchy.flags, hierarchy.type FROM hierarchy WHERE hierarchy.id = " + stringify(ulObjId);
	er = lpDatabase->DoSelect(strQuery, &lpDBResult);
	if(er != erSuccess)
		goto exit;

	lpDBRow = lpDatabase->FetchRow(lpDBResult);

	if(lpDBRow == NULL) {
		er = ZARAFA_E_NOT_FOUND;
		goto exit;
	}

	if(lpDBRow[1] == NULL || lpDBRow[2] == NULL || lpDBRow[3] == NULL) {
		// owner or flags should not be NULL
		er = ZARAFA_E_DATABASE_ERROR;
		goto exit;
	}

	ulParent = lpDBRow[0] == NULL ? CACHE_NO_PARENT : atoui(lpDBRow[0]);
	ulOwner = atoui(lpDBRow[1]);
	ulFlags = atoui(lpDBRow[2]);
	ulType = atoui(lpDBRow[3]);

	if(lpulParent)
		*lpulParent = ulParent;
	if(lpulOwner)
		*lpulOwner = ulOwner;
	if(lpulFlags)
		*lpulFlags = ulFlags;
	if(lpulType)
		*lpulType = ulType;

	SetObject(ulObjId, ulParent, ulOwner, ulFlags, ulType);

exit:
	if (lpDBResult)
		lpDatabase->FreeResult(lpDBResult);

	return er;

}

// Get the store that the specified object belongs to
ECRESULT ECCacheManager::GetStore(unsigned int ulObjId, unsigned int *lpulStore, GUID *lpGuid, unsigned int maxdepth)
{
    return GetStoreAndType(ulObjId, lpulStore, lpGuid, NULL, maxdepth);
}

// Get the store that the specified object belongs to
ECRESULT ECCacheManager::GetStoreAndType(unsigned int ulObjId, unsigned int *lpulStore, GUID *lpGuid, unsigned int *lpulType, unsigned int maxdepth)
{
	ECRESULT	er = erSuccess;
	DB_RESULT	lpDBResult = NULL;
	DB_ROW		lpDBRow = NULL;
	std::string strQuery;
	ECDatabase	*lpDatabase = NULL;
	unsigned int ulSubObjId = 0;
	unsigned int ulStore = 0;
	unsigned int ulType = 0;
	GUID guid;
	
	if(maxdepth <= 0)
	    return ZARAFA_E_NOT_FOUND;

	er = GetThreadLocalDatabase(this->m_lpDatabaseFactory, &lpDatabase);

	if(er != erSuccess)
		goto exit;

	// first check the cache if we already know the store for this object
	if(_GetStore(ulObjId, &ulStore, &guid, &ulType) == erSuccess)
		goto found;

    // Get our parent folder
	if(GetParent(ulObjId, &ulSubObjId) != erSuccess) {
	    // No parent, this must be the top-level item, get the store data from here
    	strQuery = "SELECT hierarchy_id, guid, type FROM stores WHERE hierarchy_id = " + stringify(ulObjId);
    	er = lpDatabase->DoSelect(strQuery, &lpDBResult);
    	if(er != erSuccess)
	    	goto exit;

    	if(lpDatabase->GetNumRows(lpDBResult) < 1) {
    		er = ZARAFA_E_NOT_FOUND;
    		goto exit;
    	}

    	lpDBRow = lpDatabase->FetchRow(lpDBResult);

    	if(lpDBRow == NULL || lpDBRow[0] == NULL || lpDBRow[1] == NULL || lpDBRow[2] == NULL) {
    		er = ZARAFA_E_DATABASE_ERROR;
    		goto exit;
    	}
    	ulStore = atoi(lpDBRow[0]);
        memcpy(&guid, lpDBRow[1], sizeof(GUID));
        ulType = atoi(lpDBRow[2]);


	} else {
	    // We have a parent, get the store for our parent by recursively calling ourselves
	    er = GetStoreAndType(ulSubObjId, &ulStore, &guid, &ulType, maxdepth-1);
	    if(er != erSuccess)
	        goto exit;
    }

    // insert the item into the cache
    SetStore(ulObjId, ulStore, &guid, ulType);

found:    
    if(lpulStore)
        *lpulStore = ulStore;
    if(lpGuid)
        *lpGuid = guid;
    if(lpulType)
        *lpulType = ulType;

exit:
	if(lpDBResult)
		lpDatabase->FreeResult(lpDBResult);

	return er;
}

ECRESULT ECCacheManager::GetUserObject(unsigned int ulUserId, objectid_t *lpExternId, unsigned int *lpulCompanyId, std::string *lpstrSignature)
{
	ECRESULT	er = erSuccess;
	DB_RESULT	lpDBResult = NULL;
	DB_ROW		lpDBRow = NULL;
	DB_LENGTHS	lpDBLen = NULL;
	std::string	strQuery;
	ECDatabase	*lpDatabase = NULL;
	objectclass_t ulClass;
	unsigned int ulCompanyId;
	std::string externid;
	std::string signature;

	// first check the cache if we already know the external id for this user
	if (_GetUserObject(ulUserId, &ulClass, lpulCompanyId, &externid, lpstrSignature) == erSuccess) {
		if (lpExternId) {
			lpExternId->id = externid;
			lpExternId->objclass = ulClass;
		}
		goto exit;
	}

	er = GetThreadLocalDatabase(this->m_lpDatabaseFactory, &lpDatabase);
	if (er != erSuccess)
		goto exit;

	er = lpDatabase->DoSelect("SELECT externid, objectclass, signature, company FROM users "
							  "WHERE id=" + stringify(ulUserId), &lpDBResult);
	if (er != erSuccess) {
		er = ZARAFA_E_DATABASE_ERROR;
		goto exit;
	}

	lpDBRow = lpDatabase->FetchRow(lpDBResult);
	lpDBLen = lpDatabase->FetchRowLengths(lpDBResult);

	if(lpDBRow == NULL || lpDBRow[0] == NULL || lpDBRow[1] == NULL || lpDBRow[2] == NULL || lpDBRow[3] == NULL) {
		er = ZARAFA_E_NOT_FOUND;
		goto exit;
	}

	ulClass = (objectclass_t)atoui(lpDBRow[1]);
	ulCompanyId = atoui(lpDBRow[3]);

	externid.assign(lpDBRow[0], lpDBLen[0]);
	signature.assign(lpDBRow[2], lpDBLen[2]);

	// insert the item into the cache
	_AddUserObject(ulUserId, ulClass, ulCompanyId, externid, signature);

	if (lpExternId) {
		lpExternId->id = externid;
		lpExternId->objclass = ulClass;
	}

	if(lpulCompanyId)
		*lpulCompanyId = ulCompanyId;

	if(lpstrSignature)
		*lpstrSignature = signature;

exit:
	if(lpDBResult)
		lpDatabase->FreeResult(lpDBResult);

	return er;
}

ECRESULT ECCacheManager::GetUserDetails(unsigned int ulUserId, objectdetails_t *details)
{
	ECRESULT er = erSuccess;

	er = _GetUserObjectDetails(ulUserId, details);

	// on error, ECUserManagement will update the cache

	return er;
}

ECRESULT ECCacheManager::SetUserDetails(unsigned int ulUserId, objectdetails_t *details)
{
	ECRESULT er = erSuccess;

	er = _AddUserObjectDetails(ulUserId, details);

	return er;
}

ECRESULT ECCacheManager::GetUserObject(const objectid_t &sExternId, unsigned int *lpulUserId, unsigned int *lpulCompanyId, std::string *lpstrSignature)
{
	ECRESULT	er = erSuccess;
	DB_RESULT	lpDBResult = NULL;
	DB_ROW		lpDBRow = NULL;
	DB_LENGTHS	lpDBLen = NULL;
	std::string	strQuery;
	ECDatabase	*lpDatabase = NULL;
	unsigned int ulCompanyId;
	unsigned int ulUserId;
	std::string signature;
	objectclass_t objclass = sExternId.objclass;

	if (sExternId.id.empty()) {
		er = ZARAFA_E_DATABASE_ERROR;
		goto exit;
	}

	// first check the cache if we already know the external id for this user
	if (_GetUEIdObject(sExternId.id, sExternId.objclass, lpulCompanyId, lpulUserId, lpstrSignature) == erSuccess) {
		goto exit;
	}

	er = GetThreadLocalDatabase(this->m_lpDatabaseFactory, &lpDatabase);
	if (er != erSuccess)
		goto exit;

	strQuery =
		"SELECT id, signature, company, objectclass FROM users "
		"WHERE externid='" + lpDatabase->Escape(sExternId.id) + "' "
			"AND " + OBJECTCLASS_COMPARE_SQL("objectclass", sExternId.objclass);
	er = lpDatabase->DoSelect(strQuery, &lpDBResult);
	if (er != erSuccess) {
		er = ZARAFA_E_DATABASE_ERROR;
		goto exit;
	}

	// TODO: check, should return 1 answer

	lpDBRow = lpDatabase->FetchRow(lpDBResult);
	lpDBLen = lpDatabase->FetchRowLengths(lpDBResult);

	if(lpDBRow == NULL || lpDBRow[0] == NULL || lpDBRow[1] == NULL || lpDBRow[2] == NULL) {
		er = ZARAFA_E_NOT_FOUND;
		goto exit;
	}

	ulUserId = atoui(lpDBRow[0]);
	signature.assign(lpDBRow[1], lpDBLen[1]);
	ulCompanyId = atoui(lpDBRow[2]);

	// possibly update objectclass from database, to add the correct info in the cache
	if (OBJECTCLASS_ISTYPE(sExternId.objclass))
		objclass = (objectclass_t)atoi(lpDBRow[3]);

	// insert the item into the cache
	_AddUEIdObject(sExternId.id, objclass, ulCompanyId, ulUserId, signature);

	if(lpulCompanyId)
		*lpulCompanyId = ulCompanyId;

	if(lpulUserId)
		*lpulUserId = ulUserId;

	if(lpstrSignature)
		*lpstrSignature = signature;

exit:
	if(lpDBResult)
		lpDatabase->FreeResult(lpDBResult);

	return er;
}

ECRESULT ECCacheManager::GetUserObjects(const list<objectid_t> &lstExternObjIds, map<objectid_t, unsigned int> *lpmapLocalObjIds)
{
	ECRESULT er = erSuccess;
	DB_RESULT lpDBResult = NULL;
	DB_ROW lpDBRow = NULL;
	DB_LENGTHS lpDBLen = NULL;
	std::string strQuery;
	ECDatabase *lpDatabase = NULL;
	list<objectid_t> lstExternIds;
	list<objectid_t>::const_iterator iter;
	objectid_t sExternId;
	string strSignature;
	unsigned int ulLocalId = 0;
	unsigned int ulCompanyId;

	// Collect as many objects from cache as possible,
	// everything we couldn't find must be collected from the database

	for (iter = lstExternObjIds.begin(); iter != lstExternObjIds.end(); iter++) {
		if (_GetUEIdObject(iter->id, iter->objclass, NULL, &ulLocalId, NULL) == erSuccess)
			lpmapLocalObjIds->insert(make_pair(*iter, ulLocalId)); // object was found in cache
		else
			lstExternIds.push_back(*iter); // object was not found in cache
	}

	// Check if all objects have been collected from the cache
	if (lstExternIds.empty())
		goto exit;

	er = GetThreadLocalDatabase(this->m_lpDatabaseFactory, &lpDatabase);
	if (er != erSuccess)
		goto exit;

	strQuery = "SELECT id, externid, objectclass, signature, company FROM users WHERE ";
	for (iter = lstExternIds.begin(); iter != lstExternIds.end(); iter++) {
		if (iter != lstExternIds.begin())
			strQuery += " OR ";
		strQuery +=
			"(" + OBJECTCLASS_COMPARE_SQL("objectclass", iter->objclass) +
			" AND externid = '" + lpDatabase->Escape(iter->id) + "')";
	}

	er = lpDatabase->DoSelect(strQuery, &lpDBResult);
	if (er != erSuccess) {
		er = ZARAFA_E_DATABASE_ERROR;
		goto exit;
	}

	while (TRUE) {
		lpDBRow = lpDatabase->FetchRow(lpDBResult);
		lpDBLen = lpDatabase->FetchRowLengths(lpDBResult);

		if (lpDBRow == NULL || lpDBRow[0] == NULL || lpDBRow[1] == NULL || lpDBRow[2] == NULL || lpDBRow[3] == NULL || lpDBRow[4] == NULL)
			break;

		ulLocalId = atoi(lpDBRow[0]);
		sExternId.id.assign(lpDBRow[1], lpDBLen[1]);
		sExternId.objclass = (objectclass_t)atoi(lpDBRow[2]);
		strSignature.assign(lpDBRow[3], lpDBLen[3]);
		ulCompanyId = atoi(lpDBRow[4]);

		lpmapLocalObjIds->insert(make_pair(sExternId, ulLocalId));

		_AddUEIdObject(sExternId.id, sExternId.objclass, ulCompanyId, ulLocalId, strSignature);
	}

exit:
	if(lpDBResult)
		lpDatabase->FreeResult(lpDBResult);

	return er;
}

ECRESULT ECCacheManager::_AddUserObject(unsigned int ulUserId, objectclass_t ulClass, unsigned int ulCompanyId,
										std::string strExternId, std::string strSignature)
{
	ECRESULT	er = erSuccess;
	ECsUserObject sData;

	scoped_lock lock(m_hCacheMutex);

	if (OBJECTCLASS_ISTYPE(ulClass))
		goto exit;				// do not add incomplete data into the cache

	sData.ulClass = ulClass;
	sData.ulCompanyId = ulCompanyId;
	sData.strExternId = strExternId;
	sData.strSignature = strSignature;

	er = m_UserObjectCache.AddCacheItem(ulUserId, sData);

exit:
	return er;
}


ECRESULT ECCacheManager::_GetUserObject(unsigned int ulUserId, objectclass_t* lpulClass, unsigned int *lpulCompanyId,
										std::string* lpstrExternId, std::string* lpstrSignature)
{
	ECRESULT		er = erSuccess;
	ECsUserObject	*sData;

	scoped_lock lock(m_hCacheMutex);

	er = m_UserObjectCache.GetCacheItem(ulUserId, &sData);
	if(er != erSuccess)
		goto exit;

	if(lpulClass)
		*lpulClass = sData->ulClass;

	if(lpulCompanyId)
		*lpulCompanyId = sData->ulCompanyId;

	if(lpstrExternId)
		*lpstrExternId = sData->strExternId;

	if(lpstrSignature)
		*lpstrSignature = sData->strSignature;

exit:
	return er;
}

ECRESULT ECCacheManager::_DelUserObject(unsigned int ulUserId)
{
	ECRESULT			er = erSuccess;
	scoped_lock			lock(m_hCacheMutex);

	// Remove the user
	er = m_UserObjectCache.RemoveCacheItem(ulUserId);

	return er;
}

ECRESULT ECCacheManager::_AddUserObjectDetails(unsigned int ulUserId, objectdetails_t *details)
{
	ECRESULT 			er = erSuccess;
	ECsUserObjectDetails sObjectDetails;

	scoped_lock lock(m_hCacheMutex);

	if (!details) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	sObjectDetails.sDetails = *details;

	er = m_UserObjectDetailsCache.AddCacheItem(ulUserId, sObjectDetails);
	if (er != erSuccess)
		goto exit;

exit:
	return er;
}

ECRESULT ECCacheManager::_GetUserObjectDetails(unsigned int ulUserId, objectdetails_t *details)
{
	ECRESULT		er = erSuccess;
	ECsUserObjectDetails *sObjectDetails;

	scoped_lock lock(m_hCacheMutex);

	if (!details) { 
		er = ZARAFA_E_INVALID_PARAMETER; 
		goto exit; 
	}

	er = m_UserObjectDetailsCache.GetCacheItem(ulUserId, &sObjectDetails);
	if (er != erSuccess)
		goto exit;

	*details = sObjectDetails->sDetails; 

exit:
	return er;
}

ECRESULT ECCacheManager::_DelUserObjectDetails(unsigned int ulUserId)
{
	ECRESULT			er = erSuccess;
	scoped_lock			lock(m_hCacheMutex);

	// Remove the user details
	er = m_UserObjectDetailsCache.RemoveCacheItem(ulUserId);

	return er;
}

ECRESULT ECCacheManager::_AddUEIdObject(std::string strExternId, objectclass_t ulClass, unsigned int ulCompanyId,
						unsigned int ulUserId, std::string strSignature)
{
	ECRESULT	er = erSuccess;
	ECsUEIdKey sKey;
	ECsUEIdObject sData;

	scoped_lock lock(m_hCacheMutex);

	if (OBJECTCLASS_ISTYPE(ulClass))
		goto exit;				// do not add incomplete data into the cache

	sData.ulCompanyId = ulCompanyId;
	sData.ulUserId = ulUserId;
	sData.strSignature = strSignature;

	sKey.ulClass = ulClass;
	sKey.strExternId = strExternId;

	er = m_UEIdObjectCache.AddCacheItem(sKey, sData);
	if(er == erSuccess)
		goto exit;

exit:
	return er;
}

ECRESULT ECCacheManager::_GetUEIdObject(std::string strExternId, objectclass_t ulClass, unsigned int *lpulCompanyId,
						unsigned int* lpulUserId, std::string* lpstrSignature)
{
	ECRESULT		er = erSuccess;
	ECsUEIdKey		sKey;
	ECsUEIdObject	*sData;

	sKey.ulClass = ulClass;
	sKey.strExternId = strExternId;

	scoped_lock lock(m_hCacheMutex);

	er = m_UEIdObjectCache.GetCacheItem(sKey, &sData);
	if(er != erSuccess)
		goto exit;

	if(lpulCompanyId)
		*lpulCompanyId = sData->ulCompanyId;

	if(lpulUserId)
		*lpulUserId = sData->ulUserId;

	if(lpstrSignature)
		*lpstrSignature = sData->strSignature;

exit:
	return er;
}

ECRESULT ECCacheManager::_DelUEIdObject(std::string strExternId, objectclass_t ulClass)
{
	ECRESULT	er = erSuccess;
	ECsUEIdKey	sKey;

	// Remove the user
	sKey.strExternId = strExternId;
	sKey.ulClass = ulClass;

	scoped_lock lock(m_hCacheMutex);
	m_UEIdObjectCache.RemoveCacheItem(sKey);

	return er;
}

ECRESULT ECCacheManager::GetACLs(unsigned int ulObjId, struct rightsArray **lppRights)
{
    ECRESULT er = erSuccess;
    DB_RESULT	lpResult = NULL;
    DB_ROW		lpRow = NULL;
    ECDatabase *lpDatabase = NULL;
    std::string strQuery;
    struct rightsArray *lpRights = NULL;
    unsigned int ulRows = 0;

    // Try cache first
    if(_GetACLs(ulObjId, lppRights) == erSuccess)
        goto exit;

    // Failed, get it from the cache
	er = GetThreadLocalDatabase(this->m_lpDatabaseFactory, &lpDatabase);

	if(er != erSuccess)
		goto exit;

    strQuery = "SELECT id, type, rights FROM acl WHERE hierarchy_id=" + stringify(ulObjId);

    er = lpDatabase->DoSelect(strQuery, &lpResult);
    if(er != erSuccess)
        goto exit;

    ulRows = lpDatabase->GetNumRows(lpResult);

    lpRights = new struct rightsArray;
    if (ulRows > 0)
    {
	    lpRights->__size = ulRows;
		lpRights->__ptr = new struct rights [ulRows];
		memset(lpRights->__ptr, 0, sizeof(struct rights) * ulRows);

		for(unsigned int i=0;i<ulRows;i++) {
			lpRow = lpDatabase->FetchRow(lpResult);

			if(lpRow == NULL || lpRow[0] == NULL || lpRow[1] == NULL || lpRow[2] == NULL) {
				er = ZARAFA_E_DATABASE_ERROR;
				goto exit;
			}

			lpRights->__ptr[i].ulUserid = atoi(lpRow[0]);
			lpRights->__ptr[i].ulType = atoi(lpRow[1]);
			lpRights->__ptr[i].ulRights = atoi(lpRow[2]);
		}
	}
	else
	    memset(lpRights, 0, sizeof *lpRights);

    SetACLs(ulObjId, lpRights);

    *lppRights = lpRights;

exit:
    if(lpResult)
        lpDatabase->FreeResult(lpResult);

    return er;
}

ECRESULT ECCacheManager::_GetACLs(unsigned int ulObjId, struct rightsArray **lppRights)
{
    ECRESULT er = erSuccess;
    ECsACLs *sACL;
    struct rightsArray *lpRights = NULL;

	scoped_lock lock(m_hCacheMutex);

	er = m_AclCache.GetCacheItem(ulObjId, &sACL);
	if(er != erSuccess)
		goto exit;

    lpRights = new struct rightsArray;
    if (sACL->ulACLs > 0)
    {
        lpRights->__size = sACL->ulACLs;
        lpRights->__ptr = new struct rights [sACL->ulACLs];
        memset(lpRights->__ptr, 0, sizeof(struct rights) * sACL->ulACLs);

        for(unsigned int i=0;i<sACL->ulACLs;i++) {
            lpRights->__ptr[i].ulType = sACL->aACL[i].ulType;
            lpRights->__ptr[i].ulRights = sACL->aACL[i].ulMask;
            lpRights->__ptr[i].ulUserid = sACL->aACL[i].ulUserId;
        }
    }
    else
        memset(lpRights, 0, sizeof *lpRights);

    *lppRights = lpRights;

exit:
    return er;
}

ECRESULT ECCacheManager::SetACLs(unsigned int ulObjId, struct rightsArray *lpRights)
{
    ECsACLs sACLs;
    unsigned int i;
    ECRESULT er = erSuccess;

    sACLs.ulACLs = lpRights->__size;
    sACLs.aACL = new ECsACLs::ACL [lpRights->__size];

    for(i=0;i<lpRights->__size;i++) {
        sACLs.aACL[i].ulType = lpRights->__ptr[i].ulType;
        sACLs.aACL[i].ulMask = lpRights->__ptr[i].ulRights;
        sACLs.aACL[i].ulUserId = lpRights->__ptr[i].ulUserid;
    }

	scoped_lock lock(m_hCacheMutex);
	er = m_AclCache.AddCacheItem(ulObjId, sACLs);

	return er;
}

ECRESULT ECCacheManager::_DelACLs(unsigned int ulObjId)
{
    ECRESULT er = erSuccess;
	scoped_lock lock(m_hCacheMutex);

	er = m_AclCache.RemoveCacheItem(ulObjId);

    return er;
}

ECRESULT ECCacheManager::GetQuota(unsigned int ulUserId, bool bIsDefaultQuota, quotadetails_t *quota)
{
	ECRESULT er = erSuccess;

	// Try cache first
	er = _GetQuota(ulUserId, bIsDefaultQuota, quota);

	// on error, ECSecurity will update the cache

	return er;
}

ECRESULT ECCacheManager::SetQuota(unsigned int ulUserId, bool bIsDefaultQuota, quotadetails_t quota)
{
	ECRESULT er = erSuccess;
	ECsQuota	sQuota;

	sQuota.quota = quota;

	scoped_lock lock(m_hCacheMutex);

	if (bIsDefaultQuota)
		er = m_QuotaUserDefaultCache.AddCacheItem(ulUserId, sQuota);
	else
		er = m_QuotaCache.AddCacheItem(ulUserId, sQuota);

	return er;
}

ECRESULT ECCacheManager::_GetQuota(unsigned int ulUserId, bool bIsDefaultQuota, quotadetails_t *quota)
{
	ECRESULT er = erSuccess;
	ECsQuota	*sQuota;

	scoped_lock lock(m_hCacheMutex);

	if (!quota) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	if (bIsDefaultQuota)
		er = m_QuotaUserDefaultCache.GetCacheItem(ulUserId, &sQuota);
	else
		er = m_QuotaCache.GetCacheItem(ulUserId, &sQuota);

	if(er != erSuccess)
		goto exit;

	*quota = sQuota->quota;

exit:
	return er;
}

ECRESULT ECCacheManager::_DelQuota(unsigned int ulUserId, bool bIsDefaultQuota)
{
	ECRESULT er = erSuccess;
	scoped_lock lock(m_hCacheMutex);

	if (bIsDefaultQuota)
		er = m_QuotaUserDefaultCache.RemoveCacheItem(ulUserId);
	else
		er = m_QuotaCache.RemoveCacheItem(ulUserId);

	return er;
}

void ECCacheManager::ForEachCacheItem(void(callback)(const std::string &, const std::string &, const std::string &, void*), void *obj)
{
	string value;

	pthread_mutex_lock(&m_hCacheMutex);

	m_ObjectsCache.RequestStats(callback, obj);
	m_StoresCache.RequestStats(callback, obj);
	m_AclCache.RequestStats(callback, obj);
	m_QuotaCache.RequestStats(callback, obj);
	m_QuotaUserDefaultCache.RequestStats( callback, obj);
	m_UEIdObjectCache.RequestStats(callback, obj);
	m_UserObjectCache.RequestStats(callback, obj);
	m_UserObjectDetailsCache.RequestStats(callback, obj);
	m_ServerDetailsCache.RequestStats(callback, obj);
	
	pthread_mutex_unlock(&m_hCacheMutex);


	pthread_mutex_lock(&m_hCacheCellsMutex);

	m_CellCache.RequestStats(callback, obj);

	pthread_mutex_unlock(&m_hCacheCellsMutex);


	pthread_mutex_lock(&m_hCacheIndPropMutex);

	m_PropToObjectCache.RequestStats(callback, obj);
	m_ObjectToPropCache.RequestStats(callback, obj);

	pthread_mutex_unlock(&m_hCacheIndPropMutex);
}

ECRESULT ECCacheManager::DumpStats()
{
	m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Dumping cache stats:");

	pthread_mutex_lock(&m_hCacheMutex);

	m_ObjectsCache.DumpStats(m_lpLogger);
	m_StoresCache.DumpStats(m_lpLogger);
	m_AclCache.DumpStats(m_lpLogger);
	m_QuotaCache.DumpStats(m_lpLogger);
	m_QuotaUserDefaultCache.DumpStats(m_lpLogger);
	m_UEIdObjectCache.DumpStats(m_lpLogger);
	m_UserObjectCache.DumpStats(m_lpLogger);
	m_UserObjectDetailsCache.DumpStats(m_lpLogger);
	m_ServerDetailsCache.DumpStats(m_lpLogger);

	pthread_mutex_unlock(&m_hCacheMutex);


	pthread_mutex_lock(&m_hCacheCellsMutex);

	m_CellCache.DumpStats(m_lpLogger);

	pthread_mutex_unlock(&m_hCacheCellsMutex);


	pthread_mutex_lock(&m_hCacheIndPropMutex);

	m_PropToObjectCache.DumpStats(m_lpLogger);
	m_ObjectToPropCache.DumpStats(m_lpLogger);

	pthread_mutex_unlock(&m_hCacheIndPropMutex);

	return erSuccess;
}

ECRESULT ECCacheManager::GetObjectFlags(unsigned int ulObjId, unsigned int *ulFlags)
{
	return GetObject(ulObjId, NULL, NULL, ulFlags);
}

ECRESULT ECCacheManager::GetCell(sObjectTableKey* lpsRowItem, unsigned int ulPropTag, struct propVal *lpDest, struct soap *soap, bool bComputed)
{
    ECRESULT er = erSuccess;
    ECsCells *sCell;

	scoped_lock lock(m_hCacheCellsMutex);

    if (m_bCellCacheDisabled) {
        er = ZARAFA_E_NOT_FOUND;
        goto exit;
    }

    // only support caching order id 0 (non-multi-valued)
	if(lpsRowItem->ulOrderId != 0) {
		er = ZARAFA_E_NOT_FOUND;
		goto exit;
	}

	er = m_CellCache.GetCacheItem(lpsRowItem->ulObjId, &sCell);
	if(er != erSuccess)
	    goto exit;

    if(!sCell->GetPropVal(ulPropTag, lpDest, soap)) {
        if(!sCell->GetComplete() || bComputed) {
            // Object is not complete, and item is not in cache. We simply don't know anything about
            // the item, so return NOT_FOUND. Or, the item is complete but the requested property is computed, and therefore
            // not in the cache.
			m_CellCache.DecrementValidCount();
            er = ZARAFA_E_NOT_FOUND;
            goto exit;
        } else {
            // Object is complete and property is not found; we know that the property does not exist
            // so return OK with a NOT_FOUND propvalue
            lpDest->ulPropTag = PROP_TAG(PT_ERROR, PROP_ID(ulPropTag));
            lpDest->Value.ul = ZARAFA_E_NOT_FOUND;
            lpDest->__union = SOAP_UNION_propValData_ul;
        }
    }

exit:
    return er;
}

ECRESULT ECCacheManager::SetCell(sObjectTableKey* lpsRowItem, unsigned int ulPropTag, struct propVal *lpSrc)
{
    ECRESULT er = erSuccess;
    ECsCells *sCell;

    if(lpsRowItem->ulOrderId != 0)
        return ZARAFA_E_NOT_FOUND;

	scoped_lock lock(m_hCacheCellsMutex);

	if (m_CellCache.GetCacheItem(lpsRowItem->ulObjId, &sCell) == erSuccess) {
        long long ulSize = sCell->GetSize();
        sCell->AddPropVal(ulPropTag, lpSrc);
        ulSize -= sCell->GetSize();
        
        // ulSize is positive if the cache shrank
        //m_ulCellSize -= ulSize;
		m_CellCache.AddToSize(-ulSize);
    } else {
        ECsCells sNewCell;
        
        sNewCell.AddPropVal(ulPropTag, lpSrc);
        
		er = m_CellCache.AddCacheItem(lpsRowItem->ulObjId, sNewCell);
    	if(er != erSuccess)
    	    goto exit;
    }

exit:
    return er;
}

ECRESULT ECCacheManager::SetComplete(unsigned int ulObjId)
{
    ECRESULT er = erSuccess;
    ECsCells *sCell;
    
	scoped_lock lock(m_hCacheCellsMutex);

	if (m_CellCache.GetCacheItem(ulObjId, &sCell) == erSuccess) {
        sCell->SetComplete(true);
    } else {
        er = ZARAFA_E_NOT_FOUND;
        goto exit;
    }

exit:
    return er;
}

ECRESULT ECCacheManager::UpdateCell(unsigned int ulObjId, unsigned int ulPropTag, int lDelta)
{
    ECRESULT er = erSuccess;
    ECsCells *sCell;
    
	scoped_lock lock(m_hCacheCellsMutex);

	if (m_CellCache.GetCacheItem(ulObjId, &sCell) == erSuccess) {
        sCell->UpdatePropVal(ulPropTag, lDelta);
    } else {
        er = ZARAFA_E_NOT_FOUND;
        goto exit;
    }

exit:
    return er;
}

ECRESULT ECCacheManager::UpdateCell(unsigned int ulObjId, unsigned int ulPropTag, unsigned int ulMask, unsigned int ulValue)
{
    ECRESULT er = erSuccess;
    ECsCells *sCell;
    
	scoped_lock lock(m_hCacheCellsMutex);

	if (m_CellCache.GetCacheItem(ulObjId, &sCell) == erSuccess) {
        sCell->UpdatePropVal(ulPropTag, ulMask, ulValue);
    } else {
        er = ZARAFA_E_NOT_FOUND;
        goto exit;
    }

exit:
    return er;
}

ECRESULT ECCacheManager::_DelCell(unsigned int ulObjId)
{
    ECRESULT er = erSuccess;
	scoped_lock lock(m_hCacheCellsMutex);
    
	er = m_CellCache.RemoveCacheItem(ulObjId);

	return er;
}

ECRESULT ECCacheManager::GetServerDetails(const std::string &strServerId, serverdetails_t *lpsDetails)
{
	ECRESULT			er = erSuccess;
	ECsServerDetails	*sEntry;
	std::string			strServerIdLc;

	// make the key lowercase
	strServerIdLc.reserve(strServerId.size());
	std::transform(strServerId.begin(), strServerId.end(), std::back_inserter(strServerIdLc), ::tolower);

	scoped_lock lock(m_hCacheMutex);

	er = m_ServerDetailsCache.GetCacheItem(strServerIdLc, &sEntry);
	if (er != erSuccess)
		goto exit;

	if (lpsDetails)
		*lpsDetails = sEntry->sDetails;

exit:
	return er;
}

ECRESULT ECCacheManager::SetServerDetails(const std::string &strServerId, const serverdetails_t &sDetails)
{
	ECsServerDetails	sEntry;
	std::string			strServerIdLc;

	// make the key lowercase
	strServerIdLc.reserve(strServerId.size());
	std::transform(strServerId.begin(), strServerId.end(), std::back_inserter(strServerIdLc), ::tolower);

	sEntry.sDetails = sDetails;

	scoped_lock lock(m_hCacheMutex);
	return m_ServerDetailsCache.AddCacheItem(strServerIdLc, sEntry);
}

ECRESULT ECCacheManager::RemoveIndexData(unsigned int ulObjId)
{
	ECRESULT				er = erSuccess;
	ECsIndexObject	sObjectKeyLower, sObjectKeyUpper;
	ECMapObjectToProp::iterator	iter, iDel;
	ECMapPropToObject::iterator	iterPropToObj;

	std::list<ECMapObjectToProp::value_type> lstItems;
	std::list<ECMapObjectToProp::value_type>::iterator iItem;

	// Get all records with specified hierarchyid and all tags (0 -> 0xffffffff)
	sObjectKeyLower.ulObjId = ulObjId;
	sObjectKeyLower.ulTag = 0;
	sObjectKeyUpper.ulObjId = ulObjId;
	sObjectKeyUpper.ulTag = 0xffffffff;

	scoped_lock lock(m_hCacheIndPropMutex);

	er = m_ObjectToPropCache.GetCacheRange(sObjectKeyLower, sObjectKeyUpper, &lstItems);
	for (iItem = lstItems.begin(); iItem != lstItems.end(); ++iItem) {
		m_ObjectToPropCache.RemoveCacheItem(iItem->first);
		m_PropToObjectCache.RemoveCacheItem(iItem->second);
	}

	return er;
}


ECRESULT ECCacheManager::RemoveIndexData(unsigned int ulPropTag, unsigned int cbData, unsigned char *lpData)
{
	ECRESULT				er = erSuccess;
	ECsIndexProp	sObject;
	ECsIndexObject	*sObjectId;

	if(lpData == NULL || cbData == 0) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	sObject.ulTag = PROP_ID(ulPropTag);
	sObject.cbData = cbData;
	sObject.lpData = lpData; // Cheap copy, Set this item on NULL before you exit

	{
        scoped_lock lock(m_hCacheIndPropMutex);

        if(m_PropToObjectCache.GetCacheItem(sObject, &sObjectId) == erSuccess) {
            m_ObjectToPropCache.RemoveCacheItem(*sObjectId);
            m_PropToObjectCache.RemoveCacheItem(sObject);
        }
	}

    // Make sure there's no delete when it goes out of scope	
	sObject.lpData = NULL;

exit:
	return er;
}

ECRESULT ECCacheManager::RemoveIndexData(unsigned int ulPropTag, unsigned int ulObjId)
{
	ECRESULT				er = erSuccess;
	ECsIndexObject	sObject;
	ECsIndexProp	*sObjectId;

	sObject.ulTag = PROP_ID(ulPropTag);
	sObject.ulObjId = ulObjId;

	{
        scoped_lock lock(m_hCacheIndPropMutex);

        if(m_ObjectToPropCache.GetCacheItem(sObject, &sObjectId) == erSuccess) {
            m_PropToObjectCache.RemoveCacheItem(*sObjectId);
            m_ObjectToPropCache.RemoveCacheItem(sObject);
        }
	}

	return er;
}

ECRESULT ECCacheManager::_AddIndexData(ECsIndexObject* lpObject, ECsIndexProp* lpProp)
{
	ECRESULT	er = erSuccess;
	scoped_lock lock(m_hCacheIndPropMutex);

    // Remove any pre-existing references to this data
    RemoveIndexData(PROP_TAG(PT_UNSPECIFIED, lpObject->ulTag), lpObject->ulObjId);
    RemoveIndexData(PROP_TAG(PT_UNSPECIFIED, lpProp->ulTag), lpProp->cbData, lpProp->lpData);
    
	er = m_PropToObjectCache.AddCacheItem(*lpProp, *lpObject);
	if(er != erSuccess)
		goto exit;

	er = m_ObjectToPropCache.AddCacheItem(*lpObject, *lpProp);
	if(er != erSuccess)
		goto exit;

exit:
	return er;
}

ECRESULT ECCacheManager::GetPropFromObject(unsigned int ulTag, unsigned int ulObjId, struct soap *soap, unsigned int* lpcbData, unsigned char** lppData)
{
	ECRESULT		er = erSuccess;
	DB_RESULT		lpDBResult = NULL;
	DB_ROW			lpDBRow = NULL;
	DB_LENGTHS		lpDBLenths = NULL;
	std::string		strQuery;
	ECDatabase*		lpDatabase = NULL;
	ECsIndexProp	*sObject;
	ECsIndexObject	sObjectKey;
    ECsIndexProp sNewObject;

	sObjectKey.ulObjId = ulObjId;
	sObjectKey.ulTag = ulTag;

	{
		scoped_lock lock(m_hCacheIndPropMutex);
		er = m_ObjectToPropCache.GetCacheItem(sObjectKey, &sObject);
		
		if(er == erSuccess) {
			*lppData = s_alloc<unsigned char>(soap, sObject->cbData);
			*lpcbData = sObject->cbData;

			memcpy(*lppData, sObject->lpData, sObject->cbData);
			
			// All done
			goto exit;
		}
	}

	// item not found, search in the database
	er = GetThreadLocalDatabase(this->m_lpDatabaseFactory, &lpDatabase);
	if(er != erSuccess)
		goto exit;

	// Get them from the database
	strQuery = "SELECT val_binary FROM indexedproperties FORCE INDEX(PRIMARY) WHERE tag="+stringify(ulTag)+" AND hierarchyid="+stringify(ulObjId);
	er = lpDatabase->DoSelect(strQuery, &lpDBResult);
	if(er != erSuccess)
		goto exit;

	lpDBRow = lpDatabase->FetchRow(lpDBResult);
	lpDBLenths = lpDatabase->FetchRowLengths(lpDBResult);
	if(lpDBRow == NULL || lpDBRow[0] == NULL || lpDBLenths == NULL) {
		er = ZARAFA_E_NOT_FOUND;
		goto exit;
	}

	sNewObject.SetValue(ulTag, (unsigned char*) lpDBRow[0], (unsigned int) lpDBLenths[0]);

	er = _AddIndexData(&sObjectKey, &sNewObject);
	if(er != erSuccess)
		goto exit;

	sObject = &sNewObject;

	*lppData = s_alloc<unsigned char>(soap, sObject->cbData);
	*lpcbData = sObject->cbData;

	memcpy(*lppData, sObject->lpData, sObject->cbData);

exit:
	if (lpDBResult)
		lpDatabase->FreeResult(lpDBResult);

	return er;
}

ECRESULT ECCacheManager::GetObjectFromProp(unsigned int ulTag, unsigned int cbData, unsigned char* lpData, unsigned int* lpulObjId)
{
	ECRESULT		er = erSuccess;
	DB_RESULT		lpDBResult = NULL;
	DB_ROW			lpDBRow = NULL;
	std::string		strQuery;
	ECDatabase*		lpDatabase = NULL;
    ECsIndexObject sNewIndexObject;
	ECsIndexProp	sObject;

	if(lpData == NULL || lpulObjId == NULL || cbData == 0) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	if(QueryObjectFromProp(ulTag, cbData, lpData, lpulObjId) == erSuccess)
	    goto exit;

	// Item not found, search in database
    er = GetThreadLocalDatabase(this->m_lpDatabaseFactory, &lpDatabase);
    if(er != erSuccess)
        goto exit;

    // Get them from the database
    strQuery = "SELECT hierarchyid FROM indexedproperties FORCE INDEX(bin) WHERE tag="+stringify(ulTag)+" AND val_binary="+ lpDatabase->EscapeBinary(lpData, cbData);
    er = lpDatabase->DoSelect(strQuery, &lpDBResult);
    if(er != erSuccess)
        goto exit;

    lpDBRow = lpDatabase->FetchRow(lpDBResult);
    if(lpDBRow == NULL || lpDBRow[0] == NULL) {
        er = ZARAFA_E_NOT_FOUND;
        goto exit;
    }

    sNewIndexObject.ulTag = ulTag;
    sNewIndexObject.ulObjId = atoui(lpDBRow[0]);

	sObject.ulTag = ulTag;
	sObject.cbData = cbData;
	sObject.lpData = lpData; // Cheap copy, Set this item on NULL before you exit

    er = _AddIndexData(&sNewIndexObject, &sObject);
    if(er != erSuccess)
        goto exit;
        
	*lpulObjId = sNewIndexObject.ulObjId;

exit:

	if (lpDBResult)
		lpDatabase->FreeResult(lpDBResult);

	sObject.lpData = NULL; // Remove reference

	return er;
}

ECRESULT ECCacheManager::QueryObjectFromProp(unsigned int ulTag, unsigned int cbData, unsigned char* lpData, unsigned int* lpulObjId)
{
	ECRESULT		er = erSuccess;
	ECsIndexProp	sObject;
	ECsIndexObject	*sIndexObject;

	if(lpData == NULL || lpulObjId == NULL || cbData == 0) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	sObject.ulTag = ulTag;
	sObject.cbData = cbData;
	sObject.lpData = lpData; // Cheap copy, Set this item on NULL before you exit

	{
		scoped_lock lock(m_hCacheIndPropMutex);
		er = m_PropToObjectCache.GetCacheItem(sObject, &sIndexObject);
		if(er != erSuccess)
		    goto exit;
		    
		*lpulObjId = sIndexObject->ulObjId;
	}
	
exit:
	sObject.lpData = NULL;
    return er;
}

ECRESULT ECCacheManager::SetObjectProp(unsigned int ulTag, unsigned int cbData, unsigned char *lpData, unsigned int ulObjId)
{
    ECRESULT er = erSuccess;

    ECsIndexObject sObject;
    ECsIndexProp sProp;

    sObject.ulTag = ulTag;
    sObject.ulObjId = ulObjId;
    
    sProp.SetValue(ulTag, lpData, cbData);

    er = _AddIndexData(&sObject, &sProp);

    return er;
}

ECRESULT ECCacheManager::GetEntryIdFromObject(unsigned int ulObjId, struct soap *soap, entryId** lppEntryId)
{
	ECRESULT	er = erSuccess;
	entryId*	lpEntryId = s_alloc<entryId>(soap);

	er = GetEntryIdFromObject(ulObjId, soap, lpEntryId);
	if (er != erSuccess)
		goto exit;

	*lppEntryId = lpEntryId;
exit:
	if (er != erSuccess && lpEntryId)
		delete lpEntryId;

	return er;
}

ECRESULT ECCacheManager::GetEntryIdFromObject(unsigned int ulObjId, struct soap *soap, entryId* lpEntryId)
{
	ECRESULT	er = erSuccess;

	er = GetPropFromObject( PROP_ID(PR_ENTRYID), ulObjId, soap, (unsigned int*)&lpEntryId->__size, &lpEntryId->__ptr);

	return er;
}

ECRESULT ECCacheManager::GetObjectFromEntryId(entryId* lpEntryId, unsigned int* lpulObjId)
{
	ECRESULT	er = erSuccess;

	er = GetObjectFromProp( PROP_ID(PR_ENTRYID), lpEntryId->__size, lpEntryId->__ptr, lpulObjId);

	return er;
}

ECRESULT ECCacheManager::SetObjectEntryId(entryId* lpEntryId, unsigned int ulObjId)
{
    ECRESULT 	er = erSuccess;

    er = SetObjectProp( PROP_ID(PR_ENTRYID), lpEntryId->__size, lpEntryId->__ptr, ulObjId);

    return er;
}

/**
 * Convert entryid to database object id
 */
ECRESULT ECCacheManager::GetEntryListToObjectList(struct entryList *lpEntryList, ECListInt* lplObjectList)
{
	ECRESULT		er = erSuccess;
	unsigned int	ulId = 0;
	bool			bPartialCompletion = false;

	if(lpEntryList == NULL) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	for(unsigned int i=0; i < lpEntryList->__size; i++)
	{
		if(GetObjectFromEntryId(&lpEntryList->__ptr[i], &ulId) != erSuccess) {
			bPartialCompletion = true;
			continue; // Unknown entryid, next item
		}

		lplObjectList->push_back(ulId);
	}

exit:
	if(bPartialCompletion)
		er = ZARAFA_W_PARTIAL_COMPLETION;

	return er;
}

/**
 * Convert database object id to entryid
 */
ECRESULT ECCacheManager::GetEntryListFromObjectList(ECListInt* lplObjectList, struct soap *soap, struct entryList **lppEntryList)
{
	ECRESULT		er = erSuccess;
	bool			bPartialCompletion = false;
	ECListInt::iterator	iterList;
	entryList*		lpEntryList = s_alloc<entryList>(soap);

	if(lplObjectList == NULL || lppEntryList == NULL) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	lpEntryList->__ptr = s_alloc<entryId>(soap, lplObjectList->size());
	lpEntryList->__size = 0;

	for(iterList = lplObjectList->begin(); iterList != lplObjectList->end(); iterList++)
	{
		if(GetEntryIdFromObject(*iterList, soap, &lpEntryList->__ptr[lpEntryList->__size]) != erSuccess) {
			bPartialCompletion = true;
			continue; // Unknown entryid, next item
		}
		lpEntryList->__size++;
	}

	*lppEntryList = lpEntryList;
exit:
	if (er != erSuccess && lpEntryList)
		FreeEntryList(lpEntryList, true);

	if(bPartialCompletion)
		er = ZARAFA_W_PARTIAL_COMPLETION;


	return er;
}

/**
 * Get list of indexed properties for the indexer
 *
 * This is not a read-through, the data must be set via SetIndexedProperties
 *
 * @param[out] map Map of property ID -> text name
 * @return Result
 */ 
ECRESULT ECCacheManager::GetIndexedProperties(std::map<unsigned int, std::string>& map)
{
	scoped_lock lock(m_hIndexedPropertiesMutex);
	
	if(m_mapIndexedProperties.empty()) {
		return ZARAFA_E_NOT_FOUND;
	}
	map = m_mapIndexedProperties;
	
	return erSuccess;
}

/**
 * Set list of indexed properties for the indexer
 *
 * @param[in] map Map of property ID -> text name
 * @return result
 */
ECRESULT ECCacheManager::SetIndexedProperties(std::map<unsigned int, std::string>& map)
{
	scoped_lock lock(m_hIndexedPropertiesMutex);
	
	m_mapIndexedProperties = map;
	
	return erSuccess;
}
        
void ECCacheManager::DisableCellCache()
{
    m_bCellCacheDisabled = true;
}

void ECCacheManager::EnableCellCache()
{
    m_bCellCacheDisabled = false;
}
