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

#ifndef __INETMAPI_OPTIONS_H
#define __INETMAPI_OPTIONS_H

/* we do not need this on linux */
# define INETMAPI_API

typedef struct _do {
	bool use_received_date;			// Use the 'received' date instead of the current date as delivery date
	bool mark_as_read;				// Deliver the message 'read' instead of unread
	bool add_imap_data;				// Save IMAP optimizations to the server
	bool parse_smime_signed;		// Parse actual S/MIME content instead of just writing out the S/MIME data to a single attachment
	LPSBinary user_entryid;			// If not NULL, specifies the entryid of the user for whom we are delivering. If set, allows generating PR_MESSAGE_*_ME properties.
	const char *default_charset;		// Specifies the default charset to use when none is found in the source message, or when us-ascii is used in the source message. Note that this charset *must* be a superset of us-ascii
} delivery_options;

typedef struct _so {
	char *alternate_boundary;		// Specifies a specific boundary prefix to use when creating MIME boundaries
	bool no_recipients_workaround;	// Specified that we wish to accepts messages with no recipients (for example, when converting an attached email with no recipients)
	bool msg_in_msg;
	bool headers_only;
	bool add_received_date;
	int use_tnef;					// -1: minimize usage, 0: autodetect, 1: force
	bool force_utf8;
	char *charset_upgrade;
	bool allow_send_to_everyone;
	bool enable_dsn;				/**< Enable SMTP Delivery Status Notifications */
	bool always_expand_distr_list;
} sending_options;

void INETMAPI_API imopt_default_delivery_options(delivery_options *dopt);
void INETMAPI_API imopt_default_sending_options(sending_options *sopt);

#endif
