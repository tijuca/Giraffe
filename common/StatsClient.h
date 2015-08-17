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

#ifndef __STATSCLIENT_H__
#define __STATSCLIENT_H__

#include "zcdefs.h"
#include <map>
#include <string>
#include <ctime>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>

#include "ECLogger.h"

class StatsClient _final {
private:
	int fd;
	struct sockaddr_un addr;
	int addr_len;
	ECLogger *const logger;

	pthread_t countsSubmitThread;
public:
	volatile bool terminate; // older compilers don't do atomic_bool
	pthread_mutex_t mapsLock;
	std::map<std::string, double> countsMapDouble;
	std::map<std::string, int64_t> countsMapInt64;

public:
	StatsClient(const std::string & collectorSocket, ECLogger *const logger);
	~StatsClient();

	inline ECLogger *const getLogger() { return logger; }

	void countInc(const std::string & key, const std::string & key_sub);
	void countAdd(const std::string & key, const std::string & key_sub, const double n);
	void countAdd(const std::string & key, const std::string & key_sub, const int64_t n);

	void submit(const std::string & key, const time_t ts, const double value);
	void submit(const std::string & key, const time_t ts, const int64_t value);
};

#endif
