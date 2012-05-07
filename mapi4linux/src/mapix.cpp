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
#include "m4l.mapix.h"
#include "m4l.mapispi.h"
#include "m4l.debug.h"
#include "m4l.mapiutil.h"
#include "m4l.mapisvc.h"

#include <mapi.h>
#include <mapiutil.h>
#include <pthread.h>

#include "Util.h"

#include "ECConfig.h"
#include "ECDebug.h"
#include "ECGuid.h"
#include "ECMemTable.h"
#include "charset/utf16string.h"

#include "CommonUtil.h"
#include "stringutil.h"
#include "mapiguidext.h"
#include "ECRestriction.h"

#include <string>
#include <map>
#include <charset/convert.h>


/* Some required globals */
ECConfig *m4l_lpConfig = NULL;
ECLogger *m4l_lpLogger = NULL;
MAPISVC *m4l_lpMAPISVC = NULL;

/**
 * Internal MAPI4Linux function to create m4l internal ECConfig and ECLogger objects
 */
HRESULT HrCreateM4LServices()
{
	HRESULT hr = hrSuccess;
	std::basic_string<TCHAR> configfile;

	static const configsetting_t settings[] = {
		{ "ssl_port", "237" },
		{ "ssl_key_file", "c:\\program files\\zarafa\\exchange-redirector.pem" },
		{ "ssl_key_pass", "zarafa", CONFIGSETTING_EXACT },
		{ "server_address", "" },
		{ "log_method","file" },
		{ "log_file","-" },
		{ "log_level","2", CONFIGSETTING_RELOADABLE },
		{ "log_timestamp","1" },
		{ NULL, NULL },
	};

	/* Go for default location of zarafa configuration */
	configfile = _T("/etc/zarafa/");

	configfile += PATH_SEPARATOR;
	configfile += _T("exchange-redirector.cfg");

	if (!m4l_lpConfig) {
		m4l_lpConfig = ECConfig::Create(settings);
		if (!m4l_lpConfig) {
			hr = MAPI_E_NOT_ENOUGH_MEMORY;
			goto exit;
		}
		m4l_lpConfig->LoadSettings(configfile.c_str());
	}

	if (!m4l_lpLogger) {
		m4l_lpLogger = CreateLogger(m4l_lpConfig, "exchange-redirector", "ExchangeRedirector");
		if (!m4l_lpLogger) {
			hr = MAPI_E_NOT_ENOUGH_MEMORY;
			goto exit;
		}
	}

exit:
	return hr;
}

/**
 * Internal MAPI4Linux function to clean m4l internal ECConfig and ECLogger objects
 */
void HrFreeM4LServices()
{
	if (m4l_lpLogger) {
		m4l_lpLogger->Release();
		m4l_lpLogger = NULL;
	}

	if (m4l_lpConfig) {
		delete m4l_lpConfig;
		m4l_lpConfig = NULL;
	}
}

// ---
// M4LProfAdmin
// ---

M4LProfAdmin::M4LProfAdmin() {
    pthread_mutexattr_t attr;
    
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&m_mutexProfiles, &attr);
    
    pthread_mutexattr_destroy(&attr);
}

M4LProfAdmin::~M4LProfAdmin() {
    list<profEntry*>::iterator i;
    
	pthread_mutex_lock(&m_mutexProfiles);

    for(i = profiles.begin(); i != profiles.end(); i++) {
		(*i)->serviceadmin->Release();
		delete *i;
    }
    profiles.clear();
    
	pthread_mutex_unlock(&m_mutexProfiles);

    pthread_mutex_destroy(&m_mutexProfiles);
}

list<profEntry*>::iterator M4LProfAdmin::findProfile(LPTSTR lpszProfileName) {
    list<profEntry*>::iterator i;

    for(i = profiles.begin(); i != profiles.end(); i++) {
		if ((*i)->profname == (char*)lpszProfileName)
			break;
    }
    return i;
}

HRESULT M4LProfAdmin::GetLastError(HRESULT hResult, ULONG ulFlags, LPMAPIERROR* lppMAPIError) {
	TRACE_MAPILIB(TRACE_ENTRY, "M4LProfAdmin::GetLastError", "");
	*lppMAPIError = NULL;
	TRACE_MAPILIB1(TRACE_RETURN, "M4LProfAdmin::GetLastError", "0x%08x", 0);
    return hrSuccess;
}

/**
 * Returns IMAPITable object with all profiles available. Only has 2
 * properties per row: PR_DEFAULT_PROFILE (always false in Linux) and
 * PR_DISPLAY_NAME. This table does not have notifications, so changes
 * will not be present in the table.
 *
 * @param[in]	ulFlags		Unused.
 * @param[out]	lppTable	Pointer to IMAPITable object.
 * @return		HRESULT
 */
HRESULT M4LProfAdmin::GetProfileTable(ULONG ulFlags, LPMAPITABLE* lppTable) {
	TRACE_MAPILIB(TRACE_ENTRY, "M4LProfAdmin::GetProfileTable", "");
	HRESULT hr = hrSuccess;
	list<profEntry*>::iterator i;
	ECMemTable *lpTable = NULL;
	ECMemTableView *lpTableView = NULL;
	SPropValue sProps[3];
	int n = 0;
	std::wstring wDisplayName;

	SizedSPropTagArray(2, sptaProfileCols) = {2, {PR_DEFAULT_PROFILE, PR_DISPLAY_NAME}};

	pthread_mutex_lock(&m_mutexProfiles);

	if (ulFlags & MAPI_UNICODE)
		sptaProfileCols.aulPropTag[1] = CHANGE_PROP_TYPE(PR_DISPLAY_NAME_W, PT_UNICODE);
	else
		sptaProfileCols.aulPropTag[1] = CHANGE_PROP_TYPE(PR_DISPLAY_NAME_A, PT_STRING8);
		
	hr = ECMemTable::Create((LPSPropTagArray)&sptaProfileCols, PR_ROWID, &lpTable);
	if(hr != hrSuccess)
		goto exit;

	for(i = profiles.begin(); i != profiles.end(); i++)
	{
		sProps[0].ulPropTag = PR_DEFAULT_PROFILE;
		sProps[0].Value.b = false; //FIXME: support setDefaultProfile

		if (ulFlags & MAPI_UNICODE) {
			wDisplayName = convert_to<wstring>((*i)->profname);
			sProps[1].ulPropTag = PR_DISPLAY_NAME_W;
			sProps[1].Value.lpszW = (WCHAR *) wDisplayName.c_str();
		} else {
			sProps[1].ulPropTag = PR_DISPLAY_NAME_A;
			sProps[1].Value.lpszA = (char *) (*i)->profname.c_str();
		}
		
		sProps[2].ulPropTag = PR_ROWID;
		sProps[2].Value.ul = n++;

		//TODO: PR_INSTANCE_KEY

		hr = lpTable->HrModifyRow(ECKeyTable::TABLE_ROW_ADD, NULL, sProps, 3);
		if (hr != hrSuccess)
			goto exit;

	}

	hr = lpTable->HrGetView(createLocaleFromName(""), ulFlags, &lpTableView);
	if(hr != hrSuccess)
		goto exit;
		
	hr = lpTableView->QueryInterface(IID_IMAPITable, (void **)lppTable);

exit:
	pthread_mutex_unlock(&m_mutexProfiles);

	if (lpTableView)
		lpTableView->Release();

	if (lpTable)
		lpTable->Release();

	TRACE_MAPILIB1(TRACE_RETURN, "M4LProfAdmin::GetProfileTable", "0x%08x", hr);
    return hr;
}

/**
 * Create new profile with unique name.
 *
 * @param[in]	lpszProfileName	Name of the profile to create, us-ascii charset. Actual type always char*.
 * @param[in]	lpszPassword	Password of the profile, us-ascii charset. Actual type always char*.
 * @param[in]	ulUIParam		Unused in Linux.
 * @param[in]	ulFlags			Unused in Linux.
 * @return		HRESULT
 * @retval		MAPI_E_NO_ACCESS	Profilename already exists.
 * @retval		MAPI_E_NOT_ENOUGH_MEMORY	Out of memory.
 */
HRESULT M4LProfAdmin::CreateProfile(LPTSTR lpszProfileName, LPTSTR lpszPassword, ULONG ulUIParam, ULONG ulFlags) {
	TRACE_MAPILIB1(TRACE_ENTRY, "M4LProfAdmin::CreateProfile", "profilename=%s", (char*)lpszProfileName);
    HRESULT hr = hrSuccess;
    list<profEntry*>::iterator i;
    profEntry* entry = NULL;
	M4LProfSect *profilesection = NULL;
	SPropValue sPropValue;

    pthread_mutex_lock(&m_mutexProfiles);
    
    if(lpszProfileName == NULL) {
    	hr = MAPI_E_INVALID_PARAMETER;
    	goto exit;
	}
    
    i = findProfile(lpszProfileName);
    if (i != profiles.end()) {
		hr = MAPI_E_NO_ACCESS;	// duplicate profile name
		goto exit;
    }

    entry = new profEntry;
    if (!entry) {
		hr = MAPI_E_NOT_ENOUGH_MEMORY;
		goto exit;
    }
    // This is the so-called global profile section.
	profilesection = new M4LProfSect(TRUE);
	profilesection->AddRef();

	// Set the default profilename
	sPropValue.ulPropTag = PR_PROFILE_NAME_A;
	sPropValue.Value.lpszA = (char*)lpszProfileName;
	hr = profilesection->SetProps(1 ,&sPropValue, NULL);
	if (hr != hrSuccess)
		goto exit;

    entry->serviceadmin = new M4LMsgServiceAdmin(profilesection);
    if (!entry->serviceadmin) {
		delete entry;
		hr = MAPI_E_NOT_ENOUGH_MEMORY;
		goto exit;
    }
    entry->serviceadmin->AddRef();

    // enter data
    entry->profname = (char*)lpszProfileName;
    if (lpszPassword)
		entry->password = (char*)lpszPassword;

    profiles.push_back(entry);
    
exit:
    pthread_mutex_unlock(&m_mutexProfiles);

	if (profilesection)
		profilesection->Release();
    
	TRACE_MAPILIB1(TRACE_RETURN, "M4LProfAdmin::CreateProfile", "0x%08x", hr);
    return hr;
}

/**
 * Delete profile from list.
 *
 * @param[in]	lpszProfileName	Name of the profile to delete, us-ascii charset.
 * @param[in]	ulFlags			Unused in Linux.
 * @return		HRESULT
 * @retval		MAPI_E_NOT_FOUND	Profilename does not exist.
 */
HRESULT M4LProfAdmin::DeleteProfile(LPTSTR lpszProfileName, ULONG ulFlags) {
	TRACE_MAPILIB(TRACE_ENTRY, "M4LProfAdmin::DeleteProfile", "");
    HRESULT hr = hrSuccess;
    list<profEntry*>::iterator i;

    pthread_mutex_lock(&m_mutexProfiles);
    
    i = findProfile(lpszProfileName);
    if (i != profiles.end()) {
		(*i)->serviceadmin->Release();
		delete *i;
		profiles.erase(i);
    } else {
        hr = MAPI_E_NOT_FOUND;
    }

    pthread_mutex_unlock(&m_mutexProfiles);
    
	TRACE_MAPILIB1(TRACE_RETURN, "M4LProfAdmin::DeleteProfile", "0x%08x", hr);
    return hr;
}

HRESULT M4LProfAdmin::ChangeProfilePassword(LPTSTR lpszProfileName, LPTSTR lpszOldPassword, LPTSTR lpszNewPassword, ULONG ulFlags) {
	TRACE_MAPILIB(TRACE_ENTRY, "M4LProfAdmin::ChangeProfilePassword", "");
	TRACE_MAPILIB1(TRACE_RETURN, "M4LProfAdmin::ChangeProfilePassword", "0x%08x", MAPI_E_NO_SUPPORT);
    return MAPI_E_NO_SUPPORT;
}

HRESULT M4LProfAdmin::CopyProfile(LPTSTR lpszOldProfileName, LPTSTR lpszOldPassword, LPTSTR lpszNewProfileName, ULONG ulUIParam,
								  ULONG ulFlags) {
	TRACE_MAPILIB(TRACE_ENTRY, "M4LProfAdmin::CopyProfile", "");
	TRACE_MAPILIB1(TRACE_RETURN, "M4LProfAdmin::CopyProfile", "0x%08x", MAPI_E_NO_SUPPORT);
    return MAPI_E_NO_SUPPORT;
}

HRESULT M4LProfAdmin::RenameProfile(LPTSTR lpszOldProfileName, LPTSTR lpszOldPassword, LPTSTR lpszNewProfileName, ULONG ulUIParam,
									ULONG ulFlags) {
	TRACE_MAPILIB(TRACE_ENTRY, "M4LProfAdmin::RenameProfile", "");
	TRACE_MAPILIB1(TRACE_RETURN, "M4LProfAdmin::RenameProfile", "0x%08x", MAPI_E_NO_SUPPORT);
    return MAPI_E_NO_SUPPORT;
}

HRESULT M4LProfAdmin::SetDefaultProfile(LPTSTR lpszProfileName, ULONG ulFlags) {
	TRACE_MAPILIB(TRACE_ENTRY, "M4LProfAdmin::SetDefaultProfile", "");
	TRACE_MAPILIB1(TRACE_RETURN, "M4LProfAdmin::SetDefaultProfile", "0x%08x", MAPI_E_NO_SUPPORT);
    return MAPI_E_NO_SUPPORT;
}

/**
 * Request IServiceAdmin object of profile. Linux does not check the password.
 *
 * @param[in]	lpszProfileName	Name of the profile to open, us-ascii charset.
 * @param[in]	lpszPassword	Password of the profile, us-ascii charset. Not used in Linux.
 * @param[in]	ulUIParam		Unused in Linux.
 * @param[in]	ulFlags			Unused in Linux.
 * @param[out]	lppServiceAdmin	IServiceAdmin object
 * @return		HRESULT
 * @retval		MAPI_E_NOT_FOUND	Profile is not found.
 */
HRESULT M4LProfAdmin::AdminServices(LPTSTR lpszProfileName, LPTSTR lpszPassword, ULONG ulUIParam, ULONG ulFlags,
									LPSERVICEADMIN* lppServiceAdmin) {
	TRACE_MAPILIB2(TRACE_ENTRY, "M4LProfAdmin::AdminServices", "name=%s - password=%s", (char*)lpszProfileName, (lpszPassword)?(char*)lpszPassword:"NULL");
    HRESULT hr = hrSuccess;									
    list<profEntry*>::iterator i;
    
    pthread_mutex_lock(&m_mutexProfiles);

    if(lpszProfileName == NULL) {
    	hr = MAPI_E_INVALID_PARAMETER;
    	goto exit;
	}

    i = findProfile(lpszProfileName);
    if (i == profiles.end()) {
        hr = MAPI_E_NOT_FOUND;
        goto exit;
    }
    
	hr = (*i)->serviceadmin->QueryInterface(IID_IMsgServiceAdmin,(void**)lppServiceAdmin);

exit:
    pthread_mutex_unlock(&m_mutexProfiles);
	TRACE_MAPILIB1(TRACE_RETURN, "M4LProfAdmin::AdminServices", "0x%08x", hr);
    return hr;
}

// iunknown passthru
ULONG M4LProfAdmin::AddRef() {
	TRACE_MAPILIB(TRACE_ENTRY, "M4LProfAdmin::AddRef", "");
    ULONG ulRef = M4LUnknown::AddRef();
	TRACE_MAPILIB1(TRACE_ENTRY, "M4LProfAdmin::AddRef", "%d", ulRef);
	return ulRef;
}
ULONG M4LProfAdmin::Release() {
	TRACE_MAPILIB(TRACE_ENTRY, "M4LProfAdmin::Release", "");
    ULONG ulRef = M4LUnknown::Release();
	TRACE_MAPILIB1(TRACE_ENTRY, "M4LProfAdmin::Release", "%d", ulRef);
	return ulRef;
}
HRESULT M4LProfAdmin::QueryInterface(REFIID refiid, void **lpvoid) {
	TRACE_MAPILIB(TRACE_ENTRY, "M4LProfAdmin::QueryInterface", "");
	HRESULT hr = hrSuccess;

    if ((refiid == IID_IProfAdmin) || (refiid == IID_IUnknown)) {
		AddRef();
		*lpvoid = (IProfAdmin *)this;
    } else
		hr = MAPI_E_INTERFACE_NOT_SUPPORTED;

	TRACE_MAPILIB1(TRACE_RETURN, "M4LProfAdmin::QueryInterface", "0x%08x", hr);
    return hr;
}


// ---
// IMsgServceAdmin
// ---
M4LMsgServiceAdmin::M4LMsgServiceAdmin(M4LProfSect *profilesection) {
	this->profilesection = profilesection;

	profilesection->AddRef();

	pthread_mutexattr_t attr;

	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&m_mutexserviceadmin, &attr);

	pthread_mutexattr_destroy(&attr);
}

M4LMsgServiceAdmin::~M4LMsgServiceAdmin() {
    list<serviceEntry*>::iterator s;
    list<providerEntry*>::iterator p;
    
	pthread_mutex_lock(&m_mutexserviceadmin);

    for (s = services.begin(); s != services.end(); s++) {
		(*s)->provideradmin->Release();
		delete *s;
    }
    for (p = providers.begin(); p != providers.end(); p++) {
		(*p)->profilesection->Release();
		delete *p;
    }
    
    services.clear();
    providers.clear();

	profilesection->Release();

	pthread_mutex_unlock(&m_mutexserviceadmin);

	pthread_mutex_destroy(&m_mutexserviceadmin);

}
    
serviceEntry* M4LMsgServiceAdmin::findServiceAdmin(LPTSTR lpszServiceName) {
    list<serviceEntry*>::iterator i;
    for(i = services.begin(); i != services.end(); i++) {
		if ((*i)->servicename == (char*)lpszServiceName)
			return *i;
    }
    return NULL;
}

serviceEntry* M4LMsgServiceAdmin::findServiceAdmin(LPMAPIUID lpMUID) {
    list<serviceEntry*>::iterator i;
    for (i = services.begin(); i != services.end(); i++) {
		if (memcmp(&(*i)->muid, lpMUID, sizeof(MAPIUID)) == 0)
			return *i;
    }
    return NULL;
}

providerEntry* M4LMsgServiceAdmin::findProvider(LPMAPIUID lpUid) {
	list<providerEntry *>::iterator i;
	
	for(i=providers.begin();i!=providers.end();i++) {
		if(memcmp(&(*i)->uid,lpUid,sizeof(MAPIUID)) == 0) {
			return *i;
		}
	}
	return NULL;
}


HRESULT M4LMsgServiceAdmin::GetLastError(HRESULT hResult, ULONG ulFlags, LPMAPIERROR* lppMAPIError) {
	TRACE_MAPILIB(TRACE_ENTRY, "M4LMsgServiceAdmin::GetLastError", "");
    *lppMAPIError = NULL;
	TRACE_MAPILIB1(TRACE_RETURN, "M4LMsgServiceAdmin::GetLastError", "0x%08x", 0);
    return hrSuccess;
}

/**
 * Get all services in this profile in a IMAPITable object. This table
 * doesn't have updates through notifications. This table only has 3
 * properties: PR_SERVICE_UID, PR_SERVICE_NAME, PR_DISPLAY_NAME.
 *
 * @param[in]	ulFlags			Unused in Linux.
 * @param[out]	lppTable		IMAPITable return object
 * @return		HRESULT
 */
HRESULT M4LMsgServiceAdmin::GetMsgServiceTable(ULONG ulFlags, LPMAPITABLE* lppTable) {
	TRACE_MAPILIB(TRACE_ENTRY, "M4LMsgServiceAdmin::GetMsgServiceTable", "");
	HRESULT hr = hrSuccess;
	list<serviceEntry *>::iterator i;
	ECMemTable *lpTable = NULL;
	ECMemTableView *lpTableView = NULL;
	SPropValue sProps[4];
	int n = 0;
	std::wstring wServiceName, wDisplayName;
	convert_context converter;

	SizedSPropTagArray(3, sptaProviderColsUnicode) = {3, {PR_SERVICE_UID, PR_SERVICE_NAME_W, PR_DISPLAY_NAME_W} };
	SizedSPropTagArray(3, sptaProviderColsAscii) = {3, {PR_SERVICE_UID, PR_SERVICE_NAME_A, PR_DISPLAY_NAME_A} };
	
	pthread_mutex_lock(&m_mutexserviceadmin);

	if (ulFlags & MAPI_UNICODE)
		hr = ECMemTable::Create((LPSPropTagArray)&sptaProviderColsUnicode, PR_ROWID, &lpTable);
	else
		hr = ECMemTable::Create((LPSPropTagArray)&sptaProviderColsAscii, PR_ROWID, &lpTable);
	if(hr != hrSuccess)
		goto exit;
	
	// Loop through all providers, add each to the table
	for (i = services.begin(); i != services.end(); i++) {
		sProps[0].ulPropTag = PR_SERVICE_UID;
		sProps[0].Value.bin.lpb = (BYTE *) &(*i)->muid;
		sProps[0].Value.bin.cb = sizeof(GUID);

		if (ulFlags & MAPI_UNICODE) {
			wServiceName = converter.convert_to<wstring>((*i)->servicename);
			sProps[1].ulPropTag = PR_SERVICE_NAME_W;
			sProps[1].Value.lpszW = (WCHAR *) wServiceName.c_str();
		} else {
			sProps[1].ulPropTag = PR_SERVICE_NAME_A;
			sProps[1].Value.lpszA = (char *) (*i)->servicename.c_str();
		}			
		
		if (ulFlags & MAPI_UNICODE) {
			wDisplayName = converter.convert_to<wstring>((*i)->displayname);
			sProps[1].ulPropTag = PR_DISPLAY_NAME_W;
			sProps[1].Value.lpszW = (WCHAR *) wDisplayName.c_str();
		} else {
			sProps[2].ulPropTag = PR_DISPLAY_NAME_A;
			sProps[2].Value.lpszA = (char *) (*i)->displayname.c_str();
		}
		
		sProps[3].ulPropTag = PR_ROWID;
		sProps[3].Value.ul = n++;
		
		lpTable->HrModifyRow(ECKeyTable::TABLE_ROW_ADD, NULL, sProps, 4);
	}
	
	hr = lpTable->HrGetView(createLocaleFromName(""), ulFlags, &lpTableView);
	if(hr != hrSuccess)
		goto exit;
		
	hr = lpTableView->QueryInterface(IID_IMAPITable, (void **)lppTable);
	
exit:
	pthread_mutex_unlock(&m_mutexserviceadmin);

	if (lpTableView)
		lpTableView->Release();

	if (lpTable)
		lpTable->Release();

	TRACE_MAPILIB1(TRACE_RETURN, "M4LMsgServiceAdmin::GetMsgServiceTable", "0x%08x", hr);
	return hr;
}

/**
 * Create new message service in this profile.
 *
 * @param[in]	lpszService		Name of the new service to add. In Linux, this is only the ZARAFA6 (zarafaclient.so) service.
 * @param[in]	lpszDisplayName	Unused in Linux.
 * @param[in]	ulUIParam		Unused in Linux.
 * @param[in]	ulFlags			Unused in Linux.
 * @return		HRESULT
 * @retval		MAPI_E_NOT_FOUND	Service not available.
 * @retval		MAPI_E_NO_ACCESS	Service already in profile.
 * @retval		MAPI_E_NOT_ENOUGH_MEMORY	Out of memory.
 */
HRESULT M4LMsgServiceAdmin::CreateMsgService(LPTSTR lpszService, LPTSTR lpszDisplayName, ULONG ulUIParam, ULONG ulFlags) {
	TRACE_MAPILIB(TRACE_ENTRY, "M4LMsgServiceAdmin::CreateMsgService", "");
	HRESULT hr = hrSuccess;
	serviceEntry* entry = NULL;
	SVCService* service = NULL;
	LPSPropValue lpProp = NULL;

	pthread_mutex_lock(&m_mutexserviceadmin);
	
	if(lpszService == NULL || lpszDisplayName == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	hr = m4l_lpMAPISVC->GetService(lpszService, ulFlags, &service);
	if (hr != hrSuccess)
		goto exit;

	// Create a Zarafa message service
    entry = findServiceAdmin(lpszService);
	if (entry) {
		hr = MAPI_E_NO_ACCESS; // already exists
		goto exit;
	}

    entry = new serviceEntry;
	if (!entry) {
		hr = MAPI_E_NOT_ENOUGH_MEMORY;
		goto exit;
	}

	entry->provideradmin = new M4LProviderAdmin(this, (char*)lpszService);
	if (!entry->provideradmin) {
		delete entry;
		hr = MAPI_E_NOT_ENOUGH_MEMORY;
		goto exit;
	}
    entry->provideradmin->AddRef();

	entry->servicename = (char*)lpszService;
	lpProp = service->GetProp(PR_DISPLAY_NAME_A);
	entry->displayname = lpProp ? lpProp->Value.lpszA : (char*)lpszService;
	
    CoCreateGuid((LPGUID)&entry->muid);

	entry->service = service;
    
	services.push_back(entry);

	// calls entry->provideradmin->CreateProvider for each provider read from mapisvc.inf
	hr = service->CreateProviders(entry->provideradmin);

	entry->bInitialize = false;

exit:
	pthread_mutex_unlock(&m_mutexserviceadmin);

	TRACE_MAPILIB1(TRACE_RETURN, "M4LMsgServiceAdmin::CreateMsgService", "0x%08x", hr);
    return hr;
}

/**
 * Delete message service from this profile.
 *
 * @param[in]	lpUID		MAPIUID (guid) of the service to remove
 * @return		HRESULT
 * @retval		MAPI_E_NOT_FOUND	Service not in profile.
 */
HRESULT M4LMsgServiceAdmin::DeleteMsgService(LPMAPIUID lpUID) {
	TRACE_MAPILIB(TRACE_ENTRY, "M4LMsgServiceAdmin::DeleteMsgService", "");
	HRESULT hr = hrSuccess;
	string name;
	
    list<serviceEntry*>::iterator i;
    list<providerEntry*>::iterator p;
    list<providerEntry*>::iterator pNext;
    
	pthread_mutex_lock(&m_mutexserviceadmin);

    for (i = services.begin(); i != services.end(); i++) {
		if (memcmp(&(*i)->muid, lpUID, sizeof(MAPIUID)) == 0) {
			name = (*i)->servicename;
			(*i)->provideradmin->Release();
			delete *i;
			services.erase(i);
			break;
		}
    }
    
    if(name.empty()) {
    	hr = MAPI_E_NOT_FOUND;
		goto exit;
	}
    
    p = providers.begin();
    while (p != providers.end()) {
		if ((*p)->servicename == name) {
			pNext = p;
			pNext++;
			(*p)->profilesection->Release();
			delete *p;
			providers.erase(p);
			p = pNext;
		}
    }

exit:
	pthread_mutex_unlock(&m_mutexserviceadmin);

	TRACE_MAPILIB1(TRACE_RETURN, "M4LMsgServiceAdmin::DeleteMsgService", "0x%08x", hr);
    return hr;
}

HRESULT M4LMsgServiceAdmin::CopyMsgService(LPMAPIUID lpUID, LPTSTR lpszDisplayName, LPCIID lpInterfaceToCopy, LPCIID lpInterfaceDst,
										   LPVOID lpObjectDst, ULONG ulUIParam, ULONG ulFlags) {
	TRACE_MAPILIB(TRACE_ENTRY, "M4LMsgServiceAdmin::CopyMsgService", "");
	TRACE_MAPILIB1(TRACE_RETURN, "M4LMsgServiceAdmin::CopyMsgService", "0x%08x", MAPI_E_NO_SUPPORT);
    return MAPI_E_NO_SUPPORT;
}

HRESULT M4LMsgServiceAdmin::RenameMsgService(LPMAPIUID lpUID, ULONG ulFlags, LPTSTR lpszDisplayName) {
	TRACE_MAPILIB(TRACE_ENTRY, "M4LMsgServiceAdmin::RenameMsgService", "");
	TRACE_MAPILIB1(TRACE_RETURN, "M4LMsgServiceAdmin::RenameMsgService", "0x%08x", MAPI_E_NO_SUPPORT);
    return MAPI_E_NO_SUPPORT;
}

/**
 * Calls MSGServiceEntry of the given service in lpUID.
 *
 * @param[in]	lpUID		MAPIUID (guid) of the service to call MSGServiceEntry on.
 * @param[in]	ulUIParam	Passed to MSGServiceEntry
 * @param[in]	ulFlags		Passed to MSGServiceEntry. If MAPI_UNICODE is passed, lpProps should contain PT_UNICODE strings.
 * @param[in]	cValues		Passed to MSGServiceEntry
 * @param[in]	lpProps		Passed to MSGServiceEntry
 * @return		HRESULT
 * @retval		MAPI_E_NOT_FOUND	Service not available.
 */
HRESULT M4LMsgServiceAdmin::ConfigureMsgService(LPMAPIUID lpUID, ULONG ulUIParam, ULONG ulFlags, ULONG cValues, LPSPropValue lpProps) {
	TRACE_MAPILIB1(TRACE_ENTRY, "M4LMsgServiceAdmin::ConfigureMsgService", "%s", lpProps ? PropNameFromPropArray(cValues, lpProps).c_str() : "<null>");
	HRESULT hr = hrSuccess;
	M4LProviderAdmin *lpProviderAdmin = NULL;
    serviceEntry* entry;

	pthread_mutex_lock(&m_mutexserviceadmin);
	
	if (lpUID == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	// Create a new provideradmin, will NULL servicename.. ie it is a provider admin for *all* providers in the msgservice
	lpProviderAdmin = new M4LProviderAdmin(this, NULL);
	lpProviderAdmin->AddRef();

	entry = findServiceAdmin(lpUID);
	if (!entry) {
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}


	// call zarafa client Message Service Entry (provider/client/EntryPoint.cpp)
	hr = entry->service->MSGServiceEntry()(0, NULL, NULL, ulUIParam, ulFlags, MSG_SERVICE_CONFIGURE, cValues, lpProps, (LPPROVIDERADMIN)entry->provideradmin, NULL);
	if(hr != hrSuccess)
		goto exit;
	
	entry->bInitialize = true;

exit:
	pthread_mutex_unlock(&m_mutexserviceadmin);
	if(lpProviderAdmin)
		lpProviderAdmin->Release();

	TRACE_MAPILIB1(TRACE_RETURN, "M4LMsgServiceAdmin::ConfigureMsgService", "0x%08x", hr);
	return hr;
}

/**
 * Get the IProfSect object for a service in the profile.
 *
 * @param[in]	lpUID		MAPIUID (guid) of the service.
 * @param[in]	lpInterface	IID request a specific interface on the profilesection. If NULL, IID_IProfSect is used.
 * @param[in]	ulFlags		Unused in Linux.
 * @param[out]	lppProfSect	IProfSect object of the service.
 * @return		HRESULT
 * @retval		MAPI_E_NOT_FOUND	Service not in profile.
 */
HRESULT M4LMsgServiceAdmin::OpenProfileSection(LPMAPIUID lpUID, LPCIID lpInterface, ULONG ulFlags, LPPROFSECT* lppProfSect) {
	TRACE_MAPILIB1(TRACE_ENTRY, "M4LMsgServiceAdmin::OpenProfileSection", "%s", bin2hex(sizeof(GUID), (BYTE *)lpUID).c_str());
	HRESULT hr = hrSuccess;
	LPSPropValue lpsPropVal = NULL;
	IMAPIProp *lpMapiProp = NULL;
	providerEntry* entry;
	
	pthread_mutex_lock(&m_mutexserviceadmin);

	if(lpUID && memcmp(lpUID, pbGlobalProfileSectionGuid, sizeof(MAPIUID)) == 0) {
		hr = this->profilesection->QueryInterface( (lpInterface)?*lpInterface:IID_IProfSect, (void**)lppProfSect);
		goto exit;
	} else if (lpUID && memcmp(lpUID, &MUID_PROFILE_INSTANCE, sizeof(MAPIUID)) == 0) {
		// hack to support MUID_PROFILE_INSTANCE
		*lppProfSect = new M4LProfSect();
		(*lppProfSect)->AddRef();

		// @todo add PR_SEARCH_KEY should be a profile unique GUID

		// Set the default profilename
		hr = this->profilesection->QueryInterface(IID_IMAPIProp, (void**)&lpMapiProp); 
		if (hr != hrSuccess)
			goto exit;

		hr = HrGetOneProp(lpMapiProp, PR_PROFILE_NAME_A, &lpsPropVal);
		if (hr != hrSuccess)
			goto exit;

		hr = (*lppProfSect)->SetProps(1 , lpsPropVal, NULL);
		if (hr != hrSuccess)
			goto exit;	
		
		goto exit;
	} else {
    	if(lpUID)
    	    entry = findProvider(lpUID);
    	else {
    		// Profile section NULL, create a temporary profile section that will be discarded
    		*lppProfSect = new M4LProfSect();
    		(*lppProfSect)->AddRef();
    		goto exit;
		}
	}

	if (!entry) {
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}

	hr = entry->profilesection->QueryInterface( (lpInterface)?*lpInterface:IID_IProfSect, (void**)lppProfSect);

exit:
	pthread_mutex_unlock(&m_mutexserviceadmin);

	if (lpsPropVal)
		MAPIFreeBuffer(lpsPropVal);

	if (lpMapiProp)
		lpMapiProp->Release();

	TRACE_MAPILIB1(TRACE_RETURN, "M4LMsgServiceAdmin::OpenProfileSection", "0x%08x", hr);
    return hr;
}

HRESULT M4LMsgServiceAdmin::MsgServiceTransportOrder(ULONG cUID, LPMAPIUID lpUIDList, ULONG ulFlags) {
	TRACE_MAPILIB(TRACE_ENTRY, "M4LMsgServiceAdmin::MsgServiceTransportOrder", "");
	TRACE_MAPILIB1(TRACE_RETURN, "M4LMsgServiceAdmin::MsgServiceTransportOrder", "0x%08x", MAPI_E_NO_SUPPORT);
    return MAPI_E_NO_SUPPORT;
}

/**
 * Get the IProviderAdmin object for a service in the profile.
 *
 * @param[in]	lpUID		MAPIUID (guid) of the service.
 * @param[in]	ulFlags		Unused in Linux.
 * @param[out]	lppProviderAdmin	IProviderAdmin object of the service.
 * @return		HRESULT
 * @retval		MAPI_E_NOT_FOUND	Service not in profile.
 */
HRESULT M4LMsgServiceAdmin::AdminProviders(LPMAPIUID lpUID, ULONG ulFlags, LPPROVIDERADMIN* lppProviderAdmin) {
	TRACE_MAPILIB(TRACE_ENTRY, "M4LMsgServiceAdmin::AdminProviders", "");
	HRESULT hr = hrSuccess;
	serviceEntry* entry = NULL;

	pthread_mutex_lock(&m_mutexserviceadmin);

	entry = findServiceAdmin(lpUID);
	if (!entry) {
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}

	hr = entry->provideradmin->QueryInterface(IID_IProviderAdmin, (void**)lppProviderAdmin);

exit:
	pthread_mutex_unlock(&m_mutexserviceadmin);

	TRACE_MAPILIB1(TRACE_RETURN, "M4LMsgServiceAdmin::AdminProviders", "0x%08x", hr);
    return hr;
}

HRESULT M4LMsgServiceAdmin::SetPrimaryIdentity(LPMAPIUID lpUID, ULONG ulFlags) {
	TRACE_MAPILIB(TRACE_ENTRY, "M4LMsgServiceAdmin::SetPrimaryIdentity", "");
	TRACE_MAPILIB1(TRACE_RETURN, "M4LMsgServiceAdmin::SetPrimaryIdentity", "0x%08x", MAPI_E_NO_SUPPORT);
    return MAPI_E_NO_SUPPORT;
}

/**
 * Get a list of all providers in the profile in a IMAPITable
 * object. No notifications for changes are sent.
 *
 * @param[in]	ulFlags		Unused in Linux.
 * @param[out]	lppTable	IMAPITable object
 * @return		HRESULT
 */
HRESULT M4LMsgServiceAdmin::GetProviderTable(ULONG ulFlags, LPMAPITABLE* lppTable) {
	TRACE_MAPILIB(TRACE_ENTRY, "M4LMsgServiceAdmin::GetProviderTable", "");
	HRESULT hr = hrSuccess;
	ULONG cValues = 0;
	LPSPropValue lpsProps = NULL;
	list<providerEntry *>::iterator i;
	list<serviceEntry *>::iterator j;
	ECMemTable *lpTable = NULL;
	ECMemTableView *lpTableView = NULL;
	LPSPropValue lpDest = NULL;
	ULONG cValuesDest = 0;
	SPropValue sPropID;
	int n = 0;
	LPSPropTagArray lpPropTagArray = NULL;
	SizedSPropTagArray(11, sptaProviderCols) = {11, {PR_MDB_PROVIDER, PR_AB_PROVIDER_ID, PR_INSTANCE_KEY, PR_RECORD_KEY, PR_ENTRYID,
												   PR_DISPLAY_NAME_A, PR_OBJECT_TYPE, PR_PROVIDER_UID, PR_RESOURCE_TYPE,
												   PR_PROVIDER_DISPLAY_A, PR_SERVICE_UID}};


	pthread_mutex_lock(&m_mutexserviceadmin);
	
	for(j=services.begin(); j != services.end(); j++) {
		
		if ((*j)->bInitialize == false) {
			hr = (*j)->service->MSGServiceEntry()(0, NULL, NULL, 0, 0, MSG_SERVICE_CREATE, 0, NULL, (LPPROVIDERADMIN)(*j)->provideradmin, NULL);
			if(hr !=hrSuccess)
				goto exit;
			
			(*j)->bInitialize = true;
		}
	}

	hr = Util::HrCopyUnicodePropTagArray(ulFlags, (LPSPropTagArray)&sptaProviderCols, &lpPropTagArray);
	if(hr != hrSuccess)
		goto exit;

	hr = ECMemTable::Create(lpPropTagArray, PR_ROWID, &lpTable);
	if(hr != hrSuccess)
		goto exit;
	
	// Loop through all providers, add each to the table
	for(i=providers.begin(); i != providers.end(); i++) {
		hr = (*i)->profilesection->GetProps(lpPropTagArray, 0, &cValues, &lpsProps);
		if (FAILED(hr))
			goto exit;
		
		sPropID.ulPropTag = PR_ROWID;
		sPropID.Value.ul = n++;
		
		hr = Util::HrAddToPropertyArray(lpsProps, cValues, &sPropID, &lpDest, &cValuesDest);
		if(hr != hrSuccess)
			goto exit;
		
		hr = lpTable->HrModifyRow(ECKeyTable::TABLE_ROW_ADD, NULL, lpDest, cValuesDest);
		if(hr != hrSuccess)
			goto exit;
		
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
	pthread_mutex_unlock(&m_mutexserviceadmin);

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

	TRACE_MAPILIB1(TRACE_RETURN, "M4LMsgServiceAdmin::GetProviderTable", "0x%08x", hr);
	return hr;
}

// iunknown passthru
ULONG M4LMsgServiceAdmin::AddRef() {
	TRACE_MAPILIB(TRACE_ENTRY, "M4LMsgServiceAdmin::AddRef", "");
    ULONG ulRef = M4LUnknown::AddRef();
	TRACE_MAPILIB1(TRACE_RETURN, "M4LMsgServiceAdmin::AddRef", "%d", ulRef);
	return ulRef;
}
ULONG M4LMsgServiceAdmin::Release() {
	TRACE_MAPILIB(TRACE_ENTRY, "M4LMsgServiceAdmin::Release", "");
    ULONG ulRef = M4LUnknown::Release();
	TRACE_MAPILIB1(TRACE_RETURN, "M4LMsgServiceAdmin::Release", "%d", ulRef);
	return ulRef;
}
HRESULT M4LMsgServiceAdmin::QueryInterface(REFIID refiid, void **lpvoid) {
	TRACE_MAPILIB(TRACE_ENTRY, "M4LMsgServiceAdmin::QueryInterface", "");
	HRESULT hr = hrSuccess;

	if ((refiid == IID_IMsgServiceAdmin) || (refiid == IID_IUnknown)) {
		AddRef();
		*lpvoid = (IMsgServiceAdmin *)this;
	} else
		hr = MAPI_E_INTERFACE_NOT_SUPPORTED;

	TRACE_MAPILIB1(TRACE_RETURN, "M4LMsgServiceAdmin::QueryInterface", "0x%08x", hr);
	return hr;
}


// ---
// M4LMAPISession
// ---
M4LMAPISession::M4LMAPISession(LPTSTR new_profileName, M4LMsgServiceAdmin *new_serviceAdmin) {
	profileName = (char*)new_profileName;
	serviceAdmin = new_serviceAdmin;
	serviceAdmin->AddRef();
	cValues = 0;
	lpProps = NULL;
}

M4LMAPISession::~M4LMAPISession() {
    std::map<GUID, IMsgStore *>::iterator iterStores;
    
    for(iterStores = mapStores.begin(); iterStores != mapStores.end(); iterStores++) {
        iterStores->second->Release();
    }

	if(lpProps)
		MAPIFreeBuffer(lpProps);

	serviceAdmin->Release();
}

HRESULT M4LMAPISession::GetLastError(HRESULT hResult, ULONG ulFlags, LPMAPIERROR* lppMAPIError) {
	TRACE_MAPILIB(TRACE_ENTRY, "M4LMAPISession::GetLastError", "");
    *lppMAPIError = NULL;
	TRACE_MAPILIB1(TRACE_RETURN, "M4LMAPISession::GetLastError", "0x%08x", hrSuccess);
    return hrSuccess;
}

/**
 * Get a list of all message stores in this session. With Zarafa in
 * Linux, this is always atleast your own and the public where
 * available.
 *
 * @param[in]	ulFlags		Unused in Linux.
 * @param[out]	lppTable	IMAPITable object
 * @return		HRESULT
 */
HRESULT M4LMAPISession::GetMsgStoresTable(ULONG ulFlags, LPMAPITABLE* lppTable) {
	TRACE_MAPILIB(TRACE_ENTRY, "M4LMAPISession::GetMsgStoresTable", "");
	HRESULT hr = hrSuccess;
	ULONG cValues = 0;
	LPSPropValue lpsProps = NULL;
	list<providerEntry *>::iterator i;
	ECMemTable *lpTable = NULL;
	ECMemTableView *lpTableView = NULL;
	LPSPropValue lpDest = NULL;
	ULONG cValuesDest = 0;
	SPropValue sPropID;
	LPSPropValue lpType = NULL;
	int n = 0;
	LPSPropTagArray lpPropTagArray = NULL;

	SizedSPropTagArray(11, sptaProviderCols) = {11, {PR_MDB_PROVIDER, PR_INSTANCE_KEY, PR_RECORD_KEY, PR_ENTRYID,
												   PR_DISPLAY_NAME_A, PR_OBJECT_TYPE, PR_RESOURCE_TYPE, PR_PROVIDER_UID,
												   PR_RESOURCE_FLAGS, PR_DEFAULT_STORE, PR_PROVIDER_DISPLAY_A}};
	
	pthread_mutex_lock(&serviceAdmin->m_mutexserviceadmin);

	hr = Util::HrCopyUnicodePropTagArray(ulFlags, (LPSPropTagArray)&sptaProviderCols, &lpPropTagArray);
	if(hr != hrSuccess)
		goto exit;

	hr = ECMemTable::Create(lpPropTagArray, PR_ROWID, &lpTable);
	if(hr != hrSuccess)
		goto exit;
	
	// Loop through all providers, add each to the table
	for (i = serviceAdmin->providers.begin(); i != serviceAdmin->providers.end(); i++) {
		hr = (*i)->profilesection->GetProps(lpPropTagArray, 0, &cValues, &lpsProps);
		if (FAILED(hr))
			goto exit;

		lpType = PpropFindProp(lpsProps, cValues, PR_RESOURCE_TYPE);
		if(lpType == NULL || lpType->Value.ul != MAPI_STORE_PROVIDER)
			goto next;


		sPropID.ulPropTag = PR_ROWID;
		sPropID.Value.ul = n++;
		
		hr = Util::HrAddToPropertyArray(lpsProps, cValues, &sPropID, &lpDest, &cValuesDest);
		if(hr != hrSuccess)
			goto exit;

		hr = lpTable->HrModifyRow(ECKeyTable::TABLE_ROW_ADD, NULL, lpDest, cValuesDest);
		if(hr != hrSuccess)
			goto exit;

next:
		if (lpDest)
			MAPIFreeBuffer(lpDest);
		lpDest = NULL;
		if (lpsProps)
			MAPIFreeBuffer(lpsProps);
		lpsProps = NULL;
	}
	
	hr = lpTable->HrGetView(createLocaleFromName(""), ulFlags, &lpTableView);
	if(hr != hrSuccess)
		goto exit;
		
	hr = lpTableView->QueryInterface(IID_IMAPITable, (void **)lppTable);
	
exit:
	pthread_mutex_unlock(&serviceAdmin->m_mutexserviceadmin);

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

	TRACE_MAPILIB1(TRACE_RETURN, "M4LMAPISession::GetMsgStoresTable", "0x%08x", hr);
	return hr;
}

/**
 * Open a Message Store on the server.
 *
 * @param[in]	ulUIParam	Unused.
 * @param[in]	cbEntryID	Size of lpEntryID
 * @param[in]	lpEntryID	EntryID identifier of store.
 * @param[in]	lpInterface	Requested interface on lppMDB return value.
 * @param[in]	ulFlags		Passed to MSProviderInit function of provider of the store. In Linux always zarafaclient.so.
 * @param[out]	lppMDB		Pointer to IMsgStore object
 * @return		HRESULT
 */
HRESULT M4LMAPISession::OpenMsgStore(ULONG ulUIParam, ULONG cbEntryID, LPENTRYID lpEntryID, LPCIID lpInterface, ULONG ulFlags,
									 LPMDB* lppMDB) {
	TRACE_MAPILIB(TRACE_ENTRY, "M4LMAPISession::OpenMsgStore", "");
	HRESULT hr = hrSuccess;
	LPMSPROVIDER msp = NULL;
	LPMAPISUP lpISupport = NULL;
	LPMDB mdb = NULL;
	ULONG mdbver;
	// I don't want these ...
	ULONG sizeSpoolSec;
	LPBYTE pSpoolSec = NULL;
	LPMAPITABLE lpTable = NULL;
	LPSRowSet lpsRows = NULL;
	MAPIUID sProviderUID;
	ULONG cbStoreEntryID = 0;
	LPENTRYID lpStoreEntryID = NULL;
	SVCService *service = NULL;

	SizedSPropTagArray(2, sptaProviders) = { 2, {PR_ENTRYID, PR_PROVIDER_UID} };

	if (lpEntryID == NULL || lppMDB == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

    // unwrap mapi store entry
    hr = UnWrapStoreEntryID(cbEntryID, lpEntryID, &cbStoreEntryID, &lpStoreEntryID);
    if (hr != hrSuccess)
        goto exit;

	// padding in entryid solves string ending
	hr = m4l_lpMAPISVC->GetService((char*)lpEntryID+4+sizeof(GUID)+2, &service);
	if (hr != hrSuccess)
		goto exit;
	
	// Find the profile section associated with this entryID
	hr = serviceAdmin->GetProviderTable(0, &lpTable);
	if(hr != hrSuccess)
		goto exit;
		
	hr = lpTable->SetColumns((LPSPropTagArray)&sptaProviders, 0);
	if (hr != hrSuccess)
		goto exit;
		
	while(TRUE) {
		hr = lpTable->QueryRows(1, 0, &lpsRows);
		if(hr != hrSuccess)
			goto exit;
			
		if(lpsRows->cRows != 1)
			break;
			
		if(lpsRows->aRow[0].lpProps[0].ulPropTag == PR_ENTRYID && 
			lpsRows->aRow[0].lpProps[0].Value.bin.cb == cbEntryID &&
			memcmp(lpsRows->aRow[0].lpProps[0].Value.bin.lpb, lpEntryID, cbEntryID) == 0) {
				// Found it
				memcpy(&sProviderUID, lpsRows->aRow[0].lpProps[1].Value.bin.lpb, sizeof(MAPIUID));
				break;
			
		}
		FreeProws(lpsRows);
		lpsRows = NULL;
	}
	
	if(lpsRows->cRows != 1) {
		// No provider for the store, use a temporary profile section
		lpISupport = new M4LMAPISupport(this, NULL, service);
	} else
		lpISupport = new M4LMAPISupport(this, &sProviderUID, service);
		
	lpISupport->AddRef();

	// call zarafa client for the Message Store Provider (provider/client/EntryPoint.cpp)
	hr = service->MSProviderInit()(0, NULL, MAPIAllocateBuffer, MAPIAllocateMore, MAPIFreeBuffer, ulFlags, CURRENT_SPI_VERSION, &mdbver, &msp);
	if (hr != hrSuccess)
		goto exit;

	hr = msp->Logon(lpISupport, 0, (LPTSTR)profileName.c_str(), cbStoreEntryID, lpStoreEntryID, ulFlags, NULL, &sizeSpoolSec, &pSpoolSec, NULL, NULL, &mdb);
	if (hr != hrSuccess)
		goto exit;

	hr = mdb->QueryInterface(lpInterface ? (*lpInterface) : IID_IMsgStore, (void**)lppMDB);

exit:
	if (lpsRows)
		FreeProws(lpsRows);
		
	if (lpTable)
		lpTable->Release();
		
	if (lpISupport)
		lpISupport->Release();	// MSProvider object has the ref, not us.

	if (msp)
		msp->Release();

	if (mdb)
		mdb->Release();

	if (pSpoolSec)
		MAPIFreeBuffer(pSpoolSec); // we don't need this ...
	
	if (lpStoreEntryID)
		MAPIFreeBuffer(lpStoreEntryID);
		
	TRACE_MAPILIB1(TRACE_RETURN, "M4LMAPISession::OpenMsgStore", "0x%08x", hr);
	return hr;
}

/**
 * Opens the Global Addressbook from a provider of the profile (service admin).
 *
 * @param[in]	ulUIParam	Unused in Linux.
 * @param[in]	lpInterface	Requested interface on the addressbook. If NULL, IID_IAddrBook is used.
 * @param[in]	ulFlags		Passed to ABProviderInit of the provider.
 * @param[out]	lppAdrBook	Pointer to an IAddrBook object
 * @return		HRESULT
 * @retval		MAPI_E_CALL_FAILED				Provider not available
 * @retval		MAPI_E_NOT_ENOUGH_MEMORY		Out of memory
 * @retval		MAPI_E_INTERFACE_NOT_SUPPORTED	Invalid lpInterface parameter
 */
HRESULT M4LMAPISession::OpenAddressBook(ULONG ulUIParam, LPCIID lpInterface, ULONG ulFlags, LPADRBOOK* lppAdrBook) {
	TRACE_MAPILIB(TRACE_ENTRY, "M4LMAPISession::OpenAddressBook", "");
	HRESULT hr = hrSuccess;
	IAddrBook *lpAddrBook = NULL;
	M4LAddrBook *myAddrBook;
	ULONG abver;
	LPABPROVIDER lpABProvider = NULL;
	LPMAPISUP lpMAPISup = NULL;
	SPropValue sProps[1];
    list<serviceEntry*>::iterator iService;

	lpMAPISup = new M4LMAPISupport(this, NULL, NULL);
	if (!lpMAPISup) {
		hr = MAPI_E_NOT_ENOUGH_MEMORY;
		goto exit;
	}

	myAddrBook = new M4LAddrBook(serviceAdmin, lpMAPISup);
	lpAddrBook = myAddrBook;
	if (!lpAddrBook) {
		hr = MAPI_E_NOT_ENOUGH_MEMORY;
		goto exit;
	}

	// Set default properties
	sProps[0].ulPropTag = PR_OBJECT_TYPE;
	sProps[0].Value.ul = MAPI_ADDRBOOK;

	hr = lpAddrBook->SetProps(1, sProps, NULL);
	if (hr != hrSuccess)
		goto exit;

	hr = lpAddrBook->QueryInterface(lpInterface ? (*lpInterface) : IID_IAddrBook, (void**)lppAdrBook);
	if (hr != hrSuccess)
		goto exit;

	for (iService = serviceAdmin->services.begin(); iService != serviceAdmin->services.end(); iService++) {
		if ((*iService)->service->ABProviderInit() == NULL)
			continue;

		if ((*iService)->service->ABProviderInit()(0, NULL, MAPIAllocateBuffer, MAPIAllocateMore, MAPIFreeBuffer, ulFlags, CURRENT_SPI_VERSION, &abver, &lpABProvider) == hrSuccess)
		{
			vector<SVCProvider*> vABProviders = (*iService)->service->GetProviders();
			LPSPropValue lpProps;
			ULONG cValues;
			for (vector<SVCProvider*>::iterator i = vABProviders.begin(); i != vABProviders.end(); i++) {
				LPSPropValue lpUID;
				LPSPropValue lpProp;
				std::string strDisplayName = "<unknown>";
				(*i)->GetProps(&cValues, &lpProps);

				lpProp = PpropFindProp(lpProps, cValues, PR_RESOURCE_TYPE);
				lpUID = PpropFindProp(lpProps, cValues, PR_AB_PROVIDER_ID);
				if (!lpUID || !lpProp || lpProp->Value.ul != MAPI_AB_PROVIDER)
					continue;

				lpProp = PpropFindProp(lpProps, cValues, PR_DISPLAY_NAME_A);
				if (lpProp)
					strDisplayName = lpProp->Value.lpszA;

				if (myAddrBook->addProvider(profileName, strDisplayName, (LPMAPIUID)lpUID->Value.bin.lpb, lpABProvider) != hrSuccess)
					hr = MAPI_W_ERRORS_RETURNED;
			}

			// lpAddrBook has the ref, not us
			lpABProvider->Release();
			lpABProvider = NULL;
		} else {
			hr = MAPI_W_ERRORS_RETURNED;
		}
	}

exit:
	// If returning S_OK or MAPI_W_ERRORS_RETURNED, lppAdrBook must be set
	TRACE_MAPILIB1(TRACE_RETURN, "M4LMAPISession::OpenAddressBook", "0x%08x", hr);
	return hr;
}

HRESULT M4LMAPISession::OpenProfileSection(LPMAPIUID lpUID, LPCIID lpInterface, ULONG ulFlags, LPPROFSECT* lppProfSect) {
	HRESULT hr = hrSuccess;
	TRACE_MAPILIB1(TRACE_ENTRY, "M4LMAPISession::OpenProfileSection", "%s", bin2hex(sizeof(GUID), (BYTE *)lpUID).c_str());
	hr = serviceAdmin->OpenProfileSection(lpUID, lpInterface, ulFlags, lppProfSect);
	TRACE_MAPILIB1(TRACE_RETURN, "M4LMAPISession::OpenProfileSection", "0x%08x", hr);
	return hr;
}

HRESULT M4LMAPISession::GetStatusTable(ULONG ulFlags, LPMAPITABLE* lppTable) {
	TRACE_MAPILIB(TRACE_ENTRY, "M4LMAPISession::GetStatusTable", "");
	TRACE_MAPILIB1(TRACE_RETURN, "M4LMAPISession::GetStatusTable", "0x%08x", MAPI_E_NO_SUPPORT);
	return MAPI_E_NO_SUPPORT;
}

/**
 * Opens any object identified by lpEntryID from a provider in the profile.
 *
 * @param[in]	cbEntryID	Size of lpEntryID.
 * @param[in]	lpEntryID	Unique identifier of an object.
 * @param[in]	lpInterface	Requested interface on the object. If NULL, default interface of objecttype is used.
 * @param[in]	ulFlags		Passed to OpenEntry of the provider of the EntryID.
 * @param[out]	lpulObjType	Type of the object returned. Eg. MAPI_MESSAGE, MAPI_STORE, etc.
 * @param[out]	lppUnk		IUnknown interface pointer of the object returned. Can be cast to the object returned in lpulObjType.
 * @return		HRESULT
 * @retval		MAPI_E_NOT_FOUND				lpEntryID is NULL, or not found to be an entryid for any provider of your profile.
 * @retval		MAPI_E_NOT_ENOUGH_MEMORY		Out of memory
 * @retval		MAPI_E_INVALID_ENTRYID			lpEntryID does not point to an entryid
 * @retval		MAPI_E_INTERFACE_NOT_SUPPORTED	Invalid lpInterface parameter for found object
 */
HRESULT M4LMAPISession::OpenEntry(ULONG cbEntryID, LPENTRYID lpEntryID, LPCIID lpInterface, ULONG ulFlags, ULONG* lpulObjType,
								  LPUNKNOWN* lppUnk) 
{
	TRACE_MAPILIB1(TRACE_ENTRY, "M4LMAPISession::OpenEntry", "%s", bin2hex(cbEntryID, (LPBYTE)lpEntryID).c_str());
    HRESULT hr = hrSuccess;
    IMAPITable *lpTable = NULL;
    IAddrBook *lpAddrBook = NULL;
    IMsgStore *lpMDB = NULL;
    LPSRowSet lpsRows = NULL;
    ULONG cbUnWrappedID = 0;
    LPENTRYID lpUnWrappedID = NULL;
	SizedSPropTagArray(3, sptaProviders) = { 3, {PR_ENTRYID, PR_RECORD_KEY, PR_RESOURCE_TYPE} };
	GUID guidProvider;
	bool bStoreEntryID = false;
	MAPIUID muidOneOff = {MAPI_ONE_OFF_UID};
	std::map<GUID, IMsgStore *>::iterator iterStores;

    if (lpEntryID == NULL) {
        hr = MAPI_E_NOT_FOUND;
        goto exit;
    }

	if (cbEntryID <= (4 + sizeof(GUID)) ) {
		hr = MAPI_E_INVALID_ENTRYID;
		goto exit;
	}
   
	// If this a wrapped entryid, just unwrap them.
	if (muidStoreWrap == *((GUID *)&lpEntryID->ab))
	{
		hr = UnWrapStoreEntryID(cbEntryID, lpEntryID, &cbUnWrappedID, &lpUnWrappedID);
		if (hr != hrSuccess)
			goto exit;

		cbEntryID = cbUnWrappedID;
		lpEntryID = lpUnWrappedID;
		bStoreEntryID = true;
 	}

    guidProvider = *((GUID *)&lpEntryID->ab); // first 16 bytes are the store/addrbook GUID
        
    // See if we already have the store open
    iterStores = mapStores.find(guidProvider);
    if(iterStores != mapStores.end()) {

		if (bStoreEntryID == true) {
			hr = iterStores->second->QueryInterface(IID_IMsgStore, (void**)lppUnk);
			if (hr == hrSuccess)
				*lpulObjType = MAPI_STORE;
		} else {
			hr = iterStores->second->OpenEntry(cbEntryID, lpEntryID, lpInterface, ulFlags, lpulObjType, lppUnk);
		}
        goto exit;
    }

    // If this is an addressbook EntryID or a one-off entryid, use the addressbook to open the item
	if (memcmp(&guidProvider, &muidOneOff, sizeof(GUID)) == 0) {
        hr = OpenAddressBook(0, NULL, AB_NO_DIALOG, &lpAddrBook);
        if(hr != hrSuccess)
            goto exit;

        hr = lpAddrBook->OpenEntry(cbEntryID, lpEntryID, lpInterface, ulFlags, lpulObjType, lppUnk);
        
        goto exit;
    }
            
    // If not, it must be a provider entryid, so we have to find the provider

	// Find the profile section associated with this entryID
	hr = serviceAdmin->GetProviderTable(0, &lpTable);
	if(hr != hrSuccess)
		goto exit;
		
	hr = lpTable->SetColumns((LPSPropTagArray)&sptaProviders, 0);
	if (hr != hrSuccess)
		goto exit;
		
	while(TRUE) {
		hr = lpTable->QueryRows(1, 0, &lpsRows);
		if(hr != hrSuccess)
			goto exit;
			
		if(lpsRows->cRows != 1) {
			hr = MAPI_E_NOT_FOUND;
			break;
		}
			
        if(lpsRows->aRow[0].lpProps[0].ulPropTag == PR_ENTRYID &&
            lpsRows->aRow[0].lpProps[1].ulPropTag == PR_RECORD_KEY &&
            lpsRows->aRow[0].lpProps[1].Value.bin.cb == sizeof(GUID) &&
            memcmp(lpsRows->aRow[0].lpProps[1].Value.bin.lpb, &guidProvider, sizeof(GUID)) == 0)
		{
			if (lpsRows->aRow[0].lpProps[2].ulPropTag == PR_RESOURCE_TYPE && lpsRows->aRow[0].lpProps[2].Value.ul == MAPI_AB_PROVIDER) {
				hr = OpenAddressBook(0, NULL, AB_NO_DIALOG, &lpAddrBook);
				if(hr != hrSuccess)
					goto exit;

				hr = lpAddrBook->OpenEntry(cbEntryID, lpEntryID, lpInterface, ulFlags, lpulObjType, lppUnk);

				break;
			} else {
                hr = OpenMsgStore(0, lpsRows->aRow[0].lpProps[0].Value.bin.cb, (LPENTRYID)lpsRows->aRow[0].lpProps[0].Value.bin.lpb,
								  &IID_IMsgStore, MDB_WRITE | MDB_NO_DIALOG | MDB_TEMPORARY, &lpMDB);
                if(hr != hrSuccess)
                    goto exit;
                  
                // Keep the store open in case somebody else needs it later (only via this function)
                mapStores.insert(std::map<GUID, IMsgStore *>::value_type(guidProvider, lpMDB));
          
				if(bStoreEntryID == true) {
					hr = lpMDB->QueryInterface(IID_IMsgStore, (void**)lppUnk);
		            if (hr == hrSuccess)
		                *lpulObjType = MAPI_STORE;
				}else{
	                hr = lpMDB->OpenEntry(cbEntryID, lpEntryID, lpInterface, ulFlags, lpulObjType, lppUnk);
				}
                
				break;
			}
        }
			
		FreeProws(lpsRows);
		lpsRows = NULL;
	}

exit:
	if(lpUnWrappedID)
		MAPIFreeBuffer(lpUnWrappedID);

    if(lpsRows)
        FreeProws(lpsRows);
        
    if(lpAddrBook)
        lpAddrBook->Release();
        
    if(lpTable)
        lpTable->Release();
	
	TRACE_MAPILIB1(TRACE_RETURN, "M4LMAPISession::OpenEntry", "0x%08x", hr);
	return hr;
}

/**
 * Compare two EntryIDs.
 * 
 * @param[in]	cbEntryID1		Length of the first entryid
 * @param[in]	lpEntryID1		First entryid.
 * @param[in]	cbEntryID2		Length of the first entryid
 * @param[in]	lpEntryID2		First entryid.
 * @param[in]	ulFlags			Unused.
 * @param[out]	lpulResult		TRUE if EntryIDs are the same, otherwise FALSE.
 *
 * @return		HRESULT
 * @retval		MAPI_E_INVALID_ENTRYID	either lpEntryID1 or lpEntryID2 is NULL
 */
HRESULT M4LMAPISession::CompareEntryIDs(ULONG cbEntryID1, LPENTRYID lpEntryID1, ULONG cbEntryID2, LPENTRYID lpEntryID2, ULONG ulFlags,
										ULONG* lpulResult) {
	HRESULT hr = hrSuccess;
	TRACE_MAPILIB(TRACE_ENTRY, "IMAPISession::CompareEntryIDs", "");

	if (cbEntryID1 != cbEntryID2)
		*lpulResult = FALSE;
	else if (!lpEntryID1 || !lpEntryID2)
		hr = MAPI_E_INVALID_ENTRYID;
	else if (memcmp(lpEntryID1, lpEntryID2, cbEntryID1) != 0)
		*lpulResult = FALSE;
	else
		*lpulResult = TRUE;

	TRACE_MAPILIB1(TRACE_RETURN, "IMAPISession::CompareEntryIDs", "0x%08x", hr);
	return hr;
}

/**
 * Request notifications on an object identified by lpEntryID. EntryID
 * must be on the default store in Linux.
 * 
 * @param[in]	cbEntryID		Length of lpEntryID
 * @param[in]	lpEntryID		EntryID of object to Advise on.
 * @param[in]	ulEventMask		Bitmask of events to receive notifications on.
 * @param[in]	lpAdviseSink	Callback function for notifications.
 * @param[out]	lpulConnection	Connection identifier for this notification, for use with IMAPISession::Unadvise()
 *
 * @return		HRESULT
 * @retval		MAPI_E_INVALID_ENTRYID	either lpEntryID1 or lpEntryID2 is NULL
 */
HRESULT M4LMAPISession::Advise(ULONG cbEntryID, LPENTRYID lpEntryID, ULONG ulEventMask, LPMAPIADVISESINK lpAdviseSink,
							   ULONG* lpulConnection) {
	TRACE_MAPILIB(TRACE_ENTRY, "M4LMAPISession::Advise", "");
	HRESULT hr = hrSuccess;
	IMsgStore *lpMsgStore = NULL;

	//FIXME: Advise should handle one or more stores/addressbooks not only the default store,
	//       the entry identifier can be an address book, message store object or
	//       NULL which means an advise on the MAPISession.
	//       MAPISessions should hold his own ulConnection list because it should work 
	//       with one or more different objects.

	hr = HrOpenDefaultStore(this, &lpMsgStore);
	if (hr != hrSuccess)
		goto exit;

	hr = lpMsgStore->Advise(cbEntryID, lpEntryID, ulEventMask, lpAdviseSink, lpulConnection);
	if (hr != hrSuccess)
		goto exit;

exit:
	if (lpMsgStore)
		lpMsgStore->Release();

	TRACE_MAPILIB1(TRACE_RETURN, "M4LMAPISession::Advise", "0x%08x", hr);
	return hr;
}

/**
 * Remove request for notifications for a specific ID.
 * 
 * @param[in]	ulConnection	Connection identifier of Adivse call.
 *
 * @return		HRESULT
 */
HRESULT M4LMAPISession::Unadvise(ULONG ulConnection) {
	TRACE_MAPILIB(TRACE_ENTRY, "M4LMAPISession::Unadvise", "");
	HRESULT hr = hrSuccess;
	IMsgStore *lpMsgStore = NULL;

	// FIXME: should work with an internal list of connections ids, see M4LMAPISession::Advise for more information.
	hr = HrOpenDefaultStore(this, &lpMsgStore);
	if (hr != hrSuccess)
		goto exit;

	hr = lpMsgStore->Unadvise(ulConnection);
	if (hr != hrSuccess)
		goto exit;

exit:
	if (lpMsgStore)
		lpMsgStore->Release();

	TRACE_MAPILIB1(TRACE_RETURN, "M4LMAPISession::Unadvise", "0x%08x", hr);
	return hr;
}

HRESULT M4LMAPISession::MessageOptions(ULONG ulUIParam, ULONG ulFlags, LPTSTR lpszAdrType, LPMESSAGE lpMessage) {
	TRACE_MAPILIB(TRACE_ENTRY, "M4LMAPISession::MessageOptions", "");
	TRACE_MAPILIB1(TRACE_RETURN, "M4LMAPISession::MessageOptions", "0x%08x", MAPI_E_NO_SUPPORT);
	return MAPI_E_NO_SUPPORT;
}

HRESULT M4LMAPISession::QueryDefaultMessageOpt(LPTSTR lpszAdrType, ULONG ulFlags, ULONG* lpcValues, LPSPropValue* lppOptions) {
	TRACE_MAPILIB(TRACE_ENTRY, "M4LMAPISession::QueryDefaultMessageOpt", "");
	TRACE_MAPILIB1(TRACE_RETURN, "M4LMAPISession::QueryDefaultMessageOpt", "0x%08x", MAPI_E_NO_SUPPORT);
	return MAPI_E_NO_SUPPORT;
}

HRESULT M4LMAPISession::EnumAdrTypes(ULONG ulFlags, ULONG* lpcAdrTypes, LPTSTR** lpppszAdrTypes) {
	TRACE_MAPILIB(TRACE_ENTRY, "M4LMAPISession::EnumAdrTypes", "");
	TRACE_MAPILIB1(TRACE_RETURN, "M4LMAPISession::EnumAdrTypes", "0x%08x", MAPI_E_NO_SUPPORT);
	return MAPI_E_NO_SUPPORT;
}

HRESULT M4LMAPISession::QueryIdentity(ULONG* lpcbEntryID, LPENTRYID* lppEntryID) {
	TRACE_MAPILIB(TRACE_ENTRY, "M4LMAPISession::QueryIdentity", "");
	HRESULT hr = hrSuccess;
	LPSPropValue lpProp = NULL;
	LPENTRYID lpEntryID = NULL;

	lpProp = PpropFindProp(this->lpProps, this->cValues, PR_IDENTITY_ENTRYID);
	if(lpProp == NULL) {
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}

	MAPIAllocateBuffer(lpProp->Value.bin.cb, (void **)&lpEntryID);
	memcpy(lpEntryID, lpProp->Value.bin.lpb, lpProp->Value.bin.cb);

	*lppEntryID = lpEntryID;
	*lpcbEntryID = lpProp->Value.bin.cb;

exit:
	TRACE_MAPILIB1(TRACE_RETURN, "M4LMAPISession::QueryIdentity", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

HRESULT M4LMAPISession::Logoff(ULONG ulUIParam, ULONG ulFlags, ULONG ulReserved) {
	TRACE_MAPILIB(TRACE_ENTRY, "M4LMAPISession::Logoff", "");
	TRACE_MAPILIB1(TRACE_RETURN, "M4LMAPISession::Logoff", "0x%08x", 0);
	return hrSuccess;
}

HRESULT M4LMAPISession::SetDefaultStore(ULONG ulFlags, ULONG cbEntryID, LPENTRYID lpEntryID) {
	TRACE_MAPILIB(TRACE_ENTRY, "M4LMAPISession::SetDefaultStore", "");
	TRACE_MAPILIB1(TRACE_RETURN, "M4LMAPISession::SetDefaultStore", "0x%08x", MAPI_E_NO_SUPPORT);
	return MAPI_E_NO_SUPPORT;
}

HRESULT M4LMAPISession::AdminServices(ULONG ulFlags, LPSERVICEADMIN* lppServiceAdmin) {
	HRESULT hr = hrSuccess;
	TRACE_MAPILIB(TRACE_ENTRY, "M4LMAPISession::AdminServices", "");
	serviceAdmin->QueryInterface(IID_IMsgServiceAdmin,(void**)lppServiceAdmin);
	TRACE_MAPILIB1(TRACE_RETURN, "M4LMAPISession::AdminServices", "0x%08x", hr);
	return hr;
}

HRESULT M4LMAPISession::ShowForm(ULONG ulUIParam, LPMDB lpMsgStore, LPMAPIFOLDER lpParentFolder, LPCIID lpInterface,
								 ULONG ulMessageToken, LPMESSAGE lpMessageSent, ULONG ulFlags, ULONG ulMessageStatus,
								 ULONG ulMessageFlags, ULONG ulAccess, LPSTR lpszMessageClass) {
	TRACE_MAPILIB(TRACE_ENTRY, "M4LMAPISession::ShowForm", "");
	TRACE_MAPILIB1(TRACE_RETURN, "M4LMAPISession::ShowForm", "0x%08x", MAPI_E_NO_SUPPORT);
	return MAPI_E_NO_SUPPORT;
}

HRESULT M4LMAPISession::PrepareForm(LPCIID lpInterface, LPMESSAGE lpMessage, ULONG* lpulMessageToken) {
	TRACE_MAPILIB(TRACE_ENTRY, "M4LMAPISession::PrepareForm", "");
	TRACE_MAPILIB1(TRACE_RETURN, "M4LMAPISession::PrepareForm", "0x%08x", MAPI_E_NO_SUPPORT);
	return MAPI_E_NO_SUPPORT;
}

// iunknown passthru
ULONG M4LMAPISession::AddRef() {
	TRACE_MAPILIB(TRACE_ENTRY, "M4LMAPISession::AddRef", "");
    return M4LUnknown::AddRef();
}
ULONG M4LMAPISession::Release() {
	TRACE_MAPILIB(TRACE_ENTRY, "M4LMAPISession::Release", "");
    return M4LUnknown::Release();
}
HRESULT M4LMAPISession::QueryInterface(REFIID refiid, void **lpvoid) {
	TRACE_MAPILIB1(TRACE_ENTRY, "M4LMAPISession::QueryInterface", "%s", bin2hex(sizeof(GUID), (BYTE *)&refiid).c_str());
	HRESULT hr = hrSuccess;

	if ((refiid == IID_IMAPISession) || (refiid == IID_IUnknown)) {
		AddRef();
		*lpvoid = (IMAPISession *)this;
	} else {
		hr = MAPI_E_INTERFACE_NOT_SUPPORTED;
	}

	TRACE_MAPILIB1(TRACE_RETURN, "M4LMAPISession::QueryInterface", "0x%08x", hr);
	return hr;
}


// ---
// M4LAddrBook
// ---
M4LAddrBook::M4LAddrBook(M4LMsgServiceAdmin *new_serviceAdmin, LPMAPISUP newlpMAPISup) {
	serviceAdmin = new_serviceAdmin;
	m_lpMAPISup = newlpMAPISup;
	m_lpMAPISup->AddRef();
	m_lpSavedSearchPath = NULL;
}

M4LAddrBook::~M4LAddrBook() {
	for (list<abEntry>::iterator i = m_lABProviders.begin(); i != m_lABProviders.end(); i++) {
		i->lpABLogon->Logoff(0);
		i->lpABLogon->Release();
		i->lpABProvider->Release();	// TODO: call shutdown too? useless..
	}
	if(m_lpMAPISup)
		m_lpMAPISup->Release();
	if (m_lpSavedSearchPath)
		FreeProws(m_lpSavedSearchPath);
}

HRESULT M4LAddrBook::addProvider(const std::string &profilename, const std::string &displayname, LPMAPIUID lpUID, LPABPROVIDER newProvider) {
	HRESULT hr = hrSuccess;
	ULONG cbSecurity;
	LPBYTE lpSecurity = NULL;
	LPMAPIERROR lpMAPIError = NULL;
	LPABLOGON lpABLogon = NULL;
	pair<map<LPABPROVIDER,LPABLOGON>::iterator, bool> iRes;
	abEntry entry;

	hr = newProvider->Logon(m_lpMAPISup, 0, (TCHAR*)profilename.c_str(), 0, &cbSecurity, &lpSecurity, &lpMAPIError, &lpABLogon);
	if (hr != hrSuccess)
		goto exit;

	// @todo?, call lpABLogon->OpenEntry(0,NULL) to get the root folder, and save that entryid that we can use for the GetHierarchyTable of our root container?

	memcpy(&entry.muid, lpUID, sizeof(MAPIUID));
	entry.displayname = displayname;
	entry.lpABProvider = newProvider;
	newProvider->AddRef();
	entry.lpABLogon = lpABLogon;

	m_lABProviders.push_back(entry);

exit:
	if (lpSecurity)
		MAPIFreeBuffer(lpSecurity);

	if (lpMAPIError)
		MAPIFreeBuffer(lpMAPIError);

	return hr;
}

HRESULT M4LAddrBook::getDefaultSearchPath(ULONG ulFlags, LPSRowSet* lppSearchPath)
{
	HRESULT hr = hrSuccess;
	IABContainer *lpRoot = NULL;
	IMAPITable *lpTable = NULL;
	ULONG ulObjType;
	SPropValue sProp;
	ECAndRestriction cRes;
	LPSRestriction lpRes = NULL;

	hr = this->OpenEntry(0, NULL, &IID_IABContainer, 0, &ulObjType, (LPUNKNOWN*)&lpRoot);
	if (hr != hrSuccess)
		goto exit;

	hr = lpRoot->GetHierarchyTable((ulFlags & MAPI_UNICODE) | CONVENIENT_DEPTH, &lpTable);
	if (hr != hrSuccess)
		goto exit;

	// We add this restriction to filter out All Address Lists
	sProp.ulPropTag = 0xFFFD0003; //PR_EMS_AB_CONTAINERID;
	sProp.Value.ul = 7000;
	cRes.append(ECOrRestriction(
					ECPropertyRestriction(RELOP_NE, sProp.ulPropTag, &sProp) +
					ECNotRestriction(ECExistRestriction(sProp.ulPropTag)))
				);
	// only folders, not groups
	sProp.ulPropTag = PR_DISPLAY_TYPE;
	sProp.Value.ul = DT_NOT_SPECIFIC;
	cRes.append(ECPropertyRestriction(RELOP_EQ, sProp.ulPropTag, &sProp));
	// only end folders, not root container folders
	cRes.append(ECBitMaskRestriction(BMR_EQZ, PR_CONTAINER_FLAGS, AB_SUBCONTAINERS));

	hr = cRes.CreateMAPIRestriction(&lpRes);
	if (hr != hrSuccess)
		goto exit;

	hr = lpTable->Restrict(lpRes, 0);
	if (hr != hrSuccess)
		goto exit;

	hr = lpTable->QueryRows(-1, 0, lppSearchPath);
	if (hr != hrSuccess)
		goto exit;

exit:
	if (lpRes)
		MAPIFreeBuffer(lpRes);

	if (lpRoot)
		lpRoot->Release();

	if (lpTable)
		lpTable->Release();

	return hr;
}


// 
// How it works:
//   1. program calls M4LMAPISession::OpenAddressBook()
//      OpenAddressBook calls ABProviderInit() for all AB providers in the profile, and adds the returned
//      IABProvider/IABLogon interface to the Addressbook
//   2.1a. program calls M4LAddrBook::OpenEntry()
//         - lpEntryID == NULL, open root container.
//           this is a IABContainer. On this interface, use GetHierarchyTable() to get the list of all the providers'
//           entry id's. (eg. Global Address Book, Outlook addressbook)
//           This IABContainer version should be implemented as M4LABContainer
//         - lpEntryID != NULL, pass to correct IABLogon::OpenEntry()
//   2.1b. program calls M4LAddrBook::ResolveName()
//         - for every IABLogon object (in the searchpath), use OpenEntry() to get the IABContainer, and call ResolveNames()
// 

/**
 * Remove request for notifications for a specific ID.
 * 
 * @param[in]	cbEntryID	Length of lpEntryID.
 * @param[in]	lpEntryID	Unique entryid of a mapi object in the addressbook.
 * @param[in]	lpInterface	MAPI Interface to query on the object.
 * @param[in]	ulFlags		Passed to OpenEntry of the addressbook.
 * @param[out]	lpulObjType	The type of the return object
 * @param[out]	lppUnk		IUnknown pointer to the opened object. Cast to correct object.
 *
 * @return		HRESULT
 */
HRESULT M4LAddrBook::OpenEntry(ULONG cbEntryID, LPENTRYID lpEntryID, LPCIID lpInterface, ULONG ulFlags, ULONG * lpulObjType,
							   LPUNKNOWN * lppUnk) {
	
    TRACE_MAPILIB(TRACE_ENTRY, "M4LAddrBook::OpenEntry", "");

	HRESULT hr = hrSuccess;
	std::wstring name, type, email;
	IMAPIProp *lpMailUser;
	SPropValue sProps[4];

	if ((lpInterface == NULL || *lpInterface == IID_IMailUser || *lpInterface == IID_IMAPIProp || *lpInterface == IID_IUnknown) && lpEntryID != NULL) {
		hr = ECParseOneOff(lpEntryID, cbEntryID, name, type, email);
		if (hr == hrSuccess) {
			// yes, it was an one off, create IMailUser
			lpMailUser = new M4LMAPIProp();
			lpMailUser->AddRef();

			sProps[0].ulPropTag = PR_DISPLAY_NAME_W;
			sProps[0].Value.lpszW = (WCHAR*)name.c_str();

			sProps[1].ulPropTag = PR_ADDRTYPE_W;
			sProps[1].Value.lpszW = (WCHAR*)type.c_str();

			sProps[2].ulPropTag = PR_EMAIL_ADDRESS_W;
			sProps[2].Value.lpszW = (WCHAR*)email.c_str();

			sProps[3].ulPropTag = PR_ENTRYID;
			sProps[3].Value.bin.cb = cbEntryID;
			sProps[3].Value.bin.lpb = (BYTE *)lpEntryID;

			lpMailUser->SetProps(4, sProps, NULL);

			*lppUnk = (LPUNKNOWN)lpMailUser;
			*lpulObjType = MAPI_MAILUSER;

			goto exit;
		}
	}

	if (lpEntryID == NULL && (lpInterface == NULL || *lpInterface == IID_IABContainer)) {
		// 2.1a1: open root container, make an M4LABContainer which have the ABContainers of all providers as hierarchy entries.
		M4LABContainer *lpCont = NULL;
		SPropValue sPropObjectType;
		lpCont = new M4LABContainer(m_lABProviders);

		hr = lpCont->QueryInterface(IID_IABContainer, (void**)lppUnk);
		if (hr != hrSuccess) {
			delete lpCont;
			goto exit;
		}

		sPropObjectType.ulPropTag = PR_OBJECT_TYPE;
		sPropObjectType.Value.ul = MAPI_ABCONT;
		lpCont->SetProps(1, &sPropObjectType, NULL);

		*lpulObjType = MAPI_ABCONT;
	} else if (lpEntryID) {
		std::list<abEntry>::iterator i;

		hr = MAPI_E_UNKNOWN_ENTRYID;
		for (i = m_lABProviders.begin(); i != m_lABProviders.end(); i++) {
			if (memcmp((BYTE*)lpEntryID +4, &i->muid, sizeof(MAPIUID)) == 0)
				break;
		}
		if (i != m_lABProviders.end())
			hr = i->lpABLogon->OpenEntry(cbEntryID, lpEntryID, lpInterface, ulFlags, lpulObjType, lppUnk);
	} else {
		// lpEntryID == NULL
		hr = MAPI_E_INTERFACE_NOT_SUPPORTED;
	}

exit:
	TRACE_MAPILIB1(TRACE_RETURN, "M4LAddrBook::OpenEntry", "%s", GetMAPIErrorDescription(hr).c_str());
	return hr;
}

/** 
 * @todo Should use the GetSearchPath items, not just all providers.
 * 
 * @param cbEntryID1 
 * @param lpEntryID1 
 * @param cbEntryID2 
 * @param lpEntryID2 
 * @param ulFlags 
 * @param lpulResult 
 * 
 * @return 
 */
HRESULT M4LAddrBook::CompareEntryIDs(ULONG cbEntryID1, LPENTRYID lpEntryID1, ULONG cbEntryID2, LPENTRYID lpEntryID2,
									 ULONG ulFlags, ULONG* lpulResult)
{
    TRACE_MAPILIB(TRACE_ENTRY, "M4LAddrBook::CompareEntryIDs", "");
	HRESULT hr = hrSuccess;

	// m_lABProviders[0] probably always is Zarafa
	for (std::list<abEntry>::iterator i = m_lABProviders.begin(); i != m_lABProviders.end(); i++) {
		hr = i->lpABLogon->CompareEntryIDs(cbEntryID1, lpEntryID1, cbEntryID2, lpEntryID2, ulFlags, lpulResult);
		if (hr == hrSuccess || hr != MAPI_E_NO_SUPPORT)
			break;
	}

	TRACE_MAPILIB1(TRACE_RETURN, "M4LAddrBook::CompareEntryIDs", "0x%08x", hr);
	return hr;
}

HRESULT M4LAddrBook::Advise(ULONG cbEntryID, LPENTRYID lpEntryID, ULONG ulEventMask, LPMAPIADVISESINK lpAdviseSink,
							ULONG* lpulConnection) {
    TRACE_MAPILIB(TRACE_ENTRY, "M4LAddrBook::Advise", "");
	TRACE_MAPILIB1(TRACE_RETURN, "M4LAddrBook::Advise", "0x%08x", MAPI_E_NO_SUPPORT);
	return MAPI_E_NO_SUPPORT;
}

HRESULT M4LAddrBook::Unadvise(ULONG ulConnection) {
    TRACE_MAPILIB(TRACE_ENTRY, "M4LAddrBook::Unadvise", "");
	TRACE_MAPILIB1(TRACE_RETURN, "M4LAddrBook::Unadvise", "0x%08x", MAPI_E_NO_SUPPORT);
	return MAPI_E_NO_SUPPORT;
}

/**
 * Create a OneOff EntryID.
 *
 * @param[in]	lpszName		Displayname of object
 * @param[in]	lpszAdrType		Addresstype of EntryID. Mostly SMTP or ZARAFA.
 * @param[in]	lpszAddress		Address of EntryID, according to type.
 * @param[in]	ulFlags			Enable MAPI_UNICODE flag if input strings are WCHAR strings. Output will be unicode too.
 * @param[out]	lpcbEntryID		Length of lppEntryID
 * @param[out]	lpplpEntryID	OneOff EntryID for object.
 *
 * @return	HRESULT
 */
HRESULT M4LAddrBook::CreateOneOff(LPTSTR lpszName, LPTSTR lpszAdrType, LPTSTR lpszAddress, ULONG ulFlags, ULONG* lpcbEntryID,
								  LPENTRYID* lppEntryID) {
    TRACE_MAPILIB(TRACE_ENTRY, "M4LAddrBook::CreateOneOff", "");
	HRESULT hr = ECCreateOneOff(lpszName, lpszAdrType, lpszAddress, ulFlags, lpcbEntryID, lppEntryID);
	TRACE_MAPILIB2(TRACE_RETURN, "M4LAddrBook::CreateOneOff", "0x%08x: %s", hr, *lppEntryID ? bin2hex(*lpcbEntryID, (unsigned char *)lppEntryID).c_str() : "<none>");
	return hr;
}

HRESULT M4LAddrBook::NewEntry(ULONG ulUIParam, ULONG ulFlags, ULONG cbEIDContainer, LPENTRYID lpEIDContainer,
									   ULONG cbEIDNewEntryTpl, LPENTRYID lpEIDNewEntryTpl, ULONG* lpcbEIDNewEntry,
							  LPENTRYID* lppEIDNewEntry) {
    TRACE_MAPILIB(TRACE_ENTRY, "M4LAddrBook::NewEntry", "");
	TRACE_MAPILIB1(TRACE_RETURN, "M4LAddrBook::NewEntry", "0x%08x", MAPI_E_NO_SUPPORT);
	return MAPI_E_NO_SUPPORT;
}

/**
 * Resolve a list of names in the addressbook.
 * @todo use GetSearchPath, and not to loop over all providers
 *
 * @param[in]	ulUIParam		Unused in Linux.
 * @param[in]	ulFlags			Passed to ResolveNames in Addressbook.
								MAPI_UNICODE, strings in lpAdrList are WCHAR strings.
 * @param[in]	lpszNewEntryTitle	Unused in Linux
 * @param[in,out]	lpAdrList	A list of names to resolve. Resolved users are present in this list (although you can't tell of an error is returned which).
 *
 * @return	HRESULT
 * @retval	MAPI_E_AMBIGUOUS_RECIP	One or more recipients in the list are ambiguous.
 * @retval	MAPI_E_UNRESOLVED		One or more recipients in the list are not resolved.
 */
// should use PR_AB_SEARCH_PATH
HRESULT M4LAddrBook::ResolveName(ULONG ulUIParam, ULONG ulFlags, LPTSTR lpszNewEntryTitle, LPADRLIST lpAdrList) {
	TRACE_MAPILIB(TRACE_ENTRY, "M4LAddrBook::ResolveName", "");
	HRESULT hr = hrSuccess;
	ULONG objType;
	LPABCONT lpABContainer = NULL;
	LPFlagList lpFlagList;
	LPENTRYID lpOneEntryID = NULL;
	ULONG cbOneEntryID = 0;
	LPSPropValue lpNewProps = NULL;
	LPSPropValue lpNewRow = NULL;
	ULONG cNewRow = 0;
	LPSRowSet lpSearchRows = NULL;
	bool bContinue = true;

	if (lpAdrList == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	hr = MAPIAllocateBuffer(CbNewFlagList(lpAdrList->cEntries), (void**)&lpFlagList);
	if (hr != hrSuccess)
		goto exit;

	memset(lpFlagList, 0, CbNewFlagList(lpAdrList->cEntries));
	lpFlagList->cFlags = lpAdrList->cEntries;

	// Resolve local items
	for(unsigned int i=0; i<lpAdrList->cEntries;i++) {
		LPSPropValue lpDisplay = PpropFindProp(lpAdrList->aEntries[i].rgPropVals, lpAdrList->aEntries[i].cValues, PR_DISPLAY_NAME_A);
		LPSPropValue lpDisplayW = PpropFindProp(lpAdrList->aEntries[i].rgPropVals, lpAdrList->aEntries[i].cValues, PR_DISPLAY_NAME_W);
		LPSPropValue lpEntryID = PpropFindProp(lpAdrList->aEntries[i].rgPropVals, lpAdrList->aEntries[i].cValues, PR_ENTRYID);
		wstring strwDisplay;
		wstring strwType;
		wstring strwAddress;

		if(lpEntryID != NULL) {
			// Item is already resolved, leave it untouched
			lpFlagList->ulFlag[i] = MAPI_RESOLVED;
			continue;
		}

		if(lpDisplay == NULL && lpDisplayW == NULL) // Can't do much without the PR_DISPLAY_NAME
			continue;

		// Use PT_UNICODE display string if available, otherwise fallback to PT_STRING8
		if(lpDisplayW)
			strwDisplay = lpDisplayW->Value.lpszW;
		else
			strwDisplay = convert_to<std::wstring>(lpDisplay->Value.lpszA);

		// Handle 'DISPLAYNAME [ADDRTYPE:EMAILADDR]' strings
		size_t lbracketpos = strwDisplay.find('[');
		size_t rbracketpos = strwDisplay.find(']');
		size_t colonpos = strwDisplay.find(':');
		if(colonpos != std::string::npos && lbracketpos != std::string::npos && rbracketpos != std::string::npos) {
			strwType = strwDisplay.substr(lbracketpos+1, colonpos - lbracketpos - 1); // Everything from '[' up to ':'
			strwAddress = strwDisplay.substr(colonpos+1, rbracketpos - colonpos - 1); // Everything after ':' up to ']'
			strwDisplay = strwDisplay.substr(0, lbracketpos); // Everything before '['

			lpFlagList->ulFlag[i] = MAPI_RESOLVED;

			MAPIAllocateBuffer(sizeof(SPropValue) * 4, (void **)&lpNewProps);

			lpNewProps[0].ulPropTag = PR_ENTRYID;
			hr = CreateOneOff((LPTSTR)strwDisplay.c_str(), (LPTSTR)strwType.c_str(), (LPTSTR)strwAddress.c_str(), MAPI_UNICODE, &cbOneEntryID, &lpOneEntryID);
			if(hr != hrSuccess)
				goto exit;

			MAPIAllocateMore(cbOneEntryID, lpNewProps, (void **)&lpNewProps[0].Value.bin.lpb);
			memcpy(lpNewProps[0].Value.bin.lpb, lpOneEntryID, cbOneEntryID);
			lpNewProps[0].Value.bin.cb = cbOneEntryID;

			if(ulFlags & MAPI_UNICODE) {
				lpNewProps[1].ulPropTag = PR_DISPLAY_NAME_W;
				MAPIAllocateMore(sizeof(WCHAR) * (strwDisplay.length() + 1), lpNewProps, (void **)&lpNewProps[1].Value.lpszW);
				wcscpy(lpNewProps[1].Value.lpszW, strwDisplay.c_str());

				lpNewProps[2].ulPropTag = PR_ADDRTYPE_W;
				MAPIAllocateMore(sizeof(WCHAR) * (strwType.length() + 1), lpNewProps, (void **)&lpNewProps[2].Value.lpszW);
				wcscpy(lpNewProps[2].Value.lpszW, strwType.c_str());

				lpNewProps[3].ulPropTag = PR_EMAIL_ADDRESS_W;
				MAPIAllocateMore(sizeof(WCHAR) * (strwAddress.length() + 1), lpNewProps, (void **)&lpNewProps[3].Value.lpszW);
				wcscpy(lpNewProps[3].Value.lpszW, strwAddress.c_str());
			} else {
				std::string conv;

				conv = convert_to<std::string>(strwDisplay);
				lpNewProps[1].ulPropTag = PR_DISPLAY_NAME_A;
				MAPIAllocateMore(conv.length() + 1, lpNewProps, (void **)&lpNewProps[1].Value.lpszA);
				strcpy(lpNewProps[1].Value.lpszA, conv.c_str());

				conv = convert_to<std::string>(strwType);
				lpNewProps[2].ulPropTag = PR_ADDRTYPE_A;
				MAPIAllocateMore(conv.length() + 1, lpNewProps, (void **)&lpNewProps[2].Value.lpszA);
				strcpy(lpNewProps[2].Value.lpszA, conv.c_str());

				conv = convert_to<std::string>(strwAddress);
				lpNewProps[3].ulPropTag = PR_EMAIL_ADDRESS_A;
				MAPIAllocateMore(conv.length() + 1, lpNewProps, (void **)&lpNewProps[3].Value.lpszA);
				strcpy(lpNewProps[3].Value.lpszA, conv.c_str());
			}

			MAPIFreeBuffer(lpOneEntryID);
			lpOneEntryID = NULL;

			// Copy old properties + lpNewProps into row
			hr = Util::HrMergePropertyArrays(lpAdrList->aEntries[i].rgPropVals, lpAdrList->aEntries[i].cValues, lpNewProps, 4, &lpNewRow, &cNewRow);
			if(hr != hrSuccess)
				goto exit;

			MAPIFreeBuffer(lpAdrList->aEntries[i].rgPropVals);
			lpAdrList->aEntries[i].rgPropVals = lpNewRow;
			lpAdrList->aEntries[i].cValues = cNewRow;
			lpNewRow = NULL;

			MAPIFreeBuffer(lpNewProps);
			lpNewProps = NULL;
		}
	}

	hr = this->GetSearchPath(MAPI_UNICODE, &lpSearchRows);
	if (hr != hrSuccess)
		goto exit;

	for (ULONG c = 0; bContinue && c < lpSearchRows->cRows; c++) {
		LPSPropValue lpEntryID = PpropFindProp(lpSearchRows->aRow[c].lpProps, lpSearchRows->aRow[c].cValues, PR_ENTRYID);

		if (!lpEntryID)
			continue;

		hr = this->OpenEntry(lpEntryID->Value.bin.cb, (LPENTRYID)lpEntryID->Value.bin.lpb, &IID_IABContainer, 0, &objType, (IUnknown**)&lpABContainer);
		if (hr != hrSuccess)
			goto next;

		hr = lpABContainer->ResolveNames(NULL, ulFlags, lpAdrList, lpFlagList);
		if (FAILED(hr))
			goto next;
		hr = hrSuccess;

		bContinue = false;
		// may have warnings .. let's find out
		for (ULONG i = 0; i < lpFlagList->cFlags; i++) {
			if (lpFlagList->ulFlag[i] == MAPI_AMBIGUOUS) {
				hr = MAPI_E_AMBIGUOUS_RECIP;
				goto exit;
			} else if (lpFlagList->ulFlag[i] == MAPI_UNRESOLVED) {
				bContinue = true;
			}
		}

	next:
		lpABContainer->Release();
		lpABContainer = NULL;
	}

	
	// check for still unresolved addresses
	for (ULONG i = 0; bContinue && i < lpFlagList->cFlags; i++) {
		if (lpFlagList->ulFlag[i] == MAPI_UNRESOLVED) {
			hr = MAPI_E_NOT_FOUND;
			break;
		}
	}

exit:
	if (lpNewProps)
		MAPIFreeBuffer(lpNewProps);

	if (lpNewRow)
		MAPIFreeBuffer(lpNewRow);

	if (lpOneEntryID)
		MAPIFreeBuffer(lpOneEntryID);

	if (lpFlagList)
		MAPIFreeBuffer(lpFlagList);

	if (lpABContainer)
		lpABContainer->Release();

	if (lpSearchRows)
		FreeProws(lpSearchRows);

	TRACE_MAPILIB1(TRACE_RETURN, "M4LAddrBook::ResolveName", "0x%08x", hr);
	return hr;
}

HRESULT M4LAddrBook::Address(ULONG* lpulUIParam, LPADRPARM lpAdrParms, LPADRLIST* lppAdrList) {
    TRACE_MAPILIB(TRACE_ENTRY, "M4LAddrBook::Address", "");
	TRACE_MAPILIB1(TRACE_RETURN, "M4LAddrBook::Address", "0x%08x", MAPI_E_NO_SUPPORT);
	return MAPI_E_NO_SUPPORT;
}

HRESULT M4LAddrBook::Details(ULONG* lpulUIParam, LPFNDISMISS lpfnDismiss, LPVOID lpvDismissContext, ULONG cbEntryID,
									  LPENTRYID lpEntryID, LPFNBUTTON lpfButtonCallback, LPVOID lpvButtonContext,
							 LPTSTR lpszButtonText, ULONG ulFlags) {
    TRACE_MAPILIB(TRACE_ENTRY, "M4LAddrBook::Details", "");
	TRACE_MAPILIB1(TRACE_RETURN, "M4LAddrBook::Details", "0x%08x", MAPI_E_NO_SUPPORT);
	return MAPI_E_NO_SUPPORT;
}

HRESULT M4LAddrBook::RecipOptions(ULONG ulUIParam, ULONG ulFlags, LPADRENTRY lpRecip) {
    TRACE_MAPILIB(TRACE_ENTRY, "M4LAddrBook::RecipOptions", "");
	TRACE_MAPILIB1(TRACE_RETURN, "M4LAddrBook::RecipOptions", "0x%08x", MAPI_E_NO_SUPPORT);
	return MAPI_E_NO_SUPPORT;
}

HRESULT M4LAddrBook::QueryDefaultRecipOpt(LPTSTR lpszAdrType, ULONG ulFlags, ULONG* lpcValues, LPSPropValue* lppOptions) {
    TRACE_MAPILIB(TRACE_ENTRY, "M4LAddrBook::QueryDefaultRecipOpt", "");
	TRACE_MAPILIB1(TRACE_RETURN, "M4LAddrBook::QueryDefaultRecipOpt", "0x%08x", MAPI_E_NO_SUPPORT);
	return MAPI_E_NO_SUPPORT;
}

// Get Personal AddressBook
HRESULT M4LAddrBook::GetPAB(ULONG* lpcbEntryID, LPENTRYID* lppEntryID) {
    TRACE_MAPILIB(TRACE_ENTRY, "M4LAddrBook::GetPAB", "");
	TRACE_MAPILIB1(TRACE_RETURN, "M4LAddrBook::GetPAB", "0x%08x", MAPI_E_NO_SUPPORT);
	return MAPI_E_NO_SUPPORT;
}

// Set Personal AddressBook
HRESULT M4LAddrBook::SetPAB(ULONG cbEntryID, LPENTRYID lpEntryID) {
    TRACE_MAPILIB(TRACE_ENTRY, "M4LAddrBook::SetPAB", "");
	TRACE_MAPILIB1(TRACE_RETURN, "M4LAddrBook::SetPAB", "0x%08x", MAPI_E_NO_SUPPORT);
	return MAPI_E_NO_SUPPORT;
}

/**
 * Returns the EntryID of the Global Address Book container.
 *
 * @param[out]	lpcbEntryID	Length of lppEntryID.
 * @param[out]	lppEntryID	Unique identifier of the GAB container.
 *
 * @return	HRESULT
 * @retval	MAPI_E_INVALID_PARAMETER	Invalid input
 * @retval	MAPI_E_NOT_FOUND			Addressbook	is not available.
 */
HRESULT M4LAddrBook::GetDefaultDir(ULONG* lpcbEntryID, LPENTRYID* lppEntryID) {
    TRACE_MAPILIB(TRACE_ENTRY, "M4LAddrBook::GetDefaultDir", "");
	HRESULT hr = hrSuccess;
	ULONG objType;
	LPABCONT lpABContainer = NULL;
	LPSPropValue propEntryID = NULL;
	LPMAPITABLE lpTable = NULL;
	LPSRowSet lpRowSet = NULL;
	ULONG cbEntryID;
	LPENTRYID lpEntryID = NULL;
	LPSPropValue lpProp = NULL;

	if (lpcbEntryID == NULL || lppEntryID == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	// m_lABProviders[0] probably always is Zarafa
	for (std::list<abEntry>::iterator i = m_lABProviders.begin(); i != m_lABProviders.end(); i++) {
		// find a working open root container
		hr = i->lpABLogon->OpenEntry(0, NULL, &IID_IABContainer, 0, &objType, (IUnknown**)&lpABContainer);
		if (hr == hrSuccess)
			break;
	}
	if (hr != hrSuccess)
		goto exit;

	// more steps with gethierarchy() -> get entryid -> OpenEntry() ?
	hr = lpABContainer->GetHierarchyTable(0, &lpTable);
	if (hr != hrSuccess)
		goto no_hierarchy;

	hr = lpTable->QueryRows(1, 0, &lpRowSet); // can only return 1 row, as there is only 1 
	if (hr != hrSuccess)
		goto no_hierarchy;

	// get entry id from table, use it.
	lpProp = PpropFindProp(lpRowSet->aRow[0].lpProps, lpRowSet->aRow[0].cValues, PR_ENTRYID);

no_hierarchy:

	if (!lpProp) {
		// fallback to getprops on lpABContainer, actually a level too high, but it should work too.
		hr = HrGetOneProp(lpABContainer, 0, &propEntryID);
		if (hr != hrSuccess)
			goto exit;
		lpProp = propEntryID;
	}

	// make copy and return
	cbEntryID = lpProp->Value.bin.cb;
	MAPIAllocateBuffer(cbEntryID, (void**)&lpEntryID);
	memcpy(lpEntryID, lpProp->Value.bin.lpb, cbEntryID);

	*lpcbEntryID = cbEntryID;
	*lppEntryID = lpEntryID;

exit:
	if (lpRowSet)
		FreeProws(lpRowSet);

	if (lpTable)
		lpTable->Release();

	if (propEntryID)
		MAPIFreeBuffer(propEntryID);

	if (lpABContainer)
		lpABContainer->Release();

	TRACE_MAPILIB1(TRACE_RETURN, "M4LAddrBook::GetDefaultDir", "0x%08x", hr);
	return hr;
}

HRESULT M4LAddrBook::SetDefaultDir(ULONG cbEntryID, LPENTRYID lpEntryID) {
    TRACE_MAPILIB(TRACE_ENTRY, "M4LAddrBook::SetDefaultDir", "");
	TRACE_MAPILIB1(TRACE_RETURN, "M4LAddrBook::SetDefaultDir", "0x%08x", MAPI_E_NO_SUPPORT);
	return MAPI_E_NO_SUPPORT;
}

/** 
 * Returns the all hierarchy entries of the AB Root Container, see ResolveName()
 * Should return what's set with SetSearchPath().
 * Also should not return AB_SUBFOLDERS from the root container, but the subfolders.
 * 
 * @param[in] ulFlags MAPI_UNICODE
 * @param[out] lppSearchPath IABContainers EntryIDs of all known providers
 * 
 * @return MAPI Error code
 */
HRESULT M4LAddrBook::GetSearchPath(ULONG ulFlags, LPSRowSet* lppSearchPath) {
    TRACE_MAPILIB(TRACE_ENTRY, "M4LAddrBook::GetSearchPath", "");
	HRESULT hr = hrSuccess;
	LPSRowSet lpSearchPath = NULL;

	if (!m_lpSavedSearchPath) {
		hr = this->getDefaultSearchPath(ulFlags, &m_lpSavedSearchPath);
		if (hr != hrSuccess)
			goto exit;
	}

	hr = MAPIAllocateBuffer(CbNewSRowSet(m_lpSavedSearchPath->cRows), (void**)&lpSearchPath);
	if (hr != hrSuccess)
		goto exit;

	hr = Util::HrCopySRowSet(lpSearchPath, m_lpSavedSearchPath, NULL);
	if (hr != hrSuccess)
		goto exit;

	*lppSearchPath = lpSearchPath;
	lpSearchPath = NULL;

exit:
	if (lpSearchPath)
		FreeProws(lpSearchPath);

	TRACE_MAPILIB1(TRACE_RETURN, "M4LAddrBook::GetSearchPath", "0x%08x", hr);
	return hr;
}

HRESULT M4LAddrBook::SetSearchPath(ULONG ulFlags, LPSRowSet lpSearchPath) {
    TRACE_MAPILIB(TRACE_ENTRY, "M4LAddrBook::SetSearchPath", "");
	HRESULT hr = hrSuccess;

	if (m_lpSavedSearchPath) {
		FreeProws(m_lpSavedSearchPath);
		m_lpSavedSearchPath = NULL;
	}

	hr = MAPIAllocateBuffer(CbNewSRowSet(lpSearchPath->cRows), (void**)&m_lpSavedSearchPath);
	if (hr != hrSuccess)
		goto exit;

	hr = Util::HrCopySRowSet(m_lpSavedSearchPath, lpSearchPath, NULL);
	if (hr != hrSuccess)
		goto exit;

exit:
	if (hr != hrSuccess && m_lpSavedSearchPath) {
		FreeProws(m_lpSavedSearchPath);
		m_lpSavedSearchPath = NULL;
	}

	TRACE_MAPILIB1(TRACE_RETURN, "M4LAddrBook::SetSearchPath", "0x%08x", hr);
	return hr;
}

/**
 * Returns the EntryID of the Global Address Book container.
 *
 * @param[in]		ulFlags			Unused. (MAPI_UNICODE new?)
 * @param[in]		lpPropTagArray	List of properties to add in lpRecipList per recipient.
 * @param[in,out]	lpRecipList		List will be edited with requested properties from addressbook.
 *
 * @return	HRESULT
 * @retval	MAPI_E_INVALID_PARAMETER	Invalid input
 * @retval	MAPI_E_NOT_FOUND			Addressbook	is not available.
 */
HRESULT M4LAddrBook::PrepareRecips(ULONG ulFlags, LPSPropTagArray lpPropTagArray, LPADRLIST lpRecipList) {
	HRESULT hr = hrSuccess;
	IMailUser *lpMailUser = NULL;
	LPSPropValue lpProps = NULL;
	ULONG cValues = 0;
	ULONG ulType = 0;

    TRACE_MAPILIB2(TRACE_ENTRY, "M4LAddrBook::PrepareRecips", "%s %s", PropNameFromPropTagArray(lpPropTagArray).c_str(), RowSetToString((LPSRowSet)lpRecipList).c_str());

	//FIXME: lpPropTagArray can be NULL, this means that doesn't have extra properties to update only the 
	//       properties in the lpRecipList array.
	//       This function should merge properties which are in lpRecipList and lpPropTagArray, the requested
	//       properties are ordered first, followed by any additional properties that were already present for the entry.

	if (lpRecipList == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	for(unsigned int i=0; i<lpRecipList->cEntries; i++) {
		LPSPropValue lpEntryId = PpropFindProp(lpRecipList->aEntries[i].rgPropVals, lpRecipList->aEntries[i].cValues, PR_ENTRYID);

		if(lpEntryId == NULL)
			continue;

		hr = OpenEntry(lpEntryId->Value.bin.cb, (LPENTRYID)lpEntryId->Value.bin.lpb, &IID_IMailUser, 0, &ulType, (IUnknown **)&lpMailUser);
		if(hr != hrSuccess)
			goto exit;

		hr = lpMailUser->GetProps(lpPropTagArray, 0, &cValues, &lpProps);
		if(FAILED(hr))
			goto exit;

		hr = hrSuccess;

		if(lpRecipList->aEntries[i].rgPropVals)
			MAPIFreeBuffer(lpRecipList->aEntries[i].rgPropVals);

		MAPIAllocateBuffer(sizeof(SPropValue) * lpPropTagArray->cValues, (void **)&lpRecipList->aEntries[i].rgPropVals);
		memset(lpRecipList->aEntries[i].rgPropVals, 0, sizeof(SPropValue) * lpPropTagArray->cValues);
		lpRecipList->aEntries[i].cValues = lpPropTagArray->cValues;

		for(unsigned int j=0; j<lpPropTagArray->cValues;j++) {
			LPSPropValue lpProp = PpropFindProp(lpProps, cValues, lpPropTagArray->aulPropTag[j]);

			if(lpProp == NULL) {
				lpRecipList->aEntries[i].rgPropVals[j].ulPropTag = PROP_TAG(PT_ERROR, PROP_ID(lpPropTagArray->aulPropTag[j]));
				lpRecipList->aEntries[i].rgPropVals[j].Value.err = MAPI_E_NOT_FOUND;
			} else {
				hr = Util::HrCopyProperty(&lpRecipList->aEntries[i].rgPropVals[j], lpProp, lpRecipList->aEntries[i].rgPropVals);
				if(hr != hrSuccess)
					goto exit;
			}
		}

		MAPIFreeBuffer(lpProps);
		lpProps = NULL;
	}

exit:
	if(lpProps)
		MAPIFreeBuffer(lpProps);
	if(lpMailUser)
		lpMailUser->Release();

	TRACE_MAPILIB2(TRACE_RETURN, "M4LAddrBook::PrepareRecips", "%s: %s", GetMAPIErrorDescription(hr).c_str(), RowSetToString((LPSRowSet)lpRecipList).c_str());
	return hr;
}


    // imapiprop passthru
// maybe not all functions should be passed though?
HRESULT M4LAddrBook::GetLastError(HRESULT hResult, ULONG ulFlags, LPMAPIERROR* lppMAPIError) {
    TRACE_MAPILIB(TRACE_ENTRY, "M4LAddrBook::GetLastError", "");
	HRESULT hr = M4LMAPIProp::GetLastError(hResult, ulFlags, lppMAPIError);
	TRACE_MAPILIB1(TRACE_RETURN, "M4LAddrBook::GetLastError", "0x%08x", hr);
	return hr;
}

HRESULT M4LAddrBook::SaveChanges(ULONG ulFlags) {
    TRACE_MAPILIB(TRACE_ENTRY, "M4LAddrBook::SaveChanges", "");
	HRESULT hr = M4LMAPIProp::SaveChanges(ulFlags);
	TRACE_MAPILIB1(TRACE_RETURN, "M4LAddrBook::SaveChanges", "0x%08x", hr);
	return hr;
}

HRESULT M4LAddrBook::GetProps(LPSPropTagArray lpPropTagArray, ULONG ulFlags, ULONG* lpcValues, LPSPropValue* lppPropArray) {
    TRACE_MAPILIB(TRACE_ENTRY, "M4LAddrBook::GetProps", "");
	HRESULT hr = M4LMAPIProp::GetProps(lpPropTagArray, ulFlags, lpcValues, lppPropArray);
	TRACE_MAPILIB1(TRACE_RETURN, "M4LAddrBook::GetProps", "0x%08x", hr);
	return hr;
}

HRESULT M4LAddrBook::GetPropList(ULONG ulFlags, LPSPropTagArray* lppPropTagArray) {
    TRACE_MAPILIB(TRACE_ENTRY, "M4LAddrBook::GetPropList", "");
	HRESULT hr = M4LMAPIProp::GetPropList(ulFlags, lppPropTagArray);
	TRACE_MAPILIB1(TRACE_RETURN, "M4LAddrBook::GetPropList", "0x%08x", hr);
	return hr;
}

HRESULT M4LAddrBook::OpenProperty(ULONG ulPropTag, LPCIID lpiid, ULONG ulInterfaceOptions, ULONG ulFlags, LPUNKNOWN* lppUnk) {
    TRACE_MAPILIB(TRACE_ENTRY, "M4LAddrBook::OpenProperty", "");
	HRESULT hr = M4LMAPIProp::OpenProperty(ulPropTag, lpiid, ulInterfaceOptions, ulFlags, lppUnk);
	TRACE_MAPILIB1(TRACE_RETURN, "M4LAddrBook::OpenProperty", "0x%08x", hr);
	return hr;
}

HRESULT M4LAddrBook::SetProps(ULONG cValues, LPSPropValue lpPropArray, LPSPropProblemArray* lppProblems) {
    TRACE_MAPILIB(TRACE_ENTRY, "M4LAddrBook::SetProps", "");
	HRESULT hr = M4LMAPIProp::SetProps(cValues, lpPropArray, lppProblems);
	TRACE_MAPILIB1(TRACE_RETURN, "M4LAddrBook::SetProps", "0x%08x", hr);
	return hr;
}

HRESULT M4LAddrBook::DeleteProps(LPSPropTagArray lpPropTagArray, LPSPropProblemArray* lppProblems) {
    TRACE_MAPILIB(TRACE_ENTRY, "M4LAddrBook::DeleteProps", "");
	HRESULT hr = M4LMAPIProp::DeleteProps(lpPropTagArray, lppProblems);
	TRACE_MAPILIB1(TRACE_RETURN, "M4LAddrBook::DeleteProps", "0x%08x", hr);
	return hr;
}

HRESULT M4LAddrBook::CopyTo(ULONG ciidExclude, LPCIID rgiidExclude, LPSPropTagArray lpExcludeProps, ULONG ulUIParam,
							LPMAPIPROGRESS lpProgress, LPCIID lpInterface, LPVOID lpDestObj, ULONG ulFlags,
							LPSPropProblemArray* lppProblems) {
    TRACE_MAPILIB(TRACE_ENTRY, "M4LAddrBook::CopyTo", "");
	HRESULT hr = M4LMAPIProp::CopyTo(ciidExclude, rgiidExclude, lpExcludeProps, ulUIParam,
									 lpProgress, lpInterface, lpDestObj, ulFlags, lppProblems);
	TRACE_MAPILIB1(TRACE_RETURN, "M4LAddrBook::CopyTo", "0x%08x", hr);
	return hr;
}

HRESULT M4LAddrBook::CopyProps(LPSPropTagArray lpIncludeProps, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, LPCIID lpInterface,
							   LPVOID lpDestObj, ULONG ulFlags, LPSPropProblemArray* lppProblems) {
    TRACE_MAPILIB(TRACE_ENTRY, "M4LAddrBook::CopyProps", "");
	HRESULT hr = M4LMAPIProp::CopyProps(lpIncludeProps, ulUIParam, lpProgress, lpInterface,
										lpDestObj, ulFlags, lppProblems);
	TRACE_MAPILIB1(TRACE_RETURN, "M4LAddrBook::CopyProps", "0x%08x", hr);
	return hr;
}

HRESULT M4LAddrBook::GetNamesFromIDs(LPSPropTagArray* lppPropTags, LPGUID lpPropSetGuid, ULONG ulFlags, ULONG* lpcPropNames,
									 LPMAPINAMEID** lpppPropNames) {
    TRACE_MAPILIB(TRACE_ENTRY, "M4LAddrBook::GetNamesFromIDs", "");
	HRESULT hr = M4LMAPIProp::GetNamesFromIDs(lppPropTags, lpPropSetGuid, ulFlags, lpcPropNames, lpppPropNames);
	TRACE_MAPILIB1(TRACE_RETURN, "M4LAddrBook::GetNamesFromIDs", "0x%08x", hr);
	return hr;
}

HRESULT M4LAddrBook::GetIDsFromNames(ULONG cPropNames, LPMAPINAMEID* lppPropNames, ULONG ulFlags, LPSPropTagArray* lppPropTags) {
    TRACE_MAPILIB(TRACE_ENTRY, "M4LAddrBook::GetIDsFromNames", "");
	HRESULT hr = M4LMAPIProp::GetIDsFromNames(cPropNames, lppPropNames, ulFlags, lppPropTags);
	TRACE_MAPILIB1(TRACE_RETURN, "M4LAddrBook::GetIDsFromNames", "0x%08x", hr);
	return hr;
}


    // iunknown passthru
ULONG M4LAddrBook::AddRef() {
    TRACE_MAPILIB(TRACE_ENTRY, "M4LAddrBook::AddRef", "");
	ULONG ulRef = M4LUnknown::AddRef();
	TRACE_MAPILIB1(TRACE_RETURN, "M4LAddrBook::AddRef", "%d", ulRef);
	return ulRef;
}

ULONG M4LAddrBook::Release() {
    TRACE_MAPILIB(TRACE_ENTRY, "M4LAddrBook::Release", "");
	ULONG ulRef = M4LUnknown::Release();
	TRACE_MAPILIB1(TRACE_RETURN, "M4LAddrBook::Release", "%d", ulRef);
	return ulRef;
}

HRESULT M4LAddrBook::QueryInterface(REFIID refiid, void **lpvoid) {
    TRACE_MAPILIB(TRACE_ENTRY, "M4LAddrBook::QueryInterface", "");
	HRESULT hr = hrSuccess;

	if ((refiid == IID_IAddrBook) || (refiid == IID_IMAPIProp) || (refiid == IID_IUnknown)) {
		AddRef();
		*lpvoid = (IAddrBook *)this;
	} else
		hr = MAPI_E_INTERFACE_NOT_SUPPORTED;

	TRACE_MAPILIB1(TRACE_RETURN, "M4LAddrBook::QueryInterface", "0x%08x", hr);
	return hr;
}



// ---------------------------------------

// Memory map list
map<void*, list<void*>* > _memlist;
pthread_mutex_t _memlist_lock;

#define _MAPI_MEM_DEBUG 0
#define _MAPI_MEM_MORE_DEBUG 0

/**
 * Internal allocation function. Uses C++ new allocator, and catches
 * exceptions to convert to MAPI error codes.
 *
 * @param[in]	cbSize		Size of buffer to allocate
 * @param[out]	lppBuffer	Allocated buffer.
 * @return		SCODE
 * @retval		0x80040001	Allocation error
 * @fixme		Why is the return value not the mapi error MAPI_E_NOT_ENOUGH_MEMORY?
 *				I don't think Mapi programs like an error 0x80040001.
 */
SCODE MAPIAllocate(ULONG cbSize, LPVOID* lppBuffer)
{
	char *buffer;
	try {
		buffer = new char[cbSize];
	} catch (...) {
		return MAKE_MAPI_E(1);
	}
	if (!buffer)
		return MAKE_MAPI_E(1);

	*lppBuffer = (void*)buffer;
	return S_OK;
}

/**
 * Allocate a new buffer. Must be freed with MAPIFreeBuffer.
 *
 * @param[in]	cbSize		Size of buffer to allocate
 * @param[out]	lppBuffer	Allocated buffer.
 * @return		SCODE
 * @retval		MAPI_E_INVALID_PARAMETER	Invalid input parameters
 * @retval		MAPI_E_NOT_ENOUGH_MEMORY
 */
SCODE __stdcall MAPIAllocateBuffer(ULONG cbSize, LPVOID* lppBuffer)
{
	HRESULT hr = hrSuccess;

	if (lppBuffer == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	hr = MAPIAllocate(cbSize, lppBuffer);
	if (hr != hrSuccess)
		goto exit;

	pthread_mutex_lock(&_memlist_lock);
	_memlist.insert(map<void*, list<void *>* >::value_type(*lppBuffer, new list<void *>()));
	pthread_mutex_unlock(&_memlist_lock);

#if _MAPI_MEM_DEBUG
		fprintf(stderr, "New buffer: %p\n", buffer);
#endif

exit:
	return hr;
}

/**
 * Allocate a new buffer and associate it with lpObject.
 *
 * @param[in]	cbSize		Size of buffer to allocate
 * @param[in]	lpObject	Pointer from MAPIAllocate to associate this allocation with.
 * @param[out]	lppBuffer	Allocated buffer.
 * @return		SCODE
 * @retval		MAPI_E_INVALID_PARAMETER	Invalid input parameters
 * @retval		MAPI_E_NOT_ENOUGH_MEMORY
 */
SCODE __stdcall MAPIAllocateMore(ULONG cbSize, LPVOID lpObject, LPVOID* lppBuffer) {
	HRESULT hr = hrSuccess;
	map<void*, list<void*>* >::iterator mlptr;

	if (lppBuffer == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	if (!lpObject)
		return MAPIAllocateBuffer(cbSize, lppBuffer);

	hr = MAPIAllocate(cbSize, lppBuffer);
	if (hr != hrSuccess)
		goto exit;

	pthread_mutex_lock(&_memlist_lock);

#if _MAPI_MEM_MORE_DEBUG
	for (mlptr = _memlist.begin(); mlptr != _memlist.end(); mlptr++) {
		list<void *>::iterator lp;
		for (lp = mlptr->second->begin(); lp != mlptr->second->end(); lp++) {
			if ((*lp) == lpObject) {
				fprintf(stderr, "AllocateMore on an AllocateMore buffer!\n");
				break;
			}
		}
	}
#endif

	mlptr = _memlist.find(lpObject);
	if (mlptr == _memlist.end()) {
		/* lpObject was not allocatated with MAPIAllocateBuffer() */
		ASSERT(FALSE);

		/*
		 * Workaround: add lpObject and lppBuffer to the map,
		 * but be aware that we will memleak because MAPIFreeBuffer()
		 * will not be called on lpObject.
		 */
		list<void *>* members = new list<void *>;
		members->push_back(*lppBuffer);
		_memlist.insert(pair<void *, list<void *>* >(lpObject, members));
	} else {
		/* add to list */
		mlptr->second->push_back(*lppBuffer);
	}

	pthread_mutex_unlock(&_memlist_lock);

#if _MAPI_MEM_DEBUG
	fprintf(stderr, "Extra buffer: %p on %p\n", *lppBuffer, lpObject);
#endif

exit:
	return hr;
}

/**
 * Free a buffer allocated with MAPIAllocate. All buffers associated
 * with lpBuffer will be freed too.
 *
 * @param[in]	lpBuffer	Pointer to be freed.
 * @return		ULONG		Always 0 in Linux.
 */
ULONG __stdcall MAPIFreeBuffer(LPVOID lpBuffer) {
	list<void*>::iterator i;
	map<void*, list<void*>* >::iterator mlptr;

	/* Well it could happen, especially according to the MSDN.. */
	if (!lpBuffer)
		return S_OK;

	pthread_mutex_lock(&_memlist_lock);

#if _MAPI_MEM_DEBUG
	fprintf(stderr, "Freeing: %p\n", lpBuffer);
#endif

	mlptr = _memlist.find(lpBuffer);
	if (mlptr != _memlist.end()) {

		for (i = mlptr->second->begin(); i != mlptr->second->end(); i++) {
#if _MAPI_MEM_DEBUG
			fprintf(stderr, "  Freeing: %p\n", (*i));
#endif
 			delete [] (char*)(*i);
		}

		// delete list
		delete mlptr->second;
		delete [] (char *)mlptr->first;

		// remove map entry
		_memlist.erase(mlptr);
	} else {
		// item was not allocated by  MAPIAllocateBuffer
		ASSERT(FALSE);
	}

	pthread_mutex_unlock(&_memlist_lock);
	return 0;
 }

// ---
// Entry
// ---

LPPROFADMIN localProfileAdmin = NULL;

/**
 * Returns a pointer to the IProfAdmin interface. MAPIInitialize must been called previously.
 *
 * @param[in]	ulFlags			Unused in Linux.
 * @param[out]	lppProfAdmin	Return value of function
 * @return		HRESULT
 * @retval		MAPI_E_CALL_FAILED	MAPIInitialize not called previously.
 * @retval		MAPI_E_INVALID_PARAMETER	No lppProfAdmin return parameter given.
 */
HRESULT __stdcall MAPIAdminProfiles(ULONG ulFlags, LPPROFADMIN *lppProfAdmin) {
	TRACE_MAPILIB1(TRACE_ENTRY, "MAPIAdminProfiles", "flags=0x%08d", ulFlags);
	HRESULT hr = hrSuccess;
	if (!localProfileAdmin) {
		hr = MAPI_E_CALL_FAILED;
		goto exit;
	}

	if (!lppProfAdmin) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	hr = localProfileAdmin->QueryInterface(IID_IProfAdmin, (void**)lppProfAdmin);

exit:
	TRACE_MAPILIB1(TRACE_RETURN, "MAPIAdminProfiles", "0x%08x", hr);
	return hr;
}

/**
 * Login on a previously created profile.
 *
 * @param[in]	ulUIParam	Unused in Linux.
 * @param[in]	lpszProfileName	us-ascii profilename. No limit to this string in Linux (Win32 is 64 characters)
 * @param[in]	lpszPassword	us-ascii password of the profile. Not the password of the user defined in the profile. Mostly unused.
 * @param[in]	ulFlags	the following flags are used in Linux:
 *				MAPI_UNICODE : treat lpszProfileName and lpszPassword (unused) as wchar_t strings
 * @param[out]	lppSession	returns the MAPISession object on hrSuccess
 * @return		HRESULT
 * @retval		MAPI_E_CALL_FAILED	MAPIInitialize not previously called
 * @retval		MAPI_E_INVALID_PARAMETER	No profilename or lppSession return pointer given.
 * @retval		MAPI_E_NOT_ENOUGH_MEMORY
 */
HRESULT __stdcall MAPILogonEx(ULONG ulUIParam, LPTSTR lpszProfileName, LPTSTR lpszPassword, ULONG ulFlags, LPMAPISESSION* lppSession) {
	HRESULT hr = hrSuccess;
	LPMAPISESSION ms = NULL;
	LPSERVICEADMIN sa = NULL;
	string strProfname;

	TRACE_MAPILIB1(TRACE_ENTRY, "MAPILogonEx", "%s", (char*)lpszProfileName);

	if (!lpszProfileName || !lppSession) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	if (!localProfileAdmin) {
		hr = MAPI_E_CALL_FAILED;
		goto exit;
	}

	if(ulFlags & MAPI_UNICODE) {
		// since a profilename can only be us-ascii, convert
		strProfname = convert_to<string>((WCHAR *)lpszProfileName);
	} else {
		strProfname = (char*)lpszProfileName;
	}

	hr = localProfileAdmin->AdminServices((LPTSTR)strProfname.c_str(), lpszPassword, ulUIParam, ulFlags & ~MAPI_UNICODE, &sa);
	if (hr != hrSuccess)
		goto exit;

	ms = new M4LMAPISession(lpszProfileName,(M4LMsgServiceAdmin *)sa);
	if (!ms) {
		hr = MAPI_E_NOT_ENOUGH_MEMORY;
		goto exit;
	}

	hr = ms->QueryInterface(IID_IMAPISession, (void**)lppSession);

exit:
	if (sa)
		sa->Release();

	TRACE_MAPILIB1(TRACE_RETURN, "MAPILogonEx", "0x%08x", hr);
	return hr;
}

/*
 * Small (non-threadsafe protection against multiple MAPIInitialize/MAPIUnitialize calls.
 * Some (bad behaving) MAPI clients (i.e. CalHelper.exe) might call MAPIInitialize/MAPIUnitialize
 * multiple times, obviously this is very bad behavior, but it shouldn't hurt to at least
 * builtin some protection against this.
 * _MAPIInitializeCount simply counts the number of times MAPIInitialize is called, and will
 * not cleanup anything in MAPIUnitialize until the counter is back to 0.
 */
int _MAPIInitializeCount = 0;
pthread_mutex_t g_MAPILock = PTHREAD_MUTEX_INITIALIZER;

/**
 * MAPIInitialize is the first function called.
 *
 * In Linux, this will already open the zarafaclient.so file, and will retrieve
 * the entry point function pointers. If these are not present, the function will fail.
 *
 * @param[in] lpMapiInit
 *			Optional pointer to MAPIINIT struct. Unused in Linux.
 * @return	HRESULT
 * @retval	hrSuccess
 * @retval	MAPI_E_CALL_FAILED	Unable to use zarafaclient.so
 * @retval	MAPI_E_NOT_ENOUGH_MEMORY Memory allocation failed
 */
HRESULT __stdcall MAPIInitialize(LPVOID lpMapiInit) {
	TRACE_MAPILIB(TRACE_ENTRY, "MAPIInitialize", "");
	HRESULT hr = hrSuccess;

	pthread_mutex_lock(&g_MAPILock);

	if (_MAPIInitializeCount++) {
		assert(localProfileAdmin);
		localProfileAdmin->AddRef();
		goto exit;
	}

	/* Allocate special M4L services (logger, config, ...) */
	hr = HrCreateM4LServices();
	if (hr != hrSuccess)
		goto exit;

	// also static init?
	pthread_mutex_init(&_memlist_lock, NULL);

	// Loads the mapisvc.inf, and finds all providers and entry point functions
	m4l_lpMAPISVC = new MAPISVC();
	hr = m4l_lpMAPISVC->Init();
	if (hr != hrSuccess)
		goto exit;

	if (!localProfileAdmin) {
		localProfileAdmin = new M4LProfAdmin;
		if (!localProfileAdmin) {
			hr = MAPI_E_NOT_ENOUGH_MEMORY;
			goto exit;
		}
		localProfileAdmin->AddRef();

	}

exit:
	pthread_mutex_unlock(&g_MAPILock);

	TRACE_MAPILIB2(TRACE_RETURN, "MAPIInitialize", "%d, 0x%08x", _MAPIInitializeCount, hr);
	return hr;
}

/**
 * Last function of your MAPI program.  
 *
 * In Linux, this will unload the zarafaclient.so library. Any
 * object from that library you still * have will be unusable,
 * and will make your program crash when used.
 */
void __stdcall MAPIUninitialize(void) {
	TRACE_MAPILIB(TRACE_ENTRY, "MAPIUninitialize", "");

	pthread_mutex_lock(&g_MAPILock);

	/* MAPIInitialize always AddRefs localProfileAdmin */
	if (localProfileAdmin)
		localProfileAdmin->Release();

	/* Only clean everything up when this is the last MAPIUnitialize call. */
	if ((--_MAPIInitializeCount) == 0) {
		delete m4l_lpMAPISVC;

		localProfileAdmin = NULL;

		HrFreeM4LServices();

		pthread_mutex_destroy(&_memlist_lock);

	}

	pthread_mutex_unlock(&g_MAPILock);

	TRACE_MAPILIB2(TRACE_RETURN, "MAPIUninitialize", "%d, 0x%08x", _MAPIInitializeCount, hrSuccess);
}
