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

#include <platform.h>

#include <cstdio>

#include <mapi.h>
#include <mapix.h>
#include <mapiutil.h>
#include <mapidefs.h>
#include <mapiguid.h>

#include <stringutil.h>
#include <Util.h>
#include <Zarafa.h>

#include "ECFileIndex.h"
#include "zarafa-indexer.h"

 #define DEFAULT_BASE_PATH	"/var/lib/zarafa/index/"

#define BASE_PREFIX			"base: "
#define BASE_PREFIX_LEN		strlen("base: ")

ECFileIndex::ECFileIndex(ECThreadData *lpThreadData)
{
	m_lpThreadData = lpThreadData;
	m_strBasePath = m_lpThreadData->lpConfig->GetSetting("index_path");
	if (m_strBasePath.empty())
		m_strBasePath = DEFAULT_BASE_PATH;
}

ECFileIndex::~ECFileIndex()
{
}

HRESULT ECFileIndex::StoreCreate(const std::string &strStorePath)
{
	HRESULT hr = hrSuccess;

	if (CreatePath(strStorePath.c_str()) != 0)
		hr = MAPI_E_DISK_ERROR;

	return hr;
}

HRESULT ECFileIndex::SetStoreFolderSyncBase(const std::string &strFolderPath, ULONG ulData, LPBYTE lpData)
{
	HRESULT hr = hrSuccess;
	FILE *lpFile = NULL;
	std::string strOutput;

	strOutput = bin2hex(ulData, lpData);
	if (strOutput.empty()) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	lpFile = fopen(strFolderPath.c_str(), "w");
	if (!lpFile) {
		hr = MAPI_E_CALL_FAILED;
		goto exit;
	}

	/* Add prefix before writing out data */
	strOutput = BASE_PREFIX + strOutput;

	if (fwrite(strOutput.c_str(), strOutput.size(), 1, lpFile) != 1) {
		hr = MAPI_E_CALL_FAILED;
		goto exit;
	}

exit:
	if (lpFile)
		fclose(lpFile);

	if (hr != hrSuccess)
		m_lpThreadData->lpLogger->Log(EC_LOGLEVEL_FATAL, "Cannot update synchronization base for folder %s",
									  strFolderPath.c_str());

	return hr;
}

HRESULT ECFileIndex::GetStoreFolderSyncBase(const std::string &strFolderPath, ULONG *lpulData, LPBYTE *lppData)
{
	HRESULT hr = hrSuccess;
	FILE *lpFile = NULL;
	char *lpRead = NULL;
	char *lpBuff = NULL;
	ULONG ulReadSize = 0;
	ULONG ulSyncSize = 0;

	lpFile = fopen(strFolderPath.c_str(), "r");
	if (lpFile) {
		/* Determine the amount of data to read */
		fseek(lpFile, 0, SEEK_END);
		ulReadSize = ftell(lpFile);
		fseek(lpFile, 0, SEEK_SET);
	}
	if (ulReadSize) {
		/* Increase ulReadSize a bit, fgets will read ulReadSize - 1 */
		ulReadSize += sizeof('\0');

		hr = MAPIAllocateBuffer(ulReadSize, (LPVOID *)&lpRead);
		if (hr != hrSuccess)
			goto exit;

		/* Read base from file, search for correct position */
		while (!ferror(lpFile) && !feof(lpFile)) {
			if (fgets(lpRead, ulReadSize, lpFile) == NULL)
				break;

			if (strnicmp(lpRead, BASE_PREFIX, BASE_PREFIX_LEN) == 0) {
				lpBuff = &lpRead[BASE_PREFIX_LEN];
				ulSyncSize = strlen(lpBuff);
				break;
			}
		}

		/* A valid syncbase must be a multiple of 4 bytes stored as hex */
		if (ulSyncSize % (2 * sizeof(ULONG))) {
			m_lpThreadData->lpLogger->Log(EC_LOGLEVEL_FATAL, "Invalid synchronization base for folder %s",
										  strFolderPath.c_str());
			lpBuff = NULL;
			ulSyncSize = 0;
		}
	}

	if (!lpBuff) {
		/* Default base, start at beginning */
		lpBuff = "0000000000000000";
		ulSyncSize = 2 * 2 * sizeof(ULONG); /* hex: ulChangeId + ulSyncId */
	}

	hr = Util::hex2bin(lpBuff, ulSyncSize, lpulData, lppData);
	if (hr != hrSuccess)
		goto exit;

exit:
	if (lpRead)
		MAPIFreeBuffer(lpRead);

	if (lpFile)
		fclose(lpFile);

	return hr;
}

HRESULT ECFileIndex::GetStorePath(const std::string &strServerSignature, const std::string &strStoreEntryId, std::string *lpstrStorePath)
{
	HRESULT hr = hrSuccess;
	ULONG ulOffset = 0;
	CHAR szVersion = ' ';

	/* Set base path which consists of configured base and server signature. */
	lpstrStorePath->assign(m_strBasePath + PATH_SEPARATOR + strServerSignature + PATH_SEPARATOR);

	/* Discover store part of entryid but strip it from any postfixed data. */
	ulOffset = offsetof1(EID, ulVersion) * 2;
	if (strStoreEntryId.size() < ulOffset) {
		hr = MAPI_E_INVALID_ENTRYID;
		goto exit;
	}

	/*
	 * Although the version field has size (sizeof(((EID *)NULL)->ulVersion) * 2), we are
	 * only interested in the second byte to tell us the exact verion.
	 */
	szVersion = strStoreEntryId[ulOffset + 1];
	if (szVersion == '0')
		ulOffset = (sizeof(EID_V0) - 4) * 2;
	else if (szVersion == '1')
		ulOffset = (sizeof(EID) - 4) * 2;
	else
		ulOffset = 0;

	/* We have the exact size of the entryid, now we can append it to the storepath. */
	lpstrStorePath->append(strStoreEntryId, 0, ulOffset);

exit:
	return hr;
}

HRESULT ECFileIndex::GetStoreFolderInfoPath(const std::string &strStorePath, const std::string &strFolderEntryId, std::string *lpstrFolderPath)
{
	HRESULT hr = hrSuccess;
	lpstrFolderPath->assign(strStorePath + PATH_SEPARATOR + strFolderEntryId);
	return hr;
}

HRESULT ECFileIndex::GetStoreIndexPath(const std::string &strStorePath, std::string *lpstrIndexPath)
{
	HRESULT hr = hrSuccess;
	lpstrIndexPath->assign(strStorePath + PATH_SEPARATOR + "index");
	return hr;
}
