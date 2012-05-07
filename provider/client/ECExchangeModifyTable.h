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

#ifndef EC_EXCHANGE_MODIFY_TABLE_H
#define EC_EXCHANGE_MODIFY_TABLE_H

#include "ECUnknown.h"
#include "ECMemTable.h"
#include "mapidefs.h"
#include "edkmdb.h"

class ECExchangeModifyTable : public ECUnknown {
public:
	ECExchangeModifyTable(ULONG ulUniqueTag, ECMemTable *table, ECMAPIProp *lpParent, ULONG ulStartRuleId, ULONG ulFlags);
	virtual ~ECExchangeModifyTable();

	virtual HRESULT QueryInterface(REFIID refiid, void** lppInterface);

	virtual HRESULT __stdcall GetLastError(HRESULT hResult, ULONG ulFlags, LPMAPIERROR *lppMAPIError);
	virtual HRESULT __stdcall GetTable(ULONG ulFlags, LPMAPITABLE *lppTable);
	virtual HRESULT __stdcall ModifyTable(ULONG ulFlags, LPROWLIST lpMods);

	/* static creates */
	static HRESULT __stdcall CreateRulesTable(ECMAPIProp *lpParent, ULONG ulFlags, LPEXCHANGEMODIFYTABLE *lppObj);
	static HRESULT __stdcall CreateACLTable(ECMAPIProp *lpParent, ULONG ulFlags, LPEXCHANGEMODIFYTABLE *lppObj);

	class xExchangeModifyTable : public IExchangeModifyTable {
	public:
		// From IUnknown
		virtual HRESULT __stdcall QueryInterface(REFIID refiid, void** lppInterface);
		virtual ULONG __stdcall AddRef();
		virtual ULONG __stdcall Release();

		// From IExchangeModifyTable
		virtual HRESULT __stdcall GetLastError(HRESULT hResult, ULONG ulFlags, LPMAPIERROR *lppMAPIError);
		virtual HRESULT __stdcall GetTable(ULONG ulFlags, LPMAPITABLE *lppTable);
		virtual HRESULT __stdcall ModifyTable(ULONG ulFlags, LPROWLIST lpMods);
	} m_xExchangeModifyTable;

private:
	static HRESULT HrSerializeTable(ECMemTable *lpTable, char **lppSerialized);
	static HRESULT HrDeserializeTable(char *lpSerialized, ECMemTable *lpTable, ULONG *ulRuleId);

	static HRESULT OpenACLS(ECMAPIProp *lpecMapiProp, ULONG ulFlags, ECMemTable *lpTable, ULONG *lpulUniqueID);
	static HRESULT SaveACLS(ECMAPIProp *lpecMapiProp, ECMemTable *lpTable);

	ULONG	m_ulUniqueId;
	ULONG	m_ulUniqueTag;
	ULONG	m_ulFlags;
	ECMAPIProp *m_lpParent;
	ECMemTable *m_ecTable;
};


class ECExchangeRuleAction : public ECUnknown {
public:
	ECExchangeRuleAction();
	virtual ~ECExchangeRuleAction();

	HRESULT __stdcall ActionCount(ULONG *lpcActions);
	HRESULT __stdcall GetAction(ULONG ulActionNumber, LARGE_INTEGER *lpruleid, LPACTION *lppAction);

	class xExchangeRuleAction : public IExchangeRuleAction {
	public:
		// From IUnknown
		virtual HRESULT __stdcall QueryInterface(REFIID refiid, void** lppInterface);
		virtual ULONG __stdcall AddRef();
		virtual ULONG __stdcall Release();

		virtual HRESULT __stdcall ActionCount(ULONG *lpcActions);
		virtual HRESULT __stdcall GetAction(ULONG ulActionNumber, LARGE_INTEGER *lpruleid, LPACTION *lppAction);

	} m_xExchangeRuleAction;
};

#endif
