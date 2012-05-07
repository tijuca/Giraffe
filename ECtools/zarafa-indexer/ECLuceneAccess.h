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

#ifndef ECLUCENEACCESS_H
#define ECLUCENEACCESS_H

#include <string>

#include <pthread.h>

#include <CLucene.h>

#include <ECUnknown.h>

#include "zarafa-indexer.h"

/**
 * Wrapper class which provides access to the lucene classes used
 * for searching and indexing.
 */
class ECLuceneAccess : public ECUnknown {
private:
	/**
	 * Constructor
	 *
	 * @note Objects of ECLuceneAccess must only be created using the Create() function.
	 *
	 * @param[in]	lpThreadData
	 * @param[in]	strStorePath
	 */
	ECLuceneAccess(ECThreadData *lpThreadData, std::string &strStorePath);

public:
	/**
	 * Create new ECLuceneAccess object.
	 *
	 * @note Creating a new ECLuceneAccess object must always occur through this function.
	 *
	 * @param[in]	lpThreadData
	 *					Reference to the ECThreadData object.
	 * @param[in]	strStorePath
	 *					Path to store directory on harddisk.
	 * @param[out]	lppAccess
	 *					Reference to the created ECLuceneAccess object.
	 * @return HRESULT
	 */
	static HRESULT Create(ECThreadData *lpThreadData, std::string &strStorePath, ECLuceneAccess **lppAccess);

	/**
	 * Destructor
	 */
	~ECLuceneAccess();

	/**
	 * Optimize the CLucene object caches
	 *
	 * If a CLucene object has expired (ulExpireTime is larger then last use time
	 * for that object) the object will be closed causing all cache data to be
	 * flushed.
	 *
	 * @param	ulExpireTime
	 *				Expire timestamp, any timestamp below this value is considered expired.
	 * @return void
	 */
	VOID Optimize(time_t ulExpireTime);

	/**
	 * Check if the index directory to which this ECLuceneAccess object belongs to exists
	 *
	 * @return BOOL
	 */
	BOOL IndexExists();

	/**
	 * Check if the given index directory exists
	 *
	 * @param[in]	strIndexStore
	 *					Path to index direcory to check
	 * @return BOOL
	 */
	static BOOL IndexExists(std::string &strIndexStore);

	/**
	 * Request Lucene Analyzer
	 *
	 * Check if an Analyzer for this store has been previously created,
	 * or create a new Analyzer.
	 *
	 * @param[out]	lppAnalyzer
	 *					The created Analyzer.
	 * @return HRESULT
	 */
	HRESULT GetLuceneAnalyzer(lucene::analysis::Analyzer **lppAnalyzer);

	/**
	 * Request Lucene IndexWriter
	 *
	 * Check if an IndexWriter for this store has been previously created,
	 * or create a new IndexWriter. The returned object must always be released
	 * using PutLuceneWriter().
	 *
	 * @param[out]  lppWriter
	 *					The created IndexWriter.
	 * @return HRESULT
	 */
	HRESULT GetLuceneWriter(lucene::index::IndexWriter **lppWriter);

	/**
	 * Release Lucene IndexWriter
	 *
	 * Release IndexWriter obtained through the GetLuceneWriter() function.
	 *
	 * @param[in]	lpWriter
	 *					The IndexWriter to release
	 * @return VOID
	 */
	VOID PutLuceneWriter(lucene::index::IndexWriter *lpWriter);

	/**
	 * Request Lucene IndexReader
	 *
	 * Check if an IndexReader for this store has been previously created,
	 * or create a new IndexReader. The returned object must always be released
	 * using PutLuceneReader().
	 *
	 * @param[out]  lppReader
	 *					The created IndexReader.
	 */
	HRESULT GetLuceneReader(lucene::index::IndexReader **lppReader);

	/**
	 * Release Lucene IndexReader
	 *
	 * Release IndexReader obtained through the GetLuceneReader() function.
	 *
	 * @param[in]	lpReader
	 *					The IndexReader to release
	 * @param[in]	bClose
	 *					Close (and commit deletions) after unlocking the reader
	 */
	VOID PutLuceneReader(lucene::index::IndexReader *lpReader, bool bClose = false);

	/**
	 * Request Lucene IndexSearcher
	 *
	 * Check if an IndexSearcher for this store has been previously created,
	 * or create a new IndexSearcher. The returned object must always be released
	 * using PutLuceneSearcher().
	 *
	 * @param[out]  lppSearcher
	 *					The created IndexSearcher.
	 */
	HRESULT GetLuceneSearcher(lucene::search::IndexSearcher **lppSearcher);

	/**
	 * Release Lucene IndexSearcher
	 *
	 * Release IndexSearcher obtained through the GetLuceneSearcher() function.
	 *
	 * @param[in]	lpSearcher
	 *					The IndexSearcher to release
	 */
	VOID PutLuceneSearcher(lucene::search::IndexSearcher *lpSearcher);

private:
	/**
	 * Allocate Lucene Analyzer.
	 *
	 * The allocated analyzer wil be stored in m_lpAnalyzer.
	 *
	 * @return HRESULT
	 */
	HRESULT OpenLuceneAnalyzer();

	/**
	 * Delete previously allocated Lucene Analyzer
	 *
	 * @return VOID
	 */
	VOID CloseLuceneAnalyzer();

	/**
	 * Allocate Lucene IndexWriter.
	 *
	 * The allocated analyzer wil be stored in m_lpWriter.
	 *
	 * @return HRESULT
	 */
	HRESULT OpenLuceneWriter();

	/**
	 * Delete previously allocated Lucene IndexWriter
	 *
	 * @return VOID
	 */
	VOID CloseLuceneWriter();

	/**
	 * Allocate Lucene IndexReader.
	 *
	 * The allocated analyzer wil be stored in m_lpReader.
	 *
	 * @return HRESULT
	 */
	HRESULT OpenLuceneReader();

	/**
	 * Delete previously allocated Lucene IndexReader
	 *
	 * @return VOID
	 */
	VOID CloseLuceneReader();

	/**
	 * Allocate Lucene IndexSearcher.
	 *
	 * The allocated analyzer wil be stored in m_lpSearcher.
	 *
	 * @return HRESULT
	 */
	HRESULT OpenLuceneSearcher();

	/**
	 * Delete previously allocated Lucene IndexSearcher
	 *
	 * @return VOID
	 */
	VOID CloseLuceneSearcher();

private:
	ECThreadData *m_lpThreadData;

	std::string m_strIndexStore;

	pthread_mutexattr_t m_hLockAttr;
	pthread_mutex_t m_hResourceLock;
	pthread_mutex_t m_hWriteLock;
	pthread_mutex_t m_hSearchLock;

	lucene::analysis::Analyzer *m_lpAnalyzer;
	lucene::index::IndexWriter *m_lpWriter;
	lucene::index::IndexReader *m_lpReader;
	lucene::search::IndexSearcher *m_lpSearcher;

	time_t m_ulLastSearch;
	time_t m_ulLastWrite;
};

#endif /* ECLUCENEACCESS_H */

