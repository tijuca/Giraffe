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
#include "transaction.h"
#include "archiver-session.h"

namespace za { namespace operations {

/////////////////////////////
// Transaction implementation
/////////////////////////////
Transaction::Transaction(const SObjectEntry &objectEntry): m_objectEntry(objectEntry) 
{ }

HRESULT Transaction::SaveChanges(SessionPtr ptrSession, RollbackPtr *lpptrRollback)
{
	typedef MessageList::iterator iterator;
	HRESULT hr = hrSuccess;
	RollbackPtr ptrRollback(new Rollback());
	bool bPSAFailure = false;

	if (lpptrRollback == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	for (iterator iMessage = m_lstSave.begin(); iMessage != m_lstSave.end(); ++iMessage) {
		if (iMessage->bDeleteOnFailure) {
			hr = ptrRollback->Delete(ptrSession, iMessage->ptrMessage);
			if (hr == hrSuccess)
				goto exit;
		}
		hr = iMessage->ptrMessage->SaveChanges(0);
		if (hr != hrSuccess)
			goto exit;

		if (iMessage->ptrPSAction) {
			if (iMessage->ptrPSAction->Execute() != hrSuccess)
				bPSAFailure = true;
		}
	}

	*lpptrRollback = ptrRollback;

exit:
	if (hr != hrSuccess)
		ptrRollback->Execute(ptrSession);

	if (hr == hrSuccess && bPSAFailure)
		hr = MAPI_W_ERRORS_RETURNED;

	return hr;
}

HRESULT Transaction::PurgeDeletes(SessionPtr ptrSession, TransactionPtr ptrDeferredTransaction)
{
	typedef ObjectList::const_iterator iterator;
	HRESULT hr = hrSuccess;
	MessagePtr ptrMessage;
	IMAPISession *lpSession = ptrSession->GetMAPISession();

	for (iterator iObject = m_lstDelete.begin(); iObject != m_lstDelete.end(); ++iObject) {
		HRESULT hrTmp;
		if (iObject->bDeferredDelete && ptrDeferredTransaction)
			hrTmp = ptrDeferredTransaction->Delete(iObject->objectEntry);

		else {
			ULONG ulType;

			hrTmp = lpSession->OpenEntry(iObject->objectEntry.sItemEntryId.size(), iObject->objectEntry.sItemEntryId, &ptrMessage.iid, 0, &ulType, &ptrMessage);
			if (hrTmp == MAPI_E_NOT_FOUND) {
				MsgStorePtr ptrStore;

				// Try to open the message on the store
				hrTmp = ptrSession->OpenStore(iObject->objectEntry.sStoreEntryId, &ptrStore);
				if (hrTmp == hrSuccess)
					hrTmp = ptrStore->OpenEntry(iObject->objectEntry.sItemEntryId.size(), iObject->objectEntry.sItemEntryId, &ptrMessage.iid, 0, &ulType, &ptrMessage);
			}
			if (hrTmp == hrSuccess)
				hrTmp = Util::HrDeleteMessage(lpSession, ptrMessage);
		}
		if (hrTmp != hrSuccess)
			hr = MAPI_W_ERRORS_RETURNED;
	}

	return hr;
}

HRESULT Transaction::Save(IMessage *lpMessage, bool bDeleteOnFailure, const PostSaveActionPtr &ptrPSAction)
{
	SaveEntry se;
	lpMessage->AddRef();
	se.bDeleteOnFailure = bDeleteOnFailure;
	se.ptrMessage = lpMessage;
	se.ptrPSAction = ptrPSAction;
	m_lstSave.push_back(se);
	return hrSuccess;
}

HRESULT Transaction::Delete(const SObjectEntry &objectEntry, bool bDeferredDelete)
{
	DelEntry de;
	de.objectEntry = objectEntry;
	de.bDeferredDelete = bDeferredDelete;
	m_lstDelete.push_back(de);
	return hrSuccess;
}



//////////////////////////
// Rollback implementation
//////////////////////////
HRESULT Rollback::Delete(SessionPtr ptrSession, IMessage *lpMessage)
{
	HRESULT hr = hrSuccess;
	SPropArrayPtr ptrMsgProps;
	ULONG cMsgProps;
	ULONG ulType;
	DelEntry entry;

	SizedSPropTagArray(2, sptaMsgProps) = {2, {PR_ENTRYID, PR_PARENT_ENTRYID}};
	enum {IDX_ENTRYID, IDX_PARENT_ENTRYID};

	if (lpMessage == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	hr = lpMessage->GetProps((LPSPropTagArray)&sptaMsgProps, 0, &cMsgProps, &ptrMsgProps);
	if (hr != hrSuccess)
		goto exit;

	hr = ptrSession->GetMAPISession()->OpenEntry(ptrMsgProps[IDX_PARENT_ENTRYID].Value.bin.cb, (LPENTRYID)ptrMsgProps[IDX_PARENT_ENTRYID].Value.bin.lpb, &entry.ptrFolder.iid, MAPI_MODIFY, &ulType, &entry.ptrFolder);
	if (hr != hrSuccess)
		goto exit;

	entry.eidMessage.assign(ptrMsgProps[IDX_ENTRYID].Value.bin);
	m_lstDelete.push_back(entry);

exit:
	return hr;
}

HRESULT Rollback::Execute(SessionPtr ptrSession)
{
	typedef MessageList::iterator iterator;
	HRESULT hr = hrSuccess;
	SBinary entryID = {0, NULL};
	ENTRYLIST entryList = {1, &entryID};

	for (iterator iObject = m_lstDelete.begin(); iObject != m_lstDelete.end(); ++iObject) {
		entryID.cb = iObject->eidMessage.size();
		entryID.lpb = iObject->eidMessage;

		if (iObject->ptrFolder->DeleteMessages(&entryList, 0, NULL, 0) != hrSuccess)
			hr = MAPI_W_ERRORS_RETURNED;
	}

	return hr;
}

}} // namespace operations, za
