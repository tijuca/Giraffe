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

#ifndef CLIENTUTIL_H
#define CLIENTUTIL_H

#include <mapispi.h>
#include <string>
#include "ECTags.h"
#include "edkmdb.h"
#include "tstring.h"

class WSTransport;

// Indexes of sptaZarafaProfile property array
enum ePropZarafaProfileColumns
{
	PZP_EC_PATH,
	PZP_PR_PROFILE_NAME,
	PZP_EC_USERNAME_A,
	PZP_EC_USERNAME_W,
	PZP_EC_USERPASSWORD_A,
	PZP_EC_USERPASSWORD_W,
	PZP_EC_FLAGS,
	PZP_EC_SSLKEY_FILE,
	PZP_EC_SSLKEY_PASS,
	PZP_EC_PROXY_HOST,
	PZP_EC_PROXY_PORT,
	PZP_EC_PROXY_USERNAME,
	PZP_EC_PROXY_PASSWORD,
	PZP_EC_PROXY_FLAGS,
	PZP_EC_CONNECTION_TIMEOUT,
	PZP_EC_OFFLINE_PATH_A,
	PZP_EC_OFFLINE_PATH_W,
	PZP_PR_SERVICE_NAME,
	NUM_ZARAFAPROFILE_PROPS		// Array size
};

// Zarafa profile properties
const static SizedSPropTagArray(NUM_ZARAFAPROFILE_PROPS, sptaZarafaProfile) = {
	NUM_ZARAFAPROFILE_PROPS,
	{
		PR_EC_PATH,
		PR_PROFILE_NAME_A,
		PR_EC_USERNAME_A,
		PR_EC_USERNAME_W,
		PR_EC_USERPASSWORD_A,
		PR_EC_USERPASSWORD_W,
		PR_EC_FLAGS,
		PR_EC_SSLKEY_FILE,
		PR_EC_SSLKEY_PASS,
		PR_EC_PROXY_HOST,
		PR_EC_PROXY_PORT,
		PR_EC_PROXY_USERNAME,
		PR_EC_PROXY_PASSWORD,
		PR_EC_PROXY_FLAGS,
		PR_EC_CONNECTION_TIMEOUT,
		PR_EC_OFFLINE_PATH_A,
		PR_EC_OFFLINE_PATH_W,
		PR_SERVICE_NAME
	}
};

struct sGlobalProfileProps
{
	std::string		strServerPath;
	std::string		strProfileName;
	std::wstring	strUserName;
	std::wstring	strPassword;
	ULONG			ulProfileFlags;
	std::string		strSSLKeyFile;
	std::string		strSSLKeyPass;
	ULONG			ulConnectionTimeOut;
	ULONG			ulProxyFlags;
	std::string		strProxyHost;
	ULONG			ulProxyPort;
	std::string		strProxyUserName;
	std::string		strProxyPassword;
	tstring			strOfflinePath;
	bool			bIsEMS;
};

class ClientUtil {
public:
	static HRESULT	HrInitializeStatusRow (const char * lpszProviderDisplay, ULONG ulResourceType, LPMAPISUP lpMAPISup, LPSPropValue lpspvIdentity, ULONG ulFlags);
	static HRESULT	HrSetIdentity(WSTransport *lpTransport, LPMAPISUP lpMAPISup, LPSPropValue* lppIdentityProps);

	static HRESULT ReadReceipt(ULONG ulFlags, LPMESSAGE lpReadMessage, LPMESSAGE* lppEmptyMessage);

	// Get the global zarafa properties
	static HRESULT GetGlobalProfileProperties(LPPROFSECT lpGlobalProfSect, struct sGlobalProfileProps* lpsProfileProps);
	static HRESULT GetGlobalProfileProperties(LPMAPISUP lpMAPISup, struct sGlobalProfileProps* lpsProfileProps);

	// Get the deligate stores from the global profile
	static HRESULT GetGlobalProfileDeligateStoresProp(LPPROFSECT lpGlobalProfSect, ULONG* lpcDeligates, LPBYTE* lppDeligateStores);

	// Get MSEMS emulator config
	static HRESULT GetConfigPath(std::string *lpConfigPath);
	// Convert MSEMS profile properties into ZARAFA profile properties
	static HRESULT ConvertMSEMSProps(ULONG cValues, LPSPropValue pValues, ULONG *lpcValues, LPSPropValue *lppProps);

};

HRESULT HrCreateEntryId(GUID guidStore, unsigned int ulObjType, ULONG* lpcbEntryId, LPENTRYID* lppEntryId);
HRESULT HrGetServerURLFromStoreEntryId(ULONG cbEntryId, LPENTRYID lpEntryId, char** lppServerPath, bool *lpbIsPseudoUrl);
HRESULT HrResolvePseudoUrl(WSTransport *lpTransport, char *lpszUrl, std::string *lpstrServerPath, bool *lpbIsPeer);
HRESULT HrCompareEntryIdWithStoreGuid(ULONG cbEntryID, LPENTRYID lpEntryID, LPCGUID guidStore);

enum enumPublicEntryID { ePE_None, ePE_IPMSubtree, ePE_Favorites, ePE_PublicFolders, ePE_FavoriteSubFolder };

HRESULT GetPublicEntryId(enumPublicEntryID ePublicEntryID, GUID guidStore, void *lpBase, ULONG *lpcbEntryID, LPENTRYID *lppEntryID);

BOOL CompareMDBProvider(LPBYTE lpguid, const GUID *lpguidZarafa);
BOOL CompareMDBProvider(MAPIUID* lpguid, const GUID *lpguidZarafa);

#endif
