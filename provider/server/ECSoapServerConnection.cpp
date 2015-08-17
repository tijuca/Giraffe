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

#include "ECLogger.h"

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_SYS_UN_H
#include <sys/un.h>
#endif

#include "ECSoapServerConnection.h"
#include "ECServerEntrypoint.h"
#include "ECClientUpdate.h"
#include "UnixUtil.h"

extern ECLogger *g_lpLogger;

struct soap_connection_thread {
	ECSoapServerConnection*	lpSoapConnClass;
	struct soap*			lpSoap;
};


/** 
 * Creates a AF_UNIX socket in a given location and starts to listen
 * on that socket.
 * 
 * @param unix_socket the file location of that socket 
 * @param lpLogger a logger object
 * @param bInit unused
 * @param mode change the mode of the file to this value (octal!)
 * 
 * @return the socket we're listening on, or -1 for failure.
 */
static int create_pipe_socket(const char *unix_socket, ECConfig *lpConfig,
    ECLogger *lpLogger, bool bInit, int mode)
{
	int s;
	int er = 0;
	struct sockaddr_un saddr;
	memset(&saddr, 0, sizeof(struct sockaddr_un));

	s = socket(PF_UNIX, SOCK_STREAM, 0);
	if(s < 0) {
		lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to create AF_UNIX socket.");
		return -1;
	}
	
	memset(&saddr,0,sizeof(saddr));
	saddr.sun_family = AF_UNIX;
	memcpy(saddr.sun_path,unix_socket,strlen(unix_socket)+1);
	
	unlink(unix_socket);

	if (bind(s, (struct sockaddr*)&saddr, 2 + strlen(unix_socket)) == -1) {
		lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to bind to socket %s (%s). This is usually caused by another process (most likely another zarafa-server) already using this port. This program will terminate now.", unix_socket, strerror(errno));
                kill(0, SIGTERM);
                exit(1);
        }

	er = chmod(unix_socket,mode);
	if(er) {
		lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to chmod socket %s. Error: %s", unix_socket, strerror(errno));
		closesocket(s);
		return -1;
	}

	if(er) {
		lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to chown socket %s, to %s:%s. Error: %s", unix_socket, lpConfig->GetSetting("run_as_user"), lpConfig->GetSetting("run_as_group"), strerror(errno));
		closesocket(s);
		return -1;
	}
	
	if(listen(s,8) == -1) {
		lpLogger->Log(EC_LOGLEVEL_FATAL, "Can't listen on unix socket %s", unix_socket);
		closesocket(s);
		return -1;
	}

	return s;
}

/* ALERT! Big hack!
 *
 * This function relocates an open file descriptor to a new file descriptor above 1024. The
 * reason we do this, is because although we support many open fd's up to FD_SETSIZE, libraries
 * that we use may not (most notably libldap). This means that if a new socket is allocated within
 * libldap as socket 1025, libldap will fail because it was compiled with FD_SETSIZE=1024. To fix
 * this problem, we make sure that most FD's under 1024 are free for use by external libraries, while
 * we use the range 1024 -> 8192
 */
int relocate_fd(int fd, ECLogger *lpLogger)
{
	// If we only have a 1024-fd limit, just return the original fd
	if(getdtablesize() <= 1024)
		return fd;

	int relocated = fcntl(fd, F_DUPFD, 1024);
	
	if(relocated < 0) {
		// OMG we have more than FD_SETSIZE-1024 sockets in use ?? Expect problems!
		lpLogger->Log(EC_LOGLEVEL_FATAL, "WARNING: Out of file descriptors, more than %d sockets in use. You cannot increase this value, so you must decrease socket usage.", getdtablesize()-1024);
		relocated = fd; // might as well try using FD's under 1024 ....
	} else {
		close(fd);
	}
	
	return relocated;
}

/*
 * Handles the HTTP GET command from soap, only the client update install may be downloaded.
 *
 * This function can only be called when client_update_enabled is set to yes.
 */
static int http_get(struct soap *soap) 
{
	int nRet = 404;

	if (!soap || !soap->path)
		goto exit;

	if (strncmp(soap->path, "/autoupdate", strlen("/autoupdate")) == 0) {
		g_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Client update request '%s'.", soap->path);
		nRet = HandleClientUpdate(soap);
	} else {
		g_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Unrecognized GET url '%s'.", soap->path);
	}

exit:
	soap_end_send(soap); 

	return nRet;
}

ECSoapServerConnection::ECSoapServerConnection(ECConfig* lpConfig, ECLogger* lpLogger)
{
	m_lpConfig = lpConfig;
	m_lpLogger = lpLogger;

#ifdef USE_EPOLL
	m_lpDispatcher = new ECDispatcherEPoll(lpLogger, lpConfig, ECSoapServerConnection::CreatePipeSocketCallback, this);
	m_lpLogger->Log(EC_LOGLEVEL_INFO, "Using epoll events");
#else
	m_lpDispatcher = new ECDispatcherSelect(lpLogger, lpConfig, ECSoapServerConnection::CreatePipeSocketCallback, this);
	m_lpLogger->Log(EC_LOGLEVEL_INFO, "Using select events");
#endif
}

ECSoapServerConnection::~ECSoapServerConnection(void)
{
    if(m_lpDispatcher)
        delete m_lpDispatcher;
}

ECRESULT ECSoapServerConnection::ListenTCP(const char* lpServerName, int nServerPort, bool bEnableGET)
{
	ECRESULT	er = erSuccess;
	int			socket = SOAP_INVALID_SOCKET;
	struct soap	*lpsSoap = NULL;

	if(lpServerName == NULL) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	//init soap
	lpsSoap = soap_new2(SOAP_IO_KEEPALIVE | SOAP_XML_TREE | SOAP_C_UTFSTRING, SOAP_IO_KEEPALIVE | SOAP_XML_TREE | SOAP_C_UTFSTRING);
	zarafa_new_soap_listener(CONNECTION_TYPE_TCP, lpsSoap);

	if (bEnableGET)
		lpsSoap->fget = http_get;

	lpsSoap->bind_flags = SO_REUSEADDR;

        lpsSoap->socket = socket = soap_bind(lpsSoap, lpServerName, nServerPort, 100);
        if (socket == -1) {
                m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to bind to port %d: %s. This is usually caused by another process (most likely another zarafa-server) already using this port. This program will terminate now.", nServerPort, lpsSoap->fault->faultstring);
                kill(0, SIGTERM);
                exit(1);
        }

	m_lpDispatcher->AddListenSocket(lpsSoap);
    
	// Manually check for attachments, independant of streaming support
	soap_post_check_mime_attachments(lpsSoap);

	m_lpLogger->Log(EC_LOGLEVEL_NOTICE, "Listening for TCP connections on port %d", nServerPort);

exit:
	if (er != erSuccess && lpsSoap)
		soap_free(lpsSoap);

	return er;
}

ECRESULT ECSoapServerConnection::ListenSSL(const char* lpServerName, int nServerPort, bool bEnableGET, const char* lpszKeyFile, const char* lpszKeyPass, const char* lpszCAFile, const char* lpszCAPath)
{
	ECRESULT	er = erSuccess;
	int			socket = SOAP_INVALID_SOCKET;
	struct soap	*lpsSoap = NULL;
	char *server_ssl_protocols = strdup(m_lpConfig->GetSetting("server_ssl_protocols"));
	const char *server_ssl_ciphers = m_lpConfig->GetSetting("server_ssl_ciphers");
	char *ssl_name = NULL;
	int ssl_op = 0, ssl_include = 0, ssl_exclude = 0;

	if(lpServerName == NULL) {
		free(server_ssl_protocols);
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	lpsSoap = soap_new2(SOAP_IO_KEEPALIVE | SOAP_XML_TREE | SOAP_C_UTFSTRING, SOAP_IO_KEEPALIVE | SOAP_XML_TREE | SOAP_C_UTFSTRING);
	zarafa_new_soap_listener(CONNECTION_TYPE_SSL, lpsSoap);

	if (bEnableGET)
		lpsSoap->fget = http_get;

	if (soap_ssl_server_context(
			lpsSoap,
			SOAP_SSL_DEFAULT,	// we set SSL_VERIFY_PEER and more soon ourselves
			lpszKeyFile,		// key file
			lpszKeyPass,		// key password
			lpszCAFile,			// CA certificate file which signed clients
			lpszCAPath,			// CA certificate path of thrusted sources
			NULL,				// dh file, null == rsa
			NULL,				// create random data on the fly (/dev/urandom is slow .. create file?)
			"zarafa")			// unique name for ssl session cache
		)
	{
		soap_set_fault(lpsSoap);
		m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to setup ssl context: %s", *soap_faultdetail(lpsSoap));
		er = ZARAFA_E_CALL_FAILED;
		goto exit;
	}

	SSL_CTX_set_options(lpsSoap->ctx, SSL_OP_ALL);

	ssl_name = strtok(server_ssl_protocols, " ");
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
			m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unknown protocol '%s' in server_ssl_protocols setting", ssl_name);
			er = ZARAFA_E_CALL_FAILED;
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

	if (server_ssl_protocols) {
		SSL_CTX_set_options(lpsSoap->ctx, ssl_op);
	}

	if (server_ssl_ciphers && SSL_CTX_set_cipher_list(lpsSoap->ctx, server_ssl_ciphers) != 1) {
		m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Can not set SSL cipher list to '%s': %s", server_ssl_ciphers, ERR_error_string(ERR_get_error(), 0));
		er = ZARAFA_E_CALL_FAILED;
		goto exit;
	}

	if (parseBool(m_lpConfig->GetSetting("server_ssl_prefer_server_ciphers"))) {
		SSL_CTX_set_options(lpsSoap->ctx, SSL_OP_CIPHER_SERVER_PREFERENCE);
	}

	// request certificate from client, is OK if not present.
	SSL_CTX_set_verify(lpsSoap->ctx, SSL_VERIFY_PEER | SSL_VERIFY_CLIENT_ONCE, NULL);

	lpsSoap->bind_flags = SO_REUSEADDR;

        lpsSoap->socket = socket = soap_bind(lpsSoap, lpServerName, nServerPort, 100);
        if (socket == -1) {
                m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to bind to port %d: %s (SSL). This is usually caused by another process (most likely another zarafa-server) already using this port. This program will terminate now.", nServerPort, lpsSoap->fault->faultstring);
                kill(0, SIGTERM);
                exit(1);
        }

	m_lpDispatcher->AddListenSocket(lpsSoap);

	// Manually check for attachments, independant of streaming support
	soap_post_check_mime_attachments(lpsSoap);

	m_lpLogger->Log(EC_LOGLEVEL_NOTICE, "Listening for SSL connections on port %d", nServerPort);

exit:
	if (er != erSuccess && lpsSoap) {
		soap_free(lpsSoap);
	}

	return er;
}

ECRESULT ECSoapServerConnection::ListenPipe(const char* lpPipeName, bool bPriority)
{
	ECRESULT	er = erSuccess;
	int			sPipe = -1;
	struct soap	*lpsSoap = NULL;

	if(lpPipeName == NULL) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	//init soap
	lpsSoap = soap_new2(SOAP_IO_KEEPALIVE | SOAP_XML_TREE | SOAP_C_UTFSTRING, SOAP_IO_KEEPALIVE | SOAP_XML_TREE | SOAP_C_UTFSTRING);
	if (bPriority)
		zarafa_new_soap_listener(CONNECTION_TYPE_NAMED_PIPE_PRIORITY, lpsSoap);
	else
		zarafa_new_soap_listener(CONNECTION_TYPE_NAMED_PIPE, lpsSoap);
	
	// Create a unix or windows pipe
	m_strPipeName = lpPipeName;
	// set the mode stricter for the priority socket: let only the same unix user or root connect on the priority socket, users should not be able to abuse the socket
	lpsSoap->socket = sPipe = create_pipe_socket(m_strPipeName.c_str(), m_lpConfig, m_lpLogger, true, bPriority ? 0660 : 0666);
	// This just marks the socket as being a pipe, which triggers some slightly different behaviour
	strcpy(lpsSoap->path,"pipe");

	if (sPipe == -1) {
		er = ZARAFA_E_CALL_FAILED;
		goto exit;
	}

	lpsSoap->master = sPipe;

	lpsSoap->peerlen = 0;
	memset((void*)&lpsSoap->peer, 0, sizeof(lpsSoap->peer));

	m_lpDispatcher->AddListenSocket(lpsSoap);

	// Manually check for attachments, independant of streaming support
	soap_post_check_mime_attachments(lpsSoap);

	m_lpLogger->Log(EC_LOGLEVEL_NOTICE, "Listening for %spipe connections on %s", bPriority ? "priority " : "", lpPipeName);

exit:
	if (er != erSuccess && lpsSoap) {
		soap_free(lpsSoap);
	}

	return er;
}

SOAP_SOCKET ECSoapServerConnection::CreatePipeSocketCallback(void *lpParam)
{
	ECSoapServerConnection *lpThis = (ECSoapServerConnection *)lpParam;

	return (SOAP_SOCKET)create_pipe_socket(lpThis->m_strPipeName.c_str(), lpThis->m_lpConfig, lpThis->m_lpLogger, false, 0666);
}

ECRESULT ECSoapServerConnection::ShutDown()
{
    return m_lpDispatcher->ShutDown();
}

ECRESULT ECSoapServerConnection::DoHUP()
{
	return m_lpDispatcher->DoHUP();
}

ECRESULT ECSoapServerConnection::MainLoop()
{	
    ECRESULT er = erSuccess;
    
    er = m_lpDispatcher->MainLoop();
    
	return er;
}

ECRESULT ECSoapServerConnection::NotifyDone(struct soap *soap)
{
    ECRESULT er = erSuccess;
    
    er = m_lpDispatcher->NotifyDone(soap);
    
    return er;
}

ECRESULT ECSoapServerConnection::GetStats(unsigned int *lpulQueueLength, double *lpdblAge,unsigned int *lpulThreadCount, unsigned int *lpulIdleThreads)
{
    ECRESULT er = erSuccess;
    
    er = m_lpDispatcher->GetQueueLength(lpulQueueLength);
    if(er != erSuccess)
        goto exit;
        
    er = m_lpDispatcher->GetFrontItemAge(lpdblAge);
    if(er != erSuccess)
        goto exit;
        
    er = m_lpDispatcher->GetThreadCount(lpulThreadCount, lpulIdleThreads);
    if(er != erSuccess)
        goto exit;

exit:    
    return er;
}
