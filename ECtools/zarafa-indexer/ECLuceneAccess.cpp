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

#include "ECFileIndex.h"
#include "ECLucene.h"
#include "ECLuceneAccess.h"
#include "ECLuceneIndexer.h"
#include "ECLuceneSearcher.h"
#include "ECAnalyzers.h"
#include "zarafa-indexer.h"

ECLuceneAccess::ECLuceneAccess(ECThreadData *lpThreadData, std::string &strStorePath)
{
	m_lpThreadData = lpThreadData;
	m_lpThreadData->lpFileIndex->GetStoreIndexPath(strStorePath, &m_strIndexStore);
	/* Don't really care about returncode from GetStoreIndexPath */

	pthread_mutexattr_init(&m_hLockAttr);
	pthread_mutexattr_settype(&m_hLockAttr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&m_hResourceLock, &m_hLockAttr);
	pthread_mutex_init(&m_hWriteLock, NULL);
	pthread_mutex_init(&m_hSearchLock, NULL);

	m_lpAnalyzer = NULL;
	m_lpWriter = NULL;
	m_lpReader = NULL;
	m_lpSearcher = NULL;

	m_ulLastSearch = 0;
	m_ulLastWrite = 0;
}

ECLuceneAccess::~ECLuceneAccess()
{
	CloseLuceneWriter();
	CloseLuceneReader();
	CloseLuceneSearcher();
	CloseLuceneAnalyzer();

	pthread_mutex_destroy(&m_hSearchLock);
	pthread_mutex_destroy(&m_hWriteLock);
	pthread_mutex_destroy(&m_hResourceLock);
	pthread_mutexattr_destroy(&m_hLockAttr);
}

HRESULT ECLuceneAccess::Create(ECThreadData *lpThreadData, std::string &strStorePath, ECLuceneAccess **lppAccess)
{
	HRESULT hr = hrSuccess;
	ECLuceneAccess *lpAccess = NULL;

	try {
		lpAccess = new ECLuceneAccess(lpThreadData, strStorePath);
	}
	catch (...) {
		lpAccess = NULL;
	}
	if (!lpAccess) {
		hr = MAPI_E_NOT_ENOUGH_MEMORY;
		goto exit;
	}

	lpAccess->AddRef();

	if (lppAccess)
		*lppAccess = lpAccess;

exit:
	if ((hr != hrSuccess) && lpAccess)
		lpAccess->Release();

	return hr;
}

VOID ECLuceneAccess::Optimize(time_t ulExpireTime)
{
	pthread_mutex_lock(&m_hSearchLock);
	pthread_mutex_lock(&m_hWriteLock);

	if (ulExpireTime > m_ulLastSearch) {
		CloseLuceneSearcher();
	}

	if (ulExpireTime > m_ulLastWrite) {
		CloseLuceneWriter();
		CloseLuceneReader();
	}

	pthread_mutex_unlock(&m_hWriteLock);
	pthread_mutex_unlock(&m_hSearchLock);
}

BOOL ECLuceneAccess::IndexExists()
{
	return ECLuceneAccess::IndexExists(m_strIndexStore);
}

BOOL ECLuceneAccess::IndexExists(std::string &strIndexStore)
{
	BOOL ret = FALSE;
	try {
		ret = lucene::index::IndexReader::indexExists(strIndexStore.c_str());
	}
	catch (...) {
	}
	return ret;
}

HRESULT ECLuceneAccess::GetLuceneAnalyzer(lucene::analysis::Analyzer **lppAnalyzer)
{
	HRESULT hr = hrSuccess;

	pthread_mutex_lock(&m_hResourceLock);

	if (!m_lpAnalyzer) {
		hr = OpenLuceneAnalyzer();
		if (hr != hrSuccess)
			goto exit;
	}

	*lppAnalyzer = m_lpAnalyzer;

exit:
	pthread_mutex_unlock(&m_hResourceLock);

	return hr;
}

HRESULT ECLuceneAccess::GetLuceneWriter(lucene::index::IndexWriter **lppWriter)
{
	HRESULT hr = hrSuccess;

	if (pthread_mutex_trylock(&m_hWriteLock)) {
		hr = MAPI_E_BUSY;
		goto exit;
	}

	if (!m_lpWriter) {
		hr = OpenLuceneWriter();
		if (hr != hrSuccess)
			goto exit;
	}

	*lppWriter = m_lpWriter;

exit:
	/* NOTE: Writer is unlocked in PutLuceneWriter */
	if (hr != hrSuccess)
		pthread_mutex_unlock(&m_hWriteLock);

	return hr;
}

VOID ECLuceneAccess::PutLuceneWriter(lucene::index::IndexWriter *lpWriter)
{
	/* Force Lucene Searcher to refresh cache */
	m_ulLastWrite = time(NULL);
	pthread_mutex_unlock(&m_hWriteLock);
}

HRESULT ECLuceneAccess::GetLuceneReader(lucene::index::IndexReader **lppReader)
{
	HRESULT hr = hrSuccess;

	if (pthread_mutex_trylock(&m_hWriteLock)) {
		hr = MAPI_E_BUSY;
		goto exit;
	}

	/* Lucene Writer must not be open when deleting files */
	CloseLuceneWriter();

	if (!m_lpReader) {
		hr = OpenLuceneReader();
		if (hr != hrSuccess)
			goto exit;
	}

	 *lppReader = m_lpReader;

exit:
	/* NOTE: Writer is unlocked in PutLuceneReader */
	if (hr != hrSuccess)
		pthread_mutex_unlock(&m_hWriteLock);

	return hr;
}

VOID ECLuceneAccess::PutLuceneReader(lucene::index::IndexReader *lpReader, bool bClose)
{
	/* Force Lucene Searcher to refresh cache */
	m_ulLastWrite = time(NULL);
	pthread_mutex_unlock(&m_hWriteLock);
	
	if(bClose)
		CloseLuceneReader();
}

HRESULT ECLuceneAccess::GetLuceneSearcher(lucene::search::IndexSearcher **lppSearcher)
{
	HRESULT hr = hrSuccess;

	pthread_mutex_lock(&m_hSearchLock);

	/* Close Reader/Writer to make sure everything has been flushed to index */ 
	if (pthread_mutex_trylock(&m_hWriteLock) == 0) { 
		CloseLuceneWriter(); 
		CloseLuceneReader(); 
		pthread_mutex_unlock(&m_hWriteLock); 
	}

	/* Check if the index has been updated by the writer, if so reopen searcher */
	if (m_ulLastWrite > m_ulLastSearch)
		CloseLuceneSearcher();

	if (!m_lpSearcher) {
		hr = OpenLuceneSearcher();
		if (hr != hrSuccess)
			goto exit;
	}

	*lppSearcher = m_lpSearcher;

exit:
	/* NOTE: Searcher is unlocked in PutLuceneSearcher */
	if (hr != hrSuccess)
		pthread_mutex_unlock(&m_hSearchLock);

	return hr;
}

VOID ECLuceneAccess::PutLuceneSearcher(lucene::search::IndexSearcher *lpSearcher)
{
	m_ulLastSearch = time(NULL);
	pthread_mutex_unlock(&m_hSearchLock);
}

HRESULT ECLuceneAccess::OpenLuceneAnalyzer()
{
	HRESULT hr = hrSuccess;

	pthread_mutex_lock(&m_hResourceLock);

	if (!m_lpAnalyzer) {
		try {
			m_lpAnalyzer = new ECAnalyzer();
		}
		catch (...) {
			m_lpAnalyzer = NULL;
		}
		if (!m_lpAnalyzer) {
			hr = MAPI_E_NOT_ENOUGH_MEMORY;
			goto exit;
		}
	}

exit:
	pthread_mutex_unlock(&m_hResourceLock);

	return hr;
}

VOID ECLuceneAccess::CloseLuceneAnalyzer()
{
	pthread_mutex_lock(&m_hResourceLock);

	if (m_lpAnalyzer) {
		delete m_lpAnalyzer;
		m_lpAnalyzer = NULL;
	}

	pthread_mutex_unlock(&m_hResourceLock);
}

HRESULT ECLuceneAccess::OpenLuceneWriter()
{
	HRESULT hr = hrSuccess;
	lucene::analysis::Analyzer *lpAnalyzer = NULL;
	const char *lpszTmp = NULL;
	ULONG ulTmp = 0;

	pthread_mutex_lock(&m_hResourceLock);

	hr = GetLuceneAnalyzer(&lpAnalyzer);
	if (hr != hrSuccess)
		goto exit;

	try {
		m_lpWriter = new lucene::index::IndexWriter(m_strIndexStore.c_str(), lpAnalyzer, !IndexExists());

		lpszTmp = m_lpThreadData->lpConfig->GetSetting("index_max_field_length");
		ulTmp = lpszTmp ? atoui(lpszTmp) : 0;
		if (!ulTmp)
			ulTmp = lucene::index::IndexWriter::DEFAULT_MAX_FIELD_LENGTH;
		m_lpWriter->setMaxFieldLength(ulTmp);

		lpszTmp = m_lpThreadData->lpConfig->GetSetting("index_merge_factor");
		ulTmp = lpszTmp ? atoui(lpszTmp) : 0;
		if (!ulTmp)
			ulTmp = lucene::index::IndexWriter::DEFAULT_MERGE_FACTOR;
		if (ulTmp < 2)
			ulTmp = 2;
		m_lpWriter->setMergeFactor(ulTmp);

		lpszTmp = m_lpThreadData->lpConfig->GetSetting("index_max_buffered_docs");
		ulTmp = lpszTmp ? atoui(lpszTmp) : 0;
		if (!ulTmp)
			ulTmp = lucene::index::IndexWriter::DEFAULT_MAX_BUFFERED_DOCS;
		if (ulTmp < 2)
			ulTmp = 2;
		m_lpWriter->setMaxBufferedDocs(ulTmp);

		lpszTmp = m_lpThreadData->lpConfig->GetSetting("index_min_merge_docs");
		ulTmp = lpszTmp ? atoui(lpszTmp) : 0;
		if (!ulTmp)
			ulTmp = 10;
		m_lpWriter->setMinMergeDocs(ulTmp);

		lpszTmp = m_lpThreadData->lpConfig->GetSetting("index_max_merge_docs");
		ulTmp = lpszTmp ? atoui(lpszTmp) : 0;
		if (!ulTmp)
			ulTmp = lucene::index::IndexWriter::DEFAULT_MAX_MERGE_DOCS;
		m_lpWriter->setMaxMergeDocs(ulTmp);

		lpszTmp = m_lpThreadData->lpConfig->GetSetting("index_term_interval");
		ulTmp = lpszTmp ? atoui(lpszTmp) : 0;
		if (!ulTmp)
			ulTmp = lucene::index::IndexWriter::DEFAULT_TERM_INDEX_INTERVAL;
		m_lpWriter->setTermIndexInterval(ulTmp);
	}
	catch (CLuceneError &e) {
		m_lpThreadData->lpLogger->Log(EC_LOGLEVEL_FATAL, "CLucene error: %s", e.what());
		m_lpThreadData->lpLogger->Log(EC_LOGLEVEL_FATAL, "CLucene failed while creating the IndexWriter");
		m_lpThreadData->lpLogger->Log(EC_LOGLEVEL_FATAL, "		this might have been caused because of a Lock timeout.");
		m_lpThreadData->lpLogger->Log(EC_LOGLEVEL_FATAL, "		If that is the case and the zarafa-indexer is recovering");
		m_lpThreadData->lpLogger->Log(EC_LOGLEVEL_FATAL, "		from a crash it might be necessary to delete all old locks");
		m_lpThreadData->lpLogger->Log(EC_LOGLEVEL_FATAL, "		from /tmp named  lucene-*-write.lock and lucene-*-commit.lock");
		hr = MAPI_E_CALL_FAILED;
		goto exit;
	}
	catch (...) {
		m_lpThreadData->lpLogger->Log(EC_LOGLEVEL_FATAL, "Failed to open lucene writer object");
		hr = MAPI_E_NOT_ENOUGH_MEMORY;
		goto exit;
	}

exit:
	pthread_mutex_unlock(&m_hResourceLock);

	return hr;
}

VOID ECLuceneAccess::CloseLuceneWriter()
{
	pthread_mutex_lock(&m_hResourceLock);

	if (m_lpWriter) {
		m_lpWriter->close();
		delete m_lpWriter;
		m_lpWriter = NULL;
	}

	pthread_mutex_unlock(&m_hResourceLock);
}

HRESULT ECLuceneAccess::OpenLuceneReader()
{
	HRESULT hr = hrSuccess;

	pthread_mutex_lock(&m_hResourceLock);

	try {
		m_lpReader = lucene::index::IndexReader::open(m_strIndexStore.c_str());
	}
	catch (CLuceneError &e) {
		m_lpThreadData->lpLogger->Log(EC_LOGLEVEL_FATAL, "CLucene error: %s", e.what());
		m_lpThreadData->lpLogger->Log(EC_LOGLEVEL_FATAL, "CLucene failed while creating the IndexReader");
		m_lpThreadData->lpLogger->Log(EC_LOGLEVEL_FATAL, "		this might have been caused because of a Lock timeout.");
		m_lpThreadData->lpLogger->Log(EC_LOGLEVEL_FATAL, "		If that is the case and the zarafa-indexer is recovering");
		m_lpThreadData->lpLogger->Log(EC_LOGLEVEL_FATAL, "		from a crash it might be necessary to delete all old locks");
		m_lpThreadData->lpLogger->Log(EC_LOGLEVEL_FATAL, "		from /tmp named  lucene-*-write.lock and lucene-*-commit.lock");
		hr = MAPI_E_CALL_FAILED;
	}
	catch (...) {
		m_lpThreadData->lpLogger->Log(EC_LOGLEVEL_FATAL, "Failed to open lucene reader object");
		hr = MAPI_E_NOT_ENOUGH_MEMORY;
	}

	pthread_mutex_unlock(&m_hResourceLock);

	return hr;
}

VOID ECLuceneAccess::CloseLuceneReader()
{
	pthread_mutex_lock(&m_hResourceLock);

	if (m_lpReader) {
		m_lpReader->close();
		delete m_lpReader;
		m_lpReader = NULL;
	}

	pthread_mutex_unlock(&m_hResourceLock);
}

HRESULT ECLuceneAccess::OpenLuceneSearcher()
{
	HRESULT hr = hrSuccess;

	pthread_mutex_lock(&m_hResourceLock);

	try {
		m_lpSearcher = new lucene::search::IndexSearcher(m_strIndexStore.c_str());
	}
	catch (CLuceneError &e) {
		m_lpThreadData->lpLogger->Log(EC_LOGLEVEL_FATAL, "CLucene error: %s", e.what());
		hr = MAPI_E_CALL_FAILED;
	}
	catch (...) {
		m_lpThreadData->lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to allocate search object");
		hr = MAPI_E_NOT_ENOUGH_MEMORY;
	}

	pthread_mutex_unlock(&m_hResourceLock);

	return hr;
}

VOID ECLuceneAccess::CloseLuceneSearcher()
{
	pthread_mutex_lock(&m_hResourceLock);

	if (m_lpSearcher) {
		m_lpSearcher->close();
		delete m_lpSearcher;
		m_lpSearcher = NULL;
	}

	pthread_mutex_unlock(&m_hResourceLock);
}
