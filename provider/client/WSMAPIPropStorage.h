/*
 * Copyright 2005 - 2009  Zarafa B.V.
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

#ifndef WSMAPIPROPSTORAGE_H
#define WSMAPIPROPSTORAGE_H

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
	WSMAPIPropStorage(ULONG cbParentEntryId, LPENTRYID lpParentEntryId, ULONG cbEntryId, LPENTRYID lpEntryId, ULONG ulFlags, ZarafaCmd *lpCmd, pthread_mutex_t mDataLock, ECSESSIONID ecSessionId, unsigned int ulServerCapabilities, WSTransport *lpTransport);
	virtual ~WSMAPIPropStorage();

public:
	static HRESULT Create(ULONG cbParentEntryId, LPENTRYID lpParentEntryId, ULONG cbEntryId, LPENTRYID lpEntryId, ULONG ulFlags, ZarafaCmd *lpCmd, pthread_mutex_t mDataLock, ECSESSIONID ecSessionId, unsigned int ulServerCapabilities, WSTransport *lpTransport, WSMAPIPropStorage **lppPropStorage);

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
	class xECPropStorage : public IECPropStorage
	{
		public:
			// IECUnknown
			virtual ULONG AddRef();
			virtual ULONG Release();
			virtual HRESULT QueryInterface(REFIID refiid, void** lppInterface);

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
	pthread_mutex_t	mDataLock;
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
