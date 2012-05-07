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

#ifndef LMTP_H
#define LMTP_H

#include <string>
#include <vector>
#include "ECChannel.h"
#include "ECLogger.h"
#include "ECConfig.h"

enum LMTP_Command {LMTP_Command_LHLO, LMTP_Command_MAIL_FROM, LMTP_Command_RCPT_TO, LMTP_Command_DATA, LMTP_Command_RSET, LMTP_Command_QUIT };

class LMTP {
public:
	LMTP(ECChannel *lpChan, char *szServerPath, ECLogger *lpLog, ECConfig *lpConf);
	~LMTP();

	HRESULT HrGetCommand(const std::string &strCommand, LMTP_Command &eCommand);
	HRESULT HrResponse(const std::string &strResponse);

	HRESULT HrCommandLHLO(const std::string &strInput);
	HRESULT HrCommandMAILFROM(const std::string &from_addr);
	HRESULT HrCommandRCPTTO(const std::string &to_address, std::string *mail_address_unsolved);
	HRESULT HrCommandDATA(FILE *tmp);
	HRESULT HrCommandQUIT();

private:
	HRESULT HrParseAddress(const std::string &strAddress, std::string *strEmail);

	ECChannel		*m_lpChannel;
	ECLogger		*m_lpLogger;
	ECConfig		*m_lpConfig;
	std::string		m_strPath;
};

#endif

