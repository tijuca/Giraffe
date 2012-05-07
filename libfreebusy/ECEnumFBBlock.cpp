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
#include "ECEnumFBBlock.h"
#include "freebusyutil.h"
#include "stringutil.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/**
 * Constructor
 *
 * @param[in] lpFBBlock Pointer to a list of free/busy blocks
 */
ECEnumFBBlock::ECEnumFBBlock(ECFBBlockList* lpFBBlock)
{
	FBBlock_1 sBlock;

	lpFBBlock->Reset();

	while(lpFBBlock->Next(&sBlock) == hrSuccess)
		m_FBBlock.Add(&sBlock);
}

/**
 * Destructor
 */
ECEnumFBBlock::~ECEnumFBBlock(void)
{
}

/**
 * Create ECEnumFBBlock object
 * 
 * @param[in]	lpFBBlock		Pointer to a list of free/busy blocks
 * @param[out]	lppEnumFBBlock	Address of the pointer that receives the object ECEnumFBBlock pointer
 *
 * @return HRESULT
 */
HRESULT ECEnumFBBlock::Create(ECFBBlockList* lpFBBlock, ECEnumFBBlock **lppEnumFBBlock)
{
	HRESULT hr = hrSuccess;
	ECEnumFBBlock *lpEnumFBBlock = NULL;

	lpEnumFBBlock = new ECEnumFBBlock(lpFBBlock);

	hr = lpEnumFBBlock->QueryInterface(IID_ECEnumFBBlock, (void **)lppEnumFBBlock);

	if(hr != hrSuccess)
		delete lpEnumFBBlock;

	return hr;
}

/**
 * This method returns a pointer to a specified interface on an object to which a client 
 * currently holds an interface pointer.
 *
 * @param[in]	iid			Identifier of the interface being requested. 
 * @param[out]	ppvObject	Address of the pointer that receives the interface pointer requested in riid.
 *
 * @retval hrSuccess						Indicates that the interface is supported.
 * @retval MAPI_E_INTERFACE_NOT_SUPPORTED	Indicates that the interface is not supported.
 */
HRESULT ECEnumFBBlock::QueryInterface(REFIID refiid , void** lppInterface)
{
	REGISTER_INTERFACE(IID_ECEnumFBBlock, this);
	REGISTER_INTERFACE(IID_ECUnknown, this);

	REGISTER_INTERFACE(IID_IEnumFBBlock, &this->m_xEnumFBBlock);
	REGISTER_INTERFACE(IID_IUnknown, &this->m_xEnumFBBlock);

	return MAPI_E_INTERFACE_NOT_SUPPORTED;
}

/*! @copydoc IEnumFBBlock::Next */
HRESULT ECEnumFBBlock::Next(LONG celt, FBBlock_1 *pblk, LONG *pcfetch)
{

	LONG cEltFound = 0;

	for(LONG i=0; i < celt; i++)
	{
		if(m_FBBlock.Next(&pblk[i]) != hrSuccess)
			break;

		cEltFound++;
	}

	if(pcfetch)
		*pcfetch = cEltFound;

	if(cEltFound == 0)
		return S_FALSE;
	else
		return S_OK;
}

/*! @copydoc IEnumFBBlock::Skip */
HRESULT ECEnumFBBlock::Skip(LONG celt)
{
	return m_FBBlock.Skip(celt);
}

/*! @copydoc IEnumFBBlock::Reset */
HRESULT ECEnumFBBlock::Reset()
{
	return m_FBBlock.Reset();
}

/*! @copydoc IEnumFBBlock::Clone */
HRESULT ECEnumFBBlock::Clone(IEnumFBBlock **ppclone)
{
	return E_NOTIMPL;
}

/*! @copydoc IEnumFBBlock::Restrict */
HRESULT ECEnumFBBlock::Restrict(FILETIME ftmStart, FILETIME ftmEnd)
{
	LONG rtmStart = 0;
	LONG rtmEnd = 0;

	FileTimeToRTime(&ftmStart, &rtmStart);
	FileTimeToRTime(&ftmEnd, &rtmEnd);

	return m_FBBlock.Restrict(rtmStart, rtmEnd);
}

//////////////////////////////////////////////////////////////////
// Interfaces
//		IUnknown
//		IEnumFBBlock
//

HRESULT __stdcall ECEnumFBBlock::xEnumFBBlock::QueryInterface(REFIID refiid , void** lppInterface)
{
	TRACE_MAPI(TRACE_ENTRY, "IEnumFBBlock::QueryInterface", "");
	METHOD_PROLOGUE_(ECEnumFBBlock , EnumFBBlock);
	HRESULT hr = pThis->QueryInterface(refiid, lppInterface);
	TRACE_MAPI(TRACE_RETURN, "IEnumFBBlock::QueryInterface", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

ULONG __stdcall ECEnumFBBlock::xEnumFBBlock::AddRef()
{
	TRACE_MAPI(TRACE_ENTRY, "IEnumFBBlock::AddRef", "");
	METHOD_PROLOGUE_(ECEnumFBBlock , EnumFBBlock);
	return pThis->AddRef();
}

ULONG __stdcall ECEnumFBBlock::xEnumFBBlock::Release()
{
	TRACE_MAPI(TRACE_ENTRY, "IEnumFBBlock::Release", "");
	METHOD_PROLOGUE_(ECEnumFBBlock , EnumFBBlock);
	return pThis->Release();
}

HRESULT __stdcall ECEnumFBBlock::xEnumFBBlock::Next(LONG celt, FBBlock_1 *pblk, LONG *pcfetch)
{
	TRACE_MAPI(TRACE_ENTRY, "IEnumFBBlock::Next", "celt=%d", celt);
	METHOD_PROLOGUE_(ECEnumFBBlock , EnumFBBlock);
	HRESULT hr = pThis->Next(celt, pblk, pcfetch);
	TRACE_MAPI(TRACE_RETURN, "IEnumFBBlock::Next", "%s\n %s", GetDebugFBBlock((pcfetch?(*pcfetch) : (celt == 1 && hr == hrSuccess)?1:0), pblk).c_str(), GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT __stdcall ECEnumFBBlock::xEnumFBBlock::Skip(LONG celt)
{
	TRACE_MAPI(TRACE_ENTRY, "IEnumFBBlock::Skip", "celt=%d", celt);
	METHOD_PROLOGUE_(ECEnumFBBlock , EnumFBBlock);
	HRESULT hr = pThis->Skip(celt);
	TRACE_MAPI(TRACE_RETURN, "IEnumFBBlock::Skip", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT __stdcall ECEnumFBBlock::xEnumFBBlock::Reset()
{
	TRACE_MAPI(TRACE_ENTRY, "IEnumFBBlock::Reset", "");
	METHOD_PROLOGUE_(ECEnumFBBlock , EnumFBBlock);
	HRESULT hr = pThis->Reset();
	TRACE_MAPI(TRACE_RETURN, "IEnumFBBlock::Reset", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT __stdcall ECEnumFBBlock::xEnumFBBlock::Clone(IEnumFBBlock **ppclone)
{
	TRACE_MAPI(TRACE_ENTRY, "IEnumFBBlock::Clone", "");
	METHOD_PROLOGUE_(ECEnumFBBlock , EnumFBBlock);
	HRESULT hr = pThis->Clone(ppclone);
	TRACE_MAPI(TRACE_RETURN, "IEnumFBBlock::Clone", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT __stdcall ECEnumFBBlock::xEnumFBBlock::Restrict(FILETIME ftmStart, FILETIME ftmEnd)
{
	TRACE_MAPI(TRACE_ENTRY, "IEnumFBBlock::Restrict", "");
	METHOD_PROLOGUE_(ECEnumFBBlock , EnumFBBlock);
	HRESULT hr = pThis->Restrict(ftmStart, ftmEnd);
	TRACE_MAPI(TRACE_RETURN, "IEnumFBBlock::Restrict", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}
