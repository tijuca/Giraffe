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

#include "ECSecurity.h"
#include "ECSessionManagerOffline.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

ECSessionManagerOffline::ECSessionManagerOffline(ECConfig *lpConfig, ECLogger *lpLogger, bool bHostedZarafa, bool bDistributedZarafa) : ECSessionManager(lpConfig, lpLogger, NULL, bHostedZarafa, bDistributedZarafa)
{
}

ECSessionManagerOffline::~ECSessionManagerOffline(void)
{
}

ECRESULT ECSessionManagerOffline::CreateAuthSession(struct soap *soap, unsigned int ulCapabilities, ECSESSIONID *sessionID, ECAuthSession **lppAuthSession, bool bRegisterSession, bool bLockSession)
{
	ECRESULT er = erSuccess;
	ECAuthSession *lpAuthSession = NULL;
	ECSESSIONID newSessionID;

	newSessionID = rand_mt();

	lpAuthSession = new ECAuthSessionOffline(soap->ip, newSessionID, m_lpDatabaseFactory, this, ulCapabilities);
	if (lpAuthSession) {
	    if (bLockSession) {
	        lpAuthSession->Lock();
	    }
		if (bRegisterSession) {
			pthread_rwlock_wrlock(&m_hCacheRWLock);
			m_mapSessions.insert( SESSIONMAP::value_type(newSessionID, lpAuthSession) );
			pthread_rwlock_unlock(&m_hCacheRWLock);
		}

		*sessionID = newSessionID;
		*lppAuthSession = lpAuthSession;
	} else
		er = ZARAFA_E_NOT_ENOUGH_MEMORY;

	return er;
}
