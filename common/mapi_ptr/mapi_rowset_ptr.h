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

#ifndef mapi_rowset_ptr_INCLUDED
#define mapi_rowset_ptr_INCLUDED

#include <stdexcept>
#include <mapiutil.h>
#include "Util.h"

namespace details {
	template<typename _T>
	class traits {
	};

	template<>
	class traits<SRow> {
	public:
		typedef LPSRowSet	set_pointer_type;

		static inline size_t CbNew(unsigned cRows) {
			return CbNewSRowSet(cRows);
		}

		static inline HRESULT HrCopySet(set_pointer_type lpDest, set_pointer_type lpSrc, void *lpBase) {
			return Util::HrCopySRowSet(lpDest, lpSrc, lpBase);
		}

		static inline void FreeSet(set_pointer_type lpSet) {
			FreeProws(lpSet);
		}

		static inline ULONG GetSize(set_pointer_type lpSet) {
			return lpSet->cRows;
		}

		static inline SRow& GetElement(set_pointer_type lpSet, unsigned index) {
			return lpSet->aRow[index];
		}
	};

	template<>
	class traits<ADRENTRY> {
	public:
		typedef LPADRLIST	set_pointer_type;

		static inline size_t CbNew(unsigned cRows) {
			return CbNewADRLIST(cRows);
		}

		static inline HRESULT HrCopySet(set_pointer_type lpDest, set_pointer_type lpSrc, void *lpBase) {
			return Util::HrCopySRowSet((LPSRowSet)lpDest, (LPSRowSet)lpSrc, lpBase);	// Dirty, I know
		}

		static inline void FreeSet(set_pointer_type lpSet) {
			FreePadrlist(lpSet);
		}

		static inline ULONG GetSize(set_pointer_type lpSet) {
			return lpSet->cEntries;
		}

		static inline ADRENTRY& GetElement(set_pointer_type lpSet, unsigned index) {
			return lpSet->aEntries[index];
		}
	};
}

template<typename _T>
class mapi_rowset_ptr
{
public:
	typedef unsigned	size_type;
	typedef _T*			pointer;
	typedef _T&			reference;
	typedef const _T*	const_pointer;
	typedef const _T&	const_reference;

	typedef typename details::traits<_T>		_traits;
	typedef typename _traits::set_pointer_type	set_pointer_type;

	mapi_rowset_ptr(): m_lpsRowSet(NULL) {}
	mapi_rowset_ptr(set_pointer_type lpsRowSet): m_lpsRowSet(lpsRowSet) {}

	mapi_rowset_ptr(const mapi_rowset_ptr &other): m_lpsRowSet(NULL) {
		if (other.m_lpsRowSet) {
			MAPIAllocateBuffer(details::traits<_T>::CbNew(other.m_lpsRowSet->cRows), (LPVOID*)&m_lpsRowSet);
			details::traits<_T>::HrCopySet(m_lpsRowSet, other.m_lpsRowSet, m_lpsRowSet);
		}
	}

	mapi_rowset_ptr& operator=(const mapi_rowset_ptr &other) {
		if (&other != this) {
			if (m_lpsRowSet)
				details::traits<_T>::FreeSet(m_lpsRowSet);
			
			if (other.m_lpsRowSet) {
				MAPIAllocateBuffer(details::traits<_T>::CbNew(other.m_lpsRowSet->cRows), (LPVOID*)&m_lpsRowSet);
				details::traits<_T>::HrCopySet(m_lpsRowSet, other.m_lpsRowSet, m_lpsRowSet);
			} else
				m_lpsRowSet = NULL;
		}

		return *this;
	}

	~mapi_rowset_ptr() {
		if (m_lpsRowSet)
			details::traits<_T>::FreeSet(m_lpsRowSet);
	}

	void reset(LPSRowSet lpsRowSet = NULL) {
		if (m_lpsRowSet)
			details::traits<_T>::FreeSet(m_lpsRowSet);

		m_lpsRowSet = lpsRowSet;
	}

	set_pointer_type* operator&() {
		if (m_lpsRowSet) {
			details::traits<_T>::FreeSet(m_lpsRowSet);
			m_lpsRowSet = NULL;
		}
		return &m_lpsRowSet;
	}
	
	set_pointer_type release() {
		set_pointer_type lpsRowSet = m_lpsRowSet;
		m_lpsRowSet = NULL;
		return lpsRowSet;
	}

	size_type size() const { return m_lpsRowSet ? details::traits<_T>::GetSize(m_lpsRowSet) : 0; }
	bool empty() const { return m_lpsRowSet ? (details::traits<_T>::GetSize(m_lpsRowSet) == 0) : true; }

	reference operator[](unsigned i) { return details::traits<_T>::GetElement(m_lpsRowSet, i); }
	const_reference operator[](unsigned i) const { return details::traits<_T>::GetElement(m_lpsRowSet, i); }

	reference at(unsigned i) { 
		if (m_lpsRowSet == NULL || i >= details::traits<_T>::GetSize(m_lpsRowSet))
			throw std::out_of_range("i");
		return details::traits<_T>::GetElement(m_lpsRowSet, i);
	}

	const_reference at(unsigned i) const { 
		if (m_lpsRowSet == NULL || i >= details::traits<_T>::GetSize(m_lpsRowSet))
			throw std::out_of_range("i");
		return details::traits<_T>::GetElement(m_lpsRowSet, i);
	}

	set_pointer_type get() {
		return m_lpsRowSet;
	}

	set_pointer_type operator->() {
		return m_lpsRowSet;
	}

private:
	set_pointer_type	m_lpsRowSet;
};

#endif // ndef mapi_rowset_ptr_INCLUDED
