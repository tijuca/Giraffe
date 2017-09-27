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

#ifndef WSABPROPSTORAGE_H
#define WSABPROPSTORAGE_H

#include <kopano/zcdefs.h>
#include <mutex>
#include <kopano/ECUnknown.h>
#include <kopano/Util.h>
#include "IECPropStorage.h"

#include <kopano/kcodes.h>
#include "ECABLogon.h"
#include "WSTableView.h"
#include "WSTransport.h"
#include "soapKCmdProxy.h"

#include <mapi.h>
#include <mapispi.h>

class WSABPropStorage _kc_final : public ECUnknown, public IECPropStorage {
protected:
	WSABPropStorage(ULONG cbEntryId, LPENTRYID, KCmd *, std::recursive_mutex &, ECSESSIONID, WSTransport *);
	virtual ~WSABPropStorage();

public:
	static HRESULT Create(ULONG cbEntryId, LPENTRYID, KCmd *, std::recursive_mutex &, ECSESSIONID, WSTransport *, WSABPropStorage **);
	virtual HRESULT QueryInterface(REFIID refiid, void **lppInterface) _kc_override;
	static HRESULT Reload(void *lpParam, ECSESSIONID sessionId);
	
private:

	// Get a list of the properties
	virtual HRESULT HrReadProps(LPSPropTagArray *lppPropTags,ULONG *cValues, LPSPropValue *ppValues);

	// Get a single (large) property
	virtual HRESULT HrLoadProp(ULONG ulObjId, ULONG ulPropTag, LPSPropValue *lppsPropValue);

	// Write all properties to disk (overwrites a property if it already exists)
	virtual	HRESULT	HrWriteProps(ULONG cValues, LPSPropValue pValues, ULONG ulFlags = 0);

	// Delete properties from file
	virtual HRESULT HrDeleteProps(const SPropTagArray *lpsPropTagArray);

	// Save complete object to disk
	virtual HRESULT HrSaveObject(ULONG ulFlags, MAPIOBJECT *lpsMapiObject);

	// Load complete object from disk
	virtual HRESULT HrLoadObject(MAPIOBJECT **lppsMapiObject);

	virtual IECPropStorage* GetServerStorage();

	virtual HRESULT LockSoap();
	virtual HRESULT UnLockSoap();

private:
	entryId			m_sEntryId;
	KCmd*		lpCmd;
	std::recursive_mutex &lpDataLock;
	ECSESSIONID		ecSessionId;
	WSTransport*	m_lpTransport;
	ULONG			m_ulSessionReloadCallback;
	ALLOC_WRAP_FRIEND;
};

class WSABTableView _kc_final : public WSTableView {
	public:
	static HRESULT Create(ULONG ulType, ULONG ulFlags, KCmd *, std::recursive_mutex &, ECSESSIONID, ULONG cbEntryId, LPENTRYID, ECABLogon *, WSTransport *, WSTableView **);
	virtual	HRESULT	QueryInterface(REFIID refiid, void **lppInterface) _kc_override;

	protected:
	WSABTableView(ULONG ulType, ULONG ulFlags, KCmd *, std::recursive_mutex &, ECSESSIONID, ULONG cbEntryId, LPENTRYID, ECABLogon *, WSTransport *);
	ALLOC_WRAP_FRIEND;
};

#endif
