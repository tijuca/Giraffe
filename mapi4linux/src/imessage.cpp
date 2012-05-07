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

#include <mapi.h>
#include <mapix.h>
#include <mapiutil.h>

#include <imessage.h>

#include "Util.h"
#include "ECDebug.h"
#include "Trace.h"


SCODE __stdcall OpenIMsgSession(LPMALLOC lpMalloc, ULONG ulFlags, LPMSGSESS *lppMsgSess)
{
	TRACE_MAPILIB(TRACE_ENTRY, "OpenIMsgSession", "");

	SCODE sc = MAPI_E_NO_SUPPORT;
	TRACE_MAPILIB(TRACE_RETURN, "OpenIMsgSession", "sc=0x%08X", sc);
	return sc;
}

void __stdcall CloseIMsgSession(LPMSGSESS lpMsgSess)
{
	TRACE_MAPILIB(TRACE_ENTRY, "CloseIMsgSession", "");

	TRACE_MAPILIB(TRACE_RETURN, "CloseIMsgSession", "");
}

SCODE __stdcall OpenIMsgOnIStg(	LPMSGSESS lpMsgSess, LPALLOCATEBUFFER lpAllocateBuffer, LPALLOCATEMORE lpAllocateMore,
								LPFREEBUFFER lpFreeBuffer, LPMALLOC lpMalloc, LPVOID lpMapiSup, LPSTORAGE lpStg, 
								MSGCALLRELEASE *lpfMsgCallRelease, ULONG ulCallerData, ULONG ulFlags, LPMESSAGE *lppMsg )
{
	TRACE_MAPILIB(TRACE_ENTRY, "OpenIMsgOnIStg", "");
	
	SCODE sc = MAPI_E_NO_SUPPORT;

	TRACE_MAPILIB(TRACE_RETURN, "OpenIMsgOnIStg", "sc=0x%08x", sc);
	return sc;
}

HRESULT __stdcall GetAttribIMsgOnIStg(LPVOID lpObject, LPSPropTagArray lpPropTagArray, LPSPropAttrArray *lppPropAttrArray)
{
	TRACE_MAPILIB(TRACE_ENTRY, "GetAttribIMsgOnIStg", "");
	TRACE_MAPILIB(TRACE_RETURN, "GetAttribIMsgOnIStg", "");
	return MAPI_E_NO_SUPPORT;
}

HRESULT __stdcall SetAttribIMsgOnIStg(LPVOID lpObject, LPSPropTagArray lpPropTags, LPSPropAttrArray lpPropAttrs,
									  LPSPropProblemArray *lppPropProblems)
{
	TRACE_MAPILIB(TRACE_ENTRY, "SetAttribIMsgOnIStg", "");
	TRACE_MAPILIB(TRACE_RETURN, "SetAttribIMsgOnIStg", "");
	return MAPI_E_NO_SUPPORT;
}

SCODE __stdcall MapStorageSCode( SCODE StgSCode )
{
	TRACE_MAPILIB(TRACE_ENTRY, "MapStorageSCode", "");
	return StgSCode;
}
