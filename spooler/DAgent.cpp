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

/*
 * This is the Zarafa delivery Agent.
 *
 * E-mail is delivered to the client through this program; it is invoked
 * by the MTA with the username and rfc822 e-mail message, and the delivery
 * agent parses the rfc822 message, setting properties on the MAPI object
 * as it goes along.
 *
 * The delivery agent should be called with sufficient privileges to be 
 * able to open other users' inboxes.
 *
 * The actual decoding is done by the inetmapi library.
 */
/*
 * An LMTP reply contains:
 * <SMTP code> <ESMTP code> <message>
 * See RFC 1123 and 2821 for normal codes, 1893 and 2034 for enhanced codes.
 *
 * Enhanced Delivery status codes: Class.Subject.Detail
 *
 * Classes:
 * 2 = Success, 4 = Persistent Transient Failure, 5 = Permanent Failure
 *
 * Subjects:
 * 0 = Other/Unknown, 1 = Addressing, 2 = Mailbox, 3 = Mail system
 * 4 = Network/Routing, 5 = Mail delivery, 6 = Message content, 7 = Security
 *
 * Detail:
 * see rfc.
 */
#include "platform.h"
#include <stdio.h>
#include <stdlib.h>

#include <iostream>
#include <algorithm>
#include <map>

#include "mapi_ptr.h"
#include "fileutil.h"
#include "PyMapiPlugin.h"

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#include <sys/mman.h>
#include <pwd.h>

/*
  This is actually from sysexits.h
  but since those windows lamers don't have it ..
  let's copy some defines here..
*/
#define EX_OK		0	/* successful termination */
#define EX__BASE	64	/* base value for error messages */
#define EX_USAGE	64	/* command line usage error */
#define EX_SOFTWARE	70	/* internal software error */
#define EX_TEMPFAIL	75	/* temp failure; user is invited to retry */

#define USES_IID_IMAPIFolder
#define USES_IID_IExchangeManageStore
#define USES_IID_IMsgStore
#include <ECGuid.h>

#include <inetmapi.h>

#include <mapi.h>
#include <mapix.h>
#include <mapiutil.h>
#include <mapidefs.h>
#include <mapiext.h>
#include <mapiguid.h>
#include "edkguid.h"
#include "edkmdb.h"
#include "EMSAbTag.h"

#include <ctype.h>
#include <time.h>

#include "stringutil.h"
#include "CommonUtil.h"
#include "Util.h"
#include "ECLogger.h"
#include "my_getopt.h"
#include "restrictionutil.h"
#include "rules.h"
#include "archive.h"
#include "helpers/mapiprophelper.h"
#include "options.h"			// inetmapi/options.h
#include "charset/convert.h"
#include "base64.h"

#include "IECServiceAdmin.h"
#include "IECUnknown.h"
#include "ECTags.h"
#include "ECFeatures.h"

#include "ECChannel.h"
#include "UnixUtil.h"
#include "LMTP.h"
#include "ecversion.h"
#include "platform.h"
#include <signal.h>
#include "SSLUtil.h"

#include <execinfo.h>
    
using namespace std;

enum _dt {
	DM_STORE=0,
	DM_JUNK,
	DM_PUBLIC
};
typedef _dt delivery_mode;

class DeliveryArgs {
public:
	DeliveryArgs()
	{
		lpChannel = NULL;
	}

	~DeliveryArgs()
	{
		if (lpChannel)
			delete lpChannel;
	}

	/* Channel for communication from MTA */
	ECChannel *lpChannel;

	/* Connection path to Zarafa server */
	std::string strPath;

	/* Path to autorespond handler */
	std::string strAutorespond;

	/* Options for delivery into special subfolder */
	bool bCreateFolder;
	std::wstring strDeliveryFolder;
	WCHAR szPathSeperator;

	/* Delivery options */
	delivery_mode ulDeliveryMode;
	delivery_options sDeliveryOpts;

	/* Generate notifications regarding the new email */
	bool bNewmailNotify;

	/* Username is email address, resolve it to get username */
	bool bResolveAddress;
};

/**
 * ECRecipient contains an email address from LMTP, or from
 * commandline an email address or username.
 */
class ECRecipient {
public:
	ECRecipient(std::wstring wstrName)
	{
		bResolved = false;

		/* strRCPT much match recipient string from LMTP caller */
		wstrRCPT = wstrName;
		vwstrRecipients.push_back(wstrName);

		ulDisplayType = 0;
		ulAdminLevel = 0;
		bHasIMAP = false;

		sEntryId.cb = 0;
		sEntryId.lpb = NULL;

		sSearchKey.cb = 0;
		sSearchKey.lpb = NULL;
	}

	~ECRecipient()
	{
		if (sEntryId.lpb)
			MAPIFreeBuffer(sEntryId.lpb);
		if (sSearchKey.lpb)
			MAPIFreeBuffer(sSearchKey.lpb);
	}

	void combine(ECRecipient *lpRecip) {
		vwstrRecipients.push_back(lpRecip->wstrRCPT);
	}

	// sort recipients on imap data flag, then on username so find() for combine() works correctly.
	bool operator <(const ECRecipient &r) const {
		if (this->bHasIMAP == r.bHasIMAP)
			return this->wstrUsername < r.wstrUsername;
		else
			return this->bHasIMAP && !r.bHasIMAP;
	}

	BOOL bResolved;

	/* Information from LMTP caller */
	std::wstring wstrRCPT;
	std::vector<std::wstring> vwstrRecipients;

	/* User properties */
	std::wstring wstrUsername;
	std::wstring wstrFullname;
	std::wstring wstrCompany;
	std::wstring wstrEmail;
	std::wstring wstrServerDisplayName;
	std::wstring wstrDeliveryStatus;
	ULONG ulDisplayType;
	ULONG ulAdminLevel;
	std::string strAddrType;
	std::string strSMTP;
	SBinary sEntryId;
	SBinary sSearchKey;
	bool bHasIMAP;
};

HRESULT GetPluginObject(ECLogger *lpLogger, PyMapiPluginFactory *lpPyMapiPluginFactory, PyMapiPlugin **lppPyMapiPlugin)
{
	HRESULT hr = hrSuccess;
	PyMapiPlugin *lpPyMapiPlugin = NULL;

	if (!lpLogger || !lpPyMapiPluginFactory || !lppPyMapiPlugin) {
		ASSERT(FALSE);
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	if (lpPyMapiPluginFactory->CreatePlugin("DAgentPluginManager", &lpPyMapiPlugin) != hrSuccess) {
		lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to initialize the dagent plugin manager, please check your configuration.");
		hr = MAPI_E_CALL_FAILED;
		goto exit;
	}

	*lppPyMapiPlugin = lpPyMapiPlugin;
	lpPyMapiPlugin = NULL;

exit:
	if (lpPyMapiPlugin)
		delete lpPyMapiPlugin;

	return hr;
}

//Global variables

bool g_bQuit = false;
bool g_bTempfail = true; // Most errors are tempfails
unsigned int g_nLMTPThreads = 0;
ECLogger *g_lpLogger = NULL;
ECConfig *g_lpConfig = NULL;

class sortRecipients {
public:
	bool operator()(ECRecipient *left, ECRecipient *right) const
	{
		return *left < *right;
	}
};

typedef std::set<ECRecipient *, sortRecipients> recipients_t;
// we group by server to correctly single-instance the email data on each server
typedef std::map<std::wstring, recipients_t, wcscasecmp_comparison> serverrecipients_t;
// then we group by company to minimize re-opening the addressbook
typedef std::map<std::wstring, serverrecipients_t, wcscasecmp_comparison> companyrecipients_t;

void sigterm(int) {
	g_bQuit = true;
}

void sighup(int sig) {

	if (g_lpConfig) {
		if (!g_lpConfig->ReloadSettings() && g_lpLogger)
			g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to reload configuration file, continuing with current settings.");
	}

	if (g_lpLogger) {
		if (g_lpConfig) {
			char *ll = g_lpConfig->GetSetting("log_level");
			int new_ll = ll ? atoi(ll) : 2;
			g_lpLogger->SetLoglevel(new_ll);
		}
		g_lpLogger->Reset();
		g_lpLogger->Log(EC_LOGLEVEL_WARNING, "Log connection was reset");
	}
}

void sigchld(int) {
	int stat;
	while (waitpid (-1, &stat, WNOHANG) > 0) g_nLMTPThreads--;
}

// Look for segmentation faults
void sigsegv(int signr) {
	void *bt[64];
	int i, n;
	char **btsymbols;

	if(!g_lpLogger)
		goto exit;

	switch (signr) {
	case SIGSEGV:
		g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Pid %d caught SIGSEGV (%d), traceback:", getpid(), signr);
		break;
	case SIGBUS:
		g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Pid %d caught SIGBUS (%d), possible invalid mapped memory access, traceback:", getpid(), signr);
		break;
	case SIGABRT:
		g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Pid %d caught SIGABRT (%d), out of memory or unhandled exception, traceback:", getpid(), signr);
		break;
	};

	n = backtrace(bt, 64);

	btsymbols = backtrace_symbols(bt, n);

	for (i = 0; i < n; i++) {
		if (btsymbols)
			g_lpLogger->Log(EC_LOGLEVEL_FATAL, "%p %s", bt[i], btsymbols[i]);
		else
			g_lpLogger->Log(EC_LOGLEVEL_FATAL, "%p", bt[i]);
	}

exit:
	kill(getpid(), signr);
}


/**
 * Check if the message should be processed with the autoaccepter
 *
 * This function returns TRUE if the message passed is a meeting request or meeting cancellation AND
 * the store being delivered in is marked for auto-accepting of meeting requests.
 *
 * @param lpStore Store that the message will be delivered in
 * @param lpMessage Message that will be delivered
 * @return TRUE if the message needs to be autoresponded
 */
bool FNeedsAutoAccept(IMsgStore *lpStore, LPMESSAGE lpMessage)
{
	HRESULT hr = hrSuccess;
	SizedSPropTagArray(2, sptaProps) = { 2, { PR_RESPONSE_REQUESTED, PR_MESSAGE_CLASS } };
	LPSPropValue lpProps = NULL;
	ULONG cValues = 0;
	bool bAutoAccept = false, bDeclineConflict = false, bDeclineRecurring = false;
	
	hr = lpMessage->GetProps((LPSPropTagArray)&sptaProps, 0, &cValues, &lpProps);
	if (FAILED(hr))
		goto exit;

	if (PROP_TYPE(lpProps[1].ulPropTag) == PT_ERROR) {
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}
	
	if (wcscasecmp(lpProps[1].Value.lpszW, L"IPM.Schedule.Meeting.Request") != 0 && wcscasecmp(lpProps[1].Value.lpszW, L"IPM.Schedule.Meeting.Canceled") != 0) {
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}

	if (((PROP_TYPE(lpProps[0].ulPropTag) == PT_ERROR) || !lpProps[0].Value.b) && wcscasecmp(lpProps[1].Value.lpszW, L"IPM.Schedule.Meeting.Request") == 0) {
		// PR_RESPONSE_REQUESTED must be true for requests to start the auto accepter
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}
	
	hr = GetAutoAcceptSettings(lpStore, &bAutoAccept, &bDeclineConflict, &bDeclineRecurring);
	if (hr != hrSuccess)
		goto exit;
		
	if (!bAutoAccept) {
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}
	
exit:
	if(lpProps)
		MAPIFreeBuffer(lpProps);
		
	return hr == hrSuccess;
}

/**
 * Auto-respond to the passed message
 *
 * This function starts the external autoresponder. Since the external autoresponder needs to access a message,
 * we first copy (save) the message into the root folder of the store, then run the script on that message, and remove
 * the message afterwards (if the item was accepted, then it will have been moved to the calendar, which will cause
 * the delete to fail, but that's expected).
 *
 * @param lpLogger Logger
 * @param lpMAPISession MAPI Session for the user in lpRecip
 * @param lpRecip Recipient for whom lpMessage is being delivered
 * @param lpStore Store in which lpMessage is being delivered
 * @param lpMessage Message being delivered, should be a meeting request
 * 
 * @return result
 */
HRESULT HrAutoAccept(ECLogger *lpLogger, IMAPISession *lpMAPISession, ECRecipient *lpRecip, IMsgStore *lpStore, LPMESSAGE lpMessage)
{
	HRESULT hr = hrSuccess;
	IMAPIFolder *lpRootFolder = NULL;
	IMessage *lpMessageCopy = NULL;
	char *autoresponder = g_lpConfig->GetSetting("mr_autoaccepter");
	std::string strEntryID, strCmdLine;
	LPSPropValue lpEntryID = NULL;
	ULONG ulType = 0;
	ENTRYLIST sEntryList;

	// Our autoaccepter is an external script. This means that the message it is working on must be
	// saved so that it can find the message to open. Since we can't save the passed lpMessage (it
	// must be processed by the rules engine first), we make a copy, and let the autoaccept script
	// work on the copy.
	hr = lpStore->OpenEntry(0, NULL, NULL, MAPI_MODIFY, &ulType, (IUnknown **)&lpRootFolder);
	if(hr != hrSuccess)
		goto exit;
	
	hr = lpRootFolder->CreateMessage(NULL, 0, &lpMessageCopy);
	if(hr != hrSuccess)
		goto exit;
		
	hr = lpMessage->CopyTo(0, NULL, NULL, 0, NULL, &IID_IMessage, (LPVOID)lpMessageCopy, 0, NULL);
	if(hr != hrSuccess)
		goto exit;
		
	hr = lpMessageCopy->SaveChanges(0);
	if(hr != hrSuccess)
		goto exit;

	hr = HrGetOneProp(lpMessageCopy, PR_ENTRYID, &lpEntryID);
	if (hr != hrSuccess)
		goto exit;
		
	strEntryID = bin2hex(lpEntryID->Value.bin.cb, lpEntryID->Value.bin.lpb);

	// We cannot rely on the 'current locale' to be able to represent the username in wstrUsername. We therefore
	// force utf-8 output on the username. This means that the autoaccept script must also interpret the username
	// in utf-8, *not* in the current locale.
	strCmdLine = (std::string)autoresponder + " \"" + convert_to<string>("UTF-8", lpRecip->wstrUsername, rawsize(lpRecip->wstrUsername), CHARSET_WCHAR) + "\" \"" + g_lpConfig->GetSettingsPath() + "\" \"" + strEntryID + "\"";
	
	g_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Starting autoaccept with command line %s", strCmdLine.c_str());
	if(!unix_system(lpLogger, autoresponder, strCmdLine.c_str(), (const char **)environ))
		hr = MAPI_E_CALL_FAILED;
		
	// Delete the copy, irrespective of the outcome of the script.
	sEntryList.cValues = 1;
	sEntryList.lpbin = &lpEntryID->Value.bin;
	
	lpRootFolder->DeleteMessages(&sEntryList, 0, NULL, 0);
	// ignore error during delete; the autoaccept script may have already (re)moved the message
	
exit:	
	if(lpRootFolder)
		lpRootFolder->Release();
		
	if(lpMessageCopy)
		lpMessageCopy->Release();
		
	if(lpEntryID)
		MAPIFreeBuffer(lpEntryID);

	return hr;
}

/**
 * Save copy of the raw message
 *
 * @param[in] fp	File pointer to the email data
 * @param[in] lpRecipient	Pointer to a recipient name
 */
void SaveRawMessage(FILE *fp, char *lpRecipient)
{
	if (!g_lpConfig || !g_lpLogger || !fp || !lpRecipient)
		return;

	std::string strFileName = g_lpConfig->GetSetting("log_raw_message_path");

	if (parseBool(g_lpConfig->GetSetting("log_raw_message"))) {
		char szBuff[64];
		tm tmResult;
		time_t now = time(NULL);
		gmtime_safe(&now, &tmResult);

	//  @todo fix windows path!
		if (strFileName.empty()) {
			 g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to save raw message. Wrong configuration, field 'log_raw_message_path' is empty.");
			 return;
		}

		if (strFileName[strFileName.size()-1] != '/')
			strFileName += '/';

		strFileName += lpRecipient;
		sprintf(szBuff, "_%04d%02d%02d%02d%02d%02d_%08x.eml", tmResult.tm_year+1900, tmResult.tm_mon+1, tmResult.tm_mday, tmResult.tm_hour, tmResult.tm_min, tmResult.tm_sec, rand_mt());
		strFileName += szBuff;

		if(DuplicateFile(g_lpLogger, fp, strFileName))
			g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Raw message saved to '%s'", strFileName.c_str());
	}
}

/**
 * Opens the default addressbook container on the given addressbook.
 *
 * @param[in]	lpAdrBook	The IAddrBook interface
 * @param[out]	lppAddrDir	The default addressbook container.
 * @return		MAPI error code
 */
HRESULT OpenResolveAddrFolder(LPADRBOOK lpAdrBook, IABContainer **lppAddrDir)
{
	HRESULT hr			= hrSuccess;
	LPENTRYID lpEntryId	= NULL;
	ULONG cbEntryId		= 0;
	ULONG ulObj			= 0;

	if (lpAdrBook == NULL || lppAddrDir == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	hr = lpAdrBook->GetDefaultDir(&cbEntryId, &lpEntryId);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to find default resolve directory");
		goto exit;
	}

	hr = lpAdrBook->OpenEntry(cbEntryId, lpEntryId, NULL, 0, &ulObj, (LPUNKNOWN*)lppAddrDir);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to open default resolve directory");
		goto exit;
	}

exit:
	if (lpEntryId)
		MAPIFreeBuffer(lpEntryId);

	return hr;
}

/**
 * Opens the addressbook on the given session, and optionally opens
 * the default addressbook container of the addressbook.
 *
 * @param[in]	lpSession	The IMAPISession interface of the logged in user.
 * @param[out]	lpAdrBook	The Global Addressbook.
 * @param[out]	lppAddrDir	The default addressbook container, may be NULL if not wanted.
 * @return		MAPI error code.
 */
HRESULT OpenResolveAddrFolder(IMAPISession *lpSession, LPADRBOOK *lppAdrBook, IABContainer **lppAddrDir)
{
	HRESULT hr = hrSuccess;

	if (lpSession == NULL || lppAdrBook == NULL) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	hr = lpSession->OpenAddressBook(0, NULL, 0, lppAdrBook);
	if(hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to open addressbook");
		goto exit;
	}

	if (lppAddrDir) {
		hr = OpenResolveAddrFolder(*lppAdrBook, lppAddrDir);
		if(hr != hrSuccess)
			goto exit;
	}

exit:
	return hr;
}

/** 
 * Resolve usernames/email addresses to Zarafa users.
 * 
 * @param[in] lpArgs delivery options
 * @param[in] lpAddrFolder resolve users from this addressbook container
 * @param[in,out] lRCPT the list of recipients to resolve in Zarafa
 * 
 * @return MAPI Error code
 */
HRESULT ResolveUsers(DeliveryArgs *lpArgs, IABContainer *lpAddrFolder, recipients_t *lRCPT)
{
	HRESULT hr = hrSuccess;
	recipients_t::iterator iter;
	LPADRLIST lpAdrList	= NULL;   
	FlagList *lpFlagList = NULL;
	SizedSPropTagArray(13, sptaAddress) = {	13,
	{ PR_ENTRYID, PR_DISPLAY_NAME_W, PR_ACCOUNT_W, PR_SMTP_ADDRESS_A,
	  PR_ADDRTYPE_A, PR_EMAIL_ADDRESS_W, PR_DISPLAY_TYPE, PR_SEARCH_KEY,
	  PR_EC_COMPANY_NAME_W,	PR_EC_HOMESERVER_NAME_W, PR_EC_ADMINISTRATOR, 
	  PR_EC_ENABLED_FEATURES_W, PR_OBJECT_TYPE }
	};
	// pointers into the row data, non-free
	LPSPropValue lpEntryIdProp		= NULL;
	LPSPropValue lpFullNameProp		= NULL;
	LPSPropValue lpCompanyProp		= NULL;
	LPSPropValue lpAccountProp		= NULL;
	LPSPropValue lpSMTPProp			= NULL;
	LPSPropValue lpServerProp		= NULL;
	LPSPropValue lpDisplayProp		= NULL;
	LPSPropValue lpAdminProp		= NULL;
	LPSPropValue lpAddrTypeProp		= NULL;
	LPSPropValue lpEmailProp		= NULL;
	LPSPropValue lpSearchKeyProp	= NULL;
	LPSPropValue lpFeatureList		= NULL;
	LPSPropValue lpObjectProp		= NULL;

	ULONG ulRCPT = lRCPT->size();

	hr = MAPIAllocateBuffer(CbNewSRowSet(ulRCPT), (void **) &lpAdrList);
	if (hr != hrSuccess)
		goto exit;

	lpAdrList->cEntries = ulRCPT;

	hr = MAPIAllocateBuffer(CbNewFlagList(ulRCPT), (void **) &lpFlagList);
	if (hr != hrSuccess)
		goto exit;

	lpFlagList->cFlags = ulRCPT;

	for (iter = lRCPT->begin(), ulRCPT = 0; iter != lRCPT->end(); iter++, ulRCPT++) {
		lpAdrList->aEntries[ulRCPT].cValues = 1;

		hr = MAPIAllocateBuffer(sizeof(SPropValue), (void **) &lpAdrList->aEntries[ulRCPT].rgPropVals);
		if (hr != hrSuccess)
			goto exit;

		/* szName can either be the email address or username, it doesn't really matter */
		lpAdrList->aEntries[ulRCPT].rgPropVals[0].ulPropTag = PR_DISPLAY_NAME_W;
		lpAdrList->aEntries[ulRCPT].rgPropVals[0].Value.lpszW = (WCHAR*)(*iter)->wstrRCPT.c_str();

		lpFlagList->ulFlag[ulRCPT] = MAPI_UNRESOLVED;
	}

	// MAPI_UNICODE flag here doesn't have any effect, since we give all proptags ourself
	hr = lpAddrFolder->ResolveNames((LPSPropTagArray)&sptaAddress, MAPI_UNICODE | EMS_AB_ADDRESS_LOOKUP, lpAdrList, lpFlagList);
	if (hr != hrSuccess)
		goto exit;

	for (iter = lRCPT->begin(), ulRCPT = 0; iter != lRCPT->end(); iter++, ulRCPT++) {
		if (lpFlagList->ulFlag[ulRCPT] != MAPI_RESOLVED) {
			g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Failed to resolve recipient %ls", (*iter)->wstrRCPT.c_str());
			continue;
		}

		/* Yay, resolved the address, get it */
		lpEntryIdProp = PpropFindProp(lpAdrList->aEntries[ulRCPT].rgPropVals, lpAdrList->aEntries[ulRCPT].cValues, PR_ENTRYID);
		lpFullNameProp = PpropFindProp(lpAdrList->aEntries[ulRCPT].rgPropVals, lpAdrList->aEntries[ulRCPT].cValues, PR_DISPLAY_NAME_W);
		lpAccountProp = PpropFindProp(lpAdrList->aEntries[ulRCPT].rgPropVals, lpAdrList->aEntries[ulRCPT].cValues, PR_ACCOUNT_W);
		lpSMTPProp = PpropFindProp(lpAdrList->aEntries[ulRCPT].rgPropVals, lpAdrList->aEntries[ulRCPT].cValues, PR_SMTP_ADDRESS_A);
		lpObjectProp = PpropFindProp(lpAdrList->aEntries[ulRCPT].rgPropVals, lpAdrList->aEntries[ulRCPT].cValues, PR_OBJECT_TYPE);
		// the only property that is allowed NULL in this list
		lpDisplayProp = PpropFindProp(lpAdrList->aEntries[ulRCPT].rgPropVals, lpAdrList->aEntries[ulRCPT].cValues, PR_DISPLAY_TYPE);

		if(!lpEntryIdProp || !lpFullNameProp || !lpAccountProp || !lpSMTPProp || !lpObjectProp) {
			g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Not all properties found for %ls", (*iter)->wstrRCPT.c_str());
			continue;
		}

		if (lpObjectProp->Value.ul != MAPI_MAILUSER) {
			g_lpLogger->Log(EC_LOGLEVEL_WARNING, "Resolved recipient %ls is not a user", (*iter)->wstrRCPT.c_str());
			continue;
		} else if (lpDisplayProp && lpDisplayProp->Value.ul == DT_REMOTE_MAILUSER) {
			// allowed are DT_MAILUSER, DT_ROOM and DT_EQUIPMENT. all other DT_* defines are no MAPI_MAILUSER
			g_lpLogger->Log(EC_LOGLEVEL_WARNING, "Resolved recipient %ls is a contact address, unable to deliver", (*iter)->wstrRCPT.c_str());
			continue;
		}

		g_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Resolved recipient %ls as user %ls", (*iter)->wstrRCPT.c_str(), lpAccountProp->Value.lpszW);

		/* The following are allowed to be NULL */
		lpCompanyProp = PpropFindProp(lpAdrList->aEntries[ulRCPT].rgPropVals, lpAdrList->aEntries[ulRCPT].cValues, PR_EC_COMPANY_NAME_W);
		lpServerProp = PpropFindProp(lpAdrList->aEntries[ulRCPT].rgPropVals, lpAdrList->aEntries[ulRCPT].cValues, PR_EC_HOMESERVER_NAME_W);
		lpAdminProp = PpropFindProp(lpAdrList->aEntries[ulRCPT].rgPropVals, lpAdrList->aEntries[ulRCPT].cValues, PR_EC_ADMINISTRATOR);
		lpAddrTypeProp = PpropFindProp(lpAdrList->aEntries[ulRCPT].rgPropVals, lpAdrList->aEntries[ulRCPT].cValues, PR_ADDRTYPE_A);
		lpEmailProp = PpropFindProp(lpAdrList->aEntries[ulRCPT].rgPropVals, lpAdrList->aEntries[ulRCPT].cValues, PR_EMAIL_ADDRESS_W);
		lpSearchKeyProp = PpropFindProp(lpAdrList->aEntries[ulRCPT].rgPropVals, lpAdrList->aEntries[ulRCPT].cValues, PR_SEARCH_KEY);

		(*iter)->wstrUsername.assign(lpAccountProp->Value.lpszW);
		(*iter)->wstrFullname.assign(lpFullNameProp->Value.lpszW);
		(*iter)->strSMTP.assign(lpSMTPProp->Value.lpszA);
		if (Util::HrCopyBinary(lpEntryIdProp->Value.bin.cb, lpEntryIdProp->Value.bin.lpb, &(*iter)->sEntryId.cb, &(*iter)->sEntryId.lpb) != hrSuccess)
			continue;

		/* Only when multi-company has been enabled will we have the companyname. */
		if (lpCompanyProp)
			(*iter)->wstrCompany.assign(lpCompanyProp->Value.lpszW);

		/* Only when distributed has been enabled will we have the servername. */
		if (lpServerProp)
			(*iter)->wstrServerDisplayName.assign(lpServerProp->Value.lpszW);

		if (lpDisplayProp)
			(*iter)->ulDisplayType = lpDisplayProp->Value.ul;

		if (lpAdminProp)
			(*iter)->ulAdminLevel = lpAdminProp->Value.ul;

		if (lpAddrTypeProp)
			(*iter)->strAddrType.assign(lpAddrTypeProp->Value.lpszA);
		else
			(*iter)->strAddrType.assign("SMTP");

		if (lpEmailProp)
			(*iter)->wstrEmail.assign(lpEmailProp->Value.lpszW);

		if (lpSearchKeyProp) {
			if (Util::HrCopyBinary(lpSearchKeyProp->Value.bin.cb, lpSearchKeyProp->Value.bin.lpb, &(*iter)->sSearchKey.cb, &(*iter)->sSearchKey.lpb) != hrSuccess)
				continue;
		} else {
			std::string key = "SMTP:" + (*iter)->strSMTP;
			key = strToUpper(key);

			(*iter)->sSearchKey.cb = key.size() + 1; // + terminating 0
			if (MAPIAllocateBuffer((*iter)->sSearchKey.cb, (void**)&(*iter)->sSearchKey.lpb) != hrSuccess)
				continue;

			memcpy((*iter)->sSearchKey.lpb, key.c_str(), (*iter)->sSearchKey.cb);
		}

		lpFeatureList = PpropFindProp(lpAdrList->aEntries[ulRCPT].rgPropVals, lpAdrList->aEntries[ulRCPT].cValues, PR_EC_ENABLED_FEATURES_W);
		(*iter)->bHasIMAP = lpFeatureList && hasFeature(L"imap", lpFeatureList) == hrSuccess;

		(*iter)->bResolved = true;
	}

exit:
	if(lpAdrList)
		FreeProws((LPSRowSet)lpAdrList);

	if(lpFlagList)
		MAPIFreeBuffer(lpFlagList);

	return hr;
}

/** 
 * Resolve a single recipient as Zarafa user
 * 
 * @param[in] lpArgs delivery options
 * @param[in] lpAddrFolder resolve users from this addressbook container
 * @param[in,out] lpRecip recipient to resolve in Zarafa
 * 
 * @return MAPI Error code
 */
HRESULT ResolveUser(DeliveryArgs *lpArgs, IABContainer *lpAddrFolder, ECRecipient *lpRecip)
{
	HRESULT hr = hrSuccess;
	recipients_t list;

	/* Simple wrapper around ResolveUsers */
	list.insert(lpRecip);
	hr = ResolveUsers(lpArgs, lpAddrFolder, &list);
	if (hr != hrSuccess)
		goto exit;

	if (!lpRecip->bResolved)
		hr = MAPI_E_NOT_FOUND;

exit:
	return hr;
}

/** 
 * Free a list of recipients
 * 
 * @param[in] lpCompanyRecips list to free memory of, and clear.
 * 
 * @return MAPI Error code
 */
HRESULT FreeServerRecipients(companyrecipients_t *lpCompanyRecips)
{
	HRESULT hr = hrSuccess;
	companyrecipients_t::iterator iterCMP;
	serverrecipients_t::iterator iterSRV;
	recipients_t::iterator iterRCPT;

	if (!lpCompanyRecips) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	for (iterCMP = lpCompanyRecips->begin(); iterCMP != lpCompanyRecips->end(); iterCMP++) {
		for (iterSRV = iterCMP->second.begin(); iterSRV != iterCMP->second.end(); iterSRV++) {
			for (iterRCPT = iterSRV->second.begin(); iterRCPT != iterSRV->second.end(); iterRCPT++)
				delete *iterRCPT;
		}
	}

	lpCompanyRecips->clear();

exit:
	return hr;
}

/** 
 * Add a recipient to a delivery list, grouped by companies and
 * servers. If recipient is added to the container, it will be set to
 * NULL so you can't free it anymore. It will be freed when the
 * container is freed.
 * 
 * @param[in,out] lpCompanyRecips container to add recipient in
 * @param[in,out] lppRecipient Recipient to add to the container
 * 
 * @return MAPI Error code
 */
HRESULT AddServerRecipient(companyrecipients_t *lpCompanyRecips, ECRecipient **lppRecipient)
{
	HRESULT hr = hrSuccess;
	companyrecipients_t::iterator iterCMP;
	serverrecipients_t::iterator iterSRV;
	recipients_t::iterator iterRecip;
	ECRecipient *lpRecipient = *lppRecipient;

	if (!lpCompanyRecips) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	// Find or insert
	iterCMP = lpCompanyRecips->insert(companyrecipients_t::value_type(lpRecipient->wstrCompany, serverrecipients_t())).first;

	// Find or insert
	iterSRV = iterCMP->second.insert(serverrecipients_t::value_type(lpRecipient->wstrServerDisplayName, recipients_t())).first;

	// insert into sorted set
	iterRecip = iterSRV->second.find(lpRecipient);
	if (iterRecip == iterSRV->second.end()) {
		iterSRV->second.insert(lpRecipient);
		// The recipient is in the list, and no longer belongs to the caller
		*lppRecipient = NULL;
	} else {
		g_lpLogger->Log(EC_LOGLEVEL_INFO, "Combining recipient %ls and %ls, delivering only once", lpRecipient->wstrRCPT.c_str(), (*iterRecip)->wstrUsername.c_str());
		(*iterRecip)->combine(lpRecipient);
	}

exit:
	return hr;
}

/** 
 * Make a map of recipients grouped by url instead of server name
 * 
 * @param[in] lpSession MAPI admin session
 * @param[in] lpServerNameRecips recipients grouped by server name
 * @param[in] strDefaultPath default connection url to zarafa
 * @param[out] lpServerPathRecips recipients grouped by server url
 * 
 * @return MAPI Error code
 */
HRESULT ResolveServerToPath(IMAPISession *lpSession, serverrecipients_t *lpServerNameRecips, const std::string &strDefaultPath, serverrecipients_t *lpServerPathRecips)
{
	HRESULT hr = hrSuccess;
	IMsgStore *lpAdminStore = NULL;
	IECServiceAdmin *lpServiceAdmin = NULL;
	LPSPropValue	lpsObject = NULL;
	LPECSVRNAMELIST	lpSrvNameList = NULL;
	LPECSERVERLIST	lpSrvList = NULL;
	serverrecipients_t::iterator iter;

	if (!lpServerNameRecips || !lpServerPathRecips) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	/* Single server environment, use default path */
	if (lpServerNameRecips->size() == 1 && lpServerNameRecips->begin()->first.empty()) {
		lpServerPathRecips->insert(serverrecipients_t::value_type(convert_to<wstring>(strDefaultPath), lpServerNameRecips->begin()->second));
		goto exit;
	}

	hr = HrOpenDefaultStore(lpSession, &lpAdminStore);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to open default store for system account, error code: 0x%08X", hr);
		goto exit; // HrLogon() failed .. try again later
	}

	hr = HrGetOneProp(lpAdminStore, PR_EC_OBJECT, &lpsObject);
	if(hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to get internal object, error code: 0x%08X", hr);
		goto exit;
	}

	// NOTE: object is placed in Value.lpszA, not Value.x
	hr = ((IECUnknown *)lpsObject->Value.lpszA)->QueryInterface(IID_IECServiceAdmin, (void **) &lpServiceAdmin);
	if(hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to get service admin, error code: 0x%08X", hr);
		goto exit;
	}

	hr = MAPIAllocateBuffer(sizeof(ECSVRNAMELIST), (LPVOID *)&lpSrvNameList);
	if (hr != hrSuccess)
		goto exit;

	hr = MAPIAllocateMore(sizeof(WCHAR *) * lpServerNameRecips->size(), lpSrvNameList, (LPVOID *)&lpSrvNameList->lpszaServer);
	if (hr != hrSuccess)
		goto exit;

	lpSrvNameList->cServers = 0;
	for (iter = lpServerNameRecips->begin(); iter != lpServerNameRecips->end(); iter++) {
		hr = MAPIAllocateMore((iter->first.size() + 1) * sizeof(WCHAR), lpSrvNameList, (LPVOID *)&lpSrvNameList->lpszaServer[lpSrvNameList->cServers]);
		if (hr != hrSuccess)
			goto exit;

		wcscpy((LPWSTR)lpSrvNameList->lpszaServer[lpSrvNameList->cServers], iter->first.c_str());
		lpSrvNameList->cServers++;
	}

	hr = lpServiceAdmin->GetServerDetails(lpSrvNameList, EC_SERVERDETAIL_PREFEREDPATH | MAPI_UNICODE, &lpSrvList);
	if (hr != hrSuccess)
		goto exit;

	for (ULONG i = 0; i < lpSrvList->cServers; i++) {
		iter = lpServerNameRecips->find((LPWSTR)lpSrvList->lpsaServer[i].lpszName);
		if (iter == lpServerNameRecips->end()) {
			g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Server '%s' not found", (char*)lpSrvList->lpsaServer[i].lpszName);
			hr = MAPI_E_NOT_FOUND;
			goto exit;
		}

		g_lpLogger->Log(EC_LOGLEVEL_DEBUG, "%d recipient(s) on server '%ls' (url %ls)", (int)iter->second.size(),
						lpSrvList->lpsaServer[i].lpszName, lpSrvList->lpsaServer[i].lpszPreferedPath);
		lpServerPathRecips->insert(serverrecipients_t::value_type((LPWSTR)lpSrvList->lpsaServer[i].lpszPreferedPath, iter->second));
	}

	if (lpServerNameRecips->size() != lpServerPathRecips->size()) {
		g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to resolve all server names to paths");
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}

exit:
	if (lpSrvNameList)
		MAPIFreeBuffer(lpSrvNameList);

	if (lpSrvList)
		MAPIFreeBuffer(lpSrvList);

	if (lpsObject)
		MAPIFreeBuffer(lpsObject);

	if (lpServiceAdmin)
		lpServiceAdmin->Release();

	if (lpAdminStore)
		lpAdminStore->Release();

	return hr;
}

/** 
 * For a given recipient, open it's store, inbox and delivery folder.
 * 
 * @param[in] lpSession MAPI Admin session
 * @param[in] lpAdminStore Store of the admin
 * @param[in] lpRecip Resolved Zarafa recipient to open folders for
 * @param[in] lpArgs Use these delivery options to open correct folders etc.
 * @param[out] lppStore Store of the recipient
 * @param[out] lppInbox Inbox of the recipient
 * @param[out] lppFolder Delivery folder of the recipient
 * 
 * @return MAPI Error code
 */
HRESULT HrGetDeliveryStoreAndFolder(IMAPISession *lpSession, IMsgStore *lpAdminStore, ECRecipient *lpRecip, DeliveryArgs *lpArgs, LPMDB *lppStore, IMAPIFolder **lppInbox, IMAPIFolder **lppFolder)
{
	HRESULT hr = hrSuccess;
	IMAPIFolder *lpDeliveryFolder = NULL;
	LPMDB lpDeliveryStore = NULL;
	LPMDB lpUserStore = NULL;
	LPMDB lpPublicStore = NULL;
	IMAPIFolder *lpInbox = NULL;
	IMAPIFolder *lpSubFolder = NULL;
	IMAPIFolder *lpJunkFolder = NULL;
	LPEXCHANGEMANAGESTORE lpIEMS = NULL;
	LPSPropValue lpsObject = NULL;
	LPSPropValue lpJunkProp = NULL;
	LPSPropValue lpWritePerms = NULL;
	ULONG cbUserStoreEntryId = 0;
	LPENTRYID lpUserStoreEntryId = NULL;
	ULONG cbEntryId = 0;
	LPENTRYID lpEntryId = NULL;
	ULONG ulObjType = 0;
	std::wstring strDeliveryFolder = lpArgs->strDeliveryFolder;
	bool bPublicStore = false;

	hr = lpAdminStore->QueryInterface(IID_IExchangeManageStore, (void **)&lpIEMS);
	if (hr != hrSuccess)
		goto exit;

	hr = lpIEMS->CreateStoreEntryID((LPTSTR)L"", (LPTSTR)lpRecip->wstrUsername.c_str(), MAPI_UNICODE | OPENSTORE_HOME_LOGON, &cbUserStoreEntryId, &lpUserStoreEntryId);
	if (hr != hrSuccess)
		goto exit;

	hr = lpSession->OpenMsgStore(0, cbUserStoreEntryId, lpUserStoreEntryId, NULL, MDB_WRITE | MDB_NO_DIALOG | MDB_NO_MAIL | MDB_TEMPORARY, &lpUserStore);
	if (hr != hrSuccess)
		goto exit;

	hr = lpUserStore->GetReceiveFolder((LPTSTR)"IPM", 0, &cbEntryId, &lpEntryId, NULL);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to resolve incoming folder, error code: 0x%08X", hr);
		goto exit;
	}
	
	// Open the inbox
	hr = lpUserStore->OpenEntry(cbEntryId, lpEntryId, &IID_IMAPIFolder, MAPI_MODIFY, &ulObjType, (LPUNKNOWN *)&lpInbox);
	if (hr != hrSuccess || ulObjType != MAPI_FOLDER) {
		g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to open inbox folder, error code: 0x%08X", hr);
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}

	// set default delivery to inbox, and default entryid for notify
	lpDeliveryFolder = lpInbox;
	lpDeliveryStore = lpUserStore;

	switch (lpArgs->ulDeliveryMode) {
	case DM_STORE:
		g_lpLogger->Log(EC_LOGLEVEL_INFO, "Mail will be delivered in Inbox");
		break;
	case DM_JUNK:
		hr = HrGetOneProp(lpInbox, PR_ADDITIONAL_REN_ENTRYIDS, &lpJunkProp);
		if (hr != hrSuccess || lpJunkProp->Value.MVbin.lpbin[4].cb == 0) {
			g_lpLogger->Log(EC_LOGLEVEL_WARNING, "Unable to resolve junk folder, using normal Inbox");
			break;
		}

		g_lpLogger->Log(EC_LOGLEVEL_INFO, "Mail will be delivered in junkmail folder");

		// Open the Junk folder
		hr = lpUserStore->OpenEntry(lpJunkProp->Value.MVbin.lpbin[4].cb, (LPENTRYID)lpJunkProp->Value.MVbin.lpbin[4].lpb,
									&IID_IMAPIFolder, MAPI_MODIFY, &ulObjType, (LPUNKNOWN *)&lpJunkFolder);
		if (hr != hrSuccess || ulObjType != MAPI_FOLDER) {
			g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to open junkmail folder, using normal Inbox");
			break;
		}

		// set new delivery folder
		lpDeliveryFolder = lpJunkFolder;
		break;
	case DM_PUBLIC:
		hr = HrOpenECPublicStore(lpSession, &lpPublicStore);
		if (hr != hrSuccess) {
			g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to open public store, error code 0x%08X", hr);
			// revert to normal inbox delivery
			strDeliveryFolder.clear();
			g_lpLogger->Log(EC_LOGLEVEL_INFO, "Mail will be delivered in Inbox");
		} else {
			g_lpLogger->Log(EC_LOGLEVEL_INFO, "Mail will be delivered in Public store subfolder");
			lpDeliveryStore = lpPublicStore;
			bPublicStore = true;
		}
		break;
	};

	if (!strDeliveryFolder.empty() && lpArgs->ulDeliveryMode != DM_JUNK) {
		hr = OpenSubFolder(lpDeliveryStore, strDeliveryFolder.c_str(), lpArgs->szPathSeperator, g_lpLogger, bPublicStore, lpArgs->bCreateFolder, &lpSubFolder);
		if (hr != hrSuccess) {
			g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Subfolder not found, using normal Inbox. Error code 0x%08X", hr);
			// folder not found, use inbox
			lpDeliveryFolder = lpInbox;
			lpDeliveryStore = lpUserStore;
		} else {
			lpDeliveryFolder = lpSubFolder;
		}
	}

	// check if we may write in the selected folder
	hr = HrGetOneProp(lpDeliveryFolder, PR_ACCESS_LEVEL, &lpWritePerms);
	if (FAILED(hr)) {
		g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to read folder properties, error code: 0x%08X", hr);
		goto exit;
	}
	if ((lpWritePerms->Value.ul & MAPI_MODIFY) == 0) {
		g_lpLogger->Log(EC_LOGLEVEL_FATAL, "No write access in folder, using normal Inbox");
		lpDeliveryStore = lpUserStore;
		lpDeliveryFolder = lpInbox;
	}

	lpDeliveryStore->AddRef();
	*lppStore = lpDeliveryStore;

	lpInbox->AddRef();
	*lppInbox = lpInbox;

	lpDeliveryFolder->AddRef();
	*lppFolder = lpDeliveryFolder;

exit:
	if (lpUserStoreEntryId)
		MAPIFreeBuffer(lpUserStoreEntryId);

	if (lpEntryId)
		MAPIFreeBuffer(lpEntryId);

	if (lpJunkProp)
		MAPIFreeBuffer(lpJunkProp);

	if (lpWritePerms)
		MAPIFreeBuffer(lpWritePerms);

	if (lpIEMS)
		lpIEMS->Release();

	if (lpJunkFolder)
		lpJunkFolder->Release();

	if (lpSubFolder)
		lpSubFolder->Release();

	if (lpInbox)
		lpInbox->Release();

	if (lpPublicStore)
		lpPublicStore->Release();

	if (lpUserStore)
		lpUserStore->Release();

	if (lpsObject)
		MAPIFreeBuffer(lpsObject);

	return hr;
}

/** 
 * Make the message a fallback message.
 * 
 * @param[in,out] lpMessage Message to place fallback data in
 * @param[in] msg original rfc2822 received message
 * 
 * @return MAPI Error code
 */
HRESULT FallbackDelivery(LPMESSAGE lpMessage, const string &msg) {
	HRESULT			hr;
	LPSPropValue	lpPropValue = NULL;
	unsigned int	ulPropPos;
	FILETIME		ft;
	LPATTACH		lpAttach = NULL;
	ULONG			ulAttachNum;
	LPSTREAM		lpStream = NULL;
	LPSPropValue	lpAttPropValue = NULL;
	unsigned int	ulAttPropPos;
	string			newbody;

	// set props
	hr = MAPIAllocateBuffer(sizeof(SPropValue) * 8, (void **)&lpPropValue);
	if (hr != hrSuccess)
		goto exit;

	ulPropPos = 0;

	// Subject
	lpPropValue[ulPropPos].ulPropTag = PR_SUBJECT_W;
	lpPropValue[ulPropPos++].Value.lpszW = L"Fallback delivery";

	// Message flags
	lpPropValue[ulPropPos].ulPropTag = PR_MESSAGE_FLAGS;
	lpPropValue[ulPropPos++].Value.ul = 0;

	// Message class
	lpPropValue[ulPropPos].ulPropTag = PR_MESSAGE_CLASS_W;
	lpPropValue[ulPropPos++].Value.lpszW = L"IPM.Note";

	GetSystemTimeAsFileTime(&ft);

	// Submit time
	lpPropValue[ulPropPos].ulPropTag = PR_CLIENT_SUBMIT_TIME;
	lpPropValue[ulPropPos++].Value.ft = ft;

	// Delivery time
	lpPropValue[ulPropPos].ulPropTag = PR_MESSAGE_DELIVERY_TIME;
	lpPropValue[ulPropPos++].Value.ft = ft;

	newbody = "An E-Mail sent to you could not be correctly read and normally delivered to you.\n\n";
	newbody += "The original message which was sent to you is attached.\n";

	lpPropValue[ulPropPos].ulPropTag = PR_BODY_A;
	lpPropValue[ulPropPos++].Value.lpszA = (char*)newbody.c_str();

	// Add the orinal message into the errorMessage
	hr = lpMessage->CreateAttach(NULL, 0, &ulAttachNum, &lpAttach);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_WARNING, "Unable to create attachment, error code: 0x%08X", hr);
		goto exit;
	}

	hr = lpAttach->OpenProperty(PR_ATTACH_DATA_BIN, &IID_IStream, STGM_WRITE|STGM_TRANSACTED,
								MAPI_CREATE|MAPI_MODIFY, (LPUNKNOWN *)&lpStream);
	if (hr != hrSuccess)
		goto exit;

	hr = lpStream->Write(msg.c_str(), msg.size(), NULL);
	if (hr != hrSuccess)
		goto exit;

	hr = lpStream->Commit(0);
	if (hr != hrSuccess)
		goto exit;

	// Add attachment properties
	hr = MAPIAllocateBuffer(sizeof(SPropValue) * 4, (void **)&lpAttPropValue);
	if (hr != hrSuccess)
		goto exit;

	ulAttPropPos = 0;

	// Attach method .. ?
	lpAttPropValue[ulAttPropPos].ulPropTag = PR_ATTACH_METHOD;
	lpAttPropValue[ulAttPropPos++].Value.ul = ATTACH_BY_VALUE;

	lpAttPropValue[ulAttPropPos].ulPropTag = PR_ATTACH_LONG_FILENAME_W;
	lpAttPropValue[ulAttPropPos++].Value.lpszW = L"original.eml";

	lpAttPropValue[ulAttPropPos].ulPropTag = PR_ATTACH_FILENAME_W;
	lpAttPropValue[ulAttPropPos++].Value.lpszW = L"original.eml";

	lpAttPropValue[ulAttPropPos].ulPropTag = PR_ATTACH_CONTENT_ID_W;
	lpAttPropValue[ulAttPropPos++].Value.lpszW = L"dagent-001@localhost";

	// Add attachment properties
	hr = lpAttach->SetProps(ulAttPropPos, lpAttPropValue, NULL);
	if (hr != hrSuccess)
		goto exit;

	hr = lpAttach->SaveChanges(0);
	if (hr != hrSuccess)
		goto exit;

	// Add message properties
	hr = lpMessage->SetProps(ulPropPos, lpPropValue, NULL);
	if (hr != hrSuccess)
		goto exit;

	hr = lpMessage->SaveChanges(KEEP_OPEN_READWRITE);
	if (hr != hrSuccess)
		goto exit;

exit:
	if (lpAttach)
		lpAttach->Release();

	if (lpStream)
		lpStream->Release();

	if (lpPropValue)
		MAPIFreeBuffer(lpPropValue);

	if (lpAttPropValue)
		MAPIFreeBuffer(lpAttPropValue);

	return hr;
}

/** 
 * Write into the given fd, and if that fails log an error.
 * 
 * @param[in] fd file descriptor to write to
 * @param[in] buffer buffer to write
 * @param[in] len length of buffer to write
 * @param[in] wrap optional wrapping, inserts a \r\n at the point of the wrapping point
 * 
 * @return MAPI Error code
 */
HRESULT WriteOrLogError(int fd, const char* buffer, size_t len, size_t wrap = 0)
{
	HRESULT hr = hrSuccess;
	size_t pos = 0;

	if (!wrap)
		wrap = len;

	while (len) {
		if (write(fd, buffer + pos, min(len, wrap)) < 0) {
			hr = MAPI_E_CALL_FAILED;
			g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Write error to temp file for out of office mail: %s", strerror(errno));
		}
		pos += min(len, wrap);
		len -= min(len, wrap);
		if (len)
			write(fd, "\n", strlen("\n")); // will write more, break when the next block write fails
	}
	return hr;
}

/** 
 * Create an out-of-office mail, and start the script to trigger it's
 * optional sending.
 * 
 * @param[in] lpAdrBook Addressbook for email address rewrites
 * @param[in] lpMDB Store of the user that triggered the oof email
 * @param[in] lpMessage delivery message that triggered the oof email
 * @param[in] lpRecip delivery recipient sending the oof email from
 * @param[in] strBaseCommand Command to use to start the oof mailer (zarafa-autorespond)
 * 
 * @return MAPI Error code
 */
HRESULT SendOutOfOffice(LPADRBOOK lpAdrBook, LPMDB lpMDB, LPMESSAGE lpMessage, ECRecipient *lpRecip, std::string &strBaseCommand)
{
	HRESULT hr = hrSuccess;
	SizedSPropTagArray(3, sptaStoreProps) = { 3,
											  { PR_EC_OUTOFOFFICE, PR_EC_OUTOFOFFICE_MSG_W, PR_EC_OUTOFOFFICE_SUBJECT_W } };
	SizedSPropTagArray(4, sptaMessageProps) = { 4,
												{ PR_TRANSPORT_MESSAGE_HEADERS_A, PR_MESSAGE_TO_ME, PR_MESSAGE_CC_ME, PR_SUBJECT_W } };
	LPSPropValue	lpStoreProps = NULL;
	LPSPropValue	lpMessageProps = NULL;
	ULONG cValues;

	WCHAR *szSubject = L"Out of office";
	char szHeader[PATH_MAX] = {0};
	wchar_t szwHeader[PATH_MAX] = {0};
	char szTemp[PATH_MAX] = {0};
	int fd = -1;
	wstring	strFromName, strFromType, strFromEmail, strBody;
	string  unquoted, quoted;
	string  command = strBaseCommand;	
	// Environment
	const char *env[3];
	std::string strToMe;
	std::string strCcMe;

	// @fixme need to stream PR_TRANSPORT_MESSAGE_HEADERS_A and PR_EC_OUTOFOFFICE_MSG_W if they're > 8Kb

	hr = lpMDB->GetProps((LPSPropTagArray)&sptaStoreProps, 0, &cValues, &lpStoreProps);
	if (FAILED(hr))
		goto exit;
	hr = hrSuccess;

	// Check for autoresponder
	if (lpStoreProps[0].ulPropTag != PR_EC_OUTOFOFFICE || lpStoreProps[0].Value.b == false)
		goto exit;

	// Check for presence of PR_EC_OUTOFOFFICE_MSG_W
	if (lpStoreProps[1].ulPropTag != PR_EC_OUTOFOFFICE_MSG_W) {
		StreamPtr ptrStream;
		if (lpMDB->OpenProperty(PR_EC_OUTOFOFFICE_MSG_W, &IID_IStream, 0, 0, &ptrStream) != hrSuccess ||
			Util::HrStreamToString(ptrStream, strBody) != hrSuccess)
		{
			hr = MAPI_E_NOT_FOUND;
			g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to download out of office message.");
			goto exit;
		}
	} else {
		strBody = lpStoreProps[1].Value.lpszW;
	}
	if (strBody.empty()) {
		g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Out of office mail was enabled, but no message was set. Not sending empty message.");
		goto exit;
	}

	// Possibly override default subject
	if (lpStoreProps[2].ulPropTag == PR_EC_OUTOFOFFICE_SUBJECT_W) {
		szSubject = lpStoreProps[2].Value.lpszW;
	}

	hr = lpMessage->GetProps((LPSPropTagArray)&sptaMessageProps, 0, &cValues, &lpMessageProps);
	if (FAILED(hr))
		goto exit;
	hr = hrSuccess;

	// See if we're looping
	if (lpMessageProps[0].ulPropTag == PR_TRANSPORT_MESSAGE_HEADERS_A) {
		if ( (strstr(lpMessageProps[0].Value.lpszA, "X-Zarafa-Vacation:") != NULL) ||
			 (strstr(lpMessageProps[0].Value.lpszA, "Auto-Submitted:") != NULL) ||
			 (strstr(lpMessageProps[0].Value.lpszA, "Precedence:") != NULL) )
			// Vacation header already present, do not send vacation reply
			// Precedence: list/bulk/junk, do not reply to these mails
			goto exit;
	}
	

	hr = HrGetAddress(lpAdrBook, lpMessage, PR_SENDER_ENTRYID, PR_SENDER_NAME, PR_SENDER_ADDRTYPE, PR_SENDER_EMAIL_ADDRESS, strFromName, strFromType, strFromEmail);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to get sender e-mail address for autoresponder, error code: 0x%08X",hr);
		goto exit;
	}

	snprintf(szTemp, PATH_MAX, "%s/autorespond.XXXXXX", getenv("TEMP") == NULL ? "/tmp" : getenv("TEMP"));
	fd = mkstemp(szTemp);
	if (fd < 0) {
		g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to create temp file for out of office mail: %s", strerror(errno));
		goto exit;
	}

	// \n is on the beginning of the next header line because of snprintf and the requirement of the \n
	// PATH_MAX should never be reached though.
	quoted = ToQuotedBase64Header(lpRecip->wstrFullname);
	snprintf(szHeader, PATH_MAX, "From: %s <%s>", quoted.c_str(), lpRecip->strSMTP.c_str());
	if (WriteOrLogError(fd, szHeader, strlen(szHeader)) != hrSuccess)
		goto exit;

	snprintf(szHeader, PATH_MAX, "\nTo: %ls", strFromEmail.c_str());
	if (WriteOrLogError(fd, szHeader, strlen(szHeader)) != hrSuccess)
		goto exit;

	// add anti-loop header
	snprintf(szHeader, PATH_MAX, "\nX-Zarafa-Vacation: autorespond");
	if (WriteOrLogError(fd, szHeader, strlen(szHeader)) != hrSuccess)
		goto exit;

	if (lpMessageProps[3].ulPropTag == PR_SUBJECT_W) {
		// convert as one string because of [] characters
		swprintf(szwHeader, PATH_MAX, L"%ls [%ls]", szSubject, lpMessageProps[3].Value.lpszW);
	} else {
		swprintf(szwHeader, PATH_MAX, L"%ls", szSubject);
	}
	quoted = ToQuotedBase64Header(szwHeader);
	snprintf(szHeader, PATH_MAX, "\nSubject: %s", quoted.c_str());
	if (WriteOrLogError(fd, szHeader, strlen(szHeader)) != hrSuccess)
		goto exit;

	snprintf(szHeader, PATH_MAX, "\nContent-Type: text/plain; charset=utf-8; format=flowed");
	if (WriteOrLogError(fd, szHeader, strlen(szHeader)) != hrSuccess)
		goto exit;

	snprintf(szHeader, PATH_MAX, "\nContent-Transfer-Encoding: base64");
	if (WriteOrLogError(fd, szHeader, strlen(szHeader)) != hrSuccess)
		goto exit;

	snprintf(szHeader, PATH_MAX, "\nMime-Version: 1.0"); // add mime-version header, so some clients show high-characters correctly
	if (WriteOrLogError(fd, szHeader, strlen(szHeader)) != hrSuccess)
		goto exit;

	snprintf(szHeader, PATH_MAX, "\n\n"); // last header line has double \n
	if (WriteOrLogError(fd, szHeader, strlen(szHeader)) != hrSuccess)
		goto exit;

	// write body
	unquoted = convert_to<string>("UTF-8", strBody, rawsize(strBody), CHARSET_WCHAR);
	quoted = base64_encode((const unsigned char*)unquoted.c_str(), unquoted.length());
	if (WriteOrLogError(fd, quoted.c_str(), quoted.length(), 76) != hrSuccess)
		goto exit;

	close(fd);
	fd = -1;

	// Args: From, To, Subject, Username, Msg_Filename
	// Should run in UTF-8 to get correct strings in UTF-8 from shell_escape(wstring)
	command += string(" '")+shell_escape(lpRecip->strSMTP)+string("' '")+
		shell_escape(strFromEmail)+string("' '")+shell_escape(szSubject)+string("' '")+
		shell_escape(lpRecip->wstrUsername)+string("' '")+shell_escape(szTemp)+string("'");

	// Set MESSAGE_TO_ME and MESSAGE_CC_ME in environment
	strToMe = (std::string)"MESSAGE_TO_ME=" + (lpMessageProps[1].ulPropTag == PR_MESSAGE_TO_ME && lpMessageProps[1].Value.b ? "1" : "0");
	strCcMe = (std::string)"MESSAGE_CC_ME=" + (lpMessageProps[2].ulPropTag == PR_MESSAGE_CC_ME && lpMessageProps[2].Value.b ? "1" : "0");
	env[0] = strToMe.c_str();
	env[1] = strCcMe.c_str();
	env[2] = NULL;

	g_lpLogger->Log(EC_LOGLEVEL_INFO, "Starting autoresponder for out-of-office message");
	command += " 2>&1";

	if(unix_system(g_lpLogger, strBaseCommand.c_str(), command.c_str(), env) == false) {
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Autoresponder failed");
	}

exit:
	if (fd != -1)
		close(fd);

	if (szTemp[0] != 0)
		unlink(szTemp);
	
	if (lpStoreProps)
		MAPIFreeBuffer(lpStoreProps);
		
	if (lpMessageProps)
		MAPIFreeBuffer(lpMessageProps);
		
	return hr;
}

/** 
 * Create an empty message for delivery
 * 
 * @param[in] lpFolder Create the message in this folder
 * @param[in] lpFallbackFolder If write access forbids the creation, fallback to this folder
 * @param[out] lppDeliveryFolder The folder where the message was created
 * @param[out] lppMessage The newly created message
 * 
 * @return MAPI Error code
 */
HRESULT HrCreateMessage(IMAPIFolder *lpFolder, IMAPIFolder *lpFallbackFolder, IMAPIFolder **lppDeliveryFolder, IMessage **lppMessage)
{
	HRESULT hr = hrSuccess;
	IMessage *lpMessage = NULL;

	hr = lpFolder->CreateMessage(NULL, 0, &lpMessage);
	if (hr != hrSuccess && lpFallbackFolder) {
		g_lpLogger->Log(EC_LOGLEVEL_WARNING, "Unable to create new message in subfolder, using normal Inbox. Error code: %08X", hr);
		lpFolder = lpFallbackFolder;
		hr = lpFolder->CreateMessage(NULL, 0, &lpMessage);
	}
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to create new message, error code: %08X", hr);
		goto exit;
	}

	hr = lpMessage->QueryInterface(IID_IMessage, (void**)lppMessage);
	if (hr != hrSuccess)
		goto exit;

	hr = lpFolder->QueryInterface(IID_IMAPIFolder, (void**)lppDeliveryFolder);
	if (hr != hrSuccess)
		goto exit;

exit:
	if (lpMessage)
		lpMessage->Release();

	return hr;
}

/** 
 * Convert the received rfc2822 email into a MAPI message
 * 
 * @param[in] strMail the received email
 * @param[in] lpSession a MAPI Session
 * @param[in] lpMsgStore The store of the delivery
 * @param[in] lpAdrBook The Global Addressbook
 * @param[in] lpDeliveryFolder Folder to create a new message in when the conversion fails
 * @param[in] lpMessage The message to write the conversion data in
 * @param[in] lpArgs delivery options
 * @param[out] lppMessage The delivered message
 * @param[out] lpbFallbackDelivery indicating if the message is a fallback message or not
 * 
 * @return MAPI Error code
 */
HRESULT HrStringToMAPIMessage(string &strMail, IMAPISession *lpSession, IMsgStore *lpMsgStore, LPADRBOOK lpAdrBook, IMAPIFolder *lpDeliveryFolder, IMessage *lpMessage, ECRecipient *lpRecip, DeliveryArgs *lpArgs, IMessage **lppMessage, bool *lpbFallbackDelivery)
{
	HRESULT hr = hrSuccess;
	IMessage *lpFallbackMessage = NULL;
	bool bFallback = false;

	lpArgs->sDeliveryOpts.add_imap_data = lpRecip->bHasIMAP;

	// Set the properties on the object
	hr = IMToMAPI(lpSession, lpMsgStore, lpAdrBook, lpMessage, strMail, lpArgs->sDeliveryOpts, g_lpLogger);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_WARNING, "E-mail parsing failed: 0x%08X. Starting fallback delivery.", hr);

		// create new message
		hr = lpDeliveryFolder->CreateMessage(NULL, 0, &lpFallbackMessage);
		if (hr != hrSuccess) {
			g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to create fallback message, error code: 0x%08X", hr);
			goto exit;
		}

		hr = FallbackDelivery(lpFallbackMessage, strMail);
		if (hr != hrSuccess) {
			g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to deliver fallback message, error code: 0x%08X", hr);
			goto exit;
		}

		// override original message with fallback version to return
		lpMessage = lpFallbackMessage;
		bFallback = true;
	}

	// return the filled (real or fallback) message
	hr = lpMessage->QueryInterface(IID_IMessage, (void**)lppMessage);
	if (hr != hrSuccess)
		goto exit;

	*lpbFallbackDelivery = bFallback;

exit:
	if (lpFallbackMessage)
		lpFallbackMessage->Release();

	return hr;
}

/** 
 * Check if the message was expired (delivery limit, header: Expiry-Time)
 * 
 * @param[in] lpMessage message for delivery
 * @param[out] bExpired message is expired or not
 * 
 * @return always hrSuccess
 */
HRESULT HrMessageExpired(IMessage *lpMessage, bool *bExpired)
{
	HRESULT hr = hrSuccess;
	LPSPropValue lpsExpiryTime = NULL;

	// If the message has an expiry date, and it's past, skip delivering the email
	if (HrGetOneProp(lpMessage, PR_EXPIRY_TIME, &lpsExpiryTime) == hrSuccess) {
		time_t now = time(NULL);
		time_t expire;

		FileTimeToUnixTime(lpsExpiryTime->Value.ft, &expire);

		if (now > expire) {
			// exit with no errors
			hr = hrSuccess;
			*bExpired = true;

			g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Message was expired, not delivering");
			// TODO: if a read-receipt was requested, we need to send a non-read read-receipt
			goto exit;
		}
	}

	*bExpired = false;

exit:
	if (lpsExpiryTime)
		MAPIFreeBuffer(lpsExpiryTime);

	return hr;
}

/** 
 * Replace To recipient data in message with new recipient
 * 
 * @param[in] lpMessage delivery message to set new recipient data in
 * @param[in] lpRecip new recipient to deliver same message for
 * 
 * @return MAPI Error code
 */
HRESULT HrOverrideRecipProps(IMessage *lpMessage, ECRecipient *lpRecip)
{
	HRESULT hr = hrSuccess;
	LPMAPITABLE lpRecipTable = NULL;
	LPSRestriction lpRestrictRecipient = NULL;
	LPSRowSet lpsRows = NULL;
	SPropValue sPropRecip[3];
	SPropValue sCmp[2];
	bool bToMe = false;
	bool bCcMe = false;
	bool bRecipMe = false;

	SizedSPropTagArray(2, sptaColumns) = {
		2, {
			PR_RECIPIENT_TYPE,
			PR_ENTRYID,
		}
	};

	hr = lpMessage->GetRecipientTable (0, &lpRecipTable);
	if (hr != hrSuccess)
		goto exit;

	hr = lpRecipTable->SetColumns((LPSPropTagArray)&sptaColumns, 0);
	if (hr != hrSuccess)
		goto exit;

	sCmp[0].ulPropTag = PR_ADDRTYPE_A;
	sCmp[0].Value.lpszA = "ZARAFA";
	sCmp[1].ulPropTag = PR_SMTP_ADDRESS_A;
	sCmp[1].Value.lpszA = (char*)lpRecip->strSMTP.c_str();

	CREATE_RESTRICTION(lpRestrictRecipient);
	CREATE_RES_AND(lpRestrictRecipient, lpRestrictRecipient, 3);
	DATA_RES_EXIST(lpRestrictRecipient, lpRestrictRecipient->res.resAnd.lpRes[0], PR_RECIPIENT_TYPE);
	DATA_RES_PROPERTY(lpRestrictRecipient, lpRestrictRecipient->res.resAnd.lpRes[1], RELOP_EQ, PR_ADDRTYPE_A, &sCmp[0]);
	DATA_RES_PROPERTY(lpRestrictRecipient, lpRestrictRecipient->res.resAnd.lpRes[2], RELOP_EQ, PR_SMTP_ADDRESS_A, &sCmp[1]);

	hr = lpRecipTable->FindRow(lpRestrictRecipient, BOOKMARK_BEGINNING, 0);
	if (hr == hrSuccess) {
		hr = lpRecipTable->QueryRows (1, 0, &lpsRows);
		if (hr != hrSuccess)
			goto exit;

		bRecipMe = (lpsRows->cRows == 1);
		if (bRecipMe) {
			LPSPropValue lpProp = PpropFindProp(lpsRows->aRow[0].lpProps, lpsRows->aRow[0].cValues, PR_RECIPIENT_TYPE);
			if (lpProp) {
				bToMe = (lpProp->Value.ul == MAPI_TO);
				bCcMe = (lpProp->Value.ul == MAPI_CC);
			}
		}
	} else {
		/*
		 * No recipients were found, message was not to me.
		 * Don't report error to caller, since we should set
		 * the properties to indicate this message is not for us.
		 */
		hr = hrSuccess;
	}

	sPropRecip[0].ulPropTag = PR_MESSAGE_RECIP_ME;
	sPropRecip[0].Value.b = bRecipMe;
	sPropRecip[1].ulPropTag = PR_MESSAGE_TO_ME;
	sPropRecip[1].Value.b = bToMe;
	sPropRecip[2].ulPropTag = PR_MESSAGE_CC_ME;
	sPropRecip[2].Value.b = bCcMe;

	hr = lpMessage->SetProps(3, sPropRecip, NULL);
	if (hr != hrSuccess)
		goto exit;

exit:
	if (lpsRows)
		FreeProws(lpsRows);

	if(lpRecipTable)
		lpRecipTable->Release();

	FREE_RESTRICTION(lpRestrictRecipient);

	return hr;
}

/** 
 * Replace To and From recipient data in fallback message with new recipient
 * 
 * @param[in] lpMessage fallback message to set new recipient data in
 * @param[in] lpRecip new recipient to deliver same message for
 * 
 * @return MAPI Error code
 */
HRESULT HrOverrideFallbackProps(IMessage *lpMessage, ECRecipient *lpRecip)
{
	HRESULT hr = hrSuccess;
	LPENTRYID lpEntryIdSender = NULL;
	ULONG cbEntryIdSender;
	SPropValue sPropOverride[17];
	ULONG ulPropPos = 0;

	// Set From: and To: to the receiving party, reply will be to yourself...
	// Too much information?
	sPropOverride[ulPropPos].ulPropTag = PR_SENDER_NAME_W;
	sPropOverride[ulPropPos++].Value.lpszW = L"System Administrator";

	sPropOverride[ulPropPos].ulPropTag = PR_SENT_REPRESENTING_NAME_W;
	sPropOverride[ulPropPos++].Value.lpszW = L"System Administrator";

	sPropOverride[ulPropPos].ulPropTag = PR_RECEIVED_BY_NAME_W;
	sPropOverride[ulPropPos++].Value.lpszW = (WCHAR *)lpRecip->wstrEmail.c_str();

	// PR_SENDER_EMAIL_ADDRESS
	sPropOverride[ulPropPos].ulPropTag = PR_SENDER_EMAIL_ADDRESS_A;
	sPropOverride[ulPropPos++].Value.lpszA = (char *)lpRecip->strSMTP.c_str();

	// PR_SENT_REPRESENTING_EMAIL_ADDRESS
	sPropOverride[ulPropPos].ulPropTag = PR_SENT_REPRESENTING_EMAIL_ADDRESS_A;
	sPropOverride[ulPropPos++].Value.lpszA = (char *)lpRecip->strSMTP.c_str();

	// PR_RECEIVED_BY_EMAIL_ADDRESS
	sPropOverride[ulPropPos].ulPropTag = PR_RECEIVED_BY_EMAIL_ADDRESS_A;
	sPropOverride[ulPropPos++].Value.lpszA = (char *)lpRecip->strSMTP.c_str();

	sPropOverride[ulPropPos].ulPropTag = PR_RCVD_REPRESENTING_EMAIL_ADDRESS_A;
	sPropOverride[ulPropPos++].Value.lpszA = (char *)lpRecip->strSMTP.c_str();

	// PR_SENDER_ADDRTYPE
	sPropOverride[ulPropPos].ulPropTag = PR_SENDER_ADDRTYPE_W;
	sPropOverride[ulPropPos++].Value.lpszW = L"SMTP";

	// PR_SENT_REPRESENTING_ADDRTYPE
	sPropOverride[ulPropPos].ulPropTag = PR_SENT_REPRESENTING_ADDRTYPE_W;
	sPropOverride[ulPropPos++].Value.lpszW = L"SMTP";

	// PR_RECEIVED_BY_ADDRTYPE
	sPropOverride[ulPropPos].ulPropTag = PR_RECEIVED_BY_ADDRTYPE_W;
	sPropOverride[ulPropPos++].Value.lpszW = L"SMTP";

	sPropOverride[ulPropPos].ulPropTag = PR_RCVD_REPRESENTING_ADDRTYPE_W;
	sPropOverride[ulPropPos++].Value.lpszW = L"SMTP";

	// PR_SENDER_SEARCH_KEY
	sPropOverride[ulPropPos].ulPropTag = PR_SENDER_SEARCH_KEY;
	sPropOverride[ulPropPos].Value.bin.cb = lpRecip->sSearchKey.cb;
	sPropOverride[ulPropPos++].Value.bin.lpb = lpRecip->sSearchKey.lpb;

	// PR_RECEIVED_BY_SEARCH_KEY (set as previous)
	sPropOverride[ulPropPos].ulPropTag = PR_RECEIVED_BY_SEARCH_KEY;
	sPropOverride[ulPropPos].Value.bin.cb = lpRecip->sSearchKey.cb;
	sPropOverride[ulPropPos++].Value.bin.lpb = lpRecip->sSearchKey.lpb;

	// PR_SENT_REPRESENTING_SEARCH_KEY (set as previous)
	sPropOverride[ulPropPos].ulPropTag = PR_SENT_REPRESENTING_SEARCH_KEY;
	sPropOverride[ulPropPos].Value.bin.cb = lpRecip->sSearchKey.cb;
	sPropOverride[ulPropPos++].Value.bin.lpb = lpRecip->sSearchKey.lpb;

	hr = ECCreateOneOff((LPTSTR)lpRecip->wstrFullname.c_str(), (LPTSTR)L"SMTP", (LPTSTR)convert_to<wstring>(lpRecip->strSMTP).c_str(),
						MAPI_UNICODE | MAPI_SEND_NO_RICH_INFO, &cbEntryIdSender, &lpEntryIdSender);
	if (hr == hrSuccess) {
		// PR_SENDER_ENTRYID
		sPropOverride[ulPropPos].ulPropTag = PR_SENDER_ENTRYID;
		sPropOverride[ulPropPos].Value.bin.cb = cbEntryIdSender;
		sPropOverride[ulPropPos++].Value.bin.lpb = (LPBYTE)lpEntryIdSender;

		// PR_RECEIVED_BY_ENTRYID
		sPropOverride[ulPropPos].ulPropTag = PR_RECEIVED_BY_ENTRYID;
		sPropOverride[ulPropPos].Value.bin.cb = cbEntryIdSender;
		sPropOverride[ulPropPos++].Value.bin.lpb = (LPBYTE)lpEntryIdSender;

		// PR_SENT_REPRESENTING_ENTRYID
		sPropOverride[ulPropPos].ulPropTag = PR_SENT_REPRESENTING_ENTRYID;
		sPropOverride[ulPropPos].Value.bin.cb = cbEntryIdSender;
		sPropOverride[ulPropPos++].Value.bin.lpb = (LPBYTE)lpEntryIdSender;
	} else {
		hr = hrSuccess;
	}

	hr = lpMessage->SetProps(ulPropPos, sPropOverride, NULL);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to set Fallback Delivery properties: 0x%08X", hr);
		goto exit;
	}

exit:
	if (lpEntryIdSender)
		MAPIFreeBuffer(lpEntryIdSender);

	return hr;
}

/** 
 * Set new To recipient data in message
 * 
 * @param[in] lpMessage message to update recipient data in
 * @param[in] lpRecip recipient data to use
 * 
 * @return MAPI error code
 */
HRESULT HrOverrideReceivedByProps(IMessage *lpMessage, ECRecipient *lpRecip)
{
	HRESULT hr = hrSuccess;
	SPropValue sPropReceived[5];

	/* First set the PR_RECEIVED_BY_* properties */
	sPropReceived[0].ulPropTag = PR_RECEIVED_BY_ADDRTYPE_A;
	sPropReceived[0].Value.lpszA = (char *)lpRecip->strAddrType.c_str();

	sPropReceived[1].ulPropTag = PR_RECEIVED_BY_EMAIL_ADDRESS_W;
	sPropReceived[1].Value.lpszW = (WCHAR *)lpRecip->wstrUsername.c_str();

	sPropReceived[2].ulPropTag = PR_RECEIVED_BY_ENTRYID;
	sPropReceived[2].Value.bin.cb = lpRecip->sEntryId.cb;
	sPropReceived[2].Value.bin.lpb = lpRecip->sEntryId.lpb;

	sPropReceived[3].ulPropTag = PR_RECEIVED_BY_NAME_W;
	sPropReceived[3].Value.lpszW = (WCHAR *)lpRecip->wstrFullname.c_str();

	sPropReceived[4].ulPropTag = PR_RECEIVED_BY_SEARCH_KEY;
	sPropReceived[4].Value.bin.cb = lpRecip->sSearchKey.cb;
	sPropReceived[4].Value.bin.lpb = lpRecip->sSearchKey.lpb;

	hr = lpMessage->SetProps(5, sPropReceived, NULL);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to set RECEIVED_BY properties: 0x%08X", hr);
		goto exit;
	}

exit:
	return hr;
}

/** 
 * Copy a delivered message to another recipient
 * 
 * @param[in] lpOrigMessage The original delivered message
 * @param[in] lpDeliverFolder The delivery folder of the new message
 * @param[in] lpRecip recipient data to use
 * @param[in] lpArgs delivery options
 * @param[in] lpFallbackFolder Fallback folder incase lpDeliverFolder cannot be delivered to
 * @param[in] bFallbackDelivery lpOrigMessage is a fallback delivery message
 * @param[out] lppFolder folder the new message was created in
 * @param[out] lppMessage the newly copied message
 * 
 * @return MAPI Error code
 */
HRESULT HrCopyMessageForDelivery(IMessage *lpOrigMessage, IMAPIFolder *lpDeliverFolder, ECRecipient *lpRecip, DeliveryArgs *lpArgs, IMAPIFolder *lpFallbackFolder, bool bFallbackDelivery, IMAPIFolder **lppFolder = NULL, IMessage **lppMessage = NULL)
{
	HRESULT hr = hrSuccess;
	IMessage *lpMessage = NULL;
	IMAPIFolder *lpFolder = NULL;
	LPSPropTagArray lpExclude = NULL;
	za::helpers::MAPIPropHelperPtr ptrArchiveHelper;

	SizedSPropTagArray(13, sptaReceivedBy) = {
		13, {
			/* Overriden by HrOverrideRecipProps() */
			PR_MESSAGE_RECIP_ME,
			PR_MESSAGE_TO_ME,
			PR_MESSAGE_CC_ME,
			/* HrOverrideReceivedByProps() */
			PR_RECEIVED_BY_ADDRTYPE,
			PR_RECEIVED_BY_EMAIL_ADDRESS,
			PR_RECEIVED_BY_ENTRYID,
			PR_RECEIVED_BY_NAME,
			PR_RECEIVED_BY_SEARCH_KEY,
			/* Written by rules */
			PR_LAST_VERB_EXECUTED,
			PR_LAST_VERB_EXECUTION_TIME,
			PR_ICON_INDEX,
		}
	};

	SizedSPropTagArray(12, sptaFallback) = {
		12, {
			/* Overriden by HrOverrideFallbackProps() */
			PR_SENDER_ADDRTYPE,
			PR_SENDER_EMAIL_ADDRESS,
			PR_SENDER_ENTRYID,
			PR_SENDER_NAME,
			PR_SENDER_SEARCH_KEY,
			PR_SENT_REPRESENTING_ADDRTYPE,
			PR_SENT_REPRESENTING_EMAIL_ADDRESS,
			PR_SENT_REPRESENTING_ENTRYID,
			PR_SENT_REPRESENTING_NAME,
			PR_SENT_REPRESENTING_SEARCH_KEY,
			PR_RCVD_REPRESENTING_ADDRTYPE,
			PR_RCVD_REPRESENTING_EMAIL_ADDRESS,
		}
	};

	hr = HrCreateMessage(lpDeliverFolder, lpFallbackFolder, &lpFolder, &lpMessage);
	if (hr != hrSuccess)
		goto exit;

	if (bFallbackDelivery)
		lpExclude = (LPSPropTagArray)&sptaFallback;
	else
		lpExclude = (LPSPropTagArray)&sptaReceivedBy;

	/* Copy message, exclude all previously set properties (Those are recipient dependent) */
	hr = lpOrigMessage->CopyTo(0, NULL, (LPSPropTagArray)&sptaReceivedBy, 0, NULL, &IID_IMessage, lpMessage, 0, NULL);
	if (hr != hrSuccess)
		goto exit;
		
	// For a fallback, remove some more properties
	if (bFallbackDelivery)
		lpMessage->DeleteProps((LPSPropTagArray)&sptaFallback, 0);
		
	// Make sure the message is not attached to an archive
	hr = za::helpers::MAPIPropHelper::Create(MAPIPropPtr(lpMessage, true), &ptrArchiveHelper);
	if (hr != hrSuccess)
		goto exit;
	
	hr = ptrArchiveHelper->DetachFromArchives();
	if (hr != hrSuccess)
		goto exit;

	if (lpRecip->bHasIMAP)
		hr = Util::HrCopyIMAPData(lpOrigMessage, lpMessage);
	else
		hr = Util::HrDeleteIMAPData(lpMessage); // make sure the imap data is not set for this user.
	if (hr != hrSuccess)
		goto exit;

	if (lppFolder)
		lpFolder->QueryInterface(IID_IMAPIFolder, (void**)lppFolder);

	if (lppMessage)
		lpMessage->QueryInterface(IID_IMessage, (void**)lppMessage);

exit:
	if (lpMessage)
		lpMessage->Release();

	if (lpFolder)
		lpFolder->Release();

	return hr;
}

/** 
 * Make a new MAPI session under a specific username
 * 
 * @param[in] lpArgs delivery options
 * @param[in] szUsername username to create mapi session for
 * @param[out] lppSession new MAPI session for user
 * @param[in] bSuppress suppress logging (default: false)
 * 
 * @return MAPI Error code
 */
HRESULT HrGetSession(DeliveryArgs *lpArgs, const WCHAR *szUsername, IMAPISession **lppSession, bool bSuppress = false)
{
	HRESULT hr = hrSuccess;
	struct passwd *pwd = NULL;
	string strUnixUser;

	hr = HrOpenECSession(lppSession, szUsername, L"", lpArgs->strPath.c_str(), 0,
						 g_lpConfig->GetSetting("sslkey_file","",NULL), g_lpConfig->GetSetting("sslkey_pass","",NULL));
	if (hr != hrSuccess) {
		// if connecting fails, the mailer should try to deliver again.
		switch (hr) {
		case MAPI_E_NETWORK_ERROR:
			if (!bSuppress) g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to connect to zarafa server for user %ls, using socket: '%s'", szUsername, lpArgs->strPath.c_str());
			break;

		// MAPI_E_NO_ACCESS or MAPI_E_LOGON_FAILED are fatal (user does not exist)
		case MAPI_E_LOGON_FAILED:
			// running dagent as unix user != lpRecip->strUsername and ! listed in local_admin_user, which gives this error too
			if (!bSuppress) g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Access denied or Connection failed for user %ls, using socket: '%s', error code: 0x%08X", szUsername, lpArgs->strPath.c_str(), hr);
			// so also log userid we're running as
			pwd = getpwuid(getuid());
			if (pwd && pwd->pw_name)
				strUnixUser = pwd->pw_name;
			else
				strUnixUser = stringify(getuid());
			if (!bSuppress) g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Current uid:%d username:%s", getuid(), strUnixUser.c_str());
			break;

		default:
			if (!bSuppress) g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to login for user %ls, error code: 0x%08X", szUsername, hr);
			break;
		}
		goto exit;
	}

exit:
	return hr;
}

/** 
 * Run rules on a message and/or send an oof email before writing it
 * to the server.
 * 
 * @param[in] lpAdrBook Addressbook to use during rules
 * @param[in] lpStore Store the message will be written too
 * @param[in] lpInbox Inbox of the user message is being delivered to
 * @param[in] lpFolder Actual delivery folder of message
 * @param[in,out] lppMessage message being delivered, can return another message due to rules
 * @param[in] lpRecip recipient that is delivered to
 * @param[in] lpArgs delivery options
 * 
 * @return MAPI Error code
 */
HRESULT HrPostDeliveryProcessing(PyMapiPlugin *lppyMapiPlugin, LPADRBOOK lpAdrBook, LPMDB lpStore, IMAPIFolder *lpInbox, IMAPIFolder *lpFolder, IMessage **lppMessage, ECRecipient *lpRecip, DeliveryArgs *lpArgs)
{
	HRESULT hr = hrSuccess;
	IMAPISession *lpUserSession = NULL;
	SPropValuePtr ptrProp;

	hr = HrOpenECSession(&lpUserSession,
						 lpRecip->wstrUsername.c_str(), L"", lpArgs->strPath.c_str(), EC_PROFILE_FLAGS_NO_NOTIFICATIONS,
						 g_lpConfig->GetSetting("sslkey_file","",NULL), g_lpConfig->GetSetting("sslkey_pass","",NULL));
	if (hr != hrSuccess)
		goto exit;

	if(FNeedsAutoAccept(lpStore, *lppMessage)) {
		g_lpLogger->Log(EC_LOGLEVEL_INFO, "Starting MR autoaccepter");

		hr = HrAutoAccept(g_lpLogger, lpUserSession, lpRecip, lpStore, *lppMessage);
		
		if(hr == hrSuccess) {
			g_lpLogger->Log(EC_LOGLEVEL_INFO, "Autoaccept processing completed successfully. Skipping further processing.");
			// The MR autoaccepter has processed the message. Skip any further work on this message: dont
			// run rules and dont send new mail notifications (The message should be deleted now)
			hr = MAPI_E_CANCEL;
			goto exit;
		} else {
			g_lpLogger->Log(EC_LOGLEVEL_INFO, "Autoaccept processing failed, proceeding with rules processing.");
			// The MR autoaccepter did not run properly. This could be correct behaviour; for example the
			// autoaccepter may want to defer accepting to a human controller. This means we have to continue
			// processing as if the autoaccepter was not used
			hr = hrSuccess;
		}
	}
	
	if (lpFolder == lpInbox) {
		// process rules for the inbox
		hr = HrProcessRules(lppyMapiPlugin, lpUserSession, lpAdrBook, lpStore, lpInbox, lppMessage, g_lpLogger);
		if (hr == MAPI_E_CANCEL)
			g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Message canceled by rule");
		else if (hr != hrSuccess)
			g_lpLogger->Log(EC_LOGLEVEL_WARNING, "Unable to process rules, error code: 0x%08X",hr);
		// continue, still send possible out-of-office message
	}

	// do not send vacation message for junk messages
	if (lpArgs->ulDeliveryMode != DM_JUNK &&
	// do not send vacation message on delegated messages
		(HrGetOneProp(*lppMessage, PR_DELEGATED_BY_RULE, &ptrProp) != hrSuccess || ptrProp->Value.b == FALSE) )
	{
		SendOutOfOffice(lpAdrBook, lpStore, *lppMessage, lpRecip, lpArgs->strAutorespond);
	}

exit:
	if (lpUserSession)
		lpUserSession->Release();

	return hr;
}

/** 
 * Find spam header if needed, and mark delivery as spam delivery if
 * header found.
 * 
 * @param[in] strMail rfc2822 email being delivered
 * @param[in,out] lpArgs delivery options
 * 
 * @return MAPI Error code
 */
HRESULT FindSpamMarker(const std::string &strMail, DeliveryArgs *lpArgs)
{
	HRESULT hr = hrSuccess;
	char *szHeader = g_lpConfig->GetSetting("spam_header_name","",NULL);
	char *szValue = g_lpConfig->GetSetting("spam_header_value","",NULL);
	size_t end, pos;
	string match;
	string strHeaders;

	if (!szHeader || !szValue)
		goto exit;

	// find end of headers
	end = strMail.find("\r\n\r\n");
	if (end == string::npos)
		goto exit;
	end += 2;

	// copy headers in upper case, need to resize destination first
	strHeaders.resize(end);
	transform(strMail.begin(), strMail.begin() +end, strHeaders.begin(), ::toupper);

	match = string("\r\n") + szHeader;
	transform(match.begin(), match.end(), match.begin(), ::toupper);
	// find header
	pos = strHeaders.find(match.c_str());
	if (pos == string::npos)
		goto exit;

	// skip header and find end of line
	pos += match.length();
	end = strHeaders.find("\r\n", pos);

	match = szValue;
	transform(match.begin(), match.end(), match.begin(), ::toupper);
	// find value in header line (no header continuations supported here)
	pos = strHeaders.find(match.c_str(), pos);

	if (pos == string::npos || pos > end)
		goto exit;

	// found, override delivery to junkmail folder
	lpArgs->ulDeliveryMode = DM_JUNK;
	g_lpLogger->Log(EC_LOGLEVEL_INFO, "Spam marker found in e-mail, delivering to Junk-mail folder");

exit:
	return hr;
}

/** 
 * Deliver an email (source is either rfc2822 or previous delivered
 * mapi message) to a specific recipient.
 * 
 * @param[in] lpSession MAPI session (user session when not in LMTP mode, else admin session)
 * @param[in] lpStore default store for lpSession (user store when not in LMTP mode, else admin store)
 * @param[in] bIsAdmin indicates that lpSession and lpStore are an admin session and store (true in LMTP mode)
 * @param[in] lpAdrBook Global Addressbook
 * @param[in] lpOrigMessage a previously delivered message, if any
 * @param[in] bFallbackDelivery previously delivered message was a fallback message
 * @param[in] strMail original received rfc2822 email
 * @param[in] lpRecip recipient to deliver message to
 * @param[in] lpArgs delivery options
 * @param[out] lppMessage the newly delivered message
 * @param[out] lpbFallbackDelivery newly delivered message is a fallback message
 * 
 * @return MAPI Error code
 */
HRESULT ProcessDeliveryToRecipient(PyMapiPlugin *lppyMapiPlugin, IMAPISession *lpSession, IMsgStore *lpStore, bool bIsAdmin, LPADRBOOK lpAdrBook, IMessage *lpOrigMessage, bool bFallbackDelivery, std::string &strMail, ECRecipient *lpRecip, DeliveryArgs *lpArgs, IMessage **lppMessage, bool *lpbFallbackDelivery)
{
	HRESULT hr = hrSuccess;
	LPMDB lpTargetStore = NULL;
	IMAPIFolder *lpTargetFolder = NULL;
	IMAPIFolder *lpFolder = NULL;
	IMAPIFolder *lpInbox = NULL;
	IMessage *lpDeliveryMessage = NULL;
	IMessage *lpMessageTmp = NULL;
	IABContainer *lpAddrDir = NULL;
	ULONG ulResult = 0;
	ULONG ulNewMailNotify = 0;

	// single user deliver did not lookup the user
	if (lpRecip->strSMTP.empty()) {
		hr = OpenResolveAddrFolder(lpAdrBook, &lpAddrDir);
		if (hr != hrSuccess)
			goto exit;

		hr = ResolveUser(lpArgs, lpAddrDir, lpRecip);
		if (hr != hrSuccess)
			goto exit;
	}

	hr = HrGetDeliveryStoreAndFolder(lpSession, lpStore, lpRecip, lpArgs, &lpTargetStore, &lpInbox, &lpTargetFolder);
	if (hr != hrSuccess)
		goto exit;

	if (!lpOrigMessage) {
		/* No message was provided, we have to construct it personally */
		bool bExpired = false;

		hr = HrCreateMessage(lpTargetFolder, lpInbox, &lpFolder, &lpMessageTmp);
		if (hr != hrSuccess)
			goto exit;

		hr = HrStringToMAPIMessage(strMail, lpSession, lpTargetStore, lpAdrBook, lpFolder, lpMessageTmp, lpRecip, lpArgs, &lpDeliveryMessage, &bFallbackDelivery);
		if (hr != hrSuccess)
			goto exit;

		/*
		 * Check if the message has expired.
		 */
		hr = HrMessageExpired(lpDeliveryMessage, &bExpired);
		if (hr != hrSuccess)
			goto exit;
		if (bExpired) {
			/* Set special error code for callers */
			hr = MAPI_W_CANCEL_MESSAGE;
			goto exit;
		}

		hr = lppyMapiPlugin->MessageProcessing("PostConverting", lpSession, lpAdrBook, NULL, NULL, lpDeliveryMessage, &ulResult);
		if (hr != hrSuccess)
			goto exit;

		// TODO do something with ulResult

	} else {
		/* Copy message to prepare for new delivery */
		hr = HrCopyMessageForDelivery(lpOrigMessage, lpTargetFolder, lpRecip, lpArgs, lpInbox, bFallbackDelivery, &lpFolder, &lpDeliveryMessage);
		if (hr != hrSuccess)
			goto exit;
	}

	hr = HrOverrideRecipProps(lpDeliveryMessage, lpRecip);
	if (hr != hrSuccess)
		goto exit;

	if (bFallbackDelivery) {
		hr = HrOverrideFallbackProps(lpDeliveryMessage, lpRecip);
		if (hr != hrSuccess)
			goto exit;
	} else {
		hr = HrOverrideReceivedByProps(lpDeliveryMessage, lpRecip);
		if (hr != hrSuccess)
			goto exit;
	}

	hr = lppyMapiPlugin->MessageProcessing("PreDelivery", lpSession, lpAdrBook, lpTargetStore, lpTargetFolder, lpDeliveryMessage, &ulResult);
	if (hr != hrSuccess)
		goto exit;

	// TODO do something with ulResult
	if (ulResult == MP_STOP_SUCCESS) {
	    if (lppMessage) lpDeliveryMessage->QueryInterface(IID_IMessage, (void**)lppMessage);
    	if (lpbFallbackDelivery) *lpbFallbackDelivery = bFallbackDelivery;

		goto exit;
	}

	// Do rules & out-of-office
	hr = HrPostDeliveryProcessing(lppyMapiPlugin, lpAdrBook, lpTargetStore, lpInbox, lpTargetFolder, &lpDeliveryMessage, lpRecip, lpArgs);
	if (hr != MAPI_E_CANCEL) {
		// ignore other errors for rules, still want to save the delivered message
		// Save message changes, message becomes visible for the user
		hr = lpDeliveryMessage->SaveChanges(KEEP_OPEN_READWRITE);
		if (hr != hrSuccess) {
			if (hr == MAPI_E_STORE_FULL) {
				// make sure the error is printed on stderr, so this will be bounced as error by the MTA.
				// use cerr to avoid quiet mode.
				fprintf(stderr, "Store of user %ls is over quota limit.\n", lpRecip->wstrUsername.c_str());
			} else
				g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to commit message: 0x%08X", hr);
			goto exit;
		}

		hr = lppyMapiPlugin->MessageProcessing("PostDelivery", lpSession, lpAdrBook, lpTargetStore, lpTargetFolder, lpDeliveryMessage, &ulResult);
		if (hr != hrSuccess)
			goto exit;
		// TODO do something with ulResult

		if (parseBool(g_lpConfig->GetSetting("archive_on_delivery"))) {
			MAPISessionPtr ptrAdminSession;
			ArchivePtr ptrArchive;

			if (bIsAdmin)
				hr = lpSession->QueryInterface(ptrAdminSession.iid, &ptrAdminSession);
			else {
				string strServer = g_lpConfig->GetSetting("server_socket");
				strServer = GetServerUnixSocket((char*)strServer.c_str()); // let environment override if present

				hr = HrOpenECAdminSession(&ptrAdminSession, strServer.c_str(), EC_PROFILE_FLAGS_NO_NOTIFICATIONS,
										  g_lpConfig->GetSetting("sslkey_file","",NULL), g_lpConfig->GetSetting("sslkey_pass","",NULL));
			}
			if (hr != hrSuccess) {
				g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to open admin session for archive access: 0x%08X", hr);
				goto exit;
			}

			hr = Archive::Create(ptrAdminSession, g_lpLogger, &ptrArchive);
			if (hr != hrSuccess) {
				g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to instantiate archive object: 0x%08X", hr);
				goto exit;
			}

			hr = ptrArchive->HrArchiveMessageForDelivery(lpDeliveryMessage);
			if (hr != hrSuccess) {
				g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to archive message: 0x%08X", hr);
				Util::HrDeleteMessage(lpSession, lpDeliveryMessage);
				goto exit;
			}
		}

		if (lpArgs->bNewmailNotify) {

			ulNewMailNotify = TRUE;

			hr = lppyMapiPlugin->RequestCallExecution("SendNewMailNotify",  lpSession, lpAdrBook, lpTargetStore, lpTargetFolder, lpDeliveryMessage, &ulNewMailNotify, &ulResult);
			if (hr != hrSuccess) {
				// Plugin failed so fallback on the original state
				ulNewMailNotify = lpArgs->bNewmailNotify;
				hr = hrSuccess;
			}

			if (ulNewMailNotify == true) {
				hr = HrNewMailNotification(lpTargetStore, lpDeliveryMessage);
				if (hr != hrSuccess)
					g_lpLogger->Log(EC_LOGLEVEL_WARNING, "Unable to send 'New Mail' notification, error code: 0x%08X", hr);
				else
					g_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Send 'New Mail' notification");

				hr = hrSuccess;
			}
		}
	}

	if (lppMessage)
		lpDeliveryMessage->QueryInterface(IID_IMessage, (void**)lppMessage);

	if (lpbFallbackDelivery)
		*lpbFallbackDelivery = bFallbackDelivery;

exit:
	if (lpMessageTmp)
		lpMessageTmp->Release();

	if (lpDeliveryMessage)
		lpDeliveryMessage->Release();

	if (lpInbox)
		lpInbox->Release();

	if (lpFolder)
		lpFolder->Release();

	if (lpTargetFolder)
		lpTargetFolder->Release();

	if (lpTargetStore)
		lpTargetStore->Release();

	if (lpAddrDir)
		lpAddrDir->Release();

	return hr;
}

/** 
 * Log that the message was expired, and send that response for every given LMTP received recipient
 * 
 * @param[in] start Start of recipient list
 * @param[in] end End of recipient list
 */
void RespondMessageExpired(recipients_t::iterator start, recipients_t::iterator end)
{
	convert_context converter;
	g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Message was expired, not delivering");
	for (recipients_t::iterator iter = start; iter != end; iter++)
		(*iter)->wstrDeliveryStatus = L"250 2.4.7 %ls Delivery time expired";
}

/** 
 * For a specific zarafa server, deliver the same message to a list of
 * recipients. This makes sure this message is correctly single
 * instanced on this server.
 * 
 * @param[in] lpUserSession optional session of one user the message is being delivered to (cmdline dagent, NULL on LMTP mode)
 * @param[in] lpMessage an already delivered message
 * @param[in] bFallbackDelivery already delivered message is an fallback message
 * @param[in] strMail the rfc2822 received email
 * @param[in] strServer uri of the zarafa server to connect to
 * @param[in] listRecipients list of recipients present on the server connecting to
 * @param[in] lpAdrBook Global addressbook
 * @param[in] lpArgs delivery options
 * @param[out] lppMessage The newly delivered message
 * @param[out] lpbFallbackDelivery newly delivered message is a fallback message
 * 
 * @return MAPI Error code
 */
HRESULT ProcessDeliveryToServer(PyMapiPlugin *lppyMapiPlugin, IMAPISession *lpUserSession, IMessage *lpMessage, bool bFallbackDelivery, std::string &strMail, const std::string &strServer, recipients_t &listRecipients, LPADRBOOK lpAdrBook, DeliveryArgs *lpArgs, IMessage **lppMessage, bool *lpbFallbackDelivery)
{
	HRESULT hr = hrSuccess;
	IMAPISession *lpSession = NULL;
	IMsgStore *lpStore = NULL;
	recipients_t::iterator iter;
	IMessage *lpOrigMessage = NULL;
	IMessage *lpMessageTmp = NULL;
	bool bFallbackDeliveryTmp = false;
	convert_context converter;

	// if we already had a message, we can create a copy.
	if (lpMessage)
		lpMessage->QueryInterface(IID_IMessage, (void**)&lpOrigMessage);

	if (lpUserSession)
		hr = lpUserSession->QueryInterface(IID_IMAPISession, (void **)&lpSession);
	else
		hr = HrOpenECAdminSession(&lpSession, strServer.c_str(), EC_PROFILE_FLAGS_NO_NOTIFICATIONS,
								  g_lpConfig->GetSetting("sslkey_file","",NULL), g_lpConfig->GetSetting("sslkey_pass","",NULL));
	if (hr != hrSuccess || (hr = HrOpenDefaultStore(lpSession, &lpStore)) != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to open default store for system account, error code: 0x%08X", hr);

		// notify LMTP client soft error to try again later
		for (iter = listRecipients.begin(); iter != listRecipients.end(); iter++) {
			// error will be shown in postqueue status in postfix, probably too in other serves and mail syslog service
			(*iter)->wstrDeliveryStatus = L"450 4.5.0 %ls network or permissions error to zarafa server: " + wstringify(hr, true);
		}

		goto exit;
	}

	for (iter = listRecipients.begin(); iter != listRecipients.end(); iter++) {
		/*
		 * Normal error codes must be ignored, since we want to attempt to deliver the email to all users,
		 * however when the error code MAPI_W_CANCEL_MESSAGE was provided, the message has expired and it is
		 * pointles to continue delivering the mail. However we must continue looping through all recipients
		 * to inform the MTA we did handle the email properly.
		 */
		hr = ProcessDeliveryToRecipient(lppyMapiPlugin, lpSession, lpStore, lpUserSession == NULL, lpAdrBook, lpOrigMessage, bFallbackDelivery, strMail, *iter, lpArgs, &lpMessageTmp, &bFallbackDeliveryTmp);
		if (hr == hrSuccess || hr == MAPI_E_CANCEL) {
			if (hr == hrSuccess) {
				LPSPropValue lpMessageId = NULL;
				wstring wMessageId;
				if (HrGetOneProp(lpMessageTmp, PR_INTERNET_MESSAGE_ID_W, &lpMessageId) == hrSuccess) {
					wMessageId = lpMessageId->Value.lpszW;
					MAPIFreeBuffer(lpMessageId);
				}
				g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Delivered message to '%ls', Message-Id: %ls", (*iter)->wstrUsername.c_str(), wMessageId.c_str());
			}
			// cancel already logged.
			hr = hrSuccess;

			(*iter)->wstrDeliveryStatus = L"250 2.1.5 %ls Ok";
		} else if (hr == MAPI_W_CANCEL_MESSAGE) {
			/* Loop through all remaining recipients and start responding the status to LMTP */
			RespondMessageExpired(iter, listRecipients.end());
			goto exit;
		} else {
			g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to deliver message to '%ls', error code: 0x%08X", (*iter)->wstrUsername.c_str(), hr);
				
			/* LMTP requires different notification when Quota for user was exceeded */
			if (hr == MAPI_E_STORE_FULL)
				(*iter)->wstrDeliveryStatus = L"552 5.2.2 %ls Quota Exceeded";
			else
				(*iter)->wstrDeliveryStatus = L"450 4.2.0 %ls Mailbox Temporarily Unavailable";
		}

		if (lpMessageTmp) {
			if (lpOrigMessage == NULL) {
				// If we delivered the message for the first time,
				// we keep the intermediate message to make copies of.
				lpMessageTmp->QueryInterface(IID_IMessage, (void**)&lpOrigMessage);
			}
			bFallbackDelivery = bFallbackDeliveryTmp;

			lpMessageTmp->Release();
		}
		lpMessageTmp = NULL;
	}

	if (lppMessage && lpOrigMessage) {
		lpOrigMessage->QueryInterface(IID_IMessage, (void**)lppMessage);
	}

	if (lpbFallbackDelivery)
		*lpbFallbackDelivery = bFallbackDelivery;

exit:
	if (lpMessageTmp)
		lpMessageTmp->Release();

	if (lpOrigMessage)
		lpOrigMessage->Release();

	if (lpStore)
		lpStore->Release();

	if (lpSession)
		lpSession->Release();

	return hr;
}

/** 
 * Commandline dagent delivery entrypoint.
 * Deliver an email to one recipient.
 *
 * Although this function is passed a recipient list, it's only
 * because the rest of the functions it calls requires this and the
 * caller of this function already has a list.
 * 
 * @param[in] lpSession User MAPI session
 * @param[in] lpAdrBook Global addressbook
 * @param[in] fp input file which contains the email to deliver
 * @param[in] lstSingleRecip list of recipients to deliver email to (one user)
 * @param[in] lpArgs delivery options
 * 
 * @return MAPI Error code
 */
HRESULT ProcessDeliveryToSingleRecipient(PyMapiPlugin *lppyMapiPlugin, IMAPISession *lpSession, LPADRBOOK lpAdrBook, FILE *fp, recipients_t &lstSingleRecip, DeliveryArgs *lpArgs)
{
	HRESULT hr = hrSuccess;
	std::string strMail;

	/* Always start at the beginning of the file */
	rewind(fp);

	/* Read file into string */
	hr = HrMapFileToString(fp, &strMail);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to map input to memory");
		goto exit;
	}

	FindSpamMarker(strMail, lpArgs);
	
	hr = ProcessDeliveryToServer(lppyMapiPlugin, lpSession, NULL, false, strMail, lpArgs->strPath, lstSingleRecip, lpAdrBook, lpArgs, NULL, NULL);

exit:
	return hr;
}

/** 
 * Deliver email from file to a list of recipients which are grouped
 * by server.
 * 
 * @param[in] lpSession Admin MAPI Session
 * @param[in] lpAdrBook Addressbook
 * @param[in] fp file containing the received email
 * @param[in] lpServerNameRecips recipients grouped by server
 * @param[in] lpArgs delivery options
 * 
 * @return MAPI Error code
 */
HRESULT ProcessDeliveryToCompany(PyMapiPlugin *lppyMapiPlugin, IMAPISession *lpSession, LPADRBOOK lpAdrBook, FILE *fp, serverrecipients_t *lpServerNameRecips, DeliveryArgs *lpArgs)
{
	HRESULT hr = hrSuccess;
	IMessage *lpMasterMessage = NULL;
	std::string strMail;
	serverrecipients_t listServerPathRecips;
	serverrecipients_t::iterator iter;
	bool bFallbackDelivery = false;
	bool bExpired = false;

	if (!lpServerNameRecips) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	/* Always start at the beginning of the file */
	rewind(fp);

	/* Read file into string */
	hr = HrMapFileToString(fp, &strMail);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to map input to memory");
		goto exit;
	}

	FindSpamMarker(strMail, lpArgs);

	hr = ResolveServerToPath(lpSession, lpServerNameRecips, lpArgs->strPath, &listServerPathRecips);
	if (hr != hrSuccess)
		goto exit;

	for (iter = listServerPathRecips.begin(); iter != listServerPathRecips.end(); iter++) {
		IMessage *lpMessageTmp = NULL;
		bool bFallbackDeliveryTmp = false;

		if (!bExpired) {
			hr = ProcessDeliveryToServer(lppyMapiPlugin, NULL, lpMasterMessage, bFallbackDelivery, strMail, convert_to<string>(iter->first), iter->second, lpAdrBook, lpArgs, &lpMessageTmp, &bFallbackDeliveryTmp);
			if (hr == MAPI_W_CANCEL_MESSAGE) {
				bExpired =  true;
				/* Don't report the error further */
				hr = hrSuccess;
			} else if (hr != hrSuccess) {
				g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Unable to deliver all messages for server '%ls'", iter->first.c_str());
			}

			/* lpMessage is our base message which we will copy to each server/recipient */
			if (lpMessageTmp) {
				if (lpMasterMessage == NULL) {
					// keep message to make copies of on the same server
					lpMessageTmp->QueryInterface(IID_IMessage, (void**)&lpMasterMessage);
				}

				bFallbackDelivery = bFallbackDeliveryTmp;

				lpMessageTmp->Release();
			}
			lpMessageTmp = NULL;
		} else {
			/* Simply loop through all recipients to respond to LMTP */
			RespondMessageExpired(iter->second.begin(), iter->second.end());
		}
	}

	g_lpLogger->Log(EC_LOGLEVEL_INFO, "Finished processing message");

exit:
	if (lpMasterMessage)
		lpMasterMessage->Release();

	return hr;
}

/** 
 * Within a company space, find the recipient with the lowest
 * administrator rights. This user can be used to open the Global
 * Addressbook.
 * 
 * @param[in] lpServerRecips all recipients to deliver for within a company
 * @param[out] lppRecipient a recipient with the rights lower than server admin
 * 
 * @return MAPI Error code 
 */
HRESULT FindLowestAdminLevel(serverrecipients_t *lpServerRecips, ECRecipient **lppRecipient)
{
	HRESULT hr = hrSuccess;
	serverrecipients_t::iterator iterSRV;
	recipients_t::iterator iterRCP;
	ECRecipient *lpRecip = NULL;

	for (iterSRV = lpServerRecips->begin(); iterSRV != lpServerRecips->end(); iterSRV++) {
		for (iterRCP = iterSRV->second.begin(); iterRCP != iterSRV->second.end(); iterRCP++) {
			if ((*iterRCP)->ulDisplayType == DT_REMOTE_MAILUSER)
				continue;
			else if (!lpRecip)
				lpRecip = *iterRCP;
			else if ((*iterRCP)->ulAdminLevel < lpRecip->ulAdminLevel)
				lpRecip = *iterRCP;

			if (lpRecip->ulAdminLevel < 2)
				goto found;
		}
	}

found:
	if (lpRecip)
		*lppRecipient = lpRecip;
	else
		hr = MAPI_E_NOT_FOUND; /* This only happens if there are no recipients or everybody is a contact */

	return hr;
}

/** 
 * LMTP delivery entry point
 * Deliver email to a list of recipients, grouped by company, grouped by server.
 * 
 * @param[in] lpSession Admin MAPI Session
 * @param[in] fp file containing email to deliver
 * @param[in] lpCompanyRecips list of all recipients to deliver to
 * @param[in] lpArgs delivery options
 * 
 * @return MAPI Error code
 */
HRESULT ProcessDeliveryToList(PyMapiPlugin *lppyMapiPlugin, IMAPISession *lpSession, FILE *fp, companyrecipients_t *lpCompanyRecips, DeliveryArgs *lpArgs)
{
	HRESULT hr = hrSuccess;
	IMAPISession *lpUserSession = NULL;
	LPADRBOOK lpAdrBook = NULL;
	ECRecipient *lpRecip = NULL;
	companyrecipients_t::iterator iter;

	/*
	 * Find user with lowest adminlevel, we will use the addressbook for this
	 * user to make sure the recipient resolving for all recipients for the company
	 * resolving will occur with the minimum set of view-levels to other
	 * companies.
	 */
	for (iter = lpCompanyRecips->begin(); iter != lpCompanyRecips->end(); iter++) {
		hr = FindLowestAdminLevel(&iter->second, &lpRecip);
		if (hr != hrSuccess)
			goto exit;


		hr = HrGetSession(lpArgs, lpRecip->wstrUsername.c_str(), &lpUserSession);
		if (hr != hrSuccess)
			goto exit;

		hr = OpenResolveAddrFolder(lpUserSession, &lpAdrBook, NULL);
		if (hr != hrSuccess)
			goto exit;

		hr = ProcessDeliveryToCompany(lppyMapiPlugin, lpSession, lpAdrBook, fp, &iter->second, lpArgs);
		if (hr != hrSuccess)
			goto exit;

		if (lpAdrBook)
			lpAdrBook->Release();
		lpAdrBook = NULL;

		if (lpUserSession)
			lpUserSession->Release();
		lpUserSession = NULL;
	}

exit:
	if (lpAdrBook)
		lpAdrBook->Release();

	if (lpUserSession)
		lpUserSession->Release();

	return hr;
}

/** 
 * Handle an incoming LMTP connection
 * 
 * @param[in] lpArg delivery options
 * 
 * @return NULL
 */
void *HandlerLMTP(void *lpArg) {
	DeliveryArgs *lpArgs = (DeliveryArgs *) lpArg;
	std::string strMailAddress;
	companyrecipients_t mapRCPT;
	std::string inBuffer;
	HRESULT hr = hrSuccess;
	bool bLMTPQuit = false;
	int timeouts = 0;
	PyMapiPluginFactory pyMapiPluginFactory;

	LMTP lmtp(lpArgs->lpChannel, (char *)lpArgs->strPath.c_str(), g_lpLogger, g_lpConfig);

	/* For resolving addresses from Address Book */
	IMAPISession *lpSession = NULL;
	LPADRBOOK	 lpAdrBook = NULL;
	IABContainer *lpAddrDir = NULL;

	g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Starting worker for LMTP request pid %d", getpid());

	char *lpEnvGDB  = getenv("GDB");
	if (lpEnvGDB && parseBool(lpEnvGDB)) {
		Sleep(10000); //wait 10 seconds so you can attach gdb
		g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Starting worker for LMTP request");
	}
	
	hr = pyMapiPluginFactory.Init(g_lpConfig, g_lpLogger);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to instantiate plugin factory, hr=0x%08x", hr);
		goto exit;
	}

	hr = HrGetSession(lpArgs, ZARAFA_SYSTEM_USER_W, &lpSession);
	if (hr != hrSuccess)
		goto exit;

	hr = OpenResolveAddrFolder(lpSession, &lpAdrBook, &lpAddrDir);
	if (hr != hrSuccess)
		goto exit;

	// Send hello message
	
	lmtp.HrResponse("220 2.1.5 LMTP server is ready");

	while (!bLMTPQuit && !g_bQuit) {
		LMTP_Command eCommand;

		hr = lpArgs->lpChannel->HrSelect(60);
		
		if(hr == MAPI_E_TIMEOUT) {
			if(timeouts < 10) {
				timeouts++;
				continue;
			}

			lmtp.HrResponse("221 5.0.0 Connection closed due to timeout");
			g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Connection closed due to timeout");
			bLMTPQuit = true;
			
			break;
		} else if (hr == MAPI_E_NETWORK_ERROR) {
			g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Socket Error: %s", strerror(errno));
			bLMTPQuit = true;
			
			break;
		}

		timeouts = 0;
		inBuffer.clear();

		errno = 0;				// clear errno, might be from double logoff to server
		hr = lpArgs->lpChannel->HrReadLine(&inBuffer);
		if (hr != hrSuccess){
			if (errno)
				g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Failed to read line: %s", strerror(errno));
			else
				g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Client disconnected");
				
			bLMTPQuit = true;
			break;
		}
			
		if (g_bQuit) {
			lmtp.HrResponse("221 2.0.0 Server is shutting down");
			bLMTPQuit = true;
			hr = MAPI_E_CALL_FAILED;
			break;
		}

		if (g_lpLogger->Log(EC_LOGLEVEL_DEBUG))
			g_lpLogger->Log(EC_LOGLEVEL_DEBUG, "Command received: " + inBuffer);

		hr = lmtp.HrGetCommand(inBuffer, eCommand);	
		if (hr != hrSuccess) {
			lmtp.HrResponse("555 5.5.4 Command not recognized");
			continue;
		}

		switch (eCommand) {
		case LMTP_Command_LHLO:
			if (lmtp.HrCommandLHLO(inBuffer) == hrSuccess) {
				lmtp.HrResponse("250-SERVER ready"); 
				lmtp.HrResponse("250-PIPELINING");
				lmtp.HrResponse("250-ENHANCEDSTATUSCODE");
				lmtp.HrResponse("250 RSET");
			} else {
				lmtp.HrResponse("501 5.5.4 Syntax: LHLO hostname");
			}				
			break;

		case LMTP_Command_MAIL_FROM:
			// @todo, if this command is received a second time, repond: 503 5.5.1 Error: nested MAIL command
			if (lmtp.HrCommandMAILFROM(inBuffer) != hrSuccess)
				lmtp.HrResponse("503 5.1.7 Bad sender's mailbox address syntax");
			else
				lmtp.HrResponse("250 2.1.0 Ok");
			break;

		case LMTP_Command_RCPT_TO:
			if (lmtp.HrCommandRCPTTO(inBuffer, &strMailAddress) != hrSuccess) {
				lmtp.HrResponse("503 5.1.3 Bad destination mailbox address syntax");
			} else {
				ECRecipient *lpRecipient = new ECRecipient(convert_to<std::wstring>(strMailAddress));
						
				// Resolve the mail address, so to have a user name instead of a mail address
				hr = ResolveUser(lpArgs, lpAddrDir, lpRecipient);
				if (hr == hrSuccess) {
					// This is the status until it is delivered or some other error occurs
					lpRecipient->wstrDeliveryStatus = L"450 4.2.0 %ls Mailbox Temporarily Unavailable";
					
					hr = AddServerRecipient(&mapRCPT, &lpRecipient);
					if (hr != hrSuccess)
						lmtp.HrResponse("503 5.1.1 Failed to add user to recipients");
					else
						lmtp.HrResponse("250 2.1.5 Ok");
				} else {
					if (hr == MAPI_E_NOT_FOUND) {
						g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Requested e-mail address '%s' does not resolve a user.", strMailAddress.c_str());
						lmtp.HrResponse("503 5.1.1 User does not exist");
					} else {
						g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Failed to lookup email address, error: 0x%08X", hr);
						lmtp.HrResponse("503 5.1.1 Connection error: "+stringify(hr,1));
					}
				}

				/*
				 * If recipient resolving failed, we need to free the recipient structure,
				 * only when the structure was added to the mapRCPT will it be freed automatically
				 * later during email delivery.
				 */
				if (lpRecipient)
					delete lpRecipient;
			}
			break;

		case LMTP_Command_DATA:
			{
				if (mapRCPT.empty()) {
					lmtp.HrResponse("503 5.1.1 No recipients");
					break;
				}

				FILE *tmp = tmpfile();
				if (!tmp) {
					lmtp.HrResponse("503 5.1.1 Internal error during delivery");
					g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Unable to create temp file for email delivery. Please check write-access in /tmp directory.");
					break;
				}

				hr = lmtp.HrCommandDATA(tmp);
				if (hr == hrSuccess) {

					PyMapiPluginAPtr ptrPyMapiPlugin;
					hr = GetPluginObject(g_lpLogger, &pyMapiPluginFactory, &ptrPyMapiPlugin);
					if (hr != hrSuccess) {
						lmtp.HrResponse("503 5.1.1 Internal error during delivery");
						fclose(tmp);
						hr = hrSuccess;
						break;
					}					

					// During delivery lpArgs->ulDeliveryMode can be set to DM_JUNK. However it won't reset it
					// if required. So make sure to reset it here so we can safely reuse the LMTP connection
					delivery_mode ulDeliveryMode = lpArgs->ulDeliveryMode;

					hr = ProcessDeliveryToList(ptrPyMapiPlugin, lpSession, tmp, &mapRCPT, lpArgs);

					SaveRawMessage(tmp, "LMTP");
					lpArgs->ulDeliveryMode = ulDeliveryMode;
				}
					
				// We're not that interested in the error value here; if an error occurs then this will be reflected in the
				// wstrDeliveryStatus of each recipient.
				hr = hrSuccess;
					
				fclose(tmp);
					
				for(companyrecipients_t::iterator iCompany = mapRCPT.begin(); iCompany != mapRCPT.end(); iCompany++) {
					for(serverrecipients_t::iterator iServer = iCompany->second.begin(); iServer != iCompany->second.end(); iServer++) {
						for(recipients_t::iterator iRecipient = iServer->second.begin(); iRecipient != iServer->second.end(); iRecipient++) {
							std::vector<std::wstring>::iterator i;
							WCHAR wbuffer[4096];
							for (i = (*iRecipient)->vwstrRecipients.begin(); i != (*iRecipient)->vwstrRecipients.end(); i++) {
								swprintf(wbuffer, arraySize(wbuffer), (*iRecipient)->wstrDeliveryStatus.c_str(), i->c_str());
								// rawsize([N]) returns N, not contents len
								lmtp.HrResponse(convert_to<string>(CHARSET_CHAR, wbuffer, rawsize((WCHAR*)wbuffer), CHARSET_WCHAR));
							}
						}
					}
				}

				// Reset RCPT TO list now
				FreeServerRecipients(&mapRCPT);
			}
			break;

		case LMTP_Command_RSET:
			// Reset RCPT TO list
			FreeServerRecipients(&mapRCPT);
			lmtp.HrResponse("250 2.1.0 Ok");
			break;

		case LMTP_Command_QUIT:
			lmtp.HrResponse("221 2.0.0 Bye");
			bLMTPQuit = 1;
			break;	
		}
	}

exit:
	FreeServerRecipients(&mapRCPT);

	if (lpAddrDir)
		lpAddrDir->Release();

	if (lpAdrBook)
		lpAdrBook->Release();

	if (lpSession)
		lpSession->Release();

	g_lpLogger->Log(EC_LOGLEVEL_FATAL, "LMTP thread exiting");

	if (lpArgs)
		delete lpArgs;

	return NULL;
}

/**
 * Runs the LMTP service daemon. Listens on the LMTP port for incoming
 * connections and starts a new thread or child process to handle the
 * connection.  Only accepts the incoming connection when the maximum
 * number of processes hasn't been reached.
 * 
 * @param[in]	servicename	Name of the service, used to create a unix pidfile.
 * @param[in]	bDaemonize	Starts a forked process in this loop to run in the background if true.
 * @param[in]	lpArgs		Struct containing delivery parameters
 * @retval MAPI error code	
 */
HRESULT running_service(char *servicename, bool bDaemonize, DeliveryArgs *lpArgs) 
{
	HRESULT hr = hrSuccess;
	int ulListenLMTP = 0;
	fd_set readfds;
	int err = 0;
	unsigned int nMaxThreads;
	int nCloseFDs = 0, pCloseFDs[1] = {0};

    stack_t st;
    struct sigaction act;
    memset(&st, 0, sizeof(st));
    memset(&act, 0, sizeof(act));

	nMaxThreads = atoui(g_lpConfig->GetSetting("lmtp_max_threads"));
	if (nMaxThreads == 0 || nMaxThreads == INT_MAX) {
		nMaxThreads = 20;
	}

	g_lpLogger->Log(EC_LOGLEVEL_INFO, "Maximum LMTP threads set to %d", nMaxThreads);

	
	// Setup sockets
	hr = HrListen(g_lpLogger, g_lpConfig->GetSetting("server_bind"), atoi(g_lpConfig->GetSetting("lmtp_port")), &ulListenLMTP);
	if (hr != hrSuccess)
		goto exit;
		
	g_lpLogger->Log(EC_LOGLEVEL_INFO, "Listening on port %s for LMTP", g_lpConfig->GetSetting("lmtp_port"));
	pCloseFDs[nCloseFDs++] = ulListenLMTP;

	// Setup signals
	signal(SIGTERM, sigterm);
	signal(SIGINT, sigterm);

	signal(SIGHUP, sighup);		// logrotate
	signal(SIGCHLD, sigchld);
	signal(SIGPIPE, SIG_IGN);

	// SIGSEGV backtrace support
    st.ss_sp = malloc(65536);
    st.ss_flags = 0;
    st.ss_size = 65536;
    act.sa_handler = sigsegv;
    act.sa_flags = SA_ONSTACK | SA_RESETHAND;
    sigaltstack(&st, NULL);

    sigaction(SIGSEGV, &act, NULL);
	sigaction(SIGBUS, &act, NULL);
	sigaction(SIGABRT, &act, NULL);

    // Set max open file descriptors to FD_SETSIZE .. higher than this number
    // is a bad idea, as it will start breaking select() calls.
	struct rlimit file_limit;
	file_limit.rlim_cur = FD_SETSIZE;
	file_limit.rlim_max = FD_SETSIZE;

	if(setrlimit(RLIMIT_NOFILE, &file_limit) < 0) {
		g_lpLogger->Log(EC_LOGLEVEL_FATAL, "WARNING: setrlimit(RLIMIT_NOFILE, %d) failed, you will only be able to connect up to %d sockets. Either start the process as root, or increase user limits for open file descriptors", FD_SETSIZE, getdtablesize());
	}

	// fork if needed and drop privileges as requested.
	// this must be done before we do anything with pthreads
	if (bDaemonize && unix_daemonize(g_lpConfig, g_lpLogger))
		goto exit;
	
	if (!bDaemonize)
		setsid();

	unix_create_pidfile(servicename, g_lpConfig, g_lpLogger);
	if (unix_runas(g_lpConfig, g_lpLogger))
		goto exit;

	g_lpLogger = StartLoggerProcess(g_lpConfig, g_lpLogger); // maybe replace logger

	g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Starting zarafa-dagent LMTP mode version " PROJECT_VERSION_DAGENT_STR " (" PROJECT_SVN_REV_STR "), pid %d", getpid());

	// Mainloop
	while (!g_bQuit) {
		FD_ZERO(&readfds);
		FD_SET(ulListenLMTP, &readfds);

		struct timeval timeout;
		timeout.tv_sec = 10;
		timeout.tv_usec = 0;
		err = select(ulListenLMTP + 1, &readfds, NULL, NULL, &timeout);

		if (err < 0) {
			if (errno != EINTR) {
				g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Socket error!");
				g_bQuit = true;
				hr = MAPI_E_NETWORK_ERROR;
			}

			continue;
		} else if (err == 0) {
			continue;
		}

		// don't start more "threads" that lmtp_max_threads config option
		if (g_nLMTPThreads == nMaxThreads) {
			Sleep(100);
			continue;
		}

		g_nLMTPThreads++;

		// One socket has signalled a new incoming connection
		DeliveryArgs *lpDeliveryArgs = new DeliveryArgs();
		*lpDeliveryArgs = *lpArgs;

		if (FD_ISSET(ulListenLMTP, &readfds)) {
			hr = HrAccept(g_lpLogger, ulListenLMTP, &lpDeliveryArgs->lpChannel);
			
			if (hr != hrSuccess) {
				// just keep running
				delete lpDeliveryArgs;
				hr = hrSuccess;
				continue;
			}

			if (unix_fork_function(HandlerLMTP, lpDeliveryArgs, nCloseFDs, pCloseFDs) < 0) {
				g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Could not create LMTP process.");
				// just keep running
			}
			// main handler always closes information it doesn't need
			delete lpDeliveryArgs;
			hr = hrSuccess;
		
			continue;
		}

		// should not be able to get here because of continues
		g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Incoming traffic was not for me??");
	}

	g_lpLogger->Log(EC_LOGLEVEL_FATAL, "LMTP service will now exit");

	// in forked mode, send all children the exit signal
	signal(SIGTERM, SIG_IGN);
	kill(0, SIGTERM);

	// wait max 30 seconds
	for (int i = 30; g_nLMTPThreads && i; i--) {
		if (i % 5 == 0)
			g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Waiting for %d processes to exit", g_nLMTPThreads);
		sleep(1);
	}

	if (g_nLMTPThreads)
		g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Forced shutdown with %d procesess left", g_nLMTPThreads);
	else
		g_lpLogger->Log(EC_LOGLEVEL_FATAL, "LMTP service shutdown complete");

	MAPIUninitialize();

exit:
	ECChannel::HrFreeCtx();


	if(st.ss_sp)
		free(st.ss_sp);
	return hr;
}

/**
 * Commandline delivery for one given user.
 *
 * Deliver the main received from <stdin> to the user named in the
 * recipient parameter. Valid recipient input is a username in current
 * locale, or the emailadress of the user.
 *
 * @param[in]	recipient	Username or emailaddress of a user, in current locale.
 * @param[in]	bStringEmail	true if we should strip everything from the @ to get the name to deliver to.
 * @param[in]	file		Filepointer to start of email.
 * @param[in]	lpArgs		Delivery arguments, according to given options on the commandline.
 * @return		MAPI Error code.
 */
HRESULT deliver_recipient(PyMapiPlugin *lppyMapiPlugin, char *recipient, bool bStringEmail, FILE *file, DeliveryArgs *lpArgs)
{
	HRESULT hr = hrSuccess;
	IMAPISession *lpSession = NULL;
	LPADRBOOK lpAdrBook = NULL;
	IABContainer *lpAddrDir = NULL;
	ECRecipient *lpSingleRecip = NULL;
	recipients_t lRCPT;
	std::string strUsername;
	std::wstring strwLoginname;
	FILE *fpMail = NULL;

	/* Make sure file uses CRLF */
	if(HrFileLFtoCRLF(file, &fpMail) != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_WARNING, "Unable to convert input to CRLF format");
		fpMail = file;
	}

	strUsername = recipient;
	if (bStringEmail) {
		// we have to strip off the @domainname.tld to get the username
		strUsername = strUsername.substr(0, strUsername.find_first_of("@"));
	}

	lpSingleRecip = new ECRecipient(convert_to<wstring>(strUsername));
	
	// Always try to resolve the user unless we just stripped an email address.
	if (!bStringEmail) {
		// only suppress error when it has no meaning (eg. delivery of unix user to itself)
		hr = HrGetSession(lpArgs, ZARAFA_SYSTEM_USER_W, &lpSession, !lpArgs->bResolveAddress);
		if (hr == hrSuccess) {
			hr = OpenResolveAddrFolder(lpSession, &lpAdrBook, &lpAddrDir);
			if (hr != hrSuccess)
				goto exit;
			
			hr = ResolveUser(lpArgs, lpAddrDir, lpSingleRecip);
			if (hr != hrSuccess) {
				if (hr == MAPI_E_NOT_FOUND)
					g_bTempfail = false;
				goto exit;
			}
		}
		
		else if (lpArgs->bResolveAddress) {
			// Failure to open the admin session will only result in error if resolve was requested.
			// Non fatal, so when config is changes the message can be delivered.
			goto exit;
		} else {
			// set commandline user in resolved name to deliver without resolve function
			lpSingleRecip->wstrUsername = lpSingleRecip->wstrRCPT;
		}
	} else {
		// set commandline user in resolved name to deliver without resolve function
		lpSingleRecip->wstrUsername = lpSingleRecip->wstrRCPT;
	}
	
	// Release admin session, and resolve folder.
	if (lpAdrBook) { lpAdrBook->Release(); lpAdrBook = NULL; }
	if (lpAddrDir) { lpAddrDir->Release(); lpAddrDir = NULL; }
	if (lpSession) { lpSession->Release(); lpSession = NULL; }

	hr = HrGetSession(lpArgs, lpSingleRecip->wstrUsername.c_str(), &lpSession);
	if (hr != hrSuccess) {
		if (hr == MAPI_E_LOGON_FAILED) {
			// This is a hard failure, two things could have happened
			// * strUsername does not exist
			// * user does exist, but dagent is not running with the correct SYSTEM privileges
			// Since we cannot detect the difference, we're handling both of these situations
			// as hard errors
			g_bTempfail = false;
		}
		goto exit;
	}
	
	hr = OpenResolveAddrFolder(lpSession, &lpAdrBook, &lpAddrDir);
	if (hr != hrSuccess)
		goto exit;
	
	lRCPT.insert(lpSingleRecip);
	hr = ProcessDeliveryToSingleRecipient(lppyMapiPlugin, lpSession, lpAdrBook, fpMail, lRCPT, lpArgs);

	// Over quota is a hard error
	if (hr == MAPI_E_STORE_FULL)
	    g_bTempfail = false;

	// Save copy of the raw message
	SaveRawMessage(fpMail, recipient);


exit:
	if (lpSingleRecip)
		delete lpSingleRecip;

	if (fpMail && fpMail != file)
		fclose(fpMail);

	if (lpSession)
		lpSession->Release();

	if (lpAdrBook)
		lpAdrBook->Release();

	if (lpAddrDir)
		lpAddrDir->Release();

	return hr;
}

void print_help(char *name) {
	cout << "Usage:\n" << endl;
	cout << name << " <recipient>" << endl;
	cout << " [-h|--host <serverpath>] [-c|--config <configfile>] [-f|--file <email-file>]" << endl;
	cout << " [-j|--junk] [-F|--folder <foldername>] [-P|--public <foldername>] [-p <separator>] [-C|--create]" << endl;
	cout<<	" [-d|--deamonize] [-l|--listen] [-r|--read] [--add-imap-data] [-s] [-v] [-q] [-e] [-n] [-R]" << endl;
	cout << endl;
	cout << "  <recipient> Username or e-mail address of recipient" << endl;
	cout << "  -f file\t read e-mail from file" << endl;
	cout << "  -h path\t path to connect to (e.g. file:///var/run/socket)" << endl;
	cout << "  -c filename\t Use configuration file (e.g. /etc/zarafa/dagent.cfg)\n\t\t Default: no config file used." << endl;
	cout << "  -j\t\t deliver in Junkmail" << endl;
	cout << "  -F foldername\t deliver in a subfolder of the store. Eg. 'Inbox\\sales'" << endl; 
	cout << "  -P foldername\t deliver in a subfolder of the public store. Eg. 'sales\\incoming'" << endl;
	cout << "  -p separator\t Override default path separator (\\). Eg. '-p % -F 'Inbox%dealers\\resellers'" << endl;
	cout << "  -C\t\t Create the subfolder if it does not exists. Default behaviour is to revert to the normal Inbox folder" << endl;
	cout << endl;
	cout << "  --add-imap-data Add IMAP optimizations during delivery. Increases disk usage per mail." << endl;
	cout << "  -s\t\t Make DAgent silent. No errors will be printed, except when the calling parameters are wrong." << endl;
	cout << "  -v\t\t Make DAgent verbose. More information on processing email rules can be printed." << endl;
	cout << "  -q\t\t Return qmail style errors." << endl;
	cout << "  -e\t\t Strip email domain from storename, eg username@domain.com will deliver to 'username'." << endl;
	cout << "  -R\t\t Attempt to resolve the passed name. Issue an error if the resolve fails. Only one of -e and -R may be specified." << endl;
	cout << "  -n\t\t Use 'now' as delivery time. Otherwise, time from delivery at the mailserver will be used." << endl;
	cout << "  -N\t\t Do not send a new mail notification to clients looking at this inbox. (Fixes Outlook 2000 running rules too)." << endl;
	cout << "  -r\t\t Mark mail as read on delivery. Default: mark mail as new unread mail." << endl;
	cout << "  -l\t\t Run DAgent as LMTP listener" << endl;
	cout << "  -d\t\t Run DAgent as LMTP daemon, implicates -l. DAgent will run in the background." << endl;
	cout << endl;
	cout << "  -a responder\t path to autoresponder (e.g. /usr/local/bin/autoresponder)" << endl;
	cout << "\t\t The autoresponder is called with </path/to/autoresponder> <from> <to> <subject> <zarafa-username> <messagefile>" << endl;
	cout << "\t\t when the autoresponder is enabled for this user, and -j is not specified" << endl;
	cout << endl;
	cout << "<storename> is the name of the user where to deliver this mail." << endl;
	cout << "If no file is specified with -f, it will be read from standard in." << endl;
	cout << endl;
}

int main(int argc, char *argv[]) {
	FILE *fp = stdin;
	HRESULT hr = hrSuccess;
	bool bDefaultConfigWarning = false; // Provide warning when default configuration is used
	bool bExplicitConfig = false; // User added config option to commandline
	bool bDaemonize = false; // The dagent is not daemonized by default
	bool bListenLMTP = false; // Do not listen for LMTP by default
	bool qmail = false;
	int loglevel = EC_LOGLEVEL_WARNING;	// normally, log warnings and up
	bool strip_email = false;

	DeliveryArgs sDeliveryArgs;
	sDeliveryArgs.strPath = "";
	sDeliveryArgs.strAutorespond = "/usr/bin/zarafa-autorespond";
	sDeliveryArgs.bCreateFolder = false;
	sDeliveryArgs.strDeliveryFolder.clear();
	sDeliveryArgs.szPathSeperator = '\\';
	sDeliveryArgs.bResolveAddress = false;
	sDeliveryArgs.bNewmailNotify = true;
	sDeliveryArgs.ulDeliveryMode = DM_STORE;
	imopt_default_delivery_options(&sDeliveryArgs.sDeliveryOpts);

		const char *szConfig = ECConfig::GetDefaultPath("dagent.cfg");

	enum {
		OPT_HELP,
		OPT_CONFIG,
		OPT_JUNK,
		OPT_FILE,
		OPT_HOST,
		OPT_DAEMONIZE,
		OPT_LISTEN,
		OPT_FOLDER,
		OPT_PUBLIC,
		OPT_CREATE,
		OPT_MARKREAD,
		OPT_NEWMAIL
	};
	struct option long_options[] = {
		{ "help", 0, NULL, OPT_HELP },	// help text
		{ "config", 1, NULL, OPT_CONFIG },	// config file
		{ "junk", 0, NULL, OPT_JUNK },	// junk folder
		{ "file", 1, NULL, OPT_FILE },	// file as input
		{ "host", 1, NULL, OPT_HOST },	// zarafa host parameter
		{ "daemonize",0 ,NULL,OPT_DAEMONIZE}, // daemonize and listen for LMTP
		{ "listen", 0, NULL, OPT_LISTEN},   // listen for LMTP 
		{ "folder", 1, NULL, OPT_FOLDER },	// subfolder of store to deliver in
		{ "public", 1, NULL, OPT_PUBLIC },	// subfolder of public to deliver in
		{ "create", 0, NULL, OPT_CREATE },	// create subfolder if not exist
		{ "read", 0, NULL, OPT_MARKREAD },	// mark mail as read on delivery
		{ "do-not-notify", 0, NULL, OPT_NEWMAIL },	// do not send new mail notification
		{ NULL, 0, NULL, 0 }
	};

	// Default settings
	const configsetting_t lpDefaults[] = {
		{ "server_bind", "127.0.0.1" },
		{ "run_as_user", "" },
		{ "run_as_group", "" },
		{ "pid_file", "/var/run/zarafa-dagent.pid" },
		{ "lmtp_port", "2003" },
		{ "lmtp_max_threads", "20" },
		{ "process_model", "", CONFIGSETTING_UNUSED },
		{ "log_method", "file" },
		{ "log_file", "-" },
		{ "log_level", "2", CONFIGSETTING_RELOADABLE },
		{ "log_timestamp", "0" },
		{ "server_socket", CLIENT_ADMIN_SOCKET },
		{ "sslkey_file", "" },
		{ "sslkey_pass", "", CONFIGSETTING_EXACT },
		{ "spam_header_name", "X-Spam-Status" },
		{ "spam_header_value", "Yes," },
		{ "log_raw_message", "no", CONFIGSETTING_RELOADABLE },
		{ "log_raw_message_path", "/tmp", CONFIGSETTING_RELOADABLE },
		{ "archive_on_delivery", "no", CONFIGSETTING_RELOADABLE },
		{ "mr_autoaccepter", "/usr/bin/zarafa-mr-accept", CONFIGSETTING_RELOADABLE },
		{ "plugin_enabled", "yes" },
		{ "plugin_path", "/var/lib/zarafa/dagent/plugins" },
		{ "plugin_manager_path", "/usr/share/zarafa-dagent/python" },
		{ "set_rule_headers", "yes", CONFIGSETTING_RELOADABLE },
		{ NULL, NULL },
	};

	// @todo: check if we need to setlocale(LC_MESSAGE, "");
	setlocale(LC_CTYPE, "");

	if (argc < 2) {
		print_help(argv[0]);
		return EX_USAGE;
	}

	int c;
	while (1) {
		c = my_getopt_long_permissive(argc, argv, "c:jf:dh:a:F:P:p:qsvenCVrRlN", long_options, NULL);
		if (c == -1)
			break;
		switch (c) {
		case OPT_CONFIG:
		case 'c':
			szConfig = my_optarg;
			bExplicitConfig = true;
			break;
		case OPT_JUNK:
		case 'j':				// junkmail
			sDeliveryArgs.ulDeliveryMode = DM_JUNK;
			break;

		case OPT_FILE:
		case 'f':				// use file as input
			fp = fopen(my_optarg, "rb");
			if(!fp) {
				cerr << "Unable to open file '" << my_optarg << "' for reading" << endl;
				return EX_USAGE;
			}
			break;

		case OPT_LISTEN:
		case 'l':
			bListenLMTP = true;
			bExplicitConfig = true;
			break;

		case OPT_DAEMONIZE:
		case 'd':
			//-d the Dagent is daemonized; service LMTP over socket starts listening on port 2003
			bDaemonize = true;
			bListenLMTP = true;
			bExplicitConfig = true;
			break;
		
		case OPT_HOST:
		case 'h':				// 'host' (file:///var/run/zarafa)
			sDeliveryArgs.strPath = my_optarg;
			break;
		case 'a':				// external autoresponder program
			sDeliveryArgs.strAutorespond = my_optarg;
			break;
		case 'q':				// use qmail errors
			qmail = true;
			break;
		case 's':				// silent, no logging
			loglevel = EC_LOGLEVEL_NONE;
			break;
		case 'v':				// verbose logging
			if (loglevel == EC_LOGLEVEL_INFO)
				loglevel = EC_LOGLEVEL_DEBUG;
			else
				loglevel = EC_LOGLEVEL_INFO;
			break;
		case 'e':				// strip @bla.com from username
			strip_email = true;
			break;
		case 'n':
			sDeliveryArgs.sDeliveryOpts.use_received_date = false;	// conversion will use now()
			break;
		case OPT_FOLDER:
		case 'F':
			sDeliveryArgs.strDeliveryFolder = convert_to<wstring>(my_optarg);
			break;
		case OPT_PUBLIC:
		case 'P':
			sDeliveryArgs.ulDeliveryMode = DM_PUBLIC;
			sDeliveryArgs.strDeliveryFolder = convert_to<wstring>(my_optarg);
			break;
		case 'p':
			sDeliveryArgs.szPathSeperator = my_optarg[0];
			break;
		case OPT_CREATE:
		case 'C':
			sDeliveryArgs.bCreateFolder = true;
			break;
		case OPT_MARKREAD:
		case 'r':
			sDeliveryArgs.sDeliveryOpts.mark_as_read = true;
			break;
		case OPT_NEWMAIL:
		case 'N':
			sDeliveryArgs.bNewmailNotify = false;
			break;

		case 'V':
			cout << "Product version:\t" <<  PROJECT_VERSION_DAGENT_STR << endl
				 << "File version:\t\t" << PROJECT_SVN_REV_STR << endl;
			return EX_USAGE;
		case 'R':
			sDeliveryArgs.bResolveAddress = true;
			break;
		case OPT_HELP:
		default:
			print_help(argv[0]);
			return EX_USAGE;
		};

	}

	g_lpConfig = ECConfig::Create(lpDefaults);
	if (szConfig) {
		/* When LoadSettings fails, provide warning to user (but wait until we actually have the Logger) */
		if (!g_lpConfig->LoadSettings(szConfig))
			bDefaultConfigWarning = true;
		else {
			int argidx = 0;			

			// ParseParams always return true.
			g_lpConfig->ParseParams(argc-my_optind, &argv[my_optind], &argidx);
			if (argidx > 0) {
				// If one overrides the config, it is assumed that the
				// config is explicit. This causes errors from
				// ECConfig::ParseParams to be logged. Besides that
				// it doesn't make sense to override your config if
				// you don't know whats in it.
				bExplicitConfig = true;
			}

			// ECConfig::ParseParams returns the index in the passed array,
			// after some shuffling, where it stopped parsing. my_optind is
			// the index where my_getopt_long_permissive stopped parsing. So
			// adding argidx to my_optind will result in the index after all
			// options are parsed.
			my_optind += argidx;
		}
	}

	if (!bListenLMTP && my_optind == argc) {
		cerr << "Not enough options given, need at least the username" << endl;
		return EX_USAGE;
	}

	if (strip_email && sDeliveryArgs.bResolveAddress) {
		cerr << "You must specify either -e or -R, not both" << endl;
		return EX_USAGE;
	}

	if (!loglevel)
		g_lpLogger = new ECLogger_Null();
	else 
		g_lpLogger = CreateLogger(g_lpConfig, argv[0], "ZarafaDAgent");
	if (!bExplicitConfig && loglevel)
		g_lpLogger->SetLoglevel(loglevel);


	/* Warn users that we are using the default configuration */
	if (bDefaultConfigWarning && bExplicitConfig) {
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Unable to open configuration file %s", szConfig);
		g_lpLogger->Log(EC_LOGLEVEL_ERROR, "Continuing with defaults");
	}

	/* If something went wrong, create special Logger, log message and bail out */
	if (g_lpConfig->HasErrors() && bExplicitConfig) {
		LogConfigErrors(g_lpConfig, g_lpLogger);
		hr = E_FAIL;
		goto exit;
	}

	/* When path wasn't provided through commandline, resolve it from config file */
	if (sDeliveryArgs.strPath.empty())
		sDeliveryArgs.strPath = g_lpConfig->GetSetting("server_socket");
	sDeliveryArgs.strPath = GetServerUnixSocket((char*)sDeliveryArgs.strPath.c_str()); // let environment override if present

	hr = MAPIInitialize(NULL);
	if (hr != hrSuccess) {
		g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to initialize MAPI");
		goto exit;
	}

	if (bListenLMTP) {


		hr = running_service(argv[0], bDaemonize, &sDeliveryArgs);
	} else {
		// log process id prefix to distinguinsh events, file logger only affected
		g_lpLogger->SetLogprefix(LP_PID);

		PyMapiPluginFactory pyMapiPluginFactory;
		hr = pyMapiPluginFactory.Init(g_lpConfig, g_lpLogger);
		if (hr != hrSuccess) {
			g_lpLogger->Log(EC_LOGLEVEL_FATAL, "Unable to instantiate plugin factory, hr=0x%08x", hr);
			goto exit;
		}
		
		PyMapiPluginAPtr ptrPyMapiPlugin;
		hr = GetPluginObject(g_lpLogger, &pyMapiPluginFactory, &ptrPyMapiPlugin);
		if (hr != hrSuccess) 
			goto exit; // Error is logged in GetPluginObject

		hr = deliver_recipient(ptrPyMapiPlugin, argv[my_optind], strip_email, fp, &sDeliveryArgs);
		fclose(fp);
	}

	MAPIUninitialize();

exit:
	DeleteLogger(g_lpLogger);

	if (g_lpConfig)
		delete g_lpConfig;

	if (hr == hrSuccess || bListenLMTP)
		return EX_OK;			// 0
	if (g_bTempfail)
		return (qmail?111:EX_TEMPFAIL);		// please retry again later.
	return (qmail?100:EX_SOFTWARE);			// fatal error, mail was undelivered (or Fallback delivery, but still return an error)
}
