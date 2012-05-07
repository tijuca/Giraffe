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

#include <iostream>
#include <list>

using namespace std;

#include "my_getopt.h"

#include "LDAPConfigCheck.h"
#include "UnixConfigCheck.h"
#include "ServerConfigCheck.h"
#include "GatewayConfigCheck.h"
#include "IcalConfigCheck.h"
#include "MonitorConfigCheck.h"
#include "SpoolerConfigCheck.h"
#include "DAgentConfigCheck.h"

static const struct option long_options[] = {
	{ "help",   no_argument,		NULL, 'h' },
	{ "ldap",	required_argument,	NULL, 'l' },
	{ "unix",	required_argument,	NULL, 'u' },
	{ "server",	required_argument,	NULL, 's' },
	{ "gateway",required_argument,	NULL, 'g' },
	{ "ical",	required_argument,	NULL, 'i' },
	{ "monitor",required_argument,	NULL, 'm' },
	{ "spooler",required_argument,	NULL, 'p' },
	{ "dagent", required_argument,	NULL, 'a' },
	{ "hosted", required_argument,	NULL, 'c' },
	{ "distributed", required_argument,	NULL, 'd' },
	{},
};

static void print_help(char *lpszName)
{
	cout << "Configuration files validator tool" << endl;
	cout << endl;
	cout << "Usage:" << endl;
	cout << lpszName << " [options]" << endl;
	cout << endl;
	cout << "[-l|--ldap] <file>\tLocation of LDAP plugin configuration file" << endl;
	cout << "[-u|--unix] <file>\tLocation of Unix plugin configuration file" << endl;
	cout << "[-s|--server] <file>\tLocation of zarafa-server configuration file" << endl;
	cout << "[-g|--gateway] <file>\tLocation of zarafa-gateway configuration file" << endl;
	cout << "[-i|--ical] <file>\tLocation of zarafa-ical configuration file" << endl;
	cout << "[-m|--monitor] <file>\tLocation of zarafa-monitor configuration file" << endl;
	cout << "[-p|--spooler] <file>\tLocation of zarafa-spooler configuration file" << endl;
	cout << "[-a|--dagent] <file>\tLocation of zarafa-dagent configuration file" << endl;
	cout << "[-c|--hosted] [y|n]\tForce multi-company/hosted support to \'on\' or \'off\'" << endl;
	cout << "[-d|--distributed] [y|n]\tForce multi-server/distributed support to \'on\' or \'off\'" << endl;
	cout << "[-h|--help]\t\tPrint this help text" << endl;
}

int main(int argc, char* argv[])
{
	list<ECConfigCheck*> check;
	std::string strHosted, strMulti;
	bool bHosted = false;
	bool bMulti = false;

	while (true) {
		char c = my_getopt_long(argc, argv, "l:u:s:g:i:m:p:a:c:d:h", long_options, NULL);
		if (c == -1)
			break;

		switch (c) {
		case 'l':
			check.push_back(new LDAPConfigCheck(my_optarg));
			break;
		case 'u':
			check.push_back(new UnixConfigCheck(my_optarg));
			break;
		case 's':
			check.push_back(new ServerConfigCheck(my_optarg));
			/* Check if hosted is enabled, make sure we don't overwrite commandline */
			if (strHosted.empty())
				strHosted = (*check.rbegin())->getSetting("enable_hosted_zarafa");
			break;
		case 'g':
			check.push_back(new GatewayConfigCheck(my_optarg));
			break;
		case 'i':
			check.push_back(new IcalConfigCheck(my_optarg));
			break;
		case 'm':
			check.push_back(new MonitorConfigCheck(my_optarg));
			break;
		case 'p':
			check.push_back(new SpoolerConfigCheck(my_optarg));
			break;
		case 'a':
			check.push_back(new DAgentConfigCheck(my_optarg));
			break;
		case 'c':
			strHosted = my_optarg;
			break;
		case 'd':
			strMulti = my_optarg;
			break;
		case 'h':
		default:
			print_help(argv[0]);
			return 0;
		}
	}

	if (check.empty()) {
		print_help(argv[0]);
		return 1;
	}

	bHosted = (strHosted[0] == 'y' || strHosted[0] == 'Y' ||
			   strHosted[0] == 't' || strHosted[0] == 'T' ||
			   strHosted[0] == '1');
	bMulti = (strMulti[0] == 'y' || strMulti[0] == 'Y' ||
			   strMulti[0] == 't' || strMulti[0] == 'T' ||
			   strMulti[0] == '1');

	for (list<ECConfigCheck*>::iterator it = check.begin(); it != check.end(); it++) {
		if ((*it)->isDirty())
			continue;

		(*it)->setHosted(bHosted);
		(*it)->setMulti(bMulti);
		(*it)->loadChecks();
		(*it)->validate();

		/* We are only looping through the list once, just cleanup
		 * and don't care about leaving broken pointers in the list. */
		delete (*it);
	}

	return 0;
}

