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

#include "platform.h"

#include <mapidefs.h>
#include <mapicode.h>
#include <mapiguid.h>

#include "ECUnknown.h"
#include "ECGuid.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static const char THIS_FILE[] = __FILE__;
#endif

ECUnknown::ECUnknown(const char *szClassName)
{
	this->m_cRef = 0;
	this->szClassName = szClassName;
	this->lpParent = NULL;
	pthread_mutex_init(&mutex, NULL);
}

ECUnknown::~ECUnknown() {
	if(this->lpParent) {
		ASSERT(FALSE);	// apparently, we're being destructed with delete() while
						// a parent was set up, so we should be deleted via Suicide() !
	}

	pthread_mutex_destroy(&mutex);
}

ULONG ECUnknown::AddRef() {
	ULONG cRet;

	pthread_mutex_lock(&mutex);
	cRet = ++this->m_cRef;
	pthread_mutex_unlock(&mutex);

	return cRet;
}

ULONG ECUnknown::Release() {
	ULONG nRef;
	bool bLastRef = false;
	
	pthread_mutex_lock(&mutex);
	this->m_cRef--;

	nRef = m_cRef;

	if((int)m_cRef == -1)
		ASSERT(FALSE);
		
	bLastRef = this->lstChildren.empty() && this->m_cRef == 0;
	
	pthread_mutex_unlock(&mutex);
	
	if(bLastRef)
		this->Suicide();

	// The object may be deleted now

	return nRef;
}

HRESULT ECUnknown::QueryInterface(REFIID refiid, void **lppInterface) {
	REGISTER_INTERFACE(IID_ECUnknown, this);
	REGISTER_INTERFACE(IID_IUnknown, &this->m_xUnknown);

	return MAPI_E_INTERFACE_NOT_SUPPORTED;
}

HRESULT ECUnknown::AddChild(ECUnknown *lpChild) {
	
	pthread_mutex_lock(&mutex);
	
	if(lpChild) {
		this->lstChildren.push_back(lpChild);
		lpChild->SetParent(this);
	}

	pthread_mutex_unlock(&mutex);

	return hrSuccess;
}

HRESULT ECUnknown::RemoveChild(ECUnknown *lpChild) {
	HRESULT hr = hrSuccess;
	std::list<ECUnknown *>::iterator iterChild;
	bool bLastRef;
	
	pthread_mutex_lock(&mutex);

	if(lpChild) {
		for(iterChild = lstChildren.begin(); iterChild != lstChildren.end(); iterChild++) {
			if(*iterChild == lpChild)
				break;
		}
	}

	if(iterChild == lstChildren.end()) {
		hr = MAPI_E_NOT_FOUND;
		pthread_mutex_unlock(&mutex);
		goto exit;
	}

	lstChildren.erase(iterChild);

	bLastRef = this->lstChildren.empty() && this->m_cRef == 0;

	pthread_mutex_unlock(&mutex);

	if(bLastRef)
		this->Suicide();

	// The object may be deleted now
exit:
	return hr;
}

HRESULT ECUnknown::SetParent(ECUnknown *lpParent) {
	// Parent object may only be set once
	ASSERT (this->lpParent==NULL);

	this->lpParent = lpParent;

	return hrSuccess;
}

/** 
 * Returns whether this object is the parent of passed object
 * 
 * @param lpObject Possible child object
 * 
 * @return this is a parent of lpObject, or not
 */
BOOL ECUnknown::IsParentOf(const ECUnknown *lpObject) {
	while (lpObject && lpObject->lpParent) {
		if (lpObject->lpParent == this)
			return TRUE;
		lpObject = lpObject->lpParent;
	}
	return FALSE;
}

/** 
 * Returns whether this object is a child of passed object
 * 
 * @param lpObject IUnknown object which may be a child of this
 * 
 * @return lpObject is a parent of this, or not
 */
BOOL ECUnknown::IsChildOf(const ECUnknown *lpObject) {
	std::list<ECUnknown *>::const_iterator i;
	if (lpObject) {
		for (i = lpObject->lstChildren.begin(); i != lpObject->lstChildren.end(); i++) {
			if (this == *i)
				return TRUE;
			if (this->IsChildOf(*i))
				return TRUE;
		}
	}
	return FALSE;
}

// We kill the local object if there are no external (AddRef()) and no internal
// (AddChild) objects depending on us. 

HRESULT ECUnknown::Suicide() {
	HRESULT hr = hrSuccess;
	ECUnknown *lpParent = this->lpParent;

	// First, destroy the current object
	this->lpParent = NULL;
	delete this;

	// WARNING: The child list of our parent now contains a pointer to this 
	// DELETED object. We must make sure that nobody ever follows pointer references
	// in this list during this interval. The list is, therefore PRIVATE to this object,
	// and may only be access through functions in ECUnknown.

	// Now, tell our parent to delete this object
	if(lpParent) {
		lpParent->RemoveChild(this);
	}

	return hr;
}

HRESULT __stdcall ECUnknown::xUnknown::QueryInterface(REFIID refiid, void ** lppInterface)
{
	METHOD_PROLOGUE_(ECUnknown , Unknown);
	HRESULT hr = pThis->QueryInterface(refiid, lppInterface);
	return hr;
}

ULONG __stdcall ECUnknown::xUnknown::AddRef()
{
	METHOD_PROLOGUE_(ECUnknown , Unknown);
	return pThis->AddRef();
}

ULONG __stdcall ECUnknown::xUnknown::Release()
{
	METHOD_PROLOGUE_(ECUnknown , Unknown);
	return pThis->Release();
}
