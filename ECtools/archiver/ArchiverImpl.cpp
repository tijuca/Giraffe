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

/* ArchiverImpl.cpp
 * Definition of class ArchiverImpl
 */
#include <kopano/platform.h>
#include <kopano/automapi.hpp>
#include <kopano/memory.hpp>
#include "ArchiverImpl.h"
#include "ArchiveControlImpl.h"
#include "ArchiveManageImpl.h"
#include "ArchiveStateCollector.h"
#include "ArchiveStateUpdater.h"
#include "ArchiverSession.h"
#include <kopano/ECConfig.h>

namespace KC {

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
	MAPIINIT_0 sMapiInit = {MAPI_INIT_VERSION, MAPI_MULTITHREAD_NOTIFICATIONS};

	if (lpExtraSettings == NULL)
		m_lpsConfig = ECConfig::Create(Archiver::GetConfigDefaults());

	else {
		m_lpDefaults = ConcatSettings(Archiver::GetConfigDefaults(), lpExtraSettings);
		m_lpsConfig = ECConfig::Create(m_lpDefaults);
	}

	if (!m_lpsConfig->LoadSettings(lpszConfig) && (ulFlags & RequireConfig))
		return FileNotFound;
	if (!m_lpsConfig->LoadSettings(lpszConfig)) {
		if ((ulFlags & RequireConfig))
			return FileNotFound;
	} else if (m_lpsConfig->HasErrors()) {
		if (!(ulFlags & InhibitErrorLogging)) {
			KCHL::object_ptr<ECLogger> lpLogger(new ECLogger_File(EC_LOGLEVEL_FATAL, 0, "-", false));
			ec_log_set(lpLogger);
			LogConfigErrors(m_lpsConfig);
		}

		return InvalidConfig;
	}

	m_lpLogLogger = CreateLogger(m_lpsConfig, (char*)lpszAppName, "");
	if (ulFlags & InhibitErrorLogging) {
		// We need to check if we're logging to stderr. If so we'll replace
		// the logger with a NULL logger.
		auto lpFileLogger = dynamic_cast<ECLogger_File *>(m_lpLogLogger);
		if (lpFileLogger && lpFileLogger->IsStdErr()) {
			m_lpLogLogger->Release();
			m_lpLogLogger = new ECLogger_Null();
		}
		if (m_lpLogger != NULL)
			m_lpLogger->Release();
		m_lpLogger = m_lpLogLogger;
		m_lpLogger->AddRef();
	} else if (ulFlags & AttachStdErr) {
		// We need to check if the current logger isn't logging to the console
		// as that would give duplicate messages
		auto lpFileLogger = dynamic_cast<ECLogger_File *>(m_lpLogLogger);
		if (lpFileLogger == NULL || !lpFileLogger->IsStdErr()) {
			auto lpTeeLogger = new ECLogger_Tee();
			lpTeeLogger->AddLogger(m_lpLogLogger);

			KCHL::object_ptr<ECLogger_File> lpConsoleLogger(new ECLogger_File(EC_LOGLEVEL_ERROR, 0, "-", false));
			lpTeeLogger->AddLogger(lpConsoleLogger);
			m_lpLogger = lpTeeLogger;
		} else {
			m_lpLogger = m_lpLogLogger;
			m_lpLogger->AddRef();
		}
	} else {
		m_lpLogger = m_lpLogLogger;
		m_lpLogger->AddRef();
	}

	ec_log_set(m_lpLogger);
	if (m_lpsConfig->HasWarnings())
		LogConfigErrors(m_lpsConfig);

	if (m_MAPI.Initialize(&sMapiInit) != hrSuccess)
		return Failure;
	if (ArchiverSession::Create(m_lpsConfig, m_lpLogger, &m_ptrSession) != hrSuccess)
		return Failure;

	return Success;
}

eResult ArchiverImpl::GetControl(ArchiveControlPtr *lpptrControl, bool bForceCleanup)
{
    m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "ArchiverImpl::GetControl() function entry");
	if (!m_MAPI.IsInitialized())
		return Uninitialized;
    m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "ArchiverImpl::GetControl(): about to create an ArchiveControlImpl object");
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
	HRESULT hr;
    m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "ArchiverImpl::AutoAttach() function entry");
	ArchiveStateCollectorPtr ptrArchiveStateCollector;
	ArchiveStateUpdaterPtr ptrArchiveStateUpdater;

	if (ulFlags != ArchiveManage::Writable && ulFlags != ArchiveManage::ReadOnly && ulFlags != 0)
		return MAPIErrorToArchiveError(MAPI_E_INVALID_PARAMETER);

    m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "ArchiverImpl::AutoAttach() about to create collector");
	hr = ArchiveStateCollector::Create(m_ptrSession, m_lpLogger, &ptrArchiveStateCollector);
	if (hr != hrSuccess)
		return MAPIErrorToArchiveError(hr);

    m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "ArchiverImpl::AutoAttach() about to get state updater");
	hr = ptrArchiveStateCollector->GetArchiveStateUpdater(&ptrArchiveStateUpdater);
	if (hr != hrSuccess)
		return MAPIErrorToArchiveError(hr);

	if (ulFlags == 0) {
		if (parseBool(m_lpsConfig->GetSetting("auto_attach_writable")))
			ulFlags = ArchiveManage::Writable;
		else
			ulFlags = ArchiveManage::ReadOnly;
	}

    m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "ArchiverImpl::AutoAttach() about to call update all");
	return MAPIErrorToArchiveError(ptrArchiveStateUpdater->UpdateAll(ulFlags));
}

ECLogger* ArchiverImpl::GetLogger(eLogType which) const
{
	switch (which) {
		case DefaultLog:
			return ec_log_get();
		case LogOnly:
			return m_lpLogLogger;
		default:
			return nullptr;
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
		++ulSettings;

	return ulSettings;
}

} /* namespace */
