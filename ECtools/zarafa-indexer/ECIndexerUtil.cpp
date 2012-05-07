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

#include <algorithm>

#include <mapidefs.h>
#include <mapiext.h>
#include <mapiutil.h>
#include <mapix.h>

#include <edkmdb.h>

#include <rtfutil.h>
#include <stringutil.h>
#include <Util.h>

#include "ECIndexerUtil.h"
#include "zarafa-indexer.h"

#include "charset/convert.h"
#include "charset/utf8string.h"

#include "mapi_ptr.h"


HRESULT StreamToNextObject(ECSerializer *lpSerializer, ULONG ulObjType, ULONG ulProps)
{
	HRESULT hr = hrSuccess;
	ECRESULT er = erSuccess;
	SPropValue sProp;
	ULONG ulCount = 0;
	ULONG ulSize = 0;

	/* Number of properties unknown, we are at the start of the object */
	if (!ulProps) {
		er = lpSerializer->Read(&ulProps, sizeof(ulProps), 1);
		if (er != hrSuccess)
			goto exit;
	}

	/* Skip properties */
	while (ulProps--) {
		er = lpSerializer->Read(&sProp.ulPropTag, sizeof(sProp.ulPropTag), 1);
		if (er != hrSuccess)
			goto exit;

		switch (PROP_TYPE(sProp.ulPropTag)) {
		case PT_SHORT:
			ulSize = sizeof(sProp.Value.i);
			break;
		case PT_LONG:
			ulSize = sizeof(sProp.Value.l);
			break;
		case PT_FLOAT:
			ulSize = sizeof(sProp.Value.flt);
			break;
		case PT_DOUBLE:
			ulSize = sizeof(sProp.Value.dbl);
			break;
		case PT_CURRENCY:
			ulSize = sizeof(sProp.Value.cur.int64);
			break;
		case PT_APPTIME:
			ulSize = sizeof(sProp.Value.at);
			break;
		case PT_BOOLEAN:
			/* lpProp->Value.b is unsigned short while server is sending bool */
			ulSize = sizeof(bool);
			break;
		case PT_LONGLONG:
			ulSize = sizeof(sProp.Value.li.QuadPart);
			break;
		case PT_SYSTIME:
			ulSize = sizeof(sProp.Value.ft);
			break;
		case PT_ERROR:
			ulSize = sizeof(sProp.Value.err);
			break;
		case PT_STRING8:
		case PT_UNICODE:
		case PT_BINARY:
		case PT_CLSID:
			er = lpSerializer->Read(&ulSize, sizeof(ulSize), 1);
			if (er != erSuccess)
				goto exit;

			ulSize *= sizeof(unsigned char);
			break;
		case PT_MV_I2:
			er = lpSerializer->Read(&ulCount, sizeof(ulCount), 1);
			if (er != erSuccess)
				goto exit;

			ulSize = ulCount * sizeof(*sProp.Value.MVi.lpi);
			break;
		case PT_MV_I4:
			er = lpSerializer->Read(&ulCount, sizeof(ulCount), 1);
			if (er != erSuccess)
				goto exit;

			ulSize = ulCount * sizeof(*sProp.Value.MVl.lpl);
			break;
		case PT_MV_I8:
			er = lpSerializer->Read(&ulCount, sizeof(ulCount), 1);
			if (er != erSuccess)
				goto exit;

			ulSize = ulCount * sizeof(*sProp.Value.MVli.lpli);
			break;
		case PT_MV_R4:
			er = lpSerializer->Read(&ulCount, sizeof(ulCount), 1);
			if (er != erSuccess)
				goto exit;

			ulSize = ulCount * sizeof(*sProp.Value.MVflt.lpflt);
			break;
		case PT_MV_DOUBLE:
			er = lpSerializer->Read(&ulCount, sizeof(ulCount), 1);
			if (er != erSuccess)
				goto exit;

			ulSize = ulCount * sizeof(*sProp.Value.MVdbl.lpdbl);
			break;
		case PT_MV_APPTIME:
			er = lpSerializer->Read(&ulCount, sizeof(ulCount), 1);
			if (er != erSuccess)
				goto exit;

			ulSize = ulCount * sizeof(*sProp.Value.MVat.lpat);
			break;
		case PT_MV_CURRENCY:
			er = lpSerializer->Read(&ulCount, sizeof(ulCount), 1);
			if (er != erSuccess)
				goto exit;

			ulSize = ulCount * sizeof(*sProp.Value.MVcur.lpcur);
			break;
		case PT_MV_SYSTIME:
			er = lpSerializer->Read(&ulCount, sizeof(ulCount), 1);
			if (er != erSuccess)
				goto exit;

			ulSize = ulCount * sizeof(*sProp.Value.MVft.lpft);
			break;
		case PT_MV_BINARY:
		case PT_MV_CLSID:
		case PT_MV_STRING8:
		case PT_MV_UNICODE:
			er = lpSerializer->Read(&ulCount, sizeof(ulCount), 1);
			if (er != erSuccess)
				goto exit;

			for (unsigned int i = 0; i < ulCount; i++) {
				er = lpSerializer->Read(&ulSize, sizeof(ulSize), 1);
				if (er != erSuccess)
					goto exit;

				er = lpSerializer->Skip(sizeof(unsigned char), ulSize);
				if (er != erSuccess)
					goto exit;
			}

			ulSize = 0;
			break;
		case PT_UNSPECIFIED:
		case PT_NULL:
		case PT_OBJECT:
		default:
			hr = MAPI_E_NO_SUPPORT;
			goto exit;
		}

		if (ulSize) {
			er = lpSerializer->Skip(sizeof(unsigned char), ulSize);
			if (er != erSuccess)
				goto exit;
		}

		if (PROP_ID(sProp.ulPropTag) >= 0x8500) {
			// only named properties above 0x8500 send the named data, since 0x8000 - 0x8500 is static
			// we don't need the actual named property data, so seek past this
			er = lpSerializer->Skip(sizeof(GUID), 1);
			if (er != erSuccess)
				goto exit;

			// get kind of named id
			er = lpSerializer->Read(&ulSize, sizeof(ULONG), 1);
			if (er != erSuccess)
				goto exit;

			if (ulSize != MNID_ID && ulSize != MNID_STRING) {
				hr = MAPI_E_CORRUPT_DATA;
				goto exit;
			}

			if (ulSize == MNID_STRING) {
				er = lpSerializer->Read(&ulSize, sizeof(ULONG), 1);
				if (er != erSuccess)
					goto exit;

				er = lpSerializer->Skip(ulSize, 1);
			} else {
				er = lpSerializer->Skip(sizeof(LONG), 1);
			}
			if (er != erSuccess)
				goto exit;
		}

	}

	/* Skip subjobjects */
	if (ulObjType == MAPI_MESSAGE || ulObjType == MAPI_ATTACH) {
		er = lpSerializer->Read(&ulCount, sizeof(ulCount), 1);
		if (er != erSuccess)
			goto exit;

		while (ulCount--) {
			er = lpSerializer->Read(&ulObjType, sizeof(ulObjType), 1);
			if (er != erSuccess)
				goto exit;

			er = lpSerializer->Read(&ulSize, sizeof(ulSize), 1);
			if (er != erSuccess)
				goto exit;

			hr = StreamToNextObject(lpSerializer, ulObjType);
			if (hr != hrSuccess)
				goto exit;
		}
	}

exit:
	if (er != erSuccess)
		hr = ZarafaErrorToMAPIError(er);

	return hr;
}

HRESULT FlushStream(ECSerializer *lpSerializer)
{
	HRESULT hr = hrSuccess;
	ECRESULT er = erSuccess;
	char buf[4096];
	
	while (er == erSuccess)
		er = lpSerializer->Read(buf, 1, sizeof(buf));
	
	if (er == ZARAFA_E_CALL_FAILED)
		er = erSuccess;
	
	if (er != erSuccess)
		hr = ZarafaErrorToMAPIError(er);
		
	return hr;
}

HRESULT StreamToProperty(ECSerializer *lpSerializer, LPSPropValue *lppProp, LPMAPINAMEID *lppNameID)
{
	HRESULT hr = hrSuccess;
	ECRESULT er = erSuccess;
	LPSPropValue lpProp = NULL;
	LPMAPINAMEID lpNameID = NULL;
	ULONG ulLen = 0;
	convert_context converter;

	hr = MAPIAllocateBuffer(sizeof(*lpProp), (LPVOID *)&lpProp);
	if (hr != hrSuccess)
		goto exit;

	er = lpSerializer->Read(&lpProp->ulPropTag, sizeof(lpProp->ulPropTag), 1);
	if (er != erSuccess)
		goto exit;

	switch (PROP_TYPE(lpProp->ulPropTag)) {
	case PT_SHORT:
		er = lpSerializer->Read(&lpProp->Value.i, sizeof(lpProp->Value.i), 1);
		if (er != erSuccess)
			goto exit;
		break;
	case PT_LONG:
		er = lpSerializer->Read(&lpProp->Value.l, sizeof(lpProp->Value.l), 1);
		if (er != erSuccess)
			goto exit;
		break;
	case PT_FLOAT:
		er = lpSerializer->Read(&lpProp->Value.flt, sizeof(lpProp->Value.flt), 1);
		if (er != erSuccess)
			goto exit;
		break;
	case PT_DOUBLE:
		er = lpSerializer->Read(&lpProp->Value.dbl, sizeof(lpProp->Value.dbl), 1);
		if (er != erSuccess)
			goto exit;
		break;
	case PT_CURRENCY:
		er = lpSerializer->Read(&lpProp->Value.cur, sizeof(lpProp->Value.cur.int64), 1);
		if (er != erSuccess)
			goto exit;
		break;
	case PT_APPTIME:
		er = lpSerializer->Read(&lpProp->Value.at, sizeof(lpProp->Value.at), 1);
		if (er != erSuccess)
			goto exit;
		break;
	case PT_BOOLEAN:
		/* lpProp->Value.b is unsigned short while server is sending bool */
		er = lpSerializer->Read(&lpProp->Value.b, sizeof(bool), 1);
		if (er != erSuccess)
			goto exit;
		break;
	case PT_LONGLONG:
		er = lpSerializer->Read(&lpProp->Value.li.QuadPart, sizeof(lpProp->Value.li.QuadPart), 1);
		if (er != erSuccess)
			goto exit;
		break;
	case PT_STRING8:
	case PT_UNICODE:
		// Always output PT_UNICODE so we're not bound to the local charset
		lpProp->ulPropTag = CHANGE_PROP_TYPE(lpProp->ulPropTag, PT_UNICODE);
		
		er = lpSerializer->Read(&ulLen, sizeof(ulLen), 1);
		if (er == erSuccess) {
			utf8string strData(ulLen, '\0');	// Reserve memory we can safely overwrite.

			er = lpSerializer->Read((void*)strData.data(), 1, ulLen);
			if (er != erSuccess)
				goto exit;

			const std::wstring strResult = converter.convert_to<std::wstring>(strData, rawsize(strData), "UTF-8");

			hr = MAPIAllocateMore(sizeof(WCHAR) * (strResult.size() + 1), lpProp, (LPVOID*)&lpProp->Value.lpszW);
			if (hr != hrSuccess)
				goto exit;

			wcscpy(lpProp->Value.lpszW, strResult.c_str());
		} else
			goto exit;
		break;
	case PT_BINARY:
		er = lpSerializer->Read(&ulLen, sizeof(ulLen), 1);
		if (er != erSuccess)
			goto exit;

		hr = MAPIAllocateMore(ulLen + sizeof(unsigned char), lpProp, (LPVOID *)&lpProp->Value.bin.lpb);
		if (hr != hrSuccess)
			goto exit;

		memset(lpProp->Value.bin.lpb, 0, ulLen + sizeof(unsigned char));
		er = lpSerializer->Read(lpProp->Value.bin.lpb, sizeof(unsigned char), ulLen);
		if (er != erSuccess)
			goto exit;

		lpProp->Value.bin.cb = ulLen;

		break;
	case PT_SYSTIME:
		er = lpSerializer->Read(&lpProp->Value.ft, sizeof(lpProp->Value.ft.dwLowDateTime), 2);
		if (er != erSuccess)
			goto exit;
		break;
	case PT_ERROR:
		er = lpSerializer->Read(&lpProp->Value.err, sizeof(lpProp->Value.err), 1);
		if (er != erSuccess)
			goto exit;
		break;
	case PT_CLSID:
		er = lpSerializer->Read(&ulLen, sizeof(ulLen), 1);
		if (er != erSuccess)
			goto exit;

		hr = MAPIAllocateMore(ulLen + sizeof(unsigned char), lpProp, (LPVOID *)&lpProp->Value.lpguid);
		if (hr != hrSuccess)
			goto exit;

		memset(lpProp->Value.lpguid, 0, ulLen + sizeof(unsigned char));
		er = lpSerializer->Read(lpProp->Value.lpguid, sizeof(unsigned char), ulLen);
		if (er != erSuccess)
			goto exit;

		break;
	case PT_MV_I2:
		er = lpSerializer->Read(&lpProp->Value.MVi.cValues, sizeof(lpProp->Value.MVi.cValues), 1);
		if (er != erSuccess)
			goto exit;

		hr = MAPIAllocateMore(lpProp->Value.MVi.cValues * sizeof(*lpProp->Value.MVi.lpi), lpProp, (LPVOID *)&lpProp->Value.MVi.lpi);
		if (hr != hrSuccess)
			goto exit;

		er = lpSerializer->Read(lpProp->Value.MVi.lpi, sizeof(*lpProp->Value.MVi.lpi), lpProp->Value.MVi.cValues);
		if (er != erSuccess)
			goto exit;

		break;
	case PT_MV_I4:
		er = lpSerializer->Read(&lpProp->Value.MVl.cValues, sizeof(lpProp->Value.MVl.cValues), 1);
		if (er != erSuccess)
			goto exit;

		hr = MAPIAllocateMore(lpProp->Value.MVl.cValues * sizeof(*lpProp->Value.MVl.lpl), lpProp, (LPVOID *)&lpProp->Value.MVl.lpl);
		if (hr != hrSuccess)
			goto exit;

		er = lpSerializer->Read(lpProp->Value.MVl.lpl, sizeof(*lpProp->Value.MVl.lpl), lpProp->Value.MVl.cValues);
		if (er != erSuccess)
			goto exit;

		break;
	case PT_MV_I8:
		er = lpSerializer->Read(&lpProp->Value.MVli.cValues, sizeof(lpProp->Value.MVli.cValues), 1);
		if (er != erSuccess)
			goto exit;

		hr = MAPIAllocateMore(lpProp->Value.MVli.cValues * sizeof(*lpProp->Value.MVli.lpli), lpProp, (LPVOID *)&lpProp->Value.MVli.lpli);
		if (hr != hrSuccess)
			goto exit;

		er = lpSerializer->Read(lpProp->Value.MVli.lpli, sizeof(*lpProp->Value.MVli.lpli), lpProp->Value.MVli.cValues);
		if (er != erSuccess)
			goto exit;

		break;
	case PT_MV_R4:
		er = lpSerializer->Read(&lpProp->Value.MVflt.cValues, sizeof(lpProp->Value.MVflt.cValues), 1);
		if (er != erSuccess)
			goto exit;

		hr = MAPIAllocateMore(lpProp->Value.MVflt.cValues * sizeof(*lpProp->Value.MVflt.lpflt), lpProp, (LPVOID *)&lpProp->Value.MVflt.lpflt);
		if (hr != hrSuccess)
			goto exit;

		er = lpSerializer->Read(lpProp->Value.MVflt.lpflt, sizeof(*lpProp->Value.MVflt.lpflt), lpProp->Value.MVflt.cValues);
		if (er != erSuccess)
			goto exit;

		break;
	case PT_MV_DOUBLE:
		er = lpSerializer->Read(&lpProp->Value.MVdbl.cValues, sizeof(lpProp->Value.MVdbl.cValues), 1);
		if (er != erSuccess)
			goto exit;

		hr = MAPIAllocateMore(lpProp->Value.MVdbl.cValues * sizeof(*lpProp->Value.MVdbl.lpdbl), lpProp, (LPVOID *)&lpProp->Value.MVdbl.lpdbl);
		if (hr != hrSuccess)
			goto exit;

		er = lpSerializer->Read(lpProp->Value.MVdbl.lpdbl, sizeof(*lpProp->Value.MVdbl.lpdbl), lpProp->Value.MVdbl.cValues);
		if (er != erSuccess)
			goto exit;

		break;
	case PT_MV_APPTIME:
		er = lpSerializer->Read(&lpProp->Value.MVat.cValues, sizeof(lpProp->Value.MVat.cValues), 1);
		if (er != erSuccess)
			goto exit;

		hr = MAPIAllocateMore(lpProp->Value.MVat.cValues * sizeof(*lpProp->Value.MVat.lpat), lpProp, (LPVOID *)&lpProp->Value.MVat.lpat);
		if (hr != hrSuccess)
			goto exit;

		er = lpSerializer->Read(lpProp->Value.MVat.lpat, sizeof(*lpProp->Value.MVat.lpat), lpProp->Value.MVat.cValues);
		if (er != erSuccess)
			goto exit;

		break;
	case PT_MV_CURRENCY:
		er = lpSerializer->Read(&lpProp->Value.MVcur.cValues, sizeof(lpProp->Value.MVcur.cValues), 1);
		if (er != erSuccess)
			goto exit;

		hr = MAPIAllocateMore(lpProp->Value.MVcur.cValues * sizeof(*lpProp->Value.MVcur.lpcur), lpProp, (LPVOID *)&lpProp->Value.MVcur.lpcur);
		if (hr != hrSuccess)
			goto exit;

		er = lpSerializer->Read(lpProp->Value.MVcur.lpcur, sizeof(*lpProp->Value.MVcur.lpcur), lpProp->Value.MVcur.cValues);
		if (er != erSuccess)
			goto exit;

		break;
	case PT_MV_SYSTIME:
		er = lpSerializer->Read(&lpProp->Value.MVft.cValues, sizeof(lpProp->Value.MVft.cValues), 1);
		if (er != erSuccess)
			goto exit;

		hr = MAPIAllocateMore(lpProp->Value.MVft.cValues * sizeof(*lpProp->Value.MVft.lpft), lpProp, (LPVOID *)&lpProp->Value.MVft.lpft);
		if (hr != hrSuccess)
			goto exit;

		er = lpSerializer->Read(lpProp->Value.MVft.lpft, sizeof(lpProp->Value.MVft.lpft->dwLowDateTime), lpProp->Value.MVft.cValues * 2);
		if (er != erSuccess)
			goto exit;

		break;
	case PT_MV_BINARY:
		er = lpSerializer->Read(&lpProp->Value.MVbin.cValues, sizeof(lpProp->Value.MVbin.cValues), 1);
		if (er != erSuccess)
			goto exit;

		hr = MAPIAllocateMore(lpProp->Value.MVbin.cValues * sizeof(*lpProp->Value.MVbin.lpbin), lpProp, (LPVOID *)&lpProp->Value.MVbin.lpbin);
		if (hr != hrSuccess)
			goto exit;

		for (unsigned int i = 0; i < lpProp->Value.MVbin.cValues; i++) {
			er = lpSerializer->Read(&lpProp->Value.MVbin.lpbin[i].cb, sizeof(lpProp->Value.MVbin.lpbin[i].cb), 1);
			if (er != erSuccess)
				goto exit;

			hr = MAPIAllocateMore(lpProp->Value.MVbin.lpbin[i].cb + sizeof(unsigned char), lpProp, (LPVOID *)&lpProp->Value.MVbin.lpbin[i].lpb);
			if (hr != hrSuccess)
				goto exit;

			memset(lpProp->Value.MVbin.lpbin[i].lpb, 0, lpProp->Value.MVbin.lpbin[i].cb + sizeof(unsigned char));
			er = lpSerializer->Read(lpProp->Value.MVbin.lpbin[i].lpb, sizeof(unsigned char), lpProp->Value.MVbin.lpbin[i].cb);
			if (er != erSuccess)
				goto exit;
		}

		break;
	case PT_MV_CLSID:
		er = lpSerializer->Read(&lpProp->Value.MVguid.cValues, sizeof(lpProp->Value.MVguid.cValues), 1);
		if (er != erSuccess)
			goto exit;

		hr = MAPIAllocateMore(lpProp->Value.MVguid.cValues * sizeof(*lpProp->Value.MVguid.lpguid), lpProp, (LPVOID *)&lpProp->Value.MVguid.lpguid);
		if (hr != hrSuccess)
			goto exit;

		for (unsigned int i = 0; i < lpProp->Value.MVguid.cValues; i++) {
			er = lpSerializer->Read(&ulLen, sizeof(ulLen), 1);
			if (er != erSuccess)
				goto exit;

			hr = MAPIAllocateMore(ulLen + sizeof(unsigned char), lpProp, (LPVOID *)&lpProp->Value.MVguid.lpguid[i]);
			if (hr != hrSuccess)
				goto exit;

			er = lpSerializer->Read(&lpProp->Value.MVguid.lpguid[i], sizeof(unsigned char), ulLen);
			if (er != erSuccess)
				goto exit;
		}

		break;
	case PT_MV_STRING8:
	case PT_MV_UNICODE:
		// Always output PT_UNICODE so we're not bound to the local charset
		lpProp->ulPropTag = CHANGE_PROP_TYPE(lpProp->ulPropTag, PT_MV_UNICODE);

		er = lpSerializer->Read(&lpProp->Value.MVszA.cValues, sizeof(lpProp->Value.MVszA.cValues), 1);
		if (er != erSuccess)
			goto exit;

		hr = MAPIAllocateMore(lpProp->Value.MVszA.cValues * sizeof(*lpProp->Value.MVszA.lppszA), lpProp, (LPVOID *)&lpProp->Value.MVszA.lppszA);
		if (hr != hrSuccess)
			goto exit;

		for (unsigned int i = 0; i < lpProp->Value.MVszA.cValues; i++) {

			er = lpSerializer->Read(&ulLen, sizeof(ulLen), 1);
			if (er == erSuccess) {					
				utf8string strData(ulLen, '\0');	// Reserve memory we can safely overwrite.

				er = lpSerializer->Read((void*)strData.data(), 1, ulLen);
				if (er != erSuccess)
					goto exit;

				const std::wstring strResult = converter.convert_to<std::wstring>(strData, rawsize(strData), "UTF-8");

				hr = MAPIAllocateMore(sizeof(WCHAR) * (strResult.size() + 1), lpProp, (LPVOID*)&lpProp->Value.MVszW.lppszW[i]);
				if (hr != hrSuccess)
					goto exit;

				wcscpy(lpProp->Value.MVszW.lppszW[i], strResult.c_str());
			} else
				goto exit;
		}
		break;
	case PT_UNSPECIFIED:
	case PT_NULL:
	case PT_OBJECT:
	default:
		ASSERT(FALSE);
		hr = MAPI_E_NO_SUPPORT;
		goto exit;
	}

	if (PROP_ID(lpProp->ulPropTag) >= 0x8500) {
		// only named properties above 0x8500 send the named data, since 0x8000 - 0x8500 is static
		hr = MAPIAllocateBuffer(sizeof(MAPINAMEID), (void **)&lpNameID);
		if (hr != hrSuccess)
			goto exit;
			
		hr = MAPIAllocateMore(sizeof(GUID), lpNameID, (void **)&lpNameID->lpguid);
		if (hr != hrSuccess)
			goto exit;
		
		er = lpSerializer->Read(lpNameID->lpguid, 1, sizeof(GUID));
		if (er != erSuccess)
			goto exit;

		// get kind of named id
		er = lpSerializer->Read(&lpNameID->ulKind, sizeof(ULONG), 1);
		if (er != erSuccess)
			goto exit;

		if (lpNameID->ulKind != MNID_ID && lpNameID->ulKind != MNID_STRING) {
			hr = MAPI_E_CORRUPT_DATA;
			goto exit;
		}

		if (lpNameID->ulKind == MNID_STRING) {
			char *name;
			
			er = lpSerializer->Read(&ulLen, sizeof(ULONG), 1);
			if (er != erSuccess)
				goto exit;
				
			hr = MAPIAllocateMore(ulLen + 1, lpNameID, (void **)&name);
			if (hr != hrSuccess)
				goto exit;

			er = lpSerializer->Read(name, 1, ulLen);
			if (er != erSuccess)
				goto exit;
			name[ulLen] = 0;
			
			std::wstring strName = convert_to<std::wstring>(name, rawsize(name), "UTF-8");
			
			hr = MAPIAllocateMore((strName.size() + 1) * sizeof(wchar_t), lpNameID, (void **)&lpNameID->Kind.lpwstrName);
			if (hr != hrSuccess)
				goto exit;
				
			wcscpy(lpNameID->Kind.lpwstrName, strName.c_str());
			
		} else {
			er = lpSerializer->Read(&lpNameID->Kind.lID, sizeof(LONG), 1);
		}
		if (er != erSuccess)
			goto exit;
	}
	
	if (lppNameID) {
		*lppNameID = lpNameID;
		lpNameID = NULL;
	}

	if (lppProp) {
		*lppProp = lpProp;
		lpProp = NULL;
	}

exit:
	if (er != erSuccess)
		hr = ZarafaErrorToMAPIError(er);

	if (lpNameID) 
		MAPIFreeBuffer(lpNameID);

	if (lpProp)
		MAPIFreeBuffer(lpProp);

	return hr;
}

std::wstring PropToString(LPSPropValue lpProp)
{
	switch (PROP_TYPE(lpProp->ulPropTag)) {
	case PT_SHORT:
		return wstringify(lpProp->Value.i);
	case PT_LONG:
		return wstringify(lpProp->Value.l);
	case PT_FLOAT:
		return wstringify_float(lpProp->Value.flt);
	case PT_DOUBLE:
		return wstringify_double(lpProp->Value.dbl);
	case PT_CURRENCY:
		return wstringify_uint64(lpProp->Value.cur.int64);
	case PT_APPTIME:
		return wstringify_double(lpProp->Value.at);
	case PT_BOOLEAN:
		return wstringify(lpProp->Value.b);
	case PT_LONGLONG:
		return wstringify(lpProp->Value.li.QuadPart);
	case PT_STRING8: {
		std::wstring str;
		convert_context context;
		TryConvert(context, lpProp->Value.lpszA, strlen(lpProp->Value.lpszA), CHARSET_CHAR, str);
		return str;
	}
	case PT_BINARY:
		return bin2hexw(lpProp->Value.bin.cb, lpProp->Value.bin.lpb);
	case PT_SYSTIME:
		return wstringify_uint64((((unsigned long long)lpProp->Value.ft.dwHighDateTime) << 32) | lpProp->Value.ft.dwLowDateTime);
	case PT_UNICODE:
		return lpProp->Value.lpszW;
	case PT_MV_STRING8: {
		std::wstring str;
		std::wstring conv;
		convert_context context;
		
		for(unsigned int i = 0; i < lpProp->Value.MVszA.cValues; i++) {
			if (lpProp->Value.MVszA.lppszA[i]) {
				TryConvert(context, lpProp->Value.MVszA.lppszA[i], strlen(lpProp->Value.MVszA.lppszA[i]), CHARSET_CHAR, conv);
				str += conv;
				str += L" ";
			}
		}
		return str;
	}
	case PT_MV_UNICODE: {
		std::wstring str;
		
		for(unsigned int i = 0; i < lpProp->Value.MVszW.cValues; i++) {
			if (lpProp->Value.MVszW.lppszW[i]) {
				str += lpProp->Value.MVszW.lppszW[i];
				str += L" ";
			}
		}
		return str;
	}
		
	case PT_UNSPECIFIED:
	case PT_NULL:
	case PT_ERROR:
	case PT_OBJECT:
	case PT_CLSID:
	default:
		return std::wstring();
	}

	return std::wstring();
}

HRESULT CreateSyncStream(IStream **lppStream, ULONG ulInitData, LPBYTE lpInitData)
{
	HRESULT hr = hrSuccess;
	IStream *lpStream = NULL;
	LARGE_INTEGER lint = {{ 0, 0 }};
	ULONG ulSize = 0;

	hr = CreateStreamOnHGlobal(GlobalAlloc(GPTR, ulInitData), true, &lpStream);
	if (hr != hrSuccess)
		goto exit;

	hr = lpStream->Seek(lint, STREAM_SEEK_SET, NULL);
	if (hr != hrSuccess)
		goto exit;

	hr = lpStream->Write(lpInitData, ulInitData, &ulSize);
	if (hr != hrSuccess)
		goto exit;

	if (lppStream)
		*lppStream = lpStream;

exit:
	if ((hr != hrSuccess) && lpStream)
		lpStream->Release();

	return hr;

}


HRESULT OpenProperty(IMessage *lpMessage, ULONG ulPropTag, LPVOID lpBase, LPSPropValue lpProp)
{
	HRESULT hr = hrSuccess;
	StreamPtr ptrStream;
	STATSTG statstg;
	ULONG cbRead = 0;
	LPVOID lpTmp = NULL;

	if (lpMessage == NULL || lpBase == NULL || lpProp == NULL || PROP_TYPE(ulPropTag) != PT_TSTRING) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	hr = lpMessage->OpenProperty(ulPropTag, &ptrStream.iid, 0, 0, &ptrStream);
	if (hr != hrSuccess)
		goto exit;

	hr = ptrStream->Stat(&statstg, STATFLAG_NONAME);
	if (hr != hrSuccess)
		goto exit;

	hr = MAPIAllocateMore(statstg.cbSize.LowPart, lpBase, &lpTmp);	// No need to free on error
	if (hr != hrSuccess)
		goto exit;

	hr = ptrStream->Read(lpTmp, statstg.cbSize.LowPart, &cbRead);
	if (hr != hrSuccess)
		goto exit;

	lpProp->ulPropTag = ulPropTag;
	lpProp->Value.LPSZ = (LPTSTR)lpTmp;

exit:
	return hr;
}
