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

#ifndef ECSEARCHERREQUEST_H
#define ECSEARCHERREQUEST_H

#include <ECUnknown.h>

#include "zarafa-indexer.h"

class ECLuceneSearcher;
class ECIndexer;

/**
 * Supported commands from client
 */
enum command_t {
	COMMAND_PROPS,
	COMMAND_SCOPE,
	COMMAND_QUERY,
	COMMAND_SYNCRUN
};

class ECSearcherRequest : public ECUnknown {
private:
	/**
	 * Constructor
	 *
	 * @note Objects of ECSearcherRequest must only be created using the Create() function.
	 *
	 * @param[in]	lpThreadData
	 * @param[in]	lpChannel
	 * @param[in]	bUseSSL
	 */
	ECSearcherRequest(ECThreadData *lpThreadData, ECIndexer *lpIndexer, ECChannel *lpChannel, BOOL bUseSSL);

public:
	/**
	 * Create new ECSearcherRequest object.
	 *
	 *  @note Creating a new ECSearcherRequest object must always occur through this function.
	 *
	 * @param[in]	lpThreadData
	 *					Reference to the ECThreadData object.
	 * @param[in]	lpChannel
	 *					Reference to ECChannel object which is used for the incoming search request.
	 * @param[in]	bUseSSL
	 *					Use SSL for communication
	 * @param[out]	lppRequest
	 *					The created ECSearcherRequest object.
	 * @return HRESULT
	 */
	static HRESULT Create(ECThreadData *lpThreadData, ECIndexer *lpIndexer, ECChannel *lpChannel, BOOL bUseSSL, ECSearcherRequest **lppRequest);

	/**
	 * Destructor
	 */
	~ECSearcherRequest();

private:
	/**
	 * Main thread handler
	 *
	 * This function will run to start processing an incoming search request.
	 *
	 * @param[in]   lpVoid
	 *					Reference to ECIndexer object
	 * @return LPVOID
	 */
	static LPVOID RunThread(LPVOID lpVoid);

	/**
	 * Handle incoming client requests
	 *
	 * @return HRESULT
	 */
	HRESULT ProcessRequest();

	/**
	 * Parse request and validate arguments
	 *
	 * @param[in]	strRequest
	 *					Incoming client command which should be parsed.
	 * @param[out]	lpulCommand
	 *					Identified command (see command_t).
	 * @param[out]	lplistArgs
	 *					List of arguments which were passed with the command.
	 * @return HRESULT
	 */
	HRESULT parseRequest(std::string &strRequest, command_t *lpulCommand, std::vector<std::string> *lplistArgs);

	/**
	 * Handle incoming command
	 *
	 * This will handle the incoming command and returns a return string which can either be an error description
	 * or the requested data.
	 *
	 * @param[in]	ulCommand
	 *					The command as it was detected by parseRequest().
	 * @param[in]	listArgs
	 *					List of arguments which were passed with the command.
	 * @param[out]	lpstrResponse
	 *					Response string which should be send back to client.
	 * @return HRESULT
	 */
	HRESULT handleRequest(command_t ulCommand, std::vector<std::string> &listArgs, std::string *lpstrResponse);

private:
	ECThreadData *m_lpThreadData;
	ECIndexer *m_lpIndexer;

	/* Connection information */
	ECChannel *m_lpChannel;
	BOOL m_bUseSSL;

	/* Thread attribute */
	pthread_attr_t m_hThreadAttr;

	/* Scope information */
	ECLuceneSearcher *m_lpSearcher;
};

#endif /* ECSEARCHERREQUEST_H */
