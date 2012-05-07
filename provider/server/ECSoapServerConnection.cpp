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
int create_pipe_socket(const char *unix_socket, ECLogger *lpLogger, bool bInit, int mode) {
	int s;
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

	if(bind(s,(struct sockaddr*)&saddr,2 + strlen(unix_socket) ) < 0) {
		lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to bind unix socket %s", unix_socket);
		close(s);
		return -1;
	}

	chmod(unix_socket,mode);
	
	if(listen(s,8) == -1) {
		lpLogger->Log(EC_LOGLEVEL_FATAL, "Can't listen on unix socket %s", unix_socket);
		close(s);
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
int relocate_fd(int fd, ECLogger *lpLogger) {
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
	// Logger
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
	zarafa_new_soap_connection(CONNECTION_TYPE_TCP, lpsSoap, NULL, NULL);

	if (bEnableGET)
		lpsSoap->fget = http_get;

	lpsSoap->bind_flags = SO_REUSEADDR;

	lpsSoap->socket = socket = soap_bind(lpsSoap, lpServerName, nServerPort, 100);
	if (socket < 0) {
		soap_set_fault(lpsSoap);
		m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to start server on port %d: %s", nServerPort, lpsSoap->fault->faultstring);
		er = ZARAFA_E_CALL_FAILED;
		goto exit;
	}

    m_lpDispatcher->AddListenSocket(lpsSoap);
    
	// Manually check for attachments, independant of streaming support
	soap_post_check_mime_attachments(lpsSoap);

	m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Listening for TCP connections on port %d", nServerPort);

exit:
	if (er != erSuccess && lpsSoap) {
		soap_free(lpsSoap);
	}

	return er;
}

ECRESULT ECSoapServerConnection::ListenSSL(const char* lpServerName, int nServerPort, bool bEnableGET,
										   const char* lpszKeyFile, const char* lpszKeyPass,
										   const char* lpszCAFile, const char* lpszCAPath)
{
	ECRESULT	er = erSuccess;
	int			socket = SOAP_INVALID_SOCKET;
	struct soap	*lpsSoap = NULL;

	if(lpServerName == NULL) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	lpsSoap = soap_new2(SOAP_IO_KEEPALIVE | SOAP_XML_TREE | SOAP_C_UTFSTRING, SOAP_IO_KEEPALIVE | SOAP_XML_TREE | SOAP_C_UTFSTRING);
	zarafa_new_soap_connection(CONNECTION_TYPE_SSL, lpsSoap, NULL, NULL);

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
	
	// request certificate from client, is OK if not present.
	SSL_CTX_set_verify(lpsSoap->ctx, SSL_VERIFY_PEER | SSL_VERIFY_CLIENT_ONCE, NULL);

	lpsSoap->bind_flags = SO_REUSEADDR;

	lpsSoap->socket = socket = soap_bind(lpsSoap, lpServerName, nServerPort, 100);
	if (socket < 0) {
		soap_set_fault(lpsSoap);
		m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to start SSL server on port %d: %s", nServerPort, lpsSoap->fault->faultstring);
		er = ZARAFA_E_CALL_FAILED;
		goto exit;
	}


    m_lpDispatcher->AddListenSocket(lpsSoap);

	// Manually check for attachments, independant of streaming support
	soap_post_check_mime_attachments(lpsSoap);

	m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Listening for SSL connections on port %d", nServerPort);

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
		zarafa_new_soap_connection(CONNECTION_TYPE_NAMED_PIPE_PRIORITY, lpsSoap, NULL, NULL);
	else
		zarafa_new_soap_connection(CONNECTION_TYPE_NAMED_PIPE, lpsSoap, NULL, NULL);
	
	// Create a unix or windows pipe
	m_strPipeName = lpPipeName;
	// set the mode stricter for the priority socket: let only the same unix user or root connect on the priority socket, users should not be able to abuse the socket
	lpsSoap->socket = sPipe = create_pipe_socket(m_strPipeName.c_str(), m_lpLogger, true, bPriority ? 0660 : 0666);
	// This just marks the socket as being a pipe, which triggers some slightly different behaviour
	strcpy(lpsSoap->path,"pipe");

	if (sPipe == -1) {
		er = ZARAFA_E_CALL_FAILED;
		goto exit;
	}

	lpsSoap->master = sPipe;

	m_lpDispatcher->AddListenSocket(lpsSoap);

	// Manually check for attachments, independant of streaming support
	soap_post_check_mime_attachments(lpsSoap);

	m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Listening for %spipe connections on %s", bPriority ? "priority " : "", lpPipeName);

exit:
	if (er != erSuccess && lpsSoap) {
		soap_free(lpsSoap);
	}

	return er;
}

SOAP_SOCKET ECSoapServerConnection::CreatePipeSocketCallback(void *lpParam)
{
	ECSoapServerConnection *lpThis = (ECSoapServerConnection *)lpParam;

	return (SOAP_SOCKET)create_pipe_socket(lpThis->m_strPipeName.c_str(), lpThis->m_lpLogger, false, 0666);
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
