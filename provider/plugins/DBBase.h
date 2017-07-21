/*
 * Copyright 2005 - 2016 Zarafa and its licensors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License, version 3,
 * as published by the Free Software Foundation.
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
#ifndef __DBBASE_H
#define __DBBASE_H

#include <kopano/zcdefs.h>
#include "plugin.h"
#include <memory>
#include <mutex>
#include <string>

#include "ECDatabase.h"
#include <kopano/ECDefs.h>

namespace KC {

/**
 * @defgroup userplugin_dbbase Database common for user plugins
 * @ingroup userplugin
 * @{
 */


// Table names
#define DB_OBJECT_TABLE				"object"
#define DB_OBJECTPROPERTY_TABLE		"objectproperty"
#define DB_OBJECTMVPROPERTY_TABLE	"objectmvproperty"
#define DB_OBJECT_RELATION_TABLE	"objectrelation"

// Object properties
#define OP_MODTIME			"modtime"
#define OP_LOGINNAME		"loginname" 
#define OP_PASSWORD			"password"
#define OP_ISADMIN			"isadmin"
#define OB_AB_HIDDEN		"ishidden"
#define OP_FULLNAME			"fullname"
#define OP_FIRSTNAME		"firstname"
#define OP_EMAILADDRESS		"emailaddress"
#define OP_HARDQUOTA		"hardquota"
#define OP_SOFTQUOTA		"softquota"
#define OP_WARNQUOTA		"warnquota"
#define OP_USEDEFAULTQUOTA	"usedefaultquota"
#define OP_UD_HARDQUOTA		"userhardquota"
#define OP_UD_SOFTQUOTA		"usersoftquota"
#define OP_UD_WARNQUOTA		"userwarnquota"
#define OP_UD_USEDEFAULTQUOTA	"userusedefaultquota"
#define OP_GROUPNAME		"groupname"
#define OP_COMPANYID		"companyid"
#define OP_COMPANYNAME		"companyname"
#define OP_COMPANYADMIN		"companyadmin"

/**
 * Memory wrapper class for database results
 *
 * Wrapper class around DB_RESULT which makes
 * sure the DB_RESULT memory is freed when
 * the the object goes out of scope.
 *
 * @todo move the class DB_RESULT_AUTOFREE to a common place
 */
class DB_RESULT_AUTOFREE _kc_final {
public:
	/**
	 * @param[in]	lpDatabase
	 *					The database to which the result belongs
	 */
    DB_RESULT_AUTOFREE(ECDatabase *lpDatabase) {
        m_lpDatabase = lpDatabase;
    };

	/**
	 * Cast DB_RESULT_AUTOFREE to DB_RESULT
	 */
	operator DB_RESULT &(void) { return m_lpResult; }

	/**
	 * Obtain reference to DB_RESULT
	 * This will free the existing result before
	 * returning the reference to the empty result.
	 *
	 * @return Pointer to DB_RESULT
	 */
    DB_RESULT * operator & () {
        // Assume overwrite will happen soon
        m_lpResult = DB_RESULT();
        return &m_lpResult;
    };

private:
    DB_RESULT		m_lpResult;
    ECDatabase *	m_lpDatabase;
};

/**
 * User managment helper class for database access
 * 
 * This class implements most of the Database Access functions
 * which can be shared between the DB and Unix plugin.
 */
class DBPlugin : public UserPlugin {
public:
	/**
	 * @param[in]	pluginlock
	 *					The plugin mutex
	 * @param[in]	shareddata
	 *					The singleton shared plugin data.
	 * @throw std::exception
	 */
	DBPlugin(std::mutex &, ECPluginSharedData *shareddata);

	/**
	 * Initialize plugin
	 *
	 * @throw runtime_error when the database could not be initialized
	 */	
	virtual void InitPlugin();

	/**
	 * Request a list of objects for a particular company and specified objectclass.
	 *
	 * This will only create a query and will call DBPlugin::CreateSignatureList()
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
	virtual std::unique_ptr<signatures_t> getAllObjects(const objectid_t &company, objectclass_t objclass);

	/**
	 * Obtain the object details for the given object
	 *
	 * This calls DBPlugin::getObjectDetails(const list<objectid_t> &objectids)
	 *
	 * @param[in]	objectid
	 *					The objectid for which is details are requested
	 * @return The objectdetails for the given objectid
	 * @throw objectnotfound when the object was not found
	 */
	virtual std::unique_ptr<objectdetails_t> getObjectDetails(const objectid_t &objectid);

    /**
	 * Obtain the object details for the given objects
	 *
	 * It is possible that the returned map contains less elements then objectids.size()
	 * when not all objects where found.
	 *
	 * @param[in]   objectids
	 *					The list of object signatures for which the details are requested
	 * @return A map of objectid with the matching objectdetails
	 * @throw runtime_error when SQL problems occur.
	 */
	virtual std::unique_ptr<std::map<objectid_t, objectdetails_t> > getObjectDetails(const std::list<objectid_t> &objectids);

	/**
	 * Get all children for a parent for a given relation type.
	 * For example all users in a group
	 *
	 * This will only create a query and will call DBPlugin::CreateSignatureList()
	 *
	 * @param[in]	relation
	 *					The relation type which connects the child and parent object
	 * @param[in]	parentobject
	 *					The parent object for which the children are requested
	 * @return A list of object signatures of the children of the parent.
	 * @throw std::exception
	 */
	virtual std::unique_ptr<signatures_t> getSubObjectsForObject(userobject_relation_t relation, const objectid_t &parentobject);

    /**
	 * Request all parents for a childobject for a given relation type.
	 * For example all groups for a user
	 *
	 * This will only create a query and will call DBPlugin::CreateSignatureList()
	 *
	 * @param[in]	relation
	 *					The relation type which connects the child and parent object
	 * @param[in]	childobject
	 *					The childobject for which the parents are requested
	 * @return A list of object signatures of the parents of the child.
	 * @throw std::exception
	 */
	virtual std::unique_ptr<signatures_t> getParentObjectsForObject(userobject_relation_t relation, const objectid_t &childobject);

	/**
	 * Update an object with new details
	 *
	 * @param[in]	id
	 *					The object id of the object which should be updated.
	 * @param[in]	details
	 *					The objectdetails which should be written to the object.
	 * @param[in]	lpRemove
	 *					List of configuration names which should be removed from the object
	 * @throw runtime_error when SQL problems occur.
	 */
	virtual void changeObject(const objectid_t &id, const objectdetails_t &details, const std::list<std::string> *lpRemove);

	/**
	 * Create object in plugin
	 *
	 * @param[in]	details
	 *                  The object details of the new object.
	 * @return The objectsignature of the created object.
	 * @throw runtime_error When SQL problems occur.
	 * @throw collison_error When the object already exists.
	 */
	virtual objectsignature_t createObject(const objectdetails_t &details);

	/**
	 * Delete object from plugin
	 *
	 * @param[in]	id
	 *					The objectid which should be deleted
	 * @throw runtime_error When SQL problems occur.
	 * @throw objectnotfound When the object did not exist.
	 */
	virtual void deleteObject(const objectid_t &id);

    /**
	 * Add relation between child and parent. This can be used
	 * for example to add a user to a group or add
	 * permission relations on companies.
	 *
	 * @param[in]	relation
	 *					The relation type which should connect the
	 *					child and parent.
	 * @param[in]	parentobject
	 *					The parent object.
	 * @param[in]	childobject
	 *					The child object.
	 * @throw runtime_error When SQL problems occur.
	 * @throw collison_error When the relation already exists.
	 */
	virtual void addSubObjectRelation(userobject_relation_t relation,
									  const objectid_t &parentobject, const objectid_t &childobject);

	/**
	 * Delete relation between child and parent, this can be used
	 * for example to delete a user from a group or delete
	 * permission relations on companies.
	 *
	 * @param[in]	relation
	 *					The relation type which connected the
	 *					child and parent.
	 * @param[in]	parentobject
	 *					The parent object.
	 * @param[in]	childobject
	 *					The child object.
	 * @throw runtime_error When SQL problems occur.
	 * @throw objectnotfound When the relation did not exist.
	 */
	virtual void deleteSubObjectRelation(userobject_relation_t relation,
										 const objectid_t &parentobject, const objectid_t &childobject);

	/**
	 * Request quota information from object
	 *
	 * @param[in]	id
	 *					The objectid from which the quota should be read
	 * @param[in]	bGetUserDefault
	 *					Boolean to indicate if the userdefault quota must be requested.
	 * @return The quota details
	 * @throw runtime_error when SQL problems occur
	 */
	virtual std::unique_ptr<quotadetails_t> getQuota(const objectid_t &id, bool bGetUserDefault);

	/**
	 * Update object with quota information
	 *
	 * @param[in]	id
	 *					The object id which should be updated
	 * @param[in]	quotadetails
	 *					The quota details which must be written to the Database
	 * @throw runtime_error when SQL problems occur
	 */
	virtual void setQuota(const objectid_t &id, const quotadetails_t &quotadetails);

	/**
	 * Get extra properties which are set in the object details for the addressbook
	 *
	 * @note It is not mandatory to implement this function
	 *
	 * @return	a empty list of properties
	 * @throw runtime_error when SQL problems occur
	 */
	virtual std::unique_ptr<abprops_t> getExtraAddressbookProperties(void);
	
	virtual void removeAllObjects(objectid_t except);


private:
	/**
	 * Execute a query and return all objects in the form of object signatures
	 *
	 * The SQL query must request 3 columns in his order::
	 *	- externid
	 *	- objectclass
	 *	- signature
	 *	.
	 *
	 * @param[in]	query
	 *					The SQL query which should be executed
	 * @return The list of object signatures which were returned by the SQL query
	 * @throw runtime_error when SQL problems occur
	 */
	virtual std::unique_ptr<signatures_t> CreateSignatureList(const std::string &query);

	/**
	 * Convert a string to MD5Hash
	 * This will be used for hashing passwords
	 *
	 * @param[in]	strData
	 *					The data which should be converted into the MD5Hash.
	 * @param[out]	lpstrResult
	 *					The MD5Hash of strData.
	 * @return KCERR_INVALID_PARAMETER if strData is empty or lpstrResult is NULL
	 */
	virtual ECRESULT CreateMD5Hash(const std::string &strData, std::string* lpstrResult);

	/**
	 * Create a new object based on an objectdetails_t instance with an
	 * externid. This happens when the object is created via ICS.
	 *
	 * @param[in]	objectid
	 *					The objectid of the object to create
	 * @param[in]	details
	 *					The details of the object.
	 */
	void CreateObjectWithExternId(const objectid_t &objectid, const objectdetails_t &details);

	/**
	 * Create a new object based on an objectdetails_t instance without
	 * an externid. This happens when the object is created with 
	 * kopano-admin.
	 *
	 * @param[in]	details
	 *					The details of the object.
	 * @return	The new objectid
	 */
	objectid_t CreateObject(const objectdetails_t &details);

protected:
	/**
	 * Search in the Database for all users which contain the search term.
	 *
	 * @param[in]	match
	 *					The search term which should be found
	 * @param[in]	search_props
	 *					The Database property which should be checked during the query
	 * @param[in]	return_prop
	 *					The Database property which should be returned as externid
	 * @param[in]	ulFlags
	 *					If set to EMS_AB_ADDRESS_LOOKUP only exact matches will be found
	 * @return The list of object signatures which match the search term
	 * @throw objectnotfound when no results have been found
	 */
	virtual std::unique_ptr<signatures_t> searchObjects(const std::string &match, const char **search_props, const char *return_prop, unsigned int ulFlags);

	/**
	 * Update objectdetails with sendas information.
	 * This will obtain the sendas relation information by calling getSubObjectsForObject()
	 *
	 * @param[in]		objectid
	 *						The objectid for which the object details should be updated.
	 * @param[in]		lpDetails
	 *						The object details which will be updated.
	 */
	virtual void addSendAsToDetails(const objectid_t &objectid, objectdetails_t *lpDetails);

	ECDatabase *m_lpDatabase = nullptr;
};
/** @} */

} /* namespace */

#endif
