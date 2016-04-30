/*
 * Copyright 2005 - 2015  Zarafa B.V. and its licensors
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

#ifndef ZARAFAUTIL_H
#define ZARAFAUTIL_H

// All functions which used in zarafa server and client
#include "Zarafa.h"
#include <zarafa/ZarafaCode.h>
#include "soapH.h"
#include <zarafa/ECDefs.h>
#include "SOAPUtils.h"
#include <mapidefs.h>

#include <string>

bool IsZarafaEntryId(ULONG cb, LPBYTE lpEntryId);
bool ValidateZarafaEntryId(ULONG cb, LPBYTE lpEntryId, unsigned int ulCheckType);
bool ValidateZarafaEntryList(LPENTRYLIST lpMsgList, unsigned int ulCheckType);
ECRESULT ABEntryIDToID(ULONG cb, LPBYTE lpEntryId, unsigned int* lpulID, objectid_t* lpsExternId, unsigned int* lpulMapiType);
ECRESULT SIEntryIDToID(ULONG cb, LPBYTE lpInstanceId, LPGUID guidServer, unsigned int *lpulInstanceId, unsigned int *lpulPropId = NULL);
int SortCompareABEID(ULONG cbEntryID1, LPENTRYID lpEntryID1, ULONG cbEntryID2, LPENTRYID lpEntryID2);
bool CompareABEID(ULONG cbEntryID1, LPENTRYID lpEntryID1, ULONG cbEntryID2, LPENTRYID lpEntryID2);

ECRESULT ParseZarafaVersion(const std::string &strVersion, unsigned int *lpulVersion);

//Clientside functions
HRESULT HrGetStoreGuidFromEntryId(ULONG cb, LPBYTE lpEntryId, LPGUID lpguidStore);
HRESULT HrGetObjTypeFromEntryId(ULONG cb, LPBYTE lpEntryId, unsigned int* lpulObjType);
HRESULT HrSIEntryIDToID(ULONG cb, LPBYTE lpInstanceId, LPGUID guidServer, unsigned int *lpulID, unsigned int *lpulPropId = NULL);

// Serverside functions
ECRESULT GetStoreGuidFromEntryId(ULONG cb, LPBYTE lpEntryId, LPGUID guidStore);
ECRESULT GetObjTypeFromEntryId(ULONG cb, LPBYTE lpEntryId, unsigned int* lpulObjType);
ECRESULT GetStoreGuidFromEntryId(entryId sEntryId, LPGUID guidStore);
ECRESULT GetObjTypeFromEntryId(entryId sEntryId, unsigned int* lpulObjType);
ECRESULT ABEntryIDToID(entryId* lpsEntryId, unsigned int* lpulID, objectid_t* lpsExternId, unsigned int* lpulMapiType);
ECRESULT SIEntryIDToID(entryId* sInstanceId, LPGUID guidServer, unsigned int *lpulInstanceId, unsigned int *lpulPropId = NULL);
ECRESULT ABIDToEntryID(struct soap *soap, unsigned int ulID, const objectid_t& strExternId, entryId *lpsEntryId);
ECRESULT SIIDToEntryID(struct soap *soap, LPGUID guidServer, unsigned int ulInstanceId, unsigned int ulPropId, entryId *lpsInstanceId);
ECRESULT MAPITypeToType(ULONG ulMAPIType, objectclass_t *lpsUserObjClass);
ECRESULT TypeToMAPIType(objectclass_t sUserObjClass, ULONG *lpulMAPIType);

#endif
