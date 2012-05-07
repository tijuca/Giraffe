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

#ifndef ECSUBRESTRICTION_H
#define ECSUBRESTRICTION_H

#include <set>
#include <vector>

#include "ECKeyTable.h"
#include "ECDatabase.h"
#include "soapH.h"

#include "ustringutil.h"

class ECSession;

// These are some helper function to help running subqueries.

/* 
 * How we run subqueries assumes the following:
 * - SubSubRestrictions are invalid
 * - The most efficient way to run a search with a subquery is to first evaluate the subquery and THEN the
 *   main query
 *
 * We number subqueries in a query in a depth-first order. The numbering can be used because we don't have
 * nested subqueries (see above). Each subquery can therefore be pre-calculated for each item in the main
 * query target. The results of the subqueries is then passed to the main query solver, which only needs
 * to check the outcome of a subquery.
 */
 
// A set containing all the objects that match a subquery. The row id here is for the parent object, not for
// the actual object that matched the restriction (ie the message id is in here, not the recipient id or
// attachment id)
typedef std::set<unsigned int> SUBRESTRICTIONRESULT;
// A list of sets of subquery matches
typedef std::vector<SUBRESTRICTIONRESULT *> SUBRESTRICTIONRESULTS;

ECRESULT GetSubRestrictionCount(struct restrictTable *lpRestrict, unsigned int *lpulCount);
ECRESULT GetSubRestriction(struct restrictTable *lpBase, unsigned int ulCount, struct restrictSub **lppSubRestrict);

// Get results for all subqueries for a set of objects
ECRESULT RunSubRestrictions(ECSession *lpSession, void *lpECODStore, struct restrictTable *lpRestrict, ECObjectTableList *lpObjects, const ECLocale &locale, SUBRESTRICTIONRESULTS **lppResults);
ECRESULT RunSubRestriction(ECSession *lpSession, void *lpECODStore, struct restrictSub *lpSubRestrict, ECObjectTableList *lpObjects, const ECLocale &locale, SUBRESTRICTIONRESULT **lppResult);
ECRESULT FreeSubRestrictionResults(SUBRESTRICTIONRESULTS *lpResults);

#define SUBRESTRICTION_MAXDEPTH	64

#endif
