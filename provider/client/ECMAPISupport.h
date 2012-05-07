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

#ifndef ECMAPISUPPORT_H
#define ECMAPISUPPORT_H

#include "ECUnknown.h"
//#include <mapitags.h>
//#include <mapidefs.h>
#include <mapispi.h>

class ECMAPISupport : public ECUnknown
{
protected:
	ECMAPISupport(LPMAPISUP	pMAPISupObj, LPPROFSECT lpProfSect);
	~ECMAPISupport(void);

public:
    virtual HRESULT QueryInterface(REFIID refiid, void **lppInterface);
	static HRESULT Create(LPMAPISUP	pMAPISupObj, LPPROFSECT lpProfSect, ECMAPISupport **lppMAPISupport);
public: //IMAPISupport
	virtual HRESULT GetLastError(HRESULT hResult, ULONG ulFlags, LPMAPIERROR* lppMAPIError);
	virtual HRESULT GetMemAllocRoutines(LPALLOCATEBUFFER* lppAllocateBuffer, LPALLOCATEMORE* lppAllocateMore, LPFREEBUFFER* lppFreeBuffer);
	virtual HRESULT Subscribe(LPNOTIFKEY lpKey, ULONG ulEventMask, ULONG ulFlags, LPMAPIADVISESINK lpAdviseSink, ULONG* lpulConnection);
	virtual HRESULT Unsubscribe(ULONG ulConnection);
	virtual HRESULT Notify(LPNOTIFKEY lpKey, ULONG cNotification, LPNOTIFICATION lpNotifications, ULONG* lpulFlags);
	virtual HRESULT ModifyStatusRow(ULONG cValues, LPSPropValue lpColumnVals, ULONG ulFlags);
	virtual HRESULT OpenProfileSection(LPMAPIUID lpUid, ULONG ulFlags, LPPROFSECT* lppProfileObj);
	virtual HRESULT RegisterPreprocessor(LPMAPIUID lpMuid, LPTSTR lpszAdrType, LPTSTR lpszDLLName, LPSTR lpszPreprocess, LPSTR lpszRemovePreprocessInfo, ULONG ulFlags);
	virtual HRESULT NewUID(LPMAPIUID lpMuid);
	virtual HRESULT MakeInvalid(ULONG ulFlags, LPVOID lpObject, ULONG ulRefCount, ULONG cMethods);
	virtual HRESULT SpoolerYield(ULONG ulFlags);
	virtual HRESULT SpoolerNotify(ULONG ulFlags, LPVOID lpvData);
	virtual HRESULT CreateOneOff(LPTSTR lpszName, LPTSTR lpszAdrType, LPTSTR lpszAddress, ULONG ulFlags, ULONG* lpcbEntryID, LPENTRYID* lppEntryID);
	virtual HRESULT SetProviderUID(LPMAPIUID lpProviderID, ULONG ulFlags);
	virtual HRESULT CompareEntryIDs(ULONG cbEntryID1, LPENTRYID lpEntryID1, ULONG cbEntryID2, LPENTRYID lpEntryID2, ULONG ulFlags, ULONG* lpulResult);
	virtual HRESULT OpenTemplateID(ULONG cbTemplateID, LPENTRYID lpTemplateID, ULONG ulTemplateFlags, LPMAPIPROP lpMAPIPropData, LPCIID lpInterface, LPMAPIPROP* lppMAPIPropNew, LPMAPIPROP lpMAPIPropSibling);
	virtual HRESULT OpenEntry(ULONG cbEntryID, LPENTRYID lpEntryID, LPCIID lpInterface, ULONG ulOpenFlags, ULONG* lpulObjType, LPUNKNOWN* lppUnk);
	virtual HRESULT GetOneOffTable(ULONG ulFlags, LPMAPITABLE* lppTable);
	virtual HRESULT Address(ULONG* lpulUIParam, LPADRPARM lpAdrParms, LPADRLIST* lppAdrList);
	virtual HRESULT Details(ULONG* lpulUIParam, LPFNDISMISS lpfnDismiss, LPVOID lpvDismissContext, ULONG cbEntryID, LPENTRYID lpEntryID, LPFNBUTTON lpfButtonCallback, LPVOID lpvButtonContext, LPTSTR lpszButtonText, ULONG ulFlags);
	virtual HRESULT NewEntry(ULONG ulUIParam, ULONG ulFlags, ULONG cbEIDContainer, LPENTRYID lpEIDContainer, ULONG cbEIDNewEntryTpl, LPENTRYID lpEIDNewEntryTpl, ULONG* lpcbEIDNewEntry, LPENTRYID* lppEIDNewEntry);
	virtual HRESULT DoConfigPropsheet(ULONG ulUIParam, ULONG ulFlags, LPTSTR lpszTitle, LPMAPITABLE lpDisplayTable, LPMAPIPROP lpConfigData, ULONG ulTopPage);
	virtual HRESULT CopyMessages(LPCIID lpSrcInterface, LPVOID lpSrcFolder, LPENTRYLIST lpMsgList, LPCIID lpDestInterface, LPVOID lpDestFolder, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, ULONG ulFlags);
	virtual HRESULT CopyFolder(LPCIID lpSrcInterface, LPVOID lpSrcFolder, ULONG cbEntryID, LPENTRYID lpEntryID, LPCIID lpInterface, LPVOID lpDestFolder, LPTSTR lpszNewFolderName, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, ULONG ulFlags);
	virtual HRESULT DoCopyTo(LPCIID lpSrcInterface, LPVOID lpSrcObj, ULONG ciidExclude, LPCIID rgiidExclude, LPSPropTagArray lpExcludeProps, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, LPCIID lpDestInterface, LPVOID lpDestObj, ULONG ulFlags, LPSPropProblemArray* lppProblems);
	virtual HRESULT DoCopyProps(LPCIID lpSrcInterface, LPVOID lpSrcObj, LPSPropTagArray lpIncludeProps, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, LPCIID lpDestInterface, LPVOID lpDestObj, ULONG ulFlags, LPSPropProblemArray* lppProblems);
	virtual HRESULT DoProgressDialog(ULONG ulUIParam, ULONG ulFlags, LPMAPIPROGRESS* lppProgress);
	virtual HRESULT ReadReceipt(ULONG ulFlags, LPMESSAGE lpReadMessage, LPMESSAGE* lppEmptyMessage);
	virtual HRESULT PrepareSubmit(LPMESSAGE lpMessage, ULONG* lpulFlags);
	virtual HRESULT ExpandRecips(LPMESSAGE lpMessage, ULONG* lpulFlags);
	virtual HRESULT UpdatePAB(ULONG ulFlags, LPMESSAGE lpMessage);
	virtual HRESULT DoSentMail(ULONG ulFlags, LPMESSAGE lpMessage);
	virtual HRESULT OpenAddressBook(LPCIID lpInterface, ULONG ulFlags, LPADRBOOK* lppAdrBook);
	virtual HRESULT Preprocess(ULONG ulFlags, ULONG cbEntryID, LPENTRYID lpEntryID);
	virtual HRESULT CompleteMsg(ULONG ulFlags, ULONG cbEntryID, LPENTRYID lpEntryID);
	virtual HRESULT StoreLogoffTransports(ULONG* lpulFlags);
	virtual HRESULT StatusRecips(LPMESSAGE lpMessage, LPADRLIST lpRecipList);
	virtual HRESULT WrapStoreEntryID(ULONG cbOrigEntry, LPENTRYID lpOrigEntry, ULONG* lpcbWrappedEntry, LPENTRYID* lppWrappedEntry);
	virtual HRESULT ModifyProfile(ULONG ulFlags);
	virtual HRESULT IStorageFromStream(LPUNKNOWN lpUnkIn, LPCIID lpInterface, ULONG ulFlags, LPSTORAGE* lppStorageOut);
	virtual HRESULT GetSvcConfigSupportObj(ULONG ulFlags, LPMAPISUP* lppSvcSupport);

public:
	class xMAPISupport : public IMAPISupport {
		// IUnknown
		virtual ULONG __stdcall AddRef();
		virtual ULONG __stdcall Release();
		virtual HRESULT __stdcall QueryInterface(REFIID refiid, void **lppInterface);

		// IMAPISupport
		virtual HRESULT __stdcall GetLastError(HRESULT hResult, ULONG ulFlags, LPMAPIERROR* lppMAPIError);
		virtual HRESULT __stdcall GetMemAllocRoutines(LPALLOCATEBUFFER* lppAllocateBuffer, LPALLOCATEMORE* lppAllocateMore, LPFREEBUFFER* lppFreeBuffer);
		virtual HRESULT __stdcall Subscribe(LPNOTIFKEY lpKey, ULONG ulEventMask, ULONG ulFlags, LPMAPIADVISESINK lpAdviseSink, ULONG* lpulConnection);
		virtual HRESULT __stdcall Unsubscribe(ULONG ulConnection);
		virtual HRESULT __stdcall Notify(LPNOTIFKEY lpKey, ULONG cNotification, LPNOTIFICATION lpNotifications, ULONG* lpulFlags);
		virtual HRESULT __stdcall ModifyStatusRow(ULONG cValues, LPSPropValue lpColumnVals, ULONG ulFlags);
		virtual HRESULT __stdcall OpenProfileSection(LPMAPIUID lpUid, ULONG ulFlags, LPPROFSECT* lppProfileObj);
		virtual HRESULT __stdcall RegisterPreprocessor(LPMAPIUID lpMuid, LPTSTR lpszAdrType, LPTSTR lpszDLLName, LPSTR lpszPreprocess, LPSTR lpszRemovePreprocessInfo, ULONG ulFlags);
		virtual HRESULT __stdcall NewUID(LPMAPIUID lpMuid);
		virtual HRESULT __stdcall MakeInvalid(ULONG ulFlags, LPVOID lpObject, ULONG ulRefCount, ULONG cMethods);
		virtual HRESULT __stdcall SpoolerYield(ULONG ulFlags);
		virtual HRESULT __stdcall SpoolerNotify(ULONG ulFlags, LPVOID lpvData);
		virtual HRESULT __stdcall CreateOneOff(LPTSTR lpszName, LPTSTR lpszAdrType, LPTSTR lpszAddress, ULONG ulFlags, ULONG* lpcbEntryID, LPENTRYID* lppEntryID);
		virtual HRESULT __stdcall SetProviderUID(LPMAPIUID lpProviderID, ULONG ulFlags);
		virtual HRESULT __stdcall CompareEntryIDs(ULONG cbEntryID1, LPENTRYID lpEntryID1, ULONG cbEntryID2, LPENTRYID lpEntryID2, ULONG ulFlags, ULONG* lpulResult);
		virtual HRESULT __stdcall OpenTemplateID(ULONG cbTemplateID, LPENTRYID lpTemplateID, ULONG ulTemplateFlags, LPMAPIPROP lpMAPIPropData, LPCIID lpInterface, LPMAPIPROP* lppMAPIPropNew, LPMAPIPROP lpMAPIPropSibling);
		virtual HRESULT __stdcall OpenEntry(ULONG cbEntryID, LPENTRYID lpEntryID, LPCIID lpInterface, ULONG ulOpenFlags, ULONG* lpulObjType, LPUNKNOWN* lppUnk);
		virtual HRESULT __stdcall GetOneOffTable(ULONG ulFlags, LPMAPITABLE* lppTable);
		virtual HRESULT __stdcall Address(ULONG* lpulUIParam, LPADRPARM lpAdrParms, LPADRLIST* lppAdrList);
		virtual HRESULT __stdcall Details(ULONG* lpulUIParam, LPFNDISMISS lpfnDismiss, LPVOID lpvDismissContext, ULONG cbEntryID, LPENTRYID lpEntryID, LPFNBUTTON lpfButtonCallback, LPVOID lpvButtonContext, LPTSTR lpszButtonText, ULONG ulFlags);
		virtual HRESULT __stdcall NewEntry(ULONG ulUIParam, ULONG ulFlags, ULONG cbEIDContainer, LPENTRYID lpEIDContainer, ULONG cbEIDNewEntryTpl, LPENTRYID lpEIDNewEntryTpl, ULONG* lpcbEIDNewEntry, LPENTRYID* lppEIDNewEntry);
		virtual HRESULT __stdcall DoConfigPropsheet(ULONG ulUIParam, ULONG ulFlags, LPTSTR lpszTitle, LPMAPITABLE lpDisplayTable, LPMAPIPROP lpConfigData, ULONG ulTopPage);
		virtual HRESULT __stdcall CopyMessages(LPCIID lpSrcInterface, LPVOID lpSrcFolder, LPENTRYLIST lpMsgList, LPCIID lpDestInterface, LPVOID lpDestFolder, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, ULONG ulFlags);
		virtual HRESULT __stdcall CopyFolder(LPCIID lpSrcInterface, LPVOID lpSrcFolder, ULONG cbEntryID, LPENTRYID lpEntryID, LPCIID lpInterface, LPVOID lpDestFolder, LPTSTR lpszNewFolderName, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, ULONG ulFlags);
		virtual HRESULT __stdcall DoCopyTo(LPCIID lpSrcInterface, LPVOID lpSrcObj, ULONG ciidExclude, LPCIID rgiidExclude, LPSPropTagArray lpExcludeProps, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, LPCIID lpDestInterface, LPVOID lpDestObj, ULONG ulFlags, LPSPropProblemArray* lppProblems);
		virtual HRESULT __stdcall DoCopyProps(LPCIID lpSrcInterface, LPVOID lpSrcObj, LPSPropTagArray lpIncludeProps, ULONG ulUIParam, LPMAPIPROGRESS lpProgress, LPCIID lpDestInterface, LPVOID lpDestObj, ULONG ulFlags, LPSPropProblemArray* lppProblems);
		virtual HRESULT __stdcall DoProgressDialog(ULONG ulUIParam, ULONG ulFlags, LPMAPIPROGRESS* lppProgress);
		virtual HRESULT __stdcall ReadReceipt(ULONG ulFlags, LPMESSAGE lpReadMessage, LPMESSAGE* lppEmptyMessage);
		virtual HRESULT __stdcall PrepareSubmit(LPMESSAGE lpMessage, ULONG* lpulFlags);
		virtual HRESULT __stdcall ExpandRecips(LPMESSAGE lpMessage, ULONG* lpulFlags);
		virtual HRESULT __stdcall UpdatePAB(ULONG ulFlags, LPMESSAGE lpMessage);
		virtual HRESULT __stdcall DoSentMail(ULONG ulFlags, LPMESSAGE lpMessage);
		virtual HRESULT __stdcall OpenAddressBook(LPCIID lpInterface, ULONG ulFlags, LPADRBOOK* lppAdrBook);
		virtual HRESULT __stdcall Preprocess(ULONG ulFlags, ULONG cbEntryID, LPENTRYID lpEntryID);
		virtual HRESULT __stdcall CompleteMsg(ULONG ulFlags, ULONG cbEntryID, LPENTRYID lpEntryID);
		virtual HRESULT __stdcall StoreLogoffTransports(ULONG* lpulFlags);
		virtual HRESULT __stdcall StatusRecips(LPMESSAGE lpMessage, LPADRLIST lpRecipList);
		virtual HRESULT __stdcall WrapStoreEntryID(ULONG cbOrigEntry, LPENTRYID lpOrigEntry, ULONG* lpcbWrappedEntry, LPENTRYID* lppWrappedEntry);
		virtual HRESULT __stdcall ModifyProfile(ULONG ulFlags);
		virtual HRESULT __stdcall IStorageFromStream(LPUNKNOWN lpUnkIn, LPCIID lpInterface, ULONG ulFlags, LPSTORAGE* lppStorageOut);
		virtual HRESULT __stdcall GetSvcConfigSupportObj(ULONG ulFlags, LPMAPISUP* lppSvcSupport);

 	} m_xMAPISupport;

private:
	LPMAPISUP	m_lpMAPISupObj;
	//LPPROFSECT	m_lpProfSect;
};

#endif
