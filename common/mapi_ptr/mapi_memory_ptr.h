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

#ifndef mapi_memory_ptr_INCLUDED
#define mapi_memory_ptr_INCLUDED

#include <utility>

#ifdef free
	#undef free
#endif

template<typename _T>
class mapi_memory_proxy
{
public:
	typedef _T		value_type;
	typedef _T**	pointerpointer;

	mapi_memory_proxy(pointerpointer lpp) : m_lpp(lpp) {}

	operator pointerpointer() { return m_lpp; }
	operator LPVOID*() { return (LPVOID*)m_lpp; }

private:
	pointerpointer	m_lpp;
};

template <typename _T>
class mapi_memory_ptr
{
public:
	typedef _T						value_type;
	typedef _T*						pointer;
	typedef _T&						reference;
	typedef const _T*				const_pointer;
	typedef const _T&				const_reference;
	typedef mapi_memory_proxy<_T>	proxy;


	// Constructors
	mapi_memory_ptr() : m_lpMemory(NULL) {}
	
	mapi_memory_ptr(pointer lpObject) : m_lpMemory(lpObject) {}


	// Destructor
	~mapi_memory_ptr() {
		if (m_lpMemory) {
			MAPIFreeBuffer(m_lpMemory);
			m_lpMemory = NULL;
		}
	}


	// Assignment
	mapi_memory_ptr& operator=(pointer lpObject) {
		if (m_lpMemory != lpObject) {
			mapi_memory_ptr tmp(lpObject);
			swap(tmp);
		}
		return *this;
	}


	// Dereference
	pointer operator->() { return m_lpMemory; }
	const_pointer operator->() const { return m_lpMemory; }
	
	reference operator*() { return *m_lpMemory; }
	const_reference operator*() const { return *m_lpMemory; }

	
	// Utility
	void free() {
		if (m_lpMemory) {
			MAPIFreeBuffer(m_lpMemory);
			m_lpMemory = NULL;
		}
	}

	void swap(mapi_memory_ptr &other) {
		std::swap(m_lpMemory, other.m_lpMemory);
	}

	bool is_null() const {
		return m_lpMemory == NULL;
	}

	operator pointer() { return m_lpMemory; }
	operator const_pointer() const { return m_lpMemory; }

	pointer operator+(unsigned x) { return m_lpMemory + x; }
	const_pointer operator+(unsigned x) const { return m_lpMemory + x; }

	pointer get() { return m_lpMemory; }
	const_pointer get() const { return m_lpMemory; }

	pointer release() {
		pointer lpTmp = m_lpMemory;
		m_lpMemory = NULL;
		return lpTmp;
	}

	void reset(pointer lpObject) {
		free();
		m_lpMemory = lpObject;
	}

	operator void*() { return m_lpMemory; }
	operator const void*() const { return m_lpMemory; }

	proxy operator&() {
		if (m_lpMemory) {
			MAPIFreeBuffer(m_lpMemory);
			m_lpMemory = NULL;
		}

		return proxy(&m_lpMemory);
	}

	template <typename _U>
	_U as() { return (_U)m_lpMemory; }

	template <typename _U>
	const _U as() const { return (_U)m_lpMemory; }

	operator void**() {
		if (m_lpMemory) {
			MAPIFreeBuffer(m_lpMemory);
			m_lpMemory = NULL;
		}

		return (void**)&m_lpMemory;
	}

	bool operator!() const {
		return m_lpMemory == NULL;
	}
	
private:	// inhibit copying untill refcounting is implemented
	mapi_memory_ptr(const mapi_memory_ptr &other);
	mapi_memory_ptr& operator=(const mapi_memory_ptr &other);

private:
	pointer	m_lpMemory;
};

#endif // ndef mapi_memory_ptr_INCLUDED
