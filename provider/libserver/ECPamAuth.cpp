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

#include "ECPamAuth.h"
#ifndef HAVE_PAM
ECRESULT ECPAMAuthenticateUser(const char* szPamService, const std::string &strUsername, const std::string &strPassword, std::string *lpstrError)
{
	*lpstrError = "Server is not compiled with pam support.";
	return ZARAFA_E_NO_SUPPORT;
}
#else
#include <security/pam_appl.h>

class PAMLock {
public:
	PAMLock() { pthread_mutex_init(&m_mPAMAuthLock, NULL); }
	~PAMLock() { pthread_mutex_destroy(&m_mPAMAuthLock); }

	pthread_mutex_t m_mPAMAuthLock;
} static cPAMLock;

int converse(int num_msg, const struct pam_message **msg, struct pam_response **resp, void *appdata_ptr)
{
	int ret = PAM_SUCCESS;
	int i = 0;
	struct pam_response *response = NULL;
	char *password = (char *) appdata_ptr;

	if (!resp || !msg || !password)
		return PAM_CONV_ERR;

	response = (struct pam_response *) malloc(num_msg * sizeof(struct pam_response));
	if (!response)
		return PAM_BUF_ERR;

	for (i = 0; i < num_msg; i++) {
		response[i].resp_retcode = 0;
		response[i].resp = 0;

		if (msg[i]->msg_style == PAM_PROMPT_ECHO_OFF) {
			response[i].resp = strdup(password);
		} else {
			free(response);
			ret = PAM_CONV_ERR;
			goto exit;
		}
	}

	*resp = response;

exit:
	return ret;
}

ECRESULT ECPAMAuthenticateUser(const char* szPamService, const std::string &strUsername, const std::string &strPassword, std::string *lpstrError)
{
	ECRESULT er = erSuccess;
	int res = 0;
	pam_handle_t *pamh = NULL;
	struct pam_conv conv_info = { &converse, (void*)strPassword.c_str() };

	pthread_mutex_lock(&cPAMLock.m_mPAMAuthLock);

	res = pam_start(szPamService, strUsername.c_str(), &conv_info, &pamh);
	if (res != PAM_SUCCESS) 
	{
		*lpstrError = pam_strerror(NULL, res);
		er = ZARAFA_E_LOGON_FAILED;
		goto exit;
	}

	res = pam_authenticate(pamh, PAM_SILENT);

	pam_end(pamh, res);

	if (res != PAM_SUCCESS) {
		*lpstrError = pam_strerror(NULL, res);
		er = ZARAFA_E_LOGON_FAILED;
	}

exit:
	pthread_mutex_unlock(&cPAMLock.m_mPAMAuthLock);

	return er;
}
#endif
