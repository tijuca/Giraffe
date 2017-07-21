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

#include <kopano/platform.h>
#include <memory>
#include "ECSyncUtil.h"
#include <kopano/mapi_ptr.h>
#include <kopano/memory.hpp>
#include <mapix.h>

using namespace KCHL;

namespace KC {

HRESULT HrDecodeSyncStateStream(LPSTREAM lpStream, ULONG *lpulSyncId, ULONG *lpulChangeId, PROCESSEDCHANGESSET *lpSetProcessChanged)
{
	HRESULT		hr = hrSuccess;
	STATSTG		stat;
	ULONG		ulSyncId = 0;
	ULONG		ulChangeId = 0;
	ULONG		ulChangeCount = 0;
	ULONG		ulProcessedChangeId = 0;
	ULONG		ulSourceKeySize = 0;
	LARGE_INTEGER		liPos = {{0, 0}};
	PROCESSEDCHANGESSET setProcessedChanged;

	hr = lpStream->Stat(&stat, STATFLAG_NONAME);
	if(hr != hrSuccess)
		return hr;
	
	if (stat.cbSize.HighPart == 0 && stat.cbSize.LowPart == 0) {
		ulSyncId = 0;
		ulChangeId = 0;
	} else {
		if (stat.cbSize.HighPart != 0 || stat.cbSize.LowPart < 8)
			return MAPI_E_INVALID_PARAMETER;
		hr = lpStream->Seek(liPos, STREAM_SEEK_SET, NULL);
		if (hr != hrSuccess)
			return hr;
		hr = lpStream->Read(&ulSyncId, 4, NULL);
		if (hr != hrSuccess)
			return hr;
		hr = lpStream->Read(&ulChangeId, 4, NULL);
		if (hr != hrSuccess)
			return hr;
			
		// Following the sync ID and the change ID is the list of changes that were already processed for
		// this sync ID / change ID combination. This allows us partial processing of items retrieved from 
		// the server.
		if (lpSetProcessChanged != NULL && lpStream->Read(&ulChangeCount, 4, NULL) == hrSuccess) {
			// The stream contains a list of already processed items, read them
			
			for (ULONG i = 0; i < ulChangeCount; ++i) {
				std::unique_ptr<char[]> lpData;

				hr = lpStream->Read(&ulProcessedChangeId, 4, NULL);
				if (hr != hrSuccess)
					/* Not the amount of expected bytes are there */
					return hr;
				hr = lpStream->Read(&ulSourceKeySize, 4, NULL);
				if (hr != hrSuccess)
					return hr;
				if (ulSourceKeySize > 1024)
					// Stupidly large source key, the stream must be bad.
					return MAPI_E_INVALID_PARAMETER;
				lpData.reset(new char[ulSourceKeySize]);
				hr = lpStream->Read(lpData.get(), ulSourceKeySize, NULL);
				if(hr != hrSuccess)
					return hr;
				setProcessedChanged.insert(std::pair<unsigned int, std::string>(ulProcessedChangeId, std::string(lpData.get(), ulSourceKeySize)));
			}
		}
	}

	if (lpulSyncId)
		*lpulSyncId = ulSyncId;

	if (lpulChangeId)
		*lpulChangeId = ulChangeId;

	if (lpSetProcessChanged)
		lpSetProcessChanged->insert(setProcessedChanged.begin(), setProcessedChanged.end());
	return hrSuccess;
}

HRESULT ResetStream(LPSTREAM lpStream)
{
	LARGE_INTEGER liPos = {{0, 0}};
	ULARGE_INTEGER uliSize = {{8, 0}};
	HRESULT hr = lpStream->Seek(liPos, STREAM_SEEK_SET, NULL);
	if (hr != hrSuccess)
		return hr;
	hr = lpStream->SetSize(uliSize);
	if (hr != hrSuccess)
		return hr;
	hr = lpStream->Write("\0\0\0\0\0\0\0\0", 8, NULL);
	if (hr != hrSuccess)
		return hr;
	return lpStream->Seek(liPos, STREAM_SEEK_SET, NULL);
}

HRESULT CreateNullStatusStream(LPSTREAM *lppStream)
{
	StreamPtr ptrStream;

	HRESULT hr = CreateStreamOnHGlobal(GlobalAlloc(GPTR, 8), true, &~ptrStream);
	if (hr != hrSuccess)
		return hr;
	hr = ResetStream(ptrStream);
	if (hr != hrSuccess)
		return hr;
	return ptrStream->QueryInterface(IID_IStream,
	       reinterpret_cast<LPVOID *>(lppStream));
}

HRESULT HrGetOneBinProp(IMAPIProp *lpProp, ULONG ulPropTag, LPSPropValue *lppPropValue)
{
	HRESULT hr = hrSuccess;
	object_ptr<IStream> lpStream;
	memory_ptr<SPropValue> lpPropValue;
	STATSTG sStat;
	ULONG ulRead = 0;

	if (lpProp == nullptr)
		return MAPI_E_INVALID_PARAMETER;
	hr = lpProp->OpenProperty(ulPropTag, &IID_IStream, 0, 0, &~lpStream);
	if(hr != hrSuccess)
		return hr;
	hr = lpStream->Stat(&sStat, 0);
	if(hr != hrSuccess)
		return hr;
	hr = MAPIAllocateBuffer(sizeof(SPropValue), &~lpPropValue);
	if(hr != hrSuccess)
		return hr;
	hr = MAPIAllocateMore(sStat.cbSize.LowPart, lpPropValue, (void **) &lpPropValue->Value.bin.lpb);
	if(hr != hrSuccess)
		return hr;
	hr = lpStream->Read(lpPropValue->Value.bin.lpb, sStat.cbSize.LowPart, &ulRead);
	if(hr != hrSuccess)
		return hr;
	lpPropValue->Value.bin.cb = ulRead;

	*lppPropValue = lpPropValue.release();
	return hrSuccess;
}

} /* namespace */
