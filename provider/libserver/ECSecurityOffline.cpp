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

#include "ECDefs.h"
#include "ECSecurityOffline.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static const char THIS_FILE[] = __FILE__;
#endif

ECSecurityOffline::ECSecurityOffline(ECSession *lpSession, ECConfig *lpConfig, ECLogger *lpLogger) : ECSecurity(lpSession, lpConfig, lpLogger, NULL)
{
}

ECSecurityOffline::~ECSecurityOffline(void)
{
}

int ECSecurityOffline::GetAdminLevel()
{
	return 2;	// System admin. Highest admin level in multi-tenan environment, which is the case for offline servers.
}

ECRESULT ECSecurityOffline::IsAdminOverUserObject(unsigned int ulUserObjectId)
{
	return erSuccess;
}

ECRESULT ECSecurityOffline::IsAdminOverOwnerOfObject(unsigned int ulObjectId)
{
	return erSuccess;
}

ECRESULT ECSecurityOffline::IsUserObjectVisible(unsigned int ulUserObjectId)
{
	return erSuccess;
}

ECRESULT ECSecurityOffline::GetViewableCompanyIds(std::list<localobjectdetails_t> **lppObjects, unsigned int ulFlags)
{
	/*
	 * When using offline we return all companies within the offline database, this is correct behavior
	 * since the user will only have companies visible to him inside his database. Remember, during
	 * the sync getCompany() is used on the server and that will return MAPI_E_NOT_FOUND when the
	 * was not visible.
	 * When the rights change, the server will put that update into the sync list and the sync
	 * will try again to create the object in the offline database or delete it.
	 */
	return m_lpSession->GetUserManagement()->GetCompanyObjectListAndSync(CONTAINER_COMPANY, 0, lppObjects, ulFlags);
}

ECRESULT ECSecurityOffline::GetUserQuota(unsigned int ulUserId, quotadetails_t *lpDetails)
{
	ECRESULT er = erSuccess;

	if (!lpDetails) {
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	lpDetails->bIsUserDefaultQuota = false;
	lpDetails->bUseDefaultQuota = false;
	lpDetails->llHardSize = 0;
	lpDetails->llSoftSize = 0;
	lpDetails->llWarnSize = 0;

exit:
	return er;
}
