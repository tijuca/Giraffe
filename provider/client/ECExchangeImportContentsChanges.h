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

#ifndef ECEXCHANGEIMPORTCONTENTSCHANGES_H
#define ECEXCHANGEIMPORTCONTENTSCHANGES_H

#include <kopano/zcdefs.h>
#include <mapidefs.h>
#include "ECMAPIFolder.h"

#include <kopano/ECUnknown.h>
#include <kopano/IECInterfaces.hpp>

namespace KC {
class ECLogger;
}

class ECExchangeImportContentsChanges _kc_final :
    public ECUnknown, public IECImportContentsChanges {
protected:
	ECExchangeImportContentsChanges(ECMAPIFolder *lpFolder);
public:
	static	HRESULT Create(ECMAPIFolder *lpFolder, LPEXCHANGEIMPORTCONTENTSCHANGES* lppExchangeImportContentsChanges);

	// IUnknown
	virtual HRESULT QueryInterface(REFIID refiid, void **lppInterface) _kc_override;

	// IExchangeImportContentsChanges
	virtual HRESULT GetLastError(HRESULT hResult, ULONG ulFlags, LPMAPIERROR *lppMAPIError);
	virtual HRESULT Config(LPSTREAM lpStream, ULONG ulFlags);
	virtual HRESULT UpdateState(LPSTREAM lpStream);
	virtual HRESULT ImportMessageChange(ULONG cValue, LPSPropValue lpPropArray, ULONG ulFlags, LPMESSAGE * lppMessage);
	virtual HRESULT ImportMessageDeletion(ULONG ulFlags, LPENTRYLIST lpSourceEntryList);
	virtual HRESULT ImportPerUserReadStateChange(ULONG cElements, LPREADSTATE lpReadState);
	virtual HRESULT ImportMessageMove(ULONG cbSourceKeySrcFolder, BYTE *pbSourceKeySrcFolder, ULONG cbSourceKeySrcMessage, BYTE *pbSourceKeySrcMessage, ULONG cbPCLMessage, BYTE *pbPCLMessage, ULONG cbSourceKeyDestMessage, BYTE *pbSourceKeyDestMessage, ULONG cbChangeNumDestMessage, BYTE *pbChangeNumDestMessage);

	// IECImportContentsChanges
	virtual HRESULT ConfigForConversionStream(LPSTREAM lpStream, ULONG ulFlags, ULONG cValuesConversion, LPSPropValue lpPropArrayConversion);
	virtual HRESULT ImportMessageChangeAsAStream(ULONG cValue, LPSPropValue lpPropArray, ULONG ulFlags, LPSTREAM *lppstream);

private:
	bool IsProcessed(const SPropValue *remote_ck, const SPropValue *local_pcl);
	bool IsConflict(const SPropValue *local_ck, const SPropValue *remote_pcl);

	HRESULT CreateConflictMessage(LPMESSAGE lpMessage);
	HRESULT CreateConflictMessageOnly(LPMESSAGE lpMessage, LPSPropValue *lppConflictItems);
	HRESULT CreateConflictFolders();
	HRESULT CreateConflictFolder(LPTSTR lpszName, LPSPropValue lpAdditionalREN, ULONG ulMVPos, LPMAPIFOLDER lpParentFolder, LPMAPIFOLDER * lppConflictFolder);

	HRESULT ImportMessageCreateAsStream(ULONG cValue, LPSPropValue lpPropArray, WSMessageStreamImporter **lppMessageImporter);
	HRESULT ImportMessageUpdateAsStream(ULONG cbEntryId, LPENTRYID lpEntryId, ULONG cValue, LPSPropValue lpPropArray, WSMessageStreamImporter **lppMessageImporter);

	static HRESULT HrUpdateSearchReminders(LPMAPIFOLDER lpRootFolder, const SPropValue *);
	friend class ECExchangeImportHierarchyChanges;

	IStream *m_lpStream = nullptr;
	ULONG m_ulFlags = 0;
	ULONG m_ulSyncId = 0;
	ULONG m_ulChangeId = 0;
	KCHL::memory_ptr<SPropValue> m_lpSourceKey;
	KCHL::object_ptr<ECLogger> m_lpLogger;
	KCHL::object_ptr<ECMAPIFolder> m_lpFolder;
};

#endif // ECEXCHANGEIMPORTCONTENTSCHANGES_H
