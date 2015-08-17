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

#ifndef STORAGEUTIL_H
#define STORAGEUTIL_H

#include <ZarafaCode.h>
#include <mapidefs.h>

class ECDatabase;
class ECAttachmentStorage;
class ECSession;

ECRESULT CreateAttachmentStorage(ECDatabase *lpDatabase, ECAttachmentStorage **lppAttachmentStorage);	
ECRESULT CreateObject(ECSession *lpecSession, ECDatabase *lpDatabase, unsigned int ulParentObjId, unsigned int ulParentType, unsigned int ulObjType, unsigned int ulFlags, unsigned int *lpulObjId);

enum eSizeUpdateAction{ UPDATE_SET, UPDATE_ADD, UPDATE_SUB };
ECRESULT GetObjectSize(ECDatabase* lpDatabase, unsigned int ulObjId, unsigned int* lpulSize);	
ECRESULT CalculateObjectSize(ECDatabase* lpDatabase, unsigned int objid, unsigned int ulObjType, unsigned int* lpulSize);
ECRESULT UpdateObjectSize(ECDatabase* lpDatabase, unsigned int ulObjId, unsigned int ulObjType, eSizeUpdateAction updateAction, long long llSize);

/**
 * Get the corrected object type used to determine course of action.
 * MAPI_MESSAGE objects can contain only MAPI_ATTACH, MAPI_MAILUSER and MAPI_DISTLIST sub objects,
 * but in practice the object type can be different. This function will return MAPI_MAILUSER for
 * any MAPI_MESSAGE subtype that does not match the mentioned types.
 */
static inline unsigned int RealObjType(unsigned int ulObjType, unsigned int ulParentType) {
    if (ulParentType == MAPI_MESSAGE && ulObjType != MAPI_MAILUSER && ulObjType != MAPI_DISTLIST && ulObjType != MAPI_ATTACH)
        return MAPI_MAILUSER;
    return ulObjType;
}

#endif // ndef STORAGEUTIL_H
