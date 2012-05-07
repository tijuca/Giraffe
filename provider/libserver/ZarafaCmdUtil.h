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

#ifndef ZARAFACMD_UTIL_H
#define ZARAFACMD_UTIL_H

#include "ECICS.h"

// Above EC_TABLE_CHANGE_THRESHOLD, a TABLE_CHANGE notification is sent instead of individual notifications
#define EC_TABLE_CHANGE_THRESHOLD 10

class EntryId {
public:
    EntryId() { 
        updateStruct();
    }
    EntryId(const EntryId &s) { 
        m_data = s.m_data;
    }
    EntryId(const entryId *entryid) {
        if(entryid) 
            m_data = std::string((char *)entryid->__ptr, entryid->__size);
        else 
            m_data.clear();
        updateStruct();
    }
    EntryId(const entryId& entryid) {
        m_data = std::string((char *)entryid.__ptr, entryid.__size);
        updateStruct();
    }
    EntryId(const std::string& data) {
        m_data = data;
        updateStruct();
    }
    ~EntryId() { 
    }
    
    EntryId&  operator= (const EntryId &s) { 
        m_data = s.m_data;
        updateStruct();
        return *this;
    }

    unsigned int type() const {
        EID_V0 *d = (EID_V0 *)m_data.data();
        
        return d->ulType;
    }
    
    bool operator == (const EntryId &s) const {
        return m_data == s.m_data;
    }
	
	bool operator < (const EntryId &s) const {
	    return m_data < s.m_data;
	}

    operator const std::string& () const { return m_data; }    
    operator unsigned char *() const { return (unsigned char *)m_data.data(); }
    operator entryId *() { return &m_sEntryId; }
    
    unsigned int 	size() const { return m_data.size(); }
	bool			empty() const { return m_data.empty(); } 
	
private:
    void updateStruct() {
        m_sEntryId.__size = m_data.size(); 
        m_sEntryId.__ptr = (unsigned char *)m_data.data();
    }
    
    entryId m_sEntryId;
    std::string m_data;
};

// this belongs to the DeleteObjects function
typedef struct {
	unsigned int ulId;
	unsigned int ulParent;
	bool fRoot;
	bool fInOGQueue;
	short ulObjType;
	short ulParentType;
	unsigned int ulFlags;
	unsigned int ulStoreId;
	unsigned int ulObjSize;
	unsigned int ulMessageFlags;
	SOURCEKEY sSourceKey;
	SOURCEKEY sParentSourceKey;
	entryId sEntryId; //fixme: auto destroy entryid
} DELETEITEM;

// Used to group notify flags and object type.
typedef struct _TCN {
	_TCN(unsigned int t, unsigned int f): ulFlags(f), ulType(t) {}
	bool operator<(const _TCN &rhs) const { return ulFlags < rhs.ulFlags || (ulFlags == rhs.ulFlags && ulType < rhs.ulType); }

	unsigned int ulFlags;
	unsigned int ulType;
} TABLECHANGENOTIFICATION;

class PARENTINFO {
public:
    PARENTINFO() : lItems(0), lFolders(0), lAssoc(0), lDeleted(0), lDeletedFolders(0), lDeletedAssoc(0), lUnread(0), ulStoreId(0) { }
    ~PARENTINFO() { }
	int lItems;
	int lFolders;
	int lAssoc;

	int lDeleted;
	int lDeletedFolders;
	int lDeletedAssoc;

	int lUnread;
	unsigned int ulStoreId;
};

#define EC_DELETE_FOLDERS		0x00000001
#define EC_DELETE_MESSAGES		0x00000002
#define EC_DELETE_RECIPIENTS	0x00000004
#define EC_DELETE_ATTACHMENTS	0x00000008
#define EC_DELETE_CONTAINER		0x00000010
#define EC_DELETE_HARD_DELETE	0x00000020
#define	EC_DELETE_STORE			0x00000040
#define EC_DELETE_NOT_ASSOCIATED_MSG	0x00000080

typedef std::list<DELETEITEM>	ECListDeleteItems;
typedef std::set<TABLECHANGENOTIFICATION>	ECSetTableChangeNotifications;
typedef std::map<unsigned int, ECSetTableChangeNotifications> ECMapTableChangeNotifications;

void FreeDeleteItem(DELETEITEM *src);
void FreeDeletedItems(ECListDeleteItems *lplstDeleteItems);

class ECAttachmentStorage;

ECRESULT GetSourceKey(unsigned int ulObjId, SOURCEKEY *lpSourceKey);

ECRESULT ValidateDeleteObject(ECSession *lpSession, bool bCheckPermission, unsigned int ulFlags, DELETEITEM &sItem);
ECRESULT ExpandDeletedItems(ECSession *lpSession, ECDatabase *lpDatabase, ECListInt *lpsObjectList, unsigned int ulFlags, bool bCheckPermission, ECListDeleteItems *lplstDeleteItems);
ECRESULT DeleteObjectUpdateICS(ECSession *lpSession, unsigned int ulFlags, ECListDeleteItems &lstDeleted, unsigned int ulSyncId);
ECRESULT DeleteObjectSoft(ECSession *lpSession, ECDatabase *lpDatabase, unsigned int ulFlags, ECListDeleteItems &lstDeleteItems, ECListDeleteItems &lstDeleted);
ECRESULT DeleteObjectHard(ECSession *lpSession, ECDatabase *lpDatabase, ECAttachmentStorage *lpAttachmentStorage, unsigned int ulFlags, ECListDeleteItems &lstDeleteItems, bool bNoTransaction, ECListDeleteItems &lstDeleted);
ECRESULT DeleteObjectStoreSize(ECSession *lpSession, ECDatabase *lpDatabase, unsigned int ulFlags, ECListDeleteItems &lstDeleted);
ECRESULT DeleteObjectCacheUpdate(ECSession *lpSession, unsigned int ulFlags, ECListDeleteItems &lstDeleted);
ECRESULT DeleteObjectNotifications(ECSession *lpSession, unsigned int ulFlags, ECListDeleteItems &lstDeleted);
ECRESULT DeleteObjects(ECSession *lpSession, ECDatabase *lpDatabase, ECListInt *lpsObjectList, unsigned int ulFlags, unsigned int ulSyncId, bool bNoTransaction, bool bCheckPermission);
ECRESULT DeleteObjects(ECSession *lpSession, ECDatabase *lpDatabase, unsigned int ulObjectId, unsigned int ulFlags, unsigned int ulSyncId, bool bNoTransaction, bool bCheckPermission);
ECRESULT MarkStoreAsDeleted(ECSession *lpSession, ECDatabase *lpDatabase, unsigned int ulStoreHierarchyId, unsigned int ulSyncId);

ECRESULT WriteLocalCommitTimeMax(struct soap *soap, ECDatabase *lpDatabase, unsigned int ulFolderId, propVal **ppvTime);

ECRESULT UpdateTProp(ECDatabase *lpDatabase, unsigned int ulPropTag, unsigned int ulFolderId, ECListInt *lpObjectIDs);
ECRESULT UpdateTProp(ECDatabase *lpDatabase, unsigned int ulPropTag, unsigned int ulFolderId, unsigned int ulObjId);
ECRESULT UpdateFolderCount(ECDatabase *lpDatabase, unsigned int ulFolderId, unsigned int ulPropTag, int lDelta);

ECRESULT CheckQuota(ECSession *lpecSession, ULONG ulStoreId);
ECRESULT MapEntryIdToObjectId(ECSession *lpecSession, ECDatabase *lpDatabase, ULONG ulObjId, const entryId &sEntryId);
ECRESULT UpdateFolderCounts(ECDatabase *lpDatabase, ULONG ulParentId, ULONG ulFlags, propValArray *lpModProps);
ECRESULT ProcessSubmitFlag(ECDatabase *lpDatabase, ULONG ulSyncId, ULONG ulStoreId, ULONG ulObjId, bool bNewItem, propValArray *lpModProps);
ECRESULT CreateNotifications(ULONG ulObjId, ULONG ulObjType, ULONG ulParentId, ULONG ulGrandParentId, bool bNewItem, propValArray *lpModProps, struct propVal *lpvCommitTime);

ECRESULT WriteSingleProp(ECDatabase *lpDatabase, unsigned int ulObjId, unsigned int ulFolderId, struct propVal *lpPropVal, bool bColumnProp, unsigned int ulMaxQuerySize, std::string &strInsertQuery);
ECRESULT WriteProp(ECDatabase *lpDatabase, unsigned int ulObjId, unsigned int ulParentId, struct propVal *lpPropVal);

ECRESULT GetNamesFromIDs(struct soap *soap, ECDatabase *lpDatabase, struct propTagArray *lpPropTags, struct namedPropArray *lpsNames);
ECRESULT ResetFolderCount(ECSession *lpSession, unsigned int ulObjId, unsigned int *lpulUpdates = NULL);

ECRESULT GetStoreType(ECSession *lpSession, unsigned int ulObjId, unsigned int *lpulStoreType);

ECRESULT RemoveStaleIndexedProp(ECDatabase *lpDatabase, unsigned int ulPropTag, unsigned char *lpData, unsigned int cbSize);

ECRESULT ApplyFolderCounts(ECDatabase *lpDatabase, unsigned int ulFolderId, const PARENTINFO &pi);
ECRESULT ApplyFolderCounts(ECDatabase *lpDatabase, const std::map<unsigned int, PARENTINFO> &mapFolderCounts);

#define LOCK_SHARED 	0x00000001
#define LOCK_EXCLUSIVE	0x00000002

// Lock folders and start transaction: 
ECRESULT BeginLockFolders(ECDatabase *lpDatabase, const std::set<SOURCEKEY>& setObjects, unsigned int ulFlags); // may be mixed list of folders and messages
ECRESULT BeginLockFolders(ECDatabase *lpDatabase, const std::set<EntryId>& setObjects, unsigned int ulFlags);	// may be mixed list of folders and messages
ECRESULT BeginLockFolders(ECDatabase *lpDatabase, const EntryId &entryid, unsigned int ulFlags);				// single entryid, folder or message
ECRESULT BeginLockFolders(ECDatabase *lpDatabase, const SOURCEKEY &sourcekey, unsigned int ulFlags);			// single sourcekey, folder or message

#endif//ZARAFACMD_UTIL_H

