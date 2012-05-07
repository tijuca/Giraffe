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

#include <mapidefs.h>
#include <errno.h>

#include <algorithm>

#include <zlib.h>

#include <ECSerializer.h>

#include "ECAttachmentStorage.h"
#include "SOAPUtils.h"
#include "mapitags.h"
#include "stringutil.h"
#include "StreamUtil.h"

// chunk size for attachment blobs, must be equal or larger than MAX, MAX may never shrink below 384*1024.
#define CHUNK_SIZE 384*1024

/*
 * Locking requirements of ECAttachmentStorage:
 * In the case of ECAttachmentStorage locking to protect against concurrent access is futile.
 * The theory is as follows:
 * If 2 users have a reference to the same attachment, neither can delete the mail causing
 * the other person to lose the attachment. This means that concurrent copy and delete actions
 * are possible on the same attachment. The only case where this does not hold is the case
 * when a user deletes the mail he is copying at the same time and that this is the last mail
 * which references that specific attachment.
 * The only race condition which might occur is that the dagent delivers a mail with attachment,
 * the server returns the attachment id back to the dagent, but the user for whom the message
 * was intended deletes the mail & attachment. In this case the dagent will no longer send the
 * attachment but only the attachment id. When that happens the server can return an error
 * and simply request the dagent to resend the attachment and to obtain a new attachment id.
 */

// Generic Attachment storage
ECAttachmentStorage::ECAttachmentStorage(ECDatabase *lpDatabase, unsigned int ulCompressionLevel)
	: m_lpDatabase(lpDatabase)
{
	m_ulRef = 0;
	m_bFileCompression = ulCompressionLevel != 0;

	if (ulCompressionLevel > Z_BEST_COMPRESSION)
		ulCompressionLevel = Z_BEST_COMPRESSION;

	m_CompressionLevel = stringify(ulCompressionLevel);
}

ECAttachmentStorage::~ECAttachmentStorage()
{
}

ULONG ECAttachmentStorage::AddRef() {
	return ++m_ulRef;
}

ULONG ECAttachmentStorage::Release() {
	ULONG ulRef = --m_ulRef;

	if (m_ulRef == 0)
		delete this;

	return ulRef;
}

/** 
 * Create an attachment storage object which either uses the
 * ECDatabase or a filesystem as storage.
 * 
 * @param[in] lpDatabase Database class that stays valid during the lifetime of the returned ECAttachmentStorage
 * @param[in] lpConfig The server configuration object
 * @param[out] lppAttachmentStorage The attachment storage object
 * 
 * @return Zarafa error code
 * @retval ZARAFA_E_DATABASE_ERROR given database pointer wasn't valid
 */
ECRESULT ECAttachmentStorage::CreateAttachmentStorage(ECDatabase *lpDatabase, ECConfig *lpConfig, ECLogger *lpLogger, ECAttachmentStorage **lppAttachmentStorage)
{
	ECAttachmentStorage *lpAttachmentStorage = NULL;

	if (lpDatabase == NULL)
		return ZARAFA_E_DATABASE_ERROR; // somebody called this function too soon.

	if (strcmp(lpConfig->GetSetting("attachment_storage"), "files") == 0) {
		lpAttachmentStorage = new ECFileAttachment(lpDatabase, lpConfig->GetSetting("attachment_path"), atoi(lpConfig->GetSetting("attachment_compression")), lpLogger);
	} else {
		lpAttachmentStorage = new ECDatabaseAttachment(lpDatabase, lpLogger);
	}

	lpAttachmentStorage->AddRef();

	*lppAttachmentStorage = lpAttachmentStorage;

	return erSuccess;
}

/** 
 * Gets the instance id for a given hierarchy id and prop tag.
 * 
 * @param[in] ulObjId Id from the hierarchy table
 * @param[in] ulTag Proptag of the instance data
 * @param[out] lpulInstanceId Id to use as instanceid
 * 
 * @return Zarafa error code
 */
ECRESULT ECAttachmentStorage::GetSingleInstanceId(ULONG ulObjId, ULONG ulTag, ULONG *lpulInstanceId)
{	
	ECRESULT er = erSuccess;
	std::string strQuery;
	DB_RESULT lpDBResult = NULL;
	DB_ROW lpDBRow = NULL;

	strQuery =
		"SELECT `instanceid` "
		"FROM `singleinstances` "
		"WHERE `hierarchyid` = " + stringify(ulObjId) + " AND `tag` = " + stringify(ulTag);

	er = m_lpDatabase->DoSelect(strQuery, &lpDBResult);
	if (er != erSuccess)
		goto exit;

	lpDBRow = m_lpDatabase->FetchRow(lpDBResult);

	if (!lpDBRow || !lpDBRow[0]) {
		er = ZARAFA_E_NOT_FOUND;
		goto exit;
	}

	if (lpulInstanceId)
		*lpulInstanceId = atoi(lpDBRow[0]);

exit:
	if (lpDBResult)
		m_lpDatabase->FreeResult(lpDBResult);

	return er;
}

/** 
 * Get all instance ids from a list of hierarchy ids, independant of
 * the proptag.
 * @todo this should be for a given tag, or we should return the tags too (map<InstanceID, ulPropId>)
 * 
 * @param[in] lstObjIds list of hierarchy ids
 * @param[out] lstAttachIds list of unique corresponding instance ids
 * 
 * @return Zarafa error code
 */
ECRESULT ECAttachmentStorage::GetSingleInstanceIds(std::list<ULONG> &lstObjIds, std::list<ULONG> *lstAttachIds)
{
	ECRESULT er = erSuccess;
	std::string strQuery;
	DB_RESULT lpDBResult = NULL;
	DB_ROW lpDBRow = NULL;
	std::list<ULONG> lstInstanceIds;

	/* No single instances were requested... */
	if (lstObjIds.empty())
		goto exit;

	strQuery =
		"SELECT DISTINCT `instanceid` "
		"FROM `singleinstances` "
		"WHERE `hierarchyid` IN (";
	for (std::list<ULONG>::iterator i = lstObjIds.begin(); i != lstObjIds.end(); i++) {
		if (i != lstObjIds.begin())
			strQuery += ",";
		strQuery += stringify(*i);
	}

	strQuery += ")";

	er = m_lpDatabase->DoSelect(strQuery, &lpDBResult);
	if (er != erSuccess)
		goto exit;

	while ((lpDBRow = m_lpDatabase->FetchRow(lpDBResult)) != NULL) {
		if (lpDBRow[0] == NULL) {
			er = ZARAFA_E_DATABASE_ERROR;
			goto exit;
		}

		lstInstanceIds.push_back(atoi(lpDBRow[0]));
	}

	lstAttachIds->swap(lstInstanceIds);

exit:
	if (lpDBResult)
		m_lpDatabase->FreeResult(lpDBResult);

	return er;
}

/** 
 * Sets or replaces a row in the singleinstances table for a given
 * hierarchyid, tag and instanceid.
 * 
 * @param[in] ulObjId HierarchyID to set/add instance id for
 * @param[in] ulInstanceId InstanceID to set for HierarchyID + Tag
 * @param[in] ulTag PropID to set/add instance id for
 * 
 * @return Zarafa error code
 */
ECRESULT ECAttachmentStorage::SetSingleInstanceId(ULONG ulObjId, ULONG ulInstanceId, ULONG ulTag)
{
	ECRESULT er = erSuccess;
	std::string strQuery;

	/*
	 * Check if attachment reference exists, if not return error
	 */
	if (!ExistAttachmentInstance(ulInstanceId)) {
		er = ZARAFA_E_UNABLE_TO_COMPLETE;
		goto exit;
	}

	/*
	 * Create Attachment reference, use provided attachment id
	 */
	strQuery =
		"REPLACE INTO `singleinstances` (`instanceid`, `hierarchyid`, `tag`) VALUES"
		"(" + stringify(ulInstanceId) + ", " + stringify(ulObjId) + ", " +  stringify(ulTag) + ")";

	er = m_lpDatabase->DoInsert(strQuery, (unsigned int*)&ulInstanceId);
	if (er != erSuccess)
		goto exit;

exit:
	return er;
}

/** 
 * Get all HierarchyIDs for a given InstanceID.
 * 
 * @param[in] ulInstanceId InstanceID to get HierarchyIDs for
 * @param[out] lplstObjIds List of all HierarchyIDs which link to the single instance
 * 
 * @return Zarafa error code
 */
ECRESULT ECAttachmentStorage::GetSingleInstanceParents(ULONG ulInstanceId, std::list<ULONG> *lplstObjIds)
{
	ECRESULT er = erSuccess;
	std::string strQuery;
	DB_RESULT lpDBResult = NULL;
	DB_ROW lpDBRow = NULL;
	std::list<ULONG> lstObjIds;

	strQuery =
		"SELECT DISTINCT `hierarchyid` "
		"FROM `singleinstances` "
		"WHERE `instanceid` = " + stringify(ulInstanceId);
	er = m_lpDatabase->DoSelect(strQuery, &lpDBResult);
	if (er != erSuccess)
		goto exit;

	while ((lpDBRow = m_lpDatabase->FetchRow(lpDBResult)) != NULL) {
		if (lpDBRow[0] == NULL) {
			er = ZARAFA_E_DATABASE_ERROR;
			goto exit;
		}

		lstObjIds.push_back(atoi(lpDBRow[0]));
	}

	lplstObjIds->swap(lstObjIds);

exit:
	if (lpDBResult)
		m_lpDatabase->FreeResult(lpDBResult);

	return er;
}

/** 
 * Checks if there are no references to a given InstanceID anymore.
 * 
 * @param ulInstanceId InstanceID to check
 * @param bOrphan true if instance isn't referenced anymore
 * 
 * @return Zarafa error code
 */
ECRESULT ECAttachmentStorage::IsOrphanedSingleInstance(ULONG ulInstanceId, bool *bOrphan)
{
	ECRESULT er = erSuccess;
	std::string strQuery;
	DB_RESULT lpDBResult = NULL;
	DB_ROW lpDBRow = NULL;

	strQuery =
		"SELECT `instanceid` "
		"FROM `singleinstances` "
		"WHERE `instanceid` = " + stringify(ulInstanceId) + " "
		"LIMIT 1";

	er = m_lpDatabase->DoSelect(strQuery, &lpDBResult);
	if (er != erSuccess)
		goto exit;

	lpDBRow = m_lpDatabase->FetchRow(lpDBResult);

	/*
	 * No results: Single Instance ID has been cleared (refcount = 0)
	 */
	*bOrphan = (!lpDBRow || !lpDBRow[0]);

exit:
	if (lpDBResult)
		m_lpDatabase->FreeResult(lpDBResult);

	return er;
}

/** 
 * Make a list of all orphaned instances for a list of given InstanceIDs.
 * 
 * @param[in] lstAttachments List of instance ids to check
 * @param[out] lplstOrphanedAttachments List of orphaned instance ids
 * 
 * @return 
 */
ECRESULT ECAttachmentStorage::GetOrphanedSingleInstances(std::list<ULONG> &lstInstanceIds, std::list<ULONG> *lplstOrphanedInstanceIds)
{
	ECRESULT er = erSuccess;
	std::string strQuery;
	DB_RESULT lpDBResult = NULL;
	DB_ROW lpDBRow = NULL;

	strQuery =
		"SELECT DISTINCT `instanceid` "
		"FROM `singleinstances` "
		"WHERE `instanceid` IN ( ";
	for (std::list<ULONG>::iterator i = lstInstanceIds.begin(); i != lstInstanceIds.end(); i++) {
		if (i != lstInstanceIds.begin())
			strQuery += ",";
		strQuery += stringify(*i);
	}
	strQuery +=	")";

	er = m_lpDatabase->DoSelect(strQuery, &lpDBResult);
	if (er != erSuccess)
		goto exit;

	/* First make a full copy of the list of Single Instance IDs */
	lplstOrphanedInstanceIds->assign(lstInstanceIds.begin(), lstInstanceIds.end());

	/*
	 * Now filter out any Single Instance IDs which were returned in the query,
	 * any results not returned by the query imply that the refcount is 0
	 */
	while ((lpDBRow = m_lpDatabase->FetchRow(lpDBResult)) != NULL) {
		if (lpDBRow[0] == NULL) {
			er = ZARAFA_E_DATABASE_ERROR;
			goto exit;
		}

		lplstOrphanedInstanceIds->remove(atoi(lpDBRow[0]));
	}

exit:
	if (lpDBResult)
		m_lpDatabase->FreeResult(lpDBResult);

	return er;
}

/** 
 * For a given hierarchy id, check if this has a valid instance id
 * 
 * @param[in] ulObjId hierarchy id to check instance for
 * @param[in] ulPropId property id to check instance for
 * 
 * @return instance present
 */
bool ECAttachmentStorage::ExistAttachment(ULONG ulObjId, ULONG ulPropId)
{
	ECRESULT er = erSuccess;
	ULONG ulInstanceId = 0;

	/*
	 * Convert object id into attachment id
	 */
	er = GetSingleInstanceId(ulObjId, ulPropId, &ulInstanceId);
	if (er != erSuccess)
		return false;

	return ExistAttachmentInstance(ulInstanceId);
}

/** 
 * Retrieve a large property from the storage, return data as blob.
 * 
 * @param[in] soap Use soap for allocations. Returned data can directly be used to return to the client.
 * @param[in] ulObjId HierarchyID to load property for
 * @param[in] ulPropId property id to load
 * @param[out] lpiSize size of the property
 * @param[out] lppData data of the property
 * 
 * @return Zarafa error code
 */
ECRESULT ECAttachmentStorage::LoadAttachment(struct soap *soap, ULONG ulObjId, ULONG ulPropId, int *lpiSize, unsigned char **lppData)
{
	ECRESULT er = erSuccess;
	ULONG ulInstanceId = 0;

	/*
	 * Convert object id into attachment id
	 */
	er = GetSingleInstanceId(ulObjId, ulPropId, &ulInstanceId);
	if (er != erSuccess)
		goto exit;

	er = LoadAttachmentInstance(soap, ulInstanceId, lpiSize, lppData);
	if (er != erSuccess)
		goto exit;

exit:
	return er;
}

/** 
 * Retrieve a large property from the storage, return data in a serializer.
 * 
 * @param[in] ulObjId HierarchyID to load property for
 * @param[in] ulPropId property id to load
 * @param[out] lpiSize size of the property
 * @param[out] lpSink Write in this serializer
 * 
 * @return Zarafa error code
 */
ECRESULT ECAttachmentStorage::LoadAttachment(ULONG ulObjId, ULONG ulPropId, int *lpiSize, ECSerializer *lpSink)
{
	ECRESULT er = erSuccess;
	ULONG ulInstanceId = 0;

	/*
	 * Convert object id into attachment id
	 */
	er = GetSingleInstanceId(ulObjId, ulPropId, &ulInstanceId);
	if (er != erSuccess)
		goto exit;

	er = LoadAttachmentInstance(ulInstanceId, lpiSize, lpSink);
	if (er != erSuccess)
		goto exit;

exit:
	return er;
}

/** 
 * Save a property of a specific object from a given blob, optionally remove previous data.
 * 
 * @param[in] ulObjId HierarchyID of object
 * @param[in] ulPropId PropertyID to save
 * @param[in] bDeleteOld Remove old data before saving the new
 * @param[in] iSize size of lpData
 * @param[in] lpData data of the property
 * @param[out] lpulInstanceId InstanceID for the data (optional)
 * 
 * @return Zarafa error code
 */
ECRESULT ECAttachmentStorage::SaveAttachment(ULONG ulObjId, ULONG ulPropId, bool bDeleteOld, int iSize, unsigned char *lpData, ULONG *lpulInstanceId)
{
	ECRESULT er = erSuccess;
	ULONG ulInstanceId = 0;
	std::string strQuery;

	if (!lpData) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	if (bDeleteOld) {
		/*
		 * Call DeleteAttachment to decrease the refcount
		 * and optionally delete the original attachment.
		 */
		er = DeleteAttachment(ulObjId, ulPropId, true);
		if (er != erSuccess)
			goto exit;
	}

	/*
	 * Create Attachment reference, detect new attachment id.
	 */
	strQuery =
		"INSERT INTO `singleinstances` (`hierarchyid`, `tag`) VALUES"
		"(" + stringify(ulObjId) + ", " + stringify(ulPropId) + ")";
	er = m_lpDatabase->DoInsert(strQuery, (unsigned int*)&ulInstanceId);
	if (er != erSuccess)
		goto exit;

	er = SaveAttachmentInstance(ulInstanceId, ulPropId, iSize, lpData);
	if (er != erSuccess)
		goto exit;

	if (lpulInstanceId)
		*lpulInstanceId = ulInstanceId;

exit:
	return er;
}

/** 
 * Save a property of a specific object from a serializer, optionally remove previous data.
 * 
 * @param[in] ulObjId HierarchyID of object
 * @param[in] ulPropId Property to save
 * @param[in] bDeleteOld Remove old data before saving the new
 * @param[in] iSize size in lpSource
 * @param[in] lpSource serializer to read data from
 * @param[out] lpulInstanceId InstanceID for the data (optional)
 * 
 * @return Zarafa error code
 */
ECRESULT ECAttachmentStorage::SaveAttachment(ULONG ulObjId, ULONG ulPropId, bool bDeleteOld, int iSize, ECSerializer *lpSource, ULONG *lpulInstanceId)
{
	ECRESULT er = erSuccess;
	ULONG ulInstanceId = 0;
	std::string strQuery;

	if (bDeleteOld) {
		/*
		 * Call DeleteAttachment to decrease the refcount
		 * and optionally delete the original attachment.
		 */
		er = DeleteAttachment(ulObjId, ulPropId, true);
		if (er != erSuccess)
			goto exit;
	}

	/*
	 * Create Attachment reference, detect new attachment id.
	 */
	strQuery =
		"INSERT INTO `singleinstances` (`hierarchyid`, `tag`) VALUES"
		"(" + stringify(ulObjId) + ", " + stringify(ulPropId) + ")";
	er = m_lpDatabase->DoInsert(strQuery, (unsigned int*)&ulInstanceId);
	if (er != erSuccess)
		goto exit;

	er = SaveAttachmentInstance(ulInstanceId, ulPropId, iSize, lpSource);
	if (er != erSuccess)
		goto exit;

	if (lpulInstanceId)
		*lpulInstanceId = ulInstanceId;

exit:
	return er;
}

/** 
 * Save a property of an object with a given instance id, optionally remove previous data.
 * 
 * @param[in] ulObjId HierarchyID of object
 * @param[in] ulPropId Property of object
 * @param[in] bDeleteOld Remove old data before saving the new
 * @param[in] ulInstanceId Instance id to link
 * @param[out] lpulInstanceId Same number as in ulInstanceId
 * 
 * @return 
 */
ECRESULT ECAttachmentStorage::SaveAttachment(ULONG ulObjId, ULONG ulPropId, bool bDeleteOld, ULONG ulInstanceId, ULONG *lpulInstanceId)
{
	ECRESULT er = erSuccess;
	ULONG ulOldAttachId = 0;

	if (bDeleteOld) {
		/*
		 * Call DeleteAttachment to decrease the refcount
		 * and optionally delete the original attachment.
		 */
		if(GetSingleInstanceId(ulObjId, ulPropId, &ulOldAttachId) == erSuccess && ulOldAttachId == ulInstanceId) {
			// Nothing to do, we already have that instance ID
			goto exit;
		}
		 
		er = DeleteAttachment(ulObjId, ulPropId, true);
		if (er != erSuccess)
			goto exit;
	}

	er = SetSingleInstanceId(ulObjId, ulInstanceId, ulPropId);
	if (er != erSuccess)
		goto exit;

	/* InstanceId is equal to provided AttachId */
	*lpulInstanceId = ulInstanceId;

exit:
	return er;
}

/** 
 * Make a copy of attachment data for a given object.
 *
 * In reality, the data is not copied, but an extra singleinstance
 * entry is added for the new hierarchyid.
 * 
 * @param[in] ulObjId Source hierarchy id to instance data from
 * @param[in] ulNewObjId Additional hierarchy id which has the same data
 * 
 * @return Zarafa error code
 */
ECRESULT ECAttachmentStorage::CopyAttachment(ULONG ulObjId, ULONG ulNewObjId)
{
	ECRESULT er = erSuccess;
	std::string strQuery;

	/*
	 * Only update the reference count in the `singleinstances` table,
	 * no need to really physically store the attachment twice.
	 */
	strQuery =
		"INSERT INTO `singleinstances` (`hierarchyid`, `instanceid`, `tag`) "
			"SELECT " + stringify(ulNewObjId) + ", `instanceid`, `tag` "
			"FROM `singleinstances` "
			"WHERE `hierarchyid` = " + stringify(ulObjId);

	er = m_lpDatabase->DoInsert(strQuery);
	if (er != erSuccess)
		goto exit;

exit:
	return er;
}

/** 
 * Delete all properties of given list of hierarchy ids.
 * 
 * @param[in] lstDeleteObjects list of hierarchy ids to delete singleinstance data for
 * 
 * @return Zarafa error code
 */
ECRESULT ECAttachmentStorage::DeleteAttachments(std::list<ULONG> lstDeleteObjects)
{
	ECRESULT er = erSuccess;
	std::list<ULONG> lstAttachments;
	std::list<ULONG> lstDeleteAttach;
	std::string strQuery;

	/*
	 * Convert object ids into attachment ids
	 * NOTE: lstDeleteObjects.size() >= lstAttachments.size()
	 * because the list does not 100% consists of attachments id and the
	 * duplicate attachment ids will be filtered out.
	 */
	er = GetSingleInstanceIds(lstDeleteObjects, &lstAttachments);
	if (er != erSuccess)
		goto exit;

	/* No attachments present, we're done */
	if (lstAttachments.empty())
		goto exit;

	/*
	 * Remove all objects from `singleinstances` table this will decrease the
	 * reference count for each attachment.
	 */
	strQuery =
		"DELETE FROM `singleinstances` "
		"WHERE `hierarchyid` IN (";
	for (std::list<ULONG>::iterator i = lstDeleteObjects.begin(); i != lstDeleteObjects.end(); i++) {
		if (i != lstDeleteObjects.begin())
			strQuery += ",";
		strQuery += stringify(*i);
	}
	strQuery += ")";

	er = m_lpDatabase->DoDelete(strQuery);
	if (er != erSuccess)
		goto exit;

	/*
	 * Get the list of orphaned Single Instance IDs which we can delete.
	 */
	er = GetOrphanedSingleInstances(lstAttachments, &lstDeleteAttach);
	if (er != erSuccess)
		goto exit;

	if (!lstDeleteAttach.empty()) {
		er = DeleteAttachmentInstances(lstDeleteAttach, false);
		if (er != erSuccess)
			goto exit;
	}

exit:
	return er;
}

/** 
 * Delete one single instance property of an object.
 * public interface version
 * 
 * @param[in] ulObjId HierarchyID of object to delete single instance property from
 * @param[in] ulPropId Property of object to remove
 * 
 * @return 
 */
ECRESULT ECAttachmentStorage::DeleteAttachment(ULONG ulObjId, ULONG ulPropId) {
	return DeleteAttachment(ulObjId, ulPropId, false);
}

/** 
 * Delete one single instance property of an object.
 * 
 * @param[in] ulObjId HierarchyID of object to delete single instance property from
 * @param[in] ulPropId Property of object to remove
 * @param[in] bReplace Flag used for transations in ECFileStorage
 * 
 * @return Zarafa error code
 */
ECRESULT ECAttachmentStorage::DeleteAttachment(ULONG ulObjId, ULONG ulPropId, bool bReplace)
{
	ECRESULT er = erSuccess;
	ULONG ulInstanceId = 0;
	bool bOrphan = false;
	std::string strQuery;

	/*
	 * Convert object id into attachment id
	 */
	er = GetSingleInstanceId(ulObjId, ulPropId, &ulInstanceId);
	if (er != erSuccess) {
		if (er == ZARAFA_E_NOT_FOUND)
			er = erSuccess;	// Nothing to delete
		goto exit;
	}

	/*
	 * Remove object from `singleinstances` table, this will decrease the
	 * reference count for the attachment.
	 */
	strQuery =
		"DELETE FROM `singleinstances` "
		"WHERE `hierarchyid` = " + stringify(ulObjId) + " "
		"AND `tag` = " + stringify(ulPropId);

	er = m_lpDatabase->DoDelete(strQuery);
	if (er != erSuccess)
		goto exit;

	/*
	 * Check if the attachment can be permanently deleted.
	 */
	if ((IsOrphanedSingleInstance(ulInstanceId, &bOrphan) == erSuccess) && (bOrphan == true)) {
		er = DeleteAttachmentInstance(ulInstanceId, bReplace);
		if (er != erSuccess)
			goto exit;
	}

exit:
	return er;
}

/** 
 * Get the size of a large property of a specific object
 * 
 * @param[in] ulObjId HierarchyID of object
 * @param[in] ulPropId PropertyID of object
 * @param[out] lpulSize size of property
 * 
 * @return Zarafa error code
 */
ECRESULT ECAttachmentStorage::GetSize(ULONG ulObjId, ULONG ulPropId, ULONG *lpulSize)
{
	ECRESULT er = erSuccess;
	ULONG ulInstanceId = 0;

	/*
	 * Convert object id into attachment id
	 */
	er = GetSingleInstanceId(ulObjId, ulPropId, &ulInstanceId);
	if (er == ZARAFA_E_NOT_FOUND) {
		*lpulSize = 0;
		er = erSuccess;
		goto exit;
	} else if (er != erSuccess) {
		goto exit;
	}

	er = GetSizeInstance(ulInstanceId, lpulSize);
	if (er != erSuccess)
		goto exit;

exit:
	return er;
}

// Attachment storage is in database
ECDatabaseAttachment::ECDatabaseAttachment(ECDatabase *lpDatabase, ECLogger *lpLogger)
	: ECAttachmentStorage(lpDatabase, 0),  m_lpLogger(lpLogger)
{
}

ECDatabaseAttachment::~ECDatabaseAttachment()
{
}

/** 
 * For a given instance id, check if this has a valid attachment data present
 * 
 * @param[in] ulInstanceId instance id to check validity
 * 
 * @return instance present
 */
bool ECDatabaseAttachment::ExistAttachmentInstance(ULONG ulInstanceId)
{
	ECRESULT er = erSuccess;
	std::string strQuery;
	DB_RESULT lpDBResult = NULL;
	DB_ROW lpDBRow = NULL;

	strQuery = "SELECT instanceid FROM lob WHERE instanceid = " + stringify(ulInstanceId) + " LIMIT 1";

	er = m_lpDatabase->DoSelect(strQuery, &lpDBResult);
	if (er != erSuccess)
		goto exit;

	lpDBRow = m_lpDatabase->FetchRow(lpDBResult);

	if (!lpDBRow || !lpDBRow[0])
		er = ZARAFA_E_NOT_FOUND;

exit:
	if (lpDBResult)
		m_lpDatabase->FreeResult(lpDBResult);

	return er == erSuccess;
}

/** 
 * Load instance data using soap and return as blob.
 * 
 * @param[in] soap soap to use memory allocations for
 * @param[in] ulInstanceId InstanceID to load
 * @param[out] lpiSize size in lppData
 * @param[out] lppData data of instance
 * 
 * @return Zarafa error code
 */
ECRESULT ECDatabaseAttachment::LoadAttachmentInstance(struct soap *soap, ULONG ulInstanceId, int *lpiSize, unsigned char **lppData)
{
	ECRESULT er = erSuccess;
	int iSize = 0;
	int iReadSize = 0;
	unsigned char *lpData = NULL;
	std::string strQuery;
	DB_RESULT lpDBResult = NULL;
	DB_ROW lpDBRow = NULL;
	DB_LENGTHS lpDBLen = NULL;


	// we first need to know the complete size of the attachment (some old databases don't have the correct chunk size)
	strQuery = "SELECT SUM(LENGTH(val_binary)) FROM lob WHERE instanceid = " + stringify(ulInstanceId);
	er = m_lpDatabase->DoSelect(strQuery, &lpDBResult);
	if (er != erSuccess)
		goto exit;

	lpDBRow = m_lpDatabase->FetchRow(lpDBResult);
	if (lpDBRow == NULL || lpDBRow[0] == NULL) {
		er = ZARAFA_E_DATABASE_ERROR;
		goto exit;
	}

	iSize = atoi(lpDBRow[0]);

	m_lpDatabase->FreeResult(lpDBResult);
	lpDBResult = NULL;

	// get all chunks
	strQuery = "SELECT val_binary FROM lob WHERE instanceid = " + stringify(ulInstanceId) + " ORDER BY chunkid";

	er = m_lpDatabase->DoSelect(strQuery, &lpDBResult);
	if (er != erSuccess)
		goto exit;

	lpData = s_alloc<unsigned char>(soap, iSize);

	while ((lpDBRow = m_lpDatabase->FetchRow(lpDBResult))) {
		if (lpDBRow[0] == NULL) {
			// broken attachment !
			er = ZARAFA_E_DATABASE_ERROR;
			goto exit;
		}

		lpDBLen = m_lpDatabase->FetchRowLengths(lpDBResult);

		memcpy(lpData + iReadSize, lpDBRow[0], lpDBLen[0]);
		iReadSize += lpDBLen[0];
	}

	*lpiSize = iReadSize;
	*lppData = lpData;

exit:
	if (er != erSuccess && lpData && !soap)
		delete lpData;

	if (lpDBResult)
		m_lpDatabase->FreeResult(lpDBResult);

	return er;
}

/** 
 * Load instance data using a serializer.
 * 
 * @param[in] ulInstanceId InstanceID to load
 * @param[out] lpiSize size written in in lpSink
 * @param[in] lpSink serializer to write in
 * 
 * @return 
 */
ECRESULT ECDatabaseAttachment::LoadAttachmentInstance(ULONG ulInstanceId, int *lpiSize, ECSerializer *lpSink)
{
	ECRESULT er = erSuccess;
	int iReadSize = 0;
	std::string strQuery;
	DB_RESULT lpDBResult = NULL;
	DB_ROW lpDBRow = NULL;
	DB_LENGTHS lpDBLen = NULL;

	// get all chunks
	strQuery = "SELECT val_binary FROM lob WHERE instanceid = " + stringify(ulInstanceId) + " ORDER BY chunkid";

	er = m_lpDatabase->DoSelect(strQuery, &lpDBResult);
	if (er != erSuccess)
		goto exit;

	while ((lpDBRow = m_lpDatabase->FetchRow(lpDBResult))) {
		if (lpDBRow[0] == NULL) {
			// broken attachment !
			er = ZARAFA_E_DATABASE_ERROR;
			goto exit;
		}

		lpDBLen = m_lpDatabase->FetchRowLengths(lpDBResult);
		er = lpSink->Write(lpDBRow[0], 1, lpDBLen[0]);
		if (er != erSuccess)
			goto exit;

		iReadSize += lpDBLen[0];
	}

	*lpiSize = iReadSize;

exit:
	if (lpDBResult)
		m_lpDatabase->FreeResult(lpDBResult);

	return er;
}

/** 
 * Save a property in a new instance from a blob
 *
 * @note Property id here is actually useless, but legacy requires
 * this. Removing the `tag` column from the database would require a
 * database update on the lob table, which would make database
 * attachment users very unhappy.
 * 
 * @param[in] ulInstanceId InstanceID to save data under
 * @param[in] ulPropId PropertyID to save
 * @param[in] iSize size of lpData
 * @param[in] lpData Data of property
 * 
 * @return Zarafa error code
 */
ECRESULT ECDatabaseAttachment::SaveAttachmentInstance(ULONG ulInstanceId, ULONG ulPropId, int iSize, unsigned char *lpData)
{
	ECRESULT er = erSuccess;
	std::string strQuery;
	int	iChunkSize;
	int	iSizeLeft;
	int	iPtr;
	unsigned int ulChunk;

	// make chunks of 393216 bytes (384*1024)
	iSizeLeft = iSize;
	iPtr = 0;
	ulChunk = 0;

	while (iSizeLeft > 0) {
		iChunkSize = iSizeLeft < CHUNK_SIZE ? iSizeLeft : CHUNK_SIZE;

		strQuery = (std::string)"INSERT INTO lob (instanceid, chunkid, tag, val_binary) VALUES (" +
			stringify(ulInstanceId) + ", " + stringify(ulChunk) + ", " + stringify(ulPropId) +
			", " + m_lpDatabase->EscapeBinary(lpData + iPtr, iChunkSize) + ")";

		er = m_lpDatabase->DoInsert(strQuery);
		if (er != erSuccess)
			goto exit;

		ulChunk++;
		iSizeLeft -= iChunkSize;
		iPtr += iChunkSize;
	}

exit:
	return er;
}

/** 
 * Save a property in a new instance from a serializer
 *
 * @note Property id here is actually useless, but legacy requires
 * this. Removing the `tag` column from the database would require a
 * database update on the lob table, which would make database
 * attachment users very unhappy.
 * 
 * @param[in] ulInstanceId InstanceID to save data under
 * @param[in] ulPropId PropertyID to save
 * @param[in] iSize size in lpSource
 * @param[in] lpSource serializer to read data from
 * 
 * @return Zarafa error code
 */
ECRESULT ECDatabaseAttachment::SaveAttachmentInstance(ULONG ulInstanceId, ULONG ulPropId, int iSize, ECSerializer *lpSource)
{
	ECRESULT er = erSuccess;
	std::string strQuery;
	int	iChunkSize;
	int	iSizeLeft;
	unsigned int ulChunk;
	unsigned char szBuffer[CHUNK_SIZE] = {0};

	// make chunks of 393216 bytes (384*1024)
	iSizeLeft = iSize;
	ulChunk = 0;

	while (iSizeLeft > 0) {
		iChunkSize = iSizeLeft < CHUNK_SIZE ? iSizeLeft : CHUNK_SIZE;

		er = lpSource->Read(szBuffer, 1, iChunkSize);
		if (er != erSuccess)
			goto exit;

		strQuery = (std::string)"INSERT INTO lob (instanceid, chunkid, tag, val_binary) VALUES (" +
			stringify(ulInstanceId) + ", " + stringify(ulChunk) + ", " + stringify(ulPropId) +
			", " + m_lpDatabase->EscapeBinary(szBuffer, iChunkSize) + ")";

		er = m_lpDatabase->DoInsert(strQuery);
		if (er != erSuccess)
			goto exit;

		ulChunk++;
		iSizeLeft -= iChunkSize;
	}

exit:
	return er;
}

/** 
 * Delete given instances from the database
 * 
 * @param[in] lstDeleteInstances List of instance ids to remove from the database
 * @param[in] bReplace unused, see ECFileAttachment
 * 
 * @return Zarafa error code
 */
ECRESULT ECDatabaseAttachment::DeleteAttachmentInstances(std::list<ULONG> &lstDeleteInstances, bool bReplace)
{
	ECRESULT er = erSuccess;
	std::list<ULONG>::iterator iterDel;
	std::string strQuery;

	strQuery = (std::string)"DELETE FROM lob WHERE instanceid IN (";

	for (iterDel = lstDeleteInstances.begin(); iterDel != lstDeleteInstances.end(); iterDel++) {
		if (iterDel != lstDeleteInstances.begin())
			strQuery += ",";
		strQuery += stringify(*iterDel);
	}

	strQuery += ")";

	er = m_lpDatabase->DoDelete(strQuery);

	return er;
}

/** 
 * Delete a single instanceid from the database
 * 
 * @param[in] ulInstanceId instance id to remove
 * @param[in] bReplace unused, see ECFileAttachment
 * 
 * @return 
 */
ECRESULT ECDatabaseAttachment::DeleteAttachmentInstance(ULONG ulInstanceId, bool bReplace)
{
	ECRESULT er = erSuccess;
	std::string strQuery;

	strQuery = (std::string)"DELETE FROM lob WHERE instanceid=" + stringify(ulInstanceId);

	er = m_lpDatabase->DoDelete(strQuery);

	return er;
}

/** 
 * Return the size of an instance
 * 
 * @param[in] ulInstanceId InstanceID to check the size for
 * @param[out] lpulSize Size of the instance
 * @param[out] lpbCompressed unused, see ECFileAttachment
 * 
 * @return Zarafa error code
 */
ECRESULT ECDatabaseAttachment::GetSizeInstance(ULONG ulInstanceId, ULONG *lpulSize, bool *lpbCompressed)
{
	ECRESULT er = erSuccess; 
	std::string strQuery; 
	DB_RESULT lpDBResult = NULL; 
	DB_ROW lpDBRow = NULL; 
 	 
	strQuery = "SELECT SUM(LENGTH(val_binary)) FROM lob WHERE instanceid = " + stringify(ulInstanceId);
	er = m_lpDatabase->DoSelect(strQuery, &lpDBResult); 
	if (er != erSuccess) 
		goto exit; 

	lpDBRow = m_lpDatabase->FetchRow(lpDBResult); 
	if (lpDBRow == NULL || lpDBRow[0] == NULL) { 
		er = ZARAFA_E_DATABASE_ERROR; 
		goto exit; 
	} 
 	 
	*lpulSize = atoui(lpDBRow[0]); 
	if (lpbCompressed)
		*lpbCompressed = false;
 	 
exit: 
	if (lpDBResult) 
		m_lpDatabase->FreeResult(lpDBResult); 

	return er; 
}

ECRESULT ECDatabaseAttachment::Begin()
{
	return erSuccess;
}

ECRESULT ECDatabaseAttachment::Commit()
{
	return erSuccess;
}

ECRESULT ECDatabaseAttachment::Rollback()
{
	return erSuccess;
}


// Attachment storage is in separate files
ECFileAttachment::ECFileAttachment(ECDatabase *lpDatabase, std::string basepath, unsigned int ulCompressionLevel, ECLogger *lpLogger)
	: ECAttachmentStorage(lpDatabase, ulCompressionLevel)
{
	m_basepath = basepath;
	m_lpLogger = lpLogger;
	
	if (m_basepath.empty())
		m_basepath = "/var/lib/zarafa";
	
	m_bTransaction = false;
}

ECFileAttachment::~ECFileAttachment()
{
	if(m_bTransaction) {
		ASSERT(FALSE);
	}
}

/** 
 * For a given instance id, check if this has a valid attachment data present
 * 
 * @param[in] ulInstanceId instance id to check validity
 * 
 * @return instance present
 */
bool ECFileAttachment::ExistAttachmentInstance(ULONG ulInstanceId)
{
	ECRESULT er = erSuccess;
	string filename = CreateAttachmentFilename(ulInstanceId, m_bFileCompression);
	FILE *f = NULL;

	f = fopen(filename.c_str(), "r");
	if (!f) {
		filename = CreateAttachmentFilename(ulInstanceId, !m_bFileCompression);
		f = fopen(filename.c_str(), "r");
	}
	if (!f) {
		er = ZARAFA_E_NOT_FOUND;
		goto exit;
	}

	fclose(f);

exit:
	return er == erSuccess;
}

/** 
 * Load instance data using soap and return as blob.
 * 
 * @param[in] soap soap to use memory allocations for
 * @param[in] ulInstanceId InstanceID to load
 * @param[out] lpiSize size in lppData
 * @param[out] lppData data of instance
 * 
 * @return Zarafa error code
 */
ECRESULT ECFileAttachment::LoadAttachmentInstance(struct soap *soap, ULONG ulInstanceId, int *lpiSize, unsigned char **lppData)
{
	ECRESULT er = erSuccess;
	string filename;
	FILE *fp = NULL;
	gzFile gzfp = NULL;
	unsigned char *lpData = NULL;
	ULONG ulSize = 0;
	LONG lReadSize = 0;
	bool bCompressed;

	/*
	 * First we need to determine the (uncompressed) filesize
	 */
	er = GetSizeInstance(ulInstanceId, &ulSize, &bCompressed);
	if (er != erSuccess)
		goto exit;

	filename = CreateAttachmentFilename(ulInstanceId, bCompressed);
	lpData = s_alloc<unsigned char>(soap, ulSize);
	if (lpData == NULL) {
		er = ZARAFA_E_NOT_ENOUGH_MEMORY;
		goto exit;
	}

	/*
	 * CreateAttachmentFilename Already checked if we are working with an compressed or uncompressed file,
	 * no need to perform retries when our first guess (which is based on CreateAttachmentFilename) fails.
	 */
	if (bCompressed) {
		/* Compressed attachment */
		gzfp = gzopen(filename.c_str(), "rb");
		if (!gzfp) {
			er = ZARAFA_E_NOT_FOUND;
			goto exit;
		}

		lReadSize = gzread(gzfp, lpData, ulSize);
		if (lReadSize < 0) {
		    m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Error while reading attachment data from %s: %s", filename.c_str(), strerror(errno));
			er = ZARAFA_E_DATABASE_ERROR;
			goto exit;
		}
		if (ulSize != (ULONG)lReadSize) {
			er = ZARAFA_E_DATABASE_ERROR;
			goto exit;
		}
	} else {
		/* Uncompressed attachment */
		fp = fopen(filename.c_str(), "r");
		if (!fp) {
			er = ZARAFA_E_NOT_FOUND;
			goto exit;
		}

		lReadSize = (LONG)fread(lpData, 1, ulSize, fp);
		if (ulSize != (ULONG)lReadSize) {
		    m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Short read while reading attachment data from %s", filename.c_str());
			er = ZARAFA_E_DATABASE_ERROR;
			goto exit;
		}
	}

	*lpiSize = ulSize;
	*lppData = lpData;

exit:
	if (fp)
		fclose(fp);
	if (gzfp)
		gzclose(gzfp);

	return er;
}

/** 
 * Load instance data using a serializer.
 * 
 * @param[in] ulInstanceId InstanceID to load
 * @param[out] lpiSize size written in in lpSink
 * @param[in] lpSink serializer to write in
 * 
 * @return 
 */
ECRESULT ECFileAttachment::LoadAttachmentInstance(ULONG ulInstanceId, int *lpiSize, ECSerializer *lpSink)
{
	ECRESULT er = erSuccess;
	string filename;
	FILE *fp = NULL;
	gzFile gzfp = NULL;
	ULONG ulReadSize = 0;
	LONG lReadNow = 0;
	ULONG ulSize = 0;
	bool bCompressed;
	char buffer[0x1000] = {0};

	/*
	 * First we need to determine the (uncompressed) filesize
	 */
	er = GetSizeInstance(ulInstanceId, &ulSize, &bCompressed);
	if (er != erSuccess)
		goto exit;

	filename = CreateAttachmentFilename(ulInstanceId, bCompressed);

	if (bCompressed) {
		/* Compressed attachment */
		gzfp = gzopen(filename.c_str(), "rb");
		if (!gzfp) {
			er = ZARAFA_E_NOT_FOUND;
			goto exit;
		}
		while (ulReadSize < ulSize) {
			lReadNow = gzread(gzfp, buffer, sizeof(buffer));
			if (lReadNow < 0) {
			    m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Error while reading attachment data from %s: %s", filename.c_str(), strerror(errno));
				er = ZARAFA_E_DATABASE_ERROR;
				goto exit;
			}
			if (lReadNow > 0) {
				lpSink->Write(buffer, 1, lReadNow);
				ulReadSize += lReadNow;
			}
			if ((ULONG)lReadNow < sizeof(buffer))
				break;
		}
		if (ulSize != ulReadSize) {
			er = ZARAFA_E_DATABASE_ERROR;
			goto exit;
		}
	} else {
		fp = fopen(filename.c_str(), "r");
		if (!fp) {
			er = ZARAFA_E_DATABASE_ERROR;
			goto exit;
		}
		while (ulReadSize < ulSize) {
			lReadNow = (ULONG)fread(buffer, 1, sizeof(buffer), fp);
			if (lReadNow == 0 && ferror(fp)) {
    			m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Error while reading attachment data from %s: %s", filename.c_str(), strerror(errno));
    			er = ZARAFA_E_DATABASE_ERROR;
    			goto exit;
            }
			if (lReadNow > 0) {
				lpSink->Write(buffer, 1, lReadNow);
				ulReadSize += lReadNow;
			}
			if ((ULONG)lReadNow < sizeof(buffer))
				break;
		}
		if (ulSize != ulReadSize) {
            m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Short read while reading attachment data from %s", filename.c_str());
			er = ZARAFA_E_DATABASE_ERROR;
			goto exit;
		}
	}

	*lpiSize = ulSize;

exit:
	if (fp)
		fclose(fp);
	if (gzfp)
		gzclose(gzfp);

	return er;
}

/** 
 * Save a property in a new instance from a blob
 *
 * @param[in] ulInstanceId InstanceID to save data under
 * @param[in] ulPropId unused, required by interface, see ECDatabaseAttachment
 * @param[in] iSize size of lpData
 * @param[in] lpData Data of property
 * 
 * @return Zarafa error code
 */
ECRESULT ECFileAttachment::SaveAttachmentInstance(ULONG ulInstanceId, ULONG ulPropId, int iSize, unsigned char *lpData)
{
	ECRESULT er = erSuccess;
	string filename = CreateAttachmentFilename(ulInstanceId, m_bFileCompression && iSize);
	int iWritten;

	// no need to remove the file, just overwrite it
	if (m_bFileCompression && iSize) {
		gzFile gzfp = gzopen(filename.c_str(), std::string("wb" + m_CompressionLevel).c_str());
		if (!gzfp) {
			er = ZARAFA_E_DATABASE_ERROR;
			goto exit;
		}
		iWritten = gzwrite(gzfp, lpData, iSize);
		gzclose(gzfp);
	} else {
		FILE *f = fopen(filename.c_str(), "w");
		if (!f) {
			er = ZARAFA_E_DATABASE_ERROR;
			goto exit;
		}
		iWritten = fwrite(lpData, 1, iSize, f);
		fclose(f);
	}

	// set in transaction before disk full check to remove empty file
	if(m_bTransaction)
		m_setNewAttachment.insert(ulInstanceId);

	if (iWritten != iSize) {
		m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to write attachment, disk full");
		er = ZARAFA_E_DATABASE_ERROR;
		goto exit;
	}

exit:
	return er;
}

/** 
 * Save a property in a new instance from a serializer
 * 
 * @param[in] ulInstanceId InstanceID to save data under
 * @param[in] ulPropId unused, required by interface, see ECDatabaseAttachment
 * @param[in] iSize size in lpSource
 * @param[in] lpSource serializer to read data from
 * 
 * @return Zarafa error code
 */
ECRESULT ECFileAttachment::SaveAttachmentInstance(ULONG ulInstanceId, ULONG ulPropId, int iSize, ECSerializer *lpSource)
{
	ECRESULT er = erSuccess;
	string filename = CreateAttachmentFilename(ulInstanceId, m_bFileCompression);
	unsigned char szBuffer[CHUNK_SIZE];
	int iSizeLeft;
	int iChunkSize;
	int iWritten;

	iSizeLeft = iSize;

	//no need to remove the file, just overwrite it
	if (m_bFileCompression) {
		gzFile gzfp = gzopen(filename.c_str(), std::string("wb" + m_CompressionLevel).c_str());
		if (!gzfp) {
			er = ZARAFA_E_DATABASE_ERROR;
			goto exit;
		}

		// file created on disk, now in transaction
		if (m_bTransaction)
			m_setNewAttachment.insert(ulInstanceId);

		while (iSizeLeft > 0) {
			iChunkSize = iSizeLeft < CHUNK_SIZE ? iSizeLeft : CHUNK_SIZE;

			er = lpSource->Read(szBuffer, 1, iChunkSize);
			if (er != erSuccess) {
				gzclose(gzfp);
				goto exit;
			}

			iWritten = gzwrite(gzfp, szBuffer, iChunkSize);
			if (iWritten == 0)
				break;
			iSizeLeft -= iChunkSize;
		}
		gzclose(gzfp);
	} else {
		FILE *f = fopen(filename.c_str(), "w");
		if (!f) {
			er = ZARAFA_E_DATABASE_ERROR;
			goto exit;
		}

		// file created on disk, now in transaction
		if (m_bTransaction)
			m_setNewAttachment.insert(ulInstanceId);

		while (iSizeLeft > 0) {
			iChunkSize = iSizeLeft < CHUNK_SIZE ? iSizeLeft : CHUNK_SIZE;

			er = lpSource->Read(szBuffer, 1, iChunkSize);
			if (er != erSuccess) {
				fclose(f);
				goto exit;
			}

			iWritten = fwrite(szBuffer, 1, iChunkSize, f);
			if (iWritten == 0)
				break;
			iSizeLeft -= iChunkSize;
		}
		fclose(f);
		
	}
	if (iSizeLeft) {
		m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to write streaming attachment, disk full");
		er = ZARAFA_E_DATABASE_ERROR;
		goto exit;
	}

exit:
	return er;
}

/** 
 * Delete given instances from the filesystem
 * 
 * @param[in] lstDeleteInstances List of instance ids to remove from the filesystem
 * @param[in] bReplace Transaction marker
 * 
 * @return Zarafa error code
 */
ECRESULT ECFileAttachment::DeleteAttachmentInstances(std::list<ULONG> &lstDeleteInstances, bool bReplace)
{
	ECRESULT er = erSuccess;
	int errors = 0;
	std::list<ULONG>::iterator iterDel;

	for (iterDel = lstDeleteInstances.begin(); iterDel != lstDeleteInstances.end(); iterDel++) {
		er = this->DeleteAttachmentInstance(*iterDel, bReplace);
		if (er != erSuccess)
			errors++;
	}

	return errors == 0 ? erSuccess : ZARAFA_E_DATABASE_ERROR;
}

/** 
 * Mark a file deleted by renaming it
 * 
 * @param[in] ulInstanceId instance id to mark
 * 
 * @return Zarafa error code
 */
ECRESULT ECFileAttachment::MarkAttachmentForDeletion(ULONG ulInstanceId) 
{
	ECRESULT er = erSuccess;
	string filename = CreateAttachmentFilename(ulInstanceId, m_bFileCompression);

	if(rename(filename.c_str(), string(filename+".deleted").c_str()) == 0)
		goto exit; /* successed */

	if (errno == ENOENT) {
		// retry with another filename
		filename = CreateAttachmentFilename(ulInstanceId, !m_bFileCompression);
		if(rename(filename.c_str(), string(filename+".deleted").c_str()) == 0)
			goto exit; /* successed */		
	}

	if (errno == EACCES || errno == EPERM)
		er = ZARAFA_E_NO_ACCESS;
	else if (errno == ENOENT)
		er = ZARAFA_E_NOT_FOUND;
	else
		er = ZARAFA_E_DATABASE_ERROR;

exit:
	return er;
}

/** 
 * Revert a delete marked instance
 * 
 * @param[in] ulInstanceId instance id to restore
 * 
 * @return Zarafa error code
 */
ECRESULT ECFileAttachment::RestoreMarkedAttachment(ULONG ulInstanceId)
{
	ECRESULT er = erSuccess;
	string filename = CreateAttachmentFilename(ulInstanceId, m_bFileCompression);

	if(rename(string(filename+".deleted").c_str(), filename.c_str()) == 0)
        goto exit; /* successed */

	if (errno == ENOENT) {
		// retry with another filename
		filename = CreateAttachmentFilename(ulInstanceId, !m_bFileCompression);
		if(rename(string(filename+".deleted").c_str(), filename.c_str()) == 0)
			goto exit; /* successed */
	}
	
    if (errno == EACCES || errno == EPERM)
        er = ZARAFA_E_NO_ACCESS;
    else if (errno == ENOENT)
        er = ZARAFA_E_NOT_FOUND;
    else
        er = ZARAFA_E_DATABASE_ERROR;
exit:
	return er;
}

/** 
 * Delete a marked instance from the filesystem
 * 
 * @param[in] ulInstanceId instance id to remove
 * 
 * @return Zarafa error code
 */
ECRESULT ECFileAttachment::DeleteMarkedAttachment(ULONG ulInstanceId)
{
	ECRESULT er = erSuccess;
	string filename = CreateAttachmentFilename(ulInstanceId, m_bFileCompression) + ".deleted";

	if (unlink(filename.c_str()) == 0)
		goto exit; /* Successed! */

	if (errno == ENOENT) {
		filename = CreateAttachmentFilename(ulInstanceId, !m_bFileCompression) + ".deleted";
		if (unlink(filename.c_str()) == 0)
			goto exit; /* Successed! */
	}
	
	if (errno == EACCES || errno == EPERM)
		er = ZARAFA_E_NO_ACCESS;
	else if (errno != ENOENT) // ignore "file not found" error
		er = ZARAFA_E_DATABASE_ERROR;

exit:
	return er;
}

/** 
 * Delete a single instanceid from the filesystem
 * 
 * @param[in] ulInstanceId instance id to remove
 * @param[in] bReplace Transaction marker
 * 
 * @return 
 */
ECRESULT ECFileAttachment::DeleteAttachmentInstance(ULONG ulInstanceId, bool bReplace)
{
	ECRESULT er = erSuccess;
	string filename = CreateAttachmentFilename(ulInstanceId, m_bFileCompression);
   
	if(m_bTransaction) {
		if (bReplace) {
			er = MarkAttachmentForDeletion(ulInstanceId);
			if (er != erSuccess && er != ZARAFA_E_NOT_FOUND) {
				ASSERT(FALSE);
				goto exit;
			} else if(er != ZARAFA_E_NOT_FOUND) {
				 m_setMarkedAttachment.insert(ulInstanceId);
			}

			er = erSuccess;
		} else {
			m_setDeletedAttachment.insert(ulInstanceId);
		}
		goto exit;
	}

	if (unlink(filename.c_str()) != 0) {
		if (errno == ENOENT){
			filename = CreateAttachmentFilename(ulInstanceId, !m_bFileCompression);
			if (unlink(filename.c_str()) == 0)
				goto exit; /* Success! */
		}

		if (errno == EACCES || errno == EPERM)
			er = ZARAFA_E_NO_ACCESS;
		else if (errno != ENOENT) // ignore "file not found" error
			er = ZARAFA_E_DATABASE_ERROR;
	}

exit:
	return er;
}

/** 
 * Return a filename for an instance id
 * 
 * @param[in] ulInstanceId instance id to convert to a filename
 * @param[in] bCompressed add compression marker to filename
 * 
 * @return Zarafa error code
 */
std::string ECFileAttachment::CreateAttachmentFilename(ULONG ulInstanceId, bool bCompressed)
{
	string filename;
	unsigned int l1, l2;

	l1 = ulInstanceId % ATTACH_PATHDEPTH_LEVEL1;
	l2 = (ulInstanceId / ATTACH_PATHDEPTH_LEVEL1) % ATTACH_PATHDEPTH_LEVEL2;

	filename = m_basepath + PATH_SEPARATOR + stringify(l1) + PATH_SEPARATOR + stringify(l2) + PATH_SEPARATOR + stringify(ulInstanceId);

	if (bCompressed)
		filename += ".gz";

	return filename;
}

/** 
 * Return the size of an instance
 * 
 * @param[in] ulInstanceId InstanceID to check the size for
 * @param[out] lpulSize Size of the instance
 * @param[out] lpbCompressed the instance was compressed
 * 
 * @return Zarafa error code
 */
ECRESULT ECFileAttachment::GetSizeInstance(ULONG ulInstanceId, ULONG *lpulSize, bool *lpbCompressed)
{
	ECRESULT er = erSuccess;
	string filename = CreateAttachmentFilename(ulInstanceId, m_bFileCompression);
	FILE *f = NULL;
	ULONG ulSize = 0;
	bool bCompressed = m_bFileCompression;

	/*
	 * We are always going to use the normal FILE handler for determining the file size,
	 * the gzFile handler is broken since it doesn't support SEEK_END and gzseek itself
	 * is very slow. When the attachment has been zipped, we are going to read the
	 * last 4 bytes of the file, that contains the uncompressed filesize.
	 *
	 * For uncompressed files we can still use the FILE pointer and use SEEK_END
	 * which is the most simple solution.
	 */
	f = fopen(filename.c_str(), "r");
	if (!f) {
		filename = CreateAttachmentFilename(ulInstanceId, !m_bFileCompression);
		bCompressed = !m_bFileCompression;
		f = fopen(filename.c_str(), "r");
	}
	if (!f) {
		er = ZARAFA_E_NOT_FOUND;
		goto exit;
	}

	if (!bCompressed) {
		/* Uncompressed attachment */
		fseek(f, 0, SEEK_END);
		ulSize = ftell(f);
	} else {
		/* Compressed attachment */
		fseek(f, -4, SEEK_END);
		fread(&ulSize, 1, 4, f);
	}

	fclose(f);

	*lpulSize = ulSize;
	if (lpbCompressed)
		*lpbCompressed = bCompressed;

exit:
	return er;
}

ECRESULT ECFileAttachment::Begin()
{
	ECRESULT er = erSuccess;

	if(m_bTransaction) {
		// Possible a duplicate begin call, don't destroy the data in production
		ASSERT(FALSE);
		goto exit;
	}

	// Set begin values
	m_setNewAttachment.clear();
	m_setDeletedAttachment.clear();
	m_setMarkedAttachment.clear();
	m_bTransaction = true;
exit:
	return er;
}

ECRESULT ECFileAttachment::Commit()
{
	ECRESULT er = erSuccess;
	bool bError = false;
	std::set<ULONG>::iterator iterAtt;
	
	if(!m_bTransaction) {
		ASSERT(FALSE);
		goto exit;
	}

	// Disable the transaction
	m_bTransaction = false;

	// Delete the attachments
	for (iterAtt = m_setDeletedAttachment.begin(); iterAtt != m_setDeletedAttachment.end(); iterAtt++) {
		if(DeleteAttachmentInstance(*iterAtt, false) != erSuccess)
			bError = true;
	}

	// Delete marked attachments
	for (iterAtt = m_setMarkedAttachment.begin(); iterAtt != m_setMarkedAttachment.end(); iterAtt++) {
		if (DeleteMarkedAttachment(*iterAtt) != erSuccess)
			bError = true;
	}

	if (bError) {
		ASSERT(FALSE);
		er = ZARAFA_E_DATABASE_ERROR;
	}

    m_setNewAttachment.clear();
	m_setDeletedAttachment.clear();
	m_setMarkedAttachment.clear();

exit:
	return er;
}

ECRESULT ECFileAttachment::Rollback()
{
	ECRESULT er = erSuccess;
	bool bError = false;
	std::set<ULONG>::iterator iterAtt;

	if(!m_bTransaction) {
		ASSERT(FALSE);
		goto exit;
	}

	// Disable the transaction
	m_bTransaction = false;

	// Don't delete the attachments
	m_setDeletedAttachment.clear();
	
	// Remove the created attachments
	for (iterAtt = m_setNewAttachment.begin(); iterAtt != m_setNewAttachment.end(); iterAtt++) 	{
		if(DeleteAttachmentInstance(*iterAtt, false) != erSuccess)
			bError = true;
	}
	// Restore marked attachment
	for (iterAtt = m_setMarkedAttachment.begin(); iterAtt != m_setMarkedAttachment.end(); iterAtt++) {
		if (RestoreMarkedAttachment(*iterAtt) != erSuccess)
			bError = true;
	}

	m_setNewAttachment.clear();
	m_setMarkedAttachment.clear();
	

    if (bError) {
		ASSERT(FALSE);
        er = ZARAFA_E_DATABASE_ERROR;
	}

exit:
	return er;
}
