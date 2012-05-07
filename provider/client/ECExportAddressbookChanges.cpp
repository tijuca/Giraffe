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

#include "ECGuid.h"
#include "ZarafaICS.h"
#include "ZarafaUtil.h"

#include "ECABContainer.h"
#include "IECImportAddressbookChanges.h"
#include "ECMsgStore.h"

#include "ECExportAddressbookChanges.h"

#include <ECLogger.h>
#include <ECSyncLog.h>
#include <stringutil.h>
#include <Util.h>

#include <edkmdb.h>

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

ECExportAddressbookChanges::ECExportAddressbookChanges(ECMsgStore *lpStore) {
	ECSyncLog::GetLogger(&m_lpLogger);

    m_lpMsgStore = lpStore;
	m_lpChanges = NULL;
	m_lpRawChanges = NULL;
	m_ulChangeId = 0;
	m_ulThisChange = 0;
	m_lpImporter = NULL;
}

ECExportAddressbookChanges::~ECExportAddressbookChanges() {
	if(m_lpRawChanges)
		MAPIFreeBuffer(m_lpRawChanges);
	if(m_lpChanges)
		MAPIFreeBuffer(m_lpChanges);
	if(m_lpImporter)
		m_lpImporter->Release();
    if(m_lpLogger)
        m_lpLogger->Release();
}

HRESULT ECExportAddressbookChanges::QueryInterface(REFIID refiid, void **lppInterface) {
	REGISTER_INTERFACE(IID_ECUnknown, this);

	REGISTER_INTERFACE(IID_IECExportAddressbookChanges, &this->m_xECExportAddressbookChanges);
	REGISTER_INTERFACE(IID_IUnknown, &this->m_xECExportAddressbookChanges);

    return MAPI_E_INTERFACE_NOT_SUPPORTED;
}

HRESULT	ECExportAddressbookChanges::Config(LPSTREAM lpStream, ULONG ulFlags, IECImportAddressbookChanges *lpCollector)
{
    HRESULT hr = hrSuccess;
	LARGE_INTEGER lint = {{ 0, 0 }};
    ABEID abeid;
    ULONG ulNewSyncId = 0; // this is, in fact, ignored. This is because we just take the highest id that we see in the changes list.
	STATSTG sStatStg;
	ULONG ulCount = 0;
	ULONG ulProcessed = 0;
	ULONG ulRead = 0;
	ICSCHANGE *lpLastChange = NULL;
	int n = 0;

	// Read state from stream
	hr = lpStream->Stat(&sStatStg, 0);
	if(hr != hrSuccess)
		goto exit;

	hr = lpStream->Seek(lint, STREAM_SEEK_SET, NULL);
	if (hr != hrSuccess)
		goto exit;

	if(sStatStg.cbSize.QuadPart < 8) {
	    m_ulChangeId = 0;
		m_setProcessed.clear();
	} else {
		m_setProcessed.clear();

		hr = lpStream->Read(&m_ulChangeId, sizeof(ULONG), &ulRead);
		if(hr != hrSuccess)
			goto exit;

		hr = lpStream->Read(&ulCount, sizeof(ULONG), &ulRead);
		if(hr != hrSuccess)
			goto exit;

		while(ulCount) {
			hr = lpStream->Read(&ulProcessed, sizeof(ULONG), &ulRead);
			if(hr != hrSuccess)
				goto exit;

			m_setProcessed.insert(ulProcessed);

			ulCount--;
		}
	}

	// ulFlags ignored
    m_lpImporter = lpCollector;
    m_lpImporter->AddRef();

	memset(&abeid, 0, sizeof(ABEID));
	
	abeid.ulType = MAPI_ABCONT;
	memcpy(&abeid.guid, &MUIDECSAB, sizeof(GUID));
	abeid.ulId = 1; // 1 is the first container
    
    // The parent source key is the entryid of the AB container that we're sync'ing
    SBinary sSourceKeyFolder;
    sSourceKeyFolder.lpb = (BYTE *)&abeid;
    sSourceKeyFolder.cb = sizeof(ABEID);

	if(m_lpChanges)
		MAPIFreeBuffer(m_lpChanges);
	m_lpChanges = NULL;

	if(m_lpRawChanges)
		MAPIFreeBuffer(m_lpRawChanges);
	m_lpRawChanges = NULL;

    hr = m_lpMsgStore->lpTransport->HrGetChanges(sSourceKeyFolder, 0, m_ulChangeId, ICS_SYNC_AB, 0, NULL, &ulNewSyncId, &m_ulChanges, &m_lpRawChanges);
    if(hr != hrSuccess)
        goto exit;

	LOG_DEBUG(m_lpLogger, "Got %u address book changes from server.", m_ulChanges);

	if (m_ulChanges > 0) {
		// Store the max changeid for later.
		m_ulMaxChangeId = m_lpRawChanges[m_ulChanges - 1].ulChangeId;

		/*
		 * Sort the changes:
		 * Users must exist before the company, but before the group they are member of
		 * Groups must exist after the company and the users which are the members
		 * Companies must exist after the users and groups, so create that last
		 *
		 * MSw: The above comment seems to make this impossible, but the way things are ordered are:
		 *      1. Users
		 *      2. Groups
		 *      3. Companies
		 *
		 * We also want to group changes for each unique object together so we can easily
		 * perform some filtering on them:
		 * - Add + Change = Add
		 * - Change + Delete = Delete
		 * - Add + (Change +) Delete = -
		 * - Delete + Add = Delete + Add (eg. user changes from object class from/to contact)
		 */
		std::stable_sort(m_lpRawChanges, m_lpRawChanges + m_ulChanges, LeftPrecedesRight);

		// Now go through the changes to remove multiple changes for the same object.
		MAPIAllocateBuffer(sizeof(ICSCHANGE) * m_ulChanges, (void **)&m_lpChanges);

		for (ULONG i = 0; i < m_ulChanges; ++i) {
			// First check if this change hasn't been processed yet
			if (m_setProcessed.find(m_lpRawChanges[i].ulChangeId) != m_setProcessed.end())
				continue;

			// Now check if we changed objects. if so we write the last change to the changes list
			if (lpLastChange && CompareABEID(lpLastChange->sSourceKey.cb, (LPENTRYID)lpLastChange->sSourceKey.lpb,
											 m_lpRawChanges[i].sSourceKey.cb, (LPENTRYID)m_lpRawChanges[i].sSourceKey.lpb) == false) {
				m_lpChanges[n++] = *lpLastChange;
				lpLastChange = NULL;
			}

			if (!lpLastChange)
				lpLastChange = &m_lpRawChanges[i];

			else {
				switch (m_lpRawChanges[i].ulChangeType) {
					case ICS_AB_NEW:
						// This shouldn't happen since apparently we have another change for the same object.
						if (lpLastChange->ulChangeType == ICS_AB_DELETE) {
							// user was deleted and re-created (probably as different object class), so keep both events
							LOG_DEBUG(m_lpLogger, "Got an ICS_AB_NEW change for an object that was deleted, modifying into CHANGE. sourcekey=%s", bin2hex(m_lpRawChanges[i].sSourceKey.cb, m_lpRawChanges[i].sSourceKey.lpb).c_str());
							lpLastChange->ulChangeType = ICS_AB_CHANGE;
						} else
							LOG_DEBUG(m_lpLogger, "Got an ICS_AB_NEW change for an object we've seen before. sourcekey=%s", bin2hex(m_lpRawChanges[i].sSourceKey.cb, m_lpRawChanges[i].sSourceKey.lpb).c_str());
						break;

					case ICS_AB_CHANGE:
						// The only valid previous change could have been an add, since the server doesn't track
						// multiple changes and we can't change an object that was just deleted.
						// We'll ignore this in any case.
						if (lpLastChange->ulChangeType != ICS_AB_NEW)
							LOG_DEBUG(m_lpLogger, "Got an ICS_AB_CHANGE with something else than a ICS_AB_NEW as the previous changes. prev_change=%04x, sourcekey=%s", lpLastChange->ulChangeType, bin2hex(m_lpRawChanges[i].sSourceKey.cb, m_lpRawChanges[i].sSourceKey.lpb).c_str());
						LOG_DEBUG(m_lpLogger, "Ignoring ICS_AB_CHANGE due to previous ICS_AB_NEW. sourcekey=%s", bin2hex(m_lpRawChanges[i].sSourceKey.cb, m_lpRawChanges[i].sSourceKey.lpb).c_str());
						break;

					case ICS_AB_DELETE:
						if (lpLastChange->ulChangeType == ICS_AB_NEW) {
							LOG_DEBUG(m_lpLogger, "Ignoring previous ICS_AB_NEW due to current ICS_AB_DELETE. sourcekey=%s", bin2hex(m_lpRawChanges[i].sSourceKey.cb, m_lpRawChanges[i].sSourceKey.lpb).c_str());
							lpLastChange = NULL;	// An add and a delete results in nothing.
						}

						else if (lpLastChange->ulChangeType == ICS_AB_CHANGE) {
							// We'll ignore the previous change and write the delete now. This way we allow another object
							// with the same ID to be added after this object.
							LOG_DEBUG(m_lpLogger, "Replacing previous ICS_AB_CHANGE with current ICS_AB_DELETE. sourcekey=%s", bin2hex(m_lpRawChanges[i].sSourceKey.cb, m_lpRawChanges[i].sSourceKey.lpb).c_str());
							m_lpChanges[n++] = m_lpRawChanges[i];
							lpLastChange = NULL;
						}
						break;

					default:
						LOG_DEBUG(m_lpLogger, "Got an unknown change. change=%04x, sourcekey=%s", m_lpRawChanges[i].ulChangeType, bin2hex(m_lpRawChanges[i].sSourceKey.cb, m_lpRawChanges[i].sSourceKey.lpb).c_str());
						break;
				}
			}
		}	

		if (lpLastChange) {
			m_lpChanges[n++] = *lpLastChange;
			lpLastChange = NULL;
		}
	}

	m_ulChanges = n;
	LOG_DEBUG(m_lpLogger, "Got %u address book changes after sorting/filtering.", m_ulChanges);

    // Next change is first one
    m_ulThisChange = 0;

exit:
    return hr;
}

HRESULT ECExportAddressbookChanges::Synchronize(ULONG *lpulSteps, ULONG *lpulProgress)
{	
    HRESULT hr = hrSuccess;
    IMAPIProp *lpDestObj = NULL;
    IMAPIProp *lpSrcObj = NULL;
	PABEID eid = NULL;
    
    // Check if we're already done
    if(m_ulThisChange >= m_ulChanges)
        return hrSuccess;
    
    if(m_lpChanges[m_ulThisChange].sSourceKey.cb < sizeof(ABEID)) {
        hr = MAPI_E_INVALID_PARAMETER;
        goto exit;
    }

	eid = (PABEID)m_lpChanges[m_ulThisChange].sSourceKey.lpb;
	LOG_DEBUG(m_lpLogger, "abchange type=%04x, sourcekey=%s", m_lpChanges[m_ulThisChange].ulChangeType, bin2hex(m_lpChanges[m_ulThisChange].sSourceKey.cb, m_lpChanges[m_ulThisChange].sSourceKey.lpb).c_str());

	switch(m_lpChanges[m_ulThisChange].ulChangeType) {
    case ICS_AB_CHANGE:
    case ICS_AB_NEW: {
		hr = m_lpImporter->ImportABChange(eid->ulType, m_lpChanges[m_ulThisChange].sSourceKey.cb, (LPENTRYID)m_lpChanges[m_ulThisChange].sSourceKey.lpb);
        break;
    }
    case ICS_AB_DELETE: {
        hr = m_lpImporter->ImportABDeletion(eid->ulType, m_lpChanges[m_ulThisChange].sSourceKey.cb, (LPENTRYID)m_lpChanges[m_ulThisChange].sSourceKey.lpb);
        break;
    }
    default:
        hr = MAPI_E_INVALID_PARAMETER;
        goto exit;
    }
    
	if (hr == SYNC_E_IGNORE)
		hr = hrSuccess;

	else if (hr == MAPI_E_INVALID_TYPE) {
		m_lpLogger->Log(EC_LOGLEVEL_WARNING, "Ignoring invalid entry, type=%04x, sourcekey=%s", m_lpChanges[m_ulThisChange].ulChangeType, bin2hex(m_lpChanges[m_ulThisChange].sSourceKey.cb, m_lpChanges[m_ulThisChange].sSourceKey.lpb).c_str());
		hr = hrSuccess;
	}

	else if (hr != hrSuccess) {
		LOG_DEBUG(m_lpLogger, "failed type=%04x, hr=%s, sourcekey=%s", m_lpChanges[m_ulThisChange].ulChangeType, stringify(hr, true).c_str(), bin2hex(m_lpChanges[m_ulThisChange].sSourceKey.cb, m_lpChanges[m_ulThisChange].sSourceKey.lpb).c_str());
        goto exit;
	}

	// Mark the change as processed
	m_setProcessed.insert(m_lpChanges[m_ulThisChange].ulChangeId);

    m_ulThisChange++;

    if(lpulSteps)
        *lpulSteps = m_ulChanges;
        
    if(lpulProgress)
        *lpulProgress = m_ulThisChange;
        
    if(m_ulThisChange >= m_ulChanges)
        hr = hrSuccess;
    else
        hr = SYNC_W_PROGRESS;
         
exit:
    if(lpDestObj)
        lpDestObj->Release();
        
    if(lpSrcObj)
        lpSrcObj->Release();
        
    return hr;
}

HRESULT ECExportAddressbookChanges::UpdateState(LPSTREAM lpStream)
{
	HRESULT hr = hrSuccess;
	LARGE_INTEGER zero = {{0,0}};
	ULARGE_INTEGER uzero = {{0,0}};
	ULONG ulCount = 0;
	ULONG ulWritten = 0;
	ULONG ulProcessed = 0;
	std::set<ULONG>::iterator iterProcessed;

	if(m_ulThisChange == m_ulChanges) {
		// All changes have been processed, we can discard processed changes and go to the next server change ID
		m_setProcessed.clear();

		// The last change ID we received is always the highest change ID, unless there were 0 changes, then we stay
		// at the same change count
		if(m_ulChanges != 0)
			m_ulChangeId = m_ulMaxChangeId;
	}

	hr = lpStream->Seek(zero, STREAM_SEEK_SET, NULL);
	if(hr != hrSuccess)
		goto exit;

	hr = lpStream->SetSize(uzero);
	if(hr != hrSuccess)
		goto exit;

	// Write the change ID
	hr = lpStream->Write(&m_ulChangeId, sizeof(ULONG), &ulWritten);
	if(hr != hrSuccess)
		goto exit;

	ulCount = m_setProcessed.size();

	// Write the number of processed IDs to follow
	hr = lpStream->Write(&ulCount, sizeof(ULONG), &ulWritten);
	if(hr != hrSuccess)
		goto exit;

	// Write the processed IDs
	for(iterProcessed = m_setProcessed.begin(); iterProcessed != m_setProcessed.end(); iterProcessed++) {
		ulProcessed = *iterProcessed;

		hr = lpStream->Write(&ulProcessed, sizeof(ULONG), &ulWritten);
		if(hr != hrSuccess)
			goto exit;
	}

	lpStream->Seek(zero, STREAM_SEEK_SET, NULL);

	// All done
exit:
    return hr;
}


/**
 * Compares two ICSCHANGE objects and determines which of the two should precede the other.
 * This function is supposed to be used as the predicate in std::stable_sort.
 *
 * The rules for determining if a certain change should precede another are as follows:
 * - Users take precendence over Groups and Companies.
 * - Groups take precedence over Companies only.
 * - Companies never take precedence.
 * - If the changes are of the same type, precedence is determined by comparing the binary
 *   values using SortCompareABEntryID. If the compare returns less than 0, the left change
 *   should take precedence, otherwise it shouldn't. Note that that doesn't imply that the right
 *   change takes precedence.
 *
 * For the ICS system it is only important to group the users, groups and companies. But for the
 * dupliacte change filter it is necessary to group changes for a single object as well. Hence the
 * 4th rule.
 *
 * @param[in]	left
 *					An ICSCHANGE structure who's sSourceKey is interpreted as an ABEID stucture. It is compared to
 *					the right parameter.
 * @param[in]	right
 *					An ICSCHANGE structure who's sSourceKey is interpreted as an ABEID stucture. It is compared to
 *					the left parameter.
 *
 * @return		boolean
 * @retval		true	
 *					The ICSCHANGE specified by left must be processed before the ICSCHANGE
 *					specified bu right.
 * @retval		false	
 *					The ICSCHANGE specified by left must NOT be processed before the ICSCHANGE
 *					specified by right. Note that this does not mean that the ICSCHANGE specified
 *					by right must be processed before the ICSCHANGE specified by left.
 */
bool ECExportAddressbookChanges::LeftPrecedesRight(const ICSCHANGE &left, const ICSCHANGE &right)
{
	ULONG ulTypeLeft = ((ABEID*)left.sSourceKey.lpb)->ulType;
	ASSERT(ulTypeLeft == MAPI_MAILUSER || ulTypeLeft == MAPI_DISTLIST || ulTypeLeft == MAPI_ABCONT);

	ULONG ulTypeRight = ((ABEID*)right.sSourceKey.lpb)->ulType;
	ASSERT(ulTypeRight == MAPI_MAILUSER || ulTypeRight == MAPI_DISTLIST || ulTypeRight == MAPI_ABCONT);

	if (ulTypeLeft == ulTypeRight)
		return SortCompareABEID(left.sSourceKey.cb, (LPENTRYID)left.sSourceKey.lpb, right.sSourceKey.cb, (LPENTRYID)right.sSourceKey.lpb) < 0;

	if (ulTypeRight == MAPI_ABCONT)		// Company is always bigger.
		return true;

	if (ulTypeRight == MAPI_DISTLIST && ulTypeLeft == MAPI_MAILUSER)	// Group is only bigger than user.
		return true;

	return false;
}


ULONG ECExportAddressbookChanges::xECExportAddressbookChanges::AddRef()
{
    METHOD_PROLOGUE_(ECExportAddressbookChanges, ECExportAddressbookChanges);
    TRACE_MAPI(TRACE_ENTRY, "IECExportAddressbookChanges::AddRef", "");
    return pThis->AddRef();
}

ULONG ECExportAddressbookChanges::xECExportAddressbookChanges::Release()
{
    METHOD_PROLOGUE_(ECExportAddressbookChanges, ECExportAddressbookChanges);
    TRACE_MAPI(TRACE_ENTRY, "IECExportAddressbookChanges::Release", "");
    return pThis->Release();
}

HRESULT ECExportAddressbookChanges::xECExportAddressbookChanges::QueryInterface(REFIID refiid, void **lppInterface)
{
    METHOD_PROLOGUE_(ECExportAddressbookChanges, ECExportAddressbookChanges);
    TRACE_MAPI(TRACE_ENTRY, "IECExportAddressbookChanges::QueryInterface", "");
    return pThis->QueryInterface(refiid, lppInterface);
}

HRESULT ECExportAddressbookChanges::xECExportAddressbookChanges::Config(LPSTREAM lpStream, ULONG ulFlags, IECImportAddressbookChanges *lpCollector)
{
    METHOD_PROLOGUE_(ECExportAddressbookChanges, ECExportAddressbookChanges);
    TRACE_MAPI(TRACE_ENTRY, "IECExportAddressbookChanges::Config", "");
    return pThis->Config(lpStream, ulFlags, lpCollector);
}

HRESULT ECExportAddressbookChanges::xECExportAddressbookChanges::Synchronize(ULONG *lpulSteps, ULONG *lpulProgress)
{
    METHOD_PROLOGUE_(ECExportAddressbookChanges, ECExportAddressbookChanges);
    TRACE_MAPI(TRACE_ENTRY, "IECExportAddressbookChanges::Synchronize", "");
    return pThis->Synchronize(lpulSteps, lpulProgress);
}

HRESULT ECExportAddressbookChanges::xECExportAddressbookChanges::UpdateState(LPSTREAM lpStream)
{
    METHOD_PROLOGUE_(ECExportAddressbookChanges, ECExportAddressbookChanges);
    TRACE_MAPI(TRACE_ENTRY, "IECExportAddressbookChanges::UpdateState", "");
    return pThis->UpdateState(lpStream);
}

