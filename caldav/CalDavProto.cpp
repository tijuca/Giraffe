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
#include "PublishFreeBusy.h"
#include "CalDavProto.h"

using namespace std;

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


/**
 * Maping of caldav properties to Mapi properties
 */
struct sMymap {
	unsigned int ulPropTag;
	char *name;
} sPropMap[] = {
	{ PR_LOCAL_COMMIT_TIME_MAX, "getctag" },
	{ PR_LAST_MODIFICATION_TIME, "getetag" },
	{ PR_DISPLAY_NAME_W, "displayname" },
	{ PR_CONTAINER_CLASS_A, "resourcetype" },
	{ PR_DISPLAY_NAME_W, "owner" },
	{ PR_ENTRYID, "calendar-data" },
	{ PR_COMMENT_W, "calendar-description" },
	{ PR_DISPLAY_TYPE, "calendar-user-type" },
	{ PR_SMTP_ADDRESS_W, "email-address-set" },
	{ PR_SMTP_ADDRESS_W, "calendar-user-address-set" },
	{ PR_DISPLAY_NAME_W, "first-name" },
	{ PR_DISPLAY_TYPE, "record-type" }
};

/**
 * Retrieves the Mapi Property from caldav property
 * @param[in]	name	ical property name
 * @retval		ULONG	Proptery tag of corresponding mapi property
 */
ULONG GetPropmap(char *name)
{
	for(unsigned int i = 0; i < sizeof(sPropMap)/sizeof(sPropMap[0]); i++)
	{
		if (strcmp(name, sPropMap[i].name) == 0)
			return sPropMap[i].ulPropTag;
	}

	return 0;
}
/**
 * Default constructor
 * @param[in]	lpRequest	Pointer to Http class object
 * @param[in]	lpSession	Pointer to Mapi session object
 * @param[in]	lpLogger	Pointer to ECLogger object
 * @param[in]	strSrvTz	String specifying the server timezone, set in ical.cfg
 * @param[in]	strCharset	String specifying the default charset of the http response
 */
CalDAV::CalDAV(Http *lpRequest, IMAPISession *lpSession, ECLogger *lpLogger, std::string strSrvTz, std::string strCharset ) : WebDav(lpRequest, lpSession, lpLogger , strSrvTz, strCharset)
{

}
/**
 * Default Destructor
 */
CalDAV::~CalDAV()
{
}

/**
 * Process all the caldav requests
 * @param[in]	strMethod	Name of the http request(e.g PROPFIND, REPORT..)
 * @return		MAPI error code
 */
HRESULT CalDAV::HrHandleCommand(const std::string &strMethod)
{
	HRESULT hr = hrSuccess;

	hr = ProtocolBase::HrHandleCommand(strMethod);
	if (hr != hrSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Folder named '%ls' not found, error code : 0x%08X", m_wstrFldName.c_str(), hr);
		goto exit;
	}
	
	// force check  for folder for the following requests and ignore folders for rest
	if(!m_lpUsrFld &&  (!strMethod.compare("PUT") || !strMethod.compare("DELETE") || !strMethod.compare("MOVE")))
	{
		m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Folder named '%ls' not found, error code : 0x%08X", m_wstrFldName.c_str(), hr);
		goto exit;
	}

	if(!strMethod.compare("PROPFIND")) {
		hr = HrPropfind();
	}
	else if(!strMethod.compare("REPORT")) {
		hr = HrReport();
	}
	else if(!strMethod.compare("PUT")) {
		hr = HrPut();
	}
	else if(!strMethod.compare("DELETE")) {
		hr = HrDelete();
	}
	else if(!strMethod.compare("MKCALENDAR")) {
		hr = HrMkCalendar();
	}
	else if(!strMethod.compare("PROPPATCH")) {
		hr = HrPropPatch();
	}
	else if (!strMethod.compare("POST"))
	{
		hr = HrPost();
	}
	else if (!strMethod.compare("MOVE"))
	{
		hr = HrMove();
	}
	else
	{
		m_lpRequest->HrResponseHeader(501, "Not Implemented");
	}

exit:
	if (hr != hrSuccess)
		m_lpRequest->HrResponseHeader(400, "Bad Request");

	return hr;
}

/**
 * Process the PROPFIND request in general
 * @return	HRESULT
 */
HRESULT CalDAV::HrPropfind()
{
	HRESULT hr = hrSuccess;

	hr = WebDav::HrPropfind();
	if (hr != hrSuccess)
		goto exit;

exit:
	return hr;
}

/**
 * Handles the PROPFIND request, identifies the type of PROPFIND request
 *
 * @param[in]	lpsDavProp			Pointer to structure cotaining info about the PROPFIND request
 * @param[out]	lpsDavMulStatus		Response generated for the PROPFIND request
 * @return		HRESULT
 */
HRESULT CalDAV::HrHandlePropfind(WEBDAVREQSTPROPS *lpsDavProp, WEBDAVMULTISTATUS *lpsDavMulStatus)
{
	HRESULT hr = hrSuccess;
	ULONG ulDepth = 0;
	
	m_lpRequest->HrGetDepth(&ulDepth);

	if (ulDepth == 0 || m_wstrFldName.compare(L"Inbox") == 0 || m_wstrFldName.compare(L"Outbox") == 0 )
	{
		// Retrieve Properties
		hr = HrHandlePropfindRoot(lpsDavProp, lpsDavMulStatus);
	}
	else if (m_blIsCLMAC && ulDepth == 1 && (m_wstrFldName.empty() || (m_ulFolderFlag & SINGLE_FOLDER)))
	{	
		// Retrieve list of calendars
		hr = HrListCalendar(lpsDavProp, lpsDavMulStatus);
	}
	else
	{
		// Retrieve the Calendar entries list
		hr = HrListCalEntries(lpsDavProp, lpsDavMulStatus);
	}

	return hr;
}

/**
 * Handles the Depth 0 PROPFIND request
 *
 * The client requets for information about the user,store and folder by using this request
 *
 * @param[in]	sDavReqstProps		Pointer to structure cotaining info about properties requested by client
 * @param[in]	lpsDavMulStatus		Pointer to structure cotaining response to the request
 * @return		HRESULT
 */
HRESULT CalDAV::HrHandlePropfindRoot(WEBDAVREQSTPROPS *sDavReqstProps, WEBDAVMULTISTATUS *lpsDavMulStatus)
{
	HRESULT hr = hrSuccess;
	std::wstring wstrReqPath;
	std::list<WEBDAVPROPERTY>::iterator iter;		
	WEBDAVPROP *lpsDavProp = NULL;
	WEBDAVRESPONSE sDavResp;
	IMAPIProp *lpMapiProp = NULL;
	LPSPropTagArray lpPropTagArr = NULL;
	LPSPropValue lpSpropVal = NULL;
	ULONG cbsize = 0;

	lpsDavProp = &(sDavReqstProps->sProp);

	// number of properties requested by client.
	cbsize = lpsDavProp->lstProps.size();
	
	m_lpRequest->HrGetUrl(&wstrReqPath);

	if(m_ulUrlFlag & REQ_PUBLIC)
		m_wstrCalHome = L"/caldav/public/" + m_wstrFldName;
	else
		m_wstrCalHome = L"/caldav/" + (m_wstrFldOwner.empty() ? m_wstrUser : m_wstrFldOwner) + L"/" + m_wstrFldName;
	
	if (m_ulUrlFlag & REQ_PUBLIC && m_wstrFldName.empty())
		lpMapiProp = m_lpDefStore;
	else if(m_blIsCLMAC && (m_wstrFldName.empty() || m_wstrFldName.compare(L"Inbox") == 0 || m_wstrFldName.compare(L"Outbox") == 0))
		lpMapiProp = m_lpActiveStore;
	else if(m_lpUsrFld)
		lpMapiProp = m_lpUsrFld;
	else
	{
		m_lpLogger->Log( EC_LOGLEVEL_ERROR,"Error Folder named '%ls' of user '%ls' not found, error code 0x%08X", m_wstrFldName.c_str(), m_wstrUser.c_str(), hr);
		goto exit;
	}
	
	hr = MAPIAllocateBuffer(CbNewSPropTagArray(cbsize), (void **) &lpPropTagArr);
	if (hr != hrSuccess)
	{
		m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Cannot allocate memory");
		goto exit;
	}

	lpPropTagArr->cValues = cbsize;

	// Get corresponding mapi properties.
	iter = lpsDavProp->lstProps.begin();
	for(int i = 0; iter != lpsDavProp->lstProps.end(); iter++, i++)
	{
		lpPropTagArr->aulPropTag[i] = GetPropmap((char*)iter->sPropName.strPropname.c_str());
	}

	hr = lpMapiProp->GetProps((LPSPropTagArray)lpPropTagArr, 0, &cbsize, &lpSpropVal);
	if (hr == MAPI_W_ERRORS_RETURNED)
		hr = hrSuccess;		// ignoring some items
	else if (hr != hrSuccess)
	{
		m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Error in GetProps for user %ls, error code : 0x%08X", m_wstrUser.c_str(), hr);
		goto exit;
	}

	HrSetDavPropName(&(sDavResp.sPropName), "response", CALDAVNSDEF);

	HrSetDavPropName(&(sDavResp.sHRef.sPropName), "href", CALDAVNSDEF);
	sDavResp.sHRef.wstrValue = wstrReqPath;

	// map values and properties in WEBDAVRESPONSE structure.
	hr = HrMapValtoStruct(lpSpropVal, cbsize, NULL, PROPFIND_ROOT, &(lpsDavProp->lstProps), &sDavResp);
	if (hr != hrSuccess)
		goto exit;
	

	HrSetDavPropName(&(lpsDavMulStatus->sPropName), "multistatus", CALDAVNSDEF);
	lpsDavMulStatus->lstResp.push_back(sDavResp);

exit:

	if(lpPropTagArr)
		MAPIFreeBuffer(lpPropTagArr);

	if(lpSpropVal)
		MAPIFreeBuffer(lpSpropVal);

	return hr;
}

/**
 * Process the REPORT request
 *
 * REPORT has two parts CALENDAR-QUERY and MULTIGET.
 *
 * @return HRESULT
 */
HRESULT CalDAV::HrReport()
{
	HRESULT hr = hrSuccess;

	hr = WebDav::HrReport();

	return hr;
}

/** 
 * Retrieve list of entries in the calendar folder
 *
 * The function handles REPORT(calendar-query) and PROPFIND(depth 1) request,
 * REPORT method is used by mozilla clients and mac ical.app uses PROPFIND request
 *
 * @param[in]	lpsWebRCalQry	Pointer to structure containing the list of properties requested by client
 * @param[out]	lpsWebMStatus	Pointer to structure containing the response
 * @return		HRESULT
 */
HRESULT CalDAV::HrListCalEntries(WEBDAVREQSTPROPS *lpsWebRCalQry, WEBDAVMULTISTATUS *lpsWebMStatus)
{
	HRESULT hr = hrSuccess;
	std::wstring wstrConvVal;
	std::wstring wstrReqPath;
	IMAPITable *lpTable = NULL;
	LPSRowSet lpRowSet = NULL;
	LPSPropTagArray lpPropTagArr = NULL;
	LPSPropTagArray lpPropTags = NULL;
	LPSPropValue lpsPropVal = NULL;
	MapiToICal *lpMtIcal = NULL;
	ULONG cbsize = 0;
	ULONG ulTagGOID = 0;
	ULONG ulTagTsRef = 0;
	ULONG ulTagPrivate = 0;
	std::list<WEBDAVPROPERTY>::iterator iter;
	WEBDAVPROP sDavProp;
	WEBDAVPROPERTY sDavProperty;
	WEBDAVRESPONSE sWebResponse;
	bool blCensorPrivate = false;
	ULONG ulCensorFlag = 0;
	ULONG cValues = 0;
	LPSPropValue lpProps = NULL;

	ulTagGOID = CHANGE_PROP_TYPE(m_lpNamedProps->aulPropTag[PROP_GOID], PT_BINARY);
	ulTagTsRef = CHANGE_PROP_TYPE(m_lpNamedProps->aulPropTag[PROP_APPTTSREF], PT_UNICODE);
	ulTagPrivate = CHANGE_PROP_TYPE(m_lpNamedProps->aulPropTag[PROP_PRIVATE], PT_BOOLEAN);
	
	hr = m_lpRequest->HrGetUrl(&wstrReqPath);
	if(hr != hrSuccess)
		goto exit;

	if (wstrReqPath[wstrReqPath.length() - 1] != '/')
		wstrReqPath.append(1, '/');

	if ((m_ulFolderFlag & SHARED_FOLDER) && !HasDelegatePerm(m_lpDefStore, m_lpActiveStore))
		blCensorPrivate = true;
	
	HrSetDavPropName(&(lpsWebMStatus->sPropName), "multistatus", CALDAVNSDEF);
	HrSetDavPropName(&(sWebResponse.sPropName), "response", CALDAVNSDEF);
	HrSetDavPropName(&(sWebResponse.sHRef.sPropName), "href", CALDAVNSDEF);
		
	sDavProp = lpsWebRCalQry->sProp;

	if(!m_lpUsrFld)
	{
		m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Error user's folder not found, cannot list calendar entries");
		goto exit;
	}
	
	if (!lpsWebRCalQry->sFilter.lstFilters.empty())
	{
		hr = HrGetOneProp(m_lpUsrFld, PR_CONTAINER_CLASS_A, &lpsPropVal);
		if (hr != hrSuccess)
			goto exit;

		if (lpsWebRCalQry->sFilter.lstFilters.back() == "VTODO"
			&& strncmp(lpsPropVal->Value.lpszA, "IPF.Task", strlen("IPF.Task")))
		{
				goto exit;
		}
	
		if (lpsWebRCalQry->sFilter.lstFilters.back() == "VEVENT"
			&& strncmp(lpsPropVal->Value.lpszA, "IPF.Appointment", strlen("IPF.Appointment")))
		{
			goto exit;
		}
	}

	hr = m_lpUsrFld->GetContentsTable(0, &lpTable);
	if(hr != hrSuccess)
	{
		m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Error in GetContentsTable, error code : 0x%08X", hr);
		goto exit;
	}

	// +4 to add GlobalObjid, dispidApptTsRef , PR_ENTRYID and private in SetColumns along with requested data.
	cbsize = (ULONG)sDavProp.lstProps.size() + 4;

	hr = MAPIAllocateBuffer(CbNewSPropTagArray(cbsize), (void **)&lpPropTagArr);
	if(hr != hrSuccess)
	{
		m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Cannot allocate memory");
		goto exit;
	}

	lpPropTagArr->cValues = cbsize;
	lpPropTagArr->aulPropTag[0] = ulTagTsRef;
	lpPropTagArr->aulPropTag[1] = ulTagGOID;
	lpPropTagArr->aulPropTag[2] = PR_ENTRYID;
	lpPropTagArr->aulPropTag[3] = ulTagPrivate;
	
	iter = sDavProp.lstProps.begin();

	//mapi property mapping for requested properties.
	//FIXME what if the property mapping is not found.
	for (int i = 4; iter != sDavProp.lstProps.end(); iter++, i++)
	{
		sDavProperty = *iter;
		lpPropTagArr->aulPropTag[i] = GetPropmap((char*)sDavProperty.sPropName.strPropname.c_str());
	}

	hr = m_lpUsrFld->GetProps((LPSPropTagArray)lpPropTagArr, 0, &cValues, &lpProps);
	if (FAILED(hr)) {
		 m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Unable to receive folder properties, error 0x%08X", hr);
		 goto exit;
	}

	sWebResponse.sHRef.wstrValue = wstrReqPath;	
	HrMapValtoStruct(lpProps, cValues, NULL, 0, &(lpsWebRCalQry->sProp.lstProps), &sWebResponse);
	lpsWebMStatus->lstResp.push_back(sWebResponse);
	sWebResponse.lstsPropStat.clear();

	// @todo, add "start time" property and recurrence data to table and filter in loop
	// if lpsWebRCalQry->sFilter.tStart is set.
	hr = lpTable->SetColumns((LPSPropTagArray)lpPropTagArr, 0);
	if(hr != hrSuccess)
		goto exit;
	
	CreateMapiToICal(m_lpAddrBook, "utf-8", &lpMtIcal);
	if (!lpMtIcal)
	{
		hr = MAPI_E_CALL_FAILED;
		goto exit;
	}

	while(1)
	{
		hr = lpTable->QueryRows(50, 0, &lpRowSet);
		if(hr != hrSuccess)
		{
			m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Error retrieving rows of table");
			goto exit;
		}

		if(lpRowSet->cRows == 0)
			break;

		//add data from each requested property.
		for (ULONG ulRowCntr = 0; ulRowCntr < lpRowSet->cRows; ulRowCntr++)
		{
			ulCensorFlag = (ULONG)blCensorPrivate; // reset Censor flag for next message.

			//For entries created in Evolution client use Timestamp as URL ref
			if(lpRowSet->aRow[ulRowCntr].lpProps[0].ulPropTag == ulTagTsRef && m_blIsCLEVO)
				wstrConvVal = lpRowSet->aRow[ulRowCntr].lpProps[0].Value.lpszW;
			else if (lpRowSet->aRow[ulRowCntr].lpProps[1].ulPropTag == ulTagGOID)
				wstrConvVal = SPropValToWString(&(lpRowSet->aRow[ulRowCntr].lpProps[1]));
			else
				wstrConvVal.clear();

			// On some items, outlook never created the uid, so we need to create one for ical
			if (wstrConvVal.empty())
			{
				hr = CreateAndGetGuid(lpRowSet->aRow[ulRowCntr].lpProps[2].Value.bin, ulTagGOID, &wstrConvVal);
				if(hr == E_ACCESSDENIED)
				{
					// use PR_ENTRYID since we couldn't create a new guid for the item
					wstrConvVal = bin2hexw(lpRowSet->aRow[ulRowCntr].lpProps[2].Value.bin.cb, lpRowSet->aRow[ulRowCntr].lpProps[2].Value.bin.lpb);
					hr = hrSuccess;
				}
				else if(hr != hrSuccess)
					continue;
			} else {
				wstrConvVal = urlEncode(wstrConvVal);
			}

			wstrConvVal.append(L".ics");
			sWebResponse.sHRef.wstrValue = wstrReqPath + wstrConvVal;

			if (blCensorPrivate && lpRowSet->aRow[ulRowCntr].lpProps[3].ulPropTag == ulTagPrivate && lpRowSet->aRow[ulRowCntr].lpProps[3].Value.b)
				ulCensorFlag |= M2IC_CENSOR_PRIVATE;
			else
				ulCensorFlag = 0;

			hr = HrMapValtoStruct(lpRowSet->aRow[ulRowCntr].lpProps, lpRowSet->aRow[ulRowCntr].cValues, lpMtIcal, ulCensorFlag, &(lpsWebRCalQry->sProp.lstProps), &sWebResponse);
			
			lpsWebMStatus->lstResp.push_back(sWebResponse);
			sWebResponse.lstsPropStat.clear();
		}

		if(lpRowSet)
		{
			FreeProws(lpRowSet);
			lpRowSet = NULL;
		}
	}

exit:

	if (lpsPropVal)
		MAPIFreeBuffer(lpsPropVal);

	if(lpMtIcal)
		delete lpMtIcal;

	if(lpPropTags)
		MAPIFreeBuffer(lpPropTags);

	if(lpPropTagArr)
		MAPIFreeBuffer(lpPropTagArr);

	if(lpTable)
		lpTable->Release();

	if(lpRowSet)
		FreeProws(lpRowSet);
	
	if (lpProps)
			MAPIFreeBuffer(lpProps);

	return hr;
}

/**
 * Handles Report (calendar-multiget) caldav request.
 *
 * Sets values of requested caldav properties in WEBDAVMULTISTATUS structure.
 *
 * @param[in]	sWebRMGet		structure that contains the list of calendar entries and properties requested.
 * @param[out]	sWebMStatus		structure that values of requested properties.
 * @retval		HRESULT
 * @return		S_OK			always returns S_OK.
 *
 */
HRESULT CalDAV::HrHandleReport(WEBDAVRPTMGET *sWebRMGet, WEBDAVMULTISTATUS *sWebMStatus)
{
	HRESULT hr = hrSuccess;
	IMAPIFolder *lpUsrFld = NULL;
	IMAPITable *lpTable = NULL;
	LPSPropTagArray lpPropTags = NULL;
	LPSPropTagArray lpPropTagArr = NULL;
	MapiToICal *lpMtIcal = NULL;
	std::wstring wstrReqPath;	
	std::list<WEBDAVPROPERTY>::iterator iter;
	SRestriction * lpsRoot = NULL;
	ULONG cbsize = 0;
	WEBDAVPROP sDavProp;
	WEBDAVPROPERTY sDavProperty;
	WEBDAVRESPONSE sWebResponse;
	bool blCensorPrivate = false;
	
	m_lpRequest->HrGetUrl(&wstrReqPath);
	HrSetDavPropName(&(sWebResponse.sPropName), "response", CALDAVNSDEF);
	HrSetDavPropName(&sWebMStatus->sPropName, "multistatus", CALDAVNSDEF);

	if ((m_ulFolderFlag & SHARED_FOLDER) && !HasDelegatePerm(m_lpDefStore, m_lpActiveStore))
		blCensorPrivate = true;
	
	if (m_lpUsrFld)
		lpUsrFld = m_lpUsrFld;
	else {
		hr = MAPI_E_NOT_FOUND;
		m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Error user's folder not found, cannot return ical entries");
		goto exit;
	}

	hr = lpUsrFld->GetContentsTable(0, &lpTable);
	if(hr != hrSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Error in GetContentsTable, error code : 0x%08X", hr);
		goto exit;
	}

	sDavProp = sWebRMGet->sProp;

	//Add GUID in Setcolumns.
	cbsize = (ULONG)sDavProp.lstProps.size() + 2;

	hr = MAPIAllocateBuffer(CbNewSPropTagArray(cbsize), (void **)&lpPropTagArr);
	if(hr != hrSuccess)
	{
		m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Error allocating memory, error code : 0x%08X", hr);
		goto exit;
	}
	
	lpPropTagArr->cValues = cbsize;
	lpPropTagArr->aulPropTag[0] = CHANGE_PROP_TYPE(m_lpNamedProps->aulPropTag[PROP_GOID], PT_BINARY);
	lpPropTagArr->aulPropTag[1] = CHANGE_PROP_TYPE(m_lpNamedProps->aulPropTag[PROP_PRIVATE], PT_BOOLEAN);
	
	iter = sDavProp.lstProps.begin();
	for(int i = 2; iter != sDavProp.lstProps.end(); iter++, i++)
	{
		sDavProperty = *iter;
		lpPropTagArr->aulPropTag[i] = GetPropmap((char*)sDavProperty.sPropName.strPropname.c_str());
	}

	hr = lpTable->SetColumns((LPSPropTagArray)lpPropTagArr, 0);
	if(hr != hrSuccess)
		goto exit;

	cbsize = (ULONG)sWebRMGet->lstWebVal.size();
	
	CreateMapiToICal(m_lpAddrBook, "utf-8", &lpMtIcal);
	if (!lpMtIcal)
	{
		hr = MAPI_E_CALL_FAILED;
		goto exit;
	}

	for(ULONG i = 0; i < cbsize; i++)
	{
		WEBDAVVALUE sWebDavVal;
		SRowSet *lpValRows = NULL;
		ULONG ulCensorFlag = (ULONG)blCensorPrivate;
		
		sWebDavVal = sWebRMGet->lstWebVal.front();
		sWebRMGet->lstWebVal.pop_front();

		sWebResponse.sHRef = sWebDavVal;
		sWebResponse.sHRef.wstrValue = wstrReqPath + sWebDavVal.wstrValue + L".ics";
		sWebResponse.sStatus = WEBDAVVALUE();

		hr = HrMakeRestriction(W2U(sWebDavVal.wstrValue), m_lpNamedProps, &lpsRoot);
		if (hr != hrSuccess)
			goto next;
		
		hr = lpTable->FindRow(lpsRoot, BOOKMARK_BEGINNING, 0);
		if (hr != hrSuccess)
			m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Entry not found (%ls), error code : 0x%08X", sWebDavVal.wstrValue.c_str(), hr);

		// conversion if everthing goes ok, otherwise, add empty item with failed status field
		// we need to return all items requested in the multistatus reply, otherwise sunbird will stop, displaying nothing to the user.

		if (hr == hrSuccess) {
			hr = lpTable->QueryRows(1, TBL_NOADVANCE, &lpValRows); // TODO: what if we get multiple items ?
		
			if(hr != hrSuccess || lpValRows->cRows != 1)
				goto exit;

			if (blCensorPrivate && PROP_TYPE(lpValRows->aRow[0].lpProps[1].ulPropTag) != PT_ERROR && lpValRows->aRow[0].lpProps[1].Value.b)
				ulCensorFlag |= M2IC_CENSOR_PRIVATE;
			else
				ulCensorFlag = 0;
		}

		if(hr == hrSuccess)
			hr = HrMapValtoStruct(lpValRows->aRow[0].lpProps, lpValRows->aRow[0].cValues, lpMtIcal, ulCensorFlag, &sDavProp.lstProps, &sWebResponse);
		else {
			// no: "status" can only be in <D:propstat xmlns:D="DAV:"> tag, so fix in HrMapValtoStruct
			HrSetDavPropName(&(sWebResponse.sStatus.sPropName), "status", CALDAVNSDEF);
			sWebResponse.sStatus.wstrValue = L"HTTP/1.1 404 Not Found";
		}

		sWebMStatus->lstResp.push_back(sWebResponse);
		sWebResponse.lstsPropStat.clear();
next:
		if (lpsRoot)
			FREE_RESTRICTION(lpsRoot);

		if(lpValRows)
			FreeProws(lpValRows);
		lpValRows = NULL;

	}

	hr = hrSuccess;

exit:
	
	if(lpMtIcal)
		delete lpMtIcal;

	if(lpsRoot)
		FREE_RESTRICTION(lpsRoot);

	if(lpTable)
		lpTable->Release();

	if(lpPropTags)
		MAPIFreeBuffer(lpPropTags);
	
	if(lpPropTagArr)
		MAPIFreeBuffer(lpPropTagArr);
	
	return hr;
}

/**
 * Generates response to Property search set request
 *
 * The request is to list the properties that can be requested by client,
 * while requesting for attendee suggestions
 *
 * @param[out]	lpsWebMStatus	Response structure returned
 *
 * @return		HRESULT			Always returns hrSuccess
 */
HRESULT CalDAV::HrHandlePropertySearchSet(WEBDAVMULTISTATUS *lpsWebMStatus)
{
	HRESULT hr = hrSuccess;
	WEBDAVRESPONSE sDavResponse;
	WEBDAVPROPSTAT sDavPropStat;
	WEBDAVPROPERTY sWebProperty;
	WEBDAVPROP sWebProp;

	HrSetDavPropName(&(lpsWebMStatus->sPropName), "principal-search-property-set", CALDAVNSDEF);

	HrSetDavPropName(&sDavResponse.sPropName, "principal-search-property", CALDAVNSDEF);
	HrSetDavPropName(&sDavPropStat.sPropName, "prop", CALDAVNSDEF);
	HrSetDavPropName(&sDavPropStat.sProp.sPropName, "displayname", CALDAVNSDEF);
	sDavResponse.lstsPropStat.push_back(sDavPropStat);
	HrSetDavPropName(&sDavResponse.sHRef.sPropName, "description", "xml:lang", "en", CALDAVNSDEF);	
	sDavResponse.sHRef.wstrValue = L"Display Name";	
	HrSetDavPropName(&sDavPropStat.sProp.sPropName, "", "");

	lpsWebMStatus->lstResp.push_back(sDavResponse);	
	sDavResponse.lstsPropStat.clear();

	HrSetDavPropName(&sDavResponse.sPropName, "principal-search-property", CALDAVNSDEF);
	HrSetDavPropName(&sDavPropStat.sPropName, "prop", CALDAVNSDEF);
	HrSetDavPropName(&sDavPropStat.sProp.sPropName, "calendar-user-type", CALDAVNSDEF);
	sDavResponse.lstsPropStat.push_back(sDavPropStat);
	HrSetDavPropName(&sDavResponse.sHRef.sPropName, "description", "xml:lang", "en", CALDAVNSDEF);	
	sDavResponse.sHRef.wstrValue = L"Calendar user type";	
	HrSetDavPropName(&sDavPropStat.sProp.sPropName, "", "");

	lpsWebMStatus->lstResp.push_back(sDavResponse);	
	sDavResponse.lstsPropStat.clear();

	HrSetDavPropName(&sDavResponse.sPropName, "principal-search-property", CALDAVNSDEF);
	HrSetDavPropName(&sDavPropStat.sPropName, "prop", CALDAVNSDEF);
	HrSetDavPropName(&sDavPropStat.sProp.sPropName, "calendar-user-address-set", CALDAVNSDEF);
	sDavResponse.lstsPropStat.push_back(sDavPropStat);
	HrSetDavPropName(&sDavResponse.sHRef.sPropName, "description", "xml:lang", "en", CALDAVNSDEF);	
	sDavResponse.sHRef.wstrValue = L"Calendar User Address Set";	
	HrSetDavPropName(&sDavPropStat.sProp.sPropName, "", "");

	lpsWebMStatus->lstResp.push_back(sDavResponse);	
	sDavResponse.lstsPropStat.clear();

	HrSetDavPropName(&sDavResponse.sPropName, "principal-search-property", CALDAVNSDEF);
	HrSetDavPropName(&sDavPropStat.sPropName, "prop", CALDAVNSDEF);
	HrSetDavPropName(&sDavPropStat.sProp.sPropName, "email-address-set", "http://calendarserver.org/ns/");
	sDavResponse.lstsPropStat.push_back(sDavPropStat);
	HrSetDavPropName(&sDavResponse.sHRef.sPropName, "description", "xml:lang", "en", CALDAVNSDEF);	
	sDavResponse.sHRef.wstrValue = L"Email Address";
	HrSetDavPropName(&sDavPropStat.sProp.sPropName, "", "");
	

	lpsWebMStatus->lstResp.push_back(sDavResponse);
	sDavResponse.lstsPropStat.clear();
	return hr;
}
/**
 * Handles attedee suggestion list request
 * 
 * @param[in]	sWebRMGet		Pointer to WEBDAVRPTMGET structure containing user to search in global address book
 * @param[out]	sWebMStatus		Pointer to WEBDAVMULTISTATUS structure cotaning attndees list matching the request
 *
 * @return		HRESULT			Always returns hrSuccess 
 */
HRESULT CalDAV::HrHandlePropertySearch(WEBDAVRPTMGET *sWebRMGet, WEBDAVMULTISTATUS *sWebMStatus)
{
	HRESULT hr = hrSuccess;
	IABContainer *lpAbCont = NULL;
	IMAPITable *lpTable = NULL;	
	SRestriction * lpsRoot = NULL;
	SRowSet *lpValRows = NULL;
	LPSPropTagArray lpPropTagArr = NULL;
	LPSPropValue lpsPropVal = NULL;
	MapiToICal *lpMtIcal = NULL;
	ULONG cbsize = 0;
	ULONG ulPropTag = 0;
	ULONG ulTagPrivate = 0;
	std::list<WEBDAVPROPERTY>::iterator iter;
	std::list<WEBDAVVALUE>::iterator iterWebVal;
	SBinary sbEid = {0};
	WEBDAVPROP sDavProp;
	WEBDAVPROPERTY sDavProperty;
	WEBDAVRESPONSE sWebResponse;
	ULONG ulObjType = 0;
	std::wstring wstrReqPath;

	m_lpRequest->HrGetUrl(&wstrReqPath);

	// Open Global Address book
	hr = m_lpAddrBook->GetDefaultDir(&sbEid.cb, (LPENTRYID*)&sbEid.lpb);
	if (hr != hrSuccess)
		goto exit;

	hr = m_lpSession->OpenEntry(sbEid.cb, (LPENTRYID)sbEid.lpb, NULL, 0, &ulObjType, (LPUNKNOWN *)&lpAbCont);
	if (hr != hrSuccess)
		goto exit;
	
	hr = lpAbCont->GetContentsTable(0, &lpTable);
	if (hr != hrSuccess)
		goto exit;

	hr = lpTable->GetRowCount(0, &ulObjType);
	if (hr != hrSuccess)
		goto exit;

	// create restriction
	CREATE_RESTRICTION(lpsRoot);
	CREATE_RES_OR(lpsRoot, lpsRoot, sWebRMGet->lstWebVal.size()); // max: or guid or raw or entryid
	iterWebVal = sWebRMGet->lstWebVal.begin();

	for (int i = 0; i < (int)sWebRMGet->lstWebVal.size(); i++, iterWebVal++)
	{
		hr = MAPIAllocateMore(sizeof(SPropValue), lpsRoot, (void**)&lpsPropVal);
		if (hr != hrSuccess)
			goto exit;

		hr = MAPIAllocateMore(sizeof(wchar_t) * (iterWebVal->wstrValue.length() + 1), lpsRoot, (void**)&lpsPropVal->Value.lpszW);
		if (hr != hrSuccess)
			goto exit;

		lpsPropVal->ulPropTag = GetPropmap((char *)iterWebVal->sPropName.strPropname.c_str());
		memcpy(lpsPropVal->Value.lpszW, iterWebVal->wstrValue.c_str(), sizeof(wchar_t) * (iterWebVal->wstrValue.length() + 1));

		DATA_RES_CONTENT(lpsRoot,lpsRoot->res.resOr.lpRes[i],FL_SUBSTRING | FL_IGNORECASE, lpsPropVal->ulPropTag, lpsPropVal);
		lpsPropVal = NULL;
	}

	// create proptagarray.
	sDavProp = sWebRMGet->sProp;

	//Add GUID in Setcolumns.
	cbsize = (ULONG)sDavProp.lstProps.size() + 3;

	hr = MAPIAllocateBuffer(CbNewSPropTagArray(cbsize), (void **)&lpPropTagArr);
	if (hr != hrSuccess)
	{
		m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Error allocating memory, error code : 0x%08X", hr);
		goto exit;
	}
	
	ulTagPrivate = CHANGE_PROP_TYPE(m_lpNamedProps->aulPropTag[PROP_PRIVATE], PT_BOOLEAN);
	ulPropTag = CHANGE_PROP_TYPE(m_lpNamedProps->aulPropTag[PROP_GOID], PT_BINARY);
	
	lpPropTagArr->cValues = cbsize;
	lpPropTagArr->aulPropTag[0] = ulPropTag;
	lpPropTagArr->aulPropTag[1] = ulTagPrivate;
	lpPropTagArr->aulPropTag[2] = PR_ACCOUNT;

	iter = sDavProp.lstProps.begin();
	for(int i = 3; iter != sDavProp.lstProps.end(); iter++, i++)
	{
		sDavProperty = *iter;
		lpPropTagArr->aulPropTag[i] = GetPropmap((char*)sDavProperty.sPropName.strPropname.c_str());
	}

	hr = lpTable->SetColumns((LPSPropTagArray)lpPropTagArr, 0);
	if (hr != hrSuccess)
		goto exit;
	
	// restrict table
	hr = lpTable->Restrict(lpsRoot, 0);
	if (hr != hrSuccess)
		goto exit;

	hr = lpTable->GetRowCount(0, &ulObjType);
	if (hr != hrSuccess)
		goto exit;

	HrSetDavPropName(&(sWebResponse.sPropName), "response", CALDAVNSDEF);
	HrSetDavPropName(&sWebMStatus->sPropName, "multistatus", CALDAVNSDEF);
	
	// set rows into Dav structures
	while (1) {
		hr = lpTable->QueryRows(50, 0, &lpValRows); // TODO: what if we get multiple items ?
		if (hr != hrSuccess || lpValRows->cRows == 0)
			break;

		for(ULONG i = 0; i < lpValRows->cRows; i++)
		{
			WEBDAVVALUE sWebDavVal;
			
			lpsPropVal = PpropFindProp( lpValRows->aRow[i].lpProps, lpValRows->aRow[i].cValues, PR_ACCOUNT_W);
			
			HrSetDavPropName(&(sWebResponse.sHRef.sPropName), "href", CALDAVNSDEF);
			if (lpsPropVal)
				sWebResponse.sHRef.wstrValue = wstrReqPath + lpsPropVal->Value.lpszW;
			
			if (hr == hrSuccess)
				hr = HrMapValtoStruct(lpValRows->aRow[i].lpProps, lpValRows->aRow[i].cValues, NULL, 0, &sDavProp.lstProps, &sWebResponse);

			sWebMStatus->lstResp.push_back(sWebResponse);
			sWebResponse.lstsPropStat.clear();

		}
		if (lpValRows)
		{
			FreeProws(lpValRows);
			lpValRows = NULL;
		}
	}

	hr = hrSuccess;

exit:
	
	if (lpMtIcal)
		delete lpMtIcal;

	if (lpsRoot)
		FREE_RESTRICTION(lpsRoot);

	if (lpTable)
		lpTable->Release();

	if (lpPropTagArr)
		MAPIFreeBuffer(lpPropTagArr);

	return hr;
}


/**
 * Handles deletion of folder & message
 * 
 * Function moves a folder or message to the deleted items folder
 * 
 * @return	mapi error codes
 * @retval	MAPI_E_NO_ACCESS	Insufficient permissions on folder or request to delete default folder.
 * @retval	MAPI_E_NOT_FOUND	Message or folder to be deleted not found.
 *
 * @todo	Move Code To HrHandleDelete().
 */
HRESULT CalDAV::HrDelete()
{
	HRESULT hr = hrSuccess;
	int nFldId = 1;
	std::string strGuid;
	std::wstring wstrUrl;
	std::wstring wstrFldName;
	std::wstring wstrFldTmpName;
	SBinary sbEid = {0,0};
	ULONG ulObjType = 0;
	ULONG cValues = 0;
	IMAPIFolder *lpWastBoxFld = NULL;
	LPSPropValue lpProps = NULL;
	LPSPropValue lpPropWstBxEID = NULL;
	LPENTRYLIST lpEntryList= NULL;
	bool bisFolder = false;
	SizedSPropTagArray(3, lpPropTagArr) = {3, {PR_ENTRYID, PR_LAST_MODIFICATION_TIME, PR_DISPLAY_NAME_W}};

	m_lpRequest->HrGetUrl(&wstrUrl);
	bisFolder = m_ulUrlFlag & REQ_COLLECTION;

	if (!m_blFolderAccess && bisFolder)
	{
		hr = MAPI_E_NO_ACCESS;
		goto exit;
	}

	hr = HrGetOneProp(m_lpDefStore, PR_IPM_WASTEBASKET_ENTRYID, &lpPropWstBxEID);
	if(hr != hrSuccess)
		goto exit;
	
	hr = m_lpDefStore->OpenEntry(lpPropWstBxEID->Value.bin.cb, (LPENTRYID)lpPropWstBxEID->Value.bin.lpb, NULL, MAPI_MODIFY, &ulObjType, (LPUNKNOWN*)&lpWastBoxFld);
	if (hr != hrSuccess)
	{
		m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Error opening \"Deleted items\" folder, error code : 0x%08X", hr);
		goto exit;
	}
	
	// if the request is to delete calendar entry
	if (!bisFolder) {
		strGuid = StripGuid (wstrUrl);
		hr = HrMoveEntry(strGuid, lpWastBoxFld);
		goto exit;
	}

	hr = m_lpUsrFld->GetProps((LPSPropTagArray)&lpPropTagArr, 0, &cValues, &lpProps);
	if(FAILED(hr))
		goto exit;

	// Entry ID
	if (lpProps[0].ulPropTag != PT_ERROR)
		sbEid = lpProps[0].Value.bin;

	// Folder display name
	if (lpProps[2].ulPropTag != PT_ERROR)
		wstrFldName = lpProps[2].Value.lpszW;
	
	//Create Entrylist
	hr = MAPIAllocateBuffer(sizeof(ENTRYLIST), (void**)&lpEntryList);
	if (hr != hrSuccess)
		goto exit;

	lpEntryList->cValues = 1;

	hr = MAPIAllocateMore(sizeof(SBinary), lpEntryList, (void**)&lpEntryList->lpbin);
	if (hr != hrSuccess)
		goto exit;
	
	hr = MAPIAllocateMore(sbEid.cb, lpEntryList, (void**)&lpEntryList->lpbin[0].lpb);
	if (hr != hrSuccess)
		goto exit;

	lpEntryList->lpbin[0].cb = sbEid.cb;
	memcpy((void*)lpEntryList->lpbin[0].lpb, (const void*)sbEid.lpb, sbEid.cb);

	wstrFldTmpName = wstrFldName;
	while (true) {

		hr = m_lpRootFld->CopyFolder(sbEid.cb, (LPENTRYID)sbEid.lpb, NULL, lpWastBoxFld, (LPTSTR)wstrFldTmpName.c_str(), 0, NULL, MAPI_MOVE | MAPI_UNICODE);
		if (hr == MAPI_E_COLLISION) {
			// rename the folder if same folder name is present in Deleted items folder
			if (nFldId < 1000) { // Max 999 folders
				wstrFldTmpName = wstrFldName + wstringify(nFldId);
				nFldId++;
			} else {
				m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Error Deleting Folder error code : 0x%08X", hr);
				goto exit;
			}

		} else if (hr != hrSuccess ) {
			m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Error Deleting Folder error code : 0x%08X", hr);
			goto exit;
		} else
			break;
	}

exit:
	if ( hr == MAPI_E_NO_ACCESS )
	{
		m_lpRequest->HrResponseHeader(403, "Forbidden");
		m_lpRequest->HrResponseBody("This folder cannot be deleted");
	}
	else if (hr == MAPI_E_DECLINE_COPY)
	{
		m_lpRequest->HrResponseHeader(412, "Precondition Failed");
	}
	else if (hr != hrSuccess)
	{
		m_lpRequest->HrResponseHeader(404, "Not Found");
		m_lpRequest->HrResponseBody("Item to be Deleted not found");
	}
	else
		m_lpRequest->HrResponseHeader(204, "No Content");
	
	if (lpPropWstBxEID)
		MAPIFreeBuffer(lpPropWstBxEID);

	if (lpWastBoxFld)
		lpWastBoxFld->Release();

	if (lpProps)
		MAPIFreeBuffer(lpProps);
		
	if (lpEntryList)
		MAPIFreeBuffer(lpEntryList);

	return hr;
}
/**
 * Moves calendar entry to destination folder
 *
 * Function searches for the calendar refrenced by the guid value in the
 * folder opened by HrGetFolder() and moves the entry to the destination folder.
 *
 * @param[in] strGuid		The Guid refrencing a calendar entry
 * @param[in] lpDestFolder	The destination folder to which the entry is moved.
 *
 * @return	mapi error codes
 *
 * @retval	MAPI_E_NOT_FOUND	No message found containing the guid value
 * @retval	MAPI_E_NO_ACCESS	Insufficient rights on the calendar entry
 *
 * @todo	Check folder type and message type are same(i.e tasks are copied to task folder only)
 */
HRESULT CalDAV::HrMoveEntry(const std::string &strGuid, LPMAPIFOLDER lpDestFolder)
{
	HRESULT hr = hrSuccess;
	std::string strIfMatch;
	SBinary sbEid = {0,0};
	ULONG cValues = 0;
	LPSPropValue lpProps = NULL;
	IMessage *lpMessage = NULL;
	LPENTRYLIST lpEntryList= NULL;
	SizedSPropTagArray(3, lpPropTagArr) = {3, {PR_ENTRYID, PR_LAST_MODIFICATION_TIME, PR_DISPLAY_NAME_W}};

	m_lpRequest->HrGetIfMatch(&strIfMatch);

	//Find Entry With Particular Guid
	hr = HrFindAndGetMessage(strGuid, m_lpUsrFld, m_lpNamedProps, &lpMessage);
	if (hr != hrSuccess)
	{
		m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Entry to be deleted not found : 0x%08X", hr);
		goto exit;
	}
	
	// Check if the user is accessing a shared folder
	// Check for delegate permissions on shared folder
	// Check if the entry to be deleted in private
	if ((m_ulFolderFlag & SHARED_FOLDER) && 
		!HasDelegatePerm(m_lpDefStore, m_lpActiveStore) &&
		IsPrivate(lpMessage, CHANGE_PROP_TYPE(m_lpNamedProps->aulPropTag[PROP_PRIVATE], PT_BOOLEAN)) )
	{
		hr = MAPI_E_NO_ACCESS;
		goto exit;
	}

	hr = lpMessage->GetProps((LPSPropTagArray)&lpPropTagArr, 0, &cValues, &lpProps);
	if (FAILED(hr))
		goto exit;

	if (!strIfMatch.empty() && strIfMatch !=  SPropValToString(&lpProps[1]))
	{
		hr = MAPI_E_DECLINE_COPY;
		goto exit;
	}

	if (lpProps[0].ulPropTag != PT_ERROR)
		sbEid = lpProps[0].Value.bin;
	
	//Create Entrylist
	hr = MAPIAllocateBuffer(sizeof(ENTRYLIST), (void**)&lpEntryList);
	if (hr != hrSuccess)
		goto exit;

	lpEntryList->cValues = 1;

	hr = MAPIAllocateMore(sizeof(SBinary), lpEntryList, (void**)&lpEntryList->lpbin);
	if (hr != hrSuccess)
		goto exit;
	
	hr = MAPIAllocateMore(sbEid.cb, lpEntryList, (void**)&lpEntryList->lpbin[0].lpb);
	if (hr != hrSuccess)
		goto exit;

	lpEntryList->lpbin[0].cb = sbEid.cb;
	memcpy((void*)lpEntryList->lpbin[0].lpb, (const void*)sbEid.lpb, sbEid.cb);

	hr = m_lpUsrFld->CopyMessages(lpEntryList, NULL, lpDestFolder, 0, NULL, MAPI_MOVE);
	if (hr != hrSuccess)
	{
		m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Error Deleting Entry");
		goto exit;
	}
	
	// publish freebusy for default folder
	if (m_ulFolderFlag & DEFAULT_FOLDER)
		hr = HrPublishDefaultCalendar(m_lpSession, m_lpDefStore, time(NULL), FB_PUBLISH_DURATION, m_lpLogger);

	if (hr != hrSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Error Publishing Freebusy, error code : 0x%08X", hr);
		hr = hrSuccess;
	}

exit:
	if (lpProps)
		MAPIFreeBuffer(lpProps);
	
	if (lpMessage)
		lpMessage->Release();

	if (lpEntryList)
		MAPIFreeBuffer(lpEntryList);

	return hr;
}
/**
 * Handles adding new entries and modify existing entries in folder
 *
 * @return	HRESULT
 * @retval	MAPI_E_INVALID_PARAMETER	invalid folder specified in URL
 * @retval	MAPI_E_NO_ACCESS			insufficient permissions on folder or message
 * @retval	MAPI_E_INVALID_OBJECT		no message in ical data.
 */
HRESULT CalDAV::HrPut()
{
	HRESULT hr = hrSuccess;
	std::string strGuid;
	std::wstring wstrUrl;
	std::string strIcal;
	std::string strIcalMod;
	std::string strModTime;
	std::string strIfMatch;
	LPSPropValue lpPropOldModtime = NULL;
	LPSPropValue lpPropNewModtime = NULL;
	LPSPropValue lpsPropVal = NULL;
	eIcalType etype = VEVENT;
	SBinary sbUid;
	time_t ttLastModTime = 0;
	
	IMessage *lpMessage = NULL;
	ICalToMapi *lpICalToMapi = NULL;
	bool blNewEntry = false;
	bool blModified = false;

	m_lpRequest->HrGetUrl(&wstrUrl);
	m_lpRequest->HrGetIfMatch(&strIfMatch);

	if(!m_lpUsrFld)
	{
		hr = MAPI_E_INVALID_PARAMETER;
		m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Error retrieving calendar folders in PUT request, error code : 0x%08X", hr);
		goto exit;
	}
	
	strGuid = StripGuid(wstrUrl);

	//Find the Entry with particular Guid
	hr = HrFindAndGetMessage(strGuid, m_lpUsrFld, m_lpNamedProps, &lpMessage);
	if(hr == hrSuccess)
	{
		// check if entry can be modified by the user
		// check if the user is accessing shared folder
		// check if delegate permissions
		// check if the entry is private
		if ( m_ulFolderFlag & SHARED_FOLDER &&
			!HasDelegatePerm(m_lpDefStore, m_lpActiveStore) &&
			IsPrivate(lpMessage, CHANGE_PROP_TYPE(m_lpNamedProps->aulPropTag[PROP_PRIVATE], PT_BOOLEAN)) ) 
		{
			hr = MAPI_E_NO_ACCESS;
			goto exit;
		}

	} else {
		//new entry.
		hr = m_lpUsrFld->CreateMessage(NULL, 0, &lpMessage);
		if(hr != hrSuccess)
		{
			m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Error creating new message, error code : 0x%08X", hr);
			goto exit;
		}

		if(m_blIsCLEVO)
		{
			std::wstring wstrGuid;
			wstrGuid.reserve(strGuid.size());
			ULONG ulPropTag = 0;

			ulPropTag = CHANGE_PROP_TYPE(m_lpNamedProps->aulPropTag[PROP_APPTTSREF], PT_UNICODE);
			copy(strGuid.begin(), strGuid.end(), back_inserter(wstrGuid));
			hr = HrAddProperty(lpMessage, ulPropTag, false, &wstrGuid);
			if(hr != hrSuccess)
			{
				m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Error adding property to new message, error code : 0x%08X", hr);
				goto exit;
			}
		}
		blNewEntry = true;
	}

	//save Ical data to mapi.
	CreateICalToMapi(m_lpDefStore, m_lpAddrBook, false, &lpICalToMapi);
	
	m_lpRequest->HrGetBody(&strIcal);

	hr = lpICalToMapi->ParseICal(strIcal, m_strCharset, m_strSrvTz, m_lpImailUser, 0);
	if(hr!=hrSuccess)
	{
		m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Error Parsing ical data in PUT request, error code : 0x%08X", hr);
		m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Error Parsing ical data : %s", strIcal.c_str());
		goto exit;
	}

	if (lpICalToMapi->GetItemCount() == 0)
	{
		hr = MAPI_E_INVALID_OBJECT;
		m_lpLogger->Log(EC_LOGLEVEL_ERROR, "No message in ical data in PUT request, error code : 0x%08X", hr);
		goto exit;
	}

	if (lpICalToMapi->GetItemCount() > 1)
		m_lpLogger->Log(EC_LOGLEVEL_WARNING, "More than one message found in PUT, trying to combine messages");
	
	hr = HrGetOneProp (m_lpUsrFld, PR_CONTAINER_CLASS_A, &lpsPropVal);
	if (hr != hrSuccess)
		goto exit;

	hr = lpICalToMapi->GetItemInfo(0, &etype, &ttLastModTime, &sbUid);
	if(hr != hrSuccess)
	{
		hr = MAPI_E_NO_ACCESS;
		goto exit;
	}
	
	// FIXME: fix the check
	if ((etype == VEVENT && strncmp(lpsPropVal->Value.lpszA, "IPF.Appointment", strlen("IPF.Appointment")))
		|| (etype == VTODO && strncmp(lpsPropVal->Value.lpszA, "IPF.Task", strlen("IPF.Task"))))
	{
		hr = MAPI_E_NO_ACCESS;
		goto exit;
	}

	// check if entry is modified on server
	if (!strIfMatch.empty() && !blNewEntry)
	{
		hr = HrGetOneProp(lpMessage, PR_LAST_MODIFICATION_TIME, &lpPropOldModtime);
		if (hr == hrSuccess && strIfMatch != SPropValToString(lpPropOldModtime))
		{
			blModified = true;
			goto exit;
		}
	}

	hr = lpICalToMapi->GetItem(0, 0, lpMessage);
	if(hr != hrSuccess)
	{
		m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Error converting ical data to Mapi message in PUT request, error code : 0x%08X", hr);
		goto exit;
	}

	// get other messages if present
	for (ULONG n = 1; n < lpICalToMapi->GetItemCount(); n++) {
		SBinary sbSubUid;
		eIcalType eSubType = VEVENT;

		hr = lpICalToMapi->GetItemInfo(n, &eSubType, NULL, &sbSubUid);
		if(hr != hrSuccess)
		{
			hr = MAPI_E_NO_ACCESS;
			goto exit;
		}

		if (etype != eSubType || Util::CompareSBinary(sbUid, sbSubUid) != hrSuccess) {
			hr = MAPI_E_INVALID_OBJECT;
			m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Invalid sub item in ical, unable to save message");
			goto exit;
		}

		// merge in the same message, and hope for the best
		hr = lpICalToMapi->GetItem(n, 0, lpMessage);
		if(hr != hrSuccess)
		{
			m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Error converting ical data to Mapi message in PUT request, error code : 0x%08X", hr);
			goto exit;
		}
	}

	hr = lpMessage->SaveChanges(0);
	if(hr != hrSuccess)
	{
		m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Error saving Mapi message in PUT request, error code : 0x%08X", hr);
		goto exit;
	}

	// new modification time
	hr = HrGetOneProp(lpMessage, PR_LAST_MODIFICATION_TIME, &lpPropNewModtime);
	if (hr == hrSuccess)
		strModTime =  SPropValToString(lpPropNewModtime);
	
	if (strModTime.empty())
		goto exit;
	
	// Add quotes to the modification time
	strModTime.insert(0, 1, '"');
	strModTime.append(1,'"');

	// Publish freebusy only for default Calendar
	if(m_ulFolderFlag & DEFAULT_FOLDER)
		hr = HrPublishDefaultCalendar(m_lpSession, m_lpDefStore, time(NULL), FB_PUBLISH_DURATION, m_lpLogger);

	if ( hr != hrSuccess){
		m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Error Publishing Freebusy, error code : 0x%08X", hr);
		hr = hrSuccess;
	}
	
exit:
	if (hr == hrSuccess && blNewEntry)
		m_lpRequest->HrResponseHeader(201, "Created");
	else if (hr == hrSuccess && blModified && (m_blIsCLMAC || m_blIsCLEVO)) // entry is modifid on server (mac ical & evolution)
		m_lpRequest->HrResponseHeader(409, "Conflict");
	else if (hr == hrSuccess && blModified)
		m_lpRequest->HrResponseHeader(412, "Precondition Failed"); // entry is modifid on server (sunbird & TB)
	else if (hr == hrSuccess)
		m_lpRequest->HrResponseHeader(204, "No Content");
	else if( hr == MAPI_E_NOT_FOUND)
		m_lpRequest->HrResponseHeader(404, "Not Found");
	else if(hr == MAPI_E_NO_ACCESS)
		m_lpRequest->HrResponseHeader(403, "Forbidden");
	else
		m_lpRequest->HrResponseHeader(400, "Bad Request");

	if (hr == hrSuccess && !strModTime.empty())
		m_lpRequest->HrResponseHeader("Etag", strModTime);
	
	if (lpsPropVal)
		MAPIFreeBuffer(lpsPropVal);

	if(lpPropNewModtime)
		MAPIFreeBuffer(lpPropNewModtime);

	if(lpPropOldModtime)
		MAPIFreeBuffer(lpPropOldModtime);
	
	if(lpICalToMapi)
		delete lpICalToMapi;

	if(lpMessage)
		lpMessage->Release();

	return hr;
}

/**
 * Creates a new guid in the message and returns it
 * 
 * @param[in]	sbEid		EntryID of the message
 * @param[in]	ulPropTag	Property tag of the Guid property
 * @param[out]	lpstrGuid	The newly created guid is returned
 *
 * @return		HRESULT 
 */
HRESULT CalDAV::CreateAndGetGuid(SBinary sbEid, ULONG ulPropTag, std::wstring *lpwstrGuid)
{
	HRESULT hr = hrSuccess;
	string strGuid;
	LPMESSAGE lpMessage = NULL;
	ULONG ulObjType = 0;
	LPSPropValue lpProp = NULL;

	hr = m_lpActiveStore->OpenEntry(sbEid.cb, (LPENTRYID)sbEid.lpb, NULL, MAPI_BEST_ACCESS, &ulObjType, (LPUNKNOWN*)&lpMessage);
	if (hr != hrSuccess)
	{
		m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Error opening message to add Guid, error code : 0x%08X", hr);
		goto exit;
	}

	hr = HrCreateGlobalID(ulPropTag, NULL, &lpProp);
	if (hr != hrSuccess)
	{
		m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Error creating Guid, error code : 0x%08X", hr);
		goto exit;
	}

	hr = lpMessage->SetProps(1, lpProp, NULL);
	if (hr != hrSuccess)
	{
		m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Error while adding Guid to message, error code : 0x%08X", hr);
		goto exit;
	}

	hr = lpMessage->SaveChanges(0);
	if (hr != hrSuccess)
		goto exit;

	*lpwstrGuid = bin2hexw(lpProp->Value.bin.cb, lpProp->Value.bin.lpb);

exit:
	if (lpMessage)
		lpMessage->Release();

	if (lpProp)
		MAPIFreeBuffer(lpProp);

	return hr;
}
/**
 * Processes the MKCALENDAR request
 * @return	HRESULT
 * @retval		MAPI_E_NO_ACCESS	Unable to create folder, while accessing single calendar
 * @retval		MAPI_E_COLLISION	Folder with same name already exists
 */
HRESULT CalDAV::HrMkCalendar()
{
	HRESULT hr = hrSuccess;

	hr = WebDav::HrMkCalendar();
	
	return hr;
}

/**
 * Creates new calendar folder
 * 
 * @param[in]	lpsDavProp		Pointer to WEBDAVPROP structure, contains properite to be set on new folder
 * @return		HRESULT
 * @retval		MAPI_E_NO_ACCESS	Unable to create folder, while accessing single calendar
 * @retval		MAPI_E_COLLISION	Folder with same name already exists
 */
HRESULT CalDAV::HrHandleMkCal(WEBDAVPROP *lpsDavProp)
{
	HRESULT hr = hrSuccess;
	std::wstring wstrNewFldName;
	std::wstring wstrNewFldComment;
	IMAPIFolder *lpUsrFld = NULL;
	LPSPropValue lpsPropValSet = NULL;
	size_t ulFound = 0;
	ULONG ulPropTag = 0;

	ulFound = m_wstrFldName.find('/');

	// block mkcalendar when accessing single folders.
	// single folder mkcalendar folder-name : temp/NEW-FOLDER-ID
	if(!m_lpRootFld || ulFound != string::npos)
	{
		hr = MAPI_E_NO_ACCESS;
		goto exit;
	}

	wstrNewFldName = lpsDavProp->lstProps.front().wstrValue;
	hr = HrOpenUserFld(m_lpActiveStore, m_wstrFldName, false, m_ulUrlFlag & REQ_PUBLIC, m_lpLogger, &lpUsrFld);
	if(hr == hrSuccess)
	{
		hr = MAPI_E_COLLISION;
		goto exit;
	}

	hr = m_lpRootFld->CreateFolder(FOLDER_GENERIC, (LPTSTR)wstrNewFldName.c_str(), (LPTSTR)wstrNewFldComment.c_str(), NULL, MAPI_UNICODE, &lpUsrFld);
	if( hr != hrSuccess)
		goto exit;
	
	MAPIAllocateBuffer(sizeof(SPropValue), (void**)&lpsPropValSet);
	lpsPropValSet->ulPropTag = PR_CONTAINER_CLASS_A;
	lpsPropValSet->Value.lpszA = "IPF.Appointment";

	hr = lpUsrFld->SetProps(1, lpsPropValSet, NULL);
	if(hr != hrSuccess)
		goto exit;
	
	ulPropTag = CHANGE_PROP_TYPE(m_lpNamedProps->aulPropTag[PROP_FLDID], PT_UNICODE);
	hr = HrAddProperty(lpUsrFld, ulPropTag, true, &m_wstrFldName);
	if(hr != hrSuccess)
		m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Cannot Add named property, error code : 0x%08X", hr);

exit:
	if(lpsPropValSet)
		MAPIFreeBuffer(lpsPropValSet);

	if(lpUsrFld)
		lpUsrFld->Release();

	return hr;
}

/**
 * Retrieves the list of calendar and sets it in WEBDAVMULTISTATUS structure
 *
 * @param[in]	sDavProp		Pointer to requested properties of folders
 * @param[out]	lpsMulStatus	Pointer to WEBDAVMULTISTATUS structure, the calendar list and its properties are set in this structure
 *
 * @return		HRESULT
 * @retval		MAPI_E_BAD_VALUE	Method called by a non mac client
 */
/*
 * input		  		output
 * /caldav					list of /caldav/user/FLDPRFX_id
 * /caldav/other			list of /caldav/other/FLDPRFX_id
 * /caldav/other/folder		single folder, same url
 * /caldav/public			list of /caldav/public/FLDPRFX_id
 * /caldav/public/folder	single folder, same url
 */
HRESULT CalDAV::HrListCalendar(WEBDAVREQSTPROPS *sDavProp, WEBDAVMULTISTATUS *lpsMulStatus)
{
	HRESULT hr = hrSuccess;	
	WEBDAVPROP *lpsDavProp = &sDavProp->sProp;
	IMAPITable *lpHichyTable = NULL;
	IMAPITable *lpDelHichyTable = NULL;
	IMAPIFolder *lpWasteBox = NULL;
	SRestriction *lpRestrict = NULL;
	LPSPropValue lpSpropWbEID = NULL;
	LPSPropValue lpsPropSingleFld = NULL;
	LPSPropTagArray lpPropTagArr = NULL;
	LPSRowSet lpRowsALL = NULL;
	LPSRowSet lpRowsDeleted = NULL;
	size_t cbsize = 0;
	ULONG ulPropTagFldId = 0;
	ULONG ulObjType = 0;
	ULONG ulCmp = 0;
	ULONG ulDelEntries = 0;
	std::list<WEBDAVPROPERTY>::iterator iter;
	WEBDAVRESPONSE sDavResponse;
	std::wstring wstrUser;
	std::wstring wstrPath;

	m_lpRequest->HrGetUser(&wstrUser);
	m_lpRequest->HrGetUrl(&wstrPath);

	if(!m_blIsCLMAC)
	{
		hr = MAPI_E_BAD_VALUE;
		goto exit;
	}

	// all folder properties to fill request.
	cbsize = lpsDavProp->lstProps.size() + 2;

	hr = MAPIAllocateBuffer(CbNewSPropTagArray((ULONG)cbsize), (void **)&lpPropTagArr);
	if(hr != hrSuccess)
	{
		m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Cannot allocate memory");
		goto exit;
	}

	ulPropTagFldId = CHANGE_PROP_TYPE(m_lpNamedProps->aulPropTag[PROP_FLDID], PT_UNICODE);
	//add PR_ENTRYID & FolderID in setcolumns along with requested data.
	lpPropTagArr->cValues = (ULONG)cbsize;
	lpPropTagArr->aulPropTag[0] = PR_ENTRYID;
	lpPropTagArr->aulPropTag[1] = ulPropTagFldId;

	iter = lpsDavProp->lstProps.begin();
	for(int i = 2; iter != lpsDavProp->lstProps.end(); iter++, i++)	
	{
		lpPropTagArr->aulPropTag[i] = GetPropmap((char*)iter->sPropName.strPropname.c_str());
	}


	if(m_ulFolderFlag & DEFAULT_FOLDER || m_ulFolderFlag & SINGLE_FOLDER)
	{
		if (m_lpUsrFld == NULL)
		{
			m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Error User's Folder not found, while accessing single folder");
			goto exit;
		}

		hr = HrSetDavPropName(&(lpsMulStatus->sPropName), "multistatus", lpsDavProp->sPropName.strNS);
		if (hr != hrSuccess)
			goto exit;
		hr = HrSetDavPropName(&(sDavResponse.sPropName), "response", lpsDavProp->sPropName.strNS);
		if (hr != hrSuccess)
			goto exit;
		hr = HrSetDavPropName(&(sDavResponse.sHRef.sPropName), "href", lpsDavProp->sPropName.strNS);
		if (hr != hrSuccess)
			goto exit;
		sDavResponse.sHRef.wstrValue = wstrPath;

		hr = m_lpUsrFld->GetProps(lpPropTagArr, 0, (ULONG *)&cbsize, &lpsPropSingleFld);
		if (FAILED(hr))
			goto exit;

		hr = HrMapValtoStruct(lpsPropSingleFld, cbsize, NULL, 0, &lpsDavProp->lstProps, &sDavResponse);
		if (hr != hrSuccess)
			goto exit;

		lpsMulStatus->lstResp.push_back(sDavResponse);
		goto exit;
	}

	{		
		hr = HrGetSubCalendars(m_lpSession, m_lpRootFld, NULL, &lpHichyTable);
		if(hr != hrSuccess)
		{
			m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Error retrieving subcalendars for IPM_Subtree, error code : 0x%08X", hr);
			goto exit;
		}

		if(m_ulUrlFlag & REQ_PUBLIC)
			hr = HrGetOneProp(m_lpDefStore, PR_IPM_WASTEBASKET_ENTRYID, &lpSpropWbEID);
		else
			hr = HrGetOneProp(m_lpActiveStore, PR_IPM_WASTEBASKET_ENTRYID, &lpSpropWbEID);
		if(hr != hrSuccess)
		{
			m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Error retrieving subcalendars for IPM_Subtree, error code : 0x%08X", hr);
			goto exit;
		}
		
		if(m_ulUrlFlag & REQ_PUBLIC)
			hr = m_lpDefStore->OpenEntry(lpSpropWbEID->Value.bin.cb, (LPENTRYID)lpSpropWbEID->Value.bin.lpb, NULL, MAPI_BEST_ACCESS, &ulObjType, (LPUNKNOWN *)&lpWasteBox);
		else
			hr = m_lpActiveStore->OpenEntry(lpSpropWbEID->Value.bin.cb, (LPENTRYID)lpSpropWbEID->Value.bin.lpb, NULL, MAPI_BEST_ACCESS, &ulObjType, (LPUNKNOWN *)&lpWasteBox);
		if(hr != hrSuccess)
		{
			m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Error opening deleted items folder, error code : 0x%08X", hr);
			goto exit;
		}
		
		hr = HrGetSubCalendars(m_lpSession, lpWasteBox, NULL, &lpDelHichyTable);
		if(hr != hrSuccess)
		{
			m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Error retrieving subcalendars for Deleted items folder, error code : 0x%08X", hr);
			goto exit;
		}
	}


	hr = lpHichyTable->SetColumns(lpPropTagArr, 0);
	if(hr != hrSuccess)
		goto exit;

	hr = lpDelHichyTable->SetColumns(lpPropTagArr, 0);
	if(hr != hrSuccess)
		goto exit;

	// load top level container properties too for iCal 5
	hr = HrHandlePropfindRoot(sDavProp, lpsMulStatus);
	if (hr != hrSuccess)
		goto exit;
	while(1)
	{
		hr = lpHichyTable->QueryRows(50, 0, &lpRowsALL);
		if(hr != hrSuccess || lpRowsALL->cRows == 0)
			break;
		
		hr = lpDelHichyTable->QueryRows(50, 0, &lpRowsDeleted);
		if(hr != hrSuccess)
			break;

		for(ULONG i = 0; i < lpRowsALL->cRows; i++)
		{
			std::wstring wstrFldPath;

			if (lpRowsDeleted->cRows != 0 && ulDelEntries != lpRowsDeleted->cRows)
			{
				// @todo is this optimized, or just pure luck that this works? don't we need a loop?
				ulCmp = memcmp((const void *)lpRowsALL->aRow[i].lpProps[0].Value.bin.lpb,
							   (const void *)lpRowsDeleted->aRow[ulDelEntries].lpProps[0].Value.bin.lpb, lpRowsALL->aRow[i].lpProps[0].Value.bin.cb);
				if(ulCmp == 0)
				{
					ulDelEntries++;
					continue;
				}
			}

			HrSetDavPropName(&(sDavResponse.sPropName), "response", lpsDavProp->sPropName.strNS);

			if (lpRowsALL->aRow[i].lpProps[1].ulPropTag == ulPropTagFldId)
				wstrFldPath = lpRowsALL->aRow[i].lpProps[1].Value.lpszW;
			else if (lpRowsALL->aRow[i].lpProps[0].ulPropTag == PR_ENTRYID)
				// creates new ulPropTagFldId on this folder, or return PR_ENTRYID in wstrFldPath
				// @todo boolean should become default return proptag if save fails, PT_NULL for no default
				hr = HrAddProperty(m_lpActiveStore, lpRowsALL->aRow[i].lpProps[0].Value.bin, ulPropTagFldId, true, &wstrFldPath);

			if (hr != hrSuccess || wstrFldPath.empty())
			{
				m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Error adding Folder id property, error code : 0x%08X", hr);
				continue;
			}
			if(m_wstrFldOwner.empty() && !(m_ulUrlFlag & REQ_PUBLIC) )
				wstrFldPath = wstrUser + L"/" + FOLDER_PREFIX + wstrFldPath + L"/";
			else
				wstrFldPath = FOLDER_PREFIX + wstrFldPath + L"/";

			HrSetDavPropName(&(sDavResponse.sHRef.sPropName), "href", lpsDavProp->sPropName.strNS);
			sDavResponse.sHRef.wstrValue = wstrPath + wstrFldPath;

			iter = lpsDavProp->lstProps.begin();
			
			HrMapValtoStruct(lpRowsALL->aRow[i].lpProps, lpRowsALL->aRow[i].cValues, NULL, 0, &lpsDavProp->lstProps, &sDavResponse);

			lpsMulStatus->lstResp.push_back(sDavResponse);
			sDavResponse.lstsPropStat.clear();
		}

		if(lpRowsALL)
		{
			FreeProws(lpRowsALL);
			lpRowsALL = NULL;
		}

		if(lpRowsDeleted)
		{
			FreeProws(lpRowsDeleted);
			lpRowsDeleted = NULL;
		}
	}

exit:
	if(lpWasteBox)
		lpWasteBox->Release();

	if(lpHichyTable)
		lpHichyTable->Release();
	
	if(lpDelHichyTable)
		lpDelHichyTable->Release();

	if(lpsPropSingleFld)
		MAPIFreeBuffer(lpsPropSingleFld);

	if(lpSpropWbEID)
		MAPIFreeBuffer(lpSpropWbEID);

	if(lpPropTagArr)
		MAPIFreeBuffer(lpPropTagArr);

	if(lpRowsALL)
		FreeProws(lpRowsALL);
	
	if(lpRowsDeleted)
		FreeProws(lpRowsDeleted);

	if(lpRestrict)
		FREE_RESTRICTION(lpRestrict);

	return hr;
}
/**
 * Processes the PROPPATCH request
 *
 * This method is usually used to rename folder or edit comments on folder.
 *
 * @return	HRESULT
 */
HRESULT CalDAV::HrPropPatch()
{
	HRESULT hr = hrSuccess;

	hr = WebDav::HrPropPatch();

	return hr;
}

/**
 * Sets the values on mapi object for PROPPATCH request
 *
 * @param[in]	lpsDavProp	WEBDAVPROP structure containing properties to be modified and its values
 *
 * @return		HRESULT
 * @retval		MAPI_E_NO_ACCESS	Error returned while renaming the default calendar
 */
HRESULT CalDAV::HrHandlePropPatch(WEBDAVPROP *lpsDavProp)
{
	HRESULT hr = hrSuccess;
	LPSPropValue lpsPropVal = NULL;
	std::list<WEBDAVPROPERTY>::iterator iter;
	std::wstring wstrFldName;
	std::wstring wstrConvProp;
	ULONG ulCntr = 0;

	hr = MAPIAllocateBuffer( (ULONG)lpsDavProp->lstProps.size() * sizeof(SPropValue), (LPVOID*)&lpsPropVal);
	if (hr != hrSuccess)
		goto exit;

	for(iter = lpsDavProp->lstProps.begin(); iter != lpsDavProp->lstProps.end(); iter++)
	{
		if (iter->sPropName.strPropname == "displayname" )
			wstrFldName = iter->wstrValue;
		else if (iter->sPropName.strPropname == "calendar-free-busy-set")
			goto exit;
		lpsPropVal[ulCntr].ulPropTag = GetPropmap((char*)iter->sPropName.strPropname.c_str());
		
		if (lpsPropVal[ulCntr].ulPropTag == 0) // GetPropMap may not find Property Tag.
			continue;

		wstrConvProp = iter->wstrValue;
		hr = MAPIAllocateMore((wstrConvProp.length() + 1) * sizeof(WCHAR), lpsPropVal, (void**)&lpsPropVal[ulCntr].Value.lpszW);
		if (hr != hrSuccess)
			goto exit;
		wcsncpy(lpsPropVal[ulCntr].Value.lpszW ,wstrConvProp.c_str(),wstrConvProp.length() + 1);

		ulCntr++;
	}
	
	// deny rename of defalt Calendar
	if (!wstrFldName.empty() && !m_blFolderAccess)
	{
		hr = MAPI_E_NO_ACCESS;
		goto exit;
	}

	if (ulCntr != 0 && m_lpUsrFld)
		hr = m_lpUsrFld->SetProps(ulCntr, lpsPropVal, NULL);

exit:
	if(lpsPropVal)
		MAPIFreeBuffer(lpsPropVal);

	return hr;
}
/**
 * Processes the POST request from caldav client
 *
 * POST is used to request freebusy info of attendees or send
 * meeting request to attendees(used by mac ical.app only)
 *
 * @return	HRESULT
 */
HRESULT CalDAV::HrPost ()
{
	HRESULT hr = hrSuccess;
	std::string strIcal;
	ICalToMapi *lpIcalToMapi = NULL;

	hr = m_lpRequest->HrGetBody(&strIcal);
	if (hr != hrSuccess)
		goto exit;
	
	CreateICalToMapi(m_lpDefStore, m_lpAddrBook, false, &lpIcalToMapi);
	
	if (!lpIcalToMapi)
	{
		hr = MAPI_E_BAD_VALUE;
		goto exit;
	}

	hr = lpIcalToMapi->ParseICal(strIcal, m_strCharset, m_strSrvTz, m_lpImailUser, 0);
	if (hr != hrSuccess)
		goto exit;
	
	if (lpIcalToMapi->GetItemCount() == 1)
		hr = HrHandleMeeting(lpIcalToMapi);
	else
		hr = HrHandleFreebusy(lpIcalToMapi);

exit:
	
	if (lpIcalToMapi)
		delete lpIcalToMapi;

	return hr;
}

/**
 * Handles the caldav clients's request to view freebusy information
 * of attendees
 *
 * @param[in]	lpIcalToMapi	The ical to mapi conversion object
 * @return		HRESULT
 */
HRESULT CalDAV::HrHandleFreebusy(ICalToMapi *lpIcalToMapi)
{
	HRESULT hr = hrSuccess;	
	ECFreeBusySupport*	lpecFBSupport = NULL;	
	IFreeBusySupport* lpFBSupport = NULL;
	IABContainer *lpAbCont = NULL;
	IMAPITable *lpGabTable = NULL;
	MapiToICal *lpMapiToIcal = NULL;
	time_t tStart  = 0;
	time_t tEnd = 0;
	std::list<std::string> *lstUsers = NULL;
	std::string strIcal;
	std::string strUID;
	std::string strOrganiser;
	SBinary sbEid;
	WEBDAVFBINFO sWebFbInfo;
	ULONG ulObjType = 0;

	hr = lpIcalToMapi->GetItemInfo(&tStart ,&tEnd, &strUID, &lstUsers);	
	if (hr != hrSuccess)
		goto exit;

	CreateMapiToICal(m_lpAddrBook, "utf-8", &lpMapiToIcal);
	
	hr = m_lpAddrBook->GetDefaultDir(&sbEid.cb, (LPENTRYID*) &sbEid.lpb);
	if (hr != hrSuccess)
		goto exit;

	hr = m_lpSession->OpenEntry(sbEid.cb, (LPENTRYID)sbEid.lpb, NULL, 0, &ulObjType, (LPUNKNOWN *)&lpAbCont);
	if (hr != hrSuccess)
		goto exit;
	
	hr = lpAbCont->GetContentsTable(0, &lpGabTable);
	if (hr != hrSuccess)
		goto exit;
	
	hr = ECFreeBusySupport::Create(&lpecFBSupport);
	if (hr != hrSuccess)
		goto exit;

	hr = lpecFBSupport->QueryInterface(IID_IFreeBusySupport, (void**)&lpFBSupport);
		if (hr != hrSuccess)
			goto exit;
	
	hr = lpecFBSupport->Open(m_lpSession, m_lpDefStore,true);
	if (hr != hrSuccess)
		goto exit;
	
	hr = HrGetUserInfo(m_lpSession, m_lpDefStore, &strOrganiser, NULL, NULL);
	if (hr != hrSuccess)
		goto exit;

	sWebFbInfo.strOrganiser = strOrganiser;
	sWebFbInfo.tStart = tStart;
	sWebFbInfo.tEnd = tEnd;
	sWebFbInfo.strUID = strUID;

	hr = HrGetFreebusy(lpMapiToIcal, lpFBSupport , lpGabTable, lstUsers, &sWebFbInfo);
	if (hr != hrSuccess)
		goto exit;
	
	hr = WebDav::HrPost(&sWebFbInfo);
	if (hr != hrSuccess)
		goto exit;

exit:

	if (sbEid.lpb)
		MAPIFreeBuffer(sbEid.lpb);

	if (lpFBSupport)
		lpFBSupport->Release();

	if (lpGabTable)
		lpGabTable->Release();
	
	if (lpAbCont)
		lpAbCont->Release();
	
	if (lpecFBSupport)
		lpecFBSupport->Release();
	
	if (lpMapiToIcal)
		delete lpMapiToIcal;

	return hr;
}

/**
 * Handles Mac ical.app clients request to send a meeting request
 * to the attendee.
 *
 * @param[in]	lpIcalToMapi	ical to mapi conversion object
 * @return		HRESULT
 */
HRESULT CalDAV::HrHandleMeeting(ICalToMapi *lpIcalToMapi)
{
	HRESULT hr = hrSuccess;	
	LPSPropValue lpsGetPropVal = NULL;
	LPMAPIFOLDER lpOutbox = NULL;
	LPMESSAGE lpNewMsg = NULL;
	LPSPropTagArray lpsPropArray = NULL;
	LPSPropValue lpsSetPropVals = NULL;
	ULONG cValues = 0;
	ULONG ulObjType = 0;
	time_t tModTime = 0;
	SBinary sbEid = {0};
	eIcalType etype = VEVENT;	

	hr = lpIcalToMapi->GetItemInfo( 0, &etype, &tModTime, &sbEid);
	if ( hr != hrSuccess || etype != VEVENT)
	{
		hr = hrSuccess; // skip VFREEBUSY
		goto exit;
	}

	cValues = 2;
	hr = MAPIAllocateBuffer(CbNewSPropTagArray(cValues), (void **)&lpsPropArray);
	if (hr != hrSuccess)
		goto exit;

	lpsPropArray->cValues = cValues;
	lpsPropArray->aulPropTag[0] = PR_IPM_OUTBOX_ENTRYID;
	lpsPropArray->aulPropTag[1] = PR_IPM_SENTMAIL_ENTRYID;

	hr = m_lpDefStore->GetProps(lpsPropArray, 0, &cValues, &lpsGetPropVal);
	if (hr != hrSuccess && cValues != 2)
		goto exit;

	hr = m_lpDefStore->OpenEntry(lpsGetPropVal[0].Value.bin.cb, (LPENTRYID) lpsGetPropVal[0].Value.bin.lpb, NULL, MAPI_BEST_ACCESS, &ulObjType, (LPUNKNOWN*)&lpOutbox);
	if (hr != hrSuccess)
		goto exit;

	hr = lpOutbox->CreateMessage(NULL, MAPI_BEST_ACCESS, &lpNewMsg);
	if (hr != hrSuccess)
		goto exit;

	hr = lpIcalToMapi->GetItem( 0, IC2M_NO_ORGANIZER, lpNewMsg);
	if (hr != hrSuccess)
		goto exit;

	cValues = 2;
	hr = MAPIAllocateBuffer(cValues * sizeof(SPropValue), (void**)&lpsSetPropVals);
	if (hr != hrSuccess)
		goto exit;

	lpsSetPropVals[0].ulPropTag = PR_SENTMAIL_ENTRYID;
	lpsSetPropVals[0].Value.bin = lpsGetPropVal[1].Value.bin;

	lpsSetPropVals[1].ulPropTag = PR_DELETE_AFTER_SUBMIT;
	lpsSetPropVals[1].Value.b = false;

	hr = lpNewMsg->SetProps(cValues, lpsSetPropVals, NULL);
	if (hr != hrSuccess)
		goto exit;

	hr = lpNewMsg->SaveChanges(KEEP_OPEN_READWRITE);
	if (hr != hrSuccess)
		goto exit;

	hr = lpNewMsg->SubmitMessage(0);
	if (hr != hrSuccess)
		goto exit;

exit:
	
	if(hr == hrSuccess)
		m_lpRequest->HrResponseHeader(200, "Ok");
	else
		m_lpRequest->HrResponseHeader(400, "Bad Request");

	if (lpNewMsg)
		lpNewMsg->Release();

	if (lpOutbox)
		lpOutbox->Release();

	if (lpsGetPropVal)
		MAPIFreeBuffer(lpsGetPropVal);

	if (lpsSetPropVals)
		MAPIFreeBuffer(lpsSetPropVals);

	return hr;
}

/**
 * Converts the mapi message specified by EntryID to ical string.
 *
 * @param[in]	lpEid		EntryID of the mapi msg to be converted
 * @param[in]	lpMtIcal	mapi to ical conversion object
 * @param[in]	ulFlags		Flags used for mapi to ical conversion
 * @param[out]	strIcal		ical string output
 * @return		HRESULT
 */
HRESULT CalDAV::HrConvertToIcal(LPSPropValue lpEid, MapiToICal *lpMtIcal, ULONG ulFlags, std::string *lpstrIcal)
{
	HRESULT hr = hrSuccess;
	IMessage *lpMessage = NULL;
	ULONG ulObjType = 0;

	hr = m_lpActiveStore->OpenEntry(lpEid->Value.bin.cb, (LPENTRYID)lpEid->Value.bin.lpb, NULL, MAPI_BEST_ACCESS, &ulObjType, (LPUNKNOWN *)&lpMessage);
	if (hr != hrSuccess && ulObjType == MAPI_MESSAGE)
	{
		m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Error opening calendar entry, error code : 0x%08X", hr);
		goto exit;
	}

	hr = lpMtIcal->AddMessage(lpMessage, m_strSrvTz, ulFlags);
	if (hr != hrSuccess)
	{
		m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Error converting mapi message to ical, error code : 0x%08X", hr);
		goto exit;
	}

	hr = lpMtIcal->Finalize(0, NULL, lpstrIcal);
	if (hr != hrSuccess)
	{
		m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Error creating ical data, error code : 0x%08X", hr);
		goto exit;
	}

	lpMtIcal->ResetObject();

exit:
	
	if(lpMessage)
		lpMessage->Release();
	
	return hr;
}

/**
 * Set Values for properties requested by caldav client
 * 
 * @param[in]	lpProps			SpropValue array containing values of requested properties
 * @param[in]	ulPropCount		Count of propety values present in lpProps
 * @param[in]	lpMtIcal		mapi to ical conversion object pointer
 * @param[in]	ulFlags			Flags used during mapi to ical conversion
 * @param[in]	lstDavProps		Pointer to structure containing properties requested by client
 * @param[out]	lpsResponse		Pointer to Response structure
 * @return		HRESULT
 * @retval		hrSuccess		Always returns hrSuccess
 */
HRESULT CalDAV::HrMapValtoStruct(LPSPropValue lpProps, ULONG ulPropCount, MapiToICal *lpMtIcal, ULONG ulFlags, std::list<WEBDAVPROPERTY> *lstDavProps, WEBDAVRESPONSE *lpsResponse)
{
	HRESULT hr = hrSuccess;
	std::list<WEBDAVPROPERTY>::iterator iterProperty;
	WEBDAVPROPERTY sWebProperty;
	std::string strProperty;
	std::string strIcal;
	std::wstring wstrPrincipalURL;
	LPSPropValue lpFoundProp = NULL;
	WEBDAVPROP sWebProp;
	WEBDAVPROP sWebPropNotFound;
	WEBDAVPROPSTAT sPropStat;
	ULONG ulDepth = 0;
	ULONG ulFolderType = 0;

	m_lpRequest->HrGetDepth(&ulDepth);

	lpFoundProp = PpropFindProp(lpProps, ulPropCount, PR_CONTAINER_CLASS_A);
	if (lpFoundProp && !strncmp (lpFoundProp->Value.lpszA, "IPF.Appointment", strlen("IPF.Appointment")))
		ulFolderType = CALENDAR_FOLDER;
	else if (lpFoundProp && !strncmp (lpFoundProp->Value.lpszA, "IPF.Task", strlen("IPF.Task")))
		ulFolderType = TASKS_FOLDER;
	else
		ulFolderType = OTHER_FOLDER;

	if (!(m_ulUrlFlag & REQ_PUBLIC) && m_wstrFldName.empty()) {
		wstrPrincipalURL = (L"/caldav/" + ( m_wstrFldOwner.empty() 
											? m_wstrUser
											: m_wstrFldOwner ) + L"/");

	} else {
		// use the request url
		m_lpRequest->HrGetUrl(&wstrPrincipalURL);
	}

	HrSetDavPropName(&(sWebProp.sPropName), "prop", CALDAVNSDEF);
	HrSetDavPropName(&(sWebPropNotFound.sPropName), "prop", CALDAVNSDEF);
	for (iterProperty = lstDavProps->begin(); iterProperty != lstDavProps->end() ; iterProperty++)
	{
		WEBDAVVALUE sWebVal;

		sWebProperty.lstItems.clear();
		sWebProperty.lstValues.clear();
		
		sWebProperty = *iterProperty;
		strProperty = sWebProperty.sPropName.strPropname;

		lpFoundProp = PpropFindProp(lpProps, ulPropCount, GetPropmap((char*)strProperty.c_str()));
		if (strProperty == "resourcetype") {
			
			// do not set resourcetype for REPORT request(ical data)
			if(!lpMtIcal){
				HrSetDavPropName(&(sWebVal.sPropName), "collection", CALDAVNSDEF);
				sWebProperty.lstValues.push_back(sWebVal);
			}
			
			if(lpFoundProp && (!strcmp(lpFoundProp->Value.lpszA ,"IPF.Appointment") || !strcmp(lpFoundProp->Value.lpszA , "IPF.Task"))) {
				HrSetDavPropName(&(sWebVal.sPropName), "calendar", CALDAVNS);
				sWebProperty.lstValues.push_back(sWebVal);
			}else if (m_wstrFldName == L"Inbox") {
				HrSetDavPropName(&(sWebVal.sPropName), "schedule-inbox", CALDAVNS);
				sWebProperty.lstValues.push_back(sWebVal);
			}else if(m_wstrFldName == L"Outbox") {
				HrSetDavPropName(&(sWebVal.sPropName), "schedule-outbox", CALDAVNS);
				sWebProperty.lstValues.push_back(sWebVal);
			}

		} else if (strProperty == "displayname") {
			
			if (m_blIsCLMAC && ((ulFlags & PROPFIND_ROOT) == PROPFIND_ROOT))
				sWebProperty.wstrValue = m_wstrUserFullName; // @todo store PR_DISPLAY_NAME_W ?

			else
				sWebProperty.wstrValue = SPropValToWString(lpFoundProp);
			
		} else if (strProperty == "calendar-user-address-set") {
			
			HrSetDavPropName(&(sWebVal.sPropName), "href", CALDAVNSDEF);
			if ((ulFlags & PROPFIND_ROOT) == PROPFIND_ROOT)
				sWebVal.wstrValue = L"mailto:" + U2W(m_strUserEmail);
			else
				sWebVal.wstrValue = L"mailto:" + SPropValToWString(lpFoundProp);
			sWebProperty.lstValues.push_back(sWebVal);
			
			sWebVal.wstrValue = m_wstrCalHome;
			if (!m_wstrCalHome.empty())
				sWebProperty.lstValues.push_back(sWebVal);

		} else if (strProperty == "acl" || strProperty == "current-user-privilege-set") {
			
			HrBuildACL(&sWebProperty);

		} else if (strProperty == "supported-report-set") {
			
			HrBuildReportSet(&sWebProperty);

		} else if (lpFoundProp && 
					(strProperty == "calendar-description" || strProperty == "getctag" || 
					 strProperty == "getetag" || strProperty == "last-name" || 
					 strProperty == "first-name")
					) {
			
						sWebProperty.wstrValue = SPropValToWString(lpFoundProp);

		} else if (strProperty == "email-address-set") {

			HrSetDavPropName(&(sWebVal.sPropName), "email-address", CALDAVNSDEF);
			sWebVal.wstrValue = ((ulFlags & PROPFIND_ROOT) == PROPFIND_ROOT)? U2W(m_strUserEmail) : SPropValToWString(lpFoundProp);
			sWebProperty.lstValues.push_back(sWebVal);

		} else if (strProperty == "schedule-inbox-URL" && (m_ulUrlFlag & REQ_PUBLIC) == 0) {
			HrSetDavPropName(&(sWebVal.sPropName), "href", CALDAVNSDEF);
			sWebVal.wstrValue = (L"/caldav/" + (m_wstrFldOwner.empty()? m_wstrUser : m_wstrFldOwner) + L"/Inbox");
			sWebProperty.lstValues.push_back(sWebVal);

		} else if (strProperty == "schedule-outbox-URL" && (m_ulUrlFlag & REQ_PUBLIC) == 0) {
			HrSetDavPropName(&(sWebVal.sPropName), "href", CALDAVNSDEF);
			sWebVal.wstrValue = (L"/caldav/" + (m_wstrFldOwner.empty()? m_wstrUser : m_wstrFldOwner) + L"/Outbox");
			sWebProperty.lstValues.push_back(sWebVal);

		} else if (strProperty == "supported-calendar-component-set") {
			
			if (ulFolderType == CALENDAR_FOLDER) {
				HrSetDavPropName(&(sWebVal.sPropName), "comp","name", "VEVENT", CALDAVNS);
				sWebProperty.lstValues.push_back(sWebVal);

				// actually even only for the standard calender folder
				HrSetDavPropName(&(sWebVal.sPropName), "comp","name", "VFREEBUSY", CALDAVNS);
				sWebProperty.lstValues.push_back(sWebVal);
			}
			else if (ulFolderType == TASKS_FOLDER) {
				HrSetDavPropName(&(sWebVal.sPropName), "comp","name", "VTODO", CALDAVNS);
				sWebProperty.lstValues.push_back(sWebVal);
			}

			HrSetDavPropName(&(sWebVal.sPropName), "comp","name", "VTIMEZONE", CALDAVNS);
			sWebProperty.lstValues.push_back(sWebVal);

		} else if (lpFoundProp && lpMtIcal && strProperty == "calendar-data") {
			
			hr = HrConvertToIcal(lpFoundProp, lpMtIcal, ulFlags, &strIcal);
			sWebProperty.wstrValue = U2W(strIcal);
			if (hr != hrSuccess || sWebProperty.wstrValue.empty()){
				// ical data is empty so discard this calendar entry
				HrSetDavPropName(&(lpsResponse->sStatus.sPropName), "status", CALDAVNSDEF);
				lpsResponse->sStatus.wstrValue = L"HTTP/1.1 404 Not Found";
				goto exit;
			}

			strIcal.clear();

		} else if (strProperty == "calendar-order") {
			
			if (ulFolderType == CALENDAR_FOLDER) {
				lpFoundProp = PpropFindProp(lpProps, ulPropCount, PR_ENTRYID);
				if (lpFoundProp)
					HrGetCalendarOrder(lpFoundProp->Value.bin, &sWebProperty.wstrValue);

			} else  {
				// set value to 2 for tasks and non default calendar
				// so that ical.app shows default calendar in the list first everytime
				// if this value is left empty, ical.app tries to reset the order
				sWebProperty.wstrValue = L"2";
			}

		} else if (strProperty == "getcontenttype") {

			sWebProperty.wstrValue = L"text/calendar";

		} else if (strProperty == "principal-collection-set") {

			sWebProperty.wstrValue = L"/caldav/";

		} else if (strProperty == "current-user-principal" || strProperty == "owner") {

			HrSetDavPropName(&(sWebVal.sPropName), "href", CALDAVNSDEF);
			sWebVal.wstrValue = (L"/caldav/" + (m_wstrFldOwner.empty()? m_wstrUser : m_wstrFldOwner) + L"/");
			sWebProperty.lstValues.push_back(sWebVal);

		}else if (strProperty == "calendar-home-set" || strProperty == "principal-URL") {

			HrSetDavPropName(&(sWebVal.sPropName), "href", CALDAVNSDEF);
			sWebVal.wstrValue = wstrPrincipalURL;
			sWebProperty.lstValues.push_back(sWebVal);

		} else if (strProperty == "calendar-user-type") {
			if (SPropValToString(lpFoundProp) == "0")
				sWebProperty.wstrValue = L"INDIVIDUAL";
		} else if (strProperty == "record-type"){

			sWebProperty.wstrValue = L"users";

		} else {
			sWebPropNotFound.lstProps.push_back(sWebProperty);
			continue;
		}

		sWebProp.lstProps.push_back(sWebProperty);
	}
	
	HrSetDavPropName(&(sPropStat.sPropName), "propstat", CALDAVNSDEF);
	HrSetDavPropName(&(sPropStat.sStatus.sPropName), "status", CALDAVNSDEF);

	if( !sWebProp.lstProps.empty()) {
		sPropStat.sStatus.wstrValue = L"HTTP/1.1 200 OK";
		sPropStat.sProp = sWebProp;
		lpsResponse->lstsPropStat.push_back (sPropStat);
	}
	
	if( !sWebPropNotFound.lstProps.empty()) {
		sPropStat.sStatus.wstrValue = L"HTTP/1.1 404 Not Found";
		sPropStat.sProp = sWebPropNotFound;
		lpsResponse->lstsPropStat.push_back (sPropStat);
	}
exit:
	return hr;
}

/**
 * Sets the Calendar Order to 1 for default calendar folder
 *
 * The function checks if the folder is the default calendar folder, if true sets
 * the calendar-order to 1 or else is set to 2
 *
 * The calendar-order property is set to show the default calendar first in 
 * the calendar list in ical.app(Mac). This makes the default zarafa calendar default
 * in ical.app too.
 *
 * if the value is left empty ical.app tries to reset the order and sometimes sets
 * a tasks folder as default calendar
 *
 * @param[in]	sbEid				Entryid of the Folder to be checked
 * @param[out]	wstrCalendarOrder	string output in which the calendar order is set
 * @return		mapi error codes
 * @retval		MAPI_E_CALL_FAILED	the calendar-order is not set for this folder
 */
HRESULT CalDAV::HrGetCalendarOrder(SBinary sbEid, std::wstring *lpwstrCalendarOrder)
{
	HRESULT hr = hrSuccess;
	LPMAPIFOLDER lpRootCont = NULL;
	LPSPropValue lpProp = NULL;
	ULONG ulObjType = 0;
	ULONG ulResult = 0;

	lpwstrCalendarOrder->assign(L"2");
	
	hr = m_lpActiveStore->OpenEntry(0, NULL, NULL, 0, &ulObjType, (LPUNKNOWN*)&lpRootCont);
	if (hr != hrSuccess || ulObjType != MAPI_FOLDER) {
		m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Error opening root Container of user %ls, error code: (0x%08X)", m_wstrUser.c_str(), hr);
		goto exit;
	}

	// get default calendar folder entry id from root container
	hr = HrGetOneProp(lpRootCont, PR_IPM_APPOINTMENT_ENTRYID, &lpProp);
	if (hr != hrSuccess)
		goto exit;

	hr = m_lpActiveStore->CompareEntryIDs(sbEid.cb, (LPENTRYID)sbEid.lpb, lpProp->Value.bin.cb, (LPENTRYID)lpProp->Value.bin.lpb, 0, &ulResult);
	if (hr == hrSuccess && ulResult == TRUE)
		*lpwstrCalendarOrder = L"1";
exit:
	if (lpProp)
		MAPIFreeBuffer(lpProp);

	if (lpRootCont)
		lpRootCont->Release();

	return hr;
}
/**
 * Handles the MOVE http request
 *
 * The request moves mapi message from one folder to another folder
 * The url request refers to the current location of the message and
 * the destination tag in http header specifies the destination folder.
 * The message guid value is same in both url and destination tag.
 *
 * @return mapi error codes
 *
 * @retval MAPI_E_DECLINE_COPY	The mapi message is not moved as etag values does not match
 * @retval MAPI_E_NOT_FOUND		The mapi message refered by guid is not found
 * @retval MAPI_E_NO_ACCESS		The user does not sufficient rights on the mapi message
 *
 */
HRESULT CalDAV::HrMove()
{
	HRESULT hr = hrSuccess;
	LPMAPIFOLDER lpDestFolder = NULL;
	std::wstring wstrDestination;
	std::wstring wstrDestFolder;
	std::wstring wstrGuid;
	std::string strGuid;
	
	hr = m_lpRequest->HrGetDestination(&wstrDestination);
	if (hr != hrSuccess)
		goto exit;

	hr = HrParseURL(wstrDestination, NULL, NULL, &wstrDestFolder);
	if (hr != hrSuccess)
		goto exit;

	hr = HrFindFolder(m_lpActiveStore, m_lpNamedProps, m_lpLogger, false, wstrDestFolder, &lpDestFolder);
	if (hr != hrSuccess)
		goto exit;
	
	strGuid = StripGuid(wstrDestination);

	hr = HrMoveEntry(strGuid, lpDestFolder);
exit:

	// @todo - set e-tag value for the new saved message, so ical.app does not send the GET request
	if (hr == hrSuccess)
		m_lpRequest->HrResponseHeader(200, "OK");
	else if (hr == MAPI_E_DECLINE_COPY)
		m_lpRequest->HrResponseHeader(412, "Precondition Failed"); // entry is modifid on server (sunbird & TB)
	else if( hr == MAPI_E_NOT_FOUND)
		m_lpRequest->HrResponseHeader(404, "Not Found");
	else if(hr == MAPI_E_NO_ACCESS)
		m_lpRequest->HrResponseHeader(403, "Forbidden");
	else
		m_lpRequest->HrResponseHeader(400, "Bad Request");

	if (lpDestFolder)
		lpDestFolder->Release();

	return hr;
}
