/*
 * Copyright 2005 - 2015  Zarafa B.V. and its licensors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License, version 3,
 * as published by the Free Software Foundation.
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

#include <zarafa/platform.h>
#include <new>

#include "ECSecurity.h"
#include "ECSessionManagerOffline.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static const char THIS_FILE[] = __FILE__;
#endif

ECSessionManagerOffline::ECSessionManagerOffline(ECConfig *lpConfig,
    bool bHostedZarafa, bool bDistributedZarafa) :
	ECSessionManager(lpConfig, NULL, bHostedZarafa, bDistributedZarafa)
{
}

ECSessionManagerOffline::~ECSessionManagerOffline(void)
{
}

ECRESULT ECSessionManagerOffline::CreateAuthSession(struct soap *soap, unsigned int ulCapabilities, ECSESSIONID *sessionID, ECAuthSession **lppAuthSession, bool bRegisterSession, bool bLockSession)
{
	ECAuthSession *lpAuthSession = NULL;
	ECSESSIONID newSessionID;

	newSessionID = rand_mt();

	lpAuthSession = new(std::nothrow) ECAuthSessionOffline(GetSourceAddr(soap), newSessionID, m_lpDatabaseFactory, this, ulCapabilities);
	if (lpAuthSession == NULL)
		return ZARAFA_E_NOT_ENOUGH_MEMORY;
	if (bLockSession)
	        lpAuthSession->Lock();
	if (bRegisterSession) {
		pthread_rwlock_wrlock(&m_hCacheRWLock);
		m_mapSessions.insert( SESSIONMAP::value_type(newSessionID, lpAuthSession) );
		pthread_rwlock_unlock(&m_hCacheRWLock);
	}

	*sessionID = newSessionID;
	*lppAuthSession = lpAuthSession;
	return erSuccess;
}
