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

#include "platform.h"
#include "WSMAPIFolderOps.h"

#include "Mem.h"
#include "ECGuid.h"

// Utils
#include "SOAPUtils.h"
#include "WSUtil.h"

#include <charset/utf8string.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define START_SOAP_CALL retry:
#define END_SOAP_CALL 	\
	if(er == ZARAFA_E_END_OF_SESSION) { if(m_lpTransport->HrReLogon() == hrSuccess) goto retry; } \
	hr = ZarafaErrorToMAPIError(er, MAPI_E_NOT_FOUND); \
	if(hr != hrSuccess) \
		goto exit;

/*
 * The WSMAPIFolderOps for use with the WebServices transport
 */

WSMAPIFolderOps::WSMAPIFolderOps(ZarafaCmd *lpCmd, pthread_mutex_t *lpDataLock, ECSESSIONID ecSessionId, ULONG cbEntryId, LPENTRYID lpEntryId, WSTransport *lpTransport) : ECUnknown("WSMAPIFolderOps")
{
	this->lpCmd = lpCmd;
	this->lpDataLock = lpDataLock;
	this->ecSessionId = ecSessionId;
	this->m_lpTransport = lpTransport;

	lpTransport->AddSessionReloadCallback(this, Reload, &m_ulSessionReloadCallback);

	CopyMAPIEntryIdToSOAPEntryId(cbEntryId, lpEntryId, &m_sEntryId);
}

WSMAPIFolderOps::~WSMAPIFolderOps()
{
	m_lpTransport->RemoveSessionReloadCallback(m_ulSessionReloadCallback);

	FreeEntryId(&m_sEntryId, false);
}

HRESULT WSMAPIFolderOps::Create(ZarafaCmd *lpCmd, pthread_mutex_t *lpDataLock, ECSESSIONID ecSessionId, ULONG cbEntryId, LPENTRYID lpEntryId, WSTransport *lpTransport, WSMAPIFolderOps **lppFolderOps)
{
	HRESULT hr = hrSuccess;
	WSMAPIFolderOps *lpFolderOps = NULL;

	lpFolderOps = new WSMAPIFolderOps(lpCmd, lpDataLock, ecSessionId, cbEntryId, lpEntryId, lpTransport);

	hr = lpFolderOps->QueryInterface(IID_ECMAPIFolderOps, (void **)lppFolderOps);

	if(hr != hrSuccess)
		delete lpFolderOps;

	return hr;
}

HRESULT WSMAPIFolderOps::QueryInterface(REFIID refiid, void **lppInterface)
{
	REGISTER_INTERFACE(IID_ECMAPIFolderOps, this);

	return MAPI_E_INTERFACE_NOT_SUPPORTED;
}
	
HRESULT WSMAPIFolderOps::HrCreateFolder(ULONG ulFolderType, const utf8string &strFolderName, const utf8string &strComment, BOOL fOpenIfExists, ULONG ulSyncId, LPSBinary lpsSourceKey, ULONG cbNewEntryId, LPENTRYID lpNewEntryId, ULONG* lpcbEntryId, LPENTRYID* lppEntryId)
{
	HRESULT		hr = hrSuccess;
	ECRESULT	er = erSuccess;
	struct xsd__base64Binary sSourceKey;
	struct createFolderResponse sResponse;
	entryId*	lpsEntryId = NULL;

	LockSoap();

	if(lpNewEntryId) {
		hr = CopyMAPIEntryIdToSOAPEntryId(cbNewEntryId, lpNewEntryId, &lpsEntryId);
		if(hr != hrSuccess)
			goto exit;
	}

	if (lpsSourceKey) {
		sSourceKey.__ptr = lpsSourceKey->lpb;
		sSourceKey.__size = lpsSourceKey->cb;
	} else {
		sSourceKey.__ptr = NULL;
		sSourceKey.__size = 0;
	}

	START_SOAP_CALL
	{
		if(SOAP_OK != lpCmd->ns__createFolder(ecSessionId, m_sEntryId, lpsEntryId, ulFolderType, (char*)strFolderName.c_str(), (char*)strComment.c_str(), fOpenIfExists == 0 ? false : true, ulSyncId, sSourceKey, &sResponse))
			er = ZARAFA_E_NETWORK_ERROR;
		else
			er = sResponse.er;
	}
	END_SOAP_CALL

	if(lpcbEntryId != NULL && lppEntryId != NULL)
	{
		hr = CopySOAPEntryIdToMAPIEntryId(&sResponse.sEntryId, lpcbEntryId, lppEntryId);
		if(hr != hrSuccess)
			goto exit;
	}

exit:
	UnLockSoap();

	if(lpsEntryId)
		FreeEntryId(lpsEntryId, true);

	return hr;
}

HRESULT WSMAPIFolderOps::HrDeleteFolder(ULONG cbEntryId, LPENTRYID lpEntryId, ULONG ulFlags, ULONG ulSyncId)
{
	ECRESULT	er = erSuccess;
	HRESULT		hr = hrSuccess;
	entryId		sEntryId;

	LockSoap();

	// Cheap copy entryid
	hr = CopyMAPIEntryIdToSOAPEntryId(cbEntryId, lpEntryId, &sEntryId, true); 
	if(hr != hrSuccess)
		goto exit;

	START_SOAP_CALL
	{
		if(SOAP_OK != lpCmd->ns__deleteFolder(ecSessionId, sEntryId, ulFlags, ulSyncId, &er))
			er = ZARAFA_E_NETWORK_ERROR;
	}
	END_SOAP_CALL

exit:
	UnLockSoap();

	return hr;
}


HRESULT WSMAPIFolderOps::HrEmptyFolder(ULONG ulFlags, ULONG ulSyncId)
{
	ECRESULT er = erSuccess;
	HRESULT hr = hrSuccess;

	LockSoap();

	START_SOAP_CALL
	{
		if(SOAP_OK != lpCmd->ns__emptyFolder(ecSessionId, m_sEntryId, ulFlags, ulSyncId, &er))
			er = ZARAFA_E_NETWORK_ERROR;
	}
	END_SOAP_CALL

exit:
	UnLockSoap();

	return hr;
}

HRESULT WSMAPIFolderOps::HrSetReadFlags(ENTRYLIST *lpMsgList, ULONG ulFlags, ULONG ulSyncId)
{
	HRESULT			hr = hrSuccess;
	ECRESULT		er = erSuccess;
	struct entryList sEntryList;

	memset(&sEntryList, 0, sizeof(struct entryList));

	LockSoap();

	if(lpMsgList) {
		if(lpMsgList->cValues == 0)
			goto exit;
		
		hr = CopyMAPIEntryListToSOAPEntryList(lpMsgList, &sEntryList);
		if(hr != hrSuccess)
			goto exit;
	}

	START_SOAP_CALL
	{
		if(SOAP_OK != lpCmd->ns__setReadFlags(ecSessionId, ulFlags, &m_sEntryId, lpMsgList ? &sEntryList : NULL, ulSyncId, &er))
			er = ZARAFA_E_NETWORK_ERROR;
	}
	END_SOAP_CALL

exit:
	UnLockSoap();

	FreeEntryList(&sEntryList, false);

	return hr;
}

HRESULT WSMAPIFolderOps::HrSetSearchCriteria(ENTRYLIST *lpMsgList, SRestriction *lpRestriction, ULONG ulFlags)
{
	ECRESULT		er = erSuccess;
	HRESULT			hr = hrSuccess;

	struct entryList*		lpsEntryList = NULL;
	struct restrictTable*	lpsRestrict = NULL;

	LockSoap();

	if(lpMsgList) {
		lpsEntryList = new struct entryList;
		
		hr = CopyMAPIEntryListToSOAPEntryList(lpMsgList, lpsEntryList);
		if(hr != hrSuccess)
			goto exit;
	}

	if(lpRestriction) 
	{
		hr = CopyMAPIRestrictionToSOAPRestriction(&lpsRestrict, lpRestriction);
		if(hr != hrSuccess)
			goto exit;
	}
	
	START_SOAP_CALL
	{
		if(SOAP_OK != lpCmd->ns__tableSetSearchCriteria(ecSessionId, m_sEntryId, lpsRestrict, lpsEntryList, ulFlags, &er))
			er = ZARAFA_E_NETWORK_ERROR;
	}
	END_SOAP_CALL

	hr = ZarafaErrorToMAPIError(er);
	if(hr != hrSuccess)
		goto exit;

exit:
	UnLockSoap();


	if(lpsRestrict)
		FreeRestrictTable(lpsRestrict);

	if(lpsEntryList)
		FreeEntryList(lpsEntryList, true);
	
	return hr;
}

HRESULT WSMAPIFolderOps::HrGetSearchCriteria(ENTRYLIST **lppMsgList, LPSRestriction *lppRestriction, ULONG *lpulFlags)
{
	HRESULT			hr = hrSuccess;
	ECRESULT		er = erSuccess;
	ENTRYLIST*		lpMsgList = NULL;
	SRestriction*	lpRestriction = NULL;

	struct tableGetSearchCriteriaResponse sResponse;

	LockSoap();

	START_SOAP_CALL
	{
		if(SOAP_OK != lpCmd->ns__tableGetSearchCriteria(ecSessionId, m_sEntryId, &sResponse))
			er = ZARAFA_E_NETWORK_ERROR;
		else
			er = sResponse.er;
	}
	END_SOAP_CALL

	if(lppRestriction) {

		hr = ECAllocateBuffer(sizeof(SRestriction), (void **)&lpRestriction);
		if(hr != hrSuccess)
			goto exit;

		hr = CopySOAPRestrictionToMAPIRestriction(lpRestriction, sResponse.lpRestrict, lpRestriction);
		if(hr != hrSuccess)
			goto exit;
	}

	if(lppMsgList) {
		hr = CopySOAPEntryListToMAPIEntryList(sResponse.lpFolderIDs, &lpMsgList);
		if(hr != hrSuccess)
			goto exit;

	}

	if(lppMsgList)
		*lppMsgList = lpMsgList;

	if(lppRestriction)
		*lppRestriction = lpRestriction;
	
	if(lpulFlags)
		*lpulFlags = sResponse.ulFlags;

exit:
	UnLockSoap();

	if(hr != hrSuccess && lpMsgList)
		ECFreeBuffer(lpMsgList);

	if(hr != hrSuccess && lpRestriction)
		ECFreeBuffer(lpRestriction);

	return hr;
}

HRESULT WSMAPIFolderOps::HrCopyFolder(ULONG cbEntryFrom, LPENTRYID lpEntryFrom, ULONG cbEntryDest, LPENTRYID lpEntryDest, const utf8string &strNewFolderName, ULONG ulFlags, ULONG ulSyncId)
{
	HRESULT		hr = hrSuccess;
	ECRESULT	er = erSuccess;
	entryId		sEntryFrom;	//Do not free, cheap copy
	entryId		sEntryDest;	//Do not free, cheap copy

	LockSoap();

	hr = CopyMAPIEntryIdToSOAPEntryId(cbEntryFrom, lpEntryFrom, &sEntryFrom, true);
	if(hr != hrSuccess)
		goto exit;

	hr = CopyMAPIEntryIdToSOAPEntryId(cbEntryDest, lpEntryDest, &sEntryDest, true);
	if(hr != hrSuccess)
		goto exit;

	START_SOAP_CALL
	{
		if(SOAP_OK != lpCmd->ns__copyFolder(ecSessionId, sEntryFrom, sEntryDest, (char*)strNewFolderName.c_str(), ulFlags, ulSyncId, &er))
			er = ZARAFA_E_NETWORK_ERROR;
	}
	END_SOAP_CALL

exit:
	UnLockSoap();

	return hr;
}

HRESULT WSMAPIFolderOps::HrCopyMessage(ENTRYLIST *lpMsgList, ULONG cbEntryDest, LPENTRYID lpEntryDest, ULONG ulFlags, ULONG ulSyncId)
{
	HRESULT			hr = hrSuccess;
	ECRESULT		er = erSuccess;
	struct entryList sEntryList;
	entryId			sEntryDest;	//Do not free, cheap copy

	memset(&sEntryList, 0, sizeof(struct entryList));

	LockSoap();

	if(lpMsgList->cValues == 0)
		goto exit;

	hr = CopyMAPIEntryListToSOAPEntryList(lpMsgList, &sEntryList);
	if(hr != hrSuccess)
		goto exit;

	hr = CopyMAPIEntryIdToSOAPEntryId(cbEntryDest, lpEntryDest, &sEntryDest, true);
	if(hr != hrSuccess)
		goto exit;

	START_SOAP_CALL
	{
		if(SOAP_OK != lpCmd->ns__copyObjects(ecSessionId, &sEntryList, sEntryDest, ulFlags, ulSyncId, &er))
			er = ZARAFA_E_NETWORK_ERROR;
	}
	END_SOAP_CALL

exit:
	UnLockSoap();
	FreeEntryList(&sEntryList, false);
	
	return hr;
}

HRESULT WSMAPIFolderOps::HrGetMessageStatus(ULONG cbEntryID, LPENTRYID lpEntryID, ULONG ulFlags, ULONG *lpulMessageStatus)
{
	HRESULT		hr = hrSuccess;
	ECRESULT	er = erSuccess;
	entryId		sEntryId;	//Do not free, cheap copy
	
	struct messageStatus sMessageStatus;

	LockSoap();

	if(lpEntryID == NULL) {
		hr = MAPI_E_INVALID_ENTRYID;
		goto exit;
	}

	hr = CopyMAPIEntryIdToSOAPEntryId(cbEntryID, lpEntryID, &sEntryId, true);
	if(hr != hrSuccess)
		goto exit;

	START_SOAP_CALL
	{
		if(SOAP_OK != lpCmd->ns__getMessageStatus(ecSessionId, sEntryId, ulFlags, &sMessageStatus) )
			er = ZARAFA_E_NETWORK_ERROR;
		else
			er = sMessageStatus.er;
	}
	END_SOAP_CALL

	*lpulMessageStatus = sMessageStatus.ulMessageStatus;

exit:
	UnLockSoap();

	return hr;
}

HRESULT WSMAPIFolderOps::HrSetMessageStatus(ULONG cbEntryID, LPENTRYID lpEntryID, ULONG ulNewStatus, ULONG ulNewStatusMask, ULONG ulSyncId, ULONG *lpulOldStatus)
{
	HRESULT		hr = hrSuccess;
	ECRESULT	er = erSuccess;
	entryId		sEntryId;	//Do not free, cheap copy
	
	struct messageStatus sMessageStatus;

	LockSoap();

	if(lpEntryID == NULL)	{
		hr = MAPI_E_INVALID_ENTRYID;
		goto exit;
	}

	hr = CopyMAPIEntryIdToSOAPEntryId(cbEntryID, lpEntryID, &sEntryId, true);
	if(hr != hrSuccess)
		goto exit;

	START_SOAP_CALL
	{
		if(SOAP_OK != lpCmd->ns__setMessageStatus(ecSessionId, sEntryId, ulNewStatus, ulNewStatusMask, ulSyncId, &sMessageStatus) )
			er = ZARAFA_E_NETWORK_ERROR;
		else
			er = sMessageStatus.er;
	}
	END_SOAP_CALL

	if(lpulOldStatus)
		*lpulOldStatus = sMessageStatus.ulMessageStatus;
exit:
	UnLockSoap();

	return hr;
}

HRESULT WSMAPIFolderOps::HrGetChangeInfo(ULONG cbEntryID, LPENTRYID lpEntryID, LPSPropValue *lppPropPCL, LPSPropValue *lppPropCK)
{
	HRESULT		hr = hrSuccess;
	ECRESULT	er = erSuccess;
	entryId		sEntryId = {0};

	LPSPropValue			lpSPropValPCL = NULL;
	LPSPropValue			lpSPropValCK = NULL;
	getChangeInfoResponse	sChangeInfo = {{0}};

	LockSoap();

	if (lpEntryID == NULL)	{
		hr = MAPI_E_INVALID_ENTRYID;
		goto exit;
	}

	hr = CopyMAPIEntryIdToSOAPEntryId(cbEntryID, lpEntryID, &sEntryId, true);
	if(hr != hrSuccess)
		goto exit;

	if (SOAP_OK != lpCmd->ns__getChangeInfo(ecSessionId, sEntryId, &sChangeInfo))
		er = ZARAFA_E_NETWORK_ERROR;
	else
		er = sChangeInfo.er;

	hr = ZarafaErrorToMAPIError(er);
	if (hr != hrSuccess)
		goto exit;

	if (lppPropPCL) {
		hr = MAPIAllocateBuffer(sizeof *lpSPropValPCL, (void**)&lpSPropValPCL);
		if (hr != hrSuccess)
			goto exit;

		hr = CopySOAPPropValToMAPIPropVal(lpSPropValPCL, &sChangeInfo.sPropPCL, lpSPropValPCL);
		if (hr != hrSuccess)
			goto exit;
	}

	if (lppPropCK) {
		hr = MAPIAllocateBuffer(sizeof *lpSPropValCK, (void**)&lpSPropValCK);
		if (hr != hrSuccess)
			goto exit;

		hr = CopySOAPPropValToMAPIPropVal(lpSPropValCK, &sChangeInfo.sPropCK, lpSPropValCK);
		if (hr != hrSuccess)
			goto exit;
	}

	// All went well, modify ouput parameters
	if (lppPropPCL) {
		*lppPropPCL = lpSPropValPCL;
		lpSPropValPCL = NULL;
	}

	if (lppPropCK) {
		*lppPropCK = lpSPropValCK;
		lpSPropValCK = NULL;
	}

exit:
	UnLockSoap();

	if (lpSPropValPCL)
		MAPIFreeBuffer(lpSPropValPCL);

	if (lpSPropValCK)
		MAPIFreeBuffer(lpSPropValCK);

	return hr;
}

//FIXME: one lock/unlock function
HRESULT WSMAPIFolderOps::LockSoap()
{
	pthread_mutex_lock(lpDataLock);
	return erSuccess;
}

HRESULT WSMAPIFolderOps::UnLockSoap()
{
	//Clean up data create with soap_malloc
	if(lpCmd->soap)
		soap_end(lpCmd->soap);

	pthread_mutex_unlock(lpDataLock);
	return erSuccess;
}

HRESULT WSMAPIFolderOps::Reload(void *lpParam, ECSESSIONID sessionid)
{
	HRESULT hr = hrSuccess;
	WSMAPIFolderOps *lpThis = (WSMAPIFolderOps *)lpParam;

	lpThis->ecSessionId = sessionid;

	return hr;
}
