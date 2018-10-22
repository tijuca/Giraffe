/*
 * SPDX-License-Identifier: AGPL-3.0-only
 * Copyright 2005 - 2016 Zarafa and its licensors
 */
#ifndef PROVIDERUTIL_H
#define PROVIDERUTIL_H

#include <kopano/memory.hpp>
#include "WSTransport.h"

#define CT_UNSPECIFIED		0x00
#define CT_ONLINE			0x01
#define CT_OFFLINE			0x02

struct PROVIDER_INFO {
	KC::object_ptr<IMSProvider> lpMSProviderOnline;
	KC::object_ptr<IABProvider> lpABProviderOnline;
	ULONG		ulProfileFlags;	//  Profile flags when you start the first time
	ULONG		ulConnectType; // CT_* values, The type of connection when you start the first time
};

typedef std::map<std::string, PROVIDER_INFO> ECMapProvider;

HRESULT CompareStoreIDs(ULONG eid1_size, const ENTRYID *eid1, ULONG eid2_size, const ENTRYID *eid2, ULONG flags, ULONG *result);
extern HRESULT CreateMsgStoreObject(const char *profile, IMAPISupport *, ULONG eid_size, const ENTRYID *eid, ULONG msg_flags, ULONG profile_flags, WSTransport *, const MAPIUID *mdb_prov, BOOL spooler, BOOL deflt_store, BOOL offline_store, ECMsgStore **);
HRESULT SetProviderMode(IMAPISupport *lpMAPISup, ECMapProvider *lpmapProvider, LPCSTR lpszProfileName, ULONG ulConnectType);
HRESULT GetProviders(ECMapProvider *lpmapProvider, IMAPISupport *lpMAPISup, LPCSTR lpszProfileName, ULONG ulFlags, PROVIDER_INFO *lpsProviderInfo);
HRESULT GetTransportToNamedServer(WSTransport *lpTransport, LPCTSTR lpszServerName, ULONG ulFlags, WSTransport **lppTransport);

#endif // #ifndef PROVIDERUTIL_H
