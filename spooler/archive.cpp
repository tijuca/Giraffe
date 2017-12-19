/*
 * Copyright 2005 - 2016 Zarafa and its licensors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License, version 3,
 * as published by the Free Software Foundation.
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

#include <kopano/platform.h>
#include <iostream>
#include <new>
#include <utility>
#include "archive.h"

#include <kopano/ECLogger.h>
#include <kopano/ECGetText.h>
#include <kopano/MAPIErrors.h>
#include <kopano/charset/convert.h>
#include <kopano/mapi_ptr.h>

#include "helpers/StoreHelper.h"
#include "operations/copier.h"
#include "operations/instanceidmapper.h"
#include "ArchiverSession.h"
#include "helpers/ArchiveHelper.h"

#include <list>
#include <sstream>

#include <kopano/Util.h>
#include <kopano/ECDebug.h>
#include "PyMapiPlugin.h"

using namespace KC::helpers;
using namespace KC::operations;
using std::endl;
using std::list;
using std::pair;
using std::string;

#ifdef UNICODE
typedef std::wostringstream tostringstream;
#else
typedef std::ostringstream tostringstream;
#endif

void ArchiveResult::AddMessage(MessagePtr ptrMessage) {
	m_lstMessages.emplace_back(ptrMessage);
}

void ArchiveResult::Undo(IMAPISession *lpSession) {
	for (const auto i : m_lstMessages)
		Util::HrDeleteMessage(lpSession, i);
}

HRESULT Archive::Create(IMAPISession *lpSession, ArchivePtr *lpptrArchive)
{
	if (lpSession == NULL || lpptrArchive == NULL)
		return MAPI_E_INVALID_PARAMETER;
	auto x = new(std::nothrow) Archive(lpSession);
	if (x == nullptr)
		return MAPI_E_NOT_ENOUGH_MEMORY;
	lpptrArchive->reset(x);
	return hrSuccess;
}

Archive::Archive(IMAPISession *lpSession)
: m_ptrSession(lpSession, true)
{
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
	ArchiverSessionPtr ptrSession;
	InstanceIdMapperPtr ptrMapper;
	std::unique_ptr<Copier::Helper> ptrHelper;
	list<pair<MessagePtr,PostSaveActionPtr> > lstArchivedMessages;
	ArchiveResult result;
	ObjectEntryList lstReferences;
	MAPIPropHelperPtr ptrMsgHelper;
	static constexpr const SizedSPropTagArray(3, sptaMessageProps) =
		{3, {PR_ENTRYID, PR_STORE_ENTRYID, PR_PARENT_ENTRYID}};
	enum {IDX_ENTRYID, IDX_STORE_ENTRYID, IDX_PARENT_ENTRYID};

	if (lpMessage == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		ec_log_warn("Archive::HrArchiveMessageForDelivery(): invalid parameter");
		goto exit;
	}
	hr = lpMessage->GetProps(sptaMessageProps, 0, &cMsgProps, &~ptrMsgProps);
	if (hr != hrSuccess) {
		ec_log_warn("Archive::HrArchiveMessageForDelivery(): GetProps failed %x", hr);
		goto exit;
	}
	refMsgEntry.sStoreEntryId = ptrMsgProps[IDX_STORE_ENTRYID].Value.bin;
	refMsgEntry.sItemEntryId = ptrMsgProps[IDX_ENTRYID].Value.bin;
	hr = m_ptrSession->OpenMsgStore(0, ptrMsgProps[IDX_STORE_ENTRYID].Value.bin.cb,
	     reinterpret_cast<ENTRYID *>(ptrMsgProps[IDX_STORE_ENTRYID].Value.bin.lpb),
	     &iid_of(ptrStore), MDB_WRITE, &~ptrStore);
	if (hr != hrSuccess) {
		ec_log_warn("Archive::HrArchiveMessageForDelivery(): OpenMsgStore failed %x", hr);
		goto exit;
	}

	hr = StoreHelper::Create(ptrStore, &ptrStoreHelper);
	if (hr != hrSuccess) {
		ec_log_warn("Archive::HrArchiveMessageForDelivery(): StoreHelper::Create failed %x", hr);
		goto exit;
	}

	hr = ptrStoreHelper->GetArchiveList(&lstArchives);
	if (hr != hrSuccess) {
		ec_log_warn("Archive::HrArchiveMessageForDelivery(): StoreHelper::GetArchiveList failed %x", hr);
		goto exit;
	}

	if (lstArchives.empty()) {
		ec_log_debug("No archives attached to store");
		goto exit;
	}
	hr = ptrStore->OpenEntry(ptrMsgProps[IDX_PARENT_ENTRYID].Value.bin.cb,
	     reinterpret_cast<ENTRYID *>(ptrMsgProps[IDX_PARENT_ENTRYID].Value.bin.lpb),
	     &iid_of(ptrFolder), MAPI_MODIFY, &ulType, &~ptrFolder);
	if (hr != hrSuccess) {
		ec_log_warn("Archive::HrArchiveMessageForDelivery(): StoreHelper::OpenEntry failed %x", hr);
		goto exit;
	}

	hr = ArchiverSession::Create(m_ptrSession, ec_log_get(), &ptrSession);
	if (hr != hrSuccess) {
		ec_log_warn("Archive::HrArchiveMessageForDelivery(): ArchiverSession::Create failed %x", hr);
		goto exit;
	}

	/**
	 * @todo: Create an archiver config object globally in the calling application to
	 *        avoid the creation of the configuration for each message to be archived.
	 */
	hr = InstanceIdMapper::Create(ec_log_get(), NULL, &ptrMapper);
	if (hr != hrSuccess) {
		ec_log_warn("Archive::HrArchiveMessageForDelivery(): InstanceIdMapper::Create failed %x", hr);
		goto exit;
	}

	// First create all (mostly one) the archive messages without saving them.
	ptrHelper.reset(new(std::nothrow) Copier::Helper(ptrSession,
		ec_log_get(), ptrMapper, nullptr, ptrFolder));
	if (ptrHelper == nullptr) {
		hr = MAPI_E_NOT_ENOUGH_MEMORY;
		goto exit;
	}
	for (const auto &arc : lstArchives) {
		MessagePtr ptrArchivedMsg;
		PostSaveActionPtr ptrPSAction;

		hr = ptrHelper->CreateArchivedMessage(lpMessage, arc, refMsgEntry, &~ptrArchivedMsg, &ptrPSAction);
		if (hr != hrSuccess) {
			ec_log_warn("Archive::HrArchiveMessageForDelivery(): CreateArchivedMessage failed %x", hr);
			goto exit;
		}
		lstArchivedMessages.emplace_back(ptrArchivedMsg, ptrPSAction);
	}

	// Now save the messages one by one. On failure all saved messages need to be deleted.
	for (const auto &msg : lstArchivedMessages) {
		ULONG cArchivedMsgProps;
		SPropArrayPtr ptrArchivedMsgProps;
		SObjectEntry refArchiveEntry;

		hr = msg.first->GetProps(sptaMessageProps, 0,
		     &cArchivedMsgProps, &~ptrArchivedMsgProps);
		if (hr != hrSuccess) {
			ec_log_warn("Archive::HrArchiveMessageForDelivery(): ArchivedMessage GetProps failed %x", hr);
			goto exit;
		}
		refArchiveEntry.sItemEntryId = ptrArchivedMsgProps[IDX_ENTRYID].Value.bin;
		refArchiveEntry.sStoreEntryId = ptrArchivedMsgProps[IDX_STORE_ENTRYID].Value.bin;
		lstReferences.emplace_back(refArchiveEntry);
		hr = msg.first->SaveChanges(KEEP_OPEN_READWRITE);
		if (hr != hrSuccess) {
			ec_log_warn("Archive::HrArchiveMessageForDelivery(): ArchivedMessage SaveChanges failed %x", hr);
			goto exit;
		}

		if (msg.second) {
			HRESULT hrTmp = msg.second->Execute();
			if (hrTmp != hrSuccess)
				ec_log_warn("Failed to execute post save action. hr=0x%08x", hrTmp);
		}

		result.AddMessage(msg.first);
	}

	// Now add the references to the original message.
	lstReferences.sort();
	lstReferences.unique();

	hr = MAPIPropHelper::Create(MAPIPropPtr(lpMessage, true), &ptrMsgHelper);
	if (hr != hrSuccess) {
		ec_log_warn("Archive::HrArchiveMessageForDelivery(): failed creating reference to original message %x", hr);
		goto exit;
	}

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
	ArchiverSessionPtr ptrSession;
	InstanceIdMapperPtr ptrMapper;
	std::unique_ptr<Copier::Helper> ptrHelper;
	list<pair<MessagePtr,PostSaveActionPtr> > lstArchivedMessages;
	ArchiveResult result;
	static constexpr const SizedSPropTagArray(2, sptaMessageProps) = {1, {PR_STORE_ENTRYID}};
	enum {IDX_STORE_ENTRYID};

	if (lpMessage == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}
	hr = lpMessage->GetProps(sptaMessageProps, 0, &cMsgProps, &~ptrMsgProps);
	if (hr != hrSuccess) {
		ec_log_warn("Archive::HrArchiveMessageForSending(): GetProps failed %x", hr);
		goto exit;
	}
	hr = m_ptrSession->OpenMsgStore(0, ptrMsgProps[IDX_STORE_ENTRYID].Value.bin.cb,
	     reinterpret_cast<ENTRYID *>(ptrMsgProps[IDX_STORE_ENTRYID].Value.bin.lpb),
	     &iid_of(ptrStore), 0, &~ptrStore);
	if (hr != hrSuccess) {
		ec_log_warn("Archive::HrArchiveMessageForSending(): OpenMsgStore failed %x", hr);
		goto exit;
	}

	hr = StoreHelper::Create(ptrStore, &ptrStoreHelper);
	if (hr != hrSuccess) {
		ec_log_warn("Archive::HrArchiveMessageForSending(): StoreHelper::Create failed %x", hr);
		goto exit;
	}

	hr = ptrStoreHelper->GetArchiveList(&lstArchives);
	if (hr != hrSuccess) {
		kc_perror("Unable to obtain list of attached archives", hr);
		SetErrorMessage(hr, _("Unable to obtain list of attached archives."));
		goto exit;
	}

	if (lstArchives.empty()) {
		ec_log_debug("No archives attached to store");
		goto exit;
	}

	hr = ArchiverSession::Create(m_ptrSession, ec_log_get(), &ptrSession);
	if (hr != hrSuccess) {
		ec_log_warn("Archive::HrArchiveMessageForSending(): ArchiverSession::Create failed %x", hr);
		goto exit;
	}

	/**
	 * @todo: Create an archiver config object globally in the calling application to
	 *        avoid the creation of the configuration for each message to be archived.
	 */
	hr = InstanceIdMapper::Create(ec_log_get(), NULL, &ptrMapper);
	if (hr != hrSuccess) {
		ec_log_warn("Archive::HrArchiveMessageForSending(): InstanceIdMapper::Create failed %x", hr);
		goto exit;
	}

	// First create all (mostly one) the archive messages without saving them.
	// We pass an empty MAPIFolderPtr here!
	ptrHelper.reset(new(std::nothrow) Copier::Helper(ptrSession,
		ec_log_get(), ptrMapper, nullptr, MAPIFolderPtr()));
	if (ptrHelper == nullptr) {
		hr = MAPI_E_NOT_ENOUGH_MEMORY;
		goto exit;
	}
	for (const auto &arc : lstArchives) {
		ArchiveHelperPtr ptrArchiveHelper;
		MAPIFolderPtr ptrArchiveFolder;
		MessagePtr ptrArchivedMsg;
		PostSaveActionPtr ptrPSAction;

		hr = ArchiveHelper::Create(ptrSession, arc, ec_log_get(), &ptrArchiveHelper);
		if (hr != hrSuccess) {
			SetErrorMessage(hr, _("Unable to open archive."));
			goto exit;
		}
		hr = ptrArchiveHelper->GetOutgoingFolder(&~ptrArchiveFolder);
		if (hr != hrSuccess) {
			kc_perror("Failed to get outgoing archive folder", hr);
			SetErrorMessage(hr, _("Unable to get outgoing archive folder."));
			goto exit;
		}
		hr = ptrArchiveFolder->CreateMessage(&iid_of(ptrArchivedMsg), 0, &~ptrArchivedMsg);
		if (hr != hrSuccess) {
			kc_perror("Failed to create message in outgoing archive folder", hr);
			SetErrorMessage(hr, _("Unable to create archive message in outgoing archive folder."));
			goto exit;
		}

		hr = ptrHelper->ArchiveMessage(lpMessage, NULL, ptrArchivedMsg, &ptrPSAction);
		if (hr != hrSuccess) {
			SetErrorMessage(hr, _("Unable to copy message data."));
			goto exit;
		}

		ec_log_info("Stored message in archive");
		lstArchivedMessages.emplace_back(ptrArchivedMsg, ptrPSAction);
	}

	// Now save the messages one by one. On failure all saved messages need to be deleted.
	for (const auto &msg : lstArchivedMessages) {
		hr = msg.first->SaveChanges(KEEP_OPEN_READONLY);
		if (hr != hrSuccess) {
			kc_perror("Failed to save message in archive", hr);
			SetErrorMessage(hr, _("Unable to save archived message."));
			goto exit;
		}

		if (msg.second) {
			HRESULT hrTmp = msg.second->Execute();
			if (hrTmp != hrSuccess)
				ec_log_warn("Failed to execute post save action. hr=0x%08x", hrTmp);
		}

		result.AddMessage(msg.first);
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
	oss << _("Error code:") << KC_T(" ") << convert_to<tstring>(GetMAPIErrorDescription(hr))
		<< KC_T(" (") << tstringify(hr, true) << KC_T(")") << endl;
	if (Util::HrMAPIErrorToText(hr, &lpszDesc) == hrSuccess)
		oss << _("Error description:") << KC_T(" ") << lpszDesc << endl;
	m_strErrorMessage.assign(oss.str());
}

#ifndef ENABLE_PYTHON
PyMapiPluginFactory::PyMapiPluginFactory() {}
PyMapiPluginFactory::~PyMapiPluginFactory() {}

HRESULT PyMapiPluginFactory::create_plugin(ECConfig *, ECLogger *,
    const char *, pym_plugin_intf **ret)
{
	*ret = new(std::nothrow) pym_plugin_intf;
	return *ret != nullptr ? hrSuccess : MAPI_E_NOT_ENOUGH_MEMORY;
}
#endif
