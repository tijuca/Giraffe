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

// IECMultiStoreTable.h: interface for the IECMultiStoreTable class.
//
//////////////////////////////////////////////////////////////////////

#ifndef IECMULTISTORETABLE
#define IECMULTISTORETABLE

#include <kopano/ECDefs.h>

namespace KC {

class IECMultiStoreTable : public IUnknown {
public:
	/* ulFlags is currently unused */
	virtual HRESULT __stdcall OpenMultiStoreTable(LPENTRYLIST lpMsgList, ULONG ulFlags, LPMAPITABLE *lppTable) = 0;
};

} /* namespace */

#endif
