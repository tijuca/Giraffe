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

class mapi_rowset_ptr
{
public:
	typedef unsigned	size_type;
	typedef SRow*		pointer;
	typedef SRow&		reference;
	typedef const SRow*	const_pointer;
	typedef const SRow&	const_reference;

	mapi_rowset_ptr(): m_lpsRowSet(NULL) {}
	mapi_rowset_ptr(LPSRowSet lpsRowSet): m_lpsRowSet(lpsRowSet) {}

	mapi_rowset_ptr(const mapi_rowset_ptr &other): m_lpsRowSet(NULL) {
		if (other.m_lpsRowSet) {
			MAPIAllocateBuffer(CbNewSRowSet(other.m_lpsRowSet->cRows), (LPVOID*)&m_lpsRowSet);
			Util::HrCopySRowSet(m_lpsRowSet, other.m_lpsRowSet, m_lpsRowSet);
		}
	}

	mapi_rowset_ptr& operator=(const mapi_rowset_ptr &other) {
		if (&other != this) {
			if (m_lpsRowSet)
				FreeProws(m_lpsRowSet);
			
			if (other.m_lpsRowSet) {
				MAPIAllocateBuffer(CbNewSRowSet(other.m_lpsRowSet->cRows), (LPVOID*)&m_lpsRowSet);
				Util::HrCopySRowSet(m_lpsRowSet, other.m_lpsRowSet, m_lpsRowSet);
			} else
				m_lpsRowSet = NULL;
		}

		return *this;
	}

	~mapi_rowset_ptr() {
		if (m_lpsRowSet)
			FreeProws(m_lpsRowSet);
	}

	void reset(LPSRowSet lpsRowSet = NULL) {
		if (m_lpsRowSet)
			FreeProws(m_lpsRowSet);

		m_lpsRowSet = lpsRowSet;
	}

	LPSRowSet* operator&() {
		if (m_lpsRowSet) {
			FreeProws(m_lpsRowSet);
			m_lpsRowSet = NULL;
		}
		return &m_lpsRowSet;
	}

	size_type size() const { return m_lpsRowSet ? m_lpsRowSet->cRows : 0; }
	bool empty() const { return m_lpsRowSet ? (m_lpsRowSet->cRows == 0) : true; }

	reference operator[](unsigned i) { return m_lpsRowSet->aRow[i]; }
	const_reference operator[](unsigned i) const { return m_lpsRowSet->aRow[i]; }

	reference at(unsigned i) { 
		if (m_lpsRowSet == NULL || i >= m_lpsRowSet->cRows)
			throw std::out_of_range("i");
		return m_lpsRowSet->aRow[i];
	}

	const_reference at(unsigned i) const { 
		if (m_lpsRowSet == NULL || i >= m_lpsRowSet->cRows)
			throw std::out_of_range("i");
		return m_lpsRowSet->aRow[i];
	}

	LPSRowSet get() {
		return m_lpsRowSet;
	}

private:
	LPSRowSet	m_lpsRowSet;
};

#endif // ndef mapi_rowset_ptr_INCLUDED
