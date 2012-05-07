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

#ifndef PROVIDERUTIL_H
#define PROVIDERUTIL_H

#include "WSTransport.h"

#define CT_UNSPECIFIED		0x00
#define CT_ONLINE			0x01
#define CT_OFFLINE			0x02

typedef struct {
	IMSProvider *lpMSProviderOnline;
	IMSProvider *lpMSProviderOffline;
	IABProvider *lpABProviderOnline;
	IABProvider *lpABProviderOffline;
	ULONG		ulProfileFlags;	//  Profile flags when you start the first time
	ULONG		ulConnectType; // CT_* values, The type of connection when you start the first time
}PROVIDER_INFO;

typedef std::map<std::string, PROVIDER_INFO> ECMapProvider;

HRESULT GetWrappedSupportObject(IMAPISupport* lpMAPISup, LPPROFSECT lpProfileObj, IMAPISupport** lppWrappedMAPISupport);
HRESULT CompareStoreIDs(ULONG cbEntryID1, LPENTRYID lpEntryID1, ULONG cbEntryID2, LPENTRYID lpEntryID2, ULONG ulFlags, ULONG *lpulResult);

HRESULT CreateMsgStoreObject(char *lpszProfname, LPMAPISUP lpMAPISup, ULONG cbEntryID, LPENTRYID lpEntryID, ULONG ulMsgFlags, ULONG ulProfileFlags, WSTransport* lpTransport,
							MAPIUID* lpguidMDBProvider, BOOL bSpooler, BOOL fIsDefaultStore, BOOL bOfflineStore,
							ECMsgStore** lppMsgStore);



HRESULT RemoveAllProviders(ECMapProvider *lpmapProvider);
HRESULT SetProviderMode(IMAPISupport *lpMAPISup, ECMapProvider *lpmapProvider, LPCSTR lpszProfileName, ULONG ulConnectType);
HRESULT GetProviders(ECMapProvider *lpmapProvider, IMAPISupport *lpMAPISup, LPCSTR lpszProfileName, ULONG ulFlags, PROVIDER_INFO *lpsProviderInfo);
HRESULT GetLastConnectionType(IMAPISupport *lpMAPISup, ULONG *lpulType);

HRESULT GetMAPIUniqueProfileId(LPMAPISUP lpMAPISup, tstring *lpstrUniqueId);

HRESULT GetTransportToNamedServer(WSTransport *lpTransport, LPCTSTR lpszServerName, ULONG ulFlags, WSTransport **lppTransport);

#endif // #ifndef PROVIDERUTIL_H
