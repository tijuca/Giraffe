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

#ifndef ECCACHE_INCLUDED
#define ECCACHE_INCLUDED

#include <list>
#include <string>
#include <assert.h>

class ECLogger;

template<typename Key>
class KeyEntry {
public:
	Key key;
	time_t ulLastAccess;
};

template<typename Key>
bool KeyEntryOrder(const KeyEntry<Key> &a, const KeyEntry<Key> &b) {
	return a.ulLastAccess < b.ulLastAccess;
}

template<typename Value>
unsigned int GetCacheAdditionalSize(const Value &val) {
	return 0;
}

class ECsCacheEntry {
public:
	ECsCacheEntry() { ulLastAccess = 0; }

	time_t 	ulLastAccess;
};

class ECCacheBase
{
public:
	typedef unsigned long		count_type;
	typedef unsigned long long	size_type;

	virtual ~ECCacheBase();

	virtual count_type ItemCount() const = 0;
	virtual size_type Size() const = 0;
	
	size_type MaxSize() const { return m_ulMaxSize; }
	long MaxAge() const { return m_lMaxAge; }
	size_type HitCount() const { return m_ulCacheHit; }
	size_type ValidCount() const { return m_ulCacheValid; }

	// Decrement the valid count. Used from ECCacheManger::GetCell.
	void DecrementValidCount() { 
		assert(m_ulCacheValid >= 1);
		m_ulCacheValid--;
	}

	// Call the provided callback with some statistics.
	void RequestStats(void(callback)(const std::string &, const std::string &, const std::string &, void*), void *obj);

	// Dump statistics to the provided logger.
	void DumpStats(ECLogger *lpLogger);

protected:
	ECCacheBase(const std::string &strCachename, size_type ulMaxSize, long lMaxAge);
	void IncrementHitCount() { m_ulCacheHit++; }
	void IncrementValidCount() { m_ulCacheValid++; }
	void ClearCounters() { m_ulCacheHit = m_ulCacheValid = 0; }

private:
	const std::string	m_strCachename;
	const size_type		m_ulMaxSize;
	const long			m_lMaxAge;
	size_type			m_ulCacheHit;
	size_type			m_ulCacheValid;
};


template<typename _MapType>
class ECCache : public ECCacheBase
{
public:
	typedef typename _MapType::key_type		key_type;
	typedef typename _MapType::mapped_type	mapped_type;
	
	ECCache(const std::string &strCachename, size_type ulMaxSize, long lMaxAge)
		: ECCacheBase(strCachename, ulMaxSize, lMaxAge)
		, m_ulSize(0)
	{ }
	
	ECRESULT ClearCache()
	{
		m_map.clear();
		m_ulSize = 0;
		ClearCounters();
		return erSuccess;
	}
	
	count_type ItemCount() const
	{
		return m_map.size();
	}
	
	size_type Size() const
	{
		return (m_map.size() * sizeof(typename _MapType::value_type) + 64) + m_ulSize;
	}

	ECRESULT RemoveCacheItem(const key_type &key) 
	{
		ECRESULT er = erSuccess;
		typename _MapType::iterator iter;

		iter = m_map.find(key);
		if(iter == m_map.end()) {
			er = ZARAFA_E_NOT_FOUND;
			goto exit;
		}

		m_ulSize -= GetCacheAdditionalSize(iter->second);
		m_map.erase(iter);

	exit:
		return er;
	}
	
	ECRESULT GetCacheItem(const key_type &key, mapped_type **lppValue)
	{
		ECRESULT er = erSuccess;
		time_t	tNow  = GetProcessTime();
		typename _MapType::iterator iter;
		typename _MapType::iterator iterDelete;

		iter = m_map.find(key);
		
		if (iter != m_map.end()) {
			// Cache age of the cached item, if expired remove the item from the cache
			if (MaxAge() != 0 && (long)(tNow - iter->second.ulLastAccess) >= MaxAge()) {

				// Loop through all items and check
				for (iter = m_map.begin(); iter != m_map.end();) {
					if ((long)(tNow - iter->second.ulLastAccess) >= MaxAge()) {
						iterDelete = iter;
						iter++;
						m_map.erase(iterDelete);
					} else {
						iter++;
					}
				}
				er = ZARAFA_E_NOT_FOUND;
			} else {
				*lppValue = &iter->second;
				// If we have an aging cache, we don't update the timestamp,
				// so we can't keep a value longer in the cache than the max age.
				// If we have a non-aging cache, we need to update it,
				// to see the oldest 5% to purge from the cache.
				if (MaxAge() == 0)
					iter->second.ulLastAccess = tNow;
				er = erSuccess;
			}
		} else {
			er = ZARAFA_E_NOT_FOUND;
		}

		IncrementHitCount();
		if (er == erSuccess)
			IncrementValidCount();

		return er;
	}

	ECRESULT GetCacheRange(const key_type &lower, const key_type &upper, std::list<typename _MapType::value_type> *values)
	{
		typedef typename _MapType::iterator iterator;

		iterator iLower = m_map.lower_bound(lower);
		iterator iUpper = m_map.upper_bound(upper);
		for (iterator i = iLower; i != iUpper; ++i)
			values->push_back(*i);

		return erSuccess;
	}
	
	ECRESULT AddCacheItem(const key_type &key, const mapped_type &value)
	{
		typedef typename _MapType::value_type value_type;
		typedef typename _MapType::iterator iterator;
		std::pair<iterator,bool> result;

		if (MaxSize() == 0)
			return erSuccess;

		result = m_map.insert(value_type(key, value));

		if (result.second == false) {
			// The key already exists but its value is unmodified. So update it now
			m_ulSize += (int)(GetCacheAdditionalSize(value) - GetCacheAdditionalSize(result.first->second));
			result.first->second = value;
			result.first->second.ulLastAccess = GetProcessTime();
			// Since there is a very small chance that we need to purge the cache, we're skipping that here.
		} else {
			// We just inserted a new entry.
			m_ulSize += GetCacheAdditionalSize(value);
			
			result.first->second.ulLastAccess = GetProcessTime();
			
			UpdateCache(0.05F);
		}

		return erSuccess;
	}
	
	// Used in ECCacheManager::SetCell, where the content of a cache item is modified.
	ECRESULT AddToSize(long long ulSize)
	{
		m_ulSize += ulSize;
		return erSuccess;
	}

private:
	ECRESULT PurgeCache(float ratio)
	{
		std::list<KeyEntry<typename _MapType::iterator> > lstEntries;
		typename std::list<KeyEntry<typename _MapType::iterator> >::iterator iterEntry;
		typename _MapType::iterator iterMap;

		for(iterMap = m_map.begin(); iterMap != m_map.end(); iterMap++) {
			KeyEntry<typename _MapType::iterator> k;
			k.key = iterMap;
			k.ulLastAccess = iterMap->second.ulLastAccess;

			lstEntries.push_back(k);
		}

		lstEntries.sort(KeyEntryOrder<typename _MapType::iterator>);

		// We now have a list of all cache items, sorted by access time, (oldest first)
		unsigned int ulDelete = (unsigned int)(m_map.size() * ratio);

		// Remove the oldest ulDelete entries from the cache, removing [ratio] % of all
		// cache entries.
		for (iterEntry = lstEntries.begin(); iterEntry != lstEntries.end() && ulDelete > 0; ++iterEntry, --ulDelete) {
			iterMap = iterEntry->key;

			m_ulSize -= GetCacheAdditionalSize(iterMap->second);
			m_map.erase(iterMap);
		}

		return erSuccess;
	}
	
	ECRESULT UpdateCache(float ratio)
	{
		if( Size() > MaxSize()) {
			PurgeCache(ratio);
		}

		return erSuccess;
	}

private:
	_MapType			m_map;	
	size_type			m_ulSize;
};

#endif // ndef ECCACHE_INCLUDED
