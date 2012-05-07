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

#include <mapi.h>
#include <mapix.h>
#include <mapiutil.h>
#include <mapidefs.h>
#include <mapiguid.h>

#include <CommonUtil.h>
#include <ECChannel.h>
#include <stringutil.h>
#include <Util.h>

#include "ECFileIndex.h"
#include "ECLucene.h"
#include "ECLuceneSearcher.h"
#include "ECSearcher.h"
#include "ECSearcherRequest.h"
#include "ECIndexer.h"
#include "zarafa-indexer.h"

ECSearcherRequest::ECSearcherRequest(ECThreadData *lpThreadData, ECIndexer *lpIndexer, ECChannel *lpChannel, BOOL bUseSSL)
: m_lpThreadData(lpThreadData)
, m_lpIndexer(lpIndexer)
, m_lpChannel(lpChannel)
, m_bUseSSL(bUseSSL)
, m_lpSearcher(NULL)
{
	pthread_attr_init(&m_hThreadAttr);
	pthread_attr_setdetachstate(&m_hThreadAttr, PTHREAD_CREATE_DETACHED);
}

ECSearcherRequest::~ECSearcherRequest()
{
	if (m_lpSearcher)
		m_lpSearcher->Release();

	if (m_lpChannel)
		delete m_lpChannel;

	pthread_attr_destroy(&m_hThreadAttr);
}

HRESULT ECSearcherRequest::Create(ECThreadData *lpThreadData, ECIndexer *lpIndexer, ECChannel *lpChannel, BOOL bUseSSL, ECSearcherRequest **lppRequest)
{
	HRESULT hr = hrSuccess;
	ECSearcherRequest *lpRequest = NULL;
	pthread_t thread;

	try {
		lpRequest = new ECSearcherRequest(lpThreadData, lpIndexer, lpChannel, bUseSSL);
	}
	catch (...) {
		lpRequest = NULL;
	}
	if (!lpRequest) {
		hr = MAPI_E_NOT_ENOUGH_MEMORY;
		goto exit;
	}

	lpRequest->AddRef();

	if (pthread_create(&thread, &lpRequest->m_hThreadAttr, ECSearcherRequest::RunThread, lpRequest) != 0) {
		hr = MAPI_E_CALL_FAILED;
		goto exit;
	}

	if (lppRequest)
		*lppRequest = lpRequest;

exit:
	if (hr != hrSuccess && lpRequest)
		lpRequest->Release();

	return hr;
}

LPVOID ECSearcherRequest::RunThread(LPVOID lpVoid)
{
	HRESULT hr = hrSuccess;
	ECSearcherRequest *lpRequest = (ECSearcherRequest *)lpVoid;

	lpRequest->m_lpThreadData->lpLogger->Log(EC_LOGLEVEL_ERROR, "Search thread started");

	hr = lpRequest->ProcessRequest();
	if (hr != hrSuccess)
		goto exit;

exit:
	if (lpRequest)
		lpRequest->Release();

	return NULL;
}

HRESULT ECSearcherRequest::ProcessRequest()
{
	HRESULT hr = hrSuccess;

	if (m_bUseSSL) {
		if (m_lpChannel->HrEnableTLS() != hrSuccess) {
			m_lpThreadData->lpLogger->Log(EC_LOGLEVEL_ERROR, "Unable to negotiate SSL connection");
		}
	}

	while (!m_lpThreadData->bShutdown) {
		std::string strLine;
		command_t ulCommand;
		std::vector<std::string> listArgs;
		std::string strResponse;

		hr = m_lpChannel->HrSelect(60);
		if (hr == MAPI_E_TIMEOUT) {
			m_lpThreadData->lpLogger->Log(EC_LOGLEVEL_INFO, "Connection closed because of timeout");
			goto exit;
		} else if (hr != hrSuccess) {
			m_lpThreadData->lpLogger->Log(EC_LOGLEVEL_ERROR, "Socket error: %s", strerror(errno));
			goto exit;
		}

		errno = 0;
		hr = m_lpChannel->HrReadLine(&strLine);
		if (hr != hrSuccess) {
			if (errno)
				m_lpThreadData->lpLogger->Log(EC_LOGLEVEL_ERROR, "Search thread read error 0x%08X, %s", hr, strerror(errno));
			else {
				m_lpThreadData->lpLogger->Log(EC_LOGLEVEL_INFO, "Search thread client disconnecting");
				hr = hrSuccess;
			}
			goto exit;
		}

		m_lpThreadData->lpLogger->Log(EC_LOGLEVEL_DEBUG, "Command: %s", strLine.c_str());

		hr = parseRequest(strLine, &ulCommand, &listArgs);
		if (hr != hrSuccess) {
			switch (hr) {
			case MAPI_E_STRING_TOO_LONG:
				strResponse = stringify(MAPI_E_STRING_TOO_LONG, true) + ": Too many arguments provided";
				break;
			case MAPI_W_ERRORS_RETURNED:
				strResponse = stringify(MAPI_W_ERRORS_RETURNED, true) + ": Could not parse arguments";
				break;
			case MAPI_E_NO_SUPPORT:
				strResponse = stringify(MAPI_E_NO_SUPPORT, true) + ": Command not supported";
				break;
			default:
				strResponse = stringify(hr, true) + ": Failed to parse request";
				break;
			}
		} else {
			hr = handleRequest(ulCommand, listArgs, &strResponse);
			if (hr != hrSuccess) {
				strResponse = stringify(hr, true) + ": Failed to handle request";

				m_lpThreadData->lpLogger->Log(EC_LOGLEVEL_ERROR, "Unable to handle request, error 0x%08X", hr);
				hr = hrSuccess;
			}
		}

		m_lpThreadData->lpLogger->Log(EC_LOGLEVEL_DEBUG, "Response: %s", strResponse.c_str());

		hr = m_lpChannel->HrWriteLine(strResponse);
		if (hr != hrSuccess) {
			m_lpThreadData->lpLogger->Log(EC_LOGLEVEL_DEBUG, "Search thread write error 0x%08X", hr);
			goto exit;
		}
	}

exit:
	if (hr != hrSuccess)
		m_lpThreadData->lpLogger->Log(EC_LOGLEVEL_WARNING, "Search thread done, exit code 0x%08X", hr);
	else
		m_lpThreadData->lpLogger->Log(EC_LOGLEVEL_WARNING, "Search thread done");
	return hr;
}

HRESULT ECSearcherRequest::parseRequest(std::string &strRequest, command_t *lpulCommand, std::vector<std::string> *lplistArgs)
{
	HRESULT hr = hrSuccess;
	std::string strCommand;
	std::string strArgs;
	size_t pos_space;
	size_t pos_nospace;

	if (!lpulCommand || !lplistArgs) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	strRequest = trim(strRequest);
	if (strRequest.empty()) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	pos_space = strRequest.find_first_of(' ');
	pos_nospace = strRequest.find_first_not_of(' ', pos_space);

	if (pos_space != std::string::npos && pos_nospace != std::string::npos) {
		strCommand.assign(strRequest, 0, pos_space);
		strArgs.assign(strRequest, pos_nospace, std::string::npos);
	} else {
		strCommand.assign(strRequest);
		strArgs.clear();
	}

	if (stricmp(strCommand.c_str(), "PROPS") == 0) {
		*lpulCommand = COMMAND_PROPS;

		if (!strArgs.empty()) {
			hr = MAPI_E_STRING_TOO_LONG;
			goto exit;
		}
	} else if (stricmp(strCommand.c_str(), "SCOPE") == 0) {
		*lpulCommand = COMMAND_SCOPE;

		if (strArgs.empty()) {
			hr = MAPI_W_ERRORS_RETURNED;
			goto exit;
		}

		*lplistArgs = tokenize(strArgs, " ");

		if (lplistArgs->size() < 2) {
			hr = MAPI_W_ERRORS_RETURNED;
			goto exit;
		}
	} else if (stricmp(strCommand.c_str(), "QUERY") == 0) {
		*lpulCommand = COMMAND_QUERY;

		if (strArgs.empty()) {
			hr = MAPI_W_ERRORS_RETURNED;
			goto exit;
		}

		lplistArgs->assign(1, strArgs);
	} else if (stricmp(strCommand.c_str(), "SYNCRUN") == 0) {
		*lpulCommand = COMMAND_SYNCRUN;

		if (!strArgs.empty()) {
			hr = MAPI_E_STRING_TOO_LONG;
			goto exit;
		}
	} else {
		hr = MAPI_E_NO_SUPPORT;
		goto exit;
	}

exit:
	return hr;
}

HRESULT ECSearcherRequest::handleRequest(command_t ulCommand, std::vector<std::string> &listArgs, std::string *lpstrResponse)
{
	HRESULT hr = hrSuccess;
	std::string strStorePath;
	string_list_t lResults;

	switch (ulCommand) {
	case COMMAND_PROPS:
		hr = m_lpThreadData->lpLucene->GetIndexedProps(&lResults);
		if (hr != hrSuccess)
			goto exit;

		*lpstrResponse = "OK:";
		*lpstrResponse += concatenate(lResults, "; ");
		break;
	case COMMAND_SCOPE:
		hr = m_lpThreadData->lpFileIndex->GetStorePath(listArgs[0], listArgs[1], &strStorePath);
		if (hr != hrSuccess) {
			*lpstrResponse = stringify(MAPI_E_INVALID_ENTRYID, true) + ": Invalid entryid provided";
			hr = hrSuccess;
			goto exit;
		}

		listArgs.erase(listArgs.begin(), listArgs.begin() + 2);

		if (m_lpSearcher) {
			m_lpSearcher->Release();
			m_lpSearcher = NULL;
		}

		hr = m_lpThreadData->lpLucene->CreateSearcher(m_lpThreadData, strStorePath, listArgs, &m_lpSearcher);
		if (hr != hrSuccess) {
			if (hr == MAPI_E_NOT_FOUND)
				*lpstrResponse = stringify(MAPI_E_NOT_FOUND, true) + ": Store has not yet been indexed";
			else
				*lpstrResponse = stringify(MAPI_E_CALL_FAILED, true) + ": Failed to create searcher";

			m_lpSearcher = NULL;
			hr = hrSuccess;
			goto exit;
		}

		*lpstrResponse = "OK:";
		break;
	case COMMAND_QUERY:
		if (!m_lpSearcher) {
			*lpstrResponse = stringify(MAPI_E_NOT_INITIALIZED, true) + ": No scope provided";
			goto exit;
		}

		hr = m_lpSearcher->SearchEntries(listArgs.front(), &lResults);
		if (hr != hrSuccess) {
			*lpstrResponse = stringify(MAPI_E_CALL_FAILED, true) + ": Search failed";
			hr = hrSuccess;
			goto exit;
		}

		*lpstrResponse = "OK:";
		*lpstrResponse += concatenate(lResults, "; ");
		break;
	case COMMAND_SYNCRUN:
		hr = m_lpIndexer->RunSynchronization();
		if (hr != hrSuccess) {
			*lpstrResponse = stringify(hr, true) + ": Command failed";
			hr = hrSuccess;
			goto exit;
		}

		*lpstrResponse = "OK:";
		break;
	}

exit:
	return hr;
}
