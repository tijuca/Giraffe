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

#include <string>
#include <list>
#include <memory>
#include <new>
#include <ostream>
#include <utility>
#include "ArchiveManage.h"
#include "ArchiveManageImpl.h"
#include "ArchiverSession.h"
#include "helpers/StoreHelper.h"
#include <kopano/charset/convert.h>
#include "ECACL.h"
#include <kopano/ECConfig.h>
#include <kopano/userutil.h>
#include "ArchiveStateUpdater.h"
#include <kopano/ECRestriction.h>
#include <kopano/MAPIErrors.h>

using namespace KC::helpers;

namespace KC {
    
inline UserEntry ArchiveManageImpl::MakeUserEntry(const std::string &strUser) {
	UserEntry entry;
	entry.UserName = strUser;
	return entry;
}

/**
 * Create an ArchiveManageImpl object.
 *
 * @param[in]	ptrSession
 *					Pointer to a Session object.
 * @param[in]	lpConfig
 * 					Pointer to an ECConfig object.
 * @param[in]	lpszUser
 *					The username of the user for which to create the archive manager.
 * @param[in]	lpLogger
 *					Pointer to an ECLogger object to which message will be logged.
 * @param[out]	lpptrArchiveManager
 *					Pointer to an ArchiveManagePtr that will be assigned the address of the returned object.
 */
HRESULT ArchiveManageImpl::Create(ArchiverSessionPtr ptrSession, ECConfig *lpConfig, const TCHAR *lpszUser, ECLogger *lpLogger, ArchiveManagePtr *lpptrArchiveManage)
{
	HRESULT hr;

	if (lpszUser == NULL)
		return MAPI_E_INVALID_PARAMETER;
	
	std::unique_ptr<ArchiveManageImpl> ptrArchiveManage(
		new(std::nothrow) ArchiveManageImpl(ptrSession, lpConfig, lpszUser, lpLogger));
	if (ptrArchiveManage == nullptr)
		return MAPI_E_NOT_ENOUGH_MEMORY;
	hr = ptrArchiveManage->Init();
	if (hr != hrSuccess)
		return hr;
	
	*lpptrArchiveManage = std::move(ptrArchiveManage);
	return hrSuccess;
}

HRESULT ArchiveManage::Create(LPMAPISESSION lpSession, ECLogger *lpLogger, const TCHAR *lpszUser, ArchiveManagePtr *lpptrManage)
{
	HRESULT hr;
	ArchiverSessionPtr ptrArchiverSession;

	hr = ArchiverSession::Create(MAPISessionPtr(lpSession, true), NULL, lpLogger, &ptrArchiverSession);
	if (hr != hrSuccess)
		return hr;

	return ArchiveManageImpl::Create(ptrArchiverSession, NULL, lpszUser,
		lpLogger, lpptrManage);
}

/**
 * @param[in]	ptrSession
 *					Pointer to a Session object.
 * @param[in]	lpConfig
 * 					Pointer to an ECConfig object.
 * @param[in]	lpszUser
 *					The username of the user for which to create the archive manager.
 * @param[in]	lpLogger
 *					Pointer to an ECLogger object to which message will be logged.
 */
ArchiveManageImpl::ArchiveManageImpl(ArchiverSessionPtr ptrSession, ECConfig *lpConfig, const tstring &strUser, ECLogger *lpLogger) :
	m_ptrSession(ptrSession),
	m_lpConfig(lpConfig),
	m_strUser(strUser)
{
	m_lpLogger = new(std::nothrow) ECArchiverLogger(lpLogger);
	if (m_lpLogger == nullptr)
		return;
	m_lpLogger->SetUser(strUser);
	if (lpConfig == nullptr)
		return;
	const char *loglevelstring = lpConfig->GetSetting("log_level");
	if (loglevelstring == nullptr)
		return;
	unsigned int loglevel = strtoul(loglevelstring, NULL, 0);
	m_lpLogger->SetLoglevel(loglevel);
}

ArchiveManageImpl::~ArchiveManageImpl()
{
	m_lpLogger->Release();
}

/**
 * Initialize the ArchiveManager object.
 */
HRESULT ArchiveManageImpl::Init()
{
	HRESULT hr;

	hr = m_ptrSession->OpenStoreByName(m_strUser, &~m_ptrUserStore);
	if (hr != hrSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Failed to open user store '" TSTRING_PRINTF "' (hr=%s).", m_strUser.c_str(), stringify(hr, true).c_str());
		return hr;
	}
	return hrSuccess;
}

/**
 * Attach an archive to the store of the user for which the ArchiveManger was created.
 *
 * @param[in]	lpszArchiveServer
 * 					If not NULL, this argument specifies the path of the server on which to create the archive.
 * @param[in]	lpszArchive
 *					The username of the non-active user that's the placeholder for the archive.
 * @param[in]	lpszFolder
 *					The name of the folder that will be used as the root of the archive. If ATT_USE_IPM_SUBTREE is passed
 *					in the ulFlags argument, the lpszFolder argument is ignored. If this argument is NULL, the username of
 *					the user is used as the root foldername.
 * @param[in]	ulFlags
 *					@ref flags specifying the options used for attaching the archive. 
 *
 * @section flags Flags
 * @li \b ATT_USE_IPM_SUBTREE	Use the IPM subtree of the archive store as the root of the archive.
 * @li \b ATT_WRITABLE			Make the archive writable for the user.
 */
eResult ArchiveManageImpl::AttachTo(const char *lpszArchiveServer, const TCHAR *lpszArchive, const TCHAR *lpszFolder, unsigned ulFlags)
{
	return MAPIErrorToArchiveError(AttachTo(lpszArchiveServer, lpszArchive, lpszFolder, ulFlags, ExplicitAttach));
}

HRESULT ArchiveManageImpl::AttachTo(const char *lpszArchiveServer, const TCHAR *lpszArchive, const TCHAR *lpszFolder, unsigned ulFlags, AttachType attachType)
{
	HRESULT	hr;
	MsgStorePtr ptrArchiveStore;
	tstring strFoldername;
	abentryid_t sUserEntryId;
	ArchiverSessionPtr ptrArchiveSession(m_ptrSession);
	ArchiverSessionPtr ptrRemoteSession;

	// Resolve the requested user.
	hr = m_ptrSession->GetUserInfo(m_strUser, &sUserEntryId, &strFoldername, NULL);
	if (hr != hrSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Failed to resolve user information for '" TSTRING_PRINTF "' (hr=%s).", m_strUser.c_str(), stringify(hr, true).c_str());
		return hr;
	}

	if (ulFlags & UseIpmSubtree)
		strFoldername.clear();	// Empty folder name indicates the IPM subtree.
	else if (lpszFolder)
		strFoldername.assign(lpszFolder);
		
	if (lpszArchiveServer) {
		hr = m_ptrSession->CreateRemote(lpszArchiveServer, m_lpLogger, &ptrRemoteSession);
		if (hr != hrSuccess) {
			m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Failed to connect to archive server '%s' (hr=%s).", lpszArchiveServer, stringify(hr, true).c_str());
			return hr;
		}
		
		ptrArchiveSession = ptrRemoteSession;
	}
	
	// Find the requested archive.
	hr = ptrArchiveSession->OpenStoreByName(lpszArchive, &~ptrArchiveStore);
	if (hr != hrSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Failed to open archive store '" TSTRING_PRINTF "' (hr=%s).", lpszArchive, stringify(hr, true).c_str());
		return hr;
	}

	return AttachTo(ptrArchiveStore, strFoldername, lpszArchiveServer,
		sUserEntryId, ulFlags, attachType);
}

HRESULT ArchiveManageImpl::AttachTo(LPMDB lpArchiveStore, const tstring &strFoldername, const char *lpszArchiveServer, const abentryid_t &sUserEntryId, unsigned ulFlags, AttachType attachType)
{
	HRESULT hr;
	ArchiveHelperPtr ptrArchiveHelper;
	abentryid_t sAttachedUserEntryId;
	StoreHelperPtr ptrStoreHelper;
	ObjectEntryList lstArchives;
	SObjectEntry objectEntry;
	bool bEqual = false;
	ArchiveType aType = UndefArchive;
	SPropValuePtr ptrArchiveName;
	SPropValuePtr ptrArchiveStoreId;
	
	// Check if we're not trying to attach a store to itself.
	hr = m_ptrSession->CompareStoreIds(m_ptrUserStore, lpArchiveStore, &bEqual);
	if (hr != hrSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Failed to compare user and archive store (hr=%s).", stringify(hr, true).c_str());
		return hr;
	}
	if (bEqual) {
		m_lpLogger->Log(EC_LOGLEVEL_FATAL, "User and archive store are the same.");
		return MAPI_E_INVALID_PARAMETER;
	}
	hr = HrGetOneProp(lpArchiveStore, PR_ENTRYID, &~ptrArchiveStoreId);
	if (hr != hrSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Failed to get archive store entryid (hr=%s).", stringify(hr, true).c_str());
		return hr;
	}

	hr = StoreHelper::Create(m_ptrUserStore, &ptrStoreHelper);
	if (hr != hrSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Failed to create store helper (hr=%s).", stringify(hr, true).c_str());
		return hr;
	}
	
	hr = ptrStoreHelper->GetArchiveList(&lstArchives);
	if (hr != hrSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Failed to get archive list (hr=%s).", stringify(hr, true).c_str());
		return hr;
	}

	// Find ptrArchiveStoreId in lstArchives
	for (const auto &arc : lstArchives) {
		bool bEqual;
		if (m_ptrSession->CompareStoreIds(arc.sStoreEntryId, ptrArchiveStoreId->Value.bin, &bEqual) == hrSuccess && bEqual) {
			m_lpLogger->Log(EC_LOGLEVEL_FATAL, "An archive for this '" TSTRING_PRINTF "' is already present in this store.", m_strUser.c_str());
			return MAPI_E_UNABLE_TO_COMPLETE;
		}
	}

	hr = ArchiveHelper::Create(lpArchiveStore, strFoldername, lpszArchiveServer, &ptrArchiveHelper);
	if (hr != hrSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Failed to create archive helper (hr=%s).", stringify(hr, true).c_str());
		return hr;
	}

	// Check if the archive is usable for the requested type
	hr = ptrArchiveHelper->GetArchiveType(&aType, NULL);
	if (hr != hrSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Failed to get archive type (hr=%s).", stringify(hr, true).c_str());
		return hr;
	}

	m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Archive Type: %d", (int)aType);
	if (aType == UndefArchive) {
		m_lpLogger->Log(EC_LOGLEVEL_NOTICE, "Preparing archive for first use");
		hr = ptrArchiveHelper->PrepareForFirstUse(m_lpLogger);
		if (hr != hrSuccess) {
			m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Failed to prepare archive (hr=0x%08x).", hr);
			return hr;
		}
	} else if (aType == SingleArchive && !strFoldername.empty()) {
		m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Attempted to create an archive folder in an archive store that has an archive in its root.");
		return MAPI_E_COLLISION;
	} else if (aType == MultiArchive && strFoldername.empty()) {
		m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Attempted to create an archive in the root of an archive store that has archive folders.");
		return MAPI_E_COLLISION;
	}

	// Check if the archive is attached yet.
	hr = ptrArchiveHelper->GetAttachedUser(&sAttachedUserEntryId);
	if (hr == MAPI_E_NOT_FOUND)
		hr = hrSuccess;

	else if ( hr == hrSuccess && (!sAttachedUserEntryId.empty() && sAttachedUserEntryId != sUserEntryId)) {
		tstring strUser;
		tstring strFullname;

		hr = m_ptrSession->GetUserInfo(sAttachedUserEntryId, &strUser, &strFullname);
		if (hr == hrSuccess)
			m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Archive is already used by " TSTRING_PRINTF " (" TSTRING_PRINTF ").", strUser.c_str(), strFullname.c_str());
		else
			m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Archive is already used (user entry: %s).", sAttachedUserEntryId.tostring().c_str());

		return MAPI_E_COLLISION;
	} else if (hr != hrSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Failed to get attached user for the requested archive (hr=%s).", stringify(hr, true).c_str());
		return hr;
	}
		
	// Add new archive to list of archives.
	hr = ptrArchiveHelper->GetArchiveEntry(true, &objectEntry);
	if (hr != hrSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Failed to get archive entry (hr=%s).", stringify(hr, true).c_str());
		return hr;
	}

	lstArchives.push_back(std::move(objectEntry));
	lstArchives.sort();
	lstArchives.unique();
	
	hr = ptrStoreHelper->SetArchiveList(lstArchives);
	if (hr != hrSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Failed to update archive list (hr=%s).", stringify(hr, true).c_str());
		return hr;
	}

	hr = ptrArchiveHelper->SetAttachedUser(sUserEntryId);
	if (hr != hrSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Failed to mark archive used (hr=%s).", stringify(hr, true).c_str());
		return hr;
	}

	hr = ptrArchiveHelper->SetArchiveType(strFoldername.empty() ? SingleArchive : MultiArchive, attachType);
	if (hr != hrSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Failed to set archive type to %d (hr=%s).", (int)(strFoldername.empty() ? SingleArchive : MultiArchive), stringify(hr, true).c_str());
		return hr;
	}
	
	// Update permissions
	if (!lpszArchiveServer) {	// No need to set permissions on a remote archive.
		hr = ptrArchiveHelper->SetPermissions(sUserEntryId, (ulFlags & Writable) == Writable);
		if (hr != hrSuccess) {
			m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Failed to set permissions on archive (hr=%s).", stringify(hr, true).c_str());
			return hr;
		}
	}

	// Create search folder
	hr = ptrStoreHelper->UpdateSearchFolders();
	if (hr != hrSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Failed to set search folders (hr=%s).", stringify(hr, true).c_str());
		return hr;	
	}
	hr = HrGetOneProp(lpArchiveStore, PR_DISPLAY_NAME, &~ptrArchiveName);
	if (hr != hrSuccess) {
		hr = hrSuccess;
		m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Successfully attached '" TSTRING_PRINTF "' in 'Unknown' for user '" TSTRING_PRINTF "'.", strFoldername.empty() ? _T("Root") : strFoldername.c_str(), m_strUser.c_str());
	} else
		m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Successfully attached '" TSTRING_PRINTF "' in '" TSTRING_PRINTF "' for user '" TSTRING_PRINTF "'.", strFoldername.empty() ? _T("Root") : strFoldername.c_str(), ptrArchiveName->Value.LPSZ, m_strUser.c_str());

	return hrSuccess;
}

/**
 * Detach an archive from a users store.
 *
 * @param[in]	lpszArchiveServer
 * 					If not NULL, this argument specifies the path of the server on which to create the archive.
 * @param[in]	lpszArchive
 *					The username of the non-active user that's the placeholder for the archive.
 * @param[in]	lpszFolder
 *					The name of the folder that's be used as the root of the archive. If this paramater
 *					is set to NULL and the user has only one archive in the archive store, which 
 *					is usually the case, that archive will be detached. If a user has multiple archives
 *					in the archive store, the exact folder need to be specified.
 *					If the archive root was placed in the IPM subtree of the archive store, this parameter
 *					must be set to NULL.
 */
eResult ArchiveManageImpl::DetachFrom(const char *lpszArchiveServer, const TCHAR *lpszArchive, const TCHAR *lpszFolder)
{
	HRESULT hr;
	entryid_t sUserEntryId;
	StoreHelperPtr ptrStoreHelper;
	ObjectEntryList lstArchives;
	ObjectEntryList::iterator iArchive;
	MsgStorePtr ptrArchiveStore;
	ArchiveHelperPtr ptrArchiveHelper;
	SPropValuePtr ptrArchiveStoreEntryId;
	MAPIFolderPtr ptrArchiveFolder;
	SPropValuePtr ptrDisplayName;
	ULONG ulType = 0;
	ArchiverSessionPtr ptrArchiveSession(m_ptrSession);
	ArchiverSessionPtr ptrRemoteSession;

	hr = StoreHelper::Create(m_ptrUserStore, &ptrStoreHelper);
	if (hr != hrSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Failed to create store helper (hr=%s).", stringify(hr, true).c_str());
		return MAPIErrorToArchiveError(hr);
	}
	
	hr = ptrStoreHelper->GetArchiveList(&lstArchives);
	if (hr != hrSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Failed to get archive list (hr=%s).", stringify(hr, true).c_str());
		return MAPIErrorToArchiveError(hr);
	}

	if (lpszArchiveServer) {
		hr = m_ptrSession->CreateRemote(lpszArchiveServer, m_lpLogger, &ptrRemoteSession);
		if (hr != hrSuccess) {
			m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Failed to connect to archive server '%s' (hr=%s).", lpszArchiveServer, stringify(hr, true).c_str());
			return MAPIErrorToArchiveError(hr);
		}
		
		ptrArchiveSession = ptrRemoteSession;
	}

	hr = ptrArchiveSession->OpenStoreByName(lpszArchive, &~ptrArchiveStore);
	if (hr != hrSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Failed to open archive store '" TSTRING_PRINTF "' (hr=%s).", lpszArchive, stringify(hr, true).c_str());
		return MAPIErrorToArchiveError(hr);
	}
	hr = HrGetOneProp(ptrArchiveStore, PR_ENTRYID, &~ptrArchiveStoreEntryId);
	if (hr != hrSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Failed to get archive entryid (hr=%s).", stringify(hr, true).c_str());
		return MAPIErrorToArchiveError(hr);
	}

	// Find an archives on the passed store.
	iArchive = find_if(lstArchives.begin(), lstArchives.end(), StoreCompare(ptrArchiveStoreEntryId->Value.bin));
	if (iArchive == lstArchives.end()) {
		m_lpLogger->Log(EC_LOGLEVEL_FATAL, "'" TSTRING_PRINTF "' has no archive on '" TSTRING_PRINTF "'", m_strUser.c_str(), lpszArchive);
		return MAPIErrorToArchiveError(MAPI_E_NOT_FOUND);
	}
	
	// If no folder name was passed and there are more archives for this user on this archive, we abort.
	if (lpszFolder == NULL) {
		ObjectEntryList::iterator iNextArchive(iArchive);
		++iNextArchive;

		if (find_if(iNextArchive, lstArchives.end(), StoreCompare(ptrArchiveStoreEntryId->Value.bin)) != lstArchives.end()) {
			m_lpLogger->Log(EC_LOGLEVEL_FATAL, "'" TSTRING_PRINTF "' has multiple archives on '" TSTRING_PRINTF "'", m_strUser.c_str(), lpszArchive);
			return MAPIErrorToArchiveError(MAPI_E_COLLISION);
		}
	}
	
	// If a folder name was passed, we need to find the correct folder.
	if (lpszFolder) {
		while (iArchive != lstArchives.end()) {
			hr = ptrArchiveStore->OpenEntry(iArchive->sItemEntryId.size(), iArchive->sItemEntryId, &ptrArchiveFolder.iid(), fMapiDeferredErrors, &ulType, &~ptrArchiveFolder);
			if (hr != hrSuccess) {
				m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Failed to open archive folder (hr=%s).", stringify(hr, true).c_str());
				return MAPIErrorToArchiveError(hr);
			}
			hr = HrGetOneProp(ptrArchiveFolder, PR_DISPLAY_NAME, &~ptrDisplayName);
			if (hr != hrSuccess) {
				m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Failed to get archive folder name (hr=%s).", stringify(hr, true).c_str());
				return MAPIErrorToArchiveError(hr);
			}
			
			if (_tcscmp(ptrDisplayName->Value.LPSZ, lpszFolder) == 0)
				break;
				
			iArchive = find_if(++iArchive, lstArchives.end(), StoreCompare(ptrArchiveStoreEntryId->Value.bin));
		}
		
		if (iArchive == lstArchives.end()) {
			m_lpLogger->Log(EC_LOGLEVEL_FATAL, "'" TSTRING_PRINTF "' has no archive named '" TSTRING_PRINTF "' on '" TSTRING_PRINTF "'", m_strUser.c_str(), lpszFolder, lpszArchive);
			return MAPIErrorToArchiveError(MAPI_E_NOT_FOUND);
		}
	}
	
	assert(iArchive != lstArchives.end());
	lstArchives.erase(iArchive);
	
	hr = ptrStoreHelper->SetArchiveList(lstArchives);
	if (hr != hrSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Failed to update archive list (hr=%s).", stringify(hr, true).c_str());
		return MAPIErrorToArchiveError(hr);
	}

	// Update search folders
	hr = ptrStoreHelper->UpdateSearchFolders();
	if (hr != hrSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Failed to set search folders (hr=%s).", stringify(hr, true).c_str());
		return MAPIErrorToArchiveError(hr);
	}

	if (lpszFolder)
		m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Successfully detached '" TSTRING_PRINTF "' in '" TSTRING_PRINTF "' from '" TSTRING_PRINTF "'.", lpszFolder, lpszArchive, m_strUser.c_str());
	else
		m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Successfully detached '" TSTRING_PRINTF "' from '" TSTRING_PRINTF "'.", lpszArchive, m_strUser.c_str());

	return MAPIErrorToArchiveError(hrSuccess);
}

/**
 * Detach an archive from a users store based on its index
 *
 * @param[in]	ulArchive
 * 					The index of the archive in the list of archives.
 */
eResult ArchiveManageImpl::DetachFrom(unsigned int ulArchive)
{
	HRESULT hr;
	StoreHelperPtr ptrStoreHelper;
	ObjectEntryList lstArchives;
	ObjectEntryList::iterator iArchive;

	hr = StoreHelper::Create(m_ptrUserStore, &ptrStoreHelper);
	if (hr != hrSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Failed to create store helper (hr=%s).", stringify(hr, true).c_str());
		return MAPIErrorToArchiveError(hr);
	}

	hr = ptrStoreHelper->GetArchiveList(&lstArchives);
	if (hr != hrSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Failed to get archive list (hr=%s).", stringify(hr, true).c_str());
		return MAPIErrorToArchiveError(hr);
	}

	iArchive = lstArchives.begin();
	for (unsigned int i = 0; i < ulArchive && iArchive != lstArchives.end(); ++i, ++iArchive);
	if (iArchive == lstArchives.end()) {
		m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Archive %u does not exist.", ulArchive);
		return MAPIErrorToArchiveError(MAPI_E_NOT_FOUND);
	}

	lstArchives.erase(iArchive);

	hr = ptrStoreHelper->SetArchiveList(lstArchives);
	if (hr != hrSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Failed to update archive list (hr=%s).", stringify(hr, true).c_str());
		return MAPIErrorToArchiveError(hr);
	}

	// Update search folders
	hr = ptrStoreHelper->UpdateSearchFolders();
	if (hr != hrSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Failed to set search folders (hr=%s).", stringify(hr, true).c_str());
		return MAPIErrorToArchiveError(hr);
	}

	m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Successfully detached archive %u from '" TSTRING_PRINTF "'.", ulArchive, m_strUser.c_str());
	return MAPIErrorToArchiveError(hrSuccess);
}

/**
 * List the attached archives for a user.
 *
 * @param[in]	ostr
 *					The std::ostream to which the list will be outputted.
 */
eResult ArchiveManageImpl::ListArchives(std::ostream &ostr)
{
	eResult er;
	ArchiveList	lstArchives;
	ULONG ulIdx = 0;

	er = ListArchives(&lstArchives, "Root Folder");
	if (er != Success)
		return er;

	ostr << "User '" << convert_to<std::string>(m_strUser) << "' has " << lstArchives.size() << " attached archives:" << std::endl;
	for (const auto &arc : lstArchives) {
		ostr << "\t" << ulIdx
			 << ": Store: " << arc.StoreName
			 << ", Folder: " << arc.FolderName;

		if (arc.Rights != ARCHIVE_RIGHTS_ABSENT) {
			 ostr << ", Rights: ";
			if (arc.Rights == ROLE_OWNER)
				ostr << "Read Write";
			else if (arc.Rights == ROLE_REVIEWER)
				ostr << "Read Only";
			else
				ostr << "Modified: " << AclRightsToString(arc.Rights);
		}

		ostr << std::endl;
	}
	return Success;
}

eResult ArchiveManageImpl::ListArchives(ArchiveList *lplstArchives, const char *lpszIpmSubtreeSubstitude)
{
	HRESULT hr;
	StoreHelperPtr ptrStoreHelper;
	bool bAclCapable = true;
	ObjectEntryList lstArchives;
	MsgStorePtr ptrArchiveStore;
	ULONG ulType = 0;
	ArchiveList lstEntries;

	hr = StoreHelper::Create(m_ptrUserStore, &ptrStoreHelper);
	if (hr != hrSuccess)
		return MAPIErrorToArchiveError(hr);

	hr = m_ptrSession->GetUserInfo(m_strUser, NULL, NULL, &bAclCapable);
	if (hr != hrSuccess)
		return MAPIErrorToArchiveError(hr);

	hr = ptrStoreHelper->GetArchiveList(&lstArchives);
	if (hr != hrSuccess)
		return MAPIErrorToArchiveError(hr);
		
	for (const auto &arc : lstArchives) {
		HRESULT hrTmp = hrSuccess;
		ULONG cStoreProps = 0;
		SPropArrayPtr ptrStoreProps;
		ArchiveEntry entry;
		MAPIFolderPtr ptrArchiveFolder;
		SPropValuePtr ptrPropValue;
		ULONG ulCompareResult = FALSE;
		static constexpr const SizedSPropTagArray(4, sptaStoreProps) = {4, {PR_DISPLAY_NAME_A, PR_MAILBOX_OWNER_ENTRYID, PR_IPM_SUBTREE_ENTRYID, PR_STORE_RECORD_KEY}};
		enum {IDX_DISPLAY_NAME, IDX_MAILBOX_OWNER_ENTRYID, IDX_IPM_SUBTREE_ENTRYID, IDX_STORE_RECORD_KEY};

		entry.Rights = ARCHIVE_RIGHTS_ERROR;

		hrTmp = m_ptrSession->OpenStore(arc.sStoreEntryId, &~ptrArchiveStore);
		if (hrTmp != hrSuccess) {
			m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Failed to open store (hr=%s)", stringify(hrTmp, true).c_str());
			entry.StoreName = "Failed id=" + arc.sStoreEntryId.tostring() + ", hr=" + stringify(hrTmp, true);
			lstEntries.push_back(std::move(entry));
			continue;
		}
		
		hrTmp = ptrArchiveStore->GetProps(sptaStoreProps, 0, &cStoreProps, &~ptrStoreProps);
		if (FAILED(hrTmp))
			entry.StoreName = entry.StoreOwner = "Unknown (" + stringify(hrTmp, true) + ")";
		else {
			if (ptrStoreProps[IDX_DISPLAY_NAME].ulPropTag == PR_DISPLAY_NAME_A)
				entry.StoreName = ptrStoreProps[IDX_DISPLAY_NAME].Value.lpszA;
			else
				entry.StoreName = "Unknown (" + stringify(ptrStoreProps[IDX_DISPLAY_NAME].Value.err, true) + ")";
				
			if (ptrStoreProps[IDX_MAILBOX_OWNER_ENTRYID].ulPropTag == PR_MAILBOX_OWNER_ENTRYID) {
				MAPIPropPtr ptrOwner;

				hrTmp = m_ptrSession->OpenMAPIProp(ptrStoreProps[IDX_MAILBOX_OWNER_ENTRYID].Value.bin.cb,
				        reinterpret_cast<ENTRYID *>(ptrStoreProps[IDX_MAILBOX_OWNER_ENTRYID].Value.bin.lpb),
				        &~ptrOwner);
				if (hrTmp == hrSuccess)
					hrTmp = HrGetOneProp(ptrOwner, PR_ACCOUNT_A, &~ptrPropValue);

				if (hrTmp == hrSuccess)
					entry.StoreOwner = ptrPropValue->Value.lpszA;
				else
					entry.StoreOwner = "Unknown (" + stringify(hrTmp, true) + ")";
			} else
				entry.StoreOwner = "Unknown (" + stringify(ptrStoreProps[IDX_MAILBOX_OWNER_ENTRYID].Value.err, true) + ")";

			if (lpszIpmSubtreeSubstitude) {
				if (ptrStoreProps[IDX_IPM_SUBTREE_ENTRYID].ulPropTag != PR_IPM_SUBTREE_ENTRYID)
					hrTmp = MAPI_E_NOT_FOUND;
				else
					hrTmp = ptrArchiveStore->CompareEntryIDs(arc.sItemEntryId.size(), arc.sItemEntryId,
															 ptrStoreProps[IDX_IPM_SUBTREE_ENTRYID].Value.bin.cb, (LPENTRYID)ptrStoreProps[IDX_IPM_SUBTREE_ENTRYID].Value.bin.lpb,
															 0, &ulCompareResult);
				if (hrTmp != hrSuccess) {
					m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Failed to compare entry ids (hr=%s)", stringify(hrTmp, true).c_str());
					ulCompareResult = FALSE;	// Let's assume it's not the IPM Subtree.
				}
			}
			if (ptrStoreProps[IDX_STORE_RECORD_KEY].ulPropTag == PR_STORE_RECORD_KEY)
				entry.StoreGuid = bin2hex(ptrStoreProps[IDX_STORE_RECORD_KEY].Value.bin.cb, ptrStoreProps[IDX_STORE_RECORD_KEY].Value.bin.lpb);
		}

		hrTmp = ptrArchiveStore->OpenEntry(arc.sItemEntryId.size(), arc.sItemEntryId, &ptrArchiveFolder.iid(), fMapiDeferredErrors, &ulType, &~ptrArchiveFolder);
		if (hrTmp != hrSuccess) {
			m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Failed to open folder (hr=%s)", stringify(hrTmp, true).c_str());
			entry.FolderName = "Failed id=" + arc.sStoreEntryId.tostring() + ", hr=" + stringify(hrTmp, true);
			lstEntries.push_back(std::move(entry));
			continue;
		}
		
		if (lpszIpmSubtreeSubstitude && ulCompareResult == TRUE) {
			assert(lpszIpmSubtreeSubstitude != NULL);
			entry.FolderName = lpszIpmSubtreeSubstitude;
		} else {
			hrTmp = HrGetOneProp(ptrArchiveFolder, PR_DISPLAY_NAME_A, &~ptrPropValue);
			if (hrTmp != hrSuccess)
				entry.FolderName = "Unknown (" + stringify(hrTmp, true) + ")";
			else
				entry.FolderName = ptrPropValue->Value.lpszA ;
		}

		if (bAclCapable && !arc.sStoreEntryId.isWrapped()) {
			hrTmp = GetRights(ptrArchiveFolder, &entry.Rights);
			if (hrTmp != hrSuccess)
				m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Failed to get archive rights (hr=%s)", stringify(hrTmp, true).c_str());
		} else
			entry.Rights = ARCHIVE_RIGHTS_ABSENT;

		lstEntries.push_back(std::move(entry));
	}

	lplstArchives->swap(lstEntries);
	return MAPIErrorToArchiveError(hrSuccess);
}

/**
 * Print a list of users with an attached archive store
 *
 * @param[in]  ostr
 *                     Output stream to write results to.
 *
 * @return eResult
*/
eResult ArchiveManageImpl::ListAttachedUsers(std::ostream &ostr)
{
	eResult er;
	UserList lstUsers;

	er = ListAttachedUsers(&lstUsers);
	if (er != Success)
		return er;

	if (lstUsers.empty()) {
		ostr << "No users have an archive attached." << std::endl;
		return Success;
	}

	ostr << "Users with an attached archive:" << std::endl;
	for (const auto &user : lstUsers)
		ostr << "\t" << user.UserName << std::endl;
	return Success;
}

eResult ArchiveManageImpl::ListAttachedUsers(UserList *lplstUsers)
{
	HRESULT hr;
	std::list<std::string> lstUsers;
	UserList lstUserEntries;

	if (lplstUsers == NULL)
		return MAPIErrorToArchiveError(MAPI_E_INVALID_PARAMETER);

	hr = GetArchivedUserList(m_ptrSession->GetMAPISession(),
	     m_ptrSession->GetSSLPath(), m_ptrSession->GetSSLPass(), &lstUsers);
	if (hr != hrSuccess)
		return MAPIErrorToArchiveError(hr);

	std::transform(lstUsers.begin(), lstUsers.end(), std::back_inserter(lstUserEntries), &MakeUserEntry);
	lplstUsers->swap(lstUserEntries);
	return MAPIErrorToArchiveError(hr);
}

/**
 * Auto attach and detach archives to user stores based on the addressbook
 * settings.
 */
eResult ArchiveManageImpl::AutoAttach(unsigned int ulFlags)
{
	HRESULT hr = hrSuccess;
    m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "ArchiveManageImpl::AutoAttach(): function entry");
	ArchiveStateCollectorPtr ptrArchiveStateCollector;
	ArchiveStateUpdaterPtr ptrArchiveStateUpdater;

	if (ulFlags != ArchiveManage::Writable && ulFlags != ArchiveManage::ReadOnly && ulFlags != 0) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

    m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "ArchiveManageImpl::AutoAttach(): about to create ArchiveStateCollector");
	hr = ArchiveStateCollector::Create(m_ptrSession, m_lpLogger, &ptrArchiveStateCollector);
	if (hr != hrSuccess)
		goto exit;

    m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "ArchiveManageImpl::AutoAttach(): about to get ArchiveStateUpdater");
	hr = ptrArchiveStateCollector->GetArchiveStateUpdater(&ptrArchiveStateUpdater);
	if (hr != hrSuccess)
		goto exit;

	if (ulFlags == 0) {
		if (!m_lpConfig || parseBool(m_lpConfig->GetSetting("auto_attach_writable")))
			ulFlags = ArchiveManage::Writable;
		else
			ulFlags = ArchiveManage::ReadOnly;
	}

    m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "ArchiveManageImpl::AutoAttach(): about to call ArchiveStateUpdater::Update");
	hr = ptrArchiveStateUpdater->Update(m_strUser, ulFlags);

exit:
    m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "ArchiveManageImpl::AutoAttach(): function exit. Result: 0x%08X (%s)", hr, GetMAPIErrorMessage(hr));
	return MAPIErrorToArchiveError(hr);
}

/**
 * Obtain the rights for the user for which the instance of ArhiceveManageImpl
 * was created on the passed folder.
 *
 * @param[in]	lpFolder	The folder to get the rights from
 * @param[out]	lpulRights	The rights the current user has on the folder.
 */
HRESULT ArchiveManageImpl::GetRights(LPMAPIFOLDER lpFolder, unsigned *lpulRights)
{
	HRESULT hr;
	SPropValuePtr ptrName;
	ExchangeModifyTablePtr ptrACLModifyTable;
	MAPITablePtr ptrACLTable;
	SPropValue sPropUser;
	SRowSetPtr ptrRows;
	static constexpr const SizedSPropTagArray(1, sptaTableProps) = {1, {PR_MEMBER_RIGHTS}};

	if (lpFolder == NULL || lpulRights == NULL)
		return MAPI_E_INVALID_PARAMETER;

	// In an ideal world we would use the user entryid for the restriction.
	// However, the ACL table is a client side table, which doesn't implement
	// comparing AB entryids correctly over multiple servers. Since we're
	// most likely dealing with multiple servers here, we'll use the users
	// fullname instead.
	hr = HrGetOneProp(m_ptrUserStore, PR_MAILBOX_OWNER_NAME, &~ptrName);
	if (hr != hrSuccess)
		return hr;
	hr = lpFolder->OpenProperty(PR_ACL_TABLE, &IID_IExchangeModifyTable, 0, 0, &~ptrACLModifyTable);
	if (hr != hrSuccess)
		return hr;
	hr = ptrACLModifyTable->GetTable(0, &~ptrACLTable);
	if (hr != hrSuccess)
		return hr;
	hr = ptrACLTable->SetColumns(sptaTableProps, TBL_BATCH);
	if (hr != hrSuccess)
		return hr;

	sPropUser.ulPropTag = PR_MEMBER_NAME;
	sPropUser.Value.LPSZ = ptrName->Value.LPSZ;

	hr = ECPropertyRestriction(RELOP_EQ, PR_MEMBER_NAME, &sPropUser, ECRestriction::Cheap)
	     .FindRowIn(ptrACLTable, BOOKMARK_BEGINNING, 0);
	if (hr != hrSuccess)
		return hr;
	hr = ptrACLTable->QueryRows(1, 0, &ptrRows);
	if (hr != hrSuccess)
		return hr;

	if (ptrRows.empty()) {
		assert(false);
		return MAPI_E_NOT_FOUND;
	}

	*lpulRights = ptrRows[0].lpProps[0].Value.ul;
	return hrSuccess;
}

} /* namespace */
