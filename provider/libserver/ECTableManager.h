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

#ifndef TABLEMANAGER_H
#define TABLEMANAGER_H

#include <map>

#include "ECDatabase.h"
#include "ECGenericObjectTable.h"
#include "ZarafaCode.h"


class ECSession;
class ECSessionManager;

/* 
 * The table manager is responsible for opening tables, and providing
 * access to open tables by a handle ID for each table, and updating tables when
 * a change is made to the underlying data and sending notifications to clients
 * which require table update notifications.
 *
 * Each session has its own table manager. This could be optimised in the future
 * by having one large table manager, with each table having multiple cursors for
 * each user that has the table open. This would save memory on overlapping tables
 * (probably resulting in around 30% less memory usage for the server).
 */

typedef struct {
	typedef enum { TABLE_TYPE_GENERIC, TABLE_TYPE_OUTGOINGQUEUE, TABLE_TYPE_MULTISTORE, TABLE_TYPE_USERSTORES,
		   TABLE_TYPE_SYSTEMSTATS, TABLE_TYPE_THREADSTATS, TABLE_TYPE_USERSTATS, TABLE_TYPE_SESSIONSTATS, TABLE_TYPE_COMPANYSTATS,
			TABLE_TYPE_MAILBOX} TABLE_TYPE;
		   
    TABLE_TYPE ulTableType;
    
	union __sTable {
		struct __sGeneric {
			unsigned int ulParentId;
			unsigned int ulObjectType;
			unsigned int ulObjectFlags;
		} sGeneric ;
		struct __sOutgoingQueue {
			unsigned int ulStoreId;
			unsigned int ulFlags;
		} sOutgoingQueue;
	} sTable;
	ECGenericObjectTable *lpTable; // Actual table object
	unsigned int ulSubscriptionId; // Subscription ID for table event subscription on session manager
} TABLE_ENTRY;

typedef std::map<unsigned int, TABLE_ENTRY *> TABLEENTRYMAP;

class ECTableManager {
public:
	ECTableManager(ECSession *lpSession);
	~ECTableManager();

	ECRESULT	OpenGenericTable(unsigned int ulParent, unsigned int ulObjType, unsigned int ulFlags, unsigned int *lpulTableId, bool fLoad = true);
	ECRESULT	OpenOutgoingQueueTable(unsigned int ulStoreId, unsigned int *lpulTableId);
	ECRESULT	OpenABTable(unsigned int ulParent, unsigned int ulParentType, unsigned int ulObjType, unsigned int ulFlags, unsigned int *lpulTableId);
	ECRESULT	OpenMultiStoreTable(unsigned int ulObjType, unsigned int ulFlags, unsigned int *lpulTableId);
	ECRESULT	OpenUserStoresTable(unsigned int ulFlags, unsigned int *lpulTableId);
	ECRESULT	OpenStatsTable(unsigned int ulTableType, unsigned int ulFlags, unsigned int *lpulTableId);
	ECRESULT	OpenMailBoxTable(unsigned int ulflags, unsigned int *lpulTableId);

	ECRESULT	GetTable(unsigned int lpulTableId, ECGenericObjectTable **lppTable);
	ECRESULT	CloseTable(unsigned int lpulTableId);

	ECRESULT	UpdateOutgoingTables(ECKeyTable::UpdateType ulType, unsigned int ulStoreId, unsigned int ulObjId, unsigned int ulFlags, unsigned int ulObjType);
	ECRESULT	UpdateTables(ECKeyTable::UpdateType ulType, unsigned int ulFlags, unsigned int ulObjId, unsigned int ulChildId, unsigned int ulObjType);

	ECRESULT	GetStats(unsigned int *lpulTables, unsigned int *lpulObjectSize);

private:
	static	void *	SearchThread(void *lpParam);

	void		AddTableEntry(TABLE_ENTRY *lpEntry, unsigned int *lpulTableId);

	ECSession								*lpSession;
	TABLEENTRYMAP							mapTable;
	unsigned int							ulNextTableId;
	pthread_mutex_t							hListMutex;
};

#endif // TABLEMANAGER_H

