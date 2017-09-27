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

#ifndef ECMAPIFOLDER_H
#define ECMAPIFOLDER_H

#include <kopano/zcdefs.h>
#include <mapidefs.h>
#include <kopano/Util.h>
#include "WSTransport.h"
#include "ECMsgStore.h"
#include "ECMAPIContainer.h"

class WSMessageStreamExporter;
class WSMessageStreamImporter;

class ECMAPIFolder :
    public ECMAPIContainer, public IMAPIFolder, public IFolderSupport {
protected:
	ECMAPIFolder(ECMsgStore *lpMsgStore, BOOL fModify, WSMAPIFolderOps *lpFolderOps, const char *szClassName);
	virtual ~ECMAPIFolder();

public:
	static HRESULT Create(ECMsgStore *lpMsgStore, BOOL fModify, WSMAPIFolderOps *lpFolderOps, ECMAPIFolder **lppECMAPIFolder);

	static HRESULT	GetPropHandler(ULONG ulPropTag, void* lpProvider, ULONG ulFlags, LPSPropValue lpsPropValue, void *lpParam, void *lpBase);	
	static HRESULT SetPropHandler(ULONG ulPropTag, void *lpProvider, const SPropValue *lpsPropValue, void *lpParam);

	// Our table-row getprop handler (handles client-side generation of table columns)
	static HRESULT TableRowGetProp(void* lpProvider, struct propVal *lpsPropValSrc, LPSPropValue lpsPropValDst, void **lpBase, ULONG ulType);
	virtual HRESULT	QueryInterface(REFIID refiid, void **lppInterface) _kc_override;
	virtual HRESULT OpenProperty(ULONG ulPropTag, LPCIID lpiid, ULONG ulInterfaceOptions, ULONG ulFlags, LPUNKNOWN *lppUnk);

	// Override IMAPIProp
	virtual HRESULT SaveChanges(ULONG ulFlags) _kc_override;
	virtual HRESULT CopyTo(ULONG ciidExclude, LPCIID rgiidExclude, const SPropTagArray *lpExcludeProps, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, LPCIID lpInterface, LPVOID lpDestObj, ULONG ulFlags, LPSPropProblemArray *lppProblems) _kc_override;
	virtual HRESULT CopyProps(const SPropTagArray *lpIncludeProps, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, LPCIID lpInterface, LPVOID lpDestObj, ULONG ulFlags, LPSPropProblemArray *lppProblems) _kc_override;
	virtual HRESULT GetProps(const SPropTagArray *lpPropTagArray, ULONG ulFlags, ULONG *lpcValues, LPSPropValue *lppPropArray) _kc_override;
	virtual HRESULT SetProps(ULONG cValues, const SPropValue *lpPropArray, LPSPropProblemArray *lppProblems) _kc_override;
	virtual HRESULT DeleteProps(const SPropTagArray *lpPropTagArray, LPSPropProblemArray *lppProblems) _kc_override;

	// We override from IMAPIContainer
	virtual HRESULT SetSearchCriteria(LPSRestriction lpRestriction, LPENTRYLIST lpContainerList, ULONG ulSearchFlags);
	virtual HRESULT GetSearchCriteria(ULONG ulFlags, LPSRestriction *lppRestriction, LPENTRYLIST *lppContainerList, ULONG *lpulSearchState);

	virtual HRESULT CreateMessage(LPCIID lpInterface, ULONG ulFlags, LPMESSAGE *lppMessage);
	virtual HRESULT CreateMessageWithEntryID(LPCIID lpInterface, ULONG ulFlags, ULONG cbEntryID, LPENTRYID lpEntryID, LPMESSAGE *lppMessage);
	virtual HRESULT CopyMessages(LPENTRYLIST lpMsgList, LPCIID lpInterface, LPVOID lpDestFolder, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, ULONG ulFlags);
	virtual HRESULT DeleteMessages(LPENTRYLIST lpMsgList, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, ULONG ulFlags);
	virtual HRESULT CreateFolder(ULONG ulFolderType, LPTSTR lpszFolderName, LPTSTR lpszFolderComment, LPCIID lpInterface, ULONG ulFlags, LPMAPIFOLDER *lppFolder);
	virtual HRESULT CopyFolder(ULONG cbEntryID, LPENTRYID lpEntryID, LPCIID lpInterface, LPVOID lpDestFolder, LPTSTR lpszNewFolderName, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, ULONG ulFlags);
	virtual HRESULT DeleteFolder(ULONG cbEntryID, LPENTRYID lpEntryID, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, ULONG ulFlags);
	virtual HRESULT SetReadFlags(LPENTRYLIST lpMsgList, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, ULONG ulFlags);
	virtual HRESULT GetMessageStatus(ULONG cbEntryID, LPENTRYID lpEntryID, ULONG ulFlags, ULONG *lpulMessageStatus);
	virtual HRESULT SetMessageStatus(ULONG cbEntryID, LPENTRYID lpEntryID, ULONG ulNewStatus, ULONG ulNewStatusMask, ULONG *lpulOldStatus);
	virtual HRESULT SaveContentsSort(const SSortOrderSet *lpSortCriteria, ULONG ulFlags);
	virtual HRESULT EmptyFolder(ULONG ulUIParam, LPMAPIPROGRESS lpProgress, ULONG ulFlags);

	// Override IFolderSupport
	virtual HRESULT GetSupportMask(DWORD * pdwSupportMask);

	// Override genericprops
	virtual HRESULT SetEntryId(ULONG eid_size, const ENTRYID *eid);
	virtual HRESULT HrSetPropStorage(IECPropStorage *lpStorage, BOOL fLoadProps);
	
	// Streaming support
	virtual HRESULT CreateMessageFromStream(ULONG ulFlags, ULONG ulSyncId, ULONG cbEntryID, LPENTRYID lpEntryID, WSMessageStreamImporter **lppsStreamImporter);
	virtual HRESULT GetChangeInfo(ULONG cbEntryID, LPENTRYID lpEntryID, LPSPropValue *lppPropPCL, LPSPropValue *lppPropCK);
	virtual HRESULT UpdateMessageFromStream(ULONG ulSyncId, ULONG cbEntryID, LPENTRYID lpEntryID, LPSPropValue lpConflictItems, WSMessageStreamImporter **lppsStreamImporter);

protected:
	WSMAPIFolderOps	*	lpFolderOps;
	IMAPIAdviseSink *m_lpFolderAdviseSink = nullptr;
	ULONG m_ulConnection = 0;

	friend class		ECExchangeImportHierarchyChanges;	// Allowed access to lpFolderOps
	ALLOC_WRAP_FRIEND;
};



#endif // ECMAPIFOLDER_H
