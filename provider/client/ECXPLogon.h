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

#ifndef ECXPLOGON_H
#define ECXPLOGON_H

#include "ECUnknown.h"
#include "IMAPIOffline.h"
#include "pthread.h"
#include <string>

/*typedef struct _MAILBOX_INFO {
	std::string		strFullName;

}MAILBOX_INFO, LPMAILBOX_INFO*;
*/
class ECXPProvider;

class ECXPLogon : public ECUnknown
{
protected:
	ECXPLogon(const std::string &strProfileName, BOOL bOffline, ECXPProvider *lpXPProvider, LPMAPISUP lpMAPISup);
	virtual ~ECXPLogon();

public:
	static  HRESULT Create(const std::string &strProfileName, BOOL bOffline, ECXPProvider *lpXPProvider, LPMAPISUP lpMAPISup, ECXPLogon **lppECXPLogon);

	virtual HRESULT QueryInterface(REFIID refiid, void **lppInterface);

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

	class xXPLogon : public IXPLogon {
		// IUnknown
		virtual ULONG __stdcall AddRef();
		virtual ULONG __stdcall Release();
		virtual HRESULT __stdcall QueryInterface(REFIID refiid, void **lppInterface);

		//IXPLogon
		virtual HRESULT __stdcall AddressTypes(ULONG * lpulFlags, ULONG * lpcAdrType, LPTSTR ** lpppszAdrTypeArray, ULONG * lpcMAPIUID, LPMAPIUID ** lpppUIDArray);
		virtual HRESULT __stdcall RegisterOptions(ULONG * lpulFlags, ULONG * lpcOptions, LPOPTIONDATA * lppOptions);
		virtual HRESULT __stdcall TransportNotify(ULONG * lpulFlags, LPVOID * lppvData);
		virtual HRESULT __stdcall Idle(ULONG ulFlags);
		virtual HRESULT __stdcall TransportLogoff(ULONG ulFlags);
		virtual HRESULT __stdcall SubmitMessage(ULONG ulFlags, LPMESSAGE lpMessage, ULONG * lpulMsgRef, ULONG * lpulReturnParm);
		virtual HRESULT __stdcall EndMessage(ULONG ulMsgRef, ULONG * lpulFlags);
		virtual HRESULT __stdcall Poll(ULONG * lpulIncoming);
		virtual HRESULT __stdcall StartMessage(ULONG ulFlags, LPMESSAGE lpMessage, ULONG * lpulMsgRef);
		virtual HRESULT __stdcall OpenStatusEntry(LPCIID lpInterface, ULONG ulFlags, ULONG * lpulObjType, LPMAPISTATUS * lppEntry);
		virtual HRESULT __stdcall ValidateState(ULONG ulUIParam, ULONG ulFlags);
		virtual HRESULT __stdcall FlushQueues(ULONG ulUIParam, ULONG cbTargetTransport, LPENTRYID lpTargetTransport, ULONG ulFlags);

	} m_xXPLogon;

private:
	class xMAPIAdviseSink : public IMAPIAdviseSink {
	public:
		HRESULT __stdcall QueryInterface(REFIID refiid, void ** lppInterface);
		ULONG __stdcall AddRef();
		ULONG __stdcall Release();

		ULONG __stdcall OnNotify(ULONG cNotif, LPNOTIFICATION lpNotifs);
	} m_xMAPIAdviseSink;

	ULONG OnNotify(ULONG cNotif, LPNOTIFICATION lpNotifs);

	HRESULT HrUpdateTransportStatus();
	HRESULT SetOutgoingProps (LPMESSAGE lpMessage);
	HRESULT ClearOldSubmittedMessages(LPMAPIFOLDER lpFolder);
private:
	LPMAPISUP		m_lpMAPISup;
	LPTSTR			*m_lppszAdrTypeArray;
	ULONG			m_ulTransportStatus;
	ECXPProvider	*m_lpXPProvider;
	bool			m_bCancel;
	pthread_cond_t	m_hExitSignal;
	pthread_mutex_t	m_hExitMutex;
	ULONG			m_cbSubmitEntryID;
	LPENTRYID		m_lpSubmitEntryID;
	ULONG			m_bOffline;
};

#endif // #ifndef ECXPLOGON_H
