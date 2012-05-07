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
#include "archive.h"

#include "ECLogger.h"
#include "ECGetText.h"
#include "charset/convert.h"
#include "mapi_ptr.h"

#include "helpers/storehelper.h"
#include "operations/copier.h"
#include "operations/instanceidmapper.h"
#include "archiver-session.h"

#include <list>
#include <sstream>

#include "Util.h"
#include "ECDebug.h"

using namespace za::helpers;
using namespace za::operations;
using namespace std;

typedef std::auto_ptr<Copier::Helper> HelperPtr;

#ifdef UNICODE
typedef std::wostringstream tostringstream;
#else
typedef std::ostringstream tostringstream;
#endif


void ArchiveResult::AddMessage(MessagePtr ptrMessage) {
	m_lstMessages.push_back(ptrMessage);
}

void ArchiveResult::Undo(IMAPISession *lpSession) {
	for (list<MessagePtr>::iterator i = m_lstMessages.begin(); i != m_lstMessages.end(); ++i)
		Util::HrDeleteMessage(lpSession, *i);
}


HRESULT Archive::Create(IMAPISession *lpSession, ECLogger *lpLogger, ArchivePtr *lpptrArchive)
{
	HRESULT hr = hrSuccess;

	if (lpSession == NULL || lpLogger == NULL || lpptrArchive == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	try {
		lpptrArchive->reset(new Archive(lpSession, lpLogger));
	} catch (const std::bad_alloc &) {
		hr = MAPI_E_NOT_ENOUGH_MEMORY;
	}

exit:
	return hr;
}

Archive::Archive(IMAPISession *lpSession, ECLogger *lpLogger)
: m_ptrSession(lpSession, true)
, m_lpLogger(lpLogger)
{
	m_lpLogger->AddRef();
}

Archive::~Archive()
{
	m_lpLogger->Release();
}

HRESULT Archive::HrArchiveMessageForDelivery(IMessage *lpMessage)
{
	HRESULT hr = hrSuccess;
	ULONG cMsgProps;
	SPropArrayPtr ptrMsgProps;
	MsgStorePtr ptrStore;
	ULONG ulType;
	MAPIFolderPtr ptrFolder;
	StoreHelperPtr ptrStoreHelper;
	SObjectEntry refMsgEntry;
	ObjectEntryList lstArchives;
	ObjectEntryList::iterator iArchive;
	SessionPtr ptrSession;
	InstanceIdMapperPtr ptrMapper;
	HelperPtr ptrHelper;
	list<pair<MessagePtr,PostSaveActionPtr> > lstArchivedMessages;
	list<pair<MessagePtr,PostSaveActionPtr> >::iterator iArchivedMessage;
	ArchiveResult result;
	ObjectEntryList lstReferences;
	MAPIPropHelperPtr ptrMsgHelper;

	SizedSPropTagArray(3, sptaMessageProps) = {3, {PR_ENTRYID, PR_STORE_ENTRYID, PR_PARENT_ENTRYID}};
	enum {IDX_ENTRYID, IDX_STORE_ENTRYID, IDX_PARENT_ENTRYID};

	if (lpMessage == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	hr = lpMessage->GetProps((LPSPropTagArray)&sptaMessageProps, 0, &cMsgProps, &ptrMsgProps);
	if (hr != hrSuccess)
		goto exit;

	refMsgEntry.sStoreEntryId.assign(ptrMsgProps[IDX_STORE_ENTRYID].Value.bin);
	refMsgEntry.sItemEntryId.assign(ptrMsgProps[IDX_ENTRYID].Value.bin);

	hr = m_ptrSession->OpenMsgStore(0, ptrMsgProps[IDX_STORE_ENTRYID].Value.bin.cb, (LPENTRYID)ptrMsgProps[IDX_STORE_ENTRYID].Value.bin.lpb, &ptrStore.iid, MDB_WRITE, &ptrStore);
	if (hr != hrSuccess)
		goto exit;

	hr = StoreHelper::Create(ptrStore, &ptrStoreHelper);
	if (hr != hrSuccess)
		goto exit;

	hr = ptrStoreHelper->GetArchiveList(&lstArchives);
	if (hr != hrSuccess)
		goto exit;

	if (lstArchives.empty()) {
		m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "No archives attached to store");
		goto exit;
	}

	hr = ptrStore->OpenEntry(ptrMsgProps[IDX_PARENT_ENTRYID].Value.bin.cb, (LPENTRYID)ptrMsgProps[IDX_PARENT_ENTRYID].Value.bin.lpb, &ptrFolder.iid, MAPI_MODIFY, &ulType, &ptrFolder);
	if (hr != hrSuccess)
		goto exit;

	hr = Session::Create(m_ptrSession, m_lpLogger, &ptrSession);
	if (hr != hrSuccess)
		goto exit;

	/**
	 * @todo: Create an archiver config object globally in the calling application to
	 *        avoid the creation of the configuration for each message to be archived.
	 */
	hr = InstanceIdMapper::Create(m_lpLogger, NULL, &ptrMapper);
	if (hr != hrSuccess)
		goto exit;

	// First create all (mostly one) the archive messages without saving them.
	ptrHelper.reset(new Copier::Helper(ptrSession, m_lpLogger, ptrMapper, NULL, ptrFolder));
	for (iArchive = lstArchives.begin(); iArchive != lstArchives.end(); ++iArchive) {
		MessagePtr ptrArchivedMsg;
		PostSaveActionPtr ptrPSAction;

		hr = ptrHelper->CreateArchivedMessage(lpMessage, *iArchive, refMsgEntry, &ptrArchivedMsg, &ptrPSAction);
		if (hr != hrSuccess)
			goto exit;

		lstArchivedMessages.push_back(make_pair(ptrArchivedMsg, ptrPSAction));
	}

	// Now save the messages one by one. On failure all saved messages need to be deleted.
	for (iArchivedMessage = lstArchivedMessages.begin(); iArchivedMessage != lstArchivedMessages.end(); ++iArchivedMessage) {
		ULONG cArchivedMsgProps;
		SPropArrayPtr ptrArchivedMsgProps;
		SObjectEntry refArchiveEntry;

		hr = iArchivedMessage->first->GetProps((LPSPropTagArray)&sptaMessageProps, 0, &cArchivedMsgProps, &ptrArchivedMsgProps);
		if (hr != hrSuccess)
			goto exit;

		refArchiveEntry.sItemEntryId.assign(ptrArchivedMsgProps[IDX_ENTRYID].Value.bin);
		refArchiveEntry.sStoreEntryId.assign(ptrArchivedMsgProps[IDX_STORE_ENTRYID].Value.bin);
		lstReferences.push_back(refArchiveEntry);

		hr = iArchivedMessage->first->SaveChanges(KEEP_OPEN_READWRITE);
		if (hr != hrSuccess)
			goto exit;

		if (iArchivedMessage->second) {
			HRESULT hrTmp = iArchivedMessage->second->Execute();
			if (hrTmp != hrSuccess)
				m_lpLogger->Log(EC_LOGLEVEL_WARNING, "Failed to execute post save action. hr=0x%08x", hrTmp);
		}

		result.AddMessage(iArchivedMessage->first);
	}

	// Now add the references to the original message.
	lstReferences.sort();
	lstReferences.unique();

	hr = MAPIPropHelper::Create(MAPIPropPtr(lpMessage, true), &ptrMsgHelper);
	if (hr != hrSuccess)
		goto exit;

	hr = ptrMsgHelper->SetArchiveList(lstReferences, true);

exit:
	// On error delete all saved archives
	if (FAILED(hr))
		result.Undo(m_ptrSession);

	return hr;
}


HRESULT Archive::HrArchiveMessageForSending(IMessage *lpMessage, ArchiveResult *lpResult)
{
	HRESULT hr = hrSuccess;
	ULONG cMsgProps;
	SPropArrayPtr ptrMsgProps;
	MsgStorePtr ptrStore;
	StoreHelperPtr ptrStoreHelper;
	ObjectEntryList lstArchives;
	ObjectEntryList::iterator iArchive;
	SessionPtr ptrSession;
	InstanceIdMapperPtr ptrMapper;
	HelperPtr ptrHelper;
	list<pair<MessagePtr,PostSaveActionPtr> > lstArchivedMessages;
	list<pair<MessagePtr,PostSaveActionPtr> >::iterator iArchivedMessage;
	ArchiveResult result;

	SizedSPropTagArray(2, sptaMessageProps) = {1, {PR_STORE_ENTRYID}};
	enum {IDX_STORE_ENTRYID};

	if (lpMessage == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	hr = lpMessage->GetProps((LPSPropTagArray)&sptaMessageProps, 0, &cMsgProps, &ptrMsgProps);
	if (hr != hrSuccess)
		goto exit;

	hr = m_ptrSession->OpenMsgStore(0, ptrMsgProps[IDX_STORE_ENTRYID].Value.bin.cb, (LPENTRYID)ptrMsgProps[IDX_STORE_ENTRYID].Value.bin.lpb, &ptrStore.iid, 0, &ptrStore);
	if (hr != hrSuccess)
		goto exit;

	hr = StoreHelper::Create(ptrStore, &ptrStoreHelper);
	if (hr != hrSuccess)
		goto exit;

	hr = ptrStoreHelper->GetArchiveList(&lstArchives);
	if (hr != hrSuccess) {
		m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to obtain list of attached archives. hr=0x%08x", hr);
		SetErrorMessage(hr, _("Unable to obtain list of attached archives."));
		goto exit;
	}

	if (lstArchives.empty()) {
		m_lpLogger->Log(EC_LOGLEVEL_DEBUG, "No archives attached to store");
		goto exit;
	}

	hr = Session::Create(m_ptrSession, m_lpLogger, &ptrSession);
	if (hr != hrSuccess)
		goto exit;

	/**
	 * @todo: Create an archiver config object globally in the calling application to
	 *        avoid the creation of the configuration for each message to be archived.
	 */
	hr = InstanceIdMapper::Create(m_lpLogger, NULL, &ptrMapper);
	if (hr != hrSuccess)
		goto exit;

	// First create all (mostly one) the archive messages without saving them.
	ptrHelper.reset(new Copier::Helper(ptrSession, m_lpLogger, ptrMapper, NULL, MAPIFolderPtr()));	// We pass an empty MAPIFolderPtr here!
	for (iArchive = lstArchives.begin(); iArchive != lstArchives.end(); ++iArchive) {
		ArchiveHelperPtr ptrArchiveHelper;
		MAPIFolderPtr ptrArchiveFolder;
		MessagePtr ptrArchivedMsg;
		PostSaveActionPtr ptrPSAction;

		hr = ArchiveHelper::Create(ptrSession, *iArchive, m_lpLogger, &ptrArchiveHelper);
		if (hr != hrSuccess) {
			SetErrorMessage(hr, _("Unable to open archive."));
			goto exit;
		}

		hr = ptrArchiveHelper->GetOutgoingFolder(&ptrArchiveFolder);
		if (hr != hrSuccess) {
			m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Failed to get outgoing archive folder. hr=0x%08x", hr);
			SetErrorMessage(hr, _("Unable to get outgoing archive folder."));
			goto exit;
		}

		hr = ptrArchiveFolder->CreateMessage(&ptrArchivedMsg.iid, 0, &ptrArchivedMsg);
		if (hr != hrSuccess) {
			m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Failed to create message in outgoing archive folder. hr=0x%08x", hr);
			SetErrorMessage(hr, _("Unable to create archive message in outgoing archive folder."));
			goto exit;
		}

		hr = ptrHelper->ArchiveMessage(lpMessage, NULL, ptrArchivedMsg, &ptrPSAction);
		if (hr != hrSuccess) {
			SetErrorMessage(hr, _("Unable to copy message data."));
			goto exit;
		}

		m_lpLogger->Log(EC_LOGLEVEL_INFO, "Stored message in archive");
		lstArchivedMessages.push_back(make_pair(ptrArchivedMsg, ptrPSAction));
	}

	// Now save the messages one by one. On failure all saved messages need to be deleted.
	for (iArchivedMessage = lstArchivedMessages.begin(); iArchivedMessage != lstArchivedMessages.end(); ++iArchivedMessage) {
		hr = iArchivedMessage->first->SaveChanges(KEEP_OPEN_READONLY);
		if (hr != hrSuccess) {
			m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Failed to save message in archive. hr=0x%08x", hr);
			SetErrorMessage(hr, _("Unable to save archived message."));
			goto exit;
		}

		if (iArchivedMessage->second) {
			HRESULT hrTmp = iArchivedMessage->second->Execute();
			if (hrTmp != hrSuccess)
				m_lpLogger->Log(EC_LOGLEVEL_WARNING, "Failed to execute post save action. hr=0x%08x", hrTmp);
		}

		result.AddMessage(iArchivedMessage->first);
	}

	if (lpResult)
		std::swap(result, *lpResult);

exit:
	// On error delete all saved archives
	if (FAILED(hr))
		result.Undo(m_ptrSession);

	return hr;
}

void Archive::SetErrorMessage(HRESULT hr, LPCTSTR lpszMessage)
{
	tostringstream	oss;
	LPTSTR lpszDesc;

	oss << lpszMessage << endl;
	oss << _("Error code:") << _T(" ") << convert_to<tstring>(GetMAPIErrorDescription(hr))
		<< _T(" (") << tstringify(hr, true) << _T(")") << endl;

	if (Util::HrMAPIErrorToText(hr, &lpszDesc) == hrSuccess)
		oss << _("Error description:") << _T(" ") << lpszDesc << endl;

	m_strErrorMessage.assign(oss.str());
}
