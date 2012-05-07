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

#ifndef archivehelper_INCLUDED
#define archivehelper_INCLUDED

#include "archiver-common.h"

#include <boost/smart_ptr.hpp>
#include <string>

#include <CommonUtil.h>
#include <mapi_ptr.h>

#include "archiver-session_fwd.h"
#include "tstring.h"

class ECLogger;

namespace za { namespace helpers {

class ArchiveHelper;
typedef boost::shared_ptr<ArchiveHelper> ArchiveHelperPtr;

enum ArchiveType {
	UndefArchive = 0,
	SingleArchive = 1,
	MultiArchive = 2
};

enum AttachType {
	ExplicitAttach = 0,
	ImplicitAttach = 1
};

/**
 * The ArchiveHelper class is a utility class that operates on a message store that's used as
 * an archive.
 */
class ArchiveHelper
{
public:
	static HRESULT Create(LPMDB lpArchiveStore, const tstring &strFolder, const char *lpszServerPath, ArchiveHelperPtr *lpptrArchiveHelper);
	static HRESULT Create(LPMDB lpArchiveStore, LPMAPIFOLDER lpArchiveFolder, const char *lpszServerPath, ArchiveHelperPtr *lpptrArchiveHelper);
	static HRESULT Create(SessionPtr ptrSession, const SObjectEntry &archiveEntry, ECLogger *lpLogger, ArchiveHelperPtr *lpptrArchiveHelper);
	~ArchiveHelper();
	
	HRESULT GetAttachedUser(abentryid_t *lpsUserEntryId);
	HRESULT SetAttachedUser(const abentryid_t &sUserEntryId);
	HRESULT GetArchiveEntry(bool bCreate, SObjectEntry *lpsObjectEntry);

	HRESULT GetArchiveType(ArchiveType *lparchType, AttachType *lpattachType);
	HRESULT SetArchiveType(ArchiveType archType, AttachType attachType);
	
	HRESULT SetPermissions(const abentryid_t &sUserEntryId, bool bWritable);
	
	HRESULT GetArchiveFolderFor(MAPIFolderPtr &ptrSourceFolder, SessionPtr ptrSession, LPMAPIFOLDER *lppDestinationFolder);
	HRESULT GetHistoryFolder(LPMAPIFOLDER *lppHistoryFolder);
	HRESULT GetOutgoingFolder(LPMAPIFOLDER *lppOutgoingFolder);
	HRESULT GetDeletedItemsFolder(LPMAPIFOLDER *lppOutgoingFolder);

	HRESULT GetArchiveFolder(bool bCreate, LPMAPIFOLDER *lppArchiveFolder);
	HRESULT IsArchiveFolder(LPMAPIFOLDER lpFolder, bool *lpbResult);

	MsgStorePtr GetMsgStore() const { return m_ptrArchiveStore; }

	HRESULT PrepareForFirstUse(ECLogger *lpLogger = NULL);

private:
	ArchiveHelper(LPMDB lpArchiveStore, const tstring &strFolder, const std::string &strServerPath);
	ArchiveHelper(LPMDB lpArchiveStore, LPMAPIFOLDER lpArchiveFolder, const std::string &strServerPath);
	HRESULT Init();

	enum eSpecFolder {
		sfBase = 0,			//< The root of the special folders, which is a child of the archive root
		sfHistory = 1,		//< The history folder, which is a child of the special root
		sfOutgoing = 2,		//< The outgoing folder, which is a child of the special root
		sfDeleted = 3		//< The deleted items folder, which is a child of the special root
	};
	HRESULT GetSpecialFolderEntryID(eSpecFolder sfWhich, ULONG *lpcbEntryID, LPENTRYID *lppEntryID);
	HRESULT SetSpecialFolderEntryID(eSpecFolder sfWhich, ULONG cbEntryID, LPENTRYID lpEntryID);
	HRESULT GetSpecialFolder(eSpecFolder sfWhich, LPMAPIFOLDER *lppSpecialFolder);
	HRESULT CreateSpecialFolder(eSpecFolder sfWhich, LPMAPIFOLDER *lppSpecialFolder);
	HRESULT IsSpecialFolder(eSpecFolder sfWhich, LPMAPIFOLDER lpFolder, bool *lpbResult);

private:
	MsgStorePtr	m_ptrArchiveStore;
	MAPIFolderPtr m_ptrArchiveFolder;
	tstring	m_strFolder;
	const std::string m_strServerPath;
	
	PROPMAP_START
	PROPMAP_DEF_NAMED_ID(ATTACHED_USER_ENTRYID)
	PROPMAP_DEF_NAMED_ID(ARCHIVE_TYPE)
	PROPMAP_DEF_NAMED_ID(ATTACH_TYPE)
	PROPMAP_DEF_NAMED_ID(SPECIAL_FOLDER_ENTRYIDS)
};

}} // Namespaces

#endif // ndef archivehelper_INCLUDED
