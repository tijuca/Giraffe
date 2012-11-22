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

#ifndef ECCHANNEL_H
#define ECCHANNEL_H

#include <stdio.h>
#include <iostream>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include <string>

#include "ECConfig.h"
#include "ECLogger.h"

// ECChannel is the communication channel with the other side. Initially, it is
// a simple way to read/write full lines of data. The reason why we specify
// a special 'HrWriteLine' instead of 'HrWrite' is that SSL encryption prefers
// writing all the data at once, instead of via multiple write() calls. Also, 
// this ensures that the ECChannel class is responsible for reading, writing
// and culling newline characters.

class ECChannel {
public:
	ECChannel(int socket);
	~ECChannel();

	HRESULT HrEnableTLS();

	HRESULT HrGets(char *szBuffer, ULONG ulBufSize, ULONG *lpulRead);
	HRESULT HrReadLine(std::string * strBuffer, ULONG ulMaxBuffer = 65536);
	HRESULT HrWriteString(char * szBuffer);
	HRESULT HrWriteString(const std::string & strBuffer);
	HRESULT HrWriteLine(char *szBuffer, int len = 0);
	HRESULT HrWriteLine(const std::string & strBuffer);
	HRESULT HrReadBytes(char *szBuffer, ULONG ulByteCount);
	HRESULT HrReadBytes(std::string * strBuffer, ULONG ulByteCount);
	HRESULT HrReadAndDiscardBytes(ULONG ulByteCount);

	HRESULT HrSelect(int seconds);

	void SetIPAddress(char *szIPAddress);
	std::string GetIPAddress();
		
	bool UsingSsl();
	bool sslctx();

	static HRESULT HrSetCtx(ECConfig * lpConfig, ECLogger * lpLogger);
	static HRESULT HrFreeCtx();

private:
	int fd;
	SSL *lpSSL;
	static SSL_CTX *lpCTX;
	std::string strIP;

	char *fd_gets(char *buf, int *lpulLen);
	char *SSL_gets(char *buf, int *lpulLen);
};

/* helpers to open socket */
HRESULT HrListen(ECLogger *lpLogger, const char *szPath, int *lpulListenSocket);
HRESULT HrListen(ECLogger *lpLogger, const char *szBind, int ulPort, int *lpulListenSocket);
/* accept data on connection */
HRESULT HrAccept(ECLogger *lpLogger, int ulListenFD, ECChannel **lppChannel);

#endif
