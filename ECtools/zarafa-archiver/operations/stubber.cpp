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
#include "ECLogger.h"
#include "ECConfig.h"
#include "stubber.h"
#include "archiver-common.h"
#include "helpers/mapiprophelper.h"
#include <mapiext.h>
using namespace za::helpers;

namespace za { namespace operations {

/**
 * Constructor
 * 
 * @param[in]	lpLogger
 *					Pointer to the logger.
 * @param[in]	ulptStubbed
 *					The proptag of the stubbed property {72e98ebc-57d2-4ab5-b0aad50a7b531cb9}/stubbed
 *
 * @return HRESULT
 */
Stubber::Stubber(ECLogger *lpLogger, ULONG ulptStubbed, int ulAge, bool bProcessUnread)
: ArchiveOperationBase(lpLogger, ulAge, bProcessUnread, ARCH_NEVER_STUB)
, m_ulptStubbed(ulptStubbed)
{ }

HRESULT Stubber::ProcessEntry(LPMAPIFOLDER lpFolder, ULONG cProps, const LPSPropValue lpProps)
{
	HRESULT hr = hrSuccess;
	LPSPropValue lpEntryId = NULL;
	MessagePtr ptrMessage;
	ULONG ulType = 0;

	ASSERT(lpFolder != NULL);
	if (lpFolder == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}
	
	lpEntryId = PpropFindProp(lpProps, cProps, PR_ENTRYID);
	if (lpEntryId == NULL) {
		Logger()->Log(EC_LOGLEVEL_FATAL, "PR_ENTRYID missing");
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}

	Logger()->Log(EC_LOGLEVEL_DEBUG, "Opening message (%s)", bin2hex(lpEntryId->Value.bin.cb, lpEntryId->Value.bin.lpb).c_str());
	hr = lpFolder->OpenEntry(lpEntryId->Value.bin.cb, (LPENTRYID)lpEntryId->Value.bin.lpb, &IID_IECMessageRaw, MAPI_BEST_ACCESS, &ulType, &ptrMessage);
	if (hr == MAPI_E_NOT_FOUND) {
		Logger()->Log(EC_LOGLEVEL_WARNING, "Failed to open message. This can happen if the search folder is lagging.");
		hr = hrSuccess;
		goto exit;
	} else if (hr != hrSuccess) {
		Logger()->Log(EC_LOGLEVEL_FATAL, "Failed to open message. (hr=%s)", stringify(hr, true).c_str());
		goto exit;
	}

	hr = ProcessEntry(ptrMessage);

exit:
	return hr;
}

HRESULT Stubber::ProcessEntry(LPMESSAGE lpMessage)
{
	HRESULT hr = hrSuccess;
	SPropValue sProps[3];
	SPropValue sProp = {0};
	MAPITablePtr ptrAttTable;
	SRowSetPtr ptrRowSet;
	AttachPtr ptrAttach;
	ULONG ulAttachNum = 0;
	MAPIPropHelperPtr ptrMsgHelper;
	ObjectEntryList lstMsgArchives;

	SizedSPropTagArray(1, sptaTableProps) = {1, {PR_ATTACH_NUM}};

	ASSERT(lpMessage != NULL);
	if (lpMessage == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	hr = VerifyRestriction(lpMessage);
	if (hr == MAPI_E_NOT_FOUND) {
		Logger()->Log(EC_LOGLEVEL_WARNING, "Ignoring message because it doesn't match the criteria for begin stubbed.");
		Logger()->Log(EC_LOGLEVEL_WARNING, "This can happen when huge amounts of message are being processed.");

		// This is not an error
		hr = hrSuccess;
		goto exit;
	} else if (hr != hrSuccess) {
		Logger()->Log(EC_LOGLEVEL_WARNING, "Failed to verify message criteria. (hr=%s)", stringify(hr, true).c_str());
		goto exit;
	}


	// Verify if we have at least one archive that's in the current multi-server cluster.
	hr = MAPIPropHelper::Create(MAPIPropPtr(lpMessage, true), &ptrMsgHelper);
	if (hr != hrSuccess) {
		Logger()->Log(EC_LOGLEVEL_FATAL, "Failed to create prop helper. (hr=%s)", stringify(hr, true).c_str());
		goto exit;
	}
	
	hr = ptrMsgHelper->GetArchiveList(&lstMsgArchives);
	if (hr != hrSuccess) {
		if (hr == MAPI_E_CORRUPT_DATA) {
			Logger()->Log(EC_LOGLEVEL_ERROR, "Existing list of archives is corrupt, skipping message.");
			hr = hrSuccess;
		} else
			Logger()->Log(EC_LOGLEVEL_FATAL, "Failed to get list of archives. (hr=%s)", stringify(hr, true).c_str());
		goto exit;
	}

	if (find_if(lstMsgArchives.begin(), lstMsgArchives.end(), IsNotWrapped()) == lstMsgArchives.end()) {
		Logger()->Log(EC_LOGLEVEL_WARNING, "Message has no archives that are directly accessible, message will not be stubbed.");
		goto exit;
	}
	
	
		
	sProps[0].ulPropTag = m_ulptStubbed;
	sProps[0].Value.b = 1;
	
	sProps[1].ulPropTag = PR_BODY;
	sProps[1].Value.LPSZ = _T("This message is archived...");

	sProps[2].ulPropTag = PR_ICON_INDEX;
	sProps[2].Value.l = 2;

	hr = lpMessage->SetProps(3, sProps, NULL);
	if (hr != hrSuccess) {
		Logger()->Log(EC_LOGLEVEL_FATAL, "Failed to set properties. (hr=%s)", stringify(hr, true).c_str());
		goto exit;
	}

	hr = lpMessage->GetAttachmentTable(fMapiDeferredErrors, &ptrAttTable);
	if (hr != hrSuccess) {
		Logger()->Log(EC_LOGLEVEL_FATAL, "Failed to get attachment table. (hr=%s)", stringify(hr, true).c_str());
		goto exit;
	}
	
	hr = HrQueryAllRows(ptrAttTable, (LPSPropTagArray)&sptaTableProps, NULL, NULL, 0, &ptrRowSet);
	if (hr != hrSuccess) {
		Logger()->Log(EC_LOGLEVEL_FATAL, "Failed to get attachment numbers. (hr=%s)", stringify(hr, true).c_str());
		goto exit;
	}
	
	if (!ptrRowSet.empty()) {
		Logger()->Log(EC_LOGLEVEL_INFO, "Removing %u attachments", ptrRowSet.size());
		for (ULONG i = 0; i < ptrRowSet.size(); ++i) {
			hr = lpMessage->DeleteAttach(ptrRowSet[i].lpProps[0].Value.ul, 0, NULL, 0);
			if (hr != hrSuccess) {
				Logger()->Log(EC_LOGLEVEL_FATAL, "Failed to delete attachment %u. (hr=%s)", i, stringify(hr, true).c_str());
				goto exit;
			}
		}
		
		Logger()->Log(EC_LOGLEVEL_INFO, "Adding placeholder attachment");		
		hr = lpMessage->CreateAttach(&ptrAttach.iid, 0, &ulAttachNum, &ptrAttach);
		if (hr != hrSuccess) {
			Logger()->Log(EC_LOGLEVEL_FATAL, "Failed to create attachment. (hr=%s)", stringify(hr, true).c_str());
			goto exit;
		}
		
		sProp.ulPropTag = PR_ATTACH_FILENAME;
		sProp.Value.LPSZ = _T("dummy");
		hr = ptrAttach->SetProps(1, &sProp, NULL);
		if (hr != hrSuccess) {
			Logger()->Log(EC_LOGLEVEL_FATAL, "Failed to set attachment properties. (hr=%s)", stringify(hr, true).c_str());
			goto exit;
		}
		
		hr = ptrAttach->SaveChanges(0);
		if (hr != hrSuccess) {
			Logger()->Log(EC_LOGLEVEL_FATAL, "Failed to save attachment. (hr=%s)", stringify(hr, true).c_str());
			goto exit;
		}
	}
	
	hr = lpMessage->SaveChanges(0);
	if (hr != hrSuccess) {
		Logger()->Log(EC_LOGLEVEL_FATAL, "Failed to save stubbed message. (hr=%s)", stringify(hr, true).c_str());
		goto exit;
	}
	
exit:
	return hr;

}

}} // namespaces 
