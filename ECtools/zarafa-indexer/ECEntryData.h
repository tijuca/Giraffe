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

#ifndef ECENTRYDATA_H
#define ECENTRYDATA_H

#include <list>
#include <string>

#include <ECUnknown.h>
#include "tstring.h"

class ECEntryData;

/**
 * Collection of properties for a folder.
 *
 * This class contains the EntryID/SourceKey for
 * the indexed folder in order to open the folder
 * when it must be indexed. It also contains the stream
 * in which the current synchronization state is stored.
 */
class ECFolderData : public ECUnknown {
private:
	/**
	 * Constructor
	 *
	 * Objects of ECFolderData must only be created using the Create() function.
	 */
	ECFolderData();
public:
	/**
	 * Create new ECFolderData object
	 *
	 * @note Creating a new ECFolderData object must always occur through this function.
	 *		 A reference to a ECEntryData object is required because all ECFolderData
	 *		 must be attached to a ECEntryData object upon creation.
	 *
	 * @param[in]	lpEntryData
	 *					The ECEntryData object to which this folder must be attached
	 * @param[out]	lppFolderData
	 *					The created ECFolderData object reference
	 * @return HRESULT
	 */
	static HRESULT Create(ECEntryData *lpEntryData, ECFolderData **lppFolderData);

	/**
	 * Destructor
	 */
	~ECFolderData();

	/**
	 * Sourcekey of the folder.
	 * May only be initialized by the HrAddEntryData() function.
	 */
	SBinary m_sFolderSourceKey;

	/**
	 * EntryID of the folder.
	 * May only be initialized by the HrAddEntryData() function.
	 */
	SBinary m_sFolderEntryId;

	/**
	 * Foldername, only used for debug messages.
	 */
	tstring m_strFolderName;

	/**
	 * Path to the harddisk file where to store the synchronization base physically
	 */
	std::string m_strFolderPath;

	/**
	 * Synchronization base for contents changes in this folder.
	 */
	LPSTREAM m_lpContentsSyncBase;
};

typedef std::list<ECFolderData *> folderdata_list_t;

/**
 * Collection of properties for a user store.
 *
 * This class contains the EntryID/SourceKey for
 * the indexed store in order to open the store
 * when it must be indexed. It also contains the stream
 * in which the current synchronization state is stored
 * for the hierarchy changes. Contents changes can be
 * tracked through the list of ECFolderData objects.
 */
class ECEntryData : public ECUnknown {
private:
	/**
	 * Constructor
	 *
	 * Objects of ECEntryData must only be created using the Create() function.
	 */
	ECEntryData();
public:
	/**
	 * Create new ECEntryData object
	 *
	 * @note Creating a new ECEntryData object must always occur through this
	 *		 this function.
	 *
	 * @param[out]	lppEntryData
	 *					The created ECEntryData object reference
	 * @return HRESULT
	 */
	static HRESULT Create(ECEntryData **lppEntryData);

	/**
	 * Destructor
	 */
	~ECEntryData();

	/**
	 * This entry is for the public store
	 */
	BOOL m_bPublicFolder;

	/**
	 * EntryID of the user to which the store belongs.
	 * May only be initialized by the HrAddEntryData() function.
	 */
	SBinary m_sUserEntryId;

	/**
	 * EntryID of the store.
	 * May only be initialized by the HrAddEntryData() function.
	 */
	SBinary m_sStoreEntryId;

	/**
	 * EntryID of the root folder of the store.
	 * May only be initialized by the HrAddEntryData() function.
	 */
	SBinary m_sRootEntryId;

	/**
	 * Sourcekey of the root folder of the store.
	 * May only be initialized by the HrAddEntryData() function.
	 */
	SBinary m_sRootSourceKey;

	/**
	 * Username of store owner, only used for debug messages.
	 */
	tstring m_strUserName;

	/**
	 * Path to harddisk directory under which all index files will be physically stored.
	 */
	std::string m_strStorePath;

	/**
	 * Name of the server which is hosting this store.
	 */
	tstring m_strServerName;

	/**
	 * Synchronization base for hierarchy changes in this folder.
	 */
	LPSTREAM m_lpHierarchySyncBase;

	/**
	 * List of ECFolderData objects which are member of this store,
	 * new entries may only be added through the ECFolderData::Create() function.
	 */
	folderdata_list_t m_lFolders;
};

typedef std::list<ECEntryData *> entrydata_list_t;

/**
 * Copy SBinary data into a ECEntryData or ECFolderData structure
 *
 * @param[in]	lpDst
 *					Reference to destination SBinary structure.
 * @param[in]	lpEntry
 *					Reference to ECEntryData which acts as parent for
 *					the lpDst parameter. Note that using this function
 *					for a ECFolderData structure requires this parameter
 *					to be set to the parent of the ECFolderData object.
 * @param[in]	lpSrc
 *					Reference to the source SBinary structure.
 * @return HRESULT
 */
HRESULT HrAddEntryData(SBinary *lpDst, ECEntryData *lpEntry, SBinary *lpSrc);

#endif /* ECENTRYDATA_H */
