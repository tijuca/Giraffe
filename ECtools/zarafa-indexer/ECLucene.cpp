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
#include "zarafa-indexer.h"

#include "charset/convert.h"

void CleanLuceneStores(lucenestore_map_t::value_type &entry)
{
	entry.second->Release();
}

ECLucene::ECLucene(ECThreadData *lpThreadData)
{
	const char *lpszCacheTimeout = NULL;
	convert_context converter;

	m_lpThreadData = lpThreadData;

	pthread_mutexattr_init(&m_hLockAttr);
	pthread_mutexattr_settype(&m_hLockAttr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&m_hLock, &m_hLockAttr);

	pthread_mutex_init(&m_hInstanceLock, NULL);

#define INDEX_PROP(__name, __storage, __prop) \
	indexprop_map_t::value_type(__prop, NShttpmail_t(_T(__name), __storage))

#define KEYWORD		( lucene::document::Field::INDEX_UNTOKENIZED | lucene::document::Field::STORE_YES )	///< (not?) searchable as complete item and value returned in result
#define UNSTORED	( lucene::document::Field::INDEX_TOKENIZED | lucene::document::Field::STORE_NO )	///< searchable as tokenized item, value not returned
#define TEXT		( lucene::document::Field::INDEX_TOKENIZED | lucene::document::Field::STORE_YES )	///< searchable as tokenized item and value returned in result

	/* 
	 * The following should actually be KEYWORD because we don't want the data
	 * to be tokenized, unfortunately CLucene seems unable to perform searches
	 * on untokenized fields.
	 */
	m_mapIndexedProps.insert(INDEX_PROP("entryid",			TEXT,		PR_ENTRYID));

	/*
	 * The following should be untokenized, but CLucene can't search on untokenized
	 * fields. This means that instead of KEYWORD we will have to use UNSTORED. We don't
	 * use TEXT because that will tokenize _and_ store the data which means the data will
	 * be stored twice in the index.
	 *
	 * For sourcekey we have to use KEYWORD since we are using it in a 'term' when deleting
	 * documents (which needs a 'stored' field type).
	 */
	m_mapIndexedProps.insert(INDEX_PROP("sourcekey",		KEYWORD,	PR_SOURCE_KEY));
	m_mapIndexedProps.insert(INDEX_PROP("folderid",			UNSTORED,	PR_PARENT_ENTRYID));
	m_mapIndexedProps.insert(INDEX_PROP("messageclass",		UNSTORED,	PR_MESSAGE_CLASS));

	/*
	 * We only want to search on the following fields, but we don't need
	 * the data inside the field to be visible in a searchresult, so only
	 * tokenize and index these fields but do not store them.
	 */
	m_mapIndexedProps.insert(INDEX_PROP("subject",			UNSTORED,	PR_SUBJECT));
	m_mapIndexedProps.insert(INDEX_PROP("textdescription",	UNSTORED,	PR_BODY));
	m_mapIndexedProps.insert(INDEX_PROP("sendername",		UNSTORED,	PR_SENDER_NAME));
	m_mapIndexedProps.insert(INDEX_PROP("senderemail",		UNSTORED,	PR_SENDER_EMAIL_ADDRESS));
	m_mapIndexedProps.insert(INDEX_PROP("fromname",			UNSTORED,	PR_SENT_REPRESENTING_NAME));
	m_mapIndexedProps.insert(INDEX_PROP("fromemail",		UNSTORED,	PR_SENT_REPRESENTING_EMAIL_ADDRESS));
	m_mapIndexedProps.insert(INDEX_PROP("displayto",		UNSTORED,	PR_DISPLAY_TO));
	m_mapIndexedProps.insert(INDEX_PROP("displaycc",		UNSTORED,	PR_DISPLAY_CC));
	m_mapIndexedProps.insert(INDEX_PROP("displaybcc",		UNSTORED,	PR_DISPLAY_BCC));

	m_mapIndexedProps.insert(INDEX_PROP("business_telephone", UNSTORED,	PR_BUSINESS_TELEPHONE_NUMBER));
	m_mapIndexedProps.insert(INDEX_PROP("business2_telephone",UNSTORED,	PR_BUSINESS2_TELEPHONE_NUMBER));
	m_mapIndexedProps.insert(INDEX_PROP("department",		UNSTORED,	PR_DEPARTMENT_NAME));
	m_mapIndexedProps.insert(INDEX_PROP("mobile_telephone",	UNSTORED,	PR_MOBILE_TELEPHONE_NUMBER));
	m_mapIndexedProps.insert(INDEX_PROP("home_telephone",	UNSTORED,	PR_HOME_TELEPHONE_NUMBER));
	m_mapIndexedProps.insert(INDEX_PROP("title",			UNSTORED,	PR_TITLE));
	m_mapIndexedProps.insert(INDEX_PROP("company",			UNSTORED,	PR_COMPANY_NAME));
	m_mapIndexedProps.insert(INDEX_PROP("display",			UNSTORED,	PR_DISPLAY_NAME));
	m_mapIndexedProps.insert(INDEX_PROP("sensitivity",		UNSTORED,	PR_SENSITIVITY));

	for (indexprop_map_t::iterator iter = m_mapIndexedProps.begin(); iter != m_mapIndexedProps.end(); iter++) {
		if (!(iter->second.ulStorage & lucene::document::Field::INDEX_NO))
			m_listIndexedProps.push_back(converter.convert_to<std::string>(iter->second.strNShttpmail) + " " + stringify(PROP_ID(iter->first), true));
	}

	lpszCacheTimeout = m_lpThreadData->lpConfig->GetSetting("index_cache_timeout");
	m_ulCacheTimeout = (lpszCacheTimeout ? atoui(lpszCacheTimeout) : 0);
}

ECLucene::~ECLucene()
{
	pthread_mutex_destroy(&m_hInstanceLock);
	pthread_mutex_destroy(&m_hLock);
	pthread_mutexattr_destroy(&m_hLockAttr);

	for_each(m_mapLuceneStores.begin(), m_mapLuceneStores.end(), CleanLuceneStores);
}

HRESULT ECLucene::CreateIndexer(ECThreadData *lpThreadData, std::string &strStorePath, IMsgStore *lpMsgStore, IMAPISession *lpAdminSession, ECLuceneIndexer **lppIndexer)
{
	HRESULT hr = hrSuccess;
	ECLuceneAccess *lpAccess = NULL;
	ECLuceneIndexer *lpIndexer = NULL;

	hr = lpThreadData->lpFileIndex->StoreCreate(strStorePath);
	if (hr != hrSuccess)
		goto exit;

	hr = CreateAccess(lpThreadData, strStorePath, &lpAccess);
	if (hr != hrSuccess)
		goto exit;

	hr = ECLuceneIndexer::Create(lpThreadData, lpAccess, lpMsgStore, lpAdminSession, &lpIndexer);
	if (hr != hrSuccess)
		goto exit;

	if (lppIndexer)
		*lppIndexer = lpIndexer;

exit:
	if ((hr != hrSuccess) && lpIndexer)
		lpIndexer->Release();

	if (lpAccess)
		lpAccess->Release();

	return hr;
}

HRESULT ECLucene::CreateSearcher(ECThreadData *lpThreadData, std::string &strStorePath, std::vector<std::string> &listFolder, ECLuceneSearcher **lppSearcher)
{
	HRESULT hr = hrSuccess;
	ECLuceneAccess *lpAccess = NULL;
	ECLuceneSearcher *lpSearcher = NULL;
	std::string strIndexPath;
	unsigned int ulMaxResults = 0;
	
	ulMaxResults = atoui(m_lpThreadData->lpConfig->GetSetting("limit_results"));

	hr = lpThreadData->lpFileIndex->GetStoreIndexPath(strStorePath, &strIndexPath);
	if (hr != hrSuccess)
		goto exit;

	if (!ECLuceneAccess::IndexExists(strIndexPath)) {
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}

	hr = CreateAccess(lpThreadData, strStorePath, &lpAccess);
	if (hr != hrSuccess)
		goto exit;

	hr = ECLuceneSearcher::Create(m_lpThreadData, lpAccess, listFolder, ulMaxResults, &lpSearcher);
	if (hr != hrSuccess)
		goto exit;

	if (lppSearcher)
		*lppSearcher = lpSearcher;

exit:
	if ((hr != hrSuccess) && lpSearcher)
		lpSearcher->Release();

	if (lpAccess)
		lpAccess->Release();

	return hr;
}

HRESULT ECLucene::CreateAccess(ECThreadData *lpThreadData, std::string &strStorePath, ECLuceneAccess **lppAccess)
{
	HRESULT hr = hrSuccess;
	ECLuceneAccess *lpAccess = NULL;
	lucenestore_map_t::iterator iter;

	pthread_mutex_lock(&m_hLock);
	iter = m_mapLuceneStores.find(strStorePath);
	if (iter != m_mapLuceneStores.end())
		lpAccess = iter->second;
	pthread_mutex_unlock(&m_hLock);

	if (!lpAccess) {
		hr = ECLuceneAccess::Create(lpThreadData, strStorePath, &lpAccess);
		if (hr != hrSuccess)
			goto exit;

		pthread_mutex_lock(&m_hLock);
		m_mapLuceneStores.insert(lucenestore_map_t::value_type(strStorePath, lpAccess));
		pthread_mutex_unlock(&m_hLock);
	}

	/*
	 * IMPORTANT: Increase refcount!
	 * The refcount was already increased once during ECLuceneAccess::Create(),
	 * that refcount is kept because we cached the object in m_mapLuceneStores.
	 * This means a second AddRef() is needed to give the object to the caller.
	 */
	lpAccess->AddRef();

	if (lppAccess)
		*lppAccess = lpAccess;

exit:
	if ((hr != hrSuccess) && lpAccess)
		lpAccess->Release();

	return hr;
}

VOID ECLucene::OptimizeAccess(time_t ulExpireTime)
{
	lucenestore_map_t::iterator iter;

	pthread_mutex_lock(&m_hLock);

	for (iter = m_mapLuceneStores.begin(); iter != m_mapLuceneStores.end(); iter++)
		iter->second->Optimize(ulExpireTime);

	pthread_mutex_unlock(&m_hLock);
}

VOID ECLucene::OptimizeCache(time_t ulExpireTime)
{
	instance_map_t::iterator iter;

	/* Caching is disabled */
	if (!m_ulCacheTimeout)
		return;

	pthread_mutex_lock(&m_hInstanceLock);

	for (iter = m_mInstanceCache.begin(); iter != m_mInstanceCache.end(); /* no increment */) {
		if (ulExpireTime > iter->second.ulTimestamp)
			m_mInstanceCache.erase(iter++);
		else
			iter++;
	}

	pthread_mutex_unlock(&m_hInstanceLock);
}

HRESULT ECLucene::Optimize(BOOL bReset)
{
	HRESULT hr = hrSuccess;
	time_t ulExpireTime = 0;
	
	if (!bReset)
		ulExpireTime = time(NULL) - m_ulCacheTimeout;

	OptimizeAccess(ulExpireTime);
	OptimizeCache(ulExpireTime);

	return hr;
}

HRESULT ECLucene::OptimizeIndex(ECThreadData *lpThreadData, std::string strStorePath)
{
	HRESULT hr = hrSuccess;
	ECLuceneAccess *lpAccess = NULL;
	lucene::index::IndexWriter *lpWriter = NULL;
	
	hr = CreateAccess(lpThreadData, strStorePath, &lpAccess);
	if(hr != hrSuccess)
		goto exit;
	
	hr = lpAccess->GetLuceneWriter(&lpWriter);	
	if(hr != hrSuccess)
		goto exit;

	try {
		lpWriter->optimize();
	}
	catch (CLuceneError &e) {
		hr = MAPI_E_CALL_FAILED;
		m_lpThreadData->lpLogger->Log(EC_LOGLEVEL_FATAL, "CLucene error during index optimize: %s", e.what());
		goto exit;
	}
	catch (std::exception &e) {
		hr = MAPI_E_CALL_FAILED;
		m_lpThreadData->lpLogger->Log(EC_LOGLEVEL_FATAL, "STD error during index optimize: %s", e.what());
		goto exit;
	}
	catch (...) {
		hr = MAPI_E_CALL_FAILED;
		m_lpThreadData->lpLogger->Log(EC_LOGLEVEL_FATAL, "Unknown error during index optimize");
		goto exit;
	}
				
exit:
	if(lpWriter)
		lpAccess->PutLuceneWriter(lpWriter);
		
	if(lpAccess)
		lpAccess->Release();
		
	return hr;
}

VOID ECLucene::Delete(std::string &strStorePath)
{
	ECLuceneAccess *lpAccess = NULL;
	lucenestore_map_t::iterator iter;

	pthread_mutex_lock(&m_hLock);
	iter = m_mapLuceneStores.find(strStorePath);
	if (iter != m_mapLuceneStores.end()) {
		lpAccess = iter->second;
		m_mapLuceneStores.erase(iter);
	}
	pthread_mutex_unlock(&m_hLock);

	if (lpAccess)
		lpAccess->Release();
}

NShttpmail_t* ECLucene::GetIndexedProp(ULONG ulPropTag)
{
	if(PROP_TYPE(ulPropTag) == PT_STRING8) 
		ulPropTag = CHANGE_PROP_TYPE(ulPropTag, PT_UNICODE);
	if(PROP_TYPE(ulPropTag) == PT_MV_STRING8) 
		ulPropTag = CHANGE_PROP_TYPE(ulPropTag, PT_MV_UNICODE);
	
	indexprop_map_t::iterator iter = m_mapIndexedProps.find(ulPropTag);
	if (iter == m_mapIndexedProps.end())
		return NULL;
	return &iter->second;
}	

HRESULT ECLucene::GetIndexedProps(string_list_t *lpProps)
{
	HRESULT hr = hrSuccess;

	if (lpProps)
		*lpProps = m_listIndexedProps;

	return hr;
}

HRESULT ECLucene::GetIndexedProps(LPSPropTagArray lpExtra, LPSPropTagArray *lppProps)
{
	HRESULT hr = hrSuccess;
	indexprop_map_t::const_iterator iter;
	LPSPropTagArray lpProps = NULL;

	hr = MAPIAllocateBuffer(CbNewSPropTagArray(m_mapIndexedProps.size() + (lpExtra ? lpExtra->cValues : 0)), (LPVOID *)&lpProps);
	if (hr != hrSuccess)
		goto exit;

	lpProps->cValues = 0;

	for (iter = m_mapIndexedProps.begin(); iter != m_mapIndexedProps.end(); iter++) {
		lpProps->aulPropTag[lpProps->cValues] = iter->first;
		lpProps->cValues++;
	}

	if (lpExtra) {
		for (ULONG i = 0; i < lpExtra->cValues; i++) {
			lpProps->aulPropTag[lpProps->cValues] = lpExtra->aulPropTag[i];
			lpProps->cValues++;
		}
	}

	if (lppProps)
		*lppProps = lpProps;

exit:
	if ((hr != hrSuccess) && lpProps)
		MAPIFreeBuffer(lpProps);

	return hr;
}

HRESULT ECLucene::GetAttachmentCache(std::string &strInstanceId, std::wstring *lpstrAttachData)
{
	HRESULT hr = hrSuccess;
	instance_map_t::iterator iter;

	/* Caching is disabled */
	if (!m_ulCacheTimeout)
		return MAPI_E_NOT_FOUND;

	pthread_mutex_lock(&m_hInstanceLock);

	iter = m_mInstanceCache.find(strInstanceId);
	if (iter == m_mInstanceCache.end()) {
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}

	iter->second.ulTimestamp = time(NULL);
	if (lpstrAttachData)
		lpstrAttachData->assign(iter->second.strAttachData);

exit:
	pthread_mutex_unlock(&m_hInstanceLock);

	return hr;
}

HRESULT ECLucene::UpdateAttachmentCache(std::string &strInstanceId, std::wstring &strAttachData)
{
	HRESULT hr = hrSuccess;

	/* Caching is disabled */
	if (!m_ulCacheTimeout)
		goto exit;

	pthread_mutex_lock(&m_hInstanceLock);
	m_mInstanceCache.insert(instance_map_t::value_type(strInstanceId, InstanceCacheEntry_t(strAttachData)));
	pthread_mutex_unlock(&m_hInstanceLock);

exit:
	return hr;
}
