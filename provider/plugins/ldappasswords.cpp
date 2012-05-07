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

#include "platform.h"
#include "base64.h"
#include "plugin.h"

#include <openssl/rand.h>
#include <openssl/des.h>
#include <openssl/md5.h>
#include <openssl/sha.h>
#include <string.h>
#include <iostream>

#include "ldappasswords.h"

using namespace std;


static const char saltchars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789./";
static const char b64chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/** 
 * Encode string in base-64
 *
 * @param[out] out the buffer to which the result will be written.
 * @param[in] in the input string, not necessarily 0-terminated.
 * @param[in] len the number of bytes from \c in to encode.
 * 
 * The buffer \c out must be large enough to hold the result, which is
 * \c len / 3 * 4 + 4 (approximately).
 */
static void b64_encode(char *out, const unsigned char *in, unsigned int len) {
	unsigned int i, j;
	unsigned char bytes[3];

	for (i = j = 0; i < len / 3 * 3 + 3; i += 3) {
		bytes[0] = in[i];
		out[j++] = b64chars[(bytes[0] >> 2) & 0x3f];

		if (i + 1 >= len)
			bytes[1] = 0;
		else
			bytes[1] = in[i + 1];

		out[j++] = b64chars[((bytes[0] << 4) & 0x30) | ((bytes[1] >> 4) & 0x0f)];

		if (i + 2 >= len)
			bytes[2] = 0;
		else
			bytes[2] = in[i + 2];

		if (i + 1 < len)
			out[j++] = b64chars[((bytes[1] << 2) & 0x3c) | ((bytes[2] >> 6) & 0x03)];
		else
			out[j++] = '=';
		if (i + 2 < len)
			out[j++] = b64chars[bytes[2] & 0x3f];
		else
			out[j++] = '=';
	}

	out[j++] = 0;
}


static char *password_encrypt_crypt(const char *data, unsigned int len) {
	char salt[3], *res;
	unsigned char rand[16];
	char cryptbuf[32];

	RAND_pseudo_bytes(rand, 8);
	salt[0] = saltchars[rand[0] & 63];
	salt[1] = saltchars[rand[1] & 63];

	des_fcrypt(data, salt, cryptbuf);

	res = new char[32];
	snprintf(res, 31, "{CRYPT}%s", cryptbuf);

	return res;
}

static int password_check_crypt(const char *data, unsigned int len, const char *crypted) {
	char salt[3];
	char cryptbuf[32];

	salt[0] = crypted[0];
	salt[1] = crypted[1];
	salt[2] = 0;

	des_fcrypt(data, salt, cryptbuf);

	if (!strcmp(cryptbuf, crypted))
		return 0;
	else
		return 1;
}

static char *password_encrypt_md5(const char *data, unsigned int len) {
	unsigned char md5_out[MD5_DIGEST_LENGTH];
	const int base64_len = MD5_DIGEST_LENGTH * 4 / 3 + 4;
	char b64_out[MD5_DIGEST_LENGTH * 4 / 3 + 4];
	char *res;

	MD5((unsigned char *) data, len, md5_out);
	b64_encode(b64_out, md5_out, MD5_DIGEST_LENGTH);

	res = new char[base64_len + 12];
	snprintf(res, base64_len + 11, "{MD5}%s", b64_out);

	return res;
}

static int password_check_md5(const char *data, unsigned int len, const char *crypted) {
	unsigned char md5_out[MD5_DIGEST_LENGTH];
	char b64_out[MD5_DIGEST_LENGTH * 4 / 3 + 4];

	MD5((unsigned char *) data, len, md5_out);
	b64_encode(b64_out, md5_out, MD5_DIGEST_LENGTH);

	if (!strcmp(b64_out, crypted))
		return 0;
	else
		return 1;
}

// md5sum + salt at the end. md5sum length == 16, salt length == 4
static char *password_encrypt_smd5(const char *data, unsigned int len) {
	MD5_CTX ctx;
	unsigned char md5_out[MD5_DIGEST_LENGTH + 4];
	unsigned char *salt = md5_out + MD5_DIGEST_LENGTH; // salt is at the end of the digest
	const int base64_len = MD5_DIGEST_LENGTH * 4 / 3 + 4;
	char b64_out[MD5_DIGEST_LENGTH * 4 / 3 + 4];
	char *res;

	RAND_bytes(salt, 4);

	MD5_Init(&ctx);
	MD5_Update(&ctx, data, len);
	MD5_Update(&ctx, salt, 4);
	MD5_Final(md5_out, &ctx);	// writes upto the salt

	b64_encode(b64_out, md5_out, MD5_DIGEST_LENGTH + 4);

	res = new char[base64_len + 12];
	snprintf(res, base64_len + 11, "{SMD5}%s", b64_out);

	return res;
}

static int password_check_smd5(const char *data, unsigned int len, const char *crypted) {
	std::string digest;
	std::string salt;
	unsigned char md5_out[MD5_DIGEST_LENGTH];
	char b64_out[MD5_DIGEST_LENGTH * 4 / 3 + 4];
	MD5_CTX ctx;

	digest = base64_decode(crypted);
	salt.assign(digest.c_str()+MD5_DIGEST_LENGTH, digest.length()-MD5_DIGEST_LENGTH);

	MD5_Init(&ctx);
	MD5_Update(&ctx, data, len);
	MD5_Update(&ctx, salt.c_str(), salt.length());
	MD5_Final(md5_out, &ctx);

	b64_encode(b64_out, md5_out, MD5_DIGEST_LENGTH);

	if (!strncmp(b64_out, crypted, MD5_DIGEST_LENGTH))
		return 0;
	else
		return 1;
}

static char *password_encrypt_ssha(const char *data, unsigned int len, bool bSalted) {
	unsigned char SHA_out[SHA_DIGEST_LENGTH];
	const int base64_len = SHA_DIGEST_LENGTH * 4 / 3 + 4;
	char b64_out[SHA_DIGEST_LENGTH * 4 / 3 + 4];
	char *res;
	unsigned char salt[4];
	std::string pwd;

	pwd.assign(data, len);
	if (bSalted) {
		RAND_bytes(salt, 4);
		pwd.append((const char*)salt, 4);
	}

	SHA1((const unsigned char*)pwd.c_str(), pwd.length(), SHA_out);
	b64_encode(b64_out, SHA_out, SHA_DIGEST_LENGTH);

	res = new char[base64_len + 12];
	snprintf(res, base64_len + 11, "{%s}%s", bSalted ? "SSHA" : "SHA", b64_out);

	return res;
}

static int password_check_ssha(const char *data, unsigned int len, const char *crypted, bool bSalted) {
	std::string digest;
	std::string salt;
	std::string pwd;
	unsigned char SHA_out[SHA_DIGEST_LENGTH];

	pwd.assign(data, len);

	digest = base64_decode(crypted);

	if (bSalted) {
		salt.assign(digest.c_str()+SHA_DIGEST_LENGTH, digest.length()-SHA_DIGEST_LENGTH);
		pwd += salt;
	}

	memset(SHA_out, 0, sizeof(SHA_out));
	SHA1((const unsigned char*)pwd.c_str(), pwd.length(), SHA_out);

	digest.assign((char*)SHA_out, SHA_DIGEST_LENGTH);
	if (bSalted)
		digest += salt;
	pwd = base64_encode((const unsigned char*)digest.c_str(), digest.length());

	if (!strcmp(pwd.c_str(), crypted))
		return 0;
	else
		return 1;
}


char *encryptPassword(int type, const char *password) {
	switch(type) {
	case PASSWORD_CRYPT:
		return password_encrypt_crypt(password, strlen(password));
	case PASSWORD_MD5:
		return password_encrypt_md5(password, strlen(password));
	case PASSWORD_SMD5:
		return password_encrypt_smd5(password, strlen(password));
	case PASSWORD_SHA:
		return password_encrypt_ssha(password, strlen(password), false);
	case PASSWORD_SSHA:
		return password_encrypt_ssha(password, strlen(password), true);
	default:
		return NULL;
	}
}


int checkPassword(int type, const char *password, const char *crypted) {
	switch(type) {
	case PASSWORD_CRYPT:
		return password_check_crypt(password, strlen(password), crypted);
	case PASSWORD_MD5:
		return password_check_md5(password, strlen(password), crypted);
	case PASSWORD_SMD5:
		return password_check_smd5(password, strlen(password), crypted);
	case PASSWORD_SHA:
		return password_check_ssha(password, strlen(password), crypted, false);
	case PASSWORD_SSHA:
		return password_check_ssha(password, strlen(password), crypted, true);
	default:
		return 1;
	}
}
