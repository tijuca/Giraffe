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
#include "WSSerializedMessage.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/**
 * Constructor
 * @param[in]	lpSoap		The gSoap object from which the MTOM attachments must be obtained.
 * @param[in]	strStreamId	The expected stream id. Used to validate the MTOM attachment obtained from gSoap.
 * @param[in]	cbProps		The amount of properties returned from the original soap call.
 * @param[in]	lpProps		The properties returned from the original soap call. Only the pointer is stored, no
 *                          copy is made since our parent object will stay alive for our complete lifetime.
 */
WSSerializedMessage::WSSerializedMessage(soap *lpSoap, const std::string strStreamId, ULONG cbProps, LPSPropValue lpProps)
: m_lpSoap(lpSoap)
, m_strStreamId(strStreamId)
, m_cbProps(cbProps)
, m_lpProps(lpProps)
, m_bUsed(false)
{
}

/**
 * Get a copy of the properties stored with this object.
 * @param[out]	lpcbProps	The amount of properties returned.
 * @param[out]	lppProps	A copy of the properties. The caller is responsible for deleting them.
 */
HRESULT WSSerializedMessage::GetProps(ULONG *lpcbProps, LPSPropValue *lppProps)
{
	HRESULT hr = hrSuccess;

	if (lpcbProps == NULL || lppProps == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	hr = Util::HrCopyPropertyArray(m_lpProps, m_cbProps, lppProps, lpcbProps);

exit:
	return hr;
}

/**
 * Copy the message stream to an IStream instance.
 * @param[in]	lpDestStream	The stream to write the data to.
 * @retval	MAPI_E_INVALID_PARAMETER	lpDestStream is NULL.
 */
HRESULT WSSerializedMessage::CopyData(LPSTREAM lpDestStream)
{
	HRESULT hr = hrSuccess;

	if (lpDestStream == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	hr = DoCopyData(lpDestStream);
	if (hr != hrSuccess)
		goto exit;

	hr = lpDestStream->Commit(0);

exit:
	return hr;
}

/**
 * Read the data from the gSoap MTOM attachment, but discard it immediately.
 */
HRESULT WSSerializedMessage::DiscardData()
{
	// The data must be read from the network.
	return DoCopyData(NULL);
}

/**
 * Copy the message stream to an IStream instance.
 * @param[in]	lpDestStream	The stream to write the data to. If lpDestStream is
 *                              NULL, the data will be discarded.
 */
HRESULT WSSerializedMessage::DoCopyData(LPSTREAM lpDestStream)
{
	HRESULT hr = hrSuccess;

	if (m_bUsed) {
		hr = MAPI_E_UNCONFIGURED;
		goto exit;
	}

	m_bUsed = true;
	m_hr = hrSuccess;
	m_ptrDestStream.reset(lpDestStream);

	m_lpSoap->fmimewriteopen = StaticMTOMWriteOpen;
	m_lpSoap->fmimewrite = StaticMTOMWrite;
	m_lpSoap->fmimewriteclose = StaticMTOMWriteClose;

	soap_get_mime_attachment(m_lpSoap, (void*)this);
    if (m_lpSoap->error) {
		hr = MAPI_E_NETWORK_ERROR;
		goto exit;
	}
	hr = m_hr;

exit:
	return hr;
}




void* WSSerializedMessage::StaticMTOMWriteOpen(struct soap *soap, void *handle, const char *id, const char *type, const char *description, enum soap_mime_encoding encoding)
{
	WSSerializedMessage	*lpMessage = reinterpret_cast<WSSerializedMessage*>(handle);
	return lpMessage->MTOMWriteOpen(soap, handle, id, type, description, encoding);
}

int WSSerializedMessage::StaticMTOMWrite(struct soap *soap, void *handle, const char *buf, size_t len)
{
	WSSerializedMessage	*lpMessage = reinterpret_cast<WSSerializedMessage*>(handle);
	return lpMessage->MTOMWrite(soap, handle, buf, len);
}

void WSSerializedMessage::StaticMTOMWriteClose(struct soap *soap, void *handle)
{
	WSSerializedMessage	*lpMessage = reinterpret_cast<WSSerializedMessage*>(handle);
	lpMessage->MTOMWriteClose(soap, handle);
}

void* WSSerializedMessage::MTOMWriteOpen(struct soap *soap, void *handle, const char *id, const char* /*type*/, const char* /*description*/, enum soap_mime_encoding encoding)
{
	if (encoding != SOAP_MIME_BINARY || m_strStreamId.compare(id) != 0) {
		soap->error = SOAP_ERR;
		m_hr = MAPI_E_INVALID_TYPE;
		m_ptrDestStream.reset();
	}

	return handle;
}

int WSSerializedMessage::MTOMWrite(struct soap *soap, void* /*handle*/, const char *buf, size_t len)
{
	HRESULT hr = hrSuccess;
	ULONG cbWritten = 0;

	if (!m_ptrDestStream)
		return SOAP_OK;

	hr = m_ptrDestStream->Write(buf, (ULONG)len, &cbWritten);
	if (hr != hrSuccess) {
		soap->error = SOAP_ERR;
		m_hr = hr;
		m_ptrDestStream.reset();
	}
	// @todo: Should we check if everything was written?

	return SOAP_OK;
}

void WSSerializedMessage::MTOMWriteClose(struct soap *soap, void *handle)
{
	m_ptrDestStream.reset();
}
