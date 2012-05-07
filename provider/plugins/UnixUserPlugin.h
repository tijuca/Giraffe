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
#ifndef UNIXUSERPLUGIN_H
#define UNIXUSERPLUGIN_H

#include <stdexcept>
#include <string>
#include <ECIConv.h>

#include "plugin.h"
#include "DBBase.h"

/**
 * @defgroup userplugin_unix UNIX user plugin
 * @ingroup userplugin
 * @{
 */

/**
 * UNIX user plugin
 *
 * User management based on Unix.  It uses /etc/passwd for the
 * loginname, fullname; and /etc/shadow for the password.  Because it
 * needs to read /etc/shadow the server needs to run as a member of
 * the shadow group.  When password updates need to be done, root
 * privileges are required.
 *
 * Extra attributes, such as email addresses, are stored in the objectproperty
 * tables, which are always present. It's exactly the same as the DBUserPlugin.
 */
class UnixUserPlugin: public DBPlugin {
public:
    /**
	 * Constructor
	 *
	 * @param[in]	pluginlock
	 *					The plugin mutex
	 * @param[in]   lpSharedData
	 *					The singleton shared plugin data.
	 * @throw runtime_error When configuration file could not be loaded
	 * @throw notsupported When multi-server or multi-company support is enabled.
	 */
	UnixUserPlugin(pthread_mutex_t *pluginlock, ECPluginSharedData *lpSharedData) throw(std::exception);

    /**
	 * Destructor
	 */
	virtual ~UnixUserPlugin();

    /**
	 * Initialize plugin
	 */
	void InitPlugin() throw(std::exception);

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
	 * @throw toomanyobjects When more then one object was found.
	 * @throw objectnotfound When no object was found.
	 * @throw runtime_error When an unsupported objectclass was requested.
	 */
	virtual objectsignature_t resolveName(objectclass_t objclass, const string &name, const objectid_t &company) throw(std::exception);

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
	 * @throw objectnotfound When no user with the given name exists.
	 * @throw login_error When the wrong password is provied or the user is nonactive.
	 */
	virtual objectsignature_t authenticateUser(const string &username, const string &password, const objectid_t &company) throw(std::exception);

    /**
	 * Request a list of objects for a particular company and specified objectclass.
	 *
	 * @param[in]	company
	 *					The company beneath which the objects should be listed.
	 *					This objectid can be empty.
	 * @param[in]   objclass
	 *					The objectclass of the objects which should be returned.
	 *					The objectclass can be partially unknown (OBJECTCLASS_UNKNOWN, MAILUSER_UNKNOWN, ...)
	 * @return The list of object signatures of all objects which were found
	 * @throw std::exception
	 */
	virtual auto_ptr<signatures_t> getAllObjects(const objectid_t &company, objectclass_t objclass) throw(std::exception);

	/**
	 * Obtain the object details for the given object
	 *
	 * Besides the information from Unix, the details will also
	 * be merged with details from DBPlugin::getObjectDetails()
	 *
	 * @param[in]	objectid
	 *					The objectid for which is details are requested
	 * @return The objectdetails for the given objectid
	 * @throw std::exception
	 */
	virtual auto_ptr<objectdetails_t> getObjectDetails(const objectid_t &objectid) throw (std::exception);

    /**
	 * Obtain the object details for the given objects
	 *
	 * This will loop through the list of objectids and call
	 * UnixUserPlugin::getObjectDetails(const objectid_t &objectid) for each objectid.
	 *
	 * @param[in]	objectids
	 *					The list of object signatures for which the details are requested
	 * @return A map of objectid with the matching objectdetails
	 * @throw std::exception
	 */
	virtual auto_ptr<map<objectid_t, objectdetails_t> > getObjectDetails(const list<objectid_t> &objectids) throw(std::exception);

    /**
	 * Get all children for a parent for a given relation type.
	 * For example all users in a group
	 *
	 * OBJECTRELATION_GROUP_MEMBER will be queried to Unix directly, for
	 * all other relations DBPlugin::getSubObjectsForObject() is called.
	 * 
	 * @param[in]	relation
	 *					The relation type which connects the child and parent object
	 * @param[in]	parentobject
	 *					The parent object for which the children are requested
	 * @return A list of object signatures of the children of the parent.
	 * @throw std::exception
	 */
	virtual auto_ptr<signatures_t> getSubObjectsForObject(userobject_relation_t relation, const objectid_t &parentobject) throw(std::exception);

    /**
	 * Request all parents for a childobject for a given relation type.
	 * For example all groups for a user
	 *
	 * OBJECTRELATION_GROUP_MEMBER will be queried to Unix directly, for
	 * all other relations DBPlugin::getParentObjectsForObject() is called.
	 *
	 * @param[in]	relation
	 *					The relation type which connects the child and parent object
	 * @param[in]	childobject
	 *					The childobject for which the parents are requested
	 * @return A list of object signatures of the parents of the child.
	 * @throw std::exception
	 */
	virtual auto_ptr<signatures_t> getParentObjectsForObject(userobject_relation_t relation, const objectid_t &childobject) throw(std::exception);

	/**
	 * Search for all objects which match the given string,
	 * the name and email address should be compared for this search.
	 *
	 * Besides checking Unix information, the email address is also checked by calling
	 * DBPlugin::searchObjects().
	 *
	 * @param[in]	match
	 *					The string which should be found
	 * @param[in]	ulFlags
	 *					If EMS_AB_ADDRESS_LOOKUP the string must exactly match the name or email address
	 *					otherwise a partial match is allowed.
	 * @return List of object signatures which match the given string
	 * @throw objectnotfound When no object was found
	 */
	virtual auto_ptr<signatures_t> searchObject(const string &match, unsigned int ulFlags) throw(std::exception);

    /**
	 * Obtain details for the public store
	 *
	 * @note This function has not been implemented and will always throw an exception.
	 *
	 * @return The public store details
	 * @throw notsupported Always when this function is called
	 */
	virtual auto_ptr<objectdetails_t> getPublicStoreDetails() throw(std::exception);

	/**
	 * Obtain the objectdetails for a server
	 *
	 * @note This function has not been implemented and will always throw an exception.
	 *
	 * @param[in]	server
	 *					The externid of the server
	 * @return The server details
	 * @throw notsupported Always when this function is called
	 */
	virtual auto_ptr<serverdetails_t> getServerDetails(const string &server) throw(std::exception);
	
	/**
	 * Create object in plugin
	 *
	 * @note This function has not been implemented and will always throw an exception.
	 *
	 * @param[in]	details
	 *					The object details of the new object.
	 * @return The objectsignature of the created object.
	 * @throw notimplemented Always when this function is called
	 */
	virtual objectsignature_t createObject(const objectdetails_t &details) throw(std::exception);

	/**
	 * Update an object with new details
	 *
	 * Does not support changing OB_PROP_S_PASSWORD, OB_PROP_S_LOGIN and OB_PROP_S_FULLNAME,
	 * all other details are updated by calling DBPlugin::changeObject().
	 *
     * @param[in]	id
	 *					The object id of the object which should be updated.
	 * @param[in]	details
	 *					The objectdetails which should be written to the object.
	 * @param[in]	lpRemove
	 *					List of configuration names which should be removed from the object
	 * @throw runtime_error When OB_PROP_S_PASSWORD, OB_PROP_S_LOGIN, OB_PROP_S_FULLNAME or is non-empty.
	 */
	virtual void changeObject(const objectid_t &id, const objectdetails_t &details, const std::list<std::string> *lpRemove) throw(std::exception);

    /**
	 * Delete object from plugin
	 *
	 * @note This function has not been implemented and will always throw an exception.
	 *
	 * @param[in]	id
	 *					The objectid which should be deleted
	 * @throw notimplemented Always when this function is called
	 */
	virtual void deleteObject(const objectid_t &id) throw(std::exception);

	/**
	 * Modify id of object in plugin
	 *
	 * @note This function is not supported by this plugin and will always throw an exception
	 *
	 * @param[in]	oldId
	 *					The old objectid
	 * @param[in]	newId
	 *					The new objectid
	 * @throw notsupported Always when this function is called
	 */
	virtual void modifyObjectId(const objectid_t &oldId, const objectid_t &newId) throw(std::exception);

	/**
	 * Add relation between child and parent. This can be used
	 * for example to add a user to a group or add
	 * permission relations on companies.
	 *
	 * Only OBJECTRELATION_QUOTA_USERRECIPIENT and OBJECTRELATION_USER_SENDAS
	 * are supported. All other relation types are forwarded to
	 * DBPlugin::addSubObjectRelation()
	 *
	 * @param[in]	relation
	 *					The relation type which should connect the child and parent.
	 * @param[in]	parentobject
	 *					The parent object.
	 * @param[in]	childobject
	 *					The child object.
	 * @throw notimplemented when an unsupported relation is requested
	 */	 
	virtual void addSubObjectRelation(userobject_relation_t relation,
									  const objectid_t &parentobject, const objectid_t &childobject) throw(std::exception);

	/**
	 * Delete relation between child and parent, this can be used
	 * for example to delete a user from a group or delete
	 * permission relations on companies.
	 *
	 * Only OBJECTRELATION_QUOTA_USERRECIPIENT and OBJECTRELATION_USER_SENDAS
	 * are supported. All other relation types are forwarded to
	 * DBPlugin::deleteSubObjectRelation().
	 *
	 * @param[in]	relation
	 *					The relation type which connected the child and parent.
	 * @param[in]	parentobject
	 *					The parent object.
	 * @param[in]	childobject
	 *					The child object.
	 * @throw notimplemented when an unsupported relation is requested
	 */
	virtual void deleteSubObjectRelation(userobject_relation_t relation,
										 const objectid_t &parentobject, const objectid_t &childobject) throw(std::exception);

private:
	ECIConv *m_iconv, *m_iconvrev;

	/**
	 * Find a user with specific name
	 *
	 * @param[in]	id
	 *					The id of the user which must be found
	 * @param[out]	pwd
	 *					The found passwd structure
	 * @param[out]	buffer
	 *					A buffer which will contain the strings for pwd
	 * @throw objectnotfound If no user was found.
	 */
	void findUserID(const string &id, struct passwd *pwd, char *buffer) throw(std::exception);

	/**
	 * Find a user with specific name
	 *
	 * @param[in]	name
	 *					The name of the user which must be found
	 * @param[out]	pwd
	 *					The found passwd structure
	 * @param[out]	buffer
	 *					A buffer which will contain the strings for pwd
	 * @throw objectnotfound If no user was found.
	 */
	void findUser(const string &name, struct passwd *pwd, char *buffer) throw(std::exception);

	/**
	 * Find a group with specific ID
	 *
	 * @param[in]	id
	 *					The id of the group which must be found
	 * @param[out]	grp
	 *					The found group structure
	 * @param[out]	buffer
	 *					A buffer which will contain the strings for grp
	 * @throw objectnotfound If no group was found.
	 */
	void findGroupID(const string &id, struct group *grp, char *buffer) throw(std::exception);

	/**
	 * Find a group with specific name
	 *
	 * @param[in]	name
	 *					The name of the group which must be found
	 * @param[out]	grp
	 *					The found group structure
	 * @param[out]	buffer
	 *					A buffer which will contain the strings for grp
	 * @throw objectnotfound If no group was found.
	 */
	void findGroup(const string &name, struct group *grp, char *buffer) throw(std::exception);

	/**
	 * Resolve user name to objectsignature
	 *
	 * This will call UnixUserPlugin::findUser()
	 *
	 * @param[in]	name
	 *					Name of the user which must be found
	 * @return The objectsignature of the resolved object
	 * @throw std::exception
	 */
	objectsignature_t resolveUserName(const string &name) throw(std::exception);

	/**
	 * Resolve group name to objectsignature
	 *
	 * This will call UnixUserPlugin::findGroup()
	 *
	 * @param[in]	name
	 *					Name of the group which must be found
	 * @return The objectsignature of the resolved object
	 * @throw std::exception
	 */
	objectsignature_t resolveGroupName(const string &name) throw(std::exception);

	/**
	 * Match a user with given search query
	 *
	 * @param[in]	pw
	 *					The user which should be checked
	 * @param[in]	match
	 *					Check if the term matches the user pw
	 * @param[in]	ulFlags
	 *					If EMS_AB_ADDRESS_LOOKUP the string must exactlymatch the name or
	 *					email address otherwise a partial match is allowed.
	 * @return True if the user matches the query
	 */
	bool matchUserObject(struct passwd *pw, const string &match, unsigned int ulFlags);

	/**
	 * Match a group with given search query
	 *
	 * @param[in]	gr
	 *					The group which should be checked
	 * @param[in]	match
	 *					Check if the term matches the group gr
	 * @param[in]	ulFlags
	 *					If EMS_AB_ADDRESS_LOOKUP the string must exactlymatch the name or
	 *					email address otherwise a partial match is allowed.
	 * @return True if the group matches the query
	 */
	bool matchGroupObject(struct group *gr, const string &match, unsigned int ulFlags);

	/**
	 * Create a list containing all users which optionally match the search term.
	 *
	 * @param[in]	match
	 *					Optional parameter. Search for all users which match the term
	 * @param[in]	ulFlags
	 *					Optional parameter. If EMS_AB_ADDRESS_LOOKUP the string must exactly
	 *					match the name or email address otherwise a partial match is allowed.
	 * @return List of objectsignatures
	 */
	auto_ptr<signatures_t> getAllUserObjects(const string &match = string(), unsigned int ulFlags = 0) throw(std::exception);

	/**
	 * Create a list containing all groups which optionally match the search term.
	 *
	 * @param[in]	match
	 *					Optional parameter. Search for all groups which match the term
	 * @param[in]	ulFlags
	 *					Optional parameter. If EMS_AB_ADDRESS_LOOKUP the string must exactly
	 *					match the name or email address otherwise a partial match is allowed.
	 * @return List of objectsignatures
	 */
	auto_ptr<signatures_t> getAllGroupObjects(const string &match = string(), unsigned int ulFlags = 0) throw(std::exception);

	/**
	 * Copy object details from struct passwd to objectdetails
	 *
	 * @param[in]	pw
	 *					Pointer to struct pw from which the details must be collected
	 * @return The objectdetails which were collected from pw
	 */
	auto_ptr<objectdetails_t> objectdetailsFromPwent(struct passwd *pw); // PAM part

	/**
	 * Copy object details from struct group to objectdetails
	 *
	 * @param[in]	gr
	 *					Pointer to struct group from which the details must be collected
	 * @return The objectdetails which were collected from gr
	 */
	auto_ptr<objectdetails_t> objectdetailsFromGrent(struct group *gr);

	/**
	 * Query the Database to obtain the signature for the objectid.
	 *
	 * @param[in]	id
	 *					The object for which the signature is requested
	 * @return the object signature
	 */
	std::string	getDBSignature(const objectid_t &id);

	/**
	 * Check errno for errors which should trigger an exception
	 *
	 * @param[in]	user
	 *					The username for which the exception will be thrown.
	 * @throw runtime_error Thrown when errno was set.
	 */
	void errnoCheck(const string &user);
};

extern "C" {
	extern UserPlugin* getUserPluginInstance(pthread_mutex_t*, ECPluginSharedData*);
	extern void deleteUserPluginInstance(UserPlugin*);
	extern int getUserPluginVersion();
}
/** @} */
#endif
