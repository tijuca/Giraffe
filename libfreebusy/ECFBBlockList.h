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
 * Free/Busy block list
 *
 * @addtogroup libfreebusy
 * @{
 */

#ifndef ECFBBLOCKLIST_H
#define ECFBBLOCKLIST_H

#include "freebusy.h"
#include <map>
#include <kopano/zcdefs.h>

namespace KC {

typedef std::map<LONG, FBBlock_1>mapFB;

class ECFBBlockList _kc_final {
public:
	ECFBBlockList(void);
	ECFBBlockList(const ECFBBlockList &);
	void operator=(const ECFBBlockList &) = delete; /* not implemented */
	HRESULT Add(const FBBlock_1 &);
	HRESULT Next(FBBlock_1* pblk);
	HRESULT Reset();
	HRESULT Skip(LONG items);
	HRESULT Restrict(LONG tmStart, LONG tmEnd);
	void Clear();
	ULONG Size();
	HRESULT Merge(const FBBlock_1 &);
	HRESULT GetEndTime(LONG *rtmEnd);
private:
	mapFB			m_FBMap;
	mapFB::iterator	m_FBIter;
	LONG m_tmRestictStart = 0, m_tmRestictEnd = 0;
	bool m_bInitIter = false;
};

} /* namespace */

#endif // ECFBBLOCKLIST_H
/** @} */
