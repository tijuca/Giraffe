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
 * Free/busy data for specified users
 *
 * @addtogroup libfreebusy
 * @{
 */

#ifndef ECFREEBUSYSUPPORT_H
#define ECFREEBUSYSUPPORT_H

#include "zcdefs.h"
#include "freebusy.h"
#include "freebusyguid.h"

#include <mapi.h>
#include <mapidefs.h>
#include <mapix.h>

#include "freebusy.h"
#include "freebusyguid.h"

#include "ECUnknown.h"
#include "Trace.h"
#include "ECDebug.h"
#include "ECGuid.h"


#include "ECFBBlockList.h"

/**
 * Implementatie of the IFreeBusySupport interface
 */
class ECFreeBusySupport : public ECUnknown
{
private:
	ECFreeBusySupport(void);
	~ECFreeBusySupport(void);
public:
	static HRESULT Create(ECFreeBusySupport** lppFreeBusySupport);

	// From IUnknown
		virtual HRESULT QueryInterface(REFIID refiid, void **lppInterface);

		// IFreeBusySupport
		virtual HRESULT Open(IMAPISession* lpMAPISession, IMsgStore* lpMsgStore, BOOL bStore);
		virtual HRESULT Close();
		virtual HRESULT LoadFreeBusyData(	ULONG cMax, FBUser *rgfbuser, IFreeBusyData **prgfbdata,
											HRESULT *phrStatus, ULONG *pcRead);

		virtual HRESULT LoadFreeBusyUpdate(ULONG cUsers, FBUser *lpUsers, IFreeBusyUpdate **lppFBUpdate, ULONG *lpcFBUpdate, void *lpData4);
		virtual HRESULT CommitChanges();
		virtual HRESULT GetDelegateInfo(FBUser, void *);
		virtual HRESULT SetDelegateInfo(void *);
		virtual HRESULT AdviseFreeBusy(void *);
		virtual HRESULT Reload(void *);
		virtual HRESULT GetFBDetailSupport(void **, BOOL );
		virtual HRESULT HrHandleServerSched(void *);
		virtual HRESULT HrHandleServerSchedAccess();
		virtual BOOL FShowServerSched(BOOL );
		virtual HRESULT HrDeleteServerSched();
		virtual HRESULT GetFReadOnly(void *);
		virtual HRESULT SetLocalFB(void *);
		virtual HRESULT PrepareForSync();
		virtual HRESULT GetFBPublishMonthRange(void *);
		virtual HRESULT PublishRangeChanged();
		virtual HRESULT CleanTombstone();
		virtual HRESULT GetDelegateInfoEx(FBUser sFBUser, unsigned int *lpulStatus, unsigned int *lpulStart, unsigned int *lpulEnd);
		virtual HRESULT PushDelegateInfoToWorkspace();
		virtual HRESULT Placeholder21(void *, HWND, BOOL );
		virtual HRESULT Placeholder22();

public:
	// Interface voor Outlook 2002 and up
	class xFreeBusySupport _final : public IFreeBusySupport
	{
		public:
			// From IUnknown
			virtual HRESULT __stdcall QueryInterface(REFIID refiid, void **lppInterface) _override;
			virtual ULONG __stdcall AddRef(void) _override;
			virtual ULONG __stdcall Release(void) _override;

			// From IFreeBusySupport
			virtual HRESULT __stdcall Open(IMAPISession* lpMAPISession, IMsgStore* lpMsgStore, BOOL bStore);
			virtual HRESULT __stdcall Close();
			virtual HRESULT __stdcall LoadFreeBusyData(	ULONG cMax, FBUser *rgfbuser, IFreeBusyData **prgfbdata,
												HRESULT *phrStatus, ULONG *pcRead);

			virtual HRESULT __stdcall LoadFreeBusyUpdate(ULONG cUsers, FBUser *lpUsers, IFreeBusyUpdate **lppFBUpdate, ULONG *lpcFBUpdate, void *lpData4);
			virtual HRESULT __stdcall CommitChanges();
			virtual HRESULT __stdcall GetDelegateInfo(FBUser fbUser, void *lpData);
			virtual HRESULT __stdcall SetDelegateInfo(void *lpData);
			virtual HRESULT __stdcall AdviseFreeBusy(void *lpData);
			virtual HRESULT __stdcall Reload(void *lpData);
			virtual HRESULT __stdcall GetFBDetailSupport(void **lppData, BOOL bData);
			virtual HRESULT __stdcall HrHandleServerSched(void *lpData);
			virtual HRESULT __stdcall HrHandleServerSchedAccess();
			virtual BOOL __stdcall FShowServerSched(BOOL bData);
			virtual HRESULT __stdcall HrDeleteServerSched();
			virtual HRESULT __stdcall GetFReadOnly(void *lpData);
			virtual HRESULT __stdcall SetLocalFB(void *lpData);
			virtual HRESULT __stdcall PrepareForSync();
			virtual HRESULT __stdcall GetFBPublishMonthRange(void *lpData);
			virtual HRESULT __stdcall PublishRangeChanged();
			virtual HRESULT __stdcall CleanTombstone();
			virtual HRESULT __stdcall GetDelegateInfoEx(FBUser fbUser, unsigned int *lpData1, unsigned int *lpData2, unsigned int *lpData3);
			virtual HRESULT __stdcall PushDelegateInfoToWorkspace();
			virtual HRESULT __stdcall Placeholder21(void *lpData, HWND hwnd, BOOL bData);
			virtual HRESULT __stdcall Placeholder22();
	} m_xFreeBusySupport;

	// Interface for Outlook 2000
	class xFreeBusySupportOutlook2000 _final : public IFreeBusySupportOutlook2000 {
		public:
			// From IUnknown
			virtual HRESULT __stdcall QueryInterface(REFIID refiid, void **lppInterface) _override;
			virtual ULONG __stdcall AddRef(void) _override;
			virtual ULONG __stdcall Release(void) _override;

			// From IFreeBusySupport
			virtual HRESULT __stdcall Open(IMAPISession* lpMAPISession, IMsgStore* lpMsgStore, BOOL bStore);
			virtual HRESULT __stdcall Close();
			virtual HRESULT __stdcall LoadFreeBusyData(	ULONG cMax, FBUser *rgfbuser, IFreeBusyData **prgfbdata,
												HRESULT *phrStatus, ULONG *pcRead);

			virtual HRESULT __stdcall LoadFreeBusyUpdate(ULONG cUsers, FBUser *lpUsers, IFreeBusyUpdate **lppFBUpdate, ULONG *lpcFBUpdate, void *lpData4);
			virtual HRESULT __stdcall CommitChanges();
			virtual HRESULT __stdcall GetDelegateInfo(FBUser fbUser, void *lpData);
			virtual HRESULT __stdcall SetDelegateInfo(void *lpData);
			virtual HRESULT __stdcall AdviseFreeBusy(void *lpData);
			virtual HRESULT __stdcall Reload(void *lpData);
			virtual HRESULT __stdcall GetFBDetailSupport(void **lppData, BOOL bData);
			virtual HRESULT __stdcall HrHandleServerSched(void *lpData);
			virtual HRESULT __stdcall HrHandleServerSchedAccess();
			virtual BOOL __stdcall FShowServerSched(BOOL bData);
			virtual HRESULT __stdcall HrDeleteServerSched();
			virtual HRESULT __stdcall GetFReadOnly(void *lpData);
			virtual HRESULT __stdcall SetLocalFB(void *lpData);
			virtual HRESULT __stdcall PrepareForSync();
			virtual HRESULT __stdcall GetFBPublishMonthRange(void *lpData);
			virtual HRESULT __stdcall PublishRangeChanged();
			//virtual HRESULT __stdcall CleanTombstone();
			virtual HRESULT __stdcall GetDelegateInfoEx(FBUser fbUser, unsigned int *lpData1, unsigned int *lpData2, unsigned int *lpData3);
			virtual HRESULT __stdcall PushDelegateInfoToWorkspace();
			virtual HRESULT __stdcall Placeholder21(void *lpData, HWND hwnd, BOOL bData);
			virtual HRESULT __stdcall Placeholder22();
	} m_xFreeBusySupportOutlook2000;

private:
	IMAPISession*	m_lpSession;
	IMsgStore*		m_lpPublicStore;
	IMsgStore*		m_lpUserStore;
	IMAPIFolder*	m_lpFreeBusyFolder;
	unsigned int	m_ulOutlookVersion;
};

#endif // ECFREEBUSYSUPPORT_H

/** @} */
