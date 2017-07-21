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
#include <utility>
#include "pcutil.hpp"
#include <mapicode.h>
#include <kopano/stringutil.h>
#include <mapidefs.h>
#include <kopano/ECGuid.h>
#include "versions.h"

namespace KC {

bool IsKopanoEntryId(ULONG cb, LPBYTE lpEntryId)
{
	EID*	peid = NULL;

	if(lpEntryId == NULL)
		return false;

	peid = (PEID)lpEntryId;

	/* TODO: maybe also a check on objType */
	if( (cb == sizeof(EID) && peid->ulVersion == 1) ||
		(cb == sizeof(EID_V0) && peid->ulVersion == 0 ) )
		return true;

	return false;
}

bool ValidateZEntryId(ULONG cb, LPBYTE lpEntryId, unsigned int ulCheckType)
{

	EID*	peid = NULL;

	if(lpEntryId == NULL)
		return false;

	peid = (PEID)lpEntryId;

	if( ((cb == sizeof(EID) && peid->ulVersion == 1) ||
		 (cb == sizeof(EID_V0) && peid->ulVersion == 0 ) ) &&
		 peid->usType == ulCheckType)
		return true;
	return false;
}

/**
 * Validate a kopano entryid list on a specific mapi object type
 * 
 * @param lpMsgList		Pointer to an ENTRYLIST structure that contains the number 
 *						of items to validate and an array of ENTRYID structures identifying the items.
 * @param ulCheckType	Contains the type of the objects in the lpMsgList. 
 *
 * @return bool			true if all the items in the lpMsgList matches with the object type
 */
bool ValidateZEntryList(LPENTRYLIST lpMsgList, unsigned int ulCheckType)
{

	EID*	peid = NULL;

	if (lpMsgList == NULL)
		return false;

	for (ULONG i = 0; i < lpMsgList->cValues; ++i) {
		peid = (PEID)lpMsgList->lpbin[i].lpb;

		if( !(((lpMsgList->lpbin[i].cb == sizeof(EID) && peid->ulVersion == 1) ||
			 (lpMsgList->lpbin[i].cb == sizeof(EID_V0) && peid->ulVersion == 0 ) ) &&
			 peid->usType == ulCheckType))
			return false;
	}
	return true;
}

ECRESULT GetStoreGuidFromEntryId(ULONG cb, LPBYTE lpEntryId, LPGUID lpguidStore)
{
	EID*	peid = NULL;

	if(lpEntryId == NULL || lpguidStore == NULL)
		return KCERR_INVALID_PARAMETER;

	peid = (PEID)lpEntryId;

	if(!((cb == sizeof(EID) && peid->ulVersion == 1) ||
		 (cb == sizeof(EID_V0) && peid->ulVersion == 0 )) )
		return KCERR_INVALID_ENTRYID;

	memcpy(lpguidStore, &peid->guid, sizeof(GUID));
	return erSuccess;
}

ECRESULT GetObjTypeFromEntryId(ULONG cb, LPBYTE lpEntryId, unsigned int* lpulObjType)
{
	EID*	peid = NULL;

	if (lpEntryId == NULL || lpulObjType == NULL)
		return KCERR_INVALID_PARAMETER;

	peid = (PEID)lpEntryId;

	if(!((cb == sizeof(EID) && peid->ulVersion == 1) ||
		 (cb == sizeof(EID_V0) && peid->ulVersion == 0 )) )
		return KCERR_INVALID_ENTRYID;

	*lpulObjType = peid->usType;
	return erSuccess;
}

ECRESULT GetObjTypeFromEntryId(entryId sEntryId,  unsigned int* lpulObjType) {
    return GetObjTypeFromEntryId(sEntryId.__size, sEntryId.__ptr, lpulObjType);
}

ECRESULT GetStoreGuidFromEntryId(entryId sEntryId, LPGUID lpguidStore) {
    return GetStoreGuidFromEntryId(sEntryId.__size, sEntryId.__ptr, lpguidStore);
}

HRESULT HrGetStoreGuidFromEntryId(ULONG cb, LPBYTE lpEntryId, LPGUID lpguidStore)
{
	ECRESULT er = GetStoreGuidFromEntryId(cb, lpEntryId, lpguidStore);

	return kcerr_to_mapierr(er);
}

HRESULT HrGetObjTypeFromEntryId(ULONG cb, LPBYTE lpEntryId, unsigned int* lpulObjType)
{
	ECRESULT er = GetObjTypeFromEntryId(cb, lpEntryId, lpulObjType);

	return kcerr_to_mapierr(er);
}

ECRESULT ABEntryIDToID(ULONG cb, LPBYTE lpEntryId, unsigned int* lpulID, objectid_t* lpsExternId, unsigned int* lpulMapiType)
{
	unsigned int	ulID = 0;
	objectid_t		sExternId;
	objectclass_t	sClass = ACTIVE_USER;

	if (lpEntryId == NULL || lpulID == NULL || cb < CbNewABEID(""))
		return KCERR_INVALID_PARAMETER;

	auto lpABEID = reinterpret_cast<const ABEID *>(lpEntryId);
	if (memcmp(&lpABEID->guid, &MUIDECSAB, sizeof(GUID)) != 0)
		return KCERR_INVALID_ENTRYID;

	ulID = lpABEID->ulId;
	MAPITypeToType(lpABEID->ulType, &sClass);

	if (lpABEID->ulVersion == 1)
		sExternId = objectid_t(base64_decode(reinterpret_cast<const char *>(lpABEID->szExId)), sClass);

	*lpulID = ulID;

	if (lpsExternId)
		*lpsExternId = std::move(sExternId);
	if (lpulMapiType)
		*lpulMapiType = lpABEID->ulType;
	return erSuccess;
}

ECRESULT ABEntryIDToID(entryId* lpsEntryId, unsigned int* lpulID, objectid_t* lpsExternId, unsigned int *lpulMapiType)
{
	if (lpsEntryId == NULL)
		return KCERR_INVALID_PARAMETER;
	return ABEntryIDToID(lpsEntryId->__size, lpsEntryId->__ptr, lpulID, lpsExternId, lpulMapiType);
}

ECRESULT SIEntryIDToID(ULONG cb, LPBYTE lpInstanceId, LPGUID guidServer, unsigned int *lpulInstanceId, unsigned int *lpulPropId)
{
	LPSIEID lpInstanceEid;

	if (lpInstanceId == NULL)
		return KCERR_INVALID_PARAMETER;
	lpInstanceEid = (LPSIEID)lpInstanceId;

	if (guidServer)
		memcpy(guidServer, (LPBYTE)lpInstanceEid + sizeof(SIEID), sizeof(GUID));
	if (lpulInstanceId)
		*lpulInstanceId = lpInstanceEid->ulId;
	if (lpulPropId)
		*lpulPropId = lpInstanceEid->ulType;
	return erSuccess;
}

template<typename T> static int twcmp(T a, T b)
{
	/* see elsewhere for raison d'être */
	return (a < b) ? -1 : (a == b) ? 0 : 1;
}

/**
 * Compares ab entryids and returns an int, can be used for sorting algorithms.
 * <0 left first
 *  0 same, or invalid
 * >0 right first
 */
int SortCompareABEID(ULONG cbEntryID1, LPENTRYID lpEntryID1, ULONG cbEntryID2, LPENTRYID lpEntryID2)
{
	int rv = 0;
	auto peid1 = reinterpret_cast<const ABEID *>(lpEntryID1);
	auto peid2 = reinterpret_cast<const ABEID *>(lpEntryID2);

	if (lpEntryID1 == NULL || lpEntryID2 == NULL)
		return 0;
	if (peid1->ulVersion != peid2->ulVersion)
		return twcmp(peid1->ulVersion, peid2->ulVersion);

	// sort: user(6), group(8), company(4)
	if (peid1->ulType != peid2->ulType)  {
		if (peid1->ulType == MAPI_ABCONT)
			return -1;
		else if (peid2->ulType == MAPI_ABCONT)
			return 1;
		else
			rv = twcmp(peid1->ulType, peid2->ulType);
		if (rv != 0)
			return rv;
	}

	if (peid1->ulVersion == 0) {
		rv = twcmp(peid1->ulId, peid2->ulId);
	} else {
		rv = strcmp((char*)peid1->szExId, (char*)peid2->szExId);
	}
	if (rv != 0)
		return rv;
	rv = memcmp(&peid1->guid, &peid2->guid, sizeof(GUID));
	if (rv != 0)
		return rv;
	return 0;
}

bool CompareABEID(ULONG cbEntryID1, LPENTRYID lpEntryID1, ULONG cbEntryID2, LPENTRYID lpEntryID2)
{
	auto peid1 = reinterpret_cast<const ABEID *>(lpEntryID1);
	auto peid2 = reinterpret_cast<const ABEID *>(lpEntryID2);

	if (lpEntryID1 == NULL || lpEntryID2 == NULL)
		return false;

	if (peid1->ulVersion == peid2->ulVersion)
	{
		if(cbEntryID1 != cbEntryID2)
			return false;
		if(cbEntryID1 < CbNewABEID(""))
			return false;
		if (peid1->ulVersion == 0) {
			if(peid1->ulId != peid2->ulId)
				return false;
		} else {
			if (strcmp((char*)peid1->szExId, (char*)peid2->szExId))
				return false;
		}
	}
	else
	{
		if (cbEntryID1 < CbNewABEID("") || cbEntryID2 < CbNewABEID(""))
			return false;
		if(peid1->ulId != peid2->ulId)
			return false;
	}

	if(peid1->guid != peid2->guid)
		return false;
	if(peid1->ulType != peid2->ulType)
		return false;
	return true;
}

HRESULT HrSIEntryIDToID(ULONG cb, LPBYTE lpInstanceId, LPGUID guidServer, unsigned int *lpulInstanceId, unsigned int *lpulPropId)
{
	if(lpInstanceId == NULL)
		return MAPI_E_INVALID_PARAMETER;
	return kcerr_to_mapierr(SIEntryIDToID(cb, lpInstanceId, guidServer, lpulInstanceId, lpulPropId));
}

ECRESULT ABIDToEntryID(struct soap *soap, unsigned int ulID, const objectid_t& sExternId, entryId *lpsEntryId)
{
	ECRESULT er;
	std::string		strEncExId  = base64_encode((unsigned char*)sExternId.id.c_str(), sExternId.id.size());
	unsigned int	ulLen       = 0;

	if (lpsEntryId == NULL)
		return KCERR_INVALID_PARAMETER;

	ulLen = CbNewABEID(strEncExId.c_str());
	auto lpUserEid = reinterpret_cast<ABEID *>(s_alloc<char>(soap, ulLen));
	memset(lpUserEid, 0, ulLen);
	lpUserEid->ulId = ulID;
	er = TypeToMAPIType(sExternId.objclass, &lpUserEid->ulType);
	if (er != erSuccess) {
		s_free(soap, lpUserEid);
		return er; /* or make default type user? */
	}

	memcpy(&lpUserEid->guid, &MUIDECSAB, sizeof(GUID));

	// If the externid is non-empty, we'll create a V1 entry id.
	if (!sExternId.id.empty())
	{
		lpUserEid->ulVersion = 1;
		// avoid FORTIFY_SOURCE checks in strcpy to an address that the compiler thinks is 1 size large
		memcpy(lpUserEid->szExId, strEncExId.c_str(), strEncExId.length()+1);
	}

	lpsEntryId->__size = ulLen;
	lpsEntryId->__ptr = (unsigned char*)lpUserEid;
	return erSuccess;
}

ECRESULT SIIDToEntryID(struct soap *soap, LPGUID guidServer, unsigned int ulInstanceId, unsigned int ulPropId, entryId *lpsInstanceId)
{
	LPSIEID lpInstanceEid = NULL;
	ULONG ulSize = 0;

	assert(ulPropId < 0x0000FFFF);
	if (lpsInstanceId == NULL)
		return KCERR_INVALID_PARAMETER;

	ulSize = sizeof(SIEID) + sizeof(GUID);

	lpInstanceEid = (LPSIEID)s_alloc<char>(soap, ulSize);
	memset(lpInstanceEid, 0, ulSize);

	lpInstanceEid->ulId = ulInstanceId;
	lpInstanceEid->ulType = ulPropId;
	memcpy(&lpInstanceEid->guid, MUIDECSI_SERVER, sizeof(GUID));

	memcpy((char *)lpInstanceEid + sizeof(SIEID), guidServer, sizeof(GUID));

	lpsInstanceId->__size = ulSize;
	lpsInstanceId->__ptr = (unsigned char *)lpInstanceEid;
	return erSuccess;
}

ECRESULT SIEntryIDToID(entryId* sInstanceId, LPGUID guidServer, unsigned int *lpulInstanceId, unsigned int *lpulPropId)
{
	if (sInstanceId == NULL)
		return KCERR_INVALID_PARAMETER;
	return SIEntryIDToID(sInstanceId->__size, sInstanceId->__ptr, guidServer, lpulInstanceId, lpulPropId);
}

// NOTE: when using this function, we can never be sure that we return the actual objectclass_t.
// MAPI_MAILUSER can also be any type of nonactive user, groups can be security groups etc...
// This can only be used as a hint. You should really look the user up since you should either know the
// users table id, or extern id of the user too!
ECRESULT MAPITypeToType(ULONG ulMAPIType, objectclass_t *lpsUserObjClass)
{
	objectclass_t		sUserObjClass = OBJECTCLASS_UNKNOWN;

	if (lpsUserObjClass == NULL)
		return KCERR_INVALID_PARAMETER;

	switch (ulMAPIType) {
	case MAPI_MAILUSER:
		sUserObjClass = OBJECTCLASS_USER;
		break;
	case MAPI_DISTLIST:
		sUserObjClass = OBJECTCLASS_DISTLIST;
		break;
	case MAPI_ABCONT:
		sUserObjClass = OBJECTCLASS_CONTAINER;
		break;
	default:
		return KCERR_INVALID_TYPE;
	}

	*lpsUserObjClass = std::move(sUserObjClass);
	return erSuccess;
}

ECRESULT TypeToMAPIType(objectclass_t sUserObjClass, ULONG *lpulMAPIType)
{
	ULONG			ulMAPIType = MAPI_MAILUSER;

	if (lpulMAPIType == NULL)
		return KCERR_INVALID_PARAMETER;

	// Check for correctness of mapping!
	switch (OBJECTCLASS_TYPE(sUserObjClass))
	{
	case OBJECTTYPE_MAILUSER:
		ulMAPIType = MAPI_MAILUSER;
		break;
	case OBJECTTYPE_DISTLIST:
		ulMAPIType = MAPI_DISTLIST;
		break;
	case OBJECTTYPE_CONTAINER:
		ulMAPIType = MAPI_ABCONT;
		break;
	default:
		return KCERR_INVALID_TYPE;
	}

	*lpulMAPIType = ulMAPIType;
	return erSuccess;
}

/**
 * Parse a Kopano version string in the form [0,]<general>,<major>,<minor>[,<svn_revision>] and
 * place the result in a 32 bit unsigned integer.
 * The format of the result is 1 byte general, 1 bytes major and 2 bytes minor.
 * The svn_revision is optional and ignored in any case.
 *
 * @param[in]	strVersion		The version string to parse
 * @param[out]	lpulVersion		Pointer to the unsigned integer in which the result is stored.
 *
 * @retval		KCERR_INVALID_PARAMETER	The version string could not be parsed.
 */
ECRESULT ParseKopanoVersion(const std::string &strVersion, unsigned int *lpulVersion)
{
	const char *lpszStart = strVersion.c_str();
	char *lpszEnd = NULL;
	unsigned int ulGeneral, ulMajor, ulMinor;

	// For some reason the server returns its version prefixed with "0,". We'll
	// just ignore that.
	// We assume that there's no actual live server running 0,x,y,z
	if (strncmp(lpszStart, "0,", 2) == 0)
		lpszStart += 2;

	ulGeneral = strtoul(lpszStart, &lpszEnd, 10);
	if (lpszEnd == NULL || lpszEnd == lpszStart || *lpszEnd != ',')
		return KCERR_INVALID_PARAMETER;

	lpszStart = lpszEnd + 1;
	ulMajor = strtoul(lpszStart, &lpszEnd, 10);
	if (lpszEnd == NULL || lpszEnd == lpszStart || *lpszEnd != ',')
		return KCERR_INVALID_PARAMETER;

	lpszStart = lpszEnd + 1;
	ulMinor = strtoul(lpszStart, &lpszEnd, 10);
	if (lpszEnd == NULL || lpszEnd == lpszStart || (*lpszEnd != ',' && *lpszEnd != '\0'))
		return KCERR_INVALID_PARAMETER;

	if (lpulVersion)
		*lpulVersion = MAKE_KOPANO_VERSION(ulGeneral, ulMajor, ulMinor);
	return erSuccess;
}

} /* namespace */
