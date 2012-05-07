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

#include "ECKrbAuth.h"
#ifndef HAVE_KRB5
ECRESULT ECKrb5AuthenticateUser(const std::string &strUsername, const std::string &strPassword, std::string *lpstrError)
{
	*lpstrError = "Server is not compiled with kerberos support.";
	return ZARAFA_E_NO_SUPPORT;
}
#else
// error_message() is wrongly typed in c++ context
extern "C" {
#include <krb5.h>
#include <et/com_err.h>
}

ECRESULT ECKrb5AuthenticateUser(const std::string &strUsername, const std::string &strPassword, std::string *lpstrError)
{
	ECRESULT er = erSuccess;
	krb5_error_code code = 0;
	krb5_get_init_creds_opt options;
	krb5_creds my_creds;
	krb5_context ctx;
	krb5_principal me;
	char *name = NULL;

	memset(&ctx, 0, sizeof(ctx));
	memset(&me, 0, sizeof(me));

	code = krb5_init_context(&ctx);

	if (code) {
		*lpstrError = std::string("Unable to initialize kerberos 5 library: code ") + error_message(code);
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	code = krb5_parse_name(ctx, strUsername.c_str(), &me);
	if (code) {
		*lpstrError = std::string("Error parsing kerberos 5 username: code ") + error_message(code);
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	code = krb5_unparse_name(ctx, me, &name);
	if (code) {
		*lpstrError = std::string("Error unparsing kerberos 5 username: code ") + error_message(code);
		er = ZARAFA_E_INVALID_PARAMETER;
		goto exit;
	}

	krb5_get_init_creds_opt_init(&options);
	memset(&my_creds, 0, sizeof(my_creds));

	code = krb5_get_init_creds_password(ctx, &my_creds, me, (char*)strPassword.c_str(), 0, 0, 0, NULL, &options);
	if (code) {
		*lpstrError = error_message(code);
		er = ZARAFA_E_LOGON_FAILED;
		goto exit;
	} 

exit:
	if (name)
		krb5_free_unparsed_name(ctx, name);

	if (me)
		krb5_free_principal(ctx, me);

	if (ctx)
		krb5_free_context(ctx);

	memset(&ctx, 0, sizeof(ctx));
	memset(&me, 0, sizeof(me));

	return er;
}
#endif
