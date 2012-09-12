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

#include <mapitags.h>
#include <mapidefs.h>
#include <mapiutil.h>

#include <libintl.h>

#include "ECMAPI.h"
#include "stringutil.h"
#include "SOAPUtils.h"
#include "soapH.h"
#include "ECGenProps.h"
#include "Zarafa.h"
#include "ECDefs.h"
#include "ECUserManagement.h"
#include "ECSecurity.h"
#include "ECSessionManager.h"
#include "ECLockManager.h"

#include <edkmdb.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define _(string) dcgettext("zarafa", string, LC_MESSAGES)

extern ECSessionManager*	g_lpSessionManager;

ECGenProps::ECGenProps()
{
	// Nothing to do
}

ECGenProps::~ECGenProps()
{
	// Nothing to do
}

ECRESULT ECGenProps::GetMVPropSubquery(unsigned int ulPropTagRequested, std::string &subquery) 
{
	ECRESULT er = erSuccess;
	unsigned int ulType = PROP_TYPE(ulPropTagRequested);

	//skip MV_INSTANCE
	switch(ulType) {
		case PT_MV_I2:
		case PT_MV_LONG:
			subquery = "SELECT concat(count(*), ':', group_concat(length(val_ulong),':',val_ulong ORDER BY subquery.orderid SEPARATOR '')) FROM mvproperties AS subquery WHERE subquery.hierarchyid=hierarchy.id AND subquery.type="+stringify(PROP_TYPE(ulPropTagRequested))+" AND subquery.tag="+stringify(PROP_ID(ulPropTagRequested))+" GROUP BY subquery.hierarchyid";
			break;
		case PT_MV_R4:
		case PT_MV_DOUBLE:
		case PT_MV_APPTIME:
			subquery = "SELECT concat(count(*), ':', group_concat(length(val_double),':',val_double ORDER BY subquery.orderid SEPARATOR '')) FROM mvproperties AS subquery WHERE subquery.hierarchyid=hierarchy.id AND subquery.type="+stringify(PROP_TYPE(ulPropTagRequested))+" AND subquery.tag="+stringify(PROP_ID(ulPropTagRequested))+" GROUP BY subquery.hierarchyid";
			break;
		case PT_MV_CURRENCY:
		case PT_MV_SYSTIME:
			subquery = "SELECT group_concat(count(*),':',length(val_hi),':',val_hi ORDER BY subquery.orderid SEPARATOR '')) FROM mvproperties AS subquery WHERE subquery.hierarchyid=hierarchy.id AND subquery.type="+stringify(PROP_TYPE(ulPropTagRequested))+" AND subquery.tag="+stringify(PROP_ID(ulPropTagRequested))+" GROUP BY subquery.hierarchyid";
			subquery += "),(SELECT group_concat(count(*),':',length(val_lo),':',val_lo ORDER BY subquery.orderid SEPARATOR '')) FROM mvproperties AS subquery WHERE subquery.hierarchyid=hierarchy.id AND subquery.type="+stringify(PROP_TYPE(ulPropTagRequested))+" AND subquery.tag="+stringify(PROP_ID(ulPropTagRequested))+" GROUP BY subquery.hierarchyid";
			break;
		case PT_MV_BINARY:
		case PT_MV_CLSID:
			subquery = "SELECT concat(count(*), ':', group_concat(length(val_binary),':',val_binary ORDER BY subquery.orderid SEPARATOR '')) FROM mvproperties AS subquery WHERE subquery.hierarchyid=hierarchy.id AND subquery.type="+stringify(PROP_TYPE(ulPropTagRequested))+" AND subquery.tag="+stringify(PROP_ID(ulPropTagRequested))+" GROUP BY subquery.hierarchyid";
			break;
		case PT_MV_STRING8:
		case PT_MV_UNICODE:
			subquery = "SELECT concat(count(*), ':', group_concat(char_length(val_string),':',val_string ORDER BY subquery.orderid SEPARATOR '')) FROM mvproperties AS subquery WHERE subquery.hierarchyid=hierarchy.id AND subquery.type="+stringify(PROP_TYPE(ulPropTagRequested))+" AND subquery.tag="+stringify(PROP_ID(ulPropTagRequested))+" GROUP BY subquery.hierarchyid";
			break;
		case PT_MV_I8:
			subquery = "SELECT concat(count(*), ':', group_concat(length(val_longint),':',val_longint ORDER BY subquery.orderid SEPARATOR '')) FROM mvproperties AS subquery WHERE subquery.hierarchyid=hierarchy.id AND subquery.type="+stringify(PROP_TYPE(ulPropTagRequested))+" AND subquery.tag="+stringify(PROP_ID(ulPropTagRequested))+" GROUP BY subquery.hierarchyid";
			break;
		default:
			er = ZARAFA_E_NOT_FOUND;
			break;
	}
	

	return er;
}

ECRESULT ECGenProps::GetPropSubquery(unsigned int ulPropTagRequested, std::string &subquery) 
{
	ECRESULT er = erSuccess;

	switch(ulPropTagRequested) {
	case PR_PARENT_DISPLAY_W:
	case PR_PARENT_DISPLAY_A:
		subquery = "SELECT properties.val_string FROM properties JOIN hierarchy as subquery ON properties.hierarchyid=subquery.parent WHERE subquery.id=hierarchy.id AND properties.tag=0x3001"; // PR_DISPLAY_NAME of parent
		break;

    case PR_EC_OUTGOING_FLAGS:
        subquery = "SELECT outgoingqueue.flags FROM outgoingqueue where outgoingqueue.hierarchy_id = hierarchy.id and outgoingqueue.flags & 1 = 1";
        break;

	default:
		er = ZARAFA_E_NOT_FOUND;
		break;
	}

	return er;
}

/**
 * Get a property substitution
 *
 * This is used in tables; A substitition works as follows:
 *
 * - In the table engine, any column with the requested property tag is replaced with the required property
 * - The requested property is retrieved from cache or database as if the column had been retrieved as such in the first place
 * - You must create a GetPropComputed entry to convert back to the originally requested property
 *
 * @param ulObjType MAPI_MESSAGE, or MAPI_FOLDER
 * @param ulPropTagRequested The property tag set by SetColumns
 * @param ulPropTagRequired[out] Output property to be retrieved from the database
 * @return Result
 */
ECRESULT ECGenProps::GetPropSubstitute(unsigned int ulObjType, unsigned int ulPropTagRequested, unsigned int *lpulPropTagRequired)
{
	ECRESULT er = erSuccess;

	unsigned int ulPropTagRequired = 0;

	switch(PROP_ID(ulPropTagRequested)) {
		case PROP_ID(PR_NORMALIZED_SUBJECT):
			ulPropTagRequired = PR_SUBJECT;
			break;
		case PROP_ID(PR_CONTENT_UNREAD):
			if(ulObjType == MAPI_MESSAGE)
				ulPropTagRequired = PR_MESSAGE_FLAGS;
			else
				er = ZARAFA_E_NOT_FOUND;
			break;
		default:
			er = ZARAFA_E_NOT_FOUND;
			goto exit;
	}

	*lpulPropTagRequired = ulPropTagRequired;

exit:
	return er;
}

// This should be synchronized with GetPropComputed
ECRESULT ECGenProps::IsPropComputed(unsigned int ulPropTag, unsigned int ulObjType)
{
	ECRESULT		er = erSuccess;

	switch(ulPropTag) {
		case PR_MSG_STATUS:
		case PR_EC_IMAP_ID:
		case PR_NORMALIZED_SUBJECT_A:
		case PR_NORMALIZED_SUBJECT_W:
		case PR_SUBMIT_FLAGS:
		    er = erSuccess;
		    break;
		case PR_CONTENT_UNREAD:
			if(ulObjType == MAPI_MESSAGE)
				er = erSuccess;
			else
				er = ZARAFA_E_NOT_FOUND;
			break;
		case PR_RECORD_KEY:
			if (ulObjType == MAPI_ATTACH)
				er = ZARAFA_E_NOT_FOUND;
			else
				er = erSuccess;
			break;
		default:
			er = ZARAFA_E_NOT_FOUND;
			break;
	}

	return er;
}

// This should be synchronized with GetPropComputedUncached
ECRESULT ECGenProps::IsPropComputedUncached(unsigned int ulPropTag, unsigned int ulObjType)
{
    ECRESULT er = erSuccess;
    
    switch(PROP_ID(ulPropTag)) {
        case PROP_ID(PR_LONGTERM_ENTRYID_FROM_TABLE):
		case PROP_ID(PR_ENTRYID):
		case PROP_ID(PR_PARENT_ENTRYID): 
		case PROP_ID(PR_STORE_ENTRYID):
		case PROP_ID(PR_USER_NAME):
		case PROP_ID(PR_MAILBOX_OWNER_NAME):
		case PROP_ID(PR_USER_ENTRYID):
		case PROP_ID(PR_MAILBOX_OWNER_ENTRYID):
		case PROP_ID(PR_EC_MAILBOX_OWNER_ACCOUNT):
		case PROP_ID(PR_EC_HIERARCHYID):
		case PROP_ID(PR_INSTANCE_KEY):
		case PROP_ID(PR_OBJECT_TYPE):
		case PROP_ID(PR_SOURCE_KEY):
		case PROP_ID(PR_PARENT_SOURCE_KEY):
		case PROP_ID(PR_RIGHTS):
		case PROP_ID(PR_ACCESS_LEVEL):
		case PROP_ID(PR_ACCESS):
		case PROP_ID(PR_ROW_TYPE):
		case PROP_ID(PR_MAPPING_SIGNATURE):
		    er = erSuccess;
		    break;
		case PROP_ID(PR_RECORD_KEY):
			if (ulObjType == MAPI_ATTACH)
				er = ZARAFA_E_NOT_FOUND;
			else
				er = erSuccess;
			break;
		case PROP_ID(PR_DISPLAY_NAME): // only the store property is generated
		case PROP_ID(PR_EC_DELETED_STORE):
		    if(ulObjType == MAPI_STORE)
    			er = erSuccess;
            else
                er = ZARAFA_E_NOT_FOUND;
			break;
		case PROP_ID(PR_CONTENT_COUNT):
			if (ulObjType == MAPI_MESSAGE)
				er = erSuccess;
			else
				er = ZARAFA_E_NOT_FOUND;
			break;
        default:
            er = ZARAFA_E_NOT_FOUND;
            break;
    }
    
    return er;
}

// These are properties that are never written to the 'properties' table; ie they are never directly queried. This
// is not the same as the generated properties, as they may access data in the database to *create* a generated
// property. 
ECRESULT ECGenProps::IsPropRedundant(unsigned int ulPropTag, unsigned int ulObjType)
{
    ECRESULT er = erSuccess;
    
    switch(PROP_ID(ulPropTag)) {
		case PROP_ID(PR_ACCESS):					// generated from ACLs
		case PROP_ID(PR_USER_NAME):				// generated from owner (hierarchy)
		case PROP_ID(PR_MAILBOX_OWNER_NAME):		// generated from owner (hierarchy)
		case PROP_ID(PR_USER_ENTRYID):			// generated from owner (hierarchy)
		case PROP_ID(PR_MAILBOX_OWNER_ENTRYID):	// generated from owner (hierarchy)
		case PROP_ID(PR_EC_MAILBOX_OWNER_ACCOUNT): // generated from owner (hierarchy)
		case PROP_ID(PR_EC_HIERARCHYID):			// generated from hierarchy
		case PROP_ID(PR_SUBFOLDERS):				// generated from hierarchy
		case PROP_ID(PR_HASATTACH):				// generated from hierarchy
		case PROP_ID(PR_LONGTERM_ENTRYID_FROM_TABLE): // generated from hierarchy
		case PROP_ID(PR_ENTRYID):				// generated from hierarchy
		case PROP_ID(PR_PARENT_ENTRYID): 		// generated from hierarchy
		case PROP_ID(PR_STORE_ENTRYID):			// generated from store id
		case PROP_ID(PR_INSTANCE_KEY):			// table data only
		case PROP_ID(PR_OBJECT_TYPE):			// generated from hierarchy
		case PROP_ID(PR_CONTENT_COUNT):			// generated from hierarchy
		case PROP_ID(PR_CONTENT_UNREAD):			// generated from hierarchy
		case PROP_ID(PR_RIGHTS):					// generated from security system
		case PROP_ID(PR_ACCESS_LEVEL):			// generated from security system
		case PROP_ID(PR_PARENT_SOURCE_KEY):		// generated from ics system
		case PROP_ID(PR_FOLDER_TYPE):			// generated from hierarchy (CreateFolder)
		case PROP_ID(PR_EC_IMAP_ID):				// generated for each new mail and updated on move by the server
		    er = erSuccess;
		    break;
		case PROP_ID(PR_RECORD_KEY):				// generated from hierarchy except for attachments
			if (ulObjType == MAPI_ATTACH)
				er = ZARAFA_E_NOT_FOUND;
			else
				er = erSuccess;
			break;
		default:
			er = ZARAFA_E_NOT_FOUND;
			break;
    }
    
    return er;
}

ECRESULT ECGenProps::GetPropComputed(struct soap *soap, unsigned int ulObjType, unsigned int ulPropTagRequested, unsigned int ulObjId, struct propVal *lpPropVal)
{
	ECRESULT		er = erSuccess;
	char*			lpszColon = NULL;

	switch(PROP_ID(ulPropTagRequested)) {
	case PROP_ID(PR_MSG_STATUS):
		if(lpPropVal->ulPropTag != ulPropTagRequested) {
			lpPropVal->ulPropTag = PR_MSG_STATUS;
			lpPropVal->__union = SOAP_UNION_propValData_ul;

			lpPropVal->Value.ul = 0;
		} else {
			er = ZARAFA_E_NOT_FOUND;
		}
		break;
    case PROP_ID(PR_EC_IMAP_ID):
    	if(lpPropVal->ulPropTag != ulPropTagRequested) {
			lpPropVal->ulPropTag = PR_EC_IMAP_ID;
			lpPropVal->__union = SOAP_UNION_propValData_ul;
			lpPropVal->Value.ul = ulObjId;
		} else {
			er = ZARAFA_E_NOT_FOUND;
		}
		break;
	case PROP_ID(PR_CONTENT_UNREAD):
		// Convert from PR_MESSAGE_FLAGS to PR_CONTENT_UNREAD
		if(ulObjType == MAPI_MESSAGE && lpPropVal->ulPropTag != PR_CONTENT_UNREAD) {
			lpPropVal->ulPropTag = PR_CONTENT_UNREAD;
			lpPropVal->__union = SOAP_UNION_propValData_ul;
			lpPropVal->Value.ul = lpPropVal->Value.ul & MSGFLAG_READ ? 0 : 1;
		} else {
			er = ZARAFA_E_NOT_FOUND;
		}
		break;
    case PROP_ID(PR_NORMALIZED_SUBJECT):
    	if(lpPropVal->ulPropTag != PR_SUBJECT) {
    		lpPropVal->ulPropTag = PROP_TAG(PT_ERROR, PROP_ID(PR_NORMALIZED_SUBJECT));
    		lpPropVal->Value.ul = ZARAFA_E_NOT_FOUND;
    		lpPropVal->__union = SOAP_UNION_propValData_ul;
    	} else {
    		lpPropVal->ulPropTag = ulPropTagRequested;
    		
			lpszColon = strchr(lpPropVal->Value.lpszA, ':');
			if(lpszColon != NULL && (lpszColon - lpPropVal->Value.lpszA) >1 && (lpszColon - lpPropVal->Value.lpszA) < 4)
			{
				lpszColon++; // skip ':'
				if(strlen(lpszColon)+1 <= strlen(lpPropVal->Value.lpszA) && lpszColon[0] == ' ')
					lpszColon++;// skip space

				lpPropVal->Value.lpszA = s_alloc<char>(soap, strlen(lpszColon)+1);

				strcpy(lpPropVal->Value.lpszA, lpszColon);
			}
		}
        break;
	case PROP_ID(PR_SUBMIT_FLAGS):
		if (ulObjType == MAPI_MESSAGE) {
			if (g_lpSessionManager->GetLockManager()->IsLocked(ulObjId, NULL))
				lpPropVal->Value.ul |= SUBMITFLAG_LOCKED;
			else
				lpPropVal->Value.ul &= ~SUBMITFLAG_LOCKED;
		}
		break;
	case PROP_ID(PR_RECORD_KEY):
		if (ulObjType == MAPI_ATTACH && lpPropVal->ulPropTag != ulPropTagRequested) {
			lpPropVal->ulPropTag = PR_RECORD_KEY;
			lpPropVal->__union = SOAP_UNION_propValData_bin;
			
			lpPropVal->Value.bin = s_alloc<struct xsd__base64Binary>(soap);
			lpPropVal->Value.bin->__ptr = s_alloc<unsigned char>(soap, sizeof(ULONG));
			
			lpPropVal->Value.bin->__size = sizeof(ULONG);
			memcpy(lpPropVal->Value.bin->__ptr, &ulObjId, sizeof(ULONG));
		} else
			er = ZARAFA_E_NOT_FOUND;
		break;
	default:
		er = ZARAFA_E_NOT_FOUND;
		goto exit;
	}

exit:

	return er;
}

// All in memory properties
ECRESULT ECGenProps::GetPropComputedUncached(struct soap *soap, ECSession* lpSession, unsigned int ulPropTag, unsigned int ulObjId, unsigned int ulOrderId, unsigned int ulStoreId, unsigned int ulParentId, unsigned int ulObjType, struct propVal *lpPropVal)
{
	ECRESULT		er = erSuccess;
	unsigned int	ulRights = 0;
	bool			bOwner = false;
	bool			bAdmin = false;
	unsigned int	ulFlags = 0;
	unsigned int	ulUserId = 0;
	char*			lpStoreName = NULL;
	struct propVal	sPropVal = {0};

	struct propValArray sPropValArray = {0, 0};
	struct propTagArray sPropTagArray = {0, 0};

	switch(PROP_ID(ulPropTag)) {
		case PROP_ID(PR_LONGTERM_ENTRYID_FROM_TABLE):
		case PROP_ID(PR_ENTRYID):
		case PROP_ID(PR_PARENT_ENTRYID): 
		case PROP_ID(PR_STORE_ENTRYID):
		{
			entryId sEntryId;

			if (ulPropTag == PR_PARENT_ENTRYID) {
				if(ulParentId == 0) {
					er = lpSession->GetSessionManager()->GetCacheManager()->GetParent(ulObjId, &ulParentId);
					if(er != erSuccess)
						goto exit;
				}

                er = lpSession->GetSessionManager()->GetCacheManager()->GetObject(ulParentId, NULL, NULL, &ulFlags, &ulObjType);
                if(er != erSuccess)
                    goto exit;

                if(ulObjType == MAPI_FOLDER) {
                    ulObjId = ulParentId;
                } // else PR_PARENT_ENTRYID == PR_ENTRYID

                        // The following line is merged from trunk@35290, where a bit more work
                        // is performed within the next block. So this change is a bit useless except
                        // when we decide to merge those changes later for some reason.					
			}else if (ulPropTag == PR_STORE_ENTRYID || ulObjId == ulStoreId) {
				ulObjId = ulStoreId;
			}

			er = lpSession->GetSessionManager()->GetCacheManager()->GetEntryIdFromObject(ulObjId, soap, &sEntryId);
			if(er != erSuccess) {
				// happens on recipients, attachments and msg-in-msg .. TODO: add strict type checking?
				//ASSERT(FALSE);
				er = ZARAFA_E_NOT_FOUND;
				goto exit;
			}
			sPropVal.ulPropTag = ulPropTag;

			sPropVal.__union = SOAP_UNION_propValData_bin;
			sPropVal.Value.bin = s_alloc<struct xsd__base64Binary>(soap);

			sPropVal.Value.bin->__ptr = sEntryId.__ptr;
			sPropVal.Value.bin->__size = sEntryId.__size;

			break;
		}
		case PROP_ID(PR_USER_ENTRYID):
			sPropTagArray.__ptr = new unsigned int[1];
			sPropTagArray.__ptr[0] = PR_ENTRYID;
			sPropTagArray.__size = 1;

			ulUserId = lpSession->GetSecurity()->GetUserId();
			if(	lpSession->GetUserManagement()->GetProps(soap, ulUserId, &sPropTagArray, &sPropValArray) == erSuccess &&
				sPropValArray.__ptr && sPropValArray.__ptr[0].ulPropTag == PR_ENTRYID)
			{
				sPropVal.__union = sPropValArray.__ptr[0].__union;
				sPropVal.ulPropTag = PR_USER_ENTRYID;
				sPropVal.Value.bin = sPropValArray.__ptr[0].Value.bin; // memory is allocated in GetUserData(..)
			}else{
				er = ZARAFA_E_NOT_FOUND;
				goto exit;
			}
			break;
		case PROP_ID(PR_USER_NAME):
			sPropTagArray.__ptr = new unsigned int[1];
			sPropTagArray.__ptr[0] = PR_ACCOUNT;
			sPropTagArray.__size = 1;

			ulUserId = lpSession->GetSecurity()->GetUserId();
			if(	lpSession->GetUserManagement()->GetProps(soap, ulUserId, &sPropTagArray, &sPropValArray) == erSuccess &&
				sPropValArray.__ptr && sPropValArray.__ptr[0].ulPropTag == PR_ACCOUNT)
			{
				sPropVal.__union = sPropValArray.__ptr[0].__union;
				sPropVal.ulPropTag = CHANGE_PROP_TYPE(PR_USER_NAME, (PROP_TYPE(ulPropTag)));
				sPropVal.Value.lpszA = sPropValArray.__ptr[0].Value.lpszA;// memory is allocated in GetUserData(..)
			}else{
				er = ZARAFA_E_NOT_FOUND;
				goto exit;
			}
			break;
		case PROP_ID(PR_DISPLAY_NAME):
		{
			unsigned int ulStoreType = 0;

			if(ulObjType != MAPI_STORE) {
			    er = ZARAFA_E_NOT_FOUND;
			    goto exit;
	        }

			er = lpSession->GetSessionManager()->GetCacheManager()->GetStoreAndType(ulObjId, NULL, NULL, &ulStoreType);
			if (er != erSuccess)
				goto exit;
        
			er = GetStoreName(soap, lpSession, ulObjId, ulStoreType, &lpStoreName);
			if(er != erSuccess)
				goto exit;
		
			sPropVal.__union = SOAP_UNION_propValData_lpszA;
			sPropVal.ulPropTag = CHANGE_PROP_TYPE(PR_DISPLAY_NAME, (PROP_TYPE(ulPropTag)));
			sPropVal.Value.lpszA = lpStoreName;
		}
		break;
		case PROP_ID(PR_MAILBOX_OWNER_NAME):
			sPropTagArray.__ptr = new unsigned int[1];
			sPropTagArray.__ptr[0] = PR_DISPLAY_NAME;
			sPropTagArray.__size = 1;

			if(lpSession->GetSecurity()->GetStoreOwner(ulStoreId, &ulUserId) == erSuccess &&
				lpSession->GetUserManagement()->GetProps(soap, ulUserId, &sPropTagArray, &sPropValArray) == erSuccess &&
				sPropValArray.__ptr && sPropValArray.__ptr[0].ulPropTag == PR_DISPLAY_NAME)
			{
				sPropVal.__union = sPropValArray.__ptr[0].__union;
				sPropVal.ulPropTag = CHANGE_PROP_TYPE(PR_MAILBOX_OWNER_NAME, (PROP_TYPE(ulPropTag)));
				sPropVal.Value.lpszA = sPropValArray.__ptr[0].Value.lpszA; // memory is allocated in GetUserData(..)
			}else{
				er = ZARAFA_E_NOT_FOUND;
				goto exit;
			}
		break;
		case PROP_ID(PR_MAILBOX_OWNER_ENTRYID):
			sPropTagArray.__ptr = new unsigned int[1];
			sPropTagArray.__ptr[0] = PR_ENTRYID;
			sPropTagArray.__size = 1;

			if(lpSession->GetSecurity()->GetStoreOwner(ulStoreId, &ulUserId) == erSuccess &&
				lpSession->GetUserManagement()->GetProps(soap, ulUserId, &sPropTagArray, &sPropValArray) == erSuccess &&
				sPropValArray.__ptr && sPropValArray.__ptr[0].ulPropTag == PR_ENTRYID)
			{
				sPropVal.__union = sPropValArray.__ptr[0].__union;
				sPropVal.ulPropTag = PR_MAILBOX_OWNER_ENTRYID;
				sPropVal.Value.bin = sPropValArray.__ptr[0].Value.bin;// memory is allocated in GetUserData(..)
			}else{
				er = ZARAFA_E_NOT_FOUND;
				goto exit;
			}
			break;
		case PROP_ID(PR_EC_MAILBOX_OWNER_ACCOUNT):
			sPropTagArray.__ptr = new unsigned int[1];
			sPropTagArray.__ptr[0] = PR_ACCOUNT;
			sPropTagArray.__size = 1;

			if (lpSession->GetSecurity()->GetStoreOwner(ulStoreId, &ulUserId) == erSuccess &&
				lpSession->GetUserManagement()->GetProps(soap, ulUserId, &sPropTagArray, &sPropValArray) == erSuccess &&
				sPropValArray.__ptr && sPropValArray.__ptr[0].ulPropTag == PR_ACCOUNT) {

				sPropVal.__union = sPropValArray.__ptr[0].__union;
				sPropVal.ulPropTag = CHANGE_PROP_TYPE(PR_EC_MAILBOX_OWNER_ACCOUNT, (PROP_TYPE(ulPropTag)));
				sPropVal.Value.lpszA = sPropValArray.__ptr[0].Value.lpszA; // memory is allocated in GetUserData(..)
			} else {
				er = ZARAFA_E_NOT_FOUND;
				goto exit;
			}
			break;
        case PROP_ID(PR_EC_HIERARCHYID):
			sPropVal.ulPropTag = ulPropTag;
			sPropVal.__union = SOAP_UNION_propValData_ul;
			
			sPropVal.Value.ul = ulObjId;
            break;
		case PROP_ID(PR_INSTANCE_KEY):
			sPropVal.ulPropTag = ulPropTag;
			sPropVal.__union = SOAP_UNION_propValData_bin;
			
			sPropVal.Value.bin = s_alloc<struct xsd__base64Binary>(soap);
			sPropVal.Value.bin->__ptr = s_alloc<unsigned char>(soap, sizeof(ULONG)*2);
			
			sPropVal.Value.bin->__size = sizeof(ULONG)*2;
			memcpy(sPropVal.Value.bin->__ptr, &ulObjId, sizeof(ULONG));
			memcpy(sPropVal.Value.bin->__ptr+sizeof(ULONG), &ulOrderId, sizeof(ULONG));
			break;
		case PROP_ID(PR_RECORD_KEY):
			sPropVal.ulPropTag = ulPropTag;
			sPropVal.__union = SOAP_UNION_propValData_bin;
			
			sPropVal.Value.bin = s_alloc<struct xsd__base64Binary>(soap);
			sPropVal.Value.bin->__ptr = s_alloc<unsigned char>(soap, sizeof(ULONG));
			
			sPropVal.Value.bin->__size = sizeof(ULONG);
			memcpy(sPropVal.Value.bin->__ptr, &ulObjId, sizeof(ULONG));
			break;
		case PROP_ID(PR_OBJECT_TYPE):
			sPropVal.ulPropTag = PR_OBJECT_TYPE;
			sPropVal.__union = SOAP_UNION_propValData_ul;
			sPropVal.Value.ul = ulObjType;
			break;
		case PROP_ID(PR_SOURCE_KEY):
			sPropVal.ulPropTag = PR_SOURCE_KEY;
			sPropVal.__union = SOAP_UNION_propValData_bin;
			sPropVal.Value.bin = s_alloc<struct xsd__base64Binary>(soap);
			sPropVal.Value.bin->__size = 0;
			sPropVal.Value.bin->__ptr = NULL;

			er = lpSession->GetSessionManager()->GetCacheManager()->GetPropFromObject(PROP_ID(PR_SOURCE_KEY), ulObjId, soap, (unsigned int*)&sPropVal.Value.bin->__size, &sPropVal.Value.bin->__ptr);
			break;
		case PROP_ID(PR_PARENT_SOURCE_KEY):
			sPropVal.ulPropTag = PR_PARENT_SOURCE_KEY;
			sPropVal.__union = SOAP_UNION_propValData_bin;
			sPropVal.Value.bin = s_alloc<struct xsd__base64Binary>(soap);
			sPropVal.Value.bin->__size = 0;
			sPropVal.Value.bin->__ptr = NULL;

			if(ulParentId == 0) {
				er = lpSession->GetSessionManager()->GetCacheManager()->GetObject(ulObjId, &ulParentId, NULL, NULL, NULL);
				if(er != erSuccess)
					goto exit;
			}
			
			er = lpSession->GetSessionManager()->GetCacheManager()->GetPropFromObject(PROP_ID(PR_SOURCE_KEY), ulParentId, soap, (unsigned int*)&sPropVal.Value.bin->__size, &sPropVal.Value.bin->__ptr);
			if(er != erSuccess)
				goto exit;

			break;
		case PROP_ID(PR_CONTENT_COUNT):
			if(ulObjType == MAPI_MESSAGE) {
				sPropVal.ulPropTag = ulPropTag;
				sPropVal.__union = SOAP_UNION_propValData_ul;
				sPropVal.Value.ul = 1;
			} else {
				er = ZARAFA_E_NOT_FOUND;
				goto exit;
			}
			break;
		case PROP_ID(PR_RIGHTS):
			if(ulObjType != MAPI_FOLDER)
			{
				er = ZARAFA_E_NOT_FOUND;
				goto exit;
			}

			sPropVal.ulPropTag = PR_RIGHTS;
			sPropVal.__union = SOAP_UNION_propValData_ul;
		
			if(lpSession->GetSecurity()->IsStoreOwner(ulObjId) == erSuccess || lpSession->GetSecurity()->IsAdminOverOwnerOfObject(ulObjId) == erSuccess)
				sPropVal.Value.ul = ecRightsAll;
			else if(lpSession->GetSecurity()->GetObjectPermission(ulObjId, &ulRights) == hrSuccess)
				sPropVal.Value.ul = ulRights;
			else
				sPropVal.Value.ul = 0;

			break;
		case PROP_ID(PR_ACCESS):
			if(ulObjType == MAPI_STORE || ulObjType == MAPI_ATTACH)
			{
				er = ZARAFA_E_NOT_FOUND;
				goto exit;
			}

			sPropVal.ulPropTag = PR_ACCESS;
			sPropVal.__union = SOAP_UNION_propValData_ul;
			sPropVal.Value.ul = 0;

			// Optimize: for a message, the rights are equal to that of the parent. It is more efficient for
			// the cache to check the folder permissions than the message permissions
			if (ulObjType == MAPI_MESSAGE && ulParentId)
				ulObjId = ulParentId;

			// if the requested object is from the owners store, return all permissions	
			if (lpSession->GetSecurity()->IsStoreOwner(ulObjId) == erSuccess ||
				lpSession->GetSecurity()->IsAdminOverOwnerOfObject(ulObjId) == erSuccess) {
				switch(ulObjType) {
					case MAPI_FOLDER:
						sPropVal.Value.ul = MAPI_ACCESS_READ | MAPI_ACCESS_MODIFY | MAPI_ACCESS_DELETE;
						if(ulFlags != FOLDER_SEARCH) //FOLDER_GENERIC, FOLDER_ROOT 
							sPropVal.Value.ul |= MAPI_ACCESS_CREATE_HIERARCHY | MAPI_ACCESS_CREATE_CONTENTS | MAPI_ACCESS_CREATE_ASSOCIATED;
						break;
					case MAPI_MESSAGE:
						sPropVal.Value.ul = MAPI_ACCESS_READ | MAPI_ACCESS_MODIFY | MAPI_ACCESS_DELETE;
						break;
					case MAPI_ATTACH:
					case MAPI_STORE:
					default:
						er = ZARAFA_E_NOT_FOUND;
						goto exit;
				}

				break;
			}

			// someone else is accessing your store, so check their rights
			ulRights = 0;

			lpSession->GetSecurity()->GetObjectPermission(ulObjId, &ulRights);// skip error checking, ulRights = 0

			// will be false when someone else created this object in this store (or true if you're that someone)
			bOwner = (lpSession->GetSecurity()->IsOwner(ulObjId) == erSuccess);

			switch(ulObjType) {
				case MAPI_FOLDER:
					if( (ulRights&ecRightsReadAny)==ecRightsReadAny)
						sPropVal.Value.ul |= MAPI_ACCESS_READ;

					if( bOwner == true || (ulRights&ecRightsFolderAccess) == ecRightsFolderAccess)
						sPropVal.Value.ul |= MAPI_ACCESS_DELETE | MAPI_ACCESS_MODIFY;
					
					if(ulFlags != FOLDER_SEARCH) //FOLDER_GENERIC, FOLDER_ROOT 
					{
						if( (ulRights&ecRightsCreateSubfolder) == ecRightsCreateSubfolder)
							sPropVal.Value.ul |= MAPI_ACCESS_CREATE_HIERARCHY;
							
						if( (ulRights&ecRightsCreate) == ecRightsCreate )
							sPropVal.Value.ul |= MAPI_ACCESS_CREATE_CONTENTS;	
							
						if( (ulRights&ecRightsFolderAccess) == ecRightsFolderAccess)
							sPropVal.Value.ul |= MAPI_ACCESS_CREATE_ASSOCIATED;
					}

					break;
				case MAPI_MESSAGE:
					if( (ulRights&ecRightsReadAny)==ecRightsReadAny)
						sPropVal.Value.ul |= MAPI_ACCESS_READ;

					if( (ulRights&ecRightsEditAny)==ecRightsEditAny || (bOwner == true && (ulRights&ecRightsEditOwned) == ecRightsEditOwned) )
						sPropVal.Value.ul |= MAPI_ACCESS_MODIFY;

					if( (ulRights&ecRightsDeleteAny) == ecRightsDeleteAny || (bOwner == true && (ulRights&ecRightsDeleteOwned) == ecRightsDeleteOwned) )
						sPropVal.Value.ul |= MAPI_ACCESS_DELETE;

					break;
				case MAPI_ATTACH:
				case MAPI_STORE:
				default:
					er = ZARAFA_E_NOT_FOUND;
					goto exit;
			}
			break;
		case PROP_ID(PR_ACCESS_LEVEL):
			{
				sPropVal.ulPropTag = PR_ACCESS_LEVEL;
				sPropVal.__union = SOAP_UNION_propValData_ul;
				sPropVal.Value.ul = 0;

				ulRights = 0;

				// @todo if store only open with read rights, access level = 0
				if(lpSession->GetSecurity()->IsAdminOverOwnerOfObject(ulObjId) == erSuccess)
					bAdmin = true; // Admin of all stores
				else if(lpSession->GetSecurity()->IsStoreOwner(ulObjId) == erSuccess)
					bAdmin = true; // Admin of your one store
				else {
					lpSession->GetSecurity()->GetObjectPermission(ulObjId, &ulRights); // skip error checking, ulRights = 0

					bOwner = lpSession->GetSecurity()->IsOwner(ulObjId) == erSuccess; // owner of this particular object in someone else's store
				}

				if(bAdmin == true || ulRights&ecRightsCreate || ulRights&ecRightsEditAny || ulRights&ecRightsDeleteAny || ulRights&ecRightsCreateSubfolder || 
					(bOwner == true  && (ulRights&ecRightsEditOwned || ulRights&ecRightsDeleteOwned)) )
				{
					sPropVal.Value.ul = MAPI_MODIFY;
				}
			}
			break;
        case PROP_ID(PR_ROW_TYPE):
			sPropVal.ulPropTag = ulPropTag;
			sPropVal.__union = SOAP_UNION_propValData_ul;
			sPropVal.Value.ul = TBL_LEAF_ROW;
            break;

		case PROP_ID(PR_MAPPING_SIGNATURE):
			sPropVal.ulPropTag = ulPropTag;

			sPropVal.Value.bin = s_alloc<struct xsd__base64Binary>(soap);
			sPropVal.Value.bin->__ptr = s_alloc<unsigned char>(soap, sizeof(GUID));

			sPropVal.__union = SOAP_UNION_propValData_bin;
			sPropVal.Value.bin->__size = sizeof(GUID);

			er = lpSession->GetServerGUID((GUID*)sPropVal.Value.bin->__ptr);
			if(er != erSuccess){
				er = ZARAFA_E_NOT_FOUND;
				goto exit;
			}
			break;
		case PROP_ID(PR_EC_DELETED_STORE):
			sPropVal.ulPropTag = PR_EC_DELETED_STORE;
			sPropVal.__union = SOAP_UNION_propValData_b;

			er = IsOrphanStore(lpSession, ulObjId, &sPropVal.Value.b);
			if(er != erSuccess){
				er = ZARAFA_E_NOT_FOUND;
				goto exit;
			}
			break;
		default:
			er = ZARAFA_E_NOT_FOUND;
			goto exit;
	}

	*lpPropVal = sPropVal;
exit:
	if(sPropTagArray.__ptr)
		delete[] sPropTagArray.__ptr;

	if(soap == NULL) { // soap != NULL gsoap will cleanup the memory
		if (sPropValArray.__ptr) 
			delete[] sPropValArray.__ptr;

		if (er != erSuccess)
			FreePropVal(&sPropVal, false);
	}

	return er;
}

/**
 * Is the given store a orphan store.
 *
 * @param[in] lpSession	Session to use for this context
 * @param[in] ulObjId	Hierarchy id of a store
 * @param[out] lpbIsOrphan	True is the store is a orphan store, false if not.
 */
ECRESULT ECGenProps::IsOrphanStore(ECSession* lpSession, unsigned int ulObjId, bool *lpbIsOrphan)
{
	ECRESULT	er = erSuccess;
	ECDatabase *lpDatabase = NULL;
	DB_RESULT 	lpDBResult = NULL;
	DB_ROW		lpDBRow = NULL;
	DB_LENGTHS	lpDBLength = NULL;
	std::string strQuery;
	bool		bIsOrphan = false;

	if (!lpSession || !lpbIsOrphan) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	er = lpSession->GetDatabase(&lpDatabase);
	if (er != erSuccess)
		goto exit;

	strQuery = "SELECT 0 FROM stores where user_id != 0 and hierarchy_id="+stringify(ulObjId);
	er = lpDatabase->DoSelect(strQuery, &lpDBResult);
	if(er != erSuccess)
		goto exit;

	if (lpDatabase->GetNumRows(lpDBResult) == 0)
		bIsOrphan = true;


	*lpbIsOrphan = bIsOrphan;

exit:
	if (lpDBResult)
		lpDatabase->FreeResult(lpDBResult);

	return er;
}

/* Get store name
 *
 * Gets the PR_DISPLAY_NAME for the given store ID
 *
 * @param soap gSOAP struct for memory allocation
 * @param lpSession Session to use for this context
 * @param ulStoreId Store ID to get display name for
 * @param lppStoreName Output pointer
 * @return result
 */
ECRESULT ECGenProps::GetStoreName(struct soap *soap, ECSession* lpSession, unsigned int ulStoreId, unsigned int ulStoreType, char** lppStoreName)
{
	ECRESULT			er = erSuccess;
	unsigned int		ulUserId = 0;
	unsigned int	    ulCompanyId = 0;
	struct propValArray sPropValArray = {0, 0};
	struct propTagArray sPropTagArray = {0, 0};

	string				strFormat;
	char*				lpStoreName = NULL;

	er = lpSession->GetSecurity()->GetStoreOwner(ulStoreId, &ulUserId);
	if (er != erSuccess)
		goto exit;

	// get the companyid to which the logged in user belongs to.
	er = lpSession->GetSecurity()->GetUserCompany(&ulCompanyId);
	if (er != erSuccess)
		goto exit;

	// When the userid belongs to a company or group everybody, the store is considered a public store.
	if(ulUserId == ZARAFA_UID_EVERYONE || ulUserId == ulCompanyId) {
	    strFormat = _("Public Folders");
	} else {
        sPropTagArray.__ptr = new unsigned int[3];
        sPropTagArray.__ptr[0] = PR_DISPLAY_NAME;
        sPropTagArray.__ptr[1] = PR_ACCOUNT;
        sPropTagArray.__ptr[2] = PR_EC_COMPANY_NAME;
        sPropTagArray.__size = 3;

        er = lpSession->GetUserManagement()->GetProps(soap, ulUserId, &sPropTagArray, &sPropValArray);
        if (er != erSuccess || !sPropValArray.__ptr) {
            er = ZARAFA_E_NOT_FOUND;
            goto exit;
        }

        strFormat = string(lpSession->GetSessionManager()->GetConfig()->GetSetting("storename_format"));

        for (int i = 0; i < sPropValArray.__size; i++) {
            string sub;
            size_t pos = 0;

            switch (sPropValArray.__ptr[i].ulPropTag) {
            case PR_DISPLAY_NAME:
                sub = "%f";
                break;
            case PR_ACCOUNT:
                sub = "%u";
                break;
            case PR_EC_COMPANY_NAME:
                sub = "%c";
                break;
            default:
                break;
            }

            if (sub.empty())
                continue;

            while ((pos = strFormat.find(sub, pos)) != string::npos)
                strFormat.replace(pos, sub.size(), sPropValArray.__ptr[i].Value.lpszA);
        }

		if (ulStoreType == ECSTORE_TYPE_PRIVATE)
			strFormat = string(_("Inbox")) + " - " + strFormat;
		else if (ulStoreType == ECSTORE_TYPE_ARCHIVE)
			strFormat = string(_("Archive")) + " - " + strFormat;
		else
			assert(false);
    }
    
	lpStoreName = s_alloc<char>(soap, strFormat.size() + 1);
	strcpy(lpStoreName, strFormat.c_str());

	*lppStoreName = lpStoreName;

exit:
	if(sPropTagArray.__ptr)
		delete[] sPropTagArray.__ptr;

	return er;
}

