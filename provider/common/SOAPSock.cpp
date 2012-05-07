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
#include "SOAPSock.h"

#include <sys/un.h>

#include "SOAPUtils.h"
#include "threadutil.h"
#include "CommonUtil.h"
#include <string>
#include <map>

#include <charset/convert.h>
#include <charset/utf8string.h>


using namespace std;

static int ssl_zvcb_index = -1;	// the index to get our custom data

// This function wraps the GSOAP fopen call to support "file:///var/run/socket" unix-socket URI's
int gsoap_connect_pipe(struct soap *soap, const char *endpoint, const char *host, int port)
{
	int fd;
	struct sockaddr_un saddr;
	memset(&saddr, 0, sizeof(struct sockaddr_un));

	// See stdsoap2.cpp:tcp_connect() function
	if (soap_valid_socket(soap->socket))
	    return SOAP_OK;

	soap->socket = SOAP_INVALID_SOCKET;

	if (strncmp(endpoint, "file://", 7) || strchr(endpoint+7,'/') == NULL)
   	{
      	return SOAP_EOF;
	}

	fd = socket(PF_UNIX, SOCK_STREAM, 0);

	saddr.sun_family = AF_UNIX;
	strcpy(saddr.sun_path, strchr(endpoint+7,'/'));

	connect(fd, (struct sockaddr *)&saddr, sizeof(struct sockaddr_un));

 	soap->sendfd = soap->recvfd = SOAP_INVALID_SOCKET;
	soap->socket = fd;

	// Because 'file:///var/run/file' will be parsed into host='', path='/var/run/file',
	// the gSOAP code doesn't set the soap->status variable. (see soap_connect_command in
	// stdsoap2.cpp:12278) This could possibly lead to
	// soap->status accidentally being set to SOAP_GET, which would break things. The
	// chances of this happening are, of course, small, but also very real.

	soap->status = SOAP_POST;

   	return SOAP_OK;
}


HRESULT CreateSoapTransport(ULONG ulUIFlags,
							const std::string &strServerPath,
							const std::string &strSSLKeyFile,
							const std::string &strSSLKeyPass,
							ULONG ulConnectionTimeOut,
							const std::string &strProxyHost,
							const WORD		&wProxyPort,
							const std::string &strProxyUserName,
							const std::string &strProxyPassword,
							const ULONG		&ulProxyFlags,
							int				iSoapiMode,
							int				iSoapoMode,
							ZarafaCmd **lppCmd)
{
	HRESULT		hr = hrSuccess;
	ZarafaCmd*	lpCmd = NULL;

	if (strServerPath.empty() || lppCmd == NULL) {
		hr = E_INVALIDARG;
		goto exit;
	}

	lpCmd = new ZarafaCmd();

	soap_set_imode(lpCmd->soap, iSoapiMode);
	soap_set_omode(lpCmd->soap, iSoapoMode);

	lpCmd->endpoint = strdup(strServerPath.c_str());

#ifdef WITH_OPENSSL
	if (strncmp("https:", lpCmd->endpoint, 6) == 0) {
		// no need to add certificates to call, since soap also calls SSL_CTX_set_default_verify_paths()
		if(soap_ssl_client_context(lpCmd->soap,
								SOAP_SSL_DEFAULT | SOAP_SSL_SKIP_HOST_CHECK,
								strSSLKeyFile.empty()? NULL : strSSLKeyFile.c_str(), 
								strSSLKeyPass.empty()? NULL : strSSLKeyPass.c_str(),
								NULL, NULL,
								NULL)) {
			hr = E_INVALIDARG;
			goto exit;
		}

		// set connection string as callback information
		if (ssl_zvcb_index == -1) {
			ssl_zvcb_index = SSL_get_ex_new_index(0, NULL, NULL, NULL, NULL);
		}
		// callback data will be set right before tcp_connect()

		// set our own certificate check function
		lpCmd->soap->fsslverify = ssl_verify_callback_zarafa_silent;

		SSL_CTX_set_verify(lpCmd->soap->ctx, SSL_VERIFY_PEER, lpCmd->soap->fsslverify);
	}
#endif

	if(strncmp("file:", lpCmd->endpoint, 5) == 0) {
		lpCmd->soap->fconnect = gsoap_connect_pipe;
	} else {

		if ((ulProxyFlags&0x0000001/*EC_PROFILE_PROXY_FLAGS_USE_PROXY*/) && !strProxyHost.empty()) {
			lpCmd->soap->proxy_host = strdup(strProxyHost.c_str());
			lpCmd->soap->proxy_port = wProxyPort;

			if (!strProxyUserName.empty())
				lpCmd->soap->proxy_userid = strdup(strProxyUserName.c_str());

			if (!strProxyPassword.empty())
				lpCmd->soap->proxy_passwd = strdup(strProxyPassword.c_str());
		}

		lpCmd->soap->connect_timeout = ulConnectionTimeOut;
	}


	*lppCmd = lpCmd;

exit:
	if (hr != hrSuccess && lpCmd) {
		free((void*)lpCmd->endpoint);	// because of strdup()
		delete lpCmd;
		lpCmd = NULL;
	}

	return hr;
}

VOID DestroySoapTransport(ZarafaCmd *lpCmd)
{
	if (!lpCmd)
		return;

	if (lpCmd->endpoint)
		free((void *)lpCmd->endpoint);   // because of strdup()

	if (lpCmd->soap->proxy_host)
		free((void *)lpCmd->soap->proxy_host);

	if (lpCmd->soap->proxy_userid)
		free((void *)lpCmd->soap->proxy_userid);

	if (lpCmd->soap->proxy_passwd)
		free((void *)lpCmd->soap->proxy_passwd);

	delete lpCmd;
}


int ssl_verify_callback_zarafa_silent(int ok, X509_STORE_CTX *store)
{
	int sslerr;

	if (ok == 0)
	{
		// Get the last ssl error
		sslerr = X509_STORE_CTX_get_error(store);
		switch (sslerr)
		{
			case X509_V_ERR_CERT_HAS_EXPIRED:
			case X509_V_ERR_CERT_NOT_YET_VALID:
			case X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN:
				// always ignore these errors
				X509_STORE_CTX_set_error(store, X509_V_OK);
				ok = 1;
				break;
			default:
				break;
		}
	}

	return ok;
}

