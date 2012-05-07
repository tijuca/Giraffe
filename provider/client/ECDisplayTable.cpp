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

#include "platform.h"
#include "ECDisplayTable.h"

#include "ECGetText.h"

#include "CommonUtil.h"

#include "Mem.h"
#include "ECMemTable.h"


#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

// This function is NOT used, but is used as dummy for the xgettext parser so it
// can find the translation strings. **DO NOT REMOVE THIS FUNCTION**
void dummy() {
	LPCTSTR a;

	/* User General tab */
	a = _("General");
	a = _("Name");
	a = _("First:");
	a = _("Initials:");
	a = _("Last:");
	a = _("Display:");
	a = _("Alias:");
	a = _("Address:");
	a = _("City:");
	a = _("State:");
	a = _("Zip code:");
	a = _("Country/Region:");
	a = _("Title:");
	a = _("Company:");
	a = _("Department:");
	a = _("Office:");
	a = _("Assistant:");
	a = _("Phone:");

	/* User Phone/Notes tab */
	a = _("Phone/Notes");
	a = _("Phone numbers");
	a = _("Business:");
	a = _("Business 2:");
	a = _("Fax:");
	a = _("Assistant:");
	a = _("Home:");
	a = _("Home 2:");
	a = _("Mobile:");
	a = _("Pager:");
	a = _("Notes:");

	/* User Organization tab */
	a = _("Organization");
	a = _("Manager:");
	a = _("Direct reports:");

	/* User Member Of tab */
	a = _("Member Of");
	a = _("Group membership:");

	/* User E-mail Addresses tab */
	a = _("E-mail Addresses");
	a = _("E-mail addresses:");

	/* Distlist General tab */
	a = _("General");
	a = _("Display name:");
	a = _("Alias name:");
	a = _("Owner:");
	a = _("Notes:");
	a = _("Members");
	a = _("Modify members...");

	/* Distlist Member Of tab */
	a = _("Member Of");
	a = _("Group membership:");
}

