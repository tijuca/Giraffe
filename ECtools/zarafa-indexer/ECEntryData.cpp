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

#include <mapidefs.h>
#include <mapix.h>

#include <Util.h>

#include "ECEntryData.h"

void release_folderdata(folderdata_list_t::value_type &entry)
{
	entry->Release();
}

ECFolderData::ECFolderData()
{
	m_sFolderSourceKey.cb = 0;
	m_sFolderSourceKey.lpb = NULL;

	m_sFolderEntryId.cb = 0;
	m_sFolderEntryId.lpb = NULL;

	m_lpContentsSyncBase = NULL;
}

HRESULT ECFolderData::Create(ECEntryData *lpEntryData, ECFolderData **lppFolderData)
{
	HRESULT hr = hrSuccess;
	ECFolderData *lpFolderData = NULL;

	try {
		lpFolderData = new ECFolderData();
	}
	catch (...) {
		lpFolderData = NULL;
	}
	if (!lpFolderData) {
		hr = MAPI_E_NOT_ENOUGH_MEMORY;
		goto exit;
	}

	lpEntryData->m_lFolders.push_back(lpFolderData);
	lpFolderData->AddRef();

	if (lppFolderData)
		*lppFolderData = lpFolderData;

exit:
	if ((hr != hrSuccess) && lpFolderData)
		lpFolderData->Release();

	return hr;
}

ECFolderData::~ECFolderData()
{
	if (m_sFolderSourceKey.lpb)
		MAPIFreeBuffer(m_sFolderSourceKey.lpb);
	if (m_sFolderEntryId.lpb)
		MAPIFreeBuffer(m_sFolderEntryId.lpb);

	if (m_lpContentsSyncBase)
		m_lpContentsSyncBase->Release();
}

ECEntryData::ECEntryData()
{
	m_sUserEntryId.cb = 0;
	m_sUserEntryId.lpb = NULL;

	m_sStoreEntryId.cb = 0;
	m_sStoreEntryId.lpb = NULL;

	m_sRootEntryId.cb = 0;
	m_sRootEntryId.lpb = NULL;

	m_sRootSourceKey.cb = 0;
	m_sRootSourceKey.lpb = NULL;

	m_lpHierarchySyncBase = NULL;
}

HRESULT ECEntryData::Create(ECEntryData **lppEntryData)
{
	HRESULT hr = hrSuccess;
	ECEntryData *lpEntryData = NULL;

	try {
		lpEntryData = new ECEntryData();
	}
	catch (...) {
		lpEntryData = NULL;
	}
	if (!lpEntryData) {
		hr = MAPI_E_NOT_ENOUGH_MEMORY;
		goto exit;
	}

	lpEntryData->AddRef();

	if (lppEntryData)
		*lppEntryData = lpEntryData;

exit:
	if ((hr != hrSuccess) && lpEntryData)
		lpEntryData->Release();

	return hr;
}

ECEntryData::~ECEntryData()
{
	if (m_sUserEntryId.lpb)
		MAPIFreeBuffer(m_sUserEntryId.lpb);
	if (m_sStoreEntryId.lpb)
		MAPIFreeBuffer(m_sStoreEntryId.lpb);
	if (m_sRootEntryId.lpb)
		MAPIFreeBuffer(m_sRootEntryId.lpb);
	if (m_sRootSourceKey.lpb)
		MAPIFreeBuffer(m_sRootSourceKey.lpb);

	if (m_lpHierarchySyncBase)
		m_lpHierarchySyncBase->Release();

	for_each(m_lFolders.begin(), m_lFolders.end(), release_folderdata);
}

HRESULT HrAddEntryData(SBinary *lpDst, ECEntryData *lpEntry, SBinary *lpSrc)
{
	HRESULT hr = hrSuccess;

	hr = Util::HrCopyBinary(lpSrc->cb, lpSrc->lpb, &lpDst->cb, &lpDst->lpb);
	if (hr != hrSuccess)
		goto exit;

exit:
	return hr;
}
