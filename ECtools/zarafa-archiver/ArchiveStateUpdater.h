/*
 * Copyright 2005 - 2015  Zarafa B.V. and its licensors
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License, version 3,
 * as published by the Free Software Foundation with the following
 * additional terms according to sec. 7:
 * 
 * "Zarafa" is a registered trademark of Zarafa B.V.
 * The licensing of the Program under the AGPL does not imply a trademark 
 * license. Therefore any rights, title and interest in our trademarks 
 * remain entirely with us.
 * 
 * Our trademark policy (see TRADEMARKS.txt) allows you to use our trademarks
 * in connection with Propagation and certain other acts regarding the Program.
 * In any case, if you propagate an unmodified version of the Program you are
 * allowed to use the term "Zarafa" to indicate that you distribute the Program.
 * Furthermore you may use our trademarks where it is necessary to indicate the
 * intended purpose of a product or service provided you use it in accordance
 * with honest business practices. For questions please contact Zarafa at
 * trademark@zarafa.com.
 *
 * The interactive user interface of the software displays an attribution 
 * notice containing the term "Zarafa" and/or the logo of Zarafa. 
 * Interactive user interfaces of unmodified and modified versions must 
 * display Appropriate Legal Notices according to sec. 5 of the GNU Affero 
 * General Public License, version 3, when you propagate unmodified or 
 * modified versions of the Program. In accordance with sec. 7 b) of the GNU 
 * Affero General Public License, version 3, these Appropriate Legal Notices 
 * must retain the logo of Zarafa or display the words "Initial Development 
 * by Zarafa" if the display of the logo is not reasonably feasible for
 * technical reasons.
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

#ifndef ARCHIVESTATEUPDATER_H_INCLUDED
#define ARCHIVESTATEUPDATER_H_INCLUDED

#include "ArchiveStateCollector.h"

/**
 * This class updates the current archive state to the should-be state.
 */
class ArchiveStateUpdater {
public:
	typedef ArchiveStateCollector::ArchiveInfo		ArchiveInfo;
	typedef ArchiveStateCollector::ArchiveInfoMap	ArchiveInfoMap;

	static HRESULT Create(const ArchiverSessionPtr &ptrSession, ECLogger *lpLogger, const ArchiveInfoMap &mapArchiveInfo, ArchiveStateUpdaterPtr *lpptrUpdater);

	virtual ~ArchiveStateUpdater();

	HRESULT UpdateAll(unsigned int ulAttachFlags);
	HRESULT Update(const tstring &userName, unsigned int ulAttachFlags);

private:
	ArchiveStateUpdater(const ArchiverSessionPtr &ptrSession, ECLogger *lpLogger, const ArchiveInfoMap &mapArchiveInfo);

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
	ArchiverSessionPtr	m_ptrSession;
	ECLogger	*m_lpLogger;

	ArchiveInfoMap	m_mapArchiveInfo;
};

#endif // !defined ARCHIVESTATEUPDATER_H_INCLUDED
