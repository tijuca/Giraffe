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
#include <algorithm>

#include <errno.h>
#include <assert.h>
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif

#include "EMSAbTag.h"
#include "ECConfig.h"
#include "ECDefs.h"
#include "ECLogger.h"
#include "ECPluginSharedData.h"

#include "stringutil.h"
#include "md5.h"

using namespace std;
#include "ECDatabaseFactory.h"
#include "DBUserPlugin.h"
#include "ecversion.h"

extern "C" {
	UserPlugin* getUserPluginInstance(pthread_mutex_t *pluginlock, ECPluginSharedData *shareddata) {
		return new DBUserPlugin(pluginlock, shareddata);
	}

	void deleteUserPluginInstance(UserPlugin *up) {
		delete up;
	}

	int getUserPluginVersion() {
		return PROJECT_VERSION_REVISION;
	}
}

DBUserPlugin::DBUserPlugin(pthread_mutex_t *pluginlock, ECPluginSharedData *shareddata)
	: DBPlugin(pluginlock, shareddata)
{
	if (m_bDistributed)
		throw notsupported("Distributed Zarafa not supported when using the Database Plugin");
}

DBUserPlugin::~DBUserPlugin()
{
}

void DBUserPlugin::InitPlugin() throw(exception)
{
	DBPlugin::InitPlugin();
}

objectsignature_t DBUserPlugin::resolveName(objectclass_t objclass, const string &name, const objectid_t &company) throw(exception)
{
	objectid_t	id;
	ECRESULT	er;
	string		strQuery;
	DB_RESULT_AUTOFREE	lpResult(m_lpDatabase);
	DB_ROW		lpDBRow = NULL;
	DB_LENGTHS	lpDBLen = NULL;
	string signature;
	char *lpszSearchProperty;

	if (company.id.empty())
		m_logger->Log(EC_LOGLEVEL_DEBUG, "%s Class %x, Name %s", __FUNCTION__, objclass, name.c_str());
	else
		m_logger->Log(EC_LOGLEVEL_DEBUG, "%s Class %x, Name %s, Company %s", __FUNCTION__, objclass, name.c_str(), company.id.c_str());

	switch (objclass) {
	case OBJECTCLASS_USER:
	case ACTIVE_USER:
	case NONACTIVE_USER:
	case NONACTIVE_ROOM:
	case NONACTIVE_EQUIPMENT:
	case NONACTIVE_CONTACT:
		lpszSearchProperty = OP_LOGINNAME;
		break;
	case OBJECTCLASS_DISTLIST:
	case DISTLIST_GROUP:
	case DISTLIST_SECURITY:
	case DISTLIST_DYNAMIC:
		lpszSearchProperty = OP_GROUPNAME;
		break;
	case CONTAINER_COMPANY:
		lpszSearchProperty = OP_COMPANYNAME;
		break;
	case OBJECTCLASS_CONTAINER:
	case CONTAINER_ADDRESSLIST:
	case OBJECTCLASS_UNKNOWN:
		// We don't have a search property, search through all fields
		lpszSearchProperty = NULL;
		break;
	default:
		throw runtime_error("Object is wrong type");
	}

	/*
	 * Join the DB_OBJECT_TABLE together twice. This is done because
	 * we need to link the entry for the user and the entry for the
	 * company into a single line. Once those are all linked we can
	 * set the WHERE restrictions to get the correct line.
	 */
	strQuery =
		"SELECT DISTINCT o.externid, o.objectclass, modtime.value, user.value "
		"FROM " + (string)DB_OBJECT_TABLE + " AS o "
		"JOIN " + (string)DB_OBJECTPROPERTY_TABLE + " AS user "
			"ON user.objectid = o.id "
			"AND upper(user.value) = upper('" + m_lpDatabase->Escape(name) + "') ";
	if (lpszSearchProperty)
		strQuery += "AND user.propname = '" + (string)lpszSearchProperty + "' ";

	if (m_bHosted && !company.id.empty()) {
		// join company, company itself inclusive
		strQuery +=
			"JOIN " + (string)DB_OBJECTPROPERTY_TABLE + " AS usercompany "
				"ON usercompany.objectid = o.id "
				"AND (usercompany.propname = '" + OP_COMPANYID + "' AND usercompany.value = hex('" + m_lpDatabase->Escape(company.id) + "') OR "
					"usercompany.propname = '" + OP_COMPANYNAME + "' AND usercompany.objectid = '" + m_lpDatabase->Escape(company.id) + "')";
	}
	strQuery +=
		"LEFT JOIN " + (string)DB_OBJECTPROPERTY_TABLE + " AS modtime "
			"ON modtime.propname = '" + OP_MODTIME + "' "
			"AND modtime.objectid = o.id ";
	if (objclass != OBJECTCLASS_UNKNOWN)
		strQuery += "WHERE " + OBJECTCLASS_COMPARE_SQL("o.objectclass", objclass);

	er = m_lpDatabase->DoSelect(strQuery, &lpResult);
	if(er != erSuccess) {
		throw runtime_error(string("db_query: ") + strerror(er));
	}

	while ((lpDBRow = m_lpDatabase->FetchRow(lpResult)) != NULL) {
		if (lpDBRow[0] == NULL || lpDBRow[1] == NULL || lpDBRow[3] == NULL)
			throw runtime_error(string("db_row_failed: object null"));

		if (stricmp(lpDBRow[3], name.c_str()) != 0)
			continue;

		lpDBLen = m_lpDatabase->FetchRowLengths(lpResult);
		if (lpDBLen == NULL || lpDBLen[0] == 0)
			throw runtime_error(string("db_row_failed: object empty"));

		if(lpDBRow[2] != NULL)
			signature = lpDBRow[2];

		id = objectid_t(string(lpDBRow[0], lpDBLen[0]), (objectclass_t)atoi(lpDBRow[1]));
		return objectsignature_t(id, signature);
	}

	throw objectnotfound(name);
}


objectsignature_t DBUserPlugin::authenticateUser(const string &username, const string &password, const objectid_t &company) throw(exception)
{
	objectid_t	objectid;
	std::string signature;
	ECRESULT	er;
	string		strQuery;
	DB_RESULT_AUTOFREE lpResult(m_lpDatabase);
	DB_ROW		lpDBRow = NULL;
	DB_LENGTHS	lpDBLen = NULL;

	std::string salt;
	MD5*		crypt;
	char*		hex;
	std::string strMD5;

	/*
	 * Join the DB_OBJECT_TABLE together twice. This is done because
	 * we need to link the entry for the user and the entry for the
	 * company into a single line. Once those are all linked we can
	 * set the WHERE restrictions to get the correct line.
	 */
	strQuery =
		"SELECT pass.propname, pass.value, o.externid, modtime.value, op.value "
		"FROM " + (string)DB_OBJECT_TABLE + " AS o "
		"JOIN " + (string)DB_OBJECTPROPERTY_TABLE + " AS op "
		"ON o.id = op.objectid "
		"JOIN " + (string)DB_OBJECTPROPERTY_TABLE + " AS pass "
		"ON pass.objectid = o.id ";
	if (m_bHosted && !company.id.empty()) {
		// join company, company itself inclusive
		strQuery +=
			"JOIN " + (string)DB_OBJECTPROPERTY_TABLE + " AS usercompany "
				"ON usercompany.objectid = o.id "
				"AND (usercompany.propname = '" + OP_COMPANYID + "' AND usercompany.value = hex('" + m_lpDatabase->Escape(company.id) + "') OR "
					"usercompany.propname = '" + OP_COMPANYNAME + "' AND usercompany.objectid = '" + m_lpDatabase->Escape(company.id) + "')";
	}
	strQuery +=
		"LEFT JOIN " + (string)DB_OBJECTPROPERTY_TABLE + " AS modtime "
		"ON modtime.objectid = o.id "
		"AND modtime.propname = '" + OP_MODTIME + "' "
		"WHERE " + OBJECTCLASS_COMPARE_SQL("o.objectclass", ACTIVE_USER) + " "
		"AND op.propname = '" + (string)OP_LOGINNAME + "' "
		"AND op.value = '" + m_lpDatabase->Escape(username) + "' "
		"AND pass.propname = '" + (string)OP_PASSWORD "'";

	er = m_lpDatabase->DoSelect(strQuery, &lpResult);
	if(er != erSuccess) {
		throw runtime_error(string("db_query: ") + strerror(er));
	}

	while ((lpDBRow = m_lpDatabase->FetchRow(lpResult)) != NULL) {

		if (lpDBRow[0] == NULL || lpDBRow[1] == NULL || lpDBRow[2] == NULL || lpDBRow[4] == NULL)
			throw runtime_error("Trying to authenticate failed: database error");

		if (stricmp(lpDBRow[4], username.c_str()) != 0)
			continue;

		lpDBLen = m_lpDatabase->FetchRowLengths(lpResult);
		if (lpDBLen == NULL || lpDBLen[2] == 0)
			throw runtime_error("Trying to authenticate failed: database error");

		if(strcmp(lpDBRow[0], OP_PASSWORD) == 0)
		{
			// Check Password
			crypt = new MD5();
			salt = lpDBRow[1];
			salt.resize(8);

			crypt->update((unsigned char*)salt.c_str(), (unsigned int)salt.length());
			crypt->update((unsigned char*)password.c_str(), (unsigned int)password.size());
			crypt->finalize();

			hex = crypt->hex_digest();
			strMD5 = salt+hex;
			delete [] hex;
			delete crypt;

			if(strMD5.compare((string)lpDBRow[1]) == 0) {
				objectid = objectid_t(string(lpDBRow[2], lpDBLen[2]), ACTIVE_USER);	// Password is oke
			} else {
				throw login_error("Trying to authenticate failed: wrong username or password");
			}
		} else {
			throw login_error("Trying to authenticate failed: wrong username or password");
		}

		if(lpDBRow[3] != NULL)
			signature = lpDBRow[3];

		return objectsignature_t(objectid, signature);
	}

	throw login_error("Trying to authenticate failed: wrong username or password");
}

auto_ptr<signatures_t> DBUserPlugin::searchObject(const string &match, unsigned int ulFlags) throw(std::exception)
{
	const char *search_props[] =
	{
		OP_LOGINNAME, OP_FULLNAME, OP_EMAILADDRESS,	/* This will match all users */
		OP_GROUPNAME,								/* This will match all groups */
		OP_COMPANYNAME,								/* This will match all companies */
		NULL,
	};

	m_logger->Log(EC_LOGLEVEL_DEBUG, "%s %s flags:%x", __FUNCTION__, match.c_str(), ulFlags);

	return searchObjects(match.c_str(), search_props, NULL, ulFlags);
}

void DBUserPlugin::modifyObjectId(const objectid_t &oldId, const objectid_t &newId) throw(std::exception)
{
	throw notimplemented("Modifying objects is not supported when using the DB user plugin.");
}

void DBUserPlugin::setQuota(const objectid_t &objectid, const quotadetails_t &quotadetails) throw(std::exception)
{
	string strQuery;
	ECRESULT er = erSuccess;
	DB_RESULT_AUTOFREE lpResult(m_lpDatabase);
	DB_ROW lpDBRow = NULL;

	// check if user exist
	strQuery =
		"SELECT o.externid "
		"FROM " + (string)DB_OBJECT_TABLE + " AS o "
		"WHERE o.externid='" + m_lpDatabase->Escape(objectid.id) + "' "
			"AND " + OBJECTCLASS_COMPARE_SQL("o.objectclass", objectid.objclass);

	er = m_lpDatabase->DoSelect(strQuery, &lpResult);
	if(er != erSuccess)
		throw runtime_error(string("db_query: ") + strerror(er));

	if(m_lpDatabase->GetNumRows(lpResult) != 1)
		throw objectnotfound(objectid.id);

	lpDBRow = m_lpDatabase->FetchRow(lpResult);
	if(lpDBRow == NULL || lpDBRow[0] == NULL)
		throw runtime_error(string("db_row_failed: object null"));

	DBPlugin::setQuota(objectid, quotadetails);
}

auto_ptr<objectdetails_t> DBUserPlugin::getPublicStoreDetails() throw(std::exception)
{
	throw notsupported("public store details");
}

auto_ptr<serverdetails_t> DBUserPlugin::getServerDetails(const string &server) throw(std::exception)
{
	throw notsupported("server details");
}

void DBUserPlugin::addSubObjectRelation(userobject_relation_t relation, const objectid_t &parentobject, const objectid_t &childobject) throw(std::exception)
{
	ECRESULT er = erSuccess;
	string strQuery;
	DB_RESULT_AUTOFREE  lpResult(m_lpDatabase);

	// Check if parent exist
	strQuery =
		"SELECT o.externid "
		"FROM " + (string)DB_OBJECT_TABLE + " AS o "
		"WHERE o.externid='" + m_lpDatabase->Escape(parentobject.id) + "' "
			"AND " + OBJECTCLASS_COMPARE_SQL("o.objectclass", parentobject.objclass);
	er = m_lpDatabase->DoSelect(strQuery, &lpResult);
	if (er != erSuccess)
		throw runtime_error(string("db_query: ") + strerror(er));

	if(m_lpDatabase->GetNumRows(lpResult) != 1)
		throw objectnotfound("db_user: Relation does not exist, id:" + parentobject.id);

	DBPlugin::addSubObjectRelation(relation, parentobject, childobject);
}
