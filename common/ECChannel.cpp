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

#include "platform.h"

#include "ECChannel.h"
#include "stringutil.h"
#include <csignal>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include <cerrno>
#include <mapicode.h>

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif


#ifndef hrSuccess
#define hrSuccess 0
#endif

/*
To generate a RSA key:
openssl genrsa -out privkey.pem 2048

Creating a certificate request:
openssl req -new -key privkey.pem -out cert.csr

Creating a self-signed test certificate:
openssl req -new -x509 -key privkey.pem -out cacert.pem -days 1095
*/

// because of statics
SSL_CTX* ECChannel::lpCTX = NULL;

HRESULT ECChannel::HrSetCtx(ECConfig *lpConfig, ECLogger *lpLogger) {
	HRESULT hr = hrSuccess;
	const char *szFile = NULL;
	const char *szPath = NULL;
 	char *ssl_protocols = strdup(lpConfig->GetSetting("ssl_protocols"));
	const char *ssl_ciphers = lpConfig->GetSetting("ssl_ciphers");
 	char *ssl_name = NULL;
 	int ssl_op = 0, ssl_include = 0, ssl_exclude = 0;

	if (lpConfig == NULL) {
		lpLogger->Log(EC_LOGLEVEL_ERROR, "ECChannel::HrSetCtx(): invalid parameters");
		hr = MAPI_E_CALL_FAILED;
		goto exit;
	}

	if (lpCTX) {
		SSL_CTX_free(lpCTX);
		lpCTX = NULL;
	}

	SSL_library_init();
	SSL_load_error_strings();

	// enable *all* server methods, not just ssl2 and ssl3, but also tls1 and tls1.1
	lpCTX = SSL_CTX_new(SSLv23_server_method());

	SSL_CTX_set_options(lpCTX, SSL_OP_ALL);			 // enable quirk and bug workarounds

	ssl_name = strtok(ssl_protocols, " ");
	while(ssl_name != NULL) {
		int ssl_proto = 0;
		bool ssl_neg = false;

		if (*ssl_name == '!') {
			ssl_name++;
			ssl_neg = true;
		}

		if (strcmp_ci(ssl_name, SSL_TXT_SSLV2) == 0)
			ssl_proto = 0x01;
		else if (strcmp_ci(ssl_name, SSL_TXT_SSLV3) == 0)
			ssl_proto = 0x02;
		else if (strcmp_ci(ssl_name, SSL_TXT_TLSV1) == 0)
			ssl_proto = 0x04;
#ifdef SSL_TXT_TLSV1_1
		else if (strcmp_ci(ssl_name, SSL_TXT_TLSV1_1) == 0)
			ssl_proto = 0x08;
#endif
#ifdef SSL_TXT_TLSV1_2
		else if (strcmp_ci(ssl_name, SSL_TXT_TLSV1_2) == 0)
			ssl_proto = 0x10;
#endif
		else {
			lpLogger->Log(EC_LOGLEVEL_ERROR, "Unknown protocol '%s' in ssl_protocols setting", ssl_name);
			hr = MAPI_E_CALL_FAILED;
			goto exit;
		}

		if (ssl_neg)
			ssl_exclude |= ssl_proto;
		else
			ssl_include |= ssl_proto;

		ssl_name = strtok(NULL, " ");
	}

	if (ssl_include != 0) {
		// Exclude everything, except those that are included (and let excludes still override those)
		ssl_exclude |= 0x1f & ~ssl_include;
	}

	if ((ssl_exclude & 0x01) != 0)
		ssl_op |= SSL_OP_NO_SSLv2;
	if ((ssl_exclude & 0x02) != 0)
		ssl_op |= SSL_OP_NO_SSLv3;
	if ((ssl_exclude & 0x04) != 0)
		ssl_op |= SSL_OP_NO_TLSv1;
#ifdef SSL_OP_NO_TLSv1_1
	if ((ssl_exclude & 0x08) != 0)
		ssl_op |= SSL_OP_NO_TLSv1_1;
#endif
#ifdef SSL_OP_NO_TLSv1_2
	if ((ssl_exclude & 0x10) != 0)
		ssl_op |= SSL_OP_NO_TLSv1_2;
#endif

	if (ssl_protocols) {
		SSL_CTX_set_options(lpCTX, ssl_op);
	}

	if (ssl_ciphers && SSL_CTX_set_cipher_list(lpCTX, ssl_ciphers) != 1) {
		lpLogger->Log(EC_LOGLEVEL_ERROR, "Can not set SSL cipher list to '%s': %s", ssl_ciphers, ERR_error_string(ERR_get_error(), 0));
		hr = MAPI_E_CALL_FAILED;
		goto exit;
	}

	if (parseBool(lpConfig->GetSetting("ssl_prefer_server_ciphers"))) {
		SSL_CTX_set_options(lpCTX, SSL_OP_CIPHER_SERVER_PREFERENCE);
	}

	SSL_CTX_set_default_verify_paths(lpCTX);

	if (SSL_CTX_use_certificate_chain_file(lpCTX, lpConfig->GetSetting("ssl_certificate_file")) != 1) {
		lpLogger->Log(EC_LOGLEVEL_ERROR, "SSL CTX certificate file error: %s", ERR_error_string(ERR_get_error(), 0));
		hr = MAPI_E_CALL_FAILED;
		goto exit;
	}

	if (SSL_CTX_use_PrivateKey_file(lpCTX, lpConfig->GetSetting("ssl_private_key_file"), SSL_FILETYPE_PEM) != 1) {
		lpLogger->Log(EC_LOGLEVEL_ERROR, "SSL CTX private key file error: %s", ERR_error_string(ERR_get_error(), 0));
		hr = MAPI_E_CALL_FAILED;
		goto exit;
	}

	if (SSL_CTX_check_private_key(lpCTX) != 1) {
		lpLogger->Log(EC_LOGLEVEL_ERROR, "SSL CTX check private key error: %s", ERR_error_string(ERR_get_error(), 0));
		hr = MAPI_E_CALL_FAILED;
		goto exit;
	}

	if (strcmp(lpConfig->GetSetting("ssl_verify_client"), "yes") == 0) {
		SSL_CTX_set_verify(lpCTX, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, 0);
	} else {
		SSL_CTX_set_verify(lpCTX, SSL_VERIFY_NONE, 0);
	}

	if (strlen(lpConfig->GetSetting("ssl_verify_file")) > 0)
		szFile = lpConfig->GetSetting("ssl_verify_file");

	if (strlen(lpConfig->GetSetting("ssl_verify_path")) > 0)
		szPath = lpConfig->GetSetting("ssl_verify_path");

	if (szFile || szPath) {
		if (SSL_CTX_load_verify_locations(lpCTX, szFile, szPath) != 1)
			lpLogger->Log(EC_LOGLEVEL_ERROR, "SSL CTX error loading verify locations: %s", ERR_error_string(ERR_get_error(), 0));
	}

exit:
	free(ssl_protocols);

	if (hr != hrSuccess)
		HrFreeCtx();

	return hr;
}

HRESULT ECChannel::HrFreeCtx() {
	if (lpCTX) {
		SSL_CTX_free(lpCTX);
		lpCTX = NULL;
	}
	return hrSuccess;
}

ECChannel::ECChannel(int fd) {
	int flag = 1;
    
	this->fd = fd;
	lpSSL = NULL;
	
	setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char *>(&flag), sizeof(flag));
}

ECChannel::~ECChannel() {
	if (lpSSL) {
		SSL_shutdown(lpSSL);
		SSL_free(lpSSL);
		lpSSL = NULL;
	}
	closesocket(fd);
}

HRESULT ECChannel::HrEnableTLS(ECLogger *const lpLogger) {
	int rc = -1;
	HRESULT hr = hrSuccess;

	if (lpSSL || lpCTX == NULL) {
		hr = MAPI_E_CALL_FAILED;
		lpLogger->Log(EC_LOGLEVEL_ERROR, "ECChannel::HrEnableTLS(): invalid parameters");
		goto exit;
	}

	lpSSL = SSL_new(lpCTX);
	if (!lpSSL) {
		lpLogger->Log(EC_LOGLEVEL_ERROR, "ECChannel::HrEnableTLS(): SSL_new failed");
		hr = MAPI_E_CALL_FAILED;
		goto exit;
	}

	SSL_clear(lpSSL);

	if (SSL_set_fd(lpSSL, fd) != 1) {
		lpLogger->Log(EC_LOGLEVEL_ERROR, "ECChannel::HrEnableTLS(): SSL_set_fd failed");
		hr = MAPI_E_CALL_FAILED;
		goto exit;
	}

	SSL_set_accept_state(lpSSL);
	if ((rc = SSL_accept(lpSSL)) != 1) {
		lpLogger->Log(EC_LOGLEVEL_ERROR, "ECChannel::HrEnableTLS(): SSL_accept failed: %d", SSL_get_error(lpSSL, rc));
		hr = MAPI_E_CALL_FAILED;
		goto exit;
	}

exit:
	if (hr != hrSuccess && lpSSL) {
		SSL_shutdown(lpSSL);
		SSL_free(lpSSL);
		lpSSL = NULL;
	}

	return hr;
}

HRESULT ECChannel::HrGets(char *szBuffer, ULONG ulBufSize, ULONG *lpulRead) {
	char *lpRet = NULL;
	int len = ulBufSize;

	if (!szBuffer || !lpulRead)
		return MAPI_E_INVALID_PARAMETER;

	if (lpSSL)
		lpRet = SSL_gets(szBuffer, &len);
	else
		lpRet = fd_gets(szBuffer, &len);

	if (lpRet) {
		*lpulRead = len;
		return hrSuccess;
	}

	return MAPI_E_CALL_FAILED;
}

/** 
 * Read a line from a socket. Reads as much data until it encounters a
 * \n characters.
 * 
 * @param[out] strBuffer network data will be placed in this buffer
 * @param[in] ulMaxBuffer optional, default 65k, breaks reading after this limit is reached
 * 
 * @return MAPI_ERROR_CODE
 * @retval MAPI_E_TOO_BIG more data in the network buffer than requested to read
 */
HRESULT ECChannel::HrReadLine(std::string * strBuffer, ULONG ulMaxBuffer) {
	HRESULT hr = hrSuccess;
	ULONG ulRead = 0;

	if(!strBuffer)
		return MAPI_E_INVALID_PARAMETER;

	char buffer[65536];

	// clear the buffer before appending
	strBuffer->clear();

	do {
		hr = HrGets(buffer, 65536, &ulRead);
		if (hr != hrSuccess)
			break;

		strBuffer->append(buffer, ulRead);
		if (strBuffer->size() > ulMaxBuffer) {
			hr = MAPI_E_TOO_BIG;
			break;
		}
	} while (ulRead == 65535);	// zero-terminator is not counted

	return hr;
}

HRESULT ECChannel::HrWriteString(const char *szBuffer)
{
	HRESULT hr = hrSuccess;

	if(!szBuffer)
		return MAPI_E_INVALID_PARAMETER;

	if (lpSSL) {
		if (SSL_write(lpSSL, szBuffer, (int)strlen(szBuffer)) < 1)
			hr = MAPI_E_CALL_FAILED;
	}
	else {
		if (send(fd, szBuffer, (int)strlen(szBuffer), 0) < 1)
			hr = MAPI_E_CALL_FAILED;
	}

	return hr;
}

HRESULT ECChannel::HrWriteString(const std::string & strBuffer) {
	HRESULT hr = hrSuccess;

	if (lpSSL) {
		if (SSL_write(lpSSL, strBuffer.c_str(), (int)strBuffer.size()) < 1)
			hr = MAPI_E_CALL_FAILED;
	} else {
		if (send(fd, strBuffer.c_str(), (int)strBuffer.size(), 0) < 1)
			hr = MAPI_E_CALL_FAILED;
	}

	return hr;
}

/**
 * Writes a line of data to socket
 *
 * Function takes specified lenght of data from the pointer,
 * if length is not specified all the data of pointed by buffer is used. 
 * It then adds CRLF to the end of the data and writes it to the socket
 *
 * @param[in]	szBuffer	pointer to the data to be written to socket
 * @param[in]	len			optional paramter to specify lenght of data in szBuffer, if empty then all data of szBuffer is written to socket.
 * 
 * @retval		MAPI_E_CALL_FAILED	unable to write data to socket
 */
HRESULT ECChannel::HrWriteLine(const char *szBuffer, int len) {
	std::string strLine;

	if (len == 0)
		strLine.assign(szBuffer);
	else
		strLine.assign(szBuffer, len);

	strLine += "\r\n";
	
	return HrWriteString(strLine);
}

HRESULT ECChannel::HrWriteLine(const std::string & strBuffer) {
	return HrWriteString(strBuffer + "\r\n");
}

/**
 * Read and discard bytes
 *
 * Read from socket and discard the data
 *
 * @param[in] ulByteCount Amount of bytes to discard 
 *
 * @retval MAPI_E_NETWORK_ERROR Unable to read bytes.
 * @retval MAPI_E_CALL_FAILED Reading wrong amount of data.
 */
HRESULT ECChannel::HrReadAndDiscardBytes(ULONG ulByteCount) {
	ULONG ulTotRead = 0;
	char szBuffer[4096];

	while (ulTotRead < ulByteCount) {
		ULONG ulBytesLeft = ulByteCount - ulTotRead;
		ULONG ulRead = ulBytesLeft > sizeof szBuffer ? sizeof szBuffer : ulBytesLeft;

		if (lpSSL)
			ulRead = SSL_read(lpSSL, szBuffer, ulRead);
		else
			ulRead = recv(fd, szBuffer, ulRead, 0);

		if (ulRead == (ULONG)-1) {
			if (errno == EINTR)
				continue;

			return MAPI_E_NETWORK_ERROR;
		}

		if (ulRead == 0 || ulRead > ulByteCount)
			return MAPI_E_NETWORK_ERROR;

		ulTotRead += ulRead;
	}

	return (ulTotRead == ulByteCount) ? hrSuccess : MAPI_E_CALL_FAILED;
}

HRESULT ECChannel::HrReadBytes(char *szBuffer, ULONG ulByteCount) {
	ULONG ulRead = 0;
	ULONG ulTotRead = 0;

	if(!szBuffer)
		return MAPI_E_INVALID_PARAMETER;

	while(ulTotRead < ulByteCount) {
		if (lpSSL)
			ulRead = SSL_read(lpSSL, szBuffer + ulTotRead, ulByteCount - ulTotRead);
		else
			ulRead = recv(fd, szBuffer + ulTotRead, ulByteCount - ulTotRead, 0);

		if (ulRead == (ULONG)-1) {
			if (errno == EINTR)
				continue;

			return MAPI_E_NETWORK_ERROR;
		}

		if (ulRead == 0 || ulRead > ulByteCount)
			return MAPI_E_NETWORK_ERROR;

		ulTotRead += ulRead;
	}

	szBuffer[ulTotRead] = '\0';

	return (ulTotRead == ulByteCount) ? hrSuccess : MAPI_E_CALL_FAILED;
}

HRESULT ECChannel::HrReadBytes(std::string * strBuffer, ULONG ulByteCount) {
	HRESULT hr = hrSuccess;
	char *buffer = NULL;

	if(!strBuffer) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	try {
		buffer = new char[ulByteCount + 1];
	}
	catch (std::exception &) {
		hr = MAPI_E_NOT_ENOUGH_MEMORY;
		goto exit;
	}

	hr = HrReadBytes(buffer, ulByteCount);
	if (hr != hrSuccess)
		goto exit;

	strBuffer->assign(buffer, ulByteCount);

exit:
	if (buffer)
		delete [] buffer;

	return hr;
}

HRESULT ECChannel::HrSelect(int seconds) {
	fd_set fds;
	int res = 0;
	struct timeval timeout = { seconds, 0 };

	if(fd >= FD_SETSIZE)
	    return MAPI_E_NOT_ENOUGH_MEMORY;
	if(lpSSL && SSL_pending(lpSSL))
		return hrSuccess;

	FD_ZERO(&fds);
	FD_SET(fd, &fds);

	res = select(fd + 1, &fds, NULL, NULL, &timeout);
	if (res == -1) {
		if (errno == EINTR)
			/*
			 * We _must_ return to the caller so it gets a chance
			 * to e.g. shut down as a result of SIGTERM.
			 */
			return MAPI_E_CANCEL;

		return MAPI_E_NETWORK_ERROR;
	}

	if (res == 0)
		return MAPI_E_TIMEOUT;

	return hrSuccess;
}

bool ECChannel::UsingSsl() {
	return lpSSL != NULL;
}

bool ECChannel::sslctx() {
	return lpCTX != NULL;
}

/** 
 * read from buffer until \n is found, or buffer length is reached
 * return buffer always contains \0 in the end, so max read from network is *lpulLen -1
 *
 * @param[out] buf buffer to read network data in
 * @param[in,out] lpulLen input is max size to read, output is read bytes from network
 * 
 * @return NULL on error, or buf
 */
char * ECChannel::fd_gets(char *buf, int *lpulLen) {
	char *newline = NULL, *bp = buf;
	int len = *lpulLen;

	if (--len < 1)
		return NULL;

	do {
		/*
		 * Return NULL when we read nothing:
		 * other side has closed its writing socket.
		 */
		int n = recv(fd, bp, len, MSG_PEEK);

		if (n == 0)
			return NULL;

		if (n == -1) {
			if (errno == EINTR)
				continue;

			return NULL;
		}

		if ((newline = (char *)memchr((void *)bp, '\n', n)) != NULL)
			n = newline - bp + 1;

	retry:
		int recv_n = recv(fd, bp, n, 0);

		if (recv_n == 0)
			return NULL;

		if (recv_n == -1) {
			if (errno == EINTR)
				goto retry;

			return NULL;
		}

		bp += recv_n;
		len -= recv_n;
	}
	while(!newline && len > 0);

	//remove the lf or crlf
	if(newline){
		bp--;
		newline--;
		if(newline >= buf && *newline == '\r')
			bp--;
	}

	*bp = '\0';
	*lpulLen = (int)(bp - buf);

	return buf;
}

char * ECChannel::SSL_gets(char *buf, int *lpulLen) {
	char *newline, *bp = buf;
	int len = *lpulLen;
	int n = 0;

	if (--len < 1)
		return NULL;

	do {
		/*
		 * Return NULL when we read nothing:
		 * other side has closed its writing socket.
		 */
		if ((n = SSL_peek(lpSSL, bp, len)) <= 0)
			return NULL;

		if ((newline = (char *)memchr((void *)bp, '\n', n)) != NULL)
			n = newline - bp + 1;

		if ((n = SSL_read(lpSSL, bp, n)) < 0)
			return NULL;

		bp += n;
		len -= n;
	} while (!newline && len > 0);
	
	//remove the lf or crlf
	if(newline){
		bp--;
		newline--;
		if(newline >= buf && *newline == '\r')
			bp--;
	}

	*bp = '\0';
	*lpulLen = (int)(bp - buf);
	return buf;
}

void ECChannel::SetIPAddress(char *szIPAddress)
{
	strIP = szIPAddress;
}

const std::string& ECChannel::GetIPAddress() const
{
	return strIP;
}

HRESULT HrListen(ECLogger *lpLogger, const char *szPath, int *lpulListenSocket)
{
	HRESULT hr = hrSuccess;
	int fd = -1;
	struct sockaddr_un sun_addr;
	mode_t prevmask = 0;

	if (szPath == NULL || lpulListenSocket == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	memset(&sun_addr, 0, sizeof(sun_addr));
	sun_addr.sun_family = AF_UNIX;
	strcpy(sun_addr.sun_path, szPath);


	if ((fd = socket(PF_UNIX, SOCK_STREAM, 0)) == -1) {
		if (lpLogger)
			lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to create AF_UNIX socket.");
		hr = MAPI_E_NETWORK_ERROR;
		goto exit;
	}

	unlink(szPath);

	// make files with permissions 0666
	prevmask = umask(0111);

	if (bind(fd, (struct sockaddr *)&sun_addr, sizeof(sun_addr)) == -1) {
                if (lpLogger)
			lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to bind to socket %s (%s). This is usually caused by another process (most likely another zarafa-server) already using this port. This program will terminate now.", szPath, strerror(errno));
                kill(0, SIGTERM);
                exit(1);
        }

	// TODO: backlog of SOMAXCONN should be configurable
	if (listen(fd, SOMAXCONN) == -1) {
		if (lpLogger)
			lpLogger->Log(EC_LOGLEVEL_ERROR, "Unable to start listening on socket %s.", szPath);
		hr = MAPI_E_NETWORK_ERROR;
		goto exit;
	}

	*lpulListenSocket = fd;

exit:
	if (prevmask)
		umask(prevmask);

	return hr;
}

HRESULT HrListen(ECLogger *lpLogger, const char *szBind, int ulPort, int *lpulListenSocket)
{
	HRESULT hr = hrSuccess;
	int fd = -1, opt = 1;
	struct sockaddr_in sin_addr;

	if (lpulListenSocket == NULL || ulPort == 0 || szBind == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	sin_addr.sin_family = AF_INET;
	sin_addr.sin_addr.s_addr = inet_addr(szBind);
	sin_addr.sin_port = htons(ulPort);


	if ((fd = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
		if (lpLogger)
			lpLogger->Log(EC_LOGLEVEL_ERROR, "Unable to create TCP socket.");
		hr = MAPI_E_NETWORK_ERROR;
		goto exit;
	}

		// TODO: should be configurable?
		if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char *>(&opt), sizeof(opt)) == -1)
			lpLogger->Log(EC_LOGLEVEL_WARNING, "Unable to set reuseaddr socket option: %s", strerror(errno));

		if (bind(fd, (struct sockaddr *)&sin_addr, sizeof(sin_addr)) == -1) {
			if (lpLogger)
				lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to bind to port %d (%s). This is usually caused by another process (most likely another zarafa-server) already using this port. This program will terminate now.", ulPort, strerror(errno));
		kill(0, SIGTERM);
		exit(1);
	}

	// TODO: backlog of SOMAXCONN should be configurable
	if (listen(fd, SOMAXCONN) == -1) {
		if (lpLogger)
			lpLogger->Log(EC_LOGLEVEL_ERROR, "Unable to start listening on port %d.", ulPort);
		hr = MAPI_E_NETWORK_ERROR;
		goto exit;
	}

	*lpulListenSocket = fd;

exit:
	return hr;
}

HRESULT HrAccept(ECLogger *lpLogger, int ulListenFD, ECChannel **lppChannel)
{
	HRESULT hr = hrSuccess;
	int socket = 0;
	struct sockaddr_in client;
	ECChannel *lpChannel = NULL;
	socklen_t len = sizeof(client);

#ifdef TCP_FASTOPEN
	int qlen = SOMAXCONN;
	setsockopt(ulListenFD, SOL_TCP, TCP_FASTOPEN, &qlen, sizeof(qlen));
#endif

	if (ulListenFD < 0 || lppChannel == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		if (lpLogger)
			lpLogger->Log(EC_LOGLEVEL_ERROR, "HrAccept: invalid parameters");
		goto exit;
	}

	memset(&client, 0, sizeof(client));

	socket = accept(ulListenFD, (struct sockaddr *)&client, &len);

	if (socket == -1) {
		if (lpLogger)
			lpLogger->Log(EC_LOGLEVEL_ERROR, "Unable to accept(): %s", strerror(errno));
		hr = MAPI_E_NETWORK_ERROR;
		goto exit;
	}
	if (lpLogger)
		lpLogger->Log(EC_LOGLEVEL_INFO, "Accepted connection from %s", inet_ntoa(client.sin_addr));

	lpChannel = new ECChannel(socket);
	lpChannel->SetIPAddress(inet_ntoa(client.sin_addr));

	*lppChannel = lpChannel;

exit:
	if (hr != hrSuccess && lpChannel)
		delete lpChannel;

	return hr;
}
