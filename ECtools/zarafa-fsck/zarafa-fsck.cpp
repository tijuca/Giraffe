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

#include <platform.h>

#include <iostream>
#include <map>

#include <CommonUtil.h>
#include <mapiext.h>
#include <mapiguidext.h>
#include <mapiutil.h>
#include <mapix.h>
#include <my_getopt.h>
#include <namedprops.h>
#include <stringutil.h>
#include <edkmdb.h>
#include <edkguid.h>
#include "charset/convert.h"
#include "charset/localeutil.h"

#include "zarafa-fsck.h"

using namespace std;

string auto_fix;
string auto_del;

/*
 * Some typedefs to make typing easier. ;)
 */
typedef pair<string, ZarafaFsck* > CHECKMAP_P;
typedef map<string, ZarafaFsck* > CHECKMAP;
typedef map<string, ZarafaFsck* >::iterator CHECKMAP_I;

enum {
	OPT_HELP = 129,
	OPT_HOST,
	OPT_PASS,
	OPT_USER,
	OPT_PUBLIC,
	OPT_CALENDAR,
	//OPT_STICKY,
	//OPT_EMAIL,
	OPT_CONTACT,
	OPT_TASK,
	//OPT_JOURNAL,
	OPT_AUTO_FIX,
	OPT_AUTO_DEL,
	OPT_CHECK_ONLY,
	OPT_PROMPT,
	OPT_ALL
};

struct option long_options[] = {
	{ "help",	0, NULL, OPT_HELP },
	{ "host",	1, NULL, OPT_HOST },
	{ "pass",	1, NULL, OPT_PASS },
	{ "user",	1, NULL, OPT_USER },
	{ "public",	0, NULL, OPT_PUBLIC },
	{ "calendar",	0, NULL, OPT_CALENDAR },
	//{ "sticky",	0, NULL, OPT_STICKY }
	//{ "email",	0, NULL, OPT_EMAIL },
	{ "contact",	0, NULL, OPT_CONTACT },
	{ "task",	0, NULL, OPT_TASK },
	//{ "journal",	0, NULL, OPT_JOURNAL },
	{ "all",	0, NULL, OPT_ALL },
	{ "autofix",	1, NULL, OPT_AUTO_FIX },
	{ "autodel",	1, NULL, OPT_AUTO_DEL },
	{ "checkonly",	0, NULL, OPT_CHECK_ONLY },
	{ "prompt",		0, NULL, OPT_PROMPT },
	{}
};

void print_help(char *strName)
{
	cout << "Calendar item validator tool" << endl;
	cout << endl;
	cout << "Usage:" << endl;
	cout << strName << " [options] [filters]" << endl;
	cout << endl;
	cout << "Options:" << endl;
	cout << "[-h|--host] <hostname>\tZarafa server" << endl;
	cout << "[-u|--user] <username>\tUsername used to login" << endl;
	cout << "[-p|--pass] <password>\tPassword used to login" << endl;
	cout << "[-P|--prompt]\t\tPrompt for password to login" << endl;
	cout << "[--public]\t\tCheck the public store for the user" << endl;
        cout << "[--autofix] <yes/no>\tAutoreply to all \"fix property\" messages" << endl;
        cout << "[--autodel] <yes/no>\tAutoreply to all \"delete item\" messages" << endl;
	cout << "[--checkonly]\t\tImplies \"--autofix no\" and \"--autodel no\"" << endl;
	cout << "[--help]\t\tPrint this help screen" << endl;
	cout << endl;
	cout << "Filters:" << endl;
	cout << "[--calendar]\tCheck all Calendar folders" << endl;
	//cout << "[--sticky]\tCheck all Sticky folders" << endl;
	//cout << "[--email]\tCheck all Email folders" << endl;
	cout << "[--contact]\tCheck all Contact folders" << endl;
	cout << "[--task]\tCheck all Task folders" << endl;
	//cout << "[--journal]\tCheck all Journal folders" << endl;
	cout << "[--all]\tCheck all folders" << endl;
}

void disclaimer()
{
	std::string dummy;

	cout << "*********" << endl;
	cout << " WARNING" << endl;
	cout << "*********" << endl;
	cout << "Although this program tries to fix broken items in your calender" << endl;
	cout << "so Microsoft Outlook will show your calender correctly again," << endl;
	cout << "it may fail doing so, and possibly make things worse." << endl;
	cout << "Unfixable items should be deleted from the calender to make it" << endl;
	cout << "correctly viewable in both Microsoft Outlook and Zarafa Webaccess again." << endl;
	cout << endl;
	cout << "Zarafa cannot be held responsible for loss of data," << endl;
	cout << "or any other damage this program may cause." << endl;
	cout << "To accept these terms, press <ENTER> to continue or <CTRL-C> to quit." << endl;

	getline(cin,dummy);
}

HRESULT allocNamedIdList(ULONG ulSize, LPMAPINAMEID **lpppNameArray)
{
	HRESULT hr = hrSuccess;
	LPMAPINAMEID *lppArray = NULL;
	LPMAPINAMEID lpBuffer = NULL;

	hr = MAPIAllocateBuffer(ulSize * sizeof(LPMAPINAMEID), (void**)&lppArray);
	if (hr != hrSuccess)
		goto exit;

	hr = MAPIAllocateMore(ulSize * sizeof(MAPINAMEID), lppArray, (void**)&lpBuffer);
	if (hr != hrSuccess) {
		MAPIFreeBuffer(lppArray);
		goto exit;
	}

	for (ULONG i = 0; i < ulSize; i++)
		lppArray[i] = &lpBuffer[i];

	*lpppNameArray = lppArray;

exit:
	return hr;
}

void freeNamedIdList(LPMAPINAMEID *lppNameArray)
{
	MAPIFreeBuffer(lppNameArray);
}

HRESULT ReadProperties(LPMESSAGE lpMessage, ULONG ulCount, ULONG *lpTag, LPSPropValue *lppPropertyArray)
{
	HRESULT hr = hrSuccess;
	LPSPropTagArray lpPropertyTagArray = NULL;
	ULONG ulPropertyCount = 0;

	hr = MAPIAllocateBuffer(sizeof(SPropTagArray) + (sizeof(ULONG) * ulCount), (void**)&lpPropertyTagArray);
	if (hr != hrSuccess)
		goto exit;

	lpPropertyTagArray->cValues = ulCount;
	for (ULONG i = 0; i < ulCount; i++)
		lpPropertyTagArray->aulPropTag[i] = lpTag[i];

	hr = lpMessage->GetProps(lpPropertyTagArray, 0, &ulPropertyCount, lppPropertyArray);
	if (FAILED(hr)) {
		cout << "Failed to obtain all properties." << endl;
		goto exit;
	}

exit:
	if (lpPropertyTagArray)
		MAPIFreeBuffer(lpPropertyTagArray);

	return hr;
}

HRESULT ReadNamedProperties(LPMESSAGE lpMessage, ULONG ulCount, LPMAPINAMEID *lppTag,
			    LPSPropTagArray *lppPropertyTagArray, LPSPropValue *lppPropertyArray)
{
	HRESULT hr = hrSuccess;
	ULONG ulReadCount = 0;

	hr = lpMessage->GetIDsFromNames(ulCount, lppTag, 0, lppPropertyTagArray);
	if(hr != hrSuccess) {
		cout << "Failed to obtain IDs from names." << endl;
		/*
		 * Override status to make sure FAILED() will catch this,
		 * this is required to make sure the called won't attempt
		 * to access lppPropertyArray.
		 */
		hr = MAPI_E_CALL_FAILED;
		goto exit;
	}

	hr = lpMessage->GetProps(*lppPropertyTagArray, 0, &ulReadCount, lppPropertyArray);
	if (FAILED(hr)) {
		cout << "Failed to obtain all properties." << endl;
		goto exit;
	}

exit:
	return hr;
}

HRESULT DetectFolderDetails(LPMAPIFOLDER lpFolder, string *lpName, string *lpClass, ULONG *lpFolderType)
{
	HRESULT hr = hrSuccess;
	LPSPropValue lpPropertyArray = NULL;
	ULONG ulPropertyCount = 0;

	SizedSPropTagArray(3, PropertyTagArray) = {
		3,
		{
			PR_DISPLAY_NAME_A,
			PR_CONTAINER_CLASS_A,
			PR_FOLDER_TYPE,
		}
	};

	hr = lpFolder->GetProps((LPSPropTagArray)&PropertyTagArray,
			      0,
			      &ulPropertyCount,
			      &lpPropertyArray);
	if (FAILED(hr)) {
		cout << "Failed to obtain all properties." << endl;
		goto exit;
	}

	*lpFolderType = 0;

	for (ULONG i = 0; i < ulPropertyCount; i++) {
		if (PROP_TYPE(lpPropertyArray[i].ulPropTag) == PT_ERROR)
			hr = MAPI_E_INVALID_OBJECT;
		else if (lpPropertyArray[i].ulPropTag == PR_DISPLAY_NAME_A)
			*lpName = lpPropertyArray[i].Value.lpszA;
		else if (lpPropertyArray[i].ulPropTag == PR_CONTAINER_CLASS_A)
			*lpClass = lpPropertyArray[i].Value.lpszA;
		else if (lpPropertyArray[i].ulPropTag == PR_FOLDER_TYPE)
			*lpFolderType = lpPropertyArray[i].Value.ul;
	}

	/*
	 * As long as we found what we were looking for, we should be satisfied.
	 */
	if (!lpName->empty() && !lpClass->empty())
		hr = hrSuccess;

exit:
	if (lpPropertyArray)
		MAPIFreeBuffer(lpPropertyArray);

	return hr;
}

HRESULT RunFolderValidation(LPMAPIFOLDER lpRootFolder, LPSRow lpRow, CHECKMAP checkmap)
{
	HRESULT hr = hrSuccess;
	LPSPropValue lpItemProperty = NULL;
	LPMAPIFOLDER lpFolder = NULL;
	ZarafaFsck *lpFsck = NULL;
	ULONG ulObjectType = 0;
	string strName;
	string strClass;
	ULONG ulFolderType = 0;

	lpItemProperty = PpropFindProp(lpRow->lpProps, lpRow->cValues, PR_ENTRYID);
	if (!lpItemProperty) {
		cout << "Row does not contain an EntryID." << endl;
		goto exit;
	}

	hr = lpRootFolder->OpenEntry(lpItemProperty->Value.bin.cb,
				   (LPENTRYID)lpItemProperty->Value.bin.lpb,
				   &IID_IMAPIFolder,
				   0,
				   &ulObjectType,
				   (IUnknown**)&lpFolder);
	if (hr != hrSuccess) {
		cout << "Failed to open EntryID." << endl;
		goto exit;
	}

	/*
	 * Detect folder name and class.
	 */
	hr = DetectFolderDetails(lpFolder, &strName, &strClass, &ulFolderType);
	if (hr != hrSuccess) {
		if (!strName.empty()) {
			cout << "Unknown class, skipping entry";
			cout << " \"" << strName << "\"" << endl;
		} else
			cout << "Failed to detect folder details." << endl;
		goto exit;
	}

	if (ulFolderType != FOLDER_GENERIC) {
		cout << "Ignoring search folder: ";
		cout << "\"" << strName << "\" (" << strClass << ")" << endl;
		goto exit;
	}

	for (CHECKMAP_I i = checkmap.begin(); i != checkmap.end(); ++i) {
		if (i->first == strClass) {
			lpFsck = i->second;
			break;
		}
	}

	if (lpFsck)
		lpFsck->ValidateFolder(lpFolder, strName);
	else {
		cout << "Ignoring folder: ";
		cout << "\"" << strName << "\" (" << strClass << ")" << endl;
	}

exit:
	if(lpFolder)
		lpFolder->Release();

	return hr;
}

HRESULT RunStoreValidation(char* strHost, char* strUser, char* strPass, char *strAltUser, bool bPublic, CHECKMAP checkmap)
{
	HRESULT hr = hrSuccess;
	LPMAPISESSION lpSession = NULL;
	LPMDB lpStore = NULL;
	LPMDB lpAltStore = NULL;
	LPMDB lpReadStore = NULL;
	LPMAPIFOLDER lpRootFolder = NULL;
	LPMAPITABLE lpHierarchyTable = NULL;
	LPSRowSet lpRows = NULL;
	ULONG ulObjectType;
	ULONG ulCount;
    LPEXCHANGEMANAGESTORE lpIEMS = NULL;
    // user
    ULONG			cbUserStoreEntryID = 0;
    LPENTRYID		lpUserStoreEntryID = NULL;
	wstring strwUsername;
	wstring strwAltUsername;
	wstring strwPassword;

	hr = MAPIInitialize(NULL);
	if (hr != hrSuccess) {
		cout << "Unable to initialize session" << endl;
		goto exit;
	}

	// input from commandline is current locale
	if (strUser)
		strwUsername = convert_to<wstring>(strUser);
	if (strPass)
		strwPassword = convert_to<wstring>(strPass);
	if (strAltUser)
		strwAltUsername = convert_to<wstring>(strAltUser);

	hr = HrOpenECSession(&lpSession, strwUsername.c_str(), strwPassword.c_str(), (const char *)strHost, 0, NULL, NULL);
	if(hr != hrSuccess) {
		cout << "Wrong username or password." << endl;
		goto exit;
	}
	
	if (bPublic) {
		hr = HrOpenECPublicStore(lpSession, &lpStore);
		if (hr != hrSuccess) {
			cout << "Failed to open public store." << endl;
			goto exit;
		}
	} else {
		hr = HrOpenDefaultStore(lpSession, &lpStore);
		if (hr != hrSuccess) {
			cout << "Failed to open default store." << endl;
			goto exit;
		}
	}

	if (!strwAltUsername.empty()) {
        hr = lpStore->QueryInterface(IID_IExchangeManageStore, (void **)&lpIEMS);
        if (hr != hrSuccess) {
            cout << "Cannot open ExchangeManageStore object" << endl;
            goto exit;
        }

        hr = lpIEMS->CreateStoreEntryID(L"", (LPTSTR)strwAltUsername.c_str(), MAPI_UNICODE | OPENSTORE_HOME_LOGON, &cbUserStoreEntryID, &lpUserStoreEntryID);
        if (hr != hrSuccess) {
            cout << "Cannot get user store id for user" << endl;
            goto exit;
        }

        hr = lpSession->OpenMsgStore(0, cbUserStoreEntryID, lpUserStoreEntryID, NULL, MDB_WRITE | MDB_NO_DIALOG | MDB_NO_MAIL | MDB_TEMPORARY, &lpAltStore);
        if (hr != hrSuccess) {
            cout << "Cannot open user store of user" << endl;
            goto exit;
        }
        
        lpReadStore = lpAltStore;
	} else {
	    lpReadStore = lpStore;
    }

	hr = lpReadStore->OpenEntry(0, NULL, &IID_IMAPIFolder, 0, &ulObjectType, (IUnknown **)&lpRootFolder);
	if(hr != hrSuccess) {
		cout << "Failed to open root folder." << endl;
		goto exit;
	}

	hr = lpRootFolder->GetHierarchyTable(CONVENIENT_DEPTH, &lpHierarchyTable);
	if (hr != hrSuccess) {
		cout << "Failed to open hierarchy." << endl;
		goto exit;
	}

	/*
	 * Check if we have found at least *something*.
	 */
	hr = lpHierarchyTable->GetRowCount(0, &ulCount);
	if(hr != hrSuccess) {
		cout << "Failed to count number of rows." << endl;
		goto exit;
	} else if (!ulCount) {
		cout << "No entries inside Calendar." << endl;
		goto exit;
	}

	/*
	 * Loop through each row/entry and validate.
	 */
	while (true) {
		hr = lpHierarchyTable->QueryRows(20, 0, &lpRows);
		if (hr != hrSuccess)
			break;

		if (lpRows->cRows == 0)
			break;

		for (ULONG i = 0; i < lpRows->cRows; i++)
			RunFolderValidation(lpRootFolder, &lpRows->aRow[i], checkmap);

		if (lpRows) {
			FreeProws(lpRows);
			lpRows = NULL;
		}
	}

exit:
    if (lpUserStoreEntryID)
        MAPIFreeBuffer(lpUserStoreEntryID);

    if (lpIEMS)
        lpIEMS->Release();
        
	if (lpRows) {
		FreeProws(lpRows);
		lpRows = NULL;
	}

	if(lpHierarchyTable)
		lpHierarchyTable->Release();

	if (lpRootFolder)
		lpRootFolder->Release();

    if (lpAltStore)
        lpAltStore->Release();

	if (lpStore)
		lpStore->Release();

	if (lpSession)
		lpSession->Release();

	MAPIUninitialize();

	return hr;
}

int main(int argc, char *argv[])
{
	HRESULT hr = hrSuccess;
	CHECKMAP checkmap;
	char* strHost = NULL;
	char* strUser = NULL;
	char* strPass = "";
	char* strAltUser = NULL;
	int c;
	bool bAll = false;
	bool bPrompt = false;
	bool bPublic = false;

	setlocale(LC_MESSAGES, "");
	if (!forceUTF8Locale(true))
		return -1;

	strHost = GetServerUnixSocket();

	/*
	 * Read arguments.
	 */
	while (true) {
		c = my_getopt_long(argc, argv, "u:p:h:a:P", long_options, NULL);
		if (c == -1)
			break;

		switch (c) {
		case 'a':
		    strAltUser = my_optarg;
		    break;
		case OPT_USER:
		case 'u':
			strUser = my_optarg;
			break;
		case OPT_PASS:
		case 'p':
			strPass = my_optarg;
			break;
        case OPT_PROMPT:
        case 'P':
            bPrompt = true;
            break;
		case OPT_PUBLIC:
			bPublic = true;
			break;
		case OPT_HOST:
		case 'h':
			strHost = my_optarg;
			break;
		case OPT_HELP:
			print_help(argv[0]);
			return 0;
		case OPT_CALENDAR:
			checkmap.insert(CHECKMAP_P("IPF.Appointment", new ZarafaFsckCalendar()));
			break;
		//case OPT_STICKY:
		//	checkmap.insert(CHECKMAP_P("IPF.StickyNote", new ZarafaFsckStickyNote()));
		//	break;
		//case OPT_EMAIL:
		//	checkmap.insert(CHECKMAP_P("IPF.Note", new ZarafaFsckNote()));
		//	break;
		case OPT_CONTACT:
			checkmap.insert(CHECKMAP_P("IPF.Contact", new ZarafaFsckContact()));
			break;
		case OPT_TASK:
			checkmap.insert(CHECKMAP_P("IPF.Task", new ZarafaFsckTask()));
			break;
		//case OPT_JOURNAL:
		//	checkmap.insert(CHECKMAP_P("IPF.Journal", new ZarafaFsckJournal()));
		//	break;
		case OPT_ALL:
			bAll = true;
			break;
		case OPT_AUTO_FIX:
			auto_fix = my_optarg;
			break;
		case OPT_AUTO_DEL:
			auto_del = my_optarg;
			break;
		case OPT_CHECK_ONLY:
			auto_fix = "no";
			auto_del = "no";
			break;
		default:
			cout << "Invalid argument" << endl;
			print_help(argv[0]);
			return 1;
		};
	}

	disclaimer();

	/*
	 * Validate arguments.
	 */
	if (!strHost || !strUser) {
		cout << "Arguments missing: " << (!strHost ? "Host" : "User") << endl;
		print_help(argv[0]);
		return 1;
	}
	
	if (bPrompt) {
		strPass = get_password("Enter password:");
		if(!strPass) {
			cout << "Invalid password." << endl;
			return 1;
		}
	}

	if (checkmap.empty()) {
		if (!bAll)
			cout << "Filter arguments missing, defaulting to --all" << endl;
		checkmap.insert(CHECKMAP_P("IPF.Appointment", new ZarafaFsckCalendar()));
		//checkmap.insert(CHECKMAP_P("IPF.StickyNote", new ZarafaFsckStickyNote()));
		//checkmap.insert(CHECKMAP_P("IPF.Note", new ZarafaFsckNote()));
		checkmap.insert(CHECKMAP_P("IPF.Contact", new ZarafaFsckContact()));
		checkmap.insert(CHECKMAP_P("IPF.Task", new ZarafaFsckTask()));
		//checkmap.insert(CHECKMAP_P("IPF.Journal", new ZarafaFsckJournal()));
	}

	hr = RunStoreValidation(strHost, strUser, strPass, strAltUser, bPublic, checkmap);

	/*
	 * Cleanup
	 */
	if (hr == hrSuccess)
		cout << endl << "Statistics:" << endl;

	for (CHECKMAP_I i = checkmap.begin(); i != checkmap.end(); i = checkmap.begin()) {
		ZarafaFsck *lpFsck = i->second;
		
		if (hr == hrSuccess)
			lpFsck->PrintStatistics(i->first);
		
		checkmap.erase(i);
		delete lpFsck;
	}

	return (hr == hrSuccess);
}
