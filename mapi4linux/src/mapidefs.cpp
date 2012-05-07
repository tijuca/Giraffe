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
#include "m4l.mapidefs.h"
#include "m4l.mapix.h"
#include "m4l.debug.h"
#include "m4l.mapiutil.h"

#include "ECDebug.h"
#include "Util.h"
#include "ECMemTable.h"
#include "charset/convert.h"
#include "ustringutil.h"

#include <mapi.h>
#include <mapicode.h>
#include <mapiguid.h>
#include <mapix.h>
#include <mapiutil.h>

#include "ECConfig.h"
#include "CommonUtil.h"

#include <set>

// ---
// IMAPIProp
// ---

M4LMAPIProp::M4LMAPIProp() {
}

M4LMAPIProp::~M4LMAPIProp() {
	list<LPSPropValue>::iterator i;
	for(i = properties.begin(); i != properties.end(); i++) {
		MAPIFreeBuffer(*i);
	}
	properties.clear();
}

HRESULT M4LMAPIProp::GetLastError(HRESULT hResult, ULONG ulFlags, LPMAPIERROR* lppMAPIError) {
	*lppMAPIError = NULL;
	return hrSuccess;
}

HRESULT M4LMAPIProp::SaveChanges(ULONG ulFlags) {
	// memory only.
	TRACE_MAPILIB(TRACE_ENTRY, "IMAPIProp::SaveChanges", "");
	TRACE_MAPILIB1(TRACE_RETURN, "IMAPIProp::SaveChanges", "0x%08x", hrSuccess);
	return hrSuccess;
}

HRESULT M4LMAPIProp::GetProps(LPSPropTagArray lpPropTagArray, ULONG ulFlags, ULONG* lpcValues, LPSPropValue* lppPropArray) {
	TRACE_MAPILIB2(TRACE_ENTRY, "IMAPIProp::GetProps", "PropTagArray=%s\nfFlags=0x%08X", PropNameFromPropTagArray(lpPropTagArray).c_str(), ulFlags);
	list<LPSPropValue>::iterator i;
	ULONG c;
	LPSPropValue props = NULL;
	HRESULT hr = hrSuccess;
	SPropValue sConvert;
	convert_context converter;
	std::wstring unicode;
	std::string ansi;
	LPSPropValue lpCopy = NULL;

	if (!lpPropTagArray) {
		// all properties are requested
		hr = MAPIAllocateBuffer(sizeof(SPropValue)*properties.size(), (void**)&props);
		if (hr != hrSuccess)
			goto exit;

		for (c = 0, i = properties.begin(); i != properties.end(); i++, c++) {
			// perform unicode conversion if required
			if ((ulFlags & MAPI_UNICODE) && (PROP_TYPE((*i)->ulPropTag) == PT_STRING8)) {
				sConvert.ulPropTag = CHANGE_PROP_TYPE((*i)->ulPropTag, PT_UNICODE);
				unicode = converter.convert_to<wstring>((*i)->Value.lpszA);
				sConvert.Value.lpszW = (WCHAR*)unicode.c_str();

				lpCopy = &sConvert;
			} else if (((ulFlags & MAPI_UNICODE) == 0) && (PROP_TYPE((*i)->ulPropTag) == PT_UNICODE)) {
				sConvert.ulPropTag = CHANGE_PROP_TYPE((*i)->ulPropTag, PT_STRING8);
				ansi = converter.convert_to<string>((*i)->Value.lpszW);
				sConvert.Value.lpszA = (char*)ansi.c_str();

				lpCopy = &sConvert;
			} else {
				lpCopy = *i;
			}

			hr = Util::HrCopyProperty(&props[c], lpCopy, (void *)props);
			if (hr != hrSuccess)
				goto exit;
		}
	} else {
		hr = MAPIAllocateBuffer(sizeof(SPropValue)*lpPropTagArray->cValues, (void**)&props);
		if (hr != hrSuccess)
			goto exit;

		for (c = 0; c < lpPropTagArray->cValues; c++) {
			for (i = properties.begin(); i != properties.end(); i++) {
				if (PROP_ID((*i)->ulPropTag) == PROP_ID(lpPropTagArray->aulPropTag[c])) {
					// perform unicode conversion if required
					if (PROP_TYPE((*i)->ulPropTag) == PT_STRING8 && 
						(PROP_TYPE(lpPropTagArray->aulPropTag[c]) == PT_UNICODE ||
						 ((ulFlags & MAPI_UNICODE) && PROP_TYPE(lpPropTagArray->aulPropTag[c]) == PT_UNSPECIFIED)))
					{
						// string8 to unicode
						sConvert.ulPropTag = CHANGE_PROP_TYPE((*i)->ulPropTag, PT_UNICODE);
						unicode = converter.convert_to<wstring>((*i)->Value.lpszA);
						sConvert.Value.lpszW = (WCHAR*)unicode.c_str();

						lpCopy = &sConvert;
					}
					else if (PROP_TYPE((*i)->ulPropTag) == PT_UNICODE &&
							 (PROP_TYPE(lpPropTagArray->aulPropTag[c]) == PT_STRING8 ||
							  (((ulFlags & MAPI_UNICODE) == 0) && PROP_TYPE(lpPropTagArray->aulPropTag[c]) == PT_UNSPECIFIED)))
					{
						// unicode to string8
						sConvert.ulPropTag = CHANGE_PROP_TYPE((*i)->ulPropTag, PT_STRING8);
						ansi = converter.convert_to<string>((*i)->Value.lpszW);
						sConvert.Value.lpszA = (char*)ansi.c_str();

						lpCopy = &sConvert;
					}
					else if (PROP_TYPE((*i)->ulPropTag) == PT_MV_STRING8 && 
							 (PROP_TYPE(lpPropTagArray->aulPropTag[c]) == PT_MV_UNICODE ||
							  ((ulFlags & MAPI_UNICODE) && PROP_TYPE(lpPropTagArray->aulPropTag[c]) == PT_UNSPECIFIED)))
					{
						// mv string8 to mv unicode
						sConvert.ulPropTag = CHANGE_PROP_TYPE((*i)->ulPropTag, PT_MV_UNICODE);
						sConvert.Value.MVszW.cValues = (*i)->Value.MVszA.cValues;
						hr = MAPIAllocateMore((*i)->Value.MVszA.cValues * sizeof(WCHAR*), props, (void**)&sConvert.Value.MVszW.lppszW);
						if (hr != hrSuccess)
							goto exit;
						for (ULONG c = 0; c < (*i)->Value.MVszA.cValues; c++) {
							unicode = converter.convert_to<wstring>((*i)->Value.MVszA.lppszA[c]);
							hr = MAPIAllocateMore(unicode.length() * sizeof(WCHAR) + sizeof(WCHAR), props, (void**)&sConvert.Value.MVszW.lppszW[c]);
							if (hr != hrSuccess)
								goto exit;
							wcscpy(sConvert.Value.MVszW.lppszW[c], unicode.c_str());
						}

						lpCopy = &sConvert;
					}
					else if (PROP_TYPE((*i)->ulPropTag) == PT_MV_UNICODE &&
							 (PROP_TYPE(lpPropTagArray->aulPropTag[c]) == PT_MV_STRING8 ||
							  (((ulFlags & MAPI_UNICODE) == 0) && PROP_TYPE(lpPropTagArray->aulPropTag[c]) == PT_UNSPECIFIED)))
					{
						// mv string8 to mv unicode
						sConvert.ulPropTag = CHANGE_PROP_TYPE((*i)->ulPropTag, PT_MV_STRING8);
						sConvert.Value.MVszA.cValues = (*i)->Value.MVszW.cValues;
						hr = MAPIAllocateMore((*i)->Value.MVszW.cValues * sizeof(char*), props, (void**)&sConvert.Value.MVszA.lppszA);
						if (hr != hrSuccess)
							goto exit;
						for (ULONG c = 0; c < (*i)->Value.MVszW.cValues; c++) {
							ansi = converter.convert_to<string>((*i)->Value.MVszW.lppszW[c]);
							hr = MAPIAllocateMore(ansi.length() * sizeof(char) + sizeof(char), props, (void**)&sConvert.Value.MVszA.lppszA[c]);
							if (hr != hrSuccess)
								goto exit;
							strcpy(sConvert.Value.MVszA.lppszA[c], ansi.c_str());
						}

						lpCopy = &sConvert;
					} else {
						// memory property is requested property
						lpCopy = *i;
					}

					hr = Util::HrCopyProperty(&props[c], lpCopy, (void *)props);
					if (hr != hrSuccess)
						goto exit;
					break;
				}
			}
		
			if (i == properties.end()) {
				// Not found
				props[c].ulPropTag = PROP_TAG(PT_ERROR, PROP_ID(lpPropTagArray->aulPropTag[c]));
				props[c].Value.err = MAPI_E_NOT_FOUND;
        		
				hr = MAPI_W_ERRORS_RETURNED;
			}
		}
	}

	*lpcValues = c;
	*lppPropArray = props;

exit:
	if (FAILED(hr) && props)
		MAPIFreeBuffer(props);

	TRACE_MAPILIB2(TRACE_RETURN, "IMAPIProp::GetProps", "%s\n%s", GetMAPIErrorDescription(hr).c_str(), PropNameFromPropArray(*lpcValues, *lppPropArray).c_str());
	return hr;
}

HRESULT M4LMAPIProp::GetPropList(ULONG ulFlags, LPSPropTagArray* lppPropTagArray) {
	TRACE_MAPILIB(TRACE_ENTRY, "IMAPIProp::GetPropList", "");
	TRACE_MAPILIB1(TRACE_RETURN, "IMAPIProp::GetPropList", "0x%08x", MAPI_E_NO_SUPPORT);
	return MAPI_E_NO_SUPPORT;
}

HRESULT M4LMAPIProp::OpenProperty(ULONG ulPropTag, LPCIID lpiid, ULONG ulInterfaceOptions, ULONG ulFlags, LPUNKNOWN* lppUnk) {
	TRACE_MAPILIB(TRACE_ENTRY, "IMAPIProp::OpenProperty", "");
	TRACE_MAPILIB1(TRACE_RETURN, "IMAPIProp::OpenProperty", "0x%08x", MAPI_E_NO_SUPPORT);
	return MAPI_E_NO_SUPPORT;
}

HRESULT M4LMAPIProp::SetProps(ULONG cValues, LPSPropValue lpPropArray, LPSPropProblemArray* lppProblems) {
	TRACE_MAPILIB1(TRACE_ENTRY, "IMAPIProp::SetProps", "%s", PropNameFromPropArray(cValues, lpPropArray).c_str());
	list<LPSPropValue>::iterator i, del;
	ULONG c;
	LPSPropValue pv = NULL;
	HRESULT hr = hrSuccess;

	// Validate input
	if (lpPropArray == NULL || cValues == 0) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	for (c=0; c<cValues; c++) {
		// TODO: return MAPI_E_INVALID_PARAMETER, if multivalued property in 
		//       the array and its cValues member is set to zero.		
		if (PROP_TYPE(lpPropArray[c].ulPropTag) == PT_OBJECT) {
			hr = MAPI_E_INVALID_PARAMETER;
			goto exit;
		}		
	}

    // remove possible old properties
	for (c=0; c<cValues; c++) {
		for(i = properties.begin(); i != properties.end(); ) {
			if ( PROP_ID((*i)->ulPropTag) == PROP_ID(lpPropArray[c].ulPropTag) && 
				(*i)->ulPropTag != PR_NULL && 
				PROP_TYPE((*i)->ulPropTag) != PT_ERROR)
			{
				del = i++;
				MAPIFreeBuffer(*del);
				properties.erase(del);
				break;
			} else {
				i++;
			}
		}
	}

    // set new properties
	for (c=0; c<cValues; c++) {
		// Ignore PR_NULL property tag and all properties with a type of PT_ERROR
		if(PROP_TYPE(lpPropArray[c].ulPropTag) == PT_ERROR || 
			lpPropArray[c].ulPropTag == PR_NULL)
			continue;
		
		hr = MAPIAllocateBuffer(sizeof(SPropValue), (void**)&pv);
		if (hr != hrSuccess)
			goto exit;

		memset(pv, 0, sizeof(SPropValue));
		hr = Util::HrCopyProperty(pv, &lpPropArray[c], (void *)pv);
		if (hr != hrSuccess) {
			MAPIFreeBuffer(pv);
			goto exit;
		}
		properties.push_back(pv);
	}

exit:
	TRACE_MAPILIB1(TRACE_RETURN, "IMAPIProp::SetProps", "0x%08x", hr);
	return hr;
}

HRESULT M4LMAPIProp::DeleteProps(LPSPropTagArray lpPropTagArray, LPSPropProblemArray* lppProblems) {
	TRACE_MAPILIB(TRACE_ENTRY, "IMAPIProp::DeleteProps", "");

	HRESULT hr = hrSuccess;
	list<LPSPropValue>::iterator i;

	for (ULONG c = 0; c < lpPropTagArray->cValues; c++) {
		for (i = properties.begin(); i != properties.end(); i++) {
			// @todo check PT_STRING8 vs PT_UNICODE
			if ((*i)->ulPropTag == lpPropTagArray->aulPropTag[c] ||
				(PROP_TYPE((*i)->ulPropTag) == PT_UNSPECIFIED && PROP_ID((*i)->ulPropTag) == PROP_ID(lpPropTagArray->aulPropTag[c])) )
			{
				MAPIFreeBuffer(*i);
				properties.erase(i);
				break;
			}
		}
	}

	TRACE_MAPILIB1(TRACE_RETURN, "IMAPIProp::DeleteProps", "0x%08x", hr);
	return hr;
}

HRESULT M4LMAPIProp::CopyTo(ULONG ciidExclude, LPCIID rgiidExclude, LPSPropTagArray lpExcludeProps, ULONG ulUIParam,
			    LPMAPIPROGRESS lpProgress, LPCIID lpInterface, LPVOID lpDestObj, ULONG ulFlags,
			    LPSPropProblemArray* lppProblems) {
	TRACE_MAPILIB(TRACE_ENTRY, "IMAPIProp::CopyTo", "");
	TRACE_MAPILIB1(TRACE_RETURN, "IMAPIProp::CopyTo", "0x%08x", MAPI_E_NO_SUPPORT);
	return MAPI_E_NO_SUPPORT;
}

HRESULT M4LMAPIProp::CopyProps(LPSPropTagArray lpIncludeProps, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, LPCIID lpInterface,
			       LPVOID lpDestObj, ULONG ulFlags, LPSPropProblemArray* lppProblems) {
	TRACE_MAPILIB(TRACE_ENTRY, "IMAPIProp::CopyProps", "");
	TRACE_MAPILIB1(TRACE_RETURN, "IMAPIProp::CopyProps", "0x%08x", MAPI_E_NO_SUPPORT);
	return MAPI_E_NO_SUPPORT;
}

HRESULT M4LMAPIProp::GetNamesFromIDs(LPSPropTagArray* lppPropTags, LPGUID lpPropSetGuid, ULONG ulFlags, ULONG* lpcPropNames,
				     LPMAPINAMEID** lpppPropNames) {
	TRACE_MAPILIB(TRACE_ENTRY, "IMAPIProp::GetNamesFromIDs", "");
	TRACE_MAPILIB1(TRACE_RETURN, "IMAPIProp::GetNamesFromIDs", "0x%08x", MAPI_E_NO_SUPPORT);
	return MAPI_E_NO_SUPPORT;
}

HRESULT M4LMAPIProp::GetIDsFromNames(ULONG cPropNames, LPMAPINAMEID* lppPropNames, ULONG ulFlags, LPSPropTagArray* lppPropTags) {
	TRACE_MAPILIB(TRACE_ENTRY, "IMAPIProp::GetIDsFromNames", "");
	TRACE_MAPILIB1(TRACE_RETURN, "IMAPIProp::GetIDsFromNames", "0x%08x", MAPI_E_NO_SUPPORT);
	return MAPI_E_NO_SUPPORT;
}

// iunknown passthru
ULONG M4LMAPIProp::AddRef() {
	return M4LUnknown::AddRef();
}
ULONG M4LMAPIProp::Release() {
	return M4LUnknown::Release();
}
HRESULT M4LMAPIProp::QueryInterface(REFIID refiid, void **lpvoid) {
	TRACE_MAPILIB(TRACE_ENTRY, "IMAPIProp::QueryInterface", "");
	HRESULT hr = hrSuccess;
	if ((refiid == IID_IMAPIProp) || (refiid == IID_IUnknown)) {
		AddRef();
		*lpvoid = (IMAPIProp *)this;
		hr = hrSuccess;
	} else
		hr = MAPI_E_INTERFACE_NOT_SUPPORTED;

	TRACE_MAPILIB1(TRACE_RETURN, "IMAPIProp::QueryInterface", "0x%08x", hr);
	return hr;
}


// ---
// IProfSect
// ---

M4LProfSect::M4LProfSect(BOOL bGlobalProf) {
	this->bGlobalProf = bGlobalProf;
}

M4LProfSect::~M4LProfSect() {
}

HRESULT M4LProfSect::ValidateState(ULONG ulUIParam, ULONG ulFlags) {
	TRACE_MAPILIB(TRACE_ENTRY, "M4LProfSect::ValidateState", "");
	TRACE_MAPILIB1(TRACE_RETURN, "M4LProfSect::ValidateState", "0x%08x", MAPI_E_NO_SUPPORT);
	return MAPI_E_NO_SUPPORT;
}

HRESULT M4LProfSect::SettingsDialog(ULONG ulUIParam, ULONG ulFlags) {
	TRACE_MAPILIB(TRACE_ENTRY, "M4LProfSect::SettingsDialog", "");
	TRACE_MAPILIB1(TRACE_RETURN, "M4LProfSect::SettingsDialog", "0x%08x", MAPI_E_NO_SUPPORT);
	return MAPI_E_NO_SUPPORT;
}

HRESULT M4LProfSect::ChangePassword(LPTSTR lpOldPass, LPTSTR lpNewPass, ULONG ulFlags) {
	TRACE_MAPILIB(TRACE_ENTRY, "M4LProfSect::ChangePassword", "");
	TRACE_MAPILIB1(TRACE_RETURN, "M4LProfSect::ChangePassword", "0x%08x", MAPI_E_NO_SUPPORT);
	return MAPI_E_NO_SUPPORT;
}

HRESULT M4LProfSect::FlushQueues(ULONG ulUIParam, ULONG cbTargetTransport, LPENTRYID lpTargetTransport, ULONG ulFlags) {
	TRACE_MAPILIB(TRACE_ENTRY, "M4LProfSect::FlushQueues", "");
	TRACE_MAPILIB1(TRACE_RETURN, "M4LProfSect::FlushQueues", "0x%08x", MAPI_E_NO_SUPPORT);
	return MAPI_E_NO_SUPPORT;
}

// IMAPIProp passthru
HRESULT M4LProfSect::GetLastError(HRESULT hResult, ULONG ulFlags, LPMAPIERROR* lppMAPIError) {
	return M4LMAPIProp::GetLastError(hResult, ulFlags, lppMAPIError);
}

HRESULT M4LProfSect::SaveChanges(ULONG ulFlags) {
	return M4LMAPIProp::SaveChanges(ulFlags);
}

HRESULT M4LProfSect::GetProps(LPSPropTagArray lpPropTagArray, ULONG ulFlags, ULONG* lpcValues, LPSPropValue* lppPropArray) {
	return M4LMAPIProp::GetProps(lpPropTagArray, ulFlags, lpcValues, lppPropArray);
}

HRESULT M4LProfSect::GetPropList(ULONG ulFlags, LPSPropTagArray* lppPropTagArray) {
	return M4LMAPIProp::GetPropList(ulFlags, lppPropTagArray);
}

HRESULT M4LProfSect::OpenProperty(ULONG ulPropTag, LPCIID lpiid, ULONG ulInterfaceOptions, ULONG ulFlags, LPUNKNOWN* lppUnk) {
	return M4LMAPIProp::OpenProperty(ulPropTag, lpiid, ulInterfaceOptions, ulFlags, lppUnk);
}

HRESULT M4LProfSect::SetProps(ULONG cValues, LPSPropValue lpPropArray, LPSPropProblemArray* lppProblems) {
	HRESULT hr = hrSuccess;

	hr = M4LMAPIProp::SetProps(cValues, lpPropArray, lppProblems);


	return hr;
}

HRESULT M4LProfSect::DeleteProps(LPSPropTagArray lpPropTagArray, LPSPropProblemArray* lppProblems) {
	return M4LMAPIProp::DeleteProps(lpPropTagArray, lppProblems);
}

HRESULT M4LProfSect::CopyTo(ULONG ciidExclude, LPCIID rgiidExclude, LPSPropTagArray lpExcludeProps, ULONG ulUIParam,
			    LPMAPIPROGRESS lpProgress, LPCIID lpInterface, LPVOID lpDestObj, ULONG ulFlags, LPSPropProblemArray* lppProblems) {
	return M4LMAPIProp::CopyTo(ciidExclude, rgiidExclude, lpExcludeProps, ulUIParam,
							   lpProgress, lpInterface, lpDestObj, ulFlags, lppProblems);
}

HRESULT M4LProfSect::CopyProps(LPSPropTagArray lpIncludeProps, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, LPCIID lpInterface,
							   LPVOID lpDestObj, ULONG ulFlags, LPSPropProblemArray* lppProblems) {
	return M4LMAPIProp::CopyProps(lpIncludeProps, ulUIParam, lpProgress, lpInterface, lpDestObj, ulFlags, lppProblems);
}

HRESULT M4LProfSect::GetNamesFromIDs(LPSPropTagArray* lppPropTags, LPGUID lpPropSetGuid, ULONG ulFlags, ULONG* lpcPropNames,
									 LPMAPINAMEID** lpppPropNames) {
	return M4LMAPIProp::GetNamesFromIDs(lppPropTags, lpPropSetGuid, ulFlags, lpcPropNames, lpppPropNames);
}

HRESULT M4LProfSect::GetIDsFromNames(ULONG cPropNames, LPMAPINAMEID* lppPropNames, ULONG ulFlags, LPSPropTagArray* lppPropTags) {
	return M4LMAPIProp::GetIDsFromNames(cPropNames, lppPropNames, ulFlags, lppPropTags);
}

// iunknown passthru
ULONG M4LProfSect::AddRef() {
	return M4LUnknown::AddRef();
}
ULONG M4LProfSect::Release() {
	return M4LUnknown::Release();
}
HRESULT M4LProfSect::QueryInterface(REFIID refiid, void **lpvoid) {
	TRACE_MAPILIB(TRACE_ENTRY, "M4LProfSect::QueryInterface", "");
	HRESULT hr = hrSuccess;

	if ((refiid == IID_IProfSect) || (refiid == IID_IMAPIProp) || (refiid == IID_IUnknown)) {
		AddRef();
		*lpvoid = (IProfSect *)this;
		hr = hrSuccess;
    } else
		hr = MAPI_E_INTERFACE_NOT_SUPPORTED;

	TRACE_MAPILIB1(TRACE_RETURN, "M4LProfSect::QueryInterface", "0x%08x", hr);
	return hr;
}


// ---
// IMAPITable
// ---
M4LMAPITable::M4LMAPITable() {
}

M4LMAPITable::~M4LMAPITable() {
}

HRESULT M4LMAPITable::GetLastError(HRESULT hResult, ULONG ulFlags, LPMAPIERROR *lppMAPIError) {
	*lppMAPIError = NULL;
	return hrSuccess;
}

HRESULT M4LMAPITable::Advise(ULONG ulEventMask, LPMAPIADVISESINK lpAdviseSink, ULONG * lpulConnection) {
	TRACE_MAPILIB(TRACE_ENTRY, "M4LMAPITable::Advise", "");
	TRACE_MAPILIB1(TRACE_RETURN, "M4LMAPITable::Advise", "0x%08x", MAPI_E_NO_SUPPORT);
	return MAPI_E_NO_SUPPORT;
}

HRESULT M4LMAPITable::Unadvise(ULONG ulConnection) {
	TRACE_MAPILIB(TRACE_ENTRY, "M4LMAPITable::Unadvise", "");
	TRACE_MAPILIB1(TRACE_RETURN, "M4LMAPITable::Unadvise", "0x%08x", MAPI_E_NO_SUPPORT);
	return MAPI_E_NO_SUPPORT;
}

HRESULT M4LMAPITable::GetStatus(ULONG *lpulTableStatus, ULONG *lpulTableType) {
	TRACE_MAPILIB(TRACE_ENTRY, "M4LMAPITable::GetStatus", "");
	TRACE_MAPILIB1(TRACE_RETURN, "M4LMAPITable::GetStatus", "0x%08x", MAPI_E_NO_SUPPORT);
	return MAPI_E_NO_SUPPORT;
}

HRESULT M4LMAPITable::SetColumns(LPSPropTagArray lpPropTagArray, ULONG ulFlags) {
	TRACE_MAPILIB(TRACE_ENTRY, "M4LMAPITable::SetColumns", "");
	TRACE_MAPILIB1(TRACE_RETURN, "M4LMAPITable::SetColumns", "0x%08x", MAPI_E_NO_SUPPORT);
	return MAPI_E_NO_SUPPORT;
}

HRESULT M4LMAPITable::QueryColumns(ULONG ulFlags, LPSPropTagArray *lpPropTagArray) {
	TRACE_MAPILIB(TRACE_ENTRY, "M4LMAPITable::QueryColumns", "");
	TRACE_MAPILIB1(TRACE_RETURN, "M4LMAPITable::QueryColumns", "0x%08x", MAPI_E_NO_SUPPORT);
	return MAPI_E_NO_SUPPORT;
}

HRESULT M4LMAPITable::GetRowCount(ULONG ulFlags, ULONG *lpulCount) {
	TRACE_MAPILIB(TRACE_ENTRY, "M4LMAPITable::GetRowCount", "");
	TRACE_MAPILIB1(TRACE_RETURN, "M4LMAPITable::GetRowCount", "0x%08x", MAPI_E_NO_SUPPORT);
	return MAPI_E_NO_SUPPORT;
}

HRESULT M4LMAPITable::SeekRow(BOOKMARK bkOrigin, LONG lRowCount, LONG *lplRowsSought) {
	TRACE_MAPILIB(TRACE_ENTRY, "M4LMAPITable::SeekRow", "");
	TRACE_MAPILIB1(TRACE_RETURN, "M4LMAPITable::SeekRow", "0x%08x", MAPI_E_NO_SUPPORT);
	return MAPI_E_NO_SUPPORT;
}

HRESULT M4LMAPITable::SeekRowApprox(ULONG ulNumerator, ULONG ulDenominator) {
	TRACE_MAPILIB(TRACE_ENTRY, "M4LMAPITable::SeekRowApprox", "");
	TRACE_MAPILIB1(TRACE_RETURN, "M4LMAPITable::SeekRowApprox", "0x%08x", MAPI_E_NO_SUPPORT);
	return MAPI_E_NO_SUPPORT;
}

HRESULT M4LMAPITable::QueryPosition(ULONG *lpulRow, ULONG *lpulNumerator, ULONG *lpulDenominator) {
	TRACE_MAPILIB(TRACE_ENTRY, "M4LMAPITable::QueryPosition", "");
	TRACE_MAPILIB1(TRACE_RETURN, "M4LMAPITable::QueryPosition", "0x%08x", MAPI_E_NO_SUPPORT);
	return MAPI_E_NO_SUPPORT;
}

HRESULT M4LMAPITable::FindRow(LPSRestriction lpRestriction, BOOKMARK bkOrigin, ULONG ulFlags) {
	TRACE_MAPILIB(TRACE_ENTRY, "M4LMAPITable::FindRow", "");
	TRACE_MAPILIB1(TRACE_RETURN, "M4LMAPITable::FindRow", "0x%08x", MAPI_E_NO_SUPPORT);
	return MAPI_E_NO_SUPPORT;
}

HRESULT M4LMAPITable::Restrict(LPSRestriction lpRestriction, ULONG ulFlags) {
	TRACE_MAPILIB(TRACE_ENTRY, "M4LMAPITable::Restrict", "");
	TRACE_MAPILIB1(TRACE_RETURN, "M4LMAPITable::Restrict", "0x%08x", MAPI_E_NO_SUPPORT);
	return MAPI_E_NO_SUPPORT;
}

HRESULT M4LMAPITable::CreateBookmark(BOOKMARK* lpbkPosition) {
	TRACE_MAPILIB(TRACE_ENTRY, "M4LMAPITable::CreateBookmark", "");
	TRACE_MAPILIB1(TRACE_RETURN, "M4LMAPITable::CreateBookmark", "0x%08x", MAPI_E_NO_SUPPORT);
	return MAPI_E_NO_SUPPORT;
}

HRESULT M4LMAPITable::FreeBookmark(BOOKMARK bkPosition) {
	TRACE_MAPILIB(TRACE_ENTRY, "M4LMAPITable::FreeBookmark", "");
	TRACE_MAPILIB1(TRACE_RETURN, "M4LMAPITable::FreeBookmark", "0x%08x", MAPI_E_NO_SUPPORT);
	return MAPI_E_NO_SUPPORT;
}

HRESULT M4LMAPITable::SortTable(LPSSortOrderSet lpSortCriteria, ULONG ulFlags) {
	TRACE_MAPILIB(TRACE_ENTRY, "M4LMAPITable::SortTable", "");
	TRACE_MAPILIB1(TRACE_RETURN, "M4LMAPITable::SortTable", "0x%08x", MAPI_E_NO_SUPPORT);
	return MAPI_E_NO_SUPPORT;
}

HRESULT M4LMAPITable::QuerySortOrder(LPSSortOrderSet *lppSortCriteria) {
	TRACE_MAPILIB(TRACE_ENTRY, "M4LMAPITable::QuerySortOrder", "");
	TRACE_MAPILIB1(TRACE_RETURN, "M4LMAPITable::QuerySortOrder", "0x%08x", MAPI_E_NO_SUPPORT);
	return MAPI_E_NO_SUPPORT;
}

HRESULT M4LMAPITable::QueryRows(LONG lRowCount, ULONG ulFlags, LPSRowSet *lppRows) {
    // TODO
	TRACE_MAPILIB(TRACE_ENTRY, "M4LMAPITable::QueryRows", "");
	TRACE_MAPILIB1(TRACE_RETURN, "M4LMAPITable::QueryRows", "0x%08x", MAPI_E_NO_SUPPORT);
	return MAPI_E_NO_SUPPORT;
}

HRESULT M4LMAPITable::Abort() {
	TRACE_MAPILIB(TRACE_ENTRY, "M4LMAPITable::Abort", "");
	TRACE_MAPILIB1(TRACE_RETURN, "M4LMAPITable::Abort", "0x%08x", MAPI_E_NO_SUPPORT);
	return MAPI_E_NO_SUPPORT;
}

HRESULT M4LMAPITable::ExpandRow(ULONG cbInstanceKey, LPBYTE pbInstanceKey, ULONG ulRowCount,
								ULONG ulFlags, LPSRowSet * lppRows, ULONG *lpulMoreRows) {
	TRACE_MAPILIB(TRACE_ENTRY, "M4LMAPITable::ExpandRow", "");
	TRACE_MAPILIB1(TRACE_RETURN, "M4LMAPITable::ExpandRow", "0x%08x", MAPI_E_NO_SUPPORT);
	return MAPI_E_NO_SUPPORT;
}

HRESULT M4LMAPITable::CollapseRow(ULONG cbInstanceKey, LPBYTE pbInstanceKey, ULONG ulFlags, ULONG *lpulRowCount) {
	TRACE_MAPILIB(TRACE_ENTRY, "M4LMAPITable::CollapseRow", "");
	TRACE_MAPILIB1(TRACE_RETURN, "M4LMAPITable::CollapseRow", "0x%08x", MAPI_E_NO_SUPPORT);
	return MAPI_E_NO_SUPPORT;
}

HRESULT M4LMAPITable::WaitForCompletion(ULONG ulFlags, ULONG ulTimeout, ULONG *lpulTableStatus) {
	TRACE_MAPILIB(TRACE_ENTRY, "M4LMAPITable::WaitForCompletion", "");
	TRACE_MAPILIB1(TRACE_RETURN, "M4LMAPITable::WaitForCompletion", "0x%08x", MAPI_E_NO_SUPPORT);
	return MAPI_E_NO_SUPPORT;
}

HRESULT M4LMAPITable::GetCollapseState(ULONG ulFlags, ULONG cbInstanceKey, LPBYTE lpbInstanceKey, ULONG *lpcbCollapseState,
			 LPBYTE *lppbCollapseState) {
	TRACE_MAPILIB(TRACE_ENTRY, "M4LMAPITable::GetCollapseState", "");
	TRACE_MAPILIB1(TRACE_RETURN, "M4LMAPITable::GetCollapseState", "0x%08x", MAPI_E_NO_SUPPORT);
	return MAPI_E_NO_SUPPORT;
}

HRESULT M4LMAPITable::SetCollapseState(ULONG ulFlags, ULONG cbCollapseState, LPBYTE pbCollapseState, BOOKMARK *lpbkLocation) {
	TRACE_MAPILIB(TRACE_ENTRY, "M4LMAPITable::SetCollapseState", "");
	TRACE_MAPILIB1(TRACE_RETURN, "M4LMAPITable::SetCollapseState", "0x%08x", MAPI_E_NO_SUPPORT);
	return MAPI_E_NO_SUPPORT;
}

// iunknown passthru
ULONG M4LMAPITable::AddRef() {
	return M4LUnknown::AddRef();
}
ULONG M4LMAPITable::Release() {
	return M4LUnknown::Release();
}
HRESULT M4LMAPITable::QueryInterface(REFIID refiid, void **lpvoid) {
	TRACE_MAPILIB(TRACE_ENTRY, "M4LMAPITable::QueryInterface", "");
	HRESULT hr = hrSuccess;

	if ((refiid == IID_IMAPITable) || (refiid == IID_IUnknown)) {
		AddRef();
		*lpvoid = (IMAPITable *)this;
		hr = hrSuccess;
	} else
		hr = MAPI_E_INTERFACE_NOT_SUPPORTED;

	TRACE_MAPILIB1(TRACE_RETURN, "M4LMAPITable::QueryInterface", "0x%08x", hr);
	return hr;
}

// ---
// IProviderAdmin
// ---
M4LProviderAdmin::M4LProviderAdmin(M4LMsgServiceAdmin* new_msa, char *szService) {
	msa = new_msa;
	if(szService)
		this->szService = strdup(szService);
	else
		this->szService = NULL;
}

M4LProviderAdmin::~M4LProviderAdmin() {
	if(szService)
		free(szService);
}

HRESULT M4LProviderAdmin::GetLastError(HRESULT hResult, ULONG ulFlags, LPMAPIERROR* lppMAPIError) {
    *lppMAPIError = NULL;
    return hrSuccess;
}

HRESULT M4LProviderAdmin::GetProviderTable(ULONG ulFlags, LPMAPITABLE* lppTable) {
	TRACE_MAPILIB(TRACE_ENTRY, "M4LProviderAdmin::GetProviderTable", "");
	HRESULT hr = hrSuccess;
	ULONG cValues = 0;
	LPSPropValue lpsProps = NULL;
	list<providerEntry *>::iterator i;
	ECMemTable *lpTable = NULL;
	ECMemTableView *lpTableView = NULL;
	LPSPropValue lpDest = NULL;
	ULONG cValuesDest = 0;
	SPropValue sPropID;
	int n = 0;
	LPSPropTagArray lpPropTagArray = NULL;

	SizedSPropTagArray(8, sptaProviderCols) = {8, {PR_MDB_PROVIDER, PR_INSTANCE_KEY, PR_RECORD_KEY, PR_ENTRYID, PR_DISPLAY_NAME_A, PR_OBJECT_TYPE, PR_RESOURCE_TYPE, PR_PROVIDER_UID} };
	
	pthread_mutex_lock(&msa->m_mutexserviceadmin);

	hr = Util::HrCopyUnicodePropTagArray(ulFlags, (LPSPropTagArray)&sptaProviderCols, &lpPropTagArray);
	if(hr != hrSuccess)
		goto exit;

	hr = ECMemTable::Create(lpPropTagArray, PR_ROWID, &lpTable);
	if(hr != hrSuccess)
		goto exit;
	
	// Loop through all providers, add each to the table
	for(i=msa->providers.begin(); i != msa->providers.end(); i++) {
		if(szService) {
			if(strcmp(szService, (*i)->servicename.c_str()) != 0)
				continue;
		}
		
		hr = (*i)->profilesection->GetProps((LPSPropTagArray)&sptaProviderCols, 0, &cValues, &lpsProps);
		if (FAILED(hr))
			goto exit;
		
		sPropID.ulPropTag = PR_ROWID;
		sPropID.Value.ul = n++;
		
		hr = Util::HrAddToPropertyArray(lpsProps, cValues, &sPropID, &lpDest, &cValuesDest);
		if(hr != hrSuccess)
			goto exit;
		
		lpTable->HrModifyRow(ECKeyTable::TABLE_ROW_ADD, NULL, lpDest, cValuesDest);
		
		MAPIFreeBuffer(lpDest);
		MAPIFreeBuffer(lpsProps);
		lpDest = NULL;
		lpsProps = NULL;
	}
	
	hr = lpTable->HrGetView(createLocaleFromName(""), ulFlags, &lpTableView);
	if(hr != hrSuccess)
		goto exit;
		
	hr = lpTableView->QueryInterface(IID_IMAPITable, (void **)lppTable);
	
exit:
	pthread_mutex_unlock(&msa->m_mutexserviceadmin);

	if (lpPropTagArray)
		MAPIFreeBuffer(lpPropTagArray);

	if (lpTableView)
		lpTableView->Release();

	if (lpTable)
		lpTable->Release();

	if (lpDest)
		MAPIFreeBuffer(lpDest);
	
	if (lpsProps)
		MAPIFreeBuffer(lpsProps);
	
	TRACE_MAPILIB1(TRACE_RETURN, "M4LProviderAdmin::GetProviderTable", "0x%08x", hr);
	return hr;
}

/** 
 * Add a provider to a MAPI service
 * 
 * @param[in] lpszProvider name of the provider to add, must be known through mapisvc.inf
 * @param[in] cValues number of properties in lpProps
 * @param[in] lpProps properties to set on the provider context (properties from mapisvc.inf)
 * @param[in] ulUIParam Unused in linux
 * @param[in] ulFlags must be 0
 * @param[out] lpUID a uid which will identify this added provider in the service context
 * 
 * @return MAPI Error code
 */
HRESULT M4LProviderAdmin::CreateProvider(LPTSTR lpszProvider, ULONG cValues, LPSPropValue lpProps, ULONG ulUIParam,
										 ULONG ulFlags, MAPIUID* lpUID) {
    TRACE_MAPILIB(TRACE_ENTRY, "M4LProviderAdmin::CreateProvider", "");
	SPropValue sProps[10];
	ULONG nProps = 0;
	LPSPropValue lpResource = NULL;
	LPSPropValue lpsPropValProfileName = NULL;
	providerEntry *entry = NULL;
	serviceEntry* lpService = NULL;
	SVCProvider* lpProvider = NULL;
	ULONG cProviderProps = 0;
	LPSPropValue lpProviderProps = NULL;
	HRESULT hr = hrSuccess;

	pthread_mutex_lock(&msa->m_mutexserviceadmin);

	if(szService == NULL) {
		hr = MAPI_E_NO_ACCESS;
		goto exit;
	}

	lpService = msa->findServiceAdmin((LPTSTR)szService);
	if (!lpService) {
		hr = MAPI_E_NO_ACCESS;
		goto exit;
	}

	lpProvider = lpService->service->GetProvider(lpszProvider, ulFlags);
	if (!lpProvider) {
		hr = MAPI_E_NO_ACCESS;
		goto exit;
	}

	entry = new providerEntry;

	entry->profilesection = new M4LProfSect();
	if(!entry->profilesection) {
		delete entry;
		hr = MAPI_E_NOT_ENOUGH_MEMORY;
		goto exit;
	}
	
	// Set the default profilename
	hr = HrGetOneProp((IProfSect*)msa->profilesection, PR_PROFILE_NAME_A, &lpsPropValProfileName);
	if (hr != hrSuccess)
		goto exit;

	hr = entry->profilesection->SetProps(1, lpsPropValProfileName, NULL);
	if (hr != hrSuccess)
		goto exit;

	CoCreateGuid((LPGUID)&entry->uid);
	entry->profilesection->AddRef();

	// no need to free this, not a copy!
	lpProvider->GetProps(&cProviderProps, &lpProviderProps);
	hr = entry->profilesection->SetProps(cProviderProps, lpProviderProps, NULL);
	if (hr != hrSuccess)
		goto exit;

	if (cValues && lpProps) {
		hr = entry->profilesection->SetProps(cValues, lpProps, NULL);
		if (hr != hrSuccess)
			goto exit;
	}

	sProps[nProps].ulPropTag = PR_INSTANCE_KEY;
	sProps[nProps].Value.bin.lpb = (BYTE *)&entry->uid;
	sProps[nProps].Value.bin.cb = sizeof(GUID);
	nProps++;

	sProps[nProps].ulPropTag = PR_PROVIDER_UID;
	sProps[nProps].Value.bin.lpb = (BYTE *)&entry->uid;
	sProps[nProps].Value.bin.cb = sizeof(GUID);
	nProps++;

	lpResource = PpropFindProp(lpProviderProps, cProviderProps, PR_RESOURCE_TYPE);
	if (!lpResource || lpResource->Value.ul == MAPI_STORE_PROVIDER) {
		sProps[nProps].ulPropTag = PR_OBJECT_TYPE;
		sProps[nProps].Value.ul = MAPI_STORE;
		nProps++;

		lpResource = PpropFindProp(lpProviderProps, cProviderProps, PR_RESOURCE_FLAGS);

		sProps[nProps].ulPropTag = PR_DEFAULT_STORE;
		sProps[nProps].Value.b = (lpResource && (lpResource->Value.ul & STATUS_DEFAULT_STORE) == STATUS_DEFAULT_STORE);
		nProps++;
	} else if (lpResource->Value.ul == MAPI_AB_PROVIDER) {
		sProps[nProps].ulPropTag = PR_OBJECT_TYPE;
		sProps[nProps].Value.ul = MAPI_ADDRBOOK;
		nProps++;
	}

	sProps[nProps].ulPropTag = PR_SERVICE_UID;
	sProps[nProps].Value.bin.lpb = (BYTE *)&lpService->muid;
	sProps[nProps].Value.bin.cb = sizeof(GUID);
	nProps++;

	hr = entry->profilesection->SetProps(nProps, sProps, NULL);
	if (hr != hrSuccess)
		goto exit;

	entry->servicename = szService;
		
	msa->providers.push_back(entry);

	if(lpUID)
		*lpUID = entry->uid;

	// We should really call the MSGServiceEntry with MSG_SERVICE_PROVIDER_CREATE, but there
	// isn't much use at the moment. (since we don't store the profile data on disk? or why not?)
	// another rumor is that that is only called once per service, not once per created provider. huh?
	
exit:
	pthread_mutex_unlock(&msa->m_mutexserviceadmin);

	if (lpsPropValProfileName)
		MAPIFreeBuffer(lpsPropValProfileName);

	TRACE_MAPILIB1(TRACE_RETURN, "M4LProviderAdmin::CreateProvider", "0x%08x", hr);
    return hr;
}

HRESULT M4LProviderAdmin::DeleteProvider(LPMAPIUID lpUID) {
	HRESULT hr = MAPI_E_NOT_FOUND;	
	TRACE_MAPILIB(TRACE_ENTRY, "M4LProviderAdmin::DeleteProvider", "");
	list<providerEntry*>::iterator i;
	
	for(i = msa->providers.begin(); i != msa->providers.end(); i++) {
		if(memcmp(&(*i)->uid, lpUID, sizeof(MAPIUID)) == 0) {
			msa->providers.erase(i);
			hr = hrSuccess;
			break;
		}
	}
	
	TRACE_MAPILIB1(TRACE_RETURN, "M4LProviderAdmin::DeleteProvider", "0x%08x", MAPI_E_NO_SUPPORT);
    return hr;
}

HRESULT M4LProviderAdmin::OpenProfileSection(LPMAPIUID lpUID, LPCIID lpInterface, ULONG ulFlags, LPPROFSECT* lppProfSect) {
	TRACE_MAPILIB(TRACE_ENTRY, "M4LProviderAdmin::OpenProfileSection", "");
	HRESULT hr = hrSuccess;
	providerEntry *provider = NULL;
	// See provider/client/guid.h
	unsigned char globalGuid[] =    { 0x13,0xDB,0xB0,0xC8,0xAA,0x05,0x10,0x1A,0x9B,0xB0,0x00,0xAA,0x00,0x2F,0xC4,0x5A };
	
	pthread_mutex_lock(&msa->m_mutexserviceadmin);

	// Special ID: the global guid opens the profile's global profile section instead of a local profile
	if(memcmp(lpUID,&globalGuid,sizeof(MAPIUID)) == 0) {
		hr = msa->OpenProfileSection(lpUID, lpInterface, ulFlags, lppProfSect);
		goto exit;
	}
	
	provider = msa->findProvider(lpUID);
	if(provider == NULL) {
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}

	hr = provider->profilesection->QueryInterface(lpInterface ? (*lpInterface) : IID_IProfSect, (void**)lppProfSect);

exit:
	pthread_mutex_unlock(&msa->m_mutexserviceadmin);

	TRACE_MAPILIB1(TRACE_RETURN, "M4LProviderAdmin::OpenProfileSection", "0x%08x", hr);
	return hr;
}

// iunknown passthru
ULONG M4LProviderAdmin::AddRef() {
	return M4LUnknown::AddRef();
}
ULONG M4LProviderAdmin::Release() {
	return M4LUnknown::Release();
}
HRESULT M4LProviderAdmin::QueryInterface(REFIID refiid, void **lpvoid) {
	TRACE_MAPILIB(TRACE_ENTRY, "M4LProviderAdmin::QueryInterface", "");
	HRESULT hr = hrSuccess;

	if ((refiid == IID_IProviderAdmin) || (refiid == IID_IUnknown)) {
		AddRef();
		*lpvoid = (IProviderAdmin *)this;
		hr = hrSuccess;
	} else
		hr = MAPI_E_INTERFACE_NOT_SUPPORTED;

	TRACE_MAPILIB1(TRACE_RETURN, "M4LProviderAdmin::QueryInterface", "0x%08x", hr);
	return hr;
}


// 
// IMAPIAdviseSink
// 

M4LMAPIAdviseSink::M4LMAPIAdviseSink(LPNOTIFCALLBACK lpFn, void *lpContext) {
	this->lpContext = lpContext;
	this->lpFn = lpFn;
}

M4LMAPIAdviseSink::~M4LMAPIAdviseSink() {
}

ULONG M4LMAPIAdviseSink::OnNotify(ULONG cNotif, LPNOTIFICATION lpNotifications) {
	return this->lpFn(this->lpContext, cNotif, lpNotifications);
}


// iunknown passthru
ULONG M4LMAPIAdviseSink::AddRef() {
	return M4LUnknown::AddRef();
}
ULONG M4LMAPIAdviseSink::Release() {
	return M4LUnknown::Release();
}
HRESULT M4LMAPIAdviseSink::QueryInterface(REFIID refiid, void **lpvoid) {
	TRACE_MAPILIB(TRACE_ENTRY, "M4LMAPIAdviseSink::QueryInterface", "");
	HRESULT hr = hrSuccess;
	if ((refiid == IID_IMAPIAdviseSink) || (refiid == IID_IUnknown)) {
		AddRef();
		*lpvoid = (IMAPIAdviseSink *)this;
		hr = hrSuccess;
	} else
		hr = MAPI_E_INTERFACE_NOT_SUPPORTED;

	TRACE_MAPILIB1(TRACE_RETURN, "M4LMAPIAdviseSink::QueryInterface", "0x%08x", hr);
	return hr;
}

// 
// IMAPIContainer
// 

M4LMAPIContainer::M4LMAPIContainer() {
}

M4LMAPIContainer::~M4LMAPIContainer() {
}

HRESULT M4LMAPIContainer::GetContentsTable(ULONG ulFlags, LPMAPITABLE* lppTable) {
	return MAPI_E_NO_SUPPORT;
}

HRESULT M4LMAPIContainer::GetHierarchyTable(ULONG ulFlags, LPMAPITABLE* lppTable) {
	return MAPI_E_NO_SUPPORT;
}

HRESULT M4LMAPIContainer::OpenEntry(ULONG cbEntryID, LPENTRYID lpEntryID, LPCIID lpInterface, ULONG ulFlags, ULONG* lpulObjType, LPUNKNOWN* lppUnk) {
	return MAPI_E_NO_SUPPORT;
}

HRESULT M4LMAPIContainer::SetSearchCriteria(LPSRestriction lpRestriction, LPENTRYLIST lpContainerList, ULONG ulSearchFlags) {
	return MAPI_E_NO_SUPPORT;
}

HRESULT M4LMAPIContainer::GetSearchCriteria(ULONG ulFlags, LPSRestriction* lppRestriction, LPENTRYLIST* lppContainerList, ULONG* lpulSearchState) {
	return MAPI_E_NO_SUPPORT;
}

// imapiprop passthru
HRESULT M4LMAPIContainer::GetLastError(HRESULT hResult, ULONG ulFlags, LPMAPIERROR* lppMAPIError) {
	return M4LMAPIProp::GetLastError(hResult, ulFlags, lppMAPIError);
}

HRESULT M4LMAPIContainer::SaveChanges(ULONG ulFlags) {
	return M4LMAPIProp::SaveChanges(ulFlags);
}

HRESULT M4LMAPIContainer::GetProps(LPSPropTagArray lpPropTagArray, ULONG ulFlags, ULONG* lpcValues, LPSPropValue* lppPropArray) {
	return M4LMAPIProp::GetProps(lpPropTagArray, ulFlags, lpcValues, lppPropArray);
}

HRESULT M4LMAPIContainer::GetPropList(ULONG ulFlags, LPSPropTagArray* lppPropTagArray) {
	return M4LMAPIProp::GetPropList(ulFlags, lppPropTagArray);
}

HRESULT M4LMAPIContainer::OpenProperty(ULONG ulPropTag, LPCIID lpiid, ULONG ulInterfaceOptions, ULONG ulFlags, LPUNKNOWN* lppUnk) {
	return M4LMAPIProp::OpenProperty(ulPropTag, lpiid, ulInterfaceOptions, ulFlags, lppUnk);
}

HRESULT M4LMAPIContainer::SetProps(ULONG cValues, LPSPropValue lpPropArray, LPSPropProblemArray* lppProblems) {
	return M4LMAPIProp::SetProps(cValues, lpPropArray, lppProblems);
}

HRESULT M4LMAPIContainer::DeleteProps(LPSPropTagArray lpPropTagArray, LPSPropProblemArray* lppProblems) {
	return M4LMAPIProp::DeleteProps(lpPropTagArray, lppProblems);
}

HRESULT M4LMAPIContainer::CopyTo(ULONG ciidExclude, LPCIID rgiidExclude, LPSPropTagArray lpExcludeProps, ULONG ulUIParam,
								 LPMAPIPROGRESS lpProgress, LPCIID lpInterface, LPVOID lpDestObj, ULONG ulFlags,
								 LPSPropProblemArray* lppProblems) {
	return M4LMAPIProp::CopyTo(ciidExclude, rgiidExclude, lpExcludeProps, ulUIParam, lpProgress, lpInterface, lpDestObj, ulFlags, lppProblems);
}

HRESULT M4LMAPIContainer::CopyProps(LPSPropTagArray lpIncludeProps, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, LPCIID lpInterface,
									LPVOID lpDestObj, ULONG ulFlags, LPSPropProblemArray* lppProblems) {
	return M4LMAPIProp::CopyProps(lpIncludeProps, ulUIParam, lpProgress, lpInterface, lpDestObj, ulFlags, lppProblems);
}

HRESULT M4LMAPIContainer::GetNamesFromIDs(LPSPropTagArray* lppPropTags, LPGUID lpPropSetGuid, ULONG ulFlags, ULONG* lpcPropNames,
										  LPMAPINAMEID** lpppPropNames) {
	return M4LMAPIProp::GetNamesFromIDs(lppPropTags, lpPropSetGuid, ulFlags, lpcPropNames, lpppPropNames);
}

HRESULT M4LMAPIContainer::GetIDsFromNames(ULONG cPropNames, LPMAPINAMEID* lppPropNames, ULONG ulFlags, LPSPropTagArray* lppPropTags) {
	return M4LMAPIProp::GetIDsFromNames(cPropNames, lppPropNames, ulFlags, lppPropTags);
}

// iunknown passthru
ULONG M4LMAPIContainer::AddRef() {
	return M4LUnknown::AddRef();
}
ULONG M4LMAPIContainer::Release() {
	return M4LUnknown::Release();
}
HRESULT M4LMAPIContainer::QueryInterface(REFIID refiid, void **lpvoid) {
	TRACE_MAPILIB(TRACE_ENTRY, "M4LMAPIContainer::QueryInterface", "");
	HRESULT hr = hrSuccess;
	if ((refiid == IID_IMAPIContainer) || (refiid == IID_IMAPIProp) || (refiid == IID_IUnknown)) {
		AddRef();
		*lpvoid = (IMAPIContainer *)this;
		hr = hrSuccess;
	} else
		hr = MAPI_E_INTERFACE_NOT_SUPPORTED;

	TRACE_MAPILIB1(TRACE_RETURN, "M4LMAPIContainer::QueryInterface", "0x%08x", hr);
	return hr;
}

// 
// IABContainer
// 

M4LABContainer::M4LABContainer(const std::list<abEntry> &lABEntries) : m_lABEntries(lABEntries) {
}

M4LABContainer::~M4LABContainer() {
}

HRESULT M4LABContainer::CreateEntry(ULONG cbEntryID, LPENTRYID lpEntryID, ULONG ulCreateFlags, LPMAPIPROP* lppMAPIPropEntry) {
	return MAPI_E_NO_SUPPORT;
}

HRESULT M4LABContainer::CopyEntries(LPENTRYLIST lpEntries, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, ULONG ulFlags) {
	return MAPI_E_NO_SUPPORT;
}

HRESULT M4LABContainer::DeleteEntries(LPENTRYLIST lpEntries, ULONG ulFlags) {
	return MAPI_E_NO_SUPPORT;
}

HRESULT M4LABContainer::ResolveNames(LPSPropTagArray lpPropTagArray, ULONG ulFlags, LPADRLIST lpAdrList, LPFlagList lpFlagList) {
	return MAPI_E_NO_SUPPORT;
}

// 
// imapicontainer passthru
//
HRESULT M4LABContainer::GetContentsTable(ULONG ulFlags, LPMAPITABLE* lppTable) {
	return MAPI_E_NO_SUPPORT;
}

/** 
 * Merges all HierarchyTables from the providers passed in the constructor.
 * 
 * @param[in] ulFlags MAPI_UNICODE
 * @param[out] lppTable ECMemTable with combined contents from all providers
 * 
 * @return 
 */
HRESULT M4LABContainer::GetHierarchyTable(ULONG ulFlags, LPMAPITABLE* lppTable) {
	HRESULT hr = hrSuccess;
	ECMemTable *lpTable = NULL;
	ECMemTableView *lpTableView = NULL;
	std::list<abEntry>::const_iterator iter;
	ULONG n = 0;

	// make a list of all hierarchy tables, and create the combined column list
	std::list<LPMAPITABLE> lHierarchies;
	std::set<ULONG> stProps;
	LPSPropTagArray lpColumns = NULL;
	for (iter = m_lABEntries.begin(); iter != m_lABEntries.end(); iter++) {
		ULONG ulObjType;
		LPABCONT lpABContainer = NULL;
		LPMAPITABLE lpABHierarchy = NULL;
		LPSPropTagArray lpPropArray = NULL;

		hr = iter->lpABLogon->OpenEntry(0, NULL, &IID_IABContainer, 0, &ulObjType, (IUnknown**)&lpABContainer);
		if (hr != hrSuccess)
			goto next_container;

		hr = lpABContainer->GetHierarchyTable(ulFlags, &lpABHierarchy);
		if (hr != hrSuccess)
			goto next_container;

		hr = lpABHierarchy->QueryColumns(TBL_ALL_COLUMNS, &lpPropArray);
		if (hr != hrSuccess)
			goto next_container;

		std::copy(lpPropArray->aulPropTag, lpPropArray->aulPropTag + lpPropArray->cValues, std::inserter(stProps, stProps.begin()));
		lpABHierarchy->AddRef();
		lHierarchies.push_back(lpABHierarchy);

	next_container:
		if (lpABContainer)
			lpABContainer->Release();
		lpABContainer = NULL;

		if (lpABHierarchy)
			lpABHierarchy->Release();
		lpABHierarchy = NULL;

		if (lpPropArray)
			MAPIFreeBuffer(lpPropArray);
		lpPropArray = NULL;
	}

	// remove key row
	stProps.erase(PR_ROWID);

	hr = MAPIAllocateBuffer(CbNewSPropTagArray(stProps.size() + 1), (void**)&lpColumns);
	if (hr != hrSuccess)
		goto exit;

	lpColumns->cValues = stProps.size();
	std::copy(stProps.begin(), stProps.end(), lpColumns->aulPropTag);
	lpColumns->aulPropTag[lpColumns->cValues] = PR_NULL; // will be used for PR_ROWID

	hr = ECMemTable::Create(lpColumns, PR_ROWID, &lpTable);
	if(hr != hrSuccess)
		goto exit;

	// get enough columns from queryrows to add the PR_ROWID
	lpColumns->cValues++;

	n = 0;
	for (std::list<LPMAPITABLE>::iterator i = lHierarchies.begin(); i != lHierarchies.end(); i++) {
		LPSRowSet lpsRows = NULL;

		hr = (*i)->SetColumns(lpColumns, 0);
		if (hr != hrSuccess)
			goto exit;

		while (true) {
			hr = (*i)->QueryRows(1, 0, &lpsRows);
			if (hr != hrSuccess)
				goto exit;
			if (lpsRows->cRows == 0)
				break;

			lpsRows->aRow[0].lpProps[stProps.size()].ulPropTag = PR_ROWID;
			lpsRows->aRow[0].lpProps[stProps.size()].Value.ul = n++;

			hr = lpTable->HrModifyRow(ECKeyTable::TABLE_ROW_ADD, NULL, lpsRows->aRow[0].lpProps, lpsRows->aRow[0].cValues);

			FreeProws(lpsRows);
			lpsRows = NULL;

			if(hr != hrSuccess)
				goto exit;
		}
		if (lpsRows)
			FreeProws(lpsRows);
	}

	hr = lpTable->HrGetView(createLocaleFromName(""), ulFlags, &lpTableView);
	if(hr != hrSuccess)
		goto exit;
		
	hr = lpTableView->QueryInterface(IID_IMAPITable, (void **)lppTable);

exit:
	for (std::list<LPMAPITABLE>::iterator i = lHierarchies.begin(); i != lHierarchies.end(); i++)
		(*i)->Release();

	if (lpColumns)
		MAPIFreeBuffer(lpColumns);

	if (lpTableView)
		lpTableView->Release();

	if (lpTable)
		lpTable->Release();

	return hr;
}

HRESULT M4LABContainer::OpenEntry(ULONG cbEntryID, LPENTRYID lpEntryID, LPCIID lpInterface, ULONG ulFlags, ULONG* lpulObjType, LPUNKNOWN* lppUnk) {
	HRESULT hr = hrSuccess;
	std::list<abEntry>::const_iterator iter;
	LPABLOGON lpABLogon = NULL;
	MAPIUID muidEntry;

	if (cbEntryID < sizeof(MAPIUID) + 4 || !lpEntryID) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	// get the provider muid
	memcpy(&muidEntry, (LPBYTE)lpEntryID + 4, sizeof(MAPIUID));

	// locate provider
	for (iter = m_lABEntries.begin(); iter != m_lABEntries.end(); iter++) {
		if (memcmp(&muidEntry, &iter->muid, sizeof(MAPIUID)) == 0)
		{
			lpABLogon = iter->lpABLogon;
			break;
		}
	}
	if (!lpABLogon) {
		hr = MAPI_E_UNKNOWN_ENTRYID;
		goto exit;
	}

	// open root container of provider
	hr = lpABLogon->OpenEntry(cbEntryID, lpEntryID, lpInterface, ulFlags, lpulObjType, lppUnk);

exit:
	return hr;
}

HRESULT M4LABContainer::SetSearchCriteria(LPSRestriction lpRestriction, LPENTRYLIST lpContainerList, ULONG ulSearchFlags) {
	return MAPI_E_NO_SUPPORT;
}

HRESULT M4LABContainer::GetSearchCriteria(ULONG ulFlags, LPSRestriction* lppRestriction, LPENTRYLIST* lppContainerList, ULONG* lpulSearchState) {
	return MAPI_E_NO_SUPPORT;
}

// imapiprop passthru
HRESULT M4LABContainer::GetLastError(HRESULT hResult, ULONG ulFlags, LPMAPIERROR* lppMAPIError) {
	return M4LMAPIProp::GetLastError(hResult, ulFlags, lppMAPIError);
}

HRESULT M4LABContainer::SaveChanges(ULONG ulFlags) {
	return M4LMAPIProp::SaveChanges(ulFlags);
}

HRESULT M4LABContainer::GetProps(LPSPropTagArray lpPropTagArray, ULONG ulFlags, ULONG* lpcValues, LPSPropValue* lppPropArray) {
	return M4LMAPIProp::GetProps(lpPropTagArray, ulFlags, lpcValues, lppPropArray);
}

HRESULT M4LABContainer::GetPropList(ULONG ulFlags, LPSPropTagArray* lppPropTagArray) {
	return M4LMAPIProp::GetPropList(ulFlags, lppPropTagArray);
}

HRESULT M4LABContainer::OpenProperty(ULONG ulPropTag, LPCIID lpiid, ULONG ulInterfaceOptions, ULONG ulFlags, LPUNKNOWN* lppUnk) {
	return M4LMAPIProp::OpenProperty(ulPropTag, lpiid, ulInterfaceOptions, ulFlags, lppUnk);
}

HRESULT M4LABContainer::SetProps(ULONG cValues, LPSPropValue lpPropArray, LPSPropProblemArray* lppProblems) {
	return M4LMAPIProp::SetProps(cValues, lpPropArray, lppProblems);
}

HRESULT M4LABContainer::DeleteProps(LPSPropTagArray lpPropTagArray, LPSPropProblemArray* lppProblems) {
	return M4LMAPIProp::DeleteProps(lpPropTagArray, lppProblems);
}

HRESULT M4LABContainer::CopyTo(ULONG ciidExclude, LPCIID rgiidExclude, LPSPropTagArray lpExcludeProps, ULONG ulUIParam,
								 LPMAPIPROGRESS lpProgress, LPCIID lpInterface, LPVOID lpDestObj, ULONG ulFlags,
								 LPSPropProblemArray* lppProblems) {
	return M4LMAPIProp::CopyTo(ciidExclude, rgiidExclude, lpExcludeProps, ulUIParam, lpProgress, lpInterface, lpDestObj, ulFlags, lppProblems);
}

HRESULT M4LABContainer::CopyProps(LPSPropTagArray lpIncludeProps, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, LPCIID lpInterface,
									LPVOID lpDestObj, ULONG ulFlags, LPSPropProblemArray* lppProblems) {
	return M4LMAPIProp::CopyProps(lpIncludeProps, ulUIParam, lpProgress, lpInterface, lpDestObj, ulFlags, lppProblems);
}

HRESULT M4LABContainer::GetNamesFromIDs(LPSPropTagArray* lppPropTags, LPGUID lpPropSetGuid, ULONG ulFlags, ULONG* lpcPropNames,
										  LPMAPINAMEID** lpppPropNames) {
	return M4LMAPIProp::GetNamesFromIDs(lppPropTags, lpPropSetGuid, ulFlags, lpcPropNames, lpppPropNames);
}

HRESULT M4LABContainer::GetIDsFromNames(ULONG cPropNames, LPMAPINAMEID* lppPropNames, ULONG ulFlags, LPSPropTagArray* lppPropTags) {
	return M4LMAPIProp::GetIDsFromNames(cPropNames, lppPropNames, ulFlags, lppPropTags);
}

// iunknown passthru
ULONG M4LABContainer::AddRef() {
	return M4LUnknown::AddRef();
}
ULONG M4LABContainer::Release() {
	return M4LUnknown::Release();
}
HRESULT M4LABContainer::QueryInterface(REFIID refiid, void **lpvoid) {
	TRACE_MAPILIB(TRACE_ENTRY, "M4LABContainer::QueryInterface", "");
	HRESULT hr = hrSuccess;
	if ((refiid == IID_IABContainer) || (refiid == IID_IMAPIContainer) || (refiid == IID_IMAPIProp) || (refiid == IID_IUnknown)) {
		AddRef();
		*lpvoid = (IABContainer *)this;
		hr = hrSuccess;
	} else
		hr = MAPI_E_INTERFACE_NOT_SUPPORTED;

	TRACE_MAPILIB1(TRACE_RETURN, "M4LABContainer::QueryInterface", "0x%08x", hr);
	return hr;
}
