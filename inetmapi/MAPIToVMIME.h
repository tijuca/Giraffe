/*
 * Copyright 2005 - 2016 Zarafa and its licensors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License, version 3,
 * as published by the Free Software Foundation.
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

#ifndef MAPITOVMIME
#define MAPITOVMIME

#include <kopano/zcdefs.h>
#include <memory>
#include <mapix.h>

#include <string>
#include <vmime/vmime.hpp>
#include <vmime/mailbox.hpp>
#include <inetmapi/options.h>
#include <mapidefs.h>
#include <kopano/charset/convert.h>
#include "SMIMEMessage.h"

namespace KC {

/**
 * %MTV_SPOOL:	add X-Mailer headers on message
 */
enum {
	MTV_NONE = 0,
	MTV_SPOOL = 1 << 0,
	MTV_SKIP_CONTENT = 1 << 1,
};

class MAPIToVMIME _kc_final {
public:
	MAPIToVMIME();
	MAPIToVMIME(IMAPISession *, IAddrBook *, sending_options);
	~MAPIToVMIME();

	HRESULT convertMAPIToVMIME(IMessage *in, vmime::shared_ptr<vmime::message> *out, unsigned int = MTV_NONE);
	std::wstring getConversionError(void) const;

private:
	sending_options sopt;
	LPADRBOOK m_lpAdrBook;
	LPMAPISESSION m_lpSession;
	std::wstring m_strError;
	convert_context m_converter;
	vmime::charset m_vmCharset;		//!< charset to use in email
	std::string m_strCharset;		//!< charset to use in email + //TRANSLIT tag
	std::string m_strHTMLCharset;	//!< HTML body charset in MAPI message (input)

	enum eBestBody { plaintext, html, realRTF };
	
	HRESULT fillVMIMEMail(IMessage *lpMessage, bool bSkipContent, vmime::messageBuilder* lpVMMessageBuilder);

	HRESULT handleTextparts(IMessage* lpMessage, vmime::messageBuilder* lpVMMessageBuilder, eBestBody *bestBody);
	HRESULT getMailBox(LPSRow lpRow, vmime::shared_ptr<vmime::address> *mbox);
	HRESULT processRecipients(IMessage* lpMessage, vmime::messageBuilder* lpVMMessageBuilder);
	HRESULT handleExtraHeaders(IMessage *in, vmime::shared_ptr<vmime::header> out, unsigned int);
	HRESULT handleReplyTo(IMessage *in, vmime::shared_ptr<vmime::header> hdr);
	HRESULT handleContactEntryID(ULONG cValues, LPSPropValue lpProps, std::wstring &strName, std::wstring &strType, std::wstring &strEmail);
	HRESULT handleSenderInfo(IMessage* lpMessage, vmime::shared_ptr<vmime::header>);

	HRESULT handleAttachments(IMessage* lpMessage, vmime::messageBuilder* lpVMMessageBuilder);
	HRESULT handleSingleAttachment(IMessage* lpMessage, LPSRow lpRow, vmime::messageBuilder* lpVMMessageBuilder);
	HRESULT parseMimeTypeFromFilename(std::wstring strFilename, vmime::mediaType *lpMT, bool *lpbSendBinary);
	HRESULT setBoundaries(vmime::shared_ptr<vmime::header> hdr, vmime::shared_ptr<vmime::body> body, const std::string &boundary);
	HRESULT handleXHeaders(IMessage *in, vmime::shared_ptr<vmime::header> out, unsigned int);
	HRESULT handleTNEF(IMessage* lpMessage, vmime::messageBuilder* lpVMMessageBuilder, eBestBody bestBody);

	// build Messages
	HRESULT BuildNoteMessage(IMessage *in, vmime::shared_ptr<vmime::message> *out, unsigned int = MTV_NONE);
	HRESULT BuildMDNMessage(IMessage *in, vmime::shared_ptr<vmime::message> *out);

	// util
	void capitalize(char *s);
	void removeEnters(WCHAR *s);
	vmime::text getVmimeTextFromWide(const WCHAR* lpszwInput, bool bWrapInWord = true);
	vmime::text getVmimeTextFromWide(const std::wstring& strwInput, bool bWrapInWord = true);
	bool is_voting_request(IMessage *lpMessage) const;
	bool has_reminder(IMessage *) const ;
};

} /* namespace */

#endif
