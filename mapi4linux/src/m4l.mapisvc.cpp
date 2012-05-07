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
#include "stringutil.h"

#include "m4l.mapisvc.h"
#include "mapix.h"
#include "mapidefs.h"
#include "mapicode.h"
#include "mapitags.h"
#include "mapiutil.h"

#include "Util.h"

#include <iostream>
#include <arpa/inet.h>

#include <boost/algorithm/string.hpp>
namespace ba = boost::algorithm;
#include <boost/filesystem.hpp>
namespace bfs = boost::filesystem;

using namespace std;

// linux version of PR_SERVICE_DLL_NAME
#define PR_SERVICE_SO_NAME                         PROP_TAG( PT_TSTRING,   0x3D13)
#define PR_SERVICE_SO_NAME_W                       PROP_TAG( PT_UNICODE,   0x3D13)
#define PR_SERVICE_SO_NAME_A                       PROP_TAG( PT_STRING8,   0x3D13)

INFLoader::INFLoader()
{
	// only the properties used in mapisvc.inf
	m_mapDefs["PR_AB_PROVIDER_ID"] = PR_AB_PROVIDER_ID;			// HEX2BIN
	m_mapDefs["PR_DISPLAY_NAME"] = PR_DISPLAY_NAME_A;			// STRING8
	m_mapDefs["PR_MDB_PROVIDER"] = PR_MDB_PROVIDER;				// HEX2BIN
	m_mapDefs["PR_PROVIDER_DISPLAY"] = PR_PROVIDER_DISPLAY_A;	// STRING8
	m_mapDefs["PR_RESOURCE_FLAGS"] = PR_RESOURCE_FLAGS;			// ULONG
	m_mapDefs["PR_RESOURCE_TYPE"] = PR_RESOURCE_TYPE;			// ULONG
	m_mapDefs["PR_SERVICE_DLL_NAME"] = PR_SERVICE_DLL_NAME_A;	// STRING8
	m_mapDefs["PR_SERVICE_SO_NAME"] = PR_SERVICE_SO_NAME_A;		// STRING8, custom property
	m_mapDefs["PR_SERVICE_ENTRY_NAME"] = PR_SERVICE_ENTRY_NAME; // Always STRING8

	// only the definitions used in mapisvc.inf
	m_mapDefs["SERVICE_SINGLE_COPY"] = SERVICE_SINGLE_COPY;
	m_mapDefs["SERVICE_PRIMARY_IDENTITY"] = SERVICE_PRIMARY_IDENTITY;

	m_mapDefs["MAPI_AB_PROVIDER"] = MAPI_AB_PROVIDER;
	m_mapDefs["MAPI_STORE_PROVIDER"] = MAPI_STORE_PROVIDER;

	m_mapDefs["STATUS_PRIMARY_IDENTITY"] = STATUS_PRIMARY_IDENTITY;
	m_mapDefs["STATUS_DEFAULT_STORE"] = STATUS_DEFAULT_STORE;
	m_mapDefs["STATUS_PRIMARY_STORE"] = STATUS_PRIMARY_STORE;
	m_mapDefs["STATUS_NO_DEFAULT_STORE"] = STATUS_NO_DEFAULT_STORE;
}

INFLoader::~INFLoader()
{
}

/** 
 * Loads all *.inf files in the paths returned by GetINFPaths()
 * 
 * @return MAPI Error code
 */
HRESULT INFLoader::LoadINFs()
{
	HRESULT hr = hrSuccess;
	vector<string> paths = GetINFPaths();

	for (vector<string>::iterator i = paths.begin(); i != paths.end(); i++) {
		bfs::path infdir(*i);
		if (!bfs::exists(infdir))
			// silently continue or print init error?
			continue;

		bfs::directory_iterator inffile_last;
		for (bfs::directory_iterator inffile(infdir); inffile != inffile_last; inffile++) {
			if (is_directory(inffile->status()))
				continue;

			string strFilename = inffile->path().file_string();
			string::size_type pos = strFilename.rfind(".inf", strFilename.size(), strlen(".inf"));

			if (pos == string::npos || strFilename.size() - pos != strlen(".inf"))
				// silently skip files not ending in pos
				continue;

			hr = LoadINF(inffile->path().file_string().c_str());
			if (hr != hrSuccess)
				goto exit;
		}
	}

exit:
	return hr;
}

/** 
 * Read the contents of a "mapisvc.inf" file
 * 
 * @param filename The filename (including path) to load
 *
 * @retval MAPI_E_NOT_FOUND given filename does not exist on disk, or no access to this file (more likely)
 * @return MAPI Error code
 */
#define MAXLINELEN 4096
HRESULT INFLoader::LoadINF(const char *filename)
{
	HRESULT hr = hrSuccess;
	FILE *fp = NULL;
	char cBuffer[MAXLINELEN] = {0};
	inf::iterator iSection = m_mapSections.end();
	string strLine;
	string strName;
	string strValue;
	size_t pos;

	fp = fopen(filename, "r");
	if (fp == NULL) {
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}

	while (!feof(fp)) {
		memset(&cBuffer, 0, sizeof(cBuffer));

		if (!fgets(cBuffer, sizeof(cBuffer), fp))
			continue;

		strLine = trim(string(cBuffer), " \t");

		/* Skip empty lines any lines which start with # */
		if (strLine.empty() || strLine[0] == '#')
 			continue;

		/* Get setting name */
		pos = strLine.find('=');
		if (pos != string::npos) {
			strName = strLine.substr(0, pos);
			strValue = strLine.substr(pos + 1);
		} else {
			if (strLine[0] == '[') {
				pos = strLine.find(']');
				if (pos == string::npos)
					continue;	// skip line
				strName = strLine.substr(1, pos-1);

				pair<inf::iterator, bool> rv = m_mapSections.insert(make_pair(strName, inf_section()));
				iSection = rv.first;
			}
			// always continue with next line.
			continue;
		}

		if (iSection == m_mapSections.end())
			continue;

		// Parse strName in a property, else leave name?
		iSection->second.insert(make_pair(trim(strName, " \t\r\n"), trim(strValue, " \t\r\n")));
	}

exit:
	if (fp)
		fclose(fp);

	return hrSuccess;
}

/** 
 * Get the inf_section (provider info) for a given section name
 * 
 * @param strSectionName name of the section to find in the inf file
 * 
 * @return corresponding info, or empty inf_section;
 */
const inf_section* INFLoader::GetSection(const string& strSectionName) const
{
	inf::const_iterator iSection;

	iSection = m_mapSections.find(strSectionName);
	if (iSection == m_mapSections.end()) {
		static inf_section empty;
		return &empty;
	}
	return &iSection->second;
}

/** 
 * The filename of the config file to load.
 * 
 * @return path + filename of mapisvc.inf
 */
vector<string> INFLoader::GetINFPaths()
{
	vector<string> ret;
	char *env = getenv("MAPI_CONFIG_PATH");
	if (env)
		ba::split(ret, env, ba::is_any_of(":"), ba::token_compress_on);
	else
	// @todo, load both, or just one?
		ret.push_back(MAPICONFIGDIR);
	return ret;
}

/** 
 * Create a SPropValue from 2 strings found in the mapisvc.inf file.
 * 
 * @param[in] strTag The property tag
 * @param[in] strData The data for the property
 * @param[in] base MAPIAllocateMore base pointer
 * @param[in,out] lpProp Already allocated pointer
 * 
 * @return MAPI error code
 */
HRESULT INFLoader::MakeProperty(const std::string& strTag, const std::string& strData, void *base, LPSPropValue lpProp) const
{
	HRESULT hr = hrSuccess;
	SPropValue sProp;

	sProp.ulPropTag = DefinitionFromString(strTag, true);
	switch (PROP_TYPE(sProp.ulPropTag)) {
	case PT_LONG:
	{
		// either a definition, or a hexed network order value
		set<string> vValues;
		sProp.Value.ul = 0;
		ba::split(vValues, strData, ba::is_any_of("| \t"), ba::token_compress_on);
		for (set<string>::iterator i = vValues.begin(); i != vValues.end(); i++)
			sProp.Value.ul |= DefinitionFromString(*i, false);
		break;
	}
	case PT_UNICODE:
		sProp.ulPropTag = CHANGE_PROP_TYPE(sProp.ulPropTag, PT_STRING8);
	case PT_STRING8:
	{
		hr = MAPIAllocateMore((strData.length() +1) * sizeof(char), base, (void**)&sProp.Value.lpszA);
		if (hr != hrSuccess)
			goto exit;
		strcpy(sProp.Value.lpszA, strData.c_str());
		break;
	}
	case PT_BINARY:
	{
		hr = Util::hex2bin(strData.data(), strData.length(), &sProp.Value.bin.cb, &sProp.Value.bin.lpb, base);
		if (hr != hrSuccess)
			goto exit;
		break;
	}
	default:
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	*lpProp = sProp;

exit:
	return hr;
}

/** 
 * Convert a string as C-defined value to the defined value. This can
 * be properties, prop values, or hex values in network order.
 * 
 * @param strDef the string to convert
 * @param bProp strDef is a propvalue or not
 * 
 * @return 
 */
ULONG INFLoader::DefinitionFromString(const std::string& strDef, bool bProp) const
{
	std::map<std::string, unsigned int>::const_iterator i;
	char *end;
	unsigned int hex;

	i = m_mapDefs.find(strDef);
	if (i != m_mapDefs.end())
		return i->second;
	// parse strProp as hex
	hex = strtoul(strDef.c_str(), &end, 16);
	if (end < strDef.c_str()+strDef.length())
		return bProp ? PR_NULL : 0;
	return (ULONG)ntohl(hex);
}

SVCProvider::SVCProvider()
{
	m_cValues = 0;
	m_lpProps = NULL;
}

SVCProvider::~SVCProvider()
{
	if (m_lpProps)
		MAPIFreeBuffer(m_lpProps);
}

/** 
 * Return the properties of this provider section
 * 
 * @param[out] lpcValues number of properties in lppPropValues
 * @param[out] lppPropValues pointer to internal properties
 */
void SVCProvider::GetProps(ULONG *lpcValues, LPSPropValue *lppPropValues)
{
	*lpcValues = m_cValues;
	*lppPropValues = m_lpProps;
}

HRESULT SVCProvider::Init(const INFLoader& cINF, const inf_section* infProvider)
{
	HRESULT hr = hrSuccess;
	inf_section::const_iterator iSection;
	vector<string> prop;

	hr = MAPIAllocateBuffer(sizeof(SPropValue) * infProvider->size(), (void**)&m_lpProps);
	if (hr != hrSuccess)
		goto exit;

	for (m_cValues = 0, iSection = infProvider->begin(); iSection != infProvider->end(); iSection++) {
		// add properties to list
		if (cINF.MakeProperty(iSection->first, iSection->second, m_lpProps, &m_lpProps[m_cValues]) == hrSuccess)
			m_cValues++;
	}

exit:
	return hr;
}


SVCService::SVCService()
{
	m_dl = NULL;

	m_fnMSGServiceEntry = NULL;
	m_fnMSProviderInit = NULL;
	m_fnABProviderInit = NULL;

	m_cValues = 0;
	m_lpProps = NULL;
}

SVCService::~SVCService()
{
#ifndef VALGRIND
	if (m_dl)
		dlclose(m_dl);
#endif
	if (m_lpProps)
		MAPIFreeBuffer(m_lpProps);
	for (std::map<std::string, SVCProvider*>::iterator i = m_sProviders.begin(); i != m_sProviders.end(); i++)
		delete i->second;
}

/** 
 * Process a service section from the read inf file. Converts all
 * properties for the section, reads the associated shared library,
 * and find the entry point functions.
 * 
 * @param[in] cINF the INFLoader class which read the mapisvc.inf file
 * @param[in] infService the service section to initialize
 * 
 * @return MAPI Error code
 */
HRESULT SVCService::Init(const INFLoader& cINF, const inf_section* infService)
{
	HRESULT hr = hrSuccess;
	inf_section::const_iterator iSection;
	const inf_section* infProvider = NULL;
	vector<string> prop;
	LPSPropValue lpSO;
	void **cf;

	hr = MAPIAllocateBuffer(sizeof(SPropValue) * infService->size(), (void**)&m_lpProps);
	if (hr != hrSuccess)
		goto exit;

	for (m_cValues = 0, iSection = infService->begin(); iSection != infService->end(); iSection++) {
		// convert section to class
		if (iSection->first.compare("Providers") == 0) {
			// make new providers list
			// *new function, new loop
			ba::split(prop, iSection->second, ba::is_any_of(", \t"), ba::token_compress_on);

			for (vector<string>::iterator i = prop.begin(); i != prop.end(); i++) {
				infProvider = cINF.GetSection(*i);

				pair<std::map<std::string, SVCProvider*>::iterator, bool> prov = m_sProviders.insert(make_pair(*i, new SVCProvider()));
				if (prov.second == false)
					continue;	// already exists

				prov.first->second->Init(cINF, infProvider);
			}
		} else {
			// add properties to list
			if (cINF.MakeProperty(iSection->first, iSection->second, m_lpProps, &m_lpProps[m_cValues]) == hrSuccess)
				m_cValues++;
		}
	}

	// find PR_SERVICE_SO_NAME / PR_SERVICE_DLL_NAME, load library
	lpSO = PpropFindProp(m_lpProps, m_cValues, PR_SERVICE_SO_NAME_A);
	if (!lpSO)
		lpSO = PpropFindProp(m_lpProps, m_cValues, PR_SERVICE_DLL_NAME_A);
	if (!lpSO) {
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}

	m_dl = dlopen(lpSO->Value.lpszA, RTLD_NOW);
	if (!m_dl) {
		cerr << "Unable to load " << lpSO->Value.lpszA << ": " << dlerror() << endl;
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}

	// @todo use PR_SERVICE_ENTRY_NAME
	cf = (void**)&m_fnMSGServiceEntry;
	*cf = dlsym(m_dl, "MSGServiceEntry");
	if (!m_fnMSGServiceEntry) {
		// compulsary function in provider
		cerr << "Unable to find MSGServiceEntry in " << lpSO->Value.lpszA << ": " << dlerror() << endl;
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}

	cf = (void**)&m_fnMSProviderInit;
	*cf = dlsym(m_dl, "MSProviderInit");

	cf = (void**)&m_fnABProviderInit;
	*cf = dlsym(m_dl, "ABProviderInit");

exit:
	return hr;
}

/** 
 * Calls the CreateProvider on the given IProviderAdmin object to
 * create all providers of this service in your profile.
 * 
 * @param[in] lpProviderAdmin  IProviderAdmin object where all providers should be created
 * 
 * @return MAPI Error code
 */
HRESULT SVCService::CreateProviders(IProviderAdmin *lpProviderAdmin)
{
	HRESULT hr = hrSuccess;
	std::map<std::string, SVCProvider*>::iterator i;

	for (i = m_sProviders.begin(); i != m_sProviders.end(); i++) 
	{
		// CreateProvider will find the provider properties itself. the property parameters can be used for other properties.
		hr = lpProviderAdmin->CreateProvider((TCHAR*)i->first.c_str(), 0, NULL, 0, 0, NULL);
		if (hr != hrSuccess)
			goto exit;
	}

exit:
	return hr;
}

LPSPropValue SVCService::GetProp(ULONG ulPropTag)
{
	return PpropFindProp(m_lpProps, m_cValues, ulPropTag);
}

SVCProvider* SVCService::GetProvider(LPTSTR lpszProvider, ULONG ulFlags)
{
	std::map<std::string, SVCProvider*>::iterator i = m_sProviders.find((const char*)lpszProvider);
	if (i == m_sProviders.end())
		return NULL;
	return i->second;
}

vector<SVCProvider*> SVCService::GetProviders()
{
	vector<SVCProvider*> ret;

	for (std::map<std::string, SVCProvider*>::iterator i = m_sProviders.begin(); i != m_sProviders.end(); i++)
		ret.push_back(i->second);
	return ret;
}

SVC_MSGServiceEntry SVCService::MSGServiceEntry()
{
	return m_fnMSGServiceEntry;
}

SVC_MSProviderInit SVCService::MSProviderInit()
{
	return m_fnMSProviderInit;
}

SVC_ABProviderInit SVCService::ABProviderInit()
{
	return m_fnABProviderInit;
}

MAPISVC::MAPISVC()
{
}

MAPISVC::~MAPISVC()
{
	for (std::map<std::string, SVCService*>::iterator i = m_sServices.begin(); i != m_sServices.end(); i++)
		delete i->second;
}

HRESULT MAPISVC::Init()
{
	HRESULT hr = hrSuccess;
	INFLoader inf;
	const inf_section* infServices = NULL;
	inf_section::const_iterator iServices;
	const inf_section* infService = NULL;

	hr = inf.LoadINFs();
	if (hr != hrSuccess)
		goto exit;


	infServices = inf.GetSection("Services");

	for (iServices = infServices->begin(); iServices != infServices->end(); iServices++) {
		// ZARAFA6, ZCONTACTS
		infService = inf.GetSection(iServices->first);

		pair<std::map<std::string, SVCService*>::iterator, bool> i = m_sServices.insert(make_pair(iServices->first, new SVCService()));
		if (i.second == false)
			continue;			// already exists

		hr = i.first->second->Init(inf, infService);
		if (hr != hrSuccess) {
			// remove this service provider since it doesn't work
			delete i.first->second;
			m_sServices.erase(i.first);
			hr = hrSuccess;
		}
	}

exit:
	return hr;
}

/** 
 * Returns the service class of the requested service
 * 
 * @param[in] lpszService us-ascii service name
 * @param[in] ulFlags unused, could be used for MAPI_UNICODE in lpszService
 * @param lppService 
 * 
 * @return 
 */
HRESULT MAPISVC::GetService(LPTSTR lpszService, ULONG ulFlags, SVCService **lppService)
{
	std::map<std::string, SVCService*>::iterator i;
	
	i = m_sServices.find((char*)lpszService);
	if (i == m_sServices.end())
		return MAPI_E_NOT_FOUND;

	*lppService = i->second;

	return hrSuccess;
}

/** 
 * Finds the service object for a given dll name.
 * 
 * @param[in] lpszDLLName dll name of the service provider
 * @param[out] lppService the service object for the provider, or untouched on error
 * 
 * @return MAPI Error code
 * @retval MAPI_E_NOT_FOUND no service object for the given dll name
 */
HRESULT MAPISVC::GetService(char* lpszDLLName, SVCService **lppService)
{
	std::map<std::string, SVCService*>::iterator i;
	LPSPropValue lpDLLName;

	for (i = m_sServices.begin(); i != m_sServices.end(); i++) {
		lpDLLName = i->second->GetProp(PR_SERVICE_DLL_NAME_A);
		if (!lpDLLName || !lpDLLName->Value.lpszA)
			continue;
		if (strcmp(lpDLLName->Value.lpszA, lpszDLLName) == 0) {
			*lppService = i->second;
			return hrSuccess;
		}
	}

	return MAPI_E_NOT_FOUND;
}
