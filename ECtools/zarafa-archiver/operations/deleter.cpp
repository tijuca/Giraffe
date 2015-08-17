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

#include <platform.h>
#include "ECArchiverLogger.h"
#include "deleter.h"

using namespace std;

namespace za { namespace operations {

/**
 * Constructor
 *
 * @param[in]	lpLogger
 *					Pointer to the logger.
 *
 * @return HRESULT
 */
Deleter::Deleter(ECArchiverLogger *lpLogger, int ulAge, bool bProcessUnread)
: ArchiveOperationBaseEx(lpLogger, ulAge, bProcessUnread, ARCH_NEVER_DELETE)
{ }

/**
 * Destructor
 */
Deleter::~Deleter()
{
	PurgeQueuedMessages();
}

HRESULT Deleter::EnterFolder(LPMAPIFOLDER)
{
	return hrSuccess;
}

HRESULT Deleter::LeaveFolder()
{
	// Folder is still available, purge now!
	return PurgeQueuedMessages();
}

HRESULT Deleter::DoProcessEntry(ULONG cProps, const LPSPropValue &lpProps)
{
	HRESULT hr = hrSuccess;
	LPSPropValue lpEntryId = NULL;

	lpEntryId = PpropFindProp(lpProps, cProps, PR_ENTRYID);
	if (lpEntryId == NULL) {
		Logger()->Log(EC_LOGLEVEL_FATAL, "PR_ENTRYID missing");
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}
	
	if (m_lstEntryIds.size() >= 50) {
		hr = PurgeQueuedMessages();
		if (hr != hrSuccess)
			goto exit;
	}

	m_lstEntryIds.push_back(lpEntryId->Value.bin);
	
exit:
	return hr;
}

/**
 * Delete the messages that are queued for deletion.
 * @return HRESULT
 */
HRESULT Deleter::PurgeQueuedMessages()
{
	HRESULT hr = hrSuccess;
	EntryListPtr ptrEntryList;
	list<entryid_t>::const_iterator iEntryId;
	ULONG ulIdx = 0;
	
	if (m_lstEntryIds.empty())
		goto exit;
	
	hr = MAPIAllocateBuffer(sizeof(ENTRYLIST), &ptrEntryList);
	if (hr != hrSuccess)
		goto exit;
	
	hr = MAPIAllocateMore(m_lstEntryIds.size() * sizeof(SBinary), ptrEntryList, (LPVOID*)&ptrEntryList->lpbin);
	if (hr != hrSuccess)
		goto exit;
		
	ptrEntryList->cValues = m_lstEntryIds.size();
	for (iEntryId = m_lstEntryIds.begin(); iEntryId != m_lstEntryIds.end(); ++iEntryId, ++ulIdx) {
		ptrEntryList->lpbin[ulIdx].cb = iEntryId->size();
		ptrEntryList->lpbin[ulIdx].lpb = *iEntryId;
	}
	
	hr = CurrentFolder()->DeleteMessages(ptrEntryList, 0, NULL, 0);
	if (hr != hrSuccess) {
		Logger()->Log(EC_LOGLEVEL_FATAL, "Failed to delete %u messages. (hr=%s)", ptrEntryList->cValues, stringify(hr, true).c_str());
		goto exit;
	}
	
	m_lstEntryIds.clear();
	
exit:
	return hr;
}

}} // namespaces 
