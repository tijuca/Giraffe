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
#include "ECTestProtocol.h"
#include "SOAPUtils.h"
#include "ECSessionManager.h"
#include "ECSession.h"
#include "ECTPropsPurge.h"
#include "ECSearchClient.h"

struct soap;

extern ECSessionManager*    g_lpSessionManager;

ECRESULT TestPerform(struct soap *soap, ECSession *lpSession, char *szCommand, unsigned int ulArgs, char **args)
{
    ECRESULT er = erSuccess;

    if(stricmp(szCommand, "purge_deferred") == 0) {
        while (1) {
            unsigned int ulFolderId = 0;
			ECDatabase *lpDatabase = NULL;

			er = lpSession->GetDatabase(&lpDatabase);
            if(er != erSuccess)
				goto exit;
			
            er = ECTPropsPurge::GetLargestFolderId(lpDatabase, &ulFolderId);
            if(er != erSuccess) {
                er = erSuccess;
                break;
            }
            
            er = ECTPropsPurge::PurgeDeferredTableUpdates(lpDatabase, ulFolderId);
            if(er != erSuccess)
                goto exit;
        }
            
    } else if (stricmp(szCommand, "indexer_syncrun") == 0) {
		if (parseBool(g_lpSessionManager->GetConfig()->GetSetting("search_enabled"))) {
			er = ECSearchClient(
				g_lpSessionManager->GetConfig()->GetSetting("search_socket"),
				60 * 10 /* 10 minutes should be enough for everyone */
			).SyncRun();
		}
	} else if (stricmp(szCommand, "run_searchfolders") == 0) {
		lpSession->GetSessionManager()->GetSearchFolders()->FlushAndWait();
	} else if (stricmp(szCommand, "kill_sessions") == 0) {
	    ECSESSIONID id = lpSession->GetSessionId();
	    
	    // Remove all sessions except our own
	    er = lpSession->GetSessionManager()->CancelAllSessions(id);
    } else if(stricmp(szCommand, "sleep") == 0) {
        if(ulArgs == 1 && args[0])
            Sleep(atoui(args[0]) * 1000);
    }
    
exit:
    return er;
}

ECRESULT TestSet(struct soap *soap, ECSession *lpSession, char *szVarName, char *szValue)
{
    ECRESULT er = erSuccess;

    if(stricmp(szVarName, "cell_cache_disabled") == 0) {
        if(atoi(szValue) > 0)
            g_lpSessionManager->GetCacheManager()->DisableCellCache();
        else
            g_lpSessionManager->GetCacheManager()->EnableCellCache();
            
    } else if (stricmp(szVarName, "search_enabled") == 0) {
		// Since there's no object that represents the indexer, it's probably cleanest to
		// update the configuration.
		if (atoi(szValue) > 0)
			g_lpSessionManager->GetConfig()->AddSetting("search_enabled", "yes", 0);
		else
			g_lpSessionManager->GetConfig()->AddSetting("search_enabled", "no", 0);
	}
    
    return er;
}

ECRESULT TestGet(struct soap *soap, ECSession *lpSession, char *szVarName, char **szValue)
{
    ECRESULT er = erSuccess;
    
    if(!stricmp(szVarName, "ping")) {
        *szValue = s_strcpy(soap, "pong");
    } else {
        er = ZARAFA_E_NOT_FOUND;
    }
    
    return er;
}
