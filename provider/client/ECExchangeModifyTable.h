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

#ifndef EC_EXCHANGE_MODIFY_TABLE_H
#define EC_EXCHANGE_MODIFY_TABLE_H

#include "zcdefs.h"
#include "ECUnknown.h"
#include "ECMemTable.h"
#include <mapidefs.h>
#include <edkmdb.h>
#include "IECExchangeModifyTable.h"

class ECExchangeModifyTable : public ECUnknown {
public:
	ECExchangeModifyTable(ULONG ulUniqueTag, ECMemTable *table, ECMAPIProp *lpParent, ULONG ulStartRuleId, ULONG ulFlags);
	virtual ~ECExchangeModifyTable();

	virtual HRESULT QueryInterface(REFIID refiid, void** lppInterface);

	virtual HRESULT __stdcall GetLastError(HRESULT hResult, ULONG ulFlags, LPMAPIERROR *lppMAPIError);
	virtual HRESULT __stdcall GetTable(ULONG ulFlags, LPMAPITABLE *lppTable);
	virtual HRESULT __stdcall ModifyTable(ULONG ulFlags, LPROWLIST lpMods);

	virtual HRESULT __stdcall DisablePushToServer();

	/* static creates */
	static HRESULT __stdcall CreateRulesTable(ECMAPIProp *lpParent, ULONG ulFlags, LPEXCHANGEMODIFYTABLE *lppObj);
	static HRESULT __stdcall CreateACLTable(ECMAPIProp *lpParent, ULONG ulFlags, LPEXCHANGEMODIFYTABLE *lppObj);

	class xExchangeModifyTable _final : public IExchangeModifyTable {
	public:
		// From IUnknown
		virtual HRESULT __stdcall QueryInterface(REFIID refiid, void **lppInterface) _override;
		virtual ULONG __stdcall AddRef(void) _override;
		virtual ULONG __stdcall Release(void) _override;

		// From IExchangeModifyTable
		virtual HRESULT __stdcall GetLastError(HRESULT hResult, ULONG ulFlags, LPMAPIERROR *lppMAPIError);
		virtual HRESULT __stdcall GetTable(ULONG ulFlags, LPMAPITABLE *lppTable);
		virtual HRESULT __stdcall ModifyTable(ULONG ulFlags, LPROWLIST lpMods);
	} m_xExchangeModifyTable;

	class xECExchangeModifyTable _final : public IECExchangeModifyTable {
		// From IUnknown
		virtual HRESULT __stdcall QueryInterface(REFIID refiid, void **lppInterface) _override;
		virtual ULONG __stdcall AddRef(void) _override;
		virtual ULONG __stdcall Release(void) _override;

		// From IExchangeModifyTable
		virtual HRESULT __stdcall GetLastError(HRESULT hResult, ULONG ulFlags, LPMAPIERROR *lppMAPIError) _override;
		virtual HRESULT __stdcall GetTable(ULONG ulFlags, LPMAPITABLE *lppTable) _override;
		virtual HRESULT __stdcall ModifyTable(ULONG ulFlags, LPROWLIST lpMods) _override;

		// From IECExchangeModifyTable
		virtual HRESULT __stdcall DisablePushToServer(void) _override;
	} m_xECExchangeModifyTable;

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
	bool	m_bPushToServer;
};


class ECExchangeRuleAction : public ECUnknown {
public:
	ECExchangeRuleAction();
	virtual ~ECExchangeRuleAction();

	HRESULT __stdcall ActionCount(ULONG *lpcActions);
	HRESULT __stdcall GetAction(ULONG ulActionNumber, LARGE_INTEGER *lpruleid, LPACTION *lppAction);

	class xExchangeRuleAction _final : public IExchangeRuleAction {
	public:
		// From IUnknown
		virtual HRESULT __stdcall QueryInterface(REFIID refiid, void **lppInterface) _override;
		virtual ULONG __stdcall AddRef(void) _override;
		virtual ULONG __stdcall Release(void) _override;

		virtual HRESULT __stdcall ActionCount(ULONG *lpcActions);
		virtual HRESULT __stdcall GetAction(ULONG ulActionNumber, LARGE_INTEGER *lpruleid, LPACTION *lppAction);

	} m_xExchangeRuleAction;
};

#endif
