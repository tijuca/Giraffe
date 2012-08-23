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
#include "archivestateupdater.h"
#include "archiver-session.h"
#include "ECLogger.h"
#include "ECIterators.h"
#include "HrException.h"
#include "ECRestriction.h"
#include "CommonUtil.h"
#include "mapiguidext.h"
#include "helpers/storehelper.h"
#include "helpers/archivehelper.h"
#include "archivemanage.h"

using namespace za;
using namespace za::helpers;

namespace Predicates {

	/**
	 * Compare two SObjectEntry instances.
	 * This version does a binary compare of the embedded entry ids.
	 */
	class SObjectEntry_equals_binary {
	public:
		SObjectEntry_equals_binary(const SObjectEntry &objEntry): m_objEntry(objEntry) {}
		bool operator()(const SObjectEntry &objEntry) { return objEntry == m_objEntry; }
	private:
		const SObjectEntry &m_objEntry;
	};


	/**
	 * Compare two SObjectEntry instances.
	 * This method uses CompareEntryIDs to do the comparison.
	 */
	class SObjectEntry_equals_compareEntryId {
	public:
		SObjectEntry_equals_compareEntryId(IMAPISession *lpSession, const SObjectEntry &objEntry): m_lpSession(lpSession), m_objEntry(objEntry) {}
		bool operator()(const SObjectEntry &objEntry) {
			HRESULT hr = hrSuccess;
			ULONG ulResult = 0;
			
			hr = m_lpSession->CompareEntryIDs(m_objEntry.sStoreEntryId.size(), m_objEntry.sStoreEntryId, objEntry.sStoreEntryId.size(), objEntry.sStoreEntryId, 0, &ulResult);
			if (hr != hrSuccess || ulResult == 0)
				return false;

			hr = m_lpSession->CompareEntryIDs(m_objEntry.sItemEntryId.size(), m_objEntry.sItemEntryId, objEntry.sItemEntryId.size(), objEntry.sItemEntryId, 0, &ulResult);
			return (hr == hrSuccess && ulResult == 1);
		}
	private:
		IMAPISession *m_lpSession;
		const SObjectEntry &m_objEntry;
	};


	/**
	 * Compare a store entryid with the store entryid from an SObjectEntry instance.
	 * This method uses CompareEntryIDs to do the comparison.
	 */
	class storeId_equals_compareEntryId {
	public:
		storeId_equals_compareEntryId(IMAPISession *lpSession, const entryid_t &storeId): m_lpSession(lpSession), m_storeId(storeId) {}
		bool operator()(const SObjectEntry &objEntry) {
			HRESULT hr = hrSuccess;
			ULONG ulResult = 0;
			
			hr = m_lpSession->CompareEntryIDs(m_storeId.size(), m_storeId, objEntry.sStoreEntryId.size(), objEntry.sStoreEntryId, 0, &ulResult);
			return (hr == hrSuccess && ulResult == 1);
		}
	private:
		IMAPISession *m_lpSession;
		const entryid_t &m_storeId;
	};

	class MapInfo_contains_userName {
	public:
		MapInfo_contains_userName(const tstring &userName): m_userName(userName) {}
		bool operator()(const ArchiveStateUpdater::ArchiveInfoMap::value_type &pair) { return m_userName.compare(pair.second.userName) == 0; }
	private:
		const tstring &m_userName;
	};

} // namespace Predicates


/**
 * Create an ArchiveStateUpdater instance.
 * @param[in]	ptrSession		The archiver session.
 * @param[in]	lpLogger		The logger.
 * @param[in]	mapArchiveInfo	The map containing the users that have and/or
 * 								should have an archive attached to their
 * 								primary store.
 * @param[out]	lpptrUpdater	The new ArchiveStateUpdater instance
 */
HRESULT ArchiveStateUpdater::Create(const SessionPtr &ptrSession, ECLogger *lpLogger, const ArchiveInfoMap &mapArchiveInfo, ArchiveStateUpdaterPtr *lpptrUpdater)
{
	HRESULT hr = hrSuccess;
	ArchiveStateUpdaterPtr ptrUpdater;
	
	try {
		ptrUpdater = ArchiveStateUpdaterPtr(new ArchiveStateUpdater(ptrSession, lpLogger, mapArchiveInfo));
	} catch (const std::bad_alloc &) {
		hr = MAPI_E_NOT_ENOUGH_MEMORY;
		goto exit;
	}

	*lpptrUpdater = ptrUpdater;

exit:
	return hr;
}

/**
 * Constructor
 * @param[in]	ptrSession		The archiver session.
 * @param[in]	lpLogger		The logger.
 * @param[in]	mapArchiveInfo	The map containing the users that have and/or
 * 								should have an archive attached to their
 * 								primary store.
 */
ArchiveStateUpdater::ArchiveStateUpdater(const SessionPtr &ptrSession, ECLogger *lpLogger, const ArchiveInfoMap &mapArchiveInfo): m_ptrSession(ptrSession), m_lpLogger(lpLogger), m_mapArchiveInfo(mapArchiveInfo)
{
	if (m_lpLogger)
		m_lpLogger->AddRef();
	else
		m_lpLogger = new ECLogger_Null();
}

ArchiveStateUpdater::~ArchiveStateUpdater()
{
	m_lpLogger->Release();
}

/**
 * Update all users to the required state.
 */
HRESULT ArchiveStateUpdater::UpdateAll(unsigned int ulAttachFlags)
{
	HRESULT hr = hrSuccess;

	for (ArchiveInfoMap::const_iterator i = m_mapArchiveInfo.begin(); i != m_mapArchiveInfo.end(); ++i) {
		HRESULT hrTmp = UpdateOne(i->first, i->second, ulAttachFlags);
		if (hrTmp != hrSuccess)
			m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Failed to auto attach store for user '" TSTRING_PRINTF "', hr=0x%08x", i->second.userName.c_str(), hrTmp);
	}

	return hr;
}

/**
 * Update a single user to the required state.
 * @param[in]	userName	The username of the user to update.
 */
HRESULT ArchiveStateUpdater::Update(const tstring &userName, unsigned int ulAttachFlags)
{
	HRESULT hr = hrSuccess;

	// First see if the username can be found in the map.
	ArchiveInfoMap::const_iterator i = std::find_if(m_mapArchiveInfo.begin(), m_mapArchiveInfo.end(), Predicates::MapInfo_contains_userName(userName));
	if (i == m_mapArchiveInfo.end()) {
		// Resolve the username and search by entryid.
		abentryid_t userId;

		m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Unable to find entry for user '" TSTRING_PRINTF "', trying to resolve.", userName.c_str());
		hr = m_ptrSession->GetUserInfo(userName, &userId, NULL, NULL);
		if (hr != hrSuccess)
			goto exit;

		i = m_mapArchiveInfo.find(userId);
		if (i == m_mapArchiveInfo.end()) {
		m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Unable to find entry for userid %s.", userId.tostring().c_str());
			hr = MAPI_E_NOT_FOUND;
			goto exit;
		}
	}

	hr = UpdateOne(i->first, i->second, ulAttachFlags);
	if (hr != hrSuccess)
		m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Failed to auto attach store for user '" TSTRING_PRINTF "', hr=0x%08x", userName.c_str(), hr);

exit:
	return hr;
}

/**
 * Update one single user.
 * @param[in]	userId		The entryid of the user to update.
 * @param[in[	info		The ArchiveInfo object containing the current and
 * 							required state.
 */
HRESULT ArchiveStateUpdater::UpdateOne(const abentryid_t &userId, const ArchiveInfo& info, unsigned int ulAttachFlags)
{
	HRESULT hr = hrSuccess;
	
	if (info.userName.empty()) {
		// Found a store that has archives attached but no archive- servers or couplings
		// are defined in the GAB.
		hr = RemoveImplicit(info.storeId, tstring(), userId, info.lstArchives);
	}

	else if (info.storeId.empty()) {
		// Found a user in the GAB that has at least one archive- server or coupling
		// defined but has no archives attached.
		hr = AddCouplingBased(info.userName, info.lstCouplings, ulAttachFlags);
		if (hr == hrSuccess)
			hr = AddServerBased(info.userName, userId, info.lstServers, ulAttachFlags);
	}

	else {
		hr = VerifyAndUpdate(userId, info, ulAttachFlags);
	}

return hr;
}

/**
 * Remove/detach all implicit attached archives
 * @param[in]	storeId		The entryid of the primary store to process.
 * @param[in]	userName	The name of the user owning the store to process. This
 * 							is an alternative way of finding the store if
 * 							storeId is unwrapped.
 * @param[in]	userId		The entryid of the user owning the store to process.
 * 							This is an alternative way of finding the store if
 * 							storeId is unwrapped and userName is unknown.
 * @param[in]	lstArchives	The list of archives to remove the implicit attached
 * 							archives from.
 */
HRESULT ArchiveStateUpdater::RemoveImplicit(const entryid_t &storeId, const tstring &userName, const abentryid_t &userId, const ObjectEntryList &lstArchives)
{
	HRESULT hr = hrSuccess;
	MsgStorePtr ptrUserStore;
	StoreHelperPtr ptrUserStoreHelper;
	ObjectEntryList lstCurrentArchives;
	ULONG ulDetachCount = 0;

	m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Removing implicitly attached archives.");

	hr = m_ptrSession->OpenStore(storeId, &ptrUserStore);
	if (hr == MAPI_E_INVALID_ENTRYID) {
		m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Got invalid entryid, attempting to resolve...");
		
		// The storeId was obtained from the MailboxTable that currently does not return
		// a correct entryid. See https://jira.zarafa.com/browse/ZCP-8613
		if (!userName.empty()) {
			m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Resolving user '" TSTRING_PRINTF "'", userName.c_str());
			hr = m_ptrSession->OpenStoreByName(userName, &ptrUserStore);
			if (hr != hrSuccess) {
				m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Failed to resolve store for user '" TSTRING_PRINTF "'", userName.c_str());
				goto exit;
			}
		} else if (userId.size() != 0) {
			tstring strUserName;
			m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Resolving user id %s", userId.tostring().c_str());
			hr = m_ptrSession->GetUserInfo(userId, &strUserName, NULL);
			if (hr != hrSuccess) {
				m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Failed to get info for user id %s", userId.tostring().c_str());
				goto exit;
			}
				
			m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Resolving user '" TSTRING_PRINTF "'", userName.c_str());
			hr = m_ptrSession->OpenStoreByName(strUserName, &ptrUserStore);
			if (hr != hrSuccess) {
				m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Failed to resolve store for user '" TSTRING_PRINTF "'", userName.c_str());
				goto exit;
			}
		}
	}
	if (hr != hrSuccess)
		goto exit;

	hr = StoreHelper::Create(ptrUserStore, &ptrUserStoreHelper);
	if (hr != hrSuccess)
		goto exit;

	hr = ptrUserStoreHelper->GetArchiveList(&lstCurrentArchives);
	if (hr != hrSuccess)
		goto exit;

	m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Processing %lu archives for implicitly attached archives", lstArchives.size());
	for (ObjectEntryList::const_iterator i = lstArchives.begin(); i != lstArchives.end(); ++i) {
		MsgStorePtr ptrArchStore;
		ULONG ulType;
		MAPIFolderPtr ptrArchFolder;
		ArchiveHelperPtr ptrArchiveHelper;
		AttachType attachType;

		hr = m_ptrSession->OpenStore(i->sStoreEntryId, &ptrArchStore);
		if (hr == MAPI_E_NOT_FOUND) {
			m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Archive store returned not found, detaching it.");
			lstCurrentArchives.remove_if(Predicates::SObjectEntry_equals_binary(*i));
			continue;
		}
		if (hr != hrSuccess) {
			m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Failed to open archive store. hr=0x%08x", hr);
			goto exit;
		}

		hr = ptrArchStore->OpenEntry(i->sItemEntryId.size(), i->sItemEntryId, &ptrArchFolder.iid, 0, &ulType, &ptrArchFolder);
		if (hr != hrSuccess) {
			m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Failed to open archive root. hr=0x%08x", hr);
			if (hr == MAPI_E_NOT_FOUND) {
				m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Possibly invalid entry, skipping...");
				continue;
			}
			goto exit;
		}

		hr = ArchiveHelper::Create(ptrArchStore, ptrArchFolder, NULL, &ptrArchiveHelper);
		if (hr != hrSuccess)
			goto exit;

		hr = ptrArchiveHelper->GetArchiveType(NULL, &attachType);
		if (hr != hrSuccess) {
			m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Failed to get attachType. hr=0x%08x", hr);
			goto exit;
		}

		if (attachType == ImplicitAttach) {
			m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Archive was implicitly attached, detaching.");
			lstCurrentArchives.remove_if(Predicates::SObjectEntry_equals_binary(*i));
			ulDetachCount++;
		} else
			m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Archive was explicitly attached");
	}

	if (ulDetachCount > 0) {
		hr = ptrUserStoreHelper->SetArchiveList(lstCurrentArchives, true);
		if (hr != hrSuccess) {
			m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Failed to set archive list, hr=0x%08x", hr);
			goto exit;
		}

		if (!userName.empty())
			m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Auto detached %u archive(s) from '" TSTRING_PRINTF "'.", ulDetachCount, userName.c_str());
		else {
			tstring strUserName;
			if (m_ptrSession->GetUserInfo(userId, &strUserName, NULL) == hrSuccess)
				m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Auto detached %u archive(s) from '" TSTRING_PRINTF "'.", ulDetachCount, strUserName.c_str());
			else
				m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Auto detached %u archive(s).", ulDetachCount);
		}

		hr = ptrUserStoreHelper->UpdateSearchFolders();
		if (hr != hrSuccess)
			goto exit;
	}

exit:
	return hr;
}

/**
 * Split a coupling in a store name and a folder name.
 * A coupling is defined as <storename>:<foldername>
 * @param[in]	strCoupling		The coupling to parse
 * @param[out]	lpstrArchive	The archive store name
 * @param[out]	lpstrFolder		The archive folder name
 */
HRESULT ArchiveStateUpdater::ParseCoupling(const tstring &strCoupling, tstring *lpstrArchive, tstring *lpstrFolder)
{
	HRESULT hr = hrSuccess;
	
	tstring strArchive = strCoupling;
	tstring strFolder;
	tstring::size_type idxColon;
	
	m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Coupling: '" TSTRING_PRINTF "'", strArchive.c_str());

	idxColon = strArchive.find(':');
	if (idxColon == std::string::npos) {
		m_lpLogger->Log(EC_LOGLEVEL_ERROR, "No ':' seperator found in coupling");
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	strFolder.assign(strArchive.substr(idxColon + 1));
	strArchive.resize(idxColon);

	m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Coupling: archive='" TSTRING_PRINTF "', folder='" TSTRING_PRINTF "'", strArchive.c_str(), strFolder.c_str());
	if (strArchive.empty() || strFolder.empty()) {
		m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Invalid coupling: archive='" TSTRING_PRINTF "', folder='" TSTRING_PRINTF "'", strArchive.c_str(), strFolder.c_str());
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	lpstrArchive->swap(strArchive);
	lpstrFolder->swap(strFolder);

exit:
	return hr;
}

/**
 * Add/attach coupling based archives.
 * @param[in]	userName		The username of the primary store to attach the
 * 								archives to.
 * @param[in]	lstCouplings	The list of couplings to attach to the store.
 */
HRESULT ArchiveStateUpdater::AddCouplingBased(const tstring &userName, const std::list<tstring> &lstCouplings, unsigned int ulAttachFlags)
{
	HRESULT hr = hrSuccess;
	ArchiveManagePtr ptrManage;
	ArchiveManageImpl* lpManage = NULL;

	m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Attaching coupling based archives.");

	if (lstCouplings.empty()) {
		m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Empty coupling list");
		goto exit;
	}

	hr = ArchiveManageImpl::Create(m_ptrSession, NULL, userName.c_str(), m_lpLogger, &ptrManage);
	if (hr != hrSuccess)
		goto exit;

	lpManage = dynamic_cast<ArchiveManageImpl*>(ptrManage.get());
	ASSERT(lpManage != NULL);
	if (lpManage == NULL) {
		m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to dynamic cast to ArchiveManageImpl pointer.");
		hr = MAPI_E_CALL_FAILED;
		goto exit;
	}

	m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Attaching %lu couplings", lstCouplings.size());
	for (std::list<tstring>::const_iterator i = lstCouplings.begin(); i != lstCouplings.end(); ++i) {
		tstring strArchive;
		tstring strFolder;

		hr = ParseCoupling(*i, &strArchive, &strFolder);
		if (hr != hrSuccess)
			goto exit;

		hr = lpManage->AttachTo(NULL, strArchive.c_str(), strFolder.c_str(), ulAttachFlags, ImplicitAttach);
		if (hr != hrSuccess) {
			m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Failed to attach to store '" TSTRING_PRINTF "' in folder '" TSTRING_PRINTF "', hr=0x%08x", strArchive.c_str(), strFolder.c_str(), hr);
			goto exit;
		}
	}

exit:
	return hr;
}

/**
 * Add/attach server based archives.
 * @param[in]	userName		The username of the primary store to attach the
 * 								archives to.
 * @param[in]	userId			The entryid of the user whose primary store to
 * 								attach to.
 * @param[in]	lstServers		The list of servers on which an archive for userName
 * 								should be created or opened and attached to the
 * 								primary store.
 */
HRESULT ArchiveStateUpdater::AddServerBased(const tstring &userName, const abentryid_t &userId, const std::list<tstring> &lstServers, unsigned int ulAttachFlags)
{
	HRESULT hr = hrSuccess;
	ArchiveManagePtr ptrManage;
	ArchiveManageImpl* lpManage = NULL;

	m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Attaching servername based archives.");

	if (lstServers.empty()) {
		m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Empty servername list");
		goto exit;
	}

	hr = ArchiveManageImpl::Create(m_ptrSession, NULL, userName.c_str(), m_lpLogger, &ptrManage);
	if (hr != hrSuccess)
		goto exit;

	lpManage = dynamic_cast<ArchiveManageImpl*>(ptrManage.get());
	ASSERT(lpManage != NULL);
	if (lpManage == NULL) {
		m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to dynamic cast to ArchiveManageImpl pointer.");
		hr = MAPI_E_CALL_FAILED;
		goto exit;
	}

	m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Attaching %lu servers", lstServers.size());
	for (std::list<tstring>::const_iterator i = lstServers.begin(); i != lstServers.end(); ++i) {
		MsgStorePtr ptrArchive;
		
		hr = m_ptrSession->OpenOrCreateArchiveStore(userName, *i, &ptrArchive);
		if (hr != hrSuccess) {
			m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Failed to open or create the archive for user '" TSTRING_PRINTF "' on server '" TSTRING_PRINTF "', hr=0x%08x", userName.c_str(), i->c_str(), hr);
			goto exit;
		}

		hr = lpManage->AttachTo(ptrArchive, L"", NULL, userId, ulAttachFlags, ImplicitAttach);
		if (hr != hrSuccess) {
			m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Failed to attach to archive store for user '" TSTRING_PRINTF "' on server '" TSTRING_PRINTF "', hr=0x%08x", userName.c_str(), i->c_str(), hr);
			goto exit;
		}
	}

exit:
	return hr;
}

/**
 * Verify the current state and update it to match the required state.
 * @param[in]	userId		The entryid of the user whose primary store to
 * 							process.
 * @param[in]	info		ArchiveInfo instance containing the current and
 * 							requried state.
 */
HRESULT ArchiveStateUpdater::VerifyAndUpdate(const abentryid_t &userId, const ArchiveInfo& info, unsigned int ulAttachFlags)
{
	HRESULT hr = hrSuccess;
	std::list<tstring> lstServers;
	std::list<tstring> lstCouplings;
	ObjectEntryList lstArchives = info.lstArchives;

	// Handle the automated couplings
	for (std::list<tstring>::const_iterator i = info.lstCouplings.begin(); i != info.lstCouplings.end(); ++i) {
		tstring strArchive;
		tstring strFolder;
		SObjectEntry objEntry;
		ObjectEntryList::iterator iObjEntry;

		hr = ParseCoupling(*i, &strArchive, &strFolder);
		if (hr != hrSuccess)
			goto exit;

		hr = FindArchiveEntry(strArchive, strFolder, &objEntry);
		if (hr == MAPI_E_NOT_FOUND) {
			m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "store '" TSTRING_PRINTF "', folder '" TSTRING_PRINTF "' does not exist. Adding to coupling list", strArchive.c_str(), strFolder.c_str());
			lstCouplings.push_back(*i);
			continue;
		}
		if (hr != hrSuccess) {
			m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Failed to get archive entry for store '" TSTRING_PRINTF "', folder '" TSTRING_PRINTF "', hr=0x%08x", strArchive.c_str(), strFolder.c_str(), hr);
			goto exit;
		}

		// see if entry is in list of attached archives.
		iObjEntry = std::find_if(lstArchives.begin(), lstArchives.end(), Predicates::SObjectEntry_equals_compareEntryId(m_ptrSession->GetMAPISession(), objEntry));
		if (iObjEntry == lstArchives.end()) {
			// Found a coupling that's not yet attached. Add it to the to-attach-list.
			m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "store '" TSTRING_PRINTF "', folder '" TSTRING_PRINTF "' not yet attached. Adding to coupling list", strArchive.c_str(), strFolder.c_str());
			lstCouplings.push_back(*i);
		} else {
			// Found a coupling that's already attached. Remove it from lstArchives, which is later processed to remove all
			// implicitly attached archives from it.
			m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "store '" TSTRING_PRINTF "', folder '" TSTRING_PRINTF "' already attached. Removing from post process list", strArchive.c_str(), strFolder.c_str());
			lstArchives.erase(iObjEntry);
		}
	}

	// Handle the automated archive stores
	for (std::list<tstring>::const_iterator i = info.lstServers.begin(); i != info.lstServers.end(); ++i) {
		entryid_t archiveId;
		ObjectEntryList::iterator iObjEntry;

		hr = m_ptrSession->GetArchiveStoreEntryId(info.userName, *i, &archiveId);
		if (hr == MAPI_E_NOT_FOUND) {
			m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "archive store for '" TSTRING_PRINTF "' on server '" TSTRING_PRINTF "' does not exist. Adding to server list", info.userName.c_str(), i->c_str());
			lstServers.push_back(*i);
			continue;
		}
		if (hr != hrSuccess) {
			m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Failed to get archive store id for '" TSTRING_PRINTF "' on server '" TSTRING_PRINTF "', hr=0x%08x", info.userName.c_str(), i->c_str(), hr);
			goto exit;
		}

		// see if entry is in list of attached archives (store entryid only)
		iObjEntry = std::find_if(lstArchives.begin(), lstArchives.end(), Predicates::storeId_equals_compareEntryId(m_ptrSession->GetMAPISession(), archiveId));
		if (iObjEntry == lstArchives.end()) {
			// Found a server/archive that's not yet attached. Add it to the to-attach-list.
			m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "archive store for '" TSTRING_PRINTF "' on server '" TSTRING_PRINTF "' not yet attached. Adding to server list", info.userName.c_str(), i->c_str());
			lstServers.push_back(*i);
		} else {
			// Found a server/archive that's already attached. Remove it from lstArchives, which is later processed to remove all
			// implicitly attached archives from it.
			m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "archive store for '" TSTRING_PRINTF "' on server '" TSTRING_PRINTF "' already attached. Removing from post process list", info.userName.c_str(), i->c_str());
			lstArchives.erase(iObjEntry);
		}
	}

	hr = RemoveImplicit(info.storeId, info.userName, abentryid_t(), lstArchives);
	if (hr != hrSuccess)
		goto exit;

	hr = AddCouplingBased(info.userName, lstCouplings, ulAttachFlags);
	if (hr != hrSuccess)
		goto exit;

	hr = AddServerBased(info.userName, userId, lstServers, ulAttachFlags);
	if (hr != hrSuccess)
		goto exit;

exit:
	return hr;
}

/**
 * Find the SObjectEntry for the archive specified by store- and foldername.
 * @param[in]	strArchive	The store name of the archive.
 * @param[in]	strFolder	The folder name of the archive.
 * @param[out]	lpObjEntry	The returned SObjectEntry.
 * @retval	MAPI_E_NOT_FOUND	The requested archive does not exist.
 */
HRESULT ArchiveStateUpdater::FindArchiveEntry(const tstring &strArchive, const tstring &strFolder, SObjectEntry *lpObjEntry)
{
	HRESULT hr = hrSuccess;
	MsgStorePtr ptrArchiveStore;
	ArchiveHelperPtr ptrArchiveHelper;

	hr = m_ptrSession->OpenStoreByName(strArchive, &ptrArchiveStore);
	if (hr != hrSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Failed to open store for user '" TSTRING_PRINTF "', hr=0x%08x", strArchive.c_str(), hr);
		goto exit;
	}

	hr = ArchiveHelper::Create(ptrArchiveStore, strFolder, NULL, &ptrArchiveHelper);
	if (hr != hrSuccess)
		goto exit;

	hr = ptrArchiveHelper->GetArchiveEntry(false, lpObjEntry);
	if (hr != hrSuccess) {
		if (hr != MAPI_E_NOT_FOUND)
			m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Failed to get archive entry for folder '" TSTRING_PRINTF "', hr=0x%08x", strFolder.c_str(), hr);
		goto exit;
	}

exit:
	return hr;
}
