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

#include <pthread.h>
#include "LDAPCache.h"
#include "LDAPUserPlugin.h"
#include "stringutil.h"

LDAPCache::LDAPCache()
{
	pthread_mutexattr_init(&m_hMutexAttrib);
	pthread_mutexattr_settype(&m_hMutexAttrib, PTHREAD_MUTEX_RECURSIVE);

	pthread_mutex_init(&m_hMutex, &m_hMutexAttrib);

	m_lpCompanyCache = std::auto_ptr<dn_cache_t>(new dn_cache_t());
	m_lpGroupCache = std::auto_ptr<dn_cache_t>(new dn_cache_t());
	m_lpUserCache = std::auto_ptr<dn_cache_t>(new dn_cache_t());
	m_lpAddressListCache = std::auto_ptr<dn_cache_t>(new dn_cache_t());
}

LDAPCache::~LDAPCache()
{
	pthread_mutex_destroy(&m_hMutex);
	pthread_mutexattr_destroy(&m_hMutexAttrib);
}

bool LDAPCache::isObjectTypeCached(objectclass_t objclass)
{
	bool bCached = false;

	pthread_mutex_lock(&m_hMutex);

	switch (objclass) {
	case OBJECTCLASS_USER:
	case ACTIVE_USER:
	case NONACTIVE_USER:
	case NONACTIVE_ROOM:
	case NONACTIVE_EQUIPMENT:
	case NONACTIVE_CONTACT:
		bCached = !m_lpUserCache->empty();
		break;
	case OBJECTCLASS_DISTLIST:
	case DISTLIST_GROUP:
	case DISTLIST_SECURITY:
	case DISTLIST_DYNAMIC:
		bCached = !m_lpGroupCache->empty();
		break;
	case CONTAINER_COMPANY:
		bCached = !m_lpCompanyCache->empty();
		break;
	case CONTAINER_ADDRESSLIST:
		bCached = !m_lpAddressListCache->empty();
		break;
	default:
		break;
	}

	pthread_mutex_unlock(&m_hMutex);

	return bCached;
}

void LDAPCache::setObjectDNCache(objectclass_t objclass, std::auto_ptr<dn_cache_t> lpCache)
{
	/*
	 * Always merge caches rather then overwritting them.
	 */
	std::auto_ptr<dn_cache_t> lpTmp = getObjectDNCache(NULL, objclass);
	// cannot use insert() because it does not override existing entries
	for (dn_cache_t::iterator i = lpCache->begin(); i != lpCache->end(); i++)
		(*lpTmp)[i->first] = i->second;
	lpCache = lpTmp;

	pthread_mutex_lock(&m_hMutex);

	switch (objclass) {
	case OBJECTCLASS_USER:
	case ACTIVE_USER:
	case NONACTIVE_USER:
	case NONACTIVE_ROOM:
	case NONACTIVE_EQUIPMENT:
	case NONACTIVE_CONTACT:
		m_lpUserCache = lpCache;
		break;
	case OBJECTCLASS_DISTLIST:
	case DISTLIST_GROUP:
	case DISTLIST_SECURITY:
	case DISTLIST_DYNAMIC:
		m_lpGroupCache = lpCache;
		break;
	case CONTAINER_COMPANY:
		m_lpCompanyCache = lpCache;
		break;
	case CONTAINER_ADDRESSLIST:
		m_lpAddressListCache = lpCache;
		break;
	default:
		break;
	}

	pthread_mutex_unlock(&m_hMutex);
}

std::auto_ptr<dn_cache_t> LDAPCache::getObjectDNCache(LDAPUserPlugin *lpPlugin, objectclass_t objclass)
{
	std::auto_ptr<dn_cache_t> cache;

	pthread_mutex_lock(&m_hMutex);

	/* If item was not yet cached, make sure it is done now. */
	if (!isObjectTypeCached(objclass) && lpPlugin)
		lpPlugin->getAllObjects(objectid_t(), objclass); // empty company, so request all objects of type

	switch (objclass) {
	case OBJECTCLASS_USER:
	case ACTIVE_USER:
	case NONACTIVE_USER:
	case NONACTIVE_ROOM:
	case NONACTIVE_EQUIPMENT:
	case NONACTIVE_CONTACT:
		cache.reset(new dn_cache_t(*m_lpUserCache.get()));
		break;
	case OBJECTCLASS_DISTLIST:
	case DISTLIST_GROUP:
	case DISTLIST_SECURITY:
	case DISTLIST_DYNAMIC:
		cache.reset(new dn_cache_t(*m_lpGroupCache.get()));
		break;
	case CONTAINER_COMPANY:
		cache.reset(new dn_cache_t(*m_lpCompanyCache.get()));
		break;
	case CONTAINER_ADDRESSLIST:
		cache.reset(new dn_cache_t(*m_lpAddressListCache.get()));
		break;
	default:
		break;
	}

	pthread_mutex_unlock(&m_hMutex);

	return cache;
}

objectid_t LDAPCache::getParentForDN(const std::auto_ptr<dn_cache_t> &lpCache, const std::string &dn)
{
	objectid_t entry;
	std::string parent_dn;

	if (lpCache->empty())
		goto exit;

	// @todo make sure we find the largest DN match
	for (dn_cache_t::iterator it = lpCache->begin(); it != lpCache->end(); it++) {
		/* Key should be larger then current guess, but has to be smaller then the userobject dn */
		if (it->second.size() > parent_dn.size() && it->second.size() < dn.size()) {
			/* If key matches the end of the userobject dn, we have a positive match */
			if (stricmp(dn.c_str() + (dn.size() - it->second.size()), it->second.c_str()) == 0) {
				parent_dn = it->second;
				entry = it->first;
			}
		}
	}

exit:
	/* Either empty, or the correct result */
	return entry;
}

std::auto_ptr<dn_list_t> LDAPCache::getChildrenForDN(const std::auto_ptr<dn_cache_t> &lpCache, const std::string &dn)
{
	std::auto_ptr<dn_list_t> list = std::auto_ptr<dn_list_t>(new dn_list_t());

	/* Find al DN's which are hierarchically below the given dn */
	for (dn_cache_t::iterator iter = lpCache->begin(); iter != lpCache->end(); iter++) {
		/* Key should be larger then root DN */
		if (iter->second.size() > dn.size()) {
			/* If key matches the end of the root dn, we have a positive match */
			if (stricmp(iter->second.c_str() + (iter->second.size() - dn.size()), dn.c_str()) == 0)
				list->push_back(iter->second);
		}
	}

	return list;
}

std::string LDAPCache::getDNForObject(const std::auto_ptr<dn_cache_t> &lpCache, const objectid_t &externid)
{
	dn_cache_t::iterator it = lpCache->find(externid);
	if (it != lpCache->end())
		return it->second;
	return std::string();
}

bool LDAPCache::isDNInList(const std::auto_ptr<dn_list_t> &lpList, const std::string &dn)
{
	/* We were given an DN, check if a parent of that dn is listed as filterd */
	for (dn_list_t::iterator iter = lpList->begin(); iter != lpList->end(); iter++) {
		/* Key should be larger or equal then user DN */
		if (iter->size() <= dn.size()) {
			/* If key matches the end of the user dn, we have a positive match */
			if (stricmp(dn.c_str() + (dn.size() - iter->size()), iter->c_str()) == 0)
				return true;
		}
	}

	return false;
}
