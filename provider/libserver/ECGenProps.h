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

#ifndef ECGENPROPS_H
#define ECGENPROPS_H

#include "ZarafaCode.h"

#include <string>
#include <map>

#include "ECSession.h"
/*
 * This class is a general serverside class for generated properties. A Generated
 * property is any property that cannot be directly stored or read from the database.
 * This means properties like PR_ENTRYID because they are not stored as a property, 
 * properties like PR_NORMALIZED_SUBJECT because they are computed from the PR_SUBJECT,
 * and PR_MAPPING_SIGNATURE because they are stored elsewhere within the database.
 *
 * The client can also generate some properties, but only if these are fixed properties
 * that don't have any storage on the server, and they are also properties that are
 * never sorted on in tables. (due to the server actually doing the sorting)
 */

class ECGenProps {
public:
	ECGenProps();
	~ECGenProps();

	// Returns whether a different property should be retrieved instead of the
	// requested property.
	static ECRESULT	GetPropSubstitute(unsigned int ulObjType, unsigned int ulPropTagRequested, unsigned int *lpulPropTagRequired);

	// Return erSuccess if a property can be generated in GetPropComputed()
	static ECRESULT IsPropComputed(unsigned int ulPropTag, unsigned int ulObjType);
	// Return erSuccess if a property can be generated in GetPropComputedUncached()
	static ECRESULT IsPropComputedUncached(unsigned int ulPropTag, unsigned int ulObjType);
	// Return erSuccess if a property needn't be saved in the properties table
	static ECRESULT IsPropRedundant(unsigned int ulPropTag, unsigned int ulObjType);

	// Returns a subquery to run for the specified column
	static ECRESULT GetPropSubquery(unsigned int ulPropTagRequested, std::string &subquery);

	// Does post-processing after retrieving data from either cache or DB
	static ECRESULT	GetPropComputed(struct soap *soap, unsigned int ulObjType, unsigned int ulPropTagRequested, unsigned int ulObjId, struct propVal *lpsPropVal);

	// returns the computed value for a property which doesn't has database actions
	static ECRESULT GetPropComputedUncached(struct soap *soap, ECSession* lpSession, unsigned int ulPropTag, unsigned int ulObjId, unsigned int ulOrderId, unsigned int ulStoreId, unsigned int ulParentId, unsigned int ulObjType, struct propVal *lpPropVal);

	static ECRESULT GetMVPropSubquery(unsigned int ulPropTagRequested, std::string &subquery);
	static ECRESULT GetStoreName(struct soap *soap, ECSession* lpSession, unsigned int ulStoreId, unsigned int ulStoreType, char** lppStoreName);
	static ECRESULT IsOrphanStore(ECSession* lpSession, unsigned int ulObjId, bool *lpbIsOrphan);
private:

};

#endif // ECGENPROPS_H

