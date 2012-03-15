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

// Damn windows header defines max which break C++ header files
#undef max

#include <iostream>

#include "ECVMIMEUtils.h"
#include "MAPISMTPTransport.h"
#include "CommonUtil.h"
#include "charset/convert.h"

#include <mapi.h>
#include <mapitags.h>
#include <mapidefs.h>
#include <mapiutil.h>
#include <mapix.h>
#include "mapiext.h"
#include <EMSAbTag.h>
#include "ECABEntryID.h"

using namespace std;

class mapiTimeoutHandler : public vmime::net::timeoutHandler
{
public:
	mapiTimeoutHandler() : m_last(0) {};
	virtual ~mapiTimeoutHandler() {};

	// @todo add logging
	virtual bool isTimeOut() { return getTime() >= (m_last + 5*60); };
	virtual void resetTimeOut() { m_last = getTime(); };
	virtual bool handleTimeOut() { return false; };

	const unsigned int getTime() const {
		return vmime::platform::getHandler()->getUnixTime();
	}

private:
	unsigned int m_last;
};

class mapiTimeoutHandlerFactory : public vmime::net::timeoutHandlerFactory
{
public:
	vmime::ref<vmime::net::timeoutHandler> create() {
		return vmime::create<mapiTimeoutHandler>();
	};
};

ECVMIMESender::ECVMIMESender(ECLogger *newlpLogger, std::string strSMTPHost, int port) : ECSender(newlpLogger, strSMTPHost, port) {
}

ECVMIMESender::~ECVMIMESender() {
}

// copied from vmime net/transport.cpp
static void extractMailboxes(vmime::mailboxList& recipients, const vmime::addressList& list)
{
	for (int i = 0 ; i < list.getAddressCount() ; ++i)
	{
		vmime::ref <vmime::mailbox> mbox = list.getAddressAt(i)->clone().dynamicCast <vmime::mailbox>();

		if (mbox != NULL)
			recipients.appendMailbox(mbox);
	}
}

/**
 * Adds all the recipients from a table into the passed recipient list
 *
 * @param lpAdrBook Pointer to the addressbook for the user sending the message (important for multi-tenancy separation)
 * @param lpTable Table to read recipients from
 * @param recipients Reference to list of recipients to append to
 * @param setGroups Set of groups already processed, used for loop-detection in nested expansion
 * @param setRecips Set of recipients already processed, used for duplicate-recip detection
 * @param bAllowEveryone Allow sending to 'everyone'
 *
 * This function takes a MAPI table, reads all items from it, expands any groups and adds all expanded recipients into the passed
 * recipient table. Group expansion is recursive.
 */
HRESULT ECVMIMESender::HrAddRecipsFromTable(LPADRBOOK lpAdrBook, IMAPITable *lpTable, vmime::mailboxList &recipients, std::set<std::wstring> &setGroups, std::set<std::wstring> &setRecips, bool bAllowEveryone)
{
	HRESULT hr = hrSuccess;
	LPSRowSet lpRowSet = NULL;
	std::wstring strName, strEmail, strType;

	hr = lpTable->QueryRows(-1, 0, &lpRowSet);
	if (hr != hrSuccess)
		goto exit;

	// Get all recipients from the group
	for (ULONG i = 0; i < lpRowSet->cRows; i++) {
		LPSPropValue lpPropObjectType = PpropFindProp( lpRowSet->aRow[i].lpProps, lpRowSet->aRow[i].cValues, PR_OBJECT_TYPE);
		
		if(lpPropObjectType == NULL || lpPropObjectType->Value.ul == MAPI_MAILUSER) {
			// Normal recipient
			if (HrGetAddress(lpAdrBook, lpRowSet->aRow[i].lpProps, lpRowSet->aRow[i].cValues,
							 PR_ENTRYID, PR_DISPLAY_NAME_W, PR_ADDRTYPE_W, PR_EMAIL_ADDRESS_W,
							 strName, strType, strEmail) == hrSuccess)
			{

				if(!strEmail.empty() && setRecips.find(strEmail) == setRecips.end()) {
					recipients.appendMailbox(vmime::create<vmime::mailbox>(convert_to<string>(strEmail)));
					setRecips.insert(strEmail);
					lpLogger->Log(EC_LOGLEVEL_DEBUG, "RCPT TO: %ls", strEmail.c_str());	
				}
			}
		}
		else if(lpPropObjectType->Value.ul == MAPI_DISTLIST) {
			// Group
			LPSPropValue lpGroupName = PpropFindProp(lpRowSet->aRow[i].lpProps, lpRowSet->aRow[i].cValues, PR_EMAIL_ADDRESS_W);
			LPSPropValue lpGroupEntryID = PpropFindProp(lpRowSet->aRow[i].lpProps, lpRowSet->aRow[i].cValues, PR_ENTRYID);
	
			if(bAllowEveryone == false) {
				bool bEveryone = false;
				
				if(EntryIdIsEveryone(lpGroupEntryID->Value.bin.cb, (LPENTRYID)lpGroupEntryID->Value.bin.lpb, &bEveryone) == hrSuccess && bEveryone) {
					lpLogger->Log(EC_LOGLEVEL_ERROR, "Denying send to Everyone");
					error = std::wstring(L"You are not allowed to send to the 'Everyone' group");
					hr = MAPI_E_NO_ACCESS;
					goto exit;
				}
			}

			// Recursively expand all recipients in the group
			hr = HrExpandGroup(lpAdrBook, lpGroupName, lpGroupEntryID, recipients, setGroups, setRecips, bAllowEveryone);
			if (hr == MAPI_E_TOO_COMPLEX || hr == MAPI_E_INVALID_PARAMETER) {
				// ignore group nesting loop and non user/group types (eg. companies)
				hr = hrSuccess;
			} else if (hr != hrSuccess) {
				// eg. MAPI_E_NOT_FOUND
				lpLogger->Log(EC_LOGLEVEL_ERROR, "Error while expanding group. Group: %ls, error: 0x%08x", lpGroupName ? lpGroupName->Value.lpszW : L"<unknown>", hr);
				error = std::wstring(L"Error in group '") + (lpGroupName ? lpGroupName->Value.lpszW : L"<unknown>") + L"', unable to send e-mail";
				goto exit;
			}
		}
	}

exit:
	if(lpRowSet)
		FreeProws(lpRowSet);
		
	return hr;
}


/**
 * Takes a group entry of a recipient table and expands the recipients for the group recursively by adding them to the recipients list
 *
 * @param lpAdrBook Pointer to the addressbook for the user sending the message (important for multi-tenancy separation)
 * @param lpGroupName Pointer to PR_EMAIL_ADDRESS_W entry for the recipient in the recipient table
 * @param lpGroupEntryId Pointer to PR_ENTRYID entry for the recipient in the recipient table
 * @param recipients Reference to list of VMIME recipients to be appended to
 * @param setGroups Set of already-processed groups (used for loop detecting in group expansion)
 * @param setRecips Set of recipients already processed, used for duplicate-recip detection
 *
 * This function expands the specified group by opening the group and adding all user entries to the recipients list, and
 * recursively expanding groups in the group. 
 *
 * lpGroupEntryID may be NULL, in which case lpGroupName is used to resolve the group via the addressbook. If
 * both parameters are set, lpGroupEntryID is used, and lpGroupName is ignored.
 */
HRESULT ECVMIMESender::HrExpandGroup(LPADRBOOK lpAdrBook, LPSPropValue lpGroupName, LPSPropValue lpGroupEntryID, vmime::mailboxList &recipients, std::set<std::wstring> &setGroups, std::set<std::wstring> &setRecips, bool bAllowEveryone)
{
	HRESULT hr = hrSuccess;
	IDistList *lpGroup = NULL;
	ULONG ulType = 0;
	IMAPITable *lpTable = NULL;
	LPSRowSet lpRows = NULL;
	LPSPropValue lpEmailAddress = NULL;

	if(lpGroupEntryID == NULL || lpAdrBook->OpenEntry(lpGroupEntryID->Value.bin.cb, (LPENTRYID)lpGroupEntryID->Value.bin.lpb, NULL, 0, &ulType, (IUnknown **)&lpGroup) != hrSuccess || ulType != MAPI_DISTLIST) {
		// Entry id for group was not given, or the group could not be opened, or the entryid was not a group (eg one-off entryid)
		// Therefore resolve group name, and open that instead.
		
		if(lpGroupName == NULL) {
			hr = MAPI_E_NOT_FOUND;
			goto exit;
		}
		
		MAPIAllocateBuffer(sizeof(SRowSet), (void **)&lpRows);
		lpRows->cRows = 1;
		MAPIAllocateBuffer(sizeof(SPropValue), (void **)&lpRows->aRow[0].lpProps);
		lpRows->aRow[0].cValues = 1;
		
		lpRows->aRow[0].lpProps[0].ulPropTag = PR_DISPLAY_NAME_W;
		lpRows->aRow[0].lpProps[0].Value.lpszW = lpGroupName->Value.lpszW;
		
		hr = lpAdrBook->ResolveName(0, MAPI_UNICODE | EMS_AB_ADDRESS_LOOKUP, NULL, (LPADRLIST)lpRows);
		if(hr != hrSuccess)
			goto exit;
			
		lpGroupEntryID = PpropFindProp(lpRows->aRow[0].lpProps, lpRows->aRow[0].cValues, PR_ENTRYID);
		if(!lpGroupEntryID) {
			hr = MAPI_E_NOT_FOUND;
			goto exit;
		}

		if (lpGroup)
			lpGroup->Release();
		lpGroup = NULL;

		// Open resolved entry
		hr = lpAdrBook->OpenEntry(lpGroupEntryID->Value.bin.cb, (LPENTRYID)lpGroupEntryID->Value.bin.lpb, NULL, 0, &ulType, (IUnknown **)&lpGroup);
		if(hr != hrSuccess)
			goto exit;
			
		if(ulType != MAPI_DISTLIST) {
			lpLogger->Log(EC_LOGLEVEL_DEBUG, "Expected group, but opened type %d", ulType);	
			hr = MAPI_E_INVALID_PARAMETER;
			goto exit;
		}
	}
	
	hr = HrGetOneProp(lpGroup, PR_EMAIL_ADDRESS_W, &lpEmailAddress);
	if(hr != hrSuccess)
		goto exit;
		
	if(setGroups.find(lpEmailAddress->Value.lpszW) != setGroups.end()) {
		// Group loops in nesting
		hr = MAPI_E_TOO_COMPLEX;
		goto exit;
	}
	
	// Add group name to list of processed groups
	setGroups.insert(lpEmailAddress->Value.lpszW);
	
	hr = lpGroup->GetContentsTable(MAPI_UNICODE, &lpTable);
	if(hr != hrSuccess)
		goto exit;

	hr = HrAddRecipsFromTable(lpAdrBook, lpTable, recipients, setGroups, setRecips, bAllowEveryone);
	if(hr != hrSuccess)
		goto exit;
	
exit:
	if(lpTable)
		lpTable->Release();
		
	if(lpRows)
		FreeProws(lpRows);
		
	if(lpGroup)
		lpGroup->Release();
		
	if(lpEmailAddress)
		MAPIFreeBuffer(lpEmailAddress);
		
	return hr;
}

HRESULT ECVMIMESender::HrMakeRecipientsList(LPADRBOOK lpAdrBook, LPMESSAGE lpMessage, vmime::ref<vmime::message> vmMessage, vmime::mailboxList &recipients, bool bAllowEveryone)
{
	HRESULT hr = hrSuccess;
	SRestriction sRestriction;
	SPropValue sRestrictProp;
	LPMAPITABLE lpRTable = NULL;
	bool bResend = false;
	std::set<std::wstring> setGroups; // Set of groups to make sure we don't get into an expansion-loop
	std::set<std::wstring> setRecips; // Set of recipients to make sure we don't send two identical RCPT TO's
	LPSPropValue lpMessageFlags = NULL;
	
	hr = HrGetOneProp(lpMessage, PR_MESSAGE_FLAGS, &lpMessageFlags);
	if (hr != hrSuccess)
		goto exit;
		
	if(lpMessageFlags->Value.ul & MSGFLAG_RESEND)
		bResend = true;
	
	hr = lpMessage->GetRecipientTable(MAPI_UNICODE, &lpRTable);
	if (hr != hrSuccess)
		goto exit;

	// When resending, only send to MAPI_P1 recipients
	if(bResend) {
		sRestriction.rt = RES_PROPERTY;
		sRestriction.res.resProperty.relop = RELOP_EQ;
		sRestriction.res.resProperty.ulPropTag = PR_RECIPIENT_TYPE;
		sRestriction.res.resProperty.lpProp = &sRestrictProp;

		sRestrictProp.ulPropTag = PR_RECIPIENT_TYPE;
		sRestrictProp.Value.ul = MAPI_P1;

		hr = lpRTable->Restrict(&sRestriction, TBL_BATCH);
		if (hr != hrSuccess)
			goto exit;
	}

	hr = HrAddRecipsFromTable(lpAdrBook, lpRTable, recipients, setGroups, setRecips, bAllowEveryone);
	if (hr != hrSuccess)
		goto exit;
	
exit:
	if (lpMessageFlags)
		MAPIFreeBuffer(lpMessageFlags);

	if (lpRTable)
		lpRTable->Release();

	return hr;
}


// This function does not catch the vmime exception
// it should be handled by the calling party.

HRESULT ECVMIMESender::sendMail(LPADRBOOK lpAdrBook, LPMESSAGE lpMessage, vmime::ref<vmime::message> vmMessage, bool bAllowEveryone)
{
	HRESULT hr = hrSuccess;
	vmime::mailbox expeditor;
	vmime::mailboxList recipients;
	vmime::ref<vmime::messaging::session> vmSession;
	vmime::ref<vmime::messaging::transport>	vmTransport;
	vmime::ref<vmime::net::smtp::MAPISMTPTransport> mapiTransport = NULL;

	if (!lpMessage || !vmMessage) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	smtpresult = 0;
	error.clear();

	try {
		// Session initialization (global properties)
		vmSession = vmime::create <vmime::net::session>();

		// set the server address and port, plus type of service by use of url
		// and get our special mapismtp mailer
		vmime::utility::url url("mapismtp", smtphost, smtpport);
		vmTransport = vmSession->getTransport(url);
		vmTransport->setTimeoutHandlerFactory(vmime::create<mapiTimeoutHandlerFactory>());

		// cast to access interface extra's
		mapiTransport = vmTransport.dynamicCast<vmime::net::smtp::MAPISMTPTransport>();

		// get expeditor for 'mail from:' smtp command
		try {
			const vmime::mailbox& mbox = *vmMessage->getHeader()->findField(vmime::fields::FROM)->
				getValue().dynamicCast <const vmime::mailbox>();

			expeditor = mbox;
		}
		catch (vmime::exceptions::no_such_field&) {
			throw vmime::exceptions::no_expeditor();
		}

		if (expeditor.isEmpty()) {
			// cancel this message as unsendable, would otherwise be thrown out of transport::send()
			hr = MAPI_W_CANCEL_MESSAGE;
			error = L"No expeditor in e-mail";
			goto exit;
		}

		hr = HrMakeRecipientsList(lpAdrBook, lpMessage, vmMessage, recipients, bAllowEveryone);
		if (hr != hrSuccess)
			goto exit;

		if (recipients.isEmpty()) {
			// cancel this message as unsendable, would otherwise be thrown out of transport::send()
			hr = MAPI_W_CANCEL_MESSAGE;
			error = L"No recipients in e-mail";
			goto exit;
		}

        // Remove BCC headers from the message we're about to send
        try {
			vmime::ref <vmime::headerField> bcc = vmMessage->getHeader()->findField(vmime::fields::BCC);
			vmMessage->getHeader()->removeField(bcc);
        }
        catch (vmime::exceptions::no_such_field&) { }

		// Generate the message, "stream" it and delegate the sending
		// to the generic send() function.
		std::ostringstream oss;
		vmime::utility::outputStreamAdapter ossAdapter(oss);

		vmMessage->generate(ossAdapter);

		const string& str(oss.str()); // copy?
		vmime::utility::inputStreamStringAdapter isAdapter(str); // direct oss.str() ?
		
		if (mapiTransport)
			mapiTransport->setLogger(lpLogger);

		// send the email already!
		try {
			vmTransport->connect();
		} catch (vmime::exception &e) {
			// special error, smtp server not respoding, so try later again
			lpLogger->Log(EC_LOGLEVEL_ERROR, "Connect to SMTP: %s. E-Mail will be tried again later.", e.what());
			hr = MAPI_W_NO_SERVICE;
			goto exit;
		}

		try {
			vmTransport->send(expeditor, recipients, isAdapter, str.length(), NULL);
			vmTransport->disconnect();
		}
		catch (vmime::exceptions::command_error& e) {
			if (mapiTransport)
				lstFailedRecipients = mapiTransport->getRecipientErrorList();
			lpLogger->Log(EC_LOGLEVEL_ERROR, "SMTP: %s Response: %s", e.what(), e.response().c_str());
			smtpresult = atoi(e.response().substr(0, e.response().find_first_of(" ")).c_str());
			error = convert_to<wstring>(e.response());
			// message should be cancelled, unsendable, test by smtp result code.
			hr = MAPI_W_CANCEL_MESSAGE;
			goto exit;
		} 
		catch (vmime::exception &e) {
			// special error, smtp server not respoding, so try later again
			lpLogger->Log(EC_LOGLEVEL_ERROR, "SMTP: %s. E-Mail will be tried again later.", e.what());
			hr = MAPI_W_NO_SERVICE;
			goto exit;
		}

		if (mapiTransport && mapiTransport->getRecipientErrorCount() > 0) {
			// hr value returned to spooler
			hr = MAPI_W_PARTIAL_COMPLETION;
			lstFailedRecipients = mapiTransport->getRecipientErrorList();
			error = L"Unable to reach all recipients";
			smtpresult = 250;	// ok value
			lpLogger->Log(EC_LOGLEVEL_ERROR, "SMTP Error: Not all recipients could be reached. User will be notified.");
		}
	}
	catch (vmime::exception& e) {
		// connection_greeting_error, ...?
		lpLogger->Log(EC_LOGLEVEL_ERROR, "%s", e.what());
		error = convert_to<wstring>(e.what());
		hr = MAPI_E_NETWORK_ERROR;
	}
	catch (std::exception& e) {
		lpLogger->Log(EC_LOGLEVEL_ERROR, "%s",e.what());
		error = convert_to<wstring>(e.what());
		hr = MAPI_E_NETWORK_ERROR;
	}

exit:
	return hr;
}

ECScreenSender::ECScreenSender(ECLogger *newlpLogger) : ECSender(newlpLogger, NULL, 0)
{
}

ECScreenSender::~ECScreenSender()
{
}

HRESULT ECScreenSender::sendMail(vmime::ref<vmime::message> vmMessage)
{
	HRESULT hr = hrSuccess;
	string mail;

	try {
		mail = vmMessage->generate();
		cout << endl << endl << "----------" << endl << endl <<
			mail << endl << endl << "----------" << endl;
	}
	catch (...) {
		cerr << "exception.\n";
		hr = MAPI_E_CALL_FAILED;
	}
	return hr;
}
