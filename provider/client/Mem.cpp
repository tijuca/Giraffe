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

#include <mapispi.h>
#include <mapicode.h>
#include "Trace.h"
#include "Mem.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

// We don't want that here
#undef ECAllocateBuffer

LPMALLOC			_pmalloc = NULL;
LPALLOCATEBUFFER	_pfnAllocBuf = NULL;
LPALLOCATEMORE		_pfnAllocMore = NULL;
LPFREEBUFFER		_pfnFreeBuf = NULL;
HINSTANCE			_hInstance = NULL;

LPALLOCATEBUFFER GetAllocateBuffer() { return _pfnAllocBuf; }
LPALLOCATEMORE GetAllocateMore() { return _pfnAllocMore; }
LPFREEBUFFER GetFreeBuffer() { return _pfnFreeBuf; }
HINSTANCE GetInstance() { return _hInstance; }
LPMALLOC GetMalloc(){ return _pmalloc; }

// This is the same as client-side MAPIFreeBuffer, but uses
// the linked memory routines passed in during MSProviderInit()

// Use the EC* functions to allocate memory that will be
// passed back to the caller through MAPI

HRESULT ECFreeBuffer(void *lpvoid) {
	if(_pfnFreeBuf == NULL)
		return MAPI_E_CALL_FAILED;
	else return _pfnFreeBuf(lpvoid);
}

// Memory tracing
HRESULT ECAllocateBufferDbg(ULONG cbSize, void **lpvoid, char *szFromWhere, int linenr) {
	HRESULT hr = hrSuccess;
	if(_pfnAllocBuf == NULL)
		hr = MAPI_E_CALL_FAILED;
	else hr = _pfnAllocBuf(cbSize, lpvoid);

	TRACE_INTERNAL(TRACE_ENTRY, "MEM", "ECAllocateBuffer", "%s(%d): ALLOC 0x%08x", szFromWhere, linenr, *(unsigned int *)lpvoid);

	return hr;
}

HRESULT ECAllocateBuffer(ULONG cbSize, void **lpvoid) {
	if(_pfnAllocBuf == NULL)
		return MAPI_E_CALL_FAILED;
	else return _pfnAllocBuf(cbSize, lpvoid);
}

HRESULT ECAllocateMore(ULONG cbSize, void *lpBase, void **lpvoid) {
	if(_pfnAllocMore == NULL)
		return MAPI_E_CALL_FAILED;
	else return _pfnAllocMore(cbSize, lpBase, lpvoid);
}

// Use these functions for internal memory management

/* Use new() and delete() instead ! 
HRESULT HrAlloc(ULONG cbSize, void **lpvoid) {
	if(_pmalloc == NULL)
		return MAPI_E_CALL_FAILED;
	else {
		*lpvoid = _pmalloc->Alloc(cbSize);
		if(*lpvoid == NULL)
			return MAPI_E_NOT_ENOUGH_MEMORY;
		else
			return hrSuccess;
	}
}

HRESULT HrRealloc(ULONG cbSize, void *lpBase, void **lpvoid) {
	if(_pmalloc == NULL)
		return MAPI_E_CALL_FAILED;
	else {
		*lpvoid = _pmalloc->Realloc(lpBase, cbSize);
		if(*lpvoid == NULL)
			return MAPI_E_NOT_ENOUGH_MEMORY;
		else
			return hrSuccess;
	}
}

HRESULT HrFree(void *lpvoid) {
	if(lpvoid == NULL)
		return MAPI_E_CALL_FAILED;

	if(_pmalloc == NULL)
		return MAPI_E_CALL_FAILED;
	else {
		_pmalloc->Free(lpvoid);
		return hrSuccess;
	}
}

*/

HRESULT AllocNewMapiObject(ULONG ulUniqueId, ULONG ulObjId, ULONG ulObjType, MAPIOBJECT **lppMapiObject)
{
	MAPIOBJECT *sMapiObject;

	sMapiObject = new MAPIOBJECT;
	sMapiObject->lstChildren = new ECMapiObjects;
	sMapiObject->lstDeleted = new std::list<ULONG>;
	sMapiObject->lstAvailable = new std::list<ULONG>;
	sMapiObject->lstModified = new std::list<ECProperty>;
	sMapiObject->lstProperties = new std::list<ECProperty>;
	sMapiObject->lpInstanceID = NULL;
	sMapiObject->cbInstanceID = 0;
	sMapiObject->bChangedInstance = false;
	sMapiObject->bChanged = false;
	sMapiObject->bDelete = false;
	sMapiObject->ulUniqueId = ulUniqueId;
	sMapiObject->ulObjId = ulObjId;
	sMapiObject->ulObjType = ulObjType;
	*lppMapiObject = sMapiObject;

	return hrSuccess;
}

HRESULT FreeMapiObject(MAPIOBJECT *lpsObject)
{
	ECMapiObjects::iterator iterSObj;

	delete lpsObject->lstAvailable;
	delete lpsObject->lstDeleted;
	delete lpsObject->lstModified;
	delete lpsObject->lstProperties;

	for (iterSObj = lpsObject->lstChildren->begin(); iterSObj != lpsObject->lstChildren->end(); iterSObj++)
		FreeMapiObject(*iterSObj);

	delete lpsObject->lstChildren;

	if (lpsObject->lpInstanceID)
		ECFreeBuffer(lpsObject->lpInstanceID);

	delete lpsObject;

	return hrSuccess;
}
