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

#ifndef ECMEMSTREAM_H
#define ECMEMSTREAM_H

#include "zcdefs.h"
#include "ECUnknown.h"

/* The ECMemBlock class is basically a random-access block of data that can be
 * read from and written to, expanded and contracted, and has a Commit and Revert
 * function to save and reload data.
 *
 * The commit and revert functions use memory sparingly, as only changed blocks
 * are held in memory.
 */


class ECMemBlock _final : public ECUnknown {
private:
	ECMemBlock(char *buffer, ULONG ulDataLen, ULONG ulFlags);
	~ECMemBlock();

public:
	static HRESULT	Create(char *buffer, ULONG ulDataLen, ULONG ulFlags, ECMemBlock **lppStream);

	virtual HRESULT QueryInterface(REFIID refiid, void **lppInterface) _override;

	virtual HRESULT	ReadAt(ULONG ulPos, ULONG ulLen, char *buffer, ULONG *ulBytesRead);
	virtual HRESULT WriteAt(ULONG ulPos, ULONG ulLen, char *buffer, ULONG *ulBytesWritten);
	virtual HRESULT Commit();
	virtual HRESULT Revert();
	virtual HRESULT SetSize(ULONG ulSize);
	virtual HRESULT GetSize(ULONG *ulSize);

	virtual char* GetBuffer();

private:
	char *	lpCurrent;
	ULONG	cbCurrent, cbTotal;
	char *	lpOriginal;
	ULONG	cbOriginal;
	ULONG	ulFlags;
};

/* 
 * This is an IStream-compatible wrapper for ECMemBlock
 */

class ECMemStream _final : public ECUnknown {
public:
	typedef HRESULT (*CommitFunc)(IStream *lpStream, void *lpParam);
	typedef HRESULT (*DeleteFunc)(void *lpParam); /* Caller's function to remove lpParam data from memory */

private:
	ECMemStream(char *buffer, ULONG ulDAtaLen, ULONG ulFlags, CommitFunc lpCommitFunc, DeleteFunc lpDeleteFunc, void *lpParam);
	ECMemStream(ECMemBlock *lpMemBlock, ULONG ulFlags, CommitFunc lpCommitFunc, DeleteFunc lpDeleteFunc, void *lpParam);
	~ECMemStream();

public:
	static  HRESULT	Create(char *buffer, ULONG ulDataLen, ULONG ulFlags, CommitFunc lpCommitFunc, DeleteFunc lpDeleteFunc,
						   void *lpParam, ECMemStream **lppStream);
	static  HRESULT	Create(ECMemBlock *lpMemBlock, ULONG ulFlags, CommitFunc lpCommitFunc, DeleteFunc lpDeleteFunc,
						   void *lpParam, ECMemStream **lppStream);

	virtual ULONG Release(void) _override;
	virtual HRESULT QueryInterface(REFIID refiid, void **lppInterface) _override;

	virtual HRESULT Read(void *pv, ULONG cb, ULONG *pcbRead);
	virtual HRESULT Write(const void *pv, ULONG cb, ULONG *pcbWritten);
	virtual HRESULT Seek(LARGE_INTEGER dlibmove, DWORD dwOrigin, ULARGE_INTEGER *plibNewPosition);
	virtual HRESULT SetSize(ULARGE_INTEGER libNewSize);
	virtual HRESULT CopyTo(IStream *pstm, ULARGE_INTEGER cb, ULARGE_INTEGER *pcbRead, ULARGE_INTEGER *pcbWritten);
	virtual HRESULT Commit(DWORD grfCommitFlags);
	virtual HRESULT Revert();
	virtual HRESULT LockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType);
	virtual HRESULT UnlockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType);
	virtual HRESULT Stat(STATSTG *pstatstg, DWORD grfStatFlag);
	virtual HRESULT Clone(IStream **ppstm);

	virtual ULONG GetSize();
	virtual char* GetBuffer();

	class xStream _final : public IStream {
		// AddRef and Release from ECUnknown
		virtual ULONG   __stdcall AddRef(void) _override;
		virtual ULONG   __stdcall Release(void) _override;
		virtual HRESULT __stdcall QueryInterface(REFIID refiid, LPVOID *lppInterface) _override;

		virtual HRESULT __stdcall Read(void *pv, ULONG cb, ULONG *pcbRead) _override;
		virtual HRESULT __stdcall Write(const void *pv, ULONG cb, ULONG *pcbWritten) _override;
		virtual HRESULT __stdcall Seek(LARGE_INTEGER dlibmove, DWORD dwOrigin, ULARGE_INTEGER *plibNewPosition) _override;
		virtual HRESULT __stdcall SetSize(ULARGE_INTEGER libNewSize) _override;
		virtual HRESULT __stdcall CopyTo(IStream *pstm, ULARGE_INTEGER cb, ULARGE_INTEGER *pcbRead, ULARGE_INTEGER *pcbWritten) _override;
		virtual HRESULT __stdcall Commit(DWORD grfCommitFlags) _override;
		virtual HRESULT __stdcall Revert() _override;
		virtual HRESULT __stdcall LockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType) _override;
		virtual HRESULT __stdcall UnlockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType) _override;
		virtual HRESULT __stdcall Stat(STATSTG *pstatstg, DWORD grfStatFlag) _override;
		virtual HRESULT __stdcall Clone(IStream **ppstm) _override;
	} m_xStream;

private:
	ULARGE_INTEGER	liPos;
	ECMemBlock		*lpMemBlock;
	CommitFunc		lpCommitFunc;
	DeleteFunc		lpDeleteFunc;
	void *			lpParam;
	BOOL			fDirty;
	ULONG			ulFlags;
};

#endif // ECMEMSTREAM_H
