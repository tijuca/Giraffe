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

#ifndef ECSYNCHRONIZATION_H
#define ECSYNCHRONIZATION_H

#include <list>

#include <ECUnknown.h>

#include "ECChangeData.h"
#include "zarafa-indexer.h"

class ECEntryData;
class ECFolderData;
class ECIndexerData;
class ECThreadData;

/**
 * Wrapper class which performs all required synchronization actions and collects all changes
 */
class ECSynchronization : public ECUnknown {
private:
	/**
	 * Constructor
	 *
	 * @note Objects of ECSynchronization must only be created using the Create() function.
	 *
	 * @param[in]	lpThreadData
	 * @param[in]	lpIndexerData
	 */
	ECSynchronization(ECThreadData *lpThreadData, ECIndexerData *lpIndexerData);

public:
	/**
	 * Create new ECSynchronization object
	 *
	 * @note Creating a new ECSynchronization object must always occur through this function.
	 *
	 * @param[in]	lpThreadData
	 *					Reference to the ECThreadData object.
	 * @param[in]	lpIndexerData
	 *					Reference to the ECIndexerData object.
	 * @param[out]	lppSynchronization
	 *					The created ECSynchronization object.
	 * @return HRESULT
	 */
	 static HRESULT Create(ECThreadData *lpThreadData, ECIndexerData *lpIndexerData, ECSynchronization **lppSynchronization);

	/**
	 * Destructor
	 */
	~ECSynchronization();

	/**
	 * Synchronize Address Book
	 *
	 * @param[in]	lpStore
	 *					IMsgStore belonging to SYSTEM user.
	 * @param[in]	lpSyncBase
	 *					Reference to IStream object which contains synchronization base.
	 * @return HRESULT
	 */
	HRESULT GetAddressBookChanges(IMsgStore *lpStore, LPSTREAM lpSyncBase);

	/**
	 * Synchronize hierarchy changes
	 *
	 * @param[in]	lpEntryData
	 *					Reference to ECEntryData object with information about the store we are synchronizing.
	 * @param[in]	lpMsgStore
	 *					Reference to IMsgStore of the store which must be synchronized.
	 * @param[in]	lpFolder
	 *					Root folder which hierarchy must be synchronized.
	 * @return HRESULT
	 */
	HRESULT GetHierarchyChanges(ECEntryData *lpEntryData, IMsgStore *lpMsgStore, IMAPIFolder *lpFolder);

	/**
	 * Synchronize contents changes
	 *
	 * @param[in]	lpEntryData
	 *					Reference to ECEntryData object with information about the store we are synchronizing.
	 * @param[in]	lpMsgStore
	 *					Reference to IMsgStore of the store which must be synchronized.
	 * @param[in]	lpRootFolder
	 *					Root folder which hierarchy contents must be synchronized.
	 * @param[in]	lpAdminSession
	 * 					The admin MAPI session.
	 * @return HRESULT
	 */
	HRESULT GetContentsChanges(ECEntryData *lpEntryData, IMsgStore *lpMsgStore, IMAPIFolder *lpRootFolder, IMAPISession *lpAdminSession);

	/**
	 * Synchronize contents changes
	 *
	 * @param[in]	lpEntryData
	 *					Reference to ECEntryData object with information about the store we are synchronizing.
	 * @param[in]	lpFolderData
	 *					Reference to ECFolderData object with information about the folder we are synchronizing.
	 * @param[in]	lpMsgStore
	 *					Reference to IMsgStore of the store which must be synchronized.
	 * @param[in]	lpFolder
	 *					Folder which contents must be synchronized.
	 * @param[in]	lpAdminSession
	 * 					The admin MAPI session.
	 * @return HRESULT
	 */
	HRESULT GetContentsChanges(ECEntryData *lpEntryData, ECFolderData *lpFolderData, IMsgStore *lpMsgStore, IMAPIFolder *lpFolder, IMAPISession *lpAdminSession);

	/**
	 * Start merged synchronization
	 *
	 * Normally every call to GetAddressBookChanges(), GetHierarchyChanges() or GetContentsChanges()
	 * is treated individually. If before a call StartChanges() is called, all subsequent calls to
	 * those functions are handled as a single batch until StopChanges() is called.
	 *
	 * @return HRESULT
	 */
	HRESULT StartMergedChanges();

	/**
	 * Stop merged synchronization
	 *
	 * Normally every call to GetAddressBookChanges(), GetHierarchyChanges() or GetContentsChanges()
	 * is treated individually. If before a call StartChanges() is called, all subsequent calls to
	 * those functions are handled as a single batch until StopChanges() is called.
	 *
	 * @return HRESULT
	 */
	HRESULT StopMergedChanges();
	
	/**
	 * Retreive all changed items detected during synchronization
	 *
	 * @param[out]	lppChanges
	 *					The detected changes during synchronization.
	 * @return HRESULT
	 */
	HRESULT GetNextChanges(ECChanges **lppChanges);

private:
	/**
	 * Create change data for a new synchronization command
	 *
	 * @param[in]	bMerge
	 *					If set to TRUE the no new change data will be
	 *					allocated if old data is still available. This
	 *					will cause the new changes to be merged with the
	 *					older set.
	 * @return HRESULT
	 */
	HRESULT CreateChangeData(BOOL bMerge);

private:
	ECThreadData *m_lpThreadData;
	ECIndexerData *m_lpIndexerData;
	ECChangeData *m_lpChanges;
	BOOL m_bStream;
	BOOL m_bMerged;
	BOOL m_bCompleted;
};

#endif /* ECSYNCHRONIZATION_H */
