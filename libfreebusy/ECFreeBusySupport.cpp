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
#include <new>
#include <kopano/platform.h>
#include <kopano/memory.hpp>
#include "ECFreeBusySupport.h"

#include "ECFreeBusyUpdate.h"
#include "ECFreeBusyData.h"
#include <kopano/CommonUtil.h>
#include <kopano/mapiext.h>
#include <mapiutil.h>

#include "freebusyutil.h"
#include <kopano/mapi_ptr.h>

using namespace KCHL;

namespace KC {

ECFreeBusySupport::ECFreeBusySupport(void)
{
	GetClientVersion(&m_ulOutlookVersion);
}

ECFreeBusySupport::~ECFreeBusySupport(void)
{
	if(m_lpFreeBusyFolder)
		m_lpFreeBusyFolder->Release();

	if(m_lpUserStore)
		m_lpUserStore->Release();

	if(m_lpPublicStore)
		m_lpPublicStore->Release();

	if(m_lpSession)
		m_lpSession->Release();
}

HRESULT ECFreeBusySupport::Create(ECFreeBusySupport **lppECFreeBusySupport)
{
	return alloc_wrap<ECFreeBusySupport>().put(lppECFreeBusySupport);
}

HRESULT ECFreeBusySupport::QueryInterface(REFIID refiid, void **lppInterface)
{
	REGISTER_INTERFACE2(ECFreeBusySupport, this);
	REGISTER_INTERFACE2(ECUnknown, this);
	REGISTER_INTERFACE2(IFreeBusySupport, this);
	REGISTER_INTERFACE2(IUnknown, this);
	return MAPI_E_INTERFACE_NOT_SUPPORTED;
}

HRESULT ECFreeBusySupport::Open(IMAPISession* lpMAPISession, IMsgStore* lpMsgStore, BOOL bStore)
{
	HRESULT hr = hrSuccess;
	object_ptr<IMsgStore> lpPublicStore;

	if(lpMAPISession == NULL)
		return MAPI_E_INVALID_OBJECT;
#ifdef DEBUG
	if (lpMsgStore) {
		memory_ptr<SPropValue> lpPropArray;
		HrGetOneProp(lpMsgStore, PR_DISPLAY_NAME_A, &lpPropArray);
		ec_log_debug("ECFreeBusySupport::Open", "Storename=%s", (lpPropArray && lpPropArray->ulPropTag == PR_DISPLAY_NAME_A) ? lpPropArray->Value.lpszA : "Error");
	}
#endif

	// Hold the mapisession, the session will be released by function 'close' or 
	// on delete the class
	hr = lpMAPISession->QueryInterface(IID_IMAPISession, (void**)&m_lpSession);
	if(hr != hrSuccess)
		return hr;

	// Open the public store for communicate with the freebusy information.
	hr = HrOpenECPublicStoreOnline(lpMAPISession, &~lpPublicStore);
	if(hr != hrSuccess)
		return hr;
	hr = lpPublicStore->QueryInterface(IID_IMsgStore, (void**)&m_lpPublicStore);
	if(hr != hrSuccess)
		return hr;

	if(lpMsgStore) {
		//Hold the use store for update freebusy
		hr = lpMsgStore->QueryInterface(IID_IMsgStore, (void**)&m_lpUserStore);
		if(hr != hrSuccess)
			return hr;
	}
	return hr;
}

HRESULT ECFreeBusySupport::Close()
{
	if(m_lpSession)
	{
		m_lpSession->Release();
		m_lpSession = NULL;
	}

	if(m_lpPublicStore)
	{
		m_lpPublicStore->Release();
		m_lpPublicStore = NULL;
	}

	if(m_lpUserStore)
	{
		m_lpUserStore->Release();
		m_lpUserStore = NULL;
	}

	return S_OK;
}

HRESULT ECFreeBusySupport::LoadFreeBusyData(ULONG cMax, FBUser *rgfbuser, IFreeBusyData **prgfbdata, HRESULT *phrStatus, ULONG *pcRead)
{
	HRESULT			hr = S_OK;
	ULONG			ulFindUsers = 0;
	ECFBBlockList	fbBlockList;
	LONG			rtmStart = 0;
	LONG			rtmEnd = 0;
	ULONG			i;

	if((cMax > 0 && rgfbuser == NULL) || prgfbdata == NULL)
		return MAPI_E_INVALID_PARAMETER;

	for (i = 0; i < cMax; ++i) {
		object_ptr<IMessage> lpMessage;

		if (GetFreeBusyMessage(m_lpSession, m_lpPublicStore, nullptr, rgfbuser[i].m_cbEid, rgfbuser[i].m_lpEid, false, &~lpMessage) == hrSuccess) {
			object_ptr<ECFreeBusyData> lpECFreeBusyData;
			ECFreeBusyData::Create(&~lpECFreeBusyData);

			fbBlockList.Clear();

			hr = GetFreeBusyMessageData(lpMessage, &rtmStart, &rtmEnd, &fbBlockList);
			if(hr != hrSuccess)
				return hr;

			// Add fbdata
			lpECFreeBusyData->Init(rtmStart, rtmEnd, &fbBlockList);
			hr = lpECFreeBusyData->QueryInterface(IID_IFreeBusyData, (void**)&prgfbdata[i]);
			if(hr != hrSuccess)
				return hr;

			++ulFindUsers;
		}// else No free busy information, gives the empty class
		else
			prgfbdata[i] = NULL;
	}

	if(pcRead)
		*pcRead = ulFindUsers;

	return hr;
}

HRESULT ECFreeBusySupport::LoadFreeBusyUpdate(ULONG cUsers, FBUser *lpUsers, IFreeBusyUpdate **lppFBUpdate, ULONG *lpcFBUpdate, void *lpData4)
{
	HRESULT				hr = hrSuccess;
	ULONG				cFBUpdate = 0;

	if((cUsers > 0 && lpUsers == NULL) || lppFBUpdate == NULL)
		return MAPI_E_INVALID_PARAMETER;

	for (unsigned int i = 0; i < cUsers; ++i) {
		object_ptr<IMessage> lpMessage;

		// Get the FB message, is not exist create them
		hr = GetFreeBusyMessage(m_lpSession, m_lpPublicStore, m_lpUserStore, lpUsers[i].m_cbEid, lpUsers[i].m_lpEid, true, &~lpMessage);
		if (hr != hrSuccess)
		{
			lppFBUpdate[i] = NULL;//FIXME: what todo with this?
			continue;
		}

		object_ptr<ECFreeBusyUpdate> lpECFBUpdate;
		hr = ECFreeBusyUpdate::Create(lpMessage, &~lpECFBUpdate);
		if(hr != hrSuccess)
			return hr;
		hr = lpECFBUpdate->QueryInterface(IID_IFreeBusyUpdate, (void**)&lppFBUpdate[i]);
		if(hr != hrSuccess)
			return hr;
		++cFBUpdate;
	}

	if(lpcFBUpdate)
		*lpcFBUpdate = cFBUpdate;
	return hr;
}

HRESULT ECFreeBusySupport::GetDelegateInfoEx(FBUser sFBUser, unsigned int *lpulStatus, unsigned int *lpulStart, unsigned int *lpulEnd)
{
	HRESULT hr = hrSuccess;
	object_ptr<IFreeBusyData> lpFBData;
	HRESULT ulStatus = 0;
	ULONG ulRead = 0;

	struct StatusOL2K3 {
		ULONG ulResourceType;		/* 0x68410003 PR_SCHDINFO_RESOURCE_TYPE always 0*/
		ULONG ulReserved1;			/* always 1? olk 2007 = 0 */

		char **lppszFullname;		/* PR_MAILBOX_OWNER_NAME, but allocation method is unknown */
		SBinary *lpUserEntryID;		/* PR_MAILBOX_OWNER_ENTRYID, but allocation method is unknown  */
		ULONG *lpUnknown4;			/* pointer to NULL, -- not present in OL2K */
		ULONG ulBossWantsCopy;		/* 0x6842000B PR_SCHDINFO_BOSS_WANTS_COPY always 1 */

		ULONG ulBossWantsInfo;		/* 0x684B000B PR_SCHDINFO_BOSS_WANTS_INFO always 1 */
		ULONG ulDontEmailDelegates;	/* 0x6843000B PR_SCHDINFO_DONT_MAIL_DELEGATES always 1 */

		ULONG fDoesAutoAccept;
		ULONG fDoesRejectRecurring;
		ULONG fDoesRejectConflict;

		ULONG ulReserved11;			/* always 0 -- unknown -- not present in OL2K */
	} *lpStatusOlk2k3;

	struct StatusOL2K7 {
		ULONG ulResourceType;		/* 0x68410003 PR_SCHDINFO_RESOURCE_TYPE always 0*/
		ULONG ulReserved1;			/* always 1? olk 2007 = 0 */

		char **lppszFullname;		/* PR_MAILBOX_OWNER_NAME, but allocation method is unknown */
		SBinary *lpUserEntryID;		/* PR_MAILBOX_OWNER_ENTRYID, but allocation method is unknown  */
		ULONG *lpUnknown4;			/* pointer to NULL, -- not present in OL2K */
		ULONG ulBossWantsCopy;		/* 0x6842000B PR_SCHDINFO_BOSS_WANTS_COPY always 1 */

		ULONG ulBossWantsInfo;		/* 0x684B000B PR_SCHDINFO_BOSS_WANTS_INFO always 1 */
		ULONG ulDontEmailDelegates;	/* 0x6843000B PR_SCHDINFO_DONT_MAIL_DELEGATES always 1 */

		ULONG ulReserved11;			/* always 0 -- unknown -- not present in OL2K */
		ULONG fDoesAutoAccept;
		ULONG fDoesRejectRecurring;
		ULONG fDoesRejectConflict;

	} *lpStatus;

	struct StatusOL2K {
		ULONG ulResourceType;		/* always 0 */
		ULONG ulReserved1;			/* always 1 */

		char **lppszFullname;		/* PR_MAILBOX_OWNER_NAME, but allocation method is unknown */
		SBinary *lpUserEntryID;		/* PR_MAILBOX_OWNER_ENTRYID, but allocation method is unknown  */
		ULONG ulBossWantsCopy;		/* always 1 */

		ULONG ulBossWantsInfo;		/* always 1 */
		ULONG ulDontEmailDelegates;	/* always 1 */

		ULONG fDoesAutoAccept;
		ULONG fDoesRejectRecurring;
		ULONG fDoesRejectConflict;
	} *lpStatusOlk2K;

	bool bAutoAccept = true, bDeclineConflict = true, bDeclineRecurring = true;
	ULONG ulObjType = 0;
	MailUserPtr ptrUser;
	MsgStorePtr ptrStore;
	SPropValuePtr ptrName;

	if (m_lpSession->OpenEntry(sFBUser.m_cbEid, sFBUser.m_lpEid, &iid_of(ptrUser), 0, &ulObjType, &~ptrUser) == hrSuccess &&
	    HrGetOneProp(ptrUser, PR_ACCOUNT, &~ptrName) == hrSuccess &&
	    HrOpenUserMsgStore(m_lpSession, ptrName->Value.LPSZ, &~ptrStore) == hrSuccess)
	{
		GetAutoAcceptSettings(ptrStore, &bAutoAccept, &bDeclineConflict, &bDeclineRecurring);
		// ignore error, default true.
		hr = hrSuccess;
	}

	switch (m_ulOutlookVersion) {
	case CLIENT_VERSION_OLK2000:
	case CLIENT_VERSION_OLK2002:
		lpStatusOlk2K = (StatusOL2K*)lpulStatus;
		memset(lpStatusOlk2K, 0, sizeof(StatusOL2K));

		lpStatusOlk2K->ulReserved1 = 1;
		lpStatusOlk2K->ulBossWantsCopy = 0; // WARNING Outlook will crash if it will be enabled (1)!
		lpStatusOlk2K->ulBossWantsInfo = 1;
		lpStatusOlk2K->ulDontEmailDelegates = 1;

		// They don't seem to have much effect, as outlook will always plan the resource.
		lpStatusOlk2K->fDoesAutoAccept = bAutoAccept;
		lpStatusOlk2K->fDoesRejectConflict = bDeclineConflict;
		lpStatusOlk2K->fDoesRejectRecurring = bDeclineRecurring;

		break;
	case CLIENT_VERSION_OLK2003:
		lpStatusOlk2k3 = (StatusOL2K3*)lpulStatus;
		memset(lpStatusOlk2k3, 0, sizeof(StatusOL2K3));

		lpStatusOlk2k3->ulReserved1 = 0;
		lpStatusOlk2k3->ulBossWantsCopy = 0; // WARNING Outlook will crash if it will be enabled (1)!
		lpStatusOlk2k3->ulBossWantsInfo = 1;
		lpStatusOlk2k3->ulDontEmailDelegates = 1;

		lpStatusOlk2k3->fDoesAutoAccept = bAutoAccept;
		lpStatusOlk2k3->fDoesRejectConflict = bDeclineConflict;
		lpStatusOlk2k3->fDoesRejectRecurring = bDeclineRecurring;

		break;
	default:
		lpStatus = (StatusOL2K7*)lpulStatus;
		memset(lpStatus, 0, sizeof(StatusOL2K7));

		lpStatus->ulReserved1 = 0;
		lpStatus->ulBossWantsCopy = 0; // WARNING Outlook will crash if it will be enabled (1)!
		lpStatus->ulBossWantsInfo = 1;
		lpStatus->ulDontEmailDelegates = 1;

		// Atleast Outlook 2007 should be able to correctly use these, if you restart outlook.
		lpStatus->fDoesAutoAccept = bAutoAccept;
		lpStatus->fDoesRejectConflict = bDeclineConflict;
		lpStatus->fDoesRejectRecurring = bDeclineRecurring;
		break;
	};

	// These two dates seem to be the published range in RTimes for the specified user. However, just specifying zero
	// doesn't seem to matter when booking resources, so it looks like these values are ignored.

	// We'll get the values anyway just to be sure.
	hr = LoadFreeBusyData(1, &sFBUser, &~lpFBData, &ulStatus, &ulRead);
	if(hr != hrSuccess)
		return hr;
	if (ulRead != 1)
		return MAPI_E_NOT_FOUND;
	return lpFBData->GetFBPublishRange((LONG *)lpulStart, (LONG *)lpulEnd);
	// if an error is returned, outlook will send an email to the resource.
	// PR_LAST_VERB_EXECUTED (ulong) will be set to 516, so outlook knows modifications need to be mailed too.
}

} /* namespace */
