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

#include <iostream>
#include <string>
#include <stdexcept>
#include <sys/types.h>
#include <pwd.h>
#include <sstream>
#include <assert.h>
#include <sys/stat.h>
#include <grp.h>
#include <crypt.h>
#include <set>
#include <iterator>

#ifdef HAVE_SHADOW_H
#include <shadow.h>
#include <errno.h>
#endif
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif

#include "EMSAbTag.h"
#include "ECConfig.h"
#include "ECDefs.h"
#include "ECLogger.h"
#include "ECPluginSharedData.h"
#include "stringutil.h"

using namespace std;

#include "UnixUserPlugin.h"
#include "ecversion.h"

/**
 * static buffer size for getpwnam_r() calls etc.
 * needs to be big enough for all strings in the passwd/group/spwd struct:
 */
#define PWBUFSIZE 16384

extern "C" {
	UserPlugin* getUserPluginInstance(pthread_mutex_t *pluginlock, ECPluginSharedData *shareddata) {
		return new UnixUserPlugin(pluginlock, shareddata);
	}

	void deleteUserPluginInstance(UserPlugin *up) {
		delete up;
	}

	int getUserPluginVersion() {
		return PROJECT_VERSION_REVISION;
	}
}

UnixUserPlugin::UnixUserPlugin(pthread_mutex_t *pluginlock, ECPluginSharedData *shareddata) throw(exception)
	: DBPlugin(pluginlock, shareddata), m_iconv(NULL)
{
	const configsetting_t lpDefaults [] = {
		{ "fullname_charset", "iso-8859-15" },		// us-ascii compatible with support for high characters
		{ "default_domain", "localhost" },			// no sane default
		{ "non_login_shell", "/bin/false", CONFIGSETTING_RELOADABLE },	// create a non-login box when a user has this shell
		{ "min_user_uid", "1000", CONFIGSETTING_RELOADABLE },
		{ "max_user_uid", "10000", CONFIGSETTING_RELOADABLE },
		{ "except_user_uids", "", CONFIGSETTING_RELOADABLE },
		{ "min_group_gid", "1000", CONFIGSETTING_RELOADABLE },
		{ "max_group_gid", "10000", CONFIGSETTING_RELOADABLE },
		{ "except_group_gids", "", CONFIGSETTING_RELOADABLE },
		{ NULL, NULL }
	};

	m_config = shareddata->CreateConfig(lpDefaults);
	if (!m_config)
		throw runtime_error(string("Not a valid configuration file."));

	if (m_bHosted)
		throw notsupported("Hosted Zarafa not supported when using the Unix Plugin");
	if (m_bDistributed)
		throw notsupported("Distributed Zarafa not supported when using the Unix Plugin");
}

UnixUserPlugin::~UnixUserPlugin()
{
	if (m_iconv)
		delete m_iconv;
}

void UnixUserPlugin::InitPlugin() throw(std::exception) {
	DBPlugin::InitPlugin();

	// we only need unix_charset -> zarafa charset
	m_iconv = new ECIConv("utf-8", m_config->GetSetting("fullname_charset"));
}

void UnixUserPlugin::findUserID(const string &id, struct passwd *pwd, char *buffer) throw(std::exception)
{
	struct passwd *pw = NULL;
	uid_t minuid = fromstring<const char *, uid_t>(m_config->GetSetting("min_user_uid"));
	uid_t maxuid = fromstring<const char *, uid_t>(m_config->GetSetting("max_user_uid"));
	vector<string> exceptuids = tokenize(m_config->GetSetting("except_user_uids"), " \t");
	objectid_t objectid;

	errno = 0;
	getpwuid_r(atoi(id.c_str()), pwd, buffer, PWBUFSIZE, &pw);
	errnoCheck(id);

	if (pw == NULL)
		throw objectnotfound(id);

	if (pw->pw_uid < minuid || pw->pw_uid >= maxuid)
		throw objectnotfound(id);
			
	for (unsigned int i = 0; i < exceptuids.size(); ++i)
		if (pw->pw_uid == fromstring<const std::string, uid_t>(exceptuids[i]))
			throw objectnotfound(id);
}

void UnixUserPlugin::findUser(const string &name, struct passwd *pwd, char *buffer) throw(std::exception)
{
	struct passwd *pw = NULL;
	uid_t minuid = fromstring<const char *, uid_t>(m_config->GetSetting("min_user_uid"));
	uid_t maxuid = fromstring<const char *, uid_t>(m_config->GetSetting("max_user_uid"));
	vector<string> exceptuids = tokenize(m_config->GetSetting("except_user_uids"), " \t");
	objectid_t objectid;

	errno = 0;
	getpwnam_r(name.c_str(), pwd, buffer, PWBUFSIZE, &pw);
	errnoCheck(name);

	if (pw == NULL)
		throw objectnotfound(name);

	if (pw->pw_uid < minuid || pw->pw_uid >= maxuid)
		throw objectnotfound(name);
			
	for (unsigned int i = 0; i < exceptuids.size(); ++i)
		if (pw->pw_uid == fromstring<const std::string, uid_t>(exceptuids[i]))
			throw objectnotfound(name);
}

void UnixUserPlugin::findGroupID(const string &id, struct group *grp, char *buffer) throw(std::exception)
{
	struct group *gr = NULL;
	gid_t mingid = fromstring<const char *, gid_t>(m_config->GetSetting("min_group_gid"));
	gid_t maxgid = fromstring<const char *, gid_t>(m_config->GetSetting("max_group_gid"));
	vector<string> exceptgids = tokenize(m_config->GetSetting("except_group_gids"), " \t");
	objectid_t objectid;

	errno = 0;
	getgrgid_r(atoi(id.c_str()), grp, buffer, PWBUFSIZE, &gr);
	errnoCheck(id);

	if (gr == NULL)
		throw objectnotfound(id);

	if (gr->gr_gid < mingid || gr->gr_gid >= maxgid)
		throw objectnotfound(id);

	for (unsigned int i = 0; i < exceptgids.size(); ++i)
		if (gr->gr_gid == fromstring<const std::string, gid_t>(exceptgids[i]))
			throw objectnotfound(id);
}

void UnixUserPlugin::findGroup(const string &name, struct group *grp, char *buffer) throw(std::exception)
{
	struct group *gr = NULL;
	gid_t mingid = fromstring<const char *, gid_t>(m_config->GetSetting("min_group_gid"));
	gid_t maxgid = fromstring<const char *, gid_t>(m_config->GetSetting("max_group_gid"));
	vector<string> exceptgids = tokenize(m_config->GetSetting("except_group_gids"), " \t");
	objectid_t objectid;

	errno = 0;
	getgrnam_r(name.c_str(), grp, buffer, PWBUFSIZE, &gr);
	errnoCheck(name);

	if (gr == NULL)
		throw objectnotfound(name);

	if (gr->gr_gid < mingid || gr->gr_gid >= maxgid)
		throw objectnotfound(name);

	for (unsigned int i = 0; i < exceptgids.size(); ++i)
		if (gr->gr_gid == fromstring<const std::string, gid_t>(exceptgids[i]))
			throw objectnotfound(name);
}

objectsignature_t UnixUserPlugin::resolveUserName(const string &name) throw(std::exception)
{
	char buffer[PWBUFSIZE];
	struct passwd pws;
	const char *lpszNonActive =  m_config->GetSetting("non_login_shell");
	objectid_t objectid;

	findUser(name, &pws, buffer);

	if (strcmp(pws.pw_shell, lpszNonActive) != 0)
		objectid = objectid_t(tostring(pws.pw_uid), ACTIVE_USER);
	else
		objectid = objectid_t(tostring(pws.pw_uid), NONACTIVE_USER);

	return objectsignature_t(objectid, getDBSignature(objectid) + pws.pw_gecos + pws.pw_name);
}

objectsignature_t UnixUserPlugin::resolveGroupName(const string &name) throw(std::exception)
{
	char buffer[PWBUFSIZE];
	struct group grp;
	objectid_t objectid;
  
	findGroup(name, &grp, buffer);

	objectid = objectid_t(tostring(grp.gr_gid), DISTLIST_SECURITY);
	return objectsignature_t(objectid, grp.gr_name);
}

objectsignature_t UnixUserPlugin::resolveName(objectclass_t objclass, const string &name, const objectid_t &company) throw(std::exception)
{
	objectsignature_t user;
	objectsignature_t group;

	if (company.id.empty())
		m_logger->Log(EC_LOGLEVEL_DEBUG, "%s Class %x, Name %s", __FUNCTION__, objclass, name.c_str());
	else
		m_logger->Log(EC_LOGLEVEL_DEBUG, "%s Class %x, Name %s, Company %s", __FUNCTION__, objclass, name.c_str(), company.id.c_str());

	switch (OBJECTCLASS_TYPE(objclass)) {
	case OBJECTTYPE_UNKNOWN:
		// Caller doesn't know what he is looking for, try searching through
		// users and groups. Note that 1 function _must_ fail because otherwise
		// we have a duplicate entry.
		try {
			user = resolveUserName(name);
		} catch (std::exception &e) {
			// object is not a user
		}

		try {
			group = resolveGroupName(name);
		} catch (std::exception &e) {
			// object is not a group
		}

		if (!user.id.id.empty()) {
			// Object is both user and group
			if (!group.id.id.empty())
				throw toomanyobjects(name);
			return user;
		} else {
			// Object is neither user not group
			if (group.id.id.empty())
				throw objectnotfound(name);
			return group;
		}
	case OBJECTTYPE_MAILUSER:
		return resolveUserName(name);
	case OBJECTTYPE_DISTLIST:
		return resolveGroupName(name);
	default:
		throw runtime_error("Unknown object type " + stringify(objclass));
	}
}

objectsignature_t UnixUserPlugin::authenticateUser(const string &username, const string &password, const objectid_t &companyname) throw(std::exception) {
	struct passwd pws, *pw = NULL;
	char buffer[PWBUFSIZE];
	uid_t minuid = fromstring<const char *, uid_t>(m_config->GetSetting("min_user_uid"));
	uid_t maxuid = fromstring<const char *, uid_t>(m_config->GetSetting("max_user_uid"));
	vector<string> exceptuids = tokenize(m_config->GetSetting("except_user_uids"), " \t");
	auto_ptr<struct crypt_data> cryptdata = auto_ptr<struct crypt_data>(NULL);
	auto_ptr<objectdetails_t> ud = auto_ptr<objectdetails_t>(NULL);
	objectid_t objectid;
	const char *crpw = NULL;

	cryptdata.reset(new struct crypt_data); // malloc because it is > 128K !
	memset(cryptdata.get(), 0, sizeof(struct crypt_data));

	errno = 0;
	getpwnam_r(username.c_str(), &pws, buffer, PWBUFSIZE, &pw);
	errnoCheck(username);

	if (pw == NULL)
		throw objectnotfound(username);

	if (pw->pw_uid < minuid || pw->pw_uid >= maxuid)
		throw objectnotfound(username);
		
	for (unsigned i = 0; i < exceptuids.size(); ++i)
		if (pw->pw_uid == fromstring<const std::string, uid_t>(exceptuids[i]))
			throw objectnotfound(username);

	if (strcmp(pw->pw_shell, m_config->GetSetting("non_login_shell")) == 0)
		throw login_error("Non-active user disallowed to login");

	ud = objectdetailsFromPwent(pw);
	crpw = crypt_r(password.c_str(), ud->GetPropString(OB_PROP_S_PASSWORD).c_str(), cryptdata.get());

	if (!crpw || strcmp(crpw, ud->GetPropString(OB_PROP_S_PASSWORD).c_str()))
		throw login_error("Trying to authenticate failed: wrong username or password");

	objectid = objectid_t(tostring(pw->pw_uid), ACTIVE_USER);
	return objectsignature_t(objectid, getDBSignature(objectid) + pw->pw_gecos + pw->pw_name);
}

bool UnixUserPlugin::matchUserObject(struct passwd *pw, const string &match, unsigned int ulFlags)
{
	string email;
	bool matched = false;

	// username or fullname
	if(ulFlags & EMS_AB_ADDRESS_LOOKUP) {
		matched =
			(stricmp(pw->pw_name, (char*)match.c_str()) == 0) ||
			(stricmp((char*)m_iconv->convert(pw->pw_gecos).c_str(), (char*)match.c_str()) == 0);
	} else {
		matched =
			(strnicmp(pw->pw_name, (char*)match.c_str(), match.size()) == 0) ||
			(strnicmp((char*)m_iconv->convert(pw->pw_gecos).c_str(), (char*)match.c_str(), match.size()) == 0);
	}

	if (matched)
		return matched;

	email = string(pw->pw_name) + "@" + m_config->GetSetting("default_domain");
	if(ulFlags & EMS_AB_ADDRESS_LOOKUP)
		matched = (email == match);
	else
		matched = (strnicmp((char*)email.c_str(), (char*)match.c_str(), match.size()) == 0);

	return matched;
}

bool UnixUserPlugin::matchGroupObject(struct group *gr, const string &match, unsigned int ulFlags)
{
	bool matched = false;

	if(ulFlags & EMS_AB_ADDRESS_LOOKUP)
		matched = stricmp(gr->gr_name, (char*)match.c_str()) == 0;
	else
		matched = strnicmp(gr->gr_name, (char*)match.c_str(), match.size()) == 0;

	return matched;
}

auto_ptr<signatures_t> UnixUserPlugin::getAllUserObjects(const string &match, unsigned int ulFlags) throw(std::exception)
{
	auto_ptr<signatures_t> objectlist = auto_ptr<signatures_t>(new signatures_t());;
	char buffer[PWBUFSIZE];
	struct passwd pws, *pw = NULL;
	uid_t minuid = fromstring<const char *, uid_t>(m_config->GetSetting("min_user_uid"));
	uid_t maxuid = fromstring<const char *, uid_t>(m_config->GetSetting("max_user_uid"));
	const char *lpszNonActive =  m_config->GetSetting("non_login_shell");
	vector<string> exceptuids = tokenize(m_config->GetSetting("except_user_uids"), " \t");
	set<uid_t> exceptuidset;
	objectid_t objectid;

	transform(exceptuids.begin(), exceptuids.end(), inserter(exceptuidset, exceptuidset.begin()), fromstring<const std::string,uid_t>);

	setpwent();
	while (true) {
		getpwent_r(&pws, buffer, PWBUFSIZE, &pw);
		if (pw == NULL)
			break;

		// system users don't have zarafa accounts
		if (pw->pw_uid < minuid || pw->pw_uid >= maxuid)
			continue;

		if (exceptuidset.find(pw->pw_uid) != exceptuidset.end())
			continue;

		if (!match.empty() && !matchUserObject(pw, match, ulFlags))
			continue;

		if (strcmp(pw->pw_shell, lpszNonActive) != 0)
			objectid = objectid_t(tostring(pw->pw_uid), ACTIVE_USER);
		else
			objectid = objectid_t(tostring(pw->pw_uid), NONACTIVE_USER);

		objectlist->push_back(objectsignature_t(objectid, getDBSignature(objectid) + pw->pw_gecos + pw->pw_name));
	}
	endpwent();

	return objectlist;
}

auto_ptr<signatures_t> UnixUserPlugin::getAllGroupObjects(const string &match, unsigned int ulFlags) throw(std::exception)
{
	auto_ptr<signatures_t> objectlist = auto_ptr<signatures_t>(new signatures_t());;
	char buffer[PWBUFSIZE];
	struct group grs, *gr = NULL;
	gid_t mingid = fromstring<const char *, gid_t>(m_config->GetSetting("min_group_gid"));
	gid_t maxgid = fromstring<const char *, gid_t>(m_config->GetSetting("max_group_gid"));
	vector<string> exceptgids = tokenize(m_config->GetSetting("except_group_gids"), " \t");
	set<gid_t> exceptgidset;
	objectid_t objectid;

	transform(exceptgids.begin(), exceptgids.end(), inserter(exceptgidset, exceptgidset.begin()), fromstring<const std::string,uid_t>);

	setgrent();
	while (true) {
		getgrent_r(&grs, buffer, PWBUFSIZE, &gr);
		if (gr == NULL)
			break;

		// system groups don't have zarafa accounts
		if (gr->gr_gid < mingid || gr->gr_gid >= maxgid)
			continue;

		if (exceptgidset.find(gr->gr_gid) != exceptgidset.end())
			continue;

		if (!match.empty() && !matchGroupObject(gr, match, ulFlags))
			continue;

		objectid = objectid_t(tostring(gr->gr_gid), DISTLIST_SECURITY);
		objectlist->push_back(objectsignature_t(objectid, gr->gr_name));
	}
	endgrent();

	return objectlist;
}

auto_ptr<signatures_t> UnixUserPlugin::getAllObjects(const objectid_t &companyid, objectclass_t objclass) throw(exception)
{
	ECRESULT er = erSuccess;
	auto_ptr<signatures_t> objectlist = auto_ptr<signatures_t>(new signatures_t());
	auto_ptr<signatures_t> objects;
	signatures_t::iterator iterObjs;
	map<objectclass_t, string> objectstrings;
	map<objectclass_t, string>::iterator iterStrings;
	DB_RESULT_AUTOFREE lpResult(m_lpDatabase);
	DB_ROW lpDBRow = NULL;
	string strQuery;
	string strSubQuery;
	unsigned int ulRows = 0;

	if (companyid.id.empty())
		m_logger->Log(EC_LOGLEVEL_DEBUG, "%s Class %x", __FUNCTION__, objclass);
	else
		m_logger->Log(EC_LOGLEVEL_DEBUG, "%s Company %s, Class %x", __FUNCTION__, companyid.id.c_str(), objclass);

	// use mutex to protect thread-unsafe setpwent()/setgrent() calls
	pthread_mutex_lock(m_plugin_lock);
	switch (OBJECTCLASS_TYPE(objclass)) {
	case OBJECTTYPE_UNKNOWN:
		objects = getAllUserObjects();
		objectlist->merge(*objects.get());
		objects = getAllGroupObjects();
		objectlist->merge(*objects.get());
		break;
	case OBJECTTYPE_MAILUSER:
		objects = getAllUserObjects();
		objectlist->merge(*objects.get());
		break;
	case OBJECTTYPE_DISTLIST:
		objects = getAllGroupObjects();
		objectlist->merge(*objects.get());
		break;
	case OBJECTTYPE_CONTAINER:
		pthread_mutex_unlock(m_plugin_lock);
		throw notsupported("objecttype not supported " + stringify(objclass));
	default:
		pthread_mutex_unlock(m_plugin_lock);
		throw runtime_error("Unknown object type " + stringify(objclass));
	}
	pthread_mutex_unlock(m_plugin_lock);

	// Cleanup old entries from deleted users/groups
	if (objectlist->empty())
		return objectlist;

	// Distribute all objects over the various types
	for (iterObjs = objectlist->begin(); iterObjs != objectlist->end(); iterObjs++) {
		if (!objectstrings[iterObjs->id.objclass].empty())
			objectstrings[iterObjs->id.objclass] += ", ";
		objectstrings[iterObjs->id.objclass] += iterObjs->id.id;
	}

	// make list of obsolete objects
	strQuery = "SELECT id, objectclass FROM " + (string)DB_OBJECT_TABLE + " WHERE ";
	for (iterStrings = objectstrings.begin(); iterStrings != objectstrings.end(); iterStrings++) {
		if (iterStrings != objectstrings.begin())
			strQuery += "AND ";
		strQuery +=
			"(externid NOT IN (" + iterStrings->second + ") "
			 "AND " + OBJECTCLASS_COMPARE_SQL("objectclass", iterStrings->first) + ")";
	}

	er = m_lpDatabase->DoSelect(strQuery, &lpResult);
	if (er != erSuccess) {
		m_logger->Log(EC_LOGLEVEL_ERROR, "Unix plugin: Unable to cleanup old entries");
		goto exit;
	}

	// check if we have obsolute objects
	ulRows = m_lpDatabase->GetNumRows(lpResult);
	if (!ulRows)
		goto exit;

	// clear our stringlist containing the valid entries and fill it with the deleted item ids
	objectstrings.clear();

	while ((lpDBRow = m_lpDatabase->FetchRow(lpResult)) != NULL) {
		if (!objectstrings[(objectclass_t)atoi(lpDBRow[1])].empty())
			objectstrings[(objectclass_t)atoi(lpDBRow[1])] += ", ";
		objectstrings[(objectclass_t)atoi(lpDBRow[1])] += lpDBRow[0];
	}

	// remove obsolete objects
	strQuery = "DELETE FROM " + (string)DB_OBJECT_TABLE + " WHERE ";
	for (iterStrings = objectstrings.begin(); iterStrings != objectstrings.end(); iterStrings++) {
		if (iterStrings != objectstrings.begin())
			strQuery += "OR ";
		strQuery += "(externid IN (" + iterStrings->second + ") AND objectclass = " + stringify(iterStrings->first) + ")";
	}

	er = m_lpDatabase->DoDelete(strQuery, &ulRows);
	if (er != erSuccess) {
		m_logger->Log(EC_LOGLEVEL_ERROR, "Unix plugin: Unable to cleanup old entries in object table");
		goto exit;
	} else if (ulRows > 0) {
		m_logger->Log(EC_LOGLEVEL_INFO, "Unix plugin: Cleaned-up %d old entries from object table", ulRows);
	}

	// Create subquery to select all ids which will be deleted
	strSubQuery =
		"SELECT o.id "
		"FROM " + (string)DB_OBJECT_TABLE + " AS o "
		"WHERE ";
	for (iterStrings = objectstrings.begin(); iterStrings != objectstrings.end(); iterStrings++) {
		if (iterStrings != objectstrings.begin())
			strSubQuery += "OR ";
		strSubQuery += "(o.externid IN (" + iterStrings->second + ") AND o.objectclass = " + stringify(iterStrings->first) + ")";
	}

	// remove obsolute object properties
	strQuery =
		"DELETE FROM " + (string)DB_OBJECTPROPERTY_TABLE + " "
		"WHERE objectid IN (" + strSubQuery + ")";
	er = m_lpDatabase->DoDelete(strQuery, &ulRows);
	if (er != erSuccess) {
		m_logger->Log(EC_LOGLEVEL_ERROR, "Unix plugin: Unable to cleanup old entries in objectproperty table");
		goto exit;
	} else if (ulRows > 0) {
		m_logger->Log(EC_LOGLEVEL_INFO, "Unix plugin: Cleaned-up %d old entries from objectproperty table", ulRows);
	}

	strQuery =
		"DELETE FROM " + (string)DB_OBJECT_RELATION_TABLE + " "
		"WHERE objectid IN (" + strSubQuery + ") "
		"OR parentobjectid IN (" + strSubQuery + ")";
	er = m_lpDatabase->DoDelete(strQuery, &ulRows);
	if (er != erSuccess) {
		m_logger->Log(EC_LOGLEVEL_ERROR, "Unix plugin: Unable to cleanup old entries in objectrelation table");
		goto exit;
	} else if (ulRows > 0) {
		m_logger->Log(EC_LOGLEVEL_INFO, "Unix plugin: Cleaned-up %d old entries from objectrelation table", ulRows);
	}

exit:
	return objectlist;
}

auto_ptr<objectdetails_t> UnixUserPlugin::getObjectDetails(const objectid_t &externid) throw(exception)
{
	ECRESULT er = erSuccess;
	char buffer[PWBUFSIZE];
	auto_ptr<objectdetails_t> ud = auto_ptr<objectdetails_t>(NULL);
	struct passwd pws;
	struct group grp;
	DB_RESULT_AUTOFREE lpResult(m_lpDatabase);
	DB_ROW lpRow = NULL;
	string strQuery;

	m_logger->Log(EC_LOGLEVEL_DEBUG, "%s", __FUNCTION__);

	switch (externid.objclass) {
	case ACTIVE_USER:
	case NONACTIVE_USER:
	case NONACTIVE_ROOM:
	case NONACTIVE_EQUIPMENT:
	case NONACTIVE_CONTACT:
		findUserID(externid.id, &pws, buffer);
		ud = objectdetailsFromPwent(&pws);
		break;
	case DISTLIST_GROUP:
	case DISTLIST_SECURITY:
		findGroupID(externid.id, &grp, buffer);
		ud = objectdetailsFromGrent(&grp);
		break;
	default:
		throw runtime_error("Object is wrong type");
		break;
	}

	strQuery = "SELECT id FROM " + (string)DB_OBJECT_TABLE + " WHERE externid = '" + externid.id + "' AND objectclass = " + stringify(externid.objclass);
	er = m_lpDatabase->DoSelect(strQuery, &lpResult);
	if (er != erSuccess)
		throw runtime_error(externid.id);

	lpRow = m_lpDatabase->FetchRow(lpResult);

	if (lpRow && lpRow[0]) {
		strQuery = "UPDATE " + (string)DB_OBJECT_TABLE + " SET externid='" + externid.id + "',objectclass=" + stringify(externid.objclass) + " WHERE id=" + lpRow[0];
		er = m_lpDatabase->DoUpdate(strQuery);
	} else {
		strQuery = "INSERT INTO " + (string)DB_OBJECT_TABLE + " (externid, objectclass) VALUES ('" + externid.id + "', " + stringify(externid.objclass) + ")";
		er = m_lpDatabase->DoInsert(strQuery);
	}
	if (er != erSuccess)
		throw runtime_error(externid.id);

	try {
		ud->MergeFrom(*DBPlugin::getObjectDetails(externid));
	} catch (...) { } // ignore errors; we'll try with just the information we have from Pwent

	return ud;
}

void UnixUserPlugin::changeObject(const objectid_t &id, const objectdetails_t &details, const std::list<std::string> *lpRemove) throw(std::exception)
{
	objectdetails_t tmp(details);

	// explicitly deny pam updates
	if (!details.GetPropString(OB_PROP_S_PASSWORD).empty())
		throw runtime_error("Updating the password is not allowed with the Unix plugin.");
	if (!details.GetPropString(OB_PROP_S_FULLNAME).empty())
		throw runtime_error("Updating the fullname is not allowed with the Unix plugin.");

	// Although updating username is invalid in Unix plugin, we still receive the OB_PROP_S_LOGIN field.
	// This is because zarafa-admin -u <username> sends it, and that requirement is because the
	// UpdateUserDetailsFromClient call needs to convert the username/company to details.
	// Remove the username detail to allow updating user information.
	tmp.SetPropString(OB_PROP_S_LOGIN, string());

	DBPlugin::changeObject(id, tmp, lpRemove);
}

objectsignature_t UnixUserPlugin::createObject(const objectdetails_t &details) throw(std::exception) {
	throw notimplemented("Creating objects is not supported when using the Unix user plugin.");
}

void UnixUserPlugin::deleteObject(const objectid_t &id) throw(std::exception) {
	throw notimplemented("Deleting objects is not supported when using the Unix user plugin.");
}

void UnixUserPlugin::modifyObjectId(const objectid_t &oldId, const objectid_t &newId) throw(std::exception)
{
	throw notimplemented("Modifying objectid is not supported when using the Unix user plugin.");
}

auto_ptr<signatures_t> UnixUserPlugin::getParentObjectsForObject(userobject_relation_t relation, const objectid_t &childid) throw(std::exception)
{
	auto_ptr<signatures_t> objectlist = auto_ptr<signatures_t>(new signatures_t());
	char buffer[PWBUFSIZE];
	struct passwd pws;
	struct group grs, *gr = NULL;
	gid_t mingid = fromstring<const char *, gid_t>(m_config->GetSetting("min_group_gid"));
	gid_t maxgid = fromstring<const char *, gid_t>(m_config->GetSetting("max_group_gid"));
	vector<string> exceptgids = tokenize(m_config->GetSetting("except_group_gids"), " \t");
	set<gid_t> exceptgidset;
	string username;

	if (relation != OBJECTRELATION_GROUP_MEMBER)
		return DBPlugin::getParentObjectsForObject(relation, childid);

	m_logger->Log(EC_LOGLEVEL_DEBUG, "%s Relation: Group member", __FUNCTION__);

	findUserID(childid.id, &pws, buffer);
	username = pws.pw_name; // make sure we have a _copy_ of the string, not just another pointer

	try {
		findGroupID(tostring(pws.pw_gid), &grs, buffer);
		objectlist->push_back(objectsignature_t(objectid_t(tostring(grs.gr_gid), DISTLIST_SECURITY), grs.gr_name));
	} catch (std::exception &e) {
		// Ignore error
	}	

	transform(exceptgids.begin(), exceptgids.end(), inserter(exceptgidset, exceptgidset.begin()), fromstring<const std::string,gid_t>);

	// This is a rather expensive operation: loop through all the
	// groups, and check each member for each group.
	pthread_mutex_lock(m_plugin_lock);
	setgrent();
	while (true) {
		getgrent_r(&grs, buffer, PWBUFSIZE, &gr);
		if (gr == NULL)
			break;

		// system users don't have zarafa accounts
		if (gr->gr_gid < mingid || gr->gr_gid >= maxgid)
			continue;

		if (exceptgidset.find(gr->gr_gid) != exceptgidset.end())
			continue;

		for (int i = 0; gr->gr_mem[i] != NULL; i++) {
			if (strcmp(username.c_str(), gr->gr_mem[i]) == 0) {
				objectlist->push_back(objectsignature_t(objectid_t(tostring(gr->gr_gid), DISTLIST_SECURITY), gr->gr_name));
				break;
			}
		}
	}
	endgrent();
	pthread_mutex_unlock(m_plugin_lock);

	// because users can be explicitly listed in their default group
	objectlist->sort();
	objectlist->unique();

	return objectlist;
}

auto_ptr<signatures_t> UnixUserPlugin::getSubObjectsForObject(userobject_relation_t relation, const objectid_t &parentid) throw(std::exception)
{
	auto_ptr<signatures_t> objectlist = auto_ptr<signatures_t>(new signatures_t());
	char buffer[PWBUFSIZE];
	struct passwd pws, *pw = NULL;
	struct group grp;
	uid_t minuid = fromstring<const char *, uid_t>(m_config->GetSetting("min_user_uid"));
	uid_t maxuid = fromstring<const char *, uid_t>(m_config->GetSetting("max_user_uid"));
	const char *lpszNonActive =  m_config->GetSetting("non_login_shell");
	gid_t mingid = fromstring<const char *, gid_t>(m_config->GetSetting("min_group_gid"));
	gid_t maxgid = fromstring<const char *, gid_t>(m_config->GetSetting("max_group_gid"));
	vector<string> exceptuids = tokenize(m_config->GetSetting("except_user_uids"), " \t");
	set<uid_t> exceptuidset;
	objectid_t objectid;

	if (relation != OBJECTRELATION_GROUP_MEMBER)
		return DBPlugin::getSubObjectsForObject(relation, parentid);

	m_logger->Log(EC_LOGLEVEL_DEBUG, "%s Relation: Group member", __FUNCTION__);

	findGroupID(parentid.id, &grp, buffer);
	for (unsigned int i = 0; grp.gr_mem[i] != NULL; i++) {
		try {
			objectlist->push_back(resolveUserName(grp.gr_mem[i]));
		} catch (std::exception &e) {
			// Ignore error
		}
	}

	transform(exceptuids.begin(), exceptuids.end(), inserter(exceptuidset, exceptuidset.begin()), fromstring<const std::string,uid_t>);

	// iterate through /etc/passwd users to find default group (eg. users) and add it to the list
	pthread_mutex_lock(m_plugin_lock);
	setpwent();
	while (true) {
		getpwent_r(&pws, buffer, PWBUFSIZE, &pw);
		if (pw == NULL)
			break;

		// system users don't have zarafa accounts
		if (pw->pw_uid < minuid || pw->pw_uid >= maxuid)
			continue;

		if (exceptuidset.find(pw->pw_uid) != exceptuidset.end())
			continue;

		// is it a member, and fits the default group in the range?
		if (pw->pw_gid == grp.gr_gid && pw->pw_gid >= mingid && pw->pw_gid < maxgid) {
			if (strcmp(pw->pw_shell, lpszNonActive) != 0)
				objectid = objectid_t(tostring(pw->pw_uid), ACTIVE_USER);
			else
				objectid = objectid_t(tostring(pw->pw_uid), NONACTIVE_USER);

			objectlist->push_back(objectsignature_t(objectid, getDBSignature(objectid) + pw->pw_gecos + pw->pw_name));
		}
	}
	endpwent();
	pthread_mutex_unlock(m_plugin_lock);

	// because users can be explicitly listed in their default group
	objectlist->sort();
	objectlist->unique();

	return objectlist;
}

void UnixUserPlugin::addSubObjectRelation(userobject_relation_t relation, const objectid_t &id, const objectid_t &member) throw(std::exception)
{
	if (relation != OBJECTRELATION_QUOTA_USERRECIPIENT && relation != OBJECTRELATION_USER_SENDAS)
		throw notimplemented("Adding object relations is not supported when using the Unix user plugin.");
	DBPlugin::addSubObjectRelation(relation, id, member);
}

void UnixUserPlugin::deleteSubObjectRelation(userobject_relation_t relation, const objectid_t &id, const objectid_t &member) throw(std::exception)
{
	if (relation != OBJECTRELATION_QUOTA_USERRECIPIENT && relation != OBJECTRELATION_USER_SENDAS)
		throw notimplemented("Deleting object relations is not supported when using the Unix user plugin.");
	DBPlugin::deleteSubObjectRelation(relation, id, member);
}

auto_ptr<signatures_t> UnixUserPlugin::searchObject(const string &match, unsigned int ulFlags) throw(std::exception)
{
	char buffer[PWBUFSIZE];
	struct passwd pws, *pw = NULL;
	auto_ptr<signatures_t> objectlist = auto_ptr<signatures_t>(new signatures_t());
	auto_ptr<signatures_t> objects;

	m_logger->Log(EC_LOGLEVEL_DEBUG, "%s %s flags:%x", __FUNCTION__, match.c_str(), ulFlags);

	pthread_mutex_lock(m_plugin_lock);
	objects = getAllUserObjects(match, ulFlags);
	objectlist->merge(*objects.get());
	objects = getAllGroupObjects(match, ulFlags);
	objectlist->merge(*objects.get());
	pthread_mutex_unlock(m_plugin_lock);

	// See if we get matches based on database details as well
	try {
		const char *search_props[] = { OP_EMAILADDRESS, NULL };
		objects = DBPlugin::searchObjects(match, search_props, NULL, ulFlags);

		for (signatures_t::iterator iter = objects->begin(); iter != objects->end(); iter++) {
			// the DBPlugin returned the DB signature, so we need to prepend this with the gecos signature
			errno = 0;
			getpwuid_r(atoi(iter->id.id.c_str()), &pws, buffer, PWBUFSIZE, &pw);
			errnoCheck(iter->id.id);

			if (pw == NULL)	// object not found anymore
				continue;

			objectlist->push_back(objectsignature_t(iter->id, iter->signature + pw->pw_gecos + pw->pw_name));
		}
	} catch (objectnotfound &e) {
			// Ignore exception, we will check lObjects.empty() later.
	} // All other exceptions should be thrown further up the chain.

	objectlist->sort();
	objectlist->unique();

	if (objectlist->empty())
		throw objectnotfound(string("unix_plugin: no match: ") + match);

	return objectlist;
}

auto_ptr<objectdetails_t> UnixUserPlugin::getPublicStoreDetails() throw(std::exception)
{
	throw notsupported("public store details");
}

auto_ptr<serverdetails_t> UnixUserPlugin::getServerDetails(const string &server) throw(std::exception)
{
	throw notsupported("server details");
}

auto_ptr<map<objectid_t, objectdetails_t> > UnixUserPlugin::getObjectDetails(const list<objectid_t> &objectids) throw (std::exception) {
	auto_ptr<map<objectid_t, objectdetails_t> > mapdetails = auto_ptr<map<objectid_t, objectdetails_t> >(new map<objectid_t,objectdetails_t>);
	auto_ptr<objectdetails_t> uDetails;
	list<objectid_t>::const_iterator iterID;
	objectdetails_t details;

	if (objectids.empty())
		return mapdetails;

	for (iterID = objectids.begin(); iterID != objectids.end(); iterID++) {
		try {
			uDetails = this->getObjectDetails(*iterID);
		}
		catch (objectnotfound &e) {
			// ignore not found error
			continue;
		}

		(*mapdetails)[*iterID] = (*uDetails.get());
	}
    
    return mapdetails;
}


// -------------
// private
// -------------

auto_ptr<objectdetails_t> UnixUserPlugin::objectdetailsFromPwent(struct passwd *pw) {
	auto_ptr<objectdetails_t> ud = auto_ptr<objectdetails_t>(new objectdetails_t());
	string gecos;
	size_t comma;

	ud->SetPropString(OB_PROP_S_LOGIN, string(pw->pw_name));
	if (strcmp(pw->pw_shell, m_config->GetSetting("non_login_shell")) == 0)
		ud->SetClass(NONACTIVE_USER);
	else
		ud->SetClass(ACTIVE_USER);

	gecos = m_iconv->convert(pw->pw_gecos);

	// gecos may contain room/phone number etc. too
	comma = gecos.find(",");
	if (comma != string::npos) {
		ud->SetPropString(OB_PROP_S_FULLNAME, gecos.substr(0,comma));
	} else {
		ud->SetPropString(OB_PROP_S_FULLNAME, gecos);
	}

	if (!strcmp(pw->pw_passwd, "x")) {
		// shadow password entry
		struct spwd spws, *spw = NULL;
		char sbuffer[PWBUFSIZE];

		getspnam_r(pw->pw_name, &spws, sbuffer, PWBUFSIZE, &spw);
		if (spw == NULL) {
			// invalid entry, must have a shadow password set in this case
			// throw objectnotfound(ud->id);
			// too bad that the password couldn't be found, but it's not that critical
			m_logger->Log(EC_LOGLEVEL_WARNING, "Warning: unable to find password for user '%s', errno: %s", pw->pw_name, strerror(errno));
			ud->SetPropString(OB_PROP_S_PASSWORD, string("x"));	// set invalid password entry, cannot login without a password
		} else {
			ud->SetPropString(OB_PROP_S_PASSWORD, string(spw->sp_pwdp));
		}
	} else if (!strcmp(pw->pw_passwd, "*") || !strcmp(pw->pw_passwd, "!")){
		throw objectnotfound(string());
	} else {
		ud->SetPropString(OB_PROP_S_PASSWORD, string(pw->pw_passwd));
	}
	
	// This may be overridden by settings in the database
	ud->SetPropString(OB_PROP_S_EMAIL, string(pw->pw_name) + "@" + m_config->GetSetting("default_domain"));

	return ud;
}

auto_ptr<objectdetails_t> UnixUserPlugin::objectdetailsFromGrent(struct group *gr) {
	auto_ptr<objectdetails_t> gd(new objectdetails_t(DISTLIST_SECURITY));

	gd->SetPropString(OB_PROP_S_LOGIN, string(gr->gr_name));
	gd->SetPropString(OB_PROP_S_FULLNAME, string(gr->gr_name));

	return gd;
}

std::string UnixUserPlugin::getDBSignature(const objectid_t &id)
{
	string strQuery;
	DB_RESULT_AUTOFREE lpResult(m_lpDatabase);
	DB_ROW lpDBRow = NULL;
	ECRESULT er = erSuccess;

	strQuery =
		"SELECT op.value "
		"FROM " + (string)DB_OBJECTPROPERTY_TABLE + " AS op "
		"JOIN " + (string)DB_OBJECT_TABLE + " AS o "
			"ON op.objectid = o.id "
		"WHERE o.externid = '" + m_lpDatabase->Escape(id.id) + "' "
			"AND o.objectclass = " + stringify(id.objclass) + " "
			"AND op.propname = '" + OP_MODTIME + "'";

	er = m_lpDatabase->DoSelect(strQuery, &lpResult);
	if (er != erSuccess)
		return string();

	lpDBRow = m_lpDatabase->FetchRow(lpResult);
	if (lpDBRow == NULL || lpDBRow[0] == NULL)
		return string();

	return lpDBRow[0];
}

void UnixUserPlugin::errnoCheck(const string &user) {
	if (errno) {
		char buffer[256];
		char *retbuf;
		retbuf = strerror_r(errno, buffer, 256);

		// from the getpwnam() man page: (notice the last or...)
		//  ERRORS
		//    0 or ENOENT or ESRCH or EBADF or EPERM or ...
		//    The given name or uid was not found.

		switch (errno) {
			// 0 is handled in top if()
		case ENOENT:
		case ESRCH:
		case EBADF:
		case EPERM:
			// calling function must check pw == NULL to throw objectnotfound()
			break;

		default:
			// broken system .. do not delete user from database
			throw runtime_error(string("unable to query for user ")+user+string(". Error: ")+retbuf);
		};

	}
}

