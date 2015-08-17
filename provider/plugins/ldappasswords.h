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

// -*- Mode: c++ -*-

#ifndef LDAPPASSWORDS_H
#define LDAPPASSWORDS_H

/**
 * @defgroup userplugin_password Password validation
 * @ingroup userplugin
 * @{
 */


enum {
	PASSWORD_CRYPT,
	PASSWORD_MD5,
	PASSWORD_SMD5,
	PASSWORD_SHA,
	PASSWORD_SSHA
};

/**
 * Encrypt passwird using requested encryption type
 *
 * The returned array must be deleted with delete []
 *
 * @param[in]	type
 *					The encryption type (CRYPT, MD5, SMD5, SSHA)
 * @param[in]	password
 *					The password which should be encrypted
 * @return The encrypted password
 */
extern char *encryptPassword(int type, const char *password);

/**
 * Compare unencrypted password with encrypted password with the
 * requested encryption type
 *
 * @param[in]	type
 *					The encryption type (CRYPT, MD5, SMD5, SSHA)
 * @param[in]	password
 *					The unencryped password which should match crypted
 * @param[in]	crypted
 *					The encrypted password which should match password
 * @return 0 when the passwords match
 */
extern int checkPassword(int type, const char *password, const char *crypted);

/** @} */
#endif
