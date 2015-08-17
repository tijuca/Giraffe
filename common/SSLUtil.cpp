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
static const char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif


static pthread_mutex_t *ssl_locks;

static void ssl_lock(int mode, int n, const char *file, int line)
{
	if (mode & CRYPTO_LOCK) {
		pthread_mutex_lock(&ssl_locks[n]);
	} else {
		pthread_mutex_unlock(&ssl_locks[n]);
	}
}

static unsigned long ssl_id_function(void)
{
    return ((unsigned long) pthread_self());
}

void ssl_threading_setup() {
	if (ssl_locks)
		return;
	pthread_mutexattr_t mattr;
	// make recursive, because of openssl bug http://rt.openssl.org/Ticket/Display.html?id=2813&user=guest&pass=guest
	pthread_mutexattr_init(&mattr);
	pthread_mutexattr_settype(&mattr, PTHREAD_MUTEX_RECURSIVE);
	ssl_locks = new pthread_mutex_t[CRYPTO_num_locks()];
	for (int i=0; i < CRYPTO_num_locks(); i++)
		pthread_mutex_init(&ssl_locks[i], &mattr);
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
	CRYPTO_set_id_callback(NULL);
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

	sk_SSL_COMP_free(SSL_COMP_get_compression_methods());
}

void ssl_random_init()
{
	rand_init();

	while (RAND_status() == 0) {
		char buffer[16];
		rand_get(buffer, sizeof buffer);
		RAND_seed(buffer, sizeof buffer);
	}
}

void ssl_random(bool b64bit, uint64_t *lpullId)
{
	BIGNUM bn;
	BN_init(&bn);

	if (b64bit)
	{
		unsigned long long ullId = 0;

		if (BN_rand(&bn, sizeof(ullId) * 8, -1, 0) == 0)
			rand_get((char *)&ullId, sizeof ullId);
		else
			BN_bn2bin(&bn, (unsigned char*)&ullId);

		*lpullId = ullId;
	}
	else
	{
		uint32_t ullId = 0;

		if (BN_rand(&bn, sizeof(ullId) * 8, -1, 0) == 0)
			rand_get((char *)&ullId, sizeof ullId);
		else
			BN_bn2bin(&bn, (unsigned char*)&ullId);

		*lpullId = ullId;
	}

	BN_free(&bn);
}
