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

#include "platform.h"
#include "ECACL.h"
#include "CommonUtil.h"
#include "stringutil.h"

#include <sstream>
#include <algorithm>

// The data in this array must be sorted on the ulRights field.
struct AclRightName {
	unsigned ulRight;
	const char *szRight;
};

static const AclRightName g_rights[] = {
	{RIGHTS_READ_ITEMS, "item read"},
	{RIGHTS_CREATE_ITEMS, "item create"},
	{RIGHTS_EDIT_OWN, "edit own"},
	{RIGHTS_DELETE_OWN, "delete own"},
	{RIGHTS_EDIT_ALL, "edit all"},
	{RIGHTS_DELETE_ALL, "delete all"},
	{RIGHTS_CREATE_SUBFOLDERS, "create sub"},
	{RIGHTS_FOLDER_OWNER, "own"},
	{RIGHTS_FOLDER_CONTACT, "contact"},
	{RIGHTS_FOLDER_VISIBLE, "view"}
};

// The data in this array must be sorted on the ulRights field.
struct AclRoleName {
	unsigned ulRights;
	const char *szRole;
};

static const AclRoleName g_roles[] = {
	{RIGHTS_NONE, "none"},	// Actually a right, but not seen as such by IsRight
	{ROLE_NONE, "none"},	// This might be confusing
	{ROLE_REVIEWER, "reviewer"},
	{ROLE_CONTRIBUTOR, "contributer"},
	{ROLE_NONEDITING_AUTHOR, "non-editting author"},
	{ROLE_AUTHOR, "author"},
	{ROLE_EDITOR, "editor"},
	{ROLE_PUBLISH_EDITOR, "publish editor"},
	{ROLE_PUBLISH_AUTHOR, "publish author"},
	{ROLE_OWNER, "owner"}
};

static inline bool IsRight(unsigned ulRights) {
	// A right has exactly 1 bit set. Otherwise it's a role
	return (ulRights ^ (ulRights - 1)) == 0;
}

static inline bool operator<(const AclRightName &lhs, const AclRightName &rhs) {
	return lhs.ulRight < rhs.ulRight;
}

static inline bool operator<(const AclRoleName &lhs, const AclRoleName &rhs) {
	return lhs.ulRights < rhs.ulRights;
}

static const AclRightName *FindAclRight(unsigned ulRights) {
	const AclRightName rn = {ulRights, NULL};

	const AclRightName *lpRightName = std::lower_bound(g_rights, arrayEnd(g_rights), rn);
	if (lpRightName != arrayEnd(g_rights) && lpRightName->ulRight == ulRights)
		return lpRightName;

	return NULL;
}

static const AclRoleName *FindAclRole(unsigned ulRights) {
	const AclRoleName rn = {ulRights, NULL};

	const AclRoleName *lpRoleName = std::lower_bound(g_roles, arrayEnd(g_roles), rn);
	if (lpRoleName != arrayEnd(g_roles) && lpRoleName->ulRights == ulRights)
		return lpRoleName;

	return NULL;
}

std::string AclRightsToString(unsigned ulRights)
{
	if (ulRights == unsigned(-1))
		return "missing or invalid";
	
	if (IsRight(ulRights)) {
		const AclRightName *lpRightName = FindAclRight(ulRights);
		if (lpRightName == NULL)
			return stringify(ulRights, true);
		return lpRightName->szRight;
	}

	const AclRoleName *lpRoleName = FindAclRole(ulRights);
	if (lpRoleName != NULL)
		return lpRoleName->szRole;

	std::ostringstream ostr;
	bool empty = true;
	for (unsigned bit = 0, mask = 1; bit < 32; ++bit, mask <<= 1) {
		if (ulRights & mask) {
			if (!empty)
				ostr << ",";
			empty = false;
			ostr << AclRightsToString(mask);
		}
	}
	return ostr.str();
}
