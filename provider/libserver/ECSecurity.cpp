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

// ECSecurity.cpp: implementation of the ECSecurity class.
//
//////////////////////////////////////////////////////////////////////

#include "platform.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <dirent.h>

#include "ECDatabaseUtils.h"
#include "ECDatabase.h"
#include "ECSessionManager.h"
#include "ECSession.h"

#include "ECDefs.h"
#include "ECSecurity.h"

#include "stringutil.h"
#include "Trace.h"
#include "Zarafa.h"
#include "md5.h"

#include "ECDefs.h"
#include <mapidefs.h>
#include <mapicode.h>
#include <mapitags.h>

#include <mapiext.h>

#include <stdarg.h>

#include <openssl/ssl.h>
#include <openssl/evp.h>
#include <openssl/x509.h>
#include <openssl/bio.h>

#include <algorithm>

#include "ECUserManagement.h"
#include "SOAPUtils.h"
#include "SOAPDebug.h"
#include "edkmdb.h"
#include "ECDBDef.h"
#include "ZarafaCmdUtil.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define MAX_PARENT_LIMIT 64

/** 
 * ECSecurity constructor
 * 
 * @param[in] lpSession user session
 * @param[in] lpConfig config object
 * @param[in] lpLogger log object for normal logging
 * @param[in] lpAudit optional log object for auditing
 */
ECSecurity::ECSecurity(ECSession *lpSession, ECConfig *lpConfig, ECLogger *lpLogger, ECLogger *lpAudit)
{
	m_lpSession = lpSession;
	m_lpConfig = lpConfig;
	m_lpLogger = lpLogger;
	m_lpAudit = lpAudit;
	m_lpGroups = NULL;
	m_lpViewCompanies = NULL;
	m_lpAdminCompanies = NULL;
	m_ulUserID = 0;
	m_ulCompanyID = 0;
	m_bRestrictedAdmin = parseBool(lpConfig->GetSetting("restrict_admin_permissions"));
}

ECSecurity::~ECSecurity()
{
	if(m_lpGroups)
		delete m_lpGroups;
	if(m_lpViewCompanies)
		delete m_lpViewCompanies;
	if(m_lpAdminCompanies)
		delete m_lpAdminCompanies;
}

/** 
 * Called once for each login after the object was constructed. Since
 * this function can return errors, this is not done in the
 * constructor.
 * 
 * @param[in] ulUserId current logged in user
 * 
 * @return Zarafa error code
 */
ECRESULT ECSecurity::SetUserContext(unsigned int ulUserId)
{
	ECRESULT er = erSuccess;
	ECUserManagement *lpUserManagement = m_lpSession->GetUserManagement();

	m_ulUserID = ulUserId;

	er = lpUserManagement->GetObjectDetails(m_ulUserID, &m_details);
	if(er != erSuccess)
		goto exit;

	// Get the company we're assigned to
	if(m_lpSession->GetSessionManager()->IsHostedSupported()) {
		m_ulCompanyID = m_details.GetPropInt(OB_PROP_I_COMPANYID);
	} else {
		m_ulCompanyID = 0;
	}

	/*
	 * Don't initialize m_lpGroups, m_lpViewCompanies and m_lpAdminCompanies
	 * We should wait with that until the first time we actually use it,
	 * this will save quite a lot of LDAP queries since often we don't
	 * even need the list at all.
	 */

exit:
	return er;
}

// helper class to remember groups we've seen to break endless loops
class cUniqueGroup {
public:
	bool operator()(const localobjectdetails_t &obj) {
		return m_seen.find(obj) != m_seen.end();
	}

	std::set<localobjectdetails_t> m_seen;
};
/** 
 * This function returns a list of security groups the user is a
 * member of. If a group contains a group, it will be appended to the
 * list. The list will be a unique list of groups in the end.
 * 
 * @param[in]  ulUserId A user or group to query the grouplist for.
 * @param[out] lppGroups The unique list of group ids
 * 
 * @return Zarafa error code
 */
ECRESULT ECSecurity::GetGroupsForUser(unsigned int ulUserId, std::list<localobjectdetails_t> **lppGroups)
{
	ECRESULT er = erSuccess;
	std::list<localobjectdetails_t> *lpGroups = NULL;
	std::list<localobjectdetails_t>::iterator iterGroups;
	cUniqueGroup cSeenGroups;

	/* Gets the current user's membership information.
	 * This means you will be in the same groups until you login again */
	er = m_lpSession->GetUserManagement()->GetParentObjectsOfObjectAndSync(OBJECTRELATION_GROUP_MEMBER,
																		   ulUserId, &lpGroups, USERMANAGEMENT_IDS_ONLY);
	if (er != erSuccess)
		goto exit;

	/* A user is only member of a group when he can also view the group */
	for (iterGroups = lpGroups->begin(); iterGroups != lpGroups->end(); ) {

		/* Since this function is only used by ECSecurity, we can only
		 * test for security groups here. However, normal groups were
		 * used to be security enabled, so only check for dynamic
		 * groups here to exclude.
		 */
		if (IsUserObjectVisible(iterGroups->ulId) != erSuccess || iterGroups->GetClass() == DISTLIST_DYNAMIC) {
			lpGroups->erase(iterGroups++);
		} else {
			cSeenGroups.m_seen.insert(*iterGroups);

			std::list<localobjectdetails_t> *lpGroupInGroups = NULL;
			std::list<localobjectdetails_t>::iterator li;

			er = m_lpSession->GetUserManagement()->GetParentObjectsOfObjectAndSync(OBJECTRELATION_GROUP_MEMBER,
																				   iterGroups->ulId, &lpGroupInGroups, USERMANAGEMENT_IDS_ONLY);
			if (er == erSuccess) {
				// Adds all groups from lpGroupInGroups to the main lpGroups list, except when already in cSeenGroups
				remove_copy_if(lpGroupInGroups->begin(), lpGroupInGroups->end(), back_inserter(*lpGroups), cSeenGroups);
				delete lpGroupInGroups;
			}
			er = erSuccess;		// Ignore error (eg. cannot use that function on group Everyone)

			iterGroups++;
		}
	}

	*lppGroups = lpGroups;

exit:
	return er;
}

/** 
 * Return the bitmask of permissions for an object
 * 
 * @param[in] ulObjId hierarchy object to get permission mask for
 * @param[out] lpulRights permission mask
 * 
 * @return always erSuccess
 */
ECRESULT ECSecurity::GetObjectPermission(unsigned int ulObjId, unsigned int* lpulRights)
{
	ECRESULT		er = erSuccess;
	unsigned int	i = 0;
	list<localobjectdetails_t>::iterator iterGroups;
	struct rightsArray *lpRights = NULL;
	unsigned		ulCurObj = ulObjId;
	bool 			bFoundACL = false;
	unsigned int	ulDepth = 0;

	if(lpulRights)
		*lpulRights = 0;

	// Get the deepest GRANT ACL that applies to this user or groups that this user is in
	// WARNING we totally ignore DENY ACL's here. This means that the deepest GRANT counts. In practice
	// this doesn't matter because GRANTmask = ~DENYmask.
	while(true)
	{
		if(m_lpSession->GetSessionManager()->GetCacheManager()->GetACLs(ulCurObj, &lpRights) == erSuccess) {
			// This object has ACL's, check if any of them are for this user

			for(i=0;i<lpRights->__size;i++) {
				if(lpRights->__ptr[i].ulType == ACCESS_TYPE_GRANT && lpRights->__ptr[i].ulUserid == m_ulUserID) {
					*lpulRights |= lpRights->__ptr[i].ulRights;
					bFoundACL = true;
				}
			}

			// Check for the company we are in and add the permissions
			for (i = 0; i < lpRights->__size; i++) {
				if (lpRights->__ptr[i].ulType == ACCESS_TYPE_GRANT && lpRights->__ptr[i].ulUserid == m_ulCompanyID) {
					*lpulRights |= lpRights->__ptr[i].ulRights;
					bFoundACL = true;
				}
			}

			// Also check for groups that we are in, and add those permissions
			if(m_lpGroups || GetGroupsForUser(m_ulUserID, &m_lpGroups) == erSuccess) {
				for(iterGroups = m_lpGroups->begin(); iterGroups != m_lpGroups->end(); iterGroups++) {
					for(i=0;i<lpRights->__size;i++) {
						if(lpRights->__ptr[i].ulType == ACCESS_TYPE_GRANT && lpRights->__ptr[i].ulUserid == iterGroups->ulId) {
							*lpulRights |= lpRights->__ptr[i].ulRights;
							bFoundACL = true;
						}
					}
				}
			}
		}

		if(lpRights)
		{
			FreeRightsArray(lpRights);
			lpRights = NULL;
		}

		if(bFoundACL) {
			// If any of the ACLs at this level were for us, then use these ACLs.
			break;
		}

		// There were no ACLs or no ACLs for us, go to the parent and try there
		er = m_lpSession->GetSessionManager()->GetCacheManager()->GetParent(ulCurObj, &ulCurObj);
		if(er != erSuccess) {
			// No more parents, break (with ulRights = 0)
			er = erSuccess;
			goto exit;
		}
		
		// This can really only happen if you have a broken tree in the database, eg a record which has
		// parent == id. To break out of the loop we limit the depth to 64 which is very deep in practice. This means
		// that you never have any rights for folders that are more than 64 levels of folders away from their ACL ..
		ulDepth++;
		
		if(ulDepth > MAX_PARENT_LIMIT) {
			m_lpSession->GetSessionManager()->GetLogger()->Log(EC_LOGLEVEL_FATAL, "Maximum depth reached for object %d, deepest object: %d", ulObjId, ulCurObj);
			er = erSuccess;
			goto exit;
		}
	}

exit:
	if(lpRights)
		FreeRightsArray(lpRights);

	return er;
}

/**
 * Check permission for a certain object id
 *
 * This function checks if ANY of the passed permissions are granted on the passed object
 * for the currently logged-in user.
 *
 * @param[in] ulObjId Object ID for which the permission should be checked
 * @param[in] ulACLMask Mask of permissions to be checked
 * @return Zarafa error code
 * @retval erSuccess if permission is granted
 * @retval ZARAFA_E_NO_ACCESS if permission is denied
 */
ECRESULT ECSecurity::HaveObjectPermission(unsigned int ulObjId, unsigned int ulACLMask)
{
	unsigned int	ulRights = 0;

	GetObjectPermission(ulObjId, &ulRights);

	if((ulRights & ulACLMask) > 0) {
		return erSuccess;
	} else {
		return ZARAFA_E_NO_ACCESS;
	}
}

/** 
 * Checks if you are the owner of the given object id. This can return
 * no access, since other people may have created an object in the
 * owner's store (or true if you're that someone).
 * 
 * @param[in] ulObjId hierarchy object to check ownership of
 * 
 * @return Zarafa error code
 */
ECRESULT ECSecurity::IsOwner(unsigned int ulObjId)
{
	ECRESULT		er;
	unsigned int	ulOwner = 0;

	er = GetOwner(ulObjId, &ulOwner);
	if (er != erSuccess || ulOwner != m_ulUserID)
	{
		er = ZARAFA_E_NO_ACCESS;
		goto exit;
	}

	er = erSuccess;

exit:
	return er;
}

/** 
 * Get the original creator of an object
 * 
 * @param[in] ulObjId hierarchy object to get ownership of
 * @param[out] lpulOwnerId owner userid (may not even exist anymore)
 * 
 * @return Zarafa error code
 */
ECRESULT ECSecurity::GetOwner(unsigned int ulObjId, unsigned int *lpulOwnerId)
{
	ECRESULT		er = erSuccess;

	// Default setting
	*lpulOwnerId = 0;

	er = m_lpSession->GetSessionManager()->GetCacheManager()->GetOwner(ulObjId, lpulOwnerId);
	if (er != erSuccess)
	{
		er = ZARAFA_E_NOT_FOUND;
		goto exit;
	}

exit:
	return er;
}

/** 
 * Check for a deleted folder as parent of ulId, max folder depth as
 * defined (64).
 * 
 * @param[in] ulId object id to start checking from
 * 
 * @return ZARAFA_E_NOT_FOUND Error if a parent has the delete flag
 */
ECRESULT ECSecurity::CheckDeletedParent(unsigned int ulId)
{
	ECRESULT er = erSuccess;
	unsigned int ulParentObjId = 0;
	unsigned int ulObjFlags = 0;
	unsigned int ulObjType = 0;
	unsigned int ulDepth = 0;
	ECCacheManager *lpCache = m_lpSession->GetSessionManager()->GetCacheManager();

	do {
		er = lpCache->GetObject(ulId, &ulParentObjId, NULL, &ulObjFlags, &ulObjType);
		if (er != erSuccess)
			goto exit;

		if (ulObjFlags & MSGFLAG_DELETED) {
			er = ZARAFA_E_NOT_FOUND;
			goto exit;
		}

		ulId = ulParentObjId;
		ulDepth++;
	} while (ulObjType != MAPI_STORE && ulParentObjId != CACHE_NO_PARENT && ulDepth <= MAX_PARENT_LIMIT);

	// return error when max depth is reached, so we don't create folders and messages deeper than the limit
	if (ulDepth == MAX_PARENT_LIMIT)
		er = ZARAFA_E_NOT_FOUND;

exit:
	return er;
}

/** 
 * For the current user context, check the permissions on a given object
 * 
 * @param[in] ulObjId hierarchy object to check permissions on
 * @param[in] ulecRights minimal permission required on object to succeed
 * 
 * @return Zarafa error code
 * @retval erSuccess requested access on object allowed
 * @retval ZARAFA_E_NO_ACCESS requested access on object denied
 */
ECRESULT ECSecurity::CheckPermission(unsigned int ulObjId, unsigned int ulecRights)
{
	ECRESULT		er = ZARAFA_E_NO_ACCESS;
	bool			bOwnerFound = false;
	unsigned int	ulStoreOwnerId = 0;
	unsigned int	ulStoreType = 0;
	unsigned int	ulObjectOwnerId = 0;
	unsigned int	ulACL = 0;
	int				nCheckType = 0;
	unsigned int	ulObjType;
	unsigned int	ulParentId;
	unsigned int	ulParentType;

	if(m_ulUserID == ZARAFA_UID_SYSTEM) {
		// SYSTEM is always allowed everything
		er = erSuccess;
		goto exit;
	}

	// special case: stores and root containers are always allowed to be opened
	if (ulecRights == ecSecurityFolderVisible || ulecRights == ecSecurityRead) {
		er = m_lpSession->GetSessionManager()->GetCacheManager()->GetObject(ulObjId, &ulParentId, NULL, NULL, &ulObjType);
		if (er != erSuccess)
			goto exit;
		if (ulObjType == MAPI_STORE)
			goto exit;
		er = m_lpSession->GetSessionManager()->GetCacheManager()->GetObject(ulParentId, NULL, NULL, NULL, &ulParentType);
		if (er != erSuccess)
			goto exit;
		if (ulObjType == MAPI_FOLDER && ulParentType == MAPI_STORE)
			goto exit;
	}

	// Is the current user the owner of the store
	if(GetStoreOwnerAndType(ulObjId, &ulStoreOwnerId, &ulStoreType) == erSuccess && ulStoreOwnerId == m_ulUserID) {
		if (ulStoreType == ECSTORE_TYPE_ARCHIVE) {
			if (ulecRights == ecSecurityFolderVisible || ulecRights == ecSecurityRead) {
				er = erSuccess;
				goto exit;
			}
		} else {
			er = erSuccess;
			goto exit;
		}
	}

	// is current user the owner of the object
	if (GetOwner(ulObjId, &ulObjectOwnerId) == erSuccess && ulObjectOwnerId == m_ulUserID)
	{
		bOwnerFound = true;
		if (ulStoreType == ECSTORE_TYPE_ARCHIVE) {
			if(ulecRights == ecSecurityFolderVisible || ulecRights == ecSecurityRead) {
				er = erSuccess;
				goto exit;
			}
		} else {
			if(ulecRights == ecSecurityFolderVisible || ulecRights == ecSecurityRead || ulecRights == ecSecurityCreate) {
				er = erSuccess;
				goto exit;
			}
		}
	}

	// Since this is the most complicated check, do this one last
	if(IsAdminOverOwnerOfObject(ulObjId) == erSuccess) {
		if(!m_bRestrictedAdmin) {
			er = erSuccess;
			goto exit;
		} else {
			// If restricted admin mode is set, admins only receive folder permissions.
			if(ulecRights == ecSecurityFolderVisible || ulecRights == ecSecurityFolderAccess || ulecRights == ecSecurityCreateFolder) {
				er = erSuccess;
				goto exit;
			}
		}
	}

	ulACL = 0;
	switch(ulecRights){
		case ecSecurityRead:// 1
			ulACL |= ecRightsReadAny;
			nCheckType = 1;
			break;
		case ecSecurityCreate:// 2
			ulACL |= ecRightsCreate;
			nCheckType = 1;
			break;
		case ecSecurityEdit:// 3
			ulACL |= ecRightsEditAny;
			if(bOwnerFound)
				ulACL |= ecRightsEditOwned;
			nCheckType = 1;
			break;
		case ecSecurityDelete:// 4
			ulACL |= ecRightsDeleteAny;
			if(bOwnerFound)
				ulACL |= ecRightsDeleteOwned;
			nCheckType = 1;
			break;
		case ecSecurityCreateFolder:// 5
			ulACL |= ecRightsCreateSubfolder;
			nCheckType = 1;
			break;
		case ecSecurityFolderVisible:// 6
			ulACL |= ecRightsFolderVisible;
			nCheckType = 1;
			break;
		case ecSecurityFolderAccess: // 7
			if(bOwnerFound == false || ulStoreType == ECSTORE_TYPE_ARCHIVE)
				ulACL |= ecRightsFolderAccess;
			nCheckType = 1;
			break;
		case ecSecurityOwner:// 8
			nCheckType = 2;
			break;
		case ecSecurityAdmin:// 9
			nCheckType = 3;
			break;
		default:
			nCheckType = 0;//  No rights
		break;
	}

	if(nCheckType == 1) { //Get the acl of the object
		if(ulACL == 0) {
			// No ACLs required, so grant access
			er = erSuccess;
			goto exit;
		}
		er = HaveObjectPermission(ulObjId, ulACL);
	} else if(nCheckType == 2) {// Is owner ?
		if (bOwnerFound)
			er = erSuccess;
	} else if(nCheckType == 3) { // Is admin?
		// We already checked IsAdminOverOwnerOfObject() above, so we'll never get here.
		er = erSuccess;
	}

exit:
	if (er == erSuccess && (ulecRights == ecSecurityCreate || ulecRights == ecSecurityEdit || ulecRights == ecSecurityCreateFolder)) {
		// writing in a deleted parent is not allowed
		er = CheckDeletedParent(ulObjId);
	}

	if(er != erSuccess)
		TRACE_INTERNAL(TRACE_ENTRY,"Security","ECSecurity::CheckPermission","object=%d, rights=%d", ulObjId, ulecRights);

	if (m_lpAudit && m_ulUserID != ZARAFA_UID_SYSTEM) {
		unsigned int ulType = 0;
		objectdetails_t sStoreDetails;
		std::string strStoreOwner;
		std::string strUsername;

		m_lpSession->GetSessionManager()->GetCacheManager()->GetObject(ulObjId, NULL, NULL, NULL, &ulType);
		if (er == ZARAFA_E_NO_ACCESS || ulStoreOwnerId != m_ulUserID) {
			GetUsername(&strUsername);
			if (ulStoreOwnerId != m_ulUserID) {
				if (m_lpSession->GetUserManagement()->GetObjectDetails(ulStoreOwnerId, &sStoreDetails) != erSuccess) {
					// should not really happen on store owners?
					strStoreOwner = "<non-existing>";
				} else {
					strStoreOwner = sStoreDetails.GetPropString(OB_PROP_S_LOGIN);
				}
			} else {
				strStoreOwner = strUsername;
			}
		}

		if (er == ZARAFA_E_NO_ACCESS) {
			m_lpAudit->Log(EC_LOGLEVEL_FATAL, "access denied objectid=%d type=%d ownername='%s' username='%s' rights='%s'",
						   ulObjId, ulType, strStoreOwner.c_str(), strUsername.c_str(), RightsToString(ulecRights));
		} else if (ulStoreOwnerId != m_ulUserID) {
			m_lpAudit->Log(EC_LOGLEVEL_FATAL, "access allowed objectid=%d type=%d ownername='%s' username='%s' rights='%s'",
						   ulObjId, ulType, strStoreOwner.c_str(), strUsername.c_str(), RightsToString(ulecRights));
		} else {
			// you probably don't want to log all what a user does in it's own store, do you?
			m_lpAudit->Log(EC_LOGLEVEL_INFO, "access allowed objectid=%d type=%d userid=%d", ulObjId, ulType, m_ulUserID);
		}
	}

	return er;
}

/** 
 * Get the ACLs on a given object in a protocol struct to send to the client
 * 
 * @param[in] objid hierarchy object to get the ACLs for
 * @param[in] ulType rights access type, denied or grant
 * @param[out] lpsRightsArray structure with the current rights on objid
 * 
 * @return Zarafa error code
 */
ECRESULT ECSecurity::GetRights(unsigned int objid, int ulType, struct rightsArray *lpsRightsArray)
{
	ECRESULT			er = ZARAFA_E_NO_ACCESS;
	DB_RESULT			lpDBResult = NULL;
	DB_ROW				lpDBRow = NULL;
	ECDatabase			*lpDatabase = NULL;
	std::string			strQuery;
	unsigned int		ulCount = 0;
	unsigned int		i=0;
	objectid_t			sExternId;

	er = m_lpSession->GetDatabase(&lpDatabase);
	if (er != erSuccess)
		goto exit;

	if (lpsRightsArray == NULL) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	strQuery = "SELECT a.id, a.type, a.rights FROM acl AS a WHERE a.hierarchy_id="+stringify(objid);

	if(ulType == ACCESS_TYPE_DENIED)
		strQuery += " AND a.type="+stringify(ACCESS_TYPE_DENIED);
	else if(ulType == ACCESS_TYPE_GRANT)
		strQuery += " AND a.type="+stringify(ACCESS_TYPE_GRANT);
	// else ACCESS_TYPE_DENIED and ACCESS_TYPE_GRANT

	er = lpDatabase->DoSelect(strQuery, &lpDBResult);
	if(er != erSuccess)
		goto exit;

	ulCount = lpDatabase->GetNumRows(lpDBResult);
	if(ulCount > 0)
	{
		lpsRightsArray->__ptr = new struct rights[ulCount];
		lpsRightsArray->__size = ulCount;

		memset(lpsRightsArray->__ptr, 0, sizeof(struct rights) * ulCount);

		for(i=0; i < ulCount; i++){
			lpDBRow = lpDatabase->FetchRow(lpDBResult);

			if(lpDBRow == NULL) {
				er = ZARAFA_E_DATABASE_ERROR;
				goto exit;
			}

			lpsRightsArray->__ptr[i].ulUserid = atoi(lpDBRow[0]);
			lpsRightsArray->__ptr[i].ulType   = atoi(lpDBRow[1]);
			lpsRightsArray->__ptr[i].ulRights = atoi(lpDBRow[2]);
			lpsRightsArray->__ptr[i].ulState  = RIGHT_NORMAL;

			// do not use internal id's with the cache
			if (lpsRightsArray->__ptr[i].ulUserid == ZARAFA_UID_SYSTEM) {
				sExternId = objectid_t(ACTIVE_USER);
			} else if (lpsRightsArray->__ptr[i].ulUserid == ZARAFA_UID_EVERYONE) {
				sExternId = objectid_t(DISTLIST_GROUP);
			} else {
				er = m_lpSession->GetUserManagement()->GetExternalId(lpsRightsArray->__ptr[i].ulUserid, &sExternId);
				if (er != erSuccess)
					goto exit;
			}

			er = ABIDToEntryID(NULL, lpsRightsArray->__ptr[i].ulUserid, sExternId, &lpsRightsArray->__ptr[i].sUserId);
			if (er != erSuccess)
				goto exit;
		}
	}else{
		lpsRightsArray->__ptr = NULL;
		lpsRightsArray->__size = 0;
	}

	er = erSuccess;

exit:
	if(lpDBResult)
		lpDatabase->FreeResult(lpDBResult);

	return er;
}

/** 
 * Update the rights on a given object
 * 
 * @param[in] objid hierarchy object id to set rights on
 * @param[in] lpsRightsArray protocol struct containing new rights for this object
 * 
 * @return Zarafa error code
 */
ECRESULT ECSecurity::SetRights(unsigned int objid, struct rightsArray *lpsRightsArray)
{
	ECRESULT			er = erSuccess;
	unsigned int		i;
	std::string			strQueryNew, strQueryDeniedNew;
	std::string			strQueryModify, strQueryDeniedModify;
	std::string			strQueryDelete, strQueryDeniedDelete;
	unsigned int		ulDeniedRights=0;
	ECDatabase			*lpDatabase = NULL;
	unsigned int		ulUserId = 0;
	objectid_t			sExternId;
	objectdetails_t		sDetails;
	unsigned int		ulErrors = 0;

	er = m_lpSession->GetDatabase(&lpDatabase);
	if (er != erSuccess)
		goto exit;

	if (lpsRightsArray == NULL) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	// Invalidate cache for this object
	m_lpSession->GetSessionManager()->GetCacheManager()->Update(fnevObjectModified, objid);

	for(i=0; i< lpsRightsArray->__size; i++)
	{
		// FIXME: check for each object if it belongs to the store we're logged into (except for admin)

		// Get the correct local id
		if (lpsRightsArray->__ptr[i].sUserId.__size > 0 && lpsRightsArray->__ptr[i].sUserId.__ptr != NULL)
		{
			er = ABEntryIDToID(&lpsRightsArray->__ptr[i].sUserId, &ulUserId, &sExternId, NULL);
			if (er != erSuccess)
				goto exit;

			// internal user/group doesn't have an externid
			if (!sExternId.id.empty())
			{
				// Get real ulUserId on this server
				er = m_lpSession->GetUserManagement()->GetLocalId(sExternId, &ulUserId, NULL);
				if (er != erSuccess)
					goto exit;
			}
		}
		else
			ulUserId = lpsRightsArray->__ptr[i].ulUserid;

		er = m_lpSession->GetUserManagement()->GetObjectDetails(ulUserId, &sDetails);
		if (er != erSuccess)
			goto exit;

		// You can only set (delete is ok) permissions on active users, and security groups
		// Outlook 2007 blocks this client side, other clients get this error.
		if ((lpsRightsArray->__ptr[i].ulState & ~(RIGHT_DELETED | RIGHT_AUTOUPDATE_DENIED)) != 0 &&
			sDetails.GetClass() != ACTIVE_USER &&
			sDetails.GetClass() != DISTLIST_SECURITY &&
			sDetails.GetClass() != CONTAINER_COMPANY) {
				ulErrors++;
				continue;
		}

		// Auto create denied rules
		ulDeniedRights = lpsRightsArray->__ptr[i].ulRights^ecRightsAllMask;
		if(lpsRightsArray->__ptr[i].ulRights & ecRightsEditAny)
			ulDeniedRights&=~ecRightsEditOwned;
		else if(lpsRightsArray->__ptr[i].ulRights & ecRightsEditOwned){
			ulDeniedRights&=~ecRightsEditOwned;
			ulDeniedRights|=ecRightsEditAny;
		}

		if(lpsRightsArray->__ptr[i].ulRights & ecRightsDeleteAny)
			ulDeniedRights&=~ecRightsDeleteOwned;
		else if(lpsRightsArray->__ptr[i].ulRights & ecRightsDeleteOwned)
		{
			ulDeniedRights&=~ecRightsDeleteOwned;
			ulDeniedRights|=ecRightsDeleteAny;
		}

		if(lpsRightsArray->__ptr[i].ulState == RIGHT_NORMAL)
		{
			// Do nothing...
		}
		else if((lpsRightsArray->__ptr[i].ulState & RIGHT_NEW) || (lpsRightsArray->__ptr[i].ulState & RIGHT_MODIFY))
		{
			strQueryNew = "REPLACE INTO acl (id, hierarchy_id, type, rights) VALUES ";
			strQueryNew+="("+stringify(ulUserId)+","+stringify(objid)+","+stringify(lpsRightsArray->__ptr[i].ulType)+","+stringify(lpsRightsArray->__ptr[i].ulRights)+")";

			er = lpDatabase->DoInsert(strQueryNew);
			if(er != erSuccess)
				goto exit;

			if(lpsRightsArray->__ptr[i].ulState & RIGHT_AUTOUPDATE_DENIED){
				strQueryNew = "REPLACE INTO acl (id, hierarchy_id, type, rights) VALUES ";
				strQueryNew+=" ("+stringify(ulUserId)+","+stringify(objid)+","+stringify(ACCESS_TYPE_DENIED)+","+stringify(ulDeniedRights)+")";

				er = lpDatabase->DoInsert(strQueryNew);
				if(er != erSuccess)
					goto exit;

			}

		}
		else if(lpsRightsArray->__ptr[i].ulState & RIGHT_DELETED)
		{
			strQueryDelete = "DELETE FROM acl WHERE ";
			strQueryDelete+="(hierarchy_id="+stringify(objid)+" AND id="+stringify(ulUserId)+" AND type="+stringify(lpsRightsArray->__ptr[i].ulType)+")";

			er = lpDatabase->DoDelete(strQueryDelete);
			if(er != erSuccess)
				goto exit;

			if(lpsRightsArray->__ptr[i].ulState & RIGHT_AUTOUPDATE_DENIED) {
				strQueryDelete = "DELETE FROM acl WHERE ";
				strQueryDelete+="(hierarchy_id="+stringify(objid)+" AND id="+stringify(ulUserId)+" AND type="+stringify(ACCESS_TYPE_DENIED)+")";

				er = lpDatabase->DoDelete(strQueryDelete);
				if(er != erSuccess)
					goto exit;
			}

		}else{
			// a Hacker ?
		}
	}

	if (ulErrors == lpsRightsArray->__size)
		er = ZARAFA_E_INVALID_PARAMETER;	// all acl's failed
	else if (ulErrors)
		er = ZARAFA_W_PARTIAL_COMPLETION;	// some acl's failed
	else
		er = erSuccess;

exit:
	return er;
}

/** 
 * Return the company id of the current user context
 * 
 * @param[out] lpulCompanyId company id of user
 * 
 * @return always erSuccess
 */
ECRESULT ECSecurity::GetUserCompany(unsigned int *lpulCompanyId)
{
	*lpulCompanyId = m_ulCompanyID;
	return erSuccess;
}

/** 
 * Get a list of company ids that may be viewed by the current user
 * 
 * @param[in] ulFlags Usermanagemnt flags
 * @param[out] lppObjects New allocated list of company details
 * 
 * @return Zarafa error code
 */
ECRESULT ECSecurity::GetViewableCompanyIds(unsigned int ulFlags, list<localobjectdetails_t> **lppObjects)
{
	ECRESULT er = erSuccess;
	list<localobjectdetails_t>::iterator iter;

	/*
	 * We have the viewable companies stored in our cache,
	 * if it is present use that, otherwise just create a
	 * new one.
	 * NOTE: We always request GetViewableCompanies with 0 as ulFlags,
	 * this because we are caching the list here and some callers might
	 * want all details while others will only want the id's.
	 */
	if (!m_lpViewCompanies) {
		er = GetViewableCompanies(0, &m_lpViewCompanies);
		if (er != erSuccess)
			goto exit;
	}

	/*
	 * Because of the difference in flags it is possible we have
	 * too many entries in the list. We need to filter those out now.
	 */
	*lppObjects = new list<localobjectdetails_t>();
	for (iter = m_lpViewCompanies->begin(); iter != m_lpViewCompanies->end(); iter++) {
		if ((m_ulUserID != 0) &&
			(ulFlags & USERMANAGEMENT_ADDRESSBOOK) &&
			iter->GetPropBool(OB_PROP_B_AB_HIDDEN))
			continue;

		if (ulFlags & USERMANAGEMENT_IDS_ONLY)
			(*lppObjects)->push_back(localobjectdetails_t(iter->ulId, iter->GetClass()));
		else
			(*lppObjects)->push_back(localobjectdetails_t(iter->ulId, *iter));
	}

exit:
	return er;
}

/** 
 * Check if the given user id is readable by the current user
 * 
 * @param[in] ulUserObjectId internal user id
 * 
 * @return Zarafa error code
 * @retval erSuccess viewable
 * @retval ZARAFA_E_NOT_FOUND not viewable
 */
ECRESULT ECSecurity::IsUserObjectVisible(unsigned int ulUserObjectId)
{
	ECRESULT er = erSuccess;
	list<localobjectdetails_t>::iterator iterCompany;
	objectid_t sExternId;
	unsigned int ulCompanyId;

	if (ulUserObjectId == 0 ||
		ulUserObjectId == m_ulUserID ||
		ulUserObjectId == m_ulCompanyID ||
		ulUserObjectId == ZARAFA_UID_SYSTEM ||
		ulUserObjectId == ZARAFA_UID_EVERYONE ||
		m_details.GetPropInt(OB_PROP_I_ADMINLEVEL) == ADMIN_LEVEL_SYSADMIN ||
		!m_lpSession->GetSessionManager()->IsHostedSupported()) {
		goto exit;
	}

	er = m_lpSession->GetUserManagement()->GetExternalId(ulUserObjectId, &sExternId, &ulCompanyId);
	if (er != erSuccess)
		goto exit;

	// still needed?
	if (sExternId.objclass == CONTAINER_COMPANY)
		ulCompanyId = ulUserObjectId;

	if (!m_lpViewCompanies) {
		er = GetViewableCompanies(0, &m_lpViewCompanies);
		if (er != erSuccess)
			goto exit;
	}

	for (iterCompany = m_lpViewCompanies->begin(); iterCompany != m_lpViewCompanies->end(); iterCompany++) {
		if (iterCompany->ulId == ulCompanyId) {
			er = erSuccess;
			goto exit;
		}
	}

	/* Item was not found */
	er = ZARAFA_E_NOT_FOUND;

exit:
	return er;
}

/** 
 * Internal helper function to get a list of viewable company details
 *
 * @todo won't this bug when we cache only the id's in a first call,
 * and then when we need the full details, the m_lpViewCompanies will
 * only contain the id's?
 *
 * @param[in] ulFlags usermanagement flags
 * @param[in] lppObjects new allocated list with company details
 * 
 * @return 
 */
ECRESULT ECSecurity::GetViewableCompanies(unsigned int ulFlags, list<localobjectdetails_t> **lppObjects)
{
	ECRESULT er = erSuccess;
	list<localobjectdetails_t> *lpObjects = NULL;
	objectdetails_t details;

	if (m_details.GetPropInt(OB_PROP_I_ADMINLEVEL) == ADMIN_LEVEL_SYSADMIN)
		er = m_lpSession->GetUserManagement()->GetCompanyObjectListAndSync(CONTAINER_COMPANY, 0, &lpObjects, ulFlags);
	else
		er = m_lpSession->GetUserManagement()->GetParentObjectsOfObjectAndSync(OBJECTRELATION_COMPANY_VIEW,
																			   m_ulCompanyID, &lpObjects,
																			   ulFlags);
	if (er != erSuccess) {
		/* Whatever the error might be, it only indicates we
		 * are not allowed to view _other_ companyspaces.
		 * It doesn't restrict us from viewing our own... */
		lpObjects = new list<localobjectdetails_t>();
		er = erSuccess;
	}

	/* We are going to insert the requested companyID to the list as well,
	 * this way we guarentee that _all_ viewable companies are in the list.
	 * And we can use sort() and unique() to prevent duplicate entries while
	 * making sure the we can savely handle things when the current company
	 * is either added or not present in the RemoteViewableCompanies. */
	if (m_ulCompanyID != 0) {
		if (!(ulFlags & USERMANAGEMENT_IDS_ONLY)) {
			er = m_lpSession->GetUserManagement()->GetObjectDetails(m_ulCompanyID, &details);
			if (er != erSuccess)
				goto exit;
		} else {
			details = objectdetails_t(CONTAINER_COMPANY);
		}
		lpObjects->push_back(localobjectdetails_t(m_ulCompanyID, details));
	}

	lpObjects->sort();
	lpObjects->unique();

	*lppObjects = lpObjects;

exit:
	if (er != erSuccess && lpObjects)
		delete lpObjects;

	return er;
}

/** 
 * Get a list of company details which the current user is admin over
 *
 * @param[in] ulFlags usermanagement flags
 * @param[out] lppObjects company list
 * 
 * @return Zarafa error code
 */
ECRESULT ECSecurity::GetAdminCompanies(unsigned int ulFlags, list<localobjectdetails_t> **lppObjects)
{
	ECRESULT er = erSuccess;
	list<localobjectdetails_t> *lpObjects = NULL;
	list<localobjectdetails_t>::iterator iterObjects;
	list<localobjectdetails_t>::iterator iterObjectsRemove;

	if (m_details.GetPropInt(OB_PROP_I_ADMINLEVEL) == ADMIN_LEVEL_SYSADMIN)
		er = m_lpSession->GetUserManagement()->GetCompanyObjectListAndSync(CONTAINER_COMPANY, 0, &lpObjects, ulFlags);
	else
		er = m_lpSession->GetUserManagement()->GetParentObjectsOfObjectAndSync(OBJECTRELATION_COMPANY_ADMIN,
																			   m_ulUserID, &lpObjects,
																			   ulFlags);
	if (er != erSuccess)
		goto exit;

	/* A user is only admin over an company when he has privileges to view the company */
	for (iterObjects = lpObjects->begin(); iterObjects != lpObjects->end(); ) {
		if (IsUserObjectVisible(iterObjects->ulId) != erSuccess) {
			iterObjectsRemove = iterObjects;
			iterObjects++;
			lpObjects->erase(iterObjectsRemove);
		} else {
			iterObjects++;
		}
	}

	*lppObjects = lpObjects;

exit:
	if (er != erSuccess && lpObjects)
		delete lpObjects;

	return er;
}

/** 
 * Return the current logged in UserID _OR_ if you're an administrator
 * over user that is set as owner of the given object, return the
 * owner of the object.
 * 
 * @param[in] ulObjId object to get ownership of if admin, defaults to 0 to get the current UserID
 * 
 * @return user id of object
 */
unsigned int ECSecurity::GetUserId(unsigned int ulObjId)
{
	ECRESULT er = erSuccess;

	unsigned int ulUserId = this->m_ulUserID;

	if (ulObjId != 0 && IsAdminOverOwnerOfObject(ulObjId) == erSuccess) {
		er = GetOwner(ulObjId, &ulUserId);
		if (er != erSuccess)
			ulUserId = this->m_ulUserID;
	}

	return ulUserId;
}

/** 
 * Check if the given object id is in your own store
 * 
 * @param[in] ulObjId hierarchy object id of object to check
 * 
 * @return Zarafa error code
 * @retval erSuccess object is in current user's store
 */
ECRESULT ECSecurity::IsStoreOwner(unsigned int ulObjId)
{
	ECRESULT er = erSuccess;
	unsigned int ulStoreId = 0;

	er = m_lpSession->GetSessionManager()->GetCacheManager()->GetStore(ulObjId, &ulStoreId, NULL);
	if(er != erSuccess)
		goto exit;

	er = IsOwner(ulStoreId);
	if(er != erSuccess)
		goto exit;

exit:
	return er;
}

/** 
 * Return the current user's admin level
 * 
 * @return admin level of user
 */
int ECSecurity::GetAdminLevel()
{
	return m_details.GetPropInt(OB_PROP_I_ADMINLEVEL);
}

/** 
 * Get the owner of the store in which the given objectid resides in
 * 
 * @param[in] ulObjId hierarchy object id to get store owner of
 * @param[in] lpulOwnerId user id of store in which ulObjId resides
 * 
 * @return Zarafa error code
 */
ECRESULT ECSecurity::GetStoreOwner(unsigned int ulObjId, unsigned int* lpulOwnerId)
{
	return GetStoreOwnerAndType(ulObjId, lpulOwnerId, NULL);
}

/** 
 * Get the owner and type of the store in which the given objectid resides in
 * 
 * @param[in] ulObjId hierarchy object id to get store owner of
 * @param[out] lpulOwnerId user id of store in which ulObjId resides
 * @param[out] lpulStoreType type of store in which ulObjId resides
 * 
 * @return Zarafa error code
 */
ECRESULT ECSecurity::GetStoreOwnerAndType(unsigned int ulObjId, unsigned int* lpulOwnerId, unsigned int* lpulStoreType)
{
	ECRESULT er = erSuccess;
	unsigned int ulStoreId = 0;

	if (lpulOwnerId || lpulStoreType) {
		er = m_lpSession->GetSessionManager()->GetCacheManager()->GetStoreAndType(ulObjId, &ulStoreId, NULL, lpulStoreType);
		if (er != erSuccess)
			goto exit;
	}

	if (lpulOwnerId) {
		er = GetOwner(ulStoreId, lpulOwnerId);
		if (er != erSuccess)
			goto exit;
	}

exit:
	return er;
}

/** 
 * Check if the current user is admin over a given user id (user/group/company/...)
 * 
 * @todo this function should be renamed to IsAdminOfUserObject(id) or something like that
 * 
 * @param[in] ulUserObjectId id of user
 * 
 * @return Zarafa error code
 * @retval erSuccess Yes, admin
 * @retval ZARAFA_E_NO_ACCESS No, not admin
 */
ECRESULT ECSecurity::IsAdminOverUserObject(unsigned int ulUserObjectId)
{
	ECRESULT er = ZARAFA_E_NO_ACCESS;
	list<localobjectdetails_t>::iterator objectIter;
	unsigned int ulCompanyId;
	objectdetails_t objectdetails;
	objectid_t sExternId;

	/* Hosted disabled: When admin level is not zero, then the user
	 * is the administrator, otherwise the user isn't and we don't need
	 * to look any further. */
	if (!m_lpSession->GetSessionManager()->IsHostedSupported()) {
		if (m_details.GetPropInt(OB_PROP_I_ADMINLEVEL) != 0)
			er = erSuccess;
		goto exit;
	}

	/* If hosted is enabled, system administrators are administrator over all users. */
	if (m_details.GetPropInt(OB_PROP_I_ADMINLEVEL) == ADMIN_LEVEL_SYSADMIN) {
		er = erSuccess;
		goto exit;
	}

	/*
	 * Determine to which company the user belongs
	 */
	er = m_lpSession->GetUserManagement()->GetExternalId(ulUserObjectId, &sExternId, &ulCompanyId);
	if (er != erSuccess)
		goto exit;

	// still needed?
	if (sExternId.objclass == CONTAINER_COMPANY)
		ulCompanyId = ulUserObjectId;

	/*
	 * If ulCompanyId is the company where the logged in user belongs to,
	 * then the only thing we need to check is the "isadmin" boolean.
	 */
	if (m_ulCompanyID == ulCompanyId) {
		if (m_details.GetPropInt(OB_PROP_I_ADMINLEVEL) != 0)
			er = erSuccess;
		else
			er = ZARAFA_E_NO_ACCESS;
		goto exit;
	}

	if (!m_lpAdminCompanies) {
		er = GetAdminCompanies(USERMANAGEMENT_IDS_ONLY, &m_lpAdminCompanies);
		if (er != erSuccess)
			goto exit;
	}

	for (objectIter = m_lpAdminCompanies->begin(); objectIter != m_lpAdminCompanies->end(); objectIter++) {
		if (objectIter->ulId == ulCompanyId) {
			er = erSuccess;
			goto exit;
		}
	}

	/* Item was not found, so no access */
	er = ZARAFA_E_NO_ACCESS;

exit:
	return er;
}

/** 
 * Check if we're admin over the user who owns the given object id
 * 
 * @param ulObjectId hierarchy object id
 * 
 * @return Zarafa error code
 * @retval erSuccess Yes
 * @retval ZARAFA_E_NO_ACCESS No
 */
ECRESULT ECSecurity::IsAdminOverOwnerOfObject(unsigned int ulObjectId)
{
	ECRESULT er = ZARAFA_E_NO_ACCESS;
	unsigned int ulOwner;

	/*
	 * Request the ownership if the object.
	 */
	er = GetStoreOwner(ulObjectId, &ulOwner);
	if (er != erSuccess)
		goto exit;

	er = IsAdminOverUserObject(ulOwner);

exit:
	return er;
}

/** 
 * Get the size of the store in which the given ulObjId resides in
 * 
 * @param[in] ulObjId hierarchy object id
 * @param[out] lpllStoreSize size of store
 * 
 * @return Zarafa error code
 */
ECRESULT ECSecurity::GetStoreSize(unsigned int ulObjId, long long* lpllStoreSize)
{
	ECRESULT		er = erSuccess;
	ECDatabase		*lpDatabase = NULL;
	DB_RESULT		lpDBResult = NULL;
	DB_ROW			lpDBRow = NULL;
	std::string		strQuery;
	unsigned int	ulStore;

	er = m_lpSession->GetDatabase(&lpDatabase);
	if (er != erSuccess)
		goto exit;

	er = m_lpSession->GetSessionManager()->GetCacheManager()->GetStore(ulObjId, &ulStore, NULL);
	if(er != erSuccess)
		goto exit;

	strQuery = "SELECT val_longint FROM properties WHERE tag="+stringify(PROP_ID(PR_MESSAGE_SIZE_EXTENDED))+" AND type="+stringify(PROP_TYPE(PR_MESSAGE_SIZE_EXTENDED)) + " AND hierarchyid="+stringify(ulStore);
	er = lpDatabase->DoSelect(strQuery, &lpDBResult);
	if(er != erSuccess)
		goto exit;

	if(lpDatabase->GetNumRows(lpDBResult) != 1) {
		// This mostly happens when we're creating a new store, so return 0 sized store
		er = erSuccess;
		*lpllStoreSize = 0;
		goto exit;
	}

	lpDBRow = lpDatabase->FetchRow(lpDBResult);
	if(lpDBRow == NULL || lpDBRow[0] == NULL) {
		er = ZARAFA_E_DATABASE_ERROR;
		goto exit;
	}

	*lpllStoreSize = _atoi64(lpDBRow[0]);

exit:
	if(lpDBResult)
		lpDatabase->FreeResult(lpDBResult);

	return er;
}

/** 
 * Get the store size of a given user
 * 
 * @param[in] ulUserId internal user id
 * @param[out] lpllUserSize store size of user
 * 
 * @return Zarafa error code
 */
ECRESULT ECSecurity::GetUserSize(unsigned int ulUserId, long long* lpllUserSize)
{
	ECRESULT		er = erSuccess;
	ECDatabase		*lpDatabase = NULL;
	DB_RESULT		lpDBResult = NULL;
	DB_ROW			lpDBRow = NULL;

	std::string		strQuery;
	long long		llUserSize = 0;

	er = m_lpSession->GetDatabase(&lpDatabase);
	if (er != erSuccess)
		goto exit;

	strQuery =
		"SELECT p.val_longint "
		"FROM properties AS p "
		"JOIN stores AS s "
			"ON s.hierarchy_id = p.hierarchyid "
		"WHERE "
			"s.user_id = " + stringify(ulUserId) + " " +
			"AND p.tag = " + stringify(PROP_ID(PR_MESSAGE_SIZE_EXTENDED)) + " "
			"AND p.type = " + stringify(PROP_TYPE(PR_MESSAGE_SIZE_EXTENDED));
	er = lpDatabase->DoSelect(strQuery, &lpDBResult);
	if(er != erSuccess)
		goto exit;

	if (lpDatabase->GetNumRows(lpDBResult) != 1) {
		*lpllUserSize = 0;
		goto exit;
	}

	lpDBRow = lpDatabase->FetchRow(lpDBResult);
	if (lpDBRow == NULL) {
		er = ZARAFA_E_DATABASE_ERROR;
		goto exit;
	}

	if (lpDBRow[0] == NULL)
		llUserSize = 0;
	else
		llUserSize = _atoi64(lpDBRow[0]);

	*lpllUserSize = llUserSize;

exit:
	if(lpDBResult)
		lpDatabase->FreeResult(lpDBResult);

	return er;
}

/** 
 * Gets the quota status value (ok, warn, soft, hard) for a given
 * store and store size
 * 
 * @note if you already know the owner of the store, it's better to
 * call ECSecurity::CheckUserQuota
 *
 * @param[in] ulStoreId store to check quota for
 * @param[in] llStoreSize current store size of the store
 * @param[out] lpQuotaStatus quota status value
 * 
 * @return Zarafa error code
 */
ECRESULT ECSecurity::CheckQuota(unsigned int ulStoreId, long long llStoreSize, eQuotaStatus* lpQuotaStatus)
{
	ECRESULT er = erSuccess;
	unsigned int ulOwnerId;

	// Get the store owner
	er = GetStoreOwner(ulStoreId, &ulOwnerId);
	if(er != erSuccess)
		goto exit;

	er = CheckUserQuota(ulOwnerId, llStoreSize, lpQuotaStatus);

exit:
	return er;
}

/** 
 * Gets the quota status value (ok, warn, soft, hard) for a given
 * store and store size
 * 
 * @param[in] ulUserId user to check quota for
 * @param[in] llStoreSize current store size of the store
 * @param[out] lpQuotaStatus quota status
 * 
 * @return Zarafa error code
 */
ECRESULT ECSecurity::CheckUserQuota(unsigned int ulUserId, long long llStoreSize, eQuotaStatus *lpQuotaStatus)
{
	ECRESULT		er = erSuccess;
	quotadetails_t	quotadetails;

	if (ulUserId == ZARAFA_UID_EVERYONE) {
		/* Publicly owned stores are never over quota.
		 * But do publicly owned stores actually exist since the owner is either a user or company */
		*lpQuotaStatus = QUOTA_OK;
		goto exit;
	}

	er = GetUserQuota(ulUserId, false, &quotadetails);
	if (er != erSuccess)
		goto exit;

	// check the options
	if(quotadetails.llHardSize > 0 && llStoreSize >= quotadetails.llHardSize)
		*lpQuotaStatus = QUOTA_HARDLIMIT;
	else if(quotadetails.llSoftSize > 0 && llStoreSize >= quotadetails.llSoftSize)
		*lpQuotaStatus = QUOTA_SOFTLIMIT;
	else if(quotadetails.llWarnSize > 0 && llStoreSize >= quotadetails.llWarnSize)
		*lpQuotaStatus = QUOTA_WARN;
	else
		*lpQuotaStatus = QUOTA_OK;

exit:
	return er;
}

/** 
 * Get the quota details of a user
 * 
 * @param[in] ulUserId internal user id
 * @param[out] lpDetails quota details
 * 
 * @return Zarafa error code
 */
ECRESULT ECSecurity::GetUserQuota(unsigned int ulUserId, bool bGetUserDefault, quotadetails_t *lpDetails)
{
	ECRESULT er = erSuccess;
	char* lpszWarnQuota = NULL;
	char* lpszSoftQuota = NULL;
	char* lpszHardQuota = NULL;
	quotadetails_t quotadetails;
	objectid_t sExternId;
	unsigned int ulCompanyId;

	if (!lpDetails) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	er = m_lpSession->GetUserManagement()->GetExternalId(ulUserId, &sExternId, &ulCompanyId);
	if (er != erSuccess)
		goto exit;

	ASSERT(!bGetUserDefault || sExternId.objclass == CONTAINER_COMPANY);

	/* Not all objectclasses support quota */
	if ((sExternId.objclass == NONACTIVE_CONTACT) ||
		(OBJECTCLASS_TYPE(sExternId.objclass) == OBJECTTYPE_DISTLIST) ||
		(sExternId.objclass == CONTAINER_ADDRESSLIST))
			goto exit;

	er = m_lpSession->GetUserManagement()->GetQuotaDetailsAndSync(ulUserId, &quotadetails, bGetUserDefault);
	if (er != erSuccess)
		goto exit;

	/* When the default quota boolean is set, we need to look at the next quota level */
	if (!quotadetails.bUseDefaultQuota)
		goto exit;

	/* Request default quota values from company level if enabled */
	if ((OBJECTCLASS_TYPE(sExternId.objclass) == OBJECTTYPE_MAILUSER) && ulCompanyId) {
		er = m_lpSession->GetUserManagement()->GetQuotaDetailsAndSync(ulCompanyId, &quotadetails, true);
		if (er == erSuccess && !quotadetails.bUseDefaultQuota)
			goto exit; /* On failure, or when we should use the default, we're done */

		er = erSuccess;
	}

	/* No information from company, the last level we can check is the configuration file */
	if (OBJECTCLASS_TYPE(sExternId.objclass) == OBJECTTYPE_MAILUSER) {
		lpszWarnQuota = m_lpSession->GetSessionManager()->GetConfig()->GetSetting("quota_warn");
		lpszSoftQuota = m_lpSession->GetSessionManager()->GetConfig()->GetSetting("quota_soft");
		lpszHardQuota = m_lpSession->GetSessionManager()->GetConfig()->GetSetting("quota_hard");
	} else if (sExternId.objclass == CONTAINER_COMPANY) {
		lpszWarnQuota = m_lpSession->GetSessionManager()->GetConfig()->GetSetting("companyquota_warn");
	}

	quotadetails.bUseDefaultQuota = true;
	quotadetails.bIsUserDefaultQuota = false;
	if (lpszWarnQuota)
		quotadetails.llWarnSize = _atoi64(lpszWarnQuota) * 1024 * 1024;
	if (lpszSoftQuota)
		quotadetails.llSoftSize = _atoi64(lpszSoftQuota) * 1024 * 1024;
	if (lpszHardQuota)
		quotadetails.llHardSize = _atoi64(lpszHardQuota) * 1024 * 1024;

exit:
	if (er == erSuccess)
		*lpDetails = quotadetails;

	return er;
}

/** 
 * Get the username of the current user context
 * 
 * @param[out] lpstrUsername login name of the user
 * 
 * @return always erSuccess
 */
ECRESULT ECSecurity::GetUsername(std::string *lpstrUsername)
{
	if (m_ulUserID)
		*lpstrUsername = m_details.GetPropString(OB_PROP_S_LOGIN);
	else
		*lpstrUsername = ZARAFA_SYSTEM_USER;
	return erSuccess;
}

/**
 * Get memory size of this object
 *
 * @return Object size in bytes
 */
unsigned int ECSecurity::GetObjectSize()
{
	unsigned int ulSize = sizeof(*this);

	list<localobjectdetails_t>::iterator iter;

	ulSize += m_details.GetObjectSize();

	if (m_lpGroups) {
		for (iter = m_lpGroups->begin(); iter != m_lpGroups->end(); iter++)
			ulSize += iter->GetObjectSize();
	}

	if (m_lpViewCompanies)
	{
		for (iter = m_lpViewCompanies->begin(); iter != m_lpViewCompanies->end(); iter++)
			ulSize += iter->GetObjectSize();
	}

	if (m_lpAdminCompanies)
	{
		for (iter = m_lpAdminCompanies->begin(); iter != m_lpAdminCompanies->end(); iter++)
			ulSize += iter->GetObjectSize();
	}

	return ulSize;
}

