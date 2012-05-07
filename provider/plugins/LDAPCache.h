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

#ifndef LDAPCACHE_H
#define LDAPCACHE_H

#include <memory>
#include <list>
#include <map>
#include <string>

#include "ECDefs.h"
#include "ZarafaUser.h"

class LDAPUserPlugin;

/**
 * @defgroup userplugin_ldap_cache LDAP user plugin cache
 * @ingroup userplugin_ldap
 * @{
 */


/**
 * Cache type, std::string is LDAP DN sting
 */
typedef std::map<objectid_t, std::string> dn_cache_t;
typedef std::list<std::string> dn_list_t;

/**
 * LDAP Cache which collects DNs with the matching
 * objectid and name.
 */
class LDAPCache {
private:
	/* Protect mutex from being overriden */
	pthread_mutex_t m_hMutex;
	pthread_mutexattr_t m_hMutexAttrib;

	std::auto_ptr<dn_cache_t> m_lpCompanyCache;		/* CONTAINER_COMPANY */
	std::auto_ptr<dn_cache_t> m_lpGroupCache;		/* OBJECTCLASS_DISTLIST */
	std::auto_ptr<dn_cache_t> m_lpUserCache;		/* OBJECTCLASS_USER */
	std::auto_ptr<dn_cache_t> m_lpAddressListCache; /* CONTAINER_ADDRESSLIST */

public:
	/**
	 * Default constructor
	 */
	LDAPCache();

	/**
	 * Destructor
	 */
	~LDAPCache();

	/**
	 * Check if the requested objclass class is cached.
	 *
	 * @param[in]	objclass
	 *					The objectclass which could be cached.
	 * @return TRUE if the object class is cached.
	 */
	bool isObjectTypeCached(objectclass_t objclass);

	/**
	 * Add new entries for the object cache.
	 *
	 * @param[in]	objclass
	 *					The objectclass which should be cached.
	 * @param[in]	lpCache
	 *					The data to add to the cache.
	 */
	void setObjectDNCache(objectclass_t objclass, std::auto_ptr<dn_cache_t> lpCache);

	/**
	 * Obtain the cached data
	 *
	 * @param[in]	lpPlugin
	 *					Pointer to the plugin, if the objectclass was not cached,
	 *					lpPlugin->getAllObjects() will be called to fill the cache.
	 * @param[in]	objclass
	 *						The objectclass for which the cache is requested
	 * @return The cache data
	 */
	std::auto_ptr<dn_cache_t> getObjectDNCache(LDAPUserPlugin *lpPlugin, objectclass_t objclass);

	/**
	 * Helper function: Search the cache for the direct parent for a DN.
	 * If the DN has multiple parents only the parent closest to the DN will be returned.
	 *
	 * @param[in]	lpCache
	 *					The cache which should be checked.
	 * @param[in]	dn
	 *					The DN for which the parent should be found.
	 * @return The cache entry of the parent. Contents will be empty if no parent was found.
	 */
	static objectid_t getParentForDN(const std::auto_ptr<dn_cache_t> &lpCache, const std::string &dn);

	/**
	 * Helper function: List all DNs which are hierarchially below the given DN.
	 *
	 * @param[in]	lpCache
	 *					The cache which should be checked.
	 * @param[in]	dn
	 *					The DN for which the children should be found
	 * @return The list of children for the DN
	 */
	static std::auto_ptr<dn_list_t> getChildrenForDN(const std::auto_ptr<dn_cache_t> &lpCache, const std::string &dn);

	/**
	 * Search the cache to obtain the DN for an object based on the object id.
	 *
	 * @param[in]	lpCache
	 *					The cache which should be checked.
	 * @param[in]	externid
	 *					The objectid which should be found in the cache
	 * @return the DN for the object id
	 */
	static std::string getDNForObject(const std::auto_ptr<dn_cache_t> &lpCache, const objectid_t &externid);

	/**
	 * Check if the given DN is present in the DN list
	 *
	 * @param[in]	lpList
	 *					The list which should be checked
	 * @param[in]	dn
	 *					The DN which should be found in the list
	 * @return TRUE if the DN is found in the list
	 */
	static bool isDNInList(const std::auto_ptr<dn_list_t> &lpList, const std::string &dn);
};

/** @} */

#endif /* LDAPCACHE_H */
