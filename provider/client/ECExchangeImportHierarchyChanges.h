/*
 * SPDX-License-Identifier: AGPL-3.0-only
 * Copyright 2005 - 2016 Zarafa and its licensors
 */
#ifndef ECEXCHANGEIMPORTCHIERARCHYCHANGES_H
#define ECEXCHANGEIMPORTCHIERARCHYCHANGES_H

#include <mapidefs.h>
#include "ECMAPIFolder.h"
#include <kopano/ECUnknown.h>
#include <kopano/Util.h>
#include <kopano/memory.hpp>

class ECExchangeImportHierarchyChanges final :
    public KC::ECUnknown, public IExchangeImportHierarchyChanges {
protected:
	ECExchangeImportHierarchyChanges(ECMAPIFolder *lpFolder);
public:
	static	HRESULT Create(ECMAPIFolder *lpFolder, LPEXCHANGEIMPORTHIERARCHYCHANGES* lppExchangeImportHierarchyChanges);
	virtual HRESULT QueryInterface(const IID &, void **) override;
	virtual HRESULT GetLastError(HRESULT hResult, ULONG ulFlags, LPMAPIERROR *lppMAPIError);
	virtual HRESULT Config(LPSTREAM lpStream, ULONG ulFlags);
	virtual HRESULT UpdateState(LPSTREAM lpStream);
	virtual HRESULT ImportFolderChange(ULONG cValue, LPSPropValue lpPropArray);
	virtual HRESULT ImportFolderDeletion(ULONG ulFlags, LPENTRYLIST lpSourceEntryList);

private:
	KC::object_ptr<ECMAPIFolder> m_lpFolder;
	IStream *m_lpStream = nullptr;
	unsigned int m_ulFlags = 0, m_ulSyncId = 0, m_ulChangeId = 0;
	ALLOC_WRAP_FRIEND;
};

#endif // ECEXCHANGEIMPORTCHIERARCHYCHANGES_H
