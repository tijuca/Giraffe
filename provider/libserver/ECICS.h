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

#ifndef ECICS_H
#define ECICS_H

#include "ECSession.h"

#include <set>

// This class is used to pass SOURCEKEYs internally between parts of the server backend. You can use it as a char* to get the data, use size() to get the size,
// and have various ways of creating new SOURCEKEYs, including using a GUID and an ID, which is used for zarafa-generated source keys.

class SOURCEKEY {
public:
    SOURCEKEY() { lpData = NULL; ulSize = 0; };
    SOURCEKEY(const SOURCEKEY &s) { 
        if(&s == this) return; 
        if(s.ulSize == 0) { 
            ulSize = 0; 
            lpData = NULL;
        } else { 
            lpData = new char[s.ulSize]; 
            memcpy(lpData, s.lpData, s.ulSize); 
            ulSize = s.ulSize;
        } 
    }
    SOURCEKEY(unsigned int ulSize, char *lpData) { 
		if (lpData) {
			this->lpData = new char[ulSize];
			memcpy(this->lpData, lpData, ulSize);
		} else {
			this->lpData = NULL;
		}
		this->ulSize = ulSize;
    }
    SOURCEKEY(GUID guid, unsigned long long ullId) { 
        // Use 22-byte sourcekeys (16 bytes GUID + 6 bytes counter)
        ulSize = sizeof(GUID) + 6;
        lpData = new char [ulSize]; 
        memcpy(lpData, (char *)&guid, sizeof(guid)); 
        memcpy(lpData+sizeof(GUID), &ullId, ulSize - sizeof(GUID)); 
    }
    SOURCEKEY(struct xsd__base64Binary &sourcekey) {
        this->lpData = new char[sourcekey.__size];
        memcpy(this->lpData, sourcekey.__ptr, sourcekey.__size);
        this->ulSize = sourcekey.__size;
    }
    ~SOURCEKEY() { 
        if(lpData) delete [] lpData; 
    }
    
    SOURCEKEY&  operator= (const SOURCEKEY &s) { 
        if(&s == this) return *this; 
        if(lpData) delete [] lpData; 
        lpData = new char[s.ulSize]; 
        memcpy(lpData, s.lpData, s.ulSize); 
        ulSize = s.ulSize; 
        return *this; 
    }
    
    bool operator == (const SOURCEKEY &s) const {
        if(this == &s)
            return true;
        if(ulSize != s.ulSize)
            return false;
        return memcmp(lpData, s.lpData, s.ulSize) == 0;
    }
	
	bool operator < (const SOURCEKEY &s) const {
		if(this == &s)
			return false;
		if(ulSize == s.ulSize)
			return memcmp(lpData, s.lpData, ulSize) < 0;
		else if(ulSize > s.ulSize) {
			int d = memcmp(lpData, s.lpData, s.ulSize);
			return (d == 0) ? false : (d < 0);			// If the compared part is equal, the shortes is less (s)
		} else {
			int d = memcmp(lpData, s.lpData, ulSize);
			return (d == 0) ? true : (d < 0);			// If the compared part is equal, the shortes is less (this)
		}
	}
    
    operator unsigned char *() const { return (unsigned char *)lpData; }
    
    operator const std::string () const { return std::string((char *)lpData, ulSize); }
    
    unsigned int 	size() const { return ulSize; }
	bool			empty() const { return ulSize == 0; } 
private:
    char *lpData;
    unsigned int ulSize;
};

ECRESULT AddChange(BTSession *lpecSession, unsigned int ulSyncId, SOURCEKEY sSourceKey, SOURCEKEY sParentSourceKey, unsigned int ulChange, unsigned int ulFlags = 0, bool fForceNewChangeKey = false, std::string * lpstrChangeKey = NULL, std::string * lpstrChangeList = NULL);
ECRESULT AddABChange(BTSession *lpecSession, unsigned int ulChange, SOURCEKEY sSourceKey, SOURCEKEY sParentSourceKey);
ECRESULT GetChanges(struct soap *soap, ECSession *lpSession, SOURCEKEY sSourceKeyFolder, unsigned int ulSyncId, unsigned int ulChangeId, unsigned int ulChangeType, unsigned int ulFlags, struct restrictTable *lpsRestrict, unsigned int *lpulMaxChangeId, icsChangesArray **lppChanges);
ECRESULT GetSyncStates(struct soap *soap, ECSession *lpSession, mv_long ulaSyncId, syncStateArray *lpsaSyncState);
void* CleanupSyncsTable(void* lpTmpMain);
void* CleanupChangesTable(void* lpTmpMain);
void *CleanupSyncedMessagesTable(void *lpTmpMain);

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
 *
 * @return HRESULT
 */
ECRESULT AddToLastSyncedMessagesSet(ECDatabase *lpDatabase, unsigned int ulSyncId, const SOURCEKEY &sSourceKey, const SOURCEKEY &sParentSourceKey);

ECRESULT RemoveFromLastSyncedMessagesSet(ECDatabase *lpDatabase, unsigned int ulSyncId, const SOURCEKEY &sSourceKey, const SOURCEKEY &sParentSourceKey);

#endif
