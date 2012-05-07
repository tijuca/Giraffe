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

#ifndef ECSEARCHER_H
#define ECSEARCHER_H

#include <string>

#include <ECUnknown.h>

#include "zarafa-indexer.h"

class ECIndexer;

/**
 * Main searcher handler
 *
 * This class manages all aspects of the searching through the index.
 * It runs in its own private thread and will listen for incoming connections
 * from remote clients for search queries.
 */
class ECSearcher : public ECUnknown {
private:
	/**
	 * Constructor
	 *
	 * @note Objects of ECSearcher must only be created using the Create() function.
	 *
	 * @param[in]	lpThreadData
	 * @param[in]	ulSocket
	 * @param[in]	bUseSsl
	 */
	ECSearcher(ECThreadData *lpThreadData, ECIndexer *lpIndexer, int ulSocket, bool bUseSsl);

public:
	/**
	 * Create new ECSearcher object.
	 *
	 * @note Creating a new ECSearcher object must always occur through this function.
	 *
	 * @param[in]	lpThreadData
	 *					 Reference to the ECThreadData object.
	 * @param[in]	ulSocket
	 *					 Listening socket for new connections.
	 * @param[in]	bUseSsl
	 *					 If the given ulSocket expects SSL data or not.
	 * @param[out]	lppSearcher
	 *					The created ECSearcher object.
	 * @return HRESULT
	 */
	static HRESULT Create(ECThreadData *lpThreadData, ECIndexer *lpIndexer, int ulSocket, bool bUseSsl, ECSearcher **lppSearcher);

	/**
	 * Destructor
	 */
	~ECSearcher();

private:
	/**
	 * Main thread handler
	 *
	 * This function will be run constantly listening for incoming search requests.
	 *
	 * @param[in]   lpVoid
	 *					 Reference to ECIndexer object
	 * @return LPVOID
	 */
	static LPVOID RunThread(LPVOID lpVoid);

	/**
	 * Main function which will listen for incoming connections
	 *
	 * When an incoming connection has been accepted a new thread
	 * will be started with ECSearcherRequestwhich to handle
	 * the incoming request.
	 *
	 * @return HRESULT
	 */
	HRESULT Listen();

private:
	ECThreadData *m_lpThreadData;
	ECIndexer *m_lpIndexer;
	int m_ulSocket;
	bool m_bUseSsl;

	/* Thread attribute */
	pthread_attr_t m_hThreadAttr;
};

#endif /* ECSEARCHER_H */
