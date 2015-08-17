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

#ifndef EC_STATS_TABLES_H
#define EC_STATS_TABLES_H

#include "ECGenericObjectTable.h"
#include "ECSession.h"

#include <string>
#include <list>
#include <map>

typedef struct _statstrings {
	std::string name;
	std::string description;
	std::string value;
} statstrings;

class ECSystemStatsTable : public ECGenericObjectTable {
protected:
	ECSystemStatsTable(ECSession *lpSession, unsigned int ulFlags, const ECLocale &locale);

public:
	static ECRESULT Create(ECSession *lpSession, unsigned int ulFlags, const ECLocale &locale, ECSystemStatsTable **lppTable);

	virtual ECRESULT Load();

	static ECRESULT QueryRowData(ECGenericObjectTable *lpThis, struct soap *soap, ECSession *lpSession, ECObjectTableList* lpRowList, struct propTagArray *lpsPropTagArray, void* lpObjectData, struct rowSet **lppRowSet, bool bCacheTableData, bool bTableLimit);

private:
	static void GetStatsCollectorData(const std::string &name, const std::string &description, const std::string &value, void *obj);

	std::map<unsigned int, statstrings> m_mapStatData;
	unsigned int id;
};


typedef struct _sessiondata {
	ECSESSIONID sessionid;
	ECSESSIONGROUPID sessiongroupid;
	std::string srcaddress;
	unsigned int port;
	unsigned int idletime;
	unsigned int capability;
	bool locked;
	int peerpid;
	std::string username;
	std::list<BUSYSTATE> busystates;
	double dblUser, dblSystem, dblReal;
	std::string version;
	std::string clientapp;
	unsigned int requests;
	std::string url;
	std::string proxyhost;
	std::string client_application_version, client_application_misc;
} sessiondata;

class ECSessionStatsTable : public ECGenericObjectTable {
protected:
	ECSessionStatsTable(ECSession *lpSession, unsigned int ulFlags, const ECLocale &locale);

public:
	static ECRESULT Create(ECSession *lpSession, unsigned int ulFlags, const ECLocale &locale, ECSessionStatsTable **lppTable);

	virtual ECRESULT Load();

	static ECRESULT QueryRowData(ECGenericObjectTable *lpThis, struct soap *soap, ECSession *lpSession, ECObjectTableList* lpRowList, struct propTagArray *lpsPropTagArray, void* lpObjectData, struct rowSet **lppRowSet, bool bCacheTableData, bool bTableLimit);

private:
	static void GetSessionData(ECSession *lpSession, void *obj);

	std::map<unsigned int, sessiondata> m_mapSessionData;
	unsigned int id;
};


class ECUserStatsTable : public ECGenericObjectTable {
protected:
	ECUserStatsTable(ECSession *lpSession, unsigned int ulFlags, const ECLocale &locale);

public:
	static ECRESULT Create(ECSession *lpSession, unsigned int ulFlags, const ECLocale &locale, ECUserStatsTable **lppTable);

	virtual ECRESULT Load();

	static ECRESULT QueryRowData(ECGenericObjectTable *lpThis, struct soap *soap, ECSession *lpSession, ECObjectTableList* lpRowList, struct propTagArray *lpsPropTagArray, void* lpObjectData, struct rowSet **lppRowSet, bool bCacheTableData, bool bTableLimit);

private:
	ECRESULT LoadCompanyUsers(ULONG ulCompanyId);
};


class ECCompanyStatsTable : public ECGenericObjectTable {
protected:
	ECCompanyStatsTable(ECSession *lpSession, unsigned int ulFlags, const ECLocale &locale);

public:
	static ECRESULT Create(ECSession *lpSession, unsigned int ulFlags, const ECLocale &locale, ECCompanyStatsTable **lppTable);

	virtual ECRESULT Load();

	static ECRESULT QueryRowData(ECGenericObjectTable *lpThis, struct soap *soap, ECSession *lpSession, ECObjectTableList* lpRowList, struct propTagArray *lpsPropTagArray, void* lpObjectData, struct rowSet **lppRowSet, bool bCacheTableData, bool bTableLimit);

private:
};

class ECServerStatsTable : public ECGenericObjectTable {
protected:
	ECServerStatsTable(ECSession *lpSession, unsigned int ulFlags, const ECLocale &locale);

public:
	static ECRESULT Create(ECSession *lpSession, unsigned int ulFlags, const ECLocale &locale, ECServerStatsTable **lppTable);

	virtual ECRESULT Load();

	static ECRESULT QueryRowData(ECGenericObjectTable *lpThis, struct soap *soap, ECSession *lpSession, ECObjectTableList* lpRowList, struct propTagArray *lpsPropTagArray, void* lpObjectData, struct rowSet **lppRowSet, bool bCacheTableData, bool bTableLimit);

private:
	std::map<unsigned int, std::string> m_mapServers;
};

// Link to provider/server
void zarafa_get_server_stats(unsigned int *lpulQueueLen, double *lpDblQueueAge, unsigned int *lpulThreads, unsigned int *lpulIdleThreads);

#endif
