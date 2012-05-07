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

#ifndef ICALTOMAPI_H
#define ICALTOMAPI_H

#include <mapidefs.h>
#include <string>
#include <list>

#include "icalitem.h"
#include "icalmapi.h"

#define IC2M_NO_RECIPIENTS	0x0001
#define IC2M_APPEND_ONLY	0x0002
#define IC2M_NO_ORGANIZER	0x0004

class ICALMAPI_API ICalToMapi {
public:
	/*
	    - lpPropObj to lookup named properties
	    - Addressbook (Zarafa Global AddressBook for looking up users)
	 */
	ICalToMapi(IMAPIProp *lpPropObj, LPADRBOOK lpAdrBook, bool bNoRecipients) : m_lpPropObj(lpPropObj), m_lpAdrBook(lpAdrBook), m_bNoRecipients(bNoRecipients) {};
	virtual ~ICalToMapi() {};

	virtual HRESULT ParseICal(const std::string& strIcal, const std::string& strCharset, const std::string& strServerTZ, IMailUser *lpImailUser, ULONG ulFlags) = 0;
	virtual ULONG GetItemCount() = 0;
	virtual HRESULT GetItemInfo(ULONG ulPosition, eIcalType *lpType, time_t *lptLastModified, SBinary *lpUid) = 0;
	virtual HRESULT GetItem(ULONG ulPosition, ULONG ulFlags, LPMESSAGE lpMessage) = 0;
	virtual HRESULT GetItemInfo(time_t *lptstart, time_t *lptend, std::string *lpstrUID, std::list<std::string> **lplstUsers) = 0;

protected:
	LPMAPIPROP m_lpPropObj;
	LPADRBOOK m_lpAdrBook;
	bool m_bNoRecipients;
};

HRESULT ICALMAPI_API CreateICalToMapi(IMAPIProp *lpPropObj, LPADRBOOK lpAdrBook, bool bNoRecipients, ICalToMapi **lppICalToMapi);

#endif
