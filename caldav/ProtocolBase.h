/*
 * Copyright 2005 - 2015  Zarafa B.V. and its licensors
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License, version 3,
 * as published by the Free Software Foundation with the following
 * additional terms according to sec. 7:
 * 
 * "Zarafa" is a registered trademark of Zarafa B.V.
 * The licensing of the Program under the AGPL does not imply a trademark 
 * license. Therefore any rights, title and interest in our trademarks 
 * remain entirely with us.
 * 
 * Our trademark policy (see TRADEMARKS.txt) allows you to use our trademarks
 * in connection with Propagation and certain other acts regarding the Program.
 * In any case, if you propagate an unmodified version of the Program you are
 * allowed to use the term "Zarafa" to indicate that you distribute the Program.
 * Furthermore you may use our trademarks where it is necessary to indicate the
 * intended purpose of a product or service provided you use it in accordance
 * with honest business practices. For questions please contact Zarafa at
 * trademark@zarafa.com.
 *
 * The interactive user interface of the software displays an attribution 
 * notice containing the term "Zarafa" and/or the logo of Zarafa. 
 * Interactive user interfaces of unmodified and modified versions must 
 * display Appropriate Legal Notices according to sec. 5 of the GNU Affero 
 * General Public License, version 3, when you propagate unmodified or 
 * modified versions of the Program. In accordance with sec. 7 b) of the GNU 
 * Affero General Public License, version 3, these Appropriate Legal Notices 
 * must retain the logo of Zarafa or display the words "Initial Development 
 * by Zarafa" if the display of the logo is not reasonably feasible for
 * technical reasons.
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

#ifndef PROTOCOLBASE_H
#define PROTOCOLBASE_H

#include <mapi.h>
#include "Http.h"

class ProtocolBase
{
public:
	ProtocolBase(Http *lpRequest, IMAPISession *lpSession, ECLogger *lpLogger, std::string strSrvTz, std::string strCharset);
	virtual ~ProtocolBase();

	HRESULT HrInitializeClass();

	virtual HRESULT HrHandleCommand(const std::string &strMethod) = 0;
	
protected:
	Http *m_lpRequest;
	IMAPISession *m_lpSession;
	ECLogger *m_lpLogger;
	std::string m_strSrvTz;
	std::string m_strCharset;

	IMsgStore *m_lpDefStore;		//!< We always need the store of the user that is logged in.
	IMsgStore *m_lpActiveStore;		//!< The store we're acting on
	IAddrBook *m_lpAddrBook;
	IMailUser *m_lpLoginUser;		//!< the logged in user
	IMailUser *m_lpActiveUser;		//!< the owner of m_lpActiveStore
	IMAPIFolder *m_lpUsrFld;		//!< The active folder (calendar, inbox, outbox)
	IMAPIFolder *m_lpIPMSubtree;	//!< IPMSubtree of active store, used for CopyFolder/CreateFolder

	SPropTagArray *m_lpNamedProps; //!< named properties of the active store
	std::wstring m_wstrFldOwner;   //!< url owner part
	std::wstring m_wstrFldName;	   //!< url foldername part

	std::wstring m_wstrUser;	//!< login username (http auth user)

	bool m_blFolderAccess;		//!< can we delete the current folder
	ULONG m_ulUrlFlag;
	ULONG m_ulFolderFlag;

	convert_context m_converter;

	std::string W2U(const std::wstring&); //!< convert widestring to utf-8
	std::string W2U(const WCHAR* lpwWideChar);
	std::wstring U2W(const std::string&); //!< convert utf-8 to widestring
	std::string SPropValToString(SPropValue * lpSprop);
};

#endif
