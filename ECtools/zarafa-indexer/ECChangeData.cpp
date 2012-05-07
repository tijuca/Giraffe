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

#include <platform.h>

#include <algorithm>

#include <mapi.h>
#include <mapix.h>
#include <mapiutil.h>
#include <mapidefs.h>
#include <mapiguid.h>

#include <CommonUtil.h>

#include "ECChangeData.h"
#include "zarafa-indexer.h"

static inline bool sourceid_lt(const sourceid_list_t::value_type &left, const sourceid_list_t::value_type &right)
{
	return (left->lpProps[0].Value.bin.lpb && right->lpProps[0].Value.bin.lpb) && (left->lpProps[0].Value.bin < right->lpProps[0].Value.bin);
}

static inline bool sourceid_eq(const sourceid_list_t::value_type &left, const sourceid_list_t::value_type &right)
{
	return (left->lpProps[0].Value.bin.lpb && right->lpProps[0].Value.bin.lpb) && (left->lpProps[0].Value.bin == right->lpProps[0].Value.bin);
}

static inline void sourceid_free(sourceid_list_t::value_type &entry)
{
	if (entry->lpStream)
		entry->lpStream->Release();
	MAPIFreeBuffer(entry);
}

struct findsourceid_if {
	sourceid_list_t::value_type m_value;

	findsourceid_if(sourceid_list_t::value_type value)
		: m_value(value) {}

	bool operator()(const sourceid_list_t::value_type &entry)
	{
		return sourceid_eq(m_value, entry);
	}
};

ECChanges::ECChanges()
{
}

ECChanges::~ECChanges()
{
	for_each(lCreate.begin(), lCreate.end(), sourceid_free);
	for_each(lChange.begin(), lChange.end(), sourceid_free);
	for_each(lDelete.begin(), lDelete.end(), sourceid_free);
}

VOID ECChanges::Sort()
{
	lCreate.sort(sourceid_lt);
	lChange.sort(sourceid_lt);
	lDelete.sort(sourceid_lt);
}

// The default unique from STL cannot call a free() routine,
// so we copy/paste and do the free too
static inline void unique_free(sourceid_list_t &lstSourceIds)
{
	sourceid_list_t::iterator __first = lstSourceIds.begin();
	sourceid_list_t::iterator __last = lstSourceIds.end();
	if (__first == __last)
		return;
	sourceid_list_t::iterator __next = __first;
	while (++__next != __last)
	{
		if (sourceid_eq(*__first, *__next)) {
			sourceid_free(*__next);
			lstSourceIds.erase(__next);
		} else
			__first = __next;
		__next = __first;
	}
}

static inline void erase_if(sourceid_list_t &list, sourceid_list_t::value_type &search)
{
	sourceid_list_t::iterator found;

	found = find_if(list.begin(), list.end(), findsourceid_if(search));
	if (found != list.end()) {
		sourceid_free(*found);
		list.erase(found);
	}
}

VOID ECChanges::Unique()
{
	unique_free(lCreate);
	unique_free(lChange);
	unique_free(lDelete);
}

VOID ECChanges::Filter()
{
	for (sourceid_list_t::iterator iter = lDelete.begin(); iter != lDelete.end(); iter++) {
		erase_if(lCreate, *iter);
		erase_if(lChange, *iter);
	}
}

ECChangeData::ECChangeData()
{
	pthread_mutex_init(&m_hThreadLock, NULL);

	m_ulCreate = 0;
	m_ulChange = 0;
	m_ulDelete = 0;
	m_ulTotalCreate = 0;
	m_ulTotalChange = 0;
	m_ulTotalDelete = 0;
}

ECChangeData::~ECChangeData()
{
	pthread_mutex_destroy(&m_hThreadLock);
}

HRESULT ECChangeData::Create(ECChangeData **lppChanges)
{
	HRESULT hr = hrSuccess;
	ECChangeData *lpChanges = NULL;

	try {
		lpChanges = new ECChangeData();
	}
	catch (...) {
		lpChanges = NULL;
	}
	if (!lpChanges) {
		hr = MAPI_E_NOT_ENOUGH_MEMORY;
		goto exit;
	}

	lpChanges->AddRef();

	if (lppChanges)
		*lppChanges = lpChanges;

exit:
	if ((hr != hrSuccess) && lpChanges)
		lpChanges->Release();

	return hr;
}

VOID ECChangeData::Size(ULONG *lpCreate, ULONG *lpChange, ULONG *lpDelete)
{
	if (lpCreate)
		*lpCreate = m_ulTotalCreate;
	if (lpChange)
		*lpChange = m_ulTotalChange;
	if (lpDelete)
		*lpDelete = m_ulTotalDelete;
}

VOID ECChangeData::CurrentSize(ULONG *lpCreate, ULONG *lpChange, ULONG *lpDelete)
{
	if (lpCreate)
		*lpCreate = m_ulCreate;
	if (lpChange)
		*lpChange = m_ulChange;
	if (lpDelete)
		*lpDelete = m_ulDelete;
}

VOID ECChangeData::InsertCreate(sourceid_list_t::value_type &sEntry)
{
	pthread_mutex_lock(&m_hThreadLock);
	m_sChanges.lCreate.push_back(sEntry);
	m_ulCreate++;
	m_ulTotalCreate++;
	pthread_mutex_unlock(&m_hThreadLock);
}

VOID ECChangeData::InsertChange(sourceid_list_t::value_type &sEntry)
{
	pthread_mutex_lock(&m_hThreadLock);
	m_sChanges.lChange.push_back(sEntry);
	m_ulChange++;
	m_ulTotalChange++;
	pthread_mutex_unlock(&m_hThreadLock);					
}

VOID ECChangeData::InsertDelete(sourceid_list_t::value_type &sEntry)
{
	pthread_mutex_lock(&m_hThreadLock);
	m_sChanges.lDelete.push_back(sEntry);
	m_ulDelete++;
	m_ulTotalDelete++;
	pthread_mutex_unlock(&m_hThreadLock);
}

HRESULT ECChangeData::ClaimChanges(ECChanges *lpChanges)
{
	HRESULT hr = hrSuccess;

	if (!lpChanges) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	pthread_mutex_lock(&m_hThreadLock);

	lpChanges->lCreate.assign(m_sChanges.lCreate.begin(), m_sChanges.lCreate.end());
	m_sChanges.lCreate.clear();
	m_ulCreate = 0;

	lpChanges->lChange.assign(m_sChanges.lChange.begin(), m_sChanges.lChange.end());
	m_sChanges.lChange.clear();
	m_ulChange = 0;

	lpChanges->lDelete.assign(m_sChanges.lDelete.begin(), m_sChanges.lDelete.end());
	m_sChanges.lDelete.clear();
	m_ulDelete = 0;

	pthread_mutex_unlock(&m_hThreadLock);

exit:
	return hr;
}
