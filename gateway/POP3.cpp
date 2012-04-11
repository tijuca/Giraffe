/*
 * Copyright 2005 - 2009  Zarafa B.V.
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
#include <algorithm>

#include <mapi.h>
#include <mapix.h>
#include <mapicode.h>
#include <mapidefs.h>
#include <mapiutil.h>

#include <CommonUtil.h>
#include <Util.h>
#include <ECTags.h>
#include <inetmapi.h>
#include <mapiext.h>

#include "stringutil.h"
#include "charset/convert.h"
#include "charset/utf8string.h"
#include "ECFeatures.h"

#include "POP3.h"
using namespace std;

/**
 * @ingroup gateway_pop3
 * @{
 */

POP3::POP3(const char *szServerPath, ECChannel *lpChannel, ECLogger *lpLogger, ECConfig *lpConfig) : ClientProto(szServerPath, lpChannel, lpLogger, lpConfig) {
	lpSession = NULL;
	lpStore = NULL;
	lpInbox = NULL;
	lpAddrBook = NULL;

	imopt_default_sending_options(&sopt);
	sopt.no_recipients_workaround = true;	// do not stop processing mail on empty recipient table
	sopt.add_received_date = true;			// add Received header (outlook uses this)
}

POP3::~POP3() {
	for (vector<MailListItem>::iterator i = lstMails.begin(); i != lstMails.end(); i++) {
		delete [] i->sbEntryID.lpb;
	}

	if (lpInbox)
		lpInbox->Release();

	if (lpStore)
		lpStore->Release();

	if (lpSession)
		lpSession->Release();

	if (lpAddrBook)
		lpAddrBook->Release();
}

/** 
 * Returns number of minutes to keep connection alive
 * 
 * @return user logged in (true) or not (false)
 */
int POP3::getTimeoutMinutes() {
	if (lpStore != NULL)
		return 5;				// 5 minutes when logged in
	else
		return 1;				// 1 minute when not logged in
}

HRESULT POP3::HrSendGreeting(const std::string &strHostString) {
	HRESULT hr = hrSuccess;

	if (parseBool(lpConfig->GetSetting("server_hostname_greeting")))
		hr = HrResponse(POP3_RESP_OK, "Zarafa POP3 gateway ready" + strHostString);
	else
		hr = HrResponse(POP3_RESP_OK, "Zarafa POP3 gateway ready");

	return hr;
}

/** 
 * Send client an error message that the socket will be closed by the server
 * 
 * @param[in] strQuitMsg quit message for client
 * @return MAPI error code
 */
HRESULT POP3::HrCloseConnection(const std::string &strQuitMsg)
{
	return HrResponse(POP3_RESP_ERR, strQuitMsg);
}

/** 
 * Process the requested command from the POP3 client
 * 
 * @param[in] strIput received input from client
 * 
 * @return MAPI error code
 */
HRESULT POP3::HrProcessCommand(const std::string &strInput)
{
	HRESULT hr = hrSuccess;
	vector<string> vWords;
	string strCommand;

	vWords = tokenize(strInput, ' ');
	if (vWords.empty()) {
		lpLogger->Log(EC_LOGLEVEL_WARNING, "Empty line received");
		hr = MAPI_E_CALL_FAILED;
		goto exit;
	}

	if (lpLogger->Log(EC_LOGLEVEL_DEBUG))
		lpLogger->Log(EC_LOGLEVEL_DEBUG, "Command received: %s", vWords[0].c_str());

	strCommand = vWords[0];
	transform(strCommand.begin(), strCommand.end(), strCommand.begin(), ::toupper);

	if (strCommand.compare("USER") == 0) {
		if (vWords.size() != 2) {
			hr = HrResponse(POP3_RESP_ERR, "User command must have 1 argument");
			goto exit;
		}
		hr = HrCmdUser(vWords[1]);
	} else if (strCommand.compare("PASS") == 0) {
		if (vWords.size() < 2) {
			hr = HrResponse(POP3_RESP_ERR, "Pass command must have 1 argument");
			goto exit;
		}
		string strPass = strInput;
		strPass.erase(0, strCommand.length()+1);
		hr = HrCmdPass(strPass);
	} else if (strCommand.compare("STAT") == 0) {
		if (vWords.size() != 1) {
			hr = HrResponse(POP3_RESP_ERR, "Stat command has no arguments");
			goto exit;
		}
		hr = HrCmdStat();
	} else if (strCommand.compare("LIST") == 0) {
		if (vWords.size() > 2) {
			hr = HrResponse(POP3_RESP_ERR, "List must have 0 or 1 arguments");
			goto exit;
		}

		if (vWords.size() == 2) {
			hr = HrCmdList(strtoul(vWords[1].c_str(), NULL, 0));
		} else {
			hr = HrCmdList();
		}
	} else if (strCommand.compare("RETR") == 0) {
		if (vWords.size() != 2) {
			hr = HrResponse(POP3_RESP_ERR, "RETR must have 1 argument");
			goto exit;
		}
		hr = HrCmdRetr(strtoul(vWords[1].c_str(), NULL, 0));
	} else if (strCommand.compare("DELE") == 0) {
		if (vWords.size() != 2) {
			hr = HrResponse(POP3_RESP_ERR, "DELE must have 1 argument");
			goto exit;
		}
		hr = HrCmdDele(strtoul(vWords[1].c_str(), NULL, 0));
	} else if (strCommand.compare("NOOP") == 0) {
		if (vWords.size() > 1) {
			hr = HrResponse(POP3_RESP_ERR, "NOOP must have 0 arguments");
			goto exit;
		}
		hr = HrCmdNoop();
	} else if (strCommand.compare("RSET") == 0) {
		if (vWords.size() > 1) {
			hr = HrResponse(POP3_RESP_ERR, "RSET must have 0 arguments");
			goto exit;
		}
		hr = HrCmdRset();
	} else if (strCommand.compare("TOP") == 0) {
		if (vWords.size() != 3) {
			hr = HrResponse(POP3_RESP_ERR, "TOP must have 2 arguments");
			goto exit;
		}

		hr = HrCmdTop(strtoul(vWords[1].c_str(), NULL, 0), strtoul(vWords[2].c_str(), NULL, 0));
	} else if (strCommand.compare("UIDL") == 0) {
		if (vWords.size() > 2) {
			hr = HrResponse(POP3_RESP_ERR, "UIDL must have 0 or 1 arguments");
			goto exit;
		}

		if (vWords.size() == 2) {
			hr = HrCmdUidl(strtoul(vWords[1].c_str(), NULL, 0));
		} else {
			hr = HrCmdUidl();
		}
	} else if (strCommand.compare("QUIT") == 0) {
		hr = HrCmdQuit();
		// let the gateway quit from the socket read loop
		hr = MAPI_E_END_OF_SESSION;
	} else {
		hr = HrResponse(POP3_RESP_ERR, "Function not (yet) implemented");
		lpLogger->Log(EC_LOGLEVEL_ERROR, "non-existing function called: %s", vWords[0].c_str());
		hr = MAPI_E_CALL_FAILED;
	}

exit:
	return hr;
}

/** 
 * Cleanup connection
 * 
 * @return hrSuccess
 */
HRESULT POP3::HrDone(bool bSendResponse)
{
	// no cleanup for POP3 required
	return hrSuccess;
}

/** 
 * Send a response to the client, either +OK or -ERR
 * 
 * @param[in] strResult +OK or -ERR result (use defines)
 * @param[in] strResponse string to send to client with given result
 * 
 * @return MAPI Error code
 */
HRESULT POP3::HrResponse(const string &strResult, const string &strResponse) {
    if(lpLogger->Log(EC_LOGLEVEL_DEBUG))
		lpLogger->Log(EC_LOGLEVEL_DEBUG, "%s%s", strResult.c_str(), strResponse.c_str());
	return lpChannel->HrWriteLine(strResult + strResponse);
}

/** 
 * @brief Handle the USER command
 *
 * Stores the username in the class, since the password is in a second call
 *
 * @param[in] strUser loginname of the user who wants to login
 * 
 * @return MAPI Error code
 */
HRESULT POP3::HrCmdUser(const string &strUser) {
	HRESULT hr = hrSuccess;
	if (lpStore != NULL) {
		hr = HrResponse(POP3_RESP_ERR, "Can't login twice");
	} else if (strUser.length() > POP3_MAX_RESPONSE_LENGTH) {
		lpLogger->Log(EC_LOGLEVEL_ERROR, "Username too long: %d > %d", (int)strUser.length(), POP3_MAX_RESPONSE_LENGTH);
		hr = HrResponse(POP3_RESP_ERR, "Username to long");
	} else {
		szUser = strUser;
		hr = HrResponse(POP3_RESP_OK, "Waiting for password");
	}
	return hr;
}

/** 
 * @brief Handle the PASS command
 *
 * Now that we have the password, we can login.
 *
 * @param[in] strPass password of the user to login with
 * 
 * @return MAPI Error code
 */
HRESULT POP3::HrCmdPass(const string &strPass) {
	HRESULT hr = hrSuccess;

	if (lpStore != NULL) {
		hr = HrResponse(POP3_RESP_ERR, "Can't login twice");
	} else if (strPass.length() > POP3_MAX_RESPONSE_LENGTH) {
		lpLogger->Log(EC_LOGLEVEL_ERROR, "Password too long: %d > %d", (int)strPass.length(), POP3_MAX_RESPONSE_LENGTH);
		hr = HrResponse(POP3_RESP_ERR, "Password to long");
	} else if (szUser.empty()) {
		hr = HrResponse(POP3_RESP_ERR, "Give username first");
	} else {
		hr = this->HrLogin(szUser, strPass);
		if (hr != hrSuccess) {
			HrResponse(POP3_RESP_ERR, "Wrong username or password");
			goto exit;
		}

		hr = this->HrMakeMailList();
		if (hr != hrSuccess) {
			HrResponse(POP3_RESP_ERR, "Can't get mail list");
			goto exit;
		}

		hr = HrResponse(POP3_RESP_OK, "Username and password accepted");
	}

exit:
	return hr;
}

/** 
 * @brief Handle the STAT command
 *
 * STAT displays the number of messages and the total size of the Inbox 
 * 
 * @return MAPI Error code
 */
HRESULT POP3::HrCmdStat() {
	HRESULT hr = hrSuccess;
	ULONG ulSize = 0;
	char szResponse[POP3_MAX_RESPONSE_LENGTH];

	for (size_t i = 0; i < lstMails.size(); i++) {
		ulSize += lstMails[i].ulSize;
	}

	snprintf(szResponse, POP3_MAX_RESPONSE_LENGTH, "%u %u", (ULONG)lstMails.size(), ulSize);
	hr = HrResponse(POP3_RESP_OK, szResponse);

	return hr;
}

/** 
 * @brief Handle the LIST command
 * 
 * List shows for every message the number to retrieve the message
 * with and the size of the message. Since we don't know a client
 * which uses this size exactly, we can use the table version.
 * 
 * @return MAPI Error code
 */
HRESULT POP3::HrCmdList() {
	HRESULT hr = hrSuccess;
	char szResponse[POP3_MAX_RESPONSE_LENGTH];

	snprintf(szResponse, POP3_MAX_RESPONSE_LENGTH, "%u messages", (ULONG)lstMails.size());
	hr = HrResponse(POP3_RESP_OK, szResponse);
	if (hr != hrSuccess)
		goto exit;

	for (size_t i = 0; i < lstMails.size(); i++) {
		snprintf(szResponse, POP3_MAX_RESPONSE_LENGTH, "%u %u", (ULONG)i + 1, lstMails[i].ulSize);
		hr = lpChannel->HrWriteLine(szResponse);
		if (hr != hrSuccess)
			goto exit;
	}

	hr = lpChannel->HrWriteLine(".");

exit:
	return hr;
}

/** 
 * @brief Handle the LIST <number> command
 *
 * Shows the size of the given mail number.  
 *
 * @param[in] ulMailNr number of the email, starting at 1
 * 
 * @return MAPI Error code
 */
HRESULT POP3::HrCmdList(unsigned int ulMailNr) {
	HRESULT hr = hrSuccess;
	char szResponse[POP3_MAX_RESPONSE_LENGTH];

	if (ulMailNr > lstMails.size() || ulMailNr < 1) {
		HrResponse(POP3_RESP_ERR, "Wrong mail number");
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}

	snprintf(szResponse, POP3_MAX_RESPONSE_LENGTH, "%u %u", ulMailNr, lstMails[ulMailNr - 1].ulSize);
	hr = HrResponse(POP3_RESP_OK, szResponse);

exit:
	return hr;
}

/** 
 * @brief Handle the RETR command
 *
 * Retrieve the complete mail for a given number
 * 
 * @param[in] ulMailNr number of the email, starting at 1
 * 
 * @return 
 */
HRESULT POP3::HrCmdRetr(unsigned int ulMailNr) {
	HRESULT hr = hrSuccess;
	LPMESSAGE lpMessage = NULL;
	LPSTREAM lpStream = NULL;
	ULONG ulObjType;
	string strMessage;
	char *szMessage = NULL;
	char szResponse[POP3_MAX_RESPONSE_LENGTH];

	if (ulMailNr < 1 || ulMailNr > lstMails.size()) {
		HrResponse(POP3_RESP_ERR, "mail nr not found");
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}

	hr = lpStore->OpenEntry(lstMails[ulMailNr - 1].sbEntryID.cb, (LPENTRYID) lstMails[ulMailNr - 1].sbEntryID.lpb, &IID_IMessage, MAPI_DEFERRED_ERRORS,
							&ulObjType, (LPUNKNOWN *) &lpMessage);
	if (hr != hrSuccess) {
		HrResponse(POP3_RESP_ERR, "Failing to open entry");
		goto exit;
	}

	hr = lpMessage->OpenProperty(PR_EC_IMAP_EMAIL, &IID_IStream, 0, 0, (LPUNKNOWN*)&lpStream);
	if (hr == hrSuccess) {
		hr = Util::HrStreamToString(lpStream, strMessage);
		if (hr == hrSuccess)
			strMessage = DotFilter(strMessage.c_str());
	}
	if (hr != hrSuccess) {
		// unable to load streamed version, so try full conversion.
		hr = IMToINet(lpSession, lpAddrBook, lpMessage, &szMessage, sopt, lpLogger);
		if (hr != hrSuccess) {
			lpLogger->Log(EC_LOGLEVEL_ERROR, "Error converting MAPI to MIME: 0x%08x", hr);
			HrResponse(POP3_RESP_ERR, "Converting MAPI to MIME error");
			goto exit;
		}

		strMessage = DotFilter(szMessage);
	}

	snprintf(szResponse, POP3_MAX_RESPONSE_LENGTH, "%u octets", (ULONG)strMessage.length());
	HrResponse(POP3_RESP_OK, szResponse);

	lpChannel->HrWriteLine(strMessage);
	lpChannel->HrWriteLine(".");

exit:
	if (lpStream)
		lpStream->Release();

	if (lpMessage)
		lpMessage->Release();

	if (szMessage)
		delete [] szMessage;

	return hr;
}

/** 
 * @brief Handle the DELE command
 *
 * Mark an email for deletion after the QUIT command
 * 
 * @param[in] ulMailNr number of the email, starting at 1
 * 
 * @return MAPI Error code
 */
HRESULT POP3::HrCmdDele(unsigned int ulMailNr) {
	HRESULT hr = hrSuccess;

	if (ulMailNr < 1 || ulMailNr > lstMails.size()) {
		hr = HrResponse(POP3_RESP_ERR, "mail nr not found");
		if (hr == hrSuccess)
			hr = MAPI_E_NOT_FOUND;
		goto exit;
	}

	lstMails[ulMailNr - 1].bDeleted = true;
	hr = HrResponse(POP3_RESP_OK, "mail deleted");

exit:
	return hr;
}

/** 
 * @brief Handle the NOOP command
 * 
 * Sends empty OK string to client
 * @return MAPI Error code
 */
HRESULT POP3::HrCmdNoop() {
	return HrResponse(POP3_RESP_OK, string());
}

/** 
 * @brief Handle the RSET command
 * 
 * Resets the connections, sets every email to not deleted.
 * 
 * @return MAPI Error code
 */
HRESULT POP3::HrCmdRset() {
	for (vector<MailListItem>::iterator i = lstMails.begin(); i != lstMails.end(); i++) {
		i->bDeleted = false;
	}

	return HrResponse(POP3_RESP_OK, "Undeleted mails");
}

/** 
 * @brief Handle the QUIT command
 *
 * Delete all delete marked emails and close the connection. 
 * 
 * @return MAPI Error code
 */
HRESULT POP3::HrCmdQuit() {
	HRESULT hr = hrSuccess;
	unsigned int DeleteCount = 0;
	SBinaryArray ba = {0, NULL};
	vector<MailListItem>::iterator i;

	for (i = lstMails.begin(); i != lstMails.end(); i++) {
		if (i->bDeleted) {
			DeleteCount++;
		}
	}

	if (DeleteCount) {
		ba.cValues = DeleteCount;
		ba.lpbin = new SBinary[DeleteCount];
		DeleteCount = 0;

		for (i = lstMails.begin(); i != lstMails.end(); i++) {
			if (i->bDeleted) {
				ba.lpbin[DeleteCount] = i->sbEntryID;
				DeleteCount++;
			}
		}

		lpInbox->DeleteMessages(&ba, 0, NULL, 0);
		// ignore error, we always send the Bye to the client
	}

	hr = HrResponse(POP3_RESP_OK, "Bye");

	if (ba.lpbin)
		delete [] ba.lpbin;

	return hr;
}

/** 
 * @brief Handle the UIDL command 
 * 
 * List all messages by number and Unique ID (EntryID). This ID must
 * be valid over different sessions.
 *
 * @return MAPI Error code
 */
HRESULT POP3::HrCmdUidl() {
	HRESULT hr = hrSuccess;
	char szResponse[POP3_MAX_RESPONSE_LENGTH];
	string strResponse;

	hr = lpChannel->HrWriteLine("+OK");
	if (hr != hrSuccess)
		goto exit;

	for (size_t i = 0; i < lstMails.size(); i++) {
		snprintf(szResponse, POP3_MAX_RESPONSE_LENGTH, "%u ", (ULONG)i + 1);
		strResponse = szResponse;
		strResponse += bin2hex(lstMails[i].sbEntryID.cb, lstMails[i].sbEntryID.lpb);

		hr = lpChannel->HrWriteLine(strResponse);
		if (hr != hrSuccess)
			goto exit;
	}

	hr = lpChannel->HrWriteLine(".");
	if (hr != hrSuccess)
		goto exit;

exit:
	return hr;
}

/** 
 * @brief Handle the UIDL <number> command
 * 
 * List the given message number by number and Unique ID
 * (EntryID). This ID must be valid over different sessions.
 *
 * @param ulMailNr number of the email to get the Unique ID for
 * 
 * @return MAPI Error code
 */
HRESULT POP3::HrCmdUidl(unsigned int ulMailNr) {
	HRESULT hr = hrSuccess;
	string strResponse;
	char szResponse[POP3_MAX_RESPONSE_LENGTH];

	if (ulMailNr < 1 || ulMailNr > lstMails.size()) {
		hr = HrResponse(POP3_RESP_ERR, "mail nr not found");
		if (hr == hrSuccess)
			hr = MAPI_E_NOT_FOUND;
		goto exit;
	}

	snprintf(szResponse, POP3_MAX_RESPONSE_LENGTH, "%u ", ulMailNr);
	strResponse = szResponse;
	strResponse += bin2hex(lstMails[ulMailNr - 1].sbEntryID.cb, lstMails[ulMailNr - 1].sbEntryID.lpb);

	hr = HrResponse(POP3_RESP_OK, strResponse);

exit:
	return hr;
}

/** 
 * @brief Handle the TOP command
 *
 * List the first N body lines of an email. The headers are always sent.
 * 
 * @param[in] ulMailNr The email to list
 * @param[in] ulLines The number of lines of the email to send
 * 
 * @return MAPI Error code
 */
HRESULT POP3::HrCmdTop(unsigned int ulMailNr, unsigned int ulLines) {
	HRESULT hr = hrSuccess;
	LPMESSAGE lpMessage = NULL;
	LPSTREAM lpStream = NULL;
	ULONG ulObjType;
	char *szMessage = NULL;
	string strMessage;
	string::size_type ulPos;

	if (ulMailNr < 1 || ulMailNr > lstMails.size()) {
		hr = HrResponse(POP3_RESP_ERR, "mail nr not found");
		if (hr == hrSuccess)
			hr = MAPI_E_NOT_FOUND;
		goto exit;
	}

	hr = lpStore->OpenEntry(lstMails[ulMailNr - 1].sbEntryID.cb, (LPENTRYID) lstMails[ulMailNr - 1].sbEntryID.lpb, &IID_IMessage, MAPI_DEFERRED_ERRORS,
							&ulObjType, (LPUNKNOWN *) &lpMessage);
	if (hr != hrSuccess) {
		HrResponse(POP3_RESP_ERR, "Failing to open entry");
		goto exit;
	}

	hr = lpMessage->OpenProperty(PR_EC_IMAP_EMAIL, &IID_IStream, 0, 0, (LPUNKNOWN*)&lpStream);
	if (hr == hrSuccess)
		hr = Util::HrStreamToString(lpStream, strMessage);
	if (hr != hrSuccess) {
		// unable to load streamed version, so try full conversion.
		hr = IMToINet(lpSession, lpAddrBook, lpMessage, &szMessage, sopt, lpLogger);
		if (hr != hrSuccess) {
			lpLogger->Log(EC_LOGLEVEL_ERROR, "Error converting MAPI to MIME: 0x%08x", hr);
			HrResponse(POP3_RESP_ERR, "Converting MAPI to MIME error");
			goto exit;
		}

		strMessage = szMessage;
	}

	ulPos = strMessage.find("\r\n\r\n", 0);

	ulLines++;
	while (ulPos != string::npos && ulLines--)
		ulPos = strMessage.find("\r\n", ulPos + 1);

	if (ulPos != string::npos)
		strMessage = strMessage.substr(0, ulPos);

	strMessage = DotFilter(strMessage.c_str());

	if (HrResponse(POP3_RESP_OK, string()) != hrSuccess ||
		lpChannel->HrWriteLine(strMessage) != hrSuccess ||
		lpChannel->HrWriteLine(".") != hrSuccess)
	{
		hr = MAPI_E_CALL_FAILED;
		goto exit;
	}

exit:
	if (lpStream)
		lpStream->Release();

	if (lpMessage)
		lpMessage->Release();

	if (szMessage)
		delete [] szMessage;

	return hr;
}

/** 
 * Open the Inbox with the given login credentials
 * 
 * @param[in] strUsername Username to login with
 * @param[in] strPassword Corresponding password of username
 * 
 * @return MAPI Error code
 */
HRESULT POP3::HrLogin(const std::string &strUsername, const std::string &strPassword) {
	HRESULT hr = hrSuccess;
	ULONG cbEntryID = 0;
	LPENTRYID lpEntryID = NULL;
	ULONG ulObjType = 0;
	wstring strwUsername;
	wstring strwPassword;

	hr = TryConvert(strUsername, rawsize(strUsername), "windows-1252", strwUsername);
	if (hr != hrSuccess) {
		lpLogger->Log(EC_LOGLEVEL_ERROR, "Illegal byte sequence in username");
		goto exit;
	}
	hr = TryConvert(strPassword, rawsize(strPassword), "windows-1252", strwPassword);
	if (hr != hrSuccess) {
		lpLogger->Log(EC_LOGLEVEL_ERROR, "Illegal byte sequence in password");
		goto exit;
	}
	
	hr = HrOpenECSession(&lpSession, strwUsername.c_str(), strwPassword.c_str(), m_strPath.c_str(),
						 EC_PROFILE_FLAGS_NO_NOTIFICATIONS, NULL, NULL);
	if (hr != hrSuccess) {
		lpLogger->Log(EC_LOGLEVEL_ERROR, "Failed to login from %s with invalid username \"%s\" or wrong password. Error: 0x%X",
					  lpChannel->GetIPAddress().c_str(), strUsername.c_str(), hr);
		goto exit;
	}

	hr = HrOpenDefaultStore(lpSession, &lpStore);
	if (hr != hrSuccess) {
		lpLogger->Log(EC_LOGLEVEL_ERROR, "Failed to open default store");
		goto exit;
	}

	hr = lpSession->OpenAddressBook(0, NULL, AB_NO_DIALOG, &lpAddrBook);
	if (hr != hrSuccess) {
		lpLogger->Log(EC_LOGLEVEL_ERROR, "Failed to open addressbook");
		goto exit;
	}

	// check if pop3 access is disabled
	if (isFeatureDisabled("pop3", lpAddrBook, lpStore)) {
		lpLogger->Log(EC_LOGLEVEL_ERROR, "POP3 not enabled for user '%s'", strUsername.c_str());
		hr = MAPI_E_LOGON_FAILED;
		goto exit;
	}

	hr = lpStore->GetReceiveFolder((LPTSTR)"IPM", 0, &cbEntryID, &lpEntryID, NULL);
	if (hr != hrSuccess) {
		lpLogger->Log(EC_LOGLEVEL_ERROR, "Failed to find receive folder of store");
		goto exit;
	}

	hr = lpStore->OpenEntry(cbEntryID, lpEntryID, &IID_IMAPIFolder, MAPI_MODIFY, &ulObjType, (LPUNKNOWN *) &lpInbox);
	if (ulObjType != MAPI_FOLDER)
		hr = MAPI_E_NOT_FOUND;

	if (hr != hrSuccess) {
		lpLogger->Log(EC_LOGLEVEL_ERROR, "Failed to open receive folder");
		goto exit;
	}

	lpLogger->Log(EC_LOGLEVEL_ERROR, "POP3 Login from %s for user %s", lpChannel->GetIPAddress().c_str(), strUsername.c_str());

exit:
	if (lpEntryID)
		MAPIFreeBuffer(lpEntryID);

	if (hr != hrSuccess) {
		if (lpInbox) {
			lpInbox->Release();
			lpInbox = NULL;
		}
		if (lpStore) {
			lpStore->Release();
			lpStore = NULL;
		}
	}

	return hr;
}

/** 
 * Make a list of all emails in the Opened inbox
 * 
 * @return MAPI Error code
 */
HRESULT POP3::HrMakeMailList() {
	HRESULT hr = hrSuccess;
	LPMAPITABLE lpTable = NULL;
	LPSRowSet lpRows = NULL;
	MailListItem sMailListItem;
	enum { EID, SIZE, NUM_COLS };
	SizedSPropTagArray(NUM_COLS, spt) = { NUM_COLS, {PR_ENTRYID, PR_MESSAGE_SIZE} };
	const static SizedSSortOrderSet(1, tableSort) =
		{ 1, 0, 0,
		  {
			  { PR_CREATION_TIME, TABLE_SORT_ASCEND }
		  }
		};

	hr = lpInbox->GetContentsTable(0, &lpTable);
	if (hr != hrSuccess)
		goto exit;

	hr = lpTable->SetColumns((LPSPropTagArray) &spt, 0);
	if (hr != hrSuccess)
		goto exit;

	hr = lpTable->SortTable((LPSSortOrderSet)&tableSort, 0);
	if (hr != hrSuccess) {
		goto exit;
	}

	hr = lpTable->QueryRows(-1, 0, &lpRows);
	if (hr != hrSuccess)
		goto exit;

	lstMails.clear();
	for (ULONG i = 0; i < lpRows->cRows; i++) {
		if (PROP_TYPE(lpRows->aRow[i].lpProps[EID].ulPropTag) == PT_ERROR) {
			lpLogger->Log(EC_LOGLEVEL_ERROR, "Missing EntryID in message table for message %d", i);
			continue;
		}

		if (PROP_TYPE(lpRows->aRow[i].lpProps[SIZE].ulPropTag) == PT_ERROR) {
			lpLogger->Log(EC_LOGLEVEL_ERROR, "Missing size in message table for message %d", i);
			continue;
		}

		sMailListItem.sbEntryID.cb = lpRows->aRow[i].lpProps[EID].Value.bin.cb;
		sMailListItem.sbEntryID.lpb = new BYTE[lpRows->aRow[i].lpProps[EID].Value.bin.cb];
		memcpy(sMailListItem.sbEntryID.lpb, lpRows->aRow[i].lpProps[EID].Value.bin.lpb, lpRows->aRow[i].lpProps[EID].Value.bin.cb);
		sMailListItem.bDeleted = false;
		sMailListItem.ulSize = lpRows->aRow[i].lpProps[SIZE].Value.l;
		lstMails.push_back(sMailListItem);
	}

exit:
	if (lpRows)
		FreeProws(lpRows);

	if (lpTable)
		lpTable->Release();

	return hr;
}

/** 
 * Since a POP3 email stops with one '.' line, we need to escape these lines in the actual email.
 * 
 * @param[in] input input email to escape
 * 
 * @return POP3 escaped email
 */
string POP3::DotFilter(const char *input) {
	string output;
	ULONG i = 0;

	while (input[i] != '\0') {
		if (input[i] == '.' && input[i-1] == '\n')
			output += '.';
		output += input[i++];
	}
	return output;
}

/** @} */
