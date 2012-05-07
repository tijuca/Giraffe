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

#ifndef ECMessageStreamImporterIStreamAdapter_INCLUDED
#define ECMessageStreamImporterIStreamAdapter_INCLUDED

#include "ECUnknown.h"
#include "WSMessageStreamImporter.h"
#include "mapi_ptr.h"

/**
 * This class wraps a WSMessageStreamImporter object and exposes it as an IStream.
 * The actual import callto the server will be initiated by the first write to the
 * IStream.
 * On commit, the call thread will block until the asynchronous call has completed, and
 * the return value will be returned.
 */
class ECMessageStreamImporterIStreamAdapter : public ECUnknown
{
public:
	static HRESULT Create(WSMessageStreamImporter *lpStreamImporter, IStream **lppStream);

	// IUnknown
	virtual HRESULT QueryInterface(REFIID refiid, void **lppInterface);

	// ISequentialStream
	virtual HRESULT Read(void *pv, ULONG cb, ULONG *pcbRead);
	virtual HRESULT Write(const void *pv, ULONG cb, ULONG *pcbWritten);

	// IStream
	virtual HRESULT Seek(LARGE_INTEGER dlibMove, DWORD dwOrigin, ULARGE_INTEGER *plibNewPosition);
	virtual HRESULT SetSize(ULARGE_INTEGER libNewSize);
	virtual HRESULT CopyTo(IStream *pstm, ULARGE_INTEGER cb, ULARGE_INTEGER *pcbRead, ULARGE_INTEGER *pcbWritten);
	virtual HRESULT Commit(DWORD grfCommitFlags);
	virtual HRESULT Revert(void);
	virtual HRESULT LockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType);
	virtual HRESULT UnlockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType);
	virtual HRESULT Stat(STATSTG *pstatstg, DWORD grfStatFlag);
	virtual HRESULT Clone(IStream **ppstm);

private:
	ECMessageStreamImporterIStreamAdapter(WSMessageStreamImporter *lpStreamImporter);
	~ECMessageStreamImporterIStreamAdapter();

private:
	class xSequentialStream : public ISequentialStream
	{
		// IUnknown
	    virtual ULONG __stdcall AddRef();
	    virtual ULONG __stdcall Release();
	    virtual HRESULT __stdcall QueryInterface(REFIID refiid, void **lppInterface);
		
		// ISequentialStream
		virtual HRESULT __stdcall Read(void *pv, ULONG cb, ULONG *pcbRead);
		virtual HRESULT __stdcall Write(const void *pv, ULONG cb, ULONG *pcbWritten);
	} m_xSequentialStream;

	class xStream : public IStream
	{
		// IUnknown
	    virtual ULONG __stdcall AddRef();
	    virtual ULONG __stdcall Release();
	    virtual HRESULT __stdcall QueryInterface(REFIID refiid, void **lppInterface);
		
		// ISequentialStream
		virtual HRESULT __stdcall Read(void *pv, ULONG cb, ULONG *pcbRead);
		virtual HRESULT __stdcall Write(const void *pv, ULONG cb, ULONG *pcbWritten);
	
		// IStream
	    virtual HRESULT __stdcall Seek(LARGE_INTEGER dlibMove, DWORD dwOrigin, ULARGE_INTEGER *plibNewPosition);
	    virtual HRESULT __stdcall SetSize(ULARGE_INTEGER libNewSize);
	    virtual HRESULT __stdcall CopyTo(IStream *pstm, ULARGE_INTEGER cb, ULARGE_INTEGER *pcbRead, ULARGE_INTEGER *pcbWritten);
	    virtual HRESULT __stdcall Commit(DWORD grfCommitFlags);
	    virtual HRESULT __stdcall Revert(void);
	    virtual HRESULT __stdcall LockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType);
	    virtual HRESULT __stdcall UnlockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType);
	    virtual HRESULT __stdcall Stat(STATSTG *pstatstg, DWORD grfStatFlag);
	    virtual HRESULT __stdcall Clone(IStream **ppstm);
	} m_xStream;

private:
	WSMessageStreamImporterPtr	m_ptrStreamImporter;
	WSMessageStreamSinkPtr		m_ptrSink;
};

typedef mapi_object_ptr<ECMessageStreamImporterIStreamAdapter> ECMessageStreamImporterIStreamAdapterPtr;

#endif // ndef ECMessageStreamImporterIStreamAdapter_INCLUDED
