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

#ifndef ECUSERMANAGEMENT_H
#define ECUSERMANAGEMENT_H

#include <kopano/zcdefs.h>
#include <list>
#include <map>
#include <mutex>
#include <ctime>
#include <kopano/kcodes.h>
#include <kopano/pcuser.hpp>
#include <kopano/ECConfig.h>
#include "ECSession.h"
#include <kopano/ECLogger.h>
#include <kopano/ECDefs.h>
#include "plugin.h"

struct soap;

namespace KC {

class localobjectdetails_t _kc_final : public objectdetails_t {
public:
	localobjectdetails_t(void) = default;
	localobjectdetails_t(unsigned int id, objectclass_t objclass) : objectdetails_t(objclass), ulId(id) {};
	localobjectdetails_t(unsigned int id, const objectdetails_t &details) : objectdetails_t(details), ulId(id) {};

	bool operator==(const localobjectdetails_t &obj) const { return ulId == obj.ulId; };
	bool operator<(const localobjectdetails_t &obj) const { return ulId < obj.ulId; } ;

	unsigned int ulId = 0;
};

class usercount_t _kc_final {
public:
	enum ucIndex {
		ucActiveUser = 0,
		ucNonActiveUser,
		ucRoom,
		ucEquipment,
		ucContact,
		ucNonActiveTotal,			// Must be right before ucMAX
		ucMAX = ucNonActiveTotal	// Must be very last
	};

	usercount_t(void)
	{
		memset(m_ulCounts, 0, sizeof(m_ulCounts));
	}
	
	usercount_t(unsigned int ulActiveUser, unsigned int ulNonActiveUser, unsigned int ulRoom, unsigned int ulEquipment, unsigned int ulContact): m_bValid(true) {
		m_ulCounts[ucActiveUser]	= ulActiveUser;
		m_ulCounts[ucNonActiveUser]	= ulNonActiveUser;
		m_ulCounts[ucRoom]			= ulRoom;
		m_ulCounts[ucEquipment]		= ulEquipment;
		m_ulCounts[ucContact]		= ulContact;
	}

	usercount_t(const usercount_t &other): m_bValid(other.m_bValid) {
		memcpy(m_ulCounts, other.m_ulCounts, sizeof(m_ulCounts));
	}

	void swap(usercount_t &other) {
		std::swap(m_bValid, other.m_bValid);
		for (unsigned i = 0; i < ucMAX; ++i)
			std::swap(m_ulCounts[i], other.m_ulCounts[i]);
	}

	void assign(unsigned int ulActiveUser, unsigned int ulNonActiveUser, unsigned int ulRoom, unsigned int ulEquipment, unsigned int ulContact) {
		usercount_t tmp(ulActiveUser, ulNonActiveUser, ulRoom, ulEquipment, ulContact);
		swap(tmp);
	}

	void assign(const usercount_t &other) {
		if (&other != this) {
			usercount_t tmp(other);
			swap(tmp);
		}
	}

	usercount_t& operator=(const usercount_t &other) {
		assign(other);
		return *this;
	}

	bool isValid() const {
		return m_bValid;
	}

	void set(ucIndex index, unsigned int ulValue) {
		if (index != ucNonActiveTotal) {
			assert(index >= 0 && index < ucMAX);
			m_ulCounts[index] = ulValue;
			m_bValid = true;
		}
	}

	unsigned int operator[](ucIndex index) const {
		if (index == ucNonActiveTotal)
			return m_ulCounts[ucNonActiveUser] + m_ulCounts[ucRoom] + m_ulCounts[ucEquipment];	// Contacts don't count for non-active stores.
		assert(index >= 0 && index < ucMAX);
		return m_ulCounts[index];
	}

private:
	bool m_bValid = false;
	unsigned int	m_ulCounts[ucMAX];
};

// Use for ulFlags
#define USERMANAGEMENT_IDS_ONLY			0x1		// Return only local userID (the ulId field). 'details' is undefined in this case
#define USERMANAGEMENT_ADDRESSBOOK		0x2		// Return only objects which should be visible in the address book
#define USERMANAGEMENT_FORCE_SYNC		0x4		// Force sync with external database
#define USERMANAGEMENT_SHOW_HIDDEN		0x8		// Show hidden entries

// Use for ulLicenseStatus in CheckUserLicense()
#define USERMANAGEMENT_LIMIT_ACTIVE_USERS		0x1	/* Limit reached, but not yet exceeded */
#define USERMANAGEMENT_LIMIT_NONACTIVE_USERS	0x2 /* Limit reached, but not yet exceeded */
#define USERMANAGEMENT_EXCEED_ACTIVE_USERS		0x4 /* Limit exceeded */
#define USERMANAGEMENT_EXCEED_NONACTIVE_USERS	0x8 /* Limit exceeded */

#define USERMANAGEMENT_BLOCK_CREATE_ACTIVE_USER		( USERMANAGEMENT_LIMIT_ACTIVE_USERS | USERMANAGEMENT_EXCEED_ACTIVE_USERS )
#define USERMANAGEMENT_BLOCK_CREATE_NONACTIVE_USER	( USERMANAGEMENT_LIMIT_NONACTIVE_USERS | USERMANAGEMENT_EXCEED_NONACTIVE_USERS )
#define USERMANAGEMENT_USER_LICENSE_EXCEEDED		( USERMANAGEMENT_EXCEED_ACTIVE_USERS | USERMANAGEMENT_EXCEED_NONACTIVE_USERS )

class _kc_export ECUserManagement _kc_final {
public:
	_kc_hidden ECUserManagement(BTSession *, ECPluginFactory *, ECConfig *);
	_kc_hidden virtual ~ECUserManagement(void) _kc_impdtor;

	// Authenticate a user
	_kc_hidden virtual ECRESULT AuthUserAndSync(const char *user, const char *pass, unsigned int *user_id);

	// Get data for an object, with on-the-fly delete of the specified object id
	virtual ECRESULT GetObjectDetails(unsigned int obj_id, objectdetails_t *ret);
	// Get quota details for a user object
	_kc_hidden virtual ECRESULT GetQuotaDetailsAndSync(unsigned int obj_id, quotadetails_t *ret, bool get_user_default = false);
	// Set quota details for a user object
	_kc_hidden virtual ECRESULT SetQuotaDetailsAndSync(unsigned int obj_id, const quotadetails_t &);
	// Get (typed) objectlist for company, or list of all companies, with on-the-fly delete/create of users and groups
	_kc_hidden virtual ECRESULT GetCompanyObjectListAndSync(objectclass_t, unsigned int company_id, std::list<localobjectdetails_t> **objs, unsigned int flags = 0);
	// Get subobjects in an object, with on-the-fly delete of the specified parent object
	_kc_hidden virtual ECRESULT GetSubObjectsOfObjectAndSync(userobject_relation_t, unsigned int parent_id, std::list<localobjectdetails_t> **objs, unsigned int flags = 0);
	// Get parent to which an object belongs, with on-the-fly delete of the specified child object id
	_kc_hidden virtual ECRESULT GetParentObjectsOfObjectAndSync(userobject_relation_t, unsigned int child_id, std::list<localobjectdetails_t> **groups, unsigned int flags = 0);

	// Set data for a single user, with on-the-fly delete of the specified user id
	_kc_hidden virtual ECRESULT SetObjectDetailsAndSync(unsigned int obj_id, const objectdetails_t &, std::list<std::string> *remove_props);

	// Add a member to a group, with on-the-fly delete of the specified group id
	_kc_hidden virtual ECRESULT AddSubObjectToObjectAndSync(userobject_relation_t, unsigned int parent_id, unsigned int child_id);
	_kc_hidden virtual ECRESULT DeleteSubObjectFromObjectAndSync(userobject_relation_t, unsigned int parent_id, unsigned int child_id);

	// Resolve a user name to a user id, with on-the-fly create of the specified user
	_kc_hidden virtual ECRESULT ResolveObjectAndSync(objectclass_t, const char *name, unsigned int *obj_id);

	// Get a local object ID for a part of a name
	virtual ECRESULT SearchObjectAndSync(const char *search_string, unsigned int flags, unsigned int *id);

	// Create an object
	_kc_hidden virtual ECRESULT CreateObjectAndSync(const objectdetails_t &, unsigned int *id);
	// Delete an object
	_kc_hidden virtual ECRESULT DeleteObjectAndSync(unsigned int obj_id);
	// Either modify or create an object with a specific object id and type (used for synchronize)
	_kc_hidden virtual ECRESULT CreateOrModifyObject(const objectid_t &extern_id, const objectdetails_t &, unsigned int pref_id, std::list<std::string> *remove_props);

	// Get MAPI property data for a group or user/group/company id, with on-the-fly delete of the specified user/group/company
	_kc_hidden virtual ECRESULT GetProps(struct soap *, unsigned int obj_id, struct propTagArray *, struct propValArray *);
	_kc_hidden virtual ECRESULT GetContainerProps(struct soap *, unsigned int obj_id, struct propTagArray *, struct propValArray *);
	// Do the same for a whole set of items
	_kc_hidden virtual ECRESULT QueryContentsRowData(struct soap *, ECObjectTableList *rowlist, struct propTagArray *, struct rowSet **);
	_kc_hidden virtual ECRESULT QueryHierarchyRowData(struct soap *, ECObjectTableList *rowlist, struct propTagArray *, struct rowSet **);
	_kc_hidden virtual ECRESULT GetUserCount(unsigned int *active, unsigned int *inactive); // returns active users and non-active users (so you may get ulUsers=3, ulNonActives=5)
	_kc_hidden virtual ECRESULT GetUserCount(usercount_t *);
	_kc_hidden virtual ECRESULT GetCachedUserCount(usercount_t *);
	_kc_hidden virtual ECRESULT GetPublicStoreDetails(objectdetails_t *);
	virtual ECRESULT GetServerDetails(const std::string &server, serverdetails_t *);
	_kc_hidden virtual ECRESULT GetServerList(serverlist_t *);

	/* Check if the user license status */
	_kc_hidden ECRESULT CheckUserLicense(unsigned int *licstatus);

	// Returns true if ulId is an internal ID (so either SYSTEM or EVERYONE)
	bool		IsInternalObject(unsigned int ulId);

	// Create a v1 based AB SourceKey
	_kc_hidden ECRESULT GetABSourceKeyV1(unsigned int user_id, SOURCEKEY *);

	// Get userinfo from cache
	_kc_hidden ECRESULT GetExternalId(unsigned int di, objectid_t *extern_id, unsigned int *company_id = nullptr, std::string *signature = nullptr);
	_kc_hidden ECRESULT GetLocalId(const objectid_t &extern_id, unsigned int *id, std::string *signature = nullptr);

	/* calls localid->externid and login->user/company conversions */
	_kc_hidden virtual ECRESULT UpdateUserDetailsFromClient(objectdetails_t *);

	/* Create an ABEID in version 1 or version 0 */
	_kc_hidden ECRESULT CreateABEntryID(struct soap *, unsigned int vers, unsigned int obj_id, unsigned int type, objectid_t *extern_id, gsoap_size_t *eid_size, ABEID **eid);

	/* Resync all objects from the plugin. */
	_kc_hidden ECRESULT SyncAllObjects(void);

private:
	/* Convert a user loginname to username and companyname */
	_kc_hidden virtual ECRESULT ConvertLoginToUserAndCompany(objectdetails_t *);
	/* Convert username and companyname to loginname */
	_kc_hidden virtual ECRESULT ConvertUserAndCompanyToLogin(objectdetails_t *);
	/* convert extern IDs to local IDs */
	_kc_hidden virtual ECRESULT ConvertExternIDsToLocalIDs(objectdetails_t *);
	/* convert local IDs to extern IDs */
	_kc_hidden virtual ECRESULT ConvertLocalIDsToExternIDs(objectdetails_t *);
	/* calls externid->localid and user/company->login conversions */
	_kc_hidden virtual ECRESULT UpdateUserDetailsToClient(objectdetails_t *);
	_kc_hidden ECRESULT ComplementDefaultFeatures(objectdetails_t *);
	_kc_hidden ECRESULT RemoveDefaultFeatures(objectdetails_t *);
	_kc_hidden bool MustHide(/*const*/ ECSecurity &, unsigned int flags, const objectdetails_t &) const;

	// Get object details from list
	_kc_hidden ECRESULT GetLocalObjectListFromSignatures(const std::list<objectsignature_t> &signatures, const std::map<objectid_t, unsigned int> &extern_to_local, unsigned int flags, std::list<localobjectdetails_t> *);
	// Get local details
	_kc_hidden ECRESULT GetLocalObjectDetails(unsigned int id, objectdetails_t *);

	// Get remote details
	_kc_hidden ECRESULT GetExternalObjectDetails(unsigned int id, objectdetails_t *);

	// Get userid from usertable or create a new user/group if it doesn't exist yet
	_kc_hidden ECRESULT GetLocalObjectIdOrCreate(const objectsignature_t &signature, unsigned int *id);
	_kc_hidden ECRESULT GetLocalObjectsIdsOrCreate(const std::list<objectsignature_t> &signatures, map<objectid_t, unsigned int> *local_objids);

	// Get a list of local object IDs in the database plus any internal objects (SYSTEM, EVERYONE)
	_kc_hidden ECRESULT GetLocalObjectIdList(objectclass_t, unsigned int company_id, std::list<unsigned int> **objs);

	// Converts anonymous Object Detail to property. */
	_kc_hidden ECRESULT ConvertAnonymousObjectDetailToProp(struct soap *, objectdetails_t *, unsigned int tag, struct propVal *);
	// Converts the data in user/group/company details fields into property value array for content tables and MAPI_MAILUSER and MAPI_DISTLIST objects
	_kc_hidden ECRESULT ConvertObjectDetailsToProps(struct soap *, unsigned int id, objectdetails_t *, struct propTagArray *proptags, struct propValArray *propvals);
	// Converts the data in company/addresslist details fields into property value array for hierarchy tables and MAPI_ABCONT objects
	_kc_hidden ECRESULT ConvertContainerObjectDetailsToProps(struct soap *, unsigned int id, objectdetails_t *, struct propTagArray *proptags, struct propValArray *propvals);
	// Create GlobalAddressBook properties
	_kc_hidden ECRESULT ConvertABContainerToProps(struct soap *, unsigned int id, struct propTagArray *, struct propValArray *);

	_kc_hidden ECRESULT MoveOrCreateLocalObject(const objectsignature_t &signature, unsigned int *obj_id, bool *moved);
	_kc_hidden ECRESULT CreateLocalObjectSimple(const objectsignature_t &signature, unsigned int pref_id);
	_kc_hidden ECRESULT CreateLocalObject(const objectsignature_t &signature, unsigned int *obj_id);
	_kc_hidden ECRESULT MoveOrDeleteLocalObject(unsigned int obj_id, objectclass_t);
	_kc_hidden ECRESULT MoveLocalObject(unsigned int obj_id, objectclass_t, unsigned int company_id, const std::string &newusername);
	_kc_hidden ECRESULT DeleteLocalObject(unsigned int obj_id, objectclass_t);
	_kc_hidden ECRESULT UpdateObjectclassOrDelete(const objectid_t &extern_id, unsigned int *obj_id);
	_kc_hidden ECRESULT GetUserAndCompanyFromLoginName(const std::string &login, std::string *user, std::string *company);

	// Process the modification of a user-object
	_kc_hidden ECRESULT CheckObjectModified(unsigned int obj_id, const std::string &localsignature, const std::string &remotesignature);
	_kc_hidden ECRESULT ProcessModification(unsigned int id, const std::string &newsignature);

	_kc_hidden ECRESULT ResolveObject(objectclass_t, const std::string &name, const objectid_t &company, objectid_t *extern_id);
	_kc_hidden ECRESULT CreateABEntryID(struct soap *, const objectid_t &extern_id, struct propVal *);
	_kc_hidden ECRESULT CreateABEntryID(struct soap *, unsigned int obj_id, unsigned int type, struct propVal *);
	_kc_hidden ECRESULT GetSecurity(ECSecurity **);

protected:
	ECPluginFactory 	*m_lpPluginFactory;
	BTSession			*m_lpSession;
	ECConfig			*m_lpConfig;

private:
	std::recursive_mutex m_hMutex;
	usercount_t 				m_userCount;
	time_t m_usercount_ts = 0;
};

#define KOPANO_UID_EVERYONE 1
#define KOPANO_UID_SYSTEM 2

#define KOPANO_ACCOUNT_SYSTEM "SYSTEM"
#define KOPANO_FULLNAME_SYSTEM "SYSTEM"
#define KOPANO_ACCOUNT_EVERYONE "Everyone"
#define KOPANO_FULLNAME_EVERYONE "Everyone"

/*
* Fixed addressbook containers
* Only IDs 0, 1 and 2 are available for hardcoding
* IDs for the fixed addressbook containers. This is because
* those IDs are the only ones which will not conflict with
* entries in the users table.
*
* The account name of the containers are used for the path
* name of the container and is used in determine the exact
* order of the containers and the subcontainers in the Address
* Book. The fullname of the containers are used as display
* name to the user.
*/
#define KOPANO_UID_ADDRESS_BOOK			0
#define KOPANO_UID_GLOBAL_ADDRESS_BOOK		1
#define KOPANO_UID_GLOBAL_ADDRESS_LISTS		2

#define KOPANO_ACCOUNT_ADDRESS_BOOK		"Kopano Address Book"
#define KOPANO_FULLNAME_ADDRESS_BOOK		"Kopano Address Book"
#define KOPANO_ACCOUNT_GLOBAL_ADDRESS_BOOK	"Global Address Book"
#define KOPANO_FULLNAME_GLOBAL_ADDRESS_BOOK	"Global Address Book"
#define KOPANO_ACCOUNT_GLOBAL_ADDRESS_LISTS	"Global Address Lists"
#define KOPANO_FULLNAME_GLOBAL_ADDRESS_LISTS	"All Address Lists"

} /* namespace */

#endif
