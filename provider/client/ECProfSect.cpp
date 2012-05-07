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
#include "Zarafa.h"
#include "ECGuid.h"

#include "ECDebug.h"

#include "ECProfSect.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

ECProfSect::ECProfSect(LPPROFSECT lpProfSect)
{
	m_lpProfSect = lpProfSect;
	m_lpProfSect->AddRef();
}

ECProfSect::~ECProfSect(void)
{
	m_lpProfSect->Release();
}

HRESULT ECProfSect::Create(LPPROFSECT lpProfSect, ECProfSect **lppObj)
{
	HRESULT hr = hrSuccess;
	ECProfSect *lpObj = NULL;

	if(lpProfSect == NULL || lppObj == NULL){
		hr = MAPI_E_INVALID_OBJECT;
		goto exit;
	}

	lpObj = new ECProfSect(lpProfSect);

	hr = lpObj->QueryInterface(IID_ECUnknown, (void **)lppObj);

exit:
	return hr;
}

HRESULT	ECProfSect::QueryInterface(REFIID refiid, void **lppInterface)
{
	REGISTER_INTERFACE(IID_ECUnknown, this);

	REGISTER_INTERFACE(IID_IProfSect, &this->m_xProfSect);
	REGISTER_INTERFACE(IID_IUnknown, &this->m_xProfSect);

	return MAPI_E_INTERFACE_NOT_SUPPORTED;
}

HRESULT ECProfSect::GetLastError(HRESULT hResult, ULONG ulFlags, LPMAPIERROR FAR * lppMAPIError) {
	return m_lpProfSect->GetLastError(hResult, ulFlags, lppMAPIError);
}

HRESULT ECProfSect::SaveChanges(ULONG ulFlags) {
	return m_lpProfSect->SaveChanges(ulFlags);
}

HRESULT ECProfSect::GetProps(LPSPropTagArray lpPropTagArray, ULONG ulFlags, ULONG FAR * lpcValues, LPSPropValue FAR * lppPropArray) {
	return m_lpProfSect->GetProps(lpPropTagArray, ulFlags, lpcValues, lppPropArray);
}

HRESULT ECProfSect::GetPropList(ULONG ulFlags, LPSPropTagArray FAR * lppPropTagArray) {
	return m_lpProfSect->GetPropList(ulFlags, lppPropTagArray);
}

HRESULT ECProfSect::OpenProperty(ULONG ulPropTag, LPCIID lpiid, ULONG ulInterfaceOptions, ULONG ulFlags, LPUNKNOWN FAR * lppUnk) {
	return m_lpProfSect->OpenProperty(ulPropTag, lpiid, ulInterfaceOptions, ulFlags, lppUnk);
}

HRESULT ECProfSect::SetProps(ULONG cValues, LPSPropValue lpPropArray, LPSPropProblemArray FAR * lppProblems) {
	return m_lpProfSect->SetProps(cValues, lpPropArray, lppProblems);
}

HRESULT ECProfSect::DeleteProps(LPSPropTagArray lpPropTagArray, LPSPropProblemArray FAR * lppProblems) {
	return m_lpProfSect->DeleteProps(lpPropTagArray, lppProblems);
}

HRESULT ECProfSect::CopyTo(ULONG ciidExclude, LPCIID rgiidExclude, LPSPropTagArray lpExcludeProps, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, LPCIID lpInterface, LPVOID lpDestObj, ULONG ulFlags, LPSPropProblemArray FAR * lppProblems) {
	return m_lpProfSect->CopyTo(ciidExclude, rgiidExclude, lpExcludeProps, ulUIParam, lpProgress, lpInterface, lpDestObj, ulFlags, lppProblems);
}

HRESULT ECProfSect::CopyProps(LPSPropTagArray lpIncludeProps, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, LPCIID lpInterface, LPVOID lpDestObj, ULONG ulFlags, LPSPropProblemArray FAR * lppProblems) {
	return m_lpProfSect->CopyProps(lpIncludeProps, ulUIParam, lpProgress, lpInterface, lpDestObj, ulFlags, lppProblems);
}

HRESULT ECProfSect::GetNamesFromIDs(LPSPropTagArray FAR * lppPropTags, LPGUID lpPropSetGuid, ULONG ulFlags, ULONG FAR * lpcPropNames, LPMAPINAMEID FAR * FAR * lpppPropNames) {
	return m_lpProfSect->GetNamesFromIDs(lppPropTags, lpPropSetGuid, ulFlags, lpcPropNames, lpppPropNames);
}

HRESULT ECProfSect::GetIDsFromNames(ULONG cPropNames, LPMAPINAMEID FAR * lppPropNames, ULONG ulFlags, LPSPropTagArray FAR * lppPropTags) {
	return m_lpProfSect->GetIDsFromNames(cPropNames, lppPropNames, ulFlags, lppPropTags);
}


////////////////////////////////////////////
// Interface IProfSect

HRESULT __stdcall ECProfSect::xProfSect::QueryInterface(REFIID refiid, void ** lppInterface)
{
	TRACE_MAPI(TRACE_ENTRY, "IProfSect::QueryInterface", "%s", DBGGUIDToString(refiid).c_str());
	METHOD_PROLOGUE_(ECProfSect , ProfSect);
	HRESULT hr = pThis->QueryInterface(refiid, lppInterface);
	TRACE_MAPI(TRACE_RETURN, "IProfSect::QueryInterface", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

ULONG __stdcall ECProfSect::xProfSect::AddRef()
{
	TRACE_MAPI(TRACE_ENTRY, "IProfSect::AddRef", "");
	METHOD_PROLOGUE_(ECProfSect , ProfSect);
	return pThis->AddRef();
}

ULONG __stdcall ECProfSect::xProfSect::Release()
{
	TRACE_MAPI(TRACE_ENTRY, "IProfSect::Release", "");
	METHOD_PROLOGUE_(ECProfSect , ProfSect);
	return pThis->Release();
}

HRESULT __stdcall ECProfSect::xProfSect::GetLastError(HRESULT hError, ULONG ulFlags,
    LPMAPIERROR * lppMapiError)
{
	TRACE_MAPI(TRACE_ENTRY, "IProfSect::GetLastError", "");
	METHOD_PROLOGUE_(ECProfSect , ProfSect);
	HRESULT hr = pThis->GetLastError(hError, ulFlags, lppMapiError);
	TRACE_MAPI(TRACE_RETURN, "IProfSect::GetLastError", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT __stdcall ECProfSect::xProfSect::SaveChanges(ULONG ulFlags)
{
	TRACE_MAPI(TRACE_ENTRY, "IProfSect::SaveChanges", "");
	METHOD_PROLOGUE_(ECProfSect , ProfSect);
	HRESULT hr = pThis->SaveChanges(ulFlags);
	TRACE_MAPI(TRACE_RETURN, "IProfSect::SaveChanges", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT __stdcall ECProfSect::xProfSect::GetProps(LPSPropTagArray lpPropTagArray, ULONG ulFlags, ULONG FAR * lpcValues, LPSPropValue FAR * lppPropArray)
{
	TRACE_MAPI(TRACE_ENTRY, "IProfSect::GetProps", "%s, flags=%08X", PropNameFromPropTagArray(lpPropTagArray).c_str(), ulFlags);
	METHOD_PROLOGUE_(ECProfSect , ProfSect);
	HRESULT hr = pThis->GetProps(lpPropTagArray, ulFlags, lpcValues, lppPropArray);
	TRACE_MAPI(TRACE_RETURN, "IProfSect::GetProps", "%s\n %s", GetMAPIErrorDescription(hr).c_str(), PropNameFromPropArray(*lpcValues, *lppPropArray).c_str());
	return hr;
}

HRESULT __stdcall ECProfSect::xProfSect::GetPropList(ULONG ulFlags, LPSPropTagArray FAR * lppPropTagArray)
{
	TRACE_MAPI(TRACE_ENTRY, "IProfSect::GetPropList", "");
	METHOD_PROLOGUE_(ECProfSect , ProfSect);
	HRESULT hr = pThis->GetPropList(ulFlags, lppPropTagArray);
	TRACE_MAPI(TRACE_RETURN, "IProfSect::GetPropList", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT __stdcall ECProfSect::xProfSect::OpenProperty(ULONG ulPropTag, LPCIID lpiid, ULONG ulInterfaceOptions, ULONG ulFlags, LPUNKNOWN FAR * lppUnk)
{
	TRACE_MAPI(TRACE_ENTRY, "IProfSect::OpenProperty", "proptag=%s, flags=%d, lpiid=%s, InterfaceOptions=%d", PropNameFromPropTag(ulPropTag).c_str(), ulFlags, (lpiid)?DBGGUIDToString(*lpiid).c_str():"NULL", ulInterfaceOptions);
	METHOD_PROLOGUE_(ECProfSect , ProfSect);
	HRESULT hr = pThis->OpenProperty(ulPropTag, lpiid, ulInterfaceOptions, ulFlags, lppUnk);
	TRACE_MAPI(TRACE_RETURN, "IProfSect::OpenProperty", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT __stdcall ECProfSect::xProfSect::SetProps(ULONG cValues, LPSPropValue lpPropArray, LPSPropProblemArray FAR * lppProblems)
{
	TRACE_MAPI(TRACE_ENTRY, "IProfSect::SetProps", "%s", PropNameFromPropArray(cValues, lpPropArray).c_str());
	METHOD_PROLOGUE_(ECProfSect , ProfSect);
	HRESULT hr = pThis->SetProps(cValues, lpPropArray, lppProblems);
	TRACE_MAPI(TRACE_RETURN, "IProfSect::SetProps", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT __stdcall ECProfSect::xProfSect::DeleteProps(LPSPropTagArray lpPropTagArray, LPSPropProblemArray FAR * lppProblems)
{
	TRACE_MAPI(TRACE_ENTRY, "IProfSect::DeleteProps", "%s", PropNameFromPropTagArray(lpPropTagArray).c_str());
	METHOD_PROLOGUE_(ECProfSect , ProfSect);
	HRESULT hr = pThis->DeleteProps(lpPropTagArray, lppProblems);
	TRACE_MAPI(TRACE_RETURN, "IProfSect::DeleteProps", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT __stdcall ECProfSect::xProfSect::CopyTo(ULONG ciidExclude, LPCIID rgiidExclude, LPSPropTagArray lpExcludeProps, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, LPCIID lpInterface, LPVOID lpDestObj, ULONG ulFlags, LPSPropProblemArray FAR * lppProblems)
{
	TRACE_MAPI(TRACE_ENTRY, "IProfSect::CopyTo", "");
	METHOD_PROLOGUE_(ECProfSect , ProfSect);
	HRESULT hr = pThis->CopyTo(ciidExclude, rgiidExclude, lpExcludeProps, ulUIParam, lpProgress, lpInterface, lpDestObj, ulFlags, lppProblems);
	TRACE_MAPI(TRACE_RETURN, "IProfSect::CopyTo", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT __stdcall ECProfSect::xProfSect::CopyProps(LPSPropTagArray lpIncludeProps, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, LPCIID lpInterface, LPVOID lpDestObj, ULONG ulFlags, LPSPropProblemArray FAR * lppProblems)
{
	TRACE_MAPI(TRACE_ENTRY, "IProfSect::CopyProps", "");
	METHOD_PROLOGUE_(ECProfSect , ProfSect);
	HRESULT hr = pThis->CopyProps(lpIncludeProps, ulUIParam, lpProgress, lpInterface, lpDestObj, ulFlags, lppProblems);
	TRACE_MAPI(TRACE_RETURN, "IProfSect::CopyProps", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT __stdcall ECProfSect::xProfSect::GetNamesFromIDs(LPSPropTagArray * pptaga, LPGUID lpguid, ULONG ulFlags, ULONG * pcNames, LPMAPINAMEID ** pppNames)
{
	TRACE_MAPI(TRACE_ENTRY, "IProfSect::GetNamesFromIDs", "");
	METHOD_PROLOGUE_(ECProfSect , ProfSect);
	HRESULT hr = pThis->GetNamesFromIDs(pptaga, lpguid, ulFlags, pcNames, pppNames);
	TRACE_MAPI(TRACE_RETURN, "IProfSect::GetNamesFromIDs", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT __stdcall ECProfSect::xProfSect::GetIDsFromNames(ULONG cNames, LPMAPINAMEID * ppNames, ULONG ulFlags, LPSPropTagArray * pptaga)
{
	TRACE_MAPI(TRACE_ENTRY, "IProfSect::GetIDsFromNames", "");
	METHOD_PROLOGUE_(ECProfSect , ProfSect);
	HRESULT hr = pThis->GetIDsFromNames(cNames, ppNames, ulFlags, pptaga);
	TRACE_MAPI(TRACE_RETURN, "IProfSect::GetIDsFromNames", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}
