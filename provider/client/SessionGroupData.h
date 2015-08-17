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

#ifndef ECSESSIONGROUPDATA_H
#define ECSESSIONGROUPDATA_H

#include <pthread.h>

#include <mapispi.h>

#include "ZarafaCode.h"
#include "ClientUtil.h"

class ECNotifyMaster;
class WSTransport;

class ECSessionGroupInfo {
public:
	std::string strServer;
	std::string strProfile;

	ECSessionGroupInfo()
		: strServer(), strProfile()
	{
	}

	ECSessionGroupInfo(const std::string &strServer, const std::string &strProfile)
		: strServer(strServer), strProfile(strProfile)
	{
	}
};

static inline bool operator==(const ECSessionGroupInfo &a, const ECSessionGroupInfo &b)
{
	return	(a.strServer.compare(b.strServer) == 0) &&
			(a.strProfile.compare(b.strProfile) == 0);
}

static inline bool operator<(const ECSessionGroupInfo &a, const ECSessionGroupInfo &b)
{
	return	(a.strServer.compare(b.strServer) < 0) ||
			((a.strServer.compare(b.strServer) == 0) && (a.strProfile.compare(b.strProfile) < 0));
}

class SessionGroupData
{
private:
	/* SessionGroup ID to which this data belongs */
	ECSESSIONGROUPID	m_ecSessionGroupId;
	ECSessionGroupInfo	m_ecSessionGroupInfo;

	/* Notification information */
	ECNotifyMaster*		m_lpNotifyMaster;
	ULONG				m_ulTransportFlags;

	/* Mutex */
	pthread_mutex_t		m_hMutex;
	pthread_mutexattr_t	m_hMutexAttrib;
	sGlobalProfileProps m_sProfileProps;

	/* Refcounting */
	pthread_mutex_t		m_hRefMutex;
	ULONG				m_cRef;

public:
	SessionGroupData(ECSESSIONGROUPID ecSessionGroupId, ECSessionGroupInfo *lpInfo, const sGlobalProfileProps &sProfileProps);
	~SessionGroupData(void);

	static HRESULT Create(ECSESSIONGROUPID ecSessionGroupId, ECSessionGroupInfo *lpInfo, const sGlobalProfileProps &sProfileProps, SessionGroupData **lppData);

	HRESULT GetOrCreateNotifyMaster(ECNotifyMaster **lppMaster);
	HRESULT GetTransport(WSTransport **lppTransport);
	
	ULONG AddRef();
	ULONG Release();
	
	BOOL IsOrphan();

	ECSESSIONGROUPID GetSessionGroupId();
};

#endif /* ECSESSIONGROUPDATA_H */
