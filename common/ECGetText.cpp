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
#include "ECGetText.h"
#include "charset/convert.h"

#include <map>
#include <string>

#include <pthread.h>
#include <assert.h>

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

namespace detail {

	/**
	 * This class performs the actual conversion and caching of the translated messages.
	 * Results are cached based on the pointer value, not the string content. This implies
	 * two assumptions:
	 * 1. Gettext allways returns the same pointer for a particular translation.
	 * 2. If there's no translation, the original pointer is returned. So we assume that the
	 *    compiler optimized string literals to have the same address if they're equal. If
	 *    this assumption is false, this will lead to more conversions, and more memory usage
	 *    by the cache.
	 */
	class converter 
	{
	public:
		/**
		 * Get the global converter instance.
		 * @return	The global converter instance.
		 */
		static converter *getInstance() {
			pthread_mutex_lock(&s_hInstanceLock);
			if (!s_lpInstance) {
				s_lpInstance = new converter;
				atexit(&destroy);
			}
			pthread_mutex_unlock(&s_hInstanceLock);

			return s_lpInstance;
		}

		/**
		 * Perform the actual cache lookup or conversion.
		 *
		 * @param[in]	lpsz	The string to convert.
		 * @return	The converted string.
		 */
		const wchar_t *convert(const char *lpsz) {
			pthread_mutex_lock(&m_hCacheLock);

			std::pair<cache_type::iterator, bool> insResult = m_cache.insert(cache_type::value_type(lpsz, std::wstring()));
			if (insResult.second == true)	// successfull insert, so not found in cache
				insResult.first->second.assign(m_converter.convert_to<std::wstring>(lpsz));
			
			const wchar_t *lpszW = insResult.first->second.c_str();

			pthread_mutex_unlock(&m_hCacheLock);

			return lpszW;
		}

	private:
		/**
		 * Destroys the instance in application exit.
		 */
		static void destroy() {
			assert(s_lpInstance);
			delete s_lpInstance;
			s_lpInstance = NULL;
		}

		/**
		 * Constructor
		 */
		converter() {
			pthread_mutex_init(&m_hCacheLock, NULL);
		}

		/**
		 * Destructor
		 */
		~converter() {
			pthread_mutex_destroy(&m_hCacheLock);
		}

	private:
		static converter		*s_lpInstance;
		static pthread_mutex_t	s_hInstanceLock;

		typedef std::map<const char *, std::wstring>	cache_type;
		convert_context	m_converter;
		cache_type		m_cache;
		pthread_mutex_t	m_hCacheLock;
	};

	converter* converter::s_lpInstance = NULL;
	pthread_mutex_t converter::s_hInstanceLock = PTHREAD_MUTEX_INITIALIZER;

} // namespace detail


/**
 * Performs a 'regular' gettext and converts the result to a wide character string.
 *
 * @param[in]	domainname	The domain to use for the translation
 * @param[in]	msgid		The msgid of the message to be translated.
 *
 * @return	The converted, translated string.
 */
LPWSTR zarafa_dcgettext_wide(const char *domainname, const char *msgid)
{
	const char *lpsz = msgid;
	detail::converter *lpConverter = detail::converter::getInstance();

#ifndef NO_GETTEXT
	lpsz = dcgettext(domainname, msgid, LC_MESSAGES);
#endif

	return (LPWSTR)lpConverter->convert(lpsz);
}
