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

#include "platform.h"
#include "CommonUtil.h"
#include "ECGenericObjectTable.h"
#include "stringutil.h"
#include "ECSearchClient.h"
#include "ECCacheManager.h"
#include "SOAPDebug.h"
#include "ZarafaCmdUtil.h"
#include "Util.h"
#include "ECStatsCollector.h"
#include "ustringutil.h"

#include <map>
#include <string>
#include <list>
#include <boost/algorithm/string/join.hpp>

/**
 * Gets the field name for the passed tag
 *
 * This should be in sync with what the indexer generates when it is indexing information.
 *
 * FORMAT:
 *
 * - 0x86FF or
 * - 00112233445566778899AABBCCDDEEFF.0x8510 or
 * - 00112233445566778899AABBCCDDEEFF.Keywords
 *
 * @param lpDatabase Database handle
 * @param ulTag Property tag to generate a field name for
 * @return String containing the field name
 */
std::string GetDynamicFieldName(ECDatabase *lpDatabase, unsigned int ulTag)
{
    ECRESULT er = erSuccess;
    
    struct propTagArray pta;
    struct namedPropArray sNames = {0,0};
    unsigned int aTag[1] = { PROP_ID(ulTag)-0x8500 };
    std::string strName;
    
    strName = stringify_int64(PROP_ID(ulTag), true);

    if(PROP_ID(ulTag) >= 0x8500) {
        pta.__size = 1;
        pta.__ptr = aTag;
        
        er = GetNamesFromIDs(NULL, lpDatabase, &pta, &sNames);
        if (er != erSuccess)
            goto exit;
            
        if(sNames.__ptr == NULL || sNames.__ptr[0].lpguid == NULL || sNames.__ptr[0].lpguid->__ptr == NULL|| (sNames.__ptr[0].lpId == NULL && sNames.__ptr[0].lpString == NULL)) 
            goto exit;
            
        strName = bin2hex(sizeof(GUID), (unsigned char *)sNames.__ptr[0].lpguid->__ptr);
        strName += ".";
        if(sNames.__ptr[0].lpId)
            strName += stringify(*sNames.__ptr[0].lpId);
        else
            strName += sNames.__ptr[0].lpString;
    }
    
exit:
    if(sNames.__ptr) 
        FreeNamedPropArray(&sNames, false);        
        
    return strName;
}

/**
 * Build a search query for the lucene search engine. It will convert a soap restriction to a lucene query
 *
 * @param[in] lpDatabase Database handle
 * @param[in] mapIndexedPropTags Map with the supported properties to match the MAPI Tag to Lucene Tag
 * @param[in] lpsRestrict Restriction which should converted to the indexed search query
 * @param[in] ulLevel Recursiveness level, limited to RESTRICT_MAX_DEPTH
 * @param[in,out] bHaveContent An indexed search query must have content to search for
 * @param[in,out] bNotOnly restriction only has not's and can't be run in the indexer
 * @param[out] strIndexQuery The search query for the indexer
 *
 * @return Zarafa error code
 * @retval ZARAFA_E_NO_SUPPORT	The restriction is not support to work with lucene indexer
 * @retval ZARAFA_E_IGNORE_ME	The data must be ignored
 */
ECRESULT BuildLuceneQuery(ECDatabase *lpDatabase, std::map<unsigned int, std::string> &mapIndexedPropTags, struct restrictTable *lpsRestrict, ULONG ulLevel, ULONG ulPrefixLen, bool &bHaveContent, bool &bNotOnly, std::string &strIndexQuery)
{
	ECRESULT		er = erSuccess;
	unsigned int	i = 0;
	size_t			ulStart = 0;
	size_t			ulEnd = 0;
	bool			bLast = false;
	std::string		strLocalQuery;
	std::string		strField;
	std::map<unsigned int, std::string>::iterator	iterPropTag;

	ULONG ulNots = 0;
	bool bState = false;

	if(ulLevel > RESTRICT_MAX_DEPTH) {
		er = ZARAFA_E_TOO_COMPLEX;
		goto exit;
	}

	switch(lpsRestrict->ulType) {
	case RES_COMMENT:
		// @todo check or this type is used by outlook search
	    //er = BuildLuceneQuery(mapIndexedPropTags, lpsRestrict->lpComment->lpResTable, ulLevel+1, strLocalQuery);
	    //if(er != erSuccess)
	       // goto exit;
		er = ZARAFA_E_NO_SUPPORT;
	    break;
	    
	case RES_OR:
		ulNots = 0;
		
		for(i=0; i<lpsRestrict->lpOr->__size; i++) {
			bState = false;
			
			strLocalQuery.clear();

			er = BuildLuceneQuery(lpDatabase, mapIndexedPropTags, lpsRestrict->lpOr->__ptr[i], ulLevel+1, ulPrefixLen, bHaveContent, bState, strLocalQuery);
			if (er == ZARAFA_E_IGNORE_ME) {
				continue;
			} else if (er != erSuccess) {
				goto exit;
			}

			if (bState)
				ulNots++;
			
			strIndexQuery += " ";
			strIndexQuery += strLocalQuery;
		}

		bNotOnly = lpsRestrict->lpOr->__size == ulNots;
		
		if(strIndexQuery.empty()) {
			er = ZARAFA_E_IGNORE_ME;
			goto exit;
		}
		strIndexQuery = "(" + strIndexQuery + ")";

		er = erSuccess;
		break;	
		
	case RES_AND:
		ulNots = 0;
		
		for(i=0; i<lpsRestrict->lpAnd->__size; i++) {
			bState = false;
			
			strLocalQuery.clear();

			er = BuildLuceneQuery(lpDatabase, mapIndexedPropTags, lpsRestrict->lpAnd->__ptr[i], ulLevel+1, ulPrefixLen, bHaveContent, bState, strLocalQuery);
			if (er == ZARAFA_E_IGNORE_ME) {
				continue;
			} else if (er != erSuccess) {
				goto exit;
			}

			if (bState)
				ulNots++;
				
            strIndexQuery += "+ ";
            strIndexQuery += strLocalQuery;
		}

		bNotOnly = lpsRestrict->lpAnd->__size == ulNots;

		if(strIndexQuery.empty()) {
			er = ZARAFA_E_IGNORE_ME;
			goto exit;
		}
		strIndexQuery = "(" + strIndexQuery + ")";

		er = erSuccess;
		break;	

	case RES_NOT:
		er = BuildLuceneQuery(lpDatabase, mapIndexedPropTags, lpsRestrict->lpNot->lpNot, ulLevel+1, ulPrefixLen, bHaveContent, bNotOnly, strLocalQuery);
		if (er == ZARAFA_E_IGNORE_ME) {
			break;
		} else if (er != erSuccess) {
			goto exit;
		}

		strIndexQuery += "!(" + strLocalQuery + ")";
		bNotOnly = true;
		break;

	case RES_CONTENT:

		if (PROP_TYPE(lpsRestrict->lpContent->lpProp->ulPropTag) != PT_STRING8 && PROP_TYPE(lpsRestrict->lpContent->lpProp->ulPropTag) != PT_UNICODE) {
			er = ZARAFA_E_NO_SUPPORT;
			break;
		}

		// The indexer can only do case-insensitive, substring searches
		if ((lpsRestrict->lpContent->ulFuzzyLevel & FL_SUBSTRING) == 0 ||
			(lpsRestrict->lpContent->ulFuzzyLevel & FL_IGNORECASE) == 0)
		{
			er = ZARAFA_E_NO_SUPPORT;
			break;
		}

		// Check if the property is a valid indexed property
		iterPropTag = mapIndexedPropTags.find(PROP_ID(lpsRestrict->lpContent->ulPropTag));
		if(iterPropTag == mapIndexedPropTags.end()){
		    strField = GetDynamicFieldName(lpDatabase, lpsRestrict->lpContent->ulPropTag);
		} else {
		    strField = iterPropTag->second;
        }

		strLocalQuery = StringEscape(lpsRestrict->lpContent->lpProp->Value.lpszA, "+-&|!(){}[]^\"~*?:\\", '\\');

		/*
		 * Substring searches are done with the query: (field: "Looking for something")
		 * Prefix searches are done with the query: (field: nothing*)
		 * Note 1: The query: (field: "nothing*") is invalid and will not produce results
		 * Note 2: The query: (field: nothing to see) is also invalid as the words 'to' and 'see' will
		 *         not be searched for in 'field' but in the default field.
		 *
		 * To prevent problems a query with both FL_SUBSTRING and FL_PREFIX is going to be parsed into:
		 * (field: nothing* AND field: to* AND field: see*)
		 * This won't be completely accurate since the exact substring is lost (instead of finding the
		 * requested sentence we find all messages containing all of the multiple words).
		 */
		if (lpsRestrict->lpContent->ulFuzzyLevel & (FL_PREFIX | FL_SUBSTRING)) {
			vector<string> words;
			do {
			    std::string strWord;
			    
				ulEnd = strLocalQuery.find(' ', ulStart);
				if (ulEnd == std::string::npos) {
					ulEnd = strLocalQuery.size();
					bLast = true;
				}

				strWord = string(strLocalQuery, ulStart, ulEnd - ulStart);
				
				if(u8_len(strWord.c_str()) >= ulPrefixLen)
    				words.push_back(strField + ": " + strWord + "*");
                else
    				words.push_back(strField + ": " + strWord);

				ulStart = ulEnd + 1;
			} while (!bLast);
			// build strIndexQuery
			strIndexQuery = boost::algorithm::join(words, string(" AND "));
		} else {
			strIndexQuery.append(strField);
			strIndexQuery.append(": \"");
			strIndexQuery.append(strLocalQuery);
			strIndexQuery.append("\"");
		}

		if (strIndexQuery.empty()) {
			er = ZARAFA_E_IGNORE_ME;
			goto exit;
		}
		bHaveContent = true;
		break;

	case RES_PROPERTY:
		// make sure shared folders are using the indexer by checking for just one prop, and only == and != are supported
		if (PROP_TYPE(lpsRestrict->lpProp->ulPropTag) == PT_LONG) {
			// Check if the property is a valid indexed property
			iterPropTag = mapIndexedPropTags.find(PROP_ID(lpsRestrict->lpProp->ulPropTag));
			if(iterPropTag == mapIndexedPropTags.end()){
				er = ZARAFA_E_NO_SUPPORT;
				break;
			}

			if (lpsRestrict->lpProp->ulType == RELOP_EQ) {
				strIndexQuery.append(iterPropTag->second);
				strIndexQuery.append(": \"");
				strIndexQuery.append(stringify(lpsRestrict->lpProp->lpProp->Value.ul));
				strIndexQuery.append("\"");
				break;
			} else if (lpsRestrict->lpProp->ulType == RELOP_NE) {
				strIndexQuery.append("!(");
				strIndexQuery.append(iterPropTag->second);
				strIndexQuery.append(": \"");
				strIndexQuery.append(stringify(lpsRestrict->lpProp->lpProp->Value.ul));
				strIndexQuery.append("\")");
				bNotOnly = true;
				break;
			}
		}
		er = ZARAFA_E_NO_SUPPORT;
		break;
	case RES_COMPAREPROPS:
	case RES_BITMASK:
	case RES_SIZE:
		er = ZARAFA_E_NO_SUPPORT;
		break;

	case RES_EXIST:
		iterPropTag = mapIndexedPropTags.find(PROP_ID(lpsRestrict->lpExist->ulPropTag));

		if (lpsRestrict->lpExist->ulPropTag == PR_LAST_MODIFICATION_TIME || iterPropTag != mapIndexedPropTags.end())
			er = ZARAFA_E_IGNORE_ME;
		else
			er = ZARAFA_E_NO_SUPPORT;

		break;

	case RES_SUBRESTRICTION:
		er = ZARAFA_E_NO_SUPPORT;
		break;

	default:
		ASSERT(FALSE); // A new restriction type?
		er = ZARAFA_E_NO_SUPPORT;
		break;
	}

exit:
	if (ulLevel == 0 && (!bHaveContent || bNotOnly)) {
		// we had a too simple restriction, and lucene won't be able
		// to find the items requested, so skip this restriction in
		// lucene, and do a database check.
		er = ZARAFA_E_NO_SUPPORT;
	}
	return er;
}

/** 
 * Starting point to build Lucene query
 *
 * @param[in] lpDatabase Database handle
 * @param[in] mapIndexedPropTags Map with the supported properties to match the MAPI Tag to Lucene Tag
 * @param[in] lpsRestrict Restriction which should converted to the indexed search query
 * @param[out] strIndexQuery The search query for the indexer
 *
 * @return Zarafa error code
 * @retval ZARAFA_E_NO_SUPPORT	The restriction is not support to work with lucene indexer
 */
ECRESULT BuildLuceneQuery(ECDatabase *lpDatabase, std::map<unsigned int, std::string> &mapIndexedPropTags, struct restrictTable *lpsRestrict, unsigned int ulPrefixLen, std::string &strIndexQuery)
{
	bool bHaveContent = false;
	bool bNotOnly = false;
	strIndexQuery.reserve(8192); // prealloc some memory, but unknown how large this will become
	return BuildLuceneQuery(lpDatabase, mapIndexedPropTags, lpsRestrict, 0, ulPrefixLen, bHaveContent, bNotOnly, strIndexQuery);
}

/** 
 * Try to run the restriction using the indexer instead of slow
 * database queries. Will fail if the restriction is unable to run by
 * the indexer.
 * 
 * @param[in] lpConfig config object
 * @param[in] lpLogger log object
 * @param[in] lpCacheManager cachemanager object
 * @param[in] guidServer current server guid
 * @param[in] ulStoreId store id to search in
 * @param[in] lstFolders list of folders to search in
 * @param[in] lpRestrict restriction to search against
 * @param[out] lppIndexerResults results found by indexer
 * 
 * @return Zarafa error code
 */
ECRESULT GetIndexerResults(ECDatabase *lpDatabase, ECConfig *lpConfig, ECLogger *lpLogger, ECCacheManager *lpCacheManager, GUID *guidServer, unsigned int ulStoreId, ECListInt &lstFolders, struct restrictTable *lpRestrict, ECSearchResultArray **lppIndexerResults)
{
    ECRESULT er = erSuccess;
	std::string strIndexQuery;
	ECSearchClient *lpSearchClient = NULL;
	std::map<unsigned int, std::string> mapIndexedPropTags;
	entryId *lpStoreId = NULL;
	entryList *lpEntryList = NULL;
	ECSearchResultArray	*lpIndexerResults = NULL;
	struct timeval tstart, tend;
	LONGLONG	llelapsedtime;

	if (!lpDatabase) {
		er = ZARAFA_E_DATABASE_ERROR;
		goto exit;
	}

	if(parseBool(lpConfig->GetSetting("index_services_enabled")) == true &&
		strlen(lpConfig->GetSetting("index_services_path")) > 0)
	{
		lpSearchClient = new ECSearchClient(lpConfig->GetSetting("index_services_path"), atoui(lpConfig->GetSetting("index_services_search_timeout")) );
		if (!lpSearchClient) {
			er = ZARAFA_E_NOT_ENOUGH_MEMORY;
			goto exit;
		}

		if(lpCacheManager->GetIndexedProperties(mapIndexedPropTags) != erSuccess) {
            er = lpSearchClient->GetProperties(mapIndexedPropTags);
            if (er == ZARAFA_E_NETWORK_ERROR)
                lpLogger->Log(EC_LOGLEVEL_ERROR, "Error while connecting to indexer on %s", lpConfig->GetSetting("index_services_path"));
            else if (er != erSuccess)
                lpLogger->Log(EC_LOGLEVEL_ERROR, "Error while querying indexer on %s, 0x%08x", lpConfig->GetSetting("index_services_path"), er);

            if (er != erSuccess)
                goto exit;
                
            er = lpCacheManager->SetIndexedProperties(mapIndexedPropTags);
            
            if (er != erSuccess)
                goto exit;
        }  

		er = BuildLuceneQuery(lpDatabase, mapIndexedPropTags, lpRestrict, atoui(lpConfig->GetSetting("index_services_prefix_chars")), strIndexQuery);
		if (er != erSuccess) {
			lpLogger->Log(EC_LOGLEVEL_DEBUG, "Unable to run restriction in indexer: 0x%08X, reverting to normal database search.", er);
			goto exit;
		}
		
        er = lpCacheManager->GetEntryIdFromObject(ulStoreId, NULL, &lpStoreId);
        if(er != erSuccess)
            goto exit;

        er = lpCacheManager->GetEntryListFromObjectList(&lstFolders, NULL, &lpEntryList);
        if(er != erSuccess)
            goto exit;

		lpLogger->Log(EC_LOGLEVEL_DEBUG, "Using index, query: %s", strIndexQuery.c_str());
        
        std::string strGuid = bin2hex(sizeof(GUID), (LPBYTE)guidServer);

		gettimeofday(&tstart, NULL);

        er = lpSearchClient->Query(strGuid, lpStoreId, lpEntryList, strIndexQuery, &lpIndexerResults);
		
		gettimeofday(&tend, NULL);
		llelapsedtime = difftimeval(&tstart,&tend);
		g_lpStatsCollector->Max(SCN_INDEXER_SEARCH_MAX, llelapsedtime);
		g_lpStatsCollector->Avg(SCN_INDEXER_SEARCH_AVG, llelapsedtime);

        if (er != erSuccess) {
			g_lpStatsCollector->Increment(SCN_INDEXER_SEARCH_ERRORS);
            lpLogger->Log(EC_LOGLEVEL_ERROR, "Error while querying indexer on %s, 0x%08x", lpConfig->GetSetting("index_services_path"), er);
		} else
			lpLogger->Log(EC_LOGLEVEL_DEBUG, "Indexed query results found in %.4f ms", llelapsedtime/1000.0);
	} else {
	    er = ZARAFA_E_NOT_FOUND;
    }

	*lppIndexerResults = lpIndexerResults;

exit:
	if(lpStoreId)
		FreeEntryId(lpStoreId, true);
	
	if(lpEntryList)
		FreeEntryList(lpEntryList, true);

	if (lpSearchClient)
		delete lpSearchClient;

	if (er != erSuccess)
		g_lpStatsCollector->Increment(SCN_DATABASE_SEARCHES);
	else
		g_lpStatsCollector->Increment(SCN_INDEXED_SEARCHES);
    
    return er;
}
