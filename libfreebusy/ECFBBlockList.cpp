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
#include "ECFBBlockList.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

ECFBBlockList::ECFBBlockList(void)
{
	m_bInitIter = false;
	m_FBIter = m_FBMap.end();
	m_tmRestictStart = 0;
	m_tmRestictEnd = 0;
}

ECFBBlockList::~ECFBBlockList(void)
{
}

void ECFBBlockList::Copy(ECFBBlockList *lpfbBlkList)
{
	this->m_FBMap = lpfbBlkList->m_FBMap;
	this->Restrict(lpfbBlkList->m_tmRestictStart, lpfbBlkList->m_tmRestictEnd);
}

HRESULT ECFBBlockList::Add(FBBlock_1* lpFBBlock)
{
	HRESULT hr = hrSuccess;

	if(lpFBBlock == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	m_FBMap.insert(mapFB::value_type(lpFBBlock->m_tmStart, *lpFBBlock));

exit:
	return hr;
}

HRESULT ECFBBlockList::Merge(FBBlock_1* lpFBBlock)
{
	HRESULT hr = hrSuccess;
	mapFB::iterator	FBIter;	

	if(lpFBBlock == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	for(FBIter = m_FBMap.begin(); FBIter != m_FBMap.end(); FBIter++)
	{
		if(FBIter->second.m_tmEnd == lpFBBlock->m_tmStart)
		{
			FBIter->second.m_tmEnd = lpFBBlock->m_tmEnd;
			break;
		}
	}

	if(FBIter == m_FBMap.end())
	{
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}

exit:
	return hr;
}

HRESULT ECFBBlockList::Next(FBBlock_1* pblk)
{
	HRESULT hr = hrSuccess;

	if(pblk == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	// Set iter on the begin of the list
	if(m_bInitIter == false) {
		Restrict(m_tmRestictStart, m_tmRestictEnd);
	}

	// Check if you are at the end of the list or the item doesn't matched with the restriction
	if(m_FBIter == m_FBMap.end() || (m_tmRestictEnd != 0 && (ULONG)m_FBIter->second.m_tmStart > (ULONG)m_tmRestictEnd) )
	{
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}

	*pblk = (*m_FBIter).second;
	// blocks before the start time get capped on the start time
	if (pblk->m_tmStart < m_tmRestictStart)
		pblk->m_tmStart = m_tmRestictStart;

	m_FBIter++;

exit:
	return hr;
}

HRESULT ECFBBlockList::Reset()
{
	m_bInitIter = false;

	return hrSuccess;
}

HRESULT ECFBBlockList::Skip(LONG items)
{
	if(m_bInitIter == false) {
		Restrict(m_tmRestictStart, m_tmRestictEnd);
	}

	for(LONG i=0; i < items; i++)
	{
		// Check if you are at the end of the list or the item doesn't matched with the restriction
		if(m_FBIter == m_FBMap.end() || (m_tmRestictEnd != 0 && (ULONG)m_FBIter->second.m_tmStart > (ULONG)m_tmRestictEnd) )
			break; //FIXME: gives a error or always oke?

		m_FBIter++;
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
	while(m_tmRestictStart != 0 && m_FBIter != m_FBMap.end()) {
		
		if( (ULONG)m_FBIter->second.m_tmEnd > (ULONG)m_tmRestictStart )
			break;

		m_FBIter++;
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
	mapFB::iterator	FBIter;

	FBIter = m_FBMap.begin();
	
	// seek to the first matched item
	while(m_tmRestictStart != 0 && FBIter != m_FBMap.end()) {
		
		if( (ULONG)FBIter->second.m_tmEnd > (ULONG)m_tmRestictStart )
			break;

		FBIter++;
	}

	// loop while you reached end of list or doesn't mached with the restriction
	while(FBIter != m_FBMap.end() && (m_tmRestictEnd == 0 || (ULONG)FBIter->second.m_tmStart <= (ULONG)m_tmRestictEnd))
	{
		size++;
		FBIter++;
	}	

	return size;
}

HRESULT ECFBBlockList::GetEndTime(LONG *lprtmEnd)
{
	HRESULT			hr = hrSuccess;
	mapFB::iterator	FBIter;
	LONG			ulEnd = 0;
	bool			bFound = false;

	if(lprtmEnd == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	FBIter = m_FBMap.begin();
	while(FBIter != m_FBMap.end() && (m_tmRestictEnd == 0 || (ULONG)FBIter->second.m_tmStart <= (ULONG)m_tmRestictEnd))
	{
		ulEnd = FBIter->second.m_tmEnd;	
		FBIter++;		
		bFound = true;
	}	

	if(bFound)
		*lprtmEnd = ulEnd;
	else
		hr = MAPI_E_NOT_FOUND;

exit:
	return hr;
}
