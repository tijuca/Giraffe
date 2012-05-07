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

#ifndef mapi_object_ptr_INCLUDED
#define mapi_object_ptr_INCLUDED

#include <algorithm>
#include <mapiutil.h>
#include "IECUnknown.h"
#include "ECTags.h"
#include "ECGuid.h"
#include "mapi_memory_ptr.h"

// http://tinyurl.com/ydb363n
template<typename BaseT, typename DerivedT>
class Conversion 
{ 
    static DerivedT& derived();
    static char test(const BaseT&);
    static char (&test(...))[2];
public:
    enum { exists = (sizeof(test(derived())) == sizeof(char)) }; 
};

// Forward declaration
template<typename _T, REFIID _R>
class mapi_object_proxy;

// Specializations need forward declaration as well
template<>
class mapi_object_proxy<IUnknown, IID_IUnknown>;


template<typename _T, REFIID _R = GUID_NULL>
class mapi_object_ptr
{
public:
	static const IID				iid;
	typedef _T						value_type;
	typedef _T*						pointer;
	typedef _T**					pointerpointer;
	typedef _T&						reference;
	typedef const _T*				const_pointer;
	typedef const _T&				const_reference;
	typedef mapi_object_proxy<_T,_R>	proxy;

	// Constructors
	mapi_object_ptr() : m_lpObject(NULL) {}
	
	explicit mapi_object_ptr(pointer lpObject, bool bAddRef = false) : m_lpObject(lpObject) {
		if (bAddRef && m_lpObject)
			m_lpObject->AddRef();
	}
	
	mapi_object_ptr(const mapi_object_ptr &other) : m_lpObject(other.m_lpObject) {
		if (m_lpObject)
			m_lpObject->AddRef();
	}
	
	
	// Destructor
	~mapi_object_ptr() {
		if (m_lpObject) {
			m_lpObject->Release();
			m_lpObject = NULL;
		}
	}
	
	
	// Assignment
	mapi_object_ptr& operator=(pointer lpObject) {
		if (m_lpObject != lpObject) {
			mapi_object_ptr tmp(lpObject);
			swap(tmp);
		}
		return *this;
	}
	
	mapi_object_ptr& operator=(const mapi_object_ptr &other) {
		if (this != &other) {
			mapi_object_ptr tmp(other);
			swap(tmp);
		}
		return *this;
	}
	
	
	// Dereference
	pointer operator->() const {
		return m_lpObject;
	}
	
	
	// Utility
	void swap(mapi_object_ptr &other) {
		std::swap(m_lpObject, other.m_lpObject);
	}
	
	template<typename _U>
	HRESULT QueryInterface(_U &refResult) {
		HRESULT		hr = MAPI_E_NOT_INITIALIZED;
		typename _U::pointer	lpNewObject = NULL;
		
		if (m_lpObject) {
			hr = m_lpObject->QueryInterface(_U::iid, (void**)&lpNewObject);
			if (hr == hrSuccess)
				refResult = lpNewObject;

			/**
			 * Here we check if it makes sence to try to get the requested interface through the 
			 * PR_EC_OBJECT object.
			 * It only makes sence to attempt this if the current type (value_type) is derived from
			 * IMAPIProp. If it's higher than IMAPIProp no OpenProperty exists. If it's derived from
			 * ECMAPIProp/ECGenericProp there's no need to try the workaround because we can be sure
			 * it's not wrapped by MAPI.
			 *
			 * The Conversion<IMAPIProp,value_type>::exists is some template magic that checks at
			 * compile time. We could check at run time with a dynamic_cast, but we know the current
			 * type at compile time, so why not check it at compile time?
			 **/
			else if (Conversion<IMAPIProp,value_type>::exists && hr == MAPI_E_INTERFACE_NOT_SUPPORTED) {
				mapi_memory_ptr<SPropValue> ptrPropValue;

				if (HrGetOneProp(m_lpObject, PR_EC_OBJECT, &ptrPropValue) != hrSuccess)
					goto exit;	// hr is still MAPI_E_INTERFACE_NOT_SUPPORTED

				hr = ((IECUnknown*)ptrPropValue->Value.lpszA)->QueryInterface(_U::iid, (void**)&lpNewObject);
				if (hr == hrSuccess)
					refResult = lpNewObject;
			}
		}

	exit:
		return hr;
	}

	operator pointer() const {
		return m_lpObject;
	}

	proxy operator&() {
		return proxy(this);
	}

	bool operator!() const {
		return m_lpObject == NULL;
	}

	pointer get() const {
		return m_lpObject;
	}

	template<typename _P>
	_P as() {
		_P ptrTmp;
		QueryInterface(ptrTmp);
		return ptrTmp;
	}

	void reset(pointer lpObject = NULL) {
		if (m_lpObject)
			m_lpObject->Release();

		m_lpObject = lpObject;
		if (m_lpObject)
			m_lpObject->AddRef();
	}

	pointer release() {
		pointer p = m_lpObject;
		m_lpObject = NULL;
		return p;
	}

private:
	pointerpointer release_and_get() {
		if (m_lpObject) {
			m_lpObject->Release();
			m_lpObject = NULL;
		}

		return &m_lpObject;
	}

private:
	pointer	m_lpObject;

friend class mapi_object_proxy<_T, _R>;
};


template<typename _T, REFIID _R>
class mapi_object_proxy
{
public:
	typedef _T		value_type;
	typedef _T**	pointerpointer;
	typedef mapi_object_ptr<_T, _R>	obj_ptr_type;

	mapi_object_proxy(obj_ptr_type* lppobjptr) : m_lppobjptr(lppobjptr) {}

	operator pointerpointer() { return m_lppobjptr->release_and_get(); }
	operator LPUNKNOWN*() { return (LPUNKNOWN*)m_lppobjptr->release_and_get(); }
	operator LPVOID*() { return (LPVOID*)m_lppobjptr->release_and_get(); }
	operator obj_ptr_type*() { return m_lppobjptr; }

private:
	obj_ptr_type	*m_lppobjptr;
};

template<>
class mapi_object_proxy<IUnknown, IID_IUnknown>
{
public:
	typedef IUnknown	value_type;
	typedef IUnknown**	pointerpointer;
	typedef mapi_object_ptr<IUnknown, IID_IUnknown>	obj_ptr_type;

	mapi_object_proxy(obj_ptr_type* lppobjptr) : m_lppobjptr(lppobjptr) {}

	operator pointerpointer() { return m_lppobjptr->release_and_get(); }
	operator LPVOID*() { return (LPVOID*)m_lppobjptr->release_and_get(); }
	operator obj_ptr_type*() { return m_lppobjptr; }

private:
	obj_ptr_type	*m_lppobjptr;
};


template<typename _T, REFIID _R>
const IID mapi_object_ptr<_T,_R>::iid(_R);

#define DEFINEMAPIPTR(_class) typedef mapi_object_ptr<I ## _class, IID_I ## _class> _class ## Ptr

#endif // mapi_object_ptr_INCLUDED
