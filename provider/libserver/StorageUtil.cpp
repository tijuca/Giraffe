/*
 * Copyright 2005 - 2016 Zarafa and its licensors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License, version 3,
 * as published by the Free Software Foundation.
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

#include <kopano/platform.h>
#include <kopano/memory.hpp>
#include <kopano/tie.hpp>
#include "StorageUtil.h"
#include "ECDatabase.h"
#include "ECAttachmentStorage.h"
#include "ECSession.h"
#include "ECSessionManager.h"
#include "ECSecurity.h"
#include "cmdutil.hpp"
#include <edkmdb.h>

using namespace KCHL;

namespace KC {

// External objects
extern ECSessionManager *g_lpSessionManager;	// ECServerEntrypoint.cpp

ECRESULT CreateObject(ECSession *lpecSession, ECDatabase *lpDatabase, unsigned int ulParentObjId, unsigned int ulParentType, unsigned int ulObjType, unsigned int ulFlags, unsigned int *lpulObjId) 
{
	ECRESULT		er;
	unsigned int	ulNewObjId = 0;
	unsigned int	ulAffected = 0;
	unsigned int	ulOwner = 0;
	std::string		strQuery;

	assert(ulParentType == MAPI_FOLDER || ulParentType == MAPI_MESSAGE || ulParentType == MAPI_ATTACH);
	//
    // We skip quota checking because we do this during writeProps.

	if(ulParentType == MAPI_FOLDER) {
		// Only check creating items in folders. Creating items in messages and attachments is not security-checked since
		// you should check access rights on the top-level message, not on the underlying objects.
		er = lpecSession->GetSecurity()->CheckPermission(ulParentObjId, ecSecurityCreate);
		if(er != erSuccess)
			return er;
	}

	ulOwner = lpecSession->GetSecurity()->GetUserId(ulParentObjId); // Owner of object is either the current user or the owner of the folder

	// Create object
	strQuery = "INSERT INTO hierarchy (parent, type, flags, owner) values("+stringify(ulParentObjId)+", "+stringify(ulObjType)+", "+stringify(ulFlags)+", "+stringify(ulOwner)+")";
	er = lpDatabase->DoInsert(strQuery, &ulNewObjId, &ulAffected);
	if(er != erSuccess)
		return er;

	if (ulObjType == MAPI_MESSAGE) {
		strQuery = "INSERT INTO properties (hierarchyid, tag, type, val_ulong) VALUES ("+ stringify(ulNewObjId) + "," + stringify(PROP_ID(PR_MESSAGE_FLAGS)) + "," + stringify(PROP_TYPE(PR_MESSAGE_FLAGS)) + "," + stringify(ulFlags) + ")";
		er = lpDatabase->DoInsert(strQuery);
		if (er != erSuccess)
			return er;
	}

	// Save this item in the cache, as there is a very high probability that this data will be required very soon (almost 100% sure)
	g_lpSessionManager->GetCacheManager()->SetObject(ulNewObjId, ulParentObjId, ulOwner, ulFlags, ulObjType);

	// return new object id to saveObject
	if (lpulObjId)
		*lpulObjId = ulNewObjId;
	return erSuccess;
}

/* Get the size of an object, PR_MESSAGE_SIZE or PR_ATTACH_SIZE */
ECRESULT GetObjectSize(ECDatabase* lpDatabase, unsigned int ulObjId, unsigned int* lpulSize)
{
	ECRESULT		er = erSuccess;
	DB_RESULT lpDBResult;
	DB_ROW			lpDBRow = NULL;
	unsigned int	ulSize = 0;
	std::string		strQuery;

	strQuery = "SELECT val_ulong FROM properties WHERE hierarchyid="+stringify(ulObjId)+" AND ((tag="+stringify(PROP_ID(PR_MESSAGE_SIZE))+" AND type="+stringify(PROP_TYPE(PR_MESSAGE_SIZE))+") OR (tag="+stringify(PROP_ID(PR_ATTACH_SIZE))+" AND type="+stringify(PROP_TYPE(PR_ATTACH_SIZE))+") )" + " LIMIT 1";
	er = lpDatabase->DoSelect(strQuery, &lpDBResult);
	if(er != erSuccess)
		return er;
	if (lpDBResult.get_num_rows() != 1)
		return KCERR_NOT_FOUND;
	lpDBRow = lpDBResult.fetch_row();
	if (lpDBRow == nullptr || lpDBRow[0] == nullptr)
		return KCERR_NOT_FOUND;
	ulSize = atoi(lpDBRow[0]);
	*lpulSize = ulSize;
	return erSuccess;
}

ECRESULT CalculateObjectSize(ECDatabase* lpDatabase, unsigned int objid, unsigned int ulObjType, unsigned int* lpulSize)
{
	ECRESULT		er = erSuccess;
	DB_RESULT lpDBResult;
	DB_ROW			lpDBRow = NULL;
	unsigned int	ulSize = 0;
	std::string		strQuery;
	ECDatabaseAttachment *lpDatabaseStorage = NULL;

	*lpulSize = 0;
	//	strQuery = "SELECT SUM(16 + IF(val_ulong, 4, 0)+IF(val_double||val_longint||val_hi||val_lo, 8, 0)+ LENGTH(IFNULL(val_string, 0))+length(IFNULL(val_binary, 0))) FROM properties WHERE hierarchyid=" + stringify(objid);

	// SQLite doesn't support IF-type statements, so we're now using a slightly simpler construct ..
	strQuery = "SELECT (SELECT SUM(20 + LENGTH(IFNULL(val_string, 0))+length(IFNULL(val_binary, 0))) FROM properties WHERE hierarchyid=" + stringify(objid) + ") + IFNULL( (SELECT SUM(LENGTH(lob.val_binary)) FROM `lob` JOIN `singleinstances` ON singleinstances.instanceid = lob.instanceid WHERE singleinstances.hierarchyid=" + stringify(objid) + "), 0)";
	er = lpDatabase->DoSelect(strQuery, &lpDBResult);
	if(er != erSuccess)
		return er;
	lpDBRow = lpDBResult.fetch_row();
	if(lpDBRow == NULL || lpDBRow[0] == NULL)
		ulSize = 0;
	else
		ulSize = atoui(lpDBRow[0])+ 28;// + hierarchy size

	std::unique_ptr<ECAttachmentStorage> lpAttachmentStorage(g_lpSessionManager->get_atxconfig()->new_handle(lpDatabase));
	if (lpAttachmentStorage == nullptr)
		return KCERR_NOT_ENOUGH_MEMORY;

	// since we already did the length magic in the previous query, we only need the 
	// extra size for filestorage and S3 storage, i.e. not database storage
	lpDatabaseStorage = dynamic_cast<ECDatabaseAttachment *>(lpAttachmentStorage.get());
	if (!lpDatabaseStorage) {
		size_t ulAttachSize = 0;
		er = lpAttachmentStorage->GetSize(objid, PROP_ID(PR_ATTACH_DATA_BIN), &ulAttachSize);
		if (er != erSuccess)
			return er;
		ulSize += ulAttachSize;
	}

	// Calculate also mv-props
	strQuery = "SELECT SUM(20 + LENGTH(IFNULL(val_string, 0))+length(IFNULL(val_binary, 0))) FROM mvproperties WHERE hierarchyid=" + stringify(objid);
	er = lpDatabase->DoSelect(strQuery, &lpDBResult);
	if(er != erSuccess)
		return er;
	lpDBRow = lpDBResult.fetch_row();
	if(lpDBRow != NULL && lpDBRow[0] != NULL)
		ulSize += atoui(lpDBRow[0]); // Add the size

	// Get parent sizes
	strQuery = "SELECT SUM(IFNULL(p.val_ulong, 0)) FROM hierarchy as h JOIN properties AS p ON hierarchyid=h.id WHERE h.parent=" + stringify(objid)+ " AND ((p.tag="+stringify(PROP_ID(PR_MESSAGE_SIZE))+" AND p.type="+stringify(PROP_TYPE(PR_MESSAGE_SIZE)) + ") || (p.tag="+stringify(PROP_ID(PR_ATTACH_SIZE))+" AND p.type="+stringify(PROP_TYPE(PR_ATTACH_SIZE))+ "))";
	er = lpDatabase->DoSelect(strQuery, &lpDBResult);
	if(er != erSuccess)
		return er;
	lpDBRow = lpDBResult.fetch_row();
	if(lpDBRow != NULL && lpDBRow[0] != NULL)
		ulSize += atoui(lpDBRow[0]); // Add the size
	*lpulSize = ulSize;
	return erSuccess;
}

ECRESULT UpdateObjectSize(ECDatabase* lpDatabase, unsigned int ulObjId, unsigned int ulObjType, eSizeUpdateAction updateAction, long long llSize)
{
	ECRESULT er;
	unsigned int	ulPropTag = 0;
	unsigned int	ulAffRows = 0;
	std::string		strQuery;
	std::string		strField;

	if(ulObjType == MAPI_ATTACH) {
		ulPropTag = PR_ATTACH_SIZE;
		strField = "val_ulong";
	}else if(ulObjType == MAPI_STORE) {
		ulPropTag = PR_MESSAGE_SIZE_EXTENDED;
		strField = "val_longint";
	}else {
		ulPropTag = PR_MESSAGE_SIZE;
		strField = "val_ulong";
	}

	auto gcache = g_lpSessionManager->GetCacheManager();
	if (updateAction == UPDATE_SET) {
		strQuery = "REPLACE INTO properties(hierarchyid, tag, type, "+strField+") VALUES(" + stringify(ulObjId) + "," + stringify(PROP_ID(ulPropTag)) + "," + stringify(PROP_TYPE(ulPropTag)) + "," + stringify_int64(llSize) + ")";
		er = lpDatabase->DoInsert(strQuery);

		if(er != erSuccess)
			return er;

		if(ulObjType == MAPI_MESSAGE) {
			// Update cell cache for new size
			sObjectTableKey key;
			struct propVal sPropVal;
			
			key.ulObjId = ulObjId;
			key.ulOrderId = 0;
			sPropVal.ulPropTag = PR_MESSAGE_SIZE;
			sPropVal.Value.ul = llSize;
			sPropVal.__union = SOAP_UNION_propValData_ul;
			er = gcache->SetCell(&key, PR_MESSAGE_SIZE, &sPropVal);
			if(er != erSuccess)
				return er;
		}
	} else {
		strQuery = "UPDATE properties SET "+strField+"=";

		if(updateAction == UPDATE_ADD)
			strQuery += strField+"+";
		else if(updateAction == UPDATE_SUB)
			strQuery += strField+"-";

		strQuery += stringify_int64(llSize) +" WHERE tag="+stringify(PROP_ID(ulPropTag))+" AND type="+stringify(PROP_TYPE(ulPropTag)) + " AND hierarchyid="+stringify(ulObjId);
		if(updateAction == UPDATE_SUB)
			strQuery += " AND "+strField+" >="+stringify_int64(llSize);

		er = lpDatabase->DoUpdate(strQuery, &ulAffRows);
		
		if(er != erSuccess) 
			return er;
		
		if(ulObjType == MAPI_MESSAGE) {
			// Update cell cache
			er = gcache->UpdateCell(ulObjId, PR_MESSAGE_SIZE, (updateAction == UPDATE_ADD ? llSize : -llSize));
			if(er != erSuccess)
				return er;
		}
	}
	return erSuccess;
}

} /* namespace */
