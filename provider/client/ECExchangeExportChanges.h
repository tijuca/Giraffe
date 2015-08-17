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

#ifndef ECEXCHANGEEXPORTCHANGES_H
#define ECEXCHANGEEXPORTCHANGES_H

#include "zcdefs.h"
#include <mapidefs.h>
#include <vector>
#include <set>
#include <string>

#include "ECICS.h"
#include "ECMAPIProp.h"

#include <ECLogger.h>
#include <ECUnknown.h>
#include <IECExportChanges.h>
#include <IECImportContentsChanges.h>

#include "WSMessageStreamExporter.h"

class ECExchangeExportChanges _final : public ECUnknown {
protected:
	ECExchangeExportChanges(ECMsgStore *lpStore, const std::string& strSK, const wchar_t *szDisplay, unsigned int ulSyncType);
	virtual ~ECExchangeExportChanges();

public:
	static	HRESULT Create(ECMsgStore *lpStore, REFIID iid, const std::string& strSK, const wchar_t *szDisplay, unsigned int ulSyncType, LPEXCHANGEEXPORTCHANGES* lppExchangeExportChanges);

	virtual HRESULT QueryInterface(REFIID refiid, void **lppInterface);

	virtual HRESULT GetLastError(HRESULT hResult, ULONG ulFlags, LPMAPIERROR *lppMAPIError);
	virtual HRESULT Config(LPSTREAM lpStream, ULONG ulFlags, LPUNKNOWN lpCollector, LPSRestriction lpRestriction, LPSPropTagArray lpIncludeProps, LPSPropTagArray lpExcludeProps, ULONG ulBufferSize);
	virtual HRESULT ConfigSelective(ULONG ulPropTag, LPENTRYLIST lpEntries, LPENTRYLIST lpParents, ULONG ulFlags, LPUNKNOWN lpCollector, LPSPropTagArray lpIncludeProps, LPSPropTagArray lpExcludeProps, ULONG ulBufferSize);
	virtual HRESULT Synchronize(ULONG FAR * pulSteps, ULONG FAR * pulProgress);
	virtual HRESULT UpdateState(LPSTREAM lpStream);
	
	virtual HRESULT GetChangeCount(ULONG *lpcChanges);
	virtual HRESULT SetMessageInterface(REFIID refiid);
	virtual HRESULT SetLogger(ECLogger *lpLogger);

private:
	void LogMessageProps(int loglevel, ULONG cValues, LPSPropValue lpPropArray);

private:
	class xECExportChanges _final : public IECExportChanges {
		// IUnknown
		virtual ULONG __stdcall AddRef(void) _override;
		virtual ULONG __stdcall Release(void) _override;
		virtual HRESULT __stdcall QueryInterface(REFIID refiid, void **lppInterface) _override;

		// IExchangeExportChanges
		virtual HRESULT __stdcall GetLastError(HRESULT hResult, ULONG ulFlags, LPMAPIERROR *lppMAPIError);
		virtual HRESULT __stdcall Config(LPSTREAM lpStream, ULONG ulFlags, LPUNKNOWN lpCollector, LPSRestriction lpRestriction, LPSPropTagArray lpIncludeProps, LPSPropTagArray lpExcludeProps, ULONG ulBufferSize);
		virtual HRESULT __stdcall Synchronize(ULONG FAR * pulSteps, ULONG FAR * pulProgress);
		virtual HRESULT __stdcall UpdateState(LPSTREAM lpStream);

		virtual HRESULT __stdcall ConfigSelective(ULONG ulPropTag, LPENTRYLIST lpEntries, LPENTRYLIST lpParents, ULONG ulFlags, LPUNKNOWN lpCollector, LPSPropTagArray lpIncludeProps, LPSPropTagArray lpExcludeProps, ULONG ulBufferSize) _override;
		virtual HRESULT __stdcall GetChangeCount(ULONG *lpcChanges) _override;
		virtual HRESULT __stdcall SetMessageInterface(REFIID refiid) _override;
		virtual HRESULT __stdcall SetLogger(ECLogger *lpLogger) _override;
	} m_xECExportChanges;
	
	HRESULT ExportMessageChanges();
	HRESULT ExportMessageChangesSlow();
	HRESULT ExportMessageChangesFast();
	HRESULT ExportMessageFlags();
	HRESULT ExportMessageDeletes();
	HRESULT ExportFolderChanges();
	HRESULT ExportFolderDeletes();
	HRESULT UpdateStream(LPSTREAM lpStream);
	HRESULT ChangesToEntrylist(std::list<ICSCHANGE> * lpLstChanges, LPENTRYLIST * lppEntryList);

	unsigned long	m_ulSyncType;
	bool			m_bConfiged;
	ECMsgStore*		m_lpStore;
	std::string		m_sourcekey;
	std::wstring	m_strDisplay;
	LPSTREAM		m_lpStream;
	ULONG			m_ulFlags;
	ULONG			m_ulSyncId;
	ULONG			m_ulChangeId;
	ULONG			m_ulStep;
	ULONG			m_ulBatchSize;
	ULONG			m_ulBufferSize;
	ULONG			m_ulEntryPropTag;

	IID				m_iidMessage;

	LPEXCHANGEIMPORTCONTENTSCHANGES		m_lpImportContents;
	LPECIMPORTCONTENTSCHANGES			m_lpImportStreamedContents;
	LPEXCHANGEIMPORTHIERARCHYCHANGES	m_lpImportHierarchy;
	
	WSMessageStreamExporterPtr			m_ptrStreamExporter;
	
	std::vector<ICSCHANGE> m_lstChange;

	typedef std::list<ICSCHANGE>	ChangeList;
	typedef ChangeList::iterator	ChangeListIter;
	ChangeList m_lstFlag;
	ChangeList m_lstSoftDelete;
	ChangeList m_lstHardDelete;

	typedef std::set<std::pair<unsigned int, std::string> > PROCESSEDCHANGESSET;
	
	PROCESSEDCHANGESSET m_setProcessedChanges;

	ICSCHANGE *			m_lpChanges;
	ULONG				m_ulChanges;
	ULONG				m_ulMaxChangeId;
	LPSRestriction		m_lpRestrict;

	ECLogger			*m_lpLogger;
	clock_t				m_clkStart;
	struct z_tms			m_tmsStart;
	
	HRESULT AddProcessedChanges(ChangeList &lstChanges);
	
};

#endif // ECEXCHANGEEXPORTCHANGES_H
