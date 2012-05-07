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
#include "WSMessageStreamImporter.h"
#include "WSUtil.h"
#include "ECSyncSettings.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

/////////////////////////////////////
// WSMessageStreamSink Implementation

/**
 * Create a new WSMessageStreamSink instance
 * @param[in]	lpFifoBuffer	The fifobuffer to write the data into.
 * @param[in]	ulTimeout		The timeout in ms to use when writing to the
 * 								fifobuffer.
 * @param[out]	lppSink			The newly created object
 */
HRESULT WSMessageStreamSink::Create(ECFifoBuffer *lpFifoBuffer, ULONG ulTimeout, WSMessageStreamSink **lppSink)
{
	HRESULT hr = hrSuccess;
	WSMessageStreamSinkPtr ptrSink;

	if (lpFifoBuffer == NULL || lppSink == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	try {
		ptrSink.reset(new WSMessageStreamSink(lpFifoBuffer, ulTimeout));
	} catch (const std::bad_alloc &) {
		hr = MAPI_E_NOT_ENOUGH_MEMORY;
		goto exit;
	}

	*lppSink = ptrSink.release();

exit:
	return hr;
}

/**
 * Write data into the underlaying fifo buffer.
 * @param[in]	lpData	Pointer to the data
 * @param[in]	cbData	The amount of data in bytes.
 */
HRESULT WSMessageStreamSink::Write(LPVOID lpData, ULONG cbData)
{
	return ZarafaErrorToMAPIError(m_lpFifoBuffer->Write(lpData, cbData, m_ulTimeout, NULL));
}

/**
 * Constructor
 * @param[in]	lpFifoBuffer	The fifobuffer to write the data into.
 */
WSMessageStreamSink::WSMessageStreamSink(ECFifoBuffer *lpFifoBuffer, ULONG ulTimeout)
: m_lpFifoBuffer(lpFifoBuffer)
, m_ulTimeout(ulTimeout)
{ }

/**
 * Destructor
 * Closes the underlaying fifo buffer, causing the reader to stop reading.
 */
WSMessageStreamSink::~WSMessageStreamSink()
{
	m_lpFifoBuffer->Close();
}


/////////////////////////////////////////
// WSMessageStreamImporter Implementation
HRESULT WSMessageStreamImporter::Create(ULONG ulFlags, ULONG ulSyncId, ULONG cbEntryID, LPENTRYID lpEntryID, ULONG cbFolderEntryID, LPENTRYID lpFolderEntryID, bool bNewMessage, LPSPropValue lpConflictItems, WSTransport *lpTransport, WSMessageStreamImporter **lppStreamImporter)
{
	HRESULT hr = hrSuccess;
	entryId sEntryId = {0};
	entryId sFolderEntryId = {0};
	propVal sConflictItems = {0};
	WSMessageStreamImporterPtr ptrStreamImporter;
	ECSyncSettings* lpSyncSettings = NULL;

	if (lppStreamImporter == NULL || 
		lpEntryID == NULL || cbEntryID == 0 || 
		lpFolderEntryID == NULL || cbFolderEntryID == 0 || 
		(bNewMessage == true && lpConflictItems != NULL) ||
		lpTransport == NULL)
	{
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	hr = CopyMAPIEntryIdToSOAPEntryId(cbEntryID, lpEntryID, &sEntryId, false);
	if (hr != hrSuccess)
		goto exit;

	hr = CopyMAPIEntryIdToSOAPEntryId(cbFolderEntryID, lpFolderEntryID, &sFolderEntryId, false);
	if (hr != hrSuccess)
		goto exit;

	if (lpConflictItems) {
		hr = CopyMAPIPropValToSOAPPropVal(&sConflictItems, lpConflictItems);
		if (hr != hrSuccess)
			goto exit;
	}

	lpSyncSettings = ECSyncSettings::GetInstance();

	try {
		ptrStreamImporter.reset(new WSMessageStreamImporter(ulFlags, ulSyncId, sEntryId, sFolderEntryId, bNewMessage, sConflictItems, lpTransport, lpSyncSettings->StreamBufferSize(), lpSyncSettings->StreamTimeout()));

		// The following are now owned by the stream importer
		sEntryId.__ptr = NULL;
		sFolderEntryId.__ptr = NULL;
		sConflictItems.Value.bin = NULL;
	} catch (const std::bad_alloc &) {
		hr = MAPI_E_NOT_ENOUGH_MEMORY;
		goto exit;
	}

	*lppStreamImporter = ptrStreamImporter.release();

exit:
	delete[] sEntryId.__ptr;
	delete[] sFolderEntryId.__ptr;
	if (sConflictItems.Value.bin)
		delete[] sConflictItems.Value.bin->__ptr;
	delete[] sConflictItems.Value.bin;

	return hr;
}

HRESULT WSMessageStreamImporter::StartTransfer(WSMessageStreamSink **lppSink)
{
	HRESULT hr = hrSuccess;
	WSMessageStreamSinkPtr ptrSink;
	
	if (m_threadPool.dispatch(this) == false) {
		hr = MAPI_E_CALL_FAILED;
		goto exit;
	}

	hr = WSMessageStreamSink::Create(&m_fifoBuffer, m_ulTimeout, &ptrSink);
	if (hr != hrSuccess) {
		m_fifoBuffer.Close();
		goto exit;
	}

	AddChild(ptrSink);

	*lppSink = ptrSink.release();

exit:
	return hr;
}

HRESULT WSMessageStreamImporter::GetAsyncResult(HRESULT *lphrResult)
{
	HRESULT hr = hrSuccess;

	if (lphrResult == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	if (wait(m_ulTimeout) == false) {
		hr = MAPI_E_TIMEOUT;
		goto exit;
	}

	*lphrResult = m_hr;

exit:
	return hr;
}

WSMessageStreamImporter::WSMessageStreamImporter(ULONG ulFlags, ULONG ulSyncId, const entryId &sEntryId, const entryId &sFolderEntryId, bool bNewMessage, const propVal &sConflictItems, WSTransport *lpTransport, ULONG ulBufferSize, ULONG ulTimeout)
: m_ulFlags(ulFlags)
, m_ulSyncId(ulSyncId)
, m_sEntryId(sEntryId)
, m_sFolderEntryId(sFolderEntryId)
, m_bNewMessage(bNewMessage)
, m_sConflictItems(sConflictItems)
, m_ptrTransport(lpTransport, true)
, m_fifoBuffer(ulBufferSize)
, m_threadPool(1)
, m_ulTimeout(ulTimeout)
{ 
}

WSMessageStreamImporter::~WSMessageStreamImporter()
{ 
	delete[] m_sEntryId.__ptr;
	delete[] m_sFolderEntryId.__ptr;
	if (m_sConflictItems.Value.bin)
		delete[] m_sConflictItems.Value.bin->__ptr;
	delete[] m_sConflictItems.Value.bin;
}

void WSMessageStreamImporter::run()
{
	unsigned int ulResult = 0;
	xsd__Binary	sStreamData = {{0}};

	struct soap *lpSoap = m_ptrTransport->m_lpCmd->soap;
	propVal *lpsConflictItems = NULL;

	if (m_sConflictItems.ulPropTag != 0)
		lpsConflictItems = &m_sConflictItems;

	sStreamData.xop__Include.__ptr = (unsigned char*)this;
	sStreamData.xop__Include.type = "application/binary";

	m_ptrTransport->LockSoap();

	soap_set_omode(lpSoap, SOAP_ENC_MTOM | SOAP_IO_CHUNK);	
	lpSoap->fmimereadopen = &StaticMTOMReadOpen;
	lpSoap->fmimeread = &StaticMTOMRead;
	lpSoap->fmimereadclose = &StaticMTOMReadClose;

	m_hr = hrSuccess;
	if (m_ptrTransport->m_lpCmd->ns__importMessageFromStream(m_ptrTransport->m_ecSessionId, m_ulFlags, m_ulSyncId, m_sFolderEntryId, m_sEntryId, m_bNewMessage, lpsConflictItems, sStreamData, &ulResult) != SOAP_OK) {
		m_hr = MAPI_E_NETWORK_ERROR;
	} else if (m_hr == hrSuccess) {	// Could be set from callback
		m_hr = ZarafaErrorToMAPIError(ulResult, MAPI_E_NOT_FOUND);
	}

	m_ptrTransport->UnLockSoap();
}

void* WSMessageStreamImporter::StaticMTOMReadOpen(struct soap *soap, void *handle, const char *id, const char *type, const char *description)
{
	WSMessageStreamImporter	*lpImporter = reinterpret_cast<WSMessageStreamImporter*>(handle);
	return lpImporter->MTOMReadOpen(soap, handle, id, type, description);
}

size_t WSMessageStreamImporter::StaticMTOMRead(struct soap *soap, void *handle, char *buf, size_t len)
{
	WSMessageStreamImporter	*lpImporter = reinterpret_cast<WSMessageStreamImporter*>(handle);
	return lpImporter->MTOMRead(soap, handle, buf, len);
}

void WSMessageStreamImporter::StaticMTOMReadClose(struct soap *soap, void *handle)
{
	WSMessageStreamImporter	*lpImporter = reinterpret_cast<WSMessageStreamImporter*>(handle);
	lpImporter->MTOMReadClose(soap, handle);
}

void* WSMessageStreamImporter::MTOMReadOpen(struct soap* /*soap*/, void *handle, const char* /*id*/, const char* /*type*/, const char* /*description*/)
{
	return handle;
}

size_t WSMessageStreamImporter::MTOMRead(struct soap* soap, void* /*handle*/, char *buf, size_t len)
{
	ECRESULT er = erSuccess;
	ECFifoBuffer::size_type cbRead = 0;

	er = m_fifoBuffer.Read(buf, len, m_ulTimeout, &cbRead);
	if (er != erSuccess) {
		m_hr = ZarafaErrorToMAPIError(er);
		return 0;
	}

	return cbRead;
}

void WSMessageStreamImporter::MTOMReadClose(struct soap* /*soap*/, void* /*handle*/)
{
	m_fifoBuffer.Close();
}
