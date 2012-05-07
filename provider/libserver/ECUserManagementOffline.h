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

#include "ECUserManagement.h"

class ECUserManagementOffline :	public ECUserManagement
{
public:
	ECUserManagementOffline(ECSession *lpSession, ECPluginFactory *lpPluginFactory, ECConfig *lpConfig, ECLogger *lpLogger);
	virtual ~ECUserManagementOffline(void);


	//virtual ECRESULT	AuthUserAndSync(char *szUsername, char *szPassword, unsigned int *lpulUserId);
	//virtual ECRESULT	GetUserDetailsAndSync(unsigned int ulUserId, userdetails_t *lpDetails);
	virtual ECRESULT	GetUserQuotaDetailsAndSync(unsigned int ulUserId, quotadetails_t *lpDetails);
	/*
	virtual ECRESULT	SetUserQuotaDetailsAndSync(unsigned int ulUserId, quotadetails_t sDetails);
	virtual ECRESULT	GetGroupDetailsAndSync(unsigned int ulGroupId, groupdetails_t *lpDetails);
	virtual ECRESULT	GetUserListAndSync(std::list<localuserdetails> **lppUsers, unsigned int ulFlags = 0);
	virtual ECRESULT	GetGroupListAndSync(std::list<localgroupdetails> **lppGroups, unsigned int ulFlags = 0);
	virtual ECRESULT	GetUserGroupListAndSync(std::list<localuserobjectdetails> **lppUserGroups, unsigned int ulFlags = 0);
	virtual ECRESULT	GetMembersOfGroupAndSync(unsigned int ulGroupId, std::list<localuserdetails> **lppUsers, unsigned int ulFlags = 0);
	virtual ECRESULT	GetGroupMembershipAndSync(unsigned int ulUserId, std::list<localgroupdetails> **lppGroups, unsigned int ulFlags = 0);
	virtual ECRESULT	SetUserDetailsAndSync(unsigned int ulUserId, userdetails_t sDetails, int update);
	virtual ECRESULT	SetGroupDetailsAndSync(unsigned int ulGroupId, groupdetails_t sDetails, int update);
	virtual ECRESULT	AddMemberToGroupAndSync(unsigned int ulGroupId, unsigned int ulUserId);
	virtual ECRESULT	DeleteMemberFromGroupAndSync(unsigned int ulGroupId, unsigned int ulUserId);


	virtual ECRESULT	ResolveUserAndSync(char *szUsername, unsigned int *lpulUserId, bool *lpbIsNonActive = NULL);
	virtual ECRESULT	ResolveGroupAndSync(char *szGroupname, unsigned int *lpulGroupId);
	virtual ECRESULT	SearchPartialUserAndSync(char *szSearchString, unsigned int *lpulId);
	virtual ECRESULT	SearchPartialGroupAndSync(char *szSearchString, unsigned int *lpulId);

	// Create a user
	virtual ECRESULT	CreateUserAndSync(userdetails_t details, unsigned int *ulId);
	// Delete a user
	virtual ECRESULT	DeleteUserAndSync(unsigned int ulId);
	// Create a group
	virtual ECRESULT	CreateGroupAndSync(groupdetails_t details, unsigned int *ulId);
	// Delete a group
	virtual ECRESULT	DeleteGroupAndSync(unsigned int ulId);

	// Get MAPI property data for a group or user/group id, with on-the-fly delete of the specified user/group
	virtual ECRESULT	GetProps(struct soap *soap, unsigned int ulId, struct propTagArray *lpPropTagArray, struct propValArray *lppPropValArray);
	*/

	/* override QueryRowData() and GetUserList() ? */

	/*
	// AddressBook functions -- override?
	//virtual ECRESULT GetUserIDList(unsigned int ulParentID, unsigned int ulObjType, unsigned int** lppulUserList, unsigned int* lpulSize);
	//virtual ECRESULT ResolveUser(struct soap *soap, struct propValArray* lpPropValArraySearch, struct propTagArray* lpsPropTagArray, struct propValArray* lpPropValArrayDst, unsigned int* lpulFlag);
	//virtual ECRESULT ResolveGroup(struct soap *soap, struct propValArray* lpPropValArraySearch, struct propTagArray* lpsPropTagArray, struct propValArray* lpPropValArrayDst, unsigned int* lpulFlag);
	*/
};
