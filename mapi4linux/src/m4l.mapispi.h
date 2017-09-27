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

#ifndef __M4L_MAPISPI_IMPL_H
#define __M4L_MAPISPI_IMPL_H

#include <kopano/zcdefs.h>
#include <map>
#include <mutex>
#include "m4l.common.h"
#include "m4l.mapisvc.h"
#include <mapispi.h>
#include <mapix.h>
#include <edkmdb.h>

struct M4LSUPPORTADVISE {
	M4LSUPPORTADVISE(LPNOTIFKEY lpKey, ULONG ulEventMask, ULONG ulFlags, LPMAPIADVISESINK lpAdviseSink)
	{
		this->lpKey = lpKey;
		this->ulEventMask = ulEventMask;
		this->ulFlags = ulFlags;
		this->lpAdviseSink = lpAdviseSink;
	}

	LPNOTIFKEY lpKey;
	ULONG ulEventMask;
	ULONG ulFlags;
	LPMAPIADVISESINK lpAdviseSink;
};

typedef std::map<ULONG, M4LSUPPORTADVISE> M4LSUPPORTADVISES;

struct findKey {
	LPNOTIFKEY m_lpKey;

	findKey(LPNOTIFKEY lpKey) 
	{
		m_lpKey = lpKey;
	}

	bool operator()(const M4LSUPPORTADVISES::value_type &entry) const
	{
		return (entry.second.lpKey->cb == m_lpKey->cb) &&
			   (memcmp(entry.second.lpKey->ab, m_lpKey->ab, m_lpKey->cb) == 0);
	}
};

class M4LMAPIGetSession : public M4LUnknown, public IMAPIGetSession {
private:
	LPMAPISESSION		session;

public:
	M4LMAPIGetSession(LPMAPISESSION new_session);
	virtual ~M4LMAPIGetSession();

	// IMAPIGetSession
	virtual HRESULT GetMAPISession(LPUNKNOWN *lppSession) _kc_override;
	virtual HRESULT QueryInterface(REFIID refiid, void **lpvoid) _kc_override;
};

class M4LMAPISupport : public M4LUnknown, public IMAPISupport {
private:
	LPMAPISESSION		session;
	LPMAPIUID			lpsProviderUID;
	SVCService*			service;
	std::mutex m_advises_mutex;
	M4LSUPPORTADVISES	m_advises;
	ULONG m_connections = 0;

public:
	M4LMAPISupport(LPMAPISESSION new_session, LPMAPIUID sProviderUID, SVCService* lpService);
	virtual ~M4LMAPISupport();
	virtual HRESULT GetLastError(HRESULT hResult, ULONG ulFlags, LPMAPIERROR *lppMAPIError);
	virtual HRESULT GetMemAllocRoutines(LPALLOCATEBUFFER *lpAllocateBuffer, LPALLOCATEMORE *lpAllocateMore, LPFREEBUFFER *lpFreeBuffer);
	virtual HRESULT Subscribe(LPNOTIFKEY lpKey, ULONG ulEventMask, ULONG ulFlags, LPMAPIADVISESINK lpAdviseSink, ULONG *lpulConnection);
	virtual HRESULT Unsubscribe(ULONG ulConnection);
	virtual HRESULT Notify(LPNOTIFKEY lpKey, ULONG cNotification, LPNOTIFICATION lpNotifications, ULONG *lpulFlags);
	virtual HRESULT ModifyStatusRow(ULONG cValues, LPSPropValue lpColumnVals, ULONG ulFlags);
	virtual HRESULT OpenProfileSection(const MAPIUID *uid, ULONG flags, IProfSect **);
	virtual HRESULT RegisterPreprocessor(LPMAPIUID lpMuid, LPTSTR lpszAdrType, LPTSTR lpszDLLName, LPSTR lpszPreprocess, LPSTR lpszRemovePreprocessInfo, ULONG ulFlags);
	virtual HRESULT NewUID(LPMAPIUID lpMuid);
	virtual HRESULT MakeInvalid(ULONG ulFlags, LPVOID lpObject, ULONG ulRefCount, ULONG cMethods);

	virtual HRESULT SpoolerYield(ULONG ulFlags);
	virtual HRESULT SpoolerNotify(ULONG ulFlags, LPVOID lpvData);
	virtual HRESULT CreateOneOff(LPTSTR lpszName, LPTSTR lpszAdrType, LPTSTR lpszAddress, ULONG ulFlags, ULONG *lpcbEntryID, LPENTRYID *lppEntryID);
	virtual HRESULT SetProviderUID(LPMAPIUID lpProviderID, ULONG ulFlags);
	virtual HRESULT CompareEntryIDs(ULONG asize, const ENTRYID *a, ULONG bsize, const ENTRYID *b, ULONG cmp_flags, ULONG *result);
	virtual HRESULT OpenTemplateID(ULONG cbTemplateID, LPENTRYID lpTemplateID, ULONG ulTemplateFlags, LPMAPIPROP lpMAPIPropData, LPCIID lpInterface, LPMAPIPROP *lppMAPIPropNew, LPMAPIPROP lpMAPIPropSibling);
	virtual HRESULT OpenEntry(ULONG eid_size, const ENTRYID *eid, const IID *intf, ULONG flags, ULONG *obj_type, IUnknown **);
	virtual HRESULT GetOneOffTable(ULONG ulFlags, LPMAPITABLE *lppTable);
	virtual HRESULT Address(ULONG *lpulUIParam, LPADRPARM lpAdrParms, LPADRLIST *lppAdrList);
	virtual HRESULT Details(ULONG *lpulUIParam, LPFNDISMISS lpfnDismiss, LPVOID lpvDismissContext, ULONG cbEntryID, LPENTRYID lpEntryID, LPFNBUTTON lpfButtonCallback, LPVOID lpvButtonContext, LPTSTR lpszButtonText, ULONG ulFlags);
	virtual HRESULT NewEntry(ULONG_PTR ulUIParam, ULONG ulFlags, ULONG cbEIDContainer, ENTRYID *lpEIDContainer, ULONG cbEIDNewEntryTpl, ENTRYID *lpEIDNewEntryTpl, ULONG *lpcbEIDNewEntry, ENTRYID **lppEIDNewEntry);
	virtual HRESULT DoConfigPropsheet(ULONG ulUIParam, ULONG ulFlags, LPTSTR lpszTitle, LPMAPITABLE lpDisplayTable, LPMAPIPROP lpCOnfigData, ULONG ulTopPage);
	virtual HRESULT CopyMessages(LPCIID lpSrcInterface, LPVOID lpSrcFolder, LPENTRYLIST lpMsgList, LPCIID lpDestInterface, LPVOID lpDestFolder, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, ULONG ulFlags);
	virtual HRESULT CopyFolder(LPCIID lpSrcInterface, LPVOID lpSrcFolder, ULONG cbEntryID, LPENTRYID lpEntryID, LPCIID lpDestInterface, LPVOID lpDestFolder, LPTSTR lszNewFolderName, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, ULONG ulFlags);
	virtual HRESULT DoCopyTo(LPCIID lpSrcInterface, LPVOID lpSrcObj, ULONG ciidExclude, LPCIID rgiidExclude, const SPropTagArray *lpExcludeProps, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, LPCIID lpDestInterface, LPVOID lpDestObj, ULONG ulFlags, LPSPropProblemArray *lppProblems);
	virtual HRESULT DoCopyProps(LPCIID lpSrcInterface, LPVOID lpSrcObj, const SPropTagArray *lpIncludeProps, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, LPCIID lpDestInterface, LPVOID lpDestObj, ULONG ulFlags, LPSPropProblemArray *lppProblems);
	virtual HRESULT DoProgressDialog(ULONG ulUIParam, ULONG ulFlags, LPMAPIPROGRESS *lppProgress);
	virtual HRESULT ReadReceipt(ULONG ulFlags, LPMESSAGE lpReadMessage, LPMESSAGE *lppEmptyMessage);
	virtual HRESULT PrepareSubmit(LPMESSAGE lpMessage, ULONG *lpulFlags);
	virtual HRESULT ExpandRecips(LPMESSAGE lpMessage, ULONG *lpulFlags);
	virtual HRESULT UpdatePAB(ULONG ulFlags, LPMESSAGE lpMessage);
	virtual HRESULT DoSentMail(ULONG ulFlags, LPMESSAGE lpMessage);
	virtual HRESULT OpenAddressBook(LPCIID lpInterface, ULONG ulFlags, LPADRBOOK *lppAdrBook);
	virtual HRESULT Preprocess(ULONG ulFlags, ULONG cbEntryID, LPENTRYID lpEntryID);
	virtual HRESULT CompleteMsg(ULONG ulFlags, ULONG cbEntryID, LPENTRYID lpEntryID);
	virtual HRESULT StoreLogoffTransports(ULONG *lpulFlags);
	virtual HRESULT StatusRecips(LPMESSAGE lpMessage, LPADRLIST lpRecipList);
	virtual HRESULT WrapStoreEntryID(ULONG cbOrigEntry, const ENTRYID *lpOrigEntry, ULONG *lpcbWrappedEntry, ENTRYID **lppWrappedEntry);
	virtual HRESULT ModifyProfile(ULONG ulFlags);

	virtual HRESULT IStorageFromStream(LPUNKNOWN lpUnkIn, LPCIID lpInterface, ULONG ulFlags, LPSTORAGE *lppStorageOut);
	virtual HRESULT GetSvcConfigSupportObj(ULONG ulFlags, LPMAPISUP *lppSvcSupport);
	virtual HRESULT QueryInterface(REFIID refiid, void **lpvoid) _kc_override;
};


#endif
