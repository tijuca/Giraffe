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
#define POP3_RESP_TEMPFAIL "-ERR [SYS/TEMP] "
#define POP3_RESP_PERMFAIL "-ERR [SYS/PERM] "
#define POP3_RESP_AUTH_ERROR "-ERR [AUTH] "
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
	std::string GetCapabilityString();

	HRESULT HrCmdCapability();
	HRESULT HrCmdStarttls();
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
	BOOL IsAuthorized() { return !!lpStore; }

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
