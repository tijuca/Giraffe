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

#include <mapidefs.h>
#include <mapiutil.h>
#include <mapispi.h>

#include <string>
#include <stack>
#include <set>
#include <map>

#include "edkmdb.h"

#include "Util.h"
#include "ECIConv.h"
#include "CommonUtil.h"
#include "stringutil.h"
#include "charset/convert.h"
#include "tstring.h"

#include "ECMemStream.h"
#include "IECSingleInstance.h"
#include "ECGuid.h"
#include "codepage.h"
#include "rtfutil.h"
#include "mapiext.h"

#include "ustringutil.h"
#include "mapi_ptr.h"

#include "HtmlToTextParser.h"
#include "ECLogger.h"
#include "HtmlEntity.h"

using namespace std;

#include "ECGetText.h"
#include "charset/convert.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define BLOCKSIZE	65536

// HACK: prototypes may differ depending on the compiler and/or system (the
// second parameter may or may not be 'const'). This redeclaration is a hack
// to have a common prototype "iconv_cast".
class iconv_HACK
{
public:
	iconv_HACK(const char** ptr) : m_ptr(ptr) { }

	// the compiler will choose the right operator
	operator const char**() { return m_ptr; }
	operator char**() { return const_cast <char**>(m_ptr); }

private:
	const char** m_ptr;
};


class PropTagCompare
{
public:
	PropTagCompare() {}
	bool operator()(ULONG lhs, ULONG rhs) const { 
		if (PROP_TYPE(lhs) == PT_UNSPECIFIED || PROP_TYPE(rhs) == PT_UNSPECIFIED)
			return PROP_ID(lhs) < PROP_ID(rhs); 
		return lhs < rhs;
	}
};

typedef std::set<ULONG,PropTagCompare> PropTagSet;


/** 
 * Add or replaces a prop value in a an SPropValue array
 * 
 * @param[in] lpSrc Array of properties
 * @param[in] cValues Number of properties in lpSrc
 * @param[in] lpToAdd Add or replace this property
 * @param[out] lppDest All properties returned
 * @param[out] cDestValues Number of properties in lppDest
 * 
 * @return MAPI error code
 */
HRESULT Util::HrAddToPropertyArray(LPSPropValue lpSrc, ULONG cValues, LPSPropValue lpToAdd, LPSPropValue *lppDest, ULONG *cDestValues)
{
	HRESULT hr = hrSuccess;
	LPSPropValue lpDest = NULL;
	LPSPropValue lpFind = NULL;
	unsigned int i = 0;
	unsigned int n = 0;

	hr = MAPIAllocateBuffer(sizeof(SPropValue) * (cValues + 1), (void **)&lpDest);
	if (hr != hrSuccess)
		goto exit;

	for(i=0;i<cValues;i++) {
		hr = HrCopyProperty(&lpDest[n], &lpSrc[i], lpDest);

		if(hr == hrSuccess)
			n++;

		hr = hrSuccess;
	}

	lpFind = PpropFindProp(lpDest, n, lpToAdd->ulPropTag);

	if(lpFind) {
		hr = HrCopyProperty(lpFind, lpToAdd, lpDest);
	} else {

		hr = HrCopyProperty(&lpDest[n], lpToAdd, lpDest);

		n++;
	}

	if(hr != hrSuccess)
		goto exit;

	*lppDest = lpDest;
	*cDestValues = n;

exit:
	return hr;
}

/**
 * Check if object supports HTML
 *
 * @param lpMapiProp Interface to check
 * @result TRUE if object supports unicode, FALSE if not or error
 */
#ifndef STORE_HTML_OK
#define STORE_HTML_OK 0x00010000
#endif
bool Util::FHasHTML(IMAPIProp *lpProp)
{
	HRESULT hr = hrSuccess;
	LPSPropValue lpPropSupport = NULL;

	hr = HrGetOneProp(lpProp, PR_STORE_SUPPORT_MASK, &lpPropSupport);
	if(hr != hrSuccess)
		goto exit;

	if((lpPropSupport->Value.ul & STORE_HTML_OK) == 0)
		hr = MAPI_E_NOT_FOUND;

exit:
	if(lpPropSupport)
		MAPIFreeBuffer(lpPropSupport);

	return hr == hrSuccess;
}

/** 
 * Merges to proptag arrays, lpAdds properties may overwrite properties from lpSrc
 * 
 * @param[in] lpSrc Source property array
 * @param[in] cValues Number of properties in lpSrc
 * @param[in] lpAdds Properties to combine with lpSrc, adding or replacing values
 * @param[in] cAddValues Number of properties in lpAdds
 * @param[out] lppDest New array containing all properties
 * @param[out] cDestValues Number of properties in lppDest
 * 
 * @return MAPI error code
 */
HRESULT	Util::HrMergePropertyArrays(LPSPropValue lpSrc, ULONG cValues, LPSPropValue lpAdds, ULONG cAddValues, LPSPropValue *lppDest, ULONG *cDestValues)
{
	HRESULT hr = hrSuccess;
	map<ULONG, LPSPropValue> mapPropSource;
	map<ULONG, LPSPropValue>::iterator iterPropSource;
	ULONG i = 0;
	LPSPropValue lpProps = NULL;

	for (i = 0; i < cValues; i ++) {
		mapPropSource[lpSrc[i].ulPropTag] = &lpSrc[i];
	}

	for (i = 0; i < cAddValues; i ++) {
		mapPropSource[lpAdds[i].ulPropTag] = &lpAdds[i];
	}

	hr = MAPIAllocateBuffer(sizeof(SPropValue)*mapPropSource.size(), (void**)&lpProps);
	if (hr != hrSuccess)
		goto exit;

	for (i = 0, iterPropSource = mapPropSource.begin(); iterPropSource != mapPropSource.end(); iterPropSource++, i++) {
		hr = Util::HrCopyProperty(&lpProps[i], iterPropSource->second, lpProps);
		if (hr != hrSuccess)
			goto exit;
	}

	*cDestValues = i;
	*lppDest = lpProps;
	lpProps = NULL;

exit:
	if (lpProps)
		MAPIFreeBuffer(lpProps);

	return hr;
}

/** 
 * Copies a whole array of properties, but leaves the external data
 * where it is (ie binary, string data is not copied). PT_ERROR
 * properties can be filtered.
 * 
 * @param[in] lpSrc Array of properties
 * @param[in] cValues Number of values in lpSrc
 * @param[out] lppDest Duplicate array with data pointers into lpSrc values
 * @param[out] cDestValues Number of values in lppDest
 * @param[in] bExcludeErrors if true, copy even PT_ERROR properties
 * 
 * @return MAPI error code
 */
HRESULT Util::HrCopyPropertyArrayByRef(LPSPropValue lpSrc, ULONG cValues, LPSPropValue *lppDest, ULONG *cDestValues, bool bExcludeErrors)
{
    HRESULT hr = hrSuccess;
    LPSPropValue lpDest = NULL;
    unsigned int i = 0;
    unsigned int n = 0;
    
    hr = MAPIAllocateBuffer(sizeof(SPropValue) * cValues, (void **)&lpDest);
	if (hr != hrSuccess)
		goto exit;
    
    for (i = 0; i < cValues; i++) {
        if(!bExcludeErrors || PROP_TYPE(lpSrc[i].ulPropTag) != PT_ERROR) {
            hr = HrCopyPropertyByRef(&lpDest[n], &lpSrc[i]);
        
            if(hr == hrSuccess)
                n++;
            
            hr = hrSuccess;
        }
    }
    
    *lppDest = lpDest;
	*cDestValues = n;

exit:    
    return hr;
}

/** 
 * Copies a whole array of properties, data of lpSrc will also be
 * copied into lppDest.  PT_ERROR properties can be filtered.
 * 
 * @param[in] lpSrc Array of properties
 * @param[in] cValues Number of values in lpSrc
 * @param[out] lppDest Duplicate array with data pointers into lpSrc values
 * @param[out] cDestValues Number of values in lppDest
 * @param[in] bExcludeErrors if true, copy even PT_ERROR properties
 * 
 * @return MAPI error code
 */
HRESULT Util::HrCopyPropertyArray(LPSPropValue lpSrc, ULONG cValues, LPSPropValue *lppDest, ULONG *cDestValues, bool bExcludeErrors)
{
	HRESULT hr = hrSuccess;
	LPSPropValue lpDest = NULL;
	unsigned int i = 0;
	unsigned int n = 0;

	hr = MAPIAllocateBuffer(sizeof(SPropValue) * cValues, (void **)&lpDest);
	if (hr != hrSuccess)
		goto exit;

	for (i = 0; i < cValues; i++) {
	    if(!bExcludeErrors || PROP_TYPE(lpSrc[i].ulPropTag) != PT_ERROR) {
    		hr = HrCopyProperty(&lpDest[n], &lpSrc[i], lpDest);

	    	if(hr == hrSuccess)
		    	n++;

    		hr = hrSuccess;
        }
	}

	*lppDest = lpDest;
	*cDestValues = n;

exit:
	return hr;
	// FIXME potential incomplete copy possible, should free data (?)
}

/** 
 * Copy array of properties in already allocated location.
 * 
 * @param[in] lpSrc Array of properties to copy
 * @param[in] cValues Number of properties in lpSrc
 * @param[out] lpDest Array of properties with enough space to contain cValues properties
 * @param[in] lpBase Base pointer to use with MAPIAllocateMore
 * 
 * @return MAPI error code
 */
HRESULT Util::HrCopyPropertyArray(LPSPropValue lpSrc, ULONG cValues, LPSPropValue lpDest, void *lpBase)
{
	HRESULT hr = hrSuccess;
	unsigned int i;

	for (i = 0; i < cValues; i++) {
		hr = HrCopyProperty(&lpDest[i], &lpSrc[i], lpBase);

		if(hr != hrSuccess)
			goto exit;
	}

exit:
	return hr;
}

/** 
 * Copy array of properties in already allocated location. but leaves the external data
 * where it is (ie binary, string data is not copied)
 * 
 * @param[in] lpSrc Array of properties to copy
 * @param[in] cValues Number of properties in lpSrc
 * @param[out] lpDest Array of properties with enough space to contain cValues properties
 * @param[in] lpBase Base pointer to use with MAPIAllocateMore
 * 
 * @return MAPI error code
 */
HRESULT Util::HrCopyPropertyArrayByRef(LPSPropValue lpSrc, ULONG cValues, LPSPropValue lpDest)
{
	HRESULT hr = hrSuccess;
	unsigned int i;

	for (i = 0; i < cValues; i++) {
		hr = HrCopyPropertyByRef(&lpDest[i], &lpSrc[i]);

		if(hr != hrSuccess)
			goto exit;
	}

exit:
	return hr;
}

/** 
 * Copies one property to somewhere else, but doesn't copy external data (ie binary or string data)
 * 
 * @param[out] lpDest Destination to copy property to
 * @param[in] lpSrc Source property to make copy of
 * 
 * @return always hrSuccess
 */
HRESULT Util::HrCopyPropertyByRef(LPSPropValue lpDest, LPSPropValue lpSrc)
{
    // Just a simple memcpy !
    memcpy(lpDest, lpSrc, sizeof(SPropValue));
    
    return hrSuccess;
}

/** 
 * Copies one property to somewhere else, alloc'ing space if required
 * 
 * @param[out] lpDest Destination to copy property to
 * @param[in] lpSrc Source property to make copy of
 * @param[in] lpBase Base pointer to use with lpfAllocMore
 * @param[in] lpfAllocMore Pointer to the MAPIAllocateMore function, can be NULL
 * 
 * @return MAPI error code
 */
HRESULT Util::HrCopyProperty(LPSPropValue lpDest, LPSPropValue lpSrc, void *lpBase, ALLOCATEMORE * lpfAllocMore)
{
	HRESULT hr = hrSuccess;

	if(lpfAllocMore == NULL)
		lpfAllocMore = MAPIAllocateMore;

	switch(PROP_TYPE(lpSrc->ulPropTag)) {	
	case PT_I2:
		lpDest->Value.i = lpSrc->Value.i;
		break;
	case PT_LONG:
		lpDest->Value.ul = lpSrc->Value.ul;
		break;
	case PT_BOOLEAN:
		lpDest->Value.b = lpSrc->Value.b;
		break;
	case PT_R4:
		lpDest->Value.flt = lpSrc->Value.flt;
		break;
	case PT_DOUBLE:
		lpDest->Value.dbl = lpSrc->Value.dbl;
		break;
	case PT_APPTIME:
		lpDest->Value.at = lpSrc->Value.at;
		break;
	case PT_CURRENCY:
		lpDest->Value.cur = lpSrc->Value.cur;
		break;
	case PT_SYSTIME:
		lpDest->Value.ft = lpSrc->Value.ft;
		break;
	case PT_I8:
		lpDest->Value.li = lpSrc->Value.li;
		break;
	case PT_UNICODE:
		if(lpSrc->Value.lpszW == NULL) {
			hr = MAPI_E_INVALID_PARAMETER;
			goto exit;
		}

		hr = lpfAllocMore(wcslen(lpSrc->Value.lpszW)*sizeof(wchar_t)+sizeof(wchar_t), lpBase, (void**)&lpDest->Value.lpszW);
		if (hr != hrSuccess)
			goto exit;
		wcscpy(lpDest->Value.lpszW, lpSrc->Value.lpszW);
		break;
	case PT_STRING8:
		if(lpSrc->Value.lpszA == NULL) {
			hr = MAPI_E_INVALID_PARAMETER;
			goto exit;
		}

		hr = lpfAllocMore(strlen(lpSrc->Value.lpszA)*sizeof(char)+sizeof(char), lpBase, (void**)&lpDest->Value.lpszA);
		if (hr != hrSuccess)
			goto exit;
		strcpy(lpDest->Value.lpszA, lpSrc->Value.lpszA);
		break;
	case PT_BINARY:
		if(lpSrc->Value.bin.cb > 0) {
			hr = lpfAllocMore(lpSrc->Value.bin.cb, lpBase, (void **) &lpDest->Value.bin.lpb);
			if (hr != hrSuccess)
				goto exit;
		}

		
		lpDest->Value.bin.cb = lpSrc->Value.bin.cb;
		
		if(lpSrc->Value.bin.cb > 0)
			memcpy(lpDest->Value.bin.lpb, lpSrc->Value.bin.lpb, lpSrc->Value.bin.cb);
		else
			lpDest->Value.bin.lpb = NULL;

		break;
	case PT_CLSID:
		hr = lpfAllocMore(sizeof(GUID), lpBase, (void **)&lpDest->Value.lpguid);
		if (hr != hrSuccess)
			goto exit;
		memcpy(lpDest->Value.lpguid, lpSrc->Value.lpguid, sizeof(GUID));
		break;
	case PT_ERROR:
		lpDest->Value.err = lpSrc->Value.err;
		break;
	case PT_SRESTRICTION:
		if(lpSrc->Value.lpszA == NULL) {
			hr = MAPI_E_INVALID_PARAMETER;
			goto exit;
		}
		// NOTE: we place the object pointer in lpszA to make sure it's on the same offset as Value.x on 32bit as 64bit machines
		hr = lpfAllocMore(sizeof(SRestriction), lpBase, (void **)&lpDest->Value.lpszA);
		if (hr != hrSuccess)
			goto exit;
		hr = Util::HrCopySRestriction((LPSRestriction)lpDest->Value.lpszA, (LPSRestriction)lpSrc->Value.lpszA, lpBase);
		break;
	case PT_ACTIONS:
		if(lpSrc->Value.lpszA == NULL) {
			hr = MAPI_E_INVALID_PARAMETER;
			goto exit;
		}
		// NOTE: we place the object pointer in lpszA to make sure it's on the same offset as Value.x on 32bit as 64bit machines
		hr = lpfAllocMore(sizeof(ACTIONS), lpBase, (void **)&lpDest->Value.lpszA);
		if (hr != hrSuccess)
			goto exit;
		hr = Util::HrCopyActions((ACTIONS *)lpDest->Value.lpszA, (ACTIONS *)lpSrc->Value.lpszA, lpBase);
		break;
	case PT_NULL:
		break;
	case PT_OBJECT:
		lpDest->Value.x = 0;
		break;
	// MV properties
	case PT_MV_I2:
		hr = lpfAllocMore(sizeof(short int) * lpSrc->Value.MVi.cValues, lpBase, (void **)&lpDest->Value.MVi.lpi);
		if (hr != hrSuccess)
			goto exit;
		memcpy(lpDest->Value.MVi.lpi, lpSrc->Value.MVi.lpi, sizeof(short int) * lpSrc->Value.MVi.cValues);
		lpDest->Value.MVi.cValues = lpSrc->Value.MVi.cValues;
		break;
	case PT_MV_LONG:
		hr = lpfAllocMore(sizeof(LONG) * lpSrc->Value.MVl.cValues, lpBase, (void **)&lpDest->Value.MVl.lpl);
		if (hr != hrSuccess)
			goto exit;
		memcpy(lpDest->Value.MVl.lpl, lpSrc->Value.MVl.lpl, sizeof(LONG) * lpSrc->Value.MVl.cValues);
		lpDest->Value.MVl.cValues = lpSrc->Value.MVl.cValues;
		break;
	case PT_MV_FLOAT:
		hr = lpfAllocMore(sizeof(float) * lpSrc->Value.MVflt.cValues, lpBase, (void **)&lpDest->Value.MVflt.lpflt);
		if (hr != hrSuccess)
			goto exit;
		memcpy(lpDest->Value.MVflt.lpflt, lpSrc->Value.MVflt.lpflt, sizeof(float) * lpSrc->Value.MVflt.cValues);
		lpDest->Value.MVflt.cValues = lpSrc->Value.MVflt.cValues;
		break;
	case PT_MV_DOUBLE:
	case PT_MV_APPTIME:
		hr = lpfAllocMore(sizeof(double) * lpSrc->Value.MVdbl.cValues, lpBase, (void **)&lpDest->Value.MVdbl.lpdbl);
		if (hr != hrSuccess)
			goto exit;
		memcpy(lpDest->Value.MVdbl.lpdbl, lpSrc->Value.MVdbl.lpdbl, sizeof(double) * lpSrc->Value.MVdbl.cValues);
		lpDest->Value.MVdbl.cValues = lpSrc->Value.MVdbl.cValues;
		break;
	case PT_MV_I8:
		hr = lpfAllocMore(sizeof(LONGLONG) * lpSrc->Value.MVli.cValues, lpBase, (void **)&lpDest->Value.MVli.lpli);
		if (hr != hrSuccess)
			goto exit;
		memcpy(lpDest->Value.MVli.lpli, lpSrc->Value.MVli.lpli, sizeof(LONGLONG) * lpSrc->Value.MVli.cValues);
		lpDest->Value.MVli.cValues = lpSrc->Value.MVli.cValues;
		break;
	case PT_MV_CURRENCY:
		hr = lpfAllocMore(sizeof(CURRENCY) * lpSrc->Value.MVcur.cValues, lpBase, (void **)&lpDest->Value.MVcur.lpcur);
		if (hr != hrSuccess)
			goto exit;
		memcpy(lpDest->Value.MVcur.lpcur, lpSrc->Value.MVcur.lpcur, sizeof(CURRENCY) * lpSrc->Value.MVcur.cValues);
		lpDest->Value.MVcur.cValues = lpSrc->Value.MVcur.cValues;
		break;
	case PT_MV_SYSTIME:
		hr = lpfAllocMore(sizeof(FILETIME) * lpSrc->Value.MVft.cValues, lpBase, (void **)&lpDest->Value.MVft.lpft);
		if (hr != hrSuccess)
			goto exit;
		memcpy(lpDest->Value.MVft.lpft, lpSrc->Value.MVft.lpft, sizeof(FILETIME) * lpSrc->Value.MVft.cValues);
		lpDest->Value.MVft.cValues = lpSrc->Value.MVft.cValues;
		break;
	case PT_MV_STRING8:
		hr = lpfAllocMore(sizeof(LPSTR *) * lpSrc->Value.MVszA.cValues, lpBase, (void **)&lpDest->Value.MVszA.lppszA);
		if (hr != hrSuccess)
			goto exit;
		for (ULONG i = 0; i < lpSrc->Value.MVszA.cValues; i++) {
			int datalength = strlen(lpSrc->Value.MVszA.lppszA[i]) + 1;
			hr = lpfAllocMore(datalength, lpBase, (void **)&lpDest->Value.MVszA.lppszA[i]);
			if (hr != hrSuccess)
				goto exit;
			memcpy(lpDest->Value.MVszA.lppszA[i], lpSrc->Value.MVszA.lppszA[i], datalength);
		}
		lpDest->Value.MVszA.cValues = lpSrc->Value.MVszA.cValues;
		break;
	case PT_MV_UNICODE:
		hr = lpfAllocMore(sizeof(LPWSTR *) * lpSrc->Value.MVszW.cValues, lpBase, (void **)&lpDest->Value.MVszW.lppszW);
		if (hr != hrSuccess)
			goto exit;
		for (ULONG i = 0; i < lpSrc->Value.MVszW.cValues; i++) {
			hr = lpfAllocMore(wcslen(lpSrc->Value.MVszW.lppszW[i]) * sizeof(WCHAR) + sizeof(WCHAR), lpBase, (void**)&lpDest->Value.MVszW.lppszW[i]);
			if (hr != hrSuccess)
				goto exit;
			wcscpy(lpDest->Value.MVszW.lppszW[i], lpSrc->Value.MVszW.lppszW[i]);
		}
		lpDest->Value.MVszW.cValues = lpSrc->Value.MVszW.cValues;
		break;
	case PT_MV_BINARY:
		hr = lpfAllocMore(sizeof(SBinary) * lpSrc->Value.MVbin.cValues, lpBase, (void **)&lpDest->Value.MVbin.lpbin);
		if (hr != hrSuccess)
			goto exit;
		for (ULONG i = 0; i < lpSrc->Value.MVbin.cValues; i++) {
			hr = lpfAllocMore(lpSrc->Value.MVbin.lpbin[i].cb, lpBase, (void **)&lpDest->Value.MVbin.lpbin[i].lpb);
			if (hr != hrSuccess)
				goto exit;
			memcpy(lpDest->Value.MVbin.lpbin[i].lpb, lpSrc->Value.MVbin.lpbin[i].lpb, lpSrc->Value.MVbin.lpbin[i].cb);
			lpDest->Value.MVbin.lpbin[i].cb = lpSrc->Value.MVbin.lpbin[i].cb;
		}
		lpDest->Value.MVbin.cValues = lpSrc->Value.MVbin.cValues;
		break;
	case PT_MV_CLSID:
		hr = lpfAllocMore(sizeof(GUID) * lpSrc->Value.MVguid.cValues, lpBase, (void **)&lpDest->Value.MVguid.lpguid);
		if (hr != hrSuccess)
			goto exit;
		memcpy(lpDest->Value.MVguid.lpguid, lpSrc->Value.MVguid.lpguid, sizeof(GUID) * lpSrc->Value.MVguid.cValues);
		lpDest->Value.MVguid.cValues = lpSrc->Value.MVguid.cValues;
		break;

	default:
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	lpDest->ulPropTag = lpSrc->ulPropTag;

exit:
	return hr;
}

/** 
 * Make a copy of an SRestriction structure
 * 
 * @param[out] lppDest Copy of the restiction in lpSrc
 * @param[in] lpSrc restriction to make a copy of
 * 
 * @return MAPI error code
 */
HRESULT Util::HrCopySRestriction(LPSRestriction *lppDest, LPSRestriction lpSrc)
{
	HRESULT hr = hrSuccess;
	LPSRestriction lpDest = NULL;

	hr = MAPIAllocateBuffer(sizeof(SRestriction), (void **)&lpDest);
	if (hr != hrSuccess)
		goto exit;

	hr = HrCopySRestriction(lpDest, lpSrc, lpDest);

	if(hr != hrSuccess)
		goto exit;

	*lppDest = lpDest;

exit:
	return hr;
}

/** 
 * Make a copy of an SRestriction struction on a preallocated destination
 * 
 * @param[out] lppDest Copy of the restiction in lpSrc
 * @param[in] lpSrc restriction to make a copy of
 * @param[in] lpBase Base pointer to use with MAPIAllocateMore
 * 
 * @return MAPI error code
 */
HRESULT	Util::HrCopySRestriction(LPSRestriction lpDest, LPSRestriction lpSrc, void *lpBase)
{
	HRESULT hr = hrSuccess;
	unsigned int i = 0;

	if (!lpDest || !lpSrc || !lpBase) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	lpDest->rt = lpSrc->rt;

	switch(lpSrc->rt) {
		case RES_AND:
			lpDest->res.resAnd.cRes = lpSrc->res.resAnd.cRes;
			hr = MAPIAllocateMore(sizeof(SRestriction) * lpSrc->res.resAnd.cRes, lpBase, (void **)&lpDest->res.resAnd.lpRes);
			if (hr != hrSuccess)
				goto exit;

			for(i=0;i<lpSrc->res.resAnd.cRes;i++) {
				hr = HrCopySRestriction(&lpDest->res.resAnd.lpRes[i], &lpSrc->res.resAnd.lpRes[i], lpBase);
				if(hr != hrSuccess)
					goto exit;
			}
			break;
		case RES_OR:
			lpDest->res.resOr.cRes = lpSrc->res.resOr.cRes;
			hr = MAPIAllocateMore(sizeof(SRestriction) * lpSrc->res.resOr.cRes, lpBase, (void **)&lpDest->res.resOr.lpRes);
			if (hr != hrSuccess)
				goto exit;

			for(i=0;i<lpSrc->res.resOr.cRes;i++) {
				hr = HrCopySRestriction(&lpDest->res.resOr.lpRes[i], &lpSrc->res.resOr.lpRes[i], lpBase);
				if(hr != hrSuccess)
					goto exit;
			}
			break;
		case RES_NOT:
			hr = MAPIAllocateMore(sizeof(SRestriction), lpBase, (void **) &lpDest->res.resNot.lpRes);
			if (hr != hrSuccess)
				goto exit;
			hr = HrCopySRestriction(lpDest->res.resNot.lpRes, lpSrc->res.resNot.lpRes, lpBase);
			
			break;
		case RES_CONTENT:
			lpDest->res.resContent.ulFuzzyLevel = lpSrc->res.resContent.ulFuzzyLevel;
			lpDest->res.resContent.ulPropTag = lpSrc->res.resContent.ulPropTag;
			hr = MAPIAllocateMore(sizeof(SPropValue), lpBase, (void **) &lpDest->res.resContent.lpProp);
			if (hr != hrSuccess)
				goto exit;
			hr = HrCopyProperty(lpDest->res.resContent.lpProp, lpSrc->res.resContent.lpProp, lpBase);
			break;
		case RES_PROPERTY:
			lpDest->res.resProperty.relop = lpSrc->res.resProperty.relop;
			lpDest->res.resProperty.ulPropTag = lpSrc->res.resProperty.ulPropTag;
			hr = MAPIAllocateMore(sizeof(SPropValue), lpBase, (void **) &lpDest->res.resProperty.lpProp);
			if (hr != hrSuccess)
				goto exit;
			hr = HrCopyProperty(lpDest->res.resProperty.lpProp, lpSrc->res.resProperty.lpProp, lpBase);
			break;
		case RES_COMPAREPROPS:	 
			lpDest->res.resCompareProps.relop = lpSrc->res.resCompareProps.relop;
			lpDest->res.resCompareProps.ulPropTag1 = lpSrc->res.resCompareProps.ulPropTag1;
			lpDest->res.resCompareProps.ulPropTag2 = lpSrc->res.resCompareProps.ulPropTag2;
			break;
		case RES_BITMASK:			 
			lpDest->res.resBitMask.relBMR = lpSrc->res.resBitMask.relBMR;
			lpDest->res.resBitMask.ulMask = lpSrc->res.resBitMask.ulMask;
			lpDest->res.resBitMask.ulPropTag = lpSrc->res.resBitMask.ulPropTag;
			break;
		case RES_SIZE:			
			lpDest->res.resSize.cb = lpSrc->res.resSize.cb;
			lpDest->res.resSize.relop = lpSrc->res.resSize.relop;
			lpDest->res.resSize.ulPropTag = lpSrc->res.resSize.ulPropTag;
			break;
		case RES_EXIST:			
			lpDest->res.resExist.ulPropTag = lpSrc->res.resExist.ulPropTag;
			break;
		case RES_SUBRESTRICTION:	
			lpDest->res.resSub.ulSubObject = lpSrc->res.resSub.ulSubObject;
			hr = MAPIAllocateMore(sizeof(SRestriction), lpBase, (void **)&lpDest->res.resSub.lpRes);
			if (hr != hrSuccess)
				goto exit;
			hr = HrCopySRestriction(lpDest->res.resSub.lpRes, lpSrc->res.resSub.lpRes, lpBase);
			break;
		case RES_COMMENT: // What a weird restriction type
			lpDest->res.resComment.cValues	= lpSrc->res.resComment.cValues;
			lpDest->res.resComment.lpRes	= NULL;

			if(lpSrc->res.resComment.cValues > 0)
			{
				hr = MAPIAllocateMore(sizeof(SPropValue) * lpSrc->res.resComment.cValues, lpBase, (void **) &lpDest->res.resComment.lpProp);
				if (hr != hrSuccess)
					goto exit;

				hr = HrCopyPropertyArray(lpSrc->res.resComment.lpProp, lpSrc->res.resComment.cValues, lpDest->res.resComment.lpProp, lpBase);
				if(hr != hrSuccess)
					goto exit;
			}

			if(lpSrc->res.resComment.lpRes) {
				hr = MAPIAllocateMore(sizeof(SRestriction), lpBase, (void **) &lpDest->res.resComment.lpRes);
				if (hr != hrSuccess)
					goto exit;
				hr = HrCopySRestriction(lpDest->res.resComment.lpRes, lpSrc->res.resComment.lpRes, lpBase);
			}

			break;
	}

exit:
	return hr;
}

/** 
 * Make a copy of an ACTIONS structure (rules) on a preallocated destination
 * 
 * @param lpDest Copy of the actions in lpSrc
 * @param lpSrc actions to make a copy of
 * @param lpBase Base pointer to use with MAPIAllocateMore
 * 
 * @return MAPI error code
 */
HRESULT	Util::HrCopyActions(ACTIONS * lpDest, ACTIONS * lpSrc, void *lpBase)
{
	HRESULT hr = hrSuccess;
	unsigned int i;

	lpDest->cActions = lpSrc->cActions;
	lpDest->ulVersion = lpSrc->ulVersion;
	hr = MAPIAllocateMore(sizeof(ACTION) * lpSrc->cActions, lpBase, (void **)&lpDest->lpAction);
	if (hr != hrSuccess)
		goto exit;

	memset(lpDest->lpAction, 0, sizeof(ACTION) * lpSrc->cActions);

	for (i = 0; i < lpSrc->cActions; i++) {
		hr = HrCopyAction(&lpDest->lpAction[i], &lpSrc->lpAction[i], lpBase);
		if(hr != hrSuccess)
			goto exit;
	}

exit:
	return hr;
}

/** 
 * Make a copy of one ACTION structure (rules) on a preallocated destination
 * 
 * @param lpDest Copy of the action in lpSrc
 * @param lpSrc action to make a copy of
 * @param lpBase Base pointer to use with MAPIAllocateMore
 * 
 * @return MAPI error code
 */
HRESULT	Util::HrCopyAction(ACTION * lpDest, ACTION * lpSrc, void *lpBase)
{
	HRESULT hr = hrSuccess;

	lpDest->acttype = lpSrc->acttype;
	lpDest->ulActionFlavor = lpSrc->ulActionFlavor;
	lpDest->lpRes = NULL; // also unused
	lpDest->lpPropTagArray = NULL; // unused according to edkmdb.h
	lpDest->ulFlags = lpSrc->ulFlags;

	switch(lpSrc->acttype) {
		case OP_MOVE:
		case OP_COPY:
			lpDest->actMoveCopy.cbStoreEntryId = lpSrc->actMoveCopy.cbStoreEntryId;
			hr = MAPIAllocateMore(lpSrc->actMoveCopy.cbStoreEntryId, lpBase, (void **) &lpDest->actMoveCopy.lpStoreEntryId);
			if(hr != hrSuccess)
				goto exit;
			memcpy(lpDest->actMoveCopy.lpStoreEntryId, lpSrc->actMoveCopy.lpStoreEntryId, lpSrc->actMoveCopy.cbStoreEntryId);

			lpDest->actMoveCopy.cbFldEntryId = lpSrc->actMoveCopy.cbFldEntryId;
			hr = MAPIAllocateMore(lpSrc->actMoveCopy.cbFldEntryId, lpBase, (void **) &lpDest->actMoveCopy.lpFldEntryId);
			if(hr != hrSuccess)
				goto exit;
			memcpy(lpDest->actMoveCopy.lpFldEntryId, lpSrc->actMoveCopy.lpFldEntryId, lpSrc->actMoveCopy.cbFldEntryId);

			break;
		case OP_REPLY:
		case OP_OOF_REPLY:
			lpDest->actReply.cbEntryId = lpSrc->actReply.cbEntryId;
			hr = MAPIAllocateMore(lpSrc->actReply.cbEntryId, lpBase, (void **) &lpDest->actReply.lpEntryId);
			if(hr != hrSuccess)
				goto exit;
			memcpy(lpDest->actReply.lpEntryId, lpSrc->actReply.lpEntryId, lpSrc->actReply.cbEntryId);

			lpDest->actReply.guidReplyTemplate = lpSrc->actReply.guidReplyTemplate;
			break;
		case OP_DEFER_ACTION:
			lpDest->actDeferAction.cbData = lpSrc->actDeferAction.cbData;
			hr = MAPIAllocateMore(lpSrc->actDeferAction.cbData, lpBase, (void **)&lpDest->actDeferAction.pbData);
			if(hr != hrSuccess)
				goto exit;
			memcpy(lpDest->actDeferAction.pbData, lpSrc->actDeferAction.pbData, lpSrc->actDeferAction.cbData);

			break;
		case OP_BOUNCE:
			lpDest->scBounceCode = lpSrc->scBounceCode;
			break;
		case OP_FORWARD:
		case OP_DELEGATE:
			hr = MAPIAllocateMore(CbNewADRLIST(lpSrc->lpadrlist->cEntries), lpBase, (void **)&lpDest->lpadrlist);
			if(hr != hrSuccess)
				goto exit;
			hr = HrCopySRowSet((LPSRowSet)lpDest->lpadrlist, (LPSRowSet)lpSrc->lpadrlist, lpBase);
			break;
		case OP_TAG:
			hr = HrCopyProperty(&lpDest->propTag, &lpSrc->propTag, lpBase);
			break;
		case OP_DELETE:
		case OP_MARK_AS_READ:
			 break;
		default:
			break;
	}

exit:
	return hr;
}

/** 
 * Make a copy of a complete rowset
 * 
 * @param[out] lpDest Preallocated SRowSet structure for destination. Should have enough place for lpSrc->cRows.
 * @param[in] lpSrc Make a copy of the rows in this set
 * @param[in] lpBase Use MAPIAllocateMore with this pointer
 * 
 * @return MAPI error code
 */
HRESULT Util::HrCopySRowSet(LPSRowSet lpDest, LPSRowSet lpSrc, void *lpBase)
{
	HRESULT hr = hrSuccess;
	unsigned int i;

	lpDest->cRows = 0;
	for (i = 0; i < lpSrc->cRows; i++) {
		hr = HrCopySRow(&lpDest->aRow[i], &lpSrc->aRow[i], lpBase);
		if (hr != hrSuccess)
			goto exit;

		lpDest->cRows++;
	}

exit:
	return hr;
}

/** 
 * Make a copy one row of a rowset.
 *
 * @note According to MSDN, rows in an SRowSet should use seperate
 * MAPIAllocate() calls (not MAPIAllocateMore) so that rows can be
 * freed individually.  Make sure to free your RowSet with
 * FreeProws(), which frees the rows individually.
 *
 * However, when you have a rowset within a rowset (eg. lpadrlist in
 * OP_FORWARD and OP_DELEGATE rules) these need to be allocated to the
 * original row, and not separate
 * 
 * @param[out] lpDest Preallocated destination base pointer of the new row
 * @param[in] lpSrc Row to make a copy of
 * @param[in] lpBase Optional base pointer to allocate memory for properties
 * 
 * @return MAPI error code
 */
HRESULT	Util::HrCopySRow(LPSRow lpDest, LPSRow lpSrc, void *lpBase)
{
	HRESULT hr = hrSuccess;

	lpDest->cValues = lpSrc->cValues;

	if (lpBase)
		hr = MAPIAllocateMore(sizeof(SPropValue) * lpSrc->cValues, lpBase, (void **) &lpDest->lpProps);
	else
		hr = MAPIAllocateBuffer(sizeof(SPropValue) * lpSrc->cValues, (void **) &lpDest->lpProps);
	if (hr != hrSuccess)
		goto exit;

	hr = HrCopyPropertyArray(lpSrc->lpProps, lpSrc->cValues, lpDest->lpProps, lpBase ? lpBase : lpDest->lpProps);

exit:
	return hr;
}

HRESULT	Util::HrCopyPropTagArray(LPSPropTagArray lpSrc, LPSPropTagArray *lppDest)
{
	HRESULT hr = hrSuccess;
	SPropTagArrayPtr ptrPropTagArray;

	hr = MAPIAllocateBuffer(CbNewSPropTagArray(lpSrc->cValues), &ptrPropTagArray);
	if (hr != hrSuccess)
		goto exit;

	memcpy(ptrPropTagArray->aulPropTag, lpSrc->aulPropTag, lpSrc->cValues * sizeof *lpSrc->aulPropTag);
	ptrPropTagArray->cValues = lpSrc->cValues;

	*lppDest = ptrPropTagArray.release();

exit:
	return hr;
}

/**
 * Copies a LPSPropTagArray while forcing all string types to either
 * PT_STRING8 or PT_UNICODE according to the MAPI_UNICODE flag in
 * ulFlags.
 *
 * @param[in]	ulFlags	0 or MAPI_UNICODE for PT_STRING8 or PT_UNICODE proptags
 * @param[in]	lpSrc	Source SPropTagArray to copy to lppDest
 * @param[out]	lppDest	Destination SPropTagArray with fixed types for strings
 */
HRESULT Util::HrCopyUnicodePropTagArray(ULONG ulFlags, LPSPropTagArray lpSrc, LPSPropTagArray *lppDest)
{
	HRESULT hr = hrSuccess;
	LPSPropTagArray lpPropTagArray = NULL;

	hr = MAPIAllocateBuffer(CbNewSPropTagArray(lpSrc->cValues), (void**)&lpPropTagArray);
	if (hr != hrSuccess)
		goto exit;

	for (ULONG n = 0; n < lpSrc->cValues; n++) {
		if (PROP_TYPE(lpSrc->aulPropTag[n]) == PT_STRING8 || PROP_TYPE(lpSrc->aulPropTag[n]) == PT_UNICODE)
			lpPropTagArray->aulPropTag[n] = CHANGE_PROP_TYPE(lpSrc->aulPropTag[n], ((ulFlags & MAPI_UNICODE) ? PT_UNICODE : PT_STRING8));
		else
			lpPropTagArray->aulPropTag[n] = lpSrc->aulPropTag[n];
	}
	lpPropTagArray->cValues = lpSrc->cValues;

	*lppDest = lpPropTagArray;

exit:
	return hr;
}

/** 
 * Make a copy of a byte array using MAPIAllocate functions. This
 * function has a special case: when the input size is 0, the returned
 * pointer is not allocated and NULL is returned.
 * 
 * @param[in] ulSize number of bytes in lpSrc to copy
 * @param[in] lpSrc bytes to copy into lppDest
 * @param[out] lpulDestSize number of bytes copied from lpSrc
 * @param[out] lppDest copied buffer
 * @param[in] lpBase Optional base pointer for MAPIAllocateMore
 * 
 * @return MAPI error code
 */
HRESULT Util::HrCopyBinary(ULONG ulSize, LPBYTE lpSrc, ULONG *lpulDestSize, LPBYTE *lppDest, LPVOID lpBase)
{
	HRESULT hr = hrSuccess;
	LPBYTE lpDest = NULL;

	if (ulSize == 0) {
		*lpulDestSize = 0;
		*lppDest = NULL;
		goto exit;
	}

	if (lpBase)
		hr = MAPIAllocateMore(ulSize, lpBase, (void **)&lpDest);
	else
		hr = MAPIAllocateBuffer(ulSize, (void **) &lpDest);
	if (hr != hrSuccess)
		goto exit;

	memcpy(lpDest, lpSrc, ulSize);

	*lppDest = lpDest;
	*lpulDestSize = ulSize;

exit:
	return hr;
}

/** 
 * Make a copy of an EntryID. Since this actually uses HrCopyBinary, this
 * function has a special case: when the input size is 0, the returned
 * pointer is not allocated and NULL is returned.
 * 
 * @param[in] ulSize size of the entryid
 * @param[in] lpSrc the entryid to make a copy of
 * @param[out] lpulDestSize output size of the entryid
 * @param[out] lppDest the copy of the entryid
 * @param[in] lpBase Optional pointer for MAPIAllocateMore
 * 
 * @return MAPI Error code
 */
HRESULT	Util::HrCopyEntryId(ULONG ulSize, LPENTRYID lpSrc, ULONG *lpulDestSize, LPENTRYID* lppDest, LPVOID lpBase)
{
	return HrCopyBinary(ulSize, (LPBYTE)lpSrc, lpulDestSize, (LPBYTE *)lppDest, lpBase);
}

/**
 * Compare two SBinary values.
 * A shorter binary value always compares less to a longer binary value. So only
 * when the two values are equal in size the actual data is compared.
 * 
 * @param[in]	left
 *					The left SBinary value that should be compared to right.
 * @param[in]	right
 *					The right SBinary value that should be compared to left.
 *
 * @return		integer
 * @retval		0	The two values are equal.
 * @retval		<0	The left value is 'less than' the right value.
 * @retval		>0  The left value is 'greater than' the right value.
 */
int Util::CompareSBinary(const SBinary &sbin1, const SBinary &sbin2)
{
	if (sbin1.lpb && sbin2.lpb && sbin1.cb > 0 && sbin1.cb == sbin2.cb)
		return memcmp(sbin1.lpb, sbin2.lpb, sbin1.cb);
	else
		return sbin1.cb - sbin2.cb;
}

/** 
 * Compare two properties, optionally using an ECLocale object for
 * correct string compares. String compares are always done case
 * insensitive.
 *
 * The function cannot compare different typed properties.  The PR_ANR
 * property is a special case, where it checks for 'contains' rather
 * than 'is equal'.
 * 
 * @param[in] lpProp1 property to compare
 * @param[in] lpProp2 property to compare
 * @param[in] locale current locale object
 * @param[out] lpCompareResult the compare result
 *             0, the properties are equal
 *            <0, The left value is 'less than' the right value.
 *            >0, The left value is 'greater than' the right value.
 * 
 * @return MAPI Error code
 * @retval MAPI_E_INVALID_PARAMETER input parameters are NULL or property types are different
 * @retval MAPI_E_INVALID_TYPE the type of the properties is not a valid MAPI type
 */
// @todo: Check if we need unicode string functions here
HRESULT Util::CompareProp(LPSPropValue lpProp1, LPSPropValue lpProp2, const ECLocale &locale, int* lpCompareResult)
{
	HRESULT	hr = hrSuccess;
	int		nCompareResult = 0;
	unsigned int		i;

	if(lpProp1 == NULL || lpProp2 == NULL || lpCompareResult == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	if(PROP_TYPE(lpProp1->ulPropTag) != PROP_TYPE(lpProp2->ulPropTag)) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	switch(PROP_TYPE(lpProp1->ulPropTag)) {
	case PT_I2:
		nCompareResult = lpProp1->Value.i - lpProp2->Value.i;
		break;
	case PT_LONG:
		nCompareResult = lpProp1->Value.ul - lpProp2->Value.ul;
		break;
	case PT_R4:
		if(lpProp1->Value.flt == lpProp2->Value.flt)
			nCompareResult = 0;
		else if(lpProp1->Value.flt < lpProp2->Value.flt)
			nCompareResult = -1;
		else
			nCompareResult = 1;
		break;
	case PT_BOOLEAN:
		nCompareResult = lpProp1->Value.b - lpProp2->Value.b;
		break;
	case PT_DOUBLE:
	case PT_APPTIME:
		if(lpProp1->Value.dbl == lpProp2->Value.dbl)
			nCompareResult = 0;
		else if(lpProp1->Value.dbl < lpProp2->Value.dbl)
			nCompareResult = -1;
		else
			nCompareResult = 1;
		break;
	case PT_I8:
		nCompareResult = (int)(lpProp1->Value.li.QuadPart - lpProp2->Value.li.QuadPart);
		break;
	case PT_UNICODE:
		if (lpProp1->Value.lpszW && lpProp2->Value.lpszW)
			if(lpProp2->ulPropTag == PR_ANR) {
				nCompareResult = wcs_icontains(lpProp1->Value.lpszW, lpProp2->Value.lpszW, locale);
			} else
				nCompareResult = wcs_icompare(lpProp1->Value.lpszW, lpProp2->Value.lpszW, locale);
		else
			nCompareResult = lpProp1->Value.lpszW != lpProp2->Value.lpszW;
		break;
	case PT_STRING8:
		if (lpProp1->Value.lpszA && lpProp2->Value.lpszA)
			if(lpProp2->ulPropTag == PR_ANR) {
				nCompareResult = str_icontains(lpProp1->Value.lpszA, lpProp2->Value.lpszA, locale);
			} else
				nCompareResult = str_icompare(lpProp1->Value.lpszA, lpProp2->Value.lpszA, locale);
		else
			nCompareResult = lpProp1->Value.lpszA != lpProp2->Value.lpszA;
		break;
	case PT_SYSTIME:
	case PT_CURRENCY:
		if(lpProp1->Value.cur.Hi == lpProp2->Value.cur.Hi)
			nCompareResult = lpProp1->Value.cur.Lo - lpProp2->Value.cur.Lo;
		else
			nCompareResult = lpProp1->Value.cur.Hi - lpProp2->Value.cur.Hi;
		break;
	case PT_BINARY:
		nCompareResult = CompareSBinary(lpProp1->Value.bin, lpProp2->Value.bin);
		break;
	case PT_CLSID:
		nCompareResult = memcmp(lpProp1->Value.lpguid, lpProp2->Value.lpguid, sizeof(GUID));
		break;
		
	case PT_MV_I2:
		if (lpProp1->Value.MVi.cValues == lpProp2->Value.MVi.cValues) {
			for(i=0; i < lpProp1->Value.MVi.cValues; i++) {
				nCompareResult = lpProp1->Value.MVi.lpi[i] - lpProp2->Value.MVi.lpi[i];
				if(nCompareResult != 0)
					break;
			}
		} else
			nCompareResult = lpProp1->Value.MVi.cValues - lpProp2->Value.MVi.cValues;
		break;
	case PT_MV_LONG:
		if (lpProp1->Value.MVl.cValues == lpProp2->Value.MVl.cValues) {
			for(i=0; i < lpProp1->Value.MVl.cValues; i++) {
				nCompareResult = lpProp1->Value.MVl.lpl[i] - lpProp2->Value.MVl.lpl[i];
				if(nCompareResult != 0)
					break;
			}
		} else
			nCompareResult = lpProp1->Value.MVl.cValues - lpProp2->Value.MVl.cValues;
		break;
	case PT_MV_R4:
		if (lpProp1->Value.MVflt.cValues == lpProp2->Value.MVflt.cValues) {
			for(i=0; i < lpProp1->Value.MVflt.cValues; i++) {
				nCompareResult = lpProp1->Value.MVflt.lpflt[i] - lpProp2->Value.MVflt.lpflt[i];
				if(nCompareResult != 0)
					break;
			}
		} else
			nCompareResult = lpProp1->Value.MVflt.cValues - lpProp2->Value.MVflt.cValues;
		break;
	case PT_MV_DOUBLE:
	case PT_MV_APPTIME:
		if (lpProp1->Value.MVdbl.cValues == lpProp2->Value.MVdbl.cValues) {
			for(i=0; i < lpProp1->Value.MVdbl.cValues; i++) {
				nCompareResult = lpProp1->Value.MVdbl.lpdbl[i] - lpProp2->Value.MVdbl.lpdbl[i];
				if(nCompareResult != 0)
					break;
			}
		} else
			nCompareResult = lpProp1->Value.MVdbl.cValues - lpProp2->Value.MVdbl.cValues;
		break;
	case PT_MV_I8:
		if (lpProp1->Value.MVli.cValues == lpProp2->Value.MVli.cValues) {
			for(i=0; i < lpProp1->Value.MVli.cValues; i++) {
				nCompareResult = (int)(lpProp1->Value.MVli.lpli[i].QuadPart - lpProp2->Value.MVli.lpli[i].QuadPart);
				if(nCompareResult != 0)
					break;
			}
		} else
			nCompareResult = lpProp1->Value.MVli.cValues - lpProp2->Value.MVli.cValues;
		break;
	case PT_MV_SYSTIME:
	case PT_MV_CURRENCY:
		if (lpProp1->Value.MVcur.cValues == lpProp2->Value.MVcur.cValues) {
			for(i=0; i < lpProp1->Value.MVcur.cValues; i++) {
				if(lpProp1->Value.MVcur.lpcur[i].Hi == lpProp2->Value.MVcur.lpcur[i].Hi)
					nCompareResult = lpProp1->Value.MVcur.lpcur[i].Lo - lpProp2->Value.MVcur.lpcur[i].Lo;
				else
					nCompareResult = lpProp1->Value.MVcur.lpcur[i].Hi - lpProp2->Value.MVcur.lpcur[i].Hi;

				if(nCompareResult != 0)
					break;
			}
		} else
			nCompareResult = lpProp1->Value.MVcur.cValues == lpProp2->Value.MVcur.cValues;
		break;
	case PT_MV_CLSID:
		if (lpProp1->Value.MVguid.cValues == lpProp2->Value.MVguid.cValues) {
			nCompareResult = memcmp(lpProp1->Value.MVguid.lpguid, lpProp2->Value.MVguid.lpguid, sizeof(GUID)*lpProp1->Value.MVguid.cValues);
		} else {
			nCompareResult = lpProp1->Value.MVguid.cValues - lpProp2->Value.MVguid.cValues;
		}
		break;
	case PT_MV_BINARY:
		if (lpProp1->Value.MVbin.cValues == lpProp2->Value.MVbin.cValues) {
			for(i=0; i < lpProp1->Value.MVbin.cValues; i++) {
				nCompareResult = CompareSBinary(lpProp1->Value.MVbin.lpbin[i], lpProp2->Value.MVbin.lpbin[i]);
				if(nCompareResult != 0)
					break;
			}
		} else
			nCompareResult = lpProp1->Value.MVbin.cValues - lpProp2->Value.MVbin.cValues;
		break;
	case PT_MV_UNICODE:
		if (lpProp1->Value.MVszW.cValues == lpProp2->Value.MVszW.cValues) {
			for(i=0; i < lpProp1->Value.MVszW.cValues; i++) {
				if (lpProp1->Value.MVszW.lppszW[i] && lpProp2->Value.MVszW.lppszW[i])
					nCompareResult = wcscasecmp(lpProp1->Value.MVszW.lppszW[i], lpProp2->Value.MVszW.lppszW[i]);
				else
					nCompareResult = lpProp1->Value.MVszW.lppszW[i] != lpProp2->Value.MVszW.lppszW[i];

				if(nCompareResult != 0)
					break;
			}
		} else
			nCompareResult = lpProp1->Value.MVszA.cValues - lpProp2->Value.MVszA.cValues;
		break;
	case PT_MV_STRING8:
		if (lpProp1->Value.MVszA.cValues == lpProp2->Value.MVszA.cValues) {
			for(i=0; i < lpProp1->Value.MVszA.cValues; i++) {
				if (lpProp1->Value.MVszA.lppszA[i] && lpProp2->Value.MVszA.lppszA[i])
					nCompareResult = stricmp(lpProp1->Value.MVszA.lppszA[i], lpProp2->Value.MVszA.lppszA[i]);
				else
					nCompareResult = lpProp1->Value.MVszA.lppszA[i] != lpProp2->Value.MVszA.lppszA[i];

				if(nCompareResult != 0)
					break;
			}
		} else
			nCompareResult = lpProp1->Value.MVszA.cValues - lpProp2->Value.MVszA.cValues;
		break;
	default:
		hr = MAPI_E_INVALID_TYPE;
		goto exit;
		break;
	}

	*lpCompareResult = nCompareResult;

exit:
	return hr;
}

/** 
 * Calculates the number of bytes the property uses of memory,
 * excluding the SPropValue struct itself, and any pointers required in this struct.
 * 
 * @param[in] lpProp The property to calculate the size of
 * 
 * @return size of the property
 */
unsigned int Util::PropSize(LPSPropValue lpProp)
{
	unsigned int ulSize, i;

	if(lpProp == NULL)
		return 0;

	switch(PROP_TYPE(lpProp->ulPropTag)) {
	case PT_I2:
		return 2;
	case PT_BOOLEAN:
	case PT_R4:
	case PT_LONG:
		return 4;
	case PT_APPTIME:
	case PT_DOUBLE:
	case PT_I8:
		return 8;
	case PT_UNICODE:
		return lpProp->Value.lpszW ? wcslen(lpProp->Value.lpszW) : 0;
	case PT_STRING8:
		return lpProp->Value.lpszA ? strlen(lpProp->Value.lpszA) : 0;
	case PT_SYSTIME:
	case PT_CURRENCY:
		return 8;
	case PT_BINARY:
		return lpProp->Value.bin.cb;
	case PT_CLSID:
		return sizeof(GUID);
	case PT_MV_I2:
		return 2 * lpProp->Value.MVi.cValues;
	case PT_MV_R4:
		return 4 * lpProp->Value.MVflt.cValues;
	case PT_MV_LONG:
		return 4 * lpProp->Value.MVl.cValues;
	case PT_MV_APPTIME:
	case PT_MV_DOUBLE:
		return 8 * lpProp->Value.MVdbl.cValues;
	case PT_MV_I8:
		return 8 * lpProp->Value.MVli.cValues;
	case PT_MV_UNICODE:
		ulSize = 0;
		for(i=0; i < lpProp->Value.MVszW.cValues; i++)
			ulSize += (lpProp->Value.MVszW.lppszW[i]) ? wcslen(lpProp->Value.MVszW.lppszW[i]) : 0;
		return ulSize;
	case PT_MV_STRING8:
		ulSize = 0;
		for(i=0; i < lpProp->Value.MVszA.cValues; i++)
			ulSize += (lpProp->Value.MVszA.lppszA[i]) ? strlen(lpProp->Value.MVszA.lppszA[i]) : 0;
		return ulSize;
	case PT_MV_SYSTIME:
	case PT_MV_CURRENCY:
		return 8 * lpProp->Value.MVcur.cValues;	
	case PT_MV_BINARY:
		ulSize = 0;
		for(i=0; i < lpProp->Value.MVbin.cValues; i++)
			ulSize+= lpProp->Value.MVbin.lpbin[i].cb;
		return ulSize;
	case PT_MV_CLSID:
		return sizeof(GUID) * lpProp->Value.MVguid.cValues;
	default:
		return 0;
	}
}

/**
 * Convert plaintext to HTML using streams.
 *
 * Converts the text stream to HTML, and writes in the html stream.
 * Both streams will be at the end on return.  This function does no
 * error checking.  If a character in the text cannot be represented
 * in the given codepage, it will make a unicode HTML entity instead.
 *
 * @param[in]	text	IStream object as plain text input, must be PT_UNICODE
 * @param[out]	html	IStream object for HTML output
 * @param[in]	ulCodePage Codepage to convert HTML stream to
 *
 * @return		HRESULT				Mapi error code, from IMemStream
 * @retval		MAPI_E_NOT_FOUND	No suitable charset found for given ulCodepage
 * @retval		MAPI_E_BAD_CHARWIDTH Iconv error
 */
// @todo please optimize function, quite slow. (mostly due to HtmlEntityFromChar)
#define BUFSIZE 65536
HRESULT Util::HrTextToHtml(IStream *text, IStream *html, ULONG ulCodepage)
{
	HRESULT hr = hrSuccess;
	ULONG cRead;
	std::wstring strHtml;
	WCHAR lpBuffer[BUFSIZE];
	char* header1 = "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 3.2//EN\">\n" \
					"<HTML>\n" \
					"<HEAD>\n" \
					"<META HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html; charset=";
	// inserts charset in header here
	char* header2 = "\">\n"												\
					"<META NAME=\"Generator\" CONTENT=\"Zarafa HTML builder 1.0\">\n" \
					"<TITLE></TITLE>\n" \
					"</HEAD>\n" \
					"<BODY>\n" \
					"<!-- Converted from text/plain format -->\n" \
					"\n" \
					"<P><FONT STYLE=\"font-family: courier\" SIZE=2>\n";
					
	char* footer = "</FONT>\n" \
					"</P>\n" \
					"\n" \
					"</BODY>" \
					"</HTML>";
	ULONG i = 0;

	size_t	stRead = 0;
	size_t	stWrite = 0;
	size_t	stWritten;
	size_t  err;
	const char	*readBuffer = NULL;
	char	*writeBuffer = NULL;
	char	*wPtr = NULL;
	iconv_t	cd = (iconv_t)-1;
	char	*lpszCharset;

	hr = HrGetCharsetByCP(ulCodepage, &lpszCharset);
	if (hr != hrSuccess) {
		// client actually should have set the PR_INTERNET_CPID to the correct value
		lpszCharset = "us-ascii";
		hr = hrSuccess;
	}

	cd = iconv_open(lpszCharset, CHARSET_WCHAR);
	if (cd == (iconv_t)-1) {
		hr = MAPI_E_BAD_CHARWIDTH;
		goto exit;
	}

	try {
		writeBuffer = new char[BUFSIZE * 2];
	} catch (...) {
		hr = MAPI_E_NOT_ENOUGH_MEMORY;
		goto exit;
	}

	// @todo, run this through iconv aswell?
	hr = html->Write(header1, strlen(header1), NULL);
	if (hr != hrSuccess)
		goto exit;

	hr = html->Write(lpszCharset, strlen(lpszCharset), NULL);
	if (hr != hrSuccess)
		goto exit;

	hr = html->Write(header2, strlen(header2), NULL);
	if (hr != hrSuccess)
		goto exit;

	while (1) {
		strHtml.clear();

		hr = text->Read(lpBuffer, BUFSIZE*sizeof(WCHAR), &cRead);
		if (hr != hrSuccess)
			goto exit;

		if (cRead == 0)
			break;

		cRead /= sizeof(WCHAR);

		// escape some characters in HTML
		for (i = 0; i < cRead; i++) {
			if (lpBuffer[i] == ' ') {
				if ((i+1) < cRead && lpBuffer[i+1] == ' ')
					strHtml += L"&nbsp;";
				else
					strHtml += L" ";
			} else {
				std::wstring str;
				CHtmlEntity::CharToHtmlEntity(lpBuffer[i], str);
				strHtml += str;
			}
		}

		// convert WCHAR to wanted (8bit) charset 
		readBuffer = (const char*)strHtml.c_str();
		stRead = strHtml.size() * sizeof(WCHAR);

		while (stRead > 0) {
			wPtr = writeBuffer;
			stWrite = BUFSIZE * 2;

			err = iconv(cd, iconv_HACK(&readBuffer), &stRead, &wPtr, &stWrite);

			stWritten = (BUFSIZE * 2) - stWrite;
			// write to stream
			hr = html->Write(writeBuffer, stWritten, NULL);
			if (hr != hrSuccess)
				goto exit;

			if (err == (size_t)-1) {
				// make html number from WCHAR entry
				std::string strHTMLUnicode = "&#";
				strHTMLUnicode += stringify(*(WCHAR*)readBuffer);
				strHTMLUnicode += ";";

				hr = html->Write(strHTMLUnicode.c_str(), strHTMLUnicode.length(), NULL);
				if (hr != hrSuccess)
					goto exit;

				// skip unknown character
				readBuffer += sizeof(WCHAR);
				stRead -= sizeof(WCHAR);
			}
		}
	}
	// @todo, run through iconv?
	hr = html->Write(footer, strlen(footer), NULL);

exit:
	if (cd != (iconv_t)-1)
		iconv_close(cd);

	if (writeBuffer)
		delete [] writeBuffer;

	return hr;
}

/** 
 * Convert a plain-text widestring text to html data in specific
 * codepage. No html headers or footers are set in the string like the
 * stream version does.
 * 
 * @param[in] text plaintext to convert to html
 * @param[out] strHTML append to this html string
 * @param[in] ulCodepage html will be in this codepage
 * 
 * @return MAPI Error code
 */
HRESULT Util::HrTextToHtml(const WCHAR *text, std::string &strHTML, ULONG ulCodepage)
{
	HRESULT hr = hrSuccess;
	char	*lpszCharset;
	wstring wHTML;

	hr = HrGetCharsetByCP(ulCodepage, &lpszCharset);
	if (hr != hrSuccess) {
		// client actually should have set the PR_INTERNET_CPID to the correct value
		lpszCharset = "us-ascii";
		hr = hrSuccess;
	}

	// escape some characters in HTML
	for (ULONG i = 0; text[i] != '\0'; i++) {
		if (text[i] == ' ') {
			if (text[i+1] == ' ')
				wHTML += L"&nbsp;";
			else
				wHTML += L" ";
		} else {
			std::wstring str;
			CHtmlEntity::CharToHtmlEntity(text[i], str);
			wHTML += str;
		}
	}

	try {
		strHTML += convert_to<string>(lpszCharset, wHTML, rawsize(wHTML), CHARSET_WCHAR);
	} catch (const convert_exception &e) {
	}

	return hr;
}

struct _rtfcodepages {
	int id;						// RTF codepage ID
	ULONG ulCodepage;			// Windows codepage
} RTFCODEPAGES[] = {
	{437, 437},					// United States IBM
	{708, 0},					// Arabic (ASMO 708)
	{709, 0},					// Arabic (ASMO 449+, BCON V4)
	{710, 0},					// Arabic (transparent Arabic)
	{711, 0},					// Arabic (Nafitha Enhanced)
	{720, 0},					// Arabic (transparent ASMO)
	{819, 0},		 // Windows 3.1 (United States and Western Europe)
	{850, 1252},		 // IBM multilingual
	{852, 1251},		 // Eastern European
	{860, 0},		 // Portuguese
	{862, 0},		 // Hebrew
	{863, 0},		 // French Canadian
	{864, 0},		 // Arabic
	{865, 0},		 // Norwegian
	{866, 0},		 // Soviet Union
	{874, 0},		 // Thai
	{932, 50220},		 // Japanese
	{936, 936},		 // Simplified Chinese
	{949, 0},		 // Korean
	{950, 0},		 // Traditional Chinese
	{1250, 0},		 // Windows 3.1 (Eastern European)
	{1251, 0},		 // Windows 3.1 (Cyrillic)
	{1252, 0},		 // Western European
	{1253, 0},		 // Greek
	{1254, 0},		 // Turkish
	{1255, 0},		 // Hebrew
	{1256, 0},		 // Arabic
	{1257, 0},		 // Baltic
	{1258, 0},		 // Vietnamese
	{1361, 0},		 // Johab
};

/**
 * Convert plaintext to uncompressed RTF using streams.
 *
 * Converts the text stream to RTF, and writes in the rtf stream.
 * Both streams will be at the end on return.  This function does no
 * error checking.
 *
 * @param[in]	text	IStream object as plain text input, must be PT_UNICODE
 * @param[out]	rtf		IStream object for RTF output
 *
 * @return		HRESULT		hrSuccess
 */
// @todo: remove this shizzle ?
HRESULT Util::HrTextToRtf(IStream *text, IStream *rtf)
{
	ULONG cRead;
	WCHAR c[BUFSIZE];
	char* header = "{\\rtf1\\ansi\\ansicpg1252\\fromtext \\deff0{\\fonttbl\n" \
					"{\\f0\\fswiss Arial;}\n" \
					"{\\f1\\fmodern Courier New;}\n" \
					"{\\f2\\fnil\\fcharset2 Symbol;}\n" \
					"{\\f3\\fmodern\\fcharset0 Courier New;}}\n" \
					"{\\colortbl\\red0\\green0\\blue0;\\red0\\green0\\blue255;}\n" \
					"\\uc1\\pard\\plain\\deftab360 \\f0\\fs20 ";
	char* footer = "}";
	ULONG i = 0;

	rtf->Write(header, strlen(header), NULL);

	while(1) {
		text->Read(c, BUFSIZE * sizeof(WCHAR), &cRead);

		if(cRead == 0)
			break;

		cRead /= sizeof(WCHAR);

		for (i = 0; i < cRead; i++) {

			switch (c[i]) {
			case 0:
				break;
			case '\r':
				break;
			case '\n':
				rtf->Write("\\par\n",5,NULL);
				break;

			case '\\':
				rtf->Write("\\\\",2,NULL);
				break;

			case '{':
				rtf->Write("\\{",2,NULL);
				break;

			case '}':
				rtf->Write("\\}",2,NULL);
				break;

			case '\t':
				rtf->Write("\\tab ",5,NULL);
				break;

			case '\f':			// formfeed, ^L
				rtf->Write("\\page\n",6,NULL);
				break;
			default:
				if (c[i] < ' ' || (c[i] > 127 && c[i] <= 255)) {
					char hex[16];
					snprintf(hex, 16, "\\'%X", c[i]);
					rtf->Write(hex, strlen(hex), NULL);
				} else if (c[i] > 255) {
					// make a unicode char. in signed short
					char hex[16];
					snprintf(hex, 16, "\\u%hd ?", (signed short)c[i]); // %hd is signed short (h is the inverse of l modifier)
					rtf->Write(hex, strlen(hex), NULL);
				} else {
					rtf->Write(&c[i], 1, NULL);
				}
			}
		}
	}

	rtf->Write(footer, strlen(footer), NULL);

	return hrSuccess;
}

/** 
 * Find a given property tag in an array of property tags.  Use
 * PT_UNSPECIFIED in the proptype to find the first matching property
 * id in the property array.
 * 
 * @param[in] lpPropTags The property tag array to search in
 * @param[in] ulPropTag The property tag to search for
 * 
 * @return index in the lpPropTags array
 */
LONG Util::FindPropInArray(LPSPropTagArray lpPropTags, ULONG ulPropTag)
{
	unsigned int i = 0;

	if (!lpPropTags)
		return -1;

	for(i=0 ; i < lpPropTags->cValues; i++) {
		if(lpPropTags->aulPropTag[i] == ulPropTag)
			break;
		if(PROP_TYPE(ulPropTag) == PT_UNSPECIFIED && PROP_ID(lpPropTags->aulPropTag[i]) == PROP_ID(ulPropTag))
			break;
	}

	if(i != lpPropTags->cValues)
		return i;

	return -1;
}


/** 
 * Return a human readable string for a specified HRESULT code.  You
 * should only call this function if hr contains an error. If an error
 * code is not specified in this function, it will return 'access
 * denied' as default error string.
 * 
 * @param[in]	hr			return string version for the given value.
 * @param[out]	lppszError	Pointer to a character pointer that will contain the error
 * 							message on success. If no lpBase was provided, the result
 * 							must be freed with MAPIFreeBuffer.
 * @param[in]	lpBase		optional base pointer for use with MAPIAllocateMore.
 * 
 * @retval	hrSuccess on success.
 */
HRESULT Util::HrMAPIErrorToText(HRESULT hr, LPTSTR *lppszError, void *lpBase)
{
	tstring strError;
	LPCTSTR lpszError = NULL;

	if (lppszError == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	switch(hr)
	{
	case MAPI_E_END_OF_SESSION:
		lpszError = _("End of Session");
		break;
	case MAPI_E_NETWORK_ERROR:
		lpszError = _("Connection lost");
		break;
	case MAPI_E_NO_ACCESS:
		lpszError = _("Access denied");
		break;
	case MAPI_E_FOLDER_CYCLE:
		lpszError = _("Unable to move or copy folders. Can't copy folder. A top-level can't be copied to one of its subfolders. Or, you may not have appropriate permissions for the folder. To check your permissions for the folder, right-click the folder, and then click Properties on the shortcut menu.");
		break;
	case MAPI_E_STORE_FULL:
		lpszError = _("The message store has reached its maximum size. To reduce the amount of data in this message store, select some items that you no longer need, and permanently (SHIFT + DEL) delete them.");
		break;
	case MAPI_E_USER_CANCEL:
		lpszError = _("The user canceled the operation, typically by clicking the Cancel button in a dialog box.");
		break;
	case MAPI_E_LOGON_FAILED:
		lpszError = _("A logon session could not be established.");
		break;
	case MAPI_E_COLLISION:
		lpszError = _("The name of the folder being moved or copied is the same as that of a subfolder in the destination folder. The message store provider requires that folder names be unique. The operation stops without completing.");
		break;
	case MAPI_W_PARTIAL_COMPLETION:
		lpszError = _("The operation succeeded, but not all entries were successfully processed, copied, deleted or moved");// emptied
		break;
	case MAPI_E_UNCONFIGURED:
		lpszError = _("The provider does not have enough information to complete the logon. Or, the service provider has not been configured.");
		break;
	case MAPI_E_FAILONEPROVIDER:
		lpszError = _("One of the providers cannot log on, but this error should not disable the other services.");
		break;
	case MAPI_E_DISK_ERROR:
		lpszError = _("A database error or I/O error has occurred.");
		break;
	case MAPI_E_HAS_FOLDERS:
		lpszError = _("The subfolder being deleted contains subfolders.");
		break;
	case MAPI_E_HAS_MESSAGES:
		lpszError = _("The subfolder being deleted contains messages.");
		break;
	default: {
			strError = _("No description available.");
			strError.append(1, ' ');
			strError.append(_("MAPI error code:"));
			strError.append(1, ' ');
			strError.append(tstringify(hr, true));
			lpszError = strError.c_str();
		}
		break;
	}

	if (lpBase == NULL)
		hr = MAPIAllocateBuffer((_tcslen(lpszError) + 1) * sizeof *lpszError, (void**)lppszError);
	else
		hr = MAPIAllocateMore((_tcslen(lpszError) + 1) * sizeof *lpszError, lpBase, (void**)lppszError);
	if (hr != hrSuccess)
		goto exit;

	_tcscpy(*lppszError, lpszError);

exit:
	return hr;
}

/** 
 * Checks for invalid data in a proptag array. NULL input is
 * considered valid. Any known property type is considered valid,
 * including PT_UNSPECIFIED, PT_NULL and PT_ERROR.
 * 
 * @param[in] lpPropTagArray property tag array to validate
 * 
 * @return true for valid, false for invalid
 */
bool Util::ValidatePropTagArray(LPSPropTagArray lpPropTagArray)
{
	bool bResult = false;
	unsigned int i;

	if(lpPropTagArray == NULL){
		bResult = true;
		goto exit;
	}

	for(i=0; i < lpPropTagArray->cValues; i++)
	{
		switch (PROP_TYPE(lpPropTagArray->aulPropTag[i]))
		{
			case PT_UNSPECIFIED:
			case PT_NULL:
			case PT_I2:
			case PT_I4:
			case PT_R4:
			case PT_R8:
			case PT_BOOLEAN:
			case PT_CURRENCY:
			case PT_APPTIME:
			case PT_SYSTIME:
			case PT_I8:
			case PT_STRING8:
			case PT_BINARY:
			case PT_UNICODE:
			case PT_CLSID:
			case PT_OBJECT:
			case PT_MV_I2:
			case PT_MV_LONG:
			case PT_MV_R4:
			case PT_MV_DOUBLE:
			case PT_MV_CURRENCY:
			case PT_MV_APPTIME:
			case PT_MV_SYSTIME:
			case PT_MV_BINARY:
			case PT_MV_STRING8:
			case PT_MV_UNICODE:
			case PT_MV_CLSID:
			case PT_MV_I8:
			case PT_ERROR:
				bResult = true;
				break;
			default:
				bResult = false;
				goto exit;
				break;
		}
	}

exit:
	return bResult;
}

/** 
 * Append the full contents of a stream into a std::string. It might use
 * the ECMemStream interface if available, otherwise it will do a
 * normal stream copy. The position of the stream on return is not
 * stable.
 * 
 * @param[in] sInput The stream to copy data from
 * @param[in] strOutput The string to place data in
 * 
 * @return MAPI Error code
 */
HRESULT Util::HrStreamToString(IStream *sInput, std::string &strOutput) {
	HRESULT hr = hrSuccess;
	ECMemStream *lpMemStream = NULL;
	ULONG ulRead = 0;
	char buffer[BUFSIZE];
	LARGE_INTEGER zero = {{0,0}};

	if (sInput->QueryInterface(IID_ECMemStream, (LPVOID*)&lpMemStream) == hrSuccess) {
		// getsize, getbuffer, assign
		strOutput.append(lpMemStream->GetBuffer(), lpMemStream->GetSize());
		lpMemStream->Release();
	} else {
		// manual copy

		hr = sInput->Seek(zero, SEEK_SET, NULL);
		if (hr != hrSuccess)
			goto exit;

		while(1) {
			hr = sInput->Read(buffer, BUFSIZE, &ulRead);

			if(hr != hrSuccess || ulRead == 0)
				break;

			strOutput.append(buffer, ulRead);
		}
	}

exit:
	return hr;
}

/** 
 * Append the full contents of a stream into a std::string. It might use
 * the ECMemStream interface if available, otherwise it will do a
 * normal stream copy. The position of the stream on return is not
 * stable.
 * 
 * @param[in] sInput The stream to copy data from
 * @param[in] strOutput The string to place data in
 * 
 * @return MAPI Error code
 */
HRESULT Util::HrStreamToString(IStream *sInput, std::wstring &strOutput) {
	HRESULT hr = hrSuccess;
	ECMemStream *lpMemStream = NULL;
	ULONG ulRead = 0;
	char buffer[BUFSIZE];
	LARGE_INTEGER zero = {{0,0}};

	if (sInput->QueryInterface(IID_ECMemStream, (LPVOID*)&lpMemStream) == hrSuccess) {
		// getsize, getbuffer, assign
		strOutput.append((WCHAR*)lpMemStream->GetBuffer(), lpMemStream->GetSize() / sizeof(WCHAR));
		lpMemStream->Release();
	} else {
		// manual copy

		hr = sInput->Seek(zero, SEEK_SET, NULL);
		if (hr != hrSuccess)
			goto exit;

		while(1) {
			hr = sInput->Read(buffer, BUFSIZE, &ulRead);

			if(hr != hrSuccess || ulRead == 0)
				break;

			strOutput.append((WCHAR*)buffer, ulRead / sizeof(WCHAR));
		}
	}

exit:
	return hr;
}

/**
 * Convert a byte stream from ulCodepage to WCHAR.
 *
 * @param[in]	sInput		Input stream
 * @param[in]	ulCodepage	codepage of input stream
 * @param[out]	lppwOutput	output in WCHAR
 * @return MAPI error code
 */
HRESULT Util::HrConvertStreamToWString(IStream *sInput, ULONG ulCodepage, std::wstring *wstrOutput)
{
	HRESULT hr = hrSuccess;
	char	*lpszCharset;
	convert_context converter;
	string data;

	hr = HrGetCharsetByCP(ulCodepage, &lpszCharset);
	if (hr != hrSuccess) {
		lpszCharset = "us-ascii";
		hr = hrSuccess;
	}

	hr = HrStreamToString(sInput, data);
	if (hr != hrSuccess)
		goto exit;

	try {
		wstrOutput->assign(converter.convert_to<wstring>(CHARSET_WCHAR"//IGNORE", data, rawsize(data), lpszCharset));
	} catch (std::exception &e) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

exit:
	return hr;
}

/**
 * Converts HTML (PT_BINARY, with specified codepage) to plain text (PT_UNICODE)
 *
 * @param[in]	html	IStream to PR_HTML
 * @param[out]	text	IStream to PR_BODY_W
 * @param[in]	ulCodepage	codepage of html stream
 * @return		HRESULT	MAPI error code
 */
HRESULT Util::HrHtmlToText(IStream *html, IStream *text, ULONG ulCodepage)
{
	HRESULT hr = hrSuccess;
	wstring wstrHTML;
	CHtmlToTextParser	parser;
	
	hr = HrConvertStreamToWString(html, ulCodepage, &wstrHTML);
	if(hr != hrSuccess)
		goto exit;

	if (!parser.Parse(wstrHTML.c_str())) {
		hr = MAPI_E_CORRUPT_DATA;
		goto exit;
	}

	{
		std::wstring &strText = parser.GetText();

		hr = text->Write(strText.data(), (strText.size()+1)*sizeof(WCHAR), NULL);
	}

exit:
	return hr;
}

/**
 * This converts from HTML to RTF by doing to following:
 *
 * Always escape { and } to \{ and \}
 * Always escape \r\n to \par
 * All HTML tags are converted from, say <BODY onclick=bla> to \r\n{\htmltagX <BODY onclick=bla>}
 * Each tag with text content gets an extra {\htmltag64} to suppress generated <P>'s in the final HTML output
 * Some tags output \htmlrtf \par \htmlrtf0 so that the plaintext version of the RTF has newlines in the right places
 * Some effort is done so that data between <STYLE> tags is output as a single entity
 * <!-- and --> tags are supported and output as a single htmltagX entity
 *
 * This gives an RTF stream that converts back to the original HTML when viewed in OL, but also preserves plaintext content
 * when all HTML content is removed.
 *
 * @param[in]	strHTML	HTML string in WCHAR for uniformity
 * @param[out]	strRTF	RTF output, containing unicode chars
 * @return mapi error code
 */
HRESULT Util::HrHtmlToRtf(const WCHAR *lpwHTML, std::string &strRTF)
{
    HRESULT hr = hrSuccess;
    
	int tag = 0, type = 0;
	stack<unsigned int> stackTag;
	size_t pos = 0;
	bool inTag = false;
	int ulCommentMode = 0;		// 0=no comment, 1=just starting top-level comment, 2=inside comment level 1, 3=inside comment level 2, etc
	int ulStyleMode = 0;
	int ulParMode = 0;
	bool bFirstText = true;
	bool bPlainCRLF = false;

	if (!lpwHTML) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	// @todo default codepage is set on windows-1252, but is this correct for non-western outlooks?
	strRTF = "{\\rtf1\\ansi\\ansicpg1252\\fromhtml1 \\deff0{\\fonttbl\r\n"
	        "{\\f0\\fswiss\\fcharset0 Arial;}\r\n"
            "{\\f1\\fmodern Courier New;}\r\n"
            "{\\f2\\fnil\\fcharset2 Symbol;}\r\n"
            "{\\f3\\fmodern\\fcharset0 Courier New;}\r\n"
            "{\\f4\\fswiss\\fcharset0 Arial;}\r\n"
            "{\\f5\\fswiss Tahoma;}\r\n"
            "{\\f6\\fswiss\\fcharset0 Times New Roman;}}\r\n"
            "{\\colortbl\\red0\\green0\\blue0;\\red0\\green0\\blue255;\\red0\\green0\\blue255;}\r\n"
            "\\uc1\\pard\\plain\\deftab360 \\f0\\fs24 ";
	// \\uc1 is important, for characters that are not supported by the reader of this rtf.

	stackTag.push(RTF_OUTHTML);
    	
	// We 'convert' from HTML to text by doing a rather simple stripping
	// of tags, and conversion of some strings
	while (lpwHTML[pos]) {
		type = RTF_TAG_TYPE_UNK;

		// Remember if this tag should output a CRLF
		if(lpwHTML[pos] == '<') {
            bPlainCRLF = false;
            		
			// Process important tags first
			if(StrCaseCompare(lpwHTML, L"<HTML", pos)) {
				type = RTF_TAG_TYPE_HTML;
			} else if(StrCaseCompare(lpwHTML, L"</HTML", pos)) {
				type = RTF_TAG_TYPE_HTML;
			} else if(StrCaseCompare(lpwHTML, L"<HEAD", pos)) {
				type = RTF_TAG_TYPE_HEAD;
			} else if(StrCaseCompare(lpwHTML, L"</HEAD", pos)) {
				type = RTF_TAG_TYPE_HEAD;
			} else if(StrCaseCompare(lpwHTML, L"<BODY", pos)) {
				type = RTF_TAG_TYPE_BODY;
			} else if(StrCaseCompare(lpwHTML, L"</BODY", pos)) {
				type = RTF_TAG_TYPE_BODY;
			} else if(StrCaseCompare(lpwHTML, L"<P", pos)) {
				type = RTF_TAG_TYPE_P;
				bPlainCRLF = true;
			} else if(StrCaseCompare(lpwHTML, L"</P", pos)) {
				type = RTF_TAG_TYPE_P;
				bPlainCRLF = true;
			} else if(StrCaseCompare(lpwHTML, L"<DIV", pos)) {
				type = RTF_TAG_TYPE_ENDP;
			} else if(StrCaseCompare(lpwHTML, L"</DIV", pos)) {
				type = RTF_TAG_TYPE_ENDP;
				bPlainCRLF = true;
			} else if(StrCaseCompare(lpwHTML, L"<SPAN", pos)) {
				type = RTF_TAG_TYPE_STARTP;
			} else if(StrCaseCompare(lpwHTML, L"</SPAN", pos)) {
				type = RTF_TAG_TYPE_STARTP;
			} else if(StrCaseCompare(lpwHTML, L"<A", pos)) {
				type = RTF_TAG_TYPE_STARTP;
			} else if(StrCaseCompare(lpwHTML, L"</A", pos)) {
				type = RTF_TAG_TYPE_STARTP;
			} else if(StrCaseCompare(lpwHTML, L"<BR", pos)) {
				type = RTF_TAG_TYPE_BR;
				bPlainCRLF = true;
			} else if(StrCaseCompare(lpwHTML, L"<PRE", pos)) {
				type = RTF_TAG_TYPE_PRE;
			} else if(StrCaseCompare(lpwHTML, L"</PRE", pos)) {
				type = RTF_TAG_TYPE_PRE;
			} else if(StrCaseCompare(lpwHTML, L"<FONT", pos)) {
				type = RTF_TAG_TYPE_FONT;
			} else if(StrCaseCompare(lpwHTML, L"</FONT", pos)) {
				type = RTF_TAG_TYPE_FONT;
			} else if(StrCaseCompare(lpwHTML, L"<META", pos)) {
				type = RTF_TAG_TYPE_HEADER;
			} else if(StrCaseCompare(lpwHTML, L"<LINK", pos)) {
				type = RTF_TAG_TYPE_HEADER;
			} else if(StrCaseCompare(lpwHTML, L"<H", pos) && isdigit(lpwHTML[pos+2]) ) {
				type = RTF_TAG_TYPE_HEADER;
			} else if(StrCaseCompare(lpwHTML, L"</H", pos) && isdigit(lpwHTML[pos+3]) ) {
				type = RTF_TAG_TYPE_HEADER;
			} else if(StrCaseCompare(lpwHTML, L"<TITLE", pos)) {
				type = RTF_TAG_TYPE_TITLE;
			} else if(StrCaseCompare(lpwHTML, L"</TITLE", pos)) {
				type = RTF_TAG_TYPE_TITLE;
			} else if(StrCaseCompare(lpwHTML, L"<PLAIN", pos)) {
				type = RTF_TAG_TYPE_FONT;
			} else if(StrCaseCompare(lpwHTML, L"</PLAIN", pos)) {
				type = RTF_TAG_TYPE_FONT;
			}
	        
			if(StrCaseCompare(lpwHTML, L"</", pos)) {
				type |= RTF_FLAG_CLOSE;
			}
		}

        // Set correct state flag if closing tag (RTF_IN*)
        if(type & RTF_FLAG_CLOSE) {
            switch(type & 0xF0) {
                case RTF_TAG_TYPE_HEAD:
                    if(!stackTag.empty() && stackTag.top() == RTF_INHEAD)
                        stackTag.pop();
                    break;
                case RTF_TAG_TYPE_BODY:
                    if(!stackTag.empty() && stackTag.top() == RTF_INBODY)
                        stackTag.pop();
                    break;
                case RTF_TAG_TYPE_HTML:
                    if(!stackTag.empty() && stackTag.top() == RTF_INHTML)
                        stackTag.pop();
                    break;
                default:
                    break;
            }
        }
        
        // Process special tag input
        if(lpwHTML[pos] == '<' && !inTag) {
            if(StrCaseCompare(lpwHTML, L"<!--", pos))
                ulCommentMode++;
            
            if(ulCommentMode == 0) {
                if(StrCaseCompare(lpwHTML, L"<STYLE", pos))
                    ulStyleMode = 1;
                else if(StrCaseCompare(lpwHTML, L"</STYLE", pos)) {
                    if(ulStyleMode == 3) {
                        // Close the style content tag
                        strRTF += "}";
                    }
                    ulStyleMode = 0;
                } else if(StrCaseCompare(lpwHTML, L"<DIV", pos) || StrCaseCompare(lpwHTML, L"<P", pos)) {
                    ulParMode = 1;
                } else if(StrCaseCompare(lpwHTML, L"</DIV", pos) || StrCaseCompare(lpwHTML, L"</P", pos)) {
                    ulParMode = 0;
                }
            }
            
            if(ulCommentMode < 2 && ulStyleMode < 2) {
                strRTF += "\r\n{\\*\\htmltag" + stringify((ulParMode == 2 ? RTF_FLAG_INPAR : 0) | tag | type | stackTag.top()) + " ";
                inTag = true;
                bFirstText = true;
                
                if(ulCommentMode) {
                    // Inside comment now
                    ulCommentMode++;
                }
            }
        }
        
        // Do actual output
        if(lpwHTML[pos] == '\r') {
            // Ingore \r
        } else if(lpwHTML[pos] == '\n') {
            if(inTag || ulCommentMode || ulStyleMode)
                strRTF += "\\par ";
            else
                strRTF += "\r\n{\\*\\htmltag" + stringify((ulParMode == 2 ? RTF_FLAG_INPAR : 0) | stackTag.top()) + " \\par }";
        } else if(lpwHTML[pos] == '\t') {
            if(inTag || ulCommentMode || ulStyleMode)
                strRTF += "\\tab ";
            else
                strRTF += "\r\n{\\*\\htmltag" + stringify((ulParMode == 2 ? RTF_FLAG_INPAR : 0) | stackTag.top()) + " \\tab }";
        } else if(lpwHTML[pos] == '{') {
            strRTF += "\\{";
        } else if(lpwHTML[pos] == '}') {
            strRTF += "\\}";
        } else if(lpwHTML[pos] == '\\') {
            strRTF += "\\\\";
		} else if(lpwHTML[pos] > 127) {
			// Unicode character
			char hex[12];
			snprintf(hex, 12, "\\u%hd ?", (signed short)lpwHTML[pos]);
			strRTF += hex;
        } else if(StrCaseCompare(lpwHTML, L"&nbsp;", pos)) {
            if(inTag || ulCommentMode || ulStyleMode)
                strRTF += "&nbsp;";
            else
                strRTF += "\r\n{\\*\\htmltag64}{\\*\\htmltag" + stringify((ulParMode == 2 ? RTF_FLAG_INPAR : 0) | RTF_TAG_TYPE_STARTP | stackTag.top()) + " &nbsp;}";
                
            pos+=5;
		} else if(!inTag && !ulCommentMode && !ulStyleMode && lpwHTML[pos] == '&' && CHtmlEntity::validateHtmlEntity(std::wstring(lpwHTML + pos, 10)) ) {
			size_t semicolon = pos;
			while (lpwHTML[semicolon] && lpwHTML[semicolon] != ';') semicolon++;

            if (lpwHTML[semicolon]) {
                std::wstring strEntity;
				WCHAR c;
                std::string strChar;

				strEntity.assign(lpwHTML + pos+1, semicolon-pos-1);
				c = CHtmlEntity::HtmlEntityToChar(strEntity);

				if (c > 32 && c < 128)
					strChar = c;
				else {
					// Unicode character
					char hex[12];
					snprintf(hex, 12, "\\u%hd ?", (signed short)c); // unicode char + ascii representation (see \ucN rtf command)
					strChar = hex;
				}

				// both strChar and strEntity in output, unicode in rtf space, entity in html space
                strRTF += std::string("\\htmlrtf ") + strChar + "\\htmlrtf0{\\*\\htmltag" +
					stringify((ulParMode == 2 ? RTF_FLAG_INPAR : 0) | RTF_TAG_TYPE_STARTP | stackTag.top()) +
					"&" + convert_to<string>(strEntity) + ";}";
                pos += strEntity.size() + 2;
                continue;
            }
        } else {
            if(!inTag && bFirstText) {
                bFirstText = false;
            }
            strRTF += lpwHTML[pos];
        }

        // Do post-processing output
        if(lpwHTML[pos] == '>' && (inTag || ulCommentMode)) {
            if(!ulCommentMode && ulStyleMode < 2)
                strRTF += "}";
                
            if(pos > 2 && StrCaseCompare(lpwHTML, L"-->", pos-2) && ulCommentMode) {
                ulCommentMode--;
                
                if(ulCommentMode == 1) {
                    ulCommentMode = 0;
                    strRTF += "}";
                }
            }

            if(pos > 6 && StrCaseCompare(lpwHTML, L"/STYLE>", pos-6) && ulStyleMode) {
                ulStyleMode = 0;
                strRTF += "}";
            }

            if(ulStyleMode == 1)
                ulStyleMode++;
                
            if(ulParMode == 1)
                ulParMode++;
                
            if(ulStyleMode == 2) {
                // Output the style content as a tag
                ulStyleMode = 3;
                strRTF += "\r\n{\\*\\htmltag" + stringify(RTF_TAG_TYPE_UNK | stackTag.top()) + " ";
            } 
            
            if(!ulStyleMode && !ulCommentMode) {
                // Normal text must have \*\htmltag64 to suppress <p> in the final html output
                strRTF += "{\\*\\htmltag64}";
            }            
            
            inTag = false;

            if(bPlainCRLF && !ulCommentMode && !ulStyleMode) {
                // Add a plaintext newline if needed, but only for non-style and non-comment parts
                strRTF += "\\htmlrtf \\par \\htmlrtf0 ";
            }
        }
        
        
        // Next char
        pos++;

        // Set correct state flag (RTF_IN*)
        if(!(type & RTF_FLAG_CLOSE)) {
            switch(type & 0xF0) {
                case RTF_TAG_TYPE_HTML:
                    stackTag.push(RTF_INHTML);
                    break;
                case RTF_TAG_TYPE_BODY:
                    stackTag.push(RTF_INBODY);
                    break;
                case RTF_TAG_TYPE_HEAD:
                    stackTag.push(RTF_INHEAD);
                    break;
                default:
                    break;
            }
        }
	}
	
	strRTF +="}\r\n";

exit:
	return hr;
}

/**
 * Convert html stream to rtf stream, using a codepage for the html input.
 *
 * We convert the HTML Stream from the codepage to wstring, because of
 * the codepage of the html string, which can be any codepage.
 *
 * @param[in]	html	Stream to the HTML string, read only
 * @param[in]	rtf		Stream to the RTF string, write only
 * @param[in]	ulCodepage	codepage of the HTML input.
 * @return MAPI error code
 */
HRESULT	Util::HrHtmlToRtf(IStream *html, IStream *rtf, unsigned int ulCodepage)
{
	HRESULT hr = hrSuccess;
	wstring wstrHTML;
	std::string strRTF;
	
	hr = HrConvertStreamToWString(html, ulCodepage, &wstrHTML);
	if(hr != hrSuccess)
		goto exit;
	
	hr = HrHtmlToRtf(wstrHTML.c_str(), strRTF);
	if(hr != hrSuccess)
		goto exit;

	hr = rtf->Write(strRTF.c_str(), strRTF.size(), NULL);

exit:
	return hr;
}

/** 
 * Converts binary data into it's hexidecimal string representation.
 * 
 * @param[in] inLength number of bytes in input
 * @param[in] input data buffer to convert
 * @param[out] output MAPIAllocateBuffer/More allocated buffer containing the string
 * @param[in] parent optional pointer for MAPIAllocateMore
 * 
 * @return MAPI error code
 */
HRESULT Util::bin2hex(ULONG inLength, LPBYTE input, char **output, void *parent) {
	const char digits[] = "0123456789ABCDEF";
	char *buffer = NULL;
	HRESULT hr = hrSuccess;
	ULONG i, j;

	if (parent)
		hr = MAPIAllocateMore(inLength*2+1, parent, (void**)&buffer);
	else
		hr = MAPIAllocateBuffer(inLength*2+1, (void**)&buffer);
	if (hr != hrSuccess)
		goto exit;

	for (i = 0, j = 0; i < inLength; i++) {
		buffer[j++] = digits[input[i]>>4];
		buffer[j++] = digits[input[i]&0x0F];
	}

	buffer[j] = '\0';
	*output = buffer;
exit:
	
	return hr;
}

/** 
 * Converts a string containing hexidecimal numbers into binary
 * data. And it adds a 0 at the end of the data.
 * 
 * @todo, check usage of this function to see if the terminating 0 is
 * really useful. should really be removed.
 *
 * @param[in] input string to convert
 * @param[in] len length of the input (must be a multiple of 2)
 * @param[out] outLength length of the output
 * @param[out] output binary version of the input
 * @param[in] parent optional pointer used for MAPIAllocateMore
 * 
 * @return MAPI Error code
 */
HRESULT Util::hex2bin(const char *input, size_t len, ULONG *outLength, LPBYTE *output, void *parent)
{
	HRESULT hr = hrSuccess;
	LPBYTE buffer = NULL;

	if (len % 2 != 0) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	if (parent)
		hr = MAPIAllocateMore(len/2+1, parent, (void**)&buffer);
	else
		hr = MAPIAllocateBuffer(len/2+1, (void**)&buffer);
	if (hr != hrSuccess)
		goto exit;


	hr = hex2bin(input, len, buffer);
	if(hr != hrSuccess)
		goto exit;

	buffer[len/2] = '\0';

	*outLength = len/2;
	*output = buffer;

exit:
	return hr;
}


/** 
 * Converts a string containing hexidecimal numbers into binary
 * data.
 * 
 * @param[in] input string to convert
 * @param[in] len length of the input (must be a multiple of 2)
 * @param[out] output binary version of the input, must be able to receive len/2 bytes
 * 
 * @return MAPI Error code
 */
HRESULT Util::hex2bin(const char *input, size_t len, LPBYTE output)
{
	HRESULT hr = hrSuccess;
	ULONG i, j;

	if (len % 2 != 0) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	for (i = 0, j = 0; i < len; j++) {
		output[j] = x2b(input[i++]) << 4;
		output[j] |= x2b(input[i++]);
	}

exit:
	return hr;
}


/** 
 * Return the original body property tag of a message, or PR_NULL when unknown.
 * 
 * @param[in]	lpBody			Pointer to the SPropValue containing the PR_BODY property.
 * @param[in]	lpHtml			Pointer to the SPropValue containing the PR_HTML property.
 * @param[in]	lpRtfCompressed	Pointer to the SPropValue containing the PR_RTF_COMPRESSED property.
 * @param[in]	lpRtfInSync		Pointer to the SPropValue containing the PR_RTF_IN_SYNC property.
 * @param[in]	ulFlags			If MAPI_UNICODE is specified, the PR_BODY proptag
 * 								will be the PT_UNICODE version. Otherwise the
 * 								PT_STRING8 version is returned.
 * 
 * @return 
 */
ULONG Util::GetBestBody(LPSPropValue lpBody, LPSPropValue lpHtml, LPSPropValue lpRtfCompressed, LPSPropValue lpRtfInSync, ULONG ulFlags)
{
	/**
	 * In this function we try to determine the best body based on the combination of values and error values
	 * for PR_BODY, PR_HTML, PR_RTF_COMPRESSED and PR_RTF_IN_SYNC according to the rules as described in ECMessage.cpp.
	 * Some checks performed here seem redundant, but are actualy required to determine if the source provider
	 * implements this scheme as we expect it (Scalix doesn't always seem to do so).
	 */
	ULONG ulProp = PR_NULL;
	const ULONG ulBodyTag = ((ulFlags & MAPI_UNICODE) ? PR_BODY_W : PR_BODY_A);

	if (lpRtfInSync->ulPropTag != PR_RTF_IN_SYNC)
		goto exit;

	if ((lpBody->ulPropTag == ulBodyTag || (PROP_TYPE(lpBody->ulPropTag) == PT_ERROR && lpBody->Value.err == MAPI_E_NOT_ENOUGH_MEMORY)) &&
		(PROP_TYPE(lpHtml->ulPropTag) == PT_ERROR && lpHtml->Value.err == MAPI_E_NOT_FOUND) &&
		(PROP_TYPE(lpRtfCompressed->ulPropTag) == PT_ERROR && lpRtfCompressed->Value.err == MAPI_E_NOT_FOUND))
	{
		ulProp = ulBodyTag;
		goto exit;
	}

	if ((lpHtml->ulPropTag == PR_HTML || (PROP_TYPE(lpHtml->ulPropTag) == PT_ERROR && lpHtml->Value.err == MAPI_E_NOT_ENOUGH_MEMORY)) &&
		(PROP_TYPE(lpBody->ulPropTag) == PT_ERROR && lpBody->Value.err == MAPI_E_NOT_ENOUGH_MEMORY) &&
		(PROP_TYPE(lpRtfCompressed->ulPropTag) == PT_ERROR && lpRtfCompressed->Value.err == MAPI_E_NOT_ENOUGH_MEMORY) &&
		lpRtfInSync->Value.b == FALSE)
	{
		ulProp = PR_HTML;
		goto exit;
	}

	if ((lpRtfCompressed->ulPropTag == PR_RTF_COMPRESSED || (PROP_TYPE(lpRtfCompressed->ulPropTag) == PT_ERROR && lpRtfCompressed->Value.err == MAPI_E_NOT_ENOUGH_MEMORY)) &&
		(PROP_TYPE(lpBody->ulPropTag) == PT_ERROR && lpBody->Value.err == MAPI_E_NOT_ENOUGH_MEMORY) &&
		(PROP_TYPE(lpHtml->ulPropTag) == PT_ERROR && lpHtml->Value.err == MAPI_E_NOT_FOUND) &&
		lpRtfInSync->Value.b == TRUE)
	{
		ulProp = PR_RTF_COMPRESSED;
		goto exit;
	}

exit:
	return ulProp;
}

/** 
 * Return the original body property tag of a message, or PR_NULL when unknown.
 * 
 * @param[in]	lpPropObj	The object to get the best body proptag from.
 * @param[in]	ulFlags		If MAPI_UNICODE is specified, the PR_BODY proptag
 * 							will be the PT_UNICODE version. Otherwise the
 * 							PT_STRING8 version is returned.
 * 
 * @return 
 */
ULONG Util::GetBestBody(IMAPIProp* lpPropObj, ULONG ulFlags)
{
	ULONG ulProp = PR_NULL;
	HRESULT hr = hrSuccess;
	SPropArrayPtr ptrBodies;
	const ULONG ulBodyTag = ((ulFlags & MAPI_UNICODE) ? PR_BODY_W : PR_BODY_A);
	SizedSPropTagArray (4, sBodyTags) = { 4, {
			ulBodyTag,
			PR_HTML,
			PR_RTF_COMPRESSED,
			PR_RTF_IN_SYNC
		} };
	ULONG cValues = 0;

	hr = lpPropObj->GetProps((LPSPropTagArray)&sBodyTags, 0, &cValues, &ptrBodies);
	if (FAILED(hr))
		goto exit;

	ulProp = GetBestBody(&ptrBodies[0], &ptrBodies[1], &ptrBodies[2], &ptrBodies[3], ulFlags);

exit:
	return ulProp;
}

/** 
 * Return the original body property tag of a message, or PR_NULL when unknown.
 * 
 * @param[in]	lpPropArray	The array of properties on which to base the result.
 *                          This array must include PR_BODY, PR_HTML, PR_RTF_COMPRESSED
 *							and PR_RTF_IN_SYNC.
 * @param[in]	cValues		The number of properties in lpPropArray.
 * @param[in]	ulFlags		If MAPI_UNICODE is specified, the PR_BODY proptag
 * 							will be the PT_UNICODE version. Otherwise the
 * 							PT_STRING8 version is returned.
 * 
 * @return 
 */
ULONG Util::GetBestBody(LPSPropValue lpPropArray, ULONG cValues, ULONG ulFlags)
{
	ULONG ulProp = PR_NULL;
	LPSPropValue lpBody, lpHtml, lpRtfCompressed, lpRtfInSync;

	lpBody = PpropFindProp(lpPropArray, cValues, CHANGE_PROP_TYPE(PR_BODY, PT_UNSPECIFIED));
	if (!lpBody)
		goto exit;

	lpHtml = PpropFindProp(lpPropArray, cValues, CHANGE_PROP_TYPE(PR_HTML, PT_UNSPECIFIED));
	if (!lpHtml)
		goto exit;

	lpRtfCompressed = PpropFindProp(lpPropArray, cValues, CHANGE_PROP_TYPE(PR_RTF_COMPRESSED, PT_UNSPECIFIED));
	if (!lpRtfCompressed)
		goto exit;

	lpRtfInSync = PpropFindProp(lpPropArray, cValues, CHANGE_PROP_TYPE(PR_RTF_IN_SYNC, PT_UNSPECIFIED));
	if (!lpRtfInSync)
		goto exit;

	ulProp = GetBestBody(lpBody, lpHtml, lpRtfCompressed, lpRtfInSync, ulFlags);

exit:
	return ulProp;
}

/**
 * Check if a proptag specifies a body property. This is PR_BODY, PR_HTML
 * or PR_RTF_COMPRESSED. If the type is set to error, it can still classified
 * as a body proptag.
 *
 * @param[in]	ulPropTag	The proptag to check,
 * @retval true if the proptag specified a body property.
 * @retval false otherwise.
 */
bool Util::IsBodyProp(ULONG ulPropTag)
{
	switch (PROP_ID(ulPropTag)) {
		case PROP_ID(PR_BODY):
		case PROP_ID(PR_HTML):
		case PROP_ID(PR_RTF_COMPRESSED):
			return true;

		default:
			return false;
	}
}

/** 
 * Find an interface IID in an array of interface definitions.
 * 
 * @param[in] lpIID interface to find
 * @param[in] ulIIDs number of entries in lpIIDs
 * @param[in] lpIIDs array of interfaces
 * 
 * @return MAPI error code
 * @retval MAPI_E_NOT_FOUND interface not found in array
 */
HRESULT Util::FindInterface(LPCIID lpIID, ULONG ulIIDs, LPCIID lpIIDs) {
	HRESULT hr = MAPI_E_NOT_FOUND;
	ULONG i;

	if (!lpIIDs || !lpIID)
		goto exit;

	for (i = 0; i < ulIIDs; i++) {
		if (*lpIID == lpIIDs[i]) {
			hr = hrSuccess;
			break;
		}
	}

exit:
	return hr;
}

/** 
 * Copy a complete stream to another.
 * 
 * @param[in] lpSrc Input stream to copy
 * @param[in] lpDest Stream to append data of lpSrc to
 * 
 * @return MAPI error code
 */
HRESULT Util::CopyStream(LPSTREAM lpSrc, LPSTREAM lpDest) {
	HRESULT hr;
	ULARGE_INTEGER liRead = {{0}}, liWritten = {{0}};
	STATSTG stStatus;

	hr = lpSrc->Stat(&stStatus, 0);
	if (FAILED(hr))
		goto exit;

	hr = lpSrc->CopyTo(lpDest, stStatus.cbSize, &liRead, &liWritten);
	if (FAILED(hr))
		goto exit;

	if (liRead.QuadPart != liWritten.QuadPart) {
		hr = MAPI_W_PARTIAL_COMPLETION;
		goto exit;
	}

	hr = lpDest->Commit(0);

exit:
	return hr;
}

/** 
 * Copy all recipients from a source message to another message.
 * 
 * @param[in] lpSrc Message containing recipients to copy
 * @param[out] lpDest Message to add (append) all recipients to
 * 
 * @return MAPI error code
 */
HRESULT Util::CopyRecipients(LPMESSAGE lpSrc, LPMESSAGE lpDest) {
	HRESULT hr;
	LPMAPITABLE lpTable = NULL;
	LPSRowSet lpRows = NULL;
	LPSPropTagArray lpTableColumns = NULL;
	ULONG ulRows = 0;

	hr = lpSrc->GetRecipientTable(MAPI_UNICODE, &lpTable);
	if (hr != hrSuccess)
		goto exit;

	hr = lpTable->QueryColumns(TBL_ALL_COLUMNS, &lpTableColumns);
	if (hr != hrSuccess)
		goto exit;

	hr = lpTable->SetColumns(lpTableColumns, 0);
	if (hr != hrSuccess)
		goto exit;

	hr = lpTable->GetRowCount(0, &ulRows);
	if (hr != hrSuccess)
		goto exit;

	if (ulRows == 0)	// Nothing to do!
		goto exit;

	hr = lpTable->QueryRows(ulRows, 0, &lpRows);
	if (hr != hrSuccess)
		goto exit;

	// LPADRLIST and LPSRowSet are binary compatible \o/
	hr = lpDest->ModifyRecipients(MODRECIP_ADD, (LPADRLIST)lpRows);
	if (hr != hrSuccess)
		goto exit;

exit:
	if (lpTableColumns)
		MAPIFreeBuffer(lpTableColumns);

	if (lpRows)
		FreeProws(lpRows);

	if (lpTable)
		lpTable->Release();

	return hr;
}

/** 
 * Copy an single-instance id to another object, if possible.
 * 
 * @param lpSrc Source object (message or attachment)
 * @param lpDst Destination object to have the same contents as source
 * 
 * @return always hrSuccess
 */
HRESULT Util::CopyInstanceIds(LPMAPIPROP lpSrc, LPMAPIPROP lpDst)
{
	HRESULT hr = hrSuccess;
	IECSingleInstance *lpSrcInstance = NULL;
	IECSingleInstance *lpDstInstance = NULL;
	ULONG cbInstanceID = 0;
	LPENTRYID lpInstanceID = NULL;

	/* 
	 * We are always going to return hrSuccess, if for some reason we can't copy the single instance,
	 * we always have the real data as fallback.
	 */
	if (lpSrc->QueryInterface(IID_IECSingleInstance, (LPVOID *)&lpSrcInstance) != hrSuccess)
		goto exit;

	if (lpDst->QueryInterface(IID_IECSingleInstance, (LPVOID *)&lpDstInstance) != hrSuccess)
		goto exit;

	/*
	 * Transfer instance Id, if this succeeds we're in luck and we might not
	 * have to send the attachment to the server. Note that while SetSingleInstanceId()
	 * might succeed now, the attachment might be deleted on the server between
	 * SetSingleInstanceId() and SaveChanges(). In that case SaveChanges will fail
	 * and we will have to resend the attachment data.
	 */
	if (lpSrcInstance->GetSingleInstanceId(&cbInstanceID, &lpInstanceID) != hrSuccess)
		goto exit;

	if (lpDstInstance->SetSingleInstanceId(cbInstanceID, lpInstanceID) != hrSuccess)
		goto exit;

exit:
	if (lpSrcInstance)
		lpSrcInstance->Release();
	if (lpDstInstance)
		lpDstInstance->Release();
	if (lpInstanceID)
		MAPIFreeBuffer(lpInstanceID);

	return hr;
}

/** 
 * Copy all attachment properties from one attachment to another. The
 * exclude property tag array is optional.
 * 
 * @param[in] lpSrcAttach Attachment to copy data from
 * @param[out] lpDstAttach Attachment to copy data to
 * @param[in] lpExcludeProps Optional list of properties to not copy
 * 
 * @return MAPI error code
 */
HRESULT Util::CopyAttachmentProps(LPATTACH lpSrcAttach, LPATTACH lpDstAttach, LPSPropTagArray lpExcludeProps)
{
	HRESULT hr = hrSuccess;

	hr = Util::DoCopyTo(&IID_IAttachment, lpSrcAttach, 0, NULL, lpExcludeProps, 0, NULL, &IID_IAttachment, lpDstAttach, 0, NULL);
	if (hr != hrSuccess)
		goto exit;

exit:
	return hr;
}

/** 
 * Copy all attachments from one message to another.
 * 
 * @param[in] lpSrc Source message to copy from
 * @param[in] lpDest Message to copy attachments to
 * 
 * @return MAPI error code
 */
HRESULT Util::CopyAttachments(LPMESSAGE lpSrc, LPMESSAGE lpDest) {
	HRESULT hr;
	bool bPartial = false;

	// table
	LPMAPITABLE lpTable = NULL;
	LPSRowSet lpRows = NULL;
	LPSPropTagArray lpTableColumns = NULL;
	ULONG ulRows = 0;

	// attachments
	LPSPropValue lpAttachNum = NULL;
	LPSPropValue lpHasAttach = NULL;
	ULONG ulAttachNr = 0;
	LPATTACH lpSrcAttach = NULL;
	LPATTACH lpDestAttach = NULL;

	hr = HrGetOneProp(lpSrc, PR_HASATTACH, &lpHasAttach);
	if (hr != hrSuccess) {
		hr = hrSuccess;
		goto exit;
	}
	if (lpHasAttach->Value.b == FALSE)
		goto exit;

	hr = lpSrc->GetAttachmentTable(MAPI_UNICODE, &lpTable);
	if (hr != hrSuccess)
		goto exit;

	hr = lpTable->QueryColumns(TBL_ALL_COLUMNS, &lpTableColumns);
	if (hr != hrSuccess)
		goto exit;

	hr = lpTable->SetColumns(lpTableColumns, 0);
	if (hr != hrSuccess)
		goto exit;

	hr = lpTable->GetRowCount(0, &ulRows);
	if (hr != hrSuccess)
		goto exit;

	hr = lpTable->QueryRows(ulRows, 0, &lpRows);
	if (hr != hrSuccess)
		goto exit;

	for (ULONG i=0; i < lpRows->cRows; i++) {
		lpAttachNum = PpropFindProp(lpRows->aRow[i].lpProps, lpRows->aRow[i].cValues, PR_ATTACH_NUM);
		if (!lpAttachNum) {
			bPartial = true;
			goto next_attach;
		}

		hr = lpSrc->OpenAttach(lpAttachNum->Value.ul, NULL, 0, &lpSrcAttach);
		if (hr != hrSuccess) {
			bPartial = true;
			goto next_attach;
		}

		hr = lpDest->CreateAttach(NULL, 0, &ulAttachNr, &lpDestAttach);
		if (hr != hrSuccess) {
			bPartial = true;
			goto next_attach;
		}

		hr = CopyAttachmentProps(lpSrcAttach, lpDestAttach);
		if (hr != hrSuccess) {
			bPartial = true;
			goto next_attach;
		}

		/*
		 * Try making a single instance copy (without sending the attachment data to server).
		 * No error checking, we do not care if this fails, we still have all the data.
		 */
		CopyInstanceIds(lpSrcAttach, lpDestAttach);

		hr = lpDestAttach->SaveChanges(0);
		if (hr != hrSuccess)
			goto exit;

next_attach:
		if (lpSrcAttach) {
			lpSrcAttach->Release();
			lpSrcAttach = NULL;
		}

		if (lpDestAttach) {
			lpDestAttach->Release();
			lpDestAttach = NULL;
		}
	}

	if (bPartial)
		hr = MAPI_W_PARTIAL_COMPLETION;

exit:
	if (lpHasAttach)
		MAPIFreeBuffer(lpHasAttach);

	if (lpTableColumns)
		MAPIFreeBuffer(lpTableColumns);

	if (lpRows)
		FreeProws(lpRows);

	if (lpTable)
		lpTable->Release();

	return hr;
}

/**
 * Copies all folders and contents from lpSrc to lpDest folder.
 *
 * Recursively copies contents from one folder to another. Location of
 * lpSrc and lpDest does not matter, as long as there is read rights
 * in lpSrc and write rights in lpDest.
 *
 * @param[in]	lpSrc	Source folder to copy
 * @param[in]	lpDest	Source folder to copy
 * @param[in]	ulFlags	See ISupport::DoCopyTo for valid flags
 * @param[in]	ulUIParam	Unused in Linux.
 * @param[in]	lpProgress	IMAPIProgress object. Unused in Linux.
 *
 * @return MAPI error code.
 */
HRESULT Util::CopyHierarchy(LPMAPIFOLDER lpSrc, LPMAPIFOLDER lpDest, ULONG ulFlags, ULONG ulUIParam, LPMAPIPROGRESS lpProgress) {
	HRESULT hr;
	bool bPartial = false;
	LPMAPITABLE lpTable = NULL;
	LPSRowSet lpRowSet = NULL;
	SizedSPropTagArray(2, sptaName) = { 2, { PR_DISPLAY_NAME_W, PR_ENTRYID } };
	LPMAPIFOLDER lpSrcFolder = NULL, lpDestFolder = NULL;
	ULONG ulObj;
	LPMAPIFOLDER lpSrcParam = NULL;
	LPMAPIFOLDER lpDestParam = NULL;

	// sanity checks
	if (!lpSrc || !lpDest) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	hr = lpSrc->QueryInterface(IID_IMAPIFolder, (void**)&lpSrcParam);
	if (hr != hrSuccess)
		goto exit;

	hr = lpDest->QueryInterface(IID_IMAPIFolder, (void**)&lpDestParam);
	if (hr != hrSuccess)
		goto exit;


	hr = lpSrc->GetHierarchyTable(MAPI_UNICODE, &lpTable);
	if (hr != hrSuccess)
		goto exit;

	hr = lpTable->SetColumns((LPSPropTagArray)&sptaName, 0);
	if (hr != hrSuccess)
		goto exit;

	while (true) {
		hr = lpTable->QueryRows(1, 0, &lpRowSet);
		if (hr != hrSuccess)
			goto exit;

		if (lpRowSet->cRows == 0)
			break;

		hr = lpSrc->OpenEntry(lpRowSet->aRow[0].lpProps[1].Value.bin.cb, (LPENTRYID)lpRowSet->aRow[0].lpProps[1].Value.bin.lpb, &IID_IMAPIFolder, 0, &ulObj, (LPUNKNOWN*)&lpSrcFolder);
		if (hr != hrSuccess) {
			bPartial = true;
			goto next_folder;
		}

		hr = lpDest->CreateFolder(FOLDER_GENERIC, (LPTSTR)lpRowSet->aRow[0].lpProps[0].Value.lpszW, NULL, &IID_IMAPIFolder,
								  MAPI_UNICODE | (ulFlags & MAPI_NOREPLACE ? 0 : OPEN_IF_EXISTS), &lpDestFolder);
		if (hr != hrSuccess) {
			bPartial = true;
			goto next_folder;
		}

		hr = Util::DoCopyTo(&IID_IMAPIFolder, lpSrcFolder, 0, NULL, NULL, ulUIParam, lpProgress, &IID_IMAPIFolder, lpDestFolder, ulFlags, NULL);
		if (FAILED(hr))
			goto exit;
		else if (hr != hrSuccess) {
			bPartial = true;
			goto next_folder;
		}

		if (ulFlags & MAPI_MOVE)
			lpSrc->DeleteFolder(lpRowSet->aRow[0].lpProps[1].Value.bin.cb, (LPENTRYID)lpRowSet->aRow[0].lpProps[1].Value.bin.lpb, 0, NULL, 0);

next_folder:
		if (lpRowSet) {
			FreeProws(lpRowSet);
			lpRowSet = NULL;
		}

		if (lpSrcFolder) {
			lpSrcFolder->Release();
			lpSrcFolder = NULL;
		}

		if (lpDestFolder) {
			lpDestFolder->Release();
			lpDestFolder = NULL;
		}
	}

	if (bPartial)
		hr = MAPI_W_PARTIAL_COMPLETION;

exit:
	if (lpDestParam)
		lpDestParam->Release();

	if (lpSrcParam)
		lpSrcParam->Release();

	if (lpRowSet)
		FreeProws(lpRowSet);

	if (lpSrcFolder)
		lpSrcFolder->Release();

	if (lpDestFolder)
		lpDestFolder->Release();

	if (lpTable)
		lpTable->Release();

	return hr;
}

/** 
 * Copy all messages from a folder to another.
 * 
 * @param[in] ulWhat 0 for normal messages, MAPI_ASSOCIATED for associated messages
 * @param[in] lpSrc The source folder to copy messages from
 * @param[out] lpDest The destination folder to copy messages in
 * @param[in] ulFlags CopyTo flags, like MAPI_MOVE, or 0
 * @param[in] ulUIParam Unused parameter passed to CopyTo functions
 * @param[in] lpProgress Unused progress object
 * 
 * @return MAPI error code
 */
#define MAX_ROWS 50
HRESULT Util::CopyContents(ULONG ulWhat, LPMAPIFOLDER lpSrc, LPMAPIFOLDER lpDest, ULONG ulFlags, ULONG ulUIParam, LPMAPIPROGRESS lpProgress) {
	HRESULT hr;
	bool bPartial = false;
	LPMAPITABLE lpTable = NULL;
	LPSRowSet lpRowSet = NULL;
	SizedSPropTagArray(1, sptaEntryID) = { 1, { PR_ENTRYID } };
	ULONG ulObj;
	LPMESSAGE lpSrcMessage = NULL, lpDestMessage = NULL;
	LPENTRYLIST lpDeleteEntries = NULL;


	hr = lpSrc->GetContentsTable(MAPI_UNICODE | ulWhat, &lpTable);
	if (hr != hrSuccess)
		goto exit;

	hr = lpTable->SetColumns((LPSPropTagArray)&sptaEntryID, 0);
	if (hr != hrSuccess)
		goto exit;

	hr = MAPIAllocateBuffer(sizeof(ENTRYLIST), (void**)&lpDeleteEntries);
	if (hr != hrSuccess)
		goto exit;

	hr = MAPIAllocateMore(sizeof(SBinary)*MAX_ROWS, lpDeleteEntries, (void**)&lpDeleteEntries->lpbin);
	if (hr != hrSuccess)
		goto exit;

	while (true) {
		hr = lpTable->QueryRows(MAX_ROWS, 0, &lpRowSet);
		if (hr != hrSuccess)
			goto exit;

		if (lpRowSet->cRows == 0)
			break;

		lpDeleteEntries->cValues = 0;

		for (ULONG i = 0; i < lpRowSet->cRows; i++) {
			hr = lpSrc->OpenEntry(lpRowSet->aRow[i].lpProps[0].Value.bin.cb, (LPENTRYID)lpRowSet->aRow[i].lpProps[0].Value.bin.lpb, &IID_IMessage, 0, &ulObj, (LPUNKNOWN*)&lpSrcMessage);
			if (hr != hrSuccess) {
				bPartial = true;
				goto next_item;
			}

			hr = lpDest->CreateMessage(&IID_IMessage, ulWhat | MAPI_MODIFY, &lpDestMessage);
			if (hr != hrSuccess) {
				bPartial = true;
				goto next_item;
			}

			hr = Util::DoCopyTo(&IID_IMessage, lpSrcMessage, 0, NULL, NULL, ulUIParam, lpProgress, &IID_IMessage, lpDestMessage, ulFlags, NULL);
			if (FAILED(hr))
				goto exit;
			else if (hr != hrSuccess) {
				bPartial = true;
				goto next_item;
			}

			hr = lpDestMessage->SaveChanges(0);
			if (hr != hrSuccess) {
				bPartial = true;
			} else if (ulFlags & MAPI_MOVE) {
				lpDeleteEntries->lpbin[lpDeleteEntries->cValues].cb = lpRowSet->aRow[i].lpProps[0].Value.bin.cb;
				lpDeleteEntries->lpbin[lpDeleteEntries->cValues].lpb = lpRowSet->aRow[i].lpProps[0].Value.bin.lpb;
				lpDeleteEntries->cValues++;
			}
next_item:
			if (lpDestMessage) {
				lpDestMessage->Release();
				lpDestMessage = NULL;
			}

			if (lpSrcMessage) {
				lpSrcMessage->Release();
				lpSrcMessage = NULL;
			}
		}

		if ((ulFlags & MAPI_MOVE) && lpDeleteEntries->cValues > 0) {
			if (lpSrc->DeleteMessages(lpDeleteEntries, 0, NULL, 0) != hrSuccess)
				bPartial = true;
		}

		if (lpRowSet) {
			FreeProws(lpRowSet);
			lpRowSet = NULL;
		}
	}

	if (bPartial)
		hr = MAPI_W_PARTIAL_COMPLETION;

exit:
	if (lpDeleteEntries)
		MAPIFreeBuffer(lpDeleteEntries);

	if (lpDestMessage)
		lpDestMessage->Release();

	if (lpSrcMessage)
		lpSrcMessage->Release();

	if (lpRowSet)
		FreeProws(lpRowSet);

	if (lpTable)
		lpTable->Release();

	return hr;
}

/** 
 * Call OpenProperty on a property of an object to get the streamed
 * version of that property. Will try to open with STGM_TRANSACTED,
 * and disable this flag if an error was received.
 * 
 * @param[in] ulPropType The type of the property to open.
 * @param ulSrcPropTag The source property tag to open on the source object
 * @param lpPropSrc The source object containing the property to open
 * @param ulDestPropTag The destination property tag to open on the destination object
 * @param lpPropDest The destination object where the property should be copied to
 * @param lppSrcStream The source property as stream
 * @param lppDestStream The destination property as stream
 * 
 * @return MAPI error code
 */
HRESULT Util::TryOpenProperty(ULONG ulPropType, ULONG ulSrcPropTag, LPMAPIPROP lpPropSrc, ULONG ulDestPropTag, LPMAPIPROP lpPropDest, LPSTREAM *lppSrcStream, LPSTREAM *lppDestStream) {
	HRESULT hr;
	LPSTREAM lpSrc = NULL, lpDest = NULL;

	hr = lpPropSrc->OpenProperty(PROP_TAG(ulPropType, PROP_ID(ulSrcPropTag)), &IID_IStream, 0, 0, (LPUNKNOWN*)&lpSrc);
	if (hr != hrSuccess)
		goto exit;

	// some mapi functions/providers don't implement STGM_TRANSACTED, retry again without this flag
	hr = lpPropDest->OpenProperty(PROP_TAG(ulPropType, PROP_ID(ulDestPropTag)), &IID_IStream, STGM_WRITE | STGM_TRANSACTED, MAPI_CREATE | MAPI_MODIFY, (LPUNKNOWN*)&lpDest);
	if (hr != hrSuccess) {
		hr = lpPropDest->OpenProperty(PROP_TAG(ulPropType, PROP_ID(ulDestPropTag)), &IID_IStream, STGM_WRITE, MAPI_CREATE | MAPI_MODIFY, (LPUNKNOWN*)&lpDest);
	}
	if (hr != hrSuccess)
		goto exit;

	*lppSrcStream = lpSrc;
	*lppDestStream = lpDest;

exit:
	if (hr != hrSuccess) {
		if (lpSrc)
			lpSrc->Release();

		if (lpDest)
			lpDest->Release();
	}

	return hr;
}

/** 
 * Adds a SPropProblem structure to an SPropProblemArray. If the
 * problem array already contains data, it will first be copied to a
 * new array, and one problem will be appended.
 * 
 * @param[in] lpProblem The new problem to add to the array
 * @param[in,out] lppProblems *lppProblems is NULL for a new array, otherwise a copy plus the addition is returned
 * 
 * @return MAPI error code
 */
HRESULT Util::AddProblemToArray(LPSPropProblem lpProblem, LPSPropProblemArray *lppProblems) {
	HRESULT hr = hrSuccess;
	LPSPropProblemArray lpNewProblems = NULL;
	LPSPropProblemArray lpOrigProblems = *lppProblems;

	if (!lpOrigProblems) {
		hr = MAPIAllocateBuffer(CbNewSPropProblemArray(1), (void**)&lpNewProblems);
		if (hr != hrSuccess)
			goto exit;

		lpNewProblems->cProblem = 1;
	} else {
		hr = MAPIAllocateBuffer(CbNewSPropProblemArray(lpOrigProblems->cProblem+1), (void**)&lpNewProblems);
		if (hr != hrSuccess)
			goto exit;

		lpNewProblems->cProblem = lpOrigProblems->cProblem +1;
		memcpy(lpNewProblems->aProblem, lpOrigProblems->aProblem, sizeof(SPropProblem) * lpOrigProblems->cProblem);
		MAPIFreeBuffer(lpOrigProblems);
	}

	memcpy(&lpNewProblems->aProblem[lpNewProblems->cProblem -1], lpProblem, sizeof(SPropProblem));

	*lppProblems = lpNewProblems;

exit:
	return hr;
}

/** 
 * Copies a MAPI object in-memory to a new MAPI object. Only
 * IID_IStream or IID_IMAPIProp compatible interfaces can be copied.
 * 
 * @param[in] lpSrcInterface The expected interface of lpSrcObj. Cannot be NULL.
 * @param[in] lpSrcObj The source object to copy. Cannot be NULL.
 * @param[in] ciidExclude Number of interfaces in rgiidExclude
 * @param[in] rgiidExclude NULL or Interfaces to exclude in the copy, will return MAPI_E_INTERFACE_NOT_SUPPORTED if requested interface is found in the exclude list.
 * @param[in] lpExcludeProps NULL or Array of properties to exclude in the copy process
 * @param[in] ulUIParam Parameter for the callback in lpProgress, unused
 * @param[in] lpProgress Unused progress object
 * @param[in] lpDestInterface The expected interface of lpDstObj. Cannot be NULL.
 * @param[out] lpDestObj The existing destination object. Cannot be NULL.
 * @param[in] ulFlags can contain CopyTo flags, like MAPI_MOVE or MAPI_NOREPLACE
 * @param[in] lppProblems Optional array containing problems encountered during the copy.
 * 
 * @return MAPI error code
 */
HRESULT Util::DoCopyTo(LPCIID lpSrcInterface, LPVOID lpSrcObj, ULONG ciidExclude, LPCIID rgiidExclude,
					   LPSPropTagArray lpExcludeProps, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, LPCIID lpDestInterface,
					   LPVOID lpDestObj, ULONG ulFlags, LPSPropProblemArray * lppProblems)
{
	HRESULT hr = hrSuccess;
	LPUNKNOWN lpUnkSrc = (LPUNKNOWN)lpSrcObj, lpUnkDest = (LPUNKNOWN)lpDestObj;
	bool bPartial = false;
	// Properties that can never be copied (if you do this wrong, copying a message to a PST will fail)
	SizedSPropTagArray(23, sExtraExcludes) = { 19, { PR_STORE_ENTRYID, PR_STORE_RECORD_KEY, PR_STORE_SUPPORT_MASK, PR_MAPPING_SIGNATURE,
													 PR_MDB_PROVIDER, PR_ACCESS_LEVEL, PR_RECORD_KEY, PR_HASATTACH, PR_NORMALIZED_SUBJECT,
													 PR_MESSAGE_SIZE, PR_DISPLAY_TO, PR_DISPLAY_CC, PR_DISPLAY_BCC, PR_ACCESS, PR_SUBJECT_PREFIX,
													 PR_OBJECT_TYPE, PR_ENTRYID, PR_PARENT_ENTRYID, PR_INTERNET_CONTENT,
													 PR_NULL, PR_NULL, PR_NULL, PR_NULL }};

	LPMAPIPROP lpPropSrc = NULL, lpPropDest = NULL;
	LPSPropTagArray lpSPropTagArray = NULL;
	LPSPropValue lpProps = NULL;

	if (!lpSrcInterface || !lpSrcObj || !lpDestInterface || !lpDestObj) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	// source is "usually" the same as dest .. so we don't check (as ms mapi doesn't either)

	hr = FindInterface(lpSrcInterface, ciidExclude, rgiidExclude);
	if (hr == hrSuccess) {
		hr = MAPI_E_INTERFACE_NOT_SUPPORTED;
		goto exit;
	}

	hr = FindInterface(lpDestInterface, ciidExclude, rgiidExclude);
	if (hr == hrSuccess) {
		hr = MAPI_E_INTERFACE_NOT_SUPPORTED;
		goto exit;
	}

	// first test IID_IStream .. the rest is IID_IMAPIProp compatible
	if (*lpSrcInterface == IID_IStream) {
		hr = FindInterface(&IID_IStream, ciidExclude, rgiidExclude);
		if (hr == hrSuccess) {
			hr = MAPI_E_INTERFACE_NOT_SUPPORTED;
			goto exit;
		}
		if (*lpDestInterface != IID_IStream) {
			hr = MAPI_E_INTERFACE_NOT_SUPPORTED;
			goto exit;
		}
		hr = CopyStream((LPSTREAM)lpSrcObj, (LPSTREAM)lpDestObj);
		goto exit;
	}


	hr = FindInterface(&IID_IMAPIProp, ciidExclude, rgiidExclude);
	if (hr == hrSuccess) {
		hr = MAPI_E_INTERFACE_NOT_SUPPORTED;
		goto exit;
	}

	// end sanity checks


	// check message, folder, attach, recipients, stream, mapitable, ... ?

	if (*lpSrcInterface == IID_IMAPIFolder) {

		// MS MAPI does not perform this check
		if (*lpDestInterface != IID_IMAPIFolder) {
			// on store, create folder and still go ?
			hr = MAPI_E_INTERFACE_NOT_SUPPORTED;
			goto exit;
		}

		if (!lpExcludeProps || Util::FindPropInArray(lpExcludeProps, PR_CONTAINER_CONTENTS) == -1) {
			sExtraExcludes.aulPropTag[sExtraExcludes.cValues++] = PR_CONTAINER_CONTENTS;
			hr = CopyContents(0, (LPMAPIFOLDER)lpSrcObj, (LPMAPIFOLDER)lpDestObj, ulFlags, ulUIParam, lpProgress);
			if (hr != hrSuccess)
				bPartial = true;
		}

		if (!lpExcludeProps || Util::FindPropInArray(lpExcludeProps, PR_FOLDER_ASSOCIATED_CONTENTS) == -1) {
			sExtraExcludes.aulPropTag[sExtraExcludes.cValues++] = PR_FOLDER_ASSOCIATED_CONTENTS;
			hr = CopyContents(MAPI_ASSOCIATED, (LPMAPIFOLDER)lpSrcObj, (LPMAPIFOLDER)lpDestObj, ulFlags, ulUIParam, lpProgress);
			if (hr != hrSuccess)
				bPartial = true;
		}

		if (!lpExcludeProps || Util::FindPropInArray(lpExcludeProps, PR_CONTAINER_HIERARCHY) == -1) {
			// add to lpExcludeProps so CopyProps ignores them
			sExtraExcludes.aulPropTag[sExtraExcludes.cValues++] = PR_CONTAINER_HIERARCHY;
			hr = CopyHierarchy((LPMAPIFOLDER)lpSrcObj, (LPMAPIFOLDER)lpDestObj, ulFlags, ulUIParam, lpProgress);
			if (hr != hrSuccess)
				bPartial = true;
		}


	} else if (*lpSrcInterface == IID_IMessage) {
		// recipients & attachments
		// this is done in CopyProps ()
	} else if (*lpSrcInterface == IID_IAttachment) {
		// data stream
		// this is done in CopyProps ()
	} else if (*lpSrcInterface == IID_IMAPIContainer || *lpSrcInterface == IID_IMAPIProp) {
		// props only
		// this is done in CopyProps ()
	} else if (*lpSrcInterface == IID_IMailUser || *lpSrcInterface == IID_IDistList) {
		// in one if() ?
		// this is done in CopyProps () ???
		// what else besides props ???
	} else {
		// stores, ... ?
		hr = MAPI_E_INTERFACE_NOT_SUPPORTED;
		goto exit;
	}

	// we have a IMAPIProp compatible interface here, and we don't want to crash
	hr = QueryInterfaceMapiPropOrValidFallback(lpUnkSrc, lpSrcInterface, (IUnknown**)&lpPropSrc);
	if (hr != hrSuccess)
		goto exit;

	hr = QueryInterfaceMapiPropOrValidFallback(lpUnkDest, lpDestInterface, (IUnknown**)&lpPropDest);
	if (hr != hrSuccess)
		goto exit;

	if(!FHasHTML(lpPropDest)) {
		sExtraExcludes.aulPropTag[sExtraExcludes.cValues++] = PR_HTML;
	}

	hr = lpPropSrc->GetPropList(MAPI_UNICODE, &lpSPropTagArray);
	if (FAILED(hr))
		goto exit;

	// filter excludes
	if (lpExcludeProps || sExtraExcludes.cValues != 0) {
		for (ULONG i = 0; i < lpSPropTagArray->cValues; i++) {
			if (lpExcludeProps && Util::FindPropInArray(lpExcludeProps, CHANGE_PROP_TYPE(lpSPropTagArray->aulPropTag[i], PT_UNSPECIFIED)) != -1)
				lpSPropTagArray->aulPropTag[i] = PR_NULL;
			else if (Util::FindPropInArray((LPSPropTagArray)&sExtraExcludes, CHANGE_PROP_TYPE(lpSPropTagArray->aulPropTag[i], PT_UNSPECIFIED)) != -1)
				lpSPropTagArray->aulPropTag[i] = PR_NULL;
		}
	}
	
	// Force some extra properties
	if (*lpSrcInterface == IID_IMessage) {
		bool bAddAttach = false;
		bool bAddRecip = false;

		if (Util::FindPropInArray(lpExcludeProps, PR_MESSAGE_ATTACHMENTS) == -1 &&	// not in exclude
			Util::FindPropInArray(lpSPropTagArray, PR_MESSAGE_ATTACHMENTS) == -1)		// not yet in props to copy
			bAddAttach = true;

		if (Util::FindPropInArray(lpExcludeProps, PR_MESSAGE_RECIPIENTS) == -1 &&		// not in exclude
			Util::FindPropInArray(lpSPropTagArray, PR_MESSAGE_RECIPIENTS) == -1)		// not yet in props to copy
			bAddRecip = true;

		if (bAddAttach || bAddRecip) {
			LPSPropTagArray lpTempSPropTagArray = NULL;
			ULONG ulNewPropCount = lpSPropTagArray->cValues + (bAddAttach ? (bAddRecip ? 2 : 1) : 1);
			
			hr = MAPIAllocateBuffer(CbNewSPropTagArray(ulNewPropCount), (LPVOID*)&lpTempSPropTagArray);
			if (hr != hrSuccess)
				goto exit;

			memcpy(lpTempSPropTagArray->aulPropTag, lpSPropTagArray->aulPropTag, lpSPropTagArray->cValues * sizeof *lpSPropTagArray->aulPropTag);

			if (bAddAttach)
				lpTempSPropTagArray->aulPropTag[ulNewPropCount - (bAddRecip ? 2 : 1)] = PR_MESSAGE_ATTACHMENTS;
			if (bAddRecip)
				lpTempSPropTagArray->aulPropTag[ulNewPropCount - 1] = PR_MESSAGE_RECIPIENTS;
			lpTempSPropTagArray->cValues = ulNewPropCount;

			std::swap(lpTempSPropTagArray, lpSPropTagArray);
			MAPIFreeBuffer(lpTempSPropTagArray);
		}
	}

	// this is input for CopyProps
	hr = Util::DoCopyProps(lpSrcInterface, lpSrcObj, lpSPropTagArray, ulUIParam, lpProgress, lpDestInterface, lpDestObj, 0, lppProblems);
	if (hr != hrSuccess)
		goto exit;

	// TODO: mapi move, delete OPEN message ???

exit:
	// Partial warning when data was copied.
	if (bPartial)
		hr = MAPI_W_PARTIAL_COMPLETION;

	if (lpProps)
		MAPIFreeBuffer(lpProps);

	if (lpSPropTagArray)
		MAPIFreeBuffer(lpSPropTagArray);

	if (lpPropSrc)
		lpPropSrc->Release();

	if (lpPropDest)
		lpPropDest->Release();

	return hr;
}

/**
 * Check if the interface is an validate IMAPIProp interface
 *
 * @param[in] lpInterface Pointer to an interface GUID
 *
 * @retval MAPI_E_INTERFACE_NOT_SUPPORTED Interface not supported
 * @retval S_OK Interface supported
 */
HRESULT Util::ValidMapiPropInterface(LPCIID lpInterface)
{
	HRESULT hr = MAPI_E_INTERFACE_NOT_SUPPORTED;
	
	if (!lpInterface)
		goto exit;

	if (*lpInterface == IID_IAttachment ||
		*lpInterface == IID_IMAPIProp ||
		*lpInterface == IID_IProfSect ||
		*lpInterface == IID_IMsgStore ||
		*lpInterface == IID_IMessage ||
		*lpInterface == IID_IAddrBook ||
		*lpInterface == IID_IMailUser ||
		*lpInterface == IID_IMAPIContainer ||
		*lpInterface == IID_IMAPIFolder ||
		*lpInterface == IID_IABContainer ||
		*lpInterface == IID_IDistList)
	{
		hr = S_OK;
	}

exit:
	return hr;
}

/**
 * Queryinterface IMAPIProp or a supported fallback interface
 *
 * @param[in] lpInObj		Pointer to an IUnknown supported interface
 * @param[in] lpInterface	Pointer to an interface GUID
 * @param[out] lppOutObj	Pointer to a pointer which support a IMAPIProp interface.
 *
 * @retval MAPI_E_INTERFACE_NOT_SUPPORTED Interface not supported
 * @retval S_OK Interface supported
 */
HRESULT Util::QueryInterfaceMapiPropOrValidFallback(LPUNKNOWN lpInObj, LPCIID lpInterface, LPUNKNOWN *lppOutObj)
{
	HRESULT hr = hrSuccess;

	if (!lpInObj || !lppOutObj) {
		hr = MAPI_E_INTERFACE_NOT_SUPPORTED;
		goto exit;
	}

	hr = lpInObj->QueryInterface(IID_IMAPIProp, (void**)lppOutObj);
	if (hr == hrSuccess)
		goto exit;

	hr = ValidMapiPropInterface(lpInterface);
	if (hr != hrSuccess)
		goto exit;

	hr = lpInObj->QueryInterface(*lpInterface, (void**)lppOutObj);
	if (hr != hrSuccess)
		goto exit;

exit:
	return hr;
}

/** 
 * Copy properties of one MAPI object to another. Only IID_IMAPIProp
 * compatible objects are supported.
 * 
 * @param[in] lpSrcInterface The expected interface of lpSrcObj. Cannot be NULL.
 * @param[in] lpSrcObj The source object to copy. Cannot be NULL.
 * @param[in] lpIncludeProps List of properties to copy, or NULL for all properties.
 * @param[in] ulUIParam Parameter for the callback in lpProgress, unused
 * @param[in] lpProgress Unused progress object
 * @param[in] lpDestInterface The expected interface of lpDstObj. Cannot be NULL.
 * @param[out] lpDestObj The existing destination object. Cannot be NULL.
 * @param[in] ulFlags can contain CopyTo flags, like MAPI_MOVE or MAPI_NOREPLACE
 * @param[in] lppProblems Optional array containing problems encountered during the copy.
 * 
 * @return MAPI error code
 */
HRESULT Util::DoCopyProps(LPCIID lpSrcInterface, LPVOID lpSrcObj, LPSPropTagArray lpIncludeProps, ULONG ulUIParam,
						  LPMAPIPROGRESS lpProgress, LPCIID lpDestInterface, LPVOID lpDestObj, ULONG ulFlags,
						  LPSPropProblemArray * lppProblems)
{
	HRESULT hr = hrSuccess;
	LPUNKNOWN lpUnkSrc = (LPUNKNOWN)lpSrcObj, lpUnkDest = (LPUNKNOWN)lpDestObj;
	IECUnknown* lpZarafa = NULL;
	LPSPropValue lpZObj = NULL;
	bool bPartial = false;

	LPMAPIPROP lpSrcProp = NULL, lpDestProp = NULL;
	LPSPropValue lpProps = NULL;
	ULONG cValues = 0;
	LPSPropTagArray lpsDestPropArray = NULL;
	LPSPropProblemArray lpProblems = NULL;

	// named props
	ULONG cNames = 0;
	LPSPropTagArray lpsSrcNameTagArray = NULL, lpsDestNameTagArray = NULL;
	LPMAPINAMEID *lppNames = NULL;
	LPSPropTagArray lpsDestTagArray = NULL;

	// attachments
	LPSPropValue lpAttachMethod = NULL;
	LPSTREAM lpSrcStream = NULL, lpDestStream = NULL;
	LPMESSAGE lpSrcMessage = NULL, lpDestMessage = NULL;

	LONG ulIdRTF;
	LONG ulIdHTML;
	LONG ulIdBODY;
	ULONG ulBodyProp = PR_BODY;

	if (!lpSrcInterface || !lpDestInterface || !lpSrcObj || !lpDestObj || !lpIncludeProps) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	// q-i src and dest to check if IID_IMAPIProp is present
	hr = QueryInterfaceMapiPropOrValidFallback(lpUnkSrc, lpSrcInterface, (IUnknown**)&lpSrcProp);
	if (hr != hrSuccess)
		goto exit;

	hr = QueryInterfaceMapiPropOrValidFallback(lpUnkDest, lpDestInterface, (IUnknown**)&lpDestProp);
	if (hr != hrSuccess)
		goto exit;

	// take some shortcuts if we're dealing with a Zarafa message destination
	if (HrGetOneProp(lpDestProp, PR_EC_OBJECT, &lpZObj) == hrSuccess) {
		if (lpZObj->Value.lpszA != NULL)
			((IECUnknown*)lpZObj->Value.lpszA)->QueryInterface(IID_ECMessage, (void**)&lpZarafa);
	}

	if (ulFlags & MAPI_NOREPLACE) {
		hr = lpDestProp->GetPropList(MAPI_UNICODE, &lpsDestPropArray);
		if (hr != hrSuccess)
			goto exit;

		for (ULONG i = 0; i < lpIncludeProps->cValues; i++) {
			if (Util::FindPropInArray(lpsDestPropArray, lpIncludeProps->aulPropTag[i]) != -1) {
				// hr = MAPI_E_COLLISION;
				// goto exit;
				// MSDN says collision, MS MAPI ignores these properties.
				lpIncludeProps->aulPropTag[i] = PR_NULL;
			}
		}
	}

	if (lpZarafa) {
		// Use only one body text property, RTF, HTML or BODY when we're copying to another Zarafa message.
		ulIdRTF = Util::FindPropInArray(lpIncludeProps, PR_RTF_COMPRESSED);
		ulIdHTML = Util::FindPropInArray(lpIncludeProps, PR_HTML);
		ulIdBODY = Util::FindPropInArray(lpIncludeProps, PR_BODY_W);

		// find out the original body type, and only copy that version
		ulBodyProp = GetBestBody(lpSrcProp, fMapiUnicode);

		if (ulBodyProp == PR_BODY && ulIdBODY != -1) {
			// discard html and rtf
			if(ulIdHTML != -1)
				lpIncludeProps->aulPropTag[ulIdHTML] = PR_NULL;
			if(ulIdRTF != -1)
				lpIncludeProps->aulPropTag[ulIdRTF] = PR_NULL;
		} else if (ulBodyProp == PR_HTML && ulIdHTML != -1) {
			// discard plain and rtf
			if(ulIdBODY != -1)
				lpIncludeProps->aulPropTag[ulIdBODY] = PR_NULL;
			if(ulIdRTF != -1)
				lpIncludeProps->aulPropTag[ulIdRTF] = PR_NULL;
		} else if (ulBodyProp == PR_RTF_COMPRESSED && ulIdRTF != -1) {
			// discard plain and html
			if(ulIdHTML != -1)
				lpIncludeProps->aulPropTag[ulIdHTML] = PR_NULL;
			if(ulIdBODY != -1)
				lpIncludeProps->aulPropTag[ulIdBODY] = PR_NULL;
		}
	}

	for (ULONG i = 0; i < lpIncludeProps->cValues; i++) {
		bool isProblem = false;

		// TODO: ?
		// for all PT_OBJECT properties on IMAPIProp, MS MAPI tries:
		// IID_IMessage, IID_IStreamDocfile, IID_IStorage

		if (PROP_TYPE(lpIncludeProps->aulPropTag[i]) == PT_OBJECT ||
			PROP_ID(lpIncludeProps->aulPropTag[i]) == PROP_ID(PR_ATTACH_DATA_BIN)) {
			// if IMessage: PR_MESSAGE_RECIPIENTS, PR_MESSAGE_ATTACHMENTS
			if (*lpSrcInterface == IID_IMessage) {

				if (lpIncludeProps->aulPropTag[i] == PR_MESSAGE_RECIPIENTS) {
					// TODO: add ulFlags, and check for MAPI_NOREPLACE
					hr = Util::CopyRecipients((LPMESSAGE)lpSrcObj, (LPMESSAGE)lpDestObj);
				} else if (lpIncludeProps->aulPropTag[i] == PR_MESSAGE_ATTACHMENTS) {
					// TODO: add ulFlags, and check for MAPI_NOREPLACE
					hr = Util::CopyAttachments((LPMESSAGE)lpSrcObj, (LPMESSAGE)lpDestObj);
				} else {
					hr = MAPI_E_INTERFACE_NOT_SUPPORTED;
				}
				if (hr != hrSuccess) {
					isProblem = true;
					goto next_include_check;
				}

			} else if (*lpSrcInterface == IID_IMAPIFolder) {
				// MS MAPI skips these in CopyProps(), for unknown reasons
				if (lpIncludeProps->aulPropTag[i] == PR_CONTAINER_CONTENTS ||
					lpIncludeProps->aulPropTag[i] == PR_CONTAINER_HIERARCHY ||
					lpIncludeProps->aulPropTag[i] == PR_FOLDER_ASSOCIATED_CONTENTS) {
					lpIncludeProps->aulPropTag[i] = PR_NULL;
				} else {
					isProblem = true;
				}
			} else if (*lpSrcInterface == IID_IAttachment) {
				// In attachments, IID_IMessage can be present!  for PR_ATTACH_DATA_OBJ
				// find method and copy this PT_OBJECT
				hr = HrGetOneProp(lpSrcProp, PR_ATTACH_METHOD, &lpAttachMethod);
				if (hr != hrSuccess) {
					isProblem = true;
					goto next_include_check;
				}

				switch(lpAttachMethod->Value.ul) {
				case ATTACH_BY_VALUE:
				case ATTACH_OLE:
					// stream

					// Not being able to open the source message is not an error: it may just not be there
					if(((LPATTACH)lpSrcObj)->OpenProperty(PR_ATTACH_DATA_BIN, &IID_IStream, 0, 0, (LPUNKNOWN *)&lpSrcStream) == hrSuccess) {
						// While dragging and dropping, Outlook 2007 (atleast) returns an internal MAPI object to CopyTo as destination
						// The internal MAPI object is unable to make a stream STGM_TRANSACTED, so we retry the action without that flag
						// to get the stream without the transaction feature.
						hr = ((LPATTACH)lpDestObj)->OpenProperty(PR_ATTACH_DATA_BIN, &IID_IStream, STGM_WRITE | STGM_TRANSACTED, MAPI_CREATE | MAPI_MODIFY, (LPUNKNOWN *)&lpDestStream);
						if (hr != hrSuccess)
							hr = ((LPATTACH)lpDestObj)->OpenProperty(PR_ATTACH_DATA_BIN, &IID_IStream, STGM_WRITE, MAPI_CREATE | MAPI_MODIFY, (LPUNKNOWN *)&lpDestStream);
						if (hr != hrSuccess) {
							isProblem = true;
							goto next_include_check;
						}

						hr = Util::CopyStream(lpSrcStream, lpDestStream);
						if (hr != hrSuccess) {
							isProblem = true;
							goto next_include_check;
						}
					} else if(lpAttachMethod->Value.ul == ATTACH_OLE &&
						((LPATTACH)lpSrcObj)->OpenProperty(PR_ATTACH_DATA_OBJ, &IID_IStream, 0, 0, (LPUNKNOWN *)&lpSrcStream) == hrSuccess) {
						// OLE 2.0 must be open with PR_ATTACH_DATA_OBJ

						hr = ((LPATTACH)lpDestObj)->OpenProperty(PR_ATTACH_DATA_OBJ, &IID_IStream, STGM_WRITE | STGM_TRANSACTED, MAPI_CREATE | MAPI_MODIFY, (LPUNKNOWN *)&lpDestStream);
						if (hr == E_FAIL)
							hr = ((LPATTACH)lpDestObj)->OpenProperty(PR_ATTACH_DATA_OBJ, &IID_IStream, STGM_WRITE, MAPI_CREATE | MAPI_MODIFY, (LPUNKNOWN *)&lpDestStream);
						if (hr != hrSuccess) {
							isProblem = true;
							goto next_include_check;
						}

						hr = Util::CopyStream(lpSrcStream, lpDestStream);
						if (hr != hrSuccess) {
							isProblem = true;
							goto next_include_check;
						}
					}

					break;
				case ATTACH_EMBEDDED_MSG:
					// message

					if(((LPATTACH)lpSrcObj)->OpenProperty(PR_ATTACH_DATA_OBJ, &IID_IMessage, 0, 0, (LPUNKNOWN *)&lpSrcMessage) == hrSuccess) {
						// Not being able to open the source message is not an error: it may just not be there
						hr = ((LPATTACH)lpDestObj)->OpenProperty(PR_ATTACH_DATA_OBJ, &IID_IMessage, 0, MAPI_CREATE | MAPI_MODIFY, (LPUNKNOWN *)&lpDestMessage);
						if (hr != hrSuccess) {
							isProblem = true;
							goto next_include_check;
						}

						hr = Util::DoCopyTo(&IID_IMessage, lpSrcMessage, 0, NULL, NULL, ulUIParam, lpProgress, &IID_IMessage, lpDestMessage, 0, NULL);
						if (hr != hrSuccess) {
							isProblem = true;
							goto next_include_check;
						}

						hr = lpDestMessage->SaveChanges(0);
						if (hr != hrSuccess) {
							isProblem = true;
							goto next_include_check;
						}
					}

					break;
				default:
					// OLE objects?
					isProblem = true;
					break;
				};
			} else {
				isProblem = true;
			}

			// TODO: try the 3 MSMAPI interfaces (message, stream, storage) if unhandled?

next_include_check:

			if (isProblem) {
				SPropProblem sProblem;

				bPartial = true;

				sProblem.ulIndex = i;
				sProblem.ulPropTag = lpIncludeProps->aulPropTag[i];
				sProblem.scode = MAPI_E_INTERFACE_NOT_SUPPORTED; // hr?

				hr = AddProblemToArray(&sProblem, &lpProblems);
				if (hr != hrSuccess)
					goto exit;
			}

			if (lpAttachMethod) {
				MAPIFreeBuffer(lpAttachMethod);
				lpAttachMethod = NULL;
			}

			if (lpDestStream) {
				lpDestStream->Release();
				lpDestStream = NULL;
			}

			if (lpSrcStream) {
				lpSrcStream->Release();
				lpSrcStream = NULL;
			}

			if (lpSrcMessage) {
				lpSrcMessage->Release();
				lpSrcMessage = NULL;
			}

			if (lpDestMessage) {
				lpDestMessage->Release();
				lpDestMessage = NULL;
			}

			// skip this prop for the final SetProps()
			lpIncludeProps->aulPropTag[i] = PR_NULL;
		}
	}


	hr = lpSrcProp->GetProps(lpIncludeProps, 0, &cValues, &lpProps);
	if (FAILED(hr))
		goto exit;

	// make map for destination property tags, because named id's may differ in src and dst
	hr = MAPIAllocateBuffer(CbNewSPropTagArray(cValues), (void**)&lpsDestTagArray);
	if (hr != hrSuccess)
		goto exit;

	// get named props
	for (ULONG i = 0; i < cValues; i++) {
		lpsDestTagArray->aulPropTag[i] = lpProps[i].ulPropTag;

		if (PROP_ID(lpProps[i].ulPropTag) >= 0x8000)
			cNames++;
	}

	if (cNames) {
		hr = MAPIAllocateBuffer(CbNewSPropTagArray(cNames), (void**)&lpsSrcNameTagArray);
		if (hr != hrSuccess)
			goto exit;

		lpsSrcNameTagArray->cValues = cNames;
		cNames = 0;
		for (ULONG i = 0; i < cValues; i++) {
			if (PROP_ID(lpProps[i].ulPropTag) >= 0x8000)
				lpsSrcNameTagArray->aulPropTag[cNames++] = lpProps[i].ulPropTag;
		}

		// ignore warnings on unknown named properties, but don't copy those either (see PT_ERROR below)
		hr = lpSrcProp->GetNamesFromIDs(&lpsSrcNameTagArray, NULL, 0, &cNames, &lppNames);
		if (FAILED(hr))
			goto exit;

		hr = lpDestProp->GetIDsFromNames(cNames, lppNames, MAPI_CREATE, &lpsDestNameTagArray);
		if (FAILED(hr))
			goto exit;

		// make new lookup map for lpProps[] -> lpsDestNameTag[]
		for (ULONG i=0, j=0; i < cValues && j < cNames; i++) {
			if (PROP_ID(lpProps[i].ulPropTag) == PROP_ID(lpsSrcNameTagArray->aulPropTag[j])) {
				if (PROP_TYPE(lpsDestNameTagArray->aulPropTag[j]) != PT_ERROR) {
					// replace with new proptag, so we can open the correct property
					lpsDestTagArray->aulPropTag[i] = PROP_TAG(PROP_TYPE(lpProps[i].ulPropTag), PROP_ID(lpsDestNameTagArray->aulPropTag[j]));
				} else {
					// leave on PT_ERROR, so we don't copy the property
					lpsDestTagArray->aulPropTag[i] = PROP_TAG(PT_ERROR, PROP_ID(lpsDestNameTagArray->aulPropTag[j]));
					// don't even return a warning because although not all data could be copied
				}
				j++;
			}
		}
	}


	// find all MAPI_E_NOT_ENOUGH_MEMORY errors
	for (ULONG i = 0; i < cValues; i++) {
		if (PROP_TYPE(lpProps[i].ulPropTag) == PT_ERROR && (lpProps[i].Value.err == MAPI_E_NOT_ENOUGH_MEMORY || 
			(PROP_ID(lpProps[i].ulPropTag) == PROP_ID(PR_BODY) || 
			 PROP_ID(lpProps[i].ulPropTag) == PROP_ID(PR_HTML) ||
			 PROP_ID(lpProps[i].ulPropTag) == PROP_ID(PR_RTF_COMPRESSED)) ))
		{

			ASSERT(PROP_ID(lpIncludeProps->aulPropTag[i]) == PROP_ID(lpProps[i].ulPropTag));
			
			hr = Util::TryOpenProperty(PROP_TYPE(lpIncludeProps->aulPropTag[i]), lpProps[i].ulPropTag, lpSrcProp, lpsDestTagArray->aulPropTag[i], lpDestProp, &lpSrcStream, &lpDestStream);
			if (hr != hrSuccess) {
				// TODO: check, partial or problemarray?
				// when the prop was not found (body property), it actually wasn't present, so don't mark as partial
				if (hr != MAPI_E_NOT_FOUND)
					bPartial = true;
				goto next_stream_prop;
			}

			hr = Util::CopyStream(lpSrcStream, lpDestStream);
			if (hr != hrSuccess)
				bPartial = true;

next_stream_prop:
			if (lpSrcStream) {
				lpSrcStream->Release();
				lpSrcStream = NULL;
			}

			if (lpDestStream) {
				lpDestStream->Release();
				lpDestStream = NULL;
			}
		}
	}

	// set destination proptags in orignal properties
	for (ULONG i=0; i < cValues; i++) {
		lpProps[i].ulPropTag = lpsDestTagArray->aulPropTag[i];

		// Reset PT_ERROR properties because outlook xp pst doesn't support to set props this.
		if (PROP_TYPE(lpProps[i].ulPropTag) == PT_ERROR)
			lpProps[i].ulPropTag = PR_NULL;
	}

	hr = lpDestProp->SetProps(cValues, lpProps, NULL);
	if (FAILED(hr))
		goto exit;

	// TODO: test how this should work on CopyProps() !!
	if (ulFlags & MAPI_MOVE) {
		// TODO: add problem array
		hr = lpSrcProp->DeleteProps(lpIncludeProps, NULL);
		if (FAILED(hr))
			goto exit;
	}

exit:
	if (bPartial)
		hr = MAPI_W_PARTIAL_COMPLETION;

	if (hr != hrSuccess) {
		// may not return a problem set when we have an warning/error code in hr
		if (lpProblems)
			MAPIFreeBuffer(lpProblems);
	} else {
		if (lppProblems)
			*lppProblems = lpProblems;
		else if (lpProblems)
			MAPIFreeBuffer(lpProblems);
	}

	if (lppNames)
		MAPIFreeBuffer(lppNames);

	if (lpsSrcNameTagArray)
		MAPIFreeBuffer(lpsSrcNameTagArray);

	if (lpsDestNameTagArray)
		MAPIFreeBuffer(lpsDestNameTagArray);

	if (lpsDestTagArray)
		MAPIFreeBuffer(lpsDestTagArray);

	if (lpSrcMessage)
		lpSrcMessage->Release();

	if (lpDestMessage)
		lpDestMessage->Release();

	if (lpSrcStream)
		lpSrcStream->Release();

	if (lpDestStream)
		lpDestStream->Release();

	if (lpAttachMethod)
		MAPIFreeBuffer(lpAttachMethod);

	if (lpSrcProp)
		lpSrcProp->Release();

	if (lpDestProp)
		lpDestProp->Release();

	if (lpProps)
		MAPIFreeBuffer(lpProps);

	if (lpsDestPropArray)
		MAPIFreeBuffer(lpsDestPropArray);

	if (lpZarafa)
		lpZarafa->Release();

	if (lpZObj)
		MAPIFreeBuffer(lpZObj);

	return hr;
}

/** 
 * Copy the IMAP data properties if available, with single instance on
 * the IMAP Email.
 * 
 * @param[in] lpSrcMsg Copy IMAP data from this message
 * @param[in] lpDstMsg Copy IMAP data to this message
 * 
 * @return MAPI error code
 */
HRESULT Util::HrCopyIMAPData(LPMESSAGE lpSrcMsg, LPMESSAGE lpDstMsg)
{
	HRESULT hr = hrSuccess;
	LPSTREAM lpSrcStream = NULL;
	LPSTREAM lpDestStream = NULL;
	SizedSPropTagArray(3, sptaIMAP) = {
		3, { PR_EC_IMAP_EMAIL_SIZE,
			 PR_EC_IMAP_BODY,
			 PR_EC_IMAP_BODYSTRUCTURE
		}
	};
	ULONG cValues = 0;
	LPSPropValue lpIMAPProps = NULL;

	// special case: get PR_EC_IMAP_BODY if present, and copy with single instance
	// hidden property in zarafa, try to copy contents
	if (Util::TryOpenProperty(PT_BINARY, PR_EC_IMAP_EMAIL, lpSrcMsg, PR_EC_IMAP_EMAIL, lpDstMsg, &lpSrcStream, &lpDestStream) == hrSuccess) {
		if (Util::CopyStream(lpSrcStream, lpDestStream) == hrSuccess) {
			/*
			 * Try making a single instance copy for IMAP body data (without sending the data to server).
			 * No error checking, we do not care if this fails, we still have all the data.
			 */
			Util::CopyInstanceIds(lpSrcMsg, lpDstMsg);

			// Since we have a copy of the original email body, copy the other properties for IMAP too
			hr = lpSrcMsg->GetProps((LPSPropTagArray)&sptaIMAP, 0, &cValues, &lpIMAPProps);
			if (FAILED(hr))
				goto exit;

			hr = lpDstMsg->SetProps(cValues, lpIMAPProps, NULL);
			if (FAILED(hr))
				goto exit;

			hr = hrSuccess;
		}
	}

exit:
	if (lpDestStream)
		lpDestStream->Release();

	if (lpSrcStream)
		lpSrcStream->Release();

	if (lpIMAPProps)
		MAPIFreeBuffer(lpIMAPProps);

	return hr;
}

HRESULT Util::HrDeleteIMAPData(LPMESSAGE lpMsg)
{
	SizedSPropTagArray(4, sptaIMAP) = {
		4, { PR_EC_IMAP_EMAIL_SIZE,
			 PR_EC_IMAP_EMAIL,
			 PR_EC_IMAP_BODY,
			 PR_EC_IMAP_BODYSTRUCTURE
		}
	};

	return lpMsg->DeleteProps((SPropTagArray*)&sptaIMAP, NULL);
}

/** 
 * Get the quota status object for a store with given quota limits.
 * 
 * @param[in] lpMsgStore Store to get the quota for
 * @param[in] lpsQuota The (optional) quota limits to check
 * @param[out] lppsQuotaStatus Quota status struct
 * 
 * @return MAPI error code
 */
HRESULT Util::HrGetQuotaStatus(IMsgStore *lpMsgStore, LPECQUOTA lpsQuota, LPECQUOTASTATUS *lppsQuotaStatus)
{
	HRESULT			hr = hrSuccess;
	LPECQUOTASTATUS	lpsQuotaStatus = NULL;
	LPSPropValue 	lpProps = NULL;
    SizedSPropTagArray(1, sptaProps) = {1, {PR_MESSAGE_SIZE_EXTENDED}};
    ULONG 			cValues = 0;
	
	if (lpMsgStore == NULL || lppsQuotaStatus == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}
	
	hr = lpMsgStore->GetProps((LPSPropTagArray)&sptaProps, 0, &cValues, &lpProps);
	if (hr != hrSuccess)
		goto exit;
		
	if (cValues != 1 || lpProps[0].ulPropTag != PR_MESSAGE_SIZE_EXTENDED) {
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}
	
	hr = MAPIAllocateBuffer(sizeof *lpsQuotaStatus, (void**)&lpsQuotaStatus);
	if (hr != hrSuccess)
		goto exit;
	memset(lpsQuotaStatus, 0, sizeof *lpsQuotaStatus);
	
	lpsQuotaStatus->llStoreSize = lpProps[0].Value.li.QuadPart;
	lpsQuotaStatus->quotaStatus = QUOTA_OK;
	if (lpsQuota && lpsQuotaStatus->llStoreSize > 0) {
		if (lpsQuota->llHardSize > 0 && lpsQuotaStatus->llStoreSize > lpsQuota->llHardSize)
			lpsQuotaStatus->quotaStatus = QUOTA_HARDLIMIT;
		else if (lpsQuota->llSoftSize > 0 && lpsQuotaStatus->llStoreSize > lpsQuota->llSoftSize)
			lpsQuotaStatus->quotaStatus = QUOTA_SOFTLIMIT;
		else if (lpsQuota->llWarnSize > 0 && lpsQuotaStatus->llStoreSize > lpsQuota->llWarnSize)
			lpsQuotaStatus->quotaStatus = QUOTA_WARN;
	}
	
	*lppsQuotaStatus = lpsQuotaStatus;
	lpsQuotaStatus = NULL;
	
exit:
	if (lpsQuotaStatus)
		MAPIFreeBuffer(lpsQuotaStatus);
		
	if (lpProps)
		MAPIFreeBuffer(lpProps);

	return hr;
}

/** 
 * Removes properties from lpDestMsg, which do are not listed in
 * lpsValidProps.
 *
 * Named properties listed in lpsValidProps map to names in
 * lpSourceMsg. The corresponding property tags are checked in
 * lpDestMsg.
 * 
 * @param[out] lpDestMsg The message to delete properties from, which are found "invalid"
 * @param[in] lpSourceMsg The message for which named properties may be lookupped, listed in lpsValidProps
 * @param[in] lpsValidProps Properties which are valid in lpDestMsg. All others should be removed.
 * 
 * @return MAPI error code
 */
HRESULT Util::HrDeleteResidualProps(LPMESSAGE lpDestMsg, LPMESSAGE lpSourceMsg, LPSPropTagArray lpsValidProps)
{
	HRESULT			hr = hrSuccess;
	LPSPropTagArray	lpsPropArray = NULL;
	LPSPropTagArray	lpsNamedPropArray = NULL;
	LPSPropTagArray	lpsMappedPropArray = NULL;
	ULONG			cPropNames = 0;
	LPMAPINAMEID	*lppPropNames = NULL;
	PropTagSet		sPropTagSet;

	if (lpDestMsg == NULL || lpSourceMsg == NULL || lpsValidProps == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	hr = lpDestMsg->GetPropList(0, &lpsPropArray);
	if (hr != hrSuccess || lpsPropArray->cValues == 0)
		goto exit;

	hr = MAPIAllocateBuffer(CbNewSPropTagArray(lpsValidProps->cValues), (void**)&lpsNamedPropArray);
	if (hr != hrSuccess)
		goto exit;
	memset(lpsNamedPropArray, 0, CbNewSPropTagArray(lpsValidProps->cValues));

	for (unsigned i = 0; i < lpsValidProps->cValues; ++i)
		if (PROP_ID(lpsValidProps->aulPropTag[i]) >= 0x8000)
			lpsNamedPropArray->aulPropTag[lpsNamedPropArray->cValues++] = lpsValidProps->aulPropTag[i];

	if (lpsNamedPropArray->cValues > 0) {
		hr = lpSourceMsg->GetNamesFromIDs(&lpsNamedPropArray, NULL, 0, &cPropNames, &lppPropNames);
		if (FAILED(hr))
			goto exit;

		hr = lpDestMsg->GetIDsFromNames(cPropNames, lppPropNames, MAPI_CREATE, &lpsMappedPropArray);
		if (FAILED(hr))
			goto exit;
	}
	hr = hrSuccess;

	// Add the PropTags the message currently has
	for (unsigned i = 0; i < lpsPropArray->cValues; ++i)
		sPropTagSet.insert(lpsPropArray->aulPropTag[i]);

	// Remove the regular properties we want to keep
	for (unsigned i = 0; i < lpsValidProps->cValues; ++i)
		if (PROP_ID(lpsValidProps->aulPropTag[i]) < 0x8000)
			sPropTagSet.erase(lpsValidProps->aulPropTag[i]);

	// Remove the mapped named properties we want to keep. Filter failed named properties, so they will be removed
	for (unsigned i = 0; lpsMappedPropArray != NULL && i < lpsMappedPropArray->cValues; ++i)
		if (PROP_TYPE(lpsMappedPropArray->aulPropTag[i]) != PT_ERROR)
			sPropTagSet.erase(lpsMappedPropArray->aulPropTag[i]);

	if (sPropTagSet.empty())
		goto exit;

	// Reuse lpsPropArray to hold the properties we're going to delete
	ASSERT(lpsPropArray->cValues >= sPropTagSet.size());
	memset(lpsPropArray->aulPropTag, 0, lpsPropArray->cValues * sizeof *lpsPropArray->aulPropTag);
	lpsPropArray->cValues = 0;

	for (PropTagSet::const_iterator it = sPropTagSet.begin(); it != sPropTagSet.end(); ++it)
		lpsPropArray->aulPropTag[lpsPropArray->cValues++] = *it;

	hr = lpDestMsg->DeleteProps(lpsPropArray, NULL);
	if (hr != hrSuccess)
		goto exit;

	hr = lpDestMsg->SaveChanges(KEEP_OPEN_READWRITE);

exit:
	if (lpsMappedPropArray)
		MAPIFreeBuffer(lpsMappedPropArray);

	if (lppPropNames)
		MAPIFreeBuffer(lppPropNames);

	if (lpsNamedPropArray)
		MAPIFreeBuffer(lpsNamedPropArray);

	if (lpsPropArray)
		MAPIFreeBuffer(lpsPropArray);

	return hr;
}

/** 
 * Find an EntryID using exact binary matches in an array of
 * properties.
 * 
 * @param[in]  cbEID number of bytes in lpEID
 * @param[in]  lpEID the EntryID to find in the property array
 * @param[in]  cbEntryIDs number of properties in lpEntryIDs
 * @param[in]  lpEntryIDs array of entryid properties
 * @param[out] lpbFound TRUE if folder was found
 * @param[out] lpPos index number the folder was found in if *lpbFound is TRUE, otherwise untouched
 * 
 * @return MAPI error code
 * @retval MAPI_E_INVALID_PARAMETER a passed parameter was invalid
 */
HRESULT Util::HrFindEntryIDs(ULONG cbEID, LPENTRYID lpEID, ULONG cbEntryIDs, LPSPropValue lpEntryIDs, BOOL *lpbFound, ULONG* lpPos)
{
	HRESULT hr = hrSuccess;
	BOOL bFound = FALSE;
	ULONG i;

	if (cbEID == 0 || lpEID == NULL || cbEntryIDs == 0 || lpEntryIDs == NULL || lpbFound == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	for (i = 0; bFound == FALSE && i < cbEntryIDs; i++) {
		if (PROP_TYPE(lpEntryIDs[i].ulPropTag) != PT_BINARY)
			continue;
		if (cbEID != lpEntryIDs[i].Value.bin.cb)
			continue;
		if (memcmp(lpEID, lpEntryIDs[i].Value.bin.lpb, cbEID) == 0) {
			bFound = TRUE;
			break;
		}
	}

	*lpbFound = bFound;
	if (bFound && lpPos)
		*lpPos = i;

exit:
	return hr;
}

HRESULT Util::HrDeleteAttachments(LPMESSAGE lpMsg)
{
	HRESULT hr = hrSuccess;
	MAPITablePtr ptrAttachTable;
	SRowSetPtr ptrRows;

	SizedSPropTagArray(1, sptaAttachNum) = {1, {PR_ATTACH_NUM}};

	if (lpMsg == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	hr = lpMsg->GetAttachmentTable(0, &ptrAttachTable);
	if (hr != hrSuccess)
		goto exit;

	hr = HrQueryAllRows(ptrAttachTable, (LPSPropTagArray)&sptaAttachNum, NULL, NULL, 0, &ptrRows);
	if (hr != hrSuccess)
		goto exit;

	for (SRowSetPtr::size_type i = 0; i < ptrRows.size(); ++i) {
		hr = lpMsg->DeleteAttach(ptrRows[i].lpProps[0].Value.l, 0, NULL, 0);
		if (hr != hrSuccess)
			goto exit;
	}

exit:
	return hr;
}

HRESULT Util::HrDeleteRecipients(LPMESSAGE lpMsg)
{
	HRESULT hr = hrSuccess;
	MAPITablePtr ptrRecipTable;
	SRowSetPtr ptrRows;

	SizedSPropTagArray(1, sptaRowId) = {1, {PR_ROWID}};

	if (lpMsg == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	hr = lpMsg->GetRecipientTable(0, &ptrRecipTable);
	if (hr != hrSuccess)
		goto exit;

	hr = HrQueryAllRows(ptrRecipTable, (LPSPropTagArray)&sptaRowId, NULL, NULL, 0, &ptrRows);
	if (hr != hrSuccess)
		goto exit;

	hr = lpMsg->ModifyRecipients(MODRECIP_REMOVE, (LPADRLIST)ptrRows.get());
	if (hr != hrSuccess)
		goto exit;

exit:
	return hr;
}

HRESULT Util::HrDeleteMessage(IMAPISession *lpSession, IMessage *lpMessage)
{
	HRESULT hr = hrSuccess;
	ULONG cMsgProps;
	SPropArrayPtr ptrMsgProps;
	MsgStorePtr ptrStore;
	ULONG ulType;
	MAPIFolderPtr ptrFolder;
	ENTRYLIST entryList = {1, NULL};

	SizedSPropTagArray(3, sptaMessageProps) = {3, {PR_ENTRYID, PR_STORE_ENTRYID, PR_PARENT_ENTRYID}};
	enum {IDX_ENTRYID, IDX_STORE_ENTRYID, IDX_PARENT_ENTRYID};

	hr = lpMessage->GetProps((LPSPropTagArray)&sptaMessageProps, 0, &cMsgProps, &ptrMsgProps);
	if (hr != hrSuccess)
		goto exit;

	hr = lpSession->OpenMsgStore(0, ptrMsgProps[IDX_STORE_ENTRYID].Value.bin.cb, (LPENTRYID)ptrMsgProps[IDX_STORE_ENTRYID].Value.bin.lpb, &ptrStore.iid, MDB_WRITE, &ptrStore);
	if (hr != hrSuccess)
		goto exit;

	hr = ptrStore->OpenEntry(ptrMsgProps[IDX_PARENT_ENTRYID].Value.bin.cb, (LPENTRYID)ptrMsgProps[IDX_PARENT_ENTRYID].Value.bin.lpb, &ptrFolder.iid, MAPI_MODIFY, &ulType, &ptrFolder);
	if (hr != hrSuccess)
		goto exit;

	entryList.cValues = 1;
	entryList.lpbin = &ptrMsgProps[IDX_ENTRYID].Value.bin;

	hr = ptrFolder->DeleteMessages(&entryList, 0, NULL, DELETE_HARD_DELETE);

exit:
	return hr;
}

/**
 * Read a property via OpenProperty and put the output in std::string
 *
 * @param[in] lpProp Object to read from
 * @param[in] ulPropTag Proptag to open
 * @param[out] strData String to write to
 * @return result
 */
HRESULT Util::ReadProperty(IMAPIProp *lpProp, ULONG ulPropTag, std::string &strData)
{
	HRESULT hr = hrSuccess;
	IStream *lpStream = NULL;

	hr = lpProp->OpenProperty(ulPropTag, &IID_IStream, 0, 0, (IUnknown **)&lpStream);
	if(hr != hrSuccess)
		goto exit;
		
	hr = HrStreamToString(lpStream, strData);
	if(hr != hrSuccess)
		goto exit;
	
exit:
	if(lpStream)
		lpStream->Release();
		
	return hr;
}

/**
 * Write a property using OpenProperty()
 *
 * This function will open a stream to the given property and write all data from strData into
 * it usin STGM_DIRECT and MAPI_MODIFY | MAPI_CREATE. This means the existing data will be over-
 * written
 *
 * @param[in] lpProp Object to write to
 * @param[in] ulPropTag Property to write
 * @param[in] strData Data to write
 * @return result
 */
HRESULT Util::WriteProperty(IMAPIProp *lpProp, ULONG ulPropTag, const std::string &strData)
{
	HRESULT hr = hrSuccess;
	IStream *lpStream = NULL;
	ULONG len = 0;

	hr = lpProp->OpenProperty(ulPropTag, &IID_IStream, STGM_DIRECT, MAPI_CREATE | MAPI_MODIFY, (IUnknown **)&lpStream);
	if(hr != hrSuccess)
		goto exit;
		
	hr = lpStream->Write(strData.data(), strData.size(), &len);
	if(hr != hrSuccess)
		goto exit;
		
	hr = lpStream->Commit(0);
	if(hr != hrSuccess)
		goto exit;

exit:
	if(lpStream)
		lpStream->Release();
		
	return hr;
}
