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

#ifndef BASE_H
#define BASE_H

#include <mapi.h>
#include "Http.h"

#include <map>

using namespace std;


class ProtocolBase
{
public:
	ProtocolBase(Http *lpRequest, IMAPISession *lpSession, ECLogger *lpLogger, std::string strSrvTz, std::string strCharset);
	virtual ~ProtocolBase();

	virtual HRESULT HrHandleCommand(const std::string &strMethod);
	
protected:
	Http *m_lpRequest;
	IMAPISession *m_lpSession;
	ECLogger *m_lpLogger;
	IMAPIFolder *m_lpUsrFld;
	IMAPIFolder *m_lpRootFld;
	IMsgStore *m_lpDefStore;
	IMsgStore *m_lpSharedStore;
	IMsgStore *m_lpPubStore;
	IMsgStore *m_lpActiveStore;
	IAddrBook *m_lpAddrBook;
	IMailUser *m_lpImailUser;
	SPropTagArray *m_lpNamedProps;
	std::wstring m_wstrFldName;
	std::wstring m_wstrFldOwner;
	std::string m_strUserEmail;
	std::wstring m_wstrUserFullName;
	std::string m_strCharset;
	std::string m_strSrvTz;
	std::wstring m_wstrUser;
	bool m_blFolderAccess;
	bool m_blIsCLMAC;
	bool m_blIsCLEVO;
	ULONG m_ulUrlFlag;
	ULONG m_ulFolderFlag;
	convert_context m_converter;

	HRESULT HrGetFolder();
	bool CheckFolderAccess(IMsgStore *lpStore, IMAPIFolder *lpRootFolder, IMAPIFolder *lpCurrentFolder) const;
	
	// convert widestring to utf-8
	std::string W2U(std::wstring);
	// convert utf-8 to widestring
	std::wstring U2W(std::string);
	std::string SPropValToString(SPropValue * lpSprop);
	std::wstring SPropValToWString(SPropValue * lpSprop);
};
#endif
