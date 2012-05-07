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

#ifndef SOAPUTILS_H
#define SOAPUTILS_H

#include "soapH.h"
#include "SOAPAlloc.h"
#include "ZarafaCode.h"
#include "ZarafaUser.h"
#include "ustringutil.h"

#include <list>
#include <string>

// SortOrderSets
void				FreeSortOrderArray(struct sortOrderArray *lpsSortOrder);
int					CompareSortOrderArray(struct sortOrderArray *lpsSortOrder1, struct sortOrderArray *lpsSortOrder2);

// PropTagArrays
ECRESULT			CopyPropTagArray(struct soap *soap, struct propTagArray* lpPTsSrc, struct propTagArray** lppsPTsDst);
void				FreePropTagArray(struct propTagArray *lpsPropTags, bool bFreeBase = true);

// RowSets
void				FreeRowSet(struct rowSet *lpRowSet, bool bBasePointerDel);

// Restrictions
ECRESULT			FreeRestrictTable(struct restrictTable *lpRestrict);
ECRESULT			CopyRestrictTable(struct soap *soap, struct restrictTable *lpSrc, struct restrictTable **lppDst);

// SearchCriteria
ECRESULT			CopySearchCriteria(struct soap* soap, struct searchCriteria *lpSrc, struct searchCriteria **lppSrc);
ECRESULT			FreeSearchCriteria(struct searchCriteria *lpSearchCriteria);

// PropValArrays
ECRESULT			FreePropValArray(struct propValArray *lpPropValArray, bool bFreeBase = false);
struct propVal *	FindProp(struct propValArray *lpPropValArray, unsigned int ulPropTag);
ECRESULT			CopyPropValArray(struct propValArray *lpSrc, struct propValArray *lpDst, struct soap *soap);
ECRESULT			CopyPropValArray(struct propValArray *lpSrc, struct propValArray **lppDst, struct soap *soap);
ECRESULT			MergePropValArray(struct soap *soap, struct propValArray* lpsPropValArray1, struct propValArray* lpsPropValArray2, struct propValArray* lpPropValArrayNew);

// PropVals
ECRESULT			CompareProp(struct propVal *lpProp1, struct propVal *lpProp2, const ECLocale &locale, int* lpCompareResult);
ECRESULT			CompareMVPropWithProp(struct propVal *lpMVProp1, struct propVal *lpProp2, unsigned int ulType, const ECLocale &locale, bool* lpfMatch);

unsigned int		PropSize(struct propVal *lpProp);
ECRESULT			FreePropVal(struct propVal *lpProp, bool bBasePointerDel);
ECRESULT			CopyPropVal(struct propVal *lpSrc, struct propVal *lpDst, struct soap *soap=NULL, bool bTruncate = false);
ECRESULT			CopyPropVal(struct propVal *lpSrc, struct propVal **lppDst, struct soap *soap=NULL, bool bTruncate = false); /* allocates new lpDst and calls other version */

// EntryList
ECRESULT			CopyEntryList(struct soap *soap, struct entryList *lpSrc, struct entryList **lppDst);
ECRESULT			FreeEntryList(struct entryList *lpEntryList, bool bFreeBase = true);

// EntryId
ECRESULT			CopyEntryId(struct soap *soap, entryId* lpSrc, entryId** lppDst);
ECRESULT			FreeEntryId(entryId* lpEntryId, bool bFreeBase);

// Notification
ECRESULT			FreeNotificationStruct(notification *lpNotification, bool bFreeBase=true);
ECRESULT			CopyNotificationStruct(struct soap *soap, notification *lpNotification, notification &rNotifyTo);

ECRESULT			FreeNotificationArrayStruct(notificationArray *lpNotifyArray, bool bFreeBase);
ECRESULT			CopyNotificationArrayStruct(notificationArray *lpNotifyArrayFrom, notificationArray *lpNotifyArrayTo);

ECRESULT			FreeUserObjectArray(struct userobjectArray *lpUserobjectArray, bool bFreeBase);

// Rights
ECRESULT			FreeRightsArray(struct rightsArray *lpRights);
ECRESULT			CopyRightsArrayToSoap(struct soap *soap, struct rightsArray *lpRightsArraySrc, struct rightsArray **lppRightsArrayDst);

// userobjects
ECRESULT			CopyUserObjectDetailsToSoap(unsigned int ulId, entryId *lpUserEid, const objectdetails_t &details,
												struct soap *soap, struct userobject *lpObject);
ECRESULT			CopyUserDetailsToSoap(unsigned int ulId, entryId *lpUserEid, const objectdetails_t &details,
										  struct soap *soap, struct user *lpUser);
ECRESULT			CopyUserDetailsFromSoap(struct user *lpUser, std::string *lpstrExternId, objectdetails_t *details, struct soap *soap);
ECRESULT			CopyGroupDetailsToSoap(unsigned int ulId, entryId *lpGroupEid, const objectdetails_t &details,
										   struct soap *soap, struct group *lpGroup);
ECRESULT			CopyGroupDetailsFromSoap(struct group *lpGroup, std::string *lpstrExternId, objectdetails_t *details, struct soap *soap);
ECRESULT			CopyCompanyDetailsToSoap(unsigned int ulId, entryId *lpCompanyEid, unsigned int ulAdmin, entryId *lpAdminEid, 
											 const objectdetails_t &details, struct soap *soap, struct company *lpCompany);
ECRESULT			CopyCompanyDetailsFromSoap(struct company *lpCompany, std::string *lpstrExternId, unsigned int ulAdmin,
											   objectdetails_t *details, struct soap *soap);

ECRESULT			FreeNamedPropArray(struct namedPropArray *array, bool bFreeBase);

ULONG 				NormalizePropTag(ULONG ulPropTag);

unsigned int SearchCriteriaSize(struct searchCriteria *lpSrc);
unsigned int RestrictTableSize(struct restrictTable *lpSrc);
unsigned int PropValArraySize(struct propValArray *lpSrc);
unsigned int EntryListSize(struct entryList *lpSrc);
unsigned int EntryIdSize(entryId *lpEntryid);
unsigned int NotificationStructSize(notification *lpNotification);
unsigned int PropTagArraySize(struct propTagArray *pPropTagArray);
unsigned int SortOrderArraySize(struct sortOrderArray *lpsSortOrder);

class DynamicPropValArray {
public:
    DynamicPropValArray(struct soap *soap, unsigned int ulHint = 10);
    ~DynamicPropValArray();
    
    // Copies the passed propVal
    ECRESULT AddPropVal(struct propVal &propVal);
    
    // Return a propvalarray of all properties passed
    ECRESULT GetPropValArray(struct propValArray *lpPropValArray);

private:
    ECRESULT Resize(unsigned int ulSize);
    
    struct soap *m_soap;
    struct propVal *m_lpPropVals;
    unsigned int m_ulCapacity;
    unsigned int m_ulPropCount;
};

class DynamicPropTagArray {
public:
    DynamicPropTagArray(struct soap *soap);
    ~DynamicPropTagArray();
    
    ECRESULT AddPropTag(unsigned int ulPropTag);
    BOOL HasPropTag(unsigned int ulPropTag) const;
    
    ECRESULT GetPropTagArray(struct propTagArray *lpPropTagArray);
    
private:
    std::list<unsigned int> m_lstPropTags;
    struct soap *m_soap;
};

#endif
