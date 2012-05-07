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
#include "ECFreeBusyUpdate.h"
#include "freebusytags.h"

#include "freebusyutil.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

ECFreeBusyUpdate::ECFreeBusyUpdate(IMessage* lpMessage)
{
	m_lpMessage = lpMessage;

	if(m_lpMessage)
		m_lpMessage->AddRef();
}

ECFreeBusyUpdate::~ECFreeBusyUpdate(void)
{
	if(m_lpMessage)
		m_lpMessage->Release();
}

HRESULT ECFreeBusyUpdate::Create(IMessage* lpMessage, ECFreeBusyUpdate **lppECFreeBusyUpdate)
{
	HRESULT hr = hrSuccess;
	ECFreeBusyUpdate *lpECFreeBusyUpdate = NULL;

	lpECFreeBusyUpdate = new ECFreeBusyUpdate(lpMessage);

	hr = lpECFreeBusyUpdate->QueryInterface(IID_ECFreeBusyUpdate, (void **)lppECFreeBusyUpdate);

	if(hr != hrSuccess)
		delete lpECFreeBusyUpdate;

	return hr;
}


HRESULT ECFreeBusyUpdate::QueryInterface(REFIID refiid, void** lppInterface)
{
	REGISTER_INTERFACE(IID_ECFreeBusyUpdate, this);
	REGISTER_INTERFACE(IID_ECUnknown, this);

	REGISTER_INTERFACE(IID_IFreeBusyUpdate, &this->m_xFreeBusyUpdate);
	REGISTER_INTERFACE(IID_IUnknown, &this->m_xFreeBusyUpdate);

	return MAPI_E_INTERFACE_NOT_SUPPORTED;
}


HRESULT ECFreeBusyUpdate::Reload()
{
	return S_OK;
}

HRESULT ECFreeBusyUpdate::PublishFreeBusy(FBBlock_1 *lpBlocks, ULONG nBlocks)
{
	HRESULT hr = S_OK;

	if(nBlocks > 0 && lpBlocks == NULL)
	{
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	for(ULONG i=0; i < nBlocks; i++)
	{
		m_fbBlockList.Add(&lpBlocks[i]);
	}

exit:
	return hr;
}

HRESULT ECFreeBusyUpdate::RemoveAppt()
{
	
	return S_OK;
}

HRESULT ECFreeBusyUpdate::ResetPublishedFreeBusy()
{
	m_fbBlockList.Clear();

	return S_OK;
}

HRESULT ECFreeBusyUpdate::ChangeAppt()
{
	return S_OK;
}

HRESULT ECFreeBusyUpdate::SaveChanges(FILETIME ftStart, FILETIME ftEnd)
{
	HRESULT			hr = hrSuccess;
	ULONG			cValues = 0;
	ULONG			cProps = 0;
	ULONG			ulMonths;
	LPSPropValue	lpPropArray = NULL;
	LPSPropValue	lpPropFBDataArray = NULL;
	LONG			rtmStart = 0;
	LONG			rtmEnd = 0;
	FILETIME		ft;	
	time_t			tmUnixStart;
	time_t			tmUnixEnd;
	struct tm		tmStart;
	struct tm		tmEnd;

	SizedSPropTagArray(8, sPropsFBDelete) = {
		8,
		{
			PR_FREEBUSY_ALL_EVENTS,
			PR_FREEBUSY_ALL_MONTHS,
			PR_FREEBUSY_BUSY_EVENTS,
			PR_FREEBUSY_BUSY_MONTHS,
			PR_FREEBUSY_OOF_EVENTS,
			PR_FREEBUSY_OOF_MONTHS,
			PR_FREEBUSY_TENTATIVE_EVENTS,
			PR_FREEBUSY_TENTATIVE_MONTHS
		}
	};


	FileTimeToRTime(&ftStart, &rtmStart);
	FileTimeToRTime(&ftEnd, &rtmEnd);

	if(m_lpMessage == NULL)
	{
		hr = MAPI_E_INVALID_OBJECT;
		goto exit;
	}

	if((ULONG)rtmStart > (ULONG)rtmEnd)
	{
		hr = MAPI_E_BAD_VALUE;
		goto exit;
	}

	GetSystemTimeAsFileTime(&ft);

	// Restrict on start and end date
	m_fbBlockList.Restrict(rtmStart, rtmEnd);

	//Calculate months
	RTimeToUnixTime(rtmStart, &tmUnixStart);
	RTimeToUnixTime(rtmEnd, &tmUnixEnd);

	tmStart = *gmtime(&tmUnixStart);
	tmEnd = *gmtime(&tmUnixEnd);

	ulMonths = DiffYearMonthToMonth(&tmStart, &tmEnd);
	if(ulMonths == 0)
		ulMonths++;

	cValues = 9;
	cProps = 0;
	MAPIAllocateBuffer(sizeof(SPropValue) * cValues, (void**)&lpPropArray);

	lpPropArray[cProps].ulPropTag = PR_FREEBUSY_LAST_MODIFIED;
	lpPropArray[cProps++].Value.ft = ft;

	lpPropArray[cProps].ulPropTag = PR_FREEBUSY_START_RANGE;
	lpPropArray[cProps++].Value.l = rtmStart;

	lpPropArray[cProps].ulPropTag = PR_FREEBUSY_END_RANGE;
	lpPropArray[cProps++].Value.l = rtmEnd;

	lpPropArray[cProps].ulPropTag = PR_FREEBUSY_NUM_MONTHS;
	lpPropArray[cProps++].Value.l = ulMonths;	

	hr = m_lpMessage->SetProps(cProps, lpPropArray, NULL);
	if(hr != hrSuccess)
		goto exit;

	// Delete all free/busy data properties	
	hr = m_lpMessage->DeleteProps((LPSPropTagArray)&sPropsFBDelete, NULL);
  	if(hr != hrSuccess)
		goto exit;


	if(CreateFBProp(fbZarafaAllBusy, ulMonths, PR_FREEBUSY_ALL_MONTHS, PR_FREEBUSY_ALL_EVENTS, &m_fbBlockList, &lpPropFBDataArray) == hrSuccess)
	{
		hr = m_lpMessage->SetProps(2, lpPropFBDataArray, NULL);
		if(hr != hrSuccess)
			goto exit;

		if(lpPropFBDataArray){ 
			MAPIFreeBuffer(lpPropFBDataArray); 
			lpPropFBDataArray = NULL;
		}
	}

	if(CreateFBProp(fbBusy, ulMonths, PR_FREEBUSY_BUSY_MONTHS, PR_FREEBUSY_BUSY_EVENTS, &m_fbBlockList, &lpPropFBDataArray) == hrSuccess)
	{
		hr = m_lpMessage->SetProps(2, lpPropFBDataArray, NULL);
		if(hr != hrSuccess)
			goto exit;

		if(lpPropFBDataArray){
			MAPIFreeBuffer(lpPropFBDataArray);
			lpPropFBDataArray = NULL;
		}
	}
	
	if(CreateFBProp(fbTentative, ulMonths, PR_FREEBUSY_TENTATIVE_MONTHS,PR_FREEBUSY_TENTATIVE_EVENTS, &m_fbBlockList, &lpPropFBDataArray) == hrSuccess)
	{
		hr = m_lpMessage->SetProps(2, lpPropFBDataArray, NULL);
		if(hr != hrSuccess)
			goto exit;

		if(lpPropFBDataArray){ MAPIFreeBuffer(lpPropFBDataArray); lpPropFBDataArray = NULL; }
	}

	if(CreateFBProp(fbOutOfOffice, ulMonths, PR_FREEBUSY_OOF_MONTHS,PR_FREEBUSY_OOF_EVENTS, &m_fbBlockList, &lpPropFBDataArray) == hrSuccess)
	{
		hr = m_lpMessage->SetProps(2, lpPropFBDataArray, NULL);
		if(hr != hrSuccess)
			goto exit;

		if(lpPropFBDataArray){ MAPIFreeBuffer(lpPropFBDataArray); lpPropFBDataArray = NULL; }
	}

	hr = m_lpMessage->SaveChanges(KEEP_OPEN_READWRITE);
	if(hr != hrSuccess)
		goto exit;

exit:
	m_fbBlockList.Reset();

	if(lpPropArray)
		MAPIFreeBuffer(lpPropArray);

	if(lpPropFBDataArray)
		MAPIFreeBuffer(lpPropFBDataArray);

	return hr;
}

HRESULT ECFreeBusyUpdate::GetFBTimes()
{
	return S_OK;
}

HRESULT ECFreeBusyUpdate::Intersect()
{
	return S_OK;
}


//////////////////////////////////////////////////////////////////
// Interfaces
//		IUnknown
//		IFreeBusyUpdate
//

HRESULT __stdcall ECFreeBusyUpdate::xFreeBusyUpdate::QueryInterface(REFIID refiid, void** lppInterface)
{
	TRACE_MAPI(TRACE_ENTRY, "IFreeBusyUpdate::QueryInterface", "");
	METHOD_PROLOGUE_(ECFreeBusyUpdate, FreeBusyUpdate);
	HRESULT hr = pThis->QueryInterface(refiid, lppInterface);
	TRACE_MAPI(TRACE_RETURN, "IFreeBusyUpdate::QueryInterface", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

ULONG __stdcall ECFreeBusyUpdate::xFreeBusyUpdate::AddRef()
{
	TRACE_MAPI(TRACE_ENTRY, "IFreeBusyUpdate::AddRef", "");
	METHOD_PROLOGUE_(ECFreeBusyUpdate , FreeBusyUpdate);
	return pThis->AddRef();
}

ULONG __stdcall ECFreeBusyUpdate::xFreeBusyUpdate::Release()
{
	TRACE_MAPI(TRACE_ENTRY, "IFreeBusyUpdate::Release", "");
	METHOD_PROLOGUE_(ECFreeBusyUpdate, FreeBusyUpdate);
	return pThis->Release();
}

HRESULT __stdcall ECFreeBusyUpdate::xFreeBusyUpdate::Reload()
{
	TRACE_MAPI(TRACE_ENTRY, "IFreeBusyUpdate::Reload", "");
	METHOD_PROLOGUE_(ECFreeBusyUpdate, FreeBusyUpdate);
	HRESULT hr = pThis->Reload();
	TRACE_MAPI(TRACE_RETURN, "IFreeBusyUpdate::Reload", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT __stdcall ECFreeBusyUpdate::xFreeBusyUpdate::PublishFreeBusy(FBBlock_1 *lpBlocks, ULONG nBlocks)
{
	TRACE_MAPI(TRACE_ENTRY, "IFreeBusyUpdate::PublishFreeBusy", "%s", GetDebugFBBlock(nBlocks, lpBlocks).c_str());
	METHOD_PROLOGUE_(ECFreeBusyUpdate, FreeBusyUpdate);
	HRESULT hr = pThis->PublishFreeBusy(lpBlocks, nBlocks);
	TRACE_MAPI(TRACE_RETURN, "IFreeBusyUpdate::PublishFreeBusy", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT __stdcall ECFreeBusyUpdate::xFreeBusyUpdate::RemoveAppt()
{
	TRACE_MAPI(TRACE_ENTRY, "IFreeBusyUpdate::RemoveAppt", "");
	METHOD_PROLOGUE_(ECFreeBusyUpdate, FreeBusyUpdate);
	HRESULT hr = pThis->RemoveAppt();
	TRACE_MAPI(TRACE_RETURN, "IFreeBusyUpdate::RemoveAppt", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT __stdcall ECFreeBusyUpdate::xFreeBusyUpdate::ResetPublishedFreeBusy()
{
	TRACE_MAPI(TRACE_ENTRY, "IFreeBusyUpdate::ResetPublishedFreeBusy", "");
	METHOD_PROLOGUE_(ECFreeBusyUpdate, FreeBusyUpdate);
	HRESULT hr = pThis->ResetPublishedFreeBusy();
	TRACE_MAPI(TRACE_RETURN, "IFreeBusyUpdate::ResetPublishedFreeBusy", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT __stdcall ECFreeBusyUpdate::xFreeBusyUpdate::ChangeAppt()
{
	TRACE_MAPI(TRACE_ENTRY, "IFreeBusyUpdate::ChangeAppt", "");
	METHOD_PROLOGUE_(ECFreeBusyUpdate, FreeBusyUpdate);
	HRESULT hr = pThis->ChangeAppt();
	TRACE_MAPI(TRACE_RETURN, "IFreeBusyUpdate::ChangeAppt", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT __stdcall ECFreeBusyUpdate::xFreeBusyUpdate::SaveChanges(FILETIME ftBegin, FILETIME ftEnd)
{
	TRACE_MAPI(TRACE_ENTRY, "IFreeBusyUpdate::SaveChanges", "");
	METHOD_PROLOGUE_(ECFreeBusyUpdate, FreeBusyUpdate);
	HRESULT hr = pThis->SaveChanges(ftBegin, ftEnd);
	TRACE_MAPI(TRACE_RETURN, "IFreeBusyUpdate::SaveChanges", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT __stdcall ECFreeBusyUpdate::xFreeBusyUpdate::GetFBTimes()
{
	TRACE_MAPI(TRACE_ENTRY, "IFreeBusyUpdate::GetFBTimes", "");
	METHOD_PROLOGUE_(ECFreeBusyUpdate, FreeBusyUpdate);
	HRESULT hr = pThis->GetFBTimes();
	TRACE_MAPI(TRACE_RETURN, "IFreeBusyUpdate::GetFBTimes", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT __stdcall ECFreeBusyUpdate::xFreeBusyUpdate::Intersect()
{
	TRACE_MAPI(TRACE_ENTRY, "IFreeBusyUpdate::Intersect", "");
	METHOD_PROLOGUE_(ECFreeBusyUpdate, FreeBusyUpdate);
	HRESULT hr = pThis->Intersect();
	TRACE_MAPI(TRACE_RETURN, "IFreeBusyUpdate::Intersect", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}
