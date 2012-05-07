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

#ifndef POP3_H
#define POP3_H

#include <vector>
#include "ClientProto.h"

/**
 * @defgroup gateway_pop3 POP3 
 * @ingroup gateway
 * @{
 */

#define POP3_MAX_RESPONSE_LENGTH 512

#define POP3_RESP_OK "+OK "
#define POP3_RESP_ERR "-ERR "

/* enum POP3_Command { POP3_CMD_USER, POP3_CMD_PASS, POP3_CMD_STAT, POP3_CMD_LIST, POP3_CMD_RETR, POP3_CMD_DELE, POP3_CMD_NOOP, */
/* 		POP3_CMD_RSET, POP3_CMD_QUIT, POP3_CMD_TOP, POP3_CMD_UIDL }; */

class POP3 : public ClientProto {
public:
	POP3(const char *szServerPath, ECChannel *lpChannel, ECLogger *lpLogger, ECConfig *lpConfig);
	~POP3();

	int getTimeoutMinutes();

	HRESULT HrSendGreeting(const std::string &strHostString);
	HRESULT HrCloseConnection(const std::string &strQuitMsg);
	HRESULT HrProcessCommand(const std::string &strInput);
	HRESULT HrDone(bool bSendResponse);

private:
	HRESULT HrCmdUser(const std::string &strUser);
	HRESULT HrCmdPass(const std::string &strPass);
	HRESULT HrCmdStat();
	HRESULT HrCmdList();
	HRESULT HrCmdList(unsigned int ulMailNr);
	HRESULT HrCmdRetr(unsigned int ulMailNr);
	HRESULT HrCmdDele(unsigned int ulMailNr);
	HRESULT HrCmdNoop();
	HRESULT HrCmdRset();
	HRESULT HrCmdQuit();
	HRESULT HrCmdUidl();
	HRESULT HrCmdUidl(unsigned int ulMailNr);
	HRESULT HrCmdTop(unsigned int ulMailNr, unsigned int ulLines);

	HRESULT HrResponse(const std::string &strResult, const std::string &strResponse);

private:
	struct MailListItem {
		SBinary sbEntryID;
		ULONG ulSize;
		bool bDeleted;
	};

	HRESULT HrMakeMailList();
	HRESULT HrLogin(const std::string &strUsername, const std::string &strPassword);
	std::string DotFilter(const char *input);

	IMAPISession	*lpSession;
	IMsgStore		*lpStore;
	IMAPIFolder		*lpInbox;
	IAddrBook		*lpAddrBook;
	sending_options sopt;

	std::string szUser;

	std::vector<MailListItem> lstMails;
};

/** @} */
#endif
