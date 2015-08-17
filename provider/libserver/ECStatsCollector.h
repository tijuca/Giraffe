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

#ifndef EC_STATS_COLLECTOR_H
#define EC_STATS_COLLECTOR_H

#include "zcdefs.h"
#include <string>
#include <map>
#include <pthread.h>

#include "IECStatsCollector.h"

typedef union _statdata {
	float f;
	LONGLONG ll;
	time_t ts;
} SCData;

enum SCType { SCDT_FLOAT, SCDT_LONGLONG, SCDT_TIMESTAMP };

typedef struct _ECStat {
	SCData data;
	LONGLONG avginc;
	SCType type;
	const char *name;
	const char *description;
	pthread_mutex_t lock;
} ECStat;

typedef std::map<SCName, ECStat> SCMap;

typedef struct _ECStrings {
	std::string description;
	std::string value;
} ECStrings;

class ECStatsCollector _final : public IECStatsCollector {
public:
	ECStatsCollector();
	~ECStatsCollector();

	void Increment(SCName name, float inc) _override;
	void Increment(SCName name, int inc = 1) _override;
	void Increment(SCName name, LONGLONG inc) _override;

	void Set(SCName name, float set) _override;
	void Set(SCName name, LONGLONG set) _override;
	void SetTime(SCName name, time_t set) _override;

	void Min(SCName name, float min) _override;
	void Min(SCName name, LONGLONG min) _override;
	void MinTime(SCName name, time_t min) _override;

	void Max(SCName name, float max) _override;
	void Max(SCName name, LONGLONG max) _override;
	void MaxTime(SCName name, time_t max) _override;

	void Avg(SCName name, float add) _override;
	void Avg(SCName name, LONGLONG add) _override;
	void AvgTime(SCName name, time_t add) _override;

	/* strings are separate, used by ECSerial */
	void Set(const std::string &name, const std::string &description, const std::string &value) _override;
	void Remove(const std::string &name) _override;

	std::string GetValue(SCMap::iterator iSD);
	std::string GetValue(SCName name) _override;

	void ForEachStat(void(callback)(const std::string &, const std::string &, const std::string &, void*), void *obj);
	void ForEachString(void(callback)(const std::string &, const std::string &, const std::string &, void*), void *obj);

	void Reset(void) _override;
	void Reset(SCName name) _override;

private:
	void AddStat(SCName index, SCType type, const char *name, const char *description);

	SCMap m_StatData;
	pthread_mutex_t m_StringsLock;
	std::map<std::string, ECStrings> m_StatStrings;
};

/* actual variable is in ECServerEntryPoint.cpp */
extern ECStatsCollector* g_lpStatsCollector;

#endif
