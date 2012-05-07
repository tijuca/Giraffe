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

#ifndef ECLUCENE_H
#define ECLUCENE_H

#include <map>
#include <string>
#include <vector>

#include <pthread.h>

#include "zarafa-indexer.h"

class ECLuceneAccess;
class ECLuceneIndexer;
class ECLuceneSearcher;

/**
 * When no field is specified in the Query we use a default
 * field. It is not recommended to rely on which field is is set
 * as default. The client should at all times make sure the field
 * is specified in the query.
 */
#define DEFAULT_FIELD	_T("textdescription")

/**
 * The unqiue field which will be returned to the client as
 * identifier for a search result.
 */
#define UNIQUE_FIELD	_T("entryid")

/**
 * The field on which the filter is applied for a particular query.
 */
#define FILTER_FIELD	_T("folderid")

/**
 * httpmail namespace with corresponding MAPI and Lucene information.
 */
struct NShttpmail_t {
	/**
	 * Constructor
	 *
	 * @param[in]	strNShttpmail
	 * @param[in]	ulStorage
	 */
	NShttpmail_t(std::wstring strNShttpmail, unsigned int ulStorage)
		: strNShttpmail(strNShttpmail), ulStorage(ulStorage) {}
		
	/**
	 * httpmail namespace name
	 */
	const std::wstring strNShttpmail;

	/**
	 * Storage type (see lucene::document::Field flags)
	 */
	const unsigned int ulStorage;
};

/**
 * Cache entry for Single Instance Attachment data
 *
 * When Single Instance attachment data is encountered it will only be parsed to
 * plain-text once and stored in a cache in case the same attachment is found later
 * again.
 */
struct InstanceCacheEntry_t {
	/**
	 * Constructor
	 *
	 * @param[in] strAttachData
	 */
	InstanceCacheEntry_t(std::wstring &strAttachData)
		: strAttachData(strAttachData), ulTimestamp(time(NULL)) {}

	/**
	 * Parsed attachment data (plain-text)
	 */
	const std::wstring strAttachData;

	/**
	 * Last access time for cache entry
	 */
	time_t ulTimestamp;
};

typedef std::vector<std::string> string_list_t;
typedef std::map<unsigned int, NShttpmail_t> indexprop_map_t;
typedef std::map<std::string, InstanceCacheEntry_t> instance_map_t;
typedef std::map<std::string , ECLuceneAccess *> lucenestore_map_t;

/**
 * Wrapper to the Lucene classes access
 * 
 * This class contains a cache for ECLuceneAccess objects for all previously
 * opened stores. This also contains a cache for all parsed Single Instance attachments,
 * the Single Instance cache is shared between all indexed stores.
 */
class ECLucene {
public:
	/**
	 * Constructor
	 *
	 * @param[in]	lpThreadData
	 */
	ECLucene(ECThreadData *lpThreadData);

	/**
	 * Destructor
	 */
	~ECLucene();

	/**
	 * Create ECLuceneIndexer object
	 *
	 * @param[in]	lpThreadData
	 *					Reference to ECThreadData object.
	 * @param[in]	strStorePath
	 *					Path to store directory on harddisk.
	 * @param[in]	lpMsgStore
	 *					Reference to the IMsgStore object of the store to index.
	 * @param[in]	lpAdminSession
	 * 					The admin MAPI session.
	 * @param[out]	lppIndexer
	 *					Reference to the created ECLuceneIndexer object.
	 * @return HRESULT
	 */
	HRESULT CreateIndexer(ECThreadData *lpThreadData, std::string &strStorePath, IMsgStore *lpMsgStore, IMAPISession *lpAdminSession, ECLuceneIndexer **lppIndexer);

	/**
	 * Create ECLuceneSearcher object
	 *
	 * @param[in]	lpThreadData
	 *					Reference to ECThreadData object.
	 * @param[in]	strStorePath
	 *					Path to store directory on harddisk.
	 * @param[in]	listFolder
	 *					List of folders which limits the scope of the search query for ECLuceneSearcher.
	 * @param[out]	lppSearcher
	 *					Reference to the created ECLuceneSearcher object.
	 * @return HRESULT
	 */
	HRESULT CreateSearcher(ECThreadData *lpThreadData, std::string &strStorePath, std::vector<std::string> &listFolder, ECLuceneSearcher **lppSearcher);

	/**
	 * Perform cache optimization
	 *
	 * Purge all expired caches.
	 *
	 * @param[in]	bReset
	 *					If set to TRUE the configured timeout value is disregarded and all caches will be purged.
	 * @return HRESULT
	 */
	HRESULT Optimize(BOOL bReset);

	/**
	 * Delete all caches belonging to the given path to the store directory on harddisk
	 *
	 * @param[in]	strStorePath
	 * @return VOID
	 */
	VOID Delete(std::string &strStorePath);

	/**
	 * Search for NShttpmail_t reference which belongs to the given proptag
	 *
	 * @param[in]	ulPropTag
	 *					Property tag for which the corresponding NShttpmail_t object must be found.
	 * @return NShttpmail_t*
	 */
	NShttpmail_t* GetIndexedProp(ULONG ulPropTag);

	/**
	 * Request a list of all properties which have been indexed by Lucene.
	 *
	 * Each list entry is formatted as follows:
	 *     "NShttpmail_t::NShttpmail_t NShttpmail_t::strPropId"
	 *
	 * @param[out]	lpProps
	 *					List of std::strings containing all indexed properties
	 * @return HRESULT
	 */
	HRESULT GetIndexedProps(string_list_t *lpProps);

	/**
	 * Request a SPropTagArray with all indexed properties
	 *
	 * The SPropTagArray will consist of all properties indexed by Lucene, it can
	 * optionally be merged with additional property tags provided by the lpExtra
	 * parameter.
	 *
	 * @param[in]	lpExtra
	 *					Optional parameter containing a list of extra property tags
	 *					which should be added to the SPropTagArray returned in lppProps.
	 * @param[out]	lppProps
	 *					The created SPropTagArray containing all indexed properties,
	 *					plus the optionally extra properties as provided in lpExtra.
	 * @return HRESULT
	 */
	HRESULT GetIndexedProps(LPSPropTagArray lpExtra, LPSPropTagArray *lppProps);

	/**
	 * Search Single Instance Attachment cache for parsed attachment data
	 *
	 * @param[in]	strInstanceId
	 *					Single Instance ID which is requested.
	 * @param[out]	lpstrAttachData
	 *					The parsed to plain-text Single Instance attachment data.
	 * @return HRESULT
	 */
	HRESULT GetAttachmentCache(std::string &strInstanceId, std::wstring *lpstrAttachData);

	/**
	 * Store parsed atachment data for a particular Single Instance Attachment
	 *
	 * @param[in]	strInstanceId
	 *					Single Instance ID which is requested.
	 * @param[out]	strAttachData
	 *					The parsed to plain-text Single Instance attachment data.
	 * @return HRESULT
	 */
	HRESULT UpdateAttachmentCache(std::string &strInstanceId, std::wstring &strAttachData);
	
	HRESULT OptimizeIndex(ECThreadData *lpThreadData, std::string strStorePath);

private:
	/**
	 * Create ECLuceneAccess object
	 *
	 * Check if ECLuceneAccess object already exists in cache, or create a new
	 * ECLuceneAccess object.
	 *
	 * @param[in]	lpThreadData
	 *					Reference to ECThreadData object.
	 * @param[in]	strStorePath
	 *					Path to store directory on harddisk.
	 * @param[out]	lppAccess
	 *					Reference to the created ECLuceneAccess object.
	 * @return HRESULT
	 */
	HRESULT CreateAccess(ECThreadData *lpThreadData, std::string &strStorePath, ECLuceneAccess **lppAccess);

	/**
	 * Optimize ECLuceneAccess cache based on expire time
	 *
	 * @param[in]	ulExpireTime
	 *					If last access time of cache entry is lower then ulExpireTime
	 *					then entry is considered expired.
	 */
	VOID OptimizeAccess(time_t ulExpireTime);

	/**
	 * Optimize Single Instance Attachment cache based on expire time
	 *
	 * @param[in]	ulExpireTime
	 *					If last access time of cache entry is lower then ulExpireTime
	 *					then entry is considered expired.
	 */
	VOID OptimizeCache(time_t ulExpireTime);

private:
	ECThreadData *m_lpThreadData;

	pthread_mutexattr_t m_hLockAttr;
	pthread_mutex_t m_hLock;
	pthread_mutex_t m_hInstanceLock;

	indexprop_map_t m_mapIndexedProps;
	string_list_t m_listIndexedProps;

	lucenestore_map_t m_mapLuceneStores;

	instance_map_t m_mInstanceCache;

	ULONG m_ulCacheTimeout;
};

#endif /* ECLUCENE_H */
