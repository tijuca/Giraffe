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
#include "stringutil.h"

// Damn windows header defines max which break C++ header files
#undef max

#include <string>
#include <fstream>
#include <iostream>
#include <stdlib.h>

// vmime
#include "vmime/vmime.hpp"
#include "vmime/textPartFactory.hpp"
#include "mapiTextPart.h"
#include "vmime/platforms/posix/posixHandler.hpp"

// mapi
#include <mapix.h>
#include <mapiutil.h>
#include <mapiext.h>
#include "edkmdb.h"
#include "CommonUtil.h"
#include "charset/convert.h"
// inetmapi
#include "inetmapi.h"
#include "VMIMEToMAPI.h"
#include "MAPIToVMIME.h"
#include "ECVMIMEUtils.h"
#include "ECMapiUtils.h"
#include "ECLogger.h"

using namespace std;

ECSender::ECSender(ECLogger *newlpLogger, const std::string &strSMTPHost, int port) {
	lpLogger = newlpLogger;
	if (!lpLogger)
		lpLogger = new ECLogger_Null();
	else
		lpLogger->AddRef();

	smtpresult = 0;
	smtphost = strSMTPHost;
	smtpport = port;
}

ECSender::~ECSender() {
	lpLogger->Release();
}

int ECSender::getSMTPResult() {
	return smtpresult;
}

const WCHAR* ECSender::getErrorString() {
	return error.c_str();
}

void ECSender::setError(const std::wstring &newError) {
	error = newError;
}

void ECSender::setError(const std::string &newError) {
	error = convert_to<wstring>(newError);
}

bool ECSender::haveError() {
	return ! error.empty();
}

const unsigned int ECSender::getRecipientErrorCount() const
{
	return lstFailedRecipients.size();
}

const unsigned int ECSender::getRecipientErrorSMTPCode(unsigned int offset) const
{
	if (offset >= lstFailedRecipients.size())
		return 0;
	return lstFailedRecipients[offset].ulSMTPcode;
}

const string ECSender::getRecipientErrorText(unsigned int offset) const
{
	if (offset >= lstFailedRecipients.size())
		return string();
	return lstFailedRecipients[offset].strSMTPResponse;
}

const wstring ECSender::getRecipientErrorDisplayName(unsigned int offset) const
{
	if (offset >= lstFailedRecipients.size())
		return wstring();
	return lstFailedRecipients[offset].strRecipName;
}

const string ECSender::getRecipientErrorEmailAddress(unsigned int offset) const
{
	if (offset >= lstFailedRecipients.size())
		return string();
	return lstFailedRecipients[offset].strRecipEmail;
}

pthread_mutex_t vmInitLock = PTHREAD_MUTEX_INITIALIZER;
static void InitializeVMime()
{
	pthread_mutex_lock(&vmInitLock);
	try {
		vmime::platform::getHandler();
	}
	catch (vmime::exceptions::no_platform_handler &) {
		vmime::platform::setHandler<vmime::platforms::posix::posixHandler>();
		// need to have a unique indentifier in the mediaType
		vmime::textPartFactory::getInstance()->registerType<vmime::mapiTextPart>(vmime::mediaType(vmime::mediaTypes::TEXT, "mapi"));
		// init our random engine for random message id generation
		rand_init();
	}
	pthread_mutex_unlock(&vmInitLock);
}

static string generateRandomMessageId()
{
#define IDLEN 38
	char id[IDLEN] = {0};
	// the same format as the vmime generator, but with more randomness
	snprintf(id, IDLEN, "zarafa.%08x.%04x.%08x%08x", (unsigned int)time(NULL), getpid(), rand_mt(), rand_mt());
	return string(id, IDLEN -1); // do not include \0 in std::string
#undef IDLEN
}

INETMAPI_API ECSender* CreateSender(ECLogger *lpLogger, const std::string &smtp, int port) {
	return new ECVMIMESender(lpLogger, smtp, port);
}

// parse rfc822 input, and set props in lpMessage
INETMAPI_API HRESULT IMToMAPI(IMAPISession *lpSession, IMsgStore *lpMsgStore, IAddrBook *lpAddrBook, IMessage *lpMessage, const string &input, delivery_options dopt, ECLogger *lpLogger)
{
	HRESULT hr = hrSuccess;
	VMIMEToMAPI *VMToM = NULL;

	VMToM = new VMIMEToMAPI(lpAddrBook, lpLogger, dopt);

	InitializeVMime();

	// fill mapi object from buffer
	hr = VMToM->convertVMIMEToMAPI(input, lpMessage);

	delete VMToM;
	
	return hr;
}

// Read properties from lpMessage object and fill a buffer with internet rfc822 format message
INETMAPI_API HRESULT IMToINet(IMAPISession *lpSession, IAddrBook *lpAddrBook, IMessage *lpMessage, char** lppbuf, sending_options sopt, ECLogger *lpLogger)
{
	HRESULT hr = hrSuccess;
	std::ostringstream oss;
	char *lpszData = NULL;

	hr = IMToINet(lpSession, lpAddrBook, lpMessage, oss, sopt, lpLogger);
	if (hr != hrSuccess)
		goto exit;
        
	lpszData = new char[oss.str().size()+1];
	strcpy(lpszData, oss.str().c_str());

	*lppbuf = lpszData;
exit:    
	return hr;
}

INETMAPI_API HRESULT IMToINet(IMAPISession *lpSession, IAddrBook *lpAddrBook, IMessage *lpMessage, std::ostream &os, sending_options sopt, ECLogger *lpLogger)
{
	HRESULT			hr			= hrSuccess;
	LPSPropValue	lpTime		= NULL;
	LPSPropValue	lpMessageId	= NULL;
	MAPIToVMIME*	mToVM		= new MAPIToVMIME(lpSession, lpAddrBook, lpLogger, sopt);
	vmime::ref<vmime::message>	lpVMMessage	= NULL;
	vmime::utility::outputStreamAdapter adapter(os);

	InitializeVMime();

	hr = mToVM->convertMAPIToVMIME(lpMessage, &lpVMMessage);
	if (hr != hrSuccess)
		goto exit;

	try {
		// vmime messageBuilder has set Date header to now(), so we overwrite it.
		if (HrGetOneProp(lpMessage, PR_CLIENT_SUBMIT_TIME, &lpTime) == hrSuccess) {
			lpVMMessage->getHeader()->Date()->setValue(FiletimeTovmimeDatetime(lpTime->Value.ft));
		}
		// else, try PR_MESSAGE_DELIVERY_TIME, maybe other timestamps?

		if (HrGetOneProp(lpMessage, PR_INTERNET_MESSAGE_ID_A, &lpMessageId) == hrSuccess) {
			lpVMMessage->getHeader()->MessageId()->setValue(lpMessageId->Value.lpszA);
		}

		lpVMMessage->generate(adapter);
	}
	catch (vmime::exception&) {
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}
	catch (std::exception&) {
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}
	
exit:
	if (lpTime)
		MAPIFreeBuffer(lpTime);

	if (lpMessageId)
		MAPIFreeBuffer(lpMessageId);
	
	delete mToVM;

	return hr;
}


// Read properties from lpMessage object and to internet rfc2822 format message
// then send it using the provided ECSender object
INETMAPI_API HRESULT IMToINet(IMAPISession *lpSession, IAddrBook *lpAddrBook, IMessage *lpMessage, ECSender *mailer_base, sending_options sopt, ECLogger *lpLogger)
{
	HRESULT			hr			= hrSuccess;
	MAPIToVMIME		*mToVM		= new MAPIToVMIME(lpSession, lpAddrBook, lpLogger, sopt);
	vmime::ref<vmime::message>	vmMessage;
	ECVMIMESender	*mailer		= dynamic_cast<ECVMIMESender*>(mailer_base);
	wstring			wstrError;

	if (!mailer) {
		hr = MAPI_E_INVALID_PARAMETER;
		goto exit;
	}

	InitializeVMime();

	hr = mToVM->convertMAPIToVMIME(lpMessage, &vmMessage);

	if (hr != hrSuccess) {
		wstrError = mToVM->getConversionError();
		if (wstrError.empty())
			wstrError = L"No error details specified";

		mailer->setError(L"Conversion error: " + wstringify(hr, true) + L". " + wstrError + L". Your email is not sent at all and cannot be retried.");
		goto exit;
	}

	try {
		// vmime::messageId::generateId() is not random enough since we use forking in the spooler
		vmime::messageId msgId(generateRandomMessageId(), vmime::platform::getHandler()->getHostName());
		vmMessage->getHeader()->MessageId()->setValue(msgId);
		lpLogger->Log(EC_LOGLEVEL_ERROR, "Sending message with Message-ID: " + msgId.getId());
	}
	catch (vmime::exception& e) {
		mailer->setError(e.what());
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}
	catch (std::exception& e) {
		mailer->setError(e.what());
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}
	catch (...) {
		hr = MAPI_E_NOT_FOUND;
		goto exit;
	}
	
	hr = mailer->sendMail(lpAddrBook, lpMessage, vmMessage, sopt.allow_send_to_everyone);

exit:
	delete mToVM;

	return hr;
}

/** 
 * Create BODY and BODYSTRUCTURE strings for IMAP.
 * 
 * @param[in] input an RFC-822 email
 * @param[out] lpSimple optional BODY result
 * @param[out] lpExtended optional BODYSTRUCTURE result
 * 
 * @return MAPI Error code
 */
INETMAPI_API HRESULT createIMAPProperties(const std::string &input, std::string *lpEnvelope, std::string *lpBody, std::string *lpBodyStructure)
{
	InitializeVMime();

	VMIMEToMAPI VMToM;

	return VMToM.createIMAPProperties(input, lpEnvelope, lpBody, lpBodyStructure);
}

