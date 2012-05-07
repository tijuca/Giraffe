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

// -*- Mode: c++ -*-
#ifndef PLUGIN_H
#define PLUGIN_H

// define to see which exception is thrown from a plugin
//#define EXCEPTION_DEBUG

#include "ECDefs.h"
#include "ZarafaUser.h"

#include <list>
#include <map>
#include <string>
#include <stdexcept>
#include <iostream>
#include <sstream>
#include <memory>
#include <pthread.h>
#include "ECPluginSharedData.h"
#include "IECStatsCollector.h"

/**
 * @defgroup userplugin Server user plugin
 * @{
 */

using namespace std;

/**
 * The objectsignature combines the object id with the
 * signature of that object. With the signature ECUserManagement
 * can determine if the object has been changed since the last
 * sync. This way ECUserManagement can cache things like the
 * object details without constantly accessing the plugin.
 */
class objectsignature_t {
public:
	/**
	 * Constructor for combining objectid and signature
	 *
	 * @param[in]	i
	 *					The unique objectid
	 * @param[in]	s
	 *					The object signature
	 */
    objectsignature_t(const objectid_t &i, const std::string &s) : id(i), signature(s) {};

	/**
	 * Default constructor, creates empty objectid with empty signature
	 */
    objectsignature_t() : id(), signature("") {};

	/**
	 * Object signature equality comparison
	 *
	 * @param[in]	sig
	 *					Compare the objectsignature_t sig with the current object.
	 *					Only the objectid will be used for equality, the signature
	 *					will be ignored.
	 * @return TRUE if the objects are equal
	 */
    bool operator== (const objectsignature_t &sig) { return id == sig.id; };

	/**
	 * Object signature less-then comparison
	 *
	 * @param[in]	sig
	 *					Compare the objectsignature_t sig with the current object.
	 *					Only the objectid will be used for the less-then comparison,
	 *					the signature will be ignored.
	 * @return TRUE if the current object is less then the sig object
	 */
    bool operator< (const objectsignature_t &sig) { return id.id < sig.id.id; };

	/**
	 * externid with objectclass
	 */
    objectid_t id;

	/**
	 * Signature.
	 * The exact contents of the signature depends on the plugin,
	 * when checking for changes only check if the signature is different.
	 */
    std::string signature;
};

typedef list<objectsignature_t> signatures_t;

typedef list<unsigned int> abprops_t;

class ECConfig;
class ECLogger;

/**
 * Main user plugin interface
 *
 * The user plugin interface defines methods for user management.
 * All functions should return auto_ptr<> to prevent expensive copy operations
 * for large amount of data.
 */
class UserPlugin {
public:
	/**
	 * Constructor
	 *
	 * @param[in]	pluginlock
	 *					The plugin mutex
	 * @param[in]	shareddata
	 *					The singleton shared plugin data.
	 * @throw std::exception
	 */
	UserPlugin(pthread_mutex_t *pluginlock, ECPluginSharedData *shareddata) :
		m_plugin_lock(pluginlock), m_config(NULL),
		m_logger(shareddata->GetLogger()),
		m_lpStatsCollector(shareddata->GetStatsCollector()),
		m_bHosted(shareddata->IsHosted()),
		m_bDistributed(shareddata->IsDistributed()) { };

	/**
	 * Destructor
	 */
	virtual ~UserPlugin() {};

	/**
	 * Initialize plugin
	 *
	 * @throw std::exception
	 */
	virtual void InitPlugin() throw(std::exception) = 0;

	/**
	 * Resolve name and company to objectsignature
	 *
	 * @param[in]	objclass
	 *					The objectclass of the name which should be resolved.
	 *					The objectclass can be partially unknown (OBJECTCLASS_UNKNOWN, MAILUSER_UNKNOWN, ...)
	 * @param[in]	name
	 *					The name which should be resolved
	 * @param[in]	company
	 *					The company beneath which the name should be searched
	 *					This objectid can be empty.
	 * @return The object signature of the resolved object
	 * @throw std::exception
	 */
	virtual objectsignature_t resolveName(objectclass_t objclass, const string &name, const objectid_t &company) throw(std::exception) = 0;

	/**
	 * Authenticate user with username and password
	 *
	 * @param[in]	username
	 *					The username of the user to be authenticated
	 * @param[in]	password
	 *					The password of the user to be authenticated
	 * @param[in]	company
	 *					The objectid of the company to which the user belongs.
	 *					This objectid can be empty.
	 * @return The objectsignature of the authenticated user
	 * @throw std::exception
	 */
	virtual objectsignature_t authenticateUser(const string &username, const string &password, const objectid_t &company) throw(std::exception) = 0;

	/**
	 * Request a list of objects for a particular company and specified objectclass.
	 *
	 * @param[in]	company
	 *					The company beneath which the objects should be listed.
	 *					This objectid can be empty.
	 * @param[in]	objclass
	 *					The objectclass of the objects which should be returned.
	 *					The objectclass can be partially unknown (OBJECTCLASS_UNKNOWN, MAILUSER_UNKNOWN, ...)
	 * @return The list of object signatures of all objects which were found
	 * @throw std::exception
	 */
	virtual auto_ptr<signatures_t> getAllObjects(const objectid_t &company, objectclass_t objclass) throw(std::exception) = 0;

	/**
	 * Obtain the object details for the given object
	 *
	 * @param[in]	objectid
	 *					The objectid for which is details are requested
	 * @return The objectdetails for the given objectid
	 * @throw std::exception
	 */
	virtual auto_ptr<objectdetails_t> getObjectDetails(const objectid_t &objectid) throw(std::exception) = 0;

	/**
	 * Obtain the object details for the given objects
	 *
	 * @param[in]	objectids
	 *					The list of object signatures for which the details are requested
	 * @return A map of objectid with the matching objectdetails
	 * @throw std::exception
	 */
	virtual auto_ptr<map<objectid_t, objectdetails_t> > getObjectDetails(const list<objectid_t> &objectids) throw(std::exception) = 0;

	/**
	 * Get all children for a parent for a given relation type.
	 * For example all users in a group
	 *
	 * @param[in]	relation
	 *					The relation type which connects the child and parent object
	 * @param[in]	parentobject
	 *					The parent object for which the children are requested
	 * @return A list of object signatures of the children of the parent.
	 * @throw std::exception
	 */
	virtual auto_ptr<signatures_t> getSubObjectsForObject(userobject_relation_t relation, const objectid_t &parentobject) throw(std::exception) = 0;

	/**
	 * Request all parents for a childobject for a given relation type.
	 * For example all groups for a user
	 *
	 * @param[in]	relation
	 *					The relation type which connects the child and parent object
	 * @param[in]	childobject
	 *					The childobject for which the parents are requested
	 * @return A list of object signatures of the parents of the child.
	 * @throw std::exception
	 */
	virtual auto_ptr<signatures_t> getParentObjectsForObject(userobject_relation_t relation, const objectid_t &childobject) throw(std::exception) = 0;

	/**
	 * Search for all objects which match the given string,
	 * the name and email address should be compared for this search.
	 *
	 * @param[in]	match
	 *					The string which should be found
	 * @param[in]	ulFlags
	 *					If EMS_AB_ADDRESS_LOOKUP the string must exactly match the name or email address
	 *					otherwise a partial match is allowed.
	 * @return List of object signatures which match the given string
	 * @throw std::exception
	 */
	virtual auto_ptr<signatures_t> searchObject(const string &match, unsigned int ulFlags) throw(std::exception) = 0;

	/**
	 * Obtain details for the public store
	 *
	 * @note This function is only mandatory for multi-server environments
	 *
	 * @return The public store details
	 * @throw std::exception
	 */
	virtual auto_ptr<objectdetails_t> getPublicStoreDetails() throw(std::exception) = 0;

	/**
	 * Obtain the objectdetails for a server
	 *
	 * @note This function is only mandatory for multi-server environments
	 *
	 * @param[in]	server
	 *					The externid of the server
	 * @return The server details
	 * @throw std::exception
	 */
	virtual auto_ptr<serverdetails_t> getServerDetails(const string &server) throw(std::exception) = 0;

	/**
	 * Update an object with new details
	 *
	 * @note It is not mandatory to implement this function
	 *
	 * @param[in]	id
	 *					The object id of the object which should be updated.
	 * @param[in]	details
	 *					The objectdetails which should be written to the object.
	 * @param[in]	lpRemove
	 *					List of configuration names which should be removed from the object
	 * @throw std::exception
	 */
	virtual void changeObject(const objectid_t &id, const objectdetails_t &details, const std::list<std::string> *lpRemove) throw(std::exception) = 0;

	/**
	 * Create object in plugin
	 *
	 * @note It is not mandatory to implement this function
	 *
	 * @param[in]	details
	 *					The object details of the new object.
	 * @return The objectsignature of the created object.
	 * @throw std::exception
	 */
	virtual objectsignature_t createObject(const objectdetails_t &details) throw(std::exception) = 0;

	/**
	 * Delete object from plugin
	 *
	 * @note It is not mandatory to implement this function
	 *
	 * @param[in]	id
	 *					The objectid which should be deleted
	 * @throw std::exception
	 */
	virtual void deleteObject(const objectid_t &id) throw(std::exception) = 0;

	/**
	 * Modify id of object in plugin
	 *
	 * @note It is not mandatory to implement this function
	 *
	 * @param[in]	oldId
	 *					The old objectid
	 * @param[in]	newId
	 *					The new objectid
	 * @throw std::exception
	 */
	virtual void modifyObjectId(const objectid_t &oldId, const objectid_t &newId) throw(std::exception) = 0;

	/**
 	 * Add relation between child and parent. This can be used
	 * for example to add a user to a group or add
	 * permission relations on companies.
	 *
	 * @note It is not mandatory to implement this function
	 *
	 * @param[in]	relation
	 *					The relation type which should connect the
	 *					child and parent.
	 * @param[in]	parentobject
	 *					The parent object.
	 * @param[in]	childobject
	 *					The child object.
	 * @throw std::exception
	 */
	virtual void addSubObjectRelation(userobject_relation_t relation,
									  const objectid_t &parentobject, const objectid_t &childobject) throw(std::exception) = 0;

	/**
	 * Delete relation between child and parent, this can be used
	 * for example to delete a user from a group or delete
	 * permission relations on companies.
	 *
	 * @note It is not mandatory to implement this function
	 *
	 * @param[in]	relation
	 *					The relation type which connected the
	 *					child and parent.
	 * @param[in]	parentobject
	 *					The parent object.
	 * @param[in]	childobject
	 *					The child object.
	 * @throw std::exception
	 */
	virtual void deleteSubObjectRelation(userobject_relation_t relation,
										 const objectid_t &parentobject, const objectid_t &childobject) throw(std::exception) = 0;
	
	/**
	 * Get quota information from object.
	 * There are two quota types, normal quota and userdefault quota,
	 * the first quota is the quote for the object itself while the userdefault
	 * quota can only be requested on containers (i.e. groups or companies) and
	 * is the quota for the members of that container.
	 *
	 * @param[in]	id
	 *					The objectid from which the quota should be read
	 * @param[in]	bGetUserDefault
	 *					Boolean to indicate if the userdefault quota must be requested.
	 * @throw std::exception
	 */
	virtual auto_ptr<quotadetails_t> getQuota(const objectid_t &id, bool bGetUserDefault) throw(std::exception) = 0;

	/**
	 * Set quota information on object
	 *
	 * @note It is not mandatory to implement this function
	 *
	 * @param[in]	id
	 *					The objectid which should be updated
	 * @param[in]	quotadetails
	 *					The quota information which should be written to the object
	 * @throw std::exception
	 */
	virtual void setQuota(const objectid_t &id, const quotadetails_t &quotadetails) throw(std::exception) = 0;

	/**
	 * Get extra properties which are set in the object details for the addressbook
	 *
	 * @note It is not mandatory to implement this function
	 *
	 * @return	a list of properties
	 * @throw std::exception
	 */
	virtual auto_ptr<abprops_t> getExtraAddressbookProperties() throw(std::exception) = 0;

	/**
	 * Reset entire plugin - use with care - this deletes (almost) all entries in the user database
	 *
	 * @param except The exception of all objects, which should NOT be deleted (usually the userid 
	 *               of the caller)
	 *
	 */
	virtual void removeAllObjects(objectid_t except) throw(std::exception) = 0;
	

protected:
	/**
	 * Pointer to pthread mutex
	 */
	pthread_mutex_t *m_plugin_lock;

	/**
	 * Pointer to local configuration manager.
	 */
	ECConfig *m_config;

	/**
	 * Pointer to logger
	 */
	ECLogger *m_logger;

	/**
	 * Pointer to statscollector
	 */
	IECStatsCollector *m_lpStatsCollector;

	/**
	 * Boolean to indicate if multi-company features are enabled
	 */
	bool m_bHosted;

	/**
	 * Boolean to indicate if multi-server features are enabled
	 */
	bool m_bDistributed;
};

/**
 * Exception which is thrown when no object was found during a search
 */
class objectnotfound: public runtime_error {
public:
	/**
	 * Constructor
	 *
	 * @param[in]	arg
	 *					The description why the exception was thrown
	 */
	objectnotfound(const string &arg) : runtime_error(arg) {
#ifdef EXCEPTION_DEBUG
		cerr << "objectnotfound exception: " << arg << endl;
#endif
	}
};

/**
 * Exception which is thrown when too many objects where returned in
 * a search.
 */
class toomanyobjects: public runtime_error {
public:
	/**
	 * Constructor
	 *
	 * @param[in]	arg
	 *					The description why the exception was thrown
	 */
	toomanyobjects(const string &arg) : runtime_error(arg) {
#ifdef EXCEPTION_DEBUG
		cerr << "toomanyobjects exception: " << arg << endl;
#endif
	}
};

/**
 * Exception which is thrown when an object is being created
 * while it already existed.
 */
class collision_error: public runtime_error {
public:
	/**
	 * Constructor
	 *
	 * @param[in]	arg
	 *					The description why the exception was thrown
	 */
	collision_error(const string &arg) : runtime_error(arg) {
#ifdef EXCEPTION_DEBUG
		cerr << "collision_error exception: " << arg << endl;
#endif
	}
};

/**
 * Exception which is thrown when a problem has been found with
 * the data read from the plugin backend.
 */
class data_error: public runtime_error {
public:
	/**
	 * Constructor
	 *
	 * @param[in]	arg
	 *					The description why the exception was thrown
	 */
	data_error(const string &arg) : runtime_error(arg) {
#ifdef EXCEPTION_DEBUG
		cerr << "data_error exception: " << arg << endl;
#endif
	}
};

/**
 * Exception which is thrown when the function was not
 * implemented by the plugin.
 */
class notimplemented: public runtime_error {
public:
	/**
	 * Constructor
	 *
	 * @param[in]	arg
	 *					The description why the exception was thrown
	 */
	notimplemented(const string &arg) : runtime_error(arg) {
#ifdef EXCEPTION_DEBUG
		cerr << "notimplemented exception: " << arg << endl;
#endif
	}
};

/**
 * Exception which is thrown when a function is called for
 * an unsupported feature. This can be because a multi-company
 * or multi-server function is called while this feature is
 * disabled.
 */
class notsupported : public runtime_error {
public:
	/**
	 * Constructor
	 *
	 * @param[in]	arg
	 *					The description why the exception was thrown
	 */
	notsupported(const string &arg) : runtime_error(arg) {
#ifdef EXCEPTION_DEBUG
		cerr << "notsupported exception: " << arg << endl;
#endif
	}
};

/**
 * Exception which is thrown when a user could not be logged in
 */
class login_error: public runtime_error {
public:
	/**
	 * Constructor
	 *
	 * @param[in]	arg
	 *					The description why the exception was thrown
	 */
	login_error(const string &arg) : runtime_error(arg) {
#ifdef EXCEPTION_DEBUG
		cerr << "login_error exception: " << arg << endl;
#endif
	}
};

/**
 * Exception which is thrown when LDAP returns errors
 */
class ldap_error: public runtime_error {
	int m_ldaperror;
public:
	/**
	 * Constructor
	 *
	 * @param[in]	arg
	 *					The description why the exception was thrown
	 * @param[in]	ldaperror
						The ldap error code why the exception was thrown
	 */
	ldap_error(const string &arg, int ldaperror=0) : runtime_error(arg) {
		m_ldaperror = ldaperror;
#ifdef EXCEPTION_DEBUG
		cerr << "ldap_error exception: " << arg << endl;
#endif
	}

	int GetLDAPError() {return m_ldaperror;}
};

/**
 * Convert string to objecttype as specified by template Tout
 *
 * @param[in]	s
 *					The string which should be converted.
 *					The type depends on template Tin.
 * @return The objecttype Tout which was converted from string
 */
template <class Tin, class Tout>
static inline Tout fromstring(const Tin &s) {
	istringstream i(s);
	Tout res;
	i >> res;
	return res;
}

/**
 * Convert input to string using ostringstream
 *
 * @param[in]	i
 *					Input which should be converted to string
 *					The type depends on template Tin.
 * @return The string representation of i
 */
template <class Tin>
static inline string tostring(const Tin i) {
	ostringstream o;
	o << i;
	return o.str();
}

/** @} */
#endif
