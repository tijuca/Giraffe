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

#ifndef ECSERVERINDEXER_H
#define ECSERVERINDEXER_H

#include <mapidefs.h>
#include <set>
#include <list>

class ECThreadData;
class ECLogger;
class ECConfig;
class IMAPISession;
class IMsgStore;
class ECIndexDB;

#include "ECUnknown.h"
#include "ECIndexImporter.h"

/**
 * The ECServerIndexer's job is to create and update all the indexes for a server. It does this
 * using a single thread that is created and destroyed at the same time as the ECServerIndexer object.
 *
 * It does this by first initiating a full index of all the data on a server. After that, it enters
 * the incremental state, when only changes are processed.
 */

class ECServerIndexer : public ECUnknown {
public:
    static HRESULT Create(ECConfig *lpConfig, ECLogger *lpLogger, ECThreadData *lpThreadData, ECServerIndexer **lppIndexer);

    // This is only to FORCE a new synchronization NOW. This is normally not needed in
    // production.
    HRESULT RunSynchronization();
    
private:
    ECServerIndexer(ECConfig *lpConfig, ECLogger *lpLogger, ECThreadData *lpThreadData);
    ~ECServerIndexer();
    
    HRESULT Start();
    HRESULT Thread();
    static void *ThreadEntry(void *lpParam);
    
    HRESULT BuildIndexes();
    HRESULT IndexStore(SBinary *lpsEntryId, unsigned int ulStoreType);
    HRESULT IndexFolder(IMsgStore *lpStore, SBinary *lpsEntryId, const WCHAR *szName, ECIndexImporter *lpImporter, ECIndexDB *lpIndexDB);
    HRESULT IndexStubTargets(const std::list<ECIndexImporter::ArchiveItem> *lpStubTargets, ECIndexImporter *lpImporter);
    HRESULT IndexStubTargetsServer(const std::string& strServerName, const std::list<std::string> &lstStubTargets, ECIndexImporter *lpImporter);
    HRESULT GetServerState(std::string &strState);
    HRESULT ProcessChanges();
    HRESULT GetServerGUID(GUID *lpGuid);

    ECLogger *m_lpLogger;
    ECConfig *m_lpConfig;
    ECThreadData *m_lpThreadData;
	MAPISessionPtr m_ptrSession;
	MsgStorePtr m_ptrAdminStore;
    
    // Thread synchronization
    pthread_t m_thread; 
    pthread_mutex_t m_mutexExit;
    pthread_cond_t m_condExit;
    bool m_bExit;

    pthread_mutex_t m_mutexTrack;
    pthread_cond_t m_condTrack;
    unsigned int m_ulTrack;

    bool m_bFast;
    bool m_bThreadStarted;
    
    GUID m_guidServer;

    // State information, save to disk and loaded from disk by LoadState()/SaveState()
    unsigned int m_ulIndexerState;				// 0 = initial load, 1 = incremental    
    std::string m_strICSState;					// During state 1, contains server ICS state 

    HRESULT LoadState();
    HRESULT SaveState();

    enum { stateUnknown = 0, stateBuilding, stateRunning };
 
};

#endif
