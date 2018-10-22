/*
 * SPDX-License-Identifier: AGPL-3.0-only
 * Copyright 2005 - 2016 Zarafa and its licensors
 */

#ifndef EC_USERSTORE_TABLE_H
#define EC_USERSTORE_TABLE_H

#include <kopano/zcdefs.h>
#include <kopano/Util.h>
/* #include "ECStoreObjectTable.h" */
#include "ECGenericObjectTable.h"
#include <kopano/pcuser.hpp>
#include <map>

struct soap;

namespace KC {

class ECSession;

struct ECUserStore {
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
};

class _kc_export_dycast ECUserStoreTable final : public ECGenericObjectTable {
	protected:
	_kc_hidden ECUserStoreTable(ECSession *, unsigned int flags, const ECLocale &);

public:
	_kc_hidden static ECRESULT Create(ECSession *, unsigned int flags, const ECLocale &, ECUserStoreTable **ret);
	_kc_hidden static ECRESULT QueryRowData(ECGenericObjectTable *, struct soap *, ECSession *, const ECObjectTableList *, const struct propTagArray *, const void *priv, struct rowSet **rowset, bool cache_table_data, bool table_limit);
	_kc_hidden virtual ECRESULT Load(void);

private:
	std::map<unsigned int, ECUserStore> m_mapUserStoreData;
	ALLOC_WRAP_FRIEND;
};

} /* namespace */

#endif
