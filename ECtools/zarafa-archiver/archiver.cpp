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
#include "archiver.h"
#include "archiver-session.h"
#include "archivectrl.h"
#include "archivemanage.h"
#include "archivestatecollector.h"
#include "archivestateupdater.h"

#include "mapix.h"

#include "ECConfig.h"
#include "ECLogger.h"

#include <string>
using namespace std;

class AutoMAPI {
public:
	AutoMAPI() : m_bInitialized(false) {}
	~AutoMAPI() {
		if (m_bInitialized)
			MAPIUninitialize();
	}
	
	HRESULT Initialize(MAPIINIT_0 *lpMapiInit) {
		HRESULT hr = hrSuccess;

		if (!m_bInitialized) {
			hr = MAPIInitialize(lpMapiInit);		
			if (hr == hrSuccess)
				m_bInitialized = true;
		}
			
		return hr;
	}

	bool IsInitialized() const {
		return m_bInitialized;
	}

private:
	bool m_bInitialized;
};

class ArchiverImpl : public Archiver
{
public:
	ArchiverImpl();
	~ArchiverImpl();

	eResult Init(const char *lpszAppName, const char *lpszConfig, const configsetting_t *lpExtraSettings, unsigned int ulFlags);

	eResult GetControl(ArchiveControlPtr *lpptrControl, bool bForceCleanup);
	eResult GetManage(const TCHAR *lpszUser, ArchiveManagePtr *lpptrManage);
	eResult AutoAttach(unsigned int ulFlags);

	ECConfig* GetConfig() const;
	ECLogger* GetLogger(eLogType which) const;

private:
	configsetting_t* ConcatSettings(const configsetting_t *lpSettings1, const configsetting_t *lpSettings2);
	unsigned CountSettings(const configsetting_t *lpSettings);

private:
	AutoMAPI		m_MAPI;
	ECConfig		*m_lpsConfig;
	ECLogger		*m_lpLogger;
    ECLogger        *m_lpLogLogger; // Logs only to the log specified in the config
	SessionPtr 		m_ptrSession;
	configsetting_t	*m_lpDefaults;
};



const char* Archiver::GetConfigPath()
{
	static string s_strConfigPath;

	if (s_strConfigPath.empty()) {
		const char *lpszConfigPath = getenv("ZARAFA_ARCHIVER_CONF");
		if (!lpszConfigPath || lpszConfigPath[0] == '\0')
			s_strConfigPath = ECConfig::GetDefaultPath("archiver.cfg");
		else
			s_strConfigPath = lpszConfigPath;
	}

	return s_strConfigPath.c_str();
}

const configsetting_t* Archiver::GetConfigDefaults()
{
	static const configsetting_t s_lpDefaults[] = {
		// Connect settings
		{ "server_socket",	"" },
		{ "sslkey_file",	"" },
		{ "sslkey_pass",	"", CONFIGSETTING_EXACT },
		
		// Archive settings
		{ "archive_enable",	"yes" },
		{ "archive_after", 	"30" },

		{ "stub_enable",	"no" },
		{ "stub_unread",	"no" },
		{ "stub_after", 	"0" },

		{ "delete_enable",	"no" },
		{ "delete_unread",	"no" },
		{ "delete_after", 	"0" },
		
		{ "purge_enable",	"no" },
		{ "purge_after", 	"2555" },

		{ "track_history",	"no" },
		{ "cleanup_action",	"store" },
		{ "cleanup_follow_purge_after",	"no" },
		{ "enable_auto_attach",	"no" },
		{ "auto_attach_writable",	"yes" },

		// Log options
		{ "log_method",		"file" },
		{ "log_file",		"-" },
		{ "log_level",		"2", CONFIGSETTING_RELOADABLE },
		{ "log_timestamp",	"1" },

		{ "mysql_host",		"localhost" },
		{ "mysql_port",		"3306" },
		{ "mysql_user",		"root" },
		{ "mysql_password",	"",	CONFIGSETTING_EXACT },
		{ "mysql_database",	"zarafa-archiver" },
		{ "mysql_socket",	"" },

		{ NULL, NULL },
	};

	return s_lpDefaults;
}

eResult Archiver::Create(auto_ptr_type *lpptrArchiver)
{
	eResult r = Success;
	auto_ptr_type ptrArchiver;

	if (lpptrArchiver == NULL) {
		r = InvalidParameter;
		goto exit;
	}

	try {
		ptrArchiver.reset(new ArchiverImpl());
	} catch (std::bad_alloc &) {
		r = OutOfMemory;
		goto exit;
	}

	*lpptrArchiver = ptrArchiver;

exit:
	return r;
}

HRESULT Archiver::CreateManage(LPMAPISESSION lpSession, ECLogger *lpLogger, const TCHAR *lpszUser, ArchiveManagePtr *lpptrManage)
{
	HRESULT hr = hrSuccess;
	ECConfig *lpsConfig = NULL; 
	SessionPtr ptrArchiverSession;

	lpsConfig = ECConfig::Create(GetConfigDefaults());
	if (!lpsConfig->LoadSettings(GetConfigPath())) {
		// Just log warnings and errors and continue with default.
		if (lpLogger)
			LogConfigErrors(lpsConfig, lpLogger);
	}

	hr = Session::Create(MAPISessionPtr(lpSession, true), lpsConfig, lpLogger, &ptrArchiverSession);
	if (hr != hrSuccess)
		goto exit;

	hr = ArchiveManageImpl::Create(ptrArchiverSession, NULL, lpszUser, lpLogger, lpptrManage);

exit:
	delete lpsConfig;
	return hr;
}


ArchiverImpl::ArchiverImpl()
: m_lpsConfig(NULL)
, m_lpLogger(NULL)
, m_lpLogLogger(NULL)
, m_lpDefaults(NULL)
{
}

ArchiverImpl::~ArchiverImpl()
{
	if (m_lpLogger)
		m_lpLogger->Release();

	if (m_lpLogLogger)
		m_lpLogLogger->Release();

	delete m_lpsConfig;
	delete[] m_lpDefaults;
}

eResult ArchiverImpl::Init(const char *lpszAppName, const char *lpszConfig, const configsetting_t *lpExtraSettings, unsigned int ulFlags)
{
	eResult r = Success;

	MAPIINIT_0 sMapiInit = {MAPI_INIT_VERSION, MAPI_MULTITHREAD_NOTIFICATIONS};

	if (lpExtraSettings == NULL)
		m_lpsConfig = ECConfig::Create(Archiver::GetConfigDefaults());

	else {
		m_lpDefaults = ConcatSettings(Archiver::GetConfigDefaults(), lpExtraSettings);
		m_lpsConfig = ECConfig::Create(m_lpDefaults);
	}

	if (!m_lpsConfig->LoadSettings(lpszConfig) && (ulFlags & RequireConfig)) {
		r = FileNotFound;
		goto exit;
	}
	if (!m_lpsConfig->LoadSettings(lpszConfig)) {
		if ((ulFlags & RequireConfig)) {
			r = FileNotFound;
			goto exit;
		}
	} else if (m_lpsConfig->HasErrors()) {
		if (!(ulFlags & InhibitErrorLogging)) {
			ECLogger *lpLogger = new ECLogger_File(EC_LOGLEVEL_FATAL, 0, "-");
			LogConfigErrors(m_lpsConfig, lpLogger);
			lpLogger->Release();
		}
		
		r = InvalidConfig;
		goto exit;
	}

	m_lpLogLogger = CreateLogger(m_lpsConfig, (char*)lpszAppName, "");
	if (ulFlags & InhibitErrorLogging) {
		// We need to check if we're logging to stderr. If so we'll replace
		// the logger with a NULL logger.
		ECLogger_File *lpFileLogger = dynamic_cast<ECLogger_File*>(m_lpLogLogger);
		if (lpFileLogger && lpFileLogger->IsStdErr()) {
			m_lpLogLogger->Release();
			m_lpLogLogger = new ECLogger_Null();
		}
		m_lpLogger = m_lpLogLogger;
		m_lpLogger->AddRef();
	} else if (ulFlags & AttachStdErr) {
		// We need to check if the current logger isn't logging to the console
		// as that would give duplicate messages
		ECLogger_File *lpFileLogger = dynamic_cast<ECLogger_File*>(m_lpLogLogger);
		if (lpFileLogger == NULL || !lpFileLogger->IsStdErr()) {
			ECLogger_Tee *lpTeeLogger = new ECLogger_Tee();
			lpTeeLogger->AddLogger(m_lpLogLogger);

			ECLogger_File *lpConsoleLogger = new ECLogger_File(EC_LOGLEVEL_ERROR, 0, "-", false);
			lpTeeLogger->AddLogger(lpConsoleLogger);
			lpConsoleLogger->Release();

			m_lpLogger = lpTeeLogger;
		} else {
			m_lpLogger = m_lpLogLogger;
			m_lpLogger->AddRef();
		}
	} else {
		m_lpLogger = m_lpLogLogger;
		m_lpLogger->AddRef();
	}

	if (m_lpsConfig->HasWarnings())
		LogConfigErrors(m_lpsConfig, m_lpLogger);

	if (m_MAPI.Initialize(&sMapiInit) != hrSuccess) {
		r = Failure;
		goto exit;
	}
	
	if (Session::Create(m_lpsConfig, m_lpLogger, &m_ptrSession) != hrSuccess) {
		r = Failure;
		goto exit;
	}

exit:
	return r;
}

eResult ArchiverImpl::GetControl(ArchiveControlPtr *lpptrControl, bool bForceCleanup)
{
	if (!m_MAPI.IsInitialized())
		return Uninitialized;
		
	return MAPIErrorToArchiveError(ArchiveControlImpl::Create(m_ptrSession, m_lpsConfig, m_lpLogger, bForceCleanup, lpptrControl));
}

eResult ArchiverImpl::GetManage(const TCHAR *lpszUser, ArchiveManagePtr *lpptrManage)
{
	if (!m_MAPI.IsInitialized())
		return Uninitialized;
		
	return MAPIErrorToArchiveError(ArchiveManageImpl::Create(m_ptrSession, m_lpsConfig, lpszUser, m_lpLogger, lpptrManage));
}

eResult ArchiverImpl::AutoAttach(unsigned int ulFlags)
{
	HRESULT hr = hrSuccess;
	ArchiveStateCollectorPtr ptrArchiveStateCollector;
	ArchiveStateUpdaterPtr ptrArchiveStateUpdater;

	if (ulFlags != ArchiveManage::Writable && ulFlags != ArchiveManage::ReadOnly && ulFlags != 0) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	hr = ArchiveStateCollector::Create(m_ptrSession, m_lpLogger, &ptrArchiveStateCollector);
	if (hr != hrSuccess)
		goto exit;

	hr = ptrArchiveStateCollector->GetArchiveStateUpdater(&ptrArchiveStateUpdater);
	if (hr != hrSuccess)
		goto exit;

	if (ulFlags == 0) {
		if (parseBool(m_lpsConfig->GetSetting("auto_attach_writable")))
			ulFlags = ArchiveManage::Writable;
		else
			ulFlags = ArchiveManage::ReadOnly;
	}

	hr = ptrArchiveStateUpdater->UpdateAll(ulFlags);

exit:
	return MAPIErrorToArchiveError(hr);
}

ECConfig* ArchiverImpl::GetConfig() const
{
	return m_lpsConfig;
}

ECLogger* ArchiverImpl::GetLogger(eLogType which) const
{
	switch (which) {
		case DefaultLog: return m_lpLogger;
		case LogOnly: return m_lpLogLogger;
	}
}

configsetting_t* ArchiverImpl::ConcatSettings(const configsetting_t *lpSettings1, const configsetting_t *lpSettings2)
{
	configsetting_t *lpMergedSettings = NULL;
	unsigned ulSettings = 0;
	unsigned ulIndex = 0;

	ulSettings = CountSettings(lpSettings1) + CountSettings(lpSettings2);
	lpMergedSettings = new configsetting_t[ulSettings + 1];

	while (lpSettings1->szName != NULL)
		lpMergedSettings[ulIndex++] = *lpSettings1++;
	while (lpSettings2->szName != NULL)
		lpMergedSettings[ulIndex++] = *lpSettings2++;
	memset(&lpMergedSettings[ulIndex], 0, sizeof(lpMergedSettings[ulIndex]));

	return lpMergedSettings;
}

unsigned ArchiverImpl::CountSettings(const configsetting_t *lpSettings)
{
	unsigned ulSettings = 0;

	while ((lpSettings++)->szName != NULL)
		ulSettings++;

	return ulSettings;
}
