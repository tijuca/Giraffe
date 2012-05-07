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
#include "SSLUtil.h"
#include <pthread.h>
#include <openssl/bn.h>
#include <openssl/rand.h>
#include <openssl/ssl.h>
#include <openssl/conf.h>
#include <openssl/engine.h>

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif


pthread_mutex_t*	ssl_locks = NULL;

void ssl_lock(int mode, int n, const char *file, int line) {
	if (mode & CRYPTO_LOCK) {
		pthread_mutex_lock(&ssl_locks[n]);
	} else {
		pthread_mutex_unlock(&ssl_locks[n]);
	}
}

unsigned long ssl_id_function() {
    return ((unsigned long) pthread_self());
}

void ssl_threading_setup() {
	if (ssl_locks)
		return;
	ssl_locks = new pthread_mutex_t[CRYPTO_num_locks()];
	for (int i=0; i < CRYPTO_num_locks(); i++)
		pthread_mutex_init(&ssl_locks[i], NULL);
	CRYPTO_set_locking_callback(ssl_lock);
	// no need to set on win32 (or maybe use GetCurrentThreadId)
	CRYPTO_set_id_callback(ssl_id_function);
}

void ssl_threading_cleanup() {
	if (!ssl_locks)
		return;
	for (int i=0; i < CRYPTO_num_locks(); i++)
		pthread_mutex_destroy(&ssl_locks[i]);
	delete [] ssl_locks;
	ssl_locks = NULL;
	CRYPTO_set_locking_callback(NULL);
}

/**
 * Free most of the SSL library allocated memory.
 *
 * This will remove most of the memmory used by 
 * the ssl library. Don't use this function in libraries
 * because it will unload the whole SSL data.
 *
 * This function makes valgrind happy
 */
void SSL_library_cleanup()
{
	#ifndef OPENSSL_NO_ENGINE
		ENGINE_cleanup();
	#endif

	ERR_free_strings();
	ERR_remove_state(0);
	EVP_cleanup();
	CRYPTO_cleanup_all_ex_data();

	CONF_modules_unload(0);
}

void ssl_random_init()
{
	rand_init();

	while (RAND_status() == 0) {
		unsigned int r = rand_mt();
		RAND_seed(&r, sizeof(unsigned int));
	}
}

void ssl_random(bool b64bit, unsigned long long *lpullId)
{
	BIGNUM bn;
	unsigned long long ullId = 0;

	if (!b64bit) {
		ullId = rand_mt();
	} else {
		BN_init(&bn);

		if (BN_rand(&bn, sizeof(unsigned long long)*8, -1, 0) == 0) {
			ullId = ((unsigned long long)rand_mt() << 32) | rand_mt();
		} else {
			BN_bn2bin(&bn, (unsigned char*)&ullId);
		}

		BN_free(&bn);
	}

	*lpullId = ullId;
}
