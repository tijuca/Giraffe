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

#ifndef MAPITOVMIME
#define MAPITOVMIME

#include <mapix.h>

#include <string>
#include <vmime/vmime.hpp>
#include <vmime/mailbox.hpp>
#include "options.h"
#include "mapidefs.h"
#include "ECLogger.h"
#include "charset/convert.h"
#include "SMIMEMessage.h"

class MAPIToVMIME
{
public:
	MAPIToVMIME();
	MAPIToVMIME(IMAPISession *lpSession, IAddrBook *lpAddrBook, ECLogger *newlogger, sending_options sopt);
	~MAPIToVMIME();

	HRESULT convertMAPIToVMIME(IMessage *lpMessage, vmime::ref<vmime::message> *lpvmMessage);
	std::wstring getConversionError();

private:
	ECLogger *lpLogger;
	sending_options sopt;
	LPADRBOOK m_lpAdrBook;
	LPMAPISESSION m_lpSession;
	std::wstring m_strError;
	convert_context m_converter;
	vmime::charset m_vmCharset;		//!< charset to use in email
	std::string m_strCharset;		//!< charset to use in email + //TRANSLIT tag
	std::string m_strHTMLCharset;	//!< HTML body charset in MAPI message (input)

	HRESULT fillVMIMEMail(IMessage *lpMessage, bool bSkipContent, vmime::messageBuilder* lpVMMessageBuilder);

	HRESULT handleTextparts(IMessage* lpMessage, vmime::messageBuilder* lpVMMessageBuilder, BOOL* lpbRealRTF);
	HRESULT getMailBox(LPSRow lpRow, vmime::ref<vmime::address> *lpvmMailbox);
	HRESULT processRecipients(IMessage* lpMessage, vmime::messageBuilder* lpVMMessageBuilder);
	HRESULT handleExtraHeaders(IMessage *lpMessage, vmime::ref<vmime::header> vmHeader);
	HRESULT handleReplyTo(IMessage* lpMessage, vmime::ref<vmime::header> vmHeader);
	HRESULT handleContactEntryID(ULONG cValues, LPSPropValue lpProps, std::wstring &strName, std::wstring &strType, std::wstring &strEmail);
	HRESULT handleSenderInfo(IMessage* lpMessage, vmime::ref<vmime::header> vmHeader);

	HRESULT handleAttachments(IMessage* lpMessage, vmime::messageBuilder* lpVMMessageBuilder);
	HRESULT handleSingleAttachment(IMessage* lpMessage, LPSRow lpRow, vmime::messageBuilder* lpVMMessageBuilder);
	HRESULT parseMimeTypeFromFilename(std::wstring strFilename, vmime::mediaType *lpMT, bool *lpbSendBinary);
	HRESULT setBoundaries(vmime::ref<vmime::header> vmHeader, vmime::ref<vmime::body> vmBody, const std::string& boundary);
	HRESULT handleXHeaders(IMessage* lpMessage, vmime::ref<vmime::header> vmHeader);
	HRESULT handleTNEF(IMessage* lpMessage, vmime::messageBuilder* lpVMMessageBuilder, BOOL bRealRTF);

	// build Messages
	HRESULT BuildNoteMessage(IMessage *lpMessage, bool bSkipContent, vmime::ref<vmime::message> *lpvmMessage);
	HRESULT BuildMDNMessage(IMessage *lpMessage, vmime::ref<vmime::message> *lpvmMessage);

	// util
	void capitalize(char *s);
	void removeEnters(WCHAR *s);
	vmime::text getVmimeTextFromWide(const WCHAR* lpszwInput, bool bWrapInWord = true);
	vmime::text getVmimeTextFromWide(const std::wstring& strwInput, bool bWrapInWord = true);
};

#endif
