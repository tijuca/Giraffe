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

#ifndef copier_INCLUDED
#define copier_INCLUDED

#include "operations.h"
#include "postsaveaction.h"
#include "transaction_fwd.h"
#include "instanceidmapper_fwd.h"
#include "archiver-session_fwd.h"
#include "archiver-common.h"
#include <map>
#include <boost/smart_ptr.hpp>

class ECConfig;

namespace za { namespace operations {

/**
 * Performs the copy part of the archive operation.
 */
class Copier : public ArchiveOperationBaseEx
{
public:
	Copier(SessionPtr ptrSession, ECConfig *lpConfig, ECArchiverLogger *lpLogger, const ObjectEntryList &lstArchives, LPSPropTagArray lpExcludeProps, int ulAge, bool bProcessUnread);
	~Copier();

	/**
	 * Override ArchiveOperationBaseEx's GetRestriction to add some more
	 * magic.
	 */
	HRESULT GetRestriction(LPMAPIPROP lpMapiProp, LPSRestriction *lppRestriction);

	/**
	 * Set the operation that will perform the deletion if required.
	 * @param[in]	ptrDeleteOp		The delete operation.
	 */
	void SetDeleteOperation(DeleterPtr ptrDeleteOp);
	
	/**
	 * Set the operation that will perform the stubbing if required.
	 * @param[in]	ptrStubOp		The stub operation.
	 */
	void SetStubOperation(StubberPtr ptrStubOp);

public:
	class Helper // For lack of a better name
	{
	public:
		Helper(SessionPtr ptrSession, ECLogger *lpLogger, const InstanceIdMapperPtr &ptrMapper, LPSPropTagArray lpExcludeProps, LPMAPIFOLDER lpFolder);

		/**
		 * Create a copy of a message in the archive, effectively archiving the message.
		 * @param[in]	lpSource		The message to archive.
		 * @param[in]	archiveEntry	SObjectEntry specifying the archive to archive in.
		 * @param[in]	refMsgEntry		SObejctEntry referencing the original message (used as a back reference from the archive).
		 * @param[out]	lppArchivedMsg	The new message.
		 */
		HRESULT CreateArchivedMessage(LPMESSAGE lpSource, const SObjectEntry &archiveEntry, const SObjectEntry &refMsgEntry, LPMESSAGE *lppArchivedMsg, PostSaveActionPtr *lpptrPSAction);

		/**
		 * Get the folder that acts as root for an archive.
		 * @param[in]	archiveEntry		SObjectEntry specifying the archive to archive.
		 * @param[out]	lppArchiveFolder	The archive root folder.
		 */
		HRESULT GetArchiveFolder(const SObjectEntry &archiveEntry, LPMAPIFOLDER *lppArchiveFolder);

		/**
		 * Copy the message to the archive and setup the special properties.
		 * @param[in]	lpSource		The message to archive.
		 * @param[in]	lpMsgEntry		SObejctEntry referencing the original message (used as a back reference from the archive).
		 * @param[in]	lpDest			The message to archive to.
		 */
		HRESULT ArchiveMessage(LPMESSAGE lpSource, const SObjectEntry *lpMsgEntry, LPMESSAGE lpDest, PostSaveActionPtr *lpptrPSAction);

		/**
		 * Update the single instance IDs of the destination message based on
		 * existing mappings of instance IDs stored in previous runs.
		 * @param[in]	lpSource		The reference message.
		 * @param[in]	lpDest			The message to update.
		 */
		HRESULT UpdateIIDs(LPMESSAGE lpSource, LPMESSAGE lpDest, PostSaveActionPtr *lpptrPSAction);

		/**
		 * Get the Session instance associated with this instance.
		 */
		SessionPtr& GetSession() { return m_ptrSession; }

	private:
		typedef std::map<entryid_t,MAPIFolderPtr> ArchiveFolderMap;
		ArchiveFolderMap m_mapArchiveFolders;

		SessionPtr m_ptrSession;
		ECLogger *m_lpLogger;
		LPSPropTagArray m_lpExcludeProps;
		MAPIFolderPtr m_ptrFolder;
		InstanceIdMapperPtr m_ptrMapper;
	};

private:
	HRESULT EnterFolder(LPMAPIFOLDER lpFolder);
	HRESULT LeaveFolder();
	HRESULT DoProcessEntry(ULONG cProps, const LPSPropValue &lpProps);

private:
	/**
	 * Perform an initial archive of a message. This will be used to archive
	 * a message that has not been archived before.
	 *
	 * @param[in]	lpMessage			The message to archive
	 * @param[in]	archiveRootEntry	The SObjectEntry describing the archive root folder
	 * @param[in]	refMsgEntry			The SObjectEntry describing the message to be archived
	 * @param[out]	lpptrTransaction	A Transaction object used to save and delete the proper messages when everything is setup
	 */
	HRESULT DoInitialArchive(LPMESSAGE lpMessage, const SObjectEntry &archiveRootEntry, const SObjectEntry &refMsgEntry, TransactionPtr *lpptrTransaction);

	/**
	 * Track an existing archive and create a new archive of a message. This is used for the
	 * track_history option and will move the old archive to the history folder while creating
	 * a new archive message that will reference that old archive.
	 *
	 * @param[in]	lpMessage			The message to archive
	 * @param[in]	archiveRootEntry	The SObjectEntry describing the archive root folder
	 * @param[in]	archiveMsgEntry		The SObjectEntry describing the existing archive message
	 * @param[in]	refMsgEntry			The SObjectEntry describing the message to be archived
	 * @param[in]	bUpdateHistory		If true, update the history references
	 * @param[out]	lpptrTransaction	A Transaction object used to save and delete the proper messages when everything is setup
	 */
	HRESULT DoTrackAndRearchive(LPMESSAGE lpMessage, const SObjectEntry &archiveRootEntry, const SObjectEntry &archiveMsgEntry, const SObjectEntry &refMsgEntry, bool bUpdateHistory, TransactionPtr *lpptrTransaction);

	/**
	 * Update an existing archive of a message. This is used for dirty messages when the track_history
	 * option is disabled.
	 *
	 * @param[in]	lpMessage			The message to archive
	 * @param[in]	archiveMsgEntry		The SObjectEntry describing the existing archive message
	 * @param[in]	refMsgEntry			The SObjectEntry describing the message to be archived
	 * @param[out]	lpptrTransaction	A Transaction object used to save and delete the proper messages when everything is setup
	 */
	HRESULT DoUpdateArchive(LPMESSAGE lpMessage, const SObjectEntry &archiveMsgEntry, const SObjectEntry &refMsgEntry, TransactionPtr *lpptrTransaction);

	/**
	 * Move an archived message from one archive folder to another. This is only needed when the
	 * message in the primary store has been moved. The references in old archives will automatically
	 * be updated.
	 *
	 * @note This function actually creates a copy of the message and delete the old message through
	 *       the transaction system. Otherwise no rollback can be guaranteed.
	 *
	 * @param[in]	archiveRootEntry	The SObjectEntry describing the archive root folder
	 * @param[in]	archiveMsgEntry		The SObjectEntry describing the existing archive message
	 * @param[in]	refMsgEntry			The SObjectEntry describing the message to be archived
	 * @param[out]	lpptrTransaction	A Transaction object used to save and delete the proper messages when everything is setup
	 */
	HRESULT DoMoveArchive(const SObjectEntry &archiveRootEntry, const SObjectEntry &archiveMsgEntry, const SObjectEntry &refMsgEntry, TransactionPtr *lpptrTransaction);

	/**
	 * Execute the delete or stub operation if available. If both are set the delete operation
	 * has precedence if the message matches the restriction set for it. If non of the operations
	 * restriction match for the message, nothing will be done.
	 *
	 * @param[in]	lpMessage		The message to process
	 * @param[in]	lpFolder		The parent folder
	 * @param[in]	cProps			The amount of props found in lpProps
	 * @param[in]	lpProps			A list of properties containing the information to open the correct message.
	 */
	HRESULT ExecuteSubOperations(LPMESSAGE lpMessage, LPMAPIFOLDER lpFolder, ULONG cProps, const LPSPropValue lpProps);

	/**
	 * Move an archive message to the special history folder.
	 * 
	 * @param[in]	sourceArchiveRoot	The SObjectEntry describing the archive root folder
	 * @param[in]	sourceMsgEntry		The SObjectEntry describing the archive message to move
	 * @param[in]	ptrTransaction		A Transaction object used to save and delete the proper messages when everything is setup
	 * @param[out]	lpNewEntry			The SObjectEntry describing the moved message.
	 * @param[out]	lppNewMessage		The newly created message. This argument is allowed to be NULL
	 */
	HRESULT MoveToHistory(const SObjectEntry &sourceArchiveRoot, const SObjectEntry &sourceMsgEntry, TransactionPtr ptrTransaction, SObjectEntry *lpNewEntry, LPMESSAGE *lppNewMessage);

	/**
	 * Open the history message referenced by lpArchivedMsg and update it's reference. Continue doing that for all
	 * history messages.
	 *
	 * @param[in]	lpArchivedMsg		The archived message who's predecessor to update.
	 * @param[in]	refMsgEntry			The SObjectEntry describing to reference
	 * @param[in]	ptrTransaction		A Transaction object used to save and delete the proper messages when everything is setup
	 */
	HRESULT UpdateHistoryRefs(LPMESSAGE lpArchivedMsg, const SObjectEntry &refMsgEntry, TransactionPtr ptrTransaction);

private:
	SessionPtr m_ptrSession;
	ECConfig *m_lpConfig;
	ObjectEntryList m_lstArchives;
	SPropTagArrayPtr m_ptrExcludeProps;

	DeleterPtr m_ptrDeleteOp;
	StubberPtr m_ptrStubOp;

	typedef std::auto_ptr<Helper> HelperPtr;
	HelperPtr m_ptrHelper;

	TransactionPtr m_ptrTransaction;
	InstanceIdMapperPtr m_ptrMapper;
};

}} // namespaces

#endif // ndef copier_INCLUDED
