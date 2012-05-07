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

#ifndef ECCHANGEDATA_H
#define ECCHANGEDATA_H

#include <ECUnknown.h>

#include "zarafa-indexer.h"

/**
 * structure representing a synchronization change.
 */
struct sourceid_t {
	/**
	 * Constructor
	 */
	sourceid_t() : ulProps(0), lpProps(NULL), lpStream(NULL) {}

	/**
	 * Number of elements in lpProps array
	 */
	ULONG ulProps;

	/**
	 * Array of SPropValue, the first property
	 * will act as index in lists of sourceid_t
	 */
	LPSPropValue lpProps;

	/**
	 * Stream containing message
	 */
	IStream *lpStream;
};

typedef std::list<sourceid_t *> sourceid_list_t;

/**
 * Collection of synchronization changes
 * This class owns 2 lists of synchronization changes,
 * the first list is for all creation changes while the
 * second list is for all deletions. Sorting and deleting
 * duplicate entries should be done using the Sort() and
 * Unique() functions on this class rather then on the
 * lists itself.
 */
class ECChanges {
public:
	/**
	 * Constructor
	 */
	ECChanges();

	/**
	 * Destructor
	 */
	~ECChanges();

    /**
	 * Sort both lists with created and deleted items.
	 *
	 * This will call std::list::sort() on both lists.
	 */
	VOID Sort();

	/**
	 * Remove all duplicate entries in both lists with created and deleted items.
	 *
	 * This will call std::list::unique() on both lists.
	 */
	VOID Unique();

	/**
	 * This will filter out all items in the create list which are also listed
	 * in the delete list
	 */
	VOID Filter();

	/**
	 * List of creation synchronization changes.
	 */
	sourceid_list_t lCreate;

	/**
	 * List of change synchronization changes.
	 */
	sourceid_list_t lChange;

	/**
	 * List of deletion synchronization changes.
	 */
	sourceid_list_t lDelete;
};

/**
 * Collector of synchronization changes.
 * The ECSynchronization class will collect all changes
 * in the ECChangeData class. This class will keep track of
 * all detected changes. To handle batches of changes use ClaimChanges().
 */
class ECChangeData : public ECUnknown {
private:
	/**
	 * Constructor
	 *
	 * @note Objects of ECChangeData must only be created using the Create() function.
	 */
	ECChangeData();

public:
	/**
	 * Create new ECChangeData object
	 *
	 * @note Creating a new ECChangeData object must always occur through this function.
	 *
	 * @param[out]	lppChanges
	 *					The created ECChangeData object.
	 */
	static HRESULT Create(ECChangeData **lppChanges);

	/**
	 * Destructor
	 */
	~ECChangeData();

	/**
	 * Check the total size of the created and deleted items lists.
	 *
	 * The returned value equals the current size of both lists,
	 * but also includes the number of changes which were already
	 * collected with ClaimChanges().
	 *
	 * @param[out]	lpCreate
	 *					Number of create changes
	 * @param[out]	lpChange
	 *					Number of modify changes
	 * @param[out]	lpDelete
	 *					Number of delete changes
	 */
	VOID Size(ULONG *lpCreate, ULONG *lpChange, ULONG *lpDelete);

	/**
	 * Check the size of the created and deleted items lists.
	 *
	 * @param[out]	lpCreate
	 *					Number of create changes
	 * @param[out]	lpChange
	 *					Number of modify changes
	 * @param[out]	lpDelete
	 *					Number of delete changes
	 */
	VOID CurrentSize(ULONG *lpCreate, ULONG *lpChange, ULONG *lpDelete);

	/**
	 * Add new change to be created
	 *
	 * @param[in]	sEntry
	 *					Reference to the change to create
	 */
	VOID InsertCreate(sourceid_list_t::value_type &sEntry);

	/**
	 * Add new change to be modified
	 *
	 * @param[in]	sEntry
	 *					Reference to the change to modify
	 */
	VOID InsertChange(sourceid_list_t::value_type &sEntry);

	/**
	 * Add new change to be deleted
	 *
	 * @param[in]	sEntry
	 *					Reference to the change to deleted
	 */
	VOID InsertDelete(sourceid_list_t::value_type &sEntry);

	/**
	 * Move all changes from object into given lists
	 *
	 * @param[out]	lpChanges
	 *					Reference to where the changes should be moved
	 * @return HRESULT
	 */
	HRESULT ClaimChanges(ECChanges *lpChanges);

private:
	pthread_mutex_t m_hThreadLock;
	ECChanges m_sChanges;
	ULONG m_ulCreate;
	ULONG m_ulChange;
	ULONG m_ulDelete;
	ULONG m_ulTotalCreate;
	ULONG m_ulTotalChange;
	ULONG m_ulTotalDelete;
};

#endif /* ECCHANGEDATA_H */
