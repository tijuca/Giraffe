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
#include "ECDatabase.h"

#include <mapidefs.h>
#include "edkmdb.h"

#include "ECSecurity.h"
#include "ECSessionManager.h"
#include "ECGenProps.h"
#include "ECSession.h"
#include "stringutil.h"

#include "ECMailBoxTable.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

ECMailBoxTable::ECMailBoxTable(ECSession *lpSession, unsigned int ulFlags, const ECLocale &locale) : 
	ECStoreObjectTable(lpSession, 0, NULL, 0, MAPI_STORE, ulFlags, locale)
{
	m_ulStoreTypes = 3; // 1. Show all users store 2. Public stores
}

ECMailBoxTable::~ECMailBoxTable(void)
{
}

ECRESULT ECMailBoxTable::Create(ECSession *lpSession, unsigned int ulFlags, const ECLocale &locale, ECMailBoxTable **lppTable)
{
	*lppTable = new ECMailBoxTable(lpSession, ulFlags, locale);

	(*lppTable)->AddRef();

	return erSuccess;
}

ECRESULT ECMailBoxTable::Load()
{
	ECRESULT er = erSuccess;
	ECDatabase *lpDatabase = NULL;
	DB_RESULT 	lpDBResult = NULL;
	DB_ROW		lpDBRow = NULL;
	std::string strQuery;
	std::list<unsigned int> lstObjIds;

	er = lpSession->GetDatabase(&lpDatabase);
	if (er != erSuccess)
		goto exit;

	Clear();

	//@todo Load all stores depends on m_ulStoreTypes, 1. privates, 2. publics or both
	strQuery = "SELECT hierarchy_id FROM stores";
	er = lpDatabase->DoSelect(strQuery, &lpDBResult);
	if(er != erSuccess)
		goto exit;

	while(1) {
		lpDBRow = lpDatabase->FetchRow(lpDBResult);

		if(lpDBRow == NULL)
			break;

		if (!lpDBRow[0])
			continue; // Broken store table?

		lstObjIds.push_back(atoui(lpDBRow[0]));
	}

	LoadRows(&lstObjIds, 0);

exit:
	if (lpDBResult) {
		lpDatabase->FreeResult(lpDBResult);
		lpDBResult = NULL;
	}
	return er;
}
