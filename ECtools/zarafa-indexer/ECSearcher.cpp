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

#include <algorithm>

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
#include "zarafa-indexer.h"

ECSearcher::ECSearcher(ECThreadData *lpThreadData, ECIndexer *lpIndexer, int ulSocket, bool bUseSsl)
: m_lpThreadData(lpThreadData)
, m_lpIndexer(lpIndexer)
, m_ulSocket(ulSocket)
, m_bUseSsl(bUseSsl)
{
	pthread_attr_init(&m_hThreadAttr);
	pthread_attr_setdetachstate(&m_hThreadAttr, PTHREAD_CREATE_DETACHED);
}

ECSearcher::~ECSearcher()
{
	pthread_attr_destroy(&m_hThreadAttr);
}

HRESULT ECSearcher::Create(ECThreadData *lpThreadData, ECIndexer *lpIndexer, int ulSocket, bool bUseSsl, ECSearcher **lppSearcher)
{
	HRESULT hr = hrSuccess;
	ECSearcher *lpSearcher = NULL;
	pthread_t thread;

	try {
		lpSearcher = new ECSearcher(lpThreadData, lpIndexer, ulSocket, bUseSsl);
	}
	catch (...) {
		lpSearcher = NULL;
	}
	if (!lpSearcher) {
		hr = MAPI_E_NOT_ENOUGH_MEMORY;
		goto exit;
	}

	lpSearcher->AddRef();

	if (pthread_create(&thread, &lpSearcher->m_hThreadAttr, ECSearcher::RunThread, lpSearcher) != 0) {
		hr = MAPI_E_CALL_FAILED;
		goto exit;
	}

	if (lppSearcher)
		*lppSearcher = lpSearcher;

exit:
	if (hr != hrSuccess && lpSearcher)
		lpSearcher->Release();

	return hr;
}

LPVOID ECSearcher::RunThread(LPVOID lpVoid)
{
	HRESULT hr = hrSuccess;
	ECSearcher *lpSearcher = (ECSearcher *)lpVoid;

	if (lpSearcher->m_lpThreadData->bShutdown)
		return NULL;

	hr = lpSearcher->Listen();
	if (hr != hrSuccess)
		goto exit;

exit:
	return NULL;
}

HRESULT ECSearcher::Listen()
{
	HRESULT hr = hrSuccess;
	int ulAcceptFails = 0;

	while (!m_lpThreadData->bShutdown) {
		ECSearcherRequest *lpRequest = NULL;
		ECChannel *lpChannel = NULL;

		hr = HrAccept(m_lpThreadData->lpLogger, m_ulSocket, &lpChannel);
		if (hr != hrSuccess) {
			// HrAccept already logged
			hr = hrSuccess;
			ulAcceptFails++;
			if (ulAcceptFails > 10) {
				m_lpThreadData->lpLogger->Log(EC_LOGLEVEL_FATAL, "Failed to accept last 10 incoming search request, bailing out");
				goto exit;
			}
			// wait for the next accept not to flood the indexer server with errors
			Sleep(1000);
			continue;
		}
		ulAcceptFails = 0;

		hr = ECSearcherRequest::Create(m_lpThreadData, m_lpIndexer, lpChannel, m_bUseSsl, &lpRequest);
		if (hr != hrSuccess) {
			m_lpThreadData->lpLogger->Log(EC_LOGLEVEL_FATAL, "Failed to start processing search request");
			hr = hrSuccess;
			delete lpChannel;
		}

		/* ECSearcherRequest will clean itself up when the request has been handled */
	}

exit:
	// close socket
	close(m_ulSocket);
	// remove ssl context
	ECChannel::HrFreeCtx();

	return hr;
}
