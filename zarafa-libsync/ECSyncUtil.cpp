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
#include "ECSyncUtil.h"
#include "mapi_ptr.h"

#include <mapix.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

HRESULT HrDecodeSyncStateStream(LPSTREAM lpStream, ULONG *lpulSyncId, ULONG *lpulChangeId, PROCESSEDCHANGESSET *lpSetProcessChanged)
{
	HRESULT		hr = hrSuccess;
	STATSTG		stat;
	ULONG		ulSyncId = 0;
	ULONG		ulChangeId = 0;
	ULONG		ulChangeCount = 0;
	ULONG		ulProcessedChangeId = 0;
	ULONG		ulSourceKeySize = 0;
	char		*lpData = NULL;

	LARGE_INTEGER		liPos = {{0, 0}};
	PROCESSEDCHANGESSET setProcessedChanged;

	hr = lpStream->Stat(&stat, STATFLAG_NONAME);
	if(hr != hrSuccess)
		goto exit;
	
	if (stat.cbSize.HighPart == 0 && stat.cbSize.LowPart == 0) {
		ulSyncId = 0;
		ulChangeId = 0;
	} else {
		if (stat.cbSize.HighPart != 0 || stat.cbSize.LowPart < 8){
			hr = MAPI_E_INVALID_PARAMETER;
			goto exit;
		}
		
		hr = lpStream->Seek(liPos, STREAM_SEEK_SET, NULL);
		if (hr != hrSuccess)
			goto exit;

		hr = lpStream->Read(&ulSyncId, 4, NULL);
		if (hr != hrSuccess)
			goto exit;

		hr = lpStream->Read(&ulChangeId, 4, NULL);
		if (hr != hrSuccess)
			goto exit;
			
		// Following the sync ID and the change ID is the list of changes that were already processed for
		// this sync ID / change ID combination. This allows us partial processing of items retrieved from 
		// the server.
		if (lpSetProcessChanged != NULL && lpStream->Read(&ulChangeCount, 4, NULL) == hrSuccess) {
			// The stream contains a list of already processed items, read them
			
			for (ULONG i = 0; i < ulChangeCount; ++i) {
				hr = lpStream->Read(&ulProcessedChangeId, 4, NULL);
				if (hr != hrSuccess)
					goto exit; // Not the amount of expected bytes are there
				
				hr = lpStream->Read(&ulSourceKeySize, 4, NULL);
				if (hr != hrSuccess)
					goto exit;
					
				if (ulSourceKeySize > 1024) {
					// Stupidly large source key, the stream must be bad.
					hr = MAPI_E_INVALID_PARAMETER;
					goto exit;
				}
					
				lpData = new char[ulSourceKeySize];
					
				hr = lpStream->Read(lpData, ulSourceKeySize, NULL);
				if(hr != hrSuccess)
					goto exit;
					
				setProcessedChanged.insert(std::pair<unsigned int, std::string>(ulProcessedChangeId, std::string(lpData, ulSourceKeySize)));
				
				delete []lpData;
				lpData =  NULL;
			}
		}
	}

	if (lpulSyncId)
		*lpulSyncId = ulSyncId;

	if (lpulChangeId)
		*lpulChangeId = ulChangeId;

	if (lpSetProcessChanged)
		lpSetProcessChanged->insert(setProcessedChanged.begin(), setProcessedChanged.end());

exit:
	if (lpData)
		delete[] lpData;

	return hr;
}

HRESULT ResetStream(LPSTREAM lpStream)
{
	HRESULT hr = hrSuccess;
	LARGE_INTEGER liPos = {{0, 0}};
	ULARGE_INTEGER uliSize = {{8, 0}};
	hr = lpStream->Seek(liPos, STREAM_SEEK_SET, NULL);
	if (hr != hrSuccess)
		goto exit;
	hr = lpStream->SetSize(uliSize);
	if (hr != hrSuccess)
		goto exit;
	hr = lpStream->Write("\0\0\0\0\0\0\0\0", 8, NULL);
	if (hr != hrSuccess)
		goto exit;
	hr = lpStream->Seek(liPos, STREAM_SEEK_SET, NULL);

exit:
	return hr;
}

HRESULT CreateNullStatusStream(LPSTREAM *lppStream)
{
	HRESULT hr = hrSuccess;
	StreamPtr ptrStream;

	hr = CreateStreamOnHGlobal(GlobalAlloc(GPTR, 8), true, &ptrStream);
	if (hr != hrSuccess)
		goto exit;

	hr = ResetStream(ptrStream);
	if (hr != hrSuccess)
		goto exit;

	hr = ptrStream->QueryInterface(IID_IStream, (LPVOID*)lppStream);

exit:
	return hr;
}

HRESULT HrGetOneBinProp(IMAPIProp *lpProp, ULONG ulPropTag, LPSPropValue *lppPropValue)
{
	HRESULT hr = hrSuccess;
	IStream *lpStream = NULL;
	LPSPropValue lpPropValue = NULL;
	STATSTG sStat;
	ULONG ulRead = 0;

	if(!lpProp){
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	hr = lpProp->OpenProperty(ulPropTag, &IID_IStream, 0, 0, (IUnknown **)&lpStream);
	if(hr != hrSuccess)
		goto exit;

	hr = lpStream->Stat(&sStat, 0);
	if(hr != hrSuccess)
		goto exit;

	hr = MAPIAllocateBuffer(sizeof(SPropValue), (void **)&lpPropValue);
	if(hr != hrSuccess)
		goto exit;

	hr = MAPIAllocateMore(sStat.cbSize.LowPart, lpPropValue, (void **) &lpPropValue->Value.bin.lpb);
	if(hr != hrSuccess)
		goto exit;

	hr = lpStream->Read(lpPropValue->Value.bin.lpb, sStat.cbSize.LowPart, &ulRead);
	if(hr != hrSuccess)
		goto exit;

	lpPropValue->Value.bin.cb = ulRead;

	*lppPropValue = lpPropValue;

exit:
	if(hr != hrSuccess && lpPropValue)
		MAPIFreeBuffer(lpPropValue);

	if(lpStream)
		lpStream->Release();

	return hr;
}
