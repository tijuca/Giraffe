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

#ifndef archivestateupdater_INCLUDED
#define archivestateupdater_INCLUDED

#include "archivestateupdater_fwd.h"
#include "archivestatecollector.h"

#include "userutil.h"
#include "archiver-session_fwd.h"
#include "archiver-common.h"
#include "tstring.h"

#include <boost/smart_ptr.hpp>
#include <list>
#include <map>

/**
 * This class updates the current archive state to the should-be state.
 */
class ArchiveStateUpdater {
public:
	typedef ArchiveStateCollector::ArchiveInfo		ArchiveInfo;
	typedef ArchiveStateCollector::ArchiveInfoMap	ArchiveInfoMap;

	static HRESULT Create(const SessionPtr &ptrSession, ECLogger *lpLogger, const ArchiveInfoMap &mapArchiveInfo, ArchiveStateUpdaterPtr *lpptrUpdater);

	virtual ~ArchiveStateUpdater();

	HRESULT UpdateAll(unsigned int ulAttachFlags);
	HRESULT Update(const tstring &userName, unsigned int ulAttachFlags);

private:
	ArchiveStateUpdater(const SessionPtr &ptrSession, ECLogger *lpLogger, const ArchiveInfoMap &mapArchiveInfo);

	HRESULT PopulateUserList();
	HRESULT PopulateFromContainer(LPABCONT lpContainer);

	HRESULT UpdateOne(const abentryid_t &userId, const ArchiveInfo& info, unsigned int ulAttachFlags);
	HRESULT RemoveImplicit(const entryid_t &storeId, const tstring &userName, const abentryid_t &userId, const ObjectEntryList &lstArchives);

	HRESULT ParseCoupling(const tstring &strCoupling, tstring *lpstrArchive, tstring *lpstrFolder);
	HRESULT AddCouplingBased(const tstring &userName, const std::list<tstring> &lstCouplings, unsigned int ulAttachFlags);
	HRESULT AddServerBased(const tstring &userName, const abentryid_t &userId, const std::list<tstring> &lstServers, unsigned int ulAttachFlags);
	HRESULT VerifyAndUpdate(const abentryid_t &userId, const ArchiveInfo& info, unsigned int ulAttachFlags);

	HRESULT FindArchiveEntry(const tstring &strArchive, const tstring &strFolder, SObjectEntry *lpObjEntry);

private:
	SessionPtr	m_ptrSession;
	ECLogger	*m_lpLogger;

	ArchiveInfoMap	m_mapArchiveInfo;
};

#endif // ndef archivestateupdater_INCLUDED
