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

#include <platform.h>

#include <mapi.h>
#include <mapix.h>
#include <mapiutil.h>
#include <mapidefs.h>
#include <mapiguid.h>

#include <edkguid.h>

#include <CommonUtil.h>
#include <ECDebug.h>
#include <ECGuid.h>
#include <Trace.h>
#include <Util.h>

#include "ECSynchronizationImportChanges.h"

ECSynchronizationImportChanges::ECSynchronizationImportChanges(ECThreadData *lpThreadData, SBinary *lpFolderSourceKey,
															   ECChangeData *lpChanges)
{
	m_lpThreadData = lpThreadData;

	if (lpFolderSourceKey) {
		m_sParentSourceKey.ulPropTag = PR_PARENT_SOURCE_KEY;
		m_sParentSourceKey.Value.bin = *lpFolderSourceKey;
	} else {
		m_sParentSourceKey.ulPropTag = 0;
		m_sParentSourceKey.Value.bin.cb = 0;
		m_sParentSourceKey.Value.bin.lpb = NULL;
	}
	m_lpChanges = lpChanges;
}

ECSynchronizationImportChanges::~ECSynchronizationImportChanges()
{
}

HRESULT ECSynchronizationImportChanges::Create(ECThreadData *lpThreadData, SBinary *lpFolderSourceKey,
											   ECChangeData *lpChanges, REFIID refiid,
											   LPVOID *lppSyncImportChanges)
{
	HRESULT hr = hrSuccess;
	ECSynchronizationImportChanges *lpSyncImportChanges = NULL;

	try {
		lpSyncImportChanges = new ECSynchronizationImportChanges(lpThreadData, lpFolderSourceKey, lpChanges);
	}
	catch (...) {
		lpSyncImportChanges = NULL;
	}
	if (!lpSyncImportChanges) {
		hr = MAPI_E_NOT_ENOUGH_MEMORY;
		goto exit;
	}

	lpSyncImportChanges->AddRef();

	hr = lpSyncImportChanges->QueryInterface(refiid, lppSyncImportChanges);
	if (hr != hrSuccess)
		goto exit;

exit:
	if (lpSyncImportChanges)
		lpSyncImportChanges->Release();

	return hr;
}

HRESULT ECSynchronizationImportChanges::QueryInterface(REFIID refiid, LPVOID *lppvoid)
{
	HRESULT hr = hrSuccess;

	if (refiid == IID_ECUnknown) {
		*lppvoid = this;
		AddRef();
	} else if (refiid == IID_IECImportAddressbookChanges) {
		*lppvoid = &this->m_xECImportAddressbookChanges;
		AddRef();
	} else if (refiid == IID_IExchangeImportHierarchyChanges) {
		*lppvoid = &this->m_xExchangeImportHierarchyChanges;
		AddRef();
	} else if (refiid == IID_IExchangeImportContentsChanges) {
		*lppvoid = &this->m_xExchangeImportContentsChanges;
		AddRef();
	} else if (refiid == IID_IECImportContentsChanges) {
		*lppvoid = &this->m_xExchangeImportContentsChanges2;
		AddRef();
	} else
		hr = MAPI_E_INTERFACE_NOT_SUPPORTED;

	return hr;
}

HRESULT ECSynchronizationImportChanges::GetLastError(HRESULT hResult, ULONG ulFlags, LPMAPIERROR *lppMAPIError)
{
	return MAPI_E_NO_SUPPORT;
}

HRESULT ECSynchronizationImportChanges::Config(LPSTREAM lpStream, ULONG ulFlags)
{
	HRESULT hr = hrSuccess;
	
	if (!m_lpChanges) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

exit:
	return hrSuccess;
}

HRESULT ECSynchronizationImportChanges::UpdateState(LPSTREAM lpStream)
{
	/* We don't have to do anything here */
	return hrSuccess;
}

HRESULT ECSynchronizationImportChanges::ImportABChange(ULONG type, ULONG cbObjId, LPENTRYID lpObjId)
{
	HRESULT hr = hrSuccess;
	sourceid_t *lpsSourceId = NULL;
	SPropValue sProps[1];
	ULONG ulProps = 0;

	if (type != MAPI_MAILUSER && type != MAPI_ABCONT)
		goto exit;

	/** 
	 * We use PR_ADDRESS_BOOK_ENTRYID here, because it's being used in
	 * the restriction (ECIndexer.cpp). It will be compared to
	 * PR_ENTRYID, but the correct compare function needs to be used
	 * because of old (mixed) V0 and V1 addressbook entryid's.
	 */
	sProps[ulProps].ulPropTag = PR_ADDRESS_BOOK_ENTRYID;
	sProps[ulProps].Value.bin.cb = cbObjId;
	sProps[ulProps].Value.bin.lpb = (LPBYTE)lpObjId;
	ulProps++;

	hr = CreateSourceId(ulProps, sProps, &lpsSourceId);
	if (hr != hrSuccess)
		goto exit;

	m_lpChanges->InsertCreate(lpsSourceId);

exit:
	if ((hr != hrSuccess) && lpsSourceId)
		MAPIFreeBuffer(lpsSourceId);

	return hr;
}

HRESULT ECSynchronizationImportChanges::ImportABDeletion(ULONG type, ULONG cbObjId, LPENTRYID lpObjId)
{
	HRESULT hr = hrSuccess;
	sourceid_t *lpsSourceId = NULL;
	SPropValue sProps[1];
	ULONG ulProps = 0;

	if (type != MAPI_MAILUSER)
		goto exit;

	sProps[ulProps].ulPropTag = PR_ENTRYID;
	sProps[ulProps].Value.bin.cb = cbObjId;
	sProps[ulProps].Value.bin.lpb = (LPBYTE)lpObjId;
	ulProps++;

	hr = CreateSourceId(ulProps, sProps, &lpsSourceId);
	if (hr != hrSuccess)
		goto exit;

	m_lpChanges->InsertDelete(lpsSourceId);
	lpsSourceId = NULL;

exit:
	if ((hr != hrSuccess) && lpsSourceId)
		MAPIFreeBuffer(lpsSourceId);

	return hr;
}

HRESULT ECSynchronizationImportChanges::ImportFolderChange(ULONG cValues, LPSPropValue lpPropArray)
{
	HRESULT hr = hrSuccess;
	sourceid_t *lpsSourceId = NULL;
	LPSPropValue lpTmp = NULL;
	SPropValue sProps[2];
	ULONG ulProps = 0;

	if (m_sParentSourceKey.ulPropTag != PR_PARENT_SOURCE_KEY) {
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}

	lpTmp = PpropFindProp(lpPropArray, cValues, PR_FOLDER_TYPE);
	if (!lpTmp || lpTmp->Value.ul != FOLDER_GENERIC)
		goto exit;

	lpTmp = PpropFindProp(lpPropArray, cValues, PR_SOURCE_KEY);
	if (!lpTmp || !lpTmp->Value.bin.cb) {
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}

	sProps[ulProps] = *lpTmp;
	ulProps++;

	lpTmp = PpropFindProp(lpPropArray, cValues, PR_PARENT_SOURCE_KEY);
	if (!lpTmp || !lpTmp->Value.bin.cb)
		sProps[ulProps] = m_sParentSourceKey;
	else
		sProps[ulProps] = *lpTmp;
	ulProps++;

	hr = CreateSourceId(ulProps, sProps, &lpsSourceId);
	if (hr != hrSuccess)
		goto exit;

	m_lpChanges->InsertCreate(lpsSourceId);
	lpsSourceId = NULL;

exit:
	if ((hr != hrSuccess) && lpsSourceId)
		MAPIFreeBuffer(lpsSourceId);

	return hr;
}

HRESULT ECSynchronizationImportChanges::ImportFolderDeletion(ULONG ulFlags, LPENTRYLIST lpSourceEntryList)
{
	HRESULT hr = hrSuccess;
	sourceid_t *lpsSourceId = NULL;
	SPropValue sProps[2];
	ULONG ulProps = 0;

	if (m_sParentSourceKey.ulPropTag != PR_PARENT_SOURCE_KEY) {
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}

	for (ULONG i = 0; i < lpSourceEntryList->cValues; i++) {
		ulProps = 0;

		sProps[ulProps].ulPropTag = PR_SOURCE_KEY;
		sProps[ulProps].Value.bin = lpSourceEntryList->lpbin[i];
		ulProps++;

		sProps[ulProps] = m_sParentSourceKey;
		ulProps++;

		hr = CreateSourceId(ulProps, sProps, &lpsSourceId);
		if (hr != hrSuccess)
			goto exit;

		m_lpChanges->InsertDelete(lpsSourceId);
		lpsSourceId = NULL;
	}

exit:
	if ((hr != hrSuccess) && lpsSourceId)
		MAPIFreeBuffer(lpsSourceId);

	return hr;
}

HRESULT ECSynchronizationImportChanges::ImportMessageChange(ULONG cValues, LPSPropValue lpPropArray, ULONG ulFlags, LPMESSAGE * lppMessage)
{
	HRESULT hr = hrSuccess;
	sourceid_t *lpsSourceId = NULL;
	LPSPropValue lpTmp = NULL;
	SPropValue sProps[4];
	ULONG ulProps = 0;

	if (m_sParentSourceKey.ulPropTag != PR_PARENT_SOURCE_KEY) {
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}

	lpTmp = PpropFindProp(lpPropArray, cValues, PR_SOURCE_KEY);
	if (!lpTmp || !lpTmp->Value.bin.cb) {
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}

	sProps[ulProps] = *lpTmp;
	ulProps++;

	lpTmp = PpropFindProp(lpPropArray, cValues, PR_PARENT_SOURCE_KEY);
	if (!lpTmp || !lpTmp->Value.bin.cb)
		sProps[ulProps] = m_sParentSourceKey;
	else
		sProps[ulProps] = *lpTmp;
	ulProps++;

	lpTmp = PpropFindProp(lpPropArray, cValues, PR_ENTRYID);
	if (lpTmp && lpTmp->Value.bin.cb) {
		sProps[ulProps] = *lpTmp;
		ulProps++;
	}

	lpTmp = PpropFindProp(lpPropArray, cValues, PR_PARENT_ENTRYID);
	if (lpTmp && lpTmp->Value.bin.cb) {
		sProps[ulProps] = *lpTmp;
		ulProps++;
	}

	hr = CreateSourceId(ulProps, sProps, &lpsSourceId);
	if (hr != hrSuccess)
		goto exit;

	if (ulFlags & SYNC_NEW_MESSAGE)
		m_lpChanges->InsertCreate(lpsSourceId);
	else
		m_lpChanges->InsertChange(lpsSourceId);
	
	lpsSourceId = NULL;

exit:
	if ((hr != hrSuccess) && lpsSourceId)
		MAPIFreeBuffer(lpsSourceId);

	/* Always return SYNC_E_IGNORE, otherwise we should have provided the lppMessage */
	return (hr == hrSuccess) ? SYNC_E_IGNORE : hr;
}

HRESULT ECSynchronizationImportChanges::ImportMessageDeletion(ULONG ulFlags, LPENTRYLIST lpSourceEntryList)
{
	HRESULT hr = hrSuccess;
	sourceid_t *lpsSourceId = NULL;
	SPropValue sProps[2];
	ULONG ulProps = 0;

	if (m_sParentSourceKey.ulPropTag != PR_PARENT_SOURCE_KEY) {
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}

	for (ULONG i = 0; i < lpSourceEntryList->cValues; i++) {
		ulProps = 0;

		sProps[ulProps].ulPropTag = PR_SOURCE_KEY;
		sProps[ulProps].Value.bin = lpSourceEntryList->lpbin[i];
		ulProps++;

		sProps[ulProps] = m_sParentSourceKey;
		ulProps++;

		hr = CreateSourceId(ulProps, sProps, &lpsSourceId);
		if (hr != hrSuccess)
			goto exit;

		m_lpChanges->InsertDelete(lpsSourceId);
		lpsSourceId = NULL;
	}

exit:
	if ((hr != hrSuccess) && lpsSourceId)
		MAPIFreeBuffer(lpsSourceId);

	return hr;
}

HRESULT ECSynchronizationImportChanges::ImportPerUserReadStateChange(ULONG cElements, LPREADSTATE lpReadState)
{
	/* ReadState changes are not important for the indexer */
	return hrSuccess;
}

HRESULT ECSynchronizationImportChanges::ImportMessageMove(ULONG cbSourceKeySrcFolder, BYTE FAR * pbSourceKeySrcFolder,
														  ULONG cbSourceKeySrcMessage, BYTE FAR * pbSourceKeySrcMessage,
														  ULONG cbPCLMessage, BYTE FAR * pbPCLMessage,
														  ULONG cbSourceKeyDestMessage, BYTE FAR * pbSourceKeyDestMessage,
														  ULONG cbChangeNumDestMessage, BYTE FAR * pbChangeNumDestMessage)
{
	HRESULT hr = hrSuccess;
	sourceid_t *lpsSourceId = NULL;
	SPropValue sProps[2];
	ULONG ulProps = 0;

	if (m_sParentSourceKey.ulPropTag != PR_PARENT_SOURCE_KEY) {
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}

	sProps[ulProps].ulPropTag = PR_SOURCE_KEY;
	sProps[ulProps].Value.bin.cb = cbSourceKeySrcMessage;
	sProps[ulProps].Value.bin.lpb = pbSourceKeySrcMessage;
	ulProps++;

	sProps[ulProps].ulPropTag = PR_PARENT_SOURCE_KEY;
	sProps[ulProps].Value.bin.cb = cbSourceKeySrcFolder;
	sProps[ulProps].Value.bin.lpb = pbSourceKeySrcFolder;
	ulProps++;

	hr = CreateSourceId(ulProps, sProps, &lpsSourceId);
	if (hr != hrSuccess)
		goto exit;

	m_lpChanges->InsertDelete(lpsSourceId);
	lpsSourceId = NULL;

	/* Overwrite for the actual creation */
	ulProps = 0;

	sProps[ulProps].ulPropTag = PR_SOURCE_KEY;
	sProps[ulProps].Value.bin.cb = cbSourceKeySrcMessage;
	sProps[ulProps].Value.bin.lpb = pbSourceKeySrcMessage;
	ulProps++;

	sProps[ulProps] = m_sParentSourceKey;
	ulProps++;

	hr = CreateSourceId(ulProps, sProps, &lpsSourceId);
	if (hr != hrSuccess)
		goto exit;

	m_lpChanges->InsertCreate(lpsSourceId);
	lpsSourceId = NULL;

exit:
	if ((hr != hrSuccess) && lpsSourceId)
		MAPIFreeBuffer(lpsSourceId);

	/* Always return SYNC_E_IGNORE, otherwise we should have provided the lppMessage */
	return (hr == hrSuccess) ? SYNC_E_IGNORE : hr;
}

HRESULT ECSynchronizationImportChanges::ConfigForConversionStream(LPSTREAM lpStream, ULONG ulFlags,
																  ULONG cValuesConversion, LPSPropValue lpPropArrayConversion)
{
	HRESULT hr = hrSuccess;

	/* Nothing interesting to do, simply call main Config() */
	hr = Config(lpStream, ulFlags);
	if (hr != hrSuccess)
		goto exit;

exit:
	return hr;
}

HRESULT ECSynchronizationImportChanges::ImportMessageChangeAsAStream(ULONG cValues, LPSPropValue lpPropArray, ULONG ulFlags, LPSTREAM *lppstream)
{
	HRESULT hr = hrSuccess;
	IStream *lpStream = NULL;
	sourceid_t *lpsSourceId = NULL;
	LPSPropValue lpTmp = NULL;

	if (m_sParentSourceKey.ulPropTag != PR_PARENT_SOURCE_KEY) {
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}

	lpTmp = PpropFindProp(lpPropArray, cValues, PR_SOURCE_KEY);
	if (!lpTmp || !lpTmp->Value.bin.cb) {
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}

	// PR_SOURCE_KEY must be the first property in the list
	if (lpTmp != lpPropArray)
		std::swap(*lpPropArray, *lpTmp);

	lpTmp = PpropFindProp(lpPropArray, cValues, PR_PARENT_SOURCE_KEY);
	if (!lpTmp) {
		lpTmp = PpropFindProp(lpPropArray, cValues, CHANGE_PROP_TYPE(PR_PARENT_SOURCE_KEY, PT_UNSPECIFIED));
		if (!lpTmp) {
			hr = MAPI_E_NOT_FOUND;
			goto exit;
		}
	}

	hr = CreateSourceId(cValues, lpPropArray, &lpsSourceId);
	if (hr != hrSuccess)
		goto exit;

	hr = CreateStreamOnHGlobal(GlobalAlloc(GPTR, 0), true, &lpStream);
	if (hr != hrSuccess)
		goto exit;

	if (lppstream) {
		hr = lpStream->QueryInterface(IID_IStream, (LPVOID*)lppstream);
		if (hr != hrSuccess)
			goto exit;
	}

	hr = lpStream->QueryInterface(IID_IStream, (LPVOID*)&lpsSourceId->lpStream);
	if (hr != hrSuccess)
		goto exit;
		
	if (ulFlags & SYNC_NEW_MESSAGE)
		m_lpChanges->InsertCreate(lpsSourceId);
	else
		m_lpChanges->InsertChange(lpsSourceId);

	// lpsSourceId was added to a list, make sure not to free it.
	lpsSourceId = NULL;

exit:
	if (lpStream)
		lpStream->Release();

	if (lpsSourceId) {
		if (lpsSourceId->lpStream)
			lpsSourceId->lpStream->Release();
		MAPIFreeBuffer(lpsSourceId);
	}

	return hr;
}

HRESULT ECSynchronizationImportChanges::SetMessageInterface(REFIID /*refiid*/)
{
	return hrSuccess;
}

HRESULT ECSynchronizationImportChanges::CreateSourceId(ULONG ulProps, LPSPropValue lpProps, sourceid_t **lppSourceId)
{
	HRESULT hr = hrSuccess;
	sourceid_t *lpSourceId = NULL;

	if (!lppSourceId) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	ASSERT(ulProps > 0);
	ASSERT(lpProps != NULL);
	ASSERT(PROP_TYPE(lpProps[0].ulPropTag) == PT_BINARY);

	hr = MAPIAllocateBuffer(sizeof(*lpSourceId), (LPVOID *)&lpSourceId);
	if (hr != hrSuccess)
		goto exit;

	/* Initialize memory with default constructor */
	*lpSourceId = sourceid_t();

	hr = MAPIAllocateMore(sizeof(*lpSourceId->lpProps) * ulProps, lpSourceId, (LPVOID *)&lpSourceId->lpProps);
	if (hr != hrSuccess)
		goto exit;
	
	hr = Util::HrCopyPropertyArray(lpProps, ulProps, lpSourceId->lpProps, lpSourceId);
	if (hr != hrSuccess)
		goto exit;
	
	lpSourceId->ulProps = ulProps;

	if (lppSourceId)
		*lppSourceId = lpSourceId;

exit:
	if ((hr != hrSuccess) && lpSourceId)
		MAPIFreeBuffer(lpSourceId);

	return hr;
}

ULONG ECSynchronizationImportChanges::xECImportAddressbookChanges::AddRef()
{
	TRACE_MAPI(TRACE_ENTRY, "IECImportAddressbookChanges::AddRef", "");
	METHOD_PROLOGUE_(ECSynchronizationImportChanges, ECImportAddressbookChanges);
	return pThis->AddRef();
}

ULONG ECSynchronizationImportChanges::xECImportAddressbookChanges::Release()
{
	TRACE_MAPI(TRACE_ENTRY, "IECImportAddressbookChanges::Release", "");
	METHOD_PROLOGUE_(ECSynchronizationImportChanges, ECImportAddressbookChanges);
	return pThis->Release();
}

HRESULT ECSynchronizationImportChanges::xECImportAddressbookChanges::QueryInterface(REFIID refiid, LPVOID *lppvoid)
{
	TRACE_MAPI(TRACE_ENTRY, "ECImportAddressbookChanges::QueryInterface", "");
	METHOD_PROLOGUE_(ECSynchronizationImportChanges, ECImportAddressbookChanges);
	HRESULT hr = pThis->QueryInterface(refiid, lppvoid);
	TRACE_MAPI(TRACE_RETURN, "ECImportAddressbookChanges::QueryInterface", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECSynchronizationImportChanges::xECImportAddressbookChanges::GetLastError(HRESULT hResult, ULONG ulFlags, LPMAPIERROR *lppMAPIError)
{
	TRACE_MAPI(TRACE_ENTRY, "ECImportAddressbookChanges:::GetLastError", "");
	METHOD_PROLOGUE_(ECSynchronizationImportChanges, ECImportAddressbookChanges);
	HRESULT hr = pThis->GetLastError(hResult, ulFlags, lppMAPIError);
	TRACE_MAPI(TRACE_RETURN, "ECImportAddressbookChanges::GetLastError", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECSynchronizationImportChanges::xECImportAddressbookChanges::Config(LPSTREAM lpStream, ULONG ulFlags)
{
	TRACE_MAPI(TRACE_ENTRY, "ECImportAddressbookChanges:::Config", "");
	METHOD_PROLOGUE_(ECSynchronizationImportChanges, ECImportAddressbookChanges);
	HRESULT hr = pThis->Config(lpStream, ulFlags);
	TRACE_MAPI(TRACE_RETURN, "ECImportAddressbookChanges::Config", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECSynchronizationImportChanges::xECImportAddressbookChanges::UpdateState(LPSTREAM lpStream)
{
	TRACE_MAPI(TRACE_ENTRY, "ECImportAddressbookChanges::UpdateState", "");
	METHOD_PROLOGUE_(ECSynchronizationImportChanges, ECImportAddressbookChanges);
	HRESULT hr = pThis->UpdateState(lpStream);
	TRACE_MAPI(TRACE_RETURN, "ECImportAddressbookChanges::UpdateState", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECSynchronizationImportChanges::xECImportAddressbookChanges::ImportABChange(ULONG type, ULONG cbObjId, LPENTRYID lpObjId)
{
	TRACE_MAPI(TRACE_ENTRY, "ECImportAddressbookChanges::ImportABChange", "");
	METHOD_PROLOGUE_(ECSynchronizationImportChanges, ECImportAddressbookChanges);
	HRESULT hr = pThis->ImportABChange(type, cbObjId, lpObjId);
	TRACE_MAPI(TRACE_RETURN, "ECImportAddressbookChanges::ImportABChange", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECSynchronizationImportChanges::xECImportAddressbookChanges::ImportABDeletion(ULONG type, ULONG cbObjId, LPENTRYID lpObjId)
{
	TRACE_MAPI(TRACE_ENTRY, "ECImportAddressbookChanges::ImportABDeletion", "");
	METHOD_PROLOGUE_(ECSynchronizationImportChanges, ECImportAddressbookChanges);
	HRESULT hr = pThis->ImportABDeletion(type, cbObjId, lpObjId);
	TRACE_MAPI(TRACE_RETURN, "ECImportAddressbookChanges::ImportABDeletion", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

ULONG ECSynchronizationImportChanges::xExchangeImportHierarchyChanges::AddRef()
{
	TRACE_MAPI(TRACE_ENTRY, "IExchangeImportHierarchyChanges::AddRef", "");
	METHOD_PROLOGUE_(ECSynchronizationImportChanges, ExchangeImportHierarchyChanges);
	return pThis->AddRef();
}

ULONG ECSynchronizationImportChanges::xExchangeImportHierarchyChanges::Release()
{
	TRACE_MAPI(TRACE_ENTRY, "IExchangeImportHierarchyChanges::Release", "");
	METHOD_PROLOGUE_(ECSynchronizationImportChanges, ExchangeImportHierarchyChanges);
	return pThis->Release();
}

HRESULT ECSynchronizationImportChanges::xExchangeImportHierarchyChanges::QueryInterface(REFIID refiid, LPVOID *lppvoid)
{
	TRACE_MAPI(TRACE_ENTRY, "IExchangeImportHierarchyChanges::QueryInterface", "");
	METHOD_PROLOGUE_(ECSynchronizationImportChanges, ExchangeImportHierarchyChanges);
	HRESULT hr = pThis->QueryInterface(refiid, lppvoid);
	TRACE_MAPI(TRACE_RETURN, "IExchangeImportHierarchyChanges::QueryInterface", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECSynchronizationImportChanges::xExchangeImportHierarchyChanges::GetLastError(HRESULT hResult, ULONG ulFlags, LPMAPIERROR *lppMAPIError)
{
	TRACE_MAPI(TRACE_ENTRY, "IExchangeImportHierarchyChanges::GetLastError", "");
	METHOD_PROLOGUE_(ECSynchronizationImportChanges, ExchangeImportHierarchyChanges);
	HRESULT hr = pThis->GetLastError(hResult, ulFlags, lppMAPIError);
	TRACE_MAPI(TRACE_RETURN, "IExchangeImportHierarchyChanges::GetLastError", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECSynchronizationImportChanges::xExchangeImportHierarchyChanges::Config(LPSTREAM lpStream, ULONG ulFlags)
{
	TRACE_MAPI(TRACE_ENTRY, "IExchangeImportHierarchyChanges::Config", "");
	METHOD_PROLOGUE_(ECSynchronizationImportChanges, ExchangeImportHierarchyChanges);
	HRESULT hr = pThis->Config(lpStream, ulFlags);
	TRACE_MAPI(TRACE_RETURN, "IExchangeImportHierarchyChanges::Config", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECSynchronizationImportChanges::xExchangeImportHierarchyChanges::UpdateState(LPSTREAM lpStream)
{
	TRACE_MAPI(TRACE_ENTRY, "IExchangeImportHierarchyChanges::UpdateState", "");
	METHOD_PROLOGUE_(ECSynchronizationImportChanges, ExchangeImportHierarchyChanges);
	HRESULT hr = pThis->UpdateState(lpStream);
	TRACE_MAPI(TRACE_RETURN, "IExchangeImportHierarchyChanges::UpdateState", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECSynchronizationImportChanges::xExchangeImportHierarchyChanges::ImportFolderChange(ULONG cValue, LPSPropValue lpPropArray)
{
	TRACE_MAPI(TRACE_ENTRY, "IExchangeImportHierarchyChanges::ImportFolderChange", "");
	METHOD_PROLOGUE_(ECSynchronizationImportChanges, ExchangeImportHierarchyChanges);
	HRESULT hr = pThis->ImportFolderChange(cValue, lpPropArray);
	TRACE_MAPI(TRACE_RETURN, "IExchangeImportHierarchyChanges::ImportFolderChange", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECSynchronizationImportChanges::xExchangeImportHierarchyChanges::ImportFolderDeletion(ULONG ulFlags, LPENTRYLIST lpSourceEntryList)
{
	TRACE_MAPI(TRACE_ENTRY, "IExchangeImportHierarchyChanges::ImportFolderDeletion", "");
	METHOD_PROLOGUE_(ECSynchronizationImportChanges, ExchangeImportHierarchyChanges);
	HRESULT hr = pThis->ImportFolderDeletion(ulFlags, lpSourceEntryList);
	TRACE_MAPI(TRACE_RETURN, "IExchangeImportHierarchyChanges::ImportFolderDeletion", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

ULONG ECSynchronizationImportChanges::xExchangeImportContentsChanges::AddRef()
{
	TRACE_MAPI(TRACE_ENTRY, "IExchangeImportContentsChanges::AddRef", "");
	METHOD_PROLOGUE_(ECSynchronizationImportChanges, ExchangeImportContentsChanges);
	return pThis->AddRef();
}

ULONG ECSynchronizationImportChanges::xExchangeImportContentsChanges::Release()
{
	TRACE_MAPI(TRACE_ENTRY, "IExchangeImportContentsChanges::Release", "");
	METHOD_PROLOGUE_(ECSynchronizationImportChanges, ExchangeImportContentsChanges);
	return pThis->Release();
}

HRESULT ECSynchronizationImportChanges::xExchangeImportContentsChanges::QueryInterface(REFIID refiid, LPVOID *lppvoid)
{
	TRACE_MAPI(TRACE_ENTRY, "IExchangeImportContentsChanges::QueryInterface", "");
	METHOD_PROLOGUE_(ECSynchronizationImportChanges, ExchangeImportContentsChanges);
	HRESULT hr = pThis->QueryInterface(refiid, lppvoid);
	TRACE_MAPI(TRACE_RETURN, "IExchangeImportContentsChanges::QueryInterface", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECSynchronizationImportChanges::xExchangeImportContentsChanges::GetLastError(HRESULT hResult, ULONG ulFlags, LPMAPIERROR *lppMAPIError)
{
	TRACE_MAPI(TRACE_ENTRY, "IExchangeImportContentsChanges::GetLastError", "");
	METHOD_PROLOGUE_(ECSynchronizationImportChanges, ExchangeImportContentsChanges);
	HRESULT hr = pThis->GetLastError(hResult, ulFlags, lppMAPIError);
	TRACE_MAPI(TRACE_RETURN, "IExchangeImportHierarchyChanges::GetLastError", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECSynchronizationImportChanges::xExchangeImportContentsChanges::Config(LPSTREAM lpStream, ULONG ulFlags)
{
	TRACE_MAPI(TRACE_ENTRY, "IExchangeImportContentsChanges::Config", "");
	METHOD_PROLOGUE_(ECSynchronizationImportChanges, ExchangeImportContentsChanges);
	HRESULT hr = pThis->Config(lpStream, ulFlags);
	TRACE_MAPI(TRACE_RETURN, "IExchangeImportHierarchyChanges::Config", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECSynchronizationImportChanges::xExchangeImportContentsChanges::UpdateState(LPSTREAM lpStream)
{
	TRACE_MAPI(TRACE_ENTRY, "IExchangeImportContentsChanges::UpdateState", "");
	METHOD_PROLOGUE_(ECSynchronizationImportChanges, ExchangeImportContentsChanges);
	HRESULT hr = pThis->UpdateState(lpStream);
	TRACE_MAPI(TRACE_RETURN, "IExchangeImportHierarchyChanges::UpdateState", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECSynchronizationImportChanges::xExchangeImportContentsChanges::ImportMessageChange(ULONG cValue, LPSPropValue lpPropArray,
																					ULONG ulFlags, LPMESSAGE * lppMessage)
{
	TRACE_MAPI(TRACE_ENTRY, "IExchangeImportContentsChanges::ImportMessageChange", "");
	METHOD_PROLOGUE_(ECSynchronizationImportChanges, ExchangeImportContentsChanges);
	HRESULT hr = pThis->ImportMessageChange(cValue, lpPropArray, ulFlags, lppMessage);
	TRACE_MAPI(TRACE_RETURN, "IExchangeImportHierarchyChanges::ImportMessageChange", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECSynchronizationImportChanges::xExchangeImportContentsChanges::ImportMessageDeletion(ULONG ulFlags, LPENTRYLIST lpSourceEntryList)
{
	TRACE_MAPI(TRACE_ENTRY, "IExchangeImportContentsChanges::ImportMessageDeletion", "");
	METHOD_PROLOGUE_(ECSynchronizationImportChanges, ExchangeImportContentsChanges);
	HRESULT hr = pThis->ImportMessageDeletion(ulFlags, lpSourceEntryList);
	TRACE_MAPI(TRACE_RETURN, "IExchangeImportHierarchyChanges::ImportMessageDeletion", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECSynchronizationImportChanges::xExchangeImportContentsChanges::ImportPerUserReadStateChange(ULONG cElements, LPREADSTATE lpReadState)
{
	TRACE_MAPI(TRACE_ENTRY, "IExchangeImportContentsChanges::ImportPerUserReadStateChange", "");
	METHOD_PROLOGUE_(ECSynchronizationImportChanges, ExchangeImportContentsChanges);
	HRESULT hr = pThis->ImportPerUserReadStateChange(cElements, lpReadState);
	TRACE_MAPI(TRACE_RETURN, "IExchangeImportHierarchyChanges::ImportPerUserReadStateChange", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECSynchronizationImportChanges::xExchangeImportContentsChanges::ImportMessageMove(ULONG cbSourceKeySrcFolder, BYTE FAR * pbSourceKeySrcFolder,
																						  ULONG cbSourceKeySrcMessage, BYTE FAR * pbSourceKeySrcMessage,
																						  ULONG cbPCLMessage, BYTE FAR * pbPCLMessage,
																						  ULONG cbSourceKeyDestMessage, BYTE FAR * pbSourceKeyDestMessage,
																						  ULONG cbChangeNumDestMessage, BYTE FAR * pbChangeNumDestMessage)
{
	TRACE_MAPI(TRACE_ENTRY, "IExchangeImportContentsChanges::ImportMessageMove", "");
	METHOD_PROLOGUE_(ECSynchronizationImportChanges, ExchangeImportContentsChanges);
	HRESULT hr = pThis->ImportMessageMove(cbSourceKeySrcFolder, pbSourceKeySrcFolder,
										  cbSourceKeySrcMessage, pbSourceKeySrcMessage,
										  cbPCLMessage, pbPCLMessage,
										  cbSourceKeyDestMessage, pbSourceKeyDestMessage,
										  cbChangeNumDestMessage, pbChangeNumDestMessage);
	TRACE_MAPI(TRACE_RETURN, "IExchangeImportHierarchyChanges::ImportMessageMove", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

ULONG ECSynchronizationImportChanges::xExchangeImportContentsChanges2::AddRef()
{
	TRACE_MAPI(TRACE_ENTRY, "IExchangeImportContentsChanges2::AddRef", "");
	METHOD_PROLOGUE_(ECSynchronizationImportChanges, ExchangeImportContentsChanges2);
	return pThis->AddRef();
}

ULONG ECSynchronizationImportChanges::xExchangeImportContentsChanges2::Release()
{
	TRACE_MAPI(TRACE_ENTRY, "IExchangeImportContentsChanges2::Release", "");
	METHOD_PROLOGUE_(ECSynchronizationImportChanges, ExchangeImportContentsChanges2);
	return pThis->Release();
}

HRESULT ECSynchronizationImportChanges::xExchangeImportContentsChanges2::QueryInterface(REFIID refiid, LPVOID *lppvoid)
{
	TRACE_MAPI(TRACE_ENTRY, "IExchangeImportContentsChanges2::QueryInterface", "");
	METHOD_PROLOGUE_(ECSynchronizationImportChanges, ExchangeImportContentsChanges2);
	HRESULT hr = pThis->QueryInterface(refiid, lppvoid);
	TRACE_MAPI(TRACE_RETURN, "IExchangeImportContentsChanges2::QueryInterface", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECSynchronizationImportChanges::xExchangeImportContentsChanges2::GetLastError(HRESULT hResult, ULONG ulFlags, LPMAPIERROR *lppMAPIError)
{
	TRACE_MAPI(TRACE_ENTRY, "IExchangeImportContentsChanges2::GetLastError", "");
	METHOD_PROLOGUE_(ECSynchronizationImportChanges, ExchangeImportContentsChanges2);
	HRESULT hr = pThis->GetLastError(hResult, ulFlags, lppMAPIError);
	TRACE_MAPI(TRACE_RETURN, "IExchangeImportContentsChanges2::GetLastError", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECSynchronizationImportChanges::xExchangeImportContentsChanges2::Config(LPSTREAM lpStream, ULONG ulFlags)
{
	TRACE_MAPI(TRACE_ENTRY, "IExchangeImportContentsChanges2::Config", "");
	METHOD_PROLOGUE_(ECSynchronizationImportChanges, ExchangeImportContentsChanges2);
	HRESULT hr = pThis->Config(lpStream, ulFlags);
	TRACE_MAPI(TRACE_RETURN, "IExchangeImportContentsChanges2::Config", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECSynchronizationImportChanges::xExchangeImportContentsChanges2::UpdateState(LPSTREAM lpStream)
{
	TRACE_MAPI(TRACE_ENTRY, "IExchangeImportContentsChanges2::UpdateState", "");
	METHOD_PROLOGUE_(ECSynchronizationImportChanges, ExchangeImportContentsChanges2);
	HRESULT hr = pThis->UpdateState(lpStream);
	TRACE_MAPI(TRACE_RETURN, "IExchangeImportContentsChanges2::UpdateState", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECSynchronizationImportChanges::xExchangeImportContentsChanges2::ImportMessageChange(ULONG cValue, LPSPropValue lpPropArray,
																					 ULONG ulFlags, LPMESSAGE * lppMessage)
{
	TRACE_MAPI(TRACE_ENTRY, "IExchangeImportContentsChanges2::ImportMessageChange", "");
	METHOD_PROLOGUE_(ECSynchronizationImportChanges, ExchangeImportContentsChanges2);
	HRESULT hr = pThis->ImportMessageChange(cValue, lpPropArray, ulFlags, lppMessage);
	TRACE_MAPI(TRACE_RETURN, "IExchangeImportContentsChanges2::ImportMessageChange", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECSynchronizationImportChanges::xExchangeImportContentsChanges2::ImportMessageDeletion(ULONG ulFlags, LPENTRYLIST lpSourceEntryList)
{
	TRACE_MAPI(TRACE_ENTRY, "IExchangeImportContentsChanges2::ImportMessageDeletion", "");
	METHOD_PROLOGUE_(ECSynchronizationImportChanges, ExchangeImportContentsChanges2);
	HRESULT hr = pThis->ImportMessageDeletion(ulFlags, lpSourceEntryList);
	TRACE_MAPI(TRACE_RETURN, "IExchangeImportContentsChanges2::ImportMessageDeletion", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECSynchronizationImportChanges::xExchangeImportContentsChanges2::ImportPerUserReadStateChange(ULONG cElements, LPREADSTATE lpReadState)
{
	TRACE_MAPI(TRACE_ENTRY, "IExchangeImportContentsChanges2::ImportPerUserReadStateChange", "");
	METHOD_PROLOGUE_(ECSynchronizationImportChanges, ExchangeImportContentsChanges2);
	HRESULT hr = pThis->ImportPerUserReadStateChange(cElements, lpReadState);
	TRACE_MAPI(TRACE_RETURN, "IExchangeImportContentsChanges2::ImportPerUserReadStateChange", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECSynchronizationImportChanges::xExchangeImportContentsChanges2::ImportMessageMove(ULONG cbSourceKeySrcFolder, BYTE FAR * pbSourceKeySrcFolder,
																						   ULONG cbSourceKeySrcMessage, BYTE FAR * pbSourceKeySrcMessage,
																						   ULONG cbPCLMessage, BYTE FAR * pbPCLMessage,
																						   ULONG cbSourceKeyDestMessage, BYTE FAR * pbSourceKeyDestMessage,
																						   ULONG cbChangeNumDestMessage, BYTE FAR * pbChangeNumDestMessage)
{
	TRACE_MAPI(TRACE_ENTRY, "IExchangeImportContentsChanges2::ImportMessageMove", "");
	METHOD_PROLOGUE_(ECSynchronizationImportChanges, ExchangeImportContentsChanges2);
	HRESULT hr = pThis->ImportMessageMove(cbSourceKeySrcFolder, pbSourceKeySrcFolder,
										  cbSourceKeySrcMessage, pbSourceKeySrcMessage,
										  cbPCLMessage, pbPCLMessage,
										  cbSourceKeyDestMessage, pbSourceKeyDestMessage,
										  cbChangeNumDestMessage, pbChangeNumDestMessage);
	TRACE_MAPI(TRACE_RETURN, "IExchangeImportContentsChanges2::ImportMessageMove", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECSynchronizationImportChanges::xExchangeImportContentsChanges2::ConfigForConversionStream(LPSTREAM lpStream, ULONG ulFlags,
																						   ULONG cValuesConversion, LPSPropValue lpPropArrayConversion)
{
	TRACE_MAPI(TRACE_ENTRY, "IExchangeImportContentsChanges2::ConfigForConversionStream", "");
	METHOD_PROLOGUE_(ECSynchronizationImportChanges, ExchangeImportContentsChanges2);
	HRESULT hr = pThis->ConfigForConversionStream(lpStream, ulFlags, cValuesConversion, lpPropArrayConversion);
	TRACE_MAPI(TRACE_RETURN, "IExchangeImportContentsChanges2::ConfigForConversionStream", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECSynchronizationImportChanges::xExchangeImportContentsChanges2::ImportMessageChangeAsAStream(ULONG cValue, LPSPropValue lpPropArray, ULONG ulFlags, LPSTREAM *lppstream)
{
	TRACE_MAPI(TRACE_ENTRY, "IExchangeImportContentsChanges2::ImportMessageChangeAsAStream", "");
	METHOD_PROLOGUE_(ECSynchronizationImportChanges, ExchangeImportContentsChanges2);
	HRESULT hr = pThis->ImportMessageChangeAsAStream(cValue, lpPropArray, ulFlags, lppstream);
	TRACE_MAPI(TRACE_RETURN, "IExchangeImportContentsChanges2::ImportMessageChangeAsAStream", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT ECSynchronizationImportChanges::xExchangeImportContentsChanges2::SetMessageInterface(REFIID refiid)
{
	TRACE_MAPI(TRACE_ENTRY, "IExchangeImportContentsChanges2::SetMessageInterface", "");
	METHOD_PROLOGUE_(ECSynchronizationImportChanges, ExchangeImportContentsChanges2);
	HRESULT hr = pThis->SetMessageInterface(refiid);
	TRACE_MAPI(TRACE_RETURN, "IExchangeImportContentsChanges2::SetMessageInterface", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}
