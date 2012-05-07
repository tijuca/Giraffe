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

#ifndef VMIMETOMAPI
#define VMIMETOMAPI

#include <vmime/vmime.hpp>
#include <mapix.h>
#include "mapidefs.h"
#include "ECLogger.h"
#include "options.h"
#include "charset/convert.h"

#define MAPI_CHARSET vmime::charset(vmime::charsets::UTF_8)
#define MAPI_CHARSET_STRING "UTF-8"

enum BODYLEVEL { BODY_NONE, BODY_PLAIN, BODY_HTML };
enum ATTACHLEVEL { ATTACH_NONE, ATTACH_INLINE, ATTACH_NORMAL };

typedef struct sMailState {
	BODYLEVEL bodyLevel;		//!< the current body state. plain upgrades none, html upgrades plain and none.
	ULONG ulLastCP;
	ATTACHLEVEL attachLevel;	//!< the current attachment state
	bool bAttachSignature;		//!< add a signed signature at the end
	ULONG ulMsgInMsg;			//!< counter for msg-in-msg level
	std::string strHTMLBody;	//!< cache for the current complete untouched HTML body, used for finding CIDs or locations (inline images)

	sMailState() {
		this->reset();
		ulMsgInMsg = 0;
	};
	void reset() {
		bodyLevel = BODY_NONE;
		ulLastCP = 0;
		attachLevel = ATTACH_NONE;
		bAttachSignature = false;
		strHTMLBody.clear();
	};
} sMailState;

class VMIMEToMAPI
{
public:
	VMIMEToMAPI();
	VMIMEToMAPI(LPADRBOOK lpAdrBook, ECLogger *newlogger, delivery_options dopt);
	virtual	~VMIMEToMAPI();

	HRESULT convertVMIMEToMAPI(const std::string &input, IMessage *lpMessage);
	HRESULT createIMAPProperties(const std::string &input, std::string *lpEnvelope, std::string *lpBody, std::string *lpBodyStructure);

private:
	ECLogger *lpLogger;
	delivery_options m_dopt;
	LPADRBOOK m_lpAdrBook;
	IABContainer *m_lpDefaultDir;
	sMailState m_mailState;
	convert_context m_converter;

	HRESULT fillMAPIMail(vmime::ref<vmime::message> vmMessage, IMessage *lpMessage);
	HRESULT disectBody(vmime::ref<vmime::header> vmHeader, vmime::ref<vmime::body> vmBody, IMessage* lpMessage, bool onlyBody = false, bool filterDouble = false, bool appendBody = false);

	HRESULT handleHeaders(vmime::ref<vmime::header> vmHeader, IMessage* lpMessage);
	HRESULT handleRecipients(vmime::ref<vmime::header> vmHeader, IMessage* lpMessage);
	HRESULT modifyRecipientList(LPADRLIST lpRecipients, vmime::ref<vmime::addressList> vmAddressList, ULONG ulRecipType);
	HRESULT modifyFromAddressBook(LPSPropValue *lppPropVals, ULONG *lpulValues, const char *email, const WCHAR *fullname, ULONG ulRecipType, LPSPropTagArray lpPropsList);

	HRESULT handleTextpart(vmime::ref<vmime::header> vmHeader, vmime::ref<vmime::body> vmBody, IMessage* lpMessage, bool bAppendBody);
	HRESULT handleHTMLTextpart(vmime::ref<vmime::header> vmHeader, vmime::ref<vmime::body> vmBody, IMessage* lpMessage, bool bAppendBody);
	HRESULT handleAttachment(vmime::ref<vmime::header> vmHeader, vmime::ref<vmime::body> vmBody, IMessage* lpMessage, bool bAllowEmpty = true);
	HRESULT handleMessageToMeProps(IMessage *lpMessage, LPADRLIST lpRecipients);

	vmime::charset getCompatibleCharset(const vmime::charset &vmCharset);
	std::wstring getWideFromVmimeText(const vmime::text &vmText);
	
	HRESULT postWriteFixups(IMessage *lpMessage);

	std::string mailboxToEnvelope(vmime::ref<vmime::mailbox> mbox);
	std::string addressListToEnvelope(vmime::ref<vmime::addressList> mbox);
	HRESULT createIMAPEnvelope(vmime::ref<vmime::message> vmMessage, IMessage* lpMessage);
	std::string createIMAPEnvelope(vmime::ref<vmime::message> vmMessage);

	HRESULT createIMAPBody(const std::string &input, vmime::ref<vmime::message> vmMessage, IMessage* lpMessage);

	HRESULT messagePartToStructure(const std::string &input, vmime::ref<vmime::bodyPart> vmBodyPart, std::string *lpSimple, std::string *lpExtended);
	HRESULT bodyPartToStructure(const std::string &input, vmime::ref<vmime::bodyPart> vmBodyPart, std::string *lpSimple, std::string *lpExtended);
	std::string getStructureExtendedFields(vmime::ref<vmime::header> vmHeaderPart);
	std::string parameterizedFieldToStructure(vmime::ref<vmime::parameterizedHeaderField> vmParamField);
	std::string::size_type countBodyLines(const std::string &input, std::string::size_type start, std::string::size_type length);
};

#endif
