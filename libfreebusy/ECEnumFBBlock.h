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

/**
 * @file
 * Free/busy data blocks
 *
 * @addtogroup libfreebusy
 * @{
 */

#ifndef ECENUMFBBLOCK_H
#define ECENUMFBBLOCK_H

#include "freebusy.h"
#include "ECUnknown.h"
#include "Trace.h"
#include "ECDebug.h"
#include "ECGuid.h"
#include "freebusyguid.h"

#include "ECFBBlockList.h"

/**
 * Implementatie of the IEnumFBBlock interface
 */
class ECEnumFBBlock : public ECUnknown
{
private:
	ECEnumFBBlock(ECFBBlockList* lpFBBlock);
	~ECEnumFBBlock(void);
public:
	static HRESULT Create(ECFBBlockList* lpFBBlock, ECEnumFBBlock **lppECEnumFBBlock);
	
	virtual HRESULT QueryInterface(REFIID refiid, void** lppInterface);
	virtual HRESULT Next(LONG celt, FBBlock_1 *pblk, LONG *pcfetch);
	virtual HRESULT Skip(LONG celt);
	virtual HRESULT Reset();
	virtual HRESULT Clone(IEnumFBBlock **ppclone);
	virtual HRESULT Restrict(FILETIME ftmStart, FILETIME ftmEnd);

public:
	/* IEnumFBBlock wrapper class */
	class xEnumFBBlock : public IEnumFBBlock
	{
		public:
			// From IUnknown
			virtual HRESULT __stdcall QueryInterface(REFIID refiid, void** lppInterface);
			virtual ULONG __stdcall AddRef();
			virtual ULONG __stdcall Release();

			// From IEnumFBBlock
			virtual HRESULT __stdcall Next(LONG celt, FBBlock_1 *pblk, LONG *pcfetch);
			virtual HRESULT __stdcall Skip(LONG celt);
			virtual HRESULT __stdcall Reset();
			virtual HRESULT __stdcall Clone(IEnumFBBlock **ppclone);
			virtual HRESULT __stdcall Restrict(FILETIME ftmStart, FILETIME ftmEnd);
	}m_xEnumFBBlock;

	ECFBBlockList	m_FBBlock; /**< Freebusy time blocks */
};

#endif // ECENUMFBBLOCK_H
/** @} */
