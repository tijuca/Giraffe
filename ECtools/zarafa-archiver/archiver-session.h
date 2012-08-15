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

#ifndef session_INCLUDED
#define session_INCLUDED

#include "archiver-session_fwd.h"
#include "archiver-common.h"
#include "tstring.h"

#include <mapix.h>
#include <mapi_ptr.h>

#include <string>
#include <iostream>
#include <memory>

class ECConfig;
class ECLogger;

/**
 * The Session class wraps the MAPISession an provides commonly used operations. It also
 * checks the license. This way the license doesn't need to be checked all over the place.
 */
class Session
{
public:
	static HRESULT Create(ECConfig *lpConfig, ECLogger *lpLogger, SessionPtr *lpptrSession);
	static HRESULT Create(const MAPISessionPtr &ptrSession, ECLogger *lpLogger, SessionPtr *lpptrSession);
	static HRESULT Create(const MAPISessionPtr &ptrSession, ECConfig *lpConfig, ECLogger *lpLogger, SessionPtr *lpptrSession);
	~Session();
	
	HRESULT OpenStoreByName(const tstring &strUser, LPMDB *lppMsgStore);
	HRESULT OpenStore(const entryid_t &sEntryId, ULONG ulFlags, LPMDB *lppMsgStore);
	HRESULT OpenStore(const entryid_t &sEntryId, LPMDB *lppMsgStore);
	HRESULT OpenReadOnlyStore(const entryid_t &sEntryId, LPMDB *lppMsgStore);
	HRESULT GetUserInfo(const tstring &strUser, abentryid_t *lpsEntryId, tstring *lpstrFullname, bool *bAclCapable);
	HRESULT GetUserInfo(const abentryid_t &sEntryId, tstring *lpstrUser, tstring *lpstrFullname);
	HRESULT GetGAL(LPABCONT *lppAbContainer);
	HRESULT CompareStoreIds(LPMDB lpUserStore, LPMDB lpArchiveStore, bool *lpbResult);
	HRESULT CompareStoreIds(const entryid_t &sEntryId1, const entryid_t &sEntryId2, bool *lpbResult);
	HRESULT ServerIsLocal(const std::string &strServername, bool *lpbResult);
	
	HRESULT CreateRemote(const char *lpszServerPath, ECLogger *lpLogger, SessionPtr *lpptrSession);

	HRESULT OpenMAPIProp(ULONG cbEntryID, LPENTRYID lpEntryID, LPMAPIPROP *lppProp);

	HRESULT OpenOrCreateArchiveStore(const tstring& strUserName, const tstring& strServerName, LPMDB *lppArchiveStore);
	HRESULT GetArchiveStoreEntryId(const tstring& strUserName, const tstring& strServerName, entryid_t *lpArchiveId);

	IMAPISession *GetMAPISession();
	const char *GetSSLPath() const;
	const char *GetSSLPass() const;

private:
	Session(ECLogger *lpLogger);
	HRESULT Init(ECConfig *lpConfig);
	HRESULT Init(const char *lpszServerPath, const char *lpszSslPath, const char *lpszSslPass);
	HRESULT Init(const MAPISessionPtr &ptrSession, const char *lpszSslPath, const char *lpszSslPass);

	HRESULT CreateArchiveStore(const tstring& strUserName, const tstring& strServerName, LPMDB *lppArchiveStore);

private:
	MAPISessionPtr	m_ptrSession;
	MsgStorePtr		m_ptrAdminStore;
	ECLogger		*m_lpLogger;
	
	std::string		m_strSslPath;
	std::string		m_strSslPass;
};

#endif // ndef session_INCLUDED
