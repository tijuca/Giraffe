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
#include <new>
#include "ECPropertyEntry.h"
#include "Mem.h"
#include <kopano/charset/convert.h>

//
// ECPropertyEntry
//
DEF_INVARIANT_CHECK(ECPropertyEntry) {
	// There should always be a proptag set.
	assert(ulPropTag != 0);
	assert(PROP_ID(ulPropTag) != 0);

	// PT_STRING8 and PT_MV_STRING8 are never stored.
	assert(PROP_TYPE(ulPropTag) != PT_STRING8);
	assert(PROP_TYPE(ulPropTag) != PT_MV_STRING8);
}

ECPropertyEntry::ECPropertyEntry(ULONG tag) :
	ulPropTag(tag)
{
	DEBUG_CHECK_INVARIANT;
}

ECPropertyEntry::ECPropertyEntry(std::unique_ptr<ECProperty> &&p) :
	ulPropTag(p->GetPropTag()), lpProperty(std::move(p))
{
	DEBUG_CHECK_INVARIANT;
}

ECPropertyEntry::~ECPropertyEntry()
{
	DEBUG_CHECK_INVARIANT;
}

// NOTE: lpsPropValue must be checked already
HRESULT ECPropertyEntry::HrSetProp(const SPropValue *lpsPropValue)
{
	DEBUG_GUARD;

	HRESULT hr = hrSuccess;
	assert(this->ulPropTag != 0);
	assert(this->ulPropTag == lpsPropValue->ulPropTag);

	if(this->lpProperty)
		this->lpProperty->CopyFrom(lpsPropValue);
	else
		lpProperty.reset(new ECProperty(lpsPropValue));
	this->fDirty = TRUE;

	return hr;
}

HRESULT ECPropertyEntry::HrSetProp(ECProperty *property)
{
	DEBUG_GUARD;

	HRESULT hr = hrSuccess;

	assert(property->GetPropTag() != 0);
	assert(this->lpProperty == NULL);
	lpProperty.reset(property);
	this->fDirty = TRUE;

	return hr;
}

HRESULT ECPropertyEntry::HrSetClean()
{
	DEBUG_GUARD;

	this->fDirty = FALSE;

	return hrSuccess;
}

// ECProperty
//
// C++ class representing a property
//
//
// 
//
DEF_INVARIANT_CHECK(ECProperty) {
	// There should always be a proptag set.
	assert(ulPropTag != 0);
	assert(PROP_ID(ulPropTag) != 0);

	// PT_STRING8 and PT_MV_STRING8 are never stored.
	assert(PROP_TYPE(ulPropTag) != PT_STRING8);
	assert(PROP_TYPE(ulPropTag) != PT_MV_STRING8);
}

ECProperty::ECProperty(const ECProperty &Property) {
	SPropValue sPropValue;

	assert(Property.ulPropTag != 0);
	sPropValue.ulPropTag = Property.ulPropTag;
	sPropValue.Value = Property.Value;
	memset(&this->Value, 0, sizeof(union __UPV));
	this->ulSize = 0;

	CopyFromInternal(&sPropValue);

	DEBUG_CHECK_INVARIANT;
}
	
ECProperty::ECProperty(const SPropValue *lpsProp)
{
	memset(&this->Value, 0, sizeof(union __UPV));
	this->ulSize = 0;
	assert(lpsProp->ulPropTag != 0);
	CopyFromInternal(lpsProp);

	DEBUG_CHECK_INVARIANT;
}

HRESULT ECProperty::CopyFrom(const SPropValue *lpsProp)
{
	DEBUG_GUARD;
	return CopyFromInternal(lpsProp);
}

HRESULT ECProperty::CopyFromInternal(const SPropValue *lpsProp)
{
	ULONG ulNewSize = 0;

	if (lpsProp == NULL)
		return dwLastError = MAPI_E_INVALID_PARAMETER;

	this->dwLastError = 0;
	this->ulPropTag = lpsProp->ulPropTag;
	assert(lpsProp->ulPropTag != 0);

	switch(PROP_TYPE(lpsProp->ulPropTag)) {
	case PT_I2:
		this->ulSize = 4;
		this->Value.i = lpsProp->Value.i;
		break;
	case PT_I4:
		this->ulSize = 4;
		this->Value.l = lpsProp->Value.l;
		break;
	case PT_R4:
		this->ulSize = 4;
		this->Value.flt = lpsProp->Value.flt;
		break;
	case PT_R8:
		this->ulSize = 4;
		this->Value.dbl = lpsProp->Value.dbl;
		break;
	case PT_BOOLEAN:
		this->ulSize = 4;
		this->Value.b = lpsProp->Value.b;
		break;
	case PT_CURRENCY:
		this->ulSize = 4;
		this->Value.cur = lpsProp->Value.cur;
		break;
	case PT_APPTIME:
		this->ulSize = 4;
		this->Value.at = lpsProp->Value.at;
		break;
	case PT_SYSTIME:
		this->ulSize = 4;
		this->Value.ft = lpsProp->Value.ft;
		break;
	case PT_STRING8: {
		std::wstring wstrTmp;

		if (lpsProp->Value.lpszA == NULL)
			return dwLastError = MAPI_E_INVALID_PARAMETER;

		if (TryConvert(lpsProp->Value.lpszA, wstrTmp) != hrSuccess)
			return dwLastError = MAPI_E_INVALID_PARAMETER;

		ulNewSize = wstrTmp.length() + 1;
		if(ulSize < ulNewSize) {
			delete[] this->Value.lpszW;
			this->Value.lpszW = new(std::nothrow) WCHAR[ulNewSize];
			if (this->Value.lpszW == NULL)
				return dwLastError = MAPI_E_NOT_ENOUGH_MEMORY;
		}

		ulSize = ulNewSize;
		
		this->ulPropTag = CHANGE_PROP_TYPE(lpsProp->ulPropTag, PT_UNICODE);
		wcscpy(this->Value.lpszW, wstrTmp.c_str());

		break;
	}
	case PT_BINARY: {
		if (lpsProp->Value.bin.lpb == NULL && lpsProp->Value.bin.cb)
			return dwLastError = MAPI_E_INVALID_PARAMETER;

		ulNewSize = lpsProp->Value.bin.cb;

		if(ulNewSize == 0)	{
			delete[] this->Value.bin.lpb;
			this->Value.bin.lpb = NULL;
			this->Value.bin.cb = 0;
			ulSize = 0;
			break;
		}

		if(ulSize < ulNewSize) {
			delete[] this->Value.bin.lpb;
			this->Value.bin.lpb = new(std::nothrow) BYTE[ulNewSize];
			if (this->Value.bin.lpb == NULL)
				return dwLastError = MAPI_E_NOT_ENOUGH_MEMORY;
		}
		ulSize = ulNewSize;

		this->Value.bin.cb = lpsProp->Value.bin.cb;
		memcpy(this->Value.bin.lpb, lpsProp->Value.bin.lpb, lpsProp->Value.bin.cb);
		
		break;
	}
	case PT_UNICODE: {
		if (lpsProp->Value.lpszW == NULL)
			return dwLastError = MAPI_E_INVALID_PARAMETER;

		ulNewSize = wcslen(lpsProp->Value.lpszW)+1;
		if(ulSize < ulNewSize) {
			delete[] this->Value.lpszW;
			this->Value.lpszW = new(std::nothrow) WCHAR[ulNewSize];
			if (this->Value.lpszW == NULL)
				return dwLastError = MAPI_E_NOT_ENOUGH_MEMORY;
		}

		ulSize = ulNewSize;
		
		wcscpy(this->Value.lpszW, lpsProp->Value.lpszW);

		break;
	}
	case PT_CLSID: {
		if (lpsProp->Value.lpguid == NULL)
			return dwLastError = MAPI_E_INVALID_PARAMETER;

		if(ulSize != sizeof(GUID)) {
			ulSize = sizeof(GUID);
			this->Value.lpguid = new(std::nothrow) GUID;
			if (this->Value.lpguid == NULL)
				return dwLastError = MAPI_E_NOT_ENOUGH_MEMORY;
		}

		memcpy(this->Value.lpguid, lpsProp->Value.lpguid, sizeof(GUID));

		break;
	}
	case PT_I8:
		ulSize = 8;
		this->Value.li = lpsProp->Value.li;
		break;
	case PT_MV_I2: {
		if (lpsProp->Value.MVi.lpi == NULL)
			return dwLastError = MAPI_E_INVALID_PARAMETER;

		ulNewSize = sizeof(short int)*lpsProp->Value.MVi.cValues;

		if(ulSize < ulNewSize) {
			delete[] this->Value.MVi.lpi;
			this->Value.MVi.lpi = new(std::nothrow) short int[lpsProp->Value.MVi.cValues];
			if (this->Value.MVi.lpi == NULL)
				return dwLastError = MAPI_E_NOT_ENOUGH_MEMORY;
		}

		ulSize = ulNewSize;

		this->Value.MVi.cValues = lpsProp->Value.MVi.cValues;
		memcpy(this->Value.MVi.lpi, lpsProp->Value.MVi.lpi, lpsProp->Value.MVi.cValues * sizeof(short int));

		break;
	}
	case PT_MV_LONG: {
		if (lpsProp->Value.MVl.lpl == NULL)
			return dwLastError = MAPI_E_INVALID_PARAMETER;

		ulNewSize = sizeof(LONG) * lpsProp->Value.MVl.cValues;

		if(ulSize < ulNewSize) {
			delete[] this->Value.MVl.lpl;
			this->Value.MVl.lpl = new(std::nothrow) LONG[lpsProp->Value.MVl.cValues];
			if (this->Value.MVl.lpl == NULL)
				return dwLastError = MAPI_E_NOT_ENOUGH_MEMORY;
		}
		
		ulSize = ulNewSize;

		this->Value.MVl.cValues = lpsProp->Value.MVl.cValues;
		memcpy(this->Value.MVl.lpl, lpsProp->Value.MVl.lpl, lpsProp->Value.MVl.cValues * sizeof(int));
		
		break;
	}
	case PT_MV_R4: {
		if (lpsProp->Value.MVflt.lpflt == NULL)
			return dwLastError = MAPI_E_INVALID_PARAMETER;

		ulNewSize = sizeof(float) * lpsProp->Value.MVflt.cValues;

		if(ulSize < ulNewSize) {
			delete[] this->Value.MVflt.lpflt;
			this->Value.MVflt.lpflt = new(std::nothrow) float[lpsProp->Value.MVflt.cValues];
			if (this->Value.MVflt.lpflt == NULL)
				return dwLastError = MAPI_E_NOT_ENOUGH_MEMORY;
		}
		
		ulSize = ulNewSize;

		this->Value.MVflt.cValues = lpsProp->Value.MVflt.cValues;
		memcpy(this->Value.MVflt.lpflt, lpsProp->Value.MVflt.lpflt, lpsProp->Value.MVflt.cValues * sizeof(float));
		
		break;
	}
	case PT_MV_DOUBLE: {
		if (lpsProp->Value.MVdbl.lpdbl == NULL)
			return dwLastError = MAPI_E_INVALID_PARAMETER;

		ulNewSize = sizeof(double) * lpsProp->Value.MVdbl.cValues;

		if(ulSize < ulNewSize) {
			delete[] this->Value.MVdbl.lpdbl;
			this->Value.MVdbl.lpdbl = new(std::nothrow) double[lpsProp->Value.MVdbl.cValues];
			if (this->Value.MVdbl.lpdbl == NULL)
				return dwLastError = MAPI_E_NOT_ENOUGH_MEMORY;
		}

		ulSize = ulNewSize;

		this->Value.MVdbl.cValues = lpsProp->Value.MVdbl.cValues;
		memcpy(this->Value.MVdbl.lpdbl, lpsProp->Value.MVdbl.lpdbl, lpsProp->Value.MVdbl.cValues * sizeof(double));

		break;
	}
	case PT_MV_CURRENCY: {
		if (lpsProp->Value.MVcur.lpcur == NULL)
			return dwLastError = MAPI_E_INVALID_PARAMETER;

		ulNewSize = sizeof(CURRENCY) * lpsProp->Value.MVcur.cValues;

		if(ulSize < ulNewSize) {
			delete[] this->Value.MVcur.lpcur;
			this->Value.MVcur.lpcur = new(std::nothrow) CURRENCY[lpsProp->Value.MVcur.cValues];
			if (this->Value.MVcur.lpcur == NULL)
				return dwLastError = MAPI_E_NOT_ENOUGH_MEMORY;
		}
		
		ulSize = ulNewSize;

		this->Value.MVcur.cValues = lpsProp->Value.MVcur.cValues;
		memcpy(this->Value.MVcur.lpcur, lpsProp->Value.MVcur.lpcur, lpsProp->Value.MVcur.cValues * sizeof(CURRENCY));

		break;
	}
	case PT_MV_APPTIME: {
		if (lpsProp->Value.MVat.lpat == NULL)
			return dwLastError = MAPI_E_INVALID_PARAMETER;

		ulNewSize = sizeof(double) * lpsProp->Value.MVat.cValues;

		if(ulSize < ulNewSize) {
			if(this->Value.MVat.lpat)
				delete[] this->Value.MVat.lpat;
			this->Value.MVat.lpat = new(std::nothrow) double[lpsProp->Value.MVat.cValues];
			if (this->Value.MVat.lpat == NULL)
				return dwLastError = MAPI_E_NOT_ENOUGH_MEMORY;
		}

		ulSize = ulNewSize;

		this->Value.MVat.cValues = lpsProp->Value.MVat.cValues;
		memcpy(this->Value.MVat.lpat, lpsProp->Value.MVat.lpat, lpsProp->Value.MVat.cValues * sizeof(double));
		
		break;
	}
	case PT_MV_SYSTIME: {
		if (lpsProp->Value.MVft.lpft == NULL)
			return dwLastError = MAPI_E_INVALID_PARAMETER;

		ulNewSize = sizeof(FILETIME) * lpsProp->Value.MVft.cValues;

		if(ulSize < ulNewSize) {
			delete[] this->Value.MVft.lpft;
			this->Value.MVft.lpft = new(std::nothrow) FILETIME[lpsProp->Value.MVft.cValues];
			if (this->Value.MVft.lpft == NULL)
				return dwLastError = MAPI_E_NOT_ENOUGH_MEMORY;
		}
		
		ulSize = ulNewSize;
        		
		this->Value.MVft.cValues = lpsProp->Value.MVft.cValues;
		memcpy(this->Value.MVft.lpft, lpsProp->Value.MVft.lpft, lpsProp->Value.MVft.cValues * sizeof(FILETIME));
		
		break;
	}
	case PT_MV_BINARY: {
		if (lpsProp->Value.MVbin.lpbin == NULL)
			return dwLastError = MAPI_E_INVALID_PARAMETER;

		ulNewSize = sizeof(void *) * lpsProp->Value.MVbin.cValues;

		if(ulSize < ulNewSize) {
			
			if(this->Value.MVbin.lpbin){
				for (unsigned int i = 0; i < this->Value.MVbin.cValues; ++i)
					delete[] this->Value.MVbin.lpbin[i].lpb;
				delete[] this->Value.MVbin.lpbin;
			}
			this->Value.MVbin.lpbin = new(std::nothrow) SBinary[lpsProp->Value.MVbin.cValues];
			if (this->Value.MVbin.lpbin == NULL)
				return dwLastError = MAPI_E_NOT_ENOUGH_MEMORY;

			memset(this->Value.MVbin.lpbin, 0, sizeof(SBinary) * lpsProp->Value.MVbin.cValues);
		}
		else {
			for(unsigned int i = lpsProp->Value.MVbin.cValues; i < this->Value.MVbin.cValues; ++i)
				delete[] this->Value.MVbin.lpbin[i].lpb;
		}

		ulSize = ulNewSize;

		this->Value.MVbin.cValues = lpsProp->Value.MVbin.cValues;

		for (unsigned int i = 0; i < lpsProp->Value.MVbin.cValues; ++i) {
			if(lpsProp->Value.MVbin.lpbin[i].cb > 0)
			{
				if (lpsProp->Value.MVbin.lpbin[i].lpb == NULL)
					return dwLastError = MAPI_E_INVALID_PARAMETER;
				if(this->Value.MVbin.lpbin[i].lpb == NULL || this->Value.MVbin.lpbin[i].cb < lpsProp->Value.MVbin.lpbin[i].cb) {
					delete[] this->Value.MVbin.lpbin[i].lpb;
					this->Value.MVbin.lpbin[i].lpb = new BYTE [lpsProp->Value.MVbin.lpbin[i].cb];
				}
				
				memcpy(this->Value.MVbin.lpbin[i].lpb, lpsProp->Value.MVbin.lpbin[i].lpb, lpsProp->Value.MVbin.lpbin[i].cb);
			}else {
				delete[] this->Value.MVbin.lpbin[i].lpb;
				this->Value.MVbin.lpbin[i].lpb = NULL;
			}
			this->Value.MVbin.lpbin[i].cb = lpsProp->Value.MVbin.lpbin[i].cb;
		}

		break;
	}
	case PT_MV_STRING8: {
		convert_context converter;

		if (lpsProp->Value.MVszA.lppszA == NULL)
			return dwLastError = MAPI_E_INVALID_PARAMETER;

		ulNewSize = sizeof(void *) * lpsProp->Value.MVszA.cValues;

		if(ulSize < ulNewSize) {
			
			if(this->Value.MVszW.lppszW) {
				for (unsigned int i = 0; i < this->Value.MVszW.cValues; ++i)
					delete [] this->Value.MVszW.lppszW[i];
				delete [] this->Value.MVszW.lppszW;
			}
			this->Value.MVszW.lppszW = new(std::nothrow) wchar_t *[lpsProp->Value.MVszA.cValues];
			if (this->Value.MVszW.lppszW == NULL)
				return dwLastError = MAPI_E_NOT_ENOUGH_MEMORY;

			memset(this->Value.MVszW.lppszW, 0, sizeof(wchar_t *) * lpsProp->Value.MVszA.cValues);
		}
		else {
			for(unsigned int i = lpsProp->Value.MVszW.cValues; i < this->Value.MVszW.cValues; ++i)
				delete[] this->Value.MVszW.lppszW[i];
		}

		ulSize = ulNewSize;

		this->Value.MVszW.cValues = lpsProp->Value.MVszA.cValues;

		for (unsigned int i = 0; i < lpsProp->Value.MVszA.cValues; ++i) {
			std::wstring wstrTmp;

			if (lpsProp->Value.MVszA.lppszA[i] == NULL)
				return dwLastError = MAPI_E_INVALID_PARAMETER;
			if (TryConvert(lpsProp->Value.MVszA.lppszA[i], wstrTmp) != hrSuccess)
				return dwLastError = MAPI_E_INVALID_PARAMETER;
			if(this->Value.MVszW.lppszW[i] == NULL || wcslen(this->Value.MVszW.lppszW[i]) < wstrTmp.length())
			{
				delete[] this->Value.MVszW.lppszW[i];
				this->Value.MVszW.lppszW[i] = new WCHAR [wstrTmp.length() + sizeof(wchar_t)];
			}
			wcscpy(this->Value.MVszW.lppszW[i], wstrTmp.c_str());
		}

		this->ulPropTag = CHANGE_PROP_TYPE(lpsProp->ulPropTag, PT_MV_UNICODE);
		break;
	}
	case PT_MV_UNICODE: {
		if (lpsProp->Value.MVszW.lppszW == NULL)
			return dwLastError = MAPI_E_INVALID_PARAMETER;

		ulNewSize = sizeof(void *) * lpsProp->Value.MVszW.cValues;

		if(ulSize < ulNewSize) {
			if(this->Value.MVszW.lppszW) {
				for (unsigned int i = 0; i < this->Value.MVszW.cValues; ++i)
					delete[] this->Value.MVszW.lppszW[i];
				delete[] this->Value.MVszW.lppszW;
			}
			this->Value.MVszW.lppszW = new(std::nothrow) wchar_t *[lpsProp->Value.MVszW.cValues];
			if (this->Value.MVszW.lppszW == NULL)
				return dwLastError = MAPI_E_NOT_ENOUGH_MEMORY;

			memset(this->Value.MVszW.lppszW, 0, sizeof(WCHAR *) * lpsProp->Value.MVszW.cValues);
		}
		else {
			for(unsigned int i = lpsProp->Value.MVszW.cValues; i < this->Value.MVszW.cValues; ++i)
				delete[] this->Value.MVszW.lppszW[i];
		}
		
		ulSize = ulNewSize;

		this->Value.MVszW.cValues = lpsProp->Value.MVszW.cValues;

		for (unsigned int i = 0; i < lpsProp->Value.MVszW.cValues; ++i) {
			if (lpsProp->Value.MVszW.lppszW[i] == NULL)
				return dwLastError = MAPI_E_INVALID_PARAMETER;
			if(this->Value.MVszW.lppszW[i] == NULL || wcslen(this->Value.MVszW.lppszW[i]) < wcslen(lpsProp->Value.MVszW.lppszW[i])) {
				delete[] this->Value.MVszW.lppszW[i];
				this->Value.MVszW.lppszW[i] = new WCHAR [wcslen(lpsProp->Value.MVszW.lppszW[i])+sizeof(WCHAR)];
			}
			wcscpy(this->Value.MVszW.lppszW[i], lpsProp->Value.MVszW.lppszW[i]);
		}

		break;
	}
	case PT_MV_CLSID: {
		if (lpsProp->Value.MVguid.lpguid == NULL)
			return dwLastError = MAPI_E_INVALID_PARAMETER;

		ulNewSize = sizeof(GUID) * lpsProp->Value.MVguid.cValues;

		if(ulSize < ulNewSize) {
			delete[] this->Value.MVguid.lpguid;
			this->Value.MVguid.lpguid = new(std::nothrow) GUID[lpsProp->Value.MVguid.cValues];
			if (this->Value.MVguid.lpguid == NULL)
				return dwLastError = MAPI_E_NOT_ENOUGH_MEMORY;
		}

		ulSize = ulNewSize;

        this->Value.MVguid.cValues = lpsProp->Value.MVguid.cValues;
		memcpy(this->Value.MVguid.lpguid, lpsProp->Value.MVguid.lpguid, sizeof(GUID) * lpsProp->Value.MVguid.cValues);
			
		break;
	}
	case PT_MV_I8: {
		if (lpsProp->Value.MVli.lpli == NULL)
			return dwLastError = MAPI_E_INVALID_PARAMETER;

		ulNewSize = sizeof(LARGE_INTEGER) * lpsProp->Value.MVli.cValues;

		if(ulSize < ulNewSize) {
			delete[] this->Value.MVli.lpli;
			this->Value.MVli.lpli = new(std::nothrow) LARGE_INTEGER[lpsProp->Value.MVli.cValues];
			if (this->Value.MVli.lpli == NULL)
				return dwLastError = MAPI_E_NOT_ENOUGH_MEMORY;
		}

		ulSize = ulNewSize;

		this->Value.MVli.cValues = lpsProp->Value.MVli.cValues;
		memcpy(this->Value.MVli.lpli, lpsProp->Value.MVli.lpli, lpsProp->Value.MVli.cValues * sizeof(LARGE_INTEGER));

		break;
	}
	case PT_ERROR:
		this->Value.err = lpsProp->Value.err;
		break;
	default:
		// Unknown type (PR_NULL not include)
		assert(false);
		dwLastError = MAPI_E_INVALID_PARAMETER;
		break;
	}
	return dwLastError;
}

ECProperty::~ECProperty()
{
	DEBUG_GUARD;

	switch(PROP_TYPE(ulPropTag)) {
		case PT_I2:
		case PT_I4:
		case PT_R4:
		case PT_R8:
		case PT_BOOLEAN:
		case PT_CURRENCY:
		case PT_APPTIME:
		case PT_SYSTIME:
		case PT_I8:
			break;
		case PT_BINARY:
			delete[] this->Value.bin.lpb;
			break;
		case PT_STRING8:
			assert("We should never have PT_STRING8 storage" == nullptr);
			// Deliberate fallthrough
		case PT_UNICODE:
			delete [] this->Value.lpszW;
			break;
		case PT_CLSID:
			delete this->Value.lpguid;
			break;
		case PT_MV_I2:
			delete [] Value.MVi.lpi;
			break;
		case PT_MV_LONG:
			delete [] this->Value.MVl.lpl;
			break;
		case PT_MV_R4:
			delete [] this->Value.MVflt.lpflt;
			break;
		case PT_MV_DOUBLE:
			delete [] this->Value.MVdbl.lpdbl;
			break;
		case PT_MV_CURRENCY:
			delete [] this->Value.MVcur.lpcur;
			break;
		case PT_MV_APPTIME:
			delete [] this->Value.MVat.lpat;
			break;
		case PT_MV_SYSTIME:
			delete [] this->Value.MVft.lpft;
			break;
		case PT_MV_BINARY: {
			for (unsigned int i = 0; i <this->Value.MVbin.cValues; ++i)
				delete[] this->Value.MVbin.lpbin[i].lpb;
			delete [] this->Value.MVbin.lpbin;
			break;
		}
		case PT_MV_STRING8:
			assert("We should never have PT_MV_STRING8 storage" == nullptr);
			// Deliberate fallthrough
		case PT_MV_UNICODE: {
			for (unsigned int i = 0; i < this->Value.MVszW.cValues; ++i)
				delete [] this->Value.MVszW.lppszW[i];
			delete [] this->Value.MVszW.lppszW;
			break;
		}
		case PT_MV_CLSID: {
			delete [] this->Value.MVguid.lpguid;
			break;
		}
		case PT_MV_I8:
			delete [] this->Value.MVli.lpli;
			break;
		case PT_ERROR:
			break;
		default:
			break;
	}
}

HRESULT ECProperty::CopyToByRef(LPSPropValue lpsProp) const
{
	DEBUG_GUARD;

    HRESULT hr = hrSuccess;
    
    lpsProp->ulPropTag = this->ulPropTag;
	memcpy(&lpsProp->Value, &this->Value, sizeof(union __UPV));
    return hr;
}

HRESULT ECProperty::CopyTo(LPSPropValue lpsProp, void *lpBase, ULONG ulRequestPropTag) {
	DEBUG_GUARD;

	HRESULT hr = hrSuccess;

	assert
	(
		PROP_TYPE(ulRequestPropTag) == PROP_TYPE(this->ulPropTag) ||
		((PROP_TYPE(ulRequestPropTag) == PT_STRING8 || PROP_TYPE(ulRequestPropTag) == PT_UNICODE) && PROP_TYPE(this->ulPropTag) == PT_UNICODE) ||
		((PROP_TYPE(ulRequestPropTag) == PT_MV_STRING8 || PROP_TYPE(ulRequestPropTag) == PT_MV_UNICODE) && PROP_TYPE(this->ulPropTag) == PT_MV_UNICODE)
	);

	lpsProp->ulPropTag = ulRequestPropTag;

	switch(PROP_TYPE(this->ulPropTag)) {
	case PT_I2:
		lpsProp->Value.i = this->Value.i;
		break;
	case PT_I4:
		lpsProp->Value.l = this->Value.l;
		break;
	case PT_R4:
		lpsProp->Value.flt = this->Value.flt;
		break;
	case PT_R8:
		lpsProp->Value.dbl = this->Value.dbl;
		break;
	case PT_BOOLEAN:
		lpsProp->Value.b = this->Value.b;
		break;
	case PT_CURRENCY:
		lpsProp->Value.cur = this->Value.cur;
		break;
	case PT_APPTIME:
		lpsProp->Value.at = this->Value.at;
		break;
	case PT_SYSTIME:
		lpsProp->Value.ft = this->Value.ft;
		break;
	case PT_BINARY: {
		if (this->Value.bin.cb == 0) {
			lpsProp->Value.bin.lpb = NULL;
			lpsProp->Value.bin.cb = this->Value.bin.cb;
		} else {
			BYTE *lpBin = NULL;
			hr = ECAllocateMore(this->Value.bin.cb, lpBase, (LPVOID *)&lpBin);

			if(hr != hrSuccess)
				dwLastError = hr;
			else {
				memcpy(lpBin, this->Value.bin.lpb, this->Value.bin.cb);
				lpsProp->Value.bin.lpb = lpBin;
				lpsProp->Value.bin.cb = this->Value.bin.cb;
			}
		}
		break;
	}
	case PT_STRING8:
		assert("We should never have PT_STRING8 storage" == nullptr);
		// Deliberate fallthrough
	case PT_UNICODE: {
		if (PROP_TYPE(ulRequestPropTag) == PT_UNICODE) {
			hr = ECAllocateMore(sizeof(WCHAR) * (wcslen(this->Value.lpszW) + 1), lpBase, (LPVOID*)&lpsProp->Value.lpszW);
			if (hr != hrSuccess)
				dwLastError = hr;
			else
				wcscpy(lpsProp->Value.lpszW, this->Value.lpszW);
		}

		else {
			std::string dst;
			if (TryConvert(this->Value.lpszW, dst) != hrSuccess) {
				dwLastError = MAPI_E_INVALID_PARAMETER;
				return hr;
			}

			hr = ECAllocateMore(dst.length() + 1, lpBase, (LPVOID*)&lpsProp->Value.lpszA);
			if (hr != hrSuccess)
				dwLastError = hr;
			else
				strcpy(lpsProp->Value.lpszA, dst.c_str());
		}
		break;
	}
	case PT_CLSID: {
		GUID *lpGUID;
		hr = ECAllocateMore(sizeof(GUID), lpBase, (LPVOID *)&lpGUID);

		if(hr != hrSuccess)
			dwLastError = hr;
		else {
			memcpy(lpGUID, this->Value.lpguid, sizeof(GUID));
			lpsProp->Value.lpguid = lpGUID;
		}
		break;
	}
	case PT_I8:
		lpsProp->Value.li = this->Value.li;
		break;
	case PT_MV_I2: {
		short int *lpShort;
		hr = ECAllocateMore(this->Value.MVi.cValues * sizeof(short int), lpBase, (LPVOID *)&lpShort);

		if(hr != hrSuccess)
			dwLastError = hr;
		else {
			lpsProp->Value.MVi.cValues = this->Value.MVi.cValues;
			memcpy(lpShort, this->Value.MVi.lpi, this->Value.MVi.cValues * sizeof(short int));
			lpsProp->Value.MVi.lpi = lpShort;
		}
		break;
	}
	case PT_MV_LONG: {
		LONG *lpLong;
		hr = ECAllocateMore(this->Value.MVl.cValues * sizeof(LONG), lpBase, (LPVOID *)&lpLong);

		if(hr != hrSuccess)
			dwLastError = hr;
		else {
			lpsProp->Value.MVl.cValues = this->Value.MVl.cValues;
			memcpy(lpLong, this->Value.MVl.lpl, this->Value.MVl.cValues * sizeof(LONG));
			lpsProp->Value.MVl.lpl = lpLong;
		}
		break;
	}
	case PT_MV_R4: {
		float *lpFloat;
		hr = ECAllocateMore(this->Value.MVflt.cValues * sizeof(float), lpBase, (LPVOID *)&lpFloat);

		if(hr != hrSuccess)
			dwLastError = hr;
		else {
			lpsProp->Value.MVflt.cValues = this->Value.MVflt.cValues;
			memcpy(lpFloat, this->Value.MVflt.lpflt, this->Value.MVflt.cValues * sizeof(float));
			lpsProp->Value.MVflt.lpflt = lpFloat;
		}
		break;
	}
	case PT_MV_DOUBLE: {
		double *lpDouble;
		hr = ECAllocateMore(this->Value.MVdbl.cValues * sizeof(double), lpBase, (LPVOID *)&lpDouble);

		if(hr != hrSuccess)
			dwLastError = hr;
		else {
			lpsProp->Value.MVdbl.cValues = this->Value.MVdbl.cValues;
			memcpy(lpDouble, this->Value.MVdbl.lpdbl, this->Value.MVdbl.cValues * sizeof(double));
			lpsProp->Value.MVdbl.lpdbl = lpDouble;
		}
		break;
	}
	case PT_MV_CURRENCY: {
		CURRENCY *lpCurrency;
		hr = ECAllocateMore(this->Value.MVcur.cValues * sizeof(CURRENCY), lpBase, (LPVOID *)&lpCurrency);

		if(hr != hrSuccess)
			dwLastError = hr;
		else {
			lpsProp->Value.MVcur.cValues = this->Value.MVcur.cValues;
			memcpy(lpCurrency, this->Value.MVcur.lpcur, this->Value.MVcur.cValues * sizeof(CURRENCY));
			lpsProp->Value.MVcur.lpcur = lpCurrency;
		}
		break;
	}
	case PT_MV_APPTIME: {
		double *lpApptime;
		hr = ECAllocateMore(this->Value.MVat.cValues * sizeof(double), lpBase, (LPVOID *)&lpApptime);

		if(hr != hrSuccess)
			dwLastError = hr;
		else {
			lpsProp->Value.MVat.cValues = this->Value.MVat.cValues;
			memcpy(lpApptime, this->Value.MVat.lpat, this->Value.MVat.cValues * sizeof(double));
			lpsProp->Value.MVat.lpat = lpApptime;
		}
		break;
	}
	case PT_MV_SYSTIME: {
		FILETIME *lpFiletime;
		hr = ECAllocateMore(this->Value.MVft.cValues * sizeof(FILETIME), lpBase, (LPVOID *)&lpFiletime);

		if(hr != hrSuccess)
			dwLastError = hr;
		else {
			lpsProp->Value.MVft.cValues = this->Value.MVft.cValues;
			memcpy(lpFiletime, this->Value.MVft.lpft, this->Value.MVft.cValues * sizeof(FILETIME));
			lpsProp->Value.MVft.lpft = lpFiletime;
		}
		break;
	}
	case PT_MV_BINARY: {
		SBinary *lpBin;
		hr = ECAllocateMore(this->Value.MVbin.cValues * sizeof(SBinary), lpBase, (LPVOID *)&lpBin);

		if(hr != hrSuccess)
			dwLastError = hr;
		else {
			lpsProp->Value.MVbin.cValues = this->Value.MVbin.cValues;
			lpsProp->Value.MVbin.lpbin = lpBin;
			
			for (unsigned int i = 0; i < this->Value.MVbin.cValues; ++i) {
				lpsProp->Value.MVbin.lpbin[i].cb = this->Value.MVbin.lpbin[i].cb;
				if(lpsProp->Value.MVbin.lpbin[i].cb > 0)
				{
					hr = ECAllocateMore(this->Value.MVbin.lpbin[i].cb, lpBase, reinterpret_cast<void **>(&lpsProp->Value.MVbin.lpbin[i].lpb));
					if (hr != hrSuccess)
						return hr;
					memcpy(lpsProp->Value.MVbin.lpbin[i].lpb, this->Value.MVbin.lpbin[i].lpb, lpsProp->Value.MVbin.lpbin[i].cb);
				}else
					lpsProp->Value.MVbin.lpbin[i].lpb = NULL;
			}
		}
		break;
	}
	case PT_MV_STRING8:
		assert("We should never have PT_MV_STRING8 storage" == nullptr);
		// Deliberate fallthrough
	case PT_MV_UNICODE: {
		if (PROP_TYPE(ulRequestPropTag) == PT_MV_STRING8) {
			lpsProp->Value.MVszA.cValues = this->Value.MVszW.cValues;
			hr = ECAllocateMore(this->Value.MVszW.cValues * sizeof(LPSTR), lpBase, (LPVOID*)&lpsProp->Value.MVszA.lppszA);
			if (hr != hrSuccess)
				dwLastError = hr;

			else {
				convert_context converter;

				for (ULONG i = 0; hr == hrSuccess && i < this->Value.MVszW.cValues; ++i) {
					std::string strDst;
					if (TryConvert(this->Value.MVszW.lppszW[i], strDst) != hrSuccess) {
						dwLastError = MAPI_E_INVALID_PARAMETER;
						return hr;
					}

					hr = ECAllocateMore(strDst.size() + 1, lpBase, (LPVOID*)&lpsProp->Value.MVszA.lppszA[i]);
					if (hr != hrSuccess)
						dwLastError = hr;
					else
						strcpy(lpsProp->Value.MVszA.lppszA[i], strDst.c_str());
				}
			}
		} else {
			lpsProp->Value.MVszW.cValues = this->Value.MVszW.cValues;
			hr = ECAllocateMore(this->Value.MVszW.cValues * sizeof(LPWSTR), lpBase, (LPVOID*)&lpsProp->Value.MVszW.lppszW);
			if (hr != hrSuccess)
				dwLastError = hr;

			else {
				for (ULONG i = 0; hr == hrSuccess && i < this->Value.MVszW.cValues; ++i) {
					hr = ECAllocateMore(sizeof(WCHAR) * (wcslen(this->Value.MVszW.lppszW[i]) + 1), lpBase, (LPVOID*)&lpsProp->Value.MVszW.lppszW[i]);
					if (hr != hrSuccess)
						dwLastError = hr;
					else
						wcscpy(lpsProp->Value.MVszW.lppszW[i], this->Value.MVszW.lppszW[i]);
				}
			}
		}
		break;
	}
	case PT_MV_CLSID: {
		GUID *lpGuid;
		hr = ECAllocateMore(this->Value.MVguid.cValues * sizeof(GUID), lpBase, (LPVOID *)&lpGuid);

		if(hr != hrSuccess)
			dwLastError = hr;
		else {
			memcpy(lpGuid, this->Value.MVguid.lpguid, sizeof(GUID) * this->Value.MVguid.cValues);
			lpsProp->Value.MVguid.cValues = this->Value.MVguid.cValues;
			lpsProp->Value.MVguid.lpguid = lpGuid;
		}
		break;
	}
	case PT_MV_I8: {
		LARGE_INTEGER *lpLarge;
		hr = ECAllocateMore(this->Value.MVli.cValues * sizeof(LARGE_INTEGER), lpBase, (LPVOID *)&lpLarge);

		if(hr != hrSuccess)
			dwLastError = hr;
		else {
			lpsProp->Value.MVli.cValues = this->Value.MVli.cValues;
			memcpy(lpLarge, this->Value.MVli.lpli, this->Value.MVli.cValues * sizeof(LARGE_INTEGER));
			lpsProp->Value.MVli.lpli = lpLarge;
		}
		break;
	}
	case PT_ERROR:
		lpsProp->Value.err = this->Value.err;
		break;
	default: // I hope there are no pointers involved here!
		assert(false);
		lpsProp->Value = this->Value;
		break;
	}
	return hr;
}

bool ECProperty::operator==(const ECProperty &property) const {
	DEBUG_GUARD;

	return	property.ulPropTag == this->ulPropTag ||
			(
				PROP_ID(property.ulPropTag) == PROP_ID(this->ulPropTag) && 
				(
					(PROP_TYPE(property.ulPropTag) == PT_STRING8 && PROP_TYPE(this->ulPropTag) == PT_UNICODE) ||
					(PROP_TYPE(property.ulPropTag) == PT_MV_STRING8 && PROP_TYPE(this->ulPropTag) == PT_MV_UNICODE)
				)
			);
}

SPropValue ECProperty::GetMAPIPropValRef(void) const
{
	DEBUG_GUARD;

	SPropValue ret;

	ret.ulPropTag = this->ulPropTag;
	ret.dwAlignPad = 0;
	ret.Value = this->Value;

	return ret;
}
