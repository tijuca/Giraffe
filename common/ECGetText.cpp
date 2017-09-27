/*
 * Copyright 2005 - 2016 Zarafa and its licensors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License, version 3,
 * as published by the Free Software Foundation.
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

#include <kopano/zcdefs.h>
#include <kopano/lockhelper.hpp>
#include <kopano/platform.h>
#include <kopano/ECGetText.h>
#include <kopano/charset/convert.h>

#include <map>
#include <mutex>
#include <string>
#include <cassert>

namespace KC {

namespace detail {

	/**
	 * This class performs the actual conversion and caching of the translated messages.
	 * Results are cached based on the pointer value, not the string content. This implies
	 * two assumptions:
	 * 1. Gettext always returns the same pointer for a particular translation.
	 * 2. If there's no translation, the original pointer is returned. So we assume that the
	 *    compiler optimized string literals to have the same address if they're equal. If
	 *    this assumption is false, this will lead to more conversions, and more memory usage
	 *    by the cache.
	 */
	class converter _kc_final {
	public:
		/**
		 * Get the global converter instance.
		 * @return	The global converter instance.
		 */
		static converter *getInstance() {
			scoped_lock locker(s_hInstanceLock);
			if (!s_lpInstance) {
				s_lpInstance = new converter;
				atexit(&destroy);
			}
			return s_lpInstance;
		}

		/**
		 * Perform the actual cache lookup or conversion.
		 *
		 * @param[in]	lpsz	The string to convert.
		 * @return	The converted string.
		 */
		const wchar_t *convert(const char *lpsz) {
			scoped_lock l_cache(m_hCacheLock);
			auto insResult = m_cache.insert({lpsz, std::wstring()});
			if (insResult.second == true)	// successful insert, so not found in cache
				insResult.first->second.assign(m_converter.convert_to<std::wstring>(lpsz));
			
			const wchar_t *lpszW = insResult.first->second.c_str();
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

	private:
		static converter		*s_lpInstance;
		static std::mutex s_hInstanceLock;

		typedef std::map<const char *, std::wstring>	cache_type;
		convert_context	m_converter;
		cache_type		m_cache;
		std::mutex m_hCacheLock;
	};

	std::mutex converter::s_hInstanceLock;
	converter* converter::s_lpInstance = NULL;
} // namespace detail

/**
 * Performs a 'regular' gettext and converts the result to a wide character string.
 *
 * @param[in]	domainname	The domain to use for the translation
 * @param[in]	msgid		The msgid of the message to be translated.
 *
 * @return	The converted, translated string.
 */
LPWSTR kopano_dcgettext_wide(const char *domainname, const char *msgid)
{
	const char *lpsz = msgid;

#ifndef NO_GETTEXT
	lpsz = dcgettext(domainname, msgid, LC_MESSAGES);
#endif
	return const_cast<wchar_t *>(detail::converter::getInstance()->convert(lpsz));
}

} /* namespace */
