/*
 * Copyright 2005 - 2015  Zarafa B.V. and its licensors
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License, version 3,
 * as published by the Free Software Foundation with the following
 * additional terms according to sec. 7:
 * 
 * "Zarafa" is a registered trademark of Zarafa B.V.
 * The licensing of the Program under the AGPL does not imply a trademark 
 * license. Therefore any rights, title and interest in our trademarks 
 * remain entirely with us.
 * 
 * Our trademark policy (see TRADEMARKS.txt) allows you to use our trademarks
 * in connection with Propagation and certain other acts regarding the Program.
 * In any case, if you propagate an unmodified version of the Program you are
 * allowed to use the term "Zarafa" to indicate that you distribute the Program.
 * Furthermore you may use our trademarks where it is necessary to indicate the
 * intended purpose of a product or service provided you use it in accordance
 * with honest business practices. For questions please contact Zarafa at
 * trademark@zarafa.com.
 *
 * The interactive user interface of the software displays an attribution 
 * notice containing the term "Zarafa" and/or the logo of Zarafa. 
 * Interactive user interfaces of unmodified and modified versions must 
 * display Appropriate Legal Notices according to sec. 5 of the GNU Affero 
 * General Public License, version 3, when you propagate unmodified or 
 * modified versions of the Program. In accordance with sec. 7 b) of the GNU 
 * Affero General Public License, version 3, these Appropriate Legal Notices 
 * must retain the logo of Zarafa or display the words "Initial Development 
 * by Zarafa" if the display of the logo is not reasonably feasible for
 * technical reasons.
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

#ifndef STREAMUTIL_H
#define STREAMUTIL_H

#include <pthread.h>

#include "ZarafaCode.h"
#include "ECDatabase.h"
#include "ECDatabaseUtils.h"
#include "ECSession.h"
#include "ZarafaCmdUtil.h"

#include <SOAPUtils.h>

#ifdef DEBUG
#include <cstdio>
#endif

class ECFifoBuffer;
class ECSerializer;
class ECAttachmentStorage;

struct StreamCaps;
typedef const StreamCaps* LPCSTREAMCAPS;

// Utility Functions
ECRESULT SerializeDatabasePropVal(LPCSTREAMCAPS lpStreamInfo, DB_ROW lpRow, DB_LENGTHS lpLen, ECSerializer *lpSink);
ECRESULT SerializePropVal(LPCSTREAMCAPS lpStreamInfo, const struct propVal &sPropVal, ECSerializer *lpSink, const NamedPropDefMap *lpNamedPropDefs);
ECRESULT SerializeProps(ECSession *lpecSession, ECAttachmentStorage *lpAttachmentStorage, LPCSTREAMCAPS lpStreamInfo, unsigned int ulObjId, unsigned int ulObjType, unsigned int ulParentId, unsigned int ulStoreId, GUID *lpsGuid, ULONG ulFlags, ECSerializer *lpSink);
ECRESULT SerializeMessage(ECSession *lpecSession, ECDatabase *lpDatabase, ECAttachmentStorage *lpAttachmentStorage, LPCSTREAMCAPS lpStreamInfo, unsigned int ulObjId, unsigned int ulObjType, unsigned int ulStoreId, GUID *lpsGuid, ULONG ulFlags, ECSerializer *lpSink, bool bTop);

ECRESULT DeserializePropVal(struct soap *soap, LPCSTREAMCAPS lpStreamInfo, propVal **lppsPropval, ECSerializer *lpSource);
ECRESULT DeserializeProps(ECSession *lpecSession, ECDatabase *lpDatabase, ECAttachmentStorage *lpAttachmentStorage, LPCSTREAMCAPS lpStreamInfo, unsigned int ulObjId, unsigned int ulObjType, unsigned int ulStoreId, GUID *lpsGuid, bool bNewItem, ECSerializer *lpSource, struct propValArray **lppPropValArray);
ECRESULT DeserializeObject(ECSession *lpecSession, ECDatabase *lpDatabase, ECAttachmentStorage *lpAttachmentStorage, LPCSTREAMCAPS lpStreamInfo, unsigned int ulObjId, unsigned int ulStoreId, GUID *lpsGuid, bool bNewItem, unsigned long long ullIMAP, ECSerializer *lpSource, struct propValArray **lppPropValArray);

ECRESULT GetValidatedPropType(DB_ROW lpRow, unsigned int *lpulType);
ECRESULT GetValidatedPropCount(ECDatabase *lpDatabase, DB_RESULT lpDBResult, unsigned int *lpulCount);

#endif // ndef STREAMUTIL_H
