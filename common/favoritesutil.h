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

#ifndef FAVORITESUTIL_H
#define FAVORITESUTIL_H

#include <mapix.h>

#define FAVO_FOLDER_LEVEL_BASE		0x00000
#define FAVO_FOLDER_LEVEL_ONE		0x00001
#define FAVO_FOLDER_LEVEL_SUB		0x00002

#define FAVO_FOLDER_INHERIT_AUTO	0x10000 //unused

// Default column set for shortcut folder (favorites)
enum {
	SC_INSTANCE_KEY,
	SC_FAV_PUBLIC_SOURCE_KEY,
	SC_FAV_PARENT_SOURCE_KEY,
	SC_FAV_DISPLAY_NAME, 
	SC_FAV_DISPLAY_ALIAS,
	SC_FAV_LEVEL_MASK,
	SC_FAV_CONTAINER_CLASS,
	SHORTCUT_NUM
};

LPSPropTagArray GetShortCutTagArray();

HRESULT AddToFavorite(IMAPIFolder *lpShortcutFolder, ULONG ulLevel, LPCTSTR lpszAliasName, ULONG ulFlags, ULONG cValues, LPSPropValue lpPropArray);
HRESULT GetShortcutFolder(LPMAPISESSION lpSession, LPTSTR lpszFolderName, LPTSTR lpszFolderComment, ULONG ulFlags, LPMAPIFOLDER* lppShortcutFolder);
HRESULT CreateShortcutFolder(IMsgStore *lpMsgStore, LPTSTR lpszFolderName, LPTSTR lpszFolderComment, ULONG ulFlags, LPMAPIFOLDER* lppShortcutFolder);
HRESULT GetFavorite(IMAPIFolder *lpShortcutFolder, ULONG ulFlags, IMAPIFolder *lpMapiFolder, ULONG *lpcValues, LPSPropValue *lppShortCutPropValues);

HRESULT DelFavoriteFolder(IMAPIFolder *lpShortcutFolder, LPSPropValue lpPropSourceKey);
HRESULT AddFavoriteFolder(IMAPIFolder *lpShortcutFolder, LPMAPIFOLDER lpFolder, LPCTSTR lpszAliasName, ULONG ulFlags);

#endif //#ifndef FAVORITESUTIL_H
