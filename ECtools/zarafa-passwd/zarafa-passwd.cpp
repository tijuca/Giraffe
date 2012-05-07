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
#include "my_getopt.h"
#include "charset/convert.h"

#include <math.h>
#include <mapidefs.h>
#include <mapispi.h>
#include <mapix.h>
#include <mapiutil.h>

#include "IECServiceAdmin.h"
#include "IECUnknown.h"

#include "ECTags.h"
#include "ECGuid.h"
#include "CommonUtil.h"
#include "ecversion.h"

using namespace std;

enum modes {
	MODE_INVALID = 0, MODE_CHANGE_PASSWD, MODE_HELP
};

enum {
	OPT_HELP = 129,  // high to avoid clashes with modes
	OPT_HOST
};

struct option long_options[] = {
		{ "help", 0, NULL, OPT_HELP },
		{ "host", 1, NULL, OPT_HOST }
};

void print_help(char *name) {
	cout << "Usage:" << endl;
	cout << name << " [action] [options]" << endl << endl;
	cout << "Actions: [-u] " << endl;
	cout << "\t" << " -u user" << "\t" << "update user password, -p or -P" << endl;
	cout << endl;
	cout << "Options: [-u username] [-p password] [-o oldpassword] [-h path]" << endl;
	cout << "\t" << " -o oldpass" << "\t\t" << "old password to login" << endl;
	cout << "\t" << " -p pass" << "\t\t" << "set password to pass" << endl;
	cout << endl;
	cout << "Global options: [-h|--host path]" << endl;
	cout << "\t" << " -h path" << "\t\t" << "connect through <path>, e.g. file:///var/run/socket" << endl;
	cout << "\t" << " -V Print version info." << endl;
	cout << "\t" << " --help" << "\t\t" << "show this help text." << endl;
	cout << endl;
}

int parse_yesno(char *opt) {
	if (opt[0] == 'y' || opt[0] == '1')
		return 1;
	return 0;
}

HRESULT UpdatePassword(char* lpPath, char* lpUsername, char* lpPassword, char* lpNewPassword)
{
	HRESULT hr = hrSuccess;
	LPMAPISESSION lpSession = NULL;
	
	IECUnknown *lpECMsgStore = NULL;
	IMsgStore *lpMsgStore = NULL;

	IECServiceAdmin *lpServiceAdmin = NULL;
	ULONG cbUserId = 0;
	LPENTRYID lpUserId = NULL;
	LPSPropValue lpPropValue = NULL;
	
	ECUSER sECUser;
	convert_context converter;
	std::wstring strwUsername, strwPassword;

	strwUsername = converter.convert_to<wstring>(lpUsername);
	strwPassword = converter.convert_to<wstring>(lpPassword);

	hr = HrOpenECSession(&lpSession, strwUsername.c_str(), strwPassword.c_str(), lpPath, EC_PROFILE_FLAGS_NO_NOTIFICATIONS | EC_PROFILE_FLAGS_NO_PUBLIC_STORE, NULL, NULL);
	if(hr != hrSuccess) {
		cerr << "Wrong username or password." << endl;
		goto exit;
	}

	hr = HrOpenDefaultStore(lpSession, &lpMsgStore);
	if(hr != hrSuccess) {
		cerr << "Unable to open store." << endl;
		goto exit;
	}

	hr = HrGetOneProp(lpMsgStore, PR_EC_OBJECT, &lpPropValue);
	if(hr != hrSuccess || !lpPropValue)
		goto exit;

	lpECMsgStore = (IECUnknown *)lpPropValue->Value.lpszA;
	if(!lpECMsgStore)
		goto exit;

	lpECMsgStore->AddRef();

	MAPIFreeBuffer(lpPropValue); lpPropValue = NULL;

	hr = lpECMsgStore->QueryInterface(IID_IECServiceAdmin, (void**)&lpServiceAdmin);
	if(hr != hrSuccess)
		goto exit;

	hr = lpServiceAdmin->ResolveUserName((LPTSTR)lpUsername, 0, &cbUserId, &lpUserId);
	if (hr != hrSuccess) {
		cerr << "Unable to update password, user not found." << endl;
		goto exit;
	}
		
	memset(&sECUser, 0, sizeof(ECUSER));
	sECUser.sUserId.cb = cbUserId;
	sECUser.sUserId.lpb = (unsigned char*)lpUserId;
	sECUser.ulIsAdmin = (unsigned int)-1;
	sECUser.ulObjClass = (objectclass_t)-1;
	sECUser.lpszPassword = (LPTSTR)lpNewPassword;

	hr = lpServiceAdmin->SetUser(&sECUser, 0);
	if(hr != hrSuccess) {
		cerr << "Unable to update user password." << endl;
		goto exit;
	}

exit:
	if (lpUserId)
		MAPIFreeBuffer(lpUserId);

	if (lpPropValue)
		MAPIFreeBuffer(lpPropValue);
	
	if (lpMsgStore)
		lpMsgStore->Release();

	if(lpECMsgStore)
		lpECMsgStore->Release();

	if (lpServiceAdmin)
		lpServiceAdmin->Release();

	if (lpSession)
		lpSession->Release();

	return hr;
}

int main(int argc, char* argv[])
{
	HRESULT hr = hrSuccess;
	char*	username = NULL;
	char*	newpassword = NULL;
	char	szOldPassword[80];
	char	szNewPassword[80];
	char*	oldpassword = NULL;
	char*	repassword = NULL;
	char*	path = NULL;
	modes	mode = MODE_INVALID;
	int		passprompt = 1;

	setlocale(LC_MESSAGES, "");
	setlocale(LC_CTYPE, "");

	if(argc < 2) {
		print_help(argv[0]);
		return 1;
	}

	int c;
	while (1) {
		c = my_getopt_long(argc, argv, "u:Pp:h:o:V", long_options, NULL);
		if (c == -1)
			break;
		switch (c) {
		case 'u':
			mode = MODE_CHANGE_PASSWD;
			username = my_optarg;
			break;
		case 'p':
			newpassword = my_optarg;
			passprompt = 0;
			break;
		case 'o':
			oldpassword = my_optarg;
			passprompt = 0;
			break;
			// error handling?
		case '?':
			break;
		case OPT_HOST:
		case 'h':
			path = my_optarg;
			break;
		case 'V':
			cout << "Product version:\t" <<  PROJECT_VERSION_PASSWD_STR << endl
				 << "File version:\t\t" << PROJECT_SVN_REV_STR << endl;
			return 1;			
		case OPT_HELP:
			mode = MODE_HELP;
			break;
		default:
			break;
		};
	}

	// check parameters
	if (my_optind < argc) {
		cerr << "Too many options given." << endl;
		return 1;
	}

	if (mode == MODE_INVALID) {
		cerr << "No correct command given." << endl;
		return 1;
	}

	if (mode == MODE_HELP) {
		print_help(argv[0]);
		return 0;
	}

	if (mode == MODE_CHANGE_PASSWD && ((newpassword == NULL && passprompt == 0) ||
		username == NULL || (oldpassword == NULL && passprompt == 0)) ) {
		cerr << "Missing information to update user password." << endl;
		return 1;
	}

	//Init mapi
	hr = MAPIInitialize(NULL);
	if (hr != hrSuccess) {
		cerr << "Unable to initialize" << endl;
		goto exit;
	}

	
	// fully logged on, action!

	switch(mode) {
	case MODE_CHANGE_PASSWD:
		
		if(passprompt)
		{
			oldpassword = get_password("Enter old password:");
			if(oldpassword == NULL)
			{
				cerr << "Wrong old password" << endl;
				goto exit;
			}
			
			cout << endl;

			strcpy(szOldPassword, oldpassword);

			newpassword = get_password("Enter new password:");
			if(oldpassword == NULL)
			{
				cerr << "Wrong new password" << endl;
				goto exit;
			}

			cout << endl;
			
			strcpy(szNewPassword, newpassword);

			repassword = get_password("Re-Enter password:");
			if(strcmp(newpassword, repassword) != 0) {
				cerr << "Passwords don't match" << endl;
				
			}
			cout << endl;

			oldpassword = szOldPassword;
			newpassword = szNewPassword;
		}

		hr = UpdatePassword(path, username, oldpassword, newpassword);
		if (hr != hrSuccess)
			goto exit;		

	case MODE_INVALID:
	case MODE_HELP:
		// happy compiler
		break;
	};

exit:

	MAPIUninitialize();
	if (hr == hrSuccess)
		return 0;
	else
		return 1;
}

