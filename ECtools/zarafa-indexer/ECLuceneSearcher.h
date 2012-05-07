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

#ifndef ECLUCENESEARCHER_H
#define ECLUCENESEARCHER_H

#include <string>
#include <vector>

#include <pthread.h>

#include <CLucene.h>
#include <CLucene/search/QueryFilter.h>

#include <ECUnknown.h>

#include "zarafa-indexer.h"

class ECLuceneAccess;

/**
 * Main class to perform searching by CLucene
 */
class ECLuceneSearcher : public ECUnknown {
private:
	/**
	 * Constructor
	 *
	 * @note Objects of ECLuceneSearcher must only be created using the Create() function.
	 *
	 * @param[in]	lpThreadData
	 * @param[in]	lpLuceneAccess
	 * @param[in]	listFolder
	 */
	ECLuceneSearcher(ECThreadData *lpThreadData, ECLuceneAccess *lpLuceneAccess, string_list_t &listFolder, unsigned int ulMaxResults);

public:
	/**
	 * Create new ECLuceneSearcher object.
	 *
	 * @note Creating a new ECLuceneSearcher object must always occur through this function.
	 *
	 * @param[in]	lpThreadData
	 *					Reference to the ECThreadData object.
	 * @param[in]	lpLuceneAccess
	 *					Reference to the ECLuceneAccess object.
	 * @param[in]	listFolder
	 *					List of folders which limits the scope of the search query for ECLuceneSearcher.
	 * @param[in]	ulMaxResults
	 *					Maximum  number of results to produce
	 * @param[out]	lppSearcher
	 *					The created ECLuceneSearcher object.
	 * @return HRESULT
	 */
	static HRESULT Create(ECThreadData *lpThreadData, ECLuceneAccess *lpLuceneAccess, string_list_t &listFolder, unsigned int ulMaxResults, ECLuceneSearcher **lppSearcher);

	/**
	 * Destructor
	 */
	~ECLuceneSearcher();

	/**
	 * Execute query to search for messages
	 *
	 * @param[in]	strQuery
	 *					CLucene query to execute on the index.
	 * @param[out]	lplistResults
	 *					List of search results along with the score.
	 * @return HRESULT
	 */
	HRESULT SearchEntries(std::string &strQuery, string_list_t *lplistResults);

private:
	/**
	 * Create query filter to restrict search to only messages which match this filter.
	 *
	 * @param[out]	lppFilter
	 *					The QueryFilter to restrict the search results.
	 * @return HRESULT
	 */
	HRESULT CreateQueryFilter(lucene::search::QueryFilter **lppFilter);

	/**
	 * Execute query and use the given filter to restrict results.
	 *
	 * @param[in]	strQuery
	 *					CLucene query to execute on the index.
	 * @param[in]	lpFilter
	 *					The QueryFilter to restrict the search results.
	 * @param[out]	lplistResults
	 *					List of search results along with the score.
	 * @return HRESULT
	 */
	HRESULT SearchIndexedEntries(std::string &strQuery, lucene::search::QueryFilter *lpFilter, string_list_t *lplistResults);

private:
	ECThreadData *m_lpThreadData;
	ECLuceneAccess *m_lpLuceneAccess;

	string_list_t m_listFolder;
	unsigned int m_ulMaxResults;
};

#endif /* ECLUCENESEARCHER_H */
