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

#include "ZarafaUtil.h"
#include "mapicode.h"
#include "../common/stringutil.h"
#include "../common/base64.h"

#include <mapidefs.h>
#include "ECGuid.h"
#include "ZarafaVersions.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

bool IsZarafaEntryId(ULONG cb, LPBYTE lpEntryId)
{
	bool	bZarafaEntry = false;
	EID*	peid = NULL;

	if(lpEntryId == NULL)
		goto exit;

	peid = (PEID)lpEntryId;

	if( (cb == sizeof(EID) && peid->ulVersion == 1) ||
		(cb == sizeof(EID_V0) && peid->ulVersion == 0 ) )
		bZarafaEntry = true;

	//TODO: maybe also a check on objType
exit:
	return bZarafaEntry;
}

bool ValidateZarafaEntryId(ULONG cb, LPBYTE lpEntryId, unsigned int ulCheckType)
{

	bool	bOk = false;
	EID*	peid = NULL;

	if(lpEntryId == NULL)
		goto exit;

	peid = (PEID)lpEntryId;

	if( ((cb == sizeof(EID) && peid->ulVersion == 1) ||
		 (cb == sizeof(EID_V0) && peid->ulVersion == 0 ) ) &&
		 peid->ulType == ulCheckType)
	{
		bOk = true;
	}

exit:
	return bOk;
}

/**
 * Validate a zarafa entryid list on a specific mapi object type
 * 
 * @param lpMsgList		Pointer to an ENTRYLIST structure that contains the number 
 *						of items to validate and an array of ENTRYID structures identifying the items.
 * @param ulCheckType	Contains the type of the objects in the lpMsgList. 
 *
 * @return bool			true if all the items in the lpMsgList matches with the object type
 */
bool ValidateZarafaEntryList(LPENTRYLIST lpMsgList, unsigned int ulCheckType)
{

	bool	bOk = true;
	EID*	peid = NULL;

	if(lpMsgList == NULL) {
		bOk = false;
		goto exit;
	}

	for(ULONG i=0; i < lpMsgList->cValues; i++)
	{
		peid = (PEID)lpMsgList->lpbin[i].lpb;

		if( !(((lpMsgList->lpbin[i].cb == sizeof(EID) && peid->ulVersion == 1) ||
			 (lpMsgList->lpbin[i].cb == sizeof(EID_V0) && peid->ulVersion == 0 ) ) &&
			 peid->ulType == ulCheckType))
		{
			bOk = false;
			goto exit;
		}
	}

exit:
	return bOk;
}

ECRESULT GetStoreGuidFromEntryId(ULONG cb, LPBYTE lpEntryId, LPGUID lpguidStore)
{
	ECRESULT er = erSuccess;
	EID*	peid = NULL;

	if(lpEntryId == NULL || lpguidStore == NULL) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	peid = (PEID)lpEntryId;

	if(!((cb == sizeof(EID) && peid->ulVersion == 1) ||
		 (cb == sizeof(EID_V0) && peid->ulVersion == 0 )) )
	{
		er = ZARAFA_E_INVALID_ENTRYID;
		goto exit;
	}

	memcpy(lpguidStore, &peid->guid, sizeof(GUID));

exit:
	return er;
}

ECRESULT GetObjTypeFromEntryId(ULONG cb, LPBYTE lpEntryId, unsigned int* lpulObjType)
{
	ECRESULT er = erSuccess;
	EID*	peid = NULL;

	if(lpEntryId == NULL || lpulObjType == NULL) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	peid = (PEID)lpEntryId;

	if(!((cb == sizeof(EID) && peid->ulVersion == 1) ||
		 (cb == sizeof(EID_V0) && peid->ulVersion == 0 )) )
	{
		er = ZARAFA_E_INVALID_ENTRYID;
		goto exit;
	}


	*lpulObjType = peid->ulType;

exit:
	return er;
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

	return ZarafaErrorToMAPIError(er);
}

HRESULT HrGetObjTypeFromEntryId(ULONG cb, LPBYTE lpEntryId, unsigned int* lpulObjType)
{
	ECRESULT er = GetObjTypeFromEntryId(cb, lpEntryId, lpulObjType);

	return ZarafaErrorToMAPIError(er);
}

ECRESULT ABEntryIDToID(ULONG cb, LPBYTE lpEntryId, unsigned int* lpulID, objectid_t* lpsExternId, unsigned int* lpulMapiType)
{
	ECRESULT		er = erSuccess;
	PABEID			lpABEID = NULL;
	unsigned int	ulID = 0;
	objectid_t		sExternId;
	objectclass_t	sClass = ACTIVE_USER;

	if(lpEntryId == NULL || lpulID == NULL || cb < CbNewABEID("")) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	lpABEID = (PABEID)lpEntryId;
	ulID = lpABEID->ulId;
	MAPITypeToType(lpABEID->ulType, &sClass);

	if (lpABEID->ulVersion == 1)
		sExternId = objectid_t(base64_decode((char*)lpABEID->szExId), sClass);

	*lpulID = ulID;

	if (lpsExternId)
		*lpsExternId = sExternId;

	if (lpulMapiType)
		*lpulMapiType = lpABEID->ulType;

exit:
	return er;
}

ECRESULT ABEntryIDToID(entryId* lpsEntryId, unsigned int* lpulID, objectid_t* lpsExternId, unsigned int *lpulMapiType)
{
	ECRESULT er = erSuccess;

	if(lpsEntryId == NULL) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	er = ABEntryIDToID(lpsEntryId->__size, lpsEntryId->__ptr, lpulID, lpsExternId, lpulMapiType);

exit:
	return er;
}

ECRESULT SIEntryIDToID(ULONG cb, LPBYTE lpInstanceId, LPGUID guidServer, unsigned int *lpulInstanceId, unsigned int *lpulPropId)
{
	ECRESULT er = erSuccess;
	LPSIEID lpInstanceEid = NULL;

	if (lpInstanceId == NULL) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	lpInstanceEid = (LPSIEID)lpInstanceId;

	if (guidServer)
		memcpy(guidServer, (LPBYTE)lpInstanceEid + sizeof(SIEID), sizeof(GUID));
	if (lpulInstanceId)
		*lpulInstanceId = lpInstanceEid->ulId;
	if (lpulPropId)
		*lpulPropId = lpInstanceEid->ulType;

exit:
	return er;
}

/**
 * Compares ab entryid's and returns an int, can be used for sorting algorithms.
 * <0 left first
 *  0 same, or invalid
 * >0 right first
 */
int SortCompareABEID(ULONG cbEntryID1, LPENTRYID lpEntryID1, ULONG cbEntryID2, LPENTRYID lpEntryID2)
{
	int rv = 0;

	PABEID peid1 = (PABEID)lpEntryID1;
	PABEID peid2 = (PABEID)lpEntryID2;

	if (lpEntryID1 == NULL || lpEntryID2 == NULL)
		goto exit;

	if (peid1->ulVersion != peid2->ulVersion) {
		rv = peid1->ulVersion - peid2->ulVersion;
		goto exit;
	}

	// sort: user(6), group(8), company(4)
	if (peid1->ulType != peid2->ulType)  {
		if (peid1->ulType == MAPI_ABCONT)
			rv = -1;
		else if (peid2->ulType == MAPI_ABCONT)
			rv = 1;

		rv = peid1->ulType - peid2->ulType;
	}

	if (peid1->ulVersion == 0) {
		rv = peid1->ulId - peid2->ulId;
	} else {
		rv = strcmp((char*)peid1->szExId, (char*)peid2->szExId);
	}
	if (rv != 0)
		goto exit;

	rv = memcmp(&peid1->guid, &peid2->guid, sizeof(GUID));
	if (rv != 0)
		goto exit;

exit:
	return rv;
}

bool CompareABEID(ULONG cbEntryID1, LPENTRYID lpEntryID1, ULONG cbEntryID2, LPENTRYID lpEntryID2)
{
	bool fTheSame = false;

	PABEID peid1 = (PABEID)lpEntryID1;
	PABEID peid2 = (PABEID)lpEntryID2;

	if (lpEntryID1 == NULL || lpEntryID2 == NULL)
		goto exit;

	if (peid1->ulVersion == peid2->ulVersion)
	{
		if(cbEntryID1 != cbEntryID2)
			goto exit;

		if(cbEntryID1 < CbNewABEID(""))
			goto exit;

		if (peid1->ulVersion == 0) {
			if(peid1->ulId != peid2->ulId)
				goto exit;
		} else {
			if (strcmp((char*)peid1->szExId, (char*)peid2->szExId))
				goto exit;
		}
	}
	else
	{
		if (cbEntryID1 < CbNewABEID("") || cbEntryID2 < CbNewABEID(""))
			goto exit;

		if(peid1->ulId != peid2->ulId)
			goto exit;
	}

	if(peid1->guid != peid2->guid)
		goto exit;

	if(peid1->ulType != peid2->ulType)
		goto exit;

	fTheSame = true;

exit:
	return fTheSame;
}

HRESULT HrSIEntryIDToID(ULONG cb, LPBYTE lpInstanceId, LPGUID guidServer, unsigned int *lpulInstanceId, unsigned int *lpulPropId)
{
	HRESULT hr = S_OK;

	if(lpInstanceId == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	hr = ZarafaErrorToMAPIError( SIEntryIDToID(cb, lpInstanceId, guidServer, lpulInstanceId, lpulPropId) );

exit:
	return hr;
}

ECRESULT ABIDToEntryID(struct soap *soap, unsigned int ulID, const objectid_t& sExternId, entryId *lpsEntryId)
{
	ECRESULT		er			= erSuccess;
	PABEID			lpUserEid	= NULL;
	std::string		strEncExId  = base64_encode((unsigned char*)sExternId.id.c_str(), sExternId.id.size());
	unsigned int	ulLen       = 0;

	if (lpsEntryId == NULL) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	ulLen = CbNewABEID(strEncExId.c_str());
	lpUserEid = (PABEID)s_alloc<char>(soap, ulLen);
	memset(lpUserEid, 0, ulLen);
	lpUserEid->ulId = ulID;
	er = TypeToMAPIType(sExternId.objclass, &lpUserEid->ulType);
	if (er != erSuccess)
		goto exit;				// 	or make default type user?
	memcpy(&lpUserEid->guid, MUIDECSAB_SERVER, sizeof(GUID));

	// If the externid is non-empty, we'll create a V1 entry id.
	if (!sExternId.id.empty())
	{
		lpUserEid->ulVersion = 1;
		// avoid FORTIFY_SOURCE checks in strcpy to an address that the compiler thinks is 1 size large
		memcpy(lpUserEid->szExId, strEncExId.c_str(), strEncExId.length()+1);
	}

	lpsEntryId->__size = ulLen;
	lpsEntryId->__ptr = (unsigned char*)lpUserEid;

exit:
	return er;
}

ECRESULT SIIDToEntryID(struct soap *soap, LPGUID guidServer, unsigned int ulInstanceId, unsigned int ulPropId, entryId *lpsInstanceId)
{
	ECRESULT er = erSuccess;
	LPSIEID lpInstanceEid = NULL;
	ULONG ulSize = 0;

	ASSERT(ulPropId < 0x0000FFFF);

	if (lpsInstanceId == NULL) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	ulSize = sizeof(SIEID) + sizeof(GUID);

	lpInstanceEid = (LPSIEID)s_alloc<char>(soap, ulSize);
	memset(lpInstanceEid, 0, ulSize);

	lpInstanceEid->ulId = ulInstanceId;
	lpInstanceEid->ulType = ulPropId;
	memcpy(&lpInstanceEid->guid, MUIDECSI_SERVER, sizeof(GUID));

	memcpy((char *)lpInstanceEid + sizeof(SIEID), guidServer, sizeof(GUID));

	lpsInstanceId->__size = ulSize;
	lpsInstanceId->__ptr = (unsigned char *)lpInstanceEid;

exit:
	return er;
}

ECRESULT SIEntryIDToID(entryId* sInstanceId, LPGUID guidServer, unsigned int *lpulInstanceId, unsigned int *lpulPropId)
{
	ECRESULT er = erSuccess;

	if (sInstanceId == NULL) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	er = SIEntryIDToID(sInstanceId->__size, sInstanceId->__ptr, guidServer, lpulInstanceId, lpulPropId);

exit:
	return er;
}

// NOTE: when using this function, we can never be sure that we return the actual objectclass_t.
// MAPI_MAILUSER can also be any type of nonactive user, groups can be security groups etc...
// This can only be used as a hint. You should really look the user up since you should either know the
// users table id, or extern id of the user too!
ECRESULT MAPITypeToType(ULONG ulMAPIType, objectclass_t *lpsUserObjClass)
{
	ECRESULT			er = erSuccess;
	objectclass_t		sUserObjClass = OBJECTCLASS_UNKNOWN;

	if (lpsUserObjClass == NULL) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

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
		er = ZARAFA_E_INVALID_TYPE;
		goto exit;
	}

	*lpsUserObjClass = sUserObjClass;

exit:
	return er;
}

ECRESULT TypeToMAPIType(objectclass_t sUserObjClass, ULONG *lpulMAPIType)
{
	ECRESULT		er = erSuccess;
	ULONG			ulMAPIType = MAPI_MAILUSER;

	if (lpulMAPIType == NULL) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

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
		er = ZARAFA_E_INVALID_TYPE;
		goto exit;
	}

	*lpulMAPIType = ulMAPIType;

exit:
	return er;
}

/**
 * Parse a Zarafa version string in the form [0,]<general>,<major>,<minor>[,<svn_revision>] and
 * place the result in a 32bit unsigned integer.
 * The format of the result is 1 byte general, 1 bytes major and 2 bytes minor.
 * The svn_revision is optional and ignored in any case.
 *
 * @param[in]	strVersion		The version string to parse
 * @param[out]	lpulVersion		Pointer to the unsigned integer in which the result is stored.
 *
 * @retval		ZARAFA_E_INVALID_PARAMETER	The version string could not be parsed.
 */
ECRESULT ParseZarafaVersion(const std::string &strVersion, unsigned int *lpulVersion)
{
	ECRESULT er = ZARAFA_E_INVALID_PARAMETER;
	const char *lpszStart = strVersion.c_str();
	char *lpszEnd = NULL;
	unsigned int ulGeneral, ulMajor, ulMinor;

	// For some reason the server returns its version prefixed with "0,". We'll
	// just ignore that.
	// We assume that there's no actual live server running Zarafa 0,x,y,z
	if (strncmp(lpszStart, "0,", 2) == 0)
		lpszStart += 2;

	ulGeneral = strtoul(lpszStart, &lpszEnd, 10);
	if (lpszEnd == NULL || lpszEnd == lpszStart || *lpszEnd != ',')
		goto exit;

	lpszStart = lpszEnd + 1;
	ulMajor = strtoul(lpszStart, &lpszEnd, 10);
	if (lpszEnd == NULL || lpszEnd == lpszStart || *lpszEnd != ',')
		goto exit;

	lpszStart = lpszEnd + 1;
	ulMinor = strtoul(lpszStart, &lpszEnd, 10);
	if (lpszEnd == NULL || lpszEnd == lpszStart || (*lpszEnd != ',' && *lpszEnd != '\0'))
		goto exit;

	if (lpulVersion)
		*lpulVersion = MAKE_ZARAFA_VERSION(ulGeneral, ulMajor, ulMinor);
	er = erSuccess;

exit:
	return er;
}
