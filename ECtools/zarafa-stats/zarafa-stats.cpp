/*
 * Copyright 2005 - 2009  Zarafa B.V.
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

#include <iostream>
#include <fstream>
#include <mapi.h>
#include <mapiutil.h>
#include <curses.h>
#include <edkmdb.h>

#include <map>
#include <set>

#include "CommonUtil.h"
#include "stringutil.h"
#include "ECTags.h"
#include "my_getopt.h"
#include "charset/convert.h"

using namespace std;

enum eTableType { INVALID_STATS = -1, SYSTEM_STATS, SESSION_STATS, USER_STATS, COMPANY_STATS, SESSION_TOP, OPTION_HOST, OPTION_USER };

struct option long_options[] = {
		{ "system", 0, NULL, SYSTEM_STATS },
		{ "sessions", 0, NULL, SESSION_STATS },
		{ "users", 0, NULL, USER_STATS },
		{ "company", 0, NULL, COMPANY_STATS },
		{ "top", 0, NULL, SESSION_TOP },
		{ "host", 1, NULL, OPTION_HOST },
		{ "user", 1, NULL, OPTION_USER },
		{ NULL, 0, NULL, 0 }
};

// sort on something invalid to get the order in which the server added the rows
const static SizedSSortOrderSet(2, tableSortSystem) =
{ 1, 0, 0,
  {
	  { PR_NULL, TABLE_SORT_DESCEND }
  }
}
;
const static SizedSSortOrderSet(2, tableSortSession) =
{ 2, 0, 0,
  {
	  { PR_EC_STATS_SESSION_IPADDRESS, TABLE_SORT_ASCEND },
	  { PR_EC_STATS_SESSION_IDLETIME, TABLE_SORT_DESCEND }
  }
};

const static SizedSSortOrderSet(2, tableSortUser) =
{ 2, 0, 0,
  {
	  { PR_EC_COMPANY_NAME, TABLE_SORT_ASCEND },
	  { PR_EC_USERNAME_A, TABLE_SORT_ASCEND },
  }
};

const static SizedSSortOrderSet(2, tableSortCompany) =
{ 1, 0, 0,
  {
	  { PR_EC_COMPANY_NAME, TABLE_SORT_ASCEND }
  }
};

LPSSortOrderSet sortorders[] = {
	(LPSSortOrderSet)&tableSortSystem, (LPSSortOrderSet)&tableSortSession,
	(LPSSortOrderSet)&tableSortUser, (LPSSortOrderSet)&tableSortCompany
};
ULONG ulTableProps[] = {
	PR_EC_STATSTABLE_SYSTEM, PR_EC_STATSTABLE_SESSIONS,
	PR_EC_STATSTABLE_USERS, PR_EC_STATSTABLE_COMPANY
};

typedef struct {
    double dblUser;
    double dblSystem;
    double dblReal;
    unsigned int ulRequests;
} TIMES;

typedef struct _SESSION {
    unsigned long long ullSessionId;
    unsigned long long ullSessionGroupId;
    TIMES times;
    TIMES dtimes;

    unsigned int ulIdle;
    int ulPeerPid;
    bool bLocked;
    
    std::string strUser;
    std::string strIP;
    std::string strBusy;
    std::string strPeer;
    std::string strClientVersion;
    std::string strClientApp;
    
    bool operator < (const _SESSION &b) { 
        double total = this->dtimes.dblUser + this->dtimes.dblSystem;
        double btotal = b.dtimes.dblUser + b.dtimes.dblSystem;
        
        if(total == btotal) {
            return this->times.dblUser + this->times.dblSystem > b.times.dblUser + b.times.dblSystem;
        } else {
            return total > btotal;
        }
    }
} SESSION;

bool sort_sessionid(const SESSION &a, const SESSION &b) { return a.ullSessionId < b.ullSessionId; } // && group < ?
bool sort_user(const SESSION &a, const SESSION &b) { return a.strUser < b.strUser; }
bool sort_busy(const SESSION &a, const SESSION &b) { return a.strBusy < b.strBusy; }
bool sort_ippeer(const SESSION &a, const SESSION &b) { return a.strIP < b.strIP || (a.strIP == b.strIP && a.strPeer < b.strPeer); }
bool sort_version(const SESSION &a, const SESSION &b) { return a.strClientVersion < b.strClientVersion; }
bool sort_app(const SESSION &a, const SESSION &b) { return a.strClientApp < b.strClientApp; }

typedef bool(*SortFuncPtr)(const SESSION&, const SESSION&);
SortFuncPtr sortfunc;

std::string GetString(LPSPropValue lpProps, ULONG cValues, ULONG ulPropTag)
{
    LPSPropValue lpProp = PpropFindProp(lpProps, cValues, ulPropTag);
    
    if(lpProp == NULL)
        return "";
        
    switch(PROP_TYPE(ulPropTag)) {
        case PT_STRING8: return lpProp->Value.lpszA;
        case PT_MV_STRING8: {
            std::string s;
            for(ULONG i = 0; i < lpProp->Value.MVszA.cValues; i++) {
                s+= lpProp->Value.MVszA.lppszA[i];
                s+= " ";
            }
            return s;
        }
        case PT_LONG: {
            char s[80];
            snprintf(s, sizeof(s), "%d", lpProp->Value.ul);
            return s;
        }
    }
    
    return "";
}

std::string GetPidName(int pid)
{
    std::string procpath = "/proc/" + stringify(pid) + "/exe";
    char s[1024];
    memset(s, 0, sizeof(s));

    if(readlink(procpath.c_str(), s, sizeof(s)) == -1)
        return stringify(pid) + " <defunct>";
    
    s[sizeof(s)-1] = 0;

    return stringify(pid) + " " + basename(s);    
}

unsigned long long GetLongLong(LPSPropValue lpProps, ULONG cValues, ULONG ulPropTag)
{
    LPSPropValue lpProp = PpropFindProp(lpProps, cValues, ulPropTag);
    
    if(lpProp == NULL)
        return -1;
        
    switch(PROP_TYPE(ulPropTag)) {
        case PT_LONG: return lpProp->Value.ul;
        case PT_LONGLONG: return lpProp->Value.li.QuadPart;
    }

    return 0;
}

double GetDouble(LPSPropValue lpProps, ULONG cValues, ULONG ulPropTag)
{
    LPSPropValue lpProp = PpropFindProp(lpProps, cValues, ulPropTag);
    
    if(lpProp == NULL)
        return 0;
        
    switch(PROP_TYPE(ulPropTag)) {
        case PT_DOUBLE: return lpProp->Value.dbl;
    }
    
    return 0;
}

void showtop(LPMDB lpStore, bool bLocal)
{
    HRESULT hr = hrSuccess;
    IMAPITable *lpTable = NULL;
    LPSRowSet lpsRowSet = NULL;
    WINDOW *win = NULL;
    std::map<unsigned long long, TIMES> mapLastTimes;
    std::map<unsigned long long, TIMES>::iterator iterTimes;
    std::map<std::string, std::string> mapStats;
    std::map<std::string, double> mapDiffStats;
    std::list<SESSION> lstSessions;
    std::list<SESSION>::iterator iterSessions;
    std::set<std::string> setUsers;
    std::set<std::string> setHosts;
    std::map<unsigned long long, unsigned int> mapSessionGroups;
    std::map<unsigned long long, unsigned int>::iterator iterSessionGroups;
	char date[64];
	time_t now;
    unsigned int ulSessGrp = 1;
    double dblUser, dblSystem;
    int wx,wy;
    int key;

	// columns in sizes, not literal offsets
	int cols[] = {0,4,21,13,12,16,15,8,8,7,7};
	int ofs = 0;
	bool bColumns[] = {false,false,true,true,true,true,true,true,true,true,true}; // key 1 through err?
	SortFuncPtr fSort[] = {NULL,sort_sessionid,sort_version,sort_user,sort_ippeer,sort_app,NULL}; // key a through g
	bool bReverse = false;
	SizedSPropTagArray (2, sptaSystem) = { 2, { PR_DISPLAY_NAME_A, PR_EC_STATS_SYSTEM_VALUE } };

    // Init ncurses
    win = initscr(); 
    if(win == NULL) {
        cerr << "ncurses error\n";
        return;
    }

    cbreak(); 
    noecho();
    nonl();
    nodelay(win, TRUE);

    getmaxyx(win, wy, wx);

    while(1) {
        int line = 0;
		werase(win);

		hr = lpStore->OpenProperty(PR_EC_STATSTABLE_SYSTEM, &IID_IMAPITable, 0, 0, (LPUNKNOWN*)&lpTable);
		if(hr != hrSuccess)
		    goto exit;

		hr = lpTable->SetColumns((LPSPropTagArray)&sptaSystem, 0);
		if(hr != hrSuccess)
			goto exit;
		    
        hr = lpTable->QueryRows(-1, 0, &lpsRowSet);
        if(hr != hrSuccess)
            goto exit;
            
        for(ULONG i = 0; i< lpsRowSet->cRows;i++) {
            LPSPropValue lpName = PpropFindProp(lpsRowSet->aRow[i].lpProps, lpsRowSet->aRow[i].cValues, PR_DISPLAY_NAME_A);
            LPSPropValue lpValue = PpropFindProp(lpsRowSet->aRow[i].lpProps, lpsRowSet->aRow[i].cValues, PR_EC_STATS_SYSTEM_VALUE);
            
            if(lpName && lpValue) {
                mapDiffStats[lpName->Value.lpszA] = atof(lpValue->Value.lpszA) - atof(mapStats[lpName->Value.lpszA].c_str());
                mapStats[lpName->Value.lpszA] = lpValue->Value.lpszA;
            }
        }
        
        FreeProws(lpsRowSet);
        lpsRowSet = NULL;
        lpTable->Release();
        lpTable = NULL;

        hr = lpStore->OpenProperty(PR_EC_STATSTABLE_SESSIONS, &IID_IMAPITable, 0, 0, (LPUNKNOWN*)&lpTable);
        if(hr != hrSuccess)
            goto exit;

        hr = lpTable->QueryRows(-1, 0, &lpsRowSet);
        if(hr != hrSuccess)
            break;
            
        lstSessions.clear();
        mapSessionGroups.clear();
        setUsers.clear();
        dblUser = dblSystem = 0;
        ulSessGrp = 1;
        
        for(ULONG i = 0; i < lpsRowSet->cRows; i++) {
            SESSION session;

            session.strUser = GetString(lpsRowSet->aRow[i].lpProps, lpsRowSet->aRow[i].cValues, PR_EC_USERNAME_A);
            session.strIP = GetString(lpsRowSet->aRow[i].lpProps, lpsRowSet->aRow[i].cValues, PR_EC_STATS_SESSION_IPADDRESS);
            session.strBusy = GetString(lpsRowSet->aRow[i].lpProps, lpsRowSet->aRow[i].cValues, PR_EC_STATS_SESSION_BUSYSTATES);
            session.strClientVersion = GetString(lpsRowSet->aRow[i].lpProps, lpsRowSet->aRow[i].cValues, PR_EC_STATS_SESSION_CLIENT_VERSION);
            session.strClientApp = GetString(lpsRowSet->aRow[i].lpProps, lpsRowSet->aRow[i].cValues, PR_EC_STATS_SESSION_CLIENT_APPLICATION);

            session.ulPeerPid = GetLongLong(lpsRowSet->aRow[i].lpProps, lpsRowSet->aRow[i].cValues, PR_EC_STATS_SESSION_PEER_PID);
            session.times.ulRequests = GetLongLong(lpsRowSet->aRow[i].lpProps, lpsRowSet->aRow[i].cValues, PR_EC_STATS_SESSION_REQUESTS);
            session.ullSessionId = GetLongLong(lpsRowSet->aRow[i].lpProps, lpsRowSet->aRow[i].cValues, PR_EC_STATS_SESSION_ID);
            session.ullSessionGroupId = GetLongLong(lpsRowSet->aRow[i].lpProps, lpsRowSet->aRow[i].cValues, PR_EC_STATS_SESSION_GROUP_ID);
            if(session.ulPeerPid) {
				session.strPeer = stringify(session.ulPeerPid);
			}

            session.times.dblUser = GetDouble(lpsRowSet->aRow[i].lpProps, lpsRowSet->aRow[i].cValues, PR_EC_STATS_SESSION_CPU_USER);
            session.times.dblSystem = GetDouble(lpsRowSet->aRow[i].lpProps, lpsRowSet->aRow[i].cValues, PR_EC_STATS_SESSION_CPU_SYSTEM);
            session.times.dblReal = GetDouble(lpsRowSet->aRow[i].lpProps, lpsRowSet->aRow[i].cValues, PR_EC_STATS_SESSION_CPU_REAL);

            iterTimes = mapLastTimes.find(session.ullSessionId);
            if(iterTimes != mapLastTimes.end()) {
                session.dtimes.dblUser = session.times.dblUser - iterTimes->second.dblUser;
                session.dtimes.dblSystem = session.times.dblSystem - iterTimes->second.dblSystem;
                session.dtimes.dblReal = session.times.dblReal - iterTimes->second.dblReal;
                
                dblUser += session.dtimes.dblUser;
                dblSystem += session.dtimes.dblSystem;
                
                session.dtimes.ulRequests = session.times.ulRequests - iterTimes->second.ulRequests;
            } else {
                session.dtimes.dblUser = session.dtimes.dblSystem = session.dtimes.dblReal = 0;
            }
            mapLastTimes[session.ullSessionId] = session.times;
            
            lstSessions.push_back(session);
            setUsers.insert(session.strUser);
            setHosts.insert(session.strIP);
            
            if(session.ullSessionGroupId != 0) {
                iterSessionGroups = mapSessionGroups.find(session.ullSessionGroupId);
                if(iterSessionGroups == mapSessionGroups.end())
                    mapSessionGroups[session.ullSessionGroupId] = ulSessGrp++;
            }
        }

		if (sortfunc)
			lstSessions.sort(sortfunc);
		else
			lstSessions.sort();
		if (bReverse)
			lstSessions.reverse();

        wmove(win, 0,0);
		memset(date, 0, 64);
		now = time(NULL);
		strftime(date, 64, "%c", localtime(&now) );
        wprintw(win, "Last update: %s", date);
		
        wmove(win, 1,0);
        wprintw(win, "Sess: %d", lstSessions.size());
        wmove(win, 1, 12);
        wprintw(win, "Sess grp: %d", mapSessionGroups.size());
        wmove(win, 1, 30);
        wprintw(win, "Users: %d", setUsers.size());
        wmove(win, 1, 42);
        wprintw(win, "Hosts: %d", setHosts.size());
        wmove(win, 1, 54);
        wprintw(win, "CPU: %d%%", (int)((dblUser+dblSystem)*100));
        wmove(win, 1, 64);
        wprintw(win, "QLen: %s", mapStats["queuelen"].c_str());
        wmove(win, 1, 73);
        wprintw(win, "QAge: %.5s", mapStats["queueage"].c_str());
        wmove(win, 1, 85);
        if(mapDiffStats["soap_request"] > 0)
            wprintw(win, "RTT: %d ms", (int)(mapDiffStats["response_time"] / mapDiffStats["soap_request"]));

        wmove(win, 2, 0);
        wprintw(win, "SQL/s SEL:%5d UPD:%4d INS:%4d DEL:%4d", (int)mapDiffStats["sql_select"], (int)mapDiffStats["sql_update"], (int)mapDiffStats["sql_insert"], (int)mapDiffStats["sql_delete"]);
        wmove(win, 2, 50);
        wprintw(win, "Threads(idle): %s(%s)", mapStats["threads"].c_str(), mapStats["threads_idle"].c_str());
        wmove(win, 2, 75);
        wprintw(win, "MWOPS: %d  MROPS: %d", (int)mapDiffStats["mwops"], (int)mapDiffStats["mrops"]);
        wmove(win, 2, 102);
        wprintw(win, "SOAP calls: %d", (int)mapDiffStats["soap_request"]);

		ofs = cols[0];
        if (bColumns[0]) { wmove(win, 4, ofs); wprintw(win, "GRP");			ofs += cols[1]; }
        if (bColumns[1]) { wmove(win, 4, ofs); wprintw(win, "SESSIONID");	ofs += cols[2]; }
        if (bColumns[2]) { wmove(win, 4, ofs); wprintw(win, "VERSION");		ofs += cols[3]; }
        if (bColumns[3]) { wmove(win, 4, ofs); wprintw(win, "USERID");		ofs += cols[4]; }
        if (bColumns[4]) { wmove(win, 4, ofs); wprintw(win, "IP/PID");		ofs += cols[5]; }
        if (bColumns[5]) { wmove(win, 4, ofs); wprintw(win, "APP");			ofs += cols[6]; }
        if (bColumns[6]) { wmove(win, 4, ofs); wprintw(win, "TIME");		ofs += cols[7]; }
        if (bColumns[7]) { wmove(win, 4, ofs); wprintw(win, "CPUTIME");		ofs += cols[8]; }
        if (bColumns[8]) { wmove(win, 4, ofs); wprintw(win, "CPU%");		ofs += cols[9]; }
        if (bColumns[9]) { wmove(win, 4, ofs); wprintw(win, "NREQ");		ofs += cols[10]; }
        if (bColumns[10]) { wmove(win, 4, ofs); wprintw(win, "TASK"); }
        
        for(iterSessions = lstSessions.begin(); iterSessions != lstSessions.end(); iterSessions++) {
            if(iterSessions->dtimes.dblUser + iterSessions->dtimes.dblSystem > 0)
                wattron(win, A_BOLD);
            else
                wattroff(win, A_BOLD);

			ofs = cols[0];
			if (bColumns[0]) {
				wmove(win, 5 + line, ofs);
				if(iterSessions->ullSessionGroupId > 0)
					wprintw(win, "%3d", mapSessionGroups[iterSessions->ullSessionGroupId]);
				ofs += cols[1];
			}
			if (bColumns[1]) {
				wmove(win, 5 + line, ofs);
				wprintw(win, "%19llu", iterSessions->ullSessionId);
				ofs += cols[2];
			}
			if (bColumns[2]) {
				wmove(win, 5 + line, ofs);
				wprintw(win, "%.12s", iterSessions->strClientVersion.c_str());
				ofs += cols[3];
			}
			if (bColumns[3]) {
				wmove(win, 5 + line, ofs);
				wprintw(win, "%.11s", iterSessions->strUser.c_str());
				ofs += cols[4];
			}
			if (bColumns[4]) {
				wmove(win, 5 + line, ofs);
				if(iterSessions->ulPeerPid > 0)
					wprintw(win, "%.20s", iterSessions->strPeer.c_str());
				else
					wprintw(win, "%s", iterSessions->strIP.c_str());
				ofs += cols[5];
			}
			if (bColumns[5]) {
				wmove(win, 5 + line, ofs);
				wprintw(win, "%.14s", iterSessions->strClientApp.c_str());
				ofs += cols[6];
			}
			if (bColumns[6]) {
				wmove(win, 5 + line, ofs);
				wprintw(win, "%d:%02d", (int)iterSessions->times.dblReal/60, (int)iterSessions->times.dblReal%60);
				ofs += cols[7];
			}
			if (bColumns[7]) {
				wmove(win, 5 + line, ofs);
				wprintw(win, "%d:%02d", (int)(iterSessions->times.dblUser + iterSessions->times.dblSystem)/60, (int)(iterSessions->times.dblUser + iterSessions->times.dblUser)%60);
				ofs += cols[8];
			}
			if (bColumns[8]) {
				wmove(win, 5 + line, ofs);
				wprintw(win, "%d", (int)(iterSessions->dtimes.dblUser*100.0 + iterSessions->dtimes.dblSystem*100.0));
				ofs += cols[9];
			}
			if (bColumns[9]) {
				wmove(win, 5 + line, ofs);
				wprintw(win, "%d", (int)iterSessions->dtimes.ulRequests);
				ofs += cols[10];
			}
			if (bColumns[10]) {
				wmove(win, 5 + line, ofs);
				wprintw(win, "%s", iterSessions->strBusy.c_str());
			}

            line++;
            
            if(line + 5>= wy)
            	break;
        }
		wattroff(win, A_BOLD);

        FreeProws(lpsRowSet);
        lpsRowSet = NULL;

        lpTable->Release();
        lpTable = NULL;
        
        wrefresh(win);
        timeout(1000);
        if((key = getch()) != ERR) {
			if (key == 27)				// escape key
				break;
			if (key == KEY_RESIZE)		// resize action
				getmaxyx(win, wy, wx);
			if (key >= '0' && key <= '9')
				bColumns[key-'1'] = !bColumns[key-'1']; // en/disable columns
			if (key >= 'a' && key <= 'g') {				// 'a' - 'g', sort columns
				if (sortfunc == fSort[key-'a'])
					bReverse = !bReverse;
				else
					bReverse = false;
				sortfunc = fSort[key-'a'];
			}
		}
    }

exit:
    endwin();
    
    if(lpTable)
        lpTable->Release();
    if(lpsRowSet)
        FreeProws(lpsRowSet);
}
 
void dumptable(eTableType eTable, LPMDB lpStore) {
	HRESULT hr = hrSuccess;
	IMAPITable *lpTable = NULL;
	LPSRowSet lpRowSet = NULL;
	ULONG c, p, m;
	time_t t;
	char *szTimeString = NULL;
	ULONG ulTimeLength = 0;

	hr = lpStore->OpenProperty(ulTableProps[eTable], &IID_IMAPITable, 0, MAPI_DEFERRED_ERRORS, (LPUNKNOWN*)&lpTable);
	if (hr != hrSuccess) {
		cout << "Unable to open requested statistics table" << endl;
		goto exit;
	}

	if (sortorders[eTable] != NULL)
		hr = lpTable->SortTable(sortorders[eTable], 0);

	if (hr != hrSuccess) {
		cout << "Unable to sort statistics table" << endl;
		goto exit;
	}

	while (TRUE) {

		hr = lpTable->QueryRows(50, 0, &lpRowSet);
		if (hr != hrSuccess)
			goto exit;

		if (lpRowSet->cRows == 0)
			break;

		for (c = 0; c < lpRowSet->cRows; c++) {
			for (p = 0; p < lpRowSet->aRow[c].cValues; p++) {
				cout << stringify(lpRowSet->aRow[c].lpProps[p].ulPropTag, true) << ": ";
				switch (PROP_TYPE(lpRowSet->aRow[c].lpProps[p].ulPropTag)) {
				case PT_SHORT:
					cout << lpRowSet->aRow[c].lpProps[p].Value.i;
					break;
				case PT_LONG:
					cout << lpRowSet->aRow[c].lpProps[p].Value.ul;
					break;
				case PT_LONGLONG:
					cout << lpRowSet->aRow[c].lpProps[p].Value.li.QuadPart;
					break;
				case PT_FLOAT:
					cout << lpRowSet->aRow[c].lpProps[p].Value.flt;
					break;
				case PT_DOUBLE:
					cout << lpRowSet->aRow[c].lpProps[p].Value.dbl;
					break;
				case PT_SYSTIME:
					FileTimeToUnixTime(lpRowSet->aRow[c].lpProps[p].Value.ft, &t);
					szTimeString = ctime(&t);
					ulTimeLength = strlen(szTimeString);
					szTimeString[ulTimeLength-1] = '\0'; // -1 to strip enter from ctime
					cout << szTimeString;
					break;
				case PT_BOOLEAN:
					cout << (lpRowSet->aRow[c].lpProps[p].Value.b ? "True" : "False");
					break;
				case PT_STRING8:
					cout << lpRowSet->aRow[c].lpProps[p].Value.lpszA;
					break;
				case PT_MV_STRING8:
					for (m = 0; m < lpRowSet->aRow[c].lpProps[p].Value.MVszA.cValues; m++) {
						cout << lpRowSet->aRow[c].lpProps[p].Value.MVszA.lppszA[m];
						if (m+1 < lpRowSet->aRow[c].lpProps[p].Value.MVszA.cValues)
							cout << ", ";
					}
					break;
				case PT_ERROR:
					cout << "error: " << stringify(lpRowSet->aRow[c].lpProps[p].Value.err, true);
					break;
				default:
					cout << "Unable to display this property";
					break;
				}
				cout << endl;
			}
			if (eTable != SYSTEM_STATS)
				cout << endl;
		}

		FreeProws(lpRowSet);
		lpRowSet = NULL;
	}

exit:
	if (lpRowSet)
		FreeProws(lpRowSet);

	if (lpTable)
		lpTable->Release();
}

void print_help(char *name)
{
	cout << "Usage:" << endl;
	cout << name << " <options> [table]" << endl << endl;
	cout << "Supported tables:" << endl;
	cout << "  --system" << "\tGives information about threads, SQL and caches" << endl;
	cout << "  --session" << "\tGives information about sessions and server time spent in SOAP calls" << endl;
	cout << "  --users" << "\tGives information about users, store sizes and quotas" << endl;
	cout << "  --company" << "\tGives information about companies, company sizes and quotas" << endl;
	cout << "  --top" << "\t\tShows top-like information about sessions" << endl;
	cout << "Options:" << endl;
	cout << "  --user, -u <user>" << "\tUse specified username to logon" << endl;
	cout << "  --host, -h <url>" << "\tUse specified url to logon (eg http://127.0.0.1:236/zarafa)" << endl;
}

int main(int argc, char *argv[])
{
	HRESULT hr = hrSuccess;
	IMAPISession *lpSession = NULL;
	LPMDB lpStore = NULL;
	eTableType eTable = INVALID_STATS;
	char *user = NULL;
	char *pass = NULL;
	char *host = NULL;
	wstring strwUsername;
	wstring strwPassword;

	setlocale(LC_MESSAGES, "");
	setlocale(LC_CTYPE, "");
	
	if(argc < 2) {
		print_help(argv[0]);
		return 1;
	}

	int c;
	while (1) {
		c = my_getopt_long(argc, argv, "h:u:", long_options, NULL);
		if(c == -1)
			break;
		switch(c) {
			case '?':
				print_help(argv[0]);
				return 1;
			case OPTION_HOST:
			case 'h': 
				host = my_optarg;
				break;
			case 'u':
				user = my_optarg;
				break;
			case SYSTEM_STATS:
			case SESSION_STATS:
			case USER_STATS: 
			case COMPANY_STATS:
			case SESSION_TOP:
				eTable = (eTableType)c;
				break;
		}
	}
	if (eTable == INVALID_STATS) {
		print_help(argv[0]);
		return 1;
	}

	hr = MAPIInitialize(NULL);
	if (hr != hrSuccess) {
		cerr << "Cannot init mapi" << endl;
		goto exit;
	}
	
	if(user) {
        pass = get_password("Enter password:");
        if(!pass) {
            cout << "Invalid password." << endl;
            return 1;
	    }
	}

	strwUsername = convert_to<wstring>(user ? user : "SYSTEM");
	strwPassword = convert_to<wstring>(pass ? pass : "");

	hr = HrOpenECSession(&lpSession, strwUsername.c_str(), strwPassword.c_str(), (const char *)host, EC_PROFILE_FLAGS_NO_NOTIFICATIONS | EC_PROFILE_FLAGS_NO_PUBLIC_STORE);
	if (hr != hrSuccess) {
		cout << "Cannot open admin session on host " << (host ? host : (char *)"localhost") << ", username " << (user ? user : (char *)"SYSTEM") << endl;
		goto exit;
	}

	hr = HrOpenDefaultStore(lpSession, &lpStore);
	if (hr != hrSuccess) {
		cout << "Unable to open default store" << endl;
		goto exit;
	}

	if(eTable == SESSION_TOP)
    	showtop(lpStore, host == NULL);
    else
    	dumptable(eTable, lpStore);

exit:
	if(lpStore)
		lpStore->Release();

	if(lpSession)
		lpSession->Release();

	MAPIUninitialize();

	return hr != hrSuccess;
}
