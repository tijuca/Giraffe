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

#include <mapidefs.h>
#include <mapiutil.h>
#include <mapix.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <sys/socket.h>

#include <base64.h>
#include <ECChannel.h>
#include <ECDefs.h>
#include <stringutil.h>

#include "ECChannelClient.h"

ECChannelClient::ECChannelClient(const char *szPath, const char *szTokenizer)
{
	m_strTokenizer = szTokenizer;

	m_strPath = GetServerNameFromPath(szPath);

	if ((strncmp(szPath, "file", 4) == 0) || (szPath[0] == PATH_SEPARATOR)) {
		m_bSocket = true;
		m_ulPort = 0;
	} else {
		m_bSocket = false;
		m_ulPort = atoi(GetServerPortFromPath(szPath).c_str());
	}

	m_lpChannel = NULL;
	m_ulTimeout = 5;
}

ECChannelClient::~ECChannelClient()
{
	if (m_lpChannel)
		delete m_lpChannel;
}

ECRESULT ECChannelClient::DoCmd(const std::string &strCommand, std::vector<std::string> &lstResponse)
{
	ECRESULT er = erSuccess;
	std::string strResponse;

	er = Connect();
	if (er != erSuccess)
		goto exit;

	er = m_lpChannel->HrWriteLine(strCommand);
	if (er != erSuccess)
		goto exit;

	er = m_lpChannel->HrSelect(m_ulTimeout);
	if (er != erSuccess)
		goto exit;

	er = m_lpChannel->HrReadLine(&strResponse);
	if (er != erSuccess)
		goto exit;

	lstResponse = tokenize(strResponse, m_strTokenizer);

	if (!lstResponse.empty() && lstResponse.front() == "OK") {
		lstResponse.erase(lstResponse.begin());
	} else {
		er = ZARAFA_E_CALL_FAILED;
		goto exit;
	}

exit:
	return er;
}

ECRESULT ECChannelClient::Connect()
{
	ECRESULT er = erSuccess;

	if (!m_lpChannel) {
		if (m_bSocket)
			er = ConnectSocket();
		else
			er = ConnectHttp();
	}

	return er;
}

ECRESULT ECChannelClient::ConnectSocket()
{
	ECRESULT er = erSuccess;
	int fd = -1;
	struct sockaddr_un saddr;

	memset(&saddr, 0, sizeof(saddr));
	saddr.sun_family = AF_UNIX;
	strcpy(saddr.sun_path, m_strPath.c_str());

	if ((fd = socket(PF_UNIX, SOCK_STREAM, 0)) < 0) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	if (connect(fd, (struct sockaddr *)&saddr, sizeof(saddr)) < 0) {
		er = ZARAFA_E_NETWORK_ERROR;
		goto exit;
	}

	m_lpChannel = new ECChannel(fd);
	if (!m_lpChannel) {
		er = ZARAFA_E_NOT_ENOUGH_MEMORY;
		goto exit;
	}

exit:
	if (er != erSuccess && fd != -1)
		close(fd);

	return er;
}

ECRESULT ECChannelClient::ConnectHttp()
{
	ECRESULT er = erSuccess;
	int fd = -1;
	struct sockaddr_in saddr;

	memset(&saddr, 0, sizeof(saddr));
	saddr.sin_family = AF_INET;
	saddr.sin_addr.s_addr = inet_addr(m_strPath.c_str());
	saddr.sin_port = htons(m_ulPort);


	if ((fd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		er = ZARAFA_E_NETWORK_ERROR;
		goto exit;
	}

	if (connect(fd, (struct sockaddr *)&saddr, sizeof(saddr)) < 0) {
		er = ZARAFA_E_NETWORK_ERROR;
		goto exit;
	}

	m_lpChannel = new ECChannel(fd);
	if (!m_lpChannel) {
		er = ZARAFA_E_NOT_ENOUGH_MEMORY;
		goto exit;
	}

exit:
	if (er != erSuccess && fd != -1) {
		close(fd);
	}

	return er;
}
