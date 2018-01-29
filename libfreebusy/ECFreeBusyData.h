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

/**
 * @file
 * Free/busy data for one user
 *
 * @addtogroup libfreebusy
 * @{
 */

#ifndef ECFREEBUSYDATA_H
#define ECFREEBUSYDATA_H

#include <kopano/zcdefs.h>
#include "freebusy.h"
#include "freebusyguid.h"

#include <kopano/ECUnknown.h>
#include <kopano/ECDebug.h>
#include <kopano/ECGuid.h>
#include <kopano/Util.h>
#include "ECFBBlockList.h"

namespace KC {

/**
 * Implementatie of the IFreeBusyData interface
 */
class ECFreeBusyData _kc_final : public ECUnknown, public IFreeBusyData {
public:
	static HRESULT Create(LONG start, LONG end, const ECFBBlockList &, ECFreeBusyData **);
	virtual HRESULT QueryInterface(REFIID refiid, void **lppInterface) _kc_override;
	virtual HRESULT Reload(void *) { return E_NOTIMPL; }
	virtual HRESULT EnumBlocks(IEnumFBBlock **ppenumfb, FILETIME ftmStart, FILETIME ftmEnd);
	virtual HRESULT Merge(void *) { return E_NOTIMPL; }
	virtual HRESULT GetDelegateInfo(void *) { return E_NOTIMPL; }
	virtual HRESULT FindFreeBlock(LONG, LONG, LONG, BOOL, LONG, LONG, LONG, FBBlock_1 *);
	virtual HRESULT InterSect(void *, LONG, void *) { return E_NOTIMPL; }
	virtual HRESULT SetFBRange(LONG rtmStart, LONG rtmEnd);
	virtual HRESULT NextFBAppt(void *, ULONG, void *, ULONG, void *, void *) { return E_NOTIMPL; }
	virtual HRESULT GetFBPublishRange(LONG *prtmStart, LONG *prtmEnd);

private:
	ECFreeBusyData(LONG start, LONG end, const ECFBBlockList &);

	ECFBBlockList	m_fbBlockList;
	LONG m_rtmStart = 0, m_rtmEnd = 0; /* PR_FREEBUSY_START_RANGE, PR_FREEBUSY_END_RANGE */
	ALLOC_WRAP_FRIEND;
};

} /* namespace */

#endif // ECFREEBUSYDATA_H

/** @} */
