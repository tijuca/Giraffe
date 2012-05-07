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

#ifndef ECShortTermEntryIDManager_INCLUDED
#define ECShortTermEntryIDManager_INCLUDED

#include "ZarafaCode.h"
#include "soapStub.h"

#include <map>
#include <pthread.h>

struct xsd_base64Binary;
class ECCacheManager;

/**
 * This class is responsible for creating short term entry ids and manages the mapping
 * of those entry ids to actual objects.
 * The format of a short term entryid is the same as the real entryid for an object but
 * with the uniqueid replaced with another id, and the first byte of the abFlags set to 0xff.
 * 
 * This format is choosen to ensure backward compatibilty. Some operations will be performed
 * on the entryid client side, which break when a different format is used.
 * 
 * The alteration of the uniqueid is done to make sure short term entryids can't be faked by
 * just setting the first byte of the abFlags to 0xff.
 * 
 * Currently the only short term entryids in use are for stores that are opened on another server
 * than the home server of the user to which they belong. But in theory they can be used for other
 * objects as well if there's a need for that.
 */
class ECShortTermEntryIDManager
{
public:
	ECShortTermEntryIDManager();
	~ECShortTermEntryIDManager();

	ECRESULT GetEntryIdForObjectId(unsigned int ulObjId, ECCacheManager *lpCacheManager, entryId *lpEntryId);
	ECRESULT GetObjectIdFromEntryId(const entryId *lpEntryId, unsigned int *lpulObjId);

	static ECRESULT IsShortTermEntryId(const entryId *lpEntryId, bool *lpbResult);

private:
	/**
	 * This struct is used as the less template parameter for the map that is keyed
	 * with entryId structures.
	 */
	struct compareLessEid {
		bool operator()(const entryId &lhs, const entryId &rhs) const;
	};

	typedef std::map<unsigned int, entryId>	IdToEidMap;
	typedef std::map<entryId, unsigned int, ECShortTermEntryIDManager::compareLessEid> EidToIdMap;

	static void freeEid(IdToEidMap::value_type &eid);

private:
	IdToEidMap	m_mapIdToEid;
	EidToIdMap	m_mapEidToId;

	pthread_mutex_t	m_hMutex;
};

#endif // ndef ECShortTermEntryIDManager_INCLUDED
