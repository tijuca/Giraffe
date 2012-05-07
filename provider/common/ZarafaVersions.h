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

#ifndef ZARAFAVERSIONS_INCLUDED
#define ZARAFAVERSIONS_INCLUDED

#include "ecversion.h"

#define MAKE_ZARAFA_VERSION(major, minor, update) \
	((((major) & 0xff) << 24) | (((minor) & 0xff) << 16) | ((update) & 0xffff))

#define MAKE_ZARAFA_MAJOR(major, minor) \
	MAKE_ZARAFA_VERSION((major), (minor), 0)

#define MAKE_ZARAFA_GENERAL(major) \
	MAKE_ZARAFA_MAJOR((major), 0)

#define ZARAFA_MAJOR_MASK	0xffff0000
#define ZARAFA_GENERAL_MASK	0xff000000

#define ZARAFA_GET_MAJOR(version)	\
	((version) & ZARAFA_MAJOR_MASK)

#define ZARAFA_GET_GENERAL(version)	\
	((version) & ZARAFA_GENERAL_MASK)


// Current thing
#define ZARAFA_CUR_MAJOR		MAKE_ZARAFA_MAJOR(PROJECT_VERSION_MAJOR, PROJECT_VERSION_MINOR)
#define ZARAFA_CUR_GENERAL		MAKE_ZARAFA_GENERAL(PROJECT_VERSION_MAJOR)

// Important version(s) we check for
#define ZARAFA_VERSION_6_40_0	MAKE_ZARAFA_VERSION(6, 40, 0)
#define ZARAFA_VERSION_UNKNOWN	MAKE_ZARAFA_VERSION(0xff, 0xff, 0xffff)


#define ZARAFA_COMPARE_VERSION_TO_MAJOR(version, major)	\
	((version) < (major) ? -1 : (ZARAFA_GET_MAJOR(version) > (major) ? 1 : 0))

#define ZARAFA_COMPARE_VERSION_TO_GENERAL(version, general) \
	((version) < (general) ? -1 : (ZARAFA_GET_GENERAL(version) > (general) ? 1 : 0))


#endif // ndef ZARAFAVERSIONS_INCLUDED
