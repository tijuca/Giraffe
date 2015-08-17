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

#ifndef WSMAPIPROPSTORAGE_H
#define WSMAPIPROPSTORAGE_H

#include "zcdefs.h"
#include "ECUnknown.h"
#include "IECPropStorage.h"

#include "ZarafaCode.h"
#include "soapZarafaCmdProxy.h"
#include "WSTransport.h"

#include <mapi.h>
#include <mapispi.h>
#include <pthread.h>

class convert_context;

class WSMAPIPropStorage : public ECUnknown
{

protected:
	WSMAPIPropStorage(ULONG cbParentEntryId, LPENTRYID lpParentEntryId, ULONG cbEntryId, LPENTRYID lpEntryId, ULONG ulFlags, ZarafaCmd *lpCmd, pthread_mutex_t *lpDataLock, ECSESSIONID ecSessionId, unsigned int ulServerCapabilities, WSTransport *lpTransport);
	virtual ~WSMAPIPropStorage();

public:
	static HRESULT Create(ULONG cbParentEntryId, LPENTRYID lpParentEntryId, ULONG cbEntryId, LPENTRYID lpEntryId, ULONG ulFlags, ZarafaCmd *lpCmd, pthread_mutex_t *lpDataLock, ECSESSIONID ecSessionId, unsigned int ulServerCapabilities, WSTransport *lpTransport, WSMAPIPropStorage **lppPropStorage);

	virtual HRESULT QueryInterface(REFIID refiid, void **lppInterface);

	// For ICS
	virtual HRESULT HrSetSyncId(ULONG ulSyncId);

	// Register advise on load object
	virtual HRESULT RegisterAdvise(ULONG ulEventMask, ULONG ulConnection);

	virtual HRESULT GetEntryIDByRef(ULONG *lpcbEntryID, LPENTRYID *lppEntryID);

private:

	// Get a list of the properties
	virtual HRESULT HrReadProps(LPSPropTagArray *lppPropTags,ULONG *cValues, LPSPropValue *ppValues);

	// Get a single (large) property
	virtual HRESULT HrLoadProp(ULONG ulObjId, ULONG ulPropTag, LPSPropValue *lppsPropValue);

	// Write all properties to disk (overwrites a property if it already exists)
	virtual	HRESULT	HrWriteProps(ULONG cValues, LPSPropValue pValues, ULONG ulFlags = 0);

	// Delete properties from file
	virtual HRESULT HrDeleteProps(LPSPropTagArray lpsPropTagArray);

	// Save complete object to server
	virtual HRESULT HrSaveObject(ULONG ulFlags, MAPIOBJECT *lpsMapiObject);

	// Load complete object from server
	virtual HRESULT HrLoadObject(MAPIOBJECT **lppsMapiObject);

	virtual IECPropStorage* GetServerStorage();

	/* very private */
	virtual ECRESULT EcFillPropTags(struct saveObject *lpsSaveObj, MAPIOBJECT *lpsMapiObj);
	virtual ECRESULT EcFillPropValues(struct saveObject *lpsSaveObj, MAPIOBJECT *lpsMapiObj);
	virtual HRESULT HrMapiObjectToSoapObject(MAPIOBJECT *lpsMapiObject, struct saveObject *lpSaveObj, convert_context *lpConverter);
	virtual HRESULT HrUpdateSoapObject(MAPIOBJECT *lpsMapiObject, struct saveObject *lpsSaveObj, convert_context *lpConverter);
	virtual void    DeleteSoapObject(struct saveObject *lpSaveObj);
	virtual HRESULT HrUpdateMapiObject(MAPIOBJECT *lpClientObj, struct saveObject *lpsServerObj);

	virtual ECRESULT ECSoapObjectToMapiObject(struct saveObject *lpsSaveObj, MAPIOBJECT *lpsMapiObject);

	virtual HRESULT LockSoap();
	virtual HRESULT UnLockSoap();

	static HRESULT Reload(void *lpParam, ECSESSIONID sessionId);

	/* ECParentStorage may access my functions (used to read PR_ATTACH_DATA_BIN chunks through HrLoadProp()) */
	friend class ECParentStorage;

public:
	class xECPropStorage _final : public IECPropStorage {
		public:
			// IECUnknown
			virtual ULONG AddRef(void) _override;
			virtual ULONG Release(void) _override;
			virtual HRESULT QueryInterface(REFIID refiid, void **lppInterface) _override;

			// IECPropStorage
			virtual HRESULT HrReadProps(LPSPropTagArray *lppPropTags,ULONG *cValues, LPSPropValue *lppValues);
			virtual HRESULT HrLoadProp(ULONG ulObjId, ULONG ulPropTag, LPSPropValue *lppsPropValue);
			virtual	HRESULT	HrWriteProps(ULONG cValues, LPSPropValue lpValues, ULONG ulFlags = 0);
			virtual HRESULT HrDeleteProps(LPSPropTagArray lpsPropTagArray);
			virtual HRESULT HrSaveObject(ULONG ulFlags, MAPIOBJECT *lpsMapiObject);
			virtual HRESULT HrLoadObject(MAPIOBJECT **lppsMapiObject);
			virtual IECPropStorage* GetServerStorage();
	}m_xECPropStorage;

private:
	entryId			m_sEntryId;
	entryId			m_sParentEntryId;
	ZarafaCmd*		lpCmd;
	pthread_mutex_t *lpDataLock;
	ECSESSIONID		ecSessionId;
	unsigned int	ulServerCapabilities;
	ULONG			m_ulSyncId;
	ULONG			m_ulConnection;
	ULONG			m_ulEventMask;
	ULONG			m_ulFlags;
	ULONG			m_ulSessionReloadCallback;
	WSTransport		*m_lpTransport;
	bool			m_bSubscribed;
};


#endif
