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
#include "WebDav.h"
#include "stringutil.h"
#include "CommonUtil.h"
#include <libical/ical.h>

using namespace std;

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/**
 * Default constructor
 * @param[in]	lpRequest	Pointer to http Request object
 * @param[in]	lpSession	Pointer to mapi session of the user
 * @param[in]	lpLogger	Pointer to logger object to log errors and information
 */
WebDav::WebDav(Http *lpRequest, IMAPISession *lpSession, ECLogger *lpLogger, std::string strSrvTz, std::string strCharset):ProtocolBase(lpRequest, lpSession, lpLogger, strSrvTz, strCharset)
{	
	m_lpXmlDoc  = NULL;
}

/**
 * Default Destructor
 */
WebDav::~WebDav()
{
	if (m_lpXmlDoc)
		xmlFreeDoc(m_lpXmlDoc);
}

/**
 * Parse the xml request body
 * @return		HRESULT
 * @retval		MAPI_E_INVALID_PARAMETER		Unable to parse the xml data
 */
HRESULT WebDav::HrParseXml()
{
	HRESULT hr = hrSuccess;
	std::string strBody;

	ASSERT(m_lpXmlDoc == NULL);
	if (m_lpXmlDoc != NULL)
		return hr;

	m_lpRequest->HrGetBody(&strBody);

	m_lpXmlDoc = xmlReadMemory((char *)strBody.c_str(),(int)strBody.length(), "PROVIDE_BASE.xml", NULL,  XML_PARSE_NOBLANKS);
	if (m_lpXmlDoc == NULL)
		hr = MAPI_E_INVALID_PARAMETER;

	strBody.clear();

	return hr;
}

/**
 * Parse the PROPFIND request
 *
 * Example of PROPFIND request
 *
 * <D:propfind xmlns:D="DAV:" xmlns:CS="http://calendarserver.org/ns/">
 *		<D:prop>
 *			<D:resourcetype/>
 *			<D:owner/>
 *			<CS:getctag/>
 *		</D:prop>
 * </D:propfind>
 *
 * @return		HRESULT
 * @retval		MAPI_E_CORRUPT_DATA		Invalid xml data
 * @retval		MAPI_E_NOT_FOUND		Folder requested in url not found
 *
 */
HRESULT WebDav::HrPropfind()
{
	HRESULT hr = hrSuccess;
		
	WEBDAVPROPSTAT sPropStat;
	WEBDAVRESPONSE sDavResp;
	WEBDAVMULTISTATUS sDavMStatus;
	WEBDAVREQSTPROPS sDavReqsProps;
	WEBDAVPROP sDavPropRet;
	std::string strFldPath;
	std::string strXml;
	xmlNode * lpXmlNode = NULL;

	// libxml parser parses the xml data and returns the DomTree pointer.
	hr = HrParseXml();
	if (hr != hrSuccess)
		goto exit;

	lpXmlNode = xmlDocGetRootElement(m_lpXmlDoc);

	if (!lpXmlNode || !lpXmlNode->name || xmlStrcmp(lpXmlNode->name, (const xmlChar *)"propfind"))
	{
		hr = MAPI_E_CORRUPT_DATA;
		goto exit;
	}
	
	lpXmlNode = lpXmlNode->children;
	if (!lpXmlNode || !lpXmlNode->name || xmlStrcmp(lpXmlNode->name, (const xmlChar *)"prop"))
	{
		hr = MAPI_E_CORRUPT_DATA;
		goto exit;
	}
		
	HrSetDavPropName(&(sDavPropRet.sPropName),lpXmlNode);
	lpXmlNode = lpXmlNode->children;

	while (lpXmlNode)
	{
		WEBDAVPROPERTY sProperty;
		
		HrSetDavPropName(&(sProperty.sPropName),lpXmlNode);
		sDavPropRet.lstProps.push_back(sProperty);
		lpXmlNode = lpXmlNode->next;
	}

	/*
	 * Call to CALDAV::HrHandlePropfind
	 * This function Retrieves data from server and adds it to the structure.
	 */		
	sDavReqsProps.sProp = sDavPropRet;
	hr = HrHandlePropfind(&sDavReqsProps, &sDavMStatus);
	if (hr != hrSuccess)
		goto exit;

	// Convert WEBMULTISTATUS structure to xml data.
	hr = RespStructToXml(&sDavMStatus, &strXml);
	if (hr != hrSuccess)
	{
		m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Unable to convert reponse to xml: 0x%08X", hr);
		goto exit;
	}
	
exit:
	if(hr == hrSuccess)
	{
		m_lpRequest->HrResponseHeader(207, "Multi-Status");
		m_lpRequest->HrResponseHeader("Content-Type", "application/xml; charset=\"utf-8\""); 
		m_lpRequest->HrResponseBody(strXml);
	}
	else if (hr == MAPI_E_NOT_FOUND)
		m_lpRequest->HrResponseHeader(404, "Not Found");
	else if (hr == MAPI_E_NO_ACCESS)
		m_lpRequest->HrResponseHeader(403, "Access Denied");
	else 
		m_lpRequest->HrResponseHeader(500, "Internal Server Error");

	return hr;
}


/**
 * Converts WEBDAVMULTISTATUS response structure to xml data
 * @param[in]	sDavMStatus		Pointer to WEBDAVMULTISTATUS structure
 * @param[out]	strXml			Return string for xml data
 *
 * @return		HRESULT
 * @retval		MAPI_E_NOT_ENOUGH_MEMORY	Error alocating memory
 * @retval		MAPI_E_CALL_FAILED			Error initializing xml writer
 * @retval		MAPI_E_CALL_FAILED			Error writing xml data
 */
HRESULT WebDav::RespStructToXml(WEBDAVMULTISTATUS *sDavMStatus, std::string *strXml)
{
	HRESULT hr = hrSuccess;
	int ulRet = 0;
	std::string strNsPrefix;
	xmlTextWriterPtr xmlWriter = NULL;
	xmlBufferPtr xmlBuff = NULL;
	std::list<WEBDAVRESPONSE>::iterator iterResp;
	std::string strNs;	
	std::map<std::string,std::string>::iterator iterMapNS;

	strNsPrefix = "C";
	xmlBuff = xmlBufferCreate();
	
	if (xmlBuff == NULL)
	{
		hr = MAPI_E_NOT_ENOUGH_MEMORY;
		m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Error allocating memory to xmlBuffer");
		goto exit;
	}

	//Initialize Xml-Writer
	xmlWriter = xmlNewTextWriterMemory(xmlBuff, 0);
	if (xmlWriter == NULL)
	{
		hr = MAPI_E_CALL_FAILED;
		m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Error Initializing xmlWriter");
		goto exit;
	}

	// let xml use enters and spaces to make it somewhat readable
	// if indedentaion is not present, iCal.app does not show suggestionlist.
	xmlTextWriterSetIndent(xmlWriter, 1);

	//start xml-data i.e <xml version="1.0" encoding="utf-8"?>
	ulRet = xmlTextWriterStartDocument(xmlWriter, NULL, "UTF-8", NULL);
	if (ulRet < 0)
	{
		hr = MAPI_E_CALL_FAILED;
		m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Error writing xml data");
		goto exit;
	}

	// @todo move this default to sDavMStatus constructor, never different.
	if(sDavMStatus->sPropName.strNS.empty())
		sDavMStatus->sPropName.strNS = "DAV:";
	if (sDavMStatus->sPropName.strPropname.empty())
		sDavMStatus->sPropName.strPropname = "multistatus";

	strNsPrefix = "C";
	m_mapNs[sDavMStatus->sPropName.strNS.c_str()] = "C";

	//<multistatus>	
	xmlTextWriterStartElementNS(xmlWriter,
								(const xmlChar *)strNsPrefix.c_str(),
								(const xmlChar *)sDavMStatus->sPropName.strPropname.c_str(),
								(const xmlChar *)sDavMStatus->sPropName.strNS.c_str());

	//write all xmlname spaces in main tag.
	for(iterMapNS = m_mapNs.begin(); iterMapNS != m_mapNs.end(); iterMapNS++)
	{
		std::string strprefix;
		strNs = iterMapNS->first;
		if(sDavMStatus->sPropName.strNS == strNs || strNs.empty())
			continue;
		hr = RegisterNs(strNs, &strNsPrefix);
		strprefix = "xmlns:" + strNsPrefix;
		
		xmlTextWriterWriteAttribute	(xmlWriter,
									(const xmlChar *) strprefix.c_str(),
									(const xmlChar *) strNs.c_str());
	}
	// <response>
	iterResp = sDavMStatus->lstResp.begin();
	for (int i=0; sDavMStatus->lstResp.end() != iterResp; i++,iterResp++)
	{
		WEBDAVRESPONSE sDavResp;
		sDavResp = *iterResp;
		hr = HrWriteSResponse(xmlWriter, &strNsPrefix, sDavResp);
		if(hr != hrSuccess)
			goto exit;
	}

	//</multistatus>
	ulRet = xmlTextWriterEndElement(xmlWriter);
	//EOF
	ulRet = xmlTextWriterEndDocument(xmlWriter);
	// force write to buffer
	ulRet = xmlTextWriterFlush(xmlWriter);

	strXml->assign((char *)xmlBuff->content, xmlBuff->use);

exit:
	if (xmlWriter)
		xmlFreeTextWriter(xmlWriter);

	if (xmlBuff)
		xmlBufferFree(xmlBuff);

	return hr;
}

/**
 * Parse the REPORT request, identify the type of report request
 *
 * @return	HRESULT
 * @retval	MAPI_E_CORRUPT_DATA		Invalid xml request
 *
 */
HRESULT WebDav::HrReport()
{
	HRESULT hr = hrSuccess;
	xmlNode * lpXmlNode = NULL;
	
	hr = HrParseXml();
	if (hr != hrSuccess)
		goto exit;

	lpXmlNode = xmlDocGetRootElement(m_lpXmlDoc);
	if (!lpXmlNode)
	{
		hr = MAPI_E_CORRUPT_DATA;
		goto exit;
	}

	if (lpXmlNode->name && !xmlStrcmp(lpXmlNode->name, (const xmlChar *)"calendar-query") )
	{
		//CALENDAR-QUERY
		//Retrieves the list of GUIDs
		hr = HrHandleRptCalQry();

	}
	else if (lpXmlNode->name && !xmlStrcmp(lpXmlNode->name, (const xmlChar *)"calendar-multiget") )
	{
		//MULTIGET
		//Retrieves Ical data for each GUID that client requests
		hr = HrHandleRptMulGet();
	}
	else if (lpXmlNode->name && !xmlStrcmp(lpXmlNode->name, (const xmlChar *)"principal-property-search"))
	{
		// suggestion list while adding attendees on mac iCal.
		hr = HrPropertySearch();
	}
	else if (lpXmlNode->name && !xmlStrcmp(lpXmlNode->name, (const xmlChar *)"principal-search-property-set"))
	{
		// which all properties to be searched while searching for attendees.
		hr = HrPropertySearchSet();
	}
	else if (lpXmlNode->name && !xmlStrcmp(lpXmlNode->name, (const xmlChar *)"expand-property"))
	{
		// ignore expand-property
		m_lpRequest->HrResponseHeader(200, "OK");
	}
	else
		m_lpRequest->HrResponseHeader(500, "Internal Server Error");

exit:
	return hr;
}

/**
 * Parses the calendar-query REPORT request.
 *
 * The request is for retrieving list of calendar entries in folder
 * Example of the request
 *
 * <calendar-query xmlns:D="DAV:" xmlns="urn:ietf:params:xml:ns:caldav">
 *		<D:prop>
 *			<D:getetag/>
 *		</D:prop>
 *		<filter>
 *			<comp-filter name="VCALENDAR">
 *				<comp-filter name="VEVENT"/>
 *			</comp-filter>
 *		</filter>
 * </calendar-query>
 *
 * @return	HRESULT
 * @retval	MAPI_E_CORRUPT_DATA		Invalid xml data
 */
// @todo, do not return MAPI_E_CORRUPT_DATA, but make a clean error report for the client.
// only return MAPI_E_CORRUPT_DATA when xml is invalid (which normal working clients won't send)
HRESULT WebDav::HrHandleRptCalQry()
{
	HRESULT hr = hrSuccess;
	xmlNode * lpXmlNode = NULL;
	xmlNode * lpXmlChildNode = NULL;
	xmlAttr * lpXmlAttr = NULL;
	xmlNode * lpXmlChildAttr = NULL;
	WEBDAVREQSTPROPS sReptQuery;
	WEBDAVMULTISTATUS sWebMStatus;
	std::string strXml;

	lpXmlNode = xmlDocGetRootElement(m_lpXmlDoc);
	if (!lpXmlNode)
	{
		hr = MAPI_E_CORRUPT_DATA;
		goto exit;
	}

	// REPORT calendar-query
	sReptQuery.sPropName.strPropname.assign((const char*) lpXmlNode->name);
	sReptQuery.sFilter.tStart = 0;

	//HrSetDavPropName(&(sReptQuery.sPropName),lpXmlNode);
	lpXmlNode = lpXmlNode->children;

	while (lpXmlNode) {
		if (xmlStrcmp(lpXmlNode->name, (const xmlChar *)"filter") == 0)
		{
			// @todo convert xml filter to mapi restriction			

			// "old" code
			if (!lpXmlNode->children) {
				hr = MAPI_E_CORRUPT_DATA;
				goto exit;
			}
			HrSetDavPropName(&(sReptQuery.sFilter.sPropName),lpXmlNode);
			lpXmlChildNode = lpXmlNode->children;

			lpXmlAttr = lpXmlChildNode->properties;
			if (lpXmlAttr && lpXmlAttr->children)
				lpXmlChildAttr = lpXmlAttr->children;
			else
			{
				hr = MAPI_E_CORRUPT_DATA;
				goto exit;
			}

			if (!lpXmlChildAttr || !lpXmlChildAttr->content || xmlStrcmp(lpXmlChildAttr->content, (const xmlChar *)"VCALENDAR"))
			{
				hr = E_FAIL;
				goto exit;	
			}

			sReptQuery.sFilter.lstFilters.push_back((char *)lpXmlChildAttr->content);

			lpXmlChildNode = lpXmlChildNode->children;
			if (lpXmlChildNode->properties && lpXmlChildNode->properties->children)
			{
				lpXmlAttr = lpXmlChildNode->properties;
				lpXmlChildAttr = lpXmlAttr->children;
			}
			else
			{
				hr = MAPI_E_CORRUPT_DATA;
				goto exit;
			}

			if (lpXmlChildAttr == NULL || lpXmlChildAttr->content == NULL) {
				hr = MAPI_E_CORRUPT_DATA;
				goto exit;
			}

			if (xmlStrcmp( lpXmlChildAttr->content, (const xmlChar *)"VTODO") == 0 || xmlStrcmp(lpXmlChildAttr->content, (const xmlChar *)"VEVENT") == 0) {
				sReptQuery.sFilter.lstFilters.push_back((char *)lpXmlChildAttr->content);
			} else {
				hr = MAPI_E_CORRUPT_DATA;
				goto exit;
			}

			// filter not done here.., time-range in lpXmlChildNode->children.
			if (lpXmlChildNode->children) {
				lpXmlChildNode = lpXmlChildNode->children;
				while (lpXmlChildNode) {
					if (xmlStrcmp(lpXmlChildNode->name, (const xmlChar *)"time-range") == 0) {

						if (lpXmlChildNode->properties == NULL || lpXmlChildNode->properties->children == NULL) {
							lpXmlChildNode = lpXmlChildNode->next;
							continue;
						}

						lpXmlChildAttr = lpXmlChildNode->properties->children;
						if (xmlStrcmp(lpXmlChildAttr->name, (const xmlChar *)"start") == 0) {
							// timestamp from ical
							icaltimetype iTime = icaltime_from_string((const char *)lpXmlChildAttr->content);
							sReptQuery.sFilter.tStart = icaltime_as_timet(iTime);
							// @note this is still being ignored in CalDavProto::HrListCalEntries
						}
						// other lpXmlChildAttr->name .. like "end" maybe?
					}
					// other lpXmlChildNode->name ..  like, uh?
					lpXmlChildNode = lpXmlChildNode->next;
				}
			}
		}
		else if (xmlStrcmp(lpXmlNode->name, (const xmlChar *)"prop") == 0)
		{
			if (lpXmlNode->ns && lpXmlNode->ns->href)
				sReptQuery.sPropName.strNS.assign((const char*) lpXmlNode->ns->href);
			else
				sReptQuery.sPropName.strNS.assign(WEBDAVNS);

			HrSetDavPropName(&(sReptQuery.sProp.sPropName),lpXmlNode);
			lpXmlChildNode = lpXmlNode->children;
			while (lpXmlChildNode)
			{
				//properties requested by client.
				WEBDAVPROPERTY sWebProperty;

				HrSetDavPropName(&(sWebProperty.sPropName),lpXmlChildNode);
				sReptQuery.sProp.lstProps.push_back(sWebProperty);
				lpXmlChildNode = lpXmlChildNode->next;
			}
		} else {
			m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Skipping unknown XML element: %s", lpXmlNode->name);
		}
		lpXmlNode = lpXmlNode->next;
	}

	// @todo, load depth 0 ? .. see propfind version.

	//Retrieve Data from the server & return WEBMULTISTATUS structure.
	hr = HrListCalEntries(&sReptQuery, &sWebMStatus);
	if (hr != hrSuccess)
		goto exit;

	//Convert WEBMULTISTATUS structure to xml data.
	hr = RespStructToXml(&sWebMStatus, &strXml);
	if (hr != hrSuccess)
		 goto exit;

	//Respond to the client with xml data.
	m_lpRequest->HrResponseHeader(207, "Multi-Status");
	m_lpRequest->HrResponseHeader("Content-Type", "application/xml; charset=\"utf-8\""); 
	m_lpRequest->HrResponseBody(strXml);

exit:
	if (hr != hrSuccess)
	{
		m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Unable to process report calendar query: 0x%08X", hr);
		m_lpRequest->HrResponseHeader(500, "Internal Server Error");
	}

	return hr;
}
/**
 * Parses the calendar-multiget REPORT request
 *
 * The request contains the list of guid and the response should have the 
 * ical data
 * Example of the request
 * <calendar-multiget xmlns:D="DAV:" xmlns="urn:ietf:params:xml:ns:caldav">
 *		<D:prop>
 *			<D:getetag/>
 *			<calendar-data/>
 *		</D:prop>
 *		<D:href>/caldav/user/path-to-calendar/d8516650-b6fc-11dd-92a5-a494cf95cb3a.ics</D:href>
 * </calendar-multiget>
 *
 * @return	HRESULT
 * @retval	MAPI_E_CORRUPT_DATA		Invalid xml data in request 
 */
HRESULT WebDav::HrHandleRptMulGet()
{
	HRESULT hr = hrSuccess;
	WEBDAVRPTMGET sRptMGet;
	WEBDAVMULTISTATUS sWebMStatus;
	xmlNode * lpXmlNode = NULL;
	xmlNode * lpXmlChildNode = NULL;
	xmlNode * lpXmlContentNode = NULL;
	std::string strXml;
	
	lpXmlNode = xmlDocGetRootElement(m_lpXmlDoc);
	if (!lpXmlNode)
	{
		hr = MAPI_E_CORRUPT_DATA;
		goto exit;
	}

	//REPORT Multiget Request.
	// xml data to structures
	HrSetDavPropName(&(sRptMGet.sPropName),lpXmlNode);
	lpXmlNode = lpXmlNode->children;
	if (!lpXmlNode || !lpXmlNode->name || xmlStrcmp(lpXmlNode->name, (const xmlChar *)"prop"))
	{
		hr = MAPI_E_CORRUPT_DATA;
		goto exit;
	}

	HrSetDavPropName(&(sRptMGet.sProp.sPropName),lpXmlNode);
	lpXmlChildNode = lpXmlNode->children;
	while (lpXmlChildNode)
	{
		//Reqeuested properties of the client.
		WEBDAVPROPERTY sWebProperty;
	
		HrSetDavPropName(&(sWebProperty.sPropName),lpXmlChildNode);
		sRptMGet.sProp.lstProps.push_back(sWebProperty);
		lpXmlChildNode = lpXmlChildNode->next;
	}
	
	if (lpXmlNode->next)
		lpXmlChildNode = lpXmlNode->next;
	else
	{
		hr = MAPI_E_CORRUPT_DATA;
		goto exit;
	}

	while (lpXmlChildNode)
	{	
		//List of ALL GUIDs whose ical data is requested.
		WEBDAVVALUE sWebVal;
		std::string strGuid;
		size_t found = 0;
		lpXmlContentNode = lpXmlChildNode->children;

		HrSetDavPropName(&(sWebVal.sPropName),lpXmlChildNode);
		strGuid.assign((const char *) lpXmlContentNode->content);

		found = strGuid.rfind("/");
		if(found == string::npos || (found+1) == strGuid.length()) {
			lpXmlChildNode = lpXmlChildNode->next;
			continue;
		}
		found++;

		// strip url and .ics from guid, and convert %hex to real data
		strGuid.erase(0, found);
		strGuid.erase(strGuid.length() - 4);
		strGuid = urlDecode(strGuid);
		sWebVal.strValue = strGuid;

		sRptMGet.lstWebVal.push_back(sWebVal);
		lpXmlChildNode = lpXmlChildNode->next;
	}

	//Retrieve Data from the Server and return WEBMULTISTATUS structure.
	hr = HrHandleReport(&sRptMGet, &sWebMStatus);
	if (hr != hrSuccess)
		goto exit;

	//Convert WEBMULTISTATUS structure to xml data
	hr = RespStructToXml(&sWebMStatus, &strXml);
	if (hr != hrSuccess)
		goto exit;

	m_lpRequest->HrResponseHeader(207, "Multi-Status");
	m_lpRequest->HrResponseHeader("Content-Type", "application/xml; charset=\"utf-8\""); 
	m_lpRequest->HrResponseBody(strXml);

exit:
	if(hr != hrSuccess)
	{
		m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Unable to process report multi-get: 0x%08X", hr);
		m_lpRequest->HrResponseHeader(500, "Internal Server Error");
	}
	
	return hr;
}

/**
 * Parses the property-search request and generates xml response
 *
 * The response contains list of users matching the request
 * for the attendeee suggestion list in mac
 *
 * @return	HRESULT
 */
HRESULT WebDav::HrPropertySearch()
{
	HRESULT hr = hrSuccess;
	WEBDAVRPTMGET sRptMGet;
	WEBDAVMULTISTATUS sWebMStatus;
	WEBDAVPROPERTY sWebProperty;
	WEBDAVVALUE sWebVal;
	xmlNode *lpXmlNode = NULL;
	xmlNode *lpXmlChildNode = NULL;
	xmlNode *lpXmlSubChildNode = NULL;
	std::string strXml;

	lpXmlNode = xmlDocGetRootElement(m_lpXmlDoc);
	if (!lpXmlNode)
	{
		hr = MAPI_E_CORRUPT_DATA;
		goto exit;
	}
	lpXmlNode = lpXmlNode->children;
	// <principal-property-search>
	HrSetDavPropName(&(sRptMGet.sPropName),lpXmlNode);

	//REPORT Multiget Request.
	// xml data to structures
	while (lpXmlNode) {
		
		// <property-search>
		if (!lpXmlNode || !lpXmlNode->name || xmlStrcmp(lpXmlNode->name, (const xmlChar *)"property-search"))
		{
			hr = hrSuccess;
			break;
		}
		
		// <prop>
		lpXmlChildNode = lpXmlNode->children;
		if (!lpXmlChildNode || !lpXmlChildNode->name || xmlStrcmp(lpXmlChildNode->name, (const xmlChar *)"prop"))
		{
			hr = MAPI_E_CORRUPT_DATA;
			goto exit;;
		}
		HrSetDavPropName(&(sRptMGet.sProp.sPropName),lpXmlChildNode);
		
		// eg <diplayname>
		lpXmlSubChildNode = lpXmlChildNode->children;		
		HrSetDavPropName(&(sWebVal.sPropName),lpXmlSubChildNode);

		// <match>
		if (!lpXmlChildNode->next) {
			hr = MAPI_E_CORRUPT_DATA;
			goto exit;
		}

		lpXmlChildNode = lpXmlChildNode->next;		
		if(lpXmlChildNode->children->content)
			sWebVal.strValue.assign((char*)lpXmlChildNode->children->content);
		sRptMGet.lstWebVal.push_back(sWebVal);
		if(lpXmlNode->next)
			lpXmlNode = lpXmlNode->next;
	}

	if (!lpXmlNode || !lpXmlNode->name || xmlStrcmp(lpXmlNode->name, (const xmlChar *)"prop"))
	{
		hr = MAPI_E_CORRUPT_DATA;
		goto exit;
	}
	lpXmlChildNode = lpXmlNode->children;
	while (lpXmlChildNode)
	{	
		HrSetDavPropName(&(sWebProperty.sPropName),lpXmlChildNode);
		
		sRptMGet.sProp.lstProps.push_back(sWebProperty);
		lpXmlChildNode = lpXmlChildNode->next;
	}

	//Retrieve Data from the Server and return WEBMULTISTATUS structure.
	hr = HrHandlePropertySearch(&sRptMGet, &sWebMStatus);
	if (hr != hrSuccess)
		goto exit;

	//Convert WEBMULTISTATUS structure to xml data
	hr = RespStructToXml(&sWebMStatus, &strXml);
	if (hr != hrSuccess)
		goto exit;

	m_lpRequest->HrResponseHeader(207 , "Multi-Status");
	m_lpRequest->HrResponseHeader("Content-Type", "application/xml; charset=\"utf-8\""); 
	m_lpRequest->HrResponseBody(strXml);

exit:
	if(hr != hrSuccess)
	{
		m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Unable to process report multi-get: 0x%08X", hr);
		m_lpRequest->HrResponseHeader(500, "Internal Server Error");
	}
	
	return hr;
}
/**
 * Handles the property-search-set request
 *
 * The response contains list of property that the client can request while 
 * requesting attendees in suggestion list
 *
 * @return	HRESULT
 */
HRESULT WebDav::HrPropertySearchSet()
{
	HRESULT hr = hrSuccess;
	WEBDAVMULTISTATUS sDavMStatus;
	std::string strXml;

	hr = HrHandlePropertySearchSet(&sDavMStatus);
	if (hr != hrSuccess)
		goto exit;

	hr = RespStructToXml(&sDavMStatus, &strXml);
	if (hr != hrSuccess)
		goto exit;

	m_lpRequest->HrResponseHeader(200, "OK");
	m_lpRequest->HrResponseHeader("Content-Type", "application/xml; charset=\"utf-8\"");
	m_lpRequest->HrResponseBody(strXml);
exit:
	return hr;
}
/**
 * Generates xml response for POST request to view freebusy information
 * 
 * @param[in]	lpsWebFbInfo	WEBDAVFBINFO structure, contains all freebusy information
 * @return		HRESULT 
 */
HRESULT WebDav::HrPostFreeBusy(WEBDAVFBINFO *lpsWebFbInfo)
{
	HRESULT hr = hrSuccess;
	std::list<WEBDAVFBUSERINFO>::iterator itFbUserInfo;
	WEBDAVMULTISTATUS sWebMStatus;
	
	std::string strXml;

	HrSetDavPropName(&sWebMStatus.sPropName,"schedule-response", CALDAVNS);

	for (itFbUserInfo = lpsWebFbInfo->lstFbUserInfo.begin(); itFbUserInfo != lpsWebFbInfo->lstFbUserInfo.end(); itFbUserInfo++)
	{
		WEBDAVPROPERTY sWebProperty;
		WEBDAVVALUE sWebVal;
		WEBDAVRESPONSE sWebResPonse;

		HrSetDavPropName(&sWebResPonse.sPropName,"response", CALDAVNS);	

		HrSetDavPropName(&sWebProperty.sPropName,"recipient", CALDAVNS);
		HrSetDavPropName(&sWebVal.sPropName,"href", WEBDAVNS);
		
		sWebVal.strValue = "mailto:" + itFbUserInfo->strUser;
		sWebProperty.lstValues.push_back(sWebVal);
		sWebResPonse.lstProps.push_back(sWebProperty);

		sWebProperty.lstValues.clear();
		
		HrSetDavPropName(&sWebProperty.sPropName,"request-status", CALDAVNS);
		sWebProperty.strValue = itFbUserInfo->strIcal.empty() ? "3.8;No authority" : "2.0;Success";
		sWebResPonse.lstProps.push_back(sWebProperty);
		
		if (!itFbUserInfo->strIcal.empty()) {	
			HrSetDavPropName(&sWebProperty.sPropName,"calendar-data", CALDAVNS);
			sWebProperty.strValue = itFbUserInfo->strIcal;
			sWebResPonse.lstProps.push_back(sWebProperty);
		}

		sWebMStatus.lstResp.push_back(sWebResPonse);
	}

	hr = RespStructToXml(&sWebMStatus, &strXml);
	if (hr != hrSuccess)
		goto exit;

exit:
	if (hr == hrSuccess) {
		m_lpRequest->HrResponseHeader(200, "OK");
		m_lpRequest->HrResponseHeader("Content-Type", "application/xml; charset=\"utf-8\"");
		m_lpRequest->HrResponseBody(strXml);
	} else {
		m_lpRequest->HrResponseHeader(404, "Not found");
	}

	return hr;
}

/**
 * Converts WEBDAVVALUE structure to xml data
 * @param[in]	xmlWriter	xml writer to write xml data
 * @param[in]	sWebVal		WEBDAVVALUE structure to be converted to xml
 * @param[in]	szNsPrefix	Current Namespace prefix
 * @return		HRESULT
 * @retval		MAPI_E_CALL_FAILED	Unable to write xml data
 */
HRESULT WebDav::WriteData(xmlTextWriterPtr xmlWriter, WEBDAVVALUE sWebVal, std::string * szNsPrefix)
{
	std::string strNs;
	int ulRet = 0;
	convert_context converter;
	HRESULT hr = hrSuccess;

	strNs = sWebVal.sPropName.strNS;

	if(strNs.empty())
	{
		ulRet = xmlTextWriterWriteElement(xmlWriter,
										  (const xmlChar *)sWebVal.sPropName.strPropname.c_str(),
										  (const xmlChar *)sWebVal.strValue.c_str());
		goto exit;
	}

	// Retrieve the namespace prefix if present in map.
	hr = GetNs(szNsPrefix, &strNs);
	// if namespace is not present in the map then insert it into map.
	if (hr != hrSuccess)
		hr = RegisterNs(strNs, szNsPrefix);	

	if (hr != hrSuccess)
		goto exit;
	
	/*Write xml none of the form
	 *	<D:href>/caldav/user/calendar/entryGUID.ics</D:href>
	 */
	ulRet =	xmlTextWriterWriteElementNS(xmlWriter,
										(const xmlChar *)szNsPrefix->c_str(),
										(const xmlChar *)sWebVal.sPropName.strPropname.c_str(),
										(const xmlChar *)(strNs.empty() ? NULL : strNs.c_str()),
										(const xmlChar *)sWebVal.strValue.c_str());
	if (ulRet == -1) {
		hr = MAPI_E_CALL_FAILED;
		goto exit;
	}
	
exit:
	
	return hr;
}
/**
 * Converts WEBDAVPROPNAME	to xml data
 * @param[in]	xmlWriter		xml writer to write xml data
 * @param[in]	sWebPropName	sWebPropName structure to be written into xml
 * @param[in]	lpstrNsPrefix	current namespace prefix
 * @return		HRESULT
 */
HRESULT WebDav::WriteNode(xmlTextWriterPtr xmlWriter, WEBDAVPROPNAME sWebPropName, std::string * lpstrNsPrefix)
{
	std::string strNs;
	int ulRet = 0;
	HRESULT hr = hrSuccess;

	strNs = sWebPropName.strNS;
	
	if(strNs.empty())
	{
		ulRet = xmlTextWriterStartElement	(xmlWriter,
											(const xmlChar *)sWebPropName.strPropname.c_str());
		goto exit;
	}

	hr = GetNs(lpstrNsPrefix, &strNs);
	if (hr != hrSuccess)
	   hr =	RegisterNs(strNs, lpstrNsPrefix);

	if (hr != hrSuccess)
		goto exit;
	/*Write Xml data of the form
	 * <D:propstat>
	 * the end tag </D:propstat> is written by "xmlTextWriterEndElement(xmlWriter)"
	 */
	ulRet =	xmlTextWriterStartElementNS (xmlWriter,
										(const xmlChar *)lpstrNsPrefix->c_str(),
										(const xmlChar *)sWebPropName.strPropname.c_str(),
										(const xmlChar *)(strNs.empty() ? NULL : strNs.c_str()));

	if (ulRet == -1) {
		hr = MAPI_E_CALL_FAILED;
		goto exit;
	}
	
	if (!sWebPropName.strPropAttribName.empty())
		ulRet = xmlTextWriterWriteAttribute(xmlWriter, (const xmlChar *)sWebPropName.strPropAttribName.c_str(), (const xmlChar *) sWebPropName.strPropAttribValue.c_str());
	
exit:
	return hr;
}
/**
 * Adds namespace prefix into map of namespaces
 * @param[in]	strNs			Namespace name
 * @param[in]	lpstrNsPrefix	Namespace prefix
 * @return		HRESULT			Always returns hrSuccess 
 */
HRESULT WebDav::RegisterNs(std::string strNs, std::string *lpstrNsPrefix)
{
	(*lpstrNsPrefix)[0]++;
	m_mapNs[strNs] = *lpstrNsPrefix;

	return hrSuccess;
}
/**
 * Returns the namespace prefix for the corresponding namespace name
 * @param[in,out]	lpstrPrefx	Return string for namespace prefix
 * @param[in,out]	lpstrNs		Namespace name, is set to empty string if namespace prefix found
 * @return			HRESULT
 * @retval			MAPI_E_NOT_FOUND	Namespace prefix not found
 */
HRESULT WebDav::GetNs(std::string * lpstrPrefx, std::string *lpstrNs)
{
	HRESULT hr = hrSuccess;
	map <std::string,std::string>::iterator itMpNs;

	itMpNs = m_mapNs.find(*lpstrNs);
	if (itMpNs != m_mapNs.end())
	{
		lpstrPrefx->assign(itMpNs->second);
		lpstrNs->clear();
	}
	else
		hr = MAPI_E_NOT_FOUND;

	return hr;
}
/**
 * Converts WEBDAVRESPONSE structure to xml data
 *
 * @param[in]	xmlWriter		xml writer to write xml data
 * @param[in]	lpstrNsPrefix	Pointer to string containing the current namespace prefix
 * @param[in]	sResponse		WEBDAVRESPONSE structure to be converted to xml data
 * @return		HRESULT 
 */
HRESULT WebDav::HrWriteSResponse(xmlTextWriterPtr xmlWriter,std::string *lpstrNsPrefix ,WEBDAVRESPONSE sResponse)
{
	HRESULT hr = hrSuccess;
	WEBDAVRESPONSE sWebResp;
	std::string strNsPrefix;
	std::list<WEBDAVPROPSTAT>::iterator iterPropStat;
	std::list<WEBDAVPROPERTY>::iterator iterProperty;
	int ulRet = 0;

	sWebResp = sResponse;
	// <response>
	hr = WriteNode(xmlWriter, sWebResp.sPropName, lpstrNsPrefix);
	
	// <href>xxxxxxxxxxxxxxxx</href>
	if (!sWebResp.sHRef.sPropName.strPropname.empty())
		hr = WriteData(xmlWriter, sWebResp.sHRef, lpstrNsPrefix);

	// Only set for broken calendar entries
	// <D:status>HTTP/1.1 404 Not Found</D:status>
	if (!sWebResp.sStatus.sPropName.strPropname.empty())
		hr = WriteData(xmlWriter,sWebResp.sStatus , lpstrNsPrefix);

	iterPropStat = sWebResp.lstsPropStat.begin();
	while(sWebResp.lstsPropStat.end() != iterPropStat)
	{
		WEBDAVPROPSTAT sDavPropStat;
		sDavPropStat = *iterPropStat;
		HrWriteSPropStat(xmlWriter, lpstrNsPrefix, sDavPropStat);
		iterPropStat++;
	}

	if (!sWebResp.lstProps.empty())
	{
		HrWriteResponseProps(xmlWriter, lpstrNsPrefix, &(sWebResp.lstProps));
	}

	//</response>
	ulRet = xmlTextWriterEndElement(xmlWriter);

	return hr;
}

/**
 * Converts WEBDAVPROPERTY list to xml data
 *
 * @param[in]	xmlWriter		xml writer to write xml data
 * @param[in]	lpstrNsPrefix	Pointer to string containing the current namespace prefix
 * @param[in]	lplstProps		WEBDAVPROPERTY list to be converted to xml data
 * @return		HRESULT 
 */
HRESULT WebDav::HrWriteResponseProps(xmlTextWriterPtr xmlWriter, std::string *lpstrNsPrefix, std::list<WEBDAVPROPERTY> *lplstProps)
{
	HRESULT hr = hrSuccess;
	std::list<WEBDAVPROPERTY>::iterator iterProp;
	ULONG ulRet = 0;

	iterProp = lplstProps->begin();
	while (lplstProps->end() != iterProp)
	{
		WEBDAVPROPERTY sWebProperty;
		sWebProperty = *iterProp;
		
		if (!sWebProperty.strValue.empty())
		{
			WEBDAVVALUE sWebVal;
			sWebVal.sPropName = sWebProperty.sPropName;
			sWebVal.strValue = sWebProperty.strValue;
			//<getctag xmlns="xxxxxxxxxxx">xxxxxxxxxxxxxxxxx</getctag>
			WriteData(xmlWriter,sWebVal,lpstrNsPrefix);
		}
		else
		{	//<resourcetype>
			WriteNode(xmlWriter, sWebProperty.sPropName,lpstrNsPrefix);
		}
		
		//loop for sub properties
		for (int k = 0; !sWebProperty.lstValues.empty(); k++)
		{
			WEBDAVVALUE sWebVal;
			sWebVal = sWebProperty.lstValues.front();
			//<collection/>
			if (!sWebVal.strValue.empty())
				WriteData(xmlWriter,sWebVal,lpstrNsPrefix);
			else
			{
				WriteNode(xmlWriter, sWebVal.sPropName, lpstrNsPrefix);
				ulRet = xmlTextWriterEndElement(xmlWriter);
			}
			sWebProperty.lstValues.pop_front();
		}
		if (sWebProperty.strValue.empty())
			ulRet = xmlTextWriterEndElement(xmlWriter);
		iterProp++;
	}

	return hr;

}
/**
 * Converts WEBDAVPROPSTAT structure to xml data
 *
 * @param[in]	xmlWriter		xml writer to write xml data
 * @param[in]	lpstrNsPrefix	Pointer to string containing the current namespace prefix
 * @param[in]	lpsPropStat		WEBDAVPROPSTAT structure to be converted to xml data
 * @return		HRESULT 
 */
HRESULT WebDav::HrWriteSPropStat(xmlTextWriterPtr xmlWriter, std::string *lpstrNsPrefix, WEBDAVPROPSTAT lpsPropStat)
{
	HRESULT hr = hrSuccess;
	WEBDAVPROPSTAT sWebPropStat;
	WEBDAVPROP sWebProp;
	int ulRet = 0;
	std::list<WEBDAVPROPERTY>::iterator iterProp;
	
	sWebPropStat = lpsPropStat;
	//<propstat>
	hr = WriteNode(xmlWriter, sWebPropStat.sPropName,lpstrNsPrefix);
	
	sWebProp = sWebPropStat.sProp;

	//<prop>
	hr = WriteNode(xmlWriter, sWebProp.sPropName,lpstrNsPrefix);

	iterProp = sWebProp.lstProps.begin();
	//loop	for properties list
	while (sWebProp.lstProps.end() != iterProp)
	{			
		WEBDAVPROPERTY sWebProperty;
		sWebProperty = *iterProp;
		
		if (!sWebProperty.strValue.empty())
		{
			WEBDAVVALUE sWebVal;
			sWebVal.sPropName = sWebProperty.sPropName;
			sWebVal.strValue = sWebProperty.strValue;
			//<getctag xmlns="xxxxxxxxxxx">xxxxxxxxxxxxxxxxx</getctag>
			WriteData(xmlWriter,sWebVal,lpstrNsPrefix);
		}
		else
		{	//<resourcetype>
			WriteNode(xmlWriter, sWebProperty.sPropName,lpstrNsPrefix);
		}
		
		if(!sWebProperty.lstItems.empty())
			HrWriteItems(xmlWriter, lpstrNsPrefix, &sWebProperty);

        //loop for sub properties
		for (int k = 0; !sWebProperty.lstValues.empty(); k++)
		{
			WEBDAVVALUE sWebVal;
			sWebVal = sWebProperty.lstValues.front();
			//<collection/>
			if (!sWebVal.strValue.empty())
				WriteData(xmlWriter,sWebVal,lpstrNsPrefix);
			else
			{
				WriteNode(xmlWriter, sWebVal.sPropName, lpstrNsPrefix);
				ulRet = xmlTextWriterEndElement(xmlWriter);
			}
			sWebProperty.lstValues.pop_front();
		}
		//end tag if started
		//</resourcetype>
		if (sWebProperty.strValue.empty())
			ulRet = xmlTextWriterEndElement(xmlWriter);

		iterProp++;
	}
	//</prop>
	ulRet = xmlTextWriterEndElement(xmlWriter);
	
	//<status xmlns="xxxxxxx">HTTP/1.1 200 OK</status>
	hr = WriteData(xmlWriter,sWebPropStat.sStatus,lpstrNsPrefix);

	//</propstat>
	ulRet = xmlTextWriterEndElement(xmlWriter);

	return hr;
}

/**
 * Converts Items List to xml data
 *
 * @param[in]	xmlWriter		xml writer to write xml data
 * @param[in]	lpstrNsPrefix	Pointer to string containing the current namespace prefix
 * @param[in]	lpsWebProperty	WEBDAVPROPERTY structure containing the list of items
 * @return		HRESULT			Always returns hrSuccess
 */
HRESULT WebDav::HrWriteItems(xmlTextWriterPtr xmlWriter, std::string *lpstrNsPrefix,WEBDAVPROPERTY *lpsWebProperty)
{
	HRESULT hr = hrSuccess;
	WEBDAVITEM sDavItem;
	ULONG ulDepthPrev = 0;
	ULONG ulDepthCur = 0;
	bool blFirst = true;

	ulDepthPrev = lpsWebProperty->lstItems.front().ulDepth;
	while(!lpsWebProperty->lstItems.empty())
	{
		sDavItem = lpsWebProperty->lstItems.front();
		ulDepthCur = sDavItem.ulDepth;
		
		if(ulDepthCur >= ulDepthPrev)
		{
			if(ulDepthCur == ulDepthPrev && !blFirst && sDavItem.sDavValue.sPropName.strPropname != "ace")
			{
				xmlTextWriterEndElement(xmlWriter);
			}
			blFirst = false;

			if (!sDavItem.sDavValue.strValue.empty())
			{
				WriteData(xmlWriter,sDavItem.sDavValue,lpstrNsPrefix);
				sDavItem.ulDepth = sDavItem.ulDepth - 1;
			}
			else
			{
				//<resourcetype>
				WriteNode(xmlWriter, sDavItem.sDavValue.sPropName,lpstrNsPrefix);
			}
		}
		else
		{
			for(ULONG i = ulDepthCur ; i <= ulDepthPrev ; i++)
			{
			 xmlTextWriterEndElement(xmlWriter);
			}
			
			if (!sDavItem.sDavValue.strValue.empty())
			{
				WriteData(xmlWriter,sDavItem.sDavValue,lpstrNsPrefix);
				sDavItem.ulDepth = sDavItem.ulDepth - 1;
			}
			else
			{
				//<resourcetype>
				WriteNode(xmlWriter, sDavItem.sDavValue.sPropName,lpstrNsPrefix);
			}
		}
		ulDepthPrev = sDavItem.ulDepth;
		lpsWebProperty->lstItems.pop_front();
	}

	for(ULONG i = 0 ; i <= ulDepthPrev ; i++)
	{
		xmlTextWriterEndElement(xmlWriter);
	}

	return hr;
}

/**
 * Set Name and namespace to WEBDAVPROPNAME structure from xmlNode
 *
 * The structure contains information about a xml element
 *
 * @param[in]	lpsDavPropName		Strutcture to which all the values are set
 * @param[in]	lpXmlNode			Pointer to xmlNode object, it contains name and namespace
 *
 * @return		HRESULT		Always returns hrsuccess
 */
HRESULT WebDav::HrSetDavPropName(WEBDAVPROPNAME *lpsDavPropName, xmlNode *lpXmlNode)
{
	lpsDavPropName->strPropname.assign((const char*)lpXmlNode->name);
	lpsDavPropName->strNS.assign((const char*)lpXmlNode->ns->href);
	if(!lpsDavPropName->strNS.empty())
		m_mapNs[lpsDavPropName->strNS] = "";

	lpsDavPropName->strPropAttribName = "";
	lpsDavPropName->strPropAttribValue = "";
	return hrSuccess;
}

/**
 * Set Name and namespace to WEBDAVPROPNAME structure
 *
 * The structure contains information about a xml element
 *
 * @param[in]	lpsDavPropName		Strutcture to which all the values are set
 * @param[in]	strPropName			Name of the element 
 * @param[in]	strNs				Namespace of the element
 *
 * @return		HRESULT		Always returns hrsuccess
 */
HRESULT WebDav::HrSetDavPropName(WEBDAVPROPNAME *lpsDavPropName, std::string strPropName, std::string strNs)
{
	lpsDavPropName->strPropname.assign(strPropName);
	lpsDavPropName->strNS.assign(strNs);
	if (!lpsDavPropName->strNS.empty())
		m_mapNs[lpsDavPropName->strNS].clear();
	lpsDavPropName->strPropAttribName.clear();
	lpsDavPropName->strPropAttribValue.clear();

	return hrSuccess;
}

/**
 * Set Name, attribute and namespace to WEBDAVPROPNAME structure
 *
 * The structure contains information about a xml element
 *
 * @param[in]	lpsDavPropName		Strutcture to which all the values are set
 * @param[in]	strPropName			Name of the element
 * @param[in]	strPropAttribName	Attribute's name to be set in the element
 * @param[in]	strPropAttribValue	Attribute's value to be set in the element
 * @param[in]	strNs				Namespace of the element
 *
 * @return		HRESULT		Always returns hrsuccess
 */
HRESULT WebDav::HrSetDavPropName(WEBDAVPROPNAME *lpsDavPropName,std::string strPropName, std::string strPropAttribName, std::string strPropAttribValue, std::string strNs)
{
	lpsDavPropName->strPropname.assign(strPropName);
	lpsDavPropName->strNS.assign(strNs);
	lpsDavPropName->strPropAttribName = strPropAttribName;
	lpsDavPropName->strPropAttribValue = strPropAttribValue;
	if (!lpsDavPropName->strNS.empty())
		m_mapNs[lpsDavPropName->strNS] = "";
	return hrSuccess;
}

/**
 * Parse the PROPPATCH xml request
 * 
 * @return	HRESULT
 * @retval	MAPI_E_CORRUPT_DATA		Invalid xml data in request
 * @retval	MAPI_E_NO_ACCESS		Not enough permissions to edit folder names or comments
 * @retval	MAPI_E_COLLISION		A folder with same name already exists
 */
HRESULT WebDav::HrPropPatch()
{
	HRESULT hr = hrSuccess;	
	WEBDAVPROP sDavProp;
	WEBDAVMULTISTATUS sDavMStatus;
	std::string strFldPath;
	std::string strXml;
	xmlNode * lpXmlNode = NULL;

	//libxml parser parses the xml data and returns the DomTree pointer.
	hr = HrParseXml();
	if (hr != hrSuccess)
		goto exit;

	lpXmlNode = xmlDocGetRootElement(m_lpXmlDoc);
	if (!lpXmlNode || !lpXmlNode->name || xmlStrcmp(lpXmlNode->name, (const xmlChar *)"propertyupdate"))
	{
		hr = MAPI_E_CORRUPT_DATA;
		goto exit;
	}

	lpXmlNode = lpXmlNode->children;
	if (!lpXmlNode || !lpXmlNode->name || xmlStrcmp(lpXmlNode->name, (const xmlChar *)"set"))
	{
		hr = MAPI_E_CORRUPT_DATA;
		goto exit;
	}

	lpXmlNode = lpXmlNode->children;
	if (!lpXmlNode || !lpXmlNode->name || xmlStrcmp(lpXmlNode->name, (const xmlChar *)"prop"))
	{
		hr = MAPI_E_CORRUPT_DATA;
		goto exit;
	}

	if(lpXmlNode->ns && lpXmlNode->ns->href)
		HrSetDavPropName(&(sDavProp.sPropName),"prop",(char *)lpXmlNode->ns->href);
	else
		HrSetDavPropName(&(sDavProp.sPropName),"prop", WEBDAVNS);

	lpXmlNode = lpXmlNode->children;
	while(lpXmlNode)
	{
		WEBDAVPROPERTY sProperty;

		if(lpXmlNode->ns && lpXmlNode->ns->href)
			HrSetDavPropName(&(sProperty.sPropName),(char *)lpXmlNode->name,(char*)lpXmlNode->ns->href);
		else
			HrSetDavPropName(&(sProperty.sPropName),(char *)lpXmlNode->name, WEBDAVNS);

		if (lpXmlNode->children && lpXmlNode->children->content) {
			sProperty.strValue = (char *)lpXmlNode->children->content;
		}

		sDavProp.lstProps.push_back(sProperty);

		lpXmlNode = lpXmlNode->next;
	}	

	hr = HrHandlePropPatch(&sDavProp, &sDavMStatus);
	if (hr != hrSuccess)
		goto exit;

	// Convert WEBMULTISTATUS structure to xml data.
	hr = RespStructToXml(&sDavMStatus, &strXml);
	if (hr != hrSuccess)
	{
		m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Unable to convert reponse to xml: 0x%08X", hr);
		goto exit;
	}

exit:
	if (hr != hrSuccess) {
		// this is important for renaming your calendar folder
		if (hr == MAPI_E_COLLISION)
			m_lpRequest->HrResponseHeader(409, "Conflict");
		else
			m_lpRequest->HrResponseHeader(403, "Forbidden");
	} else {
		m_lpRequest->HrResponseHeader(207 , "Multi-Status");
		m_lpRequest->HrResponseHeader("Content-Type", "application/xml; charset=\"utf-8\""); 
		m_lpRequest->HrResponseBody(strXml);
	}

	return hr;
}
/**
 * Handle xml parsing for MKCALENDAR request
 *
 * @return	HRESULT
 * @retval	MAPI_E_CORRUPT_DATA		Invalid xml request
 * @retval	MAPI_E_COLLISION		Folder with same name exists
 * @retval	MAPI_E_NO_ACCESS		Not enough permissions to create a folder
 */
// input url: /caldav/username/7F2A8EB0-5E2C-4EB7-8B46-6ECBFE91BA3F/
/* input xml:
	  <x0:mkcalendar xmlns:x1="DAV:" xmlns:x2="http://apple.com/ns/ical/" xmlns:x0="urn:ietf:params:xml:ns:caldav">
	   <x1:set>
	    <x1:prop>
		 <x1:displayname>Untitled</x1:displayname>
		 <x2:calendar-color>#492BA1FF</x2:calendar-color>
		 <x2:calendar-order>9</x2:calendar-order>
		 <x0:calendar-free-busy-set><YES/></x0:calendar-free-busy-set>
		 <x0:calendar-timezone> ... ical timezone data snipped ... </x0:calendar-timezone>
		</x1:prop>
	   </x1:set>
	  </x0:mkcalendar>
*/
HRESULT WebDav::HrMkCalendar()
{
	HRESULT hr = hrSuccess;
	xmlNode * lpXmlNode = NULL;
	WEBDAVPROP sDavProp;

	hr = HrParseXml();
	if(hr != hrSuccess)
	{
		m_lpLogger->Log(EC_LOGLEVEL_ERROR,"Parsing Error For MKCALENDAR");
		goto exit;
	}
	
	lpXmlNode = xmlDocGetRootElement(m_lpXmlDoc);
	if (!lpXmlNode || !lpXmlNode->name || xmlStrcmp(lpXmlNode->name, (const xmlChar *)"mkcalendar"))
	{
		hr = MAPI_E_CORRUPT_DATA;
		goto exit;
	}
	
	lpXmlNode = lpXmlNode->children;
	if (!lpXmlNode || !lpXmlNode->name || xmlStrcmp(lpXmlNode->name, (const xmlChar *)"set"))
	{
		hr = MAPI_E_CORRUPT_DATA;
		goto exit;
	}

	lpXmlNode = lpXmlNode->children;
	if (!lpXmlNode || !lpXmlNode->name || xmlStrcmp(lpXmlNode->name, (const xmlChar *)"prop"))
	{
		hr = MAPI_E_CORRUPT_DATA;
		goto exit;
	}
	
	if(lpXmlNode->ns && lpXmlNode->ns->href)
		HrSetDavPropName(&(sDavProp.sPropName),(char*)lpXmlNode->name,(char*)lpXmlNode->ns->href);
	else
		HrSetDavPropName(&(sDavProp.sPropName),(char*)lpXmlNode->name,WEBDAVNS);

	lpXmlNode = lpXmlNode->children;
	while(lpXmlNode)
	{
		WEBDAVPROPERTY sProperty;

		if (lpXmlNode->ns && lpXmlNode->ns->href)
			HrSetDavPropName(&(sProperty.sPropName),(char*)lpXmlNode->name,(char*)lpXmlNode->ns->href);
		else
			HrSetDavPropName(&(sProperty.sPropName),(char*)lpXmlNode->name,WEBDAVNS);

		if (lpXmlNode->children && lpXmlNode->children->content)
			sProperty.strValue = (char*)lpXmlNode->children->content;

		// @todo we should have a generic xml to structs converter, this is *way* too hackish
		if (sProperty.sPropName.strPropname.compare("supported-calendar-component-set") == 0) {
			xmlNodePtr lpXmlChild = lpXmlNode->children;
			while (lpXmlChild) {
				if (lpXmlChild->type == XML_ELEMENT_NODE && xmlStrcmp(lpXmlChild->name, (const xmlChar *)"comp") == 0) {
					if (lpXmlChild->properties && lpXmlChild->properties->children && lpXmlChild->properties->children->content)
						sProperty.strValue = (char*)lpXmlChild->properties->children->content;
				}
				lpXmlChild = lpXmlChild->next;
			}
		}

		sDavProp.lstProps.push_back(sProperty);	

		lpXmlNode = lpXmlNode->next;
	}

	hr = HrHandleMkCal(&sDavProp);

exit:

	if(hr == MAPI_E_COLLISION)
	{
		m_lpRequest->HrResponseHeader(409,"CONFLICT");
		m_lpRequest->HrResponseBody("Folder with same name already exists");
	}
	else if(hr == MAPI_E_NO_ACCESS)
		m_lpRequest->HrResponseHeader(403 ,"Forbidden");
	else if(hr == hrSuccess)
		m_lpRequest->HrResponseHeader(201,"Created");
	else
		m_lpRequest->HrResponseHeader(500,"Bad Request");

	return hr;
}
