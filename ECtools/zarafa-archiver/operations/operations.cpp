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
#include "ECConfig.h"
#include "operations.h"
#include "logger.h"
#include "helpers/mapiprophelper.h"
#include "helpers/archivehelper.h"
#include "archiver-session.h"
#include "ECRestriction.h"

#include <mapiutil.h>
#include <mapiext.h>

#include "Util.h"
#include "stringutil.h"
#include "mapi_ptr.h"
#include "mapiguidext.h"

#include <algorithm>
using namespace std;
using namespace za::helpers;

namespace za { namespace operations {

/**
 * Constructor.
 * @param[in]	lpLogger
 *					Pointer to an ECLogger object that's used for logging.
 */
ArchiveOperationBase::ArchiveOperationBase(ECArchiverLogger *lpLogger, int ulAge, bool bProcessUnread, ULONG ulInhibitMask)
: m_lpLogger(lpLogger)
, m_ulAge(ulAge)
, m_bProcessUnread(bProcessUnread)
, m_ulInhibitMask(ulInhibitMask)
{
	GetSystemTimeAsFileTime(&m_ftCurrent);
}



HRESULT ArchiveOperationBase::GetRestriction(LPMAPIPROP lpMapiProp, LPSRestriction *lppRestriction)
{
	HRESULT hr = hrSuccess;
	ULARGE_INTEGER li;
	SPropValue sPropRefTime;
	ECAndRestriction resResult;

	const ECOrRestriction resDefault(
		ECAndRestriction(
			ECExistRestriction(PR_MESSAGE_DELIVERY_TIME) +
			ECPropertyRestriction(RELOP_LT, PR_MESSAGE_DELIVERY_TIME, &sPropRefTime, ECRestriction::Cheap)
		) +
		ECAndRestriction(
			ECExistRestriction(PR_CLIENT_SUBMIT_TIME) +
			ECPropertyRestriction(RELOP_LT, PR_CLIENT_SUBMIT_TIME, &sPropRefTime, ECRestriction::Cheap)
		)
	);

	PROPMAP_START
	PROPMAP_NAMED_ID(FLAGS, PT_LONG, PSETID_Archive, dispidFlags)
	PROPMAP_INIT(lpMapiProp)

	if (lppRestriction == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	if (m_ulAge < 0) {
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}

	li.LowPart = m_ftCurrent.dwLowDateTime;
	li.HighPart = m_ftCurrent.dwHighDateTime;
	
	li.QuadPart -= (m_ulAge * _DAY);
		
	sPropRefTime.ulPropTag = PROP_TAG(PT_SYSTIME, 0);
	sPropRefTime.Value.ft.dwLowDateTime = li.LowPart;
	sPropRefTime.Value.ft.dwHighDateTime = li.HighPart;

	resResult.append(resDefault);
	if (!m_bProcessUnread)
		resResult.append(ECBitMaskRestriction(BMR_NEZ, PR_MESSAGE_FLAGS, MSGFLAG_READ));
	resResult.append(
		ECNotRestriction(
			ECAndRestriction(
				ECExistRestriction(PROP_FLAGS) +
				ECBitMaskRestriction(BMR_NEZ, PROP_FLAGS, m_ulInhibitMask)
			)
		)
	);


	hr = resResult.CreateMAPIRestriction(lppRestriction);

exit:
	return hr;
}

HRESULT ArchiveOperationBase::VerifyRestriction(LPMESSAGE lpMessage)
{
	HRESULT hr = hrSuccess;
	SRestrictionPtr ptrRestriction;

	hr = GetRestriction(lpMessage, &ptrRestriction);
	if (hr != hrSuccess)
		goto exit;

	hr = TestRestriction(ptrRestriction, lpMessage, createLocaleFromName(""));

exit:
	return hr;
}

/**
 * Constructor.
 * @param[in]	lpLogger
 *					Pointer to an ECLogger object that's used for logging.
 */
ArchiveOperationBaseEx::ArchiveOperationBaseEx(ECArchiverLogger *lpLogger, int ulAge, bool bProcessUnread, ULONG ulInhibitMask)
: ArchiveOperationBase(lpLogger, ulAge, bProcessUnread, ulInhibitMask)
{ }

/**
 * Process the message that exists in the search folder ptrFolder and deletegate the work to the derived class.
 * The property array lpProps contains at least the entryid of the message to open and the entryid of the parent
 * folder of the message to open.
 *
 * @param[in]	lpFolder
 *					A MAPIFolder pointer that points to the parent folder.
 * @param[in]	cProps
 *					The number op properties pointed to by lpProps.
 * @param[in]	lpProps
 *					Pointer to an array of properties that are used by the Operation object.
 *
 * @return HRESULT
 */
HRESULT ArchiveOperationBaseEx::ProcessEntry(LPMAPIFOLDER lpFolder, ULONG cProps, const LPSPropValue lpProps)
{
	HRESULT hr = hrSuccess;
	LPSPropValue lpFolderEntryId;
	bool bReloadFolder = false;
	ULONG ulType = 0;

	ASSERT(lpFolder != NULL);
	if (lpFolder == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}
	
	lpFolderEntryId = PpropFindProp(lpProps, cProps, PR_PARENT_ENTRYID);
	if (lpFolderEntryId == NULL) {
		Logger()->Log(EC_LOGLEVEL_FATAL, "PR_PARENT_ENTRYID missing");
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}
	
	if (!m_ptrCurFolderEntryId.is_null()) {
		int nResult = 0;
		// @todo: Create correct locale.
		hr = Util::CompareProp(m_ptrCurFolderEntryId, lpFolderEntryId, createLocaleFromName(""), &nResult);
		if (hr != hrSuccess) {
			Logger()->Log(EC_LOGLEVEL_FATAL, "Failed to compare current and new entryid. (hr=%s)", stringify(hr, true).c_str());
			goto exit;
		}
		
		if (nResult != 0) {
			Logger()->Log(EC_LOGLEVEL_DEBUG, "Leaving folder (%s)", bin2hex(m_ptrCurFolderEntryId->Value.bin.cb, m_ptrCurFolderEntryId->Value.bin.lpb).c_str());
			Logger()->SetFolder(_T(""));
			hr = LeaveFolder();
			if (hr != hrSuccess) {
				Logger()->Log(EC_LOGLEVEL_FATAL, "Failed to leave folder. (hr=%s)", stringify(hr, true).c_str());
				goto exit;
			}
				
			bReloadFolder = true;
		}
	}
	
	if (m_ptrCurFolderEntryId.is_null() || bReloadFolder) {
		SPropValuePtr ptrPropValue;
        
		Logger()->Log(EC_LOGLEVEL_DEBUG, "Opening folder (%s)", bin2hex(lpFolderEntryId->Value.bin.cb, lpFolderEntryId->Value.bin.lpb).c_str());
	
		hr = lpFolder->OpenEntry(lpFolderEntryId->Value.bin.cb, (LPENTRYID)lpFolderEntryId->Value.bin.lpb, &m_ptrCurFolder.iid, MAPI_BEST_ACCESS|fMapiDeferredErrors, &ulType, &m_ptrCurFolder);
		if (hr != hrSuccess) {
			Logger()->Log(EC_LOGLEVEL_FATAL, "Failed to open folder. (hr=%s)", stringify(hr, true).c_str());
			goto exit;
		}
		
		hr = MAPIAllocateBuffer(sizeof(SPropValue), &m_ptrCurFolderEntryId);
		if (hr != hrSuccess)
			goto exit;
		
		hr = Util::HrCopyProperty(m_ptrCurFolderEntryId, lpFolderEntryId, m_ptrCurFolderEntryId);
		if (hr != hrSuccess)
			goto exit;

		if (HrGetOneProp(m_ptrCurFolder, PR_DISPLAY_NAME, &ptrPropValue) == hrSuccess)
			Logger()->SetFolder(ptrPropValue->Value.LPSZ);
		else
			Logger()->SetFolder(_T("<Unnamed>"));

		hr = EnterFolder(m_ptrCurFolder);
		if (hr != hrSuccess) {
			Logger()->Log(EC_LOGLEVEL_FATAL, "Failed to enter folder. (hr=%s)", stringify(hr, true).c_str());
			goto exit;
		}
	}
	
	hr = DoProcessEntry(cProps, lpProps);
	
exit:
	return hr;
}

}} // namespaces 
