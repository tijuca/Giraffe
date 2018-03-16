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

#ifndef WSMAPIPROPSTORAGE_H
#define WSMAPIPROPSTORAGE_H

#include <kopano/zcdefs.h>
#include <mutex>
#include <kopano/ECUnknown.h>
#include <kopano/Util.h>
#include "IECPropStorage.h"

#include <kopano/kcodes.h>
#include "WSTransport.h"

#include <mapi.h>
#include <mapispi.h>

namespace KC {
class convert_context;
}

class KCmdProxy;

class WSMAPIPropStorage _kc_final : public ECUnknown, public IECPropStorage {
protected:
	WSMAPIPropStorage(ULONG cbParentEntryId, LPENTRYID lpParentEntryId, ULONG cbEntryId, LPENTRYID, ULONG ulFlags, KCmdProxy *, std::recursive_mutex &, ECSESSIONID, unsigned int ulServerCapabilities, WSTransport *);
	virtual ~WSMAPIPropStorage();

public:
	static HRESULT Create(ULONG cbParentEntryId, LPENTRYID lpParentEntryId, ULONG cbEntryId, LPENTRYID, ULONG ulFlags, KCmdProxy * , std::recursive_mutex &, ECSESSIONID, unsigned int ulServerCapabilities, WSTransport *, WSMAPIPropStorage **);
	virtual HRESULT QueryInterface(REFIID refiid, void **lppInterface) _kc_override;

	// For ICS
	virtual HRESULT HrSetSyncId(ULONG ulSyncId);

	// Register advise on load object
	virtual HRESULT RegisterAdvise(ULONG ulEventMask, ULONG ulConnection);

	virtual HRESULT GetEntryIDByRef(ULONG *lpcbEntryID, LPENTRYID *lppEntryID);

private:

	// Get a single (large) property
	virtual HRESULT HrLoadProp(ULONG ulObjId, ULONG ulPropTag, LPSPropValue *lppsPropValue);

	// Save complete object to server
	virtual HRESULT HrSaveObject(ULONG flags, MAPIOBJECT *lpSavedObjects);

	// Load complete object from server
	virtual HRESULT HrLoadObject(MAPIOBJECT **lppsMapiObject);
	virtual IECPropStorage *GetServerStorage() override { return this; }

	/* very private */
	virtual ECRESULT EcFillPropTags(const struct saveObject *, MAPIOBJECT *);
	virtual ECRESULT EcFillPropValues(const struct saveObject *, MAPIOBJECT *);
	virtual HRESULT HrMapiObjectToSoapObject(const MAPIOBJECT *, struct saveObject *, convert_context *);
	virtual HRESULT HrUpdateSoapObject(const MAPIOBJECT *, struct saveObject *, convert_context *);
	virtual void    DeleteSoapObject(struct saveObject *lpSaveObj);
	virtual HRESULT HrUpdateMapiObject(MAPIOBJECT *, const struct saveObject *);
	virtual ECRESULT ECSoapObjectToMapiObject(const struct saveObject *, MAPIOBJECT *);
	virtual HRESULT LockSoap();
	virtual HRESULT UnLockSoap();

	static HRESULT Reload(void *lpParam, ECSESSIONID sessionId);

	/* ECParentStorage may access my functions (used to read PR_ATTACH_DATA_BIN chunks through HrLoadProp()) */
	friend class ECParentStorage;

private:
	entryId			m_sEntryId;
	entryId			m_sParentEntryId;
	KCmdProxy *lpCmd;
	std::recursive_mutex &lpDataLock;
	ECSESSIONID		ecSessionId;
	unsigned int	ulServerCapabilities;
	ULONG m_ulSyncId = 0, m_ulConnection = 0, m_ulEventMask = 0;
	ULONG			m_ulFlags;
	ULONG			m_ulSessionReloadCallback;
	WSTransport		*m_lpTransport;
	bool m_bSubscribed = false;
	ALLOC_WRAP_FRIEND;
};


#endif
