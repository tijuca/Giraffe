/*
 * Copyright 2005 - 2015  Zarafa B.V. and its licensors
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License, version 3,
 * as published by the Free Software Foundation with the following
 * additional terms according to sec. 7:
 * 
 * "Zarafa" is a registered trademark of Zarafa B.V.
 * The licensing of the Program under the AGPL does not imply a trademark 
 * license. Therefore any rights, title and interest in our trademarks 
 * remain entirely with us.
 * 
 * Our trademark policy (see TRADEMARKS.txt) allows you to use our trademarks
 * in connection with Propagation and certain other acts regarding the Program.
 * In any case, if you propagate an unmodified version of the Program you are
 * allowed to use the term "Zarafa" to indicate that you distribute the Program.
 * Furthermore you may use our trademarks where it is necessary to indicate the
 * intended purpose of a product or service provided you use it in accordance
 * with honest business practices. For questions please contact Zarafa at
 * trademark@zarafa.com.
 *
 * The interactive user interface of the software displays an attribution 
 * notice containing the term "Zarafa" and/or the logo of Zarafa. 
 * Interactive user interfaces of unmodified and modified versions must 
 * display Appropriate Legal Notices according to sec. 5 of the GNU Affero 
 * General Public License, version 3, when you propagate unmodified or 
 * modified versions of the Program. In accordance with sec. 7 b) of the GNU 
 * Affero General Public License, version 3, these Appropriate Legal Notices 
 * must retain the logo of Zarafa or display the words "Initial Development 
 * by Zarafa" if the display of the logo is not reasonably feasible for
 * technical reasons.
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

#ifndef ZARAFAUSER_H
#define ZARAFAUSER_H

#include <map>
#include <list>
#include <string>
#include <stringutil.h>
#include "ECDefs.h"

/* Extern object */
typedef struct objectid_t {
	objectid_t(const std::string &id, objectclass_t objclass);
	explicit objectid_t(const std::string &str);
	objectid_t(objectclass_t objclass);
	objectid_t();

	bool operator==(const objectid_t &x);
	bool operator!=(const objectid_t &x);
	std::string tostring() const;

	std::string id;
	objectclass_t objclass;
} objectid_t;

inline bool operator < (const objectid_t &a, const objectid_t &b) {
	if (a.objclass < b.objclass)
		return true;
	if ((a.objclass == b.objclass) && a.id < b.id)
		return true;
	return false;
}

/* Object properties
 * Prefix: OB_PROP
 * Type: S - String
 *       B - Boolean
 *       I - Integer
 *		 O - objectid_t
 * Name: Property name
 */
enum property_key_t {
	OB_PROP_B_AB_HIDDEN,
	OB_PROP_S_FULLNAME,
	OB_PROP_S_LOGIN,
	OB_PROP_S_PASSWORD,
	OB_PROP_I_COMPANYID,		/* COMPANY localid */
	OB_PROP_O_COMPANYID,		/* COMPANY externid */
	OB_PROP_I_ADMINLEVEL,
	OB_PROP_S_RESOURCE_DESCRIPTION,
	OB_PROP_I_RESOURCE_CAPACITY,
	OB_PROP_S_EMAIL,
	OB_PROP_LS_ALIASES,
	OB_PROP_I_SYSADMIN,		/* SYSADMIN localid */
	OB_PROP_O_SYSADMIN,		/* SYSADMIN externid */
	OB_PROP_LS_CERTIFICATE,
	OB_PROP_LI_SENDAS,		/* SENDAS localid .. */
	OB_PROP_LO_SENDAS,		/* SENDAS externid */
	OB_PROP_S_EXCH_DN,
	OB_PROP_O_EXTERNID,
	OB_PROP_S_SERVERNAME,
	OB_PROP_S_HTTPPATH,
	OB_PROP_S_SSLPATH,
	OB_PROP_S_FILEPATH,
	OB_PROP_MAX				/* Not a propname, keep as last entry */
};

typedef std::map<property_key_t, std::string> property_map;
typedef std::map<property_key_t, std::list<std::string> > property_mv_map;

/** One user's / group's details
 */
class objectdetails_t {
public:
	objectdetails_t(const objectdetails_t &objdetails);
	objectdetails_t(objectclass_t objclass);
	objectdetails_t();

	unsigned int 			GetPropInt(const property_key_t &propname) const;
	bool					GetPropBool(const property_key_t &propname) const;
	std::string				GetPropString(const property_key_t &propname) const;
	std::list<unsigned int>	GetPropListInt(const property_key_t &propname) const;
	std::list<std::string>	GetPropListString(const property_key_t &propname) const;
	objectid_t				GetPropObject(const property_key_t &propname) const;
	std::list<objectid_t>	GetPropListObject(const property_key_t &propname) const;
	property_map			GetPropMapAnonymous() const;
	property_mv_map			GetPropMapListAnonymous() const;
	bool					HasProp(const property_key_t &propname) const;

	bool			PropListStringContains(const property_key_t &propname, const std::string &value, bool ignoreCase = false) const;

	void			SetPropInt(const property_key_t &propname, unsigned int value);
	void			SetPropBool(const property_key_t &propname, bool value);
	void			SetPropString(const property_key_t &propname, const std::string &value);
	void			SetPropListString(const property_key_t &propname, const std::list<std::string> &value);
	void			SetPropObject(const property_key_t &propname, const objectid_t &value);
	void			SetPropListObject(const property_key_t &propname, const std::list<objectid_t> &value);

	/* "mv" props */
	void			AddPropInt(const property_key_t &propname, unsigned int value);
	void			AddPropString(const property_key_t &propname, const std::string &value);
	void			AddPropObject(const property_key_t &propname, const objectid_t &value);
	void			ClearPropList(const property_key_t &propname);

	void			MergeFrom(const objectdetails_t &from);

	void			SetClass(objectclass_t objclass);
	objectclass_t	GetClass() const;

	virtual unsigned int GetObjectSize();

	std::string		ToStr();

private:
	objectclass_t m_objclass;
	property_map m_mapProps;
	property_mv_map m_mapMVProps;
};

/** Quota Details
 */
class quotadetails_t {
public:
	quotadetails_t() : 
		bUseDefaultQuota(true), bIsUserDefaultQuota(false),
		llWarnSize(0), llSoftSize(0), llHardSize(0) {}

	bool bUseDefaultQuota;
	bool bIsUserDefaultQuota; /* Default quota for users within company */
	long long llWarnSize;
	long long llSoftSize;
	long long llHardSize;
};

/** Server Details
*/
class serverdetails_t {
public:
	serverdetails_t(const std::string &servername = std::string());

	void SetHostAddress(const std::string &hostaddress);
	void SetFilePath(const std::string &filepath);
	void SetHttpPort(unsigned port);
	void SetSslPort(unsigned port);
	void SetProxyPath(const std::string &proxy);

	const std::string&	GetServerName() const;
	const std::string&	GetHostAddress() const;
	unsigned			GetHttpPort() const;
	unsigned			GetSslPort() const;

	std::string	GetFilePath() const;
	std::string	GetHttpPath() const;
	std::string	GetSslPath() const;
	std::string	GetProxyPath() const;

private:
	std::string m_strServerName;
	std::string m_strHostAddress;
	std::string m_strFilePath;
	unsigned	m_ulHttpPort;
	unsigned	m_ulSslPort;
	std::string	m_strProxyPath;
};

typedef std::list<std::string> serverlist_t;

#endif
