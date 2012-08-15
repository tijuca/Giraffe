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

#include "ECDatabaseUtils.h"
#include "ECSessionManager.h"
#include "ECPluginFactory.h"
#include "ECDBDef.h"
#include "ECDatabaseUpdate.h"
#include "ECLicenseClient.h"
#include "ECGuid.h"
#include "ECLockManager.h"

#include "soapH.h"

#include <mapidefs.h>
#include <mapitags.h>

#include <sys/times.h>
#include <time.h>

#include <algorithm>
#include <sstream>
#include <set>
#include <deque>
#include <algorithm>
#include <stdio.h>

#include "ECTags.h"
#include "stringutil.h"
#include "SOAPUtils.h"
#include "ZarafaCode.h"
#include "Trace.h"
#include "ZarafaCmd.nsmap"
#include "ECFifoBuffer.h"
#include "ECSerializer.h"
#include "StreamUtil.h"
#include "CommonUtil.h"
#include "StorageUtil.h"

#include "ZarafaICS.h"

#include "Zarafa.h"
#include "ZarafaUtil.h"
#include "md5.h"

#include "ECAttachmentStorage.h"
#include "ECGenProps.h"
#include "ECUserManagement.h"
#include "ECSecurity.h"
#include "ECICS.h"
#include "ECMultiStoreTable.h"
#include "ECStatsCollector.h"
#include "ECStringCompat.h"
#include "ECTPropsPurge.h"
#include "ZarafaVersions.h"
#include "ECTestProtocol.h"
#include "ECTPropsPurge.h"

#include "ECDefs.h"
#include <EMSAbTag.h>
#include <edkmdb.h>
#include "ecversion.h"
#include "mapiext.h"

#include "../server/ECSoapServerConnection.h"

#include "ZarafaCmdUtil.h"
#include "ECThreadPool.h"

#define STRIN_FIX(s) (bSupportUnicode ? (s) : ECStringCompat::WTF1252_to_UTF8(soap, (s)))
#define STROUT_FIX(s) (bSupportUnicode ? (s) : ECStringCompat::UTF8_to_WTF1252(soap, (s)))
#define STROUT_FIX_CPY(s) (bSupportUnicode ? s_strcpy(soap, (s)) : ECStringCompat::UTF8_to_WTF1252(soap, (s)))

#define LOG_SOAP_DEBUG(logger, _msg, ...) if (logger->Log(EC_LOGLEVEL_DEBUG|EC_LOGLEVEL_SOAP)) { logger->Log(EC_LOGLEVEL_DEBUG|EC_LOGLEVEL_SOAP, "soap: "_msg, ##__VA_ARGS__); }

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

extern ECSessionManager*	g_lpSessionManager;
extern ECStatsCollector*	g_lpStatsCollector;

// Hold the status of the softdelete purge system
bool g_bPurgeSoftDeleteStatus = FALSE;

double timespec2dbl(timespec t) {
    return (double)t.tv_sec + t.tv_nsec/1000000000.0;
}

ECRESULT CreateEntryId(GUID guidStore, unsigned int ulObjType, entryId** lppEntryId)
{
	ECRESULT	er = erSuccess;
	entryId*	lpEntryId = NULL;
	EID			eid;

	if(lppEntryId == NULL) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	if(CoCreateGuid(&eid.uniqueId) != hrSuccess) {
		er = ZARAFA_E_CALL_FAILED;
		goto exit;
	}

	eid.guid = guidStore;
	eid.usType = ulObjType;
	lpEntryId = new entryId;

	lpEntryId->__size = sizeof(EID);
	lpEntryId->__ptr = new unsigned char[lpEntryId->__size];

	memcpy(lpEntryId->__ptr, &eid, lpEntryId->__size);

	*lppEntryId = lpEntryId;

exit:

	return er;
}


/**
 * Get the local user id based on the entryid or the user id for old clients.
 *
 * When an entryid is provided, the extern id is extracted and the local user id
 * is resolved based on that. If no entryid is provided the provided legacy user id
 * is used as local user is and the extern id is resolved based on that. Old clients
 * that are not multi server aware provide the legacy user id in stead of the entryid.
 *
 * @param[in]	sUserId			The entryid of the user for which to obtain the local id
 * @param[in]	ulLegacyUserId	The legacy user id, which will be used as the entryid when.
 *								no entryid is provided (old clients).
 * @param[out]	lpulUserId		The local user id.
 * @param[out]	lpsExternId		The extern id of the user. This can be NULL if the extern id
 *								is not required by the caller.
 *
 * @retval	ZARAFA_E_INVALID_PARAMATER	One or more parameters are invalid.
 * @retval	ZARAFA_E_NOT_FOUND			The local is is not found.
 */
ECRESULT GetLocalId(entryId sUserId, unsigned int ulLegacyUserId, unsigned int *lpulUserId, objectid_t *lpsExternId)
{
	ECRESULT 		er = erSuccess;
	unsigned int	ulUserId = 0;
	objectid_t		sExternId;
	objectdetails_t	sDetails;

	if (lpulUserId == NULL)
	{
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	// If no entryid is present, use the 'current' user.
	if (ulLegacyUserId == 0 && sUserId.__size == 0) {
		// When lpsExternId is requested, the 'current' user will not be
		// requested in this way. However, to make sure a caller does expect a result in the future 
		// we'll return an error in that case.
		if (lpsExternId != NULL)
			er = ZARAFA_E_INVALID_PARAMETER;
		else
			*lpulUserId = 0;

		// TODO: return value in lpulUserId ?
		goto exit;
	}

	if (sUserId.__ptr) {
		// Extract the information from the entryid.
		er = ABEntryIDToID(&sUserId, &ulUserId, &sExternId, NULL);
		if (er != erSuccess)
			goto exit;

		// If an extern id is present, we should get an object based on that.
		if (!sExternId.id.empty())
			er = g_lpSessionManager->GetCacheManager()->GetUserObject(sExternId, &ulUserId, NULL, NULL);
	} else {
		// use user id from 6.20 and older clients
		ulUserId = ulLegacyUserId;
		if (lpsExternId)
			er = g_lpSessionManager->GetCacheManager()->GetUserObject(ulLegacyUserId, &sExternId, NULL, NULL);
	}
	if (er != erSuccess)
		goto exit;

	*lpulUserId = ulUserId;
	if (lpsExternId)
		*lpsExternId = sExternId;

exit:
	return er;
}

/**
 * Check if a user has a store of a particular type on the local server.
 *
 * On a single server configuration this function will return true for
 * all ECSTORE_TYPE_PRIVATE and ECSTORE_TYPE_PUBLIC requests and false otherwise.
 *
 * In single tennant mode, requests for ECSTORE_TYPE_PUBLIC will always return true,
 * regardless of the server on which the public should exist. This is actually wrong
 * but is the same behaviour as before.
 *
 * @param[in]	lpecSession			The ECSession object for the current session.
 * @param[in]	ulUserId			The user id of the user for which to check if a
 *									store is available.
 * @param[in]	ulStoreType			The store type to check for.
 * @param[out]	lpbHasLocalStore	The boolean that will contain the result on success.
 *
 * @retval	ZARAFA_E_INVALID_PARAMETER	One or more parameters are invalid.
 * @retval	ZARAFA_E_NOT_FOUND			The user specified by ulUserId was not found.
 */
ECRESULT CheckUserStore(ECSession *lpecSession, unsigned ulUserId, unsigned ulStoreType, bool *lpbHasLocalStore)
{
	ECRESULT er = erSuccess;
	objectdetails_t	sDetails;
	bool bPrivateOrPublic;

	if (lpecSession == NULL || lpbHasLocalStore == NULL || !ECSTORE_TYPE_ISVALID(ulStoreType)) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	bPrivateOrPublic = (ulStoreType == ECSTORE_TYPE_PRIVATE || ulStoreType == ECSTORE_TYPE_PUBLIC);

	if (g_lpSessionManager->IsDistributedSupported()) {
        er = lpecSession->GetUserManagement()->GetObjectDetails(ulUserId, &sDetails);
		if (er != erSuccess)
			goto exit;

		if (bPrivateOrPublic) {
			// @todo: Check if there's a define or constant for everyone.
			if (ulUserId == 2)	// Everyone, public in single tennant
				*lpbHasLocalStore = true;
			else
				*lpbHasLocalStore = (stricmp(sDetails.GetPropString(OB_PROP_S_SERVERNAME).c_str(), g_lpSessionManager->GetConfig()->GetSetting("server_name")) == 0);
		} else	// Archive store
			*lpbHasLocalStore = sDetails.PropListStringContains((property_key_t)PR_EC_ARCHIVE_SERVERS_A, g_lpSessionManager->GetConfig()->GetSetting("server_name"), true);
	} else	// Single tennant
		*lpbHasLocalStore = bPrivateOrPublic;

exit:
	return er;
}

ECRESULT GetABEntryID(unsigned int ulUserId, soap *lpSoap, entryId *lpUserId)
{
	ECRESULT			er = erSuccess;
	entryId				sUserId = {0};
	objectid_t			sExternId;

	if (lpSoap == NULL)
	{
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}	

	if (ulUserId == ZARAFA_UID_SYSTEM) {
		sExternId.objclass = ACTIVE_USER;
	} else if (ulUserId == ZARAFA_UID_EVERYONE) {
		sExternId.objclass = DISTLIST_SECURITY;
	} else {
		er = g_lpSessionManager->GetCacheManager()->GetUserObject(ulUserId, &sExternId, NULL, NULL);
		if (er != erSuccess)
			goto exit;
	}

	er = ABIDToEntryID(lpSoap, ulUserId, sExternId, &sUserId);
	if (er != erSuccess)
		goto exit;

	*lpUserId = sUserId;	// pointer (__ptr) is copied, not data

exit:
	return er;
}

ECRESULT PeerIsServer(struct soap *soap, const std::string &strServerName, const std::string &strHttpPath, const std::string &strSslPath, bool *lpbResult)
{
	ECRESULT		er = erSuccess;
	bool			bResult = false;

	if (soap == NULL || lpbResult == NULL)
	{
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	// First check if we're connecting through unix-socket/named-pipe and if the request url matches this server
	if (SOAP_CONNECTION_TYPE_NAMED_PIPE(soap) &&
			stricmp(strServerName.c_str(), g_lpSessionManager->GetConfig()->GetSetting("server_name")) == 0)
		bResult = true;

	else
	{
		const std::string *lpstrPath = &strHttpPath;
		if (lpstrPath->empty())
			lpstrPath = &strSslPath;
		if (!lpstrPath->empty())
		{
			std::string		strHost;
			std::string::size_type ulHostStart = 0;
			std::string::size_type ulHostEnd = 0;
			struct addrinfo	sHint = {0};
			struct addrinfo *lpsAddrInfo = NULL;
			struct addrinfo *lpsAddrIter = NULL;
			
			ulHostStart = lpstrPath->find("://");
			if (ulHostStart == std::string::npos) {
				er = ZARAFA_E_INVALID_PARAMETER;
				goto exit;
			}
			ulHostStart += 3;	// Skip the '://'

			ulHostEnd = lpstrPath->find(':', ulHostStart);
			if (ulHostEnd == std::string::npos) {
				er = ZARAFA_E_INVALID_PARAMETER;
				goto exit;
			}

			strHost = lpstrPath->substr(ulHostStart, ulHostEnd - ulHostStart);

			sHint.ai_family = AF_UNSPEC;
			sHint.ai_socktype = SOCK_STREAM;

			if (getaddrinfo(strHost.c_str(), NULL, &sHint, &lpsAddrInfo) != 0) {
				er = ZARAFA_E_NOT_FOUND;
				goto exit;
			}

			lpsAddrIter = lpsAddrInfo;
			while (lpsAddrIter && bResult == false) {
				if (soap->peerlen >= sizeof(sockaddr) && lpsAddrIter->ai_family == ((sockaddr*)&soap->peer)->sa_family) {
					switch (lpsAddrIter->ai_family) {
						case AF_INET:
							{
								sockaddr_in *lpsLeft = (sockaddr_in*)lpsAddrIter->ai_addr;
								sockaddr_in *lpsRight = (sockaddr_in*)&soap->peer;
								bResult = (memcmp(&lpsLeft->sin_addr, &lpsRight->sin_addr, sizeof(lpsLeft->sin_addr)) == 0);
							}
							break;

						case AF_INET6:
							{
								sockaddr_in6 *lpsLeft = (sockaddr_in6*)lpsAddrIter->ai_addr;
								sockaddr_in6 *lpsRight = (sockaddr_in6*)&soap->peer;
								bResult = (memcmp(&lpsLeft->sin6_addr, &lpsRight->sin6_addr, sizeof(lpsLeft->sin6_addr)) == 0);
							}
							break;

						default: break;
					}
				}

				lpsAddrIter = lpsAddrIter->ai_next;
			}
			freeaddrinfo(lpsAddrInfo);
		}
	}

	*lpbResult = bResult;

exit:
	return er;
}

/**
 * Get the best server path for a server
 *
 * This function will return the 'best' server path to redirect the client to. This is
 * done by examining the existing incoming connection, and choosing an appropriate access
 * method to the given server. Rules are as follows:
 *
 * - If bProxy is TRUE and the destination server has a proxy address, return the proxy address
 * - If bProxy is FALSE:
 *   - If existing connection is HTTP, return first available of: HTTP, HTTPS
 *   - If existing connection is HTTPS, return HTTPS
 *   - If existing connection is FILE, return first available of: HTTPS, HTTP
 *
 * @param[in] soap SOAP structure for incoming request
 * @param[in] lpecSession Session for the request
 * @param[in] strServerName Server name to get path for
 * @param[in] bProxy TRUE if we are requesting the proxy address for a server
 * @param[out] lpstrServerPath Output path of server (URL)
 * @return result
 */
ECRESULT GetBestServerPath(struct soap *soap, ECSession *lpecSession, const std::string &strServerName, std::string *lpstrServerPath)
{
	ECRESULT	er = erSuccess;
	std::string	strServerPath;
	bool		bConnectPipe = false;
	SOAPINFO	*lpInfo = NULL;

	serverdetails_t	sServerDetails;
	std::string		strFilePath;
	std::string		strHttpPath;
	std::string		strSslPath;
	std::string		strProxyPath;
	char *			szProxyHeader = lpecSession->GetSessionManager()->GetConfig()->GetSetting("proxy_header");

	if (soap == NULL || soap->user == NULL || lpstrServerPath == NULL)
	{
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}
	
	lpInfo = (SOAPINFO *)soap->user;

	er = lpecSession->GetUserManagement()->GetServerDetails(strServerName, &sServerDetails);
	if (er != erSuccess)
		goto exit;

    strProxyPath = sServerDetails.GetProxyPath();
	strFilePath = sServerDetails.GetFilePath();
	strHttpPath = sServerDetails.GetHttpPath();
	strSslPath = sServerDetails.GetSslPath();

	// Always redirect if proxy_header is "*"
    if (!strcmp(szProxyHeader, "*") || lpInfo->bProxy) {
        if(!strProxyPath.empty()) {
            *lpstrServerPath = strProxyPath;
            goto exit;
        } else {
            lpecSession->GetSessionManager()->GetLogger()->Log(EC_LOGLEVEL_FATAL, "Proxy path not set for server '%s'! falling back to direct address.", strServerName.c_str());
        }
    }    

	if (!strFilePath.empty())
	{
		er = PeerIsServer(soap, strServerName, strHttpPath, strSslPath, &bConnectPipe);
		if (er != erSuccess)
			goto exit;
	} else {
		// TODO: check if same server, and set strFilePath 'cause it's known

		bConnectPipe = false;
	}

	if (bConnectPipe)
		strServerPath = strFilePath;
	else
		switch (SOAP_CONNECTION_TYPE(soap))
		{
		case CONNECTION_TYPE_TCP:
			if (!strHttpPath.empty())
				strServerPath = strHttpPath;
			else if (!strSslPath.empty())
				strServerPath = strSslPath;
			break;

		case CONNECTION_TYPE_SSL:
			if (!strSslPath.empty())
				strServerPath = strSslPath;
			break;

		case CONNECTION_TYPE_NAMED_PIPE:
		case CONNECTION_TYPE_NAMED_PIPE_PRIORITY:
			if (!strSslPath.empty())
				strServerPath = strSslPath;
			else if (!strHttpPath.empty())
				strServerPath = strHttpPath;
			break;
		}

	if (strServerPath.empty())
	{
		er = ZARAFA_E_NOT_FOUND;
		goto exit;
	}

	*lpstrServerPath = strServerPath;

exit:
	return er;
}


// exception: This function does internal Begin + Commit/Rollback
ECRESULT MoveObjects(ECSession *lpSession, ECDatabase *lpDatabase, ECListInt* lplObjectIds, unsigned int ulDestFolderId, unsigned int ulSyncId);
// these functions don't do Begin + Commit/Rollback
ECRESULT WriteProps(struct soap *soap, ECSession *lpecSession, ECDatabase *lpDatabase, ECAttachmentStorage *lpAttachmentStorage, struct saveObject *lpsSaveObj, unsigned int ulObjId, bool fNewItem, unsigned int ulSyncId, struct saveObject *lpsReturnObj, bool *lpfHaveChangeKey, FILETIME *ftCreated, FILETIME *ftModified);

ECRESULT DoNotifySubscribe(ECSession *lpecSession, unsigned long long ulSessionId, struct notifySubscribe *notifySubscribe);
ECRESULT SaveLogonTime(ECSession *lpecSession, bool bLogon);


/**
 * logon: log on and create a session with provided credentials
 */
int ns__logon(struct soap *soap, char *user, char *pass, char *clientVersion, unsigned int clientCaps, unsigned int logonFlags, struct xsd__base64Binary sLicenseRequest, ULONG64 ullSessionGroup, char *szClientApp, struct logonResponse *lpsResponse)
{
	ECRESULT	er = erSuccess;
	ECSession	*lpecSession = NULL;
	ECSESSIONID	sessionID = 0;
	GUID		sServerGuid = {0};
    ECLicenseClient *lpLicenseClient = NULL;
    unsigned int ulLicenseResponse;
    unsigned char *lpLicenseResponse = NULL;
	struct timespec startTimes = {0}, endTimes = {0};
	double          dblStart = GetTimeOfDay();

	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &startTimes);

    LOG_SOAP_DEBUG(g_lpSessionManager->GetLogger(), "%020llu: S logon", sessionID);

    if ((clientCaps & ZARAFA_CAP_UNICODE) == 0) {
		user = ECStringCompat::WTF1252_to_UTF8(soap, user);
		pass = ECStringCompat::WTF1252_to_UTF8(soap, pass);
		clientVersion = ECStringCompat::WTF1252_to_UTF8(soap, clientVersion);
		szClientApp = ECStringCompat::WTF1252_to_UTF8(soap, szClientApp);
	}

	// check username and password
	er = g_lpSessionManager->CreateSession(soap, user, pass, clientVersion, szClientApp, clientCaps, ullSessionGroup, &sessionID, &lpecSession, true, (logonFlags & ZARAFA_LOGON_NO_UID_AUTH) == 0);
	if(er != erSuccess){
		er = ZARAFA_E_LOGON_FAILED;
		goto exit;
	}

	// We allow Zarafa 6 clients to connect to a Zarafa 7 server. However, anything below that will be
	// denied. We can't say what future clients may or may not be capable of. So we'll leave that to the
	// clients.
	if (ZARAFA_COMPARE_VERSION_TO_GENERAL(lpecSession->ClientVersion(), MAKE_ZARAFA_GENERAL(6)) < 0) {
		g_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_WARNING, "Rejected logon attempt from a %s version client.", clientVersion ? clientVersion : "<unknown>");
		er = ZARAFA_E_INVALID_VERSION;
		goto exit;
	}


	lpsResponse->ulSessionId = sessionID;
	lpsResponse->lpszVersion = "0,"PROJECT_VERSION_SERVER_STR;
	lpsResponse->ulCapabilities = ZARAFA_LATEST_CAPABILITIES;

	if (clientCaps & ZARAFA_CAP_COMPRESSION) {
		// client knows compression, then turn it on
		lpsResponse->ulCapabilities |= ZARAFA_CAP_COMPRESSION;
		// (ECSessionManager::ValidateSession() will do this for all other functions)
		soap_set_imode(soap, SOAP_ENC_ZLIB);	// also autodetected
		soap_set_omode(soap, SOAP_ENC_ZLIB | SOAP_IO_CHUNK);
	}

	if (clientCaps & ZARAFA_CAP_MULTI_SERVER)
		lpsResponse->ulCapabilities |= ZARAFA_CAP_MULTI_SERVER;
		
	if (clientCaps & ZARAFA_CAP_ENHANCED_ICS && parseBool(g_lpSessionManager->GetConfig()->GetSetting("enable_enhanced_ics"))) {
		lpsResponse->ulCapabilities |= ZARAFA_CAP_ENHANCED_ICS;
		soap_set_omode(soap, SOAP_ENC_MTOM | SOAP_IO_CHUNK);
		soap_set_imode(soap, SOAP_ENC_MTOM);
		soap_post_check_mime_attachments(soap);
	}

	if (clientCaps & ZARAFA_CAP_UNICODE)
		lpsResponse->ulCapabilities |= ZARAFA_CAP_UNICODE;

	if (clientCaps & ZARAFA_CAP_MSGLOCK)
		lpsResponse->ulCapabilities |= ZARAFA_CAP_MSGLOCK;

    if(sLicenseRequest.__size) {
        lpLicenseClient = new ECLicenseClient(g_lpSessionManager->GetConfig()->GetSetting("license_socket"), atoui(g_lpSessionManager->GetConfig()->GetSetting("license_timeout")));
        
        er = lpLicenseClient->Auth(sLicenseRequest.__ptr, sLicenseRequest.__size, &lpLicenseResponse, &ulLicenseResponse);

        // If the license server is not running, report this as no access.
        if(er == ZARAFA_E_NETWORK_ERROR)
            er = ZARAFA_E_NO_ACCESS;

		// kill license client, so we close the socket early
		delete lpLicenseClient;
        
        if(er != erSuccess) {
            g_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_FATAL, "Client requested license but zarafa-licensed could not be contacted");
            goto exit; // Note that Auth() succeeds even if the client request was denied. An error here is a real parsing error for example.
        }
        
        lpsResponse->sLicenseResponse.__size = ulLicenseResponse;
        lpsResponse->sLicenseResponse.__ptr = (unsigned char *)s_memcpy(soap, (const char *)lpLicenseResponse, ulLicenseResponse);
    }

	er = g_lpSessionManager->GetServerGUID(&sServerGuid);
	if (er != erSuccess)
		goto exit;

	lpsResponse->sServerGuid.__ptr = (unsigned char*)s_memcpy(soap, (char*)&sServerGuid, sizeof(sServerGuid));
	lpsResponse->sServerGuid.__size = sizeof(sServerGuid);

    // Only save logon if credentials were supplied by the user; otherwise the logon is probably automated
    if(lpecSession->GetAuthMethod() == ECSession::METHOD_USERPASSWORD || lpecSession->GetAuthMethod() == ECSession::METHOD_SSO)
        SaveLogonTime(lpecSession, true);
	
exit:
	if (lpecSession)
		lpecSession->Unlock();
        
    if(lpLicenseResponse)
        delete [] lpLicenseResponse;

	lpsResponse->er = er;

	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &endTimes);

	LOG_SOAP_DEBUG(g_lpSessionManager->GetLogger(), "%020llu: E logon 0x%08x %f %f", sessionID, er, timespec2dbl(endTimes) - timespec2dbl(startTimes), GetTimeOfDay() - dblStart);

	return SOAP_OK;
}

/**
 * logon: log on and create a session with provided credentials
 */
int ns__ssoLogon(struct soap *soap, ULONG64 ulSessionId, char *szUsername, struct xsd__base64Binary *lpInput, char *szClientVersion, unsigned int clientCaps, struct xsd__base64Binary sLicenseRequest, ULONG64 ullSessionGroup, char *szClientApp, struct ssoLogonResponse *lpsResponse)
{
	ECRESULT		er = ZARAFA_E_LOGON_FAILED;
	ECAuthSession	*lpecAuthSession = NULL;
	ECSession		*lpecSession = NULL;
	ECSESSIONID		newSessionID = 0;
	GUID			sServerGuid = {0};
	xsd__base64Binary *lpOutput = NULL;
	char*			lpszEnabled = NULL;
	ECLicenseClient*lpLicenseClient = NULL;
	struct timespec startTimes = {0}, endTimes = {0};
	double          dblStart = GetTimeOfDay();

	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &startTimes);

    LOG_SOAP_DEBUG(g_lpSessionManager->GetLogger(), "%020"PRIu64": S ssoLogon", ulSessionId);

	if (!lpInput || lpInput->__size == 0 || lpInput->__ptr == NULL || !szUsername || !szClientVersion)
		goto exit;

	lpszEnabled = g_lpSessionManager->GetConfig()->GetSetting("enable_sso");
	if (!(lpszEnabled && stricmp(lpszEnabled, "yes") == 0))
		goto nosso;

	if (ulSessionId == 0) {
		// new auth session
		er = g_lpSessionManager->CreateAuthSession(soap, clientCaps, &newSessionID, &lpecAuthSession, true, true);
		if (er != erSuccess) {
			er = ZARAFA_E_LOGON_FAILED;
			goto exit;
		}
		// when the first validate fails, remove the correct sessionid
		ulSessionId = newSessionID;
	} else {
		er = g_lpSessionManager->ValidateSession(soap, ulSessionId, &lpecAuthSession, true);
		if (er != erSuccess)
			goto exit;
	}

    if ((clientCaps & ZARAFA_CAP_UNICODE) == 0) {
		szUsername = ECStringCompat::WTF1252_to_UTF8(soap, szUsername);
		szClientVersion = ECStringCompat::WTF1252_to_UTF8(soap, szClientVersion);
		szClientApp = ECStringCompat::WTF1252_to_UTF8(soap, szClientApp);
	}

	er = lpecAuthSession->ValidateSSOData(soap, szUsername, szClientVersion, szClientApp, lpInput, &lpOutput);
	if (er == ZARAFA_E_SSO_CONTINUE) {
		// continue validation exchange
		lpsResponse->lpOutput = lpOutput;
	} else if (er == erSuccess) {
		// done and logged in
        if(sLicenseRequest.__size) {
            lpLicenseClient = new ECLicenseClient(g_lpSessionManager->GetConfig()->GetSetting("license_socket"), atoui(g_lpSessionManager->GetConfig()->GetSetting("license_timeout")));
            
            er = lpLicenseClient->Auth(sLicenseRequest.__ptr, sLicenseRequest.__size, &lpsResponse->sLicenseResponse.__ptr, (unsigned int *)&lpsResponse->sLicenseResponse.__size);
            // If the license server is not running, report this as no access.
            if(er == ZARAFA_E_NETWORK_ERROR)
                er = ZARAFA_E_NO_ACCESS;

			// kill license client, so we close the socket early
			delete lpLicenseClient;
            
            if(er != erSuccess) {
                g_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_FATAL, "Client requested license but zarafa-licensed could not be contacted");
                goto exit; // Note that Auth() succeeds even if the client request was denied. An error here is a real parsing error for example.
            }
        }

		// create ecsession from ecauthsession, and place in session map
		er = g_lpSessionManager->RegisterSession(lpecAuthSession, ullSessionGroup, szClientVersion, szClientApp, &newSessionID, &lpecSession, true);
		if (er != erSuccess) {
			g_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_FATAL, "User authenticated, but failed to create session. Error 0x%08X", er);
			goto exit;
		}

	// We allow Zarafa 6 clients to connect to a Zarafa 7 server. However, anything below that will be
	// denied. We can't say what future clients may or may not be capable of. So we'll leave that to the
	// clients.
	if (ZARAFA_COMPARE_VERSION_TO_GENERAL(lpecSession->ClientVersion(), MAKE_ZARAFA_GENERAL(6)) < 0) {
		g_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_WARNING, "Rejected logon attempt from a %s version client.", szClientVersion ? szClientVersion : "<unknown>");
		er = ZARAFA_E_INVALID_VERSION;
		goto exit;
	}


		// delete authsession
		lpecAuthSession->Unlock();
		
		g_lpSessionManager->RemoveSession(ulSessionId);
		lpecAuthSession = NULL;

		// return ecsession number
		lpsResponse->lpOutput = NULL;

	} else {
		// delete authsession
		lpecAuthSession->Unlock();

		g_lpSessionManager->RemoveSession(ulSessionId);
		lpecAuthSession = NULL;

		er = ZARAFA_E_LOGON_FAILED;
		goto exit;
	}

	lpsResponse->ulSessionId = newSessionID;
	lpsResponse->lpszVersion = "0,"PROJECT_VERSION_SERVER_STR;
	lpsResponse->ulCapabilities = ZARAFA_LATEST_CAPABILITIES;

	if (clientCaps & ZARAFA_CAP_COMPRESSION) {
		// client knows compression, then turn it on
		lpsResponse->ulCapabilities |= ZARAFA_CAP_COMPRESSION;
		// (ECSessionManager::ValidateSession() will do this for all other functions)
		soap_set_imode(soap, SOAP_ENC_ZLIB);	// also autodetected
		soap_set_omode(soap, SOAP_ENC_ZLIB | SOAP_IO_CHUNK);
	}

	if (clientCaps & ZARAFA_CAP_MULTI_SERVER)
		lpsResponse->ulCapabilities |= ZARAFA_CAP_MULTI_SERVER;

	if (clientCaps & ZARAFA_CAP_ENHANCED_ICS && parseBool(g_lpSessionManager->GetConfig()->GetSetting("enable_enhanced_ics"))) {
		lpsResponse->ulCapabilities |= ZARAFA_CAP_ENHANCED_ICS;
		soap_set_omode(soap, SOAP_ENC_MTOM | SOAP_IO_CHUNK);
		soap_set_imode(soap, SOAP_ENC_MTOM);
		soap_post_check_mime_attachments(soap);
	}

	if (clientCaps & ZARAFA_CAP_UNICODE)
		lpsResponse->ulCapabilities |= ZARAFA_CAP_UNICODE;

	if (clientCaps & ZARAFA_CAP_MSGLOCK)
		lpsResponse->ulCapabilities |= ZARAFA_CAP_MSGLOCK;

    if(er != ZARAFA_E_SSO_CONTINUE) {
        // Don't reset er to erSuccess on SSO_CONTINUE, we don't need the server guid yet
    	er = g_lpSessionManager->GetServerGUID(&sServerGuid);
    	if (er != erSuccess)
    		goto exit;
    
    	lpsResponse->sServerGuid.__ptr = (unsigned char*)s_memcpy(soap, (char*)&sServerGuid, sizeof(sServerGuid));
    	lpsResponse->sServerGuid.__size = sizeof(sServerGuid);
    }
    
    if(lpecSession && (lpecSession->GetAuthMethod() == ECSession::METHOD_USERPASSWORD || lpecSession->GetAuthMethod() == ECSession::METHOD_SSO))
        SaveLogonTime(lpecSession, true);

exit:
    if (lpecAuthSession)
        lpecAuthSession->Unlock();

	if (lpecSession)
		lpecSession->Unlock();
        
	if (er == erSuccess)
		g_lpStatsCollector->Increment(SCN_LOGIN_SSO);
	else if (er != ZARAFA_E_SSO_CONTINUE)
		g_lpStatsCollector->Increment(SCN_LOGIN_DENIED);

nosso:
	lpsResponse->er = er;

	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &endTimes);

	LOG_SOAP_DEBUG(g_lpSessionManager->GetLogger(), "%020"PRIu64": E ssoLogon 0x%08x %f %f", ulSessionId, er, timespec2dbl(endTimes) - timespec2dbl(startTimes), GetTimeOfDay() - dblStart);

	return SOAP_OK;
}

/**
 * logoff: invalidate the session and close all notifications and memory held by the session
 */
int ns__logoff(struct soap *soap, ULONG64 ulSessionId, unsigned int *result)
{
	ECRESULT	er = erSuccess;
	ECSession 	*lpecSession = NULL;
	struct timespec startTimes = {0}, endTimes = {0};
	double          dblStart = GetTimeOfDay();

	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &startTimes);

    LOG_SOAP_DEBUG(g_lpSessionManager->GetLogger(), "%020"PRIu64": S logoff", ulSessionId);

	er = g_lpSessionManager->ValidateSession(soap, ulSessionId, &lpecSession, true);
	if(er != erSuccess)
		goto exit;

    if(lpecSession->GetAuthMethod() == ECSession::METHOD_USERPASSWORD || lpecSession->GetAuthMethod() == ECSession::METHOD_SSO)
        SaveLogonTime(lpecSession, false);

	lpecSession->Unlock();

    // lpecSession is discarded. It is not locked, so we can do that. We only did the 'validatesession'
    // call to see if the session id existed in the first place, and the request is coming from the correct
    // IP address. Another logoff() call called at the same time may remove the session *here*, in which case the following call
    // will fail. This makes sure people can't terminate each others sessions unless they have the same source IP.

	er = g_lpSessionManager->RemoveSession(ulSessionId);
exit:
    *result = er;

	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &endTimes);
	LOG_SOAP_DEBUG(g_lpSessionManager->GetLogger(), "%020"PRIu64": E logoff 0x%08x %f %f", ulSessionId, 0, timespec2dbl(endTimes) - timespec2dbl(startTimes), GetTimeOfDay() - dblStart);

    return SOAP_OK;
}

#define SOAP_ENTRY_FUNCTION_HEADER(resultvar, fname) \
    ECRESULT		er = erSuccess; \
    struct timespec	startTimes = {0}, endTimes = {0};	\
    double			dblStart = GetTimeOfDay(); \
    ECSession		*lpecSession = NULL; \
    unsigned int 	*lpResultVar = &resultvar; \
	char            *szFname = #fname; \
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &startTimes); \
	LOG_SOAP_DEBUG(g_lpSessionManager->GetLogger(), "%020"PRIu64": S %s", ulSessionId, szFname); \
	er = g_lpSessionManager->ValidateSession(soap, ulSessionId, &lpecSession, true);\
	const bool bSupportUnicode = (er == erSuccess ? (lpecSession->GetCapabilities() & ZARAFA_CAP_UNICODE) != 0 : false); \
	const ECStringCompat stringCompat(bSupportUnicode); \
	if(er != erSuccess) \
		goto __soapentry_exit; \
	lpecSession->AddBusyState(pthread_self(), #fname);

#define SOAP_ENTRY_FUNCTION_FOOTER \
__soapentry_exit: \
    *lpResultVar = er; \
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &endTimes); \
    if(lpecSession) { \
    	lpecSession->AddClocks( timespec2dbl(endTimes) - timespec2dbl(startTimes), \
    	                        0, \
							    GetTimeOfDay() - dblStart); \
		LOG_SOAP_DEBUG(g_lpSessionManager->GetLogger(), "%020"PRIu64": E %s 0x%08x %f %f", ulSessionId, szFname, er, timespec2dbl(endTimes) - timespec2dbl(startTimes), GetTimeOfDay() - dblStart); \
		lpecSession->RemoveBusyState(pthread_self()); \
        lpecSession->Unlock(); \
    } \
    return SOAP_OK;


// This is the variadic macro we would like to use. Unfortunately, the win32 vc++ compiler
// doesn't understand variadic macro's, so we have to split them into the cases below...
#define SOAP_ENTRY_START(fname,resultvar,...) \
int ns__##fname(struct soap *soap, ULONG64 ulSessionId, ##__VA_ARGS__) \
{ \
    SOAP_ENTRY_FUNCTION_HEADER(resultvar, fname)

#define SOAP_ENTRY_END() \
    SOAP_ENTRY_FUNCTION_FOOTER \
}


#define ALLOC_DBRESULT() \
	DB_ROW 			UNUSED_VAR		lpDBRow = NULL; \
	DB_LENGTHS		UNUSED_VAR		lpDBLen = NULL; \
	DB_RESULT		UNUSED_VAR		lpDBResult = NULL; \
	std::string		UNUSED_VAR		strQuery;

#define USE_DATABASE() \
       ECDatabase*             lpDatabase = NULL; \
       ALLOC_DBRESULT(); \
    \
       er = lpecSession->GetDatabase(&lpDatabase); \
       if (er != erSuccess) { \
               er = ZARAFA_E_DATABASE_ERROR; \
               goto __soapentry_exit; \
       }

#define FREE_DBRESULT() \
    if(lpDBResult) { \
        lpDatabase->FreeResult(lpDBResult); \
        lpDBResult = NULL; \
    }

#define ROLLBACK_ON_ERROR() \
	if (lpDatabase && FAILED(er)) \
		lpDatabase->Rollback(); \


// Save the current time as the last logon time for the logged-on user of lpecSession
ECRESULT SaveLogonTime(ECSession *lpecSession, bool bLogon) 
{
    ECRESULT er = erSuccess;
    unsigned int ulUserId = 0;
    unsigned int ulStoreId = 0;
    time_t now = time(NULL);
    FILETIME ft;
    unsigned int ulProperty = bLogon ? PR_LAST_LOGON_TIME : PR_LAST_LOGOFF_TIME;
	ECDatabase *lpDatabase = NULL; 
	ALLOC_DBRESULT(); 
    
	er = lpecSession->GetDatabase(&lpDatabase); 
	if (er != erSuccess)
		goto exit; 
    
    UnixTimeToFileTime(now, &ft);
    
        
    ulUserId = lpecSession->GetSecurity()->GetUserId();
    
    strQuery = "SELECT hierarchy_id FROM stores WHERE stores.user_id=" + stringify(ulUserId);
    
    er = lpDatabase->DoSelect(strQuery, &lpDBResult);
    if(er != erSuccess)
        goto exit;
        
    if(lpDatabase->GetNumRows(lpDBResult) == 0)
        goto exit; // User has no store on this server
        
    lpDBRow = lpDatabase->FetchRow(lpDBResult);
    ulStoreId = atoi(lpDBRow[0]);
    
    strQuery = "REPLACE INTO properties (tag, type, hierarchyid, val_hi, val_lo) VALUES(" 
                + stringify(PROP_ID(ulProperty)) + ","  + stringify(PROP_TYPE(ulProperty)) + ","
                + stringify(ulStoreId) + "," 
                + stringify(ft.dwHighDateTime) + "," + stringify(ft.dwLowDateTime) + ")";
                
    er = lpDatabase->DoInsert(strQuery);
    if(er != erSuccess)
        goto exit;
    
exit:
    ROLLBACK_ON_ERROR();
    FREE_DBRESULT();

    return er;
}

ECRESULT PurgeSoftDelete(ECSession *lpecSession, unsigned int ulLifetime, unsigned int *lpulMessages, unsigned int *lpulFolders, unsigned int *lpulStores, bool *lpbExit)
{
	ECRESULT 		er = erSuccess;
	ECDatabase*		lpDatabase = NULL;
	DB_RESULT		lpDBResult = NULL;
	DB_ROW			lpDBRow = NULL;
	std::string		strQuery;
	FILETIME		ft;
	unsigned int	ulDeleteFlags = 0;
	time_t			ulTime = 0;
	ECListInt		lObjectIds;
	ECListIntIterator	iterObjectId;
	unsigned int	ulFolders = 0, ulMessages = 0;
	unsigned int	ulStores = 0;
	bool 			bExitDummy = false;

	if (g_bPurgeSoftDeleteStatus) {
		er = ZARAFA_E_BUSY;
		goto exit;
	}
	
	g_bPurgeSoftDeleteStatus = TRUE;

	if (!lpbExit)
		lpbExit = &bExitDummy;

	er = lpecSession->GetDatabase(&lpDatabase);
	if (er != erSuccess)
		goto exit;

	// Although it doesn't make sence for the message deleter to include EC_DELETE_FOLDERS, it doesn't hurt either, since they shouldn't be there
	// and we really want to delete all the softdeleted items anyway.
	ulDeleteFlags = EC_DELETE_CONTAINER | EC_DELETE_FOLDERS | EC_DELETE_MESSAGES | EC_DELETE_RECIPIENTS | EC_DELETE_ATTACHMENTS | EC_DELETE_HARD_DELETE;

	GetSystemTimeAsFileTime(&ft);
	FileTimeToUnixTime(ft, &ulTime);

	ulTime -= ulLifetime;

	UnixTimeToFileTime(ulTime, &ft);

	// Select softdeleted stores (ignore softdelete_lifetime setting because a store can't be restored anyway)
	strQuery = "SELECT id FROM hierarchy WHERE parent IS NULL AND (flags&"+stringify(MSGFLAG_DELETED)+")="+stringify(MSGFLAG_DELETED)+" AND type="+stringify(MAPI_STORE);
	er = lpDatabase->DoSelect(strQuery, &lpDBResult);
	if(er != erSuccess)
		goto exit;

	ulStores = lpDatabase->GetNumRows(lpDBResult);
	if(ulStores > 0)
	{
		while( (lpDBRow = lpDatabase->FetchRow(lpDBResult)) )
		{
			if(lpDBRow == NULL || lpDBRow[0] == NULL)
				continue;

			lObjectIds.push_back(atoui(lpDBRow[0]));
		}
		// free before we call DeleteObjects()
		if(lpDBResult){	lpDatabase->FreeResult(lpDBResult); lpDBResult = NULL;}

		if (*lpbExit) {
			er = ZARAFA_E_USER_CANCEL;
			goto exit;
		}

		g_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_INFO, "Start to purge %d stores", (int)lObjectIds.size());

		for(iterObjectId = lObjectIds.begin(); iterObjectId != lObjectIds.end() && !(*lpbExit); iterObjectId++)
		{
			g_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_INFO, " purge store (%d)", *iterObjectId);

			er = DeleteObjects(lpecSession, lpDatabase, *iterObjectId, ulDeleteFlags|EC_DELETE_STORE, 0, false, false);
			if(er != erSuccess) {
				g_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_ERROR, "Error while removing softdelete store objects, error code: 0x%x.", er);
				goto exit;
			}
		}

		g_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_INFO, "Store purge done");

	}
	if(lpDBResult){	lpDatabase->FreeResult(lpDBResult); lpDBResult = NULL;}

	if (*lpbExit) {
		er = ZARAFA_E_USER_CANCEL;
		goto exit;
	}

	// Select softdeleted folders
	strQuery = "SELECT h.id FROM hierarchy AS h JOIN properties AS p ON p.hierarchyid=h.id AND p.tag="+stringify(PROP_ID(PR_DELETED_ON))+" AND p.type="+stringify(PROP_TYPE(PR_DELETED_ON))+" WHERE (h.flags&"+stringify(MSGFLAG_DELETED)+")="+stringify(MSGFLAG_DELETED)+" AND p.val_hi<="+stringify(ft.dwHighDateTime)+" AND h.type="+stringify(MAPI_FOLDER);
	er = lpDatabase->DoSelect(strQuery, &lpDBResult);
	if(er != erSuccess)
		goto exit;

	ulFolders = lpDatabase->GetNumRows(lpDBResult);
	if(ulFolders > 0)
	{
		// Remove all items
		lObjectIds.clear();

		while( (lpDBRow = lpDatabase->FetchRow(lpDBResult)) )
		{
			if(lpDBRow == NULL || lpDBRow[0] == NULL)
				continue;

			lObjectIds.push_back(atoui(lpDBRow[0]));
		}
		// free before we call DeleteObjects()
		if(lpDBResult){	lpDatabase->FreeResult(lpDBResult); lpDBResult = NULL;}

		if (*lpbExit) {
			er = ZARAFA_E_USER_CANCEL;
			goto exit;
		}

		g_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_INFO, "Start to purge %d folders", (int)lObjectIds.size());

		er = DeleteObjects(lpecSession, lpDatabase, &lObjectIds, ulDeleteFlags, 0, false, false);
		if(er != erSuccess) {
			g_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_ERROR, "Error while removing softdelete folder objects, error code: 0x%x.", er);
			goto exit;
		}

		g_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_INFO, "Folder purge done");

	}
	if(lpDBResult){	lpDatabase->FreeResult(lpDBResult); lpDBResult = NULL;}

	if (*lpbExit) {
		er = ZARAFA_E_USER_CANCEL;
		goto exit;
	}

	// Select softdeleted messages
	strQuery = "SELECT h.id FROM hierarchy AS h JOIN properties AS p ON p.hierarchyid=h.id AND p.tag="+stringify(PROP_ID(PR_DELETED_ON))+" AND p.type="+stringify(PROP_TYPE(PR_DELETED_ON))+" WHERE (h.flags&"+stringify(MSGFLAG_DELETED)+")="+stringify(MSGFLAG_DELETED)+" AND h.type="+stringify(MAPI_MESSAGE)+" AND p.val_hi<="+stringify(ft.dwHighDateTime);
	er = lpDatabase->DoSelect(strQuery, &lpDBResult);
	if(er != erSuccess)
		goto exit;

	ulMessages = lpDatabase->GetNumRows(lpDBResult);
	if(ulMessages > 0)
	{
		// Remove all items
		lObjectIds.clear();

		while( (lpDBRow = lpDatabase->FetchRow(lpDBResult)) )
		{
			if(lpDBRow == NULL || lpDBRow[0] == NULL)
				continue;

			lObjectIds.push_back(atoui(lpDBRow[0]));
		}
		// free before we call DeleteObjects()
		if(lpDBResult){	lpDatabase->FreeResult(lpDBResult); lpDBResult = NULL;}

		if (*lpbExit) {
			er = ZARAFA_E_USER_CANCEL;
			goto exit;
		}

		g_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_INFO, "Start to purge %d messages", (int)lObjectIds.size());

		er = DeleteObjects(lpecSession, lpDatabase, &lObjectIds, ulDeleteFlags, 0, false, false);
		if(er != erSuccess) {
			g_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_ERROR, "Error while removing softdelete message objects, error code: 0x%x.", er);
			goto exit;
		}

		g_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_INFO, "Message purge done");
	}

	// these stats are only from toplevel objects
	if(lpulFolders)
		*lpulFolders = ulFolders;

	if(lpulMessages)
		*lpulMessages = ulMessages;

	if (lpulStores)
		*lpulStores = ulStores;

exit:
	if (er != ZARAFA_E_BUSY)
		g_bPurgeSoftDeleteStatus = FALSE;

	if(lpDBResult)
		lpDatabase->FreeResult(lpDBResult);

	return er;
}

/**
 * getPublicStore: get the root entryid, the store entryid and the store GUID for the public store.
 * FIXME, GUID is duplicate
 */
SOAP_ENTRY_START(getPublicStore, lpsResponse->er, unsigned int ulFlags, struct getStoreResponse *lpsResponse)
{
    unsigned int		ulCompanyId = 0;
	std::string			strStoreName;
	std::string			strStoreServer;
	std::string			strServerPath;
	std::string			strCompanyName;
	const std::string	strThisServer = g_lpSessionManager->GetConfig()->GetSetting("server_name", "", "Unknown");
	objectdetails_t		details;
    USE_DATABASE();

	if (lpecSession->GetSessionManager()->IsHostedSupported()) {
		/* Hosted support, Public store owner is company */
		er = lpecSession->GetSecurity()->GetUserCompany(&ulCompanyId);
		if (er != erSuccess)
			goto exit;

        er = lpecSession->GetUserManagement()->GetObjectDetails(ulCompanyId, &details);
        if(er != erSuccess)
            goto exit;
        
        strStoreServer = details.GetPropString(OB_PROP_S_SERVERNAME);
		strCompanyName = details.GetPropString(OB_PROP_S_FULLNAME);
	} else {
		ulCompanyId = ZARAFA_UID_EVERYONE; /* No hosted support, Public store owner is Everyone */

		if (lpecSession->GetSessionManager()->IsDistributedSupported()) {
			/*
			* GetObjectDetailsAndSync will return the group details for EVERYONE when called
			* with ZARAFA_UID_EVERYONE. But we want the pseudo company for that contains the
			* public store.
			*/
			er = lpecSession->GetUserManagement()->GetPublicStoreDetails(&details);
			if (er == ZARAFA_E_NO_SUPPORT) {
				/* Not supported: No MultiServer with this plugin, so we're good */
				strStoreServer = strThisServer;
				er = erSuccess;
			} else if (er == erSuccess)
				strStoreServer = details.GetPropString(OB_PROP_S_SERVERNAME);
			else
				goto exit;
		} else
			strStoreServer = strThisServer;
	}

	if (lpecSession->GetSessionManager()->IsDistributedSupported())
	{
		if (strStoreServer.empty()) {
			if (!strCompanyName.empty())
				g_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_ERROR, "Company %s has no home server for its public store.", strCompanyName.c_str());
			else
				g_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_ERROR, "Public store has no home server.");
				
			er = ZARAFA_E_NOT_FOUND;
			goto exit;
		}
	
		/* Do we own the store? */
		if (stricmp(strThisServer.c_str(), strStoreServer.c_str()) != 0)
		{
			if ((ulFlags & EC_OVERRIDE_HOMESERVER) == 0) {
				er = GetBestServerPath(soap, lpecSession, strStoreServer, &strServerPath);
				if (er != erSuccess)
					goto exit;
	            
				lpsResponse->lpszServerPath = STROUT_FIX_CPY(strServerPath.c_str());
				g_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_INFO, "Redirecting request to '%s'", lpsResponse->lpszServerPath);
				g_lpStatsCollector->Increment(SCN_REDIRECT_COUNT, 1);
				er = ZARAFA_E_UNABLE_TO_COMPLETE;
				goto exit;
            }
		}
	}

	/*
	 * The public store is stored in the database with the companyid as owner.
	 */
	strQuery =
		"SELECT hierarchy.id, stores.guid, stores.hierarchy_id "
		"FROM stores "
		"JOIN hierarchy on stores.hierarchy_id=hierarchy.parent "
		"WHERE stores.user_id = " + stringify(ulCompanyId);

	er = lpDatabase->DoSelect(strQuery, &lpDBResult);
	if(er != erSuccess)
		goto exit;

	if(lpDatabase->GetNumRows(lpDBResult) == 0) {
		er = ZARAFA_E_NOT_FOUND;
		goto exit;
	}

	lpDBRow = lpDatabase->FetchRow(lpDBResult);
	lpDBLen = lpDatabase->FetchRowLengths(lpDBResult);

	if( lpDBRow == NULL || lpDBRow[0] == NULL || lpDBRow[1] == NULL || lpDBRow[2] == NULL ||
		lpDBLen == NULL || lpDBLen[1] == 0)
	{
		er = ZARAFA_E_DATABASE_ERROR; // this should never happen
		goto exit;
	}

	er = g_lpSessionManager->GetCacheManager()->GetEntryIdFromObject(atoui(lpDBRow[0]), soap, 0, &lpsResponse->sRootId);
	if(er != erSuccess)
	    goto exit;

    if((ulFlags & EC_OVERRIDE_HOMESERVER) == 0)
        lpsResponse->lpszServerPath = STROUT_FIX_CPY(string("pseudo://" + strStoreServer).c_str());
   	er = g_lpSessionManager->GetCacheManager()->GetEntryIdFromObject(atoui(lpDBRow[2]), soap, ulFlags, &lpsResponse->sStoreId);

	if(er != erSuccess)
	    goto exit;

	lpsResponse->guid.__size= lpDBLen[1];
	lpsResponse->guid.__ptr = s_alloc<unsigned char>(soap, lpDBLen[1]);

	memcpy(lpsResponse->guid.__ptr, lpDBRow[1], lpDBLen[1]);

exit:
    FREE_DBRESULT();
}
SOAP_ENTRY_END()

/**
 * getStore: get the root entryid, the store entryid and the GUID of a store specified with lpsEntryId
 * FIXME: output store entryid equals input store entryid ?
 * FIXME: output GUID is also in entryid
 */
SOAP_ENTRY_START(getStore, lpsResponse->er, entryId* lpsEntryId, struct getStoreResponse *lpsResponse)
{
	unsigned int	ulStoreId = 0;
	unsigned int	ulUserId = 0;
	objectdetails_t	sUserDetails;
	string			strServerName;
	string			strServerPath;
    USE_DATABASE();

	if (!lpsEntryId) {
		ulUserId = lpecSession->GetSecurity()->GetUserId();

        // Check if the store should be available on this server
		if (lpecSession->GetSessionManager()->IsDistributedSupported() && 
            !lpecSession->GetUserManagement()->IsInternalObject(ulUserId)) {
			er = lpecSession->GetUserManagement()->GetObjectDetails(ulUserId, &sUserDetails);
			if (er != erSuccess)
				goto exit;
            strServerName = sUserDetails.GetPropString(OB_PROP_S_SERVERNAME);
            if (strServerName.empty()) {
                er = ZARAFA_E_NOT_FOUND;
                goto exit;
            }
            if (stricmp(strServerName.c_str(), g_lpSessionManager->GetConfig()->GetSetting("server_name")) != 0)  {
                er = GetBestServerPath(soap, lpecSession, strServerName, &strServerPath);
                if (er != erSuccess)
                    goto exit;
                
                lpsResponse->lpszServerPath = STROUT_FIX_CPY(strServerPath.c_str());
                g_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_INFO, "Redirecting request to '%s'", lpsResponse->lpszServerPath);
                g_lpStatsCollector->Increment(SCN_REDIRECT_COUNT, 1);
                er = ZARAFA_E_UNABLE_TO_COMPLETE;
                goto exit;
            }
		}
	}
    
    // If strServerName is empty, we're not running in distributed mode or we're dealing
    // with a local account. Just use the name from the configuration.
    if (strServerName.empty())
        strServerName = g_lpSessionManager->GetConfig()->GetSetting("server_name" ,"", "Unknown");
    
    // Always return a pseudo URL
    lpsResponse->lpszServerPath = STROUT_FIX_CPY(string("pseudo://" + strServerName).c_str());

	strQuery = "SELECT hierarchy.id, stores.guid, stores.hierarchy_id "
	           "FROM stores join hierarchy on stores.hierarchy_id=hierarchy.parent ";

	if(lpsEntryId) {
		er = lpecSession->GetObjectFromEntryId(lpsEntryId, &ulStoreId);
		if(er != erSuccess)
			goto exit;

		strQuery += "WHERE stores.hierarchy_id=" + stringify(ulStoreId) + " LIMIT 1";// FIXME: mysql query
	}else {
		strQuery += "WHERE stores.user_id=" + stringify(ulUserId) 
				 + " AND stores.type=" + stringify(ECSTORE_TYPE_PRIVATE);
	}

	er = lpDatabase->DoSelect(strQuery, &lpDBResult);
	if(er != erSuccess)
		goto exit;

	if(lpDatabase->GetNumRows(lpDBResult) == 0) {
		er = ZARAFA_E_NOT_FOUND;
		goto exit;
	}

	lpDBRow = lpDatabase->FetchRow(lpDBResult);
	lpDBLen = lpDatabase->FetchRowLengths(lpDBResult);

	if( lpDBRow == NULL || lpDBRow[0] == NULL || lpDBRow[1] == NULL || lpDBRow[2] == NULL ||
		lpDBLen == NULL || lpDBLen[1] == 0 )
	{
		er = ZARAFA_E_DATABASE_ERROR; // this should never happen
		goto exit;
	}

	er = g_lpSessionManager->GetCacheManager()->GetEntryIdFromObject(atoui(lpDBRow[0]), soap, 0, &lpsResponse->sRootId);
	if(er != erSuccess)
	    goto exit;

	er = g_lpSessionManager->GetCacheManager()->GetEntryIdFromObject(atoui(lpDBRow[2]), soap, 0, &lpsResponse->sStoreId);
	if(er != erSuccess)
	    goto exit;

	lpsResponse->guid.__size= lpDBLen[1];
	lpsResponse->guid.__ptr = s_alloc<unsigned char>(soap, lpDBLen[1]);

	memcpy(lpsResponse->guid.__ptr, lpDBRow[1], lpDBLen[1]);

exit:
    FREE_DBRESULT();
}
SOAP_ENTRY_END()

/**
 * getStoreName: get the PR_DISPLAY_NAME of the store specified in sEntryId
 */
SOAP_ENTRY_START(getStoreName, lpsResponse->er, entryId sEntryId, struct getStoreNameResponse *lpsResponse)
{
	unsigned int	ulObjId = 0;
	unsigned int	ulStoreType = 0;

	er = lpecSession->GetObjectFromEntryId(&sEntryId, &ulObjId);
	if(er != erSuccess)
	    goto exit;

	er = lpecSession->GetSessionManager()->GetCacheManager()->GetStoreAndType(ulObjId, NULL, NULL, &ulStoreType);
	if (er != erSuccess)
		goto exit;

	er = ECGenProps::GetStoreName(soap, lpecSession, ulObjId, ulStoreType, &lpsResponse->lpszStoreName);
	if(er != erSuccess)
		goto exit;

	if (!bSupportUnicode)
		lpsResponse->lpszStoreName = STROUT_FIX(lpsResponse->lpszStoreName);

exit:
	;
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(getStoreType, lpsResponse->er, entryId sEntryId, struct getStoreTypeResponse *lpsResponse)
{
	unsigned int	ulObjId = 0;

	er = lpecSession->GetObjectFromEntryId(&sEntryId, &ulObjId);
	if (er != erSuccess)
	    goto exit;

	er = lpecSession->GetSessionManager()->GetCacheManager()->GetStoreAndType(ulObjId, NULL, NULL, &lpsResponse->ulStoreType);

exit:
	;
}
SOAP_ENTRY_END()

ECRESULT ReadProps(struct soap *soap, ECSession *lpecSession, unsigned int ulObjId, unsigned ulObjType, unsigned int ulObjTypeParent, CHILDPROPS sChildProps, struct propTagArray *lpsPropTag, struct propValArray *lpsPropVal)
{
	ECRESULT er = erSuccess;

	quotadetails_t	sDetails;
	unsigned int	ulCompanyId = 0;
	unsigned int	ulStoreOwner = 0;

	ECAttachmentStorage *lpAttachmentStorage = NULL;

	struct propVal sPropVal;
	ECStringCompat stringCompat(lpecSession->GetCapabilities() & ZARAFA_CAP_UNICODE);

	DB_RESULT		lpDBResultMV = NULL;
	USE_DATABASE();
        
	if(ulObjType == MAPI_STORE) //fimxe: except public stores
	{
		if (ECGenProps::GetPropComputedUncached(soap, NULL, lpecSession, PR_USER_NAME, ulObjId, 0, ulObjId, 0, ulObjType, &sPropVal) == erSuccess) {
			er = FixPropEncoding(soap, stringCompat, Out, &sPropVal);
			if (er != erSuccess)
				goto exit;
				
		    sChildProps.lpPropTags->AddPropTag(sPropVal.ulPropTag);
		    sChildProps.lpPropVals->AddPropVal(sPropVal);
        }

		if (ECGenProps::GetPropComputedUncached(soap, NULL, lpecSession, PR_USER_ENTRYID, ulObjId, 0, ulObjId, 0, ulObjType, &sPropVal) == erSuccess) {
		    sChildProps.lpPropTags->AddPropTag(PR_USER_ENTRYID);
		    sChildProps.lpPropVals->AddPropVal(sPropVal);
        }

        er = lpecSession->GetSecurity()->GetStoreOwner(ulObjId, &ulStoreOwner);
        if (er != erSuccess)
            goto exit;

		// Quota information
		if (lpecSession->GetSecurity()->GetUserQuota(ulStoreOwner, false, &sDetails) == erSuccess)
		{
			// PR_QUOTA_WARNING_THRESHOLD
			sPropVal.ulPropTag = PR_QUOTA_WARNING_THRESHOLD;
			sPropVal.__union = SOAP_UNION_propValData_ul;
			sPropVal.Value.ul = (unsigned long)(sDetails.llWarnSize / 1024);

			sChildProps.lpPropTags->AddPropTag(PR_QUOTA_WARNING_THRESHOLD);
			sChildProps.lpPropVals->AddPropVal(sPropVal);
			    
			// PR_QUOTA_SEND_THRESHOLD
			sPropVal.ulPropTag = PR_QUOTA_SEND_THRESHOLD;
			sPropVal.__union = SOAP_UNION_propValData_ul;
			sPropVal.Value.ul = (unsigned long)(sDetails.llSoftSize / 1024);

			sChildProps.lpPropTags->AddPropTag(PR_QUOTA_SEND_THRESHOLD);
            sChildProps.lpPropVals->AddPropVal(sPropVal);

			// PR_QUOTA_RECEIVE_THRESHOLD
			sPropVal.ulPropTag = PR_QUOTA_RECEIVE_THRESHOLD;
			sPropVal.__union = SOAP_UNION_propValData_ul;
			sPropVal.Value.ul = (unsigned long)(sDetails.llHardSize  / 1024);

			sChildProps.lpPropTags->AddPropTag(PR_QUOTA_RECEIVE_THRESHOLD);
            sChildProps.lpPropVals->AddPropVal(sPropVal);
		}

		if (lpecSession->GetCapabilities() & ZARAFA_CAP_MAILBOX_OWNER) {
			// get the companyid to which the logged in user belongs to.
			er = lpecSession->GetSecurity()->GetUserCompany(&ulCompanyId);
			if (er != erSuccess)
				goto exit;

			// 5.0 client knows how to handle the PR_MAILBOX_OWNER_* properties
			if(ulStoreOwner != ZARAFA_UID_EVERYONE && ulStoreOwner != ulCompanyId )	{

				if (ECGenProps::GetPropComputedUncached(soap, NULL, lpecSession, PR_MAILBOX_OWNER_NAME, ulObjId, 0, ulObjId, 0, ulObjType, &sPropVal) == erSuccess) {
					er = FixPropEncoding(soap, stringCompat, Out, &sPropVal);
					if (er != erSuccess)
						goto exit;
						
					sChildProps.lpPropTags->AddPropTag(sPropVal.ulPropTag);
					sChildProps.lpPropVals->AddPropVal(sPropVal);
				}

				// Add PR_MAILBOX_OWNER_ENTRYID
				if (ECGenProps::GetPropComputedUncached(soap, NULL, lpecSession, PR_MAILBOX_OWNER_ENTRYID, ulObjId, 0, ulObjId, 0, ulObjType, &sPropVal) == erSuccess) {
					sChildProps.lpPropTags->AddPropTag(PR_MAILBOX_OWNER_ENTRYID);
                    sChildProps.lpPropVals->AddPropVal(sPropVal);
				}
			}
		}

		// Add PR_DISPLAY_NAME
		if (ECGenProps::GetPropComputedUncached(soap, NULL, lpecSession, PR_DISPLAY_NAME, ulObjId, 0, ulObjId, 0, ulObjType, &sPropVal) == erSuccess) {
			er = FixPropEncoding(soap, stringCompat, Out, &sPropVal);
			if (er != erSuccess)
				goto exit;
				
		    sChildProps.lpPropTags->AddPropTag(sPropVal.ulPropTag);
		    sChildProps.lpPropVals->AddPropVal(sPropVal);
		}

		if(ECGenProps::GetPropComputedUncached(soap, NULL, lpecSession, PR_MAPPING_SIGNATURE, ulObjId, 0, 0, 0, ulObjType, &sPropVal) == erSuccess) {
		    sChildProps.lpPropTags->AddPropTag(PR_MAPPING_SIGNATURE);
		    sChildProps.lpPropVals->AddPropVal(sPropVal);
		}

		if (!sChildProps.lpPropTags->HasPropTag(PR_SORT_LOCALE_ID)) {
			sPropVal.__union = SOAP_UNION_propValData_ul;
			sPropVal.ulPropTag = PR_SORT_LOCALE_ID;
			sPropVal.Value.ul = lpecSession->GetSessionManager()->GetSortLCID(ulObjId);

			sChildProps.lpPropTags->AddPropTag(PR_SORT_LOCALE_ID);
			sChildProps.lpPropVals->AddPropVal(sPropVal);
		}
	}

	//PR_PARENT_SOURCE_KEY for folders and messages
	if(ulObjType == MAPI_FOLDER || (ulObjType == MAPI_MESSAGE && ulObjTypeParent == MAPI_FOLDER))
	{
		if(ECGenProps::GetPropComputedUncached(soap, NULL, lpecSession, PR_PARENT_SOURCE_KEY, ulObjId, 0, 0, 0, ulObjType, &sPropVal) == erSuccess)
		{
		    sChildProps.lpPropTags->AddPropTag(sPropVal.ulPropTag);
		    sChildProps.lpPropVals->AddPropVal(sPropVal);
		}

		if(ECGenProps::GetPropComputedUncached(soap, NULL, lpecSession, PR_SOURCE_KEY, ulObjId, 0, 0, 0, ulObjType, &sPropVal) == erSuccess)
		{
		    sChildProps.lpPropTags->AddPropTag(sPropVal.ulPropTag);
		    sChildProps.lpPropVals->AddPropVal(sPropVal);
		}
	}

	if (ulObjType == MAPI_MESSAGE || ulObjType == MAPI_ATTACH) {
		ULONG ulPropTag = (ulObjType == MAPI_MESSAGE ? PR_EC_IMAP_EMAIL : PR_ATTACH_DATA_BIN);
		er = CreateAttachmentStorage(lpDatabase, &lpAttachmentStorage);
		if (er != erSuccess)
			goto exit;

		if (lpAttachmentStorage->ExistAttachment(ulObjId, PROP_ID(ulPropTag)))
		    sChildProps.lpPropTags->AddPropTag(ulPropTag);
	}

	if (ulObjType == MAPI_MAILUSER || ulObjType == MAPI_DISTLIST) {
		if(ECGenProps::GetPropComputedUncached(soap, NULL, lpecSession, PR_INSTANCE_KEY, ulObjId, 0, 0, 0, ulObjType, &sPropVal) == erSuccess) {
		    sChildProps.lpPropTags->AddPropTag(sPropVal.ulPropTag);
		    sChildProps.lpPropVals->AddPropVal(sPropVal);
		}
	}

	// Set the PR_RECORD_KEY
	if (ulObjType != MAPI_ATTACH || !sChildProps.lpPropTags->HasPropTag(PR_RECORD_KEY)) {
		if (ECGenProps::GetPropComputedUncached(soap, NULL, lpecSession, PR_RECORD_KEY, ulObjId, 0, 0, 0, ulObjType, &sPropVal) == erSuccess) {
			sChildProps.lpPropTags->AddPropTag(sPropVal.ulPropTag);
			sChildProps.lpPropVals->AddPropVal(sPropVal);
		}
	}

	if (ulObjType == MAPI_FOLDER || ulObjType == MAPI_STORE || ulObjType == MAPI_MESSAGE) {
        // Get PARENT_ENTRYID
        strQuery = "SELECT hierarchy.parent,hierarchy.flags,hierarchy.type FROM hierarchy WHERE hierarchy.id="+stringify(ulObjId);
        er = lpDatabase->DoSelect(strQuery, &lpDBResult);
        if(er != erSuccess)
            goto exit;

        if(lpDatabase->GetNumRows(lpDBResult) > 0)
        {
            lpDBRow = lpDatabase->FetchRow(lpDBResult);

            if(lpDBRow && lpDBRow[0]) {

                if(ECGenProps::GetPropComputedUncached(soap, NULL, lpecSession, PR_PARENT_ENTRYID, ulObjId, 0, 0, 0, ulObjType, &sPropVal) == erSuccess)
                {
                    sChildProps.lpPropTags->AddPropTag(sPropVal.ulPropTag);
                    sChildProps.lpPropVals->AddPropVal(sPropVal);
                }
            }

            if(lpDBRow && lpDBRow[1] && lpDBRow[2] && atoi(lpDBRow[2]) == 3) {
                // PR_RIGHTS
                if (ECGenProps::GetPropComputedUncached(soap, NULL, lpecSession, PR_RIGHTS, ulObjId, 0, 0, 0, ulObjType, &sPropVal) == erSuccess) {
                    sChildProps.lpPropTags->AddPropTag(sPropVal.ulPropTag);
                    sChildProps.lpPropVals->AddPropVal(sPropVal);
                }
            }

            // Set the flags PR_ACCESS and PR_ACCESS_LEVEL
            if(lpDBRow && lpDBRow[0] && lpDBRow[2] && (atoi(lpDBRow[2]) == 3 || atoi(lpDBRow[2]) == 5) && lpDBRow[1])
            {
                if (ECGenProps::GetPropComputedUncached(soap, NULL, lpecSession, PR_ACCESS, ulObjId, 0, 0, 0, ulObjType, &sPropVal) == erSuccess) {
                    sChildProps.lpPropTags->AddPropTag(sPropVal.ulPropTag);
                    sChildProps.lpPropVals->AddPropVal(sPropVal);
                }
            }

            if(lpDBRow && lpDBRow[2] && lpDBRow[1])
            {
                if (ECGenProps::GetPropComputedUncached(soap, NULL, lpecSession, PR_ACCESS_LEVEL, ulObjId, 0, 0, 0, ulObjType, &sPropVal) == erSuccess) {
                    sChildProps.lpPropTags->AddPropTag(sPropVal.ulPropTag);
                    sChildProps.lpPropVals->AddPropVal(sPropVal);
                }
            }
        }
    }
    
	er = sChildProps.lpPropTags->GetPropTagArray(lpsPropTag);
	if(er != erSuccess)
	    goto exit;
	    
	er = sChildProps.lpPropVals->GetPropValArray(lpsPropVal);
	if(er != erSuccess)
	    goto exit;


exit:
	FREE_DBRESULT();

	if (lpAttachmentStorage)
		lpAttachmentStorage->Release();

	if (lpDBResultMV)
		lpDatabase->FreeResult(lpDBResultMV);

__soapentry_exit:
	return er;
}

/**
 * loadProp: Reads a single, large property from the database. No size limit.
 *   this can also be a complete attachment
 */
SOAP_ENTRY_START(loadProp, lpsResponse->er, entryId sEntryId, unsigned int ulObjId, unsigned int ulPropTag, struct loadPropResponse *lpsResponse)
{
	ECAttachmentStorage *lpAttachmentStorage = NULL;
	USE_DATABASE();

	if(ulObjId == 0) {
        er = lpecSession->GetObjectFromEntryId(&sEntryId, &ulObjId);
        if (er != erSuccess)
            goto exit;
            
    }
    
	// Check permission
	er = lpecSession->GetSecurity()->CheckPermission(ulObjId, ecSecurityRead);
	if(er != erSuccess)
		goto exit;

	er = lpDatabase->Begin();
	if(er != erSuccess)
		goto exit;

	if (ulPropTag != PR_ATTACH_DATA_BIN && ulPropTag != PR_EC_IMAP_EMAIL) {
		if((ulPropTag&MV_FLAG) == MV_FLAG)
			strQuery = "SELECT " + (std::string)MVPROPCOLORDER + " FROM mvproperties WHERE hierarchyid="+stringify(ulObjId)+ " AND tag = " + stringify(PROP_ID(ulPropTag))+" GROUP BY hierarchyid, tag";
		else
			strQuery = "SELECT " PROPCOLORDER " FROM properties WHERE hierarchyid = " + stringify(ulObjId) + " AND tag = " + stringify(PROP_ID(ulPropTag));
		er = lpDatabase->DoSelect(strQuery, &lpDBResult);
		if(er != erSuccess)
			goto exit;

		if(lpDatabase->GetNumRows(lpDBResult) != 1) {
			er = ZARAFA_E_NOT_FOUND;
			goto exit;
		}

		lpDBRow = lpDatabase->FetchRow(lpDBResult);
		lpDBLen = lpDatabase->FetchRowLengths(lpDBResult);

		if (lpDBRow == NULL || lpDBLen == NULL)
		{
			er = ZARAFA_E_DATABASE_ERROR;
			goto exit;
		}

		lpsResponse->lpPropVal = s_alloc<propVal>(soap);
		memset(lpsResponse->lpPropVal,0,sizeof(struct propVal));

		er = CopyDatabasePropValToSOAPPropVal(soap, lpDBRow, lpDBLen, lpsResponse->lpPropVal);
	} else {

		lpsResponse->lpPropVal = s_alloc<propVal>(soap);
		memset(lpsResponse->lpPropVal,0,sizeof(struct propVal));

		lpsResponse->lpPropVal->ulPropTag = ulPropTag;
		lpsResponse->lpPropVal->__union = SOAP_UNION_propValData_bin;
		lpsResponse->lpPropVal->Value.bin = s_alloc<struct xsd__base64Binary>(soap);
		lpsResponse->lpPropVal->Value.bin->__size = 0;

		er = CreateAttachmentStorage(lpDatabase, &lpAttachmentStorage);
		if (er != erSuccess)
			goto exit;

		er = lpAttachmentStorage->LoadAttachment(soap, ulObjId, PROP_ID(ulPropTag), &lpsResponse->lpPropVal->Value.bin->__size, &lpsResponse->lpPropVal->Value.bin->__ptr);
		if (er != erSuccess)
			goto exit;
	}

	er = lpDatabase->Commit();
	if(er != erSuccess)
		goto exit;

	if (!bSupportUnicode) {
		er = FixPropEncoding(soap, stringCompat, Out, lpsResponse->lpPropVal);
		if (er != erSuccess)
			goto exit;
	}

exit:
	if (lpAttachmentStorage)
		lpAttachmentStorage->Release();

	ROLLBACK_ON_ERROR();

    FREE_DBRESULT();
}
SOAP_ENTRY_END()

//TODO: flag to get size of normal folder or deleted folders
ECRESULT GetFolderSize(ECDatabase* lpDatabase, unsigned int ulFolderId, long long* lpllFolderSize)
{

	ECRESULT		er = erSuccess;
	DB_RESULT		lpDBResult = NULL;
	DB_ROW			lpDBRow = NULL;
	long long		llSize = 0;
	long long		llSubSize = 0;
	std::string		strQuery;

	// sum size of all messages in a folder
	strQuery = "SELECT SUM(p.val_ulong) FROM hierarchy AS h JOIN properties AS p ON p.hierarchyid=h.id AND p.tag="+stringify(PROP_ID(PR_MESSAGE_SIZE))+" AND p.type="+stringify(PROP_TYPE(PR_MESSAGE_SIZE))+" WHERE h.parent="+stringify(ulFolderId)+" AND h.type="+stringify(MAPI_MESSAGE);
	// except the deleted items!
	strQuery += " AND h.flags & " + stringify(MSGFLAG_DELETED)+ "=0";

	er = lpDatabase->DoSelect(strQuery, &lpDBResult);
	if(er != erSuccess)
		goto exit;

	lpDBRow = lpDatabase->FetchRow(lpDBResult);
	if(lpDBRow == NULL || lpDBRow[0] == NULL)
		llSize = 0;
	else
		llSize = _atoi64(lpDBRow[0]);

	// Free results
	if(lpDBResult) { lpDatabase->FreeResult(lpDBResult); lpDBResult = NULL; }

	// Get the subfolders
	strQuery = "SELECT id FROM hierarchy WHERE parent=" + stringify(ulFolderId) + " AND type="+stringify(MAPI_FOLDER);
	// except the deleted items!
	strQuery += " AND flags & " + stringify(MSGFLAG_DELETED)+ "=0";

	er = lpDatabase->DoSelect(strQuery, &lpDBResult);
	if(er != erSuccess)
		goto exit;

	if(lpDatabase->GetNumRows(lpDBResult) > 0)
	{
		// Walk through the folder list
		while( (lpDBRow = lpDatabase->FetchRow(lpDBResult)) )
		{
			if(lpDBRow[0] == NULL)
				continue; //Skip item

			er = GetFolderSize(lpDatabase, atoi(lpDBRow[0]), &llSubSize);
			if(er != erSuccess)
				goto exit;

			llSize += llSubSize;
		}
	}

	// Free results
	if(lpDBResult) { lpDatabase->FreeResult(lpDBResult); lpDBResult = NULL; }

	*lpllFolderSize = llSize;

exit:
	// Free results
	if(lpDBResult)
		lpDatabase->FreeResult(lpDBResult);

	return er;
}

/**
 * Write properties to an object
 *
 * @param[in] soap 
 * @param[in] lpecSession Pointer to the session of the caller; Cannot be NULL.
 * @param[in] lpDatabase Pointer to the database handler of the caller; Cannot be NULL. 
 * @param[in] lpAttachmentStorage Pointer to an attachment storage object; Cannot be NULL.
 * @param[in] lpsSaveObj Data object which include the new propery information; Cannot be NULL.
 * @param[in] ulObjId Identify the database object to write the property data to the database.
 * @param[in] fNewItem false for an existing object, true for a new object.
 * @param[in] ulSyncId Client sync identifier.
 * @param[out] lpsReturnObj
 * @param[out] lpfHaveChangeKey
 * @param[out] lpftCreated Receives create time if new object (can be now or passed value in lpsSaveObj)
 * @param[out] lpftModified Receives modification time of written object (can be now or passed value in lpsSaveObj)
 *
 * \remarks
 * 		Check the permissions and quota before you call this function.
 *
 * @todo unclear comment -> sync id only to saveObject !
 */
ECRESULT WriteProps(struct soap *soap, ECSession *lpecSession, ECDatabase *lpDatabase, ECAttachmentStorage *lpAttachmentStorage, struct saveObject *lpsSaveObj, unsigned int ulObjId, bool fNewItem, unsigned int ulSyncId, struct saveObject *lpsReturnObj, bool *lpfHaveChangeKey, FILETIME *lpftCreated, FILETIME *lpftModified)
{
	ECRESULT		er;
    std::string		strInsertQuery;
	std::string		strColName;
	std::string		strQuery;

	struct propValArray *lpPropValArray = &lpsSaveObj->modProps;

	unsigned int	ulParent = 0;
	unsigned int	ulGrandParent = 0;
	unsigned int	ulParentType = 0;
	unsigned int	ulObjType = 0;
	unsigned int	ulOwner = 0;
	unsigned int	ulFlags = 0;
	unsigned int	ulAffected = 0;
	unsigned int	j;
	int				i;
	unsigned int	ulPropInserts = 0;

	ULONG			nMVItems;
	unsigned long long ullIMAP = 0;

	std::set<unsigned int>	setInserted;
	std::set<unsigned int>::iterator iterInserted;

	GUID sGuidServer;
	std::list<ULONG> lstObjIds;
	ULONG ulInstanceId = 0;
	ULONG ulInstanceTag = 0;
	bool bAttachmentStored = false;
	entryId sUserId;
	std::string strUsername;

	ECStringCompat stringCompat(lpecSession->GetCapabilities() & ZARAFA_CAP_UNICODE);
	std::string	strColData;
	
	std::string strInsert;
	std::string strInsertTProp;
	
	SOURCEKEY		sSourceKey;
	SOURCEKEY		sParentSourceKey;

	DB_RESULT		lpDBResult = NULL;

	if(!lpAttachmentStorage) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	if (lpfHaveChangeKey)
		*lpfHaveChangeKey = false;

	er = g_lpSessionManager->GetServerGUID(&sGuidServer);
	if (er != erSuccess)
		goto exit;

    er = g_lpSessionManager->GetCacheManager()->GetObject(ulObjId, &ulParent, &ulOwner, &ulFlags, &ulObjType);
    if(er != erSuccess)
        goto exit;

	if(ulObjType != MAPI_STORE){
		er = g_lpSessionManager->GetCacheManager()->GetObject(ulParent, &ulGrandParent, NULL, NULL, &ulParentType);
		if(er != erSuccess)
			goto exit;
	}

	if(ulObjType == MAPI_FOLDER && !ulSyncId)
	{
		for(i=0;i<lpPropValArray->__size;i++) {

			// Check whether the requested folder name already exists
			if(lpPropValArray->__ptr[i].ulPropTag == PR_DISPLAY_NAME )
			{
				if(lpPropValArray->__ptr[i].Value.lpszA == NULL)
					break; // Name property found, but name isn't present. This is broken, so skip this.

				strQuery = "SELECT hierarchy.id FROM hierarchy JOIN properties ON hierarchy.id = properties.hierarchyid WHERE hierarchy.parent=" + stringify(ulParent) + " AND hierarchy.type="+stringify(MAPI_FOLDER)+" AND hierarchy.flags & " + stringify(MSGFLAG_DELETED)+ "=0 AND properties.tag=" + stringify(ZARAFA_TAG_DISPLAY_NAME) + " AND properties.val_string = '" + lpDatabase->Escape(lpPropValArray->__ptr[i].Value.lpszA) + "' AND properties.type="+stringify(PT_STRING8)+" AND hierarchy.id!=" + stringify(ulObjId);
				er = lpDatabase->DoSelect(strQuery, &lpDBResult);
				if(er != erSuccess)
					goto exit;

				if(lpDatabase->GetNumRows(lpDBResult) > 0) {
					er = ZARAFA_E_COLLISION;
					goto exit;
				}

				// Free results
				if(lpDBResult) { lpDatabase->FreeResult(lpDBResult); lpDBResult = NULL; }

				break;
			}
		}// for(...)
	}



	// If we have a recipient, remove the old one. The client does not send a diff of props because ECMemTable cannot do this
	if (!fNewItem && (ulObjType == MAPI_MAILUSER || ulObjType == MAPI_DISTLIST)) {
		strQuery = "DELETE FROM properties WHERE hierarchyid=" + stringify(ulObjId);

		er = lpDatabase->DoDelete(strQuery);
		if(er != erSuccess)
			goto exit;

		strQuery = "DELETE FROM mvproperties WHERE hierarchyid=" + stringify (ulObjId);

		er = lpDatabase->DoDelete(strQuery);
		if(er != erSuccess)
			goto exit;

		if (ulObjType != lpsSaveObj->ulObjType) {
			strQuery = "UPDATE hierarchy SET type=" +stringify(lpsSaveObj->ulObjType) +" WHERE id=" + stringify (ulObjId);
			er = lpDatabase->DoUpdate(strQuery);
			if(er != erSuccess)
				 goto exit;

			// Switch the type so the cache will be updated at the end.
			ulObjType = lpsSaveObj->ulObjType;
		}
	}

	/* FIXME: Support multiple InstanceIds */
	if (lpsSaveObj->lpInstanceIds && lpsSaveObj->lpInstanceIds->__size) {
		GUID sGuidTmp;

		if (lpsSaveObj->lpInstanceIds->__size > 1) {
			er = ZARAFA_E_UNKNOWN_INSTANCE_ID;
			goto exit;
		}

		er = SIEntryIDToID(&lpsSaveObj->lpInstanceIds->__ptr[0], &sGuidTmp, (unsigned int*)&ulInstanceId, (unsigned int*)&ulInstanceTag);
		if (er != erSuccess)
			goto exit;

		/* Server GUID must always match */
		if (memcmp(&sGuidTmp, &sGuidServer, sizeof(sGuidTmp)) != 0) {
			er = ZARAFA_E_UNKNOWN_INSTANCE_ID;
			goto exit;
		}

		/* The attachment should at least exist */
		if (!lpAttachmentStorage->ExistAttachmentInstance(ulInstanceId)) {
			er = ZARAFA_E_UNKNOWN_INSTANCE_ID;
			goto exit;
		}

		/*
		 * Check if we have access to the instance which is being referenced,
		 * a user has access to an instance when he is administrator or owns at
		 * least one reference to the instance.
		 * For security we won't announce the difference between not finding the instance
		 * or not having access to it.
		 */
		if (lpecSession->GetSecurity()->GetAdminLevel() != ADMIN_LEVEL_SYSADMIN) {
			er = lpAttachmentStorage->GetSingleInstanceParents(ulInstanceId, &lstObjIds);
			if (er != erSuccess)
				goto exit;

			er = ZARAFA_E_UNKNOWN_INSTANCE_ID;
			for (std::list<ULONG>::iterator i = lstObjIds.begin(); i != lstObjIds.end(); i++) {
				if (lpecSession->GetSecurity()->CheckPermission(*i, ecSecurityRead) == erSuccess) {
						er = erSuccess;
						break;
				}
			}

			if (er != erSuccess)
				goto exit;
		}

		er = lpAttachmentStorage->SaveAttachment(ulObjId, ulInstanceTag, !fNewItem, ulInstanceId, &ulInstanceId);
		if (er != erSuccess)
			goto exit;

		lpsReturnObj->lpInstanceIds = s_alloc<entryList>(soap);
		lpsReturnObj->lpInstanceIds->__size = 1;
		lpsReturnObj->lpInstanceIds->__ptr = s_alloc<entryId>(soap, lpsReturnObj->lpInstanceIds->__size);

		er = SIIDToEntryID(soap, &sGuidServer, ulInstanceId, ulInstanceTag, &lpsReturnObj->lpInstanceIds->__ptr[0]);
		if (er != erSuccess) {
			lpsReturnObj->lpInstanceIds = NULL;
			goto exit;
		}

		/* Either by instanceid or attachment data, we have stored the attachment */
		bAttachmentStored = true;
	}

	// Write the properties
	for(i=0;i<lpPropValArray->__size;i++) {
	    // Check if we already inserted this property tag. We only accept the first.
	    iterInserted = setInserted.find(lpPropValArray->__ptr[i].ulPropTag);
	    if(iterInserted != setInserted.end())
	        continue;

        // Check if we actually need to write this property. The client may send us properties
        // that we generate, so we don't need to save them
        if(ECGenProps::IsPropRedundant(lpPropValArray->__ptr[i].ulPropTag, ulObjType) == erSuccess)
            continue;

		// Some properties may only be saved on the first save (fNewItem == TRUE) for folders, stores and messages
		if(!fNewItem && (ulObjType == MAPI_MESSAGE || ulObjType == MAPI_FOLDER || ulObjType == MAPI_STORE)) {
			switch(lpPropValArray->__ptr[i].ulPropTag) {
				case PR_LAST_MODIFICATION_TIME:
				case PR_MESSAGE_FLAGS:
				case PR_SEARCH_KEY:
				case PR_LAST_MODIFIER_NAME:
				case PR_LAST_MODIFIER_ENTRYID:
					if(ulSyncId == 0)
						// Only on first write, unless sent by ICS
						continue;
                    break;
				case PR_SOURCE_KEY:
					if(ulParentType == MAPI_FOLDER)
						// Only on first write, unless message-in-message
						continue;
                    break;
			}
		}

		if(lpPropValArray->__ptr[i].ulPropTag == PR_LAST_MODIFIER_NAME_W || lpPropValArray->__ptr[i].ulPropTag == PR_LAST_MODIFIER_ENTRYID) {
			if(!fNewItem && ulSyncId == 0)
                continue;
		}

		// Same goes for flags in PR_MESSAGE_FLAGS
		if(lpPropValArray->__ptr[i].ulPropTag == PR_MESSAGE_FLAGS) {
			// Normalize PR_MESSAGE_FLAGS so that the user cannot set things like MSGFLAG_ASSOCIATED 
		    lpPropValArray->__ptr[i].Value.ul = (lpPropValArray->__ptr[i].Value.ul & (MSGFLAG_SETTABLE_BY_USER | MSGFLAG_SETTABLE_BY_SPOOLER)) | ulFlags;
		}

		// Make sure we dont have a colliding PR_SOURCE_KEY. This can happen if a user imports an exported message for example.
		if(lpPropValArray->__ptr[i].ulPropTag == PR_SOURCE_KEY)
		{
		    // Remove any old (deleted) indexed property if it's there
		    er = RemoveStaleIndexedProp(lpDatabase, PR_SOURCE_KEY, lpPropValArray->__ptr[i].Value.bin->__ptr, lpPropValArray->__ptr[i].Value.bin->__size);
		    if(er != erSuccess) {
		        // Unable to remove the (old) sourcekey in use. This means that it is in use by some other object. We just skip
		        // the property so that it is generated later as a new random sourcekey
		        er = erSuccess;
				continue;
            }
				
			// Insert sourcekey, use REPLACE because createfolder already created a sourcekey.
			// Because there is a non-primary unique key on the
			// val_binary part of the table, it will fail if the source key is duplicate.
			strQuery = "REPLACE INTO indexedproperties(hierarchyid,tag,val_binary) VALUES ("+stringify(ulObjId)+","+stringify(PROP_ID(PR_SOURCE_KEY))+","+lpDatabase->EscapeBinary(lpPropValArray->__ptr[i].Value.bin->__ptr, lpPropValArray->__ptr[i].Value.bin->__size)+")";
			er = lpDatabase->DoInsert(strQuery);
			if(er != erSuccess)
				goto exit;

			setInserted.insert(lpPropValArray->__ptr[i].ulPropTag);

			// Remember the source key in the cache
			g_lpSessionManager->GetCacheManager()->SetObjectProp(PROP_ID(PR_SOURCE_KEY), lpPropValArray->__ptr[i].Value.bin->__size, lpPropValArray->__ptr[i].Value.bin->__ptr, ulObjId);
			continue;
		}

		// attachments are in the blob too
		if (lpPropValArray->__ptr[i].ulPropTag == PR_ATTACH_DATA_BIN || lpPropValArray->__ptr[i].ulPropTag == PR_EC_IMAP_EMAIL) {
			/* 
			 * bAttachmentStored indicates we already processed the attachment.
			 * this could happen when the user provided the instance ID but as
			 * backup also send the PR_ATTACH_DATA_BIN.
			 */
			if (bAttachmentStored)
				continue;

			er = lpAttachmentStorage->SaveAttachment(ulObjId, PROP_ID(lpPropValArray->__ptr[i].ulPropTag), !fNewItem,
													 lpPropValArray->__ptr[i].Value.bin->__size,
													 lpPropValArray->__ptr[i].Value.bin->__ptr,
													 &ulInstanceId);
			if (er != erSuccess)
				goto exit;

			lpsReturnObj->lpInstanceIds = s_alloc<entryList>(soap);
			lpsReturnObj->lpInstanceIds->__size = 1;
			lpsReturnObj->lpInstanceIds->__ptr = s_alloc<entryId>(soap, lpsReturnObj->lpInstanceIds->__size);

			er = SIIDToEntryID(soap, &sGuidServer, ulInstanceId, PROP_ID(lpPropValArray->__ptr[i].ulPropTag), &lpsReturnObj->lpInstanceIds->__ptr[0]);
			if (er != erSuccess) {
				lpsReturnObj->lpInstanceIds = NULL;
				goto exit;
			}

			continue;
		}
		
		// We have to return the values for PR_LAST_MODIFICATION_TIME and PR_CREATION_TIME
		if (lpPropValArray->__ptr[i].ulPropTag == PR_LAST_MODIFICATION_TIME) {
			lpftModified->dwHighDateTime = lpPropValArray->__ptr[i].Value.hilo->hi;
			lpftModified->dwLowDateTime = lpPropValArray->__ptr[i].Value.hilo->lo;
		}
		
		if (lpPropValArray->__ptr[i].ulPropTag == PR_CREATION_TIME) {
			lpftCreated->dwHighDateTime = lpPropValArray->__ptr[i].Value.hilo->hi;
			lpftCreated->dwLowDateTime = lpPropValArray->__ptr[i].Value.hilo->lo;
		}
		
		if (lpfHaveChangeKey && lpPropValArray->__ptr[i].ulPropTag == PR_CHANGE_KEY)
			*lpfHaveChangeKey = true;


		if((PROP_TYPE(lpPropValArray->__ptr[i].ulPropTag) & MV_FLAG) == MV_FLAG) {
			// Make sure string prop_types become PT_MV_STRING8
			if (PROP_TYPE(lpPropValArray->__ptr[i].ulPropTag) == PT_MV_UNICODE)
				lpPropValArray->__ptr[i].ulPropTag = CHANGE_PROP_TYPE(lpPropValArray->__ptr[i].ulPropTag, PT_MV_STRING8);

			//Write mv properties
			nMVItems = GetMVItemCount(&lpPropValArray->__ptr[i]);
			for(j=0; j < nMVItems; j++)
			{
				ASSERT(PROP_TYPE(lpPropValArray->__ptr[i].ulPropTag) != PT_MV_UNICODE);

				// Make sure string propvals are in UTF8
				if (PROP_TYPE(lpPropValArray->__ptr[i].ulPropTag) == PT_MV_STRING8)
					lpPropValArray->__ptr[i].Value.mvszA.__ptr[j] = stringCompat.to_UTF8(soap, lpPropValArray->__ptr[i].Value.mvszA.__ptr[j]);

				er = CopySOAPPropValToDatabaseMVPropVal(&lpPropValArray->__ptr[i], j, strColName, strColData, lpDatabase);
				if(er != erSuccess)
					continue;

				strQuery = "REPLACE INTO mvproperties(hierarchyid,orderid,tag,type," + strColName + ") VALUES(" + stringify(ulObjId) + "," + stringify(j) + "," + stringify(PROP_ID(lpPropValArray->__ptr[i].ulPropTag)) + "," + stringify(PROP_TYPE(lpPropValArray->__ptr[i].ulPropTag)) + "," + strColData + ")";
				er = lpDatabase->DoInsert(strQuery, NULL, &ulAffected);
				if(er != erSuccess)
					goto exit;

				// According to the MySQL documentation (http://dev.mysql.com/doc/refman/5.0/en/mysql-affected-rows.html) ulAffected rows
				// will be 2 if a row was replaced.
				// Interestingly, I (MSw) have observer in a consecutive call to the above replace query, where in both cases an old value
				// was replaced with a new value, that it returned 1 the first time and 2 the second time.
				// We'll allow both though.
				if(ulAffected != 1 && ulAffected != 2) {
					g_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_FATAL, "Unable to update MVProperties during save: %d, object id: %d", ulAffected, ulObjId);
					er = ZARAFA_E_DATABASE_ERROR;
					goto exit;
				}
			}

			if(!fNewItem) {
				strQuery = "DELETE FROM mvproperties WHERE hierarchyid=" + stringify (ulObjId) +
							" AND tag=" + stringify(PROP_ID(lpPropValArray->__ptr[i].ulPropTag)) +
							" AND type=" + stringify(PROP_TYPE(lpPropValArray->__ptr[i].ulPropTag)) +
							" AND orderid >= " + stringify(nMVItems);

				er = lpDatabase->DoDelete(strQuery);
				if(er != erSuccess)
					goto exit;
			}

		} else {
            // Make sure string propvals are in UTF8 with tag PT_STRING8
            if (PROP_TYPE(lpPropValArray->__ptr[i].ulPropTag) == PT_STRING8 || PROP_TYPE(lpPropValArray->__ptr[i].ulPropTag) == PT_UNICODE) {
            	lpPropValArray->__ptr[i].Value.lpszA = stringCompat.to_UTF8(soap, lpPropValArray->__ptr[i].Value.lpszA);
                lpPropValArray->__ptr[i].ulPropTag = CHANGE_PROP_TYPE(lpPropValArray->__ptr[i].ulPropTag, PT_STRING8);
		    }
                                                                    
			// Write the property to the database
			er = WriteSingleProp(lpDatabase, ulObjId, ulParent, &lpPropValArray->__ptr[i], false, lpDatabase->GetMaxAllowedPacket(), strInsert);
			if (er == ZARAFA_E_TOO_BIG) {
				er = lpDatabase->DoInsert(strInsert);
				if (er == erSuccess) {
					strInsert.clear();
					er = WriteSingleProp(lpDatabase, ulObjId, ulParent, &lpPropValArray->__ptr[i], false, lpDatabase->GetMaxAllowedPacket(), strInsert);
				}
			}
			if(er != erSuccess)
				goto exit;
			
			// Write the property to the table properties if needed (only on objects in folders (folders, messages), and if the property is being tracked here.
			if(ulParentType == MAPI_FOLDER) {
				// Cache the written value
				sObjectTableKey key(ulObjId,0);
				g_lpSessionManager->GetCacheManager()->SetCell(&key, lpPropValArray->__ptr[i].ulPropTag, &lpPropValArray->__ptr[i]);
				
				if (0) {
					// FIXME do we need this code? Currently we get always a deferredupdate!
					// Please also update streamutil.cpp:DeserializeProps
					er = WriteSingleProp(lpDatabase, ulObjId, ulParent, &lpPropValArray->__ptr[i], true, lpDatabase->GetMaxAllowedPacket(), strInsertTProp);
					if (er == ZARAFA_E_TOO_BIG) {
						er = lpDatabase->DoInsert(strInsertTProp);
						if (er == erSuccess) {
							strInsertTProp.clear();
							er = WriteSingleProp(lpDatabase, ulObjId, ulParent, &lpPropValArray->__ptr[i], true, lpDatabase->GetMaxAllowedPacket(), strInsertTProp);
						}
					}
					if(er != erSuccess)
						goto exit;
				}
			}
		}

		setInserted.insert(lpPropValArray->__ptr[i].ulPropTag);
	} // for(i=0;i<lpPropValArray->__size;i++)

	if(!strInsert.empty()) {
		er = lpDatabase->DoInsert(strInsert);
		if(er != erSuccess)
			goto exit;
	}
	
	if(ulParentType == MAPI_FOLDER) {
		if(0) {
			/* Modification, just directly write the tproperties
			 * The idea behind this is that we'd need some serious random-access reads to properties later when flushing
			 * tproperties, and we have the properties in memory now anyway. Also, modifications usually are just a few properties, causing
			 * only minor random I/O on tproperties, and a tproperties flush reads all the properties, not just the modified ones.
			 */
			if(!strInsertTProp.empty()) {
				er = lpDatabase->DoInsert(strInsertTProp);
				if(er != erSuccess)
					goto exit;
			}
		} else {
			// Instead of writing directly to tproperties, save a delayed write request.
			if(ulParent != CACHE_NO_PARENT) {
                er = ECTPropsPurge::AddDeferredUpdateNoPurge(lpDatabase, ulParent, 0, ulObjId);
                if(er != erSuccess)
                    goto exit;
			}
		}
	}
		
	// Insert the properties
	if(ulPropInserts > 0) {
		er = lpDatabase->DoInsert(strInsertQuery, NULL, NULL);
		if(er != erSuccess)
			goto exit;
	}
	
	if(ulObjType == MAPI_MESSAGE) {
		iterInserted = setInserted.find(PR_LAST_MODIFIER_NAME_W);
		
		// update the PR_LAST_MODIFIER_NAME and PR_LAST_MODIFIER_ENTRYID
		if(iterInserted == setInserted.end()) {
			er = GetABEntryID(lpecSession->GetSecurity()->GetUserId(), soap, &sUserId);
			if(er != erSuccess)
				goto exit;
			
			lpecSession->GetSecurity()->GetUsername(&strUsername);

			strQuery = "REPLACE INTO properties(hierarchyid, tag, type, val_string, val_binary) VALUES(" +
						stringify(ulObjId) + "," +
						stringify(PROP_ID(PR_LAST_MODIFIER_NAME_A)) + "," +
						stringify(PROP_TYPE(PR_LAST_MODIFIER_NAME_A)) + ",\"" +
						lpDatabase->Escape(strUsername) +
						"\", NULL), (" +
						stringify(ulObjId) + "," +
						stringify(PROP_ID(PR_LAST_MODIFIER_ENTRYID)) + "," +
						stringify(PROP_TYPE(PR_LAST_MODIFIER_ENTRYID)) +
						", NULL, " +
						lpDatabase->EscapeBinary(sUserId.__ptr, sUserId.__size) + ")";

			er = lpDatabase->DoInsert(strQuery);
			if(er != erSuccess)
				goto exit;

			sObjectTableKey key(ulObjId,0);
			struct propVal	sPropVal;

			sPropVal.ulPropTag = PR_LAST_MODIFIER_NAME_A;
			sPropVal.Value.lpszA = (char*)strUsername.c_str();
			sPropVal.__union = SOAP_UNION_propValData_lpszA;
            g_lpSessionManager->GetCacheManager()->SetCell(&key, PR_LAST_MODIFIER_NAME_A, &sPropVal);

			sPropVal.ulPropTag = PR_LAST_MODIFIER_ENTRYID;
			sPropVal.Value.bin = &sUserId;
			sPropVal.__union = SOAP_UNION_propValData_bin;
			g_lpSessionManager->GetCacheManager()->SetCell(&key, PR_LAST_MODIFIER_ENTRYID, &sPropVal);
		}
	}

	if(!fNewItem) {
		// Update, so write the modtime and clear UNMODIFIED flag
		if(setInserted.find(PR_LAST_MODIFICATION_TIME) == setInserted.end()) {
		    struct propVal sProp;
		    struct hiloLong sHilo;
		    struct sObjectTableKey key;
		    
		    FILETIME ft;
		    
		    sProp.ulPropTag = PR_LAST_MODIFICATION_TIME;
		    sProp.Value.hilo = &sHilo;
		    sProp.__union = SOAP_UNION_propValData_hilo;
			UnixTimeToFileTime(time(NULL), &sProp.Value.hilo->hi, &sProp.Value.hilo->lo);
			
			ft.dwHighDateTime = sProp.Value.hilo->hi;
			ft.dwLowDateTime = sProp.Value.hilo->lo;
			
			er = WriteProp(lpDatabase, ulObjId, ulParent, &sProp);
			if (er != erSuccess)
			    goto exit;
			
			*lpftModified = ft;
			
            // Add to cache
            key.ulObjId = ulObjId;
            key.ulOrderId = 0;
			if(ulParentType == MAPI_FOLDER)
                g_lpSessionManager->GetCacheManager()->SetCell(&key, PR_LAST_MODIFICATION_TIME, &sProp);
			
		}
		
		if(ulObjType == MAPI_MESSAGE) {
			// Unset MSGFLAG_UNMODIFIED
			strQuery = "UPDATE properties SET val_ulong=val_ulong&"+stringify(~MSGFLAG_UNMODIFIED) + " WHERE hierarchyid=" + stringify(ulObjId)+ " AND tag=" + stringify(PROP_ID(PR_MESSAGE_FLAGS)) + " AND type=" + stringify(PROP_TYPE(PR_MESSAGE_FLAGS));
			er = lpDatabase->DoUpdate(strQuery);
			if(er != erSuccess)
				goto exit;

			// Update cache	
			if(ulParentType == MAPI_FOLDER)
                g_lpSessionManager->GetCacheManager()->UpdateCell(ulObjId, PR_MESSAGE_FLAGS, (unsigned int)MSGFLAG_UNMODIFIED, 0);
		}
	} else {
		// New item, make sure PR_CREATION_TIME and PR_LAST_MODIFICATION_TIME are available
		if(setInserted.find(PR_LAST_MODIFICATION_TIME) == setInserted.end() || setInserted.find(PR_CREATION_TIME) == setInserted.end()) {
			struct propVal sPropTime;
			FILETIME ft;
			unsigned int tags[] = { PR_LAST_MODIFICATION_TIME, PR_CREATION_TIME };
			
			// Get current time
			UnixTimeToFileTime(time(NULL), &ft);
			
			sPropTime.Value.hilo = s_alloc<struct hiloLong>(soap);
			sPropTime.Value.hilo->hi = ft.dwHighDateTime;
			sPropTime.Value.hilo->lo = ft.dwLowDateTime;
			sPropTime.__union = SOAP_UNION_propValData_hilo;

			// Same thing for both PR_LAST_MODIFICATION_TIME and PR_CREATION_TIME			
			for(unsigned int i = 0; i < sizeof(tags)/sizeof(tags[0]); i++) {
				if(setInserted.find(tags[i]) == setInserted.end()) {
				    sObjectTableKey key;
					sPropTime.ulPropTag = tags[i];
					
					strQuery.clear();
					WriteSingleProp(lpDatabase, ulObjId, ulParent, &sPropTime, false, 0, strQuery);
					er = lpDatabase->DoInsert(strQuery);
					if(er != erSuccess)
						goto exit;
						
					strQuery.clear();
					WriteSingleProp(lpDatabase, ulObjId, ulParent, &sPropTime, true, 0, strQuery);
					er = lpDatabase->DoInsert(strQuery);
					if(er != erSuccess)
						goto exit;

					if(tags[i] == PR_LAST_MODIFICATION_TIME)
						*lpftModified = ft;
					if(tags[i] == PR_CREATION_TIME)
						*lpftCreated = ft;
						
                    // Add to cache
                    key.ulObjId = ulObjId;
                    key.ulOrderId = 0;
        			if(ulParentType == MAPI_FOLDER)
                        g_lpSessionManager->GetCacheManager()->SetCell(&key, tags[i], &sPropTime);
				}
			}
		}
	}

	if(fNewItem && ulObjType == MAPI_MESSAGE) {
        // Add PR_SOURCE_KEY to new messages without a given PR_SOURCE_KEY
        // This isn't for folders, this done in the createfolder function
		iterInserted = setInserted.find(PR_SOURCE_KEY);

		if(iterInserted == setInserted.end()) {
			er = lpecSession->GetNewSourceKey(&sSourceKey);
			if (er != erSuccess)
				goto exit;

			strQuery = "INSERT INTO indexedproperties(hierarchyid,tag,val_binary) VALUES(" + stringify(ulObjId) + "," + stringify(PROP_ID(PR_SOURCE_KEY)) + "," + lpDatabase->EscapeBinary(sSourceKey, sSourceKey.size()) + ")";
			er = lpDatabase->DoInsert(strQuery);

			if(er != erSuccess)
				goto exit;

			g_lpSessionManager->GetCacheManager()->SetObjectProp(PROP_ID(PR_SOURCE_KEY), sSourceKey.size(), sSourceKey, ulObjId);
		}


		if(ulParentType == MAPI_FOLDER) {
			// Add a PR_EC_IMAP_ID to the newly created message
		    sObjectTableKey key(ulObjId, 0);
			struct propVal sProp;

			er = g_lpSessionManager->GetNewSequence(ECSessionManager::SEQ_IMAP, &ullIMAP);
			if(er != erSuccess)
				goto exit;
				
			strQuery = "INSERT INTO properties(hierarchyid, tag, type, val_ulong) VALUES(" +
						stringify(ulObjId) + "," +
						stringify(PROP_ID(PR_EC_IMAP_ID)) + "," +
						stringify(PROP_TYPE(PR_EC_IMAP_ID)) + "," +
						stringify(ullIMAP) +
						")";
			er = lpDatabase->DoInsert(strQuery);
			if(er != erSuccess)
				goto exit;

			sProp.ulPropTag = PR_EC_IMAP_ID;
			sProp.Value.ul = ullIMAP;
			sProp.__union = SOAP_UNION_propValData_ul;
			er = g_lpSessionManager->GetCacheManager()->SetCell(&key, PR_EC_IMAP_ID, &sProp);
			if (er != erSuccess)
				goto exit;
		}
	}

	if(fNewItem && ulParentType == MAPI_FOLDER)
        // Since we have written a new item, we know that the cache contains *all* properties for this object
        g_lpSessionManager->GetCacheManager()->SetComplete(ulObjId);
        
	// We know the values for the object cache, so add them here
	if(ulObjType == MAPI_MESSAGE || ulObjType == MAPI_FOLDER)
		g_lpSessionManager->GetCacheManager()->SetObject(ulObjId, ulParent, ulOwner, ulFlags, ulObjType);
	
exit:

    FREE_DBRESULT();
	return er;
}

// You need to check the permissions before you call this function
ECRESULT DeleteProps(ECSession *lpecSession, ECDatabase *lpDatabase, ULONG ulObjId, struct propTagArray *lpsPropTags) {
	ECRESULT er = erSuccess;
	int				i;
	std::string		strQuery;
	sObjectTableKey key;
	struct propVal  sPropVal;

	// Delete one or more properties of an object
	for(i=0;i<lpsPropTags->__size;i++) {
		if((lpsPropTags->__ptr[i]&MV_FLAG) == 0)
			strQuery = "DELETE FROM properties WHERE hierarchyid="+stringify(ulObjId)+" AND tag="+stringify(PROP_ID(lpsPropTags->__ptr[i]));
		else // mvprops
			strQuery = "DELETE FROM mvproperties WHERE hierarchyid="+stringify(ulObjId)+" AND tag="+stringify(PROP_ID(lpsPropTags->__ptr[i]) );

		er = lpDatabase->DoDelete(strQuery);
		if(er != erSuccess)
			goto exit;
			
		// Remove from tproperties
		if((lpsPropTags->__ptr[i]&MV_FLAG) == 0) {
			strQuery = "DELETE FROM tproperties WHERE hierarchyid="+stringify(ulObjId)+" AND tag="+stringify(PROP_ID(lpsPropTags->__ptr[i]));

			er = lpDatabase->DoDelete(strQuery);
			if(er != erSuccess)
				goto exit;
		}			

		// Update cache with NOT_FOUND for this property
		key.ulObjId = ulObjId;
		key.ulOrderId = 0;
		sPropVal.ulPropTag = PROP_TAG(PT_ERROR, PROP_ID(lpsPropTags->__ptr[i]));
		sPropVal.Value.ul = ZARAFA_E_NOT_FOUND;
		sPropVal.__union = SOAP_UNION_propValData_ul;

		g_lpSessionManager->GetCacheManager()->SetCell(&key, lpsPropTags->__ptr[i], &sPropVal);
	}


exit:
	return er;
}

unsigned int SaveObject(struct soap *soap, ECSession *lpecSession, ECDatabase *lpDatabase, ECAttachmentStorage *lpAttachmentStorage,
						unsigned int ulStoreId, unsigned int ulParentObjId, unsigned int ulParentType, unsigned int ulFlags,
						unsigned int ulSyncId, struct saveObject *lpsSaveObj, struct saveObject *lpsReturnObj, unsigned int ulLevel, bool *lpfHaveChangeKey = NULL)
{
	ECRESULT er = erSuccess;
	ALLOC_DBRESULT();
	int n;
	unsigned int ulParentObjType = 0;
	unsigned int ulSize = 0;
	unsigned int ulObjId = 0;
	bool fNewItem = false;
	bool fHasAttach = false;
	bool fGenHasAttach = false;
	ECListDeleteItems lstDeleteItems;
	ECListDeleteItems lstDeleted;
	FILETIME ftCreated = { 0,0 };
	FILETIME ftModified = { 0,0 };

	if (ulLevel <= 0) {
	    er = ZARAFA_E_TOO_COMPLEX;
	    goto exit;
    }

	// reset return object
	lpsReturnObj->__size = 0;
	lpsReturnObj->__ptr = NULL;
	lpsReturnObj->delProps.__size = 0;
	lpsReturnObj->delProps.__ptr = NULL;
	lpsReturnObj->modProps.__size = 0;
	lpsReturnObj->modProps.__ptr = NULL;
	lpsReturnObj->bDelete = false;
	lpsReturnObj->ulClientId = lpsSaveObj->ulClientId;
	lpsReturnObj->ulServerId = lpsSaveObj->ulServerId;
	lpsReturnObj->ulObjType = lpsSaveObj->ulObjType;
	lpsReturnObj->lpInstanceIds = NULL;

	if (lpsSaveObj->ulServerId == 0) {
		if (ulParentObjId == 0) {
			er = ZARAFA_E_INVALID_PARAMETER;
			goto exit;
		}

		er = CreateObject(lpecSession, lpDatabase, ulParentObjId, ulParentType, lpsSaveObj->ulObjType, ulFlags, &lpsReturnObj->ulServerId);
		if (er != erSuccess)
			goto exit;

		fNewItem = true;

		ulObjId = lpsReturnObj->ulServerId;
	} else {
	    ulObjId = lpsSaveObj->ulServerId;
    }

	if (lpsSaveObj->bDelete) {
		// make list of all children object id's in std::list<int> ?
		ECListInt lstDel;

		lstDel.push_back(lpsSaveObj->ulServerId);

		// we always hard delete, because we can only delete submessages here
		// make sure we also delete message-in-message attachments, so all message related flags are on
		ULONG ulDelFlags = EC_DELETE_CONTAINER | EC_DELETE_MESSAGES | EC_DELETE_RECIPIENTS | EC_DELETE_ATTACHMENTS | EC_DELETE_HARD_DELETE;

		// Collect recursive parent objects, validate item and check the permissions
		er = ExpandDeletedItems(lpecSession, lpDatabase, &lstDel, ulDelFlags, false, &lstDeleteItems);
		if (er != erSuccess) {
			ASSERT(FALSE);
			goto exit;
		}

		er = DeleteObjectHard(lpecSession, lpDatabase, lpAttachmentStorage, ulDelFlags, lstDeleteItems, true, lstDeleted);
		if (er != erSuccess) {
			ASSERT(FALSE);
			goto exit;
		}

		er = DeleteObjectStoreSize(lpecSession, lpDatabase, ulDelFlags, lstDeleted);
		if (er != erSuccess) {
			ASSERT(FALSE);
			goto exit;
		}

		er = DeleteObjectCacheUpdate(lpecSession, ulDelFlags, lstDeleted);
		if (er != erSuccess) {
			ASSERT(FALSE);
			goto exit;
		}

		// object deleted, so we're done
		goto exit;
	}

	// ------
	// the following code is only for objects that still exist
	// ------
	{

	    // Don't delete properties if this is a new object: this avoids any delete queries that cause unneccessary locks on the tables
		if (lpsSaveObj->delProps.__size > 0 && !fNewItem) {
			er = DeleteProps(lpecSession, lpDatabase, lpsReturnObj->ulServerId, &lpsSaveObj->delProps);
			if (er != erSuccess)
				goto exit;
		}

		if (lpsSaveObj->modProps.__size > 0) {
			er = WriteProps(soap, lpecSession, lpDatabase, lpAttachmentStorage, lpsSaveObj, lpsReturnObj->ulServerId, fNewItem, ulSyncId, lpsReturnObj, lpfHaveChangeKey, &ftCreated, &ftModified);
			if (er != erSuccess)
				goto exit;
		}

		// check children
		if (lpsSaveObj->__size > 0) {
			lpsReturnObj->__size = lpsSaveObj->__size;
			lpsReturnObj->__ptr = s_alloc<struct saveObject>(soap, lpsReturnObj->__size);

			for (int i = 0; i < lpsSaveObj->__size; i++) {
				er = SaveObject(soap, lpecSession, lpDatabase, lpAttachmentStorage, ulStoreId, /*myself as parent*/lpsReturnObj->ulServerId, lpsReturnObj->ulObjType, 0, ulSyncId, &lpsSaveObj->__ptr[i], &lpsReturnObj->__ptr[i], lpsReturnObj->ulObjType == MAPI_MESSAGE ? ulLevel-1 : ulLevel);
				if (er != erSuccess)
					goto exit;
			}
		}
		
		if (lpsReturnObj->ulObjType == MAPI_MESSAGE) {
			// Generate properties that we need to generate (PR_HASATTACH, PR_LAST_MODIFICATION_TIME, PR_CREATION_TIME)
			if (fNewItem) {
				// We have to write PR_HASTTACH since it is a new object
				fGenHasAttach = true;
				// We can generate PR_HASATTACH from the passed object data
				for (int i = 0; i < lpsSaveObj->__size; i++) {
					if(lpsSaveObj->__ptr[i].ulObjType == MAPI_ATTACH) {
						fHasAttach = true;
						break;
					}
				}
			} else {
				// Modified object. Only change PR_HASATTACH if something has changed
				for (int i = 0; i < lpsSaveObj->__size; i++) {
					if(lpsSaveObj->__ptr[i].ulObjType == MAPI_ATTACH && (lpsSaveObj->__ptr[i].bDelete || lpsSaveObj->__ptr[i].ulServerId == 0)) {
						// An attachment was deleted or added in this call
						fGenHasAttach = true;
						break;
					}
				}
				if(fGenHasAttach) {
					// An attachment was added or deleted, check the database to see if any attachments are left.
					strQuery = "SELECT id FROM hierarchy WHERE parent=" + stringify(lpsReturnObj->ulServerId) + " AND type=" + stringify(MAPI_ATTACH) + " LIMIT 1";
					
					er = lpDatabase->DoSelect(strQuery, &lpDBResult);
					if(er != erSuccess)
						goto exit;
						
					fHasAttach = lpDatabase->GetNumRows(lpDBResult) > 0;
					
					lpDatabase->FreeResult(lpDBResult);
					lpDBResult = NULL;
				}
			}
			
			if(fGenHasAttach) {
				// We have to generate/update PR_HASATTACH
				unsigned int ulParentTmp, ulOwnerTmp, ulFlagsTmp, ulTypeTmp;
				
				sObjectTableKey key(lpsReturnObj->ulServerId, 0);
				struct propVal sPropHasAttach;
				sPropHasAttach.ulPropTag = PR_HASATTACH;
				sPropHasAttach.Value.b = fHasAttach;
				sPropHasAttach.__union = SOAP_UNION_propValData_b;
		
				// Write in properties		
				strQuery.clear();
				WriteSingleProp(lpDatabase, lpsReturnObj->ulServerId, ulParentObjId, &sPropHasAttach, false, 0, strQuery);
				er = lpDatabase->DoInsert(strQuery);
				if(er != erSuccess)
					goto exit;
					
				// Write in tproperties
				strQuery.clear();
				WriteSingleProp(lpDatabase, lpsReturnObj->ulServerId, ulParentObjId, &sPropHasAttach, true, 0, strQuery);
				er = lpDatabase->DoInsert(strQuery);
				if(er != erSuccess)
					goto exit;
				
				// Update cache, since it may have been written before by WriteProps with a possibly wrong value
				g_lpSessionManager->GetCacheManager()->SetCell(&key, PR_HASATTACH, &sPropHasAttach);
				
				// Update MSGFLAG_HASATTACH in the same way. We can assume PR_MESSAGE_FLAGS is already available, so we
				// just do an update (instead of REPLACE INTO)
				strQuery = (std::string)"UPDATE properties SET val_ulong = val_ulong " + (fHasAttach ? " | 0x10 " : " & ~0x10") + " WHERE hierarchyid = " + stringify(lpsReturnObj->ulServerId) + " AND tag = " + stringify(PROP_ID(PR_MESSAGE_FLAGS)) + " AND type = " + stringify(PROP_TYPE(PR_MESSAGE_FLAGS));
				er = lpDatabase->DoUpdate(strQuery);
				if(er != erSuccess)
					goto exit;
					
				// Update cache if it's actually in the cache
				if(g_lpSessionManager->GetCacheManager()->GetCell(&key, PR_MESSAGE_FLAGS, &sPropHasAttach, soap, false) == erSuccess) {
					sPropHasAttach.Value.ul &= ~MSGFLAG_HASATTACH;
					sPropHasAttach.Value.ul |= fHasAttach ? MSGFLAG_HASATTACH : 0;
					g_lpSessionManager->GetCacheManager()->SetCell(&key, PR_MESSAGE_FLAGS, &sPropHasAttach);
				}

				// More cache
				if (g_lpSessionManager->GetCacheManager()->GetObject(lpsReturnObj->ulServerId, &ulParentTmp, &ulOwnerTmp, &ulFlagsTmp, &ulTypeTmp) == erSuccess) {
					ulFlags &= ~MSGFLAG_HASATTACH;
					ulFlags |= fHasAttach ? MSGFLAG_HASATTACH : 0;
					g_lpSessionManager->GetCacheManager()->SetObject(lpsReturnObj->ulServerId, ulParentTmp, ulOwnerTmp, ulFlagsTmp, ulTypeTmp);
				}
			}
		}
		// 1. calc size of object, now that all children are saved.

		if (lpsReturnObj->ulObjType == MAPI_MESSAGE || lpsReturnObj->ulObjType == MAPI_ATTACH) {
			// Remove old size
			if (fNewItem != true && lpsReturnObj->ulObjType == MAPI_MESSAGE && ulParentType == MAPI_FOLDER) {
				if (GetObjectSize(lpDatabase, lpsReturnObj->ulServerId, &ulSize) == erSuccess)
					UpdateObjectSize(lpDatabase, ulStoreId, MAPI_STORE, UPDATE_SUB, ulSize);
			}

			// Add new size
			if (CalculateObjectSize(lpDatabase, lpsReturnObj->ulServerId, lpsReturnObj->ulObjType, &ulSize) == erSuccess) {
				UpdateObjectSize(lpDatabase, lpsReturnObj->ulServerId, lpsReturnObj->ulObjType, UPDATE_SET, ulSize);

				if (lpsReturnObj->ulObjType == MAPI_MESSAGE && ulParentType == MAPI_FOLDER) {
					UpdateObjectSize(lpDatabase, ulStoreId, MAPI_STORE, UPDATE_ADD, ulSize);
				}

			} else {
				ASSERT(FALSE);
			}
		}

		// 2. find props to return

		// the server returns the following 4 properties, when the item is new:
		//   PR_CREATION_TIME, (PR_PARENT_SOURCE_KEY, PR_SOURCE_KEY /type==5|3) (PR_RECORD_KEY /type==7|5|3)
		// TODO: recipients: PR_INSTANCE_KEY
		// it always sends the following property:
		//   PR_LAST_MODIFICATION_TIME
		// currently, it always sends them all
		// we also can't send the PR_MESSAGE_SIZE and PR_MESSAGE_FLAGS, since the recursion is the wrong way around: attachments come later than the actual message
		// we can skip PR_ACCESS and PR_ACCESS_LEVEL because the client already inherited those from the parent
		// we need to alloc 2 properties for PR_CHANGE_KEY and PR_PREDECESSOR_CHANGE_LIST
		lpsReturnObj->delProps.__size = 8;
		lpsReturnObj->delProps.__ptr = s_alloc<unsigned int>(soap, lpsReturnObj->delProps.__size);
		lpsReturnObj->modProps.__size = 8;
		lpsReturnObj->modProps.__ptr = s_alloc<struct propVal>(soap, lpsReturnObj->modProps.__size);

		n = 0;

		// set the PR_RECORD_KEY
		// New clients generate the instance key, old clients don't. See if one was provided.
		if (lpsSaveObj->ulObjType == MAPI_ATTACH || lpsSaveObj->ulObjType == MAPI_MESSAGE || lpsSaveObj->ulObjType == MAPI_FOLDER) {
			bool bSkip = false;
			if (lpsSaveObj->ulObjType == MAPI_ATTACH) {
				for (int i = 0; !bSkip && i < lpsSaveObj->modProps.__size; ++i)
					bSkip = lpsSaveObj->modProps.__ptr[i].ulPropTag == PR_RECORD_KEY;
				// @todo if we don't have a pr_record_key for an attachment, generate a guid for it like the client does
			}
			if (!bSkip && ECGenProps::GetPropComputedUncached(soap, NULL, lpecSession, PR_RECORD_KEY, lpsSaveObj->ulServerId, 0, 0, ulParentObjId, lpsSaveObj->ulObjType, &lpsReturnObj->modProps.__ptr[n]) == erSuccess) {
				lpsReturnObj->delProps.__ptr[n] = PR_RECORD_KEY;
				n++;
			}
		}

		if (lpsSaveObj->ulObjType != MAPI_STORE) {
			er = g_lpSessionManager->GetCacheManager()->GetObject(ulParentObjId, NULL, NULL, NULL, &ulParentObjType);
			if(er != erSuccess)
				goto exit;
		}

		//PR_PARENT_SOURCE_KEY for folders and messages
		if (lpsSaveObj->ulObjType == MAPI_FOLDER || (lpsSaveObj->ulObjType == MAPI_MESSAGE && ulParentObjType == MAPI_FOLDER))
		{
			if(ECGenProps::GetPropComputedUncached(soap, NULL, lpecSession, PR_PARENT_SOURCE_KEY, ulObjId, 0, 0, ulParentObjId, lpsSaveObj->ulObjType, &lpsReturnObj->modProps.__ptr[n]) == erSuccess)
			{
				lpsReturnObj->delProps.__ptr[n] = PR_PARENT_SOURCE_KEY;
				n++;
			}

			if(ECGenProps::GetPropComputedUncached(soap, NULL, lpecSession, PR_SOURCE_KEY, ulObjId, 0, 0, ulParentObjId, lpsSaveObj->ulObjType, &lpsReturnObj->modProps.__ptr[n]) == erSuccess)
			{
				lpsReturnObj->delProps.__ptr[n] = PR_SOURCE_KEY;
				n++;
			}
		}

		// PR_LAST_MODIFICATION_TIME
		lpsReturnObj->delProps.__ptr[n] = PR_LAST_MODIFICATION_TIME;

		lpsReturnObj->modProps.__ptr[n].__union = SOAP_UNION_propValData_hilo;
		lpsReturnObj->modProps.__ptr[n].ulPropTag = PR_LAST_MODIFICATION_TIME;
		lpsReturnObj->modProps.__ptr[n].Value.hilo = s_alloc<hiloLong>(soap);

		lpsReturnObj->modProps.__ptr[n].Value.hilo->hi = ftModified.dwHighDateTime;
		lpsReturnObj->modProps.__ptr[n].Value.hilo->lo = ftModified.dwLowDateTime;

		n++;

		if (fNewItem)
		{
			lpsReturnObj->delProps.__ptr[n] = PR_CREATION_TIME;

			lpsReturnObj->modProps.__ptr[n].__union = SOAP_UNION_propValData_hilo;
			lpsReturnObj->modProps.__ptr[n].ulPropTag = PR_CREATION_TIME;
			lpsReturnObj->modProps.__ptr[n].Value.hilo = s_alloc<hiloLong>(soap);

			lpsReturnObj->modProps.__ptr[n].Value.hilo->hi = ftCreated.dwHighDateTime;
			lpsReturnObj->modProps.__ptr[n].Value.hilo->lo = ftCreated.dwLowDateTime;

			n++;
		}


		// set actual array size
		lpsReturnObj->delProps.__size = n;
		lpsReturnObj->modProps.__size = n;
	}

exit:
    FREE_DBRESULT();

	FreeDeletedItems(&lstDeleteItems);

	return er;
}

SOAP_ENTRY_START(saveObject, lpsLoadObjectResponse->er, entryId sParentEntryId, entryId sEntryId, struct saveObject *lpsSaveObj, unsigned int ulFlags, unsigned int ulSyncId, struct loadObjectResponse *lpsLoadObjectResponse)
{
	USE_DATABASE();
	unsigned int	ulStoreId = 0;
	unsigned int	ulGrandParent = 0;
	unsigned int	ulGrandParentType = 0;
	unsigned int	ulParentObjId = 0;
	unsigned int	ulParentObjType = 0;
	unsigned int	ulObjType = lpsSaveObj->ulObjType;
	unsigned int	ulObjFlags = 0;
	unsigned int	ulPrevReadState = 0;
	unsigned int	ulNewReadState = 0;
	SOURCEKEY		sSourceKey;
	SOURCEKEY		sParentSourceKey;
	struct saveObject sReturnObject;
	std::string		strChangeKey;
	std::string		strChangeList;

	BOOL			fNewItem = false;
	bool			fHaveChangeKey = false;
	unsigned int	ulObjId = 0;
	struct propVal	*pvCommitTime = NULL;

	ECAttachmentStorage *lpAttachmentStorage = NULL;

	er = CreateAttachmentStorage(lpDatabase, &lpAttachmentStorage);
	if (er != erSuccess)
		goto exit;

	er = lpAttachmentStorage->Begin();
	if (er != erSuccess)
		goto exit;

	
	er = lpDatabase->Begin();
	if (er != erSuccess)
		goto exit;
		
	if (!sParentEntryId.__ptr) {
		// saveObject is called on the store itself (doesn't have a parent)
		ulParentObjType = MAPI_STORE;
		ulStoreId = lpsSaveObj->ulServerId;
	} else {
		if (lpsSaveObj->ulServerId == 0) {
			// new object, parent entry id given by client
			er = lpecSession->GetObjectFromEntryId(&sParentEntryId, &ulParentObjId);
			if (er != erSuccess)
				goto exit;

			fNewItem = true;

			// Lock folder counters now
            strQuery = "SELECT val_ulong FROM properties WHERE hierarchyid = " + stringify(ulParentObjId) + " FOR UPDATE";
            er = lpDatabase->DoSelect(strQuery, NULL);
			if (er != erSuccess)
			    goto exit;
		} else {
			// existing item, search parent ourselves cause the client just sent it's store entryid (see ECMsgStore::OpenEntry())
			er = g_lpSessionManager->GetCacheManager()->GetObject(lpsSaveObj->ulServerId, &ulParentObjId, NULL, &ulObjFlags, &ulObjType);
			if (er != erSuccess)
				goto exit;
				
            if (ulObjFlags & MSGFLAG_DELETED) {
                er = ZARAFA_E_OBJECT_DELETED;
                goto exit;
            }

			fNewItem = false;
			
			// Lock folder counters now
            strQuery = "SELECT val_ulong FROM properties WHERE hierarchyid = " + stringify(ulParentObjId) + " FOR UPDATE";
            er = lpDatabase->DoSelect(strQuery, NULL);
			if (er != erSuccess)
			    goto exit;
            
            // We also need the old read flags so we can compare the new read flags to see if we need to update the unread counter. Note
            // that the read flags can only be modified through saveObject() when using ICS.
            
            strQuery = "SELECT val_ulong FROM properties WHERE hierarchyid = " + stringify(lpsSaveObj->ulServerId) + " AND tag = " + stringify(PROP_ID(PR_MESSAGE_FLAGS)) + " AND type = " + stringify(PROP_TYPE(PR_MESSAGE_FLAGS));
            er = lpDatabase->DoSelect(strQuery, &lpDBResult);
            if (er != erSuccess)
                goto exit;
                
            lpDBRow = lpDatabase->FetchRow(lpDBResult);
            
            if(!lpDBRow || !lpDBRow[0]) {
                ulPrevReadState = 0;
            } else {
                ulPrevReadState = atoui(lpDBRow[0]) & MSGFLAG_READ;
            }
            
            ulNewReadState = ulPrevReadState; // Copy read state, may be updated by SaveObject() later
                
            FREE_DBRESULT();

		}

		if (ulParentObjId != CACHE_NO_PARENT) {
			er = g_lpSessionManager->GetCacheManager()->GetObject(ulParentObjId, NULL, NULL, NULL, &ulParentObjType);
			if(er != erSuccess)
				goto exit;

			er = lpecSession->GetSessionManager()->GetCacheManager()->GetStore(ulParentObjId, &ulStoreId, NULL);
			if(er != erSuccess)
				goto exit;
		} else {
			// we get here on create store, but I'm not exactly sure why :|
			ulParentObjType = MAPI_STORE;
			ulStoreId = lpsSaveObj->ulServerId;
		}
	}

	if (ulParentObjId != 0 && ulParentObjType != MAPI_STORE) {
		er = g_lpSessionManager->GetCacheManager()->GetObject(ulParentObjId, &ulGrandParent, NULL, NULL, &ulGrandParentType);
		if(er != erSuccess)
			goto exit;
	}

	// Check permissions
	if(!fNewItem && ulObjType == MAPI_FOLDER) {
		er = lpecSession->GetSecurity()->CheckPermission(lpsSaveObj->ulServerId, ecSecurityFolderAccess);
	}else if(fNewItem) {
		er = lpecSession->GetSecurity()->CheckPermission(ulParentObjId, ecSecurityCreate);
	} else {
		er = lpecSession->GetSecurity()->CheckPermission(lpsSaveObj->ulServerId, ecSecurityEdit);
	}
	if(er != erSuccess)
		goto exit;
	
	/////////////////////////////////////////////////
	// Quota check
	if(ulObjType == MAPI_MESSAGE) {
		er = CheckQuota(lpecSession, ulStoreId);
		if(er != erSuccess)
		    goto exit;
	
		// Update folder counts
		if(fNewItem) {
			er = UpdateFolderCounts(lpDatabase, ulParentObjId, ulFlags, &lpsSaveObj->modProps);
			if (er != erSuccess)
				goto exit;
		}
		else if(ulSyncId != 0) {
			// On modified appointments, unread flags may have changed (only possible during ICS import)
			for(int i=0; i < lpsSaveObj->modProps.__size; i++) {
				if(lpsSaveObj->modProps.__ptr[i].ulPropTag == PR_MESSAGE_FLAGS) {
					ulNewReadState = lpsSaveObj->modProps.__ptr[i].Value.ul & MSGFLAG_READ;
					break;
				}
			}

			if (ulPrevReadState != ulNewReadState) {
				er = UpdateFolderCount(lpDatabase, ulParentObjId, PR_CONTENT_UNREAD, ulNewReadState == MSGFLAG_READ ? -1 : 1);
				if (er != erSuccess)
					goto exit;
			}
		}
	}

	er = SaveObject(soap, lpecSession, lpDatabase, lpAttachmentStorage, ulStoreId, ulParentObjId, ulParentObjType, ulFlags, ulSyncId, lpsSaveObj, &sReturnObject, atoui(g_lpSessionManager->GetConfig()->GetSetting("embedded_attachment_limit")), &fHaveChangeKey);
	if (er != erSuccess)
		goto exit;

	// update PR_LOCAL_COMMIT_TIME_MAX for disconnected clients who want to know if the folder contents changed
	if (ulObjType == MAPI_MESSAGE && ulParentObjType == MAPI_FOLDER) {
		er = WriteLocalCommitTimeMax(soap, lpDatabase, ulParentObjId, &pvCommitTime);
		if(er != erSuccess)
			goto exit;
	}

	if (lpsSaveObj->ulServerId == 0) {
		er = MapEntryIdToObjectId(lpecSession, lpDatabase, sReturnObject.ulServerId, sEntryId);
		if (er != erSuccess)
			goto exit;

		ulObjId = sReturnObject.ulServerId;

		// now that we have an entry id, find the generated PR_RECORD_KEY from SaveObject and override it with the PR_ENTRYID value (fixme, ZCP-6706)
		{
			int rki;
			for (rki = 0; rki < sReturnObject.modProps.__size; rki++)
				if (sReturnObject.modProps.__ptr[rki].ulPropTag == PR_RECORD_KEY)
					break;
			// @note static alloc of 8 props in SaveObject. we did not find the record key: make it now
			if (rki == sReturnObject.modProps.__size && rki < 8) {
				ECGenProps::GetPropComputedUncached(soap, NULL, lpecSession, PR_RECORD_KEY, sReturnObject.ulServerId, 0, 0, ulParentObjId,
													lpsSaveObj->ulObjType, &sReturnObject.modProps.__ptr[rki]);
				sReturnObject.modProps.__size++;
				sReturnObject.delProps.__ptr[rki] = PR_RECORD_KEY;
				sReturnObject.delProps.__size++;
			}
		}
	} else {
		ulObjId = lpsSaveObj->ulServerId;
	}

	// 3. pr_source_key magic
	if ((sReturnObject.ulObjType == MAPI_MESSAGE && ulParentObjType == MAPI_FOLDER) ||
		(sReturnObject.ulObjType == MAPI_FOLDER && !(ulFlags & FOLDER_SEARCH)))
	{
		GetSourceKey(sReturnObject.ulServerId, &sSourceKey);
		GetSourceKey(ulParentObjId, &sParentSourceKey);

		if (sReturnObject.ulObjType == MAPI_MESSAGE && ulParentObjType == MAPI_FOLDER) {
			if (lpsSaveObj->ulServerId == 0) {
				AddChange(lpecSession, ulSyncId, sSourceKey, sParentSourceKey, ICS_MESSAGE_NEW, 0, !fHaveChangeKey, &strChangeKey, &strChangeList);
			} else {
				AddChange(lpecSession, ulSyncId, sSourceKey, sParentSourceKey, ICS_MESSAGE_CHANGE, 0, !fHaveChangeKey, &strChangeKey, &strChangeList);
			}
		} else if (lpsSaveObj->ulObjType == MAPI_FOLDER && !(ulFlags & FOLDER_SEARCH)) {
			AddChange(lpecSession, ulSyncId, sSourceKey, sParentSourceKey, ICS_FOLDER_CHANGE, 0, !fHaveChangeKey, &strChangeKey, &strChangeList);
		}

		if(!strChangeKey.empty()){
			sReturnObject.delProps.__ptr[sReturnObject.delProps.__size] = PR_CHANGE_KEY;
			sReturnObject.delProps.__size++;
			sReturnObject.modProps.__ptr[sReturnObject.modProps.__size].ulPropTag = PR_CHANGE_KEY;
			sReturnObject.modProps.__ptr[sReturnObject.modProps.__size].__union = SOAP_UNION_propValData_bin;
			sReturnObject.modProps.__ptr[sReturnObject.modProps.__size].Value.bin = s_alloc<struct xsd__base64Binary>(soap);
			sReturnObject.modProps.__ptr[sReturnObject.modProps.__size].Value.bin->__size = strChangeKey.size();
			sReturnObject.modProps.__ptr[sReturnObject.modProps.__size].Value.bin->__ptr = s_alloc<unsigned char>(soap, strChangeKey.size());
			memcpy(sReturnObject.modProps.__ptr[sReturnObject.modProps.__size].Value.bin->__ptr, strChangeKey.c_str(), strChangeKey.size());
			sReturnObject.modProps.__size++;
		}

		if(!strChangeList.empty()){
			sReturnObject.delProps.__ptr[sReturnObject.delProps.__size] = PR_PREDECESSOR_CHANGE_LIST;
			sReturnObject.delProps.__size++;
			sReturnObject.modProps.__ptr[sReturnObject.modProps.__size].ulPropTag = PR_PREDECESSOR_CHANGE_LIST;
			sReturnObject.modProps.__ptr[sReturnObject.modProps.__size].__union = SOAP_UNION_propValData_bin;
			sReturnObject.modProps.__ptr[sReturnObject.modProps.__size].Value.bin = s_alloc<struct xsd__base64Binary>(soap);
			sReturnObject.modProps.__ptr[sReturnObject.modProps.__size].Value.bin->__size = strChangeList.size();
			sReturnObject.modProps.__ptr[sReturnObject.modProps.__size].Value.bin->__ptr = s_alloc<unsigned char>(soap, strChangeList.size());
			memcpy(sReturnObject.modProps.__ptr[sReturnObject.modProps.__size].Value.bin->__ptr, strChangeList.c_str(), strChangeList.size());
			sReturnObject.modProps.__size++;
		}
	}

	// 5. TODO: ulSyncId updates sync tables

	// 6. process MSGFLAG_SUBMIT if needed
	er = ProcessSubmitFlag(lpDatabase, ulSyncId, ulStoreId, ulObjId, fNewItem, &lpsSaveObj->modProps);
	if (er != erSuccess)
		goto exit;

	if (ulParentObjType == MAPI_FOLDER) {
		er = ECTPropsPurge::NormalizeDeferredUpdates(lpecSession, lpDatabase, ulParentObjId);
		if (er != erSuccess)
			goto exit;
	}

	er = lpAttachmentStorage->Commit();
	if (er != erSuccess)
		goto exit;

	er = lpDatabase->Commit();
	if (er != erSuccess)
		goto exit;

	// 7. notification
	// Only Notify on MAPI_MESSAGE, MAPI_FOLDER and MAPI_STORE
	// but don't nofity if parent object is a store and object type is attachment or message
	CreateNotifications(ulObjId, ulObjType, ulParentObjId, ulGrandParent, fNewItem, &lpsSaveObj->modProps, pvCommitTime);

	lpsLoadObjectResponse->sSaveObject = sReturnObject;

	g_lpStatsCollector->Increment(SCN_DATABASE_MWOPS);
	
exit:
	if (er != erSuccess && lpAttachmentStorage)
		lpAttachmentStorage->Rollback();

	if (lpAttachmentStorage)
		lpAttachmentStorage->Release();

	ROLLBACK_ON_ERROR();
	FREE_DBRESULT();
}
SOAP_ENTRY_END()

ECRESULT LoadObject(struct soap *soap, ECSession *lpecSession, unsigned int ulObjId, unsigned int ulObjType, unsigned int ulParentObjType, struct saveObject *lpsSaveObj, std::map<unsigned int, CHILDPROPS> *lpChildProps)
{
	ECRESULT 		er = erSuccess;
	ECAttachmentStorage *lpAttachmentStorage = NULL;
	ULONG			ulInstanceId = 0;
	ULONG			ulInstanceTag = 0;
	struct saveObject sSavedObject;
	int i;
	GUID			sGuidServer;
	ChildPropsMap mapChildProps;
	ChildPropsMap::iterator iterProps;
	USE_DATABASE();
	CHILDPROPS		sEmptyProps;
	
	sEmptyProps.lpPropVals = new DynamicPropValArray(soap);
	sEmptyProps.lpPropTags = new DynamicPropTagArray(soap);

	memset(&sSavedObject, 0, sizeof(saveObject));

	// Check permission
	if (ulObjType == MAPI_STORE || (ulObjType == MAPI_FOLDER && ulParentObjType == MAPI_STORE))
		// Always read rights on the store and the root folder
		er = erSuccess;
	else if (ulObjType == MAPI_FOLDER)
		er = lpecSession->GetSecurity()->CheckPermission(ulObjId, ecSecurityFolderVisible);
	else if (ulObjType == MAPI_MESSAGE)
		er = lpecSession->GetSecurity()->CheckPermission(ulObjId, ecSecurityRead);
		
    // Allow reading MAPI_MAILUSER and MAPI_ATTACH since that is only called internally

	if (er != erSuccess)
		goto exit;

	sSavedObject.ulClientId = 0;
	sSavedObject.ulServerId = ulObjId;
	sSavedObject.ulObjType = ulObjType;
	
	if(lpChildProps == NULL) {
	    // We were not provided with a property list for this object, get our own now.
	    er = PrepareReadProps(soap, lpDatabase, true, lpecSession->GetCapabilities() & ZARAFA_CAP_UNICODE, ulObjId, 0, MAX_PROP_SIZE, &mapChildProps, NULL);
	    if(er != erSuccess)
	        goto exit;
	        
        lpChildProps = &mapChildProps;
    }
	
	iterProps = lpChildProps->find(ulObjId);
	if(iterProps == lpChildProps->end())
    	er = ReadProps(soap, lpecSession, ulObjId, ulObjType, ulParentObjType, sEmptyProps, &sSavedObject.delProps, &sSavedObject.modProps);
    else
    	er = ReadProps(soap, lpecSession, ulObjId, ulObjType, ulParentObjType, iterProps->second, &sSavedObject.delProps, &sSavedObject.modProps);

	if (er != erSuccess)
		goto exit;
		
    FreeChildProps(&mapChildProps);
    

	if (ulObjType == MAPI_MESSAGE || ulObjType == MAPI_ATTACH) {
        // Pre-load *all* properties of *all* subobjects for fast accessibility
        er = PrepareReadProps(soap, lpDatabase, true, lpecSession->GetCapabilities() & ZARAFA_CAP_UNICODE, 0, ulObjId, MAX_PROP_SIZE, &mapChildProps, NULL);
        if (er != erSuccess)
            goto exit;

		// find subobjects
		strQuery = "SELECT id, type FROM hierarchy WHERE parent="+stringify(ulObjId);
		er = lpDatabase->DoSelect(strQuery, &lpDBResult);
		if(er != erSuccess)
			goto exit;

		sSavedObject.__size = lpDatabase->GetNumRows(lpDBResult);
		sSavedObject.__ptr = s_alloc<saveObject>(soap, sSavedObject.__size);
		memset(sSavedObject.__ptr, 0, sizeof(saveObject) * sSavedObject.__size);

		for (i = 0; i < sSavedObject.__size; i++) {
		    unsigned int ulChildId;
			lpDBRow = lpDatabase->FetchRow(lpDBResult);
			lpDBLen = lpDatabase->FetchRowLengths(lpDBResult);

			if(lpDBRow == NULL || lpDBLen == NULL) {
				er = ZARAFA_E_DATABASE_ERROR; // this should never happen
				goto exit;
			}
			
			ulChildId = atoi(lpDBRow[0]);
			
   			LoadObject(soap, lpecSession, atoi(lpDBRow[0]), atoi(lpDBRow[1]), ulObjType, &sSavedObject.__ptr[i], &mapChildProps);
		}
    	FreeChildProps(&mapChildProps);
	}

	if (ulObjType == MAPI_MESSAGE || ulObjType == MAPI_ATTACH) {
		er = CreateAttachmentStorage(lpDatabase, &lpAttachmentStorage);
		if (er != erSuccess)
			goto exit;

		er = g_lpSessionManager->GetServerGUID(&sGuidServer);
		if (er != erSuccess)
			goto exit;

		/* Not having a single instance ID is not critical, we might create it at a later time */
		// @todo this should be GetSingleInstanceIdsWithTags(ulObjId, map<tag, instanceid>);
		if (ulObjType == MAPI_MESSAGE)
			ulInstanceTag = PROP_ID(PR_EC_IMAP_EMAIL);
		else
			ulInstanceTag = PROP_ID(PR_ATTACH_DATA_BIN);
		if (lpAttachmentStorage->GetSingleInstanceId(ulObjId, ulInstanceTag, &ulInstanceId) == erSuccess) {
			sSavedObject.lpInstanceIds = s_alloc<entryList>(soap);
			sSavedObject.lpInstanceIds->__size = 1;
			sSavedObject.lpInstanceIds->__ptr = s_alloc<entryId>(soap, sSavedObject.lpInstanceIds->__size);

			er = SIIDToEntryID(soap, &sGuidServer, ulInstanceId, ulInstanceTag, &sSavedObject.lpInstanceIds->__ptr[0]);
			if (er != erSuccess) {
				sSavedObject.lpInstanceIds->__size = 0;
				goto exit;
			}
		}
	}

	if (ulObjType == MAPI_MESSAGE) {
		// @todo: Check if we can do this on the fly to avoid the additional lookup.
		for (int i = 0; i < sSavedObject.modProps.__size; ++i) {
			if (sSavedObject.modProps.__ptr[i].ulPropTag == PR_SUBMIT_FLAGS) {
				if (g_lpSessionManager->GetLockManager()->IsLocked(ulObjId, NULL))
					sSavedObject.modProps.__ptr[i].Value.ul |= SUBMITFLAG_LOCKED;
				else
					sSavedObject.modProps.__ptr[i].Value.ul &= ~SUBMITFLAG_LOCKED;
			}
		}
	}

	*lpsSaveObj = sSavedObject;

exit:
	if (lpAttachmentStorage)
		lpAttachmentStorage->Release();
		
    if (sEmptyProps.lpPropVals)
        delete sEmptyProps.lpPropVals;
    if (sEmptyProps.lpPropTags)
        delete sEmptyProps.lpPropTags;

	FREE_DBRESULT();
__soapentry_exit:
	return er;
}

SOAP_ENTRY_START(loadObject, lpsLoadObjectResponse->er, entryId sEntryId, struct notifySubscribe *lpsNotSubscribe, unsigned int ulFlags, struct loadObjectResponse *lpsLoadObjectResponse)
{
	unsigned int	ulObjId = 0;
	unsigned int	ulObjFlags = 0;
	unsigned int	ulObjType = 0;
	unsigned int	ulParentId = 0;
	unsigned int	ulParentObjType = 0;
	unsigned int	ulOwnerId = 0;
	unsigned int	ulEidFlags = 0;
	USE_DATABASE();

	struct saveObject sSavedObject;

	er = BeginLockFolders(lpDatabase, EntryId(sEntryId), LOCK_SHARED);
	if(er != erSuccess)
	    goto exit;
	
	/*
	 * 2 Reasons to send ZARAFA_E_UNABLE_TO_COMPLETE (and have the client try to open the store elsewhere):
	 *  1. We can't find the object based on the entryid.
	 *  2. The owner of the store is not supposed to have a store on this server.
	 */
	er = lpecSession->GetObjectFromEntryId(&sEntryId, &ulObjId, &ulEidFlags);
	if ((ulEidFlags & OPENSTORE_OVERRIDE_HOME_MDB) == 0) {
		if (er == ZARAFA_E_NOT_FOUND &&	sEntryId.__size >= (int)min(sizeof(EID), sizeof(EID_V0)) && ((EID*)sEntryId.__ptr)->usType == MAPI_STORE)
			er = ZARAFA_E_UNABLE_TO_COMPLETE;	// Reason 1
	}
	if (er != erSuccess)
		goto exit;
		
	er = g_lpSessionManager->GetCacheManager()->GetObject(ulObjId, &ulParentId, &ulOwnerId, &ulObjFlags, &ulObjType);
    if (er != erSuccess)
        goto exit;
    
	if(ulObjType == MAPI_STORE) {
		if ((ulEidFlags & OPENSTORE_OVERRIDE_HOME_MDB) == 0) {
			if (lpecSession->GetSessionManager()->IsDistributedSupported() && !lpecSession->GetUserManagement()->IsInternalObject(ulOwnerId)) {
				objectdetails_t sUserDetails;

				if (lpecSession->GetUserManagement()->GetObjectDetails(ulOwnerId, &sUserDetails) == erSuccess) {
					unsigned int ulStoreType;
					er = lpecSession->GetSessionManager()->GetCacheManager()->GetStoreAndType(ulObjId, NULL, NULL, &ulStoreType);
					if (er != erSuccess)
						goto exit;

					if (ulStoreType == ECSTORE_TYPE_PRIVATE || ulStoreType == ECSTORE_TYPE_PUBLIC) {
						std::string strServerName = sUserDetails.GetPropString(OB_PROP_S_SERVERNAME);
						if (strServerName.empty()) {
							er = ZARAFA_E_NOT_FOUND;
							goto exit;
						}

						if (stricmp(strServerName.c_str(), g_lpSessionManager->GetConfig()->GetSetting("server_name")) != 0) {
							er = ZARAFA_E_UNABLE_TO_COMPLETE;	// Reason 2
							goto exit;
						}
					} else if (ulStoreType == ECSTORE_TYPE_ARCHIVE) {
						// We allow an archive store to be opened by sysadmins even if it's not supposed
						// to exist on this server for a particular user.
						if (lpecSession->GetSecurity()->GetAdminLevel() < ADMIN_LEVEL_SYSADMIN &&
						   !sUserDetails.PropListStringContains((property_key_t)PR_EC_ARCHIVE_SERVERS_A, g_lpSessionManager->GetConfig()->GetSetting("server_name"), true))
						{
							er = ZARAFA_E_NOT_FOUND;
							goto exit;
						}
					} else {
						er = ZARAFA_E_NOT_FOUND;
						goto exit;
					}
				} else {
					// unhooked store of a deleted user
					if(lpecSession->GetSecurity()->GetAdminLevel() < ADMIN_LEVEL_SYSADMIN) {
						er = ZARAFA_E_NO_ACCESS;
						goto exit;
					}
				}
			}
		}
        ulParentObjType = 0;
	} else if(ulObjType == MAPI_MESSAGE) {
		// If the object is locked on another session, access should be denied
		ECSESSIONID ulLockedSessionId;

		if (g_lpSessionManager->GetLockManager()->IsLocked(ulObjId, &ulLockedSessionId) && ulLockedSessionId != ulSessionId) {
			er = ZARAFA_E_NO_ACCESS;
			goto exit;
		}
		
		ulParentObjType = MAPI_FOLDER;
	} else if(ulObjType == MAPI_FOLDER) {


		// Disable searchfolder for delegate stores without admin permissions
		if ( (ulObjFlags&FOLDER_SEARCH) && lpecSession->GetSecurity()->GetAdminLevel() == 0 && 
			lpecSession->GetSecurity()->IsStoreOwner(ulObjId) != erSuccess)
		{
			er = ZARAFA_E_NOT_FOUND;
			goto exit;
		}

        er = g_lpSessionManager->GetCacheManager()->GetObject(ulParentId, NULL, NULL, NULL, &ulParentObjType);
		if (er != erSuccess)
			goto exit;
			
        // Reset folder counts now (Note: runs in a DB transaction!). Note: we only update counts when lpNotSubscribe is not NULL; this
        // makes sure that we only reset folder counts on the first open of a folder, and not when the folder properties are updated (eg
        // due to counter changes)
        if(lpsNotSubscribe && ulObjFlags != FOLDER_SEARCH && parseBool(g_lpSessionManager->GetConfig()->GetSetting("counter_reset")))
            ResetFolderCount(lpecSession, ulObjId);
    }

	// check if flags were passed, older clients call checkExistObject
    if(ulFlags & 0x80000000) {
        // Flags passed by client, check object flags
        ulFlags = ulFlags & ~0x80000000;
        
        if((ulObjFlags & MSGFLAG_DELETED) != ulFlags) {
        	if (ulObjType == MAPI_STORE)
        		er = ZARAFA_E_UNABLE_TO_COMPLETE;
        	else
            	er = ZARAFA_E_NOT_FOUND;
            goto exit;
        }
    }

	// Subscribe for notification
	if (lpsNotSubscribe) {
		er = DoNotifySubscribe(lpecSession, ulSessionId, lpsNotSubscribe);
		if (er != erSuccess)
			goto exit;
	}

	er = LoadObject(soap, lpecSession, ulObjId, ulObjType, ulParentObjType, &sSavedObject, NULL);
	if (er != erSuccess)
		goto exit;

	lpsLoadObjectResponse->sSaveObject = sSavedObject;

	g_lpStatsCollector->Increment(SCN_DATABASE_MROPS);

exit:
    lpDatabase->Commit();
}
SOAP_ENTRY_END()

// if lpsNewEntryId is NULL this function create a new entryid
// if lpsOrigSourceKey is NULL this function creates a new sourcekey
ECRESULT CreateFolder(ECSession *lpecSession, ECDatabase *lpDatabase, unsigned int ulParentId, entryId *lpsNewEntryId, unsigned int type, char *name, char *comment, bool openifexists, bool bNotify, unsigned int ulSyncId, struct xsd__base64Binary *lpsOrigSourceKey, unsigned int *lpFolderId, bool *lpbExist)
{
	ECRESULT		er = erSuccess;
	ALLOC_DBRESULT();

	unsigned int	ulFolderId = 0;
	unsigned int	ulLastId = 0;
	unsigned int	ulStoreId = 0;
	unsigned int	ulGrandParent = 0;
	bool			bExist = false;
	bool			bFreeNewEntryId = false;
	GUID			guid;
	SOURCEKEY		sSourceKey;
	unsigned int	tags [] = { PR_CONTENT_COUNT, PR_CONTENT_UNREAD, PR_ASSOC_CONTENT_COUNT, PR_DELETED_MSG_COUNT, PR_DELETED_FOLDER_COUNT, PR_DELETED_ASSOC_MSG_COUNT, PR_FOLDER_CHILD_COUNT };
	unsigned int	timeTags [] = { PR_LAST_MODIFICATION_TIME, PR_CREATION_TIME };
	time_t			now = 0;
	struct propVal  sProp;
    struct hiloLong sHilo;

	// You're only allowed to create search folders in your own store
	if(type == FOLDER_SEARCH) {
	    if(lpecSession->GetSecurity()->IsStoreOwner(ulParentId) != erSuccess && (lpecSession->GetSecurity()->IsAdminOverOwnerOfObject(ulParentId) != erSuccess)) {
	        er = ZARAFA_E_NO_ACCESS;
	        goto exit;
        }
    }

	er = lpecSession->GetSessionManager()->GetCacheManager()->GetStore(ulParentId, &ulStoreId, &guid);
	if(er != erSuccess)
	    goto exit;

	er = g_lpSessionManager->GetCacheManager()->GetParent(ulParentId, &ulGrandParent);
	if(er != erSuccess)
		goto exit;

	// Check whether the requested name already exists

	strQuery = "SELECT hierarchy.id, properties.val_string FROM hierarchy JOIN properties ON hierarchy.id = properties.hierarchyid WHERE hierarchy.parent=" + stringify(ulParentId) + " AND hierarchy.type="+stringify(MAPI_FOLDER)+" AND hierarchy.flags & " + stringify(MSGFLAG_DELETED)+ "=0 AND properties.tag=" + stringify(ZARAFA_TAG_DISPLAY_NAME) + " AND properties.val_string = '" + lpDatabase->Escape(name) + "' AND properties.type="+stringify(PT_STRING8);
	er = lpDatabase->DoSelect(strQuery, &lpDBResult);
	if(er != erSuccess)
		goto exit;


	while (!bExist && (lpDBRow = lpDatabase->FetchRow(lpDBResult)) != NULL) {
		if (lpDBRow[0] == NULL || lpDBRow[1] == NULL) {
			er = ZARAFA_E_DATABASE_ERROR;
			goto exit;
		}

		if (stricmp(lpDBRow[1], name) == 0)
			bExist = true;
	}


	if(bExist && !ulSyncId) {
		// Check folder read access
		er = lpecSession->GetSecurity()->CheckPermission(ulParentId, ecSecurityFolderVisible);
		if(er != erSuccess)
			goto exit;

		// Object exists
		if (!openifexists) {
			er = ZARAFA_E_COLLISION;
			goto exit;
		}
		
		ulFolderId = atoi(lpDBRow[0]);

	} else {
		// Check write permission of the folder destination
		er = lpecSession->GetSecurity()->CheckPermission(ulParentId, ecSecurityCreateFolder);
		if(er != erSuccess)
			goto exit;

		// Create folder
		strQuery = "INSERT INTO hierarchy (parent, type, flags, owner) values(" + stringify(ulParentId) + "," + stringify(ZARAFA_OBJTYPE_FOLDER) + ", " + stringify(type) + ", "+stringify(lpecSession->GetSecurity()->GetUserId(ulParentId))+")";
		er = lpDatabase->DoInsert(strQuery, &ulLastId);
		if(er != erSuccess)
			goto exit;

		if(lpsNewEntryId == NULL) {
			er = CreateEntryId(guid, MAPI_FOLDER, &lpsNewEntryId);
			if(er != erSuccess)
				goto exit;

			bFreeNewEntryId = true;
		}

		//Create entryid, 0x0FFF = PR_ENTRYID
		er = RemoveStaleIndexedProp(lpDatabase, PR_ENTRYID, lpsNewEntryId->__ptr, lpsNewEntryId->__size);
		if(er != erSuccess)
		    goto exit;
		strQuery = "INSERT INTO indexedproperties (hierarchyid,tag,val_binary) VALUES("+stringify(ulLastId)+", 0x0FFF, "+lpDatabase->EscapeBinary(lpsNewEntryId->__ptr, lpsNewEntryId->__size)+")";
		er = lpDatabase->DoInsert(strQuery);
		if(er != erSuccess)
			goto exit;

		// Create Displayname
		strQuery = "INSERT INTO properties (hierarchyid, tag, type, val_string) values(" + stringify(ulLastId) + "," + stringify(ZARAFA_TAG_DISPLAY_NAME) + "," + stringify(PT_STRING8) + ",'" + lpDatabase->Escape(name) + "')";
		er = lpDatabase->DoInsert(strQuery);
		if(er != erSuccess)
			goto exit;

		// Create Displayname
		strQuery = "INSERT INTO tproperties (hierarchyid, tag, type, folderid, val_string) values(" + stringify(ulLastId) + "," + stringify(ZARAFA_TAG_DISPLAY_NAME) + "," + stringify(PT_STRING8) + "," + stringify(ulParentId) + ",'" + lpDatabase->Escape(name) + "')";
		er = lpDatabase->DoInsert(strQuery);
		if(er != erSuccess)
			goto exit;
			
		// Create counters
		for(unsigned int i = 0; i < sizeof(tags)/sizeof(tags[0]); i++) {
			sProp.ulPropTag = tags[i];
			sProp.__union = SOAP_UNION_propValData_ul;
			sProp.Value.ul = 0;
			
			er = WriteProp(lpDatabase, ulLastId, ulParentId, &sProp);
			if(er != erSuccess)
				goto exit;
		}
		
		// Create PR_SUBFOLDERS
		sProp.ulPropTag = PR_SUBFOLDERS;
		sProp.__union = SOAP_UNION_propValData_b;
		sProp.Value.b = false;
		
		er = WriteProp(lpDatabase, ulLastId, ulParentId, &sProp);
		if(er != erSuccess)
			goto exit;
			
		// Create PR_FOLDERTYPE
		sProp.ulPropTag = PR_FOLDER_TYPE;
		sProp.__union = SOAP_UNION_propValData_ul;
		sProp.Value.ul = type;

		er = WriteProp(lpDatabase, ulLastId, ulParentId, &sProp);
		if(er != erSuccess)
			goto exit;

        // Create PR_COMMENT			
		if (comment) {
		    sProp.ulPropTag = PR_COMMENT_A;
		    sProp.__union = SOAP_UNION_propValData_lpszA;
		    sProp.Value.lpszA = comment;
		    
		    er = WriteProp(lpDatabase, ulLastId, ulParentId, &sProp);
			if(er != erSuccess)
				goto exit;
		}
		
		// Create PR_LAST_MODIFICATION_TIME and PR_CREATION_TIME
		now = time(NULL);
		for(unsigned int i=0; i < sizeof(timeTags)/sizeof(timeTags[0]); i++) {
		    sProp.ulPropTag = timeTags[i];
		    sProp.__union = SOAP_UNION_propValData_hilo;
		    sProp.Value.hilo = &sHilo;
		    UnixTimeToFileTime(now, &sProp.Value.hilo->hi, &sProp.Value.hilo->lo);
		    
		    er = WriteProp(lpDatabase, ulLastId, ulParentId, &sProp);
			if(er != erSuccess)
				goto exit;
        }

		// Create SourceKey
		if (lpsOrigSourceKey && lpsOrigSourceKey->__size > (int)sizeof(GUID) && lpsOrigSourceKey->__ptr){
			sSourceKey = SOURCEKEY(lpsOrigSourceKey->__size, (char*)lpsOrigSourceKey->__ptr);
		}else{
			er = lpecSession->GetNewSourceKey(&sSourceKey);
			if(er != erSuccess)
				goto exit;
		}

		er = RemoveStaleIndexedProp(lpDatabase, PR_SOURCE_KEY, sSourceKey, sSourceKey.size());
		if(er != erSuccess)
		    goto exit;
		    
		strQuery = "INSERT INTO indexedproperties(hierarchyid,tag,val_binary) VALUES(" + stringify(ulLastId) + "," + stringify(PROP_ID(PR_SOURCE_KEY)) + "," + lpDatabase->EscapeBinary(sSourceKey, sSourceKey.size()) + ")";
		er = lpDatabase->DoInsert(strQuery);
		if(er != erSuccess)
			goto exit;

		ulFolderId = ulLastId;
		
		er = UpdateFolderCount(lpDatabase, ulParentId, PR_SUBFOLDERS, 1);
		if(er != erSuccess)
			goto exit;
		er = UpdateFolderCount(lpDatabase, ulParentId, PR_FOLDER_CHILD_COUNT, 1);
		if(er != erSuccess)
			goto exit;
	}

	if(bExist == false && !(type & FOLDER_SEARCH)){
		SOURCEKEY sParentSourceKey;
		
		GetSourceKey(ulParentId, &sParentSourceKey);
		AddChange(lpecSession, ulSyncId, sSourceKey, sParentSourceKey, ICS_FOLDER_NEW);
	}

	// Notify that the folder has been created
	if(bExist == false && bNotify == true) {
		g_lpSessionManager->GetCacheManager()->Update(fnevObjectModified, ulParentId);
		g_lpSessionManager->NotificationCreated(MAPI_FOLDER, ulFolderId, ulParentId);
		g_lpSessionManager->NotificationModified(MAPI_FOLDER, ulParentId);

		// Update all tables viewing this folder
		g_lpSessionManager->UpdateTables(ECKeyTable::TABLE_ROW_ADD, 0, ulParentId, ulFolderId, MAPI_FOLDER);


		// Update notification, grandparent of the mainfolder
		g_lpSessionManager->UpdateTables(ECKeyTable::TABLE_ROW_MODIFY, 0, ulGrandParent, ulParentId, MAPI_FOLDER);
	}

	if(lpFolderId)
		*lpFolderId = ulFolderId;

	if(lpbExist)
		*lpbExist = bExist;

exit:
	FREE_DBRESULT();

	if(bFreeNewEntryId == true && lpsNewEntryId)
		FreeEntryId(lpsNewEntryId, true);

	return er;
}

/**
 * createFolder: Create a folder object in the hierarchy table, and add a 'PR_DISPLAY_NAME' property.
 *
 * The data model actually supports multiple folders having the same PR_DISPLAY_NAME, however, MAPI does not,
 * so we have to give the engine the knowledge of the PR_DISPLAY_NAME property here, one of the few properties
 * that the backend engine actually knows about.
 *
 * Of course, the frontend could also enforce this constraint, but that would require 2 server accesses (1. check
 * existing, 2. create folder), and we're trying to keep the amount of server accesses as low as possible.
 *
 */

SOAP_ENTRY_START(createFolder, lpsResponse->er, entryId sParentId, entryId* lpsNewEntryId, unsigned int ulType, char *szName, char *szComment, bool fOpenIfExists, unsigned int ulSyncId, struct xsd__base64Binary sOrigSourceKey, struct createFolderResponse *lpsResponse)
{
	unsigned int	ulParentId = 0;
	unsigned int	ulFolderId = 0;
	USE_DATABASE();

	if(szName == NULL) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	// Fixup the input strings
	szName = STRIN_FIX(szName);
	szComment = STRIN_FIX(szComment);

	er = lpDatabase->Begin();
	if (er != erSuccess)
		goto exit;

	er = lpecSession->GetObjectFromEntryId(&sParentId, &ulParentId);
	if (er != erSuccess)
		goto exit;

	er = CreateFolder(lpecSession, lpDatabase, ulParentId, lpsNewEntryId, ulType, szName, szComment, fOpenIfExists, true, ulSyncId, &sOrigSourceKey, &ulFolderId, NULL);
	if (er != erSuccess)
		goto exit;

	er = lpDatabase->Commit();
	if (er != erSuccess)
		goto exit;

	er = g_lpSessionManager->GetCacheManager()->GetEntryIdFromObject(ulFolderId, soap, 0, &lpsResponse->sEntryId);
	if (er != erSuccess)
		goto exit;

exit:
	ROLLBACK_ON_ERROR();
}
SOAP_ENTRY_END()

/**
 * tableOpen: Open a mapi table
 *
 * @param[in]	lpecSession	server session object, cannot be NULL
 * @param[in]	sEntryId	entryid data
 * @param[in]	ulTableType	the type of table to open:
 *	TABLETYPE_MS
 *		For all tables of a messagestore.
 *	TABLETYPE_AB
 *		For the addressbook tables.
 *	TABLETYPE_SPOOLER
 *		For the spooler tables, the sEntryId must always the store entryid.
 *		ulType and ulFlags are ignored, reserved for future use.
 *	TABLE_TYPE_MULTISTORE
 *		Special Zarafa only table to have given objects from different stores in one table view.
 *	TABLE_TYPE_USERSTORES
 *		Special Zarafa only table. Lists all combinations of users and stores on this server. (Used for the orphan management in zarafa-admin).
 *	TABLE_TYPE_STATS_*
 *		Special Zarafa only tables. Used for various statistics and other uses.
 * @param[in]	ulType		the type of the object you want to open.
 * @param[in]	ulFlags		ulFlags from the client
 *	MAPI_ASSOCIATED
 *		List associated messages/folders instead of normal messages/folders.
 *	MAPI_UNICODE
 *		Default and all columns will contain _W string properties, otherwise _A strings are used.
 *	CONVENIENT_DEPTH
 *		Returns a convenient depth (flat list) of all folders.
 *	SHOW_SOFT_DELETES
 *		List deleted items in this table, rewritten to MSGFLAG_DELETED.
 *	EC_TABLE_NOCAP
 *		Do not cap string entries to 255 bytes, Zarafa extention.
 *
 * @param[out]	lpulTableId	Server table id for this new table.
 */
ECRESULT OpenTable(ECSession *lpecSession, entryId sEntryId, unsigned int ulTableType, unsigned int ulType, unsigned int ulFlags, unsigned int *lpulTableId)
{
    ECRESULT er = erSuccess;
	objectid_t	sExternId;
	unsigned int	ulTableId = 0;
	unsigned int	ulId = 0;
	unsigned int	ulTypeId = 0;

	switch (ulTableType) {
		case TABLETYPE_MS:
			if(ulFlags & SHOW_SOFT_DELETES)
			{
				ulFlags &=~SHOW_SOFT_DELETES;
				ulFlags |= MSGFLAG_DELETED;
			}

			er = lpecSession->GetObjectFromEntryId(&sEntryId, &ulId);
			if(er != erSuccess)
				goto exit;

			er = lpecSession->GetTableManager()->OpenGenericTable(ulId, ulType, ulFlags, &ulTableId);
			if( er != erSuccess)
				goto exit;

			break;
		case TABLETYPE_AB:
			er = ABEntryIDToID(&sEntryId, &ulId, &sExternId, &ulTypeId);
			if(er != erSuccess)
				goto exit;

			// If an extern id is present, we should get an object based on that.
			if (!sExternId.id.empty())
				er = g_lpSessionManager->GetCacheManager()->GetUserObject(sExternId, &ulId, NULL, NULL);
				
			er = lpecSession->GetTableManager()->OpenABTable(ulId, ulTypeId, ulType, ulFlags, &ulTableId);
			if( er != erSuccess)
				goto exit;

			break;
		case TABLETYPE_SPOOLER:
			// sEntryId must be a store entryid or zero for all stores
			if(sEntryId.__size > 0 ) {
				er = lpecSession->GetObjectFromEntryId(&sEntryId, &ulId);
				if(er != erSuccess)
					goto exit;
			}else
				ulId = 0; //All stores

			er = lpecSession->GetTableManager()->OpenOutgoingQueueTable(ulId, &ulTableId);
			if( er != erSuccess)
				goto exit;

			break;
		case TABLETYPE_MULTISTORE:
			er = lpecSession->GetTableManager()->OpenMultiStoreTable(ulType, ulFlags, &ulTableId);
			if( er != erSuccess)
				goto exit;
			break;
		case TABLETYPE_USERSTORES:
			er = lpecSession->GetTableManager()->OpenUserStoresTable(ulFlags, &ulTableId);
			if( er != erSuccess)
				goto exit;
			break;
		case TABLETYPE_STATS_SYSTEM:
		case TABLETYPE_STATS_SESSIONS:
		case TABLETYPE_STATS_USERS:
		case TABLETYPE_STATS_COMPANY:
		case TABLETYPE_STATS_SERVERS:
			er = lpecSession->GetTableManager()->OpenStatsTable(ulTableType, ulFlags, &ulTableId);
			if (er != erSuccess)
				goto exit;
			break;
		case TABLETYPE_MAILBOX:
			er = lpecSession->GetTableManager()->OpenMailBoxTable(ulFlags, &ulTableId);
			if (er != erSuccess)
				goto exit;
			break;
		default:
			er = ZARAFA_E_BAD_VALUE;
			goto exit;
			break; //Happy compiler
	} // switch (ulTableType)

	*lpulTableId = ulTableId;
exit:
	return er;
}

SOAP_ENTRY_START(tableOpen, lpsTableOpenResponse->er, entryId sEntryId, unsigned int ulTableType, unsigned ulType, unsigned int ulFlags, struct tableOpenResponse *lpsTableOpenResponse)
{
    unsigned int ulTableId = 0;
    
    er = OpenTable(lpecSession, sEntryId, ulTableType, ulType, ulFlags, &ulTableId);
    if(er != erSuccess)
        goto exit;
        
	lpsTableOpenResponse->ulTableId = ulTableId;
exit:
    ;
}
SOAP_ENTRY_END()

/**
 * tableClose: close the table with the specified table ID
 */
SOAP_ENTRY_START(tableClose, *result, unsigned int ulTableId, unsigned int *result)
{
	er = lpecSession->GetTableManager()->CloseTable(ulTableId);
}
SOAP_ENTRY_END()

/**
 * tableSetSearchCritieria: set search criteria for a searchfolder
 */
SOAP_ENTRY_START(tableSetSearchCriteria, *result, entryId sEntryId, struct restrictTable *lpRestrict, struct entryList *lpFolders, unsigned int ulFlags, unsigned int *result)
{
	unsigned int	ulStoreId = 0;
	unsigned int	ulParent = 0;

	if(!(ulFlags & STOP_SEARCH) && (lpRestrict == NULL || lpFolders == NULL)) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	er = lpecSession->GetObjectFromEntryId(&sEntryId, &ulParent);
	if(er != erSuccess)
	    goto exit;

	er = lpecSession->GetSessionManager()->GetCacheManager()->GetStore(ulParent, &ulStoreId, NULL);
	if(er != erSuccess)
		goto exit;

	// Check permission
	er = lpecSession->GetSecurity()->CheckPermission(ulParent, ecSecurityEdit);
	if(er != erSuccess)
		goto exit;

	// If a STOP was requested, then that's all we need to do
	if(ulFlags & STOP_SEARCH) {
		er = lpecSession->GetSessionManager()->GetSearchFolders()->SetSearchCriteria(ulStoreId, ulParent, NULL);
	} else {
		struct searchCriteria sSearchCriteria;

		if (!bSupportUnicode) {
			er = FixRestrictionEncoding(soap, stringCompat, In, lpRestrict);
			if (er != erSuccess)
				goto exit;
		}

		sSearchCriteria.lpRestrict = lpRestrict;
		sSearchCriteria.lpFolders = lpFolders;
		sSearchCriteria.ulFlags = ulFlags;

		er = lpecSession->GetSessionManager()->GetSearchFolders()->SetSearchCriteria(ulStoreId, ulParent, &sSearchCriteria);
	}

exit:
	;
}
SOAP_ENTRY_END()

/**
 * tableGetSearchCriteria: get the search criteria for a searchfolder previously called with tableSetSearchCriteria
 */
SOAP_ENTRY_START(tableGetSearchCriteria, lpsResponse->er,  entryId sEntryId, struct tableGetSearchCriteriaResponse *lpsResponse)
{
	unsigned int	ulFlags = 0;
	unsigned int	ulStoreId = 0;
	unsigned int	ulId = 0;
	struct searchCriteria *lpSearchCriteria = NULL;

	er = lpecSession->GetObjectFromEntryId(&sEntryId, &ulId);
	if(er != erSuccess)
	    goto exit;

	er = g_lpSessionManager->GetCacheManager()->GetStore(ulId, &ulStoreId, NULL);
	 if(er != erSuccess)
		 goto exit;

	// Check permission
	er = lpecSession->GetSecurity()->CheckPermission(ulId, ecSecurityRead);
	if(er != erSuccess)
		goto exit;

	er = lpecSession->GetSessionManager()->GetSearchFolders()->GetSearchCriteria(ulStoreId, ulId, &lpSearchCriteria, &ulFlags);
	if(er != erSuccess)
		goto exit;

	er = CopyRestrictTable(soap, lpSearchCriteria->lpRestrict, &lpsResponse->lpRestrict);
	if(er != erSuccess)
		goto exit;

	if (!bSupportUnicode) {
		er = FixRestrictionEncoding(soap, stringCompat, Out, lpsResponse->lpRestrict);
		if (er != erSuccess)
			goto exit;
	}

	er = CopyEntryList(soap, lpSearchCriteria->lpFolders, &lpsResponse->lpFolderIDs);
	if(er != erSuccess)
		goto exit;

	lpsResponse->ulFlags = ulFlags;

exit:
	if(lpSearchCriteria)
		FreeSearchCriteria(lpSearchCriteria);
}
SOAP_ENTRY_END()

/**
 * tableSetColumns: called from IMAPITable::SetColumns()
 */
SOAP_ENTRY_START(tableSetColumns, *result, unsigned int ulTableId, struct propTagArray *aPropTag, unsigned int *result)
{
	ECGenericObjectTable	*lpTable = NULL;

	er = lpecSession->GetTableManager()->GetTable(ulTableId, &lpTable);
	if(er != erSuccess)
		goto exit;

	er = lpTable->SetColumns(aPropTag, false);

exit:
	if (lpTable)
		lpTable->Release();
}
SOAP_ENTRY_END()

/**
 * tableQueryColumns: called from IMAPITable::GetColumns()
 */
SOAP_ENTRY_START(tableQueryColumns, lpsResponse->er, unsigned int ulTableId, unsigned int ulFlags, struct tableQueryColumnsResponse *lpsResponse)
{
	ECGenericObjectTable	*lpTable = NULL;

	struct propTagArray *lpPropTags = NULL;

	// Init
	lpsResponse->sPropTagArray.__size = 0;
	lpsResponse->sPropTagArray.__ptr = NULL;

	er = lpecSession->GetTableManager()->GetTable(ulTableId, &lpTable);

	if(er != erSuccess)
		goto exit;

	er = lpTable->GetColumns(soap, ulFlags, &lpPropTags);

	if(er != erSuccess)
		goto exit;

	lpsResponse->sPropTagArray.__size = lpPropTags->__size;
	lpsResponse->sPropTagArray.__ptr = lpPropTags->__ptr;

exit:
	if (lpTable)
		lpTable->Release();
}
SOAP_ENTRY_END()

/**
 * tableRestrict: called from IMAPITable::Restrict()
 */
SOAP_ENTRY_START(tableRestrict, *result, unsigned int ulTableId, struct restrictTable *lpsRestrict, unsigned int *result)
{
	ECGenericObjectTable	*lpTable = NULL;

	er = lpecSession->GetTableManager()->GetTable(ulTableId, &lpTable);

	if(er != erSuccess)
		goto exit;

	if (!bSupportUnicode) {
		er = FixRestrictionEncoding(soap, stringCompat, In, lpsRestrict);
		if (er != erSuccess)
			goto exit;
	}

	er = lpTable->Restrict(lpsRestrict);

exit:
	if (lpTable)
		lpTable->Release();
}
SOAP_ENTRY_END()

/**
 * tableSort: called from IMAPITable::Sort()
 */
SOAP_ENTRY_START(tableSort, *result, unsigned int ulTableId, struct sortOrderArray *lpSortOrder, unsigned int ulCategories, unsigned int ulExpanded, unsigned int *result)
{
	ECGenericObjectTable	*lpTable = NULL;

	if(lpSortOrder == NULL) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	er = lpecSession->GetTableManager()->GetTable(ulTableId, &lpTable);

	if(er != erSuccess)
		goto exit;

	er = lpTable->SetSortOrder(lpSortOrder, ulCategories, ulExpanded);

exit:
	if (lpTable)
		lpTable->Release();
}
SOAP_ENTRY_END()

/**
 * tableQueryRows: called from IMAPITable::QueryRows()
 */
SOAP_ENTRY_START(tableQueryRows, lpsResponse->er, unsigned int ulTableId, unsigned int ulRowCount, unsigned int ulFlags, struct tableQueryRowsResponse *lpsResponse)
{
	ECGenericObjectTable	*lpTable = NULL;
	struct rowSet	*lpRowSet = NULL;

	lpsResponse->sRowSet.__ptr = NULL;
	lpsResponse->sRowSet.__size = 0;

	// Get the table
	er = lpecSession->GetTableManager()->GetTable(ulTableId, &lpTable);
	if(er != erSuccess)
		goto exit;

	// FIXME: Check permission

	er = lpTable->QueryRows(soap, ulRowCount, ulFlags, &lpRowSet);
	if(er != erSuccess)
		goto exit;

	if (!bSupportUnicode) {
		er = FixRowSetEncoding(soap, stringCompat, Out, lpRowSet);
		if (er != erSuccess)
			goto exit;
	}

	lpsResponse->sRowSet.__ptr = lpRowSet->__ptr;
	lpsResponse->sRowSet.__size = lpRowSet->__size;
exit:
	if (lpTable)
		lpTable->Release();
}
SOAP_ENTRY_END()

/**
 * tableGetRowCount: called from IMAPITable::GetRowCount()
 */
SOAP_ENTRY_START(tableGetRowCount, lpsResponse->er, unsigned int ulTableId, struct tableGetRowCountResponse *lpsResponse)
{
	ECGenericObjectTable	*lpTable = NULL;

	//FIXME: security? give rowcount 0 is failed ?
	er = lpecSession->GetTableManager()->GetTable(ulTableId, &lpTable);

	if(er != erSuccess)
		goto exit;

	er = lpTable->GetRowCount(&lpsResponse->ulCount, &lpsResponse->ulRow);

exit:
	if (lpTable)
		lpTable->Release();
}
SOAP_ENTRY_END()

/**
 * tableSeekRow: called from IMAPITable::SeekRow()
 */
SOAP_ENTRY_START(tableSeekRow, lpsResponse->er, unsigned int ulTableId , unsigned int ulBookmark, int lRows, struct tableSeekRowResponse *lpsResponse)
{
	ECGenericObjectTable	*lpTable = NULL;

	er = lpecSession->GetTableManager()->GetTable(ulTableId, &lpTable);

	if(er != erSuccess)
		goto exit;

	er = lpTable->SeekRow(ulBookmark, lRows, &lpsResponse->lRowsSought);

exit:
	if (lpTable)
		lpTable->Release();
}
SOAP_ENTRY_END()

/**
 * tableFindRow: called from IMAPITable::FindRow()
 */
SOAP_ENTRY_START(tableFindRow, *result, unsigned int ulTableId ,unsigned int ulBookmark, unsigned int ulFlags, struct restrictTable *lpsRestrict, unsigned int *result)
{
	ECGenericObjectTable	*lpTable = NULL;

	er = lpecSession->GetTableManager()->GetTable(ulTableId, &lpTable);

	if(er != erSuccess)
		goto exit;

	if (!bSupportUnicode) {
		er = FixRestrictionEncoding(soap, stringCompat, In, lpsRestrict);
		if (er != erSuccess)
			goto exit;
	}

	er = lpTable->FindRow(lpsRestrict, ulBookmark, ulFlags);

exit:
	if (lpTable)
		lpTable->Release();
}
SOAP_ENTRY_END()

/**
 * tableCreateBookmark: called from IMAPITable::CreateBookmark()
 */
SOAP_ENTRY_START(tableCreateBookmark, lpsResponse->er, unsigned int ulTableId, struct tableBookmarkResponse *lpsResponse)
{
	ECGenericObjectTable	*lpTable = NULL;
	unsigned int ulbkPosition = 0;

	er = lpecSession->GetTableManager()->GetTable(ulTableId, &lpTable);

	if(er != erSuccess)
		goto exit;

	er = lpTable->CreateBookmark(&ulbkPosition);
	if(er != erSuccess)
		goto exit;

	lpsResponse->ulbkPosition = ulbkPosition;

exit:
	if (lpTable)
		lpTable->Release();
}
SOAP_ENTRY_END()

/**
 * tableCreateBookmark: called from IMAPITable::FreeBookmark()
 */
SOAP_ENTRY_START(tableFreeBookmark, *result, unsigned int ulTableId, unsigned int ulbkPosition, unsigned int *result)
{
	ECGenericObjectTable	*lpTable = NULL;

	er = lpecSession->GetTableManager()->GetTable(ulTableId, &lpTable);

	if(er != erSuccess)
		goto exit;

	er = lpTable->FreeBookmark(ulbkPosition);
	if(er != erSuccess)
		goto exit;

exit:
	if (lpTable)
		lpTable->Release();
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(tableExpandRow, lpsResponse->er, unsigned int ulTableId, xsd__base64Binary sInstanceKey, unsigned int ulRowCount, unsigned int ulFlags, tableExpandRowResponse* lpsResponse)
{
	ECGenericObjectTable	*lpTable = NULL;
	struct rowSet	*lpRowSet = NULL;
	unsigned int ulMoreRows = 0;

	er = lpecSession->GetTableManager()->GetTable(ulTableId, &lpTable);

	if(er != erSuccess)
		goto exit;

	er = lpTable->ExpandRow(soap, sInstanceKey, ulRowCount, ulFlags, &lpRowSet, &ulMoreRows);
	if(er != erSuccess)
		goto exit;

	if (!bSupportUnicode) {
		er = FixRowSetEncoding(soap, stringCompat, Out, lpRowSet);
		if (er != erSuccess)
			goto exit;
	}

    lpsResponse->ulMoreRows = ulMoreRows;
    lpsResponse->rowSet = *lpRowSet;

exit:
	if (lpTable)
		lpTable->Release();
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(tableCollapseRow, lpsResponse->er, unsigned int ulTableId, xsd__base64Binary sInstanceKey, unsigned int ulFlags, tableCollapseRowResponse* lpsResponse)
{
	ECGenericObjectTable	*lpTable = NULL;
	unsigned int ulRows = 0;

	er = lpecSession->GetTableManager()->GetTable(ulTableId, &lpTable);

	if(er != erSuccess)
		goto exit;

	er = lpTable->CollapseRow(sInstanceKey, ulFlags, &ulRows);
	if(er != erSuccess)
		goto exit;

    lpsResponse->ulRows = ulRows;

exit:
    if (lpTable)
		lpTable->Release();
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(tableGetCollapseState, lpsResponse->er, unsigned int ulTableId, struct xsd__base64Binary sBookmark, tableGetCollapseStateResponse *lpsResponse)
{
    ECGenericObjectTable *lpTable = NULL;

    er = lpecSession->GetTableManager()->GetTable(ulTableId, &lpTable);
    if(er != erSuccess)
        goto exit;

    er = lpTable->GetCollapseState(soap, sBookmark, &lpsResponse->sCollapseState);
    if(er != erSuccess)
        goto exit;

exit:
	if (lpTable)
		lpTable->Release();
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(tableSetCollapseState, lpsResponse->er, unsigned int ulTableId, struct xsd__base64Binary sCollapseState, struct tableSetCollapseStateResponse *lpsResponse);
{
    ECGenericObjectTable *lpTable = NULL;

    er = lpecSession->GetTableManager()->GetTable(ulTableId, &lpTable);
    if(er != erSuccess)
        goto exit;

    er = lpTable->SetCollapseState(sCollapseState, &lpsResponse->ulBookmark);
    if(er != erSuccess)
        goto exit;

exit:
	if (lpTable)
		lpTable->Release();

}
SOAP_ENTRY_END()

/**
 * tableSetMultiStoreEntryIDs: client sets content for MultiStoreTable
 */
SOAP_ENTRY_START(tableSetMultiStoreEntryIDs, *result, unsigned int ulTableId, struct entryList *lpEntryList, unsigned int *result)
{
	ECGenericObjectTable	*lpTable = NULL;
	ECMultiStoreTable		*lpMultiStoreTable = NULL;
	ECListInt	lObjectList;


	if(lpEntryList == NULL) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	er = lpecSession->GetTableManager()->GetTable(ulTableId, &lpTable);
	if (er != erSuccess)
		goto exit;

	// ignore errors
	g_lpSessionManager->GetCacheManager()->GetEntryListToObjectList(lpEntryList, &lObjectList);

	lpMultiStoreTable = dynamic_cast<ECMultiStoreTable*>(lpTable);
	if (!lpMultiStoreTable) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	er = lpMultiStoreTable->SetEntryIDs(&lObjectList);
	if(er != erSuccess)
	    goto exit;

exit:
	if (lpTable)
		lpTable->Release();

}
SOAP_ENTRY_END()

SOAP_ENTRY_START(tableMulti, lpsResponse->er, struct tableMultiRequest sRequest, struct tableMultiResponse *lpsResponse)
{
    unsigned int ulTableId = sRequest.ulTableId;
    ECGenericObjectTable *lpTable = NULL;
    struct rowSet *lpRowSet = NULL;
    
    if(sRequest.lpOpen) {
        er = OpenTable(lpecSession, sRequest.lpOpen->sEntryId, sRequest.lpOpen->ulTableType, sRequest.lpOpen->ulType, sRequest.lpOpen->ulFlags, &lpsResponse->ulTableId);
		if(er != erSuccess)
			goto exit;

        ulTableId = lpsResponse->ulTableId;
    }
    
	er = lpecSession->GetTableManager()->GetTable(ulTableId, &lpTable);
	if(er != erSuccess)
		goto exit;

    if(sRequest.lpSort) {
        er = lpTable->SetSortOrder(&sRequest.lpSort->sSortOrder, sRequest.lpSort->ulCategories, sRequest.lpSort->ulExpanded);
        if(er != erSuccess)
            goto exit;
    }

    if(sRequest.lpSetColumns) {
        er = lpTable->SetColumns(sRequest.lpSetColumns, false);
        if(er != erSuccess)
            goto exit;
    }
    
    if(sRequest.lpRestrict || (sRequest.ulFlags&TABLE_MULTI_CLEAR_RESTRICTION)) {
		if (!bSupportUnicode && sRequest.lpRestrict) {
			er = FixRestrictionEncoding(soap, stringCompat, In, sRequest.lpRestrict);
			if (er != erSuccess)
				goto exit;
		}

        er = lpTable->Restrict(sRequest.lpRestrict);
        if(er != erSuccess)
            goto exit;
    }
    
    if(sRequest.lpQueryRows) {
        er = lpTable->QueryRows(soap, sRequest.lpQueryRows->ulCount, sRequest.lpQueryRows->ulFlags, &lpRowSet);
        if(er != erSuccess)
            goto exit;

		if (!bSupportUnicode) {
			er = FixRowSetEncoding(soap, stringCompat, Out, lpRowSet);
			if (er != erSuccess)
				goto exit;
		}
                
        lpsResponse->sRowSet.__ptr = lpRowSet->__ptr;
        lpsResponse->sRowSet.__size = lpRowSet->__size;
    }

exit:
	if (lpTable)
		lpTable->Release();

}
SOAP_ENTRY_END()

// Delete a set of messages, recipients, or attachments
SOAP_ENTRY_START(deleteObjects, *result, unsigned int ulFlags, struct entryList *lpEntryList, unsigned int ulSyncId, unsigned int *result)
{
	ECListInt	lObjectList;
	unsigned int ulDeleteFlags = EC_DELETE_ATTACHMENTS | EC_DELETE_RECIPIENTS | EC_DELETE_CONTAINER | EC_DELETE_MESSAGES;
	USE_DATABASE();

	if(lpEntryList == NULL) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	if(ulFlags & DELETE_HARD_DELETE)
		ulDeleteFlags |= EC_DELETE_HARD_DELETE;

	// ignore errors
	g_lpSessionManager->GetCacheManager()->GetEntryListToObjectList(lpEntryList, &lObjectList);

	er = DeleteObjects(lpecSession, lpDatabase, &lObjectList, ulDeleteFlags, ulSyncId, false, true);
	if(er != erSuccess)
		goto exit;

exit:
    ;
}
SOAP_ENTRY_END()

// Delete everything in a folder, but not the folder itself
// Quirk: this works with messages also, deleting attachments and recipients, but not the message itself.
//FIXME: michel? what with associated messages ?
SOAP_ENTRY_START(emptyFolder, *result, entryId sEntryId, unsigned int ulFlags, unsigned int ulSyncId, unsigned int *result)
{
	unsigned int		ulDeleteFlags = 0;
	unsigned int		ulId = 0;
	ECListInt			lObjectIds;
	USE_DATABASE();

	er = lpecSession->GetObjectFromEntryId(&sEntryId, &ulId);
	if(er != erSuccess)
	    goto exit;

	// Check Rights set permission
	er = lpecSession->GetSecurity()->CheckPermission(ulId, ecSecurityDelete);
	if(er != erSuccess)
		goto exit;

	// Add object into the list
	lObjectIds.push_back(ulId);

	ulDeleteFlags = EC_DELETE_MESSAGES | EC_DELETE_FOLDERS | EC_DELETE_RECIPIENTS | EC_DELETE_ATTACHMENTS;

	if((ulFlags & DELETE_HARD_DELETE) == DELETE_HARD_DELETE)
		ulDeleteFlags |= EC_DELETE_HARD_DELETE;

	if((ulFlags&DEL_ASSOCIATED) == 0)
		ulDeleteFlags |= EC_DELETE_NOT_ASSOCIATED_MSG;

	er = DeleteObjects(lpecSession, lpDatabase, &lObjectIds, ulDeleteFlags, ulSyncId, false, true);
	if (er != erSuccess)
		goto exit;

exit:
	ROLLBACK_ON_ERROR();
}
SOAP_ENTRY_END()

/* FIXME
 *
 * Currently, when deleteFolders is called with DEL_FOLDERS but without DEL_MESSAGES, it will return an error
 * when a subfolder of the specified folder contains messages. I don't think this is up to spec. DeleteObjects
 * should therefore be changed so that the check is only done against messages and folders directly under the
 * top-level object.
 */

// Deletes a complete folder, with optional recursive subfolder and submessage deletion
SOAP_ENTRY_START(deleteFolder, *result,  entryId sEntryId, unsigned int ulFlags, unsigned int ulSyncId, unsigned int *result)
{
	unsigned int		ulDeleteFlags = 0;
	unsigned int		ulId = 0;
	unsigned int		ulFolderFlags = 0;
	ECListInt			lObjectIds;
	USE_DATABASE();

	er = lpecSession->GetObjectFromEntryId(&sEntryId, &ulId);
	if(er != erSuccess)
	    goto exit;

	// Check permission
	er = lpecSession->GetSecurity()->CheckPermission(ulId, ecSecurityFolderAccess);
	if(er != erSuccess)
		goto exit;

	er = g_lpSessionManager->GetCacheManager()->GetObjectFlags(ulId, &ulFolderFlags);
	if(er != erSuccess)
		goto exit;

	// insert objectid into the delete list
	lObjectIds.push_back(ulId);

	ulDeleteFlags = EC_DELETE_CONTAINER;

	if(ulFlags & DEL_FOLDERS)
		ulDeleteFlags |= EC_DELETE_FOLDERS;

	if(ulFlags & DEL_MESSAGES)
		ulDeleteFlags |= EC_DELETE_MESSAGES | EC_DELETE_RECIPIENTS | EC_DELETE_ATTACHMENTS;

	if( (ulFlags & DELETE_HARD_DELETE) || ulFolderFlags == FOLDER_SEARCH)
		ulDeleteFlags |= EC_DELETE_HARD_DELETE;

	er = DeleteObjects(lpecSession, lpDatabase, &lObjectIds, ulDeleteFlags, ulSyncId, false, true);
	if (er != erSuccess)
		goto exit;

exit:
	ROLLBACK_ON_ERROR();
}
SOAP_ENTRY_END()

ECRESULT DoNotifySubscribe(ECSession *lpecSession, unsigned long long ulSessionId, struct notifySubscribe *notifySubscribe)
{
	ECRESULT er = erSuccess;
	unsigned int ulKey = 0;
	ECGenericObjectTable *lpTable = NULL;

	//NOTE: An sKey with size 4 is a table notification id
	if(notifySubscribe->sKey.__size == 4) {
		 memcpy(&ulKey, notifySubscribe->sKey.__ptr, 4);
	}else {
		er = lpecSession->GetObjectFromEntryId(&notifySubscribe->sKey, &ulKey);
		if(er != erSuccess)
			goto exit;

		// Check permissions
		er = lpecSession->GetSecurity()->CheckPermission(ulKey, ecSecurityFolderVisible);
		if(er != erSuccess)
			goto exit;
	}
	
	if(notifySubscribe->ulEventMask & fnevTableModified) {
	    // An advise has been done on a table. The table ID is in 'ulKey' in this case. When this is done
	    // we have to populate the table first since row modifications would otherwise be wrong until the
	    // table is populated; if the table is unpopulated and a row changes, the row will be added into the table
	    // whenever it is modified, producing a TABLE_ROW_ADDED for that row instead of the correct TABLE_ROW_MODIFIED.
	    er = lpecSession->GetTableManager()->GetTable(ulKey, &lpTable);
	    if(er != erSuccess)
	        goto exit;
	        
        er = lpTable->Populate();
        if(er != erSuccess)
            goto exit;
	}

	er = lpecSession->AddAdvise(notifySubscribe->ulConnection, ulKey, notifySubscribe->ulEventMask);
	if(er == erSuccess){
		TRACE_SOAP(TRACE_INFO, "ns__notifySubscribe", "connectionId: %d SessionId: %d Mask: %d",notifySubscribe->ulConnection, ulSessionId, notifySubscribe->ulEventMask);
	}

exit:
	if (lpTable)
		lpTable->Release();

	return er;
}

SOAP_ENTRY_START(notifySubscribe, *result,  struct notifySubscribe *notifySubscribe, unsigned int *result)
{
	if (notifySubscribe == NULL) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	if (notifySubscribe->ulEventMask == fnevZarafaIcsChange)
		er = lpecSession->AddChangeAdvise(notifySubscribe->ulConnection, &notifySubscribe->sSyncState);

	else
		er = DoNotifySubscribe(lpecSession, ulSessionId, notifySubscribe);

	if (er != erSuccess)
		goto exit;

exit:
    ;
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(notifySubscribeMulti, *result, struct notifySubscribeArray *notifySubscribeArray, unsigned int *result)
{
	if (notifySubscribeArray == NULL) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	for (unsigned i = 0; i < notifySubscribeArray->__size; ++i) {
		if (notifySubscribeArray->__ptr[i].ulEventMask == fnevZarafaIcsChange)
			er = lpecSession->AddChangeAdvise(notifySubscribeArray->__ptr[i].ulConnection, &notifySubscribeArray->__ptr[i].sSyncState);

		else
			er = DoNotifySubscribe(lpecSession, ulSessionId, &notifySubscribeArray->__ptr[i]);

		if (er != erSuccess) {
			for (unsigned j = 0; j < i; ++j)
				lpecSession->DelAdvise(notifySubscribeArray->__ptr[j].ulConnection);

			break;
		}
	}
exit:
	;
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(notifyUnSubscribe, *result, unsigned int ulConnection, unsigned int *result)
{
	er = lpecSession->DelAdvise(ulConnection);
	if (er != erSuccess)
		goto exit;

exit:
    ;
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(notifyUnSubscribeMulti, *result, struct mv_long *ulConnectionArray, unsigned int *result)
{
	unsigned int erTmp = erSuccess;
	unsigned int erFirst = erSuccess;

	if (ulConnectionArray == NULL) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	for (int i = 0; i < ulConnectionArray->__size; ++i) {
		erTmp = lpecSession->DelAdvise(ulConnectionArray->__ptr[i]);
		if (erTmp != erSuccess && erFirst == erSuccess)
			erFirst = erTmp;
	}

	// return first seen error (if any).
	er = erFirst;

exit:
    ;
}
SOAP_ENTRY_END()

/*
 * Gets notifications queued for the session group that the specified session is attached to; you can access
 * all notifications of a session group via any session on that group. The request itself is handled by the
 * ECNotificationManager class since you don't want to block the calling thread while waiting for notifications.
 */
int ns__notifyGetItems(struct soap *soap, ULONG64 ulSessionId, struct notifyResponse *notifications)
{
	ECRESULT er = erSuccess;
	ECSession *lpSession = NULL;
	
	// Check if the session exists, and discard result
	er = g_lpSessionManager->ValidateSession(soap, ulSessionId, &lpSession, true);
	if(er != erSuccess) {
		// Directly return with error in er
		notifications->er = er;
		// SOAP call itself succeeded
		return SOAP_OK;
	}
	
	// discard lpSession
	lpSession->Unlock();
	lpSession = NULL;
	
    g_lpSessionManager->DeferNotificationProcessing(ulSessionId, soap);

    // Return SOAP_NULL so that the caller does *nothing* with the soap struct since we have passed it to the session
    // manager for deferred processing
    throw SOAP_NULL;
}

SOAP_ENTRY_START(getRights, lpsRightResponse->er, entryId sEntryId, int ulType, struct rightsResponse *lpsRightResponse)
{
	unsigned int	ulobjid = 0;

	struct rightsArray *lpsRightArray = NULL;

	er = lpecSession->GetObjectFromEntryId(&sEntryId, &ulobjid);
	if(er != erSuccess)
	    goto exit;

	lpsRightArray = new struct rightsArray;
	memset(lpsRightArray, 0, sizeof(struct rightsArray));

	er = lpecSession->GetSecurity()->GetRights(ulobjid, ulType, lpsRightArray);
	if(er != erSuccess)
		goto exit;

	er = CopyRightsArrayToSoap(soap, lpsRightArray, &lpsRightResponse->pRightsArray);
	if (er != erSuccess)
		goto exit;

exit:
	if (lpsRightArray) {
		for (unsigned int i = 0; i < lpsRightArray->__size; i++)
			if (lpsRightArray->__ptr[i].sUserId.__ptr)
				delete [] lpsRightArray->__ptr[i].sUserId.__ptr;

		if (lpsRightArray->__size > 0)
			delete[] lpsRightArray->__ptr;

		delete lpsRightArray;
	}
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(setRights, *result, entryId sEntryId, struct rightsArray *lpsRightsArray, unsigned int *result)
{
	unsigned int	ulObjId = 0;

	if(lpsRightsArray == NULL) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	er = lpecSession->GetObjectFromEntryId(&sEntryId, &ulObjId);
	if(er != erSuccess)
	    goto exit;

	// Check Rights set permission
	er = lpecSession->GetSecurity()->CheckPermission(ulObjId, ecSecurityFolderAccess);
	if(er != erSuccess)
		goto exit;

	er = lpecSession->GetSecurity()->SetRights(ulObjId, lpsRightsArray);
	if(er != erSuccess)
		goto exit;

exit:
    ;
}
SOAP_ENTRY_END()

/* DEPRICATED:
 *   this is only accessable through IECSecurity, and nobody calls this anymore!
 */
SOAP_ENTRY_START(getUserObjectList, lpsUserObjectResponse->er, unsigned int ulCompanyId, entryId sCompanyId, int ulType, struct userobjectResponse *lpsUserObjectResponse)
{
	// TODO: if we just throw the soap call away, it has the same result?
	er = ZARAFA_E_NO_SUPPORT;
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(getOwner, lpsResponse->er, entryId sEntryId, struct getOwnerResponse *lpsResponse)
{
	unsigned int	ulobjid = 0;

	er = lpecSession->GetObjectFromEntryId(&sEntryId, &ulobjid);
	if(er != erSuccess)
	    goto exit;

	er = lpecSession->GetSecurity()->GetOwner(ulobjid, &lpsResponse->ulOwner);
	if(er != erSuccess)
		goto exit;

	er = GetABEntryID(lpsResponse->ulOwner, soap, &lpsResponse->sOwner);
	if (er != erSuccess)
		goto exit;

exit:
    ;
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(getIDsFromNames, lpsResponse->er,  struct namedPropArray *lpsNamedProps, unsigned int ulFlags, struct getIDsFromNamesResponse *lpsResponse)
{
	unsigned int	i;
	std::string		strEscapedString;
	std::string		strEscapedGUID;
	unsigned int	ulLastId = 0;
    USE_DATABASE();

	if(lpsNamedProps == NULL) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	er = lpDatabase->Begin();
	if(er != erSuccess)
		goto exit;

	lpsResponse->lpsPropTags.__ptr = s_alloc<unsigned int>(soap, lpsNamedProps->__size);
	lpsResponse->lpsPropTags.__size = 0;

	// One query per named property (too slow ?) FIXME could be faster if brought down to less SQL queries
	for(i=0;i<lpsNamedProps->__size;i++) {
		strQuery = "SELECT id FROM names WHERE ";

		// ID, then add ID where clause
		if(lpsNamedProps->__ptr[i].lpId != NULL) {
			strQuery += "nameid=" + stringify(*lpsNamedProps->__ptr[i].lpId) + " ";
		}

		// String, then add STRING where clause
		else if(lpsNamedProps->__ptr[i].lpString != NULL) {
			strEscapedString = lpDatabase->Escape(lpsNamedProps->__ptr[i].lpString);

			strQuery += "namestring='" + strEscapedString + "' ";
		}

		// Add a GUID specifier if there
		if(lpsNamedProps->__ptr[i].lpguid != NULL) {
			strEscapedGUID = lpDatabase->EscapeBinary(lpsNamedProps->__ptr[i].lpguid->__ptr, lpsNamedProps->__ptr[i].lpguid->__size);

			strQuery += "AND guid=" + strEscapedGUID;
		}

		// Run the query
		er = lpDatabase->DoSelect(strQuery, &lpDBResult);
		if(er != erSuccess)
			goto exit;

		if(lpDatabase->GetNumRows(lpDBResult) == 0) {
			// No rows found, so the named property has not been registered yet

			if(ulFlags & MAPI_CREATE) {
				// Create requested ? then add a new named property

				// GUID must be specified with create
				if(lpsNamedProps->__ptr[i].lpguid == NULL) {
					er = ZARAFA_E_NO_ACCESS;
					goto exit;
				}

				strQuery = "INSERT INTO names (nameid, namestring, guid) VALUES(";

				if(lpsNamedProps->__ptr[i].lpId) {
					strQuery += stringify(*lpsNamedProps->__ptr[i].lpId);
				} else {
					strQuery += "null";
				}

				strQuery += ",";

				if(lpsNamedProps->__ptr[i].lpString) {
					strQuery += "'" + strEscapedString + "'";
				} else {
					strQuery += "null";
				}

				strQuery += ",";

				if(lpsNamedProps->__ptr[i].lpguid) {
					strQuery += strEscapedGUID;
				} else {
					strQuery += "null";
				}

				strQuery += ")";

				er = lpDatabase->DoInsert(strQuery, &ulLastId);
				if(er != erSuccess)
					goto exit;

				lpsResponse->lpsPropTags.__ptr[i] = ulLastId+1; // offset one because 0 is 'not found'
			} else {
				// No create ? Then not found
				lpsResponse->lpsPropTags.__ptr[i] = 0;
			}
		} else {
			// found it
			lpDBRow = lpDatabase->FetchRow(lpDBResult);

			if(lpDBRow!= NULL && lpDBRow[0] != NULL)
				lpsResponse->lpsPropTags.__ptr[i] = atoi(lpDBRow[0])+1;
			else
				lpsResponse->lpsPropTags.__ptr[i] = 0;
		}

		//Free database results
		FREE_DBRESULT();
	}

	// Everything is done, now set the size
	lpsResponse->lpsPropTags.__size = lpsNamedProps->__size;

	er = lpDatabase->Commit();
	if(er != erSuccess)
		goto exit;
exit:
    FREE_DBRESULT();
	ROLLBACK_ON_ERROR();
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(getNamesFromIDs, lpsResponse->er, struct propTagArray *lpPropTags, struct getNamesFromIDsResponse *lpsResponse)
{
	struct namedPropArray lpsNames;
	USE_DATABASE();

	if(lpPropTags == NULL) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}
	
	er = GetNamesFromIDs(soap, lpDatabase, lpPropTags, &lpsNames);
	if (er != erSuccess)
	    goto exit;
	    
    lpsResponse->lpsNames = lpsNames;
	
exit:
    ;
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(getReceiveFolder, lpsReceiveFolder->er, entryId sStoreId, char* lpszMessageClass, struct receiveFolderResponse *lpsReceiveFolder)
{
	unsigned int	ulStoreid = 0;
	char			*lpDest;
	USE_DATABASE();

	lpszMessageClass = STRIN_FIX(lpszMessageClass);

	er = lpecSession->GetObjectFromEntryId(&sStoreId, &ulStoreid);
	if(er != erSuccess)
	    goto exit;

	// Check for default store
	if(lpszMessageClass == NULL)
		lpszMessageClass = "";

	strQuery = "SELECT objid, messageclass FROM receivefolder WHERE storeid="+stringify(ulStoreid)+" AND (";

	strQuery += "messageclass='"+lpDatabase->Escape(lpszMessageClass)+"'";

	lpDest = lpszMessageClass;
	do {
		lpDest = strchr(lpDest, '.');

		if(lpDest){
			strQuery += " OR messageclass='"+lpDatabase->Escape(string(lpszMessageClass, lpDest-lpszMessageClass))+"'";
			lpDest++;
		}

	}while(lpDest);

	if(strlen(lpszMessageClass) != 0)
		strQuery += " OR messageclass=''";

	strQuery += ") ORDER BY length(messageclass) DESC LIMIT 1";

	er = lpDatabase->DoSelect(strQuery, &lpDBResult);
	if(er != erSuccess)
		goto exit;


	if(lpDatabase->GetNumRows(lpDBResult) == 1) {
		lpDBRow = lpDatabase->FetchRow(lpDBResult);

		if(lpDBRow == NULL || lpDBRow[0] == NULL || lpDBRow[1] == NULL){
			er = ZARAFA_E_DATABASE_ERROR;
			goto exit;
		}

		er = g_lpSessionManager->GetCacheManager()->GetEntryIdFromObject(atoui(lpDBRow[0]), soap, 0, &lpsReceiveFolder->sReceiveFolder.sEntryId);
		if(er != erSuccess)
			goto exit;

		lpsReceiveFolder->sReceiveFolder.lpszAExplicitClass = STROUT_FIX_CPY(lpDBRow[1]);
	}else{
		//items not found
		er = ZARAFA_E_NOT_FOUND;
	}


exit:
    FREE_DBRESULT();
}
SOAP_ENTRY_END()

// FIXME: should be able to delete an entry too
SOAP_ENTRY_START(setReceiveFolder, *result, entryId sStoreId, entryId* lpsEntryId, char* lpszMessageClass, unsigned int *result)
{
	bool			bIsUpdate = false;
	unsigned int	ulCheckStoreId = 0;
	unsigned int	ulStoreid = 0;
	unsigned int	ulId = 0;
	USE_DATABASE();

	lpszMessageClass = STRIN_FIX(lpszMessageClass);

	er = lpDatabase->Begin();
	if (er != erSuccess)
		goto exit;

	// Check, lpsEntryId and lpszMessageClass can't both be empty or 0
	if(lpsEntryId == NULL && (lpszMessageClass == NULL || lpszMessageClass == '\0') ){
		er = ZARAFA_E_INVALID_TYPE;
		goto exit;
	}

	er = lpecSession->GetObjectFromEntryId(&sStoreId, &ulStoreid);
	if(er != erSuccess)
	    goto exit;

	// an empty lpszMessageClass is the default folder
	if(lpszMessageClass == NULL)
		lpszMessageClass = "";

	// If the lpsEntryId parameter is set to NULL then replace the current receive folder with the message store's default.
	if(lpsEntryId)
	{
		// Check if object really exist and the relation between storeid and ulId

		er = lpecSession->GetObjectFromEntryId(lpsEntryId, &ulId);
		if(er != erSuccess)
			goto exit;

		// Check if storeid and ulId have a relation
		er = lpecSession->GetSessionManager()->GetCacheManager()->GetStore(ulId, &ulCheckStoreId, NULL);
		if(er != erSuccess){
			er = ZARAFA_E_INVALID_ENTRYID;
			goto exit;
		}

		if(ulStoreid != ulCheckStoreId) {
			er = ZARAFA_E_INVALID_ENTRYID;
			goto exit;
		}

		er = lpecSession->GetSecurity()->CheckDeletedParent(ulId);
		if (er != erSuccess)
			goto exit;

	} else {
		// Set MessageClass with the default of the store (that's the empty MessageClass)
		strQuery = "SELECT objid FROM receivefolder WHERE storeid="+stringify(ulStoreid)+" AND messageclass=''";
		er = lpDatabase->DoSelect(strQuery, &lpDBResult);
		if(er != erSuccess)
			goto exit;

		if(lpDatabase->GetNumRows(lpDBResult) == 1)
		{
			lpDBRow = lpDatabase->FetchRow(lpDBResult);

			if(lpDBRow == NULL || lpDBRow[0] == NULL){
				er = ZARAFA_E_DATABASE_ERROR;
				goto exit;
			}

			//Set the default folder
			ulId = atoi(lpDBRow[0]);
		}else{
			er = ZARAFA_E_DATABASE_ERROR; //FIXME: no default error ?
			goto exit;
		}

		FREE_DBRESULT();
	}


	strQuery = "SELECT objid, id FROM receivefolder WHERE storeid="+stringify(ulStoreid)+" AND messageclass='"+lpDatabase->Escape(lpszMessageClass)+"'";
	er = lpDatabase->DoSelect(strQuery, &lpDBResult);
	if(er != erSuccess)
		goto exit;

	bIsUpdate = false;
	// If ok, item already exists, return ok
	if(lpDatabase->GetNumRows(lpDBResult) == 1){
		lpDBRow = lpDatabase->FetchRow(lpDBResult);

		if(lpDBRow == NULL || lpDBRow[0] == NULL || lpDBRow[1] == NULL){
			er = ZARAFA_E_DATABASE_ERROR;
			goto exit;
		}

		// Item exists
		if(ulId == atoui(lpDBRow[0])){
			er = lpDatabase->Rollback();	// Nothing changed, so Commit() would also do.
			goto exit;
		}

		bIsUpdate = true;
	}

	//Free database results
	FREE_DBRESULT();

	// Check permission
	//FIXME: also on delete?
	if(bIsUpdate)
		er = lpecSession->GetSecurity()->CheckPermission(ulStoreid, ecSecurityEdit);
	else
		er = lpecSession->GetSecurity()->CheckPermission(ulStoreid, ecSecurityCreate);
	if(er != erSuccess)
		goto exit;


	if(bIsUpdate) {
		strQuery = "UPDATE receivefolder SET objid="+stringify(ulId);
		strQuery+= " WHERE storeid="+stringify(ulStoreid)+" AND messageclass='"+lpDatabase->Escape(lpszMessageClass)+"'";
		er = lpDatabase->DoUpdate(strQuery);
	}else{
		strQuery = "INSERT INTO receivefolder (storeid, objid, messageclass) VALUES (";
		strQuery += stringify(ulStoreid)+", "+stringify(ulId)+", '"+lpDatabase->Escape(lpszMessageClass)+"')";
		er = lpDatabase->DoInsert(strQuery);
	}

	if(er != erSuccess)
		goto exit;

	er = lpDatabase->Commit();
	if (er != erSuccess)
		goto exit;

exit:
    FREE_DBRESULT();
	ROLLBACK_ON_ERROR();
}
SOAP_ENTRY_END()

/*
 * WARNING
 *
 * lpsEntryID != NULL && lpMessageList != NULL: messages in lpMessageList must be set, lpsEntryId MUST BE IGNORED (may be entryid of search folder)
 * lpsEntryID == NULL && lpMessageList != NULL: called from IMessage::SetReadFlag, lpMessageList->__size == 1
 * lpsEntryID != NULL && lpMessageList == NULL: 'mark all messages as (un)read'
 *
 * Items are assumed to all be in the same store.
 *
 */
SOAP_ENTRY_START(setReadFlags, *result, unsigned int ulFlags, entryId* lpsEntryId, struct entryList *lpMessageList, unsigned int ulSyncId, unsigned int *result)
{
	std::list<unsigned int> lHierarchyIDs;
	std::list<unsigned int>::iterator iterHierarchyIDs;
	std::list<std::pair<unsigned int, unsigned int>	> lObjectIds;
	std::list<std::pair<unsigned int, unsigned int> >::iterator iObjectid;
	std::string		strQueryCache;
	USE_DATABASE();

	unsigned int	i = 0;
	unsigned int	ulParent = 0;
	unsigned int	ulGrandParent = 0;
	unsigned int	ulFlagsNotify = 0;
	unsigned int	ulFlagsRemove = 0;
	unsigned int	ulFlagsAdd = 0;
	unsigned int	ulFolderId = 0;
	size_t			cObjectSize = 0;
	
	// List of unique parents
	std::map<unsigned int, int> mapParents;
	std::map<unsigned int, int>::iterator iterParents;
	std::set<unsigned int> setParents;
	std::set<unsigned int>::iterator iParents;

	//NOTE: either lpMessageList may be NULL or lpsEntryId may be NULL

	if(ulFlags & GENERATE_RECEIPT_ONLY)
		goto exit;

    if(lpMessageList == NULL && lpsEntryId == NULL) {
        // Bad input
        er = ZARAFA_E_INVALID_PARAMETER;
        goto exit;
    }

	if(lpMessageList) {
		// Ignore errors
		g_lpSessionManager->GetCacheManager()->GetEntryListToObjectList(lpMessageList, &lHierarchyIDs);
	}

	strQuery = "UPDATE properties SET ";

	if ((ulFlags & CLEAR_NRN_PENDING) || (ulFlags & SUPPRESS_RECEIPT) || (ulFlags & GENERATE_RECEIPT_ONLY) )
		ulFlagsRemove |= MSGFLAG_NRN_PENDING;

	if ((ulFlags & CLEAR_RN_PENDING) || (ulFlags & SUPPRESS_RECEIPT) || (ulFlags & GENERATE_RECEIPT_ONLY) )
		ulFlagsRemove |= MSGFLAG_RN_PENDING;

    if (!(ulFlags & GENERATE_RECEIPT_ONLY) && (ulFlags & CLEAR_READ_FLAG))
        ulFlagsRemove |= MSGFLAG_READ;
	else if( !(ulFlags & GENERATE_RECEIPT_ONLY) )
        ulFlagsAdd |= MSGFLAG_READ;

	if(ulFlagsRemove != 0)
		strQuery += "val_ulong=val_ulong & ~" + stringify(ulFlagsRemove);

	if(ulFlagsAdd != 0) {
		strQuery += (ulFlagsRemove!=0)?",":"";
		strQuery += "val_ulong=val_ulong | " + stringify(ulFlagsAdd);
	}

	if(ulFlagsRemove == 0 && ulFlagsAdd == 0)
	{
		// Nothing to update
		goto exit;
	}

	er = lpDatabase->Begin();
	if(er != erSuccess)
		goto exit;

	if(lpMessageList == NULL) {
	    // No message list passed, so 'mark all items (un)read'
        er = lpecSession->GetObjectFromEntryId(lpsEntryId, &ulFolderId);
        if(er != erSuccess)
            goto exit;
            
        er = lpDatabase->DoSelect("SELECT val_ulong FROM properties WHERE hierarchyid=" + stringify(ulFolderId) + " FOR UPDATE", NULL);
        if(er != erSuccess)
            goto exit;

        // Check permission
        er = lpecSession->GetSecurity()->CheckPermission(ulFolderId, ecSecurityRead);
        if(er != erSuccess)
            goto exit;

		// Purge changes
		ECTPropsPurge::PurgeDeferredTableUpdates(lpDatabase, ulFolderId);

		// Get all items MAPI_MESSAGE exclude items with flags MSGFLAG_DELETED AND MSGFLAG_ASSOCIATED of which we will be changing flags
		
		// Note we use FOR UPDATE which locks the records in the hierarchy (and in tproperties as a sideeffect), which serializes access to the rows, avoiding deadlocks
		strQueryCache = "SELECT id, tproperties.val_ulong FROM hierarchy FORCE INDEX(parenttypeflags) JOIN tproperties FORCE INDEX(PRIMARY) ON tproperties.hierarchyid=hierarchy.id AND tproperties.tag = " + stringify(PROP_ID(PR_MESSAGE_FLAGS)) + " AND tproperties.type = " + stringify(PROP_TYPE(PR_MESSAGE_FLAGS)) + " WHERE parent="+ stringify(ulFolderId) + " AND hierarchy.type=5 AND flags = 0 AND (tproperties.val_ulong & " + stringify(ulFlagsRemove) + " OR tproperties.val_ulong & " + stringify(ulFlagsAdd) + " != " + stringify(ulFlagsAdd) + ") AND tproperties.folderid = " + stringify(ulFolderId) + " FOR UPDATE";
		er = lpDatabase->DoSelect(strQueryCache, &lpDBResult);
		if(er != erSuccess)
			goto exit;

		while( (lpDBRow = lpDatabase->FetchRow(lpDBResult)) )
		{
			if(lpDBRow[0] == NULL || lpDBRow[1] == NULL){
				er = ZARAFA_E_DATABASE_ERROR;
				goto exit;
			}

			lObjectIds.push_back( std::pair<unsigned int, unsigned int>( atoui(lpDBRow[0]), atoui(lpDBRow[1]) ));
            i++;
		}

		ulParent = ulFolderId;

        FREE_DBRESULT();
	} else {
		if(lHierarchyIDs.empty()) {
			// Nothing to do
			lpDatabase->Commit();
			goto exit;
		}
		
	    // Because the messagelist can contain messages from all over the place, we have to check permissions for all the parent folders of the items
	    // we are setting 'read' or 'unread'
		for(iterHierarchyIDs = lHierarchyIDs.begin(); iterHierarchyIDs != lHierarchyIDs.end(); iterHierarchyIDs++)
		{

			// Get the parent object. Note that the cache will hold this information so the loop below with GetObject() will
			// be done directly from the cache (assuming it's not too large)
			if(g_lpSessionManager->GetCacheManager()->GetObject(*iterHierarchyIDs, &ulParent, NULL, NULL) != erSuccess) {
			    continue;
			}
			
			setParents.insert(ulParent);
        }

        // Lock parent folders        
        for(iParents = setParents.begin(); iParents != setParents.end(); iParents++) {
            er = lpDatabase->DoSelect("SELECT val_ulong FROM properties WHERE hierarchyid=" + stringify(*iParents) + " FOR UPDATE", NULL);
            if(er != erSuccess)
                goto exit;
        }

        // Check permission
        for(iParents = setParents.begin(); iParents != setParents.end(); iParents++) {
            er = lpecSession->GetSecurity()->CheckPermission(ulParent, ecSecurityRead);
            if(er != erSuccess)
                goto exit;
        }
        
        // Now find all messages that will actually change
		strQueryCache = "SELECT id, properties.val_ulong FROM hierarchy JOIN properties ON hierarchy.id=properties.hierarchyid AND properties.tag = " + stringify(PROP_ID(PR_MESSAGE_FLAGS)) + " AND properties.type = " + stringify(PROP_TYPE(PR_MESSAGE_FLAGS)) + " WHERE hierarchy.type=5 AND flags = 0 AND (properties.val_ulong & " + stringify(ulFlagsRemove) + " OR properties.val_ulong & " + stringify(ulFlagsAdd) + " != " + stringify(ulFlagsAdd) + ") AND hierarchyid IN (";
		for(iterHierarchyIDs = lHierarchyIDs.begin(); iterHierarchyIDs != lHierarchyIDs.end(); iterHierarchyIDs++)
		{
			if(iterHierarchyIDs != lHierarchyIDs.begin())
				strQueryCache += ",";
				
			strQueryCache += stringify(*iterHierarchyIDs);
		}
		strQueryCache += ") FOR UPDATE"; // See comment above about FOR UPDATE
		er = lpDatabase->DoSelect(strQueryCache, &lpDBResult);
		if(er != erSuccess)
			goto exit;


		while( (lpDBRow = lpDatabase->FetchRow(lpDBResult)) )
		{
			if(lpDBRow[0] == NULL || lpDBRow[1] == NULL){
				er = ZARAFA_E_DATABASE_ERROR;
				goto exit;
			}

			lObjectIds.push_back( std::pair<unsigned int, unsigned int>( atoui(lpDBRow[0]), atoui(lpDBRow[1]) ) );
            i++;
		}
		
		FREE_DBRESULT();
	}

	// Security passed, and we have a list of all the items that must be changed, and the records are locked
	
	// Check if there is anything to do
	if(lObjectIds.empty()) {
		lpDatabase->Commit();
		goto exit;
	}

    strQuery += " WHERE properties.hierarchyid IN(";

    lHierarchyIDs.clear();
    for(iObjectid = lObjectIds.begin(); iObjectid != lObjectIds.end(); iObjectid++) {

        if(iObjectid != lObjectIds.begin())
            strQuery += ",";

        strQuery += stringify(iObjectid->first);
        lHierarchyIDs.push_back(iObjectid->first);
    }
    strQuery += ")";

	strQuery += " AND properties.tag = " + stringify(PROP_ID(PR_MESSAGE_FLAGS)) + "  AND properties.type = " + stringify(PROP_TYPE(PR_MESSAGE_FLAGS));

   	// Update the database
   	er = lpDatabase->DoUpdate(strQuery);
   	if(er != erSuccess)
    	goto exit;


	er = UpdateTProp(lpDatabase, PR_MESSAGE_FLAGS, ulParent, &lHierarchyIDs); // FIXME ulParent is not constant for all lHierarchyIDs
	if(er != erSuccess)
		goto exit;
    
    // Add changes to ICS
    for(iObjectid = lObjectIds.begin(); iObjectid != lObjectIds.end(); iObjectid++) {
        if( (ulFlagsRemove & MSGFLAG_READ) || (ulFlagsAdd & MSGFLAG_READ) ) {
            // Only save ICS change when the actual readflag has changed
            SOURCEKEY		sSourceKey;
            SOURCEKEY		sParentSourceKey;

			if(g_lpSessionManager->GetCacheManager()->GetObject(iObjectid->first, &ulParent, NULL, NULL) != erSuccess) {
			    continue;
			}

            GetSourceKey(iObjectid->first, &sSourceKey);
            GetSourceKey(ulParent, &sParentSourceKey);

            // Because we know that ulFlagsRemove && MSGFLAG_READ || ulFlagsAdd & MSGFLAG_READ and we assume
            // that they are never both TRUE, we can ignore ulFlagsRemove and just look at ulFlagsAdd for the new
            // readflag state
            AddChange(lpecSession, ulSyncId, sSourceKey, sParentSourceKey, ICS_MESSAGE_FLAG, ulFlagsAdd & MSGFLAG_READ);
        }
    }

    // Update counters, by counting the number of changes per folder
    for(iObjectid = lObjectIds.begin(); iObjectid != lObjectIds.end(); iObjectid++)
    {
        er = g_lpSessionManager->GetCacheManager()->GetObject(iObjectid->first, &ulParent, NULL, NULL);
        if(er != erSuccess)
        	goto exit;
        	
		mapParents.insert(std::pair<unsigned int, unsigned int>(ulParent, 0));
		
		if(ulFlagsAdd & MSGFLAG_READ)
			if((iObjectid->second & MSGFLAG_READ) == 0)
				mapParents[ulParent]--; // Decrease unread count
		if(ulFlagsRemove & MSGFLAG_READ)
			if((iObjectid->second & MSGFLAG_READ) == MSGFLAG_READ)
				mapParents[ulParent]++; // Increase unread count
	}
	
	for(iterParents = mapParents.begin(); iterParents != mapParents.end(); iterParents++) {
		if(iterParents->second != 0) {
			er = g_lpSessionManager->GetCacheManager()->GetParent(iterParents->first, &ulGrandParent);
			if(er != erSuccess)
				goto exit;
			er = UpdateFolderCount(lpDatabase, iterParents->first, PR_CONTENT_UNREAD, iterParents->second);
			if (er != erSuccess)
				goto exit;
		}
	}
	
	er = lpDatabase->Commit();
    if(er != erSuccess)
	    goto exit;

	// Now, update cache and send the notifications

	cObjectSize = lObjectIds.size();

    // Loop through the messages, updating each
    for(iObjectid = lObjectIds.begin(); iObjectid != lObjectIds.end(); iObjectid++)
    {
		// Remove the item from the cache 
        g_lpSessionManager->GetCacheManager()->UpdateCell(iObjectid->first, PR_MESSAGE_FLAGS, (ulFlagsAdd | ulFlagsRemove) & MSGFLAG_READ, ulFlagsAdd & MSGFLAG_READ);

        if(g_lpSessionManager->GetCacheManager()->GetObject(iObjectid->first, &ulParent, NULL, &ulFlagsNotify) != erSuccess) {
            ulParent = 0;
            ulFlagsNotify = 0;
		}

        // Update the message itself in tables and object notification
        g_lpSessionManager->NotificationModified(MAPI_MESSAGE, iObjectid->first, ulParent);

        if(ulParent &&  cObjectSize < EC_TABLE_CHANGE_THRESHOLD)
            g_lpSessionManager->UpdateTables(ECKeyTable::TABLE_ROW_MODIFY, ulFlagsNotify&MSGFLAG_NOTIFY_FLAGS, ulParent, iObjectid->first, MAPI_MESSAGE);
    }

    // Loop through all the parent folders of the objects, sending notifications for them
    for(iterParents = mapParents.begin(); iterParents != mapParents.end(); iterParents++) {
        // The parent has changed its PR_CONTENT_UNREAD
        g_lpSessionManager->GetCacheManager()->Update(fnevObjectModified, iterParents->first);
        g_lpSessionManager->NotificationModified(MAPI_FOLDER, iterParents->first);

        // The grand parent's table view of the parent has changed
        if(g_lpSessionManager->GetCacheManager()->GetObject(iterParents->first, &ulGrandParent, NULL, &ulFlagsNotify) == erSuccess)
            g_lpSessionManager->UpdateTables(ECKeyTable::TABLE_ROW_MODIFY, ulFlagsNotify&MSGFLAG_NOTIFY_FLAGS, ulGrandParent, iterParents->first, MAPI_FOLDER);

        if(cObjectSize >= EC_TABLE_CHANGE_THRESHOLD)
            g_lpSessionManager->UpdateTables(ECKeyTable::TABLE_CHANGE, ulFlagsNotify&MSGFLAG_NOTIFY_FLAGS, iterParents->first, 0, MAPI_MESSAGE);
    }

exit:
    ROLLBACK_ON_ERROR();
    FREE_DBRESULT();
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(createUser, lpsUserSetResponse->er, struct user *lpsUser, struct setUserResponse *lpsUserSetResponse)
{
	unsigned int		ulUserId = 0;
	objectdetails_t		details(ACTIVE_USER); // should this function also be able to createContact?

	if (lpsUser == NULL || lpsUser->lpszUsername == NULL || lpsUser->lpszFullName == NULL || lpsUser->lpszMailAddress == NULL ||
		(lpsUser->lpszPassword == NULL && lpsUser->ulIsNonActive == 0)) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	if (!bSupportUnicode) {
		er = FixUserEncoding(soap, stringCompat, In, lpsUser);
		if (er != erSuccess)
			goto exit;
	}

	er = CopyUserDetailsFromSoap(lpsUser, NULL, &details, soap);
	if (er != erSuccess)
		goto exit;

	er = lpecSession->GetUserManagement()->UpdateUserDetailsFromClient(&details);
	if (er != erSuccess)
		goto exit;

	// Check permission
	er = lpecSession->GetSecurity()->IsAdminOverUserObject(details.GetPropInt(OB_PROP_I_COMPANYID));
	if(er != erSuccess)
		goto exit;

    // Create user and sync
	er = lpecSession->GetUserManagement()->CreateObjectAndSync(details, &ulUserId);
	if(er != erSuccess)
	    goto exit;

	er = GetABEntryID(ulUserId, soap, &lpsUserSetResponse->sUserId);
	if (er != erSuccess)
		goto exit;

	lpsUserSetResponse->ulUserId = ulUserId;

exit:
    ;
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(setUser, *result, struct user *lpsUser, unsigned int *result)
{
	objectdetails_t		details;
	objectdetails_t		oldDetails;
	unsigned int		ulUserId = 0;
	objectid_t			sExternId;

	if(lpsUser == NULL) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	if (!bSupportUnicode) {
		er = FixUserEncoding(soap, stringCompat, In, lpsUser);
		if (er != erSuccess)
			goto exit;
	}

	if (lpsUser->sUserId.__size > 0 && lpsUser->sUserId.__ptr != NULL)
	{
		er = GetLocalId(lpsUser->sUserId, lpsUser->ulUserId, &ulUserId, &sExternId);
		if (er != erSuccess) 
			goto exit;
	}
	else
		ulUserId = lpsUser->ulUserId;

	if(ulUserId) {
		er = lpecSession->GetUserManagement()->GetObjectDetails(ulUserId, &oldDetails);
		if (er != erSuccess)
			goto exit;
	}

	// Check security
	// @todo add check on anonymous (mv)properties
	if (lpecSession->GetSecurity()->IsAdminOverUserObject(ulUserId) == erSuccess) {
		// admins can update anything of a user
		// FIXME: prevent the user from removing admin rights from itself?
		er = erSuccess;
	} else if (lpecSession->GetSecurity()->GetUserId() == ulUserId) {
		// you're only allowed to set your password, force the lpsUser struct to only contain that update
		if (lpsUser->lpszUsername && oldDetails.GetPropString(OB_PROP_S_LOGIN) != lpsUser->lpszUsername) {
			g_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_FATAL, "Disallowing user %s to update its username to %s",
												 oldDetails.GetPropString(OB_PROP_S_LOGIN).c_str(), lpsUser->lpszUsername);
			lpsUser->lpszUsername = NULL;
		}

		// leave lpszPassword

		if (lpsUser->lpszMailAddress && oldDetails.GetPropString(OB_PROP_S_EMAIL) != lpsUser->lpszMailAddress) {
			g_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_FATAL, "Disallowing user %s to update its mail address to %s",
												 oldDetails.GetPropString(OB_PROP_S_LOGIN).c_str(), lpsUser->lpszMailAddress);
			lpsUser->lpszMailAddress = NULL;
		}

		if (lpsUser->lpszFullName && oldDetails.GetPropString(OB_PROP_S_FULLNAME) != lpsUser->lpszFullName) {
			g_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_FATAL, "Disallowing user %s to update its fullname to %s",
												 oldDetails.GetPropString(OB_PROP_S_LOGIN).c_str(), lpsUser->lpszFullName);
			lpsUser->lpszFullName = NULL;
		}

		if (lpsUser->lpszServername && oldDetails.GetPropString(OB_PROP_S_SERVERNAME) != lpsUser->lpszServername) {
			g_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_FATAL, "Disallowing user %s to update its home server to %s",
												 oldDetails.GetPropString(OB_PROP_S_LOGIN).c_str(), lpsUser->lpszServername);
			lpsUser->lpszServername = NULL;
		}

		// FIXME: check OB_PROP_B_NONACTIVE too? NOTE: ulIsNonActive is now ignored.
		if (lpsUser->ulObjClass != (ULONG)-1 && oldDetails.GetClass() != (objectclass_t)lpsUser->ulObjClass) {
			g_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_FATAL, "Disallowing user %s to update its active flag to %d",
												 oldDetails.GetPropString(OB_PROP_S_LOGIN).c_str(), lpsUser->ulObjClass);
			lpsUser->ulObjClass = (ULONG)-1;
		}

		if (lpsUser->ulIsAdmin != (ULONG)-1 && oldDetails.GetPropInt(OB_PROP_I_ADMINLEVEL) != lpsUser->ulIsAdmin) {
			g_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_FATAL, "Disallowing user %s to update its admin flag to %d",
												 oldDetails.GetPropString(OB_PROP_S_LOGIN).c_str(), lpsUser->ulIsAdmin);
			lpsUser->ulIsAdmin = (ULONG)-1;
		}
	} else {
		// you cannot set any details if you're not an admin or not yourself
		g_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_FATAL, "Disallowing user %s to update details of user %s",
											 oldDetails.GetPropString(OB_PROP_S_LOGIN).c_str(), lpsUser->lpszUsername);
		er = ZARAFA_E_NO_ACCESS;
		goto exit;
	}

	details = objectdetails_t(ACTIVE_USER);

	// construct new details
	er = CopyUserDetailsFromSoap(lpsUser, &sExternId.id, &details, soap);
	if (er != erSuccess)
		goto exit;

	er = lpecSession->GetUserManagement()->UpdateUserDetailsFromClient(&details);
	if (er != erSuccess)
		goto exit;

	er = lpecSession->GetUserManagement()->SetObjectDetailsAndSync(ulUserId, details, NULL);

exit:
    ;
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(getUser, lpsGetUserResponse->er, unsigned int ulUserId, entryId sUserId, struct getUserResponse *lpsGetUserResponse)
{
	objectdetails_t	details;
	entryId			sTmpUserId = {0};

	er = GetLocalId(sUserId, ulUserId, &ulUserId, NULL);
	if (er != erSuccess)
		goto exit;

	/* Check if we are able to view the returned userobject */
	er = lpecSession->GetSecurity()->IsUserObjectVisible(ulUserId);
	if (er != erSuccess)
		goto exit;

	lpsGetUserResponse->lpsUser = s_alloc<user>(soap);
	memset(lpsGetUserResponse->lpsUser, 0, sizeof(struct user));

	if(ulUserId == 0)
        ulUserId = lpecSession->GetSecurity()->GetUserId();

    er = lpecSession->GetUserManagement()->GetObjectDetails(ulUserId, &details);
    if(er != erSuccess)
        goto exit;

	if (OBJECTCLASS_TYPE(details.GetClass()) != OBJECTTYPE_MAILUSER) {
		er = ZARAFA_E_NOT_FOUND;
		goto exit;
	}

	er = GetABEntryID(ulUserId, soap, &sTmpUserId);
	if (er == erSuccess)
		er = CopyUserDetailsToSoap(ulUserId, &sTmpUserId, details, soap, lpsGetUserResponse->lpsUser);
	if (er != erSuccess)
		goto exit;

	// 6.40.0 stores the object class in the IsNonActive field
	if (lpecSession->ClientVersion() == ZARAFA_VERSION_6_40_0)
		lpsGetUserResponse->lpsUser->ulIsNonActive = lpsGetUserResponse->lpsUser->ulObjClass;

	if (!bSupportUnicode) {
		er = FixUserEncoding(soap, stringCompat, Out, lpsGetUserResponse->lpsUser);
		if (er != erSuccess)
			goto exit;
	}

exit:;
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(getUserList, lpsUserList->er, unsigned int ulCompanyId, entryId sCompanyId, struct userListResponse *lpsUserList)
{
	std::list<localobjectdetails_t> *lpUsers = NULL;
	std::list<localobjectdetails_t>::iterator iterUsers;
	entryId		sUserEid = {0};

	er = GetLocalId(sCompanyId, ulCompanyId, &ulCompanyId, NULL);
	if (er != erSuccess)
		goto exit;

	/* Input check, if ulCompanyId is 0, we want the user's company,
	 * otherwise we must check if the requested company is visible for the user. */
	if (ulCompanyId == 0) {
		er = lpecSession->GetSecurity()->GetUserCompany(&ulCompanyId);
		if (er != erSuccess)
			goto exit;
	} else {
		er = lpecSession->GetSecurity()->IsUserObjectVisible(ulCompanyId);
		if (er != erSuccess)
			goto exit;
	}

	er = lpecSession->GetUserManagement()->GetCompanyObjectListAndSync(OBJECTCLASS_USER, ulCompanyId, &lpUsers, 0);
	if(er != erSuccess)
		goto exit;

    lpsUserList->sUserArray.__size = 0;
    lpsUserList->sUserArray.__ptr = s_alloc<user>(soap, lpUsers->size());

    for(iterUsers = lpUsers->begin(); iterUsers != lpUsers->end(); iterUsers++)
	{
		if (OBJECTCLASS_TYPE(iterUsers->GetClass()) != OBJECTTYPE_MAILUSER ||
			iterUsers->GetClass() == NONACTIVE_CONTACT)
				continue;

		er = GetABEntryID(iterUsers->ulId, soap, &sUserEid);
		if (er != erSuccess)
			goto exit;

		er = CopyUserDetailsToSoap(iterUsers->ulId, &sUserEid, *iterUsers, soap, &lpsUserList->sUserArray.__ptr[lpsUserList->sUserArray.__size]);
		if (er != erSuccess)
			goto exit;

		// 6.40.0 stores the object class in the IsNonActive field
		if (lpecSession->ClientVersion() == ZARAFA_VERSION_6_40_0)
			lpsUserList->sUserArray.__ptr[lpsUserList->sUserArray.__size].ulIsNonActive = lpsUserList->sUserArray.__ptr[lpsUserList->sUserArray.__size].ulObjClass;

		if (!bSupportUnicode) {
			er = FixUserEncoding(soap, stringCompat, Out, lpsUserList->sUserArray.__ptr + lpsUserList->sUserArray.__size);
			if (er != erSuccess)
				goto exit;
		}

        lpsUserList->sUserArray.__size++;
    }

exit:
    if(lpUsers)
        delete lpUsers;
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(getSendAsList, lpsUserList->er, unsigned int ulUserId, entryId sUserId, struct userListResponse *lpsUserList)
{
	objectdetails_t userDetails, senderDetails;
	list<unsigned int> userIds;
	list<unsigned int>::iterator iterUserIds;
	entryId sSenderEid = {0};

	er = GetLocalId(sUserId, ulUserId, &ulUserId, NULL);
	if (er != erSuccess)
		goto exit;

	er = lpecSession->GetSecurity()->IsUserObjectVisible(ulUserId);
	if (er != erSuccess)
		goto exit;

	er = lpecSession->GetUserManagement()->GetObjectDetails(ulUserId, &userDetails);
	if (er != erSuccess)
		goto exit;

	userIds = userDetails.GetPropListInt(OB_PROP_LI_SENDAS);

	lpsUserList->sUserArray.__size = 0;
	lpsUserList->sUserArray.__ptr = s_alloc<user>(soap, userIds.size());

	for (iterUserIds = userIds.begin(); iterUserIds != userIds.end(); iterUserIds++) {
		if (lpecSession->GetSecurity()->IsUserObjectVisible(*iterUserIds) != erSuccess)
			continue;

		er = lpecSession->GetUserManagement()->GetObjectDetails(*iterUserIds, &senderDetails);
		if (er == ZARAFA_E_NOT_FOUND)
			continue;
		if (er != erSuccess)
			goto exit;

		er = GetABEntryID(*iterUserIds, soap, &sSenderEid);
		if (er != erSuccess)
			goto exit;

		er = CopyUserDetailsToSoap(*iterUserIds, &sSenderEid, senderDetails, soap, &lpsUserList->sUserArray.__ptr[lpsUserList->sUserArray.__size]);
		if (er != erSuccess)
			goto exit;

		// 6.40.0 stores the object class in the IsNonActive field
		if (lpecSession->ClientVersion() == ZARAFA_VERSION_6_40_0)
			lpsUserList->sUserArray.__ptr[lpsUserList->sUserArray.__size].ulIsNonActive = lpsUserList->sUserArray.__ptr[lpsUserList->sUserArray.__size].ulObjClass;

		if (!bSupportUnicode) {
			er = FixUserEncoding(soap, stringCompat, Out, lpsUserList->sUserArray.__ptr + lpsUserList->sUserArray.__size);
			if (er != erSuccess)
				goto exit;
		}

		lpsUserList->sUserArray.__size++;
	}
	er = erSuccess;

exit:
	;
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(addSendAsUser, *result, unsigned int ulUserId, entryId sUserId, unsigned int ulSenderId, entryId sSenderId, unsigned int *result)
{

	er = GetLocalId(sUserId, ulUserId, &ulUserId, NULL);
	if (er != erSuccess)
		goto exit;

	er = GetLocalId(sSenderId, ulSenderId, &ulSenderId, NULL);
	if (er != erSuccess)
		goto exit;

	if (ulUserId == ulSenderId) {
		er = ZARAFA_E_COLLISION;
		goto exit;
	}

	// Check security, only admins can set sendas users, not the user itself
	if(lpecSession->GetSecurity()->IsAdminOverUserObject(ulUserId) != erSuccess) {
		er = ZARAFA_E_NO_ACCESS;
		goto exit;
	}

	// needed?
	er = lpecSession->GetSecurity()->IsUserObjectVisible(ulUserId);
	if (er != erSuccess)
		goto exit;

	er = lpecSession->GetUserManagement()->AddSubObjectToObjectAndSync(OBJECTRELATION_USER_SENDAS, ulUserId, ulSenderId);
	
exit:
	;
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(delSendAsUser, *result, unsigned int ulUserId, entryId sUserId, unsigned int ulSenderId, entryId sSenderId, unsigned int *result)
{

	er = GetLocalId(sUserId, ulUserId, &ulUserId, NULL);
	if (er != erSuccess)
		goto exit;

	er = GetLocalId(sSenderId, ulSenderId, &ulSenderId, NULL);
	if (er != erSuccess)
		goto exit;

	if (ulUserId == ulSenderId) {
		er = ZARAFA_E_COLLISION;
		goto exit;
	}

	// Check security, only admins can set sendas users, not the user itself
	if(lpecSession->GetSecurity()->IsAdminOverUserObject(ulUserId) != erSuccess) {
		er = ZARAFA_E_NO_ACCESS;
		goto exit;
	}

	// needed ?
	er = lpecSession->GetSecurity()->IsUserObjectVisible(ulUserId);
	if (er != erSuccess)
		goto exit;

	er = lpecSession->GetUserManagement()->DeleteSubObjectFromObjectAndSync(OBJECTRELATION_USER_SENDAS, ulUserId, ulSenderId);

exit:
	;
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(purgeSoftDelete, *result, unsigned int ulDays, unsigned int *result)
{
    unsigned int	ulFolders = 0;
    unsigned int	ulMessages = 0;
	unsigned int	ulStores = 0;

    // Only system-admins may run this
    if(lpecSession->GetSecurity()->GetAdminLevel() < ADMIN_LEVEL_SYSADMIN) {
        er = ZARAFA_E_NO_ACCESS;
        goto exit;
    }

    g_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_INFO, "Start forced softdelete clean up");

    er = PurgeSoftDelete(lpecSession, ulDays * 24 * 60 * 60, &ulMessages, &ulFolders, &ulStores, NULL);

    if (er == erSuccess)
		g_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_INFO, "Softdelete done: removed stores: %d, removed folders: %d, removed messages: %d", ulStores, ulFolders, ulMessages);
    else if (er == ZARAFA_E_BUSY)
		g_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_INFO, "Softdelete already running");
	else
        g_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_INFO, "Softdelete failed: removed stores: %d, removed folders: %d, removed messages: %d", ulStores, ulFolders, ulMessages);

exit:
    ;
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(purgeCache, *result, unsigned int ulFlags, unsigned int *result)
{
    if(lpecSession->GetSecurity()->GetAdminLevel() < ADMIN_LEVEL_SYSADMIN) {
        er = ZARAFA_E_NO_ACCESS;
        goto exit;
    }

    er = g_lpSessionManager->GetCacheManager()->PurgeCache(ulFlags);

	g_lpStatsCollector->SetTime(SCN_SERVER_LAST_CACHECLEARED, time(NULL));
exit:
    ;
}
SOAP_ENTRY_END()

//Create a store
// Info: Userid can also be a group id ('everyone' for public store)
SOAP_ENTRY_START(createStore, *result, unsigned int ulStoreType, unsigned int ulUserId, entryId sUserId, entryId sStoreId, entryId sRootId, unsigned int ulFlags, unsigned int *result)
{
	unsigned int	ulStoreId = 0;
	unsigned int	ulRootMapId = 0;
	objectdetails_t userDetails;
	unsigned int	ulCompanyId = 0;
	bool			bHasLocalStore = false;

	SOURCEKEY		sSourceKey;
	GUID			guidStore;
	time_t			now;
	unsigned int	timeProps[] = { PR_LAST_MODIFICATION_TIME, PR_CREATION_TIME };
	struct propVal 	sProp;
	struct hiloLong sHilo;

	struct rightsArray srightsArray;
	USE_DATABASE();

	memset(&srightsArray, 0 , sizeof(srightsArray));

	if((unsigned int)sStoreId.__size < sizeof(EID)) {
	    er = ZARAFA_E_INVALID_PARAMETER;
	    goto exit;
    }
    
    // Normalize flags
	((EID *)sStoreId.__ptr)->usFlags = 0;

	er = GetLocalId(sUserId, ulUserId, &ulUserId, NULL);
	if (er != erSuccess)
		goto exit;

	er = CheckUserStore(lpecSession, ulUserId, ulStoreType, &bHasLocalStore);
	if (er != erSuccess)
		goto exit;

	if (!bHasLocalStore && (ulFlags & EC_OVERRIDE_HOMESERVER) == 0) {
		g_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_FATAL, "Create store requested, but store is not on this server, or server property not set for object %d", ulUserId);
		er = ZARAFA_E_NOT_FOUND;
		goto exit;
	}

	g_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_FATAL, "Started to create store (userid=%d, type=%d)", ulUserId, ulStoreType);

	er = lpDatabase->Begin();
	if (er != erSuccess)
		goto exit;

	// Check permission
	er = lpecSession->GetSecurity()->IsAdminOverUserObject(ulUserId);
	if (er != erSuccess)
		goto exit;

	// Get object details, and resolve company
	er = lpecSession->GetUserManagement()->GetObjectDetails(ulUserId, &userDetails);
	if (er != erSuccess)
		goto exit;

	if (lpecSession->GetSessionManager()->IsHostedSupported())
		ulCompanyId = userDetails.GetPropInt(OB_PROP_I_COMPANYID);

	// Validate store entryid
	if(ValidateZarafaEntryId(sStoreId.__size, sStoreId.__ptr, MAPI_STORE) == false) {
		er = ZARAFA_E_INVALID_ENTRYID;
		goto exit;
	}

	// Validate root entryid
	if(ValidateZarafaEntryId(sRootId.__size, sRootId.__ptr, MAPI_FOLDER) == false) {
		er = ZARAFA_E_INVALID_ENTRYID;
		goto exit;
	}

	er = GetStoreGuidFromEntryId(sStoreId.__size, sStoreId.__ptr, &guidStore);
	if(er != erSuccess)
		goto exit;

	// Check if there's already a store for the user or group
	strQuery = "SELECT 0 FROM stores WHERE (type="+stringify(ulStoreType)+" AND user_id="+stringify(ulUserId)+") OR guid="+lpDatabase->EscapeBinary((unsigned char*)&guidStore , sizeof(GUID));
	er = lpDatabase->DoSelect(strQuery, &lpDBResult);
	if(er != erSuccess)
		goto exit;

	if(lpDatabase->GetNumRows(lpDBResult) > 0) {
		er = ZARAFA_E_COLLISION;
		goto exit;
	}

	FREE_DBRESULT();

	// Create Toplevel of the store
	strQuery = "INSERT INTO hierarchy(parent, type, owner) VALUES(NULL, "+stringify(MAPI_STORE)+", "+ stringify(ulUserId)+")";
	er = lpDatabase->DoInsert(strQuery, &ulStoreId);
	if(er != erSuccess)
		goto exit;

	// Create the rootfolder of a store
	strQuery = "INSERT INTO hierarchy(parent, type, owner) VALUES("+stringify(ulStoreId)+", "+stringify(MAPI_FOLDER)+ ", "+ stringify(ulUserId)+")";
	er = lpDatabase->DoInsert(strQuery, &ulRootMapId);
	if(er != erSuccess)
		goto exit;

	//Init storesize
	UpdateObjectSize(lpDatabase, ulStoreId, MAPI_STORE, UPDATE_SET, 0);


	// Add SourceKey

	er = lpecSession->GetNewSourceKey(&sSourceKey);
	if(er != erSuccess)
		goto exit;
		
    er = RemoveStaleIndexedProp(lpDatabase, PR_SOURCE_KEY, sSourceKey, sSourceKey.size());
    if(er != erSuccess)
        goto exit;
        
	strQuery = "INSERT INTO indexedproperties(hierarchyid,tag,val_binary) VALUES(" + stringify(ulRootMapId) + "," + stringify(PROP_ID(PR_SOURCE_KEY)) + "," + lpDatabase->EscapeBinary(sSourceKey, sSourceKey.size()) + ")";
	er = lpDatabase->DoInsert(strQuery);
	if(er != erSuccess)
		goto exit;

	// Add store entryid: 0x0FFF = PR_ENTRYID
    er = RemoveStaleIndexedProp(lpDatabase, PR_ENTRYID, sStoreId.__ptr, sStoreId.__size);
    if(er != erSuccess)
        goto exit;
        
	strQuery = "INSERT INTO indexedproperties (hierarchyid,tag,val_binary) VALUES("+stringify(ulStoreId)+", 0x0FFF, "+lpDatabase->EscapeBinary(sStoreId.__ptr, sStoreId.__size)+")";
	er = lpDatabase->DoInsert(strQuery);
	if(er != erSuccess)
		goto exit;

	// Add rootfolder entryid: 0x0FFF = PR_ENTRYID
    er = RemoveStaleIndexedProp(lpDatabase, PR_ENTRYID, sRootId.__ptr, sRootId.__size);
    if(er != erSuccess)
        goto exit;
        
	strQuery = "INSERT INTO indexedproperties (hierarchyid,tag,val_binary) VALUES("+stringify(ulRootMapId)+", 0x0FFF, "+lpDatabase->EscapeBinary(sRootId.__ptr, sRootId.__size)+")";
	er = lpDatabase->DoInsert(strQuery);
	if(er != erSuccess)
		goto exit;

	// Add rootfolder type: 0x3601 = FOLDER_ROOT (= 0)
	strQuery = "INSERT INTO properties (tag,type,hierarchyid,val_ulong) VALUES(0x3601, 0x0003, "+stringify(ulRootMapId)+", 0)";
	er = lpDatabase->DoInsert(strQuery);
	if(er != erSuccess)
		goto exit;
		
    now = time(NULL);
    for(unsigned int i = 0; i < arraySize(timeProps); i++) {
        sProp.ulPropTag = timeProps[i];
        sProp.__union = SOAP_UNION_propValData_hilo;
        sProp.Value.hilo = &sHilo;
        
        UnixTimeToFileTime(now, &sProp.Value.hilo->hi, &sProp.Value.hilo->lo);
        
        WriteProp(lpDatabase, ulStoreId, 0, &sProp);
        if(er != erSuccess)
            goto exit;
            
        WriteProp(lpDatabase, ulRootMapId, 0, &sProp);
        if(er != erSuccess)
            goto exit;
    }

	// Couple store with user
	strQuery = "INSERT INTO stores(hierarchy_id, user_id, type, user_name, company, guid) VALUES(" +
		stringify(ulStoreId) + ", " +
		stringify(ulUserId) + ", " +
		stringify(ulStoreType) + ", " +
		"'" + lpDatabase->Escape(userDetails.GetPropString(OB_PROP_S_LOGIN)) + "', " +
		stringify(ulCompanyId) + ", " +
		lpDatabase->EscapeBinary((unsigned char*)&guidStore , sizeof(GUID))+")";
	er = lpDatabase->DoInsert(strQuery);
	if(er != erSuccess)
		goto exit;


	// Set ACL's on public store
	if(ulStoreType == ECSTORE_TYPE_PUBLIC) {
		// ulUserId == a group
		// ulUserId 1 = group everyone

		srightsArray.__ptr = new rights[1];
		srightsArray.__ptr[0].ulRights = ecRightsDefaultPublic;
		srightsArray.__ptr[0].ulUserid = ulUserId;
		srightsArray.__ptr[0].ulState = RIGHT_NEW|RIGHT_AUTOUPDATE_DENIED;
		srightsArray.__ptr[0].ulType = ACCESS_TYPE_GRANT;
		memset(&srightsArray.__ptr[0].sUserId, 0, sizeof(entryId));

		srightsArray.__size = 1;
		er = lpecSession->GetSecurity()->SetRights(ulStoreId, &srightsArray);
		if(er != erSuccess)
			goto exit;
	}

	er = lpDatabase->Commit();
	if (er != erSuccess)
		goto exit;

	g_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_FATAL, "Finished create store (userid=%d, storeid=%d, type=%d)", ulUserId, ulStoreId, ulStoreType);

exit:
	if(er == ZARAFA_E_NO_ACCESS)
		g_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_FATAL, "Failed to create store access denied");
	else if(er != erSuccess)
		g_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_FATAL, "Failed to create store (id=%d), errorcode=0x%08X", ulUserId, er);

	if(srightsArray.__size > 0)
		delete [] srightsArray.__ptr;

    FREE_DBRESULT();
	ROLLBACK_ON_ERROR();
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(createGroup, lpsSetGroupResponse->er, struct group *lpsGroup, struct setGroupResponse *lpsSetGroupResponse)
{
	unsigned int			ulGroupId = 0;
	objectdetails_t			details(DISTLIST_SECURITY); // DB plugin wants to be able to set permissions on groups

	if (lpsGroup == NULL || lpsGroup->lpszGroupname == NULL || lpsGroup->lpszFullname == NULL) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	if (!bSupportUnicode) {
		er = FixGroupEncoding(soap, stringCompat, In, lpsGroup);
		if (er != erSuccess)
			goto exit;
	}

	er = CopyGroupDetailsFromSoap(lpsGroup, NULL, &details, soap);
	if (er != erSuccess)
		goto exit;

	er = lpecSession->GetUserManagement()->UpdateUserDetailsFromClient(&details);
	if (er != erSuccess)
		goto exit;

	// Check permission
	er = lpecSession->GetSecurity()->IsAdminOverUserObject(details.GetPropInt(OB_PROP_I_COMPANYID));
	if(er != erSuccess)
		goto exit;

    er = lpecSession->GetUserManagement()->CreateObjectAndSync(details, &ulGroupId);
    if(er != erSuccess)
        goto exit;

	er = GetABEntryID(ulGroupId, soap, &lpsSetGroupResponse->sGroupId);
	if (er != erSuccess)
		goto exit;

	lpsSetGroupResponse->ulGroupId = ulGroupId;

exit:
    ;
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(setGroup, *result, struct group *lpsGroup, unsigned int *result)
{
	objectdetails_t details;
	unsigned int	ulGroupId = 0;
	objectid_t		sExternId;
	
	if(lpsGroup == NULL) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	if (!bSupportUnicode) {
		er = FixGroupEncoding(soap, stringCompat, In, lpsGroup);
		if (er != erSuccess)
			goto exit;
	}

	if (lpsGroup->sGroupId.__size > 0 && lpsGroup->sGroupId.__ptr != NULL)
	{
		er = GetLocalId(lpsGroup->sGroupId, lpsGroup->ulGroupId, &ulGroupId, &sExternId);
		if (er != erSuccess) 
			goto exit;
	}
	else
		ulGroupId = lpsGroup->ulGroupId;

	// Check permission
	er = lpecSession->GetSecurity()->IsAdminOverUserObject(ulGroupId);
	if(er != erSuccess)
		goto exit;

	details = objectdetails_t(DISTLIST_GROUP);

	er = CopyGroupDetailsFromSoap(lpsGroup, &sExternId.id, &details, soap);
	if (er != erSuccess)
		goto exit;

	er = lpecSession->GetUserManagement()->UpdateUserDetailsFromClient(&details);
	if (er != erSuccess)
		goto exit;

	er = lpecSession->GetUserManagement()->SetObjectDetailsAndSync(ulGroupId, details, NULL);
	if(er != erSuccess)
	    goto exit;

exit:
    ;
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(getGroup, lpsResponse->er, unsigned int ulGroupId, entryId sGroupId, struct getGroupResponse *lpsResponse)
{
	objectdetails_t details;
	entryId			sTmpGroupId = {0};

	er = GetLocalId(sGroupId, ulGroupId, &ulGroupId, NULL);
	if (er != erSuccess)
		goto exit;

	/* Check if we are able to view the returned userobject */
	er = lpecSession->GetSecurity()->IsUserObjectVisible(ulGroupId);
	if (er != erSuccess)
		goto exit;

    er = lpecSession->GetUserManagement()->GetObjectDetails(ulGroupId, &details);
    if(er != erSuccess)
        goto exit;

	if (OBJECTCLASS_TYPE(details.GetClass()) != OBJECTTYPE_DISTLIST) {
		er = ZARAFA_E_NOT_FOUND;
		goto exit;
	}

	lpsResponse->lpsGroup = s_alloc<group>(soap);

	er = GetABEntryID(ulGroupId, soap, &sTmpGroupId);
	if (er == erSuccess)
		er = CopyGroupDetailsToSoap(ulGroupId, &sTmpGroupId, details, soap, lpsResponse->lpsGroup);
	if (er != erSuccess)
		goto exit;

	if (!bSupportUnicode) {
		er = FixGroupEncoding(soap, stringCompat, Out, lpsResponse->lpsGroup);
		if (er != erSuccess)
			goto exit;
	}

exit:
    ;
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(getGroupList, lpsGroupList->er, unsigned int ulCompanyId, entryId sCompanyId, struct groupListResponse *lpsGroupList)
{
	std::list<localobjectdetails_t> *lpGroups = NULL;
	std::list<localobjectdetails_t>::iterator iterGroups;

	entryId	sGroupEid = {0};

	er = GetLocalId(sCompanyId, ulCompanyId, &ulCompanyId, NULL);
	if (er != erSuccess)
		goto exit;

	/* Input check, if ulCompanyId is 0, we want the user's company,
	 * otherwise we must check if the requested company is visible for the user. */
	if (ulCompanyId == 0) {
		er = lpecSession->GetSecurity()->GetUserCompany(&ulCompanyId);
		if (er != erSuccess)
			goto exit;
	} else {
		er = lpecSession->GetSecurity()->IsUserObjectVisible(ulCompanyId);
		if (er != erSuccess)
			goto exit;
	}

	er = lpecSession->GetUserManagement()->GetCompanyObjectListAndSync(OBJECTCLASS_DISTLIST, ulCompanyId, &lpGroups, 0);
	if (er != erSuccess)
		goto exit;

	lpsGroupList->sGroupArray.__size = 0;
	lpsGroupList->sGroupArray.__ptr = s_alloc<group>(soap, lpGroups->size());
	for(iterGroups = lpGroups->begin(); iterGroups != lpGroups->end(); iterGroups++)
	{
		if (OBJECTCLASS_TYPE(iterGroups->GetClass()) != OBJECTTYPE_DISTLIST)
			continue;

		er = GetABEntryID(iterGroups->ulId, soap, &sGroupEid);
		if (er != erSuccess)
			goto exit;

		er = CopyGroupDetailsToSoap(iterGroups->ulId, &sGroupEid, *iterGroups, soap, &lpsGroupList->sGroupArray.__ptr[lpsGroupList->sGroupArray.__size]);
		if (er != erSuccess)
			goto exit;

		if (!bSupportUnicode) {
			er = FixGroupEncoding(soap, stringCompat, Out, lpsGroupList->sGroupArray.__ptr + lpsGroupList->sGroupArray.__size);
			if (er != erSuccess)
				goto exit;
		}

	    lpsGroupList->sGroupArray.__size++;
	}

exit:
    if (lpGroups)
        delete lpGroups;
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(groupDelete, *result, unsigned int ulGroupId, entryId sGroupId, unsigned int *result)
{
	er = GetLocalId(sGroupId, ulGroupId, &ulGroupId, NULL);
	if (er != erSuccess)
		goto exit;

	// Check permission
	er = lpecSession->GetSecurity()->IsAdminOverUserObject(ulGroupId);
	if(er != erSuccess)
		goto exit;

    er = lpecSession->GetUserManagement()->DeleteObjectAndSync(ulGroupId);

exit:
    ;
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(resolveUsername, lpsResponse->er, char *lpszUsername, struct resolveUserResponse *lpsResponse)
{
	unsigned int		ulUserId = 0;

	if(lpszUsername == NULL) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	er = lpecSession->GetUserManagement()->ResolveObjectAndSync(OBJECTCLASS_USER, STRIN_FIX(lpszUsername), &ulUserId);
    if(er != erSuccess)
        goto exit;

	/* Check if we are able to view the returned userobject */
	er = lpecSession->GetSecurity()->IsUserObjectVisible(ulUserId);
	if (er != erSuccess)
		goto exit;
		
	er = GetABEntryID(ulUserId, soap, &lpsResponse->sUserId);
	if (er != erSuccess)
		goto exit;

	lpsResponse->ulUserId = ulUserId;

exit:
    ;
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(resolveGroupname, lpsResponse->er, char *lpszGroupname, struct resolveGroupResponse *lpsResponse)
{
	unsigned int	ulGroupId = 0;

	if(lpszGroupname == NULL) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

    er = lpecSession->GetUserManagement()->ResolveObjectAndSync(OBJECTCLASS_DISTLIST, STRIN_FIX(lpszGroupname), &ulGroupId);
    if(er != erSuccess)
        goto exit;

	/* Check if we are able to view the returned userobject */
	er = lpecSession->GetSecurity()->IsUserObjectVisible(ulGroupId);
	if (er != erSuccess)
		goto exit;

	er = GetABEntryID(ulGroupId, soap, &lpsResponse->sGroupId);
	if (er != erSuccess)
		goto exit;

	lpsResponse->ulGroupId = ulGroupId;

exit:
    ;
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(deleteGroupUser, *result, unsigned int ulGroupId, entryId sGroupId, unsigned int ulUserId, entryId sUserId, unsigned int *result)
{
	er = GetLocalId(sGroupId, ulGroupId, &ulGroupId, NULL);
	if (er != erSuccess)
		goto exit;

	// Check permission
	er = lpecSession->GetSecurity()->IsAdminOverUserObject(ulGroupId);
	if(er != erSuccess)
		goto exit;

	er = GetLocalId(sUserId, ulUserId, &ulUserId, NULL);
	if (er != erSuccess)
		goto exit;

    er = lpecSession->GetUserManagement()->DeleteSubObjectFromObjectAndSync(OBJECTRELATION_GROUP_MEMBER, ulGroupId, ulUserId);

exit:
    ;
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(addGroupUser, *result, unsigned int ulGroupId, entryId sGroupId, unsigned int ulUserId, entryId sUserId, unsigned int *result)
{
	er = GetLocalId(sGroupId, ulGroupId, &ulGroupId, NULL);
	if (er != erSuccess)
		goto exit;

	// Check permission
	er = lpecSession->GetSecurity()->IsAdminOverUserObject(ulGroupId);
	if(er != erSuccess)
		goto exit;

	er = GetLocalId(sUserId, ulUserId, &ulUserId, NULL);
	if (er != erSuccess)
		goto exit;

    er = lpecSession->GetUserManagement()->AddSubObjectToObjectAndSync(OBJECTRELATION_GROUP_MEMBER, ulGroupId, ulUserId);

exit:
    ;
}
SOAP_ENTRY_END()

// not only returns users of a group anymore
// TODO resolve group in group here on the fly?
SOAP_ENTRY_START(getUserListOfGroup, lpsUserList->er, unsigned int ulGroupId, entryId sGroupId, struct userListResponse *lpsUserList)
{
	std::list<localobjectdetails_t> *lpUsers = NULL;
	std::list<localobjectdetails_t>::iterator iterUsers;
	std::list<std::list<localobjectdetails_t> *>::iterator iterSources;
	entryId		sUserEid = {0};

	er = GetLocalId(sGroupId, ulGroupId, &ulGroupId, NULL);
	if (er != erSuccess)
		goto exit;

	er = lpecSession->GetSecurity()->IsUserObjectVisible(ulGroupId);
	if (er != erSuccess)
		goto exit;

    er = lpecSession->GetUserManagement()->GetSubObjectsOfObjectAndSync(OBJECTRELATION_GROUP_MEMBER, ulGroupId, &lpUsers);
    if(er != erSuccess)
        goto exit;

    lpsUserList->sUserArray.__size = 0;
    lpsUserList->sUserArray.__ptr = s_alloc<user>(soap, lpUsers->size());

	for(iterUsers = lpUsers->begin(); iterUsers != lpUsers->end(); iterUsers++) {
		if (lpecSession->GetSecurity()->IsUserObjectVisible(iterUsers->ulId) != erSuccess) {
			continue;
		}

		er = GetABEntryID(iterUsers->ulId, soap, &sUserEid);
		if (er != erSuccess)
			goto exit;

		// @todo Whoops, we can have group-in-groups. But since details of a group are almost identical to user details (eg. name, fullname, email)
		// this copy will succeed without any problems ... but it's definitly not correct.
		er = CopyUserDetailsToSoap(iterUsers->ulId, &sUserEid, *iterUsers, soap, &lpsUserList->sUserArray.__ptr[lpsUserList->sUserArray.__size]);
		if (er != erSuccess)
			goto exit;

		// 6.40.0 stores the object class in the IsNonActive field
		if (lpecSession->ClientVersion() == ZARAFA_VERSION_6_40_0)
			lpsUserList->sUserArray.__ptr[lpsUserList->sUserArray.__size].ulIsNonActive = lpsUserList->sUserArray.__ptr[lpsUserList->sUserArray.__size].ulObjClass;

		if (!bSupportUnicode) {
			er = FixUserEncoding(soap, stringCompat, Out, lpsUserList->sUserArray.__ptr + lpsUserList->sUserArray.__size);
			if (er != erSuccess)
				goto exit;
		}

		lpsUserList->sUserArray.__size++;
	}

exit:
    if(lpUsers)
        delete lpUsers;
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(getGroupListOfUser, lpsGroupList->er, unsigned int ulUserId, entryId sUserId, struct groupListResponse *lpsGroupList)
{
	std::list<localobjectdetails_t> *lpGroups = NULL;
	std::list<localobjectdetails_t>::iterator iterGroups;

	entryId sGroupEid = {0};

	er = GetLocalId(sUserId, ulUserId, &ulUserId, NULL);
	if (er != erSuccess)
		goto exit;

	er = lpecSession->GetSecurity()->IsUserObjectVisible(ulUserId);
	if (er != erSuccess)
		goto exit;

    er = lpecSession->GetUserManagement()->GetParentObjectsOfObjectAndSync(OBJECTRELATION_GROUP_MEMBER, ulUserId, &lpGroups);
    if(er != erSuccess)
        goto exit;

	lpsGroupList->sGroupArray.__size = 0;
	lpsGroupList->sGroupArray.__ptr = s_alloc<group>(soap, lpGroups->size());
	for(iterGroups = lpGroups->begin(); iterGroups != lpGroups->end(); iterGroups++) {
		if (lpecSession->GetSecurity()->IsUserObjectVisible(iterGroups->ulId) != erSuccess) {
			continue;
		}

		er = GetABEntryID(iterGroups->ulId, soap, &sGroupEid);
		if (er != erSuccess)
			goto exit;

		er = CopyGroupDetailsToSoap(iterGroups->ulId, &sGroupEid, *iterGroups, soap, &lpsGroupList->sGroupArray.__ptr[lpsGroupList->sGroupArray.__size]);
		if (er != erSuccess)
			goto exit;

		if (!bSupportUnicode) {
			er = FixGroupEncoding(soap, stringCompat, Out, lpsGroupList->sGroupArray.__ptr + lpsGroupList->sGroupArray.__size);
			if (er != erSuccess)
				goto exit;
		}

		lpsGroupList->sGroupArray.__size++;
	}

exit:
    if(lpGroups)
        delete lpGroups;
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(createCompany, lpsResponse->er, struct company *lpsCompany, struct setCompanyResponse *lpsResponse)
{
	unsigned int ulCompanyId = 0;
	objectdetails_t details(CONTAINER_COMPANY);

	if (lpsCompany == NULL) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	if (!g_lpSessionManager->IsHostedSupported()) {
		er = ZARAFA_E_NO_SUPPORT;
		goto exit;
	}

	// Check permission, only the system user is allowed to create or delete a company
	er = lpecSession->GetSecurity()->IsAdminOverUserObject(ZARAFA_UID_SYSTEM);
	if(er != erSuccess)
		goto exit;

	if (!bSupportUnicode) {
		er = FixCompanyEncoding(soap, stringCompat, In, lpsCompany);
		if (er != erSuccess)
			goto exit;
	}

	er = CopyCompanyDetailsFromSoap(lpsCompany, NULL, ZARAFA_UID_SYSTEM, &details, soap);
	if (er != erSuccess)
		goto exit;

	er = lpecSession->GetUserManagement()->UpdateUserDetailsFromClient(&details);
	if (er != erSuccess)
		goto exit;

	er = lpecSession->GetUserManagement()->CreateObjectAndSync(details, &ulCompanyId);
	if(er != erSuccess)
		goto exit;

	er = GetABEntryID(ulCompanyId, soap, &lpsResponse->sCompanyId);
	if (er != erSuccess)
		goto exit;

	lpsResponse->ulCompanyId = ulCompanyId;

exit:
;
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(deleteCompany, *result, unsigned int ulCompanyId, entryId sCompanyId, unsigned int *result)
{
	if (!g_lpSessionManager->IsHostedSupported()) {
		er = ZARAFA_E_NO_SUPPORT;
		goto exit;
	}

	// Check permission, only the system user is allowed to create or delete a company
	er = lpecSession->GetSecurity()->IsAdminOverUserObject(ZARAFA_UID_SYSTEM);
	if(er != erSuccess)
		goto exit;

	er = GetLocalId(sCompanyId, ulCompanyId, &ulCompanyId, NULL);
	if (er != erSuccess)
		goto exit;

	er = lpecSession->GetUserManagement()->DeleteObjectAndSync(ulCompanyId);

exit:
;
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(setCompany, *result, struct company *lpsCompany, unsigned int *result)
{
	objectdetails_t details;
	unsigned int	ulCompanyId = 0;
	unsigned int	ulAdministrator = 0;
	objectid_t		sExternId;
	
	if(lpsCompany == NULL) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	if (!g_lpSessionManager->IsHostedSupported()) {
		er = ZARAFA_E_NO_SUPPORT;
		goto exit;
	}

	if (!bSupportUnicode) {
		er = FixCompanyEncoding(soap, stringCompat, In, lpsCompany);
		if (er != erSuccess)
			goto exit;
	}

	if (lpsCompany->sCompanyId.__size > 0 && lpsCompany->sCompanyId.__ptr != NULL)
	{
		er = GetLocalId(lpsCompany->sCompanyId, lpsCompany->ulCompanyId, &ulCompanyId, &sExternId);
		if (er != erSuccess) 
			goto exit;
	}
	else
		ulCompanyId = lpsCompany->ulCompanyId;

	er = GetLocalId(lpsCompany->sAdministrator, lpsCompany->ulAdministrator, &ulAdministrator, NULL);
	if (er != erSuccess)
		goto exit;

	// Check permission
	er = lpecSession->GetSecurity()->IsAdminOverUserObject(ulCompanyId);
	if(er != erSuccess)
		goto exit;

	details = objectdetails_t(CONTAINER_COMPANY);

	er = CopyCompanyDetailsFromSoap(lpsCompany, &sExternId.id, ulAdministrator, &details, soap);
	if (er != erSuccess)
		goto exit;

	er = lpecSession->GetUserManagement()->UpdateUserDetailsFromClient(&details);
	if (er != erSuccess)
		goto exit;

	er = lpecSession->GetUserManagement()->SetObjectDetailsAndSync(ulCompanyId, details, NULL);
	if(er != erSuccess)
		goto exit;

exit:
;
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(getCompany, lpsResponse->er, unsigned int ulCompanyId, entryId sCompanyId, struct getCompanyResponse *lpsResponse)
{
	objectdetails_t details;
	unsigned int ulAdmin = 0;
	entryId sAdminEid = {0};
	entryId sTmpCompanyId = {0};

	if (!g_lpSessionManager->IsHostedSupported()) {
		er = ZARAFA_E_NO_SUPPORT;
		goto exit;
	}

	er = GetLocalId(sCompanyId, ulCompanyId, &ulCompanyId, NULL);
	if (er != erSuccess)
		goto exit;

	/* Input check, if ulCompanyId is 0, we want the user's company,
	 * otherwise we must check if the requested company is visible for the user. */
	if (ulCompanyId == 0) {
		er = lpecSession->GetSecurity()->GetUserCompany(&ulCompanyId);
		if (er != erSuccess)
			goto exit;
	} else {
		er = lpecSession->GetSecurity()->IsUserObjectVisible(ulCompanyId);
		if (er != erSuccess)
			goto exit;
	}

	er = lpecSession->GetUserManagement()->GetObjectDetails(ulCompanyId, &details);
	if(er != erSuccess)
		goto exit;

	if (details.GetClass() != CONTAINER_COMPANY) {
		er = ZARAFA_E_NOT_FOUND;
		goto exit;
	}

	ulAdmin = details.GetPropInt(OB_PROP_I_SYSADMIN);

	er = lpecSession->GetSecurity()->IsUserObjectVisible(ulAdmin);
	if (er != erSuccess)
		goto exit;

	er = GetABEntryID(ulAdmin, soap, &sAdminEid);
	if (er != erSuccess)
		goto exit;

	er = GetABEntryID(ulCompanyId, soap, &sTmpCompanyId);
	if (er != erSuccess)
		goto exit;

	lpsResponse->lpsCompany = s_alloc<company>(soap);
	memset(lpsResponse->lpsCompany, 0, sizeof(company));
	er = CopyCompanyDetailsToSoap(ulCompanyId, &sTmpCompanyId, ulAdmin, &sAdminEid, details, soap, lpsResponse->lpsCompany);
	if (er != erSuccess)
		goto exit;

	if (!bSupportUnicode) {
		er = FixCompanyEncoding(soap, stringCompat, Out, lpsResponse->lpsCompany);
		if (er != erSuccess)
			goto exit;
	}

exit:
;
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(resolveCompanyname, lpsResponse->er, char *lpszCompanyname, struct resolveCompanyResponse *lpsResponse)
{
	unsigned int ulCompanyId = 0;

	if (!g_lpSessionManager->IsHostedSupported()) {
		er = ZARAFA_E_NO_SUPPORT;
		goto exit;
	}

	if(lpszCompanyname == NULL) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	er = lpecSession->GetUserManagement()->ResolveObjectAndSync(CONTAINER_COMPANY, STRIN_FIX(lpszCompanyname), &ulCompanyId);
	if(er != erSuccess)
		goto exit;

	/* Check if we are able to view the returned userobject */
	er = lpecSession->GetSecurity()->IsUserObjectVisible(ulCompanyId);
	if (er != erSuccess)
		goto exit;

	er = GetABEntryID(ulCompanyId, soap, &lpsResponse->sCompanyId);
	if (er != erSuccess)
		goto exit;
	
	lpsResponse->ulCompanyId = ulCompanyId;

exit:
;
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(getCompanyList, lpsCompanyList->er, struct companyListResponse *lpsCompanyList)
{
	unsigned int	ulAdmin = 0;
	entryId			sCompanyEid = {0};
	entryId			sAdminEid = {0};
	std::list<localobjectdetails_t> *lpCompanies = NULL;
	std::list<localobjectdetails_t>::iterator iterCompanies;

	if (!g_lpSessionManager->IsHostedSupported()) {
		er = ZARAFA_E_NO_SUPPORT;
		goto exit;
	}

	er = lpecSession->GetSecurity()->GetViewableCompanyIds(0, &lpCompanies);
	if(er != erSuccess)
		goto exit;

	lpsCompanyList->sCompanyArray.__size = 0;
	lpsCompanyList->sCompanyArray.__ptr = s_alloc<company>(soap, lpCompanies->size());
	for(iterCompanies = lpCompanies->begin(); iterCompanies != lpCompanies->end(); iterCompanies++) {
		ulAdmin = iterCompanies->GetPropInt(OB_PROP_I_SYSADMIN);

		er = lpecSession->GetSecurity()->IsUserObjectVisible(ulAdmin);
		if (er != erSuccess)
			goto exit;

		er = GetABEntryID(iterCompanies->ulId, soap, &sCompanyEid);
		if (er != erSuccess)
			goto exit;
			
		er = GetABEntryID(ulAdmin, soap, &sAdminEid);
		if (er != erSuccess)
			goto exit;

		er = CopyCompanyDetailsToSoap(iterCompanies->ulId, &sCompanyEid, ulAdmin, &sAdminEid, *iterCompanies, soap, &lpsCompanyList->sCompanyArray.__ptr[lpsCompanyList->sCompanyArray.__size]);
		if (er != erSuccess)
			goto exit;

		if (!bSupportUnicode) {
			er = FixCompanyEncoding(soap, stringCompat, Out, lpsCompanyList->sCompanyArray.__ptr + lpsCompanyList->sCompanyArray.__size);
			if (er != erSuccess)
				goto exit;
		}

		lpsCompanyList->sCompanyArray.__size++;
	}

exit:
	if(lpCompanies)
		delete lpCompanies;
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(addCompanyToRemoteViewList, *result, unsigned int ulSetCompanyId, entryId sSetCompanyId, unsigned int ulCompanyId, entryId sCompanyId, unsigned int *result)
{
	if (!g_lpSessionManager->IsHostedSupported()) {
		er = ZARAFA_E_NO_SUPPORT;
		goto exit;
	}

	er = GetLocalId(sCompanyId, ulCompanyId, &ulCompanyId, NULL);
	if (er != erSuccess)
		goto exit;

	// Check permission
	er = lpecSession->GetSecurity()->IsAdminOverUserObject(ulCompanyId);
	if(er != erSuccess)
		goto exit;

	er = GetLocalId(sSetCompanyId, ulSetCompanyId, &ulSetCompanyId, NULL);
	if (er != erSuccess)
		goto exit;

	er = lpecSession->GetUserManagement()->AddSubObjectToObjectAndSync(OBJECTRELATION_COMPANY_VIEW, ulCompanyId, ulSetCompanyId);
	if(er != erSuccess)
		goto exit;

exit:
	;
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(delCompanyFromRemoteViewList, *result, unsigned int ulSetCompanyId, entryId sSetCompanyId, unsigned int ulCompanyId, entryId sCompanyId, unsigned int *result)
{
	if (!g_lpSessionManager->IsHostedSupported()) {
		er = ZARAFA_E_NO_SUPPORT;
		goto exit;
	}

	er = GetLocalId(sCompanyId, ulCompanyId, &ulCompanyId, NULL);
	if (er != erSuccess)
		goto exit;

	// Check permission
	er = lpecSession->GetSecurity()->IsAdminOverUserObject(ulCompanyId);
	if(er != erSuccess)
		goto exit;

	er = GetLocalId(sSetCompanyId, ulSetCompanyId, &ulSetCompanyId, NULL);
	if (er != erSuccess)
		goto exit;

	er = lpecSession->GetUserManagement()->DeleteSubObjectFromObjectAndSync(OBJECTRELATION_COMPANY_VIEW, ulCompanyId, ulSetCompanyId);
	if(er != erSuccess)
		goto exit;

exit:
	;
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(getRemoteViewList, lpsCompanyList->er, unsigned int ulCompanyId, entryId sCompanyId, struct companyListResponse *lpsCompanyList)
{
	unsigned int	ulAdmin = 0;
	entryId			sCompanyEid = {0};
	entryId			sAdminEid = {0};

	std::list<localobjectdetails_t> *lpCompanies = NULL;
	std::list<localobjectdetails_t>::iterator iterCompanies;

	if (!g_lpSessionManager->IsHostedSupported()) {
		er = ZARAFA_E_NO_SUPPORT;
		goto exit;
	}

	er = GetLocalId(sCompanyId, ulCompanyId, &ulCompanyId, NULL);
	if (er != erSuccess)
		goto exit;

	/* Input check, if ulCompanyId is 0, we want the user's company,
	 * otherwise we must check if the requested company is visible for the user. */
	if (ulCompanyId == 0) {
		er = lpecSession->GetSecurity()->GetUserCompany(&ulCompanyId);
		if (er != erSuccess)
			goto exit;
	} else {
		er = lpecSession->GetSecurity()->IsUserObjectVisible(ulCompanyId);
		if (er != erSuccess)
			goto exit;
	}

	er = lpecSession->GetUserManagement()->GetSubObjectsOfObjectAndSync(OBJECTRELATION_COMPANY_VIEW, ulCompanyId, &lpCompanies);
	if(er != erSuccess)
		goto exit;

	lpsCompanyList->sCompanyArray.__size = 0;
	lpsCompanyList->sCompanyArray.__ptr = s_alloc<company>(soap, lpCompanies->size());

	for(iterCompanies = lpCompanies->begin(); iterCompanies != lpCompanies->end(); iterCompanies++) {
		if (lpecSession->GetSecurity()->IsUserObjectVisible(iterCompanies->ulId) != erSuccess) {
			continue;
		}

		ulAdmin = iterCompanies->GetPropInt(OB_PROP_I_SYSADMIN);
		er = lpecSession->GetSecurity()->IsUserObjectVisible(ulAdmin);
		if (er != erSuccess)
			goto exit;

		er = GetABEntryID(iterCompanies->ulId, soap, &sCompanyEid);
		if (er != erSuccess)
			goto exit;
			
		er = GetABEntryID(ulAdmin, soap, &sAdminEid);
		if (er != erSuccess)
			goto exit;

		er = CopyCompanyDetailsToSoap(iterCompanies->ulId, &sCompanyEid, ulAdmin, &sAdminEid, *iterCompanies, soap, &lpsCompanyList->sCompanyArray.__ptr[lpsCompanyList->sCompanyArray.__size]);
		if (er != erSuccess)
			goto exit;

		if (!bSupportUnicode) {
			er = FixCompanyEncoding(soap, stringCompat, Out, lpsCompanyList->sCompanyArray.__ptr + lpsCompanyList->sCompanyArray.__size);
			if (er != erSuccess)
				goto exit;
		}

		lpsCompanyList->sCompanyArray.__size++;
	}

exit:
	if(lpCompanies)
		delete lpCompanies;
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(addUserToRemoteAdminList, *result, unsigned int ulUserId, entryId sUserId, unsigned int ulCompanyId, entryId sCompanyId, unsigned int *result)
{
	if (!g_lpSessionManager->IsHostedSupported()) {
		er = ZARAFA_E_NO_SUPPORT;
		goto exit;
	}

	er = GetLocalId(sCompanyId, ulCompanyId, &ulCompanyId, NULL);
	if (er != erSuccess)
		goto exit;

	// Check permission
	er = lpecSession->GetSecurity()->IsAdminOverUserObject(ulCompanyId);
	if(er != erSuccess)
		goto exit;

	er = GetLocalId(sUserId, ulUserId, &ulUserId, NULL);
	if (er != erSuccess)
		goto exit;

	er = lpecSession->GetUserManagement()->AddSubObjectToObjectAndSync(OBJECTRELATION_COMPANY_ADMIN, ulCompanyId, ulUserId);
	if(er != erSuccess)
		goto exit;

exit:
	;
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(delUserFromRemoteAdminList, *result, unsigned int ulUserId, entryId sUserId, unsigned int ulCompanyId, entryId sCompanyId, unsigned int *result)
{
	if (!g_lpSessionManager->IsHostedSupported()) {
		er = ZARAFA_E_NO_SUPPORT;
		goto exit;
	}

	er = GetLocalId(sCompanyId, ulCompanyId, &ulCompanyId, NULL);
	if (er != erSuccess)
		goto exit;

	// Check permission
	er = lpecSession->GetSecurity()->IsAdminOverUserObject(ulCompanyId);
	if(er != erSuccess)
		goto exit;

	er = GetLocalId(sUserId, ulUserId, &ulUserId, NULL);
	if (er != erSuccess)
		goto exit;

	er = lpecSession->GetUserManagement()->DeleteSubObjectFromObjectAndSync(OBJECTRELATION_COMPANY_ADMIN, ulCompanyId, ulUserId);
	if(er != erSuccess)
		goto exit;

exit:
	;
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(getRemoteAdminList, lpsUserList->er, unsigned int ulCompanyId, entryId sCompanyId, struct userListResponse *lpsUserList)
{
	std::list<localobjectdetails_t> *lpUsers = NULL;
	std::list<localobjectdetails_t>::iterator iterUsers;
	entryId		sUserEid = {0};

	if (!g_lpSessionManager->IsHostedSupported()) {
		er = ZARAFA_E_NO_SUPPORT;
		goto exit;
	}

	er = GetLocalId(sCompanyId, ulCompanyId, &ulCompanyId, NULL);
	if (er != erSuccess)
		goto exit;

	/* Input check, if ulCompanyId is 0, we want the user's company,
	 * otherwise we must check if the requested company is visible for the user. */
	if (ulCompanyId == 0) {
		er = lpecSession->GetSecurity()->GetUserCompany(&ulCompanyId);
		if (er != erSuccess)
			goto exit;
	} else {
		er = lpecSession->GetSecurity()->IsUserObjectVisible(ulCompanyId);
		if (er != erSuccess)
			goto exit;
	}

	// only users can be admins, nonactive users make no sense.
	er = lpecSession->GetUserManagement()->GetSubObjectsOfObjectAndSync(OBJECTRELATION_COMPANY_ADMIN, ulCompanyId, &lpUsers);
	if(er != erSuccess)
		goto exit;

	lpsUserList->sUserArray.__size = 0;
	lpsUserList->sUserArray.__ptr = s_alloc<user>(soap, lpUsers->size());

	for(iterUsers = lpUsers->begin(); iterUsers != lpUsers->end(); iterUsers++) {
		if (lpecSession->GetSecurity()->IsUserObjectVisible(iterUsers->ulId) != erSuccess) {
			continue;
		}

		er = GetABEntryID(iterUsers->ulId, soap, &sUserEid);
		if (er != erSuccess)
			goto exit;

		er = CopyUserDetailsToSoap(iterUsers->ulId, &sUserEid, *iterUsers, soap, &lpsUserList->sUserArray.__ptr[lpsUserList->sUserArray.__size]);
		if (er != erSuccess)
			goto exit;

		// 6.40.0 stores the object class in the IsNonActive field
		if (lpecSession->ClientVersion() == ZARAFA_VERSION_6_40_0)
			lpsUserList->sUserArray.__ptr[lpsUserList->sUserArray.__size].ulIsNonActive = lpsUserList->sUserArray.__ptr[lpsUserList->sUserArray.__size].ulObjClass;

		if (!bSupportUnicode) {
			er = FixUserEncoding(soap, stringCompat, Out, lpsUserList->sUserArray.__ptr + lpsUserList->sUserArray.__size);
			if (er != erSuccess)
				goto exit;
		}

		lpsUserList->sUserArray.__size++;

		if (sUserEid.__ptr)
		{
			// sUserEid is placed in userdetails, no need to free
			sUserEid.__ptr = NULL;
			sUserEid.__size = 0;
		}
	}

exit:
	if(lpUsers)
		delete lpUsers;
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(submitMessage, *result, entryId sEntryId, unsigned int ulFlags, unsigned int *result)
{
	unsigned int	ulParentId	= 0;
	unsigned int	ulObjId = 0;
	unsigned int	ulMsgFlags 	= 0;
	unsigned int	ulStoreId = 0;
	unsigned int	ulStoreOwner = 0;
	SOURCEKEY		sSourceKey;
	SOURCEKEY		sParentSourceKey;
	bool			bMessageChanged = false;

	eQuotaStatus	QuotaStatus;
	long long		llStoreSize = 0;
	objectdetails_t details;
	
	USE_DATABASE();

	er = lpecSession->GetObjectFromEntryId(&sEntryId, &ulObjId);
	if(er != erSuccess)
	    goto exit;

	er = lpecSession->GetSessionManager()->GetCacheManager()->GetStore(ulObjId, &ulStoreId, NULL);
	if(er != erSuccess)
	    goto exit;

    er = lpecSession->GetSessionManager()->GetCacheManager()->GetObject(ulObjId, &ulParentId, NULL, &ulMsgFlags, NULL);
    if(er != erSuccess)
        goto exit;
        
    er = lpecSession->GetSessionManager()->GetCacheManager()->GetObject(ulStoreId, NULL, &ulStoreOwner, NULL, NULL);
    if(er != erSuccess)
        goto exit;
     
    er = lpecSession->GetUserManagement()->GetObjectDetails(ulStoreOwner, &details);
    if(er != erSuccess)
        goto exit;
         
    // Cannot submit a message in a public store   
    if(OBJECTCLASS_TYPE(details.GetClass()) != OBJECTTYPE_MAILUSER) {
        er = ZARAFA_E_NO_ACCESS;
        goto exit;
    }

	// Check permission
	er = lpecSession->GetSecurity()->CheckPermission(ulStoreId, ecSecurityOwner);
	if(er != erSuccess)
		goto exit;

	/////////////////////////////////////////////////
	// Quota check
	er = lpecSession->GetSecurity()->GetStoreSize(ulStoreId, &llStoreSize);
	if(er != erSuccess)
		goto exit;

	er = lpecSession->GetSecurity()->CheckQuota(ulStoreId, llStoreSize, &QuotaStatus);
	if(er != erSuccess)
		goto exit;

	if(QuotaStatus == QUOTA_SOFTLIMIT || QuotaStatus == QUOTA_HARDLIMIT) {
		er = ZARAFA_E_STORE_FULL;
		goto exit;
	}

	er = lpDatabase->Begin();
	if(er != erSuccess)
		goto exit;

	// Set PR_MESSAGE_FLAGS to MSGFLAG_SUBMIT|MSGFLAG_UNSENT
	if(!(ulFlags & EC_SUBMIT_MASTER)) {
	    // Set the submit flag (because it has just been submitted), and set it to UNSENT, as it has definitely
	    // not been sent if the user has just submitted it.
		strQuery = "UPDATE properties SET val_ulong=val_ulong|"+stringify(MSGFLAG_SUBMIT|MSGFLAG_UNSENT)+" where hierarchyid="+stringify(ulObjId) + " and tag=" + stringify(PROP_ID(PR_MESSAGE_FLAGS)) + " and type=" + stringify(PROP_TYPE(PR_MESSAGE_FLAGS));
		er = lpDatabase->DoUpdate(strQuery);
		if(er != erSuccess)
			goto exit;

		// Add change to ICS
		GetSourceKey(ulObjId, &sSourceKey);
		GetSourceKey(ulParentId, &sParentSourceKey);

		AddChange(lpecSession, 0, sSourceKey, sParentSourceKey, ICS_MESSAGE_CHANGE);

		// Mask for notification
		bMessageChanged = true;
	}
	
	er = UpdateTProp(lpDatabase, PR_MESSAGE_FLAGS, ulParentId, ulObjId);
	if(er != erSuccess)
		goto exit;

	// Insert the message into the outgoing queue
	strQuery = "INSERT INTO outgoingqueue (store_id, hierarchy_id, flags) VALUES("+stringify(ulStoreId)+", "+stringify(ulObjId)+","+stringify(ulFlags)+")";
	er = lpDatabase->DoInsert(strQuery);
	if(er != erSuccess)
		goto exit;

	er = lpDatabase->Commit();
	if(er != erSuccess)
		goto exit;

	if (bMessageChanged) {
		// Update cache
		g_lpSessionManager->GetCacheManager()->Update(fnevObjectModified, ulObjId);
		g_lpSessionManager->GetCacheManager()->Update(fnevObjectModified, ulParentId);

		// Notify
		g_lpSessionManager->NotificationModified(MAPI_MESSAGE, ulObjId, ulParentId);
		g_lpSessionManager->UpdateTables(ECKeyTable::TABLE_ROW_MODIFY, 0, ulParentId, ulObjId, MAPI_MESSAGE);
	}
	
	g_lpSessionManager->UpdateOutgoingTables(ECKeyTable::TABLE_ROW_ADD, ulStoreId, ulObjId, ulFlags, MAPI_MESSAGE);

exit:
    ROLLBACK_ON_ERROR();
    FREE_DBRESULT();
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(finishedMessage, *result,  entryId sEntryId, unsigned int ulFlags, unsigned int *result)
{
	unsigned int	ulParentId	= 0;
	unsigned int	ulGrandParentId = 0;
	unsigned int	ulAffectedRows = 0;
	unsigned int	ulStoreId = (unsigned int)-1; // not 0 security issue
	unsigned int	ulObjId = 0;
	unsigned int	ulPrevFlags = 0;
	bool			bMessageChanged = false;
	SOURCEKEY		sSourceKey;
	SOURCEKEY		sParentSourceKey;

	USE_DATABASE();

	er = lpDatabase->Begin();
	if(er != erSuccess)
		goto exit;

	er = lpecSession->GetObjectFromEntryId(&sEntryId, &ulObjId);
	if(er != erSuccess)
	    goto exit;

	er = g_lpSessionManager->GetCacheManager()->GetParent(ulObjId, &ulParentId);
	if(er != erSuccess)
		goto exit;

	//Get storeid
	er = lpecSession->GetSessionManager()->GetCacheManager()->GetStore(ulObjId, &ulStoreId, NULL);
	switch (er) {
	case erSuccess:
		break;
	case ZARAFA_E_NOT_FOUND:
		// ulObjId should be in outgoingtable, but the ulStoreId cannot be retrieved
		// because ulObjId does not exist in the hierarchy table, so we remove the message
		// fix table and notify and pass error to caller

		g_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_WARNING, "Unable to find store for hierarchy id %d", ulObjId);

		ulStoreId = 0;
		goto table;
	default:
		goto exit;				// database error
	}

	// Check permission
	er = lpecSession->GetSecurity()->CheckPermission(ulStoreId, ecSecurityOwner);
	if(er != erSuccess)
		goto exit;
		
    strQuery = "SELECT val_ulong FROM properties WHERE hierarchyid="+stringify(ulObjId) + " AND tag=" + stringify(PROP_ID(PR_MESSAGE_FLAGS)) + " AND type=" + stringify(PROP_TYPE(PR_MESSAGE_FLAGS)) + " FOR UPDATE";
    er = lpDatabase->DoSelect(strQuery, &lpDBResult);
    if(er != erSuccess)
        goto exit;
        
    lpDBRow = lpDatabase->FetchRow(lpDBResult);
    if(lpDBRow == NULL || lpDBRow[0] == NULL) {
        er = ZARAFA_E_DATABASE_ERROR;
        goto exit;
    }
    
    ulPrevFlags = atoui(lpDBRow[0]);

	strQuery = "UPDATE properties ";

	if(!(ulFlags & EC_SUBMIT_MASTER)) {
        // Removing from local queue; remove submit flag and unsent flag
	    strQuery += " SET val_ulong=val_ulong&~"+stringify(MSGFLAG_SUBMIT|MSGFLAG_UNSENT);
    } else {
        // Removing from master queue
    	if(ulFlags & EC_SUBMIT_DOSENTMAIL) {
            // Spooler sent message and moved, remove submit flag and unsent flag
    	    strQuery += " SET val_ulong=val_ulong&~" +stringify(MSGFLAG_SUBMIT|MSGFLAG_UNSENT);
        } else {
            // Spooler only sent message
            strQuery += " SET val_ulong=val_ulong&~" +stringify(MSGFLAG_UNSENT);
        }
    }

    // Always set message read
    strQuery += ", val_ulong=val_ulong|" + stringify(MSGFLAG_READ) + " WHERE hierarchyid="+stringify(ulObjId) + " AND tag=" + stringify(PROP_ID(PR_MESSAGE_FLAGS)) + " AND type=" + stringify(PROP_TYPE(PR_MESSAGE_FLAGS));

   	er = lpDatabase->DoUpdate(strQuery);
   	if(er != erSuccess)
   		goto exit;
   		
	er = UpdateTProp(lpDatabase, PR_MESSAGE_FLAGS, ulParentId, ulObjId);
	if(er != erSuccess)
		goto exit;

    if(!(ulPrevFlags & MSGFLAG_READ)) {
        // The item has been set read, decrease the unread counter for the folder
        er = UpdateFolderCount(lpDatabase, ulParentId, PR_CONTENT_UNREAD, -1);
        if(er != erSuccess)
            goto exit;
    }

	GetSourceKey(ulObjId, &sSourceKey);
	GetSourceKey(ulParentId, &sParentSourceKey);

	AddChange(lpecSession, 0, sSourceKey, sParentSourceKey, ICS_MESSAGE_CHANGE);
    
	// NOTE: Unlock message is done in client

	// Mark for notification
	bMessageChanged = true;

table:

	// delete the message from the outgoing queue
	strQuery = "DELETE FROM outgoingqueue WHERE hierarchy_id="+stringify(ulObjId) + " AND flags & 1=" + stringify(ulFlags & 1);
	er = lpDatabase->DoDelete(strQuery, &ulAffectedRows);
	if(er != erSuccess)
		goto exit;

	er = lpDatabase->Commit();
	if(er != erSuccess)
		goto exit;


	// Remove messge from the outgoing queue
	g_lpSessionManager->UpdateOutgoingTables(ECKeyTable::TABLE_ROW_DELETE, ulStoreId, ulObjId, ulFlags, MAPI_MESSAGE);

	// The flags have changed, so we have to send a modified 
	if (bMessageChanged) {
		lpecSession->GetSessionManager()->GetCacheManager()->Update(fnevObjectModified, ulObjId);
		g_lpSessionManager->NotificationModified(MAPI_MESSAGE, ulObjId, ulParentId);

		if(g_lpSessionManager->GetCacheManager()->GetParent(ulObjId, &ulParentId) == erSuccess) {
			g_lpSessionManager->GetCacheManager()->Update(fnevObjectModified, ulParentId);
			g_lpSessionManager->NotificationModified(MAPI_FOLDER, ulParentId);
	        
			g_lpSessionManager->UpdateTables(ECKeyTable::TABLE_ROW_MODIFY, 0, ulParentId, ulObjId, MAPI_MESSAGE);
			if(g_lpSessionManager->GetCacheManager()->GetParent(ulParentId, &ulGrandParentId) == erSuccess) {
				g_lpSessionManager->UpdateTables(ECKeyTable::TABLE_ROW_MODIFY, 0, ulGrandParentId, ulParentId, MAPI_FOLDER);
			}
		}
	}

exit:
    ROLLBACK_ON_ERROR();
    FREE_DBRESULT();
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(abortSubmit, *result, entryId sEntryId, unsigned int *result)
{
	unsigned int	ulParentId	= 0;
	unsigned int	ulStoreId	= 0;
	unsigned int	ulSubmitFlags = 0;
	unsigned int	ulGrandParentId = 0;
	unsigned int	ulObjId = 0;
	SOURCEKEY		sSourceKey;
	SOURCEKEY		sParentSourceKey;

	USE_DATABASE();

	er = lpecSession->GetObjectFromEntryId(&sEntryId, &ulObjId);
	if(er != erSuccess)
	    goto exit;

	er = g_lpSessionManager->GetCacheManager()->GetParent(ulObjId, &ulParentId);
	if(er != erSuccess)
		goto exit;

	// Check permission
	er = lpecSession->GetSecurity()->CheckPermission(ulObjId, ecSecurityOwner);
	if(er != erSuccess)
		goto exit;

	//Get storeid
	er = lpecSession->GetSessionManager()->GetCacheManager()->GetStore(ulObjId, &ulStoreId, NULL);
	if(er != erSuccess)
		goto exit;
		
	er = lpDatabase->Begin();
	if(er != erSuccess)
		goto exit;

	// Get storeid and check if the message into the queue
	strQuery = "SELECT store_id, flags FROM outgoingqueue WHERE hierarchy_id="+stringify(ulObjId);
	er = lpDatabase->DoSelect(strQuery, &lpDBResult);
	if(er != erSuccess)
		goto exit;
// FIXME: can be also more than 2??
	if(lpDatabase->GetNumRows(lpDBResult) != 1){
		er = ZARAFA_E_NOT_IN_QUEUE;
		goto exit;
	}

	lpDBRow = lpDatabase->FetchRow(lpDBResult);
	if(lpDBRow == NULL || lpDBRow[0] == NULL || lpDBRow[1] == NULL) {
		er = ZARAFA_E_DATABASE_ERROR;
		goto exit;
	}

	ulStoreId = atoui(lpDBRow[0]);
	ulSubmitFlags = atoi(lpDBRow[1]);

	// delete the message from the outgoing queue
	strQuery = "DELETE FROM outgoingqueue WHERE hierarchy_id="+stringify(ulObjId);
	er = lpDatabase->DoDelete(strQuery);
	if(er != erSuccess)
		goto exit;

	// remove in property PR_MESSAGE_FLAGS the MSGFLAG_SUBMIT flag
	strQuery = "UPDATE properties SET val_ulong=val_ulong& ~"+stringify(MSGFLAG_SUBMIT)+" WHERE hierarchyid="+stringify(ulObjId)+ " AND tag=" + stringify(PROP_ID(PR_MESSAGE_FLAGS)) + " AND type=" + stringify(PROP_TYPE(PR_MESSAGE_FLAGS));
	er = lpDatabase->DoUpdate(strQuery);
	if(er != erSuccess)
		goto exit;
		
	er = UpdateTProp(lpDatabase, PR_MESSAGE_FLAGS, ulParentId, ulObjId);
	if(er != erSuccess)
		goto exit;
	
	// Update ICS system
	GetSourceKey(ulObjId, &sSourceKey);
	GetSourceKey(ulParentId, &sParentSourceKey);

	AddChange(lpecSession, 0, sSourceKey, sParentSourceKey, ICS_MESSAGE_CHANGE);
    
	er = lpDatabase->Commit();
	if(er != erSuccess)
		goto exit;

	g_lpSessionManager->UpdateOutgoingTables(ECKeyTable::TABLE_ROW_DELETE, ulStoreId, ulObjId, ulSubmitFlags, MAPI_MESSAGE);

	if(g_lpSessionManager->GetCacheManager()->GetParent(ulObjId, &ulParentId) == erSuccess) {

        g_lpSessionManager->GetCacheManager()->Update(fnevObjectModified, ulObjId);
		g_lpSessionManager->NotificationModified(MAPI_MESSAGE, ulObjId, ulParentId);
        g_lpSessionManager->GetCacheManager()->Update(fnevObjectModified, ulParentId);
		g_lpSessionManager->NotificationModified(MAPI_FOLDER, ulParentId);


		g_lpSessionManager->UpdateTables(ECKeyTable::TABLE_ROW_MODIFY, 0, ulParentId, ulObjId, MAPI_MESSAGE);
		if(g_lpSessionManager->GetCacheManager()->GetParent(ulParentId, &ulGrandParentId) == erSuccess) {
			g_lpSessionManager->UpdateTables(ECKeyTable::TABLE_ROW_MODIFY, 0, ulGrandParentId, ulParentId, MAPI_FOLDER);
		}
	}

exit:
    ROLLBACK_ON_ERROR();
    FREE_DBRESULT();
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(isMessageInQueue, *result, entryId sEntryId, unsigned int *result)
{
	unsigned int	ulObjId = 0;
	USE_DATABASE();

	er = lpecSession->GetObjectFromEntryId(&sEntryId, &ulObjId);
	if(er != erSuccess)
	    goto exit;

	// Checks if message is unsent
	strQuery = "SELECT hierarchy_id FROM outgoingqueue WHERE hierarchy_id=" + stringify(ulObjId) + " AND flags & " + stringify(EC_SUBMIT_MASTER);

	if(lpDatabase->DoSelect(strQuery, &lpDBResult) != erSuccess) {
	    er = ZARAFA_E_DATABASE_ERROR;
	    goto exit;
    }

    if(lpDatabase->GetNumRows(lpDBResult) == 0) {
        er = ZARAFA_E_NOT_FOUND;
        goto exit;
    }

exit:
    FREE_DBRESULT();
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(resolveStore, lpsResponse->er, struct xsd__base64Binary sStoreGuid, struct resolveUserStoreResponse *lpsResponse)
{
	USE_DATABASE();
	string strStoreGuid;

	if (sStoreGuid.__ptr == NULL || sStoreGuid.__size == 0) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	strStoreGuid = lpDatabase->EscapeBinary(sStoreGuid.__ptr, sStoreGuid.__size);

	// @todo: Check if this is supposed to work with public stores.
	strQuery =
		"SELECT u.id, s.hierarchy_id, s.guid, s.company "
		"FROM stores AS s "
		"LEFT JOIN users AS u "
			"ON s.user_id = u.id "
		"WHERE s.guid=" + strStoreGuid ;
	if(lpDatabase->DoSelect(strQuery, &lpDBResult) != erSuccess) {
		er = ZARAFA_E_DATABASE_ERROR;
		goto exit;
	}

	if(lpDatabase->GetNumRows(lpDBResult) != 1) {
		er = ZARAFA_E_NOT_FOUND;
		goto exit;
	}

	lpDBRow = lpDatabase->FetchRow(lpDBResult);
	lpDBLen = lpDatabase->FetchRowLengths(lpDBResult);

	if(lpDBRow == NULL || lpDBRow[1] == NULL || lpDBRow[2] == NULL || lpDBRow[3] == NULL || lpDBLen == NULL) {
		er = ZARAFA_E_NOT_FOUND;
		goto exit;
	}

	if (lpDBRow[0] == NULL) {
		// check if we're admin over the store object
		er = lpecSession->GetSecurity()->IsAdminOverUserObject(atoi(lpDBRow[3]));
		if (er != erSuccess)
			goto exit;

		lpsResponse->ulUserId = 0;
		lpsResponse->sUserId.__size = 0;
		lpsResponse->sUserId.__ptr = NULL;
	} else {
		lpsResponse->ulUserId = atoi(lpDBRow[0]);

		er = GetABEntryID(lpsResponse->ulUserId, soap, &lpsResponse->sUserId);
		if (er != erSuccess)
			goto exit;
	}

	er = g_lpSessionManager->GetCacheManager()->GetEntryIdFromObject(atoui(lpDBRow[1]), soap, OPENSTORE_OVERRIDE_HOME_MDB, &lpsResponse->sStoreId);
	if(er != erSuccess)
		goto exit;

	lpsResponse->guid.__size = lpDBLen[2];
	lpsResponse->guid.__ptr = s_alloc<unsigned char>(soap, lpDBLen[2]);
	memcpy(lpsResponse->guid.__ptr, lpDBRow[2], lpDBLen[2]);

exit:
	FREE_DBRESULT();
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(resolveUserStore, lpsResponse->er, char *szUserName, unsigned int ulStoreTypeMask, unsigned int ulFlags, struct resolveUserStoreResponse *lpsResponse)
{
	unsigned int		ulObjectId = 0;
	objectdetails_t		sUserDetails;
	string				strServerName;

	USE_DATABASE();

	if (szUserName == NULL) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	szUserName = STRIN_FIX(szUserName);

	if (ulStoreTypeMask == 0)
		ulStoreTypeMask = ECSTORE_TYPE_MASK_PRIVATE | ECSTORE_TYPE_MASK_PUBLIC;

	er = lpecSession->GetUserManagement()->ResolveObjectAndSync(OBJECTCLASS_USER, szUserName, &ulObjectId);
	if ((er == ZARAFA_E_NOT_FOUND || er == ZARAFA_E_INVALID_PARAMETER) && lpecSession->GetSessionManager()->IsHostedSupported())
		// FIXME: this function is being misused, szUserName can also be a company name
		er = lpecSession->GetUserManagement()->ResolveObjectAndSync(CONTAINER_COMPANY, szUserName, &ulObjectId);
	if (er != erSuccess)
		goto exit;

	/* If we are allowed to view the user, we are allowed to know the store exists */
	er = lpecSession->GetSecurity()->IsUserObjectVisible(ulObjectId);
	if (er != erSuccess)
		goto exit;

	er = lpecSession->GetUserManagement()->GetObjectDetails(ulObjectId, &sUserDetails);
	if (er != erSuccess)
		goto exit;

	/* Only users and companies have a store */
	if (((OBJECTCLASS_TYPE(sUserDetails.GetClass()) == OBJECTTYPE_MAILUSER) && (sUserDetails.GetClass() == NONACTIVE_CONTACT)) ||
		((OBJECTCLASS_TYPE(sUserDetails.GetClass()) != OBJECTTYPE_MAILUSER) && (sUserDetails.GetClass() != CONTAINER_COMPANY)))
	{
		er = ZARAFA_E_NOT_FOUND;
		goto exit;
	}

	if (lpecSession->GetSessionManager()->IsDistributedSupported() && 
		!lpecSession->GetUserManagement()->IsInternalObject(ulObjectId)) 
	{
		if (ulStoreTypeMask & (ECSTORE_TYPE_MASK_PRIVATE | ECSTORE_TYPE_MASK_PUBLIC)) {
			/* Check if this is the correct server for its store */
			string strServerName = sUserDetails.GetPropString(OB_PROP_S_SERVERNAME);
			if (strServerName.empty()) {
				er = ZARAFA_E_NOT_FOUND;
				goto exit;
			}

			if (stricmp(strServerName.c_str(), g_lpSessionManager->GetConfig()->GetSetting("server_name")) != 0) {
				if ((ulFlags & OPENSTORE_OVERRIDE_HOME_MDB) == 0) {
					string	strServerPath;

					er = GetBestServerPath(soap, lpecSession, strServerName, &strServerPath);
					if (er != erSuccess)
						goto exit;

					lpsResponse->lpszServerPath = STROUT_FIX_CPY(strServerPath.c_str());
					g_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_INFO, "Redirecting request to '%s'", lpsResponse->lpszServerPath);
					g_lpStatsCollector->Increment(SCN_REDIRECT_COUNT, 1);
					er = ZARAFA_E_UNABLE_TO_COMPLETE;
					goto exit;
                }
			}
		}

		else if (ulStoreTypeMask & ECSTORE_TYPE_MASK_ARCHIVE) {
			// We allow an archive store to be resolved by sysadmins even if it's not supposed
			// to exist on this server for a particular user.
			if (lpecSession->GetSecurity()->GetAdminLevel() < ADMIN_LEVEL_SYSADMIN &&
				!sUserDetails.PropListStringContains((property_key_t)PR_EC_ARCHIVE_SERVERS_A, g_lpSessionManager->GetConfig()->GetSetting("server_name"), false))
			{
				// No redirect with archive stores because there can be multiple archive stores.
				er = ZARAFA_E_NOT_FOUND;
				goto exit;
			}
		}

		else {
			ASSERT(FALSE);
			er = ZARAFA_E_NOT_FOUND;
			goto exit;
		}
	}

	strQuery = "SELECT hierarchy_id, guid FROM stores WHERE user_id = " + stringify(ulObjectId) + " AND (1 << type) & " + stringify(ulStoreTypeMask);
    if(lpDatabase->DoSelect(strQuery, &lpDBResult) != erSuccess) {
    	er = ZARAFA_E_DATABASE_ERROR;
    	goto exit;
	}

    lpDBRow = lpDatabase->FetchRow(lpDBResult);
    lpDBLen = lpDatabase->FetchRowLengths(lpDBResult);

    if (lpDBRow == NULL) {
 		   er = ZARAFA_E_NOT_FOUND;
 		   goto exit;
    }
    if (lpDBRow[0] == NULL || lpDBRow[1] == NULL || lpDBLen == NULL || lpDBLen[1] == 0) {
 		   er = ZARAFA_E_DATABASE_ERROR;
 		   goto exit;
    }

    /* We found the store, so we don't need to check if this is the correct server. */
    strServerName = g_lpSessionManager->GetConfig()->GetSetting("server_name", "", "Unknown");
    // Always return the pseudo URL.
    lpsResponse->lpszServerPath = STROUT_FIX_CPY(string("pseudo://" + strServerName).c_str());

    er = g_lpSessionManager->GetCacheManager()->GetEntryIdFromObject(atoui(lpDBRow[0]), soap, ulFlags & OPENSTORE_OVERRIDE_HOME_MDB, &lpsResponse->sStoreId);

	if(er != erSuccess)
		goto exit;

	er = GetABEntryID(ulObjectId, soap, &lpsResponse->sUserId);
	if (er != erSuccess)
		goto exit;

	lpsResponse->ulUserId = ulObjectId;
	lpsResponse->guid.__size = lpDBLen[1];
	lpsResponse->guid.__ptr = s_alloc<unsigned char>(soap, lpDBLen[1]);
	memcpy(lpsResponse->guid.__ptr, lpDBRow[1], lpDBLen[1]);

exit:
    FREE_DBRESULT();
}
SOAP_ENTRY_END()

typedef struct{
	unsigned int ulId;
	unsigned int ulType;
	unsigned int ulParent;
	unsigned int ulNewId;
	unsigned int ulFlags;
	unsigned int ulMessageFlags;
	unsigned int ulOwner;
	SOURCEKEY 	 sSourceKey;
	SOURCEKEY	 sParentSourceKey;
	SOURCEKEY	 sNewSourceKey;
	EntryId	 	 sOldEntryId;
	EntryId	 	 sNewEntryId;
	bool		 bMoved;
}COPYITEM;

// Move one or more messages and/or moved a softdeleted message to a normal message
ECRESULT MoveObjects(ECSession *lpSession, ECDatabase *lpDatabase, ECListInt* lplObjectIds, unsigned int ulDestFolderId, unsigned int ulSyncId)
{
	ECRESULT		er = erSuccess;
	bool			bPartialCompletion = false;
	unsigned int	ulGrandParent = 0;
	COPYITEM		sItem;
	unsigned int	ulItemSize = 0;
	unsigned int	ulSourceStoreId = 0;
	unsigned int	ulDestStoreId = 0;
	long long		llStoreSize;
	eQuotaStatus	QuotaStatus;
	bool			bUpdateDeletedSize = false;
	ECListIntIterator	iObjectId;
	size_t			cCopyItems = 0;

	unsigned int	ulSourceId = 0;
	unsigned long long ullIMAP = 0;

	std::list<unsigned int> lstParent;
	std::list<unsigned int>::iterator iterParent;
	std::list<unsigned int> lstGrandParent;
	std::list<unsigned int>::iterator iterGrandParent;
	std::list<COPYITEM> lstCopyItems;
	std::list<COPYITEM>::iterator iterCopyItems;

	SOURCEKEY	sDestFolderSourceKey;

    std::map<unsigned int, PARENTINFO> mapFolderCounts;

	entryId*	lpsNewEntryId = NULL;
	entryId*	lpsOldEntryId = NULL;
	GUID		guidStore;

	ALLOC_DBRESULT();

	if(lplObjectIds == NULL) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	if(lplObjectIds->empty())
		goto exit; // Nothing to do

	// Check permission, Destination folder
	er = lpSession->GetSecurity()->CheckPermission(ulDestFolderId, ecSecurityCreate);
	if(er != erSuccess)
		goto exit;

	er = lpSession->GetSessionManager()->GetCacheManager()->GetStore(ulDestFolderId, &ulDestStoreId, &guidStore);
	if(er != erSuccess)
		goto exit;

	// Get from the first object the store id
	ulSourceId = *lplObjectIds->begin();

	GetSourceKey(ulDestFolderId, &sDestFolderSourceKey);

	// Get all items for the object list
	strQuery = "SELECT h.id, h.parent, h.type, h.flags, h.owner, p.val_ulong, p2.val_ulong FROM hierarchy AS h LEFT JOIN properties AS p ON p.hierarchyid=h.id AND p.tag="+stringify(PROP_ID(PR_MESSAGE_SIZE))+" AND p.type="+stringify(PROP_TYPE(PR_MESSAGE_SIZE)) + 
			   " LEFT JOIN properties AS p2 ON p2.hierarchyid=h.id AND p2.tag = " + stringify(PROP_ID(PR_MESSAGE_FLAGS)) + " AND p2.type = " + stringify(PROP_TYPE(PR_MESSAGE_FLAGS)) + " WHERE h.id IN(";

	for(iObjectId = lplObjectIds->begin(); iObjectId != lplObjectIds->end(); iObjectId++) {
		if(iObjectId != lplObjectIds->begin())
			strQuery += ",";

		strQuery += stringify(*iObjectId);
	}
	strQuery += ")";

	er = lpDatabase->DoSelect(strQuery, &lpDBResult);
	if(er != erSuccess)
		goto exit;

	// First, put all the root objects in the list
	while( (lpDBRow = lpDatabase->FetchRow(lpDBResult)) != NULL)
	{
		if(lpDBRow[0] == NULL || lpDBRow[1] == NULL || lpDBRow[2] == NULL || lpDBRow[3] == NULL || lpDBRow[4] == NULL) // no id, type or parent folder?
			continue;

        sItem.bMoved 	= false;
		sItem.ulId		= atoi(lpDBRow[0]);
		sItem.ulParent	= atoi(lpDBRow[1]);
		sItem.ulType	= atoi(lpDBRow[2]);
		sItem.ulFlags	= atoi(lpDBRow[3]);
		sItem.ulOwner	= atoi(lpDBRow[4]);
		sItem.ulMessageFlags = lpDBRow[6] ? atoi(lpDBRow[6]) : 0;

		if (sItem.ulType != MAPI_MESSAGE) {
			bPartialCompletion = true;
			continue;
		}
		
		GetSourceKey(sItem.ulId, &sItem.sSourceKey);
		GetSourceKey(sItem.ulParent, &sItem.sParentSourceKey);

		// Check permission, source messages
		er = lpSession->GetSecurity()->CheckPermission(sItem.ulId, ecSecurityDelete);
		if(er == erSuccess) {

			// Check if the source and dest the same store
		    er = lpSession->GetSessionManager()->GetCacheManager()->GetStore(sItem.ulId, &ulSourceStoreId, NULL);
			if(er != erSuccess || ulSourceStoreId != ulDestStoreId) {
				bPartialCompletion = true;
				er = erSuccess;
				continue;
			}

			lstCopyItems.push_back(sItem);
			ulItemSize += (lpDBRow[5] != NULL)? atoi(lpDBRow[5]) : 0;

			// check if it a deleted item
			if(lpDBRow[3] != NULL && (atoi(lpDBRow[3])&MSGFLAG_DELETED ) == MSGFLAG_DELETED)
				bUpdateDeletedSize = true;
		}else {
			bPartialCompletion = true;
			er = erSuccess;
		}
	}
	
	// Free database results
	if(lpDBResult) { lpDatabase->FreeResult(lpDBResult); lpDBResult = NULL;}

	// Check the quota size when the item is a softdelete item
	if(bUpdateDeletedSize == true)
	{
		/////////////////////////////////////////////////
		// Quota check
		er = lpSession->GetSecurity()->GetStoreSize(ulDestFolderId, &llStoreSize);
		if(er != erSuccess)
			goto exit;

		// substract itemsize and check
		llStoreSize -= (llStoreSize >= (long long)ulItemSize)?(long long)ulItemSize:0;
		er = lpSession->GetSecurity()->CheckQuota(ulDestFolderId, llStoreSize, &QuotaStatus);
		if(er != erSuccess)
			goto exit;

		if(QuotaStatus == QUOTA_HARDLIMIT) {
			er = ZARAFA_E_STORE_FULL;
			goto exit;
		}

	}

	cCopyItems = lstCopyItems.size();

	// Move the messages to an other folder
	for(iterCopyItems=lstCopyItems.begin(); iterCopyItems != lstCopyItems.end(); iterCopyItems++) {
		sObjectTableKey key(iterCopyItems->ulId, 0);
		struct propVal sPropIMAPId;

		// Check or it is a move to the same parent, skip them
		if(iterCopyItems->ulParent == ulDestFolderId && (iterCopyItems->ulFlags&MSGFLAG_DELETED) == 0)
			continue;


		er = g_lpSessionManager->GetCacheManager()->GetEntryIdFromObject(iterCopyItems->ulId, NULL, 0, &lpsOldEntryId);
		if(er != erSuccess) {
			bPartialCompletion = true;
			er = erSuccess;
			// FIXME: Delete from list: iterCopyItems
			continue;
		}
		
		iterCopyItems->sOldEntryId = EntryId(lpsOldEntryId);

		FreeEntryId(lpsOldEntryId, true);
		lpsOldEntryId = NULL;

		er = CreateEntryId(guidStore, MAPI_MESSAGE, &lpsNewEntryId);
		if(er != erSuccess)
			goto exit;
			
        iterCopyItems->sNewEntryId = EntryId(lpsNewEntryId);

		FreeEntryId(lpsNewEntryId, true);
		lpsNewEntryId = NULL;

        // Update entryid (changes on move)
		strQuery = "REPLACE INTO indexedproperties(hierarchyid,tag,val_binary) VALUES (" + stringify(iterCopyItems->ulId) + ", 0x0FFF," + lpDatabase->EscapeBinary(iterCopyItems->sNewEntryId, iterCopyItems->sNewEntryId.size()) + ")";
		er = lpDatabase->DoUpdate(strQuery);
		if(er != erSuccess)
			goto exit;

		er = lpSession->GetNewSourceKey(&iterCopyItems->sNewSourceKey);
		if(er != erSuccess)
			goto exit;
			
        // Update source key (changes on move)
		strQuery = "REPLACE INTO indexedproperties(hierarchyid,tag,val_binary) VALUES (" + stringify(iterCopyItems->ulId) + "," + stringify(PROP_ID(PR_SOURCE_KEY)) + "," + lpDatabase->EscapeBinary(iterCopyItems->sNewSourceKey, iterCopyItems->sNewSourceKey.size()) + ")";
		er = lpDatabase->DoUpdate(strQuery);
		if(er != erSuccess)
			goto exit;

        // Update IMAP ID (changes on move)
        er = g_lpSessionManager->GetNewSequence(ECSessionManager::SEQ_IMAP, &ullIMAP);
        if(er != erSuccess)
            goto exit;

        strQuery = "REPLACE INTO properties(hierarchyid, tag, type, val_ulong) VALUES(" +
                    stringify(iterCopyItems->ulId) + "," +
                    stringify(PROP_ID(PR_EC_IMAP_ID)) + "," +
                    stringify(PROP_TYPE(PR_EC_IMAP_ID)) + "," +
                    stringify(ullIMAP) +
                    ")";

        er = lpDatabase->DoInsert(strQuery);
        if(er != erSuccess)
            goto exit;

		sPropIMAPId.ulPropTag = PR_EC_IMAP_ID;
		sPropIMAPId.Value.ul = ullIMAP;
		sPropIMAPId.__union = SOAP_UNION_propValData_ul;
		er = g_lpSessionManager->GetCacheManager()->SetCell(&key, PR_EC_IMAP_ID, &sPropIMAPId);
		if (er != erSuccess)
			goto exit;

        strQuery = "UPDATE hierarchy SET parent="+stringify(ulDestFolderId)+", flags=flags&"+stringify(~MSGFLAG_DELETED)+" WHERE id="+stringify(iterCopyItems->ulId);
        
        er = lpDatabase->DoUpdate(strQuery);
        if(er != erSuccess) {
	        bPartialCompletion = true;
            er = erSuccess;
            // FIXME: Delete from list: iterCopyItems
            continue;
        }
                                                                                
		// FIXME update last modification time

		// remove PR_DELETED_ON, This is on a softdeleted message
		strQuery = "DELETE FROM properties WHERE hierarchyid="+stringify(iterCopyItems->ulId)+" AND tag="+stringify(PROP_ID(PR_DELETED_ON))+" AND type="+stringify(PROP_TYPE(PR_DELETED_ON));
		er = lpDatabase->DoDelete(strQuery);
		if(er != erSuccess) {
			bPartialCompletion = true;
			er = erSuccess;//ignore error
		}

		// a move is a delete in the originating folder and a new in the destination folder except for softdelete that is a change
		if(iterCopyItems->ulParent != ulDestFolderId){
			AddChange(lpSession, ulSyncId, iterCopyItems->sSourceKey, iterCopyItems->sParentSourceKey, ICS_MESSAGE_HARD_DELETE);
			AddChange(lpSession, ulSyncId, iterCopyItems->sNewSourceKey, sDestFolderSourceKey, ICS_MESSAGE_NEW);
		}else if(iterCopyItems->ulFlags & MSGFLAG_DELETED) {
			// Restore a softdeleted message
			AddChange(lpSession, ulSyncId, iterCopyItems->sNewSourceKey, sDestFolderSourceKey, ICS_MESSAGE_NEW);
		}

		er = ECTPropsPurge::AddDeferredUpdate(lpSession, lpDatabase, ulDestFolderId, iterCopyItems->ulParent, iterCopyItems->ulId);
		if(er != erSuccess)
			goto exit;

		// Track folder count changes
		if(iterCopyItems->ulType == MAPI_MESSAGE) {
			if((iterCopyItems->ulFlags & MSGFLAG_DELETED) == MSGFLAG_DELETED) {
				// Undelete
				if(iterCopyItems->ulFlags & MAPI_ASSOCIATED) {
					// Associated message undeleted
					mapFolderCounts[iterCopyItems->ulParent].lDeletedAssoc--;
					mapFolderCounts[ulDestFolderId].lAssoc++;
				} else {
					// Message undeleted
					mapFolderCounts[iterCopyItems->ulParent].lDeleted--;
					mapFolderCounts[ulDestFolderId].lItems++;
					if((iterCopyItems->ulMessageFlags & MSGFLAG_READ) == 0) {
						// Undeleted message was unread
						mapFolderCounts[ulDestFolderId].lUnread++;
					}
				}
			} else {
				// Move
				mapFolderCounts[iterCopyItems->ulParent].lItems--;
				mapFolderCounts[ulDestFolderId].lItems++;
				if((iterCopyItems->ulMessageFlags & MSGFLAG_READ) == 0) {
				    mapFolderCounts[iterCopyItems->ulParent].lUnread--;
				    mapFolderCounts[ulDestFolderId].lUnread++;
				}
			}
		} 
		
		iterCopyItems->bMoved = true;
    }
    
    er = ApplyFolderCounts(lpDatabase, mapFolderCounts);
    if(er != erSuccess)
        goto exit;

	// change the size if it is a soft delete item
	if(bUpdateDeletedSize == true)
		UpdateObjectSize(lpDatabase, ulDestStoreId, MAPI_STORE, UPDATE_ADD, ulItemSize);

	for(iterCopyItems=lstCopyItems.begin(); iterCopyItems != lstCopyItems.end(); iterCopyItems++) {
	    if(iterCopyItems->bMoved) {
            // Cache update for object
            g_lpSessionManager->GetCacheManager()->SetObject(iterCopyItems->ulId, ulDestFolderId, iterCopyItems->ulOwner, iterCopyItems->ulFlags & ~MSGFLAG_DELETED /* possible undelete */, iterCopyItems->ulType);

            // Remove old sourcekey and entryid and add them
            g_lpSessionManager->GetCacheManager()->RemoveIndexData(iterCopyItems->ulId);

            g_lpSessionManager->GetCacheManager()->SetObjectProp(PROP_ID(PR_SOURCE_KEY), iterCopyItems->sNewSourceKey.size(), iterCopyItems->sNewSourceKey, iterCopyItems->ulId);

            g_lpSessionManager->GetCacheManager()->SetObjectProp(PROP_ID(PR_ENTRYID), iterCopyItems->sNewEntryId.size(), iterCopyItems->sNewEntryId, iterCopyItems->ulId);
        }
    }
    
    er = lpDatabase->Commit();
    if(er != erSuccess)
        goto exit;

    for(iterCopyItems=lstCopyItems.begin(); iterCopyItems != lstCopyItems.end(); iterCopyItems++) {
        if(iterCopyItems->bMoved) {
            // update destenation folder after PR_ENTRYID update
            if(cCopyItems < EC_TABLE_CHANGE_THRESHOLD) {
                //Update messages
                g_lpSessionManager->UpdateTables(ECKeyTable::TABLE_ROW_DELETE, 0, iterCopyItems->ulParent, iterCopyItems->ulId, iterCopyItems->ulType);
                //Update destenation folder
                g_lpSessionManager->UpdateTables(ECKeyTable::TABLE_ROW_ADD, 0, ulDestFolderId, iterCopyItems->ulId, iterCopyItems->ulType);
            }

            // Update Store object
            g_lpSessionManager->NotificationMoved(iterCopyItems->ulType, iterCopyItems->ulId, ulDestFolderId, iterCopyItems->ulParent, iterCopyItems->sOldEntryId);

            lstParent.push_back(iterCopyItems->ulParent);
        }		
	}


	lstParent.sort();
	lstParent.unique();

	//Update message folders
	for(iterParent = lstParent.begin(); iterParent != lstParent.end(); iterParent++) {
        if(cCopyItems >= EC_TABLE_CHANGE_THRESHOLD) 
            g_lpSessionManager->UpdateTables(ECKeyTable::TABLE_CHANGE, 0, *iterParent, 0, MAPI_MESSAGE);

		// update the source parent folder for disconnected clients
		WriteLocalCommitTimeMax(NULL, lpDatabase, *iterParent, NULL);
		// ignore error, no need to set partial even.

		// Get the grandparent
		g_lpSessionManager->GetCacheManager()->GetParent(*iterParent, &ulGrandParent);

        g_lpSessionManager->GetCacheManager()->Update(fnevObjectModified, *iterParent);
		g_lpSessionManager->NotificationModified(MAPI_FOLDER, *iterParent);

		g_lpSessionManager->UpdateTables(ECKeyTable::TABLE_ROW_MODIFY, 0, ulGrandParent, *iterParent, MAPI_FOLDER);

	}

	// update the destination folder for disconnected clients
	WriteLocalCommitTimeMax(NULL, lpDatabase, ulDestFolderId, NULL);
	// ignore error, no need to set partial even.

    if(cCopyItems >= EC_TABLE_CHANGE_THRESHOLD) 
        g_lpSessionManager->UpdateTables(ECKeyTable::TABLE_CHANGE, 0, ulDestFolderId, 0, MAPI_MESSAGE);

	//Update destination folder
    g_lpSessionManager->GetCacheManager()->Update(fnevObjectModified, ulDestFolderId);
	g_lpSessionManager->NotificationModified(MAPI_FOLDER, ulDestFolderId);

	// Update the grandfolder of dest. folder
	g_lpSessionManager->GetCacheManager()->GetParent(ulDestFolderId, &ulGrandParent);
	g_lpSessionManager->UpdateTables(ECKeyTable::TABLE_ROW_MODIFY, 0, ulGrandParent, ulDestFolderId, MAPI_FOLDER);

	if(bPartialCompletion && er == erSuccess)
		er = ZARAFA_W_PARTIAL_COMPLETION;
exit:

	if(lpDatabase && er != erSuccess && er != ZARAFA_W_PARTIAL_COMPLETION)
		lpDatabase->Rollback();

	if(lpDBResult)
		lpDatabase->FreeResult(lpDBResult);

	if(lpsNewEntryId)
		FreeEntryId(lpsNewEntryId, true);

	if(lpsOldEntryId)
		FreeEntryId(lpsOldEntryId, true);

	return er;
}

/**
 * Copy one message with his parent data like attachments and recipient
 *
 * @param[in] lpecSession Pointer to a session object; cannot be NULL.
 * @param[in] lpAttachmentStorage Pointer to an attachment storage object. If NULL is passed in lpAttachmentStorage, 
 * 									a default storage object with transaction enabled will be used.
 * @param[in] ulObjId Source object that identify the message, recipient or attachment to copy.
 * @param[in] ulDestFolderId Destenation object to received the copied message, recipient or attachment.
 * @param[in] bIsRoot Identify the root object; For callers this should be true;
 * @param[in] bDoNotification true if you want to send object notifications.
 * @param[in] bDoTableNotification true if you want to send table notifications.
 * @param[in] ulSyncId Client sync identify.
 *
 * @FIXME It is possible to send notifications before a commit, this can give issues with the cache! 
 * 			This function should be refactored
 */
ECRESULT CopyObject(ECSession *lpecSession, ECAttachmentStorage *lpAttachmentStorage, unsigned int ulObjId, unsigned int ulDestFolderId, bool bIsRoot, bool bDoNotification, bool bDoTableNotification, unsigned int ulSyncId)
{
	ECRESULT		er = erSuccess;
	ECDatabase		*lpDatabase = NULL;
	std::string		strQuery;
	DB_RESULT		lpDBResult = NULL;
	DB_ROW			lpDBRow;
	unsigned int	ulNewObjectId = 0;
	std::string		strExclude;
	long long		llStoreSize;
	unsigned int	ulStoreId = 0;
	unsigned int	ulSize;
	unsigned int	ulObjType;
	unsigned int	ulParent = 0;
	unsigned int	ulFlags = 0;
	GUID			guidStore;
	eQuotaStatus QuotaStatus;
	SOURCEKEY		sSourceKey;
	SOURCEKEY		sParentSourceKey;
	entryId*		lpsNewEntryId = NULL;
	unsigned long long ullIMAP = 0;
	ECAttachmentStorage *lpInternalAttachmentStorage = NULL;

	er = lpecSession->GetDatabase(&lpDatabase);
	if (er != erSuccess)
		goto exit;

	if (!lpAttachmentStorage) {
		if (!bIsRoot) {
			er = ZARAFA_E_INVALID_PARAMETER;
			goto exit;
		}

		er = CreateAttachmentStorage(lpDatabase, &lpInternalAttachmentStorage);
		if(er != erSuccess)
			goto exit;

		lpAttachmentStorage = lpInternalAttachmentStorage;
		// Hack, when lpInternalAttachmentStorage exist your are in a transaction!
	}

	er = lpecSession->GetSessionManager()->GetCacheManager()->GetStore(ulDestFolderId, &ulStoreId, &guidStore);
	if(er != erSuccess)
	    goto exit;

	// Check permission
	if(bIsRoot == true)
	{
		er = lpecSession->GetSecurity()->CheckPermission(ulObjId, ecSecurityRead);
		if(er != erSuccess)
			goto exit;

		/////////////////////////////////////////////////
		// Quota check
		er = lpecSession->GetSecurity()->GetStoreSize(ulDestFolderId, &llStoreSize);
		if(er != erSuccess)
			goto exit;

		er = lpecSession->GetSecurity()->CheckQuota(ulDestFolderId, llStoreSize, &QuotaStatus);
		if(er != erSuccess)
			goto exit;

		if(QuotaStatus == QUOTA_HARDLIMIT) {
			er = ZARAFA_E_STORE_FULL;
			goto exit;
		}

		// Start tranaction
		if (lpInternalAttachmentStorage) {
			er = lpInternalAttachmentStorage->Begin();
			if (er != erSuccess)
				goto exit;

			er = lpDatabase->Begin();
			if (er != erSuccess)
				goto exit;
		}
	}

	// Get the hierarchy messageroot but not the deleted items
	strQuery = "SELECT h.parent, h.type, p.val_ulong FROM hierarchy AS h LEFT JOIN properties AS p ON h.id = p.hierarchyid AND p.tag = " + stringify(PROP_ID(PR_MESSAGE_FLAGS)) + " AND p.type = " + stringify(PROP_TYPE(PR_MESSAGE_FLAGS)) + " WHERE h.flags & " + stringify(MSGFLAG_DELETED) + " = 0 AND id="+stringify(ulObjId);

	er = lpDatabase->DoSelect(strQuery, &lpDBResult);
	if(er != erSuccess)
		goto exit;

	if(lpDatabase->GetNumRows(lpDBResult) < 1) {
		er = ZARAFA_E_NOT_FOUND;// FIXME: right error?
		goto exit;
	}

	lpDBRow = lpDatabase->FetchRow(lpDBResult);
	if( lpDBRow == NULL || lpDBRow[0] == NULL || lpDBRow[1] == NULL) {
		er = ZARAFA_E_NOT_FOUND;
		goto exit;
	}

	ulObjType		= atoui(lpDBRow[1]);
	ulParent		= atoui(lpDBRow[0]);
	if (lpDBRow[2])
		ulFlags		= atoui(lpDBRow[2]);

	if (bIsRoot == true && ulObjType != MAPI_MESSAGE) {
		er = ZARAFA_E_INVALID_ENTRYID;
		goto exit;
	}

	//FIXME: Why do we always use the mod and create time of the old object? Create time can always be NOW
	//Create new message (Only valid flag in hierarchy is MSGFLAG_ASSOCIATED)
	strQuery = "INSERT INTO hierarchy(parent, type, flags, owner) VALUES(" +
		stringify(ulDestFolderId) + ", " +
		(string)lpDBRow[1] + ", " +
		stringify(ulFlags) + "&" + stringify(MSGFLAG_ASSOCIATED) + "," +
		stringify(lpecSession->GetSecurity()->GetUserId()) + ") ";
	er = lpDatabase->DoInsert(strQuery, &ulNewObjectId);
	if(er != erSuccess)
		goto exit;

	if(bIsRoot == true) {
		sObjectTableKey key(ulNewObjectId, 0);
		propVal sProp;

		// Create message entry
		er = CreateEntryId(guidStore, MAPI_MESSAGE, &lpsNewEntryId);
		if(er != erSuccess)
			goto exit;

		//0x0FFF = PR_ENTRYID
		strQuery = "INSERT INTO indexedproperties (hierarchyid,tag,val_binary) VALUES("+stringify(ulNewObjectId)+", 0x0FFF, "+lpDatabase->EscapeBinary(lpsNewEntryId->__ptr, lpsNewEntryId->__size)+")";
		er = lpDatabase->DoInsert(strQuery);
		if(er != erSuccess)
			goto exit;

		// Add a PR_EC_IMAP_ID
		er = g_lpSessionManager->GetNewSequence(ECSessionManager::SEQ_IMAP, &ullIMAP);
		if(er != erSuccess)
			goto exit;

		strQuery = "INSERT INTO properties(hierarchyid, tag, type, val_ulong) VALUES(" +
					stringify(ulNewObjectId) + "," +
					stringify(PROP_ID(PR_EC_IMAP_ID)) + "," +
					stringify(PROP_TYPE(PR_EC_IMAP_ID)) + "," +
					stringify(ullIMAP) +
					")";

		er = lpDatabase->DoInsert(strQuery);
		if(er != erSuccess)
			goto exit;

		sProp.ulPropTag = PR_EC_IMAP_ID;
		sProp.Value.ul = ullIMAP;
		sProp.__union = SOAP_UNION_propValData_ul;
		er = g_lpSessionManager->GetCacheManager()->SetCell(&key, PR_EC_IMAP_ID, &sProp);
		if (er != erSuccess)
			goto exit;
	}

	//Free Results
	if(lpDBResult) { lpDatabase->FreeResult(lpDBResult); lpDBResult = NULL; }
	if(lpsNewEntryId) { FreeEntryId(lpsNewEntryId, true); lpsNewEntryId = NULL; }


	// Get child items of the message like , attachment, recipient...
	strQuery = "SELECT id FROM hierarchy WHERE parent="+stringify(ulObjId);
	er = lpDatabase->DoSelect(strQuery, &lpDBResult);
	if(er != erSuccess)
		goto exit;

	if(lpDatabase->GetNumRows(lpDBResult) > 0) {

		while( (lpDBRow = lpDatabase->FetchRow(lpDBResult)) )
		{
			if(lpDBRow[0] == NULL)
				continue; // FIXME: Skip, give an error/warning ?

			er = CopyObject(lpecSession, lpAttachmentStorage, atoui(lpDBRow[0]), ulNewObjectId, false, false, false, ulSyncId);
			if(er != erSuccess && er != ZARAFA_E_NOT_FOUND)
				goto exit;
			else
				er = erSuccess;

		}

	}

	// Exclude properties
	// PR_DELETED_ON
	strExclude = " AND NOT (tag="+stringify(PROP_ID(PR_DELETED_ON))+" AND type="+stringify(PROP_TYPE(PR_DELETED_ON))+")";

	//Exclude PR_SOURCE_KEY, PR_CHANGE_KEY, PR_PREDECESSOR_CHANGE_LIST
	strExclude += " AND NOT (tag="+stringify(PROP_ID(PR_SOURCE_KEY))+" AND type="+stringify(PROP_TYPE(PR_SOURCE_KEY))+")";
	strExclude += " AND NOT (tag="+stringify(PROP_ID(PR_CHANGE_KEY))+" AND type="+stringify(PROP_TYPE(PR_CHANGE_KEY))+")";
	strExclude += " AND NOT (tag="+stringify(PROP_ID(PR_PREDECESSOR_CHANGE_LIST))+" AND type="+stringify(PROP_TYPE(PR_PREDECESSOR_CHANGE_LIST))+")";
	strExclude += " AND NOT (tag="+stringify(PROP_ID(PR_EC_IMAP_ID))+" AND type="+stringify(PROP_TYPE(PR_EC_IMAP_ID))+")";
	// because of #7699, messages contain PR_LOCAL_COMMIT_TIME_MAX
	strExclude += " AND NOT (tag="+stringify(PROP_ID(PR_LOCAL_COMMIT_TIME_MAX))+" AND type="+stringify(PROP_TYPE(PR_LOCAL_COMMIT_TIME_MAX))+")";

	// Copy properties...
	strQuery = "INSERT INTO properties (hierarchyid, tag, type, val_ulong, val_string, val_binary,val_double,val_longint,val_hi,val_lo) SELECT "+stringify(ulNewObjectId)+", tag,type,val_ulong,val_string,val_binary,val_double,val_longint,val_hi,val_lo FROM properties WHERE hierarchyid ="+stringify(ulObjId)+strExclude;
	er = lpDatabase->DoInsert(strQuery);
	if(er != erSuccess)
		goto exit;
		
	// Copy MVproperties...
	strQuery = "INSERT INTO mvproperties (hierarchyid, orderid, tag, type, val_ulong, val_string, val_binary,val_double,val_longint,val_hi,val_lo) SELECT "+stringify(ulNewObjectId)+", orderid, tag,type,val_ulong,val_string,val_binary,val_double,val_longint,val_hi,val_lo FROM mvproperties WHERE hierarchyid ="+stringify(ulObjId);
	er = lpDatabase->DoInsert(strQuery);
	if(er != erSuccess)
		goto exit;

	// Copy large objects... if present
	er = lpAttachmentStorage->CopyAttachment(ulObjId, ulNewObjectId);
	if(er != erSuccess && er != ZARAFA_E_NOT_FOUND)
		goto exit;
	er = erSuccess;

	if(bIsRoot == true)
	{
		// Create indexedproperties, Add new PR_SOURCE_KEY
		er = lpecSession->GetNewSourceKey(&sSourceKey);
		if(er != erSuccess)
			goto exit;

		strQuery = "INSERT INTO indexedproperties(hierarchyid,tag,val_binary) VALUES(" + stringify(ulNewObjectId) + "," + stringify(PROP_ID(PR_SOURCE_KEY)) + "," + lpDatabase->EscapeBinary(sSourceKey, sSourceKey.size()) + ")";
		er = lpDatabase->DoInsert(strQuery);
		if(er != erSuccess)
			goto exit;

		// Track folder count changes
		// Can we copy deleted items?
		if(ulFlags & MAPI_ASSOCIATED) {
			// Associated message undeleted
			er = UpdateFolderCount(lpDatabase, ulDestFolderId, PR_ASSOC_CONTENT_COUNT, 1);
		} else {
			// Message undeleted
			er = UpdateFolderCount(lpDatabase, ulDestFolderId, PR_CONTENT_COUNT, 1);
			if(er == erSuccess && (ulFlags & MSGFLAG_READ) == 0) {
				// Undeleted message was unread
				er = UpdateFolderCount(lpDatabase, ulDestFolderId, PR_CONTENT_UNREAD, 1);
			}
		}
		if (er != erSuccess)
			goto exit;

		// Update ICS system
		GetSourceKey(ulDestFolderId, &sParentSourceKey);
		AddChange(lpecSession, ulSyncId, sSourceKey, sParentSourceKey, ICS_MESSAGE_NEW);

		// Hack, when lpInternalAttachmentStorage exist your are in a transaction!
		if (lpInternalAttachmentStorage) {
			// Deferred tproperties
			er = ECTPropsPurge::AddDeferredUpdate(lpecSession, lpDatabase, ulDestFolderId, 0, ulNewObjectId);
			if(er != erSuccess)
				goto exit;

			er = lpInternalAttachmentStorage->Commit();
			if (er != erSuccess)
				goto exit;

			er = lpDatabase->Commit();
			if (er != erSuccess)
				goto exit;
		} else {
			// Deferred tproperties, let the caller handle the purge so we won't purge every 20 messages on a copy
			// of a complete folder.
			er = ECTPropsPurge::AddDeferredUpdateNoPurge(lpDatabase, ulDestFolderId, 0, ulNewObjectId);
			if(er != erSuccess)
				goto exit;
		}

		g_lpSessionManager->GetCacheManager()->SetObjectProp(PROP_ID(PR_SOURCE_KEY), sSourceKey.size(), sSourceKey, ulNewObjectId);

		// Update Size
		if(GetObjectSize(lpDatabase, ulNewObjectId, &ulSize) == erSuccess)
		{
			if(lpecSession->GetSessionManager()->GetCacheManager()->GetStore(ulNewObjectId, &ulStoreId, NULL) == erSuccess)
				UpdateObjectSize(lpDatabase, ulStoreId, MAPI_STORE, UPDATE_ADD, ulSize);
		}
	}


	g_lpSessionManager->GetCacheManager()->Update(fnevObjectModified, ulDestFolderId);

	if(bDoNotification){
		// Update destenation folder
    	if(bDoTableNotification)
        	g_lpSessionManager->UpdateTables(ECKeyTable::TABLE_ROW_ADD, 0, ulDestFolderId, ulNewObjectId, MAPI_MESSAGE);

		g_lpSessionManager->NotificationModified(MAPI_FOLDER, ulDestFolderId);

		// Notify object is copied
		g_lpSessionManager->NotificationCopied(MAPI_MESSAGE, ulNewObjectId, ulDestFolderId, ulObjId, ulParent);
	}

exit:
	if(er != erSuccess && lpInternalAttachmentStorage) {
		// Rollback attachments and database!
		lpInternalAttachmentStorage->Rollback();
		lpDatabase->Rollback();
	}

	if (lpInternalAttachmentStorage)
		lpInternalAttachmentStorage->Release();

	//Free Results
	if(lpDBResult)
		lpDatabase->FreeResult(lpDBResult);

	if(lpsNewEntryId)
		FreeEntryId(lpsNewEntryId, true);

	return er;
}

/**
 * Copy folder and his childs
 *
 * @note please check the object type before you call this function, the type should be MAPI_FOLDER
 */
ECRESULT CopyFolderObjects(struct soap *soap, ECSession *lpecSession, unsigned int ulFolderFrom, unsigned int ulDestFolderId, char *lpszNewFolderName, bool bCopySubFolder, unsigned int ulSyncId)
{
	ECRESULT		er = erSuccess;
	ECDatabase		*lpDatabase = NULL;
	std::string		strQuery, strSubQuery, strExclude;
	DB_RESULT		lpDBResult = NULL;
	DB_ROW			lpDBRow;

	unsigned int	ulNewDestFolderId = 0;
	unsigned int	ulItems = 0;
	unsigned int	ulGrandParent = 0;

	unsigned int	ulDestStoreId = 0;
	unsigned int	ulSourceStoreId = 0;

	bool			bPartialCompletion = false;
	long long		llStoreSize = 0;
	eQuotaStatus QuotaStatus;

	SOURCEKEY		sSourceKey;
	SOURCEKEY		sParentSourceKey;
	ECAttachmentStorage *lpAttachmentStorage = NULL;

	if(lpszNewFolderName == NULL) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	er = lpecSession->GetDatabase(&lpDatabase);
	if (er != erSuccess)
		goto exit;

	er = CreateAttachmentStorage(lpDatabase, &lpAttachmentStorage);
	if(er != erSuccess)
	    goto exit;

	er = lpecSession->GetSessionManager()->GetCacheManager()->GetStore(ulDestFolderId, &ulDestStoreId, NULL);
	if(er != erSuccess)
	    goto exit;

	er = lpecSession->GetSessionManager()->GetCacheManager()->GetStore(ulFolderFrom, &ulSourceStoreId, NULL);
	if(er != erSuccess)
	    goto exit;

	er = lpAttachmentStorage->Begin();
	if(er != erSuccess)
		goto exit;

	er = lpDatabase->Begin();
	if(er != erSuccess)
		goto exit;

	/////////////////////////////////////////////////
	// Quota check
	er = lpecSession->GetSecurity()->GetStoreSize(ulDestFolderId, &llStoreSize);
	if(er != erSuccess)
		goto exit;

	er = lpecSession->GetSecurity()->CheckQuota(ulDestFolderId, llStoreSize, &QuotaStatus);
	if(er != erSuccess)
		goto exit;

	if(QuotaStatus == QUOTA_HARDLIMIT) {
		er = ZARAFA_E_STORE_FULL;
		goto exit;
	}

	// Create folder (with a sourcekey)
	er = CreateFolder(lpecSession, lpDatabase, ulDestFolderId, NULL, FOLDER_GENERIC, lpszNewFolderName, NULL, false, true, ulSyncId, NULL, &ulNewDestFolderId, NULL);
	if(er != erSuccess)
		goto exit;

	// Always use the string version if you want to exclude properties
	strExclude = " AND NOT (tag="+stringify(PROP_ID(PR_DELETED_ON))+" AND type="+stringify(PROP_TYPE(PR_DELETED_ON))+")";
	strExclude += " AND NOT (tag="+stringify(PROP_ID(PR_DISPLAY_NAME_A))+" AND type="+stringify(PROP_TYPE(PR_DISPLAY_NAME_A))+")";

	//Exclude PR_SOURCE_KEY, PR_CHANGE_KEY, PR_PREDECESSOR_CHANGE_LIST
	strExclude += " AND NOT (tag="+stringify(PROP_ID(PR_SOURCE_KEY))+" AND type="+stringify(PROP_TYPE(PR_SOURCE_KEY))+")";
	strExclude += " AND NOT (tag="+stringify(PROP_ID(PR_CHANGE_KEY))+" AND type="+stringify(PROP_TYPE(PR_CHANGE_KEY))+")";
	strExclude += " AND NOT (tag="+stringify(PROP_ID(PR_PREDECESSOR_CHANGE_LIST))+" AND type="+stringify(PROP_TYPE(PR_PREDECESSOR_CHANGE_LIST))+")";

	// Exclude the counters
	strExclude += " AND NOT (tag="+stringify(PROP_ID(PR_CONTENT_COUNT))+" AND type="+stringify(PROP_TYPE(PR_CONTENT_COUNT))+")";
	strExclude += " AND NOT (tag="+stringify(PROP_ID(PR_CONTENT_UNREAD))+" AND type="+stringify(PROP_TYPE(PR_CONTENT_UNREAD))+")";
	strExclude += " AND NOT (tag="+stringify(PROP_ID(PR_FOLDER_CHILD_COUNT))+" AND type="+stringify(PROP_TYPE(PR_FOLDER_CHILD_COUNT))+")";
	strExclude += " AND NOT (tag="+stringify(PROP_ID(PR_ASSOC_CONTENT_COUNT))+" AND type="+stringify(PROP_TYPE(PR_ASSOC_CONTENT_COUNT))+")";
	strExclude += " AND NOT (tag="+stringify(PROP_ID(PR_DELETED_MSG_COUNT))+" AND type="+stringify(PROP_TYPE(PR_DELETED_MSG_COUNT))+")";
	strExclude += " AND NOT (tag="+stringify(PROP_ID(PR_DELETED_FOLDER_COUNT))+" AND type="+stringify(PROP_TYPE(PR_DELETED_FOLDER_COUNT))+")";
	strExclude += " AND NOT (tag="+stringify(PROP_ID(PR_DELETED_ASSOC_MSG_COUNT))+" AND type="+stringify(PROP_TYPE(PR_DELETED_ASSOC_MSG_COUNT))+")";
	strExclude += " AND NOT (tag="+stringify(PROP_ID(PR_SUBFOLDERS))+" AND type="+stringify(PROP_TYPE(PR_SUBFOLDER))+")";

	// Copy properties...
	strQuery = "REPLACE INTO properties (hierarchyid, tag, type, val_ulong, val_string, val_binary,val_double,val_longint,val_hi,val_lo) SELECT "+stringify(ulNewDestFolderId)+", tag,type,val_ulong,val_string,val_binary,val_double,val_longint,val_hi,val_lo FROM properties WHERE hierarchyid ="+stringify(ulFolderFrom)+strExclude;
	er = lpDatabase->DoInsert(strQuery);
	if(er != erSuccess)
		goto exit;

	// Copy MVproperties...
	strQuery = "REPLACE INTO mvproperties (hierarchyid, orderid, tag, type, val_ulong, val_string, val_binary,val_double,val_longint,val_hi,val_lo) SELECT "+stringify(ulNewDestFolderId)+", orderid, tag,type,val_ulong,val_string,val_binary,val_double,val_longint,val_hi,val_lo FROM mvproperties WHERE hierarchyid ="+stringify(ulFolderFrom);
	er = lpDatabase->DoInsert(strQuery);
	if(er != erSuccess)
		goto exit;

	// Copy large objects... if present .. probably not, on a folder
	er = lpAttachmentStorage->CopyAttachment(ulFolderFrom, ulNewDestFolderId);
	if(er != erSuccess && er != ZARAFA_E_NOT_FOUND)
		goto exit;
	er = erSuccess;

	// update ICS sytem with a change
	GetSourceKey(ulDestFolderId, &sParentSourceKey);
	GetSourceKey(ulNewDestFolderId, &sSourceKey);

	AddChange(lpecSession, ulSyncId, sSourceKey, sParentSourceKey, ICS_FOLDER_CHANGE);


	//Select all Messages of the home folder
	// Skip deleted and associated items
	strQuery = "SELECT id FROM hierarchy WHERE parent="+stringify(ulFolderFrom)+ " AND type="+stringify(MAPI_MESSAGE)+" AND flags & " + stringify(MSGFLAG_DELETED|MSGFLAG_ASSOCIATED) + " = 0";
	er = lpDatabase->DoSelect(strQuery, &lpDBResult);
	if(er != erSuccess)
		goto exit;

	ulItems = lpDatabase->GetNumRows(lpDBResult);

	// Walk through the messages list
	while( (lpDBRow = lpDatabase->FetchRow(lpDBResult)) )
	{
		if(lpDBRow[0] == NULL)
			continue; //FIXME: error show ???

		er = CopyObject(lpecSession, lpAttachmentStorage, atoui(lpDBRow[0]), ulNewDestFolderId, true, false, false, ulSyncId);
		// FIXME: handle ZARAFA_E_STORE_FULL
		if(er == ZARAFA_E_NOT_FOUND) {
			bPartialCompletion = true;
		} else if(er != erSuccess)
			goto exit;
	}

	if(lpDBResult) {
		lpDatabase->FreeResult(lpDBResult);
		lpDBResult = NULL;
	}

	// update the destination folder for disconnected clients
	er = WriteLocalCommitTimeMax(NULL, lpDatabase, ulNewDestFolderId, NULL);
	if (er != erSuccess)
		goto exit;

    er = ECTPropsPurge::AddDeferredUpdate(lpecSession, lpDatabase, ulDestFolderId, 0, ulNewDestFolderId);
    if(er != erSuccess)
        goto exit;

	er = lpDatabase->Commit();
	if (er != erSuccess)
		goto exit;

	er = lpAttachmentStorage->Commit();
	if (er != erSuccess)
		goto exit;

	// Notifications
	if(ulItems > 0)
	{
		//Update destenation folder
		g_lpSessionManager->UpdateTables(ECKeyTable::TABLE_CHANGE, 0, ulNewDestFolderId, 0, MAPI_MESSAGE);

		// Update the grandfolder of dest. folder
		g_lpSessionManager->UpdateTables(ECKeyTable::TABLE_ROW_MODIFY, 0, ulDestFolderId, ulNewDestFolderId, MAPI_FOLDER);

		//Update destination folder
        g_lpSessionManager->GetCacheManager()->Update(fnevObjectModified, ulNewDestFolderId);
		g_lpSessionManager->NotificationModified(MAPI_FOLDER, ulNewDestFolderId);

	}

	g_lpSessionManager->GetCacheManager()->GetParent(ulFolderFrom ,&ulGrandParent);
	g_lpSessionManager->NotificationCopied(MAPI_FOLDER, ulNewDestFolderId, ulDestFolderId, ulFolderFrom, ulGrandParent);

	if(bCopySubFolder) {
		//Select all folders of the home folder
		// Skip deleted folders
		strQuery = "SELECT hierarchy.id, properties.val_string FROM hierarchy JOIN properties ON hierarchy.id = properties.hierarchyid WHERE hierarchy.parent=" + stringify(ulFolderFrom) +" AND hierarchy.type="+stringify(MAPI_FOLDER)+" AND (flags & " + stringify(MSGFLAG_DELETED) + ") = 0 AND properties.tag=" + stringify(ZARAFA_TAG_DISPLAY_NAME) + " AND properties.type="+stringify(PT_STRING8);
		er = lpDatabase->DoSelect(strQuery, &lpDBResult);
		if(er != erSuccess)
			goto exit;

		if(lpDatabase->GetNumRows(lpDBResult) > 0) {

			// Walk through the folder list
			while( (lpDBRow = lpDatabase->FetchRow(lpDBResult)) )
			{
				if(lpDBRow[0] == NULL || lpDBRow[1] == NULL)
					continue; // ignore

				// Create SubFolder with messages. This object type checking is done in the where of the query
				er = CopyFolderObjects(soap, lpecSession, atoui(lpDBRow[0]), ulNewDestFolderId, lpDBRow[1], true, ulSyncId);
				if(er == ZARAFA_W_PARTIAL_COMPLETION)
					bPartialCompletion = true;
				else if(er != erSuccess)
					goto exit;
			}
		}
	}

	if(bPartialCompletion && er == erSuccess)
		er = ZARAFA_W_PARTIAL_COMPLETION;

exit:
    if(lpDatabase && er != erSuccess && er != ZARAFA_W_PARTIAL_COMPLETION) {
        lpDatabase->Rollback();
		if (lpAttachmentStorage)
			lpAttachmentStorage->Rollback();
	}

	if (lpAttachmentStorage)
		lpAttachmentStorage->Release();

	if(lpDBResult)
		lpDatabase->FreeResult(lpDBResult);

	return er;

}

/**
 * Copy one or more messages to a destenation
 */
SOAP_ENTRY_START(copyObjects, *result, struct entryList *aMessages, entryId sDestFolderId, unsigned int ulFlags, unsigned int ulSyncId, unsigned int *result)
{
	bool			bPartialCompletion = false;
	unsigned int	ulGrandParent=0;
	unsigned int	ulDestFolderId = 0;
	ECListInt			lObjectIds;
	ECListIntIterator	iObjectId;
	size_t				cObjectItems = 0;
	std::set<EntryId> setEntryIds;

	USE_DATABASE();

	if(aMessages == NULL) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}
	
	for(unsigned int i=0; i < aMessages->__size; i++) {
	    setEntryIds.insert(EntryId(aMessages->__ptr[i]));
	}
	setEntryIds.insert(EntryId(sDestFolderId));
	
	er = BeginLockFolders(lpDatabase, setEntryIds, LOCK_EXCLUSIVE);
	if(er != erSuccess)
	    goto exit;

	er = lpecSession->GetObjectFromEntryId(&sDestFolderId, &ulDestFolderId);
	if(er != erSuccess)
	    goto exit;

	// Check permission, Destination folder
	er = lpecSession->GetSecurity()->CheckPermission(ulDestFolderId, ecSecurityCreate);
	if(er != erSuccess)
		goto exit;

	if(g_lpSessionManager->GetCacheManager()->GetEntryListToObjectList(aMessages, &lObjectIds) != erSuccess)
		bPartialCompletion = true;

	// @note The object type checking wille be done in MoveObjects or CopyObject

	//check copy or a move
	if(ulFlags & FOLDER_MOVE ) { // A move
		er = MoveObjects(lpecSession, lpDatabase, &lObjectIds, ulDestFolderId, ulSyncId);
		if(er != erSuccess)
			goto exit;

	}else { // A copy

		cObjectItems = lObjectIds.size();
		for(iObjectId = lObjectIds.begin(); iObjectId != lObjectIds.end(); iObjectId++)
		{
			er = CopyObject(lpecSession, NULL, *iObjectId, ulDestFolderId, true, true, cObjectItems < EC_TABLE_CHANGE_THRESHOLD, ulSyncId);
			if(er != erSuccess) {
				bPartialCompletion = true;
				er = erSuccess;
			}
		}

		// update the destination folder for disconnected clients
		er = WriteLocalCommitTimeMax(NULL, lpDatabase, ulDestFolderId, NULL);
		if (er != erSuccess)
			goto exit;

		if(cObjectItems >= EC_TABLE_CHANGE_THRESHOLD)
		    g_lpSessionManager->UpdateTables(ECKeyTable::TABLE_CHANGE, 0, ulDestFolderId, 0, MAPI_MESSAGE);

		// Update the grandfolder of dest. folder
		g_lpSessionManager->GetCacheManager()->GetParent(ulDestFolderId, &ulGrandParent);
		g_lpSessionManager->UpdateTables(ECKeyTable::TABLE_ROW_MODIFY, 0, ulGrandParent, ulDestFolderId, MAPI_FOLDER);

		//Update destination folder
        g_lpSessionManager->GetCacheManager()->Update(fnevObjectModified, ulDestFolderId);
		g_lpSessionManager->NotificationModified(MAPI_FOLDER, ulDestFolderId);

	}

	if(bPartialCompletion && er == erSuccess)
		er = ZARAFA_W_PARTIAL_COMPLETION;
exit:
    lpDatabase->Commit();
    
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(copyFolder, *result, entryId sEntryId, entryId sDestFolderId, char *lpszNewFolderName, unsigned int ulFlags, unsigned int ulSyncId, unsigned int *result)
{
	unsigned int	ulAffRows = 0;
	unsigned int	ulOldParent = 0;
	unsigned int	ulGrandParent = 0;
	unsigned int	ulParentCycle = 0;
	unsigned int	ulDestStoreId = 0;
	unsigned int	ulSourceStoreId = 0;
	unsigned int	ulObjFlags = 0;
	unsigned int	ulOldGrandParent = 0;
	unsigned int	ulFolderId = 0;
	unsigned int	ulDestFolderId = 0;
	unsigned int	ulSourceType = 0;
	unsigned int	ulDestType = 0;
	long long		llFolderSize = 0;

	SOURCEKEY		sSourceKey;
	SOURCEKEY		sParentSourceKey; // Old parent
	SOURCEKEY		sDestSourceKey; // New parent

	std::string 	strSubQuery;
	USE_DATABASE();

	// NOTE: lpszNewFolderName can be NULL
	if (lpszNewFolderName)
		lpszNewFolderName = STRIN_FIX(lpszNewFolderName);

	er = lpecSession->GetObjectFromEntryId(&sEntryId, &ulFolderId);
	if(er != erSuccess)
	    goto exit;

	// Get source store
    er = g_lpSessionManager->GetCacheManager()->GetStore(ulFolderId, &ulSourceStoreId, NULL);
    if(er != erSuccess)
        goto exit;

	er = lpecSession->GetObjectFromEntryId(&sDestFolderId, &ulDestFolderId);
	if(er != erSuccess)
	    goto exit;

	// Get dest store
    er = g_lpSessionManager->GetCacheManager()->GetStore(ulDestFolderId, &ulDestStoreId, NULL);
    if(er != erSuccess)
        goto exit;

	if(ulDestStoreId != ulSourceStoreId) {
		ASSERT(FALSE);
		er = ZARAFA_E_NO_SUPPORT;
		goto exit;
	}

	// Check permission
	er = lpecSession->GetSecurity()->CheckPermission(ulDestFolderId, ecSecurityCreateFolder);
	if(er != erSuccess)
		goto exit;

	if(ulFlags & FOLDER_MOVE ) // is the folder editable?
		er = lpecSession->GetSecurity()->CheckPermission(ulFolderId, ecSecurityFolderAccess);
	else // is the folder readable
		er = lpecSession->GetSecurity()->CheckPermission(ulFolderId, ecSecurityRead);
	if(er != erSuccess)
		goto exit;

	// Check MAPI_E_FOLDER_CYCLE
	if(ulFolderId == ulDestFolderId) {
		er = ZARAFA_E_FOLDER_CYCLE;
		goto exit;
	}

	// Get the parent id, for notification and copy
	er = g_lpSessionManager->GetCacheManager()->GetObject(ulFolderId, &ulOldParent, NULL, &ulObjFlags, &ulSourceType);
	if(er != erSuccess)
		goto exit;

	er = g_lpSessionManager->GetCacheManager()->GetObject(ulDestFolderId, NULL, NULL, NULL, &ulDestType);
	if(er != erSuccess)
		goto exit;

	if (ulSourceType != MAPI_FOLDER || ulDestType != MAPI_FOLDER) {
		er = ZARAFA_E_INVALID_ENTRYID;
		goto exit;
	}

	// Check folder and dest folder are the same
	if(!(ulObjFlags & MSGFLAG_DELETED) && (ulFlags & FOLDER_MOVE) && (ulDestFolderId == ulOldParent))
		goto exit; // Do nothing... folder already on the right place

	ulParentCycle = ulDestFolderId;
	while(g_lpSessionManager->GetCacheManager()->GetParent(ulParentCycle, &ulParentCycle) == erSuccess)
	{
		if(ulFolderId == ulParentCycle)
		{
			er = ZARAFA_E_FOLDER_CYCLE;
			goto exit;
		}
	}

	// Check whether the requested name already exists
	strQuery = "SELECT hierarchy.id FROM hierarchy JOIN properties ON hierarchy.id = properties.hierarchyid WHERE parent=" + stringify(ulDestFolderId) + " AND (hierarchy.flags & " + stringify(MSGFLAG_DELETED) + ") = 0 AND hierarchy.type="+stringify(MAPI_FOLDER)+" AND properties.tag=" + stringify(ZARAFA_TAG_DISPLAY_NAME) + " AND properties.type="+stringify(PT_STRING8);
	if(lpszNewFolderName) {
		strQuery+= " AND properties.val_string = '" + lpDatabase->Escape(lpszNewFolderName) + "'";
	} else {
		strSubQuery = "SELECT properties.val_string FROM hierarchy JOIN properties ON hierarchy.id = properties.hierarchyid WHERE hierarchy.id=" + stringify(ulFolderId) + " AND properties.tag=" + stringify(ZARAFA_TAG_DISPLAY_NAME) + " AND properties.type=" + stringify(PT_STRING8);
		strQuery+= " AND properties.val_string = ("+strSubQuery+")";
	}

	er = lpDatabase->DoSelect(strQuery, &lpDBResult);
	if(er != erSuccess)
		goto exit;

	if(lpDatabase->GetNumRows(lpDBResult) > 0 && !ulSyncId) {
		er = ZARAFA_E_COLLISION;
		goto exit;
	}

	FREE_DBRESULT();

	if(lpszNewFolderName == NULL)
	{
		strQuery = "SELECT properties.val_string FROM hierarchy JOIN properties ON hierarchy.id = properties.hierarchyid WHERE hierarchy.id=" + stringify(ulFolderId) + " AND properties.tag=" + stringify(ZARAFA_TAG_DISPLAY_NAME) + " AND properties.type=" + stringify(PT_STRING8);
		er = lpDatabase->DoSelect(strQuery, &lpDBResult);
		if(er != erSuccess)
			goto exit;

		lpDBRow = lpDatabase->FetchRow(lpDBResult);
		if( lpDBRow == NULL || lpDBRow[0] == NULL) {
			er = ZARAFA_E_NOT_FOUND;
			goto exit;
		}

		lpszNewFolderName = s_alloc<char>(soap, strlen(lpDBRow[0])+1);
		memcpy(lpszNewFolderName, lpDBRow[0], strlen(lpDBRow[0])+1);

		FREE_DBRESULT();
	}

	//check copy or a move
	if(ulFlags & FOLDER_MOVE ) {

        if(ulObjFlags & MSGFLAG_DELETED) {
            // The folder we're moving used to be deleted. This effictively makes this call an un-delete. We need to get the folder size
            // for quota management
            er = GetFolderSize(lpDatabase, ulFolderId, &llFolderSize);
            if(er != erSuccess)
                goto exit;
        }

		// Get grandParent of the old folder
		g_lpSessionManager->GetCacheManager()->GetParent(ulOldParent, &ulOldGrandParent);

		er = lpDatabase->Begin();
		if(er != erSuccess)
			goto exit;

		// Move the folder to the dest. folder
		
		// FIXME update modtime
		strQuery = "UPDATE hierarchy SET parent="+stringify(ulDestFolderId)+", flags=flags&"+stringify(~MSGFLAG_DELETED)+" WHERE id="+stringify(ulFolderId);
		if(lpDatabase->DoUpdate(strQuery, &ulAffRows) != erSuccess) {
		    lpDatabase->Rollback();
			er = ZARAFA_E_DATABASE_ERROR;
			goto exit;
		}

		if(ulAffRows != 1) {
		    lpDatabase->Rollback();
			er = ZARAFA_E_DATABASE_ERROR;
			goto exit;
		}

		// Update the folder to the destination folder
		//Info: Always an update, It's not faster first check and than update/or not
		strQuery = "UPDATE properties SET val_string = '" + lpDatabase->Escape(lpszNewFolderName) + "' WHERE tag=" + stringify(ZARAFA_TAG_DISPLAY_NAME) + " AND hierarchyid="+stringify(ulFolderId) + " AND type=" + stringify(PT_STRING8);
		if(lpDatabase->DoUpdate(strQuery, &ulAffRows) != erSuccess) {
		    lpDatabase->Rollback();
			er = ZARAFA_E_DATABASE_ERROR;
			goto exit;
		}

		// remove PR_DELETED_ON, as the folder is a softdelete folder
		strQuery = "DELETE FROM properties WHERE hierarchyid="+stringify(ulFolderId)+" AND tag="+stringify(PROP_ID(PR_DELETED_ON))+" AND type="+stringify(PROP_TYPE(PR_DELETED_ON));
		er = lpDatabase->DoDelete(strQuery);
		if(er != erSuccess) {
		    lpDatabase->Rollback();
			er = ZARAFA_E_DATABASE_ERROR;
			goto exit;
		}

		// Update the store size if we did an undelete. Note ulSourceStoreId == ulDestStoreId.
		if(llFolderSize > 0)
    		UpdateObjectSize(lpDatabase, ulSourceStoreId, MAPI_STORE, UPDATE_ADD, llFolderSize);

		// ICS
		GetSourceKey(ulFolderId, &sSourceKey);
		GetSourceKey(ulDestFolderId, &sDestSourceKey);
		GetSourceKey(ulOldParent, &sParentSourceKey);
			
		AddChange(lpecSession, ulSyncId, sSourceKey, sParentSourceKey, ICS_FOLDER_CHANGE);
		AddChange(lpecSession, ulSyncId, sSourceKey, sDestSourceKey, ICS_FOLDER_CHANGE);

		// Update folder counters
		if((ulObjFlags & MSGFLAG_DELETED) == MSGFLAG_DELETED) {
			// Undelete
			er = UpdateFolderCount(lpDatabase, ulOldParent, PR_DELETED_FOLDER_COUNT, -1);
			if (er == erSuccess)
				er = UpdateFolderCount(lpDatabase, ulDestFolderId, PR_SUBFOLDERS, 1);
			if (er == erSuccess)
				er = UpdateFolderCount(lpDatabase, ulDestFolderId, PR_FOLDER_CHILD_COUNT, 1);
		} else {
			// Move
			er = UpdateFolderCount(lpDatabase, ulOldParent, PR_SUBFOLDERS, -1);
			if (er == erSuccess)
				er = UpdateFolderCount(lpDatabase, ulOldParent, PR_FOLDER_CHILD_COUNT, -1);
			if (er == erSuccess)
				er = UpdateFolderCount(lpDatabase, ulDestFolderId, PR_SUBFOLDERS, 1);
			if (er == erSuccess)
				er = UpdateFolderCount(lpDatabase, ulDestFolderId, PR_FOLDER_CHILD_COUNT, 1);
		}
		if (er != erSuccess)
			goto exit;

        er = ECTPropsPurge::AddDeferredUpdate(lpecSession, lpDatabase, ulDestFolderId, ulOldParent, ulFolderId);
        if(er != erSuccess)
            goto exit;

		er = lpDatabase->Commit();
		if(er != erSuccess) {
			lpDatabase->Rollback();
			goto exit;
		}

		// Cache update for objects
		g_lpSessionManager->GetCacheManager()->Update(fnevObjectMoved, ulFolderId);
		// Notify that the folder has moved
		g_lpSessionManager->NotificationMoved(MAPI_FOLDER, ulFolderId, ulDestFolderId, ulOldParent);

		// Update the old folder
		g_lpSessionManager->GetCacheManager()->Update(fnevObjectModified, ulOldParent);
		g_lpSessionManager->UpdateTables(ECKeyTable::TABLE_ROW_DELETE, 0, ulOldParent, ulFolderId, MAPI_FOLDER);
		g_lpSessionManager->NotificationModified(MAPI_FOLDER, ulOldParent);

		// Update the old folder's parent
		g_lpSessionManager->UpdateTables(ECKeyTable::TABLE_ROW_MODIFY, 0, ulOldGrandParent, ulOldParent, MAPI_FOLDER);

		// Update the destination folder
		g_lpSessionManager->GetCacheManager()->Update(fnevObjectModified, ulDestFolderId);
		g_lpSessionManager->UpdateTables(ECKeyTable::TABLE_ROW_ADD, 0, ulDestFolderId, ulFolderId, MAPI_FOLDER);
		g_lpSessionManager->NotificationModified(MAPI_FOLDER, ulDestFolderId);

		// Update the destination's parent
		g_lpSessionManager->GetCacheManager()->GetParent(ulDestFolderId, &ulGrandParent);
		g_lpSessionManager->UpdateTables(ECKeyTable::TABLE_ROW_MODIFY, 0, ulGrandParent, ulDestFolderId, MAPI_FOLDER);

	}else {// a copy

		er = CopyFolderObjects(soap, lpecSession, ulFolderId, ulDestFolderId, lpszNewFolderName, !!(ulFlags&COPY_SUBFOLDERS), ulSyncId);
		if(er != erSuccess)
			goto exit;

	}

exit:
    FREE_DBRESULT();
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(notify, *result, struct notification sNotification, unsigned int *result)
{
	unsigned int ulKey = 0;
	USE_DATABASE();

	// You are only allowed to send newmail notifications at the moment. This could currently
	// only be misused to send other users new mail popup notification for e-mails that aren't
	// new at all ...
	if (sNotification.ulEventType != fnevNewMail) {
	    er = ZARAFA_E_NO_ACCESS;
	    goto exit;
    }
    
    if (sNotification.newmail == NULL || sNotification.newmail->pParentId == NULL || sNotification.newmail->pEntryId == NULL) {
        er = ZARAFA_E_INVALID_PARAMETER;
        goto exit;
    }

	if (!bSupportUnicode && sNotification.newmail->lpszMessageClass != NULL)
		sNotification.newmail->lpszMessageClass = STRIN_FIX(sNotification.newmail->lpszMessageClass);

	er = lpecSession->GetObjectFromEntryId(sNotification.newmail->pParentId, &ulKey);
	if(er != erSuccess)
	    goto exit;

	sNotification.ulConnection = ulKey;

	er = g_lpSessionManager->AddNotification(&sNotification, ulKey);

exit:
    ;
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(getReceiveFolderTable, lpsReceiveFolderTable->er, entryId sStoreId, struct receiveFolderTableResponse *lpsReceiveFolderTable)
{
	int				ulRows = 0;
	int				i;
	unsigned int	ulStoreid = 0;
	USE_DATABASE();

	er = lpecSession->GetObjectFromEntryId(&sStoreId, &ulStoreid);
	if(er != erSuccess)
	    goto exit;

	// Check permission
	er = lpecSession->GetSecurity()->CheckPermission(ulStoreid, ecSecurityRead);
	if(er != erSuccess)
		goto exit;

	strQuery = "SELECT objid, messageclass FROM receivefolder WHERE storeid="+stringify(ulStoreid);

	er = lpDatabase->DoSelect(strQuery, &lpDBResult);
	if(er != erSuccess)
		goto exit;


	ulRows = lpDatabase->GetNumRows(lpDBResult);

	lpsReceiveFolderTable->sFolderArray.__ptr = s_alloc<receiveFolder>(soap, ulRows);
	lpsReceiveFolderTable->sFolderArray.__size = 0;

	i = 0;
	while((lpDBRow = lpDatabase->FetchRow(lpDBResult)) )
	{

		if(lpDBRow == NULL || lpDBRow[0] == NULL || lpDBRow[1] == NULL){
			er = ZARAFA_E_DATABASE_ERROR;
			goto exit;
		}

		er = g_lpSessionManager->GetCacheManager()->GetEntryIdFromObject(atoui(lpDBRow[0]), soap, 0, &lpsReceiveFolderTable->sFolderArray.__ptr[i].sEntryId);
		if(er != erSuccess){
			er = erSuccess;
			continue;
		}

		lpsReceiveFolderTable->sFolderArray.__ptr[i].lpszAExplicitClass = STROUT_FIX_CPY(lpDBRow[1]);

		i++;
	}

	lpsReceiveFolderTable->sFolderArray.__size = i;

exit:
    FREE_DBRESULT();
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(deleteUser, *result, unsigned int ulUserId, entryId sUserId, unsigned int *result)
{
	er = GetLocalId(sUserId, ulUserId, &ulUserId, NULL);
	if (er != erSuccess)
		goto exit;

	// Check permission
	er = lpecSession->GetSecurity()->IsAdminOverUserObject(ulUserId);
	if(er != erSuccess)
		goto exit;

    er = lpecSession->GetUserManagement()->DeleteObjectAndSync(ulUserId);
    if(er != erSuccess)
        goto exit;

exit:
    ;
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(unhookStore, *result, unsigned int ulStoreType, entryId sUserId, unsigned int ulSyncId, unsigned int *result)
{
	unsigned int	ulUserId = 0;
	unsigned int	ulAffected = 0;
	std::string		strGUID = "Unknown";

	USE_DATABASE();

	// do not use GetLocalId since the user may exist on a different server,
	// but will be migrated here and we need to remove the previous store with different guid.
	er = ABEntryIDToID(&sUserId, &ulUserId, NULL, NULL);
	if(er != erSuccess)
		goto exit;

	if (ulUserId == 0 || ulUserId == ZARAFA_UID_SYSTEM || !ECSTORE_TYPE_ISVALID(ulStoreType))
	{
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	er = lpDatabase->Begin();
	if (er != erSuccess)
		goto exit;

	strQuery = "SELECT guid FROM stores WHERE user_id=" + stringify(ulUserId) + " AND type=" + stringify(ulStoreType);
	er = lpDatabase->DoSelect(strQuery, &lpDBResult);
	if (er != erSuccess)
		goto exit;

	lpDBRow = lpDatabase->FetchRow(lpDBResult);
	lpDBLen = lpDatabase->FetchRowLengths(lpDBResult);

	if (lpDBRow == NULL || lpDBRow[0] == NULL) {
		// store not on this server
		er = ZARAFA_E_NOT_FOUND;
		goto exit;
	} 
	strGUID = bin2hex(lpDBLen[0], (unsigned char*)lpDBRow[0]);

	strQuery = "UPDATE stores SET user_id=0 WHERE user_id=" + stringify(ulUserId) + " AND type=" + stringify(ulStoreType);
	er = lpDatabase->DoUpdate(strQuery, &ulAffected);
	if (er != erSuccess)
		goto exit;

	// ulAffected == 0: The user was already orphaned 
	// ulAffected == 1: correctly disowned owner of store
	if (ulAffected > 1) {
		er = ZARAFA_E_COLLISION;
		goto exit;
	}

	er = lpDatabase->Commit();
	if (er != erSuccess)
		goto exit;

exit:
	if (er != erSuccess)
		g_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_FATAL, "Unhook of store (type %d) with userid %d and guid %s failed with error code 0x%x",  ulStoreType, ulUserId, strGUID.c_str(), er);
	else
		g_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_FATAL, "Unhook of store (type %d) with userid %d and guid %s succeeded",  ulStoreType, ulUserId, strGUID.c_str());
		
	ROLLBACK_ON_ERROR();
	FREE_DBRESULT();
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(hookStore, *result, unsigned int ulStoreType, entryId sUserId, struct xsd__base64Binary sStoreGuid, unsigned int ulSyncId, unsigned int *result)
{
	unsigned int	ulUserId = 0;
	unsigned int	ulAffected = 0;
	objectdetails_t sUserDetails;

	USE_DATABASE();

	// do not use GetLocalId since the user may exist on a different server,
	// but will be migrated here and we need to hook an old store with specified guid.
	er = ABEntryIDToID(&sUserId, &ulUserId, NULL, NULL);
	if(er != erSuccess)
		goto exit;

	if (ulUserId == 0 || ulUserId == ZARAFA_UID_SYSTEM || !ECSTORE_TYPE_ISVALID(ulStoreType))
	{
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	// get user details, see if this is the correct server
	er = lpecSession->GetUserManagement()->GetObjectDetails(ulUserId, &sUserDetails);
	if (er != erSuccess)
		goto exit;

	// check if store currently is owned and the correct type
	strQuery = "SELECT users.id, stores.id, stores.user_id, stores.hierarchy_id, stores.type FROM stores LEFT JOIN users ON stores.user_id = users.id WHERE guid = ";
	strQuery += lpDatabase->EscapeBinary(sStoreGuid.__ptr, sStoreGuid.__size);
	
	er = lpDatabase->DoSelect(strQuery, &lpDBResult);
	if (er != erSuccess)
		goto exit;

	lpDBRow = lpDatabase->FetchRow(lpDBResult);
	lpDBLen = lpDatabase->FetchRowLengths(lpDBResult);

	if(lpDBRow == NULL || lpDBLen == NULL) {
		er = ZARAFA_E_NOT_FOUND;
		goto exit;
	}

	if (lpDBRow[4] == NULL) {
		er = ZARAFA_E_DATABASE_ERROR;
		goto exit;
	}

	if (lpDBRow[0]) {
		// this store already belongs to a user
		er = ZARAFA_E_COLLISION;
		goto exit;
	}

	if (atoui(lpDBRow[4]) != ulStoreType) {
		g_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_FATAL, "Requested store type is %u, actual store type is %s", ulStoreType, lpDBRow[4]);
		er = ZARAFA_E_INVALID_TYPE;
		goto exit;
	}

	g_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_FATAL, "Hooking store %s to user %d", lpDBRow[1], ulUserId);

	// lpDBRow[2] is the old user id, which is now orphaned. We'll use this id to make the other store orphaned, so we "trade" user id's.

	// update user with new store id
	er = lpDatabase->Begin();
	if (er != erSuccess)
		goto exit;

	// remove previous user of store
	strQuery = "UPDATE stores SET user_id = " + string(lpDBRow[2]) + " WHERE user_id = " + stringify(ulUserId) + " AND type = " + stringify(ulStoreType);

	er = lpDatabase->DoUpdate(strQuery, &ulAffected);
	if (er != erSuccess)
		goto exit;

	// ulAffected == 0: The user was already orphaned 
	// ulAffected == 1: correctly disowned previous owner of store
	if (ulAffected > 1) {
		er = ZARAFA_E_COLLISION;
		goto exit;
	}

	// set new store
	strQuery = "UPDATE stores SET user_id = " + stringify(ulUserId) + " WHERE guid = ";
	strQuery += lpDatabase->EscapeBinary(sStoreGuid.__ptr, sStoreGuid.__size);

	er = lpDatabase->DoUpdate(strQuery, &ulAffected);
	if (er != erSuccess)
		goto exit;

	// we can't have one store being owned by multiple users
	if (ulAffected != 1) {
		er = ZARAFA_E_COLLISION;
		goto exit;
	}

	// update owner of store
	strQuery = "UPDATE hierarchy SET owner = " + stringify(ulUserId) + " WHERE id = " + string(lpDBRow[3]);

	er = lpDatabase->DoUpdate(strQuery, &ulAffected);
	if (er != erSuccess)
		goto exit;

	// one store has only one entry point in the hierarchy
	// (may be zero, when the user returns to it's original store, so the owner field stays the same)
	if (ulAffected > 1) {
		er = ZARAFA_E_COLLISION;
		goto exit;
	}

	er = lpDatabase->Commit();
	if (er != erSuccess)
		goto exit;

	// remove store cache item
	g_lpSessionManager->GetCacheManager()->Update(fnevObjectMoved, atoi(lpDBRow[3]));

exit:
	if (er != erSuccess)
		g_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_FATAL, "Hook of store failed: 0x%x", er);
		
	ROLLBACK_ON_ERROR();
	FREE_DBRESULT();
}
SOAP_ENTRY_END()

// Used to move the store to the public, but this isn't possible due to multi-server setups.
// And is was super slow because of the storeid column in the properties table.
SOAP_ENTRY_START(deleteStore, *result, unsigned int ulStoreId, unsigned int ulSyncId, unsigned int *result)
{
	er = ZARAFA_E_NOT_IMPLEMENTED;
}
SOAP_ENTRY_END()

// softdelete the store from the database, so this function returns quickly
SOAP_ENTRY_START(removeStore, *result, struct xsd__base64Binary sStoreGuid, unsigned int ulSyncId, unsigned int *result)
{
	unsigned int	ulCompanyId = 0;
	unsigned int	ulStoreHierarchyId = 0;
	objectdetails_t sObjectDetails;
	std::string		strUsername;

	USE_DATABASE();

	// find store id and company of guid
	strQuery = "SELECT users.id, stores.guid, stores.hierarchy_id, stores.company, stores.user_name FROM stores LEFT JOIN users ON stores.user_id = users.id WHERE stores.guid = ";
	strQuery += lpDatabase->EscapeBinary(sStoreGuid.__ptr, sStoreGuid.__size);

	er = lpDatabase->DoSelect(strQuery, &lpDBResult);
	if (er != erSuccess)
		goto exit;

	lpDBRow = lpDatabase->FetchRow(lpDBResult);
	lpDBLen = lpDatabase->FetchRowLengths(lpDBResult);

	if(lpDBRow == NULL || lpDBLen == NULL) {
		er = ZARAFA_E_NOT_FOUND;
		goto exit;
	}

	// if users.id != NULL, user still present .. log a warning admin is doing something that might not have been the action it wanted to do.
	if (lpDBRow[0] != NULL) {
		// trying to remove store from existing user
		strUsername = lpDBRow[4];
		if (lpecSession->GetUserManagement()->GetObjectDetails(atoi(lpDBRow[0]), &sObjectDetails) == erSuccess)
			strUsername = sObjectDetails.GetPropString(OB_PROP_S_LOGIN); // fullname?

		g_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_FATAL, "Unable to remove store: store is in use by user %s", strUsername.c_str());
		er = ZARAFA_E_COLLISION;
		goto exit;
	}

	// these are all 'not null' columns
	ulStoreHierarchyId = atoi(lpDBRow[2]);
	ulCompanyId = atoi(lpDBRow[3]);

	// Must be administrator over the company to be able to remove the store
	er = lpecSession->GetSecurity()->IsAdminOverUserObject(ulCompanyId);
	if (er != erSuccess)
		goto exit;

	g_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_FATAL, "Started to remove store (%s) with storename %s", bin2hex(lpDBLen[1], (unsigned char*)lpDBRow[1]).c_str(), lpDBRow[4]);

	er = lpDatabase->Begin();
	if(er != hrSuccess)
		goto exit;

	// Soft delete store
	er = MarkStoreAsDeleted(lpecSession, lpDatabase, ulStoreHierarchyId, ulSyncId);
	if(er != erSuccess)
		goto exit;

	// Remove the store entry
	strQuery = "DELETE FROM stores WHERE guid=" + lpDatabase->EscapeBinary((unsigned char*)lpDBRow[1], lpDBLen[1]);
	er = lpDatabase->DoDelete(strQuery);
	if(er != erSuccess)
		goto exit;

	// Remove receivefolder entries
	strQuery = "DELETE FROM receivefolder WHERE storeid="+stringify(ulStoreHierarchyId);
	er = lpDatabase->DoDelete(strQuery);
	if(er != erSuccess)
		goto exit;

	// Remove the acls
	strQuery = "DELETE FROM acl WHERE hierarchy_id="+stringify(ulStoreHierarchyId);
	er = lpDatabase->DoDelete(strQuery);
	if(er != erSuccess)
		goto exit;
	// TODO: acl cache!

	er = lpDatabase->Commit();
	if(er != erSuccess)
		goto exit;

	g_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_FATAL, "Finished remove store (%s)", bin2hex(lpDBLen[1], (unsigned char*)lpDBRow[1]).c_str());

exit:
	FREE_DBRESULT();

	if(er == ZARAFA_E_NO_ACCESS)
		g_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_FATAL, "Failed to remove store access denied");
	else if(er != erSuccess) {

		if(lpDatabase)
			lpDatabase->Rollback();

		g_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_FATAL, "Failed to remove store, errorcode=0x%08X", er);
	}
}
SOAP_ENTRY_END()

void* SoftDeleteRemover(void* lpTmpMain)
{
	ECRESULT		er = erSuccess;
	ECRESULT*		lper = NULL;
	char*			lpszSetting = NULL;
	unsigned int	ulDeleteTime = 0;
	unsigned int	ulFolders = 0;
	unsigned int	ulStores = 0;
	unsigned int	ulMessages = 0;
	ECSession		*lpecSession = NULL;

	lpszSetting = g_lpSessionManager->GetConfig()->GetSetting("softdelete_lifetime");

	if(lpszSetting)
		ulDeleteTime = atoi(lpszSetting) * 24 * 60 * 60;

	if(ulDeleteTime == 0)
		goto exit;

	er = g_lpSessionManager->CreateSessionInternal(&lpecSession);
	if(er != erSuccess)
		goto exit;

	// Lock the session
	lpecSession->Lock();

	g_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_INFO, "Start scheduled softdelete clean up");

	er = PurgeSoftDelete(lpecSession, ulDeleteTime, &ulMessages, &ulFolders, &ulStores, (bool*)lpTmpMain);

exit:
	if(ulDeleteTime > 0) {
		if (er == erSuccess)
			g_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_INFO, "Softdelete done: removed stores: %d, removed folders: %d, removed messages: %d", ulStores, ulFolders, ulMessages);
		else if (er == ZARAFA_E_BUSY)
			g_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_INFO, "Softdelete already running");
		else
			g_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_INFO, "Softdelete failed: removed stores: %d, removed folders: %d, removed messages: %d", ulStores, ulFolders, ulMessages);
	}

	if(lpecSession) {
		lpecSession->Unlock(); 
		g_lpSessionManager->RemoveSessionInternal(lpecSession);
	}

	// Exit with the error result
	lper = new ECRESULT;
	*lper = er;

	// Do not pthread_exit() because linuxthreads is broken and will not free any objects
	// pthread_exit((void*)lper);

	return (void *)lper;
}


SOAP_ENTRY_START(checkExistObject, *result, entryId sEntryId, unsigned int ulFlags, unsigned int *result)
{
	unsigned int	ulObjId = 0;
	unsigned int	ulObjType = 0;
	unsigned int	ulDBFlags = 0;
	
	USE_DATABASE();

	er = BeginLockFolders(lpDatabase, EntryId(sEntryId), LOCK_SHARED);
	if(er != erSuccess)
	    goto exit;
	
	er = lpecSession->GetObjectFromEntryId(&sEntryId, &ulObjId);
	if(er != erSuccess)
	    goto exit;

	er = g_lpSessionManager->GetCacheManager()->GetObject(ulObjId, NULL, NULL, &ulDBFlags, &ulObjType);
	if(er != erSuccess)
	    goto exit;

	if(ulFlags & SHOW_SOFT_DELETES) {
		if(!(ulDBFlags & MSGFLAG_DELETED)) {
			er = ZARAFA_E_NOT_FOUND;
			goto exit;
		}
	} else {
		if(ulDBFlags & MSGFLAG_DELETED) {
			er = ZARAFA_E_NOT_FOUND;
			goto exit;
		}
	}

	// Deny open to non-store-owners of search folders
	if(ulObjType == MAPI_FOLDER) {
	    if(lpecSession->GetSecurity()->IsStoreOwner(ulObjId) != erSuccess && lpecSession->GetSecurity()->IsAdminOverOwnerOfObject(ulObjId) != erSuccess) {
	    	if(ulDBFlags & FOLDER_SEARCH) {
	    		er = ZARAFA_E_NOT_FOUND;
	    		goto exit;
			}
        }
    }
exit:
    lpDatabase->Commit();
    
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(readABProps, readPropsResponse->er, entryId sEntryId, struct readPropsResponse *readPropsResponse)
{
	// FIXME: when props are PT_ERROR, they shouldn't be send to the client
	// now we have properties in the client which are MAPI_E_NOT_ENOUGH_MEMORY,
	// while they shouldn't be present (or atleast MAPI_E_NOT_FOUND)

	// These properties must be of type PT_UNICODE for string properties
	unsigned int sProps[] = {
		/* Don't touch the order of the first 7 elements!!! */
		PR_ENTRYID, PR_CONTAINER_FLAGS, PR_DEPTH, PR_EMS_AB_CONTAINERID, PR_DISPLAY_NAME, PR_EMS_AB_IS_MASTER, PR_EMS_AB_PARENT_ENTRYID,
		PR_EMAIL_ADDRESS, PR_OBJECT_TYPE, PR_DISPLAY_TYPE, PR_SEARCH_KEY, PR_PARENT_ENTRYID, PR_ADDRTYPE, PR_RECORD_KEY, PR_ACCOUNT,
		PR_SMTP_ADDRESS, PR_TRANSMITABLE_DISPLAY_NAME, PR_EMS_AB_HOME_MDB, PR_EMS_AB_HOME_MTA, PR_EMS_AB_PROXY_ADDRESSES,
		PR_EC_ADMINISTRATOR, PR_EC_NONACTIVE, PR_EC_COMPANY_NAME, PR_EMS_AB_X509_CERT, PR_AB_PROVIDER_ID, PR_EMS_AB_HIERARCHY_PATH,
		PR_EC_SENDAS_USER_ENTRYIDS, PR_EC_HOMESERVER_NAME, PR_DISPLAY_TYPE_EX, CHANGE_PROP_TYPE(PR_EMS_AB_IS_MEMBER_OF_DL, PT_MV_BINARY),
		PR_EC_ENABLED_FEATURES, PR_EC_DISABLED_FEATURES, PR_EC_ARCHIVE_SERVERS, PR_EC_ARCHIVE_COUPLINGS, PR_EMS_AB_ROOM_CAPACITY, PR_EMS_AB_ROOM_DESCRIPTION
	};

	unsigned int sPropsContainerRoot[] = {
		/* Don't touch the order of the first 7 elements!!! */
		PR_ENTRYID, PR_CONTAINER_FLAGS, PR_DEPTH, PR_EMS_AB_CONTAINERID, PR_DISPLAY_NAME, PR_EMS_AB_IS_MASTER, PR_EMS_AB_PARENT_ENTRYID,
		PR_OBJECT_TYPE, PR_DISPLAY_TYPE, PR_SEARCH_KEY, PR_RECORD_KEY, PR_PARENT_ENTRYID, PR_AB_PROVIDER_ID, PR_EMS_AB_HIERARCHY_PATH, PR_ACCOUNT,
		PR_EC_HOMESERVER_NAME, PR_EC_COMPANY_NAME
	};

	struct propTagArray ptaProps;
	unsigned int		ulId = 0;
	unsigned int    	ulTypeId = 0;
	ECDatabase*			lpDatabase = NULL;
	objectid_t			sExternId;
	auto_ptr<abprops_t>	lExtraProps;
	abprops_t::iterator	iterProps;
	unsigned int		*lpProps = NULL;
	unsigned int		ulProps = 0;
	int		i = 0;

	er = lpecSession->GetDatabase(&lpDatabase);
	if (er != erSuccess)
		goto exit;

	er = ABEntryIDToID(&sEntryId, &ulId, &sExternId, &ulTypeId);
	if(er != erSuccess)
		goto exit;

	// A v1 EntryID would return a non-empty extern id string.
	if (!sExternId.id.empty())
	{
		er = lpecSession->GetSessionManager()->GetCacheManager()->GetUserObject(sExternId, &ulId, NULL, NULL);
		if (er != erSuccess)
			goto exit;
	}

	er = lpecSession->GetSecurity()->IsUserObjectVisible(ulId);
	if (er != erSuccess)
		goto exit;

	if (ulTypeId == MAPI_ABCONT) {
		lpProps = sPropsContainerRoot;
		ulProps = arraySize(sPropsContainerRoot);
	} else if (ulTypeId == MAPI_MAILUSER || ulTypeId == MAPI_DISTLIST) {
		lpProps = sProps;
		ulProps = arraySize(sProps);
	} else {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	/* Load the additional addressbook properties */
	try {
		UserPlugin *lpPlugin = NULL;
		if (GetThreadLocalPlugin(g_lpSessionManager->GetPluginFactory(), &lpPlugin, g_lpSessionManager->GetLogger()) == erSuccess) {
			lExtraProps = lpPlugin->getExtraAddressbookProperties();
		}
	} catch (...) { }

	ptaProps.__size = ulProps;
	if (lExtraProps.get())
		ptaProps.__size += lExtraProps->size();
	ptaProps.__ptr = s_alloc<unsigned int>(soap, ptaProps.__size);

	/* Copy fixed properties */
	memcpy(ptaProps.__ptr, lpProps, ulProps * sizeof(unsigned int));
	i = ulProps;

	/* Copy extra properties */
	if (lExtraProps.get()) {
		for (iterProps = lExtraProps->begin(); iterProps != lExtraProps->end(); iterProps++) {
			ptaProps.__ptr[i] = *iterProps;
			/* The client requires some properties with non-standard types */
			switch ( PROP_ID(ptaProps.__ptr[i]) ) {
			case PROP_ID(PR_MANAGER_NAME):
			case PROP_ID(PR_EMS_AB_MANAGER):
				/* Rename PR_MANAGER_NAME to PR_EMS_AB_MANAGER and provide the PT_BINARY version with the entryid */
				ptaProps.__ptr[i] = CHANGE_PROP_TYPE(PR_EMS_AB_MANAGER, PT_BINARY);
				break;
			case PROP_ID(PR_EMS_AB_REPORTS):
				ptaProps.__ptr[i] = CHANGE_PROP_TYPE(PR_EMS_AB_REPORTS, PT_MV_BINARY);
				break;
			case PROP_ID(PR_EMS_AB_OWNER):
				/* Also provide the PT_BINARY version with the entryid */
				ptaProps.__ptr[i] = CHANGE_PROP_TYPE(PR_EMS_AB_OWNER, PT_BINARY);
				break;
			default:
				// @note plugin most likely returns PT_STRING8 and PT_MV_STRING8 types
				// Since CopyDatabasePropValToSOAPPropVal() always returns PT_UNICODE types, we will convert these here too
				// Therefore, the sProps / sPropsContainerRoot must contain PT_UNICODE types only!
				if (PROP_TYPE(ptaProps.__ptr[i]) == PT_MV_STRING8)
					ptaProps.__ptr[i] = CHANGE_PROP_TYPE(ptaProps.__ptr[i], PT_MV_UNICODE);
				else if (PROP_TYPE(ptaProps.__ptr[i]) == PT_STRING8)
					ptaProps.__ptr[i] = CHANGE_PROP_TYPE(ptaProps.__ptr[i], PT_UNICODE);
				break;
			}
			i++;
		}
	}

	/* Update the total size, the previously set value might not be accurate */
	ptaProps.__size = i;

	/* Read properties */
	if (ulTypeId == MAPI_ABCONT) {
		er = lpecSession->GetUserManagement()->GetContainerProps(soap, ulId, &ptaProps, &readPropsResponse->aPropVal);
		if (er != erSuccess)
			goto exit;
	} else {
		er = lpecSession->GetUserManagement()->GetProps(soap, ulId, &ptaProps, &readPropsResponse->aPropVal);
		if (er != erSuccess)
			goto exit;
	}

	/* Copy properties which have been correctly read to tag array */
	readPropsResponse->aPropTag.__size = 0;
	readPropsResponse->aPropTag.__ptr = s_alloc<unsigned int>(soap, ptaProps.__size);

	for (int i = 0; i < readPropsResponse->aPropVal.__size; i++) {
		if (!bSupportUnicode) {
			er = FixPropEncoding(soap, stringCompat, Out, readPropsResponse->aPropVal.__ptr + i);
			if (er != erSuccess)
				goto exit;
		}

		if(PROP_TYPE(readPropsResponse->aPropVal.__ptr[i].ulPropTag) != PT_ERROR)
			readPropsResponse->aPropTag.__ptr[readPropsResponse->aPropTag.__size++] = readPropsResponse->aPropVal.__ptr[i].ulPropTag;
	}

exit:
    ;
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(writeABProps, *result, entryId sEntryId, struct propValArray *aPropVal, unsigned int *result)
{
    er = ZARAFA_E_NOT_FOUND;
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(deleteABProps, *result, entryId sEntryId, struct propTagArray *lpsPropTags, unsigned int *result)
{
    er = ZARAFA_E_NOT_FOUND;
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(loadABProp, lpsResponse->er, entryId sEntryId, unsigned int ulPropTag, struct loadPropResponse *lpsResponse)
{
	return SOAP_OK;
}
SOAP_ENTRY_END()

/**
 * ns__abResolveNames
 *
 * @param[in]	lpaPropTag	SOAP proptag array containing requested properties.
 * @param[in]	lpsRowSet	Rows with possible search request, if matching flag in lpaFlags is MAPI_UNRESOLVED.
 * @param[in]	lpaFlags	Status of row.
 * @param[in]	ulFlags		Client ulFlags for IABContainer::ResolveNames()
 * @param[out]	lpsABResolveNames	copies of new rows and flags.
 */
SOAP_ENTRY_START(abResolveNames, lpsABResolveNames->er, struct propTagArray* lpaPropTag, struct rowSet* lpsRowSet, struct flagArray* lpaFlags, unsigned int ulFlags, struct abResolveNamesResponse* lpsABResolveNames)
{
	unsigned int	ulFlag = 0;
	unsigned int    ulObjectId = 0;
	char*			search = NULL;
	struct propValArray sPropValArrayDst;
	struct propVal *lpDisplayName = NULL;

	lpsABResolveNames->aFlags.__size = lpaFlags->__size;
	lpsABResolveNames->aFlags.__ptr = s_alloc<unsigned int>(soap, lpaFlags->__size);
	memset(lpsABResolveNames->aFlags.__ptr, 0, sizeof(unsigned int) * lpaFlags->__size);

	lpsABResolveNames->sRowSet.__size = lpsRowSet->__size;
	lpsABResolveNames->sRowSet.__ptr = s_alloc<propValArray>(soap, lpsRowSet->__size);
	memset(lpsABResolveNames->sRowSet.__ptr, 0, sizeof(struct propValArray) * lpsRowSet->__size);

	if (!bSupportUnicode) {
		er = FixRowSetEncoding(soap, stringCompat, In, lpsRowSet);
		if (er != erSuccess)
			goto exit;
	}

	for(int i = 0; i < lpsRowSet->__size; i++)
	{
		lpsABResolveNames->aFlags.__ptr[i] = lpaFlags->__ptr[i];

		if(lpaFlags->__ptr[i] == MAPI_RESOLVED)
			continue; // Client knows the information

		lpDisplayName = FindProp(&lpsRowSet->__ptr[i], CHANGE_PROP_TYPE(PR_DISPLAY_NAME, PT_UNSPECIFIED));
		if(lpDisplayName == NULL || (PROP_TYPE(lpDisplayName->ulPropTag) != PT_STRING8 && PROP_TYPE(lpDisplayName->ulPropTag) != PT_UNICODE))
			continue; // No display name

		/* Blackberry likes it to put a '=' in front of the username */
		search = strrchr(lpDisplayName->Value.lpszA, '=');
		if (search) {
			search++;
			ulFlags |= EMS_AB_ADDRESS_LOOKUP;
		} else {
			search = lpDisplayName->Value.lpszA;
		}

		/* NOTE: ECUserManagement is responsible for calling ECSecurity::IsUserObjectVisible(ulObjectId) for found objects */
		switch (lpecSession->GetUserManagement()->SearchObjectAndSync(search, ulFlags, &ulObjectId)) {
		case ZARAFA_E_COLLISION:
			ulFlag = MAPI_AMBIGUOUS;
			break;
		case erSuccess:
			ulFlag = MAPI_RESOLVED;
			break;
		case ZARAFA_E_NOT_FOUND:
		default:
			ulFlag = MAPI_UNRESOLVED;
			break;
		}

		lpsABResolveNames->aFlags.__ptr[i] = ulFlag;

		if(lpsABResolveNames->aFlags.__ptr[i] == MAPI_RESOLVED) {
			er = lpecSession->GetUserManagement()->GetProps(soap, ulObjectId, lpaPropTag, &sPropValArrayDst);
			if(er != erSuccess)
				goto exit;
			er = MergePropValArray(soap, &lpsRowSet->__ptr[i], &sPropValArrayDst, &lpsABResolveNames->sRowSet.__ptr[i]);
			if(er != erSuccess)
				goto exit;
		}
	}

	if (!bSupportUnicode) {
		er = FixRowSetEncoding(soap, stringCompat, Out, &lpsABResolveNames->sRowSet);
		if (er != erSuccess)
			goto exit;
	}

exit:
    ;
}
SOAP_ENTRY_END()

/** 
 * Syncs a new list of companies, and for each company syncs the users.
 * 
 * @param[in] ulCompanyId unused, id of company to sync
 * @param[in] sCompanyId unused, entryid of company to sync
 * @param[out] result zarafa error code
 * 
 * @return soap error code
 */
SOAP_ENTRY_START(syncUsers, *result, unsigned int ulCompanyId, entryId sCompanyId, unsigned int *result)
{
	er = lpecSession->GetUserManagement()->SyncAllObjects();
}
SOAP_ENTRY_END()

///////////////////
// Quota
//

SOAP_ENTRY_START(GetQuota, lpsQuota->er, unsigned int ulUserid, entryId sUserId, bool bGetUserDefault, struct quotaResponse* lpsQuota)
{
	quotadetails_t	quotadetails;

	er = GetLocalId(sUserId, ulUserid, &ulUserid, NULL);
	if (er != erSuccess)
		goto exit;

	// Check permission
	if(lpecSession->GetSecurity()->IsAdminOverUserObject(ulUserid) != erSuccess &&
		(lpecSession->GetSecurity()->GetUserId() != ulUserid))
	{
		er = ZARAFA_E_NO_ACCESS;
		goto exit;
	}

	er = lpecSession->GetSecurity()->GetUserQuota(ulUserid, bGetUserDefault, &quotadetails);
	if(er != erSuccess)
	    goto exit;

	lpsQuota->sQuota.bUseDefaultQuota = quotadetails.bUseDefaultQuota;
	lpsQuota->sQuota.bIsUserDefaultQuota = quotadetails.bIsUserDefaultQuota;
	lpsQuota->sQuota.llHardSize = quotadetails.llHardSize;
	lpsQuota->sQuota.llSoftSize = quotadetails.llSoftSize;
	lpsQuota->sQuota.llWarnSize = quotadetails.llWarnSize;

exit:
    ;
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(SetQuota, *result, unsigned int ulUserid, entryId sUserId, struct quota* lpsQuota, unsigned int *result)
{
	quotadetails_t	quotadetails;

	er = GetLocalId(sUserId, ulUserid, &ulUserid, NULL);
	if (er != erSuccess)
		goto exit;

	// Check permission
	if(lpecSession->GetSecurity()->IsAdminOverUserObject(ulUserid) != erSuccess &&
		(lpecSession->GetSecurity()->GetUserId() != ulUserid))
	{
		er = ZARAFA_E_NO_ACCESS;
		goto exit;
	}

	quotadetails.bUseDefaultQuota = lpsQuota->bUseDefaultQuota;
	quotadetails.bIsUserDefaultQuota = lpsQuota->bIsUserDefaultQuota;
	quotadetails.llHardSize = lpsQuota->llHardSize;
	quotadetails.llSoftSize = lpsQuota->llSoftSize;
	quotadetails.llWarnSize = lpsQuota->llWarnSize;

	er = lpecSession->GetUserManagement()->SetQuotaDetailsAndSync(ulUserid, quotadetails);

exit:
    ;
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(AddQuotaRecipient, *result, unsigned int ulCompanyid, entryId sCompanyId, unsigned int ulRecipientId, entryId sRecipientId, unsigned int ulType, unsigned int *result);
{
	er = GetLocalId(sCompanyId, ulCompanyid, &ulCompanyid, NULL);
	if (er != erSuccess)
		goto exit;

	if (lpecSession->GetSecurity()->IsAdminOverUserObject(ulCompanyid) != erSuccess) {
		er = ZARAFA_E_NO_ACCESS;
		goto exit;
	}

	er = GetLocalId(sRecipientId, ulRecipientId, &ulRecipientId, NULL);
	if (er != erSuccess)
		goto exit;

	if (OBJECTCLASS_TYPE(ulType) == OBJECTTYPE_MAILUSER)
		er = lpecSession->GetUserManagement()->AddSubObjectToObjectAndSync(OBJECTRELATION_QUOTA_USERRECIPIENT, ulCompanyid, ulRecipientId);
	else if (ulType == CONTAINER_COMPANY)
		er = lpecSession->GetUserManagement()->AddSubObjectToObjectAndSync(OBJECTRELATION_QUOTA_COMPANYRECIPIENT, ulCompanyid, ulRecipientId);
	else
		er = ZARAFA_E_INVALID_TYPE;

exit:
	;
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(DeleteQuotaRecipient, *result, unsigned int ulCompanyid, entryId sCompanyId, unsigned int ulRecipientId, entryId sRecipientId, unsigned int ulType, unsigned int *result);
{
	er = GetLocalId(sCompanyId, ulCompanyid, &ulCompanyid, NULL);
	if (er != erSuccess)
		goto exit;

	if (lpecSession->GetSecurity()->IsAdminOverUserObject(ulCompanyid) != erSuccess) {
		er = ZARAFA_E_NO_ACCESS;
		goto exit;
	}

	er = GetLocalId(sRecipientId, ulRecipientId, &ulRecipientId, NULL);
	if (er != erSuccess)
		goto exit;

	if (OBJECTCLASS_TYPE(ulType) == OBJECTTYPE_MAILUSER)
		er = lpecSession->GetUserManagement()->DeleteSubObjectFromObjectAndSync(OBJECTRELATION_QUOTA_USERRECIPIENT, ulCompanyid, ulRecipientId);
	else if (ulType == CONTAINER_COMPANY)
		er = lpecSession->GetUserManagement()->DeleteSubObjectFromObjectAndSync(OBJECTRELATION_QUOTA_COMPANYRECIPIENT, ulCompanyid, ulRecipientId);
	else
		er = ZARAFA_E_INVALID_TYPE;

exit:
	;
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(GetQuotaRecipients, lpsUserList->er, unsigned int ulUserid, entryId sUserId, struct userListResponse *lpsUserList)
{
	std::list<localobjectdetails_t> *lpUsers = NULL;
	std::list<localobjectdetails_t>::iterator iterUsers;
	objectid_t sExternId;
	objectdetails_t details;
	userobject_relation_t relation;
	unsigned int ulCompanyId;
	bool bHasLocalStore = false;
	entryId		sUserEid = {0};

	// does not return full class in sExternId.objclass
	er = GetLocalId(sUserId, ulUserid, &ulUserid, &sExternId);
	if (er != erSuccess)
		goto exit;

	// re-evaluate userid to externid to get the full class (mapi clients only know MAPI_ABCONT for companies)
	er = g_lpSessionManager->GetCacheManager()->GetUserObject(ulUserid, &sExternId, NULL, NULL);
	if (er != erSuccess)
		goto exit;

	er = CheckUserStore(lpecSession, ulUserid, ECSTORE_TYPE_PRIVATE, &bHasLocalStore);
	if (er != erSuccess)
		goto exit;

	if (!bHasLocalStore) {
		er = ZARAFA_E_NOT_FOUND;
		goto exit;
	}

	//Check permission
	if (lpecSession->GetSecurity()->IsAdminOverUserObject(ulUserid) != erSuccess) {
		er = ZARAFA_E_NO_ACCESS;
		goto exit;
	}

	/* Not all objectclasses support quota */
	if ((sExternId.objclass == NONACTIVE_CONTACT) ||
		(OBJECTCLASS_TYPE(sExternId.objclass) == OBJECTTYPE_DISTLIST) ||
		(sExternId.objclass == CONTAINER_ADDRESSLIST)) {
			er = ZARAFA_E_INVALID_TYPE;
			goto exit;
	}

	er = lpecSession->GetUserManagement()->GetObjectDetails(ulUserid, &details);
	if (er != erSuccess)
		goto exit;

	if (OBJECTCLASS_TYPE(details.GetClass())== OBJECTTYPE_MAILUSER) {
		ulCompanyId = details.GetPropInt(OB_PROP_I_COMPANYID);
		relation = OBJECTRELATION_QUOTA_USERRECIPIENT;
	} else if (details.GetClass() == CONTAINER_COMPANY) {
		ulCompanyId = ulUserid;
		relation = OBJECTRELATION_QUOTA_COMPANYRECIPIENT;
	} else {
		er = ZARAFA_E_INVALID_TYPE;
		goto exit;
	}

	/* When uLCompanyId is 0 then there are no recipient relations we could request,
	 * in that case we should manually allocate the list so it is safe to add the user
	 * to the list. */
	if (ulCompanyId != 0) {
		er = lpecSession->GetUserManagement()->GetSubObjectsOfObjectAndSync(relation, ulCompanyId, &lpUsers);
		if (er != erSuccess)
			goto exit;
	} else
		lpUsers = new std::list<localobjectdetails_t>();

	if (OBJECTCLASS_TYPE(details.GetClass())== OBJECTTYPE_MAILUSER) {
		/* The main recipient (the user over quota) must be the first entry */
		lpUsers->push_front(localobjectdetails_t(ulUserid, details));
	} else if (details.GetClass() == CONTAINER_COMPANY) {
		/* Append the system administrator for the company */
		unsigned int ulSystem;
		objectdetails_t systemdetails;

		ulSystem = details.GetPropInt(OB_PROP_I_SYSADMIN);
		er = lpecSession->GetSecurity()->IsUserObjectVisible(ulSystem);
		if (er != erSuccess)
			goto exit;

		er = lpecSession->GetUserManagement()->GetObjectDetails(ulSystem, &systemdetails);
		if (er != erSuccess)
			goto exit;

		lpUsers->push_front(localobjectdetails_t(ulSystem, systemdetails));

		/* The main recipient (the company's public store) must be the first entry */
		lpUsers->push_front(localobjectdetails_t(ulUserid, details));
	}

	lpsUserList->sUserArray.__size = 0;
	lpsUserList->sUserArray.__ptr = s_alloc<user>(soap, lpUsers->size());

	for(iterUsers = lpUsers->begin(); iterUsers != lpUsers->end(); iterUsers++) {
		if ((OBJECTCLASS_TYPE(iterUsers->GetClass()) != OBJECTTYPE_MAILUSER) ||
			(details.GetClass() == NONACTIVE_CONTACT))
				continue;

		if (lpecSession->GetSecurity()->IsUserObjectVisible(iterUsers->ulId) != erSuccess)
			continue;

		er = GetABEntryID(iterUsers->ulId, soap, &sUserEid);
		if (er != erSuccess)
			goto exit;

		er = CopyUserDetailsToSoap(iterUsers->ulId, &sUserEid, *iterUsers, soap, &lpsUserList->sUserArray.__ptr[lpsUserList->sUserArray.__size]);
		if (er != erSuccess)
			goto exit;

		// 6.40.0 stores the object class in the IsNonActive field
		if (lpecSession->ClientVersion() == ZARAFA_VERSION_6_40_0)
			lpsUserList->sUserArray.__ptr[lpsUserList->sUserArray.__size].ulIsNonActive = lpsUserList->sUserArray.__ptr[lpsUserList->sUserArray.__size].ulObjClass;

		if (!bSupportUnicode) {
			er = FixUserEncoding(soap, stringCompat, Out, lpsUserList->sUserArray.__ptr + lpsUserList->sUserArray.__size);
			if (er != erSuccess)
				goto exit;
		}

		lpsUserList->sUserArray.__size++;

		if (sUserEid.__ptr)
		{
			// sUserEid is placed in userdetails, no need to free
			sUserEid.__ptr = NULL;
			sUserEid.__size = 0;
		}
	}

exit:
	if(lpUsers)
		delete lpUsers;
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(GetQuotaStatus, lpsQuotaStatus->er, unsigned int ulUserid, entryId sUserId, struct quotaStatus* lpsQuotaStatus)
{
	quotadetails_t	quotadetails;
	long long		llStoreSize = 0;
	objectid_t		sExternId;
	bool			bHasLocalStore = false;

	eQuotaStatus QuotaStatus;

	//Set defaults
	lpsQuotaStatus->llStoreSize = 0;

	// does not return full class in sExternId.objclass
	er = GetLocalId(sUserId, ulUserid, &ulUserid, &sExternId);
	if (er != erSuccess)
		goto exit;

	// re-evaluate userid to externid to get the full class (mapi clients only know MAPI_ABCONT for companies)
	er = g_lpSessionManager->GetCacheManager()->GetUserObject(ulUserid, &sExternId, NULL, NULL);
	if (er != erSuccess)
		goto exit;

	er = CheckUserStore(lpecSession, ulUserid, ECSTORE_TYPE_PRIVATE, &bHasLocalStore);
	if (er != erSuccess)
		goto exit;

	if (!bHasLocalStore) {
		er = ZARAFA_E_NOT_FOUND;
		goto exit;
	}

	// Check permission
	if(lpecSession->GetSecurity()->IsAdminOverUserObject(ulUserid) != erSuccess &&
		(lpecSession->GetSecurity()->GetUserId() != ulUserid))
	{
		er = ZARAFA_E_NO_ACCESS;
		goto exit;
	}

	/* Not all objectclasses support quota */
	if ((sExternId.objclass == NONACTIVE_CONTACT) ||
		(OBJECTCLASS_TYPE(sExternId.objclass) == OBJECTTYPE_DISTLIST) ||
		(sExternId.objclass == CONTAINER_ADDRESSLIST)) {
			er = ZARAFA_E_INVALID_TYPE;
			goto exit;
	}

	if (OBJECTCLASS_TYPE(sExternId.objclass) == OBJECTTYPE_MAILUSER || sExternId.objclass == CONTAINER_COMPANY) {
		er = lpecSession->GetSecurity()->GetUserSize(ulUserid, &llStoreSize);
		if(er != erSuccess)
			goto exit;
	} else {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	// check the store quota status
	er = lpecSession->GetSecurity()->CheckUserQuota(ulUserid, llStoreSize, &QuotaStatus);
	if(er != erSuccess)
		goto exit;

	lpsQuotaStatus->llStoreSize = llStoreSize;
	lpsQuotaStatus->ulQuotaStatus = (unsigned int)QuotaStatus;

exit:
	;
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(getMessageStatus, lpsStatus->er, entryId sEntryId, unsigned int ulFlags, struct messageStatus* lpsStatus)
{
	unsigned int	ulMsgStatus = 0;
	unsigned int	ulId = 0;
	USE_DATABASE();

	er = lpecSession->GetObjectFromEntryId(&sEntryId, &ulId);
	if(er != erSuccess)
	    goto exit;

	//Check security
	er = lpecSession->GetSecurity()->CheckPermission(ulId, ecSecurityRead);
	if(er != erSuccess)
		goto exit;

	// Get the old flags
	strQuery = "SELECT val_ulong FROM properties WHERE hierarchyid="+stringify(ulId)+" AND tag=3607 AND type=3";
	if(lpDatabase->DoSelect(strQuery, &lpDBResult) != erSuccess) {
		er = ZARAFA_E_DATABASE_ERROR;
		goto exit;
	}

	if (lpDatabase->GetNumRows(lpDBResult) == 1) {
		lpDBRow = lpDatabase->FetchRow(lpDBResult);
		if(lpDBRow == NULL || lpDBRow[0] == NULL) {
			er = ZARAFA_E_DATABASE_ERROR;
			goto exit;
		}

		ulMsgStatus = atoui(lpDBRow[0]);
	}

	lpsStatus->ulMessageStatus = ulMsgStatus;

exit:
    FREE_DBRESULT();
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(setMessageStatus, lpsOldStatus->er, entryId sEntryId, unsigned int ulNewStatus, unsigned int ulNewStatusMask, unsigned int ulSyncId, struct messageStatus* lpsOldStatus)
{
	unsigned int	ulOldMsgStatus = 0;
	unsigned int	ulNewMsgStatus = 0;
	unsigned int	ulRows = 0;
	unsigned int	ulId = 0;
	unsigned int	ulParent = 0;
	unsigned int	ulObjFlags = 0;
	
	SOURCEKEY		sSourceKey;
	SOURCEKEY		sParentSourceKey;

	USE_DATABASE();

	er = lpecSession->GetObjectFromEntryId(&sEntryId, &ulId);
	if(er != erSuccess)
	    goto exit;

	//Check security
	er = lpecSession->GetSecurity()->CheckPermission(ulId, ecSecurityEdit);
	if(er != erSuccess)
		goto exit;

	er = lpDatabase->Begin();
	if(er != erSuccess)
		goto exit;

	er = g_lpSessionManager->GetCacheManager()->GetObject(ulId, &ulParent, NULL, &ulObjFlags);
	if(er != erSuccess)
		goto exit;

	// Get the old flags (PR_MSG_STATUS)
	strQuery = "SELECT val_ulong FROM properties WHERE hierarchyid="+stringify(ulId)+" AND tag=3607 AND type=3";
	if(lpDatabase->DoSelect(strQuery, &lpDBResult) != erSuccess) {
		er = ZARAFA_E_DATABASE_ERROR;
		goto exit;
	}

	ulRows = lpDatabase->GetNumRows(lpDBResult);
	if (ulRows == 1) {
		lpDBRow = lpDatabase->FetchRow(lpDBResult);
		if(lpDBRow == NULL || lpDBRow[0] == NULL) {
			er = ZARAFA_E_DATABASE_ERROR;
			goto exit;
		}

		ulOldMsgStatus = atoui(lpDBRow[0]);
	}

	// Set the new flags
	ulNewMsgStatus = (ulOldMsgStatus &~ulNewStatusMask) | (ulNewStatusMask & ulNewStatus);

	if(ulRows > 0){
		strQuery = "UPDATE properties SET val_ulong="+stringify(ulNewMsgStatus)+" WHERE  hierarchyid="+stringify(ulId)+" AND tag=3607 AND type=3";
		er = lpDatabase->DoUpdate(strQuery);
	}else {
		strQuery = "INSERT INTO properties(hierarchyid, tag, type, val_ulong) VALUES("+stringify(ulId)+", 3607, 3,"+stringify(ulNewMsgStatus)+")";
		er = lpDatabase->DoInsert(strQuery);
	}

	if(er != erSuccess) {
		er = ZARAFA_E_DATABASE_ERROR;
		goto exit;
	}

	lpsOldStatus->ulMessageStatus = ulOldMsgStatus;

	GetSourceKey(ulId, &sSourceKey);
	GetSourceKey(ulParent, &sParentSourceKey);

	AddChange(lpecSession, ulSyncId, sSourceKey, sParentSourceKey, ICS_MESSAGE_CHANGE);

	er = lpDatabase->Commit();
	if(er != erSuccess)
		goto exit;

	// Now, send the notifications
	g_lpSessionManager->GetCacheManager()->Update(fnevObjectModified, ulId);
	g_lpSessionManager->NotificationModified(MAPI_MESSAGE, ulId, ulParent);
	g_lpSessionManager->UpdateTables(ECKeyTable::TABLE_ROW_MODIFY, ulObjFlags&MSGFLAG_NOTIFY_FLAGS, ulParent, ulId, MAPI_MESSAGE);

exit:
    ROLLBACK_ON_ERROR();
    FREE_DBRESULT();
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(getChanges, lpsChangesResponse->er, struct xsd__base64Binary sSourceKeyFolder, unsigned int ulSyncId, unsigned int ulChangeId, unsigned int ulChangeType, unsigned int ulFlags, struct restrictTable *lpsRestrict, struct icsChangeResponse* lpsChangesResponse)
{
	icsChangesArray *lpChanges = NULL;
	SOURCEKEY		sSourceKey(sSourceKeyFolder.__size, (char *)sSourceKeyFolder.__ptr);

	er = GetChanges(soap, lpecSession, sSourceKey, ulSyncId, ulChangeId, ulChangeType, ulFlags, lpsRestrict, &lpsChangesResponse->ulMaxChangeId, &lpChanges);
	if(er != erSuccess)
		goto exit;

	lpsChangesResponse->sChangesArray = *lpChanges;

exit:
    ;
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(setSyncStatus, lpsResponse->er, struct xsd__base64Binary sSourceKeyFolder, unsigned int ulSyncId, unsigned int ulChangeId, unsigned int ulChangeType, unsigned int ulFlags, struct setSyncStatusResponse *lpsResponse)
{
	SOURCEKEY		sSourceKey(sSourceKeyFolder.__size, (char *)sSourceKeyFolder.__ptr);
	unsigned int	ulFolderId = 0;
	USE_DATABASE();

	er = lpDatabase->Begin();
	if (er != erSuccess)
		goto exit;

    if(sSourceKey.size()) {
    	er = g_lpSessionManager->GetCacheManager()->GetObjectFromProp(PROP_ID(PR_SOURCE_KEY), sSourceKey.size(), sSourceKey, &ulFolderId);
    	if(er != erSuccess)
	        goto exit;
    } else {
        ulFolderId = 0;
    }
    
    if(ulFolderId == 0) {
        if(lpecSession->GetSecurity()->GetAdminLevel() != ADMIN_LEVEL_SYSADMIN)
            er = ZARAFA_E_NO_ACCESS;
    } else {
        //Check security
        if(ulChangeType == ICS_SYNC_CONTENTS){
            er = lpecSession->GetSecurity()->CheckPermission(ulFolderId, ecSecurityRead);
        }else if(ulChangeType == ICS_SYNC_HIERARCHY){
            er = lpecSession->GetSecurity()->CheckPermission(ulFolderId, ecSecurityFolderVisible);
        }else{
            er = ZARAFA_E_INVALID_TYPE;
        }
    }
	if(er != erSuccess)
		goto exit;

	if(ulSyncId == 0){
	    // SyncID is 0, which means the client will be requesting an initial sync from this new sync. The change_id will
		// be updated in another call done when the synchronization is complete.
		strQuery = "INSERT INTO syncs (change_id, sourcekey, sync_type, sync_time) VALUES (1, "+lpDatabase->EscapeBinary(sSourceKey, sSourceKey.size())+", '" + stringify(ulChangeType) + "', FROM_UNIXTIME("+stringify(time(NULL))+"))";
		er = lpDatabase->DoInsert(strQuery, &ulSyncId);
		if (er == erSuccess) {
			er = lpDatabase->Commit();
			lpsResponse->ulSyncId = ulSyncId;
		}
		goto exit;
	}

	strQuery = "SELECT sourcekey, change_id, sync_type FROM syncs WHERE id ="+stringify(ulSyncId)+" FOR UPDATE";
	//TODO check existing sync
	er = lpDatabase->DoSelect(strQuery, &lpDBResult);
	if(er != erSuccess)
		goto exit;

	if(lpDatabase->GetNumRows(lpDBResult) == 0){
		er = ZARAFA_E_NOT_FOUND;
		goto exit;
	}

	lpDBRow = lpDatabase->FetchRow(lpDBResult);
	lpDBLen = lpDatabase->FetchRowLengths(lpDBResult);

	if( lpDBRow == NULL || lpDBRow[0] == NULL || lpDBRow[1] == NULL || lpDBRow[2] == NULL){
		er = ZARAFA_E_DATABASE_ERROR; // this should never happen
		goto exit;
	}

	if(lpDBLen[0] != sSourceKey.size() || memcmp(lpDBRow[0], sSourceKey, sSourceKey.size()) != 0){
		er = ZARAFA_E_COLLISION;
		goto exit;
	}

	if(atoui(lpDBRow[2]) != ulChangeType){
		er = ZARAFA_E_COLLISION;
		goto exit;
	}

	strQuery = "UPDATE syncs SET change_id = "+stringify(ulChangeId)+", sync_time = FROM_UNIXTIME("+stringify(time(NULL))+") WHERE id = "+stringify(ulSyncId);
	er = lpDatabase->DoUpdate(strQuery);
	if(er != erSuccess)
		goto exit;

	er = lpDatabase->Commit();
	if (er != erSuccess)
		goto exit;

	lpsResponse->ulSyncId = ulSyncId;
exit:
	ROLLBACK_ON_ERROR();
    FREE_DBRESULT();
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(getEntryIDFromSourceKey, lpsResponse->er, entryId sStoreId, struct xsd__base64Binary folderSourceKey, struct xsd__base64Binary messageSourceKey, struct getEntryIDFromSourceKeyResponse *lpsResponse)
{
	unsigned int	ulObjType = 0;
	unsigned int	ulObjId = 0;
	unsigned int	ulMessageId = 0;
	unsigned int	ulFolderId = 0;
	unsigned int	ulParent = 0;
	unsigned int	ulStoreId = 0;
	unsigned int	ulStoreFound = 0;
	EID				eid;
	
	USE_DATABASE();

	er = BeginLockFolders(lpDatabase, SOURCEKEY(folderSourceKey), LOCK_SHARED);
	if(er != erSuccess)
	    goto exit;
	
	er = lpecSession->GetObjectFromEntryId(&sStoreId, &ulStoreId);
	if(er != erSuccess)
	    goto exit;

	er = g_lpSessionManager->GetCacheManager()->GetObjectFromProp(PROP_ID(PR_SOURCE_KEY), folderSourceKey.__size, folderSourceKey.__ptr, &ulFolderId);
	if(er != erSuccess)
		goto exit;
		
	if(messageSourceKey.__size != 0) {
		er = g_lpSessionManager->GetCacheManager()->GetObjectFromProp(PROP_ID(PR_SOURCE_KEY), messageSourceKey.__size, messageSourceKey.__ptr, &ulMessageId);
		if(er != erSuccess)
			goto exit;

        // Check if given sourcekey is in the given parent sourcekey
		er = g_lpSessionManager->GetCacheManager()->GetParent(ulMessageId, &ulParent);
		if(er != erSuccess || ulFolderId != ulParent) {
			er = ZARAFA_E_NOT_FOUND;
			goto exit;
		}
		
		
		ulObjId = ulMessageId;
		ulObjType = MAPI_MESSAGE;

	} else {
		ulObjId = ulFolderId;
		ulObjType = MAPI_FOLDER;
	}

	// Check if the folder given is actually in the store we're working on (may not be so if cache
	// is out-of-date during a re-import of a store that has been deleted and re-imported). In this case
	// we return NOT FOUND, which really is true since we cannot found the given sourcekey in this store.
    er = g_lpSessionManager->GetCacheManager()->GetStore(ulFolderId, &ulStoreFound, NULL);
    if(er != erSuccess || ulStoreFound != ulStoreId) {
        er = ZARAFA_E_NOT_FOUND;
        goto exit;
    }

	// Check security
	if(ulObjType == MAPI_FOLDER){
		er = lpecSession->GetSecurity()->CheckPermission(ulObjId, ecSecurityFolderVisible);
	}else{
		er = lpecSession->GetSecurity()->CheckPermission(ulObjId, ecSecurityRead);
	}
	if(er != erSuccess)
		goto exit;

	er = g_lpSessionManager->GetCacheManager()->GetEntryIdFromObject(ulObjId, soap, 0, &lpsResponse->sEntryId);
	if(er != erSuccess)
		goto exit;

exit:
    lpDatabase->Commit();
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(getSyncStates, lpsResponse->er, struct mv_long ulaSyncId, struct getSyncStatesReponse *lpsResponse)
{
	er = GetSyncStates(soap, lpecSession, ulaSyncId, &lpsResponse->sSyncStates);
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(getLicenseAuth, lpsResponse->er, struct xsd__base64Binary sAuthData, struct getLicenseAuthResponse *lpsResponse)
{
    ECLicenseClient *lpLicenseClient = new ECLicenseClient(g_lpSessionManager->GetConfig()->GetSetting("license_socket"), atoui(g_lpSessionManager->GetConfig()->GetSetting("license_timeout")));
	unsigned char *data = NULL;

    er = lpLicenseClient->Auth(sAuthData.__ptr, sAuthData.__size, &data, (unsigned int *)&lpsResponse->sAuthResponse.__size);
    if(er != erSuccess)
        goto exit;

	lpsResponse->sAuthResponse.__ptr = s_alloc<unsigned char>(soap, lpsResponse->sAuthResponse.__size);
	memcpy(lpsResponse->sAuthResponse.__ptr, data, lpsResponse->sAuthResponse.__size);

exit:
	if (data)
		delete [] data;

    delete lpLicenseClient;
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(getLicenseCapa, lpsResponse->er, unsigned int ulServiceType, struct getLicenseCapaResponse *lpsResponse)
{
    ECLicenseClient *lpLicenseClient = new ECLicenseClient(g_lpSessionManager->GetConfig()->GetSetting("license_socket"), atoui(g_lpSessionManager->GetConfig()->GetSetting("license_timeout")));
    std::vector<std::string> lstCapabilities;

    er = lpLicenseClient->GetCapabilities(ulServiceType, lstCapabilities);
    if(er != erSuccess)
        goto exit;
        
    lpsResponse->sCapabilities.__size = lstCapabilities.size();
    lpsResponse->sCapabilities.__ptr = s_alloc<char *>(soap, lstCapabilities.size());
    
    for(unsigned int i=0; i<lstCapabilities.size(); i++) {
        lpsResponse->sCapabilities.__ptr[i] = s_strcpy(soap, lstCapabilities[i].c_str());
    }
exit:

    delete lpLicenseClient;
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(getLicenseUsers, lpsResponse->er, unsigned int ulServiceType, struct getLicenseUsersResponse *lpsResponse)
{
	unsigned int ulUsers = 0;

	ECLicenseClient *lpLicenseClient = new ECLicenseClient(g_lpSessionManager->GetConfig()->GetSetting("license_socket"), atoui(g_lpSessionManager->GetConfig()->GetSetting("license_timeout")));
	std::vector<std::string> lstCapabilities;

	er = lpLicenseClient->GetInfo(ulServiceType, &ulUsers);
	if(er != erSuccess)
		goto exit;

	lpsResponse->ulUsers = ulUsers;

exit:

	delete lpLicenseClient;
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(resolvePseudoUrl, lpsResponse->er, char *lpszPseudoUrl, struct resolvePseudoUrlResponse* lpsResponse)
{
	std::string		strServerPath;

	if (!lpecSession->GetSessionManager()->IsDistributedSupported())
	{
		/**
         * Non distributed environments do issue pseudo url's, but merely to 
         * make upgrading later possible. We'll just return the passed pseudo url
		 * and say that we're the peer. That would cause the client to keep on
		 * using the current connection.
         **/
		lpsResponse->lpszServerPath = lpszPseudoUrl;
		lpsResponse->bIsPeer = true;
		goto exit;
	}

	lpszPseudoUrl = STRIN_FIX(lpszPseudoUrl);

	if (strncmp(lpszPseudoUrl, "pseudo://", 9))
	{
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}
	
	er = GetBestServerPath(soap, lpecSession, lpszPseudoUrl + 9, &strServerPath);
	if (er != erSuccess)
		goto exit;

	lpsResponse->lpszServerPath = STROUT_FIX_CPY(strServerPath.c_str());
	lpsResponse->bIsPeer = stricmp(g_lpSessionManager->GetConfig()->GetSetting("server_name"), lpszPseudoUrl + 9) == 0;

exit:
	;
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(getServerDetails, lpsResponse->er, struct mv_string8 szaSvrNameList, unsigned int ulFlags, struct getServerDetailsResponse* lpsResponse)
{
	serverdetails_t	sDetails;
	std::string		strServerPath;
	std::string		strPublicServer;
	objectdetails_t	details;

	if (!lpecSession->GetSessionManager()->IsDistributedSupported())
	{
		/**
		 * We want to pretend the method doesn't exist. The best way to do that is to return
		 * SOAP_NO_METHOD. But that doesn't fit well in the macro famework. And since the
		 * client would translate that to a ZARAFA_E_NETWORK_ERROR anyway, we'll just return
		 * that instead.
		 **/
		er = ZARAFA_E_NETWORK_ERROR;
		goto exit;
	}

	// lookup server which contains the public
	if (lpecSession->GetUserManagement()->GetPublicStoreDetails(&details) == erSuccess)
		strPublicServer = details.GetPropString(OB_PROP_S_SERVERNAME);

	if (ulFlags & ~(EC_SERVERDETAIL_NO_NAME|EC_SERVERDETAIL_FILEPATH|EC_SERVERDETAIL_HTTPPATH|EC_SERVERDETAIL_SSLPATH|EC_SERVERDETAIL_PREFEREDPATH)) {
		er = ZARAFA_E_UNKNOWN_FLAGS;
		goto exit;
	}
	
	if (ulFlags == 0)
		ulFlags = EC_SERVERDETAIL_FILEPATH|EC_SERVERDETAIL_HTTPPATH|EC_SERVERDETAIL_SSLPATH	|EC_SERVERDETAIL_PREFEREDPATH;
	
	if (szaSvrNameList.__size > 0 && szaSvrNameList.__ptr != NULL) {
		lpsResponse->sServerList.__size = szaSvrNameList.__size;
		lpsResponse->sServerList.__ptr = s_alloc<struct server>(soap, szaSvrNameList.__size);
		memset(lpsResponse->sServerList.__ptr, 0, szaSvrNameList.__size * sizeof *lpsResponse->sServerList.__ptr);
		
		for (int i = 0; i < szaSvrNameList.__size; ++i) {
			er = lpecSession->GetUserManagement()->GetServerDetails(STRIN_FIX(szaSvrNameList.__ptr[i]), &sDetails);
			if (er != erSuccess)
				goto exit;

			
			if (stricmp(sDetails.GetServerName().c_str(), g_lpSessionManager->GetConfig()->GetSetting("server_name")) == 0)
				lpsResponse->sServerList.__ptr[i].ulFlags |= EC_SDFLAG_IS_PEER;

			// note: "contains a public of a company" is also a possibility
			if (!strPublicServer.empty() && stricmp(sDetails.GetServerName().c_str(), strPublicServer.c_str()) == 0)
				lpsResponse->sServerList.__ptr[i].ulFlags |= EC_SDFLAG_HAS_PUBLIC;
				
			if ((ulFlags & EC_SERVERDETAIL_NO_NAME) != EC_SERVERDETAIL_NO_NAME)
				lpsResponse->sServerList.__ptr[i].lpszName = STROUT_FIX_CPY(sDetails.GetServerName().c_str());
				
			if ((ulFlags & EC_SERVERDETAIL_FILEPATH) == EC_SERVERDETAIL_FILEPATH)
				lpsResponse->sServerList.__ptr[i].lpszFilePath = STROUT_FIX_CPY(sDetails.GetFilePath().c_str());
				
			if ((ulFlags & EC_SERVERDETAIL_HTTPPATH) == EC_SERVERDETAIL_HTTPPATH)
				lpsResponse->sServerList.__ptr[i].lpszHttpPath = STROUT_FIX_CPY(sDetails.GetHttpPath().c_str());
				
			if ((ulFlags & EC_SERVERDETAIL_SSLPATH) == EC_SERVERDETAIL_SSLPATH)
				lpsResponse->sServerList.__ptr[i].lpszSslPath = STROUT_FIX_CPY(sDetails.GetSslPath().c_str());
				
			if ((ulFlags & EC_SERVERDETAIL_PREFEREDPATH) == EC_SERVERDETAIL_PREFEREDPATH) {
				if (GetBestServerPath(soap, lpecSession, sDetails.GetServerName(), &strServerPath) == erSuccess)
					lpsResponse->sServerList.__ptr[i].lpszPreferedPath = STROUT_FIX_CPY(strServerPath.c_str());
			}
		}
	}
exit:
	;
}
SOAP_ENTRY_END()

// legacy calls required for 6.30 clients
SOAP_ENTRY_START(getServerBehavior, lpsResponse->er, struct getServerBehaviorResponse* lpsResponse) 
{ 
    lpsResponse->ulBehavior = 1; 
} 
SOAP_ENTRY_END() 
 
SOAP_ENTRY_START(setServerBehavior, *result, unsigned int ulBehavior, unsigned int *result) 
{ 
    er = ZARAFA_E_NO_SUPPORT; 
} 
SOAP_ENTRY_END() 

typedef struct {
	ECSession		*lpecSession;
	ECDatabase		*lpSharedDatabase;
	ECDatabase		*lpDatabase;
	ECAttachmentStorage *lpAttachmentStorage;
	ECRESULT er;
	ECThreadPool	*lpThreadPool;
} MTOMSessionInfo;

typedef struct {
	ECFifoBuffer	*lpFifoBuffer;
	unsigned int	ulObjectId;
	unsigned int	ulStoreId;
	bool			bNewItem;
	unsigned long long ullIMAP;
	GUID			sGuid;
	ULONG			ulFlags;
	ECWaitableTask  *lpTask;
	struct propValArray *lpPropValArray;
	MTOMSessionInfo *lpSessionInfo;
} MTOMStreamInfo;


typedef MTOMStreamInfo * LPMTOMStreamInfo;

void *SerializeObject(void *arg)
{
	ECRESULT            er = erSuccess;
	LPMTOMStreamInfo	lpStreamInfo = NULL;
	ECSerializer		*lpSink = NULL;

	lpStreamInfo = (LPMTOMStreamInfo)arg;
	ASSERT(lpStreamInfo != NULL);

	lpStreamInfo->lpSessionInfo->lpSharedDatabase->ThreadInit();

	lpSink = new ECFifoSerializer(lpStreamInfo->lpFifoBuffer, ECFifoSerializer::serialize);
	er = SerializeObject(lpStreamInfo->lpSessionInfo->lpecSession, lpStreamInfo->lpSessionInfo->lpSharedDatabase, lpStreamInfo->lpSessionInfo->lpAttachmentStorage, NULL, lpStreamInfo->ulObjectId, MAPI_MESSAGE, lpStreamInfo->ulStoreId, &lpStreamInfo->sGuid, lpStreamInfo->ulFlags, lpSink, true);

	delete lpSink;

	lpStreamInfo->lpSessionInfo->lpSharedDatabase->ThreadEnd();

	lpStreamInfo->lpSessionInfo->er = er;

	return NULL;
}

void *MTOMReadOpen(struct soap* soap, void *handle, const char *id, const char* /*type*/, const char* /*options*/)
{
	typedef ECDeferredFunc<void*, void*(*)(void*), void*> task_type;

	LPMTOMStreamInfo	lpStreamInfo = NULL;

	lpStreamInfo = (LPMTOMStreamInfo)handle;
	ASSERT(lpStreamInfo != NULL);

	if (lpStreamInfo->lpSessionInfo->er != erSuccess) {
		soap->error = SOAP_FATAL_ERROR;
		return NULL;
	}

	lpStreamInfo->lpFifoBuffer = new ECFifoBuffer();

	if (strncmp(id, "emcas-", 6) == 0) {
		std::auto_ptr<task_type> ptrTask(new task_type(SerializeObject, lpStreamInfo));
		if (ptrTask->dispatchOn(lpStreamInfo->lpSessionInfo->lpThreadPool) == false) {
			g_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_ERROR, "Failed to dispatch serialization task for '%s'", id);
			soap->error = SOAP_FATAL_ERROR;

			delete lpStreamInfo->lpFifoBuffer;
			lpStreamInfo->lpFifoBuffer = NULL;
			return NULL;
		}
		lpStreamInfo->lpTask = ptrTask.release();
	} else {
		g_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_ERROR, "Got stream request for unknown id: '%s'", id);
		soap->error = SOAP_FATAL_ERROR;

		delete lpStreamInfo->lpFifoBuffer;
		lpStreamInfo->lpFifoBuffer = NULL;
		return NULL;
	}
	
	return (void *)lpStreamInfo;
}

size_t MTOMRead(struct soap* /*soap*/, void *handle, char *buf, size_t len)
{
	ECRESULT			er = erSuccess;
	LPMTOMStreamInfo		lpStreamInfo = (LPMTOMStreamInfo)handle;
	ECFifoBuffer::size_type	cbRead = 0;
	
	ASSERT(lpStreamInfo->lpFifoBuffer != NULL);

	er = lpStreamInfo->lpFifoBuffer->Read(buf, len, STR_DEF_TIMEOUT, &cbRead);
	if (er != erSuccess) {
		g_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_FATAL, "Failed to read data. er=%s", stringify(er).c_str());
	}
	
	return cbRead;
}
void MTOMReadClose(struct soap* soap, void *handle)
{ 
	LPMTOMStreamInfo		lpStreamInfo = (LPMTOMStreamInfo)handle;
	
	ASSERT(lpStreamInfo->lpFifoBuffer != NULL);

	// We get here when the last call to MTOMRead returned 0 OR when
	// an error occured within gSOAP's bowels. In the last case we need
	// to close the FIFO to make sure the writing thread won't lock up.
	// Since gSOAP won't be reading from the FIFO in any case once we
	// read this point it's safe to just close the FIFO.
	lpStreamInfo->lpFifoBuffer->Close(ECFifoBuffer::cfRead);
	if (lpStreamInfo->lpTask) {
		lpStreamInfo->lpTask->wait();	 // Todo: use result() to wait and get result
		delete lpStreamInfo->lpTask;
	}
	delete lpStreamInfo->lpFifoBuffer;
	lpStreamInfo->lpFifoBuffer = NULL;
}

void MTOMSessionDone(void *param)
{
	MTOMSessionInfo *lpInfo = (MTOMSessionInfo *)param;

	lpInfo->lpAttachmentStorage->Release();
	lpInfo->lpecSession->Unlock();
	delete lpInfo->lpSharedDatabase;
	delete lpInfo->lpThreadPool;
	delete lpInfo;
}

SOAP_ENTRY_START(exportMessageChangesAsStream, lpsResponse->er, unsigned int ulFlags, struct propTagArray sPropTags, struct sourceKeyPairArray sSourceKeyPairs, unsigned int ulPropTag, exportMessageChangesAsStreamResponse *lpsResponse)
{
	LPMTOMStreamInfo	lpStreamInfo = NULL;
	ECAttachmentStorage *lpAttachmentStorage = NULL;
	unsigned int		ulObjectId = 0;
	unsigned int		ulParentId = 0;
	unsigned int		ulParentCheck = 0;
	unsigned int		ulObjFlags = 0;
	unsigned int		ulStoreId = 0;
	unsigned long		ulObjCnt = 0;
	GUID				sGuid;
	ECObjectTableList	rows;
	struct rowSet		*lpRowSet = NULL; // Do not free, used in response data
	ECODStore			ecODStore;
	ECDatabase 			*lpBatchDB;
	unsigned int		ulDepth = 20;
	unsigned int		ulMode = 0;
	MTOMSessionInfo		*lpMTOMSessionInfo = NULL;
	bool				bUseSQLMulti = parseBool(g_lpSessionManager->GetConfig()->GetSetting("enable_sql_procedures"));

	// Backward compat, old clients do not send ulPropTag
	if(!ulPropTag)
	    ulPropTag = PR_SOURCE_KEY;
	
	if(ulFlags & SYNC_BEST_BODY)
	  ulMode = 1;
	else if(ulFlags & SYNC_LIMITED_IMESSAGE)
	  ulMode = 2;
	
	USE_DATABASE();

	if(ulPropTag == PR_ENTRYID) {
		std::set<EntryId>	setEntryIDs;

    	for(unsigned i = 0; i < sSourceKeyPairs.__size; ++i)
	        setEntryIDs.insert(EntryId(sSourceKeyPairs.__ptr[i].sObjectKey));

    	er = BeginLockFolders(lpDatabase, setEntryIDs, LOCK_SHARED);
	} else if (ulPropTag == PR_SOURCE_KEY) {
		std::set<SOURCEKEY> setParentSourcekeys;

    	for(unsigned i = 0; i < sSourceKeyPairs.__size; ++i)
	        setParentSourcekeys.insert(SOURCEKEY(sSourceKeyPairs.__ptr[i].sParentKey));

    	er = BeginLockFolders(lpDatabase, setParentSourcekeys, LOCK_SHARED);
    } else
        er = ZARAFA_E_INVALID_PARAMETER;
    
    if (er == ZARAFA_E_NOT_FOUND) {
		// BeginLockFolders returns ZARAFA_E_NOT_FOUND when there's no
		// folder to lock, which can only happen if none of the passed
		// objects exist.
		// This is not an error as that's perfectly valid when performing
		// a selective export. So we'll just return an empty batch, which
		// will be interpreted by the caller as 'all message are deleted'.
		er = erSuccess;
		goto exit;
	} else if (er != erSuccess)
		goto exit;

	ulDepth = atoui(lpecSession->GetSessionManager()->GetConfig()->GetSetting("embedded_attachment_limit"));
	
	er = lpecSession->GetAdditionalDatabase(&lpBatchDB);
	if (er != erSuccess)
	    goto exit;
	    
	if ((lpecSession->GetCapabilities() & ZARAFA_CAP_ENHANCED_ICS) == 0) {
		er = ZARAFA_E_NO_SUPPORT;
		goto exit;
	}

	er = CreateAttachmentStorage(lpDatabase, &lpAttachmentStorage);
	if (er != erSuccess)
		goto exit;

	lpMTOMSessionInfo = new MTOMSessionInfo;
	lpMTOMSessionInfo->lpAttachmentStorage = lpAttachmentStorage;
	lpMTOMSessionInfo->lpAttachmentStorage->AddRef();
	lpMTOMSessionInfo->lpecSession = lpecSession; // Should be unlocked after MTOM is done
	lpMTOMSessionInfo->lpecSession->Lock();
	lpMTOMSessionInfo->lpSharedDatabase = lpBatchDB;
	lpMTOMSessionInfo->er = erSuccess;
	lpMTOMSessionInfo->lpThreadPool = new ECThreadPool(1);
	
	((SOAPINFO *)soap->user)->fdone = MTOMSessionDone;
	((SOAPINFO *)soap->user)->fdoneparam = lpMTOMSessionInfo;

	lpsResponse->sMsgStreams.__ptr = s_alloc<messageStream>(soap, sSourceKeyPairs.__size);

	for (unsigned i = 0; i < sSourceKeyPairs.__size; ++i) {
		// Progress information
		lpsResponse->sMsgStreams.__ptr[ulObjCnt].ulStep = i;			

		// Find the correct object
		er = g_lpSessionManager->GetCacheManager()->GetObjectFromProp(PROP_ID(ulPropTag), 
																	sSourceKeyPairs.__ptr[i].sObjectKey.__size, 
																	sSourceKeyPairs.__ptr[i].sObjectKey.__ptr, &ulObjectId);
		if(er != erSuccess) {
		    er = erSuccess;
			goto next_object;
        }
        
        if(sSourceKeyPairs.__ptr[i].sParentKey.__size) {
            er = g_lpSessionManager->GetCacheManager()->GetObjectFromProp(PROP_ID(ulPropTag), 
                                                                        sSourceKeyPairs.__ptr[i].sParentKey.__size, 
                                                                        sSourceKeyPairs.__ptr[i].sParentKey.__ptr, &ulParentId);
            if(er != erSuccess) {
                er = erSuccess;
                goto next_object;
            }

            er = g_lpSessionManager->GetCacheManager()->GetObject(ulObjectId, &ulParentCheck, NULL, &ulObjFlags, NULL);
            if (er != erSuccess) {
                er = erSuccess;
                goto next_object;
            }
            
            if (ulParentId != ulParentCheck) {
                ASSERT(false);
                goto next_object;
            }
        }
        		
		if ((ulObjFlags & MSGFLAG_DELETED) != (ulFlags & MSGFLAG_DELETED)) {
			goto next_object;
		}
		
		// Check security
		er = lpecSession->GetSecurity()->CheckPermission(ulObjectId, ecSecurityRead);
		if (er != erSuccess) {
		    er = erSuccess;
			goto next_object;
        }
        
		// Get store
		er = g_lpSessionManager->GetCacheManager()->GetStore(ulObjectId, &ulStoreId, &sGuid);
		if(er != erSuccess) {
		    er = erSuccess;
			goto next_object;
        }
        

		lpStreamInfo = (MTOMStreamInfo *)soap_malloc(soap, sizeof(MTOMStreamInfo));
		lpStreamInfo->ulObjectId = ulObjectId;
		lpStreamInfo->ulStoreId = ulStoreId;
		lpStreamInfo->bNewItem = false;
		lpStreamInfo->ullIMAP = 0;
		lpStreamInfo->sGuid = sGuid;
		lpStreamInfo->ulFlags = ulFlags;
		lpStreamInfo->lpPropValArray = NULL;
		lpStreamInfo->lpTask = NULL;
		lpStreamInfo->lpSessionInfo = lpMTOMSessionInfo;

		if(bUseSQLMulti)
			strQuery += "call StreamObj(" + stringify(ulObjectId) + "," + stringify(ulDepth) + ", " + stringify(ulMode) + ");";
		
		// Setup the MTOM Attachments
		memset(&lpsResponse->sMsgStreams.__ptr[ulObjCnt].sStreamData, 0, sizeof(lpsResponse->sMsgStreams.__ptr[ulObjCnt].sStreamData));
		lpsResponse->sMsgStreams.__ptr[ulObjCnt].sStreamData.xop__Include.__ptr = (unsigned char*)lpStreamInfo;
		lpsResponse->sMsgStreams.__ptr[ulObjCnt].sStreamData.xop__Include.type = s_strcpy(soap, "application/binary");
		lpsResponse->sMsgStreams.__ptr[ulObjCnt].sStreamData.xop__Include.id = s_strcpy(soap, string("emcas-" + stringify(ulObjCnt, false)).c_str());

		ulObjCnt++;
		
		// Remember the object ID since we need it later
		rows.push_back(sObjectTableKey(ulObjectId, 0));
next_object:
		;
	}
	lpsResponse->sMsgStreams.__size = ulObjCnt;
                    
    // The results of this query will be consumed by the MTOMRead function
    if(!strQuery.empty()) {
        er = lpBatchDB->DoSelectMulti(strQuery);
        if(er != erSuccess)
            goto exit;
    }
                 
    memset(&ecODStore, 0, sizeof(ECODStore));
	ecODStore.ulObjType = MAPI_MESSAGE;
	
	// Get requested properties for all rows
	er = ECStoreObjectTable::QueryRowData(NULL, soap, lpecSession, &rows, &sPropTags, &ecODStore, &lpRowSet, true, true);
	if (er != erSuccess)
	    goto exit;
	    
    ASSERT(lpRowSet->__size == (int)ulObjCnt);
    
    for(int i = 0; i < lpRowSet->__size ; i++) {
		lpsResponse->sMsgStreams.__ptr[i].sPropVals = lpRowSet->__ptr[i];
    }

	soap->fmimereadopen = &MTOMReadOpen;
	soap->fmimeread = &MTOMRead;
	soap->fmimereadclose = &MTOMReadClose;

	g_lpStatsCollector->Increment(SCN_DATABASE_MROPS, (int)ulObjCnt);
	
exit:
    lpDatabase->Commit();
    
	if (er != erSuccess) {
		// Do not output any streams
		lpsResponse->sMsgStreams.__size = 0;
	}

	if(lpAttachmentStorage)
		lpAttachmentStorage->Release();

	FREE_DBRESULT();
}
SOAP_ENTRY_END()

void *DeserializeObject(void *arg)
{
	LPMTOMStreamInfo	lpStreamInfo = NULL;
	ECSerializer		*lpSource = NULL;
	ECRESULT			er = erSuccess;

	lpStreamInfo = (LPMTOMStreamInfo)arg;
	ASSERT(lpStreamInfo != NULL);

	lpSource = new ECFifoSerializer(lpStreamInfo->lpFifoBuffer, ECFifoSerializer::deserialize);
	er = DeserializeObject(lpStreamInfo->lpSessionInfo->lpecSession, lpStreamInfo->lpSessionInfo->lpDatabase, lpStreamInfo->lpSessionInfo->lpAttachmentStorage, NULL, lpStreamInfo->ulObjectId, lpStreamInfo->ulStoreId, &lpStreamInfo->sGuid, lpStreamInfo->bNewItem, lpStreamInfo->ullIMAP, lpSource, &lpStreamInfo->lpPropValArray);
	delete lpSource;

	return (void*)(uintptr_t)er;
}

void *MTOMWriteOpen(struct soap* soap, void *handle, const char * /*id*/, const char* /*type*/, const char* /*description*/, enum soap_mime_encoding /*encoding*/)
{
	typedef ECDeferredFunc<void*, void*(*)(void*), void*> task_type;

	LPMTOMStreamInfo	lpStreamInfo = NULL;
	lpStreamInfo = (LPMTOMStreamInfo)handle;

	// Just return the handle (needed for gsoap to operate properly
	lpStreamInfo->lpFifoBuffer = new ECFifoBuffer();

	std::auto_ptr<task_type> ptrTask(new task_type(DeserializeObject, lpStreamInfo));
	if (ptrTask->dispatchOn(lpStreamInfo->lpSessionInfo->lpThreadPool) == false) {
		g_lpSessionManager->GetLogger()->Log(EC_LOGLEVEL_ERROR, "Failed to dispatch deserialization task");
		soap->error = SOAP_FATAL_ERROR;
		return NULL;
	}
	lpStreamInfo->lpTask = ptrTask.release();

	return handle;
}

int MTOMWrite(struct soap* soap, void *handle, const char *buf, size_t len)
{
	ECRESULT			er = erSuccess;
	LPMTOMStreamInfo	lpStreamInfo = NULL;
	
	lpStreamInfo = (LPMTOMStreamInfo)handle;
	ASSERT(lpStreamInfo != NULL);

	// Only write data if a reader thread is available
	if (lpStreamInfo->lpTask) {
		er = lpStreamInfo->lpFifoBuffer->Write(buf, len, STR_DEF_TIMEOUT, NULL);
		if (er != erSuccess) {
			soap->errnum = (int)er;
			return SOAP_EOF;
		}
	}

	return SOAP_OK;
}

void MTOMWriteClose(struct soap *soap, void *handle)
{
	ECRESULT er = erSuccess;

	LPMTOMStreamInfo	lpStreamInfo = NULL;
	lpStreamInfo = (LPMTOMStreamInfo)handle;
	ASSERT(lpStreamInfo != NULL);

	lpStreamInfo->lpFifoBuffer->Close(ECFifoBuffer::cfWrite);
	if (lpStreamInfo->lpTask) {
		lpStreamInfo->lpTask->wait();	// Todo: use result() to wait and get result
		delete lpStreamInfo->lpTask;
	}
	delete lpStreamInfo->lpFifoBuffer;
	lpStreamInfo->lpFifoBuffer = NULL;

	// Signal error to caller
	if(er != erSuccess)
		soap->error = SOAP_FATAL_ERROR;
}

SOAP_ENTRY_START(importMessageFromStream, *result, unsigned int ulFlags, unsigned int ulSyncId, entryId sFolderEntryId, entryId sEntryId, bool bIsNew, struct propVal *lpsConflictItems, struct xsd__Binary sStreamData, unsigned int *result)
{
	MTOMStreamInfo	*lpsStreamInfo = NULL;
	unsigned int	ulObjectId = 0;
	unsigned int	ulParentId = 0;
	unsigned int	ulParentType = 0;
	unsigned int	ulGrandParentId = 0;
	unsigned int	ulStoreId = 0;
	unsigned int	ulOwner = 0;
	unsigned long long ullIMAP = 0;
	GUID			sGuid = {0};
	SOURCEKEY		sSourceKey;
	SOURCEKEY		sParentSourceKey;
	ECListInt		lObjectList;
	ULONG			nMVItems = 0;
	std::string		strColName;
	std::string		strColData;
	unsigned int	ulAffected = 0;
	unsigned int	ulDeleteFlags = EC_DELETE_ATTACHMENTS | EC_DELETE_RECIPIENTS | EC_DELETE_CONTAINER | EC_DELETE_MESSAGES | EC_DELETE_HARD_DELETE;
	ECListDeleteItems lstDeleteItems;
	ECListDeleteItems lstDeleted;
	ECAttachmentStorage *lpAttachmentStorage = NULL;
	MTOMSessionInfo		*lpMTOMSessionInfo = NULL;

	USE_DATABASE();

	er = CreateAttachmentStorage(lpDatabase, &lpAttachmentStorage);
	if (er != erSuccess)
		goto exit;

	lpMTOMSessionInfo = new MTOMSessionInfo;
	lpMTOMSessionInfo->lpAttachmentStorage = lpAttachmentStorage;
	lpMTOMSessionInfo->lpAttachmentStorage->AddRef();
	lpMTOMSessionInfo->lpecSession = lpecSession; // Should be unlocked after MTOM is done
	lpMTOMSessionInfo->lpecSession->Lock();
	lpMTOMSessionInfo->lpDatabase = lpDatabase;
	lpMTOMSessionInfo->lpSharedDatabase = NULL;
	lpMTOMSessionInfo->er = erSuccess;
	lpMTOMSessionInfo->lpThreadPool = new ECThreadPool(1);
	
	((SOAPINFO *)soap->user)->fdone = MTOMSessionDone;
	((SOAPINFO *)soap->user)->fdoneparam = lpMTOMSessionInfo;

	er = lpAttachmentStorage->Begin();
	if (er != erSuccess)
		goto exit;

	er = lpDatabase->Begin();
	if (er != erSuccess)
		goto exit;

	// Get the parent object id.
	er = lpecSession->GetObjectFromEntryId(&sFolderEntryId, &ulParentId);
	if (er != erSuccess)
		goto exit;
		
	// Lock the parent folder
	strQuery = "SELECT val_ulong FROM properties WHERE hierarchyid = " + stringify(ulParentId) + " FOR UPDATE";
	er = lpDatabase->DoSelect(strQuery, NULL);
	if (er != erSuccess)
		goto exit;

	if (!bIsNew) {
		// Delete the existing message and recreate it
		er = lpecSession->GetObjectFromEntryId(&sEntryId, &ulObjectId);
		if (er != erSuccess)
			goto exit;

		// When a message is update the flags are not passed. So obtain the old flags before
		// deleting so we can pass them to CreateObject.
		er = g_lpSessionManager->GetCacheManager()->GetObjectFlags(ulObjectId, &ulFlags);
		if (er != erSuccess)
			goto exit;

		// Get the original IMAP ID
		strQuery = "SELECT val_ulong FROM properties WHERE"
						" hierarchyid=" + stringify(ulObjectId) +
						" and tag=" + stringify(PROP_ID(PR_EC_IMAP_ID)) +
						" and type=" + stringify(PROP_TYPE(PR_EC_IMAP_ID));
		er = lpDatabase->DoSelect(strQuery, &lpDBResult);
		if (er != erSuccess)
			goto exit;

		lpDBRow = lpDatabase->FetchRow(lpDBResult);
		if (lpDBRow == NULL || lpDBRow[0] == NULL) {
		    // Items created in previous versions of Zarafa will not have a PR_EC_IMAP_ID. The rule
		    // is that an item has a PR_EC_IMAP_ID that is equal to the hierarchyid in that case.
		    ullIMAP = ulObjectId;
		} else {
    		// atoui return a unsigned int at best, but since PR_EC_IMAP_ID is a PT_LONG, the same conversion
	    	// will be done when getting the property through MAPI.
    		ullIMAP = atoui(lpDBRow[0]);
        }    		
		FREE_DBRESULT()
		
		
		lObjectList.push_back(ulObjectId);

		// Collect recursive parent objects, validate item and check the permissions
		er = ExpandDeletedItems(lpecSession, lpDatabase, &lObjectList, ulDeleteFlags, true, &lstDeleteItems);
		if (er != erSuccess) {
			ASSERT(FALSE);
			goto exit;
		}

		// Delete the items hard
		er = DeleteObjectHard(lpecSession, lpDatabase, lpAttachmentStorage, ulDeleteFlags, lstDeleteItems, true, lstDeleted);
		if (er != erSuccess) {
			ASSERT(FALSE);
			goto exit;
		}

		// Update storesize
		er = DeleteObjectStoreSize(lpecSession, lpDatabase, ulDeleteFlags, lstDeleted);
		if (er != erSuccess) {
			ASSERT(FALSE);
			goto exit;
		}

		// Update cache
		er = DeleteObjectCacheUpdate(lpecSession, ulDeleteFlags, lstDeleted);
		if (er != erSuccess) {
			ASSERT(FALSE);
			goto exit;
		}

	}

	// Create the message
	if (bIsNew) {
		er = CreateObject(lpecSession, lpDatabase, ulParentId, MAPI_FOLDER, MAPI_MESSAGE, ulFlags, &ulObjectId);
		if (er != erSuccess)
			goto exit;
	} else {
		ulOwner = lpecSession->GetSecurity()->GetUserId(ulParentId); // Owner of object is either the current user or the owner of the folder

		// Reinsert the entry in the hierarchy table with the same id so the change notification later doesn't
		// become a add notification because the id is different.
		strQuery = "INSERT INTO hierarchy (id, parent, type, flags, owner) values("+stringify(ulObjectId)+", "+stringify(ulParentId)+", "+stringify(MAPI_MESSAGE)+", "+stringify(ulFlags)+", "+stringify(ulOwner)+")";
		er = lpDatabase->DoInsert(strQuery);
		if(er != erSuccess)
			goto exit;
	}

	// Get store
	er = g_lpSessionManager->GetCacheManager()->GetStore(ulObjectId, &ulStoreId, &sGuid);
	if(er != erSuccess)
		goto exit;

	// Quota check
	er = CheckQuota(lpecSession, ulStoreId);
	if (er != erSuccess)
		goto exit;

	// Map entryId <-> ulObjectId
	er = MapEntryIdToObjectId(lpecSession, lpDatabase, ulObjectId, sEntryId);
	if (er != erSuccess)
		goto exit;

	// Deserialize the streamed message
	soap->fmimewriteopen = &MTOMWriteOpen;
	soap->fmimewrite = &MTOMWrite;
	soap->fmimewriteclose= &MTOMWriteClose;

	// We usualy don't pass database object to other threads. However, since
	// we wan't to be able to perform a complete rollback we need to pass it
	// to thread that processes the data and puts it in the database.
	lpsStreamInfo = new MTOMStreamInfo;
	lpsStreamInfo->ulObjectId = ulObjectId;
	lpsStreamInfo->ulStoreId = ulStoreId;
	lpsStreamInfo->bNewItem = bIsNew;
	lpsStreamInfo->ullIMAP = ullIMAP;
	lpsStreamInfo->sGuid = sGuid;
	lpsStreamInfo->ulFlags = ulFlags;
	lpsStreamInfo->lpPropValArray = NULL;
	lpsStreamInfo->lpTask = NULL;
	lpsStreamInfo->lpSessionInfo = lpMTOMSessionInfo;

	if (soap_check_mime_attachments(soap)) {
		struct soap_multipart *content;
		
		content = soap_get_mime_attachment(soap, (void*)lpsStreamInfo);
		
		if (!content) {
			er = ZARAFA_E_CALL_FAILED;
			goto exit;
		}
		
		// Flush remaining attachments (that shouldn't even be there)
		while (true) {
			content = soap_get_mime_attachment(soap, (void*)lpsStreamInfo);
			if (!content)
				break;
		};
	}

	er = g_lpSessionManager->GetCacheManager()->GetObject(ulParentId, &ulGrandParentId, NULL, NULL, &ulParentType);
	if (er != erSuccess)
		goto exit;

	// pr_source_key magic
	if (ulParentType == MAPI_FOLDER) {
		GetSourceKey(ulObjectId, &sSourceKey);
		GetSourceKey(ulParentId, &sParentSourceKey);

		AddChange(lpecSession, ulSyncId, sSourceKey, sParentSourceKey, bIsNew ? ICS_MESSAGE_NEW : ICS_MESSAGE_CHANGE);
	}

	// Update the folder counts
	er = UpdateFolderCounts(lpDatabase, ulParentId, ulFlags, lpsStreamInfo->lpPropValArray);
	if (er != erSuccess)
		goto exit;

	// Set PR_CONFLICT_ITEMS if available
	if (lpsConflictItems != NULL && lpsConflictItems->ulPropTag == PR_CONFLICT_ITEMS) {

		// Delete to be sure
		strQuery = "DELETE FROM mvproperties WHERE hierarchyid=" + stringify(ulObjectId) + " AND tag=" + stringify(PROP_ID(PR_CONFLICT_ITEMS)) + " AND type=" + stringify(PROP_TYPE(PR_CONFLICT_ITEMS));
		er = lpDatabase->DoDelete(strQuery);
		if (er != erSuccess)
			goto exit;

		nMVItems = GetMVItemCount(lpsConflictItems);
		for (unsigned i = 0; i < nMVItems; ++i) {
			er = CopySOAPPropValToDatabaseMVPropVal(lpsConflictItems, i, strColName, strColData, lpDatabase);
			if (er != erSuccess)
				goto exit;

			strQuery = "INSERT INTO mvproperties(hierarchyid,orderid,tag,type," + strColName + ") VALUES(" + stringify(ulObjectId) + "," + stringify(i) + "," + stringify(PROP_ID(PR_CONFLICT_ITEMS)) + "," + stringify(PROP_TYPE(PR_CONFLICT_ITEMS)) + "," + strColData + ")";
			er = lpDatabase->DoInsert(strQuery, NULL, &ulAffected);
			if (er != erSuccess)
				goto exit;
			if (ulAffected != 1) {
				er = ZARAFA_E_DATABASE_ERROR;
				goto exit;
			}
		}
	}

	// Process MSGFLAG_SUBMIT
	// If the messages was saved by an ICS syncer, then we need to sync the PR_MESSAGE_FLAGS for MSGFLAG_SUBMIT if it
	// was included in the save.
	er = ProcessSubmitFlag(lpDatabase, ulSyncId, ulStoreId, ulObjectId, bIsNew, lpsStreamInfo->lpPropValArray);
	if (er != erSuccess)
		goto exit;

	if (ulParentType == MAPI_FOLDER) {
		er = ECTPropsPurge::NormalizeDeferredUpdates(lpecSession, lpDatabase, ulParentId);
		if (er != erSuccess)
			goto exit;
	}

	er = lpAttachmentStorage->Commit();
	if (er != erSuccess)
		goto exit;

	er = lpDatabase->Commit();
	if (er != erSuccess)
		goto exit;

	// Notification
	CreateNotifications(ulObjectId, MAPI_MESSAGE, ulParentId, ulGrandParentId, bIsNew, lpsStreamInfo->lpPropValArray, NULL);

	g_lpStatsCollector->Increment(SCN_DATABASE_MWOPS);

exit:
	if (lpsStreamInfo) {
		if (lpsStreamInfo->lpPropValArray)
			FreePropValArray(lpsStreamInfo->lpPropValArray, true);
		delete lpsStreamInfo;
	}

	FreeDeletedItems(&lstDeleteItems);

	if(lpAttachmentStorage && er != erSuccess)
		lpAttachmentStorage->Rollback();
	
	if (lpAttachmentStorage)
		lpAttachmentStorage->Release();

	ROLLBACK_ON_ERROR();
	FREE_DBRESULT();

	if (er != erSuccess) {
		// remove from cache, else we can get sync issue, with missing messages offline
		lpecSession->GetSessionManager()->GetCacheManager()->RemoveIndexData(ulObjectId);
		lpecSession->GetSessionManager()->GetCacheManager()->Update(fnevObjectDeleted, ulObjectId);
	}
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(getChangeInfo, lpsResponse->er, entryId sEntryId, struct getChangeInfoResponse *lpsResponse)
{
	unsigned int	ulObjId = 0;

	USE_DATABASE();

	// Get object
	er = lpecSession->GetObjectFromEntryId(&sEntryId, &ulObjId);
	if (er != erSuccess)
		goto exit;
	
	// Check security
	er = lpecSession->GetSecurity()->CheckPermission(ulObjId, ecSecurityRead);
	if (er != erSuccess)
		goto exit;


	// Get the Change Key
	strQuery = "SELECT val_binary FROM properties "
				"WHERE tag = " + stringify(PROP_ID(PR_CHANGE_KEY)) +
				" AND type = " + stringify(PROP_TYPE(PR_CHANGE_KEY)) +
				" AND hierarchyid = " + stringify(ulObjId);

	er = lpDatabase->DoSelect(strQuery, &lpDBResult);
	if(er != erSuccess)
		goto exit;

	if (lpDatabase->GetNumRows(lpDBResult) > 0) {
		lpDBRow = lpDatabase->FetchRow(lpDBResult);
		lpDBLen = lpDatabase->FetchRowLengths(lpDBResult);

		lpsResponse->sPropCK.ulPropTag = PR_CHANGE_KEY;
		lpsResponse->sPropCK.__union = SOAP_UNION_propValData_bin;
		lpsResponse->sPropCK.Value.bin = s_alloc<xsd__base64Binary>(soap, 1);
		lpsResponse->sPropCK.Value.bin->__size = lpDBLen[0];
		lpsResponse->sPropCK.Value.bin->__ptr = s_alloc<unsigned char>(soap, lpDBLen[0]);
		memcpy(lpsResponse->sPropCK.Value.bin->__ptr, lpDBRow[0], lpDBLen[0]);
	} else {
		er = ZARAFA_E_NOT_FOUND;
		goto exit;
	}

	FREE_DBRESULT();


	// Get the Predecessor Change List
	strQuery = "SELECT val_binary FROM properties "
				"WHERE tag = " + stringify(PROP_ID(PR_PREDECESSOR_CHANGE_LIST)) +
				" AND type = " + stringify(PROP_TYPE(PR_PREDECESSOR_CHANGE_LIST)) +
				" AND hierarchyid = " + stringify(ulObjId);

	er = lpDatabase->DoSelect(strQuery, &lpDBResult);
	if(er != erSuccess)
		goto exit;

	if (lpDatabase->GetNumRows(lpDBResult) > 0) {
		lpDBRow = lpDatabase->FetchRow(lpDBResult);
		lpDBLen = lpDatabase->FetchRowLengths(lpDBResult);

		lpsResponse->sPropPCL.ulPropTag = PR_PREDECESSOR_CHANGE_LIST;
		lpsResponse->sPropPCL.__union = SOAP_UNION_propValData_bin;
		lpsResponse->sPropPCL.Value.bin = s_alloc<xsd__base64Binary>(soap, 1);
		lpsResponse->sPropPCL.Value.bin->__size = lpDBLen[0];
		lpsResponse->sPropPCL.Value.bin->__ptr = s_alloc<unsigned char>(soap, lpDBLen[0]);
		memcpy(lpsResponse->sPropPCL.Value.bin->__ptr, lpDBRow[0], lpDBLen[0]);
	} else {
		er = ZARAFA_E_NOT_FOUND;
		goto exit;
	}

exit:
	FREE_DBRESULT();
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(purgeDeferredUpdates, lpsResponse->er, struct purgeDeferredUpdatesResponse *lpsResponse)
{
    unsigned int ulFolderId = 0;
    USE_DATABASE();
    
    // Only system-admins may run this
    if(lpecSession->GetSecurity()->GetAdminLevel() < ADMIN_LEVEL_SYSADMIN) {
        er = ZARAFA_E_NO_ACCESS;
        goto exit;
    }

    er = ECTPropsPurge::GetLargestFolderId(lpDatabase, &ulFolderId);
    if(er == ZARAFA_E_NOT_FOUND) {
        // Nothing to purge
        lpsResponse->ulDeferredRemaining = 0;
        goto exit;
    }
    
    if(er != erSuccess)
        goto exit;
        
    er = ECTPropsPurge::PurgeDeferredTableUpdates(lpDatabase, ulFolderId);
    if(er != erSuccess)
        goto exit;

    er = ECTPropsPurge::GetDeferredCount(lpDatabase, &lpsResponse->ulDeferredRemaining);
    if(er != erSuccess)
        goto exit;
        
exit:
    ;        
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(testPerform, *result, char *szCommand, struct testPerformArgs sPerform, unsigned int *result)
{
    if(parseBool(g_lpSessionManager->GetConfig()->GetSetting("enable_test_protocol")))
        er = TestPerform(soap, lpecSession, szCommand, sPerform.__size, sPerform.__ptr);
    else
        er = ZARAFA_E_NO_ACCESS;
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(testSet, *result, char *szVarName, char *szValue, unsigned int *result)
{
    if(parseBool(g_lpSessionManager->GetConfig()->GetSetting("enable_test_protocol")))
        er = TestSet(soap, lpecSession, szVarName, szValue);
    else
        er = ZARAFA_E_NO_ACCESS;
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(testGet, lpsResponse->er, char *szVarName, struct testGetResponse *lpsResponse)
{
    if(parseBool(g_lpSessionManager->GetConfig()->GetSetting("enable_test_protocol")))
        er = TestGet(soap, lpecSession, szVarName, &lpsResponse->szValue);
    else
        er = ZARAFA_E_NO_ACCESS;
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(setLockState, *result, entryId sEntryId, bool bLocked, unsigned int *result)
{
	unsigned int ulObjId = 0;
	unsigned int ulOwner = 0;
	unsigned int ulObjType = 0;

	er = g_lpSessionManager->GetCacheManager()->GetObjectFromEntryId(&sEntryId, &ulObjId);
	if (er != erSuccess)
		goto exit;

	er = g_lpSessionManager->GetCacheManager()->GetObject(ulObjId, NULL, &ulOwner, NULL, &ulObjType);
	if (er != erSuccess)
		goto exit;

	if (ulObjType != MAPI_MESSAGE) {
		er = ZARAFA_E_NO_SUPPORT;
		goto exit;
	}

	// Do we need to be owner?
	er = lpecSession->GetSecurity()->CheckPermission(ulObjId, ecSecurityOwner);
	if (er != erSuccess)
		goto exit;

	if (bLocked) {
		er = lpecSession->LockObject(ulObjId);
		if (er == ZARAFA_E_NO_ACCESS)
			er = ZARAFA_E_SUBMITTED;
	} else {
		er = lpecSession->UnlockObject(ulObjId);
	}

exit:
	;
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(getUserClientUpdateStatus, lpsResponse->er, entryId sUserId, struct userClientUpdateStatusResponse *lpsResponse)
{
	USE_DATABASE();
	objectid_t sExternId;
	unsigned int ulUserId = 0;
	bool bHasLocalStore = false;

	if (!parseBool(g_lpSessionManager->GetConfig()->GetSetting("client_update_enabled"))) {
		er = ZARAFA_E_NO_SUPPORT;
		goto exit;
	}

	er = GetLocalId(sUserId, 0, &ulUserId, &sExternId);
	if (er != erSuccess)
		goto exit;

	er = CheckUserStore(lpecSession, ulUserId, ECSTORE_TYPE_PRIVATE, &bHasLocalStore);
	if (er != erSuccess)
		goto exit;

	if (!bHasLocalStore) {
		er = ZARAFA_E_NOT_FOUND;
		goto exit;
	}

	strQuery = "SELECT trackid, UNIX_TIMESTAMP(updatetime), currentversion, latestversion, computername, status FROM clientupdatestatus WHERE userid="+stringify(ulUserId);
	er = lpDatabase->DoSelect(strQuery, &lpDBResult);
	if(er != erSuccess)
		goto exit;

	lpDBRow = lpDatabase->FetchRow(lpDBResult);
	if (!lpDBRow) {
		er = MAPI_E_NOT_FOUND;
		goto exit;
	}

	if(lpDBRow[0]) lpsResponse->ulTrackId = atoui(lpDBRow[0]);
	if(lpDBRow[1]) lpsResponse->tUpdatetime = atoui(lpDBRow[1]);
	if(lpDBRow[2]) lpsResponse->lpszCurrentversion = s_strcpy(soap, lpDBRow[2]);
	if(lpDBRow[3]) lpsResponse->lpszLatestversion =  s_strcpy(soap, lpDBRow[3]);
	if(lpDBRow[4]) lpsResponse->lpszComputername =  s_strcpy(soap, lpDBRow[4]);
	if(lpDBRow[5]) lpsResponse->ulStatus = atoui(lpDBRow[5]);

exit:
	FREE_DBRESULT();
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(removeAllObjects, *result, entryId sExceptUserId, unsigned int *result)
{
    er = ZARAFA_E_NO_SUPPORT;
}
SOAP_ENTRY_END()

SOAP_ENTRY_START(resetFolderCount, lpsResponse->er, entryId sEntryId, struct resetFolderCountResponse *lpsResponse)
{
	unsigned int ulObjId = 0;
	unsigned int ulOwner = 0;
	unsigned int ulObjType = 0;

	er = g_lpSessionManager->GetCacheManager()->GetObjectFromEntryId(&sEntryId, &ulObjId);
	if (er != erSuccess)
		goto exit;

	er = g_lpSessionManager->GetCacheManager()->GetObject(ulObjId, NULL, &ulOwner, NULL, &ulObjType);
	if (er != erSuccess)
		goto exit;

	if (ulObjType != MAPI_FOLDER) {
		er = ZARAFA_E_INVALID_TYPE;
		goto exit;
	}

	er = lpecSession->GetSecurity()->CheckPermission(ulObjId, ecSecurityOwner);
	if (er != erSuccess)
		goto exit;

	er = ResetFolderCount(lpecSession, ulObjId, &lpsResponse->ulUpdates);

exit:
	;
}
SOAP_ENTRY_END()
