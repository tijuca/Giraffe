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

#ifndef ZARAFAICS_H
#define ZARAFAICS_H

// Flags for ns__getChanges and ns__setSyncStatus
#define ICS_SYNC_CONTENTS	1
#define ICS_SYNC_HIERARCHY	2
#define ICS_SYNC_AB			3

#define ICS_MESSAGE			0x1000
#define ICS_FOLDER			0x2000
#define ICS_AB				0x4000

#define ICS_ACTION_MASK		0x000F

#define ICS_NEW				0x0001
#define ICS_CHANGE			0x0002
#define ICS_FLAG			0x0003
#define	ICS_SOFT_DELETE		0x0004
#define ICS_HARD_DELETE		0x0005
#define ICS_MOVED			0x0006 //not used

#define ICS_CHANGE_FLAG_NEW 		(1 << (ICS_NEW))
#define ICS_CHANGE_FLAG_CHANGE		(1 << (ICS_CHANGE))
#define ICS_CHANGE_FLAG_FLAG		(1 << (ICS_FLAG))
#define ICS_CHANGE_FLAG_SOFT_DELETE	(1 << (ICS_SOFT_DELETE))
#define ICS_CHANGE_FLAG_HARD_DELETE (1 << (ICS_HARD_DELETE))
#define ICS_CHANGE_FLAG_MOVED		(1 << (ICS_MOVED))

#define ICS_MESSAGE_CHANGE		(ICS_MESSAGE | ICS_CHANGE)
#define ICS_MESSAGE_FLAG		(ICS_MESSAGE | ICS_FLAG)
#define ICS_MESSAGE_SOFT_DELETE	(ICS_MESSAGE | ICS_SOFT_DELETE)
#define ICS_MESSAGE_HARD_DELETE	(ICS_MESSAGE | ICS_HARD_DELETE)
#define ICS_MESSAGE_NEW			(ICS_MESSAGE | ICS_NEW)

#define ICS_FOLDER_CHANGE		(ICS_FOLDER | ICS_CHANGE)
#define ICS_FOLDER_SOFT_DELETE	(ICS_FOLDER | ICS_SOFT_DELETE)
#define ICS_FOLDER_HARD_DELETE	(ICS_FOLDER | ICS_HARD_DELETE)
#define ICS_FOLDER_NEW			(ICS_FOLDER | ICS_NEW)

#define ICS_AB_CHANGE			(ICS_AB | ICS_CHANGE)
#define ICS_AB_NEW				(ICS_AB | ICS_NEW)
#define ICS_AB_DELETE			(ICS_AB | ICS_HARD_DELETE)

#define ICS_ACTION(x)			((x) & ICS_ACTION_MASK)

#endif

