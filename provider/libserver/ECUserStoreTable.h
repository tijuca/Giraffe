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

#ifndef EC_USERSTORE_TABLE_H
#define EC_USERSTORE_TABLE_H

/* #include "ECStoreObjectTable.h" */
#include "ECGenericObjectTable.h"
#include "ZarafaUser.h"
#include <map>

class ECSession;

typedef struct {
	long long		ulUserId;		// id of user (-1 if there is no user)
	objectid_t		sExternId;		// externid of user
	std::string		strUsername;	// actual username from ECUserManagement
	std::string		strGuessname;	// "guess" from user_name column in stores table
	unsigned int	ulCompanyId;	// company id of store (or user if store is not found)
	GUID			sGuid;			// The GUID of the store
	unsigned int	ulStoreType;	// Type of the store (private, public, archive)
	unsigned int	ulObjId;		// Hierarchyid of the store
	std::string		strCompanyName;	// Company name of the store. (can be empty)
	time_t			tModTime;		// Modification time of the store
	unsigned long long ullStoreSize;// Size of the store
} ECUserStore;

class ECUserStoreTable : public ECGenericObjectTable
{
protected:
	ECUserStoreTable(ECSession *lpSession, unsigned int ulFlags, const ECLocale &locale);
	virtual ~ECUserStoreTable();

public:
	static ECRESULT Create(ECSession *lpSession, unsigned int ulFlags, const ECLocale &locale, ECUserStoreTable **lppTable);

	static ECRESULT QueryRowData(ECGenericObjectTable *lpThis, struct soap *soap, ECSession *lpSession, ECObjectTableList* lpRowList, struct propTagArray *lpsPropTagArray, void* lpObjectData, struct rowSet **lppRowSet, bool bCacheTableData, bool bTableLimit);

    virtual ECRESULT Load();

private:
	std::map<unsigned int, ECUserStore> m_mapUserStoreData;
};

#endif
