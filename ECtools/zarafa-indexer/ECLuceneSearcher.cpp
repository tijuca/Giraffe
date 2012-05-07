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

#include <platform.h>

#include <algorithm>

#include <mapi.h>
#include <mapix.h>
#include <mapiutil.h>
#include <mapidefs.h>
#include <mapiguid.h>

#include <edkguid.h>

#include <ECGuid.h>
#include <ECTags.h>
#include <IECMultiStoreTable.h>
#include <stringutil.h>
#include <Util.h>
#include <ECIConv.h>

#include "ECFileIndex.h"
#include "ECLucene.h"
#include "ECLuceneAccess.h"
#include "ECLuceneSearcher.h"
#include "zarafa-indexer.h"

#include "charset/convert.h"

ECLuceneSearcher::ECLuceneSearcher(ECThreadData *lpThreadData, ECLuceneAccess *lpLuceneAccess, string_list_t &listFolder, unsigned int ulMaxResults)
{
	m_lpThreadData = lpThreadData;

	m_lpLuceneAccess = lpLuceneAccess;
	m_lpLuceneAccess->AddRef();

	m_listFolder = listFolder;
	m_ulMaxResults = ulMaxResults;
}

ECLuceneSearcher::~ECLuceneSearcher()
{
	if (m_lpLuceneAccess)
		m_lpLuceneAccess->Release();
}

HRESULT ECLuceneSearcher::Create(ECThreadData *lpThreadData, ECLuceneAccess *lpLuceneAccess, string_list_t &listFolder, unsigned int ulMaxResults, ECLuceneSearcher **lppSearcher)
{
	HRESULT hr = hrSuccess;
	ECLuceneSearcher *lpSearcher = NULL;

	try {
		lpSearcher = new ECLuceneSearcher(lpThreadData, lpLuceneAccess, listFolder, ulMaxResults);
	}
	catch (...) {
		lpSearcher = NULL;
	}
	if (!lpSearcher) {
		hr = MAPI_E_NOT_ENOUGH_MEMORY;
		goto exit;
	}

	lpSearcher->AddRef();

	if (lppSearcher)
		*lppSearcher = lpSearcher;

exit:
	if ((hr != hrSuccess) && lpSearcher)
		lpSearcher->Release();

	return hr;
}

HRESULT ECLuceneSearcher::SearchEntries(std::string &strQuery, string_list_t *lplistResults)
{
	HRESULT hr = hrSuccess;
	lucene::search::QueryFilter *lpFilter = NULL;

	hr = CreateQueryFilter(&lpFilter);
	if (hr != hrSuccess)
		goto exit;

	hr = SearchIndexedEntries(strQuery, lpFilter, lplistResults);
	if (hr != hrSuccess)
		goto exit;

exit:
	if (lpFilter)
		delete lpFilter;

	return hr;
}

HRESULT ECLuceneSearcher::CreateQueryFilter(lucene::search::QueryFilter **lppFilter)
{
	HRESULT hr = hrSuccess;
	lucene::analysis::Analyzer *lpAnalyzer = NULL;
	lucene::search::Query *lpQuery = NULL;
	std::wstring strQuery;
	convert_context converter;

	if (!lppFilter) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	if (m_listFolder.empty()) {
		*lppFilter = NULL;
		goto exit;
	}

	hr = m_lpLuceneAccess->GetLuceneAnalyzer(&lpAnalyzer);
	if (hr != hrSuccess)
		goto exit;

	strQuery = FILTER_FIELD;
	strQuery += L":(";
	for (string_list_t::iterator iter = m_listFolder.begin(); iter != m_listFolder.end(); iter++)
		strQuery += L" " + converter.convert_to<std::wstring>(*iter);
	strQuery += L")";

	try {
		lpQuery = lucene::queryParser::QueryParser::parse(strQuery.c_str(), FILTER_FIELD, lpAnalyzer);
		if (!lpQuery) {
			hr = MAPI_E_NOT_ENOUGH_MEMORY;
			goto exit;
		}

		*lppFilter = new lucene::search::QueryFilter(lpQuery);
		if (!(*lppFilter)) {
			hr = MAPI_E_CALL_FAILED;
			goto exit;
		}
	}
	catch (CLuceneError &e) {
		m_lpThreadData->lpLogger->Log(EC_LOGLEVEL_FATAL, "CLucene error: %s", e.what());
		hr = MAPI_E_CALL_FAILED;
		goto exit;
	}
	catch (std::exception &e) {
		m_lpThreadData->lpLogger->Log(EC_LOGLEVEL_FATAL, "STD error during query filter: %s", e.what());
		hr = MAPI_E_CALL_FAILED;
		goto exit;
	}
	catch (...) {
		m_lpThreadData->lpLogger->Log(EC_LOGLEVEL_FATAL, "Unknown error during query filter");
		hr = MAPI_E_CALL_FAILED;
		goto exit;
	}

exit:
	if (lpQuery)
		delete lpQuery;

	return hr;
}

HRESULT ECLuceneSearcher::SearchIndexedEntries(std::string &strQuery, lucene::search::QueryFilter *lpFilter, string_list_t *lplistResults)
{
	HRESULT hr = hrSuccess;
	lucene::analysis::Analyzer *lpAnalyzer = NULL;
	lucene::search::IndexSearcher *lpSearcher = NULL;
	lucene::search::Query *lpQuery = NULL;
	lucene::search::Hits *lpHits = NULL;
	convert_context converter;

	hr = m_lpLuceneAccess->GetLuceneSearcher(&lpSearcher);
	if (hr != hrSuccess)
		goto exit;

	hr = m_lpLuceneAccess->GetLuceneAnalyzer(&lpAnalyzer);
	if (hr != hrSuccess)
		goto exit;

	try {
		lpQuery = lucene::queryParser::QueryParser::parse(converter.convert_to<wstring>(strQuery, rawsize(strQuery), "UTF-8").c_str(), DEFAULT_FIELD, lpAnalyzer);
		if (!lpQuery) {
			hr = MAPI_E_CALL_FAILED;
			goto exit;
		}

		lpHits = lpSearcher->search(lpQuery, lpFilter);
		if (!lpHits) {
			hr = MAPI_E_NOT_FOUND;
			goto exit;
		}

		for (int i = 0; i < lpHits->length(); i++) {
			if(m_ulMaxResults && i >= m_ulMaxResults)
				break;

			lucene::document::Document *lpDoc = &lpHits->doc(i);
			const wchar_t *lpszEntryId = lpDoc->get(UNIQUE_FIELD);

			/* Actually this is a bug, why is the unique field not present? */
			if (!lpszEntryId)
				continue;

			lplistResults->push_back(converter.convert_to<std::string>(lpszEntryId) + " " + stringify_float(lpHits->score(i)));
			
		}
	}
	catch (CLuceneError &e) {
		m_lpThreadData->lpLogger->Log(EC_LOGLEVEL_FATAL, "CLucene error: %s", e.what());
		hr = MAPI_E_CALL_FAILED;
		goto exit;
	}
	catch (std::exception &e) {
		m_lpThreadData->lpLogger->Log(EC_LOGLEVEL_FATAL, "STD error during search: %s", e.what());
		hr = MAPI_E_CALL_FAILED;
		goto exit;
	}
	catch (...) {
		m_lpThreadData->lpLogger->Log(EC_LOGLEVEL_FATAL, "Unknown error during search");
		hr = MAPI_E_CALL_FAILED;
		goto exit;
	}

exit:
	if (lpHits)
		delete lpHits;

	if (lpQuery)
		delete lpQuery;

	if (lpSearcher)
		m_lpLuceneAccess->PutLuceneSearcher(lpSearcher);

	return hr;
}
