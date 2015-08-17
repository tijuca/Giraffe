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

#ifndef ECEXPORTADDRESSBOOKCHANGES_H
#define ECEXPORTADDRESSBOOKCHANGES_H

#include "zcdefs.h"
#include <set>

#include "ECABContainer.h"

class IECImportAddressbookChanges;
class ECLogger;

class ECExportAddressbookChanges : public ECUnknown {
public:
	ECExportAddressbookChanges(ECMsgStore *lpContainer);
	virtual ~ECExportAddressbookChanges();

    virtual HRESULT QueryInterface(REFIID refiid, void **lppInterface);

	// IECExportAddressbookChanges
	virtual HRESULT	Config(LPSTREAM lpState, ULONG ulFlags, IECImportAddressbookChanges *lpCollector);
	virtual HRESULT Synchronize(ULONG *lpulSteps, ULONG *lpulProgress);
	virtual HRESULT UpdateState(LPSTREAM lpState);

private:
	static bool LeftPrecedesRight(const ICSCHANGE &left, const ICSCHANGE &right);

private:
	class xECExportAddressbookChanges _final : public IECExportAddressbookChanges {
		// IUnknown
		virtual ULONG __stdcall AddRef(void) _override;
		virtual ULONG __stdcall Release(void) _override;
		virtual HRESULT __stdcall QueryInterface(REFIID refiid, void **lppInterface) _override;
		
		// IECExportAddressbookChanges
		virtual HRESULT __stdcall Config(LPSTREAM lpState, ULONG ulFlags, IECImportAddressbookChanges *lpCollector) _override;
		virtual HRESULT __stdcall Synchronize(ULONG *lpulSteps, ULONG *lpulProgress) _override;
		virtual HRESULT __stdcall UpdateState(LPSTREAM lpState) _override;

	} m_xECExportAddressbookChanges;
	
private:
	IECImportAddressbookChanges *m_lpImporter;
	unsigned int				m_ulChangeId;
	ECMsgStore					*m_lpMsgStore;
	unsigned int				m_ulThisChange;
	ULONG						m_ulChanges;
	ULONG						m_ulMaxChangeId;
	ICSCHANGE					*m_lpRawChanges; // Raw data from server
	ICSCHANGE					*m_lpChanges;	 // Same data, but sorted (users, then groups)
	std::set<ULONG>				m_setProcessed;
	ECLogger					*m_lpLogger;
};

#endif
