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
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <cctype>
#include <algorithm>

#include <mapi.h>
#include <mapix.h>
#include <mapicode.h>
#include <mapidefs.h>
#include <mapiutil.h>
#include <inetmapi.h>
#include <mapiext.h>

#include <CommonUtil.h>
#include <ECTags.h>
#include "ECChannel.h"
#include "LMTP.h"
#include "stringutil.h"

using namespace std;


LMTP::LMTP(ECChannel *lpChan, char *szServerPath, ECLogger *lpLog, ECConfig *lpConf) {
    m_lpChannel = lpChan;
    m_lpLogger = lpLog;
    m_lpConfig = lpConf;
    m_strPath = szServerPath;
}

LMTP::~LMTP() {
}

// MAIL FROM:<john@zarafa.com>
// MAIL FROM:< iets@hier.nl>

/** 
 * Tests the start of the input for the LMTP command. LMTP is case
 * insensitive.
 * LMTP commands are:
 * @arg LHLO
 * @arg MAIL FROM:
 * @arg RCPT TO:
 * @arg DATA
 * @arg RSET
 * @arg QUIT
 * 
 * @param[in]  strCommand The received line from the LMTP client
 * @param[out] eCommand enum describing the received command
 * 
 * @return MAPI error code
 * @retval MAPI_E_CALL_FAILED unknown or unsupported command received
 */
HRESULT LMTP::HrGetCommand(const string &strCommand, LMTP_Command &eCommand)
{
	HRESULT hr = hrSuccess;
	
	if (strnicmp(strCommand.c_str(), "LHLO", strlen("LHLO")) == 0)
		eCommand = LMTP_Command_LHLO;
	else if (strnicmp(strCommand.c_str(), "MAIL FROM:", strlen("MAIL FROM:")) == 0)
		eCommand = LMTP_Command_MAIL_FROM;
	else if (strnicmp(strCommand.c_str(), "RCPT TO:", strlen("RCPT TO:")) == 0)
		eCommand = LMTP_Command_RCPT_TO;
	else if (strnicmp(strCommand.c_str(), "DATA", strlen("DATA")) == 0)
		eCommand = LMTP_Command_DATA;
	else if (strnicmp(strCommand.c_str(), "RSET", strlen("RSET")) == 0)
		eCommand = LMTP_Command_RSET;
	else if (strnicmp(strCommand.c_str(), "QUIT", strlen("QUIT")) == 0)
		eCommand = LMTP_Command_QUIT;
	else
		hr = MAPI_E_CALL_FAILED;
	
	return hr;
}

/** 
 * Send the following response to the LMTP client.
 * 
 * @param[in] strResponse String to send
 * 
 * @return Possible error during write to the client
 */
HRESULT LMTP::HrResponse(const string &strResponse)
{
	HRESULT hr;

	if (m_lpLogger->Log(EC_LOGLEVEL_DEBUG))
		m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "< %s", strResponse.c_str());

	hr = m_lpChannel->HrWriteLine(strResponse);
	if (hr != hrSuccess)
		m_lpLogger->Log(EC_LOGLEVEL_FATAL, "LMTP write error");

	return hr;
}

/** 
 * Parse the received string for a valid LHLO command.
 * 
 * @param[in] strInput the full LHLO command received
 * 
 * @return always hrSuccess
 */
HRESULT LMTP::HrCommandLHLO(const string &strInput)
{
	if (m_lpLogger->Log(EC_LOGLEVEL_DEBUG)) {
		// Input definitly starts with LHLO
		// use HrResponse("501 5.5.4 Syntax: LHLO hostname"); in case of error, but we don't.
		size_t pos = strInput.find(' ');
		m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "LHLO ID: %s", strInput.c_str() + pos + 1);
	}
	return hrSuccess;
}

/** 
 * Parse the received string for a valid MAIL FROM: command.
 * The correct syntax for the MAIL FROM is:
 *  MAIL FROM:<email@address.domain>
 *
 * However, it's possible extra spaces are added in the string, and we
 * should correctly accept this to deliver the mail.
 * We ignore the contents from the address, and use the From: header.
 * 
 * @param[in] strFrom the full MAIL FROM command
 * 
 * @return MAPI error code
 * @retval MAPI_E_NOT_FOUND < or > character was not found: this is fatal.
 */
HRESULT LMTP::HrCommandMAILFROM(const string &strFrom)
{
	HRESULT hr = hrSuccess;
	std::string strAddress;

	// strFrom is only checked for syntax
	hr = HrParseAddress(strFrom, &strAddress);
	// Discard the address, but not the error!. We use the From: header in the mail.
	return hr;
}

/** 
 * Parse the received string for a valid RCPT TO: command.
 * 
 * @param[in]  strTo the full RCPT TO command
 * @param[out] strUnresolved the parsed email address from the command, user will be resolved by DAgent.
 * 
 * @return MAPI error code
 * @retval MAPI_E_NOT_FOUND < or > character was not found: this is fatal.
 */
HRESULT LMTP::HrCommandRCPTTO(const string &strTo, string *strUnresolved)
{
	HRESULT hr = hrSuccess;

	hr = HrParseAddress(strTo, strUnresolved);
	
	if (hr == hrSuccess) {
		if(m_lpLogger->Log(EC_LOGLEVEL_DEBUG))
		    m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Parsed command '%s' to recipient address '%s'", strTo.c_str(), strUnresolved->c_str());
    } else {
	    m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Invalid recipient address in command '%s'", strTo.c_str());
    }
	
	return hr;
}

/** 
 * Receive the DATA from the client and save to a file using \r\n
 * enters. This file will be mmap()ed by the DAgent.
 * 
 * @param[in] tmp a FILE pointer to a temporary file with write access
 * 
 * @return MAPI error code, read/write errors from client.
 */
HRESULT LMTP::HrCommandDATA(FILE *tmp)
{
	HRESULT hr = hrSuccess;
	std::string inBuffer;
	std::string message;
	int offset;

	hr = HrResponse("354 2.1.5 Start mail input; end with <CRLF>.<CRLF>");
	if (hr != hrSuccess)
		goto exit;

	// Now the mail body needs to be read line by line until <CRLF>.<CRLF> is encountered
	while (1) {
		hr = m_lpChannel->HrReadLine(&inBuffer);
		if(hr != hrSuccess)
			goto exit;
		
		if(inBuffer == ".")
		    break;

		offset = 0;
		if (inBuffer[0] == '.')
			offset = 1;			// "remove" escape point, since it wasn't the end of mail marker

		fwrite((char *)inBuffer.c_str() + offset, 1, inBuffer.size() - offset, tmp);

		// The data from HrReadLine does not contain the CRLF, so add that here
		fwrite("\r\n", 1, 2, tmp);

		message += inBuffer + "\r\n";
	}
	if (m_lpLogger->Log(EC_LOGLEVEL_DEBUG +1)) // really hidden output (limited to 10k in logger)
		m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Received message:\n" + message);

exit:
	if (hr != hrSuccess)
		m_lpLogger->Log(EC_LOGLEVEL_ERROR, "Error during DATA communication with client.");

	return hr;
}

/** 
 * Handle the very difficult QUIT command.
 * 
 * @return always hrSuccess
 */
HRESULT LMTP::HrCommandQUIT()
{
	return hrSuccess;
}

/** 
 * Parse an address given in a MAIL FROM or RCPT TO command.
 * 
 * @param[in]  strInput a full MAIL FROM or RCPT TO command
 * @param[out] strAddress the address found in the command
 * 
 * @return MAPI error code
 * @retval MAPI_E_NOT_FOUND mandatory < or > not found in command.
 */
HRESULT LMTP::HrParseAddress(const std::string &strInput, std::string *strAddress)
{
	std::string strAddr;
	size_t pos1;
	size_t pos2;

	pos1 = strInput.find('<');
	pos2 = strInput.find('>', pos1);

	if (pos1 == std::string::npos || pos2 == std::string::npos)
		return MAPI_E_NOT_FOUND;

	strAddr = strInput.substr(pos1+1, pos2-pos1-1);

	trim(strAddr);

	strAddress->swap(strAddr);

	return hrSuccess;
}
