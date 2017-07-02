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

#ifndef ECABPROVIDER
#define ECABPROVIDER

#include <kopano/zcdefs.h>
#include <kopano/ECUnknown.h>
#include <mapispi.h>

class ZCABProvider _kc_final : public ECUnknown  {
protected:
	ZCABProvider(ULONG ulFlags, const char *szClassName);

public:
	static  HRESULT Create(ZCABProvider **lppZCABProvider);
	virtual HRESULT QueryInterface(REFIID refiid, void **lppInterface) _kc_override;
    virtual HRESULT Shutdown(ULONG * lpulFlags);
	virtual HRESULT Logon(LPMAPISUP lpMAPISup, ULONG ulUIParam, LPTSTR lpszProfileName, ULONG ulFlags, ULONG * lpulcbSecurity, LPBYTE * lppbSecurity, LPMAPIERROR * lppMAPIError, LPABLOGON * lppABLogon);

private:
	class xABProvider _kc_final : public IABProvider {
		#include <kopano/xclsfrag/IUnknown.hpp>
		#include <kopano/xclsfrag/IABProvider.hpp>
	} m_xABProvider;
};

#endif
