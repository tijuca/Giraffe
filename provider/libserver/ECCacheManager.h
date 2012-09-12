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

#ifndef ECCACHEMANAGER
#define ECCACHEMANAGER

#include <map>
#include <pthread.h>

#include "ECDatabaseFactory.h"
#include "ECDatabaseUtils.h"
#include "ECGenericObjectTable.h"	// ECListInt
#include "ECConfig.h"
#include "ECLogger.h"
#include "SOAPUtils.h"
#include <mapidefs.h>
#include <ECCache.h>

#include "../common/ECKeyTable.h"

#ifdef __GNUC__
#if __GNUC__ < 3
	#define SGIBASE_NAMESPACE   sgi
    #include <hash_map.h>
    namespace sgi { using ::hash_map; }; // inherit globals
#else
	#if __GNUC__ == 4 && __GNUC_MINOR__ >= 3 && defined(__DEPRECATED)
		#undef __DEPRECATED
	#endif

    #include <ext/hash_map>

    #if __GNUC__ == 3 && __GNUC_MINOR__ == 0
		#define SGIBASE_NAMESPACE   std
        namespace sgi = std;               // GCC 3.0
    #else
		#define SGIBASE_NAMESPACE  __gnu_cxx
        namespace sgi = ::__gnu_cxx;       // GCC 3.1 and later
    #endif

	#if __GNUC__ == 4 && __GNUC_MINOR__ >= 3
		#define __DEPRECATED 1
	#endif
#endif
#else
	#define SGIBASE_NAMESPACE       sgi
	// Win32
	#include <hash_map>

	// The win32 standard hash_map is different to sgi's hash_map, so we have a workaround here
	// We basically map the standard SGI interface to the win32 interface, BUT the EqualFunc
	// function must be a less<> function in win32 (!!), This is automatically fixed when you
	// use sgi::equal_to as we define this here, but when you define your own EqualFunc, this must
	// be a less<> function in win32, and equal_to<> in sgi-compatible environments
	namespace sgi {

		template <typename Key, typename HashFunc, typename EqualFunc>
			class Traits {
			private:
				const HashFunc hasher;
				const EqualFunc comparer;
			public:
				Traits() {};
				~Traits() {};
				enum
					{	// parameters for hash table
					bucket_size = 4,	// 0 < bucket_size
					min_buckets = 8};	// min_buckets = 2 ^^ N, 0 < N

				size_t operator() (const Key &a) const { return hasher(a); };
				bool operator() (const Key &a, const Key &b) const { return comparer(a,b); };
			};

		template <typename Key, typename Value, typename HashFunc, typename EqualFunc>
			class hash_map : public stdext::hash_map<Key, Value, Traits<Key, HashFunc, EqualFunc> > { };

		template <typename Key>
			class hash {
			public:
				hash() {};
				~hash() {};

				size_t operator() (const Key value) const { return stdext::hash_value<Key>(value); }
			};

		template <typename Key>
			class equal_to {
			private:
				const std::less<Key> comparer;
			public:
				equal_to() {};
				~equal_to() {};

				bool operator() (const Key &a, const Key &b) const { return comparer(a,b); }
			};
	}
#endif

// We now have sgi::hash_map, std::map, google::sparse_hash_map and google::dense_hash_map to choose from

#ifdef HAVE_SPARSEHASH
#include <google/sparse_hash_map>
#include <google/dense_hash_map>

template <typename Key, typename T>
struct hash_map {
	typedef google::sparse_hash_map<Key,T> Type;
};

#else

template <typename Key, typename T>
struct hash_map {
	typedef sgi::hash_map<Key, T, sgi::hash<Key>, sgi::equal_to<Key> > Type;
};

#endif

class ECSessionManager;

class ECsStores : public ECsCacheEntry {
public:
	unsigned int	ulStore;
	GUID			guidStore;
	unsigned int	ulType;
};

class ECsUserObject : public ECsCacheEntry {
public:
	objectclass_t		ulClass;
	std::string			strExternId;
	unsigned int		ulCompanyId;
	std::string			strSignature;
};

/* same as objectid_t, join? */
typedef struct {
	objectclass_t		ulClass;
	std::string			strExternId;
} ECsUEIdKey;

inline bool operator < (ECsUEIdKey a, ECsUEIdKey b) {
	if (a.ulClass < b.ulClass)
		return true;
	if ((a.ulClass == b.ulClass) && a.strExternId < b.strExternId)
		return true;
	return false;
}

/* Intern Id cache */
class ECsUEIdObject : public ECsCacheEntry {
public:
	unsigned int		ulCompanyId;
	unsigned int		ulUserId;
	std::string			strSignature;
};

class ECsUserObjectDetails : public ECsCacheEntry {
public:
	objectdetails_t			sDetails;
};

class ECsServerDetails : public ECsCacheEntry {
public:
	serverdetails_t			sDetails;
};

class ECsObjects : public ECsCacheEntry {
public:
	unsigned int	ulParent;
	unsigned int	ulOwner;
	unsigned int	ulFlags;
	unsigned int	ulType;
};

class ECsQuota : public ECsCacheEntry {
public:
	quotadetails_t	quota;
};

class ECsIndexObject : public ECsCacheEntry {
public:
	inline bool operator==(const ECsIndexObject &other) const
	{
		if (ulObjId == other.ulObjId && ulTag == other.ulTag)
			return true;

		return false;
	}

	inline bool operator<(const ECsIndexObject &other) const
	{
		if(ulObjId < other.ulObjId)
			return true;
		else if(ulObjId == other.ulObjId && ulTag < other.ulTag)
			return true;

		return false;
	}

public:
	unsigned int ulObjId;
	unsigned int ulTag;
};

class ECsIndexProp : public ECsCacheEntry {
public:
    ECsIndexProp() : ECsCacheEntry() { 
		lpData = NULL; 
		ulTag = 0; 
		cbData = 0;
	}

	~ECsIndexProp() {
		if(lpData) {
			delete[] lpData;
		}
	}
    
    ECsIndexProp(const ECsIndexProp &src) {
        if(this == &src)
            return;
        Copy(&src, this);
    }
    
    ECsIndexProp& operator=(const ECsIndexProp &src) {
        if(this != &src) {
			Free();
			Copy(&src, this);
		}
		return *this;
    }

	// @todo check this function, is this really ok?
	inline bool operator<(const ECsIndexProp &other) const
	{

		if(cbData < other.cbData)
			return true;

		if(cbData == other.cbData) {
			if(lpData == NULL && other.lpData)
				return true;
			else if(lpData && other.lpData == NULL)
				return false;

			int c = memcmp(lpData, other.lpData, cbData);
			if(c < 0)
				return true;
			else if(c == 0 && ulTag < other.ulTag)
				return true;
		}

		return false;
	}

	inline bool operator==(const ECsIndexProp &other) const
	{

		if(cbData != other.cbData || ulTag != other.ulTag)
			return false;

		if (lpData == other.lpData)
			return true;

		if (lpData == NULL || other.lpData == NULL)
			return false;

		if(memcmp(lpData, other.lpData, cbData) == 0)
			return true;

		return false;
	}

	void SetValue(unsigned int ulTag, unsigned char* lpData, unsigned int cbData) {

		if(lpData == NULL|| cbData == 0)
			return;

		Free();

		this->lpData = new unsigned char[cbData];
		this->cbData = cbData;
		this->ulTag = ulTag;

		memcpy(this->lpData, lpData, (size_t)cbData);

	}
protected:
	void Free() {
		if(lpData) {
			delete[] lpData;
		}
		ulTag = 0; 
		cbData = 0;
		lpData = NULL;
	}

	void Copy(const ECsIndexProp* src, ECsIndexProp* dst) {
		if(src->lpData != NULL && src->cbData>0) {
			dst->lpData = new unsigned char[src->cbData];
			memcpy(dst->lpData, src->lpData, (size_t)src->cbData);
			dst->cbData = src->cbData;
		} else {
			dst->lpData = NULL;
			dst->cbData = 0;
		}

		dst->ulTag = src->ulTag;
	}
public:
	unsigned int	ulTag;
	unsigned char*	lpData;
	unsigned int	cbData;
};

class ECsCells : public ECsCacheEntry {
public:
    ECsCells() : ECsCacheEntry() { 
    	m_bComplete = false; 
#ifdef HAVE_SPARSEHASH    	
    	mapPropVals.set_deleted_key(-1); 
#endif    	
	};
    ~ECsCells() {
		std::map<unsigned int, struct propVal>::iterator i;
        for(i=mapPropVals.begin(); i!=mapPropVals.end(); i++) {
            FreePropVal(&i->second, false); 
        }
    };
    
    ECsCells(const ECsCells &src) {
        struct propVal val;
        std::map<unsigned int, struct propVal>::const_iterator i;
        for(i=src.mapPropVals.begin(); i!=src.mapPropVals.end(); i++) {
            CopyPropVal((struct propVal *)&i->second, &val);
            mapPropVals[i->first] = val;
        }
#ifdef HAVE_SPARSEHASH    	
    	mapPropVals.set_deleted_key(-1); 
#endif    	
        m_bComplete = src.m_bComplete;
    }
    
    ECsCells& operator=(const ECsCells &src) {
        struct propVal val;
        std::map<unsigned int, struct propVal>::iterator i;
        for(i=mapPropVals.begin(); i!=mapPropVals.end(); i++) {
            FreePropVal(&i->second, false); 
        }
        mapPropVals.clear();
        
        for(i=((ECsCells &)src).mapPropVals.begin(); i!=((ECsCells &)src).mapPropVals.end(); i++) {
            CopyPropVal((struct propVal *)&i->second, &val);
            mapPropVals[i->first] = val;
        }
        m_bComplete = src.m_bComplete;
		return *this;
    }
    
    // Add a property value for this object
    void AddPropVal(unsigned int ulPropTag, struct propVal *lpPropVal) {
        struct propVal val;
        ulPropTag = NormalizeDBPropTag(ulPropTag); // Only cache PT_STRING8
		std::pair<std::map<unsigned int, struct propVal>::iterator,bool> res;
        CopyPropVal(lpPropVal, &val, NULL, true);
        val.ulPropTag = NormalizeDBPropTag(val.ulPropTag);
		res = mapPropVals.insert(std::make_pair(ulPropTag, val));
		if (res.second == false) {
            FreePropVal(&res.first->second, false); 
            res.first->second = val;	// reassign
        }
    }
    
    // get a property value for this object
    bool GetPropVal(unsigned int ulPropTag, struct propVal *lpPropVal, struct soap *soap) {
        std::map<unsigned int, struct propVal>::iterator i;
        i = mapPropVals.find(NormalizeDBPropTag(ulPropTag));
        if(i == mapPropVals.end())
            return false;
        CopyPropVal(&i->second, lpPropVal, soap);
        if(NormalizeDBPropTag(ulPropTag) == lpPropVal->ulPropTag)
	        lpPropVal->ulPropTag = ulPropTag; // Switch back to requested type (not on PT_ERROR of course)
        return true;
    }
    
    // Updates a LONG type property
    void UpdatePropVal(unsigned int ulPropTag, int lDelta) {
        std::map<unsigned int, struct propVal>::iterator i;
        if(PROP_TYPE(ulPropTag) != PT_LONG)
            return;
        i = mapPropVals.find(ulPropTag);
        if(i == mapPropVals.end())
            return;
        i->second.Value.ul += lDelta;
    }
    
    // Updates a LONG type property
    void UpdatePropVal(unsigned int ulPropTag, unsigned int ulMask, unsigned int ulValue) {
        std::map<unsigned int, struct propVal>::iterator i;
        if(PROP_TYPE(ulPropTag) != PT_LONG)
            return;
        i = mapPropVals.find(ulPropTag);
        if(i == mapPropVals.end())
            return;
        i->second.Value.ul &= ~ulMask;
        i->second.Value.ul |= ulValue & ulMask;
    }
    
    void SetComplete(bool bComplete) {
        this->m_bComplete = bComplete;
    }
    
    bool GetComplete() {
        return this->m_bComplete;
    }

    // Gets the amount of memory used by this object    
    size_t GetSize() const {
        size_t ulSize = 0;
        
        std::map<unsigned int, struct propVal>::const_iterator i;
        for(i=mapPropVals.begin(); i!=mapPropVals.end(); i++) {
            switch(i->second.__union) {
                case SOAP_UNION_propValData_lpszA:
                    ulSize += i->second.Value.lpszA ? (unsigned int)strlen(i->second.Value.lpszA) : 0;
					break;
                case SOAP_UNION_propValData_bin:
                    ulSize += i->second.Value.bin ? i->second.Value.bin->__size + sizeof(i->second.Value.bin[0]) : 0;
					break;
                case SOAP_UNION_propValData_hilo:
                    ulSize += sizeof(i->second.Value.hilo[0]);
					break;
                default:
                    break;
            }
            
            ulSize += sizeof(std::map<unsigned int, struct propVal>::value_type);
        }
        ulSize += sizeof(*this);
        
        return ulSize;
    }
    
    // All properties for this object; propTag => propVal
    std::map<unsigned int, struct propVal> mapPropVals;
    
    bool m_bComplete;
};

class ECsACLs : public ECsCacheEntry {
public:
	ECsACLs() : ECsCacheEntry() { ulACLs = 0; aACL = NULL; }
    ECsACLs(const ECsACLs &src) {
        ulACLs = src.ulACLs;
        aACL = new ACL[src.ulACLs];
        memcpy(aACL, src.aACL, sizeof(ACL) * src.ulACLs);
    };
    ECsACLs& operator=(const ECsACLs &src) {
		if (this != &src) {
			if(aACL) delete [] aACL;
			ulACLs = src.ulACLs;
			aACL = new ACL[src.ulACLs];
			memcpy(aACL, src.aACL, sizeof(ACL) * src.ulACLs);
		}
		return *this;
    };
    ~ECsACLs() { if(aACL) delete [] aACL; }
    unsigned int 	ulACLs;
    struct ACL {
        unsigned int ulType;
        unsigned int ulMask;
        unsigned int ulUserId;
    } *aACL;
};

typedef struct {
	sObjectTableKey	sKey;
	unsigned int	ulPropTag;
}ECsSortKeyKey;


struct lessindexobjectkey {
	bool operator()(const ECsIndexObject& a, const ECsIndexObject& b) const
	{
		if(a.ulObjId < b.ulObjId)
			return true;
		else if(a.ulObjId == b.ulObjId && a.ulTag < b.ulTag)
			return true;

		return false;
	}

};

inline unsigned int IPRSHash(const ECsIndexProp& _Keyval1)
{
	unsigned int b    = 378551;
	unsigned int a    = 63689;
	unsigned int hash = 0;

	for(std::size_t i = 0; i < _Keyval1.cbData; i++)
	{
		hash = hash * a + _Keyval1.lpData[i];
			a    = a * b;
	}

	return hash;
}

namespace SGIBASE_NAMESPACE
{
	// hash function for type ECsIndexProp
	template<>
	class hash<ECsIndexProp> {
		public:
			hash() {};
			~hash() {};

			size_t operator() (const ECsIndexProp &value) const { return IPRSHash(value); }
	};

	// hash function for type ECsIndexObject
	template<>
	class hash<ECsIndexObject> {
		public:
			hash() {};
			~hash() {};
			size_t operator() (const ECsIndexObject &value) const {
					hash<unsigned int> hasher;
					// @TODO check the hash function!
					return hasher(value.ulObjId * value.ulTag ) ;
			}
	};

	// hash function for type 'unsigned long long'
	template<>
	class hash<unsigned long long> {
		public:
			hash() {};
			~hash() {};
			size_t operator() (const unsigned long long &value) const {
				hash<size_t> hasher;
				return hasher(static_cast<size_t>(value));	// @todo: use other half of hash on 32bit platforms.
			}
	};
}

typedef hash_map<unsigned int, ECsObjects>::Type ECMapObjects;
typedef hash_map<unsigned int, ECsStores>::Type ECMapStores;
typedef hash_map<unsigned int, ECsACLs>::Type ECMapACLs;
typedef hash_map<unsigned int, ECsQuota>::Type ECMapQuota;
typedef hash_map<unsigned int, ECsUserObject>::Type ECMapUserObject; // userid to user object
typedef std::map<ECsUEIdKey, ECsUEIdObject> ECMapUEIdObject; // user type + externid to user object
typedef hash_map<unsigned int, ECsUserObjectDetails>::Type ECMapUserObjectDetails; // userid to user object data
typedef std::map<std::string, ECsServerDetails> ECMapServerDetails;
typedef hash_map<unsigned int, ECsCells>::Type ECMapCells;

// Index properties
typedef std::map<ECsIndexObject, ECsIndexProp, lessindexobjectkey > ECMapObjectToProp;
typedef hash_map<ECsIndexProp, ECsIndexObject>::Type ECMapPropToObject;

#define CACHE_NO_PARENT 0xFFFFFFFF

class ECCacheManager  
{
public:
	ECCacheManager(ECConfig *lpConfig, ECDatabaseFactory *lpDatabase, ECLogger *lpLogger);
	virtual ~ECCacheManager();

	ECRESULT PurgeCache(unsigned int ulFlags);

	// These are read-through (ie they access the DB if they can't find the data)
	ECRESULT GetParent(unsigned int ulObjId, unsigned int *ulParent);
	ECRESULT GetOwner(unsigned int ulObjId, unsigned int *ulOwner);
	ECRESULT GetObject(unsigned int ulObjId, unsigned int *ulParent, unsigned int *ulOwner, unsigned int *ulFlags, unsigned int *ulType = NULL);
	ECRESULT SetObject(unsigned int ulObjId, unsigned int ulParent, unsigned int ulOwner, unsigned int ulFlags, unsigned int ulType);
	// Query cache only
	ECRESULT QueryParent(unsigned int ulObjId, unsigned int *ulParent);

	ECRESULT GetStore(unsigned int ulObjId, unsigned int *ulStore, GUID *lpGuid, unsigned int maxdepth = 100);
	ECRESULT GetStoreAndType(unsigned int ulObjId, unsigned int *ulStore, GUID *lpGuid, unsigned int *ulType, unsigned int maxdepth = 100);
	ECRESULT GetObjectFlags(unsigned int ulObjId, unsigned int *ulFlags);
	ECRESULT SetStore(unsigned int ulObjId, unsigned int ulStore, GUID *lpGuid, unsigned int ulType);

	ECRESULT GetServerDetails(const std::string &strServerId, serverdetails_t *lpsDetails);
	ECRESULT SetServerDetails(const std::string &strServerId, const serverdetails_t &sDetails);

	// Cache user table
	/* hmm, externid is in the signature as well, optimize? */
	ECRESULT GetUserObject(unsigned int ulUserId, objectid_t *lpExternId, unsigned int *lpulCompanyId, std::string *lpstrSignature);
	ECRESULT GetUserObject(const objectid_t &sExternId, unsigned int *lpulUserId, unsigned int *lpulCompanyId, std::string *lpstrSignature);
	ECRESULT GetUserObjects(const std::list<objectid_t> &lstExternObjIds, std::map<objectid_t, unsigned int> *lpmapLocalObjIds);

	// Cache user information
	ECRESULT GetUserDetails(unsigned int ulUserId, objectdetails_t *details);
	ECRESULT SetUserDetails(unsigned int ulUserId, objectdetails_t *details);

	ECRESULT GetACLs(unsigned int ulObjId, struct rightsArray **lppRights);
	ECRESULT SetACLs(unsigned int ulObjId, struct rightsArray *lpRights);

	ECRESULT GetQuota(unsigned int ulUserId, bool bIsDefaultQuota, quotadetails_t *quota);
	ECRESULT SetQuota(unsigned int ulUserId, bool bIsDefaultQuota, quotadetails_t quota);

	ECRESULT Update(unsigned int ulType, unsigned int ulObjId);
	ECRESULT UpdateUser(unsigned int ulUserId);

	ECRESULT GetEntryIdFromObject(unsigned int ulObjId, struct soap *soap, entryId* lpEntrId);
	ECRESULT GetEntryIdFromObject(unsigned int ulObjId, struct soap *soap, entryId** lppEntryId);
	ECRESULT GetObjectFromEntryId(entryId* lpEntrId, unsigned int* lpulObjId);
	ECRESULT SetObjectEntryId(entryId *lpEntryId, unsigned int ulObjId);
	ECRESULT GetEntryListToObjectList(struct entryList *lpEntryList, ECListInt* lplObjectList);
	ECRESULT GetEntryListFromObjectList(ECListInt* lplObjectList, struct soap *soap, struct entryList **lppEntryList);

	// Table data functions (pure cache functions, they will never access the DB themselves. Data must be provided through Set functions)
	ECRESULT GetCell(sObjectTableKey* lpsRowItem, unsigned int ulPropTag, struct propVal *lpDest, struct soap *soap, bool bComputed);
	ECRESULT SetCell(sObjectTableKey* lpsRowItem, unsigned int ulPropTag, struct propVal *lpSrc);
	ECRESULT UpdateCell(unsigned int ulObjId, unsigned int ulPropTag, int lDelta);
	ECRESULT UpdateCell(unsigned int ulObjId, unsigned int ulPropTag, unsigned int ulMask, unsigned int ulValue);
	ECRESULT SetComplete(unsigned int ulObjId);
	
	// Cache Index properties
	
	// Read-through
	ECRESULT GetPropFromObject(unsigned int ulTag, unsigned int ulObjId, struct soap *soap, unsigned int* lpcbData, unsigned char** lppData);
	ECRESULT GetObjectFromProp(unsigned int ulTag, unsigned int cbData, unsigned char* lpData, unsigned int* lpulObjId);
	
	ECRESULT RemoveIndexData(unsigned int ulObjId);
	ECRESULT RemoveIndexData(unsigned int ulPropTag, unsigned int cbData, unsigned char *lpData);
	ECRESULT RemoveIndexData(unsigned int ulPropTag, unsigned int ulObjId);
	
	// Read cache only
	ECRESULT QueryObjectFromProp(unsigned int ulTag, unsigned int cbData, unsigned char* lpData, unsigned int* lpulObjId);

	ECRESULT SetObjectProp(unsigned int ulTag, unsigned int cbData, unsigned char* lpData, unsigned int ulObjId);

	void ForEachCacheItem(void(callback)(const std::string &, const std::string &, const std::string &, void*), void *obj);
	ECRESULT DumpStats();
	
	// Cache list of properties indexed by zarafa-indexer
	ECRESULT GetIndexedProperties(std::map<unsigned int, std::string>& map);
	ECRESULT SetIndexedProperties(std::map<unsigned int, std::string>& map);

	// Test
	void DisableCellCache();
	void EnableCellCache();
	
private:
	// cache functions
	ECRESULT _GetACLs(unsigned int ulObjId, struct rightsArray **lppRights);
	ECRESULT _DelACLs(unsigned int ulObjId);
	
	ECRESULT _GetObject(unsigned int ulObjId, unsigned int *ulParent, unsigned int *ulOwner, unsigned int *ulFlags, unsigned int *ulType);
	ECRESULT _DelObject(unsigned int ulObjId);

	ECRESULT _GetStore(unsigned int ulObjId, unsigned int *ulStore, GUID *lpGuid, unsigned int *ulType);
	ECRESULT _DelStore(unsigned int ulObjId);

	ECRESULT _AddUserObject(unsigned int ulUserId, objectclass_t ulClass, unsigned int ulCompanyId,
							std::string strExternId, std::string strSignature);
	ECRESULT _GetUserObject(unsigned int ulUserId, objectclass_t* lpulClass, unsigned int *lpulCompanyId,
							std::string* lpstrExternId, std::string* lpstrSignature);
	ECRESULT _DelUserObject(unsigned int ulUserId);

	ECRESULT _AddUEIdObject(std::string strExternId, objectclass_t ulClass, unsigned int ulCompanyId,
							unsigned int ulUserId, std::string strSignature);
	ECRESULT _GetUEIdObject(std::string strExternId, objectclass_t ulClass, unsigned int *lpulCompanyId,
							unsigned int* lpulUserId, std::string* lpstrSignature);
	ECRESULT _DelUEIdObject(std::string strExternId, objectclass_t ulClass);

	ECRESULT _AddUserObjectDetails(unsigned int ulUserId, objectdetails_t *details);
	ECRESULT _GetUserObjectDetails(unsigned int ulUserId, objectdetails_t *details);
	ECRESULT _DelUserObjectDetails(unsigned int ulUserId);

	ECRESULT _DelCell(unsigned int ulObjId);

	ECRESULT _GetQuota(unsigned int ulUserId, bool bIsDefaultQuota, quotadetails_t *quota);
	ECRESULT _DelQuota(unsigned int ulUserId, bool bIsDefaultQuota);

	// Cache Index properties
	ECRESULT _AddIndexData(ECsIndexObject* lpObject, ECsIndexProp* lpProp);	

private:
	ECDatabaseFactory*	m_lpDatabaseFactory;
	ECLogger*			m_lpLogger;

	pthread_mutex_t		m_hCacheMutex;			// Store, Object, User, ACL, server cache
	pthread_mutex_t		m_hCacheCellsMutex;		// Cell cache
	pthread_mutex_t		m_hCacheIndPropMutex;	// Indexed properties cache
	
	// Quota cache, to reduce the impact of the user plugin
	// m_mapQuota contains user and company cache, except when it's the company user default quota
	// m_mapQuotaUserDefault contains company user default quota
	// this can't be in the same map, since the id is the same for "company" and "company user default"
	ECCache<ECMapQuota>			m_QuotaCache;
	ECCache<ECMapQuota>			m_QuotaUserDefaultCache;

	// Object cache, (hierarchy table)
	ECCache<ECMapObjects>		m_ObjectsCache;

	// Store cache
	ECCache<ECMapStores>		m_StoresCache;

	// User cache
	ECCache<ECMapUserObject>	m_UserObjectCache;
	ECCache<ECMapUEIdObject>	m_UEIdObjectCache;
	ECCache<ECMapUserObjectDetails>	m_UserObjectDetailsCache;

	// ACL cache
	ECCache<ECMapACLs>			m_AclCache;

	// Cell cache, include the column data of a loaded table
	ECCache<ECMapCells>			m_CellCache;
	
	// Server cache
	ECCache<ECMapServerDetails>	m_ServerDetailsCache;
	
	//Index properties
	ECCache<ECMapPropToObject>	m_PropToObjectCache;
	ECCache<ECMapObjectToProp>	m_ObjectToPropCache;
	
	// Properties from zarafa-indexer
	std::map<unsigned int, std::string> m_mapIndexedProperties;
	pthread_mutex_t				m_hIndexedPropertiesMutex;
	
	// Testing
	bool						m_bCellCacheDisabled;
};

#endif
