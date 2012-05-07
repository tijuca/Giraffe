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

#ifndef _M4L_IMESSAGE_H_
#define _M4L_IMESSAGE_H_
#define _IMESSAGE_H_

extern "C"
{

typedef struct _MSGSESS *LPMSGSESS;

typedef void (__stdcall MSGCALLRELEASE)( ULONG ulCallerData, LPMESSAGE lpMessage );

STDAPI_(SCODE) OpenIMsgSession(LPMALLOC lpMalloc, ULONG ulFlags, LPMSGSESS *lppMsgSess);

STDAPI_(void) CloseIMsgSession(LPMSGSESS lpMsgSess);

STDAPI_(SCODE) OpenIMsgOnIStg(LPMSGSESS lpMsgSess, LPALLOCATEBUFFER lpAllocateBuffer,
								LPALLOCATEMORE lpAllocateMore, LPFREEBUFFER lpFreeBuffer, LPMALLOC lpMalloc,
								LPVOID lpMapiSup, LPSTORAGE lpStg, MSGCALLRELEASE *lpfMsgCallRelease,
								ULONG ulCallerData, ULONG ulFlags, LPMESSAGE *lppMsg);

#define IMSG_NO_ISTG_COMMIT		((ULONG) 0x00000001)

#define PROPATTR_MANDATORY		((ULONG) 0x00000001)
#define PROPATTR_READABLE		((ULONG) 0x00000002)
#define PROPATTR_WRITEABLE		((ULONG) 0x00000004)

#define PROPATTR_NOT_PRESENT	((ULONG) 0x00000008)

typedef struct _SPropAttrArray
{
	ULONG	cValues;							
	ULONG	aPropAttr[MAPI_DIM];
} SPropAttrArray, * LPSPropAttrArray;

#define CbNewSPropAttrArray(_cattr) \
	(offsetof(SPropAttrArray,aPropAttr) + (_cattr)*sizeof(ULONG))
#define CbSPropAttrArray(_lparray) \
	(offsetof(SPropAttrArray,aPropAttr) + \
	(UINT)((_lparray)->cValues)*sizeof(ULONG))

#define SizedSPropAttrArray(_cattr, _name) \
struct _SPropAttrArray_ ## _name \
{ \
	ULONG	cValues; \
	ULONG	aPropAttr[_cattr]; \
} _name

STDAPI_(HRESULT) GetAttribIMsgOnIStg(LPVOID lpObject, LPSPropTagArray lpPropTagArray, 
						   LPSPropAttrArray *lppPropAttrArray );

STDAPI_(HRESULT) SetAttribIMsgOnIStg(LPVOID lpObject, LPSPropTagArray lpPropTags, 
						   LPSPropAttrArray lpPropAttrs, LPSPropProblemArray *lppPropProblems);

STDAPI_(SCODE) MapStorageSCode( SCODE StgSCode );

} //extern "C"

#endif	/* _M4L_IMESSAGE_H_ */

