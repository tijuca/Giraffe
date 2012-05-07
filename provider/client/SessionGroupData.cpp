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

#include <mapicode.h>
#include <mapix.h>

#include "ClientUtil.h"
#include "ECNotifyMaster.h"
#include "ECSessionGroupManager.h"
#include "SessionGroupData.h"
#include "WSTransport.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif


SessionGroupData::SessionGroupData(ECSESSIONGROUPID ecSessionGroupId, ECSessionGroupInfo *lpInfo, const sGlobalProfileProps &sProfileProps)
{
	m_ecSessionGroupId = ecSessionGroupId;

	if (lpInfo) {
		m_ecSessionGroupInfo.strServer = lpInfo->strServer;
		m_ecSessionGroupInfo.strProfile = lpInfo->strProfile;
	}

	m_lpNotifyMaster = NULL;
	m_sProfileProps = sProfileProps;
	m_cRef = 0;

	pthread_mutexattr_init(&m_hMutexAttrib);
	pthread_mutexattr_settype(&m_hMutexAttrib, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&m_hMutex, &m_hMutexAttrib);
	pthread_mutex_init(&m_hRefMutex, &m_hMutexAttrib);
}

SessionGroupData::~SessionGroupData(void)
{
	if (m_lpNotifyMaster)
		m_lpNotifyMaster->Release();

	pthread_mutex_destroy(&m_hMutex);
	pthread_mutex_destroy(&m_hRefMutex);
	pthread_mutexattr_destroy(&m_hMutexAttrib);
}

HRESULT SessionGroupData::Create(ECSESSIONGROUPID ecSessionGroupId, ECSessionGroupInfo *lpInfo, const sGlobalProfileProps &sProfileProps, SessionGroupData **lppData)
{
	HRESULT hr = hrSuccess;
	SessionGroupData *lpData = NULL;

	lpData = new SessionGroupData(ecSessionGroupId, lpInfo, sProfileProps);
	lpData->AddRef();

	*lppData = lpData;

	return hr;
}


HRESULT SessionGroupData::GetOrCreateNotifyMaster(ECNotifyMaster **lppMaster)
{
	HRESULT hr = hrSuccess;

	pthread_mutex_lock(&m_hMutex);

	if (!m_lpNotifyMaster)
		hr = ECNotifyMaster::Create(this, &m_lpNotifyMaster);

	pthread_mutex_unlock(&m_hMutex);

	*lppMaster = m_lpNotifyMaster;

	return hr;
}

HRESULT SessionGroupData::GetTransport(WSTransport **lppTransport)
{
	HRESULT hr = hrSuccess;
	WSTransport *lpTransport = NULL;

	hr = WSTransport::Create(MDB_NO_DIALOG, &lpTransport);
	if (hr != hrSuccess) 
		goto exit;

	hr = lpTransport->HrLogon(m_sProfileProps);
	if (hr != hrSuccess) 
		goto exit;

	// Since we are doing request that take max EC_SESSION_KEEPALIVE_TIME, set timeout to that plus 10 seconds
	lpTransport->HrSetRecvTimeout(EC_SESSION_KEEPALIVE_TIME + 10);

	*lppTransport = lpTransport;

exit:
	return hr;
}

ECSESSIONGROUPID SessionGroupData::GetSessionGroupId()
{
	return m_ecSessionGroupId;
}

ULONG SessionGroupData::AddRef()
{
	ULONG cRef;
    
	pthread_mutex_lock(&m_hRefMutex);
    m_cRef++;
	cRef = m_cRef;
    pthread_mutex_unlock(&m_hRefMutex);
	
	return cRef;
}

ULONG SessionGroupData::Release()
{
	ULONG cRef;
    
	pthread_mutex_lock(&m_hRefMutex);
	m_cRef--;
    cRef = m_cRef;
    pthread_mutex_unlock(&m_hRefMutex);

	return cRef;
}

BOOL SessionGroupData::IsOrphan()
{
    return m_cRef == 0;
}
