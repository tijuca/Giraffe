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

/**
 * @file
 * Updates the freebusy data
 *
 * @addtogroup libfreebusy
 * @{
 */

#ifndef ECFREEBUSYUPDATE_H
#define ECFREEBUSYUPDATE_H

#include "zcdefs.h"
#include "freebusy.h"
#include "freebusyguid.h"
#include "ECUnknown.h"
#include "Trace.h"
#include "ECDebug.h"
#include "ECGuid.h"

#include <mapi.h>
#include <mapidefs.h>

#include "ECFBBlockList.h"

/**
 * Implementatie of the IFreeBusyUpdate interface
 */
class ECFreeBusyUpdate : public ECUnknown
{
private:
	ECFreeBusyUpdate(IMessage* lpMessage);
	~ECFreeBusyUpdate(void);
public:
	static HRESULT Create(IMessage* lpMessage, ECFreeBusyUpdate **lppECFreeBusyUpdate);
	
	virtual HRESULT QueryInterface(REFIID refiid, void** lppInterface);

	virtual HRESULT Reload();
	virtual HRESULT PublishFreeBusy(FBBlock_1 *lpBlocks, ULONG nBlocks);
	virtual HRESULT RemoveAppt();
	virtual HRESULT ResetPublishedFreeBusy();
	virtual HRESULT ChangeAppt();
	virtual HRESULT SaveChanges(FILETIME ftStart, FILETIME ftEnd);
	virtual HRESULT GetFBTimes();
	virtual HRESULT Intersect();

public:
	class xFreeBusyUpdate _final : public IFreeBusyUpdate {
		public:
		// From IUnknown
		virtual HRESULT __stdcall QueryInterface(REFIID refiid, void **lppInterface) _override;
		virtual ULONG __stdcall AddRef(void) _override;
		virtual ULONG __stdcall Release(void) _override;

		// From IFreeBusyUpdate
		virtual HRESULT __stdcall Reload();
		virtual HRESULT __stdcall PublishFreeBusy(FBBlock_1 *lpBlocks, ULONG nBlocks);
		virtual HRESULT __stdcall RemoveAppt();
		virtual HRESULT __stdcall ResetPublishedFreeBusy();
		virtual HRESULT __stdcall ChangeAppt();
		virtual HRESULT __stdcall SaveChanges(FILETIME ftBegin, FILETIME ftEnd);
		virtual HRESULT __stdcall GetFBTimes();
		virtual HRESULT __stdcall Intersect();

	}m_xFreeBusyUpdate;

private:
	IMessage*		m_lpMessage; /**< Pointer to the free/busy message received from GetFreeBusyMessage */
	ECFBBlockList	m_fbBlockList; /**< Freebusy time blocks */

};

#endif // ECFREEBUSYUPDATE_H

/** @} */
