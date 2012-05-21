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
#include "WSMessageStreamExporter.h"
#include "WSSerializedMessage.h"
#include "WSTransport.h"
#include "charset/convert.h"
#include "WSUtil.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/**
 * Create a WSMessageStreamExporter instance.
 * @param[in]	ulOffset	The offset that should be used to index the streams. The server returns [0:B-A), while the client would
 *                          expect [A-B). This offset makes sure GetSerializedObject can be called with an index from the expected
 *                          range.
 * @param[in]	ulCount		The number message streams that should be handled. The actual amount of streams returned from the server
 *                          could be less if those messages didn't exist anymore on the server. This makes sure the client can still
 *                          request those streams and an appropriate error can be returned.
 * @param[in]	streams		The streams (or actually the information about the streams).
 * @param[in]	lpTransport	Pointer to the parent transport. Used to get the streams from the network. This transport MUST be used 
 *                          exclusively by this WSMessageStreamExporter only.
 * @param[out]	lppStreamExporter	The new instance.
 */
HRESULT WSMessageStreamExporter::Create(ULONG ulOffset, ULONG ulCount, const messageStreamArray &streams, WSTransport *lpTransport, WSMessageStreamExporter **lppStreamExporter)
{
	HRESULT hr = hrSuccess;
	StreamInfo* lpsi = NULL;
	WSMessageStreamExporterPtr ptrStreamExporter;
	convert_context converter;

	try {
		ptrStreamExporter.reset(new WSMessageStreamExporter());
	} catch (const std::bad_alloc&) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	
	for (unsigned i = 0; i < streams.__size; ++i) {
		lpsi = new StreamInfo;

		lpsi->id.assign(streams.__ptr[i].sStreamData.xop__Include.id);
		
		hr = MAPIAllocateBuffer(streams.__ptr[i].sPropVals.__size * sizeof(SPropValue), &lpsi->ptrPropVals);
		if (hr != hrSuccess)
			goto exit;
		for (int j = 0; j < streams.__ptr[i].sPropVals.__size; ++j) {
			hr = CopySOAPPropValToMAPIPropVal(&lpsi->ptrPropVals[j], &streams.__ptr[i].sPropVals.__ptr[j], lpsi->ptrPropVals, &converter);
			if (hr != hrSuccess)
				goto exit;
		}
		lpsi->cbPropVals = streams.__ptr[i].sPropVals.__size;

		ptrStreamExporter->m_mapStreamInfo[streams.__ptr[i].ulStep + ulOffset] = lpsi;
		lpsi = NULL;
	}

	ptrStreamExporter->m_ulExpectedIndex = ulOffset;
	ptrStreamExporter->m_ulMaxIndex = ulOffset + ulCount;
	ptrStreamExporter->m_ptrTransport.reset(lpTransport);

	*lppStreamExporter = ptrStreamExporter.release();

exit:
	delete lpsi;
	return hr;
}

/**
 * Check if any more streams are available from the exporter.
 */
bool WSMessageStreamExporter::IsDone() const
{
	assert(m_ulExpectedIndex <= m_ulMaxIndex);
	return m_ulExpectedIndex == m_ulMaxIndex;
}

/**
 * Request a serialized messages.
 * @param[in]	ulIndex		The index of the requested messages stream. The first time this must equal the ulOffset used during
 *                          construction. At each consecutive call this must be one higher than the previous call.
 * @param[out]	lppSerializedMessage	The requested stream.
 *
 * @retval	MAPI_E_INVALID_PARAMETER	ulIndex was not as expected or lppSerializedMessage is NULL
 * @retval	SYNC_E_OBJECT_DELETED		The message was deleted on the server.
 */
HRESULT WSMessageStreamExporter::GetSerializedMessage(ULONG ulIndex, WSSerializedMessage **lppSerializedMessage)
{
	HRESULT hr = hrSuccess;
	StreamInfoMap::const_iterator iStreamInfo;
	WSSerializedMessagePtr ptrMessage;

	if (ulIndex != m_ulExpectedIndex || lppSerializedMessage == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	iStreamInfo = m_mapStreamInfo.find(ulIndex);
	if (iStreamInfo == m_mapStreamInfo.end()) {
		++m_ulExpectedIndex;
		hr = SYNC_E_OBJECT_DELETED;
		goto exit;
	}

	try {
		ptrMessage.reset(new WSSerializedMessage(m_ptrTransport->m_lpCmd->soap, iStreamInfo->second->id, iStreamInfo->second->cbPropVals, iStreamInfo->second->ptrPropVals.get()));
	} catch(const std::bad_alloc &) {
		hr = MAPI_E_NOT_ENOUGH_MEMORY;
		goto exit;
	}

	AddChild(ptrMessage);

	++m_ulExpectedIndex;
	*lppSerializedMessage = ptrMessage.release();	

exit:
	return hr;
}

WSMessageStreamExporter::WSMessageStreamExporter()
{ 
}

WSMessageStreamExporter::~WSMessageStreamExporter()
{
	// Discard data of all remaining streams so all data is read from the network
	if(m_ptrTransport->m_lpCmd) {
		m_ptrTransport->m_lpCmd->soap->fmimewriteopen = NULL;
		m_ptrTransport->m_lpCmd->soap->fmimewrite = NULL;
		m_ptrTransport->m_lpCmd->soap->fmimewriteclose = NULL;

		while (soap_get_mime_attachment(m_ptrTransport->m_lpCmd->soap, NULL));	// This is a loop!
	}

	for (StreamInfoMap::iterator i = m_mapStreamInfo.begin(); i != m_mapStreamInfo.end(); ++i)
		delete i->second;
}
