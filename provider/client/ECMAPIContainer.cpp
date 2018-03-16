/*
 * Copyright 2005 - 2016 Zarafa and its licensors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License, version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <kopano/platform.h>
#include <kopano/memory.hpp>
#include "kcore.hpp"
#include "ECMAPIContainer.h"
#include "ECMAPITable.h"
#include "Mem.h"

#include <kopano/ECGuid.h>
#include <kopano/ECDebug.h>

//#include <edkmdb.h>
#include <kopano/mapiext.h>
#include <mapiutil.h>

using namespace KC;

ECMAPIContainer::ECMAPIContainer(ECMsgStore *lpMsgStore, ULONG ulObjType,
    BOOL fModify, const char *szClassName) :
	ECMAPIProp(lpMsgStore, ulObjType, fModify, NULL, szClassName)
{

}

HRESULT	ECMAPIContainer::QueryInterface(REFIID refiid, void **lppInterface)
{
	REGISTER_INTERFACE2(ECMAPIContainer, this);
	REGISTER_INTERFACE2(ECMAPIProp, this);
	REGISTER_INTERFACE2(ECUnknown, this);
	REGISTER_INTERFACE2(IMAPIContainer, this);
	REGISTER_INTERFACE2(IMAPIProp, this);
	REGISTER_INTERFACE2(IUnknown, this);
	return MAPI_E_INTERFACE_NOT_SUPPORTED;
}

HRESULT ECMAPIContainer::CopyTo(ULONG ciidExclude, LPCIID rgiidExclude,
    const SPropTagArray *lpExcludeProps, ULONG ulUIParam,
    LPMAPIPROGRESS lpProgress, LPCIID lpInterface, void *lpDestObj,
    ULONG ulFlags, SPropProblemArray **lppProblems)
{
	return Util::DoCopyTo(&IID_IMAPIContainer, static_cast<IMAPIContainer *>(this), ciidExclude, rgiidExclude, lpExcludeProps, ulUIParam, lpProgress, lpInterface, lpDestObj, ulFlags, lppProblems);
}

HRESULT ECMAPIContainer::CopyProps(const SPropTagArray *lpIncludeProps,
    ULONG ulUIParam, LPMAPIPROGRESS lpProgress, LPCIID lpInterface,
    void *lpDestObj, ULONG ulFlags, SPropProblemArray **lppProblems)
{
	return Util::DoCopyProps(&IID_IMAPIContainer, static_cast<IMAPIContainer *>(this), lpIncludeProps, ulUIParam, lpProgress, lpInterface, lpDestObj, ulFlags, lppProblems);
}

HRESULT ECMAPIContainer::SetSearchCriteria(LPSRestriction lpRestriction, LPENTRYLIST lpContainerList, ULONG ulSearchFlags)
{
	return MAPI_E_NO_SUPPORT;
}

HRESULT ECMAPIContainer::GetSearchCriteria(ULONG ulFlags, LPSRestriction *lppRestriction, LPENTRYLIST *lppContainerList, ULONG *lpulSearchState)
{
	return MAPI_E_NO_SUPPORT;
}

HRESULT ECMAPIContainer::GetContentsTable(ULONG ulFlags, LPMAPITABLE *lppTable)
{
	HRESULT			hr = hrSuccess;
	object_ptr<ECMAPITable> lpTable;
	object_ptr<WSTableView> lpTableOps;
	std::string		strName = "Contents table";

#ifdef DEBUG
	{
		LPSPropValue lpDisplay;
		HrGetOneProp(this, PR_DISPLAY_NAME_A, &lpDisplay);
		if (lpDisplay != nullptr)
			strName = lpDisplay->Value.lpszA;
	}
#endif
	hr = ECMAPITable::Create(strName.c_str(), this->GetMsgStore()->m_lpNotifyClient, 0, &~lpTable);
	if(hr != hrSuccess)
		return hr;
	hr = this->GetMsgStore()->lpTransport->HrOpenTableOps(MAPI_MESSAGE, ulFlags & (MAPI_UNICODE | SHOW_SOFT_DELETES | MAPI_ASSOCIATED | EC_TABLE_NOCAP), m_cbEntryId, m_lpEntryId, this->GetMsgStore(), &~lpTableOps);
	if(hr != hrSuccess)
		return hr;
	hr = lpTable->HrSetTableOps(lpTableOps, !(ulFlags & MAPI_DEFERRED_ERRORS));

	if(hr != hrSuccess)
		return hr;

	hr = lpTable->QueryInterface(IID_IMAPITable, (void **)lppTable);

	AddChild(lpTable);
	return hr;
}

HRESULT ECMAPIContainer::GetHierarchyTable(ULONG ulFlags, LPMAPITABLE *lppTable)
{
	HRESULT			hr = hrSuccess;
	object_ptr<ECMAPITable> lpTable;
	object_ptr<WSTableView> lpTableOps;
	std::string		strName = "Hierarchy table";
	
#ifdef DEBUG
	{
		LPSPropValue lpDisplay;
		HrGetOneProp(this, PR_DISPLAY_NAME_A, &lpDisplay);
		if (lpDisplay != nullptr)
			strName = lpDisplay->Value.lpszA;
	}
#endif

	hr = ECMAPITable::Create(strName.c_str(), this->GetMsgStore()->m_lpNotifyClient, 0, &~lpTable);
	if(hr != hrSuccess)
		return hr;
	hr = this->GetMsgStore()->lpTransport->HrOpenTableOps(MAPI_FOLDER, ulFlags & (MAPI_UNICODE | SHOW_SOFT_DELETES | CONVENIENT_DEPTH), m_cbEntryId, m_lpEntryId, this->GetMsgStore(), &~lpTableOps);
	if(hr != hrSuccess)
		return hr;
	hr = lpTable->HrSetTableOps(lpTableOps, !(ulFlags & MAPI_DEFERRED_ERRORS));

	if(hr != hrSuccess)
		return hr;

	hr = lpTable->QueryInterface(IID_IMAPITable, (void **)lppTable);

	AddChild(lpTable);
	return hr;
}

HRESULT ECMAPIContainer::OpenEntry(ULONG cbEntryID, const ENTRYID *lpEntryID,
    const IID *lpInterface, ULONG ulFlags, ULONG *lpulObjType,
    IUnknown **lppUnk)
{
	return this->GetMsgStore()->OpenEntry(cbEntryID, lpEntryID, lpInterface, ulFlags, lpulObjType, lppUnk);
}
