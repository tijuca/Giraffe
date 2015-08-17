/*
 * Copyright 2005 - 2015  Zarafa B.V. and its licensors
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License, version 3,
 * as published by the Free Software Foundation with the following
 * additional terms according to sec. 7:
 * 
 * "Zarafa" is a registered trademark of Zarafa B.V.
 * The licensing of the Program under the AGPL does not imply a trademark 
 * license. Therefore any rights, title and interest in our trademarks 
 * remain entirely with us.
 * 
 * Our trademark policy (see TRADEMARKS.txt) allows you to use our trademarks
 * in connection with Propagation and certain other acts regarding the Program.
 * In any case, if you propagate an unmodified version of the Program you are
 * allowed to use the term "Zarafa" to indicate that you distribute the Program.
 * Furthermore you may use our trademarks where it is necessary to indicate the
 * intended purpose of a product or service provided you use it in accordance
 * with honest business practices. For questions please contact Zarafa at
 * trademark@zarafa.com.
 *
 * The interactive user interface of the software displays an attribution 
 * notice containing the term "Zarafa" and/or the logo of Zarafa. 
 * Interactive user interfaces of unmodified and modified versions must 
 * display Appropriate Legal Notices according to sec. 5 of the GNU Affero 
 * General Public License, version 3, when you propagate unmodified or 
 * modified versions of the Program. In accordance with sec. 7 b) of the GNU 
 * Affero General Public License, version 3, these Appropriate Legal Notices 
 * must retain the logo of Zarafa or display the words "Initial Development 
 * by Zarafa" if the display of the logo is not reasonably feasible for
 * technical reasons.
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

#include <algorithm>

#include <mapicode.h>
#include <mapidefs.h>
#include <mapiguid.h>

#include "IStreamAdapter.h"

IStreamAdapter::IStreamAdapter(std::string& str) : m_pos(0), m_str(str) {
}

IStreamAdapter::~IStreamAdapter()
{
}

HRESULT IStreamAdapter::QueryInterface(REFIID iid, void **pv){
	if(iid == IID_IStream || iid == IID_ISequentialStream || iid == IID_IUnknown) {
		*pv = this;
		return hrSuccess;
	}
	return MAPI_E_INTERFACE_NOT_SUPPORTED;
}

ULONG IStreamAdapter::AddRef() { return 1; }
ULONG IStreamAdapter::Release() { return 1; }

HRESULT IStreamAdapter::Read(void *pv, ULONG cb, ULONG *pcbRead)
{
	size_t toread = std::min(cb, (ULONG)(m_str.size() - m_pos));
	
	memcpy(pv, m_str.data() + m_pos, toread);
	
	m_pos += toread;
	
	if(pcbRead)
		*pcbRead = toread;
	
	return hrSuccess;
}

HRESULT IStreamAdapter::Write(const void *pv, ULONG cb, ULONG *pcbWritten)
{
	if(m_pos + cb > m_str.size()) {
		m_str.resize(m_pos+cb);
	}
	
	memcpy((void *)(m_str.data() + m_pos), pv, cb);
	
	m_pos += cb;
	
	if(pcbWritten)
		*pcbWritten = cb;
	
	return hrSuccess;
}

HRESULT IStreamAdapter::Seek(LARGE_INTEGER dlibMove, DWORD dwOrigin, ULARGE_INTEGER *plibNewPosition)
{
	if(dwOrigin == SEEK_SET) {
		if (dlibMove.QuadPart < 0)
			m_pos = 0;
		else
			m_pos = dlibMove.QuadPart;
	}
	else if(dwOrigin == SEEK_CUR) {
		if (dlibMove.QuadPart < 0 &&
		    m_pos < static_cast<ULONGLONG>(-dlibMove.QuadPart))
			m_pos = 0;
		else
			m_pos += dlibMove.QuadPart;
	}
	else if(dwOrigin == SEEK_END) {
		if (dlibMove.QuadPart < 0 &&
		    m_str.size() < static_cast<ULONGLONG>(-dlibMove.QuadPart))
			m_pos = 0;
		else
			m_pos = m_str.size() + dlibMove.QuadPart;
	}

	// Fix overflow		
	if (m_pos > m_str.size())
		m_pos = m_str.size();
	
	if (plibNewPosition)
		plibNewPosition->QuadPart = m_pos;
		
	return hrSuccess;
}

HRESULT IStreamAdapter::SetSize(ULARGE_INTEGER libNewSize)
{
	LARGE_INTEGER zero = { { 0 } };
	m_str.resize(libNewSize.QuadPart);
	return Seek(zero, 0, NULL);
}

HRESULT IStreamAdapter::CopyTo(IStream *pstm, ULARGE_INTEGER cb, ULARGE_INTEGER *pcbRead, ULARGE_INTEGER *pcbWritten)
{
	HRESULT hr = hrSuccess;
	
	char buf[4096];
	ULONG len = 0;
	
	while(1) {
		hr = Read(buf, sizeof(buf), &len);
		if(hr != hrSuccess)
			goto exit;
			
		if(len == 0)
			break;
			
		hr = pstm->Write(buf, len, NULL);
		if(hr != hrSuccess)
			goto exit;
	}
	
exit:
	return hr;
}

HRESULT IStreamAdapter::Commit(DWORD grfCommitFlags)
{
	return hrSuccess;
}

HRESULT IStreamAdapter::Revert(void)
{
	return hrSuccess;
}

HRESULT IStreamAdapter::LockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType)
{
	return MAPI_E_NO_SUPPORT;
}

HRESULT IStreamAdapter::UnlockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType)
{
	return MAPI_E_NO_SUPPORT;
}

HRESULT IStreamAdapter::Stat(STATSTG *pstatstg, DWORD grfStatFlag)
{
	memset(pstatstg, 0, sizeof(STATSTG));
	pstatstg->cbSize.QuadPart = m_str.size();
	
	return hrSuccess;
}

HRESULT IStreamAdapter::Clone(IStream **ppstm)
{
	return MAPI_E_NO_SUPPORT;
}

IStream *IStreamAdapter::get()
{
	return this;
}