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

#ifndef ECMAPIFOLDER_H
#define ECMAPIFOLDER_H

#include <mapidefs.h>
#include "WSTransport.h"
#include "ECMsgStore.h"
#include "ECMAPIContainer.h"

class WSMessageStreamExporter;
class WSMessageStreamImporter;

class ECMAPIFolder : public ECMAPIContainer {
protected:
	ECMAPIFolder(ECMsgStore *lpMsgStore, BOOL fModify, WSMAPIFolderOps *lpFolderOps, char *szClassName);
	virtual ~ECMAPIFolder();

public:
	static HRESULT Create(ECMsgStore *lpMsgStore, BOOL fModify, WSMAPIFolderOps *lpFolderOps, ECMAPIFolder **lppECMAPIFolder);

	static HRESULT	GetPropHandler(ULONG ulPropTag, void* lpProvider, ULONG ulFlags, LPSPropValue lpsPropValue, void *lpParam, void *lpBase);	
	static HRESULT	SetPropHandler(ULONG ulPropTag, void* lpProvider, LPSPropValue lpsPropValue, void *lpParam);

	// Our table-row getprop handler (handles client-side generation of table columns)
	static HRESULT TableRowGetProp(void* lpProvider, struct propVal *lpsPropValSrc, LPSPropValue lpsPropValDst, void **lpBase, ULONG ulType);

	virtual HRESULT	QueryInterface(REFIID refiid, void **lppInterface);

	virtual HRESULT OpenProperty(ULONG ulPropTag, LPCIID lpiid, ULONG ulInterfaceOptions, ULONG ulFlags, LPUNKNOWN FAR * lppUnk);

	// Override IMAPIProp
	virtual HRESULT SaveChanges(ULONG ulFlags);
	virtual HRESULT CopyTo(ULONG ciidExclude, LPCIID rgiidExclude, LPSPropTagArray lpExcludeProps, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, LPCIID lpInterface, LPVOID lpDestObj, ULONG ulFlags, LPSPropProblemArray FAR * lppProblems);
	virtual HRESULT CopyProps(LPSPropTagArray lpIncludeProps, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, LPCIID lpInterface, LPVOID lpDestObj, ULONG ulFlags, LPSPropProblemArray FAR * lppProblems);
	virtual HRESULT GetProps(LPSPropTagArray lpPropTagArray, ULONG ulFlags, ULONG FAR * lpcValues, LPSPropValue FAR * lppPropArray);
	virtual HRESULT SetProps(ULONG cValues, LPSPropValue lpPropArray, LPSPropProblemArray FAR * lppProblems);
	virtual HRESULT DeleteProps(LPSPropTagArray lpPropTagArray, LPSPropProblemArray FAR * lppProblems);

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
	virtual HRESULT SaveContentsSort(LPSSortOrderSet lpSortCriteria, ULONG ulFlags);
	virtual HRESULT EmptyFolder(ULONG ulUIParam, LPMAPIPROGRESS lpProgress, ULONG ulFlags);

	// Override IFolderSupport
	virtual HRESULT GetSupportMask(DWORD * pdwSupportMask);

	// Override genericprops
	virtual HRESULT SetEntryId(ULONG cbEntryId, LPENTRYID lpEntryId);
	virtual HRESULT HrSetPropStorage(IECPropStorage *lpStorage, BOOL fLoadProps);
	
	// Streaming support
	virtual HRESULT ExportMessageChangesAsStream(ULONG ulFlags, std::vector<ICSCHANGE> &sChanges, ULONG ulStart, ULONG ulCount, LPSPropTagArray lpsProps, WSMessageStreamExporter **lppsStreamExporter);
	virtual HRESULT CreateMessageFromStream(ULONG ulFlags, ULONG ulSyncId, ULONG cbEntryID, LPENTRYID lpEntryID, WSMessageStreamImporter **lppsStreamImporter);
	virtual HRESULT GetChangeInfo(ULONG cbEntryID, LPENTRYID lpEntryID, LPSPropValue *lppPropPCL, LPSPropValue *lppPropCK);
	virtual HRESULT UpdateMessageFromStream(ULONG ulSyncId, ULONG cbEntryID, LPENTRYID lpEntryID, LPSPropValue lpConflictItems, WSMessageStreamImporter **lppsStreamImporter);

public:
	class xMAPIFolder : public IMAPIFolder {
		public:
		// From IUnknown
		virtual HRESULT __stdcall QueryInterface(REFIID refiid, void** lppInterface);
		virtual ULONG __stdcall AddRef();
		virtual ULONG __stdcall Release();	
		
		// From IMAPIProp
		virtual HRESULT __stdcall GetLastError(HRESULT hError, ULONG ulFlags, LPMAPIERROR * lppMapiError);
		virtual HRESULT __stdcall SaveChanges(ULONG ulFlags);
		virtual HRESULT __stdcall GetProps(LPSPropTagArray lpPropTagArray, ULONG ulFlags, ULONG FAR * lpcValues, LPSPropValue FAR * lppPropArray);
		virtual HRESULT __stdcall GetPropList(ULONG ulFlags, LPSPropTagArray FAR * lppPropTagArray);
		virtual HRESULT __stdcall OpenProperty(ULONG ulPropTag, LPCIID lpiid, ULONG ulInterfaceOptions, ULONG ulFlags, LPUNKNOWN FAR * lppUnk);
		virtual HRESULT __stdcall SetProps(ULONG cValues, LPSPropValue lpPropArray, LPSPropProblemArray FAR * lppProblems);
		virtual HRESULT __stdcall DeleteProps(LPSPropTagArray lpPropTagArray, LPSPropProblemArray FAR * lppProblems);
		virtual HRESULT __stdcall CopyTo(ULONG ciidExclude, LPCIID rgiidExclude, LPSPropTagArray lpExcludeProps, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, LPCIID lpInterface, LPVOID lpDestObj, ULONG ulFlags, LPSPropProblemArray FAR * lppProblems);
		virtual HRESULT __stdcall CopyProps(LPSPropTagArray lpIncludeProps, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, LPCIID lpInterface, LPVOID lpDestObj, ULONG ulFlags, LPSPropProblemArray FAR * lppProblems);
		virtual HRESULT __stdcall GetNamesFromIDs(LPSPropTagArray * pptaga, LPGUID lpguid, ULONG ulFlags, ULONG * pcNames, LPMAPINAMEID ** pppNames);
		virtual HRESULT __stdcall GetIDsFromNames(ULONG cNames, LPMAPINAMEID * ppNames, ULONG ulFlags, LPSPropTagArray * pptaga);

		// From IMAPIContainer
		virtual HRESULT __stdcall GetContentsTable(ULONG ulFlags, LPMAPITABLE *lppTable);
		virtual HRESULT __stdcall GetHierarchyTable(ULONG ulFlags, LPMAPITABLE *lppTable);
		virtual HRESULT __stdcall OpenEntry(ULONG cbEntryID, LPENTRYID lpEntryID, LPCIID lpInterface, ULONG ulFlags, ULONG *lpulObjType, LPUNKNOWN *lppUnk);
		virtual HRESULT __stdcall SetSearchCriteria(LPSRestriction lpRestriction, LPENTRYLIST lpContainerList, ULONG ulSearchFlags);
		virtual HRESULT __stdcall GetSearchCriteria(ULONG ulFlags, LPSRestriction *lppRestriction, LPENTRYLIST *lppContainerList, ULONG *lpulSearchState);

		// From IMAPIFolder
		virtual HRESULT __stdcall CreateMessage(LPCIID lpInterface, ULONG ulFlags, LPMESSAGE *lppMessage);
		virtual HRESULT __stdcall CopyMessages(LPENTRYLIST lpMsgList, LPCIID lpInterface, LPVOID lpDestFolder, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, ULONG ulFlags);
		virtual HRESULT __stdcall DeleteMessages(LPENTRYLIST lpMsgList, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, ULONG ulFlags);
		virtual HRESULT __stdcall CreateFolder(ULONG ulFolderType, LPTSTR lpszFolderName, LPTSTR lpszFolderComment, LPCIID lpInterface, ULONG ulFlags, LPMAPIFOLDER *lppFolder);
		virtual HRESULT __stdcall CopyFolder(ULONG cbEntryID, LPENTRYID lpEntryID, LPCIID lpInterface, LPVOID lpDestFolder, LPTSTR lpszNewFolderName, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, ULONG ulFlags);
		virtual HRESULT __stdcall DeleteFolder(ULONG cbEntryID, LPENTRYID lpEntryID, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, ULONG ulFlags);
		virtual HRESULT __stdcall SetReadFlags(LPENTRYLIST lpMsgList, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, ULONG ulFlags);
		virtual HRESULT __stdcall GetMessageStatus(ULONG cbEntryID, LPENTRYID lpEntryID, ULONG ulFlags, ULONG *lpulMessageStatus);
		virtual HRESULT __stdcall SetMessageStatus(ULONG cbEntryID, LPENTRYID lpEntryID, ULONG ulNewStatus, ULONG ulNewStatusMask, ULONG *lpulOldStatus);
		virtual HRESULT __stdcall SaveContentsSort(LPSSortOrderSet lpSortCriteria, ULONG ulFlags);
		virtual HRESULT __stdcall EmptyFolder(ULONG ulUIParam, LPMAPIPROGRESS lpProgress, ULONG ulFlags);

	} m_xMAPIFolder;

	class xFolderSupport : public IFolderSupport {
		public:
		// From IUnknown
		virtual HRESULT __stdcall QueryInterface(REFIID refiid, void** lppInterface);
		virtual ULONG __stdcall AddRef();
		virtual ULONG __stdcall Release();

		// From IFolderSupport
		virtual HRESULT __stdcall GetSupportMask(DWORD * pdwSupportMask);


	} m_xFolderSupport;

protected:
	WSMAPIFolderOps	*	lpFolderOps;

	LPMAPIADVISESINK	m_lpFolderAdviseSink;
	ULONG				m_ulConnection;

	friend class		ECExchangeImportHierarchyChanges;	// Allowed access to lpFolderOps
};



#endif // ECMAPIFOLDER_H
