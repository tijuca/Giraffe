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

// based on vmime/messaging/smtp/SMTPTransport.hpp, but with additions
// we cannot use a class derived from SMTPTransport, since that class has alot of privates

//
// VMime library (http://www.vmime.org)
// Copyright (C) 2002-2009 Vincent Richard <vincent@vincent-richard.net>
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License as
// published by the Free Software Foundation; either version 3 of
// the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
//
// Linking this library statically or dynamically with other modules is making
// a combined work based on this library.  Thus, the terms and conditions of
// the GNU General Public License cover the whole combination.
//

#ifndef MAPI_NET_SMTP_SMTPTRANSPORT_HPP_INCLUDED
#define MAPI_NET_SMTP_SMTPTRANSPORT_HPP_INCLUDED


#include "vmime/config.hpp"

#include "vmime/net/transport.hpp"
#include "vmime/net/socket.hpp"
#include "vmime/net/timeoutHandler.hpp"

#include "vmime/net/smtp/SMTPServiceInfos.hpp"

#include "inetmapi.h"

namespace vmime {
namespace net {
namespace smtp {


class SMTPResponse;


/** SMTP transport service.
  */

class MAPISMTPTransport : public transport
{
public:

	MAPISMTPTransport(ref <session> sess, ref <security::authenticator> auth, const bool secured = false);
	~MAPISMTPTransport();

	const string getProtocolName() const;

	static const serviceInfos& getInfosInstance();
	const serviceInfos& getInfos() const;

	void connect();
	bool isConnected() const;
	void disconnect();

	void noop();

	void send(const mailbox& expeditor, const mailboxList& recipients, utility::inputStream& is, const utility::stream::size_type size, utility::progressListener* progress = NULL);

	bool isSecuredConnection() const;
	ref <connectionInfos> getConnectionInfos() const;

	// additional functions
	const int getRecipientErrorCount() const;
	const std::vector<sFailedRecip> getRecipientErrorList() const;
	void setLogger(ECLogger *lpLogger);


private:

	void sendRequest(const string& buffer, const bool end = true);
	ref <SMTPResponse> readResponse();

	void internalDisconnect();

	void helo();
	void authenticate();
#if VMIME_HAVE_SASL_SUPPORT
	void authenticateSASL();
#endif // VMIME_HAVE_SASL_SUPPORT

#if VMIME_HAVE_TLS_SUPPORT
	void startTLS();
#endif // VMIME_HAVE_TLS_SUPPORT

	ref <socket> m_socket;
	bool m_authentified;

	bool m_extendedSMTP;
	std::map <string, std::vector <string> > m_extensions;

	ref <timeoutHandler> m_timeoutHandler;

	const bool m_isSMTPS;

	bool m_secured;
	ref <connectionInfos> m_cntInfos;


	// Service infos
	static SMTPServiceInfos sm_infos;

	// additional data
	std::vector<sFailedRecip> m_lstFailedRecipients;
	ECLogger *m_lpLogger;
};


} // smtp
} // net
} // vmime


#endif // MAPI_NET_SMTP_SMTPTRANSPORT_HPP_INCLUDED
