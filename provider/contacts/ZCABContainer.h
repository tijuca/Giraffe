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

#ifndef ZCABCONTAINER_H
#define ZCABCONTAINER_H

#include <mapispi.h>
#include <mapidefs.h>
#include "ZCABLogon.h"
#include "ZCABData.h"
#include "ZCMAPIProp.h"

/* should be derived from IMAPIProp, but since we don't do anything with those functions, let's skip the red tape. */
class ZCABContainer : public ECUnknown
{
protected:
	ZCABContainer(std::vector<zcabFolderEntry> *lpFolders, IMAPIFolder *lpContacts, LPMAPISUP lpMAPISup, void* lpProvider, char *szClassName);
	virtual ~ZCABContainer();

private:
	HRESULT MakeWrappedEntryID(ULONG cbEntryID, LPENTRYID lpEntryID, ULONG ulObjType, ULONG ulOffset, ULONG *lpcbEntryID, LPENTRYID *lppEntryID);

public:
	static HRESULT	Create(std::vector<zcabFolderEntry> *lpFolders, IMAPIFolder *lpContacts, LPMAPISUP lpMAPISup, void* lpProvider, ZCABContainer **lppABContainer);
	static HRESULT	Create(IMessage *lpContact, ULONG cbEntryID, LPENTRYID lpEntryID, LPMAPISUP lpMAPISup, ZCABContainer **lppABContainer);

	HRESULT GetFolderContentsTable(ULONG ulFlags, LPMAPITABLE *lppTable);
	HRESULT GetDistListContentsTable(ULONG ulFlags, LPMAPITABLE *lppTable);

	// IUnknown
	virtual HRESULT	QueryInterface(REFIID refiid, void **lppInterface);

	// IABContainer
	virtual HRESULT CreateEntry(ULONG cbEntryID, LPENTRYID lpEntryID, ULONG ulCreateFlags, LPMAPIPROP* lppMAPIPropEntry);
	virtual HRESULT CopyEntries(LPENTRYLIST lpEntries, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, ULONG ulFlags);
	virtual HRESULT DeleteEntries(LPENTRYLIST lpEntries, ULONG ulFlags);
	virtual HRESULT ResolveNames(LPSPropTagArray lpPropTagArray, ULONG ulFlags, LPADRLIST lpAdrList, LPFlagList lpFlagList);

	// From IMAPIContainer
	virtual HRESULT GetContentsTable(ULONG ulFlags, LPMAPITABLE *lppTable);
	virtual HRESULT GetHierarchyTable(ULONG ulFlags, LPMAPITABLE *lppTable);
	virtual HRESULT OpenEntry(ULONG cbEntryID, LPENTRYID lpEntryID, LPCIID lpInterface, ULONG ulFlags, ULONG *lpulObjType, LPUNKNOWN *lppUnk);
	virtual HRESULT SetSearchCriteria(LPSRestriction lpRestriction, LPENTRYLIST lpContainerList, ULONG ulSearchFlags);
	virtual HRESULT GetSearchCriteria(ULONG ulFlags, LPSRestriction *lppRestriction, LPENTRYLIST *lppContainerList, ULONG *lpulSearchState);

	// very limited IMAPIProp, passed to ZCMAPIProp for m_lpDistList.
	virtual HRESULT GetProps(LPSPropTagArray lpPropTagArray, ULONG ulFlags, ULONG FAR * lpcValues, LPSPropValue FAR * lppPropArray);
	virtual HRESULT GetPropList(ULONG ulFlags, LPSPropTagArray FAR * lppPropTagArray);

private:
	class xABContainer : public IABContainer {
		public:
		// From IUnknown
		virtual HRESULT __stdcall QueryInterface(REFIID refiid, void** lppInterface);
		virtual ULONG __stdcall AddRef();
		virtual ULONG __stdcall Release();	

		// From IABContainer and IDistList
		virtual HRESULT __stdcall CreateEntry(ULONG cbEntryID, LPENTRYID lpEntryID, ULONG ulCreateFlags, LPMAPIPROP* lppMAPIPropEntry);
		virtual HRESULT __stdcall CopyEntries(LPENTRYLIST lpEntries, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, ULONG ulFlags);
		virtual HRESULT __stdcall DeleteEntries(LPENTRYLIST lpEntries, ULONG ulFlags);
		virtual HRESULT __stdcall ResolveNames(LPSPropTagArray lpPropTagArray, ULONG ulFlags, LPADRLIST lpAdrList, LPFlagList lpFlagList);
		
		// From IMAPIContainer
		virtual HRESULT __stdcall GetContentsTable(ULONG ulFlags, LPMAPITABLE *lppTable);
		virtual HRESULT __stdcall GetHierarchyTable(ULONG ulFlags, LPMAPITABLE *lppTable);
		virtual HRESULT __stdcall OpenEntry(ULONG cbEntryID, LPENTRYID lpEntryID, LPCIID lpInterface, ULONG ulFlags, ULONG *lpulObjType, LPUNKNOWN *lppUnk);
		virtual HRESULT __stdcall SetSearchCriteria(LPSRestriction lpRestriction, LPENTRYLIST lpContainerList, ULONG ulSearchFlags);
		virtual HRESULT __stdcall GetSearchCriteria(ULONG ulFlags, LPSRestriction *lppRestriction, LPENTRYLIST *lppContainerList, ULONG *lpulSearchState);

		// From IMAPIProp (mostly MAPI_E_NO_SUPPORT)
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
	} m_xABContainer;

private:
	/* reference to ZCABLogon .. ZCABLogon needs to live because of this, so AddChild */
	std::vector<zcabFolderEntry> *m_lpFolders;
	IMAPIFolder *m_lpContactFolder;
	LPMAPISUP m_lpMAPISup;
	void *m_lpProvider;

	/* distlist version of this container */
	IMAPIProp *m_lpDistList;
};

#endif
