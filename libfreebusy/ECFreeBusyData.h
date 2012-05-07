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
 * Free/busy data for one user
 *
 * @addtogroup libfreebusy
 * @{
 */

#ifndef ECFREEBUSYDATA_H
#define ECFREEBUSYDATA_H

#include "freebusy.h"
#include "freebusyguid.h"

#include "ECUnknown.h"
#include "Trace.h"
#include "ECDebug.h"
#include "ECGuid.h"

#include "ECFBBlockList.h"

/**
 * Implementatie of the IFreeBusyData interface
 */
class ECFreeBusyData : public ECUnknown
{
private:
	ECFreeBusyData();
	~ECFreeBusyData(void);
public:
	static HRESULT Create(ECFreeBusyData **lppECFreeBusyData);

	HRESULT Init(LONG rtmStart, LONG rtmEnd, ECFBBlockList* lpfbBlockList);
	
	virtual HRESULT QueryInterface(REFIID refiid, void** lppInterface);
	virtual HRESULT Reload(void*);
	virtual HRESULT EnumBlocks(IEnumFBBlock **ppenumfb, FILETIME ftmStart, FILETIME ftmEnd);
	virtual HRESULT Merge(void *);
	virtual HRESULT GetDeligateInfo(void *);
	virtual HRESULT FindFreeBlock(LONG, LONG, LONG, BOOL, LONG, LONG, LONG, FBBlock_1 *);
	virtual HRESULT InterSect(void *, LONG, void *);
	virtual HRESULT SetFBRange(LONG rtmStart, LONG rtmEnd);
	virtual HRESULT NextFBAppt(void *, ULONG, void *, ULONG, void *, void *);
	virtual HRESULT GetFBPublishRange(LONG *prtmStart, LONG *prtmEnd);

public:
	class xFreeBusyData : public IFreeBusyData
	{
		public:
			// From IUnknown
			virtual HRESULT __stdcall QueryInterface(REFIID refiid , void** lppInterface);
			virtual ULONG __stdcall AddRef();
			virtual ULONG __stdcall Release();

			// From IFreeBusyData
			virtual HRESULT __stdcall Reload(void*);
			virtual HRESULT __stdcall EnumBlocks(IEnumFBBlock **ppenumfb, FILETIME ftmStart, FILETIME ftmEnd);
			virtual HRESULT __stdcall Merge(void *);
			virtual HRESULT __stdcall GetDeligateInfo(void *);
			virtual HRESULT __stdcall FindFreeBlock(LONG, LONG, LONG, BOOL, LONG, LONG, LONG, FBBlock_1 *);
			virtual HRESULT __stdcall InterSect(void *, LONG, void *);
			virtual HRESULT __stdcall SetFBRange(LONG rtmStart, LONG rtmEnd);
			virtual HRESULT __stdcall NextFBAppt(void *, ULONG, void *, ULONG, void *, void *);
			virtual HRESULT __stdcall GetFBPublishRange(LONG *prtmStart, LONG *prtmEnd);
	}m_xFreeBusyData;

private:
	ECFBBlockList	m_fbBlockList;
	LONG			m_rtmStart; // PR_FREEBUSY_START_RANGE
	LONG			m_rtmEnd; // PR_FREEBUSY_END_RANGE
};

#endif // ECFREEBUSYDATA_H

/** @} */
