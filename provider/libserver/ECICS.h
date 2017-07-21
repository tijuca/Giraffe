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

#ifndef ECICS_H
#define ECICS_H

#include <kopano/zcdefs.h>
#include "ECSession.h"

#include <set>

struct soap;

namespace KC {

// This class is used to pass SOURCEKEYs internally between parts of the server backend. You can use it as a char* to get the data, use size() to get the size,
// and have various ways of creating new SOURCEKEYs, including using a GUID and an ID, which is used for kopano-generated source keys.

class SOURCEKEY _kc_final {
public:
	SOURCEKEY(void) : ulSize(0) {}
	SOURCEKEY(const SOURCEKEY &s) : ulSize(s.ulSize)
	{
		if (ulSize > 0) {
			lpData.reset(new char[s.ulSize]);
			memcpy(lpData.get(), s.lpData.get(), s.ulSize);
		}
	}
	SOURCEKEY(SOURCEKEY &&o) :
	    ulSize(o.ulSize), lpData(std::move(o.lpData))
	{}
	SOURCEKEY(unsigned int z, const char *d) : ulSize(z)
	{
		if (d != nullptr && z > 0) {
			lpData.reset(new char[ulSize]);
			memcpy(lpData.get(), d, ulSize);
		}
	}
	SOURCEKEY(const GUID &guid, unsigned long long ullId) :
		ulSize(sizeof(GUID) + 6), lpData(new char[ulSize])
	{
		memcpy(&lpData[0], &guid, sizeof(guid));
		memcpy(&lpData[sizeof(GUID)], &ullId, ulSize - sizeof(GUID));
	}
	SOURCEKEY(const struct xsd__base64Binary &sourcekey) :
		ulSize(sourcekey.__size)
	{
		if (ulSize > 0) {
			lpData.reset(new char[ulSize]);
			memcpy(this->lpData.get(), sourcekey.__ptr, sourcekey.__size);
		}
	}
    SOURCEKEY&  operator= (const SOURCEKEY &s) {
        if(&s == this) return *this; 
		lpData.reset(new char[s.ulSize]);
		ulSize = s.ulSize;
		memcpy(lpData.get(), s.lpData.get(), ulSize);
        return *this; 
    }
    
    bool operator == (const SOURCEKEY &s) const {
        if(this == &s)
            return true;
        if(ulSize != s.ulSize)
            return false;
		return memcmp(lpData.get(), s.lpData.get(), s.ulSize) == 0;
    }
	
	bool operator < (const SOURCEKEY &s) const {
		if(this == &s)
			return false;
		if(ulSize == s.ulSize)
			return memcmp(lpData.get(), s.lpData.get(), ulSize) < 0;
		else if(ulSize > s.ulSize) {
			int d = memcmp(lpData.get(), s.lpData.get(), s.ulSize);
			return (d == 0) ? false : (d < 0);			// If the compared part is equal, the shortes is less (s)
		} else {
			int d = memcmp(lpData.get(), s.lpData.get(), ulSize);
			return (d == 0) ? true : (d < 0);			// If the compared part is equal, the shortes is less (this)
		}
	}
    
	operator unsigned char *(void) const { return reinterpret_cast<unsigned char *>(lpData.get()); }
	operator std::string(void) const { return std::string(lpData.get(), ulSize); }
    unsigned int 	size() const { return ulSize; }
	bool			empty() const { return ulSize == 0; } 
private:
	unsigned int ulSize;
	std::unique_ptr<char[]> lpData;
};

ECRESULT AddChange(BTSession *lpecSession, unsigned int ulSyncId, const SOURCEKEY &sSourceKey, const SOURCEKEY &sParentSourceKey, unsigned int ulChange, unsigned int ulFlags = 0, bool fForceNewChangeKey = false, std::string *lpstrChangeKey = NULL, std::string *lpstrChangeList = NULL);
ECRESULT AddABChange(BTSession *lpecSession, unsigned int ulChange, SOURCEKEY sSourceKey, SOURCEKEY sParentSourceKey);
ECRESULT GetChanges(struct soap *soap, ECSession *lpSession, SOURCEKEY sSourceKeyFolder, unsigned int ulSyncId, unsigned int ulChangeId, unsigned int ulChangeType, unsigned int ulFlags, struct restrictTable *lpsRestrict, unsigned int *lpulMaxChangeId, icsChangesArray **lppChanges);
ECRESULT GetSyncStates(struct soap *soap, ECSession *lpSession, mv_long ulaSyncId, syncStateArray *lpsaSyncState);
extern _kc_export void *CleanupSyncsTable(void *);
extern _kc_export void *CleanupSyncedMessagesTable(void *);

/**
 * Adds the message specified by sSourceKey to the last set of syncedmessages for the syncer identified by
 * ulSyncId. This causes GetChanges to know that the message is available on the client so it doesn't need
 * to send a add to the client.
 *
 * @param[in]	lpDatabase
 *					Pointer to the database.
 * @param[in]	ulSyncId
 *					The sync id of the client for whom the message is to be registered.
 * @param[in]	sSourceKey
 *					The source key of the message.
 * @param[in]	sParentSourceKey
 *					THe source key of the folder containing the message.
 */
ECRESULT AddToLastSyncedMessagesSet(ECDatabase *lpDatabase, unsigned int ulSyncId, const SOURCEKEY &sSourceKey, const SOURCEKEY &sParentSourceKey);

ECRESULT CheckWithinLastSyncedMessagesSet(ECDatabase *lpDatabase, unsigned int ulSyncId, const SOURCEKEY &sSourceKey);
ECRESULT RemoveFromLastSyncedMessagesSet(ECDatabase *lpDatabase, unsigned int ulSyncId, const SOURCEKEY &sSourceKey, const SOURCEKEY &sParentSourceKey);

} /* namespace */

#endif
