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
 *
 */

#include <kopano/platform.h>
#include "ECFBBlockList.h"

namespace KC {

ECFBBlockList::ECFBBlockList(void)
{
	m_bInitIter = false;
	m_FBIter = m_FBMap.end();
	m_tmRestictStart = 0;
	m_tmRestictEnd = 0;
}

void ECFBBlockList::Copy(ECFBBlockList *lpfbBlkList)
{
	this->m_FBMap = lpfbBlkList->m_FBMap;
	this->Restrict(lpfbBlkList->m_tmRestictStart, lpfbBlkList->m_tmRestictEnd);
}

HRESULT ECFBBlockList::Add(FBBlock_1* lpFBBlock)
{
	if (lpFBBlock == NULL)
		return MAPI_E_INVALID_PARAMETER;
	m_FBMap.insert(mapFB::value_type(lpFBBlock->m_tmStart, *lpFBBlock));
	return hrSuccess;
}

HRESULT ECFBBlockList::Merge(FBBlock_1* lpFBBlock)
{
	if (lpFBBlock == NULL)
		return MAPI_E_INVALID_PARAMETER;

	auto FBIter = m_FBMap.begin();
	for (; FBIter != m_FBMap.cend(); ++FBIter)
		if(FBIter->second.m_tmEnd == lpFBBlock->m_tmStart)
		{
			FBIter->second.m_tmEnd = lpFBBlock->m_tmEnd;
			break;
		}

	if (FBIter == m_FBMap.cend())
		return MAPI_E_NOT_FOUND;
	return hrSuccess;
}

HRESULT ECFBBlockList::Next(FBBlock_1* pblk)
{
	if (pblk == NULL)
		return MAPI_E_INVALID_PARAMETER;

	// Set iter on the begin of the list
	if (!m_bInitIter)
		Restrict(m_tmRestictStart, m_tmRestictEnd);

	// Check if you are at the end of the list or the item doesn't matched with the restriction
	if (m_FBIter == m_FBMap.cend() || (m_tmRestictEnd != 0 && static_cast<ULONG>(m_FBIter->second.m_tmStart) > static_cast<ULONG>(m_tmRestictEnd)))
		return MAPI_E_NOT_FOUND;

	*pblk = (*m_FBIter).second;
	// blocks before the start time get capped on the start time
	if (pblk->m_tmStart < m_tmRestictStart)
		pblk->m_tmStart = m_tmRestictStart;

	++m_FBIter;
	return hrSuccess;
}

HRESULT ECFBBlockList::Reset()
{
	m_bInitIter = false;

	return hrSuccess;
}

HRESULT ECFBBlockList::Skip(LONG items)
{
	if (!m_bInitIter)
		Restrict(m_tmRestictStart, m_tmRestictEnd);

	for (LONG i = 0; i < items; ++i) {
		// Check if you are at the end of the list or the item doesn't matched with the restriction
		if (m_FBIter == m_FBMap.cend() || (m_tmRestictEnd != 0 && (ULONG)m_FBIter->second.m_tmStart > (ULONG)m_tmRestictEnd) )
			break; //FIXME: gives a error or always oke?
		++m_FBIter;
	}

	return hrSuccess;
}

HRESULT ECFBBlockList::Restrict(LONG tmStart, LONG tmEnd)
{
	m_tmRestictStart = tmStart;
	m_tmRestictEnd = tmEnd;

	m_FBIter = m_FBMap.begin();
	m_bInitIter = true;

	// seek to the first matched item
	while (m_tmRestictStart != 0 && m_FBIter != m_FBMap.cend()) {
		if( (ULONG)m_FBIter->second.m_tmEnd > (ULONG)m_tmRestictStart )
			break;
		++m_FBIter;
	}

	return S_OK;
}

void ECFBBlockList::Clear()
{
	m_FBMap.clear();
	m_FBIter = m_FBMap.end();
	m_bInitIter = false;
	m_tmRestictStart = 0;
	m_tmRestictEnd = 0;
}

/*
	Get the size of fbBlocks, restriction proof

*/
ULONG ECFBBlockList::Size()
{
	ULONG			size = 0;
	auto FBIter = m_FBMap.cbegin();
	
	// seek to the first matched item
	while (m_tmRestictStart != 0 && FBIter != m_FBMap.cend()) {
		if( (ULONG)FBIter->second.m_tmEnd > (ULONG)m_tmRestictStart )
			break;
		++FBIter;
	}

	// loop while you reached end of list or doesn't mached with the restriction
	while (FBIter != m_FBMap.cend() && (m_tmRestictEnd == 0 || static_cast<ULONG>(FBIter->second.m_tmStart) <= static_cast<ULONG>(m_tmRestictEnd))) {
		++size;
		++FBIter;
	}	

	return size;
}

HRESULT ECFBBlockList::GetEndTime(LONG *lprtmEnd)
{
	LONG			ulEnd = 0;
	bool			bFound = false;

	if (lprtmEnd == NULL)
		return MAPI_E_INVALID_PARAMETER;

	auto FBIter = m_FBMap.cbegin();
	while (FBIter != m_FBMap.cend() && (m_tmRestictEnd == 0 || static_cast<ULONG>(FBIter->second.m_tmStart) <= static_cast<ULONG>(m_tmRestictEnd))) {
		ulEnd = FBIter->second.m_tmEnd;	
		++FBIter;
		bFound = true;
	}	

	if (!bFound)
		return MAPI_E_NOT_FOUND;
	*lprtmEnd = ulEnd;
	return hrSuccess;
}

} /* namespace */
