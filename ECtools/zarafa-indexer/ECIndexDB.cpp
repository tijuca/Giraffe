/*
 * Copyright 2005 - 2011  Zarafa B.V.
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
#include "ECIndexDB.h"
#include "ECAnalyzers.h"
#include "ECLogger.h"

#include "ECConfig.h"
#include "ECIndexerUtil.h"
#include "CommonUtil.h"
#include "stringutil.h"
#include "charset/convert.h"

#include <unicode/coll.h>
#include <unicode/ustring.h>
#include <kcpolydb.h>
#include <kcdbext.h>

#include <list>
#include <string>

#include <CLucene/util/Reader.h>

using namespace kyotocabinet;

// Key/Value types for KT_TERMS
typedef struct __attribute__((__packed__)) {
    unsigned int folder;
    unsigned short field;
    unsigned int doc;
    unsigned short version;
    unsigned char len;
} TERMENTRY;

typedef struct {
    unsigned int type; // Must be KT_TERMS
    char prefix[1]; // Actually more than 1 char
} TERMKEY;

// Key/Value types for KT_VERSION
typedef struct {
    unsigned int type; // Must be KT_VERSION
    unsigned int doc;
} VERSIONKEY;

typedef struct {
    unsigned short version;
} VERSIONVALUE;

// Key/Value types for KT_SOURCEKEY
typedef struct {
    unsigned int type;
    char sourcekey[1];
} SOURCEKEYKEY;

typedef struct {
    unsigned int doc;
} SOURCEKEYVALUE;
    
enum KEYTYPES { KT_TERMS, KT_VERSION, KT_SOURCEKEY };

#define MIN(x,y) ((x) < (y) ? (x) : (y))
#define MAX(x,y) ((x) > (y) ? (x) : (y))

ECIndexDB::ECIndexDB(const std::string &strIndexId, ECConfig *lpConfig, ECLogger *lpLogger)
{
    UErrorCode status = U_ZERO_ERROR;
    
    m_lpConfig = lpConfig;
    m_lpLogger = lpLogger;
    
    m_lpAnalyzer = new ECAnalyzer();
    
    ParseProperties(lpConfig->GetSetting("index_exclude_properties"), m_setExcludeProps);
    
    m_lpIndex = NULL;

    m_lpCollator = Collator::createInstance(status);
	m_lpCollator->setAttribute(UCOL_STRENGTH, UCOL_PRIMARY, status);
}

ECIndexDB::~ECIndexDB()
{
    if(m_lpIndex) {
        m_lpIndex->close();
        delete m_lpIndex;
    }
    if(m_lpAnalyzer)
        delete m_lpAnalyzer;
        
    if(m_lpCollator)
        delete m_lpCollator;
}

HRESULT ECIndexDB::Create(const std::string &strIndexId, ECConfig *lpConfig, ECLogger *lpLogger, ECIndexDB **lppIndexDB)
{
    HRESULT hr = hrSuccess;
    
    ECIndexDB *lpIndex = new ECIndexDB(strIndexId, lpConfig, lpLogger);
    
    hr = lpIndex->Open(strIndexId);
    if(hr != hrSuccess)
        goto exit;

    *lppIndexDB = lpIndex;
        
exit:
    if (hr != hrSuccess && lpIndex)
        delete lpIndex;
        
    return hr;
}

HRESULT ECIndexDB::Open(const std::string &strIndexId)
{
    HRESULT hr = hrSuccess;

    m_lpIndex = new IndexDB();

    if(!m_lpIndex->open(std::string(m_lpConfig->GetSetting("index_path")) + PATH_SEPARATOR + strIndexId + ".kct#zcomp=zlib", PolyDB::OWRITER | PolyDB::OCREATE | PolyDB::OREADER)) {
        m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to open index: %s", m_lpIndex->error().message());
        hr = MAPI_E_NOT_FOUND;
        goto exit;
    }
    
exit:    
    return hr;
}

HRESULT ECIndexDB::AddTerm(folderid_t folder, docid_t doc, fieldid_t field, unsigned int version, std::wstring wstrTerm)
{
    HRESULT hr = hrSuccess;
    std::string strQuery;
    std::string strValues;
    lucene::analysis::TokenStream* stream = NULL;
    lucene::analysis::Token* token = NULL;
    unsigned int type = KT_TERMS;

    char buf[sizeof(TERMENTRY) + 256];
    char keybuf[sizeof(unsigned int) + 256];
    TERMENTRY *sTerm = (TERMENTRY *)buf;

    // Preset all the key/value parts that will not change
    sTerm->folder = folder;
    sTerm->doc = doc;
    sTerm->field = field;
    sTerm->version = version;
    
    memcpy(keybuf, (char *)&type, sizeof(type));

    // Check if the property is excluded from indexing
    if (m_setExcludeProps.find(field) == m_setExcludeProps.end())
    {   
        const wchar_t *text;
        unsigned int len;
        unsigned int keylen;
        
        lucene::util::StringReader reader(wstrTerm.c_str());
        
        stream = m_lpAnalyzer->tokenStream(L"", &reader);
        
        while((token = stream->next())) {
            text = token->termText();
            len = MIN(wcslen(text), 255);
            
            if(len == 0)
                goto next;
            
            // Generate sortkey and put it directly into our key
            keylen = GetSortKey(text, MIN(len, 3), keybuf + sizeof(unsigned int), 256);
            
            if(keylen > 256) {
                ASSERT(false);
                keylen = 256;
            }
            
            if(len <= 3) {
                // No more term info in the value
                sTerm->len = 0;
            } else {
                sTerm->len = GetSortKey(text + 3, len - 3, buf + sizeof(TERMENTRY), 256);
                if(sTerm->len > 256) {
                    ASSERT(false);
                    sTerm->len = 256;
                }
            }
                
            m_lpIndex->append(keybuf, sizeof(unsigned int) + keylen, buf, sizeof(TERMENTRY) + sTerm->len);
            
next:        
            delete token;
        }
    }
    
    if(stream)
        delete stream;
    
    return hr;
}

HRESULT ECIndexDB::RemoveTermsFolder(folderid_t folder)
{
    return hrSuccess;
}

/**
 * Remove terms for a document in preparation for update
 *
 * In practice, actual deletion is deferred until a later time since removal of
 * search terms is an expensive operation. We track removed or updates messages
 * by inserting them into the 'updates' table, which will remove them from any
 * future search queries.
 *
 * If this delete is called prior to a call to AddTerm(), the returned version
 * must be used in the call to AddTerm(), which ensures that the new search terms
 * will be returned in future searches.
 *
 * @param[in] doc Document ID to remove
 * @param[out] lpulVersion Version of new terms to be added
 * @return Result
 */
HRESULT ECIndexDB::RemoveTermsDoc(docid_t doc, unsigned int *lpulVersion)
{
    HRESULT hr = hrSuccess;
    VERSIONKEY sKey;
    VERSIONVALUE sValue;
    char *value = NULL;
    size_t cb = 0;
    
    sKey.type = KT_VERSION;
    sKey.doc = doc;
    
    value = m_lpIndex->get((char *)&sKey, sizeof(sKey), &cb);
    
    if(!value || cb != sizeof(VERSIONVALUE)) {
        sValue.version = 1;
    } else {
        sValue = *(VERSIONVALUE *)value;
        sValue.version++;
    }
    
    if(!m_lpIndex->set((char *)&sKey, sizeof(sKey), (char *)&sValue, sizeof(sValue))) {
        m_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to set version for document: %s", m_lpIndex->error().message());
        hr = MAPI_E_NOT_FOUND;
        goto exit;
    }
    
    if(lpulVersion)
        *lpulVersion = sValue.version;
     
exit:   
    if (value)
        delete [] value;
        
    return hr;
}

/**
 * Remove terms for a document
 *
 * Uses the same logic as RemoveTermsDoc() but takes a sourcekey as a parameter.
 *
 * @param[in] store Store ID to remove document from
 * @param[in[ folder Folder ID to remove document from
 * @param[in] strSourceKey Source key to remove
 * @return Result
 */
HRESULT ECIndexDB::RemoveTermsDoc(folderid_t folder, std::string strSourceKey)
{
    HRESULT hr = hrSuccess;
    unsigned int type = KT_SOURCEKEY;
    std::string strKey;
    SOURCEKEYVALUE *sValue = NULL;
    size_t cb = 0;
    
    strKey.assign((char *)&type, sizeof(type));
    strKey += strSourceKey;
    
    sValue = (SOURCEKEYVALUE *)m_lpIndex->get(strKey.c_str(), strKey.size(), &cb);
    
    if(!sValue || cb != sizeof(SOURCEKEYVALUE)) {
        hr = MAPI_E_NOT_FOUND;
        goto exit;
    }
    
    m_lpIndex->remove(strKey);
    
    hr = RemoveTermsDoc(sValue->doc, NULL);
    if (hr != hrSuccess)
        goto exit;
    
exit:
    if (sValue)
        delete [] sValue;
    
    return hr;
}

// FIXME lstFolders should be setFolders in the first place
HRESULT ECIndexDB::QueryTerm(std::list<unsigned int> &lstFolders, std::set<unsigned int> &setFields, std::wstring &wstrTerm, std::list<docid_t> &lstMatches)
{
    HRESULT hr = hrSuccess;
    std::set<unsigned int> setFolders;
    std::set<unsigned int> setMatches;
    std::set<unsigned int>::iterator j;
    std::list<unsigned int>::iterator i;
    size_t len;
    char *value = NULL;
    unsigned int offset;
    unsigned int type = KT_TERMS;
    TERMENTRY *p;
    char keybuf[sizeof(unsigned int) + 256];
    unsigned int keylen = 0;
    char valbuf[256];
    unsigned int vallen = 0;
    
    lstMatches.clear();
    
    // Generate a lookup set for the folders
    for(i = lstFolders.begin(); i != lstFolders.end(); i++) {
        setFolders.insert(*i);
    }

    // Make the key that we will search    
    memcpy(keybuf, (char *)&type, sizeof(type));
    keylen = GetSortKey(wstrTerm.c_str(), MIN(wstrTerm.size(), 3), keybuf + sizeof(unsigned int), 256);
    keylen = MIN(256, keylen);
    
    if(wstrTerm.size() > 3) {
        vallen = GetSortKey(wstrTerm.c_str() + 3, wstrTerm.size() - 3, valbuf, 256);
        vallen = MIN(256, vallen);
    } else {
        vallen = 0;
    }
    
    value = m_lpIndex->get(keybuf, sizeof(unsigned int) + keylen, &len);
    offset = 0;

    if(!value)
        goto exit; // No matches
    
    while(offset < len) {
        p = (TERMENTRY *) (value + offset);
        
        if (setFolders.count(p->folder) == 0)
            goto next;
        if (setFields.count(p->field) == 0)
            goto next;
        if (p->len < vallen)
            goto next;
            
        if (vallen == 0 || strncmp(valbuf, value + offset + sizeof(TERMENTRY), vallen) == 0) {
            VERSIONKEY sKey;
            VERSIONVALUE *sVersion;
            size_t cb = 0;
            sKey.type = KT_VERSION;
            sKey.doc = p->doc;
            
            // Check if the version is ok
            sVersion = (VERSIONVALUE *)m_lpIndex->get((char *)&sKey, sizeof(sKey), &cb);
            
            if(!sVersion || cb != sizeof(VERSIONVALUE) || sVersion->version == p->version)
                setMatches.insert(p->doc);
        }
next:
        offset += sizeof(TERMENTRY) + p->len;            
    }
    
    for(j = setMatches.begin(); j != setMatches.end(); j++) {
        lstMatches.push_back(*j);
    }
    
exit:
    if (value)
        delete [] value;
        
    return hr;
}

/**
 * Add document sourcekey
 *
 * @param[in] store Store ID of document
 * @param[in] folder Folder ID of document (hierarchyid)
 * @param[in] strSourceKey Source key of document
 * @param[in] doc Document ID of document (hierarchyid)
 * @return result
 */
HRESULT ECIndexDB::AddSourcekey(folderid_t folder, std::string strSourceKey, docid_t doc)
{
    HRESULT hr = hrSuccess;
    std::string strKey, strValue;
    unsigned int type = KT_SOURCEKEY;
    
    strKey.assign((char *)&type, sizeof(type));
    strKey.append(strSourceKey);
    
    strValue.assign((char *)&doc, sizeof(doc));
        
    m_lpIndex->set(strKey, strValue);
        
    return hr;
}

/**
 * Create a sortkey from the wchar_t input passed
 *
 * @param wszInput Input string
 * @param len Length of wszInput in characters
 * @param szOutput Output buffer
 * @param outLen Output buffer size in bytes
 * @return Length of data written to szOutput
 */
size_t ECIndexDB::GetSortKey(const wchar_t *wszInput, size_t len, char *szOutput, size_t outLen)
{
    UChar in[1024];
    int32_t inlen;
    int32_t keylen;
    UErrorCode error = U_ZERO_ERROR;

    u_strFromWCS(in, arraySize(in), &inlen, wszInput, len, &error);
    
    keylen = m_lpCollator->getSortKey(in, inlen, (uint8_t *)szOutput, (int32_t)outLen);
    
    return keylen - 1;
}
