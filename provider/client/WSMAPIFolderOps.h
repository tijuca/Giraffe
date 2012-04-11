/*
 * Copyright 2005 - 2009  Zarafa B.V.
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

#ifndef WSMAPIFOLDEROPS_H
#define WSMAPIFOLDEROPS_H

#include "ECUnknown.h"
#include "Zarafa.h"
#include "ZarafaCode.h"
#include "soapZarafaCmdProxy.h"
#include "ECICS.h"

#include <vector>

#include <mapi.h>
#include <mapispi.h>
#include <pthread.h>

class WSTransport;
class utf8string;

class WSMAPIFolderOps : public ECUnknown
{
protected:
	WSMAPIFolderOps(ZarafaCmd *lpCmd, pthread_mutex_t mDataLock, ECSESSIONID ecSessionId, ULONG cbEntryId, LPENTRYID lpEntryId, WSTransport *lpTransport);
	virtual ~WSMAPIFolderOps();

public:
	static HRESULT Create(ZarafaCmd *lpCmd, pthread_mutex_t mDataLock, ECSESSIONID ecSessionId, ULONG cbEntryId, LPENTRYID lpEntryId, WSTransport *lpTransport, WSMAPIFolderOps **lppFolderOps);

	virtual HRESULT QueryInterface(REFIID refiid, void **lppInterface);
	
	// Creates a folder object with only a PR_DISPLAY_NAME and type
	virtual HRESULT HrCreateFolder(ULONG ulFolderType, const utf8string &strFolderName, const utf8string &strComment, BOOL fOpenIfExists, ULONG ulSyncId, LPSBinary lpsSourceKey, ULONG cbNewEntryId, LPENTRYID lpNewEntryId, ULONG* lpcbEntryId, LPENTRYID* lppEntryId);

	// Completely remove a folder, the messages in it, the folders in it or any combination
	virtual HRESULT HrDeleteFolder(ULONG cbEntryId, LPENTRYID lpEntryId, ULONG ulFlags, ULONG ulSyncId);

	// Empty folder (ie delete all folders and messages in folder)
	virtual HRESULT HrEmptyFolder(ULONG ulFlags, ULONG ulSyncId);

	// Set read/unread flags on messages
	virtual HRESULT HrSetReadFlags(ENTRYLIST *lpMsgList, ULONG ulFlags, ULONG ulSyncId);

	// Set / Get search criteria
	virtual HRESULT HrSetSearchCriteria(ENTRYLIST *lpMsgList, SRestriction *lpRestriction, ULONG ulFlags);
	virtual HRESULT HrGetSearchCriteria(ENTRYLIST **lppMsgList, LPSRestriction *lppRestriction, ULONG *lpulFlags);

	// Move or copy a folder
	virtual HRESULT HrCopyFolder(ULONG cbEntryFrom, LPENTRYID lpEntryFrom, ULONG cbEntryDest, LPENTRYID lpEntryDest, const utf8string &strNewFolderName, ULONG ulFlags, ULONG ulSyncId);

	// Move or copy a message
	virtual HRESULT HrCopyMessage(ENTRYLIST *lpMsgList, ULONG cbEntryDest, LPENTRYID lpEntryDest, ULONG ulFlags, ULONG ulSyncId);
	
	// Message status
	virtual HRESULT HrGetMessageStatus(ULONG cbEntryID, LPENTRYID lpEntryID, ULONG ulFlags, ULONG *lpulMessageStatus);
	virtual HRESULT HrSetMessageStatus(ULONG cbEntryID, LPENTRYID lpEntryID, ULONG ulNewStatus, ULONG ulNewStatusMask, ULONG ulSyncId, ULONG *lpulOldStatus);
	
	// Streaming Support
	virtual HRESULT HrGetChangeInfo(ULONG cbEntryID, LPENTRYID lpEntryID, LPSPropValue *lppPropPCL, LPSPropValue *lppPropCK);

	// Reload callback
	static HRESULT Reload(void *lpParam, ECSESSIONID sessionid);

private:
	virtual HRESULT LockSoap();
	virtual HRESULT UnLockSoap();

private:
	entryId			m_sEntryId;		// Entryid of the folder
	ZarafaCmd*		lpCmd;			// command object
	pthread_mutex_t	mDataLock;		//
	ECSESSIONID		ecSessionId;	// Id of the session
	ULONG			m_ulSessionReloadCallback;
	WSTransport *	m_lpTransport;
};

#endif
