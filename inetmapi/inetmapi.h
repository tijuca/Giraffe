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

#ifndef INETMAPI_H
#define INETMAPI_H

/* WARNING */
/* mapidefs.h may not be included _before_ any vmime include! */

#include <mapix.h>
#include <mapidefs.h>
#include <vector>
#include "options.h"

class ECLogger;

/* we do not need this on linux */
# define INETMAPI_API

typedef struct _sFailedRecip {
	std::string strRecipEmail;
	std::wstring strRecipName;
	unsigned int ulSMTPcode;
	std::string strSMTPResponse;
} sFailedRecip;

// Sender Base class
// implementation of smtp sender in ECVMIMEUtils as ECVMIMESender
class ECSender {
protected:
	std::string smtphost;
	int smtpport;
	std::wstring error;
	int smtpresult;

	std::vector<sFailedRecip> lstFailedRecipients;

	ECLogger *lpLogger;

public:
	ECSender(ECLogger *newlpLogger, const std::string &strSMTPHost, int port);
	virtual	~ECSender();

	virtual int getSMTPResult();
	virtual const WCHAR* getErrorString();
	virtual void setError(const std::wstring &newError);
	virtual void setError(const std::string &newError);
	virtual bool haveError();

	virtual const unsigned int getRecipientErrorCount() const;
	virtual const unsigned int getRecipientErrorSMTPCode(unsigned int offset) const;
	virtual const std::string getRecipientErrorText(unsigned int offset) const;
	virtual const std::wstring getRecipientErrorDisplayName(unsigned int offset) const;
	virtual const std::string getRecipientErrorEmailAddress(unsigned int offset) const;
};

bool ValidateCharset(const char *charset);

/* c wrapper to create object */
INETMAPI_API ECSender* CreateSender(ECLogger *lpLogger, const std::string &smtp, int port);

// Read char Buffer and set properties on open lpMessage object
INETMAPI_API HRESULT IMToMAPI(IMAPISession *lpSession, IMsgStore *lpMsgStore, IAddrBook *lpAddrBook, IMessage *lpMessage, const std::string &input, delivery_options dopt, ECLogger *lpLogger = NULL);

// Read properties from lpMessage object and fill a buffer with internet rfc822 format message
// Use this one for retrieving messages not in outgoing que, they already have PR_SENDER_EMAIL/NAME
// This can be used in making pop3 / imap server

// Read properties from lpMessage object and fill buffer with internet rfc822 format message
INETMAPI_API HRESULT IMToINet(IMAPISession *lpSession, IAddrBook *lpAddrBook, IMessage *lpMessage, char** lppbuf, sending_options sopt, ECLogger *lpLogger = NULL);

// Read properties from lpMessage object and output to stream with internet rfc822 format message
INETMAPI_API HRESULT IMToINet(IMAPISession *lpSession, IAddrBook *lpAddrBook, IMessage *lpMessage, std::ostream &os, sending_options sopt, ECLogger *lpLogger = NULL);

// Read properties from lpMessage object and send using  lpSMTPHost
INETMAPI_API HRESULT IMToINet(IMAPISession *lpSession, IAddrBook *lpAddrBook, IMessage *lpMessage, ECSender *mailer, sending_options sopt, ECLogger *lpLogger);

// Parse the RFC822 input and create IMAP Envelope, Body and Bodystructure property values
INETMAPI_API HRESULT createIMAPProperties(const std::string &input, std::string *lpEnvelope, std::string *lpBody, std::string *lpBodyStructure);

#endif // INETMAPI_H
