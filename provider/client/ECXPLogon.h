/*
 * Copyright 2005 - 2016 Zarafa and its licensors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License, version 3,
 * as published by the Free Software Foundation.
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

#ifndef ECXPLOGON_H
#define ECXPLOGON_H

#include <kopano/zcdefs.h>
#include <condition_variable>
#include <mutex>
#include <kopano/ECUnknown.h>
#include "IMAPIOffline.h"
#include <string>

/* struct MAILBOX_INFO {
	std::string		strFullName;
};
typedef struct MAILBOX_INFO LPMAILBOX_INFO*;
*/
class ECXPProvider;

class ECXPLogon _kc_final : public ECUnknown {
protected:
	ECXPLogon(const std::string &strProfileName, BOOL bOffline, ECXPProvider *lpXPProvider, LPMAPISUP lpMAPISup);
	virtual ~ECXPLogon();

public:
	static  HRESULT Create(const std::string &strProfileName, BOOL bOffline, ECXPProvider *lpXPProvider, LPMAPISUP lpMAPISup, ECXPLogon **lppECXPLogon);
	virtual HRESULT QueryInterface(REFIID refiid, void **lppInterface) _kc_override;
	virtual HRESULT AddressTypes(ULONG * lpulFlags, ULONG * lpcAdrType, LPTSTR ** lpppszAdrTypeArray, ULONG * lpcMAPIUID, LPMAPIUID  ** lpppUIDArray);
	virtual HRESULT RegisterOptions(ULONG * lpulFlags, ULONG * lpcOptions, LPOPTIONDATA * lppOptions);
	virtual HRESULT TransportNotify(ULONG * lpulFlags, LPVOID * lppvData);
	virtual HRESULT Idle(ULONG ulFlags);
	virtual HRESULT TransportLogoff(ULONG ulFlags);

	virtual HRESULT SubmitMessage(ULONG ulFlags, LPMESSAGE lpMessage, ULONG * lpulMsgRef, ULONG * lpulReturnParm);
	virtual HRESULT EndMessage(ULONG ulMsgRef, ULONG * lpulFlags);
	virtual HRESULT Poll(ULONG * lpulIncoming);
	virtual HRESULT StartMessage(ULONG ulFlags, LPMESSAGE lpMessage, ULONG * lpulMsgRef);
	virtual HRESULT OpenStatusEntry(LPCIID lpInterface, ULONG ulFlags, ULONG * lpulObjType, LPMAPISTATUS * lppEntry);
	virtual HRESULT ValidateState(ULONG ulUIParam, ULONG ulFlags);
	virtual HRESULT FlushQueues(ULONG ulUIParam, ULONG cbTargetTransport, LPENTRYID lpTargetTransport, ULONG ulFlags);

	class xXPLogon _kc_final : public IXPLogon {
		#include <kopano/xclsfrag/IUnknown.hpp>

		// <kopano/xclsfrag/IXPLogon.hpp>
		virtual HRESULT __stdcall AddressTypes(ULONG *flags, ULONG *lpcAdrType, LPTSTR **lpppszAdrTypeArray, ULONG *lpcMAPIUID, LPMAPIUID **lpppUIDArray) _kc_override;
		virtual HRESULT __stdcall RegisterOptions(ULONG *flags, ULONG *lpcOptions, LPOPTIONDATA *lppOptions) _kc_override;
		virtual HRESULT __stdcall TransportNotify(ULONG *flags, LPVOID *lppvData) _kc_override;
		virtual HRESULT __stdcall Idle(ULONG flags) _kc_override;
		virtual HRESULT __stdcall TransportLogoff(ULONG flags) _kc_override;
		virtual HRESULT __stdcall SubmitMessage(ULONG flags, LPMESSAGE lpMessage, ULONG *lpulMsgRef, ULONG *lpulReturnParm) _kc_override;
		virtual HRESULT __stdcall EndMessage(ULONG ulMsgRef, ULONG *flags) _kc_override;
		virtual HRESULT __stdcall Poll(ULONG *lpulIncoming) _kc_override;
		virtual HRESULT __stdcall StartMessage(ULONG flags, LPMESSAGE lpMessage, ULONG *lpulMsgRef) _kc_override;
		virtual HRESULT __stdcall OpenStatusEntry(LPCIID lpInterface, ULONG flags, ULONG *lpulObjType, LPMAPISTATUS *lppEntry) _kc_override;
		virtual HRESULT __stdcall ValidateState(ULONG ui_param, ULONG flags) _kc_override;
		virtual HRESULT __stdcall FlushQueues(ULONG ui_param, ULONG cbTargetTransport, LPENTRYID lpTargetTransport, ULONG flags) _kc_override;
	} m_xXPLogon;

private:
	class xMAPIAdviseSink _kc_final : public IMAPIAdviseSink {
	public:
		#include <kopano/xclsfrag/IUnknown.hpp>
		// <kopano/xclsfrag/IMAPIAdviseSink.hpp>
		ULONG __stdcall OnNotify(ULONG cNotif, LPNOTIFICATION lpNotifs) _kc_override;
	} m_xMAPIAdviseSink;

	ULONG OnNotify(ULONG cNotif, LPNOTIFICATION lpNotifs);

	HRESULT HrUpdateTransportStatus();
	HRESULT SetOutgoingProps (LPMESSAGE lpMessage);
	HRESULT ClearOldSubmittedMessages(LPMAPIFOLDER lpFolder);
private:
	LPMAPISUP		m_lpMAPISup;
	TCHAR **m_lppszAdrTypeArray = nullptr;
	ULONG m_ulTransportStatus = 0;
	ECXPProvider	*m_lpXPProvider;
	bool m_bCancel = false;
	std::condition_variable m_hExitSignal;
	std::mutex m_hExitMutex;
	ULONG			m_bOffline;
};

#endif // #ifndef ECXPLOGON_H
