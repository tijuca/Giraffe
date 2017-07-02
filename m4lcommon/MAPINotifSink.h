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

#ifndef MAPINOTIFSINK_H
#define MAPINOTIFSINK_H

#include <kopano/zcdefs.h>
#include <condition_variable>
#include <list>
#include <mutex>
#include <mapi.h>
#include <mapix.h>
#include <mapidefs.h>
#include <kopano/ECUnknown.h>

namespace KC {

class _kc_export MAPINotifSink _kc_final : public IMAPIAdviseSink {
public:
    static HRESULT Create(MAPINotifSink **lppSink);
	_kc_hidden virtual ULONG __stdcall AddRef(void) _kc_override;
	virtual ULONG __stdcall Release(void) _kc_override;
	_kc_hidden virtual HRESULT __stdcall QueryInterface(REFIID iid, void **iface) _kc_override;
	_kc_hidden virtual ULONG __stdcall OnNotify(ULONG n, LPNOTIFICATION notif) _kc_override;
	virtual HRESULT __stdcall GetNotifications(ULONG *n, LPNOTIFICATION *notif, BOOL fNonBlock, ULONG timeout);

private:
	_kc_hidden MAPINotifSink(void) = default;
	_kc_hidden virtual ~MAPINotifSink(void);

	std::mutex m_hMutex;
	std::condition_variable m_hCond;
    std::list<NOTIFICATION *> m_lstNotifs;
	bool m_bExit = false;
	unsigned int m_cRef = 0;
};


HRESULT MAPICopyUnicode(WCHAR *lpSrc, void *lpBase, WCHAR **lpDst);
HRESULT MAPICopyString(char *lpSrc, void *lpBase, char **lpDst);

} /* namespace */

#endif

