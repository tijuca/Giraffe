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

#ifndef ECFILEINDEX_H
#define ECFILEINDEX_H

#include <string>

class ECThreadData;

/**
 * Wrapper class for all physical harddisk access.
 *
 * This class contains all functions for writing folder synchronization
 * bases to the harddisk, determining index path and creating the index folder.
 */
class ECFileIndex {
public:
	/**
	 * Constructor
	 *
	 * @param[in]	lpThreadData
	 */
	ECFileIndex(ECThreadData *lpThreadData);

	/**
	 * Destructor
	 */
	~ECFileIndex();

	/**
	 * Create store folder.
	 *
	 * @param[in]	strStorePath
	 *					Full path which should be created recursively.
	 * @return HRESULT
	 */
	HRESULT StoreCreate(const std::string &strStorePath);

	/**
	 * Write synchronization base for folder to disk
	 *
	 * This writes the synchronization base for a particular folder to disk, it can be
	 * be read later by using GetStoreFolderSyncBase(). By storing the synchronization base
	 * to disk will allow to service to start from the correct position after a restart.
	 *
	 * @param[in]	strFolderPath
	 *					Full path to the file in which the synchronization base should be written.
	 * @param[in]	ulData
	 *					Number of bytes in the lpData argument.
	 * @param[in]	lpData
	 *					Byte array containing the synchronization base which should be written to disk.
	 * @return HRESULT
	 */
	HRESULT SetStoreFolderSyncBase(const std::string &strFolderPath, ULONG ulData, LPBYTE lpData);

	/**
	 * Read synchronization base for folder from disk
	 *
	 * @param[in]	strFolderPath
	 *					Full path to the file in which the synchronization base is stored.
	 * @param[out]	lpulData
	 *					Number of bytes in the lppData argument.
	 * @param[out]	lppData
	 *					Byte array containing the synchronization base which was read from disk.
	 * @return HRESULT
	 */
	HRESULT GetStoreFolderSyncBase(const std::string &strFolderPath, ULONG *lpulData, LPBYTE *lppData);

	/**
	 * Construct location for store folder
	 *
	 * Beneath the store folder all synhronization bases for each folder in the
	 * store will be kept. It also acts as base folder for the CLucene index files/folders.
	 *
	 * @param[in]	strServerSignature
	 *					Signature of the server on which the store is stored
	 * @param[in]	strStoreEntryId
	 *					Entryid of the store
	 * @param[out]	lpstrStorePath
	 *					Full path of the store folder
	 * @return HRESULT
	 */
	HRESULT GetStorePath(const std::string &strServerSignature, const std::string &strStoreEntryId, std::string *lpstrStorePath);

	/**
	 * Construct location of the folder info file
	 *
	 * The folder info file is located beneath the store folder and contains the synchronization
	 * base for that particular folder.
	 *
	 * @param[in]	strStorePath
	 *					Full path of the store folder (as generated by GetStorePath()).
	 * @param[in]	strFolderEntryId
	 *					Entryid of the folder.
	 * @param[out]	lpstrFolderPath
	 *					Full path to the folder info file.
	 * @return HRESULT
	 */
	HRESULT GetStoreFolderInfoPath(const std::string &strStorePath, const std::string &strFolderEntryId, std::string *lpstrFolderPath);

	/**
	 * Construct location of the CLucene index folder
	 *
	 * The CLucene index folder is located beneath the store folder and contains all CLucene files
	 * for that particular store.
	 *
	 * @param[in]	strStorePath
	 *					Full path of the store folder (as generated by GetStorePath()).
	 * @param[out]	lpstrIndexPath
	 *					Full path to the CLucene index folder.
	 * @return HRESULT
	 */
	HRESULT GetStoreIndexPath(const std::string &strStorePath, std::string *lpstrIndexPath);

private:
	ECThreadData *m_lpThreadData;
	std::string m_strBasePath;
};

#endif /* ECFILEINDEX_H */
